/*
 * ASCENT: new file. The effect colors of the texture mappers (tmapfx.c,
 * docs/re/effectcolors.md).
 */
#ifndef _TMAPFX_H
#define _TMAPFX_H

#include "types.h"

extern int Tmap_fx;			// the bitmap of draw_tmap has BM_FLAG_DTU_EFFECTS
extern int Tmap_fx_row;		// the pulsing fade table row of color 0xCF (0x14C4E1, 25 to 30)

typedef struct tmap_fx_span {
	ubyte *behind;			// the last pixel of the span that an effect pixel read
	ubyte *below;			// the last pixel of the next row that an effect pixel read
	ubyte *below_start, *below_end;
} tmap_fx_span;

// 1 for the eight effect colors.
int tmap_fx_color_code(int c);
// The color of effect color c from the screen pixels behind and below.
// linear: the rules of the linear mapper.
int tmap_fx_color(int c, int behind, int below, int linear);
void tmap_fx_span_begin(tmap_fx_span *s, ubyte *dest, int y);
int tmap_fx_next_below(tmap_fx_span *s);
void tmap_fx_fix_span(ubyte *dest, const ubyte *raw, int n, const ubyte *saved, int n_saved, int y);

#endif
