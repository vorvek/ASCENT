/*
 * ASCENT: C version of tmap_per.asm, the perspective scanline renderer.
 * tmap_per.asm stays in the tree as the reference. It is not built.
 *
 * The code keeps the arithmetic of the assembly:
 *   - 32 bit wrapping steps of u, v, z and the lighting value,
 *   - one divide for each pixel until the write address is a multiple of 4,
 *   - then one pair of divides for each 16 pixels (NBITS) with 4 extra bits
 *     of precision (ZSHIFT), and linear steps between them in a packed
 *     6:10,6:10 register (a carry out of u goes into the fraction of v),
 *   - the two ways to finish the last 1 to 15 pixels,
 *   - the lighting value does not advance on a transparent pixel in the
 *     16 pixel loop with Transparency_on, and does advance in the other loops.
 *
 * Differences from the assembly:
 *   - The assembly did no range check and trusted the clipper. Here a scanline
 *     outside the canvas is skipped and the right end is clipped to the canvas.
 *   - A divide by zero or a quotient that does not fit in 32 bits was a
 *     processor exception. Here a scanline that starts with z == 0 is skipped
 *     and a quotient is truncated to 32 bits.
 *   - A lighting value outside gr_fade_table is clamped (see tmap_guard_light).
 *   - The assembly shifted fx_l and fx_dl_dx in place. Here they are not changed.
 *   - The unreachable z fix loop (dep_loop) is not ported.
 */

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"
#include "tmapfx.h"

#include <stdlib.h>
#include <string.h>

#define NBITS	4		// 2^NBITS pixels plotted per divide
#define ZSHIFT	4		// precision used in PDIV macro
#define NPIXS	(1 << NBITS)

//	idiv with a sign extended 32 bit dividend. z is never 0 here.
//	The quotient of 0x80000000 / -1 does not fit: it was a processor
//	exception, here it wraps.
static inline fix tmap_idiv(fix a, fix z)
{
	return (z == -1) ? -a : a / z;
}

//	Texture index for a divide for each coordinate.
#define DIV_INDEX(u,v,z)	(((tmap_idiv(v,z) & 63) << 6) | (tmap_idiv(u,z) & 63))

//	The PDIV macro: returns a/z in 16.16 format with ZSHIFT bits of fraction.
static inline fix tmap_pdiv(fix a, fix z)
{
	return (fix) ((uint32_t) (((int64_t) a * (1 << ZSHIFT)) / z) << (16 - ZSHIFT));
}

//	Pack V0:U0 and the deltas to V1:U1 in 6:10,6:10 format.
#define PACK_UV(U0,V0)	((((uint32_t) (V0) << 10) & 0xffff0000u) | (((uint32_t) (U0) >> 6) & 0xffffu))
#define PACK_DUV(U0,V0,U1,V1)	\
	((((uint32_t) ((V1) - (V0)) << (10 - NBITS)) & 0xffff0000u) | ((uint32_t) (((U1) - (U0)) >> (NBITS + 6)) & 0xffffu))

// -------------------------------------------------------------------------------------
//	tmap_loop: one divide for each pixel. Draws loop_count+1 pixels.
//	With lighting, the test for the transparent color is on the lighted pixel.
static void per_loop_divide(ubyte *dest, int loop_count, fix u, fix v, fix z, fix l, fix dl)
{
	const ubyte	*src = pixptr;
	const ubyte	*fade = gr_fade_table;
	fix	du = fx_du_dx, dv = fx_dv_dx, dz = fx_dz_dx;
	uint	c;

	if (Lighting_on) {
		do {
			c = fade[(l & 0xff00) | src[DIV_INDEX(u,v,z)]];
			l += dl;
			if (c != 255)
				*dest = c;
			dest++;
			v += dv;
			u += du;
			z += dz;
			if (z == 0)
				return;		// would be dividing by 0, so abort
		} while (--loop_count >= 0);
	} else {
		do {
			c = src[DIV_INDEX(u,v,z)];
			if (c != 255)
				*dest = c;
			dest++;
			v += dv;
			u += du;
			z += dz;
			if (z == 0)
				return;
		} while (--loop_count >= 0);
	}
}

