/*
 * ASCENT: C replacement for vesa.asm (VESA BIOS modes, bank switching,
 * and the DPMI selector for segment A000h).
 *
 * vesa.asm stays in the tree as the reference. It is not built.
 *
 * There is no VESA BIOS and no 64K window now. An SVGA screen mode is a
 * linear 8-bit frame in memory, the same as each other mode (see gr.c).
 * The functions here keep their names and parameters. A "vesa address"
 * is an offset from the start of the video memory (gr_video_memory).
 * The 2d library does not call the drawing functions: it routes BM_SVGA
 * bitmaps to the linear code.
 */

#include <string.h>

#include "types.h"
#include "gr.h"
#include "grdef.h"

// There is no selector to allocate. Returns 0 (no error).
int gr_init_A0000(void)
{
	return 0;
}

// Returns 0 if gr_set_mode can set this VESA mode number, or an error
// code from the list in gr.h.
// ASCENT: the frame has 8 bits for each pixel, so the 15-bit modes
// (110h and 113h) give error 4 (the video card doesn't support the mode).
int gr_vesa_checkmode(int mode)
{
	switch( mode )
	{
	case 0x100:		// 640x400
	case 0x101:		// 640x480
	case 0x102:		// 800x600, 16 colors (gr.c makes 400x600 from it)
	case 0x103:		// 800x600
	case 0x105:		// 1024x768
		return 0;
	}
	return 4;
}

int gr_vesa_setmodea(int mode)
{
	return gr_vesa_checkmode( mode );
}

// There are no 64K pages.
void gr_vesa_setpage(int page)
{
	page = page;
}

void gr_vesa_incpage(void)
{
}

// Sets the first pixel that the display shows, then shows the frame.
void gr_vesa_setstart(short x, short y )
{
	gr_display_start = (int)y * grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize + x;
	gr_present();
}

// The row size of the frame is always the width of the mode.
int gr_vesa_setlogical(int pixels_per_scanline)
{
	return pixels_per_scanline;
}

// x1 and x2 don't need to be sorted.
void gr_vesa_scanline(short x1, short x2, short y, unsigned char color )
{
	if (x2 < x1)	{
		short t = x1; x1 = x2; x2 = t;
	}
	memset( gr_video_memory + (int)y * (int)gr_var_bwidth + x1, color, (size_t)(x2 - x1 + 1) );
}

void gr_vesa_pixel( unsigned char color, unsigned int offset )
{
	gr_video_memory[offset] = color;
}

// Copies a block of width by height pixels to the video memory.
// The source rows have no space between them. The step between the
// destination rows is gr_var_bwidth.
void gr_vesa_bitblt( unsigned char * source_ptr, unsigned int vesa_address, int height, int width )
{
	ubyte * dbits = gr_video_memory + vesa_address;
	int y;

	if (width <= 0)
		return;

	for (y=0; y<height; y++ )	{
		memmove( dbits, source_ptr, (size_t)width );
		source_ptr += width;
		dbits += gr_var_bwidth;
	}
}

// Draws the linear bitmap source at x,y in the bitmap dest.
void gr_vesa_bitmap( grs_bitmap * source, grs_bitmap * dest, int x, int y )
{
	ubyte * sbits = source->bm_data;
	ubyte * dbits = dest->bm_data + (ptrdiff_t)y * (int)gr_var_bwidth + x;
	int row;

	for (row=0; row<source->bm_h; row++ )	{
		memmove( dbits, sbits, (size_t)source->bm_w );
		sbits += source->bm_rowsize;
		dbits += dest->bm_rowsize;
	}
}

// Copies to dest each pixel of source1 that is different from the
// pixel of source2 (the earlier frame). The size is that of source1.
// The assembly compares units of 4 bytes, then single bytes, and also
// writes the last unit that it compares. This function does the same.
// ASCENT: the assembly reads before the row if the width is less than 4.
// The C function compares only the bytes of the row.
static void vesa_update_row( ubyte * s1, ubyte * s2, ubyte * d, int count, int unit )
{
	int i = 0;

	if (count <= 0)
		return;

	do {
		// repe cmpsd / repe cmpsb
		while (count)	{
			int equal = (memcmp( s1+i, s2+i, (size_t)unit ) == 0);
			i += unit;
			count--;
			if (!equal)
				break;
		}
		memcpy( d+i-unit, s1+i-unit, (size_t)unit );
	} while (count != 0);
}

void gr_vesa_update( grs_bitmap * source1, grs_bitmap * dest, grs_bitmap * source2 )
{
	ubyte * s1 = source1->bm_data;
	ubyte * s2 = source2->bm_data;
	ubyte * d = dest->bm_data;
	int w = source1->bm_w;
	int row;

	for (row=0; row<source1->bm_h; row++ )	{
		vesa_update_row( s1, s2, d, w >> 2, 4 );
		vesa_update_row( s1 + (w & ~3), s2 + (w & ~3), d + (w & ~3), w & 3, 1 );
		s1 += source1->bm_rowsize;
		s2 += source1->bm_rowsize;		// the assembly uses the row size of source1 for the two
		d += dest->bm_rowsize;
	}
}
