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
#include "args.h"
#include "dtufile.h"		// ASCENT: dtu_log()

int Dtu_thief_bits = 0;

// The times of the 0.5 second limits (0x1466E0 locks, 0x1466E4 search,
// 0x146700 move silently), GameTime
static fix Lock_time = 0, Search_time = 0, Silent_time = 0;
// 0x14534A: Hide In Shadows may roll at the next stop
static int Hide_ready = 0;
// ASCENT: the thief state lives in the records of the level, as in the
// executable, so the level state and the save games keep it:
// - the wall record (the pad of the Descent 1 structure, `wall.pad`):
//   byte 0x16 (low byte) is 1 when the racial search (0xB3ECE) has asked
//   the wall, byte 0x17 (high byte) is the thief level + 1 of a failed
//   pick. The fresh branch of level_start (0xAD67B) clears both bytes;
//   the port does it in dtu_thief_level_loaded().
// - the trigger record bytes +0x0F and +0x10 (`unk0f`) and the trap record
//   bytes +0x03 and +0x04 (the raw record of dtutrig.c): 0 nothing known,
//   level + 1 a failed find, 0xFF found, 0xFE found and left alone; the
//   second byte: level + 1 of a failed removal. The readers of the level
//   files clear them (0xAD6A8, 0xAD6D4 for a fresh level).
static int wall_searched(int wn) { return Walls[wn].pad & 0xFF; }
static void set_wall_searched(int wn) { Walls[wn].pad = (short) ((Walls[wn].pad & 0xFF00) | 1); }
static int lock_failed(int wn) { return (Walls[wn].pad >> 8) & 0xFF; }
static void set_lock_failed(int wn, int v) { Walls[wn].pad = (short) ((Walls[wn].pad & 0xFF) | ((v & 0xFF) << 8)); }
static int trig_byte(int t, int n) { return (Dtu_triggers[t].unk0f >> (8 * n)) & 0xFF; }
static void set_trig_byte(int t, int n, int v)
{
	Dtu_triggers[t].unk0f = (Dtu_triggers[t].unk0f & ~(0xFFu << (8 * n))) | ((uint) (v & 0xFF) << (8 * n));
}
static int trap_byte(int t, int n) { ubyte *r = dtu_trig_trap_raw(t); return r ? r[3 + n] : 0; }
static void set_trap_byte(int t, int n, int v) { ubyte *r = dtu_trig_trap_raw(t); if (r) r[3 + n] = (ubyte) v; }
#define Trig_find(t)				trig_byte(t, 0)
#define Trig_remove(t)				trig_byte(t, 1)
#define Set_trig_find(t, v)		set_trig_byte(t, 0, v)
#define Set_trig_remove(t, v)		set_trig_byte(t, 1, v)
#define Trap_find(t)				trap_byte(t, 0)
#define Trap_remove(t)				trap_byte(t, 1)
#define Set_trap_find(t, v)		set_trap_byte(t, 0, v)
#define Set_trap_remove(t, v)		set_trap_byte(t, 1, v)
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
	Dtu_thief_bits &=~(DTU_TS_HIDDEN|DTU_TS_HIDE_FAILED|DTU_TS_CLIMB_SKILL);
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
	if (lock_failed(wn) == ((level + 1) & 0xFF)) {
		message(933, "You are not yet ready to pick this lock.");
		return;
	}
	dtu_char_log("thief: pick the lock of wall %d (seg %d side %d)", wn, hit.hit_seg, hit.hit_side);
	if (!dtu_thief_roll(2, NULL)) {
		set_lock_failed(wn, level + 1);
		message(934, "This lock is currently too difficult for you to pick.");
		return;
	}
	w->flags &= ~WALL_DOOR_LOCKED;
	set_lock_failed(wn, 0);
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
	for (i = 0; i < Dtu_num_triggers; i++)
		if (Trig_find(i) == 0xFE) Set_trig_find(i, 0xFF);
	for (i = 0; i < Dtu_num_traps; i++)
		if (Trap_find(i) == 0xFE) Set_trap_find(i, 0xFF);
	Dtu_thief_bits ^= DTU_TS_SEARCHING;
	if (Dtu_thief_bits & DTU_TS_SEARCHING)
		message(939, "Searching...");
	else
		message(938, "No longer searching.");
}

