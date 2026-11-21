/*
 * ASCENT: C replacement for linear.asm, and for the inline assembly
 * functions (#pragma aux) that bitblt.c had for linear bitmaps.
 *
 * linear.asm stays in the tree as the reference. It is not built.
 * Each function gives the same pixels as the assembly.
 */

#include <string.h>

#include "types.h"
#include "gr.h"
#include "grdef.h"

unsigned int gr_var_color;
unsigned char * gr_var_bitmap;
unsigned int gr_var_bwidth;

// Draws one horizontal run of the X-major line. The assembly uses
// REP STOSB with the direction flag set for a right to left line.
static ubyte * line_hrun( ubyte * p, uint32_t count, int xadvance, ubyte color )
{
	while (count--)	{
		*p = color;
		p += xadvance;
	}
	return p;
}

// Draws one vertical run of the Y-major line. The assembly loop tests
// the count after it draws the pixel, and the count is never 0.
static ubyte * line_vrun( ubyte * p, uint32_t count, int bwidth, ubyte color )
{
	do {
		*p = color;
		p += bwidth;
	} while (--count);
	return p;
}

// Fast run length slice line drawing implementation.
// Draws a line between the specified endpoints in color gr_var_color
// into the bitmap gr_var_bitmap, which has gr_var_bwidth bytes for each row.
// The function does not clip.
void gr_linear_line( int x0, int y0, int x1, int y1 )
{
	int bwidth = (int)gr_var_bwidth;
	ubyte color = (ubyte)gr_var_color;
	ubyte * p;
	int xdelta, ydelta, xadvance, i;
	uint32_t whole_step, adj_up, adj_down, error_term;
	uint32_t initial_count, final_count, count;

	// We'll draw top to bottom, to reduce the number of cases we have to handle,
	// and to make lines between the same endpoints always draw the same pixels.
	if (y0 > y1)	{
		int t;
		t = y0; y0 = y1; y1 = t;
		t = x0; x0 = x1; x1 = t;
	}

	p = gr_var_bitmap + (ptrdiff_t)y0 * bwidth + x0;

	ydelta = y1 - y0;		// guaranteed to be positive
	xdelta = x1 - x0;

	if (xdelta == 0)	{
		// Vertical line
		for (i=ydelta; i>=0; i--)	{
			*p = color;
			p += bwidth;
		}
		return;
	}

	xadvance = 1;
	if (xdelta < 0)	{
		xadvance = -1;
		xdelta = -xdelta;
	}

	if (ydelta == 0)	{
		// Horizontal line. Always drawn left to right.
		if (xadvance < 0)
			p -= xdelta;
		memset( p, color, (size_t)xdelta + 1 );
		return;
	}

	if (ydelta == xdelta)	{
		// Diagonal line
		int step = xadvance + bwidth;
		for (i=ydelta; i>=0; i--)	{
			*p = color;
			p += step;
		}
		return;
	}

	if (xdelta >= ydelta)	{
		// X-major (more horizontal than vertical) line.
		whole_step = (uint32_t)xdelta / (uint32_t)ydelta;	// minimum # of pixels in a run
		adj_up = ((uint32_t)xdelta % (uint32_t)ydelta) * 2;
		adj_down = (uint32_t)ydelta * 2;

		// Initial error term; reflects an initial step of 0.5 along the Y axis.
		error_term = ((uint32_t)xdelta % (uint32_t)ydelta) - adj_down;

		// The initial and last runs are partial, because Y advances only 0.5 for
		// these runs, rather than 1.
		initial_count = (whole_step >> 1) + 1;
		final_count = initial_count;

		if (whole_step & 1)	{
			// Odd run length: add 0.5 of a pixel to the error term.
			error_term += (uint32_t)ydelta;
		} else if (adj_up == 0)	{
			// Even run length and no fractional advance: the extra pixel
			// goes to the last run.
			initial_count--;
		}

		// Draw the first, partial run of pixels.
		p = line_hrun( p, initial_count, xadvance, color );
		p += bwidth;

		// Draw all full runs. There are ydelta-1 of them.
		if (ydelta > 1)	{
			// The assembly adjusts the error term by -1 so that it can use
			// the carry flag of the addition as the test.
			error_term--;
			for (i=0; i<ydelta-1; i++)	{
				count = whole_step;
				error_term += adj_up;
				if (error_term < adj_up)	{		// carry
					count++;
					error_term -= adj_down;
				}
				p = line_hrun( p, count, xadvance, color );
				p += bwidth;
			}
		}

		// Draw the final run of pixels.
		line_hrun( p, final_count, xadvance, color );
	} else {
		// Y-major (more vertical than horizontal) line.
		whole_step = (uint32_t)ydelta / (uint32_t)xdelta;
		adj_up = ((uint32_t)ydelta % (uint32_t)xdelta) * 2;
		adj_down = (uint32_t)xdelta * 2;

		// Initial error term; reflects an initial step of 0.5 along the X axis.
		error_term = ((uint32_t)ydelta % (uint32_t)xdelta) - adj_down;

		initial_count = (whole_step >> 1) + 1;
		final_count = initial_count;

		if (whole_step & 1)	{
			error_term += (uint32_t)xdelta;
		} else if (adj_up == 0)	{
			initial_count--;
		}

		// Draw the first, partial run of pixels.
		p = line_vrun( p, initial_count, bwidth, color );
		p += xadvance;

		// Draw all full runs. There are xdelta-1 of them.
		if (xdelta > 1)	{
			error_term--;
			for (i=0; i<xdelta-1; i++)	{
				count = whole_step;
				error_term += adj_up;
				if (error_term < adj_up)	{		// carry
					count++;
					error_term -= adj_down;
				}
				p = line_vrun( p, count, bwidth, color );
				p += xadvance;
			}
		}

		// Draw the final run of pixels.
		line_vrun( p, final_count, bwidth, color );
	}
}

