/*
 * ASCENT: new file. The player character of Descent to Undermountain. It
 * replaces the ship values of the Descent 1 player. See dtuchar.h.
 *
 * The rules are from DTUGAME.EXE version 1.3. Each comment with an address
 * (0x...) names the function or the data of the executable that the code
 * follows. docs/re/character.md has the details. A comment that says
 * "guess" marks a part that is not verified.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stddef.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "segment.h"
#include "gameseg.h"
#include "fvi.h"
#include "bm.h"
#include "robot.h"
#include "text.h"
#include "gauges.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtudisp.h"
#include "dtutrig.h"
#include "dtuchar.h"
#include "dtuspell.h"	// ASCENT: the spell effects of the armor class and the slots of a rest
#include "dtuthief.h"
#include "dtufile.h"		// ASCENT: dtu_log()
#include "dtucombat.h"	// ASCENT: turn undead of the Spider Piece 0x12
#include "dtuatk.h"		// ASCENT: the fast items (Dtu_fix_speed_items)
#include "newmenu.h"
#include "digi.h"
#include "automap.h"
#include "fuelcen.h"	// ASCENT: the fire of a rest place
#include "dtuscript.h"	// ASCENT: the script 0x1FC of the Spider Piece 0x18
#include "wall.h"			// ASCENT: the test command wall of -chartest
#include "gameseq.h"		// ASCENT: Current_level_num for the test command quest
#include "dtuscroll.h"	// ASCENT: the text screen of the Scroll
#include "dtudesc.h"		// ASCENT: the test commands of the description window

#define DTU_WEAK __attribute__((weak))

// A script event of the item code (0xADB3B, 0xAC055). dtuscript.c can
// give the strong definition.
DTU_WEAK void dtu_hook_script_event(int event)
{
	dtu_char_log("script event 0x%x (no script code)", event);
}

_Static_assert(sizeof(dtu_player_attr) == 80, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, max_hit_points) == 0x0A, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, armor_class) == 0x16, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, strength) == 0x20, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, hide_in_shadows) == 0x30, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, gold) == 0x44, "dtu_player_attr");
_Static_assert(offsetof(dtu_player_attr, alignment) == 0x4C, "dtu_player_attr");
_Static_assert(sizeof(dtu_item_record) == 16, "dtu_item_record");
_Static_assert(offsetof(dtu_item_record, modifier) == 12, "dtu_item_record");
_Static_assert(sizeof(dtu_item_ref) == 2, "dtu_item_ref");
_Static_assert(DTU_P_ATTR + sizeof(dtu_player_attr) == 0xE7, "player record");

dtu_character Dtu_char;
int Dtu_fix_multiclass_thac0 = 1;
// ASCENT: the spell lists of a save of an older version of the port, which
// put the spells of a new character and the picked up spells into the item
// list and gave a new cleric only Abjure (1.3: 0xB65BB, 0x8BDB4, 0xAF8F5).
// At the first level start after a load: the spells of the item list that a
// list of the character takes and has not go to it, one item each, and a
// cleric gets the cleric spells of the levels. A save of 1.3 has nothing to
// change. -origchar clears it.
int Dtu_fix_spell_save_repair = 1;
static int Spell_repair_pending;
int Dtu_fix_wizardry_ring = 1;
// ASCENT: 0xAF8C7 and 0xAF8CD add the new hit points of a level as an
// integer to the hit points of the player object and of P+0x53, which are
// fixed point (16.16). So in 1.3 the hit points in use grow by n / 65536
// at a new level, and only the maximum (P+0xA1) and the integer copy
// (P+0xA3) grow by n. 1 (default): the hit points grow by n. -origchar
// clears it.
int Dtu_fix_level_hit_points = 1;
int Dtu_fix_potion_frame = 1;
int Dtu_fix_luck_coin = 1;
int Dtu_fix_speed_items = 1;
int Dtu_fix_cursed_scroll = 1;
int Dtu_fix_potion_class = 1;
int Dtu_fix_creature_missile = 1;
int Dtu_backstab = 0;
int Dtu_auto_choice = -1;
static int Dtu_last_attack_roll = 0;

// The potion timers of the player record (0x396B2) and the values that
// belong to them. All times are GameTime.
static struct {
	fix	intel_end;			// P+0x1A93 Poshun uv Hil Gient Inteljuns
	fix	str_end;				// P+0x1A97 Hill Giant Strength
	fix	dragon_end;			// P+0x1A9B Dragon's Blood
	fix	dex_end;				// P+0x1A9F Agility
	fix	luck_end;			// P+0x1A58 Luck
	int	intel;				// the intelligence of the potion
	int	dragon_extra;		// the percent value of Dragon's Blood for a warrior
	int	gem_uses;			// P+0x1A57 Gem of Curing: the uses (byte, saved)
	int	str_gain, pct_gain;	// P+0x1A55, P+0x1A56: the gain of the spell Strength
	fix	intel_acc;			// Dtu_fix_potion_frame
	int	load_left[4];		// ASCENT: the seconds left of the four timers of a loaded save (0xBA911), for the level start
} Potion;
static int Worn_bits = 0;		// 0x1458AC: the effect bits of the worn items
static int Wizardry_count = 0;	// doublings of the Ring of Wizardry

static FILE *Char_log = NULL;
static int Char_options_read = 0;
static char *Char_save_file = NULL;
static char *Char_script = NULL;
static int Char_frame = 0;
static int Known_n_inventory = 0;

// ASCENT: the player record of the last loaded save game. dtu_char_pack
// starts from it, so the fields that the port does not use (docs/re/
// character.md lists the known ones) go back into a save unchanged and a
// save of the port stays a save of the original.
static ubyte Loaded_record[DTU_PLAYER_RECORD_SIZE];
static int Have_loaded_record;
extern char Dtu_player_archive[16];		// dtugame.c
extern int Fe_khelben_mission;			// MsnNum 0x13E588 (dtufe_town.c)

// The time when a rest place works again (0x1C5E40 of the station table,
// here for each segment).
static fix Rest_time[MAX_SEGMENTS];

//------------------------------------------------------------------------
// Tables of the executable

// 0x146048: the experience for a level. Entry [class][level + 2] is the
// experience that ends level "level" (0 is the first level).
static int Xp_table[4][21] = {
	{ 0, 0, 1500, 3000, 6000, 13000, 27500, 55000, 110000, 225000, 450000, 675000, 900000, 1125000, 1350000, 1575000, 1800000, 2025000, 2250000, 92475000, 992700000 },
	{ 0, 0, 2500, 5000, 10000, 20000, 40000, 60000, 90000, 135000, 250000, 375000, 750000, 1125000, 1500000, 1875000, 2250000, 2625000, 3000000, 93375000, 993750000 },
	{ 0, 0, 2000, 4000, 8000, 16000, 32000, 64000, 125000, 250000, 500000, 750000, 1000000, 1250000, 1500000, 1750000, 2000000, 2250000, 2500000, 82750000, 993000000 },
	{ 0, 0, 1250, 2500, 5000, 10000, 20000, 30000, 70000, 110000, 160000, 220000, 440000, 660000, 880000, 1100000, 1320000, 1540000, 1760000, 91980000, 992200000 },
};

// 0x146590: THAC0. The index is the level plus 1.
static ubyte Thac0_table[4][18] = {
	{ 20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14, 12, 12, 12, 10, 10, 10 },
	{ 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 15 },
	{ 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3 },
	{ 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13, 13, 12, 12 },
};

// 0x146518: the hit die of a class. 0x146538: the number of dice at a
// level. 0x146528: the fixed hit points at a level.
static int Hit_die[4] = { 8, 4, 10, 6 };
static int Hit_dice_levels[4] = { 9, 10, 9, 10 };		// a die up to this level value (0 is the first level), the fixed value after it
static int Hit_fixed[4] = { 2, 1, 3, 2 };

// 0x1402E2 and 0x140332 (fighter): hit points for constitution 0 to 19
static int Con_bonus[2][20] = {
	{ -9, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2, 2 },
	{ -9, -3, -2, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5 },
};

// 0x13A956 and 0x13A9A2: attack and damage bonus for strength 0 to 19.
// 0x13A9F2 and 0x13AA06: for a fighter with strength 18, by the percent
// value (1 to 50, 51 to 75, 76 to 90, 91 to 99, and 0 for 18/00).
static int Str_hit[20] = { 0, -5, -3, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3 };
static int Str_damage[20] = { 3, -4, -2, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 7 };
static int Str_extra_hit[5] = { 0, 1, 1, 1, 2 };
static int Str_extra_damage[5] = { 1, 1, 2, 3, 4 };

// 0x14019A: armor class for dexterity 0 to 19
static int Dex_ac[20] = { 5, 5, 5, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4, -4 };
// 0x146240: what the Gloves of Dexterity add, for dexterity 0 to 19
static int Dex_glove[20] = { 4, 4, 4, 4, 4, 4, 4, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1 };

// 0x145C04: armor class of the armor numbers 0 to 52
static signed char Armor_ac[53] = {
	-2, -2, -1, -5, -7, -3, -4, -6, -7, -8, -9, -10, -2, -3, -1, -3,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -2, -3, -4,
	-5, -1, 0, 0, 0, 0, 0, -2, -4, -6, -8, 0, 0, 0, -1, -1,
	-2, -3, -1, -1, -1 };

// 0x13F6CD and the next ones: the items of a new character, by class
// (0x8BD25). Each character gets a Scroll (misc 102) before them (0xB655D).
static ubyte Start_items[4][4][2] = {
	{ { OBJ_WEAPON, 27 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 79 }, { 0, 0 } },						// cleric: Mace, Leather Armor, Potion of Healing
	{ { OBJ_WEAPON, 18 }, { OBJ_MISC, 79 }, { 0, 0 }, { 0, 0 } },									// mage: Dagger, Potion of Healing (and six spells)
	{ { OBJ_WEAPON, 20 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 2 }, { OBJ_MISC, 79 } },				// fighter: Broad Sword, Leather Armor, Gem of Curing, Potion of Healing
	{ { OBJ_WEAPON, 19 }, { OBJ_ARMOR, 1 }, { OBJ_MISC, 13 }, { OBJ_MISC, 79 } },			// thief: Short Sword, Leather Armor, Apprentice Lockpicks, Potion of Healing
};

//------------------------------------------------------------------------
// Log

void dtu_char_log(char *format, ...)
{
	va_list args;

	if (!Char_log)
		return;
	fprintf(Char_log, "%8.3f f%-5d ", f2fl(GameTime), FrameCount);
	va_start(args, format);
	vfprintf(Char_log, format, args);
	va_end(args);
	fprintf(Char_log, "\n");
	fflush(Char_log);
}

static void read_options(void)
{
	int t;

	if (Char_options_read)
		return;
	Char_options_read = 1;
	if ((t = FindArg("-charlog")) != 0 && t + 1 < Num_args)
		Char_log = fopen(Args[t + 1], "wt");
	if ((t = FindArg("-char")) != 0 && t + 1 < Num_args)
		Char_save_file = Args[t + 1];
	if ((t = FindArg("-chartest")) != 0 && t + 1 < Num_args)
		Char_script = Args[t + 1];
	if (FindArg("-origchar"))
		Dtu_fix_multiclass_thac0 = Dtu_fix_spell_save_repair = Dtu_fix_level_hit_points = Dtu_fix_secret_door_names =
			Dtu_fix_thac0_row = Dtu_fix_new_thac0 = 0;
	if (FindArg("-origcombat"))
		Dtu_fix_wizardry_ring = Dtu_fix_potion_frame = Dtu_fix_luck_coin = Dtu_fix_speed_items = Dtu_fix_cursed_scroll = Dtu_fix_creature_missile = Dtu_fix_potion_class = Dtu_fix_backstab =
			Dtu_fix_creature_strength = Dtu_fix_shield_ac = 0;
	if ((t = FindArg("-autochoice")) != 0 && t + 1 < Num_args)
		Dtu_auto_choice = atoi(Args[t + 1]);
}

// A text of the string table of the game, or the default.
static char *game_text(int n, char *def)
{
	if (n >= 0 && n < N_TEXT_STRINGS && Text_string[n] && Text_string[n][0])
		return Text_string[n];
	return def;
}

//------------------------------------------------------------------------
// Item data

static item_info *item_entry(int type, int id)
{
	if (id < 0)
		return NULL;
	switch (type) {
		case OBJ_MISC:		return (id < N_misc_types) ? &Misc_info[id] : NULL;
		case OBJ_ARMOR:	return (id < N_armor_types) ? &Armor_info[id] : NULL;
		case OBJ_SPELL:	return (id < N_spell_types) ? &Spell_info[id] : NULL;
		case OBJ_WEAPON:	return (id < N_weapon_types) ? &Weapon_item_info[id] : NULL;
	}
	return NULL;
}

static int rec_int(int record, char *key, int def)
{
	char value[80];

	if (record < 0 || !bm_tbl_get(record, key, value, sizeof(value)))
		return def;
	return atoi(value);
}

static fix rec_fix(int record, char *key, fix def)
{
	char value[80];

	if (record < 0 || !bm_tbl_get(record, key, value, sizeof(value)))
		return def;
	return fl2f(atof(value));
}

// The keywords of the readers 0x23780 ($OBJECT), 0x22BB1 ($ARMOR) and
// 0x209F3 ($WEAPON).
int dtu_item_get(int type, int id, dtu_item_data *d)
{
	item_info *it = item_entry(type, id);
	int r;
	char value[80];

	memset(d, 0, sizeof(*d));
	if (!it || it->record < 0)
		return 0;
	r = it->record;
	d->name = it->name;
	if (!d->name[0] && type == OBJ_WEAPON) {
		static char weapon_name[DTU_NAME_LENGTH];

		bm_tbl_get(r, "weapon_name", weapon_name, sizeof(weapon_name));
		d->name = weapon_name;
	}
	d->icon = it->icon > 0 ? it->icon : 0;
	d->value = it->value;
	d->stackable = bm_tbl_has_flag(r, "stackable");
	d->stack_limit = it->stack_limit > 0 ? it->stack_limit : 10;
	d->uses = rec_int(r, "uses", 1);

	if (bm_tbl_has_flag(r, "all_use"))		d->class_mask |= DTU_USE_ALL;
	if (bm_tbl_has_flag(r, "cleric_use"))	d->class_mask |= DTU_USE_CLERIC;
	if (bm_tbl_has_flag(r, "mage_use"))		d->class_mask |= DTU_USE_MAGE;
	if (bm_tbl_has_flag(r, "fighter_use"))	d->class_mask |= DTU_USE_FIGHTER;
	if (bm_tbl_has_flag(r, "thief_use"))	d->class_mask |= DTU_USE_THIEF;

	if (type == OBJ_MISC || type == OBJ_ARMOR) {
		if (bm_tbl_has_flag(r, "helmet"))		d->wear_mask |= DTU_WEAR_HELMET;
		if (bm_tbl_has_flag(r, "cloak"))			d->wear_mask |= DTU_WEAR_CLOAK;
		if (bm_tbl_has_flag(r, "necklace"))		d->wear_mask |= DTU_WEAR_NECKLACE;
		if (bm_tbl_has_flag(r, "armor"))			d->wear_mask |= DTU_WEAR_ARMOR;
		if (bm_tbl_has_flag(r, "gauntlet"))		d->wear_mask |= DTU_WEAR_GAUNTLET;
		if (bm_tbl_has_flag(r, "shield"))		d->wear_mask |= DTU_WEAR_SHIELD;
		if (bm_tbl_has_flag(r, "ring"))			d->wear_mask |= DTU_WEAR_RING;
		if (bm_tbl_has_flag(r, "belt"))			d->wear_mask |= DTU_WEAR_BELT;
		if (bm_tbl_has_flag(r, "boot"))			d->wear_mask |= DTU_WEAR_BOOT;
	}
	if (type == OBJ_MISC) {
		if (bm_tbl_has_flag(r, "bow"))			d->wear_mask |= DTU_WEAR_BOW;
		if (bm_tbl_has_flag(r, "crossbow"))		d->wear_mask |= DTU_WEAR_CROSSBOW;
		if (bm_tbl_has_flag(r, "lockpick")) {
			d->wear_mask |= DTU_WEAR_LOCKPICK;
			d->class_mask |= DTU_USE_THIEF;
		}
		// The table lines of the four lockpick items (10 to 13) of the disc
		// have no wear word. The table fixup (0x1A528, dtutblfix.h) gives
		// them the lockpick wear bit 0x800 and the thief bit, and the code
		// of the lockpick slot knows these four numbers (0xB02D6). The test
		// keeps the slot with -rawtables.
		if (id >= 10 && id <= 13)
			d->wear_mask |= DTU_WEAR_LOCKPICK;
		// 0x145A04: armor class of the misc numbers 0 to 127
		if (id == 8 || id == 9)
			d->ac_bonus = -1;
		d->to_hit_bonus = rec_int(r, "to_hit_bonus", 0);
		d->damage_bonus = rec_int(r, "damage_bonus", 0);
		// 0x31752: a misc item with the flag non_hotkey_item has no hot key
		d->hot_key_ok = !bm_tbl_has_flag(r, "non_hotkey_item");
	}
	else if (type == OBJ_ARMOR) {
		if (id < 53)
			d->ac_bonus = Armor_ac[id];
		d->hot_key_ok = 0;
	}
	else if (type == OBJ_WEAPON) {
		// 0x1458B8: armor class of the weapon numbers 0 to 82. One entry is
		// not 0: number 54, the table line has ac_adjustment=5.
		if (id == 54)
			d->ac_bonus = -9;
		d->melee = 1;
		if (bm_tbl_get(r, "attack_type", value, sizeof(value)) && !stricmp(value, "PROJECTILE"))
			d->melee = 0;
		// ASCENT: the values of a line with no key are the ones of 0x207E7
		// (the reader 0x209F3 calls it first): 1d6 (+0x0D 6, +0x0E 1) and a
		// fire_wait of 0.25 (+0x64 0x4000), as bm_read_weapon. The port took
		// 0 sides and 1.0: the arrows of the tables (weapon 10, 12, 74, 75)
		// have no damage_dice key and rolled no dice.
		d->dice_sides = rec_int(r, "damage_dice", 6);
		d->dice_n = rec_int(r, "#damage_dice", 1);
		d->damage_bonus = rec_int(r, "damage_bonus", 0);
		d->to_hit_bonus = rec_int(r, "to_hit_bonus", 0);
		d->attack_wait = rec_fix(r, "fire_wait", F1_0 / 4);
		d->hot_key_ok = 1;
	}
	else
		d->hot_key_ok = 1;
	return 1;
}

static int entry_data(int index, dtu_item_data *d)
{
	if (index < 0 || index >= Dtu_n_inventory) {
		memset(d, 0, sizeof(*d));
		return 0;
	}
	return dtu_item_get(Dtu_inventory[index].type, Dtu_inventory[index].id, d);
}

// The entry of a reference, or NULL (0x35863).
static dtu_inv_item *ref_entry(dtu_item_ref ref)
{
	if (ref.list != 0 || ref.n < 0 || ref.n >= Dtu_n_inventory)
		return NULL;
	return &Dtu_inventory[ref.n];
}

static int find_entry(int type, int id)
{
	int i;

	for (i = 0; i < Dtu_n_inventory; i++)
		if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id)
			return i;
	return -1;
}

//------------------------------------------------------------------------
// Classes

static int valid_class(int c)			// 0x35AAB
{
	return c >= 0 && c <= 3;
}

static int has_class(int c)			// 0x35A09 cleric, 0x35A2B mage, 0x35A6F fighter, 0x35A4D thief
{
	return Dtu_char.pa.class1 == c || Dtu_char.pa.class2 == c;
}

static int class_use_bits(void)
{
	static int bits[4] = { DTU_USE_CLERIC, DTU_USE_MAGE, DTU_USE_FIGHTER, DTU_USE_THIEF };
	int m = 0;

	if (valid_class(Dtu_char.pa.class1)) m |= bits[Dtu_char.pa.class1];
	if (valid_class(Dtu_char.pa.class2)) m |= bits[Dtu_char.pa.class2];
	return m;
}

// ASCENT: D-002 (verified in the bytes, 0xAF62D to 0xAF654): 0xAF605 reads
// byte level value + 1 of the row of 18 bytes, so level 18 (level value
// 17) reads past the row: the next row in memory (0x146548 cleric,
// 0x14655A thief, 0x14656C fighter, 0x14657E mage), and for a mage the low
// byte 0x48 (72) of the pointer table 0x146590. 1 (default): the last
// entry of the row. 0 (-origchar): the bytes that 1.3 reads.
int Dtu_fix_thac0_row = 1;

// ASCENT: D-089 (verified in the bytes, 0x8BF3E, 0x8BFF5): a new character
// has THAC0 20 until the first experience award. 1 (default): the THAC0 of
// the table from the start (each equipment update computes it). 0
// (-origchar): only the level code computes it, as 1.3.
int Dtu_fix_new_thac0 = 1;

static int thac0_entry(int c, int l)
{
	if (l < 0) l = 0;
	if (l == 18 && !Dtu_fix_thac0_row)
		switch (c) {
			case DTU_CLASS_CLERIC:	return Thac0_table[DTU_CLASS_THIEF][0];
			case DTU_CLASS_THIEF:	return Thac0_table[DTU_CLASS_FIGHTER][0];
			case DTU_CLASS_FIGHTER:	return Thac0_table[DTU_CLASS_MAGE][0];
			default:						return 0x48;
		}
	if (l > 17) l = 17;
	return Thac0_table[c][l];
}

// 0xAF605
static void update_thac0(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1, t = 20, t2;

	if (valid_class(pa->class1))
		t = thac0_entry(pa->class1, l1);
	if (valid_class(pa->class2)) {
		t2 = thac0_entry(pa->class2, l2);
		// The executable takes the higher value (0xAF657).
		if (Dtu_fix_multiclass_thac0 ? (t2 < t) : (t2 > t))
			t = t2;
	}
	Dtu_char.thac0 = t;
}

//------------------------------------------------------------------------
// The values that depend on the equipment (0xAFB55)

static int armor_is(dtu_inv_item *e, int a, int b)
{
	return e && e->type == OBJ_ARMOR && e->id >= a && e->id <= b;
}

static int table_ac(dtu_inv_item *e)
{
	dtu_item_data d;

	if (!e || !dtu_item_get(e->type, e->id, &d))
		return 0;
	return d.ac_bonus;
}

// ASCENT: the numbers that 0xAFB55 tests, as names (dtu_char_update and
// dtu_char_item_powers use them)
static int is_shield(dtu_inv_item *e)		{ return armor_is(e, 2, 2) || armor_is(e, 12, 13); }		// 0xAF9CF
static int is_magic_armor(dtu_inv_item *e)	{ return armor_is(e, 5, 11) || armor_is(e, 39, 42); }		// 0xAF9F3, 0xAF982
static int is_heavy_armor(dtu_inv_item *e)	{ return armor_is(e, 3, 4) || armor_is(e, 7, 11); }		// 0xB0B0A
static int is_prot_cloak(dtu_inv_item *e)	{ return armor_is(e, 47, 49); }								// 0xAF930
static int is_prot_ring(dtu_inv_item *e)	{ return armor_is(e, 0, 0) || armor_is(e, 14, 15); }		// 0xAF9AB
#define LATHANDER_SPELL	8		// armor 45: USE casts Continual Light (0x37A6A)

// ASCENT: the armor class that an item adds in a slot (DTU_N_SLOTS: the
// hand), before the rules that need other items (the cloaks and the rings
// of protection, magic armor). dtu_char_update and dtu_char_item_ac use it.
static int slot_ac(dtu_inv_item *e, int slot)
{
	switch (slot) {
		case DTU_SLOT_BOOT:		// 0xAFBC8: the boot place reads only armor
		case DTU_SLOT_ARMOR:
			return e->type == OBJ_ARMOR ? table_ac(e) : 0;
		case DTU_SLOT_NECKLACE:	// armor 33 (Amulet of Shield) gives a spell effect only
			return armor_is(e, 33, 33) ? 0 : table_ac(e);
		case DTU_N_SLOTS:		// the item in the hand (0xB0356)
			return (e->type == OBJ_WEAPON || e->type == OBJ_MISC) ? table_ac(e) : 0;
		case DTU_SLOT_BELT: case DTU_SLOT_SHIELD: case DTU_SLOT_GAUNTLET: case DTU_SLOT_CLOAK:
		case DTU_SLOT_RING1: case DTU_SLOT_RING2:
			return table_ac(e);
	}
	return 0;		// the lockpicks, the bow and the crossbow
}

// 0xB02BE: Open Locks of a thief with lockpicks
int dtu_char_lockpick_bonus(int id)
{
	switch (id) {
		case 10: return 5;
		case 13: return 10;
		case 11: return 15;
		case 12: return 20;
	}
	return 0;
}

// The effect bits of the worn items (0xAFB55 collects them in edx). Item
// bit n gives the effect of Worn_fx[n] (0xB08A8, 0xB0A98).
static const int Worn_fx[15] = {
	DTU_FX_CHILL_TOUCH, DTU_FX_RESIST_FIRE, 0, DTU_FX_FEATHER_FALL, DTU_FX_FLY, DTU_FX_FREE_ACTION, DTU_FX_INFRAVISION,
	DTU_FX_JUMP, DTU_FX_DETECT_MAGIC, DTU_FX_HASTE, DTU_FX_SHIELD, DTU_FX_LEVITATE, DTU_FX_IMP_INVIS, DTU_FX_INVISIBLE,
	DTU_FX_PROT_EVIL
};

static int armor_id(dtu_item_ref ref)
{
	dtu_inv_item *e = ref_entry(ref);

	return (e && e->type == OBJ_ARMOR) ? e->id : -1;
}

// ASCENT: the item bits (Worn_fx) of one item in one slot (0xAFB55 with
// the jump tables 0xAFAE5, 0xAFAF5, 0xAFB25); slot DTU_N_SLOTS is the hand.
// worn_item_bits and the description window (dtu_char_item_fx) use it.
// any_race: the tests of the race pass (a human).
static int item_bits(int type, int id, int slot, int any_race)
{
	int bits = 0, human = any_race || Dtu_char.pa.race == 0;

	switch (slot) {
		case DTU_SLOT_BOOT:			// boots (0xAFBEE, jump table 0xAFAE5)
			if (type == OBJ_ARMOR)
				switch (id) {
					case 34: bits |= 1 << 4; break;		// Winged Boots: Fly
					case 35: bits |= 1 << 3; break;		// Boots of Feather Falling
					case 36: bits |= 1 << 11; break;		// Boots of Levitation
					case 37: bits |= 1 << 9; break;		// Boots of Speed: Haste
				}
			break;
		case DTU_SLOT_NECKLACE:		// 33: Amulet of Shield (0xAFCBA tests the number only)
			if (id == 33)
				bits |= 1 << 10;
			break;
		case DTU_SLOT_GAUNTLET:		// 51: Glim-Gauntlet, Detect Magic (0xAFD99)
			if (type == OBJ_ARMOR && id == 51)
				bits |= 1 << 8;
			break;
		case DTU_SLOT_CLOAK:			// 44: Chameleon Cloak, Improved Invisibility; 46: Elven Cloak, Invisibility (0xAFF54)
			if (type == OBJ_ARMOR && id == 44) bits |= 1 << 12;
			if (type == OBJ_ARMOR && id == 46) bits |= 1 << 13;
			break;
		case DTU_SLOT_RING1:			// rings (jump tables 0xAFAF5 and 0xAFB25). Ring 16
		case DTU_SLOT_RING2:			// (Armoring) sets no effect bit: see armoring_ac().
			if (type == OBJ_ARMOR)
				switch (id) {
					case 17: bits |= 1 << 1; break;		// Ring of Coolness: Resist Fire
					case 18: bits |= 1 << 3; break;		// Feather Fall
					case 19: bits |= 1 << 4; break;		// Flying
					case 20: bits |= 1 << 5; break;		// Free Action
					case 22: if (any_race || Dtu_char.pa.race <= 0) bits |= 1 << 6; break;		// Infravision, humans only
					case 23: bits |= 1 << 7; break;		// Jumping
					case 24: bits |= 1 << 8; break;		// Magic Detection
					case 25: bits |= 1 << 9; break;		// Quickness: Haste
				}
			break;
		case DTU_N_SLOTS:
			// weapon 54 in the hand (0xB038F): bits 0, 1, 3, 5, 7, 8, 9, 12,
			// and 6 for a human
			if (type == OBJ_WEAPON && id == 54) {
				bits |= 0x2b | 0x1380;
				if (human)
					bits |= 0x40;
			}
			break;
	}
	return bits;
}

// ASCENT: Dtu_fix_speed_items. The line of the Short Bow of Speed has
// pc_spell1=21 and the line of the Short Sword of Quickness pc_spell=21
// (Haste). The executable has no keyword pc_spell1, 0xAFB55 does not read
// the bow slot and 0x44300 reads PC_spell only for a weapon with
// attack_type 0: no power. Corrected: the attacks of the item are twice as
// fast (dtuatk.c). No Haste effect: item_bits does not read them.
int dtu_char_item_fast(int type, int id)
{
	item_info *it = item_entry(type, id);
	dtu_item_data d;
	int slot = dtu_char_item_slot(type, id);

	if (!it)
		return 0;
	if (slot == DTU_SLOT_BOW)
		return dtu_atk_item_fast(rec_int(it->record, "pc_spell1", -1), Dtu_fix_speed_items);
	if (slot == DTU_N_SLOTS && dtu_item_get(type, id, &d) && d.melee)
		return dtu_atk_item_fast(rec_int(it->record, "pc_spell", -1), Dtu_fix_speed_items);
	return 0;
}

static int worn_item_bits(void)
{
	static const int slots[7] = { DTU_SLOT_BOOT, DTU_SLOT_NECKLACE, DTU_SLOT_GAUNTLET, DTU_SLOT_CLOAK,
		DTU_SLOT_RING1, DTU_SLOT_RING2, DTU_SLOT_BOW };
	int bits = 0, i;
	dtu_inv_item *e;

	for (i = 0; i < 7; i++) {
		e = ref_entry(Dtu_char.slot[slots[i]]);
		if (e)
			bits |= item_bits(e->type, e->id, slots[i], 0);
	}
	e = ref_entry(Dtu_char.current);
	if (e)
		bits |= item_bits(e->type, e->id, DTU_N_SLOTS, 0);
	return bits;
}

static void worn_effects(void)
{
	object *p = ConsoleObject;
	int bits, gone, i, rings = 0;

	if (!p || p->type != OBJ_PLAYER)
		return;
	bits = worn_item_bits();
	gone = Worn_bits & ~bits;
	// 0xB03FC: an item that is taken off ends its effect (0xB0A98 with 1)
	for (i = 0; i < 15; i++)
		if ((gone & (1 << i)) && Worn_fx[i])
			dtu_spell_clear_effect(p, Worn_fx[i]);
	if (bits != Worn_bits)
		dtu_char_log("worn items: effect bits 0x%04x (were 0x%04x)", bits, Worn_bits);
	Worn_bits = bits;
	// 0xB08A8: the effects of the worn items, with no end. Infravision only
	// for a human (0xB0968).
	for (i = 0; i < 15; i++)
		if ((bits & (1 << i)) && Worn_fx[i] && !(i == 6 && Dtu_char.pa.race != 0)) {
			if (!(p->dtu.m_88 & Worn_fx[i]))
				dtu_spell_set_effect(p, Worn_fx[i], DTU_FX_NO_END, 0, 0);
		}

	// The Ring of Wizardry (armor 27, 0xB00A5): the mage slots of the spell
	// levels 1 to 3 double each time 0xAFB55 runs while it is worn, and
	// are halved one time when it is taken off (0xB075F). Corrected: one
	// doubling while it is worn.
	if (armor_id(Dtu_char.slot[DTU_SLOT_RING1]) == 27) rings++;
	if (armor_id(Dtu_char.slot[DTU_SLOT_RING2]) == 27) rings++;
	if (rings) {
		if (Dtu_fix_wizardry_ring)
			Wizardry_count = 1;
		else
			Wizardry_count += rings;
	}
	else if (Wizardry_count > 0)
		Wizardry_count--;
	dtu_spell_set_wizardry(Wizardry_count);
}

// ASCENT: 0xAFAD1 with the Ring of Armoring (armor 16) on a mage: the caps
// of Armor, Magical Vestment and Shield of dtu_spell_player_ac(), each
// followed by 1 better armor class (0xB05FC, 0xB064F, 0xB067C).
// ASCENT: missile: the value against missiles (Shield, Dtu_fix_shield_ac).
static int armoring_ac(int ac, int missile)
{
	int shield = dtu_spell_shield_ac(missile);
	int w = (ConsoleObject && ConsoleObject->type == OBJ_PLAYER) ? ConsoleObject->dtu.m_88 : 0;

	if ((w & DTU_FX_ARMOR) && ac > 6)
		ac = 6 - 1;
	if (w & DTU_FX_VESTMENT) {
		int L = dtu_spell_player_level(35), v = 5 - (L - 5) / 3;

		if (v < 1) v = 1;
		if (v < ac) ac = v - 1;
	}
	if ((w & DTU_FX_SHIELD) && ac > shield)
		ac = shield - 1;
	if (w & DTU_FX_SLOW)
		ac += 4;
	return ac;
}

// The values of the potions and of the spell Strength (0x396B2, 0x8FCFA)
static void potion_values(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	// ASCENT: the values of the table of use_misc
	int giant = dtu_char_misc_use(0x50)->lo, dragon = dtu_char_misc_use(0x4A)->lo, dex = dtu_char_misc_use(0x48)->lo;

	if (Potion.str_end) {								// Hill Giant Strength: at least 19
		if (pa->strength < giant) {
			pa->strength = giant;
			pa->strength_extra = 0;
		}
	}
	if (Potion.dragon_end) {							// Dragon's Blood: at least 18
		if (pa->strength < dragon) {
			pa->strength = dragon;
			pa->strength_extra = Potion.dragon_extra;
		}
	}
	if (Potion.dex_end && pa->dexterity < dex)	// Agility: at least 18
		pa->dexterity = dex;
	if (Potion.intel_end)
		pa->intelligence = Potion.intel;
	if (Potion.str_gain || Potion.pct_gain) {	// the spell Strength
		pa->strength += Potion.str_gain;
		if (Potion.pct_gain) {
			int x = pa->strength_extra + Potion.pct_gain;
			pa->strength_extra = (x > 100) ? 0 : x;
		}
	}
}

// 0x8FCFA: the gain of the spell Strength. A warrior: 1d8, above 18 each
// point is 10 percent (above 100 it becomes 0, 18/00); not at 18/00. Other
// classes: at most 18, nothing at 18.
void dtu_char_strength_gain(int gain, int warrior)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int s = pa->strength, e = pa->strength_extra, g = 0, p = 0;

	if (warrior) {
		if (s == 18 && e == 0)
			return;
		if (s + gain > 18) {
			g = 18 - s;
			p = 10 * (s + gain - 18);
			if (e + p > 100)
				p = 100 - e;
		}
		else
			g = gain;
	}
	else {
		if (s >= 18)
			return;
		g = (s + gain > 18) ? 18 - s : gain;
	}
	Potion.str_gain = g;
	Potion.pct_gain = p;
	dtu_char_log("Strength: strength %d/%d, gain %d and %d percent", s, e, g, p);
	dtu_char_update();
}

// 0x90BE4: the gains go
void dtu_char_strength_end(void)
{
	Potion.str_gain = Potion.pct_gain = 0;
	dtu_char_update();
}

void dtu_char_update(void)
{
	dtu_player_attr *pa = &Dtu_char.pa, *base = &Dtu_char.base;
	dtu_inv_item *e;
	int sum = 0, ring_best = 0;
	int shield = 0, magic_armor = 0, heavy_armor = 0, prot_ring = 0;
	int ac, dex, missile;

	if (!Dtu_char.created)
		return;

	// The thief skills start from the values without items (0xAFB92). An
	// item that sets an ability sets it while it is worn (0xB07C6).
	pa->hide_in_shadows = base->hide_in_shadows;
	pa->move_silently = base->move_silently;
	pa->find_traps = base->find_traps;
	pa->climb_walls = base->climb_walls;
	pa->open_locks = base->open_locks;
	pa->strength = base->strength;
	pa->strength_extra = base->strength_extra;
	pa->dexterity = base->dexterity;
	pa->constitution = base->constitution;

	// boot
	e = ref_entry(Dtu_char.slot[DTU_SLOT_BOOT]);
	if (e)
		sum += slot_ac(e, DTU_SLOT_BOOT);

	// belt. Armor 38 is the Girdle of Hill Giant Strength.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_BELT]);
	if (e) {
		sum += slot_ac(e, DTU_SLOT_BELT);
		if (armor_is(e, 38, 38)) {
			pa->strength = DTU_GIRDLE_STRENGTH;
			pa->strength_extra = 0;
		}
	}

	// necklace. Armor 33 (Amulet of Shield) gives a spell effect only.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_NECKLACE]);
	if (e)
		sum += slot_ac(e, DTU_SLOT_NECKLACE);

	// shield. 0xAF9CF: the numbers 2, 12 and 13 are shields.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_SHIELD]);
	if (e) {
		shield = is_shield(e);
		sum += slot_ac(e, DTU_SLOT_SHIELD);
	}

	// gauntlet
	e = ref_entry(Dtu_char.slot[DTU_SLOT_GAUNTLET]);
	if (e) {
		sum += slot_ac(e, DTU_SLOT_GAUNTLET);
		if (armor_is(e, 52, 52)) {					// Gauntlets of Ogre Power
			pa->strength = DTU_OGRE_STRENGTH;
			pa->strength_extra = 0;
		}
		else if (armor_is(e, 50, 50)) {			// Gloves of Dexterity
			if (pa->dexterity <= 19)
				pa->dexterity += Dex_glove[pa->dexterity];
			if (has_class(DTU_CLASS_THIEF))
				pa->open_locks += DTU_GLOVES_LOCKS_THIEF;
			else
				pa->open_locks = DTU_GLOVES_LOCKS;
		}
		else if (e->type == OBJ_MISC && e->id == 9) {	// Gauntlets (0xB0B5F)
			pa->find_traps -= DTU_SKILL_LOSS;
			pa->open_locks -= DTU_SKILL_LOSS;
		}
	}

	// armor. 0xAF9F3: the numbers 5 to 11 are magic armor. 0xAF982: 39 to
	// 42 are the Bracers of Defense. 0xB0B0A: in the numbers 3, 4 and 7 to
	// 11 (chain mail, plate mail) three thief skills do not work.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_ARMOR]);
	if (e && e->type == OBJ_ARMOR) {
		sum += slot_ac(e, DTU_SLOT_ARMOR);
		magic_armor = is_magic_armor(e);
		heavy_armor = is_heavy_armor(e);
		if (heavy_armor) {								// 0xB0B36
			pa->hide_in_shadows -= DTU_SKILL_LOSS;
			pa->move_silently -= DTU_SKILL_LOSS;
			pa->climb_walls -= DTU_SKILL_LOSS;
		}
	}

	// cloak. 0xAF930: the numbers 47 to 49 are the Cloaks of Protection.
	// They do not work with a shield, magic armor or heavy armor.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_CLOAK]);
	if (e) {
		if (e->type == OBJ_ARMOR) {
			if (!is_prot_cloak(e) || (!shield && !magic_armor && !heavy_armor))
				sum += slot_ac(e, DTU_SLOT_CLOAK);
			if (e->id == 43)								// Cloak of Elvenkind
				pa->hide_in_shadows = DTU_ELVENKIND_HIDE;
		}
		else
			sum += table_ac(e);
	}

	// rings. 0xAF9AB: the numbers 0, 14 and 15 are the Rings of Protection.
	// No ring adds to magic armor. Of two rings of protection the better one
	// counts. 21 is the Ring of Health, 26 the Ring of Strength.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_RING1]);
	if (e) {
		if (e->type == OBJ_ARMOR) {
			prot_ring = is_prot_ring(e);
			if (!magic_armor) {
				if (prot_ring) ring_best = table_ac(e);
				else sum += table_ac(e);
			}
			if (e->id == 21) pa->constitution = DTU_RING_CONSTITUTION;
			if (e->id == 26) { pa->strength = DTU_RING_STRENGTH; pa->strength_extra = 0; }
		}
		else
			sum += table_ac(e);
	}
	e = ref_entry(Dtu_char.slot[DTU_SLOT_RING2]);
	if (e) {
		if (e->type == OBJ_ARMOR) {
			int this_prot = is_prot_ring(e);
			int v = table_ac(e);

			prot_ring |= this_prot;
			if (!magic_armor) {
				if (prot_ring) { if (v < ring_best) ring_best = v; }
				else if (!this_prot) sum += v;
			}
			if (e->id == 21) pa->constitution = DTU_RING_CONSTITUTION;
			if (e->id == 26) { pa->strength = DTU_RING_STRENGTH; pa->strength_extra = 0; }
		}
		else
			sum += table_ac(e);
	}
	sum += ring_best;

	// lockpicks, for a thief (0xB02BE)
	e = ref_entry(Dtu_char.slot[DTU_SLOT_LOCKPICK]);
	if (e && e->type == OBJ_MISC && has_class(DTU_CLASS_THIEF))
		pa->open_locks += dtu_char_lockpick_bonus(e->id);

	// 0xB030B: invisible (effect bits 16, 17): Open Locks - 15. 0xB032A: the
	// Find Traps effect (bit 20): Find/Remove Traps 100.
	if (ConsoleObject && ConsoleObject->type == OBJ_PLAYER) {
		if (ConsoleObject->dtu.m_88 & (DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE))
			pa->open_locks -= 15;
		if (ConsoleObject->dtu.m_88 & DTU_FX_FIND_TRAPS)
			pa->find_traps = 100;
	}

	// the item in the hand (0xB0356)
	e = ref_entry(Dtu_char.current);
	if (e)
		sum += slot_ac(e, DTU_N_SLOTS);
	// 0xB038F: weapon 54 (Sword of W'hin) in the hand: Find/Remove Traps
	// and Climb Walls 250 (95 after the limit). ASCENT: Climb Walls was
	// missing (0xB03C2).
	if (e && e->type == OBJ_WEAPON && e->id == 54) {
		pa->find_traps = DTU_WHIN_SKILL;
		pa->climb_walls = DTU_WHIN_SKILL;
	}

	// The potions (0x396B2) and the spell Strength (0x8FCFA) change the
	// values of the record while they last
	potion_values();

	// 0xB03E6 and 0xB08A8: the effects of the worn items
	worn_effects();

	// 0xB0B7E: each thief skill is 0 to 95
	#define CLAMP95(v) do { if ((v) < 0) (v) = 0; if ((v) > DTU_SKILL_MAX) (v) = DTU_SKILL_MAX; } while (0)
	CLAMP95(pa->hide_in_shadows);
	CLAMP95(pa->move_silently);
	CLAMP95(pa->find_traps);
	CLAMP95(pa->climb_walls);
	CLAMP95(pa->open_locks);
	#undef CLAMP95

	// 0xB05C5. The spell effects that change the armor class (0xB05D4 to
	// 0xB0696 and 0xB0702) are in dtuspell.c.
	dex = pa->dexterity;
	if (dex < 1) dex = 1;
	if (dex > 19) dex = 19;
	pa->dexterity = dex;
	pa->dex_ac_adjust = Dex_ac[dex];
	// ASCENT: two values, against melee (the record, the screens) and
	// against missiles; they differ only with Shield (Dtu_fix_shield_ac)
	for (missile = 0; missile < 2; missile++) {
		ac = 10 + (signed char) sum;
		// ASCENT: Ring 16 (Armoring) in a ring place sets a flag (0xB0014,
		// 0xB01B6) that 0xAFAD1 reads: a mage gets 1 more after each cap.
		if (has_class(DTU_CLASS_MAGE) && (armor_id(Dtu_char.slot[DTU_SLOT_RING1]) == 16 || armor_id(Dtu_char.slot[DTU_SLOT_RING2]) == 16))
			ac = armoring_ac(ac, missile);
		else
			ac = dtu_spell_player_ac_vs(ac, 0, missile);
		ac += pa->dex_ac_adjust;
		ac = dtu_spell_player_ac(ac, 1);
		if (ac < -10) ac = -10;
		if (ac > 10) ac = 10;
		if (missile)
			Dtu_char.ac_missile = ac;
		else
			pa->armor_class = ac;
	}

	// ASCENT: 0xAFB55 does not call 0xAF605 (its callers are 0x8BA3A and
	// 0xAF489), so in 1.3 the THAC0 20 that the creation writes (0x8BFF5,
	// D-089) stays until the first level check. Dtu_fix_new_thac0 (default)
	// updates it here.
	if (Dtu_fix_new_thac0)
		update_thac0();
}

//------------------------------------------------------------------------
// The inventory. The list is Dtu_inventory[] of dtuobj.c, which the pick
// up code fills. The references of the character are indexes of it.

static void all_refs(void (*f)(dtu_item_ref *ref, int a), int a)
{
	int i;

	f(&Dtu_char.current, a);
	for (i = 0; i < DTU_N_HOT_KEYS; i++) f(&Dtu_char.hot_key[i], a);
	for (i = 0; i < DTU_N_SLOTS; i++) f(&Dtu_char.slot[i], a);
}

// A hot key box can hold a spell (list 1 or 2): the spell lists do not
// change with the item list.
static void ref_removed(dtu_item_ref *ref, int index)
{
	if (ref->list != 0) return;
	if (ref->n == index) ref->n = -1;
	else if (ref->n > index) ref->n--;
}

static void ref_check(dtu_item_ref *ref, int unused)
{
	(void) unused;
	if (ref->list != 0) return;
	if (ref->n >= Dtu_n_inventory) ref->n = -1;
}

// Takes items of an entry. The references follow the list when the entry
// goes away (0xB6F01 and 0x31C4B).
static void inv_remove(int index, int count)
{
	if (index < 0 || index >= Dtu_n_inventory)
		return;
	if (Dtu_inventory[index].count <= count)
		all_refs(ref_removed, index);
	dtu_inventory_remove(index, count);
	Known_n_inventory = Dtu_n_inventory;
	dtu_char_update();
}

// 0x31C41: a new weapon goes into the first free hot key box. The first
// weapon becomes the item in the hand. (The function was not read. The
// save game of a new character and the help text of the game show the
// result.)
// The name of a weapon is weapon_name= of its table line. The list code
// of dtuobj.c reads name=.
static void entry_name(int index)
{
	dtu_inv_item *e = &Dtu_inventory[index];
	item_info *it = item_entry(e->type, e->id);

	if (!e->name[0] && it && it->record >= 0)
		bm_tbl_get(it->record, "weapon_name", e->name, sizeof(e->name));
}

static void new_entry(int index)
{
	dtu_inv_item *e = &Dtu_inventory[index];
	int i;

	entry_name(index);
	if (e->type != OBJ_WEAPON)
		return;
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		if (Dtu_char.hot_key[i].list == 0 && Dtu_char.hot_key[i].n == index)
			return;
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		if (Dtu_char.hot_key[i].n < 0) {
			Dtu_char.hot_key[i].list = 0;
			Dtu_char.hot_key[i].n = index;
			break;
		}
	if (Dtu_char.current.n < 0) {
		Dtu_char.current.n = index;
		dtu_char_update();
	}
}

static void sync_inventory(void)
{
	int i;

	if (Dtu_n_inventory < Known_n_inventory)
		all_refs(ref_check, 0);
	for (i = Known_n_inventory; i < Dtu_n_inventory; i++)
		new_entry(i);
	Known_n_inventory = Dtu_n_inventory;
}

int dtu_char_wear_mask(int index)
{
	dtu_item_data d;

	if (!entry_data(index, &d))
		return 0;
	return d.wear_mask;
}

static int slot_wear_bit(int slot)
{
	static int bits[DTU_N_SLOTS] = { DTU_WEAR_LOCKPICK, DTU_WEAR_CLOAK, DTU_WEAR_NECKLACE, DTU_WEAR_ARMOR,
		DTU_WEAR_GAUNTLET, DTU_WEAR_SHIELD, DTU_WEAR_RING, DTU_WEAR_RING, DTU_WEAR_BELT, DTU_WEAR_BOOT,
		DTU_WEAR_BOW, DTU_WEAR_CROSSBOW };

	return (slot >= 0 && slot < DTU_N_SLOTS) ? bits[slot] : 0;
}

//------------------------------------------------------------------------
// ASCENT: the rules of one item, for the description window (dtudesc.c)

int dtu_char_item_slot(int type, int id)
{
	dtu_item_data d;
	int slot;

	if (type == OBJ_WEAPON)
		return DTU_N_SLOTS;
	if (!dtu_item_get(type, id, &d))
		return -1;
	for (slot = 0; slot < DTU_N_SLOTS; slot++)
		if (d.wear_mask & slot_wear_bit(slot))
			return slot;
	return -1;
}

int dtu_char_item_ac(int type, int id)
{
	dtu_inv_item e;
	int slot = dtu_char_item_slot(type, id);

	memset(&e, 0, sizeof(e));
	e.type = type;
	e.id = id;
	return slot < 0 ? 0 : slot_ac(&e, slot);
}

int dtu_char_item_fx(int type, int id)
{
	int slot = dtu_char_item_slot(type, id), bits, fx = 0, i;

	if (slot < 0)
		return 0;
	bits = item_bits(type, id, slot, 1);
	for (i = 0; i < 15; i++)
		if (bits & (1 << i))
			fx |= Worn_fx[i];
	return fx;
}

int dtu_char_item_powers(int type, int id, dtu_item_power *p, int n)
{
	dtu_inv_item e;
	int k = 0, slot = dtu_char_item_slot(type, id);

	#define ADD(a, b, c) do { if (k < n) { p[k].kind = (a); p[k].value = (b); p[k].value2 = (c); } k++; } while (0)
	memset(&e, 0, sizeof(e));
	e.type = type;
	e.id = id;
	// the values of dtu_char_update, in its order
	if (slot == DTU_SLOT_BELT && armor_is(&e, 38, 38))
		ADD(DTU_IP_STRENGTH, DTU_GIRDLE_STRENGTH, 0);
	if (slot == DTU_SLOT_GAUNTLET && armor_is(&e, 52, 52))
		ADD(DTU_IP_STRENGTH, DTU_OGRE_STRENGTH, 100);
	if (slot == DTU_SLOT_GAUNTLET && armor_is(&e, 50, 50)) {
		// the points for the base dexterity of the character of now
		ADD(DTU_IP_DEXTERITY, Dex_glove[Dtu_char.base.dexterity < 0 ? 0 : Dtu_char.base.dexterity > 19 ? 19 : Dtu_char.base.dexterity], 0);
		ADD(DTU_IP_LOCKS_THIEF, DTU_GLOVES_LOCKS_THIEF, 0);
		ADD(DTU_IP_LOCKS_OTHER, DTU_GLOVES_LOCKS, 0);
	}
	if (slot == DTU_SLOT_GAUNTLET && type == OBJ_MISC && id == 9)
		ADD(DTU_IP_NO_SKILLS, 2, 0);
	if (slot == DTU_SLOT_ARMOR && is_heavy_armor(&e))
		ADD(DTU_IP_NO_SKILLS, 1, 0);
	if (slot == DTU_SLOT_CLOAK && is_prot_cloak(&e))
		ADD(DTU_IP_PROT_CLOAK, 0, 0);
	if (slot == DTU_SLOT_CLOAK && armor_is(&e, 43, 43))
		ADD(DTU_IP_HIDE, DTU_ELVENKIND_HIDE, 0);
	if (slot == DTU_SLOT_RING1 && type == OBJ_ARMOR) {
		if (is_prot_ring(&e))
			ADD(DTU_IP_PROT_RING, 0, 0);
		if (table_ac(&e))
			ADD(DTU_IP_RING_NO_MAGIC, 0, 0);
		if (id == 21)
			ADD(DTU_IP_CONSTITUTION, DTU_RING_CONSTITUTION, 0);
		if (id == 26)
			ADD(DTU_IP_STRENGTH, DTU_RING_STRENGTH, 100);
		if (id == 16)
			ADD(DTU_IP_ARMORING, 0, 0);
		if (id == 27)
			ADD(DTU_IP_WIZARDRY, 0, 0);
	}
	if (slot == DTU_SLOT_LOCKPICK && dtu_char_lockpick_bonus(id))
		ADD(DTU_IP_LOCKS_THIEF, dtu_char_lockpick_bonus(id), 0);
	if (slot == DTU_N_SLOTS && id == 54)
		ADD(DTU_IP_TRAPS_CLIMB, DTU_WHIN_SKILL < DTU_SKILL_MAX ? DTU_WHIN_SKILL : DTU_SKILL_MAX, 0);
	// the use (dtu_char_use) and the key of Turn Undead (0xADCB3)
	if (type == OBJ_ARMOR && id == 45) {
		ADD(DTU_IP_USE_SPELL, LATHANDER_SPELL, 0);
		ADD(DTU_IP_TURN_UNDEAD, 0, 0);
	}
	// ASCENT: the attack rate of the fast items (Dtu_fix_speed_items)
	if (dtu_char_item_fast(type, id))
		ADD(DTU_IP_FAST_ATTACK, 0, 0);
	// 0xB0968: the effect Infravision of a worn item only for a human
	if (dtu_char_item_fx(type, id) & DTU_FX_INFRAVISION)
		ADD(DTU_IP_INFRA_HUMAN, 0, 0);
	#undef ADD
	return k < n ? k : n;
}

int dtu_char_slot_of(int index)
{
	int i;

	for (i = 0; i < DTU_N_SLOTS; i++)
		if (Dtu_char.slot[i].n == index && index >= 0)
			return i;
	return -1;
}

// 0x3067C and the next ones: a slot takes the item if the wear bits of the
// item have the bit of the slot. The test of the class bits is a guess:
// the place where the executable tests them was not found.
int dtu_char_equip(int index, int slot)
{
	dtu_item_data d;
	int old;

	if (!entry_data(index, &d) || !(d.wear_mask & slot_wear_bit(slot)))
		return 0;
	if (d.class_mask && !(d.class_mask & class_use_bits())) {
		HUD_init_message("%s", game_text(Dtu_inventory[index].type == OBJ_ARMOR ? 828 : 825, "You cannot use this object."));
		return 0;
	}
	old = dtu_char_slot_of(index);
	if (old >= 0)
		Dtu_char.slot[old].n = -1;
	Dtu_char.slot[slot].list = 0;
	Dtu_char.slot[slot].n = index;
	dtu_char_update();
	dtu_char_log("equip: entry %d '%s' in slot %d, armor class %d", index, d.name, slot, Dtu_char.pa.armor_class);
	return 1;
}

int dtu_char_equip_auto(int index)
{
	dtu_item_data d;
	int slot, first = -1;

	if (!entry_data(index, &d))
		return 0;
	if (Dtu_inventory[index].type == OBJ_WEAPON || (!d.wear_mask && d.hot_key_ok)) {
		dtu_char_set_current(index);
		return Dtu_char.current.n == index;
	}
	for (slot = 0; slot < DTU_N_SLOTS; slot++)
		if (d.wear_mask & slot_wear_bit(slot)) {
			if (first < 0) first = slot;
			if (Dtu_char.slot[slot].n < 0)
				return dtu_char_equip(index, slot);
		}
	return first >= 0 ? dtu_char_equip(index, first) : 0;
}

void dtu_char_unequip(int slot)
{
	if (slot < 0 || slot >= DTU_N_SLOTS || Dtu_char.slot[slot].n < 0)
		return;
	dtu_char_log("unequip: slot %d", slot);
	Dtu_char.slot[slot].n = -1;
	dtu_char_update();
}

void dtu_char_set_current(int index)
{
	dtu_item_data d;

	if (index < 0) {
		Dtu_char.current.n = -1;
		dtu_spell_put_away();
		dtu_char_update();
		return;
	}
	if (!entry_data(index, &d) || !d.hot_key_ok)
		return;
	if (Dtu_inventory[index].type == OBJ_WEAPON && d.class_mask && !(d.class_mask & class_use_bits())) {
		HUD_init_message("%s", game_text(825, "You cannot use this object."));		// guess, see dtu_char_equip()
		return;
	}
	// The executable keeps one reference (P+0x1AD7): an item in the hand
	// takes the place of a spell in the hand.
	dtu_spell_put_away();
	Dtu_char.current.list = 0;
	Dtu_char.current.n = index;
	dtu_char_update();
	dtu_char_log("current item: entry %d '%s'", index, d.name);
}

void dtu_char_set_hot_key(int key, int index)
{
	dtu_item_data d;

	if (key < 0 || key >= DTU_N_HOT_KEYS)
		return;
	if (index < 0) {
		Dtu_char.hot_key[key].list = 0;
		Dtu_char.hot_key[key].n = -1;
		return;
	}
	if (!entry_data(index, &d) || !d.hot_key_ok)
		return;
	Dtu_char.hot_key[key].list = 0;
	Dtu_char.hot_key[key].n = index;
}

// The spell of a hot key box (list 1 mage, 2 cleric), or -1
int dtu_char_hot_key_spell(int key)
{
	if (key < 0 || key >= DTU_N_HOT_KEYS)
		return -1;
	return dtu_spell_of_ref(Dtu_char.hot_key[key].list, Dtu_char.hot_key[key].n);
}

// The entry of the item list of a hot key box, or -1
int dtu_char_hot_key_item(int key)
{
	if (key < 0 || key >= DTU_N_HOT_KEYS || !ref_entry(Dtu_char.hot_key[key]))
		return -1;
	return Dtu_char.hot_key[key].n;
}

// 1 if the box holds the reference of the hand (P+0x1AD7): the border of
// 0x329CB. The port keeps a spell in the hand in dtuspell.c.
int dtu_char_hot_key_in_hand(int key)
{
	int spell = dtu_char_hot_key_spell(key);

	if (spell >= 0)
		return spell == Dtu_spell_current;
	return Dtu_spell_current < 0 && dtu_char_hot_key_item(key) >= 0 && Dtu_char.hot_key[key].n == Dtu_char.current.n;
}

// 0x35462: 1 if a hot key box holds the spell
static int spell_in_hot_key(int spell)
{
	int i;

	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		if (dtu_char_hot_key_spell(i) == spell)
			return 1;
	return 0;
}

// 0x304D6 (a click on a hot key box with a held spell): the box takes the
// spell if the character can use it (0x3533C; 0x31752 takes all spells)
// and no box holds it yet (0x35462). Returns 1 if the box took it.
int dtu_char_set_hot_key_spell(int key, int spell)
{
	int list, index;

	if (key < 0 || key >= DTU_N_HOT_KEYS || !dtu_spell_book_ref(spell, &list, &index))
		return 0;
	if (!dtu_spell_usable(spell) || spell_in_hot_key(spell))
		return 0;
	Dtu_char.hot_key[key].list = (ubyte) list;
	Dtu_char.hot_key[key].n = (byte) index;
	// "Selected spell %s as weapon" (0x123CEC) is a log text: the spell in
	// the hand does not change.
	dtu_char_log("hot key %d: spell %d '%s'", key, spell, dtu_spell_name(spell));
	return 1;
}

static int Last_hot_key = -1;			// 0x13AD15: the box that was chosen last

// 0x43A5A with a box: the item or the spell of the box goes into the hand.
// A spell needs no test here: the attack key makes the tests of 0x41A4D.
void dtu_char_select_hot_key(int key)
{
	int spell = dtu_char_hot_key_spell(key);

	if (spell >= 0) {
		if (dtu_spell_take(spell))
			Last_hot_key = key;
		return;
	}
	if (key < 0 || key >= DTU_N_HOT_KEYS || !ref_entry(Dtu_char.hot_key[key]))
		return;
	dtu_char_set_current(Dtu_char.hot_key[key].n);
	Last_hot_key = key;
}

// The refusal of 0x43B0D: sound 3000 (0xBB8) with the volume 1.0 (0x2D955)
#define SOUND_REFUSE	3000

static void refuse(int key, char *why)
{
	digi_play_sample(SOUND_REFUSE, F1_0);
	dtu_char_log("hot key %d refused: %s (sound %d)", key, why, SOUND_REFUSE);
}

// A flag of the weapon of an entry of the item list (Weapon_item_info
// +0x3E: "arrow" bit 4, "bolt" bit 8), 0 for an entry that is no weapon
static int weapon_flag(int n, char *flag)
{
	if (n < 0 || n >= Dtu_n_inventory || Dtu_inventory[n].type != OBJ_WEAPON || Dtu_inventory[n].id >= N_weapon_types)
		return 0;
	return bm_tbl_has_flag(Weapon_item_info[Dtu_inventory[n].id].record, flag);
}

// 0x43A5A with a box from a key 1 to 0, a click on a box or Cycle Weapon,
// with its tests in this order:
// - nothing in the attack states 6 and 11 of the arms (0x43ADC);
// - the attack key was pressed and its attack or cast has not started
//   (0x13B69F, the port's Request of dtucombat.c, which stays set while
//   the cast time of a spell runs): sound 3000 (0x43AF7). The executable
//   skips the sound when 0x13D2C8 is 2 (not traced);
// - an empty box: the empty hand, the last box becomes -1 (0x43CBD);
// - a box whose entry 0x35863 does not find: sound 3000 (0x43B7D);
// - arrows without the bow or bolts without the crossbow: "You need a %s to
//   fire %ss" and sound 3000 (0x43B85 to 0x43CB4);
// - the box that was chosen last (0x13AD15): sound 3000 (0x43D32).
void dtu_char_press_hot_key(int key)
{
	int s = dtu_combat_view_state(), n;

	if (key < 0 || key >= DTU_N_HOT_KEYS)
		return;
	if (s == DAS_ATTACK || s == DAS_ATTACK3)
		return;
	if (dtu_combat_attack_pending()) {
		refuse(key, "an attack or a cast waits");
		return;
	}
	if (Dtu_char.hot_key[key].n < 0) {
		dtu_char_log("hot key %d: empty box, the empty hand", key);
		dtu_char_set_current(-1);
		Last_hot_key = -1;
		return;
	}
	n = dtu_char_hot_key_item(key);
	if (n < 0 && dtu_char_hot_key_spell(key) < 0) {
		refuse(key, "no entry");
		return;
	}
	if (n >= 0) {
		int arrow = weapon_flag(n, "arrow"), bolt = weapon_flag(n, "bolt");

		if ((arrow && Dtu_char.slot[DTU_SLOT_BOW].n < 0) || (!arrow && bolt && Dtu_char.slot[DTU_SLOT_CROSSBOW].n < 0)) {
			// 0x1258C0 with "bow" and "arrow" or "crossbow" and "bolt"
			HUD_init_message("You need a %s to fire %ss", arrow ? LT("bow") : LT("crossbow"), arrow ? LT("arrow") : LT("bolt"));
			refuse(key, "no launcher");
			return;
		}
	}
	if (key == Last_hot_key) {
		refuse(key, "the box that was chosen last");
		return;
	}
	dtu_char_select_hot_key(key);
}

int dtu_char_last_hot_key(void)
{
	return Last_hot_key;
}

void dtu_char_forget_hot_key(void)
{
	Last_hot_key = -1;
}

// 0x35CE5 (the held spell is put on the figure of the paper doll, 0x35ADF):
// a spell that the character can use and that is in no box goes into the
// first free box, or into the box that was chosen last (box 0 if none),
// and into the hand. Returns 1 if a box took it.
int dtu_char_arm_spell(int spell)
{
	int list, index, key;

	if (!dtu_spell_book_ref(spell, &list, &index) || !dtu_spell_usable(spell) || spell_in_hot_key(spell))
		return 0;
	for (key = 0; key < DTU_N_HOT_KEYS; key++)
		if (Dtu_char.hot_key[key].n < 0)
			break;
	if (key >= DTU_N_HOT_KEYS)
		key = (Last_hot_key >= 0 && Last_hot_key < DTU_N_HOT_KEYS) ? Last_hot_key : 0;
	Dtu_char.hot_key[key].list = (ubyte) list;
	Dtu_char.hot_key[key].n = (byte) index;
	dtu_char_log("hot key %d: spell %d '%s' (paper doll)", key, spell, dtu_spell_name(spell));
	// 0x35E79: 0x13AD15 = -100, then 0x43A5A(box) with its tests
	Last_hot_key = -100;
	dtu_char_press_hot_key(key);
	return 1;
}

// 0x3533C for an entry of the item list. A spell (a scroll in the list):
// the tests of dtu_spell_usable() (39, 40 and 53 always, else 0x359F4 and
// the class bits). The Scroll (misc 102) always. Else the count is 1 or
// more, or -1, and the class bits of byte 3 of the entry fit a class of
// the character (0x352F7; no class bit: no class fits).
int dtu_char_item_usable(int index)
{
	dtu_inv_item *e;
	dtu_item_data d;

	if (index < 0 || index >= Dtu_n_inventory)
		return 0;
	e = &Dtu_inventory[index];
	if (e->type == OBJ_SPELL)
		return dtu_spell_usable(e->id) && (e->count >= 1 || e->count == -1);
	if (e->type == OBJ_MISC && e->id == 102)
		return 1;
	if (e->count < 1 && e->count != -1)
		return 0;
	if (!entry_data(index, &d))
		return 0;
	return (d.class_mask & 0xff & class_use_bits()) != 0;
}

// 0x35CE5 with a held weapon or spell of the item list (list 0). Returns
// 0 if the cursor keeps it.
static int arm_item(int index)
{
	int key;

	// 0x35D0A: 0x35462, a box holds the reference: the cursor keeps it
	for (key = 0; key < DTU_N_HOT_KEYS; key++)
		if (Dtu_char.hot_key[key].list == 0 && Dtu_char.hot_key[key].n == index)
			return 0;
	// 0x35D71: the first empty box, else the box that was chosen last
	// (0x13AD15), else box 0
	for (key = 0; key < DTU_N_HOT_KEYS; key++)
		if (Dtu_char.hot_key[key].n < 0)
			break;
	if (key >= DTU_N_HOT_KEYS)
		key = (Last_hot_key >= 0 && Last_hot_key < DTU_N_HOT_KEYS) ? Last_hot_key : 0;
	// 0x35DF5: arrows without a bow (0x35EDE, 0x35F9D) or bolts without a
	// crossbow (0x35F0F, 0x35F64): sound 3000 and the cursor lets go (no
	// message here)
	if ((weapon_flag(index, "arrow") && !ref_entry(Dtu_char.slot[DTU_SLOT_BOW])) ||
			(weapon_flag(index, "bolt") && !ref_entry(Dtu_char.slot[DTU_SLOT_CROSSBOW]))) {
		refuse(key, "paper doll: no launcher");
		return 1;
	}
	Dtu_char.hot_key[key].list = 0;
	Dtu_char.hot_key[key].n = (byte) index;
	dtu_char_log("hot key %d: entry %d '%s' (paper doll)", key, index, Dtu_inventory[index].name);
	// 0x35E79: 0x13AD15 = -100, then 0x43A5A(box) with its tests
	Last_hot_key = -100;
	dtu_char_press_hot_key(key);
	return 1;
}

// 0x35ADF, the press action of the figure of the paper doll, with a held
// entry of the item list. The wear bits of the held entry are those of
// 0x317EA, or 0 if 0x3533C fails (0x30344 and 0x312C3 at the pick up).
// The first bit in this order gives the slot: cloak, necklace, armor,
// gauntlet, shield, ring (ring 1 if it is empty, else ring 2), belt, boot,
// bow, crossbow, lockpicks. The slot takes the entry (an item in it goes
// back to the list) and 0xAFB55 runs. The helmet bit has no slot. With no
// slot: a weapon or a spell that 0x3533C passes goes into a hot key box
// and into the hand (0x35CE5); anything else is used (0x35E9C: 0x37A6A).
int dtu_char_put_on_doll(int index)
{
	static const int order[][2] = {
		{ DTU_WEAR_CLOAK, DTU_SLOT_CLOAK }, { DTU_WEAR_NECKLACE, DTU_SLOT_NECKLACE },
		{ DTU_WEAR_ARMOR, DTU_SLOT_ARMOR }, { DTU_WEAR_GAUNTLET, DTU_SLOT_GAUNTLET },
		{ DTU_WEAR_SHIELD, DTU_SLOT_SHIELD }, { DTU_WEAR_RING, DTU_SLOT_RING1 },
		{ DTU_WEAR_BELT, DTU_SLOT_BELT }, { DTU_WEAR_BOOT, DTU_SLOT_BOOT },
		{ DTU_WEAR_BOW, DTU_SLOT_BOW }, { DTU_WEAR_CROSSBOW, DTU_SLOT_CROSSBOW },
		{ DTU_WEAR_LOCKPICK, DTU_SLOT_LOCKPICK },
	};
	int usable, wear, i, type;

	if (index < 0 || index >= Dtu_n_inventory)
		return 1;
	usable = dtu_char_item_usable(index);
	wear = usable ? dtu_char_wear_mask(index) : 0;
	for (i = 0; i < (int) (sizeof(order) / sizeof(order[0])); i++)
		if (wear & order[i][0]) {
			int slot = order[i][1];
			// 0x35BF4: ring 1 if its reference is empty, else ring 2
			if (slot == DTU_SLOT_RING1 && Dtu_char.slot[DTU_SLOT_RING1].n >= 0)
				slot = DTU_SLOT_RING2;
			dtu_char_log("paper doll: entry %d '%s' to slot %d", index, Dtu_inventory[index].name, slot);
			dtu_char_equip(index, slot);
			return 1;
		}
	type = Dtu_inventory[index].type;
	if ((type == OBJ_WEAPON || type == OBJ_SPELL) && usable)
		return arm_item(index);
	dtu_char_log("paper doll: entry %d '%s' is used", index, Dtu_inventory[index].name);
	dtu_char_use(index);
	return 1;
}

int dtu_char_has(int type, int id)
{
	return find_entry(type, id) >= 0;
}

// 0xB6D24: takes the items out of all entries of the type.
void dtu_char_take(int type, int id, int count)
{
	int i;

	while (count > 0 && (i = find_entry(type, id)) >= 0) {
		int n = Dtu_inventory[i].count;

		if (n > count) n = count;
		if (n < 1) n = 1;
		dtu_char_log("take: %d of entry %d '%s'", n, i, Dtu_inventory[i].name);
		inv_remove(i, n);
		count -= n;
	}
}

//------------------------------------------------------------------------
// Hit points. The game counts them in the shields of the Descent player
// (P+0x53 of the executable is the same field).

int dtu_char_hit_points(void)
{
	fix s = Players[Player_num].shields;

	return s > 0 ? f2i(s) : 0;
}

int dtu_char_max_hit_points(void)
{
	return Dtu_char.created ? Dtu_char.pa.max_hit_points : 100;
}

static void set_hit_points(int hp)
{
	Players[Player_num].shields = i2f(hp);
	if (ConsoleObject && ConsoleObject->type == OBJ_PLAYER)
		ConsoleObject->shields = i2f(hp);
	Dtu_char.pa.hit_points = hp;
}

int dtu_char_heal(int points)
{
	int hp = dtu_char_hit_points(), max = dtu_char_max_hit_points(), n;

	if (points <= 0 || hp >= max)
		return 0;
	n = (hp + points > max) ? max - hp : points;
	set_hit_points(hp + n);
	return n;
}

void dtu_char_add_hit_points(int points)
{
	if (points > 0)
		set_hit_points(dtu_char_hit_points() + points);
}

int dtu_char_armor_class(void)
{
	return Dtu_char.created ? Dtu_char.pa.armor_class : 10;
}

int dtu_char_armor_class_vs(int missile)
{
	if (!Dtu_char.created)
		return 10;
	// the two values differ only with Shield
	if (!missile || !ConsoleObject || !(ConsoleObject->dtu.m_88 & DTU_FX_SHIELD))
		return Dtu_char.pa.armor_class;
	return Dtu_char.ac_missile;
}

int dtu_char_thac0(void)
{
	return Dtu_char.created ? Dtu_char.thac0 : 20;
}

int dtu_char_pickup_allowed(void)
{
	return GameTime >= Dtu_char.no_pickup_until;
}

//------------------------------------------------------------------------
// Use (0x37A6A). The executable has one case for each item number (the
// $OBJECT keywords pc_spell, duration and set_* are not read there).
// docs/re/combat.md, section B6.

// A choice box (0x655FE, nm_messagebox). Returns the index of the choice,
// -1 for Escape. The option -autochoice N answers N without a box. A run
// with no one to answer (-soak, -frames) takes the first choice.
int dtu_char_choice(char *text, int n, ...)
{
	va_list args;
	char *c[4] = { NULL, NULL, NULL, NULL };
	int i, r;

	va_start(args, n);
	for (i = 0; i < n && i < 4; i++)
		c[i] = va_arg(args, char *);
	va_end(args);
	if (Dtu_auto_choice >= 0) {
		r = (Dtu_auto_choice < n) ? Dtu_auto_choice : n - 1;
		dtu_char_log("choice box '%s': %d (option -autochoice)", text, r);
		return r;
	}
	if (FindArg("-soak") || FindArg("-frames")) {
		dtu_char_log("choice box '%s': 0 (a run with no one to answer)", text);
		return 0;
	}
	text = dtu_tr(text);		// ASCENT: the text of the code in the language (dtulang.h)
	switch (n) {
		case 1: r = nm_messagebox(NULL, 1, c[0], "%s", text); break;
		case 2: r = nm_messagebox(NULL, 2, c[0], c[1], "%s", text); break;
		case 3: r = nm_messagebox(NULL, 3, c[0], c[1], c[2], "%s", text); break;
		default: r = nm_messagebox(NULL, 4, c[0], c[1], c[2], c[3], "%s", text); break;
	}
	dtu_char_log("choice box '%s': %d", text, r);
	return r;
}

// The vclip of a spell on the player (0x8CAD3)
static void spell_vclip(int spell)
{
	if (ConsoleObject)
		dtu_spell_cast_vclip(spell, ConsoleObject);
}

static fix rnd_time(int mod, int add, int unit)
{
	return i2f((rand() % mod + add) * unit);
}

// 0xAE837: the item in the hand falls to the floor at the center of the
// segment of the player (0x3C93B, 0x13899) and leaves the inventory
// (0xB6D0C). "You dropped your weapon!"
void dtu_char_drop_weapon(void)
{
	dtu_inv_item *e = ref_entry(Dtu_char.current);
	object *p = ConsoleObject;
	vms_vector c;
	int type, id, objnum;

	if (!e || !p || p->type != OBJ_PLAYER)
		return;
	type = e->type;
	id = e->id;
	compute_segment_center(&c, &Segments[p->segnum]);
	objnum = dtu_create_item(type, id, 1, p->segnum, &c);
	if (objnum < 0) {
		HUD_init_message("You were disarmed!");
		dtu_char_log("drop weapon: no object, disarmed");
	}
	else {
		HUD_init_message("You dropped your weapon!");
		dtu_char_log("drop weapon: type %d id %d '%s' is object %d in segment %d", type, id, e->name, objnum, p->segnum);
	}
	dtu_char_take(type, id, 1);
}

// 0x396B2: the end of the potion timers. force: all end (Dispel Magic, the
// Potion of Anti-Magic).
void dtu_char_potions_end(int force)
{
	int changed = 0;

	// ASCENT: the texts of 0x396B2 begin with "Potion of" (0x1246C1 to
	// 0x124735)
	if (Potion.intel_end && (force || GameTime > Potion.intel_end)) {
		Potion.intel_end = 0;
		// ASCENT: 0x396B2 sets the intelligence of word 0x187BB6 (the value
		// without items) again; the port kept the potion value
		Dtu_char.pa.intelligence = Dtu_char.base.intelligence;
		HUD_init_message("Potion of Hill Giant Intelligence expired.");
		changed = 1;
	}
	else if (Potion.intel_end && !force) {
		// each call, 1 in 63: parts of the map are forgotten (0x185FEC).
		// The executable rolls in each frame; corrected: one roll for each
		// reference frame (Dtu_fix_potion_frame).
		int n = 1, s;

		if (Dtu_fix_potion_frame) {
			Potion.intel_acc += FrameTime;
			n = Potion.intel_acc / (3 * F1_0 / 64);
			Potion.intel_acc -= n * (3 * F1_0 / 64);
		}
		while (n-- > 0)
			if (rand() % 63 == 0) {
				int step = 16 + rand() % 17;
				for (s = rand() % 49; s <= Highest_segment_index; s += step)
					Automap_visited[s] = 0;
				dtu_char_log("potion: the intelligence potion makes the player forget parts of the map");
			}
	}
	if (Potion.str_end && (force || GameTime > Potion.str_end)) {
		Potion.str_end = 0;
		HUD_init_message("Potion of Hill Giant Strength expired.");
		changed = 1;
	}
	if (Potion.dragon_end && (force || GameTime > Potion.dragon_end)) {
		Potion.dragon_end = 0;
		HUD_init_message("Potion of Dragon's Blood expired.");
		changed = 1;
	}
	if (Potion.dex_end && (force || GameTime > Potion.dex_end)) {
		Potion.dex_end = 0;
		HUD_init_message("Potion of Agility expired.");
		changed = 1;
	}
	if (Potion.luck_end && (force || GameTime > Potion.luck_end)) {
		Potion.luck_end = 0;
		dtu_script_set_gflag(0x4D, 0);		// ASCENT: 0x39913, global flag 77
		HUD_init_message("Your luck just ran out.");
		changed = 1;
	}
	if (changed) {
		dtu_char_log("potion: a potion effect ends%s", force ? " (force)" : "");
		dtu_char_update();
	}
}

// ASCENT: 0xB6FE3, called first by 0xB7084 (use one): a stack of a
// stackable type above its stack_limit drops to the limit. The executable
// also makes a count below 1 into 1, so an endless stack (-1) goes at its
// first use; the port keeps the endless stack (a defect, no setting: no
// shipped item has one).
static void clamp_stacks(void)
{
	dtu_item_data d;
	int i;

	for (i = 0; i < Dtu_n_inventory; i++)
		if (dtu_item_get(Dtu_inventory[i].type, Dtu_inventory[i].id, &d) && d.stackable &&
				!(Dtu_inventory[i].type == OBJ_MISC && Dtu_inventory[i].id == 102) && Dtu_inventory[i].count > d.stack_limit) {
			dtu_char_log("use: entry %d '%s' count %d cut to the stack limit %d (0xB6FE3)", i, Dtu_inventory[i].name,
				Dtu_inventory[i].count, d.stack_limit);
			Dtu_inventory[i].count = d.stack_limit;
		}
}

static int any_potion(void)
{
	return Potion.intel_end || Potion.str_end || Potion.dragon_end || Potion.dex_end || Potion.luck_end;
}

// ASCENT: no text of the effect. Each potion shows its own text of 0x37A6A.
static void effect_on_player(int mask, fix duration)
{
	if (ConsoleObject)
		dtu_spell_set_effect(ConsoleObject, mask, duration, 1, 0);
}

static void spell_on_player(int spell)
{
	if (ConsoleObject)
		dtu_spell_cast_on(spell, ConsoleObject, ConsoleObject);
}

// 0x2FACC: the system shock of constitution 1 to 19
static const ubyte System_shock[19] = { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 88, 90, 95, 97, 99, 99 };

// ASCENT: the values of the cases of use_misc (0x37A6A) as a table, so that
// the description window (dtu_char_misc_use, dtudesc.c) reads what the use
// does. Times: (rand() % t_mod + t_add) * t_unit seconds, no roll for
// t_mod 1. The Gem of Curing has its spell in lo, the Spider Piece 0x16 its
// area spell in lo.
static const dtu_misc_use Misc_use[] = {
	{ 0x02, DTU_MU_GEM, 11, 11 },
	{ 0x12, DTU_MU_TURN },
	{ 0x13, DTU_MU_MAP },
	{ 0x14, DTU_MU_ATTACK },
	{ 0x16, DTU_MU_AREA_SPELLS, 14, 14, 0, 0, 0, 0, 7, { 1, 35, 19, 40, 43, 18, 50 } },
	{ 0x17, DTU_MU_TOUR },
	{ 0x18, DTU_MU_ABILITY },
	{ 0x19, DTU_MU_SPELLS_KEEP, 0, 0, 0, 0, 0, 0, 4, { 48, 20, 21, 25 } },
	{ 0x1A, DTU_MU_FULL_HEAL, 0, 0, 0, 0, 0, 0, 9, { 1, 45, 15, 24, 35, 38, 39, 43, 49 } },
	{ 0x48, DTU_MU_DEXTERITY, 18, 18, 1, 10, 60 },
	{ 0x49, DTU_MU_DISPEL },
	{ 0x4A, DTU_MU_DRAGON, 18, 18 },
	{ 0x4B, DTU_MU_HEAL, 6, 27 },
	{ 0x4C, DTU_MU_EFFECT, 0, 0, 1, 10, 60, DTU_FX_RESIST_FIRE },
	{ 0x4D, DTU_MU_SPELLS, 0, 0, 0, 0, 0, 0, 1, { 19 } },
	{ 0x4E, DTU_MU_EFFECT, 0, 0, 4, 5, 600, DTU_FX_FREE_ACTION },
	{ 0x4F, DTU_MU_HEAL, 4, 10 },
	{ 0x50, DTU_MU_GIANT, 19, 19, 4, 5, 600 },
	{ 0x51, DTU_MU_INTEL, 5, 7, 4, 5, 600 },
	{ 0x52, DTU_MU_EFFECT, 0, 0, 8, 1, 600, DTU_FX_INFRAVISION },
	{ 0x53, DTU_MU_EFFECT, 0, 0, 4, 3, 600, DTU_FX_INVISIBLE },
	{ 0x54, DTU_MU_EFFECT, 0, 0, 16, 5, 60, DTU_FX_INVULNERABLE },
	{ 0x55, DTU_MU_SPELLS, 0, 0, 0, 0, 0, 0, 1, { 27 } },
	{ 0x56, DTU_MU_LUCK, 0, 0, 1, 60, 60 },
	{ 0x57, DTU_MU_EFFECT, 0, 0, 16, 5, 60, DTU_FX_HASTE },
	{ 0x58, DTU_MU_TOUGH, 2, 18, 6, 7, 600 },
	{ 0x65, DTU_MU_QUEST },
	{ 0x66, DTU_MU_SCROLL },
	{ 0x6A, DTU_MU_CURSED, 0, 0, 0, 0, 0, 0, 5, { 7, 9, 44, 22, 16 } },
	{ 0x7F, DTU_MU_SCROLL },
};

const dtu_misc_use *dtu_char_misc_use(int id)
{
	int i;

	for (i = 0; i < (int) (sizeof(Misc_use) / sizeof(Misc_use[0])); i++)
		if (Misc_use[i].id == id)
			return &Misc_use[i];
	return NULL;
}

// ASCENT: two values of use_misc that the description window shows too
// (dtudesc.c). 0x38BE9: the Potion of Dragon's Blood lasts 1 to this many
// minutes (the level of class 1, from 1).
int dtu_char_dragon_minutes(void)
{
	return Dtu_char.pa.class1_level + 1;
}

// 0x38196: the Spider Piece 0x12 makes class 1 a cleric of this level
// number (0 is the first level) for the Turn Undead call.
int dtu_char_spider_turn_level(void)
{
	return (Dtu_char.pa.class1_level + 1) * 2;
}

// The value of a use: rand() % (hi - lo + 1) + lo
static int use_roll(const dtu_misc_use *u)
{
	return rand() % (u->hi - u->lo + 1) + u->lo;
}

static fix use_time(const dtu_misc_use *u)
{
	return u->t_mod > 1 ? rnd_time(u->t_mod, u->t_add, u->t_unit) : i2f(u->t_add * u->t_unit);
}

// The misc items. Returns 1 if the item is used up (the cases that go to
// 0x39694), 0 if not, -1 if the item has no use.
static int use_misc(dtu_inv_item *e)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	object *p = ConsoleObject;
	int hp = dtu_char_hit_points(), max = dtu_char_max_hit_points(), n, i;
	const dtu_misc_use *u = dtu_char_misc_use(e->id);

	if (!u)
		return -1;
	switch (e->id) {
		case 0x02:		// Gem of Curing: not used up
			// 0x3891B: the use counter P+0x1A57 must not be above the
			// mission number MsnNum 0x13E588 (0x8445A): 0 for a new game
			// (0x82BC7), the next mission at Khelben (0x83E5E). So one use
			// at the start and one more for each mission. 0x389E0 adds 1.
			if (Potion.gem_uses > Fe_khelben_mission) {
				HUD_init_message("You cannot use the Gem Of Curing at this time.");
				dtu_char_log("use: Gem of Curing refused, %d uses, mission %d", Potion.gem_uses, Fe_khelben_mission);
				return 0;
			}
			if (p->dtu.m_88 & DTU_FX_BLIND) {
				dtu_spell_clear_effect(p, DTU_FX_BLIND);
				HUD_init_message("...And now you can see!");
			}
			else if (hp < max)
				spell_on_player(u->lo);
			Potion.gem_uses++;
			dtu_char_log("use: Gem of Curing, use %d", Potion.gem_uses);
			return 0;
		case 0x13:		// Spider Piece: the whole map is known
			if (e->count > 0) {
				for (i = 0; i <= Highest_segment_index; i++)
					Automap_visited[i] = 1;
				e->count--;
				dtu_char_log("use: Spider Piece 0x13, the map is known, %d charges", e->count);
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x16:		// Spider Piece: area Fear and seven spells on the player
			if (e->count > 0) {
				dtu_spell_area(u->lo, &p->pos, p->segnum, p, dtu_spell_player_level(u->lo));
				for (i = 0; i < u->n_spells; i++)
					spell_on_player(u->spells[i]);
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x19:		// Assembled Spider: four effects with no end
			if (e->count > 0) {
				static const int m[4] = { DTU_FX_SPIDER_CLIMB, DTU_FX_FREE_ACTION, DTU_FX_HASTE, DTU_FX_INFRAVISION };
				for (i = 0; i < u->n_spells && i < 4; i++) {
					spell_on_player(u->spells[i]);
					if (p->dtu.m_88 & m[i])
						dtu_spell_set_effect(p, m[i], DTU_FX_NO_END, 0, 0);
				}
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x1A:		// Spider Piece: all hit points and nine spells
			if (e->count > 0) {
				dtu_char_heal(max);
				for (i = 0; i < u->n_spells; i++)
					spell_on_player(u->spells[i]);
				if (p->dtu.m_88 & (DTU_FX_BLIND|DTU_FX_DEAF))
					spell_on_player(42);
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x12:		// Spider Piece: turn undead (0xADC46)
			// ASCENT: 0x38196 makes class 1 a cleric of level (class 1
			// level + 1) * 2 (0 is the first level) for the call, so the turn
			// is at that level + 1; with a class 2 cleric or the cloak 45 the
			// level of 0xADC91 and 0xADCB3. The port had level 2.
			if (e->count > 0) {
				int c1 = pa->class1, l1 = pa->class1_level;

				pa->class1 = DTU_CLASS_CLERIC;
				pa->class1_level = dtu_char_spider_turn_level();
				dtu_combat_turn_undead(0);
				pa->class1 = c1;
				pa->class1_level = l1;
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x18:		// Spider Piece: script 0x1FC (0x38463), the cast vclip of spell 41
			if (e->count > 0) {
				dtu_script_run_id(0x1FC);		// 0xAC055(0, 0x1FC): one point more of an ability
				e->count--;
				spell_vclip(41);
				dtu_char_log("use: Spider Piece 0x18, script 0x1FC, %d charges", e->count);
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x14:		// Spider Piece: the attack on the people in sight (0x385D3)
		case 0x17:		// Spider Piece: the view visits each creature (0x38268)
			if (e->count > 0) {
				dtu_spider_tour_start(e->id);		// dtucombat.c
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x48:		// Potion of Agility: dexterity 18 for 600 s
			if (pa->dexterity < u->lo) {
				Potion.dex_end = GameTime + use_time(u);
				HUD_init_message("You have increased agility for %d minutes.", u->t_add * u->t_unit / 60);		// ASCENT: 0x38A6C
				spell_vclip(21);
			}
			break;
		case 0x49:		// Potion of Anti-Magic: all effects end
			if (p->dtu.m_88 || any_potion()) {
				dtu_spell_dispel(p);
				dtu_char_potions_end(1);
			}
			spell_vclip(13);
			break;
		case 0x4A:		// Potion of Dragon's Blood: strength 18 for 1 to (level + 1) rounds
			if (pa->strength < u->lo) {
				n = rand() % dtu_char_dragon_minutes() + 1;
				Potion.dragon_end = GameTime + i2f(n * 60);
				Potion.dragon_extra = has_class(DTU_CLASS_FIGHTER) ? rand() % 100 : 0;
				HUD_init_message("You have increased strength for %d minutes.", n);		// ASCENT: 0x38BE9
				spell_vclip(50);
			}
			break;
		case 0x4B:		// Potion of Extra-Healing: 6 to 27 hit points
			if (hp < max) {
				n = dtu_char_heal(use_roll(u));
				HUD_init_message("You feel much better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, hp, dtu_char_hit_points(), max);
			}
			spell_vclip(11);
			break;
		case 0x4C:		// Potion of Fire Resistance: Resist Fire for 600 s
			effect_on_player(u->fx, use_time(u));
			spell_vclip(43);
			HUD_init_message("You have Fire Resistance for %d minutes.", u->t_add * u->t_unit / 60);		// ASCENT: 0x38D75
			break;
		case 0x4D:		// Potion of Flying: the spell Fly
			spell_on_player(u->spells[0]);
			break;
		case 0x4E:		// Potion of Freedom: Free Action for 50 to 80 rounds
			effect_on_player(u->fx, use_time(u));
			HUD_init_message("You are immune to paralysis for awhile.");
			break;
		case 0x4F:		// Potion of Healing: 4 to 10 hit points
			if (hp < max) {
				n = dtu_char_heal(use_roll(u));
				HUD_init_message("You feel better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, hp, dtu_char_hit_points(), max);
			}
			spell_vclip(10);
			break;
		case 0x50:		// Potion of Hill Giant Strength: 19 for 50 to 80 rounds
			if (pa->strength < u->lo) {
				Potion.str_end = GameTime + use_time(u);
				HUD_init_message("You have Hill Giant strength for awhile.");
				spell_vclip(50);		// ASCENT: 0x38C09
			}
			break;
		case 0x51:		// Poshun uv Hil Gient Inteljuns: intelligence 5 to 7
			Potion.intel = use_roll(u);
			Potion.intel_end = GameTime + use_time(u);
			HUD_init_message("%s", game_text(977, "Yooz fielz mor smartr."));		// ASCENT: 0x39013, TEX 977
			// a random vclip and a flash for each point of intelligence: the
			// vclip of one spell here
			spell_vclip(7);
			break;
		case 0x52:		// Potion of Infravision: 10 to 80 rounds
			effect_on_player(u->fx, use_time(u));
			spell_vclip(25);
			HUD_init_message("%s", game_text(968, "You have infravision for some time."));		// ASCENT: 0x391A7, TEX 968
			break;
		case 0x53:		// the second "Potion of Agility": Invisibility for 30 to 60 rounds
			effect_on_player(u->fx, use_time(u));
			spell_vclip(26);
			HUD_init_message("%s", game_text(969, "You are invisible for now."));		// ASCENT: 0x3923E, TEX 969
			break;
		case 0x54:		// Potion of Invulnerability: 5 to 20 rounds
			effect_on_player(u->fx, use_time(u));
			spell_vclip(28);
			HUD_init_message("%s", game_text(970, "You presently feel invincible."));		// ASCENT: 0x39335, TEX 970
			break;
		case 0x55:		// Potion of Jumping: the spell Jump
			spell_on_player(u->spells[0]);
			break;
		case 0x56:		// Potion of Luck: one hour, one critical hit
			Potion.luck_end = GameTime + use_time(u);
			// ASCENT: the vclips of the spells 1, 3 and 38 and global flag 77
			// (0x393A3 to 0x393D6). Script 508 of a Spider Piece reads the flag.
			spell_vclip(1);
			spell_vclip(3);
			spell_vclip(38);
			dtu_script_set_gflag(0x4D, 1);
			HUD_init_message("You feel lucky today.");
			break;
		case 0x57:		// Potion of Speed: Haste for 5 to 20 rounds
			effect_on_player(u->fx, use_time(u));
			spell_vclip(21);
			HUD_init_message("%s", game_text(972, "You feel faster."));		// ASCENT: 0x3944C, TEX 972
			break;
		case 0x58:		// Potion of Toughening
			if (max < hp) {
				int roll = rand() % 100 + 1, c = pa->constitution;

				if (c < 1) c = 1;
				if (c > 19) c = 19;
				HUD_init_message("You don't feel so good.");
				dtu_char_log("use: Potion of Toughening again: system shock d100 %d against %d", roll, System_shock[c - 1]);
				if (roll > System_shock[c - 1])
					dtu_damage_player(p, NULL, i2f(hp + 1));
			}
			else {
				n = use_roll(u);
				dtu_spell_set_effect(p, DTU_FX_BLESS, use_time(u), 0, 0);
				set_hit_points(hp + n);
				HUD_init_message("You feel tougher.");
				dtu_char_log("use: Potion of Toughening: %d hit points, %d of %d", n, dtu_char_hit_points(), max);
			}
			break;
		case 0x65:		// Orb of Insight: script event 0x1FE (dtuscript.c)
			dtu_char_log("use: Orb of Insight, script event 0x1FE");
			dtu_hook_script_event(0x1FE);
			return 0;
		case 0x66:		// Scroll: 0x9DC42 shows the scroll text of the count
		case 0x7F:		// thief scroll: the text 1 (0x37DE0)
			// The port counts the Scroll of count 0 (the mission summary)
			// as 1 (dtu_char_load_save, the new character); no level has
			// a Scroll of count 1 (they have 2 to 27).
			n = e->count;
			if (e->has_record) {
				dtu_item_record rec;
				memcpy(&rec, e->record, sizeof(rec));
				n = rec.count;
			} else if (n == 1)
				n = 0;
			if (e->id == 0x7F)
				n = 1;
			dtu_char_log("use: '%s' shows the scroll text %d", e->name, n);
			dtu_scroll_show(n);		// dtuscroll.c
			return 0;
		case 0x6A:		// Cursed Scroll
			{
				ushort *a[6] = { &Dtu_char.base.strength, &Dtu_char.base.dexterity, &Dtu_char.base.constitution,
					&Dtu_char.base.intelligence, &Dtu_char.base.wisdom, &Dtu_char.base.charisma };

				for (i = 0; i < u->n_spells; i++)
					if (rand() % 2)
						spell_on_player(u->spells[i]);
				if (rand() % 2)
					for (i = 0; i <= Highest_segment_index; i++)
						Automap_visited[i] = 0;
				for (i = 0; i < 6; i++)
					if (rand() % 2 && *a[i] > 0)
						*a[i] = rand() % *a[i] + 1;
				Dtu_char.pa.intelligence = Dtu_char.base.intelligence;
				Dtu_char.pa.wisdom = Dtu_char.base.wisdom;
				Dtu_char.pa.charisma = Dtu_char.base.charisma;
				dtu_char_update();
				hp = dtu_char_hit_points();
				// ASCENT: 0x38159 divides rand() by the hit points as a fix:
				// below half a point. Dtu_fix_cursed_scroll: rand() % hit points
				// as whole points.
				if (hp > 0)
					dtu_damage_player(p, NULL, Dtu_fix_cursed_scroll ? i2f(rand() % hp) : (fix) (rand() % i2f(hp)));
				HUD_init_message("You no longer think that reading the scroll was such a good idea...");
			}
			// ASCENT: 0x3817F jumps past the use up 0x39694: the scroll stays
			return 0;
		default:
			return -1;
	}
	return 1;
}

int dtu_char_use(int index)
{
	dtu_inv_item *e;
	int r;

	if (index < 0 || index >= Dtu_n_inventory || !ConsoleObject)
		return 0;
	e = &Dtu_inventory[index];
	if (Dtu_player_dead)
		return 0;

	dtu_char_log("use: entry %d type %d id %d '%s' count %d", index, e->type, e->id, e->name, e->count);
	if (e->type == OBJ_WEAPON) {
		// weapon 13, the item in the hand (0x37B58). ASCENT: 0x37B69 to
		// 0x37BC7: global flag 0x86 = one of the effects 0xC004124, global
		// flag 0x91 = 1, script 0x1FD; then without global flag 0xA0 and for
		// a Lawful Good character Continual Light, Free Action and
		// Protection From Evil. The port returned at the effects and set no
		// flag.
		if (e->id != 13 || Dtu_char.current.n != index)
			return 0;
		dtu_script_set_gflag(0x86, (ConsoleObject->dtu.m_88 & 0xC004124) != 0);
		dtu_script_set_gflag(0x91, 1);
		dtu_hook_script_event(0x1FD);
		if (!dtu_script_gflag(0xA0) && Dtu_char.pa.alignment == 0) {
			dtu_spell_area(8, &ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject, dtu_spell_player_level(8));
			spell_on_player(20);
			spell_on_player(53);
		}
		return 0;
	}
	if (e->type == OBJ_ARMOR) {
		if (e->id == 45)		// armor 45 (Cloak of Elvenkind): area Continual Light
			dtu_spell_area(LATHANDER_SPELL, &ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject, dtu_spell_player_level(LATHANDER_SPELL));
		return 0;
	}
	if (e->type != OBJ_MISC)
		return 0;

	// ASCENT: Dtu_fix_potion_class. The table fixup of 1.3 (0x1A344) makes
	// the Potion of Hill Giant Strength (80) and the Potion of
	// Invulnerability (84) items of the fighter, as the rulebook has them,
	// and the inventory marks them for the other classes, but the use code
	// (0x37A6A) has no class test.
	if (Dtu_fix_potion_class && (e->id == 80 || e->id == 84) && !dtu_char_item_usable(index)) {
		HUD_init_message("%s", game_text(825, "You cannot use this object."));
		return 0;
	}

	r = use_misc(e);
	if (r < 0) {
		dtu_char_log("use: '%s' (misc %d) has no use", e->name, e->id);
		return 0;
	}
	if (r == 0) {
		dtu_char_update();
		return 0;
	}
	// 0x39694: 0xB7084, one item of the entry is used up
	dtu_char_update();
	clamp_stacks();
	inv_remove(index, 1);
	return 1;
}

// 0x3614B and 0x2FAF0: the item in the hand is used if it is a misc item.
// 0xB7084: one item of a stack, or one charge, is used. A count of -1 has
// no limit. At 0 the entry goes away and the references follow the list.
void dtu_char_use_up(int index)
{
	clamp_stacks();
	if (index < 0 || index >= Dtu_n_inventory || Dtu_inventory[index].count < 0)
		return;
	inv_remove(index, 1);
}

int dtu_char_use_current(void)
{
	dtu_inv_item *e = ref_entry(Dtu_char.current);

	if (!e || e->type != OBJ_MISC)
		return 0;
	return dtu_char_use(Dtu_char.current.n);
}

//------------------------------------------------------------------------
// Drop (0xB717B)

int dtu_char_drop(int index)
{
	dtu_inv_item *e;
	item_info *it;
	object *player = ConsoleObject, *obj;
	vms_vector pos, down;
	fvi_query fq;
	fvi_info hit;
	fix size;
	int seg = -1, objnum, d;
	static int dist[4] = { 15, 10, 6, 0 };

	if (index < 0 || index >= Dtu_n_inventory || !player || player->type != OBJ_PLAYER)
		return -1;
	e = &Dtu_inventory[index];
	it = item_entry(e->type, e->id);
	// 0xB715A: no drop with 700 objects or more
	if (!it || Highest_object_index >= 700) {
		HUD_init_message("%s", game_text(784, "You can't drop that."));
		return -1;
	}
	// ASCENT: 0xB611C: no drop of an item whose table line has plot_item
	// (Weapon_info +0x52, Armor_info +0x24, Misc_info +0x26, Spell_info
	// +0x26)
	if (rec_int(it->record, "plot_item", 0) != 0) {
		dtu_char_log("drop: entry %d '%s' is a plot item (0xB611C)", index, e->name);
		HUD_init_message("%s", game_text(784, "You can't drop that."));
		return -1;
	}
	// ASCENT: 0xB7197 to 0xB7230, weapon 13 (the talking sword): global
	// flag 0x8A at each try; d18 above intelligence or d7 above the class 1
	// level (from 1): flag 0x90, script 0x1FD, no drop.
	if (e->type == OBJ_WEAPON && e->id == 13) {
		int r1 = rand() % 18 + 1, r2 = 0;

		dtu_script_set_gflag(0x8A, 1);
		if (r1 <= Dtu_char.pa.intelligence)
			r2 = rand() % 7 + 1;
		if (r1 > Dtu_char.pa.intelligence || r2 > Dtu_char.pa.class1_level + 1) {
			dtu_char_log("drop: weapon 13 refuses, d18 %d, d7 %d", r1, r2);
			dtu_script_set_gflag(0x90, 1);
			dtu_hook_script_event(0x1FD);
			return -1;
		}
	}
	size = rec_fix(it->record, "size", i2f(5));

	// The executable makes the object with the function 0x3C93B, which was
	// not read. The place in front of the player is a guess: far enough
	// that the player does not touch the item.
	for (d = 0; d < 4 && seg < 0; d++) {
		vm_vec_scale_add(&pos, &player->pos, &player->orient.fvec, i2f(dist[d]));
		seg = find_point_seg(&pos, player->segnum);
	}
	if (seg < 0) {
		pos = player->pos;
		seg = player->segnum;
	}
	// down to the floor
	down = pos;
	down.y -= i2f(200);
	fq.p0 = &pos;
	fq.p1 = &down;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = -1;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq, &hit) == HIT_WALL && hit.hit_seg >= 0) {
		pos = hit.hit_pnt;
		pos.y += size;
		seg = find_point_seg(&pos, hit.hit_seg);
		if (seg < 0) {
			pos.y = hit.hit_pnt.y + F1_0 / 4;
			seg = hit.hit_seg;
		}
	}

	objnum = obj_create(e->type, e->id, seg, &pos, &vmd_identity_matrix, size, CT_NONE, MT_NONE, RT_ITEM);
	if (objnum < 0) {
		HUD_init_message("%s", game_text(784, "You can't drop that."));
		return -1;
	}
	obj = &Objects[objnum];
	obj->shields = F1_0;
	obj->lifeleft = IMMORTAL_TIME;
	obj->dtu.item_bitmap = it->obj_bitmap;
	obj->dtu.m_1d5 = (e->count > 0 && e->count < 128) ? e->count : 1;

	dtu_char_log("drop: entry %d '%s' count %d, object %d in segment %d", index, e->name, e->count, objnum, seg);
	// 0xB72BA: no pick up for 5 seconds
	Dtu_char.no_pickup_until = GameTime + i2f(5);
	digi_play_sample(0x8A3, F1_0);		// ASCENT: 0xB72E2, the sound of the drop
	inv_remove(index, e->count > 0 ? e->count : 1);
	return objnum;
}

//------------------------------------------------------------------------
// Rest (0x360D5 and fuelcen.c 0x3E137, 0x3E1F6). A rest place is a segment
// with the special value 2 or 6. It works again 300 seconds after a rest.

int dtu_char_rest(void)
{
	object *player = ConsoleObject;
	int seg, special;

	if (!Dtu_char.created || !player || player->type != OBJ_PLAYER || Dtu_player_dead)
		return 0;
	seg = player->segnum;
	special = Segments[seg].special;
	if ((special != 2 && special != 6) || Rest_time[seg] > GameTime) {
		HUD_init_message("You can not rest here");
		dtu_char_log("rest: not here (segment %d, special %d)", seg, special);
		return 0;
	}
	dtu_char_log("rest: segment %d, hit points %d -> %d", seg, dtu_char_hit_points(), dtu_char_max_hit_points());
	set_hit_points(Dtu_char.pa.max_hit_points);
	Rest_time[seg] = GameTime + i2f(300);
	HUD_init_message("You feel refreshed");
	digi_play_sample(62, F1_0 / 2);		// 0x3E309: sound 62 at half volume
	// 0x3E354: remove the fire of the rest place (0x3DC0F finds it,
	// "killing fireball %d", lifeleft 0, 0x68F45). fuelcen_update_all()
	// makes it again when the time of the place has passed.
	{
		int objnum;

		while ((objnum = fuelcen_rest_fire(seg)) >= 0) {
			dtu_char_log("rest: killing fireball %d", objnum);
			Objects[objnum].lifeleft = 0;
			obj_delete(objnum);
		}
	}
	dtu_spell_restore_slots();		// 0x36049 calls 0x3551B
	dtu_creature_alert(player, 5);	// ASCENT: 0x3E2C1, the alert function 0x160AB
	dtu_spell_rest_shuffle();		// 0x3E1F6 calls 0x92FF2
	// The executable also calls 0x4D95D(player) (not read). This is not in
	// the port.
	return 1;
}

int dtu_char_rest_ready(int segnum)
{
	if (segnum < 0 || segnum >= MAX_SEGMENTS)
		return 0;
	return Rest_time[segnum] <= GameTime;
}

//------------------------------------------------------------------------
// Experience and levels

// 0x37253
int dtu_char_next_level(int *points_to_next, int *percent_to_next)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int left = 0, span = 100, classes = 1, xp = Dtu_char.experience[0];

	if (valid_class(pa->class1) && pa->class1_level >= 0 && pa->class1_level <= 18) {
		left = Xp_table[pa->class1][pa->class1_level + 2] - Dtu_char.experience[0];
		span = Xp_table[pa->class1][pa->class1_level + 2] - Xp_table[pa->class1][pa->class1_level + 1];
	}
	if (valid_class(pa->class2) && pa->class2_level >= 0 && pa->class2_level <= 18) {
		int left2 = Xp_table[pa->class2][pa->class2_level + 2] - Dtu_char.experience[1];

		if (left > left2) {
			left = left2;
			span = Xp_table[pa->class2][pa->class2_level + 2] - Xp_table[pa->class2][pa->class2_level + 1];
		}
		xp += Dtu_char.experience[1];
		classes = 2;
	}
	if (span == 0) span = 100;
	if (points_to_next) *points_to_next = classes * left;
	if (percent_to_next) *percent_to_next = left * 100 / span;
	return xp;
}

// 0xAF43A: the levels that the experience gives. Returns 1 for a new level.
static int check_level(int class_num, int xp, short *level)
{
	int next = *level + 1, gained = 0;

	if (!valid_class(class_num))
		return 0;
	if (next > 18) {
		*level = 17;
		return 0;
	}
	while (next < 18 && xp >= Xp_table[class_num][next + 1]) {
		(*level)++;
		next++;
		gained = 1;
	}
	return gained;
}

// 0xAF694: the hit points of the levels old+1 to new of one class.
static int level_hit_points(int class_num, int old_level, int new_level)
{
	int con = Dtu_char.base.constitution, level, sum = 0;
	int die = Hit_die[class_num], half = die >> 1;

	if (con > 19) con = 19;
	for (level = old_level + 1; level <= new_level && level < 18; level++) {
		int n = 0;

		if (level <= Hit_dice_levels[class_num])
			n += rand() % (die - half + 1) + half;			// one die, at least half of it
		else
			n += Hit_fixed[class_num];
		n += Con_bonus[class_num == DTU_CLASS_FIGHTER][con];
		dtu_char_log("level: class %d level %d gives %d hit points", class_num, level + 1, n);
		sum += n;
	}
	return sum;
}

// 0xAF489
static void check_levels(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	static char *class_name[4] = { "Cleric", "Mage", "Fighter", "Thief" };
	int old1 = pa->class1_level, old2 = pa->class2_level;
	int new1, new2, hp = 0;

	new1 = check_level(pa->class1, Dtu_char.experience[0], &pa->class1_level);
	new2 = check_level(pa->class2, Dtu_char.experience[1], &pa->class2_level);
	Dtu_char.base.class1_level = pa->class1_level;
	Dtu_char.base.class2_level = pa->class2_level;
	if (!new1 && !new2)
		return;

	// 0xAF560: a box with the button "Ok" (TEX 34) for each class that
	// gained a level, in the order cleric, mage, fighter, thief; text TEX
	// 844 with the class name (TEX 642, 643, 644, 647)
	{
		int i, mask = (new1 ? 1 << pa->class1 : 0) | (new2 ? 1 << pa->class2 : 0);

		for (i = 0; i < 4; i++)
			if (mask & (1 << i)) {
				char text[160];

				sprintf(text, game_text(844, "Your character has attained\nnew %s levels!"),
					game_text(642 + (i == 3 ? 5 : i), class_name[i]));
				dtu_char_choice(text, 1, game_text(34, "Ok"));
			}
	}
	if (new1)
		hp += level_hit_points(pa->class1, old1, pa->class1_level);
	if (new2)
		hp += level_hit_points(pa->class2, old2, pa->class2_level);
	// 0xAF86F: one half for a character with two classes
	if (valid_class(pa->class2))
		hp >>= 1;
	dtu_char_log("level: class1 level %d -> %d, class2 level %d -> %d, %d hit points", old1 + 1, pa->class1_level + 1,
		old2 + 1, pa->class2_level + 1, hp);
	pa->max_hit_points += hp;
	Dtu_char.base.max_hit_points += hp;
	Dtu_char.base.hit_points += hp;
	if (Dtu_fix_level_hit_points)
		set_hit_points(dtu_char_hit_points() + hp);
	else {
		// 0xAF8C7, 0xAF8CD: the integer is added to the fixed point values
		Players[Player_num].shields += hp;
		if (ConsoleObject && ConsoleObject->type == OBJ_PLAYER)
			ConsoleObject->shields += hp;
		Dtu_char.pa.hit_points += hp;
	}
	// 0xAF8F5 (the end of 0xAF694): a cleric gets every cleric spell of the
	// spell levels of the cleric level (0xB75A8)
	if (has_class(DTU_CLASS_CLERIC))
		dtu_spell_grant_cleric();
	update_thac0();
}

// 0xAF3CF
void dtu_char_award_xp(int points)
{
	int classes;

	if (!Dtu_char.created || points == 0)
		return;
	classes = valid_class(Dtu_char.pa.class2) ? 2 : 1;
	points /= classes;
	if (points == 0)
		points = 1;
	Dtu_char.experience[0] += points;
	if (classes == 2)
		Dtu_char.experience[1] += points;
	dtu_char_log("experience: +%d for each class, now %d", points, Dtu_char.experience[0]);
	check_levels();
}

// 0x8BA3A(attributes, level). The level of both classes is level - 1 (one
// less again for two classes, at least 0), the experience is the start of
// that level (table 0x146048), the hit points are (hit die 0x13FDF3 +
// constitution 0x8C01B) * (level value + 1) for each class, the mean of
// the two (rounded toward 0), and THAC0 (0xAF605). The executable writes
// the maximum and the integer hit points of the record (P+0xA1, P+0xA3)
// and not the hit points in use (P+0x53). A thief gets 60 + 30 * level
// value skill points (P+0x1AB3); the test is the class mask of the front
// end (0x3A6465), which the port reads as the classes of the character.
void dtu_char_set_level(int level)
{
	static int die[4] = { 8, 4, 10, 6 };		// 0x13FDF3: cleric, mage, fighter, thief
	dtu_player_attr *pa = &Dtu_char.pa;
	int lv = level > 18 ? 18 : level, two = valid_class(pa->class2), hp, con = pa->constitution;

	if (!Dtu_char.created || !valid_class(pa->class1))
		return;
	lv--;
	if (two)
		lv--;
	if (lv < 0)
		lv = 0;
	if (con < 1) con = 1;
	if (con > 19) con = 19;
	pa->class1_level = pa->class2_level = (short) lv;
	Dtu_char.base.class1_level = Dtu_char.base.class2_level = (short) lv;
	Dtu_char.experience[0] = Xp_table[pa->class1][lv + 1];
	Dtu_char.experience[1] = two ? Xp_table[pa->class2][lv + 1] : 0;
	if (has_class(DTU_CLASS_THIEF))
		Dtu_char.thief_points = 60 + 30 * lv;
	hp = (die[pa->class1] + Con_bonus[pa->class1 == DTU_CLASS_FIGHTER][con]) * (lv + 1);
	if (two)
		hp = (hp + (die[pa->class2] + Con_bonus[pa->class2 == DTU_CLASS_FIGHTER][con]) * (lv + 1)) / 2;
	pa->max_hit_points = pa->hit_points = (short) hp;
	Dtu_char.base.max_hit_points = Dtu_char.base.hit_points = (short) hp;
	update_thac0();
	dtu_char_log("level: set to %d: class levels %d/%d, experience %d/%d, %d hit points (in use %d)", level, lv + 1,
		two ? lv + 1 : 0, Dtu_char.experience[0], Dtu_char.experience[1], hp, dtu_char_hit_points());
}

//------------------------------------------------------------------------
// Combat values

// 0x2A7B9 and 0x2B4BB: the column of the percent value of strength 18.
// The percent 0 is 18/00, the best column.
static int str18_index(int extra)
{
	return extra > 90 ? 3 : extra > 75 ? 2 : extra > 50 ? 1 : extra > 0 ? 0 : 4;
}

// 0x2A789: the attack bonus of the strength
static int str_hit_bonus(int str, int extra)
{
	int b;

	if (str < 1) str = 1;
	if (str > 19) str = 19;
	b = Str_hit[str];
	if (str == 18 && has_class(DTU_CLASS_FIGHTER))
		b += Str_extra_hit[str18_index(extra)];
	return b;
}

// ASCENT: 0x2A789 for the creature path of 0x2A7F3 (0x2ACA7, the percent
// 100) under -origcombat (Dtu_fix_creature_strength of dtuobj.c, D-003)
int dtu_char_str_hit_bonus(int str, int extra)
{
	return str_hit_bonus(str, extra);
}

// 0x2B494: the damage bonus of the strength
static int str_damage_bonus(int str, int extra)
{
	int b;

	if (str > 19) str = 19;
	b = Str_damage[str];
	if (str == 18 && has_class(DTU_CLASS_FIGHTER))
		b += Str_extra_damage[str18_index(extra)];
	return b;
}

// The launcher of a missile weapon (0x2AB0A, 0x2B0C1): the bow for a
// weapon with the flag arrow, else the crossbow. Its to_hit_bonus is added
// to the roll and to the damage.
static int launcher_bonus(int weapon)
{
	dtu_inv_item *l = ref_entry(Dtu_char.slot[bm_tbl_has_flag(Weapon_item_info[weapon].record, "arrow") ? DTU_SLOT_BOW : DTU_SLOT_CROSSBOW]);
	dtu_item_data ld;

	if (l && dtu_item_get(l->type, l->id, &ld))
		return ld.to_hit_bonus;
	return 0;
}

// The values of an attack with a weapon (-1: the empty hand)
static void weapon_attack(int weapon, dtu_attack *a)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	dtu_item_data d;

	memset(a, 0, sizeof(*a));
	a->weapon_id = -1;
	a->melee = 1;
	a->thac0 = dtu_char_thac0();
	a->wait = F1_0;
	if (!Dtu_char.created) {
		a->dice_n = 1;
		a->dice_sides = 2;
		return;
	}
	// ASCENT: 0x2AAE7 and 0x2B06A take each weapon number that is not
	// negative, with no test of the dice. The port sent a weapon with 0
	// sides to the empty hand: the Flame Tongue (weapon 71, the table fixup
	// 0x1B04A gives it 0d0 and 1d8 of fire) lost its +1, its fire and its
	// bonus against undead. The damage roll adds nothing for 0 sides.
	if (weapon >= 0 && dtu_item_get(OBJ_WEAPON, weapon, &d)) {
		// 0x2AAE7 and 0x2B06A: the weapon
		a->weapon_id = weapon;
		a->melee = d.melee;
		a->to_hit_bonus = d.to_hit_bonus;
		a->dice_n = d.dice_n;
		a->dice_sides = d.dice_sides;
		a->damage_bonus = d.damage_bonus;
		a->wait = d.attack_wait;
		if (d.melee) {
			a->to_hit_bonus += str_hit_bonus(pa->strength, pa->strength_extra);
			a->damage_bonus += str_damage_bonus(pa->strength, pa->strength_extra);
		}
		else {
			// 0x2AB00 and 0x2B0B7: a missile gets the to_hit_bonus of the
			// bow or the crossbow, for the attack and for the damage.
			a->to_hit_bonus += launcher_bonus(weapon);
			a->damage_bonus += launcher_bonus(weapon);
		}
	}
	else {
		// 0x2AC13 and 0x2AF75: the empty hand
		a->to_hit_bonus = pa->to_hit_bonus + str_hit_bonus(pa->strength, pa->strength_extra);
		a->dice_n = pa->n_damage_dice;
		a->dice_sides = pa->damage_dice;
		a->damage_bonus = pa->damage_bonus;
	}
}

// The weapon of the item in the hand, or -1
static int hand_weapon_id(void)
{
	dtu_inv_item *e = ref_entry(Dtu_char.current);

	return (e && e->type == OBJ_WEAPON) ? e->id : -1;
}

void dtu_char_get_attack(dtu_attack *a)
{
	weapon_attack(hand_weapon_id(), a);
}

// 0x2A87B and 0x2B107: the Potion of Luck works when the day clock (raw
// fix) is odd. Then the luck ends (0xAE564: sound 0x8A8). The bit depends
// on the frame times (at a fixed frame rate with an even frame time it is
// never odd); corrected: a coin flip.
static int luck_now(void)
{
	if (!Potion.luck_end || !(Dtu_fix_luck_coin ? (rand() & 1) : (GameTime & 1)))
		return 0;
	Potion.luck_end = 0;
	digi_play_sample(0x8A8, F1_0);
	dtu_script_set_gflag(0x4D, 0);		// ASCENT: 0xAE58E, global flag 77
	return 1;
}

int dtu_char_attack_roll(object *target, int weapon)
{
	dtu_attack a;
	int roll = rand() % 20 + 1, result, ac = 0;

	weapon_attack(weapon, &a);
	// 0x2A87B: the Potion of Luck makes the roll a natural 20
	if (luck_now()) {
		roll = 20;
		dtu_char_log("attack roll: the Potion of Luck gives a natural 20");
	}
	Dtu_last_attack_roll = roll;
	if (roll == 20) {
		// 0x2A8B3: a critical hit, sound 3001
		HUD_init_message("You made a critical hit!");
		digi_play_sample(3001, F1_0);
		result = 2;
	}
	else if (roll == 1) {
		int r2 = rand() % 20 + 1;

		// 0x2A8EC: a critical fumble, sound 3001
		HUD_init_message("You made a critical fumble!");
		digi_play_sample(3001, F1_0);
		if (r2 > Dtu_char.pa.dexterity) {
			// 0x2A93F: a second d20 above the dexterity: the weapon falls
			dtu_char_log("attack roll: fumble, d20 %d above dexterity %d: the weapon falls", r2, Dtu_char.pa.dexterity);
			dtu_char_drop_weapon();
		}
		else {
			// 0x2A949: one of three texts; the arms go to state 10 and the
			// attack request ends
			static const char *what[3] = { LN("You lost your balance!"), LN("Your weapon was entangled!"), LN("The wind was knocked out of you!") };
			int n = rand() % 3;

			dtu_combat_fumble();
			HUD_init_message("%s", LT(what[n]));
			dtu_char_log("attack roll: fumble, d20 %d not above dexterity %d: %s", r2, Dtu_char.pa.dexterity, what[n]);
		}
		result = -1;
	}
	else {
		int r = roll + a.to_hit_bonus;

		// 0x2AB66: a backstab adds 4 (melee and thrown weapons)
		if (Dtu_backstab && a.melee)
			r += 4;
		// 0x2AB9F: Flame Tongue (71) adds 2 against a regenerating target and
		// 4 against an undead creature
		if (a.weapon_id == 71 && target) {
			if (target->dtu.m_88 & DTU_FX_REGENERATE)
				r += 2;
			if (target->type == OBJ_ROBOT && target->id < N_robot_types && (Monster_info[target->id].flags & MONSTER_F_UNDEAD))
				r += 4;
		}
		// ASCENT: a weapon missile (arrow, bolt, thrown weapon) meets the
		// armor class against missiles (Dtu_fix_shield_ac)
		result = target ? dtu_hit_test_vs(target, ConsoleObject, r, a.thac0, &ac, dtu_hit_by_missile()) : (a.thac0 - 10 > r ? 0 : 1);
		dtu_char_log("attack roll: d20 %d, roll %d against THAC0 %d and armor class %d: %d%s", roll, r, a.thac0, ac, result,
			Dtu_backstab ? " (backstab)" : "");
		return result;
	}
	dtu_char_log("attack roll: d20 %d: %d", roll, result);
	return result;
}

int dtu_char_weapon_damage(int weapon, object *target)
{
	dtu_attack a;
	int i, sum = 0, fire = 0;

	weapon_attack(weapon, &a);
	for (i = 0; i < a.dice_n; i++)
		if (a.dice_sides > 0)
			sum += rand() % a.dice_sides + 1;
	if (a.weapon_id >= 0) {
		// 0x2B0AE: the bow or the crossbow of a missile
		if (!a.melee)
			sum += launcher_bonus(weapon);
		// 0x2B107: the Potion of Luck: the highest number of the dice
		if (luck_now()) {
			sum = a.dice_n * a.dice_sides;
			dtu_char_log("damage roll: the Potion of Luck gives the highest roll");
		}
		// 0x2B15B: a backstab multiplies by (thief level + 7) / 4 (level
		// from 1); weapon 45 with a hit roll of 19 or 20 adds 3 to 6
		if (Dtu_backstab) {
			int lvl = (Dtu_char.pa.class1 == DTU_CLASS_THIEF) ? Dtu_char.pa.class1_level : Dtu_char.pa.class2_level;
			int mult = (lvl + 1 + 7) >> 2;

			sum *= mult;
			if (a.weapon_id == 45 && Dtu_last_attack_roll >= 19) {
				sum += rand() % 4 + 3;
				HUD_init_message("Your backstab was very successful!");
			}
			else
				HUD_init_message("Your backstab was successful!");
			dtu_char_log("damage roll: backstab, times %d", mult);
		}
		// 0x2B1D6: the damage bonus of the weapon (the strength bonus of a
		// melee weapon is added last, 0x2B456)
		sum += a.damage_bonus - (a.melee ? str_damage_bonus(Dtu_char.pa.strength, Dtu_char.pa.strength_extra) : launcher_bonus(weapon));
		fire = dtu_weapon_extra_damage(ConsoleObject, target, weapon, &sum);
		if (a.melee)
			sum += str_damage_bonus(Dtu_char.pa.strength, Dtu_char.pa.strength_extra);
	}
	else
		sum += a.damage_bonus;		// 0x2AFB4: the empty hand
	dtu_char_log("damage roll: %dd%d + %d: %d, fire %d", a.dice_n, a.dice_sides, a.damage_bonus, sum, fire);
	sum += fire;
	return sum > 0 ? sum : 0;
}

// ASCENT: the weapon branch of 0x2AF5E does not test the attacker: the
// missile of a creature gets the bow or crossbow of the player (0x2B0AE)
// and the Potion of Luck of the player (0x2B107). Only with
// Dtu_fix_creature_missile 0.
int dtu_char_creature_missile_damage(int weapon, int n, int sides, int sum)
{
	dtu_item_data d;

	if (!dtu_item_get(OBJ_WEAPON, weapon, &d))
		return sum;
	if (!d.melee)
		sum += launcher_bonus(weapon);
	if (luck_now()) {
		sum = n * sides;
		dtu_char_log("damage roll: a creature missile uses the Potion of Luck of the player");
	}
	return sum;
}

int dtu_char_damage_roll(void)
{
	return dtu_char_weapon_damage(hand_weapon_id(), NULL);
}

static int d20_result(int roll, int bonus, int thac0, int ac)
{
	if (roll == 20) return 2;
	if (roll == 1) return -1;
	return (thac0 - ac > roll + bonus) ? 0 : 1;			// 0x2AE32
}

int dtu_char_defense_roll(int attacker_thac0, int to_hit_bonus)
{
	int roll = rand() % 20 + 1;
	int result = d20_result(roll, to_hit_bonus, attacker_thac0, dtu_char_armor_class());

	dtu_char_log("defense roll: d20 %d + %d against THAC0 %d and armor class %d: %d", roll, to_hit_bonus, attacker_thac0,
		dtu_char_armor_class(), result);
	return result;
}

//------------------------------------------------------------------------
// The default character and the save game

static void clear_character(void)
{
	int i;

	memset(&Dtu_char, 0, sizeof(Dtu_char));
	Have_loaded_record = 0;
	memset(&Potion, 0, sizeof(Potion));		// ASCENT: no potion of the game before a load
	Last_hot_key = -1;
	Dtu_char.current.n = -1;
	for (i = 0; i < DTU_N_HOT_KEYS; i++) { Dtu_char.hot_key[i].list = 0; Dtu_char.hot_key[i].n = -1; }
	for (i = 0; i < DTU_N_SLOTS; i++) Dtu_char.slot[i].n = -1;
	Dtu_char.thac0 = 20;
	dtu_inventory_clear();
	Known_n_inventory = 0;
}

static void give_item(int type, int id)
{
	dtu_item_data d;
	int count = 1;

	if (!dtu_item_get(type, id, &d) || find_entry(type, id) >= 0)		// 0x8BD39: not twice
		return;
	if (!d.stackable && d.uses > 0)
		count = d.uses;
	dtu_inventory_add(type, id, count);
}

// The values of a new character come from the front end, which is not in
// the port. Verified for this function: the items (0x8BB91), the dice of
// the empty hand, the first level 0, THAC0 from the table. The ability
// scores, the alignment, the age and the gold are a guess. The hit points
// are the full hit die plus the constitution bonus (the save game of a new
// thief and mage with constitution 16 has 7: (6 + 2 + 4 + 2) / 2).
void dtu_char_create_default(void)
{
	dtu_player_attr *pa;
	int i;

	read_options();
	clear_character();
	pa = &Dtu_char.pa;
	strncpy(Dtu_char.name, Players[Player_num].callsign, sizeof(Dtu_char.name) - 1);
	if (!Dtu_char.name[0])
		strcpy(Dtu_char.name, "Fighter");

	pa->class1 = DTU_CLASS_FIGHTER;
	pa->class2 = DTU_CLASS_NONE;
	pa->unk_02 = -1;
	pa->race = DTU_RACE_HUMAN;
	pa->age = 21;
	pa->strength = 17;
	pa->dexterity = 14;
	pa->constitution = 16;
	pa->intelligence = 10;
	pa->wisdom = 10;
	pa->charisma = 12;
	pa->alignment = 1;
	pa->unk_4e = -1;
	pa->gold = 100;
	pa->damage_dice = 2;
	pa->n_damage_dice = 1;
	pa->to_hit_bonus = Str_hit[pa->strength];
	pa->damage_bonus = Str_damage[pa->strength];
	pa->max_hit_points = Hit_die[DTU_CLASS_FIGHTER] + Con_bonus[1][pa->constitution];
	pa->hit_points = pa->max_hit_points;
	pa->armor_class = 10;
	Dtu_char.base = *pa;
	Dtu_char.created = 1;
	Dtu_gold = pa->gold;

	give_item(OBJ_MISC, 102);
	for (i = 0; i < 4; i++)
		if (Start_items[DTU_CLASS_FIGHTER][i][0])
			give_item(Start_items[DTU_CLASS_FIGHTER][i][0], Start_items[DTU_CLASS_FIGHTER][i][1]);
	sync_inventory();
	dtu_display_inventory_reset();
	dtu_char_update();
	dtu_char_log("new character '%s': fighter, %d hit points, armor class %d, THAC0 %d, %d items", Dtu_char.name,
		pa->max_hit_points, pa->armor_class, Dtu_char.thac0, Dtu_n_inventory);
}

static int get_s32(ubyte *p)
{
	return (int) ((uint) p[0] | ((uint) p[1] << 8) | ((uint) p[2] << 16) | ((uint) p[3] << 24));
}

static void put_s32(ubyte *p, int v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static void load_ref(dtu_item_ref *ref, ubyte *p, int *map, int n_map)
{
	ref->list = 0;
	ref->n = -1;
	if (p[0] == 0 && (byte) p[1] >= 0 && p[1] < n_map)
		ref->n = map[p[1]];
}

// The entries of a list of the record go into Dtu_inventory[] as they are
// (no stacking). map[i] is the index of entry i.
static int load_list(ubyte *p, int count_off, int list_off, int max, int *map)
{
	dtu_item_record rec;
	int n, i, e;

	n = get_s32(p + count_off);
	if (n < 0) n = 0;
	if (n > max) n = max;
	for (i = 0; i < n; i++) {
		memcpy(&rec, p + list_off + 16 * i, 16);
		// The Scroll (misc 102) has count 0 and a key item -1 in the
		// executable; the port counts them as 1.
		e = dtu_inventory_append(rec.type, rec.id, rec.count > 0 ? rec.count : 1);
		if (map)
			map[i] = e;
		if (e < 0) {
			dtu_char_log("save game: entry %d (type %d id %d) does not fit the inventory", i, rec.type, rec.id);
			continue;
		}
		Dtu_inventory[e].level = rec.level;
		Dtu_inventory[e].has_record = 1;
		memcpy(Dtu_inventory[e].record, &rec, 16);
	}
	return n;
}

// SG0.SG: the player record is at 141, the attributes without items at
// 7108 (docs/formats/misc.md).
int dtu_char_load_save(char *filename)
{
	FILE *f;
	ubyte *buf, *p;
	int size, n, i, spell, map[128];

	f = fopen(filename, "rb");
	if (!f)
		return 0;
	buf = malloc(8192);
	size = (int) fread(buf, 1, 8192, f);
	fclose(f);
	if (size < DTU_SG_PLAYER_OFFSET + DTU_PLAYER_RECORD_SIZE + (int) sizeof(dtu_player_attr) || memcmp(buf, "DGSS", 4)) {
		free(buf);
		return 0;
	}
	p = buf + DTU_SG_PLAYER_OFFSET;

	clear_character();
	memcpy(Loaded_record, p, DTU_PLAYER_RECORD_SIZE);
	Have_loaded_record = 1;
	memcpy(Dtu_char.name, p + DTU_P_NAME, 16);
	memcpy(&Dtu_char.pa, p + DTU_P_ATTR, sizeof(dtu_player_attr));
	memcpy(&Dtu_char.base, p + DTU_PLAYER_RECORD_SIZE, sizeof(dtu_player_attr));
	Dtu_char.experience[0] = get_s32(p + DTU_P_EXPERIENCE);
	Dtu_char.experience[1] = get_s32(p + DTU_P_EXPERIENCE + 4);
	Dtu_char.thac0 = p[DTU_P_THAC0];
	Dtu_char.portrait = get_s32(p + DTU_P_PORTRAIT);
	Dtu_char.pa.hit_points = get_s32(p + DTU_P_HIT_POINTS) >> 16;
	Potion.gem_uses = p[DTU_P_GEM_USES];
	// ASCENT: 0xBA911: the four potion timers of the save are seconds left.
	// The level start makes them end times (potion_timers_from_save).
	for (i = 0; i < 4; i++) {
		int s = get_s32(p + DTU_P_POTIONS + 4 * i);

		Potion.load_left[i] = (s > 0 && s < 0x8000) ? s : 0;
	}
	Potion.intel = Dtu_char.pa.intelligence;
	Potion.dragon_extra = Dtu_char.pa.strength_extra;
	Dtu_char.thief_paid = p[DTU_P_THIEF_PAID];
	Dtu_char.thief_points = p[DTU_P_THIEF_POINTS] | (p[DTU_P_THIEF_POINTS + 1] << 8);
	Dtu_char.created = 1;
	Dtu_gold = Dtu_char.pa.gold;

	// The item list (P+0x103), then the key item list (P+0x18D3): the port
	// keeps both in one list.
	for (i = 0; i < 128; i++) map[i] = -1;
	n = load_list(p, DTU_P_N_ITEMS, DTU_P_ITEMS, DTU_MAX_ITEM_ENTRIES, map);
	load_list(p, DTU_P_N_KEYS, DTU_P_KEYS, DTU_MAX_KEY_ENTRIES, NULL);
	for (i = 0; i < Dtu_n_inventory; i++)
		entry_name(i);
	Known_n_inventory = Dtu_n_inventory;
	// The spell books and the spell slots (dtuspell.c)
	dtu_spell_books_from_record(p);
	Spell_repair_pending = 1;
	load_ref(&Dtu_char.current, p + DTU_P_CURRENT, map, n);
	for (i = 0; i < DTU_N_HOT_KEYS; i++) {
		ubyte *h = p + DTU_P_HOT_KEYS + 2 * i;

		load_ref(&Dtu_char.hot_key[i], h, map, n);
		// A box with a spell: list 1 (mage) or 2 (cleric) and the index in
		// that list (0x35863)
		if (dtu_spell_of_ref(h[0], (byte) h[1]) >= 0) {
			Dtu_char.hot_key[i].list = h[0];
			Dtu_char.hot_key[i].n = (byte) h[1];
		}
	}
	for (i = 0; i < 10; i++)
		load_ref(&Dtu_char.slot[i], p + DTU_P_SLOTS + 2 * i, map, n);
	load_ref(&Dtu_char.slot[DTU_SLOT_BOW], p + DTU_P_BOW, map, n);
	load_ref(&Dtu_char.slot[DTU_SLOT_CROSSBOW], p + DTU_P_CROSSBOW, map, n);
	// A spell in the hand (list 1 or 2)
	spell = dtu_spell_of_ref(p[DTU_P_CURRENT], (byte) p[DTU_P_CURRENT + 1]);
	if (spell >= 0)
		dtu_spell_select(spell);
	free(buf);
	dtu_display_inventory_reset();		// 0xBB867

	dtu_char_update();
	dtu_char_log("character '%s' from %s: class %d/%d level %d/%d, %d of %d hit points, armor class %d, THAC0 %d, %d items, %d mage and %d cleric spells",
		Dtu_char.name, filename, Dtu_char.pa.class1, Dtu_char.pa.class2, Dtu_char.pa.class1_level + 1, Dtu_char.pa.class2_level + 1,
		Dtu_char.pa.hit_points, Dtu_char.pa.max_hit_points, Dtu_char.pa.armor_class, Dtu_char.thac0, Dtu_n_inventory,
		Dtu_spell_book_n[0], Dtu_spell_book_n[1]);
	for (i = 0; i < Dtu_n_inventory; i++)
		dtu_char_log("  item %d: type %d id %d count %d level %d '%s'", i, Dtu_inventory[i].type, Dtu_inventory[i].id,
			Dtu_inventory[i].count, Dtu_inventory[i].level, Dtu_inventory[i].name);
	return 1;
}

// The index of an entry of Dtu_inventory[] in the item list of the record
// (the key items are not in that list), or -1.
static int record_index(int index)
{
	int i, n = 0;

	if (index < 0 || index >= Dtu_n_inventory || DTU_IS_KEY_ITEM(Dtu_inventory[index].type, Dtu_inventory[index].id))
		return -1;
	for (i = 0; i < index; i++)
		if (!DTU_IS_KEY_ITEM(Dtu_inventory[i].type, Dtu_inventory[i].id))
			n++;
	return n;
}

static void pack_ref(ubyte *p, dtu_item_ref ref)
{
	int n = ref.list == 0 ? record_index(ref.n) : -1;

	p[0] = 0;
	p[1] = (ubyte) n;
}

// One entry of a list: the record of the save game if the entry has one,
// else the values of the table (0xB65BB).
static void pack_entry(ubyte *e, int index)
{
	dtu_inv_item *it = &Dtu_inventory[index];
	dtu_item_record rec;
	dtu_item_data d;

	memset(&rec, 0, sizeof(rec));
	if (it->has_record && it->record[0] == it->type && it->record[1] == it->id)
		memcpy(&rec, it->record, 16);
	else {
		rec.letter = '_';
		if (entry_data(index, &d)) {
			rec.icon = d.icon;
			rec.class_mask = d.class_mask;
		}
		rec.modifier = F1_0;
	}
	rec.type = it->type;
	rec.id = it->id;
	rec.level = it->level;
	// the count 0 of the Scroll and -1 of a key item stay
	if (!(it->has_record && it->count == 1 && rec.count <= 0))
		rec.count = it->count;
	memcpy(e, &rec, 16);
}

static void pack_empty_list(ubyte *list, int n)
{
	dtu_item_record rec;
	int i;

	memset(&rec, 0, sizeof(rec));
	rec.letter = '_';						// an empty entry of the executable
	rec.level = 1;
	for (i = 0; i < n; i++)
		memcpy(list + 16 * i, &rec, 16);
}

void dtu_char_pack(ubyte *record)
{
	int i, n_items = 0, n_keys = 0, list, index;

	if (Have_loaded_record)
		memcpy(record, Loaded_record, DTU_PLAYER_RECORD_SIZE);
	else
		memset(record, 0, DTU_PLAYER_RECORD_SIZE);
	memset(record + DTU_P_NAME, 0, 16);
	strncpy((char *) record + DTU_P_NAME, Dtu_char.name, 15);
	memset(record + DTU_P_NAME2, 0, 16);
	strncpy((char *) record + DTU_P_NAME2, Dtu_char.name, 15);
	put_s32(record + DTU_P_HIT_POINTS, Players[Player_num].shields);
	put_s32(record + DTU_P_EXPERIENCE, Dtu_char.experience[0]);
	put_s32(record + DTU_P_EXPERIENCE + 4, Dtu_char.experience[1]);
	Dtu_char.pa.gold = Dtu_gold;
	memcpy(record + DTU_P_ATTR, &Dtu_char.pa, sizeof(dtu_player_attr));

	// The item list and the key item list
	pack_empty_list(record + DTU_P_ITEMS, DTU_MAX_ITEM_ENTRIES);
	pack_empty_list(record + DTU_P_KEYS, DTU_MAX_KEY_ENTRIES);
	for (i = 0; i < Dtu_n_inventory; i++) {
		if (DTU_IS_KEY_ITEM(Dtu_inventory[i].type, Dtu_inventory[i].id)) {
			if (n_keys < DTU_MAX_KEY_ENTRIES)
				pack_entry(record + DTU_P_KEYS + 16 * n_keys++, i);
		} else if (n_items < DTU_MAX_ITEM_ENTRIES)
			pack_entry(record + DTU_P_ITEMS + 16 * n_items++, i);
	}
	put_s32(record + DTU_P_N_ITEMS, n_items);
	put_s32(record + DTU_P_N_KEYS, n_keys);
	// The spell books and the spell slots (dtuspell.c)
	dtu_spell_books_to_record(record);

	record[DTU_P_THAC0] = Dtu_char.thac0;
	record[DTU_P_GEM_USES] = (ubyte) Potion.gem_uses;
	// ASCENT: 0xBA87E: the four potion timers as whole seconds left, 0 for
	// none or past
	{
		fix t[4];
		int s;

		t[0] = Potion.intel_end; t[1] = Potion.str_end; t[2] = Potion.dragon_end; t[3] = Potion.dex_end;
		for (i = 0; i < 4; i++) {
			s = (t[i] && t[i] > GameTime) ? f2i(t[i] - GameTime) : 0;
			if (!t[i] && Potion.load_left[i])
				s = Potion.load_left[i];
			put_s32(record + DTU_P_POTIONS + 4 * i, s);
		}
	}
	record[DTU_P_THIEF_PAID] = (ubyte) Dtu_char.thief_paid;
	record[DTU_P_THIEF_POINTS] = (ubyte) Dtu_char.thief_points;
	record[DTU_P_THIEF_POINTS + 1] = (ubyte) (Dtu_char.thief_points >> 8);
	pack_ref(record + DTU_P_CURRENT, Dtu_char.current);
	// A spell in the hand is the reference of the hand (list 1 or 2)
	if (Dtu_spell_current >= 0 && dtu_spell_book_ref(Dtu_spell_current, &list, &index)) {
		record[DTU_P_CURRENT] = (ubyte) list;
		record[DTU_P_CURRENT + 1] = (ubyte) index;
	}
	for (i = 0; i < DTU_N_HOT_KEYS; i++) {
		ubyte *h = record + DTU_P_HOT_KEYS + 2 * i;
		int spell = dtu_char_hot_key_spell(i);

		pack_ref(h, Dtu_char.hot_key[i]);
		// A box with a spell: the list and the index that
		// dtu_spell_books_to_record() gave the spell
		if (spell >= 0 && dtu_spell_book_ref(spell, &list, &index)) {
			h[0] = (ubyte) list;
			h[1] = (ubyte) index;
		}
	}
	for (i = 0; i < 10; i++)
		pack_ref(record + DTU_P_SLOTS + 2 * i, Dtu_char.slot[i]);
	pack_ref(record + DTU_P_BOW, Dtu_char.slot[DTU_SLOT_BOW]);
	pack_ref(record + DTU_P_CROSSBOW, Dtu_char.slot[DTU_SLOT_CROSSBOW]);
	put_s32(record + DTU_P_PORTRAIT, Dtu_char.portrait);
	// The model of the player (P+0x1AED): race, sex, class and "d_p.pof"
	if (!record[DTU_P_MODEL] && Dtu_player_archive[0])
		snprintf((char *) record + DTU_P_MODEL, 13, "%.3sd_p.pof", Dtu_player_archive);
}

//------------------------------------------------------------------------
// The hooks of the triggers (weak in dtutrig.c)

// 0x942ED with 0xB6BB3
int dtu_hook_player_has_item(int player_num, int item_class, int item_id)
{
	int has = (player_num == Player_num) && dtu_char_has(item_class, item_id);

	dtu_char_log("trigger: has item class %d id %d: %d", item_class, item_id, has);
	return has;
}

// 0xB6D0C
void dtu_hook_player_take_item(int item_class, int item_id)
{
	dtu_char_take(item_class, item_id, 1);
}

// 0x93D93: mask_b against the race (table 0x142230), mask_a against the
// classes (table 0x142220).
int dtu_hook_player_masks_ok(int player_num, int mask_a, int mask_b)
{
	static int race_bits[6] = { 1, 16, 4, 8, 32, 2 };
	static int class_bits[4] = { 4, 2, 1, 8 };
	dtu_player_attr *pa = &Dtu_char.pa;
	int race, ok;

	(void) player_num;
	if (!Dtu_char.created)
		return 1;
	race = (pa->flags & DTU_PF_HALF_ELF) ? 5 : pa->race;
	if (race < 0 || race > 5)
		race = 0;
	ok = (race_bits[race] & mask_b) != 0;
	if (ok)
		ok = (valid_class(pa->class1) && (class_bits[pa->class1] & mask_a))
			|| (valid_class(pa->class2) && (class_bits[pa->class2] & mask_a));
	if (!ok)
		dtu_char_log("trigger: masks 0x%x 0x%x do not fit the character", mask_a, mask_b);
	return ok;
}

//------------------------------------------------------------------------
// The test script of the option -chartest. The text is a list of
// "frame:command=value" with commas between. Commands: damage=N,
// heal=N, use=TYPE.ID, usecur, equip=TYPE.ID, unequip=SLOT, drop=TYPE.ID,
// current=TYPE.ID, hotkey=KEY, sethot=KEY.TYPE.ID, sethotspell=KEY.SPELL,
// armspell=SPELL, give=TYPE.ID, xp=N, rest, mission=N, race=N, search,
// gosecret=N, goside=SEG.SIDE, goobj=N, move=N, break=N, castby=SPELL.OBJ,
// spelltest=SPELL.LEVEL.OBJ.TRIALS, activate,
// str=STRENGTH.PERCENT,
// attackedby=OBJNUM, kill=OBJNUM,
// gorest (puts the player into the first rest place of the level), mode=N,
// gold=N, goseg=N (the center of segment N),
// priestcheck (the checks of the priest spells, dtu_spell_priest_check),
// dump, rules (the race and class test, rules_test()).

static void dump_character(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	dtu_attack a;
	int i;

	dtu_char_get_attack(&a);
	dtu_char_log("character '%s': class %d/%d level %d/%d race %d, hit points %d of %d, armor class %d, THAC0 %d, experience %d, gold %d",
		Dtu_char.name, pa->class1, pa->class2, pa->class1_level + 1, pa->class2_level + 1, pa->race,
		dtu_char_hit_points(), pa->max_hit_points, pa->armor_class, Dtu_char.thac0, Dtu_char.experience[0], Dtu_gold);
	dtu_char_log("  str %d/%d dex %d con %d int %d wis %d cha %d, attack: weapon %d, +%d to hit, %dd%d+%d, wait %.2f",
		pa->strength, pa->strength_extra, pa->dexterity, pa->constitution, pa->intelligence, pa->wisdom, pa->charisma,
		a.weapon_id, a.to_hit_bonus, a.dice_n, a.dice_sides, a.damage_bonus, f2fl(a.wait));
	dtu_char_log("  effects 0x%08x, worn 0x%04x, skills %d %d %d %d %d, potions %d %d %d %d %d, flag 77 %d",
		ConsoleObject ? ConsoleObject->dtu.m_88 : 0, Worn_bits, pa->hide_in_shadows, pa->move_silently, pa->open_locks,
		pa->find_traps, pa->climb_walls, Potion.intel_end != 0, Potion.str_end != 0, Potion.dragon_end != 0,
		Potion.dex_end != 0, Potion.luck_end != 0, dtu_script_gflag(0x4D));
	// ASCENT: the level and the difficulty, for the tests of a save and a load
	dtu_char_log("  dungeon level %d, difficulty %d, segment %d",
		Current_level_num, Difficulty_level, ConsoleObject ? ConsoleObject->segnum : -1);
	for (i = 0; i < Dtu_n_inventory; i++)
		dtu_char_log("  entry %d: type %d id %d '%s' count %d%s%s", i, Dtu_inventory[i].type, Dtu_inventory[i].id,
			Dtu_inventory[i].name, Dtu_inventory[i].count, Dtu_char.current.n == i ? " (in the hand)" : "",
			dtu_char_slot_of(i) >= 0 ? " (worn)" : "");
}

// The test "rules": every race and class combination of the character
// generation (0x13FECE) at the levels 1, 5 and 10, checked against the
// tables of DTUGAME.EXE 1.3 (typed again here from the executable, so a
// change of the tables of the port shows): the hit points (level 1 exact,
// later levels in the range of the dice of 0xAF694), THAC0 (0x146590, with
// Dtu_fix_multiclass_thac0), the five saving throws (0xAE596, tables at
// 0x1456D2) and the five thief skills (0x8BDE5). docs/audit/character.md.

int fe_make_test_character(int race, int mask, const int stats[6], int percentile);		// dtufe_char.c

static int Rules_checks, Rules_failures;

static void rules_check(int ok, char *what, int got, int lo, int hi, char *who)
{
	Rules_checks++;
	if (ok)
		return;
	Rules_failures++;
	dtu_char_log("rules: FAIL %s: %s %d, expected %d to %d", who, what, got, lo, hi);
}

static void rules_test(void)
{
	// the masks of the front end: 1 fighter, 2 thief, 4 mage, 8 cleric
	static const int combos[6] = { 139, 2495, 191, 191, 391, 135 };				// 0x13FECE
	static const char *race_name[6] = { "human", "half-elf", "elf", "drow", "dwarf", "halfling" };
	static const int fe_class[4] = { DTU_CLASS_FIGHTER, DTU_CLASS_THIEF, DTU_CLASS_MAGE, DTU_CLASS_CLERIC };
	// STR DEX CON INT WIS CHA, inside the limits of every race (0x8B71C)
	static const int stats[6] = { 15, 17, 16, 15, 15, 12 };
	// expected, from the executable: hit die 0x146518, constitution 16
	// (0x1402E2 / 0x140332: +2), a die up to level value 9 / 10 / 9 / 10,
	// then 2 / 1 / 3 / 2 (0x146528, 0x146538)
	static const int die[4] = { 8, 4, 10, 6 }, dice_to[4] = { 9, 10, 9, 10 }, fixed[4] = { 2, 1, 3, 2 };
	static const int con16 = 2;
	static const signed char thac0[4][18] = {										// 0x146590
		{ 20, 20, 20, 18, 18, 18, 16, 16, 16, 14, 14, 14, 12, 12, 12, 10, 10, 10 },
		{ 20, 20, 20, 19, 19, 19, 18, 18, 18, 17, 17, 17, 16, 16, 16, 15, 15, 15 },
		{ 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3 },
		{ 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13, 13, 12, 12 } };
	static const signed char saves[4][6][5] = {										// 0x1456D2
		{ {10,14,13,16,15}, {9,13,12,15,14}, {7,11,10,13,12}, {6,10,9,12,11} },
		{ {14,11,13,15,12}, {13,9,11,13,10}, {11,7,9,11,8} },
		{ {14,16,15,17,17}, {13,15,14,16,16}, {11,13,12,13,14}, {10,12,11,12,13}, {8,10,9,9,11}, {7,9,8,8,10} },
		{ {13,14,12,16,15}, {12,12,11,15,13}, {11,10,10,14,11} } };
	static const int save_div[4] = { 3, 5, 2, 4 };									// 0x1456E2
	// thief skills (0x8BDE5), race index 0 human, 1 dwarf, 2 elf, 3 drow,
	// 4 halfling, 5 half-elf; dexterity 17: +5, +5, +10, +0 (0x146078 ...)
	static const int race_hs[6] = { 0, 0, 10, 10, 15, 5 }, race_ms[6] = { 0, 0, 5, 5, 10, 0 };
	static const int race_ol[6] = { 0, 10, -5, -5, 5, 0 }, race_ft[6] = { 0, 15, 0, 0, 5, 0 };
	static const int race_cw[6] = { 0, -10, 0, 0, -15, 0 };
	static const int race_index[6] = { 0, 5, 2, 3, 1, 4 };		// front end race to the race index of the tables
	static const int levels[3] = { 1, 5, 10 };
	int race, mask, n_combos = 0;

	Rules_checks = Rules_failures = 0;
	for (race = 0; race < 6; race++)
		for (mask = 1; mask < 16; mask++) {
			int cls[2] = { -1, -1 }, n = 0, c, li, hp1, hp_lo, hp_hi;
			char who[64];
			dtu_player_attr *pa = &Dtu_char.pa;

			if (!(combos[race] & (1 << (mask - 1))))
				continue;
			for (c = 0; c < 4; c++)
				if ((mask & (1 << c)) && n < 2)
					cls[n++] = fe_class[c];
			if (!fe_make_test_character(race, mask, stats, 50)) {
				rules_check(0, "made", 0, 1, 1, (char *) race_name[race]);
				continue;
			}
			n_combos++;
			// level 1: the full die and the constitution, the mean of two
			hp1 = die[cls[0]] + con16;
			if (n == 2)
				hp1 = (hp1 + die[cls[1]] + con16) / 2;
			hp_lo = hp_hi = hp1;
			for (li = 0; li < 3; li++) {
				int target = levels[li], k, need = 0, lv[2] = { 0, 0 }, t, cat;
				int old[2] = { pa->class1_level, pa->class2_level };

				sprintf(who, "%s mask %d level %d", race_name[race], mask, target);
				// the experience that brings each class to the level at least
				for (k = 0; k < n; k++) {
					int x = Xp_table[cls[k]][target] - Dtu_char.experience[k];

					if (x > need) need = x;
				}
				if (need > 0)
					dtu_char_award_xp(need * n);
				// the levels that the experience gives (0xAF43A)
				for (k = 0; k < n; k++) {
					int v = 0;

					while (v < 17 && Dtu_char.experience[k] >= Xp_table[cls[k]][v + 2])
						v++;
					lv[k] = v;
				}
				rules_check(pa->class1_level == lv[0], "class 1 level value", pa->class1_level, lv[0], lv[0], who);
				if (n == 2)
					rules_check(pa->class2_level == lv[1], "class 2 level value", pa->class2_level, lv[1], lv[1], who);
				// hit points: the range of the dice of the new levels
				{
					int lo = 0, hi = 0;

					for (k = 0; k < n; k++) {
						int v;

						for (v = old[k] + 1; v <= lv[k]; v++)
							if (v <= dice_to[cls[k]]) {
								lo += die[cls[k]] / 2 + con16;
								hi += die[cls[k]] + con16;
							} else {
								lo += fixed[cls[k]] + con16;
								hi += fixed[cls[k]] + con16;
							}
					}
					if (n == 2) {
						lo >>= 1;
						hi >>= 1;
					}
					hp_lo += lo;
					hp_hi += hi;
					// the dice of this test are known only as a range: from
					// here on the range starts at the value of the port
					rules_check(pa->max_hit_points >= hp_lo && pa->max_hit_points <= hp_hi, "maximum hit points", pa->max_hit_points, hp_lo, hp_hi, who);
					hp_lo = hp_hi = pa->max_hit_points;
				}
				// THAC0
				t = thac0[cls[0]][lv[0] + 1];
				if (n == 2) {
					int t2 = thac0[cls[1]][lv[1] + 1];

					if (Dtu_fix_multiclass_thac0 ? t2 < t : t2 > t)
						t = t2;
				}
				rules_check(Dtu_char.thac0 == t, "THAC0", Dtu_char.thac0, t, t, who);
				// saving throws: the higher level, at most 12, the better of
				// the two classes
				for (cat = 0; cat < 5; cat++) {
					int level = (lv[0] > (n == 2 ? lv[1] : -1) ? lv[0] : lv[1]) + 1, s;

					if (level > 12) level = 12;
					s = saves[cls[0]][(level - 1) / save_div[cls[0]]][cat];
					if (n == 2) {
						int s2 = saves[cls[1]][(level - 1) / save_div[cls[1]]][cat];

						if (s2 < s) s = s2;
					}
					rules_check(dtu_spell_player_save_number(cat) == s, "saving throw", dtu_spell_player_save_number(cat), s, s, who);
				}
				// thief skills (they change only on the screen of Mirt)
				{
					int r = race_index[race], hs = 0, ms = 0, ol = 0, ft = 0, cw = 0;

					if (mask & 2) {
						hs = 5 + race_hs[r] + 5;
						ms = 10 + race_ms[r] + 5;
						ol = 10 + race_ol[r] + 10;
						ft = 5 + race_ft[r] + 0;
						cw = 60 + race_cw[r];
					} else if (race == 4)
						ft = 50;
					rules_check(pa->hide_in_shadows == hs, "hide in shadows", pa->hide_in_shadows, hs, hs, who);
					rules_check(pa->move_silently == ms, "move silently", pa->move_silently, ms, ms, who);
					rules_check(pa->open_locks == ol, "open locks", pa->open_locks, ol, ol, who);
					rules_check(pa->find_traps == ft, "find traps", pa->find_traps, ft, ft, who);
					rules_check(pa->climb_walls == cw, "climb walls", pa->climb_walls, cw, cw, who);
					if (li == 0)
						rules_check(Dtu_char.thief_points == ((mask & 2) ? 60 : 0), "thief points", Dtu_char.thief_points,
							(mask & 2) ? 60 : 0, (mask & 2) ? 60 : 0, who);
				}
				dtu_char_log("rules: %s: levels %d/%d, hit points %d, THAC0 %d, saves %d %d %d %d %d, thief %d %d %d %d %d", who,
					pa->class1_level + 1, n == 2 ? pa->class2_level + 1 : 0, pa->max_hit_points, Dtu_char.thac0,
					dtu_spell_player_save_number(0), dtu_spell_player_save_number(1), dtu_spell_player_save_number(2),
					dtu_spell_player_save_number(3), dtu_spell_player_save_number(4), pa->hide_in_shadows, pa->move_silently,
					pa->open_locks, pa->find_traps, pa->climb_walls);
			}
		}
	dtu_char_log("rules: %d combinations, %d checks, %d failures", n_combos, Rules_checks, Rules_failures);
	dtu_log("Character rules test: %d combinations, %d checks, %d failures\n", n_combos, Rules_checks, Rules_failures);
}
// dtufe.c: a save and a load with no menu (tests)
void dtufe_test_save(int slot);
void dtufe_test_load(int slot);

static void run_command(char *cmd, char *value)
{
	int a = 0, b = 0, index;

	if (value)
		sscanf(value, "%d.%d", &a, &b);
	index = find_entry(a, b);
	dtu_char_log("script: %s %s", cmd, value ? value : "");
	if (!strcmp(cmd, "damage"))			dtu_damage_player(ConsoleObject, NULL, i2f(a));
	else if (!strcmp(cmd, "attackedby")) {	// attackedby=OBJNUM: a melee attack of a creature on the player (test)
		if (a >= 0 && a <= Highest_object_index && Objects[a].type == OBJ_ROBOT)
			dtu_damage_player(ConsoleObject, &Objects[a], 0);
	}
	else if (!strcmp(cmd, "kill")) {		// ASCENT: kill=OBJNUM: the player kills a creature (test)
		if (a >= 0 && a <= Highest_object_index && Objects[a].type == OBJ_ROBOT)
			dtu_damage_creature_direct(&Objects[a], ConsoleObject, i2f(10000));
		else if (a > 0 && a <= Highest_object_index && Objects[a].type == OBJ_DRESSING)	// scenery breaks (tools/questtest.py)
			dtu_break_dressing(&Objects[a], NULL, 0);
	}
	else if (!strcmp(cmd, "area") && b >= 0 && b <= Highest_object_index)		// ASCENT: test: area=SPELL.OBJ, the area of the spell of the player at object OBJ
		dtu_spell_area(a, &Objects[b].pos, Objects[b].segnum, ConsoleObject, dtu_player_level());
	else if (!strcmp(cmd, "hitbody") && a >= 0 && a <= Highest_object_index) {	// ASCENT: test: hitbody=OBJ, the weapon in the hand hits the fallen body OBJ (Dtu_fix_revive)
		dtu_attack at;

		dtu_char_get_attack(&at);
		dtu_creature_down_swing(&Objects[a], at.weapon_id);
	}
	else if (!strcmp(cmd, "heal"))		dtu_char_heal(a);
	else if (!strcmp(cmd, "use"))			dtu_char_use(index);
	else if (!strcmp(cmd, "usecur"))		dtu_char_use_current();
	else if (!strcmp(cmd, "equip"))		dtu_char_equip_auto(index);
	else if (!strcmp(cmd, "unequip"))	dtu_char_unequip(a);
	else if (!strcmp(cmd, "drop"))		dtu_char_drop(index);
	else if (!strcmp(cmd, "current"))	dtu_char_set_current(index);
	else if (!strcmp(cmd, "hotkey"))		dtu_char_select_hot_key(a);
	else if (!strcmp(cmd, "give")) {
		int c = 1;
		sscanf(value ? value : "", "%d.%d.%d", &a, &b, &c);
		dtu_inventory_add(a, b, c);
	}
	else if (!strcmp(cmd, "skill")) {		// skill=N.VALUE: a thief skill of the base values (test)
		int *sk[5] = { &Dtu_char.base.hide_in_shadows, &Dtu_char.base.move_silently, &Dtu_char.base.open_locks,
			&Dtu_char.base.find_traps, &Dtu_char.base.climb_walls };
		if (a >= 0 && a < 5) *sk[a] = b;
		dtu_char_update();
	}
	else if (!strcmp(cmd, "str")) {		// str=STRENGTH.PERCENT: the base strength (test)
		Dtu_char.base.strength = a;
		Dtu_char.base.strength_extra = b;
		dtu_char_update();
		dump_character();
	}
	else if (!strcmp(cmd, "priestcheck"))	dtu_spell_priest_check();		// ASCENT: test, docs/audit/spells-priest.md
	else if (!strcmp(cmd, "desc"))		dtu_desc_log_all(a);			// ASCENT: test, desc or desc=CASTER_LEVEL: the texts of the description window (dtudesc.h)
	else if (!strcmp(cmd, "descitem"))	dtu_desc_open_item(a, b, 1, -1, 0);	// ASCENT: test, descitem=TYPE.ID opens the window
	else if (!strcmp(cmd, "descspell"))	dtu_desc_open_spell(a);			// ASCENT: test, descspell=SPELL opens the window
	else if (!strcmp(cmd, "fx"))			dtu_spell_set_effect(ConsoleObject, 1 << a, i2f(b), 0, 1);
	else if (!strcmp(cmd, "cast"))		dtu_spell_cast_on(a, ConsoleObject, ConsoleObject);
	else if (!strcmp(cmd, "castby") && b >= 0 && b < MAX_OBJECTS)		// test: castby=SPELL.OBJ, on the player
		dtu_spell_cast_on(a, ConsoleObject, &Objects[b]);
	else if (!strcmp(cmd, "spelltest")) {		// test: spelltest=SPELL.LEVEL.OBJ.TRIALS (dtuspell.h)
		int obj = -1, trials = 1;
		sscanf(value ? value : "", "%d.%d.%d.%d", &a, &b, &obj, &trials);
		dtu_spell_test(a, b, obj, trials);
	}
	else if (!strcmp(cmd, "xp"))			dtu_char_award_xp(a);
	else if (!strcmp(cmd, "rest"))		dtu_char_rest();
	else if (!strcmp(cmd, "mission"))	Fe_khelben_mission = a;		// test: the mission number MsnNum
	else if (!strcmp(cmd, "race"))		Dtu_char.pa.race = Dtu_char.base.race = a;		// test: race=N
	else if (!strcmp(cmd, "class"))		Dtu_char.pa.class1 = Dtu_char.base.class1 = a;	// test: class=N, the first class
	else if (!strcmp(cmd, "search"))		dtu_thief_search_toggle();		// test: the key F
	else if (!strcmp(cmd, "activate"))	dtu_player_use();		// test: the key A
	else if (!strcmp(cmd, "goside") && a >= 0 && a <= Highest_segment_index && b >= 0 && b < 6) {		// test: goside=SEG.SIDE
		vms_vector c, s, d;

		compute_segment_center(&c, &Segments[a]);
		compute_center_point_on_side(&s, &Segments[a], b);
		vm_vec_sub(&d, &s, &c);
		vm_vec_normalize(&d);
		ConsoleObject->pos = c;
		vm_vector_2_matrix(&ConsoleObject->orient, &d, NULL, NULL);
		obj_relink(ConsoleObject - Objects, a);
	}
	else if (!strcmp(cmd, "gosecret"))	dtu_thief_goto_secret(a);		// test: gosecret=N
	else if (!strcmp(cmd, "move")) {		// test: move=N, N units forward (in the segment of the point)
		vms_vector p;
		int seg;

		vm_vec_scale_add(&p, &ConsoleObject->pos, &ConsoleObject->orient.fvec, i2f(a));
		seg = find_point_seg(&p, ConsoleObject->segnum);
		if (seg >= 0) {
			ConsoleObject->pos = p;
			obj_relink(ConsoleObject - Objects, seg);
		}
	}
	else if (!strcmp(cmd, "gold")) {		// test: gold=N, the gold of the player
		Dtu_gold = a;
		Dtu_char.pa.gold = a;
	}
	else if (!strcmp(cmd, "goseg") && a >= 0 && a <= Highest_segment_index) {		// test: goseg=N, the center of segment N
		vms_vector c;

		compute_segment_center(&c, &Segments[a]);
		ConsoleObject->pos = c;
		obj_relink(ConsoleObject - Objects, a);
	}
	else if (!strcmp(cmd, "goobj") && a >= 0 && a < MAX_OBJECTS) {		// test: goobj=N[.D], D (default 4.0) in front of object N
		object *o = &Objects[a];
		vms_vector to;

		vm_vec_sub(&to, &o->pos, &ConsoleObject->pos);
		vm_vec_normalize(&to);
		vm_vec_scale_add(&ConsoleObject->pos, &o->pos, &to, -i2f(b > 0 ? b : 4));
		vm_vector_2_matrix(&ConsoleObject->orient, &to, NULL, NULL);
		obj_relink(ConsoleObject - Objects, find_point_seg(&ConsoleObject->pos, o->segnum) >= 0 ? find_point_seg(&ConsoleObject->pos, o->segnum) : o->segnum);
	}
	else if (!strcmp(cmd, "reach"))		dtu_combat_reach_test(a);		// test: reach=N, the attacks that reach creature N (tools/check_reach.py)
	else if (!strcmp(cmd, "goobjh") && a >= 0 && a <= Highest_object_index) {		// test: goobjh=N.D, D (default 4.0) in front of object N at the height of the player
		object *o = &Objects[a];
		vms_vector to, p;
		int seg;

		vm_vec_sub(&to, &o->pos, &ConsoleObject->pos);
		to.y = 0;
		if (vm_vec_normalize(&to) <= 0x100) {		// on the object: the view direction
			to = ConsoleObject->orient.fvec;
			to.y = 0;
			if (vm_vec_normalize(&to) <= 0x100)
				to.x = F1_0, to.z = 0;
		}
		{
			vm_vec_scale_add(&p, &o->pos, &to, -i2f(b > 0 ? b : 4));
			p.y = ConsoleObject->pos.y;
			seg = find_point_seg(&p, ConsoleObject->segnum);
			if (seg < 0)
				seg = find_point_seg(&p, o->segnum);
			if (seg >= 0) {
				ConsoleObject->pos = p;
				vm_vector_2_matrix(&ConsoleObject->orient, &to, NULL, NULL);
				obj_relink(ConsoleObject - Objects, seg);
			}
		}
	}
	else if (!strcmp(cmd, "onobj") && a >= 0 && a <= Highest_object_index) {		// test: onobj=N, the player stands on object N (same x and z)
		object *o = &Objects[a];
		vms_vector p = ConsoleObject->pos;
		int seg;

		p.x = o->pos.x;
		p.z = o->pos.z;
		seg = find_point_seg(&p, o->segnum);
		if (seg >= 0) {
			ConsoleObject->pos = p;
			obj_relink(ConsoleObject - Objects, seg);
		}
	}
	else if (!strcmp(cmd, "break") && a >= 0 && a < MAX_OBJECTS)		// test: break=N, the scenery N breaks
		dtu_break_dressing(&Objects[a], ConsoleObject, 0);
	else if (!strcmp(cmd, "restreset"))	memset(Rest_time, 0, sizeof(Rest_time));		// test: the rest places work again
	else if (!strcmp(cmd, "noise"))		dtu_creature_alert(ConsoleObject, a);		// ASCENT: test: noise=LEVEL, the alert 0x160AB at the player
	else if (!strcmp(cmd, "hittest") && a >= 0 && a <= Highest_object_index) {	// ASCENT: test: hittest=OBJ.MISSILE, the hit test 0x2A7F3 on object OBJ, melee (0) or a missile (1)
		int ac = 0;
		dtu_hit_test_vs(&Objects[a], NULL, 0, 20, &ac, b);
		dtu_char_log("hittest: obj %d, %s: armor class %d", a, b ? "missile" : "melee", ac);
	}
	else if (!strcmp(cmd, "sethotspell"))	dtu_char_set_hot_key_spell(a, b);		// sethotspell=KEY.SPELL
	else if (!strcmp(cmd, "armspell"))	dtu_char_arm_spell(a);
	else if (!strcmp(cmd, "sethot")) {
		int c = 0;
		sscanf(value ? value : "", "%d.%d.%d", &a, &b, &c);
		dtu_char_set_hot_key(a, find_entry(b, c));
	}
	else if (!strcmp(cmd, "gorest")) {
		int seg;
		for (seg = 0; seg <= Highest_segment_index; seg++)
			if (Segments[seg].special == 2 || Segments[seg].special == 6) {
				vms_vector center;
				compute_segment_center(&center, &Segments[seg]);
				ConsoleObject->pos = center;
				obj_relink(ConsoleObject - Objects, seg);
				dtu_char_log("script: rest place in segment %d (special %d)", seg, Segments[seg].special);
				break;
			}
		if (seg > Highest_segment_index)
			dtu_char_log("script: the level has no rest place");
	}
	else if (!strcmp(cmd, "fire"))		dtu_spell_fire();		// test: the spell, scroll or wand in the hand
	else if (!strcmp(cmd, "slots"))		// ASCENT: test: the spell slots left and the maximum (tools/check_saveload.py)
		dtu_char_log("spell slots: mage left %d %d %d max %d %d %d, cleric left %d %d %d max %d %d %d",
			Dtu_spell_slots_left[0][0], Dtu_spell_slots_left[0][1], Dtu_spell_slots_left[0][2],
			Dtu_spell_slots_max[0][0], Dtu_spell_slots_max[0][1], Dtu_spell_slots_max[0][2],
			Dtu_spell_slots_left[1][0], Dtu_spell_slots_left[1][1], Dtu_spell_slots_left[1][2],
			Dtu_spell_slots_max[1][0], Dtu_spell_slots_max[1][1], Dtu_spell_slots_max[1][2]);
	// ASCENT: the commands of the quest tests (tools/questtest.py)
	else if (!strcmp(cmd, "killscript")) {	// test: killscript=ID, each creature with script ID dies
		int i;

		for (i = 1; i <= Highest_object_index; i++)
			if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & 0x1ff) == a && Objects[i].shields >= 0) {
				dtu_char_log("script: obj %d has script %d and dies", i, a);
				dtu_damage_creature_direct(&Objects[i], NULL, Objects[i].shields + F1_0);
			}
	}
	else if (!strcmp(cmd, "face") && a >= 0 && a <= Highest_object_index) {		// test: face=N, turn to object N (level view)
		vms_vector to;

		vm_vec_sub(&to, &Objects[a].pos, &ConsoleObject->pos);
		to.y = 0;
		if (vm_vec_normalize(&to) > 0x100)
			vm_vector_2_matrix(&ConsoleObject->orient, &to, NULL, NULL);
		dtu_char_log("script: distance to obj %d: %.1f, height %.1f", a, f2fl(vm_vec_dist(&Objects[a].pos, &ConsoleObject->pos)),
			f2fl(Objects[a].pos.y - ConsoleObject->pos.y));
	}
	else if (!strcmp(cmd, "aim") && a >= 0 && a <= Highest_object_index) {		// test: aim=N, look at the center of object N
		vms_vector to;

		vm_vec_sub(&to, &Objects[a].pos, &ConsoleObject->pos);
		if (vm_vec_normalize(&to) > 0x100)
			vm_vector_2_matrix(&ConsoleObject->orient, &to, NULL, NULL);
	}
	else if (!strcmp(cmd, "warp"))		dtu_script_test_warp(a, b);		// test: warp=LEVEL.SEG, the warp of statement DB
	else if (!strcmp(cmd, "save"))		dtufe_test_save(a);				// test: save=SLOT, as Alt+F2 with no menu
	else if (!strcmp(cmd, "load"))		dtufe_test_load(a);				// test: load=SLOT, as Alt+F3 with no menu
	else if (!strcmp(cmd, "items") && a >= 0 && a <= Highest_segment_index) {	// test: items=SEG, the item objects of the segment
		char list[400], *q = list;
		int i;

		for (i = Segments[a].objects; i != -1 && q < list + 380; i = Objects[i].next)
			if (Objects[i].type == OBJ_WEAPON || Objects[i].type == OBJ_ARMOR || Objects[i].type == OBJ_MISC || Objects[i].type == OBJ_SPELL)
				q += sprintf(q, "%d.%d ", Objects[i].type, Objects[i].id);
		*q = 0;
		dtu_char_log("items in seg %d: %s", a, list);
	}
	else if (!strcmp(cmd, "objflag") && a > 0 && a <= Highest_object_index && b >= 0 && b < 8)	// test: objflag=N.K, script flag K of object N
		Objects[a].dtu.m_1b0 |= 1 << (20 + b);
	else if (!strcmp(cmd, "resume") && a > 0 && a <= Highest_object_index)		// test: resume=N.LABEL, the resume label of object N
		Objects[a].dtu.m_1b0 = (Objects[a].dtu.m_1b0 & ~0x000ffc00) | ((b & 0x3ff) << 10);
	else if (!strcmp(cmd, "talkscript")) {	// test: talkscript=ID, the use action on the first live creature with script ID
		int i;

		for (i = 1; i <= Highest_object_index; i++)
			if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & 0x1ff) == a && Objects[i].shields >= F1_0) {
				dtu_char_log("script: talk to obj %d (script %d)", i, a);
				dtu_script_talk(&Objects[i]);
				break;
			}
	}
	else if (!strcmp(cmd, "prison"))		dtu_script_test_prison();		// test: the prison sequence 0xAA3F3
	else if (!strcmp(cmd, "setlevel"))	dtu_char_set_level(a);			// test: setlevel=N, statement 9F
	else if (!strcmp(cmd, "gflag"))		dtu_script_set_gflag(a, value && strchr(value, '.') ? b : 1);		// test: gflag=N[.VALUE]
	else if (!strcmp(cmd, "mflag"))		dtu_mission_set_flag(a, value && strchr(value, '.') ? b : 1);		// test: mflag=K[.VALUE]
	else if (!strcmp(cmd, "talk") && a > 0 && a <= Highest_object_index)		// test: talk=N, the use action on object N
		dtu_script_talk(&Objects[a]);
	else if (!strcmp(cmd, "trigger") && a >= 0 && a <= Highest_segment_index && b >= 0 && b < 6)		// test: trigger=SEG.SIDE, as the player touches the side
		dtu_trig_check(a, b, DTU_ACT_PLAYER, ConsoleObject);
	else if (!strcmp(cmd, "quest")) {	// test: quest, the quest state in one line
		char flags[1100], *p = flags;
		int i;

		for (i = 1; i <= 255; i++)
			if (dtu_script_gflag(i))
				p += sprintf(p, "%d ", i);
		*p = 0;
		dtu_char_log("quest: level %d seg %d mission %d gold %d hp %d fx 0x%x gflags [ %s]", Current_level_num - 1,
			ConsoleObject->segnum, Fe_khelben_mission, Dtu_gold, dtu_char_hit_points(), ConsoleObject->dtu.m_88, flags);
		p = flags;
		for (i = 1; i <= DTU_N_MISSION_FLAGS; i++)
			if (dtu_mission_flag(i))
				p += sprintf(p, "%d ", i);
		*p = 0;
		dtu_char_log("quest: mflags [ %s] entries %d", flags, Dtu_n_inventory);
		p = flags;
		for (i = 0; i < Dtu_n_inventory && p < flags + 1000; i++)
			p += sprintf(p, "%d.%d ", Dtu_inventory[i].type, Dtu_inventory[i].id);
		*p = 0;
		dtu_char_log("quest: items [ %s]", flags);
	}
	else if (!strcmp(cmd, "obj") && a >= 0 && a <= Highest_object_index) {		// test: obj=N, the state of object N in one line
		object *o = &Objects[a];

		dtu_char_log("obj %d: type %d id %d seg %d hp %d flags 0x%x script %d resume %d sflags 0x%x var %d talks %d contains %d.%d x%d pos %.1f,%.1f,%.1f anim %d",
			a, o->type, o->id, o->segnum, f2i(o->shields), o->flags, o->dtu.m_1b0 & 0x1ff, (o->dtu.m_1b0 >> 10) & 0x3ff,
			(o->dtu.m_1b0 >> 20) & 0xf, (o->dtu.m_1b0 >> 24) & 0xf, (o->dtu.m_1b0 >> 28) & 0xf,
			o->contains_type, o->contains_id, o->contains_count,
			f2fl(o->pos.x), f2fl(o->pos.y), f2fl(o->pos.z), DTU_ANIM_STATE(o));
	}
	else if (!strcmp(cmd, "near")) {		// test: near or near=RANGE[.1], the creatures (.1: all objects) in RANGE (default 30) by distance, with the angle to the view
		int i, range = a > 0 ? a : 30;

		for (i = 0; i <= Highest_object_index && ConsoleObject; i++) {
			object *o = &Objects[i];
			vms_vector d;
			fix dist;

			if (o->type != OBJ_ROBOT)
				continue;
			dist = vm_vec_normalized_dir(&d, &o->pos, &ConsoleObject->pos);
			if (dist > i2f(range))
				continue;
			dtu_char_log("near %d: %s type %d id %d seg %d dist %.2f dot %.2f hp %d flags 0x%x size %.2f pass_thru %d",
				i, dtu_object_name(o), o->type, o->id, o->segnum, f2fl(dist), f2fl(vm_vec_dot(&d, &ConsoleObject->orient.fvec)),
				f2i(o->shields), o->flags, f2fl(o->size), o->type == OBJ_ROBOT && o->id < N_robot_types && (Monster_info[o->id].flags & MONSTER_F_PASS_THRU) ? 1 : 0);
		}
		dtu_char_log("near: target hint %d", dtu_combat_target_hint());
	}
	else if (!strcmp(cmd, "wall") && a >= 0 && a <= Highest_segment_index && b >= 0 && b < 6) {		// test: wall=SEG.SIDE, the wall of the side
		int wn = Segments[a].sides[b].wall_num;

		if (wn < 0)
			dtu_char_log("wall %d.%d: none", a, b);
		else
			dtu_char_log("wall %d.%d: type %d flags 0x%x state %d", a, b, Walls[wn].type, Walls[wn].flags, Walls[wn].state);
	}
	else if (!strcmp(cmd, "mode"))		dtu_display_select_mode(a);
	else if (!strcmp(cmd, "dump"))		dump_character();
	else if (!strcmp(cmd, "rules"))		rules_test();		// ASCENT: test, docs/audit/character.md
	else if (!strcmp(cmd, "setlevel"))	dtu_char_set_level(a);		// test: 0x8BA3A (script statement 9F)
	else
		dtu_char_log("script: unknown command");
	if (strcmp(cmd, "dump"))
		dtu_char_log("  hit points %d of %d, armor class %d, %d entries", dtu_char_hit_points(), dtu_char_max_hit_points(),
			dtu_char_armor_class(), Dtu_n_inventory);
}

static void run_script(void)
{
	char *p = Char_script;

	while (p && *p) {
		char item[80], *value, *colon;
		int len = 0;

		while (p[len] && p[len] != ',' && len < 79) len++;
		memcpy(item, p, len);
		item[len] = 0;
		p += len;
		if (*p == ',') p++;
		colon = strchr(item, ':');
		if (!colon || atoi(item) != Char_frame)
			continue;
		value = strchr(colon + 1, '=');
		if (value) *value++ = 0;
		run_command(colon + 1, value);
	}
}

//------------------------------------------------------------------------
// Calls from the game

// Dtu_fix_spell_save_repair: the first level start after a load (the
// spell tables of the level are loaded).
static void spell_save_repair(void)
{
	int i, n_moved = 0, n_cleric = 0;

	if (!Spell_repair_pending)
		return;
	Spell_repair_pending = 0;
	if (!Dtu_fix_spell_save_repair)
		return;
	for (i = 0; i < Dtu_n_inventory; ) {
		dtu_inv_item *e = &Dtu_inventory[i];

		if (e->type == OBJ_SPELL && e->count > 0 && dtu_spell_insert(e->id) == 0) {
			int n = Dtu_n_inventory;

			dtu_char_log("repair: spell %d '%s' of item %d goes to the spell list", e->id, e->name, i);
			n_moved++;
			inv_remove(i, 1);
			if (Dtu_n_inventory < n)
				continue;
		}
		i++;
	}
	if (has_class(DTU_CLASS_CLERIC))
		n_cleric = dtu_spell_grant_cleric();
	if (n_moved || n_cleric)
		dtu_char_log("repair: %d spells from the item list, %d cleric spells; %d mage and %d cleric spells",
			n_moved, n_cleric, Dtu_spell_book_n[0], Dtu_spell_book_n[1]);
}

void dtu_char_level_start(void)
{
	int i;

	read_options();
	if (!Dtu_char.created) {
		if (!Char_save_file || !dtu_char_load_save(Char_save_file))
			dtu_char_create_default();
	}
	for (i = 0; i < MAX_SEGMENTS; i++)
		Rest_time[i] = 0;
	Dtu_char.no_pickup_until = 0;
	// The hit points go from level to level. The death of the executable is
	// not in the port: a dead character starts the level with all of them.
	if (Dtu_char.pa.hit_points < 1)
		Dtu_char.pa.hit_points = Dtu_char.pa.max_hit_points;
	set_hit_points(Dtu_char.pa.hit_points);
	// ASCENT: the potion timers of a loaded save (0xBA911: now + seconds)
	{
		fix *t[4];

		t[0] = &Potion.intel_end; t[1] = &Potion.str_end; t[2] = &Potion.dragon_end; t[3] = &Potion.dex_end;
		for (i = 0; i < 4; i++)
			if (Potion.load_left[i]) {
				*t[i] = GameTime + i2f(Potion.load_left[i]);
				dtu_char_log("level start: potion timer %d from the save, %d seconds", i, Potion.load_left[i]);
				Potion.load_left[i] = 0;
			}
	}
	Dtu_player_portrait = Dtu_char.portrait;
	sync_inventory();
	spell_save_repair();
	dtu_char_update();
	dtu_spell_level_start();		// ASCENT: the slots left stay (dtuspell.c)
	dtu_char_log("level start: '%s', %d of %d hit points", Dtu_char.name, Dtu_char.pa.hit_points, Dtu_char.pa.max_hit_points);
}

void dtu_char_frame(void)
{
	fix max;

	if (!Dtu_char.created)
		return;
	Char_frame++;

	// The hit points above the maximum of Aid and the Potion of Toughening
	// stay while Bless lasts (its end takes them away)
	max = i2f(Dtu_char.pa.max_hit_points);
	if (Players[Player_num].shields > max && !(ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_BLESS)))
		set_hit_points(Dtu_char.pa.max_hit_points);
	dtu_char_potions_end(0);		// 0x396B2 from the frame 0x435A6
	dtu_thief_frame();
	Dtu_char.pa.hit_points = dtu_char_hit_points();
	Dtu_char.pa.gold = Dtu_gold;
	sync_inventory();
	if (Char_script)
		run_script();
}
