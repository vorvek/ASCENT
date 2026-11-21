/*
 * ASCENT: C version of tmap_fl1.asm (asm_tmap_scanline_matt).
 * tmap_fl1.asm stays in the tree as the reference. It is not built.
 *
 * ntmap.c calls this renderer when Lighting_on == 2. Only the editor does
 * that, to find the texture map under the mouse (render.c, _search_mode).
 * It writes the color 1 for each pixel of the scanline whose texture pixel
 * is not the transparent color (255).
 *
 * Difference from the assembly: this version does a divide for each pixel
 * (the "tmap_loop" of the assembly, used when per2_flag == 0). With
 * per2_flag != 0 the assembly did a divide for each 8 pixels and stepped
 * linearly between them, thus the edge of a transparent area can differ by
 * a pixel. The assembly did no range check. Here a scanline outside the
 * canvas is skipped and the span is clipped to the canvas.
 */

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"

#define zonk	1

void asm_tmap_scanline_matt(void)
{
	const ubyte	*src = pixptr;
	ubyte	*dest;
	int	x, xright, loop_count;
	fix	u, v, z;
	int64_t	ui, vi;

	// ASCENT: range checks. The assembly had none.
	if ((uint) fx_y > (uint) window_bottom)
		return;
	if (fx_xleft < 0)
		return;
	xright = fx_xright;
	if (xright > window_right)
		xright = window_right;

	x = fx_xleft;
	loop_count = xright - x;
	if (loop_count < 0)
		return;

	u = fx_u;
	v = fx_v;
	z = fx_z;
	if (z == 0)
		return;

	dest = write_buffer + fx_y * bytes_per_row + x;

	do {
		vi = (int64_t) v / z;
		ui = (int64_t) u / z;
		if (src[((vi & 63) << 6) | (ui & 63)] != 255)
			*dest = zonk;
		dest++;

		v += fx_dv_dx;
		u += fx_du_dx;
		z += fx_dz_dx;
		if (z == 0)
			return;		// would be dividing by 0, so abort
	} while (--loop_count >= 0);
}
