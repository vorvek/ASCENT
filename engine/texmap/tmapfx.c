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
 *
 * The dark view of the conversation window (docs/re/scripts.md, section
 * 4): while the window opens, 0x458BA(1) ramps the byte 0x14C4CE (D) from
 * 1 to 15, and back to 0 when it closes. The same functions test it before
 * the effect colors: a lit texel c (not 255) with (c & 15) <= D becomes
 * (fade[max(r - 5, 0)][c] & 15) | 8, a color of the dark half of palette
 * row 0 (lavender grey). The port puts the rule into a copy of the fade
 * table (Tmap_dim_fade) that the mappers use for the lit polygons.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "fix.h"
#include "gr.h"
#include "texmap.h"
#include "texmapl.h"
#include "tmapfx.h"
#include "rle.h"

int Tmap_fx;					// the bitmap of draw_tmap has BM_FLAG_DTU_EFFECTS
int Tmap_fx_row = 25;		// 0x14C4E1
int Tmap_dim;					// 0x14C4CE for the polygon of draw_tmap
ubyte Tmap_dim_fade[256*GR_FADE_LEVELS];
static int Tmap_dim_view;	// 0x14C4CE for the view
static int Tint_stale = 1;	// the tables of the tint must be made again (tmap_tint_polygon)

int tmap_fx_color_code(int c)
{
	switch (c) {
		case 0xCF: case 0xBF: case 0xBE: case 0x6F: case 0x6E: case 0x6D: case 0x4F: case 0x4E:
			return 1;
	}
	return 0;
}

// 0xEC4EF to 0xEC504: the dark view is tested before the effect colors
int tmap_fx_effect(int c)
{
	return tmap_fx_color_code(c) && !(Tmap_dim && (c & 15) <= Tmap_dim);
}

// 0xEC4EF to 0xEC523 (0x105A99 in the copy) with 0x14C4CF at 0, the value of
// the conversation window
void tmap_dim_set_view(int dim)
{
	int r, c;

	if (dim < 0) dim = 0;
	if (dim > 15) dim = 15;
	Tmap_dim_view = dim;
	Tmap_dim = 0;
	Tint_stale = 1;		// the fade table of the view can change (infravision)
	if (!dim)
		return;
	for (r=0; r<GR_FADE_LEVELS; r++) {
		int r5 = r - 5 < 0 ? 0 : r - 5;

		for (c=0; c<256; c++) {
			if (c != 255 && (c & 15) <= dim)
				Tmap_dim_fade[r*256 + c] = (ubyte) ((gr_fade_table[r5*256 + c] & 15) | 8);
			else
				Tmap_dim_fade[r*256 + c] = gr_fade_table[r*256 + c];
		}
	}
}

// The rule is in the functions of the lit mappers. The original game changes
// every lit texture of the view, the walls and the models (seen in the DOS
// game). The code read so far gates the functions with 0x14C4D8 (draw_tmap
// 0xEA503 to 0xEA525: bitmap flag 0x20, the tint 0x14C4CD or 0x14C4CF);
// what sets it for all polygons while the window is open was not found
// (docs/re/scripts.md, section 4).
void tmap_dim_polygon(int lighting)
{
	Tmap_dim = (Tmap_dim_view && lighting) ? Tmap_dim_view : 0;
}

// The tint of the current object (0x14C4CD, docs/re/spells.md "Detect
// Magic"). Value 0x40 with 0x14C4D0 = 15: the general path of 0x105A81
// (0x105BC8 to 0x105C35) gives a lit texel c (not 255, not an effect color,
// not a texel of the dark view) the color fade[row][(c & 15) | 0x20]: the
// blue ramp at the brightness step of the texel. The linear copy 0xEC4D7
// differs (Tmap_tint_fix 0): its setup 0xEC6BC clears 0x14C4E2 and sets it
// again only when 0x14C4CD differs from the value of the last scanline
// (0x14C4E7), so only the first scanline after a change is tinted; there
// it clears AH (0xEC62D) and tests c & 0x1F (0xEC654), so a texel with
// (c & 0x1F) <= 15 gets fade[0][(c & 15) | 0x20] (near black) and the
// others keep fade[row][c]; and with the tint (0x14C4D8 set) every other
// texel takes the row & 15 (0xEC5EA).
int Tmap_tint;
int Tmap_tint_fix = 1;			// Dtu_fix_magic_tint (dtuobj.c)
int Tmap_tint_on;				// draw_tmap: Tmap_tint is 0x40 for a lit polygon
static ubyte Tint_fade[256*GR_FADE_LEVELS];			// the perspective rule
static ubyte Tint_lin_first[256*GR_FADE_LEVELS];	// the linear rule, first scanline
static ubyte Tint_lin_rest[256*GR_FADE_LEVELS];		// the linear rule, the next scanlines
static int Tint_dim = -1;
static int Tint_lin_cache;		// 0x14C4E7

