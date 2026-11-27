/*
 * ASCENT: new file. New gear and more gold and gems, the setting "New gear"
 * (Dtu_new_gear, on by default; NewGear of ascent.cfg; the options -newgear
 * and -nonewgear change it for one run; -noimprovements and -origitems
 * clear it). It is not in DTUGAME.EXE 1.3. See docs/re/objects.md, "New
 * gear".
 *
 * With the setting the item tables of a level get seven new $ARMOR lines
 * (53 to 59) and three new $OBJECT lines (the gems 128 to 130), the unused
 * wand lines 63 to 67 of $WEAPON cast a spell, and the creatures leave more
 * gold and gems. The setting is read when the tables of a level are loaded
 * (bm_load_level_tables): a change takes effect at the next level. With the
 * setting off the tables, the drops and the market are those of 1.3 (with
 * the Dtu_fix_* corrections).
 *
 * Save games: an item is a type and a number (docs/formats/misc.md). A save
 * with a new item loads in the port with the setting; without it the item
 * has no table line and the load drops it (dtu_inventory_append). DTUGAME.EXE
 * 1.3 cannot load such a save (it asserts on a number above the count of the
 * table).
 */
#ifndef _DTUGEAR_H
#define _DTUGEAR_H

struct object;
struct dtu_tblfix_io;

extern int Dtu_new_gear;

// 1 while the tables of the current level have the new lines (the setting
// was on when they were loaded and the tables are the mission tables).
extern int Dtu_gear_loaded;

// The new lines
#define DTU_GEAR_FIRST_ARMOR	53
#define DTU_GEAR_MIRROR_SHIELD	53		// Shield +1, sends spell missiles and eye rays back
#define DTU_GEAR_BOOTS_FREE		54		// Boots of Free Action
#define DTU_GEAR_ARCHER			55		// Gloves of the Archer: +1 to hit, +2 damage with bows and crossbows
#define DTU_GEAR_CLOAK_FIRE		56		// Cloak of Fire Resistance
#define DTU_GEAR_BELT_DWARF		57		// Belt of Dwarvenkind: Constitution 18, infravision
#define DTU_GEAR_RING_REGEN		58		// Ring of Regeneration: 1 hit point each 60 seconds
#define DTU_GEAR_PLATE_FIRE		59		// Plate Mail of Fire Resistance: Plate Mail +2, Resist Fire
#define DTU_GEAR_N_ARMOR		7

#define DTU_GEAR_FIRST_MISC		128
#define DTU_GEAR_BLOODSTONE		128
#define DTU_GEAR_PEARL			129
#define DTU_GEAR_STAR_SAPPHIRE	130
#define DTU_GEAR_N_MISC			3

#define DTU_GEAR_FIRST_WAND		63		// weapons 63 to 67
#define DTU_GEAR_N_WANDS		5

#define DTU_GEAR_ARCHER_HIT		1
#define DTU_GEAR_ARCHER_DAMAGE	2
#define DTU_GEAR_REGEN_TIME		60		// seconds for one hit point
#define DTU_GEAR_DWARF_CON		18

// The global flag of the player record (dtu_script_gflag) that says that
// the Flame Tongue of the dungeon is placed (dtu_gear_place_flame_tongue).
// The scripts use flags up to 128, the port up to 0xA0.
#define DTU_GEAR_FLAG_FLAME		0xF0

// bmread.c, after the table fixup of 1.3: the wands and the values of the
// weapon lines, and the text of the new lines, which bmread.c reads with
// the reader of the tables (parse). Nothing when the setting is off or the
// tables are not the mission tables.
void dtu_gear_tables(const struct dtu_tblfix_io *io, void (*parse)(char *text));

// The armor class that a new armor line gives (as the table 0x145C04 of
// dtuchar.c), 0 for another number
int dtu_gear_armor_ac(int id);

// The gold that a dead creature leaves: 0 none. moola: the value of its
// type, name: the name of its type (the named people leave none). Uses
// rand().
int dtu_gear_gold(int moola, const char *name);

// The gem that a dead creature leaves (a misc number), or -1. gem_class: 1
// if the type is undead or intelligent (dtu_gear_gem_class), xp: the
// experience of the type. Uses rand().
int dtu_gear_gem(int gem_class, int xp);

// 1 if the creature type leaves gems: undead, or intelligent (the door bit,
// a person model of Improved AI, a spell caster or the eye rays).
int dtu_gear_gem_class(int type);

// The goods of the market for a reward stage of Khelben (0 to 8, the
// stages of Mk_reward_stock in dtufe_town.c). Calls f for each row with
// the values of a row of Mk_reward_stock: quantity, class (5 weapon, 15
// armor, 16 misc), number, count of one item (-1: the uses of the line).
// Returns the number of rows. Nothing when the setting is off.
int dtu_gear_market_stock(int stage, void (*f)(int quantity, int type, int id, int count));

// 1 if the setting takes the good (class, number) out of the 1.3 reward
// stage (the Flame Tongue of stage 5 goes to stage 7). The goods that a
// stock file has already keep their place.
int dtu_gear_market_removed(int stage, int type, int id);

// The treasury of Tamaramagil (DROW2): calls f for each item. Returns the
// number of items. Nothing when the setting is off.
int dtu_gear_dragon_treasure(void (*f)(int type, int id, int count));

// dtuscript.c: a GIVE of a script to the player. Script 160 (Daland, MYRKUL)
// gives the Potion of Extra-Healing for the consecration of the altar of
// Kelemvor; with the setting it gives the Mace of Disruption too.
void dtu_gear_script_give(int script, int type, int id);

// dtuobj_missile.c: a missile hits the player. 1 if a worn Mirror Shield
// sends it back: the velocity of the missile is turned to its source.
// spell: the spell of the missile or -1, weapon: its weapon line, source:
// the creature that sent it or NULL.
int dtu_gear_mirror(struct object *missile, struct object *player, int spell, int weapon, struct object *source);

// dtuchar.c: the Mirror Shield is in the shield place. Set by dtu_char_update.
extern int Dtu_gear_mirror_worn;

// dtuobj.c, the setup of a level (next to dtu_restore_tamaramagil): the
// Flame Tongue of the dungeon (dtuobj_setup.c). restore: the setup after
// the read of a saved level state.
void dtu_gear_place_flame_tongue(int has_state, int restore);

#endif
