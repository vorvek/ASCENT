/*
 * ASCENT platform layer: audio device and software mixer.
 *
 * It replaces the HMI Sound Operating System driver that the DOS game
 * used for digital sound. The mixer plays unsigned 8-bit mono samples on
 * PLAT_AUDIO_CHANNELS channels and one stream of 16-bit stereo music. It
 * resamples each source to the output rate with linear interpolation.
 *
 * Threads: the game thread calls the plat_audio functions. SDL calls
 * device_callback on its audio thread. Mixer_lock protects all the mixer
 * state. The mixer calls the music callback with the lock held, so that
 * plat_audio_music_stop can promise that the callback does not run later.
 *
 * Clicks: a gain never jumps. It moves to its target with a fixed slope
 * (RAMP_MS for the full range). A new channel starts at gain 0, a stopped
 * channel plays on until its gain is 0, and a sample that reaches its end
 * fades out during its last RAMP_MS.
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <string.h>

#include "platform.h"
#include "plat_audio.h"

#define RAMP_MS         3
#define BLOCK_FRAMES    512     // frames that render mixes in one pass
#define MUSIC_CHUNK     512     // frames that one call of the music callback fills
#define FRAC_ONE        ((uint64_t)1 << 32)
#define HALF_PI         1.57079632679489661923f

enum { CH_FREE, CH_PLAYING, CH_STOPPING };
enum { MUSIC_OFF, MUSIC_ON, MUSIC_FADING };

typedef struct channel {
	int state;
	int generation;             // the upper part of the handle
	const uint8_t *data;
	uint32_t length;            // in samples
	int loop;
	uint64_t pos;               // position in the sample, 32.32 fixed point
	uint64_t step;              // sample rate / output rate, 32.32 fixed point
	int volume, pan;
	float gain_l, gain_r;       // the gain now
	float target_l, target_r;   // the gain that volume and pan ask for
} channel;

typedef struct music {
	int state;
	plat_audio_music_callback callback;
	void *user;
	uint64_t phase;             // position between prev and cur, 32.32 fixed point
	uint64_t step;
	float prev[2], cur[2];      // the two source frames that are interpolated
	float hold[2];              // the last output, for the fade after a stop
	float gain, target;
	int16_t buf[MUSIC_CHUNK * 2];
	int buf_pos, buf_len;       // in frames
} music;

static SDL_Mutex *Mixer_lock;
static int Mixer_ready;
static int Mixer_rate;
static int Ramp_frames;
static float Ramp_slope;        // gain change for each frame
static channel Channels[PLAT_AUDIO_CHANNELS];
static int Next_generation;
static music Music;
static float Master_gain, Master_target;

static SDL_AudioStream *Stream;

static float slew(float cur, float target, float slope)
{
	if (cur < target) {
		cur += slope;
		if (cur > target)
			cur = target;
	} else if (cur > target) {
		cur -= slope;
		if (cur < target)
			cur = target;
	}
	return cur;
}

static float volume_to_gain(int volume)
{
	if (volume < 0)
		volume = 0;
	if (volume > PLAT_AUDIO_VOLUME_MAX)
		volume = PLAT_AUDIO_VOLUME_MAX;
	return (float)volume / (float)PLAT_AUDIO_VOLUME_MAX;
}

static void channel_set_target(channel *c)
{
	float gain = volume_to_gain(c->volume);
	int pan = c->pan;
	float angle;

	if (pan < PLAT_AUDIO_PAN_LEFT)
		pan = PLAT_AUDIO_PAN_LEFT;
	if (pan > PLAT_AUDIO_PAN_RIGHT)
		pan = PLAT_AUDIO_PAN_RIGHT;
	angle = (float)pan / (float)PLAT_AUDIO_PAN_RIGHT * HALF_PI;
	c->target_l = gain * cosf(angle);
	c->target_r = gain * sinf(angle);
	// cosf(HALF_PI) is a very small number, not 0.
	if (pan == PLAT_AUDIO_PAN_RIGHT)
		c->target_l = 0;
}

// Returns the channel of a handle if that channel still plays.
static channel *handle_to_channel(int handle)
{
	channel *c;

	if (handle < 0 || (handle & 0xff) >= PLAT_AUDIO_CHANNELS)
		return NULL;
	c = &Channels[handle & 0xff];
	if (c->state != CH_PLAYING || c->generation != (handle >> 8))
		return NULL;
	return c;
}

// ---------------------------------------------------------------------
// The mixer core. It runs on the audio thread with Mixer_lock held.

static void mix_channel(channel *c, float *acc, int frames)
{
	const uint64_t end = (uint64_t)c->length << 32;
	const uint64_t fade_len = c->step * (uint64_t)Ramp_frames;
	const float fade_scale = 1.0f / (float)fade_len;
	int i;

	for (i = 0; i < frames; i++) {
		uint32_t idx = (uint32_t)(c->pos >> 32);
		float frac = (float)(uint32_t)c->pos * (1.0f / 4294967296.0f);
		int s0, s1;
		float v;

		c->gain_l = slew(c->gain_l, c->target_l, Ramp_slope);
		c->gain_r = slew(c->gain_r, c->target_r, Ramp_slope);
		if (c->state == CH_STOPPING && c->gain_l == 0 && c->gain_r == 0) {
			c->state = CH_FREE;
			return;
		}

		s0 = c->data[idx];
		if (idx + 1 < c->length)
			s1 = c->data[idx + 1];
		else
			s1 = c->loop ? c->data[0] : s0;
		v = ((float)(s0 - 128) + (float)(s1 - s0) * frac) * 256.0f;
		if (!c->loop && end - c->pos < fade_len)
			v *= (float)(end - c->pos) * fade_scale;

		acc[i * 2] += v * c->gain_l;
		acc[i * 2 + 1] += v * c->gain_r;

		c->pos += c->step;
		if (c->pos >= end) {
			if (!c->loop) {
				c->state = CH_FREE;
				return;
			}
			c->pos %= end;
		}
	}
}

// Read the next source frame of the music into cur.
static void music_next_frame(music *m)
{
	if (m->buf_pos >= m->buf_len) {
		int n = m->callback(m->user, m->buf, MUSIC_CHUNK);
		if (n < 0)
			n = 0;
		if (n < MUSIC_CHUNK)
			memset(m->buf + n * 2, 0, (size_t)(MUSIC_CHUNK - n) * 2 * sizeof(int16_t));
		m->buf_pos = 0;
		m->buf_len = MUSIC_CHUNK;
	}
	m->prev[0] = m->cur[0];
	m->prev[1] = m->cur[1];
	m->cur[0] = m->buf[m->buf_pos * 2];
	m->cur[1] = m->buf[m->buf_pos * 2 + 1];
	m->buf_pos++;
}

static void mix_music(music *m, float *acc, int frames)
{
	int i;

	for (i = 0; i < frames; i++) {
		if (m->state == MUSIC_ON) {
			float frac;
			m->gain = slew(m->gain, m->target, Ramp_slope);
			while (m->phase >= FRAC_ONE) {
				music_next_frame(m);
				m->phase -= FRAC_ONE;
			}
			frac = (float)(uint32_t)m->phase * (1.0f / 4294967296.0f);
			m->hold[0] = m->prev[0] + (m->cur[0] - m->prev[0]) * frac;
			m->hold[1] = m->prev[1] + (m->cur[1] - m->prev[1]) * frac;
			m->phase += m->step;
		} else {
			// After a stop: hold the last value and fade it out.
			m->gain = slew(m->gain, 0, Ramp_slope);
			if (m->gain == 0) {
				m->state = MUSIC_OFF;
				return;
			}
		}
		acc[i * 2] += m->hold[0] * m->gain;
		acc[i * 2 + 1] += m->hold[1] * m->gain;
	}
}

void plat_audio_mixer_render(int16_t *out, int frames)
{
	float acc[BLOCK_FRAMES * 2];
	int i, n;

	if (!Mixer_ready) {
		memset(out, 0, (size_t)frames * 2 * sizeof(int16_t));
		return;
	}

	SDL_LockMutex(Mixer_lock);
	while (frames > 0) {
		n = frames < BLOCK_FRAMES ? frames : BLOCK_FRAMES;
		memset(acc, 0, (size_t)n * 2 * sizeof(float));

		for (i = 0; i < PLAT_AUDIO_CHANNELS; i++)
			if (Channels[i].state != CH_FREE)
				mix_channel(&Channels[i], acc, n);
		if (Music.state != MUSIC_OFF)
			mix_music(&Music, acc, n);

		for (i = 0; i < n * 2; i++) {
			float v;
			if (!(i & 1))
				Master_gain = slew(Master_gain, Master_target, Ramp_slope);
			v = acc[i] * Master_gain;
			if (v > 32767.0f)
				v = 32767.0f;
			if (v < -32768.0f)
				v = -32768.0f;
			out[i] = (int16_t)lrintf(v);
		}
		out += n * 2;
		frames -= n;
	}
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_mixer_init(int rate)
{
	if (!Mixer_lock)
		Mixer_lock = SDL_CreateMutex();
	SDL_LockMutex(Mixer_lock);
	memset(Channels, 0, sizeof(Channels));
	memset(&Music, 0, sizeof(Music));
	Music.target = 1.0f;
	Mixer_rate = rate;
	Ramp_frames = rate * RAMP_MS / 1000;
	if (Ramp_frames < 1)
		Ramp_frames = 1;
	Ramp_slope = 1.0f / (float)Ramp_frames;
	Master_gain = Master_target = 1.0f;
	Mixer_ready = 1;
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_mixer_close(void)
{
	if (!Mixer_lock)
		return;
	SDL_LockMutex(Mixer_lock);
	Mixer_ready = 0;
	memset(Channels, 0, sizeof(Channels));
	memset(&Music, 0, sizeof(Music));
	SDL_UnlockMutex(Mixer_lock);
	SDL_DestroyMutex(Mixer_lock);
	Mixer_lock = NULL;
}

int plat_audio_mixer_ramp_frames(void)
{
	return Ramp_frames;
}

// ---------------------------------------------------------------------
// The device.

static void SDLCALL device_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, int total_amount)
{
	int16_t buf[BLOCK_FRAMES * 2];
	int frames = (additional_amount + 3) / 4;       // 4 bytes for each frame

	(void)userdata;
	(void)total_amount;
	while (frames > 0) {
		int n = frames < BLOCK_FRAMES ? frames : BLOCK_FRAMES;
		plat_audio_mixer_render(buf, n);
		SDL_PutAudioStreamData(stream, buf, n * 4);
		frames -= n;
	}
}

int plat_audio_init(void)
{
	SDL_AudioSpec spec;

	if (Stream)
		return 0;
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
		SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "plat_audio_init: %s", SDL_GetError());
		return -1;
	}
	plat_audio_mixer_init(PLAT_AUDIO_RATE);

	// A small device buffer keeps the delay between the game and the sound short.
	SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, "512");
	spec.format = SDL_AUDIO_S16;
	spec.channels = 2;
	spec.freq = PLAT_AUDIO_RATE;
	Stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, device_callback, NULL);
	if (!Stream) {
		SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "plat_audio_init: %s", SDL_GetError());
		plat_audio_mixer_close();
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		return -1;
	}
	SDL_ResumeAudioStreamDevice(Stream);
	return 0;
}

void plat_audio_close(void)
{
	if (Stream) {
		// plat_shutdown calls SDL_Quit, and SDL_Quit destroys the stream.
		// The game can call this function after that, from atexit.
		if (SDL_WasInit(SDL_INIT_AUDIO)) {
			SDL_DestroyAudioStream(Stream);
			SDL_QuitSubSystem(SDL_INIT_AUDIO);
		}
		Stream = NULL;
	}
	plat_audio_mixer_close();
}

// ---------------------------------------------------------------------
// The interface for the game thread.

int plat_audio_play(const uint8_t *data, int length, int rate, int volume, int pan, int loop)
{
	channel *c = NULL;
	int i, handle = -1;

	if (!Mixer_ready || !data || length <= 0 || rate <= 0)
		return -1;

	SDL_LockMutex(Mixer_lock);
	for (i = 0; i < PLAT_AUDIO_CHANNELS; i++)
		if (Channels[i].state == CH_FREE) {
			c = &Channels[i];
			break;
		}
	if (!c) {
		// A channel that fades out after a stop is the next choice.
		for (i = 0; i < PLAT_AUDIO_CHANNELS; i++)
			if (Channels[i].state == CH_STOPPING) {
				c = &Channels[i];
				break;
			}
	}
	if (c) {
		Next_generation = (Next_generation + 1) & 0x3fffff;
		c->state = CH_PLAYING;
		c->generation = Next_generation;
		c->data = data;
		c->length = (uint32_t)length;
		c->loop = loop != 0;
		c->pos = 0;
		c->step = ((uint64_t)rate << 32) / (uint64_t)Mixer_rate;
		if (c->step == 0)
			c->step = 1;
		c->volume = volume;
		c->pan = pan;
		c->gain_l = c->gain_r = 0;
		channel_set_target(c);
		handle = (c->generation << 8) | i;
	}
	SDL_UnlockMutex(Mixer_lock);
	return handle;
}

void plat_audio_stop(int handle)
{
	channel *c;

	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	c = handle_to_channel(handle);
	if (c) {
		c->state = CH_STOPPING;
		c->target_l = c->target_r = 0;
	}
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_stop_all(void)
{
	int i;

	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	for (i = 0; i < PLAT_AUDIO_CHANNELS; i++)
		if (Channels[i].state == CH_PLAYING) {
			Channels[i].state = CH_STOPPING;
			Channels[i].target_l = Channels[i].target_r = 0;
		}
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_set_volume(int handle, int volume)
{
	channel *c;

	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	c = handle_to_channel(handle);
	if (c) {
		c->volume = volume;
		channel_set_target(c);
	}
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_set_pan(int handle, int pan)
{
	channel *c;

	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	c = handle_to_channel(handle);
	if (c) {
		c->pan = pan;
		channel_set_target(c);
	}
	SDL_UnlockMutex(Mixer_lock);
}

int plat_audio_is_playing(int handle)
{
	int playing;

	if (!Mixer_ready)
		return 0;
	SDL_LockMutex(Mixer_lock);
	playing = handle_to_channel(handle) != NULL;
	SDL_UnlockMutex(Mixer_lock);
	return playing;
}

void plat_audio_set_master_volume(int volume)
{
	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	Master_target = volume_to_gain(volume);
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_music_start(plat_audio_music_callback callback, void *user, int rate)
{
	if (!Mixer_ready)
		return;
	if (!callback || rate <= 0) {
		plat_audio_music_stop();
		return;
	}
	SDL_LockMutex(Mixer_lock);
	Music.state = MUSIC_ON;
	Music.callback = callback;
	Music.user = user;
	Music.step = ((uint64_t)rate << 32) / (uint64_t)Mixer_rate;
	if (Music.step == 0)
		Music.step = 1;
	// The first output frame reads the first source frame, and the new
	// source fades in from gain 0. A source that still plays is cut with
	// no fade: call plat_audio_music_stop some milliseconds before this
	// function if that matters.
	Music.phase = FRAC_ONE * 2;
	Music.prev[0] = Music.prev[1] = 0;
	Music.cur[0] = Music.cur[1] = 0;
	Music.buf_pos = Music.buf_len = 0;
	Music.gain = 0;
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_music_stop(void)
{
	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	if (Music.state == MUSIC_ON)
		Music.state = MUSIC_FADING;
	Music.callback = NULL;
	Music.user = NULL;
	SDL_UnlockMutex(Mixer_lock);
}

void plat_audio_music_set_volume(int volume)
{
	if (!Mixer_ready)
		return;
	SDL_LockMutex(Mixer_lock);
	Music.target = volume_to_gain(volume);
	SDL_UnlockMutex(Mixer_lock);
}
