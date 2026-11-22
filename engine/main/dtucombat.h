/*
 * ASCENT: new file. The weapon attack of the player of Descent to
 * Undermountain: the attack key, the swing of the weapon in the hand and
 * its hit test, missile and thrown weapons, and the arms and the weapon
 * that the player sees in front of the view. It replaces the placeholder
 * attack of dtuobj.c (dtu_player_attack) and the cockpit weapons of
 * Descent 1 (laser.c fire, the gun points of the ship).
 *
 * The facts are in docs/re/combat.md, part A. The address of the original
 * code (DTUGAME.EXE 1.3, object 1 at 0x10000) is at each function of
 * dtucombat.c.
 */
#ifndef _DTUCOMBAT_H
#define _DTUCOMBAT_H

#include "types.h"
#include "fix.h"
#include "object.h"

//------------------------------------------------------------------------
// Settings. 1 (default) is the corrected rule, 0 is the rule of the
// executable. The option -origcombat clears them all (and the settings of
// dtuspell.h).

// One hit roll for each swing. The executable clears the "one hit per
// smack" byte (object 0x1AF) before each call of 0x278B5, so a weapon that
// first touches a creature on frame 8 of the attack rolls once in each game
// frame while frame 8 lasts (t/16 seconds): about 4 rolls at 60 frames per
// second, 1 at 16. Corrected: one roll for each swing.
extern int Dtu_fix_one_hit;

// Haste and the attack rate. 0x41EC3 adds one half of the swing time to the
// time of the last attack while Haste (bit 13) is on, so the next swing
// can start 1.5 swing times later: Haste makes the attacks slower.
// Corrected (a guess of the intent): with Haste the swing takes one half
// of the time.
extern int Dtu_fix_haste_attack;

// Turn undead (0xADC46). When the number of creatures that a turn can
// destroy is used up, the executable clears the "can be tried" bit of the
// next undead before it stops (0xADF09), so that creature can never be
// turned again ("You can no longer attempt to turn the ...") although no
// attempt was made. Corrected: the bit stays. -origcombat clears it.
extern int Dtu_fix_turn_undead;

// Turn undead (key T, the Spider Piece 0x12): level is the cleric level
// from 1, or 0 for the levels and the cloak of the character. Returns the
// number of creatures that were turned or destroyed.
int dtu_combat_turn_undead(int level);

//------------------------------------------------------------------------
// Calls from the game

// controls.c, once in each frame of play: "held" is 1 while an attack key
// or button is down (Left Ctrl, Right Ctrl, the -testmove action
// "attack"). The attack starts on the key down (0x2FB10).
void dtu_combat_attack_key(int held);

// The attack of the player for a key down that is counted elsewhere (the
// fire button of the key table of Descent 1 in game.c). Returns 1 if a
// request was made. It is the entry that replaces dtu_player_attack().
int dtu_combat_player_attack(void);

// controls.c, once in each frame after dtu_combat_attack_key(): the
// animation of the arms (0x4450C), the hit test of the swing (0x6CFF8),
// the start of an attack (0x44300 and 0x41A4D) and the shot of a missile
// weapon (0x56A0D). It also calls dtu_spell_frame().
void dtu_combat_frame(void);

// render.c, after the mine is drawn and before the end of the 3d frame:
// the arms and the weapon of the player (render_object 0x67C5A for the
// viewer, draw_polygon_model 0x727A5).
void dtu_combat_draw_view(void);

// ASCENT: the target hint (Dtu_target_hint, dtuopts.h). 1 if the attack of
// the item in the hand would reach a hostile creature that it can hurt
// first, by the test of the attack itself (the swing sphere, the flight of
// a missile or of a spell missile), at the time of the last
// dtu_combat_frame(). The cursor of the play screen (dtudisp.c) shows it.
int dtu_combat_target_hint(void);

// The state of the arms, for tests and for the display.
int dtu_combat_view_state(void);
// 1 while the attack key was pressed and its attack or cast has not started
// (0x13B69F); also while the cast time of a spell runs.
int dtu_combat_attack_pending(void);
// 0x9305D: a cast of the player was interrupted or aborted.
void dtu_combat_cast_stopped(void);
int dtu_combat_view_frame(void);
int dtu_combat_view_model(void);

// The time of a swing of the weapon in the hand (the state time of the
// attack states, 0x441E9): max(fire_wait, 1.0), two times that with Slow.
fix dtu_combat_swing_time(void);

#endif
