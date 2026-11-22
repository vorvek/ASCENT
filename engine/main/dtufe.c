/*
 * ASCENT: new file. The front end of Descent to Undermountain. It replaces
 * the title screens and menus of Descent 1 (titles.c, menu.c, and the use
 * of newmenu.c in them). See dtufe.h and docs/re/frontend.md.
 *
 * This file has the parts that all screens use: the frame, the input, the
 * script of the option -fescript, the art (FEBGND, FESEQ, FESTREAM, FETXT,
 * the bitmaps of PORTAL.EGG), text, sound, the screen runner (0x7FD23),
 * the menu (the Descent newmenu as the executable draws it in the front
 * end), the main menu, the options, the credits and the death screen.
 *
 * An address in a comment is an address of DTUGAME.EXE 1.3 (object 1 at
 * 0x10000).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "palette.h"
#include "rle.h"
#include "key.h"
#include "mouse.h"
#include "timer.h"
#include "error.h"
#include "mem.h"
#include "args.h"
#include "text.h"
#include "piggy.h"
#include "sounds.h"
#include "digi.h"
#include "kconfig.h"
#include "platform.h"
#include "acm.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtudisp.h"
#include "dtumusic.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "newmenu.h"
#include "dtutrig.h"
#include "gamefont.h"
#include "dtufe.h"
#include "dtufe_int.h"
#include "dtumovie.h"
#include "dtustate.h"
#include "fuelcen.h"		// ASCENT: the lists of a save game
#include "switch.h"
#include "dtuscript.h"
#include "dtudlg.h"		// ASCENT: dtu_dlg_reset_view
#include "screens.h"
#include "dtuopts.h"
#include "dtuhdui.h"		// ASCENT: the upscaled pictures

int Dtu_fe_original_input = 0;
int Dtu_fe_return_waterdeep = 0;

int Fe_mx = FE_W / 2, Fe_my = FE_H / 2;
int Fe_down, Fe_press, Fe_release, Fe_rdown, Fe_rpress;
int Fe_key;
int Fe_quit;
int Fe_screen_num = -1;
grs_font *Fe_font_l, *Fe_font_m, *Fe_font_s;
grs_bitmap *Fe_cursor_bm;
int Fe_cursor_hot_x, Fe_cursor_hot_y, Fe_cursor_trans;

static grs_bitmap *Fe_bg;				// picture of the screen (FeOffScreenCanv, 0x13E378)
static ubyte Fe_pal[768];
static int Fe_need_fade_in;
static int Fe_ready;
static int Fe_frame_count;
static int Fe_mode = FE_GO_MENU;		// Function_mode of the executable (0x1468CC)
static int Fe_have_character;			// 0x141BBB
static int Fe_in_game;					// the front end started the game that runs
static int Fe_was_relative;
static int Fe_loading_from;			// see dtufe_loading_from_frontend()
static int Fe_ready_tables;			// the front end had its tables one time

// screen number -> FEBGND entry (0x13E3D7), and the entry of the stone
// texture that the menu boxes of the screen use (0x13E40B)
static int Screen_febgnd[13] = { 10, 0, 26, 14, 1, 12, 2, 3, 4, 5, 18, 19, 16 };
static int Screen_menu_bg[13] = { 11, 1, 16, 15, 1, 13, 6, 7, 8, 9, 16, 16, 16 };

// hue -> color of the palette (tables through 0x13E505). Hue 8 and hue 10
// depend on the screen.
static ubyte Hue_color[11] = { 0, 255, 247, 248, 249, 254, 253, 250, 0, 251, 0 };
static ubyte Hue8[10] = { 132, 69, 64, 132, 69, 49, 146, 129, 137, 101 };
static ubyte Hue10[10] = { 188, 233, 224, 188, 233, 241, 193, 231, 201, 238 };

extern char Dtu_player_archive[16];
extern int Dtu_frames_done;

//------------------------------------------------------------------------
// Small helpers

static uint get_u32(ubyte *p)
{
	return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24);
}

static uint get_be32(ubyte *p)
{
	return (uint)p[3] | ((uint)p[2] << 8) | ((uint)p[1] << 16) | ((uint)p[0] << 24);
}

static int get_be16(ubyte *p)
{
	return (p[0] << 8) | p[1];
}

static grs_bitmap *screen_bm(void)
{
	return &grd_curscreen->sc_canvas.cv_bitmap;
}

int fe_rand(void)
{
	return rand() & 0x7fff;
}

// Reads a part of a data file of the game. Returns memory that the caller frees.
static ubyte *read_part(char *name, int offset, int size)
{
	char path[600];
	FILE *f;
	ubyte *p;

	if (size <= 0 || !dtu_find_file(name, path, sizeof(path)))
		return NULL;
	f = fopen(path, "rb");
	if (!f)
		return NULL;
	p = malloc(size);
	if (p && (fseek(f, offset, SEEK_SET) != 0 || fread(p, 1, size, f) != (size_t) size)) {
		free(p);
		p = NULL;
	}
	fclose(f);
	return p;
}

//------------------------------------------------------------------------
// The script of the option -fescript FILE. One command in each line:
//   wait N          N frames with no input
//   move X Y        put the cursor at X, Y
//   click X Y       press the left button at X, Y, release it in the next frame
//   rclick X Y      the same with the right button
//   press X Y       press the left button at X, Y and hold it
//   release         release the left button
//   key NAME        esc, enter, space, pgup, pgdn, left, right, up, down,
//                   backsp, or one character
//   text STRING     the characters of STRING, one in each frame
//   shot FILE       write the frame to a PPM file
//   quit            leave the program
//   gamekeys        from now on the script also runs in the frames of the
//                   game, and its keys go to the game
//   hold NAME N     (ASCENT) hold the key down for N frames of the game
//                   or of a menu; the script goes on at once
//   padbutton NAME D  (ASCENT) a gamepad button down (D 1) or up (D 0):
//                   a b x y back guide start ls rs lb rb dup ddown dleft dright
//   padaxis NAME V  (ASCENT) a gamepad axis to V (-32768 to 32767):
//                   lx ly rx ry lt rt. Both go through the functions that
//                   plat_pump calls for the events of a real gamepad.
//   wheel N         (ASCENT) N steps of the mouse wheel (negative: toward
//                   the user)
// ASCENT: a key name can start with "ctrl-" (Ctrl and the key), and the
// names tab, home, end, ins, del, lctrl, lshift, lalt, f11, f12 exist.
// While the script has commands the clock of the front end is the frame
// count (30 frames are one second), so a run always gives the same frames.

#define SCRIPT_MAX	400

static char *Script_line[SCRIPT_MAX];
static int Script_n, Script_pos, Script_wait;
static int Script_btn, Script_rbtn;
static char Script_text[80];
static int Script_text_pos;
static char *Script_shot;
static int Script_in_game;			// the command "gamekeys" was run
static int Script_hold_key, Script_hold_frames;	// ASCENT: the command "hold"
static int Script_moved;			// ASCENT: a command put the cursor at Fe_mx, Fe_my

static int script_active(void)
{
	return Script_pos < Script_n || Script_wait > 0 || Script_btn || Script_rbtn || Script_text[Script_text_pos];
}

static void script_load(void)
{
	FILE *f;
	char line[200];
	int t = FindArg("-fescript");
	static int loaded;

	// ASCENT: called by fe_enter and by the direct start (-level)
	if (loaded || !t || t + 1 >= Num_args)
		return;
	loaded = 1;
	f = fopen(Args[t+1], "r");
	if (!f) {
		printf("Cannot open the script %s\n", Args[t+1]);
		return;
	}
	while (Script_n < SCRIPT_MAX && fgets(line, sizeof(line), f)) {
		int n = (int) strlen(line);
		while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r' || line[n-1] == ' '))
			line[--n] = 0;
		if (!n || line[0] == '#')
			continue;
		Script_line[Script_n] = malloc(n + 1);
		strcpy(Script_line[Script_n++], line);
	}
	fclose(f);
}

static int ascii_to_key(int c)
{
	int k;

	for (k = 1; k < 256; k++)
		if (key_to_ascii(k) == c)
			return k;
	for (k = 1; k < 256; k++)
		if (key_to_ascii(k | KEY_SHIFTED) == c)
			return k | KEY_SHIFTED;
	return 0;
}

static int script_key(char *name)
{
	static struct { char *name; int key; } names[] = {
		{ "esc", KEY_ESC }, { "enter", KEY_ENTER }, { "space", KEY_SPACEBAR },
		{ "pgup", KEY_PAGEUP }, { "pgdn", KEY_PAGEDOWN }, { "left", KEY_LEFT },
		{ "right", KEY_RIGHT }, { "up", KEY_UP }, { "down", KEY_DOWN }, { "backsp", KEY_BACKSP },
		{ "f1", KEY_F1 }, { "f2", KEY_F2 }, { "f3", KEY_F3 }, { "f4", KEY_F4 }, { "f5", KEY_F5 },
		{ "f6", KEY_F6 }, { "f7", KEY_F7 }, { "f8", KEY_F8 }, { "f9", KEY_F9 }, { "f10", KEY_F10 },
		// ASCENT: more names for the key table tests
		{ "f11", KEY_F11 }, { "f12", KEY_F12 }, { "tab", KEY_TAB }, { "home", KEY_HOME }, { "end", KEY_END },
		{ "ins", KEY_INSERT }, { "del", KEY_DELETE }, { "lctrl", KEY_LCTRL }, { "lshift", KEY_LSHIFT }, { "lalt", KEY_LALT } };
	int i;

	if (!strnicmp(name, "ctrl-", 5) && name[5])
		return KEY_CTRLED | (script_key(name + 5) & 0xFF);

	for (i = 0; i < (int) (sizeof(names) / sizeof(names[0])); i++)
		if (!stricmp(name, names[i].name))
			return names[i].key;
	return ascii_to_key(name[0]);
}

// Sets the input of one frame from the script.
static void script_frame(void)
{
	Fe_key = 0;
	Script_moved = 0;
	if (Script_btn == 1 || Script_rbtn) {		// the frame after a click: release
		Script_btn = Script_rbtn = 0;
		return;
	}
	if (Script_text[Script_text_pos]) {
		Fe_key = ascii_to_key(Script_text[Script_text_pos++]);
		return;
	}
	if (Script_wait > 0) {
		Script_wait--;
		return;
	}
	while (Script_pos < Script_n) {
		char *s = Script_line[Script_pos++];
		char cmd[16], arg[160];
		int x = 0, y = 0;

		cmd[0] = arg[0] = 0;
		sscanf(s, "%15s %159[^\n]", cmd, arg);
		sscanf(arg, "%d %d", &x, &y);
		if (!stricmp(cmd, "wait")) {
			Script_wait = x > 0 ? x - 1 : 0;
			return;
		} else if (!stricmp(cmd, "move")) {
			Fe_mx = x; Fe_my = y; Script_moved = 1;
		} else if (!stricmp(cmd, "click")) {
			Fe_mx = x; Fe_my = y; Script_btn = 1; Script_moved = 1;
			return;
		} else if (!stricmp(cmd, "press")) {
			Fe_mx = x; Fe_my = y; Script_btn = 2; Script_moved = 1;
			return;
		} else if (!stricmp(cmd, "release")) {
			Script_btn = 0;
			return;
		} else if (!stricmp(cmd, "rclick")) {
			Fe_mx = x; Fe_my = y; Script_rbtn = 1; Script_moved = 1;
			return;
		} else if (!stricmp(cmd, "key")) {
			Fe_key = script_key(arg);
			return;
		} else if (!stricmp(cmd, "text")) {
			strncpy(Script_text, arg, sizeof(Script_text) - 1);
			Script_text_pos = 0;
			if (Script_text[0])
				Fe_key = ascii_to_key(Script_text[Script_text_pos++]);
			return;
		} else if (!stricmp(cmd, "shot")) {
			Script_shot = s + 5;
			return;
		} else if (!stricmp(cmd, "quit")) {
			Fe_quit = 1;
			return;
		} else if (!stricmp(cmd, "padbutton") || !stricmp(cmd, "padaxis")) {
			static char *buttons[] = { "a", "b", "x", "y", "back", "guide", "start", "ls", "rs", "lb", "rb",
				"dup", "ddown", "dleft", "dright" };
			static char *axes[] = { "lx", "ly", "rx", "ry", "lt", "rt" };
			char pname[16];
			int v = 0, i, is_axis = !stricmp(cmd, "padaxis");
			pname[0] = 0;
			sscanf(arg, "%15s %d", pname, &v);
			for (i = 0; i < (is_axis ? 6 : 15); i++)
				if (!stricmp(pname, is_axis ? axes[i] : buttons[i])) {
					if (is_axis)
						plat_input_pad_axis(i, v);
					else
						plat_input_pad_button(i, v);
					break;
				}
			return;
		} else if (!stricmp(cmd, "hold")) {
			char kname[32];
			int n = 0;
			kname[0] = 0;
			sscanf(arg, "%31s %d", kname, &n);
			Script_hold_key = script_key(kname) & 0xFF;
			Script_hold_frames = n;
			if (Script_hold_key && n > 0)
				plat_input_key(Script_hold_key, 1);
		} else if (!stricmp(cmd, "gamekeys")) {
			// ASCENT: from here on the script also runs in the frames of
			// the game; its keys go to the game (dtufe_game_frame)
			Script_in_game = 1;
		} else if (!stricmp(cmd, "wheel")) {
			plat_input_mouse_wheel(x);		// ASCENT: the menus read it
			return;
		} else
			printf("Script: unknown command '%s'\n", s);
	}
}

// ASCENT: the script of -fescript also drives the message boxes and menus
// of newmenu.c (the death menu uses them, as the executable does). This is
// called from nm_show_frame() after the frame is on the screen. A pending
// "shot" writes that frame. A "key" goes into the key buffer. Returns 0
// after "quit".
int dtufe_menu_script_frame(void)
{
	if (!Script_n)
		return 1;
	if (Script_shot) {
		if (dtu_write_screenshot(Script_shot))
			printf("Wrote %s (menu frame %d)\n", Script_shot, Fe_frame_count);
		else
			printf("Cannot write %s\n", Script_shot);
		Script_shot = NULL;
	}
	// ASCENT: the key of the command "hold" goes up after its frames
	if (Script_hold_frames > 0 && --Script_hold_frames == 0)
		plat_input_key(Script_hold_key, 0);
	if (script_active()) {
		script_frame();
		if (Fe_key) {
			if (Fe_key & KEY_CTRLED)		// ASCENT: Ctrl with the key
				plat_input_key(KEY_LCTRL, 1);
			plat_input_key(Fe_key & 0xFF, 1);
			plat_input_key(Fe_key & 0xFF, 0);
			if (Fe_key & KEY_CTRLED)
				plat_input_key(KEY_LCTRL, 0);
			Fe_key = 0;
		}
		// ASCENT: move, click and rclick also drive the mouse pointer of
		// the menus and of the conversation window (newmenu.c, dtudlg.c)
		{
			static int last_l, last_r;
			if (Script_moved)
				plat_input_mouse_motion(0, 0, Fe_mx, Fe_my);
			if ((Script_btn != 0) != last_l)
				plat_input_mouse_button(0, last_l = (Script_btn != 0));
			if ((Script_rbtn != 0) != last_r)
				plat_input_mouse_button(1, last_r = (Script_rbtn != 0));
		}
	}
	Fe_frame_count++;
	return !Fe_quit;
}

//------------------------------------------------------------------------
// The frame

fix fe_time(void)
{
	if (Script_n)
		return (fix) (((int64_t) Fe_frame_count * F1_0) / 30);
	return timer_get_fixed_seconds();
}

void fe_input_clear(void)
{
	Fe_press = Fe_release = Fe_rpress = 0;
	Fe_key = 0;
}

static void speech_frame(void);

int fe_frame_begin(void)
{
	static int prev_l, prev_r;
	int l, r;
	grs_bitmap *sb = screen_bm();

	if (!plat_pump())
		Fe_quit = 1;

	if (script_active()) {
		script_frame();
		l = Script_btn != 0;
		r = Script_rbtn;
		key_flush();
	} else {
		int b = mouse_get_btns();
		mouse_get_pos(&Fe_mx, &Fe_my);
		l = b & 1;
		r = (b >> 1) & 1;
		Fe_key = key_inkey();
	}
	Fe_press = l && !prev_l;
	Fe_release = !l && prev_l;
	Fe_rpress = r && !prev_r;
	Fe_down = l;
	Fe_rdown = r;
	prev_l = l;
	prev_r = r;

	dtumusic_frame();
	speech_frame();

	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	if (Fe_bg && Fe_bg->bm_w == sb->bm_w && Fe_bg->bm_h == sb->bm_h) {
		int y;
		for (y = 0; y < sb->bm_h; y++)
			memcpy(sb->bm_data + y * sb->bm_rowsize, Fe_bg->bm_data + y * Fe_bg->bm_rowsize, sb->bm_w);
		dtu_hdui_blit(Fe_bg, 0, 0, sb->bm_w, sb->bm_h, sb, 0, 0, sb->bm_w, sb->bm_h);
	} else
		gr_clear_canvas(0);

	return !Fe_quit;
}

void fe_frame_end(void)
{
	if (Fe_cursor_bm)
		fe_blit(Fe_cursor_bm, Fe_mx - Fe_cursor_hot_x, Fe_my - Fe_cursor_hot_y, Fe_cursor_trans);

	if (Fe_need_fade_in) {
		Fe_need_fade_in = 0;
		if (Script_n)
			gr_palette_load(Fe_pal);
		else
			gr_palette_fade_in(Fe_pal, 32, 0);		// 0x7F50D
	}
	gr_present();

	if (Script_shot) {
		if (dtu_write_screenshot(Script_shot))
			printf("Wrote %s (front end frame %d)\n", Script_shot, Fe_frame_count);
		else
			printf("Cannot write %s\n", Script_shot);
		Script_shot = NULL;
	}

	Fe_frame_count++;
	if (!Script_n)
		plat_sleep_ms(8);
}

//------------------------------------------------------------------------
// Pictures

grs_bitmap *fe_febgnd(int entry, ubyte *pal)
{
	return dtu_febgnd_load(entry, pal);
}

void fe_fade_out(void)
{
	if (gr_palette_faded_out)
		return;
	if (Script_n)
		gr_palette_clear();
	else
		gr_palette_fade_out(Fe_pal, 16, 0);		// 0x7F54E
}

void fe_fade_in(void)
{
	Fe_need_fade_in = 1;
}

static void set_background(int entry)
{
	grs_bitmap *bm = fe_febgnd(entry, Fe_pal);

	if (Fe_bg)
		gr_free_bitmap(Fe_bg);
	Fe_bg = bm;
	if (!bm) {
		dtu_log("Front end: cannot load picture %d of FEBGND\n", entry);
		memset(Fe_pal, 0, sizeof(Fe_pal));
		memset(Fe_pal + 3 * 247, 48, 9 * 3);
	}
	// gr_find_closest_color() and the fade use gr_palette
	memcpy(gr_palette, Fe_pal, 768);
}

// 0x7F5E6: fade out, load the picture, fade in when the first frame is drawn.
int fe_load_screen(int number)
{
	if (number < 0 || number >= 13)
		return 0;
	if (Fe_screen_num == number && Fe_bg) {
		if (gr_palette_faded_out)
			fe_fade_in();
		return 1;
	}
	fe_fade_out();
	gr_palette_clear();
	set_background(Screen_febgnd[number]);
	Fe_screen_num = number;
	fe_fade_in();
	return Fe_bg != NULL;
}

static int Hue_picture_valid;
static ubyte Hue_picture[FE_HUE_MAX + 1];

void fe_show_picture(int febgnd_entry)
{
	set_background(febgnd_entry);
	Fe_screen_num = -1;
	Hue_picture_valid = 0;
}

// ASCENT: a picture that is no screen of the table (the death picture) has
// no colors for the text in its palette. The port takes the colors of the
// Waterdeep screen and uses the closest colors of the picture.
static void hue_picture_colors(void)
{
	ubyte wd[768];
	grs_bitmap *bm = fe_febgnd(Screen_febgnd[FE_SCR_WATERDEEP], wd);
	int h, c, best, best_d;

	if (bm)
		gr_free_bitmap(bm);
	for (h = 0; h <= FE_HUE_MAX; h++) {
		int i = h == 8 ? Hue8[0] : h == 10 ? Hue10[0] : Hue_color[h];
		best = 0;
		best_d = 1 << 30;
		for (c = 0; c < 256; c++) {
			int dr = wd[i*3] - Fe_pal[c*3], dg = wd[i*3+1] - Fe_pal[c*3+1], db = wd[i*3+2] - Fe_pal[c*3+2];
			int d = dr * dr + dg * dg + db * db;
			if (d < best_d) {
				best_d = d;
				best = c;
			}
		}
		Hue_picture[h] = (ubyte) (bm ? best : Hue_color[h]);
	}
	Hue_picture_valid = 1;
}

int fe_hue(int hue)
{
	int s = (Fe_screen_num >= 0 && Fe_screen_num < 10) ? Fe_screen_num : 0;

	if (hue < 0 || hue > FE_HUE_MAX)
		hue = 0;
	if (Fe_screen_num < 0 && Fe_bg) {
		if (!Hue_picture_valid)
			hue_picture_colors();
		return Hue_picture[hue];
	}
	if (hue == 8)
		return Hue8[s];
	if (hue == 10)
		return Hue10[s];
	return Hue_color[hue];
}

void fe_blit_part(grs_bitmap *bm, int sx, int sy, int w, int h, int x, int y, int trans)
{
	grs_bitmap *sb = screen_bm();
	int i, j;

	if (!bm || !bm->bm_data)
		return;
	if (sx < 0) { w += sx; x -= sx; sx = 0; }
	if (sy < 0) { h += sy; y -= sy; sy = 0; }
	if (sx + w > bm->bm_w) w = bm->bm_w - sx;
	if (sy + h > bm->bm_h) h = bm->bm_h - sy;
	if (x < 0) { w += x; sx -= x; x = 0; }
	if (y < 0) { h += y; sy -= y; y = 0; }
	if (x + w > sb->bm_w) w = sb->bm_w - x;
	if (y + h > sb->bm_h) h = sb->bm_h - y;
	if (w <= 0 || h <= 0)
		return;
	for (j = 0; j < h; j++) {
		ubyte *s = bm->bm_data + (sy + j) * bm->bm_rowsize + sx;
		ubyte *d = sb->bm_data + (y + j) * sb->bm_rowsize + x;
		if (trans < 0)
			memcpy(d, s, w);
		else
			for (i = 0; i < w; i++)
				if (s[i] != trans)
					d[i] = s[i];
	}
	dtu_hdui_blit_key(bm, sx, sy, w, h, sb, x, y, w, h, trans);	// ASCENT: the upscaled picture
}

void fe_blit(grs_bitmap *bm, int x, int y, int trans)
{
	if (bm)
		fe_blit_part(bm, 0, 0, bm->bm_w, bm->bm_h, x, y, trans);
}

void fe_restore(int x, int y, int w, int h)
{
	if (Fe_bg)
		fe_blit_part(Fe_bg, x, y, w, h, x, y, -1);
}

void fe_rect(int x, int y, int w, int h, int color)
{
	grs_bitmap *sb = screen_bm();
	int j;

	if (x < 0) { w += x; x = 0; }
	if (y < 0) { h += y; y = 0; }
	if (x + w > sb->bm_w) w = sb->bm_w - x;
	if (y + h > sb->bm_h) h = sb->bm_h - y;
	if (w <= 0 || h <= 0)
		return;
	for (j = 0; j < h; j++)
		memset(sb->bm_data + (y + j) * sb->bm_rowsize + x, color, w);
}

// The bitmaps of the front end are in PORTAL.EGG (flag 0x40). The front
// end keeps its own copy, because the archive of another level replaces
// the bitmap tables.
#define EGG_CACHE	200

static struct { char name[12]; grs_bitmap *bm; int trans; } Egg_cache[EGG_CACHE];
static int N_egg_cache;

grs_bitmap *fe_egg_bitmap(char *name, int *trans)
{
	bitmap_index bi;
	grs_bitmap *src, *bm;
	char lname[12];
	int i, y;

	for (i = 0; i < 11 && name[i]; i++)
		lname[i] = tolower(name[i]);
	lname[i] = 0;

	for (i = 0; i < N_egg_cache; i++)
		if (!strcmp(Egg_cache[i].name, lname)) {
			if (trans) *trans = Egg_cache[i].trans;
			return Egg_cache[i].bm;
		}

	bm = NULL;
	bi = piggy_find_bitmap(lname);
	if (bi.index > 0) {
		PIGGY_PAGE_IN(bi);
		src = &GameBitmaps[bi.index];
		if (src->bm_data && src->bm_w > 0 && src->bm_h > 0) {
			bm = gr_create_bitmap(src->bm_w, src->bm_h);
			if (src->bm_flags & BM_FLAG_RLE) {
				ubyte *sbits = src->bm_data + 4 + src->bm_h;
				for (y = 0; y < src->bm_h; y++) {
					gr_rle_decode(sbits, bm->bm_data + y * bm->bm_rowsize);
					sbits += src->bm_data[4 + y];
				}
			} else
				for (y = 0; y < src->bm_h; y++)
					memcpy(bm->bm_data + y * bm->bm_rowsize, src->bm_data + y * src->bm_rowsize, src->bm_w);
		}
	}
	if (!bm)
		dtu_log("Front end: no bitmap '%s' in the archive\n", lname);
	if (N_egg_cache < EGG_CACHE) {
		strcpy(Egg_cache[N_egg_cache].name, lname);
		Egg_cache[N_egg_cache].bm = bm;
		Egg_cache[N_egg_cache].trans = (bm && (GameBitmaps[bi.index].bm_flags & BM_FLAG_TRANSPARENT)) ? 255 : -1;
		if (trans) *trans = Egg_cache[N_egg_cache].trans;
		N_egg_cache++;
	}
	return bm;
}

//------------------------------------------------------------------------
// Text

void fe_string_size(grs_font *font, char *s, int *w, int *h)
{
	int aw;

	*w = *h = 0;
	if (!font || !s)
		return;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	gr_set_curfont(font);
	gr_get_string_size(s, w, h, &aw);
}

// One line. 0x805BC: the shadow is drawn first, one pixel to the right and down.
static void string_line(grs_font *font, int x, int y, int color, int shadow, char *s)
{
	gr_set_curfont(font);
	if (shadow >= 0) {
		gr_set_fontcolor(shadow, -1);
		gr_string(x + 1, y + 1, s);
	}
	gr_set_fontcolor(color, -1);
	gr_string(x, y, s);
}

// Draws the lines of s centered in the rectangle. If rh is 0 the text
// starts at ry.
void fe_string_in(grs_font *font, int rx, int ry, int rw, int rh, int hue, int shadow_hue, char *s)
{
	char line[256];
	int n_lines = 1, i, y, w, h, line_h;
	char *p;

	if (!font || !s || !s[0])
		return;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	for (p = s; *p; p++)
		if (*p == '\n' && p[1])
			n_lines++;
	line_h = font->ft_h + 1;
	y = rh ? ry + (rh - n_lines * line_h) / 2 : ry;
	p = s;
	while (*p) {
		for (i = 0; *p && *p != '\n' && i < 255; p++)
			line[i++] = *p;
		line[i] = 0;
		if (*p == '\n')
			p++;
		fe_string_size(font, line, &w, &h);
		string_line(font, rx + (rw - w) / 2, y, fe_hue(hue), shadow_hue < 0 ? -1 : fe_hue(shadow_hue), line);
		y += line_h;
	}
}

void fe_string(grs_font *font, int x, int y, int hue, int shadow_hue, char *s)
{
	char line[256];
	int i;
	char *p = s;

	if (!font || !s)
		return;
	if (x == 0x8000) {
		fe_string_in(font, 0, y, FE_W, 0, hue, shadow_hue, s);
		return;
	}
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	while (*p) {
		for (i = 0; *p && *p != '\n' && i < 255; p++)
			line[i++] = *p;
		line[i] = 0;
		if (*p == '\n')
			p++;
		string_line(font, x, y, fe_hue(hue), shadow_hue < 0 ? -1 : fe_hue(shadow_hue), line);
		y += font->ft_h + 1;
	}
}

char *fe_text(int n)
{
	if (n < 0 || n >= N_TEXT_STRINGS || !Text_string[n])
		return "";
	return Text_string[n];
}

// FETXTx.JRG and FETXTx.DAT (docs/formats/jrg.md): the bytes have their
// nibbles swapped.
static ubyte *Fetxt_jrg;
static int Fetxt_n;
static char **Fetxt_str;

char *fe_fetxt(int n)
{
	if (!Fetxt_jrg) {
		int size;
		Fetxt_jrg = dtu_load_file("FETXTE.JRG", &size);
		if (Fetxt_jrg) {
			Fetxt_n = size / 8;
			Fetxt_str = calloc(Fetxt_n, sizeof(char *));
		}
	}
	if (!Fetxt_jrg || n < 0 || n >= Fetxt_n)
		return "";
	if (!Fetxt_str[n]) {
		int size = (int) get_u32(Fetxt_jrg + 8 * n + 4), i;
		ubyte *p = read_part("FETXTE.DAT", (int) get_u32(Fetxt_jrg + 8 * n), size);
		if (!p)
			return "";
		for (i = 0; i < size; i++)
			p[i] = (ubyte) ((p[i] << 4) | (p[i] >> 4));
		p[size - 1] = 0;
		Fetxt_str[n] = (char *) p;
	}
	return Fetxt_str[n];
}

//------------------------------------------------------------------------
// FESEQ: IFF ANIM brushes (docs/formats/jrg.md)

#define FESEQ_MAX	64

static ubyte *Feseq_jrg;
static int Feseq_n;
static fe_seq *Feseq[FESEQ_MAX];

static void anim_delta(ubyte *frame, int w, int h, ubyte *p, ubyte *end)
{
	int y, x, n, k, c;

	p += 4;
	for (y = 0; y < h && p < end; y++) {
		ubyte *row = frame + y * w;
		x = 0;
		n = *p++;
		while (n-- > 0 && p < end) {
			c = *p++;
			if (c == 0) {
				k = *p++;
				c = *p++;
				while (k-- > 0) {
					if (x < w) row[x] = c;
					x++;
				}
			} else if (c > 0x80)
				x += c - 0x80;
			else
				while (c-- > 0 && p < end) {
					if (x < w) row[x] = *p;
					x++;
					p++;
				}
		}
	}
}

fe_seq *fe_seq_get(int n)
{
	ubyte *data, *p, *end, *prev = NULL;
	fe_seq *seq;
	int size, w = 0, h = 0, comp = 0, max_frames;

	if (!Feseq_jrg) {
		Feseq_jrg = dtu_load_file("FESEQ.JRG", &size);
		if (Feseq_jrg)
			Feseq_n = (int) get_u32(Feseq_jrg);
		if (Feseq_n > FESEQ_MAX)
			Feseq_n = FESEQ_MAX;
	}
	if (n < 0 || n >= Feseq_n)
		return NULL;
	if (Feseq[n])
		return Feseq[n];

	size = (int) get_u32(Feseq_jrg + 4 + 8 * n + 4);
	data = read_part("FESEQ.DAT", (int) get_u32(Feseq_jrg + 4 + 8 * n), size);
	if (!data || size < 12 || memcmp(data, "FORM", 4) || memcmp(data + 8, "ANIM", 4)) {
		free(data);
		return NULL;
	}
	seq = calloc(1, sizeof(fe_seq));
	max_frames = 64;
	seq->frames = calloc(max_frames, sizeof(grs_bitmap *));
	seq->trans = -1;

	end = data + size;
	p = data + 12;
	while (p + 12 <= end && !memcmp(p, "FORM", 4)) {		// one FORM PBM for each frame
		ubyte *fend = p + 8 + get_be32(p + 4);
		ubyte *c = p + 12;
		if (fend > end) fend = end;
		while (c + 8 <= fend) {
			int clen = (int) get_be32(c + 4);
			ubyte *body = c + 8;
			if (!memcmp(c, "BMHD", 4)) {
				w = get_be16(body);
				h = get_be16(body + 2);
				comp = body[10];
				if (body[9] == 2)
					seq->trans = get_be16(body + 12) & 255;
				seq->w = w;
				seq->h = h;
			} else if (!memcmp(c, "GRAB", 4)) {
				seq->hot_x = get_be16(body);
				seq->hot_y = get_be16(body + 2);
			} else if (!memcmp(c, "BODY", 4) && w > 0 && h > 0 && !seq->n_frames) {
				grs_bitmap *bm = gr_create_bitmap(w, h);
				int rowbytes = (w + 1) & ~1, total = rowbytes * h, o = 0, x, y;
				ubyte *raw = malloc(total), *s = body, *send = body + clen;
				memset(raw, 0, total);
				if (comp == 1) {
					while (o < total && s < send) {
						int k = *s++;
						if (k < 128) {
							k++;
							while (k-- > 0 && o < total && s < send) raw[o++] = *s++;
						} else if (k > 128) {
							k = 257 - k;
							while (k-- > 0 && o < total) raw[o++] = *s;
							s++;
						}
					}
				} else
					memcpy(raw, body, clen < total ? clen : total);
				for (y = 0; y < h; y++)
					for (x = 0; x < w; x++)
						bm->bm_data[y * bm->bm_rowsize + x] = raw[y * rowbytes + x];
				free(raw);
				seq->frames[seq->n_frames++] = bm;
				prev = bm->bm_data;
			} else if (!memcmp(c, "DLTA", 4) && prev && seq->n_frames < max_frames) {
				grs_bitmap *bm = gr_create_bitmap(w, h);
				memcpy(bm->bm_data, prev, w * h);		// gr_create_bitmap: the row size is the width
				anim_delta(bm->bm_data, w, h, body, body + clen);
				seq->frames[seq->n_frames++] = bm;
				prev = bm->bm_data;
			}
			c = body + ((clen + 1) & ~1);
		}
		p = fend + ((fend - data) & 1);
	}
	free(data);
	Feseq[n] = seq;
	return seq;
}

void fe_set_cursor(fe_seq *seq, int frame)
{
	if (!seq || !seq->n_frames) {
		Fe_cursor_bm = NULL;
		return;
	}
	if (frame < 0 || frame >= seq->n_frames)
		frame = seq->n_frames - 1;
	Fe_cursor_bm = seq->frames[frame];
	Fe_cursor_hot_x = seq->hot_x;
	Fe_cursor_hot_y = seq->hot_y;
	Fe_cursor_trans = seq->trans;
}

//------------------------------------------------------------------------
// FESTREAM: frame sequences with a screen position (feanim.c)

#define FESTREAM_MAX	64

static struct { int x, y, w, h, n; ubyte *frames; } Festream[FESTREAM_MAX];
static int Festream_n = -1;
static ubyte *Festream_jrg;

int fe_stream_count(void)
{
	if (Festream_n < 0) {
		int size, i, o = 8;
		Festream_n = 0;
		Festream_jrg = dtu_load_file("FESTREAM.JRG", &size);
		if (Festream_jrg && size >= 8) {
			int n = (int) get_u32(Festream_jrg + 4);
			for (i = 0; i < n && i < FESTREAM_MAX && o + 24 <= size; i++) {
				Festream[i].x = (int) get_u32(Festream_jrg + o);
				Festream[i].y = (int) get_u32(Festream_jrg + o + 4);
				Festream[i].w = (int) get_u32(Festream_jrg + o + 8);
				Festream[i].h = (int) get_u32(Festream_jrg + o + 12);
				Festream[i].n = (int) get_u32(Festream_jrg + o + 20);
				Festream[i].frames = Festream_jrg + o + 24;
				o += 24 + 8 * Festream[i].n;
				if (o > size)
					break;
				Festream_n = i + 1;
			}
		}
	}
	return Festream_n;
}

void fe_stream_info(int seq, int *x, int *y, int *w, int *h, int *n_frames)
{
	if (seq < 0 || seq >= fe_stream_count()) {
		*x = *y = *w = *h = *n_frames = 0;
		return;
	}
	*x = Festream[seq].x; *y = Festream[seq].y;
	*w = Festream[seq].w; *h = Festream[seq].h;
	*n_frames = Festream[seq].n;
}

grs_bitmap *fe_stream_frame(int seq, int frame)
{
	grs_bitmap *bm;
	ubyte *p;
	int w, h;

	if (seq < 0 || seq >= fe_stream_count() || Festream[seq].n < 1)
		return NULL;
	if (frame < 0 || frame >= Festream[seq].n)
		frame = Festream[seq].n - 1;
	w = Festream[seq].w;
	h = Festream[seq].h;
	if ((int) get_u32(Festream[seq].frames + 8 * frame + 4) != w * h)
		return NULL;
	p = read_part("FESTREAM.DAT", (int) get_u32(Festream[seq].frames + 8 * frame), w * h);
	if (!p)
		return NULL;
	bm = gr_create_bitmap(w, h);
	memcpy(bm->bm_data, p, w * h);
	free(p);
	bm->bm_x = Festream[seq].x;
	bm->bm_y = Festream[seq].y;
	return bm;
}

// 0x8B18D: 0.1 seconds for each frame. A click or a key shows the last frame.
grs_bitmap *fe_stream_play(int seq, fe_screen *s)
{
	grs_bitmap *bm = NULL;
	int frame = 0, n;
	fix next;

	if (seq < 0 || seq >= fe_stream_count())
		return NULL;
	n = Festream[seq].n;
	Fe_cursor_bm = NULL;		// the executable hides the cursor
	while (frame < n) {
		if (bm) gr_free_bitmap(bm);
		bm = fe_stream_frame(seq, frame);
		next = fe_time() + 0x199A;
		do {
			if (!fe_frame_begin())
				break;
			if (s && s->draw)
				s->draw(s);
			if (bm)
				fe_blit(bm, bm->bm_x, bm->bm_y, -1);
			fe_frame_end();
			if (Fe_press || Fe_key)
				frame = n - 2;
		} while (fe_time() < next && frame < n - 1);
		if (Fe_quit)
			break;
		frame++;
	}
	if (frame < n || !bm) {
		if (bm) gr_free_bitmap(bm);
		bm = fe_stream_frame(seq, -1);
	}
	fe_input_clear();
	return bm;
}

//------------------------------------------------------------------------
// Sound

// The sounds of the front end are sounds of the archive with numbers of
// the sound table (0x2DB28).
void fe_sound(int number)
{
	if (number >= 0 && number < MAX_SOUNDS && Sounds[number] != SOUND_FILE_NONE)
		digi_play_sample(number, F1_0);
}

void fe_refuse(void)			// 0x80EF7
{
	fe_sound(FE_SND_REFUSE);
}

// Speech: one stream of WERDZ.DAT (the dialogue stream, 0xA317B).
static short *Speech_data;
static int Speech_handle = -1;
static ubyte *Werdz_jrg;

int fe_speech_playing(void)
{
	return Speech_handle >= 0 && plat_audio_is_playing(Speech_handle);
}

// The mixer reads the data for a short time after the stop of a channel
// (platform.h), so the memory is released later.
#define SPEECH_OLD	8

static short *Speech_old[SPEECH_OLD];
static uint64_t Speech_old_time[SPEECH_OLD];

static void speech_release(int all)
{
	uint64_t now = plat_time_us();
	int i;

	for (i = 0; i < SPEECH_OLD; i++)
		if (Speech_old[i] && (all || now - Speech_old_time[i] > 300000))
			free(Speech_old[i]);
}

void fe_speech_stop(void)
{
	int i;

	if (Speech_handle >= 0)
		plat_audio_stop(Speech_handle);
	Speech_handle = -1;
	if (!Speech_data)
		return;
	for (i = 0; i < SPEECH_OLD - 1 && Speech_old[i]; i++)
		;
	if (Speech_old[i]) {			// no free place: wait for the mixer
		plat_sleep_ms(150);
		speech_release(1);
		i = 0;
	}
	Speech_old[i] = Speech_data;
	Speech_old_time[i] = plat_time_us();
	Speech_data = NULL;
}

static void speech_frame(void)
{
	if (Speech_handle >= 0 && !plat_audio_is_playing(Speech_handle))
		fe_speech_stop();
	speech_release(0);
}

int fe_speech_play(int n)
{
	acm_info info;
	ubyte *p;
	int size, count, i;

	fe_speech_stop();
	if (!Werdz_jrg)
		Werdz_jrg = dtu_load_file("WERDZ.JRG", &size);
	if (!Werdz_jrg || n < 0 || n >= (int) get_u32(Werdz_jrg))
		return 0;
	size = (int) get_u32(Werdz_jrg + 4 + 8 * n + 4);
	p = read_part("WERDZ.DAT", (int) get_u32(Werdz_jrg + 4 + 8 * n), size);
	if (!p)
		return 0;
	Speech_data = acm_decode(p, size, &info);
	free(p);
	if (!Speech_data)
		return 0;
	count = info.total;
	if (info.channels == 2) {		// the streams of WERDZ are mono
		count /= 2;
		for (i = 0; i < count; i++)
			Speech_data[i] = (short) ((Speech_data[2*i] + Speech_data[2*i+1]) / 2);
	}
	Speech_handle = plat_audio_play16(Speech_data, count, info.rate,
		(PLAT_AUDIO_VOLUME_MAX * Config_digi_volume) / 8, PLAT_AUDIO_PAN_CENTER, 0);
	if (Speech_handle < 0) {
		free(Speech_data);
		return 0;
	}
	return 1;
}

//------------------------------------------------------------------------
// The help bar (fehelp.c) and the screen runner (0x7FD23, feinput.c)

static char *Help_message;
static fe_screen *Cur_screen;			// the screen under a menu box
static fix Help_until;

void fe_help_message(char *s)			// 0x829B1: 3 seconds
{
	Help_message = s;
	Help_until = fe_time() + 3 * F1_0;
}

static int feature_hit(fe_feature *f, int x, int y)
{
	if (f->r) {
		int dx = x - f->cx, dy = y - f->cy;
		return dx * dx + dy * dy <= f->r * f->r;
	}
	return x >= f->x1 && x <= f->x2 && y >= f->y1 && y <= f->y2;
}

static char *feature_help(fe_feature *f)
{
	if (f->help_fn)
		return f->help_fn(f->id);
	if (f->help >= 0)
		return fe_text(f->help);
	return NULL;
}

void fe_run_screen(fe_screen *s)
{
	int i, hot, pressed = -1, pressed_key = -1;
	fix repeat_time = 0, release_until = 0;
	fe_seq *cur[2][3];
	fe_screen *prev_screen;
	fe_feature *frame_features;
	int frame_n_features;

	for (i = 0; i < s->n_controls; i++) {
		fe_control *c = &s->controls[i];
		c->bm_up = c->up ? fe_egg_bitmap(c->up, NULL) : NULL;
		c->bm_down = c->down ? fe_egg_bitmap(c->down, NULL) : NULL;
	}
	for (i = 0; i < 6; i++)
		cur[i / 3][i % 3] = s->cursor_seq[i / 3][i % 3] >= 0 ? fe_seq_get(s->cursor_seq[i / 3][i % 3]) : NULL;

	fe_load_screen(s->number);
	Help_message = NULL;
	fe_input_clear();
	s->running = 1;
	prev_screen = Cur_screen;
	Cur_screen = s;
	if (s->init)
		s->init(s);

	for (;;) {
		fe_feature *f;
		char *help;
		int key_hit = -1, ci;

		if (!s->running) {
			if (!s->may_leave || s->may_leave(s))
				break;
			s->running = 1;
		}
		if (!fe_frame_begin())
			break;
		if (s->frame)
			s->frame(s);

		frame_features = s->features;
		frame_n_features = s->n_features;
		hot = -1;
		for (i = 0; i < s->n_features; i++)
			if (feature_hit(&s->features[i], Fe_mx, Fe_my)) {
				hot = i;
				break;
			}

		// 0x81E64: a hot key is a press and a release of the feature
		if (Fe_key)
			for (i = 0; i < s->n_features; i++)
				if (Fe_key == s->features[i].key1 || Fe_key == s->features[i].key2) {
					key_hit = i;
					break;
				}
		if (key_hit >= 0) {
			f = &s->features[key_hit];
			if (f->control >= 0)
				fe_sound(FE_SND_CLICK);
			pressed_key = key_hit;
			release_until = fe_time() + F1_0 / 8;
			if (f->press) f->press(s, f->id);
			if (s->running && f->release) f->release(s, f->id);
			fe_input_clear();
		} else if (hot >= 0) {
			int act = 0;
			f = &s->features[hot];
			if (Fe_press || Fe_rpress) {
				pressed = hot;
				repeat_time = fe_time() + (2 * F1_0) / 5;
				if (f->control >= 0)
					fe_sound(FE_SND_CLICK);		// 0x7FEE2
				act = 1;
			} else if (Fe_down && pressed == hot) {
				// The executable calls the action again in each frame
				// while the button is down. The port does that only if
				// Dtu_fe_original_input is set, and repeats with a
				// delay on the controls that are marked.
				if (Dtu_fe_original_input)
					act = 1;
				else if (f->repeat && fe_time() >= repeat_time) {
					repeat_time = fe_time() + F1_0 / 8;
					act = 1;
				}
			}
			if (act) {
				if (f->press)
					f->press(s, f->id);
				if (f->repeat && f->release && !Fe_press)
					f->release(s, f->id);
				else if (Dtu_fe_original_input && !Fe_press && f->release)
					f->release(s, f->id);
				if (f->press || !Fe_press)
					Fe_release = 0;
			}
			if (Fe_release && pressed == hot && s->running) {
				pressed = -1;
				if (f->release)
					f->release(s, f->id);
			}
		}
		if (!Fe_down)
			pressed = -1;
		if (pressed_key >= 0 && fe_time() >= release_until)
			pressed_key = -1;
		// ASCENT: a handler may give the screen another feature list (the
		// quantity window of the market, as 0x84EEA does)
		if (s->features != frame_features || s->n_features != frame_n_features)
			hot = pressed = pressed_key = -1;

		if (s->draw)
			s->draw(s);

		// the picture of the control: 0x7FDB4 (cursor on it), 0x7FEE2 (pressed)
		i = pressed >= 0 ? pressed : pressed_key;
		if (i >= 0 && s->features[i].control >= 0) {
			fe_control *c = &s->controls[s->features[i].control];
			if (c->bm_down)
				fe_blit(c->bm_down, c->x2, c->y2, -1);
			else if (c->bm_up)
				fe_blit(c->bm_up, c->x, c->y, -1);
		} else if (hot >= 0 && s->features[hot].control >= 0) {
			fe_control *c = &s->controls[s->features[hot].control];
			if (c->bm_up)
				fe_blit(c->bm_up, c->x, c->y, -1);
		}

		// the help bar (0x82A04): a message has color 2, the help of
		// the feature has color 7
		help = NULL;
		i = 7;
		if (Help_message && fe_time() < Help_until) {
			help = Help_message;
			i = 2;
		} else if (hot >= 0)
			help = feature_help(&s->features[hot]);
		if (!help && s->default_help >= 0)
			help = fe_text(s->default_help);
		if (help && s->help_x2 > s->help_x1)
			fe_string_in(Fe_font_m, s->help_x1, s->help_y1, s->help_x2 - s->help_x1 + 1,
				s->help_y2 - s->help_y1 + 1, i, FE_HUE_BLACK, help);

		// the cursor: the idle picture, or the last picture of the
		// press sequence while the button is down
		ci = hot >= 0 ? s->features[hot].cursor : 0;
		if (ci < 0 || ci > 1 || !cur[ci][0])
			ci = 0;
		if (Fe_down && cur[ci][1])
			fe_set_cursor(cur[ci][1], -1);
		else
			fe_set_cursor(cur[ci][0], 0);

		fe_frame_end();
	}
	Cur_screen = prev_screen;
	fe_input_clear();
}

//------------------------------------------------------------------------
// The menu: newmenu_do3 of Descent 1 as the executable draws it while the
// front end is active (0x63FB2). The title has the large font, the other
// text the medium font. A selected item has color 7, the others color 5
// (0x63C7A). The box is a part of the stone picture of the screen (0x632E8).

static void (*Menu_backdrop)(void);

static int key_repeat(int scancode, fix *next)
{
	if (Script_n)
		return Fe_key == scancode;
	if (!keyd_pressed[scancode]) {
		*next = 0;
		return 0;
	}
	if (Dtu_fe_original_input)
		return 1;
	if (*next == 0) {
		*next = fe_time() + (2 * F1_0) / 5;
		return 1;
	}
	if (fe_time() >= *next) {
		*next = fe_time() + F1_0 / 10;
		return 1;
	}
	return 0;
}

#define SLIDER_W	96

// The start of the edit of an input menu item: the text "-empty-" becomes
// an empty field (newmenu.c of Descent 1).
static int start_edit(fe_menu_item *m, char *save, int index)
{
	strncpy(save, m->buf, 79);
	save[79] = 0;
	if (!strcmp(m->buf, fe_text(67)))
		m->buf[0] = 0;
	return index;
}

// ASCENT: the hue of a disabled item: the darkest of the hues 1 to 6 that
// is brighter than the shadow, in the palette of the screen.
static int fe_dark_hue(void)
{
	int h, best = FE_HUE_5, best_l = 0x7FFFFFFF;

	for (h = FE_HUE_1; h <= FE_HUE_6; h++) {
		int c = fe_hue(h), l = Fe_pal[c * 3] * 3 + Fe_pal[c * 3 + 1] * 6 + Fe_pal[c * 3 + 2];
		if (l > 60 && l < best_l) {
			best = h;
			best_l = l;
		}
	}
	return best;
}

// ASCENT: the text of the value of an FE_MI_CHOICE item.
static void choice_text(fe_menu_item *m, int value, char *t, int size)
{
	if (m->labels)
		snprintf(t, size, "%s", m->labels[value - m->min_value]);
	else
		snprintf(t, size, "x%d", value);
}

// ASCENT: the change of an FE_MI_CHOICE item by d. With wrap set, the value
// goes round from the last to the first.
static void choice_step(fe_menu_item *m, int d, int wrap)
{
	int v = m->value + d;

	if (m->disabled)
		return;
	if (v < m->min_value)
		v = wrap ? m->max_value : m->min_value;
	if (v > m->max_value)
		v = wrap ? m->min_value : m->max_value;
	if (v != m->value) {
		m->value = v;
		if (m->changed) m->changed(m);
	}
}

int fe_menu(char *title, char *subtitle, fe_menu_item *items, int n, int cur, int box)
{
	grs_bitmap *box_bm = NULL;
	int tw = 0, th = 0, sw = 0, sh = 0, iw = 0, w, h, x, y, i, ih, ix, top;
	int last_mx = Fe_mx, last_my = Fe_my, result = -2, border = 15;
	int editing = -1;				// the FE_MI_INPUT_MENU item that is edited, or -1
	char edit_save[80];
	fix rep_l = 0, rep_r = 0;
	ubyte pal[768];
	fe_seq *cursor = fe_seq_get(0);

	if (title && title[0])
		fe_string_size(Fe_font_l, title, &tw, &th);
	if (subtitle && subtitle[0])
		fe_string_size(Fe_font_m, subtitle, &sw, &sh);
	ih = Fe_font_m->ft_h + 2;
	for (i = 0; i < n; i++) {
		int a, b;
		fe_string_size(Fe_font_m, items[i].text ? items[i].text : "", &a, &b);
		if (items[i].type == FE_MI_SLIDER) a += 12 + SLIDER_W;
		if (items[i].type == FE_MI_CHECK) a += 22;
		if (items[i].type == FE_MI_CHOICE) {
			char t[32];
			int c, v, most = 0;
			for (v = items[i].min_value; v <= items[i].max_value; v++) {
				choice_text(&items[i], v, t, sizeof(t));
				fe_string_size(Fe_font_m, t, &c, &b);
				if (c > most) most = c;
			}
			a += 16 + most;
		}
		if (items[i].type == FE_MI_INPUT || items[i].type == FE_MI_INPUT_MENU) {
			int c;
			if (items[i].type == FE_MI_INPUT_MENU)
				fe_string_size(Fe_font_m, items[i].buf, &a, &b);
			fe_string_size(Fe_font_m, "W", &c, &b);
			c *= items[i].buf_len + 1;
			if (c > a) a = c;
		}
		items[i].w = a;
		items[i].h = ih;
		if (a > iw) iw = a;
	}
	w = iw;
	if (tw > w) w = tw;
	if (sw > w) w = sw;
	top = th + sh + 8;
	h = top + n * ih;
	w += 30 + 2 * border;
	h += 30 + 2 * border;
	if (w > FE_W) w = FE_W;
	if (h > FE_H) h = FE_H;
	x = (FE_W - w) / 2;
	y = (FE_H - h) / 2;
	ix = x + (w - iw) / 2;
	for (i = 0; i < n; i++) {
		items[i].x = ix;
		items[i].y = y + border + 15 + top + i * ih;
	}
	if (cur < 0 || cur >= n) cur = 0;
	while (cur < n - 1 && items[cur].type == FE_MI_TEXT) cur++;

	if (box && Fe_screen_num >= 0)
		box_bm = fe_febgnd(Screen_menu_bg[Fe_screen_num], pal);

	fe_input_clear();
	while (result == -2) {
		fe_menu_item *it;
		int k;

		if (!fe_frame_begin()) {
			result = -1;
			break;
		}
		k = Fe_key;
		it = &items[cur];

		// An input menu item that is edited (newmenu of Descent 1): Esc
		// gives the old text back and ends the edit, Enter ends the menu.
		if (editing >= 0) {
			if (k == KEY_ESC) {
				strcpy(items[editing].buf, edit_save);
				editing = -1;
				k = 0;
			} else if (k == KEY_ENTER) {
				result = editing;
				k = 0;
			} else if (k == KEY_UP || k == KEY_DOWN)
				k = 0;
		}

		if (k == KEY_ESC)
			result = -1;
		else if (k == KEY_UP || k == KEY_DOWN) {
			int d = k == KEY_UP ? n - 1 : 1, c = cur;
			do c = (c + d) % n; while (items[c].type == FE_MI_TEXT && c != cur);
			cur = c;
		} else if (k == KEY_ENTER) {
			if (it->type == FE_MI_CHECK) {
				it->value = !it->value;
				if (it->changed) it->changed(it);
			} else if (it->type == FE_MI_INPUT_MENU)
				editing = start_edit(it, edit_save, cur);
			else if (it->type == FE_MI_CHOICE)
				choice_step(it, 1, 1);
			else if (it->type != FE_MI_SLIDER)
				result = cur;
		} else if (k == KEY_SPACEBAR && it->type == FE_MI_CHECK) {
			it->value = !it->value;
			if (it->changed) it->changed(it);
		} else if ((it->type == FE_MI_INPUT || (it->type == FE_MI_INPUT_MENU && editing == cur)) && k) {
			int len = (int) strlen(it->buf), c = key_to_ascii(k);
			if (k == KEY_BACKSP) {
				if (len > 0) it->buf[len - 1] = 0;
			} else if (c != 255 && len < it->buf_len &&
						(isalnum(c) || c == '-' || c == '_' || c == ' ')) {		// 0x13CD1B
				it->buf[len] = (char) c;
				it->buf[len + 1] = 0;
			}
		}
		if (it->type == FE_MI_SLIDER) {
			int d = 0;
			if (key_repeat(KEY_LEFT, &rep_l)) d = -1;
			if (key_repeat(KEY_RIGHT, &rep_r)) d = 1;
			if (d && it->value + d >= it->min_value && it->value + d <= it->max_value) {
				it->value += d;
				if (it->changed) it->changed(it);
			}
		}
		if (it->type == FE_MI_CHOICE) {
			// ASCENT: a new press of left or right changes the value by one
			if (Dtu_fe_original_input ? key_repeat(KEY_LEFT, &rep_l) : k == KEY_LEFT)
				choice_step(it, -1, 0);
			else if (Dtu_fe_original_input ? key_repeat(KEY_RIGHT, &rep_r) : k == KEY_RIGHT)
				choice_step(it, 1, 0);
		}

		// the mouse: the item under the cursor becomes the current one
		for (i = 0; i < n; i++) {
			fe_menu_item *m = &items[i];
			if (m->type == FE_MI_TEXT || Fe_mx < m->x || Fe_mx >= m->x + iw || Fe_my < m->y || Fe_my >= m->y + ih)
				continue;
			if (editing >= 0 && editing != i)
				continue;
			if (Fe_mx != last_mx || Fe_my != last_my || Fe_press)
				cur = i;
			if (Fe_press || (Dtu_fe_original_input && Fe_down)) {
				if (m->type == FE_MI_MENU)
					result = i;
				else if (m->type == FE_MI_INPUT_MENU && editing < 0)
					editing = start_edit(m, edit_save, i);
				else if (m->type == FE_MI_CHECK) {
					m->value = !m->value;
					if (m->changed) m->changed(m);
				} else if (m->type == FE_MI_CHOICE && Fe_press)
					choice_step(m, 1, 1);
			}
			if (m->type == FE_MI_CHOICE && Fe_rpress)
				choice_step(m, -1, 1);
			if (m->type == FE_MI_SLIDER && Fe_down && Fe_mx >= m->x + iw - SLIDER_W) {
				int v = m->min_value + ((Fe_mx - (m->x + iw - SLIDER_W)) * (m->max_value - m->min_value + 1)) / SLIDER_W;
				if (v > m->max_value) v = m->max_value;
				if (v != m->value) {
					m->value = v;
					if (m->changed) m->changed(m);
				}
			}
		}
		last_mx = Fe_mx;
		last_my = Fe_my;

		if (Menu_backdrop)
			Menu_backdrop();
		else if (Cur_screen && Cur_screen->draw)
			Cur_screen->draw(Cur_screen);
		if (box) {
			if (box_bm)
				fe_blit_part(box_bm, 0, 0, w, h, x, y, -1);
			else
				fe_rect(x, y, w, h, fe_hue(FE_HUE_BLACK));
			// the dark edge at the right and at the bottom (0x634AF)
			fe_rect(x + w - 3, y + 3, 3, h - 3, fe_hue(FE_HUE_BLACK));
			fe_rect(x + 3, y + h - 3, w - 3, 3, fe_hue(FE_HUE_BLACK));
		}
		if (title && title[0])
			fe_string_in(Fe_font_l, x, y + border + 15, w, 0, FE_HUE_7, FE_HUE_5, title);
		if (subtitle && subtitle[0])
			fe_string_in(Fe_font_m, x, y + border + 15 + th, w, 0, FE_HUE_7, FE_HUE_5, subtitle);
		for (i = 0; i < n; i++) {
			fe_menu_item *m = &items[i];
			int hue = (i == cur && m->type != FE_MI_TEXT) ? FE_HUE_7 : FE_HUE_5;
			if (m->disabled)
				hue = i == cur ? FE_HUE_5 : fe_dark_hue();	// ASCENT
			if (m->type == FE_MI_INPUT_MENU && editing != i) {
				fe_string(Fe_font_m, m->x, m->y, hue, FE_HUE_BLACK, m->buf);
				continue;
			}
			if (m->type == FE_MI_INPUT || m->type == FE_MI_INPUT_MENU) {
				char buf[80];
				sprintf(buf, "%.60s%s", m->buf, (i == cur && ((fe_time() >> 14) & 1)) ? "_" : "");
				fe_rect(m->x - 2, m->y - 1, iw + 4, ih, fe_hue(FE_HUE_BLACK));
				fe_string(Fe_font_m, m->x, m->y, hue, -1, buf);
				continue;
			}
			fe_string(Fe_font_m, m->x, m->y, hue, FE_HUE_BLACK, m->text);
			if (m->type == FE_MI_SLIDER) {
				// The executable draws the slider with characters of the
				// Descent font. The port draws a bar.
				int sx = m->x + iw - SLIDER_W, range = m->max_value - m->min_value;
				fe_rect(sx, m->y + 2, SLIDER_W, ih - 6, fe_hue(FE_HUE_BLACK));
				if (range > 0)
					fe_rect(sx + 1, m->y + 3, ((SLIDER_W - 2) * (m->value - m->min_value)) / range, ih - 8, fe_hue(hue));
			} else if (m->type == FE_MI_CHOICE) {
				char t[32];
				int cw, chh;
				choice_text(m, m->value, t, sizeof(t));
				fe_string_size(Fe_font_m, t, &cw, &chh);
				fe_string(Fe_font_m, m->x + iw - cw, m->y, hue, FE_HUE_BLACK, t);
			} else if (m->type == FE_MI_CHECK) {
				int sx = m->x + iw - 14;
				fe_rect(sx, m->y + 2, 12, ih - 6, fe_hue(hue));
				fe_rect(sx + 1, m->y + 3, 10, ih - 8, fe_hue(FE_HUE_BLACK));
				if (m->value)
					fe_rect(sx + 3, m->y + 5, 6, ih - 12, fe_hue(hue));
			}
		}
		fe_set_cursor(cursor, 0);
		fe_frame_end();
	}
	if (box_bm)
		gr_free_bitmap(box_bm);
	fe_input_clear();
	return result;
}

// nm_messagebox (0x655FE): the choices are menu items under the text.
int fe_message_box(char *title, char *text, int n_choices, char *c0, char *c1)
{
	fe_menu_item m[2];

	memset(m, 0, sizeof(m));
	m[0].type = FE_MI_MENU; m[0].text = c0;
	m[1].type = FE_MI_MENU; m[1].text = c1;
	return fe_menu(title, text, m, n_choices, 0, 1);
}

int fe_input_box(char *title, char *buf, int max_len)
{
	fe_menu_item m[1];
	char save[64];

	memset(m, 0, sizeof(m));
	strncpy(save, buf, sizeof(save) - 1);
	save[sizeof(save) - 1] = 0;
	m[0].type = FE_MI_INPUT; m[0].text = ""; m[0].buf = buf; m[0].buf_len = max_len;
	if (fe_menu(NULL, title, m, 1, 0, 1) < 0) {
		strcpy(buf, save);
		return 0;
	}
	return 1;
}

//------------------------------------------------------------------------
// Options (0x58C18). The items of the executable that the port does not
// have are left out: detail levels, controls, joystick, and the change of
// the resolution of the dungeon.

static void opt_music(fe_menu_item *it)
{
	Config_midi_volume = (ubyte) it->value;
	dtumusic_set_volume_level(it->value);
}

static void opt_fx(fe_menu_item *it)
{
	Config_digi_volume = (ubyte) it->value;
	digi_set_digi_volume((it->value * 32768) / 8);
	fe_sound(FE_SND_CLICK);
}

static void opt_reverse(fe_menu_item *it)
{
	Config_channels_reversed = (ubyte) it->value;
}

//------------------------------------------------------------------------
// ASCENT: the menu "Improvements" (dtuopts.h). It is not in the executable.
// The front end shows it with fe_menu, the game with newmenu
// (dtufe_improvements_menu). A change sets the global at once; the game
// reads the globals each frame. The settings are written to the
// configuration file when the menu closes.

static struct { char *text; int *value; } Impr_switch[] = {
	{ "Improved physics",       &Dtu_improved_physics },
	{ "Improved jump controls", &Dtu_improved_jump },
	{ "Improved pathfinding",   &Dtu_improved_pathfinding },
	{ "Improved 3D body",       &Dtu_improved_body },
	{ "Target hint",            &Dtu_target_hint },		// dtucombat.c
	{ "Streaming safe music",   &Dtu_safe_music },		// dtumusic.c
};
#define N_IMPR	((int) (sizeof(Impr_switch) / sizeof(Impr_switch[0])))

static void impr_changed(fe_menu_item *it)
{
	if (it->buf_len >= 0 && it->buf_len < N_IMPR)
		*Impr_switch[it->buf_len].value = it->value;
}

static void fe_improvements(void)
{
	fe_menu_item m[N_IMPR];
	int i;

	memset(m, 0, sizeof(m));
	for (i = 0; i < N_IMPR; i++) {
		m[i].type = FE_MI_CHECK;
		m[i].text = Impr_switch[i].text;
		m[i].value = *Impr_switch[i].value;
		m[i].buf_len = i;			// the index in Impr_switch
		m[i].changed = impr_changed;
	}
	fe_menu("Improvements", NULL, m, N_IMPR, 0, 1);
	dtu_opts_save();
}

// The in-game form: newmenu, as the Descent options menu (F2).
static void impr_menuset(int nitems, newmenu_item *items, int *last_key, int citem)
{
	int i;

	for (i = 0; i < N_IMPR && i < nitems; i++)
		*Impr_switch[i].value = items[i].value;
}

void dtufe_improvements_menu(void)
{
	newmenu_item m[N_IMPR];
	int i;

	memset(m, 0, sizeof(m));
	for (i = 0; i < N_IMPR; i++) {
		m[i].type = NM_TYPE_CHECK;
		m[i].text = Impr_switch[i].text;
		m[i].value = *Impr_switch[i].value;
	}
	newmenu_do1(NULL, "Improvements", N_IMPR, m, impr_menuset, 0);
	for (i = 0; i < N_IMPR; i++)
		*Impr_switch[i].value = m[i].value;
	dtu_opts_save();
}

//------------------------------------------------------------------------
// ASCENT: the menu "Graphics" (dtuopts.h). It is not in the executable.
// The options menu of the front end, the ESC menu, the options menu of F2
// and the key F5 in the game open it. A change sets the global at once:
// the renderer (render.c), the scale, the filtering and the shader read
// the globals each frame. The settings are written to the configuration
// file when the menu closes. With the software renderer the other items
// are dark and do not change.

static char *Gfx_onoff[] = { "Off", "On" };
static char *Gfx_renderer[] = { "Software", "GPU" };
static char *Gfx_shader[] = { "Off", "Subtle", "VGA" };

static struct { char *text; int *value; int min, max; char **labels; } Gfx_item[] = {
	{ "Renderer",              &Dtu_gpu_renderer,       0, 1, Gfx_renderer },
	{ "Animation interpolation", &Dtu_anim_interp,      0, 1, Gfx_onoff },	// both renderers
	{ "Improved occlusion",    &Dtu_improved_occlusion, 0, 1, Gfx_onoff },
	{ "Internal resolution",   &Dtu_render_scale,       DTU_RENDER_SCALE_MIN, DTU_RENDER_SCALE_MAX, NULL },
	{ "Upscaled textures",     &Dtu_upscaled_textures,  0, 1, Gfx_onoff },
	{ "Upscaled movies",       &Dtu_upscaled_movies,    0, 1, Gfx_onoff },
	{ "Anisotropic filtering", &Dtu_anisotropic,        0, 1, Gfx_onoff },
	{ "Shader",                &Dtu_shader,             DTU_SHADER_OFF, DTU_SHADER_VGA, Gfx_shader },
	{ "Improved infravision",  &Dtu_improved_infravision, 0, 1, Gfx_onoff },
};
#define N_GFX	((int) (sizeof(Gfx_item) / sizeof(Gfx_item[0])))
#define GFX_FIRST_GPU	2		// the items from here on need the GPU renderer
// The note with the software renderer. The lines are short: the box of
// newmenu is no wider than 320 pixels.
#define GFX_NOTE_1	"The software renderer"
#define GFX_NOTE_2	"ignores these settings."
#define N_GFX_LINES	(N_GFX + 3)			// the items, a blank line, the note

static fe_menu_item *Gfx_fe;

static void gfx_fe_update(void)
{
	int i;

	for (i = GFX_FIRST_GPU; i < N_GFX; i++)
		Gfx_fe[i].disabled = !Dtu_gpu_renderer;
	Gfx_fe[N_GFX + 1].text = Dtu_gpu_renderer ? "" : GFX_NOTE_1;
	Gfx_fe[N_GFX + 2].text = Dtu_gpu_renderer ? "" : GFX_NOTE_2;
}

static void gfx_fe_changed(fe_menu_item *it)
{
	if (it->buf_len >= 0 && it->buf_len < N_GFX)
		*Gfx_item[it->buf_len].value = it->value;
	gfx_fe_update();
}

static void fe_graphics(void)
{
	fe_menu_item m[N_GFX_LINES];
	int i;

	memset(m, 0, sizeof(m));
	for (i = 0; i < N_GFX; i++) {
		m[i].type = FE_MI_CHOICE;
		m[i].text = Gfx_item[i].text;
		m[i].value = *Gfx_item[i].value;
		m[i].min_value = Gfx_item[i].min;
		m[i].max_value = Gfx_item[i].max;
		m[i].labels = Gfx_item[i].labels;
		m[i].buf_len = i;			// the index in Gfx_item
		m[i].changed = gfx_fe_changed;
	}
	for (; i < N_GFX_LINES; i++) {
		m[i].type = FE_MI_TEXT;
		m[i].text = "";
	}
	Gfx_fe = m;
	gfx_fe_update();
	fe_menu("Graphics", NULL, m, N_GFX_LINES, 0, 1);
	Gfx_fe = NULL;
	dtu_opts_save();
}

// The in-game form: newmenu. Each item is a menu item "name<tab>value".
// Left and right change the value, Enter and a click go round. Escape and
// F5 close the menu.
static char Gfx_text[N_GFX][48];

static void gfx_set_text(newmenu_item *items, int i)
{
	int v = *Gfx_item[i].value;

	if (Gfx_item[i].labels)
		snprintf(Gfx_text[i], sizeof(Gfx_text[i]), "%s\t%s", Gfx_item[i].text, Gfx_item[i].labels[v - Gfx_item[i].min]);
	else
		snprintf(Gfx_text[i], sizeof(Gfx_text[i]), "%s\tx%d", Gfx_item[i].text, v);
	items[i].text = Gfx_text[i];
	items[i].redraw = 1;
}

static void gfx_set_note(newmenu_item *items)
{
	items[N_GFX + 1].text = Dtu_gpu_renderer ? "" : GFX_NOTE_1;
	items[N_GFX + 2].text = Dtu_gpu_renderer ? "" : GFX_NOTE_2;
	items[N_GFX + 1].redraw = items[N_GFX + 2].redraw = 1;
}

// Changes item i by d. With wrap set, the value goes round.
static void gfx_step(newmenu_item *items, int i, int d, int wrap)
{
	int v;

	if (i < 0 || i >= N_GFX || (i >= GFX_FIRST_GPU && !Dtu_gpu_renderer))
		return;
	v = *Gfx_item[i].value + d;
	if (v < Gfx_item[i].min)
		v = wrap ? Gfx_item[i].max : Gfx_item[i].min;
	if (v > Gfx_item[i].max)
		v = wrap ? Gfx_item[i].min : Gfx_item[i].max;
	if (v == *Gfx_item[i].value)
		return;
	*Gfx_item[i].value = v;
	gfx_set_text(items, i);
	if (i == 0)
		gfx_set_note(items);
}

static void gfx_menuset(int nitems, newmenu_item *items, int *last_key, int citem)
{
	int k = *last_key;

	if (k == KEY_F5) {
		*last_key = KEY_ESC;
		return;
	}
	if (citem < 0 || citem >= N_GFX)
		return;
	if (k == KEY_LEFT || k == KEY_PAD4)
		gfx_step(items, citem, -1, 0);
	else if (k == KEY_RIGHT || k == KEY_PAD6)
		gfx_step(items, citem, 1, 0);
	else if (k == KEY_ENTER || k == KEY_PADENTER)
		gfx_step(items, citem, 1, 1);
	else
		return;
	*last_key = 0;
}

void dtufe_graphics_menu(void)
{
	newmenu_item m[N_GFX_LINES];
	int i, c = 0;

	memset(m, 0, sizeof(m));
	for (i = 0; i < N_GFX; i++) {
		m[i].type = NM_TYPE_MENU;
		gfx_set_text(m, i);
	}
	for (; i < N_GFX_LINES; i++) {
		m[i].type = NM_TYPE_TEXT;
		m[i].text = "";
	}
	gfx_set_note(m);
	// A click on an item ends newmenu with that item: change it and show
	// the menu again.
	while ((c = newmenu_do1(NULL, "Graphics", N_GFX_LINES, m, gfx_menuset, c)) >= 0 && c < N_GFX)
		gfx_step(m, c, 1, 1);
	dtu_opts_save();
}

// ASCENT: the Controls menu of the game (kconfig.c) from the options of the
// front end. The executable has it only in the game. The menu and the key
// screens are drawn as in the game: the palette of the game (PALETTE.256)
// while they are open, then the palette of the front end again.
static void fe_controls(void)
{
	grs_canvas *save = grd_curcanv;

	gr_palette_clear();
	gr_set_current_canvas(NULL);
	dtu_load_palette();
	gr_clear_canvas(BM_XRGB(0,0,0));
	dtu_kconfig_menu();
	gr_set_current_canvas(save);
	memcpy(gr_palette, Fe_pal, 768);
	gr_palette_load(Fe_pal);
	fe_input_clear();
	key_flush();
}

// While the front end is active (byte 0x13E338) the executable builds:
// Music Volume (slider 0 to 8), a blank line, FX Volume, a blank line,
// Reverse Stereo, two blank lines, Hires Dungeon Graphics (0x13A94B), and
// "Hear Khelben In English" when the language is not English. Brightness,
// Detail levels, Controls, the sensitivity and the joystick are items of
// the menu in the game only. The port leaves out Hires Dungeon Graphics:
// its form of the dungeon is chosen at the start (option -lowres) and
// cannot change while the program runs. The language is English.
// ASCENT: the last items "Improvements..." and "Graphics..." open the
// menus of the settings of dtuopts.h (fe_improvements, fe_graphics). They
// are not in the executable.
static void fe_options(void)
{
	fe_menu_item m[10];
	int n, impr, gfx, ctrl, choice = 0;

	do {
		n = 0;
		memset(m, 0, sizeof(m));
		m[n].type = FE_MI_SLIDER; m[n].text = fe_text(350); m[n].value = Config_midi_volume; m[n].max_value = 8; m[n++].changed = opt_music;
		m[n].type = FE_MI_TEXT; m[n++].text = "";
		m[n].type = FE_MI_SLIDER; m[n].text = fe_text(349); m[n].value = Config_digi_volume; m[n].max_value = 8; m[n++].changed = opt_fx;
		m[n].type = FE_MI_TEXT; m[n++].text = "";
		m[n].type = FE_MI_CHECK; m[n].text = fe_text(351); m[n].value = Config_channels_reversed; m[n++].changed = opt_reverse;
		m[n].type = FE_MI_TEXT; m[n++].text = "";
		ctrl = n;
		m[n].type = FE_MI_MENU; m[n++].text = fe_text(353);		// ASCENT: "Controls..."
		impr = n;
		m[n].type = FE_MI_MENU; m[n++].text = "Improvements...";
		gfx = n;
		m[n].type = FE_MI_MENU; m[n++].text = "Graphics...";
		if (m[0].value > 8) m[0].value = 8;
		if (m[2].value > 8) m[2].value = 8;
		choice = fe_menu(fe_text(208), NULL, m, n, choice, 1);
		if (choice == impr && !Fe_quit)
			fe_improvements();
		else if (choice == gfx && !Fe_quit)
			fe_graphics();
		else if (choice == ctrl && !Fe_quit)
			fe_controls();
	} while ((choice == impr || choice == gfx || choice == ctrl) && !Fe_quit);
}

//------------------------------------------------------------------------
// Credits (0x2C61B): the lines of IPHOF.TXT move up over FEBGND 160, in a
// column at x 112 that is 416 wide. The music is track 2.

#define CREDITS_PICTURE	160		// 0x2C67D: 0x2C5E6(0xA0)

static void fe_credits(void)
{
	ubyte *text;
	char *lines[400];
	int size, n = 0, i, total;
	fix start, end_time = 0;
	char *p;

	text = dtu_load_file("IPHOF.TXT", &size);
	if (!text)
		return;
	p = malloc(size + 1);
	memcpy(p, text, size);
	p[size] = 0;
	free(text);
	text = (ubyte *) p;
	for (p = (char *) text; *p && n < 400; ) {
		lines[n++] = p;
		while (*p && *p != '\n' && *p != '\r') p++;
		if (*p == '\r') *p++ = 0;
		if (*p == '\n') *p++ = 0;
	}
	total = n * Fe_font_m->ft_h;

	// 0x2C5E6(0xA0): FEBGND 160 into the picture of the screen (0x80202),
	// then fade out, show it and fade in (0x7F4CF). At the end 0x2C5E6(0xA)
	// shows the Waterdeep picture again (the main menu loads its screen).
	fe_fade_out();
	gr_palette_clear();
	fe_show_picture(CREDITS_PICTURE);
	fe_fade_in();

	dtumusic_play(DTUMUSIC_CREDITS, DTUMUSIC_LOOP_FOREVER, 100);
	fe_input_clear();
	start = fe_time();
	Fe_cursor_bm = NULL;
	while (fe_frame_begin()) {
		// one pixel for each 1/60 second. The executable moves one
		// pixel in each frame of its 70 Hz display.
		int scroll = (int) (((int64_t) (fe_time() - start) * 60) >> 16);
		int y0 = 470 - scroll;

		// 0x2CEA7: the lines have one row height (the medium font), the
		// medium font, hue 7, no shadow, and are centered. A line can
		// change that with "~" commands (0x2CBD1: ~Sl/~Ss font, ~Jc/~Jl/~Jr,
		// ~cN hue) or be a comment (";"), but IPHOF.TXT has none.
		for (i = 0; i < n; i++) {
			int y = y0 + i * Fe_font_m->ft_h;
			if (y < 5 || y > 470 - Fe_font_m->ft_h)
				continue;
			fe_string_in(Fe_font_m, 112, y, 416, 0, FE_HUE_7, -1, lines[i]);
		}
		fe_frame_end();
		if (Fe_key || Fe_press)
			break;
		if (y0 + total < 0) {		// 10 seconds after the last line
			if (!end_time)
				end_time = fe_time() + 10 * F1_0;
			else if (fe_time() >= end_time)
				break;
		}
	}
	dtumusic_stop();
	free(text);
	fe_input_clear();
}

//------------------------------------------------------------------------
// Save games (autosave.c of the executable: save 0xBA99F with the slot
// menu 0xBA486, load 0xBB780 with the slot menu 0xBA647). The file of slot
// n (0 to 9) is SG<n>.SG (docs/formats/misc.md). The port reads a slot in
// the directory of the user first and then in the directories of the game
// data, and writes it in the directory of the user. The files have the
// format of the executable: a save of DTUGAME.EXE 1.3 loads in the port and
// a save of the port loads in DTUGAME.EXE. The parts that the port does not
// use come from the file that was loaded.

#define SG_VERSION			12
#define SG_BETWEEN			28		// u32: 1 = saved in Waterdeep
#define SG_LEVEL				41		// u32: Current_level_num
#define SG_GAME_TIME			49		// fix: GameTime (0x13CD30)
#define SG_KHELBEN			133	// u32 mission, u32 blurb
#define SG_ATTR				(DTU_SG_PLAYER_OFFSET + DTU_PLAYER_RECORD_SIZE)	// 7108
#define SG_RLPD				7188
#define SG_DIFFICULTY		7362	// u32 0x13CDCA, u32 0x13B7A3, u32 0x13B697
#define SG_SIZE_BETWEEN		7374	// the size of a file with "between levels" 1
#define SG_MAX_SIZE			(SG_SIZE_BETWEEN + 12 + 54 * MAX_TRIGGERS + 16 * MAX_ROBOT_CENTERS + 40 * MAX_NUM_FUELCENS)

int Fe_load_level;

// The file of the last load: the base of the next save
static ubyte *Loaded_sg;
static int Loaded_sg_size;

static void sg_forget(void)
{
	if (Loaded_sg)
		free(Loaded_sg);
	Loaded_sg = NULL;
	Loaded_sg_size = 0;
}

static void put_u32(ubyte *p, uint v)
{
	p[0] = (ubyte) v; p[1] = (ubyte) (v >> 8); p[2] = (ubyte) (v >> 16); p[3] = (ubyte) (v >> 24);
}

// Finds the file of a slot. Returns 1 if it exists.
static int sg_find(int slot, char *path, int size)
{
	char name[16];

	sprintf(name, "SG%d.SG", slot);			// "SG%d.sg"
	if (dtu_user_find(name, path, size))		// ASCENT: the directory of the user, any case
		return 1;
	return dtu_find_file(name, path, size);
}

// The description of a slot (0xBA647: "DGSS", version 12, 20 bytes), or
// 0 if the slot has no file of this version.
static int sg_description(int slot, char *desc)
{
	char path[600];
	ubyte h[28];
	FILE *f;
	int ok = 0;

	if (!sg_find(slot, path, sizeof(path)) || (f = fopen(path, "rb")) == NULL)
		return 0;
	if (fread(h, 1, 28, f) == 28 && !memcmp(h, "DGSS", 4) && get_u32(h + 4) == SG_VERSION) {
		memcpy(desc, h + 8, 20);
		desc[20] = 0;
		ok = 1;
	}
	fclose(f);
	return ok;
}

// The model archive of the player (race, sex, first class) and the other
// copies of the name and the portrait, as make_player sets them.
static void loaded_character(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int race = pa->race >= 0 && pa->race <= 4 ? pa->race : 0;
	int cls = pa->class1 >= 0 && pa->class1 <= 3 ? pa->class1 : DTU_CLASS_FIGHTER;

	sprintf(Dtu_player_archive, "%c%c%c", "hdekf"[race], pa->female ? 'f' : 'm', "cmft"[cls]);
	strncpy(Players[Player_num].callsign, Dtu_char.name, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = 0;
	Dtu_player_portrait = Dtu_char.portrait;
}

// ASCENT: the slot menus of a game that runs (the ESC menu, the death
// menu) use the menus of the game (newmenu, as the executable does in the
// game); the front end draws them with its fonts.
static int slot_menu_nm(int save, char desc[10][24])
{
	newmenu_item m[10];
	int i, n_valid = 0;

	memset(m, 0, sizeof(m));
	for (i = 0; i < 10; i++) {
		if (save) {
			m[i].type = NM_TYPE_INPUT_MENU;			// 0xBA486: type 6, 19 characters
			m[i].text_len = 19;
		} else if (desc[i][0] && strcmp(desc[i], fe_text(67)))
			n_valid++;
		else
			m[i].type = NM_TYPE_TEXT;
		m[i].text = desc[i];
	}
	if (!save && !n_valid) {
		nm_messagebox(NULL, 1, fe_text(34), fe_text(797));	// "No saved games were found"
		return -1;
	}
	return newmenu_do(NULL, fe_text(save ? 796 : 798), 10, m, NULL);
}

// The whole file of a slot, or NULL
static ubyte *sg_read(int slot, char *path, int path_size, int *size)
{
	FILE *f;
	ubyte *b;

	if (!sg_find(slot, path, path_size) || (f = fopen(path, "rb")) == NULL)
		return NULL;
	b = malloc(SG_MAX_SIZE);
	if (!b) {
		fclose(f);
		return NULL;
	}
	*size = (int) fread(b, 1, SG_MAX_SIZE, f);
	fclose(f);
	return b;
}

int fe_load_game(int in_game)
{
	fe_menu_item m[10];
	char desc[10][24], path[600];
	ubyte *b;
	int i, size = 0, n_valid = 0, choice, mode, between;

	memset(m, 0, sizeof(m));
	for (i = 0; i < 10; i++) {
		if (sg_description(i, desc[i])) {
			m[i].type = FE_MI_MENU;
			n_valid++;
		} else {
			m[i].type = FE_MI_TEXT;
			snprintf(desc[i], sizeof(desc[i]), "%s", fe_text(67));	// "-empty-"
		}
		m[i].text = desc[i];
	}
	if (in_game) {
		choice = slot_menu_nm(0, desc);
		if (choice < 0 || Fe_quit)
			return 0;
		if (nm_messagebox(NULL, 2, fe_text(32), fe_text(33), fe_text(801)) != 0)	// "Restore Game?"
			return 0;
	} else {
		if (!n_valid) {
			fe_message_box(NULL, fe_text(797), 1, fe_text(34), NULL);	// "No saved games were found"
			return 0;
		}
		// 0xBA647 puts a text item of four empty lines first. The port
		// leaves it out.
		choice = fe_menu(NULL, fe_text(798), m, 10, 0, 1);			// "Select Game to Restore"
		if (choice < 0 || Fe_quit)
			return 0;
	}

	b = sg_read(choice, path, sizeof(path), &size);
	if (!b || size < SG_SIZE_BETWEEN || !dtu_char_load_save(path)) {
		if (b)
			free(b);
		if (in_game)
			nm_messagebox(NULL, 1, fe_text(34), fe_text(802));
		else
			fe_message_box(NULL, fe_text(802), 1, fe_text(34), NULL);	// "Incompatible save file version"
		return 0;
	}
	sg_forget();
	Loaded_sg = b;
	Loaded_sg_size = size;
	between = get_u32(b + SG_BETWEEN) != 0;
	// ASCENT: 0xBC51D: the level state of the slot, and the market
	// (delete geldor.dat, 0x87C05(S, 0))
	dtustate_load_slot(choice + 1);
	fe_market_store(choice + 1, 0);
	// 0xBBA2F: the game time of the save (the times in the level state
	// files count from it)
	if ((int) get_u32(b + SG_GAME_TIME) > 0)
		GameTime = (fix) get_u32(b + SG_GAME_TIME);
	dtu_dlg_reset_view();		// ASCENT: no dark view of a conversation in the loaded game
	Fe_khelben_mission = (int) get_u32(b + SG_KHELBEN);
	Fe_khelben_blurb = (int) get_u32(b + SG_KHELBEN + 4);
	dtu_mission_read_save(path);			// ASCENT: the mission and global flags (dtuscript.c)
	// 0xBBCF2: a save in a level has three lists after 7374 (the Descent 1
	// triggers, the matcens and the fuel centers); the level start applies
	// them before the level state.
	dtustate_set_save_lists(between ? NULL : b + SG_SIZE_BETWEEN, between ? 0 : size - SG_SIZE_BETWEEN);
	mode = between ? FE_GO_WATERDEEP : FE_GO_LEVEL;
	Fe_load_level = (int) get_u32(b + SG_LEVEL);
	if (Fe_load_level < 1)
		Fe_load_level = 1;
	loaded_character();
	Fe_have_character = 1;				// 0x141BBB
	dtu_log("Front end: loaded %s: '%s', %s, level %d, %d items, %d bytes\n", path, Dtu_char.name,
		mode == FE_GO_WATERDEEP ? "in Waterdeep" : "in a level", Fe_load_level, Dtu_n_inventory, size);
	printf("Front end: loaded %s: '%s', %s, level %d, %d items\n", path, Dtu_char.name,
		mode == FE_GO_WATERDEEP ? "in Waterdeep" : "in a level", Fe_load_level, Dtu_n_inventory);
	return mode;
}

// Writes the character as a save game "between levels" (the saves of the
// main menu and of Waterdeep, 0xBA99F(1)), or as a save in a level
// (0xBA99F(0)): then "between levels" is 0 and the three lists at 7374
// follow. The level and the position of the player are in the level state
// files.
static int sg_write(int slot, char *desc, int between)
{
	char name[16];
	ubyte *b;
	FILE *f;
	int ok, size, level = Current_level_num > 0 ? Current_level_num : 1;

	b = calloc(1, SG_MAX_SIZE);
	if (!b)
		return 0;
	// The parts that the port does not use: the file of the last load
	if (Loaded_sg && Loaded_sg_size >= SG_SIZE_BETWEEN)
		memcpy(b, Loaded_sg, SG_SIZE_BETWEEN);
	memcpy(b, "DGSS", 4);
	put_u32(b + 4, SG_VERSION);
	memset(b + 8, 0, 20);
	strncpy((char *) b + 8, desc, 19);
	put_u32(b + SG_BETWEEN, between ? 1 : 0);
	memset(b + 32, 0, 9);
	strcpy((char *) b + 32, "DTU");				// the mission
	put_u32(b + SG_LEVEL, level);
	put_u32(b + SG_GAME_TIME, GameTime);
	put_u32(b + 53, 19);							// the count of the mission flags (0x13CDD4)
	put_u32(b + SG_KHELBEN, Fe_khelben_mission);
	put_u32(b + SG_KHELBEN + 4, Fe_khelben_blurb);
	dtu_char_pack(b + DTU_SG_PLAYER_OFFSET);
	put_u32(b + DTU_SG_PLAYER_OFFSET + DTU_P_HIT_POINTS, (uint) Dtu_char.pa.hit_points << 16);
	memcpy(b + SG_ATTR, &Dtu_char.base, sizeof(dtu_player_attr));
	dtu_mission_write_save(b);				// ASCENT: the mission and global flags (dtuscript.c)
	// The block of playsave.c (0x71729), with the values of SG0.SG
	if (memcmp(b + SG_RLPD, "RLPD", 4)) {
		memcpy(b + SG_RLPD, "RLPD", 4);
		b[SG_RLPD + 4] = 14;
		b[SG_RLPD + 6] = 25;
		put_u32(b + SG_RLPD + 8, 1);				// one mission record
		put_u32(b + SG_RLPD + 12, 2);			// the difficulty
		put_u32(b + SG_RLPD + 16, 1);
		strcpy((char *) b + SG_RLPD + 20, "DTU");
	}
	b[SG_RLPD + 29] = (ubyte) level;
	if (!get_u32(b + SG_DIFFICULTY))
		put_u32(b + SG_DIFFICULTY, 2);			// the difficulty (0x13CDCA)
	size = SG_SIZE_BETWEEN;
	if (!between)
		size += dtustate_write_save_lists(b + SG_SIZE_BETWEEN, SG_MAX_SIZE - SG_SIZE_BETWEEN);

	sprintf(name, "SG%d.SG", slot);
	f = dtu_user_fopen(name, "wb");			// ASCENT: the directory of the user
	ok = f && fwrite(b, 1, size, f) == (size_t) size;
	if (f && fclose(f))
		ok = 0;
	free(b);
	dtu_log("Front end: %s %s: '%s', %d bytes\n", ok ? "wrote" : "cannot write", name, Dtu_char.name, size);
	printf("Front end: %s %s: '%s', %d bytes\n", ok ? "wrote" : "cannot write", name, Dtu_char.name, size);
	return ok;
}

// ASCENT: 0xBA99F after the slot menu: in a level the state of the level
// (0x5A756), then the level table of the slot (0xBC1EC), the market
// (0x87C05(S, 1)) and the SG file.
static int save_to_slot(int slot, char *desc, int between)
{
	if (!between)
		dtustate_save_level(Current_level_num - 1);
	if (!dtustate_save_slot(slot + 1))
		dtu_log("Front end: no level table for slot %d\n", slot + 1);
	fe_market_store(slot + 1, 1);
	return sg_write(slot, desc, between);
}

void fe_save_game(void)
{
	fe_menu_item m[10];
	char desc[10][24];
	int i, choice;

	memset(m, 0, sizeof(m));
	for (i = 0; i < 10; i++) {
		if (!sg_description(i, desc[i]))
			snprintf(desc[i], sizeof(desc[i]), "%s", fe_text(67));	// "-empty-"
		m[i].type = FE_MI_INPUT_MENU;				// 0xBA486: type 6, 19 characters
		m[i].text = "";
		m[i].buf = desc[i];
		m[i].buf_len = 19;
	}
	choice = fe_menu(NULL, fe_text(796), m, 10, 0, 1);				// "Save Game"
	if (choice < 0 || Fe_quit)
		return;
	if (!save_to_slot(choice, desc[choice], 1))
		fe_message_box(NULL, fe_text(800), 1, fe_text(34), NULL);	// "Error writing character data"
}

// The save of the ESC menu: 0xBA99F(0), in a level.
static void ig_save_game(void)
{
	char desc[10][24];
	int i, choice;

	for (i = 0; i < 10; i++)
		if (!sg_description(i, desc[i]))
			snprintf(desc[i], sizeof(desc[i]), "%s", fe_text(67));	// "-empty-"
	choice = slot_menu_nm(1, desc);
	if (choice < 0 || Fe_quit)
		return;
	if (!save_to_slot(choice, desc[choice], 0))
		nm_messagebox(NULL, 1, fe_text(34), fe_text(800));
}

//------------------------------------------------------------------------
// The main menu (0x58075, items 0x57F28, actions 0x582CC). It is drawn on
// the Waterdeep picture with no box: the executable asks for the picture
// "scroll2.pcx", which is not in its picture table (0x631D2).

static void main_menu_backdrop(void)
{
	int w, h;

	// 0x58158: the copyright text with the small font, 15 pixels above the bottom
	fe_string_size(Fe_font_s, fe_text(964), &w, &h);
	fe_string_in(Fe_font_s, 0, FE_H - h - 15, FE_W, 0, FE_HUE_2, FE_HUE_BLACK, fe_text(964));
}

static int Main_menu_choice;

static void fe_main_menu(void)
{
	fe_menu_item m[7];
	int action[7], n = 0, choice, mode;
	// 0x57F28: "Save Game" and "Return to Game" are there when the full
	// list of Waterdeep is used (0x8C4B6: Khelben was visited) or a
	// character was made or loaded (0x141BBB).
	int playing = Fe_have_character || Fe_khelben_mission || Fe_khelben_blurb;

	fe_load_screen(FE_SCR_WATERDEEP);
	memset(m, 0, sizeof(m));
	m[n].text = fe_text(323); action[n++] = 5;			// Load Game...
	if (playing) {
		m[n].text = fe_text(436); action[n++] = 6;		// Save Game
	}
	m[n].text = fe_text(0);   action[n++] = 0;			// New game
	if (playing) {
		m[n].text = fe_text(782); action[n++] = 26;		// Return to Game
	}
	m[n].text = fe_text(325); action[n++] = 13;			// Options...
	m[n].text = fe_text(328); action[n++] = 23;			// Credits
	m[n].text = fe_text(2);   action[n++] = 4;				// Quit

	// The first item that is selected is the last choice (0x13CCE9, 0 at
	// the start: "Load Game...").
	Menu_backdrop = main_menu_backdrop;
	choice = fe_menu("", fe_text(963), m, n, Main_menu_choice < n ? Main_menu_choice : 0, 0);
	Main_menu_choice = choice;
	if (Fe_quit) {
		Menu_backdrop = NULL;
		return;
	}
	if (choice < 0 || action[choice] == 4) {
		// "Are you sure you want to leave the game?"
		if (fe_message_box(NULL, fe_text(195), 2, fe_text(32), fe_text(33)) == 0)
			Fe_quit = 1;
		Main_menu_choice = 0;
		Menu_backdrop = NULL;
		return;
	}
	switch (action[choice]) {
	case 0:
		Menu_backdrop = NULL;
		if (fe_new_character()) {
			Fe_have_character = 1;
			fe_town_reset();
			dtu_mission_new_character();	// ASCENT: 0x5ADC9 in MakePlayer (dtuscript.c)
			// ASCENT: 0x87EB9 to 0x87EC3: a new level state and a new
			// market (geldor.dat is deleted). The market keeps the charisma
			// of the new character (0x13E38C, see fe_market).
			dtustate_new_game();
			sg_forget();				// ASCENT: no loaded save game is the base of a save
			fe_market_new_character();
			Fe_mode = FE_GO_WATERDEEP;
		}
		break;
	case 5:					// 0xBB780(0)
		mode = fe_load_game(0);
		if (mode)
			Fe_mode = mode;
		break;
	case 6:					// 0xBA99F(1)
		fe_save_game();
		break;
	case 26:
		Fe_mode = FE_GO_WATERDEEP;
		break;
	case 13:
		fe_options();
		break;
	case 23:
		Menu_backdrop = NULL;
		fe_credits();
		break;
	}
	Menu_backdrop = NULL;
}

//------------------------------------------------------------------------
// Start and end

static grs_font *load_font(char *name)
{
	char path[600];

	if (!dtu_find_file(name, path, sizeof(path))) {
		dtu_log("Front end: no font file %s\n", name);
		return GAME_FONT;
	}
	return gr_init_font(path);
}

static void fe_enter(void)
{
	if (!Fe_ready) {
		Fe_ready = 1;
		if (FindArg("-feoriginput"))
			Dtu_fe_original_input = 1;
		script_load();
		Fe_font_l = load_font("FNTEMFL_.FNT");		// 0x13E388
		Fe_font_m = load_font("FNTEMFM_.FNT");		// 0x13E380
		Fe_font_s = load_font("FNTEMFS_.FNT");		// 0x13E384
	}
	// The bitmaps and the sounds of the front end are in the archive of
	// the Yawning Portal (PORTAL.EGG, named by YPINN.BIN). The executable
	// loads these tables before the main menu (0xB8711).
	// The front end has one form: 640x480 (0xEB2F8 with mode 0xE).
	if (grd_curscreen->sc_w != FE_W || grd_curscreen->sc_h != FE_H)
		gr_set_mode(SM_640x480V);
	if (piggy_find_bitmap("btn1ulc").index == 0) {
		// ASCENT: 0xB88FF: back from a level whose archive is not PORTAL
		// (0x80F09), the loading picture 12 shows while the tables load
		// (0x7C1E5(-1), 0x7C2EE: no fade, the next screen replaces it).
		if (Fe_ready_tables)
			dtufe_loading_picture(12);
		plat_audio_flush();
		dtu_load_level_data("ypinn");
	}
	Fe_ready_tables = 1;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	Fe_was_relative = plat_mouse_get_relative();
	plat_mouse_set_relative(0);
	key_flush();
	mouse_flush();
}

static void fe_leave(void)
{
	fe_speech_stop();
	dtumusic_stop();
	fe_fade_out();
	// The start of the level loads other tables and releases the sounds
	// of the archive. The mixer must not play one of them then.
	plat_audio_flush();
	Speech_handle = -1;
	speech_release(1);
	if (Fe_bg) {
		gr_free_bitmap(Fe_bg);
		Fe_bg = NULL;
	}
	Fe_screen_num = -1;
	Fe_cursor_bm = NULL;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	gr_clear_canvas(0);
	if (!Dtu_hires)
		gr_set_mode(SM_320x200C);
	dtu_load_palette();
	plat_mouse_set_relative(Fe_was_relative);
	key_flush();
	mouse_flush();
}

// The title picture (0xB85B8): FEBGND 193 with the version at the bottom
// right. The executable shows it while it loads the tables.
void dtufe_title(void)
{
	fix until;
	int w, h;
	char *version = "v1.3";

	dtumovie_title();		// 0xB84F3: LOGO22S.MVE, INTRO2.MVE, TLOGOS.MVE (dtumovie.c)
	fe_enter();
	fe_show_picture(193);
	gr_palette_clear();
	fe_fade_in();
	until = fe_time() + 2 * F1_0;
	fe_string_size(Fe_font_s, version, &w, &h);
	while (fe_frame_begin()) {
		gr_set_curfont(Fe_font_s);
		gr_set_fontcolor(gr_find_closest_color(12, 0, 28), -1);
		gr_string(FE_W - (w + 10), FE_H - (h + 10), version);
		gr_set_fontcolor(gr_find_closest_color(63, 0, 0), -1);
		gr_string(FE_W - (w + 11), FE_H - (h + 11), version);
		fe_frame_end();
		if (Fe_key || Fe_press || fe_time() >= until)
			break;
	}
	fe_input_clear();
}

int dtufe_run(void)
{
	fe_enter();
	while (!Fe_quit) {
		switch (Fe_mode) {
		case FE_GO_MENU:
			fe_main_menu();
			break;
		case FE_GO_WATERDEEP:
			Fe_mode = fe_waterdeep();
			break;
		case FE_GO_BLACKSTAFF:
			fe_blackstaff();
			Fe_mode = FE_GO_WATERDEEP;
			break;
		case FE_GO_MARKET:
			fe_market();
			Fe_mode = FE_GO_WATERDEEP;
			break;
		case FE_GO_PORTAL: {
			// 0x5895E: the Yawning Portal is level 1 of the mission
			int level = dtu_find_level_num("YPINN.LVL");
			if (!level)
				level = dtu_find_level_num("YPINN");
			Fe_mode = FE_GO_MENU;
			Dtu_fe_return_waterdeep = 0;
			fe_leave();
			Fe_in_game = 1;
			Fe_loading_from = 1;		// 0xB8A09: 0x13E324 = 1
			return level ? level : 1;
			}
		case FE_GO_LEVEL:
			// A save game of a level: 0xBB780 starts the level
			// (0x4E33D) and the game goes on in Function_mode 2.
			Fe_mode = FE_GO_MENU;
			Dtu_fe_return_waterdeep = 0;
			fe_leave();
			Fe_in_game = 1;
			Fe_loading_from = 2;
			return Fe_load_level;
		default:
			Fe_mode = FE_GO_MENU;
		}
	}
	fe_leave();
	return 0;
}

// The ends of the game that go to the front end.

static int Fe_leave_game;				// 1: the game ends after this frame
static int Fe_portal_exit;				// the player entered the exit of the inn
static int Fe_portal_frame = -10;		// FrameCount of the last call of command 0

static int Fe_graphics_menu;			// ASCENT: the key F5 was pressed in this frame
static int Fe_escape_menu;				// the Escape key was pressed in this frame
static int Fe_inn_flag;					// 0x13B254: command 0 of DTU.WRP was run in this game
static int Fe_game_loaded;				// the ESC menu loaded a save game: the FE_GO_ mode

// 0 (the executable): a save game of Waterdeep that the ESC menu loads
// gives the character of the save, and the game goes on in the level that
// runs (0x423E1 sets Function_mode 2 again after the load set 4). 1
// (default): the game ends and the player is in Waterdeep.
int Dtu_fix_escape_load_waterdeep = 1;

// ASCENT: the direct start (-level NAME) skips the front end. The death of
// the player must still go to the death screen and the death menu, so the
// game runs as if the front end started it. It gets a new level state, as
// a new character does.
void dtufe_direct_start(void)
{
	Fe_in_game = 1;
	// ASCENT: -fescript with -level drives the game and its menus (as
	// after the command "gamekeys")
	script_load();
	if (Script_n)
		Script_in_game = 1;
	dtustate_new_game();
	sg_forget();
}

// 0x4225A: game() shows its ESC menu when the game loop ended with another
// Function_mode (the Escape key sets FMODE_MENU).
int dtufe_escape_key(void)
{
	if (!Fe_in_game)
		return 0;
	Fe_escape_menu = 1;
	return 1;
}

// ASCENT: the key F5 opens the Graphics menu after the frame. F5 and F6
// are the F keys that the key switch of the executable (0x42F9C) does not
// handle; F3 changes the screen size there (dtudisp.c).
int dtufe_graphics_key(void)
{
	if (!Fe_in_game)
		return 0;
	Fe_graphics_menu = 1;
	return 1;
}

// 0x42352 to 0x423F5
static void escape_menu(void)
{
	int c, mode;

	if (Fe_inn_flag) {
		// The byte 0x13B254 stays set after "No" to the question of the
		// exit of the inn: the Escape key asks the question again.
		if (nm_messagebox(NULL, 2, fe_text(32), fe_text(33), fe_text(795)) == 0) {
			Dtu_fe_return_waterdeep = 1;
			Fe_leave_game = 1;
		}
		return;
	}
	// The text of the executable is "ESC Menu\n" (0x1256FA). ASCENT: the
	// choices "Improvements" (dtufe_improvements_menu) and "Graphics"
	// (dtufe_graphics_menu) are not in the executable. The ESC menu comes
	// back after them. nm_messagebox takes at most 5 choices, so the items
	// are built here; the box is the same.
	do {
		static char *choices[6] = { "Return to dungeon", "Load Game", "Save Game", "Improvements", "Graphics", "Abort Game" };
		newmenu_item m[6];
		int i;

		memset(m, 0, sizeof(m));
		for (i = 0; i < 6; i++) {
			m[i].type = NM_TYPE_MENU;
			m[i].text = choices[i];
		}
		c = newmenu_do(NULL, "ESC Menu\n", 6, m, NULL);
		if (c == 3)
			dtufe_improvements_menu();
		else if (c == 4)
			dtufe_graphics_menu();
	} while (c == 3 || c == 4);
	if (c == 5)
		c = 4;			// the number of "Abort Game" in the original
	switch (c) {
	case 1:					// 0xBB780(1)
		mode = fe_load_game(1);
		if (mode == FE_GO_LEVEL || (mode == FE_GO_WATERDEEP && Dtu_fix_escape_load_waterdeep)) {
			Fe_game_loaded = mode;
			Fe_leave_game = 1;
		}
		break;
	case 2:					// 0xBA99F(0)
		ig_save_game();
		break;
	case 4:					// "Abort Game?" (text 103)
		if (nm_messagebox(NULL, 2, fe_text(32), fe_text(33), fe_text(103)) == 0) {
			// 0x42649: no character (0x141BBB = 0), the main menu
			Fe_have_character = 0;
			Fe_leave_game = 1;
		}
		break;
	}
}

// The loading pictures (0x7C1E5, 0x7C2BB). The executable sets the video
// mode 640x480 for the picture. The port draws the picture scaled to the
// screen of the game.
static ubyte Loading_pal[768];

int dtufe_loading_from_frontend(void)
{
	int r = Fe_loading_from;

	Fe_loading_from = 0;
	return r;
}

int dtufe_loading_picture(int entry)
{
	grs_bitmap *bm = fe_febgnd(entry, Loading_pal), *sb;
	int x, y;

	if (!bm)
		return 0;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	sb = screen_bm();
	for (y = 0; y < sb->bm_h; y++) {
		ubyte *d = sb->bm_data + y * sb->bm_rowsize;
		ubyte *s = bm->bm_data + (y * bm->bm_h / sb->bm_h) * bm->bm_rowsize;
		for (x = 0; x < sb->bm_w; x++)
			d[x] = s[x * bm->bm_w / sb->bm_w];
	}
	dtu_hdui_blit(bm, 0, 0, bm->bm_w, bm->bm_h, sb, 0, 0, sb->bm_w, sb->bm_h);	// ASCENT: the upscaled picture
	gr_free_bitmap(bm);
	memcpy(gr_palette, Loading_pal, 768);		// the screenshot of a script uses gr_palette
	gr_palette_load(Loading_pal);
	gr_present();
	dtu_log("Front end: loading picture %d\n", entry);
	if (Dtu_debug)		// ASCENT: -debug or the debug build (dtufile.h)
		printf("Loading picture %d\n", entry);
	if (FindArg("-loadshots")) {		// test option: a screenshot of each loading picture
		char name[32];
		sprintf(name, "loading%d.ppm", entry);
		dtu_write_screenshot(name);
	}
	return 1;
}

void dtufe_loading_picture_end(void)
{
	// 0x7C2BB fades out (0xD9EA4); the warp and the load then clear the
	// screen and load PALETTE.256.
	if (Script_n)
		gr_palette_clear();
	else
		gr_palette_fade_out(Loading_pal, 32, 0);
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	gr_clear_canvas(0);
	dtu_load_palette();
	gr_palette_load(gr_palette);
}

// The death sequence of dtuobj.c is at the death screen (0x423F5).
int dtu_hook_player_death_screen(void)
{
	if (!Fe_in_game)
		return 0;
	Fe_leave_game = 1;
	return 1;
}

// 0x5B3AC: a command line of DTU.WRP. Command 0 ("YPInn to Waterdeep")
// sets Function_mode 4 and the byte 0x13B254, and game() then asks "Leave
// the\nYawning Portal?" (text 795, 0x42639). The trigger code calls this
// in each frame while the player is in the segment: the port asks once
// each time the player enters it. The mission checks (commands 1 to 19)
// set the mission flags (0x13CDD4) that Khelben reads (dtuscript.c).
void dtu_hook_warp_command(int code, int value)
{
	if (code >= 1 && code <= 19)
		dtu_mission_check(code);
	if (code == 0 && Fe_in_game) {
		if (FrameCount != Fe_portal_frame + 1 && FrameCount != Fe_portal_frame)
			Fe_portal_exit = 1;
		Fe_portal_frame = FrameCount;
	}
}

int dtufe_game_frame(void)
{
	if (!Fe_in_game)
		return 0;
	dtustate_test_frame();
	if (Script_in_game && Script_n) {
		dtufe_menu_script_frame();
		if (Fe_quit)
			return 1;
	}
	if (Fe_portal_exit) {
		Fe_portal_exit = 0;
		// 0x5B3BE: command 0 saves the state of the inn before the question
		Fe_inn_flag = 1;
		dtustate_save_level(Current_level_num - 1);
		if (nm_messagebox(NULL, 2, fe_text(32), fe_text(33), fe_text(795)) == 0) {
			Dtu_fe_return_waterdeep = 1;
			Fe_leave_game = 1;
		}
	}
	if (Fe_escape_menu && !Fe_leave_game) {
		Fe_escape_menu = 0;
		escape_menu();
	}
	Fe_escape_menu = 0;
	if (Fe_graphics_menu && !Fe_leave_game)
		dtufe_graphics_menu();
	Fe_graphics_menu = 0;
	if (Fe_leave_game) {
		Fe_leave_game = 0;
		return 1;
	}
	return 0;
}

// After game(). The death of the player (0x423F5): the picture FEBGND 161
// with track 7, then the "Death Menu".
int dtufe_game_ended(void)
{
	int loaded = Fe_game_loaded;

	Fe_in_game = 0;
	Fe_leave_game = Fe_portal_exit = 0;
	Fe_inn_flag = Fe_escape_menu = Fe_graphics_menu = Fe_game_loaded = 0;
	if (Dtu_frames_done || Fe_quit || !plat_pump())
		return 0;
	// ASCENT: the test option -frames with no front end script: the
	// program ends with the game (the death screen would wait for a key).
	if (FindArg("-frames") && !Script_n)
		return 0;

	fe_enter();
	if (Dtu_player_dead) {
		fix until = fe_time() + 3 * F1_0;

		fe_town_reset();				// 0x82BC4: Khelben starts again
		Fe_have_character = 0;
		gr_palette_clear();
		fe_show_picture(161);
		fe_fade_in();
		if (dtumusic_current() != DTUMUSIC_DEATH)		// dtuobj.c may have started it
			dtumusic_play(DTUMUSIC_DEATH, 0, 100);
		Fe_cursor_bm = NULL;
		while (fe_frame_begin()) {
			fe_frame_end();
			if ((Fe_key || Fe_press) && fe_time() >= until - 2 * F1_0)
				break;
		}
		dtumusic_stop();
		Fe_mode = FE_GO_MENU;

		// 0x42554 to 0x42593: the screen goes black, the executable loads
		// PALETTE.256 and shows the "Death Menu" with the message box of
		// the game, nm_messagebox (0x655FE): no title, the choices "Load
		// Game" (0xBB780(1)) and "Abort Game", and the text "Death Menu".
		// The box has the stone background (febgnd 86) and the game fonts.
		// "Abort Game" asks "Abort Game?" (text 103). A load that is not
		// done and "No" show the menu again.
		while (!Fe_quit) {
			int c;
			gr_palette_clear();
			gr_set_current_canvas(NULL);
			dtu_load_palette();
			gr_clear_canvas(BM_XRGB(0,0,0));
			// The text in the executable is "Death Menu\n" (0x125735): the
			// line break gives an empty line under the text.
			c = nm_messagebox(NULL, 2, "Load Game", "Abort Game", "Death Menu\n");
			if (Fe_quit || !plat_pump())
				break;
			if (c == 0) {
				int mode = fe_load_game(1);
				if (mode) {
					Fe_mode = mode;
					break;
				}
			} else if (nm_messagebox(NULL, 2, fe_text(32), fe_text(33), fe_text(103)) == 0)
				break;
		}
	} else if (loaded)
		Fe_mode = loaded;			// a load of the ESC menu
	else if (Dtu_fe_return_waterdeep)
		Fe_mode = FE_GO_WATERDEEP;
	else
		Fe_mode = FE_GO_MENU;
	Dtu_fe_return_waterdeep = 0;
	return !Fe_quit;
}
