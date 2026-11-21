/*
 * ASCENT: C version of tmapfade.asm, the scanline renderer that darkens the
 * pixels on the screen through gr_fade_table.
 * tmapfade.asm stays in the tree as the reference. It is not built.
 *
 * Same results as the assembly for a span inside the canvas. The assembly
 * skips the scanline when fx_y >= window_bottom (the other renderers use >),
 * thus the last row of the canvas is never darkened. This is kept.
 *
 * Difference: when fx_xleft is negative the assembly started at x = 0 and
 * drew xright-xleft+1 pixels, which goes past xright. Here the span stops
 * at xright, which is not past the right edge of the canvas.
 */

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"

// --------------------------------------------------------------------------------------------------
// Enter:
//	fx_xleft	left x coordinate (int)
//	fx_xright	right x coordinate (int)
//	fx_y	y coordinate (int)
//	tmap_flat_shade_value	row of gr_fade_table (0..31)
void asm_tmap_scanline_shaded(void)
{
	const ubyte	*fade;
	ubyte	*dest;
	int	x, xright, count, n;

	if ((uint) fx_y >= (uint) window_bottom)
		return;

	x = fx_xleft;
	if (x < 0)
		x = 0;

	xright = fx_xright;
	if (xright >= window_right)
		xright = window_right;
	if (xright <= window_left)
		xright = window_left;

	count = xright - fx_xleft;
	if (count < 0)
		return;
	if ((uint) count > (uint) window_width)
		count = window_width;

	n = count + 1;

	// ASCENT: do not draw past xright.
	if (n > xright - x + 1)
		n = xright - x + 1;
	if (n <= 0)
		return;

	dest = write_buffer + fx_y * bytes_per_row + x;
	fade = gr_fade_table + ((tmap_flat_shade_value & 31) << 8);

	do {
		*dest = fade[*dest];		// darken pixel
		dest++;
	} while (--n);
}
