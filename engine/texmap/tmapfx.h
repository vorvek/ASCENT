/*
 * ASCENT: new file. The effect colors of the texture mappers (tmapfx.c,
 * docs/re/effectcolors.md).
 */
#ifndef _TMAPFX_H
#define _TMAPFX_H

#include "types.h"

extern int Tmap_fx;			// the bitmap of draw_tmap has BM_FLAG_DTU_EFFECTS
extern int Tmap_fx_row;		// the pulsing fade table row of color 0xCF (0x14C4E1, 25 to 30)
extern int Tmap_dim;			// the dark view of the conversation window for this polygon (0 to 15, 0: off)
extern ubyte Tmap_dim_fade[];	// the fade table of the polygons with Tmap_dim

typedef struct tmap_fx_span {
	ubyte *behind;			// the last pixel of the span that an effect pixel read
	ubyte *below;			// the last pixel of the next row that an effect pixel read
	ubyte *below_start, *below_end;
} tmap_fx_span;

// 1 for the eight effect colors.
int tmap_fx_color_code(int c);
// 1 when the texel color c is an effect color that the mapper draws with
// tmap_fx_color: not one that the dark view of Tmap_dim takes first.
int tmap_fx_effect(int c);
// The value 0x14C4CE of the dark view of the conversation window for the
// next view (0 to 15). It builds Tmap_dim_fade from gr_fade_table.
void tmap_dim_set_view(int dim);
// Tmap_dim for a polygon of draw_tmap: the bitmap flags and Lighting_on.
void tmap_dim_polygon(int bm_flags, int lighting);
// The color of effect color c from the screen pixels behind and below.
// linear: the rules of the linear mapper.
int tmap_fx_color(int c, int behind, int below, int linear);
void tmap_fx_span_begin(tmap_fx_span *s, ubyte *dest, int y);
int tmap_fx_next_below(tmap_fx_span *s);
void tmap_fx_fix_span(ubyte *dest, const ubyte *raw, int n, const ubyte *saved, int n_saved, int y);

#endif
