/*
 * ASCENT: new file, split from dtuobj.c. The scenery: the scenery that
 * breaks, the end of the game at the Flamesword, the pushed scenery and
 * the collision of two objects. The shared declarations are in
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
#include "dtusolid.h"	// ASCENT: Dtu_improved_physics, scenery as solid bodies
#include "physics.h"		// ASCENT: Dtu_player_walk, DTU_FIX_JUMP_GRAVITY
#include "digi.h"
#include <math.h>

// Scenery that moves (movement type 1 of the executable): pushed scenery
// (0x9FA41, 0x9E5BE) and the scenery that falls after a break (0xA6ACB).
// pcons.c 0x9FE68 moves it.
static ubyte Dressing_fall[MAX_OBJECTS];
static int Dressing_sig[MAX_OBJECTS];
static int Dressing_str[MAX_OBJECTS];		// strength of the push (Dtu_fix_push_body)
static vms_vector Dressing_vpush[MAX_OBJECTS];	// velocity of the mover at the push (Dtu_fix_push_rate)
static vms_vector Dressing_slide[MAX_OBJECTS];	// velocity of the slide at the end of a push (Dtu_fix_push_rate)
static ubyte Dressing_slid[MAX_OBJECTS];		// the slide of this push has started
static ubyte Dressing_awake[MAX_OBJECTS];		// ASCENT: Dtu_improved_physics, the support of the scenery is tested in the next frame
static fix Dressing_y_slack = 0;			// ASCENT: Dtu_improved_physics, bodies that overlap less than this in height are not in the way
// Pushed scenery and the bodies of the player, the creatures and other
// scenery. The executable stops the move of scenery whose center is inside
// of the body of the player (twice the radius of its move query, plus half
// of the radius of the scenery), and the player stops closer than that to
// a chair unless its last step was long: the push of a chair works only
// below about 30 frames per second (docs/re/objects.md). 1 (default): a
// body that the scenery starts in does not stop a move away from it, and
// the player is as large as its move query (dressing_hit_object). 0
// (-origobj): the rule of the executable.
int Dtu_fix_push_body = 1;
// Pushed scenery and the walls. 1 (default): the box of the model stays
// out of the walls (dressing_wall_frac). 0 (-origobj): the center with
// half of the radius of the body, a table goes into a wall.
int Dtu_fix_push_wall = 1;
// The lead of pushed scenery at the end of a push. With the rule of the
// executable the scenery ends up about 1.84 steps of the mover ahead of
// the place where the mover stopped (the gap of the last hit, 0.2 to 1.2
// steps, and the 8/7 step of the move after it), and a step is the
// velocity times the frame time: 4 units at 20 frames per second, 0.4 at
// 200. 1 (default): at the end of a push the scenery slides the
// difference to the step of the reference frame (DTU_REF_FRAMETIME), so
// that a push moves scenery as far at each frame rate as at 21 frames per
// second. 0 (-origobj): no slide.
int Dtu_fix_push_rate = 1;
#define DTU_PUSH_LEAD	1.3

// The scenery that does not break. The executable ignores the keyword
// cant_break (0x229FC) and has the bit 0x04 of Dressing_info+0x04 only
// for a few objects of some levels (0xAD3AF). 1 (default): cant_break
// works. 0 (-origobj): every scenery object can break.
int Dtu_fix_cant_break = 1;

int dtu_dressing_unbreakable(object *d)
{
	if (d->type != OBJ_DRESSING || d->id >= N_dressing_types)
		return 1;
	if (Dressing_info[d->id].flags & DRESSING_F_LEVEL_FIX)
		return 1;
	return Dtu_fix_cant_break && (Dressing_info[d->id].flags & DRESSING_F_CANT_BREAK);
}

//------------------------------------------------------------------------
// The end of the game (0xA6786, called by 0xA6ACB for object 3 of level
// 0x20, the Flamesword). Verified in the disassembly 0xA6786 to 0xA6ACB:
// 1. 0x40974 and 0x34B26: the full view and the hands of the interface
//    (the port draws them each frame). 0x417ED and 0x40E50: the player
//    stops and its controls and keys are cleared. 0xD74C3 and 0x6B088:
//    the orientation of the player is made again from its forward vector.
// 2. A light object (0x68AC8: type 13, id 0, the segment and the place of
//    the sword, control type 14 CT_LIGHT): lifeleft 0x3FFFFFFF, the
//    intensity (+0x140) 0, +0x1AE bit 2, object flags 0xC0. "DLOG:
//    couldn't make light!!!" when the object list is full.
// 3. While the spin rate y of the sword (+0x100, 0x147 from the level
//    fixes) is not above 0x2666, one game frame 0x435A6(1, 0) for each
//    step: the view bob byte 0x14530D = 0, the velocity z of the player =
//    9.0, the spin rate y + 0x41, the intensity of the light + 3.0 (the
//    word +0x142), spin_object (0x69777) when the sword spins (movement
//    type 3, so it turns twice in a frame). 147 steps: light 441.0, spin
//    0x269A.
// 4. The explode_sound of the sword type (Dressing_info +0x0A) linked to
//    the sword, forever, volume 1.0 (0x2E600 makes it 0.5). Render type
//    of the sword 0 (it is gone), object flag 0x40.
// 5. 50 steps, one game frame each: an explosion of vclip 0 at the sword
//    (0x3BD4C), size 1.0 to 10.0 (rand() % 10 + 1), its +0x1AE bit 2 and
//    flag 0x40; x and z += rand() % 11.0 - 3.0, y += rand() % 0.6875 -
//    0.6875 (rand() is at most 0x7FFF, so the modulo does nothing and the
//    explosions are 3.0 to 2.5 to the side and 0.69 to 0.19 lower, kept as
//    in dtu_break_dressing); sound 0xA8D + rand() % 5 (2701 to 2705)
//    linked to the explosion at volume rand() % 1.0 (0 to 0.5).
// 6. With no digital sound (0x2EF31) the function returns: no movie and
//    the game goes on. Else stop_time, the streams stop, the sound of
//    object 3 stops (0x2E816(3)), frames while the music or the sound
//    stream plays and while keys come, CREDITV1.MVE (0x96439), Function_
//    mode 6 (the front end), start_time.
// The port steps 3 and 5 once in each reference tick (DTU_REF_FRAMETIME,
// 3/64 s, the corrected rules of the AI), so the sequence takes 9.2 s at
// each frame rate. The player keeps the controls of a game frame, as in
// 1.3 (0x435A6 reads them). The port plays the movie also with no sound.
// The test log lines: "end sequence: ..." (tools/questtest.py).

static int End_seq_step = 0;		// 0 none, 1 the spin, 2 the explosions, 3 done
static int End_seq_light = -1, End_seq_light_sig;
static int End_seq_n, End_seq_sounds;
static fix End_seq_acc;

int spin_object(object *obj);		// object.c

void end_sequence_reset(void)
{
	End_seq_step = 0;
	End_seq_light = -1;
}

static void end_sequence_start(object *sword)
{
	object *p = ConsoleObject;
	vms_vector f;
	int n;

	if (End_seq_step)
		return;
	End_seq_step = 1;
	End_seq_n = End_seq_sounds = 0;
	End_seq_acc = 0;
	if (p) {
		vm_vec_zero(&p->mtype.phys_info.velocity);			// 0x417ED
		f = p->orient.fvec;
		vm_vector_2_matrix(&p->orient,&f,NULL,NULL);		// 0xD74C3
		check_and_fix_matrix(&p->orient);					// 0x6B088
	}
	game_flush_inputs();										// 0x40E50
	n = obj_create(OBJ_LIGHT,0,sword->segnum,&sword->pos,NULL,0,CT_LIGHT,MT_NONE,RT_NONE);
	End_seq_light = n;
	if (n < 0)
		printf("end sequence: DLOG: couldn't make light!!!\n");
	else {
		object *l = &Objects[n];

		l->lifeleft = 0x3FFFFFFF;
		l->ctype.light_info.intensity = 0;
		l->flags |= OF_DTU_NOHIT;
		End_seq_light_sig = l->signature;
	}
	printf("end sequence: start (0xA6786), sword in seg %d, light obj %d, spin 0x%x\n",sword->segnum,n,sword->mtype.spin_rate.y);
	dtu_obj_log("end sequence (0xA6786): light obj %d",n);
}

static object *end_sequence_light(void)
{
	if (End_seq_light < 0 || Objects[End_seq_light].type != OBJ_LIGHT || Objects[End_seq_light].signature != End_seq_light_sig)
		return NULL;
	return &Objects[End_seq_light];
}

// Each game frame while the sequence runs
void end_sequence_frame(void)
{
	object *s = &Objects[3], *p = ConsoleObject, *l = end_sequence_light();

	if (End_seq_step == 0 || End_seq_step == 3)
		return;
	End_seq_acc += FrameTime;
	if (End_seq_step == 1) {
		if (p)
			p->mtype.phys_info.velocity.z = i2f(9);
		if (s->movement_type == MT_SPINNING)
			spin_object(s);									// 0x69777
		while (End_seq_acc >= DTU_REF_FRAMETIME && End_seq_step == 1) {
			End_seq_acc -= DTU_REF_FRAMETIME;
			if (s->mtype.spin_rate.y > 0x2666) {
				dressing_info *di = (s->id < N_dressing_types) ? &Dressing_info[s->id] : NULL;

				printf("end sequence: %d steps, light %.1f, spin 0x%x\n",End_seq_n,l ? f2fl(l->ctype.light_info.intensity) : 0.0,
					s->mtype.spin_rate.y);
				printf("end sequence: sound %d on the sword\n",di ? di->explode_sound : -1);
				if (di && di->explode_sound >= 0 && di->explode_sound < MAX_SOUNDS)
					digi_link_sound_to_object2(di->explode_sound,3,1,F1_0/2,i2f(200));		// 0x2E600(sound, 3, 1, 1.0)
				s->render_type = RT_NONE;
				s->flags |= OF_DTU_NOHIT;
				End_seq_step = 2;
				End_seq_n = 0;
				break;
			}
			s->mtype.spin_rate.y += 0x41;
			if (l)
				l->ctype.light_info.intensity += i2f(3);
			End_seq_n++;
		}
	}
	while (End_seq_step == 2 && End_seq_acc >= DTU_REF_FRAMETIME) {
		int e = -1, size = i2f(rand() % 10 + 1), snd;
		fix vol;

		End_seq_acc -= DTU_REF_FRAMETIME;
		if (Vclip[0].num_frames > 0 && Vclip[0].play_time > 0 && s->segnum >= 0) {
			object *x = object_create_explosion(s->segnum,&s->pos,size,0);

			if (x) {
				e = x - Objects;
				x->flags |= OF_DTU_NOHIT;
				x->pos.x += (rand() % i2f(11)) - i2f(3);
				x->pos.y += (rand() % 0xB000) - 0xB000;
				x->pos.z += (rand() % i2f(11)) - i2f(3);
			}
		}
		vol = rand() % F1_0;
		snd = 0xA8D + rand() % 5;
		if (e >= 0 && snd < MAX_SOUNDS) {
			digi_link_sound_to_object2(snd,e,0,vol > F1_0/2 ? F1_0/2 : vol,i2f(200));
			End_seq_sounds++;
		}
		if (++End_seq_n >= 50) {
			printf("end sequence: %d explosions, %d sounds\n",End_seq_n,End_seq_sounds);
			digi_kill_sound_linked_to_object(3);					// 0x2E816(3)
			End_seq_step = 3;
			dtumovie_end_credits();
		}
	}
}

// 0xA6ACB (explode 0) for scenery: explode_sound at volume 1.0, nine
// explosions of break_vclip (default 137) at the size of the object, the
// scenery of the segment falls (movement type 1), then 0x6A3CC removes the
// object and object_create_egg drops what it contains. The nine places of
// the explosions: each one is made at the center of the object, then its
// x and z += rand() % 11.0 - 5.0, y += rand() % 0.6875 - 0.3125 (0xA6C90 to
// 0xA6CD6). rand() is at most 0x7FFF, so the modulo does nothing and all
// nine are 5.0 to 4.5 to one side and -0.31 to 0.19 up and down (D-035).
// ASCENT: Dtu_fix_break_spread (default) scales rand() to the range, so
// the places are -5.0 to 6.0 on x and z, as the constants say. -origobj
// gives the 1.3 places. explode 1: 0x911AA with 0x3CA7E: the explosion
// vclip 2 at 2.5 times the size and explode_sound (no debris in the port).
int Dtu_fix_break_spread = 1;

static fix break_offset(fix range,fix low)
{
	int r = rand() & 0x7FFF;		// the rand() of Watcom C (0xD7AC4)

	if (Dtu_fix_break_spread)
		return (fix) (((long long) r * range) >> 15) - low;
	return (r % range) - low;
}

void dtu_break_dressing(object *d,object *by,int explode)
{
	dressing_info *di;
	vms_vector p;
	int i, vclip;

	if (d->type != OBJ_DRESSING || d->id >= N_dressing_types || (d->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return;
	// ASCENT: 0xA6AF8: in level 0x20 (FLAME) object 3, the Flamesword, ends
	// the game (0xA6786): the light, the spin, the 50 explosions with their
	// sounds, then the credits movie and the front end (end_sequence_frame).
	if (!explode && Current_level_num - 1 == 0x20 && d - Objects == 3) {
		end_sequence_start(d);
		return;
	}
	di = &Dressing_info[d->id];
	if (!explode && dtu_dressing_unbreakable(d))
		return;
	dtu_obj_log("obj %d %s breaks obj %d %s (%s)",by ? (int)(by-Objects) : -1,by ? dtu_object_name(by) : "nothing",(int)(d-Objects),
		dtu_object_name(d),explode ? "0x3CA7E" : "0xA6ACB");
	if (explode) {
		dtu_creature_drop_contents(d);
		vclip_at(d->segnum,&d->pos,fixmul(d->size,0x28000),2);
		sound_at(di->explode_sound,d->segnum,&d->pos,F1_0);
	}
	else {
		if (di->explode_sound >= 0)
			sound_at(di->explode_sound,d->segnum,&d->pos,F1_0);
		vclip = (di->break_vclip >= 0) ? di->break_vclip : 137;
		// ASCENT: each explosion at the center plus its own offset, as
		// 0xA6C7E moves the new object (the port added up the offsets)
		for (i=0;i<9;i++) {
			p = d->pos;
			p.x += break_offset(i2f(11),i2f(5));
			p.z += break_offset(i2f(11),i2f(5));
			p.y += break_offset(0xB000,0x5000);
			vclip_at(d->segnum,&p,d->size,vclip);
			dtu_obj_log("break explosion: offset %.2f %.2f %.2f",f2fl(p.x-d->pos.x),f2fl(p.y-d->pos.y),f2fl(p.z-d->pos.z));
		}
		// the other scenery of the segment falls (guess for the meaning)
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_DRESSING && Objects[i].segnum == d->segnum && &Objects[i] != d)
				Dressing_fall[i] = 1;
		// ASCENT: Dtu_improved_physics, scenery that lay on this one falls
		memset(Dressing_awake,1,sizeof(Dressing_awake));
	}
	d->flags |= OF_SHOULD_BE_DEAD;
	if (!explode) {
		dtu_creature_drop_contents(d);
		dtu_fire_trap(d);		// 0xA6D5A: level 0 object 0x29 (dtucombat.c)
	}
}

//------------------------------------------------------------------------
// Pushed scenery (pmon.c 0x9FA41, pcons.c 0x9E5BE, the move 0x9FE68)

int dressing_pushable(object *d)
{
	int p;

	if (d->id >= N_dressing_types)
		return 999;
	p = Dressing_info[d->id].pushable;
	return p > 0 ? p : 999;		// the default of the reader is 999
}

// The body of scenery (0x748D9): radius body_radius[0] in the x-z plane and
// half height body_half_height[0] of its model, else the size.
static void dressing_body(object *d,fix *r,fix *hh)
{
	polymodel *po = obj_model(d);

	if (po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->body_half_height) {
		*r = po->body_radius[0];
		*hh = po->body_half_height[0];
	}
	else
		*r = *hh = d->size;
}

// The velocity and the thrust of the scenery become 8 times those of the
// mover in x and z; vel.y stays; movement type 1.
void push_dressing(object *mover,object *d,int strength)
{
	physics_info *pi = &d->mtype.phys_info;
	int n = d-Objects;

	if (d->type != OBJ_DRESSING || strength <= dressing_pushable(d))
		return;
	pi->velocity.x = 8 * mover->mtype.phys_info.velocity.x;
	pi->velocity.z = 8 * mover->mtype.phys_info.velocity.z;
	pi->thrust.x = 8 * mover->mtype.phys_info.thrust.x;
	pi->thrust.z = 8 * mover->mtype.phys_info.thrust.z;
	Dressing_fall[n] = 1;
	Dressing_sig[n] = d->signature;
	Dressing_str[n] = strength;
	// the fastest push of this row of pushes: the mover slows down at the
	// end and its last pushes are slow
	{
		vms_vector v = mover->mtype.phys_info.velocity;

		v.y = 0;
		if (Dressing_slid[n] || vm_vec_mag_quick(&v) > vm_vec_mag_quick(&Dressing_vpush[n]))
			Dressing_vpush[n] = v;
	}
	vm_vec_zero(&Dressing_slide[n]);
	Dressing_slid[n] = 0;
	dtu_obj_log("push: obj %d %s (strength %d) pushes obj %d %s (pushable %d), velocity %.1f %.1f",(int)(mover-Objects),
		dtu_object_name(mover),strength,n,dtu_object_name(d),dressing_pushable(d),f2fl(pi->velocity.x),f2fl(pi->velocity.z));
}

// The body of another object that pushed scenery tests, as an upright
// cylinder about its position: radius r, half height hh. Executable
// (0x3EE70 with 0x748D9): the player has body_radius[0] of model 16 (twice
// the radius of its move query) and the half height of its move query, a
// creature or scenery body_radius[0] and the half height of its model.
// Corrected (Dtu_fix_push_body): the player has the radius of its move
// query, so that scenery and the player stop at the same distance.
static int dressing_other_body(object *o,fix *r,fix *hh)
{
	if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 0;
	if (o->type == OBJ_PLAYER) {
		dtu_body b;

		if (o != ConsoleObject)
			return 0;
		dtu_player_body(&b);
		*r = Dtu_fix_push_body ? b.move_rad : 2*b.move_rad;
		*hh = b.move_rad2;
		return 1;
	}
	if (o->type == OBJ_ROBOT) {
		polymodel *po = obj_model(o);

		if (o->id < N_robot_types && (Monster_info[o->id].flags & MONSTER_F_PASS_THRU))
			return 0;
		if (po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->body_half_height) {
			*r = po->body_radius[0];
			*hh = po->body_half_height[0];
		}
		else
			*r = *hh = o->size;
		return 1;
	}
	if (o->type == OBJ_DRESSING) {
		if (o->render_type != RT_POLYOBJ)
			return 0;
		if (o->id < N_dressing_types && Dressing_info[o->id].pass_thru)
			return 0;
		dressing_body(o,r,hh);
		return 1;
	}
	return 0;
}

// Dtu_fix_push_body: other scenery is the box of its model (the box of the
// drawn points of the rest pose, model_points_box, in the x-z plane of the
// object), not the circle of body_radius[0] around the corners of the box:
// a chair at a table is already inside of that circle. Only an upright
// object has a box: a barrel on its side has no box in x-z. The box is not
// box 1 of the file: that box of chair.pof is 2.95 above the drawn chair
// and 2.3 to one side of it.
static int dressing_box(object *o,vms_vector *mn,vms_vector *mx)
{
	return o->orient.uvec.y >= F1_0*9/10 && model_points_box(o,-1,mn,mx);
}

static int dressing_has_box(object *o)
{
	vms_vector mn, mx;

	return dressing_box(o,&mn,&mx);
}

// The move mv of the point a against the box of o about the position c,
// grown by r. *t is the part (0 to 1) of the move up to the box, or -1 for
// no hit. A start inside of the box is a hit at 0 unless the move goes
// away from the center.
static void dressing_box_sweep(object *o,vms_vector *c,vms_vector *a_w,vms_vector *mv,double r,double *t)
{
	double a[2], m[2], lo[2], hi[2], tin = 0, tout = 1;
	vms_vector rel, mn, mx;
	int k;

	*t = -1;
	if (!dressing_box(o,&mn,&mx))
		return;
	vm_vec_sub(&rel,a_w,c);
	a[0] = f2fl(vm_vec_dot(&rel,&o->orient.rvec));
	a[1] = f2fl(vm_vec_dot(&rel,&o->orient.fvec));
	m[0] = f2fl(vm_vec_dot(mv,&o->orient.rvec));
	m[1] = f2fl(vm_vec_dot(mv,&o->orient.fvec));
	lo[0] = f2fl(mn.x) - r; hi[0] = f2fl(mx.x) + r;
	lo[1] = f2fl(mn.z) - r; hi[1] = f2fl(mx.z) + r;
	if (a[0] > lo[0] && a[0] < hi[0] && a[1] > lo[1] && a[1] < hi[1]) {
		double c0 = (lo[0]+hi[0])/2 - a[0], c1 = (lo[1]+hi[1])/2 - a[1];

		if (m[0]*c0 + m[1]*c1 > 0)
			*t = 0;
		return;
	}
	for (k=0;k<2;k++) {
		if (fabs(m[k]) < 1e-9) {
			if (a[k] <= lo[k] || a[k] >= hi[k])
				return;
		}
		else {
			double t0 = (lo[k] - a[k]) / m[k], t1 = (hi[k] - a[k]) / m[k];

			if (t0 > t1) { double s = t0; t0 = t1; t1 = s; }
			if (t0 > tin) tin = t0;
			if (t1 < tout) tout = t1;
			if (tin > tout)
				return;
		}
	}
	*t = tin;
}

// The corners and the mid points of the edges of the box of o about the
// position c, in the plane of the middle of the box.
static void dressing_box_points(object *o,vms_vector *c,vms_vector pts[8])
{
	static const signed char px[8] = {-1,1,1,-1, 0,1,0,-1}, pz[8] = {-1,-1,1,1, -1,0,1,0};
	vms_vector mn, mx;
	vms_matrix m;
	int i;

	if (!dressing_box(o,&mn,&mx)) {
		vm_vec_zero(&mn);
		vm_vec_zero(&mx);
	}
	vm_copy_transpose_matrix(&m,&o->orient);
	for (i=0;i<8;i++) {
		vms_vector local, ofs;

		local.x = px[i] < 0 ? mn.x : px[i] > 0 ? mx.x : (mn.x + mx.x)/2;
		local.z = pz[i] < 0 ? mn.z : pz[i] > 0 ? mx.z : (mn.z + mx.z)/2;
		local.y = (mn.y + mx.y)/2;
		vm_vec_rotate(&ofs,&local,&m);
		vm_vec_add(&pts[i],c,&ofs);
	}
}

// The move of scenery d from p0 to p1 against the box of scenery o. Two
// boxes: the points of the box of d against the box of o, and the points
// of the box of o against the box of d (the move the other way). Else the
// circle of radius rad. Returns 0 when o has no box.
static int dressing_box_hit(object *d,object *o,vms_vector *p0,vms_vector *p1,fix rad,double *t)
{
	vms_vector mv;

	if (!dressing_has_box(o))
		return 0;
	vm_vec_sub(&mv,p1,p0);
	if (dressing_has_box(d)) {
		vms_vector pts[8], back;
		double best = -1, u;
		int i;

		back = mv;
		vm_vec_negate(&back);
		dressing_box_points(d,p0,pts);
		for (i=0;i<8;i++) {
			dressing_box_sweep(o,&o->pos,&pts[i],&mv,0,&u);
			if (u >= 0 && (best < 0 || u < best))
				best = u;
		}
		dressing_box_points(o,&o->pos,pts);
		for (i=0;i<8;i++) {
			dressing_box_sweep(d,p0,&pts[i],&back,0,&u);
			if (u >= 0 && (best < 0 || u < best))
				best = u;
		}
		*t = best;
		return 1;
	}
	dressing_box_sweep(o,&o->pos,p0,&mv,f2fl(rad),t);
	return 1;
}

// The object that the move of scenery d from p0 to p1 hits first, and the
// part (0 to 1) of the move up to it. rad and rad2 are the radii of the
// query of d. Executable (0x4F7D5): a start point inside of a body is a
// hit at once. Corrected (Dtu_fix_push_body): a body that d is in already
// does not stop a move away from it, so that two bodies that overlap can
// move apart.
static int dressing_hit_object(object *d,vms_vector *p0,vms_vector *p1,fix rad,fix rad2,fix *frac)
{
	int i, best = -1;
	fix best_f = F1_0+1;
	double mx = f2fl(p1->x - p0->x), mz = f2fl(p1->z - p0->z);
	double len2 = mx*mx + mz*mz;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix r, hh;
		double cx, cz, R, b, c, t;

		if (o == d || o->type == OBJ_NONE || !dressing_other_body(o,&r,&hh))
			continue;
		if (abs(p0->y - o->pos.y) > hh + rad2 - Dressing_y_slack)
			continue;
		if (Dtu_fix_push_body && o->type == OBJ_DRESSING && dressing_box_hit(d,o,p0,p1,rad,&t)) {
			if (t < 0)
				continue;
			if (fl2f(t) < best_f) {
				best_f = fl2f(t);
				best = i;
			}
			continue;
		}
		R = f2fl(r + rad);
		cx = f2fl(o->pos.x - p0->x);
		cz = f2fl(o->pos.z - p0->z);
		c = cx*cx + cz*cz - R*R;
		if (c < 0) {
			// the start point is inside of the body
			if (Dtu_fix_push_body && mx*cx + mz*cz <= 0)
				continue;
			t = 0;
		}
		else {
			if (len2 < 1e-9)
				continue;
			b = mx*cx + mz*cz;		// toward the body
			if (b <= 0)
				continue;
			// |m t - c|^2 = R^2: t^2 len2 - 2 t b + |c|^2 - R^2 = 0
			if (b*b - len2*c < 0)
				continue;
			t = (b - sqrt(b*b - len2*c)) / len2;
			if (t < 0 || t > 1)
				continue;
		}
		if (fl2f(t) < best_f) {
			best_f = fl2f(t);
			best = i;
		}
	}
	*frac = best_f;
	return best;
}

// Dtu_fix_push_wall: the part (0 to 1) of the move frame_vec of scenery d
// that keeps its box out of the walls. The executable tests only the
// center with half of body_radius[0] (rad + 1.0 for a wall), so a table
// (body_radius 11.2, 10 from the center to an end) goes 3.4 into a wall.
// The port moves the four corners and the four mid points of the edges of
// the box of the model (the drawn points of the rest pose, at the height
// of its middle) with the move and stops at
// the first wall that one of them hits, DTU_PUSH_WALL_GAP before it. A
// point that is outside of the mine at the start (scenery that the level
// puts into a wall) is not tested.
#define DTU_PUSH_WALL_GAP	(F1_0/4)

static fix dressing_wall_frac(object *d,vms_vector *frame_vec)
{
	fix best = F1_0, len = vm_vec_mag(frame_vec);
	vms_vector mn, mx;
	vms_matrix m;
	int i;

	if (len <= 0)
		return F1_0;
	if (!model_points_box(d,-1,&mn,&mx)) {
		// no box: a sphere of the size of the object
		fvi_query fq;
		fvi_info hit;
		vms_vector p1;

		vm_vec_add(&p1,&d->pos,frame_vec);
		fq.p0 = &d->pos; fq.p1 = &p1; fq.startseg = d->segnum; fq.rad = d->size;
		fq.thisobjnum = d-Objects; fq.ignore_obj_list = NULL; fq.flags = 0;
		if (find_vector_intersection(&fq,&hit) != HIT_NONE)
			return 0;
		return F1_0;
	}
	vm_copy_transpose_matrix(&m,&d->orient);
	for (i=0;i<8;i++) {
		static const signed char px[8] = {-1,1,1,-1, 0,1,0,-1}, pz[8] = {-1,-1,1,1, -1,0,1,0};
		vms_vector local, ofs, p0, p1;
		fvi_query fq;
		fvi_info hit;
		int seg, fate;

		local.x = px[i] < 0 ? mn.x : px[i] > 0 ? mx.x : (mn.x + mx.x)/2;
		local.z = pz[i] < 0 ? mn.z : pz[i] > 0 ? mx.z : (mn.z + mx.z)/2;
		local.y = (mn.y + mx.y)/2;
		vm_vec_rotate(&ofs,&local,&m);
		vm_vec_add(&p0,&d->pos,&ofs);
		seg = find_point_seg(&p0,d->segnum);
		if (seg < 0)
			continue;
		vm_vec_add(&p1,&p0,frame_vec);
		fq.p0 = &p0; fq.p1 = &p1; fq.startseg = seg; fq.rad = 0;
		fq.thisobjnum = d-Objects; fq.ignore_obj_list = NULL; fq.flags = 0;
		fate = find_vector_intersection(&fq,&hit);
		if (fate == HIT_WALL) {
			fix f = fixdiv(vm_vec_dist(&hit.hit_pnt,&p0) - DTU_PUSH_WALL_GAP,len);

			if (f < 0)
				f = 0;
			if (f < best)
				best = f;
		}
		else if (fate != HIT_NONE)
			best = 0;
	}
	return best;
}

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_physics. The push of scenery by the player, the
// fall of scenery and the sound of scenery that moves. New code, not in
// the executable (docs/re/physics.md, section 13.2).
//
// The push of the executable gives the scenery 8 times the velocity of
// the mover at each hit, and the scenery slows down by 8 times in each
// frame. Here the player moves the scenery directly: dtu_dressing_push()
// moves it by a distance, as far as the walls and the other bodies let
// it, and tells the player how far it went. Scenery that lies on the
// pushed scenery moves with it. Scenery with no support falls.

#define DTU_SOLID_GRAVITY		DTU_FIX_JUMP_GRAVITY	// the fall of scenery, as the jump of the player
#define DTU_SOLID_FALL_GAP		(F1_0/2)		// scenery falls when its support is this much or more below it
#define DTU_SOLID_REST_GAP		(F1_0*3/4)		// scenery lies on other scenery when its bottom is this near to the top
#define DTU_SOLID_LIFT_MAX		i2f(3)			// scenery that is less than this deep in the top of other scenery is put onto it
#define DTU_PUSH_SND_START		0x1999			// 0.1 s: the sound starts after scenery moved this long
#define DTU_PUSH_SND_HOLD		0x2000			// 0.125 s: the sound stops after scenery did not move this long

static fix Dressing_vy[MAX_OBJECTS];			// the velocity of the fall
static ubyte Dressing_moved[MAX_OBJECTS];		// the scenery moved in this frame
static fix Dressing_move_time[MAX_OBJECTS];	// how long it moves
static fix Dressing_snd_hold[MAX_OBJECTS];
static ubyte Dressing_snd_on[MAX_OBJECTS];

int dtu_solid_on(void)
{
	return Dtu_improved_physics && Dtu_player_walk && Dtu_objects_enabled;
}

// A new level or a loaded game: no scenery moves. Scenery that is in the
// top of other scenery is put onto it (Dressing_awake 2: a saved game of
// an earlier version can have scenery that sank into the scenery under
// it). No scenery falls at that time: the level can have scenery that
// hangs on a wall.
void dressing_level_reset(void)
{
	memset(Dressing_fall,0,sizeof(Dressing_fall));
	memset(Dressing_awake,2,sizeof(Dressing_awake));
	memset(Dressing_vy,0,sizeof(Dressing_vy));
	memset(Dressing_moved,0,sizeof(Dressing_moved));
	memset(Dressing_move_time,0,sizeof(Dressing_move_time));
	memset(Dressing_snd_hold,0,sizeof(Dressing_snd_hold));
	memset(Dressing_snd_on,0,sizeof(Dressing_snd_on));
}

// The lowest drawn point of the scenery d, relative to its position: the
// lowest corner of the box of the drawn points of its model, turned with
// the object. Returns 0 if the model has no points.
static int dressing_drawn_bottom(object *d,fix *bot)
{
	vms_vector mn, mx;
	vms_matrix m;
	int k;

	if (!model_points_box(d,-1,&mn,&mx))
		return 0;
	vm_copy_transpose_matrix(&m,&d->orient);
	for (k=0;k<8;k++) {
		vms_vector c, w;

		c.x = (k & 1) ? mx.x : mn.x;
		c.y = (k & 2) ? mx.y : mn.y;
		c.z = (k & 4) ? mx.z : mn.z;
		vm_vec_rotate(&w,&c,&m);
		if (k == 0 || w.y < *bot)
			*bot = w.y;
	}
	return 1;
}

// The floor under the position of d (a ray straight down). Returns 0 if
// there is none.
static int dressing_floor_under(object *d,fix *floor)
{
	fvi_query gq;
	fvi_info gh;
	vms_vector down = d->pos;

	down.y -= i2f(200);
	gq.p0 = &d->pos; gq.p1 = &down; gq.startseg = d->segnum; gq.rad = 0;
	gq.thisobjnum = d-Objects; gq.ignore_obj_list = NULL; gq.flags = 0;
	if (find_vector_intersection(&gq,&gh) != HIT_WALL)
		return 0;
	*floor = gh.hit_pnt.y;
	return 1;
}

// The -objlog line of the height of scenery: the position, the floor under
// it, the lowest drawn point of the model and the bottom of its solid
// shape (dtu_solid_get), so that a test can see scenery in the floor.
void dressing_floor_log(object *d,const char *when)
{
	double floor = -9999, drawn = -9999, solid = -9999;
	fix f, b;
	dtu_solid s;

	if (!Obj_log || d->type != OBJ_DRESSING || d->render_type != RT_POLYOBJ)
		return;
	if (dressing_floor_under(d,&f))
		floor = f2fl(f);
	if (dressing_drawn_bottom(d,&b))
		drawn = f2fl(d->pos.y + b);
	if (dtu_solid_get(d,&s))
		solid = s.bot;
	dtu_obj_log("floor (%s): obj %d %s seg %d pos y %.2f floor %.2f drawn bottom %.2f (%+.2f) solid bottom %.2f (%+.2f) up %.2f",
		when,(int)(d-Objects),dtu_object_name(d),d->segnum,f2fl(d->pos.y),floor,drawn,drawn-floor,solid,solid-floor,
		f2fl(d->orient.uvec.y));
}

// ASCENT: Dtu_fix_scenery_floor. The level files put some scenery partly
// into the floor, and 1.3 draws it there (it moves no scenery at the
// load): in YPINN chair 18 is drawn 2.59 and chair 9 2.06 deep in the
// floor, in THIEF2 table 81 1.10. Saved games of earlier versions of the
// port also have chairs 2.95 in the floor (the box of chair.pof, see
// dtu_solid_get). 1 (default): at the setup of a level each upright
// scenery object whose lowest drawn point is DTU_FLOOR_LIFT_MIN to
// DTU_FLOOR_LIFT_MAX below the floor under its position goes up onto the
// floor. Scenery above the floor stays (scenery on a table, a lantern on a
// wall). 0 (-origobj): the places of the level file.
int Dtu_fix_scenery_floor = 1;
#define DTU_FLOOR_LIFT_MIN	(F1_0/16)
#define DTU_FLOOR_LIFT_MAX	i2f(4)

void dressing_floor_fix(void)
{
	int i;

	if (!Dtu_fix_scenery_floor)
		return;
	for (i=0;i<=Highest_object_index;i++) {
		object *d = &Objects[i];
		fix floor, bot, depth;
		vms_vector p;
		int seg;

		if (d->type != OBJ_DRESSING || d->render_type != RT_POLYOBJ || (d->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
			continue;
		if (d->orient.uvec.y < F1_0*9/10 || !dressing_drawn_bottom(d,&bot) || !dressing_floor_under(d,&floor))
			continue;
		depth = floor - (d->pos.y + bot);
		if (depth < DTU_FLOOR_LIFT_MIN || depth > DTU_FLOOR_LIFT_MAX)
			continue;
		p = d->pos;
		p.y += depth;
		seg = find_point_seg(&p,d->segnum);
		if (seg < 0)
			continue;
		dtu_obj_log("floor: obj %d %s is %.2f in the floor, up onto it (Dtu_fix_scenery_floor)",i,dtu_object_name(d),f2fl(depth));
		d->pos = p;
		if (seg != d->segnum)
			obj_relink(i,seg);
	}
}

// 1 if the scenery of the shape a lies on the scenery of the shape b
static int dressing_lies_on(dtu_solid *a,dtu_solid *b)
{
	if (fabs(a->bot - b->top) > f2fl(DTU_SOLID_REST_GAP))
		return 0;
	return dtu_solid_inside(b,a->cx,a->cz,0) || dtu_solid_inside(a,b->cx,b->cz,0);
}

static fix dressing_push_move(object *d,vms_vector *delta,int strength,int depth)
{
	int n = d-Objects, i, pass, hit_obj, seg, n_riders = 0;
	short riders[8];
	fix frac = F1_0, br, bhh, len = vm_vec_mag(delta);
	vms_vector p1, mv;
	dtu_solid sd;

	if (len <= 0 || d->type != OBJ_DRESSING)
		return 0;
	dressing_body(d,&br,&bhh);

	if (depth < 2 && dtu_solid_get(d,&sd))
		for (i=0;i<=Highest_object_index && n_riders < 8;i++) {
			dtu_solid sr;

			if (i != n && Objects[i].type == OBJ_DRESSING && dtu_solid_get(&Objects[i],&sr) && sr.bot > sd.bot && dressing_lies_on(&sr,&sd))
				riders[n_riders++] = i;
		}

	for (pass=0;pass<2;pass++) {
		vm_vec_add(&p1,&d->pos,delta);
		// scenery that lies on this one, or under it, is not in the way
		Dressing_y_slack = DTU_SOLID_REST_GAP;
		hit_obj = dressing_hit_object(d,&d->pos,&p1,br,bhh,&frac);
		Dressing_y_slack = 0;
		if (hit_obj < 0) {
			frac = F1_0;
			break;
		}
		// other scenery in the way moves too when the strength is enough
		if (pass == 0 && depth < 2 && Objects[hit_obj].type == OBJ_DRESSING && strength > dressing_pushable(&Objects[hit_obj])) {
			vms_vector rest;

			vm_vec_copy_scale(&rest,delta,F1_0 - frac);
			if (dressing_push_move(&Objects[hit_obj],&rest,strength,depth+1) > 0)
				continue;
		}
		frac -= fixdiv(DTU_PUSH_WALL_GAP,len);
		break;
	}
	if (frac > 0) {
		vm_vec_copy_scale(&mv,delta,frac);
		frac = fixmul(frac,dressing_wall_frac(d,&mv));
	}
	if (frac <= 0)
		return 0;
	vm_vec_copy_scale(&mv,delta,frac);
	vm_vec_add(&p1,&d->pos,&mv);
	seg = find_point_seg(&p1,d->segnum);
	if (seg < 0)
		return 0;
	d->pos = p1;
	if (seg != d->segnum)
		obj_relink(n,seg);
	Dressing_moved[n] = 1;
	Dressing_awake[n] = 1;
	for (i=0;i<n_riders;i++) {
		dressing_push_move(&Objects[riders[i]],&mv,0,2);
		Dressing_awake[riders[i]] = 1;
	}
	return frac;
}

// The strength of the player for a push (P+0xB7, 0x9E5BE)
int dtu_player_push_strength(void)
{
	return Dtu_char.created ? Dtu_char.pa.strength : 18;
}

// The player pushes the scenery d by delta. Returns the part of delta (0
// to 1) that the scenery moved.
fix dtu_dressing_push(object *d,vms_vector *delta,int strength)
{
	fix f = dressing_push_move(d,delta,strength,0);

	if (f > 0)
		dtu_obj_log("push: the player moves obj %d %s by %.3f, part %.2f",(int)(d-Objects),dtu_object_name(d),f2fl(vm_vec_mag(delta)),f2fl(f));
	return f;
}

// The height on which the scenery d with the shape s lies: the floor
// under its position, or the top of other scenery under it. Returns 0 if
// there is none. *on_solid: it is other scenery.
static int dressing_rest_height(object *d,dtu_solid *s,double *y,int *on_solid)
{
	fvi_query gq;
	fvi_info gh;
	vms_vector down = d->pos;
	int found = 0, i;

	*on_solid = 0;
	down.y -= i2f(200);
	gq.p0 = &d->pos; gq.p1 = &down; gq.startseg = d->segnum; gq.rad = 0;
	gq.thisobjnum = d-Objects; gq.ignore_obj_list = NULL; gq.flags = 0;
	if (find_vector_intersection(&gq,&gh) == HIT_WALL) {
		*y = f2fl(gh.hit_pnt.y);
		found = 1;
	}
	for (i=0;i<=Highest_object_index;i++) {
		dtu_solid o;

		if (&Objects[i] == d || Objects[i].type != OBJ_DRESSING || !dtu_solid_get(&Objects[i],&o))
			continue;
		if (o.top > s->bot + f2fl(DTU_SOLID_LIFT_MAX) || o.bot >= s->bot || (found && o.top <= *y))
			continue;
		if (!dtu_solid_inside(&o,s->cx,s->cz,0) && !dtu_solid_inside(s,o.cx,o.cz,0))
			continue;
		*y = o.top;
		*on_solid = 1;
		found = 1;
	}
	return found;
}

// Scenery whose support went away falls, with a constant gravity. Scenery
// that is in the top of other scenery is put onto it.
static void dressing_settle_frame(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *d = &Objects[i];
		dtu_solid s;
		double rest, gap;
		int on_solid;
		fix dy = 0;

		if (!Dressing_awake[i])
			continue;
		if (!dtu_solid_get(d,&s) || !dressing_rest_height(d,&s,&rest,&on_solid)) {
			Dressing_awake[i] = 0;
			Dressing_vy[i] = 0;
			continue;
		}
		gap = s.bot - rest;
		if (Dressing_awake[i] == 1 && (gap >= f2fl(DTU_SOLID_FALL_GAP) || (Dressing_vy[i] < 0 && gap > 0))) {
			Dressing_vy[i] -= fixmul(DTU_SOLID_GRAVITY,FrameTime);
			dy = fixmul(Dressing_vy[i],FrameTime);
			if (-f2fl(dy) >= gap) {
				dy = -fl2f(gap);
				Dressing_vy[i] = 0;
				Dressing_awake[i] = 0;
				dtu_obj_log("fall: obj %d %s lands at %.2f",i,dtu_object_name(d),rest);
			}
		}
		else {
			if (on_solid && gap < -0.05 && gap > -f2fl(DTU_SOLID_LIFT_MAX)) {
				dy = -fl2f(gap);
				dtu_obj_log("fall: obj %d %s is put onto the top at %.2f, up %.2f",i,dtu_object_name(d),rest,-gap);
			}
			Dressing_vy[i] = 0;
			Dressing_awake[i] = 0;
		}
		if (dy) {
			vms_vector p = d->pos;
			int seg;

			p.y += dy;
			seg = find_point_seg(&p,d->segnum);
			if (seg < 0) {
				Dressing_vy[i] = 0;
				Dressing_awake[i] = 0;
				continue;
			}
			d->pos = p;
			if (seg != d->segnum)
				obj_relink(i,seg);
		}
	}
}

// The sound 2706 of scenery that moves. The executable links the sound to
// the object while its speed is above 0x70. The port played the sound at
// the place of the object each 8 frames, so more copies of it played at
// the same time at a high frame rate, and it played for scenery that a
// wall or a body held in its place. Now: one linked sound for each object,
// from DTU_PUSH_SND_START after the scenery started to move until
// DTU_PUSH_SND_HOLD after it moved the last time.
static void dressing_sound_frame(void)
{
	int i;

	for (i=0;i<MAX_OBJECTS;i++) {
		int moved = Dressing_moved[i] && i <= Highest_object_index && Objects[i].type == OBJ_DRESSING;

		Dressing_moved[i] = 0;
		if (moved) {
			Dressing_move_time[i] += FrameTime;
			Dressing_snd_hold[i] = DTU_PUSH_SND_HOLD;
			if (!Dressing_snd_on[i] && Dressing_move_time[i] >= DTU_PUSH_SND_START) {
				digi_link_sound_to_object2(2706,i,1,F1_0/2,i2f(200));
				Dressing_snd_on[i] = 1;
			}
		}
		else if (Dressing_snd_hold[i] > 0) {
			Dressing_snd_hold[i] -= FrameTime;
			if (Dressing_snd_hold[i] <= 0) {
				if (i <= Highest_object_index)
					dressing_floor_log(&Objects[i],"move end");
				if (Dressing_snd_on[i])
					digi_kill_sound_linked_to_object(i);
				Dressing_snd_on[i] = 0;
				Dressing_move_time[i] = 0;
			}
		}
	}
}

// 0x9FE68: the move of scenery with movement type 1. The velocity is
// halved and x and z are divided by 4, then gravity and the move. The
// sound 2706 plays while the speed is above 0x70. The move ends at a stop.
//
// The slow down is in each frame, as in the executable. The push gives 8
// times the velocity of the mover, so the scenery moves the step of the
// mover in the frame after the push and one eighth of it in the next. The
// mover that walks into scenery stays where it is in the frame of a hit
// and walks one step in the next, so mover and scenery go on at one half
// of the walk speed, at each frame rate: all distances of that cycle are
// in proportion to the step. (An earlier version of the port slowed the
// scenery down once in each reference frame; above 21 frames per second the
// scenery then kept the speed of the mover for some frames and ran ahead,
// and the push grew with the frame rate.)
void dressing_move_frame(void)
{
	int i;
	int solid = dtu_solid_on();

	// ASCENT: Dtu_improved_physics, the fall of scenery
	if (solid)
		dressing_settle_frame();

	for (i=0;i<=Highest_object_index;i++) {
		object *d = &Objects[i];
		physics_info *pi = &d->mtype.phys_info;
		vms_vector p1, frame_vec, old_pos;
		fix speed, frac, br, bhh;
		int hit_obj;

		if (!Dressing_fall[i])
			continue;
		if (d->type != OBJ_DRESSING || d->signature != Dressing_sig[i] || (d->flags & OF_SHOULD_BE_DEAD)) {
			Dressing_fall[i] = 0;
			continue;
		}
		pi->velocity.x = (pi->velocity.x / 2) >> 2;
		pi->velocity.z = (pi->velocity.z / 2) >> 2;
		pi->velocity.y /= 2;
		// gravity (0x6C662): down to the floor, 3.0 times the distance.
		// ASCENT: the distance is from the lowest drawn point (the port
		// took the size of the object, a radius, as the depth of the
		// bottom). 1.3 takes body_half_height[0] (0x6C30E, the box of
		// the file) and starts the fall only at a distance above 6.0.
		// ASCENT: Dtu_improved_physics has its own fall (dressing_settle_frame).
		if (solid) {
			pi->velocity.y = 0;
			Dressing_awake[i] = 1;
		}
		else {
			fvi_query gq;
			fvi_info gh;
			vms_vector down = d->pos;

			down.y -= i2f(200);
			gq.p0 = &d->pos; gq.p1 = &down; gq.startseg = d->segnum; gq.rad = 0;
			gq.thisobjnum = i; gq.ignore_obj_list = NULL; gq.flags = 0;
			if (find_vector_intersection(&gq,&gh) == HIT_WALL) {
				fix bot = -d->size, fd;

				dressing_drawn_bottom(d,&bot);
				fd = d->pos.y + bot - gh.hit_pnt.y;
				if (fd > F1_0/16)
					pi->velocity.y -= fixmul(fd,i2f(3));
				else
					pi->velocity.y = 0;
			}
		}
		speed = vm_vec_mag_quick(&pi->velocity);
		// ASCENT: Dtu_fix_push_rate. The push has ended: the slide to the
		// lead of the reference frame, 1.843 * velocity * (reference frame
		// time - frame time), with a velocity that goes down by e in each
		// reference frame time.
		if (speed <= 0x70 && Dtu_fix_push_rate && !Dressing_slid[i]) {
			Dressing_slid[i] = 1;
			vm_vec_zero(&Dressing_slide[i]);
			if (FrameTime > 0 && FrameTime < DTU_REF_FRAMETIME) {
				double dt = f2fl(FrameTime), tref = f2fl(DTU_REF_FRAMETIME);
				double lead = DTU_PUSH_LEAD * (tref - dt);
				double k = lead * (1 - exp(-dt / tref)) / dt;

				Dressing_slide[i].x = fl2f(f2fl(Dressing_vpush[i].x) * k);
				Dressing_slide[i].z = fl2f(f2fl(Dressing_vpush[i].z) * k);
			}
		}
		if (Dtu_fix_push_rate && Dressing_slid[i]) {
			vm_vec_add2(&pi->velocity,&Dressing_slide[i]);
			speed = vm_vec_mag_quick(&pi->velocity);
		}
		if (speed <= 0x70) {
			vm_vec_zero(&pi->velocity);
			vm_vec_zero(&Dressing_slide[i]);
			Dressing_fall[i] = 0;
			if (!solid || Dressing_move_time[i] > 0) {
				dtu_obj_log("push: obj %d %s stops at %.2f %.2f %.2f",i,dtu_object_name(d),f2fl(d->pos.x),f2fl(d->pos.y),f2fl(d->pos.z));
				dressing_floor_log(d,"push stop");
			}
			continue;
		}
		vm_vec_copy_scale(&frame_vec,&pi->velocity,FrameTime);
		// 0xA007F: the executable doubles x and z of the move, each one in
		// 1 of 7 frames (rand() % 7 == 2)
		if (!Dtu_fix_push_body) {
			if (rand() % 7 == 2)
				frame_vec.x *= 2;
			if (rand() % 7 == 2)
				frame_vec.z *= 2;
		}
		dressing_body(d,&br,&bhh);

		// The bodies of the player, the creatures and other scenery
		// (0x3EE70). Executable: the query of the move has the radius
		// body_radius[0] / 2 (0x9FFD6), and a hit stops the move. The hit
		// of scenery pushes it with the velocity of this one (0xA02A8, no
		// test of "pushable"). Corrected (Dtu_fix_push_body): the whole
		// radius, other scenery is a box, and other scenery is pushed only
		// with the strength of the first push.
		vm_vec_add(&p1,&d->pos,&frame_vec);
		hit_obj = dressing_hit_object(d,&d->pos,&p1,Dtu_fix_push_body ? br : br/2,bhh,&frac);
		if (hit_obj >= 0) {
			object *o = &Objects[hit_obj];

			if (o->type == OBJ_DRESSING) {
				// the velocity of this frame, 8 times (0x9E5F1)
				vms_vector v = pi->velocity, t = pi->thrust;
				int str = Dtu_fix_push_body ? Dressing_str[i] : 0x7FFF;

				push_dressing(d,o,str);
				pi->velocity = v; pi->thrust = t;
			}
			if (!Dtu_fix_push_body)
				frac = 0;
			else {
				// stop at the body, a little before it
				fix len = vm_vec_mag(&frame_vec);

				if (len > 0)
					frac -= fixdiv(DTU_PUSH_WALL_GAP,len);
				if (frac < 0)
					frac = 0;
			}
		}
		else
			frac = F1_0;

		// The walls
		if (frac > 0) {
			if (Dtu_fix_push_wall) {
				vms_vector v;
				fix fw;

				vm_vec_copy_scale(&v,&frame_vec,frac);
				fw = dressing_wall_frac(d,&v);
				if (fw < F1_0) {
					frac = fixmul(frac,fw);
					hit_obj = -2;
				}
			}
			else {
				// executable: the query of the center with the radius
				// body_radius[0] / 2, the wall sides take rad + 1.0
				fvi_query fq;
				fvi_info hit;
				int fate;

				fq.p0 = &d->pos; fq.p1 = &p1; fq.startseg = d->segnum; fq.rad = br/2;
				fq.thisobjnum = i; fq.ignore_obj_list = NULL; fq.flags = 0;
				Fvi_dtu_body = 1;
				Fvi_dtu_rad2 = bhh;
				fate = find_vector_intersection(&fq,&hit);
				Fvi_dtu_body = 0;
				if (fate != HIT_NONE) {
					frac = 0;
					hit_obj = -2;
					if (fate == HIT_WALL && hit.hit_wallnorm.y > F1_0/2)
						pi->velocity.y = 0;
				}
			}
		}

		old_pos = d->pos;
		if (frac > 0) {
			int seg;

			vm_vec_scale_add(&p1,&d->pos,&frame_vec,frac);
			seg = find_point_seg(&p1,d->segnum);
			if (seg >= 0) {
				d->pos = p1;
				if (seg != d->segnum)
					obj_relink(i,seg);
			}
		}
		if (Dtu_fix_push_rate && Dressing_slid[i]) {
			// the slide is not a part of the velocity that the next frame
			// slows down
			fix f = fl2f(exp(-f2fl(FrameTime) / f2fl(DTU_REF_FRAMETIME)));

			pi->velocity.x -= Dressing_slide[i].x;
			pi->velocity.z -= Dressing_slide[i].z;
			Dressing_slide[i].x = fixmul(Dressing_slide[i].x,f);
			Dressing_slide[i].z = fixmul(Dressing_slide[i].z,f);
		}
		if (hit_obj != -1) {
			// a stop at a wall or a body (0xA00DE: after 5 tries the
			// velocity is 0)
			pi->velocity.x = pi->velocity.z = 0;
			vm_vec_zero(&Dressing_slide[i]);
		}
		// ASCENT: the sound only for scenery that moved in this frame
		if (vm_vec_dist_quick(&old_pos,&d->pos) > 0x70)
			Dressing_moved[i] = 1;
	}
	dressing_sound_frame();
}

// ASCENT: collide_robot_and_player 0x26FE0 to 0x27054: a creature of the
// rat type touches the player, and the player wears boots (the item
// index of the boot slot, byte P+0x1AEC, is not negative). Only the move
// of the rat finds the contact: each query skips a pass_thru creature
// (flag 0x40, 0x3F38F), so the player walks through rats. vclip 9 at the
// rat with its size (0x3BD4C), then 0x25ADF: the rat is hostile, inactive
// and goes to the center of a random segment whose bottom side has
// pad & 3 not 1 or 3 and the texture number 0 or less,
// with the behavior wander (0x1038B), animation state 1 and object flags
// 0. After 100 tries with no segment the rat plays its die sound
// (0x2E900) and is removed (0x6A3CC). No experience, no corpse.
static void rat_squash(object *rat)
{
	int n = rat-Objects, tries, seg = -1;
	vms_vector pos;

	vclip_at(rat->segnum,&rat->pos,rat->size,9);
	rat->dtu.m_1b0 |= DTU_HOSTILE;
	for (tries=0;tries<100 && seg < 0;tries++) {
		int s = Highest_segment_index > 0 ? rand() % Highest_segment_index : 0;
		int floor = Segments[s].sides[WBOTTOM].pad & 3;

		if (floor == 1 || floor == 3 || Segments[s].sides[WBOTTOM].tmap_num > 0)
			continue;
		compute_segment_center(&pos,&Segments[s]);
		if (find_point_seg(&pos,s) < 0)
			continue;
		seg = s;
	}
	if (seg < 0) {
		dtu_obj_log("rat: obj %d is squashed and removed",n);
		if (Monster_info[rat->id].die_sound > 0)
			sound_at(Monster_info[rat->id].die_sound,rat->segnum,&rat->pos,F1_0);
		dtu_creature_remove(rat,"a squashed rat with no free segment (0x25ADF)");
		return;
	}
	dtu_obj_log("rat: obj %d is squashed and goes to segment %d",n,seg);
	// ASCENT: the session log, the creature leaves the place of the player
	dtulog_event("Creature squashed as a rat: obj %d %s goes to segment %d (0x25ADF)",n,dtu_object_name(rat),seg);
	rat->pos = pos;
	obj_relink(n,seg);
	rat->ctype.ai_info.behavior = DAIB_WANDER;
	ail_keep_bits = 1;
	init_creature_ai(rat);
	ail_keep_bits = 0;
	DTU_ANIM_STATE(rat) = DTU_ANIM_GOAL(rat) = DAS_STAND;
	DTU_ANIM_FRAME(rat) = Dtu_anim_first[DAS_STAND];
	rat->flags &= ~OF_DTU_NOHIT;
}

int dtu_collide_two_objects(object *a, object *b, vms_vector *collision_point)
{
	object *t;

	if (!Dtu_objects_enabled)
		return 0;

	if (a->flags & OF_SHOULD_BE_DEAD)
		return 1;

	// the player is "a"
	if (b->type == OBJ_PLAYER && a->type != OBJ_PLAYER) {
		t = a; a = b; b = t;
	}

	if (a->type == OBJ_PLAYER) {
		switch (b->type) {
			case OBJ_MISC:
			case OBJ_ARMOR:
			case OBJ_SPELL:
			case OBJ_WEAPON:
				pick_up_item(a,b);
				break;
			case OBJ_ROBOT:
				// ASCENT: 0x26FE0: a rat that touches the player with boots
				// is squashed
				if (dtu_rat_type(b->id) && Dtu_char.slot[DTU_SLOT_BOOT].n >= 0 && Dtu_char.slot[DTU_SLOT_BOOT].n < Dtu_n_inventory) {
					rat_squash(b);
					break;
				}
				// collide_robot_and_player (0x26FC1): a hostile creature
				// that the player touches is alerted. The bump (0x2627C) is
				// not in the port: the bodies stop in the movement code.
				if (is_hostile(b) && Ail[b-Objects].awareness < 3) {
					Ail[b-Objects].awareness = 3;
					Ail[b-Objects].awareness_time = i2f(3);
				}
				// 0x2707A: the alert function 0x160AB(creature, 3)
				if (is_hostile(b))
					dtu_creature_alert(b,3);
				break;
			case OBJ_DRESSING:
				// 0x9E5BE (pcons.c): the player pushes scenery when the
				// strength (P+0xB7) is above "pushable", at each hit. The
				// count of 0x1452F1 (hits of the object 0x1452F5 in a row)
				// decides only whether the player goes back to the start of
				// the frame (0x9E598), not the push. The player stays out
				// of the scenery (physics.c).
				push_dressing(a,b,dtu_player_push_strength());
				break;
		}
	}

	(void)collision_point;
	return 1;
}
