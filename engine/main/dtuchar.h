/*
 * ASCENT: new file. The player character of Descent to Undermountain and
 * the rules that the game uses during play: the attributes, the equipment,
 * the use of items, rest, experience and levels. It replaces the ship
 * values of the Descent 1 player (shields, weapons, score).
 *
 * The facts are in docs/re/character.md. The address of the original code
 * (DTUGAME.EXE 1.3, object 1 at 0x10000) is at each function of dtuchar.c.
 * "P+0x.." is an offset in the player record of the executable (6967
 * bytes, array at 0x1D331C).
 */
#ifndef _DTUCHAR_H
#define _DTUCHAR_H

#include "types.h"
#include "fix.h"
#include "dtuobj.h"

//------------------------------------------------------------------------
// Values of the attribute fields

#define DTU_CLASS_NONE		(-1)
#define DTU_CLASS_CLERIC	0			// the letters "CMFT" of the table name (0x13A834)
#define DTU_CLASS_MAGE		1
#define DTU_CLASS_FIGHTER	2
#define DTU_CLASS_THIEF		3

#define DTU_RACE_HUMAN		0			// the letters "HDEKF" (0x13A82D). Half-elf is human with DTU_PF_HALF_ELF.
#define DTU_RACE_DWARF		1
#define DTU_RACE_ELF			2
#define DTU_RACE_DARK_ELF	3
#define DTU_RACE_HALFLING	4

#define DTU_PF_HALF_ELF		1			// bit of dtu_player_attr.flags

// Class bits of an item (the word at 0 of Misc_info and Armor_info)
#define DTU_USE_CLERIC		0x0001
#define DTU_USE_MAGE			0x0002
#define DTU_USE_FIGHTER		0x0004
#define DTU_USE_THIEF		0x0020
#define DTU_USE_ALL			0xffff

// Wear bits of an item (the word at 4 of Misc_info and Armor_info). The
// bit of a slot of the paper doll is DTU_WEAR_OF_SLOT(slot).
#define DTU_WEAR_HELMET		0x0001
#define DTU_WEAR_CLOAK		0x0002
#define DTU_WEAR_NECKLACE	0x0004
#define DTU_WEAR_ARMOR		0x0008
#define DTU_WEAR_GAUNTLET	0x0010
#define DTU_WEAR_SHIELD		0x0020
#define DTU_WEAR_RING		0x0040
#define DTU_WEAR_BELT		0x0080
#define DTU_WEAR_BOOT		0x0100
#define DTU_WEAR_BOW			0x0200
#define DTU_WEAR_CROSSBOW	0x0400
#define DTU_WEAR_LOCKPICK	0x0800

// The slots of the paper doll. 0 to 9 are the ten item references at
// P+0x1AD9 of the player record, in that order. BOW and CROSSBOW are the
// references at P+0x1AB9 and P+0x1ABB.
#define DTU_SLOT_LOCKPICK	0			// the string table calls this place "Helmet", the code puts the lockpicks here
#define DTU_SLOT_CLOAK		1
#define DTU_SLOT_NECKLACE	2
#define DTU_SLOT_ARMOR		3
#define DTU_SLOT_GAUNTLET	4
#define DTU_SLOT_SHIELD		5
#define DTU_SLOT_RING1		6
#define DTU_SLOT_RING2		7
#define DTU_SLOT_BELT		8
#define DTU_SLOT_BOOT		9
#define DTU_SLOT_BOW			10
#define DTU_SLOT_CROSSBOW	11
#define DTU_N_SLOTS			12

#define DTU_N_HOT_KEYS		10

//------------------------------------------------------------------------
// Structures with the layout of the executable. They are in the save game.

#pragma pack(push,1)

