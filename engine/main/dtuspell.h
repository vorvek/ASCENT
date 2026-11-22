/*
 * ASCENT: new file. The spells of Descent to Undermountain: the spell
 * books and the spell slots of the player, the cast (cast time, checks,
 * missiles), the effects of the spells on a target (spells.c 0x8CFFF), the
 * area effects (0x90CEF), the timers of the duration effects (0x9002C),
 * the saving throws (0xAE596), scrolls and wands. It has no part in
 * Descent 1.
 *
 * The facts are in docs/re/combat.md, part B. The address of the original
 * code (DTUGAME.EXE 1.3, object 1 at 0x10000) is at each function of
 * dtuspell.c.
 */
#ifndef _DTUSPELL_H
#define _DTUSPELL_H

#include "types.h"
#include "fix.h"
#include "object.h"

// Bits of the effect word of an object (dtu.m_88, object 0x88). The names
// of the strings of 0x9002C. physics.h has the bits that the movement
// reads (DTU_OF_).
#define DTU_FX_ARMOR			(1<<0)
#define DTU_FX_BLESS			(1<<1)
#define DTU_FX_BLIND			(1<<2)
#define DTU_FX_CHILL_TOUCH		(1<<3)
#define DTU_FX_DEAF			(1<<4)
#define DTU_FX_CONFUSION		(1<<5)
#define DTU_FX_DETECT_MAGIC	(1<<6)
#define DTU_FX_FEATHER_FALL	(1<<7)
#define DTU_FX_FEAR			(1<<8)
#define DTU_FX_PROT_FIRE		(1<<9)
#define DTU_FX_RESIST_FIRE		(1<<10)
#define DTU_FX_FLY				(1<<11)
#define DTU_FX_FREE_ACTION		(1<<12)
#define DTU_FX_HASTE			(1<<13)
#define DTU_FX_HOLD			(1<<14)
#define DTU_FX_INFRAVISION		(1<<15)
#define DTU_FX_IMP_INVIS		(1<<16)
#define DTU_FX_INVISIBLE		(1<<17)
#define DTU_FX_INVULNERABLE	(1<<18)
#define DTU_FX_JUMP			(1<<19)
#define DTU_FX_FIND_TRAPS		(1<<20)
#define DTU_FX_LEVITATE		(1<<21)
#define DTU_FX_MNEMONIC		(1<<22)
#define DTU_FX_PROT_EVIL		(1<<23)
#define DTU_FX_REGENERATE		(1<<24)
#define DTU_FX_SHIELD			(1<<25)
#define DTU_FX_SLEEP			(1<<26)
#define DTU_FX_SLOW			(1<<27)
#define DTU_FX_SPIDER_CLIMB	(1<<28)
#define DTU_FX_STONESKIN		(1<<29)
#define DTU_FX_STRENGTH		(1<<30)
#define DTU_FX_VESTMENT		((int)(1u<<31))

#define DTU_FX_NO_END			0x3FFFFFFF		// a timer that does not end

// The saving throw categories (the rows of the AD&D table)
#define DTU_SAVE_PARALYZE		0
#define DTU_SAVE_ROD			1
#define DTU_SAVE_PETRIFY		2
#define DTU_SAVE_BREATH		3
#define DTU_SAVE_SPELL		4

//------------------------------------------------------------------------
// Settings. 1 (default) is the corrected rule, 0 the rule of the
// executable. -origcombat clears them.

// The dice of Fireball, Lightning Bolt and Melf's Acid Arrows have 0 sides
// in all tables (fire_damage_dice=0, elec_damage_dice=0, no
// acid_damage_dice), and the cases set the number of the dice to 1 or
// more. 0x91BC3 divides by the sides (idiv at 0x91C8E): the executable
// would stop with a divide error on a hit. Corrected: a group with 0
// sides throws no dice, the damage is the part for each level (6 for each
// level for Fireball and Lightning Bolt, 4 for Melf's Acid Arrows), which
// is what dtu_spell_damage_roll() of dtuobj.c does. 0: the spell does no
// damage and the log tells that the executable faults here.
extern int Dtu_fix_zero_dice;

