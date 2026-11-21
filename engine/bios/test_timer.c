/*
 * ASCENT: test of timer.c (the replacement for timer.asm).
 * It checks the conversion to fixed point seconds, the wrap of the
 * original, and that the time does not go back.
 */
#include <stdio.h>

#include "types.h"
#include "fix.h"
#include "timer.h"
#include "platform.h"

// In timer.c
uint64_t timer_us_to_ticks(uint64_t us);
fix timer_ticks_to_fix(uint64_t ticks);

static int Failures;

#define CHECK(cond) do { \
		if (!(cond)) { \
			printf("FAIL line %d: %s\n", __LINE__, #cond); \
			Failures++; \
		} \
	} while (0)

int main(void)
{
	fix t, last, a;
	uint64_t us0, us1;
	int i, tick0, tick1;

	// Microseconds to PIT ticks.
	CHECK(timer_us_to_ticks(0) == 0);
	CHECK(timer_us_to_ticks(1000000) == 1193180);
	CHECK(timer_us_to_ticks(500000) == 596590);
	CHECK(timer_us_to_ticks(3600ull * 1000000) == 3600ull * 1193180);
	// 400 days: more than a 64-bit product of microseconds and 1193180 can hold.
	CHECK(timer_us_to_ticks(400ull * 86400 * 1000000) == 400ull * 86400 * 1193180);

	// PIT ticks to fixed point seconds.
	CHECK(timer_ticks_to_fix(0) == 0);
	CHECK(timer_ticks_to_fix(1193180) == F1_0);
	CHECK(timer_ticks_to_fix(596590) == F0_5);
	CHECK(timer_ticks_to_fix(1193180 / 4) == F1_0 / 4);
	CHECK(timer_ticks_to_fix(1193179) == F1_0 - 1);
	CHECK(timer_ticks_to_fix(100ull * 1193180) == i2f(100));
	// The result is the integer part of ticks * 65536 / 1193180.
	CHECK(timer_ticks_to_fix(1) == 0);
	CHECK(timer_ticks_to_fix(19) == 1);
	CHECK(timer_ticks_to_fix(18) == 0);

	// The wrap of the original: negative after 32768 seconds, 0 after 65536 seconds.
	CHECK(timer_ticks_to_fix(32767ull * 1193180) == 0x7FFF0000);
	CHECK(timer_ticks_to_fix(32768ull * 1193180 - 1) == 0x7FFFFFFF);
	CHECK(timer_ticks_to_fix(32768ull * 1193180) == (fix) 0x80000000u);
	CHECK(timer_ticks_to_fix(32768ull * 1193180) < 0);
	CHECK(timer_ticks_to_fix(65536ull * 1193180 - 1) == -1);
	CHECK(timer_ticks_to_fix(65536ull * 1193180) == 0);
	CHECK(timer_ticks_to_fix(65537ull * 1193180) == F1_0);
	// A difference of two times is correct across the wrap.
	CHECK(timer_ticks_to_fix(32769ull * 1193180) - timer_ticks_to_fix(32767ull * 1193180) == i2f(2));

	timer_init();

	// The time does not go back.
	last = timer_get_fixed_seconds();
	CHECK(last >= 0);
	for (i = 0; i < 200000; i++) {
		t = timer_get_fixed_seconds();
		if (t < last) {
			printf("FAIL: time went back, %d after %d\n", (int) t, (int) last);
			Failures++;
			break;
		}
		last = t;
	}
	last = timer_get_fixed_secondsX();
	for (i = 0; i < 1000; i++) {
		t = timer_get_fixed_secondsX();
		CHECK(t >= last);
		last = t;
	}

	// The time agrees with the platform clock.
	us0 = plat_time_us();
	t = timer_get_fixed_seconds();
	plat_sleep_ms(50);
	last = timer_get_fixed_seconds();
	us1 = plat_time_us();
	CHECK(last - t >= F1_0 / 25);			// 40 ms at least
	CHECK(last - t <= (fix) ((us1 - us0) * 65536 / 1000000) + 2);

	// timer_delay waits for the given time.
	us0 = plat_time_us();
	timer_delay(F1_0 / 10);
	us1 = plat_time_us();
	CHECK(us1 - us0 >= 100000);
	CHECK(us1 - us0 < 1000000);
	timer_delay(0);
	timer_delay(-F1_0);

	// timer_get_approx_seconds: whole interrupt periods only.
	// After timer_init the period is 65536 ticks (18.2 Hz).
	for (i = 0; i < 1000; i++) {
		a = timer_get_approx_seconds();
		t = timer_get_fixed_seconds();
		CHECK(a <= t);
		CHECK(t - a <= 3600 + 2);			// 65536 ticks are 3600 fix units
		// a is a multiple of the period: a = floor(k * 65536 * 65536 / 1193180)
		CHECK(timer_ticks_to_fix(((uint64_t) a * 1193180 / 65536 + 65535) / 65536 * 65536) == a);
	}

	// timer_set_rate sets the time to 0 and changes the period (120 Hz here).
	plat_sleep_ms(20);
	CHECK(timer_get_fixed_seconds() > F1_0 / 100);
	timer_set_rate(1193180 / 120);
	t = timer_get_fixed_seconds();
	CHECK(t >= 0 && t < F1_0 / 100);
	last = timer_get_approx_seconds();
	for (i = 0; i < 1000; i++) {
		a = timer_get_approx_seconds();
		t = timer_get_fixed_seconds();
		CHECK(a >= last);
		CHECK(a <= t);
		CHECK(t - a <= 65536 / 120 + 2);
		last = a;
	}

	// The BIOS tick count: 18.2 ticks for each second.
	tick0 = TICKER;
	plat_sleep_ms(120);
	tick1 = TICKER;
	CHECK(tick0 >= 0 && tick0 < 1573040 + 100);	// ticks in one day
	CHECK(tick1 - tick0 >= 2 && tick1 - tick0 <= 20);

	timer_close();

	if (Failures) {
		printf("test_timer: %d failures\n", Failures);
		return 1;
	}
	printf("test_timer: ok\n");
	return 0;
}