// The attributes of a character: 80 bytes. The player record has them at
// P+0x97 (the values in use). A second copy at 0x187B90 has the values
// without the effect of items. SG0.SG stores both: the first one in the
// player record, the second one after it (file offset 7108).
typedef struct dtu_player_attr {
	byte		class1;				// 0x00 P+0x97
	byte		class2;				// 0x01 P+0x98. Not 0 to 3: one class
	byte		unk_02;				// 0x02 P+0x99. -1 in the save game
	byte		race;					// 0x03 P+0x9A
	ubyte		female;				// 0x04 P+0x9B
	ubyte		unk_05;				// 0x05 P+0x9C
	ubyte		unk_06;				// 0x06 P+0x9D
	ubyte		spell_class;		// 0x07 P+0x9E. 1: cleric spells, 2: mage spells
	ubyte		age;					// 0x08 P+0x9F
	ubyte		state_flags;		// 0x09 P+0xA0. Bits of effects, not decoded
	short		max_hit_points;	// 0x0A P+0xA1
	short		hit_points;			// 0x0C P+0xA3. The hit points as an integer. The game counts in P+0x53 (fix).
	short		damage_dice;		// 0x0E P+0xA5. The empty hand: sides of a die
	short		n_damage_dice;		// 0x10 P+0xA7. The empty hand: number of dice
	short		to_hit_bonus;		// 0x12 P+0xA9. The empty hand
	short		damage_bonus;		// 0x14 P+0xAB. The empty hand
	short		armor_class;		// 0x16 P+0xAD. Result of dtu_char_update()
	short		class1_level;		// 0x18 P+0xAF. 0 is the first level: the screen shows the value plus 1
	short		class2_level;		// 0x1A P+0xB1
	short		dex_ac_adjust;		// 0x1C P+0xB3
	ubyte		infravision;		// 0x1E P+0xB5
	ubyte		unk_1f;				// 0x1F
	ushort	strength;			// 0x20 P+0xB7
	ushort	strength_extra;	// 0x22 P+0xB9. The percent value of strength 18
	ushort	wisdom;				// 0x24 P+0xBB
	ushort	intelligence;		// 0x26 P+0xBD
	ushort	charisma;			// 0x28 P+0xBF
	ushort	constitution;		// 0x2A P+0xC1
	ushort	dexterity;			// 0x2C P+0xC3
	short		thief_ability;		// 0x2E P+0xC5
	int		hide_in_shadows;	// 0x30 P+0xC7
	int		move_silently;		// 0x34 P+0xCB
	int		find_traps;			// 0x38 P+0xCF
	int		climb_walls;		// 0x3C P+0xD3
	int		open_locks;			// 0x40 P+0xD7
	int		gold;					// 0x44 P+0xDB
	int		flags;				// 0x48 P+0xDF. DTU_PF_
	short		alignment;			// 0x4C P+0xE3. 0 to 5
	short		unk_4e;				// 0x4E P+0xE5. -1 in the save game
} dtu_player_attr;

// One entry of an item list of the player record: 16 bytes. The record has
// four lists: 127 items at P+0x103 (count at P+0xF3), 127 mage spells at
// P+0x8F3 (count at P+0xF7), 127 cleric spells at P+0x10E3 (count at
// P+0xFB) and 22 key items at P+0x18D3 (count at P+0xFF).
typedef struct dtu_item_record {
	ubyte		type;					// 0x00 OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC, OBJ_SPELL
	ubyte		id;					// 0x01
	ubyte		icon;					// 0x02 icon= of the table line
	ubyte		class_mask;			// 0x03 low byte of the class bits
	ubyte		flags;				// 0x04 low byte of the flags of the table entry
	ubyte		letter;				// 0x05 '_', weapon_char= of a weapon, 'B' bow, 'C' crossbow, 'K' or 'N' for a spell
	short		level;				// 0x06 number of the level where the item was found
	short		count;				// 0x08 items of the stack, or charges. -1: no limit
	ubyte		plot_item;			// 0x0A
	ubyte		spell_level;		// 0x0B spells only
	fix		modifier;			// 0x0C modifier= of the table line
} dtu_item_record;

// A reference to an entry of a list: 2 bytes.
typedef struct dtu_item_ref {
	ubyte		list;					// 0: items, 1: mage spells, 2: cleric spells
	byte		n;						// index of the entry, -1: none
} dtu_item_ref;

#pragma pack(pop)

