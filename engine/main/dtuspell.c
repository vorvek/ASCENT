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
#include "dtuxp.h"		// ASCENT: the cause of a kill in the session log

void make_random_vector(vms_vector *vec);		// ai.c

int Dtu_fix_chill_touch = 1;
int Dtu_fix_stoneskin_word = 1;
int Dtu_fix_rary_manual = 1;
int Dtu_fix_melf_acid = 1;
int Dtu_fix_spell_hours = 1;
int Dtu_fix_cure_aid = 1;			// ASCENT: priest spells (dtuspell.h)
// ASCENT: D-105 (dtuspell.h). The flag word of Prayer after 0x56D6A: the
// bits of the reader 0x242F6, +0x36: 1 AREA, 2 stackable, 0x1C the cast
// (0 hands, 4 wand, 8 scroll, 0x10 arrow), 0x20 potion, 0x40 always, 0x80
// homing; +0x37: 1 self_cast_ok, 2 non_hotkey_item, 4 cast_in_front, 8
// hit_objects (the table fixup). 43 of the 46 tables already have 0x102 on
// line 38; DEMO and MOIN1 have cast_with_wand, BASE has cast_in_front.
int Dtu_fix_prayer_flags = 1;
static int Prayer_written;		// the player cast Prayer since the table read

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
// ASCENT: Spell_info[22]+0x49 and Spell_info[52]+0x49, the counters of
// Hold Person (0x56FF6, 0x8E8BB)
static int Hold_count[2] = { 0, 0 };

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
static fix Cast_loaded = DTU_FX_NO_END;	// ASCENT: the cast of a loaded save, for the level start
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
	// ASCENT: the word 0x102 of 0x56D6A (Dtu_fix_prayer_flags)
	if (spell == 38 && Prayer_written && !Dtu_fix_prayer_flags) {
		static char *on[] = { "stackable", "self_cast_ok", "cast_with_hands" };
		static char *off[] = { "cast_with_wand", "cast_with_scroll", "cast_with_arrow", "potion", "always", "homing",
			"non_hotkey_item", "cast_in_front", "hit_objects" };
		int i;

		for (i = 0; i < (int) (sizeof(on) / sizeof(on[0])); i++)
			if (!stricmp(flag,on[i]))
				return spell_ok(spell);
		for (i = 0; i < (int) (sizeof(off) / sizeof(off[0])); i++)
			if (!stricmp(flag,off[i]))
				return 0;
	}
	return spell_ok(spell) && bm_tbl_has_flag(Spell_info[spell].record,flag);
}

void dtu_spell_table_read(void)
{
	Prayer_written = 0;
	dtu_spell_dice_clear();
}

static char *spell_name(int spell)
{
	return spell_ok(spell) ? dtu_tr_name(Spell_info[spell].name) : "?";		// ASCENT: the language (dtulang.h)
}

