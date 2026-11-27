/*
 * ASCENT: new file, split from dtuobj.c. The alerts and the awareness of
 * the creatures, the wake of a creature, the check of the objects, the
 * gate in of a creature and the drop of objects in a segment. The shared
 * declarations are in dtuobj_int.h.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include "dtuitemfix.h"	// ASCENT: the corrected item names
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "mono.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "collide.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "piggy.h"
#include "vclip.h"
#include "weapon.h"
#include "player.h"
#include "gauges.h"
#include "lighting.h"
#include "wall.h"
#include "gamesave.h"
#include "gameseq.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtutrig.h"
#include "dtuspell.h"	// ASCENT: the spells of the creatures, the area of a missile
#include "dtuthief.h"	// ASCENT: Hide In Shadows and Move Silently in the AI
#include "dtuscript.h"	// ASCENT: quest flag 67 in the creature setup
#include "textures.h"	// ASCENT: the lava textures of BANE
#include "tmapfx.h"	// ASCENT: the tint of Detect Magic
#include "dtuchar.h"
#include "digi.h"
#include "sounds.h"
#include "dtufoot.h"		// ASCENT: the footsteps of the creatures
#include "fireball.h"
#include "palette.h"
#include "dtumusic.h"
#include "key.h"		// ASCENT: the key that ends the wait of the death
#include "dtuchar.h"
#include "dtuopts.h"	// ASCENT: Dtu_improved_ai
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()
#include "dtulog.h"		// ASCENT: the session log
#include "dtuxp.h"		// ASCENT: the kill line, the death lines of the session log
#include "dtumiss.h"		// ASCENT: the miss sound of a creature
#include "dtuobj_int.h"

static void creature_wake(object *obj, int make_hostile, int sound);

// 0xA6505 (not read in full): makes a creature active, as the wake up of
// do_ai_frame does (0x13D80).
void dtu_creature_wake(object *obj, int make_hostile)
{
	creature_wake(obj,make_hostile,1);
}

// ASCENT: the group part of the alert function 0x160AB (ai.md, "Alert
// function"). Levels 3 to 5 only: the executable calls it with 3 when the
// player hits a creature in melee (0x27B3F, caller the player), when a
// missile of the player hits a creature (0x274B7, caller the creature) and
// when the player runs into a hostile creature (0x2707A, 0x9E70D, caller the
// creature); with 4 when a body becomes a corpse (0x6A45A), when a script
// makes a creature hostile (0xA94A1) and when a creature gets up again
// (0x14602); with 5 at a rest place (0x3E2C1, caller the player) and for
// the script statement 3/0xC5 (0xAB080). Each other creature (index 1 to
// Highest_object_index - 1) that is not active, not peaceful and not
// fleeing, whose segment sees the segment of the caller (the brain bit,
// 0x9810D) and that has a free ray to the player (0x10815) turns to the
// caller, becomes hostile and wakes (0xA6505). A deaf creature (effect bit
// 4) must face the caller. The test 0x161BC (rand() % distance <= iq * 1.0)
// always passes, since rand() is at most 0x7FFF. The see sound plays for
// every other creature: the executable takes the parity of a player field
// (0x1D339F) plus the object index; the port takes the parity of the index.
// Each level first adds an awareness event (0x16055, below). The agitation
// (0x167174) and the bits of +0x15E are not in the port.
//
// ASCENT: the awareness events (0x16055, 0x16304, 0x162A1, 0x16366, ai.md
// "Awareness events"). 0x16055 keeps up to 64 events (segment, position,
// type = the level of the alert) for the frame. Each frame 0x16366 (called
// by the game frame, 0x438AC) clears New_awareness of all segments, and for
// each event 0x162A1(segment, type, 1) stores the type in the segment when
// it is higher and, while the depth is 5 or less, calls itself for each
// child of the 6 sides with depth + 1 and type 5 made 4. So the event
// reaches the segments up to 5 steps away, through any wall. Then each AI
// object with the hostile bit whose awareness is below New_awareness of its
// segment gets that awareness, the awareness time 3.0 and the morale check.
// The recursion has no visited set (up to 6^5 calls for one event); the port
// gives the same values with a search by distance.
//
// ASCENT: settings of the alert (D-112 and D-113 of docs/audit/defects-1.3.md,
// verified in the bytes). -origobj clears them.
// Dtu_fix_noise_walls: 0x162A1 follows each child with no wall test, so the
// noise goes through closed doors and walls. 1 (default): the noise passes
// only a side that a body can pass or that can be seen through (an open
// door, a grate); a closed door stops it.
// The ray of the wake test goes to the player (0x161CA calls 0x10815), also
// when the caller is a corpse or a hit creature. ASCENT: that is the rule
// of the port (D-114 is not a defect): an active creature that does not see
// the player rests again at its next look (0x14206), so an alert in 1.3
// wakes only a creature that can take on the player. The level scripts
// need it: the trigger of THIEF0 segment 207 makes each goblin hostile
// (script 50, label 900, the alert of level 4 at 0xA94A1), and the goblins
// must wait in their camp (the messages 24 and 25 of the triggers of
// segments 212, 244 and 256). A ray to the caller woke the whole camp, and
// the paths of the port brought it to the player.
// Dtu_fix_alert_last: the wake loop ends before Highest_object_index
// (0x1628D, jl), so the creature in the last slot never wakes. 1 (default):
// the loop includes it.
int Dtu_fix_noise_walls = 1;
int Dtu_fix_alert_last = 1;

#define AWARE_MAX_EVENTS	64
static struct { short segnum; short type; } Aware_events[AWARE_MAX_EVENTS];

static void awareness_event(object *obj, int type)
{
	// ASCENT: 0x16055: the types 2 to 4 call 0x15FBC, which puts the
	// believed position of a hidden player at the player (cloak_reset)
	if (type >= 2 && type <= 4)
		cloak_reset();
	if (N_aware_events < AWARE_MAX_EVENTS && obj->segnum >= 0 && obj->segnum <= Highest_segment_index) {
		Aware_events[N_aware_events].segnum = obj->segnum;
		Aware_events[N_aware_events].type = type;
		N_aware_events++;
	}
}

void awareness_events_frame(void)
{
	static ubyte new_aware[MAX_SEGMENTS], depth[MAX_SEGMENTS];
	static short queue[MAX_SEGMENTS];
	int e, i;

	if (N_aware_events == 0)
		return;
	memset(new_aware,0,sizeof(new_aware));
	for (e=0;e<N_aware_events;e++) {
		int type = Aware_events[e].type, near = (type == 5) ? 4 : type, head = 0, tail = 0;

		memset(depth,0xff,sizeof(depth));
		queue[tail++] = Aware_events[e].segnum;
		depth[Aware_events[e].segnum] = 1;
		while (head < tail) {
			int s = queue[head++], side, t = (depth[s] == 1) ? type : near;

			if (new_aware[s] < t)
				new_aware[s] = (ubyte) t;
			if (depth[s] > 5)
				continue;
			for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
				int c = Segments[s].children[side];

				if (Dtu_fix_noise_walls && c >= 0 &&
					 !(WALL_IS_DOORWAY(&Segments[s],side) & (WID_FLY_FLAG|WID_RENDPAST_FLAG)))
					continue;
				if (c >= 0 && c <= Highest_segment_index && depth[c] == 0xff) {
					depth[c] = depth[s] + 1;
					queue[tail++] = c;
				}
			}
		}
		dtu_obj_log("noise: type %d at seg %d reaches %d segments",type,Aware_events[e].segnum,tail);
	}
	N_aware_events = 0;

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || !(obj->dtu.m_1b0 & DTU_HOSTILE))
			continue;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index || Ail[i].awareness >= new_aware[obj->segnum])
			continue;
		dtu_obj_log("awareness: obj %d %s %d -> %d",i,dtu_object_name(obj),Ail[i].awareness,new_aware[obj->segnum]);
		Ail[i].awareness = new_aware[obj->segnum];
		Ail[i].awareness_time = i2f(3);
		morale_check(obj);
	}
}

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_ai, the senses of a creature (a decision of the
// project, docs/re/ai.md "Senses, alarms and doors"). In 1.3 a creature
// notices the player by the brain bit of the two segments and the ray
// 0x10815, and for a melee creature nearer than 100.0 a blocked ray counts
// as free. The brain bit is built with all doors open and all traps
// switched (0x9786A), so a melee creature behind a closed door or behind a
// wall that a trigger opens later (the four orcs of DROW2 in the closets of
// the walls 0, 2, 4 and 6) notices the player and walks into the wall, and
// an alarm wakes the creatures of the room behind a closed door.
// With the setting:
// - Sight: the ray from the eyes of the creature (5.0 above the center of a
//   creature that walks) to the center of the player hits no wall and no
//   door that is not open. An illusion wall does not stop it.
// - Hearing, for the creatures that have the melee rule in 1.3 (a melee
//   attack and no spell level): a path from the segment of the player to
//   the segment of the creature through sides that a body can pass (no
//   wall, an open door, an illusion wall), with the sum of the distances
//   between the centers of its segments at most DTU_HEAR_DIST (100.0, the
//   distance of the melee rule).
// - An alarm (the alert of the levels 3 to 5) reaches a creature along such
//   a path from the caller of at most DTU_ALARM_DIST, and the creature must
//   also see or hear the player (the rule of 1.3 for the ray, see above).
// - A creature that noticed the player (sight, hearing, a hit, an alarm) in
//   the last DTU_COMBAT_TIME seconds fights: it keeps its path to the
//   player and, when its type can, opens doors (opens_doors(), dtuobj_ai.c).
#define DTU_HEAR_DIST	i2f(100)
#define DTU_ALARM_DIST	i2f(150)
#define DTU_COMBAT_TIME	i2f(30)

static fix Hear_map[MAX_SEGMENTS], Alarm_map[MAX_SEGMENTS];
static int Hear_seg = -1;
static fix Hear_time = -1;

// The length of the shortest open path from the segment "start" to each
// segment, -1 for a segment that no open path of the length "limit" reaches
static void open_path_map(int start, fix limit, fix *dist)
{
	static short queue[MAX_SEGMENTS];
	static ubyte queued[MAX_SEGMENTS], have[MAX_SEGMENTS];
	static vms_vector center[MAX_SEGMENTS];
	int head = 0, count = 0, i;

	for (i=0;i<=Highest_segment_index;i++)
		dist[i] = -1;
	if (start < 0 || start > Highest_segment_index)
		return;
	memset(queued,0,Highest_segment_index+1);
	memset(have,0,Highest_segment_index+1);
	dist[start] = 0;
	queue[0] = start;
	queued[start] = 1;
	count = 1;
	while (count) {
		int s = queue[head], side;

		head = (head + 1) % MAX_SEGMENTS;
		count--;
		queued[s] = 0;
		if (!have[s]) {
			compute_segment_center(&center[s],&Segments[s]);
			have[s] = 1;
		}
		for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
			int c = Segments[s].children[side];
			fix d;

			if (c < 0 || c > Highest_segment_index || !(WALL_IS_DOORWAY(&Segments[s],side) & WID_FLY_FLAG))
				continue;
			if (!have[c]) {
				compute_segment_center(&center[c],&Segments[c]);
				have[c] = 1;
			}
			d = dist[s] + vm_vec_dist(&center[s],&center[c]);
			if (d > limit || (dist[c] >= 0 && dist[c] <= d))
				continue;
			dist[c] = d;
			if (!queued[c]) {
				queue[(head + count) % MAX_SEGMENTS] = c;
				queued[c] = 1;
				count++;
			}
		}
	}
}

// The length of the open path from the player to a segment, -1 for none.
// The map is made one time for each frame.
static fix hear_dist(int segnum)
{
	if (segnum < 0 || segnum > Highest_segment_index)
		return -1;
	if (Hear_time != GameTime || Hear_seg != ConsoleObject->segnum) {
		open_path_map(ConsoleObject->segnum,DTU_HEAR_DIST,Hear_map);
		Hear_time = GameTime;
		Hear_seg = ConsoleObject->segnum;
	}
	return Hear_map[segnum];
}

// Dtu_improved_ai, in place of creature_ray_to_player() (0x10815): 1 the
// creature sees the player, 2 it hears the player, 0 neither
int creature_senses_player(object *obj)
{
	vms_vector p0 = obj->pos;
	int seg;

	if (!is_flying(obj))
		p0.y += i2f(5);
	seg = find_point_seg(&p0,obj->segnum);
	if (seg < 0) {
		p0 = obj->pos;
		seg = obj->segnum;
	}
	if (ray_is_free(&p0,seg,&ConsoleObject->pos,obj-Objects))
		return 1;
	if (Robot_info[obj->id].attack_type == 1 && Monster_ext[obj->id].spell_level == 0 && hear_dist(obj->segnum) >= 0)
		return 2;
	return 0;
}

void creature_combat_mark(object *obj)
{
	Ail[obj-Objects].combat_time = GameTime ? GameTime : 1;
}

int creature_in_combat(object *obj)
{
	fix t = Ail[obj-Objects].combat_time;

	return t != 0 && GameTime >= t && GameTime - t <= DTU_COMBAT_TIME;
}

void dtu_creature_alert(object *caller, int level)
{
	int i, woken = 0, stopped = 0;

	if (!Dtu_objects_enabled || caller == NULL || ConsoleObject == NULL)
		return;
	awareness_event(caller,level);
	if (level < 3)
		return;
	if (improved_ai())
		open_path_map(caller->segnum,DTU_ALARM_DIST,Alarm_map);
	for (i=1;i<Highest_object_index + (Dtu_fix_alert_last ? 1 : 0);i++) {
		object *obj = &Objects[i];
		vms_vector v;

		if (obj == caller || obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types)
			continue;
		if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED) || DTU_ANIM_STATE(obj) == DAS_DIE)
			continue;
		if (Ail[i].activity == 2 || (obj->dtu.m_1b0 & (DTU_PEACEFUL|DTU_FLEEING)))
			continue;
		if (improved_ai()) {
			// Dtu_improved_ai: the alarm goes along an open path, and the
			// creature sees or hears the player
			if (!brain_bit(obj->segnum,caller->segnum))
				continue;
			if (Alarm_map[obj->segnum] < 0) {
				stopped++;
				continue;
			}
			if (!creature_senses_player(obj))
				continue;
		}
		else if (!brain_bit(obj->segnum,caller->segnum) || !creature_ray_to_player(obj))
			continue;
		vm_vec_sub(&v,&caller->pos,&obj->pos);
		if ((obj->dtu.m_88 & DTU_FX_DEAF) && vm_vec_dot(&v,&obj->orient.fvec) <= 0)
			continue;
		v.y = 0;
		if (vm_vec_mag_quick(&v) > F1_0/16) {
			vm_vec_normalize_quick(&v);
			vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
		}
		obj->dtu.m_1b0 |= DTU_HOSTILE;
		dtu_obj_log("alert: level %d from obj %d %s wakes obj %d %s",level,(int)(caller-Objects),dtu_object_name(caller),i,dtu_object_name(obj));
		creature_wake(obj,0,i & 1);
		woken++;
	}
	if (improved_ai() && (woken || stopped))
		dtu_obj_log("alert: level %d from obj %d: %d creatures woken, %d with no open path from the caller",level,(int)(caller-Objects),woken,stopped);
}

static void creature_wake(object *obj, int make_hostile, int sound)
{
	dtu_ai_local *ail;
	ai_static *aip;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	ail = &Ail[obj-Objects];
	aip = &obj->ctype.ai_info;

	if (make_hostile) {
		obj->dtu.m_1b0 |= DTU_HOSTILE;
		obj->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
	}
	// Dtu_improved_ai: a hit, an alarm or a script: the creature fights
	creature_combat_mark(obj);
	if (ail->activity == 2)
		return;

	ail->target = is_hostile(obj) ? 0 : -1;
	ail->activity = 2;
	set_anim(obj,DAS_STAND);
	ail->anim_start = ail->time_player_seen = GameTime;
	ail->awareness_time = i2f(3);
	if (sound)
		sound_at(Robot_info[obj->id].see_sound,obj->segnum,&obj->pos,Monster_info[obj->id].str * 0x1000);
	if (is_hostile(obj)) {
		ail->awareness = 4;
		ail->mode = DAIM_CHASE_OB;
		aip->CURRENT_STATE = DAIS_REST;
		aip->GOAL_STATE = DAIS_SRCH;
		morale_check(obj);
	}
	else {
		ail->awareness = 0;
		aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
	}
	dtu_obj_log("obj %d %s: woken, hostile %d",(int)(obj-Objects),dtu_object_name(obj),is_hostile(obj));
}

// ASCENT: the state check of the soak test (dtusoak.c): the AI values of
// each creature. Writes the first problem to msg and returns 1, or 0.
int dtu_objects_check(char *msg, int size)
{
	int i;

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];
		dtu_ai_local *ail = &Ail[i];
		polymodel *po;
		int s, f;

		if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
			continue;
		if (obj->id >= N_robot_types) {
			snprintf(msg,size,"obj %d: robot id %d of %d types",i,obj->id,N_robot_types);
			return 1;
		}
		s = DTU_ANIM_STATE(obj);
		if (s < 0 || s >= DTU_N_ANIM_STATES) {
			snprintf(msg,size,"obj %d %s: animation state %d",i,dtu_object_name(obj),s);
			return 1;
		}
		f = DTU_ANIM_FRAME(obj);
		po = obj_model(obj);
		if (po && po->n_frames > 0 && (f < -1 || f >= Dtu_anim_first[DTU_N_ANIM_STATES])) {
			snprintf(msg,size,"obj %d %s: frame %d in state %d",i,dtu_object_name(obj),f,s);
			return 1;
		}
		if (ail->target < -1 || ail->target > Highest_object_index) {
			snprintf(msg,size,"obj %d %s: target %d",i,dtu_object_name(obj),ail->target);
			return 1;
		}
		// (a path index past the end of an empty path is not used)
		if (ail->path_len < 0 || ail->path_len > DTU_PATH_MAX || ail->path_idx < -1 || ail->path_idx > DTU_PATH_MAX) {
			snprintf(msg,size,"obj %d %s: path index %d of %d",i,dtu_object_name(obj),ail->path_idx,ail->path_len);
			return 1;
		}
		for (s=0; s<ail->path_len; s++)
			if (ail->path[s] < 0 || ail->path[s] > Highest_segment_index) {
				snprintf(msg,size,"obj %d %s: path point %d is segment %d",i,dtu_object_name(obj),s,ail->path[s]);
				return 1;
			}
		if (ail->mode > DAIM_OPENDOOR) {
			snprintf(msg,size,"obj %d %s: mode %d",i,dtu_object_name(obj),ail->mode);
			return 1;
		}
	}
	return 0;
}

// Turn undead (0xADC46, dtucombat.c): the result for one creature. The
// executable sets the flags, calls 0xA6505 (the creature starts again,
// also when it is active) and the morale function 0x10ADF.
void dtu_creature_morale(object *obj)
{
	if (obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types)
		morale_check(obj);
}

// ASCENT: 0x9026D: the end of Confusion on a creature: hostile, not
// fleeing and not peaceful, target 0 (the player), then the morale check
void dtu_creature_confusion_end(object *obj)
{
	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	obj->dtu.m_1b0 = (obj->dtu.m_1b0 & ~(DTU_FLEEING|DTU_PEACEFUL)) | DTU_HOSTILE;
	Ail[obj-Objects].target = 0;
	morale_check(obj);
}

void dtu_creature_turn(object *obj, int turned)
{
	dtu_ai_local *ail;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || obj->id >= N_robot_types || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	ail = &Ail[obj-Objects];
	if (turned) {
		// 0xAE1DD: target -1, fleeing, not hostile
		ail->target = -1;
		obj->dtu.m_1b0 = (obj->dtu.m_1b0 & ~DTU_HOSTILE) | DTU_FLEEING;
	}
	else
		obj->dtu.m_1b0 |= DTU_HOSTILE;		// 0xAE0DC, 0xAE22B
	ail->activity = 0;
	dtu_creature_wake(obj,0);
	// dtu_creature_wake() makes the morale check of a hostile creature
	if (!is_hostile(obj))
		morale_check(obj);
}

// gate_in_robot (0x1374D, 0x13407): a new creature at the center of the
// segment. The limit is 2 * difficulty + 3 creatures that were made this
// way. The executable does not set the hostile bit of the new creature
// here and what sets it was not found. The port makes it hostile. (guess)
int dtu_gate_in_creature(int type, int segnum)
{
	int i, count = 0, objnum;
	vms_vector pos, v;
	object *obj;

	if (segnum < 0 || segnum > Highest_segment_index || type < 0 || type >= N_robot_types)
		return -1;

	for (i=0;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_ROBOT && Ail[i].gated && Objects[i].control_type == CT_AI)
			count++;
	if (count > 2*Difficulty_level + 3) {
		dtu_obj_log("gate: no creature, %d gated creatures live",count);
		return -1;
	}

	compute_segment_center(&pos,&Segments[segnum]);

	// the place must be free
	for (i=Segments[segnum].objects;i!=-1;i=Objects[i].next)
		if ((Objects[i].type == OBJ_ROBOT || Objects[i].type == OBJ_PLAYER) && !(Objects[i].flags & OF_DESTROYED) &&
			 vm_vec_dist_quick(&Objects[i].pos,&pos) < Objects[i].size + i2f(4)) {
			dtu_obj_log("gate: no creature, obj %d is at the center of segment %d",i,segnum);
			return -1;
		}

	objnum = obj_create(OBJ_ROBOT,type,segnum,&pos,&vmd_identity_matrix,i2f(4),CT_AI,MT_PHYSICS,RT_POLYOBJ);
	if (objnum < 0)
		return -1;
	obj = &Objects[objnum];
	obj->shields = i2f(Monster_info[type].max_hit_points > 0 ? Monster_info[type].max_hit_points : 1);
	obj->ctype.ai_info.behavior = DAIB_WANDER;
	obj->ctype.ai_info.hide_segment = segnum;
	obj->dtu.m_1b0 = DTU_HOSTILE;
	obj->contains_count = 0;
	ail_keep_bits = 1;
	setup_creature(obj);
	ail_keep_bits = 0;
	if (obj->control_type != CT_AI) {
		dtu_creature_remove(obj,"a gated creature with no AI after its setup");
		return -1;
	}
	Ail[objnum].gated = 1;

	vm_vec_sub(&v,&ConsoleObject->pos,&pos);
	v.y = 0;
	if (vm_vec_normalize(&v) > 0x100)
		vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);

	dtu_obj_log("gate: obj %d %s (type %d) in segment %d at %.1f %.1f %.1f, hp %d",objnum,dtu_object_name(obj),type,segnum,
		f2fl(pos.x),f2fl(pos.y),f2fl(pos.z),f2i(obj->shields));
	return objnum;
}

// 0x9493A: the floor of the segment opened. A creature that lives wakes up
// and falls by its gravity. The executable gives the other objects a
// movement with gravity. The port puts them on the floor below at once.
void dtu_drop_objects_in_segment(int segnum)
{
	int objnum, next;
	vms_vector floor;

	if (segnum < 0 || segnum > Highest_segment_index)
		return;
	compute_center_point_on_side(&floor,&Segments[segnum],WBOTTOM);

	for (objnum=Segments[segnum].objects;objnum!=-1;objnum=next) {
		object *obj = &Objects[objnum];
		fvi_query fq;
		fvi_info hit;
		vms_vector p0, p1;

		next = obj->next;
		if (obj->type == OBJ_PLAYER || (obj->flags & OF_SHOULD_BE_DEAD))
			continue;
		if (obj->type == OBJ_ROBOT && obj->control_type == CT_AI) {
			// 0x94984: bit 0x20 of +0x1AE, the fall (no lava, x and z
			// velocity / 8, until the feet are less than 1.0 above a floor)
			Ail[objnum].falling = 1;
			dtu_creature_wake(obj,0);
			continue;
		}
		if (obj->type != OBJ_ROBOT && obj->type != OBJ_DRESSING && obj->type != OBJ_MISC && obj->type != OBJ_ARMOR &&
			 obj->type != OBJ_SPELL && obj->type != OBJ_WEAPON)
			continue;

		p0 = obj->pos;
		p0.y = floor.y + F1_0/4;
		if (find_point_seg(&p0,segnum) < 0)
			p0 = obj->pos;
		p1 = p0;
		p1.y -= i2f(400);
		fq.p0 = &p0;
		fq.p1 = &p1;
		fq.startseg = find_point_seg(&p0,segnum);
		fq.rad = 0;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		if (fq.startseg >= 0 && find_vector_intersection(&fq,&hit) == HIT_WALL && hit.hit_seg >= 0) {
			vms_vector np = obj->pos;

			np.y += hit.hit_pnt.y - floor.y;
			if (find_point_seg(&np,hit.hit_seg) >= 0) {
				obj->pos = np;
				obj_relink(objnum,find_point_seg(&np,hit.hit_seg));
				dtu_obj_log("drop: obj %d %s falls by %.1f",objnum,dtu_object_name(obj),f2fl(floor.y - hit.hit_pnt.y));
			}
		}
	}
}