// Offsets in the player record and in SG0.SG
#define DTU_PLAYER_RECORD_SIZE	6967
#define DTU_SG_PLAYER_OFFSET		141		// the player record in SG0.SG
#define DTU_P_NAME			0x0000
#define DTU_P_HIT_POINTS	0x0053		// fix
#define DTU_P_EXPERIENCE	0x0077		// two ints: class1, class2
#define DTU_P_ATTR			0x0097
#define DTU_P_N_ITEMS		0x00F3
#define DTU_P_ITEMS			0x0103
#define DTU_P_N_KEYS			0x00FF
#define DTU_P_KEYS			0x18D3		// 22 key items
#define DTU_P_NAME2			0x1AA3		// char[16], the name again
#define DTU_P_THIEF_PAID	0x1A53		// byte, the thief level + 1 that Mirt last paid points for (0xABB53)
#define DTU_P_GEM_USES		0x1A57		// byte, the uses of the Gem of Curing (0x3891B)
#define DTU_P_THAC0			0x1A5C		// byte
#define DTU_P_POTIONS		0x1A93		// ASCENT: four ints, the seconds left of the potions of intelligence, Hill Giant Strength, Dragon's Blood and Agility in a save (0xBA87E, 0xBA911)
#define DTU_P_THIEF_POINTS	0x1AB3		// word, the thief skill points to spend (0xB4ECF)
#define DTU_P_BOW				0x1AB9
#define DTU_P_CROSSBOW		0x1ABB
#define DTU_P_HOT_KEYS		0x1AC1
#define DTU_P_CURRENT		0x1AD7
#define DTU_P_SLOTS			0x1AD9
#define DTU_P_MODEL			0x1AED		// char[13], the model file of the player
#define DTU_P_PORTRAIT		0x1B03		// int

//------------------------------------------------------------------------
// The character of the port

typedef struct dtu_character {
	int				created;
	char				name[20];
	dtu_player_attr pa;					// the values in use (PA of the asserts)
	dtu_player_attr base;				// the values without items (0x187B90)
	int				experience[2];		// P+0x77, P+0x7B
	int				thac0;				// P+0x1A5C
	int				portrait;			// P+0x1B03
	// References to entries of Dtu_inventory[]. list is always 0.
	dtu_item_ref	current;				// the item in the hand, P+0x1AD7
	dtu_item_ref	hot_key[DTU_N_HOT_KEYS];	// P+0x1AC1
	dtu_item_ref	slot[DTU_N_SLOTS];	// DTU_SLOT_
	fix				no_pickup_until;	// P+0x1ABD: no pick up before this game time (5 seconds after a drop)
	int				thief_paid;			// P+0x1A53: the thief level + 1 of the last points of Mirt, 0 none
	int				thief_points;		// P+0x1AB3: the thief skill points to spend (dtuthiefpts.c)
} dtu_character;

extern dtu_character Dtu_char;

// The decoded rule values of one table line of an item.
typedef struct dtu_item_data {
	char		*name;
	int		icon;
	int		class_mask;				// DTU_USE_
	int		wear_mask;				// DTU_WEAR_
	int		stackable;
	int		stack_limit;
	int		hot_key_ok;				// 0x31752: can stand in a hot key box
	int		uses;
	int		value;
	int		ac_bonus;				// added to the armor class (negative is better). The tables at 0x145C04, 0x145A04, 0x1458B8
	// weapons
	int		melee;					// 1: attack type 1. 0: a missile, or a launcher for missiles
	int		dice_sides;				// damage_dice=
	int		dice_n;					// #damage_dice=
	int		damage_bonus;
	int		to_hit_bonus;
	fix		attack_wait;			// fire_wait=
} dtu_item_data;

// Returns 0 if the type has no table line.
int dtu_item_get(int type, int id, dtu_item_data *d);

//------------------------------------------------------------------------
// Settings

// The THAC0 of a character with two classes. 0: the executable takes the
// higher (worse) value of the two classes (0xAF651). 1 (default): the
// lower (better) value, as in the rules of the game.
extern int Dtu_fix_multiclass_thac0;
// The hit points of a new level. 0: as 0xAF8C7, the integer is added to
// the fixed point hit points (they grow by n / 65536; the maximum grows by
// n). 1 (default): the hit points grow by n, as the rules say.
extern int Dtu_fix_level_hit_points;

// 0x8BA3A: the character becomes a character of level "level" (from 1):
// the level of each class (one less for two classes), the experience of
// that level, the hit points (die + constitution) * (level value + 1),
// the mean for two classes, THAC0. Script statement 9F and the new
// character use it.
void dtu_char_set_level(int level);

//------------------------------------------------------------------------
// Calls from the game

// After a level is loaded and the player is placed: makes the default
// character at the first call, and sets the hit points of the player.
void dtu_char_level_start(void);

// One time per frame (dtu_display_frame calls it).
void dtu_char_frame(void);

