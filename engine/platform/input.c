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
static int Pad_started;		// ASCENT: the pad layer reads the gamepads (below)

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

// ASCENT: text input (platform.h)
static plat_text_handler Text_handler;
static int Text_wanted, Text_applied;

void plat_set_text_handler(plat_text_handler handler)
{
	Text_handler = handler;
}

void plat_input_text(const char *utf8)
{
	const unsigned char *s = (const unsigned char *) utf8;

	while (s && *s) {
		uint32_t cp;
		int n, i;
		if (*s < 0x80) { cp = *s; n = 1; }
		else if ((*s & 0xE0) == 0xC0) { cp = *s & 0x1F; n = 2; }
		else if ((*s & 0xF0) == 0xE0) { cp = *s & 0x0F; n = 3; }
		else if ((*s & 0xF8) == 0xF0) { cp = *s & 0x07; n = 4; }
		else { s++; continue; }				// not a lead byte
		for (i = 1; i < n; i++) {
			if ((s[i] & 0xC0) != 0x80)
				break;
			cp = (cp << 6) | (s[i] & 0x3F);
		}
		s += i;
		if (i == n && Text_handler)
			Text_handler(cp);
	}
}

// The text input of SDL belongs to a window: it starts when there is one.
static void apply_text_input(void)
{
	SDL_Window **windows;
	int count = 0;

	if (Text_wanted == Text_applied || !SDL_WasInit(SDL_INIT_VIDEO))
		return;
	windows = SDL_GetWindows(&count);
	if (windows && count > 0) {
		static int logged;
		bool ok = Text_wanted ? SDL_StartTextInput(windows[0]) : SDL_StopTextInput(windows[0]);
		if (logged++ < 100)
			plat_log("Input: text input %s: %s", Text_wanted ? "on" : "off", ok ? "ok" : SDL_GetError());
		Text_applied = Text_wanted;
	}
	SDL_free(windows);
}

void plat_text_input(int on)
{
	Text_wanted = (on != 0);
	apply_text_input();
}