// The search of a dwarf who is no thief and has no Find Traps spell
// (0xB40D4) skips a side with side flag & 3 (0xB3504, 0xB356B).
static int dwarf_skips(int dwarf, int seg, int side)
{
	return dwarf && seg >= 0 && seg <= Highest_segment_index && side >= 0 && side < 6 && (Segments[seg].sides[side].pad & 3);
}

// 0xB34E0: a trap that hurts: on, the dwarf test of the searched side,
// and a door or wall flag (0x140), a weapon, an object, damage, or a
// creature
static int trap_harmful(int t, int dwarf, int seg, int side)
{
	dtu_trap *tp;

	if (t < 0 || t >= Dtu_num_traps)
		return 0;
	tp = &Dtu_traps[t];
	if (!tp->on || dwarf_skips(dwarf, seg, side))
		return 0;
	return (tp->flags & 0x140) || tp->weapon_type >= 0 || tp->spawn_object >= 0 || tp->stat_delta < 0 || tp->robot_type >= 0;
}

// 0xB353A: a harmful trigger: on, the dwarf test of the searched side,
// and experience below 0 (+0x30), or else the first of: the first trap
// (+0x3B, that trap only, not its chain), the trigger it enables (+0x3D),
// the trap it enables (+0x3F).
// ASCENT: the executable follows +0x3D with no limit; the port stops
// after DTU_TRIG_CHAIN_LIMIT steps (a loop of triggers would hang 1.3).
static int trigger_harmful_depth(int t, int dwarf, int seg, int side, int depth)
{
	dtu_trigger *tg;

	if (t < 0 || t >= Dtu_num_triggers || depth > DTU_TRIG_CHAIN_LIMIT)
		return 0;
	tg = &Dtu_triggers[t];
	if (!tg->on || dwarf_skips(dwarf, seg, side))
		return 0;
	if ((int) tg->unk30 < 0)
		return 1;
	if (tg->first_trap >= 0)
		return trap_harmful(tg->first_trap, dwarf, seg, side);
	if (tg->enable_trigger >= 0)
		return trigger_harmful_depth(tg->enable_trigger, dwarf, seg, side, depth + 1);
	if (tg->enable_trap >= 0)
		return trap_harmful(tg->enable_trap, dwarf, seg, side);
	return 0;
}

