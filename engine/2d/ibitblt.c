/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/2d/rcs/ibitblt.c $
 * $Revision: 1.6 $
 * $Author: john $
 * $Date: 1994/11/28 17:07:29 $
 * 
 * Rountines to copy a bitmap on top of another bitmap, but
 * only copying to pixels that are transparent.
 * 
 * $Log: ibitblt.c $
 * Revision 1.6  1994/11/28  17:07:29  john
 * Took out some unused functions in linear.asm, moved
 * gr_linear_movsd from linear.asm to bitblt.c, made sure that
 * the code in ibiblt.c sets the direction flags before rep movsing.
 * 
 * Revision 1.5  1994/11/18  22:50:22  john
 * Changed shorts to ints in parameters.
 * 
 * Revision 1.4  1994/11/09  16:35:16  john
 * First version with working RLE bitmaps.
 * 
 * Revision 1.3  1994/10/03  17:18:05  john
 * Fixed bug with edi not getting intialized to zero
 * in create_mask.
 * 
 * Revision 1.2  1994/05/31  11:10:55  john
 * *** empty log message ***
 * 
 * Revision 1.1  1994/05/30  16:08:27  john
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: ibitblt.c 1.6 1994/11/28 17:07:29 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "gr.h"
#include "mem.h"
#include "error.h"
#include "ibitblt.h"

#define MODE_NONE		0
#define MODE_SKIP		1
#define MODE_DRAW		2

// ASCENT: the original functions wrote 386 machine code into the mask
// (ADD ESI, ADD EDI, MOVSB, MOVSW, REP MOVSD, RET) and gr_ibitblt called
// that code. Now the mask is a list of records. Each record is 3 values
// of 4 bytes: the number of bytes to skip in the source, the number of
// bytes to skip in the destination, and the number of bytes to copy.
// A record with a copy count of 0 ends the list (the RET before).
// The mask is still one block from malloc that the caller frees, and
// the copied pixels are the same.
#define IBITBLT_RECORD_SIZE		12

ubyte *Code_pointer = NULL;
int Code_counter = 0;

void move_and_count( int dsource, int ddest, int ecx )
{
	if ( ecx <= 0 )
		return;

	Code_counter += IBITBLT_RECORD_SIZE;
}


void move_and_draw( int dsource, int ddest, int ecx )
{
	int32_t record[3];

	if ( ecx <= 0 )
		return;

	// The original code added to ESI and EDI only for a distance
	// greater than 0.
	record[0] = (dsource > 0) ? dsource : 0;
	record[1] = (ddest > 0) ? ddest : 0;
	record[2] = ecx;
	memcpy( Code_pointer, record, IBITBLT_RECORD_SIZE );
	Code_pointer += IBITBLT_RECORD_SIZE;
}

//-----------------------------------------------------------------------------------------
// Given bitmap, bmp, finds the size of the code 

int gr_ibitblt_find_code_size( grs_bitmap * mask_bmp, int sx, int sy, int sw, int sh, int srowsize )
{
	int x,y;
	ubyte pixel;
	int draw_mode = MODE_NONE;
	int source_offset = 0;
	int dest_offset = 0;
	int num_to_draw, draw_start_source, draw_start_dest;
	int esi, edi;

	Assert( (!(mask_bmp->bm_flags&BM_FLAG_RLE)) );

	Code_counter = 0;

	esi = source_offset = 0;
	edi = dest_offset = 0;
	draw_start_source = draw_start_dest = 0;

	for ( y=sy; y<sy+sh; y++ )	{
		for ( x=sx; x<sx+sw; x++ )	{
			dest_offset = y*mask_bmp->bm_rowsize+x;
			pixel = mask_bmp->bm_data[dest_offset];
			if ( pixel!=255 )	{
				switch ( draw_mode)	{
				case MODE_DRAW:
					move_and_count( draw_start_source-esi, draw_start_dest-edi, num_to_draw );
					esi = draw_start_source + num_to_draw;
					edi = draw_start_dest + num_to_draw;
					// fall through!!!
				case MODE_NONE:
				case MODE_SKIP:
					break;
				}
				draw_mode = MODE_SKIP;
			} else {
				switch ( draw_mode)	{
				case MODE_SKIP:
				case MODE_NONE:
					draw_start_source = source_offset;
					draw_start_dest = dest_offset;
					num_to_draw = 0;
					// fall through
				case MODE_DRAW:
					num_to_draw++;
					break;
				}
				draw_mode = MODE_DRAW;
			}
			source_offset++;
		}
		if ( draw_mode == MODE_DRAW )	{
			move_and_count( draw_start_source-esi, draw_start_dest-edi, num_to_draw );
			esi = draw_start_source + num_to_draw;
			edi = draw_start_dest + num_to_draw;
		}
		draw_mode = MODE_NONE;
		source_offset += (srowsize - sw);
	}
	Code_counter += IBITBLT_RECORD_SIZE;		// for return (ASCENT: the end record)

	//printf( "Code will be %d bytes\n", Code_counter );

	Code_counter += 16;	// for safety

	return Code_counter;
}


