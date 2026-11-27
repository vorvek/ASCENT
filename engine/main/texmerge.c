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
 * $Source: f:/miner/source/main/rcs/texmerge.c $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:31:08 $
 * 
 * Routines to cache merged textures.
 * 
 * $Log: texmerge.c $
 * Revision 2.0  1995/02/27  11:31:08  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.28  1995/01/14  19:16:56  john
 * First version of new bitmap paging code.
 * 
 * Revision 1.27  1994/12/14  18:21:58  yuan
 * *** empty log message ***
 * 
 * Revision 1.26  1994/12/13  09:50:08  john
 * Added Asserts to stop if wall looks like door.
 * 
 * Revision 1.25  1994/12/07  00:35:24  mike
 * change how flat shading average color is computed for paste-ons.
 * 
 * Revision 1.24  1994/11/19  15:20:29  mike
 * rip out unused code and data
 * 
 * Revision 1.23  1994/11/12  16:38:51  mike
 * deal with avg_color in texture merging.
 * 
 * Revision 1.22  1994/11/09  19:55:39  john
 * Added full rle support with texture rle caching.
 * 
 * Revision 1.21  1994/10/20  15:21:16  john
 * Took out the texmerge caching.
 * 
 * Revision 1.20  1994/10/10  19:00:57  john
 * Made caching info print every 1000 frames.
 * 
 * Revision 1.19  1994/10/10  18:41:21  john
 * Printed out texture caching info.
 * 
 * Revision 1.18  1994/08/11  18:59:02  mike
 * Use new assembler version of merge functions.
 * 
 * Revision 1.17  1994/06/09  12:13:14  john
 * Changed selectors so that all bitmaps have a selector of
 * 0, but inside the texture mapper they get a selector set.
 * 
 * Revision 1.16  1994/05/14  17:15:15  matt
 * Got rid of externs in source (non-header) files
 * 
 * Revision 1.15  1994/05/09  17:21:09  john
 * Took out mprintf with cache hits/misses.
 * 
 * Revision 1.14  1994/05/05  12:55:07  john
 * Made SuperTransparency work.
 * 
 * Revision 1.13  1994/05/04  11:15:37  john
 * Added Super Transparency
 * 
 * Revision 1.12  1994/04/28  23:36:04  john
 * Took out a debugging mprintf.
 * 
 * Revision 1.11  1994/04/22  17:44:48  john
 * Made top 2 bits of paste-ons pick the
 * orientation of the bitmap.
 * 
 * Revision 1.10  1994/03/31  12:05:51  matt
 * Cleaned up includes
 * 
 * Revision 1.9  1994/03/15  16:31:52  yuan
 * Cleaned up bm-loading code.
 * (And structures)
 * 
 * Revision 1.8  1994/01/24  13:15:19  john
 * Made caching work with pointers, not texture numbers,
 * that way, the animated textures cache.
 * 
 * Revision 1.7  1994/01/21  16:38:10  john
 * Took out debug info.
 * 
 * Revision 1.6  1994/01/21  16:28:43  john
 * added warning to print cache hit/miss.
 * 
 * Revision 1.5  1994/01/21  16:22:30  john
 * Put in caching/
 * 
 * Revision 1.4  1994/01/21  15:34:49  john
 * *** empty log message ***
 * 
 * Revision 1.3  1994/01/21  15:33:08  john
 * *** empty log message ***
 * 
 * Revision 1.2  1994/01/21  15:15:35  john
 * Created new module texmerge, that merges textures together and 
 * caches the results.
 * 
 * Revision 1.1  1994/01/21  14:55:29  john
 * Initial revision
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: texmerge.c 2.0 1995/02/27 11:31:08 john Exp $";
#pragma on (unreferenced)

#include <stdlib.h>
#include <string.h>

#include "gr.h"
#include "error.h"
#include "game.h"
#include "textures.h"
#include "mono.h"
#include "rle.h"
#include "piggy.h"
#include "tmapfx.h"		// ASCENT: the effect colors
#include "effects.h"		// ASCENT: texmerge_mark_wall_effects
#include "args.h"

#define MAX_NUM_CACHE_BITMAPS 50

//static grs_bitmap * cache_bitmaps[MAX_NUM_CACHE_BITMAPS];                     

typedef struct	{
	grs_bitmap * bitmap;
	grs_bitmap * bottom_bmp;
	grs_bitmap * top_bmp;
	int 		orient;
	int		last_frame_used;
} TEXTURE_CACHE;

static TEXTURE_CACHE Cache[MAX_NUM_CACHE_BITMAPS];

static int num_cache_entries = 0;

static int cache_hits = 0;
static int cache_misses = 0;

void texmerge_close();

