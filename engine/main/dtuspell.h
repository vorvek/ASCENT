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

// Chill Touch (5): 1.3 casts it on the caster, who gets the effect for
// L + 3 minutes (case 0x8D633). No table has self_cast_ok on line 5, but
// the table fixup 0x1A344 sets the bit (0x1A65E, Spell_info +0x37 bit 0)
// that the cast tests (0x56D36), and the port applies the fixup
// (dtutblfix.h). This setting gives the same cast on the caster with
// -rawtables. It is not an ASCENT correction: -origcombat keeps it.
extern int Dtu_fix_chill_touch;

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

// Melf's Acid Arrows (line 36, 0x8EFE0). The executable: no attack roll,
// 2 (L/3) + 2 acid dice in one hit, a save for half. The table fixup
// 0x1A344 sets the acid dice to 4 sides and no part for each level, so
// the dice are d4 (the table text alone gives 0 sides and 4 L). The
// game has no text of the spell and the manual tells little, so the port follows the AD&D 2nd edition
// rule: one arrow, an attack roll with the THAC0 of a fighter of the
// caster level against the armor class of the target (a natural 1
// misses, a natural 20 hits), no other bonus. A hit does 2d4 acid damage,
// and 2d4 more at the start of each of the next L/3 rounds (ACID_ROUND
// in dtuspell.c, 60.0 seconds). No save. The acid stops when the target dies or the level ends.
// The acid of the later rounds is not in the level state: it ends at a
// save and load. 1 (default): the 2nd edition rule. 0: the executable.
extern int Dtu_fix_melf_acid;

// Strength (50) and Infravision (25) compute the duration in 32 bits: it
// wraps from caster level 10 (Strength) and 8 (Infravision) and the effect
// ends at once. Corrected: no wrap; a duration stops at 4 hours 33 minutes,
// the longest timer of the port below DTU_FX_NO_END.
extern int Dtu_fix_spell_hours;
// ASCENT: Cure Light Wounds and Cure Serious Wounds (0x8DE4B, 0x8DF37) add
// the points and then cut the hit points to the maximum, so a cure on a
// character with the extra hit points of Aid takes them away. A healing
// spell that lowers the hit points is a defect; in AD&D the extra hit
// points of Aid go first. 1 (default): a cure does not lower the hit
// points. 0: the executable. -origcombat clears it.
extern int Dtu_fix_cure_aid;
// ASCENT: D-105, the Prayer of the player (0x56D6A) writes the flag word
// +0x36 of spell 38 as 0x102 at each cast; the record keeps it until the
// next table read. 1 (default): the flags of the table line. 0: after the
// first Prayer of the player the line reads as 0x102 (TARGET, stackable,
// cast with the hands, self_cast_ok, no other flag). -origcombat clears it.
extern int Dtu_fix_prayer_flags;
// ASCENT: the tables were read again (bmread.c, after the table fixup):
// the writes of the casts into the spell records end.
void dtu_spell_table_read(void);

// ASCENT: the checks of the priest spells and of turn undead (the
// -chartest command priestcheck). Writes "priestcheck: PASS ..." or
// "priestcheck: FAIL ..." lines into the -charlog file and returns the
// number of failures.
int dtu_spell_priest_check(void);

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
// 0x29F3E, 0x2A4FB, 0x913AF: damage of a weapon (or of a spell on a
// creature) also lowers the Armor pool (+0x90); below 1.0 it is 0, and the
// effect ends in the next frame with "Armor expired.".
void dtu_spell_armor_hit(object *obj, fix damage);
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
// 0x2A637: the counter of Hold Person (22, 52) is 1 before a touch spell.
void dtu_spell_touch_counter(int spell);
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
// The spell state of a creature of a restored level (0xAE3F2): the points
// of the type, the times of the restore
void dtu_spell_creature_restore(object *obj);
// The cast of the player in a state file (0x153EBC, Ai_local_info[0]
// +0x0C): the running cast time; the end of a save (restart 1: the cast
// starts again, 0x164B3); the cast time of a loaded save, which the level
// start gives back when the spell is in the hand.
fix dtu_spell_cast_timer(void);
void dtu_spell_cast_saved(int restart);
void dtu_spell_cast_load(fix timer);
// D-134: the spell points of a creature for the state file, and the saved
// points that a restored creature keeps (Dtu_fix_restore_spells, dtustate.c)
void dtu_spell_creature_points(object *obj, int *mage, int *cleric);
void dtu_spell_creature_keep_points(object *obj, int mage, int cleric);
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

