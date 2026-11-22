/*
 * ASCENT: new file. The spells of Descent to Undermountain (DTUGAME.EXE
 * 1.3: spells.c 0x8CFFF, 0x90CEF, 0x9002C, 0x91BC3, 0x911AA; game.c
 * 0x41A4D; laser.c 0x56A0D, 0x56FF6; displays.c 0x3575B, 0x3551B, 0xB7084;
 * collide.c 0xAE596, 0x2A849; 0xAFB55). See dtuspell.h and
 * docs/re/combat.md, part B.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "vclip.h"
#include "weapon.h"
#include "player.h"
#include "gauges.h"
#include "gamefont.h"
#include "gameseq.h"
#include "digi.h"
#include "kconfig.h"		// ASCENT: Config_digi_volume (0x13BFEB)
#include "fireball.h"
#include "render.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtucombat.h"
#include "dtuspell.h"

void make_random_vector(vms_vector *vec);		// ai.c

int Dtu_fix_chill_touch = 1;
int Dtu_fix_stoneskin_word = 1;
int Dtu_fix_rary_manual = 1;
int Dtu_fix_melf_acid = 1;

// Rary's Mnemonic Enhancer: the extra mage slots of spell levels 1 to 3
// (P+0x23 to P+0x25 of the executable). Rary_rest: the number of rests
// since the last cast (the rule of the manual).
static int Rary_bonus[3];
static int Rary_rest = 0;
static int Wizardry_n = 0;			// doublings of the Ring of Wizardry
// Spell_info[46]+0x49: the pool of Sleep. 0x56FF6 sets it to
// 4 + 2 (rand() % 7) (4 to 16, even) at each cast of a Sleep missile
// (0x5742C), the player's or a creature's; 0x8F89E takes the cost of each
// sleeper.
static int Sleep_pool = 0;

static int rand_mod(int n);
static int creature_cleric_level(object *obj);
static int creature_rf_name(object *obj);
static int creature_interrupt(object *obj,int hit);
static void rary_apply(int choice);

int Dtu_spell_current = -1;
int Dtu_spell_book_n[2];
short Dtu_spell_book[2][DTU_SPELL_BOOK_MAX];
int Dtu_spell_slots_max[2][9];
int Dtu_spell_slots_left[2][9];

#define MINUTE		(60*F1_0)		// one round of AD&D: 60.0 seconds of game time

static fix Cast_timer = DTU_FX_NO_END;	// 0x153EBC
static int Cast_spell = -1;
static int Hand_ref = -2;				// Dtu_char.current.n when the spell was taken in the hand
static int Screen_sel = 0;

void dtu_spell_log(char *format, ...)
{
	char buf[400];
	va_list args;

	va_start(args,format);
	vsnprintf(buf,sizeof(buf),format,args);
	va_end(args);
	dtu_obj_log("%s",buf);
}

//------------------------------------------------------------------------
// The spell table

static int spell_ok(int spell)
{
	return spell >= 0 && spell < N_spell_types && Spell_info[spell].record >= 0;
}

static int spell_int(int spell,char *key,int def)
{
	char v[32];

	if (!spell_ok(spell) || !bm_tbl_get(Spell_info[spell].record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

// The cast time in seconds of game time. The table fixup 0x1A344, called at
// the end of each table read (0x1C8F9), writes the words of 0x13A8DB over
// the cast_time= value (Spell_info+0x04, word 0x1B567A) of each spell line
// read, so cast_time= of the tables has no effect in 1.3. The values are the
// AD&D casting times: a number for segments, 60 for a round, 600 for a turn.
// Checked in DOSBox-X: Armor says "Casting time: 60 seconds ..." and acts
// 60 s later; Shield (cast_time=10 in the table) says "1 second".
static const short Cast_time_13[56] = {
	7, 5, 60, 60, 1, 1, 1, 4, 6, 2,			// 0 to 9
	5, 7, 1, 3, 4, 1, 3, 7, 5, 3,			// 10 to 19
	7, 3, 3, 5, 4, 60, 2, 1, 1, 2,			// 20 to 29
	1, 4, 3, 1, 4, 60, 2, 3, 6, 1,			// 30 to 39
	6, 600, 60, 5, 2, 1, 1, 3, 1, 1,		// 40 to 49
	600, 5, 5, 4, 60, 6,				// 50 to 55
};

static int spell_cast_time(int spell)
{
	if (spell >= 0 && spell < (int)(sizeof(Cast_time_13) / sizeof(Cast_time_13[0])) && spell < N_spell_types)
		return Cast_time_13[spell];
	return spell_int(spell,"cast_time",0);
}

static fix spell_fix(int spell,char *key,fix def)
{
	char v[32];

	if (!spell_ok(spell) || !bm_tbl_get(Spell_info[spell].record,key,v,sizeof(v)))
		return def;
	return fl2f(atof(v));
}

static int dtu_tbl_int(int record,char *key,int def)
{
	char v[32];

	if (record < 0 || !bm_tbl_get(record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

static int spell_flag(int spell,char *flag)
{
	return spell_ok(spell) && bm_tbl_has_flag(Spell_info[spell].record,flag);
}

static char *spell_name(int spell)
{
	return spell_ok(spell) ? dtu_tr_name(Spell_info[spell].name) : "?";		// ASCENT: the language (dtulang.h)
}

// Spell_info +0x36 bit 0: type=AREA
static int spell_area(int spell)
{
	char v[16];

	return spell_ok(spell) && bm_tbl_get(Spell_info[spell].record,"type",v,sizeof(v)) && !stricmp(v,"AREA");
}

// Spell_info +0x37 bit 0: the cast on the caster (0x56D36 for the player,
// 0x92104 for a creature). The reader sets it for self_cast_ok (0x25535,
// |= 1) and for cast_in_front (0x253C7, |= 5; no code tests bit 2). So a
// cast_in_front line (8, 9, 13, 30, 31 in 40 of the 46 tables) is cast on
// the caster, an AREA line of it around the caster.
static int spell_self(int spell)
{
	if (spell == 5 && Dtu_fix_chill_touch)		// ASCENT: Chill Touch on the caster (dtuspell.h)
		return 1;
	return spell_flag(spell,"self_cast_ok") || spell_flag(spell,"cast_in_front");
}

static int spell_level(int spell)
{
	return spell_int(spell,"level",1);
}

// 0 mage, 1 cleric, -1 none (the class bits of +0x00)
static int spell_book_of(int spell)
{
	if (spell_flag(spell,"mage_use")) return 0;
	if (spell_flag(spell,"cleric_use")) return 1;
	return -1;
}

static int timer_ms(fix t)
{
	return f2i(t);
}

//------------------------------------------------------------------------
// The character

static int char_is(int cls)
{
	return Dtu_char.created && (Dtu_char.pa.class1 == cls || Dtu_char.pa.class2 == cls);
}

// The level (0 based) of a class of the character, or -1.
static int char_class_level(int cls)
{
	if (!Dtu_char.created) return -1;
	if (Dtu_char.pa.class1 == cls) return Dtu_char.pa.class1_level;
	if (Dtu_char.pa.class2 == cls) return Dtu_char.pa.class2_level;
	return -1;
}

// 0x91EC1: the caster level of the player for a spell, from 1. The same
// for a spell of a book, a scroll and a wand (0x56D4E, 0x57054, 0x8D1C4,
// 0x90D01). c: the level of a cleric class (class byte 0), o: the level of
// the other class (any class byte that is not 0: mage, fighter or thief;
// the second class wins over the first), 0 if none. A spell with both
// cleric_use (+0x00 bit 0) and mage_use (bit 1): max(c, o) + 1; cleric_use:
// c + 1; else o + 1. So a fighter or a thief casts a mage wand or scroll at
// the level of the class, and a fighter/thief at the thief level. With the
// class order of the creation (fighter, thief, mage, cleric), o of a
// character with a mage class is always the mage level.
int dtu_spell_player_level(int spell)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int c = 0, o = 0;

	if (!Dtu_char.created)
		return 1;
	if (pa->class1 == DTU_CLASS_CLERIC) c = pa->class1_level;
	else o = pa->class1_level;
	if ((signed char) pa->class2 != -1) {
		if (pa->class2 == DTU_CLASS_CLERIC) c = pa->class2_level;
		else o = pa->class2_level;
	}
	if (spell_flag(spell,"cleric_use") && spell_flag(spell,"mage_use"))
		return (c > o ? c : o) + 1;
	if (spell_flag(spell,"cleric_use"))
		return c + 1;
	return o + 1;
}

// 0xAE3BB: the level of a creature: max hit points / 8, 1 to 18
static int creature_level(object *obj)
{
	int l = 1;

	if (obj->id < N_robot_types)
		l = Monster_info[obj->id].max_hit_points / 8;
	if (l < 1) l = 1;
	if (l > 18) l = 18;
	return l;
}

static int object_level(object *obj,int spell)
{
	if (obj == NULL) return 1;
	if (obj->type == OBJ_PLAYER) return dtu_spell_player_level(spell);
	if (obj->type == OBJ_ROBOT) return creature_level(obj);
	return 1;
}

static int monster_flags(object *obj)
{
	return (obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types) ? Monster_info[obj->id].flags : 0;
}

static int monster_has(object *obj,char *flag)
{
	return obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types && bm_tbl_has_flag(Monster_info[obj->id].record,flag);
}

// A number of the $MONSTER line of a creature. (The key of the cleric
// level, Robot_info+0xE3, is a guess.)
static int monster_int(object *obj,char *key,int def)
{
	char v[32];

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types || !bm_tbl_get(Monster_info[obj->id].record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

static char *target_name(object *obj)
{
	if (obj == NULL) return "nothing";
	if (obj->type == OBJ_PLAYER) return "you";
	return dtu_object_name(obj);
}

//------------------------------------------------------------------------
// Spell slots (0x3575B, 0x3551B)

// 0x145CD8: the AD&D wizard table, 18 rows of 9 (row = mage level, 0 based)
static const ubyte Mage_slots[18][9] = {
	{1},{2},{2,1},{3,2},{4,2,1},{4,2,2},{4,3,2,1},{4,3,3,2},{4,3,3,2,1},
	{4,4,3,2,2},{4,4,4,3,3},{4,4,4,4,4,1},{5,5,5,4,4,2},{5,5,5,4,4,2,1},
	{5,5,5,5,5,2,1},{5,5,5,5,5,3,2,1},{5,5,5,5,5,3,3,2},{5,5,5,5,5,3,3,2,1}
};
// 0x145DF8: the AD&D priest table, 18 rows of 7
static const ubyte Cleric_slots[18][7] = {
	{1},{2},{2,1},{3,2},{3,3,1},{3,3,2},{3,3,2,1},{3,3,3,2},{4,4,3,2,1},
	{4,4,3,3,2},{5,4,4,3,2,1},{6,5,5,3,2,2},{6,6,6,4,2,2},{6,6,6,5,3,2,1},
	{6,6,6,6,4,2,1},{7,7,7,6,4,3,1},{7,7,7,7,5,3,2},{8,8,8,8,6,4,2}
};

// 0x1403D2 to 0x1404C2: the bonus of the wisdom for the cleric spell
// levels 1 to 4 (index wisdom 0 to 19)
static int wisdom_bonus(int level,int wis)
{
	if (wis < 0) wis = 0;
	if (wis > 19) wis = 19;
	switch (level) {
		case 1: return wis >= 19 ? 3 : wis >= 14 ? 2 : wis >= 13 ? 1 : 0;
		case 2: return wis >= 16 ? 2 : wis >= 15 ? 1 : 0;
		case 3: return wis >= 19 ? 2 : wis >= 17 ? 1 : 0;
		case 4: return wis >= 18 ? 1 : 0;
	}
	return 0;
}

void dtu_spell_compute_slots(void)
{
	int m = char_class_level(DTU_CLASS_MAGE), c = char_class_level(DTU_CLASS_CLERIC), i;

	memset(Dtu_spell_slots_max,0,sizeof(Dtu_spell_slots_max));
	if (m >= 0) {
		if (m > 17) m = 17;
		for (i=0;i<9;i++)
			Dtu_spell_slots_max[0][i] = Mage_slots[m][i];
	}
	if (c >= 0) {
		if (c > 17) c = 17;
		for (i=0;i<7;i++) {
			Dtu_spell_slots_max[1][i] = Cleric_slots[c][i];
			if (Cleric_slots[c][i] > 0 && i < 4)
				Dtu_spell_slots_max[1][i] += wisdom_bonus(i+1,Dtu_char.pa.wisdom);
		}
	}
	// The Ring of Wizardry doubles P+0x23 to P+0x25 (0xB00A5), Rary's
	// Mnemonic Enhancer adds to them (0x8F2CC)
	for (i=0;i<3;i++) {
		int n = Wizardry_n;
		while (n-- > 0 && Dtu_spell_slots_max[0][i] < 127)
			Dtu_spell_slots_max[0][i] *= 2;
		Dtu_spell_slots_max[0][i] += Rary_bonus[i];
	}
	for (i=0;i<9;i++) {
		if (Dtu_spell_slots_left[0][i] > Dtu_spell_slots_max[0][i]) Dtu_spell_slots_left[0][i] = Dtu_spell_slots_max[0][i];
		if (Dtu_spell_slots_left[1][i] > Dtu_spell_slots_max[1][i]) Dtu_spell_slots_left[1][i] = Dtu_spell_slots_max[1][i];
	}
}

void dtu_spell_restore_slots(void)
{
	// The rule of the manual for Rary's Mnemonic Enhancer: the first rest
	// after the cast fills the bonus slots, the next rest ends the bonus.
	// The executable keeps the bonus (bit 22 ends only with force).
	if (Dtu_fix_rary_manual && ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_MNEMONIC)) {
		if (++Rary_rest >= 2) {
			dtu_spell_log("spell: the rest ends Rary's Mnemonic Enhancer (Dtu_fix_rary_manual)");
			dtu_spell_clear_effect(ConsoleObject,DTU_FX_MNEMONIC);
			HUD_init_message("Rary's Mnemonic Enhancer expired.");
			memset(Rary_bonus,0,sizeof(Rary_bonus));
			Rary_rest = 0;
			dtu_spell_compute_slots();
		}
	}
	memcpy(Dtu_spell_slots_left,Dtu_spell_slots_max,sizeof(Dtu_spell_slots_left));
	dtu_spell_log("spell: the slots are full again (mage %d %d %d, cleric %d %d %d)",
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_left[0][2],
		Dtu_spell_slots_left[1][0],Dtu_spell_slots_left[1][1],Dtu_spell_slots_left[1][2]);
}

// 0xB69AD
int dtu_spell_learn(int spell)
{
	int b, i;

	if (!spell_ok(spell))
		return 1;
	if (spell_flag(spell,"mage_use") && char_is(DTU_CLASS_MAGE))
		b = 0;
	else if (spell_flag(spell,"cleric_use") && char_is(DTU_CLASS_CLERIC))
		b = 1;
	else
		return 1;
	for (i=0;i<Dtu_spell_book_n[b];i++)
		if (Dtu_spell_book[b][i] == spell)
			return 2;
	if (Dtu_spell_book_n[b] >= DTU_SPELL_BOOK_MAX)
		return 1;
	Dtu_spell_book[b][Dtu_spell_book_n[b]++] = spell;
	return 0;
}

// 0xB65BB, type 17 (0xB660C to 0xB667F): a spell with the cleric bit for a
// cleric or the mage bit for a mage goes to a spell list (0xB69AD). -1: not
// for the lists (the item list takes it); else the result of 0xB69AD: 0
// added, 1 refused (the insert ends), 2 already in the list (the item list
// takes it, a scroll).
int dtu_spell_insert(int spell)
{
	if (!spell_ok(spell))
		return -1;
	if (!(spell_flag(spell,"cleric_use") && char_is(DTU_CLASS_CLERIC)) && !(spell_flag(spell,"mage_use") && char_is(DTU_CLASS_MAGE)))
		return -1;
	return dtu_spell_learn(spell);
}

// 0xB75A8(L): every spell with the cleric bit and the level L + 1 (the
// byte +0x39, 1 to 7) goes to the cleric list, if it is not there and the
// list has less than 127 entries. L above 4 or below 0 gives nothing.
static int grant_cleric_level(int L)
{
	int s, i, n = 0;

	if (L < 0 || L > 4)
		return 0;
	for (s=0;s<N_spell_types;s++) {
		int lv = spell_int(s,"level",0);

		if (!spell_ok(s) || !spell_flag(s,"cleric_use") || lv < 1 || lv > 7 || lv != L + 1)
			continue;
		if (Dtu_spell_book_n[1] >= DTU_SPELL_BOOK_MAX)
			break;
		for (i=0;i<Dtu_spell_book_n[1];i++)
			if (Dtu_spell_book[1][i] == s)
				break;
		if (i < Dtu_spell_book_n[1])
			continue;
		Dtu_spell_book[1][Dtu_spell_book_n[1]++] = s;
		dtu_spell_log("spell: '%s' (%d), cleric spell level %d, goes to the cleric list (0xB75A8)",spell_name(s),s,lv);
		n++;
	}
	return n;
}

// The new character (0x8BDB4) and each new level (0xAF8F5): a cleric gets
// all cleric spells of the spell levels 0x145ED8[0] to 0x145ED8[c] + 1, c
// the cleric level (0 based): spell level 1 at the cleric levels 1 and 2,
// 2 at 3 and 4, 3 at 5 and 6, 4 at 7 and 8, 5 at 9 and 10 (no table line
// has a cleric spell above level 4); 0xB75A8 refuses the values 5 and 6 of
// the higher levels.
int dtu_spell_grant_cleric(void)
{
	static const ubyte table[32] = { 0,0,1,1,2,2,3,3,4,4,5,5,5,6,6,6, 6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6 };
	int c = char_class_level(DTU_CLASS_CLERIC), k, n = 0;

	if (c < 0)
		return 0;
	for (k=0;k<=c && k<32;k++)
		n += grant_cleric_level(table[k]);
	dtu_spell_log("spell: the cleric of level %d gets %d new cleric spells, %d in the list",c + 1,n,Dtu_spell_book_n[1]);
	return n;
}

static int in_book(int spell)
{
	int b, i;

	for (b=0;b<2;b++)
		for (i=0;i<Dtu_spell_book_n[b];i++)
			if (Dtu_spell_book[b][i] == spell)
				return 1;
	return 0;
}

int dtu_spell_take(int spell)
{
	if (!in_book(spell))
		return 0;
	Dtu_spell_current = spell;
	Hand_ref = Dtu_char.current.n;
	dtu_spell_log("spell: '%s' (%d) is in the hand",spell_name(spell),spell);
	return 1;
}

int dtu_spell_select(int spell)
{
	if (!dtu_spell_take(spell))
		return 0;
	HUD_init_message("%s",spell_name(spell));
	return 1;
}

// The two spell lists of a save game of the original (P+0x8F3 and
// P+0x10E3, counts at P+0xF7 and P+0xFB, 16 bytes for each entry with the
// spell number in byte 1), and the spell slots: P+0x23 mage maximum and
// P+0x2C mage slots left (spell levels 1 to 9), P+0x35 cleric maximum and
// P+0x3C cleric slots left (levels 1 to 7), one byte each.
#define P_BOOK_COUNT(b)		(0xF7 + 4*(b))
#define P_BOOK_LIST(b)		((b) ? 0x10E3 : 0x8F3)
#define P_SLOTS_MAX(b)		((b) ? 0x35 : 0x23)
#define P_SLOTS_LEFT(b)		((b) ? 0x3C : 0x2C)

// The entries of the last loaded record: the save keeps their bytes.
static ubyte Book_record[2][DTU_SPELL_BOOK_MAX][16];
static int Book_record_n[2];

void dtu_spell_books_from_record(const ubyte *p)
{
	int b, i;

	for (b=0;b<2;b++) {
		const ubyte *list = p + P_BOOK_LIST(b);
		int n = p[P_BOOK_COUNT(b)] | (p[P_BOOK_COUNT(b)+1] << 8), levels = b ? 7 : 9;

		if (n < 0 || n > DTU_SPELL_BOOK_MAX) n = 0;
		Dtu_spell_book_n[b] = 0;
		Book_record_n[b] = n;
		for (i=0;i<n;i++) {
			memcpy(Book_record[b][i],list + 16*i,16);
			if (list[16*i] == OBJ_SPELL && spell_ok(list[16*i+1]))
				Dtu_spell_book[b][Dtu_spell_book_n[b]++] = list[16*i+1];
		}
		for (i=0;i<9;i++)
			Dtu_spell_slots_left[b][i] = i < levels ? p[P_SLOTS_LEFT(b) + i] : 0;
	}
	Dtu_spell_current = -1;
	dtu_spell_log("spell: %d mage spells and %d cleric spells from the save game",Dtu_spell_book_n[0],Dtu_spell_book_n[1]);
}

void dtu_spell_books_to_record(ubyte *p)
{
	int b, i, k;

	for (b=0;b<2;b++) {
		ubyte *list = p + P_BOOK_LIST(b);
		int levels = b ? 7 : 9;

		p[P_BOOK_COUNT(b)] = (ubyte)Dtu_spell_book_n[b];
		p[P_BOOK_COUNT(b)+1] = (ubyte)(Dtu_spell_book_n[b] >> 8);
		p[P_BOOK_COUNT(b)+2] = p[P_BOOK_COUNT(b)+3] = 0;
		for (i=0;i<DTU_SPELL_BOOK_MAX;i++) {
			ubyte *e = list + 16*i;
			int spell = i < Dtu_spell_book_n[b] ? Dtu_spell_book[b][i] : -1;

			memset(e,0,16);
			e[5] = '_';						// an empty entry of the executable
			e[6] = 1;
			if (spell < 0)
				continue;
			for (k=0;k<Book_record_n[b];k++)
				if (Book_record[b][k][0] == OBJ_SPELL && Book_record[b][k][1] == spell)
					break;
			if (k < Book_record_n[b])
				memcpy(e,Book_record[b][k],16);
			else {
				// 0xB69AD: the entry of a spell in a list (combat.md: letter K,
				// word 6 the spell level, byte 0x0B the sub use count)
				dtu_item_data d;

				if (dtu_item_get(OBJ_SPELL,spell,&d)) {
					e[2] = (ubyte)d.icon;
					e[3] = (ubyte)d.class_mask;
				}
				e[0] = OBJ_SPELL;
				e[1] = (ubyte)spell;
				e[5] = 'K';
				e[6] = (ubyte)spell_level(spell);
				e[8] = 1;
				e[11] = 1;
				e[14] = 1;					// modifier 1.0
			}
		}
		// The slots of a class that the character does not have stay as
		// the record had them (the executable fills them with the table
		// values of level 0: SG0.SG of a thief and mage has one cleric slot).
		if (!char_is(b ? DTU_CLASS_CLERIC : DTU_CLASS_MAGE))
			continue;
		for (i=0;i<levels;i++) {
			p[P_SLOTS_MAX(b) + i] = (ubyte)Dtu_spell_slots_max[b][i];
			p[P_SLOTS_LEFT(b) + i] = (ubyte)Dtu_spell_slots_left[b][i];
		}
	}
}

// The book and the index of a spell, for the reference of a save game
// (list 1 mage, 2 cleric). Returns 0 if no book has the spell.
int dtu_spell_book_ref(int spell, int *list, int *index)
{
	int b, i;

	for (b=0;b<2;b++)
		for (i=0;i<Dtu_spell_book_n[b];i++)
			if (Dtu_spell_book[b][i] == spell) {
				*list = b + 1;
				*index = i;
				return 1;
			}
	return 0;
}

// The spell of a reference of a save game, or -1.
int dtu_spell_of_ref(int list, int index)
{
	if (list < 1 || list > 2 || index < 0 || index >= Dtu_spell_book_n[list-1])
		return -1;
	return Dtu_spell_book[list-1][index];
}

// The icon of a spell (byte 2 of a list entry: icon= of the line, 0xB69AD)
int dtu_spell_icon(int spell)
{
	return spell_ok(spell) && Spell_info[spell].icon > 0 ? Spell_info[spell].icon : 0;
}

// The spell level (word 6 of a list entry)
int dtu_spell_level(int spell)
{
	return spell_level(spell);
}

char *dtu_spell_name(int spell)
{
	return spell_name(spell);
}

// 0x3533C for a spell entry. Spells 39, 40 and 53 always. Others: the
// character is a caster (0x359F4) and has a class of the class bits of the
// entry (0x352F7). The count test of 0x3533C passes for a list entry.
int dtu_spell_usable(int spell)
{
	if (spell == 39 || spell == 40 || spell == 53)
		return 1;
	if (!char_is(DTU_CLASS_MAGE) && !char_is(DTU_CLASS_CLERIC))
		return 0;
	return (spell_flag(spell,"mage_use") && char_is(DTU_CLASS_MAGE)) ||
		(spell_flag(spell,"cleric_use") && char_is(DTU_CLASS_CLERIC));
}

// The letter of the list entry of the spell in the hand (byte 5: K hands,
// Q wand, N scroll, B arrow, 0xB69AD), or 0 with no spell in the hand.
// 0x43EF8 takes it for the model of the arms.
int dtu_spell_hand_letter(void)
{
	int b, k, spell = Dtu_spell_current;

	if (spell < 0)
		return 0;
	for (b=0;b<2;b++)
		for (k=0;k<Book_record_n[b];k++)
			if (Book_record[b][k][0] == OBJ_SPELL && Book_record[b][k][1] == spell && Book_record[b][k][5] > ' ')
				return Book_record[b][k][5];
	if (spell_flag(spell,"cast_with_wand")) return 'Q';
	if (spell_flag(spell,"cast_with_scroll")) return 'N';
	if (spell_flag(spell,"cast_with_arrow")) return 'B';
	return 'K';
}

// ASCENT: the spell in the hand is put away (an item is taken in the hand)
void dtu_spell_put_away(void)
{
	if (Dtu_spell_current >= 0)
		dtu_spell_log("spell: '%s' (%d) is put away",spell_name(Dtu_spell_current),Dtu_spell_current);
	Dtu_spell_current = -1;
}

static void load_books_from_save(char *filename)
{
	FILE *f = fopen(filename,"rb");
	ubyte buf[DTU_SG_PLAYER_OFFSET + DTU_PLAYER_RECORD_SIZE];

	if (f == NULL)
		return;
	if (fread(buf,1,sizeof(buf),f) != sizeof(buf)) {
		fclose(f);
		return;
	}
	fclose(f);
	dtu_spell_books_from_record(buf + DTU_SG_PLAYER_OFFSET);
}

//------------------------------------------------------------------------
// The effect block (object 0x80 to 0xF8)

static fix *timer_ref(object *obj,int off)
{
	if (off >= 0x8C && off <= 0xBC) return &obj->dtu.m_8c[(off - 0x8C) / 4];
	if (off >= 0xC0 && off <= 0xC4) return &obj->dtu.m_c0[(off - 0xC0) / 4];
	if (off >= 0xC8 && off <= 0xDC) return &obj->dtu.m_c8[(off - 0xC8) / 4];
	if (off >= 0xE0 && off <= 0xE4) return &obj->dtu.m_e0[(off - 0xE0) / 4];
	if (off >= 0xE8 && off <= 0xF8) return &obj->dtu.m_e8[(off - 0xE8) / 4];
	Int3();
	return &obj->dtu.m_8c[0];
}

// The timer of each bit (0x9002C). 0: no timer.
static const short Fx_timer[32] = {
	0, 0x94, 0x98, 0x9C, 0, 0xA0, 0xA4, 0xA8, 0xAC, 0, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0, 0xC4,
	0xC8, 0xC8, 0xCC, 0xD0, 0xD4, 0xD8, 0, 0xE0, 0, 0xE4, 0xE8, 0xEC, 0xF0, 0, 0xF4, 0xF8
};

static const char *Fx_name[32] = {
	LN("Armor"), LN("Bless"), LN("Blindness"), LN("Chill Touch"), LN("Deafness"), LN("Confusion"), LN("Detect Magic"), LN("Feather Fall"),
	LN("Fear"), LN("Protection From Fire"), LN("Resist Fire"), LN("Flying"), LN("Free Action"), LN("Haste"), LN("Hold Person"), LN("Infravision"),
	LN("Improved Invisibility"), LN("Invisibility"), LN("Invulnerability"), LN("Jump"), LN("Find Traps"), LN("Levitate"), LN("Rary's Mnemonic Enhancer"), LN("Protection From Evil"),
	LN("Regeneration"), LN("Shield"), LN("Sleep"), LN("Slow"), LN("Spider Climb"), LN("Stoneskin"), LN("Strength"), LN("Magical Vestment")
};

// The texts "NAME expired." of 0x9002C (0x12E385 to 0x12E5E5). NULL: no
// text (Deafness, Feather Fall and Regeneration have none). The two
// invisibilities clear both bits before the test of bit 16 (0x906B9), so
// the text is always "Invisibility expired.".
static const char *Fx_expired[32] = {
	LN("Armor"), LN("Bless"), LN("Blinded"), LN("Chill Touch"), NULL, LN("Confusion"), LN("Detect Magic"), NULL,
	LN("Fear"), LN("Protection From Fire"), LN("Resist Fire"), LN("Flying"), LN("Free Action"), LN("Haste"), LN("Hold Person"), LN("Infravision"),
	LN("Invisibility"), LN("Invisibility"), LN("Invulnerability"), LN("Jump"), LN("Find Traps"), LN("Levitate"), LN("Rary's Mnemonic Enhancer"), LN("Protection From Evil"),
	NULL, LN("Shield"), LN("Sleep"), LN("Slow"), LN("Spider Climb"), LN("Stoneskin"), LN("Strength"), LN("Magical Vestment")
};

static int fx_bit(int mask)
{
	int b = 0;

	while (b < 32 && !(mask & (1 << b))) b++;
	return b;
}

// Sets an effect with a duration. Returns 0 if the effect is on already
// (each case of 0x8CFFF returns at once then). The message is the one of
// the case (say_effect).
static int fx_set(object *obj,int mask,fix duration)
{
	int b = fx_bit(mask);

	if (obj->dtu.m_88 & mask)
		return 0;
	obj->dtu.m_88 |= mask;
	if (Fx_timer[b])
		*timer_ref(obj,Fx_timer[b]) = (duration >= DTU_FX_NO_END) ? DTU_FX_NO_END : GameTime + duration;
	dtu_spell_log("spell: effect %s on obj %d %s for %.1f seconds",Fx_name[b],(int)(obj-Objects),target_name(obj),
		duration >= DTU_FX_NO_END ? -1.0 : f2fl(duration));
	return 1;
}

// 0x29F3E (a weapon hits a creature), 0x2A4FB (a weapon hits the player),
// 0x913AF (spell damage on a creature): with Armor (bit 0) the damage is
// taken from the pool +0x90 as well (the hit points take it in full); a
// pool below 1.0 becomes 0, and the next 0x9002C ends the effect with
// "Armor expired." and 0xAFB55 for the player.
void dtu_spell_armor_hit(object *obj,fix damage)
{
	if (!obj || !(obj->dtu.m_88 & DTU_FX_ARMOR) || damage <= 0)
		return;
	obj->dtu.m_8c[1] -= damage;
	if (obj->dtu.m_8c[1] < F1_0)
		obj->dtu.m_8c[1] = 0;
	dtu_spell_log("spell: the Armor pool of obj %d %s takes %.1f, left %.1f",(int)(obj-Objects),target_name(obj),
		f2fl(damage),f2fl(obj->dtu.m_8c[1]));
}

// The end of an effect (the end actions of 0x9002C)
static void fx_end(object *obj,int b)
{
	int mask = (b == 31) ? DTU_FX_VESTMENT : (1 << b);

	obj->dtu.m_88 &= ~mask;
	if (Fx_timer[b])
		*timer_ref(obj,Fx_timer[b]) = 0;
	switch (b) {
		case 1:		// Bless: the hit points of Aid go
			if (obj->type == OBJ_PLAYER && dtu_char_hit_points() > dtu_char_max_hit_points())
				dtu_player_damage(i2f(dtu_char_hit_points() - dtu_char_max_hit_points()));
			break;
		case 16: case 17:	// the two invisibilities end together
			obj->dtu.m_88 &= ~(DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE);
			break;
		case 8:		// Fear: the view of the player comes back (0x903D4, dtucombat.c)
			if (obj->type == OBJ_PLAYER)
				dtu_fear_view_end(obj);
			break;
		case 26:	// Sleep also ends the blindness
			obj->dtu.m_88 &= ~DTU_FX_BLIND;
			break;
		case 22:	// Rary's Mnemonic Enhancer: the extra slots go (0x90886)
			if (obj->type == OBJ_PLAYER) {
				memset(Rary_bonus,0,sizeof(Rary_bonus));
				Rary_rest = 0;
				dtu_spell_compute_slots();
			}
			break;
		case 30:	// Strength: the gains go (0x90BE4)
			if (obj->type == OBJ_PLAYER)
				dtu_char_strength_end();
			break;
	}
	if (obj->type == OBJ_ROBOT)
		dtu_spell_creature_event(obj,DTU_CSC_EXPIRED);		// 0x9002C sets +0x15F bit 3
	// 0x9002C plays sound 161 at the end of the invisibilities (the player
	// and the creatures) and sound 163 at the end of Invulnerability
	if (b == 16 || b == 17)
		digi_play_sample(161,F1_0);
	else if (b == 18)
		digi_play_sample(163,F1_0);
	if (obj->type == OBJ_PLAYER) {
		if (Fx_expired[b]) {
			HUD_init_message("%s expired.",dtu_tr(Fx_expired[b]));
			dtu_spell_log("spell: message: %s expired.",Fx_expired[b]);
		}
		dtu_char_update();
		dtu_spell_log("spell: the player has armor class %d",dtu_char_armor_class());
	}
	dtu_spell_log("spell: effect %s of obj %d %s ends at %.2f",Fx_name[b],(int)(obj-Objects),target_name(obj),f2fl(GameTime));
}

// 0x9002C: the timers of one object. force: all effects end (Dispel Magic).
static void fx_expire(object *obj,int force)
{
	int b, w = obj->dtu.m_88;

	if (w == 0)
		return;
	for (b=0;b<32;b++) {
		int mask = (b == 31) ? DTU_FX_VESTMENT : (1 << b), end = force;

		if (!(w & mask) || b == 24)		// bit 24, regeneration, has no end here
			continue;
		if (!end) {
			if (Fx_timer[b]) {
				fix t = *timer_ref(obj,Fx_timer[b]);
				end = (t != DTU_FX_NO_END && t < GameTime);
			}
			else if (b == 0)
				end = obj->dtu.m_8c[1] <= 0;		// the Armor pool +0x90
			else if (b == 9)
				end = obj->dtu.m_82 < 1;				// Protection From Fire
			else if (b == 29)
				end = obj->dtu.m_80 <= 0;				// Stoneskin
		}
		if (end && (obj->dtu.m_88 & mask))
			fx_end(obj,b);
	}
}

//------------------------------------------------------------------------
// Saving throws (0xAE596)

// 0x145692 and on: rows of 5 numbers by level band. Divisors (0x1456E2):
// cleric 3, mage 5, fighter 2, thief 4.
static const ubyte Save_cleric[4][5] = { {10,14,13,16,15}, {9,13,12,15,14}, {7,11,10,13,12}, {6,10,9,12,11} };
static const ubyte Save_mage[3][5] = { {14,11,13,15,12}, {13,9,11,13,10}, {11,7,9,11,8} };
static const ubyte Save_fighter[6][5] = { {14,16,15,17,17}, {13,15,14,16,16}, {11,13,12,13,14}, {10,12,11,12,13}, {8,10,9,9,11}, {7,9,8,8,10} };
static const ubyte Save_thief[3][5] = { {13,14,12,16,15}, {12,12,11,15,13}, {11,10,10,14,11} };

static int save_number(int cls,int level,int cat)
{
	if (level < 1) level = 1;
	if (level > 12) level = 12;
	switch (cls) {
		case DTU_CLASS_CLERIC:	return Save_cleric[(level-1)/3][cat];
		case DTU_CLASS_MAGE:		return Save_mage[(level-1)/5][cat];
		case DTU_CLASS_THIEF:	return Save_thief[(level-1)/4][cat];
		default:						return Save_fighter[(level-1)/2][cat];
	}
}

// ASCENT: the save number of the player for a category 0 to 4, from the
// class levels of the character (the test -chartest rules reads it).
int dtu_spell_player_save_number(int cat)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1, level, need;

	if (cat < 0 || cat > 4)
		cat = 4;
	level = l1 > l2 ? l1 : l2;
	need = save_number(pa->class1,level,cat);
	if (pa->class2 >= 0 && pa->class2 <= 3) {
		int n2 = save_number(pa->class2,level,cat);
		if (n2 < need) need = n2;
	}
	return need;
}

int dtu_spell_save(object *obj,int cat)
{
	int need = 20, roll = rand() % 20 + 1, level;

	if ((cat < 0 || cat > 4) && obj->type != OBJ_DRESSING)
		cat = 4;
	if (obj->type == OBJ_PLAYER) {
		dtu_player_attr *pa = &Dtu_char.pa;
		int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1;

		level = l1 > l2 ? l1 : l2;
		need = save_number(pa->class1,level,cat);
		if (pa->class2 >= 0 && pa->class2 <= 3) {
			int n2 = save_number(pa->class2,level,cat);
			if (n2 < need) need = n2;
		}
	}
	else if (obj->type == OBJ_ROBOT) {
		level = creature_level(obj);
		need = save_number(creature_cleric_level(obj) > 0 ? DTU_CLASS_CLERIC : DTU_CLASS_FIGHTER,level,cat);
		if (monster_has(obj,"magic_resistant"))
			need -= (level > 12 ? 12 : level) / 3;
	}
	else if (obj->type == OBJ_DRESSING && obj->id < N_dressing_types) {
		// The tables 0x1456E6, 0x1456E8, 0x1456EA by explode_sound, byte
		// [category - 5]; a magic object needs half
		static const ubyte t2702[2] = { 3, 2 }, t2218[2] = { 3, 14 }, tother[2] = { 7, 9 };
		int s = Dressing_info[obj->id].explode_sound, c = (cat >= 5 && cat <= 6) ? cat - 5 : 0;
		need = (s == 2702) ? t2702[c] : (s == 2218) ? t2218[c] : tother[c];
		if (Dressing_info[obj->id].flags & DRESSING_F_MAGIC)
			need /= 2;
	}
	else
		need = 10;		// the default of 0xAE596 for other types (a missile, a light)
	dtu_spell_log("save: obj %d %s, category %d: d20 %d against %d: %s",(int)(obj-Objects),target_name(obj),cat,roll,need,roll >= need ? "saved" : "failed");
	return roll >= need;
}

//------------------------------------------------------------------------
// Sounds and effects

static void sound_at(int sound,int segnum,vms_vector *pos,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS || segnum < 0 || segnum > Highest_segment_index)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_pos2(sound,segnum,0,pos,0,volume,i2f(256));
}

static void vclip_at(int segnum,vms_vector *pos,fix size,int vclip)
{
	if (vclip < 0 || vclip >= VCLIP_MAXNUM || size <= 0 || segnum < 0)
		return;
	if (Vclip[vclip].num_frames <= 0 || Vclip[vclip].play_time <= 0)
		return;
	object_create_explosion(segnum,pos,size,vclip);
}

//------------------------------------------------------------------------
// The damage of a spell (0x911AA with the roll 0x91BC3)

#define GROUP_NONE	-1
#define GROUP_FIRE	1
#define GROUP_ACID	2
#define GROUP_ELEC	3

// "group" and "count" are the dice count that the case of 0x8CFFF writes
// into the table before the roll (Fireball, Lightning Bolt, Melf's Acid
// Arrows).
static fix spell_deal(int spell,object *target,object *caster,int level,int dmg,int save);

static fix spell_damage(int spell,object *target,object *caster,int level,int group,int count,int extra)
{
	static char *keys[4][2] = {
		{ "damage_dice", "#damage_dice" }, { "fire_damage_dice", "#fire_damage_dice" },
		{ "acid_damage_dice", "#acid_damage_dice" }, { "elec_damage_dice", "#elec_damage_dice" } };
	int dmg, g;

	if (target->type != OBJ_ROBOT && target->type != OBJ_PLAYER)
		return 0;

	// A group with dice of 0 sides adds 0 for each die, as in 1.3 (the
	// divide error handler, see spell_dice in dtuobj.c); the part for each
	// level counts
	for (g=0;g<4;g++) {
		int sides = spell_int(spell,keys[g][0],0), n = (g == group) ? count : spell_int(spell,keys[g][1],0);

		if (sides == 0 && n > 0)
			dtu_spell_log("spell: '%s': %d dice with 0 sides (%s) add 0, the part for each level counts",
				spell_name(spell),n,keys[g][0]);
	}

	// the case writes the number of dice of its group (0x8E592, 0x8EEAB,
	// 0x8EFE0) before the roll
	dmg = (group >= 0 ? dtu_spell_damage_roll_count(spell,target,level,group,count) : dtu_spell_damage_roll(spell,target,level)) + extra;
	return spell_deal(spell,target,caster,level,dmg,1);
}

// The part of 0x911AA after the roll: Invulnerability, the save for half
// ("save" 0: none, Dtu_fix_melf_acid), the Armor pool, the sound, the
// vclip, the text and the damage
static fix spell_deal(int spell,object *target,object *caster,int level,int dmg,int save)
{
	fix dealt = 0;

	if (target->type == OBJ_PLAYER && (target->dtu.m_88 & DTU_FX_INVULNERABLE))
		dmg = 0;		// 0x9178B
	if (dmg > 0 && save && dtu_spell_save(target,DTU_SAVE_SPELL))
		dmg /= 2;
	else if (dmg > 0)
		dtu_spell_interrupt(target,1);		// 0x913A7, 0x917D5
	if (target->type == OBJ_ROBOT)
		dtu_spell_creature_event(target,DTU_CSC_SPELL_HIT);		// 0x91332
	// the Armor pool of a creature (0x913AF; the branch of the player has no
	// such step)
	if (target->type == OBJ_ROBOT)
		dtu_spell_armor_hit(target,i2f(dmg));

	if (target->type == OBJ_ROBOT) {
		sound_at(spell_int(spell,"monster_hit_sound",-1),target->segnum,&target->pos,F1_0/2);
		vclip_at(target->segnum,&target->pos,i2f(3),spell_int(spell,"monster_hit_vclip",-1));
		if (caster && caster->type == OBJ_PLAYER)
			HUD_init_message("You cast %s on %s for %d damage.",spell_name(spell),dtu_object_display_name(target),dmg);
		if (dmg > 0)
			dealt = dtu_damage_creature_direct(target,caster,i2f(dmg));
	}
	else {
		if (dmg > 0)
			dealt = dtu_damage_player_direct(target,caster,i2f(dmg));
		if (caster == NULL || caster == target)
			HUD_init_message("You took %d damage.",dmg);
		else
			HUD_init_message("%s cast %s on you for %d damage.",dtu_object_display_name(caster),spell_name(spell),dmg);
	}
	dtu_spell_log("spell: '%s' level %d on obj %d %s: damage %d, done %d, hp now %d",spell_name(spell),level,
		(int)(target-Objects),target_name(target),dmg,f2i(dealt),
		target->type == OBJ_PLAYER ? dtu_char_hit_points() : f2i(target->shields));
	return dealt;
}

//------------------------------------------------------------------------
// The messages of 0x8CFFF. The case writes the text into 0x3A64B0 and
// 0x347A2 puts it into the message box when 0x8CEAE gave 1.

static char Msg_t[48], Msg_c[48];		// 0x3A6584 target, 0x3A6548 caster
static const char *Msg_are, *Msg_have, *Msg_were;	// 0x3A6528, 0x3A6500, 0x3A6514
static int Msg_show;						// 0x3A65C4
// ASCENT: the sex of the subject of a message (dtulang.h, the token
// {sex:MALE|FEMALE}): the player's when the player is the target or the
// caster, else male. Msg_female is the one of the next say().
static int Msg_t_female, Msg_c_female, Msg_female;

#define V_NONE	0
#define V_ARE	1		// "are" / "is" of the caster
#define V_HAVE	2		// "have" / "has" of the caster

// 0x16636: a creature that is invisible (+0x147, set by the two
// Invisibility spells) is not seen unless the player is blind (then the
// test does not apply) or has an intelligence of 13 or more and a level of
// 10 or more
static int caster_unseen(object *c)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1;

	if (c->type != OBJ_ROBOT || !(c->dtu.m_88 & (DTU_FX_INVISIBLE|DTU_FX_IMP_INVIS)))
		return 0;
	if (ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_BLIND))
		return 0;
	return pa->intelligence < 13 || (l1 > l2 ? l1 : l2) < 10;
}

// 0x8CEAE(target, caster): 1 if the target or the caster is the player;
// else the segment of the caster is in the visible list (0x38C060; the port
// takes the segments of the last rendered frame), the caster is seen
// (0x16636), and d20 <= intelligence - 2 (P+0xBD: the Spellcraft skill of
// the 1.3 readme).
static int message_rule(object *target,object *caster)
{
	int i, seen = 0;

	if (target == ConsoleObject || caster == ConsoleObject)
		return 1;
	if (caster == NULL)
		return 0;
	for (i=0;i<N_render_segs && i<RENDER_LIST_SIZE;i++)
		if (Render_list[i] == caster->segnum) {
			seen = 1;
			break;
		}
	if (!seen || caster_unseen(caster))
		return 0;
	return rand() % 20 + 1 <= (int)Dtu_char.pa.intelligence - 2;
}

// The names and verbs of 0x8CFFF (0x8D08D to 0x8D1B4): the player is
// "You" (target "were", caster "are" and "have"), a trap "a trap", a
// creature or another object its name ("was", "is", "has").
static void msg_setup(object *target,object *caster)
{
	const char *t, *c;

	// ASCENT: the words in the language of the text (dtulang.h)
	t = (target == ConsoleObject) ? LT("You") : (target->type == OBJ_LIGHT) ? LT("a trap") : dtu_object_display_name(target);
	c = (caster == ConsoleObject) ? LT("You") : (caster == NULL || caster->type == OBJ_LIGHT) ? LT("a trap") : dtu_object_display_name(caster);
	snprintf(Msg_t,sizeof(Msg_t),"%s",t ? t : "");
	snprintf(Msg_c,sizeof(Msg_c),"%s",c ? c : "");
	Msg_were = (target == ConsoleObject) ? LT("were") : LT("was");
	Msg_are = (caster == ConsoleObject) ? LT("are") : LT("is");
	Msg_have = (caster == ConsoleObject) ? LT("have") : LT("has");
	Msg_show = message_rule(target,caster);
	Msg_t_female = (target == ConsoleObject) && dtu_lang_female();
	Msg_c_female = (caster == ConsoleObject) && dtu_lang_female();
	Msg_female = Msg_t_female;
}

static void say(const char *format, ...)
{
	char buf[200];
	va_list args;

	va_start(args,format);
	// ASCENT: the language (dtulang.h). The tokens {sex:} of the format
	// follow the subject of the message (the target, or the caster where
	// the caster is the subject); HUD_init_message resolves those of the
	// values for the player.
	vsnprintf(buf,sizeof(buf),dtu_tr_sex(format,Msg_female),args);
	va_end(args);
	Msg_female = Msg_t_female;
	dtu_spell_log("spell: message%s: %s",Msg_show ? "" : " (not shown)",buf);
	if (Msg_show)
		HUD_init_message("%s",buf);
}

// The pattern of most cases: on the caster himself "<target> <verb> ...
// %d", else "... cast on <target> by <caster> ... %d"
static void say_effect(int self,const char *self_format,int verb,const char *other_format,int n)
{
	if (!self)
		say(other_format,Msg_t,Msg_c,n);
	else if (verb == V_NONE)
		say(self_format,Msg_t,n);
	else
		say(self_format,Msg_t,verb == V_ARE ? Msg_are : Msg_have,n);
}

// The minutes and hours of the texts: the duration in whole seconds
// divided by 60 or 3600 (idiv, rounded down)
#define MINUTES(d)	(f2i(d) / 60)
#define HOURS(d)		(f2i(d) / 3600)

// The hit points of Cure Light Wounds and Cure Serious Wounds, at most the
// maximum (0x8DE4B, 0x8DF37)
static void heal(object *target,int points,int spell)
{
	int before;

	if (target->type == OBJ_PLAYER) {
		before = dtu_char_hit_points();
		dtu_char_heal(points);
		dtu_spell_log("spell: '%s' heals the player by %d: %d -> %d of %d",spell_name(spell),points,before,dtu_char_hit_points(),dtu_char_max_hit_points());
	}
	else if (target->type == OBJ_ROBOT && target->id < N_robot_types) {
		fix max = i2f(Monster_info[target->id].max_hit_points);

		before = f2i(target->shields);
		target->shields += i2f(points);
		if (target->shields > max)
			target->shields = max;
		dtu_spell_log("spell: '%s' heals obj %d %s by %d: %d -> %d",spell_name(spell),(int)(target-Objects),target_name(target),points,before,f2i(target->shields));
	}
}

// The wisdom of the player shortens Fear, Scare, Hold and the stun of
// Color Spray: d / (wisdom / 2) (0x8DAA2, 0x8E338, 0x8E8FA, 0x8F57F)
static fix wisdom_cut(object *target,fix d)
{
	int w = Dtu_char.pa.wisdom;

	if (target->type != OBJ_PLAYER || w < 2)
		return d;
	return fixdiv(d,w << 15);
}

static int rnd(int n)
{
	return rand() % n + 1;
}

// The spells that the port does not have. The cast stops with a message
// before a slot or an item is used.
static int spell_not_in_port(int spell)
{
	(void) spell;
	return 0;
}

// Robot_info+0xD9 bit 3 (0x1F448): the name of the creature starts with
// "rf", "rm", "av" or "dro" (the human and drow fighters and casters).
static int creature_rf_name(object *obj)
{
	char *n;

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	n = Monster_info[obj->id].name;
	return !strnicmp(n,"rf",2) || !strnicmp(n,"rm",2) || !strnicmp(n,"av",2) || !strnicmp(n,"dro",3);
}

//------------------------------------------------------------------------
// The NPC table (0xA504A): name, THAC0, armor class, mage level, mage
// points, cleric level, cleric points. Only the rows with spells; the
// THAC0 and the armor class of the rows are applied in dtuobj.c through
// dtu_npc_row() as well.

static const struct { char *name; signed char v[6]; } Npc_table[] = {
#include "dtunpc.h"
};

int dtu_npc_row(char *name, int v[6])
{
	int i, j;

	for (i=0;i<(int)(sizeof(Npc_table)/sizeof(Npc_table[0]));i++)
		if (!stricmp(Npc_table[i].name,name)) {
			for (j=0;j<6;j++)
				v[j] = (j >= 2) ? (ubyte) Npc_table[i].v[j] : Npc_table[i].v[j];
			return 1;
		}
	return 0;
}

// The mage level (Robot_info+0xE2) and the cleric level (+0xE3) of a
// creature: the NPC table, else spell_level= of the line (the cleric
// level only with cleric_points=).
static int creature_mage_level(object *obj)
{
	int v[6];

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	if (dtu_npc_row(Monster_info[obj->id].name,v))
		return v[2];
	return monster_int(obj,"spell_level",0);
}

static int creature_cleric_level(object *obj)
{
	int v[6];

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	if (dtu_npc_row(Monster_info[obj->id].name,v))
		return v[4];
	return monster_int(obj,"cleric_points",0) ? monster_int(obj,"spell_level",0) : 0;
}

static int creature_points(object *obj,int cleric)
{
	int v[6];

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	if (dtu_npc_row(Monster_info[obj->id].name,v))
		return v[cleric ? 5 : 3];
	return monster_int(obj,cleric ? "cleric_points" : "spell_points",0);
}

//------------------------------------------------------------------------
// A spell on an object that is not the player or a creature: scenery
// (0x2641C casts the spell of a missile on the scenery that it hits), a
// missile, an item, a light (the cases 0x8E0B5, 0x8EE76, 0x8F021 and the
// damage 0x911AA)

static fix object_spell(int spell,object *t,object *caster,int L)
{
	int unbreakable;

	switch (t->type) {
		case OBJ_DRESSING:
			if (t->id >= N_dressing_types)
				return 0;
			unbreakable = dtu_dressing_unbreakable(t);
			switch (spell) {
				case 13: case 55:		// Dispel Magic: a magic object, save 5
					if ((Dressing_info[t->id].flags & DRESSING_F_MAGIC) && !dtu_spell_save(t,5))
						dtu_break_dressing(t,caster,0);
					break;
				case 16: case 17: case 37:	// Fireball, Flaming Sphere, Meteors: save 5
					if (!unbreakable && !dtu_spell_save(t,5))
						dtu_break_dressing(t,caster,0);
					break;
				case 32:		// Lightning Bolt: save 6
					if (!unbreakable && !dtu_spell_save(t,6))
						dtu_break_dressing(t,caster,0);
					break;
				case 4: case 33: case 34: case 36: {
					// 0x911AA (0x91277): shields -= damage_percent * size;
					// below 1.0 the object explodes if it has an explode_sound
					fix d = fixmul(fl2f(spell_int(spell,"damage_percent",10) * 0.01),Dressing_info[t->id].size);
					t->shields -= d;
					dtu_spell_log("spell: '%s' on scenery obj %d %s: %.2f damage, %.2f left",spell_name(spell),(int)(t-Objects),
						dtu_object_name(t),f2fl(d),f2fl(t->shields));
					if (t->shields < F1_0 && Dressing_info[t->id].explode_sound != -1)
						dtu_break_dressing(t,caster,1);
					break;
				}
			}
			return 0;
		case OBJ_WEAPON:
			// Dispel Magic: a missile of another spell, save 4 (10 for this
			// type) or it goes
			if ((spell == 13 || spell == 55)) {
				int s = dtu_missile_spell(t);
				if (s >= 0 && s != spell && !dtu_spell_save(t,4)) {
					dtu_spell_log("spell: Dispel Magic ends the missile obj %d of '%s'",(int)(t-Objects),spell_name(s));
					t->flags |= OF_SHOULD_BE_DEAD;
				}
			}
			return 0;
		case OBJ_MISC:
			// Dispel Magic: a potion that is not a plot item
			if ((spell == 13 || spell == 55) && t->id < N_misc_types && strstr(Misc_info[t->id].name,"otion") &&
				 !dtu_tbl_int(Misc_info[t->id].record,"plot_item",0)) {
				int need = 23 - L, roll = rand() % 20 + 1;
				if (need < 2) need = 2;
				if (need > 20) need = 20;
				dtu_spell_log("spell: Dispel Magic on the potion obj %d: d20 %d against %d",(int)(t-Objects),roll,need);
				if (roll >= need) {
					sound_at(2702,t->segnum,&t->pos,F1_0/2);
					t->flags |= OF_SHOULD_BE_DEAD;
				}
			}
			return 0;
		case OBJ_LIGHT:
			// Dispel Magic: a light of a light spell (+0x144 bit 0), save 4
			if ((spell == 13 || spell == 55) && !dtu_spell_save(t,4)) {
				dtu_spell_log("spell: Dispel Magic ends the light obj %d",(int)(t-Objects));
				t->flags |= OF_SHOULD_BE_DEAD;
			}
			return 0;
	}
	return 0;
}

//------------------------------------------------------------------------
// ASCENT: Melf's Acid Arrows by the rule of AD&D 2nd edition
// (Dtu_fix_melf_acid in dtuspell.h)

#define MAX_ACID	32
// The length of one round of the acid: one AD&D round as MINUTE (60.0
// seconds of game time). Whether 1.3 counts its spell times in rounds of
// 60.0 is checked elsewhere; change this one value if that changes.
#define ACID_ROUND	MINUTE

typedef struct acid_burn {
	int	used;
	short	obj, caster;
	int	obj_sig, caster_sig;
	short	spell, level;
	short	round, rounds;		// the round of the last damage, the last round
	fix	start, next;		// GameTime of the hit, of the next damage
} acid_burn;

static acid_burn Acid[MAX_ACID];

static void acid_clear(void)
{
	int i;

	for (i=0;i<MAX_ACID;i++)
		Acid[i].used = 0;
}

static int acid_target_gone(object *t)
{
	if (t->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 1;
	if (t->type == OBJ_ROBOT)
		return t->shields < F1_0 || DTU_ANIM_STATE(t) == DAS_DIE;
	if (t->type == OBJ_PLAYER)
		return Player_is_dead || Dtu_player_dead;
	return 1;
}

// One round of acid: 2d4, the acid rule of 0x91BC3 (the regeneration of
// the target ends; no table has an acid immunity), no save
static fix acid_damage(int spell,object *target,object *caster,int level)
{
	int dmg = rnd(4) + rnd(4);

	target->dtu.m_88 &= ~DTU_FX_REGENERATE;
	return spell_deal(spell,target,caster,level,dmg,0);
}

// The hit of the arrow. The THAC0 of a fighter is the fighter row of the
// table 0x146590 (dtuchar.c): 21 - L, at most 20, at least 3 (level 18).
static fix melf_acid_hit(int spell,object *target,object *caster,int L)
{
	int thac0 = 21 - L, d20 = rnd(20), ac = 0, hit, i, rounds = L/3;
	fix dealt;

	if (thac0 > 20) thac0 = 20;
	if (thac0 < 3) thac0 = 3;
	if (d20 == 1)
		hit = 0;
	else
		hit = dtu_hit_test(target,caster,d20 == 20 ? 100 : d20,thac0,&ac);
	if (d20 == 1)
		dtu_spell_log("spell: '%s' (Dtu_fix_melf_acid): attack roll d20 1: a natural 1 misses",spell_name(spell));
	else
		dtu_spell_log("spell: '%s' (Dtu_fix_melf_acid): attack roll d20 %d against fighter THAC0 %d (level %d) and armor class %d: %s",
			spell_name(spell),d20,thac0,L,ac,hit ? "hit" : "miss");
	if (!hit) {
		if (caster && caster->type == OBJ_PLAYER)
			HUD_init_message("You miss %s with %s.",dtu_object_display_name(target),spell_name(spell));
		else if (target->type == OBJ_PLAYER && caster)
			HUD_init_message("%s misses you with %s.",dtu_object_display_name(caster),spell_name(spell));
		return 0;
	}
	dtu_spell_log("spell: '%s': acid round 1 of %d on obj %d %s at %.2f",spell_name(spell),rounds + 1,(int)(target-Objects),
		target_name(target),f2fl(GameTime));
	dealt = acid_damage(spell,target,caster,L);
	if (rounds < 1 || acid_target_gone(target))
		return dealt;
	for (i=0;i<MAX_ACID && Acid[i].used;i++)
		;
	if (i == MAX_ACID) {
		dtu_spell_log("spell: '%s': no free acid entry, the acid ends after the first round",spell_name(spell));
		return dealt;
	}
	Acid[i].used = 1;
	Acid[i].obj = target-Objects;
	Acid[i].obj_sig = target->signature;
	Acid[i].caster = caster ? caster-Objects : -1;
	Acid[i].caster_sig = caster ? caster->signature : 0;
	Acid[i].spell = spell;
	Acid[i].level = L;
	Acid[i].round = 1;
	Acid[i].rounds = rounds + 1;
	Acid[i].start = GameTime;
	Acid[i].next = GameTime + ACID_ROUND;
	return dealt;
}

// Each frame: the acid of the later rounds
static void acid_frame(void)
{
	int i;

	for (i=0;i<MAX_ACID;i++) {
		acid_burn *a = &Acid[i];
		object *t, *c = NULL;

		if (!a->used)
			continue;
		t = &Objects[a->obj];
		if (t->signature != a->obj_sig || acid_target_gone(t) || GameTime < a->start) {
			dtu_spell_log("spell: '%s': the acid on obj %d ends after round %d of %d (the target is gone)",spell_name(a->spell),
				a->obj,a->round,a->rounds);
			a->used = 0;
			continue;
		}
		if (GameTime < a->next)
			continue;
		if (a->caster >= 0 && Objects[a->caster].signature == a->caster_sig &&
			 (Objects[a->caster].type == OBJ_PLAYER || Objects[a->caster].type == OBJ_ROBOT))
			c = &Objects[a->caster];
		a->round++;
		dtu_spell_log("spell: '%s': acid round %d of %d on obj %d %s at %.2f",spell_name(a->spell),a->round,a->rounds,a->obj,
			target_name(t),f2fl(GameTime));
		acid_damage(a->spell,t,c,a->level);
		a->next += ACID_ROUND;
		if (a->round >= a->rounds) {
			dtu_spell_log("spell: '%s': the acid on obj %d ends",spell_name(a->spell),a->obj);
			a->used = 0;
		}
	}
}

//------------------------------------------------------------------------
// The effect of a spell on one target (0x8CFFF, jump table 0x8CF1F)

static fix apply_spell(int spell,object *target,object *caster,int L)
{
	int T, self = (target == caster), mf;
	fix dealt = 0;

	if (!spell_ok(spell) || target == NULL)
		return 0;
	if (target->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 0;
	if (target->type != OBJ_PLAYER && target->type != OBJ_ROBOT)
		return object_spell(spell,target,caster,L < 1 ? 1 : L);
	if (target->type == OBJ_ROBOT && (target->shields < F1_0 || DTU_ANIM_STATE(target) == DAS_DIE))
		return 0;
	if (target->type == OBJ_PLAYER && (Player_is_dead || Dtu_player_dead))
		return 0;

	T = object_level(target,spell);
	mf = monster_flags(target);
	if (L < 1) L = 1;

	// A creature that is the target of the player wakes and is hostile
	// (0xA6505 with 1, then 0x12AD9)
	if (target->type == OBJ_ROBOT && !self && caster && caster->type == OBJ_PLAYER)
		dtu_creature_wake(target,1);

	dtu_spell_log("spell: '%s' (%d) level %d on obj %d %s (level %d)",spell_name(spell),spell,L,(int)(target-Objects),target_name(target),T);

	// The names, the verbs and 0x8CEAE, before the switch as in 0x8CFFF
	msg_setup(target,caster);

	switch (spell) {
		case 1: {	// Aid (0x8D3F7): 1d8 hit points above the maximum, Bless
			// for L+1 rounds. No test of Bless; the hit points go when Bless
			// ends (0x9002C cuts them to the maximum).
			int n = rnd(8);
			fix d = (L+1)*MINUTE;

			if (target->type == OBJ_PLAYER)
				dtu_char_add_hit_points(n);
			else
				target->shields += i2f(n);
			target->dtu.m_88 &= ~DTU_FX_BLESS;
			fx_set(target,DTU_FX_BLESS,d);
			dtu_spell_log("spell: Aid gives obj %d %s %d hit points",(int)(target-Objects),target_name(target),n);
			if (self)
				say("%s %s %d aid hit points for %d minutes.",Msg_t,Msg_have,n,MINUTES(d));
			else
				say("%d aid hit points cast on %s by %s for %d minutes.",n,Msg_t,Msg_c,MINUTES(d));
			break;
		}
		case 2:		// Armor (0x8D4E7): bit 0 and a pool of 8 + L hit points
			// (+0x90), armor class 6 at most. No timer: 0x9002C ends it when
			// weapon damage has taken the pool (dtu_spell_armor_hit).
			if (target->dtu.m_88 & DTU_FX_ARMOR)
				break;
			target->dtu.m_88 |= DTU_FX_ARMOR;
			target->dtu.m_8c[1] = i2f(8 + L);
			// "%s %s %d armor hit points." (0x12D928) on the caster himself,
			// else "%d armor hit points cast on %s by %s." (0x12D943)
			if (self)
				say("%s %s %d armor hit points.",Msg_t,Msg_have,8 + L);
			else
				say("%d armor hit points cast on %s by %s.",8 + L,Msg_t,Msg_c);
			dtu_spell_log("spell: effect Armor on obj %d %s, pool %d hit points",(int)(target-Objects),target_name(target),8 + L);
			break;
		case 3:		// Bless: 6 rounds
			if (fx_set(target,DTU_FX_BLESS,i2f(360)))
				say_effect(self,"%s %s blessed for %d minutes.",V_ARE,"Bless cast on %s by %s for %d minutes.",6);
			break;
		case 4:		// Burning Hands: fire 1d3 + 2 L
		case 17:		// Flaming Sphere (no dice in the line)
		case 37:		// Melf's Minute Meteors: 1d4
			if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			break;
		case 5:		// Chill Touch: on the caster only, L+3 rounds
			if (self && fx_set(target,DTU_FX_CHILL_TOUCH,(L+3)*MINUTE))
				say("%s %s chill touch for %d minutes.",Msg_t,Msg_have,L+3);
			break;
		case 6:		// Color Spray
			if (self || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & DTU_FX_BLIND))
				break;
			if ((T > L || T >= 6) && dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			if (T <= L) {
				if (fx_set(target,DTU_FX_SLEEP,(2 + rand() % 7)*MINUTE)) {
					if (target->type == OBJ_PLAYER) {
						target->dtu.m_88 |= DTU_FX_BLIND;
						target->dtu.m_8c[3] = target->dtu.m_e8[0];
					}
					say("%s %s struck unconscious by %s.",Msg_t,Msg_were,Msg_c);
				}
			}
			else if (T <= L + 2) {
				if (fx_set(target,DTU_FX_BLIND,rnd(4)*MINUTE))
					say("%s %s blinded by %s.",Msg_t,Msg_were,Msg_c);
			}
			else if (!(target->dtu.m_88 & (DTU_FX_FREE_ACTION|DTU_FX_HOLD))) {
				if (fx_set(target,DTU_FX_HOLD,wisdom_cut(target,MINUTE)))
					say("%s %s stunned by %s.",Msg_t,Msg_were,Msg_c);
			}
			break;
		case 7:		// Confusion: L+2 rounds
			if (!dtu_spell_save(target,DTU_SAVE_SPELL) && fx_set(target,DTU_FX_CONFUSION,(L+2)*MINUTE))
				say("Confusion cast on %s by %s for %d minutes.",Msg_t,Msg_c,L+2);
			break;
		case 8: case 9:		// Continual Light: blind with no end
		case 30: case 31:		// Light: blind for 10 L or 10 (L+6) rounds
			if (self || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & DTU_FX_BLIND))
				break;
			if (!dtu_spell_save(target,DTU_SAVE_SPELL)) {
				fix d = (spell == 30) ? 10*L*MINUTE : (spell == 31) ? 10*(L+6)*MINUTE : DTU_FX_NO_END;

				if (fx_set(target,DTU_FX_BLIND,d)) {
					if (d == DTU_FX_NO_END)
						say("%s %s blinded by %s indefinitely.",Msg_t,Msg_were,Msg_c);
					else
						say("%s %s blinded by %s for %d minutes.",Msg_t,Msg_were,Msg_c,MINUTES(d));
				}
			}
			if (mf & MONSTER_F_LIGHT_SENSITIVE)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			break;
		case 10:		// Cure Light Wounds: 1d8, not undead or extraplanar
		case 11:		// Cure Serious Wounds: 3 to 17
			if (mf & (MONSTER_F_UNDEAD|MONSTER_F_EXTRAPLANAR))
				break;
			heal(target,spell == 10 ? rnd(8) : rand() % 15 + 3,spell);
			if (spell == 10)
				say("Cure light wounds cast on %s by %s.",Msg_t,Msg_c);
			else
				say("Cure serious wounds cast on %s by %s.",Msg_t,Msg_c);
			break;
		case 12:		// Detect Magic (mage): 2 L rounds
		case 54:		// Detect Magic (cleric): 10 rounds
			// The text only when the player is the target (0x8E08A)
			if (fx_set(target,DTU_FX_DETECT_MAGIC,spell == 12 ? 2*L*MINUTE : i2f(600)) && target == ConsoleObject)
				say("%s can detect magic for %d minutes.",Msg_t,spell == 12 ? 2*L : 10);
			break;
		case 13: case 55:	// Dispel Magic: all effects of the target end, no save
			fx_expire(target,1);
			if (target->type == OBJ_PLAYER)
				dtu_char_potions_end(1);		// 0x396B2(1)
			break;
		case 14:		// Fear: L rounds
		case 44:		// Scare: 1d4 + L rounds, targets of level 6 or less
			if (self || (mf & (MONSTER_F_NO_SCARE|MONSTER_F_UNDEAD)) || (target->dtu.m_88 & DTU_FX_FEAR))
				break;
			if (spell == 44 && (T > 6 || (mf & MONSTER_F_EXTRAPLANAR)))
				break;
			// 0x8F467: Scare has a save only for an elf, a half-elf or a
			// cleric player, and for a creature with +0xD9 bit 3 or a cleric
			// level
			if (spell == 44 && target->type == OBJ_PLAYER &&
				 !(Dtu_char.pa.race == DTU_RACE_ELF || (Dtu_char.pa.flags & DTU_PF_HALF_ELF) || char_is(DTU_CLASS_CLERIC)))
				;
			else if (spell == 44 && target->type == OBJ_ROBOT && !creature_cleric_level(target) && !creature_rf_name(target))
				;
			else if (dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			{
				fix d = wisdom_cut(target,(spell == 14 ? L : rnd(4) + L)*MINUTE);
				int drop = (spell == 14) ? (rand() % 100 + 1 <= 65 - 5*T) : (rand() % 2 != 0);

				fx_set(target,DTU_FX_FEAR,d);
				// The weapon falls: the player 0xAE837, a creature 0x3C4BD
				if (drop) {
					if (target->type == OBJ_PLAYER)
						dtu_char_drop_weapon();
					else
						dtu_creature_drop_contents(target);
				}
				// The player: the view moves into a gated creature that
				// flees (0x8E3C4, dtucombat.c), and Hold for the same time
				if (target->type == OBJ_PLAYER) {
					dtu_fear_view_start(target,d);
					fx_set(target,DTU_FX_HOLD,d);
				}
				if (spell == 14)
					say("Fear cast on %s by %s.",Msg_t,Msg_c);
				else
					say_effect(self,"%s %s Scared.",V_ARE,"Scare cast on %s by %s.",0);
			}
			break;
		case 0:		// Abjure (0x8D297)
			if (caster && caster->type == OBJ_ROBOT && !self) {
				// a creature caster: the damage of the line with the level 1
				// (the message "Cause Serious Wounds", 0x91987)
				dealt = spell_damage(spell,target,caster,1,GROUP_NONE,0,0);
				break;
			}
			{
				int need = 11 - L + T, roll = rand() % 20 + 1;

				if (need < 1) need = 1;
				if (need > 20) need = 20;
				if (target->type != OBJ_ROBOT || !(mf & MONSTER_F_EXTRAPLANAR) || roll < need) {
					dtu_spell_log("spell: Abjure on obj %d %s: extraplanar %d, d20 %d against %d: not affected",
						(int)(target-Objects),target_name(target),(mf & MONSTER_F_EXTRAPLANAR) != 0,roll,need);
					say("%s was not affected by abjure.",Msg_t);
					return 0;		// 0x8D56D: no common end
				}
				// The creature stops and turns slowly (movement type 3)
				vm_vec_zero(&target->mtype.phys_info.velocity);
				roll = rand() % 100 + 1;
				dtu_spell_log("spell: Abjure on obj %d %s (level %d): d20 made, d100 %d against %d: %s",(int)(target-Objects),
					target_name(target),T,roll,2*T + 70,roll >= 2*T + 70 ? "dies" : "sent away");
				if (roll >= 2*T + 70) {
					// 0x29A9C(target, no attacker, all its hit points)
					dealt = dtu_damage_creature_direct(target,NULL,target->shields);
				}
				else {
					// the hit sound of the creature (Robot_info+0x9A), then
					// 0xA9F58: the creature leaves the level
					if (target->id < N_robot_types)
						sound_at(Monster_info[target->id].hit_sound,target->segnum,&target->pos,F1_0/2);
					dtu_creature_vanish(target);
				}
			}
			break;
		case 41:		// Rary's Mnemonic Enhancer (0x8F21D)
			if ((target->dtu.m_88 & DTU_FX_MNEMONIC) && !Dtu_fix_rary_manual) {
				dtu_spell_log("spell: Rary's Mnemonic Enhancer is on already: no effect (0x8F226)");
				return 0;
			}
			target->dtu.m_88 |= DTU_FX_MNEMONIC;
			say_effect(self,"%s %s Rary's Mnemonic Enhancer for 1 day.",V_HAVE,"Rary's Mnemonic Enhancer cast on %s by %s for 1 day.",0);
			if (target->type == OBJ_PLAYER) {
				Rary_rest = 0;
				{
					// 0x655FE: the box returns the index of the choice. Escape
					// (-1) falls to the last case of 0x8F2E8: one 3rd.
					int c = dtu_char_choice("Which additional spell levels\nwould you like to cast?",3,"Three 1st","One 1st and one 2nd","One 3rd");
					rary_apply(c < 0 ? 2 : c);
				}
			}
			break;
		case 15:		// Feather Fall: L rounds
			if (fx_set(target,DTU_FX_FEATHER_FALL,L*MINUTE))
				say_effect(self,"%s %s feather fall for %d minutes.",V_HAVE,"Feather fall cast on %s by %s for %d minutes.",L);
			break;
		case 16:		// Fireball: min(L, 10) fire dice + 6 L
			dealt = spell_damage(spell,target,caster,L,GROUP_FIRE,L < 10 ? L : 10,0);
			break;
		case 18:		// Flame Walk: L+1 rounds
			if (fx_set(target,DTU_FX_RESIST_FIRE,(L+1)*MINUTE))
				say_effect(self,"%s %s fire tolerance for %d minutes.",V_HAVE,"Flame Walk cast on %s by %s for %d minutes.",L+1);
			break;
		case 19:		// Fly: 10 (1d6 + L) rounds
			if (fx_set(target,DTU_FX_FLY,10*(rnd(6) + L)*MINUTE))
				say_effect(self,"%s %s Flying.",V_HAVE,"Flying cast on %s by %s.",0);
			break;
		case 20:		// Free Action: ends Hold, 10 L rounds
			if (!self)
				break;
			if (target->dtu.m_88 & DTU_FX_HOLD)
				fx_end(target,14);
			if (fx_set(target,DTU_FX_FREE_ACTION,10*L*MINUTE))
				say_effect(self,"%s %s Free Action for %d minutes.",V_HAVE,"Free Action cast on %s by %s for %d minutes.",10*L);
			break;
		case 21:		// Haste: ends Slow, L+3 rounds
			if (target->dtu.m_88 & DTU_FX_SLOW)
				fx_end(target,27);
			if (fx_set(target,DTU_FX_HASTE,(L+3)*MINUTE))
				say_effect(self,"%s %s Haste for %d minutes.",V_HAVE,"Haste cast on %s by %s for %d minutes.",L+3);
			break;
		case 22: case 52:	// Hold Person: 2 L rounds, a humanoid or the player
			if (self || (target->dtu.m_88 & (DTU_FX_HOLD|DTU_FX_FREE_ACTION)) || (mf & MONSTER_F_UNDEAD))
				break;
			if (target->type == OBJ_ROBOT && !(mf & MONSTER_F_HUMANOID))
				break;
			if (!dtu_spell_save(target,DTU_SAVE_SPELL)) {
				fix d = wisdom_cut(target,2*L*MINUTE);

				// The text in seconds below one minute (0x8E9C5)
				if (fx_set(target,DTU_FX_HOLD,d)) {
					if (f2i(d) < 60)
						say_effect(self,"%s %s held for %d seconds.",V_ARE,"Hold Person cast on %s by %s for %d seconds.",f2i(d));
					else
						say_effect(self,"%s %s held for %d minutes.",V_ARE,"Hold Person cast on %s by %s for %d minutes.",MINUTES(d));
				}
			}
			break;
		case 23:		// Hold Undead: 1d4 + L rounds, undead of level L or less
			if (self || (target->dtu.m_88 & DTU_FX_HOLD) || !(mf & MONSTER_F_UNDEAD) || T > L)
				break;
			if (T > 2 && dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			{
				fix d = (rnd(4) + L)*MINUTE;
				if (fx_set(target,DTU_FX_HOLD,d))
					say("Hold Undead cast on %s by %s for %d minutes.",Msg_t,Msg_c,MINUTES(d));
			}
			break;
		case 24:		// Improved Invisibility: L+4 rounds
			if (fx_set(target,DTU_FX_IMP_INVIS,(L+4)*MINUTE))
				say_effect(self,"%s %s invisible for %d minutes.",V_ARE,"Improved Invisibility cast on %s by %s for %d minutes.",L+4);
			break;
		case 25:		// Infravision: humans only, 2 hours and 1 hour for each level
			if (target->type == OBJ_PLAYER && Dtu_char.pa.race != DTU_RACE_HUMAN)
				break;
			if (monster_has(target,"infravision"))
				break;
			{
				fix d = (6*L + 12)*10*MINUTE;
				if (fx_set(target,DTU_FX_INFRAVISION,d))
					say_effect(self,"%s %s Infravision for %d hours.",V_HAVE,"Infravision cast on %s by %s for %d hours.",HOURS(d));
			}
			break;
		case 26:		// Invisibility: until an attack
			if (target->dtu.m_88 & (DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE))
				break;
			if (fx_set(target,DTU_FX_INVISIBLE,DTU_FX_NO_END))
				say_effect(self,"%s %s invisible.",V_ARE,"Invisibility cast on %s by %s.",0);
			break;
		case 27:		// Jump: 1d3 + L rounds
			if (fx_set(target,DTU_FX_JUMP,(rnd(3) + L)*MINUTE))
				say_effect(self,"%s may leap for some time.",V_NONE,"Jump cast on %s by %s.",0);
			break;
		case 28:		// Knock: the case has no code (the table entry is the common end)
			dtu_spell_log("spell: Knock has no effect in the executable");
			break;
		case 29:		// Levitate: 10 L rounds
			if (fx_set(target,DTU_FX_LEVITATE,10*L*MINUTE))
				say_effect(self,"%s can levitate for %d minutes.",V_NONE,"Levitate cast on %s by %s for %d minutes.",10*L);
			break;
		case 32:		// Lightning Bolt: min(L, 10) electric dice + 6 L
			dealt = spell_damage(spell,target,caster,L,GROUP_ELEC,L < 10 ? L : 10,0);
			break;
		case 33:		// Magic Missile: 1d4 with the level 1, Shield stops it
			if (self || (target->dtu.m_88 & DTU_FX_SHIELD))
				break;
			dealt = spell_damage(spell,target,caster,1,GROUP_NONE,0,0);
			break;
		case 34:		// Magical Stone: 1d4, two dice against undead
			if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,(mf & MONSTER_F_UNDEAD) ? rnd(4) : 0);
			break;
		case 35:		// Magical Vestment: 5 L rounds
			if (fx_set(target,DTU_FX_VESTMENT,5*L*MINUTE))
				say_effect(self,"%s %s Magical Vestment for %d minutes.",V_HAVE,"Magical Vestment cast on %s by %s for %d minutes.",5*L);
			break;
		case 36:		// Melf's Acid Arrows: 2 (L/3) + 2 acid dice + 4 L
			if (!self && Dtu_fix_melf_acid)
				dealt = melf_acid_hit(spell,target,caster,L);
			else if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_ACID,2*(L/3) + 2,0);
			break;
		case 38:		// Prayer: Bless for L rounds on the caster and on the others
			// whose bit 0x200 of 0x1B0 is clear (as read). The casts of the
			// player (0x56D6A) and of a creature reach only the caster.
			if ((self || !(target->dtu.m_1b0 & DTU_PEACEFUL)) && fx_set(target,DTU_FX_BLESS,L*MINUTE))
				say_effect(self,"%s %s Prayer for %d minutes.",V_HAVE,"Prayer cast on %s by %s for %d minutes.",L);
			break;
		case 39:		// Protection From Evil (mage): 2 L rounds
		case 53:		// Protection From Evil (cleric): 3 L rounds
			if (fx_set(target,DTU_FX_PROT_EVIL,(spell == 39 ? 2 : 3)*L*MINUTE))
				say_effect(self,"%s %s Protection From Evil for %d minutes.",V_HAVE,
					"Protection From Evil cast on %s by %s for %d minutes.",(spell == 39 ? 2 : 3)*L);
			break;
		case 40:		// Protection From Fire: a pool of 12 L hit points
			target->dtu.m_82 += 12*L;
			target->dtu.m_88 |= DTU_FX_PROT_FIRE;
			// the texts give the pool after the cast (+0x82)
			if (self)
				say("%s %s %d hit points of Fire Immunity.",Msg_t,Msg_have,target->dtu.m_82);
			else
				say("%d hit points of Protection From Fire cast on %s by %s.",target->dtu.m_82,Msg_t,Msg_c);
			break;
		case 42:		// "Resist Cold": the code cures blindness and deafness
			if (!(target->dtu.m_88 & (DTU_FX_BLIND|DTU_FX_DEAF)))
				break;
			// 0x8F309: on the caster himself the text of the deafness
			// replaces the one of the blindness in the buffer; the names are
			// the ones of the caster
			if (self) {
				Msg_female = Msg_c_female;		// ASCENT: the caster is the subject
				if (target->dtu.m_88 & DTU_FX_DEAF)
					say("%s %s no longer deaf.",Msg_c,Msg_are);
				else
					say("%s %s no longer blind.",Msg_c,Msg_are);
			}
			else
				say("Cure Blindness or Deafness cast on %s by %s.",Msg_t,Msg_c);
			// 0x8F3AD: a deaf player gets the volume of the sounds back from
			// the configuration 0x13BFEB (the deafness of the fire trap)
			if (target == ConsoleObject && (target->dtu.m_88 & DTU_FX_DEAF))
				digi_set_digi_volume((Config_digi_volume * 32768) / 8);
			target->dtu.m_88 &= ~(DTU_FX_BLIND|DTU_FX_DEAF);
			target->dtu.m_8c[3] = 0;
			break;
		case 43:		// Resist Fire: L rounds
			if (fx_set(target,DTU_FX_RESIST_FIRE,L*MINUTE))
				say_effect(self,"%s %s Fire Resistance for %d minutes.",V_HAVE,"Resist Fire cast on %s by %s for %d minutes.",L);
			break;
		case 45:		// Shield: 5 L rounds
			if (fx_set(target,DTU_FX_SHIELD,5*L*MINUTE))
				say_effect(self,"%s %s Shielded for %d minutes.",V_ARE,"Shield cast on %s by %s for %d minutes.",5*L);
			break;
		case 46: {		// Sleep (0x8F766): 5 L rounds, targets of level 3 or less
			int cost;

			if (self || (target->dtu.m_88 & DTU_FX_SLEEP) || (mf & (MONSTER_F_NO_SLEEP|MONSTER_F_UNDEAD)) || T > 3)
				break;
			if (target->type == OBJ_PLAYER) {
				if (Dtu_char.pa.race == DTU_RACE_ELF && rnd(10) <= 9) break;
				if ((Dtu_char.pa.flags & DTU_PF_HALF_ELF) && rnd(10) <= 3) break;
			}
			// 0x8F840: a creature with Robot_info+0xD9 bit 3 resists on
			// d10 <= 9
			else if (creature_rf_name(target) && rnd(10) <= 9)
				break;
			// 0x8F865: the pool Spell_info+0x49 (a signed byte, set by each
			// cast in 0x5742C). A creature costs its max hit points / 4
			// (Robot_info word +0x78 >> 2), the player 2 T. Less in the pool
			// than the cost: no effect and no text.
			cost = (target->type == OBJ_ROBOT && target->id < N_robot_types) ? Monster_info[target->id].max_hit_points >> 2 : 2*T;
			if (Sleep_pool < cost) {
				dtu_spell_log("spell: Sleep pool %d, obj %d %s costs %d: no effect",Sleep_pool,(int)(target-Objects),target_name(target),cost);
				break;
			}
			Sleep_pool = (signed char) (Sleep_pool - cost);
			dtu_spell_log("spell: Sleep pool %d left after obj %d %s (cost %d)",Sleep_pool,(int)(target-Objects),target_name(target),cost);
			if (fx_set(target,DTU_FX_SLEEP,5*L*MINUTE)) {
				if (target->type == OBJ_PLAYER) {
					target->dtu.m_88 |= DTU_FX_BLIND;
					target->dtu.m_8c[3] = target->dtu.m_e8[0];
				}
				say_effect(self,"%s will be Sleeping for %d minutes.",V_NONE,"Sleep cast on %s by %s for %d minutes.",5*L);
			}
			break;
		}
		case 47:		// Slow: ends Haste, L+3 rounds
			if (self || (target->dtu.m_88 & DTU_FX_SLOW) || dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			if (target->dtu.m_88 & DTU_FX_HASTE)
				fx_end(target,13);
			if (fx_set(target,DTU_FX_SLOW,(L+3)*MINUTE))
				say_effect(self,"%s %s Slowed for %d minutes.",V_ARE,"Slow cast on %s by %s for %d minutes.",L+3);
			break;
		case 48:		// Spider Climb: L+3 rounds
			if (fx_set(target,DTU_FX_SPIDER_CLIMB,(L+3)*MINUTE))
				say_effect(self,"%s may Spider Climb for %d minutes.",V_NONE,"Spider Climb cast on %s by %s for %d minutes.",L+3);
			break;
		case 49:		// Stone Skin: 1d4 + L/2 hits
			if (target->dtu.m_88 & DTU_FX_STONESKIN)
				break;
			if (Dtu_fix_stoneskin_word)
				target->dtu.m_88 |= DTU_FX_STONESKIN;
			else
				target->dtu.m_88 = DTU_FX_STONESKIN;
			target->dtu.m_80 = rnd(4) + L/2;
			say_effect(self,"%s %s Stone Skin for %d attacks.",V_HAVE,"Stone Skin cast on %s by %s for %d attacks.",target->dtu.m_80);
			break;
		case 50:		// Strength: L hours (0x8FCFA). The player gains strength.
			if (target->dtu.m_88 & DTU_FX_STRENGTH)
				break;
			if (target->type == OBJ_PLAYER) {
				int gain = char_is(DTU_CLASS_FIGHTER) ? rnd(8) : (char_is(DTU_CLASS_THIEF) || char_is(DTU_CLASS_CLERIC)) ? rnd(6) : rnd(4);
				dtu_char_strength_gain(gain,char_is(DTU_CLASS_FIGHTER));
			}
			{
				// L * 225 << 20 in 32 bits (0x8FF5D): from the level 10 on the
				// value wraps as in the executable
				fix d = (fix)((unsigned)L * 3600u * (unsigned)F1_0);

				if (fx_set(target,DTU_FX_STRENGTH,d))
					say_effect(self,"%s %s Strength for %d hours.",V_HAVE,"Strength cast on %s by %s for %d hours.",HOURS(d));
			}
			break;
		case 51:		// "Burning Hands" (cold): the code is Find Traps, 30 rounds
			if (fx_set(target,DTU_FX_FIND_TRAPS,i2f(1800))) {
				if (self)
					say("%s can Find Traps for %d minutes.",Msg_t,30);
				else
					say("Find Traps cast on %s by %s for %d minutes.",Msg_t,Msg_c,30);
			}
			break;
		default:
			dtu_spell_log("spell: '%s' (%d) is not in the port",spell_name(spell),spell);
			if (caster && caster->type == OBJ_PLAYER)
				HUD_init_message("%s is not in this version.",spell_name(spell));
			break;
	}

	// The common end 0x8FFE5: the player gets the new armor class
	if (target->type == OBJ_PLAYER) {
		dtu_char_update();
		dtu_spell_log("spell: the player has armor class %d, effects 0x%08x",dtu_char_armor_class(),target->dtu.m_88);
	}
	return dealt;
}

//------------------------------------------------------------------------
// Area effects (0x90CEF)

static int point_sees(vms_vector *p0,int seg0,object *obj)
{
	fvi_query fq;
	fvi_info hit;

	fq.p0 = p0;
	fq.p1 = &obj->pos;
	fq.startseg = seg0;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = FQ_TRANSWALL;
	return find_vector_intersection(&fq,&hit) == HIT_NONE;
}

// The light object of the light spells: 20.0 (Light) or 60.0 (Continual
// Light), lifetime 10 L or 10 (L+6) rounds, no end for Continual Light.
static void light_object(int spell,vms_vector *pos,int segnum,int L)
{
	int objnum = obj_create(OBJ_LIGHT,0,segnum,pos,NULL,0,CT_LIGHT,MT_NONE,RT_NONE);

	if (objnum < 0)
		return;
	Objects[objnum].ctype.light_info.intensity = (spell == 30 || spell == 31) ? i2f(20) : i2f(60);
	Objects[objnum].lifeleft = (spell == 30) ? 10*L*MINUTE : (spell == 31) ? 10*(L+6)*MINUTE : IMMORTAL_TIME;
	dtu_spell_log("spell: light object %d, light %.0f, life %.0f",objnum,f2fl(Objects[objnum].ctype.light_info.intensity),
		Objects[objnum].lifeleft == IMMORTAL_TIME ? -1.0 : f2fl(Objects[objnum].lifeleft));
}

int dtu_spell_area(int spell,vms_vector *pos,int segnum,object *caster,int level)
{
	fix radius = i2f(spell_int(spell,"radius",0) + level * spell_int(spell,"level_radius",0));
	int i, n = 0;

	if (!spell_ok(spell) || segnum < 0)
		return 0;
	dtu_spell_log("spell: area of '%s' at %.1f %.1f %.1f, radius %.1f",spell_name(spell),f2fl(pos->x),f2fl(pos->y),f2fl(pos->z),f2fl(radius));
	if (spell == 8 || spell == 9 || spell == 30 || spell == 31)
		light_object(spell,pos,segnum,level);
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];

		if (o->type != OBJ_PLAYER && o->type != OBJ_ROBOT)
			continue;
		if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED|OF_EXPLODING))
			continue;
		if (o->type == OBJ_PLAYER && o != ConsoleObject)
			continue;
		if (vm_vec_dist(&o->pos,pos) > radius)
			continue;
		// The executable tests the brain bit of the two segments (0x9810D).
		// The BRN files fit no level (ai.md): the port tests a ray.
		if (!point_sees(pos,segnum,o))
			continue;
		apply_spell(spell,o,caster,level);
		if (spell == 16)		// 0x90DDF: Fireball, the fire trap (dtucombat.c)
			dtu_fireball_blind_deaf(o,vm_vec_dist(&o->pos,pos));
		n++;
	}
	return n;
}

//------------------------------------------------------------------------
// The missiles of the player: homing and the area at the end

#define MAX_TRACKED	32

static struct {
	int		objnum, signature, spell, level;
	int		target, target_sig;		// homing
	int		area_done;
	vms_vector last_pos;
	int		last_seg;
} Tracked[MAX_TRACKED];

static void track_missile(int objnum,int spell,int level,int target)
{
	int i;

	for (i=0;i<MAX_TRACKED;i++)
		if (Tracked[i].signature == 0)
			break;
	if (i >= MAX_TRACKED)
		return;
	Tracked[i].objnum = objnum;
	Tracked[i].signature = Objects[objnum].signature;
	Tracked[i].spell = spell;
	Tracked[i].level = level;
	Tracked[i].target = target;
	Tracked[i].target_sig = target >= 0 ? Objects[target].signature : 0;
	Tracked[i].area_done = 0;
	Tracked[i].last_pos = Objects[objnum].pos;
	Tracked[i].last_seg = Objects[objnum].segnum;
}

// 0x55D91 (guess for the rule): the nearest creature in front that the
// player sees, within 200.0.
static int find_homing_target(object *from,vms_vector *dir)
{
	int i, best = -1;
	fix best_d = i2f(200);

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		vms_vector v;
		fix d;

		if (o->type != OBJ_ROBOT || (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)) || o->shields < F1_0 || DTU_ANIM_STATE(o) == DAS_DIE)
			continue;
		vm_vec_sub(&v,&o->pos,&from->pos);
		d = vm_vec_normalize_quick(&v);
		if (d >= best_d || vm_vec_dot(&v,dir) < F1_0/2)
			continue;
		if (!point_sees(&from->pos,from->segnum,o))
			continue;
		best = i;
		best_d = d;
	}
	return best;
}

static void tracked_frame(void)
{
	int i;

	for (i=0;i<MAX_TRACKED;i++) {
		object *obj;

		if (Tracked[i].signature == 0)
			continue;
		obj = &Objects[Tracked[i].objnum];
		if (obj->signature != Tracked[i].signature || obj->type != OBJ_WEAPON || (obj->flags & OF_SHOULD_BE_DEAD)) {
			// The missile is gone. The area of an area spell comes from the
			// wall hit (0x26C2F calls 0x90CEF at 0x26D63, dtuobj.c) or from
			// the object hit; a missile whose time ends has no area.
			Tracked[i].signature = 0;
			continue;
		}
		Tracked[i].last_pos = obj->pos;
		Tracked[i].last_seg = obj->segnum;

		// Homing (Weapon_info+0x0B): turn towards the target. The rule of
		// the turn was not read (guess: the direction moves by 4 times
		// the frame time towards the target, a rule of time).
		if (Tracked[i].target >= 0) {
			object *t = &Objects[Tracked[i].target];
			vms_vector *vel = &obj->mtype.phys_info.velocity, dir, to;
			fix speed;

			if (t->signature != Tracked[i].target_sig || t->type != OBJ_ROBOT || t->shields < F1_0) {
				Tracked[i].target = -1;
				continue;
			}
			speed = vm_vec_mag_quick(vel);
			dir = *vel;
			vm_vec_normalize_quick(&dir);
			vm_vec_sub(&to,&t->pos,&obj->pos);
			vm_vec_normalize_quick(&to);
			vm_vec_scale_add2(&dir,&to,FrameTime*4);
			vm_vec_normalize_quick(&dir);
			vm_vec_copy_scale(vel,&dir,speed);
			vm_vector_2_matrix(&obj->orient,&dir,NULL,NULL);
		}
	}
}

//------------------------------------------------------------------------
// The function of dtuobj.h: a missile with a spell hits an object

fix dtu_spell_hit(int spell, object *target, object *caster, int level)
{
	if (!spell_ok(spell) || target == NULL)
		return 0;
	// 0x8CFFF computes the caster level itself (0x3A6540): the player
	// 0x91EC1, a creature 0xAE3BB (max hit points / 8)
	if (caster && (caster->type == OBJ_PLAYER || caster->type == OBJ_ROBOT))
		level = object_level(caster,spell);
	if (spell_area(spell)) {
		fix hp = (target->type == OBJ_ROBOT) ? target->shields : 0;

		dtu_spell_area(spell,&target->pos,target->segnum,caster,level);
		return (target->type == OBJ_ROBOT && hp > target->shields) ? hp - target->shields : 0;
	}
	return apply_spell(spell,target,caster,level);
}

//------------------------------------------------------------------------
// The cast of the player (0x41A4D, 0x56A0D, 0x56FF6)

// The source of the cast: 0 a spell of a book, 1 a scroll (a spell item
// of the inventory), 2 a wand (a weapon with PC_spell).
static int hand_spell(int *source)
{
	int n = Dtu_char.current.n;
	dtu_inv_item *e = (n >= 0 && n < Dtu_n_inventory) ? &Dtu_inventory[n] : NULL;

	if (Dtu_spell_current >= 0) {
		*source = 0;
		return Dtu_spell_current;
	}
	if (e && e->type == OBJ_SPELL && spell_ok(e->id)) {
		*source = 1;
		return e->id;
	}
	if (e && e->type == OBJ_WEAPON && e->id < N_weapon_types) {
		char v[16];
		if (bm_tbl_get(Weapon_item_info[e->id].record,"pc_spell",v,sizeof(v)) && atoi(v) >= 0 && spell_ok(atoi(v))) {
			*source = 2;
			return atoi(v);
		}
	}
	return -1;
}

int dtu_spell_in_hand(void)
{
	int source;

	return hand_spell(&source) >= 0;
}

// 0x356E0 and 0x41A4D: the checks of a spell of a book. Returns 1 if the
// spell can be cast, else 0 and the message (empty for none) in msg.
static int cast_allowed(int spell,char *msg,int size)
{
	int b = spell_book_of(spell), lvl = spell_level(spell), cl;

	msg[0] = 0;
	if (!char_is(DTU_CLASS_MAGE) && !char_is(DTU_CLASS_CLERIC)) {
		snprintf(msg,size,"%s",LT("You are not a spell caster."));
		return 0;
	}
	if (b == 0 && !char_is(DTU_CLASS_MAGE)) { snprintf(msg,size,"%s",LT("You are not a Mage.")); return 0; }
	if (b == 1 && !char_is(DTU_CLASS_CLERIC)) { snprintf(msg,size,"%s",LT("You are not a Cleric.")); return 0; }
	if (b < 0)
		return 0;
	cl = char_class_level(b == 0 ? DTU_CLASS_MAGE : DTU_CLASS_CLERIC);
	if (lvl - 1 > cl) {
		snprintf(msg,size,"%s",LT("Spell is of too high a level."));
		return 0;
	}
	if (lvl < 1 || lvl > 9 || Dtu_spell_slots_left[b][lvl-1] <= 0) {
		snprintf(msg,size,"%s",LT("Out of spell points for this spell level."));
		return 0;
	}
	// A mage spell in armor that is not MAGIC_OK
	if (b == 0) {
		int a = Dtu_char.slot[DTU_SLOT_ARMOR].n;
		if (a >= 0 && a < Dtu_n_inventory && Dtu_inventory[a].type == OBJ_ARMOR && Dtu_inventory[a].id < N_armor_types &&
			 !bm_tbl_has_flag(Armor_info[Dtu_inventory[a].id].record,"MAGIC_OK")) {
			snprintf(msg,size,LT("You cannot cast a spell while wearing %s."),dtu_tr_name(Dtu_inventory[a].name));
			return 0;
		}
	}
	// Player object +0x1AE bit 0x80: set by Levitate (0x8EDB2) and by the
	// climb of Climb Walls (0x9531D, 0x95413)
	if ((ConsoleObject->dtu.m_88 & DTU_FX_LEVITATE) || (Dtu_player_state & DTU_PS_CLIMBING)) {
		snprintf(msg,size,"%s",LT("You cannot cast a spell while climbing."));
		return 0;
	}
	return 1;
}

static int cast_checks(int spell)
{
	char msg[100];

	if (cast_allowed(spell,msg,sizeof(msg)))
		return 1;
	// 0x41B0D and 0x41C2D: sound 3000 (volume 1.0) with the refusals of
	// 0x356E0 and of the armor; the climbing test (0x41C6E) has no sound
	if (strcmp(msg,LT("You cannot cast a spell while climbing.")))
		digi_play_sample(3000,F1_0);
	if (msg[0])
		HUD_init_message("%s",msg);
	return 0;
}

// The number of missiles of a spell (0x56FF6). The same switch sets the
// pool of Sleep (0x5742C).
static int missile_count(int spell,int L)
{
	switch (spell) {
		case 46:
			Sleep_pool = (rand() % 7) * 2 + 4;
			dtu_spell_log("spell: Sleep pool %d",Sleep_pool);
			return 1;
		case 4:	return 6;
		case 6:	return rnd(6);
		case 7:	return rnd(4) + L;
		case 33:	return (L/2 + 1 < 5) ? L/2 + 1 : 5;
		case 34:	return rnd(3);
		case 37:	return L < 5 ? L : 5;
	}
	return 1;
}

// The start and the direction of a spell missile: the view with the look
// pitch, from the position plus 2.0 up (0x562BB).
static void spell_aim(object *player,vms_vector *dir,vms_vector *pos)
{
	vms_angvec a;
	vms_matrix head, view;

	a.p = Dtu_look_pitch; a.b = 0; a.h = 0;
	vm_angles_2_matrix(&head,&a);
	vm_matrix_x_matrix(&view,&player->orient,&head);
	*dir = view.fvec;
	*pos = player->pos;
	pos->y += i2f(2);
}

// The missile template (byte 4 of the entry): 2 hands, 3 wand, 4 scroll,
// 5 arrow
static int spell_template(int spell,int source)
{
	int w = (source == 2) ? 3 : (source == 1) ? 4 : spell_flag(spell,"cast_with_arrow") ? 5 : 2;

	return (w >= N_weapon_types) ? 0 : w;
}

static int spell_homes(int spell)
{
	return spell_flag(spell,"homing") && (spell != 33 || char_is(DTU_CLASS_MAGE));
}

// 0x56FF6: the life of the missile (W+0xA8): 30.0, Lightning Bolt 2.0,
// Melf's Acid Arrow (L/3 + 1) rounds. The duration and the range of the
// line are not used.
static fix spell_missile_life(int spell,int L)
{
	return (spell == 32) ? i2f(2) : (spell == 36) ? (L/3 + 1)*MINUTE : i2f(30);
}

static void player_missiles(int spell,int template_w,int L,int homing);

// The Spider Piece 0x14 (0x387B0): 0x56FF6(player, spell, template 2,
// hand 1) with the flags 0xC0 of the spell set for the shot (0x80 homing).
void dtu_spell_piece_missile(int spell)
{
	if (!spell_ok(spell) || ConsoleObject == NULL)
		return;
	player_missiles(spell,2 < N_weapon_types ? 2 : 0,dtu_spell_player_level(spell),1);
}

static void cast_now(int spell,int source)
{
	object *player = ConsoleObject;
	int L = dtu_spell_player_level(spell), template_w;

	sound_at(spell_int(spell,"cast_sound",10),player->segnum,&player->pos,F1_0/2);
	dtu_spell_log("spell: the player casts '%s' (%d) at level %d, source %d, at %.3f",spell_name(spell),spell,L,source,f2fl(GameTime));

	if (spell_self(spell)) {
		// Always on the player himself (0x56D36): an AREA line around him
		// (0x90CEF), else 0x8CFFF on him. 0x56D6A sets the word +0x36 of
		// Prayer (38) to 0x102 before the test: the AREA bit is clear, so
		// Prayer blesses the caster only.
		if (spell_area(spell) && spell != 38)
			dtu_spell_area(spell,&player->pos,player->segnum,player,L);
		else
			apply_spell(spell,player,player,L);
		vclip_at(player->segnum,&player->pos,i2f(3),spell_int(spell,"cast_vclip",-1));
		return;
	}

	// The missile template (byte 4 of the entry): 2 hands, 3 wand, 4
	// scroll, 5 arrow
	template_w = spell_template(spell,source);
	player_missiles(spell,template_w,L,spell_homes(spell));
}

// 0x56FF6(player, spell, template, 1): the missiles of a spell from the
// player. homing: flag 0x80 of the spell.
static void player_missiles(int spell,int template_w,int L,int homing)
{
	object *player = ConsoleObject;
	int i, n, target = -1;
	fix speed = spell_fix(spell,"speed",i2f(100));
	vms_vector dir, pos;

	// Casting a missile ends Invisibility (0x5705B)
	if (player->dtu.m_88 & DTU_FX_INVISIBLE)
		fx_end(player,17);

	spell_aim(player,&dir,&pos);
	if (homing)
		target = find_homing_target(player,&dir);

	n = missile_count(spell,L);
	for (i=0;i<n;i++) {
		vms_vector d = dir;
		int seg = find_point_seg(&pos,player->segnum), objnum;

		if (seg < 0) seg = player->segnum;
		// The spread of more than one missile (0x56EB0 was not read: guess)
		if (n > 1 && target < 0) {
			vms_vector r;
			make_random_vector(&r);
			vm_vec_scale_add2(&d,&r,F1_0/8);
			vm_vec_normalize_quick(&d);
		}
		objnum = dtu_create_missile(&d,&pos,seg,player-Objects,template_w,spell,i == 0);
		if (objnum < 0)
			break;
		vm_vec_copy_scale(&Objects[objnum].mtype.phys_info.velocity,&d,speed);
		dtu_missile_set_life(objnum,spell_missile_life(spell,L));
		track_missile(objnum,spell,L,target);
		dtu_spell_log("spell: missile obj %d of '%s', speed %.0f, target %d",objnum,spell_name(spell),f2fl(speed),target);
	}
}

// 0xB7084 for a spell of a book (0x35573: one slot of the level when the
// sub uses of the entry are used up)
static void use_slot(int spell)
{
	int b = spell_book_of(spell), lvl = spell_level(spell);

	if (b < 0 || lvl < 1 || lvl > 9)
		return;
	if (Dtu_spell_slots_left[b][lvl-1] > 0)
		Dtu_spell_slots_left[b][lvl-1]--;
	dtu_spell_log("spell: %s slots of level %d left: %d",b ? "cleric" : "mage",lvl,Dtu_spell_slots_left[b][lvl-1]);
}

int dtu_spell_fire(void)
{
	int source, spell = hand_spell(&source);

	if (spell < 0)
		return 1;
	if (spell_not_in_port(spell)) {
		HUD_init_message("%s is not in this version.",spell_name(spell));
		dtu_spell_log("spell: '%s' (%d) is not in the port",spell_name(spell),spell);
		return 1;
	}

	if (source == 0) {
		if (Cast_timer == DTU_FX_NO_END || Cast_spell != spell) {
			int t;
			char msg[64];

			if (!cast_checks(spell)) {
				Cast_timer = DTU_FX_NO_END;
				return 1;
			}
			// 0x41C9F: the cast time is a number of seconds of game time
			// (0x153EBC = GameTime + cast_time << 16), from 0x13A8DB, not
			// from the table line (spell_cast_time). The text divides by 60
			// above 60 seconds and says "1 second" for 0 and 1. The cast
			// follows in the first frame after the timer.
			t = spell_cast_time(spell);
			Cast_timer = GameTime + i2f(t);
			Cast_spell = spell;
			if (t > 60) snprintf(msg,sizeof(msg),LT("Casting time: %d minutes ..."),t / 60);
			else if (t > 1) snprintf(msg,sizeof(msg),LT("Casting time: %d seconds ..."),t);
			else snprintf(msg,sizeof(msg),"%s",LT("Casting time: 1 second ..."));
			HUD_init_message("%s",msg);
			dtu_spell_log("spell: the cast of '%s' starts, cast_time=%d, the cast at %.3f: \"%s\"",spell_name(spell),t,
				f2fl(Cast_timer),msg);
			return 0;
		}
		if (Cast_timer > GameTime)
			return 0;
		Cast_timer = DTU_FX_NO_END;
		Cast_spell = -1;
		cast_now(spell,0);
		use_slot(spell);
		return 1;
	}

	// A scroll or a wand: no cast time and no slot. One item or charge.
	cast_now(spell,source);
	dtu_char_use_up(Dtu_char.current.n);
	return 1;
}

// ASCENT: the target hint (dtuspell.h). The aim of cast_now and the
// flight of its missile, with no effect.
int dtu_spell_hint_target(void)
{
	object *player = ConsoleObject;
	int source, spell = hand_spell(&source), seg, target;
	vms_vector dir, pos;

	char msg[100];

	if (spell < 0 || spell_not_in_port(spell) || spell_self(spell))
		return -2;
	// a spell of a book: the checks of dtu_spell_fire (a scroll and a wand
	// have none)
	if (source == 0 && !cast_allowed(spell,msg,sizeof(msg)))
		return -1;
	spell_aim(player,&dir,&pos);
	if (spell_homes(spell)) {
		target = find_homing_target(player,&dir);
		if (target >= 0)
			return target;
	}
	seg = find_point_seg(&pos,player->segnum);
	if (seg < 0)
		seg = player->segnum;
	return dtu_missile_path_target(&pos,seg,&dir,spell_template(spell,source),spell,player-Objects,
		fixmul(spell_fix(spell,"speed",i2f(100)),spell_missile_life(spell,dtu_spell_player_level(spell))));
}

//------------------------------------------------------------------------
// The armor class and the to-hit bonus of the effects (0xAFB55, 0x2A849)

int dtu_spell_player_ac(int ac,int stage)
{
	object *p = ConsoleObject;
	int w;

	if (p == NULL || p->type != OBJ_PLAYER)
		return ac;
	w = p->dtu.m_88;
	if (stage == 0) {
		if ((w & DTU_FX_ARMOR) && ac > 6) ac = 6;
		if (w & DTU_FX_VESTMENT) {
			int L = dtu_spell_player_level(35), v = 5 - (L - 5) / 3;
			if (v < 1) v = 1;
			if (v > 5) v = 5;
			if (ac > v) ac = v;
		}
		if ((w & DTU_FX_SHIELD) && ac > 3) ac = 3;
		if (w & DTU_FX_SLOW) ac += 4;
	}
	else if (w & DTU_FX_INVULNERABLE)
		ac -= 2;
	return ac;
}

// 0x2ADAF: one of Magical Vestment (at most max(5 - (L - 5) / 3, 1) with
// the level of the creature), Shield (at most 3) or Armor (at most 6),
// in this order; then Slow adds 4, at most 10.
int dtu_spell_creature_ac(object *c,int ac)
{
	int w = c->dtu.m_88;

	if (w & DTU_FX_VESTMENT) {
		int v = 5 - (creature_level(c) - 5) / 3;

		if (v < 1) v = 1;
		if (ac > v) ac = v;
	}
	else if (w & DTU_FX_SHIELD) {
		if (ac > 3) ac = 3;
	}
	else if ((w & DTU_FX_ARMOR) && ac > 6)
		ac = 6;
	if (w & DTU_FX_SLOW) {
		ac += 4;
		if (ac > 10) ac = 10;
	}
	return ac;
}

int dtu_spell_unseen(object *obj)
{
	return obj && caster_unseen(obj);
}

//------------------------------------------------------------------------
// Calls for the items and the combat code

static int rand_mod(int n)
{
	// The saturating divide handler of the executable (0xF72AB): x % 0 is 0
	return n == 0 ? 0 : rand() % n;
}

int dtu_spell_set_effect(object *obj,int mask,fix duration,int keep_later,int message)
{
	int b = fx_bit(mask);
	fix end = (duration >= DTU_FX_NO_END) ? DTU_FX_NO_END : GameTime + duration;

	if (obj == NULL)
		return 0;
	if (Fx_timer[b]) {
		fix *t = timer_ref(obj,Fx_timer[b]);
		if (!(keep_later && (obj->dtu.m_88 & mask) && (*t == DTU_FX_NO_END || *t > end)))
			*t = end;
	}
	obj->dtu.m_88 |= mask;
	if (message && obj->type == OBJ_PLAYER) {
		if (duration >= DTU_FX_NO_END)
			HUD_init_message("%s.",dtu_tr(Fx_name[b]));
		else if (duration >= MINUTE)
			HUD_init_message("%s for %d minutes.",dtu_tr(Fx_name[b]),timer_ms(duration) / 60);
		else
			HUD_init_message("%s for %d seconds.",dtu_tr(Fx_name[b]),timer_ms(duration));
	}
	dtu_spell_log("spell: effect %s set on obj %d %s for %.1f seconds%s",Fx_name[b],(int)(obj-Objects),target_name(obj),
		duration >= DTU_FX_NO_END ? -1.0 : f2fl(duration),keep_later ? " (the later end stays)" : "");
	return 1;
}

void dtu_spell_clear_effect(object *obj,int mask)
{
	int b = fx_bit(mask);

	if (obj == NULL || !(obj->dtu.m_88 & mask))
		return;
	if (Fx_timer[b])
		*timer_ref(obj,Fx_timer[b]) = 0;
	obj->dtu.m_88 &= ~mask;
	dtu_spell_log("spell: effect %s of obj %d %s taken away",Fx_name[b],(int)(obj-Objects),target_name(obj));
}

void dtu_spell_dispel(object *obj)
{
	if (obj)
		fx_expire(obj,1);
}

fix dtu_spell_cast_on(int spell,object *target,object *caster)
{
	int L = object_level(caster,spell);

	if (!spell_ok(spell) || target == NULL)
		return 0;
	if (spell_area(spell))
		return dtu_spell_area(spell,&target->pos,target->segnum,caster,L);
	return apply_spell(spell,target,caster,L);
}

// 0x9305D
int dtu_spell_interrupt(object *obj,int hit)
{
	if (obj == NULL)
		return 0;
	if (obj->type == OBJ_PLAYER) {
		if (Cast_timer == DTU_FX_NO_END || Cast_timer <= GameTime || Cast_spell < 0)
			return 0;
		Cast_timer = DTU_FX_NO_END;
		dtu_combat_cast_stopped();
		dtu_spell_log("spell: the cast of '%s' stops (%s) at %.3f",spell_name(Cast_spell),hit ? "a hit" : "the jump key",f2fl(GameTime));
		if (hit) {
			use_slot(Cast_spell);		// 0xB7084
			HUD_init_message("Your spell was interrupted!");
		}
		else
			HUD_init_message("Your spell was aborted.");
		Cast_spell = -1;
		return 1;
	}
	if (obj->type == OBJ_ROBOT)
		return creature_interrupt(obj,hit);
	return 0;
}

// 0x8F2C6: 0 "Three 1st", 1 "One 1st and one 2nd", 2 "One 3rd". Only the
// maximum is raised: the next rest fills the slots.
static void rary_apply(int c)
{
	if (c == 0) Rary_bonus[0] += 3;
	else if (c == 1) { Rary_bonus[0]++; Rary_bonus[1]++; }
	else Rary_bonus[2]++;
	dtu_spell_compute_slots();
	dtu_spell_log("spell: Rary's Mnemonic Enhancer, choice %d: extra mage slots %d %d %d, maximum %d %d %d, left %d %d %d",c,
		Rary_bonus[0],Rary_bonus[1],Rary_bonus[2],Dtu_spell_slots_max[0][0],Dtu_spell_slots_max[0][1],Dtu_spell_slots_max[0][2],
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_left[0][2]);
}

void dtu_spell_set_wizardry(int n)
{
	if (n == Wizardry_n)
		return;
	Wizardry_n = n;
	dtu_spell_compute_slots();
	dtu_spell_log("spell: Ring of Wizardry: %d doublings, mage maximum %d %d %d",n,
		Dtu_spell_slots_max[0][0],Dtu_spell_slots_max[0][1],Dtu_spell_slots_max[0][2]);
}

//------------------------------------------------------------------------
// The spells of the creatures (0x1490B, 0x92D99, 0x9231F, 0x91FDC)

// 0x142084: 44 entries of 9 bytes: the spell, the condition bits of
// +0x15E that it answers, the effect bits of the creature that stop it,
// the Robot_info+0x98 flags that stop it. 0x92FF2 shuffles it at a rest.
typedef struct { ubyte spell; ushort cond; uint effects; ushort flags; } cspell_entry;

static cspell_entry Cspell_table[44] = {
	{1,0xE00,0x2,0x0}, {2,0xA20,0x1,0x0}, {3,0xB20,0x2,0x0}, {4,0x7DC,0x4,0x0},
	{5,0x798,0x8,0x0}, {6,0x7DC,0x4,0x0}, {7,0x7DC,0x0,0x0}, {9,0x7DC,0x0,0x300},
	{8,0x7DC,0x0,0x300}, {10,0xD580,0x0,0x0}, {11,0xD580,0x0,0x0}, {13,0x120,0x0,0x0},
	{55,0x120,0x0,0x0}, {14,0xEFFF,0x0,0x0}, {15,0x1800,0x80,0x20}, {16,0x8344,0x4,0x0},
	{17,0x8344,0x4,0x0}, {18,0x1C5,0x400,0x18}, {19,0x5A20,0x800,0x20}, {20,0x6900,0x1000,0x0},
	{21,0x6900,0x2000,0x0}, {22,0xEFFF,0x0,0x0}, {52,0xEFFF,0x0,0x0}, {24,0xCA20,0x30000,0x0},
	{26,0xCA20,0x30000,0x0}, {29,0x1020,0x200000,0x20}, {30,0x7DC,0x0,0x300}, {31,0x7DC,0x0,0x300},
	{32,0x8FFF,0x0,0x0}, {33,0xAFFF,0x0,0x0}, {34,0xAFFF,0x4,0x0}, {35,0xA20,0x80000000,0x0},
	{36,0xAFFF,0x0,0x0}, {37,0xAFFF,0x4,0x0}, {38,0xB20,0x2,0x0}, {40,0x1C5,0x200,0x18},
	{42,0xFFFF,0x0,0x1}, {43,0x1C5,0x400,0x18}, {44,0xEFFF,0x0,0x0}, {45,0xB64,0x2000000,0x0},
	{46,0xEFFF,0x0,0x0}, {47,0xEFFF,0x0,0x0}, {49,0xFFFF,0x20000000,0x0}, {50,0xEFFF,0x40000000,0x0}
};
static int Cspell_next = 0;		// 0x142210

// The Robot_info+0x98 flag bits of the executable for the table masks
static int robot98_flags(object *obj)
{
	int mf = monster_flags(obj), r = 0;

	if (mf & MONSTER_F_BLIND) r |= 0x1;
	if (mf & MONSTER_F_ELECTRICITY_IMMUNE) r |= 0x2;
	if (mf & MONSTER_F_EXTRAPLANAR) r |= 0x4;
	if (mf & MONSTER_F_FIRE_IMMUNE) r |= 0x8;
	if (monster_has(obj,"fire_resistant")) r |= 0x10;
	if (mf & MONSTER_F_FLYING) r |= 0x20;
	if (mf & MONSTER_F_HUMANOID) r |= 0x40;
	if (mf & MONSTER_F_LIGHT_SENSITIVE) r |= 0x100;
	if (monster_has(obj,"light_sensitive_2")) r |= 0x200;
	return r;
}

// The spell state of a creature: +0x19E, +0x19F (mage and cleric points),
// +0x1A8 (the time of the new points), +0x1C0 (the time of the cast),
// ailp+0x2C (the chosen entry), +0x15E (the condition bits).
static struct {
	int		signature;
	int		points[2];
	fix		recharge_time, cast_time;
	int		chosen;
	int		cond;
} Cspell[MAX_OBJECTS];

static int cspell_state(object *obj)
{
	int n = obj-Objects;

	if (Cspell[n].signature != obj->signature) {
		Cspell[n].signature = obj->signature;
		// 0x1038B step 11: the points of the monster table
		Cspell[n].points[0] = creature_points(obj,0);
		Cspell[n].points[1] = creature_points(obj,1);
		Cspell[n].recharge_time = 0;
		Cspell[n].cast_time = 0;
		Cspell[n].chosen = -1;
		Cspell[n].cond = 0;
	}
	return n;
}

// The restore of a level (0xAE3F2 at 0xBB1A7): the points of the type, the
// two times are the time of the restore, no condition bits. The chosen
// entry (ailp+0x2C) comes from the saved AI block in the executable; the
// port has none there and keeps no entry.
void dtu_spell_creature_restore(object *obj)
{
	int n;

	if (obj == NULL || obj->type != OBJ_ROBOT)
		return;
	n = obj-Objects;
	Cspell[n].signature = obj->signature;
	Cspell[n].points[0] = creature_points(obj,0);
	Cspell[n].points[1] = creature_points(obj,1);
	Cspell[n].recharge_time = GameTime;
	Cspell[n].cast_time = GameTime;
	Cspell[n].chosen = -1;
	Cspell[n].cond = 0;
}

void dtu_spell_creature_event(object *obj,int bits)
{
	if (obj == NULL || obj->type != OBJ_ROBOT)
		return;
	Cspell[cspell_state(obj)].cond |= bits;
}

// 0x9305D for a creature: the cast that waits is lost
static int creature_interrupt(object *obj,int hit)
{
	int n = cspell_state(obj), e = Cspell[n].chosen;

	if (Cspell[n].cast_time <= GameTime || e < 0)
		return 0;
	if (hit) {
		int s = Cspell_table[e].spell, b = spell_flag(s,"mage_use") ? 0 : 1;
		Cspell[n].points[b] -= spell_level(s);
		if (Cspell[n].points[0] < 1 && Cspell[n].points[1] < 1) {
			Cspell[n].points[0] = Cspell[n].points[1] = 0;
			Cspell[n].recharge_time = GameTime + (9 - creature_mage_level(obj) - creature_cleric_level(obj)) * MINUTE;
		}
	}
	Cspell[n].chosen = -1;
	Cspell[n].cast_time = GameTime;
	dtu_spell_log("cspell: obj %d %s: the cast of '%s' is interrupted",n,target_name(obj),spell_name(Cspell_table[e].spell));
	return 1;
}

// 0x10815: an fvi ray with a radius from the eye of the creature to the
// player
static int creature_clear(object *obj,fix rad)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector eye = obj->pos;
	int f;

	if (!(monster_flags(obj) & MONSTER_F_FLYING))
		eye.y += i2f(5);
	fq.p0 = &eye;
	fq.p1 = &ConsoleObject->pos;
	fq.startseg = obj->segnum;
	fq.rad = rad;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = FQ_CHECK_OBJS;
	if (find_point_seg(&eye,obj->segnum) < 0)
		fq.p0 = &obj->pos;
	f = find_vector_intersection(&fq,&hit);
	return f == HIT_NONE || (f == HIT_OBJECT && hit.hit_object == ConsoleObject-Objects);
}

// 0x91F66: no other hostile creature in the segment of the player
static int creature_safe(object *obj)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		if (o != obj && o->type == OBJ_ROBOT && o->segnum == ConsoleObject->segnum && !(o->flags & OF_SHOULD_BE_DEAD) &&
			 o->shields >= F1_0 && !(o->dtu.m_1b0 & DTU_PEACEFUL))
			return 0;
	}
	return 1;
}

static fix floor_height(object *obj)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector p1 = obj->pos;

	p1.y -= i2f(200);
	fq.p0 = &obj->pos;
	fq.p1 = &p1;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) != HIT_WALL)
		return 0;
	return obj->pos.y - hit.hit_pnt.y;
}

// 0x9231F: the condition and the weight of one entry. Returns -1 if the
// condition fails.
static int cspell_weight(object *obj,int e,fix dist)
{
	object *pl = ConsoleObject;
	int s = Cspell_table[e].spell, L = creature_level(obj), D = f2i(dist);
	int pw = pl->dtu.m_88, cw = obj->dtu.m_88, mf = monster_flags(obj);
	int AC = obj->id < N_robot_types ? Monster_info[obj->id].armor_class : 10;
	int hp = f2i(obj->shields), max = obj->id < N_robot_types ? Monster_info[obj->id].max_hit_points : 1;
	int slowed = (cw & DTU_FX_SLOW) != 0, stuck = (Cspell[obj-Objects].cond & DTU_CSC_SLOWED) != 0;
	int fleeing = (obj->dtu.m_1b0 & DTU_FLEEING) != 0;
	int v;

	if (max < 1) max = 1;
	switch (s) {
		case 1:	return 40 + rand_mod(41);
		case 2:	return AC > 6 ? 32*AC - 192 : -1;
		case 3:	case 38:	return rand_mod((32 - L) * 4);
		case 4:	return creature_clear(obj,i2f(30)) ? 50 + ((pw & 0x40600) ? 0 : rand_mod(50)) : -1;
		case 5:	return dist < i2f(200) ? 100 - D : -1;
		case 6:	return creature_clear(obj,i2f(30)) ? 25 + ((pw & DTU_FX_BLIND) ? 0 : rand_mod(75)) : -1;
		case 7:	return (creature_safe(obj) && !(pw & DTU_FX_CONFUSION)) ? 25 + rand_mod(26) : -1;
		case 8: case 9: case 30: case 31:
			return (pw & DTU_FX_BLIND) ? -1 : 25 + rand_mod(51);
		case 10: case 11:
			return hp < max - (s == 10 ? 4 : 10) ? 100 - 100*hp/max : -1;
		case 14:	return (pw & DTU_FX_FEAR) ? -1 : 25 + rand_mod(26);
		case 44:
			if (pw & DTU_FX_FEAR) return -1;
			v = !(Dtu_char.pa.race == DTU_RACE_ELF || (Dtu_char.pa.flags & DTU_PF_HALF_ELF) || char_is(DTU_CLASS_CLERIC));
			return rand_mod(50 - 16*L) + 50*v;
		case 15:	return f2i(floor_height(obj));
		case 16: case 17:
			if (!creature_safe(obj))
				return -1;
			if ((cw & (DTU_FX_PROT_FIRE|DTU_FX_RESIST_FIRE)) || (mf & MONSTER_F_FIRE_IMMUNE) || monster_has(obj,"fire_resistant") ||
				 D > spell_int(s,"radius",0))
				return 25 + rand_mod(26);
			return -1;
		case 18: case 40: case 43:
			v = Cspell[obj-Objects].cond & DTU_CSC_FIRE ? 1 : 0;
			return v ? 20 + rand_mod(40*v) : -1;
		case 19: case 29:
			if (!((pw & (DTU_FX_FLY|DTU_FX_LEVITATE)) || pl->pos.y - obj->pos.y >= i2f(20)))
				return -1;
			return (s == 19) ? 20 + rand_mod(40*fleeing) : 25 + rand_mod(51);
		case 20:	return (slowed || stuck || fleeing) ? 50 + rand_mod(51) : -1;
		case 21:
			if (!(slowed || stuck || fleeing || (pw & DTU_FX_HASTE)))
				return -1;
			return slowed ? 100 : 40 + rand_mod(41);
		case 22: case 52:
			return (pw & (DTU_FX_HOLD|DTU_FX_FREE_ACTION)) ? -1 : 50 + rand_mod(D % 50);
		case 24: case 26:
			return 100 - 100*hp/max + rand_mod(50 * !creature_clear(obj,0));
		case 32: {
			fvi_query fq;
			fvi_info hit;
			vms_vector p1;
			vm_vec_scale_add(&p1,&obj->pos,&obj->orient.fvec,i2f(160));
			fq.p0 = &obj->pos; fq.p1 = &p1; fq.startseg = obj->segnum; fq.rad = 0;
			fq.thisobjnum = obj-Objects; fq.ignore_obj_list = NULL; fq.flags = 0;
			return find_vector_intersection(&fq,&hit) == HIT_WALL ? -1 : 50;
		}
		case 33:	return (creature_clear(obj,i2f(5)) && !(pw & DTU_FX_SHIELD)) ? 50 : -1;
		case 34:	return creature_clear(obj,i2f(30)) ? 50 : -1;
		case 37:	return creature_clear(obj,i2f(30)) ? 30 : -1;
		case 35:
			v = 5 - (L - 5) / 3;
			if (v < 1) v = 1;
			return AC > v ? 8 * (AC - v) : -1;
		case 36:	return 50;
		case 45:	return AC > 3 ? 16*AC - 48 : -1;
		case 46:	return (creature_safe(obj) && !(pw & DTU_FX_SLEEP)) ? 100 - 12*L : -1;
		case 47:	return (pw & DTU_FX_SLOW) ? -1 : 50 + rand_mod((pw & DTU_FX_HASTE) ? 50 : 0);
		case 49:	return 25 + rand_mod(25*(hp < max) + 25*!(char_is(DTU_CLASS_MAGE) || char_is(DTU_CLASS_CLERIC)));
		case 50:	return D > 3 ? D : -1;
	}
	return -1;		// "bad spell"
}

// 0x91FDC: the cast
static void creature_cast(object *obj)
{
	int n = obj-Objects, e = Cspell[n].chosen, s, L = creature_level(obj), b;

	if (e < 0)
		return;
	s = Cspell_table[e].spell;
	dtu_spell_log("cspell: obj %d %s casts '%s' (%d) at level %d, points %d %d",n,target_name(obj),spell_name(s),s,L,
		Cspell[n].points[0],Cspell[n].points[1]);
	// 0x92104: +0x37 bit 0 (self_cast_ok, cast_in_front, or set by the
	// choice of Dispel Magic): on the creature itself, an AREA line
	// around it (0x90CEF), else 0x8CFFF. No case of Prayer here: as TARGET
	// it blesses the creature only.
	if (spell_self(s) || s == 13 || s == 55) {
		if (spell_area(s))
			dtu_spell_area(s,&obj->pos,obj->segnum,obj,L);
		else
			apply_spell(s,obj,obj,L);
		vclip_at(obj->segnum,&obj->pos,i2f(3),spell_int(s,"cast_vclip",-1));
	}
	else {
		// 0x56FF6(obj, spell, template 3, hand 1): the missile goes from the
		// creature to the player (guess: the facing of the creature, which
		// turns to the player in the attack)
		vms_vector dir, pos = obj->pos;
		int seg, m, k, count = missile_count(s,L);

		pos.y += i2f(2);
		seg = find_point_seg(&pos,obj->segnum);
		if (seg < 0) { pos = obj->pos; seg = obj->segnum; }
		vm_vec_sub(&dir,&ConsoleObject->pos,&pos);
		vm_vec_normalize_quick(&dir);
		sound_at(spell_int(s,"cast_sound",10),obj->segnum,&obj->pos,F1_0/2);
		for (k=0;k<count;k++) {
			m = dtu_create_missile(&dir,&pos,seg,n,3 < N_weapon_types ? 3 : 0,s,k == 0);
			if (m < 0)
				break;
			vm_vec_copy_scale(&Objects[m].mtype.phys_info.velocity,&dir,spell_fix(s,"speed",i2f(100)));
			dtu_missile_set_life(m,(s == 32) ? i2f(2) : (s == 36) ? (L/3 + 1)*MINUTE : i2f(30));
		}
	}
	b = spell_flag(s,"mage_use") ? 0 : 1;
	Cspell[n].points[b] -= spell_level(s);
	Cspell[n].chosen = -2;
	if (Cspell[n].points[0] < 1 && Cspell[n].points[1] < 1) {
		Cspell[n].points[0] = Cspell[n].points[1] = 0;
		Cspell[n].recharge_time = GameTime + (9 - creature_mage_level(obj) - creature_cleric_level(obj)) * MINUTE;
		dtu_spell_log("cspell: obj %d %s has no spell points: new points at %.1f",n,target_name(obj),f2fl(Cspell[n].recharge_time));
	}
}

// 0x92D99: the choice of a spell
static void creature_choose(object *obj,fix dist)
{
	int n = obj-Objects, k, ml = creature_mage_level(obj), cl = creature_cleric_level(obj), e = 0;
	int rf = robot98_flags(obj);

	if (Cspell[n].points[0] < 1 && Cspell[n].points[1] < 1) {
		Cspell[n].points[0] = creature_points(obj,0);
		Cspell[n].points[1] = creature_points(obj,1);
		Cspell[n].cond |= 0x8000;
	}
	for (k=0;k<44;k++) {
		int s, lvl, w, roll;

		e = Cspell_next = (Cspell_next + 1) % 44;
		s = Cspell_table[e].spell;
		if (!(Cspell_table[e].cond & Cspell[n].cond))
			continue;
		lvl = spell_level(s);
		if (spell_flag(s,"mage_use") && (ml < lvl || Cspell[n].points[0] < lvl))
			continue;
		if (spell_flag(s,"cleric_use") && (cl < lvl || Cspell[n].points[1] < lvl))
			continue;
		if (obj->dtu.m_88 & Cspell_table[e].effects)
			continue;
		if (rf & Cspell_table[e].flags)
			continue;
		if (s == 13 || s == 55) {
			// Dispel Magic: at once on itself when the player or the
			// creature has effects; the scan goes on
			if ((ConsoleObject->dtu.m_88 & 0xF3FFBECB) || (obj->dtu.m_88 & 0x0C004124)) {
				Cspell[n].chosen = e;
				creature_cast(obj);
			}
			continue;
		}
		w = cspell_weight(obj,e,dist);
		if (w < 0)
			continue;
		if (w > 100) w = 100;
		roll = rand() % 100 + 1;
		if (roll > w)
			continue;
		Cspell[n].cond &= ~Cspell_table[e].cond;
		Cspell[n].chosen = e;
		Cspell[n].cast_time = GameTime + i2f(spell_cast_time(s));	// 0x9231F reads the same word
		dtu_spell_log("cspell: obj %d %s chooses '%s' (%d), weight %d, roll %d, cast at %.2f",n,target_name(obj),spell_name(s),s,w,roll,
			f2fl(Cspell[n].cast_time));
		return;
	}
	Cspell[n].cond &= ~Cspell_table[e].cond;
	Cspell[n].chosen = -2;
}

void dtu_spell_rest_shuffle(void)
{
	int i;

	for (i=0;i<44;i++) {
		int j = rand() % 44;
		cspell_entry t = Cspell_table[i];
		Cspell_table[i] = Cspell_table[j];
		Cspell_table[j] = t;
	}
	dtu_spell_log("cspell: the creature spell table is shuffled (0x92FF2)");
}

int dtu_spell_is_area(int spell)
{
	return spell_area(spell);
}

void dtu_spell_cast_vclip(int spell,object *obj)
{
	if (obj && spell_ok(spell))
		vclip_at(obj->segnum,&obj->pos,i2f(3),spell_int(spell,"cast_vclip",-1));
}

void dtu_spell_creature_frame(object *obj,fix dist,int active)
{
	int n;

	if (obj == NULL || obj->type != OBJ_ROBOT || ConsoleObject == NULL || Player_is_dead || Dtu_player_dead)
		return;
	if (!active || obj->shields < F1_0 || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	if (creature_mage_level(obj) <= 0 && creature_cleric_level(obj) <= 0)
		return;
	n = cspell_state(obj);
	if (Cspell[n].cond == 0)
		return;
	if (Cspell[n].chosen < 0) {
		if (Cspell[n].recharge_time <= GameTime)
			creature_choose(obj,dist);
	}
	else if (Cspell[n].cast_time <= GameTime)
		creature_cast(obj);
}

//------------------------------------------------------------------------
// The frame

static void spell_init(void)
{
	static int done = 0;
	int t;

	if (done)
		return;
	done = 1;
	// the load of the character (dtu_char_load_save) has read the books
	// already: a second read would undo the changes of the level start
	// (dtuchar.c, Dtu_fix_spell_save_repair)
	if ((t = FindArg("-char")) != 0 && t+1 < Num_args && !Dtu_char.created)
		load_books_from_save(Args[t+1]);
}

void dtu_spell_frame(void)
{
	static int last_level = -1;
	int i, t;

	spell_init();
	if (ConsoleObject == NULL || !Dtu_objects_enabled)
		return;

	// The level start (0x4225A): the slots are full
	if (last_level != Current_level_num) {
		last_level = Current_level_num;
		dtu_spell_compute_slots();
		memcpy(Dtu_spell_slots_left,Dtu_spell_slots_max,sizeof(Dtu_spell_slots_left));		// 0x4225A calls 0x3551B
		Cast_timer = DTU_FX_NO_END;
		memset(Tracked,0,sizeof(Tracked));
		acid_clear();
		// Test options: -testslots N gives N slots for each spell level,
		// -testspell N puts spell N into the book and into the hand.
		if ((t = FindArg("-testslots")) != 0 && t+1 < Num_args) {
			int b, l;
			for (b=0;b<2;b++) for (l=0;l<9;l++) Dtu_spell_slots_left[b][l] = Dtu_spell_slots_max[b][l] = atoi(Args[t+1]);
		}
		if ((t = FindArg("-testspell")) != 0 && t+1 < Num_args) {
			int s = atoi(Args[t+1]);
			dtu_spell_learn(s);
			if (!dtu_spell_select(s))
				dtu_spell_log("spell: -testspell %d: the character cannot learn the spell",s);
		}
	}

	// A new item in the hand takes the place of the spell
	if (Dtu_spell_current >= 0 && Dtu_char.current.n != Hand_ref)
		Dtu_spell_current = -1;

	// 0x9002C for the player (game.c 0x438BD) and the creatures (0x14386)
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		if ((o->type == OBJ_PLAYER || o->type == OBJ_ROBOT) && o->dtu.m_88)
			fx_expire(o,0);
	}
	acid_frame();

	tracked_frame();
}

//------------------------------------------------------------------------
// The spell list of the statistics screen. The executable shows the spell
// tables of the statistics screen and the spell lists as a view of the
// inventory list (0x35ABD). Their places were not decoded: the port draws
// a list of the books in the empty panel at the lower left (guess).

static int screen_count(void)
{
	return Dtu_spell_book_n[0] + Dtu_spell_book_n[1];
}

static int screen_spell(int i)
{
	if (i < Dtu_spell_book_n[0])
		return Dtu_spell_book[0][i];
	return Dtu_spell_book[1][i - Dtu_spell_book_n[0]];
}

void dtu_spell_screen_keys(int prev,int next,int take)
{
	int n = screen_count();

	if (n <= 0)
		return;
	if (prev && Screen_sel > 0) Screen_sel--;
	if (next && Screen_sel < n - 1) Screen_sel++;
	if (Screen_sel >= n) Screen_sel = n - 1;
	if (take) {
		int s = screen_spell(Screen_sel);
		if (Dtu_spell_current == s)
			Dtu_spell_current = -1;
		else
			dtu_spell_select(s);
	}
}

void dtu_spell_screen_draw(int hires)
{
	int n = screen_count(), i, x = hires ? 40 : 20, y = hires ? 284 : 141, dy = hires ? 12 : 6, first;
	int white = gr_find_closest_color(63,63,63), gray = gr_find_closest_color(40,40,40), orange = gr_find_closest_color(63,40,10);
	char text[80];

	if (!GAME_FONT)
		return;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(gray,-1);
	if (n == 0) {
		gr_string(x,y,LT("No spells"));
		return;
	}
	sprintf(text,LT("Slots M %d/%d %d/%d %d/%d C %d/%d %d/%d %d/%d"),
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_max[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_max[0][1],
		Dtu_spell_slots_left[0][2],Dtu_spell_slots_max[0][2],Dtu_spell_slots_left[1][0],Dtu_spell_slots_max[1][0],
		Dtu_spell_slots_left[1][1],Dtu_spell_slots_max[1][1],Dtu_spell_slots_left[1][2],Dtu_spell_slots_max[1][2]);
	gr_string(x,y,text);
	first = Screen_sel - 4;
	if (first > n - 8) first = n - 8;
	if (first < 0) first = 0;
	for (i=first;i<n && i<first+8;i++) {
		int s = screen_spell(i);
		gr_set_fontcolor(s == Dtu_spell_current ? orange : (i == Screen_sel ? white : gray),-1);
		sprintf(text,"%c %d %s",i < Dtu_spell_book_n[0] ? 'M' : 'C',spell_level(s),spell_name(s));
		gr_string(x,y + dy*(1 + i - first),text);
	}
}
