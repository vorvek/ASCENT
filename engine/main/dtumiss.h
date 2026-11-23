/*
 * ASCENT: new file. The sound of a melee attack that hits nothing
 * (dtumiss.c). 1.3 has none (docs/re/combat.md, "Miss sound").
 */
#ifndef _DTUMISS_H
#define _DTUMISS_H

#include "object.h"

// 1 if a miss sound can play: the setting "Miss sound" (Dtu_miss_sound of
// dtuopts.h) is on and no option turns it off for the run (-origsound,
// -nomisssound).
int dtu_miss_sound_ready(void);

// The melee swing of the player has no hit: the swing sound (flash_sound)
// of weapon, the index of Weapon_info in the hand (-1 for none). why is
// "air" (the swing touched nothing) or "roll" (the attack roll missed), for
// the log. A new sound stops the last one of the player. Returns 1 if a
// sound started.
int dtu_miss_sound_player(const char *why, int weapon);

// Stops the last miss sound of the player if it still plays: an "air"
// sound of a swing that then touches something. why is for the log.
void dtu_miss_sound_player_stop(const char *why);

// The melee attack of a creature on the player missed: the swing sound of
// weapon (its weapon line, -1 for none) at the place of the creature, at a
// lower volume.
void dtu_miss_sound_creature(object *attacker, int weapon);

#endif
