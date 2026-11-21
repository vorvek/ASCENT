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

// ---------------------------------------------------------------------
// Input (input.c). plat_pump translates the window events and calls the
// plat_input_* functions. Each plat_input_* function calls the handler
// that the engine registered, if there is one. A test can call the
// plat_input_* functions directly, without a window.

// Keyboard. scancode is a PC scan code (set 1) from 1 to 255. A key with
// the E0 prefix has bit 7 set (right ctrl is 0x9D), as in the KEY_ constants
// of key.h. Pause is 0x61. down is 1 for a press and 0 for a release.
// The operating system repeats the press event while the key is held.
// The handler gets each repeat as one more press with no release between.
typedef void (*plat_key_handler)(int scancode, int down);
void plat_set_key_handler(plat_key_handler handler);
void plat_input_key(int scancode, int down);

// Mouse. button is 0 for left, 1 for right, 2 for middle.
// dx and dy are the relative motion in device units (not scaled to the frame).
// x and y are the position in frame pixels.
typedef void (*plat_mouse_button_handler)(int button, int down);
typedef void (*plat_mouse_motion_handler)(int dx, int dy, int x, int y);
void plat_set_mouse_handlers(plat_mouse_button_handler button, plat_mouse_motion_handler motion);
void plat_input_mouse_button(int button, int down);
void plat_input_mouse_motion(int dx, int dy, int x, int y);

// Relative mouse mode: the pointer is hidden and held in the window, and
// the motion has no limit at the window border. If there is no window yet,
// the mode starts when plat_pump first sees the window.
void plat_mouse_set_relative(int on);
int plat_mouse_get_relative(void);

// Joystick. The first joystick or gamepad that SDL finds is used.
// A device that is connected later is opened by plat_pump.
#define PLAT_JOY_AXES    4
#define PLAT_HAT_UP      1
#define PLAT_HAT_RIGHT   2
#define PLAT_HAT_DOWN    4
#define PLAT_HAT_LEFT    8

// Start the joystick support. Returns 1 if a device is open.
int plat_joy_init(void);
void plat_joy_close(void);
// Returns 1 if a device is open.
int plat_joy_present(void);
// Read the first 4 axes. Each value is from -32768 to 32767.
// Returns a mask with bit n set if axis n exists.
int plat_joy_read_axes(int axes[PLAT_JOY_AXES]);
// The current state of the first 32 buttons, bit n for button n.
uint32_t plat_joy_read_buttons(void);
// The current state of the first hat (PLAT_HAT_ bits).
int plat_joy_read_hat(void);
// Button and hat changes. button is from 0 to 31.
typedef void (*plat_joy_button_handler)(int button, int down);
typedef void (*plat_joy_hat_handler)(int hat);
void plat_set_joy_handlers(plat_joy_button_handler button, plat_joy_hat_handler hat);
void plat_input_joy_button(int button, int down);
void plat_input_joy_hat(int hat);

#endif