// Makes the default character: a human fighter of the first level with
// the items that the front end gives to a fighter.
void dtu_char_create_default(void);

// Reads a character from a save game of the original (SG0.SG). Returns 1
// if successful. Option -char FILE.
int dtu_char_load_save(char *filename);
// Writes the character into a player record buffer (6967 bytes) with the
// layout of the executable. The decoded fields come from the character;
// the other bytes come from the record of the last loaded save game.
void dtu_char_pack(ubyte *record);

// Sets the values that depend on the equipment: armor class, strength,
// dexterity, constitution, thief skills (0xAFB55).
void dtu_char_update(void);

//------------------------------------------------------------------------
// Inventory. "index" is an index of Dtu_inventory[].

// The wear bits of an entry: the slots where it can go (0x317EA).
int dtu_char_wear_mask(int index);
// The slot that holds the entry, or -1.
int dtu_char_slot_of(int index);
// Puts the entry into a slot. Returns 1 if the slot accepts it. The item
// that was in the slot goes back to the list (it never left it).
int dtu_char_equip(int index, int slot);
// Puts the entry into the first slot that accepts it and is free, or into
// the first slot that accepts it. A weapon becomes the item in the hand.
int dtu_char_equip_auto(int index);
void dtu_char_unequip(int slot);
// Makes the entry the item in the hand (P+0x1AD7). -1: the empty hand.
void dtu_char_set_current(int index);
// Hot keys. key is 0 to 9.
void dtu_char_set_hot_key(int key, int index);
void dtu_char_select_hot_key(int key);			// the keys 1 to 0: the item or the spell goes into the hand
// A box holds an entry of the item list (list 0) or a spell (list 1 mage,
// 2 cleric, the index in the spell list of dtuspell.c). The spell of a box
// or -1, the entry of the item list of a box or -1, and 1 if the box holds
// the item or the spell in the hand (the border of 0x329CB).
int dtu_char_hot_key_spell(int key);
int dtu_char_hot_key_item(int key);
int dtu_char_hot_key_in_hand(int key);
// 0x43A5A with its tests (the keys 1 to 0, a click on a box, Cycle Weapon):
// nothing in an attack state; sound 3000 while the attack key waits for its
// attack or cast, for a box with no entry, for arrows or bolts without the
// launcher, and for the box that was chosen last; an empty box gives the
// empty hand. dtu_char_select_hot_key() has no tests.
void dtu_char_press_hot_key(int key);
// The box that was chosen last (0x13AD15), or a negative number; and its reset.
int dtu_char_last_hot_key(void);
void dtu_char_forget_hot_key(void);
// A click on a box with a held spell (0x304D6). Returns 1 if the box took it.
int dtu_char_set_hot_key_spell(int key, int spell);
// The held spell put on the paper doll (0x35CE5): the first free box takes
// it and it goes into the hand. Returns 1 if a box took it.
int dtu_char_arm_spell(int spell);
// 0x3533C for an entry of the item list: 1 if the character can use it.
int dtu_char_item_usable(int index);
// The held entry of the item list put on the figure of the paper doll
// (0x35ADF): it is worn, armed or used. Returns 0 if the cursor keeps it.
int dtu_char_put_on_doll(int index);
// Uses one item of the entry (0x37A6A). Returns 1 if it was used up.
int dtu_char_use(int index);
// Uses the item in the hand (the USE button, 0x3614B).
int dtu_char_use_current(void);
// One item of the stack, or one charge, is used (0xB7084).
void dtu_char_use_up(int index);
// Drops the whole entry in front of the player (0xB717B). Returns the
// number of the new object, or -1.
int dtu_char_drop(int index);
// Takes "count" items of a type out of the inventory (0xB6D24).
void dtu_char_take(int type, int id, int count);
// 1 if the inventory has the type (0xB6BB3).
int dtu_char_has(int type, int id);
// Rest (0x360D5). Returns 1 if the character rested.
int dtu_char_rest(void);
// 1 if the rest place in the segment works now (its time, 0x1C5E40 of the
// station table, is not in the future).
int dtu_char_rest_ready(int segnum);

//------------------------------------------------------------------------
// The interface for the combat code.

