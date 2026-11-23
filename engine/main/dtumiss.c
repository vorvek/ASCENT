/*
 * ASCENT: new file. The sound of a melee attack that hits nothing.
 *
 * 1.3 plays no sound for a swing that touches nothing, and for a missed
 * attack roll only the weapon key monster_miss_sound, which no weapon line
 * of the shipped tables sets (docs/re/combat.md, "Miss sound"). The port
 * plays a short whoosh in both cases. It is an improvement, not a
 * correction: the setting "Miss sound" (Dtu_miss_sound, MissSound in
 * ascent.cfg, on by default). -origsound and -nomisssound turn it off for
 * one run, -noimprovements clears the setting as the others.
 *
 * The sounds are not game data: <hd>/sound/miss1.wav, miss2.wav, ... (up to
 * MISS_MAX, the first missing number ends the list), PCM, signed 16 bits,
 * one channel, any rate (22050 Hz as the sounds of the game). They come
 * with the content pack "sounds" (docs/install.md). Without the files
 * nothing plays and nothing fails.
 *
 * A sound is chosen at random, never the same one two times in a row. The
 * choice has its own random numbers, so the rolls of the game do not
 * change. A new sound of the player does not start while the last one
 * plays (a swing of Haste lasts 0.5 s, a sound about 0.27 s); the same
 * holds for the creatures.
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
#include "dtuobj.h"
#include "dtuopts.h"
#include "dtumiss.h"

#define MISS_MAX			8
#define MISS_VOLUME			(F1_0/2)		// as the contact sounds of a melee hit (0x27AF5)
#define MISS_VOLUME_CREATURE	(F1_0/4)
#define MISS_DISTANCE		i2f(200)		// as sound_on_object of dtucombat.c

static short *Miss_data[MISS_MAX];
static int Miss_length[MISS_MAX];
static int Miss_rate[MISS_MAX];
static int Miss_count = -1;			// -1: not loaded yet
static int Miss_off = -1;				// -1: the options are not read yet
static int Miss_last = -1;				// the last sound, for no repeat
static int Miss_handle = -1;			// the last sound of the player
static int Miss_handle_creature = -1;	// the last sound of a creature
static unsigned int Miss_seed = 0x2545f491;

static unsigned int le16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned int le32(const unsigned char *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned int)p[3] << 24); }

// Reads a WAV file of 16-bit mono PCM. Returns 1 if it is usable.
static int load_wav(const char *path, int i)
{
	FILE *f = fopen(path, "rb");
	unsigned char *b;
	long size;
	unsigned int pos, rate = 0, ok_fmt = 0;

	if (f == NULL)
		return 0;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (size < 44 || size > 4 * 1024 * 1024 || (b = malloc(size)) == NULL) {
		fclose(f);
		return 0;
	}
	if (fread(b, 1, size, f) != (size_t)size || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) {
		fclose(f);
		free(b);
		dtu_obj_log("sound: %s is not a WAV file", path);
		return 0;
	}
	fclose(f);
	for (pos = 12; pos + 8 <= (unsigned int)size; ) {
		unsigned int n = le32(b + pos + 4);

		if (n > (unsigned int)size - pos - 8)
			break;
		if (!memcmp(b + pos, "fmt ", 4) && n >= 16) {
			rate = le32(b + pos + 12);
			ok_fmt = le16(b + pos + 8) == 1 && le16(b + pos + 10) == 1 && le16(b + pos + 22) == 16 &&
				rate >= 4000 && rate <= 96000;
		}
		else if (!memcmp(b + pos, "data", 4) && ok_fmt && n >= 2) {
			int k, len = n / 2;
			short *d = malloc(len * sizeof(short));

			if (d == NULL)
				break;
			for (k = 0; k < len; k++)
				d[k] = (short)le16(b + pos + 8 + 2 * k);
			Miss_data[i] = d;
			Miss_length[i] = len;
			Miss_rate[i] = rate;
			free(b);
			return 1;
		}
		pos += 8 + n + (n & 1);
	}
	free(b);
	dtu_obj_log("sound: %s is not 16-bit mono PCM", path);
	return 0;
}

static void load(void)
{
	const char *hd = dtu_hd_dir();
	char path[700];
	int i;

	Miss_count = 0;
	if (hd == NULL)
		return;
	for (i = 0; i < MISS_MAX; i++) {
		snprintf(path, sizeof(path), "%s/sound/miss%d.wav", hd, i + 1);
		if (!load_wav(path, i))
			break;
		Miss_count++;
	}
	dtu_obj_log("sound: %d miss sounds in %s/sound", Miss_count, hd);
}

int dtu_miss_sound_ready(void)
{
	if (Miss_off < 0)
		Miss_off = FindArg("-origsound") || FindArg("-nomisssound");
	if (Miss_off || !Dtu_miss_sound)
		return 0;
	if (Miss_count < 0)
		load();
	return Miss_count > 0;
}

// A sound number, not the last one
static int choose(void)
{
	int i;

	Miss_seed = Miss_seed * 1103515245 + 12345;
	if (Miss_count < 2)
		return 0;
	i = (int)((Miss_seed >> 16) % (unsigned int)(Miss_count - 1));
	if (i >= Miss_last && Miss_last >= 0)
		i++;
	return i;
}

void dtu_miss_sound_player(const char *why)
{
	int i;

	if (!dtu_miss_sound_ready())
		return;
	if (digi_pcm16_playing(Miss_handle)) {
		dtu_obj_log("combat: miss sound skipped (%s), the last one still plays", why);
		return;
	}
	i = choose();
	Miss_last = i;
	Miss_handle = digi_play_pcm16(Miss_data[i], Miss_length[i], Miss_rate[i], MISS_VOLUME, NULL, 0, 0);
	dtu_obj_log("combat: miss sound %d of the player (%s), handle %d", i + 1, why, Miss_handle);
}

void dtu_miss_sound_creature(object *attacker)
{
	int i;

	if (attacker == NULL || !dtu_miss_sound_ready())
		return;
	if (digi_pcm16_playing(Miss_handle_creature)) {
		dtu_obj_log("combat: miss sound skipped (creature obj %d), the last one still plays", (int)(attacker - Objects));
		return;
	}
	i = choose();
	Miss_last = i;
	Miss_handle_creature = digi_play_pcm16(Miss_data[i], Miss_length[i], Miss_rate[i], MISS_VOLUME_CREATURE,
		&attacker->pos, attacker->segnum, MISS_DISTANCE);
	dtu_obj_log("combat: miss sound %d of creature obj %d %s, handle %d", i + 1, (int)(attacker - Objects),
		dtu_object_name(attacker), Miss_handle_creature);
}
