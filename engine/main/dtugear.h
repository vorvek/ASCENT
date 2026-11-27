/*
 * ASCENT: new file. New gear and more gold and gems, the setting "New gear"
 * (Dtu_new_gear, on by default; NewGear of ascent.cfg; the options -newgear
 * and -nonewgear change it for one run; -noimprovements and -origitems
 * clear it). It is not in DTUGAME.EXE 1.3. See docs/re/objects.md, "New
 * gear".
 *
 * With the setting the item tables of a level get nine new $ARMOR lines
 * (53 to 61) and three new $OBJECT lines (the gems 128 to 130), the unused
 * wand lines 63 to 67 of $WEAPON cast a spell, line 68 is the Frost Brand,
 * and the creatures leave more
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
#define DTU_GEAR_CLOAK_DISPLACE	60		// Cloak of Displacement: one attack that hits misses, each 30 seconds
#define DTU_GEAR_SASH_MAGI		61		// Sash of the Magi: a mage slot more at each level, +2 saves against spells
#define DTU_GEAR_N_ARMOR		9

// The Frost Brand: weapon line 68 (the last "Rod of Genericness" of 1.3)
// becomes a copy of the Longsword +3 (14) with this name: +3, and 3 more
// to hit and damage against a fire creature (dtu_gear_fire_creature),
// Resist Fire while it is in the hand
#define DTU_GEAR_FROST_BRAND	68
#define DTU_GEAR_FROST_FROM		14
#define DTU_GEAR_FROST_BONUS	3

// The daggers for thieves. The Dagger of Venom +2: a new weapon line 89 (the
// last place of the weapon table, 90 lines), a copy of the Dagger +2 (44)
// with its name. A hit poisons a creature that is not immune (undead and
// extraplanar types) and fails a save against poison (category 0): 1 hit
// point each second for 15 seconds, the damage of the player; a hit while
// the poison runs starts the 15 seconds again. The Assassin's Dagger +3:
// line 76 ("Temp" of 1.3, a test line that nothing uses) as a copy of the
// Dagger +2 with +3 and thieves only; a backstab with it multiplies by one
// more.
#define DTU_GEAR_VENOM			89
#define DTU_GEAR_ASSASSIN		76
#define DTU_GEAR_DAGGER_FROM	44
#define DTU_GEAR_POISON_TIME	15		// seconds, 1 hit point each second
#define DTU_GEAR_ASSASSIN_STEP	1		// the backstab multiplier: one more
#define DTU_GEAR_DISPLACE_TIME	30		// seconds until the Cloak of Displacement is ready again
#define DTU_GEAR_SASH_SLOTS		1		// mage slots more at each spell level that the wearer casts
#define DTU_GEAR_SASH_SAVE		2		// saving throws against spells

#define DTU_GEAR_FIRST_MISC		128
#define DTU_GEAR_BLOODSTONE		128
#define DTU_GEAR_PEARL			129
#define DTU_GEAR_STAR_SAPPHIRE	130
#define DTU_GEAR_N_MISC			3

#define DTU_GEAR_FIRST_WAND		63		// weapons 63 to 67
#define DTU_GEAR_N_WANDS		5

// The drops (dtu_gear_gold, dtu_gear_gem). tools/gear_tables.py reads these
// numbers for tools/gear_income.py and the wiki (tools/wiki_rules.py).
#define DTU_GEAR_GOLD_IN_10		6		// gold in 6 of 10 deaths: moola + 1d(GOLD_DICE x moola)
#define DTU_GEAR_GOLD_DICE		2
#define DTU_GEAR_GEM_ONE_IN		2		// a gem in 1 of 2 deaths of an undead or intelligent type
#define DTU_GEAR_GEM_XP_PEARL	100		// experience below: a Bloodstone
#define DTU_GEAR_GEM_XP_RUBY	500		// below: a Pearl PEARL_IN_4 times in 4, else a Ruby
#define DTU_GEAR_GEM_XP_DIAMOND	2000	// below: a Ruby RUBY_IN_3 times in 3, else a Diamond; above: a Diamond, a Star Sapphire 1 in SAPPHIRE_ONE_IN
#define DTU_GEAR_PEARL_IN_4		3
#define DTU_GEAR_RUBY_IN_3		2
#define DTU_GEAR_SAPPHIRE_ONE_IN	10

#define DTU_GEAR_ARCHER_HIT		1
#define DTU_GEAR_ARCHER_DAMAGE	2
#define DTU_GEAR_REGEN_TIME		60		// seconds for one hit point
#define DTU_GEAR_DWARF_CON		18

// The global flag of the player record (dtu_script_gflag) that says that
// the Flame Tongue of the dungeon is placed (dtu_gear_place_flame_tongue).
// The scripts use flags up to 128, the port up to 0xA0.
#define DTU_GEAR_FLAG_FLAME		0xF0
#define DTU_GEAR_FLAG_VENOM		0xF8		// the Dagger of Venom of DROW5 is placed

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

// dtuchar.c: the Cloak of Displacement is in the cloak place, and the Sash
// of the Magi in the belt place. Set by dtu_char_update.
extern int Dtu_gear_displace_worn, Dtu_gear_sash_worn;

// 1 if weapon is the Frost Brand (the tables have the new lines)
int dtu_gear_frost_brand(int weapon);

// 1 if the creature type is a fire creature for the Frost Brand: fire
// immune or fire resistant, or its weapon is a fire spell (Burning Hands,
// Fireball, Produce Fire, Melf's Minute Meteors): the dragon, the
// hatchlings, the fire mephits.
int dtu_gear_fire_creature(int type);

// dtuobj_combat.c, damage_player_w: an attack of attacker that hits the
// player. 1 if the Cloak of Displacement makes it miss (once each
// DTU_GEAR_DISPLACE_TIME seconds; a message). Spell missiles have no
// attack roll and do not come here.
int dtu_gear_displace(struct object *player, struct object *attacker);
// dtuchar.c, each frame: the message when the cloak is ready again
void dtu_gear_displace_frame(void);

// 1 if weapon is the Dagger of Venom, the Assassin's Dagger
int dtu_gear_venom(int weapon);
int dtu_gear_assassin(int weapon);
// 1 if the creature type cannot be poisoned (undead, extraplanar)
int dtu_gear_poison_immune(int type);
// dtucombat.c, the melee hit of the player that did damage. The
// Dagger of Venom of the player poisons the creature (a save).
void dtu_gear_poison_hit(struct object *creature, struct object *attacker, int weapon);
// dtuchar.c, each frame: the poison damage. The state is per object (its
// signature) and not saved: a load or a level change ends the poison.
void dtu_gear_poison_frame(void);
// dtuobj.c, the setup of a level: the Dagger of Venom of DROW5
void dtu_gear_place_venom(int has_state, int restore);

// dtuobj.c, the setup of a level (next to dtu_restore_tamaramagil): the
// Flame Tongue of the dungeon (dtuobj_setup.c). restore: the setup after
// the read of a saved level state.
void dtu_gear_place_flame_tongue(int has_state, int restore);

#endif
