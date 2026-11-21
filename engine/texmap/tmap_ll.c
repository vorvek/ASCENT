/*
 * ASCENT: C version of tmap_ll.asm, the linear scanline renderer with lighting.
 * tmap_ll.asm stays in the tree as the reference. It is not built.
 *
 * Same results as the assembly (the "loop_test = 1" code, which is the code
 * that the assembler built):
 *   - u and v are packed in one 32 bit register, v in the high word and u in
 *     the low word, each with 6 bits integer and 10 bits fraction. The low 6
 *     bits of the fraction of each coordinate are dropped and a carry out of
 *     u goes into the fraction of v.
 *   - The lighting value has 8 bits of fraction. A negative delta rounds
 *     towards 0.
 *   - With transparency, the lighting value does not advance on a
 *     transparent pixel.
 *   - The dither variables (dither_intensity_lighting) were computed and not
 *     used by the loop. They are not ported.
 *
 * Differences: a lighting value outside gr_fade_table is clamped (see
 * tmap_guard_light), the span does not go past xright, and fx_dl_dx is not
 * shifted in place.
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
//	fx_l	fixed point initial lighting value
//	fx_dl_dx	fixed point dl/dx
void asm_tmap_scanline_lin_lighted(void)
{
	const ubyte	*src = pixptr;
	const ubyte	*fade = gr_fade_table;
	ubyte	*dest;
	int	x, xright, count, n;
	uint32_t	uv, duv;
	fix	l, dl;
	uint	c;

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

	count = xright - x;
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

	// u, v coordinates and deltas 6:10:6:10
	uv = (((uint32_t) fx_v << 10) & 0xffff0000u) | (((uint32_t) fx_u >> 6) & 0xffffu);
	duv = (((uint32_t) fx_dv_dx << 10) & 0xffff0000u) | (((uint32_t) fx_du_dx >> 6) & 0xffffu);

	// lighting values are passed in fixed point, but need to be in 8 bit integer, 8 bit fraction
	dl = fx_dl_dx >> 8;
	if (dl < 0)
		dl++;		// round towards 0 for negative deltas
	l = fx_l >> 8;

	tmap_guard_light(&l, &dl, n - 1);

	if (!Transparency_on) {
		do {
			c = src[TMAP_UV_INDEX(uv)];
			uv += duv;
			*dest++ = fade[(l & 0xff00) | c];
			l += dl;
		} while (--n);
	} else {
		do {
			c = src[TMAP_UV_INDEX(uv)];
			uv += duv;
			if (c != 255) {
				*dest = fade[(l & 0xff00) | c];
				l += dl;
			}
			dest++;
		} while (--n);
	}
}
