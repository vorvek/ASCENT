/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/physics.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:33:06 $
 * 
 * Headers for physics functions and data
 * 
 * $Log: physics.h $
 * Revision 2.0  1995/02/27  11:33:06  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.29  1995/02/06  19:47:18  matt
 * New function (untested), set_thrust_from_velocity()
 * 
 * Revision 1.28  1994/12/04  22:14:20  mike
 * apply instantaneous rotation to an object due to a force blow.
 * 
 * Revision 1.27  1994/08/01  13:29:42  matt
 * Made fvi() check holes in transparent walls, and changed fvi() calling
 * parms to take all input data in query structure.
 * 
 * Revision 1.26  1994/07/28  12:35:22  matt
 * Added prototype
 * 
 * Revision 1.25  1994/07/13  21:48:05  matt
 * FVI() and physics now keep lists of segments passed through which the
 * trigger code uses.
 * 
 * Revision 1.24  1994/06/30  19:01:55  matt
 * Moved flying controls code from physics.c to controls.c
 * 
 * Revision 1.23  1994/06/16  14:14:20  mike
 * Change physics_lookahead to return hit_info.
 * 
 * Revision 1.22  1994/06/09  09:58:43  matt
 * Moved find_vector_intersection() from physics.c to new file fvi.c
 * 
 * Revision 1.21  1994/05/20  16:11:07  matt
 * Added new parm, ignore_obj, to find_vector_intersection()
 * 
 * Revision 1.20  1994/05/20  15:16:58  matt
 * Added new fvi return type; took out some troublesome (and troubling) asserts
 * 
 * 
 * 
 */

#ifndef _PHYSICS_H
#define _PHYSICS_H

#include "vecmat.h"
#include "fvi.h"

//#define FL_NORMAL  0
//#define FL_TURBO   1
//#define FL_HOVER   2
//#define FL_REVERSE 3

//these global vars are set after a call to do_physics_sim().  Ugly, I know.
//list of segments went through
extern int phys_seglist[MAX_FVI_SEGS],n_phys_segs;

// Read contrls and set physics vars
void read_flying_controls( object * obj );

//Simulate a physics object for this frame
int do_physics_sim(object *obj);

//tell us what the given object will do (as far as hiting walls) in
//the given time (in seconds) t.  Igores acceleration (sorry) 
//if check_objects is set, check with objects, else just with walls
//returns fate, fills in hit time.  If fate==HIT_NONE, hit_time undefined
//	Stuff hit_info with fvi data as set by find_vector_intersection.
//for fvi_flags, refer to fvi.h for the fvi query flags
int physics_lookahead(object *obj,fix t,int fvi_flags,fix *hit_time, fvi_info *hit_info);

//Applies an instantaneous force on an object, resulting in an instantaneous
//change in velocity.
int phys_apply_force(object *obj,vms_vector *force_vec);
int phys_apply_rot(object *obj,vms_vector *force_vec);

//this routine will set the thrust for an object to a value that will
//(hopefully) maintain the object's current velocity
int set_thrust_from_velocity(object *obj);

//------------------------------------------------------------------------
// ASCENT: the movement of the player of the target game (a person who
// walks). The code is at the end of physics.c and in controls.c. The
// constants are from DTUGAME.EXE 1.3, see docs/re/physics.md.

#define DTU_FT						(F1_0/64)	// 0x400, the step of thrust, drag and gravity
#define DTU_REF_FRAMETIME		(3*DTU_FT)	// 0xC00, the frame time of the corrected rules (21.33 frames per second)

// The body. The executable takes the values from the bounding boxes of the
// frames of the polygon models of the player (dtu_player_body(), end of
// physics.c). These constants are used only when the models are missing:
// 2.5 is half of the least width of the player (5.0, code at 0x7470A), and
// 6.25 + 2.5/2 is 7.5, about the height of the center of a player above
// the floor.
#define DTU_BODY_RAD				0x28000		// 2.5
#define DTU_BODY_HALF_HEIGHT	0x64000		// 6.25, from the center down to the feet

// ASCENT: the body of the player in the current animation frame
typedef struct dtu_body {
	fix	move_rad;		// radius of the move query (0x748D9 / 2)
	fix	move_rad2;		// second radius of the move query: the ceiling side gets 1.5 times it
	fix	floor_rad;		// radius of the floor search (the floor side gets one half of it)
	fix	floor_height;	// the floor search: from the center to the feet
	fix	half_height;	// body_half_height[0]: the head test and the knee-high query
	fix	eye_y;			// frame_eye[frame].y, added to the eye
	int	model, frame;	// the model of the hand and its frame, -1: no model
} dtu_body;
void dtu_player_body(dtu_body *b);

#define DTU_EYE_OFFSET_DEFAULT	0x30000	// 3.0 above the center when the table has no eye_offset (0x1F5A9)
#define DTU_BOB_HEIGHT			0x5333		// 0.325, the view goes up and down by this value in a walk
#define DTU_FLOOR_PROBE			0x80000		// 8.0, how far below the feet the floor is searched
#define DTU_FLOOR_EPS			0x1999		// 0.1, a distance to the floor below this value is 0
#define DTU_GRAVITY_K			0x30000		// 3.0, gravity for each unit of distance to the floor and each step
#define DTU_PUSH_UP_MAX			0x20000		// 2.0, the most that a frame lifts the player out of the floor
#define DTU_TERMINAL_VEL		((fix)0xfea20000)	// -350.0, the largest velocity down
#define DTU_SLOW_FALL_VEL		0xf0000		// 15.0, the largest velocity down with the object flag 0x80
#define DTU_WALL_BOUNCE			0x1c000		// 1.75, the part of the velocity into a wall that is taken away
#define DTU_KNEE_HIGH			0x58000		// 5.5
#define DTU_KNEE_LIFT			0x30000		// 3.0
#define DTU_JUMP_STEPS			15
#define DTU_JUMP_RATE			0x1c0000		// 28 steps of the jump for each second
#define DTU_JUMP_FLOOR_MAX		0x30000		// 3.0, no jump when the feet are higher above the floor
#define DTU_FLY_BOB_STEP		0x886			// 0.0333 of a circle on each frame
#define DTU_RUN_SCALE			0x18000		// 1.5
#define DTU_ROTTHRUST_SCALE	0x30000		// 3.0
#define DTU_LOOK_MIN				(-0x3000)	// the look pitch (fixang), up
#define DTU_LOOK_MAX				0x2fa0		// down
#define DTU_LOOK_RETURN			0x1194		// (0x1194 + FrameTime)/4 fixang on each frame after a glance