static int trigger_harmful(int t)
{
	return trigger_harmful_depth(t, 0, -1, -1, 0);
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

// 0xB37B6, 0xB3AE7: the sounds of a find, only in the path of the Find
// Traps spell (the search without the spell goes to the box at once)
static void found_sound(int magical)
{
	object *p = ConsoleObject;

	digi_link_sound_to_pos(3001, p->segnum, 0, &p->pos, 0, F1_0);
	if (magical)
		digi_link_sound_to_pos(2130, p->segnum, 0, &p->pos, 0, F1_0);
}

static void side_flag(int seg, int side, int set, int bit)
{
	if (seg < 0 || seg > Highest_segment_index || side < 0 || side > 5)
		return;
	if (set)
		Segments[seg].sides[side].pad |= bit;
	else
		Segments[seg].sides[side].pad &= ~bit;
}

// 0xB365F
static void search_triggers(int seg, int side, int spell, int dwarf)
{
	object *p = ConsoleObject;
	int t, n = 0, level = (thief_level() + 1) & 0xFF;

	if (seg < 0 || seg >= DTU_TRIG_SEG_TABLE)
		return;
	for (t = Dtu_trigger_seg_table[seg]; t >= 0 && t < Dtu_num_triggers && n < 50; t = Dtu_triggers[t].next_in_seg, n++) {
		dtu_trigger *tg = &Dtu_triggers[t];
		int roll;

		if (tg->side != side || !trigger_harmful_depth(t, dwarf, seg, side, 0))
			continue;
		// ASCENT: 0xB370D measures with the quick distance (0xD705C)
		if (vm_vec_dist_quick(&tg->pos, &p->pos) > i2f(40))
			continue;
		if (Trig_find(t) == level)
			continue;
		if (Trig_find(t) < 0x80) {
			dtu_char_log("thief: search trigger %d (seg %d side %d)", t, seg, side);
			if (!dtu_thief_roll(3, NULL)) {
				Set_trig_find(t, level);
				continue;
			}
		}
		if (Trig_find(t) == 0xFE || Trig_remove(t) == level)
			continue;
		// 0xB378E: the automap shows a found trigger (side flag 4)
		side_flag(seg, side, 1, 4);
		Set_trig_find(t, 0xFF);
		if (spell) {
			// 0xB3605: a marker for 5 seconds (not in the port), no removal
			found_sound(0);
			HUD_init_message("You found a trigger!");
			dtu_char_log("thief: the spell finds trigger %d", t);
			Set_trig_find(t, 0xFE);
			Set_trig_remove(t, level);
			return;
		}
		dtu_char_log("thief: found trigger %d", t);
		if (dtu_char_choice(text(940, "You found a trigger!\nWould you like to disable it?"), 2, text(32, "Yes"), text(33, "No")) != 0) {
			Set_trig_find(t, 0xFE);
			continue;
		}
		if (!dtu_thief_roll(3, &roll)) {
			Set_trig_remove(t, level);
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
		side_flag(seg, tg->side, 0, 4);
		tg->on = 0;
		set_side_tmap2(tg->seg, tg->side, tg->tex_off);
		message(943, "You disabled the trigger.");
	}
}

// 0xB396C
static void search_traps(int seg, int side, int spell, int dwarf)
{
	object *p = ConsoleObject;
	dtu_player_attr *pa = &Dtu_char.pa;
	int t, level = (thief_level() + 1) & 0xFF;

	for (t = 0; t < Dtu_num_traps; t++) {
		dtu_trap *tp = &Dtu_traps[t];
		int magical, saved = pa->find_traps, roll, ok;

		if (tp->seg != seg || tp->side != side || !trap_harmful(t, dwarf, seg, side))
			continue;
		// ASCENT: 0xB3A07 measures with the quick distance (0xD705C)
		if (vm_vec_dist_quick(&tp->pos, &p->pos) > i2f(40))
			continue;
		if (Trap_find(t) == level)
			continue;
		magical = tp->spawn_object >= 0 || tp->robot_type >= 0;
		// a magical trap halves the skill for the rolls (0xB3A4A). The
		// executable doubles it again after the rolls (0xB3CF9), so an
		// odd skill is 1 lower until the next 0xAFB55; the port keeps the
		// value.
		if (magical)
			pa->find_traps >>= 1;
		if (Trap_find(t) < 0x80) {
			dtu_char_log("thief: search trap %d (seg %d side %d)%s", t, seg, side, magical ? ", magical" : "");
			ok = dtu_thief_roll(3, NULL);
			if (!ok) {
				Set_trap_find(t, level);
				pa->find_traps = saved;
				continue;
			}
		}
		if (Trap_find(t) == 0xFE || Trap_remove(t) == level) {
			pa->find_traps = saved;
			continue;
		}
		// 0xB3ABB: the automap shows a found trap (side flag 8)
		side_flag(seg, tp->side, 1, 8);
		Set_trap_find(t, 0xFF);
		if (spell) {
			found_sound(magical);
			HUD_init_message(magical ? "You found a magical trap!" : "You found a trap!");
			dtu_char_log("thief: the spell finds trap %d", t);
			Set_trap_find(t, 0xFE);
			Set_trap_remove(t, level);
			// 0xB396C returns here without the restore of the skill (a
			// defect of 1.3); the next dtu_char_update() restores it
			pa->find_traps = saved;
			return;
		}
		dtu_char_log("thief: found trap %d", t);
		if (dtu_char_choice(magical ? text(944, "You found a magical trap!\nWould you like to disable it?") :
				text(945, "You found a trap!\nWould you like to disable it?"), 2, text(32, "Yes"), text(33, "No")) != 0) {
			Set_trap_find(t, 0xFE);
			pa->find_traps = saved;
			continue;
		}
		ok = dtu_thief_roll(3, &roll);
		pa->find_traps = saved;
		if (!ok) {
			Set_trap_remove(t, level);
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
		side_flag(seg, tp->side, 0, 8);
		tp->on = 0;
		set_side_tmap2(tp->seg, tp->side, tp->tex_disabled);
		message(947, "You disabled the trap.");
	}
}

//------------------------------------------------------------------------
// The racial search for secret walls (0xB3ECE)

// 0x1466E8: the d6 of the search by race (human, dwarf, elf, dark elf,
// halfling, half-elf)
static const int Secret_d6[6] = { 3, 6, 5, 5, 3, 5 };

// The race index of the tables: P+0xDF bit 0 (half-elf) counts as 5
static int race_index(void)
{
	int race = (Dtu_char.pa.flags & DTU_PF_HALF_ELF) ? 5 : Dtu_char.pa.race;

	return (race < 0 || race > 5) ? 0 : race;
}

// 0xB3E57: the secret wall of a side is found: side flag 0x10 on both
// sides, sound 3104, text 949 "You found a secret door!"
static void secret_found(int seg, int side)
{
	segment *segp = &Segments[seg];
	int child = segp->children[side];

	segp->sides[side].pad |= 0x10;
	if (child > -1) {
		int cside = find_connect_side(segp, &Segments[child]);		// 0x4ADEE

		if (cside != -1)
			Segments[child].sides[cside].pad |= 0x10;
	}
	digi_play_sample(3104, F1_0);		// 0x2D955
	message(949, "You found a secret door!");
	dtu_char_log("thief: secret wall found at seg %d side %d", seg, side);
}

// 0xB3D4E: the triggers whose first trap is the trap of a found secret
// wall. A trigger on another side needs its own racial d6. A found
// trigger gets side flag 4 and fires (0x93B29 with 0xFF). Text 948 if
// none fired.
static void secret_triggers(object *obj, dtu_trap *tp)
{
	int t, any = 0;

	for (t = 0; t < Dtu_num_triggers; t++) {
		dtu_trigger *tg = &Dtu_triggers[t];

		if (!tg->on || tg->first_trap != tp->id)
			continue;
		if (tp->seg != tg->seg || tp->side != tg->side) {
			int d = rand() % 6 + 1;

			dtu_char_log("thief: racial search, trigger %d: d6 %d against %d", t, d, Secret_d6[race_index()]);
			if (d > Secret_d6[race_index()])
				continue;
		}
		if (tg->seg >= 0 && tg->seg <= Highest_segment_index && tg->side >= 0 && tg->side < 6)
			Segments[tg->seg].sides[(int) tg->side].pad |= 4;
		dtu_trig_fire(t, DTU_ACT_FORCE, obj);
		any = 1;
	}
	if (!any)
		message(948, "");
}

// 0xB3ECE(seg, side, obj, dwarf): called each frame of the search for the
// sides of 0xB4040 while the Find Traps spell is off. dwarf is 1 for the
// race dwarf of any class.
static void racial_search(int seg, int side, object *obj, int dwarf)
{
	segment *segp = &Segments[seg];
	int wall_num = segp->sides[side].wall_num, d, t;
	wall *w;

	// ASCENT: a side without a wall makes the executable read the bytes
	// before the wall array (Walls[-1]); the port skips the side
	if (wall_num < 0 || wall_num >= Num_walls)
		return;
	w = &Walls[wall_num];
	// one search for each wall: byte 0x16 is set before the roll
	if (wall_searched(wall_num))
		return;
	set_wall_searched(wall_num);
	// 0xB3D1E: a dwarf does not search a side with side flag & 3
	if (dwarf && (segp->sides[side].pad & 3))
		return;
	d = rand() % 6 + 1;
	dtu_char_log("thief: racial search, wall %d (seg %d side %d type %d): d6 %d against %d", wall_num, seg, side, w->type, d,
		Secret_d6[race_index()]);
	if (d > Secret_d6[race_index()])
		return;
	if (w->type == WALL_CLOSED) {
		// a closed wall with a trap on the side within 40.0 of the player
		for (t = 0; t < Dtu_num_traps; t++) {
			dtu_trap *tp = &Dtu_traps[t];

			if (!tp->on || tp->seg != seg || tp->side != side)
				continue;
			if (vm_vec_dist_quick(&tp->pos, &obj->pos) > i2f(40))
				continue;
			secret_found(seg, side);
			secret_triggers(obj, tp);
		}
	}
	else if (w->type == WALL_DOOR && (w->flags & 0x40)) {
		// a secret door: the center of the side within 40.0 (0x4AD3E)
		vms_vector cp;

		compute_center_point_on_side(&cp, segp, side);
		if (vm_vec_dist_quick(&cp, &obj->pos) <= i2f(40))
			secret_found(seg, side);
	}
}

// The fresh branch of level_start (0xAD67B to 0xAD69D) clears the bytes
// 0x16 and 0x17 of each wall record. The port calls this after the load
// of the level file and before the restore of a level state, which reads
// the bytes back (dtustate.c).
void dtu_thief_level_loaded(void)
{
	int i;

	for (i = 0; i < Num_walls && i < MAX_WALLS; i++)
		Walls[i].pad = 0;
}

// 0xA6F89 (refresh_level_texture_ids), called by level_start 0xAD5BF at
// 0xAD744 after the level fixes, for a fresh and for a restored level (a
// restore then reads the side flags and the wall flags back). 0x1B2B1
// finds a texture by its name in TmapInfo and gives 0 for a name that is
// not there. Each side: side flag & 3 is cleared; water gives 1, lava 3
// (not for an illusory side, wall_is_doorway 3 or 7), wood 2; a door
// (wall type 2) with one of the 29 door textures becomes a secret door
// (wall flag 0x40), which the racial search (0xB3ECE), the door use
// (0x7E859, wall.c) and the automap (0x19335) read. tmap_num and the raw
// tmap_num2 (with the rotation bits) are compared, as in the executable.
int Dtu_fix_secret_door_names = 1;

static int texture_by_name(const char *name)		// 0x1B2B1
{
	int i;

	for (i = 0; i < NumTextures; i++)
		if (!stricmp(TmapInfo[i].filename, name))
			return i;
	return 0;
}

static int side_has(segment *segp, int side, const int *tex, int n)
{
	int k, t1 = segp->sides[side].tmap_num, t2 = segp->sides[side].tmap_num2;

	for (k = 0; k < n; k++)
		if (t1 == tex[k] || t2 == tex[k])
			return 1;
	return 0;
}

void dtu_thief_level_textures(void)
{
	static const char *water_names[6] = { "water", "water1", "water2", "wfal01", "wfal02", "wfal04" };		// 0x3CE0E4
	static const char *lava_names[4] = { "lava02", "lava", "lavaflo", "magma" };		// 0x3CE0D4
	static const char *wood_names[22] = { "wood1", "wood2", "wood3", "wood4", "wood5", "wood6", "wood7", "wood8", "wood9",
		"wood11", "wood12", "wood13", "wood14", "wood15", "wood16", "wood21", "wood22", "wood91", "wood92", "wood93",
		"wood94", "wood95" };		// 0x3CE07C
	static const char *door_names[29] = { "wald1#0", "wald2#0", "wald3#0", "wald4#0", "gdown111#0", "dleft11#0",
		"dleft31#0", "dorsk11#0", "down111#0", "drght11#0", "dup1t11#0", "gdown11#0", "shelfup1#0", "stadnup1#0",
		"stadn2p1#0", "stadn3p1#0", "trapdor1#0", "up1pdor1#0", "wald1or1#0", "wald2or1#0", "wald3or1#0", "wald4or1#0",
		"dleft111#0", "dleft311#0", "door2511#0", "dor1l511#0", "dorsk111#0", "drght111#0", "dup1t111#0" };		// 0x3D4254
	int water[6], lava[4], wood[22], door[29], k, seg, side, missing = 0;
	int n_water = 0, n_lava = 0, n_wood = 0, n_secret = 0;

	for (k = 0; k < 6; k++) { water[k] = texture_by_name(water_names[k]); missing += !water[k]; }
	for (k = 0; k < 4; k++) { lava[k] = texture_by_name(lava_names[k]); missing += !lava[k]; }
	for (k = 0; k < 22; k++) { wood[k] = texture_by_name(wood_names[k]); missing += !wood[k]; }
	for (k = 0; k < 29; k++) { door[k] = texture_by_name(door_names[k]); missing += !door[k]; }
	// Dtu_fix_secret_door_names: a name that is not found matches no side
	// (in 1.3 it is texture 0, and tmap_num2 0 is "no overlay"). This runs
	// before dtuchar.c reads its options.
	if (FindArg("-origchar"))
		Dtu_fix_secret_door_names = 0;
	if (Dtu_fix_secret_door_names) {
		for (k = 0; k < 6; k++) if (!water[k]) water[k] = -1;
		for (k = 0; k < 4; k++) if (!lava[k]) lava[k] = -1;
		for (k = 0; k < 22; k++) if (!wood[k]) wood[k] = -1;
		for (k = 0; k < 29; k++) if (!door[k]) door[k] = -1;
	}
	for (seg = 0; seg <= Highest_segment_index; seg++) {
		segment *segp = &Segments[seg];

		for (side = 0; side < 6; side++) {
			segp->sides[side].pad &= ~3;
			if (side_has(segp, side, water, 6)) {
				segp->sides[side].pad |= 1;
				n_water++;
			}
			{
				int wid = WALL_IS_DOORWAY(segp, side);

				if (wid != WID_TRANSILLUSORY_WALL && wid != WID_ILLUSORY_WALL && side_has(segp, side, lava, 4)) {
					segp->sides[side].pad |= 3;
					n_lava++;
				}
			}
			if (side_has(segp, side, wood, 22)) {
				segp->sides[side].pad |= 2;
				n_wood++;
			}
			// ASCENT: a side with no wall makes the executable read the
			// bytes before the wall array (Walls[-1]); the port skips it
			{
				int wn = segp->sides[side].wall_num;

				if (wn >= 0 && wn < Num_walls && Walls[wn].type == WALL_DOOR && side_has(segp, side, door, 29)) {
					if (!(Walls[wn].flags & 0x40))
						n_secret++;
					Walls[wn].flags |= 0x40;
				}
			}
		}
	}
	dtu_log("Level textures (0xA6F89): %d of 61 names not found, sides: %d water, %d lava, %d wood; %d secret doors\n", missing,
		n_water, n_lava, n_wood, n_secret);
	{
		char list[400] = "";

		for (k = 0; k < 61; k++) {
			const char *name = k < 6 ? water_names[k] : k < 10 ? lava_names[k - 6] : k < 32 ? wood_names[k - 10] : door_names[k - 32];
			int t = k < 6 ? water[k] : k < 10 ? lava[k - 6] : k < 32 ? wood[k - 10] : door[k - 32];

			if (t <= 0 && strlen(list) + strlen(name) + 2 < sizeof(list)) {
				strcat(list, " ");
				strcat(list, name);
			}
		}
		if (list[0])
			dtu_log("Level textures: not found (%s):%s\n", Dtu_fix_secret_door_names ? "no match" : "texture 0", list);
	}
}

// ASCENT: test
int dtu_thief_goto_secret(int n)
{
	object *p = ConsoleObject;
	int seg, side, t;

	for (seg = 0; seg <= Highest_segment_index; seg++)
		for (side = 0; side < 6; side++) {
			int wn = Segments[seg].sides[side].wall_num, secret = 0;
			vms_vector cp, center, dir;

			if (wn < 0 || wn >= Num_walls)
				continue;
			if (Walls[wn].type == WALL_DOOR && (Walls[wn].flags & 0x40))
				secret = 1;
			else if (Walls[wn].type == WALL_CLOSED)
				for (t = 0; t < Dtu_num_traps; t++)
					if (Dtu_traps[t].on && Dtu_traps[t].seg == seg && Dtu_traps[t].side == side)
						secret = 1;
			if (!secret || n-- > 0)
				continue;
			compute_center_point_on_side(&cp, &Segments[seg], side);
			compute_segment_center(&center, &Segments[seg]);
			vm_vec_sub(&dir, &cp, &center);
			vm_vec_normalize(&dir);
			p->pos = center;
			vm_vector_2_matrix(&p->orient, &dir, NULL, NULL);
			obj_relink(p - Objects, seg);
			dtu_char_log("script: secret wall %d (type %d flags 0x%x) at seg %d side %d", wn, Walls[wn].type, Walls[wn].flags, seg, side);
			return 1;
		}
	dtu_char_log("script: no such secret wall");
	return 0;
}

// 0xB4040
static void search_frame(void)
{
	object *p = ConsoleObject;
	fvi_info hit;
	int spell = (p->dtu.m_88 & DTU_FX_FIND_TRAPS) != 0, side, seg;
	// 0xB40D4: the effective race (a half-elf counts as race 5)
	int dwarf = (race_index() == DTU_RACE_DWARF && !has_class(DTU_CLASS_THIEF) && !spell);

	front_ray(p, 40, &hit);
	seg = hit.hit_seg >= 0 ? hit.hit_seg : p->segnum;
	if (seg < 0 || seg > Highest_segment_index)
		return;
	for (side = 0; side < 6; side++) {
		if (vm_vec_dot(&Segments[seg].sides[side].normals[1], &p->orient.fvec) > F1_0/4)
			continue;
		// 0xB40AF: the skill or the spell; the dwarf flag only limits the
		// sides (0xB34E0)
		if (Dtu_char.pa.find_traps != 0 || spell) {
			search_triggers(seg, side, spell, dwarf);
			search_traps(seg, side, spell, dwarf);
		}
		// Without the spell: 0xB3ECE, the racial search for secret walls
		if (!spell)
			racial_search(seg, side, p, race_index() == DTU_RACE_DWARF);
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
