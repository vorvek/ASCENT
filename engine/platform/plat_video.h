/*
 * ASCENT platform layer: the parts of the window code (video.c) and of the
 * OpenGL code (gpu.c) that the two files share. The engine does not
 * include this header.
 */
#ifndef _PLAT_VIDEO_H
#define _PLAT_VIDEO_H

#include <stdint.h>
#include <SDL3/SDL.h>

#include "platform.h"

// Load the OpenGL functions and make the shaders. The context of the
// window must be current. Returns 0 if successful.
int gpu_init(SDL_Window *window);
void gpu_shutdown(void);

// Show one frame in the window (see plat_present).
void gpu_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6);

// The rectangle of the window (in window coordinates, not pixels) that
// shows the frame of the last gpu_present.
void gpu_frame_rect(float *x, float *y, float *w, float *h, int *frame_w, int *frame_h);

// gputex.c: the loader of the upscaled textures (worker threads).
typedef struct gputex_result {
	int texture;
	unsigned serial;
	int w, h;
	uint8_t *rgba;
	struct gputex_result *next;
} gputex_result;

int gputex_start(void);
void gputex_stop(void);
void gputex_queue(int texture, unsigned serial, const plat_gpu_hd_job *job);
// One finished result or NULL. Free it with gputex_free.
gputex_result *gputex_take(void);
int gputex_pending(void);
void gputex_free(gputex_result *r);
// Fill the color of the pixels with alpha 0 from the pixels next to them.
void gputex_bleed(uint8_t *rgba, int w, int h);
// Decode a PNG file to RGBA (any thread). Free the result with free.
uint8_t *gputex_load_png(const char *path, int *w, int *h);

#endif