//-----------------------------------------------------------------------------------------
// Given bitmap, bmp, create code that transfers a bitmap of size sw*sh to position
// (sx,sy) on top of bmp, only overwritting transparent pixels of the bitmap.

ubyte	*gr_ibitblt_create_mask( grs_bitmap * mask_bmp, int sx, int sy, int sw, int sh, int srowsize )
{
	int x,y;
	ubyte pixel;
	int draw_mode = MODE_NONE;
	int source_offset = 0;
	int dest_offset = 0;
	int num_to_draw, draw_start_source, draw_start_dest;
	int esi, edi;
	int code_size;
	ubyte *code;

	Assert( (!(mask_bmp->bm_flags&BM_FLAG_RLE)) );

	code_size = gr_ibitblt_find_code_size( mask_bmp, sx, sy, sw, sh, srowsize );

	code = malloc( code_size );
	if ( code == NULL )
		return NULL;

	Code_pointer = code;

	esi = source_offset = 0;
	edi = dest_offset = 0;
	draw_start_source = draw_start_dest = 0;

	for ( y=sy; y<sy+sh; y++ )	{
		for ( x=sx; x<sx+sw; x++ )	{
			dest_offset = y*mask_bmp->bm_rowsize+x;
			pixel = mask_bmp->bm_data[dest_offset];
			if ( pixel!=255 )	{
				switch ( draw_mode)	{
				case MODE_DRAW:
					move_and_draw( draw_start_source-esi, draw_start_dest-edi, num_to_draw );
					esi = draw_start_source + num_to_draw;
					edi = draw_start_dest + num_to_draw;
					// fall through!!!
				case MODE_NONE:
				case MODE_SKIP:
					break;
				}
				draw_mode = MODE_SKIP;
			} else {
				switch ( draw_mode)	{
				case MODE_SKIP:
				case MODE_NONE:
					draw_start_source = source_offset;
					draw_start_dest = dest_offset;
					num_to_draw = 0;
					// fall through
				case MODE_DRAW:
					num_to_draw++;
					break;
				}
				draw_mode = MODE_DRAW;
			}
			source_offset++;
		}
		if ( draw_mode == MODE_DRAW )	{
			move_and_draw( draw_start_source-esi, draw_start_dest-edi, num_to_draw );
			esi = draw_start_source + num_to_draw;
			edi = draw_start_dest + num_to_draw;
		}
		draw_mode = MODE_NONE;
		source_offset += (srowsize - sw);
	}
	// ASCENT: the end record (a RET instruction before).
	memset( Code_pointer, 0, IBITBLT_RECORD_SIZE );
	Code_pointer += IBITBLT_RECORD_SIZE;

	if ( Code_pointer >= &code[code_size-1] )	
		Error( "ibitblt overwrote allocated code block\n" );

	//printf( "Code is %d bytes\n", Code_pointer - code );

	return code;
}


// ASCENT: C replacement for the inline assembly that called the
// machine code in the mask. Executes the records of the mask.
static void gr_ibitblt_do_asm(ubyte *start_si, ubyte *start_di, ubyte * code)
{
	int32_t record[3];

	for (;;)	{
		memcpy( record, code, IBITBLT_RECORD_SIZE );
		code += IBITBLT_RECORD_SIZE;
		if ( record[2] <= 0 )
			break;
		start_si += record[0];
		start_di += record[1];
		memmove( start_di, start_si, (size_t)record[2] );
		start_si += record[2];
		start_di += record[2];
	}
}

void gr_ibitblt(grs_bitmap * source_bmp, grs_bitmap * dest_bmp, ubyte * mask )
{
	if (mask != NULL )
		gr_ibitblt_do_asm( source_bmp->bm_data, dest_bmp->bm_data, mask );
}


void	gr_ibitblt_find_hole_size( grs_bitmap * mask_bmp, int *minx, int *miny, int *maxx, int *maxy )
{
	ubyte c;
	int x, y, count=0;

	Assert( (!(mask_bmp->bm_flags&BM_FLAG_RLE)) );

	*minx = mask_bmp->bm_w-1;
 	*maxx = 0;
	*miny = mask_bmp->bm_h-1;
	*maxy = 0;

	for ( y=0; y<mask_bmp->bm_h; y++ )
		for ( x=0; x<mask_bmp->bm_w; x++ )	{
			c = mask_bmp->bm_data[mask_bmp->bm_rowsize*y+x];
			if (c == 255 )	{
				if ( x < *minx ) *minx = x;
				if ( y < *miny ) *miny = y;
				if ( x > *maxx ) *maxx = x;
				if ( y > *maxy ) *maxy = y;
				count++;
			}
		}

	if ( count == 0 )	{
		Error( "Bitmap for ibitblt doesn't have transparency!\n" );
	}
}