// Stone Skin (0x8FC7A) writes the whole effect word: all other effects of
// the target end without their end action. Corrected: only its bit is set.
extern int Dtu_fix_stoneskin_word;

// Rary's Mnemonic Enhancer (0x8F21D). The executable: one cast sets bit
// 22 and raises the slot maximum by three spell levels (the choice box);
// a second cast does nothing while the bit is set; the bonus stays until
// Dispel Magic or a Potion of Anti-Magic (0x9002C with force). The
// manual: each cast gives three more levels, the bonus is good for one
// rest period and ends at the next rest after the one that filled it.
// 1 (default): the rule of the manual. 0: the rule of the executable.
extern int Dtu_fix_rary_manual;

// The creature spells (0x9231F) take rand() % n with n = 0 or less for
// some spells (Scare at caster level 4 and up, Hold Person at a distance
// that is a multiple of 50). The executable has the saturating divide
// handler (0xF72AB): x % 0 is 0. This is kept. No setting.

//------------------------------------------------------------------------
// Calls for the items, the creatures and the combat code

// Sets an effect bit with a timer (the potions of 0x37A6A, the worn items
// of 0xB08A8 with DTU_FX_NO_END). keep_later: a timer that ends later
// stays (0x37A6A). message: the HUD line of the effect. Returns 1.
int dtu_spell_set_effect(object *obj, int mask, fix duration, int keep_later, int message);
// Ends an effect without a message (0xB0A98 with 1: a worn item is taken
// off). The end action of 0x9002C does not run.
void dtu_spell_clear_effect(object *obj, int mask);
// 0x9002C with force: all effects of the object end (Dispel Magic, Potion
// of Anti-Magic).
void dtu_spell_dispel(object *obj);
// 0x8CFFF(spell, target, caster) with the levels of 0x8CFFF (the player:
// 0x91EC1, a creature: 0xAE3BB). An area spell has its area around the
// target (0x90CEF).
fix dtu_spell_cast_on(int spell, object *target, object *caster);
// 0x9305D: a cast of the object stops. hit 1: by a hit (the slot is used,
// "Your spell was interrupted!"), 0: by the jump key ("Your spell was
// aborted."). Returns 1 if a cast stopped.
int dtu_spell_interrupt(object *obj, int hit);
// The AI step of a creature that casts (0x1490B: 0x92D99 chooses, 0x91FDC
// casts). active: the creature is active (+0x1CC == 2).
void dtu_spell_creature_frame(object *obj, fix dist, int active);
// The condition bits of a creature (+0x15E) that make it cast (0x92D99).
#define DTU_CSC_FIRE		0x0001		// a fire hit
#define DTU_CSC_ALERT		0x001C		// alerted (0x160AB levels 2 to 4)
#define DTU_CSC_MELEE_HIT	0x0080		// a melee hit (0x29D85)
#define DTU_CSC_SPELL_HIT	0x0100		// spell damage (0x91332)
#define DTU_CSC_WAKE		0x0200		// wakes up (0x13F09)
#define DTU_CSC_FLINCH	0x0400		// a hit that makes it flinch
#define DTU_CSC_EXPIRED	0x0800		// an effect ended (0x9002C)
#define DTU_CSC_SLOWED	0x2000		// slowed in its movement
#define DTU_CSC_MORALE	0x4000		// a failed morale test
void dtu_spell_creature_event(object *obj, int bits);
// The NPC table of the executable (0xA504A, 191 rows, read by 0xA7E97 at
// the end of each $MONSTER line): THAC0, armor class, mage level and
// points, cleric level and points by the name of the creature. Returns 1
// and fills the six values if the name is in the table.
int dtu_npc_row(char *name, int v[6]);
// The mage slots of the Ring of Wizardry (armor 27, 0xB00A5): levels 1 to
// 3 are doubled. n is the number of doublings (dtuchar.c).
void dtu_spell_set_wizardry(int n);
// 1 if the spell has type=AREA (Spell_info+0x36 bit 0).
int dtu_spell_is_area(int spell);
// 0x8CAD3: the cast vclip of a spell on an object (the potions use it).
void dtu_spell_cast_vclip(int spell, object *obj);
// 0x92FF2 ("randomizing spell table"): a rest shuffles the creature spell
// table.
void dtu_spell_rest_shuffle(void);