// ASCENT: prototypes for the two functions below, which the original called
// before their definitions. The merge routines for the four orientations
// (gr_merge_textures, gr_merge_textures_1, _2, _3) were assembly in the 2d
// library. They are C code there now (gr.h).
void merge_textures_new( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, ubyte * dest_data );
void merge_textures_super_xparent( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, ubyte * dest_data );

//----------------------------------------------------------------------

int texmerge_init(int num_cached_textures)
{
	int i;

	if ( num_cached_textures <= MAX_NUM_CACHE_BITMAPS )
		num_cache_entries = num_cached_textures;
	else
		num_cache_entries = MAX_NUM_CACHE_BITMAPS;
	
	for (i=0; i<num_cache_entries; i++ )	{
			// Make temp tmap for use when combining
		Cache[i].bitmap = gr_create_bitmap( 64, 64 );

		//if (get_selector( Cache[i].bitmap->bm_data, 64*64,  &Cache[i].bitmap->bm_selector))
		//	Error( "ERROR ALLOCATING CACHE BITMAP'S SELECTORS!!!!" );

		Cache[i].last_frame_used = -1;
		Cache[i].top_bmp = NULL;
		Cache[i].bottom_bmp = NULL;
		Cache[i].orient = -1;
	}
	atexit( texmerge_close );

	return 1;
}

void texmerge_flush()
{
	int i;

	for (i=0; i<num_cache_entries; i++ )	{
		Cache[i].last_frame_used = -1;
		Cache[i].top_bmp = NULL;
		Cache[i].bottom_bmp = NULL;
		Cache[i].orient = -1;
	}
}


//-------------------------------------------------------------------------
void texmerge_close()
{
	int i;

	for (i=0; i<num_cache_entries; i++ )	{
		gr_free_bitmap( Cache[i].bitmap );
		Cache[i].bitmap = NULL;
	}
}

//--unused-- int info_printed = 0;

grs_bitmap * texmerge_get_cached_bitmap( int tmap_bottom, int tmap_top )
{
	grs_bitmap *bitmap_top, *bitmap_bottom;
	int i, orient;
	int lowest_frame_count;
	int least_recently_used;

//	if ( ((FrameCount % 1000)==0) && ((cache_hits+cache_misses)>0) && (!info_printed) )	{
//		mprintf( 0, "Texmap caching:  %d hits, %d misses. (Missed=%d%%)\n", cache_hits, cache_misses, (cache_misses*100)/(cache_hits+cache_misses)  );
//		info_printed = 1;
//	} else {
//		info_printed = 0;
//	}

	bitmap_top = &GameBitmaps[Textures[tmap_top&0x3FFF].index];
	bitmap_bottom = &GameBitmaps[Textures[tmap_bottom].index];
	
	orient = ((tmap_top&0xC000)>>14) & 3;

	least_recently_used = 0;
	lowest_frame_count = Cache[0].last_frame_used;
	
	for (i=0; i<num_cache_entries; i++ )	{
		if ( (Cache[i].last_frame_used > -1) && (Cache[i].top_bmp==bitmap_top) && (Cache[i].bottom_bmp==bitmap_bottom) && (Cache[i].orient==orient ))	{
			cache_hits++;
			Cache[i].last_frame_used = FrameCount;
			return Cache[i].bitmap;
		}	
		if ( Cache[i].last_frame_used < lowest_frame_count )	{
			lowest_frame_count = Cache[i].last_frame_used;
			least_recently_used = i;
		}
	}

	//---- Page out the LRU bitmap;
	cache_misses++;

	// Make sure the bitmaps are paged in...
#ifdef PIGGY_USE_PAGING
	piggy_page_flushed = 0;

	PIGGY_PAGE_IN(Textures[tmap_top&0x3FFF]);
	PIGGY_PAGE_IN(Textures[tmap_bottom]);
	if (piggy_page_flushed)	{
		// If cache got flushed, re-read 'em.
		piggy_page_flushed = 0;
		PIGGY_PAGE_IN(Textures[tmap_top&0x3FFF]);
		PIGGY_PAGE_IN(Textures[tmap_bottom]);
	}
	Assert( piggy_page_flushed == 0 );
#endif

	if (bitmap_top->bm_flags & BM_FLAG_SUPER_TRANSPARENT)	{
		merge_textures_super_xparent( orient, bitmap_bottom, bitmap_top, Cache[least_recently_used].bitmap->bm_data );
		Cache[least_recently_used].bitmap->bm_flags = BM_FLAG_TRANSPARENT;
		Cache[least_recently_used].bitmap->avg_color = bitmap_top->avg_color;
	} else	{
		merge_textures_new( orient, bitmap_bottom, bitmap_top, Cache[least_recently_used].bitmap->bm_data );
		Cache[least_recently_used].bitmap->bm_flags = bitmap_bottom->bm_flags & ~(BM_FLAG_RLE | BM_FLAG_DTU_EFFECTS);
		Cache[least_recently_used].bitmap->avg_color = bitmap_bottom->avg_color;
	}

	// ASCENT: DTUGAME.EXE sets flag 0x20 on each merged bitmap (0x7AB89),
	// and the texture mappers give its effect colors the colors of the
	// screen (tmapfx.c). Here the flag is set only when the bitmap has one.
	{
		ubyte *p = Cache[least_recently_used].bitmap->bm_data;
		int i;

		for (i=0; i<64*64; i++)
			if (tmap_fx_color_code(p[i])) {
				Cache[least_recently_used].bitmap->bm_flags |= BM_FLAG_DTU_EFFECTS;
				break;
			}
	}

	Cache[least_recently_used].top_bmp = bitmap_top;
	Cache[least_recently_used].bottom_bmp = bitmap_bottom;
	Cache[least_recently_used].last_frame_used = FrameCount;
	Cache[least_recently_used].orient = orient;

	return Cache[least_recently_used].bitmap;
}