// New values, not in the executable. DTU_FIX_* and DTU_WALL_* belong to the
// corrections Dtu_fix_jump_path, Dtu_fix_ladder and Dtu_fix_wall_slide (always
// on, -origphys turns them off). DTU_IMP_* belong to the Improvements
// Dtu_improved_physics and Dtu_improved_jump of the menu (dtuopts.h).
#define DTU_STAND_HEIGHT			0x78000		// 7.5, the center of a player that stands is this high above the floor
#define DTU_IMP_STOP_TAU			0x400			// 1/64 s, time constant of the stop and of the slide to the side
#define DTU_IMP_STOP_TIME		0x1999		// 0.1 s, the horizontal velocity is 0 this long after the keys are released
#define DTU_WALL_SLIDE			0x10000		// 1.0, Dtu_fix_wall_slide: the part of the velocity into a wall that is taken away
#define DTU_WALL_UNSTICK_SPEED	0x80000		// 8.0, Dtu_fix_wall_slide: the velocity away from the edge of a side that the body touches
#define DTU_FIX_LADDER_EXIT_SPEED	0x180000	// 24.0, the step from the top of a ladder onto the floor
#define DTU_FIX_LADDER_EXIT_TIME	0x8000		// 0.5 s, the most time of that step
#define DTU_FIX_LADDER_GRAB_FALL	0xa0000		// 10.0, a fall of less takes hold of a ladder in reach
#define DTU_FIX_JUMP_HEIGHT		0x78000		// 7.5, the top of the jump table (0x14530E)
#define DTU_FIX_JUMP_V0			0x380000		// 56.0 = 4 * height / air time, the velocity up at the start
#define DTU_FIX_JUMP_GRAVITY	0xd11111		// 209.07 = 8 * height / air time^2, air time 15/28 s
#define DTU_FIX_JUMP_HSPEED		0xe0000		// 14.0 = 0.5 * 28, the speed forward of the original jump (one step 0.5, 28 steps per second)
#define DTU_IMP_AIR_ACCEL		0x3c0000		// 60.0, the steering in the air
#define DTU_IMP_COYOTE_TIME		0x2666		// 0.15 s, a jump is still possible this long after the floor or the ladder is left

// Dtu_player_state: the byte at 0x1AE of the object of the executable
#define DTU_PS_FALLING			0x20
#define DTU_PS_JUMPING			0x40
#define DTU_PS_CLIMBING			0x80

// Bits of the int at 0x88 of the object (dtu.m_88). The names with a
// number have no known meaning.
#define DTU_OF_80					0x00000080	// no gravity, and the velocity down is 15.0 at most (guess: feather fall)
#define DTU_OF_FLY				0x00000800	// thrust in the direction of the view, no gravity
#define DTU_OF_1000				0x00001000	// the floor type does not make the player slow
#define DTU_OF_FAST				0x00002000	// two times the thrust in x and z (guess: haste)
#define DTU_OF_JUMP_SPELL		0x00080000	// two times the length of a jump
#define DTU_OF_LEVITATE			0x00200000	// no gravity, no velocity in x and z ("Levitate cancelled.")
#define DTU_OF_SLOW				0x08000000	// half of the thrust in x and z
#define DTU_OF_HELD				0x04004000	// the player does not move
#define DTU_OF_NO_GRAVITY		(DTU_OF_80|DTU_OF_FLY|DTU_OF_LEVITATE)

extern int Dtu_player_walk;
extern int Dtu_fix_physics_steps,Dtu_fix_frame_drag,Dtu_fix_stop_decay;
extern int Dtu_fix_floor_push,Dtu_fix_look_return,Dtu_fix_fly_bob,Dtu_fix_turn_ramp,Dtu_fix_jump_exit;
extern int Dtu_fix_jump_path,Dtu_fix_ladder,Dtu_fix_wall_slide;
// ASCENT: the turn keys for Dtu_fix_turn_ramp (controls.c): the direction
// (-1, 0, 1), the time the key was down at the start of the frame, the
// rotthrust of a whole turn key, and the rotthrust of the mouse and the
// joystick in this frame.
extern int Dtu_turn_dir;
extern fix Dtu_turn_hold, Dtu_turn_full_thrust, Dtu_turn_other_thrust;
extern ubyte Dtu_player_state;
extern fixang Dtu_look_pitch;
extern int Dtu_move_mode;
extern fix Dtu_floor_dist;
extern int Dtu_jump_skill;

int dtu_player_physics_sim(object *obj);
fix dtu_get_floor_dist(object *obj);
void dtu_player_jump(object *obj);
void dtu_stop_decay(object *obj);
void dtu_stop_decay_reset(void);
void dtu_player_eye(vms_vector *eye);

// In controls.c
void dtu_read_controls(void);
void dtu_move_log(object *obj);

#endif
