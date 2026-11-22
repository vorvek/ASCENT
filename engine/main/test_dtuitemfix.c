/*
 * ASCENT: new file. Test of the item corrections (dtuitemfix.c): the two
 * tables and their lookups, without the game data. The compare of the
 * tables with the item lines of the game is tools/check_itemfix.py.
 */
#include <stdio.h>
#include <string.h>

#include "dtuitemfix.h"

static int Fails;

static void check(int ok, const char *what)
{
	if (!ok) {
		printf("FAIL: %s\n", what);
		Fails++;
	}
}

static int str_is(const char *a, const char *b)
{
	return a && b && !strcmp(a, b);
}

int main(void)
{
	int i, j;
	const char *plain = "Cloak of Elvenkind";
	const char *ring = "Ring of Wizardry";

	// The tables
	for (i = 0; i < dtu_itemfix_n_names(); i++) {
		const dtu_item_name_fix *f = dtu_itemfix_name_row(i);

		check(f->name13[0] && f->name[0] && strcmp(f->name13, f->name), "a name row changes the name");
		check(strlen(f->name) < 40, "a corrected name fits DTU_NAME_LENGTH");
		check(f->kind >= DTU_IK_WEAPON && f->kind <= DTU_IK_MISC && f->id >= 0, "a name row names a line");
		for (j = 0; j < i; j++)
			check(dtu_itemfix_name_row(j)->kind != f->kind || dtu_itemfix_name_row(j)->id != f->id, "one name row for a line");
	}
	for (i = 0; i < dtu_itemfix_n_rules(); i++) {
		const dtu_item_rule *r = dtu_itemfix_rule(i);

		check(r->key[0] && r->v[0] && strcmp(r->v13, r->v), "a rule changes the value");
		check(r->source[0] != 0, "a rule names its source");
		check(!r->flag || ((r->v[0] == '0' || r->v[0] == '1') && !r->v[1]), "a word rule is 0 or 1");
		for (j = 0; j < i; j++) {
			const dtu_item_rule *o = dtu_itemfix_rule(j);
			check(o->kind != r->kind || o->id != r->id || o->flag != r->flag || strcmp(o->key, r->key), "one rule for a key of a line");
		}
	}
	check(dtu_itemfix_name_row(-1) == NULL && dtu_itemfix_name_row(dtu_itemfix_n_names()) == NULL, "name row bounds");
	check(dtu_itemfix_rule(-1) == NULL && dtu_itemfix_rule(dtu_itemfix_n_rules()) == NULL, "rule bounds");

	// The defaults
	check(Dtu_fix_item_names == 1, "the name corrections are on by default");
	check(Dtu_rulebook_items == 0, "the rulebook items are off by default");

	// The names
	// The names after the table fixup of 1.3 (dtutblfix.h)
	check(str_is(dtu_itemfix_name(DTU_IK_ARMOR, 45, "Cloak of Lathandar"), "Cloak of Lathander"), "armor 45 is the Cloak of Lathander");
	check(dtu_itemfix_name(DTU_IK_ARMOR, 43, plain) == plain, "armor 43 stays the Cloak of Elvenkind");
	check(dtu_itemfix_name(DTU_IK_ARMOR, 27, ring) == ring, "the fixup names the Ring of Wizardry: no row");
	check(str_is(dtu_itemfix_name(DTU_IK_MISC, 83, "Potion of Invisibility"), "Potion of Invisibility"), "the fixup names the Potion of Invisibility: no row");
	check(str_is(dtu_itemfix_name(DTU_IK_MISC, 127, "thief scroll  "), "Thief Scroll"), "spaces at the end of a name do not count");
	check(str_is(dtu_itemfix_name(DTU_IK_WEAPON, 45, "Cloak of Lathandar"), "Cloak of Lathandar"), "the table counts");
	check(str_is(dtu_itemfix_name(DTU_IK_ARMOR, 45, "Old Cloak"), "Old Cloak"), "another name at the number is not changed");
	check(dtu_itemfix_name(DTU_IK_ARMOR, 45, NULL) == NULL, "no name");
	Dtu_fix_item_names = 0;
	check(dtu_itemfix_name(DTU_IK_ARMOR, 45, plain) == plain, "-origitems: the name of 1.3");
	Dtu_fix_item_names = 1;

	// The rulebook values
	check(dtu_itemfix_value(DTU_IK_WEAPON, 70, "Mace of Disruption", "damage_bonus") == NULL, "off: no value");
	check(dtu_itemfix_flag(DTU_IK_ARMOR, 2, "Shield", "thief_use") == -1, "off: no word");
	Dtu_rulebook_items = 1;
	// The table fixup gives the Warhammer 1d4+1 and the Club 1d6: no rule
	check(dtu_itemfix_value(DTU_IK_WEAPON, 28, "Warhammer", "damage_dice") == NULL, "Warhammer: the fixup value");
	check(dtu_itemfix_value(DTU_IK_WEAPON, 29, "Club", "damage_dice") == NULL, "Club: the fixup value");
	check(str_is(dtu_itemfix_value(DTU_IK_WEAPON, 70, "Mace of Disruption", "Damage_Bonus"), "2"), "Mace of Disruption: +2, keys without case");
	check(dtu_itemfix_value(DTU_IK_WEAPON, 70, "Mace of Disruption", "value") == NULL, "Mace of Disruption: the other keys stay");
	check(dtu_itemfix_value(DTU_IK_WEAPON, 70, "Mace", "damage_bonus") == NULL, "another name at the number: no value");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 9, "Scimitar", "thief_use") == 0, "Scimitar: no thief");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 9, "Scimitar", "fighter_use") == -1, "Scimitar: the fighter stays");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 73, "Dwarven Scimitar", "thief_use") == 0, "a line with two names: the second");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 73, "Scimitar", "thief_use") == 0, "a line with two names: the first");
	check(dtu_itemfix_flag(DTU_IK_ARMOR, 2, "Shield", "THIEF_USE") == 0, "Shield: no thief, words without case");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 75, "Arrows +2", "magic") == -1, "Arrows +2: magic by the fixup, no rule");
	check(dtu_itemfix_flag(DTU_IK_ARMOR, 27, ring, "mage_use") == -1, "Ring of Wizardry: the mage only by the fixup, no rule");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 59, "Wand of Lightning Bolts", "all_use") == 0 && dtu_itemfix_flag(DTU_IK_WEAPON, 59, "Wand of Lightning Bolts", "mage_use") == 1,
		"Wand of Lightning Bolts: the mage only");
	check(dtu_itemfix_flag(DTU_IK_WEAPON, 13, "Wand of Lightning Bolts", "all_use") == -1, "weapon 13 (the missile) has no rule");
	Dtu_rulebook_items = 0;

	printf("test_dtuitemfix: %d name rows, %d rules, %d failures\n", dtu_itemfix_n_names(), dtu_itemfix_n_rules(), Fails);
	return Fails != 0;
}