// -------------------------------------------------------------------------------------
//	tmap_loop_fast: lighting, one pair of divides for each 16 pixels.
static void per_loop_fast(ubyte *dest, int loop_count, fix u, fix v, fix z, fix l, fix dl)
{
	const ubyte	*src = pixptr;
	const ubyte	*fade = gr_fade_table;
	fix	du = fx_du_dx, dv = fx_dv_dx, dz = fx_dz_dx;
	fix	U0, V0, U1, V1, DU1, DV1, DZ1;
	uint32_t	uv, duv;
	uint	c, lv;
	int	num_left_over, i;

	// Getting dword aligned
	while ((uintptr_t) dest & 3) {
		c = src[DIV_INDEX(u,v,z)];
		lv = l & 0xff00;
		l += dl;
		if (c != 255)		// this pixel is transparent, so don't write it (or light it)
			*dest = fade[lv | c];
		dest++;
		v += dv;
		u += du;
		z += dz;
		if (z == 0)
			return;
		if (--loop_count < 0)
			return;
	}

	num_left_over = (loop_count + 1) & (NPIXS - 1);
	if (((loop_count + 1) >> NBITS) == 0) {
		// there are no 2^NBITS chunks, do divide/pixel for whole scanline
		per_loop_divide(dest, loop_count, u, v, z, l, dl);
		return;
	}
	loop_count = (loop_count + 1) >> NBITS;

	V0 = tmap_pdiv(v, z);
	U0 = tmap_pdiv(u, z);

	// Set deltas to NPIXS pixel increments
	DU1 = (fix) ((uint32_t) du << NBITS);
	DV1 = (fix) ((uint32_t) dv << NBITS);
	DZ1 = (fix) ((uint32_t) dz << NBITS);

	do {
		u += DU1;
		v += DV1;
		z += DZ1;
		if (z == 0)
			return;

		U1 = tmap_pdiv(u, z);
		V1 = tmap_pdiv(v, z);

		uv = PACK_UV(U0, V0);
		duv = PACK_DUV(U0, V0, U1, V1);

		// Save the U1 and V1 so we don't have to divide on the next iteration
		U0 = U1;
		V0 = V1;

		if (Transparency_on) {
			for (i = 0; i < NPIXS; i++) {
				c = src[TMAP_UV_INDEX(uv)];
				uv += duv;
				if (c != 255) {
					dest[i] = fade[(l & 0xff00) | c];
					l += dl;
				}
			}
		} else {
			for (i = 0; i < NPIXS; i++) {
				c = src[TMAP_UV_INDEX(uv)];
				uv += duv;
				dest[i] = fade[(l & 0xff00) | c];
				l += dl;
			}
		}
		dest += NPIXS;
	} while (--loop_count);

	if (num_left_over == 0)
		return;

	// DoEndPixels.
	// If z + DZ1 is negative, or z falls by more than a quarter in the next
	// 16 pixels, do a divide for each pixel (new_end was always 1).
	{
		fix	z3 = (fix) ((uint32_t) z * 3);
		fix	znext = z + DZ1;

		if (znext < 0 || !(z3 < (fix) ((uint32_t) znext << 2))) {
			// NewDoEndPixels
			do {
				c = src[DIV_INDEX(u,v,z)];
				lv = l & 0xff00;
				l += dl;
				if (c != 255)
					*dest = fade[lv | c];
				dest++;
				v += dv;
				u += du;
				z += dz;
				if (z == 0)
					return;
			} while (--num_left_over);
			return;
		}
	}

	u += DU1;
	v += DV1;
	z += DZ1;
	if (z == 0)
		return;
	if (z <= (1 << (ZSHIFT+1)))
		z = 1 << (ZSHIFT+1);

	U1 = tmap_pdiv(u, z);
	V1 = tmap_pdiv(v, z);

	uv = PACK_UV(U0, V0);
	duv = PACK_DUV(U0, V0, U1, V1);

	do {
		c = src[TMAP_UV_INDEX(uv)];
		uv += duv;
		lv = l & 0xff00;
		l += dl;
		if (c != 255)
			*dest = fade[lv | c];
		dest++;
	} while (--num_left_over);
}

// -------------------------------------------------------------------------------------
//	tmap_loop_fast_nolight: no lighting, one pair of divides for each 16 pixels.
static void per_loop_fast_nolight(ubyte *dest, int loop_count, fix u, fix v, fix z, fix l, fix dl)
{
	const ubyte	*src = pixptr;
	fix	du = fx_du_dx, dv = fx_dv_dx, dz = fx_dz_dx;
	fix	U0, V0, U1, V1, DU1, DV1, DZ1;
	uint32_t	uv, duv;
	uint	c;
	int	num_left_over, i;

	// Getting dword aligned
	while ((uintptr_t) dest & 3) {
		c = src[DIV_INDEX(u,v,z)];
		if (c != 255)
			*dest = c;
		dest++;
		v += dv;
		u += du;
		z += dz;
		if (z == 0)
			return;
		if (--loop_count < 0)
			return;
	}

	num_left_over = (loop_count + 1) & (NPIXS - 1);
	if (((loop_count + 1) >> NBITS) == 0) {
		// no 2^NBITS chunks, do divide/pixel for whole scanline
		per_loop_divide(dest, loop_count, u, v, z, l, dl);
		return;
	}
	loop_count = (loop_count + 1) >> NBITS;

	V0 = tmap_pdiv(v, z);
	U0 = tmap_pdiv(u, z);

	DU1 = (fix) ((uint32_t) du << NBITS);
	DV1 = (fix) ((uint32_t) dv << NBITS);
	DZ1 = (fix) ((uint32_t) dz << NBITS);

	do {
		u += DU1;
		v += DV1;
		z += DZ1;
		if (z == 0)
			return;

		U1 = tmap_pdiv(u, z);
		V1 = tmap_pdiv(v, z);

		uv = PACK_UV(U0, V0);
		duv = PACK_DUV(U0, V0, U1, V1);

		U0 = U1;
		V0 = V1;

		if (Transparency_on) {
			for (i = 0; i < NPIXS; i++) {
				c = src[TMAP_UV_INDEX(uv)];
				uv += duv;
				if (c != 255)
					dest[i] = c;
			}
		} else {
			for (i = 0; i < NPIXS; i++) {
				dest[i] = src[TMAP_UV_INDEX(uv)];
				uv += duv;
			}
		}
		dest += NPIXS;
	} while (--loop_count);

	if (num_left_over == 0)
		return;

	// DoEndPixels_nolight
	u += DU1;
	v += DV1;
	z += DZ1;
	if (z == 0)
		return;

	U1 = tmap_pdiv(u, z);
	V1 = tmap_pdiv(v, z);

	uv = PACK_UV(U0, V0);
	duv = PACK_DUV(U0, V0, U1, V1);

	do {
		c = src[TMAP_UV_INDEX(uv)];
		uv += duv;
		if (c != 255)
			*dest = c;
		dest++;
	} while (--num_left_over);
}

