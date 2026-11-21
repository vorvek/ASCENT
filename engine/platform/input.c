/*
 * ASCENT platform layer: keyboard, mouse and joystick input.
 *
 * This file replaces the hardware access of the DOS input code in
 * engine/bios: the keyboard interrupt (INT 9), the mouse driver (INT 33h)
 * and the game port (port 201h). It translates SDL events to the terms of
 * the original code and passes them to the handlers that engine/bios
 * registers.
 */
#include <SDL3/SDL.h>

#include "platform.h"
#include "plat_input.h"

static plat_key_handler Key_handler;
static plat_mouse_button_handler Mouse_button_handler;
static plat_mouse_motion_handler Mouse_motion_handler;
static plat_joy_button_handler Joy_button_handler;
static plat_joy_hat_handler Joy_hat_handler;

static int Relative_wanted, Relative_applied;
static float Rel_frac_x, Rel_frac_y;
static int Mouse_down[3];

static int Joy_started;
static SDL_Joystick *Joy;
static SDL_JoystickID Joy_id;

// ---------------------------------------------------------------------
// Keyboard

// SDL scan code to PC scan code (set 1). An E0 key has bit 7 set.
static unsigned char Scancode_table[SDL_SCANCODE_COUNT];
static int Scancode_table_ready;