// ASCENT: the test aid -chartest spelltest=SPELL.LEVEL.OBJ.TRIALS (OBJ -1:
// the player; TRIALS 0: one cast that stays). The results go to -objlog.
void dtu_spell_test(int spell, int level, int objnum, int trials);

// The attack key with a spell in the hand, or a scroll or a wand in the
// hand (0x41A4D and 0x56A0D). Returns 1 when the request is done (cast,
// or refused), 0 while the cast time runs.
int dtu_spell_fire(void);
// The homing missile of a spell from the player (Spider Piece 0x14, 0x387B0).
void dtu_spell_piece_missile(int spell);
// 1 if the item in the hand is a spell, a scroll or a wand: the attack
// key casts.
int dtu_spell_in_hand(void);
// ASCENT: the target hint (Dtu_target_hint). The object that the missile of
// the spell in the hand would hit if it were cast now (the homing target,
// or the first body on the line of the flight), -1 for none, -2 if the
// spell makes no missile (a spell on the caster).
int dtu_spell_hint_target(void);

// Makes the spell the spell in the hand. Returns 1 if the spell is in a
// book of the character.
int dtu_spell_select(int spell);
// Adds a spell to the book of its class (0xB69AD). Returns 0 if added, 2
// if it is there already, 1 if no book takes it.
int dtu_spell_learn(int spell);
// The spell part of the item insert 0xB65BB: -1 when the spell is not for a
// list of the character (it goes to the item list), else the result of
// dtu_spell_learn.
int dtu_spell_insert(int spell);
// A cleric gets every cleric spell of the spell levels of the cleric level
// (0x8BDB4 at the creation, 0xAF8F5 at each new level). Returns the count.
int dtu_spell_grant_cleric(void);
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
// The icon (byte 2 of a list entry), the spell level (word 6) and the name
// of a spell, for the hot key boxes and the spell lists of the inventory
// screen (0x329CB, 0xB7487).
int dtu_spell_icon(int spell);
int dtu_spell_level(int spell);
char *dtu_spell_name(int spell);
// 0x3533C for a spell entry: 1 if the character can use the spell.
int dtu_spell_usable(int spell);
// Makes the spell the spell in the hand with no message (0x43A5A for a
// hot key box that holds a spell). Returns 1 if a book has the spell.
int dtu_spell_take(int spell);
// The spell in the hand is put away: an item is taken in the hand.
// The letter of the list entry of the spell in the hand (K hands, Q wand,
// N scroll, B arrow), 0 with no spell in the hand. The model of the arms.
int dtu_spell_hand_letter(void);
void dtu_spell_put_away(void);
// All slots are full again: the rest (0x3608B calls 0x3551B).
void dtu_spell_restore_slots(void);
// ASCENT: the other two callers of 0x3551B. dtu_spell_fill_slots() fills
// the slots of a new character (0x8BFFE). dtu_spell_fill_at_start() sets
// the flag 0x141C38: 1 when the player leaves Waterdeep (0x8C279), 0 after
// a load (0xBBA8A). The game start (game() 0x42227) fills the slots when
// the flag is set and the start level is 1 or below; then the flag is 0.
// A level change and a load keep the slots left. The port value 2 fills
// at any start level (the direct start of a new character, -level).
void dtu_spell_fill_slots(void);
void dtu_spell_fill_at_start(int on);
// ASCENT: the level start (StartNewLevelSub, through dtu_char_level_start)
void dtu_spell_level_start(void);
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
// ASCENT: the save number of the player for a category 0 to 4 (test).
int dtu_spell_player_save_number(int category);

