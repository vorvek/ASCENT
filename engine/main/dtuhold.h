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

#endif
