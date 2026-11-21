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
#include <stdlib.h>
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

// The draws and darkenings that went to the GPU since the last clear, in
// their order (the GPU keeps at most 64), for dtu_hdui_save and
// dtu_hdui_restore.
#define MAX_LOG		64
typedef struct hdui_op {
	int n;						// the picture, -1 for a darkening
	int sx, sy, sw, sh, dx, dy, dw, dh;
	int key;					// the transparent index of the draw, or -1
	ubyte table[256];
} hdui_op;
static hdui_op Log[MAX_LOG];
static int Log_n;

static void log_add(const hdui_op *op)
{
	if (Log_n == MAX_LOG) {
		memmove(Log, Log + 1, (MAX_LOG - 1) * sizeof(hdui_op));
		Log_n--;
	}
	Log[Log_n++] = *op;
}

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
	Log_n = 0;
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
	{
		hdui_op op;
		op.n = n;
		op.sx = sx; op.sy = sy; op.sw = sw; op.sh = sh;
		op.dx = dx + ox; op.dy = dy + oy; op.dw = dw; op.dh = dh;
		op.key = key;
		log_add(&op);
	}
}

// A darkening of the screen rectangle (x0, y0) to (x1, y1), x1 and y1 not in it.
static void darken(int x0, int y0, int x1, int y1, const ubyte *table)
{
	hdui_op op;

	if (x1 <= x0 || y1 <= y0)
		return;
	plat_gpu_ui_darken(x0, y0, x1, y1, table);
	op.n = -1;
	op.sx = op.sy = op.sw = op.sh = 0;
	op.dx = x0; op.dy = y0; op.dw = x1 - x0; op.dh = y1 - y0;
	op.key = -1;
	memcpy(op.table, table, 256);
	log_add(&op);
}

void dtu_hdui_menu_shadow(int x1, int y1, int x2, int y2, int level)
{
	const ubyte *table = gr_fade_table + ((level & 0xff) << 8);
	int ox, oy, k;

	if (!Was_on || !grd_curcanv || !screen_pos(&grd_curcanv->cv_bitmap, &ox, &oy))
		return;
	// the order of the gr_urect calls of nm_draw_background
	for (k = 5; k >= 0; k--)
		darken(x2 - k + ox, y1 + k + oy, x2 - k + 1 + ox, y2 - 5 + 1 + oy, table);
	for (k = 5; k >= 0; k--)
		darken(x1 + k + ox, y2 - k + oy, x2 + 1 + ox, y2 - k + 1 + oy, table);
}

struct dtu_hdui_state {
	int n;
	hdui_op op[MAX_LOG];
};

dtu_hdui_state *dtu_hdui_save(void)
{
	dtu_hdui_state *st;

	if (!Was_on || !(st = (dtu_hdui_state *) malloc(sizeof(*st))))
		return NULL;
	st->n = Log_n;
	memcpy(st->op, Log, Log_n * sizeof(hdui_op));
	return st;
}

void dtu_hdui_restore(dtu_hdui_state *st)
{
	int i;

	if (!st)
		return;
	if (enabled()) {
		plat_gpu_ui_clear();
		Log_n = 0;
		for (i = 0; i < st->n; i++) {
			hdui_op *o = &st->op[i];
			if (o->n < 0)
				darken(o->dx, o->dy, o->dx + o->dw, o->dy + o->dh, o->table);
			else {
				plat_gpu_ui_draw_key(o->n, o->sx, o->sy, o->sw, o->sh, o->dx, o->dy, o->dw, o->dh, o->key);
				log_add(o);
			}
		}
	}
	free(st);
}