// --------------------------------------------------------------------------------------------------
// ASCENT: a lit span of a bitmap with effect colors (tmapfx.c). The span is
// drawn as usual, then the effect pixels get their colors from the screen
// before the span (saved here) and the bitmap colors of the same pixels
// (the unlit loop into a buffer with the alignment of dest, so that it
// takes the same texels).
static void per_span_fx(ubyte *dest, int x, int loop_count, fix l, fix dl)
{
	static ubyte *buf;
	static int buf_size;
	int n = loop_count + 1, n_saved, lighting = Lighting_on;
	ubyte *raw, *saved;

	if (n * 2 + 16 > buf_size) {
		ubyte *nb = (ubyte *) realloc(buf, (size_t) n * 2 + 16);
		if (!nb)
			return;
		buf = nb;
		buf_size = n * 2 + 16;
	}
	saved = buf;
	raw = buf + n + 1;
	raw += ((uintptr_t) dest - (uintptr_t) raw) & 3;
	n_saved = (x + n <= window_right) ? n + 1 : n;
	memcpy(saved, dest, (size_t) n_saved);
	memset(raw, 255, (size_t) n);

	Lighting_on = 0;
	if (!per2_flag)
		per_loop_divide(raw, loop_count, fx_u, fx_v, fx_z, l, dl);
	else
		per_loop_fast_nolight(raw, loop_count, fx_u, fx_v, fx_z, l, dl);
	Lighting_on = lighting;

	if (!per2_flag)
		per_loop_divide(dest, loop_count, fx_u, fx_v, fx_z, l, dl);
	else
		per_loop_fast(dest, loop_count, fx_u, fx_v, fx_z, l, dl);

	tmap_fx_fix_span(dest, raw, n, saved, n_saved, fx_y);
}

// --------------------------------------------------------------------------------------------------
// Enter:
//	fx_xleft	left x coordinate (int)
//	fx_xright	right x coordinate (int)
//	fx_y	y coordinate (int)
//	pixptr	address of source pixel map
//	fx_u	fixed point initial u coordinate
//	fx_v	fixed point initial v coordinate
//	fx_z	fixed point initial z coordinate
//	fx_du_dx	fixed point du/dx
//	fx_dv_dx	fixed point dv/dx
//	fx_dz_dx	fixed point dz/dx
//	fx_l, fx_dl_dx	lighting value and delta, if Lighting_on
void asm_tmap_scanline_per(void)
{
	ubyte	*dest;
	int	x, xright, loop_count;
	fix	l, dl;

	// ASCENT: the assembly had this test commented out.
	if ((uint) fx_y > (uint) window_bottom)
		return;

	x = fx_xleft;
	if (x < 0)
		x = 0;

	// ASCENT: the assembly had this test commented out.
	xright = fx_xright;
	if (xright > window_right)
		xright = window_right;

	loop_count = xright - x;
	if (loop_count < 0)
		return;

	// ASCENT: a divide by zero exception in the assembly.
	if (fx_z == 0)
		return;

	dest = write_buffer + fx_y * bytes_per_row + x;

	// lighting values are passed in fixed point, but need to be in 8 bit integer, 8 bit fraction
	l = fx_l >> 8;
	dl = fx_dl_dx >> 8;
	if (dl < 0)
		dl++;		// round towards 0 for negative deltas

	if (Lighting_on)
		tmap_guard_light(&l, &dl, loop_count);

	// ASCENT: the effect colors of a merged texture (tmapfx.c)
	if (Tmap_fx && Lighting_on) {
		per_span_fx(dest, x, loop_count, l, dl);
		return;
	}

	if (!per2_flag)
		per_loop_divide(dest, loop_count, fx_u, fx_v, fx_z, l, dl);
	else if (!Lighting_on)
		per_loop_fast_nolight(dest, loop_count, fx_u, fx_v, fx_z, l, dl);
	else
		per_loop_fast(dest, loop_count, fx_u, fx_v, fx_z, l, dl);
}