static void init_scancode_table(void)
{
	static const struct { int sdl; int dos; } map[] = {
		{ SDL_SCANCODE_ESCAPE, 0x01 },
		{ SDL_SCANCODE_1, 0x02 }, { SDL_SCANCODE_2, 0x03 }, { SDL_SCANCODE_3, 0x04 },
		{ SDL_SCANCODE_4, 0x05 }, { SDL_SCANCODE_5, 0x06 }, { SDL_SCANCODE_6, 0x07 },
		{ SDL_SCANCODE_7, 0x08 }, { SDL_SCANCODE_8, 0x09 }, { SDL_SCANCODE_9, 0x0A },
		{ SDL_SCANCODE_0, 0x0B },
		{ SDL_SCANCODE_MINUS, 0x0C }, { SDL_SCANCODE_EQUALS, 0x0D },
		{ SDL_SCANCODE_BACKSPACE, 0x0E }, { SDL_SCANCODE_TAB, 0x0F },
		{ SDL_SCANCODE_Q, 0x10 }, { SDL_SCANCODE_W, 0x11 }, { SDL_SCANCODE_E, 0x12 },
		{ SDL_SCANCODE_R, 0x13 }, { SDL_SCANCODE_T, 0x14 }, { SDL_SCANCODE_Y, 0x15 },
		{ SDL_SCANCODE_U, 0x16 }, { SDL_SCANCODE_I, 0x17 }, { SDL_SCANCODE_O, 0x18 },
		{ SDL_SCANCODE_P, 0x19 },
		{ SDL_SCANCODE_LEFTBRACKET, 0x1A }, { SDL_SCANCODE_RIGHTBRACKET, 0x1B },
		{ SDL_SCANCODE_RETURN, 0x1C }, { SDL_SCANCODE_LCTRL, 0x1D },
		{ SDL_SCANCODE_A, 0x1E }, { SDL_SCANCODE_S, 0x1F }, { SDL_SCANCODE_D, 0x20 },
		{ SDL_SCANCODE_F, 0x21 }, { SDL_SCANCODE_G, 0x22 }, { SDL_SCANCODE_H, 0x23 },
		{ SDL_SCANCODE_J, 0x24 }, { SDL_SCANCODE_K, 0x25 }, { SDL_SCANCODE_L, 0x26 },
		{ SDL_SCANCODE_SEMICOLON, 0x27 }, { SDL_SCANCODE_APOSTROPHE, 0x28 },
		{ SDL_SCANCODE_GRAVE, 0x29 }, { SDL_SCANCODE_LSHIFT, 0x2A },
		{ SDL_SCANCODE_BACKSLASH, 0x2B }, { SDL_SCANCODE_NONUSHASH, 0x2B },
		{ SDL_SCANCODE_Z, 0x2C }, { SDL_SCANCODE_X, 0x2D }, { SDL_SCANCODE_C, 0x2E },
		{ SDL_SCANCODE_V, 0x2F }, { SDL_SCANCODE_B, 0x30 }, { SDL_SCANCODE_N, 0x31 },
		{ SDL_SCANCODE_M, 0x32 },
		{ SDL_SCANCODE_COMMA, 0x33 }, { SDL_SCANCODE_PERIOD, 0x34 }, { SDL_SCANCODE_SLASH, 0x35 },
		{ SDL_SCANCODE_RSHIFT, 0x36 }, { SDL_SCANCODE_KP_MULTIPLY, 0x37 },
		{ SDL_SCANCODE_LALT, 0x38 }, { SDL_SCANCODE_SPACE, 0x39 },
		{ SDL_SCANCODE_CAPSLOCK, 0x3A },
		{ SDL_SCANCODE_F1, 0x3B }, { SDL_SCANCODE_F2, 0x3C }, { SDL_SCANCODE_F3, 0x3D },
		{ SDL_SCANCODE_F4, 0x3E }, { SDL_SCANCODE_F5, 0x3F }, { SDL_SCANCODE_F6, 0x40 },
		{ SDL_SCANCODE_F7, 0x41 }, { SDL_SCANCODE_F8, 0x42 }, { SDL_SCANCODE_F9, 0x43 },
		{ SDL_SCANCODE_F10, 0x44 },
		{ SDL_SCANCODE_NUMLOCKCLEAR, 0x45 }, { SDL_SCANCODE_SCROLLLOCK, 0x46 },
		{ SDL_SCANCODE_KP_7, 0x47 }, { SDL_SCANCODE_KP_8, 0x48 }, { SDL_SCANCODE_KP_9, 0x49 },
		{ SDL_SCANCODE_KP_MINUS, 0x4A },
		{ SDL_SCANCODE_KP_4, 0x4B }, { SDL_SCANCODE_KP_5, 0x4C }, { SDL_SCANCODE_KP_6, 0x4D },
		{ SDL_SCANCODE_KP_PLUS, 0x4E },
		{ SDL_SCANCODE_KP_1, 0x4F }, { SDL_SCANCODE_KP_2, 0x50 }, { SDL_SCANCODE_KP_3, 0x51 },
		{ SDL_SCANCODE_KP_0, 0x52 }, { SDL_SCANCODE_KP_PERIOD, 0x53 },
		{ SDL_SCANCODE_NONUSBACKSLASH, 0x56 },
		{ SDL_SCANCODE_F11, 0x57 }, { SDL_SCANCODE_F12, 0x58 },
		{ SDL_SCANCODE_PAUSE, 0x61 },			// KEY_PAUSE in key.h
		{ SDL_SCANCODE_KP_ENTER, 0x9C }, { SDL_SCANCODE_RCTRL, 0x9D },
		{ SDL_SCANCODE_KP_DIVIDE, 0xB5 }, { SDL_SCANCODE_PRINTSCREEN, 0xB7 },
		{ SDL_SCANCODE_RALT, 0xB8 },
		{ SDL_SCANCODE_HOME, 0xC7 }, { SDL_SCANCODE_UP, 0xC8 }, { SDL_SCANCODE_PAGEUP, 0xC9 },
		{ SDL_SCANCODE_LEFT, 0xCB }, { SDL_SCANCODE_RIGHT, 0xCD },
		{ SDL_SCANCODE_END, 0xCF }, { SDL_SCANCODE_DOWN, 0xD0 }, { SDL_SCANCODE_PAGEDOWN, 0xD1 },
		{ SDL_SCANCODE_INSERT, 0xD2 }, { SDL_SCANCODE_DELETE, 0xD3 },
	};
	size_t i;

	for (i = 0; i < sizeof(map) / sizeof(map[0]); i++)
		Scancode_table[map[i].sdl] = (unsigned char) map[i].dos;
	Scancode_table_ready = 1;
}

void plat_set_key_handler(plat_key_handler handler)
{
	Key_handler = handler;
}

void plat_input_key(int scancode, int down)
{
	if (Key_handler && scancode > 0 && scancode < 256)
		Key_handler(scancode, down != 0);
}

// ---------------------------------------------------------------------
// Mouse

void plat_set_mouse_handlers(plat_mouse_button_handler button, plat_mouse_motion_handler motion)
{
	Mouse_button_handler = button;
	Mouse_motion_handler = motion;
}

void plat_input_mouse_button(int button, int down)
{
	if (Mouse_button_handler)
		Mouse_button_handler(button, down != 0);
}

void plat_input_mouse_motion(int dx, int dy, int x, int y)
{
	if (Mouse_motion_handler)
		Mouse_motion_handler(dx, dy, x, y);
}

// The wheel steps since the last plat_mouse_wheel call (the menus read it).
static int Wheel_steps;
static float Wheel_frac;

void plat_input_mouse_wheel(int steps)
{
	Wheel_steps += steps;
}

