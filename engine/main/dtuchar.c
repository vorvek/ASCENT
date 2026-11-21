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

static FILE *Char_log = NULL;
static int Char_options_read = 0;
static char *Char_save_file = NULL;
static char *Char_script = NULL;
static int Char_frame = 0;
static int Known_n_inventory = 0;

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

	// the item in the hand (0xB0356)
	e = ref_entry(Dtu_char.current);
	if (e && (e->type == OBJ_WEAPON || e->type == OBJ_MISC))
		sum += table_ac(e);

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

static void ref_removed(dtu_item_ref *ref, int index)
{
	if (ref->n == index) ref->n = -1;
	else if (ref->n > index) ref->n--;
}

static void ref_check(dtu_item_ref *ref, int unused)
{
	(void) unused;
	ref->list = 0;
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
		if (Dtu_char.hot_key[i].n == index)
			return;
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		if (Dtu_char.hot_key[i].n < 0) {
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
		dtu_char_update();
		return;
	}
	if (!entry_data(index, &d) || !d.hot_key_ok)
		return;
	if (Dtu_inventory[index].type == OBJ_WEAPON && d.class_mask && !(d.class_mask & class_use_bits())) {
		HUD_init_message("%s", game_text(825, "You cannot use this object."));		// guess, see dtu_char_equip()
		return;
	}
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
		Dtu_char.hot_key[key].n = -1;
		return;
	}
	if (!entry_data(index, &d) || !d.hot_key_ok)
		return;
	Dtu_char.hot_key[key].list = 0;
	Dtu_char.hot_key[key].n = index;
}

