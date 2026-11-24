/*
 * ASCENT: new file, split from dtuobj.c. The movement of a creature: the
 * stop distance at the player, the gravity, the integration and the
 * physics of a frame (pmon.c, 0x9F153). The shared declarations are in
 * dtuobj_int.h.
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
#include "dtuopts.h"	// ASCENT: Dtu_improved_pathfinding
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()
#include "dtulog.h"		// ASCENT: the session log
#include "dtuxp.h"		// ASCENT: the kill line, the death lines of the session log
#include "dtumiss.h"		// ASCENT: the miss sound of a creature
#include "dtuobj_int.h"

#define MAX_IGNORE_OBJS 100		// as in physics.c

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_pathfinding, the body of a creature at the player

// The reach of the melee weapon of a model: the largest distance from the
// center of the creature to the center of the player, straight in front of
// the creature, at which the weapon test of weapon_test() touches the
// cylinder of the player in a frame that can hit (frame 8 of ATTACK and
// ATTACK2 to the end of the state). weapon_test() moves the test position
// forward by the square root of 2, takes the weapon sphere of the frame
// (get_weapon_sphere()) and tests the segment from its center outward by
// its radius, with its radius, against the cylinder of the player. The
// test itself does not change.

fix model_weapon_reach(object *obj)
{
	polymodel *po = obj_model(obj);
	int n = obj->rtype.pobj_info.model_num, s, f, k;
	fix best = -1;

	if (po == NULL || n < 0 || n >= MAX_POLYGON_MODELS)
		return 0;
	if (Model_reach[n])
		return Model_reach[n] > 0 ? Model_reach[n] : 0;

	if ((po->box_flags & DTU_BOX_BODY) && po->weapon_center && po->weapon_radius)
		for (s=DAS_ATTACK;s<=DAS_ATTACK2;s++)
			for (f=Dtu_anim_first[s]+8;f<Dtu_anim_first[s+1] && f<po->n_frames;f++) {
				vms_vector c = po->weapon_center[f], dir = c;
				fix wr = po->weapon_radius[f], hit_r = DTU_BODY_RAD + wr;

				vm_vec_normalize_quick(&dir);
				c.z += 0x16a0a;		// the test position of weapon_test()
				for (k=0;k<2;k++) {
					fix x = c.x + (k ? fixmul(dir.x,wr) : 0);
					fix z = c.z + (k ? fixmul(dir.z,wr) : 0);
					fix d;

					if (abs(x) >= hit_r)
						continue;
					d = z + fix_sqrt(fixmul(hit_r,hit_r) - fixmul(x,x));
					if (d > best)
						best = d;
				}
			}

	Model_reach[n] = (best > 0) ? best : -1;
	return (best > 0) ? best : 0;
}

// The distance in x and z from the player at which a creature that walks
// to the player stops: the reach of its weapon minus 1.0 (the creature
// turns to the player at a rate, and the player moves), but not more than
// the distance at which it starts an attack (choose_attack_goal()) minus
// 1.0, and not less than the bodies (torso cylinder and player cylinder).
// 0: no stop (a creature with a ranged attack or with no weapon box).
fix creature_stop_dist(object *obj)
{
	fix reach, attack, r, hh;
	vms_vector c;

	if (Robot_info[obj->id].attack_type != 1)
		return 0;
	reach = model_weapon_reach(obj);
	if (reach <= 0)
		return 0;
	attack = obj->size + ConsoleObject->size + i2f(2);
	dtu_get_body_cylinder(obj,&c,&r,&hh);
	reach -= F1_0;
	if (reach > attack - F1_0)
		reach = attack - F1_0;
	if (reach < r + DTU_BODY_RAD)
		reach = r + DTU_BODY_RAD;
	return reach;
}

// A hostile creature that sees the player does not walk nearer to the
// player than creature_stop_dist() (Dtu_improved_pathfinding), and a
// peaceful creature not nearer than its body radius plus that of the
// player (Dtu_fix_npc_body, dtuobj.h): the part of its velocity toward the
// player is cut so that it stops there. The test of a hit does not change,
// and the player can still walk up to the creature.
static void stop_at_player(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector d;
	fix stop, dist, toward, allowed, r, hh;

	if (ConsoleObject == NULL || Player_is_dead || Dtu_player_dead)
		return;
	dtu_get_body_size(obj,&r,&hh);
	if (is_hostile(obj)) {
		// only a creature that sees the player (not one behind a door)
		if (!improved_paths() || !Ail[obj-Objects].previous_visibility)
			return;
		if ((stop = creature_stop_dist(obj)) <= 0)
			return;
	}
	else if (Dtu_fix_npc_body)
		stop = r + DTU_BODY_RAD;
	else
		return;
	if (abs(obj->pos.y - ConsoleObject->pos.y) > hh + DTU_BODY_HALF_HEIGHT)
		return;

	vm_vec_sub(&d,&obj->pos,&ConsoleObject->pos);
	d.y = 0;
	dist = vm_vec_normalize(&d);
	if (dist < 0x100)
		return;
	toward = -vm_vec_dot(&pi->velocity,&d);
	if (toward <= 0 || dist - stop > fixmul(toward,FrameTime))
		return;
	allowed = (dist > stop && FrameTime > 0) ? fixdiv(dist - stop,FrameTime) : 0;
	if (toward > allowed)
		vm_vec_scale_add2(&pi->velocity,&d,toward - allowed);
}

//------------------------------------------------------------------------
// Movement of a creature (pmon.c, 0x9F153)

int opens_doors(object *obj);

// ASCENT: Dtu_fix_fall_land (dtuobj.h). -origobj clears it.
int Dtu_fix_fall_land = 1;

// 0x6C8C8 for a creature, verified in the bytes: the damage of a fall and
// the end of the fall. Only with the fall bit (bit 0x20 of +0x1AE) and none
// of the effects 0x200880 (Levitate, Fly, Feather Fall): fell = (the height
// of the start - y) >> 17 ("descent feet", half units). Above 10: fell / 10
// + rand() % (fell / 10 * 5 + 1), one half on a floor of the type 1
// (water, side 3 of the segment of the creature), and 0x29A9C(creature, no
// attacker, damage, -1). Then the height of the start becomes y and the
// fall bit is cleared. The splash vclip of a fall on water or lava
// (0x6C93C to 0x6C964) is not in the port, as for the player.
static void creature_fall_damage(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	// (0x29A9C does nothing to a dead body)
	if (ail->falling && !(obj->dtu.m_88 & (DTU_FX_LEVITATE|DTU_FX_FLY|DTU_FX_FEATHER_FALL)) &&
		 obj->shields >= F1_0 && DTU_ANIM_STATE(obj) != DAS_DIE) {
		int fell = (ail->fall_start_y - obj->pos.y) >> 17;

		if (fell > 10) {
			int base = fell/10;
			int damage = rand() % (base*5 + 1) + base;

			if ((Segments[obj->segnum].sides[WBOTTOM].pad & 3) == 1)
				damage >>= 1;
			dtu_obj_log("fall: obj %d %s fell %d descent feet, %d damage (0x6C8C8)",(int)(obj-Objects),
				dtu_object_name(obj),fell,damage);
			damage_creature_w(obj,NULL,i2f(damage),-1);
		}
	}
	ail->fall_start_y = obj->pos.y;
	ail->falling = 0;
}

// 0x6C662 for a creature: gravity and the lift out of the floor. Returns
// the distance of the feet to the floor.
static fix creature_gravity(object *obj,vms_vector *accel,fix radius,fix half_height)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix d = creature_floor_dist(obj,radius,half_height);
	// 0x6C74F to 0x6C798: no fall for a creature that flies and is not
	// dying, nor with the effects 0x200880 (bit 0x80 of +0x1AE, the climb
	// of the player, is not used for a creature in the port)
	int no_fall = (is_flying(obj) && DTU_ANIM_STATE(obj) != DAS_DIE) ||
		(obj->dtu.m_88 & (DTU_FX_LEVITATE|DTU_FX_FLY|DTU_FX_FEATHER_FALL));

	if (d < 0) {
		fix push = DTU_PUSH_UP_MAX, ad = -d;
		vms_vector new_pos;

		if (Dtu_fix_creature_move) {
			ail->push_acc += fixmuldiv(DTU_PUSH_UP_MAX,FrameTime,DTU_REF_FRAMETIME);
			if (ail->push_acc > DTU_PUSH_UP_MAX*4)
				ail->push_acc = DTU_PUSH_UP_MAX*4;
			push = ail->push_acc;
		}
		if (push > ad)
			push = ad;
		if (Dtu_fix_creature_move)
			ail->push_acc -= push;

		new_pos = obj->pos;
		new_pos.y += push;
		if (find_point_seg(&new_pos,obj->segnum) >= 0)
			obj->pos = new_pos;
		if (obj->mtype.phys_info.velocity.y < 0)
			obj->mtype.phys_info.velocity.y = 0;
		// 0x6C839: in the floor, the end of a fall
		if (!no_fall)
			creature_fall_damage(obj);
	}
	else {
		ail->push_acc = 0;
		// Dtu_fix_corpse_floor: a creature that flies falls when it dies.
		// The executable has no gravity for it, so its body stays where it
		// died, in the air.
		if (d != 0 && (!is_flying(obj) || (Dtu_fix_corpse_floor && DTU_ANIM_STATE(obj) == DAS_DIE)))
			accel->y -= fixmul(d,DTU_GRAVITY_K);
		// 0x6C871, 0x6C89A: less than 1.0 above the floor ends a fall with
		// no damage (Dtu_fix_fall_land: with the damage). 1.0 or more starts
		// one in 1.3 (0x6CB0F: the fall bit and the height of the start).
		// The port leaves that start out: its floor probe is thin (see
		// creature_floor_dist), so a creature that the knee lift puts on a
		// step is 1.0 or more above the floor under its center, and the fall
		// bit (x and z velocity / 8) stops it at the step. So only the trap
		// 0x9493A starts a fall of a creature in the port.
		if (!no_fall && d < F1_0) {
			if (Dtu_fix_fall_land && ail->falling)
				creature_fall_damage(obj);
			ail->falling = 0;
		}
	}
	return d;
}

// 0x6CB68 for a creature, see dtu_integrate() in physics.c.
static void creature_integrate(object *obj,vms_vector *accel)
{
	physics_info *pi = &obj->mtype.phys_info;
	int count;

	if (Dtu_fix_creature_move) {
		// All steps of 1/64 second. The part of a step that is left goes
		// to the next frame of this object.
		fix *acc = &Ail[obj-Objects].step_acc;

		*acc += FrameTime;
		count = *acc / DTU_FT;
		*acc -= count * DTU_FT;
		if (count > 32)
			count = 32;
	}
	else {
		count = FrameTime / DTU_FT;
		if (count > 2)
			count = 2;
	}

	while (count--) {
		vm_vec_add2(&pi->velocity,accel);
		vm_vec_scale(&pi->velocity,F1_0-pi->drag);
	}

	if (pi->velocity.y < DTU_TERMINAL_VEL)
		pi->velocity.y = DTU_TERMINAL_VEL;
}

// Dtu_fix_lava and Dtu_fix_fall_land: a creature that does not move
// (inactive, held, flag 0x40). 1.3 runs no physics for it. An inactive
// creature stands on the floor of its segment (0x13F96): when its feet are
// less than 1.0 above the floor below it, its fall ends (Dtu_fix_fall_land:
// with the damage of 0x6C8C8) and the lava of that floor burns it
// (Dtu_fix_lava). The floor query is made only for a creature that falls
// or that stands over lava.
static void creature_still(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int seg = Segments[obj->segnum].children[WBOTTOM], lava, falls;
	fix r, hh;

	if (obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	if (seg < 0)
		seg = obj->segnum;
	lava = Dtu_fix_lava && (Segments[seg].sides[WBOTTOM].pad & 3) == 3;
	falls = Dtu_fix_fall_land && ail->falling;
	if (!lava && !falls)
		return;
	dtu_get_body_size(obj,&r,&hh);
	if (creature_floor_dist(obj,r,hh) >= F1_0)
		return;
	if (falls)
		creature_fall_damage(obj);
	else
		ail->falling = 0;
	if (lava && !(obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		dtu_lava_frame(obj,-1,-1,1);
}

void dtu_creature_physics_sim(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector accel, start_pos, frame_vec, new_pos, center;
	fix radius, half_height, cyl_radius, cyl_half;
	int tries = 0, fate;
	int ignore_obj_list[MAX_IGNORE_OBJS], n_ignore_objs = 0;
	fvi_query fq;
	fvi_info hit;
	// Dtu_improved_pathfinding: a body that hits a wall slides along it for
	// the rest of the frame time (sim_time), as the ship of Descent 1 does.
	int imp = improved_paths(), hits = 0;
	fix sim_time = FrameTime, want = 0, moved = 0, floor_d;
	int lava_seg = -1, lava_side = -1;		// the wall or floor of the last move query (dtu_lava_frame)

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return;
	// An inactive creature does not move (0x9F196), nor one that is held
	// or has the object flag 0x40 (ASCENT: 0x9F174). Dtu_fix_lava: the lava
	// burns it all the same (1.3 burns only in the move, 0x9FE54).
	if ((obj->dtu.m_88 & DTU_OF_HELD) || ail->activity == 0 || dtu_creature_frozen(obj)) {
		if (Dtu_fix_lava || Dtu_fix_fall_land)
			creature_still(obj);
		return;
	}

	// ASCENT: the move query of a rat (0x9F153) starts in the body of the
	// player when the player walks over the rat: the object test of the
	// query (0x3EE70) takes a start point in a body as a hit, so the rat
	// touches the player (0x9FB4C, collide_robot_and_player 0x26FC1). The
	// player walks through the rat, its own queries skip it (0x3F38F). The
	// move of the port does not take a start point in a body as a hit, so
	// this is the same test with the body of the player (dtu_player_body).
	if (ConsoleObject && ConsoleObject->type == OBJ_PLAYER && dtu_rat_type(obj->id) && DTU_ANIM_STATE(obj) != DAS_DIE) {
		dtu_body b;
		vms_vector d;
		fix across, r, hh;

		dtu_player_body(&b);
		dtu_get_body_size(obj,&r,&hh);
		vm_vec_sub(&d,&obj->pos,&ConsoleObject->pos);
		across = fix_sqrt(fixmul(d.x,d.x) + fixmul(d.z,d.z));
		if (across < b.move_rad + r && d.y < b.move_rad2 + hh && d.y > -(b.floor_height + hh)) {
			collide_two_objects(obj,ConsoleObject,&obj->pos);
			if (obj->flags & OF_SHOULD_BE_DEAD || DTU_ANIM_STATE(obj) == DAS_DIE)
				return;
		}
	}

	// 0x9F286: the velocity becomes one half on each frame, and the AI
	// adds direction * speed on each frame (0x11EC9). So the velocity goes
	// to the speed of the animation state: v' = (v + add) / 2. The
	// corrected rule does the same step for the reference frame time.
	if (Dtu_fix_creature_move) {
		// factor = 0.5 ^ (FrameTime / DTU_REF_FRAMETIME), from 16 steps
		static const fix half_pow[17] = { 0x10000, 0xf525, 0xeac0, 0xe0cc, 0xd744, 0xce24, 0xc567, 0xbd08,
			0xb504, 0xad58, 0xa5fe, 0x9ef5, 0x9837, 0x91c3, 0x8b95, 0x85aa, 0x8000 };
		fix t = fixdiv(FrameTime,DTU_REF_FRAMETIME);
		fix f = F1_0;

		while (t >= F1_0) {
			f >>= 1;
			t -= F1_0;
		}
		f = fixmul(f,half_pow[(t >> 12) & 15]);
		vm_vec_scale(&pi->velocity,f);
		if (ail->has_move)
			vm_vec_scale_add2(&pi->velocity,&ail->move_add,F1_0-f);
	}
	else {
		if (ail->has_move)
			vm_vec_add2(&pi->velocity,&ail->move_add);
		vm_vec_scale(&pi->velocity,F1_0/2);
	}
	ail->has_move = 0;

	// 0x6CFF8
	weapon_test(obj);
	if (obj->control_type != CT_AI)
		return;

	dtu_get_body_size(obj,&radius,&half_height);
	dtu_get_body_cylinder(obj,&center,&cyl_radius,&cyl_half);

	// A body that dies goes down with its box. (guess: the executable
	// calls the gravity function from the animation step of DIE, 0x112A5)
	if (DTU_ANIM_STATE(obj) == DAS_DIE) {
		polymodel *po = obj_model(obj);
		int frame = DTU_ANIM_FRAME(obj);

		if (po && po->body_half_height && frame >= 0 && frame < po->n_frames) {
			vms_vector mn, mx;
			fix depth;

			half_height = po->body_half_height[frame+1];
			// Dtu_fix_corpse_floor: the depth of the lowest drawn point of
			// the frame (see corpse_depth())
			if (Dtu_fix_corpse_floor && (depth = corpse_depth(obj,frame,&mn,&mx)) >= 0)
				half_height = depth;
		}
	}

	vm_vec_copy_scale(&accel,&pi->thrust,fixdiv(F1_0,pi->mass));

	floor_d = creature_gravity(obj,&accel,cyl_radius,half_height);

	// A dying creature that does not fly falls: one eighth of the velocity
	// in x and z on each frame (0x9F4E2).
	if ((!is_flying(obj) || Dtu_fix_corpse_floor) && obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD)
		ail->falling = 1;
	if (ail->falling) {
		pi->velocity.x /= 8;
		pi->velocity.z /= 8;
	}

	creature_integrate(obj,&accel);

	if (!is_flying(obj))
		vm_vec_zero(&pi->thrust);

	if (DTU_ANIM_STATE(obj) != DAS_DIE)
		stop_at_player(obj);

	start_pos = obj->pos;
	want = fixmul(vm_vec_mag(&pi->velocity),FrameTime);

	for (;;) {

		// 0x9F67C
		if (vm_vec_mag(&pi->velocity) <= 10) {
			ail->retry_count = 0;
			break;
		}

		frame_vec.x = fixmul(pi->velocity.x,sim_time);
		frame_vec.y = fixmul(pi->velocity.y,sim_time);
		frame_vec.z = fixmul(pi->velocity.z,sim_time);

		if (++tries > 5) {
			ail->retry_count = 1;
			break;
		}

		vm_vec_add(&new_pos,&obj->pos,&frame_vec);

		// 0x9F757: after more than 30 blocked frames the creature can walk
		// through its blocker if the step leads away from it.
		if (ail->blocked_count > 30 && ail->blocker >= 0 && n_ignore_objs < MAX_IGNORE_OBJS-1) {
			vms_vector np = new_pos;

			np.y = obj->pos.y;
			if (vm_vec_dist(&np,&Objects[ail->blocker].pos) > vm_vec_dist(&obj->pos,&Objects[ail->blocker].pos))
				ignore_obj_list[n_ignore_objs++] = ail->blocker;
			else
				ail->retry_count = 1;
		}

		ignore_obj_list[n_ignore_objs] = -1;

		// The radius of the query is one fourth of the radius of the body
		// (0x9F605).
		fq.p0 = &obj->pos;
		fq.p1 = &new_pos;
		fq.startseg = obj->segnum;
		fq.rad = cyl_radius/4;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = ignore_obj_list;
		fq.flags = (DTU_ANIM_STATE(obj) == DAS_DIE) ? 0 : FQ_CHECK_OBJS;

		Fvi_dtu_body = 1;
		Fvi_dtu_rad2 = half_height;
		// Dtu_improved_pathfinding: the top side is tested at the top of the
		// body (half height above the center). The executable tests it at
		// 1.5 times the half height, so a tall creature (an ogre, 28.7 from
		// the floor) cannot enter a corridor that its body fits in.
		if (imp)
			Fvi_dtu_rad2 = half_height*2/3;
		fate = find_vector_intersection(&fq,&hit);
		Fvi_dtu_body = 0;

		// 0x9F860: the hit side of the query goes to 0x30F494 (the side
		// that the lava test reads)
		Dtu_lava_hit_side = hit.hit_side;
		lava_seg = (fate == HIT_WALL) ? hit.hit_side_seg : -1;
		lava_side = hit.hit_side;

		if (fate == HIT_WALL && hit.hit_side == WBOTTOM)
			fate = 5;		// the floor
		if (fate != HIT_NONE) {
			int k = (fate == 5) ? 2 : (fate == HIT_OBJECT) ? 3 : (fate == HIT_WALL && hit.hit_side == WTOP) ? 1 : 0;

			ail->log_hits[k]++;
			if (k == 0)
				ail->log_wall_time = GameTime;
			if (fate != HIT_OBJECT) {
				ail->log_hit_seg = hit.hit_side_seg;
				ail->log_hit_side = hit.hit_side;
			}
			else
				ail->log_hit_obj = hit.hit_object;
		}

		if (fate == HIT_NONE) {
			int segnum = find_point_seg(&new_pos,obj->segnum);

			if (segnum != -1) {
				moved += vm_vec_dist(&obj->pos,&new_pos);
				obj->pos = new_pos;
				if (segnum != obj->segnum)
					obj_relink(objnum,segnum);
			}
			ail->retry_count = 0;
			ail->blocked_count = 0;
			ail->blocker = -1;
			// a slide that did not get far: the AI may take another way
			if (imp && hits && moved*4 < want)
				ail->retry_count = 1;
			break;
		}
		else if (fate == 5) {				// 0x6CD45
			vms_vector c1;
			fix feet = obj->pos.y - half_height, diff;

			// (obj->pos is start_pos here, but after a slide)
			if (!imp)
				obj->pos = start_pos;

			// A floor that is less than 5.5 above the feet lifts the
			// creature (0x6C186, see dtu_knee_high() in physics.c).
			compute_center_point_on_side(&c1,&Segments[hit.hit_side_seg],hit.hit_side);
			diff = c1.y - feet;
			if (diff > 0 && diff < DTU_KNEE_HIGH)
				pi->velocity.y = c1.y - obj->pos.y + half_height + DTU_KNEE_LIFT;
			else {
				fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

				vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
			}
			// 0x9F903: the floor ends a fall (0x6C8C8)
			creature_fall_damage(obj);
			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;
		}
		else if (fate == HIT_WALL && hit.hit_side == WTOP && !imp) {
			// 0x9F97A: a hit on a top side (or a bottom side, above) is
			// not a wall hit.
			obj->pos = start_pos;
			ail->retry_count = 0;
			break;
		}
		else if (fate == HIT_WALL) {		// 0x9E88D
			fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

			// collide_robot_and_wall (0x25CAA): a creature with the door
			// bit opens a door that it walks into. The door must be closed,
			// with no key and no lock. This is the only place where a
			// creature opens a door.
			if (opens_doors(obj) && hit.hit_side_seg >= 0 && hit.hit_side >= 0) {
				int wn = Segments[hit.hit_side_seg].sides[hit.hit_side].wall_num;

				if (wn >= 0 && Walls[wn].type == WALL_DOOR && Walls[wn].keys == KEY_NONE &&
					 Walls[wn].state == WALL_DOOR_CLOSED && !(Walls[wn].flags & WALL_DOOR_LOCKED)) {
					dtu_obj_log("obj %d %s: opens the door seg %d side %d (wall %d)",objnum,dtu_object_name(obj),
						hit.hit_side_seg,hit.hit_side,wn);
					wall_open_door(&Segments[hit.hit_side_seg],hit.hit_side);
				}
			}
			// D-181: 0x25D35 to 0x25D43 of collide_robot_and_wall: a type
			// with the flag 0x20 of Robot_info+0x98 (flying) gets 0 in the
			// circle distance of the difficulty (Robot_info+0x48, the
			// third group of $MONSTER_AI, turn_time in the port), for all
			// creatures of the type, until the tables are read again.
			// Dtu_fix_wall_circle (dtuobj.c) keeps the table.
			if (!Dtu_fix_wall_circle && obj->type == OBJ_ROBOT && obj->id < N_robot_types && (Monster_info[obj->id].flags & MONSTER_F_FLYING))
				Robot_info[obj->id].turn_time[Difficulty_level] = 0;

			if (imp) {
				// Dtu_improved_pathfinding: the body goes to the point where
				// it touches the wall (or the ceiling), loses the part of the
				// velocity into the wall, and moves on with the rest of the
				// frame time. The executable takes 1.75 times that part away
				// and stops the body for the frame, so a body that walks into
				// the corner of an opening stays at the corner.
				fix len = vm_vec_mag(&frame_vec);
				int seg = find_point_seg(&hit.hit_pnt,obj->segnum);

				hits++;
				if (seg >= 0 && len > 0) {
					fix m = vm_vec_dist(&obj->pos,&hit.hit_pnt);

					if (m > len)
						m = len;
					moved += m;
					sim_time -= fixmul(sim_time,fixdiv(m,len));
					obj->pos = hit.hit_pnt;
					if (seg != obj->segnum)
						obj_relink(objnum,seg);
				}
				if (d < 0)
					vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
				if (sim_time <= 0)
					break;
				continue;
			}
			vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,fixmul(-d,DTU_WALL_BOUNCE));
			obj->pos = start_pos;
			ail->retry_count = 1;
			break;
		}
		else if (fate == HIT_OBJECT) {	// 0x9F9C0 and 0x9FB56
			object *other = &Objects[hit.hit_object];

			if (!imp)
				obj->pos = start_pos;
			ail->retry_count = ai_tick(objnum,2);

			if (ail->blocker != hit.hit_object) {
				ail->blocker = hit.hit_object;
				ail->blocked_count = 1;
			}
			else if (!Dtu_fix_creature_move || Ref_ticks_now > 0)
				ail->blocked_count++;

			// 0x9FA41: the creature pushes scenery when its strength is
			// above the value "pushable"
			if (other->type == OBJ_DRESSING)
				push_dressing(obj,other,Monster_info[obj->id].str);

			collide_two_objects(obj,other,&hit.hit_pnt);

			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;
			if (other->type == OBJ_ROBOT && n_ignore_objs < MAX_IGNORE_OBJS-1) {
				ignore_obj_list[n_ignore_objs++] = hit.hit_object;
				continue;
			}
			// Dtu_improved_pathfinding: the body goes around scenery. It
			// loses the part of the velocity toward the center of the
			// scenery and tries again.
			if (imp && other->type == OBJ_DRESSING && !(other->flags & OF_SHOULD_BE_DEAD)) {
				vms_vector n;
				fix vn;

				vm_vec_sub(&n,&obj->pos,&other->pos);
				n.y = 0;
				if (vm_vec_normalize(&n) > 0x100 && (vn = vm_vec_dot(&pi->velocity,&n)) < 0) {
					hits++;
					vm_vec_scale_add2(&pi->velocity,&n,-vn);
					continue;
				}
			}
			// The body stops at the player and at scenery.
			pi->velocity.x = pi->velocity.z = 0;
			break;
		}
		else {									// "bad p0"
			vm_vec_negate(&pi->velocity);
			ail->retry_count = 1;
			break;
		}
	}

	if (get_seg_masks(&obj->pos,obj->segnum,0).centermask != 0) {
		if (!update_object_seg(obj)) {
			int n = find_point_seg(&start_pos,obj->segnum);

			if (n != -1) {
				obj->pos = start_pos;
				obj_relink(objnum,n);
			}
		}
	}

	// ASCENT: 0x9FCF1, the footstep of a walking creature (dtufoot.c)
	if (ail->activity == 2)
		dtufoot_creature(obj);

	// 0x9FE54: the lava
	if (!(obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		dtu_lava_frame(obj,lava_seg,lava_side,floor_d < F1_0);
}

//------------------------------------------------------------------------
// The lava (0x3D2BD)

// ASCENT: 0x3D2BD, called at the end of the move of the player (0x9E7B8)
// and of a creature (0x9FE54), verified in the bytes. Not for a creature
// with fire_immune (Robot_info+0x98 bit 0x08, MONSTER_F_FIRE_IMMUNE of the
// port) and not while bit 0x20, 0x40 or 0x80 of +0x1AE is set (the fall,
// the jump, the climb or Levitate). The segment is the one below the
// object (children[3]) or, when there is none, the segment of the object.
// The side is the hit side of the last move query (0x30F494), side 3 when
// that is -1. When the side flags & 3 of that side are 3 (lava, 0xA6F89)
// and the clock of the player (the dword at 0x1D339F of Players[Player_num],
// time_level in the decompilation) is 1.0 or more away
// from the time of the last burn (0x13B1B4), that time is set, the fire die
// count of Fireball becomes 1 (0x1B5EA4) and 0x911AA(object, object, 16, 1)
// burns it: 1d6 of fire, a save for half.
//
// Dtu_fix_lava (1, default; -origobj clears it). 1.3 has one time of the
// last burn for all objects, so the player and a creature in the lava, or
// two creatures, share one burn each second. The side is the hit side of
// the last query of any object (the player or another creature), as the
// index of a side of the segment below the object, and the move of an
// object that stands still makes no query. So whether an object that stands
// in the lava burns depends on what other objects hit, and a creature or
// a player that flies over the lava burns. The corrected rule: each object
// has its own time of the last burn; it burns when the wall or the floor
// that its own move hit in this frame is lava, or when its feet are less
// than 1.0 above the floor (on_floor) and that floor (side 3 of the
// segment below, or of its segment) is lava; and a creature that does not
// move (inactive, held, flag 0x40) burns too (creature_still). 1.3
// only burns in the move.
int Dtu_fix_lava = 1;
int Dtu_lava_hit_side = -1;		// 0x30F494

static int side_is_lava(int segnum,int side)
{
	if (segnum < 0 || segnum > Highest_segment_index || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return 0;
	return (Segments[segnum].sides[side].pad & 3) == 3;
}

void dtu_lava_frame(object *obj,int hit_seg,int hit_side,int on_floor)
{
	static fix last_burn = 0;		// 0x13B1B4
	static fix player_burn = 0;		// Dtu_fix_lava: the time of the player
	fix now = Players[Player_num].time_level, *last, d;
	int seg, lava;

	if (obj == NULL || obj->segnum < 0 || obj->segnum > Highest_segment_index)
		return;
	if (obj->type == OBJ_ROBOT) {
		if (obj->id >= N_robot_types || (Monster_info[obj->id].flags & MONSTER_F_FIRE_IMMUNE))
			return;
		if (Ail[obj-Objects].falling || obj->shields < F1_0 || DTU_ANIM_STATE(obj) == DAS_DIE)
			return;
	}
	else if (obj->type == OBJ_PLAYER) {
		if (Dtu_player_state & (DTU_PS_FALLING|DTU_PS_JUMPING|DTU_PS_CLIMBING))
			return;
		if (Player_is_dead || Dtu_player_dead)
			return;
	}
	else
		return;

	seg = Segments[obj->segnum].children[WBOTTOM];
	if (seg < 0)
		seg = obj->segnum;
	if (!Dtu_fix_lava)
		lava = side_is_lava(seg,Dtu_lava_hit_side < 0 ? WBOTTOM : Dtu_lava_hit_side);
	else {
		lava = (hit_seg >= 0 && side_is_lava(hit_seg,hit_side)) || (on_floor && side_is_lava(seg,WBOTTOM));
	}
	if (!lava)
		return;

	last = !Dtu_fix_lava ? &last_burn : (obj->type == OBJ_PLAYER) ? &player_burn : &Ail[obj-Objects].lava_time;
	d = now - *last;
	if (d < 0)
		d = -d;
	if (d < F1_0)
		return;
	*last = now;
	dtu_obj_log("lava: obj %d %s burns in seg %d (0x3D2BD)",(int)(obj-Objects),
		obj->type == OBJ_PLAYER ? "player" : dtu_object_name(obj),obj->segnum);
	dtu_spell_lava(obj);
}
