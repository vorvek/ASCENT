/*
 * ASCENT: new file. Dtu_fixed_step: the game runs in steps of 1/60 second
 * and the picture shows a state between the last two steps. New code, not
 * in the executable. See docs/re/physics.md, section 14.
 *
 * The executable has one step of the game for each picture, with the time
 * of the picture as the time of the step (FrameTime, 1/150 to 1/5 second).
 * Many of its rules count frames, so the movement changes with the frame
 * rate (section 8). The port has a correction for each known place, but
 * each new rule must be made independent of the frame rate again, and a
 * step of 1/144 second and one of 1/20 second never give the same result.
 *
 * With the fixed step, the time of a picture goes to an account. The game
 * does one step for each 1/60 second in the account, with FrameTime of
 * 1/60 second: none in a picture at a high frame rate, more than one at a
 * low frame rate. The part of a step that stays in the account is the
 * place of the picture between the last two steps (Dtu_step_alpha). The
 * positions and the orientations of the objects, the look pitch and the
 * motion of the view are put there for the picture and put back after it.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"
#include "physics.h"
#include "dtustep.h"

int Dtu_fixed_step = 1;
fix Dtu_step_alpha = F1_0;
int Dtu_step_running = 0;		// this frame uses the fixed step (game.c)

// An object that moved more than this in one step was put at another
// place (a warp, a load): the picture does not show the way between.
#define DTU_STEP_WARP		i2f(25)

static vms_vector Prev_pos[MAX_OBJECTS];
static vms_matrix Prev_orient[MAX_OBJECTS];
static int Prev_sig[MAX_OBJECTS];
static ubyte Prev_ok[MAX_OBJECTS];
static fixang Prev_pitch;
static fix Prev_bob;
static int Prev_view_ok = 0;

static vms_vector Save_pos[MAX_OBJECTS];
static vms_matrix Save_orient[MAX_OBJECTS];
static ubyte Save_used[MAX_OBJECTS];
static fixang Save_pitch;
static fix Save_bob;
static int Save_active = 0;

// No picture between two steps until the next step (a new level, a load)
void dtu_step_reset(void)
{
	memset(Prev_ok,0,sizeof(Prev_ok));
	Prev_view_ok = 0;
	Dtu_step_alpha = F1_0;
}

// Before a step: the state of the objects is the start of the way that
// the pictures show.
void dtu_step_store(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];

		if (o->type == OBJ_NONE) {
			Prev_ok[i] = 0;
			continue;
		}
		Prev_pos[i] = o->pos;
		Prev_orient[i] = o->orient;
		Prev_sig[i] = o->signature;
		Prev_ok[i] = 1;
	}
	for (;i<MAX_OBJECTS;i++)
		Prev_ok[i] = 0;
	Prev_pitch = Dtu_look_pitch;
	Prev_bob = Dtu_bob_phase;
	Prev_view_ok = 1;
}

static fix step_lerp(fix a,fix b,fix alpha)
{
	return a + fixmul(b - a,alpha);
}

// Before a picture: the objects go to the part alpha (0 to 1) of the way
// from the state before the last step to the state after it.
void dtu_step_view_begin(void)
{
	fix alpha = Dtu_step_alpha;
	int i;

	if (Save_active || alpha >= F1_0 || alpha < 0)
		return;
	Save_active = 1;
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		vms_vector d;

		Save_used[i] = 0;
		if (o->type == OBJ_NONE || !Prev_ok[i] || Prev_sig[i] != o->signature)
			continue;
		vm_vec_sub(&d,&o->pos,&Prev_pos[i]);
		if (abs(d.x) > DTU_STEP_WARP || abs(d.y) > DTU_STEP_WARP || abs(d.z) > DTU_STEP_WARP)
			continue;
		if (!d.x && !d.y && !d.z && !memcmp(&o->orient,&Prev_orient[i],sizeof(vms_matrix)))
			continue;
		Save_used[i] = 1;
		Save_pos[i] = o->pos;
		Save_orient[i] = o->orient;
		vm_vec_scale_add(&o->pos,&Prev_pos[i],&d,alpha);
		if (memcmp(&o->orient,&Prev_orient[i],sizeof(vms_matrix))) {
			vms_vector f,u;

			f.x = step_lerp(Prev_orient[i].fvec.x,o->orient.fvec.x,alpha);
			f.y = step_lerp(Prev_orient[i].fvec.y,o->orient.fvec.y,alpha);
			f.z = step_lerp(Prev_orient[i].fvec.z,o->orient.fvec.z,alpha);
			u.x = step_lerp(Prev_orient[i].uvec.x,o->orient.uvec.x,alpha);
			u.y = step_lerp(Prev_orient[i].uvec.y,o->orient.uvec.y,alpha);
			u.z = step_lerp(Prev_orient[i].uvec.z,o->orient.uvec.z,alpha);
			// a turn of almost one half in one step has no direction between
			if (vm_vec_mag_quick(&f) > F1_0/4 && vm_vec_mag_quick(&u) > F1_0/4)
				vm_vector_2_matrix(&o->orient,&f,&u,NULL);
		}
	}
	for (;i<MAX_OBJECTS;i++)
		Save_used[i] = 0;

	Save_pitch = Dtu_look_pitch;
	Save_bob = Dtu_bob_phase;
	if (Prev_view_ok) {
		Dtu_look_pitch = (fixang) (Prev_pitch + fixmul((fix) (short) (Dtu_look_pitch - Prev_pitch),alpha));
		Dtu_bob_phase = step_lerp(Prev_bob,Dtu_bob_phase,alpha);
	}

	// the line VIEW of -movelog: the place of the player in the picture
	if (ConsoleObject) {
		char text[120];

		snprintf(text,sizeof(text),"VIEW part %.4f pos %.4f %.4f %.4f fvec %.4f %.4f\n",f2fl(alpha),f2fl(ConsoleObject->pos.x),
			f2fl(ConsoleObject->pos.y),f2fl(ConsoleObject->pos.z),f2fl(ConsoleObject->orient.fvec.x),f2fl(ConsoleObject->orient.fvec.z));
		dtu_move_log_line(text);
	}
}

// After a picture: the state of the game again
void dtu_step_view_end(void)
{
	int i;

	if (!Save_active)
		return;
	Save_active = 0;
	for (i=0;i<MAX_OBJECTS;i++)
		if (Save_used[i]) {
			Objects[i].pos = Save_pos[i];
			Objects[i].orient = Save_orient[i];
			Save_used[i] = 0;
		}
	Dtu_look_pitch = Save_pitch;
	Dtu_bob_phase = Save_bob;
}
