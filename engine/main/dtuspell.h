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
