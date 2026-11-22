/*
 * ASCENT: new file. The player character of Descent to Undermountain. It
 * replaces the ship values of the Descent 1 player. See dtuchar.h.
 *
 * The rules are from DTUGAME.EXE version 1.3. Each comment with an address
 * (0x...) names the function or the data of the executable that the code
 * follows. docs/re/character.md has the details. A comment that says
 * "guess" marks a part that is not verified.
 */

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
#include "text.h"
#include "gauges.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtudisp.h"
#include "dtutrig.h"
#include "dtuchar.h"
#include "dtuspell.h"	// ASCENT: the spell effects of the armor class and the slots of a rest
#include "dtuthief.h"
#include "dtucombat.h"	// ASCENT: turn undead of the Spider Piece 0x12
#include "newmenu.h"
#include "digi.h"
#include "automap.h"

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
int Dtu_fix_wizardry_ring = 1;
int Dtu_fix_potion_frame = 1;
int Dtu_fix_luck_coin = 1;
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
	int	gem_uses;			// P+0x1A57 Gem of Curing
	int	str_gain, pct_gain;	// P+0x1A55, P+0x1A56: the gain of the spell Strength
	fix	intel_acc;			// Dtu_fix_potion_frame
} Potion;
static int Worn_bits = 0;		// 0x1458AC: the effect bits of the worn items
static int Wizardry_count = 0;	// doublings of the Ring of Wizardry
static int Rests = 0;			// the number of rests (the Gem of Curing, guess)

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
// value (0, 1 to 50, 51 to 75, 76 to 90, above 90).
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
		Dtu_fix_multiclass_thac0 = 0;
	if (FindArg("-origcombat"))
		Dtu_fix_wizardry_ring = Dtu_fix_potion_frame = Dtu_fix_luck_coin = 0;
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
		// have no wear word. The code of the lockpick slot knows these four
		// numbers (0xB02D6). That they can go into the slot is a guess.
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
		d->dice_sides = rec_int(r, "damage_dice", 0);
		d->dice_n = rec_int(r, "#damage_dice", 1);
		d->damage_bonus = rec_int(r, "damage_bonus", 0);
		d->to_hit_bonus = rec_int(r, "to_hit_bonus", 0);
		d->attack_wait = rec_fix(r, "fire_wait", F1_0);
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

