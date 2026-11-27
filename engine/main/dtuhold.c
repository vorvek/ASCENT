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
#include "gr.h"
#include "gamefont.h"

int Dtu_fix_player_hold = 1;
int Dtu_hold_view = 1;
int Dtu_sleep_view = 1;
int Dtu_fix_sleep_wake = 1;

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
		Dtu_fix_player_hold = Dtu_hold_view = Dtu_sleep_view = Dtu_fix_sleep_wake = 0;
	if (FindArg("-noholdview"))
		Dtu_hold_view = 0;
	if (FindArg("-nosleepview"))
		Dtu_sleep_view = 0;
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

static int player_asleep(void)
{
	return ConsoleObject && ConsoleObject->type == OBJ_PLAYER && !Player_is_dead && !Dtu_player_dead &&
		(ConsoleObject->dtu.m_88 & DTU_FX_SLEEP);
}

int dtu_sleep_blocks_player(void)
{
	hold_init();
	return Dtu_sleep_view && player_asleep();
}

int dtu_hold_blocks_player(void)
{
	static int was_held;
	int held;

	hold_init();
	held = (Dtu_fix_player_hold && player_held()) || dtu_sleep_blocks_player();
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
		if (dtu_sleep_blocks_player())
			HUD_init_message("You are asleep and cannot act.");
		else
			HUD_init_message("You are held and cannot act.");
	}
	return 1;
}

int dtu_hold_refuse_item(int type, int id)
{
	if (type == OBJ_MISC && id == FREEDOM_ID && dtu_hold_blocks_player() && !dtu_sleep_blocks_player()) {
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
		if (end != DTU_FX_NO_END && dtu_time_left(end) < VIEW_FADE)
			target = dtu_time_left(end) > 0 ? fixdiv(dtu_time_left(end),VIEW_FADE) : 0;
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

int dtu_sleep_view_on(void)
{
	return dtu_sleep_blocks_player() && Viewer == ConsoleObject;
}

void dtu_sleep_view_draw(void)
{
	grs_canvas *cv = grd_curcanv;
	// "z", "zz", "zzz", one step each half second of real time
	static char *text[3] = { "z", "zz", "zzz" };
	char *s = text[(timer_get_fixed_seconds() / (F1_0/2)) % 3];
	int h, aw, full;

	grs_font *font = Gamefonts[GFONT_MEDIUM_1] ? Gamefonts[GFONT_MEDIUM_1] : GAME_FONT;

	gr_clear_canvas(BM_XRGB(0,0,0));
	if (!font)
		return;
	gr_set_curfont(font);
	gr_set_fontcolor(gr_find_closest_color(56,56,56),-1);
	// the place of the widest text, so that the z do not move
	gr_get_string_size(text[2],&full,&h,&aw);
	gr_string((cv->cv_bitmap.bm_w - full) / 2,(cv->cv_bitmap.bm_h - h) / 2,s);
}

void dtu_sleep_player_hurt(object *player)
{
	hold_init();
	if (!Dtu_fix_sleep_wake || player == NULL || player->type != OBJ_PLAYER || !(player->dtu.m_88 & DTU_FX_SLEEP))
		return;
	dtulog_event("sleep: damage wakes the player (Dtu_fix_sleep_wake)");
	dtu_spell_wake(player);
}
