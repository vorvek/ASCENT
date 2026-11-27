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
#include "args.h"			// FindArg
#include "dtuscript.h"		// dtu_script_gflag

int Dtu_new_gear = 1;
int Dtu_gear_loaded = 0;
int Dtu_class_gear = 1;			// the menu line (dtugear.h)
int Dtu_class_gear_loaded = 0;
static int Class_gear_arg = -1;	// -noclassgear and the others: 0 for this run
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

// The daggers: the line (-1 for Venom: a new line at the end of the table),
// its name in 1.3, the line they copy, the new name, the value, the bonus,
// thieves only
static const struct { int id; const char *name13; int from; const char *from13, *name; int value, bonus, thief; } Gear_daggers[2] = {
	{ DTU_GEAR_VENOM, "", DTU_GEAR_DAGGER_FROM, "Dagger +2", "Dagger of Venom +2", 9000, 2, 0 },
	{ DTU_GEAR_ASSASSIN, "Temp", DTU_GEAR_DAGGER_FROM, "Dagger +2", "Assassin's Dagger +3", 15000, 3, 1 },
};

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
// Mk_reward_stock of dtufe_town.c): stage, quantity, class (17: a spell
// scroll), number, count of one item (-1: the uses of the line). The stages: 1 missions 3 and 4,
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
	{ 4, 3, 17, 13, 1 },				// scrolls of Dispel Magic (the hold of Daland, the protections of the lich of CLAN3)
	// stage 5
	{ 5, 1, 15, 20, -1 },				// Ring of Free Action
	{ 5, 1, 15, 30, -1 },				// Amulet of Protection +3
	{ 5, 1, 15, 44, -1 },				// Chameleon Cloak
	// stage 6
	{ 6, 1, 15, DTU_GEAR_MIRROR_SHIELD, -1 },
	{ 6, 1, 5, 66, -1 },				// Rod of Minute Meteors
	{ 6, 1, 15, 15, -1 },				// Ring of Protection +3
	{ 6, 1, 16, 99, -1 },				// Crossbow +3
	{ 6, 3, 17, 13, 1 },				// scrolls of Dispel Magic
	// stage 7
	{ 7, 1, 5, 67, -1 },				// Rod of Dispel Magic
	{ 7, 1, 5, 71, -1 },				// Flame Tongue (stage 5 in 1.3, Gear_market_removed)
	{ 7, 1, 15, 31, -1 },				// Amulet of Protection +4
	{ 7, 1, 15, 43, -1 },				// Cloak of Elvenkind
	{ 7, 1, 15, 46, -1 },				// Elven Cloak
	{ 7, 1, 5, DTU_GEAR_ASSASSIN, -1 },	// Assassin's Dagger +3
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
// The class gear (Dtu_class_gear, dtugear.h). The lines after the new gear:
// armor 62 and 63, misc 131 to 133 (the reader of the tables) and the next free weapon line (a copy
// of the Arrow, below). The value of each line is the price of the seller,
// so that a sale to Geldor gives back at most what the player paid. The
// Pearls have their own icon (203, tools/spell_icons.py), not the one of the
// gem Pearl (129), and the picture of the Gem of Curing.
static const char *Class_lines[] = {
	"$ARMOR name=\"Boots of Elvenkind\" picture=genboots.bbm icon=157 value=5000 weight=.5 boot magic magic_ok thief_ok stackable stack_limit=10 all_use",
	"$ARMOR name=\"Bracers of Archery\" picture=genbracr.bbm icon=152 value=7500 weight=.5 gauntlet magic magic_ok thief_ok stackable stack_limit=10 fighter_use thief_use",
	"$OBJECT name=\"Pearl of Power I\" mass=5 value=2500 mage_use icon=203 picture=gengem.bbm stackable stack_limit=10",
	"$OBJECT name=\"Pearl of Power II\" mass=5 value=6000 mage_use icon=203 picture=gengem.bbm stackable stack_limit=10",
	"$OBJECT name=\"Pearl of Power III\" mass=5 value=12000 mage_use icon=203 picture=gengem.bbm stackable stack_limit=10",
};
#define N_CLASS_LINES	((int) (sizeof(Class_lines) / sizeof(Class_lines[0])))

// The Poisoned Arrow: the Arrow (7) with this name and value (the price of
// Nalbas, 160 gold for 8)
static const struct { int from; const char *from13, *name; int value; } Class_arrow = {
	DTU_GEAR_ARROW_FROM, "Arrow", "Poisoned Arrow", 20 };

