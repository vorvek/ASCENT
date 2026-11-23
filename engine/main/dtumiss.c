/*
 * ASCENT: new file. The sound of a melee attack that hits nothing.
 *
 * 1.3 plays no sound for a swing that touches nothing, and for a missed
 * attack roll only the weapon key monster_miss_sound, which no weapon line
 * of the shipped tables sets (docs/re/combat.md, "Miss sound"). The port
 * plays the swing sound of the weapon in both cases: its flash_sound
 * (W+0x20), which 1.3 reads only for a missile, a thrown weapon and the
 * paging of the weapon in the hand. It is an improvement, not a
 * correction: the setting "Miss sound" (Dtu_miss_sound, MissSound in
 * ascent.cfg, on by default). -origsound and -nomisssound turn it off for
 * one run, -noimprovements clears the setting as the others.
 *
 * The sounds are game sounds of the archives, played through the sample
 * path of digi.c: 2400 (cmbt01a) for the blades and axes, 2420 (cmbt06a)
 * for the maces and hammers. The clubs have 2435, which has no $SOUND line
 * in BANE.BIN: the port plays 2436 (cmbt11b), the next sound of that group,
 * in its place. An empty hand, a weapon with no flash_sound and a creature
 * with no weapon line play 2400.
 *
 * A new sound of the player stops the last one (the sounds are 0.67 to
 * 0.83 s long, a swing of Haste lasts 0.5 s). The "air" sound of a swing
 * starts on the first frame of its hit test (dtucombat.c) and stops at once
 * if the swing then touches something (dtu_miss_sound_player_stop). The
 * timing does not depend on the sound. A new sound of a creature does not
 * start while the last one of a creature plays.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"
#include "args.h"
#include "digi.h"
#include "sounds.h"
#include "weapon.h"
#include "game.h"
#include "dtuobj.h"
#include "dtuopts.h"
#include "dtumiss.h"

#define MISS_VOLUME			(F1_0/2)		// as the contact sounds of a melee hit (0x27AF5)
#define MISS_VOLUME_CREATURE	(F1_0/4)
#define MISS_DISTANCE		i2f(200)		// as sound_on_object of dtucombat.c

#define MISS_SOUND_DEFAULT	2400			// cmbt01a, the blades and axes
#define MISS_SOUND_CLUB		2435			// the clubs: no $SOUND line in BANE.BIN
#define MISS_SOUND_CLUB_NEAR	2436		// cmbt11b, the next sound of the group

static int Miss_off = -1;				// -1: the options are not read yet
static int Miss_handle = -1;			// the last sound of the player
static int Miss_handle_creature = -1;	// the last sound of a creature

int dtu_miss_sound_ready(void)
{
	if (Miss_off < 0)
		Miss_off = FindArg("-origsound") || FindArg("-nomisssound");
	return !Miss_off && Dtu_miss_sound;
}

static int sound_exists(int s)
{
	return s >= 0 && s < MAX_SOUNDS && digi_xlat_sound(s) >= 0;
}

// The swing sound of a weapon (an index of Weapon_info, -1 for none)
static int weapon_sound(int weapon)
{
	int s = -1;

	if (weapon >= 0 && weapon < N_weapon_types)
		s = Weapon_info[weapon].flash_sound;
	if (s == MISS_SOUND_CLUB && !sound_exists(s))
		s = MISS_SOUND_CLUB_NEAR;
	if (!sound_exists(s))
		s = MISS_SOUND_DEFAULT;
	return s;
}

int dtu_miss_sound_player(const char *why, int weapon)
{
	int s;

	if (!dtu_miss_sound_ready())
		return 0;
	digi_stop_handle(Miss_handle);
	s = weapon_sound(weapon);
	Miss_handle = digi_play_sample_handle(s, MISS_VOLUME, NULL, 0);
	dtu_obj_log("combat: miss sound %d of the player (%s), handle %d, at %.3f", s, why, Miss_handle, f2fl(GameTime));
	return Miss_handle >= 0;
}

void dtu_miss_sound_player_stop(const char *why)
{
	if (!digi_handle_playing(Miss_handle))
		return;
	digi_stop_handle(Miss_handle);
	dtu_obj_log("combat: miss sound of the player stopped (%s), handle %d, at %.3f", why, Miss_handle, f2fl(GameTime));
	Miss_handle = -1;
}

void dtu_miss_sound_creature(object *attacker, int weapon)
{
	int s;

	if (attacker == NULL || !dtu_miss_sound_ready())
		return;
	if (digi_handle_playing(Miss_handle_creature)) {
		dtu_obj_log("combat: miss sound skipped (creature obj %d), the last one still plays", (int)(attacker - Objects));
		return;
	}
	s = weapon_sound(weapon);
	Miss_handle_creature = digi_play_sample_handle(s, MISS_VOLUME_CREATURE, attacker, MISS_DISTANCE);
	dtu_obj_log("combat: miss sound %d of creature obj %d %s, handle %d", s, (int)(attacker - Objects),
		dtu_object_name(attacker), Miss_handle_creature);
}