// ASCENT: the two bitmaps and the rotation of a bitmap of the cache, for
// the upscaled textures of the GPU renderer. Returns 0 if bm is not in the
// cache.
int texmerge_get_sources( grs_bitmap *bm, grs_bitmap **bottom, grs_bitmap **top, int *orient )
{
	int i;

	for (i=0; i<num_cache_entries; i++ )
		if ( Cache[i].bitmap == bm && Cache[i].last_frame_used > -1 && Cache[i].top_bmp && Cache[i].bottom_bmp )	{
			*bottom = Cache[i].bottom_bmp;
			*top = Cache[i].top_bmp;
			*orient = Cache[i].orient;
			return 1;
		}
	return 0;
}

void merge_textures_new( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, ubyte * dest_data )
{
//	ubyte c;
//	int x,y;
	ubyte * top_data, *bottom_data;

	if ( top_bmp->bm_flags & BM_FLAG_RLE )
		top_bmp = rle_expand_texture(top_bmp);

	if ( bottom_bmp->bm_flags & BM_FLAG_RLE )
		bottom_bmp = rle_expand_texture(bottom_bmp);

//	Assert( bottom_bmp != top_bmp );

	top_data = top_bmp->bm_data;
	bottom_data = bottom_bmp->bm_data;

//	Assert( bottom_data != top_data );

	// mprintf( 0, "Type=%d\n", type );

	switch( type )	{
		case 0:
			// Normal
			

			gr_merge_textures( bottom_data, top_data, dest_data );
			break;
		case 1:
			gr_merge_textures_1( bottom_data, top_data, dest_data );

//			for (y=0; y<64; y++ )
//				for (x=0; x<64; x++ )	{
//					c = top_data[ 64*x+(63-y) ];		
//					if (c==255)
//						c = bottom_data[ 64*y+x ];
//					*dest_data++ = c;
//				}
			break;
		case 2:
			gr_merge_textures_2( bottom_data, top_data, dest_data );
//			for (y=0; y<64; y++ )
//				for (x=0; x<64; x++ )	{
//					c = top_data[ 64*(63-y)+(63-x) ];
//					if (c==255)
//						c = bottom_data[ 64*y+x ];
//					*dest_data++ = c;
//				}
			break;
		case 3:
			gr_merge_textures_3( bottom_data, top_data, dest_data );
//			for (y=0; y<64; y++ )
//				for (x=0; x<64; x++ )	{
//					c = top_data[ 64*(63-x)+y  ];
//					if (c==255)
//						c = bottom_data[ 64*y+x ];
//					*dest_data++ = c;
//				}
			break;
	}
}

void merge_textures_super_xparent( int type, grs_bitmap * bottom_bmp, grs_bitmap * top_bmp, ubyte * dest_data )
{
	ubyte c;
	int x,y;

	ubyte * top_data, *bottom_data;

	if ( top_bmp->bm_flags & BM_FLAG_RLE )
		top_bmp = rle_expand_texture(top_bmp);

	if ( bottom_bmp->bm_flags & BM_FLAG_RLE )
		bottom_bmp = rle_expand_texture(bottom_bmp);

//	Assert( bottom_bmp != top_bmp );

	top_data = top_bmp->bm_data;
	bottom_data = bottom_bmp->bm_data;

//	Assert( bottom_data != top_data );

	//mprintf( 0, "SuperX remapping type=%d\n", type );
	//Int3();
	 
	switch( type )	{
		case 0:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*y+x ];		
					if (c==255)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = 255;
					*dest_data++ = c;
				}
			break;
		case 1:
			// 
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*x+(63-y) ];		
					if (c==255)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = 255;
					*dest_data++ = c;
				}
			break;
		case 2:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*(63-y)+(63-x) ];
					if (c==255)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = 255;
					*dest_data++ = c;
				}
			break;
		case 3:
			// Normal
			for (y=0; y<64; y++ )
				for (x=0; x<64; x++ )	{
					c = top_data[ 64*(63-x)+y  ];
					if (c==255)
						c = bottom_data[ 64*y+x ];
					else if (c==254)
						c = 255;
					*dest_data++ = c;
				}
			break;
	}
}

