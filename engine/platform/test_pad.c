/*
 * ASCENT: test of the gamepad state in input.c. It sends button and axis
 * events through plat_input_pad_button and plat_input_pad_axis, the
 * functions that plat_pump calls for the gamepad events. No device and no
 * window are open.
 */
#include <stdio.h>

#include "platform.h"

static int Failures;
static int Keys[16], N_keys;

#define CHECK(cond) do { \
		if (!(cond)) { \
			printf("FAIL line %d: %s\n", __LINE__, #cond); \
			Failures++; \
		} \
	} while (0)

static void key_handler(int scancode, int down)
{
	if (down && N_keys < 16)
		Keys[N_keys++] = scancode;
}

int main(void)
{
	plat_set_key_handler(key_handler);
	CHECK(!plat_pad_present());

	// A button: state and press count
	plat_input_pad_button(PLAT_PAD_X, 1);
	CHECK(plat_pad_present());
	CHECK(plat_pad_button(PLAT_PAD_X) == 1);
	CHECK(plat_pad_button_presses(PLAT_PAD_X) == 1);
	CHECK(plat_pad_button_presses(PLAT_PAD_X) == 0);
	plat_input_pad_button(PLAT_PAD_X, 1);		// no new press while down
	CHECK(plat_pad_button_presses(PLAT_PAD_X) == 0);
	plat_input_pad_button(PLAT_PAD_X, 0);
	CHECK(plat_pad_button(PLAT_PAD_X) == 0);
	CHECK(plat_pad_button(99) == 0);			// out of range

	// The menus (no game frame yet): d-pad and A, B give keys
	N_keys = 0;
	plat_input_pad_button(PLAT_PAD_DPAD_DOWN, 1);
	plat_input_pad_button(PLAT_PAD_DPAD_DOWN, 0);
	plat_input_pad_button(PLAT_PAD_A, 1);
	plat_input_pad_button(PLAT_PAD_A, 0);
	plat_input_pad_button(PLAT_PAD_B, 1);
	plat_input_pad_button(PLAT_PAD_B, 0);
	CHECK(N_keys == 3 && Keys[0] == 0xD0 && Keys[1] == 0x1C && Keys[2] == 0x01);

	// The left stick gives one arrow when it passes one half
	N_keys = 0;
	plat_input_pad_axis(PLAT_PAD_LEFTY, -20000);
	plat_input_pad_axis(PLAT_PAD_LEFTY, -30000);
	plat_input_pad_axis(PLAT_PAD_LEFTY, 0);
	plat_input_pad_axis(PLAT_PAD_LEFTX, 32767);
	CHECK(N_keys == 2 && Keys[0] == 0xC8 && Keys[1] == 0xCD);
	CHECK(plat_pad_axis(PLAT_PAD_LEFTX) == 32767);
	plat_input_pad_axis(PLAT_PAD_LEFTX, 0);

	// In the game: no menu keys, but Start is Escape
	plat_pad_game_frame();
	N_keys = 0;
	plat_input_pad_button(PLAT_PAD_A, 1);
	plat_input_pad_button(PLAT_PAD_A, 0);
	plat_input_pad_axis(PLAT_PAD_LEFTY, -32768);
	plat_input_pad_button(PLAT_PAD_START, 1);
	plat_input_pad_button(PLAT_PAD_START, 0);
	CHECK(N_keys == 1 && Keys[0] == 0x01);
	CHECK(plat_pad_axis(PLAT_PAD_LEFTY) == -32768);

	if (Failures)
		printf("%d failures\n", Failures);
	else
		printf("gamepad: all checks passed\n");
	return Failures != 0;
}
