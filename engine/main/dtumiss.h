/*
 * ASCENT: new file. The sound of a melee attack that hits nothing
 * (dtumiss.c). 1.3 has none (docs/re/combat.md, "Miss sound").
 */
#ifndef _DTUMISS_H
#define _DTUMISS_H

#include "object.h"

// 1 if a miss sound can play: the setting "Miss sound" (Dtu_miss_sound of
// dtuopts.h) is on, no option turns it off for the run (-origsound,
// -nomisssound), and <hd>/sound/miss1.wav exists.
int dtu_miss_sound_ready(void);

// The melee swing of the player ends with no hit. why is "air" (the swing
// touched nothing) or "roll" (the attack roll missed), for the log. A new
// sound does not start while the last one of the player still plays.
void dtu_miss_sound_player(const char *why);

// The melee attack of a creature on the player missed: a miss sound at the
// place of the creature, at a lower volume.
void dtu_miss_sound_creature(object *attacker);

#endif