int plat_mouse_wheel(void)
{
	int n = Wheel_steps;

	Wheel_steps = 0;
	return n;
}

// Set the relative mode of the window, if there is a window.
// ASCENT: the system pointer is hidden at all times (video.c hides it),
// because the game draws its own cursor. With the relative mode off, the
// game draws its cursor at the pointer position. In full screen the
// pointer is held in the window (mouse grab) also then.
static int Grab_applied = -1;
static int Grab_dirty = 1;

static void apply_relative_mode(void)
{
	SDL_Window **windows;
	int count = 0;

	if (Relative_wanted == Relative_applied && !Grab_dirty)
		return;
	if (!SDL_WasInit(SDL_INIT_VIDEO))
		return;
	windows = SDL_GetWindows(&count);
	if (windows && count > 0) {
		int grab = plat_is_fullscreen();		// ASCENT: the borderless full screen form of video.c
		if (Relative_wanted != Relative_applied
				&& SDL_SetWindowRelativeMouseMode(windows[0], Relative_wanted != 0))
			Relative_applied = Relative_wanted;
		if (grab != Grab_applied && SDL_SetWindowMouseGrab(windows[0], grab != 0))
			Grab_applied = grab;
		Grab_dirty = 0;
	}
	SDL_free(windows);
}

void plat_mouse_set_relative(int on)
{
	Relative_wanted = (on != 0);
	Rel_frac_x = Rel_frac_y = 0;
	apply_relative_mode();
}

int plat_mouse_get_relative(void)
{
	return Relative_wanted;
}

// ---------------------------------------------------------------------
// Joystick

void plat_set_joy_handlers(plat_joy_button_handler button, plat_joy_hat_handler hat)
{
	Joy_button_handler = button;
	Joy_hat_handler = hat;
}

void plat_input_joy_button(int button, int down)
{
	if (Joy_button_handler && button >= 0 && button < 32)
		Joy_button_handler(button, down != 0);
}

void plat_input_joy_hat(int hat)
{
	if (Joy_hat_handler)
		Joy_hat_handler(hat & 15);
}

static void joy_open_first(void)
{
	SDL_JoystickID *ids;
	int count = 0, i;

	if (Joy || !Joy_started)
		return;
	ids = SDL_GetJoysticks(&count);
	for (i = 0; ids && i < count && !Joy; i++) {
		Joy = SDL_OpenJoystick(ids[i]);
		if (Joy)
			Joy_id = ids[i];
	}
	SDL_free(ids);
}

static void joy_release_all(void)
{
	// The device is gone. Release what it held down.
	int i;
	for (i = 0; i < 32; i++)
		plat_input_joy_button(i, 0);
	plat_input_joy_hat(0);
}

int plat_joy_init(void)
{
	if (!Joy_started) {
		if (!SDL_InitSubSystem(SDL_INIT_JOYSTICK))
			return 0;
		Joy_started = 1;
	}
	joy_open_first();
	return Joy != NULL;
}

void plat_joy_close(void)
{
	if (Joy) {
		SDL_CloseJoystick(Joy);
		Joy = NULL;
	}
	if (Joy_started) {
		SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
		Joy_started = 0;
	}
}

int plat_joy_present(void)
{
	return Joy != NULL;
}

int plat_joy_read_axes(int axes[PLAT_JOY_AXES])
{
	int i, n, mask = 0;

	for (i = 0; i < PLAT_JOY_AXES; i++)
		axes[i] = 0;
	if (!Joy)
		return 0;
	SDL_UpdateJoysticks();
	n = SDL_GetNumJoystickAxes(Joy);
	for (i = 0; i < PLAT_JOY_AXES && i < n; i++) {
		axes[i] = SDL_GetJoystickAxis(Joy, i);
		mask |= 1 << i;
	}
	return mask;
}

uint32_t plat_joy_read_buttons(void)
{
	uint32_t bits = 0;
	int i, n;

	if (!Joy)
		return 0;
	SDL_UpdateJoysticks();
	n = SDL_GetNumJoystickButtons(Joy);
	for (i = 0; i < 32 && i < n; i++)
		if (SDL_GetJoystickButton(Joy, i))
			bits |= (uint32_t) 1 << i;
	return bits;
}

int plat_joy_read_hat(void)
{
	if (!Joy || SDL_GetNumJoystickHats(Joy) < 1)
		return 0;
	SDL_UpdateJoysticks();
	return SDL_GetJoystickHat(Joy, 0) & 15;		// the SDL_HAT_ bits are the PLAT_HAT_ bits
}

