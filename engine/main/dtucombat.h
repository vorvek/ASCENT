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
// of the time. With Dtu_fix_missile_rate the wait of a missile weapon is
// halved too.
extern int Dtu_fix_haste_attack;

// The rate of missile and thrown weapons. The fire timer of 0x56A0D starts
// again when the function was not called in the last two frames (0x56BB0),
// and the request ends with each shot: fire_wait never limits the shots,
// each key press fires. Corrected: fire_wait from one shot to the next
// (two times with Slow, half with Haste), a press before that is consumed.
extern int Dtu_fix_missile_rate;

// Rats. Each table has the creature "Rat" with pass_thru. The executable
// sets the flag 0x40 of such a creature at each load (0x6A102), and each
// object query skips that flag (fvi_sub 0x3F38F): the swing, the shots,
// the use ray and the move of the player. So no weapon, missile or spell
// missile of the player can hit a rat that it can see. Only a rat that
// runs into a player with boots dies (0x26FE0, it goes to another
// segment). Corrected: the swing and the missiles of the player (and the
// target hint) reach a rat. The player still walks through rats.
extern int Dtu_fix_rat_hit;

// The body test of a missile (0x3EE70 with 0x4F7D5): the infinite cylinder
// test 0x4F4B6 fails when the entry distance is below 0, before the caps
// clip the ray. So a missile that starts above a body and within its radius
// (a steep shot down at a low or wide creature from close range) misses it,
// though the swing reaches it. Corrected: for a missile the caps decide (the
// ray enters through the top). -origcombat gives the rule of 1.3.
extern int Dtu_fix_body_ray;

// The swing and the look pitch. The weapon sphere of the swing (0x6CFF8,
// 0x7473F) is turned with the object orientation (+0x20), which has the
// heading only; the look pitch (0x13D9AE) turns only the view and the
// missiles. So a swing goes where a level look goes: a low creature that
// is close and at the center of the screen while the player looks down is
// not hit. Corrected: the sphere of each frame (its center and the end of
// its ray) is turned with the look pitch about the eye, so the swing
// follows the view at the same distance from the eye. At pitch 0 nothing
// changes. A wall or floor between the eye and the sphere stops it there.
// -origcombat gives the rule of 1.3.
extern int Dtu_fix_swing_pitch;

// The melee attack of a creature. A creature starts an attack on the player
// in reach (both radii plus 2.0, the goal state choice 0x1599F), and the hit
// is the weapon sphere of its model frames from frame 8 of the attack state
// to its end (0x6CFF8 from 0x9F2B4). The roll against the armor class is
// made only when the sphere touches the player (0x27D82, 0x29A9C, 0x2A7F3).
// So a step back during the animation avoids every melee attack, and the
// armor class does not count. Corrected: an attack that starts with the
// player in reach, in front of the creature (facing dot 0.75, 0x1396E) and
// with no wall between them is committed. When the sphere misses because
// the player moved, the roll and the hit are made at the first hit frame
// with the player out of reach, or at the last two frames of the state.
// The attack is lost if the creature dies, is held or loses the player
// (invisible, hidden), or if the player is farther than twice the reach or
// behind a wall or a closed door. -origcombat gives the rule of 1.3.
extern int Dtu_fix_attack_commit;

// Turn undead (0xADC46). When the number of creatures that a turn can
// destroy is used up, the executable clears the "can be tried" bit of the
// next undead before it stops (0xADF09), so that creature can never be
// turned again ("You can no longer attempt to turn the ...") although no
// attempt was made. Corrected: the bit stays. Also: the T entry of the
// table writes 0xFF over the d20 (0xAE1B8), so every later undead with a
// number is turned; corrected: the d20 stays. -origcombat clears it.
extern int Dtu_fix_turn_undead;

// Turn undead (key T, the Spider Piece 0x12): level is the cleric level
// from 1, or 0 for the levels and the cloak of the character. Returns the
// number of creatures that were turned or destroyed.
int dtu_combat_turn_undead(int level);

// ASCENT: the class nibble and bit 0x10 of a creature for turn undead
// (the checks of dtu_spell_priest_check), -1 for no creature.
int dtu_combat_priest_turn_bits(object *obj);

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

// ASCENT: the load of a saved game (dtufe.c) sets the game time of the
// save. The next dtu_combat_frame starts the arms again at the stand state
// and clears the times of the attack, as at a level start. Without this a
// load in the same level kept the swing and the times of before: with an
// earlier game time the swing did not end for seconds, the arms moved on
// their own and the hot keys did not change the weapon.
void dtu_combat_reset_view(void);

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

// ASCENT: test: swingat=N, a swing of the player that touches creature N
void dtu_combat_test_swing(int objnum);

// ASCENT: test (-chartest reach=N.M): the player turns to creature N with
// a level body and looks at it with the look pitch (M 0: at its center from
// the eye, 1: level, 2: fully up, 3: fully down), and the character log
// gets what the swing, a missile and the target hint reach.
void dtu_combat_reach_test(int objnum,int mode);

// ASCENT: test (-chartest pitch=P): the look pitch (fixang, + is down).
void dtu_combat_set_pitch(int pitch);

// The state of the arms, for tests and for the display.
int dtu_combat_view_state(void);
// 1 while the attack key was pressed and its attack or cast has not started
// (0x13B69F); also while the cast time of a spell runs.
int dtu_combat_attack_pending(void);
// 0x9305D: a cast of the player was interrupted or aborted.
void dtu_combat_cast_stopped(void);
// 0x2A949: a critical fumble that does not drop the weapon: the attack
// request ends and the arms go to state 10.
void dtu_combat_fumble(void);
int dtu_combat_view_frame(void);
int dtu_combat_view_model(void);

// The time of the attack state of the item in the hand (0x441E9):
// max(fire_wait, 1.0) for a melee weapon, the time of the model for a
// missile weapon or a spell; two times that with Slow (dtuatk.h).
fix dtu_combat_swing_time(void);
// ASCENT: 1 (default): the swing time follows the end of Slow. 0
// (-origcombat): it stays doubled until the item in the hand changes.
extern int Dtu_fix_slow_swing;
// ASCENT: 1 (default): the killing blow plays the hit sound of the weapon.
// 0 (-origcombat): its kill sound, which no table sets (0x27CCF).
extern int Dtu_fix_kill_sound;

// 0x90CEF for spell 16 (Fireball): the blindness and the deafness of a
// player or creature at dist from the point (only the fire trap reaches it).
void dtu_fireball_blind_deaf(object *o, fix dist);
#define DTU_FIREBALL_BLIND_DIST		6		// ASCENT: units from the point (0x90DDF)
#define DTU_FIREBALL_BLIND_SECONDS	1800	// the blindness lasts 1 to this (0x90F73)
// 0xA6ACB: the fire trap, scenery object 0x29 of level 0 (the Yawning
// Portal). dtu_break_dressing() calls it; it does nothing for others.
void dtu_fire_trap(object *d);

// Fear and Scare on the player (0x8E3C4, 0x8F5F4): a creature of type 0
// is gated in at the player and becomes the viewer until the end of the
// effect; the player is not drawn. duration is the time of the Fear.
void dtu_fear_view_start(object *player, fix duration);
// The end of Fear on the player (0x903D4): the creature goes, the player
// stands at the center of its segment with its orientation.
void dtu_fear_view_end(object *player);

// The use of the Spider Pieces 0x14 and 0x17 (0x385D3, 0x38268): their
// frame loops, one step at each game frame.
void dtu_spider_tour_start(int piece);

#endif