static void tint_build(void)
{
	const ubyte *base = Tmap_dim ? Tmap_dim_fade : gr_fade_table;
	int r, c;

	for (r=0; r<GR_FADE_LEVELS; r++)
		for (c=0; c<256; c++) {
			int i = r*256 + c, keep = (c == 255 || tmap_fx_color_code(c) || (Tmap_dim && (c & 15) <= Tmap_dim));
			int b = (c & 15) | 0x20;

			Tint_fade[i] = keep ? base[i] : gr_fade_table[r*256 + b];
			Tint_lin_rest[i] = keep ? base[i] : gr_fade_table[(r & 15)*256 + c];
			Tint_lin_first[i] = keep ? base[i] : ((c & 0x1F) <= 15 ? gr_fade_table[b] : gr_fade_table[(r & 15)*256 + c]);
		}
	Tint_stale = 0;
	Tint_dim = Tmap_dim;
}

void tmap_tint_polygon(int lighting)
{
	Tmap_tint_on = (lighting && Tmap_tint == 0x40);
	if (Tmap_tint && lighting && (Tint_stale || Tint_dim != Tmap_dim))
		tint_build();
}

const ubyte *tmap_lit_fade(int linear)
{
	const ubyte *base = Tmap_dim ? Tmap_dim_fade : gr_fade_table;

	if (linear && !Tmap_tint_fix) {
		int first = (Tmap_tint != Tint_lin_cache);

		Tint_lin_cache = Tmap_tint;
		if (!Tmap_tint)
			return base;
		return (first && Tmap_tint == 0x40) ? Tint_lin_first : Tint_lin_rest;
	}
	return Tmap_tint_on ? Tint_fade : base;
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

// ASCENT: the solid form of a bitmap with effect colors (Dtu_fix_solid_effects,
// main/texmerge.c). On a face that the view does not see past (a closed
// wall, a side with no segment behind it), nothing is drawn under the face
// in the frame, so the screen pixels that the rule reads hold the picture
// of an earlier frame (DTUGAME.EXE reads them all the same, see
// docs/re/effectcolors.md). The solid form gives each effect texel a fixed
// color from the texels around it: their mean color, filled ring after
// ring into the area of the effect texels as plat_gpu_fx_mark and
// fill_colors (platform/gputex.c) fill the RGB form; f is the palette
// color nearest to it (the table of update_light in platform/gpu.c). The
// texel gets the color of the rule with f as the screen pixel (behind and
// below): ramp | (f & 15) for the six ramp colors, f itself for 0x4E and
// 0xCF (no pulse). The face is then drawn as a texture without the rule,
// lit like its other texels. The GPU renderer does the same in its shader
// (u_fxsolid) with the filled colors of its texture.

#define SOLID_CACHE 16
typedef struct solid_entry {
	const grs_bitmap *src;
	uint32_t hash;
	int used;
	grs_bitmap bm;
	ubyte *data;
	int size;
} solid_entry;
static solid_entry Solid[SOLID_CACHE];
static int Solid_clock;

static int expand6(int v)
{
	v &= 63;
	return (v << 2) | (v >> 4);
}

// the nearest palette color (not 255) of an RGB color, as update_light of
// gpu.c; no_fx: not an effect color either
static int nearest_color(const int *rgb, int no_fx)
{
	int c, best = 0, bd = 1 << 30, r = rgb[0] >> 2, g = rgb[1] >> 2, b = rgb[2] >> 2;

	for (c = 0; c < 255; c++) {
		int dr, dg, db, d;
		if (no_fx && tmap_fx_color_code(c))
			continue;
		dr = r - gr_palette[c*3];
		dg = g - gr_palette[c*3+1];
		db = b - gr_palette[c*3+2];
		d = 3*dr*dr + 4*dg*dg + 2*db*db;
		if (d < bd) {
			bd = d;
			best = c;
			if (!d)
				break;
		}
	}
	return best;
}

static void solid_make(solid_entry *e, const ubyte *pix, int w, int h)
{
	int n = w * h, i, x, y, pass, left;
	int *rgb = (int *) malloc(sizeof(int) * 3 * n);
	ubyte *done = (ubyte *) malloc(n);
	long sum[3] = { 0, 0, 0 }, cnt = 0;

	memcpy(e->data, pix, n);
	if (!rgb || !done) {
		free(rgb);
		free(done);
		return;
	}
	// done: 1 a source (not an effect color, not 255), 0 a target or 255
	for (i = 0; i < n; i++) {
		int c = pix[i];
		rgb[i*3] = expand6(gr_palette[c*3]);
		rgb[i*3+1] = expand6(gr_palette[c*3+1]);
		rgb[i*3+2] = expand6(gr_palette[c*3+2]);
		done[i] = c != 255 && !tmap_fx_color_code(c);
		if (done[i]) {
			sum[0] += rgb[i*3];
			sum[1] += rgb[i*3+1];
			sum[2] += rgb[i*3+2];
			cnt++;
		}
	}
	for (pass = 0; pass < 16; pass++) {
		left = 0;
		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++) {
				int j = y * w + x, dx, dy, c = 0, r = 0, g = 0, b = 0;
				if (done[j] || !tmap_fx_color_code(pix[j]))
					continue;
				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++) {
						int xx = x + dx, yy = y + dy, k;
						if ((dx | dy) == 0 || xx < 0 || yy < 0 || xx >= w || yy >= h)
							continue;
						k = yy * w + xx;
						if (done[k] == 1) {
							r += rgb[k*3];
							g += rgb[k*3+1];
							b += rgb[k*3+2];
							c++;
						}
					}
				if (c) {
					rgb[j*3] = r / c;
					rgb[j*3+1] = g / c;
					rgb[j*3+2] = b / c;
					done[j] = 2;
				} else
					left++;
			}
		for (i = 0; i < n; i++)
			if (done[i] == 2)
				done[i] = 1;
		if (!left)
			break;
	}
	for (i = 0; i < n; i++) {
		int c = pix[i], f;
		if (!tmap_fx_color_code(c))
			continue;
		if (!done[i] && cnt) {
			rgb[i*3] = (int) (sum[0] / cnt);
			rgb[i*3+1] = (int) (sum[1] / cnt);
			rgb[i*3+2] = (int) (sum[2] / cnt);
		}
		switch (c) {
			case 0xBF: case 0x6E:	f = 0x50 | (nearest_color(&rgb[i*3], 0) & 15); break;
			case 0xBE: case 0x6D:	f = 0xA0 | (nearest_color(&rgb[i*3], 0) & 15); break;
			case 0x6F: case 0x4F:	f = 0x20 | (nearest_color(&rgb[i*3], 0) & 15); break;
			default:				f = nearest_color(&rgb[i*3], 1); break;	// 0x4E, 0xCF
		}
		e->data[i] = (ubyte) f;
	}
	free(rgb);
	free(done);
}

