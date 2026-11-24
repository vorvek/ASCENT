/*
 * ASCENT: new file, split from dtufe.c. The script of the option
 * -fescript FILE: the test commands that drive the front end and the
 * menus of the game. The shared declarations are in dtufe_int.h. See
 * docs/re/frontend.md.
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
#include "dtulangart.h"		// ASCENT: the pictures of the language
#include "dtumusic.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"		// ASCENT: the slots at the game start (0x141C38)
#include "dtusoak.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "gameseq.h"
#include "newmenu.h"
#include "menu.h"			// ASCENT: Player_default_difficulty
#include "dtutrig.h"
#include "gamefont.h"
#include "automap.h"		// ASCENT: the command "mapstate"
#include "dtufe.h"
#include "dtufe_int.h"
#include "dtumovie.h"
#include "dtustate.h"
#include "fuelcen.h"		// ASCENT: the lists of a save game
#include "switch.h"
#include "dtuscript.h"
#include "dtudlg.h"		// ASCENT: dtu_dlg_reset_view
#include "dtuscroll.h"		// ASCENT: dtu_scroll_reset
#include "dtudesc.h"		// ASCENT: dtu_desc_reset
#include "screens.h"
#include "dtuopts.h"
#include "dtuitemfix.h"	// ASCENT: Dtu_rulebook_items
#include "dtuxp.h"		// ASCENT: Dtu_max_hp_level_up
#include "dtuhdui.h"		// ASCENT: the upscaled pictures
#include "dtulang.h"		// ASCENT: the language of the text
#include "dtulog.h"		// ASCENT: the session log

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
//   winmove X Y     (ASCENT) a mouse motion at X, Y in window coordinates,
//                   mapped to the frame by plat_pump as a system event
//   winclick X Y    (ASCENT) the same and a left click (released in the
//                   next frame)
//   presentstate    (ASCENT) print the window rectangle of the frame
//   viewstate, mousestate  (ASCENT) print the view projection, the mouse
//                   mode and the mouse position
// ASCENT: a key name can start with "ctrl-" (Ctrl and the key), and the
// names tab, home, end, ins, del, lctrl, lshift, lalt, f11, f12 exist.
// While the script has commands the clock of the front end is the frame
// count (30 frames are one second), so a run always gives the same frames.

#define SCRIPT_MAX	400

static char *Script_line[SCRIPT_MAX];
int Script_n, Script_pos, Script_wait;
int Script_btn, Script_rbtn;
static char Script_text[80];
static int Script_text_pos;
char *Script_shot;
int Script_in_game;			// the command "gamekeys" was run
static int Script_hold_key, Script_hold_frames;	// ASCENT: the command "hold"
static int Script_moved;			// ASCENT: a command put the cursor at Fe_mx, Fe_my
static float Script_win_x, Script_win_y;	// ASCENT: winmove and winclick (window coordinates)
static int Script_win_up;			// ASCENT: release the button of a winclick

int script_active(void)
{
	return Script_pos < Script_n || Script_wait > 0 || Script_btn || Script_rbtn || Script_text[Script_text_pos];
}

// ASCENT: the lines of the script are freed at the exit.
static void script_free(void)
{
	int i;

	for (i = 0; i < Script_n; i++)
		if (Script_line[i])
			free(Script_line[i]);
	Script_n = Script_pos = 0;
}

void script_load(void)
{
	FILE *f;
	char line[200];
	int t = FindArg("-fescript");
	static int loaded;

	// ASCENT: called by fe_enter and by the direct start (-level)
	if (loaded || !t || t + 1 >= Num_args)
		return;
	loaded = 1;
	atexit(script_free);
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
void script_frame(void)
{
	Fe_key = 0;
	Script_moved = 0;
	if (Script_win_up) {		// ASCENT: the frame after a winclick: release
		plat_inject_window_mouse(Script_win_x, Script_win_y, 0, 0);
		Script_win_up = 0;
		return;
	}
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
		} else if (!stricmp(cmd, "rel")) {
			// ASCENT: a relative mouse movement (the look of the immersive mode)
			plat_input_mouse_motion(x, y, Fe_mx, Fe_my);
			return;
		} else if (!stricmp(cmd, "mapstate")) {
			dtu_automap_print_state();		// ASCENT: for tools/uitest.py
			return;
		} else if (!stricmp(cmd, "viewstate")) {
			dtu_display_print_view();		// ASCENT: for tools/uitest.py
			return;
		} else if (!stricmp(cmd, "winmove") || !stricmp(cmd, "winclick")) {
			// ASCENT: for tools/uitest.py: a mouse event in window
			// coordinates, which the platform maps to the frame as an
			// event of the system (the test of the mouse with the
			// shape of the frame, plat_bind_vga_aspect)
			Script_win_x = Script_win_y = 0;
			sscanf(arg, "%f %f", &Script_win_x, &Script_win_y);
			plat_inject_window_mouse(Script_win_x, Script_win_y, -1, 0);
			if (!stricmp(cmd, "winclick")) {
				plat_inject_window_mouse(Script_win_x, Script_win_y, 0, 1);
				Script_win_up = 1;
			}
			return;
		} else if (!stricmp(cmd, "presentstate")) {
			// ASCENT: for tools/uitest.py: the rectangle of the window
			// that shows the frame
			float px, py, pw, ph;
			int fw, fh, ww, wh;
			plat_get_present_rect(&px, &py, &pw, &ph, &fw, &fh, &ww, &wh);
			printf("Presentstate window=%dx%d frame=%dx%d rect=%.1f,%.1f,%.1fx%.1f\n", ww, wh, fw, fh, px, py, pw, ph);
			fflush(stdout);
			return;
		} else if (!stricmp(cmd, "mousestate")) {
			// ASCENT: for tools/uitest.py
			int mx = 0, my = 0;
			mouse_get_pos(&mx, &my);
			printf("Mousestate relative=%d immersive=%d x=%d y=%d menu=%d\n", plat_mouse_get_relative(), Dtu_immersive, mx, my,
				nm_pointer_held());
			fflush(stdout);
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