// ---------------------------------------------------------------------
// ASCENT: gamepad (SDL_Gamepad)

static SDL_Gamepad *Pad;
static SDL_JoystickID Pad_id;
static int Pad_started, Pad_test;
static unsigned char Pad_buttons[PLAT_PAD_BUTTONS];
static unsigned char Pad_presses[PLAT_PAD_BUTTONS];
static int Pad_axes[PLAT_PAD_AXES];
static Uint64 Pad_game_ticks;			// the time of the last game frame
static int Pad_stick_dir[2];			// the arrow of the left stick in the menus

static int pad_menu_mode(void)
{
	Uint64 now = SDL_GetTicks();

	return Pad_game_ticks == 0 || (now >= Pad_game_ticks && now - Pad_game_ticks > 100);
}

void plat_pad_game_frame(void)
{
	Pad_game_ticks = SDL_GetTicks();
	if (!Pad_game_ticks)
		Pad_game_ticks = 1;
}

static void pad_key(int scancode, int down)
{
	plat_input_key(scancode, down);
}

void plat_input_pad_button(int button, int down)
{
	static const struct { int button, key; } menu_keys[] = {
		{ PLAT_PAD_DPAD_UP, 0xC8 }, { PLAT_PAD_DPAD_DOWN, 0xD0 },
		{ PLAT_PAD_DPAD_LEFT, 0xCB }, { PLAT_PAD_DPAD_RIGHT, 0xCD },
		{ PLAT_PAD_A, 0x1C }, { PLAT_PAD_B, 0x01 } };
	int i;

	if (button < 0 || button >= PLAT_PAD_BUTTONS)
		return;
	down = down != 0;
	if (!Pad)
		Pad_test = 1;
	if (down && !Pad_buttons[button] && Pad_presses[button] < 255)
		Pad_presses[button]++;
	Pad_buttons[button] = (unsigned char) down;
	// Start is Escape everywhere; the others only in the menus. A press
	// gives the key down and up, so that a held button does not repeat.
	if (button == PLAT_PAD_START && down) {
		pad_key(0x01, 1);
		pad_key(0x01, 0);
	}
	if (down && pad_menu_mode())
		for (i = 0; i < (int) (sizeof(menu_keys) / sizeof(menu_keys[0])); i++)
			if (menu_keys[i].button == button) {
				pad_key(menu_keys[i].key, 1);
				pad_key(menu_keys[i].key, 0);
			}
}

void plat_input_pad_axis(int axis, int value)
{
	if (axis < 0 || axis >= PLAT_PAD_AXES)
		return;
	if (!Pad)
		Pad_test = 1;
	if (value < -32768) value = -32768;
	if (value > 32767) value = 32767;
	Pad_axes[axis] = value;
	// The left stick in the menus: an arrow key when it passes one half
	if (axis == PLAT_PAD_LEFTX || axis == PLAT_PAD_LEFTY) {
		int a = axis - PLAT_PAD_LEFTX;
		int dir = value > 16384 ? 1 : value < -16384 ? -1 : 0;
		if (dir != Pad_stick_dir[a]) {
			Pad_stick_dir[a] = dir;
			if (dir && pad_menu_mode()) {
				int key = a == 0 ? (dir > 0 ? 0xCD : 0xCB) : (dir > 0 ? 0xD0 : 0xC8);
				pad_key(key, 1);
				pad_key(key, 0);
			}
		}
	}
}

static void pad_release_all(void)
{
	int i;
	for (i = 0; i < PLAT_PAD_BUTTONS; i++)
		if (Pad_buttons[i])
			plat_input_pad_button(i, 0);
	for (i = 0; i < PLAT_PAD_AXES; i++)
		plat_input_pad_axis(i, 0);
}

static void pad_open_first(void)
{
	SDL_JoystickID *ids;
	int count = 0, i;

	if (Pad || !Pad_started)
		return;
	ids = SDL_GetGamepads(&count);
	for (i = 0; ids && i < count && !Pad; i++) {
		Pad = SDL_OpenGamepad(ids[i]);
		if (Pad)
			Pad_id = ids[i];
	}
	SDL_free(ids);
}

int plat_pad_init(void)
{
	if (!Pad_started) {
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
			return 0;
		Pad_started = 1;
	}
	pad_open_first();
	return Pad != NULL;
}

int plat_pad_present(void)
{
	return Pad != NULL || Pad_test;
}

int plat_pad_button(int button)
{
	return button >= 0 && button < PLAT_PAD_BUTTONS ? Pad_buttons[button] : 0;
}

