/*
 * ASCENT: the attack timing rules of the player. See dtuatk.h and
 * docs/re/combat.md, section A8.
 */

#include <stdint.h>

#include "dtuatk.h"

fix dtu_atk_state_time(int melee,fix fire_wait,fix model_time,int effects,int fix_haste)
{
	fix t;

	if (melee) {
		// 0x44221: a time of 1.0 or less becomes 1.0
		t = fire_wait > F1_0 ? fire_wait : F1_0;
		if (effects & DTU_ATK_SLOW)
			t *= 2;
		if (fix_haste && (effects & DTU_ATK_HASTE))
			t /= 2;
		return t;
	}
	t = model_time;
	if (effects & DTU_ATK_SLOW)
		t *= 2;
	return t;
}

fix dtu_atk_melee_wait(fix state_time,int effects,int fix_haste)
{
	// 0x41ECC: last = GameTime + state_time / 2, and the gate compares
	// |GameTime - last| with the state time
	if (!fix_haste && (effects & DTU_ATK_HASTE))
		return state_time + state_time / 2;
	return state_time;
}

fix dtu_atk_missile_wait(fix fire_wait,int effects,int fix_rate,int fix_haste)
{
	fix t;

	if (!fix_rate)
		return 0;
	t = fire_wait > 0 ? fire_wait : F1_0;
	if (effects & DTU_ATK_SLOW)
		t *= 2;
	if (fix_haste && (effects & DTU_ATK_HASTE))
		t /= 2;
	return t;
}

fix dtu_atk_fire_lead(fix state_time,int first,int end,int fire_frame)
{
	int n = end - first, k = 2 * (fire_frame - first) - 1;

	// 0x44589: frame = first + (elapsed * n / state_time + 0.5), so the
	// frame reaches fire_frame at elapsed = (fire_frame - first - 0.5) *
	// state_time / n
	if (n <= 0 || k <= 0 || state_time <= 0)
		return 0;
	if (k > 2 * n)
		k = 2 * n;
	return (fix) (((int64_t) state_time * k) / (2 * n));
}

int dtu_atk_state_animates(fix state_time,int attack_state,int fix_haste)
{
	if (f2i(state_time) != 0)
		return 1;
	return fix_haste && attack_state && state_time > 0;
}

int dtu_atk_item_fast(int pc_spell,int fix_items)
{
	return fix_items && pc_spell == 21;
}

int dtu_atk_rate_effects(int effects,int item_fast)
{
	// the Haste bit is one bit: a second source of speed adds nothing
	return item_fast ? effects | DTU_ATK_HASTE : effects;
}

int dtu_atk_rate_fix_haste(int effects,int item_fast,int fix_haste)
{
	return fix_haste || (item_fast && !(effects & DTU_ATK_HASTE));
}
