/*
 * ASCENT: new file. The conversation window of Descent to Undermountain
 * (dlgmenu.c of DTUGAME.EXE 1.3). Descent 1 has nothing like it.
 *
 * The facts are in docs/re/scripts.md, section 4. Addresses are linear
 * addresses of the image of DTUGAME.EXE 1.3 with object 1 at 0x10000.
 *
 * The executable draws the window into sub-canvases of the screen over the
 * 3D view: the text area on top and the reply area under it, with the
 * layout of the table at 0x1465F4 for the interface mode. The port draws
 * the same areas on the screen after the interface. Port decisions:
 * - The dark view: the executable ramps the value 0x14C4CE from 1 to 15
 *   when the window opens and back to 0 when it closes, one step for each
 *   0x800 of game time, and the texture mappers draw every lit texture in
 *   lavender grey with it (tmapfx.c). The executable waits for the end of the
 *   ramp before it shows the window and goes on; the port does not wait.
 * - The keys are those of the executable except Tab, which the interface
 *   uses: Up and Down (also the keypad), Enter (also the keypad), Space,
 *   Escape. The scroll arrows FEBGND 186 to 189 are not done. New in the
 *   port: an orange arrow right of the frame at the top (bottom) of the
 *   reply area while replies are out of the area above (below); a click
 *   on it selects the next reply in that direction.
 * - New in the port: the keys 1 to 9 (also the keypad digits) choose the
 *   reply with that number at once. A keypad digit with no reply of that
 *   number keeps its old use (8 Up, 2 Down). The play screen does not run
 *   its hot keys while the window is open.
 * - New in the port: the mouse. While the window waits for a choice, the
 *   cursor of the play screen shows over it (also in the immersive mode,
 *   which comes back after the conversation): the reply under the cursor
 *   becomes the selection when the cursor moves, a click chooses it (a
 *   click in the window with no replies is Enter), and the wheel scrolls
 *   the text area or moves the selection in the reply area. With the
 *   setting Dtu_fe_original_input the mouse does nothing here.
 * - Test options: -dlgauto LIST chooses the replies by itself, LIST is a
 *   comma separated list of reply numbers (from 1), "enter" or "esc", one
 *   item for each wait; -dlgdelay N is the number of frames that the window
 *   is shown before each such choice (default 15).
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "gr.h"
#include "key.h"
#include "args.h"
#include "gamefont.h"
#include "game.h"
#include "dtudisp.h"
#include "dtuscript.h"
#include "dtudlg.h"
#include "mouse.h"
#include "platform.h"		// plat_mouse_wheel
#include "dtufe.h"		// Dtu_fe_original_input
#include "dtucombat.h"	// dtu_combat_ignore_held_attack

#define DLG_MAX_REPLIES		10			// 0xA7B56
#define DLG_TEXT_SIZE		0x4000		// DLG_SEG_SIZE (0xB2099)
#define DLG_MAX_LINES		200

static int Dlg_open = 0;					// 0x145534
static char Dlg_text[DLG_TEXT_SIZE];		// the texts, each with a kind byte in front
static int Dlg_text_len = 0;
static char Dlg_reply[DLG_MAX_REPLIES][800];
static int Dlg_n_replies = 0;			// 0x1465AC
static int Dlg_waiting = 0;
static int Dlg_sel = -1;				// 0x3D4990: the selected reply, -1 none
static int Dlg_focus_replies = 1;		// 0x1466C4
static int Dlg_scroll = 0;				// first line of the text area
static int Dlg_wait_frames = 0;

// The dark view (0x14C4CE, 0x458BA): the value Dim_from at the game time
// Dim_time, going up (Dim_up) or down one step each 0x800
static int Dim_from = 0;
static int Dim_up = 0;
static fix Dim_time = 0;

// The mouse (see the top of the file)
static int Reply_y0[DLG_MAX_REPLIES], Reply_y1[DLG_MAX_REPLIES];	// the rows of each shown reply at the last draw
static int Reply_shown = 0;				// the replies that the last draw showed, from Dlg_first
// ASCENT: the arrows of the reply area at the last draw (dtu_dlg_draw)
static int Arrow_up, Arrow_down, Arrow_x0, Arrow_x1, Arrow_up_y, Arrow_down_y;

// The replies in the reply area: the first (0x3D498C) and the count
// (0x3D4984). A choice starts with none (0xB2280).
static int Dlg_first = 0;
static int Dlg_visible = 0;
static int Area_x0, Area_x1, Area_y0, Area_y1, Area_ry;	// the window at the last draw, ry: the reply area
static int Ptr_x, Ptr_y;				// the cursor position at the last poll
static int Ptr_forced = 0;				// the immersive mode was turned off for the window
static int Ptr_idle = 0;				// the frames with the window not waiting
static int Ptr_center = 0;				// ASCENT: center the cursor at the next poll (0xB2433)

// -dlgauto
static char Auto_list[512];
static char *Auto_next = NULL;
static int Auto_delay = 15;
static int Auto_init = 0;

static void auto_init(void)
{
	int t;

	if (Auto_init)
		return;
	Auto_init = 1;
	if ((t = FindArg("-dlgauto")) != 0 && t+1 < Num_args) {
		strncpy(Auto_list, Args[t+1], sizeof(Auto_list)-1);
		Auto_next = Auto_list;
	}
	if ((t = FindArg("-dlgdelay")) != 0 && t+1 < Num_args)
		Auto_delay = atoi(Args[t+1]);
}

// The layout of 0xB13C4 (tables 0x1465F4, 0x146624, 0x146654): x, y,
// width, text height, reply height, for low and high resolution.
typedef struct dlg_layout { int x, y, w, h1, h2; } dlg_layout;
static dlg_layout Layout_main[2] = { { 70, 12, 237, 62, 62 }, { 141, 26, 473, 123, 124 } };
static dlg_layout Layout_full[2] = { { 10, 10, 300, 90, 90 }, { 20, 20, 600, 220, 220 } };
static dlg_layout Layout_m1[2]   = { { 10, 10, 300, 62, 62 }, { 20, 20, 600, 124, 124 } };

static dlg_layout *layout(void)
{
	int hi = Dtu_hires ? 1 : 0;

	if (Dtu_interface_mode == DTU_IM_FULL_SCREEN)
		return &Layout_full[hi];
	if (Dtu_interface_mode == 1)
		return &Layout_m1[hi];
	return &Layout_main[hi];
}

// The cursor for a choice: the immersive mode goes off until the
// conversation ends (ptr_restore).
static void ptr_force(void)
{
	Ptr_idle = 0;
	if (Ptr_forced || !Dtu_immersive || !Dtu_display_active || Dtu_fe_original_input)
		return;
	Ptr_forced = 1;
	dtu_display_set_immersive(0);
	mouse_get_pos(&Ptr_x, &Ptr_y);
}

static void ptr_restore(void)
{
	// A press of the immersive mode key in the conversation turned it on
	// already.
	if (Ptr_forced && !Dtu_immersive)
		dtu_display_set_immersive(1);		// ASCENT: no jump of the view
	Ptr_forced = 0;
	// ASCENT: the button of the click that chose the last reply can still
	// be down, and in the immersive mode it is the attack button
	dtu_combat_ignore_held_attack();
}

int dtu_dlg_view_dim(void)
{
	fix t = GameTime - Dim_time;
	int steps, d;

	if (t < 0) {		// the game time went back (a loaded game)
		dtu_dlg_reset_view();
		if (Dlg_open) {
			Dim_from = 15;
			Dim_up = 1;
		}
		return Dim_from;
	}
	steps = t / 0x800;
	if (steps > 16)
		steps = 16;
	// 0x458BA(1): D + 1 at once, then one step each 0x800, up to 15
	// 0x458BA(0): D = 15, then 14 at once and one step each 0x800 down to 0
	d = Dim_up ? Dim_from + 1 + steps : Dim_from - 1 - steps;
	if (d > 15) d = 15;
	if (d < 0) d = 0;
	return d;
}

void dtu_dlg_reset_view(void)
{
	Dim_from = 0;
	Dim_up = 0;
	Dim_time = GameTime;
}

// The ramp of 0x458BA starts from the value of now (the port does not wait
// for its end: the window shows at once)
static void dim_ramp(int up)
{
	int d = dtu_dlg_view_dim();

	if (!up && d == 0 && !Dim_up)
		return;
	Dim_from = up ? (d > 14 ? 14 : d) : 15;
	if (!up && d < 15)
		Dim_from = d + 1;
	Dim_up = up;
	Dim_time = GameTime;
}

void dtu_dlg_open(void)
{
	auto_init();
	if (Dlg_open)
		return;
	dim_ramp(1);
	// 0xB14D8: a mode other than 0, 1 and 7 changes to mode 0
	if (Dtu_display_active && Dtu_interface_mode != DTU_IM_MAIN && Dtu_interface_mode != 1 &&
		 Dtu_interface_mode != DTU_IM_FULL_SCREEN)
		dtu_display_select_mode(DTU_IM_MAIN);
	Dlg_open = 1;
	Dlg_text_len = 0;
	Dlg_text[0] = 0;
	Dlg_n_replies = 0;
	Dlg_waiting = 0;
	Dlg_sel = -1;
	Dlg_scroll = 0;
}

void dtu_dlg_close(void)
{
	ptr_restore();
	if (Dlg_open)
		dim_ramp(0);
	Dlg_open = 0;
	Dlg_waiting = 0;
	Dlg_text_len = 0;
	Dlg_n_replies = 0;
}

int dtu_dlg_is_open(void)
{
	return Dlg_open;
}

// 0xB1F64: a text goes to its buffer (speech or narration). Each text
// starts on a new line.
void dtu_dlg_add_text(const char *text, int narration)
{
	int n = (int) strlen(text);

	if (!Dlg_open)
		dtu_dlg_open();
	if (n == 0)
		return;
	if (Dlg_text_len + n + 2 >= DLG_TEXT_SIZE)
		return;
	Dlg_text[Dlg_text_len++] = narration ? 2 : 1;
	memcpy(Dlg_text + Dlg_text_len, text, n);
	Dlg_text_len += n;
	Dlg_text[Dlg_text_len++] = 0;
	if (Dtu_script_log)
		printf("dlg: %s \"%s\"\n", narration ? "narration" : "speech", text);
}

void dtu_dlg_add_reply(const char *text)
{
	if (Dlg_n_replies >= DLG_MAX_REPLIES)
		return;
	strncpy(Dlg_reply[Dlg_n_replies], text, sizeof(Dlg_reply[0])-1);
	Dlg_reply[Dlg_n_replies][sizeof(Dlg_reply[0])-1] = 0;
	if (Dtu_script_log)
		printf("dlg: reply %d) \"%s\"\n", Dlg_n_replies+1, text);
	Dlg_n_replies++;
}

void dtu_dlg_start_choice(void)
{
	if (!Dlg_open)
		dtu_dlg_open();
	Dlg_waiting = 1;
	Dlg_sel = -1;
	Dlg_focus_replies = 1;
	Dlg_scroll = 0;
	Dlg_first = 0;
	Dlg_visible = 0;
	Dlg_wait_frames = 0;
	// ASCENT: a click from before the choice does not choose
	mouse_button_down_count(0);
	plat_mouse_wheel();
	mouse_get_pos(&Ptr_x, &Ptr_y);
	Ptr_center = 1;
}

int dtu_dlg_is_waiting(void)
{
	return Dlg_open && Dlg_waiting;
}

// ASCENT: the reply for a key 1 to 9 or a keypad digit, or -1
static int digit_reply(int key)
{
	static const int pad[9] = { KEY_PAD1, KEY_PAD2, KEY_PAD3, KEY_PAD4, KEY_PAD5,
		KEY_PAD6, KEY_PAD7, KEY_PAD8, KEY_PAD9 };
	int i, n = -1;

	if (key >= KEY_1 && key <= KEY_9)
		n = key - KEY_1;
	for (i = 0; i < 9; i++)
		if (key == pad[i])
			n = i;
	return n >= 0 && n < Dlg_n_replies ? n : -1;
}

static int chosen(int result);
static void keep_reply_visible(void);

// ASCENT: the mouse of the window. Returns a choice as dtu_dlg_poll does,
// or -1.
static int mouse_poll(void)
{
	int x, y, i, hit = -1, moved, click, wheel;

	if (Dtu_fe_original_input || Dtu_immersive)
		return -1;
	// ASCENT: the first poll of a choice puts the cursor in the middle of
	// the window (0xB2433: x + width / 2, y + height / 2, 0xE89A8). The
	// poller 0xB249C then keeps the cursor in the window; the port does not
	// hold the cursor of the desktop.
	if (Ptr_center && Area_x1 > Area_x0) {
		Ptr_center = 0;
		Ptr_x = (Area_x0 + Area_x1) / 2;
		Ptr_y = (Area_y0 + Area_y1) / 2;
		mouse_set_pos(Ptr_x, Ptr_y);
	}
	mouse_get_pos(&x, &y);
	moved = x != Ptr_x || y != Ptr_y;
	Ptr_x = x;
	Ptr_y = y;
	click = mouse_button_down_count(0) > 0;
	wheel = plat_mouse_wheel();
	// ASCENT: a click on an arrow of the reply area selects the next reply
	// out of the area (keep_reply_visible then moves the area)
	if (click && x >= Arrow_x0 && x <= Arrow_x1) {
		int h = Arrow_x1 - Arrow_x0 + 2;

		if (Arrow_up && y >= Arrow_up_y - 2 && y <= Arrow_up_y + h) {
			Dlg_sel = Dlg_first - 1;
			Dlg_focus_replies = 1;
			return -1;
		}
		if (Arrow_down && y >= Arrow_down_y - h && y <= Arrow_down_y + 2) {
			Dlg_sel = Dlg_first + Dlg_visible;
			Dlg_focus_replies = 1;
			return -1;
		}
	}
	if (x < Area_x0 || x > Area_x1 || y < Area_y0 || y >= Area_y1)
		return -1;
	for (i = 0; i < Reply_shown; i++)
		if (y >= Reply_y0[i] && y < Reply_y1[i])
			hit = Dlg_first + i;
	if (hit >= 0 && (moved || click)) {
		Dlg_sel = hit;
		Dlg_focus_replies = 1;
	}
	if (click) {
		if (Dlg_n_replies == 0)
			return chosen(0);
		if (hit >= 0)
			return chosen(hit);
	}
	if (wheel) {
		if (y < Area_ry) {
			Dlg_scroll -= wheel;
			if (Dlg_scroll < 0)
				Dlg_scroll = 0;
		} else if (Dlg_n_replies) {
			Dlg_sel -= wheel;
			if (Dlg_sel < 0)
				Dlg_sel = 0;
			if (Dlg_sel >= Dlg_n_replies)
				Dlg_sel = Dlg_n_replies - 1;
			Dlg_focus_replies = 1;
		}
	}
	return -1;
}

// 0xB23C6: the texts and the replies are cleared after a choice
static int chosen(int result)
{
	if (Dtu_script_log)
		printf("dlg: choice %d\n", result);
	Dlg_text_len = 0;
	Dlg_n_replies = 0;
	Dlg_waiting = 0;
	Dlg_sel = -1;
	Dlg_scroll = 0;
	return result;
}

static int auto_choice(void)
{
	char item[32];
	int n = 0;

	if (!Auto_next || !*Auto_next)
		return -1;
	// the selection is shown from the middle of the delay on
	if (Dlg_wait_frames >= Auto_delay / 2 && Dlg_n_replies && *Auto_next >= '1' && *Auto_next <= '9') {
		Dlg_sel = atoi(Auto_next) - 1;
		if (Dlg_sel >= Dlg_n_replies)
			Dlg_sel = 0;
	}
	if (Dlg_wait_frames < Auto_delay)
		return -1;
	while (*Auto_next && *Auto_next != ',' && n < (int) sizeof(item)-1)
		item[n++] = *Auto_next++;
	item[n] = 0;
	if (*Auto_next == ',')
		Auto_next++;
	if (!strcmp(item, "esc"))
		return -2;
	if (!strcmp(item, "enter") || Dlg_n_replies == 0)
		return 0;
	n = atoi(item) - 1;
	if (n < 0 || n >= Dlg_n_replies)
		n = 0;
	return n;
}

int dtu_dlg_poll(void)
{
	int key, r, replies, any = 0;

	if (!Dlg_waiting)
		return -1;
	Dlg_wait_frames++;
	if ((r = auto_choice()) != -1) {
		Dlg_sel = r;
		return chosen(r);
	}
	ptr_force();
	while ((key = key_inkey()) != 0) {
		// the input function of the frame (0xB28F0 with the focus on the
		// replies, 0xB2BED on the text, 0xB2B6F with no reply)
		replies = Dlg_focus_replies && Dlg_n_replies > 0;
		any = 1;
		// ASCENT: 1 to 9 choose a reply at once
		if ((r = digit_reply(key)) >= 0) {
			Dlg_sel = r;
			return chosen(r);
		}
		switch (key) {
		case KEY_ESC:
			// 0xB2D01: escape. With no reply the script goes on (0xA7ADC).
			return chosen(-2);
		case KEY_ENTER:
		case KEY_PADENTER:
			// 0xB2B6F: with no reply Enter goes on, but only with the
			// focus on the replies; with the focus on the text (0xB2BED)
			// it does nothing
			if (Dlg_n_replies == 0) {
				if (Dlg_focus_replies)
					return chosen(0);
				break;
			}
			// 0xB2A5E: the selection. It is -1 only before the first
			// frame of the choice; the window closes with it and 0xA7ADC
			// ends the run as for Escape. The port returns -2 for it,
			// since -1 here is "no choice yet".
			if (Dlg_focus_replies)
				return chosen(Dlg_sel >= 0 ? Dlg_sel : -2);
			break;
		case KEY_SPACEBAR:
		case KEY_TAB:
			// 0xB2A47, 0xB2BB5 (no reply), 0xB2CEB: Space and Tab move the
			// focus between the replies and the text, also with no reply
			// (then only Enter goes on)
			Dlg_focus_replies = !Dlg_focus_replies;
			break;
		case KEY_LAPOSTRO:
			save_screen_shot(0);						// 0xB297E: 0x80DA1
			break;
		case KEY_UP:
		case KEY_PAD8:
			if (Dlg_focus_replies && Dlg_n_replies) {
				if (--Dlg_sel < 0)						// 0xB2988
					Dlg_sel = 0;
			} else if (!Dlg_focus_replies && Dlg_scroll > 0)	// 0xB2BED; 0xB2B6F: nothing
				Dlg_scroll--;
			break;
		case KEY_DOWN:
		case KEY_PAD2:
			if (Dlg_focus_replies && Dlg_n_replies) {
				if (++Dlg_sel >= Dlg_n_replies)		// 0xB2A1F
					Dlg_sel = Dlg_n_replies - 1;
			} else if (!Dlg_focus_replies)				// 0xB2BED; 0xB2B6F: nothing
				Dlg_scroll++;
			break;
		}
		if (replies)
			keep_reply_visible();
	}
	// 0xB28F0 in a frame with no key: also with the selection -1, which
	// becomes the first reply
	if (!any && Dlg_focus_replies && Dlg_n_replies > 0)
		keep_reply_visible();
	r = mouse_poll();
	// ASCENT: the wheel can move the selection out of the reply area
	if (r == -1 && Dlg_focus_replies && Dlg_n_replies > 0)
		keep_reply_visible();
	return r;
}

//------------------------------------------------------------------------
// Drawing

static int rgb6(int r, int g, int b)
{
	return gr_find_closest_color(r, g, b);
}

// Splits a text into lines of at most "width" pixels. Returns the number
// of lines; starts[i] and lens[i] give each line.
static int wrap(const char *s, int width, const char **starts, int *lens, int max)
{
	int n = 0;

	while (*s && n < max) {
		const char *p = s, *last_space = NULL;
		char buf[800];
		int len = 0, w, h, aw;

		while (*p && *p != '\n') {
			if (len >= (int) sizeof(buf)-1)
				break;
			buf[len] = *p;
			buf[len+1] = 0;
			gr_get_string_size(buf, &w, &h, &aw);
			if (w > width && len > 0) {
				if (last_space)
					p = last_space;
				break;
			}
			if (*p == ' ')
				last_space = p;
			len++;
			p++;
		}
		starts[n] = s;
		lens[n] = (int) (p - s);
		n++;
		if (*p == ' ' || *p == '\n')
			p++;
		s = p;
	}
	return n;
}

static void draw_line(int x, int y, const char *s, int len)
{
	char buf[800];

	if (len > (int) sizeof(buf)-1)
		len = sizeof(buf)-1;
	memcpy(buf, s, len);
	buf[len] = 0;
	gr_string(x, y, buf);
}

// ASCENT: a filled triangle of height half + 1 with its tip at (x, y),
// pointing up (dir 1) or down (dir -1)
static void arrow(int x, int y, int half, int dir, int color)
{
	int k;

	gr_setcolor(color);
	for (k = 0; k <= half; k++)
		gr_rect(x - k, y + dir * k, x + k, y + dir * k);
}

static void frame_rect(int x0, int y0, int x1, int y1, int color)
{
	gr_setcolor(color);
	gr_rect(x0, y0, x1, y0);
	gr_rect(x0, y1, x1, y1);
	gr_rect(x0, y0, x0, y1);
	gr_rect(x1, y0, x1, y1);
}

// 0xB1B9E: an area of height h with the margin m (0x1466AC: 10 low, 16
// high resolution) holds (h - 2m) / ft_h lines. The lines are centered in
// the height without the margins. Returns the number of lines; *top is the
// first row of the lines from the top of the area.
static int area_lines(int h, int m, int fh, int *top)
{
	int wh = h - 2 * m, n;

	n = (wh > 0 && fh > 0) ? wh / fh : 0;
	*top = m + (wh - n * fh) / 2;
	return n;
}

// 0xB170B: the column of the numbers is as wide as the widest "%d) "
// of the MaxDialogResponses (0x1465A8: 8) numbers. The text column
// starts 10 pixels after it and ends at the right edge (0xB1778).
// Returns the width of the number column. The font is the current one.
static int number_width(void)
{
	int num_w = 0, n_num = Dlg_n_replies > 8 ? Dlg_n_replies : 8, i, h, aw;

	for (i = 1; i <= n_num; i++) {
		char num[16];
		int w;
		sprintf(num, "%d) ", i);
		gr_get_string_size(num, &w, &h, &aw);
		if (w > num_w)
			num_w = w;
	}
	return num_w;
}

// The lines of a reply in the text column of width text_w
static int reply_lines(int i, int text_w)
{
	const char *rs[20];
	int rl[20];

	return wrap(Dlg_reply[i], text_w, rs, rl, 20);
}

// Adds the replies after (direction 1) or before (-1) the selection while
// they fit: each reply takes its lines and one empty line, and a reply
// that fills the rest of the area exactly fits.
static void extend_visible(const int *lines, int rows, int direction, int *used)
{
	int i;

	for (i = Dlg_sel + direction; i >= 0 && i < Dlg_n_replies; i += direction) {
		if (*used + lines[i] > rows)
			break;
		*used += lines[i] + 1;
		Dlg_visible++;
		if (direction < 0)
			Dlg_first = i;
	}
}

// The end of 0xB28F0: when the selection is above the replies of the area
// (also -1, which becomes 0), the area starts at the selection and takes
// the later replies, then the earlier ones with the rest of the rows
// (0xB29A1). When it is below, the area takes the earlier replies first
// (0xB2ABC). The selection always has its place, also when it is higher
// than the area.
static void keep_reply_visible(void)
{
	dlg_layout *L = layout();
	grs_font *old;
	int lines[DLG_MAX_REPLIES], text_w, rows, top, used, direction, i;

	if (Dlg_n_replies <= 0 || !GAME_FONT || !grd_curcanv)
		return;
	if (Dlg_sel < Dlg_first) {
		if (Dlg_sel < 0)
			Dlg_sel = 0;
		direction = 1;
	} else if (Dlg_sel >= Dlg_first + Dlg_visible)
		direction = -1;
	else
		return;

	old = grd_curcanv->cv_font;
	gr_set_curfont(GAME_FONT);
	text_w = L->w - number_width() - 10;
	for (i = 0; i < Dlg_n_replies; i++)
		lines[i] = reply_lines(i, text_w);
	grd_curcanv->cv_font = old;
	rows = area_lines(L->h2, Dtu_hires ? 16 : 10, GAME_FONT->ft_h, &top);

	Dlg_first = Dlg_sel;
	Dlg_visible = 1;
	used = lines[Dlg_sel] + 1;
	extend_visible(lines, rows, direction, &used);
	extend_visible(lines, rows, -direction, &used);
}

// The executable draws each part into a sub-canvas of the screen, which
// clips the text (0xB17EB to 0xB1891).
static void sub_canvas(grs_canvas *c, int x, int y, int w, int h)
{
	gr_init_sub_canvas(c, &VR_screen_pages[0], x, y, w > 0 ? w : 1, h > 0 ? h : 1);
	gr_set_current_canvas(c);
	gr_set_curfont(GAME_FONT);
}

void dtu_dlg_draw(void)
{
	dlg_layout *L;
	const char *starts[DLG_MAX_LINES];
	int lens[DLG_MAX_LINES], kinds[DLG_MAX_LINES];
	int n_lines = 0, fh, i, y, visible, num_w, text_x, text_w, h, aw;
	int c_speech, c_narr, c_sel, c_reply, c_frame;
	int ry, rbottom, margin, ty, rty, rh;
	grs_canvas text_canv, num_canv, reply_canv, area_canv;

	// ASCENT: the immersive mode comes back when the window has not
	// waited for a choice for half a second (dtu_dlg_close restores it at
	// once)
	if (Ptr_forced && !(Dlg_open && Dlg_waiting) && ++Ptr_idle > 15)
		ptr_restore();
	if (!Dlg_open || !GAME_FONT || !grd_curscreen)
		return;
	L = layout();
	gr_set_current_canvas(&VR_screen_pages[0]);
	gr_set_curfont(GAME_FONT);
	fh = GAME_FONT->ft_h;
	margin = Dtu_hires ? 16 : 10;

	// 0xB19A2 to 0xB19F1
	c_speech = rgb6(0, 63, 0);
	c_narr = rgb6(63, 63, 0);
	c_sel = rgb6(63, 32, 0);
	c_reply = rgb6(32, 32, 32);
	c_frame = rgb6(63, 0, 63);

	num_w = number_width();
	text_w = L->w - num_w - 10;
	text_x = L->x + L->w - text_w;

	// the text area: the lines from the scroll position, in a canvas of
	// whole lines in the middle of the area (0xB2D0D)
	{
		int pos = 0;

		while (pos < Dlg_text_len && n_lines < DLG_MAX_LINES) {
			int kind = Dlg_text[pos++];
			const char *s = Dlg_text + pos;
			int k, n = wrap(s, L->w, starts + n_lines, lens + n_lines, DLG_MAX_LINES - n_lines);

			for (k = 0; k < n; k++)
				kinds[n_lines + k] = kind;
			n_lines += n;
			pos += (int) strlen(s) + 1;
		}
	}
	visible = area_lines(L->h1, margin, fh, &ty);
	if (Dlg_scroll > n_lines - visible)
		Dlg_scroll = n_lines - visible > 0 ? n_lines - visible : 0;
	sub_canvas(&text_canv, L->x, L->y + ty, L->w, visible * fh);
	for (i = Dlg_scroll, y = 0; i < n_lines && y < visible * fh; i++, y += fh) {
		gr_set_fontcolor(kinds[i] == 2 ? c_narr : c_speech, -1);
		draw_line(0, y, starts[i], lens[i]);
	}

	// the reply area
	ry = L->y + L->h1;
	rbottom = ry + L->h2;
	rh = area_lines(L->h2, margin, fh, &rty) * fh;
	rty += ry;
	Area_x0 = L->x - 3;
	Area_x1 = L->x + L->w + 3;
	Area_y0 = L->y;
	Area_y1 = rbottom;
	Area_ry = ry;
	Reply_shown = 0;
	if (Dlg_waiting && Dlg_n_replies == 0) {
		// 0xB2E91: centered in the canvas of the whole reply area
		int w;
		sub_canvas(&area_canv, L->x, rty, L->w, rh);
		gr_set_fontcolor(c_sel, -1);
		gr_get_string_size(LT("Press Enter..."), &w, &h, &aw);
		gr_string((L->w - w) / 2, (rh - fh) / 2, LT("Press Enter..."));
	} else {
		// 0xB2D90: the number "%d)" in the number column and the reply in
		// the text column at the same row; the next reply starts one empty
		// line (ft_h) after the last line of the reply. The replies are
		// those of the area (Dlg_first, Dlg_visible), see
		// keep_reply_visible.
		sub_canvas(&num_canv, L->x, rty, num_w, rh);
		sub_canvas(&reply_canv, text_x, rty, text_w, rh);
		y = 0;
		for (i = Dlg_first; i < Dlg_first + Dlg_visible && i < Dlg_n_replies; i++) {
			char num[16];
			const char *rs[20];
			int rl[20], k, n;

			sprintf(num, "%d)", i + 1);
			gr_set_current_canvas(&num_canv);
			gr_set_fontcolor(i == Dlg_sel ? c_sel : c_reply, -1);
			gr_string(0, y, num);
			gr_set_current_canvas(&reply_canv);
			gr_set_fontcolor(i == Dlg_sel ? c_sel : c_reply, -1);
			n = wrap(Dlg_reply[i], text_w, rs, rl, 20);
			for (k = 0; k < n; k++)
				draw_line(0, y + k * fh, rs[k], rl[k]);
			Reply_y0[i - Dlg_first] = rty + y;
			Reply_y1[i - Dlg_first] = rty + y + n * fh;
			Reply_shown = i - Dlg_first + 1;
			y += n * fh + fh;
		}
	}
	gr_set_current_canvas(&VR_screen_pages[0]);

	// 0xB2EDD: the frame around the active area, the lines at the top and
	// the bottom row of the area
	if (Dlg_focus_replies)
		frame_rect(L->x - 3, ry, L->x + L->w + 3, rbottom, c_frame);
	else
		frame_rect(L->x - 3, L->y, L->x + L->w + 3, ry, c_frame);

	// ASCENT: the arrows of the reply area, right of the frame, from what
	// this draw showed: up while the first reply is not shown, down while
	// the last reply is not shown whole (a selection taller than the area
	// is cut). Nothing before the first poll of a choice sets the area.
	Arrow_up = Arrow_down = 0;
	if (Dlg_waiting && Dlg_n_replies > 0 && Reply_shown > 0) {
		int half = Dtu_hires ? 5 : 3, ax = L->x + L->w + 4 + half + 1;

		Arrow_x0 = ax - half - 1;
		Arrow_x1 = ax + half + 1;
		Arrow_up_y = ry + 2;
		Arrow_down_y = rbottom - 2;
		if (Dlg_first > 0) {
			Arrow_up = 1;
			arrow(ax, Arrow_up_y, half, 1, c_sel);
		}
		if (Dlg_first + Reply_shown < Dlg_n_replies || Reply_y1[Reply_shown - 1] > rty + rh) {
			Arrow_down = 1;
			arrow(ax, Arrow_down_y, half, -1, c_sel);
		}
	}

	// ASCENT: the cursor over the window (dtu_display_frame leaves it out
	// while the window is open)
	dtu_display_draw_cursor();
}
