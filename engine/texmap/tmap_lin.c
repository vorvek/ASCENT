/*
 * ASCENT: C version of tmap_lin.asm, the linear scanline renderer with no lighting.
 * tmap_lin.asm stays in the tree as the reference. It is not built.
 *
 * Same results as the assembly for a span inside the canvas:
 *   - u and v step in 32 bit registers that hold the coordinate shifted left
 *     by 10. The top 6 bits are the texture coordinate.
 *   - With no transparency the span has xright-xleft+1 pixels. With
 *     transparency the assembly loop draws one pixel less (xright-xleft
 *     pixels). This is kept.
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
//	pixptr	address of source pixel map
//	fx_u	fixed point initial u coordinate
//	fx_v	fixed point initial v coordinate
//	fx_du_dx	fixed point du/dx
//	fx_dv_dx	fixed point dv/dx
void asm_tmap_scanline_lin(void)
{
	const ubyte	*src = pixptr;
	ubyte	*dest;
	int	x, xright, count, n;
	uint32_t	u, v, du, dv;
	uint	c;

	if ((uint) fx_y > (uint) window_bottom)
		return;

	x = fx_xleft;
	if (x < 0)
		x = 0;

	// the assembly compares xright as an unsigned number here
	xright = fx_xright;
	if ((uint) xright >= (uint) window_right)
		xright = window_right;
	if ((uint) xright <= (uint) window_left)
		xright = window_left;

	count = xright - fx_xleft;
	if (count < 0)
		return;
	if ((uint) count > (uint) window_width)
		count = window_width;

	n = Transparency_on ? count : count + 1;

	// ASCENT: do not draw past xright.
	if (n > xright - x + 1)
		n = xright - x + 1;
	if (n <= 0)
		return;

	dest = write_buffer + fx_y * bytes_per_row + x;

	u = (uint32_t) fx_u << 10;
	v = (uint32_t) fx_v << 10;
	du = (uint32_t) fx_du_dx << 10;
	dv = (uint32_t) fx_dv_dx << 10;

	if (!Transparency_on) {
		do {
			*dest++ = src[((v >> 26) << 6) | (u >> 26)];
			v += dv;
			u += du;
		} while (--n);
	} else {
		do {
			c = src[((v >> 26) << 6) | (u >> 26)];
			v += dv;
			u += du;
			if (c != 255)
				*dest = c;
			dest++;
		} while (--n);
	}
}
