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
// player than creature_stop_dist(): the part of its velocity toward the player is cut
// so that it stops there. The test of a hit does not change, and the
// player can still walk up to the creature.
static void stop_at_player(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector d;
	fix stop, dist, toward, allowed, r, hh;

	if (!is_hostile(obj) || ConsoleObject == NULL || Player_is_dead || Dtu_player_dead)
		return;
	// only a creature that sees the player (not one behind a door)
	if (!Ail[obj-Objects].previous_visibility)
		return;
	if ((stop = creature_stop_dist(obj)) <= 0)
		return;
	dtu_get_body_size(obj,&r,&hh);
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

// ASCENT: Dtu_improved_pathfinding. The direction from the nearest point of
// the edges of a side to p. fvi gives the normal of the face also when the
// body touches an edge of the side (the corner of an opening, the rim of a
// step), and a slide along the face does not leave the edge. For a body
// that walks the normal of an upright edge (a corner) is level. Returns 0
// if p is on an edge.
static int side_edge_normal(int segnum,int side,vms_vector *p,int level,vms_vector *n)
{
	segment *segp = &Segments[segnum];
	double px = f2fl(p->x), py = f2fl(p->y), pz = f2fl(p->z), best = -1, bx = 0, by = 0, bz = 0, m;
	int k, upright = 0;

	for (k=0;k<4;k++) {
		vms_vector *a = &Vertices[segp->verts[Side_to_verts[side][k]]];
		vms_vector *b = &Vertices[segp->verts[Side_to_verts[side][(k+1)&3]]];
		double ax = f2fl(a->x), ay = f2fl(a->y), az = f2fl(a->z);
		double ex = f2fl(b->x) - ax, ey = f2fl(b->y) - ay, ez = f2fl(b->z) - az;
		double ll = ex*ex + ey*ey + ez*ez, t = 0, dx, dy, dz, d;

		if (ll > 1e-9)
			t = ((px-ax)*ex + (py-ay)*ey + (pz-az)*ez) / ll;
		if (t < 0) t = 0;
		if (t > 1) t = 1;
		dx = px - (ax + t*ex); dy = py - (ay + t*ey); dz = pz - (az + t*ez);
		d = dx*dx + dy*dy + dz*dz;
		if (best < 0 || d < best) {
			best = d;
			bx = dx; by = dy; bz = dz;
			upright = (ey*ey > 0.5*ll);
		}
	}
	if (best <= 1e-6)
		return 0;
	if (level && upright && bx*bx + bz*bz > 1e-6)
		by = 0;
	m = sqrt(bx*bx + by*by + bz*bz);
	n->x = fl2f(bx/m); n->y = fl2f(by/m); n->z = fl2f(bz/m);
	return 1;
}

// 1 if the side is the rim of a step for feet at the height "feet": its
// segment is open at the top and the top edge of the side is above the feet
// by less than a knee high. *top is the height of the top edge.
static int step_rim(int segnum,int side,fix feet,fix *top)
{
	segment *segp;
	int k;

	if (segnum < 0 || side < 0 || side == WTOP || side == WBOTTOM)
		return 0;
	segp = &Segments[segnum];
	if (segp->children[WTOP] < 0 || segp->children[side] >= 0)
		return 0;
	*top = Vertices[segp->verts[Side_to_verts[side][0]]].y;
	for (k=1;k<4;k++)
		if (Vertices[segp->verts[Side_to_verts[side][k]]].y > *top)
			*top = Vertices[segp->verts[Side_to_verts[side][k]]].y;
	return *top > feet && *top - feet < DTU_KNEE_HIGH;
}

// 1 if the hit of a move is behind the start of the body and the move goes
// away from the side (from its face and from its nearest edge).
static int moves_off_side(object *obj,vms_vector *frame_vec,fvi_info *hit)
{
	vms_vector to_hit, e;

	if (hit->hit_side_seg < 0 || hit->hit_side < 0)
		return 0;
	vm_vec_sub(&to_hit,&hit->hit_pnt,&obj->pos);
	if (vm_vec_dot(&to_hit,frame_vec) > 0)
		return 0;
	if (vm_vec_dot(frame_vec,&hit->hit_wallnorm) <= 0)
		return 0;
	if (side_edge_normal(hit->hit_side_seg,hit->hit_side,&obj->pos,0,&e) && vm_vec_dot(frame_vec,&e) <= 0)
		return 0;
	return 1;
}

// 0x6C662 for a creature: gravity and the lift out of the floor.
static void creature_gravity(object *obj,vms_vector *accel,fix radius,fix half_height)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix d = creature_floor_dist(obj,radius,half_height);

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
	}
	else {
		ail->push_acc = 0;
		// Dtu_fix_corpse_floor: a creature that flies falls when it dies.
		// The executable has no gravity for it, so its body stays where it
		// died, in the air.
		if (d != 0 && (!is_flying(obj) || (Dtu_fix_corpse_floor && DTU_ANIM_STATE(obj) == DAS_DIE)))
			accel->y -= fixmul(d,DTU_GRAVITY_K);
	}
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
	fix sim_time = FrameTime, want = 0, moved = 0;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	if (obj->dtu.m_88 & DTU_OF_HELD)
		return;
	// An inactive creature does not move (0x9F196).
	if (ail->activity == 0)
		return;
	if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return;
	// ASCENT: 0x9F174, object flag 0x40: no move
	if (dtu_creature_frozen(obj))
		return;

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

	creature_gravity(obj,&accel,cyl_radius,half_height);

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

	if (imp && DTU_ANIM_STATE(obj) != DAS_DIE)
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

		if (fate == HIT_WALL && hit.hit_side == WBOTTOM)
			fate = 5;		// the floor
		// Dtu_improved_pathfinding: a body that is in the radius of a side
		// already (it gets there at the edge of a side of a segment below
		// or above it) and moves away from the side: fvi takes the move as
		// a hit behind the start, and the body did not leave the place.
		if (imp && fate == HIT_WALL && moves_off_side(obj,&frame_vec,&hit))
			fate = HIT_NONE;
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
				int seg = find_point_seg(&hit.hit_pnt,obj->segnum), back = 0;
				vms_vector n = hit.hit_wallnorm;

				hits++;
				// A hit point behind the start (the body touches the edge of
				// a side already): the body stays where it is, as in the
				// physics of Descent 1 ("moved backwards"). The port moved it
				// back to the hit point, and it went back and forth at the
				// corner of an opening.
				if (seg >= 0 && len > 0) {
					vms_vector to_hit;

					vm_vec_sub(&to_hit,&hit.hit_pnt,&obj->pos);
					if (vm_vec_dot(&to_hit,&frame_vec) <= 0)
						back = 1;
				}
				// The body at the edge of a side (a velocity that does not go
				// into the face): it slides around the edge, not along the
				// face.
				if (d >= 0 && hit.hit_side_seg >= 0 && hit.hit_side >= 0 &&
					 side_edge_normal(hit.hit_side_seg,hit.hit_side,back ? &obj->pos : &hit.hit_pnt,!is_flying(obj),&n))
					d = vm_vec_dot(&pi->velocity,&n);
				if (back)
					seg = -1;
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
				if (d < 0) {
					fix top;

					vm_vec_scale_add2(&pi->velocity,&n,-d);
					// The rim of a step: a side of a segment that is open at the
					// top, with its top edge less than a knee high above the
					// feet (a pit under the floor, BONES segment 53). The body
					// steps up onto the rim, 2.0 at most for each hit (the
					// lift out of the floor of creature_gravity()).
					if (!is_flying(obj) && step_rim(hit.hit_side_seg,hit.hit_side,obj->pos.y - half_height,&top)) {
						vms_vector up = obj->pos;
						fix lift = top + half_height + F1_0/4 - obj->pos.y;

						if (lift > DTU_PUSH_UP_MAX)
							lift = DTU_PUSH_UP_MAX;
						up.y += lift;
						if (lift > 0 && (seg = find_point_seg(&up,obj->segnum)) >= 0) {
							obj->pos = up;
							if (seg != obj->segnum)
								obj_relink(objnum,seg);
							if (pi->velocity.y < 0)
								pi->velocity.y = 0;
						}
					}
				}
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
					fix before = fixmul(pi->velocity.x,pi->velocity.x) + fixmul(pi->velocity.z,pi->velocity.z);

					hits++;
					vm_vec_scale_add2(&pi->velocity,&n,-vn);
					// Head on (less than a fourth of the speed is left): the
					// body stops, so that the blocked move attacks the scenery
					// (0x14CA9). The slide left it no speed, which cleared the
					// retry count, and the creature stood at the scenery.
					if ((fixmul(pi->velocity.x,pi->velocity.x) + fixmul(pi->velocity.z,pi->velocity.z))*16 >= before)
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
}