//------------------------------------------------------------------------
// The spell in the hand. The executable keeps it as the current item
// reference (P+0x1AD7, list 1 for a mage spell and 2 for a cleric spell).
// The port keeps the spell number here: -1 when the item in the hand of
// dtuchar.c is used.
extern int Dtu_spell_current;

// The spell books (0xB69AD: a spell that the character can learn goes to
// the list of its class). 0: mage, 1: cleric.
#define DTU_SPELL_BOOK_MAX		127
extern int Dtu_spell_book_n[2];
extern short Dtu_spell_book[2][DTU_SPELL_BOOK_MAX];

// The spell slots (P+0x23 to P+0x42): the maximum and the slots that are
// left for each spell level. Mage levels 1 to 9, cleric levels 1 to 7.
extern int Dtu_spell_slots_max[2][9];
extern int Dtu_spell_slots_left[2][9];

//------------------------------------------------------------------------
// Calls

// One time in each frame (dtu_combat_frame calls it): the timers of the
// effects of all objects (0x9002C), the cast that waits for its time,
// the homing missiles of the player, the setup at the start of a level.
void dtu_spell_frame(void);

// The attack key with a spell in the hand, or a scroll or a wand in the
// hand (0x41A4D and 0x56A0D). Returns 1 when the request is done (cast,
// or refused), 0 while the cast time runs.
int dtu_spell_fire(void);
// 1 if the item in the hand is a spell, a scroll or a wand: the attack
// key casts.
int dtu_spell_in_hand(void);

// Makes the spell the spell in the hand. Returns 1 if the spell is in a
// book of the character.
int dtu_spell_select(int spell);
// Adds a spell to the book of its class (0xB69AD). Returns 0 if added, 2
// if it is there already, 1 if no book takes it.
int dtu_spell_learn(int spell);
// ASCENT: the spell books and the spell slots in a player record of a
// save game (6967 bytes, docs/re/character.md). The writer keeps the
// bytes of the entries that the last read had.
void dtu_spell_books_from_record(const ubyte *record);
void dtu_spell_books_to_record(ubyte *record);
// The list (1 mage, 2 cleric) and the index of a spell for a reference of
// the save game. Returns 0 if no book has it.
int dtu_spell_book_ref(int spell, int *list, int *index);
// The spell of a reference of the save game, or -1.
int dtu_spell_of_ref(int list, int index);
// All slots are full again (0x3551B: rest, level start, new level).
void dtu_spell_restore_slots(void);
// The slot maximum from the class levels and the wisdom (0x3575B).
void dtu_spell_compute_slots(void);

// The effect of a spell on one target (0x8CFFF). The strong definition
// of the function of dtuobj.h. "level" is the level of the caster.
// Returns the damage that was done.
fix dtu_spell_hit(int spell, object *target, object *caster, int level);

// The area effect of a spell (0x90CEF): each player or creature in the
// radius that the point sees. Returns the number of objects.
int dtu_spell_area(int spell, vms_vector *pos, int segnum, object *caster, int level);

// The saving throw (0xAE596): 1 if the object saves.
int dtu_spell_save(object *obj, int category);

// The caster level of the player for a spell (0x91EC1).
int dtu_spell_player_level(int spell);

// The armor class of the player with the spell effects (0xB05C5): stage 0
// before the dexterity adjustment, 1 after it. dtu_char_update() calls it.
int dtu_spell_player_ac(int ac, int stage);
// The bonus of the effects of an attacker to the hit roll (0x2A849):
// Strength +1, Bless +1, Slow -4.
int dtu_spell_to_hit_bonus(object *attacker);
// The armor class of a creature with its spell effects (0x2ADAF to 0x2ADF9).
int dtu_spell_creature_ac(object *creature, int ac);

// The spell screen of the statistics screen (dtudisp.c): the books with
// the slots, and the keys comma, period and Enter that choose the spell in
// the hand.
void dtu_spell_screen_draw(int hires);
void dtu_spell_screen_keys(int prev, int next, int take);

// The log of the option -objlog.
void dtu_spell_log(char *format, ...);

#endif
