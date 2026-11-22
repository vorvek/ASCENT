/*
 * ASCENT: new file. The Waterdeep screens of the front end of Descent to
 * Undermountain: the city (fewdeep.c of DTUGAME.EXE), the tower of Khelben
 * Blackstaff (frontend.c, fetorch.c) and the market (femarket.c). Descent 1
 * has nothing like them. See docs/re/frontend.md.
 *
 * Not done in the port: the torches and the eyes of the Blackstaff screen,
 * the change of the mission by the quest flags and the rewards of Khelben.
 * The market trades (buy, sell, stock, prices) as femarket.c does.
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
#include "dtustate.h"		// ASCENT: dtu_user_find
#include "dtuscript.h"		// the mission of Khelben
#include "object.h"			// the market: item classes, inventory, gold
#include "bm.h"
#include "weapon.h"
#include "dtuobj.h"
#include "dtuchar.h"

#define DTU_WEAK __attribute__((weak))

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
	// 0x8C25A: all streams stop after the screen, also on the way to the
	// main menu (the City Gates), which plays no music.
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
// ASCENT: the time of the last frame of the screen. A frame that takes more
// than BS_MAX_STEP (a load of upscaled pictures, a pause of the window)
// moves the timers forward by the rest, so the text pauses instead of
// jumping ahead. The original had no such long frames.
static fix Bs_last;
#define BS_MAX_STEP	(F1_0 / 10)
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
	// ASCENT: 0x83E36 first: the mission flags advance the mission and
	// give the rewards (dtuscript.c).
	Fe_khelben_mission = dtu_mission_advance(Fe_khelben_mission, &Fe_khelben_blurb);
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
	Bs_last = fe_time();
	dtumusic_play(DTUMUSIC_CREDITS, DTUMUSIC_LOOP_FOREVER, 25);		// khelben.tun, volume 25
}

static void bs_frame(fe_screen *s)
{
	fix now = fe_time();
	int line_h = Fe_font_m->ft_h + 2, scroll, visible;

	if (now - Bs_last > BS_MAX_STEP) {
		fix lost = now - Bs_last - BS_MAX_STEP;
		Bs_start += lost;
		Bs_end_time += lost;
		Bs_face_time += lost;
	}
	Bs_last = now;
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
// The market (femarket.c, 0x8474B, screen record 0x13F29E). The facts are
// in docs/re/frontend.md, section 6.

#define MK_ROWS			9			// rows of the catalog
#define MK_ROW_X			35
#define MK_ROW_Y			37
#define MK_ROW_STEP		37
#define MK_ROW_W			330
#define MK_ROW_H			32
#define MK_DESC_X			37			// the quantity and the name (0x85C77)
#define MK_PRICE_X		282		// the price (0x85C7A)
#define MK_MAX				160		// entries of one list
#define MK_PLAYER_ITEMS	127		// MAX_PLR_STUFF

// A line of the catalog: the 0x37-byte node of femarket.c.
typedef struct mk_entry {
	int	type, id;				// +0x08, +0x09: OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC
	int	cond;						// +0x0B: the count of one item record (charges), 1 for a stackable type
	int	qty;						// +0x0D: the number of items, -1: no limit
	int	value;					// +0x11: value= of the table line
	int	sell_price;				// +0x15
	int	buy_price;				// +0x19
	int	icon;						// item record +2: icon= of the table line
	int	equipped;				// +0x26
	char	name[40];				// +0x21: name= (weapon_name= for a weapon with no name=)
} mk_entry;

// The lists are circular in the executable: a scroll moves the first line
// shown, and goes round.
typedef struct mk_list {
	int		n, top;
	mk_entry	e[MK_MAX];
} mk_list;

static mk_list Mk_lists[2][3];		// [0 buy, 1 sell][0 weapons, 1 armor, 2 misc]: 0x13EB54.., 0x13EB38..
static int Mk_mode;						// MarketplaceMode 0x13ED24: 0 weapons, 1 armor, 2 misc
static int Mk_sell;						// TransactionMode 0x13ED20: 0 the player buys, 1 sells
static int Mk_charisma;					// 0x3A5088
static int Fe_market_charisma = -1;	// the record of the new character (0x13E38C), -1: none
static grs_bitmap *Mk_icons;			// FEBGND 22 (0x8578C)
static int Mk_icons_per_row, Mk_icon_count;
static grs_bitmap *Mk_popup;			// FEBGND 25 (0x860D9)
static int Mk_popup_x, Mk_popup_y;	// 0x13ED0C, 0x13ED10
static int Mk_popup_on;
static mk_entry *Mk_trans;				// 0x3A506C
static int Mk_trans_qty, Mk_trans_max, Mk_trans_price;	// 0x3A5058, 0x3A505C, 0x3A5070
static char Mk_title[48], Mk_header[512];
static int Mk_log_trade = 1;

// 0 (the executable): the market uses the charisma of the character that
// was made last in this run of the program (0x13E38C keeps pointing to
// the record of character generation), and the charisma of the player
// only if no character was made. 1 (default): the charisma of the player.
int Dtu_fix_market_charisma = 1;

// 0 (the executable): a sale adds to the stock of the market as many
// items as the player had of the line, not the number sold (0x8564F
// passes the copy of the line that was made before the sale). 1 (default):
// the number sold.
int Dtu_fix_market_sell_count = 1;

// The tables at 0x13F2DD (the player sells) and 0x13F32D (the player
// buys): percent of the value for charisma 0 to 19.
static short Mk_sell_percent[20] = { 20, 25, 27, 33, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 88, 93, 97, 99, 100 };
static short Mk_buy_percent[20] = { 400, 350, 300, 275, 250, 225, 200, 190, 180, 165, 150, 135, 125, 120, 115, 112, 108, 107, 105, 101 };

// The first stock (0x13EB6C), used when there is no geldor.dat: quantity,
// class, number, count of one item.
static int Mk_first_stock[21][4] = {
	{ -1, 16, 79, 1 }, { -1, 16, 7, 1 }, { -1, 16, 8, 1 }, { -1, 16, 6, 1 }, { -1, 16, 91, 1 },
	{ -1, 16, 89, 1 }, { 12, 16, 90, 1 }, { 1, 15, 0, 1 }, { 2, 15, 1, 1 }, { 4, 15, 2, 1 },
	{ 2, 15, 3, 1 }, { -1, 5, 7, 1 }, { -1, 5, 31, 1 }, { -1, 5, 11, 1 }, { -1, 5, 21, 1 },
	{ -1, 5, 22, 1 }, { -1, 5, 18, 1 }, { 9, 5, 30, 1 }, { 6, 5, 19, 1 }, { 3, 5, 28, 1 },
	{ 3, 5, 27, 1 } };

// The goods that the rewards of Khelben put into the market (0x879A7, the
// tables at 0x13F6A0): quantity, class, number, count (-1).
static int Mk_reward_stock[9][12][4] = {
	{ {1,15,39,-1}, {1,15,4,-1}, {1,15,14,-1}, {1,16,82,-1}, {1,16,85,-1}, {1,5,55,-1} },
	{ {1,15,12,-1}, {1,16,72,-1}, {1,16,78,-1}, {1,16,88,-1}, {1,5,43,-1}, {1,5,55,-1} },
	{ {1,16,86,-1}, {1,16,76,-1}, {1,16,11,-1}, {6,5,47,1}, {1,5,34,-1}, {1,5,40,-1}, {2,5,55,-1}, {1,5,58,-1} },
	{ {1,15,28,-1}, {1,15,18,-1}, {1,15,23,-1}, {1,5,32,-1}, {4,5,58,-1}, {1,5,38,-1} },
	{ {1,15,40,-1}, {1,15,7,-1}, {1,15,5,-1}, {1,15,21,-1}, {1,16,97,-1}, {1,5,58,-1}, {3,5,55,-1}, {1,16,92,-1} },
	{ {1,15,35,-1}, {1,15,24,-1}, {1,15,45,-1}, {1,16,94,-1}, {1,16,75,-1}, {4,16,77,-1}, {2,5,58,-1}, {1,5,59,-1}, {1,5,71,-1} },
	{ {1,15,29,-1}, {1,15,8,-1}, {1,15,6,-1}, {1,15,9,-1}, {1,16,74,-1}, {1,16,87,-1}, {7,5,55,-1} },
	{ {1,15,41,-1}, {1,15,26,-1}, {1,15,27,-1}, {1,16,2,-1}, {1,16,80,-1}, {1,16,93,-1}, {1,5,33,-1}, {1,5,39,-1},
	  {4,16,75,-1}, {6,5,58,-1}, {6,5,59,-1}, {4,16,88,-1} },
	{ {1,15,10,-1}, {1,15,52,-1}, {1,16,95,-1}, {1,5,69,-1}, {6,5,59,-1}, {4,16,75,-1}, {4,16,77,-1}, {4,16,88,-1}, {1,5,58,-1} } };

// ASCENT: the files of the market are in the directory of the user
// (dtu_user_path), with any case of the letters (dtu_user_find: the DOS
// game writes GELDOR.DAT and THESTORE.1). Two buffers: one expression can
// name two files.
static const char *mk_user_file(const char *name)
{
	static char path[2][600];
	static int i;

	i ^= 1;
	dtu_user_find(name, path[i], sizeof(path[i]));
	return path[i];
}
#define MK_STORE_FILE	mk_user_file("GELDOR.DAT")		// "geldor.dat" 0x12CE53, in the current directory

// The game flag 107 of the player (bit array at P+0x1A33, 0xADAFE) doubles
// the prices (0x86F5A). The port does not have these flags yet; a file
// that has them can define this function.
DTU_WEAK int dtu_hook_game_flag(int n)
{
	(void) n;
	return 0;
}

static item_info *mk_item(int type, int id)
{
	if (id < 0)
		return NULL;
	switch (type) {
	case OBJ_WEAPON:	return id < N_weapon_types ? &Weapon_item_info[id] : NULL;
	case OBJ_ARMOR:	return id < N_armor_types ? &Armor_info[id] : NULL;
	case OBJ_MISC:		return id < N_misc_types ? &Misc_info[id] : NULL;
	case OBJ_SPELL:	return id < N_spell_types ? &Spell_info[id] : NULL;
	}
	return NULL;
}

static int mk_category(int type)
{
	return type == OBJ_WEAPON ? 0 : type == OBJ_ARMOR ? 1 : type == OBJ_MISC ? 2 : -1;
}

static int mk_tbl_int(item_info *it, char *key, int def)
{
	char v[32];

	if (!it || it->record < 0 || !bm_tbl_get(it->record, key, v, sizeof(v)))
		return def;
	return atoi(v);
}

// 0xB61B2: the type keeps its items in one stack (the flag "stackable")
static int mk_stackable(int type, int id)
{
	item_info *it = mk_item(type, id);

	return it && it->record >= 0 && bm_tbl_has_flag(it->record, "stackable");
}

static int mk_plot_item(int type, int id)
{
	return mk_tbl_int(mk_item(type, id), "plot_item", 0) != 0;
}

// 0x86C5A: the price of one item. sell: 1 the player sells, 0 buys.
static int mk_price(int type, int id, int cond, int sell)
{
	item_info *it = mk_item(type, id);
	int value, max, fixed, d, price, cha;

	if (!it) {
		dtu_log("Player is purchasing something wierd in the market!!\n");
		value = 10;
		max = -1;
		fixed = 0;
	} else {
		value = it->value;
		// The flag "stackable" (weapon 0x02, armor 0x10, misc 0x01,
		// spell 0x02) gives a price that does not depend on the count.
		fixed = mk_stackable(type, id);
		switch (type) {
		case OBJ_WEAPON:	max = mk_tbl_int(it, "uses", 0); break;	// +0x2E, no start value
		case OBJ_ARMOR:	max = -1; break;
		default:				max = mk_tbl_int(it, "uses", 1); break;	// misc 0x23822, spell 0x2436B
		}
	}
	if (value < 1)
		value = 1;
	if (!fixed) {
		if (max < 1)
			max = 0xffff;
		if (max > 0xffff)
			max = 0xffff;
		if (cond < 0 || cond > 0xffff)
			cond = 0xffff;
		d = cond <= max ? cond : max;
		value = value * (d * 100 / max) / 100;
	}
	cha = Mk_charisma < 0 ? 0 : Mk_charisma > 19 ? 19 : Mk_charisma;	// 0x86F75
	price = value * (sell ? Mk_sell_percent[cha] : Mk_buy_percent[cha]) / 100;
	if (price < 1)
		price = 1;
	if (dtu_hook_game_flag(107))
		price *= 2;
	return price;
}

// 0x870A0: puts a line into its list, sorted by class, number, count
// (down) and the equipped flag. A line that is there already gets the
// quantity added (-1 stays -1).
static void mk_insert(mk_list *l, int type, int id, int cond, int qty, int equipped)
{
	item_info *it = mk_item(type, id);
	mk_entry *e;
	int i;

	if (!it)
		return;
	for (i = 0; i < l->n; i++) {
		e = &l->e[i];
		if (type < e->type || (type == e->type && (id < e->id || (id == e->id && (cond > e->cond
			|| (cond == e->cond && equipped < e->equipped))))))
			break;
		if (type == e->type && id == e->id && cond == e->cond && equipped == e->equipped) {
			e->qty = (e->qty < 0 || qty < 0) ? -1 : e->qty + qty;
			return;
		}
	}
	if (l->n >= MK_MAX)
		return;
	memmove(&l->e[i + 1], &l->e[i], (l->n - i) * sizeof(mk_entry));
	e = &l->e[i];
	memset(e, 0, sizeof(*e));
	e->type = type;
	e->id = id;
	e->cond = cond;
	e->qty = qty;
	e->value = it->value;
	e->icon = it->icon;
	e->equipped = equipped;
	snprintf(e->name, sizeof(e->name), "%s", it->name);
	if (!e->name[0] && type == OBJ_WEAPON && it->record >= 0)
		bm_tbl_get(it->record, "weapon_name", e->name, sizeof(e->name));
	e->sell_price = mk_price(type, id, cond, 1);
	e->buy_price = mk_price(type, id, cond, 0);
	l->n++;
	if (i < l->top)
		l->top++;
}

// 0x873EE: a line of the stock. For a stackable type the count is 1.
static void mk_add_stock(int qty, int type, int id, int count)
{
	int c = mk_category(type);

	if (c < 0)
		return;
	mk_insert(&Mk_lists[0][c], type, id, mk_stackable(type, id) ? 1 : (short) count, qty < 0 ? -1 : qty, 0);
}

// 0x86754 and 0x8622B: the item (type, number, count) is in a hot key,
// in the hand, or in a slot of the character.
static int mk_equipped(int type, int id, int cond)
{
	int refs[1 + DTU_N_HOT_KEYS + DTU_N_SLOTS], n = 0, i, k;

	refs[n++] = Dtu_char.current.n;
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		refs[n++] = Dtu_char.hot_key[i].n;
	for (i = 0; i < DTU_N_SLOTS; i++)
		refs[n++] = Dtu_char.slot[i].n;
	for (k = 0; k < n; k++) {
		dtu_inv_item *e;

		i = refs[k];
		if (i < 0 || i >= Dtu_n_inventory)
			continue;
		e = &Dtu_inventory[i];
		if (e->type == type && e->id == id && (mk_stackable(type, id) ? 1 : e->count) == cond)
			return 1;
	}
	return 0;
}

// 0x876ED: a line of the player. A stackable type gives one line with the
// number of the stack; another type gives one item with its count.
static void mk_add_player(int type, int id, int count)
{
	int c = mk_category(type), cond, qty;

	if (c < 0)
		return;
	if (mk_stackable(type, id)) {
		qty = count;
		cond = 1;
	} else {
		qty = 1;
		cond = count;
	}
	mk_insert(&Mk_lists[1][c], type, id, cond, qty, mk_equipped(type, id, cond));
}

// 0x872C1: the stock from geldor.dat, or the first stock
static void mk_read_stock(void)
{
	FILE *f = fopen(MK_STORE_FILE, "rb");
	ubyte r[10];
	int i, ok = 0;

	if (f) {
		ok = 1;
		while (fread(r, 1, 10, f) == 10) {
			int qty = (int) ((uint) r[0] | ((uint) r[1] << 8) | ((uint) r[2] << 16) | ((uint) r[3] << 24));
			int count = (int) ((uint) r[6] | ((uint) r[7] << 8) | ((uint) r[8] << 16) | ((uint) r[9] << 24));
			mk_add_stock(qty, r[4], r[5], count);		// 0x87393
		}
		fclose(f);
	}
	if (!ok)
		for (i = 0; i < 21; i++)
			mk_add_stock(Mk_first_stock[i][0], Mk_first_stock[i][1], Mk_first_stock[i][2], Mk_first_stock[i][3]);
}

// 0x86846: the stock goes into geldor.dat: armor, weapons, misc
static void mk_write_stock(void)
{
	static int order[3] = { 1, 0, 2 };
	FILE *f = fopen(MK_STORE_FILE, "wb");
	int c, i;

	if (!f)
		return;
	for (c = 0; c < 3; c++) {
		mk_list *l = &Mk_lists[0][order[c]];
		for (i = 0; i < l->n; i++) {
			mk_entry *e = &l->e[i];
			ubyte r[10];
			int v = e->qty, w = e->cond;
			r[0] = (ubyte) v; r[1] = (ubyte) (v >> 8); r[2] = (ubyte) (v >> 16); r[3] = (ubyte) (v >> 24);
			r[4] = (ubyte) e->type;
			r[5] = (ubyte) e->id;
			r[6] = (ubyte) w; r[7] = (ubyte) (w >> 8); r[8] = (ubyte) (w >> 16); r[9] = (ubyte) (w >> 24);
			fwrite(r, 1, 10, f);
		}
	}
	fclose(f);
}

// 0x8705D: the lists of the stock and of the player. Plot items are not
// listed (item record +0x0A).
static void mk_build_lists(void)
{
	int i;

	memset(Mk_lists, 0, sizeof(Mk_lists));
	mk_read_stock();
	for (i = 0; i < Dtu_n_inventory && i < MK_PLAYER_ITEMS; i++) {
		dtu_inv_item *e = &Dtu_inventory[i];
		if (!mk_plot_item(e->type, e->id))
			mk_add_player(e->type, e->id, e->count);
	}
}

static mk_list *mk_cur(void)
{
	return &Mk_lists[Mk_sell][Mk_mode];
}

static mk_entry *mk_shown(int row)
{
	mk_list *l = mk_cur();

	if (row >= l->n || l->n == 0)
		return NULL;
	return &l->e[(l->top + row) % l->n];
}

static void mk_gold_changed(void)
{
	Dtu_char.pa.gold = Dtu_gold;
}

//------------------------------------------------------------------------
// The inventory of the character. The executable takes one item of the
// type and count (0xB6E01). dtuchar.c has no public function that takes a
// whole entry and keeps the references of the character (inv_remove is
// static), so this one does it on the public data, as inv_remove does.

static void mk_ref_removed(dtu_item_ref *ref, int index)
{
	if (ref->n == index) ref->n = -1;
	else if (ref->n > index) ref->n--;
}

static int mk_take_one(int type, int id, int cond)
{
	int i, k, stack = mk_stackable(type, id);

	for (i = 0; i < Dtu_n_inventory; i++) {
		dtu_inv_item *e = &Dtu_inventory[i];
		int n;

		if (e->type != type || e->id != id || (!stack && e->count != cond))
			continue;
		n = stack ? 1 : e->count;
		if (e->count <= n) {
			mk_ref_removed(&Dtu_char.current, i);
			for (k = 0; k < DTU_N_HOT_KEYS; k++)
				mk_ref_removed(&Dtu_char.hot_key[k], i);
			for (k = 0; k < DTU_N_SLOTS; k++)
				mk_ref_removed(&Dtu_char.slot[k], i);
		}
		dtu_inventory_remove(i, n);
		return 1;
	}
	return 0;
}

// 0xB7303: the items of the type that the bag can take
static int mk_room(int type, int id)
{
	item_info *it = mk_item(type, id);
	int i, have = 0, max;

	if (mk_stackable(type, id)) {
		max = it && it->stack_limit > 0 ? it->stack_limit : 10;
		for (i = 0; i < Dtu_n_inventory; i++)
			if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id)
				have += Dtu_inventory[i].count;
	} else {
		max = MK_PLAYER_ITEMS;
		for (i = 0; i < Dtu_n_inventory; i++)
			if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id)
				have++;
	}
	return max - have;
}

//------------------------------------------------------------------------
// Drawing

static void mk_text_in(grs_font *font, int x, int y, int w, int h, int hue, char *s)
{
	fe_string_in(font, x, y, w, h, hue, -1, s);
}

// 0x858C3: the icon n of the sheet, 32x32
static void mk_draw_icon(int n, int x, int y)
{
	grs_bitmap *sb;
	int i, j, sx, sy;

	if (!Mk_icons || n < 0 || n >= Mk_icon_count)
		return;
	sx = (n % Mk_icons_per_row) * 32;
	sy = (n / Mk_icons_per_row) * 32;
	gr_set_current_canvas(&grd_curscreen->sc_canvas);
	sb = &grd_curcanv->cv_bitmap;
	for (j = 0; j < 32; j++) {
		ubyte *s = Mk_icons->bm_data + (sy + j) * Mk_icons->bm_rowsize + sx;
		if (y + j < 0 || y + j >= sb->bm_h)
			continue;
		for (i = 0; i < 32; i++)
			if (s[i] != 255 && x + i >= 0 && x + i < sb->bm_w)		// 0xE1280: 255 is not drawn
				sb->bm_data[(y + j) * sb->bm_rowsize + x + i] = s[i];
	}
}

// 0x85E06: one line of the catalog
static void mk_draw_row(int row)
{
	mk_entry *e = mk_shown(row);
	int x = MK_ROW_X, y = MK_ROW_Y + row * MK_ROW_STEP, w, h, ty;
	char buf[32];

	if (!e)
		return;
	mk_draw_icon(e->icon, x, y);
	sprintf(buf, "%d", Mk_sell ? e->sell_price : e->buy_price);
	mk_text_in(Fe_font_l, x + MK_PRICE_X, y, MK_ROW_W - MK_PRICE_X, MK_ROW_H, FE_HUE_7, buf);
	if (e->qty >= 100 || e->qty < 0)
		snprintf(buf, sizeof(buf), "%s", fe_text(701));		// "many"
	else
		sprintf(buf, "%d", e->qty);
	ty = y + (MK_ROW_H - Fe_font_m->ft_h) / 2;
	fe_string(Fe_font_m, x + MK_DESC_X, ty, FE_HUE_2, -1, buf);
	fe_string_size(Fe_font_m, fe_text(701), &w, &h);
	fe_string(Fe_font_m, x + MK_DESC_X + w + 5, ty, FE_HUE_3, -1, e->name);
}

// The selected buttons of the two radio groups (ferbtn.c, 0x13EC48 and
// 0x13ECA8): the "down" picture of the selected one. The last "_" of
// the names is the letter of the language (0xAEDC3), "e" for English.
static fe_control Mk_radio[5] = {
	{ 0, 0, NULL, 452, 81, "lbmwpnde" }, { 0, 0, NULL, 452, 174, "lbmarmde" }, { 0, 0, NULL, 453, 265, "lbmmscde" },
	{ 0, 0, NULL, 427, 15, "lbmbuyde" }, { 0, 0, NULL, 532, 15, "lbmselde" } };

static void mk_draw(fe_screen *s)
{
	char buf[32];
	int i;

	for (i = 0; i < 5; i++)
		if (!Mk_radio[i].bm_down)
			Mk_radio[i].bm_down = fe_egg_bitmap(Mk_radio[i].down, NULL);
	fe_blit(Mk_radio[Mk_mode].bm_down, Mk_radio[Mk_mode].x2, Mk_radio[Mk_mode].y2, -1);
	fe_blit(Mk_radio[3 + Mk_sell].bm_down, Mk_radio[3 + Mk_sell].x2, Mk_radio[3 + Mk_sell].y2, -1);

	for (i = 0; i < MK_ROWS; i++)
		mk_draw_row(i);

	// 0x85A3F: the gold, large font, color 2, in (452, 352) 166x25
	sprintf(buf, "%d", Dtu_gold);
	mk_text_in(Fe_font_l, 452, 352, 166, 25, FE_HUE_2, buf);

	if (Mk_popup_on && Mk_popup) {
		int px = Mk_popup_x, py = Mk_popup_y;

		fe_blit(Mk_popup, px, py, -1);
		// 0x84EEA: the title (medium font, color 3) at (35, 32) 339 wide,
		// the text under it (color 7), the quantity at (175, 144) 62x37
		mk_text_in(Fe_font_m, px + 35, py + 32, 339, 0, FE_HUE_3, Mk_title);
		mk_text_in(Fe_font_m, px + 35, py + 33 + Fe_font_m->ft_h, 339, 0, FE_HUE_7, Mk_header);
		sprintf(buf, "%d", Mk_trans_qty);
		mk_text_in(Fe_font_m, px + 175, py + 144, 62, 37, FE_HUE_7, buf);		// 0x86A63
	}
}

//------------------------------------------------------------------------
// The screen

static void mk_exit(fe_screen *s, int id)			// 0x84BA8
{
	s->running = 0;
}

static void mk_select(int sell, int mode)
{
	if (sell == Mk_sell && mode == Mk_mode)
		return;
	Mk_sell = sell;
	Mk_mode = mode;
}

static void mk_set(fe_screen *s, int id)
{
	if (id == 1 || id == 2)		// 0x84A6E buy, 0x84AEB sell
		mk_select(id - 1, Mk_mode);
	else								// 0x8494B, 0x849AA, 0x84A0C
		mk_select(Mk_sell, id - 3);
}

static void mk_scroll(fe_screen *s, int id)		// 0x84B6D, 0x84B8A
{
	mk_list *l = mk_cur();

	if (l->n <= MK_ROWS)
		return;
	l->top = id == 7 ? (l->top + l->n - 1) % l->n : (l->top + 1) % l->n;
}

static char *mk_help(int id)
{
	return id == 7 ? "Scroll up" : "Scroll down";		// 0x13EE94, 0x13EE98
}

static void mk_popup_open(fe_screen *s, mk_entry *e, char *warning);
static void mk_popup_close(fe_screen *s);

// 0x84D41: the player buys a line
static void mk_buy(fe_screen *s, mk_entry *e)
{
	char qty[16];
	int room, can;

	Mk_trans_price = e->buy_price;
	if (e->qty >= 100 || e->qty < 0)
		snprintf(qty, sizeof(qty), "%s", fe_text(701));		// "many"
	else
		sprintf(qty, "%d", e->qty);
	room = mk_room(e->type, e->id);
	if (room < 1) {
		fe_message_box(NULL, fe_text(700), 1, fe_text(34), NULL);	// 0x86993: the bag is full
		return;
	}
	if (Mk_trans_price > 0)
		can = Dtu_gold / Mk_trans_price;
	else
		can = e->qty < 0 ? 1000 : e->qty;
	if (can <= 0) {
		fe_message_box(NULL, fe_text(702), 1, fe_text(34), NULL);	// 0x86974: too little gold
		return;
	}
	// the smallest of the stock (unsigned: -1 has no limit), the gold and the room
	Mk_trans_max = can;
	if ((unsigned) e->qty < (unsigned) Mk_trans_max)
		Mk_trans_max = e->qty;
	if (room < Mk_trans_max)
		Mk_trans_max = room;
	snprintf(Mk_header, sizeof(Mk_header), fe_text(704), qty, (unsigned) Mk_trans_price, Mk_trans_max);
	mk_popup_open(s, e, NULL);
}

// 0x84CA6: the player sells a line
static void mk_sell(fe_screen *s, mk_entry *e)
{
	Mk_trans_price = e->sell_price;
	Mk_trans_max = e->qty;
	snprintf(Mk_header, sizeof(Mk_header), fe_text(705), e->qty, Mk_trans_price);
	mk_popup_open(s, e, mk_equipped(e->type, e->id, e->cond) ? fe_text(703) : NULL);
}

// 0x84BE4: a click into the catalog
static void mk_store_click(fe_screen *s, int id)
{
	int x = Fe_mx - MK_ROW_X, y = Fe_my - MK_ROW_Y, row = 0;
	mk_entry *e;

	if (x < 0 || y < 0 || x >= MK_ROW_W || y >= 0x148)
		return;
	while (row < MK_ROWS && y >= MK_ROW_H) {
		y -= MK_ROW_STEP;
		row++;
	}
	if (row >= MK_ROWS || row >= mk_cur()->n || !(e = mk_shown(row)))
		return;
	if (!Mk_sell)
		mk_buy(s, e);
	else
		mk_sell(s, e);
}

static fe_control Mk_controls[7] = {
	{ 0, 0, NULL, 507, 398, "btnmkxit" }, { 0, 0, NULL, 366, 43, "btnmkup" }, { 0, 0, NULL, 365, 96, "btnmkdwn" },
	{ 0, 0, NULL, 0, 0, "btnpulft" }, { 0, 0, NULL, 0, 0, "btnpurit" }, { 0, 0, NULL, 0, 0, "lbpyesde" },
	{ 0, 0, NULL, 0, 0, "lbprejde" } };	// "_" replaced as above

static fe_feature Mk_features[9] = {
	{ 0, 0, 0, 0, 543, 434, 36, 0, 1, KEY_ESC, KEY_X, 570, NULL, NULL, mk_exit, 0, 0 },
	{ 427, 15, 520, 64, 0, 0, 0, -1, 1, KEY_B, 0, 572, NULL, NULL, mk_set, 1, 0 },
	{ 532, 15, 625, 64, 0, 0, 0, -1, 1, KEY_S, 0, 573, NULL, NULL, mk_set, 2, 0 },
	{ 452, 81, 619, 143, 0, 0, 0, -1, 1, KEY_W, 0, 574, NULL, NULL, mk_set, 3, 0 },
	{ 452, 174, 619, 237, 0, 0, 0, -1, 1, KEY_A, 0, 575, NULL, NULL, mk_set, 4, 0 },
	{ 453, 265, 618, 326, 0, 0, 0, -1, 1, KEY_M, 0, 576, NULL, NULL, mk_set, 5, 0 },
	{ 35, 37, 364, 364, 0, 0, 0, -1, 1, 0, 0, 577, NULL, NULL, mk_store_click, 6, 0 },
	{ 366, 43, 396, 73, 0, 0, 0, 1, 0, KEY_UP, 0, -1, mk_help, NULL, mk_scroll, 7, 1 },
	{ 365, 96, 395, 126, 0, 0, 0, 2, 0, KEY_DOWN, 0, -1, mk_help, NULL, mk_scroll, 8, 1 } };

static fe_screen Mk = { Mk_controls, 7, { { 17, -1, -1 }, { 18, 40, 41 } }, Mk_features, 9,
	33, 408, 442, 461, FE_SCR_MARKET, -1, NULL, mk_draw, NULL, NULL, 0 };

//------------------------------------------------------------------------
// The quantity window (0x84EEA, FEBGND 25)

static void mk_qty_less(fe_screen *s, int id)		// 0x85740
{
	if (--Mk_trans_qty < 1)
		Mk_trans_qty = 1;
}

static void mk_qty_more(fe_screen *s, int id)		// 0x85719
{
	if (++Mk_trans_qty > Mk_trans_max)
		Mk_trans_qty = Mk_trans_max;
}

static void mk_qty_enter(fe_screen *s, int id)		// 0x86F9C, 0x86FCB
{
	char buf[8];
	int v;

	sprintf(buf, "%d", Mk_trans_qty);
	if (fe_input_box("Enter Qty", buf, 5) < 0)
		return;
	v = atoi(buf);
	if (v < 1) v = 1;
	if (v > Mk_trans_max) v = Mk_trans_max;
	Mk_trans_qty = v;
}

// 0x86B80: the quantity of the line goes down. Returns 1 if the line goes away.
static int mk_take_from_line(mk_list *l, mk_entry *e, int n)
{
	int i = (int) (e - l->e);

	if (e->qty != -1)
		e->qty -= n;
	if (e->qty >= 1 || e->qty == -1)
		return 0;
	memmove(&l->e[i], &l->e[i + 1], (l->n - i - 1) * sizeof(mk_entry));
	l->n--;
	if (l->n == 0 || i < l->top)
		l->top = l->n ? l->top - 1 : 0;
	if (l->n && l->top >= l->n)
		l->top = 0;
	return 1;
}

// 0x85458: "Accept"
static void mk_accept(fe_screen *s, int id)
{
	mk_list *l = mk_cur();
	mk_entry w = *Mk_trans;			// the copy of the line ("Widget")
	int i, n = 0, gold0 = Dtu_gold;

	if (!Mk_sell) {
		mk_take_from_line(l, Mk_trans, Mk_trans_qty);
		for (i = 0; i < Mk_trans_qty; i++) {
			if (Dtu_n_inventory >= MK_PLAYER_ITEMS)
				continue;
			if (dtu_inventory_add(w.type, w.id, w.cond) < 0)		// 0xB6315
				continue;
			mk_add_player(w.type, w.id, w.cond);				// 0x876ED
			Dtu_gold -= Mk_trans_price;
			n++;
		}
		if (Dtu_gold < 0)			// 0x8559D: the sign of the gold
			Dtu_gold = 0;
	} else {
		mk_take_from_line(l, Mk_trans, Mk_trans_qty);
		for (i = 0; i < Mk_trans_qty; i++)
			if (mk_take_one(w.type, w.id, w.cond)) {		// 0xB6E01
				Dtu_gold += Mk_trans_price;
				n++;
			}
		dtu_char_update();										// 0xAFB55
		mk_add_stock(Dtu_fix_market_sell_count ? Mk_trans_qty : w.qty, w.type, w.id, w.cond);
	}
	mk_gold_changed();
	if (Mk_log_trade) {
		int have = 0;
		for (i = 0; i < Dtu_n_inventory; i++)
			if (Dtu_inventory[i].type == w.type && Dtu_inventory[i].id == w.id)
				have += Dtu_inventory[i].count;
		printf("Market: %s %d x %s (class %d, number %d) at %d: gold %d -> %d; the inventory has %d of them, %d entries\n",
			Mk_sell ? "sold" : "bought", n, w.name, w.type, w.id, Mk_trans_price, gold0, Dtu_gold, have, Dtu_n_inventory);
		dtu_log("Market: %s %d x %s at %d: gold %d -> %d\n", Mk_sell ? "sold" : "bought", n, w.name, Mk_trans_price, gold0, Dtu_gold);
	}
	mk_popup_close(s);
}

static void mk_reject(fe_screen *s, int id)		// 0x8566D
{
	mk_popup_close(s);
}

static fe_feature Mk_popup_features[5] = {
	{ 0, 0, 0, 0, 0, 0, 0, 3, 1, KEY_LEFT, KEY_DOWN, 578, NULL, NULL, mk_qty_less, 11, 1 },
	{ 0, 0, 0, 0, 0, 0, 0, 4, 1, KEY_RIGHT, KEY_UP, 579, NULL, NULL, mk_qty_more, 12, 1 },
	// The keys of the next three features are the keys of the two
	// features above (the table at 0x13F185 to 0x13F1FF); they never act.
	{ 0, 0, 0, 0, 0, 0, 0, 5, 1, KEY_RIGHT, KEY_DOWN, 580, NULL, NULL, mk_accept, 13, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 6, 1, KEY_RIGHT, KEY_DOWN, 581, NULL, NULL, mk_reject, 14, 0 },
	{ 0, 0, 0, 0, 0, 0, 0, -1, 1, KEY_RIGHT, KEY_DOWN, 582, NULL, NULL, mk_qty_enter, 15, 0 } };

static void set_rect(fe_feature *f, int x1, int y1, int x2, int y2)
{
	f->x1 = x1; f->y1 = y1; f->x2 = x2; f->y2 = y2;
}

static void mk_popup_open(fe_screen *s, mk_entry *e, char *warning)
{
	int px, py;

	if (!Mk_popup) {
		ubyte pal[768];
		Mk_popup = fe_febgnd(25, pal);
		if (!Mk_popup)
			return;
	}
	Mk_popup_x = px = (FE_W - Mk_popup->bm_w) / 2;
	Mk_popup_y = py = (FE_H - Mk_popup->bm_h) / 2;
	Mk_trans = e;
	snprintf(Mk_title, sizeof(Mk_title), "%s", e->name);
	// 0x851A7: the places of the buttons of the window
	Mk_controls[3].x2 = px + 0x58;  Mk_controls[3].y2 = py + 0x7f;
	Mk_controls[4].x2 = px + 0x104; Mk_controls[4].y2 = py + 0x7f;
	Mk_controls[5].x2 = px + 0x36;  Mk_controls[5].y2 = py + 0x102;
	Mk_controls[6].x2 = px + 0xd9;  Mk_controls[6].y2 = py + 0x103;
	set_rect(&Mk_popup_features[0], px + 0x58, py + 0x7f, px + 0x9a, py + 0xc2);
	set_rect(&Mk_popup_features[1], px + 0x104, py + 0x7f, px + 0x147, py + 0xc5);
	set_rect(&Mk_popup_features[2], px + 0x36, py + 0x102, px + 0xc7, py + 0x13a);
	set_rect(&Mk_popup_features[3], px + 0xd9, py + 0x103, px + 0x168, py + 0x13b);
	set_rect(&Mk_popup_features[4], px + 0xaf, py + 0x90, px + 0xed, py + 0xb5);
	s->features = Mk_popup_features;
	s->n_features = 5;
	s->help_x1 = px + 0x29; s->help_y1 = py + 0xc8;
	s->help_x2 = px + 0x176; s->help_y2 = py + 0xfe;
	s->default_help = -1;
	Mk_popup_on = 1;
	Mk_trans_qty = 1;
	if (warning)
		fe_help_message(warning);		// 0x829B1
	// 0x85413: the cursor goes to (320, 280); the mouse stays in the window
	// (0xE880C). The port does not move the mouse.
}

static void mk_popup_close(fe_screen *s)
{
	Mk_popup_on = 0;
	Mk_trans = NULL;
	Mk_trans_qty = 0;
	s->features = Mk_features;
	s->n_features = 9;
	s->help_x1 = 33; s->help_y1 = 408; s->help_x2 = 442; s->help_y2 = 461;
}

//------------------------------------------------------------------------

void fe_market(void)
{
	ubyte pal[768];

	// 0x84755
	if (!Mk_icons) {
		Mk_icons = fe_febgnd(22, pal);				// 0x8578C: "FEicon file"
		if (Mk_icons) {
			Mk_icons_per_row = Mk_icons->bm_w / 32;
			Mk_icon_count = Mk_icons_per_row * (Mk_icons->bm_h / 32);
		}
	}
	// 0x85764: the charisma (0x3A5088)
	if (!Dtu_fix_market_charisma && Fe_market_charisma >= 0)
		Mk_charisma = Fe_market_charisma;
	else
		Mk_charisma = Dtu_char.pa.charisma;
	mk_build_lists();
	Mk_mode = Mk_sell = 0;
	mk_popup_close(&Mk);
	Mk.running = 0;
	dtu_log("Market: charisma %d, gold %d\n", Mk_charisma, Dtu_gold);
	fe_run_screen(&Mk);
	// 0x847DE: the stock goes into geldor.dat
	mk_popup_close(&Mk);
	mk_write_stock();
}

// 0x87BB7: the record of TheStore.<n>: bytes 0..3 and 6..9 change places,
// and bytes 4 and 5.
static void mk_swap_record(ubyte *r)
{
	ubyte t[4];

	memcpy(t, r, 4);
	memcpy(r, r + 6, 4);
	memcpy(r + 6, t, 4);
	t[0] = r[4]; r[4] = r[5]; r[5] = t[0];
}

void fe_market_store(int s, int write)
{
	char name[20];
	FILE *in, *out;
	ubyte r[10];

	if (s < 0) s = 0;
	if (s > 999) s = 999;
	sprintf(name, "THESTORE.%d", s);			// "TheStore.%d" 0x12D0FB
	if (!write)
		remove(MK_STORE_FILE);					// 0x87BFB
	in = fopen(write ? MK_STORE_FILE : mk_user_file(name), "rb");
	if (!in)
		return;
	out = fopen(write ? mk_user_file(name) : MK_STORE_FILE, "wb");
	if (!out) {
		fclose(in);
		return;
	}
	while (fread(r, 1, 10, in) == 10) {
		mk_swap_record(r);
		fwrite(r, 1, 10, out);
	}
	fclose(in);
	fclose(out);
}

void fe_market_new_character(void)
{
	remove(MK_STORE_FILE);						// 0x87BFB
	Fe_market_charisma = Dtu_char.base.charisma;	// 0x87E82: 0x13E38C = the new record
}

// 0x879A7: a reward of Khelben (0 to 8) adds goods to geldor.dat. Nothing
// is added when the file does not exist. Returns 1 if it was written.
int fe_market_reward(int n)
{
	FILE *f;
	long size;
	int i;

	if (n < 0 || n > 8 || (f = fopen(MK_STORE_FILE, "rb+")) == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, size - size % 10, SEEK_SET);
	for (i = 0; i < 12 && Mk_reward_stock[n][i][1]; i++) {
		ubyte r[10];
		int v = Mk_reward_stock[n][i][0], w = Mk_reward_stock[n][i][3];
		r[0] = (ubyte) v; r[1] = (ubyte) (v >> 8); r[2] = (ubyte) (v >> 16); r[3] = (ubyte) (v >> 24);
		r[4] = (ubyte) Mk_reward_stock[n][i][1];
		r[5] = (ubyte) Mk_reward_stock[n][i][2];
		r[6] = (ubyte) w; r[7] = (ubyte) (w >> 8); r[8] = (ubyte) (w >> 16); r[9] = (ubyte) (w >> 24);
		fwrite(r, 1, 10, f);
	}
	fclose(f);
	return 1;
}