// The attack with the item in the hand, or with the empty hand.
typedef struct dtu_attack {
	int		weapon_id;				// index of Weapon_info, or -1: the empty hand
	int		melee;					// 0: the weapon in the hand is a missile weapon
	int		thac0;
	int		to_hit_bonus;			// the sum that is added to the roll of 1d20
	int		dice_n, dice_sides;	// the damage dice
	int		damage_bonus;			// added to the sum of the dice
	fix		wait;						// time from one attack to the next (fire_wait= of the weapon, 1.0 for the empty hand)
} dtu_attack;

void dtu_char_get_attack(dtu_attack *a);

// The attack roll of the player with a weapon (-1: the empty hand) on a
// target (0x2A7F3): -1 critical fumble (a 1), 0 miss, 1 hit, 2 critical
// hit (a 20).
int dtu_char_attack_roll(object *target, int weapon);
// The damage roll of the player with a weapon on a target after a hit
// (0x2AF5E). At least 0.
int dtu_char_weapon_damage(int weapon, object *target);
// The damage roll of the weapon in the hand with no target.
int dtu_char_damage_roll(void);
// The attack roll of a creature against the player (0x2A7F3): the same
// results. to_hit_bonus is the sum of the bonus values of the creature.
int dtu_char_defense_roll(int attacker_thac0, int to_hit_bonus);

int dtu_char_armor_class(void);
int dtu_char_thac0(void);
int dtu_char_hit_points(void);
int dtu_char_max_hit_points(void);
// Adds hit points, up to the maximum. Returns the number that was added.
int dtu_char_heal(int points);
// Adds hit points above the maximum (Aid, 0x8D46A). The frame cuts them to
// the maximum when Bless is not on.
void dtu_char_add_hit_points(int points);
// Experience (0xAF3CF): a character with two classes gets one half for
// each class. Checks for a new level. A creature gives score_value= of its
// table line (add_points_to_score() of gauges.c calls this).
void dtu_char_award_xp(int points);
// ASCENT: statement 9F of the scripts (0xAB71A, 0x8BA3A): the level (from
// 1) with the experience and the hit points of that level.
void dtu_char_set_level(int level);
// 0 for 5 seconds after a drop (0x31A0E): the pick up code must not take
// an item while this is 0.
int dtu_char_pickup_allowed(void);

// Experience to the next level and the part of the level that is left in
// percent (0x37253). Returns the experience of the character.
int dtu_char_next_level(int *points_to_next, int *percent_to_next);

// The log of the option -charlog FILE.
void dtu_char_log(char *format, ...);

//------------------------------------------------------------------------
// Items, potions, choices (docs/re/combat.md B6)

// The Ring of Wizardry (armor 27) doubles the mage slots of the spell
// levels 1 to 3 each time 0xAFB55 runs while it is worn, and halves them
// one time when it is taken off. 1 (default): one doubling while it is
// worn. 0 (-origcombat): the rule of the executable.
extern int Dtu_fix_wizardry_ring;
// The Poshun uv Hil Gient Inteljuns rolls 1 in 63 in each frame to make the
// player forget parts of the map (0x396B2). 1 (default): one roll for each
// reference frame (3/64 second). 0 (-origcombat): one roll for each frame.
extern int Dtu_fix_potion_frame;
// The Potion of Luck: the executable takes the lowest bit of the day clock
// (raw fix, the sum of the frame times) as a coin flip (0x2A889). 1
// (default): rand(). 0 (-origcombat): the clock bit.
extern int Dtu_fix_luck_coin;
// The Short Bow of Speed ($OBJECT 100, pc_spell1=21) and the Short Sword
// of Quickness ($WEAPON 69, pc_spell=21). The executable has no keyword
// pc_spell1, the code of the worn items (0xAFB55) does not read the bow
// slot, and PC_spell of a melee weapon is never read (0x44300 calls
// 0x56A0D only for attack_type 0): the bow is a Short Bow +2 and the sword
// a Short Sword +2. Corrected: the bow in the bow slot and the sword in
// the hand give Haste (item bit 9), like the Ring of Quickness and the
// Boots of Speed. -origcombat clears it.
extern int Dtu_fix_speed_items;
// ASCENT: the Cursed Scroll does rand() % (hit points as a fix) damage
// (0x38159), so less than half a point. 1 (default): rand() % hit points
// in whole points. 0 (-origcombat): the rule of the executable.
extern int Dtu_fix_cursed_scroll;
// ASCENT: the damage of a missile weapon of a creature adds the bonus of
// the bow or crossbow of the player and can use up the Potion of Luck of
// the player (0x2B0AE, 0x2B107 do not test the attacker). 1 (default):
// only the missiles of the player. 0 (-origcombat): the rule of the
// executable.
extern int Dtu_fix_creature_missile;
// The damage of a creature missile with Dtu_fix_creature_missile 0: the
// dice sum of weapon (n dice of sides) with the launcher bonus and the
// Potion of Luck of the player (0x2B0AE to 0x2B158).
int dtu_char_creature_missile_damage(int weapon, int n, int sides, int sum);
// 1 while the attack of the player is a backstab (0x13A955, dtuthief.c).
extern int Dtu_backstab;
// The option -autochoice N: the choice boxes answer N.
extern int Dtu_auto_choice;

