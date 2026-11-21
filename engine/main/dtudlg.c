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
 * - The area of the window is darkened (the color table of the lighting at
 *   half light). The executable ramps a shading value 0x14C4CE that the
 *   renderer uses; how the view looks with it was not seen.
 * - The keys are those of the executable except Tab, which the interface
 *   uses: Up and Down (also the keypad), Enter (also the keypad), Space,
 *   Escape. The mouse and the scroll arrows FEBGND 186 to 189 are not done.
 * - Test options: -dlgauto LIST chooses the replies by itself, LIST is a
 *   comma separated list of reply numbers (from 1), "enter" or "esc", one
 *   item for each wait; -dlgdelay N is the number of frames that the window
 *   is shown before each such choice (default 15).
 */

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

void dtu_dlg_open(void)
{
	auto_init();
	if (Dlg_open)
		return;
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
	Dlg_wait_frames = 0;
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
	int key, r;

	if (!Dlg_waiting)
		return -1;
	Dlg_wait_frames++;
	if ((r = auto_choice()) != -1) {
		Dlg_sel = r;
		return chosen(r);
	}
	while ((key = key_inkey()) != 0) {
		switch (key) {
		case KEY_ESC:
			// 0xB2D01: escape. With no reply the script goes on (0xA7ADC).
			return chosen(-2);
		case KEY_ENTER:
		case KEY_PADENTER:
			if (Dlg_n_replies == 0)
				return chosen(0);
			// 0xB2A5E: the selection; -1 is no choice yet
			if (Dlg_focus_replies && Dlg_sel >= 0)
				return chosen(Dlg_sel);
			break;
		case KEY_SPACEBAR:
			if (Dlg_n_replies == 0)
				return chosen(0);
			Dlg_focus_replies = !Dlg_focus_replies;			// 0xB2A47, 0xB2CEB
			break;
		case KEY_UP:
		case KEY_PAD8:
			if (Dlg_focus_replies && Dlg_n_replies) {
				if (--Dlg_sel < 0)						// 0xB2988
					Dlg_sel = 0;
			} else if (Dlg_scroll > 0)
				Dlg_scroll--;
			break;
		case KEY_DOWN:
		case KEY_PAD2:
			if (Dlg_focus_replies && Dlg_n_replies) {
				if (++Dlg_sel >= Dlg_n_replies)		// 0xB2A1F
					Dlg_sel = Dlg_n_replies - 1;
			} else
				Dlg_scroll++;
			break;
		}
	}
	return -1;
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

// Half light for the pixels of a rectangle of the screen
static void darken(int x0, int y0, int x1, int y1)
{
	grs_bitmap *bm = &grd_curcanv->cv_bitmap;
	int x, y;

	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > bm->bm_w) x1 = bm->bm_w;
	if (y1 > bm->bm_h) y1 = bm->bm_h;
	for (y = y0; y < y1; y++) {
		ubyte *p = bm->bm_data + y * bm->bm_rowsize + x0;
		for (x = x0; x < x1; x++, p++)
			*p = gr_fade_table[(GR_FADE_LEVELS/3) * 256 + *p];
	}
}

static void frame_rect(int x0, int y0, int x1, int y1, int color)
{
	gr_setcolor(color);
	gr_rect(x0, y0, x1, y0);
	gr_rect(x0, y1, x1, y1);
	gr_rect(x0, y0, x0, y1);
	gr_rect(x1, y0, x1, y1);
}

void dtu_dlg_draw(void)
{
	dlg_layout *L;
	const char *starts[DLG_MAX_LINES];
	int lens[DLG_MAX_LINES], kinds[DLG_MAX_LINES];
	int n_lines = 0, line_h, i, y, visible, num_w, h, aw;
	int c_speech, c_narr, c_sel, c_reply, c_frame;
	int ry, rbottom;

	if (!Dlg_open || !GAME_FONT || !grd_curscreen)
		return;
	L = layout();
	gr_set_current_canvas(&VR_screen_pages[0]);
	gr_set_curfont(GAME_FONT);
	line_h = GAME_FONT->ft_h + 1;

	// 0xB19A2 to 0xB19F1
	c_speech = rgb6(0, 63, 0);
	c_narr = rgb6(63, 63, 0);
	c_sel = rgb6(63, 32, 0);
	c_reply = rgb6(32, 32, 32);
	c_frame = rgb6(63, 0, 63);

	darken(L->x - 3, L->y, L->x + L->w + 4, L->y + L->h1 + L->h2);

	// the text area
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
	visible = L->h1 / line_h;
	if (Dlg_scroll > n_lines - visible)
		Dlg_scroll = n_lines - visible > 0 ? n_lines - visible : 0;
	for (i = Dlg_scroll, y = L->y; i < n_lines && y + line_h <= L->y + L->h1; i++, y += line_h) {
		gr_set_fontcolor(kinds[i] == 2 ? c_narr : c_speech, -1);
		draw_line(L->x, y, starts[i], lens[i]);
	}

	// the reply area
	ry = L->y + L->h1;
	rbottom = ry + L->h2;
	if (Dlg_waiting && Dlg_n_replies == 0) {
		int w;
		gr_set_fontcolor(c_sel, -1);
		gr_get_string_size("Press Enter...", &w, &h, &aw);
		gr_string(L->x + (L->w - w) / 2, ry + (L->h2 - line_h) / 2, "Press Enter...");
	} else {
		gr_get_string_size("10) ", &num_w, &h, &aw);
		y = ry;
		for (i = 0; i < Dlg_n_replies && y + line_h <= rbottom; i++) {
			char num[16];
			const char *rs[20];
			int rl[20], k, n;

			sprintf(num, "%d)", i + 1);
			gr_set_fontcolor(i == Dlg_sel ? c_sel : c_reply, -1);
			gr_string(L->x, y, num);
			n = wrap(Dlg_reply[i], L->w - num_w, rs, rl, 20);
			for (k = 0; k < n && y + line_h <= rbottom; k++, y += line_h)
				draw_line(L->x + num_w, y, rs[k], rl[k]);
		}
	}

	// 0xB2EDD: the frame around the active area
	if (Dlg_focus_replies)
		frame_rect(L->x - 3, ry, L->x + L->w + 3, rbottom - 1, c_frame);
	else
		frame_rect(L->x - 3, L->y, L->x + L->w + 3, ry - 1, c_frame);
}
