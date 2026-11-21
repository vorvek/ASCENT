/*
 * ASCENT: audio test for a person. It is not an automatic test.
 *
 * It opens the real device and plays a pattern that it generates:
 *   0 s  a 441 Hz tone in the center
 *   1 s  the tone on the left, then on the right
 *   3 s  the tone moves from the left to the right in two seconds
 *   5 s  a looped 882 Hz tone that becomes quiet and loud again
 *   7 s  four short noise bursts: each start and stop must not click
 *   9 s  streamed music at 22050 Hz: a 330 Hz tone on the left and a
 *        660 Hz tone on the right, for two seconds
 *
 * Usage: test_audio_play [seconds]
 * With an argument, the program stops after that number of seconds.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "platform.h"

#define SAMPLE_RATE 11025
#define TWO_PI      6.28318530717958647692

static uint8_t Tone[SAMPLE_RATE];           // one second, 441 Hz
static uint8_t Noise[SAMPLE_RATE / 8];
static unsigned Music_pos;

static int music_callback(void *user, int16_t *buffer, int frames)
{
	int i;

	(void)user;
	for (i = 0; i < frames; i++) {
		buffer[i * 2] = (int16_t)(6000.0 * sin(TWO_PI * 330.0 * Music_pos / 22050.0));
		buffer[i * 2 + 1] = (int16_t)(6000.0 * sin(TWO_PI * 660.0 * Music_pos / 22050.0));
		Music_pos++;
	}
	return frames;
}

static uint64_t Start, Limit;

// Wait until `ms` milliseconds after the start. Returns 0 at the time limit.
static int wait_until(int ms)
{
	for (;;) {
		uint64_t now = plat_time_us() - Start;
		if (now >= Limit)
			return 0;
		if (now >= (uint64_t)ms * 1000)
			return 1;
		plat_sleep_ms(2);
	}
}

static void pattern(void)
{
	int h, i;
	const int vol = PLAT_AUDIO_VOLUME_MAX / 2;

	printf("tone, center\n");
	h = plat_audio_play(Tone, sizeof(Tone), SAMPLE_RATE, vol, PLAT_AUDIO_PAN_CENTER, 0);
	if (h < 0)
		printf("plat_audio_play failed\n");
	if (!wait_until(1000)) return;
	printf("channel ended: %s\n", plat_audio_is_playing(h) ? "NO" : "yes");

	printf("tone, left\n");
	plat_audio_play(Tone, sizeof(Tone), SAMPLE_RATE, vol, PLAT_AUDIO_PAN_LEFT, 0);
	if (!wait_until(2000)) return;
	printf("tone, right\n");
	plat_audio_play(Tone, sizeof(Tone), SAMPLE_RATE, vol, PLAT_AUDIO_PAN_RIGHT, 0);
	if (!wait_until(3000)) return;

	printf("tone, left to right\n");
	h = plat_audio_play(Tone, sizeof(Tone), SAMPLE_RATE, vol, PLAT_AUDIO_PAN_LEFT, 1);
	for (i = 0; i <= 100; i++) {
		plat_audio_set_pan(h, PLAT_AUDIO_PAN_RIGHT * i / 100);
		if (!wait_until(3000 + i * 20)) return;
	}
	plat_audio_stop(h);

	printf("882 Hz, quiet and loud\n");
	h = plat_audio_play(Tone, sizeof(Tone), SAMPLE_RATE * 2, vol, PLAT_AUDIO_PAN_CENTER, 1);
	for (i = 0; i <= 100; i++) {
		plat_audio_set_volume(h, vol * abs(50 - i) / 50);
		if (!wait_until(5000 + i * 20)) return;
	}
	plat_audio_stop(h);

	printf("noise bursts\n");
	for (i = 0; i < 4; i++) {
		h = plat_audio_play(Noise, sizeof(Noise), SAMPLE_RATE, vol, PLAT_AUDIO_PAN_CENTER, 1);
		if (!wait_until(7000 + i * 500 + 250)) return;
		plat_audio_stop(h);
		if (!wait_until(7000 + i * 500 + 500)) return;
	}

	printf("music stream, 330 Hz left and 660 Hz right\n");
	plat_audio_music_start(music_callback, NULL, 22050);
	if (!wait_until(11000)) return;
	plat_audio_music_stop();
	wait_until(11100);
}

int main(int argc, char **argv)
{
	int i;
	unsigned seed = 12345;

	for (i = 0; i < (int)sizeof(Tone); i++)
		Tone[i] = (uint8_t)(128 + 100.0 * sin(TWO_PI * 441.0 * i / SAMPLE_RATE));
	for (i = 0; i < (int)sizeof(Noise); i++) {
		seed = seed * 1103515245u + 12345u;
		Noise[i] = (uint8_t)(seed >> 24);
	}

	Limit = (argc > 1) ? (uint64_t)(atof(argv[1]) * 1000000.0) : (uint64_t)60 * 1000000;

	if (plat_audio_init()) {
		printf("plat_audio_init failed\n");
		return 1;
	}
	printf("device open\n");
	Start = plat_time_us();
	pattern();
	plat_audio_close();
	printf("device closed\n");
	return 0;
}
