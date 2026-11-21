/*
 * ASCENT platform layer: the mixer core of audio.c without the device.
 * test_audio.c uses it to render the mixer output into a buffer.
 * The engine code does not include this file.
 */
#ifndef _PLAT_AUDIO_H
#define _PLAT_AUDIO_H

#include <stdint.h>

// The output rate that plat_audio_init uses.
#define PLAT_AUDIO_RATE 44100

// Start the mixer without a device. rate is the output rate in Hz.
// After this call the plat_audio functions of platform.h work.
void plat_audio_mixer_init(int rate);
void plat_audio_mixer_close(void);

// Render `frames` frames of signed 16-bit stereo output (left, right).
// The device callback calls this function. A test calls it directly.
void plat_audio_mixer_render(int16_t *out, int frames);

// The length in frames of the fade at the start and at the stop of a
// sample. A change of volume or pan uses the same slope.
int plat_audio_mixer_ramp_frames(void);

#endif