// Spell_info +0x36 bit 0: type=AREA
static int spell_area(int spell)
{
	char v[16];

	if (spell == 38 && Prayer_written && !Dtu_fix_prayer_flags)
		return 0;		// ASCENT: the word 0x102 of 0x56D6A (Dtu_fix_prayer_flags)
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
// ASCENT: the cleric bit first, as the cast check 0x35683 tests it. After
// the table fixup (dtutblfix.h) Protection From Fire (40, class 0x27) has
// both bits: 1.3 casts it as a cleric spell.
static int spell_book_of(int spell)
{
	if (spell_flag(spell,"cleric_use")) return 1;
	if (spell_flag(spell,"mage_use")) return 0;
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

// ASCENT: the caster level for statement EB of the scripts (0x90CEF with
// the object of the script as the caster, dtuscript.c)
int dtu_spell_object_level(object *obj,int spell)
{
	return object_level(obj,spell);
}

static int monster_flags(object *obj)
{
	return (obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types) ? Monster_info[obj->id].flags : 0;
}

static int monster_has(object *obj,char *flag)
{
	return obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types && bm_tbl_has_flag(Monster_info[obj->id].record,flag);
}

// ASCENT: a MONSTER_F_ bit, which the creature setup by name (0xAC7B2) can
// also set (magic_resistant of the lich)
static int monster_flag(object *obj,int bit)
{
	return obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types && (Monster_info[obj->id].flags & bit);
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

// ASCENT: 0x3551B for the new character (0x8BFFE) and the game start
// (0x42284). The rule of Dtu_fix_rary_manual counts only the rest.
void dtu_spell_fill_slots(void)
{
	memcpy(Dtu_spell_slots_left,Dtu_spell_slots_max,sizeof(Dtu_spell_slots_left));
	dtu_spell_log("spell: the slots are full (mage %d %d %d, cleric %d %d %d)",
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_left[0][2],
		Dtu_spell_slots_left[1][0],Dtu_spell_slots_left[1][1],Dtu_spell_slots_left[1][2]);
}

// 0x141C38 (dtuspell.h)
static int Fill_at_start = 0;
static int Level_started = 0;

void dtu_spell_fill_at_start(int on)
{
	Fill_at_start = on;
}

void dtu_spell_level_start(void)
{
	Level_started = 1;
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
	int m = dtu_spell_use_classes(spell);

	if (m == DTU_USE_ALL)
		return 1;
	return ((m & DTU_USE_MAGE) && char_is(DTU_CLASS_MAGE)) || ((m & DTU_USE_CLERIC) && char_is(DTU_CLASS_CLERIC));
}

// ASCENT: the classes of the rule above as DTU_USE_ bits (dtuchar.h), for
// the description window too (dtudesc.c).
int dtu_spell_use_classes(int spell)
{
	if (spell == 39 || spell == 40 || spell == 53)
		return DTU_USE_ALL;
	return (spell_flag(spell,"mage_use") ? DTU_USE_MAGE : 0) | (spell_flag(spell,"cleric_use") ? DTU_USE_CLERIC : 0);
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
			// ASCENT: 0x9013F: a creature is cut to its maximum too
			// (Robot_info+0x78, whole points)
			else if (obj->type == OBJ_ROBOT && obj->id < N_robot_types && obj->shields > i2f(Monster_info[obj->id].max_hit_points))
				obj->shields = i2f(Monster_info[obj->id].max_hit_points);
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

// ASCENT: the test aid of dtu_spell_priest_check(): -1 rolls, 0 fails
// every save, 1 makes every save
static int Priest_force_save = -1;

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

	if (Priest_force_save >= 0)
		return Priest_force_save;
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
		if (monster_flag(obj,MONSTER_F_MAGIC_RESISTANT))
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
	dtu_xp_save(roll,need,roll >= need);		// ASCENT: the verbose combat log (dtuxp.h)
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

// ASCENT: the sound of Aid (0x8D435) and of the cures on the player
// (0x8DEA8, 0x8DF97): 0x2E900 with the word P+0x1B33, the happy_sound of
// the player type (+0xE0 of the reader 0x1F495), at the player, volume 1.0.
static void priest_happy_sound(void)
{
	object *pl = ConsoleObject;

	if (pl && pl->type == OBJ_PLAYER && N_player_types > 0)
		sound_at(dtu_tbl_int(Player_type_info[0].record,"happy_sound",-1),pl->segnum,&pl->pos,F1_0);
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
static int rule_half(int spell);		// ASCENT: the save for half of the line of the spell

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
	return spell_deal(spell,target,caster,level,dmg,rule_half(spell));
}

// The part of 0x911AA after the roll: Invulnerability, the save for half
// ("save" 0: none, Dtu_fix_melf_acid), the Armor pool, the sound, the
// vclip, the text and the damage
static fix spell_deal(int spell,object *target,object *caster,int level,int dmg,int save)
{
	fix dealt = 0;

	if (target->type == OBJ_PLAYER && (target->dtu.m_88 & DTU_FX_INVULNERABLE))
		dmg = 0;		// 0x9178B
	if (dmg > 0 && save && dtu_spell_save(target,DTU_SAVE_SPELL)) {
		dmg /= 2;
		dtu_xp_dmg_mult(-2,LN("save for half"));		// ASCENT: the verbose combat log (dtuxp.h)
	}
	else if (dmg > 0)
		dtu_spell_interrupt(target,1);		// 0x913A7, 0x917D5
	dtu_xp_dmg_end(dmg);
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
	dtu_xp_roll_say(caster,target);		// ASCENT: the verbose combat log (dtuxp.h)
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
// caster, else the gender of the name of the creature (female for a
// feminine name), else male. Msg_female is the one of the next say().
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
	// a creature or an item: the gender of its name in the language
	// (the field gender of the catalog, docs/formats/lang.md)
	Msg_t_female = (target == ConsoleObject) ? dtu_lang_female() :
		target->type != OBJ_LIGHT && dtu_object_name_gender(target) == DTU_GENDER_F;
	Msg_c_female = (caster == ConsoleObject) ? dtu_lang_female() :
		caster != NULL && caster->type != OBJ_LIGHT && dtu_object_name_gender(caster) == DTU_GENDER_F;
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
		priest_happy_sound();		// ASCENT: 0x8DEA8, 0x8DF97: only for the player
		// ASCENT: 0x8DEE2 adds the points and then cuts the hit points to the
		// maximum, so a cure takes away the hit points of Aid above the
		// maximum. Dtu_fix_cure_aid keeps them (AD&D: they go first).
		if (!Dtu_fix_cure_aid && before + points > dtu_char_max_hit_points()) {
			dtu_char_add_hit_points(points);
			dtu_player_damage(i2f(dtu_char_hit_points() - dtu_char_max_hit_points()));
		}
		else
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

// ASCENT: the rolls of the rules of a spell in three modes: ROLL (the cast,
// rand() as before), LOW and HIGH (the lowest and the highest result, for
// the description window: no rand() call)
#define ROLL	0
#define LOW		1
#define HIGH	2

static int die(int n,int mode)		// 1 to n
{
	return mode == ROLL ? rnd(n) : mode == LOW ? 1 : n;
}

static int die0(int n,int mode)		// 0 to n - 1
{
	return mode == ROLL ? rand() % n : mode == LOW ? 0 : n - 1;
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
#define ACID_DICE	2		// ASCENT: 2d4 for each round (dtu_spell_get_facts)
#define ACID_SIDES	4

static fix acid_damage(int spell,object *target,object *caster,int level)
{
	int dmg = 0, i;

	for (i=0;i<ACID_DICE;i++)
		dmg += rnd(ACID_SIDES);

	target->dtu.m_88 &= ~DTU_FX_REGENERATE;
	return spell_deal(spell,target,caster,level,dmg,0);
}

static void acid_add(int spell,object *target,object *caster,int L,int rounds);

// The hit of the arrow. The THAC0 of a fighter is the fighter row of the
// table 0x146590 (dtuchar.c): 21 - L, at most 20, at least 3 (level 18).
static int spell_amount(int spell,int L,int mode);		// ASCENT: below

static fix melf_acid_hit(int spell,object *target,object *caster,int L)
{
	int thac0 = 21 - L, d20 = rnd(20), ac = 0, hit, rounds = spell_amount(36,L,ROLL);
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
	acid_add(spell,target,caster,L,rounds + 1);
	return dealt;
}

// The later rounds: round 1 was done now, "rounds" is the last round
static void acid_add(int spell,object *target,object *caster,int L,int rounds)
{
	int i;

	for (i=0;i<MAX_ACID && Acid[i].used;i++)
		;
	if (i == MAX_ACID) {
		dtu_spell_log("spell: '%s': no free acid entry, the acid ends after the first round",spell_name(spell));
		return;
	}
	Acid[i].used = 1;
	Acid[i].obj = target-Objects;
	Acid[i].obj_sig = target->signature;
	Acid[i].caster = caster ? caster-Objects : -1;
	Acid[i].caster_sig = caster ? caster->signature : 0;
	Acid[i].spell = spell;
	Acid[i].level = L;
	Acid[i].round = 1;
	Acid[i].rounds = rounds;
	Acid[i].start = GameTime;
	Acid[i].next = GameTime + ACID_ROUND;
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

// ASCENT: a duration of hours (Dtu_fix_spell_hours). 1.3 computes Strength
// (L * 225 << 20, 0x8FF5D) and Infravision in 32 bits: from caster level 10
// (Strength) and 8 (Infravision) the value wraps below 0 and the effect ends
// at once. A fix holds at most 9.1 hours, and the port reads a duration of
// DTU_FX_NO_END or more as no end, so the duration stops just below that
// (4 hours 33 minutes).
static fix long_duration(int seconds)
{
	int max = (DTU_FX_NO_END - 1) / F1_0;

	if (seconds > max) {
		dtu_spell_log("spell: %d seconds is above the longest timer: %d seconds (Dtu_fix_spell_hours)",seconds,max);
		seconds = max;
	}
	return i2f(seconds);
}

// ASCENT: the three times of Color Spray by the level of the target T:
// 0 sleep (T <= L), 1 blindness (T <= L + 2), 2 stun (the others)
static fix color_time(int branch,int mode)
{
	return branch == 0 ? (2 + die0(7,mode))*MINUTE : branch == 1 ? die(4,mode)*MINUTE : MINUTE;
}

// ASCENT: the duration of the effect of a spell at caster level L, as the
// cases of apply_spell set it (a case calls this with ROLL), and the lowest
// and the highest value for the description window (LOW, HIGH). 0: no timer.
// Color Spray has three times by the level of the target (color_time).
static fix spell_time(int spell,int L,int mode)
{
	switch (spell) {
		case 6:	return mode == HIGH ? color_time(0,HIGH) : color_time(2,LOW);	// Color Spray: 1 to 8 rounds
		case 1:	return (L+1)*MINUTE;					// Aid: Bless
		case 3:	return i2f(360);						// Bless: 6 rounds
		case 5:	return (L+3)*MINUTE;					// Chill Touch
		case 7:	return (L+2)*MINUTE;					// Confusion
		case 8: case 9:	return DTU_FX_NO_END;		// Continual Light
		case 30:	return 10*L*MINUTE;					// Light
		case 31:	return 10*(L+6)*MINUTE;
		case 12:	return 2*L*MINUTE;					// Detect Magic (mage)
		case 54:	return i2f(600);						// Detect Magic (cleric)
		case 14:	return L*MINUTE;						// Fear
		case 44:	return (die(4,mode) + L)*MINUTE;	// Scare
		case 15:	return L*MINUTE;						// Feather Fall
		case 18:	return (L+1)*MINUTE;					// Flame Walk
		case 19:	return 10*(die(6,mode) + L)*MINUTE;	// Fly
		case 20:	return 10*L*MINUTE;					// Free Action
		case 21:	return (L+3)*MINUTE;					// Haste
		case 22: case 52:	return 2*L*MINUTE;		// Hold Person
		case 23:	return (die(4,mode) + L)*MINUTE;	// Hold Undead
		case 24:	return (L+4)*MINUTE;					// Improved Invisibility
		case 25:	return Dtu_fix_spell_hours ? long_duration((6*L + 12)*600) : (6*L + 12)*10*MINUTE;	// Infravision
		case 26:	return DTU_FX_NO_END;				// Invisibility: until an attack
		case 27:	return (die(3,mode) + L)*MINUTE;	// Jump
		case 29:	return 10*L*MINUTE;					// Levitate
		case 35:	return 5*L*MINUTE;					// Magical Vestment
		case 38:	return L*MINUTE;						// Prayer
		case 39:	return 2*L*MINUTE;					// Protection From Evil (mage)
		case 53:	return 3*L*MINUTE;					// Protection From Evil (cleric)
		case 43:	return L*MINUTE;						// Resist Fire
		case 45:	return 5*L*MINUTE;					// Shield
		case 46:	return 5*L*MINUTE;					// Sleep
		case 47:	return (L+3)*MINUTE;					// Slow
		case 48:	return (L+3)*MINUTE;					// Spider Climb
		case 50:	return Dtu_fix_spell_hours ? long_duration(L*3600) : (fix)((unsigned)L * 3600u * (unsigned)F1_0);	// Strength
		case 51:	return i2f(1800);						// Find Traps: 30 rounds
	}
	return 0;
}

#define SLEEP_LEVEL		3		// ASCENT: Sleep takes targets of this level or less (0x8F766)
#define SCARE_LEVEL		6		// Scare (0x8F467)

// ASCENT: the amount of a spell at caster level L in the three modes: the
// hit points of Aid and the cures, the pools of Armor and Protection From
// Fire, the hits of Stone Skin, the strength of Strength (by the class of
// the player), the targets of Hold Person, the highest level of a target
// of Hold Undead, Scare and Sleep, the later rounds of Melf's Acid Arrows
// (Dtu_fix_melf_acid). The cases and the description window read it.
static int spell_amount(int spell,int L,int mode)
{
	switch (spell) {
		case 1:	return die(8,mode);					// Aid
		case 2:	return 8 + L;							// Armor
		case 10:	return die(8,mode);					// Cure Light Wounds
		case 11:	return die0(15,mode) + 3;			// Cure Serious Wounds: 3 to 17
		case 40:	return 12*L;							// Protection From Fire
		case 49:	return die(4,mode) + L/2;			// Stone Skin
		case 22: case 52:	return die(4,mode);		// Hold Person: the counter of the cast (0x56FF6)
		case 23:	return L;								// Hold Undead: undead of level L or less
		case 36:	return L/3;							// Melf's Acid Arrows: the later rounds (melf_acid_hit)
		case 44:	return SCARE_LEVEL;					// Scare
		case 46:	return SLEEP_LEVEL;					// Sleep
		case 50:	return char_is(DTU_CLASS_FIGHTER) ? die(8,mode) :
						(char_is(DTU_CLASS_THIEF) || char_is(DTU_CLASS_CLERIC)) ? die(6,mode) : die(4,mode);
	}
	return 0;
}

// ASCENT: the dice group, the dice count and the level of the damage roll
// of a spell (the cases write the count of their group, 0x8E592, 0x8EEAB,
// 0x8EFE0). Magic Missile rolls with level 1.
static void spell_damage_args(int spell,int L,int *group,int *count,int *level)
{
	*group = GROUP_NONE;
	*count = 0;
	*level = L;
	switch (spell) {
		case 16:	*group = GROUP_FIRE; *count = L < 10 ? L : 10; break;		// Fireball
		case 32:	*group = GROUP_ELEC; *count = L < 10 ? L : 10; break;		// Lightning Bolt
		case 36:	*group = GROUP_ACID; *count = 2*(L/3) + 2; break;			// Melf's Acid Arrows of 1.3
		case 33:	*level = 1; break;											// Magic Missile
	}
}

#define STONE_UNDEAD_DIE	4		// ASCENT: Magical Stone, one more die against undead (case 34)

// ASCENT: the rule of each spell, read by apply_spell and by the
// description window (dtu_spell_get_facts): the kind of the effect text
// (DTU_SK_), the saving throw (DTU_SS_), the conditions (DTU_SR_) and the
// effect bit (DTU_FX_). apply_spell sets and tests the bit of its line
// (rule_fx), rolls the save of a case only when the line has one and the
// target is one that saves (rule_save, save_level), a damage spell saves
// for half when its line says so (spell_damage), and each case tests the
// conditions of its line through rule_reaches. So a change of a line
// changes the game and the text. The numbers (time, amount, dice) are the
// functions spell_time, spell_amount and spell_damage_args, which both
// read too. dtu_spell_rule_check tests that the kind of each line fits its
// other values.
#define N_SPELL_RULES	56

#define SR_OTHERS	DTU_SR_SPARES_CASTER

static const struct { ubyte kind, save, flags; int fx, stopped_by; } Spell_rule[N_SPELL_RULES] = {
	{ DTU_SK_ABJURE, DTU_SS_NONE, 0, 0 },						// 0 Abjure: a d20 and a d100, no save
	{ DTU_SK_AID, DTU_SS_NONE, 0, DTU_FX_BLESS },				// 1 Aid
	{ DTU_SK_ARMOR, DTU_SS_NONE, 0, DTU_FX_ARMOR },				// 2 Armor
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_BLESS },				// 3 Bless
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS, 0 },				// 4 Burning Hands
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_CHILL_TOUCH },		// 5 Chill Touch
	{ DTU_SK_COLOR, DTU_SS_NEGATES, SR_OTHERS, 0 },				// 6 Color Spray: a save above save_level
	{ DTU_SK_CAUSE, DTU_SS_NEGATES, 0, DTU_FX_CONFUSION },		// 7 Confusion
	{ DTU_SK_BLIND, DTU_SS_NEGATES, SR_OTHERS, DTU_FX_BLIND },	// 8 Continual Light
	{ DTU_SK_BLIND, DTU_SS_NEGATES, SR_OTHERS, DTU_FX_BLIND },	// 9
	{ DTU_SK_HEAL, DTU_SS_NONE, DTU_SR_NO_UNDEAD, 0 },			// 10 Cure Light Wounds (and not extraplanar)
	{ DTU_SK_HEAL, DTU_SS_NONE, DTU_SR_NO_UNDEAD, 0 },			// 11 Cure Serious Wounds
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_DETECT_MAGIC },		// 12 Detect Magic
	{ DTU_SK_DISPEL, DTU_SS_NONE, 0, 0 },						// 13 Dispel Magic
	{ DTU_SK_FEAR, DTU_SS_NEGATES, SR_OTHERS|DTU_SR_NO_UNDEAD, DTU_FX_FEAR },	// 14 Fear
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_FEATHER_FALL },		// 15 Feather Fall
	{ DTU_SK_DAMAGE, DTU_SS_HALF, DTU_SR_BURST_BLINDS, 0 },		// 16 Fireball: no test of the caster
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS, 0 },				// 17 Produce Fire
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_RESIST_FIRE },		// 18 Flame Walk
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_FLY },				// 19 Fly
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_FREE_ACTION },		// 20 Free Action
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_HASTE },				// 21 Haste
	{ DTU_SK_HOLD, DTU_SS_NEGATES, SR_OTHERS|DTU_SR_NO_UNDEAD|DTU_SR_HUMANOID, DTU_FX_HOLD },	// 22 Hold Person
	{ DTU_SK_HOLD_UNDEAD, DTU_SS_NEGATES, SR_OTHERS|DTU_SR_UNDEAD_ONLY|DTU_SR_LEVEL_CAP, DTU_FX_HOLD },	// 23 Hold Undead: a save above save_level
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_IMP_INVIS },			// 24 Improved Invisibility
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_INFRAVISION },		// 25 Infravision
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_INVISIBLE },			// 26 Invisibility
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_JUMP },				// 27 Jump
	{ DTU_SK_KNOCK, DTU_SS_NONE, 0, 0 },						// 28 Knock (the wall hit of dtuobj.c)
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_LEVITATE },			// 29 Levitate
	{ DTU_SK_BLIND, DTU_SS_NEGATES, SR_OTHERS, DTU_FX_BLIND },	// 30 Light
	{ DTU_SK_BLIND, DTU_SS_NEGATES, SR_OTHERS, DTU_FX_BLIND },	// 31
	{ DTU_SK_DAMAGE, DTU_SS_HALF, 0, 0 },						// 32 Lightning Bolt
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS|DTU_SR_STOPPED, 0, DTU_FX_SHIELD },	// 33 Magic Missile: Shield stops it
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS, 0 },				// 34 Magical Stone
	{ DTU_SK_AC, DTU_SS_NONE, 0, DTU_FX_VESTMENT },				// 35 Magical Vestment
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS, 0 },				// 36 Melf's Acid Arrows (see below)
	{ DTU_SK_DAMAGE, DTU_SS_HALF, SR_OTHERS, 0 },				// 37 Melf's Minute Meteors
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_BLESS },				// 38 Prayer
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_PROT_EVIL },			// 39 Protection From Evil
	{ DTU_SK_FIRE_POOL, DTU_SS_NONE, 0, DTU_FX_PROT_FIRE },		// 40 Protection From Fire
	{ DTU_SK_RARY, DTU_SS_NONE, 0, DTU_FX_MNEMONIC },			// 41 Rary's Mnemonic Enhancer
	{ DTU_SK_CURE_SENSES, DTU_SS_NONE, 0, 0 },					// 42 Cure Blindness or Deafness
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_RESIST_FIRE },		// 43 Resist Fire
	{ DTU_SK_CAUSE, DTU_SS_NEGATES, SR_OTHERS|DTU_SR_NO_UNDEAD|DTU_SR_LEVEL_CAP|DTU_SR_SAVE_KIN, DTU_FX_FEAR },	// 44 Scare
	{ DTU_SK_AC, DTU_SS_NONE, 0, DTU_FX_SHIELD },				// 45 Shield
	{ DTU_SK_CAUSE, DTU_SS_NONE, SR_OTHERS|DTU_SR_NO_UNDEAD|DTU_SR_LEVEL_CAP, DTU_FX_SLEEP },	// 46 Sleep
	{ DTU_SK_CAUSE, DTU_SS_NEGATES, SR_OTHERS, DTU_FX_SLOW },	// 47 Slow
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_SPIDER_CLIMB },		// 48 Spider Climb
	{ DTU_SK_STONESKIN, DTU_SS_NONE, 0, DTU_FX_STONESKIN },		// 49 Stone Skin
	{ DTU_SK_STRENGTH, DTU_SS_NONE, 0, DTU_FX_STRENGTH },		// 50 Strength
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_FIND_TRAPS },			// 51 Find Traps
	{ DTU_SK_HOLD, DTU_SS_NEGATES, SR_OTHERS|DTU_SR_NO_UNDEAD|DTU_SR_HUMANOID, DTU_FX_HOLD },	// 52 Hold Person
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_PROT_EVIL },			// 53 Protection From Evil
	{ DTU_SK_GIVE, DTU_SS_NONE, 0, DTU_FX_DETECT_MAGIC },		// 54 Detect Magic
	{ DTU_SK_DISPEL, DTU_SS_NONE, 0, 0 },						// 55 Dispel Magic
};