void dtu_char_select_hot_key(int key)
{
	if (key < 0 || key >= DTU_N_HOT_KEYS || !ref_entry(Dtu_char.hot_key[key]))
		return;
	dtu_char_set_current(Dtu_char.hot_key[key].n);
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
// Use (0x37A6A). The executable has one case for each item number. The
// port has the two healing potions.

int dtu_char_use(int index)
{
	dtu_inv_item *e;
	int before, n;

	if (index < 0 || index >= Dtu_n_inventory)
		return 0;
	e = &Dtu_inventory[index];
	if (e->type != OBJ_MISC)
		return 0;
	if (Dtu_player_dead)
		return 0;

	before = dtu_char_hit_points();
	switch (e->id) {
		case 79:		// Potion of Healing (0x38E27): 4 to 10 hit points
			if (before < dtu_char_max_hit_points()) {
				n = dtu_char_heal(rand() % 7 + 4);
				HUD_init_message("You feel better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, before, dtu_char_hit_points(), dtu_char_max_hit_points());
			}
			else
				dtu_char_log("use: '%s', no effect at %d of %d hit points", e->name, before, dtu_char_max_hit_points());
			break;
		case 75:		// Potion of Extra-Healing (0x38C1A): 6 to 27 hit points
			if (before < dtu_char_max_hit_points()) {
				n = dtu_char_heal(rand() % 22 + 6);
				HUD_init_message("You feel much better.");
				dtu_char_log("use: '%s', %d hit points, %d -> %d of %d", e->name, n, before, dtu_char_hit_points(), dtu_char_max_hit_points());
			}
			else
				dtu_char_log("use: '%s', no effect at %d of %d hit points", e->name, before, dtu_char_max_hit_points());
			break;
		default:
			// ASCENT: the effect of this item is not in the port. The item
			// stays in the inventory.
			dtu_char_log("use: '%s' (misc %d) is not in the port", e->name, e->id);
			return 0;
	}
	// 0xB7084: one item of the entry is used up
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
	dtu_spell_restore_slots();		// 0x36049 calls 0x3551B
	// The executable also calls 0x160AB(player, 5) and 0x92FF2, plays sound
	// 62 and removes the fire of the rest place. These are not in the port.
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
	result = d20_result(roll, a.to_hit_bonus, a.thac0, target_armor_class);
	if (result == 2) HUD_init_message("You made a critical hit!");
	if (result == -1) HUD_init_message("You made a critical fumble!");
	dtu_char_log("attack roll: d20 %d + %d against THAC0 %d and armor class %d: %d", roll, a.to_hit_bonus, a.thac0, target_armor_class, result);
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
	Dtu_char.current.n = -1;
	for (i = 0; i < DTU_N_HOT_KEYS; i++) Dtu_char.hot_key[i].n = -1;
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

// SG0.SG: the player record is at 141, the attributes without items at
// 7108 (docs/formats/misc.md).
int dtu_char_load_save(char *filename)
{
	FILE *f;
	ubyte *buf, *p;
	int size, n, i, map[128];
	dtu_item_record rec;

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

	n = get_s32(p + DTU_P_N_ITEMS);
	if (n < 0) n = 0;
	if (n > 127) n = 127;
	for (i = 0; i < 128; i++) map[i] = -1;
	for (i = 0; i < n; i++) {
		memcpy(&rec, p + DTU_P_ITEMS + 16 * i, 16);
		map[i] = dtu_inventory_add(rec.type, rec.id, rec.count > 0 ? rec.count : 1);
		if (map[i] >= 0)
			Dtu_inventory[map[i]].level = rec.level;
	}
	for (i = 0; i < Dtu_n_inventory; i++)
		entry_name(i);
	Known_n_inventory = Dtu_n_inventory;
	load_ref(&Dtu_char.current, p + DTU_P_CURRENT, map, n);
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		load_ref(&Dtu_char.hot_key[i], p + DTU_P_HOT_KEYS + 2 * i, map, n);
	for (i = 0; i < 10; i++)
		load_ref(&Dtu_char.slot[i], p + DTU_P_SLOTS + 2 * i, map, n);
	load_ref(&Dtu_char.slot[DTU_SLOT_BOW], p + DTU_P_BOW, map, n);
	load_ref(&Dtu_char.slot[DTU_SLOT_CROSSBOW], p + DTU_P_CROSSBOW, map, n);
	free(buf);

	dtu_char_update();
	dtu_char_log("character '%s' from %s: class %d/%d level %d/%d, %d of %d hit points, armor class %d, THAC0 %d, %d items",
		Dtu_char.name, filename, Dtu_char.pa.class1, Dtu_char.pa.class2, Dtu_char.pa.class1_level + 1, Dtu_char.pa.class2_level + 1,
		Dtu_char.pa.hit_points, Dtu_char.pa.max_hit_points, Dtu_char.pa.armor_class, Dtu_char.thac0, Dtu_n_inventory);
	return 1;
}

static void pack_ref(ubyte *p, dtu_item_ref ref)
{
	p[0] = 0;
	p[1] = (ubyte) ref.n;
}

void dtu_char_pack(ubyte *record)
{
	int i;
	dtu_item_record rec;
	dtu_item_data d;

	memset(record, 0, DTU_PLAYER_RECORD_SIZE);
	strncpy((char *) record + DTU_P_NAME, Dtu_char.name, 15);
	put_s32(record + DTU_P_HIT_POINTS, Players[Player_num].shields);
	put_s32(record + DTU_P_EXPERIENCE, Dtu_char.experience[0]);
	put_s32(record + DTU_P_EXPERIENCE + 4, Dtu_char.experience[1]);
	Dtu_char.pa.gold = Dtu_gold;
	memcpy(record + DTU_P_ATTR, &Dtu_char.pa, sizeof(dtu_player_attr));
	put_s32(record + DTU_P_N_ITEMS, Dtu_n_inventory);
	for (i = 0; i < 127; i++) {
		memset(&rec, 0, sizeof(rec));
		rec.letter = '_';						// an empty entry of the executable
		rec.level = 1;
		if (i < Dtu_n_inventory && entry_data(i, &d)) {
			rec.type = Dtu_inventory[i].type;
			rec.id = Dtu_inventory[i].id;
			rec.icon = d.icon;
			rec.class_mask = d.class_mask;
			rec.level = Dtu_inventory[i].level;
			rec.count = Dtu_inventory[i].count;
			rec.modifier = F1_0;
		}
		memcpy(record + DTU_P_ITEMS + 16 * i, &rec, 16);
	}
	record[DTU_P_THAC0] = Dtu_char.thac0;
	pack_ref(record + DTU_P_CURRENT, Dtu_char.current);
	for (i = 0; i < DTU_N_HOT_KEYS; i++)
		pack_ref(record + DTU_P_HOT_KEYS + 2 * i, Dtu_char.hot_key[i]);
	for (i = 0; i < 10; i++)
		pack_ref(record + DTU_P_SLOTS + 2 * i, Dtu_char.slot[i]);
	pack_ref(record + DTU_P_BOW, Dtu_char.slot[DTU_SLOT_BOW]);
	pack_ref(record + DTU_P_CROSSBOW, Dtu_char.slot[DTU_SLOT_CROSSBOW]);
	put_s32(record + DTU_P_PORTRAIT, Dtu_char.portrait);
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
// current=TYPE.ID, hotkey=KEY, sethot=KEY.TYPE.ID, give=TYPE.ID, xp=N, rest,
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
	else if (!strcmp(cmd, "give"))		dtu_inventory_add(a, b, 1);
	else if (!strcmp(cmd, "xp"))			dtu_char_award_xp(a);
	else if (!strcmp(cmd, "rest"))		dtu_char_rest();
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

	max = i2f(Dtu_char.pa.max_hit_points);
	if (Players[Player_num].shields > max)
		set_hit_points(Dtu_char.pa.max_hit_points);
	Dtu_char.pa.hit_points = dtu_char_hit_points();
	Dtu_char.pa.gold = Dtu_gold;
	sync_inventory();
	if (Char_script)
		run_script();
}