// A choice box (0x655FE): returns the index of the choice, -1 for Escape.
int dtu_char_choice(char *text, int n, ...);
// 0xAE837: the item in the hand falls ("You dropped your weapon!").
void dtu_char_drop_weapon(void);
// 0x396B2: the potion timers end when their time is over, or all of them
// with force (Dispel Magic, Potion of Anti-Magic).
void dtu_char_potions_end(int force);
// The spell Strength (0x8FCFA): the gain and its end (0x90BE4).
void dtu_char_strength_gain(int gain, int warrior);
void dtu_char_strength_end(void);
// A script event of the item code (0xADB3B, 0xAC055). Weak in dtuchar.c.
void dtu_hook_script_event(int event);

// For the market (dtufe*.c): the gold of the character is Dtu_gold of
// dtuobj.h, the item value is dtu_item_data.value (dtu_item_get), the
// inventory is Dtu_inventory[] with dtu_inventory_add() and
// dtu_char_take().

//------------------------------------------------------------------------
// ASCENT: the rules of the items as values, for the description window
// (dtudesc.c). The item code reads the same values, so the text of the
// window and the game cannot disagree.

// What one use of a misc item does (use_misc, 0x37A6A)
#define DTU_MU_NONE			0		// no use
#define DTU_MU_HEAL			1		// lo to hi hit points, at most the maximum
#define DTU_MU_DEXTERITY	2		// dexterity at least lo for the time
#define DTU_MU_DRAGON		3		// strength at least lo for 1 to (class 1 level) minutes; a fighter gets a random percent value
#define DTU_MU_GIANT		4		// strength at least lo for the time
#define DTU_MU_INTEL		5		// intelligence lo to hi for the time
#define DTU_MU_EFFECT		6		// the effect fx for the time
#define DTU_MU_SPELLS		7		// casts the spells on the player
#define DTU_MU_DISPEL		8		// all spell and potion effects end
#define DTU_MU_LUCK			9		// for the time: one attack roll is a natural 20 or one damage roll the highest
#define DTU_MU_TOUGH		10		// lo to hi hit points above the maximum and Bless for the time; a second one while above the maximum: system shock
#define DTU_MU_GEM			11		// cures blindness, else the spell lo; one use for each mission
#define DTU_MU_SCROLL		12		// shows a text
#define DTU_MU_CURSED		13		// harm of chance; not used up
#define DTU_MU_TURN			14		// Spider Piece 0x12: Turn Undead as a cleric of level (class 1 level) * 2 + 1
#define DTU_MU_MAP			17		// Spider Piece 0x13: the whole map is known
#define DTU_MU_ATTACK		18		// Spider Piece 0x14: a missile at each creature in sight
#define DTU_MU_AREA_SPELLS	19		// Spider Piece 0x16: the area spell lo, then the spells on the player
#define DTU_MU_TOUR			20		// Spider Piece 0x17: the view visits each creature
#define DTU_MU_ABILITY		21		// Spider Piece 0x18: one point more of an ability (script 0x1FC)
#define DTU_MU_FULL_HEAL	22		// Spider Piece 0x1A: all hit points, then the spells on the player
// The Spider Pieces (DTU_MU_TURN to DTU_MU_FULL_HEAL and DTU_MU_SPELLS_KEEP) use one charge for each use.
#define DTU_MU_QUEST		15		// an event of the quest (a script)
#define DTU_MU_SPELLS_KEEP	16		// casts the spells on the player; four effects have no end

