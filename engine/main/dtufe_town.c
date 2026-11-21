/*
 * ASCENT: new file. The Waterdeep screens of the front end of Descent to
 * Undermountain: the city (fewdeep.c of DTUGAME.EXE), the tower of Khelben
 * Blackstaff (frontend.c, fetorch.c) and the market (femarket.c). Descent 1
 * has nothing like them. See docs/re/frontend.md.
 *
 * Not done in the port: the torches and the eyes of the Blackstaff screen,
 * the change of the mission by the quest flags and the rewards of Khelben,
 * and all trade of the market (the screen opens and closes).
 *
 * An address in a comment is an address of DTUGAME.EXE 1.3.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "key.h"
#include "mem.h"
#include "text.h"
#include "dtufile.h"
#include "dtumusic.h"
#include "dtufe.h"
#include "dtufe_int.h"

int Fe_khelben_mission;			// MsnNum (0x13E588)
int Fe_khelben_blurb;			// 0x13E58C
static int Fe_khelben_visits;	// 0x13E396

void fe_town_reset(void)			// 0x82BC4
{
	Fe_khelben_mission = Fe_khelben_blurb = Fe_khelben_visits = 0;
}

//------------------------------------------------------------------------
// Waterdeep (0x8C206, screen record 0x14202D)

static int Wd_result;

static void wd_go(fe_screen *s, int id)
{
	Wd_result = id;
	s->running = 0;
}

static fe_control Wd_controls[5] = {
	{ 37, 213, "btnwdypt" }, { 293, 127, "btnwdbst" }, { 218, 190, "btnwdmkt" },
	{ 38, 24, "btnwdprg" }, { 492, 271, "btnwdcg" } };

// 0x141D48: all places
// 0x8C4C4 (Alt-F2): 0xBA99F(1), the save. 0x8C4D3 (Alt-F3): 0xBB780(0), the
// load; Waterdeep is left when the load changed Function_mode.
static void wd_save(fe_screen *s, int id)
{
	fe_save_game();
}

static void wd_load(fe_screen *s, int id)
{
	int mode = fe_load_game(0);

	if (mode) {
		Wd_result = mode;
		s->running = 0;
	}
}

static fe_feature Wd_features[6] = {
	{ 275, 120, 334, 189, 0, 0, 0, 1, 0, KEY_B, KEY_K, 566, NULL, NULL, wd_go, FE_GO_BLACKSTAFF, 0 },
	{ 10, 180, 90, 260, 0, 0, 0, 0, 0, KEY_Y, KEY_P, 567, NULL, NULL, wd_go, FE_GO_PORTAL, 0 },
	{ 215, 190, 304, 264, 0, 0, 0, 2, 0, KEY_M, KEY_4, 568, NULL, NULL, wd_go, FE_GO_MARKET, 0 },
	{ 492, 271, 635, 345, 0, 0, 0, 4, 0, KEY_X, KEY_ESC, 569, NULL, NULL, wd_go, FE_GO_MENU, 0 },
	{ 800, 600, 801, 601, 0, 0, 0, 4, 0, KEY_ALTED | KEY_F2, 0, 569, NULL, NULL, wd_save, 0, 0 },
	{ 800, 600, 801, 601, 0, 0, 0, 4, 0, KEY_ALTED | KEY_F3, 0, 569, NULL, NULL, wd_load, 0, 0 } };

// 0x141F10: before the first visit of Khelben
static fe_feature Wd_first_features[2] = {
	{ 250, 90, 375, 230, 0, 0, 0, 1, 0, KEY_B, KEY_K, 566, NULL, NULL, wd_go, FE_GO_BLACKSTAFF, 0 },
	{ 492, 271, 635, 345, 0, 0, 0, 4, 0, KEY_X, KEY_ESC, 569, NULL, NULL, wd_go, FE_GO_MENU, 0 } };

static fe_screen Wd = { Wd_controls, 5, { { 0, -1, -1 }, { 0, -1, -1 } }, Wd_features, 4,
	136, 375, 501, 400, FE_SCR_WATERDEEP, -1, NULL, NULL, NULL, NULL, 0 };

int fe_waterdeep(void)
{
	if (dtumusic_current() != 0)
		dtumusic_stop();
	dtumusic_play(0, DTUMUSIC_LOOP_FOREVER, 100);		// overview.tun

	// "Visit Khelben first": the short list is used while the mission
	// number and the blurb number are 0.
	if (Fe_khelben_mission || Fe_khelben_blurb) {
		Wd.features = Wd_features;
		Wd.n_features = 6;
	} else {
		Wd.features = Wd_first_features;
		Wd.n_features = 2;
	}
	Wd_result = FE_GO_MENU;
	fe_run_screen(&Wd);
	if (Wd_result != FE_GO_MENU || Fe_quit)
		dtumusic_stop();		// 0xA48D0
	return Wd_result;
}

//------------------------------------------------------------------------
// Blackstaff tower (0x82B31, screen record 0x13EAF3)

// 0x142840: first FETXT entry and number of entries of each mission
static short Blurb_table[34][2] = {
	{311,10}, {321,8}, {329,2}, {331,10}, {341,10}, {351,10}, {361,10}, {371,2}, {373,2}, {375,5},
	{380,2}, {382,2}, {384,5}, {389,5}, {394,3}, {397,5}, {402,2}, {404,2}, {406,5}, {411,5},
	{416,5}, {421,2}, {423,5}, {428,5}, {433,2}, {435,7}, {442,7}, {449,6}, {455,6}, {461,6},
	{467,6}, {473,6}, {479,6}, {485,2} };

#define BS_TEXT_X		150
#define BS_TEXT_Y		30
#define BS_TEXT_W		375
#define BS_LINES		7
#define BS_MAX_LINES	200
#define BS_MAX_EVENTS	200

typedef struct bs_event {
	int line;
	int type;			// 's' speech, 'p' picture, 'e' erase the picture
	int value;
} bs_event;

static char Bs_line[BS_MAX_LINES][96];
static ubyte Bs_line_hue[BS_MAX_LINES];
static int Bs_n_lines;
static bs_event Bs_event[BS_MAX_EVENTS];
static int Bs_n_events, Bs_next_event;
static grs_bitmap *Bs_face, *Bs_picture;
static int Bs_frame, Bs_prev_frame;
static fix Bs_start, Bs_face_time, Bs_end_time;
static int Bs_state;			// 0: wait 2 seconds, 1: the text moves, 2: wait 10 seconds, 3: done

// The markup of the text (0x8399D): ~cN color, ~sN speech, ~pN picture,
// ~e erase the picture, ~n line break, ~mN table of the mouth pictures.
static void bs_parse(char *text)
{
	char word[96];
	int hue = 7, w, h, n;
	char *p = text;

	Bs_n_lines = 1;
	Bs_n_events = 0;
	Bs_line[0][0] = 0;
	Bs_line_hue[0] = (ubyte) hue;
	while (*p) {
		while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')
			p++;
		for (n = 0; *p && *p != ' ' && *p != '\n' && *p != '\r' && *p != '\t' && n < 90; p++)
			word[n++] = *p;
		word[n] = 0;
		if (!n)
			break;
		if (word[0] == '~') {
			int v = atoi(word + 2);
			switch (word[1]) {
			case 'c':
				hue = v % 11;
				if (!Bs_line[Bs_n_lines - 1][0])
					Bs_line_hue[Bs_n_lines - 1] = (ubyte) hue;
				break;
			case 'n':
				if (Bs_n_lines < BS_MAX_LINES) {
					Bs_line[Bs_n_lines][0] = 0;
					Bs_line_hue[Bs_n_lines++] = (ubyte) hue;
				}
				break;
			case 's': case 'p': case 'e':
				if (Bs_n_events < BS_MAX_EVENTS) {
					Bs_event[Bs_n_events].line = Bs_n_lines - 1;
					Bs_event[Bs_n_events].type = word[1];
					Bs_event[Bs_n_events++].value = v;
				}
				break;
			}
			continue;
		}
		{
			char *line = Bs_line[Bs_n_lines - 1], test[200];
			sprintf(test, "%s%s%s", line, line[0] ? " " : "", word);
			fe_string_size(Fe_font_m, test, &w, &h);
			if ((w > BS_TEXT_W || strlen(test) > 94) && line[0] && Bs_n_lines < BS_MAX_LINES) {
				strcpy(Bs_line[Bs_n_lines], word);
				Bs_line_hue[Bs_n_lines++] = (ubyte) hue;
			} else if (strlen(test) <= 94)
				strcpy(line, test);
		}
	}
}

static void bs_init(fe_screen *s)
{
	ubyte pal[768];
	int first, count;

	Bs_face = fe_febgnd(195, pal);			// 7 pictures of the face, 82x117 each
	Bs_picture = NULL;
	Bs_frame = Bs_prev_frame = 2;

	// 0x83DC8: the text is FETXT entry "first + blurb". The blurb number
	// goes up to count - 1 and then starts again at 1.
	if (Fe_khelben_mission < 0 || Fe_khelben_mission >= 34)
		Fe_khelben_mission = 0;
	first = Blurb_table[Fe_khelben_mission][0];
	count = Blurb_table[Fe_khelben_mission][1];
	if (Fe_khelben_blurb >= count)
		Fe_khelben_blurb = count > 1 ? 1 : 0;
	bs_parse(fe_fetxt(first + Fe_khelben_blurb));
	Fe_khelben_blurb++;
	if (Fe_khelben_blurb >= count)
		Fe_khelben_blurb = count > 1 ? 1 : 0;

	Bs_next_event = 0;
	Bs_state = 0;
	Bs_start = fe_time() + 2 * F1_0;			// 0x8313C
	Bs_face_time = 0;
	dtumusic_play(DTUMUSIC_CREDITS, DTUMUSIC_LOOP_FOREVER, 25);		// khelben.tun, volume 25
}

static void bs_frame(fe_screen *s)
{
	fix now = fe_time();
	int line_h = Fe_font_m->ft_h + 2, scroll, visible;

	if (Bs_state == 0 && now >= Bs_start)
		Bs_state = 1;
	if (Bs_state == 1) {
		// 0x8329A: one pixel each 0x2800 (0.156 seconds)
		scroll = (int) ((now - Bs_start) / 0x2800);
		visible = scroll / line_h;				// lines that have started to show
		while (Bs_next_event < Bs_n_events && Bs_event[Bs_next_event].line <= visible) {
			bs_event *e = &Bs_event[Bs_next_event++];
			ubyte pal[768];
			if (e->type == 's')
				fe_speech_play(e->value);
			else {
				if (Bs_picture) gr_free_bitmap(Bs_picture);
				Bs_picture = (e->type == 'p') ? fe_febgnd(27 + e->value, pal) : NULL;		// 0x13E886
			}
		}
		if (scroll >= Bs_n_lines * line_h) {
			Bs_state = 2;
			Bs_end_time = now + 10 * F1_0;
		}
	} else if (Bs_state == 2 && now >= Bs_end_time) {
		Bs_state = 3;
		dtumusic_fade(100);
	}

	// 0x82DEC: while the speech plays, another picture of the mouth
	// after 0.1 to 0.15 seconds. With no speech: picture 2.
	if (fe_speech_playing()) {
		if (now >= Bs_face_time) {
			int f;
			do f = fe_rand() % 7; while (f == Bs_frame || f == Bs_prev_frame);
			Bs_prev_frame = Bs_frame;
			Bs_frame = f;
			Bs_face_time = now + 0x1800 + 2 * (fe_rand() % 8) * 0x100;
		}
	} else
		Bs_frame = 2;
}

static void bs_draw(fe_screen *s)
{
	grs_canvas area;
	int line_h = Fe_font_m->ft_h + 2, scroll, i;

	if (Bs_face)
		// The color around the face (the first pixel of the sheet) is not drawn.
		fe_blit_part(Bs_face, 82 * Bs_frame, 0, 82, 117, 290, 124, Bs_face->bm_data[0]);
	if (Bs_picture)
		fe_blit(Bs_picture, 80 - Bs_picture->bm_w / 2, 400 - Bs_picture->bm_h / 2, -1);

	if (Bs_state == 1 || Bs_state == 2) {
		scroll = (int) ((fe_time() - Bs_start) / 0x2800);
		if (scroll > Bs_n_lines * line_h)
			scroll = Bs_n_lines * line_h;
		gr_init_sub_canvas(&area, &grd_curscreen->sc_canvas, BS_TEXT_X, BS_TEXT_Y, BS_TEXT_W, BS_LINES * line_h);
		gr_set_current_canvas(&area);
		gr_set_curfont(Fe_font_m);
		for (i = 0; i < Bs_n_lines; i++) {
			int y = BS_LINES * line_h - scroll + i * line_h;
			if (y <= -line_h || y >= BS_LINES * line_h || !Bs_line[i][0])
				continue;
			gr_set_fontcolor(fe_hue(FE_HUE_BLACK), -1);
			gr_string(1, y + 1, Bs_line[i]);
			gr_set_fontcolor(fe_hue(Bs_line_hue[i]), -1);
			gr_string(0, y, Bs_line[i]);
		}
		gr_set_current_canvas(&grd_curscreen->sc_canvas);
	}
}

static void bs_exit(fe_screen *s, int id)			// 0x82BA5
{
	s->running = 0;
}

static fe_control Bs_controls[1] = { { 0, 0, NULL, 509, 385, "btnbsxit" } };

static fe_feature Bs_features[3] = {
	{ 0, 0, 0, 0, 551, 422, 32, 0, 1, KEY_ESC, KEY_X, 570, NULL, NULL, bs_exit, 0, 0 },
	{ 221, 258, 436, 479, 0, 0, 0, -1, 0, KEY_F1, 0, 571, NULL, NULL, NULL, 1, 0 },
	{ 291, 119, 366, 258, 0, 0, 0, -1, 0, 0, 0, 571, NULL, NULL, NULL, 2, 0 } };

static fe_screen Bs = { Bs_controls, 1, { { 20, -1, -1 }, { 44, 42, 43 } }, Bs_features, 3,
	200, 420, 435, 475, FE_SCR_BLACKSTAFF, -1, bs_init, bs_draw, bs_frame, NULL, 0 };

void fe_blackstaff(void)
{
	fe_run_screen(&Bs);
	fe_speech_stop();
	dtumusic_stop();
	if (Bs_face) gr_free_bitmap(Bs_face);
	if (Bs_picture) gr_free_bitmap(Bs_picture);
	Bs_face = Bs_picture = NULL;
	Fe_khelben_visits++;
}

//------------------------------------------------------------------------
// The market (0x8474B, screen record 0x13F29E). The port shows the screen
// and its buttons. The lists of goods and the trade are not done.

static int Mk_mode, Mk_sell;			// MarketplaceMode (0x13ED24), 0x13ED20

static void mk_exit(fe_screen *s, int id)			// 0x84BA8
{
	s->running = 0;
}

static void mk_set(fe_screen *s, int id)
{
	if (id == 1 || id == 2)
		Mk_sell = id - 1;
	else
		Mk_mode = id - 3;
}

static void mk_closed(fe_screen *s, int id)
{
	// The text of the executable for the market of the demo version.
	fe_help_message(fe_text(699));
	fe_refuse();
}

static char *mk_help(int id)
{
	return id == 7 ? "Scroll up" : "Scroll down";
}

static fe_control Mk_controls[3] = {
	{ 0, 0, NULL, 507, 398, "btnmkxit" }, { 0, 0, NULL, 366, 43, "btnmkup" }, { 0, 0, NULL, 365, 96, "btnmkdwn" } };

static fe_feature Mk_features[9] = {
	{ 0, 0, 0, 0, 543, 434, 36, 0, 1, KEY_ESC, KEY_X, 570, NULL, NULL, mk_exit, 0, 0 },
	{ 427, 15, 520, 64, 0, 0, 0, -1, 1, KEY_B, 0, 572, NULL, NULL, mk_set, 1, 0 },
	{ 532, 15, 625, 64, 0, 0, 0, -1, 1, KEY_S, 0, 573, NULL, NULL, mk_set, 2, 0 },
	{ 452, 81, 619, 143, 0, 0, 0, -1, 1, KEY_W, 0, 574, NULL, NULL, mk_set, 3, 0 },
	{ 452, 174, 619, 237, 0, 0, 0, -1, 1, KEY_A, 0, 575, NULL, NULL, mk_set, 4, 0 },
	{ 453, 265, 618, 326, 0, 0, 0, -1, 1, KEY_M, 0, 576, NULL, NULL, mk_set, 5, 0 },
	{ 35, 37, 364, 364, 0, 0, 0, -1, 1, 0, 0, 577, NULL, NULL, mk_closed, 6, 0 },
	{ 366, 43, 396, 73, 0, 0, 0, 1, 1, KEY_UP, 0, -1, mk_help, NULL, NULL, 7, 1 },
	{ 365, 96, 395, 126, 0, 0, 0, 2, 1, KEY_DOWN, 0, -1, mk_help, NULL, NULL, 8, 1 } };

static fe_screen Mk = { Mk_controls, 3, { { 17, -1, -1 }, { 18, 40, 41 } }, Mk_features, 9,
	33, 408, 442, 461, FE_SCR_MARKET, -1, NULL, NULL, NULL, NULL, 0 };

void fe_market(void)
{
	Mk_mode = Mk_sell = 0;
	fe_run_screen(&Mk);
}
