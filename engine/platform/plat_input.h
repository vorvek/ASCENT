/*
 * ASCENT platform layer: private interface between video.c and input.c.
 * The engine code does not include this file.
 */
#ifndef _PLAT_INPUT_H
#define _PLAT_INPUT_H

#include <SDL3/SDL.h>

// plat_pump calls this function for each event that is not a quit event.
// The mouse position in the event is in frame pixels.
// The relative mouse motion in the event is in device units.
void plat_input_sdl_event(const SDL_Event *ev);
void plat_input_window_changed(void);

// plat_pump calls this function after the events. In the relative mode
// (the immersive mode of the game) it holds the system pointer hidden at
// the center of the window while the window has the focus.
void plat_input_hold_pointer(void);

#endif
