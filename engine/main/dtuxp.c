/*
 * ASCENT: new file. The experience of the player: the kill line, the line
 * for a death with no experience, the next level window and the session log
 * lines. See dtuxp.h.
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "object.h"
#include "segment.h"
#include "robot.h"
#include "fvi.h"
#include "vecmat.h"
#include "gameseg.h"
#include "gauges.h"
#include "text.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtulog.h"
#include "dtulang.h"
#include "dtuxp.h"

int Dtu_kill_xp_message = 1;
int Dtu_max_hp_level_up = 0;

static const char *Pending_cause;

void dtu_xp_cause(const char *cause)
{
	Pending_cause = cause;
}

// The player can see the creature: a free ray from the eye of the player
// to the creature (no objects tested), within 200 units. The ray of the
// creature to the player (dtu_brain_sees_player) starts at the floor of the
// body and often ends in a wall.
static int player_sees(object *obj)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector eye;

	if (!ConsoleObject || ConsoleObject->type != OBJ_PLAYER || obj->segnum < 0 || ConsoleObject->segnum < 0)
		return 0;
	if (vm_vec_dist_quick(&obj->pos, &ConsoleObject->pos) > i2f(200))
		return 0;
	eye = ConsoleObject->pos;
	fq.p0 = &eye;
	fq.p1 = &obj->pos;
	fq.startseg = ConsoleObject->segnum;
	fq.rad = 0;
	fq.thisobjnum = (short) (ConsoleObject - Objects);
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	// a point that the segment of the object does not hold (the place of a
	// test command): the segment that holds it, else the same segment
	fq.startseg = find_point_seg(&eye, ConsoleObject->segnum);
	if (fq.startseg < 0)
		return obj->segnum == ConsoleObject->segnum;
	return find_vector_intersection(&fq, &hit) == HIT_NONE || dtu_brain_sees_player(obj);
}

static const char *cause_text(object *attacker, int weapon, char *buf, int size)
{
	if (Pending_cause)
		return Pending_cause;
	if (weapon >= 0) {
		snprintf(buf, size, "weapon %d", weapon);
		return buf;
	}
	if (weapon == -1 && attacker)
		return "natural weapon";
	return attacker ? "spell or direct damage" : "no attacker (spell, trap or script)";
}

// A line of the message log in the language, also in the -charlog file
// (the tests read it there)
static void say(const char *format, ...)
{
	char text[200];
	va_list ap;

	va_start(ap, format);
	vsnprintf(text, sizeof(text), dtu_tr(format), ap);
	va_end(ap);
	HUD_init_message("%s", text);
	dtu_char_log("message: %s", text);
}

void dtu_xp_creature_death(object *obj, object *attacker, int weapon, int xp)
{
	char buf[32], who[64];
	const char *cause = cause_text(attacker, weapon, buf, sizeof(buf));
	int two = Dtu_char.pa.class2 >= 0 && Dtu_char.pa.class2 <= 3;

	if (attacker && attacker->type == OBJ_PLAYER)
		snprintf(who, sizeof(who), "player");
	else if (attacker)
		snprintf(who, sizeof(who), "obj %d %s", (int) (attacker - Objects), dtu_object_name(attacker));
	else
		snprintf(who, sizeof(who), "none");
	{
		// the numbers of the last roll, if the attack code reported them
		const char *rt = dtu_xp_roll_text();

		dtulog_event("Creature death: obj %d %s, cause %s, attacker %s, experience %d, segment %d%s%s",
			(int) (obj - Objects), dtu_object_name(obj), cause, who, xp > 0 ? xp : 0, obj->segnum, rt[0] ? ", " : "", rt);
	}
	Pending_cause = NULL;
	dtu_char_log("death: obj %d, attacker %s, xp %d, seen by the player %d", (int) (obj - Objects), who, xp, player_sees(obj));
	// the roll line before the kill line
	dtu_xp_roll_say(attacker, obj);

	// The message log. 1.3: "You killed X." for the player (TEX 831 with
	// the name, 0x2A0C7 and 0x915F6), nothing for another cause.
	if (xp >= 0) {
		char *name = dtu_object_display_name(obj);

		if (!Dtu_kill_xp_message)
			say(LN("You killed %s."), name);
		else if (xp == 0)
			say(LN("You killed %s: no experience."), name);
		else if (two)
			say(LN("You killed %s: %d Exp (%d per class)."), name, xp, xp / 2 > 0 ? xp / 2 : 1);
		else
			say(LN("You killed %s: %d Exp."), name, xp);
	}
	else if (Dtu_kill_xp_message && player_sees(obj)) {
		char name[64];

		snprintf(name, sizeof(name), "%s", dtu_object_display_name(obj));
		if (attacker && attacker->type == OBJ_ROBOT && attacker != obj)
			say(LN("%s killed %s: no experience."), dtu_object_display_name(attacker), name);
		else
			say(LN("%s died: no experience."), name);
	}
}

void dtu_xp_log_award(int points, int each, int xp1, int xp2)
{
	dtulog_event("Experience: +%d, +%d for each class, now %d / %d", points, each, xp1, xp2);
}

void dtu_xp_log_level(int old1, int new1, int old2, int new2, int hp)
{
	dtulog_event("Level gained: class levels %d / %d -> %d / %d, %d hit points%s", old1, old2, new1, new2, hp,
		Dtu_max_hp_level_up ? " (MaxHpOnLevelUp)" : "");
}

//------------------------------------------------------------------------
// The next level window

static char *tex(int n, char *def)
{
	if (n >= 0 && n < N_TEXT_STRINGS && Text_string[n] && Text_string[n][0])
		return Text_string[n];
	return def;
}

static void add(char *out, int size, const char *format, ...)
{
	int n = (int) strlen(out);
	va_list ap;

	if (n >= size - 2)
		return;
	if (n)
		out[n++] = '\n';
	va_start(ap, format);
	vsnprintf(out + n, size - n, format, ap);
	va_end(ap);
}

int dtu_xp_next_level_text(char *out, int size)
{
	// the class names of the statistics screen (TEX 642, 643, 644, 647)
	static char *class_def[4] = { "Cleric", "Mage", "Fighter", "Thief" };
	static short class_text[4] = { 642, 643, 644, 647 };
	int k, n = 1, classes = 0;

	out[0] = 0;
	add(out, size, "%s", LT("Experience"));
	for (k = 0; k < 2; k++) {
		int cls, level, xp, next;

		if (!dtu_char_class_xp(k, &cls, &level, &xp, &next))
			continue;
		classes++;
		if (next < 0)
			add(out, size, LT("%s, level %d: %d (the highest level)"), tex(class_text[cls], class_def[cls]), level, xp);
		else
			add(out, size, LT("%s, level %d: %d of %d"), tex(class_text[cls], class_def[cls]), level, xp, next);
		n++;
	}
	if (classes == 2) {
		add(out, size, "%s", LT("Each class gets one half of each award."));
		n++;
	}
	dtu_lang_sex_text(out);
	return n;
}

//------------------------------------------------------------------------
// The verbose combat log (Dtu_verbose_combat). The attack code reports the
// numbers where it computes them; dtu_xp_roll_say() writes one line.

int Dtu_verbose_combat = 0;

static struct {
	int hit;					// 0: no attack roll, 1: a roll
	int d20, total, need, result;
	char hit_mods[96];
	int dmg;					// 0: no damage roll
	int dmg_total;
	char dmg_text[128];
	int save;				// 0: no save, 1: a save
	int save_d20, save_need, save_made;
} Roll;

static void cat(char *dst, int size, const char *format, ...)
{
	int n = (int) strlen(dst);
	va_list ap;

	if (n >= size - 1)
		return;
	va_start(ap, format);
	vsnprintf(dst + n, size - n, format, ap);
	va_end(ap);
}

void dtu_xp_roll_clear(void)
{
	memset(&Roll, 0, sizeof(Roll));
}

void dtu_xp_hit_start(int d20)
{
	dtu_xp_roll_clear();
	Roll.hit = 1;
	Roll.d20 = d20;
	Roll.result = -2;
}

void dtu_xp_hit_mod(int v, const char *tag)
{
	if (!Roll.hit || v == 0)
		return;
	cat(Roll.hit_mods, sizeof(Roll.hit_mods), " %+d", v);
	if (tag)
		cat(Roll.hit_mods, sizeof(Roll.hit_mods), " %s", dtu_tr(tag));
}

void dtu_xp_hit_end(int total, int need, int result)
{
	if (!Roll.hit)
		return;
	Roll.total = total;
	Roll.need = need;
	Roll.result = result;
}

void dtu_xp_dmg_start(int n, int sides, int sum)
{
	if (Roll.dmg)
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " +");
	Roll.dmg = 1;
	if (n > 0 && sides > 0)
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " %dd%d %d", n, sides, sum);
	else
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " %d", sum);
}

void dtu_xp_dmg_mod(int v, const char *tag)
{
	if (!Roll.dmg || v == 0)
		return;
	cat(Roll.dmg_text, sizeof(Roll.dmg_text), " %+d", v);
	if (tag)
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " %s", dtu_tr(tag));
}

void dtu_xp_dmg_mult(int m, const char *tag)
{
	if (!Roll.dmg || m == 1)
		return;
	if (m < 0)
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " /%d", -m);		// a division (a save for half)
	else
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " x%d", m);
	if (tag)
		cat(Roll.dmg_text, sizeof(Roll.dmg_text), " %s", dtu_tr(tag));
}

void dtu_xp_dmg_end(int total)
{
	if (Roll.dmg)
		Roll.dmg_total = total;
}

void dtu_xp_save(int d20, int need, int made)
{
	Roll.save = 1;
	Roll.save_d20 = d20;
	Roll.save_need = need;
	Roll.save_made = made;
}

// The text of the numbers, in the language
static int roll_text(char *out, int size)
{
	out[0] = 0;
	if (Roll.hit && Roll.result == 2)
		cat(out, size, LT("Roll d20 %d: critical hit."), Roll.d20);
	else if (Roll.hit && Roll.result == -1)
		cat(out, size, LT("Roll d20 %d: fumble."), Roll.d20);
	else if (Roll.hit && Roll.result == 1)
		cat(out, size, LT("Roll d20 %d%s = %d vs %d: hit."), Roll.d20, Roll.hit_mods, Roll.total, Roll.need);
	else if (Roll.hit && Roll.result == 3)
		cat(out, size, LT("Roll d20 %d%s = %d vs %d: hit, Stoneskin takes it."), Roll.d20, Roll.hit_mods, Roll.total, Roll.need);
	else if (Roll.hit && Roll.result == 0)
		cat(out, size, LT("Roll d20 %d%s = %d vs %d: miss."), Roll.d20, Roll.hit_mods, Roll.total, Roll.need);
	if (Roll.save) {
		if (out[0])
			cat(out, size, " ");
		if (Roll.save_made)
			cat(out, size, LT("Save d20 %d vs %d: made."), Roll.save_d20, Roll.save_need);
		else
			cat(out, size, LT("Save d20 %d vs %d: failed."), Roll.save_d20, Roll.save_need);
	}
	if (Roll.dmg) {
		if (out[0])
			cat(out, size, " ");
		cat(out, size, LT("Damage%s = %d."), Roll.dmg_text, Roll.dmg_total);
	}
	return out[0] != 0;
}

const char *dtu_xp_roll_text(void)
{
	static char text[256];

	roll_text(text, sizeof(text));
	return text;
}

void dtu_xp_roll_say(object *a, object *b)
{
	char text[256];

	if (Dtu_verbose_combat && ConsoleObject && (a == ConsoleObject || b == ConsoleObject) && roll_text(text, sizeof(text))) {
		HUD_init_message("%s", text);
		dtu_char_log("message: %s", text);
	}
	dtu_xp_roll_clear();
}
