/*
 * ASCENT: C version of tmap_flt.asm, the flat scanline renderer.
 * tmap_flt.asm stays in the tree as the reference. It is not built.
 *
 * Same results as the assembly for a span inside the canvas:
 *   - With tmap_flat_cthru_table == NULL the span is xright-xleft+1 pixels of
 *     tmap_flat_color.
 *   - With a cthru table each pixel on the screen goes through the table. The
 *     assembly loop draws one pixel less here (xright-xleft pixels). This is kept.
 *
 * Difference: when fx_xleft is negative the assembly started at x = 0 and
 * drew xright-xleft+1 pixels, which goes past xright. Here the span stops
 * at xright, which is not past the right edge of the canvas.
 */

#include <string.h>

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"

// --------------------------------------------------------------------------------------------------
// Enter:
//	fx_xleft	left x coordinate (int)
//	fx_xright	right x coordinate (int)
//	fx_y	y coordinate (int)
void asm_tmap_scanline_flat(void)
{
	ubyte	*dest;
	int	x, xright, count, n;

	if ((uint) fx_y > (uint) window_bottom)
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
		count = window_width - 1;

	n = tmap_flat_cthru_table ? count : count + 1;

	// ASCENT: do not draw past xright.
	if (n > xright - x + 1)
		n = xright - x + 1;
	if (n <= 0)
		return;

	dest = write_buffer + fx_y * bytes_per_row + x;

	if (!tmap_flat_cthru_table) {
		memset(dest, tmap_flat_color, n);
	} else {
		const ubyte	*table = tmap_flat_cthru_table;

		do {
			*dest = table[*dest];		// xlat already drawn pixel thru cthru table
			dest++;
		} while (--n);
	}
}
