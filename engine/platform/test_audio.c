/*
 * ASCENT: test of the mixer core of audio.c. It does not open a device.
 * It renders the mixer output into a buffer with plat_audio_mixer_render
 * and examines the numbers.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "plat_audio.h"

#define RATE        44100
#define TONE_RATE   11025
#define TONE_HZ     441             // 25 samples for each period at 11025 Hz
#define TONE_LEN    2500            // 100 periods, so that the loop has no step
#define FULL        (127 * 256)     // output of sample value 255 at gain 1
#define SQRT_HALF   0.70710678f

static int Failures;
static uint8_t Tone[TONE_LEN];
static uint8_t Dc[TONE_RATE];       // one second of the value 255
static int16_t Tone16[TONE_LEN * 2];    // the same tone as 16-bit data at 22050 Hz
static int16_t Dc16[TONE_RATE];     // one second of the value 32767
#define FULL16      32767           // output of the 16-bit value 32767 at gain 1
static int16_t Out[RATE * 2 * 2];   // two seconds of stereo output

#define CHECK(cond) \
	do { \
		if (!(cond)) { \
			printf("FAIL line %d: %s\n", __LINE__, #cond); \
			Failures++; \
		} \
	} while (0)

static void render(int frames)
{
	plat_audio_mixer_render(Out, frames);
}

// side is 0 for left and 1 for right.
static int peak(int side, int from, int to)
{
	int i, p = 0;
	for (i = from; i < to; i++)
		if (abs(Out[i * 2 + side]) > p)
			p = abs(Out[i * 2 + side]);
	return p;
}

static int max_step(int side, int from, int to)
{
	int i, m = 0;
	for (i = from + 1; i < to; i++) {
		int d = abs(Out[i * 2 + side] - Out[(i - 1) * 2 + side]);
		if (d > m)
			m = d;
	}
	return m;
}

static int zero_crossings(int side, int from, int to)
{
	int i, n = 0;
	for (i = from + 1; i < to; i++)
		if ((Out[(i - 1) * 2 + side] < 0) != (Out[i * 2 + side] < 0))
			n++;
	return n;
}

static int near(int value, int expected, int tolerance)
{
	return abs(value - expected) <= tolerance;
}

static void test_silence(void)
{
	plat_audio_mixer_init(RATE);
	memset(Out, 0x55, sizeof(Out));
	render(RATE);
	CHECK(peak(0, 0, RATE) == 0);
	CHECK(peak(1, 0, RATE) == 0);
	CHECK(plat_audio_is_playing(0) == 0);
	CHECK(plat_audio_is_playing(-1) == 0);
}

static void test_amplitude_and_pan(void)
{
	int ramp, h;

	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();

	// Full volume, left.
	h = plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	CHECK(h >= 0);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), FULL, FULL / 50));
	CHECK(peak(1, 0, RATE) == 0);

	// The same channel, right.
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_RIGHT);
	render(RATE);
	CHECK(peak(0, ramp, RATE) == 0);
	CHECK(near(peak(1, ramp, RATE), FULL, FULL / 50));

	// Center: constant power, 0.707 on each side.
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_CENTER);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), (int)(FULL * SQRT_HALF), FULL / 50));
	CHECK(near(peak(1, ramp, RATE), (int)(FULL * SQRT_HALF), FULL / 50));
	CHECK(near(peak(0, ramp, RATE), peak(1, ramp, RATE), 4));

	// Half volume, left.
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_LEFT);
	plat_audio_set_volume(h, PLAT_AUDIO_VOLUME_MAX / 2);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), FULL / 2, FULL / 50));
	CHECK(peak(1, ramp, RATE) == 0);

	// A volume above the maximum is clamped.
	plat_audio_set_volume(h, PLAT_AUDIO_VOLUME_MAX * 3);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), FULL, FULL / 50));

	// Master volume.
	plat_audio_set_master_volume(PLAT_AUDIO_VOLUME_MAX / 4);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), FULL / 4, FULL / 50));
	plat_audio_set_master_volume(PLAT_AUDIO_VOLUME_MAX);

	// Two channels add. Four channels at full volume clip and do not wrap.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX / 4, PLAT_AUDIO_PAN_LEFT, 1);
	plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX / 4, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(near(Out[(RATE - 1) * 2], FULL / 2, FULL / 100));
	plat_audio_mixer_init(RATE);
	for (h = 0; h < 4; h++)
		plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(Out[(RATE - 1) * 2] == 32767);
}

static void test_resample(void)
{
	int ramp;

	// 441 Hz for one second: 882 zero crossings.
	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();
	plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), 2 * TONE_HZ, 2));

	// The same data at twice the rate: 882 Hz.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Tone, TONE_LEN, TONE_RATE * 2, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), 4 * TONE_HZ, 2));

	// At the output rate: no resampling, 4 times the frequency.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Tone, TONE_LEN, RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), 8 * TONE_HZ, 2));

	// A mixer with another output rate.
	plat_audio_mixer_init(48000);
	ramp = plat_audio_mixer_ramp_frames();
	plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(48000 + ramp);
	CHECK(near(zero_crossings(0, ramp, 48000 + ramp), 2 * TONE_HZ, 2));

	// Linear interpolation: a 441 Hz sine moves by at most
	// amplitude * 2 * pi * 441 / 44100 between two output frames.
	// Without interpolation the step is 4 times that.
	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();
	plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(max_step(0, ramp, RATE) < (int)(FULL * 2 * 3.14159 * TONE_HZ / RATE * 1.3));
}

static void test_loop_and_end(void)
{
	int ramp, h, h2, end_frame;

	// Not looped: 2500 samples at 11025 Hz are 10000 output frames.
	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();
	end_frame = TONE_LEN * (RATE / TONE_RATE);
	h = plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 0);
	CHECK(plat_audio_is_playing(h) == 1);
	render(end_frame - 1);
	CHECK(plat_audio_is_playing(h) == 1);
	CHECK(near(peak(0, ramp, end_frame - ramp), FULL, FULL / 50));
	render(2);
	CHECK(plat_audio_is_playing(h) == 0);
	render(RATE);
	CHECK(peak(0, 0, RATE) == 0);

	// Looped: it still plays after two seconds, with the full amplitude
	// in each period and no step where the sample repeats.
	plat_audio_mixer_init(RATE);
	h = plat_audio_play(Tone, TONE_LEN, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE * 2);
	CHECK(plat_audio_is_playing(h) == 1);
	CHECK(near(peak(0, RATE * 2 - 200, RATE * 2), FULL, FULL / 50));
	CHECK(near(peak(0, end_frame - 100, end_frame + 100), FULL, FULL / 50));
	CHECK(max_step(0, ramp, RATE * 2) < (int)(FULL * 2 * 3.14159 * TONE_HZ / RATE * 1.3));
	CHECK(near(zero_crossings(0, ramp, RATE * 2), 4 * TONE_HZ, 3));

	// Stop: the report changes at once, the output is 0 after the ramp.
	plat_audio_stop(h);
	CHECK(plat_audio_is_playing(h) == 0);
	render(RATE);
	CHECK(peak(0, ramp + 1, RATE) == 0);

	// A handle of a channel that ended does not touch the next sample
	// on the same channel.
	h2 = plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	CHECK(h2 >= 0 && h2 != h);
	CHECK((h2 & 0xff) == (h & 0xff));
	plat_audio_stop(h);
	plat_audio_set_volume(h, 0);
	CHECK(plat_audio_is_playing(h) == 0);
	CHECK(plat_audio_is_playing(h2) == 1);
	render(RATE);
	CHECK(near(Out[(RATE - 1) * 2], FULL, 2));
}

static void test_ramps(void)
{
	int ramp, h, slope, i, monotonic;

	// A constant sample at full volume is the worst case for a click.
	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();
	slope = FULL / ramp + 2;
	CHECK(ramp >= RATE / 1000 && ramp <= RATE / 100);       // from 1 ms to 10 ms

	// Start.
	h = plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(abs(Out[0]) <= slope);
	CHECK(max_step(0, 0, RATE) <= slope);
	monotonic = 1;
	for (i = 1; i < ramp; i++)
		if (Out[i * 2] < Out[(i - 1) * 2])
			monotonic = 0;
	CHECK(monotonic);
	CHECK(Out[(ramp / 2) * 2] > FULL / 4 && Out[(ramp / 2) * 2] < FULL * 3 / 4);
	CHECK(near(Out[ramp * 2], FULL, 2));
	CHECK(near(Out[(RATE - 1) * 2], FULL, 2));

	// Change of volume and pan.
	plat_audio_set_volume(h, PLAT_AUDIO_VOLUME_MAX / 2);
	render(RATE);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(abs(Out[0] - FULL) <= slope);
	CHECK(near(Out[(RATE - 1) * 2], FULL / 2, 2));
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_RIGHT);
	render(RATE);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(max_step(1, 0, RATE) <= slope);
	CHECK(abs(Out[1]) <= slope);
	CHECK(Out[(RATE - 1) * 2] == 0);
	CHECK(near(Out[(RATE - 1) * 2 + 1], FULL / 2, 2));
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_LEFT);
	plat_audio_set_volume(h, PLAT_AUDIO_VOLUME_MAX);
	render(RATE);

	// Stop.
	plat_audio_stop(h);
	render(RATE);
	CHECK(abs(Out[0] - FULL) <= slope);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(Out[(ramp / 2) * 2] > FULL / 4 && Out[(ramp / 2) * 2] < FULL * 3 / 4);
	CHECK(peak(0, ramp + 1, RATE) == 0);

	// plat_audio_stop_all.
	h = plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	plat_audio_stop_all();
	CHECK(plat_audio_is_playing(h) == 0);
	render(RATE);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(peak(0, ramp + 1, RATE) == 0);

	// The end of a sample that is not looped: one second of 255, then 0.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 0);
	render(RATE * 2);
	CHECK(max_step(0, 0, RATE * 2) <= slope);
	CHECK(near(Out[(RATE - ramp - 2) * 2], FULL, 2));
	CHECK(abs(Out[(RATE - 1) * 2]) <= slope);
	CHECK(peak(0, RATE, RATE * 2) == 0);

	// A sample that is shorter than the two ramps does not click.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Dc, 20, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 0);
	render(RATE);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(peak(0, 0, 80) > 0);
	CHECK(peak(0, 81, RATE) == 0);
}

static void test_channels(void)
{
	int handles[PLAT_AUDIO_CHANNELS];
	int i, h, ramp;

	CHECK(PLAT_AUDIO_CHANNELS >= 16);

	// Before the mixer starts and after it closes, nothing plays.
	plat_audio_mixer_close();
	CHECK(plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_CENTER, 0) == -1);
	plat_audio_stop(0);
	plat_audio_stop_all();
	plat_audio_set_volume(0, 0);
	plat_audio_set_pan(0, 0);
	plat_audio_set_master_volume(0);
	plat_audio_music_stop();
	CHECK(plat_audio_is_playing(0) == 0);
	memset(Out, 0x55, sizeof(Out));
	render(100);
	CHECK(peak(0, 0, 100) == 0 && peak(1, 0, 100) == 0);

	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();

	// Arguments that are not valid.
	CHECK(plat_audio_play(NULL, 100, TONE_RATE, 100, 0, 0) == -1);
	CHECK(plat_audio_play(Dc, 0, TONE_RATE, 100, 0, 0) == -1);
	CHECK(plat_audio_play(Dc, 100, 0, 100, 0, 0) == -1);

	// All the channels play, and one more does not.
	for (i = 0; i < PLAT_AUDIO_CHANNELS; i++) {
		handles[i] = plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX / PLAT_AUDIO_CHANNELS,
			PLAT_AUDIO_PAN_LEFT, 1);
		CHECK(handles[i] >= 0);
	}
	CHECK(plat_audio_play(Dc, sizeof(Dc), TONE_RATE, 100, 0, 0) == -1);
	render(RATE);
	CHECK(near(Out[(RATE - 1) * 2], FULL, FULL / 100));
	for (i = 0; i < PLAT_AUDIO_CHANNELS; i++)
		CHECK(plat_audio_is_playing(handles[i]) == 1);

	// A stopped channel can be used again at once, before its fade ends.
	plat_audio_stop(handles[5]);
	h = plat_audio_play(Dc, sizeof(Dc), TONE_RATE, 100, 0, 0);
	CHECK(h >= 0);
	CHECK(plat_audio_is_playing(h) == 1);
	CHECK(plat_audio_is_playing(handles[5]) == 0);

	plat_audio_stop_all();
	render(ramp + 1);
	render(100);
	CHECK(peak(0, 0, 100) == 0);
}

// The music source of the test: a constant on each side, and a count.
static int Music_frames;
static int Music_calls;
static int Music_limit;

static int music_constant(void *user, int16_t *buffer, int frames)
{
	int i;

	CHECK(user == &Music_frames);
	Music_calls++;
	if (frames > Music_limit - Music_frames)
		frames = Music_limit - Music_frames;
	for (i = 0; i < frames; i++) {
		buffer[i * 2] = 8000;
		buffer[i * 2 + 1] = -4000;
	}
	Music_frames += frames;
	return frames;
}

// A 441 Hz sine at 22050 Hz on the left, silence on the right.
static int Music_phase;

static int music_sine(void *user, int16_t *buffer, int frames)
{
	int i;

	(void)user;
	for (i = 0; i < frames; i++) {
		buffer[i * 2] = (int16_t)lrint(10000.0 * sin(2 * 3.14159265358979 * TONE_HZ * Music_phase / 22050.0));
		buffer[i * 2 + 1] = 0;
		Music_phase++;
	}
	return frames;
}

static void test_music(void)
{
	int ramp, slope, calls;

	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();
	slope = 8000 / ramp + 2;

	// One second of output takes 22050 source frames.
	Music_frames = Music_calls = 0;
	Music_limit = 1 << 30;
	plat_audio_music_start(music_constant, &Music_frames, 22050);
	render(RATE);
	CHECK(Music_frames >= 22050 && Music_frames <= 22050 + 1024);
	CHECK(abs(Out[0]) <= slope);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(Out[(RATE - 1) * 2] == 8000);
	CHECK(Out[(RATE - 1) * 2 + 1] == -4000);

	// Music volume.
	plat_audio_music_set_volume(PLAT_AUDIO_VOLUME_MAX / 2);
	render(RATE);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(near(Out[(RATE - 1) * 2], 4000, 2));
	CHECK(near(Out[(RATE - 1) * 2 + 1], -2000, 2));
	plat_audio_music_set_volume(PLAT_AUDIO_VOLUME_MAX);
	render(RATE);

	// The music and a sample add.
	plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX / 4, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(near(Out[(RATE - 1) * 2], 8000 + FULL / 4, 4));
	CHECK(Out[(RATE - 1) * 2 + 1] == -4000);
	plat_audio_stop_all();
	render(RATE);

	// Stop: no more calls, a fade and then silence.
	calls = Music_calls;
	plat_audio_music_stop();
	render(RATE);
	CHECK(Music_calls == calls);
	CHECK(max_step(0, 0, RATE) <= slope);
	CHECK(abs(Out[0] - 8000) <= slope);
	CHECK(peak(0, ramp + 1, RATE) == 0);
	CHECK(peak(1, ramp + 1, RATE) == 0);

	// A source that has no more data gives silence and is asked again.
	Music_frames = Music_calls = 0;
	Music_limit = 11025;
	plat_audio_music_start(music_constant, &Music_frames, 22050);
	render(RATE);
	CHECK(Music_frames == 11025);
	CHECK(Out[(RATE / 4) * 2] == 8000);
	CHECK(Out[(RATE - 1) * 2] == 0);
	calls = Music_calls;
	render(RATE);
	CHECK(Music_calls > calls);
	plat_audio_music_stop();

	// The resampling of the music: 441 Hz at 22050 Hz is 441 Hz at 44100 Hz.
	plat_audio_mixer_init(RATE);
	Music_phase = 0;
	plat_audio_music_start(music_sine, NULL, 22050);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), 2 * TONE_HZ, 2));
	CHECK(near(peak(0, ramp, RATE + ramp), 10000, 100));
	CHECK(peak(1, 0, RATE + ramp) == 0);
	CHECK(max_step(0, ramp, RATE) < (int)(10000 * 2 * 3.14159 * TONE_HZ / RATE * 1.3));
	plat_audio_music_stop();
}

// The 16-bit samples: the same checks as for the 8-bit samples.
static void test_16bit(void)
{
	int ramp, h, h8, i, diff, d;
	static int16_t ref[RATE * 2];

	plat_audio_mixer_init(RATE);
	ramp = plat_audio_mixer_ramp_frames();

	// Amplitude: full volume on the left gives the sample value.
	h = plat_audio_play16(Tone16, TONE_LEN * 2, TONE_RATE * 2, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	CHECK(h >= 0);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), 32000, 32000 / 100));
	CHECK(peak(1, 0, RATE) == 0);

	// Pan: right, then center with constant power.
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_RIGHT);
	render(RATE);
	CHECK(peak(0, ramp, RATE) == 0);
	CHECK(near(peak(1, ramp, RATE), 32000, 32000 / 100));
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_CENTER);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), (int)(32000 * SQRT_HALF), 32000 / 100));
	CHECK(near(peak(0, ramp, RATE), peak(1, ramp, RATE), 4));

	// Volume.
	plat_audio_set_pan(h, PLAT_AUDIO_PAN_LEFT);
	plat_audio_set_volume(h, PLAT_AUDIO_VOLUME_MAX / 2);
	render(RATE);
	CHECK(near(peak(0, ramp, RATE), 16000, 32000 / 100));
	CHECK(plat_audio_is_playing(h) == 1);
	plat_audio_stop(h);
	render(RATE);
	CHECK(plat_audio_is_playing(h) == 0);
	CHECK(peak(0, ramp + 1, RATE) == 0);

	// The exact value and the sign: a constant of 32767 and of -32768.
	plat_audio_mixer_init(RATE);
	plat_audio_play16(Dc16, TONE_RATE, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(Out[(RATE - 1) * 2] == FULL16);
	plat_audio_mixer_init(RATE);
	for (i = 0; i < TONE_RATE; i++)
		Dc16[i] = -32768;
	plat_audio_play16(Dc16, TONE_RATE, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(Out[(RATE - 1) * 2] == -32768);
	for (i = 0; i < TONE_RATE; i++)
		Dc16[i] = 32767;

	// Resample: 441 Hz at 22050 Hz is 441 Hz in the output, and the same
	// data at 11025 Hz is 220.5 Hz.
	plat_audio_mixer_init(RATE);
	plat_audio_play16(Tone16, TONE_LEN * 2, TONE_RATE * 2, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), 2 * TONE_HZ, 2));
	CHECK(max_step(0, ramp, RATE) < (int)(32000 * 2 * 3.14159 * TONE_HZ / RATE * 1.3));
	plat_audio_mixer_init(RATE);
	plat_audio_play16(Tone16, TONE_LEN * 2, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE + ramp);
	CHECK(near(zero_crossings(0, ramp, RATE + ramp), TONE_HZ, 2));

	// The end of a sample that is not looped: no click, then silence, and
	// the channel is free.
	plat_audio_mixer_init(RATE);
	h = plat_audio_play16(Dc16, TONE_RATE, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 0);
	render(RATE * 2);
	CHECK(max_step(0, 0, RATE * 2) <= FULL16 / ramp + 2);
	CHECK(near(Out[(RATE - ramp - 2) * 2], FULL16, 2));
	CHECK(peak(0, RATE, RATE * 2) == 0);
	CHECK(plat_audio_is_playing(h) == 0);

	// A 16-bit sample and its 8-bit form (the high byte, unsigned) give
	// the same output within the rounding of the 8-bit form (1/256 of the
	// range, plus the interpolation of that error).
	plat_audio_mixer_init(RATE);
	{
		static uint8_t tone8[TONE_LEN * 2];
		for (i = 0; i < TONE_LEN * 2; i++)
			tone8[i] = (uint8_t)((Tone16[i] >> 8) + 128);
		h8 = plat_audio_play(tone8, TONE_LEN * 2, TONE_RATE * 2, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
		render(RATE);
		for (i = 0; i < RATE; i++)
			ref[i] = Out[i * 2];
		plat_audio_stop(h8);
	}
	plat_audio_mixer_init(RATE);
	plat_audio_play16(Tone16, TONE_LEN * 2, TONE_RATE * 2, PLAT_AUDIO_VOLUME_MAX, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	diff = 0;
	for (i = 0; i < RATE; i++) {
		d = abs(Out[i * 2] - ref[i]);
		if (d > diff)
			diff = d;
	}
	CHECK(diff <= 256);
	CHECK(diff > 0);        // the 16-bit path is not the 8-bit path

	// The two kinds of sample play together.
	plat_audio_mixer_init(RATE);
	plat_audio_play(Dc, sizeof(Dc), TONE_RATE, PLAT_AUDIO_VOLUME_MAX / 4, PLAT_AUDIO_PAN_LEFT, 1);
	plat_audio_play16(Dc16, TONE_RATE, TONE_RATE, PLAT_AUDIO_VOLUME_MAX / 4, PLAT_AUDIO_PAN_LEFT, 1);
	render(RATE);
	CHECK(near(Out[(RATE - 1) * 2], FULL / 4 + FULL16 / 4, 4));

	// Arguments that are not valid.
	CHECK(plat_audio_play16(NULL, 10, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, 0, 0) == -1);
	CHECK(plat_audio_play16(Dc16, 0, TONE_RATE, PLAT_AUDIO_VOLUME_MAX, 0, 0) == -1);
}

int main(void)
{
	int i;

	for (i = 0; i < TONE_LEN; i++)
		Tone[i] = (uint8_t)(128 + lrint(127.0 * sin(2 * 3.14159265358979 * TONE_HZ * i / TONE_RATE)));
	memset(Dc, 255, sizeof(Dc));
	for (i = 0; i < TONE_LEN * 2; i++)
		Tone16[i] = (int16_t)lrint(32000.0 * sin(2 * 3.14159265358979 * TONE_HZ * i / (TONE_RATE * 2)));
	for (i = 0; i < TONE_RATE; i++)
		Dc16[i] = 32767;

	test_silence();
	test_amplitude_and_pan();
	test_resample();
	test_loop_and_end();
	test_ramps();
	test_channels();
	test_music();
	test_16bit();
	plat_audio_mixer_close();

	if (Failures) {
		printf("test_audio: %d failures\n", Failures);
		return 1;
	}
	printf("test_audio: all tests passed\n");
	return 0;
}
