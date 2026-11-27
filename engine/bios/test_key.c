/*
 * ASCENT: test of the keyboard logic in key.c.
 * It sends press and release events through plat_input_key, the function
 * that plat_pump calls for each key event. No window is open.
 */
#include <stdio.h>

#include "types.h"
#include "fix.h"
#include "key.h"
#include "timer.h"
#include "platform.h"

static int Failures;

#define CHECK(cond) do { \
		if (!(cond)) { \
			printf("FAIL line %d: %s\n", __LINE__, #cond); \
			Failures++; \
		} \
	} while (0)

static void press(int scancode)
{
	plat_input_key(scancode, 1);
}

static void release(int scancode)
{
	plat_input_key(scancode, 0);
}

int main(void)
{
	fix t, time_pressed;
	int i, k;

	timer_init();

	// Before key_init there is no handler. An event has no effect.
	press(KEY_A);
	CHECK(!keyd_pressed[KEY_A]);

	key_init();
	CHECK(keyd_repeat == 1);
	CHECK(key_inkey() == 0);
	CHECK(key_checkch() == 0);

	// ---- A new press: state, count and buffer.
	press(KEY_A);
	CHECK(keyd_pressed[KEY_A] == 1);
	CHECK(keyd_last_pressed == KEY_A);
	CHECK(key_checkch() == 1);
	CHECK(key_peekkey() == KEY_A);
	CHECK(key_peekkey() == KEY_A);			// peek does not remove the key
	CHECK(key_inkey() == KEY_A);
	CHECK(key_inkey() == 0);
	CHECK(key_down_count(KEY_A) == 1);
	CHECK(key_down_count(KEY_A) == 0);		// the read clears the count
	CHECK(key_up_count(KEY_A) == 0);

	// ---- The repeat rule. The system sends more press events while the
	// key is held. With keyd_repeat off they do not go to the buffer.
	keyd_repeat = 0;
	press(KEY_A);
	press(KEY_A);
	press(KEY_A);
	CHECK(keyd_pressed[KEY_A] == 1);
	CHECK(key_inkey() == 0);
	CHECK(key_down_count(KEY_A) == 0);		// a repeat is not a new press

	// With keyd_repeat on, each repeat goes to the buffer. The count stays.
	keyd_repeat = 1;
	press(KEY_A);
	press(KEY_A);
	CHECK(key_inkey() == KEY_A);
	CHECK(key_inkey() == KEY_A);
	CHECK(key_inkey() == 0);
	CHECK(key_down_count(KEY_A) == 0);

	// ---- Release.
	release(KEY_A);
	CHECK(keyd_pressed[KEY_A] == 0);
	CHECK(keyd_last_released == KEY_A);
	CHECK(key_up_count(KEY_A) == 1);
	CHECK(key_up_count(KEY_A) == 0);
	CHECK(key_inkey() == 0);				// a release does not go to the buffer

	// A press after a release is a new press, also with keyd_repeat off.
	keyd_repeat = 0;
	press(KEY_A);
	CHECK(key_inkey() == KEY_A);
	CHECK(key_down_count(KEY_A) == 1);
	release(KEY_A);
	press(KEY_A);
	release(KEY_A);
	press(KEY_A);
	release(KEY_A);
	CHECK(key_down_count(KEY_A) == 2);
	CHECK(key_up_count(KEY_A) == 3);
	CHECK(key_inkey() == KEY_A);
	CHECK(key_inkey() == KEY_A);
	CHECK(key_inkey() == 0);

	// ---- Modifier flags in the key code.
	press(KEY_LSHIFT);
	CHECK(key_inkey() == (KEY_LSHIFT | KEY_SHIFTED));	// as in the original
	press(KEY_B);
	CHECK(key_inkey() == (KEY_B | KEY_SHIFTED));
	release(KEY_B);
	release(KEY_LSHIFT);
	press(KEY_B);
	CHECK(key_inkey() == KEY_B);
	release(KEY_B);

	press(KEY_RSHIFT);
	press(KEY_UP);										// a key with the E0 prefix
	CHECK(key_inkey() == (KEY_RSHIFT | KEY_SHIFTED));
	CHECK(key_inkey() == (KEY_UP | KEY_SHIFTED));
	CHECK(keyd_pressed[KEY_UP] && !keyd_pressed[KEY_PAD8]);
	release(KEY_UP);
	release(KEY_RSHIFT);

	press(KEY_LCTRL);
	press(KEY_C);
	CHECK(key_inkey() == (KEY_LCTRL | KEY_CTRLED));
	CHECK(key_inkey() == (KEY_C | KEY_CTRLED));
	release(KEY_C);
	release(KEY_LCTRL);

	press(KEY_RCTRL);
	press(KEY_F1);
	key_inkey();
	CHECK(key_inkey() == (KEY_F1 | KEY_CTRLED));
	release(KEY_F1);
	release(KEY_RCTRL);

	press(KEY_LALT);
	press(KEY_ENTER);
	CHECK(key_inkey() == (KEY_LALT | KEY_ALTED));
	CHECK(key_inkey() == (KEY_ENTER | KEY_ALTED));
	release(KEY_ENTER);
	release(KEY_LALT);

	press(KEY_RALT);
	press(KEY_LSHIFT);
	press(KEY_LCTRL);
	press(KEY_X);
	key_inkey();
	key_inkey();
	key_inkey();
	CHECK(key_inkey() == (KEY_X | KEY_SHIFTED | KEY_ALTED | KEY_CTRLED));
	release(KEY_RALT);
	// The flags are the state at the time of the press.
	release(KEY_X);
	press(KEY_X);
	CHECK(key_inkey() == (KEY_X | KEY_SHIFTED | KEY_CTRLED));
	release(KEY_X);
	release(KEY_LCTRL);
	release(KEY_LSHIFT);
	CHECK(key_inkey() == 0);

	// ---- The buffer holds 15 keys. More keys are lost. The counts are not.
	key_flush();
	for (i = 0; i < 20; i++)
		press(KEY_1 + i);
	for (i = 0; i < 15; i++)
		CHECK(key_inkey() == KEY_1 + i);
	CHECK(key_inkey() == 0);
	for (i = 0; i < 20; i++) {
		CHECK(key_down_count(KEY_1 + i) == 1);
		release(KEY_1 + i);
	}

	// ---- key_flush clears the buffer, the state and the counts.
	press(KEY_Q);
	press(KEY_W);
	release(KEY_W);
	key_flush();
	CHECK(key_inkey() == 0);
	CHECK(!keyd_pressed[KEY_Q]);
	CHECK(key_down_count(KEY_Q) == 0);
	CHECK(key_down_count(KEY_W) == 0);
	CHECK(key_up_count(KEY_W) == 0);
	CHECK(key_down_time(KEY_Q) == 0);
	release(KEY_Q);
	key_flush();

	// ---- key_down_time: fixed point seconds since the last call.
	press(KEY_SPACEBAR);
	plat_sleep_ms(60);
	t = key_down_time(KEY_SPACEBAR);
	CHECK(t >= F1_0 / 20 && t < F1_0 / 2);			// 50 ms to 500 ms
	t = key_down_time(KEY_SPACEBAR);
	CHECK(t >= 0 && t < F1_0 / 20);					// only the time since the last call
	plat_sleep_ms(30);
	release(KEY_SPACEBAR);
	plat_sleep_ms(200);									// time after the release does not count
	t = key_down_time(KEY_SPACEBAR);
	CHECK(t >= F1_0 / 40 && t < F1_0 * 150 / 1000);
	CHECK(key_down_time(KEY_SPACEBAR) == 0);
	// Two short presses add up.
	press(KEY_SPACEBAR);
	plat_sleep_ms(20);
	release(KEY_SPACEBAR);
	press(KEY_SPACEBAR);
	plat_sleep_ms(20);
	release(KEY_SPACEBAR);
	t = key_down_time(KEY_SPACEBAR);
	CHECK(t >= F1_0 * 36 / 1000 && t < F1_0 / 4);
	// A repeat event does not start the time again.
	keyd_repeat = 1;
	press(KEY_SPACEBAR);
	plat_sleep_ms(30);
	press(KEY_SPACEBAR);
	t = key_down_time(KEY_SPACEBAR);
	CHECK(t >= F1_0 * 25 / 1000);
	release(KEY_SPACEBAR);
	CHECK(key_down_time(-1) == 0);
	CHECK(key_down_time(256) == 0);
	CHECK(key_down_count(256) == 0);
	CHECK(key_up_count(-1) == 0);
	key_flush();

	// ---- key_inkey_time gives the time of the press.
	t = timer_get_fixed_seconds();
	press(KEY_T);
	release(KEY_T);
	time_pressed = -1;
	CHECK(key_inkey_time(&time_pressed) == KEY_T);
	CHECK(time_pressed >= t && time_pressed <= timer_get_fixed_seconds());
	CHECK(time_pressed == keyd_time_when_last_pressed);

	// ---- key_getch returns a key that waits in the buffer.
	press(KEY_ESC);
	release(KEY_ESC);
	CHECK(key_getch() == KEY_ESC);

	// ---- Scan codes out of range have no effect.
	plat_input_key(0, 1);
	plat_input_key(256, 1);
	plat_input_key(-5, 1);
	CHECK(key_inkey() == 0);

	// ---- key_to_ascii.
	CHECK(key_to_ascii(KEY_A) == 'a');
	CHECK(key_to_ascii(KEY_A | KEY_SHIFTED) == 'A');
	CHECK(key_to_ascii(KEY_1 | KEY_SHIFTED) == '!');
	CHECK(key_to_ascii(KEY_SPACEBAR) == ' ');
	CHECK(key_to_ascii(KEY_F1) == 255);
	CHECK(key_to_ascii(KEY_UP) == 255);

	// ---- ASCENT: the text mode. Out of it a text event does nothing and a
	// field types the US table of the original.
	key_flush();
	plat_input_text("a");
	CHECK(key_inkey() == 0);
	CHECK(key_typed_char(KEY_A | KEY_SHIFTED) == 'A');
	CHECK(key_typed_char(KEY_F1) == 0);
	// In it, the key code of the press comes first and types nothing; the
	// character of the layout follows as KEY_TEXT plus its Latin-1 code.
	key_text_mode(1);
	CHECK(key_text_active());
	press(KEY_SEMICOL);				// the key of n tilde on a Spanish layout
	plat_input_text("\xC3\xB1");		// U+00F1 in UTF-8
	release(KEY_SEMICOL);
	k = key_inkey();
	CHECK(k == KEY_SEMICOL);
	CHECK(key_typed_char(k) == 0);
	k = key_inkey();
	CHECK(k == (KEY_TEXT | 0xF1));
	CHECK(key_typed_char(k) == 0xF1);
	CHECK(key_to_ascii(k) == (char) 255);
	// "Peñíscola" (with an accent from a dead key), "@" (AltGr+2), the
	// euro sign (outside Latin-1: dropped) and a control code (dropped)
	plat_input_text("Pe\xC3\xB1\xC3\xADscola@\xE2\x82\xAC\x01");
	{
		static const int want[] = { 'P', 'e', 0xF1, 0xED, 's', 'c', 'o', 'l', 'a', '@' };
		int i;
		for (i = 0; i < 10; i++)
			CHECK(key_typed_char(key_inkey()) == want[i]);
		CHECK(key_inkey() == 0);
	}
	// The end of the text mode removes the characters that no field took
	// and keeps the key codes in their order.
	press(KEY_X);
	plat_input_text("x");
	press(KEY_Y);
	plat_input_text("y");
	key_text_mode(0);
	CHECK(!key_text_active());
	CHECK(key_inkey() == KEY_X);
	CHECK(key_inkey() == KEY_Y);
	CHECK(key_inkey() == 0);
	release(KEY_X);
	release(KEY_Y);
	plat_input_text("x");
	CHECK(key_inkey() == 0);

	// ---- After key_close the events have no effect.
	key_close();
	press(KEY_Z);
	CHECK(!keyd_pressed[KEY_Z]);
	k = 0;
	(void) k;

	if (Failures) {
		printf("test_key: %d failures\n", Failures);
		return 1;
	}
	printf("test_key: ok\n");
	return 0;
}