grs_bitmap *tmap_fx_solid_bitmap(grs_bitmap *bp)
{
	grs_bitmap *src = bp;
	solid_entry *e = NULL;
	uint32_t hash = 2166136261u;
	int i, n;

	if (!bp || !bp->bm_data || bp->bm_w <= 0 || bp->bm_h <= 0)
		return bp;
	if (bp->bm_flags & BM_FLAG_RLE)
		bp = rle_expand_texture(bp);
	if (!bp || !bp->bm_data || bp->bm_rowsize != bp->bm_w)
		return src;
	n = bp->bm_w * bp->bm_h;
	for (i = 0; i < n; i++)
		hash = (hash ^ bp->bm_data[i]) * 16777619u;
	Solid_clock++;
	for (i = 0; i < SOLID_CACHE; i++)
		if (Solid[i].data && Solid[i].src == src && Solid[i].hash == hash
				&& Solid[i].bm.bm_w == bp->bm_w && Solid[i].bm.bm_h == bp->bm_h) {
			Solid[i].used = Solid_clock;
			return &Solid[i].bm;
		}
	for (i = 0; i < SOLID_CACHE; i++)
		if (!e || Solid[i].used < e->used)
			e = &Solid[i];
	if (e->size < n) {
		ubyte *d = (ubyte *) realloc(e->data, n);
		if (!d)
			return src;
		e->data = d;
		e->size = n;
	}
	e->src = src;
	e->hash = hash;
	e->used = Solid_clock;
	e->bm = *bp;
	e->bm.bm_data = e->data;
	e->bm.bm_rowsize = (short) bp->bm_w;
	e->bm.bm_flags = bp->bm_flags & ~(BM_FLAG_RLE | BM_FLAG_DTU_EFFECTS);
	solid_make(e, bp->bm_data, bp->bm_w, bp->bm_h);
	return &e->bm;
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

		if (c == 255 || !tmap_fx_effect(c))
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