typedef struct dtu_misc_use {
	short	id;					// the misc number
	short	kind;					// DTU_MU_
	short	lo, hi;				// the range of the value of the kind
	short	t_mod, t_add, t_unit;	// the time: (rand() % t_mod + t_add) * t_unit seconds (t_mod 1: no roll; 0: none)
	int		fx;					// DTU_FX_ of an effect
	short	n_spells;
	short	spells[9];			// the spells, in the order of the cast
} dtu_misc_use;

// The use of misc number id, or NULL if it has no use.
const dtu_misc_use *dtu_char_misc_use(int id);
// The longest time of the Potion of Dragon's Blood in minutes, and the
// class 1 level number (from 0) of the Turn Undead of the Spider Piece 0x12,
// for the character of now.
int dtu_char_dragon_minutes(void);
int dtu_char_spider_turn_level(void);

// The values that a worn item sets in dtu_char_update (0xAFB55)
#define DTU_GIRDLE_STRENGTH		19		// armor 38, Girdle of Hill Giant Strength (belt)
#define DTU_OGRE_STRENGTH		18		// armor 52, Gauntlets of Ogre Power: 18/00
#define DTU_RING_STRENGTH		18		// armor 26, Ring of Strength: 18/00
#define DTU_RING_CONSTITUTION	18		// armor 21, Ring of Health
#define DTU_ELVENKIND_HIDE		95		// armor 43, Cloak of Elvenkind: Hide In Shadows
#define DTU_GLOVES_LOCKS_THIEF	10		// armor 50, Gloves of Dexterity: Open Locks + for a thief
#define DTU_GLOVES_LOCKS			37		// armor 50: Open Locks of another class
#define DTU_SKILL_LOSS			250		// the heavy armor, misc 9 (Gauntlets): the skills go to 0
#define DTU_WHIN_SKILL			250		// weapon 54, Sword of W'hin: Find/Remove Traps and Climb Walls
#define DTU_SKILL_MAX			95		// 0xB0B7E: each thief skill is 0 to 95

// The powers of a worn item (dtu_char_item_powers)
#define DTU_IP_STRENGTH		1		// value, value2 the percent value (100: 18/00)
#define DTU_IP_CONSTITUTION	2		// value
#define DTU_IP_DEXTERITY		3		// value more dexterity (the table of the gloves, for the character of now)
#define DTU_IP_HIDE			4		// Hide In Shadows becomes value
#define DTU_IP_LOCKS_THIEF	5		// Open Locks + value for a thief
#define DTU_IP_LOCKS_OTHER	6		// Open Locks of another class is value
#define DTU_IP_NO_SKILLS		7		// value 1: Hide In Shadows, Move Silently, Climb Walls; 2: Find/Remove Traps and Open Locks
#define DTU_IP_TRAPS_CLIMB	8		// Find/Remove Traps and Climb Walls value
#define DTU_IP_PROT_CLOAK		9		// no armor class with a shield, magic or heavy armor
#define DTU_IP_PROT_RING		10		// of two rings of protection the better one counts
#define DTU_IP_RING_NO_MAGIC	11		// no armor class with magic armor
#define DTU_IP_ARMORING		12		// a mage: 1 better after each cap of Armor, Magical Vestment and Shield
#define DTU_IP_WIZARDRY		13		// the mage slots of the spell levels 1 to 3 double
#define DTU_IP_USE_SPELL		14		// USE casts spell value around the player
#define DTU_IP_TURN_UNDEAD	15		// the key of Turn Undead works
#define DTU_IP_INFRA_HUMAN	16		// the Infravision effect only for a human
typedef struct dtu_item_power {
	int		kind, value, value2;
} dtu_item_power;
// Fills at most n powers of the item; returns the count.
int dtu_char_item_powers(int type, int id, dtu_item_power *p, int n);
// The effects (DTU_FX_) that the item gives while it is worn, or in the
// hand for a weapon (0xAFB55 with 0xB08A8).
int dtu_char_item_fx(int type, int id);
// The armor class that the item adds in its slot (negative is better), as
// dtu_char_update counts it before the rules that need other items.
int dtu_char_item_ac(int type, int id);
// The slot of the paper doll where the item goes, -1 for none, DTU_N_SLOTS
// for the hand (a weapon).
int dtu_char_item_slot(int type, int id);
// The Open Locks bonus of a lockpick item in the lockpick slot of a thief
// (0xB02BE), 0 for another item.
int dtu_char_lockpick_bonus(int id);

#endif
