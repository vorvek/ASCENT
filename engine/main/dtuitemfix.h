/*
 * ASCENT: new file. Corrections of the item tables of Descent to
 * Undermountain 1.3. It is not in the executable. See docs/audit/items.md.
 *
 * Two layers:
 *
 * 1. Item names (Dtu_fix_item_names, on by default, the option -origitems
 *    clears it). 1.3 shows some items under a wrong name or spelling: the
 *    Cloak of Lathandar, "Armoring" for a ring. The names that the table
 *    fixup of 1.3 writes (dtutblfix.h: the Ring of Wizardry, the Potion of
 *    Invisibility) are 1.3 and not corrections. Only the name that the
 *    player sees changes. The name of the table line stays, because the
 *    game rules read it (a name with "otion" or '+', 0x1A344).
 *
 * 2. Rulebook items (Dtu_rulebook_items, off by default, "Rulebook items"
 *    of the Improvements menu, RulebookItems of ascent.cfg, the options
 *    -rulebookitems and -norulebookitems). Values and class limits of the
 *    item lines take the value of the AD&D 2nd edition rulebooks where 1.3
 *    differs and the rulebook is clear. It changes the balance of the game,
 *    so it is off by default.
 *
 * This file has no reference to the game tables: bmread.c asks it for the
 * value of a key of an item line (bm_tbl_get, bm_tbl_has_flag), and
 * dtu_item_shown_name (dtuobj.c) asks it for a name. The two tables are in
 * dtuitemfix.c. tools/check_itemfix.py compares them with the game tables.
 */
#ifndef _DTUITEMFIX_H
#define _DTUITEMFIX_H

// The table of an item
#define DTU_IK_WEAPON	0		// $WEAPON (OBJ_WEAPON)
#define DTU_IK_ARMOR		1		// $ARMOR (OBJ_ARMOR)
#define DTU_IK_MISC		2		// $OBJECT (OBJ_MISC)

// 1: the corrected item names are shown. -origitems: the names of 1.3.
extern int Dtu_fix_item_names;
// 1: the item lines have the values of the rulebooks. See the top.
extern int Dtu_rulebook_items;

// One corrected name.
typedef struct dtu_item_name_fix {
	int			kind, id;	// DTU_IK_, the number of the line
	const char	*name13;		// the name of 1.3 after the table fixup (name= or weapon_name=). The correction applies only to this name.
	const char	*name;		// the corrected name
	const char	*why;			// the evidence, for the maintainer
} dtu_item_name_fix;

// One rulebook value.
typedef struct dtu_item_rule {
	int			kind, id;
	const char	*name13;		// the name of 1.3; names separated by '|' when the line has two. The rule applies only to these names.
	int			flag;			// 0: key=value. 1: a word of the line (thief_use, magic)
	const char	*key;
	const char	*v13;			// the value of 1.3 after the table fixup. "" for a key that the line does not have. A word: "1" or "0".
	const char	*v;			// the rulebook value
	const char	*source;		// book and page
} dtu_item_rule;

int dtu_itemfix_n_names(void);
const dtu_item_name_fix *dtu_itemfix_name_row(int i);
int dtu_itemfix_n_rules(void);
const dtu_item_rule *dtu_itemfix_rule(int i);

// The corrected English name of an item, or name when there is no
// correction for it (or Dtu_fix_item_names is 0). name is the name of the
// table line; spaces at its end do not count.
const char *dtu_itemfix_name(int kind, int id, const char *name);

// The rulebook value of key= of an item line, or NULL: no rule for it, the
// name of the line is another name, or Dtu_rulebook_items is 0.
const char *dtu_itemfix_value(int kind, int id, const char *name, const char *key);

// The rulebook state of a word of an item line: 1 the line has it, 0 it
// has not, -1 no rule (the line decides).
int dtu_itemfix_flag(int kind, int id, const char *name, const char *flag);

#endif
