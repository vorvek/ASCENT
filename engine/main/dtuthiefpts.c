/*
 * ASCENT: new file. The thief skill point screen (thiefpts.c of
 * DTUGAME.EXE 1.3, 0xB4419 to 0xB611B). Descent 1 has nothing like it.
 *
 * The facts are in docs/re/combat.md, section B7. Addresses are linear
 * addresses of the image of DTUGAME.EXE 1.3 with object 1 at 0x10000.
 *
 * Mirt gives the points (0xABB53, dtuscript.c); this file asks whether to
 * spend them (0xB5B49) and shows the screen (0xB5BB9), which has its own
 * loop, as the executable does. Port decisions:
 * - The pointer is the pointer of the menus (dtu_display_draw_pointer); its
 *   hot spot is its center, as the click point of the executable is the
 *   center of the cursor (0xB5DBC).
 * - New in the port: Print Screen also writes a screenshot, as ` does.
 * - A run with no one to answer (-soak, -frames) does not open the screen.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "key.h"
#include "mouse.h"
#include "timer.h"
#include "args.h"
#include "text.h"
#include "gamefont.h"
#include "game.h"
#include "digi.h"
#include "platform.h"
#include "dtufile.h"		// dtu_find_file
#include "dtudisp.h"		// Dtu_hires, dtu_febgnd_load, the pointer
#include "dtuhdui.h"		// the upscaled picture
#include "dtumusic.h"		// dtumusic_frame (0xA4F83)
#include "dtufe.h"			// dtufe_menu_script_frame
#include "dtuchar.h"
#include "dtulog.h"			// ASCENT: the session log
#include "dtuthiefpts.h"

#define N_SKILLS	5
#define SKILL_MAX	95			// 0x5F

// The layout tables, [low, high] (0x13A94B)
static const int Bg_art[2] = { 152, 151 };		// 0x146744: the scroll picture
#define BUTTON_ART	190							// 0x14674C (0xB5675)
static const int Margin_l[2] = { 47, 100 };		// 0x146750
static const int Margin_r[2] = { 44, 100 };		// 0x146758
static const int Margin_t[2] = { 43, 130 };		// 0x146760
static const int Margin_b[2] = { 33, 100 };		// 0x146768
// 0x146770 to 0x1467C8: the buttons in FEBGND 190: minus up, plus up,
// minus down, plus down; low at y 41 (26 x 26), high at y 1 (37 x 37)
static const int Button_x[4] = { 1, 41, 81, 121 };
static const int Button_y[2] = { 41, 1 };
static const int Button_size[2] = { 26, 37 };
// The values of the mode 0xB5971 (1 or 2, both the same): the space
// beside the points (0x1467E0), the inner margin of a row (0x1467EC), the
// space between the rows (0x1467F8), the space before the name (0x146804)
#define BUTTON_SPACE	16
#define ROW_MARGIN		3
#define ROW_SPACE		5
#define NAME_SPACE		5
// ASCENT: the most the content area may grow on each side for the long
// names of a language (the free parchment between the margin and the
// rollers), [low, high]
static const int Widen[2] = { 10, 22 };

// 0x146844: the names of the skills
static const char *Skill_name[N_SKILLS] = {
	LN("Hide In Shadows"), LN("Move Silently"), LN("Open Locks"), LN("Find/Remove Traps"), LN("Climb Walls")
};

// The state of the screen
static int Hi;
static int *Base[N_SKILLS];				// 0x146810: the skills without the items
static int *In_use[N_SKILLS];			// 0x146828: the skills in use
static int Work[N_SKILLS];				// 0x3D4E20: the skills on the screen
static int Limit;						// 0x146840: the most points one skill can take
static grs_bitmap *Bg_bm[2], *Button_bm;
static grs_font *Font;
static int Bg_x, Bg_y;					// 0x3D4F5C, 0x3D4F60
static int Cx, Cy, Cw, Ch;				// the content area (0x3D4CA0)
static int Msg_y, Msg_h;				// the message line (0x3D4CC0)
static int Minus_x, Minus_y, Plus_x, Plus_y, Bsize;		// 0x3D4E34, 0x3D4E48
static int Pts_x, Pts_y, Pts_w, Pts_h;	// 0x3D4BE0
static int Row_y[N_SKILLS], Row_h;		// 0x3D4C00
static int Val_w;						// 0x3D4D80: the width of "555%%"
// 0xB4C45: the colors (closest palette colors of 6-bit values)
static int C_white, C_red, C_blue, C_yellow, C_magenta;

static void set_skill_pointers(dtu_player_attr *a, int **p)
{
	// 0xB4DDE, 0xB4E74: the order of the screen
	p[0] = &a->hide_in_shadows;
	p[1] = &a->move_silently;
	p[2] = &a->open_locks;
	p[3] = &a->find_traps;
	p[4] = &a->climb_walls;
}

//------------------------------------------------------------------------
// The rules

// 0xB4419: 1 if skill i cannot take a point
static int skill_full(int i)
{
	return !(Work[i] < SKILL_MAX && Work[i] - *Base[i] < Limit);
}

// 0xB4499: 1 if no skill can take a point
static int all_full(void)
{
	int i;

	for (i = 0; i < N_SKILLS; i++)
		if (!skill_full(i))
			return 0;
	return 1;
}

// 0xB44B4: one point to skill i
static void add_point(int i)
{
	int n;

	if (Dtu_char.thief_points == 0 || skill_full(i))
		return;
	n = Limit - (Work[i] - *Base[i]);
	if (n > Dtu_char.thief_points)
		n = Dtu_char.thief_points;
	if (n > 0)
		n = 1;
	Work[i] += n;
	Dtu_char.thief_points -= n;
}

// 0xB457D: one point back from skill i, if it took points
static void remove_point(int i)
{
	int n = Work[i] - *Base[i];

	if (n == 0)
		return;
	if (n > 0)
		n = 1;
	Work[i] -= n;
	Dtu_char.thief_points += n;
}

//------------------------------------------------------------------------
// Drawing. The executable draws into sub-canvases of the screen; the port
// draws into the screen with the same rectangles.

static grs_bitmap *screen_bm(void)
{
	return &grd_curscreen->sc_canvas.cv_bitmap;
}

// A part of the scroll picture at its place on the screen (the copies of
// the background, 0xE0820)
static void restore_bg(int x, int y, int w, int h)
{
	grs_bitmap *bg = Bg_bm[Hi];
	int sx = x - Bg_x, sy = y - Bg_y;

	if (w <= 0 || h <= 0 || sx < 0 || sy < 0 || sx + w > bg->bm_w || sy + h > bg->bm_h)
		return;
	gr_bm_ubitblt(w, h, x, y, sx, sy, bg, screen_bm());
	dtu_hdui_blit(bg, sx, sy, w, h, screen_bm(), x, y, w, h);
}

// Text in a box of the screen: centered across (x 0x8000) and down
static void box_text(int x, int y, int w, int h, int color, const char *s)
{
	grs_canvas c, *save = grd_curcanv;
	int tw, th, aw;

	gr_init_sub_canvas(&c, &grd_curscreen->sc_canvas, x, y, w, h);
	gr_set_current_canvas(&c);
	gr_set_curfont(Font);
	gr_set_fontcolor(color, -1);
	gr_get_string_size((char *) s, &tw, &th, &aw);
	// ASCENT: a text wider than the box (a long name of a language in the
	// low resolution) starts at the left edge, cut at the right one
	gr_string(tw > w ? 0 : 0x8000, (h - th) / 2, (char *) s);
	gr_set_current_canvas(save);
}

static void hline(int x1, int x2, int y)
{
	gr_rect(x1, y, x2, y);
}

static void vline(int x, int y1, int y2)
{
	gr_rect(x, y1, x, y2);
}

// 0xB4AB5: the frame of the selected row, blue one pixel right and down,
// then magenta
static void draw_frame(int i)
{
	grs_canvas c, *save = grd_curcanv;
	int w = Cw, h = Row_h;

	gr_init_sub_canvas(&c, &grd_curscreen->sc_canvas, Cx, Cy + Row_y[i], Cw, Row_h);
	gr_set_current_canvas(&c);
	gr_setcolor(C_blue);
	hline(1, w - 1, 1);
	vline(1, 1, h - 1);
	hline(1, w - 1, h - 1);
	vline(w - 1, 1, h - 1);
	gr_setcolor(C_magenta);
	hline(0, w - 2, 0);
	vline(0, 0, h - 2);
	hline(0, w - 2, h - 2);
	vline(w - 2, 0, h - 2);
	gr_set_current_canvas(save);
}

// 0xB4843: a row: the background (0xB48B6), the name in white (0xB476D),
// the value (0xB4947): red if it took points, else yellow
static void draw_row(int i)
{
	char s[32];
	int y = Cy + Row_y[i];

	restore_bg(Cx, y, Cw, Row_h);
	box_text(Cx + Val_w + ROW_MARGIN + NAME_SPACE, y + ROW_MARGIN, Cw - 2 * ROW_MARGIN - Val_w - NAME_SPACE,
		Row_h - 2 * ROW_MARGIN, C_white, LT(Skill_name[i]));
	snprintf(s, sizeof(s), "%d%%", Work[i]);
	box_text(Cx + ROW_MARGIN, y + ROW_MARGIN, Val_w, Row_h - 2 * ROW_MARGIN, Work[i] != *Base[i] ? C_red : C_yellow, s);
}

// 0xB46FA: the frame moves from row old to row sel
static void select_row(int sel, int old)
{
	if (old >= 0)
		draw_row(old);
	if (sel >= 0)
		draw_frame(sel);
}

// 0xB463B: the points that are left, red
static void draw_points(void)
{
	char s[16];

	restore_bg(Pts_x, Pts_y, Pts_w, Pts_h);
	snprintf(s, sizeof(s), "%d", Dtu_char.thief_points);
	box_text(Pts_x, Pts_y, Pts_w, Pts_h, C_red, s);
}

// 0xB5A0E (plus) and 0xB5A52 (minus): a button up or down, with mask
static void draw_button(int plus, int down)
{
	int n = (plus ? 1 : 0) + (down ? 2 : 0);
	int x = plus ? Plus_x : Minus_x, y = plus ? Plus_y : Minus_y;

	gr_bm_ubitbltm(Bsize, Bsize, x, y, Button_x[n], Button_y[Hi], Button_bm, screen_bm());
}

// The message line (0xB5F6A): red, centered
static void draw_message(const char *s)
{
	box_text(Cx, Msg_y, Cw, Msg_h, C_red, s);
}

//------------------------------------------------------------------------
// Setup

static grs_font *screen_font(void)
{
	static grs_font *medium;
	static int tried;

	// 0xB598F, 0xB59E0: the medium font of the front end (0x13E380,
	// FNTEMFM_.FNT) in the high resolution, else the game font
	if (Hi && !tried) {
		char path[600];
		tried = 1;
		if (dtu_find_file("FNTEMFM_.FNT", path, sizeof(path)))
			medium = gr_init_font(path);
	}
	if (Hi && medium)
		return medium;
	return GAME_FONT;
}

// 0xB503D: the rectangles of the screen
static int layout(void)
{
	grs_bitmap *bg = Bg_bm[Hi];
	int w999, h999, aw, w5, h5, tw, rowh, bx, by, i, y;
	grs_canvas *save = grd_curcanv;

	Bg_x = (screen_bm()->bm_w - bg->bm_w) / 2;
	Bg_y = (screen_bm()->bm_h - bg->bm_h) / 2;
	Cx = Bg_x + Margin_l[Hi];
	Cy = Bg_y + Margin_t[Hi];
	Cw = bg->bm_w - Margin_l[Hi] - Margin_r[Hi];
	Ch = bg->bm_h - Margin_t[Hi] - Margin_b[Hi];
	Msg_h = Font->ft_h;
	Msg_y = Cy + Ch + (Hi ? 5 : 0);
	if (Cw <= 0 || Ch <= 0 || Msg_y + Msg_h > Bg_y + bg->bm_h)
		return 1;
	gr_set_current_canvas(NULL);
	gr_set_curfont(Font);
	// ASCENT: a language other than English has the long official names of
	// the skills: the content area gets wider, into the free parchment
	// beside it (at most Widen[Hi] pixels on each side), when a name is
	// wider than the name box of a row (draw_row)
	if (Dtu_language != DTU_LANG_EN) {
		int nw, need = 0;
		gr_get_string_size("555%%", &w5, &h5, &aw);
		for (i = 0; i < N_SKILLS; i++) {
			gr_get_string_size((char *) LT(Skill_name[i]), &nw, &h5, &aw);
			if (nw > need)
				need = nw;
		}
		need -= Cw - 2 * ROW_MARGIN - w5 - NAME_SPACE;
		if (need > 0) {
			need = (need + 1) / 2;
			if (need > Widen[Hi])
				need = Widen[Hi];
			Cx -= need;
			Cw += 2 * need;
		}
	}
	// the bottom row: minus, the points ("999" wide), plus
	gr_get_string_size("999", &w999, &h999, &aw);
	Bsize = Button_size[Hi];
	tw = Bsize + BUTTON_SPACE + w999 + BUTTON_SPACE + Bsize;
	rowh = Bsize > h999 ? Bsize : h999;
	bx = (Cw - tw) / 2;
	by = Ch - rowh;
	Minus_x = Cx + bx;
	Minus_y = Cy + by + (rowh - Bsize) / 2;
	Pts_x = Minus_x + Bsize + BUTTON_SPACE;
	Pts_y = Cy + by + (rowh - h999) / 2;
	Pts_w = w999;
	Pts_h = h999;
	Plus_x = Pts_x + w999 + BUTTON_SPACE;
	Plus_y = Minus_y;
	// the rows: the height of "555%%" (the string as it is) and twice the
	// margin
	gr_get_string_size("555%%", &w5, &h5, &aw);
	Val_w = w5;
	Row_h = h5 + 2 * ROW_MARGIN;
	for (i = 0, y = 0; i < N_SKILLS; i++) {
		Row_y[i] = y;
		y += Row_h + ROW_SPACE;
	}
	gr_set_current_canvas(save);
	return 0;
}

//------------------------------------------------------------------------
// The loop

// One frame on the window with the pointer of the menus (nm_show_frame)
static int show_frame(void)
{
	static ubyte under[DTU_POINTER_SIZE * DTU_POINTER_SIZE];
	grs_bitmap *scr = screen_bm();
	int px, py, x, y, ok;

	mouse_get_pos(&px, &py);
	px -= DTU_POINTER_SIZE / 2;
	py -= DTU_POINTER_SIZE / 2;
	for (y = 0; y < DTU_POINTER_SIZE; y++)
		for (x = 0; x < DTU_POINTER_SIZE; x++)
			if (px + x >= 0 && py + y >= 0 && px + x < scr->bm_w && py + y < scr->bm_h)
				under[y * DTU_POINTER_SIZE + x] = scr->bm_data[(py + y) * scr->bm_rowsize + px + x];
	dtu_display_draw_pointer(scr, px + DTU_POINTER_SIZE / 2, py + DTU_POINTER_SIZE / 2);
	gr_present();
	ok = dtufe_menu_script_frame();
	for (y = 0; y < DTU_POINTER_SIZE; y++)
		for (x = 0; x < DTU_POINTER_SIZE; x++)
			if (px + x >= 0 && py + y >= 0 && px + x < scr->bm_w && py + y < scr->bm_h)
				scr->bm_data[(py + y) * scr->bm_rowsize + px + x] = under[y * DTU_POINTER_SIZE + x];
	gr_sync_display();
	return plat_pump() && ok;
}

static int in_rect(int mx, int my, int x, int y, int w, int h)
{
	return mx >= x && mx < x + w && my >= y && my < y + h;
}

// 0xB5BB9: the screen. Returns 1 if it was shown.
static int points_screen(void)
{
	grs_canvas *save = grd_curcanv;
	int sel = 0, key = 0, btn = 0, prev_btn, plus_down = 0, minus_down = 0, i, was_rel;
	fix msg_until = 0;

	if (Dtu_char.thief_points < 1 || !grd_curscreen)
		return 0;
	Hi = Dtu_hires != 0;
	set_skill_pointers(&Dtu_char.base, Base);		// 0xB4DDE
	for (i = 0; i < N_SKILLS; i++)
		Work[i] = *Base[i];
	Limit = Dtu_char.thief_points / 2;				// 0xB4ECF
	if (Limit == 0)
		Limit = 1;
	set_skill_pointers(&Dtu_char.pa, In_use);		// 0xB4E74
	if (all_full())
		return 0;
	// 0xB4F32: the pictures and the layout
	if (!Bg_bm[Hi])
		Bg_bm[Hi] = dtu_febgnd_load(Bg_art[Hi], NULL);
	if (!Button_bm)
		Button_bm = dtu_febgnd_load(BUTTON_ART, NULL);
	Font = screen_font();
	if (!Bg_bm[Hi] || !Button_bm || !Font || layout())
		return 0;
	if (FindArg("-soak") || FindArg("-frames")) {
		dtu_char_log("thief points: %d points, the screen is not shown in a run with no one to answer", Dtu_char.thief_points);
		return 0;
	}
	dtu_char_log("thief points: the screen with %d points, at most %d for one skill", Dtu_char.thief_points, Limit);

	stop_time();									// 0x40DF9
	C_white = gr_find_closest_color(63, 63, 63);	// 0xB4C45
	C_red = gr_find_closest_color(63, 0, 0);
	C_blue = gr_find_closest_color(0, 0, 63);
	C_yellow = gr_find_closest_color(63, 63, 0);
	C_magenta = gr_find_closest_color(63, 0, 63);
	dtu_display_hide_cursor();
	gr_set_current_canvas(NULL);
	// 0xB58FE: the picture with mask, centered
	gr_bm_ubitbltm(Bg_bm[Hi]->bm_w, Bg_bm[Hi]->bm_h, Bg_x, Bg_y, 0, 0, Bg_bm[Hi], screen_bm());
	dtu_hdui_blit_key(Bg_bm[Hi], 0, 0, Bg_bm[Hi]->bm_w, Bg_bm[Hi]->bm_h, screen_bm(), Bg_x, Bg_y,
		Bg_bm[Hi]->bm_w, Bg_bm[Hi]->bm_h, 255);
	for (i = 0; i < N_SKILLS; i++)
		draw_row(i);
	select_row(0, -1);
	draw_button(1, 0);
	draw_button(0, 0);
	draw_points();
	// 0xB5CC1: the mouse stays in the content area (its center here)
	was_rel = plat_mouse_get_relative();
	mouse_set_relative(0);
	mouse_set_limits(Cx + DTU_POINTER_SIZE / 2, Cy + DTU_POINTER_SIZE / 2, Cx + Cw + DTU_POINTER_SIZE / 2,
		Cy + Ch + DTU_POINTER_SIZE / 2);
	key_flush();
	mouse_button_down_count(0);

	do {
		int mx, my;

		// the message goes after 3 seconds
		if (msg_until && timer_get_fixed_seconds() > msg_until) {
			msg_until = 0;
			restore_bg(Cx, Msg_y, Cw, Msg_h);
		}
		prev_btn = btn;
		btn = (mouse_get_btns() & 1) != 0;
		key = key_inkey();
		if (key)
			btn = 0;
		mouse_get_pos(&mx, &my);
		if (btn) {
			// 0xB5DF0: a press on plus or minus, a row under the button
			if (in_rect(mx, my, Plus_x, Plus_y, Bsize, Bsize)) {
				if (!prev_btn) {
					add_point(sel);
					draw_row(sel);
					select_row(sel, -1);
					draw_points();
				}
			} else if (in_rect(mx, my, Minus_x, Minus_y, Bsize, Bsize)) {
				if (!prev_btn) {
					remove_point(sel);
					draw_row(sel);
					select_row(sel, -1);
					draw_points();
				}
			} else
				for (i = 0; i < N_SKILLS; i++)
					if (in_rect(mx, my, Cx, Cy + Row_y[i], Cw, Row_h)) {
						if (i != sel) {
							select_row(i, sel);
							sel = i;
						}
						break;
					}
		} else switch (key) {
			case KEY_LAPOSTRO:
			case KEY_PRINT_SCREEN:
				save_screen_shot(0);					// 0x80DA1
				break;
			case KEY_MINUS:
			case KEY_LEFT:
				remove_point(sel);
				draw_row(sel);
				select_row(sel, -1);
				draw_points();
				break;
			case KEY_EQUAL:
			case KEY_RIGHT:
				add_point(sel);
				draw_row(sel);
				select_row(sel, -1);
				draw_points();
				break;
			case KEY_DOWN:
				i = sel;
				if (++sel >= N_SKILLS)
					sel = 0;
				select_row(sel, i);
				break;
			case KEY_UP:
				i = sel;
				if (--sel < 0)
					sel = N_SKILLS - 1;
				select_row(sel, i);
				break;
			case KEY_ESC:
				// 0xB5F56: points left and a skill that can take them
				if (Dtu_char.thief_points > 0 && !all_full()) {
					draw_message(LT("You must spend all points"));
					msg_until = timer_get_fixed_seconds() + 0x30000;
					digi_play_sample(3000, F1_0 / 2);	// 0x2DAD3 limits the volume to 0x8000
					key = 0;
				}
				break;
		}
		// 0xB5FC9: the button under the pressed mouse button shows down
		if (btn) {
			if (in_rect(mx, my, Plus_x, Plus_y, Bsize, Bsize)) {
				if (minus_down) { draw_button(0, 0); minus_down = 0; }
				if (!plus_down) { draw_button(1, 1); plus_down = 1; }
			} else if (in_rect(mx, my, Minus_x, Minus_y, Bsize, Bsize)) {
				if (plus_down) { draw_button(1, 0); plus_down = 0; }
				if (!minus_down) { draw_button(0, 1); minus_down = 1; }
			}
		} else {
			if (plus_down)
				draw_button(1, 0);
			if (minus_down)
				draw_button(0, 0);
			plus_down = minus_down = 0;
		}
		dtumusic_frame();							// 0xA4F83
		if (!show_frame())
			break;
	} while (key != KEY_ESC);

	// 0xB60EA: the mouse in the whole screen, the points to the skills
	mouse_set_limits(0, 0, screen_bm()->bm_w - 1, screen_bm()->bm_h - 1);
	mouse_set_relative(was_rel);
	for (i = 0; i < N_SKILLS; i++) {				// 0xB4D2F
		*In_use[i] += Work[i] - *Base[i];
		*Base[i] = Work[i];
	}
	dtu_char_log("thief points: hide %d, move %d, open locks %d, traps %d, climb %d; %d points left",
		Work[0], Work[1], Work[2], Work[3], Work[4], Dtu_char.thief_points);
	// ASCENT: the session log, the values without items
	dtulog_event("Thief points: spent, hide %d, move %d, open locks %d, traps %d, climb %d, %d points left",
		Work[0], Work[1], Work[2], Work[3], Work[4], Dtu_char.thief_points);
	key_flush();
	start_time();									// 0x40E2A
	gr_set_current_canvas(save);
	return 1;
}

int dtu_thief_points_offer(void)
{
	dtu_player_attr *b = &Dtu_char.base;

	// 0xB5B49: points, and a base skill below 95
	if (Dtu_char.thief_points < 1)
		return 0;
	if (b->hide_in_shadows >= SKILL_MAX && b->move_silently >= SKILL_MAX && b->find_traps >= SKILL_MAX &&
		 b->climb_walls >= SKILL_MAX && b->open_locks >= SKILL_MAX)
		return 0;
	if (dtu_char_choice("You have some thief skill\npoints available.\nSpend them Now?", 2,
			Text_string[32], Text_string[33]) != 0)
		return 0;
	return points_screen();
}
