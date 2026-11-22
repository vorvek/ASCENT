/*
 * ASCENT: new file. The thief skills of Descent to Undermountain. See
 * dtuthief.h. It has no part in Descent 1.
 *
 * The rules are from DTUGAME.EXE version 1.3 (the code before thiefpts.c,
 * 0xB3038 to 0xB43B9, and pcons.c). docs/re/combat.md, section B7, has the
 * details. A comment that says "guess" marks a part that is not verified.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "error.h"
#include "game.h"
#include "gameseq.h"
#include "robot.h"
#include "textures.h"
#include "dtugame.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "fvi.h"
#include "wall.h"
#include "bm.h"
#include "text.h"
#include "gauges.h"
#include "digi.h"
#include "physics.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtutrig.h"
#include "dtuthief.h"

int Dtu_thief_bits = 0;

// The times of the 0.5 second limits (0x1466E0 locks, 0x1466E4 search,
// 0x146700 move silently), GameTime
static fix Lock_time = 0, Search_time = 0, Silent_time = 0;
// 0x14534A: Hide In Shadows may roll at the next stop
static int Hide_ready = 0;
// The byte 0x17 of the wall record (the pad of the Descent 1 structure):
// the thief level + 1 of a failed pick
static ubyte Lock_failed[MAX_WALLS];
// The thief state bytes of the trigger (+0x0F, +0x10) and trap (+0x03,
// +0x04) records: 0 nothing known, level + 1 a failed find, 0xFF found,
// 0xFE found and left alone; the second byte: level + 1 of a failed
// removal
static ubyte Trig_find[DTU_MAX_TRIGGERS], Trig_remove[DTU_MAX_TRIGGERS];
static ubyte Trap_find[DTU_MAX_TRAPS], Trap_remove[DTU_MAX_TRAPS];
static int Thief_level_num = -1;
static int trigger_harmful(int t);
// The climb: the point of the last roll (0x145353)
static vms_vector Climb_check;

static char *text(int n, char *def)
{
	if (n >= 0 && n < N_TEXT_STRINGS && Text_string[n] && Text_string[n][0])
		return Text_string[n];
	return def;
}

static void message(int n, char *def)
{
	HUD_init_message("%s", text(n, def));
	dtu_char_log("thief: message %d '%s'", n, text(n, def));
}

static int has_class(int c)
{
	return Dtu_char.created && (Dtu_char.pa.class1 == c || Dtu_char.pa.class2 == c);
}

// The thief level from 1 (0xB31BD: class 2 first)
static int thief_level(void)
{
	if (Dtu_char.pa.class2 == DTU_CLASS_THIEF)
		return Dtu_char.pa.class2_level + 1;
	return Dtu_char.pa.class1_level + 1;
}

static int half_second(fix *last)
{
	fix d = GameTime - *last;

	if (d < 0) d = -d;
	if (d < F1_0/2)
		return 0;
	*last = GameTime;
	return 1;
}

static void level_check(void)
{
	if (Thief_level_num == Current_level_num)
		return;
	Thief_level_num = Current_level_num;
	memset(Lock_failed, 0, sizeof(Lock_failed));
	memset(Trig_find, 0, sizeof(Trig_find));
	memset(Trig_remove, 0, sizeof(Trig_remove));
	memset(Trap_find, 0, sizeof(Trap_find));
	memset(Trap_remove, 0, sizeof(Trap_remove));
	Dtu_thief_bits &= ~(DTU_TS_HIDDEN|DTU_TS_HIDE_FAILED|DTU_TS_CLIMB_SKILL);
	// the log: the triggers that a thief can find
	{
		int t, n = 0;
		for (t = 0; t < Dtu_num_triggers && n < 40; t++)
			if (trigger_harmful(t)) {
				dtu_char_log("thief: trigger %d at seg %d side %d (%.1f %.1f %.1f) leads to a harmful trap", t, Dtu_triggers[t].seg,
					Dtu_triggers[t].side, f2fl(Dtu_triggers[t].pos.x), f2fl(Dtu_triggers[t].pos.y), f2fl(Dtu_triggers[t].pos.z));
				n++;
			}
	}
}

int dtu_thief_roll(int skill, int *roll)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	object *p = ConsoleObject;
	int v = 0, r = rand() % 100 + 1;
	static char *names[5] = { "Hide In Shadows", "Move Silently", "Open Locks", "Find/Remove Traps", "Climb Walls" };

	switch (skill) {
		case 0: v = pa->hide_in_shadows; break;
		case 1: v = pa->move_silently; break;
		case 2: v = pa->open_locks; break;
		case 3: v = (p && (p->dtu.m_88 & DTU_FX_FIND_TRAPS)) ? 100 : pa->find_traps; break;
		case 4: v = (p && (p->dtu.m_88 & (DTU_FX_LEVITATE|DTU_FX_SPIDER_CLIMB))) ? 100 : pa->climb_walls; break;
	}
	if (roll)
		*roll = r;
	dtu_char_log("thief: %s %d, d100 %d: %s", names[skill], v, r, v >= r ? "success" : "failure");
	return v >= r;
}

// 0xB30FD: a ray of "dist" from the player along its forward vector
static int front_ray(object *obj, int dist, fvi_info *hit)
{
	fvi_query fq;
	vms_vector end;

	vm_vec_scale_add(&end, &obj->pos, &obj->orient.fvec, i2f(dist));
	fq.p0 = &obj->pos;
	fq.p1 = &end;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	return find_vector_intersection(&fq, hit);
}

// 0xB0AC2: body armor of metal (lines 3, 4, 7 to 11). Returns the entry.
static dtu_inv_item *metal_armor(void)
{
	int n = Dtu_char.slot[DTU_SLOT_ARMOR].n;
	dtu_inv_item *e;

	if (n < 0 || n >= Dtu_n_inventory)
		return NULL;
	e = &Dtu_inventory[n];
	if (e->type == OBJ_ARMOR && (e->id == 3 || e->id == 4 || (e->id >= 7 && e->id <= 11)))
		return e;
	return NULL;
}

// 0xAC2F5: a creature watches: alive, not peaceful, active, and the
// player is not hidden from it. No distance and no line of sight.
static int creature_watches(void)
{
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];

		if (o->type != OBJ_ROBOT || (o->flags & OF_SHOULD_BE_DEAD) || o->shields < F1_0)
			continue;
		if (o->dtu.m_1b0 & DTU_PEACEFUL)
			continue;
		if (!dtu_creature_is_active(o))
			continue;
		if (dtu_thief_hidden_from(o))
			continue;
		return 1;
	}
	return 0;
}

//------------------------------------------------------------------------
// Open Locks (0xB31BD), the key O

void dtu_thief_open_locks(void)
{
	object *obj = ConsoleObject;
	fvi_info hit;
	segment *seg;
	wall *w;
	int wn, level = thief_level();

	if (!obj || obj->type != OBJ_PLAYER || !Dtu_char.created)
		return;
	level_check();
	front_ray(obj, 20, &hit);
	if (!half_second(&Lock_time))
		return;
	if (Dtu_char.pa.open_locks == 0) {
		message(931, "You cannot perform this action.");
		return;
	}
	if (hit.hit_side < 0 || hit.hit_seg < 0)
		return;
	seg = &Segments[hit.hit_seg];
	wn = seg->sides[hit.hit_side].wall_num;
	if (wn < 0)
		return;
	w = &Walls[wn];
	if (w->type != WALL_DOOR)
		return;
	if (!(w->flags & WALL_DOOR_LOCKED) || w->state == WALL_DOOR_OPENING) {
		message(936, "This door is not locked.");
		return;
	}
	digi_link_sound_to_pos(2500, hit.hit_seg, hit.hit_side, &hit.hit_pnt, 0, F1_0);
	if (!(w->keys & KEY_NONE)) {
		message(932, "Strong magic prevents you from picking this lock.");
		return;
	}
	if (Lock_failed[wn] == level + 1) {
		message(933, "You are not yet ready to pick this lock.");
		return;
	}
	dtu_char_log("thief: pick the lock of wall %d (seg %d side %d)", wn, hit.hit_seg, hit.hit_side);
	if (!dtu_thief_roll(2, NULL)) {
		Lock_failed[wn] = level + 1;
		message(934, "This lock is currently too difficult for you to pick.");
		return;
	}
	w->flags &= ~WALL_DOOR_LOCKED;
	Lock_failed[wn] = 0;
	wall_open_door(seg, hit.hit_side);
	message(935, "You have successfully picked this lock!");
}

//------------------------------------------------------------------------
// Find/Remove Traps (0xB3400, 0xB4040, 0xB365F, 0xB396C)

void dtu_thief_search_toggle(void)
{
	int i;

	if (!ConsoleObject || !Dtu_char.created)
		return;
	level_check();
	if (!half_second(&Search_time))
		return;
	// the triggers and traps that the thief left alone are asked again
	for (i = 0; i < DTU_MAX_TRIGGERS; i++)
		if (Trig_find[i] == 0xFE) Trig_find[i] = 0xFF;
	for (i = 0; i < DTU_MAX_TRAPS; i++)
		if (Trap_find[i] == 0xFE) Trap_find[i] = 0xFF;
	Dtu_thief_bits ^= DTU_TS_SEARCHING;
	if (Dtu_thief_bits & DTU_TS_SEARCHING)
		message(939, "Searching...");
	else
		message(938, "No longer searching.");
}

// 0xB34E0: a trap that hurts: on, and a door or wall flag (0x140), a
// weapon, an object, damage, or a creature
static int trap_harmful(int t, int dwarf_side_pad)
{
	dtu_trap *tp;

	(void) dwarf_side_pad;
	if (t < 0 || t >= Dtu_num_traps)
		return 0;
	tp = &Dtu_traps[t];
	if (!tp->on)
		return 0;
	return (tp->flags & 0x140) || tp->weapon_type >= 0 || tp->spawn_object >= 0 || tp->stat_delta < 0 || tp->robot_type >= 0;
}

// 0xB353A: a trigger that leads to a harmful trap
static int trigger_harmful(int t)
{
	dtu_trigger *tg = &Dtu_triggers[t];
	int k, n = 0;

	if (!tg->on)
		return 0;
	for (k = tg->first_trap; k >= 0 && k < Dtu_num_traps && n < DTU_TRIG_CHAIN_LIMIT; k = Dtu_traps[k].next_trap, n++)
		if (trap_harmful(k, 0))
			return 1;
	if (tg->enable_trap >= 0 && trap_harmful(tg->enable_trap, 0))
		return 1;
	return 0;
}

static void set_side_tmap2(int seg, int side, int tmap)
{
	if (tmap == -1 || seg < 0 || seg > Highest_segment_index || side < 0 || side > 5)
		return;
	Segments[seg].sides[side].tmap_num2 = tmap;
}

static void open_trap_door(dtu_trap *tp)
{
	if ((tp->flags & DTU_TPF_OPEN_DOOR) && tp->seg >= 0 && tp->seg <= Highest_segment_index && tp->side >= 0 && tp->side < 6)
		wall_open_door(&Segments[tp->seg], tp->side);
}

static void found_sound(int magical)
{
	object *p = ConsoleObject;

	digi_link_sound_to_pos(3001, p->segnum, 0, &p->pos, 0, F1_0);
	if (magical)
		digi_link_sound_to_pos(2130, p->segnum, 0, &p->pos, 0, F1_0);
}

static void search_triggers(int seg, int side, int spell)
{
	object *p = ConsoleObject;
	int t, n = 0, level = thief_level();

	if (seg < 0 || seg >= DTU_TRIG_SEG_TABLE)
		return;
	for (t = Dtu_trigger_seg_table[seg]; t >= 0 && t < Dtu_num_triggers && n < DTU_TRIG_CHAIN_LIMIT; t = Dtu_triggers[t].next_in_seg, n++) {
		dtu_trigger *tg = &Dtu_triggers[t];
		int roll;

		if (tg->side != side || !trigger_harmful(t))
			continue;
		if (vm_vec_dist(&tg->pos, &p->pos) > i2f(40))
			continue;
		if (Trig_find[t] == level + 1)
			continue;
		if (Trig_find[t] < 0x80) {
			dtu_char_log("thief: search trigger %d (seg %d side %d)", t, seg, side);
			if (!dtu_thief_roll(3, NULL)) {
				Trig_find[t] = level + 1;
				continue;
			}
			Trig_find[t] = 0xFF;
		}
		if (Trig_find[t] == 0xFE || Trig_remove[t] == level + 1)
			continue;
		found_sound(0);
		if (spell) {
			// 0xB3605: a marker for 5 seconds (not in the port), no removal
			HUD_init_message("You found a trigger!");
			dtu_char_log("thief: the spell finds trigger %d", t);
			Trig_find[t] = 0xFE;
			Trig_remove[t] = level + 1;
			return;
		}
		dtu_char_log("thief: found trigger %d", t);
		if (dtu_char_choice(text(940, "You found a trigger!\nWould you like to disable it?"), 2, text(32, "Yes"), text(33, "No")) != 0) {
			Trig_find[t] = 0xFE;
			continue;
		}
		if (!dtu_thief_roll(3, &roll)) {
			Trig_remove[t] = level + 1;
			if (roll >= 96) {
				message(942, "Ooops!!!");
				dtu_trig_fire(t, DTU_ACT_FORCE, p);
			}
			else
				message(941, "You failed to disable the trigger.");
			continue;
		}
		if (tg->first_trap >= 0 && tg->first_trap < Dtu_num_traps)
			open_trap_door(&Dtu_traps[tg->first_trap]);
		tg->on = 0;
		set_side_tmap2(tg->seg, tg->side, tg->tex_off);
		message(943, "You disabled the trigger.");
	}
}

static void search_traps(int seg, int side, int spell)
{
	object *p = ConsoleObject;
	dtu_player_attr *pa = &Dtu_char.pa;
	int t, level = thief_level();

	for (t = 0; t < Dtu_num_traps; t++) {
		dtu_trap *tp = &Dtu_traps[t];
		int magical, saved = pa->find_traps, roll, ok;

		if (tp->seg != seg || tp->side != side || !trap_harmful(t, 0))
			continue;
		if (vm_vec_dist(&tp->pos, &p->pos) > i2f(40))
			continue;
		if (Trap_find[t] == level + 1)
			continue;
		magical = tp->spawn_object >= 0 || tp->robot_type >= 0;
		// a magical trap halves the skill for the rolls (0xB39F2)
		if (magical)
			pa->find_traps >>= 1;
		if (Trap_find[t] < 0x80) {
			dtu_char_log("thief: search trap %d (seg %d side %d)%s", t, seg, side, magical ? ", magical" : "");
			ok = dtu_thief_roll(3, NULL);
			if (!ok) {
				Trap_find[t] = level + 1;
				pa->find_traps = saved;
				continue;
			}
			Trap_find[t] = 0xFF;
		}
		if (Trap_find[t] == 0xFE || Trap_remove[t] == level + 1) {
			pa->find_traps = saved;
			continue;
		}
		found_sound(magical);
		if (spell) {
			HUD_init_message(magical ? "You found a magical trap!" : "You found a trap!");
			dtu_char_log("thief: the spell finds trap %d", t);
			Trap_find[t] = 0xFE;
			Trap_remove[t] = level + 1;
			// 0xB396C returns here without the restore of the skill (a
			// defect of 1.3); the next dtu_char_update() restores it
			pa->find_traps = saved;
			return;
		}
		dtu_char_log("thief: found trap %d", t);
		if (dtu_char_choice(magical ? text(944, "You found a magical trap!\nWould you like to disable it?") :
				text(945, "You found a trap!\nWould you like to disable it?"), 2, text(32, "Yes"), text(33, "No")) != 0) {
			Trap_find[t] = 0xFE;
			pa->find_traps = saved;
			continue;
		}
		ok = dtu_thief_roll(3, &roll);
		pa->find_traps = saved;
		if (!ok) {
			Trap_remove[t] = level + 1;
			if (roll >= 96) {
				int k;
				message(942, "Ooops!!!");
				for (k = 0; k < Dtu_num_triggers; k++)
					if (Dtu_triggers[k].first_trap == t)
						dtu_trig_fire(k, DTU_ACT_FORCE, p);
			}
			else
				message(946, "You failed to disable the trap.");
			continue;
		}
		open_trap_door(tp);
		tp->on = 0;
		set_side_tmap2(tp->seg, tp->side, tp->tex_disabled);
		message(947, "You disabled the trap.");
	}
}

// 0xB4040
static void search_frame(void)
{
	object *p = ConsoleObject;
	fvi_info hit;
	int spell = (p->dtu.m_88 & DTU_FX_FIND_TRAPS) != 0, side, seg;
	int dwarf = (Dtu_char.pa.race == DTU_RACE_DWARF && !has_class(DTU_CLASS_THIEF) && !spell);

	front_ray(p, 40, &hit);
	seg = hit.hit_seg >= 0 ? hit.hit_seg : p->segnum;
	if (seg < 0 || seg > Highest_segment_index)
		return;
	for (side = 0; side < 6; side++) {
		if (vm_vec_dot(&Segments[seg].sides[side].normals[0], &p->orient.fvec) > F1_0/4)
			continue;
		if (Dtu_char.pa.find_traps != 0 || spell || dwarf) {
			search_triggers(seg, side, spell);
			search_traps(seg, side, spell);
		}
		// Without the spell: 0xB3ECE, the racial search for secret walls
		// (a d6 against 0x1466E8[race]): not in the port
	}
}

//------------------------------------------------------------------------
// Move Silently (0xB4196), the key M; Hide In Shadows (0xB333F)

void dtu_thief_move_silently(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int racial = (pa->race == DTU_RACE_ELF || pa->race == DTU_RACE_DARK_ELF || pa->race == DTU_RACE_HALFLING);
	int thief = has_class(DTU_CLASS_THIEF), ok;
	dtu_inv_item *armor;

	if (!ConsoleObject || !Dtu_char.created)
		return;
	if (!half_second(&Silent_time))
		return;
	if (!thief && pa->move_silently == 0 && !racial) {
		message(931, "You cannot perform this action.");
		Dtu_thief_bits &= ~(DTU_TS_SILENT|DTU_TS_SILENT_FAILED);
		return;
	}
	if (Dtu_thief_bits & (DTU_TS_SILENT|DTU_TS_SILENT_FAILED)) {
		Dtu_thief_bits &= ~(DTU_TS_SILENT|DTU_TS_SILENT_FAILED);
		message(950, "You are no longer moving silently.");
		return;
	}
	armor = metal_armor();
	if (thief || pa->move_silently) {
		if (armor) {
			HUD_init_message("You cannot perform this action while wearing %s.", dtu_tr_name(armor->name));
			return;
		}
		ok = dtu_thief_roll(1, NULL);
	}
	else {
		if (armor) {
			message(951, "You cannot move silently in metal armor.");
			return;
		}
		ok = 1;
	}
	if (ok && !creature_watches())
		Dtu_thief_bits |= DTU_TS_SILENT;
	else
		Dtu_thief_bits |= DTU_TS_SILENT_FAILED;
	dtu_char_log("thief: move silently %s", (Dtu_thief_bits & DTU_TS_SILENT) ? "on" : "failed (the player is not told)");
	message(952, "You are moving silently.");
}

// 0xB333F from 0x9DFDE: one roll when the player stops
static void hide_frame(void)
{
	object *p = ConsoleObject;
	fix speed = vm_vec_mag(&p->mtype.phys_info.velocity);

	// 0x427DF: moving or a climb, a jump or a fall ends the hiding
	if (Dtu_move_mode != 0 || (Dtu_player_state & (DTU_PS_CLIMBING|DTU_PS_JUMPING|DTU_PS_FALLING)))
		Dtu_thief_bits &= ~(DTU_TS_HIDDEN|DTU_TS_HIDE_FAILED);
	if (speed > 0x72) {
		Hide_ready = 1;
		return;
	}
	if (!Hide_ready || Dtu_char.pa.hide_in_shadows == 0)
		return;
	Hide_ready = 0;
	if (dtu_thief_roll(0, NULL) && !creature_watches())
		Dtu_thief_bits |= DTU_TS_HIDDEN;
	else
		Dtu_thief_bits |= DTU_TS_HIDE_FAILED;
	dtu_char_log("thief: hide in shadows %s", (Dtu_thief_bits & DTU_TS_HIDDEN) ? "made" : "failed");
}

void dtu_thief_attack(void)
{
	if (Dtu_thief_bits & (DTU_TS_HIDDEN|DTU_TS_HIDE_FAILED|DTU_TS_SEARCHING))
		dtu_char_log("thief: the attack ends hiding and searching");
	Dtu_thief_bits &= 0xEC;
}

void dtu_thief_frame(void)
{
	object *p = ConsoleObject;

	if (!p || p->type != OBJ_PLAYER || !Dtu_char.created || !Dtu_objects_enabled)
		return;
	level_check();
	if (!(Dtu_player_state & DTU_PS_CLIMBING))
		Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
	hide_frame();
	// game.c 0x438F3
	if ((Dtu_thief_bits & DTU_TS_SEARCHING) || (p->dtu.m_88 & DTU_FX_FIND_TRAPS))
		search_frame();
}

// 0x165C4 with 0x165F9: the player is hidden when invisible (effect bits
// 16 and 17, 0x165E6) or hidden in shadows. A blind creature (the flag
// blind or the effect Blindness, 0x1660B) and a creature with iq 130 and
// level 10 or more see the player.
int dtu_thief_hidden_from(object *c)
{
	int level;
	object *p = ConsoleObject;

	if (!c || c->type != OBJ_ROBOT || c->id >= N_robot_types)
		return 0;
	if (!(Dtu_thief_bits & DTU_TS_HIDDEN) && !(p && (p->dtu.m_88 & (DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE))))
		return 0;
	if ((Monster_info[c->id].flags & MONSTER_F_BLIND) || (c->dtu.m_88 & DTU_FX_BLIND))
		return 0;
	level = Monster_info[c->id].max_hit_points / 8;
	if (Monster_info[c->id].iq >= 130 && level >= 10)
		return 0;
	return 1;
}

// 0x142BF: the player is behind the creature and moves silently
int dtu_thief_unnoticed_by(object *c)
{
	vms_vector v;

	if (!(Dtu_thief_bits & DTU_TS_SILENT) || !c || !ConsoleObject)
		return 0;
	vm_vec_sub(&v, &ConsoleObject->pos, &c->pos);
	return vm_vec_dot(&c->orient.fvec, &v) <= 0;
}

//------------------------------------------------------------------------
// Backstab (0xB337E)

int dtu_thief_backstab_check(object *target, int weapon)
{
	dtu_item_data d;
	int n = Dtu_char.current.n;

	if (weapon < 0 || !Dtu_char.created || Dtu_char.pa.thief_ability == 0)
		return 0;
	if (!(Dtu_thief_bits & DTU_TS_SILENT))
		return 0;
	if (n < 0 || n >= Dtu_n_inventory || !dtu_item_get(Dtu_inventory[n].type, Dtu_inventory[n].id, &d) || !d.melee)
		return 0;
	if (!target || target->type != OBJ_ROBOT || target->id >= N_robot_types || !(Monster_info[target->id].flags & MONSTER_F_HUMANOID))
		return 0;
	if (creature_watches())
		return 0;
	dtu_char_log("thief: backstab on obj %d %s", (int)(target-Objects), dtu_object_name(target));
	return 1;
}

//------------------------------------------------------------------------
// Climb Walls (0x951DE, 0x2C144, pcons.c 0x9EBEB)

static int ladder_texture(void)
{
	static int cached = -2;
	int i;

	if (cached != -2)
		return cached;
	cached = -1;
	for (i = 0; i < NumTextures; i++)
		if (!strncasecmp(TmapInfo[i].filename, "ladder", 6) && (TmapInfo[i].filename[6] == 0 || TmapInfo[i].filename[6] == '.')) {
			cached = i;
			break;
		}
	return cached;
}

static int side_is_ladder(segment *segp, int side)
{
	int tmap = segp->sides[side].tmap_num2 & 0x3fff, ladder = ladder_texture();

	if (tmap == 0)
		tmap = segp->sides[side].tmap_num;
	return ladder >= 0 && tmap >= ladder && tmap <= ladder + 3;
}

// The tests of 0x951DE and 0x9EBEB: a solid side (or a closed wall), that
// faces the player (dot of the normal and the forward vector at most
// 0.25), nearer than maxdist. Returns the distance to the plane, or -1.
static fix wall_distance(object *obj, segment *segp, int side, fix maxdist)
{
	vms_vector c;
	fix d;

	if (segp->children[side] != -1) {
		int wn = segp->sides[side].wall_num;

		if (wn < 0 || Walls[wn].type != WALL_CLOSED)
			return -1;
	}
	if (vm_vec_dot(&obj->orient.fvec, &segp->sides[side].normals[0]) > F1_0/4)
		return -1;
	compute_center_point_on_side(&c, segp, side);
	vm_vec_sub2(&c, &obj->pos);
	d = abs(vm_vec_dot(&c, &segp->sides[side].normals[0]));
	return d < maxdist ? d : -1;
}

static int hand_is_weapon(void)
{
	int n = Dtu_char.current.n;

	return n >= 0 && n < Dtu_n_inventory && Dtu_inventory[n].type == OBJ_WEAPON;
}

static int can_climb_by_skill(object *obj)
{
	return has_class(DTU_CLASS_THIEF) || Dtu_char.pa.climb_walls != 0 || (obj->dtu.m_88 & (DTU_FX_LEVITATE|DTU_FX_SPIDER_CLIMB));
}

void dtu_thief_climb_start(object *obj, int segnum, int side)
{
	segment *segp;
	dtu_inv_item *armor;

	if (!obj || obj != ConsoleObject || !Dtu_char.created)
		return;
	if (Dtu_player_state & DTU_PS_CLIMBING)
		return;
	if (segnum < 0 || segnum > Highest_segment_index || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return;
	segp = &Segments[segnum];
	if (wall_distance(obj, segp, side, i2f(10)) < 0)
		return;
	if (side_is_ladder(segp, side)) {
		Dtu_player_state |= DTU_PS_CLIMBING;
		Climb_check = obj->pos;
		dtu_char_log("climb: the player holds the ladder of seg %d side %d", segnum, side);
		return;
	}
	if (segp->children[side] != -1)
		return;
	if (!can_climb_by_skill(obj)) {
		message(931, "You cannot perform this action.");
		return;
	}
	if ((armor = metal_armor()) != NULL) {
		HUD_init_message("You cannot perform this action while wearing %s.", dtu_tr_name(armor->name));
		return;
	}
	if (dtu_thief_roll(4, NULL) && !hand_is_weapon()) {
		Dtu_player_state |= DTU_PS_CLIMBING;
		Dtu_thief_bits |= DTU_TS_CLIMB_SKILL;
		Climb_check = obj->pos;
		dtu_char_log("climb: the player climbs the wall of seg %d side %d at height %.2f", segnum, side, f2fl(obj->pos.y));
	}
	else {
		message(823, "You cannot get a good grip.");
		if (hand_is_weapon())
			dtu_char_log("climb: a weapon in the hand");
	}
}

void dtu_thief_climb_thrust(object *obj, fix forward, fix side)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector up = { 0, F1_0, 0 }, h, dir;
	int shift = (Dtu_thief_bits & DTU_TS_CLIMB_SKILL) ? 3 : 1;

	// 0x2C144: dir = normalize(normalize(cross(rvec, up)) + up) for a
	// forward thrust, normalize(... - up) for a backward thrust: forward
	// goes up and into the wall, back goes down and into the wall
	if (forward) {
		vm_vec_crossprod(&h, &obj->orient.rvec, &up);
		vm_vec_normalize(&h);
		if (forward > 0)
			vm_vec_add(&dir, &h, &up);
		else
			vm_vec_sub(&dir, &h, &up);
		vm_vec_normalize(&dir);
		vm_vec_scale_add2(&pi->thrust, &dir, abs(forward) >> shift);
	}
	if (side) {
		h = obj->orient.rvec;
		h.y = 0;
		vm_vec_normalize(&h);
		vm_vec_scale_add2(&pi->thrust, &h, side >> shift);
	}
}

// ASCENT: for Dtu_improved_physics, see dtuthief.h
int dtu_thief_ladder_side(object *obj, int segnum)
{
	static const int sides[4] = { 0, 2, 4, 5 };
	int i;

	if (segnum < 0 || segnum > Highest_segment_index)
		return -1;
	for (i = 0; i < 4; i++)
		if (wall_distance(obj, &Segments[segnum], sides[i], i2f(10)) >= 0 && side_is_ladder(&Segments[segnum], sides[i]))
			return sides[i];
	return -1;
}

// pcons.c 0x9EBEB(obj, 10.0)
void dtu_thief_climb_frame(object *obj, vms_vector *accel)
{
	physics_info *pi = &obj->mtype.phys_info;
	segment *segp = &Segments[obj->segnum];
	static const int sides[4] = { 0, 2, 4, 5 };
	int i, side = -1;
	fix height = dtu_get_floor_dist(obj);
	vms_vector *n;

	// ASCENT: 0x9EBEB goes on to the next side when a wall in reach is no
	// ladder and the player cannot climb it (0x9EDE6 to 0x9EE24 jump to
	// 0x9EF8B). A side wall of a ladder shaft no longer ends the climb.
	for (i = 0; i < 4; i++) {
		fix dd = wall_distance(obj, segp, sides[i], i2f(10));
		if (dd < 0)
			continue;
		// nearer than 4.0: out of the wall
		if (dd < i2f(4))
			vm_vec_scale_add2(&obj->pos, &segp->sides[sides[i]].normals[0], i2f(4) - dd);
		if ((Dtu_player_state & DTU_PS_JUMPING) || side_is_ladder(segp, sides[i]) ||
			(can_climb_by_skill(obj) && !hand_is_weapon() && !(segp->sides[sides[i]].pad & 1))) {
			side = sides[i];
			break;
		}
	}
	if (side < 0) {
		// no wall: a push away from it when high, then the fall
		if (height > i2f(3)) {
			vms_vector v;
			vm_vec_add(&v, &obj->orient.uvec, &obj->orient.fvec);
			vm_vec_scale_add2(&pi->velocity, &v, i2f(35));
			dtu_char_log("climb: PCONS: push!");
		}
		Dtu_player_state &= ~DTU_PS_CLIMBING;
		Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
		dtu_char_log("climb: PCONS: you fell! at height %.2f above the floor", f2fl(height));
		return;
	}
	n = &segp->sides[side].normals[0];
	if (Dtu_player_state & DTU_PS_JUMPING) {
		// 0x9F028: the jump lets go
		Dtu_player_state &= ~DTU_PS_CLIMBING;
		Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
		if (height > i2f(3))
			vm_vec_scale_add2(accel, n, fixdiv(i2f(150), pi->mass));
		return;
	}
	if (side_is_ladder(segp, side)) {
		Dtu_player_state &= ~DTU_PS_FALLING;
		return;
	}
	pi->thrust.x = fixmul(pi->thrust.x, 0x6666);
	pi->thrust.z = fixmul(pi->thrust.z, 0x6666);
	// each 20.0 of the climb: a new roll when higher than 3.0
	if (vm_vec_dist(&obj->pos, &Climb_check) > i2f(20)) {
		Climb_check = obj->pos;
		dtu_char_log("climb: 20.0 climbed, height %.2f above the floor, y %.2f", f2fl(height), f2fl(obj->pos.y));
		if (height > i2f(3) && !dtu_thief_roll(4, NULL)) {
			message(953, "You just slipped!");
			Dtu_player_state |= DTU_PS_FALLING;
			Dtu_player_state &= ~DTU_PS_CLIMBING;
			Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
			vm_vec_scale_add2(accel, n, fixdiv(i2f(50), pi->mass));
		}
	}
}
