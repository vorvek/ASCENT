/*
 * ASCENT: new file. New gear and more gold and gems (Dtu_new_gear). See
 * dtugear.h and docs/re/objects.md, "New gear". Not in DTUGAME.EXE 1.3.
 *
 * The tables of this file: the new table lines (Gear_lines), the wands
 * (Gear_wands), the values of two weapon lines (Gear_values), the goods of
 * the market by reward stage (Gear_market), the goods that the setting takes
 * out of a 1.3 reward stage (Gear_market_removed), the treasury of
 * Tamaramagil (Gear_dragon) and the named people that leave no gold
 * (Gear_no_gold). tools/gear_income.py has the same drop rule: keep the two
 * the same.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "weapon.h"
#include "dtuobj.h"
#include "dtuobj_int.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtutblfix.h"
#include "dtugear.h"
#include "gauges.h"		// HUD_init_message
#include "dtulang.h"		// LN, LT

int Dtu_new_gear = 1;
int Dtu_gear_loaded = 0;
int Dtu_gear_mirror_worn = 0;
int Dtu_gear_displace_worn = 0;
int Dtu_gear_sash_worn = 0;

//------------------------------------------------------------------------
// The table lines. The pictures and the icons are those of the items of the
// same kind in every mission table (the archives of the mission levels have
// gensheld, genboots, genguant, gencloak, genbelt, genring, genpmail,
// gengem, gengem2 and gengem3). The class words are those of the nearest
// line: the Shield +1 (12), the boots (34 to 37), the Gauntlets of
// Dexterity (50), the cloaks, the Girdle of Hill Giant Strength (38), the
// rings, the Plate Mail +2 (10). The effects are in dtuchar.c (by number).

static const char *Gear_lines[] = {
	"$ARMOR name=\"Mirror Shield\" picture=gensheld.bbm icon=9 value=9000 armor_class_decrement=2 weight=5 shield magic magic_ok thief_ok stackable stack_limit=10 fighter_use thief_use cleric_use",
	"$ARMOR name=\"Boots of Free Action\" picture=genboots.bbm icon=157 value=3500 pc_spell=20 weight=.5 boot magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Gloves of the Archer\" picture=genguant.bbm icon=158 value=3500 weight=.5 gauntlet magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Cloak of Fire Resistance\" picture=gencloak.bbm icon=151 value=3000 pc_spell=43 weight=.5 cloak magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Belt of Dwarvenkind\" picture=genbelt.bbm icon=155 value=7500 set_constitution=18 weight=.5 belt magic magic_ok thief_ok cleric_use fighter_use thief_use stackable stack_limit=10",
	"$ARMOR name=\"Ring of Regeneration\" picture=genring.bbm icon=138 value=15000 weight=.5 ring magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Plate Mail of Fire Resistance\" picture=genpmail.bbm icon=6 value=7500 armor_class_decrement=9 weight=15 armor magic stackable stack_limit=10 fighter_use cleric_use",
	"$ARMOR name=\"Cloak of Displacement\" picture=gencloak.bbm icon=151 value=18000 weight=.5 cloak magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Sash of the Magi\" picture=genbelt.bbm icon=153 value=22000 weight=.5 belt magic magic_ok stackable stack_limit=10 mage_use",
	"$OBJECT name=\"Bloodstone\" mass=5 value=50 all_use icon=131 picture=gengem2.bbm stackable stack_limit=10 non_hotkey_item",
	"$OBJECT name=\"Pearl\" mass=5 value=100 all_use icon=129 picture=gengem3.bbm stackable stack_limit=10 non_hotkey_item",
	"$OBJECT name=\"Star Sapphire\" mass=5 value=5000 all_use icon=130 picture=gengem.bbm stackable stack_limit=10 non_hotkey_item",
};
#define N_GEAR_LINES	((int) (sizeof(Gear_lines) / sizeof(Gear_lines[0])))

// The armor class of the new armor lines 53 to 59 (as Armor_ac of dtuchar.c)
static const signed char Gear_armor_ac[DTU_GEAR_N_ARMOR] = { -2, 0, 0, 0, 0, 0, -9, 0, 0 };

// The Frost Brand: the line of 1.3 (its name there), the line it copies
// (the Longsword +3: picture, model, sounds, dice, classes), the new name
// and the value
static const struct { int id; const char *name13; int from; const char *from13, *name; int value; } Gear_frost = {
	DTU_GEAR_FROST_BRAND, "Rod of Genericness", DTU_GEAR_FROST_FROM, "Longsword +3", "Frost Brand", 20000 };

// The wands: weapons 63 to 67 are "Wand of Genericness" (63 to 65) and
// "Rod of Genericness" (66, 67) in each mission table, with no spell
// (enemy_spell=-1). Each gets a spell of the mage list, the class of a
// mage (as the Wand of Confusion, 56) and 6 charges (uses=6 of the line).
static const struct { int id; const char *name13, *name; int spell; } Gear_wands[DTU_GEAR_N_WANDS] = {
	{ 63, "Wand of Genericness", "Wand of Sleep", 46 },
	{ 64, "Wand of Genericness", "Wand of Slow", 47 },
	{ 65, "Wand of Genericness", "Wand of Hold Undead", 23 },
	{ 66, "Rod of Genericness", "Rod of Minute Meteors", 37 },
	{ 67, "Rod of Genericness", "Rod of Dispel Magic", 13 },
};

// The values of the user's table (2026-11-27): the Mace of Disruption (70,
// 17500 in 1.3) and the Flame Tongue (71, 4500 in 1.3)
static const struct { int id; const char *name13; int value; } Gear_values[] = {
	{ 70, "Mace of Disruption", 4500 },
	{ 71, "Flame Tongue", 6000 },
};
#define N_GEAR_VALUES	((int) (sizeof(Gear_values) / sizeof(Gear_values[0])))

//------------------------------------------------------------------------
// The market: the goods that a reward of Khelben adds (stage 0 to 8, as
// Mk_reward_stock of dtufe_town.c): stage, quantity, class, number, count
// of one item (-1: the uses of the line). The stages: 1 missions 3 and 4,
// 2 missions 7 and 8, 3 missions 9 and 10, 4 mission 11, 5 mission 14,
// 6 mission 16, 7 missions 20 and 21, 8 mission 29 (docs/re/scripts.md,
// section 8).
//
// The new items of the user's table, and the magic items of 1.3 that have
// rules in the code but that no level places and no stage sells, weaker
// ones first. Left out: the Arrows +1 and +2 (74, 75): their lines have no
// value, so the market would give them away.

static const short Gear_market[][5] = {
	// stage 1
	{ 1, 1, 15, DTU_GEAR_ARCHER, -1 },
	{ 1, 1, 5, 63, -1 },				// Wand of Sleep
	// stage 2
	{ 2, 1, 5, 41, -1 },				// Club +1
	{ 2, 10, 5, 50, 1 },				// Darts +1
	{ 2, 1, 15, 17, -1 },				// Ring of Coolness
	{ 2, 1, 15, 22, -1 },				// Ring of Infravision
	{ 2, 2, 16, 83, -1 },				// Potion of Invisibility
	{ 2, 1, 16, 81, -1 },				// Potion of Hill Giant Intelligence
	// stage 3
	{ 3, 1, 5, 64, -1 },				// Wand of Slow
	{ 3, 10, 5, 51, 1 },				// Darts +2
	{ 3, 6, 5, 48, 1 },				// Bolts +2
	{ 3, 1, 15, 51, -1 },				// Glim-Gauntlet
	{ 3, 1, 5, 60, -1 },				// Wand of Magic Detection
	{ 3, 1, 16, 73, -1 },				// Potion of Anti-Magic
	// stage 4
	{ 4, 1, 15, DTU_GEAR_BOOTS_FREE, -1 },
	{ 4, 1, 15, DTU_GEAR_CLOAK_FIRE, -1 },
	{ 4, 1, 5, 65, -1 },				// Wand of Hold Undead
	{ 4, 1, 15, 25, -1 },				// Ring of Quickness
	{ 4, 1, 5, 56, -1 },				// Wand of Confusion
	// stage 5
	{ 5, 1, 15, 20, -1 },				// Ring of Free Action
	{ 5, 1, 15, 30, -1 },				// Amulet of Protection +3
	{ 5, 1, 15, 44, -1 },				// Chameleon Cloak
	// stage 6
	{ 6, 1, 15, DTU_GEAR_MIRROR_SHIELD, -1 },
	{ 6, 1, 5, 66, -1 },				// Rod of Minute Meteors
	{ 6, 1, 15, 15, -1 },				// Ring of Protection +3
	{ 6, 1, 16, 99, -1 },				// Crossbow +3
	// stage 7
	{ 7, 1, 5, 67, -1 },				// Rod of Dispel Magic
	{ 7, 1, 5, 71, -1 },				// Flame Tongue (stage 5 in 1.3, Gear_market_removed)
	{ 7, 1, 15, 31, -1 },				// Amulet of Protection +4
	{ 7, 1, 15, 43, -1 },				// Cloak of Elvenkind
	{ 7, 1, 15, 46, -1 },				// Elven Cloak
	// stage 8
	{ 8, 1, 15, DTU_GEAR_RING_REGEN, -1 },
	{ 8, 1, 15, 36, -1 },				// Boots of Levitation
};
#define N_GEAR_MARKET	((int) (sizeof(Gear_market) / sizeof(Gear_market[0])))

// The goods of a 1.3 reward stage that the setting takes out (stage,
// class, number). The Flame Tongue of stage 5 (mission 14) goes to stage 7:
// the player finds one in DROW2 first (dtu_gear_place_flame_tongue). A
// stock file that took reward 5 before keeps its Flame Tongue.
static const short Gear_market_removed[][3] = {
	{ 5, 5, 71 },
};
#define N_GEAR_REMOVED	((int) (sizeof(Gear_market_removed) / sizeof(Gear_market_removed[0])))

// The treasury of Tamaramagil (DROW2): class, number, count
static const short Gear_dragon[][3] = {
	{ 5, DTU_GEAR_FROST_BRAND, 1 },
	{ 15, DTU_GEAR_CLOAK_DISPLACE, 1 },
	{ 15, DTU_GEAR_SASH_MAGI, 1 },
	{ 15, DTU_GEAR_BELT_DWARF, 1 },
	{ 15, DTU_GEAR_RING_REGEN, 1 },
	{ 15, DTU_GEAR_PLATE_FIRE, 1 },
};
#define N_GEAR_DRAGON	((int) (sizeof(Gear_dragon) / sizeof(Gear_dragon[0])))

// The named people (the talk characters): no gold. By the name= of the
// $MONSTER line. Only those with a moola above 0 matter: the people of
// YPINN.
static const char *Gear_no_gold[] = {
	"durnan", "mirt", "aleena", "drowfem", "flaynpc", "fmtnpc", "priestes", "daland", "thashasa",
	"halaster", "ibbalar", "jeryth", "avatar",
};
#define N_NO_GOLD	((int) (sizeof(Gear_no_gold) / sizeof(Gear_no_gold[0])))

//------------------------------------------------------------------------
// The tables

static void put_text(const dtu_tblfix_io *io, int kind, int id, char *text)
{
	if (text) {
		io->set_text(kind, id, text);
		if (io->sync)
			io->sync(kind, id);
	}
}

// The weapon name of a line (weapon_name=, or the name buffer)
static int weapon_named(const dtu_tblfix_io *io, int id, const char *name)
{
	char v[64], *t = io->text(DTU_TK_WEAPON, id);

	if (!t)
		return 0;
	if (dtu_tblfix_get(t, "weapon_name", v, sizeof(v)))
		return !strcmp(v, name);
	return io->name(DTU_TK_WEAPON, id) && !strcmp(io->name(DTU_TK_WEAPON, id), name);
}

void dtu_gear_tables(const dtu_tblfix_io *io, void (*parse)(char *text))
{
	char buf[64];
	int i;

	Dtu_gear_loaded = 0;
	if (!Dtu_new_gear)
		return;
	// the mission tables: 53 armor lines, 128 misc lines, the generic wands
	if (io->count(DTU_TK_ARMOR) != DTU_GEAR_FIRST_ARMOR || io->count(DTU_TK_MISC) != DTU_GEAR_FIRST_MISC) {
		if (io->log) {
			snprintf(buf, sizeof(buf), "New gear: not this table (%d armor, %d misc lines)", io->count(DTU_TK_ARMOR), io->count(DTU_TK_MISC));
			io->log(buf);
		}
		return;
	}
	for (i = 0; i < DTU_GEAR_N_WANDS; i++)
		if (Gear_wands[i].id >= io->count(DTU_TK_WEAPON) || !weapon_named(io, Gear_wands[i].id, Gear_wands[i].name13)) {
			if (io->log)
				io->log("New gear: not this table (the generic wands)");
			return;
		}
	if (Gear_frost.id >= io->count(DTU_TK_WEAPON) || !weapon_named(io, Gear_frost.id, Gear_frost.name13) ||
			!weapon_named(io, Gear_frost.from, Gear_frost.from13)) {
		if (io->log)
			io->log("New gear: not this table (the lines of the Frost Brand)");
		return;
	}

	// The Frost Brand: all decoded fields of the Longsword +3 (copy_line
	// keeps the record and the name), then its text with the new name and
	// value
	{
		char *name = io->name(DTU_TK_WEAPON, Gear_frost.id), *t;

		io->copy_line(DTU_TK_WEAPON, Gear_frost.id, Gear_frost.from);
		snprintf(buf, sizeof(buf), "\"%s\"", Gear_frost.name);
		t = dtu_tblfix_set(io->text(DTU_TK_WEAPON, Gear_frost.from), "weapon_name", buf);
		put_text(io, DTU_TK_WEAPON, Gear_frost.id, t);
		snprintf(buf, sizeof(buf), "%d", Gear_frost.value);
		put_text(io, DTU_TK_WEAPON, Gear_frost.id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, Gear_frost.id), "value", buf));
		if (name && io->name_size > 0) {
			strncpy(name, Gear_frost.name, io->name_size - 1);
			name[io->name_size - 1] = 0;
		}
	}

	// Each write gives a new text or NULL (no change), as the writes of the
	// fixup (dtutblfix.h).
	for (i = 0; i < DTU_GEAR_N_WANDS; i++) {
		int id = Gear_wands[i].id;
		char *name = io->name(DTU_TK_WEAPON, id);

		snprintf(buf, sizeof(buf), "\"%s\"", Gear_wands[i].name);
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, id), "weapon_name", buf));
		snprintf(buf, sizeof(buf), "%d", Gear_wands[i].spell);
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, id), "PC_spell", buf));
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, id), "enemy_spell", NULL));
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_word(io->text(DTU_TK_WEAPON, id), "all_use", 0));
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_word(io->text(DTU_TK_WEAPON, id), "mage_use", 1));
		put_text(io, DTU_TK_WEAPON, id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, id), "value", "3000"));
		// the name buffer of the line (the reader keeps weapon_name there)
		if (name && io->name_size > 0) {
			strncpy(name, Gear_wands[i].name, io->name_size - 1);
			name[io->name_size - 1] = 0;
		}
	}
	for (i = 0; i < N_GEAR_VALUES; i++)
		if (Gear_values[i].id < io->count(DTU_TK_WEAPON) && weapon_named(io, Gear_values[i].id, Gear_values[i].name13)) {
			snprintf(buf, sizeof(buf), "%d", Gear_values[i].value);
			put_text(io, DTU_TK_WEAPON, Gear_values[i].id, dtu_tblfix_set(io->text(DTU_TK_WEAPON, Gear_values[i].id), "value", buf));
		}

	// The new lines go through the reader of the tables (bm_parse_text),
	// one line at a time: armor 53 to 59, misc 128 to 130.
	for (i = 0; i < N_GEAR_LINES; i++) {
		char *line = malloc(strlen(Gear_lines[i]) + 2);

		if (!line)
			return;
		sprintf(line, "%s\n", Gear_lines[i]);
		parse(line);
		free(line);
	}
	if (io->count(DTU_TK_ARMOR) != DTU_GEAR_FIRST_ARMOR + DTU_GEAR_N_ARMOR || io->count(DTU_TK_MISC) != DTU_GEAR_FIRST_MISC + DTU_GEAR_N_MISC) {
		if (io->log)
			io->log("New gear: the new lines did not go in");
		return;
	}
	Dtu_gear_loaded = 1;
	if (io->log)
		io->log("New gear: armor 53 to 61, misc 128 to 130, wands 63 to 67, the Frost Brand 68");
}

int dtu_gear_armor_ac(int id)
{
	if (id >= DTU_GEAR_FIRST_ARMOR && id < DTU_GEAR_FIRST_ARMOR + DTU_GEAR_N_ARMOR)
		return Gear_armor_ac[id - DTU_GEAR_FIRST_ARMOR];
	return 0;
}

//------------------------------------------------------------------------
// Gold and gems

// A random number 0 to n - 1 (n > 0)
static int roll(int n)
{
	return n > 0 ? rand() % n : 0;
}

int dtu_gear_gold(int moola, const char *name)
{
	int i;

	if (moola <= 0)
		return 0;
	for (i = 0; i < N_NO_GOLD; i++)
		if (name && !stricmp(name, Gear_no_gold[i]))
			return 0;
	// 6 in 10: moola + 1d(2 x moola)
	if (roll(10) >= DTU_GEAR_GOLD_IN_10)
		return 0;
	return moola + roll(DTU_GEAR_GOLD_DICE * moola) + 1;
}

int dtu_gear_gem(int gem_class, int xp)
{
	// 1 in 2
	if (!gem_class || roll(DTU_GEAR_GEM_ONE_IN) != 0)
		return -1;
	if (xp < DTU_GEAR_GEM_XP_PEARL)
		return DTU_GEAR_BLOODSTONE;
	if (xp < DTU_GEAR_GEM_XP_RUBY)
		return roll(4) < DTU_GEAR_PEARL_IN_4 ? DTU_GEAR_PEARL : 4;			// 4: Ruby
	if (xp < DTU_GEAR_GEM_XP_DIAMOND)
		return roll(3) < DTU_GEAR_RUBY_IN_3 ? 4 : 3;							// 3: Diamond
	return roll(DTU_GEAR_SAPPHIRE_ONE_IN) == 0 ? DTU_GEAR_STAR_SAPPHIRE : 3;
}

int dtu_gear_gem_class(int type)
{
	int rec, wt;
	char v[32];

	if (type < 0 || type >= N_robot_types)
		return 0;
	if (Monster_info[type].flags & (MONSTER_F_UNDEAD | MONSTER_F_HUMANOID))
		return 1;
	if (Monster_ext[type].door_opening || Monster_ext[type].person)
		return 1;
	// spell casters (a spell level, a spell as the weapon) and the eye rays
	// of the beholders and the spectators (weapons 81 to 88)
	rec = Monster_info[type].record;
	wt = (rec >= 0 && bm_tbl_get(rec, "weapon_type", v, sizeof(v))) ? atoi(v) : 0;
	if (Monster_ext[type].spell_level > 0 || wt < 0 || (wt >= 81 && wt <= 88))
		return 1;
	return 0;
}

//------------------------------------------------------------------------
// The market and the treasury

int dtu_gear_market_stock(int stage, void (*f)(int quantity, int type, int id, int count))
{
	int i, n = 0;

	if (!Dtu_new_gear)
		return 0;
	for (i = 0; i < N_GEAR_MARKET; i++)
		if (Gear_market[i][0] == stage) {
			if (f)
				f(Gear_market[i][1], Gear_market[i][2], Gear_market[i][3], Gear_market[i][4]);
			n++;
		}
	return n;
}

int dtu_gear_market_removed(int stage, int type, int id)
{
	int i;

	if (!Dtu_new_gear)
		return 0;
	for (i = 0; i < N_GEAR_REMOVED; i++)
		if (Gear_market_removed[i][0] == stage && Gear_market_removed[i][1] == type && Gear_market_removed[i][2] == id)
			return 1;
	return 0;
}

int dtu_gear_dragon_treasure(void (*f)(int type, int id, int count))
{
	int i;

	if (!Dtu_new_gear)
		return 0;
	for (i = 0; i < N_GEAR_DRAGON; i++)
		if (f)
			f(Gear_dragon[i][0], Gear_dragon[i][1], Gear_dragon[i][2]);
	return N_GEAR_DRAGON;
}

//------------------------------------------------------------------------
// The Mace of Disruption of Daland (MYRKUL, mission 11): script 160 gives
// the Potion of Extra-Healing (misc 75) and 600 experience when trigger 31
// (segment 460) took the Potion of Dragon's Blood and the altar of Kelemvor
// is consecrated. The mace comes with the potion.

#define DALAND_SCRIPT	160
#define DALAND_POTION	75
#define MACE			70

void dtu_gear_script_give(int script, int type, int id)
{
	if (!Dtu_new_gear || script != DALAND_SCRIPT || type != OBJ_MISC || id != DALAND_POTION || MACE >= N_weapon_types)
		return;
	if (dtu_inventory_add(OBJ_WEAPON, MACE, 1) >= 0)
		dtu_obj_log("new gear: script %d gives weapon %d (Mace of Disruption) with misc %d", script, MACE, id);
}

//------------------------------------------------------------------------
// The Mirror Shield. "Front": the missile comes from at most 60 degrees
// off the view direction of the player (the cosine 0.5), as a shield held
// before the body covers it. It sends back a spell missile of a creature
// that is not an area spell (an area spell bursts at the shield: its area
// has no single way back), and the eye rays of the beholders and the
// spectators (weapons 81 to 88, missiles with no spell). Arrows, bolts,
// thrown weapons, the breath and the missiles of traps (no source) hit as
// before. The missile flies at its source at its own speed.

int dtu_gear_mirror(object *missile, object *player, int spell, int weapon, object *source)
{
	vms_vector back, to;
	fix speed;

	if (!Dtu_gear_mirror_worn || !source || source->type != OBJ_ROBOT || !player || player->type != OBJ_PLAYER)
		return 0;
	if (spell >= 0 ? dtu_spell_is_area(spell) : !(weapon >= 81 && weapon <= 88))
		return 0;
	back = missile->mtype.phys_info.velocity;
	speed = vm_vec_normalize_quick(&back);
	if (speed <= 0)
		return 0;
	vm_vec_negate(&back);
	if (vm_vec_dot(&back, &player->orient.fvec) < F1_0 / 2)
		return 0;
	vm_vec_sub(&to, &source->pos, &missile->pos);
	if (vm_vec_normalize_quick(&to) <= 0)
		to = back;
	vm_vec_copy_scale(&missile->mtype.phys_info.velocity, &to, speed);
	dtu_obj_log("new gear: the Mirror Shield sends obj %d (weapon %d spell %d) back to obj %d %s", (int) (missile - Objects), weapon, spell,
		(int) (source - Objects), dtu_object_name(source));
	return 1;
}

//------------------------------------------------------------------------
// The treasury items of Tamaramagil: the Frost Brand and the Cloak of
// Displacement (the Sash of the Magi is in dtuchar.c and dtuspell.c)

int dtu_gear_frost_brand(int weapon)
{
	return Dtu_gear_loaded && weapon == DTU_GEAR_FROST_BRAND;
}

int dtu_gear_fire_creature(int type)
{
	int rec, wt;
	char v[32];

	if (type < 0 || type >= N_robot_types)
		return 0;
	if (Monster_info[type].flags & (MONSTER_F_FIRE_IMMUNE | MONSTER_F_FIRE_RESISTANT))
		return 1;
	// a fire spell as the weapon: Burning Hands (4), Fireball (16), Produce
	// Fire (17), Melf's Minute Meteors (37)
	rec = Monster_info[type].record;
	wt = (rec >= 0 && bm_tbl_get(rec, "weapon_type", v, sizeof(v))) ? atoi(v) : 0;
	return wt == -4 || wt == -16 || wt == -17 || wt == -37;
}

// The time when the Cloak of Displacement is ready again (0: ready). It is
// not saved: a loaded game starts with the cloak ready.
static fix Displace_ready;
static int Displace_waiting;

int dtu_gear_displace(object *player, object *attacker)
{
	if (!Dtu_gear_loaded || !Dtu_gear_displace_worn || !player || player->type != OBJ_PLAYER)
		return 0;
	if (Displace_waiting && !dtu_time_past(Displace_ready))
		return 0;
	Displace_ready = GameTime + i2f(DTU_GEAR_DISPLACE_TIME);
	Displace_waiting = 1;
	HUD_init_message(LN("Your Cloak of Displacement fools %s: the blow misses."), attacker ? dtu_object_display_name(attacker) : LT("the attack"));
	dtu_obj_log("new gear: the Cloak of Displacement makes the attack of %s miss, ready again at %.1f",
		attacker ? dtu_object_name(attacker) : "a missile", f2fl(Displace_ready));
	dtu_char_log("Cloak of Displacement: the attack of %s misses", attacker ? dtu_object_name(attacker) : "a missile");
	return 1;
}

void dtu_gear_displace_frame(void)
{
	if (!Displace_waiting || !dtu_time_past(Displace_ready))
		return;
	Displace_waiting = 0;
	if (Dtu_gear_displace_worn) {
		HUD_init_message(LN("Your Cloak of Displacement shimmers again."));
		dtu_char_log("Cloak of Displacement: ready again");
	}
}