static int rule_flags(int spell)
{
	return (spell >= 0 && spell < N_SPELL_RULES) ? Spell_rule[spell].flags : 0;
}

// ASCENT: the sides of the one more die against undead (Magical Stone,
// case 34), 0 none
static int spell_undead_die(int spell)
{
	return spell == 34 ? STONE_UNDEAD_DIE : 0;
}

// ASCENT: the level above which a target gets the save of the line, -1
// every target. Color Spray: a target above the caster level or of level 6
// or more (T > L || T >= 6, case 6); Hold Undead: above level 2 (case 23).
#define COLOR_SAVE_LEVEL	5
#define HOLD_UNDEAD_SAVE_LEVEL	2

static int save_level(int spell,int L)
{
	switch (spell) {
		case 6:		return L < COLOR_SAVE_LEVEL ? L : COLOR_SAVE_LEVEL;
		case 23:	return HOLD_UNDEAD_SAVE_LEVEL;
	}
	return -1;
}

// ASCENT: DTU_SR_SAVE_KIN (0x8F467, Scare): the player saves only as an
// elf, a half-elf or a cleric; a creature only with a cleric level or with
// +0xD9 bit 3
static int kin_saves(object *target)
{
	if (target->type == OBJ_PLAYER)
		return Dtu_char.pa.race == DTU_RACE_ELF || (Dtu_char.pa.flags & DTU_PF_HALF_ELF) || char_is(DTU_CLASS_CLERIC);
	if (target->type == OBJ_ROBOT)
		return creature_cleric_level(target) || creature_rf_name(target);
	return 1;
}

// A damage spell saves for half when its line says so. The other spells
// that deal damage (Abjure of a creature, the lights on a light
// sensitive creature) always save for half, as in 1.3 (0x911AA).
static int rule_half(int spell)
{
	if (spell < 0 || spell >= N_SPELL_RULES || Spell_rule[spell].kind != DTU_SK_DAMAGE)
		return 1;
	return Spell_rule[spell].save == DTU_SS_HALF;
}

// The save of the line: none for a line with no save, for a target at or
// below save_level and, with DTU_SR_SAVE_KIN, for a target of another kin
static int rule_save(int spell,object *target,int T,int L)
{
	if (spell < 0 || spell >= N_SPELL_RULES || Spell_rule[spell].save == DTU_SS_NONE)
		return 0;
	if (save_level(spell,L) >= 0 && T <= save_level(spell,L))
		return 0;
	if ((Spell_rule[spell].flags & DTU_SR_SAVE_KIN) && !kin_saves(target))
		return 0;
	return dtu_spell_save(target,DTU_SAVE_SPELL);
}

// ASCENT: 1 if the conditions (DTU_SR_) of the line let the spell act on
// the target; the cases test them here, the window prints them
static int rule_reaches(int spell,object *target,int self,int T,int L,int mf)
{
	int fl = rule_flags(spell);

	if (self && (fl & DTU_SR_SPARES_CASTER))
		return 0;
	if ((fl & DTU_SR_UNDEAD_ONLY) && !(mf & MONSTER_F_UNDEAD))
		return 0;
	if ((fl & DTU_SR_NO_UNDEAD) && (mf & MONSTER_F_UNDEAD))
		return 0;
	if ((fl & DTU_SR_HUMANOID) && target->type == OBJ_ROBOT && !(mf & MONSTER_F_HUMANOID))
		return 0;
	if ((fl & DTU_SR_LEVEL_CAP) && T > spell_amount(spell,L,ROLL))
		return 0;
	if ((fl & DTU_SR_STOPPED) && (target->dtu.m_88 & Spell_rule[spell].stopped_by))
		return 0;
	return 1;
}

int dtu_spell_stopped_by_fx(int fx)
{
	int s;

	for (s=0;s<N_SPELL_RULES;s++)
		if ((Spell_rule[s].flags & DTU_SR_STOPPED) && (Spell_rule[s].stopped_by & fx))
			return s;
	return -1;
}

//------------------------------------------------------------------------
// The effect of a spell on one target (0x8CFFF, jump table 0x8CF1F)