int plat_pad_axis(int axis)
{
	return axis >= 0 && axis < PLAT_PAD_AXES ? Pad_axes[axis] : 0;
}

int plat_pad_button_presses(int button)
{
	int n;

	if (button < 0 || button >= PLAT_PAD_BUTTONS)
		return 0;
	n = Pad_presses[button];
	Pad_presses[button] = 0;
	return n;
}

// ---------------------------------------------------------------------
// Events from plat_pump

void plat_input_sdl_event(const SDL_Event *ev)
{
	apply_relative_mode();

	switch (ev->type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		if (!Scancode_table_ready)
			init_scancode_table();
		if (ev->key.scancode > 0 && ev->key.scancode < SDL_SCANCODE_COUNT)
			plat_input_key(Scancode_table[ev->key.scancode], ev->type == SDL_EVENT_KEY_DOWN);
		break;

	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP: {
		int button;
		if (ev->button.button == SDL_BUTTON_LEFT)
			button = 0;
		else if (ev->button.button == SDL_BUTTON_RIGHT)
			button = 1;
		else if (ev->button.button == SDL_BUTTON_MIDDLE)
			button = 2;
		else
			break;
		Mouse_down[button] = (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
		plat_input_mouse_button(button, Mouse_down[button]);
		break;
	}

	case SDL_EVENT_MOUSE_MOTION: {
		// Keep the fraction of the relative motion for the next event.
		int dx, dy;
		Rel_frac_x += ev->motion.xrel;
		Rel_frac_y += ev->motion.yrel;
		dx = (int) Rel_frac_x;
		dy = (int) Rel_frac_y;
		Rel_frac_x -= (float) dx;
		Rel_frac_y -= (float) dy;
		plat_input_mouse_motion(dx, dy, (int) ev->motion.x, (int) ev->motion.y);
		break;
	}

	case SDL_EVENT_MOUSE_WHEEL: {
		// One step for each notch; a positive step is away from the user.
		int n;
		float y = ev->wheel.y;
		if (ev->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
			y = -y;
		Wheel_frac += y;
		n = (int) Wheel_frac;
		Wheel_frac -= (float) n;
		if (n)
			plat_input_mouse_wheel(n);
		break;
	}

	case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
	case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
		Grab_dirty = 1;
		apply_relative_mode();
		break;

	case SDL_EVENT_WINDOW_FOCUS_LOST: {
		// SDL releases the keys when the window loses the focus (it sends
		// the key up events). Do the same for the mouse buttons.
		int i;
		for (i = 0; i < 3; i++)
			if (Mouse_down[i]) {
				Mouse_down[i] = 0;
				plat_input_mouse_button(i, 0);
			}
		break;
	}

	case SDL_EVENT_JOYSTICK_ADDED:
		joy_open_first();
		break;

	case SDL_EVENT_JOYSTICK_REMOVED:
		if (Joy && ev->jdevice.which == Joy_id) {
			SDL_CloseJoystick(Joy);
			Joy = NULL;
			joy_release_all();
			joy_open_first();
		}
		break;

	case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
	case SDL_EVENT_JOYSTICK_BUTTON_UP:
		if (Joy && ev->jbutton.which == Joy_id)
			plat_input_joy_button(ev->jbutton.button, ev->type == SDL_EVENT_JOYSTICK_BUTTON_DOWN);
		break;

	// ASCENT: the gamepad
	case SDL_EVENT_GAMEPAD_ADDED:
		pad_open_first();
		break;

	case SDL_EVENT_GAMEPAD_REMOVED:
		if (Pad && ev->gdevice.which == Pad_id) {
			SDL_CloseGamepad(Pad);
			Pad = NULL;
			pad_release_all();
			pad_open_first();
		}
		break;

	case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
	case SDL_EVENT_GAMEPAD_BUTTON_UP:
		if (Pad && ev->gbutton.which == Pad_id)
			plat_input_pad_button(ev->gbutton.button, ev->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
		break;

	case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		if (Pad && ev->gaxis.which == Pad_id)
			plat_input_pad_axis(ev->gaxis.axis, ev->gaxis.value);
		break;

	case SDL_EVENT_JOYSTICK_HAT_MOTION:
		if (Joy && ev->jhat.which == Joy_id && ev->jhat.hat == 0)
			plat_input_joy_hat(ev->jhat.value);
		break;

	default:
		break;
	}
}

// ASCENT: video.c calls this after a change between the window and the
// full screen form, so that the mouse grab follows.
void plat_input_window_changed(void)
{
	Grab_dirty = 1;
	apply_relative_mode();
}