// ASCENT: Dtu_fix_wall_effects. DTUGAME.EXE turns the rule of the effect
// colors on only for a merged bitmap (flag 0x20, 0x7AB89) and for a tinted
// object (draw_tmap 0xEA503). A wall texture without an overlay draws its
// effect texels in their palette color through the light (black or dark red
// in a dim light, the bright red 63 0 0 near full light): the edges of
// the waterfall wfal04 as the texture of an illusion wall (THIEF0 segments
// 142 and 145), and wfal02, water2, web1, miror01, rg_e, port04 alone on a
// side. With the correction such a bitmap gets the flag as a merged one
// does. 0xCF alone does not count: its palette color is 7 0 0, near the
// color that the rule gives, and the lava walls (lava, lavaflo, magma,
// port03) have only 1 to 4 such texels. -origfx gives the rule of 1.3.
int Dtu_fix_wall_effects = 1;

// ASCENT: Dtu_fix_solid_effects. The rule reads the screen under the face
// (behind) and pixels of the next screen row (below). On a face that the
// view does not see past (a closed wall, a side with no segment behind
// it) the frame draws nothing there before the face, so DTUGAME.EXE shows
// the picture of an earlier frame in the color of the ramp: the round
// glass glas02 on the closed walls of DROW1, DROW2, DROW4 and CLAN1 shows a
// warm, moving smear of the view (the software renderer), and the GPU
// renderer, which clears the view, showed the darkest step of the ramp.
// With the correction render.c draws such a face with the solid form of
// its bitmap (texmap/tmapfx.c tmap_fx_solid_bitmap, the shader of the GPU
// renderer): each effect texel takes the rule with the color of the
// texels around it. -origfx gives the rule of 1.3.
int Dtu_fix_solid_effects = 1;

// 1 if the bitmap has an effect color other than 0xCF
static int bitmap_has_red_effect(grs_bitmap *bm)
{
	const ubyte *p = bm->bm_data, *s, *end;
	int32_t size;
	int i;

	if (!p || bm->bm_w <= 0 || bm->bm_h <= 0)
		return 0;
	if (!(bm->bm_flags & BM_FLAG_RLE)) {
		for (i=0; i<bm->bm_h*bm->bm_rowsize; i++)
			if (p[i] != 0xCF && tmap_fx_color_code(p[i]))
				return 1;
		return 0;
	}
	// the RLE form of gr_rle_decode: a 32-bit size, a byte for each row,
	// then the rows; a byte 0xE0 + n is a run of n of the next byte, 0xE0
	// ends a row
	memcpy(&size, p, 4);
	if (size < 4 + bm->bm_h || size > 4 + bm->bm_h + bm->bm_h * (bm->bm_w * 2 + 2))
		return 0;
	s = p + 4 + bm->bm_h;
	end = p + size;
	while (s < end) {
		ubyte c = *s++;
		if ((c & 0xE0) == 0xE0) {
			if ((c & 0x1F) == 0 || s >= end)
				continue;
			c = *s++;
		}
		if (c != 0xCF && tmap_fx_color_code(c))
			return 1;
	}
	return 0;
}

static void mark_wall_bitmap(grs_bitmap *bm)
{
	if (Dtu_fix_wall_effects && bitmap_has_red_effect(bm))
		bm->bm_flags |= BM_FLAG_DTU_EFFECTS;
	else
		bm->bm_flags &= ~BM_FLAG_DTU_EFFECTS;
}

// The wall bitmaps of the tables of a level: the textures and the frames
// of the effect clips that change a wall texture.
void texmerge_mark_wall_effects(void)
{
	int i, j;

	if (FindArg("-origfx"))
		Dtu_fix_wall_effects = Dtu_fix_solid_effects = 0;
	for (i=0; i<NumTextures; i++)
		mark_wall_bitmap(&GameBitmaps[Textures[i].index]);
	for (i=0; i<MAX_EFFECTS; i++)
		if (Effects[i].changing_wall_texture != -1)
			for (j=0; j<Effects[i].vc.num_frames && j<VCLIP_MAX_FRAMES; j++)
				mark_wall_bitmap(&GameBitmaps[Effects[i].vc.frames[j].index]);
}
