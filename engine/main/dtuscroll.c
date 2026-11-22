/*
 * ASCENT: new file. The text screen of the Scroll and the thief scroll
 * (dispscrl.c of DTUGAME.EXE 1.3). Descent 1 has nothing like it.
 *
 * The facts are in docs/re/ingame-ui.md, section "The scroll text screen".
 * Addresses are linear addresses of the image of DTUGAME.EXE 1.3 with
 * object 1 at 0x10000.
 *
 * The executable draws the scroll into its own bitmap and copies it to
 * the screen; the game frame skips the render while the screen is open
 * (0x435F8), so the play screen behind it stays as it was. Port decisions:
 * - The port renders the play screen in each frame and draws the scroll
 *   over it (dtu_scroll_draw), so that the GPU renderer and the upscaled
 *   interface work. The world goes on behind the scroll in both (the game
 *   time is stopped, but the frame time of 0x40E91 is not), so the view
 *   behind the scroll can move in the port.
 * - The cursor is not drawn while the screen is open (the executable
 *   shows the cursor of the last frame, which does not move).
 * - New in the port: Print Screen also writes a screenshot, as ` does.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "key.h"
#include "timer.h"
#include "gamefont.h"
#include "game.h"
#include "dtudisp.h"		// Dtu_hires, dtu_febgnd_load
#include "dtufe_int.h"		// fe_fetxt, Fe_khelben_mission
#include "dtuhdui.h"		// the upscaled picture
#include "dtuchar.h"		// dtu_char_log
#include "dtuscroll.h"

// The tables of dispscrl.c, [low, high] (0x13A94B)
static const int Scroll_art[2] = { 152, 151 };		// 0x144DDD: FEBGND entry of the scroll
static const int Box_x[2] = { 43, 85 };				// 0x144D8D: the text box in the picture
static const int Box_y[2] = { 39, 80 };				// 0x144D95
static const int Box_w[2] = { 151, 302 };			// 0x144D9D
static const int Box_h[2] = { 133, 264 };			// 0x144DA5
// 0x1452B9: the pictures of ~p0 to ~p3 (FEBGND entries)
static const int Pic_art[4][2] = { { 133, 104 }, { 131, 89 }, { 132, 90 }, { 191, 192 } };

// 0x1451E5: the FETXT entry of the mission summary of each mission 0 to 39
#define N_MISSIONS	40
// 0x144FE5 (low) and 0x144DE5 (high): the FETXT entry of the texts 0 to
// 127: 269 and 270 for 0 and 1, then 141 + n - 2 (low) or 13 + n - 2
// (high) for n = 2 to 127.
static int text_entry(int n, int hi)
{
	if (n == 0)
		return 269;
	if (n == 1)
		return 270;
	return (hi ? 13 : 141) + n - 2;
}

// 0x9CB62: the 13 colors of ~c0 to ~c12 (gr_find_closest_color, 0 to 63)
static const ubyte Color_rgb[13][3] = {
	{ 0, 0, 0 }, { 63, 63, 63 }, { 63, 0, 0 }, { 0, 63, 0 }, { 0, 0, 63 },
	{ 32, 32, 32 }, { 63, 32, 0 }, { 63, 63, 0 }, { 63, 32, 16 }, { 63, 0, 63 },
	{ 63, 50, 50 }, { 15, 31, 15 }, { 47, 0, 16 },
};
static int Colors[13];

#define N_STEPS		16
static int Open = 0;					// 0 closed, 1 opening (0x9D08D), 2 open (0x9D165)
static int Step_x[N_STEPS];				// 0x3BC9D0: the hidden width of each step
static int N_step;						// 0x3BCAD0
static int Step;						// 0x3BCADC: the step that is drawn next
static int Shown = -1;					// ASCENT: the step on the screen, -1 none yet
static fix Next_time;					// 0x3BCAE0
static int Time_stopped;
static int Hi;							// the interface of the open screen
static grs_bitmap *Clean[2];			// ASCENT: the picture as loaded, for the upscaled picture
static grs_bitmap *Pic;					// 0x144D7D: the picture with the text
static grs_bitmap *Small_pic[4][2];		// ASCENT: the pictures of ~p, loaded once
static grs_bitmap *Icons[2];			// the icons of ~i (InGameIcons, FEBGND 23 and 24)

// ASCENT: the pictures are freed at the exit.
static void free_all(void)
{
	grs_bitmap **all[] = { &Clean[0], &Clean[1], &Pic, &Icons[0], &Icons[1] };
	int i, j;

	for (i = 0; i < (int) (sizeof(all) / sizeof(all[0])); i++)
		if (*all[i]) {
			gr_free_bitmap(*all[i]);
			*all[i] = NULL;
		}
	for (i = 0; i < 4; i++)
		for (j = 0; j < 2; j++)
			if (Small_pic[i][j]) {
				gr_free_bitmap(Small_pic[i][j]);
				Small_pic[i][j] = NULL;
			}
}

static grs_bitmap *load_once(grs_bitmap **slot, int entry)
{
	static int registered = 0;

	if (!registered) {
		atexit(free_all);
		registered = 1;
	}
	if (!*slot)
		*slot = dtu_febgnd_load(entry, NULL);
	return *slot;
}

//------------------------------------------------------------------------
// The text (0x9D5F1, 0x9D8CF)

// 0x9D8CF: a word at (x, y) with a shadow of color 0 one pixel right and
// down, unless the color is color 0
static void draw_word(char *w, int x, int y)
{
	int fg = grd_curcanv->cv_font_fg_color;

	if (fg != Colors[0]) {
		grd_curcanv->cv_font_fg_color = Colors[0];
		gr_string(x + 1, y + 1, w);
		grd_curcanv->cv_font_fg_color = fg;
	}
	gr_string(x, y, w);
}

// A copy of a bitmap into the current canvas, clipped to it (the copies
// of the executable are not clipped; the pictures fit).
static void copy_in(grs_bitmap *src, int sx, int sy, int w, int h, int dx, int dy, int masked)
{
	grs_bitmap *d = &grd_curcanv->cv_bitmap;

	if (dx < 0) { sx -= dx; w += dx; dx = 0; }
	if (dy < 0) { sy -= dy; h += dy; dy = 0; }
	if (dx + w > d->bm_w) w = d->bm_w - dx;
	if (dy + h > d->bm_h) h = d->bm_h - dy;
	if (w <= 0 || h <= 0)
		return;
	if (masked)
		gr_bm_ubitbltm(w, h, dx, dy, sx, sy, src, d);
	else
		gr_bm_ubitblt(w, h, dx, dy, sx, sy, src, d);
}

// 0x9D5F1: lays out the words of the text in the box w x h of the current
// canvas. Words are separated by space, tab, CR and LF (0x144D78); a line
// break of the text is a space. Markup words: ~n (or ~N) a new line, ~cN a
// color, ~p?N picture N, ~i?N icon N.
static void layout(char *text, int w, int h)
{
	int lh = grd_curcanv->cv_font->ft_h, space_w, sh, aw, x = 0, y = 0;
	char *word;

	gr_get_string_size(" ", &space_w, &sh, &aw);
	for (word = strtok(text, " \n\r\t"); word; word = strtok(NULL, " \n\r\t")) {
		if (word[0] == '~') {
			char c = word[1];

			if (c == 'n' || c == 'N') {
				x = 0;
				y += lh + 1;
			} else if (c == 'c' || c == 'C') {
				int n = atoi(word + 2);
				grd_curcanv->cv_font_fg_color = Colors[(n < 0 || n > 12) ? 1 : n];
			} else if (c == 'p' || c == 'P') {
				int n = word[2] ? atoi(word + 3) : -1;
				grs_bitmap *bm;

				if (n >= 0 && n <= 3 && (bm = load_once(&Small_pic[n][Hi], Pic_art[n][Hi])) != NULL) {
					if (x)
						y += lh;
					y += 4;
					copy_in(bm, 0, 0, bm->bm_w, bm->bm_h, (w - bm->bm_w) / 2, y, 0);
					y += bm->bm_h + 4;
					x = 0;
				}
			} else if (c == 'i' || c == 'I') {
				int n = word[2] ? atoi(word + 3) : -1, size = Hi ? 32 : 16;	// 0x13B54C
				grs_bitmap *bm = load_once(&Icons[Hi], Hi ? 24 : 23);

				if (n >= 0 && n <= 256) {
					if (x)
						y += lh;
					y += 4;
					if (bm)
						copy_in(bm, (n & 7) * size, (n / 8) * size, size, size, (w - size) / 2, y, 1);
					y += size + 4;
					x = 0;
				}
			}
		} else {
			int ww, wh;

			gr_get_string_size(word, &ww, &wh, &aw);
			if (ww >= w)
				continue;					// 0x9D866: a word wider than the box is left out
			if (x + ww >= w) {
				x = 0;
				y += lh + 1;
				if (y >= h)
					return;
			}
			draw_word(word, x, y);
			x += ww + space_w;
			if (x >= w) {
				x = 0;
				y += lh + 1;
			}
		}
		if (y >= h)
			return;
	}
}

// 0x9CEA3: writes text n into the text box of the picture
static void write_text(int n)
{
	grs_canvas *save = grd_curcanv, canv, box;
	char *src, *text;
	int entry;

	if (n == 0) {
		int m = Fe_khelben_mission;					// 0x8445A
		if (m < 0 || m >= N_MISSIONS)
			return;
		entry = 271 + m;							// 0x1451E5[m]
	} else {
		if (n < 0 || n > 127)
			return;
		entry = text_entry(n, Hi);
	}
	src = fe_fetxt(entry);							// 0x97461
	if (!src || !src[0] || !GAME_FONT)
		return;
	text = strdup(src);
	if (!text)
		return;
	gr_init_canvas(&canv, Pic->bm_data, BM_LINEAR, Pic->bm_w, Pic->bm_h);
	gr_init_sub_canvas(&box, &canv, Box_x[Hi], Box_y[Hi], Box_w[Hi], Box_h[Hi]);
	gr_set_current_canvas(&box);
	gr_set_curfont(GAME_FONT);						// [0x13B844] + 0x10
	gr_set_fontcolor(Colors[1], -1);
	box.cv_drawmode = 0;
	layout(text, Box_w[Hi], Box_h[Hi]);
	free(text);
	gr_set_current_canvas(save);
	dtu_char_log("scroll: text %d is FETXT entry %d", n, entry);
}

//------------------------------------------------------------------------
// Open and close

static void free_pic(void)
{
	if (Pic)
		gr_free_bitmap(Pic);
	Pic = NULL;
}

int dtu_scroll_show(int n)
{
	grs_bitmap *clean;
	int i, x;

	if (Open)
		return 1;
	Hi = Dtu_hires != 0;
	// 0x9CB62
	for (i = 0; i < 13; i++)
		Colors[i] = gr_find_closest_color(Color_rgb[i][0], Color_rgb[i][1], Color_rgb[i][2]);
	// 0x9D43B: the picture
	clean = load_once(&Clean[Hi], Scroll_art[Hi]);
	if (!clean || !grd_curscreen)
		return 1;
	Pic = gr_create_bitmap(clean->bm_w, clean->bm_h);
	if (!Pic)
		return 1;
	memcpy(Pic->bm_data, clean->bm_data, clean->bm_w * clean->bm_h);
	write_text(n);
	// 0x9CDED: the hidden width of each step, from the box width down by
	// a sixteenth; the last one is 0
	x = Box_w[Hi] << 16;
	Step_x[0] = Box_w[Hi];
	for (i = 1; i < N_STEPS; i++) {
		x -= Box_w[Hi] << 12;
		Step_x[i] = x >> 16;
		if (Step_x[i] < 0)
			break;
	}
	if (i == N_STEPS)
		i--;
	Step_x[i] = 0;
	N_step = i + 1;
	// 0x9CE68: the game time stops, except in a network game
	Time_stopped = !((Game_mode & GM_MULTI) && Function_mode == FMODE_GAME);
	if (Time_stopped)
		stop_time();
	Open = 1;									// 0x1452DD = 0x9D08D
	Step = 0;
	Shown = -1;
	Next_time = 0;
	dtu_char_log("scroll: open text %d, %d steps", n, N_step);
	return 0;
}

static void close_scroll(void)
{
	Open = 0;
	Step = 0;
	Next_time = 0;
	Shown = -1;
	if (Time_stopped)
		start_time();							// 0x40E2A
	Time_stopped = 0;
	free_pic();									// 0x9D5B6
	dtu_char_log("scroll: closed");
}

void dtu_scroll_reset(void)
{
	if (Open)
		close_scroll();
}

int dtu_scroll_is_open(void)
{
	return Open != 0;
}

// 0x9D08D: the opening. A key shows the last step at once.
static void opening(int key)
{
	if (key) {
		Next_time = 0;
		if (Step < N_step)
			Step = N_step - 1;
	}
	if (timer_get_fixed_seconds() >= Next_time) {
		Shown = Step;							// 0x9D228: the step to the screen
		Step++;
		if (Step == N_step)
			Open = 2;							// 0x1452DD = 0x9D165
		else {
			Next_time = timer_get_fixed_seconds() + 0x800;
			if (Next_time < 0)
				Next_time = 0x800;
		}
	}
}

void dtu_scroll_frame(void)
{
	int key, k;

	if (!Open)
		return;
	// 0x43617: one key; ` writes a screenshot (0x80DA1) and is no key.
	// The rest of the keys are read and lost. A mouse button only does
	// that (0x545F5, 0xE8968).
	key = key_inkey();
	if (key == KEY_LAPOSTRO || key == KEY_PRINT_SCREEN) {
		save_screen_shot(0);
		key = 0;
	}
	while ((k = key_inkey()) != 0)
		;
	// 0x9CCAE
	if (Open == 1)
		opening(key);
	else if (key)
		close_scroll();							// 0x9D165
}

//------------------------------------------------------------------------
// Drawing

// The step on the screen (0x9D279, 0x9D228): the background, then the
// columns from the hidden width on, drawn from the left edge, then the
// left roller (the first Box_x / 2 columns) over the background. Color 255
// is not drawn (0xE1280).
void dtu_scroll_draw(void)
{
	grs_canvas *save = grd_curcanv;
	grs_bitmap *scr;
	int x0, y0, roll, t, w, h;

	if (!Open || Shown < 0 || !Pic || !grd_curscreen)
		return;
	gr_set_current_canvas(&VR_screen_pages[0]);
	scr = &grd_curcanv->cv_bitmap;
	x0 = (scr->bm_w - Pic->bm_w) / 2;			// 0x9D4C2
	y0 = (scr->bm_h - Pic->bm_h) / 2;
	h = Pic->bm_h;
	roll = Box_x[Hi] / 2;
	t = Step_x[Shown];
	w = Pic->bm_w - t - roll;
	if (w > 0) {
		copy_in(Pic, t + roll, 0, w, h, x0 + roll, y0, 1);
		if (Clean[Hi])
			dtu_hdui_blit_key(Clean[Hi], t + roll, 0, w, h, scr, x0 + roll, y0, w, h, 255);
	}
	copy_in(Pic, 0, 0, roll, h, x0, y0, 1);
	if (Clean[Hi])
		dtu_hdui_blit_key(Clean[Hi], 0, 0, roll, h, scr, x0, y0, roll, h, 255);
	gr_set_current_canvas(save);
}
