/*
 * ASCENT: new file. The experience of the player, port improvements that
 * DTUGAME.EXE 1.3 does not have:
 *
 * 1. The kill line (Dtu_kill_xp_message, on by default, KillXpMessage of
 *    ascent.cfg, -killxpmessage and -nokillxpmessage for one run, off with
 *    -noimprovements). 1.3 writes "You killed X." in the message log when
 *    the player kills a creature (0x2A0C7, 0x915F6). The port adds the
 *    experience of the kill, and for a character with two classes the part
 *    of each class. A creature that dies by another cause (another creature,
 *    a trap, a script, Turn Undead) gives the player no experience: when the
 *    player can see it, a short line says so. Off: the 1.3 messages.
 * 2. Maximum hit points on level up (Dtu_max_hp_level_up, off by default,
 *    MaxHpOnLevelUp of ascent.cfg, "Maximum HP on level up" of the
 *    Improvements menu, -maxhplevelup and -nomaxhplevelup for one run). A
 *    level of a class that rolls its hit die (0xAF694) gets the full die.
 * 3. The next level window: a right click on the experience field of the
 *    interface screens or on a class line of the statistics screen shows
 *    the experience of each class and the experience of its next level.
 * 4. Session log lines (dtulog.h), also in a release build: each creature
 *    death with its cause, each award of experience, each level gained.
 *
 * docs/architecture.md lists the settings; docs/debug_logs.md the lines.
 */
#ifndef _DTUXP_H
#define _DTUXP_H

#include "object.h"

extern int Dtu_kill_xp_message;
extern int Dtu_max_hp_level_up;

// The cause of the next creature death with no player attacker, for the
// session log ("script kill", "Turn Undead"). NULL clears it. The death
// takes it.
void dtu_xp_cause(const char *cause);

// A creature dies (creature_dies, dtuobj.c). attacker: the attacker or NULL.
// weapon: the weapon number, or a negative number for a spell or direct
// damage. xp: the experience that the player gets (0 for none), or -1 when
// the player is not the attacker. Writes the message of the message log
// and the line of the session log.
void dtu_xp_creature_death(object *obj, object *attacker, int weapon, int xp);

// A creature object that goes away with no death of the damage function
// (a script, Abjure, a level fix, a squashed rat, lifeleft): the code names
// the cause, then sets OF_SHOULD_BE_DEAD (dtu_creature_remove) or deletes
// it. obj_delete() writes the session log line "Creature removed: ...".
void dtu_creature_remove_cause(object *obj, const char *cause);
void dtu_creature_remove(object *obj, const char *cause);
void dtu_xp_creature_removed(object *obj);
// The session log line of a Descent 1 path that the port stops for a
// creature (the explosion, apply_damage_to_robot): what, the creature.
void dtu_xp_creature_stopped(object *obj, const char *what);

// The session log line of an award (dtu_char_award_xp): the total, the
// part of each class, the experience of the classes after it.
void dtu_xp_log_award(int points, int each, int xp1, int xp2);
// The session log line of a level gained (check_levels): the class
// levels before and after (from 1), the hit points of the new levels.
void dtu_xp_log_level(int old1, int new1, int old2, int new2, int hp);

// The text of the next level window. Returns the number of lines.
int dtu_xp_next_level_text(char *out, int size);

// 5. The verbose combat log (Dtu_verbose_combat, off by default,
// VerboseCombatLog of ascent.cfg, "Verbose combat log" of the Improvements
// menu, -verbosecombat and -noverbosecombat for one run). The attack and
// damage code reports each number where it computes it; one line in the
// message log shows them after the hit or miss message of an attack by or
// on the player: "Roll d20 14 +2 = 16 vs 13: hit. Damage 1d8 5 +1 = 6."
// "vs" is the number needed (THAC0 minus the armor class in use).
extern int Dtu_verbose_combat;
void dtu_xp_roll_clear(void);
// The attack roll: the d20, each modifier (tag: a short name, LN), the
// total, the number needed and the result (2 critical, 1 hit, 0 miss, -1
// fumble, 3 a hit that Stoneskin takes).
void dtu_xp_hit_start(int d20);
void dtu_xp_hit_mod(int v, const char *tag);
void dtu_xp_hit_end(int total, int need, int result);
// The damage roll: the dice and their sum (a second call adds a second
// roll), the bonuses and a multiplier (m < 0: a division by -m, a save for
// half) in the order of the code, the total.
void dtu_xp_dmg_start(int n, int sides, int sum);
void dtu_xp_dmg_mod(int v, const char *tag);
void dtu_xp_dmg_mult(int m, const char *tag);
void dtu_xp_dmg_end(int total);
// A saving throw: the d20, the number needed, made or not.
void dtu_xp_save(int d20, int need, int made);
// The text of the reported numbers ("" when none).
const char *dtu_xp_roll_text(void);
// The line in the message log when the setting is on and the player is a or
// b, then the numbers are cleared.
void dtu_xp_roll_say(object *a, object *b);

#endif