static fix apply_spell(int spell,object *target,object *caster,int L)
{
	int T, self = (target == caster), mf;
	int rule_fx = (spell >= 0 && spell < N_SPELL_RULES) ? Spell_rule[spell].fx : 0;	// ASCENT: the effect of the line
	fix dealt = 0;
	fix dur = 0;		// ASCENT: the duration that a case set, for its message (spell_time)

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
			int n = spell_amount(1,L,ROLL);
			fix d = spell_time(1,L,ROLL);

			priest_happy_sound();		// ASCENT: 0x8D435, also when the target is not the player
			if (target->type == OBJ_PLAYER)
				dtu_char_add_hit_points(n);
			else
				target->shields += i2f(n);
			target->dtu.m_88 &= ~rule_fx;
			fx_set(target,rule_fx,d);
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
			if (target->dtu.m_88 & rule_fx)
				break;
			target->dtu.m_88 |= rule_fx;
			target->dtu.m_8c[1] = i2f(spell_amount(2,L,ROLL));
			// "%s %s %d armor hit points." (0x12D928) on the caster himself,
			// else "%d armor hit points cast on %s by %s." (0x12D943)
			if (self)
				say("%s %s %d armor hit points.",Msg_t,Msg_have,spell_amount(2,L,ROLL));
			else
				say("%d armor hit points cast on %s by %s.",spell_amount(2,L,ROLL),Msg_t,Msg_c);
			dtu_spell_log("spell: effect Armor on obj %d %s, pool %d hit points",(int)(target-Objects),target_name(target),spell_amount(2,L,ROLL));
			break;
		case 3:		// Bless: 6 rounds
			if (fx_set(target,rule_fx,dur = spell_time(3,L,ROLL)))
				say_effect(self,"%s %s blessed for %d minutes.",V_ARE,"Bless cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 4:		// Burning Hands: fire 1d3 + 2 L
		case 17:		// Flaming Sphere (no dice in the line); 0x1A344 makes it the
						// cleric spell Produce Fire (level 4, fire 1d4 + 1 per level)
		case 37:		// Melf's Minute Meteors: 1d4
			if (rule_reaches(spell,target,self,T,L,mf))
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			break;
		case 5:		// Chill Touch: on the caster only, L+3 rounds
			if (self && fx_set(target,rule_fx,dur = spell_time(5,L,ROLL)))
				say("%s %s chill touch for %d minutes.",Msg_t,Msg_have,MINUTES(dur));
			break;
		case 6:		// Color Spray
			if (!rule_reaches(spell,target,self,T,L,mf) || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & DTU_FX_BLIND))
				break;
			if (rule_save(spell,target,T,L))
				break;
			if (T <= L) {
				if (fx_set(target,DTU_FX_SLEEP,color_time(0,ROLL))) {
					if (target->type == OBJ_PLAYER) {
						target->dtu.m_88 |= DTU_FX_BLIND;
						target->dtu.m_8c[3] = target->dtu.m_e8[0];
					}
					say("%s %s struck unconscious by %s.",Msg_t,Msg_were,Msg_c);
				}
			}
			else if (T <= L + 2) {
				if (fx_set(target,DTU_FX_BLIND,color_time(1,ROLL)))
					say("%s %s blinded by %s.",Msg_t,Msg_were,Msg_c);
			}
			else if (!(target->dtu.m_88 & (DTU_FX_FREE_ACTION|DTU_FX_HOLD))) {
				if (fx_set(target,DTU_FX_HOLD,wisdom_cut(target,color_time(2,ROLL))))
					say("%s %s stunned by %s.",Msg_t,Msg_were,Msg_c);
			}
			break;
		case 7:		// Confusion: L+2 rounds
			if (!rule_save(spell,target,T,L) && fx_set(target,rule_fx,dur = spell_time(7,L,ROLL)))
				say("Confusion cast on %s by %s for %d minutes.",Msg_t,Msg_c,MINUTES(dur));
			break;
		case 8: case 9:		// Continual Light: blind with no end
		case 30: case 31:		// Light: blind for 10 L or 10 (L+6) rounds
			if (!rule_reaches(spell,target,self,T,L,mf) || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & rule_fx))
				break;
			// ASCENT: 0x8DC4D: a save ends the case (0x90024), so a light
			// sensitive creature takes the damage only with the blindness
			if (rule_save(spell,target,T,L))
				return 0;
			{
				fix d = spell_time(spell,L,ROLL);

				if (fx_set(target,rule_fx,d)) {
					if (d == DTU_FX_NO_END)
						say("%s %s blinded by %s indefinitely.",Msg_t,Msg_were,Msg_c);
					else
						say("%s %s blinded by %s for %d minutes.",Msg_t,Msg_were,Msg_c,MINUTES(d));
				}
			}
			if (mf & MONSTER_F_LIGHT_SENSITIVE)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			// ASCENT: 0x8DD89: a blinded player gets a white flash (each
			// color +254, at most 30)
			if (target == ConsoleObject)
				PALETTE_FLASH_ADD(254,254,254);
			break;
		case 10:		// Cure Light Wounds: 1d8, not undead or extraplanar
		case 11:		// Cure Serious Wounds: 3 to 17
			if (!rule_reaches(spell,target,self,T,L,mf) || (mf & MONSTER_F_EXTRAPLANAR))
				break;
			heal(target,spell_amount(spell,L,ROLL),spell);
			if (spell == 10)
				say("Cure light wounds cast on %s by %s.",Msg_t,Msg_c);
			else
				say("Cure serious wounds cast on %s by %s.",Msg_t,Msg_c);
			break;
		case 12:		// Detect Magic (mage): 2 L rounds
		case 54:		// Detect Magic (cleric): 10 rounds
			// The text only when the player is the target (0x8E08A)
			if (fx_set(target,rule_fx,dur = spell_time(spell,L,ROLL)) && target == ConsoleObject)
				say("%s can detect magic for %d minutes.",Msg_t,MINUTES(dur));
			break;
		case 13: case 55:	// Dispel Magic: all effects of the target end, no save
			fx_expire(target,1);
			if (target->type == OBJ_PLAYER)
				dtu_char_potions_end(1);		// 0x396B2(1)
			break;
		case 14:		// Fear: L rounds
		case 44:		// Scare: 1d4 + L rounds, targets of level 6 or less
			if (!rule_reaches(spell,target,self,T,L,mf) || (mf & MONSTER_F_NO_SCARE) || (target->dtu.m_88 & rule_fx))
				break;
			if (spell == 44 && (mf & MONSTER_F_EXTRAPLANAR))
				break;
			// 0x8F467: Scare has a save only for an elf, a half-elf or a
			// cleric player, and for a creature with +0xD9 bit 3 or a cleric
			// level (DTU_SR_SAVE_KIN of its line, kin_saves)
			if (rule_save(spell,target,T,L))
				break;
			{
				fix d = wisdom_cut(target,spell_time(spell,L,ROLL));
				int drop = (spell == 14) ? (rand() % 100 + 1 <= 65 - 5*T) : (rand() % 2 != 0);

				fx_set(target,rule_fx,d);
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
					dtu_xp_cause("Abjure");		// ASCENT: the session log (dtuxp.h)
					dealt = dtu_damage_creature_direct(target,NULL,target->shields);
					dtu_xp_cause(NULL);
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
			if ((target->dtu.m_88 & rule_fx) && !Dtu_fix_rary_manual) {
				dtu_spell_log("spell: Rary's Mnemonic Enhancer is on already: no effect (0x8F226)");
				return 0;
			}
			target->dtu.m_88 |= rule_fx;
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
			if (fx_set(target,rule_fx,dur = spell_time(15,L,ROLL)))
				say_effect(self,"%s %s feather fall for %d minutes.",V_HAVE,"Feather fall cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 16:		// Fireball: min(L, 10) d6 of fire (the fixup 0x1A9C7: 6 sides, 0 per level)
			{
				int g, n, l;

				spell_damage_args(spell,L,&g,&n,&l);
				dealt = spell_damage(spell,target,caster,l,g,n,0);
			}
			break;
		case 18:		// Flame Walk: L+1 rounds
			if (fx_set(target,rule_fx,dur = spell_time(18,L,ROLL)))
				say_effect(self,"%s %s fire tolerance for %d minutes.",V_HAVE,"Flame Walk cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 19:		// Fly: 10 (1d6 + L) rounds
			if (fx_set(target,rule_fx,spell_time(19,L,ROLL)))
				say_effect(self,"%s %s Flying.",V_HAVE,"Flying cast on %s by %s.",0);
			break;
		case 20:		// Free Action: ends Hold, 10 L rounds
			// ASCENT: 0x8E6F8: Free Action on already returns before the
			// test of Hold
			if (!self || (target->dtu.m_88 & rule_fx))
				return 0;
			if (target->dtu.m_88 & DTU_FX_HOLD)
				fx_end(target,14);
			if (fx_set(target,rule_fx,dur = spell_time(20,L,ROLL)))
				say_effect(self,"%s %s Free Action for %d minutes.",V_HAVE,"Free Action cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 21:		// Haste: ends Slow, L+3 rounds
			if (target->dtu.m_88 & DTU_FX_SLOW)
				fx_end(target,27);
			if (fx_set(target,rule_fx,dur = spell_time(21,L,ROLL)))
				say_effect(self,"%s %s Haste for %d minutes.",V_HAVE,"Haste cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 22: case 52:	// Hold Person: 2 L rounds, a humanoid or the player
			if (!rule_reaches(spell,target,self,T,L,mf) || (target->dtu.m_88 & (rule_fx|DTU_FX_FREE_ACTION)))
				break;
			if (!rule_save(spell,target,T,L)) {
				fix d = wisdom_cut(target,spell_time(spell,L,ROLL));

				// ASCENT: 0x8E8C5: the counter of the cast must be above 0
				if (Hold_count[spell == 52] <= 0) {
					dtu_spell_log("spell: Hold Person counter is 0: no effect");
					return 0;
				}
				Hold_count[spell == 52]--;

				// The text in seconds below one minute (0x8E9C5)
				if (fx_set(target,rule_fx,d)) {
					if (f2i(d) < 60)
						say_effect(self,"%s %s held for %d seconds.",V_ARE,"Hold Person cast on %s by %s for %d seconds.",f2i(d));
					else
						say_effect(self,"%s %s held for %d minutes.",V_ARE,"Hold Person cast on %s by %s for %d minutes.",MINUTES(d));
				}
			}
			break;
		case 23:		// Hold Undead: 1d4 + L rounds, undead of level L or less
			if (!rule_reaches(spell,target,self,T,L,mf) || (target->dtu.m_88 & rule_fx))
				break;
			if (rule_save(spell,target,T,L))
				break;
			{
				fix d = spell_time(23,L,ROLL);
				if (fx_set(target,rule_fx,d))
					say("Hold Undead cast on %s by %s for %d minutes.",Msg_t,Msg_c,MINUTES(d));
			}
			break;
		case 24:		// Improved Invisibility: L+4 rounds
			if (fx_set(target,rule_fx,dur = spell_time(24,L,ROLL)))
				say_effect(self,"%s %s invisible for %d minutes.",V_ARE,"Improved Invisibility cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 25:		// Infravision: humans only, 2 hours and 1 hour for each level
			if (target->type == OBJ_PLAYER && Dtu_char.pa.race != DTU_RACE_HUMAN)
				break;
			if (monster_has(target,"infravision"))
				break;
			{
				fix d = spell_time(25,L,ROLL);
				if (fx_set(target,rule_fx,d))
					say_effect(self,"%s %s Infravision for %d hours.",V_HAVE,"Infravision cast on %s by %s for %d hours.",HOURS(d));
			}
			break;
		case 26:		// Invisibility: until an attack
			if (target->dtu.m_88 & (DTU_FX_IMP_INVIS|rule_fx))
				break;
			if (fx_set(target,rule_fx,spell_time(26,L,ROLL)))
				say_effect(self,"%s %s invisible.",V_ARE,"Invisibility cast on %s by %s.",0);
			break;
		case 27:		// Jump: 1d3 + L rounds
			if (fx_set(target,rule_fx,spell_time(27,L,ROLL)))
				say_effect(self,"%s may leap for some time.",V_NONE,"Jump cast on %s by %s.",0);
			break;
		case 28:		// Knock: the case has no code (the table entry is the common end)
			dtu_spell_log("spell: Knock has no effect in the executable");
			break;
		case 29:		// Levitate: 10 L rounds
			if (fx_set(target,rule_fx,dur = spell_time(29,L,ROLL)))
				say_effect(self,"%s can levitate for %d minutes.",V_NONE,"Levitate cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 32:		// Lightning Bolt: min(L, 10) d6 of electricity (the fixup 0x1AAE9)
			{
				int g, n, l;

				spell_damage_args(spell,L,&g,&n,&l);
				dealt = spell_damage(spell,target,caster,l,g,n,0);
			}
			break;
		case 33:		// Magic Missile: 1d4 with the level 1, Shield stops it
			if (!rule_reaches(spell,target,self,T,L,mf))
				break;
			{
				int g, n, l;

				spell_damage_args(spell,L,&g,&n,&l);
				dealt = spell_damage(spell,target,caster,l,g,n,0);
			}
			break;
		case 34:		// Magical Stone: 1d4, two dice against undead
			if (rule_reaches(spell,target,self,T,L,mf))
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,(mf & MONSTER_F_UNDEAD) ? rnd(spell_undead_die(spell)) : 0);
			break;
		case 35:		// Magical Vestment: 5 L rounds
			if (fx_set(target,rule_fx,dur = spell_time(35,L,ROLL)))
				say_effect(self,"%s %s Magical Vestment for %d minutes.",V_HAVE,"Magical Vestment cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 36:		// Melf's Acid Arrows: 2 (L/3) + 2 acid dice + 4 L
			if (!rule_reaches(spell,target,self,T,L,mf))
				break;
			if (Dtu_fix_melf_acid)
				dealt = melf_acid_hit(spell,target,caster,L);
			else {
				int g, n, l;

				spell_damage_args(spell,L,&g,&n,&l);
				dealt = spell_damage(spell,target,caster,l,g,n,0);
			}
			break;
		case 38:		// Prayer: Bless for L rounds on the caster and on the others
			// whose bit 0x200 of 0x1B0 is clear (as read). The casts of the
			// player (0x56D6A) and of a creature reach only the caster.
			if ((self || !(target->dtu.m_1b0 & DTU_PEACEFUL)) && fx_set(target,rule_fx,dur = spell_time(38,L,ROLL)))
				say_effect(self,"%s %s Prayer for %d minutes.",V_HAVE,"Prayer cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 39:		// Protection From Evil (mage): 2 L rounds
		case 53:		// Protection From Evil (cleric): 3 L rounds
			if (fx_set(target,rule_fx,dur = spell_time(spell,L,ROLL)))
				say_effect(self,"%s %s Protection From Evil for %d minutes.",V_HAVE,
					"Protection From Evil cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 40:		// Protection From Fire: a pool of 12 L hit points
			// ASCENT: 0x8F1B2: no effect while the pool of bit 9 lasts (the
			// port added a second pool)
			if (target->dtu.m_88 & rule_fx)
				return 0;
			target->dtu.m_82 += spell_amount(40,L,ROLL);
			target->dtu.m_88 |= rule_fx;
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
			// ASCENT: 0x8F37C sets the timer of the blindness to 0 and leaves
			// bit 2, so 0x9002C ends it in the next frame ("Blinded
			// expired.", the end action); 0x8F396 clears the deafness at once
			target->dtu.m_88 &= ~DTU_FX_DEAF;
			if (target->dtu.m_88 & DTU_FX_BLIND)
				target->dtu.m_8c[3] = 0;
			break;
		case 43:		// Resist Fire: L rounds
			if (fx_set(target,rule_fx,dur = spell_time(43,L,ROLL)))
				say_effect(self,"%s %s Fire Resistance for %d minutes.",V_HAVE,"Resist Fire cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 45:		// Shield: 5 L rounds
			if (fx_set(target,rule_fx,dur = spell_time(45,L,ROLL)))
				say_effect(self,"%s %s Shielded for %d minutes.",V_ARE,"Shield cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 46: {		// Sleep (0x8F766): 5 L rounds, targets of level 3 or less
			int cost;

			if (!rule_reaches(spell,target,self,T,L,mf) || (target->dtu.m_88 & rule_fx) || (mf & MONSTER_F_NO_SLEEP))
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
			if (fx_set(target,rule_fx,dur = spell_time(46,L,ROLL))) {
				if (target->type == OBJ_PLAYER) {
					target->dtu.m_88 |= DTU_FX_BLIND;
					target->dtu.m_8c[3] = target->dtu.m_e8[0];
				}
				say_effect(self,"%s will be Sleeping for %d minutes.",V_NONE,"Sleep cast on %s by %s for %d minutes.",MINUTES(dur));
			}
			break;
		}
		case 47:		// Slow: ends Haste, L+3 rounds
			if (!rule_reaches(spell,target,self,T,L,mf) || (target->dtu.m_88 & rule_fx) || rule_save(spell,target,T,L))
				break;
			if (target->dtu.m_88 & DTU_FX_HASTE)
				fx_end(target,13);
			if (fx_set(target,rule_fx,dur = spell_time(47,L,ROLL)))
				say_effect(self,"%s %s Slowed for %d minutes.",V_ARE,"Slow cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 48:		// Spider Climb: L+3 rounds
			if (fx_set(target,rule_fx,dur = spell_time(48,L,ROLL)))
				say_effect(self,"%s may Spider Climb for %d minutes.",V_NONE,"Spider Climb cast on %s by %s for %d minutes.",MINUTES(dur));
			break;
		case 49:		// Stone Skin: 1d4 + L/2 hits
			if (target->dtu.m_88 & rule_fx)
				break;
			if (Dtu_fix_stoneskin_word)
				target->dtu.m_88 |= rule_fx;
			else
				target->dtu.m_88 = rule_fx;
			target->dtu.m_80 = spell_amount(49,L,ROLL);
			say_effect(self,"%s %s Stone Skin for %d attacks.",V_HAVE,"Stone Skin cast on %s by %s for %d attacks.",target->dtu.m_80);
			break;
		case 50:		// Strength: L hours (0x8FCFA). The player gains strength.
			if (target->dtu.m_88 & rule_fx)
				break;
			if (target->type == OBJ_PLAYER) {
				int gain = spell_amount(50,L,ROLL);
				dtu_char_strength_gain(gain,char_is(DTU_CLASS_FIGHTER));
			}
			{
				// L * 225 << 20 in 32 bits (0x8FF5D): from the level 10 on the
				// value wraps as in the executable
				fix d = spell_time(50,L,ROLL);

				if (fx_set(target,rule_fx,d))
					say_effect(self,"%s %s Strength for %d hours.",V_HAVE,"Strength cast on %s by %s for %d hours.",HOURS(d));
			}
			break;
		case 51:		// "Burning Hands" (cold): the code is Find Traps, 30 rounds
			if (fx_set(target,rule_fx,dur = spell_time(51,L,ROLL))) {
				if (self)
					say("%s can Find Traps for %d minutes.",Msg_t,MINUTES(dur));
				else
					say("Find Traps cast on %s by %s for %d minutes.",Msg_t,Msg_c,MINUTES(dur));
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
	// ASCENT: the blindness of the spell (spell_time), no end for Continual Light
	Objects[objnum].lifeleft = (spell == 30 || spell == 31) ? spell_time(spell,L,ROLL) : IMMORTAL_TIME;
	dtu_spell_log("spell: light object %d, light %.0f, life %.0f",objnum,f2fl(Objects[objnum].ctype.light_info.intensity),
		Objects[objnum].lifeleft == IMMORTAL_TIME ? -1.0 : f2fl(Objects[objnum].lifeleft));
}

int dtu_spell_area(int spell,vms_vector *pos,int segnum,object *caster,int level)
{
	fix radius = i2f(spell_int(spell,"radius",0) + level * spell_int(spell,"level_radius",0));
	int i, n = 0;
	// ASCENT: Spell_info +0x37 bit 3 (the word hit_objects of the table
	// fixup, dtutblfix.h): 0x90CEF also takes the objects that are not
	// creatures or the player (0x90DA6).
	int any = spell_flag(spell,"hit_objects");

	if (!spell_ok(spell) || segnum < 0)
		return 0;
	dtu_spell_log("spell: area of '%s' at %.1f %.1f %.1f, radius %.1f",spell_name(spell),f2fl(pos->x),f2fl(pos->y),f2fl(pos->z),f2fl(radius));
	if (spell == 8 || spell == 9 || spell == 30 || spell == 31)
		light_object(spell,pos,segnum,level);
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];

		if (o->type == OBJ_NONE || (o->type != OBJ_PLAYER && o->type != OBJ_ROBOT && !any))
			continue;
		// ASCENT: Dtu_fix_revive: the area reaches the fallen body of a
		// creature that can get up; fire or acid destroys it
		if (dtu_creature_down(o)) {
			if (vm_vec_dist(&o->pos,pos) <= radius && point_sees(pos,segnum,o)) {
				dtu_creature_down_spell(o,caster,spell,level);
				n++;
			}
			continue;
		}
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
		if ((rule_flags(spell) & DTU_SR_BURST_BLINDS) && (o->type == OBJ_PLAYER || o->type == OBJ_ROBOT))		// 0x90DDF: Fireball, the fire trap (dtucombat.c)
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
	// ASCENT: only a weapon with attack_type 0 is a wand: 0x44300 calls
	// 0x56A0D, the one reader of PC_spell, for a spell or for such a weapon
	// (0x44423 to 0x44445). Before this the Short Sword of Quickness
	// (melee, pc_spell=21) cast Haste at the first attack and was used up
	// like a wand with one charge.
	if (e && e->type == OBJ_WEAPON && e->id < N_weapon_types) {
		char v[16];
		if (bm_tbl_get(Weapon_item_info[e->id].record,"attack_type",v,sizeof(v)) && !stricmp(v,"PROJECTILE") &&
			 bm_tbl_get(Weapon_item_info[e->id].record,"pc_spell",v,sizeof(v)) && atoi(v) >= 0 && spell_ok(atoi(v))) {
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
// ASCENT: mode LOW or HIGH gives the range of the count for the
// description window, with no roll and no change of the pools.
static int missile_count_mode(int spell,int L,int mode)
{
	switch (spell) {
		case 46:
			if (mode != ROLL)
				return 1;
			Sleep_pool = (rand() % 7) * 2 + 4;
			dtu_spell_log("spell: Sleep pool %d",Sleep_pool);
			return 1;
		case 22: case 52:
			if (mode != ROLL)
				return 1;
			// ASCENT: 0x56FF6: the counter Spell_info+0x49 of Hold Person
			// is 1d4 at each cast; the case (0x8E8BB) needs it above 0 and
			// takes one for each hold, so the area of 0x1A344 (radius 40)
			// holds at most 1d4 targets
			Hold_count[spell == 52] = spell_amount(spell,L,ROLL);		// ASCENT: 1d4, as the description window
			dtu_spell_log("spell: Hold Person counter %d",Hold_count[spell == 52]);
			return 1;
		case 4:	return 6;
		case 6:	return die(6,mode);
		case 7:	return die(4,mode) + L;
		case 33:	return (L/2 + 1 < 5) ? L/2 + 1 : 5;
		case 34:	return die(3,mode);
		case 37:	return L < 5 ? L : 5;
	}
	return 1;
}

static int missile_count(int spell,int L)
{
	return missile_count_mode(spell,L,ROLL);
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
		// Prayer blesses the caster only. The record keeps the word
		// (Dtu_fix_prayer_flags).
		if (spell == 38)
			Prayer_written = 1;
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

// ASCENT: the Shield spell (45) and the Amulet of Shield. 1.3 caps the
// armor class at 3 against every attack (0xB067C for the player, 0x2ADAF
// for a creature). The decision of the user (docs/audit/spells-wizard.md):
// 4 against melee weapons and 2 against missiles, as the spell text of
// AD&D. 1 (default): 4 and 2. 0 (-origcombat): 3.
int Dtu_fix_shield_ac = 1;

int dtu_spell_shield_ac(int missile)
{
	if (!Dtu_fix_shield_ac)
		return 3;
	return missile ? 2 : 4;
}

// ASCENT: the caps of the effects before the dexterity adjustment
// (0xB05D4 to 0xB0696): dtu_spell_player_ac and dtu_spell_get_facts.
// missile: the armor class against a missile (Shield).
static int effects_ac(int ac,int w,int vestment_level,int missile)
{
	if ((w & DTU_FX_ARMOR) && ac > 6) ac = 6;
	if (w & DTU_FX_VESTMENT) {
		int v = 5 - (vestment_level - 5) / 3;
		if (v < 1) v = 1;
		if (v > 5) v = 5;
		if (ac > v) ac = v;
	}
	if ((w & DTU_FX_SHIELD) && ac > dtu_spell_shield_ac(missile)) ac = dtu_spell_shield_ac(missile);
	if (w & DTU_FX_SLOW) ac += 4;
	return ac;
}

// ASCENT: the armor class that the effects of a mask give to a player
// with armor class 10 (the description window, dtudesc.c), against melee
int dtu_spell_fx_ac_vs(int fx,int missile)
{
	return effects_ac(10,fx & (DTU_FX_ARMOR|DTU_FX_VESTMENT|DTU_FX_SHIELD),dtu_spell_player_level(35),missile);
}

int dtu_spell_fx_ac(int fx)
{
	return dtu_spell_fx_ac_vs(fx,0);
}

int dtu_spell_player_ac_vs(int ac,int stage,int missile)
{
	object *p = ConsoleObject;
	int w;

	if (p == NULL || p->type != OBJ_PLAYER)
		return ac;
	w = p->dtu.m_88;
	if (stage == 0)
		ac = effects_ac(ac,w,dtu_spell_player_level(35),missile);
	else if (w & DTU_FX_INVULNERABLE)
		ac -= 2;
	return ac;
}

int dtu_spell_player_ac(int ac,int stage)
{
	return dtu_spell_player_ac_vs(ac,stage,0);
}

// 0x2ADAF: one of Magical Vestment (at most max(5 - (L - 5) / 3, 1) with
// the level of the creature), Shield (at most 3) or Armor (at most 6),
// in this order; then Slow adds 4, at most 10. ASCENT: Shield gives 4
// against melee and 2 against missiles with Dtu_fix_shield_ac.
int dtu_spell_creature_ac_vs(object *c,int ac,int missile)
{
	int w = c->dtu.m_88;

	if (w & DTU_FX_VESTMENT) {
		int v = 5 - (creature_level(c) - 5) / 3;

		if (v < 1) v = 1;
		if (ac > v) ac = v;
	}
	else if (w & DTU_FX_SHIELD) {
		if (ac > dtu_spell_shield_ac(missile)) ac = dtu_spell_shield_ac(missile);
	}
	else if ((w & DTU_FX_ARMOR) && ac > 6)
		ac = 6;
	if (w & DTU_FX_SLOW) {
		ac += 4;
		if (ac > 10) ac = 10;
	}
	return ac;
}

int dtu_spell_creature_ac(object *c,int ac)
{
	return dtu_spell_creature_ac_vs(c,ac,0);
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

// ASCENT: 0x2A670: the touch spell sets the counter Spell_info+0x49 of Hold
// Person (22, 52) to 1 before the cast 0x8CFFF.
void dtu_spell_touch_counter(int spell)
{
	if (spell == 22 || spell == 52)
		Hold_count[spell == 52] = 1;
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
	if (monster_flag(obj,MONSTER_F_FIRE_RESISTANT)) r |= 0x10;
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

// ASCENT: the cast timer for the state file (dtuspell.h)
fix dtu_spell_cast_timer(void)
{
	return Cast_timer;
}

void dtu_spell_cast_saved(int restart)
{
	if (restart && Cast_timer != DTU_FX_NO_END) {
		Cast_timer = DTU_FX_NO_END;
		dtu_spell_log("spell: the save ends the running cast (0x164B3)");
	}
}

void dtu_spell_cast_load(fix timer)
{
	Cast_loaded = (timer > 0 && timer != DTU_FX_NO_END) ? timer : DTU_FX_NO_END;
}

// ASCENT: D-134. The spell points of a creature for the state file (object
// +0x19E mage, +0x19F cleric, as 1.3 keeps them in the object).
void dtu_spell_creature_points(object *obj,int *mage,int *cleric)
{
	int n;

	*mage = *cleric = 0;
	if (obj == NULL || obj->type != OBJ_ROBOT)
		return;
	n = cspell_state(obj);
	*mage = Cspell[n].points[0] > 0 ? Cspell[n].points[0] : 0;
	*cleric = Cspell[n].points[1] > 0 ? Cspell[n].points[1] : 0;
}

// ASCENT: D-134 (Dtu_fix_restore_spells). A restored creature keeps the
// saved points. With none left it waits the full time for new points, as
// after its last cast (0x92D99); the state file keeps no time left.
void dtu_spell_creature_keep_points(object *obj,int mage,int cleric)
{
	int n;

	if (obj == NULL || obj->type != OBJ_ROBOT)
		return;
	n = cspell_state(obj);
	Cspell[n].points[0] = mage;
	Cspell[n].points[1] = cleric;
	if (mage < 1 && cleric < 1 && (creature_points(obj,0) > 0 || creature_points(obj,1) > 0))
		Cspell[n].recharge_time = GameTime + (9 - creature_mage_level(obj) - creature_cleric_level(obj)) * MINUTE;
	dtu_spell_log("cspell: obj %d %s keeps the saved points %d %d",n,target_name(obj),mage,cleric);
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
			if ((cw & (DTU_FX_PROT_FIRE|DTU_FX_RESIST_FIRE)) || (mf & MONSTER_F_FIRE_IMMUNE) || monster_flag(obj,MONSTER_F_FIRE_RESISTANT) ||
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
		case 33:	return (creature_clear(obj,i2f(5)) && !(pw & Spell_rule[33].stopped_by)) ? 50 : -1;
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

	// The level start. ASCENT: the slots left stay (they come from the
	// save after a load). Only the game start after Waterdeep fills them:
	// game() 0x4221E to 0x42284 calls 0x3551B when the mission is the
	// built-in one (0x297F2C = 0), the flag 0x141C38 is set and the level
	// byte P+0x71 is 1 or below, then clears the flag (0x4228B). The game
	// loop changes levels without game(). The cast that runs ends:
	// 0x16413 sets 0x153EBC (Ai_local_info[0] +0x0C) to 0x3FFFFFFF after
	// the level state write of each warp, and game() does it at 0x41FB7.
	// The missiles and the acid of the port refer to objects of the level.
	if (Level_started || last_level != Current_level_num) {
		Level_started = 0;
		last_level = Current_level_num;
		dtu_spell_compute_slots();
		if (Fill_at_start == 2 || (Fill_at_start && Current_level_num <= 1))
			dtu_spell_fill_slots();
		Fill_at_start = 0;
		Cast_timer = DTU_FX_NO_END;
		// ASCENT: the rule table against its sentences, once (dtu_spell_rule_check)
		{
			static int rule_checked;

			if (!rule_checked) {
				rule_checked = 1;
				Assert(dtu_spell_rule_check() == 0);
			}
		}
		// ASCENT: a load of a save game: the cast that ran at the save goes
		// on (0x164C7 reads 0x153EBC with Ai_local_info), with the spell in
		// the hand of the save
		if (Cast_loaded != DTU_FX_NO_END) {
			int source, spell = hand_spell(&source);

			if (spell >= 0 && source == 0) {
				Cast_timer = Cast_loaded;
				Cast_spell = spell;
				dtu_spell_log("spell: the cast of '%s' from the save goes on, the cast at %.3f",spell_name(spell),f2fl(Cast_timer));
			}
			Cast_loaded = DTU_FX_NO_END;
		}
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

//------------------------------------------------------------------------
// ASCENT: the test aid -chartest FRAME:spelltest=SPELL.LEVEL.OBJ.TRIALS.
// The player casts SPELL at caster LEVEL with no cast time, no slot and no
// missile flight: each missile of the cast (0x56FF6) goes into the effect
// of 0x8CFFF (or the area of 0x90CEF for an AREA line) on object OBJ (-1:
// the player; a spell cast on the caster always acts on the player). The
// -objlog file gets the hit points, the effects and their times before and
// after. With TRIALS of 1 or more each trial puts the target (and the
// character) back as it was and ends the rounds of acid on it; with 0
// one cast stays, so the later rounds run.

void dtu_spell_test(int spell,int level,int objnum,int trials)
{
	object *caster = ConsoleObject, *t, save_obj;
	static dtu_character save_char;
	int trial, n_trials = trials < 1 ? 1 : trials, total = 0;

	if (!caster || !spell_ok(spell)) {
		dtu_spell_log("spelltest: spell %d is not in the tables",spell);
		return;
	}
	if (level < 1)
		level = 1;
	if (spell_self(spell) || objnum < 0)
		t = caster;
	else if (objnum <= Highest_object_index && (Objects[objnum].type == OBJ_ROBOT || Objects[objnum].type == OBJ_PLAYER))
		t = &Objects[objnum];
	else {
		dtu_spell_log("spelltest: obj %d is not a creature",objnum);
		return;
	}
	for (trial=1;trial<=n_trials;trial++) {
		int hp0 = t->type == OBJ_PLAYER ? dtu_char_hit_points() : f2i(t->shields), hp1, fx0 = t->dtu.m_88, fx1, n, i, b;

		save_obj = *t;
		save_char = Dtu_char;
		n = missile_count(spell,level);
		dtu_spell_log("spelltest: '%s' (%d) level %d on obj %d %s (level %d), trial %d: %d missiles%s, hit points %d, effects 0x%08x",
			spell_name(spell),spell,level,(int)(t-Objects),target_name(t),object_level(t,spell),trial,n,
			spell_area(spell) ? " (area)" : "",hp0,fx0);
		for (i=0;i<n;i++) {
			if (spell_area(spell))
				dtu_spell_area(spell,&t->pos,t->segnum,caster,level);
			else
				apply_spell(spell,t,caster,level);
		}
		hp1 = t->type == OBJ_PLAYER ? dtu_char_hit_points() : f2i(t->shields);
		fx1 = t->dtu.m_88;
		total += hp0 - hp1;
		dtu_spell_log("spelltest: result: hit points %d to %d (damage %d), effects 0x%08x to 0x%08x",hp0,hp1,hp0 - hp1,fx0,fx1);
		for (b=0;b<32;b++)
			if ((fx1 & (1 << b)) && !(fx0 & (1 << b))) {
				fix *tm = Fx_timer[b] ? timer_ref(t,Fx_timer[b]) : NULL;

				if (tm && *tm == DTU_FX_NO_END)
					dtu_spell_log("spelltest:   new effect %s, no end",Fx_name[b]);
				else if (tm)
					dtu_spell_log("spelltest:   new effect %s, %.1f seconds",Fx_name[b],f2fl(*tm - GameTime));
				else
					dtu_spell_log("spelltest:   new effect %s, no timer",Fx_name[b]);
			}
		for (i=0;i<MAX_ACID;i++)
			if (Acid[i].used && Acid[i].obj == t-Objects)
				dtu_spell_log("spelltest:   '%s' burns for rounds 2 to %d",spell_name(Acid[i].spell),Acid[i].rounds);
		if (trials < 1)
			break;
		for (i=0;i<MAX_ACID;i++)
			if (Acid[i].used && Acid[i].obj == t-Objects)
				Acid[i].used = 0;
		*t = save_obj;
		Dtu_char = save_char;
		if (t->type == OBJ_PLAYER)
			dtu_char_update();
	}
	if (trials > 1)
		dtu_spell_log("spelltest: '%s' level %d: mean damage %.2f over %d trials",spell_name(spell),level,(double)total / n_trials,n_trials);
}

// ASCENT: the checks of the priest spells and of turn undead (the
// -chartest command priestcheck, docs/audit/spells-priest.md). Each check
// sets the state it needs, calls the case of 0x8CFFF and compares the
// result with the rule of the executable (or of the Dtu_fix_* setting).
// The player and the first live creature of the level are put back after
// each check.

static int Priest_failed;

static void priest_result(const char *name,int ok,const char *format,...)
{
	char buf[300];
	va_list args;

	va_start(args,format);
	vsnprintf(buf,sizeof(buf),format,args);
	va_end(args);
	dtu_char_log("priestcheck: %s %s: %s",ok ? "PASS" : "FAIL",name,buf);
	dtu_spell_log("priestcheck: %s %s: %s",ok ? "PASS" : "FAIL",name,buf);
	if (!ok)
		Priest_failed++;
}

static object *priest_creature(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];

		if (o->type == OBJ_ROBOT && o->id < N_robot_types && o->shields >= F1_0 &&
			 !(o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) && DTU_ANIM_STATE(o) != DAS_DIE)
			return o;
	}
	return NULL;
}

int dtu_spell_priest_check(void)
{
	object *pl = ConsoleObject, *c = priest_creature();
	dtu_player_attr pa_keep;
	fix pl_shields, c_shields = 0, before;
	int c_flags = 0, fix_keep = Dtu_fix_cure_aid, hp, max, i, n;
	static int levels_keep[2][9];

	Priest_failed = 0;
	if (!pl || pl->type != OBJ_PLAYER || !Dtu_char.created) {
		dtu_char_log("priestcheck: FAIL setup: no player or no character");
		return 1;
	}
	pa_keep = Dtu_char.pa;
	pl_shields = Players[Player_num].shields;
	memcpy(levels_keep,Dtu_spell_slots_left,sizeof(levels_keep));
	if (c) {
		c_shields = c->shields;
		c_flags = Monster_info[c->id].flags;
	}

#define PRIEST_RESET() do { \
		pl->dtu.m_88 = 0; pl->dtu.m_82 = 0; Priest_force_save = -1; Dtu_fix_cure_aid = fix_keep; \
		Dtu_char.pa = pa_keep; Players[Player_num].shields = pl->shields = pl_shields; \
		if (c) { c->dtu.m_88 = 0; c->shields = c_shields; Monster_info[c->id].flags = c_flags; } \
	} while (0)

	PRIEST_RESET();

	// Protection From Fire (0x8F1B2): 12 L in the pool, no second pool
	apply_spell(40,pl,pl,3);
	n = pl->dtu.m_82;
	apply_spell(40,pl,pl,3);
	priest_result("pff_no_stack",n == 36 && pl->dtu.m_82 == 36,"pool %d after the first cast, %d after the second (36, 36)",n,pl->dtu.m_82);
	PRIEST_RESET();

	// Bless, Aid and Prayer share bit 1 (0x8D58E, 0x8D3F7, 0x8F078)
	apply_spell(3,pl,pl,1);
	before = pl->dtu.m_8c[2];
	apply_spell(38,pl,pl,10);
	n = (pl->dtu.m_8c[2] == before);
	apply_spell(1,pl,pl,4);
	priest_result("bless_stack",n && pl->dtu.m_8c[2] == GameTime + 5*MINUTE,
		"Prayer keeps the Bless timer: %d; Aid of level 4 sets %.0f s (%.0f)",n,f2fl(pl->dtu.m_8c[2] - GameTime),5.0*60);
	PRIEST_RESET();

	// Protection From Evil (0x8F116): 3 L minutes, no second cast
	apply_spell(53,pl,pl,2);
	before = pl->dtu.m_e0[0];
	apply_spell(53,pl,pl,9);
	priest_result("pfe_no_stack",before == GameTime + 6*MINUTE && pl->dtu.m_e0[0] == before,
		"timer %.0f s (360)",f2fl(pl->dtu.m_e0[0] - GameTime));
	PRIEST_RESET();

	// Cure Light Wounds after Aid (0x8DEE2): the hit points above the
	// maximum stay with Dtu_fix_cure_aid, the executable cuts them
	max = dtu_char_max_hit_points();
	for (i=0;i<2;i++) {
		Dtu_fix_cure_aid = (i == 0);
		Players[Player_num].shields = pl->shields = i2f(max);
		apply_spell(1,pl,pl,1);
		hp = dtu_char_hit_points();
		apply_spell(10,pl,pl,1);
		if (i == 0)
			priest_result("cure_keeps_aid",hp > max && dtu_char_hit_points() == hp,"%d after Aid, %d after the cure (maximum %d)",hp,dtu_char_hit_points(),max);
		else
			priest_result("cure_cuts_aid_orig",hp > max && dtu_char_hit_points() == max,"%d after Aid, %d after the cure (maximum %d)",hp,dtu_char_hit_points(),max);
		PRIEST_RESET();
	}

	// Cure Blindness or Deafness (0x8F2FA): the blindness ends in the next
	// frame through its timer, the deafness at once
	pl->dtu.m_88 |= DTU_FX_BLIND|DTU_FX_DEAF;
	pl->dtu.m_8c[3] = DTU_FX_NO_END;
	apply_spell(42,pl,pl,1);
	n = (pl->dtu.m_88 & DTU_FX_BLIND) && pl->dtu.m_8c[3] == 0 && !(pl->dtu.m_88 & DTU_FX_DEAF);
	fx_expire(pl,0);
	priest_result("cure_blind_timer",n && !(pl->dtu.m_88 & DTU_FX_BLIND),"blind with timer 0 after the cast: %d; blind after 0x9002C: %d",n,(pl->dtu.m_88 & DTU_FX_BLIND) != 0);
	PRIEST_RESET();

	// Free Action (0x8E6F2): ends Hold, but not while Free Action is on
	pl->dtu.m_88 |= DTU_FX_FREE_ACTION|DTU_FX_HOLD;
	pl->dtu.m_c0[0] = GameTime + 100*F1_0;
	apply_spell(20,pl,pl,2);
	n = (pl->dtu.m_88 & DTU_FX_HOLD) != 0;
	pl->dtu.m_88 &= ~DTU_FX_FREE_ACTION;
	apply_spell(20,pl,pl,2);
	priest_result("free_action",n && !(pl->dtu.m_88 & DTU_FX_HOLD) && (pl->dtu.m_88 & DTU_FX_FREE_ACTION) && pl->dtu.m_8c[11] == GameTime + 20*MINUTE,
		"Hold kept while Free Action is on: %d; ended by the second cast: %d",n,!(pl->dtu.m_88 & DTU_FX_HOLD));
	PRIEST_RESET();

	// Find Traps (spell 51, a cleric spell of level 2 after 0x1A344): bit
	// 20 for 1800 s (0x8E4F6)
	apply_spell(51,pl,pl,3);
	priest_result("find_traps",(pl->dtu.m_88 & DTU_FX_FIND_TRAPS) && pl->dtu.m_c8[3] == GameTime + i2f(1800),
		"bit 20 %d, timer %.0f s (1800)",(pl->dtu.m_88 & DTU_FX_FIND_TRAPS) != 0,f2fl(pl->dtu.m_c8[3] - GameTime));
	PRIEST_RESET();

	// The caster level (0x91EC1): a fighter/cleric casts a cleric spell
	// at the cleric level, a mage spell at the fighter level
	Dtu_char.pa.class1 = DTU_CLASS_FIGHTER;
	Dtu_char.pa.class1_level = 4;
	Dtu_char.pa.class2 = DTU_CLASS_CLERIC;
	Dtu_char.pa.class2_level = 1;
	priest_result("caster_level",dtu_spell_player_level(3) == 2 && dtu_spell_player_level(33) == 5,
		"Bless %d (2), Magic Missile %d (5)",dtu_spell_player_level(3),dtu_spell_player_level(33));

	// The slots and the wisdom bonus (0x3575B, 0x145DF8, 0x1403D2)
	{
		static const struct { int level, wis, s[4]; } t[] = {
			{ 0, 12, {1,0,0,0} }, { 0, 19, {4,0,0,0} }, { 4, 17, {5,5,2,0} }, { 6, 18, {5,5,3,2} }, { 2, 18, {4,3,0,0} },
		};

		for (i=0;i<(int)(sizeof(t)/sizeof(t[0]));i++) {
			int *m = Dtu_spell_slots_max[1];

			Dtu_char.pa.class1 = DTU_CLASS_CLERIC;
			Dtu_char.pa.class2 = -1;
			Dtu_char.pa.class1_level = t[i].level;
			Dtu_char.pa.wisdom = t[i].wis;
			dtu_spell_compute_slots();
			priest_result("slots",m[0] == t[i].s[0] && m[1] == t[i].s[1] && m[2] == t[i].s[2] && m[3] == t[i].s[3],
				"cleric level %d, wisdom %d: %d %d %d %d (%d %d %d %d)",t[i].level + 1,t[i].wis,m[0],m[1],m[2],m[3],
				t[i].s[0],t[i].s[1],t[i].s[2],t[i].s[3]);
		}
	}
	PRIEST_RESET();
	dtu_spell_compute_slots();
	memcpy(Dtu_spell_slots_left,levels_keep,sizeof(levels_keep));

	if (c) {
		fix d;

		// Light (Cleric) on a light sensitive creature (0x8DC1D): a save ends
		// the case, so no damage; a failed save blinds and hurts
		c->shields = i2f(1000);
		Monster_info[c->id].flags = (c_flags | MONSTER_F_LIGHT_SENSITIVE) & ~MONSTER_F_BLIND;
		Priest_force_save = 1;
		d = apply_spell(31,c,pl,1);
		priest_result("light_save",d == 0 && !(c->dtu.m_88 & DTU_FX_BLIND),"saved: damage %.1f, blind %d",f2fl(d),(c->dtu.m_88 & DTU_FX_BLIND) != 0);
		Priest_force_save = 0;
		d = apply_spell(31,c,pl,1);
		priest_result("light_fail",d > 0 && (c->dtu.m_88 & DTU_FX_BLIND) && c->dtu.m_8c[3] == GameTime + 70*MINUTE,
			"failed: damage %.1f, blind for %.0f s (4200)",f2fl(d),f2fl(c->dtu.m_8c[3] - GameTime));
		PRIEST_RESET();

		// The end of Bless cuts a creature to its maximum (0x9013F)
		max = Monster_info[c->id].max_hit_points;
		apply_spell(1,c,pl,1);
		c->shields = i2f(max + 5);
		fx_end(c,1);
		priest_result("bless_end_creature",c->shields == i2f(max),"%.1f after the end (maximum %d)",f2fl(c->shields),max);
		PRIEST_RESET();

		// Cures do nothing on undead and extraplanar creatures (0x8DE4B)
		Monster_info[c->id].flags = c_flags | MONSTER_F_UNDEAD;
		c->shields = F1_0;
		apply_spell(11,c,pl,1);
		priest_result("cure_undead",c->shields == F1_0,"%.1f after Cure Serious Wounds (1.0)",f2fl(c->shields));
		Monster_info[c->id].flags = c_flags & ~(MONSTER_F_UNDEAD|MONSTER_F_EXTRAPLANAR);
		apply_spell(11,c,pl,1);
		priest_result("cure_living",c->shields > F1_0 && c->shields <= i2f(18) && c->shields <= i2f(Monster_info[c->id].max_hit_points),
			"%.1f after Cure Serious Wounds (4 to 18, at most the maximum %d)",f2fl(c->shields),Monster_info[c->id].max_hit_points);
		PRIEST_RESET();

		// Hold Person (0x8E85A): not on undead, only humanoids, and only
		// while the counter of the cast (1d4, 0x56FF6) is above 0
		Priest_force_save = 0;
		Hold_count[1] = 1;
		Monster_info[c->id].flags = (c_flags | MONSTER_F_HUMANOID | MONSTER_F_UNDEAD);
		apply_spell(52,c,pl,3);
		n = (c->dtu.m_88 & DTU_FX_HOLD) != 0;
		Monster_info[c->id].flags = (c_flags | MONSTER_F_HUMANOID) & ~MONSTER_F_UNDEAD;
		apply_spell(52,c,pl,3);
		priest_result("hold_person",!n && (c->dtu.m_88 & DTU_FX_HOLD) && c->dtu.m_c0[0] == GameTime + 6*MINUTE && Hold_count[1] == 0,
			"undead held: %d; humanoid held for %.0f s (360); counter %d (0)",n,f2fl(c->dtu.m_c0[0] - GameTime),Hold_count[1]);
		c->dtu.m_88 = 0;
		apply_spell(52,c,pl,3);
		priest_result("hold_person_counter",!(c->dtu.m_88 & DTU_FX_HOLD),"held with the counter at 0: %d (0)",(c->dtu.m_88 & DTU_FX_HOLD) != 0);
		for (i=0,n=0;i<20;i++)
			n += (missile_count(52,3) == 1 && Hold_count[1] >= 1 && Hold_count[1] <= 4);
		priest_result("hold_person_cast",n == 20,"20 casts set the counter to 1 to 4: %d",n);
		Hold_count[1] = 0;
		PRIEST_RESET();
	}
	else
		dtu_char_log("priestcheck: no creature in the level: the creature checks were not run");

	// Turn undead: the class of each creature (0xAC4C1, 0x145602)
	n = 0;
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		int b = dtu_combat_priest_turn_bits(o), lv = Dtu_char.pa.class1_level;

		if (b < 0)
			continue;
		if (Dtu_char.pa.class2_level > lv)
			lv = Dtu_char.pa.class2_level;
		// -origcombat (Dtu_fix_npc_list 0): the NPC list of 1.3 gives a
		// creature without the undead flag the class 15 (0xAC4A0 to 0xAC4AE)
		if (!Dtu_fix_npc_list && b == 0x0F && !(Monster_info[o->id].flags & MONSTER_F_UNDEAD))
			continue;
		if ((b & 0x10) ? (b & 0x0F) >= 9 : (b & 0x0F) != ((lv >= 7 ? 8 : lv + 1) & 0x0F)) {
			priest_result("turn_class",0,"obj %d %s: bits 0x%02x",i,target_name(o),b);
			n++;
		}
	}
	if (n == 0)
		priest_result("turn_class",1,"all creatures of the level");

#undef PRIEST_RESET
	dtu_char_log("priestcheck: %d failed",Priest_failed);
	return Priest_failed;
}

//------------------------------------------------------------------------
// ASCENT: the check of the rule table (dtuspell.h). The kind of a line
// chooses the sentence of the window; this tests that the other values of
// the line fit that sentence, so that a line whose case code changes
// fails here or in the game runs of tools/check_desc.py.

static int rule_problem(int spell,const char *what)
{
	dtu_spell_log("spell: rule check: spell %d '%s': %s",spell,spell_ok(spell) ? spell_name(spell) : "?",what);
	return 1;
}

int dtu_spell_rule_check(void)
{
	int s, bad = 0;

	for (s=0;s<N_SPELL_RULES;s++) {
		int k = Spell_rule[s].kind, fl = Spell_rule[s].flags, fx = Spell_rule[s].fx, g;
		int needs_fx = k == DTU_SK_GIVE || k == DTU_SK_CAUSE || k == DTU_SK_AC || k == DTU_SK_ARMOR || k == DTU_SK_AID ||
			k == DTU_SK_BLIND || k == DTU_SK_HOLD || k == DTU_SK_HOLD_UNDEAD || k == DTU_SK_FEAR || k == DTU_SK_FIRE_POOL ||
			k == DTU_SK_RARY || k == DTU_SK_STONESKIN || k == DTU_SK_STRENGTH;
		int timed = k == DTU_SK_GIVE || k == DTU_SK_CAUSE || k == DTU_SK_AC || k == DTU_SK_BLIND || k == DTU_SK_HOLD ||
			k == DTU_SK_HOLD_UNDEAD || k == DTU_SK_FEAR || k == DTU_SK_AID || k == DTU_SK_COLOR;
		int amount = k == DTU_SK_HEAL || k == DTU_SK_AID || k == DTU_SK_ARMOR || k == DTU_SK_FIRE_POOL ||
			k == DTU_SK_STONESKIN || k == DTU_SK_STRENGTH || k == DTU_SK_HOLD;
		dtu_spell_facts f;

		if (!spell_ok(s))
			continue;
		dtu_spell_get_facts(s,5,&f);
		if (needs_fx != (fx != 0))
			bad += rule_problem(s,needs_fx ? "the kind names an effect, the line has none" : "the line has an effect that the kind does not name");
		if (timed && spell_time(s,5,HIGH) == 0)
			bad += rule_problem(s,"the kind gives a duration, spell_time has none");
		if (amount && spell_amount(s,5,HIGH) <= 0)
			bad += rule_problem(s,"the kind gives an amount, spell_amount has none");
		if (k == DTU_SK_DAMAGE && f.kind == DTU_SK_DAMAGE) {		// Dtu_fix_melf_acid: the acid sentence
			for (g=0;g<4;g++)
				if ((f.dice_n[g] > 0 && f.dice_sides[g] > 0) || f.dice_add[g])
					break;
			if (g == 4)
				bad += rule_problem(s,"a damage line with no dice");
		}
		if (k == DTU_SK_GIVE && !spell_self(s))
			bad += rule_problem(s,"\"Gives\" on a spell that is not cast on the caster");
		if ((k == DTU_SK_CAUSE || k == DTU_SK_HOLD || k == DTU_SK_HOLD_UNDEAD) && spell_self(s))
			bad += rule_problem(s,"\"Causes\" or \"Holds\" on a spell cast on the caster");
		if (k == DTU_SK_HOLD && (!(fl & DTU_SR_HUMANOID) || fx != DTU_FX_HOLD))
			bad += rule_problem(s,"\"Holds up to N humanoids\" needs DTU_SR_HUMANOID and Hold");
		if (k == DTU_SK_HOLD_UNDEAD && ((fl & (DTU_SR_UNDEAD_ONLY|DTU_SR_LEVEL_CAP)) != (DTU_SR_UNDEAD_ONLY|DTU_SR_LEVEL_CAP) || fx != DTU_FX_HOLD))
			bad += rule_problem(s,"\"Holds undead of level N or less\" needs DTU_SR_UNDEAD_ONLY, DTU_SR_LEVEL_CAP and Hold");
		if ((fl & DTU_SR_LEVEL_CAP) && ((k != DTU_SK_CAUSE && k != DTU_SK_HOLD_UNDEAD) || spell_amount(s,5,HIGH) < 1))
			bad += rule_problem(s,"a level limit needs a sentence that prints it and an amount");
		if ((fl & DTU_SR_UNDEAD_ONLY) && (fl & DTU_SR_NO_UNDEAD))
			bad += rule_problem(s,"undead only and no undead");
		if ((save_level(s,5) >= 0 || (fl & DTU_SR_SAVE_KIN)) && Spell_rule[s].save == DTU_SS_NONE)
			bad += rule_problem(s,"a save condition on a line with no save");
		if (Spell_rule[s].save == DTU_SS_SOME)
			bad += rule_problem(s,"DTU_SS_SOME in the table: the facts derive it from the save conditions");
		// the window has the sentence "Stops Magic Missile." only (shield_lines)
		if ((fl & DTU_SR_STOPPED) && (s != 33 || Spell_rule[s].stopped_by != DTU_FX_SHIELD))
			bad += rule_problem(s,"a stopped spell that the window has no sentence for");
		if ((fl & DTU_SR_BURST_BLINDS) && (k != DTU_SK_DAMAGE || !spell_area(s)))
			bad += rule_problem(s,"the blindness near the point needs an area damage spell");
		if ((fl & DTU_SR_SPARES_CASTER) && spell_self(s) && !spell_area(s))
			bad += rule_problem(s,"a spell cast on the caster alone cannot spare the caster");
	}
	dtu_spell_log("spell: rule check: %d problems",bad);
	return bad;
}

//------------------------------------------------------------------------
// ASCENT: the facts of a spell for the description window (dtuspell.h)


const char *dtu_spell_fx_name(int mask)
{
	return mask ? Fx_name[fx_bit(mask)] : "";
}

static int facts_seconds(fix t)
{
	return t >= DTU_FX_NO_END ? -1 : f2i(t);
}

int dtu_spell_get_facts(int spell,int L,dtu_spell_facts *f)
{
	static char *keys[4][3] = {
		{ "damage_dice", "#damage_dice", "damage_level" }, { "fire_damage_dice", "#fire_damage_dice", "fire_damage_level" },
		{ "acid_damage_dice", "#acid_damage_dice", "acid_damage_level" }, { "elec_damage_dice", "#elec_damage_dice", "elec_damage_level" } };
	int g, group, count, level;

	memset(f,0,sizeof(*f));
	if (!spell_ok(spell))
		return 0;
	if (L < 1)
		L = dtu_spell_player_level(spell);
	if (L < 1)
		L = 1;
	f->book = spell_book_of(spell);
	f->level = spell_level(spell);
	f->caster_level = L;
	f->cast_time = spell_cast_time(spell);
	f->self = spell_self(spell);
	f->area = spell_area(spell);
	if (f->area)
		f->radius = spell_int(spell,"radius",0) + L * spell_int(spell,"level_radius",0);
	if (!f->self) {
		f->range = f2i(fixmul(spell_fix(spell,"speed",i2f(100)),spell_missile_life(spell,L)));
		f->missiles_lo = missile_count_mode(spell,L,LOW);
		f->missiles_hi = missile_count_mode(spell,L,HIGH);
	}
	if (spell < N_SPELL_RULES) {
		f->kind = Spell_rule[spell].kind;
		f->save = Spell_rule[spell].save;
		f->fx = Spell_rule[spell].fx;
		f->flags = Spell_rule[spell].flags;
		f->stopped_by = (f->flags & DTU_SR_STOPPED) ? Spell_rule[spell].stopped_by : 0;
	}
	// the conditions of rule_reaches and rule_save
	f->max_level = (f->flags & DTU_SR_LEVEL_CAP) ? spell_amount(spell,L,HIGH) : -1;
	f->save_level = f->save != DTU_SS_NONE ? save_level(spell,L) : -1;
	if (f->save != DTU_SS_NONE && (f->save_level >= 0 || (f->flags & DTU_SR_SAVE_KIN)))
		f->save = DTU_SS_SOME;
	f->time_lo = facts_seconds(spell_time(spell,L,LOW));
	f->time_hi = facts_seconds(spell_time(spell,L,HIGH));
	f->amount_lo = spell_amount(spell,L,LOW);
	f->amount_hi = spell_amount(spell,L,HIGH);
	f->ac = f->ac_missile = 99;
	if (f->kind == DTU_SK_AC || f->kind == DTU_SK_ARMOR) {
		f->ac = effects_ac(10,f->fx,L,0);
		f->ac_missile = effects_ac(10,f->fx,L,1);
	}
	if (spell == 36 && Dtu_fix_melf_acid) {
		// melf_acid_hit: an attack roll, ACID_DICE d ACID_SIDES now and in
		// each of the next L/3 rounds, no save
		f->kind = DTU_SK_ACID;
		f->save = DTU_SS_NONE;
		f->dice_n[2] = ACID_DICE;
		f->dice_sides[2] = ACID_SIDES;
		return 1;
	}
	if (f->kind == DTU_SK_DAMAGE) {
		spell_damage_args(spell,L,&group,&count,&level);
		for (g=0;g<4;g++) {
			f->dice_sides[g] = spell_int(spell,keys[g][0],0);
			f->dice_n[g] = (g == group) ? count : spell_int(spell,keys[g][1],0);
			f->dice_add[g] = spell_int(spell,keys[g][2],0) * level;
		}
		f->undead_die = spell_undead_die(spell);
		// the area of Fireball (dtu_spell_area, dtu_fireball_blind_deaf)
		if ((f->flags & DTU_SR_BURST_BLINDS) && f->area) {
			f->blind_dist = DTU_FIREBALL_BLIND_DIST;
			f->blind_seconds = DTU_FIREBALL_BLIND_SECONDS;
		}
	}
	return 1;
}
