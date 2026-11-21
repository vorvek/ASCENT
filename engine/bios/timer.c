/*
 * ASCENT: replacement for timer.asm.
 *
 * The original code set the rate of the PIT (8253) channel 0, counted the
 * timer interrupts (INT 8) and read the PIT counter to get the time between
 * two interrupts. This file computes the same numbers from the clock of the
 * platform layer (plat_time_us). No interrupt runs.
 *
 * Kept from the original:
 * - The unit of the time base is the PIT tick (1/1193180 second).
 * - timer_get_fixed_seconds returns floor(ticks * 65536 / 1193180) as a
 *   32-bit number. The assembly reduced the high half of the dividend with
 *   a subtract loop to prevent a divide overflow, which gives the quotient
 *   modulo 2^32. So the value becomes negative after 32768 seconds
 *   (9.1 hours) and returns to 0 after 65536 seconds (18.2 hours).
 * - timer_get_approx_seconds counts whole interrupt periods only. Its step
 *   is count_val / 1193180 second (1/18.2 second after timer_init,
 *   1/120 second after the game calls timer_set_rate).
 * - timer_set_rate sets the time to 0, as the assembly did (it cleared the
 *   interrupt count).
 *
 * Changed:
 * - The time runs before timer_init is called. It starts when the platform
 *   clock starts.
 * - timer_set_function stores the pointer and nothing calls it. The
 *   original called it from the interrupt. The sound code must use another
 *   method.
 * - timer_get_fixed_secondsX is the same as timer_get_fixed_seconds.
 *   The original X version only left out the interrupt disable.
 */
#include <stdlib.h>
#include <time.h>

#include "types.h"
#include "fix.h"
#include "timer.h"
#include "platform.h"

static uint64_t Timer_base_us;			// platform time at tick 0
static uint32_t Timer_cnt = 65536;		// PIT ticks for each interrupt
static void *Timer_user_function;
static int Timer_installed;

// Convert microseconds to PIT ticks. Exposed for the test.
uint64_t timer_us_to_ticks(uint64_t us)
{
	uint64_t sec = us / 1000000u;
	uint64_t rem = us % 1000000u;

	return sec * TIMER_FREQUENCY + rem * TIMER_FREQUENCY / 1000000u;
}

// Convert PIT ticks to fixed point seconds, with the original wrap.
// Exposed for the test.
fix timer_ticks_to_fix(uint64_t ticks)
{
	return (fix) (uint32_t) ((ticks << 16) / TIMER_FREQUENCY);
}

// PIT ticks since the time base. This is the result of timer_get_stamp64.
static uint64_t timer_get_ticks(void)
{
	return timer_us_to_ticks(plat_time_us() - Timer_base_us);
}

fix timer_get_fixed_seconds()
{
	return timer_ticks_to_fix(timer_get_ticks());
}

fix timer_get_fixed_secondsX()
{
	return timer_ticks_to_fix(timer_get_ticks());
}

fix timer_get_approx_seconds()
{
	uint64_t ticks = timer_get_ticks();

	return timer_ticks_to_fix(ticks - ticks % Timer_cnt);
}

void timer_set_rate(int count_val)
{
	uint32_t cnt = (uint32_t) count_val;

	if (cnt > 65535 || cnt == 0)
		cnt = 65536;
	Timer_cnt = cnt;
	Timer_base_us = plat_time_us();
}

void timer_set_function(void *function)
{
	Timer_user_function = function;
}

void timer_init()
{
	if (Timer_installed)
		return;
	Timer_installed = 1;
	Timer_cnt = 65536;			// the normal 18.2 Hz of the BIOS
	Timer_user_function = NULL;
	Timer_base_us = plat_time_us();
	atexit(timer_close);
}

void timer_close()
{
	Timer_installed = 0;
}

// Wait. The window events are processed during the wait.
void timer_delay(fix seconds)
{
	uint64_t end;

	if (seconds <= 0)
		return;
	end = plat_time_us() + (uint64_t) seconds * 1000000u / 65536u;
	for (;;) {
		uint64_t now = plat_time_us();
		uint64_t left;
		if (now >= end)
			break;
		left = end - now;
		plat_pump();
		if (left >= 10000)
			plat_sleep_ms(10);
		else if (left >= 1000)
			plat_sleep_ms((int) (left / 1000));
		// less than 1 ms: spin
	}
}

// The replacement for the BIOS tick count at address 0x46C: the number of
// 18.2 Hz ticks since midnight. It does not return to 0 at midnight.
int timer_get_bios_ticks(void)
{
	static int have_base;
	static uint64_t base_ticks;		// PIT ticks from midnight to platform time 0

	if (!have_base) {
		time_t now = time(NULL);
		struct tm *lt = localtime(&now);
		uint64_t sec = lt ? (uint64_t) (lt->tm_hour * 3600 + lt->tm_min * 60 + lt->tm_sec) : 0;
		base_ticks = sec * TIMER_FREQUENCY - timer_us_to_ticks(plat_time_us());
		have_base = 1;
	}
	return (int) ((base_ticks + timer_us_to_ticks(plat_time_us())) >> 16);
}