uint32_t plat_key_layout_char(int scancode)
{
	int sc;

	if (!SDL_WasInit(SDL_INIT_VIDEO) || scancode <= 0 || scancode > 255)
		return 0;
	if (!Scancode_table_ready)
		init_scancode_table();
	for (sc = 1; sc < SDL_SCANCODE_COUNT; sc++)
		if (Scancode_table[sc] == scancode) {
			SDL_Keycode k = SDL_GetKeyFromScancode((SDL_Scancode) sc, SDL_KMOD_NONE, false);
			// A keycode with SDLK_SCANCODE_MASK is a key with no character
			if (k >= 32 && !(k & SDLK_SCANCODE_MASK) && k != SDLK_DELETE)
				return (uint32_t) k;
			return 0;
		}
	return 0;
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
		if (Relative_wanted != Relative_applied) {
			// ASCENT: the session log has each change of the mode (at most 200)
			static int logged;
			bool ok = SDL_SetWindowRelativeMouseMode(windows[0], Relative_wanted != 0);
			if (logged++ < 200)
				plat_log("Input: relative mouse mode %s: %s (SDL reports %d)", Relative_wanted ? "on" : "off",
					ok ? "ok" : SDL_GetError(), SDL_GetWindowRelativeMouseMode(windows[0]));
			if (ok)
				Relative_applied = Relative_wanted;
		}
		if (grab != Grab_applied) {
			static int logged;
			bool ok = SDL_SetWindowMouseGrab(windows[0], grab != 0);
			if (logged++ < 200)
				plat_log("Input: mouse grab %s: %s", grab ? "on" : "off", ok ? "ok" : SDL_GetError());
			if (ok)
				Grab_applied = grab;
		}
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
		// ASCENT: SDL lists a gamepad as a joystick too. While the pad
		// layer runs it reads the gamepad, so the joystick layer skips it
		// and a button does not act twice (audit A10).
		if (Pad_started && SDL_IsGamepad(ids[i]))
			continue;
		Joy = SDL_OpenJoystick(ids[i]);
		if (Joy) {
			const char *name = SDL_GetJoystickName(Joy);
			Joy_id = ids[i];
			plat_log("Input: joystick '%s' (vendor %04x product %04x), %d axes", name ? name : "?",
				SDL_GetJoystickVendor(Joy), SDL_GetJoystickProduct(Joy), SDL_GetNumJoystickAxes(Joy));
		}
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
static int Pad_test;
static unsigned char Pad_buttons[PLAT_PAD_BUTTONS];
static unsigned char Pad_presses[PLAT_PAD_BUTTONS];
static int Pad_axes[PLAT_PAD_AXES];
// ASCENT: an axis counts after it was at rest once (near 0) since the pad
// was opened. A device that SDL takes for a gamepad can hold an axis at an
// end (a stick that is not there, a sensor): the right stick y looks up or
// down in each frame, and the look keys and the mouse cannot move the view.
static unsigned char Pad_axis_rested[PLAT_PAD_AXES];
#define PAD_REST_LIMIT	8000
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
	if (!Pad_axis_rested[axis] && value > -PAD_REST_LIMIT && value < PAD_REST_LIMIT) {
		Pad_axis_rested[axis] = 1;
		if (Pad)
			plat_log("Input: gamepad axis %d at rest (%d): it counts from now on", axis, value);
	}
	Pad_axes[axis] = value;
	// The left stick in the menus: an arrow key when it passes one half
	if ((axis == PLAT_PAD_LEFTX || axis == PLAT_PAD_LEFTY) && (Pad_axis_rested[axis] || !Pad)) {
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
	if (Pad) {
		// ASCENT: the session log names the pad and the axes at the open
		static const SDL_GamepadAxis axes[PLAT_PAD_AXES] = { SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
			SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER };
		int v[PLAT_PAD_AXES];
		const char *name = SDL_GetGamepadName(Pad);

		for (i = 0; i < PLAT_PAD_AXES; i++) {
			v[i] = SDL_GetGamepadAxis(Pad, axes[i]);
			Pad_axes[i] = v[i];
			Pad_axis_rested[i] = v[i] > -PAD_REST_LIMIT && v[i] < PAD_REST_LIMIT;
		}
		plat_log("Input: gamepad '%s' (vendor %04x product %04x), axes %d %d %d %d %d %d",
			name ? name : "?", SDL_GetGamepadVendor(Pad), SDL_GetGamepadProduct(Pad), v[0], v[1], v[2], v[3], v[4], v[5]);
	}
}

int plat_pad_init(void)
{
	if (!Pad_started) {
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
			return 0;
		Pad_started = 1;
		// ASCENT: a gamepad that the joystick layer opened goes to the pad
		// layer (audit A10)
		if (Joy && SDL_IsGamepad(Joy_id)) {
			SDL_CloseJoystick(Joy);
			Joy = NULL;
			joy_release_all();
			joy_open_first();
		}
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
	if (axis < 0 || axis >= PLAT_PAD_AXES)
		return 0;
	// ASCENT: the script of a test (no pad open) sets the axes directly
	return (Pad_axis_rested[axis] || !Pad) ? Pad_axes[axis] : 0;
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
// ASCENT: the input lines of the session log (docs/debug_logs.md, "Input
// lines"). The devices are always written. With -inputlog, the first
// DIAG_FIRST key events and mouse events are written one by one, then one
// summary line for each second with input.

#define DIAG_FIRST	200

static int Input_log;
static int Diag_keys, Diag_motions, Diag_buttons;
static Uint64 Diag_second;
static struct {
	int key_down, key_up, repeats, motions, buttons, wheels;
	float xrel, yrel;
	int zero_motions;		// motion events with xrel and yrel 0
	int warps;				// warps of plat_input_hold_pointer
	SDL_MouseID last_mouse;
	SDL_KeyboardID last_keyboard;
} Diag;

void plat_set_input_log(int on)
{
	Input_log = (on != 0);
}

int plat_input_log(void)
{
	return Input_log;
}

static void diag_device_lists(void)
{
	SDL_KeyboardID *kb;
	SDL_MouseID *mice;
	int n = 0, i;

	kb = SDL_GetKeyboards(&n);
	plat_log("Input: %d keyboards", n);
	for (i = 0; kb && i < n; i++)
		plat_log("Input: keyboard %u '%s'", (unsigned) kb[i], SDL_GetKeyboardNameForID(kb[i]) ? SDL_GetKeyboardNameForID(kb[i]) : "?");
	SDL_free(kb);
	n = 0;
	mice = SDL_GetMice(&n);
	plat_log("Input: %d mice", n);
	for (i = 0; mice && i < n; i++)
		plat_log("Input: mouse %u '%s'", (unsigned) mice[i], SDL_GetMouseNameForID(mice[i]) ? SDL_GetMouseNameForID(mice[i]) : "?");
	SDL_free(mice);
}

#if defined(__unix__) && !defined(__APPLE__)
// The X server: vendor, release, and the XInputExtension (XInput2 is what
// SDL uses for the relative mouse motion).
static void diag_x11(void)
{
	SDL_Window **windows;
	void *display = NULL;
	int n = 0;
	SDL_SharedObject *lib, *xi;

	windows = SDL_GetWindows(&n);
	if (windows && n > 0)
		display = SDL_GetPointerProperty(SDL_GetWindowProperties(windows[0]), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
	SDL_free(windows);
	xi = SDL_LoadObject("libXi.so.6");
	plat_log("Input: libXi.so.6 %s", xi ? "loads" : "does not load (no XInput2: SDL moves the pointer for the relative mode)");
	if (xi)
		SDL_UnloadObject(xi);
	if (!display)
		return;
	lib = SDL_LoadObject("libX11.so.6");
	if (lib) {
		char *(*server_vendor)(void *) = (char *(*)(void *)) SDL_LoadFunction(lib, "XServerVendor");
		int (*vendor_release)(void *) = (int (*)(void *)) SDL_LoadFunction(lib, "XVendorRelease");
		int (*query_ext)(void *, const char *, int *, int *, int *) =
			(int (*)(void *, const char *, int *, int *, int *)) SDL_LoadFunction(lib, "XQueryExtension");
		int op = 0, ev = 0, err = 0;
		if (server_vendor && vendor_release)
			plat_log("Input: X server '%s', release %d", server_vendor(display), vendor_release(display));
		if (query_ext)
			plat_log("Input: X extension XInputExtension %s", query_ext(display, "XInputExtension", &op, &ev, &err) ? "present" : "missing");
		SDL_UnloadObject(lib);
	}
}
#else
#define diag_x11()
#endif

void plat_input_log_start(void)
{
	static const char *hints[] = {
		SDL_HINT_MOUSE_RELATIVE_MODE_CENTER, SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE,
		SDL_HINT_MOUSE_RELATIVE_SYSTEM_SCALE, SDL_HINT_MOUSE_RELATIVE_SPEED_SCALE, SDL_HINT_MOUSE_NORMAL_SPEED_SCALE,
		SDL_HINT_MOUSE_RELATIVE_CURSOR_VISIBLE, SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, SDL_HINT_MOUSE_AUTO_CAPTURE,
		SDL_HINT_VIDEO_X11_XRANDR, SDL_HINT_VIDEO_DRIVER, SDL_HINT_KEYCODE_OPTIONS,
		SDL_HINT_X11_FORCE_OVERRIDE_REDIRECT, SDL_HINT_VIDEO_FORCE_EGL };
	static const char *env[] = { "XDG_SESSION_TYPE", "XDG_CURRENT_DESKTOP", "XDG_SESSION_DESKTOP", "DESKTOP_SESSION",
		"DISPLAY", "WAYLAND_DISPLAY", "SDL_VIDEO_DRIVER", "LANG", "XMODIFIERS", "GTK_IM_MODULE" };
	SDL_Window **windows;
	SDL_DisplayID *displays;
	int v = SDL_GetVersion(), n = 0, i;

	plat_log("Input: SDL compiled %d.%d.%d, linked %d.%d.%d (%s)", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_MICRO_VERSION,
		SDL_VERSIONNUM_MAJOR(v), SDL_VERSIONNUM_MINOR(v), SDL_VERSIONNUM_MICRO(v), SDL_GetRevision());
	for (i = 0; i < (int) (sizeof(env) / sizeof(env[0])); i++)
		plat_log("Input: environment %s=%s", env[i], SDL_getenv(env[i]) ? SDL_getenv(env[i]) : "(not set)");
	for (i = 0; i < (int) (sizeof(hints) / sizeof(hints[0])); i++)
		plat_log("Input: hint %s=%s", hints[i], SDL_GetHint(hints[i]) ? SDL_GetHint(hints[i]) : "(default)");
	diag_x11();
	displays = SDL_GetDisplays(&n);
	for (i = 0; displays && i < n; i++) {
		SDL_Rect b, u;
		SDL_GetDisplayBounds(displays[i], &b);
		SDL_GetDisplayUsableBounds(displays[i], &u);
		plat_log("Input: display %u bounds %d,%d %dx%d, usable %d,%d %dx%d, content scale %.2f", (unsigned) displays[i],
			b.x, b.y, b.w, b.h, u.x, u.y, u.w, u.h, SDL_GetDisplayContentScale(displays[i]));
	}
	SDL_free(displays);
	n = 0;
	windows = SDL_GetWindows(&n);
	if (windows && n > 0) {
		int x = 0, y = 0, w = 0, h = 0;
		SDL_GetWindowPosition(windows[0], &x, &y);
		SDL_GetWindowSize(windows[0], &w, &h);
		plat_log("Input: window flags 0x%llx, position %d,%d, size %dx%d, display scale %.2f, pixel density %.2f",
			(unsigned long long) SDL_GetWindowFlags(windows[0]), x, y, w, h,
			SDL_GetWindowDisplayScale(windows[0]), SDL_GetWindowPixelDensity(windows[0]));
	}
	SDL_free(windows);
	diag_device_lists();
}

// The summary of one second with input
static void diag_second(void)
{
	Uint64 now = SDL_GetTicks();

	if (!Input_log || now - Diag_second < 1000)
		return;
	Diag_second = now;
	if (Diag.key_down || Diag.key_up || Diag.motions || Diag.buttons || Diag.wheels || Diag.warps)
		plat_log("Input second: keys %d down (%d repeats) %d up (keyboard %u), mouse %d motions (%d with no relative motion) sum xrel %.1f yrel %.1f (mouse %u), %d buttons, %d wheel, %d warps, relative %d, grab %d, focus %d",
			Diag.key_down, Diag.repeats, Diag.key_up, (unsigned) Diag.last_keyboard, Diag.motions, Diag.zero_motions,
			Diag.xrel, Diag.yrel, (unsigned) Diag.last_mouse, Diag.buttons, Diag.wheels, Diag.warps, Relative_applied, Grab_applied,
			SDL_GetKeyboardFocus() != NULL);
	SDL_zero(Diag);
}

static void diag_event(const SDL_Event *ev)
{
	if (!Input_log && ev->type != SDL_EVENT_KEYBOARD_ADDED && ev->type != SDL_EVENT_KEYBOARD_REMOVED &&
		ev->type != SDL_EVENT_MOUSE_ADDED && ev->type != SDL_EVENT_MOUSE_REMOVED)
		return;
	switch (ev->type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		if (ev->type == SDL_EVENT_KEY_DOWN) {
			Diag.key_down++;
			Diag.repeats += ev->key.repeat != 0;
		} else
			Diag.key_up++;
		Diag.last_keyboard = ev->key.which;
		if (Diag_keys < DIAG_FIRST) {
			Diag_keys++;
			plat_log("Input key %s: scancode %d (%s), key 0x%x, mod 0x%x, repeat %d, keyboard %u, game code 0x%02x",
				ev->type == SDL_EVENT_KEY_DOWN ? "down" : "up", (int) ev->key.scancode,
				SDL_GetScancodeName(ev->key.scancode), (unsigned) ev->key.key, (unsigned) ev->key.mod, ev->key.repeat != 0,
				(unsigned) ev->key.which,
				ev->key.scancode > 0 && ev->key.scancode < SDL_SCANCODE_COUNT ? Scancode_table[ev->key.scancode] : 0);
		}
		break;
	case SDL_EVENT_MOUSE_MOTION:
		Diag.motions++;
		Diag.xrel += ev->motion.xrel;
		Diag.yrel += ev->motion.yrel;
		Diag.zero_motions += ev->motion.xrel == 0 && ev->motion.yrel == 0;
		Diag.last_mouse = ev->motion.which;
		if (Diag_motions < DIAG_FIRST) {
			Diag_motions++;
			plat_log("Input motion: mouse %u, x %.1f y %.1f (frame pixels), xrel %.2f yrel %.2f, relative %d",
				(unsigned) ev->motion.which, ev->motion.x, ev->motion.y, ev->motion.xrel, ev->motion.yrel, Relative_applied);
		}
		break;
	case SDL_EVENT_MOUSE_BUTTON_DOWN:
	case SDL_EVENT_MOUSE_BUTTON_UP:
		Diag.buttons++;
		if (Diag_buttons < DIAG_FIRST) {
			Diag_buttons++;
			plat_log("Input button %s: mouse %u, button %d, x %.1f y %.1f", ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN ? "down" : "up",
				(unsigned) ev->button.which, ev->button.button, ev->button.x, ev->button.y);
		}
		break;
	case SDL_EVENT_MOUSE_WHEEL:
		Diag.wheels++;
		break;
	case SDL_EVENT_TEXT_INPUT:			// ASCENT
		if (Diag_keys < DIAG_FIRST) {
			const unsigned char *t = (const unsigned char *) ev->text.text;
			char hex[64];
			int n = 0;
			hex[0] = 0;
			while (t && *t && n < (int) sizeof(hex) - 4)
				n += SDL_snprintf(hex + n, sizeof(hex) - n, " %02x", *t++);
			Diag_keys++;
			plat_log("Input text: UTF-8%s, text input %d", hex, Text_applied);
		}
		break;
	case SDL_EVENT_KEYBOARD_ADDED:
	case SDL_EVENT_KEYBOARD_REMOVED:
		plat_log("Input: keyboard %u %s ('%s')", (unsigned) ev->kdevice.which,
			ev->type == SDL_EVENT_KEYBOARD_ADDED ? "added" : "removed",
			SDL_GetKeyboardNameForID(ev->kdevice.which) ? SDL_GetKeyboardNameForID(ev->kdevice.which) : "?");
		break;
	case SDL_EVENT_MOUSE_ADDED:
	case SDL_EVENT_MOUSE_REMOVED:
		plat_log("Input: mouse %u %s ('%s')", (unsigned) ev->mdevice.which,
			ev->type == SDL_EVENT_MOUSE_ADDED ? "added" : "removed",
			SDL_GetMouseNameForID(ev->mdevice.which) ? SDL_GetMouseNameForID(ev->mdevice.which) : "?");
		break;
	case SDL_EVENT_WINDOW_FOCUS_GAINED:
	case SDL_EVENT_WINDOW_FOCUS_LOST:
	case SDL_EVENT_WINDOW_MOUSE_ENTER:
	case SDL_EVENT_WINDOW_MOUSE_LEAVE:
	case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
	case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
	case SDL_EVENT_WINDOW_RESIZED:
	case SDL_EVENT_WINDOW_MOVED: {
		static int count;
		if (count++ < 300)
			plat_log("Input window: %s, data %d %d, relative %d, grab %d",
				ev->type == SDL_EVENT_WINDOW_FOCUS_GAINED ? "focus gained" : ev->type == SDL_EVENT_WINDOW_FOCUS_LOST ? "focus lost" :
				ev->type == SDL_EVENT_WINDOW_MOUSE_ENTER ? "mouse enter" : ev->type == SDL_EVENT_WINDOW_MOUSE_LEAVE ? "mouse leave" :
				ev->type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ? "enter full screen" : ev->type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN ? "leave full screen" :
				ev->type == SDL_EVENT_WINDOW_RESIZED ? "resized" : "moved",
				ev->window.data1, ev->window.data2, Relative_applied, Grab_applied);
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------------
// Events from plat_pump

void plat_input_sdl_event(const SDL_Event *ev)
{
	apply_relative_mode();
	apply_text_input();
	if (!Scancode_table_ready)
		init_scancode_table();
	diag_event(ev);
	diag_second();

	switch (ev->type) {
	case SDL_EVENT_KEY_DOWN:
	case SDL_EVENT_KEY_UP:
		if (!Scancode_table_ready)
			init_scancode_table();
		if (ev->key.scancode > 0 && ev->key.scancode < SDL_SCANCODE_COUNT)
			plat_input_key(Scancode_table[ev->key.scancode], ev->type == SDL_EVENT_KEY_DOWN);
		break;

	case SDL_EVENT_TEXT_INPUT:			// ASCENT: the characters of the layout of the user
		if (Text_applied)
			plat_input_text(ev->text.text);
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

// ASCENT: SDL holds the pointer at the center in the relative mode, but
// after a change of the focus (a click on the window, Alt+Tab) it can
// delay that or leave the pointer where the click was, and the system can
// show the pointer again. Each frame with the focus, this sets the
// relative mode again if it is off, hides the pointer and moves it to the
// center of the window if it is not there. A move with a button held
// outside the window area (a move of the window by its title bar, a change
// of its size) is not stopped.
void plat_input_hold_pointer(void)
{
	SDL_Window **windows;
	SDL_Window *win = NULL;
	int count = 0;
	int wx, wy, ww, wh, inside;
	float gx, gy, cx, cy;
	SDL_MouseButtonFlags held;

	diag_second();		// ASCENT: the input summary also in a second with no event
	if (!Relative_wanted || !SDL_WasInit(SDL_INIT_VIDEO))
		return;
	windows = SDL_GetWindows(&count);
	if (windows && count > 0)
		win = windows[0];
	SDL_free(windows);
	if (!win || !(SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS))
		return;
	if (!SDL_GetWindowRelativeMouseMode(win)) {
		static int logged;
		bool ok = SDL_SetWindowRelativeMouseMode(win, true);
		if (logged++ < 50)
			plat_log("Input: the relative mode was off with the focus: set again, %s", ok ? "ok" : SDL_GetError());
		Relative_applied = 1;
	}
	if (SDL_CursorVisible())
		SDL_HideCursor();
	// ASCENT: Wayland has no global pointer position and no window
	// position (SDL gives 0), and SDL does not hold the pointer at the
	// center there, so the test below warps after each motion. Without the
	// warp protocol a warp of SDL takes the pointer lock of the relative
	// mode away and sets it again. The relative mode of Wayland locks the
	// pointer itself: no warp. (Not tested on a Wayland desktop.)
	{
		const char *driver = SDL_GetCurrentVideoDriver();
		if (driver && SDL_strcmp(driver, "wayland") == 0)
			return;
	}
	if (!SDL_GetWindowPosition(win, &wx, &wy) || !SDL_GetWindowSize(win, &ww, &wh) || ww <= 0 || wh <= 0)
		return;
	held = SDL_GetGlobalMouseState(&gx, &gy);
	cx = (float) wx + (float) (ww / 2);
	cy = (float) wy + (float) (wh / 2);
	inside = gx >= (float) wx && gy >= (float) wy && gx < (float) (wx + ww) && gy < (float) (wy + wh);
	if ((gx - cx > 2.0f || cx - gx > 2.0f || gy - cy > 2.0f || cy - gy > 2.0f) && (inside || !held)) {
		static int logged;
		if (logged++ < 20)
			plat_log("Input: warp of the pointer from %.1f,%.1f to the center %.1f,%.1f (window %d,%d %dx%d, buttons 0x%x)",
				gx, gy, cx, cy, wx, wy, ww, wh, (unsigned) held);
		Diag.warps++;
		SDL_WarpMouseGlobal(cx, cy);
	}
}

// ASCENT: video.c calls this after a change between the window and the
// full screen form, so that the mouse grab follows.
void plat_input_window_changed(void)
{
	Grab_dirty = 1;
	apply_relative_mode();
}