// The caster level of the player for a spell (0x91EC1).
int dtu_spell_player_level(int spell);
// ASCENT: the caster level of an object (the player: 0x91EC1, a creature:
// 0xAE3BB, another object: 1), for statement EB of the scripts.
int dtu_spell_object_level(object *obj, int spell);

// The armor class of the player with the spell effects (0xB05C5): stage 0
// before the dexterity adjustment, 1 after it. dtu_char_update() calls it.
int dtu_spell_player_ac(int ac, int stage);
// The armor class of a creature with its spell effects (0x2ADAF to 0x2AE1D).
int dtu_spell_creature_ac(object *creature, int ac);
// ASCENT: the same against a melee attack (missile 0) or a missile (1).
// They differ only with Shield (Dtu_fix_shield_ac).
int dtu_spell_player_ac_vs(int ac, int stage, int missile);
int dtu_spell_creature_ac_vs(object *creature, int ac, int missile);
// ASCENT: Shield and the Amulet of Shield (spell 45, DTU_FX_SHIELD). 1
// (default): armor class 4 against melee weapons and 2 against missiles, a
// decision of the user (the spell text of AD&D). 0 (-origcombat): 3 against
// both, as 1.3. dtu_spell_shield_ac() gives the value.
extern int Dtu_fix_shield_ac;
int dtu_spell_shield_ac(int missile);
// 0x16636: 1 if the object is an invisible creature that the player does
// not see.
int dtu_spell_unseen(object *obj);

// The spell screen of the statistics screen (dtudisp.c): the books with
// the slots, and the keys comma, period and Enter that choose the spell in
// the hand.
void dtu_spell_screen_draw(int hires);
void dtu_spell_screen_keys(int prev, int next, int take);

// The log of the option -objlog.
void dtu_spell_log(char *format, ...);

//------------------------------------------------------------------------
// ASCENT: the rules of a spell as values, for the description window
// (dtudesc.c). dtu_spell_get_facts reads the helpers that the cases of the
// effect function use (the duration, the amount, the damage dice, the
// missile count), so the text and the cast cannot disagree.

#define DTU_SS_NONE		0		// no saving throw
#define DTU_SS_HALF		1		// a save halves the damage
#define DTU_SS_NEGATES	2		// a save stops the effect
#define DTU_SS_SOME		3		// a save only for some targets (save_level, DTU_SR_SAVE_KIN)

#define DTU_SK_NONE			0		// the effect function has no code for it
#define DTU_SK_DAMAGE		1		// the damage dice
#define DTU_SK_HEAL			2		// amount hit points
#define DTU_SK_GIVE			3		// gives the effect fx
#define DTU_SK_CAUSE			4		// causes the effect fx on the target
#define DTU_SK_AC			5		// the effect fx: the armor class is at most ac
#define DTU_SK_ARMOR			6		// armor class at most ac until amount points of weapon damage
#define DTU_SK_AID			7		// amount hit points above the maximum and Bless
#define DTU_SK_DISPEL		8		// all effects of the target end
#define DTU_SK_ABJURE		9		// an extraplanar creature leaves or dies
#define DTU_SK_RARY			10		// more spell slots
#define DTU_SK_KNOCK			11		// opens a locked door
#define DTU_SK_CURE_SENSES	12		// cures blindness and deafness
#define DTU_SK_FIRE_POOL		13		// takes amount points of fire damage
#define DTU_SK_STONESKIN		14		// stops amount attacks
#define DTU_SK_STRENGTH		15		// amount more strength
#define DTU_SK_BLIND			16		// blinds the target (light)
#define DTU_SK_COLOR			17		// sleep, blindness or stun by the level of the target
										// 18 and 19 (Sleep, Scare) are DTU_SK_CAUSE with DTU_SR_LEVEL_CAP