// Fills nbytes bytes at dest with color.
// ASCENT: the assembly does not test for a count that is less than 1
// (it then writes over 4 GB of memory). The C function draws nothing.
void gr_linear_stosd( void * dest, unsigned char color, int nbytes )
{
	if (nbytes > 0)
		memset( dest, color, (size_t)nbytes );
}

// Compares two source buffers in units of 4 bytes and copies each
// unit that is different from sbuf1 to the same offset in dbuf.
// sbuf1 = latest source buffer, sbuf2 = earlier source buffer.
// The assembly also writes the last unit that it compares, which this
// function does too. ASCENT: for a size less than 4 the assembly reads
// and writes 4 bytes before the buffers. The C function does nothing.
void gr_update_buffer( void * sbuf1, void * sbuf2, void * dbuf, int size )
{
	ubyte * s1 = (ubyte *)sbuf1;
	ubyte * s2 = (ubyte *)sbuf2;
	ubyte * d = (ubyte *)dbuf;
	uint32_t count = (uint32_t)size >> 2;
	size_t i = 0;

	if (count == 0)
		return;

	do {
		// repe cmpsd
		while (count)	{
			int equal = (memcmp( s1+i, s2+i, 4 ) == 0);
			i += 4;
			count--;
			if (!equal)
				break;
		}
		memcpy( d+i-4, s1+i-4, 4 );
	} while (count != 0);
}

// Copies num_pixels bytes. The assembly aligns the destination and
// uses REP MOVSD, which is a forward copy.
// ASCENT: memmove gives the same result, except for a source that
// starts less than num_pixels bytes before the destination. No caller
// does that.
void gr_linear_movsd( ubyte * src, ubyte * dest, int num_pixels )
{
	if (num_pixels > 0)
		memmove( dest, src, (size_t)num_pixels );
}

// Copies num_pixels bytes. Does not write the transparent color (255).
// ASCENT: the assembly loop does not test for a count of 0.
void gr_linear_rep_movsdm( ubyte * src, ubyte * dest, int num_pixels )
{
	int i;
	ubyte c;

	for (i=0; i<num_pixels; i++ )	{
		c = src[i];
		if (c != 255)
			dest[i] = c;
	}
}

// The same as gr_linear_rep_movsdm, but each pixel goes through the
// row fade_value of gr_fade_table.
void gr_linear_rep_movsdm_faded( ubyte * src, ubyte * dest, int num_pixels, ubyte fade_value )
{
	int i;
	ubyte c;
	ubyte * fade = &gr_fade_table[(int)fade_value << 8];

	for (i=0; i<num_pixels; i++ )	{
		c = src[i];
		if (c != 255)
			dest[i] = fade[c];
	}
}

// Writes num_dest_pixels bytes. Each source pixel becomes 2 pixels.
// If the count is odd, the first source pixel becomes 1 pixel.
void gr_linear_rep_movsd_2x( ubyte * src, ubyte * dest, int num_dest_pixels )
{
	int pairs;
	ubyte c;

	if (num_dest_pixels <= 0)
		return;

	if (num_dest_pixels & 1)
		*dest++ = *src++;

	for (pairs = num_dest_pixels >> 1; pairs > 0; pairs-- )	{
		c = *src++;
		*dest++ = c;
		*dest++ = c;
	}
}
