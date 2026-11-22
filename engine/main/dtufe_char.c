/*
 * ASCENT: new file. The character generation of Descent to Undermountain
 * (fenuchar.c and ferbtn.c of DTUGAME.EXE): four screens, and the values
 * of the new character. Descent 1 has nothing like it: it replaces the
 * pilot selection of menu.c. See docs/re/frontend.md.
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
#include "object.h"
#include "player.h"
#include "text.h"
#include "dtufile.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtudisp.h"
#include "dtufe.h"
#include "dtufe_int.h"

extern char Dtu_player_archive[16];

// Class bits of the class mask (0x3A6465)
#define CM_FIGHTER	1
#define CM_THIEF		2
#define CM_MAGE		4
#define CM_CLERIC		8

enum { ST_STR, ST_DEX, ST_CON, ST_INT, ST_WIS, ST_CHA };

// State (the names of the notes in docs/re/frontend.md)
static int Race;					// 0 human, 1 half-elf, 2 elf, 3 drow, 4 dwarf, 5 halfling (0x3A645D)
static int Sex;					// 0 male, 1 female (0x3A6461)
static int Chosen;				// race and sex are chosen (0x13FE20)
static int Class_mask;
static int Stats_valid;			// 0x13F6C9
static int Alignment;			// 0 to 5 (0x3A6469)
static int Portrait;				// frame of FESEQ 21 or 22 (0x13FE0C)
static char Name[16];
static int Continue;				// 0x3A64AD
static int Screen_index, Delta;
static int Book_open;
static int Cheat_stats;			// 0x13FE03
static int Slots_stale;			// the race, the sex or the classes changed: the numbers are not valid
static int Generated;			// the numbers were rolled
static grs_bitmap *Book_bm, *Figure_bm, *Class_bm[4];

// The statistics screen
static int Src[6];				// the six rolled numbers, 0: the slot is empty
static int Row_value[6], Row_bonus[6];
static int Bonus_box;			// free bonus points
static int Holding, Held_value, Held_bonus;
static int Percentile;			// exceptional strength roll (0x13F868)
static int Final[6], Final_percentile;		// the result (record at 0x3A63F8)

#define BONUS_POINTS	7		// 0x89B95. The manual says 5.

static char *Race_word[6] = { "Human", "Half Elven", "Elven", "Drow", "Dwarven", "Halfling" };	// 0x13F850
static char *Sex_word[2] = { "Male", "Female" };

// 0x13FECE: the class masks that a race can have. Bit (mask - 1) is set
// if the combination "mask" is allowed.
static int Race_combos[6] = { 139, 2495, 191, 191, 391, 135 };

// 0x13FD8F
static int Race_mod[6][6] = {
	{ 0, 0, 0, 0, 0, 0 }, { 0, 0, 0, 0, 0, 0 }, { 0, 1, -1, 0, 0, 0 },
	{ 0, 1, -1, 0, 0, 0 }, { 0, 0, 1, 0, 0, -1 }, { -1, 1, 0, 0, 0, 0 } };
// 0x13FCE3: the least and the greatest value of each statistic
static int Race_min[6][6] = {
	{ 3, 3, 3, 3, 3, 3 }, { 3, 6, 6, 4, 3, 8 }, { 3, 6, 7, 8, 3, 8 },
	{ 3, 6, 7, 8, 3, 8 }, { 8, 3, 11, 3, 3, 3 }, { 7, 7, 10, 6, 3, 3 } };
static int Race_max[6][6] = {
	{ 18, 18, 18, 18, 18, 18 }, { 18, 18, 18, 18, 18, 18 }, { 18, 19, 18, 18, 18, 18 },
	{ 18, 19, 18, 18, 18, 18 }, { 18, 17, 19, 18, 18, 17 }, { 18, 19, 18, 18, 17, 18 } };

// 0x13FFFA: the statistics of a quick character
static int Quick_stats[4][6] = {
	{ 16, 12, 16, 10, 10, 12 },		// fighter
	{ 15, 17, 13, 11, 10, 10 },		// thief
	{ 10, 15, 10, 16, 13, 12 },		// mage
	{ 16, 10, 12, 11, 16, 11 } };		// cleric

static int Str_hit[20] = { -9, -5, -3, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3 };			// 0x1401EE
static int Str_damage[20] = { -9, -4, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 7 };		// 0x140382
static int Dex_armor[20] = { 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -4 };			// 0x14019A
static int Con_hp[2][20] = {
	{ -9, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2 },		// 0x1402E2
	{ -9, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5 } };		// 0x140332: fighter
static int Int_spells[20] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 7, 7, 7, 9, 9, 11, 11, 14, 18, 99 };			// 0x14023E
static int Wis_bonus[4][20] = {
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0, 1, 2, 2, 2, 2, 2, 3 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 0, 1, 2, 2, 2, 2 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 0, 0, 0, 1, 1, 2 },
	{ 0,0,0,0,0,0,0,0,0,0,0,0,0, 0, 0, 0, 0, 0, 1, 1 } };
static int Cha_react[20] = { -100, -7, -6, -5, -4, -3, -2, -1, 0, 0, 0, 0, 0, 1, 2, 3, 5, 6, 7, 8 };	// 0x14014A

static int clamp_stat(int v)
{
	return v < 1 ? 1 : (v > 19 ? 19 : v);
}

static int is_fighter(void) { return (Class_mask & CM_FIGHTER) != 0; }

// 0x88B25
static int combo_allowed(int mask)
{
	return mask > 0 && mask < 16 && (Race_combos[Race] & (1 << (mask - 1)));
}

static void class_words(char *out)
{
	static int bit[4] = { CM_FIGHTER, CM_THIEF, CM_MAGE, CM_CLERIC };
	int i;

	out[0] = 0;
	for (i = 0; i < 4; i++)
		if (Class_mask & bit[i]) {
			if (out[0]) strcat(out, "/");
			strcat(out, fe_text(689 + i));
		}
}

static void free_bm(grs_bitmap **bm)
{
	if (*bm)
		gr_free_bitmap(*bm);
	*bm = NULL;
}

static void clear_classes(void)			// 0x87F3C
{
	int i;

	Class_mask = 0;
	for (i = 0; i < 4; i++)
		free_bm(&Class_bm[i]);
	Stats_valid = 0;
	Slots_stale = 1;
}

//------------------------------------------------------------------------
// The shields that all screens have

static void h_abort(fe_screen *s, int id)			// 0x8B27E
{
	if (Chosen && fe_message_box(NULL, fe_text(630), 2, fe_text(32), fe_text(33)) != 0)
		return;
	s->running = 0;
	Screen_index = 0;
	Delta = 0;
	Continue = 0;
}

static void h_next(fe_screen *s, int id)			// 0x8B2A9
{
	s->running = 0;
	Delta = 1;
}

static void h_back(fe_screen *s, int id)			// 0x8B2C9
{
	s->running = 0;
	Delta = -1;
}

static void h_random(fe_screen *s, int id);

static char Help_buf[300];

static char *help_im_a(int id)			// "I am a %s %s %s"
{
	char cw[64];

	class_words(cw);
	sprintf(Help_buf, fe_text(629), Sex_word[Sex], Race_word[Race], cw);
	return Help_buf;
}

//------------------------------------------------------------------------
// Screen 1: race and sex (0x1408A1)

static int Turn_seq[6][2] = { { -1, 1 }, { 10, 2 }, { 9, 3 }, { 8, 4 }, { 7, 5 }, { 6, -1 } };		// 0x13FE26
// male rise, female rise, male recede, female recede (0x13FE56)
static int Figure_seq[6][4] = { { 25, 13, 24, 12 }, { 27, 15, 26, 14 }, { 29, 17, 28, 16 },
	{ 31, 19, 30, 18 }, { 33, 21, 32, 20 }, { 35, 23, 34, 22 } };

static void s1_draw(fe_screen *s)			// 0x88004
{
	if (Book_bm)
		fe_blit(Book_bm, Book_bm->bm_x, Book_bm->bm_y, -1);
	if (Figure_bm)
		fe_blit(Figure_bm, Figure_bm->bm_x, Figure_bm->bm_y, -1);
	// the text of the race: FETXT entry "race", small font, black
	if (Book_open)
		fe_string(Fe_font_s, 455 + 5, 131 + 5, FE_HUE_BLACK, -1, fe_fetxt(Race));
}

static void s1_init(fe_screen *s)			// 0x881B5
{
	if (!Book_open) {
		fix until = fe_time() + F1_0 / 2;
		while (fe_time() < until && fe_frame_begin())
			fe_frame_end();
		free_bm(&Book_bm);
		Book_bm = fe_stream_play(0, s);
		Book_open = 1;
	}
}

static void recede_figure(fe_screen *s)			// 0x88A1E
{
	if (Chosen) {
		grs_bitmap *bm = fe_stream_play(Figure_seq[Race][2 + Sex], s);
		if (bm) gr_free_bitmap(bm);
		free_bm(&Figure_bm);
		Chosen = 0;
	}
}

static void s1_turn(fe_screen *s, int dir)			// 0x8885B. dir 0: back, 1: forward
{
	int seq = Turn_seq[Race][dir];

	if (seq < 0) {
		fe_help_message(fe_text(dir ? 715 : 714));
		fe_refuse();
		return;
	}
	recede_figure(s);
	clear_classes();
	fe_sound(FE_SND_PAGE);
	free_bm(&Book_bm);
	Book_bm = fe_stream_play(seq, s);
	Race += dir ? 1 : -1;
}

static void s1_page_back(fe_screen *s, int id) { s1_turn(s, 0); }
static void s1_page_forward(fe_screen *s, int id) { s1_turn(s, 1); }

static void s1_choose(fe_screen *s, int sex)			// 0x889B7
{
	if (Chosen) {
		int same = (Sex == sex);
		recede_figure(s);
		if (same)
			return;
	}
	free_bm(&Figure_bm);
	Figure_bm = fe_stream_play(Figure_seq[Race][sex], s);
	Sex = sex;
	Chosen = 1;
}

static void s1_male(fe_screen *s, int id) { s1_choose(s, 0); }
static void s1_female(fe_screen *s, int id) { s1_choose(s, 1); }

static int s1_may_leave(fe_screen *s)			// 0x88128
{
	if (Delta > 0 && !Chosen) {
		fe_help_message(fe_text(712));
		fe_refuse();
		Delta = 0;
		return 0;
	}
	return 1;
}

static char *s1_help(int id)
{
	switch (id) {
	case 0:		// 0x88423
		if (Chosen)
			sprintf(Help_buf, "%s %s %s %s", fe_text(632), Sex_word[Sex], Race_word[Race], fe_text(633));
		else
			sprintf(Help_buf, "%s %s", fe_text(632), fe_text(633));
		return Help_buf;
	case 2: return "Return to main menu";		// 0x13F7F3
	case 3: return fe_text(Chosen ? 611 : 612);
	case 4: return fe_text(Race > 0 ? 710 : 714);
	case 6: return fe_text(Race < 5 ? 711 : 715);
	case 5:
	case 7:
		sprintf(Help_buf, "%s a %s %s", fe_text((Chosen && Sex == (id == 7)) ? 608 : 607),
			Sex_word[id == 7], Race_word[Race]);
		return Help_buf;
	}
	return NULL;
}

static fe_control S1_controls[4] = {
	{ 0, 0, NULL, 0, 0, "btn1ulc" }, { 0, 0, NULL, 562, 12, "btn1urc" },
	{ 0, 0, NULL, 0, 410, "btn1llc" }, { 0, 0, NULL, 556, 393, "btn1lrc" } };

static fe_feature S1_features[9] = {
	{ 0, 0, 0, 0, 38, 40, 38, 0, 1, 0x13, 0, -1, s1_help, NULL, h_random, 0, 0 },
	{ 0, 0, 0, 0, 603, 51, 38, 1, 1, KEY_ESC, 0, 583, NULL, NULL, h_abort, 1, 0 },
	{ 0, 0, 0, 0, 38, 451, 38, 2, 1, KEY_PAGEDOWN, 0, -1, s1_help, NULL, h_abort, 2, 0 },
	{ 0, 0, 0, 0, 597, 438, 38, 3, 1, KEY_PAGEUP, 0, -1, s1_help, NULL, h_next, 3, 0 },
	{ 84, 307, 133, 354, 0, 0, 0, -1, 1, KEY_LEFT, 0, -1, s1_help, s1_page_back, NULL, 4, 0 },
	{ 72, 122, 222, 374, 0, 0, 0, -1, 1, 0x02, 0, -1, s1_help, NULL, s1_male, 5, 0 },
	{ 348, 315, 395, 362, 0, 0, 0, -1, 1, KEY_RIGHT, 0, -1, s1_help, s1_page_forward, NULL, 6, 0 },
	{ 241, 123, 403, 373, 0, 0, 0, -1, 1, 0x03, 0, -1, s1_help, NULL, s1_female, 7, 0 },
	{ 461, 108, 628, 378, 0, 0, 0, -1, 0, 0, 0, 584, NULL, NULL, NULL, 8, 0 } };

static fe_screen S1 = { S1_controls, 4, { { 1, -1, -1 }, { 2, 3, 4 } }, S1_features, 9,
	117, 425, 539, 470, FE_SCR_CHAR1, 706, s1_init, s1_draw, NULL, s1_may_leave, 0 };

//------------------------------------------------------------------------
// Screen 2: class (0x140C26)

// index 0 fighter, 1 thief, 2 mage, 3 cleric (0x13FF76)
static int Class_bit[4] = { CM_FIGHTER, CM_THIEF, CM_MAGE, CM_CLERIC };
static int Class_sel_seq[4] = { 36, 42, 40, 38 };
static int Class_desel_seq[4] = { 37, 43, 41, 39 };
static int Class_desc[4] = { 616, 619, 618, 617 };

static void s2_draw(fe_screen *s)			// 0x88A80
{
	int i;

	for (i = 0; i < 4; i++)
		if (Class_bm[i])
			fe_blit(Class_bm[i], Class_bm[i]->bm_x, Class_bm[i]->bm_y, -1);
}

static void s2_toggle(fe_screen *s, int c)			// 0x88BD7
{
	int bit = Class_bit[c];

	if (!(Class_mask & bit)) {
		if (!combo_allowed(Class_mask | bit)) {
			fe_help_message(fe_text(717));
			fe_refuse();
			return;
		}
		free_bm(&Class_bm[c]);
		Class_bm[c] = fe_stream_play(Class_sel_seq[c], s);
		Class_mask |= bit;
	} else {
		int rest = Class_mask & ~bit;
		grs_bitmap *bm;
		if (rest && !combo_allowed(rest)) {
			fe_help_message(fe_text(716));
			fe_refuse();
			return;
		}
		bm = fe_stream_play(Class_desel_seq[c], s);
		if (bm) gr_free_bitmap(bm);
		free_bm(&Class_bm[c]);
		Class_mask = rest;
	}
	Stats_valid = 0;
	Slots_stale = 1;
}

static int s2_may_leave(fe_screen *s)			// 0x88AF5
{
	if (Delta > 0 && !Class_mask) {
		fe_help_message(fe_text(713));
		fe_refuse();
		Delta = 0;
		return 0;
	}
	return 1;
}

static char *s2_help(int id)			// 0x881ED
{
	char cw[64];

	if (id == 0) {
		class_words(cw);
		sprintf(Help_buf, "%s %s %s %s %s", fe_text(632), Sex_word[Sex], Race_word[Race], cw, fe_text(633));
		return Help_buf;
	}
	if (id == 2) return "Return to Race and Gender Screen";		// 0x13F80B
	if (id == 3) return fe_text(Class_mask ? 613 : 614);
	if (id >= 4 && id <= 7) {
		int c = id - 4;
		if (Class_mask & Class_bit[c]) {
			if (Class_mask == Class_bit[c])
				sprintf(Help_buf, fe_text(634), fe_text(689 + c));
			else {
				class_words(cw);
				sprintf(Help_buf, fe_text(635), cw, fe_text(689 + c));
			}
			return Help_buf;
		}
		return fe_text(combo_allowed(Class_mask | Class_bit[c]) ? Class_desc[c] : 615);
	}
	return NULL;
}

static void s2_class(fe_screen *s, int id) { s2_toggle(s, id - 4); }

static fe_control S2_controls[4] = {
	{ 0, 0, NULL, 4, 0, "btn2ulc" }, { 0, 0, NULL, 551, 0, "btn2urc" },
	{ 0, 0, NULL, 20, 392, "btn2llc" }, { 0, 0, NULL, 529, 392, "btn2lrc" } };

static fe_feature S2_features[8] = {
	{ 0, 0, 0, 0, 41, 36, 37, 0, 1, 0x13, 0, -1, s2_help, NULL, h_random, 0, 0 },
	{ 0, 0, 0, 0, 588, 35, 37, 1, 1, KEY_ESC, 0, 583, NULL, NULL, h_abort, 1, 0 },
	{ 0, 0, 0, 0, 60, 433, 36, 2, 1, KEY_PAGEDOWN, 0, -1, s2_help, NULL, h_back, 2, 0 },
	{ 0, 0, 0, 0, 571, 433, 37, 3, 1, KEY_PAGEUP, 0, -1, s2_help, NULL, h_next, 3, 0 },
	{ 88, 34, 290, 193, 0, 0, 0, -1, 1, 0x21, 0, -1, s2_help, NULL, s2_class, 4, 0 },			// F: fighter
	{ 330, 227, 531, 385, 0, 0, 0, -1, 1, 0x14, 0, -1, s2_help, NULL, s2_class, 5, 0 },		// T: thief
	{ 86, 227, 288, 384, 0, 0, 0, -1, 1, 0x32, 0, -1, s2_help, NULL, s2_class, 6, 0 },			// M: mage
	{ 331, 37, 529, 193, 0, 0, 0, -1, 1, 0x2E, 0, -1, s2_help, NULL, s2_class, 7, 0 } };		// C: cleric

static fe_screen S2 = { S2_controls, 4, { { 5, -1, -1 }, { 6, 7, 8 } }, S2_features, 8,
	112, 409, 516, 455, FE_SCR_CHAR2, 707, NULL, s2_draw, NULL, s2_may_leave, 0 };

//------------------------------------------------------------------------
// Screen 3: statistics (0x1415C5)

// 0xB0CDB(3, 6, 1, 0, 0): three dice, a 1 is rolled again
static int roll_3d6(void)
{
	int i, sum = 0, r;

	for (i = 0; i < 3; i++) {
		do r = fe_rand() % 6 + 1; while (r == 1);
		sum += r;
	}
	return sum;
}

// The strength row of a fighter who is not a halfling has the
// exceptional strength (type 2, 0x8AB79).
static int row_exceptional(int i)
{
	return i == ST_STR && is_fighter() && Race != 5;
}

// The most bonus points that a row can hold (0x8AA0A).
static int row_cap(int i)
{
	int mod = Race_mod[Race][i], v = Row_value[i], n;

	if (row_exceptional(i)) {
		n = 18 - (v + mod);
		if (n < 0) n = 0;
		if (mod >= 0) {
			int p = (v >= 18) ? Percentile : 1;
			while (p != 0) {
				n++;
				p += 10;
				if (p > 99) p = 0;
			}
		}
		return n;
	}
	n = 18 + (mod > 0 ? mod : 0) - (v + mod);
	return n < 0 ? 0 : n;
}

// The value of a row with its points and the modifier of the race (0x89D42, 0x8AAA7).
static void row_final(int i, int *final, int *pct)
{
	int mod = Race_mod[Race][i], v = Row_value[i], b = Row_bonus[i], cap = row_cap(i);

	if (b > cap) b = cap;
	*pct = 1;
	if (row_exceptional(i)) {
		int to18 = 18 - (v + mod), use, p;
		if (to18 < 0) to18 = 0;
		use = b < to18 ? b : to18;
		*final = v + mod + use;
		b -= use;
		p = (v >= 18) ? Percentile : 1;
		while (b-- > 0 && p != 0) {
			p += 10;
			if (p > 99) p = 0;
		}
		*pct = (*final >= 18) ? p : 1;
	} else {
		*final = v + b + mod;
		if (i == ST_STR && *final >= 18)
			*pct = Percentile;
	}
}

static void stat_limits(int *lo, int *hi)			// 0x8B71C
{
	int i;

	for (i = 0; i < 6; i++) {
		lo[i] = Race_min[Race][i];
		hi[i] = Race_max[Race][i];
	}
	if ((Class_mask & CM_FIGHTER) && lo[ST_STR] < 9) lo[ST_STR] = 9;
	if ((Class_mask & CM_THIEF) && lo[ST_DEX] < 9) lo[ST_DEX] = 9;
	if ((Class_mask & CM_MAGE) && lo[ST_INT] < 9) lo[ST_INT] = 9;
	if ((Class_mask & CM_CLERIC) && lo[ST_WIS] < 9) lo[ST_WIS] = 9;
}

static int str_to_hit(int str, int pct)
{
	if (is_fighter() && str == 18 && pct != 1)
		return pct == 0 ? 3 : (pct >= 51 ? 2 : 1);
	return Str_hit[clamp_stat(str)];
}

static int str_damage(int str, int pct)			// 0x8C065
{
	int d = Str_damage[clamp_stat(str)];

	if (is_fighter() && str == 18) {
		d++;
		if (pct == 0 || pct >= 76) d++;
		if (pct == 0 || pct >= 91) d++;
		if (pct == 0) d++;
	}
	return d;
}

static void s3_reset_slots(void)
{
	int i;

	for (i = 0; i < 6; i++)
		Src[i] = Row_value[i] = Row_bonus[i] = 0;
	Bonus_box = BONUS_POINTS;
	Holding = Held_value = Held_bonus = 0;
}

static void s3_generate(fe_screen *s, int id)			// 0x89A77
{
	grs_bitmap *bm;
	int i;

	Cheat_stats = 0;
	s3_reset_slots();
	for (i = 0; i < 6; i++)
		Src[i] = roll_3d6();
	Percentile = (Race == 5) ? 1 : fe_rand() % 100;
	Stats_valid = 0;
	Generated = 1;
	bm = fe_stream_play(44, s);		// the planks turn
	if (bm) gr_free_bitmap(bm);
}

// 0x8A578. slot 0 to 5: a rolled number, 6 to 11: a statistic, 12: the bonus box.
static void s3_click(fe_screen *s, int slot)
{
	int right = Fe_rdown, i, n;

	Stats_valid = 0;
	if (!Holding) {
		if (slot < 6) {
			if (Src[slot]) {
				Held_value = Src[slot]; Held_bonus = 0; Src[slot] = 0; Holding = 1;
			}
		} else if (slot < 12) {
			i = slot - 6;
			if (right && Row_bonus[i]) {
				Held_value = 0; Held_bonus = Row_bonus[i]; Row_bonus[i] = 0; Holding = 1;
			} else if (Row_value[i]) {
				Held_value = Row_value[i]; Held_bonus = Row_bonus[i];
				Row_value[i] = Row_bonus[i] = 0; Holding = 1;
			}
		} else if (Bonus_box > 0) {
			Bonus_box--; Held_value = 0; Held_bonus = 1; Holding = 1;
		} else
			fe_help_message(fe_text(722));
		return;
	}

	if (!Held_value) {			// bonus points only
		if (slot == 12) {
			if (Bonus_box > 0) {
				Bonus_box--; Held_bonus++;
			} else {
				Bonus_box += Held_bonus; Held_bonus = 0; Holding = 0;
			}
		} else if (slot < 6) {
			fe_help_message(fe_text(723));
			Bonus_box += Held_bonus; Held_bonus = 0; Holding = 0;
		} else {
			i = slot - 6;
			if (right && Row_bonus[i]) {
				Held_bonus += Row_bonus[i]; Row_bonus[i] = 0;
			} else if (!Row_value[i]) {
				fe_help_message(fe_text(724));
				Bonus_box += Held_bonus; Held_bonus = 0; Holding = 0;
			} else {
				n = row_cap(i) - Row_bonus[i];
				if (n > Held_bonus) n = Held_bonus;
				if (n <= 0)
					fe_help_message(fe_text(723));
				else {
					Row_bonus[i] += n;
					Bonus_box += Held_bonus - n; Held_bonus = 0; Holding = 0;
				}
			}
		}
		return;
	}

	// a value, with or without bonus points
	if (slot == 12)
		fe_help_message(fe_text(725));
	else if (slot < 6) {
		int old = Src[slot];
		Src[slot] = Held_value;
		Bonus_box += Held_bonus;
		Held_bonus = 0;
		Held_value = old;
		Holding = old != 0;
	} else {
		i = slot - 6;
		if (right && Row_bonus[i]) {
			Held_bonus += Row_bonus[i]; Row_bonus[i] = 0;
		} else {
			int old_v = Row_value[i], old_b = Row_bonus[i];
			Row_value[i] = Held_value;
			Row_bonus[i] = 0;
			n = row_cap(i);
			if (n > Held_bonus) n = Held_bonus;
			Row_bonus[i] = n;
			Bonus_box += Held_bonus - n;
			Held_value = old_v;
			Held_bonus = old_b;
			Holding = old_v != 0;
			if (!Holding) {
				Bonus_box += Held_bonus;
				Held_bonus = 0;
			}
		}
	}
}

static void s3_slot(fe_screen *s, int id) { s3_click(s, id); }

static void number_text(char *buf, int value, int bonus)
{
	if (value)
		sprintf(buf, bonus ? "%d+%d" : "%d", value, bonus);
	else
		sprintf(buf, "+%d", bonus);
}

static void s3_draw(fe_screen *s)			// 0x88C18, 0x8A275, 0x88E3B
{
	// p3<sex><race>: the figure of the character (0x14000E)
	static char *race_code[6] = { "hu", "he", "el", "dr", "dw", "ha" };
	static short pos[12][2] = { {26,208}, {26,208}, {25,207}, {26,206}, {22,205}, {25,207},
		{24,205}, {23,205}, {23,203}, {23,204}, {23,206}, {24,203} };
	char name[12], buf[80];
	int i, trans, label_w = 0, w, h, lo[6], hi[6];
	grs_bitmap *bm;

	sprintf(name, "p3%c%s", Sex ? 'f' : 'm', race_code[Race]);
	bm = fe_egg_bitmap(name, &trans);
	if (bm)
		fe_blit(bm, pos[Race * 2 + Sex][0], pos[Race * 2 + Sex][1], trans);

	for (i = 0; i < 6; i++)
		if (Src[i]) {
			sprintf(buf, "%d", Src[i]);
			fe_string_in(Fe_font_m, 168, 72 + 39 * i, 96, 38, FE_HUE_1, FE_HUE_BLACK, buf);
		}

	for (i = 0; i < 6; i++) {
		fe_string_size(Fe_font_m, fe_text(655 + i), &w, &h);
		if (w > label_w) label_w = w;
	}
	stat_limits(lo, hi);
	for (i = 0; i < 6; i++) {
		int y = 121 + 31 * i, final, pct, x;

		fe_string(Fe_font_m, 304, y + (31 - Fe_font_m->ft_h) / 2, FE_HUE_5, FE_HUE_BLACK, fe_text(655 + i));
		if (!Row_value[i])
			continue;
		row_final(i, &final, &pct);
		if (row_exceptional(i) && final >= 18)
			sprintf(buf, "%d/%02d", final, pct);
		else
			sprintf(buf, "%d", final);
		fe_string(Fe_font_m, 304 + label_w + 3, y + (31 - Fe_font_m->ft_h) / 2,
			Row_bonus[i] ? FE_HUE_7 : FE_HUE_1, FE_HUE_BLACK, buf);

		// the information column: (419, y) 204x31, small font
		x = 419;
		y += (31 - Fe_font_s->ft_h) / 2;
		if (final < lo[i] && !Cheat_stats) {
			sprintf(buf, fe_text(620), lo[i]);
			fe_string(Fe_font_s, x, y, FE_HUE_4, -1, buf);
		} else if (final > hi[i] && !Cheat_stats) {
			sprintf(buf, fe_text(621), hi[i]);
			fe_string(Fe_font_s, x, y, FE_HUE_4, -1, buf);
		} else {
			int v = clamp_stat(final), k;
			char num[2][24], *label[2];
			int n = 1;

			label[1] = NULL;
			switch (i) {
			case ST_STR:
				sprintf(num[0], "%+d", str_to_hit(final, pct)); label[0] = fe_text(622);
				sprintf(num[1], " %+d", str_damage(final, pct)); label[1] = fe_text(623);
				n = 2;
				break;
			case ST_DEX: sprintf(num[0], "%+d", Dex_armor[v]); label[0] = fe_text(624); break;
			case ST_CON: sprintf(num[0], "%+d", Con_hp[is_fighter()][v]); label[0] = fe_text(625); break;
			case ST_INT: sprintf(num[0], "%d", Int_spells[v]); label[0] = fe_text(626); break;
			case ST_WIS:
				// The executable prints four numbers with "st, ", "nd, ",
				// "rd, " and "th ". The port leaves out the levels with
				// no bonus spell, so that the line stays in its column.
				{
					static char *suffix[4] = { "st", "nd", "rd", "th" };
					int lv;
					num[0][0] = 0;
					for (lv = 0; lv < 4; lv++)
						if (Wis_bonus[lv][v])
							sprintf(num[0] + strlen(num[0]), "%s%d%s", num[0][0] ? "," : "", Wis_bonus[lv][v], suffix[lv]);
					if (!num[0][0])
						strcpy(num[0], "0");
				}
				label[0] = fe_text(627);
				break;
			default: sprintf(num[0], "%+d", Cha_react[v]); label[0] = fe_text(628); break;
			}
			for (k = 0; k < n; k++) {
				fe_string(Fe_font_s, x, y, FE_HUE_3, -1, num[k]);
				fe_string_size(Fe_font_s, num[k], &w, &h);
				x += w;
				fe_string(Fe_font_s, x, y, FE_HUE_1, -1, label[k]);
				fe_string_size(Fe_font_s, label[k], &w, &h);
				x += w;
			}
		}
	}

	// the bonus box (452,352) 80x48
	if (Generated) {
		sprintf(buf, "%d", Bonus_box);
		fe_string_in(Fe_font_m, 452, 352, 80, 48, FE_HUE_6, FE_HUE_BLACK, buf);
	}

	// the number that the cursor holds (0x89FE9): a framed number at the cursor
	if (Holding) {
		number_text(buf, Held_value, Held_bonus);
		fe_string_size(Fe_font_m, buf, &w, &h);
		fe_rect(Fe_mx + 14, Fe_my - 2, w + 6, h + 4, fe_hue(FE_HUE_7));
		fe_rect(Fe_mx + 15, Fe_my - 1, w + 4, h + 2, fe_hue(FE_HUE_BLACK));
		fe_string(Fe_font_m, Fe_mx + 17, Fe_my, Held_value ? (Held_bonus ? FE_HUE_7 : FE_HUE_1) : FE_HUE_6, -1, buf);
	}
}

// Checks the six rows. Returns NULL if they are valid, or the message (0x88D0E).
static char *s3_check(void)
{
	int i, lo[6], hi[6], final, pct;
	char cw[64], who[100];

	for (i = 0; i < 6; i++)
		if (!Row_value[i])
			return fe_text(718);
	if (Cheat_stats)
		return NULL;
	stat_limits(lo, hi);
	for (i = 0; i < 6; i++) {
		row_final(i, &final, &pct);
		if (final < lo[i] || final > hi[i]) {
			class_words(cw);
			sprintf(who, "%s %s", fe_text(693 + Race), cw);
			sprintf(Help_buf, fe_text(719), who, fe_text(683 + i),
				fe_text(final < lo[i] ? 720 : 721), final < lo[i] ? lo[i] : hi[i]);
			return Help_buf;
		}
	}
	return NULL;
}

static void s3_store(void)			// 0x8AAA7
{
	int i, pct;

	// what the cursor holds goes back
	if (Holding) {
		Bonus_box += Held_bonus;
		if (Held_value)
			for (i = 0; i < 6; i++)
				if (!Src[i]) { Src[i] = Held_value; break; }
		Holding = Held_value = Held_bonus = 0;
	}
	Final_percentile = 1;
	for (i = 0; i < 6; i++) {
		row_final(i, &Final[i], &pct);
		if (i == ST_STR)
			Final_percentile = pct;
	}
	if (Final[ST_STR] < 18)
		Final_percentile = 1;
}

static int s3_may_leave(fe_screen *s)			// 0x88CE8
{
	if (Delta > 0) {
		char *msg = s3_check();
		if (msg) {
			fe_help_message(msg);
			fe_refuse();
			Delta = 0;
			return 0;
		}
		Stats_valid = 1;
	}
	s3_store();
	return 1;
}

static char *s3_help(int id)
{
	if (id == 14) return "Return to Character Class Screen";		// 0x13F79D
	if (id == 15) {
		char *msg = s3_check();
		return msg ? msg : fe_text(631);
	}
	return NULL;
}

static fe_control S3_controls[4] = {
	{ 0, 0, NULL, 38, 59, "btn3ulc" }, { 0, 0, NULL, 571, 1, "btn3urc" },
	{ 0, 0, NULL, 8, 391, "btn3llc" }, { 0, 0, NULL, 540, 390, "btn3lrc" } };

#define S3_SRC(i)	{ 168, 72 + 39 * (i), 263, 109 + 39 * (i), 0, 0, 0, -1, 1, 0, 0, 593, NULL, s3_slot, NULL, (i), 0 }
#define S3_ROW(i)	{ 304, 121 + 31 * (i), 567, 151 + 31 * (i), 0, 0, 0, -1, 1, 0, 0, 587 + (i), NULL, s3_slot, NULL, 6 + (i), 0 }

static fe_feature S3_features[19] = {
	S3_SRC(0), S3_SRC(1), S3_SRC(2), S3_SRC(3), S3_SRC(4), S3_SRC(5),
	S3_ROW(0), S3_ROW(1), S3_ROW(2), S3_ROW(3), S3_ROW(4), S3_ROW(5),
	{ 431, 341, 550, 408, 0, 0, 0, -1, 0, 0, 0, 594, NULL, s3_slot, NULL, 12, 0 },
	{ 0, 0, 0, 0, 604, 33, 37, 1, 1, KEY_ESC, 0, 583, NULL, NULL, h_abort, 13, 0 },
	{ 0, 0, 0, 0, 50, 433, 38, 2, 1, KEY_PAGEDOWN, 0, -1, s3_help, NULL, h_back, 14, 0 },
	{ 0, 0, 0, 0, 585, 434, 38, 3, 1, KEY_PAGEUP, 0, -1, s3_help, NULL, h_next, 15, 0 },
	{ 29, 51, 111, 172, 0, 0, 0, 0, 1, KEY_ENTER, KEY_SPACEBAR, 585, NULL, s3_generate, NULL, 16, 0 },
	{ 31, 210, 113, 326, 0, 0, 0, -1, 0, 0, 0, -1, help_im_a, NULL, NULL, 17, 0 },
	{ 292, 68, 572, 306, 0, 0, 0, -1, 0, 0, 0, 586, NULL, NULL, NULL, 18, 0 } };

static fe_screen S3 = { S3_controls, 4, { { 9, -1, -1 }, { 10, 11, 12 } }, S3_features, 19,
	105, 421, 528, 466, FE_SCR_CHAR3, 708, NULL, s3_draw, NULL, s3_may_leave, 0 };

//------------------------------------------------------------------------
// The randomizer: the shield with "?" (0x8B402)

static void h_random(fe_screen *s, int id)
{
	int i, c, k, n;

	if (!Chosen) {
		Race = fe_rand() % 6;
		Sex = fe_rand() % 2;
		Chosen = 1;
		free_bm(&Book_bm);
		Book_bm = fe_stream_frame(Race, -1);		// 0x13FEB6: the book at the page of the race
		Book_open = 1;
		free_bm(&Figure_bm);
		Figure_bm = fe_stream_frame(Figure_seq[Race][Sex], -1);
		clear_classes();
	}
	if (!Class_mask) {
		do Class_mask = ((fe_rand() % 14) + 1) & 15; while (!combo_allowed(Class_mask));
		for (c = 0; c < 4; c++) {
			free_bm(&Class_bm[c]);
			if (Class_mask & Class_bit[c])
				Class_bm[c] = fe_stream_frame(Class_sel_seq[c], -1);
		}
	}
	// 0x8B5E8: the mean of the fixed statistics of the classes
	s3_reset_slots();
	for (i = 0; i < 6; i++) {
		k = n = 0;
		for (c = 0; c < 4; c++)
			if (Class_mask & Class_bit[c]) {
				k += Quick_stats[c][i];
				n++;
			}
		Row_value[i] = k / n;
	}
	Percentile = fe_rand() % 100;
	Stats_valid = 1;
	Slots_stale = 0;
	Generated = 1;
	s->running = 0;
	Screen_index = 1;
	Delta = 1;
}

//------------------------------------------------------------------------
// Screen 4: name, portrait, alignment (0x141B68)

static fe_seq *portrait_seq(void)
{
	return fe_seq_get(Sex ? 22 : 21);		// 0x13FE04
}

static void s4_init(fe_screen *s)
{
	fe_seq *seq = portrait_seq();

	if (!seq || Portrait >= seq->n_frames)
		Portrait = 0;
	if ((Class_mask & CM_THIEF) && Alignment == 0)		// a thief cannot be Lawful Good
		Alignment = 4;
}

static void s4_draw(fe_screen *s)			// 0x8AB94
{
	// the pressed labels of the alignments (the radio buttons of ferbtn.c, 0x141604)
	static char *label[6] = { "lb4lg_de", "lb4ng_de", "lb4ln_de", "lb4nu_de", "lb4cg_de", "lb4cn_de" };
	static short label_pos[6][2] = { {423,163}, {422,202}, {422,238}, {422,274}, {422,310}, {422,347} };
	static char *race_code[6] = { "hu", "he", "el", "dr", "dw", "ha" };
	static short pos[12][2] = { {57,177}, {73,178}, {57,176}, {64,181}, {57,176}, {66,178},
		{61,178}, {61,179}, {42,177}, {68,179}, {63,179}, {70,176} };		// 0x14009E
	fe_seq *seq = portrait_seq();
	grs_bitmap *bm;
	char name[12];
	int trans;

	sprintf(name, "p4%c%s", Sex ? 'f' : 'm', race_code[Race]);
	bm = fe_egg_bitmap(name, &trans);
	if (bm)
		fe_blit(bm, pos[Race * 2 + Sex][0], pos[Race * 2 + Sex][1], trans);

	if (seq && Portrait < seq->n_frames)
		fe_blit_part(seq->frames[Portrait], 0, 0, 108, 119, 245, 145, -1);

	bm = fe_egg_bitmap(label[Alignment], &trans);
	if (bm)
		fe_blit(bm, label_pos[Alignment][0], label_pos[Alignment][1], trans);

	if (Name[0])
		fe_string_in(Fe_font_m, 46, 112, 147, 46, FE_HUE_7, FE_HUE_BLACK, Name);
}

static void s4_portrait(fe_screen *s, int id)			// 0x8AD4E, 0x8ADA6
{
	fe_seq *seq = portrait_seq();
	int n = seq ? seq->n_frames : 1;

	// The executable moves the new picture in from the side (0x8ADFA).
	// The port shows it at once.
	Portrait = (Portrait + (id == 3 ? 1 : n - 1)) % n;
}

static void s4_name(fe_screen *s, int id)			// 0x8B078
{
	char buf[16];

	strcpy(buf, Name);
	if (fe_input_box(fe_text(422), buf, 15) && buf[0])
		strcpy(Name, buf);
}

static void s4_align(fe_screen *s, int id)			// 0x8AF5B ...
{
	int a = id - 8;

	if (a == 0 && (Class_mask & CM_THIEF)) {
		fe_help_message("Thieves can not have a\nLawful Good alignment");		// 0x12D4CC
		fe_refuse();
		return;
	}
	Alignment = a;
}

static int s4_may_leave(fe_screen *s)			// 0x8AD1C
{
	if (Delta > 0 && !Name[0]) {
		fe_help_message(fe_text(774));
		fe_refuse();
		Delta = 0;
		return 0;
	}
	return 1;
}

static fe_control S4_controls[5] = {
	{ 0, 0, NULL, 567, 0, "btn4urc" }, { 0, 0, NULL, 0, 409, "btn4llc" }, { 0, 0, NULL, 551, 413, "btn4lrc" },
	{ 0, 0, NULL, 231, 306, "btn4lft" }, { 0, 0, NULL, 314, 306, "btn4rgt" } };

#define S4_ALIGN(i, ya, yb)	{ 428, ya, 569, yb, 0, 0, 0, -1, 1, 0x02 + (i), 0, 601 + (i), NULL, s4_align, NULL, 8 + (i), 0 }

static fe_feature S4_features[14] = {
	{ 0, 0, 0, 0, 603, 32, 38, 0, 1, KEY_ESC, 0, 583, NULL, NULL, h_abort, 0, 0 },
	{ 0, 0, 0, 0, 37, 448, 38, 1, 1, KEY_PAGEDOWN, 0, 595, NULL, NULL, h_back, 1, 0 },
	{ 552, 413, 625, 474, 0, 0, 0, 2, 1, 0, 0, 596, NULL, NULL, h_next, 2, 0 },
	{ 229, 300, 288, 363, 0, 0, 0, 3, 1, KEY_LEFT, 0, 598, NULL, NULL, s4_portrait, 3, 1 },
	{ 308, 300, 371, 363, 0, 0, 0, 4, 1, KEY_RIGHT, 0, 597, NULL, NULL, s4_portrait, 4, 1 },
	{ 46, 112, 192, 158, 0, 0, 0, -1, 0, 0x31, 0, 599, NULL, NULL, s4_name, 5, 0 },
	{ 226, 124, 372, 281, 0, 0, 0, -1, 0, 0, 0, 600, NULL, NULL, NULL, 6, 0 },
	{ 40, 182, 194, 385, 0, 0, 0, -1, 0, 0, 0, -1, help_im_a, NULL, NULL, 7, 0 },
	S4_ALIGN(0, 168, 191), S4_ALIGN(1, 205, 227), S4_ALIGN(2, 242, 263),
	S4_ALIGN(3, 278, 300), S4_ALIGN(4, 315, 336), S4_ALIGN(5, 351, 373) };

static fe_screen S4 = { S4_controls, 5, { { 13, -1, -1 }, { 14, 15, 16 } }, S4_features, 14,
	109, 414, 529, 461, FE_SCR_CHAR4, 709, s4_init, s4_draw, NULL, s4_may_leave, 0 };

//------------------------------------------------------------------------
// The new character (0x8BEBC and 0x8BB91)

// Thief skills (0x8BDE5): race index 0 human, 1 dwarf, 2 elf, 3 drow,
// 4 halfling, 5 half-elf. The second index is the dexterity.
static int Hide_race[6] = { 0, 0, 10, 10, 15, 5 };			// 0x1461C8
static int Move_race[6] = { 0, 0, 5, 5, 10, 0 };				// 0x1461E0
static int Lock_race[6] = { 0, 10, -5, -5, 5, 0 };			// 0x1461F8
static int Trap_race[6] = { 0, 15, 0, 0, 5, 0 };				// 0x146210
static int Climb_race[6] = { 0, -10, 0, 0, -15, 0 };			// 0x146228
static int Hide_dex[21] = { -50,-50,-50,-50,-50,-50,-50,-50,-50,-10,-5,0,0,0,0,0,0,5,10,15,50 };		// 0x146078
static int Move_dex[21] = { -50,-50,-50,-50,-50,-50,-50,-50,-50,-20,-15,-10,-5,0,0,0,0,5,10,15,50 };	// 0x1460CC
static int Lock_dex[21] = { -50,-50,-50,-50,-50,-50,-50,-50,-50,-10,-5,0,0,0,0,0,5,10,15,20,50 };		// 0x146120
static int Trap_dex[21] = { -50,-50,-50,-50,-50,-50,-50,-50,-50,-10,-10,-5,0,0,0,0,0,0,5,10,50 };	// 0x146174

// The first items of a class: type and number (0x13F6CD ...). Order:
// fighter, thief, mage, cleric.
static int Start_items[4][4][2] = {
	{ { OBJ_WEAPON, 20 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 2 }, { OBJ_MISC, 79 } },
	{ { OBJ_WEAPON, 19 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 13 }, { OBJ_MISC, 79 } },
	{ { OBJ_WEAPON, 18 }, { OBJ_MISC, 79 }, { 0, 0 }, { 0, 0 } },
	{ { OBJ_WEAPON, 27 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 79 }, { 0, 0 } } };

static int has_item(int type, int id)
{
	int i;

	for (i = 0; i < Dtu_n_inventory; i++)
		if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id)
			return 1;
	return 0;
}

static void make_player(void)
{
	// class code of dtuchar.h for the class index of this file (0x13FDDF)
	static int class_code[4] = { DTU_CLASS_FIGHTER, DTU_CLASS_THIEF, DTU_CLASS_MAGE, DTU_CLASS_CLERIC };
	static int race_code[6] = { DTU_RACE_HUMAN, DTU_RACE_HUMAN, DTU_RACE_ELF, DTU_RACE_DARK_ELF,
		DTU_RACE_DWARF, DTU_RACE_HALFLING };		// 0x13FDC3
	static int hit_die[4] = { 10, 6, 4, 8 };		// 0x13FDF3: fighter, thief, mage, cleric
	static char race_letter[6] = { 'h', 'h', 'e', 'k', 'd', 'f' };
	static char class_letter[4] = { 'f', 't', 'm', 'c' };
	dtu_player_attr *pa = &Dtu_char.pa;
	int cls[2], n = 0, c, i, hp[2], r, d;

	for (c = 0; c < 4; c++)
		if ((Class_mask & Class_bit[c]) && n < 2)
			cls[n++] = c;
	if (n == 1)
		cls[1] = -1;

	memset(&Dtu_char, 0, sizeof(Dtu_char));
	strncpy(Dtu_char.name, Name, sizeof(Dtu_char.name) - 1);
	Dtu_char.current.n = -1;
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		Dtu_char.hot_key[i].n = -1;
	for (i = 0; i < DTU_N_SLOTS; i++)
		Dtu_char.slot[i].n = -1;

	pa->class1 = (byte) class_code[cls[0]];
	pa->class2 = (byte) (cls[1] >= 0 ? class_code[cls[1]] : DTU_CLASS_NONE);
	pa->unk_02 = -1;
	pa->race = (byte) race_code[Race];
	pa->female = (ubyte) Sex;
	pa->spell_class = (ubyte) (((Class_mask & CM_CLERIC) ? 1 : 0) | ((Class_mask & CM_MAGE) ? 2 : 0));
	pa->age = 0x15;

	// 0x8BA3A: the hit points of the first level are the full hit die
	// and the adjustment of the constitution. Two classes: the mean.
	for (i = 0; i < n; i++)
		hp[i] = hit_die[cls[i]] + Con_hp[cls[i] == 0][clamp_stat(Final[ST_CON])];
	pa->max_hit_points = (short) (n == 2 ? (hp[0] + hp[1]) / 2 : hp[0]);
	if (Cheat_stats)
		pa->max_hit_points = Final[ST_STR] == 18 ? 999 : 1;
	pa->hit_points = pa->max_hit_points;
	pa->damage_dice = 2;
	pa->n_damage_dice = 1;
	pa->to_hit_bonus = (short) str_to_hit(Final[ST_STR], Final_percentile);
	pa->damage_bonus = (short) str_damage(Final[ST_STR], Final_percentile);
	pa->armor_class = 10;
	pa->dex_ac_adjust = (short) Dex_armor[clamp_stat(Final[ST_DEX])];
	pa->strength = (ushort) Final[ST_STR];
	pa->strength_extra = (ushort) Final_percentile;
	pa->wisdom = (ushort) Final[ST_WIS];
	pa->intelligence = (ushort) Final[ST_INT];
	pa->charisma = (ushort) Final[ST_CHA];
	pa->constitution = (ushort) Final[ST_CON];
	pa->dexterity = (ushort) Final[ST_DEX];
	pa->gold = 0x66;
	pa->flags = (Race == 1) ? DTU_PF_HALF_ELF : 0;
	pa->alignment = (short) Alignment;
	pa->unk_4e = -1;

	r = (Race == 1) ? 5 : race_code[Race];
	d = clamp_stat(Final[ST_DEX]);
	if (Class_mask & CM_THIEF) {
		pa->thief_ability = 1;
		pa->hide_in_shadows = 5 + Hide_race[r] + Hide_dex[d];
		pa->move_silently = 10 + Move_race[r] + Move_dex[d];
		pa->open_locks = 10 + Lock_race[r] + Lock_dex[d];
		pa->find_traps = 5 + Trap_race[r] + Trap_dex[d];
		pa->climb_walls = 60 + Climb_race[r];
	} else if (Race == 4)
		pa->find_traps = 50;			// a dwarf who is not a thief

	Dtu_char.base = *pa;
	Dtu_char.thac0 = 20;
	// The record of the executable has the frame number of FESEQ 21 or 22.
	// The in-game portraits (FEBGND 87 to 118) are 32 pictures. Picture 0
	// is frame 0 of the female brush (compared by eye), so the 14 female
	// pictures are first. That the 18 male pictures follow in the order
	// of their brush is a guess.
	Dtu_char.portrait = Sex ? Portrait : 14 + Portrait;
	Dtu_player_portrait = Dtu_char.portrait;
	Dtu_char.created = 1;

	// 0x8BB91: the items of the classes
	dtu_inventory_clear();
	Dtu_gold = pa->gold;
	dtu_inventory_add(OBJ_MISC, 102, 1);		// each character gets this scroll first (docs/re/character.md)
	for (i = 0; i < n; i++)
		for (c = 0; c < 4; c++) {
			int type = Start_items[cls[i]][c][0], id = Start_items[cls[i]][c][1];
			if (type && !has_item(type, id))
				dtu_inventory_add(type, id, 1);
		}
	if (Class_mask & CM_MAGE) {
		// 13 spells in a random order (0x87D66), the first INT / 3 of them, and spell 46
		static int spells[13] = { 2, 4, 5, 6, 12, 15, 27, 30, 33, 39, 45, 46, 48 };
		int list[13], count = (Final[ST_INT] > 18 ? 18 : Final[ST_INT]) / 3;
		memcpy(list, spells, sizeof(list));
		for (i = 0; i < 13; i++) {
			int j = fe_rand() % 13, t = list[i];
			list[i] = list[j];
			list[j] = t;
		}
		if (count > 6) count = 6;
		for (i = 0; i < 6; i++) {
			int id = i < count ? list[i] : 46;
			if (!has_item(OBJ_SPELL, id))
				dtu_inventory_add(OBJ_SPELL, id, 1);
		}
	}
	if (Class_mask & CM_CLERIC)
		if (!has_item(OBJ_SPELL, 0))
			dtu_inventory_add(OBJ_SPELL, 0, 1);		// 0xB75A8(0)
	dtu_display_inventory_reset();			// 0x8BC69

	dtu_char_update();

	// The archive of the player model: race, sex, first class (the names
	// HMC.EGG, EFT.EGG ... of the game data).
	sprintf(Dtu_player_archive, "%c%c%c", race_letter[Race], Sex ? 'f' : 'm', class_letter[cls[0]]);
	strncpy(Players[Player_num].callsign, Name, CALLSIGN_LEN);
	Players[Player_num].callsign[CALLSIGN_LEN] = 0;

	dtu_log("Front end: new character '%s': %s %s, class mask %d, STR %d/%02d DEX %d CON %d INT %d WIS %d CHA %d, %d hit points, archive %s\n",
		Name, Sex_word[Sex], Race_word[Race], Class_mask, Final[ST_STR], Final_percentile, Final[ST_DEX], Final[ST_CON],
		Final[ST_INT], Final[ST_WIS], Final[ST_CHA], pa->max_hit_points, Dtu_player_archive);
}

//------------------------------------------------------------------------
// 0x87D73

int fe_new_character(void)
{
	static fe_screen *screens[4] = { &S1, &S2, &S3, &S4 };
	int i;

	Continue = 1;
	Screen_index = 0;
	Delta = 0;
	Race = Sex = Chosen = Class_mask = Stats_valid = Alignment = Portrait = 0;
	Book_open = Cheat_stats = Slots_stale = Generated = 0;
	Name[0] = 0;
	Percentile = 1;
	s3_reset_slots();
	Bonus_box = 0;
	free_bm(&Book_bm);
	free_bm(&Figure_bm);
	for (i = 0; i < 4; i++)
		free_bm(&Class_bm[i]);

	while (Continue && Screen_index >= 0 && Screen_index < 4 && !Fe_quit) {
		// A character whose race, sex or class changed needs new statistics.
		if (Screen_index == 2 && Slots_stale) {
			s3_reset_slots();
			Bonus_box = 0;
			Slots_stale = Generated = 0;
		}
		fe_run_screen(screens[Screen_index]);
		Screen_index += Delta;
		Delta = 0;
	}

	free_bm(&Book_bm);
	free_bm(&Figure_bm);
	for (i = 0; i < 4; i++)
		free_bm(&Class_bm[i]);

	if (!Continue || Fe_quit || Screen_index < 4)
		return 0;
	make_player();
	return 1;
}
