/*
 * ASCENT: C replacement for lbitblt.asm.
 *
 * lbitblt.asm stays in the tree as the reference. It is not built.
 */

#include <string.h>

#include "types.h"
#include "gr.h"
#include "grdef.h"

// Copies a block of width by height pixels from the top left of
// source to the top left of dest. The two bitmaps are linear.
void gr_lbitblt( grs_bitmap * source, grs_bitmap * dest, int height, int width )
{
	ubyte * sbits = source->bm_data;
	ubyte * dbits = dest->bm_data;
	int srowsize = (ushort)source->bm_rowsize;
	int drowsize = (ushort)dest->bm_rowsize;
	int y;

	if (width <= 0)
		return;

	for (y=0; y<height; y++ )	{
		memmove( dbits, sbits, (size_t)width );
		sbits += srowsize;
		dbits += drowsize;
	}
}

// The 4 functions merge two 64x64 textures. A pixel of the upper
// texture that has the transparent color (255) shows the lower texture.
// The functions differ in the rotation of the upper texture.

void gr_merge_textures( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int i;
	ubyte c;

	for (i=0; i<64*64; i++ )	{
		c = upper[i];
		if (c==255)
			c = lower[i];
		dest[i] = c;
	}
}

void gr_merge_textures_1( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y=0; y<64; y++ )
		for (x=0; x<64; x++ )	{
			c = upper[ 64*x+(63-y) ];
			if (c==255)
				c = lower[ 64*y+x ];
			*dest++ = c;
		}
}

void gr_merge_textures_2( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y=0; y<64; y++ )
		for (x=0; x<64; x++ )	{
			c = upper[ 64*(63-y)+(63-x) ];
			if (c==255)
				c = lower[ 64*y+x ];
			*dest++ = c;
		}
}

void gr_merge_textures_3( ubyte * lower, ubyte * upper, ubyte * dest )
{
	int x, y;
	ubyte c;

	for (y=0; y<64; y++ )
		for (x=0; x<64; x++ )	{
			c = upper[ 64*(63-x)+y ];
			if (c==255)
				c = lower[ 64*y+x ];
			*dest++ = c;
		}
}
