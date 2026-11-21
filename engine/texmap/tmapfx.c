/*
 * ASCENT: new file. The effect colors of the texture mappers of
 * DTUGAME.EXE 1.3 (docs/re/effectcolors.md). Descent 1 has no such colors.
 *
 * The pixel shading functions of the original (0x105A81 for the
 * perspective mapper, 0xEC4D7 for the linear one) give eight colors of a
 * merged wall texture (a wall with an overlay, flag 0x20 of the bitmap) a
 * color from the screen in place of the bitmap color: the glass of doors
 * and windows, the edges of waterfalls, the effect walls. The colors are
 * not lit. "behind" is a pixel of the span on the screen, "below" a pixel
 * of the next screen row:
 *
 *   0xCF  fade[r][fade[r][behind]], r = Tmap_fx_row - 1 (24 to 29, it pulses)
 *   0xBF  0x50 | (behind & 15)    green ramp
 *   0xBE  0xA0 | (behind & 15)    warm ramp (linear mapper: 0xC0, red ramp)
 *   0x6F  0x20 | (behind & 15)    blue ramp
 *   0x6E  0x50 | (below & 15)
 *   0x6D  0xA0 | (below & 15)     (linear mapper: 0xC0)
 *   0x4F  0x20 | (below & 15)
 *   0x4E  below
 *
 * The low 4 bits of a color are its brightness step in its ramp of 16
 * colors, so the effect shows the brightness of the screen in the color of
 * the ramp. The two screen pointers of the original advance only at an
 * effect pixel: the k-th effect pixel of a span reads the k-th pixel of
 * the span after its start ("behind"), and the pixel 2k-1 of the next
 * screen row from its left edge ("below"), which wraps at the width of the
 * view.
 */

#include <stdlib.h>
#include <string.h>

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"
#include "tmapfx.h"

int Tmap_fx;					// the bitmap of draw_tmap has BM_FLAG_DTU_EFFECTS
int Tmap_fx_row = 25;		// 0x14C4E1

int tmap_fx_color_code(int c)
{
	switch (c) {
		case 0xCF: case 0xBF: case 0xBE: case 0x6F: case 0x6E: case 0x6D: case 0x4F: case 0x4E:
			return 1;
	}
	return 0;
}

// The color of the effect color c (0x105A81 and 0xEC4D7 from the test of
// 0xCF to the end of the effect colors).
int tmap_fx_color(int c, int behind, int below, int linear)
{
	int r = (Tmap_fx_row - 1) & 0xFF, s;

	if (c == 0xCF) {
		// 0xCF looks the color up two times: the second lookup is the
		// general path of the function, so the result of the first is
		// tested against the other effect colors before it.
		c = gr_fade_table[(r & 0x1F) * 256 + behind];
		if (!tmap_fx_color_code(c) || c == 0xCF)
			return gr_fade_table[(r & (linear ? 0x0F : 0x1F)) * 256 + c];
	}
	switch (c) {
		case 0xBF:	return 0x50 | (behind & 15);
		case 0x6F:	return 0x20 | (behind & 15);
		case 0x6E:	return 0x50 | (below & 15);
		case 0x4F:	return 0x20 | (below & 15);
		case 0x4E:	return below;
		case 0xBE:
		case 0x6D:
			s = (c == 0xBE ? behind : below) & 15;
			if (!linear)
				return 0xA0 | s;
			if (s == 15)
				s = 14;
			return 0xC0 | s;
	}
	return c;
}

// The next screen row of the span at row y (the last row reads its own
// row), from the left edge of the view.
static ubyte *next_row(int y)
{
	if (y < window_bottom)
		y++;
	return write_buffer + y * bytes_per_row;
}

void tmap_fx_span_begin(tmap_fx_span *s, ubyte *dest, int y)
{
	s->behind = dest;
	s->below_start = s->below = next_row(y);
	s->below_end = s->below_start + window_width;
}

int tmap_fx_next_below(tmap_fx_span *s)
{
	int v;

	s->below++;
	v = *s->below;
	s->below++;
	if (s->below >= s->below_end)
		s->below = s->below_start;
	return v;
}

// The perspective mapper: dest[0..n-1] holds the lit span, raw[] the
// bitmap colors of the same pixels (255 for a transparent pixel that was
// not drawn), saved[0..n_saved-1] the screen before the span. The effect
// pixels are done again in their order, as the original does them while
// it draws.
void tmap_fx_fix_span(ubyte *dest, const ubyte *raw, int n, const ubyte *saved, int n_saved, int y)
{
	tmap_fx_span s;
	int i, k = 0;

	tmap_fx_span_begin(&s, dest, y);
	for (i = 0; i < n; i++) {
		int c = raw[i], behind, below;

		if (c == 255 || !tmap_fx_color_code(c))
			continue;
		k++;
		// the pixels before i have their final colors; i and the pixels
		// after it are still the screen before the span
		if (k < i)
			behind = dest[k];
		else
			behind = saved[k < n_saved ? k : n_saved - 1];
		below = tmap_fx_next_below(&s);
		dest[i] = (ubyte) tmap_fx_color(c, behind, below, 0);
	}
}
