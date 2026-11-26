/*
 * ASCENT: new file. The thief skills of Descent to Undermountain: Open
 * Locks, Find/Remove Traps, Move Silently, Hide In Shadows, Climb Walls
 * and the backstab. It has no part in Descent 1.
 *
 * The facts are in docs/re/combat.md, section B7. The address of the
 * original code (DTUGAME.EXE 1.3, object 1 at 0x10000) is at each function
 * of dtuthief.c.
 */
#ifndef _DTUTHIEF_H
#define _DTUTHIEF_H

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"

// The state bits of the player record P+0xA0
#define DTU_TS_HIDDEN			0x01		// Hide In Shadows made
#define DTU_TS_HIDE_FAILED		0x02		// Hide In Shadows tried and failed (the player is not told)
#define DTU_TS_SILENT			0x04		// Move Silently made
#define DTU_TS_SILENT_FAILED	0x08		// Move Silently tried and failed
#define DTU_TS_SEARCHING		0x10		// the search for traps is on
#define DTU_TS_CLIMB_SKILL		0x20		// the player holds a wall by the skill (not a ladder)
extern int Dtu_thief_bits;

// The skill roll 0xB3038: skill 0 Hide In Shadows, 1 Move Silently, 2 Open
// Locks, 3 Find/Remove Traps, 4 Climb Walls. Returns 1 if the d100 is at
// most the skill. *roll gets the d100.
int dtu_thief_roll(int skill, int *roll);

// The keys of the executable (kconfig items 54 to 59): M Move silently,
// F Find Traps, O Open Locks.
void dtu_thief_open_locks(void);			// 0xB31BD
void dtu_thief_search_toggle(void);			// 0xB3400
void dtu_thief_move_silently(void);			// 0xB4196

// After the load of a level file (gameseq.c): the thief bytes of the wall
// records start at 0 (0xAD67B). A level state restores them later.
void dtu_thief_level_loaded(void);
// 0xA6F89 at the level start (0xAD744): the water, lava and wood bits of
// the side flags and the secret door flag 0x40 of the doors, from the
// textures.
void dtu_thief_level_textures(void);
// 1 (default): a texture name of 0xA6F89 that the tables do not have
// matches no side. 23 of the 29 door names are in no table of 1.3
// ("dleft11#0" and so on; the tables have "dleft1#0"), so 0x1B2B1 gives
// texture 0 for them, and each door side with no overlay (tmap_num2 0)
// or with texture 0 becomes a secret door in 1.3. 0 (-origchar): as 1.3.
extern int Dtu_fix_secret_door_names;
// ASCENT: the backstab of AD&D 2nd Edition: from behind the target, and only
// a creature that sees the player stops it. 0 (-origcombat): 0xB337E.
extern int Dtu_fix_backstab;
// ASCENT: D-067, the Find/Remove Traps skill after the rolls of a magical
// trap. 0 (-origchar): doubled again (an odd skill stays 1 lower), and
// left halved after a find by the spell (0xB3CF9, 0xB3B5E). 1 (default):
// restored.
extern int Dtu_fix_find_traps_skill;

// One time in each frame (dtu_char_frame): the search for traps (0xB4040
// from game.c 0x438F3), Hide In Shadows (0xB333F from pcons.c 0x9DFDE).
void dtu_thief_frame(void);

// The use action on a side that is no door (0x951DE): the climb. walls 0:
// a ladder only (Activate with Dtu_fix_climb_key, kconfig.h).
void dtu_thief_climb_start(object *obj, int segnum, int side, int walls);
// ASCENT: the weapon in the hand at a climb (dtuthief.c). -origphys clears it.
extern int Dtu_fix_climb_weapon;
// ASCENT: the texts of a climb that the hand stops (dtuthief.c). -origphys clears it.
extern int Dtu_fix_climb_messages;
// ASCENT: the Climb Wall key (port): the use ray and the climb of 0x951DE
void dtu_thief_climb_key(object *obj);
// The climb in the movement of the player (pcons.c 0x9EBEB with 10.0):
// physics.c calls it in each frame while the player holds a wall. accel
// is the acceleration of the frame.
void dtu_thief_climb_frame(object *obj, vms_vector *accel);
// The climb thrust of the controls (0x2C144): forward goes up, back goes
// down. physics.c and controls.c call it.
void dtu_thief_climb_thrust(object *obj, fix forward, fix side);
// ASCENT: for Dtu_improved_physics (physics.c). The side of the segment
// that is a ladder in reach of the player, with the tests of 0x9EBEB
// (10.0, facing the player), or -1.
int dtu_thief_ladder_side(object *obj, int segnum);
// An attack ends Hide In Shadows and the search (mask 0xEC at 0x44402 and
// 0x5706E).
void dtu_thief_attack(void);
// 0xB337E: 1 if an attack of the player on the creature is a backstab.
int dtu_thief_backstab_check(object *target, int weapon);

// 1 if the creature does not see the player: invisible or Hide In Shadows
// (0x165C4 with 0x165F9).
int dtu_thief_hidden_from(object *creature);
// 1 if the creature does not notice the player because of Move Silently
// (0x13D8E, 0x142BF: the player is behind the creature).
int dtu_thief_unnoticed_by(object *creature);

// ASCENT: test (-chartest gosecret=N): puts the player in front of the
// secret wall N of the level (a secret door, or a closed wall with a
// trap on the side). Returns 0 if the level has fewer.
int dtu_thief_goto_secret(int n);
#endif
