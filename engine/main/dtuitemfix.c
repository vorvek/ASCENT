/*
 * ASCENT: new file. Corrections of the item tables of Descent to
 * Undermountain 1.3: the two tables and their lookups. See dtuitemfix.h and
 * docs/audit/items.md (the evidence of each row).
 *
 * The numbers are line numbers of the sections $WEAPON, $ARMOR and $OBJECT
 * of the level tables of 1.3 (the item lines are the same in every table of
 * the mission). Each row names the name of the line in 1.3: a row does not
 * apply to a table that has another item at that number.
 *
 * tools/check_itemfix.py reads the two tables of this file. Keep one row on
 * one line, with the fields in this order and the strings without escapes.
 */
#include <string.h>

#include "dtuitemfix.h"

int Dtu_fix_item_names = 1;
int Dtu_rulebook_items = 0;
int Dtu_fix_item_stats = 1;

#define W	DTU_IK_WEAPON
#define A	DTU_IK_ARMOR
#define M	DTU_IK_MISC

//------------------------------------------------------------------------
// Layer 1: the names. Addresses: DTUGAME.EXE 1.3, object 1 at 0x10000.

static const dtu_item_name_fix Name_fixes[] = {
	{ A, 45, "Cloak of Elvenkind", "Cloak of Lathander", "the second line of this name: turn undead as a cleric of level 7 (0xADC46); README.TXT of 1.3: Cloak of Lathandar" },
	{ A, 27, "Ring of Warmth", "Ring of Wizardry", "doubles the mage spell slots of levels 1 to 3 (0xAFB55 case 0x1B); README.TXT of 1.3: Ring of Wizardry" },
	{ M, 83, "Potion of Agility", "Potion of Invisibility", "the second line of this name: invisibility (0x37A6A item 0x53); README.TXT of 1.3: Potion of Invisibility" },
	{ A, 16, "Armoring", "Ring of Armoring", "a ring (the word ring of the line); the 14 other rings are Ring of ..." },
	{ M, 66, "Black Key", "Black Gem Key", "the first of the three lines 66 to 68; 67 and 68 are Black Gem Key" },
	{ W, 41, "Club+1", "Club +1", "the other magic weapons have a space before the bonus" },
	{ M, 127, "thief scroll", "Thief Scroll", "small letters and two spaces at the end" },
};
#define N_NAME_FIXES	((int) (sizeof(Name_fixes) / sizeof(Name_fixes[0])))

//------------------------------------------------------------------------
// Layer 2: the rulebook values. PHB: Player's Handbook (TSR 2159, 1995).
// DMG: Dungeon Master Guide (TSR 2160, 1995). The page is the printed page
// (the scan of the Encyclopedia Magica: the page of the PDF file).

static const dtu_item_rule Rules[] = {
	// The Warhammer does 3d10 and the Warhammer +1 does 1d6+1. The Throwing Hammer of 1.3 has the rulebook value 1d4+1.
	{ W, 28, "Warhammer", 0, "#damage_dice", "3", "1", "PHB p. 95, Table 44: warhammer 1d4+1" },
	{ W, 28, "Warhammer", 0, "damage_dice", "10", "4", "PHB p. 95, Table 44: warhammer 1d4+1" },
	{ W, 28, "Warhammer", 0, "damage_bonus", "", "1", "PHB p. 95, Table 44: warhammer 1d4+1" },
	{ W, 40, "Warhammer +1", 0, "damage_dice", "6", "4", "PHB p. 95, Table 44: warhammer 1d4+1, and +1" },
	{ W, 40, "Warhammer +1", 0, "damage_bonus", "1", "2", "PHB p. 95, Table 44: warhammer 1d4+1, and +1" },
	// The Club does 1d4+1.
	{ W, 29, "Club", 0, "damage_dice", "4", "6", "PHB p. 94, Table 44: club 1d6" },
	{ W, 29, "Club", 0, "damage_bonus", "1", "0", "PHB p. 94, Table 44: club 1d6" },
	{ W, 41, "Club+1", 0, "damage_dice", "4", "6", "PHB p. 94, Table 44: club 1d6, and +1" },
	{ W, 41, "Club+1", 0, "damage_bonus", "2", "1", "PHB p. 94, Table 44: club 1d6, and +1" },
	// A thief has no scimitar and no shield.
	{ W, 9, "Scimitar", 1, "thief_use", "1", "0", "PHB p. 54: the weapons of a thief" },
	{ W, 73, "Scimitar|Dwarven Scimitar", 1, "thief_use", "1", "0", "PHB p. 54: the weapons of a thief" },
	{ W, 42, "Scimitar +1", 1, "thief_use", "1", "0", "PHB p. 54: the weapons of a thief" },
	{ A, 2, "Shield", 1, "thief_use", "1", "0", "PHB p. 54: the armor of a thief" },
	{ A, 12, "Shield +1", 1, "thief_use", "1", "0", "PHB p. 54: the armor of a thief" },
	{ A, 13, "Shield +2", 1, "thief_use", "1", "0", "PHB p. 54: the armor of a thief" },
	// The Mace of Disruption is a mace +2 in 1.3.
	{ W, 70, "Mace of Disruption", 0, "to_hit_bonus", "2", "1", "Encyclopedia Magica vol. 2, PDF page 261 (from the DMG): it appears to be a mace +1" },
	{ W, 70, "Mace of Disruption", 0, "damage_bonus", "3", "2", "Encyclopedia Magica vol. 2, PDF page 261 (from the DMG): a mace +1; the Mace +1 of 1.3 does 1d6+2" },
	// The magic arrows add their bonus to the damage only, and the Arrows +2 are not magic.
	{ W, 74, "Arrows +1", 0, "to_hit_bonus", "", "1", "DMG p. 189, Table 109: the bonus is an attack roll adjustment" },
	{ W, 75, "Arrows +2", 0, "to_hit_bonus", "", "2", "DMG p. 189, Table 109: the bonus is an attack roll adjustment" },
	{ W, 75, "Arrows +2", 1, "magic", "0", "1", "DMG p. 189, Table 109: a weapon with a bonus is magical" },
	// Only a wizard uses the wand of lightning and the ring of wizardry.
	{ W, 59, "Wand of Lightning Bolts", 1, "all_use", "1", "0", "DMG p. 185, Table 94: Lightning (Wizard)" },
	{ W, 59, "Wand of Lightning Bolts", 1, "mage_use", "0", "1", "DMG p. 185, Table 94: Lightning (Wizard)" },
	{ A, 27, "Ring of Warmth", 1, "all_use", "1", "0", "DMG p. 184, Table 91: Wizardry (Wizard)" },
	{ A, 27, "Ring of Warmth", 1, "mage_use", "0", "1", "DMG p. 184, Table 91: Wizardry (Wizard)" },
};
#define N_RULES	((int) (sizeof(Rules) / sizeof(Rules[0])))

