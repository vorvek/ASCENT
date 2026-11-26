/*
 * ASCENT: the player under Hold. New file, see dtuhold.h.
 */
#include <stdlib.h>

#include "types.h"
#include "fix.h"
#include "timer.h"
#include "args.h"
#include "object.h"
#include "game.h"
#include "gauges.h"
#include "dtuobj.h"
#include "dtuspell.h"
#include "dtulog.h"
#include "dtulang.h"
#include "dtuhold.h"

int Dtu_fix_player_hold = 1;
int Dtu_hold_view = 1;

#define HOLD_MIN		6			// seconds
#define HOLD_MAX		18
#define VIEW_FADE		(F1_0/2)	// the time of the fade in and of the fade out
#define FREEDOM_ID	78			// misc 78, the Potion of Freedom (0x37A6A case 0x4E)

static int Init_done;

static void hold_init(void)
{
	if (Init_done)
		return;
	Init_done = 1;
	if (FindArg("-origcombat"))
		Dtu_fix_player_hold = Dtu_hold_view = 0;
	if (FindArg("-noholdview"))
		Dtu_hold_view = 0;
}

int dtu_hold_fix_on(void)
{
	hold_init();
	return Dtu_fix_player_hold;
}

fix dtu_hold_player_time(void)
{
	fix d = i2f(HOLD_MIN + rand() % (HOLD_MAX - HOLD_MIN + 1));

	dtulog_event("hold: the player is held for %d seconds (Dtu_fix_player_hold)", f2i(d));
	return d;
}

static int player_held(void)
{
	return ConsoleObject && ConsoleObject->type == OBJ_PLAYER && !Player_is_dead && !Dtu_player_dead &&
		(ConsoleObject->dtu.m_88 & DTU_FX_HOLD);
}

int dtu_hold_blocks_player(void)
{
	static int was_held;
	int held;

	hold_init();
	held = Dtu_fix_player_hold && player_held();
	// The start of the hold: a cast that runs is lost, as a hit stops it
	// (AD&D 2nd edition: a held caster cannot finish the spell)
	if (held && !was_held)
		dtu_spell_interrupt(ConsoleObject,1);
	was_held = held;
	return held;
}

int dtu_hold_refuse(void)
{
	static fix last = 0;
	fix now;

	if (!dtu_hold_blocks_player())
		return 0;
	now = timer_get_fixed_seconds();
	if (now - last > F1_0 || now < last) {
		last = now;
		HUD_init_message("You are held and cannot act.");
	}
	return 1;
}

int dtu_hold_refuse_item(int type, int id)
{
	if (type == OBJ_MISC && id == FREEDOM_ID && dtu_hold_blocks_player()) {
		dtulog_event("hold: the Potion of Freedom is used while held");
		return 0;
	}
	return dtu_hold_refuse();
}

void dtu_hold_free_player(void)
{
	if (!dtu_hold_fix_on() || !player_held() || (ConsoleObject->dtu.m_88 & DTU_FX_FEAR))
		return;
	dtu_spell_clear_effect(ConsoleObject,DTU_FX_HOLD);
	HUD_init_message("%s expired.",dtu_tr("Hold Person"));
	dtulog_event("hold: the Potion of Freedom ends the hold of the player");
}

float dtu_hold_view_amount(void)
{
	static fix amount = 0, last = 0;
	fix now = timer_get_fixed_seconds(), dt = now - last, target = 0;

	hold_init();
	last = now;
	if (Dtu_hold_view && Dtu_fix_player_hold && player_held() && !(ConsoleObject->dtu.m_88 & DTU_FX_FEAR)) {
		fix end = ConsoleObject->dtu.m_c0[0];		// the timer of Hold (0xC0)

		target = F1_0;
		// the fade out ends with the hold
		if (end != DTU_FX_NO_END && end - GameTime < VIEW_FADE)
			target = end > GameTime ? fixdiv(end - GameTime,VIEW_FADE) : 0;
	}
	// A long time with no view (a load, a level change, a menu): no fade
	if (dt < 0 || dt > F1_0/4)
		amount = target;
	else if (amount < target) {
		amount += fixdiv(dt,VIEW_FADE);
		if (amount > target) amount = target;
	}
	else if (amount > target) {
		amount -= fixdiv(dt,VIEW_FADE);
		if (amount < target) amount = target;
	}
	return f2fl(amount);
}