// 0xAF605
static void update_thac0(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1, t = 20, t2;

	if (l1 < 0) l1 = 0;
	if (l1 > 17) l1 = 17;
	if (l2 < 0) l2 = 0;
	if (l2 > 17) l2 = 17;
	if (valid_class(pa->class1))
		t = Thac0_table[pa->class1][l1];
	if (valid_class(pa->class2)) {
		t2 = Thac0_table[pa->class2][l2];
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

static int worn_item_bits(void)
{
	int bits = 0, id, r;
	dtu_inv_item *e;

	// boots (0xAFBEE, jump table 0xAFAE5)
	switch (armor_id(Dtu_char.slot[DTU_SLOT_BOOT])) {
		case 34: bits |= 1 << 4; break;		// Winged Boots: Fly
		case 35: bits |= 1 << 3; break;		// Boots of Feather Falling
		case 36: bits |= 1 << 11; break;		// Boots of Levitation
		case 37: bits |= 1 << 9; break;		// Boots of Speed: Haste
	}
	// necklace 33: Amulet of Shield (0xAFCBA tests the number only)
	e = ref_entry(Dtu_char.slot[DTU_SLOT_NECKLACE]);
	if (e && e->id == 33)
		bits |= 1 << 10;
	// gauntlet 51: Glim-Gauntlet, Detect Magic (0xAFD99)
	if (armor_id(Dtu_char.slot[DTU_SLOT_GAUNTLET]) == 51)
		bits |= 1 << 8;
	// cloak 44: Chameleon Cloak, Improved Invisibility; 46: Elven Cloak,
	// Invisibility (0xAFF54)
	id = armor_id(Dtu_char.slot[DTU_SLOT_CLOAK]);
	if (id == 44) bits |= 1 << 12;
	if (id == 46) bits |= 1 << 13;
	// rings (jump tables 0xAFAF5 and 0xAFB25). Ring 16 (Armoring) sets a
	// flag that nothing reads.
	for (r = 0; r < 2; r++) {
		switch (armor_id(Dtu_char.slot[r ? DTU_SLOT_RING2 : DTU_SLOT_RING1])) {
			case 17: bits |= 1 << 1; break;		// Ring of Coolness: Resist Fire
			case 18: bits |= 1 << 3; break;		// Feather Fall
			case 19: bits |= 1 << 4; break;		// Flying
			case 20: bits |= 1 << 5; break;		// Free Action
			case 22: if (Dtu_char.pa.race <= 0) bits |= 1 << 6; break;		// Infravision, humans only
			case 23: bits |= 1 << 7; break;		// Jumping
			case 24: bits |= 1 << 8; break;		// Magic Detection
			case 25: bits |= 1 << 9; break;		// Quickness: Haste
		}
	}
	// weapon 54 in the hand (0xB038F): bits 0, 1, 3, 5, 7, 8, 9, 12, and 6
	// for a human
	e = ref_entry(Dtu_char.current);
	if (e && e->type == OBJ_WEAPON && e->id == 54) {
		bits |= 0x2b | 0x1380;
		if (Dtu_char.pa.race == 0)
			bits |= 0x40;
	}
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

// The values of the potions and of the spell Strength (0x396B2, 0x8FCFA)
static void potion_values(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;

	if (Potion.str_end) {								// Hill Giant Strength: at least 19
		if (pa->strength < 19) {
			pa->strength = 19;
			pa->strength_extra = 0;
		}
	}
	if (Potion.dragon_end) {							// Dragon's Blood: at least 18
		if (pa->strength < 18) {
			pa->strength = 18;
			pa->strength_extra = Potion.dragon_extra;
		}
	}
	if (Potion.dex_end && pa->dexterity < 18)	// Agility: at least 18
		pa->dexterity = 18;
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
	int ac, dex;

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
	if (e && e->type == OBJ_ARMOR)
		sum += table_ac(e);

	// belt. Armor 38 is the Girdle of Hill Giant Strength.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_BELT]);
	if (e) {
		sum += table_ac(e);
		if (armor_is(e, 38, 38)) {
			pa->strength = 19;
			pa->strength_extra = 0;
		}
	}

	// necklace. Armor 33 (Amulet of Shield) gives a spell effect only.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_NECKLACE]);
	if (e && !armor_is(e, 33, 33))
		sum += table_ac(e);

	// shield. 0xAF9CF: the numbers 2, 12 and 13 are shields.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_SHIELD]);
	if (e) {
		shield = armor_is(e, 2, 2) || armor_is(e, 12, 13);
		sum += table_ac(e);
	}

	// gauntlet
	e = ref_entry(Dtu_char.slot[DTU_SLOT_GAUNTLET]);
	if (e) {
		sum += table_ac(e);
		if (armor_is(e, 52, 52)) {					// Gauntlets of Ogre Power
			pa->strength = 18;
			pa->strength_extra = 0;
		}
		else if (armor_is(e, 50, 50)) {			// Gloves of Dexterity
			if (pa->dexterity <= 19)
				pa->dexterity += Dex_glove[pa->dexterity];
			if (has_class(DTU_CLASS_THIEF))
				pa->open_locks += 10;
			else
				pa->open_locks = 37;
		}
		else if (e->type == OBJ_MISC && e->id == 9) {	// Gauntlets (0xB0B5F)
			pa->find_traps -= 250;
			pa->open_locks -= 250;
		}
	}

	// armor. 0xAF9F3: the numbers 5 to 11 are magic armor. 0xAF982: 39 to
	// 42 are the Bracers of Defense. 0xB0B0A: in the numbers 3, 4 and 7 to
	// 11 (chain mail, plate mail) three thief skills do not work.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_ARMOR]);
	if (e && e->type == OBJ_ARMOR) {
		sum += table_ac(e);
		magic_armor = armor_is(e, 5, 11) || armor_is(e, 39, 42);
		heavy_armor = armor_is(e, 3, 4) || armor_is(e, 7, 11);
		if (heavy_armor) {								// 0xB0B36
			pa->hide_in_shadows -= 250;
			pa->move_silently -= 250;
			pa->climb_walls -= 250;
		}
	}

	// cloak. 0xAF930: the numbers 47 to 49 are the Cloaks of Protection.
	// They do not work with a shield, magic armor or heavy armor.
	e = ref_entry(Dtu_char.slot[DTU_SLOT_CLOAK]);
	if (e) {
		if (e->type == OBJ_ARMOR) {
			if (!armor_is(e, 47, 49) || (!shield && !magic_armor && !heavy_armor))
				sum += table_ac(e);
			if (e->id == 43)								// Cloak of Elvenkind
				pa->hide_in_shadows = 95;
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
			prot_ring = armor_is(e, 0, 0) || armor_is(e, 14, 15);
			if (!magic_armor) {
				if (prot_ring) ring_best = table_ac(e);
				else sum += table_ac(e);
			}
			if (e->id == 21) pa->constitution = 18;
			if (e->id == 26) { pa->strength = 18; pa->strength_extra = 0; }
		}
		else
			sum += table_ac(e);
	}
	e = ref_entry(Dtu_char.slot[DTU_SLOT_RING2]);
	if (e) {
		if (e->type == OBJ_ARMOR) {
			int this_prot = armor_is(e, 0, 0) || armor_is(e, 14, 15);
			int v = table_ac(e);

			prot_ring |= this_prot;
			if (!magic_armor) {
				if (prot_ring) { if (v < ring_best) ring_best = v; }
				else if (!this_prot) sum += v;
			}
			if (e->id == 21) pa->constitution = 18;
			if (e->id == 26) { pa->strength = 18; pa->strength_extra = 0; }
		}
		else
			sum += table_ac(e);
	}
	sum += ring_best;

	// lockpicks, for a thief (0xB02BE)
	e = ref_entry(Dtu_char.slot[DTU_SLOT_LOCKPICK]);
	if (e && e->type == OBJ_MISC && has_class(DTU_CLASS_THIEF)) {
		if (e->id == 10) pa->open_locks += 5;
		if (e->id == 13) pa->open_locks += 10;
		if (e->id == 11) pa->open_locks += 15;
		if (e->id == 12) pa->open_locks += 20;
	}

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
	if (e && (e->type == OBJ_WEAPON || e->type == OBJ_MISC))
		sum += table_ac(e);
	// 0xB038F: weapon 54 (Sword of W'hin) in the hand: Find/Remove Traps
	// 250 (95 after the limit)
	if (e && e->type == OBJ_WEAPON && e->id == 54)
		pa->find_traps = 250;

	// The potions (0x396B2) and the spell Strength (0x8FCFA) change the
	// values of the record while they last
	potion_values();

	// 0xB03E6 and 0xB08A8: the effects of the worn items
	worn_effects();

	// 0xB0B7E: each thief skill is 0 to 95
	#define CLAMP95(v) do { if ((v) < 0) (v) = 0; if ((v) > 95) (v) = 95; } while (0)
	CLAMP95(pa->hide_in_shadows);
	CLAMP95(pa->move_silently);
	CLAMP95(pa->find_traps);
	CLAMP95(pa->climb_walls);
	CLAMP95(pa->open_locks);
	#undef CLAMP95

	// 0xB05C5. The spell effects that change the armor class (0xB05D4 to
	// 0xB0696 and 0xB0702) are in dtuspell.c.
	ac = 10 + (signed char) sum;
	ac = dtu_spell_player_ac(ac, 0);
	dex = pa->dexterity;
	if (dex < 1) dex = 1;
	if (dex > 19) dex = 19;
	pa->dexterity = dex;
	pa->dex_ac_adjust = Dex_ac[dex];
	ac += pa->dex_ac_adjust;
	ac = dtu_spell_player_ac(ac, 1);
	if (ac < -10) ac = -10;
	if (ac > 10) ac = 10;
	pa->armor_class = ac;

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
	dtu_char_select_hot_key(key);
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

int dtu_char_armor_class(void)
{
	return Dtu_char.created ? Dtu_char.pa.armor_class : 10;
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
// -1 for Escape. The option -autochoice N answers N without a box.
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

	if (Potion.intel_end && (force || GameTime > Potion.intel_end)) {
		Potion.intel_end = 0;
		HUD_init_message("Hill Giant Intelligence expired.");
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
		HUD_init_message("Hill Giant Strength expired.");
		changed = 1;
	}
	if (Potion.dragon_end && (force || GameTime > Potion.dragon_end)) {
		Potion.dragon_end = 0;
		HUD_init_message("Dragon's Blood expired.");
		changed = 1;
	}
	if (Potion.dex_end && (force || GameTime > Potion.dex_end)) {
		Potion.dex_end = 0;
		HUD_init_message("Agility expired.");
		changed = 1;
	}
	if (Potion.luck_end && (force || GameTime > Potion.luck_end)) {
		Potion.luck_end = 0;
		HUD_init_message("Your luck just ran out.");
		changed = 1;
	}
	if (changed) {
		dtu_char_log("potion: a potion effect ends%s", force ? " (force)" : "");
		dtu_char_update();
	}
}

static int any_potion(void)
{
	return Potion.intel_end || Potion.str_end || Potion.dragon_end || Potion.dex_end || Potion.luck_end;
}

static void effect_on_player(int mask, fix duration)
{
	if (ConsoleObject)
		dtu_spell_set_effect(ConsoleObject, mask, duration, 1, 1);
}

static void spell_on_player(int spell)
{
	if (ConsoleObject)
		dtu_spell_cast_on(spell, ConsoleObject, ConsoleObject);
}

// 0x2FACC: the system shock of constitution 1 to 19
static const ubyte System_shock[19] = { 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 88, 90, 95, 97, 99, 99 };

// The misc items. Returns 1 if the item is used up (the cases that go to
// 0x39694), 0 if not, -1 if the item has no use.
static int use_misc(dtu_inv_item *e)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	object *p = ConsoleObject;
	int hp = dtu_char_hit_points(), max = dtu_char_max_hit_points(), n, i;

	switch (e->id) {
		case 0x02:		// Gem of Curing: not used up
			// The executable compares the uses with the counter 0x13E588
			// (0x8445A), which was not decoded. The port allows one use for
			// each rest. (guess)
			if (Potion.gem_uses > Rests) {
				HUD_init_message("You cannot use the Gem Of Curing at this time.");
				return 0;
			}
			if (p->dtu.m_88 & DTU_FX_BLIND) {
				dtu_spell_clear_effect(p, DTU_FX_BLIND);
				HUD_init_message("...And now you can see!");
			}
			else if (hp < max)
				spell_on_player(11);
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
				static const int s[7] = { 1, 35, 19, 40, 43, 18, 50 };
				dtu_spell_area(14, &p->pos, p->segnum, p, dtu_spell_player_level(14));
				for (i = 0; i < 7; i++)
					spell_on_player(s[i]);
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x19:		// Assembled Spider: four effects with no end
			if (e->count > 0) {
				static const int s[4] = { 48, 20, 21, 25 };
				static const int m[4] = { DTU_FX_SPIDER_CLIMB, DTU_FX_FREE_ACTION, DTU_FX_HASTE, DTU_FX_INFRAVISION };
				for (i = 0; i < 4; i++) {
					spell_on_player(s[i]);
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
				static const int s[9] = { 1, 45, 15, 24, 35, 38, 39, 43, 49 };
				dtu_char_heal(max);
				for (i = 0; i < 9; i++)
					spell_on_player(s[i]);
				if (p->dtu.m_88 & (DTU_FX_BLIND|DTU_FX_DEAF))
					spell_on_player(42);
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x12:		// Spider Piece: turn undead as a cleric of level 2 (0xADC46)
			if (e->count > 0) {
				dtu_combat_turn_undead(2);
				e->count--;
			}
			else
				HUD_init_message("Nothing happens...");
			return 0;
		case 0x14: case 0x17: case 0x18:
			// The spell attack on the drow, the view tour of the
			// creatures, a script event: not in the port
			dtu_char_log("use: Spider Piece 0x%x is not in the port", e->id);
			return -1;
		case 0x48:		// Potion of Agility: dexterity 18 for 600 s
			if (pa->dexterity < 18) {
				Potion.dex_end = GameTime + i2f(600);
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
			if (pa->strength < 18) {
				Potion.dragon_end = GameTime + i2f((rand() % (pa->class1_level + 1) + 1) * 60);
				Potion.dragon_extra = has_class(DTU_CLASS_FIGHTER) ? rand() % 100 : 0;
				spell_vclip(50);
			}
			break;
		case 0x4B:		// Potion of Extra-Healing: 6 to 27 hit points
			if (hp < max) {
				n = dtu_char_heal(rand() % 22 + 6);
				HUD_init_message("You feel much better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, hp, dtu_char_hit_points(), max);
			}
			spell_vclip(11);
			break;
		case 0x4C:		// Potion of Fire Resistance: Resist Fire for 600 s
			effect_on_player(DTU_FX_RESIST_FIRE, i2f(600));
			spell_vclip(43);
			break;
		case 0x4D:		// Potion of Flying: the spell Fly
			spell_on_player(19);
			break;
		case 0x4E:		// Potion of Freedom: Free Action for 50 to 80 rounds
			effect_on_player(DTU_FX_FREE_ACTION, rnd_time(4, 5, 600));
			HUD_init_message("You are immune to paralysis for awhile.");
			break;
		case 0x4F:		// Potion of Healing: 4 to 10 hit points
			if (hp < max) {
				n = dtu_char_heal(rand() % 7 + 4);
				HUD_init_message("You feel better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, hp, dtu_char_hit_points(), max);
			}
			spell_vclip(10);
			break;
		case 0x50:		// Potion of Hill Giant Strength: 19 for 50 to 80 rounds
			if (pa->strength < 19) {
				Potion.str_end = GameTime + rnd_time(4, 5, 600);
				HUD_init_message("You have Hill Giant strength for awhile.");
			}
			break;
		case 0x51:		// Poshun uv Hil Gient Inteljuns: intelligence 5 to 7
			Potion.intel = rand() % 3 + 5;
			Potion.intel_end = GameTime + rnd_time(4, 5, 600);
			// a random vclip and a flash for each point of intelligence: the
			// vclip of one spell here
			spell_vclip(7);
			break;
		case 0x52:		// Potion of Infravision: 10 to 80 rounds
			effect_on_player(DTU_FX_INFRAVISION, rnd_time(8, 1, 600));
			spell_vclip(25);
			break;
		case 0x53:		// the second "Potion of Agility": Invisibility for 30 to 60 rounds
			effect_on_player(DTU_FX_INVISIBLE, rnd_time(4, 3, 600));
			spell_vclip(26);
			break;
		case 0x54:		// Potion of Invulnerability: 5 to 20 rounds
			effect_on_player(DTU_FX_INVULNERABLE, rnd_time(16, 5, 60));
			spell_vclip(28);
			break;
		case 0x55:		// Potion of Jumping: the spell Jump
			spell_on_player(27);
			break;
		case 0x56:		// Potion of Luck: one hour, one critical hit
			Potion.luck_end = GameTime + i2f(3600);
			spell_vclip(1);
			HUD_init_message("You feel lucky today.");
			break;
		case 0x57:		// Potion of Speed: Haste for 5 to 20 rounds
			effect_on_player(DTU_FX_HASTE, rnd_time(16, 5, 60));
			spell_vclip(21);
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
				n = rand() % 17 + 2;
				dtu_spell_set_effect(p, DTU_FX_BLESS, rnd_time(6, 7, 600), 0, 0);
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
		case 0x7F:		// thief scroll: the text 1
			dtu_char_log("use: '%s' shows the scroll text %d (the text screen 0x9DC42 is not in the port)", e->name,
				e->id == 0x7F ? 1 : e->count);
			return 0;
		case 0x6A:		// Cursed Scroll
			{
				static const int s[5] = { 7, 9, 44, 22, 16 };
				ushort *a[6] = { &Dtu_char.base.strength, &Dtu_char.base.dexterity, &Dtu_char.base.constitution,
					&Dtu_char.base.intelligence, &Dtu_char.base.wisdom, &Dtu_char.base.charisma };

				for (i = 0; i < 5; i++)
					if (rand() % 2)
						spell_on_player(s[i]);
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
				if (hp > 0)
					dtu_damage_player(p, NULL, i2f(rand() % hp));
				HUD_init_message("You no longer think that reading the scroll was such a good idea...");
			}
			break;
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
		// weapon 13, the item in the hand, with none of the effects
		// 0xC004124: script events, and for a Lawful Good character
		// Continual Light, Free Action and Protection From Evil
		if (e->id != 13 || Dtu_char.current.n != index || (ConsoleObject->dtu.m_88 & 0xC004124))
			return 0;
		dtu_hook_script_event(0x1FD);
		if (Dtu_char.pa.alignment == 0) {
			dtu_spell_area(8, &ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject, dtu_spell_player_level(8));
			spell_on_player(20);
			spell_on_player(53);
		}
		return 0;
	}
	if (e->type == OBJ_ARMOR) {
		if (e->id == 45)		// armor 45 (Cloak of Elvenkind): area Continual Light
			dtu_spell_area(8, &ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject, dtu_spell_player_level(8));
		return 0;
	}
	if (e->type != OBJ_MISC)
		return 0;

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
	inv_remove(index, 1);
	return 1;
}

// 0x3614B and 0x2FAF0: the item in the hand is used if it is a misc item.
// 0xB7084: one item of a stack, or one charge, is used. A count of -1 has
// no limit. At 0 the entry goes away and the references follow the list.
void dtu_char_use_up(int index)
{
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
	Rests++;
	dtu_spell_restore_slots();		// 0x36049 calls 0x3551B
	dtu_spell_rest_shuffle();		// 0x3E1F6 calls 0x92FF2
	// The executable also calls 0x160AB(player, 5), plays sound 62 and
	// removes the fire of the rest place. These are not in the port.
	return 1;
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

	if (new1) {
		char text[120];
		sprintf(text, "Your character has attained new %s level", game_text(642 + (pa->class1 == 3 ? 5 : pa->class1), class_name[pa->class1]));
		HUD_init_message("%s", text);
		hp += level_hit_points(pa->class1, old1, pa->class1_level);
	}
	if (new2) {
		char text[120];
		sprintf(text, "Your character has attained new %s level", game_text(642 + (pa->class2 == 3 ? 5 : pa->class2), class_name[pa->class2]));
		HUD_init_message("%s", text);
		hp += level_hit_points(pa->class2, old2, pa->class2_level);
	}
	// 0xAF86F: one half for a character with two classes
	if (valid_class(pa->class2))
		hp >>= 1;
	dtu_char_log("level: class1 level %d -> %d, class2 level %d -> %d, %d hit points", old1 + 1, pa->class1_level + 1,
		old2 + 1, pa->class2_level + 1, hp);
	pa->max_hit_points += hp;
	Dtu_char.base.max_hit_points += hp;
	Dtu_char.base.hit_points += hp;
	set_hit_points(dtu_char_hit_points() + hp);
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

//------------------------------------------------------------------------
// Combat values

// 0x2A789: the attack bonus of the strength
static int str_hit_bonus(int str, int extra)
{
	int b;

	if (str < 1) str = 1;
	if (str > 19) str = 19;
	b = Str_hit[str];
	if (str == 18 && has_class(DTU_CLASS_FIGHTER))
		b += Str_extra_hit[extra > 90 ? 4 : extra > 75 ? 3 : extra > 50 ? 2 : extra > 0 ? 1 : 0];
	return b;
}

// 0x2B494: the damage bonus of the strength
static int str_damage_bonus(int str, int extra)
{
	int b;

	if (str > 19) str = 19;
	b = Str_damage[str];
	if (str == 18 && has_class(DTU_CLASS_FIGHTER))
		b += Str_extra_damage[extra > 90 ? 4 : extra > 75 ? 3 : extra > 50 ? 2 : extra > 0 ? 1 : 0];
	return b;
}

void dtu_char_get_attack(dtu_attack *a)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	dtu_inv_item *e = ref_entry(Dtu_char.current);
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
	if (e && e->type == OBJ_WEAPON && dtu_item_get(e->type, e->id, &d) && d.dice_sides > 0) {
		// 0x2AAE7 and 0x2B06A: the weapon
		a->weapon_id = e->id;
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
			dtu_inv_item *l = ref_entry(Dtu_char.slot[bm_tbl_has_flag(Weapon_item_info[e->id].record, "BOLT") ? DTU_SLOT_CROSSBOW : DTU_SLOT_BOW]);
			dtu_item_data ld;

			if (l && dtu_item_get(l->type, l->id, &ld)) {
				a->to_hit_bonus += ld.to_hit_bonus;
				a->damage_bonus += ld.to_hit_bonus;
			}
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

static int d20_result(int roll, int bonus, int thac0, int ac)
{
	if (roll == 20) return 2;
	if (roll == 1) return -1;
	return (thac0 - ac > roll + bonus) ? 0 : 1;			// 0x2AE32
}

int dtu_char_attack_roll(int target_armor_class)
{
	dtu_attack a;
	int roll = rand() % 20 + 1, result;

	dtu_char_get_attack(&a);
	// 0x2AB66: a backstab adds 4
	if (Dtu_backstab)
		a.to_hit_bonus += 4;
	// 0x2A87B: the Potion of Luck makes one roll a natural 20 when the
	// day clock (raw fix) is odd. Then the luck ends (0xAE564: sound 0x8A8).
	// The bit depends on the frame times (at a fixed frame rate with an
	// even frame time it is never odd); corrected: a coin flip.
	if (Potion.luck_end && (Dtu_fix_luck_coin ? (rand() & 1) : (GameTime & 1))) {
		roll = 20;
		Potion.luck_end = 0;
		digi_play_sample(0x8A8, F1_0);
		dtu_char_log("attack roll: the Potion of Luck gives a natural 20");
	}
	result = d20_result(roll, a.to_hit_bonus, a.thac0, target_armor_class);
	Dtu_last_attack_roll = roll;
	if (result == 2) HUD_init_message("You made a critical hit!");
	if (result == -1) {
		int r2 = rand() % 20 + 1;

		HUD_init_message("You made a critical fumble!");
		// 0x2A8B1: a second d20 above the dexterity: the weapon falls
		if (r2 > Dtu_char.pa.dexterity) {
			dtu_char_log("attack roll: fumble, d20 %d above dexterity %d: the weapon falls", r2, Dtu_char.pa.dexterity);
			dtu_char_drop_weapon();
		}
	}
	dtu_char_log("attack roll: d20 %d + %d against THAC0 %d and armor class %d: %d%s", roll, a.to_hit_bonus, a.thac0, target_armor_class, result,
		Dtu_backstab ? " (backstab)" : "");
	return result;
}

int dtu_char_damage_roll(void)
{
	dtu_attack a;
	int i, sum = 0;

	dtu_char_get_attack(&a);
	for (i = 0; i < a.dice_n; i++)
		if (a.dice_sides > 0)
			sum += rand() % a.dice_sides + 1;
	// 0x2AF5E: a backstab multiplies the dice by (thief level + 7) / 4
	// (level from 1); weapon 45 with a hit roll of 19 or 20 adds 1d4 + 3
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
	sum += a.damage_bonus;
	if (sum < 0) sum = 0;
	dtu_char_log("damage roll: %dd%d + %d: %d", a.dice_n, a.dice_sides, a.damage_bonus, sum);
	return sum;
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
// armspell=SPELL, give=TYPE.ID, xp=N, rest,
// gorest (puts the player into the first rest place of the level), mode=N,
// dump.

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
	for (i = 0; i < Dtu_n_inventory; i++)
		dtu_char_log("  entry %d: type %d id %d '%s' count %d%s%s", i, Dtu_inventory[i].type, Dtu_inventory[i].id,
			Dtu_inventory[i].name, Dtu_inventory[i].count, Dtu_char.current.n == i ? " (in the hand)" : "",
			dtu_char_slot_of(i) >= 0 ? " (worn)" : "");
}

static void run_command(char *cmd, char *value)
{
	int a = 0, b = 0, index;

	if (value)
		sscanf(value, "%d.%d", &a, &b);
	index = find_entry(a, b);
	dtu_char_log("script: %s %s", cmd, value ? value : "");
	if (!strcmp(cmd, "damage"))			dtu_damage_player(ConsoleObject, NULL, i2f(a));
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
	else if (!strcmp(cmd, "fx"))			dtu_spell_set_effect(ConsoleObject, 1 << a, i2f(b), 0, 1);
	else if (!strcmp(cmd, "cast"))		dtu_spell_cast_on(a, ConsoleObject, ConsoleObject);
	else if (!strcmp(cmd, "xp"))			dtu_char_award_xp(a);
	else if (!strcmp(cmd, "rest"))		dtu_char_rest();
	else if (!strcmp(cmd, "restreset"))	memset(Rest_time, 0, sizeof(Rest_time));		// test: the rest places work again
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
	else if (!strcmp(cmd, "mode"))		dtu_display_select_mode(a);
	else if (!strcmp(cmd, "dump"))		dump_character();
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
	Dtu_player_portrait = Dtu_char.portrait;
	sync_inventory();
	dtu_char_update();
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
