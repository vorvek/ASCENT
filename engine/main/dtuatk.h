/*
 * ASCENT: the attack timing rules of the player (DTUGAME.EXE 1.3), as
 * functions of their inputs so that test_dtuatk.c can check them without
 * the game data. docs/re/combat.md, section A8.
 */

#ifndef _DTUATK_H
#define _DTUATK_H

#include "fix.h"

// The effect bits of the player object that change the timing (object
// +0x88; the same values as DTU_FX_SLOW and DTU_FX_HASTE of dtuspell.h).
#define DTU_ATK_HASTE	(1<<13)
#define DTU_ATK_SLOW		(1<<27)

// The time of the attack state of the player model (state_times[6]).
// Melee weapon or empty hand (0x441E9 to 0x44258): max(fire_wait, 1.0),
// two times that with Slow. A missile or thrown weapon keeps the time of
// the model (0x43A5A writes the time only for attack_type 1), two times
// that with Slow (0x8FBA3).
// fix_haste (Dtu_fix_haste_attack): Haste halves the time of a melee
// swing. The executable does not change it.
fix dtu_atk_state_time(int melee,fix fire_wait,fix model_time,int effects,int fix_haste);

// The time after an accepted melee attack until the gate 0x41E36 accepts
// the next one (the gate needs more than this time). state_time is the
// result of dtu_atk_state_time. The executable adds half of the state time
// with Haste (0x41EC3 to 0x41ED7): Haste is slower. With fix_haste the
// wait is the (halved) state time.
fix dtu_atk_melee_wait(fix state_time,int effects,int fix_haste);

// The time after a shot of a missile or thrown weapon until the next
// shot. 0: no wait. The executable has none: the fire timer of 0x56A0D
// starts again with each key press (0x56BB0), so fire_wait never counts.
// fix_rate (Dtu_fix_missile_rate): fire_wait, two times that with Slow.
// With fix_haste also: half of it with Haste.
fix dtu_atk_missile_wait(fix fire_wait,int effects,int fix_rate,int fix_haste);

// The time from the start of the attack state to its fire frame (0x44589,
// 0x56A0D): first and end are the first frames of the state and of the
// next state, fire_frame is fire_frames[0] of the model. With
// Dtu_fix_missile_rate a press may start the attack state this much
// before the end of the wait.
fix dtu_atk_fire_lead(fix state_time,int first,int end,int fire_frame);

// 0x44589: the frame of a state follows the time only if the integer part
// of the state time is not 0. With fix_haste an attack state with a
// shorter time (the halved swing) is animated too.
int dtu_atk_state_animates(fix state_time,int attack_state,int fix_haste);

// The item bits (0xAFB55) of the bow in the bow slot (the keyword
// pc_spell1= of its table line) or of the melee weapon in the hand (the
// keyword pc_spell=); -1: no keyword. The executable gives these items no
// effect: 0. fix_items (Dtu_fix_speed_items): spell 21 (Haste) gives item
// bit 9, the bit of the Ring of Quickness and the Boots of Speed.
int dtu_atk_item_bits(int pc_spell,int fix_items);

#endif