// The goods of the market that the class gear adds (as Gear_market): any
// mage can buy them
static const short Class_market[][5] = {
	{ 2, 1, 5, 61, -1 },				// Wand of Paralyzation
	{ 4, 1, 5, 59, -1 },				// Wand of Lightning Bolts
};
#define N_CLASS_MARKET	((int) (sizeof(Class_market) / sizeof(Class_market[0])))

int dtu_class_gear_on(void)
{
	if (Class_gear_arg < 0)
		Class_gear_arg = !FindArg("-noclassgear") && !FindArg("-noimprovements") && !FindArg("-origitems") && !FindArg("-origscript");
	return Dtu_class_gear && Class_gear_arg;
}

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

// The next text of a chain of writes: the new text, or the old one when the
// write changed nothing (NULL)
static char *chain(char *old, char *new_text)
{
	if (!new_text)
		return old;
	dtu_tblfix_free(old);
	return new_text;
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

static void class_tables(const dtu_tblfix_io *io, void (*parse)(char *text));

// The weapon line of the Rod of Kelemvor in the tables of the level, -1:
// none (dtugear.h)
static int Rod_weapon = -1;
// The weapon line of the Poisoned Arrow of the class gear, -1: none
static int Arrow_weapon = -1;

// The Rod of Kelemvor: a new line at the end of the weapon table, a copy of
// the Rod of Dispel Magic (the last wand, after its writes) with its name,
// its value, one charge and all classes. As the Dagger of Venom: the line
// goes through the reader of the tables, then the decoded fields of the
// copy, the text and the name.
static void rod_line(const dtu_tblfix_io *io, void (*parse)(char *text))
{
	int from = Gear_wands[DTU_GEAR_N_WANDS - 1].id, id = io->count(DTU_TK_WEAPON);
	char buf[64], *u, *line, *name;

	snprintf(buf, sizeof(buf), "\"%s\"", DTU_GEAR_ROD_NAME);
	u = dtu_tblfix_set(io->text(DTU_TK_WEAPON, from), "weapon_name", buf);
	if (!u)
		return;
	snprintf(buf, sizeof(buf), "%d", DTU_GEAR_ROD_VALUE);
	u = chain(u, dtu_tblfix_set(u, "value", buf));
	u = chain(u, dtu_tblfix_set(u, "uses", "1"));
	u = chain(u, dtu_tblfix_word(u, "mage_use", 0));
	u = chain(u, dtu_tblfix_word(u, "all_use", 1));
	line = malloc(strlen(u) + 12);
	if (line) {
		sprintf(line, "$WEAPON %s\n", u);
		parse(line);
		free(line);
	}
	if (io->count(DTU_TK_WEAPON) == id + 1) {
		io->copy_line(DTU_TK_WEAPON, id, from);
		put_text(io, DTU_TK_WEAPON, id, u);
		u = NULL;
		name = io->name(DTU_TK_WEAPON, id);
		if (name && io->name_size > 0) {
			strncpy(name, DTU_GEAR_ROD_NAME, io->name_size - 1);
			name[io->name_size - 1] = 0;
		}
		Rod_weapon = id;
		if (io->log) {
			snprintf(buf, sizeof(buf), "New gear: the %s is weapon %d", DTU_GEAR_ROD_NAME, id);
			io->log(buf);
		}
	}
	if (u)
		dtu_tblfix_free(u);
}

void dtu_gear_tables(const dtu_tblfix_io *io, void (*parse)(char *text))
{
	char buf[64];
	int i;

	Dtu_gear_loaded = 0;
	Dtu_class_gear_loaded = 0;
	Arrow_weapon = -1;
	Rod_weapon = -1;
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
			!weapon_named(io, Gear_frost.from, Gear_frost.from13) || io->count(DTU_TK_WEAPON) != DTU_GEAR_VENOM ||
			!weapon_named(io, DTU_GEAR_ASSASSIN, Gear_daggers[1].name13) || !weapon_named(io, DTU_GEAR_DAGGER_FROM, Gear_daggers[0].from13)) {
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

	// The daggers: copies of the Dagger +2 with a name, a value and a bonus.
	// The Assassin's Dagger takes the place of "Temp" (copy_line), the
	// Dagger of Venom is a new line at the end of the table (the reader of
	// the tables).
	for (i = 0; i < 2; i++) {
		char *t, *u;

		snprintf(buf, sizeof(buf), "\"%s\"", Gear_daggers[i].name);
		// each write gives a new text, or NULL when nothing changes (chain)
		u = dtu_tblfix_set(io->text(DTU_TK_WEAPON, Gear_daggers[i].from), "weapon_name", buf);
		if (!u)
			continue;
		snprintf(buf, sizeof(buf), "%d", Gear_daggers[i].value);
		u = chain(u, dtu_tblfix_set(u, "value", buf));
		snprintf(buf, sizeof(buf), "%d", Gear_daggers[i].bonus);
		u = chain(u, dtu_tblfix_set(u, "to_hit_bonus", buf));
		u = chain(u, dtu_tblfix_set(u, "damage_bonus", buf));
		if (Gear_daggers[i].thief) {
			u = chain(u, dtu_tblfix_word(u, "Fighter_use", 0));
			u = chain(u, dtu_tblfix_word(u, "mage_use", 0));
		}
		if (Gear_daggers[i].id == DTU_GEAR_VENOM) {
			char *line = malloc(strlen(u) + 12);

			if (line) {
				sprintf(line, "$WEAPON %s\n", u);
				parse(line);
				free(line);
			}
			// the decoded fields of the Dagger +2 (the picture on the floor of
			// a $WEAPON line read here is another one), the text and the name
			if (io->count(DTU_TK_WEAPON) == DTU_GEAR_VENOM + 1) {
				char *name;

				io->copy_line(DTU_TK_WEAPON, DTU_GEAR_VENOM, Gear_daggers[i].from);
				put_text(io, DTU_TK_WEAPON, DTU_GEAR_VENOM, u);
				u = NULL;
				name = io->name(DTU_TK_WEAPON, DTU_GEAR_VENOM);
				if (name && io->name_size > 0) {
					strncpy(name, Gear_daggers[i].name, io->name_size - 1);
					name[io->name_size - 1] = 0;
				}
			}
			if (u)
				dtu_tblfix_free(u);
		}
		else {
			char *name = io->name(DTU_TK_WEAPON, Gear_daggers[i].id);

			io->copy_line(DTU_TK_WEAPON, Gear_daggers[i].id, Gear_daggers[i].from);
			put_text(io, DTU_TK_WEAPON, Gear_daggers[i].id, u);
			if (name && io->name_size > 0) {
				strncpy(name, Gear_daggers[i].name, io->name_size - 1);
				name[io->name_size - 1] = 0;
			}
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
	if (io->count(DTU_TK_ARMOR) != DTU_GEAR_FIRST_ARMOR + DTU_GEAR_N_ARMOR || io->count(DTU_TK_MISC) != DTU_GEAR_FIRST_MISC + DTU_GEAR_N_MISC ||
			io->count(DTU_TK_WEAPON) != DTU_GEAR_VENOM + 1) {
		if (io->log)
			io->log("New gear: the new lines did not go in");
		return;
	}
	Dtu_gear_loaded = 1;
	if (io->log)
		io->log("New gear: armor 53 to 61, misc 128 to 130, wands 63 to 67, the Frost Brand 68");
	rod_line(io, parse);
	class_tables(io, parse);		// ASCENT: the class gear: the next free lines
}

// The lines of the class gear, after the lines of the new gear
static void class_tables(const dtu_tblfix_io *io, void (*parse)(char *text))
{
	char buf[64], *u, *line, *name;
	int i, id;

	if (!dtu_class_gear_on() || !weapon_named(io, Class_arrow.from, Class_arrow.from13))
		return;
	// the Poisoned Arrow: a new line at the end of the weapon table (as the
	// Dagger of Venom and the Rod of Kelemvor), its number the next free one,
	// then the decoded fields, the text and the name
	id = io->count(DTU_TK_WEAPON);
	snprintf(buf, sizeof(buf), "\"%s\"", Class_arrow.name);
	if ((u = dtu_tblfix_set(io->text(DTU_TK_WEAPON, Class_arrow.from), "weapon_name", buf)) == NULL)
		return;
	u = chain(u, dtu_tblfix_set(u, "name", buf));
	snprintf(buf, sizeof(buf), "%d", Class_arrow.value);
	u = chain(u, dtu_tblfix_set(u, "value", buf));
	if ((line = malloc(strlen(u) + 12)) != NULL) {
		sprintf(line, "$WEAPON %s\n", u);
		parse(line);
		free(line);
	}
	if (io->count(DTU_TK_WEAPON) != id + 1) {
		dtu_tblfix_free(u);
		if (io->log)
			io->log("Class gear: the line of the Poisoned Arrow did not go in");
		return;
	}
	io->copy_line(DTU_TK_WEAPON, id, Class_arrow.from);
	put_text(io, DTU_TK_WEAPON, id, u);
	name = io->name(DTU_TK_WEAPON, id);
	if (name && io->name_size > 0) {
		strncpy(name, Class_arrow.name, io->name_size - 1);
		name[io->name_size - 1] = 0;
	}
	Arrow_weapon = id;
	for (i = 0; i < N_CLASS_LINES; i++) {
		if ((line = malloc(strlen(Class_lines[i]) + 2)) == NULL)
			return;
		sprintf(line, "%s\n", Class_lines[i]);
		parse(line);
		free(line);
	}
	if (io->count(DTU_TK_ARMOR) != DTU_GEAR_BRACERS_ARCHERY + 1 || io->count(DTU_TK_MISC) != DTU_GEAR_PEARL_POWER + DTU_GEAR_N_PEARLS) {
		if (io->log)
			io->log("Class gear: the new lines did not go in");
		return;
	}
	Dtu_class_gear_loaded = 1;
	if (io->log) {
		snprintf(buf, sizeof(buf), "Class gear: weapon %d (Poisoned Arrow), armor 62 and 63, misc 131 to 133", id);
		io->log(buf);
	}
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
	// ASCENT: the class gear (Dtu_class_gear)
	if (dtu_class_gear_on())
		for (i = 0; i < N_CLASS_MARKET; i++)
			if (Class_market[i][0] == stage) {
				if (f)
					f(Class_market[i][1], Class_market[i][2], Class_market[i][3], Class_market[i][4]);
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
// before. A missile of a creature flies at its source at its own speed.
//
// The user's decision of 2026-11-27: the shield turns a spell missile
// whoever owns it when it hits, so also the Lightning Bolt of the player
// that came back from a wall and a bolt that the shield turned before and
// that a wall sent back again (Dtu_fix_bolt_pass): it turns it as often as
// the life of the bolt allows. A missile of the player goes straight back
// the way it came. A missile whose source is gone (a trap, a dead caster)
// still hits.

int dtu_gear_mirror(object *missile, object *player, int spell, int weapon, object *source)
{
	vms_vector back, to;
	fix speed;

	if (!Dtu_gear_mirror_worn || !source || (source->type != OBJ_ROBOT && source->type != OBJ_PLAYER) || !player ||
			player->type != OBJ_PLAYER)
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
	if (source->type == OBJ_ROBOT) {
		vm_vec_sub(&to, &source->pos, &missile->pos);
		if (vm_vec_normalize_quick(&to) <= 0)
			to = back;
	}
	else
		to = back;
	vm_vec_copy_scale(&missile->mtype.phys_info.velocity, &to, speed);
	if (source->type == OBJ_ROBOT)
		dtu_obj_log("new gear: the Mirror Shield sends obj %d (weapon %d spell %d) back to obj %d %s", (int) (missile - Objects), weapon, spell,
			(int) (source - Objects), dtu_object_name(source));
	else
		dtu_obj_log("new gear: the Mirror Shield sends obj %d (weapon %d spell %d) of the player back the way it came", (int) (missile - Objects),
			weapon, spell);
	return 1;
}

//------------------------------------------------------------------------
// The Rod of Kelemvor (dtugear.h). The charge is a global flag of the player
// record, so a save keeps it and a drop and a pick up do not charge it.

int dtu_gear_rod_weapon(void)
{
	return (Dtu_gear_loaded && Rod_weapon >= 0 && Rod_weapon < N_weapon_types) ? Rod_weapon : -1;
}

int dtu_gear_rod(int weapon)
{
	return weapon >= 0 && weapon == dtu_gear_rod_weapon();
}

int dtu_gear_rod_charged(void)
{
	return !dtu_script_gflag(DTU_GEAR_FLAG_ROD_SPENT);
}

int dtu_gear_rod_fire(int weapon)
{
	if (!dtu_gear_rod(weapon))
		return -1;
	if (!dtu_gear_rod_charged()) {
		HUD_init_message(LN("The Rod of Kelemvor has no power left. A rest will renew it."));
		dtu_obj_log("new gear: the Rod of Kelemvor has no charge");
		return 0;
	}
	return DTU_GEAR_ROD_LEVEL;
}

void dtu_gear_rod_spend(void)
{
	dtu_script_set_gflag(DTU_GEAR_FLAG_ROD_SPENT, 1);
	dtu_obj_log("new gear: the Rod of Kelemvor casts Dispel Magic at level %d, no charge until a rest", DTU_GEAR_ROD_LEVEL);
}

void dtu_gear_rod_rest(void)
{
	if (!dtu_script_gflag(DTU_GEAR_FLAG_ROD_SPENT))
		return;
	dtu_script_set_gflag(DTU_GEAR_FLAG_ROD_SPENT, 0);
	dtu_obj_log("new gear: the rest charges the Rod of Kelemvor again");
	if (dtu_gear_rod_carried())
		HUD_init_message(LN("The Rod of Kelemvor is charged again."));
}

static int rod_entry(void)
{
	int i, w = dtu_gear_rod_weapon();

	for (i = 0; w >= 0 && i < Dtu_n_inventory; i++)
		if (Dtu_inventory[i].type == OBJ_WEAPON && Dtu_inventory[i].id == w)
			return i;
	return -1;
}

int dtu_gear_rod_carried(void)
{
	return rod_entry() >= 0;
}

void dtu_gear_rod_test(int what)
{
	if (what == 1 && rod_entry() >= 0)
		dtu_char_set_current(rod_entry());
	else if (what == 2 && dtu_gear_rod_weapon() >= 0)
		dtu_inventory_add(OBJ_WEAPON, dtu_gear_rod_weapon(), 1);
	dtu_obj_log("new gear: rod test %d: weapon %d, entry %d, charged %d, placed %d", what, dtu_gear_rod_weapon(), rod_entry(),
		dtu_gear_rod_charged(), dtu_script_gflag(DTU_GEAR_FLAG_ROD));}

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

//------------------------------------------------------------------------
// The daggers for thieves

int dtu_gear_venom(int weapon)
{
	return Dtu_gear_loaded && (weapon == DTU_GEAR_VENOM || dtu_gear_poison_arrow(weapon));
}

int dtu_gear_poison_arrow(int weapon)
{
	return Dtu_class_gear_loaded && weapon >= 0 && weapon == Arrow_weapon;
}

int dtu_gear_poison_arrow_line(void)
{
	return Dtu_class_gear_loaded ? Arrow_weapon : -1;
}

int dtu_gear_assassin(int weapon)
{
	return Dtu_gear_loaded && weapon == DTU_GEAR_ASSASSIN;
}

int dtu_gear_poison_immune(int type)
{
	// The data has no poison flag. AD&D 2e: undead are immune to poison; the
	// extraplanar types of the tables are the summoned guardians, undead too.
	if (type < 0 || type >= N_robot_types)
		return 1;
	return (Monster_info[type].flags & (MONSTER_F_UNDEAD | MONSTER_F_EXTRAPLANAR)) != 0;
}

// The poison of each object: its signature, the seconds left, the time of
// the next point
static struct { int sig, left; fix next; } Poison[MAX_OBJECTS];

void dtu_gear_poison_hit(object *creature, object *attacker, int weapon)
{
	int n;

	if (!dtu_gear_venom(weapon) || !creature || creature->type != OBJ_ROBOT || !attacker || attacker->type != OBJ_PLAYER)
		return;
	if (creature->shields < F1_0 || (creature->flags & (OF_EXPLODING | OF_SHOULD_BE_DEAD | OF_DESTROYED)))
		return;
	n = creature - Objects;
	if (dtu_gear_poison_immune(creature->id)) {
		dtu_obj_log("new gear: obj %d %s is immune to the poison of %s", n, dtu_object_name(creature),
			dtu_gear_poison_arrow(weapon) ? "the Poisoned Arrow" : "the Dagger of Venom");
		return;
	}
	if (Poison[n].sig == creature->signature && Poison[n].left > 0) {
		// a second hit: the 15 seconds start again, no second poison
		Poison[n].left = DTU_GEAR_POISON_TIME;
		dtu_obj_log("new gear: the poison of obj %d %s starts again", n, dtu_object_name(creature));
		return;
	}
	if (dtu_spell_save_from(creature, DTU_SAVE_PARALYZE, attacker)) {
		dtu_obj_log("new gear: obj %d %s saves against the poison", n, dtu_object_name(creature));
		return;
	}
	Poison[n].sig = creature->signature;
	Poison[n].left = DTU_GEAR_POISON_TIME;
	Poison[n].next = GameTime + F1_0;
	HUD_init_message(LN("%s is poisoned."), dtu_object_display_name(creature));
	dtu_obj_log("new gear: obj %d %s is poisoned for %d seconds", n, dtu_object_name(creature), DTU_GEAR_POISON_TIME);
}

void dtu_gear_poison_frame(void)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];

		if (Poison[i].left <= 0)
			continue;
		if (o->type != OBJ_ROBOT || o->signature != Poison[i].sig || o->shields < F1_0 ||
				(o->flags & (OF_EXPLODING | OF_SHOULD_BE_DEAD | OF_DESTROYED))) {
			Poison[i].left = 0;
			continue;
		}
		if (!dtu_time_past(Poison[i].next))
			continue;
		Poison[i].next += F1_0;
		Poison[i].left--;
		dtu_obj_log("new gear: the poison takes 1 hit point of obj %d %s, %d seconds left", i, dtu_object_name(o), Poison[i].left);
		// the damage of the player (a kill gives the experience), with no
		// line "You hit" for each point
		Dtu_damage_quiet = 1;
		dtu_damage_creature_direct(o, ConsoleObject, F1_0);
		Dtu_damage_quiet = 0;
	}
}

//------------------------------------------------------------------------
// The class gear (Dtu_class_gear): the Poisoned Arrow and the Pearls of
// Power

void dtu_gear_missile_hit(object *creature, object *attacker, int weapon)
{
	if (dtu_gear_poison_arrow(weapon))
		dtu_gear_poison_hit(creature, attacker, weapon);
}

int dtu_gear_pearl_level(int misc)
{
	if (!Dtu_class_gear_loaded || misc < DTU_GEAR_PEARL_POWER || misc >= DTU_GEAR_PEARL_POWER + DTU_GEAR_N_PEARLS)
		return 0;
	return misc - DTU_GEAR_PEARL_POWER + 1;
}

// AD&D 2e (DMG, Pearl of Power): the pearl brings back one spell of its
// level that the wizard has cast, once a day. The game has no days: once in
// each dungeon level, and again after a rest.
int dtu_gear_pearl_use(int misc)
{
	int lv = dtu_gear_pearl_level(misc), k;

	if (lv <= 0)
		return 0;
	k = lv - 1;
	if (dtu_script_gflag(DTU_GEAR_GF_PEARL_USED + k)) {
		HUD_init_message(LN("The Pearl of Power is dull. It glows again in the next level or after a rest."));
		dtu_char_log("use: Pearl of Power %d refused: used in this level", lv);
		return 1;
	}
	if (Dtu_spell_slots_max[0][k] <= 0 || Dtu_spell_slots_left[0][k] >= Dtu_spell_slots_max[0][k]) {
		HUD_init_message(LN("You have no spent spell of level %d for the pearl to bring back."), lv);
		dtu_char_log("use: Pearl of Power %d: no spent mage slot of level %d (%d of %d left)", lv, lv,
			Dtu_spell_slots_left[0][k], Dtu_spell_slots_max[0][k]);
		return 1;
	}
	Dtu_spell_slots_left[0][k]++;
	dtu_script_set_gflag(DTU_GEAR_GF_PEARL_USED + k, 1);
	HUD_init_message(LN("The Pearl of Power glows: a spent spell of level %d comes back to you."), lv);
	dtu_char_log("use: Pearl of Power %d: mage slot of level %d back, %d of %d left", lv, lv,
		Dtu_spell_slots_left[0][k], Dtu_spell_slots_max[0][k]);
	return 1;
}

// 1 after the load of a game in a level, until the start of that level
static int Pearl_keep = 0;

void dtu_gear_pearl_keep(void)
{
	Pearl_keep = 1;
}

void dtu_gear_pearl_recharge(const char *why)
{
	int k, used = 0;

	if (why && !strcmp(why, "level start") && Pearl_keep) {
		Pearl_keep = 0;
		return;
	}
	Pearl_keep = 0;
	for (k = 0; k < DTU_GEAR_N_PEARLS; k++) {
		used |= dtu_script_gflag(DTU_GEAR_GF_PEARL_USED + k) << k;
		dtu_script_set_gflag(DTU_GEAR_GF_PEARL_USED + k, 0);
	}
	if (used)
		dtu_char_log("Pearls of Power ready again (%s), used 0x%x", why ? why : "", used);
}
