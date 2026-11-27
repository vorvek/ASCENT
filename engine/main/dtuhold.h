/*
 * ASCENT: the player under Hold (the bit DTU_FX_HOLD, dtuspell.h). New
 * file. See docs/re/bugs.md (D28) and docs/re/spells.md.
 *
 * 1.3 (0x8E85A, case 22 and 52 of 0x8CFFF, Hold Person): the time is the
 * caster level times 120 seconds; on the player it is divided by wisdom /
 * 2. On the player the case clears the attack requests (0x13B69F,
 * 0x13B6A3) and the inputs (0x40E50) once. While the bit is on,
 * read_flying_controls (0x2BFAE) gives no thrust and
 * no turn, the physics of the player (0x9DC66) do not move it and Activate
 * (0x95050) does nothing. The look up and down, the attack, the cast and
 * the use of items have no test of the bit.
 */
#ifndef _DTUHOLD_H
#define _DTUHOLD_H

#include "fix.h"

// ASCENT: Hold Person on the player. 1 (default): the hold lasts 6 to 18
// seconds (a random whole second, no wisdom cut), and while the player has
// the Hold bit (Hold Person, the stun of Color Spray, the freeze of Fear
// and Scare) the player cannot move, turn, look up or down, jump, attack,
// cast, use or equip an item, or change the item in the hand. A cast that
// runs at the start of the hold is lost. The menus and the pause work.
// The one exception is the Potion of Freedom: it can be drunk while held
// (from the list or from a box and the attack key) and ends the hold
// (dtu_hold_free_player).
// 0 (-origcombat): 1.3. A creature that the player holds keeps the time of
// 1.3 (2 rounds per level, the AD&D 2nd edition rule).
extern int Dtu_fix_player_hold;

// ASCENT: the 3D view of the GPU renderer loses its color while the player
// is held (not during Fear, where the view is in a fleeing creature). The
// effect comes in and goes out in about half a second and ends with the
// hold. 1 (default); 0 with -origcombat or -noholdview. The software
// renderer keeps the effect clip of 1.3.
extern int Dtu_hold_view;

// Dtu_fix_player_hold after the options of the command line are read.
int dtu_hold_fix_on(void);

// The time of Hold Person on the player with Dtu_fix_player_hold.
fix dtu_hold_player_time(void);

// 1 while the controls of the player are blocked (Dtu_fix_player_hold and
// the Hold bit).
int dtu_hold_blocks_player(void);

// For an action of the player that a hold blocks: returns 1 (and shows
// "You are held and cannot act." at most once a second) when it is
// blocked, else 0.
int dtu_hold_refuse(void);

// dtu_hold_refuse for the use of an item of the type and number (-1: no
// item): the Potion of Freedom (misc 78) is not refused.
int dtu_hold_refuse_item(int type, int id);

// The Potion of Freedom with Dtu_fix_player_hold: the hold of the player
// ends ("Hold Person expired."), as the spell Free Action ends it (case
// 20 of 0x8CFFF). Free Action prevents Hold Person and the stun of Color
// Spray (both test it), so both end; the freeze of Fear and Scare does not
// test it and stays while Fear is on. 1.3: the potion only sets Free
// Action and the hold goes on.
void dtu_hold_free_player(void);

// The amount of the desaturation of the 3D view, 0 to 1. Call once for
// each view; it moves the fade of the effect by the real time.
float dtu_hold_view_amount(void);

// ASCENT: the player under Sleep (the bit DTU_FX_SLEEP: the spell Sleep and
// the unconsciousness of Color Spray, both with the blindness bit). 1.3
// (0x8CFFF, case 46 and case 6) moves the eye of the player to the other
// side of the center of the body (Player_eye_offset negated), puts the arms
// into state 9 and clears the attack requests once; read_flying_controls
// (0x2BFAE) and the physics do not test the bit, so the sleeping player
// can move, turn and look, and nothing on the screen says that the player
// sleeps (the port draws the normal view). A melee hit wakes the player
// (0x2A839 sets the timer to 0); a missile, a spell or a trap does not.
//
// 1 (default): the whole 3D view is black, with "zzz" in its center, until
// the sleep ends; the controls are blocked as by a hold
// (dtu_hold_blocks_player: no move, turn, look up or down, jump, attack,
// cast or item, the Potion of Freedom too). 0 with -origcombat or
// -nosleepview: the view and the controls of 1.3.
extern int Dtu_sleep_view;
// AD&D 2nd edition (Sleep: "slapping or wounding awakens affected
// creatures"): any damage to the player ends the sleep at once ("Sleep
// expired.", the blindness of the sleep ends too). 1 (default); 0 with
// -origcombat: only a melee hit wakes the player, as in 1.3.
extern int Dtu_fix_sleep_wake;

// 1 while the controls of the player are blocked by Dtu_sleep_view.
int dtu_sleep_blocks_player(void);
// 1 when the view of this frame is the black view of Dtu_sleep_view.
int dtu_sleep_view_on(void);
// Draws the black view with "zzz" on the canvas of the 3D view.
void dtu_sleep_view_draw(void);
// The player took damage: Dtu_fix_sleep_wake.
struct object;
void dtu_sleep_player_hurt(struct object *player);

#endif
