/*
 * ASCENT: new file. The upscaled interface pictures, with no part in
 * DTUGAME.EXE.
 *
 * The interface (the play screen of dtudisp.c, the menu box of newmenu.c,
 * the front end of dtufe.c) draws pictures of FEBGND.DAT into the 8-bit
 * frame. tools/ui_hd.py makes upscaled copies of the stone, wood, metal
 * and scene pictures (<hd>/ui/febgnd_<n>.png). This file tells the GPU
 * presentation (plat_gpu_ui_* of gpu.c) where the frame has the pixels of
 * such a picture; the presentation then shows the upscaled picture at the
 * window resolution there, except where the frame pixel is different from
 * the picture pixel (text, numbers, icons, portraits, the cursor, the 3D
 * view). It needs the GPU renderer and the setting Upscaled textures.
 *
 * The pictures are known by the address of their pixels: febgnd_load of
 * dtudisp.c calls dtu_hdui_loaded, and a blit of the same pixels calls
 * dtu_hdui_blit. The reference of a picture is a copy of its pixels as
 * loaded, so a change of the game to the bitmap (the buttons that
 * dtudisp.c draws into the picture of the play screen) shows as 8-bit.
 */
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>

#include "gr.h"
#include "args.h"
#include "platform.h"
#include "dtuopts.h"
#include "dtuhdui.h"

extern ubyte gr_current_pal[256*3];		// palette.c: the palette of the last load

#define MAX_N		256		// FEBGND has 196 entries
#define MAX_LOADED	64

static signed char Has_file[MAX_N];		// 0 not known, 1 yes, -1 no
static struct { ubyte *data; int w, h, n; } Loaded[MAX_LOADED];
static int Next_loaded;
static int Was_on;

static int file_path(int n, char *path, int size)
{
	const char *dir = dtu_hd_dir();

	if (!dir)
		return 0;
	snprintf(path, size, "%s/ui/febgnd_%d.png", dir, n);
	return 1;
}

static int has_file(int n)
{
	char path[700];
	struct stat st;

	if (n < 0 || n >= MAX_N)
		return 0;
	if (!Has_file[n])
		Has_file[n] = (file_path(n, path, sizeof(path)) && stat(path, &st) == 0) ? 1 : -1;
	return Has_file[n] > 0;
}

static int enabled(void)
{
	static int no_arg = -1;

	if (no_arg < 0)
		no_arg = FindArg("-nohdui") != 0;
	if (!no_arg && DTU_GFX(Dtu_upscaled_textures) && plat_gpu_available()) {
		if (!Was_on)
			plat_gpu_ui_bind_palette(gr_current_pal);
		Was_on = 1;
		return 1;
	}
	if (Was_on)
		plat_gpu_ui_clear();
	Was_on = 0;
	return 0;
}

void dtu_hdui_loaded(grs_bitmap *bm, int n)
{
	char path[700];
	int i;

	if (!bm || !bm->bm_data || bm->bm_rowsize != bm->bm_w || !has_file(n) || !file_path(n, path, sizeof(path)))
		return;
	for (i = 0; i < MAX_LOADED; i++)
		if (Loaded[i].data == bm->bm_data)
			break;
	if (i == MAX_LOADED) {
		i = Next_loaded;
		Next_loaded = (Next_loaded + 1) % MAX_LOADED;
	}
	Loaded[i].data = bm->bm_data;
	Loaded[i].w = bm->bm_w;
	Loaded[i].h = bm->bm_h;
	Loaded[i].n = n;
	plat_gpu_ui_picture(n, bm->bm_w, bm->bm_h, bm->bm_data, path);
}

static int picture_of(grs_bitmap *bm)
{
	int i;

	if (!bm || !bm->bm_data)
		return -1;
	for (i = 0; i < MAX_LOADED; i++)
		if (Loaded[i].data == bm->bm_data && Loaded[i].w == bm->bm_w && Loaded[i].h == bm->bm_h)
			return Loaded[i].n;
	return -1;
}

// The position of the pixels of dest in the screen.
static int screen_pos(grs_bitmap *dest, int *x, int *y)
{
	grs_bitmap *scr;
	ptrdiff_t off;

	if (!grd_curscreen || !dest || !dest->bm_data)
		return 0;
	scr = &grd_curscreen->sc_canvas.cv_bitmap;
	if (!scr->bm_data || dest->bm_rowsize != scr->bm_rowsize || scr->bm_rowsize <= 0)
		return 0;
	off = dest->bm_data - scr->bm_data;
	if (off < 0 || off >= (ptrdiff_t) scr->bm_rowsize * scr->bm_h)
		return 0;
	*x = (int) (off % scr->bm_rowsize);
	*y = (int) (off / scr->bm_rowsize);
	return 1;
}

void dtu_hdui_blit(grs_bitmap *src, int sx, int sy, int sw, int sh, grs_bitmap *dest, int dx, int dy, int dw, int dh)
{
	dtu_hdui_blit_key(src, sx, sy, sw, sh, dest, dx, dy, dw, dh, -1);
}

void dtu_hdui_blit_key(grs_bitmap *src, int sx, int sy, int sw, int sh, grs_bitmap *dest, int dx, int dy, int dw, int dh, int key)
{
	int n, ox, oy;

	if (!enabled() || (n = picture_of(src)) < 0 || !screen_pos(dest, &ox, &oy))
		return;
	// the clip of an unscaled copy to dest (gr_bm_bitblt)
	if (sw == dw && sh == dh) {
		if (dx < 0) { sx -= dx; sw += dx; dx = 0; }
		if (dy < 0) { sy -= dy; sh += dy; dy = 0; }
		if (dx + sw > dest->bm_w) sw = dest->bm_w - dx;
		if (dy + sh > dest->bm_h) sh = dest->bm_h - dy;
		dw = sw;
		dh = sh;
	}
	if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0)
		return;
	plat_gpu_ui_draw_key(n, sx, sy, sw, sh, dx + ox, dy + oy, dw, dh, key);
}

void dtu_hdui_menu_shadow(int x1, int y1, int x2, int y2, int level)
{
	const ubyte *table = gr_fade_table + ((level & 0xff) << 8);
	int ox, oy, k;

	if (!Was_on || !grd_curcanv || !screen_pos(&grd_curcanv->cv_bitmap, &ox, &oy))
		return;
	// the order of the gr_urect calls of nm_draw_background
	for (k = 5; k >= 0; k--)
		plat_gpu_ui_darken(x2 - k + ox, y1 + k + oy, x2 - k + 1 + ox, y2 - 5 + 1 + oy, table);
	for (k = 5; k >= 0; k--)
		plat_gpu_ui_darken(x1 + k + ox, y2 - k + oy, x2 + 1 + ox, y2 - k + 1 + oy, table);
}
