/*
 * ASCENT platform layer.
 *
 * This is the only part of the engine that calls SDL. The engine code
 * does not include SDL headers. It includes only this header.
 */
#ifndef _PLATFORM_H
#define _PLATFORM_H

#include <stdint.h>

// Open the window. w and h are the size of the game frame in pixels.
// scale is the initial window size as a multiple of the frame size.
// Returns 0 if successful.
int plat_init(const char *title, int w, int h, int scale);

void plat_shutdown(void);

// Change the size of the game frame. Returns 0 if successful.
int plat_set_frame_size(int w, int h);

// Show one frame of 8-bit pixels.
// pal6 is 256 RGB entries with 6 bits for each component (VGA format).
void plat_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6);

// Process the window events. Returns 0 after the user closes the window.
int plat_pump(void);

// Time since plat_init in microseconds.
uint64_t plat_time_us(void);

void plat_sleep_ms(int ms);

#endif
