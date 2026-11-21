/*
 * ASCENT: interactive input test. It is not an automatic test.
 *
 * It opens a window and prints what the input functions of engine/bios
 * return: key codes from the key buffer, key counts, mouse buttons and
 * motion, joystick axes, buttons and hat.
 *
 * Keys:  Esc    quit
 *        F1     keyd_repeat on / off (hold a key to see the difference)
 *        F2     relative mouse mode on / off
 *        F3     print the mouse position and the joystick axes
 *        F4     key_flush, mouse_flush, joy_flush
 *
 * engine/bios/CMakeLists.txt builds this program, because it uses d_bios.
 */
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "key.h"
#include "mouse.h"
#include "joy.h"
#include "timer.h"
#include "platform.h"

#define W 320
#define H 200

static uint8_t Frame[W * H];
static uint8_t Palette[768];

static void draw_cross(int x, int y, uint8_t color)
{
	int i;
	for (i = -4; i <= 4; i++) {
		if (x + i >= 0 && x + i < W && y >= 0 && y < H)
			Frame[y * W + x + i] = color;
		if (y + i >= 0 && y + i < H && x >= 0 && x < W)
			Frame[(y + i) * W + x] = color;
	}
}

int main(int argc, char **argv)
{
	int relative = 1;
	int i, k, done = 0;
	int last_axes[4] = { 0, 0, 0, 0 };
	int last_joy_present = -1;

	(void) argc;
	(void) argv;

	if (plat_init("ASCENT input test", W, H, 3) != 0) {
		printf("plat_init failed\n");
		return 1;
	}
	for (i = 0; i < 256; i++)
		Palette[i * 3] = Palette[i * 3 + 1] = Palette[i * 3 + 2] = (uint8_t) (i >> 2);

	timer_init();
	key_init();
	printf("mouse_init: %d buttons\n", mouse_init(0));
	mouse_set_limits(0, 0, W - 1, H - 1);
	printf("joy_init: %d\n", joy_init());
	printf("Esc quit, F1 repeat, F2 relative mouse, F3 positions, F4 flush\n");
	printf("keyd_repeat=%d relative=%d\n", keyd_repeat, relative);

	while (!done && plat_pump()) {
		int dx, dy, mx, my, n;
		int axes[4];
		ubyte mask;

		// Key buffer. key_inkey also processes the window events.
		while ((k = key_inkey()) != 0) {
			int c = key_to_ascii(k);
			printf("key_inkey: 0x%03X scan 0x%02X%s%s%s", k, k & 0xFF,
				(k & KEY_SHIFTED) ? " SHIFT" : "",
				(k & KEY_CTRLED) ? " CTRL" : "",
				(k & KEY_ALTED) ? " ALT" : "");
			if (c != 255)
				printf(" '%c'", c);
			printf("\n");

			switch (k) {
			case KEY_ESC:
				done = 1;
				break;
			case KEY_F1:
				keyd_repeat = !keyd_repeat;
				printf("keyd_repeat=%d\n", keyd_repeat);
				break;
			case KEY_F2:
				relative = !relative;
				mouse_set_relative(relative);
				printf("relative=%d\n", relative);
				break;
			case KEY_F3:
				mouse_get_pos(&mx, &my);
				printf("mouse pos %d,%d  buttons 0x%X\n", mx, my, mouse_get_btns());
				mask = joystick_read_raw_axis(JOY_ALL_AXIS, axes);
				printf("joy present=%d mask=0x%X raw %d %d %d %d  buttons 0x%X\n",
					joy_present, mask, axes[0], axes[1], axes[2], axes[3], joy_get_btns());
				joy_get_pos(&mx, &my);
				printf("joy_get_pos %d,%d\n", mx, my);
				break;
			case KEY_F4:
				key_flush();
				mouse_flush();
				joy_flush();
				printf("flush\n");
				break;
			}
		}

		// Key counts: a new press and a release, not a held key.
		for (i = 1; i < 256; i++) {
			n = key_down_count(i);
			if (n)
				printf("key_down_count(0x%02X) = %d\n", i, n);
			n = key_up_count(i);
			if (n)
				printf("key_up_count(0x%02X) = %d, down time %.3f s\n", i, n, f2fl(key_down_time(i)));
		}

		// Mouse.
		mouse_get_delta(&dx, &dy);
		if (dx || dy) {
			mouse_get_pos(&mx, &my);
			printf("mouse delta %d,%d  pos %d,%d\n", dx, dy, mx, my);
		}
		for (i = 0; i < 3; i++) {
			n = mouse_button_down_count(i);
			if (n)
				printf("mouse_button_down_count(%d) = %d\n", i, n);
			else if (!mouse_button_state(i)) {
				fix t = mouse_button_down_time(i);
				if (t)
					printf("mouse button %d released, down time %.3f s\n", i, f2fl(t));
			}
		}

		// Joystick. Call joy_init again to find a stick that was connected later.
		if (plat_joy_present() != last_joy_present) {
			last_joy_present = plat_joy_present();
			printf("joystick device %s, joy_init: %d\n",
				last_joy_present ? "present" : "not present", joy_init());
		}
		for (i = 0; i < 20; i++) {
			n = joy_get_button_down_cnt(i);
			if (n)
				printf("joy_get_button_down_cnt(%d) = %d\n", i, n);
			n = joy_get_button_up_cnt(i);
			if (n)
				printf("joy_get_button_up_cnt(%d) = %d, down time %.3f s\n", i, n,
					f2fl(joy_get_button_down_time(i)));
		}
		mask = joy_read_stick(JOY_ALL_AXIS, axes);
		for (i = 0; i < 4; i++) {
			int d = axes[i] - last_axes[i];
			if (d > 8 || d < -8) {
				printf("joy axes (scaled -128..127) %d %d %d %d  mask 0x%X\n",
					axes[0], axes[1], axes[2], axes[3], mask);
				memcpy(last_axes, axes, sizeof(last_axes));
				break;
			}
		}

		// Picture: the mouse position (white) and the joystick position (gray).
		memset(Frame, 40, sizeof(Frame));
		draw_cross(W / 2 + axes[0], H / 2 + axes[1] * 3 / 4, 160);
		mouse_get_pos(&mx, &my);
		draw_cross(mx, my, 255);
		plat_present(Frame, W, H, W, Palette);
		fflush(stdout);
	}

	joy_close();
	mouse_close();
	key_close();
	timer_close();
	plat_shutdown();
	return 0;
}