#define DTU_SK_HOLD_UNDEAD	20		// holds undead of level max_level or less
#define DTU_SK_FEAR			21		// fear; the target may drop its weapon
#define DTU_SK_ACID			22		// the dice now and in each of amount later rounds
#define DTU_SK_HOLD			23		// holds up to amount humanoids

// ASCENT: the conditions of a spell line (Spell_rule of dtuspell.c). The
// cases of the effect function test them through one predicate, and the
// window prints the lines that they give, so a change of a flag changes the
// game and the text.
#define DTU_SR_SPARES_CASTER	0x01	// no effect on the caster ("Also hits you" for an area without it)
#define DTU_SR_UNDEAD_ONLY	0x02	// only undead targets
#define DTU_SR_NO_UNDEAD		0x04	// no effect on undead
#define DTU_SR_HUMANOID		0x08	// a creature target must be a humanoid (the player is one)
#define DTU_SR_LEVEL_CAP		0x10	// no effect on a target above level max_level
#define DTU_SR_SAVE_KIN		0x20	// the save only for an elf, a half-elf or a cleric (Scare)
#define DTU_SR_STOPPED		0x40	// the effect stopped_by on the target stops the spell
#define DTU_SR_BURST_BLINDS	0x80	// blinds and deafens near the point of the area (Fireball)

typedef struct dtu_spell_facts {
	int	book;					// 0 mage, 1 cleric, -1 none
	int	level;					// the spell level
	int	caster_level;			// L of the rules below
	int	cast_time;				// seconds
	int	self;					// cast on the caster
	int	area;					// 1: an area
	int	radius;					// the radius of the area in units
	int	range;					// the flight of a missile in units, 0 on the caster
	int	missiles_lo, missiles_hi;
	int	save;					// DTU_SS_
	int	kind;					// DTU_SK_
	int	fx;						// the effect bit (DTU_FX_), 0 none
	int	time_lo, time_hi;		// the duration in seconds; -1 no end; 0 none
	int	amount_lo, amount_hi;	// the amount of the kind
	int	ac;						// the armor class at most (DTU_SK_AC, DTU_SK_ARMOR), against melee
	int	ac_missile;				// ASCENT: the same against missiles (Shield, Dtu_fix_shield_ac)
	int	dice_n[4], dice_sides[4], dice_add[4];	// damage: normal, fire, acid, electric
	int	undead_die;				// sides of one more die against undead, 0 none
	int	blind_dist;				// Fireball: blinds and deafens within this many units of the point, 0 none
	int	blind_seconds;			// the longest blindness of blind_dist
	int	flags;					// ASCENT: DTU_SR_
	int	max_level;				// the highest level of a target (DTU_SR_LEVEL_CAP), else -1
	int	save_level;				// only a target above this level saves, -1 every target
	int	stopped_by;				// the effect bit that stops the spell (DTU_SR_STOPPED), else 0
} dtu_spell_facts;

// The facts of a spell at caster level L (-1: the level of the player).
// Returns 0 if the spell has no table line.
int dtu_spell_get_facts(int spell, int L, dtu_spell_facts *f);
// The armor class of a player of armor class 10 with the effects of the
// mask (Armor, Magical Vestment, Shield); 10 with none of them.
int dtu_spell_fx_ac(int fx);
// ASCENT: the same against melee (missile 0) or missiles (1), Shield
int dtu_spell_fx_ac_vs(int fx, int missile);
// ASCENT: the spell that the effect bits of fx stop on their owner
// (DTU_SR_STOPPED of the rule), -1 none
int dtu_spell_stopped_by_fx(int fx);
// ASCENT: the check of the rule table against the kinds of the sentences
// (dtu_desc_log_all, and once at the first level in a debug build).
// Returns the number of problems; each goes to the -objlog file.
int dtu_spell_rule_check(void);
// The classes that can use the spell as an item (dtu_spell_usable), as
// DTU_USE_ bits of dtuchar.h.
int dtu_spell_use_classes(int spell);
// The English name of an effect bit (the names of 0x9002C), "" for none.
const char *dtu_spell_fx_name(int mask);

#endif
