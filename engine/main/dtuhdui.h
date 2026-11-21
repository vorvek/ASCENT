/*
 * ASCENT: new file. The upscaled interface pictures (dtuhdui.c). It has
 * no part in DTUGAME.EXE.
 */
#ifndef _DTUHDUI_H
#define _DTUHDUI_H

#include "gr.h"

// Picture n of FEBGND.DAT was loaded into bm (dtudisp.c). If the upscaled
// file <hd>/ui/febgnd_<n>.png exists, the pixels of bm (as loaded, before
// a change by the game) become the reference of the picture.
void dtu_hdui_loaded(grs_bitmap *bm, int n);

// The part (sx, sy, sw, sh) of src was copied to the rectangle (dx, dy, dw,
// dh) of dest, a pixel x of the rectangle taking the pixel sx + (x - dx) *
// sw / dw (and the same for y). Nothing happens if src is no loaded FEBGND
// picture with an upscaled file, or if dest is not in the screen.
void dtu_hdui_blit(grs_bitmap *src, int sx, int sy, int sw, int sh, grs_bitmap *dest, int dx, int dy, int dw, int dh);

// The shadow of the menu box (nm_draw_background of newmenu.c): the same
// darkened rectangles in the current canvas, with the darkening level.
void dtu_hdui_menu_shadow(int x1, int y1, int x2, int y2, int level);

// The upscaled pictures that the screen shows now, for a screen that is
// saved and drawn again later (the key screen of kconfig.c draws the
// whole screen over them). dtu_hdui_save returns NULL if the upscaled
// pictures are not used. dtu_hdui_restore(state) shows them again after
// the pixels of the screen were copied back, and frees state (NULL does
// nothing).
typedef struct dtu_hdui_state dtu_hdui_state;
dtu_hdui_state *dtu_hdui_save(void);
void dtu_hdui_restore(dtu_hdui_state *state);

#endif