//------------------------------------------------------------------------

int dtu_itemfix_n_names(void)
{
	return N_NAME_FIXES;
}

const dtu_item_name_fix *dtu_itemfix_name_row(int i)
{
	return (i >= 0 && i < N_NAME_FIXES) ? &Name_fixes[i] : NULL;
}

int dtu_itemfix_n_rules(void)
{
	return N_RULES;
}

const dtu_item_rule *dtu_itemfix_rule(int i)
{
	return (i >= 0 && i < N_RULES) ? &Rules[i] : NULL;
}

// name is one of the names of list ("A|B"). Spaces at the end of name do
// not count.
static int name_is(const char *list, const char *name)
{
	int n;

	if (!name)
		return 0;
	n = (int) strlen(name);
	while (n > 0 && name[n - 1] == ' ')
		n--;
	while (*list) {
		const char *e = strchr(list, '|');
		int l = e ? (int) (e - list) : (int) strlen(list);

		if (l == n && !memcmp(list, name, n))
			return 1;
		list += l + (e != NULL);
	}
	return 0;
}

// The keys and the words of a table are not case sensitive.
static int word_is(const char *a, const char *b)
{
	for (; *a && *b; a++, b++) {
		int ca = *a, cb = *b;

		if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
		if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
		if (ca != cb)
			return 0;
	}
	return *a == *b;
}

const char *dtu_itemfix_name(int kind, int id, const char *name)
{
	int i;

	if (!Dtu_fix_item_names || !name)
		return name;
	for (i = 0; i < N_NAME_FIXES; i++)
		if (Name_fixes[i].kind == kind && Name_fixes[i].id == id && name_is(Name_fixes[i].name13, name))
			return Name_fixes[i].name;
	return name;
}

// Defects of the 1.3 tables (Dtu_fix_item_stats, on by default, -origitems
// gives 1.3). The Warhammer is a plain weapon of 2 gold that does 3d10, more
// than each other weapon. The Warhammer +1 does 1d6+1 and the older tables
// of the game (BASE.BIN, DEMO.BIN, MINES1.BIN) have 1d6: the correction is 1d6.
// "Rulebook items" has priority where it has a row for the same key.
static const dtu_item_rule Defects[] = {
	{ W, 28, "Warhammer", 0, "#damage_dice", "3", "1", "the Warhammer +1 (1d6+1) and the older tables (1d6)" },
	{ W, 28, "Warhammer", 0, "damage_dice", "10", "6", "the Warhammer +1 (1d6+1) and the older tables (1d6)" },
};
#define N_DEFECTS	((int) (sizeof(Defects) / sizeof(Defects[0])))

static const dtu_item_rule *find_in(const dtu_item_rule *t, int n, int kind, int id, const char *name, int flag, const char *key)
{
	int i;

	for (i = 0; i < n; i++) {
		const dtu_item_rule *r = &t[i];

		if (r->kind == kind && r->id == id && r->flag == flag && word_is(r->key, key) && name_is(r->name13, name))
			return r;
	}
	return NULL;
}

static const dtu_item_rule *find_rule(int kind, int id, const char *name, int flag, const char *key)
{
	const dtu_item_rule *r = NULL;

	if (!key)
		return NULL;
	if (Dtu_rulebook_items)
		r = find_in(Rules, N_RULES, kind, id, name, flag, key);
	if (!r && Dtu_fix_item_stats)
		r = find_in(Defects, N_DEFECTS, kind, id, name, flag, key);
	return r;
}

const char *dtu_itemfix_value(int kind, int id, const char *name, const char *key)
{
	const dtu_item_rule *r = find_rule(kind, id, name, 0, key);

	return r ? r->v : NULL;
}

int dtu_itemfix_flag(int kind, int id, const char *name, const char *flag)
{
	const dtu_item_rule *r = find_rule(kind, id, name, 1, flag);

	return r ? r->v[0] == '1' : -1;
}
