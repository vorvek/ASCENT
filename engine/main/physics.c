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
 * $Source: f:/miner/source/main/rcs/physics.c $
 * $Revision: 2.2 $
 * $Author: john $
 * $Date: 1995/03/24 14:48:54 $
 * 
 * Code for flying through the mines
 * 
 * $Log: physics.c $
 * Revision 2.2  1995/03/24  14:48:54  john
 * Added cheat for player to go thru walls.
 * 
 * Revision 2.1  1995/03/20  18:15:59  john
 * Added code to not store the normals in the segment structure.
 * 
 * Revision 2.0  1995/02/27  11:32:06  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.213  1995/02/22  13:40:48  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.212  1995/02/22  13:24:42  john
 * Removed the vecmat anonymous unions.
 * 
 * Revision 1.211  1995/02/06  19:46:59  matt
 * New function (untested), set_thrust_from_velocity()
 * 
 * Revision 1.210  1995/02/02  16:26:12  matt
 * Changed assert that was causing a problem
 * 
 * Revision 1.209  1995/02/02  14:07:00  matt
 * Fixed confusion about which segment you are touching when you're 
 * touching a wall.  This manifested itself in spurious lava burns.
 * 
 * Revision 1.208  1995/02/01  21:03:24  john
 * Lintified.
 * 
 * Revision 1.207  1995/01/25  13:53:35  rob
 * Removed an Int3 from multiplayer games.
 * 
 * Revision 1.206  1995/01/23  17:30:47  rob
 * Removed Int3 on bogus sim time.
 * 
 * Revision 1.205  1995/01/17  11:08:56  matt
 * Disable new-ish FVI edge checking for all objects except the player,
 * since it was causing problems with the fusion cannon.
 * 
 * Revision 1.204  1995/01/05  09:43:49  matt
 * Took out int3s from new code
 * 
 * Revision 1.203  1995/01/04  22:19:23  matt
 * Added hack to keep player from squeezing through closed walls/doors
 * 
 * Revision 1.202  1995/01/02  12:38:48  mike
 * physics hack to crazy josh not get hung up on proximity bombs.  Matt notified via email.
 * 
 * Revision 1.201  1994/12/13  15:39:22  mike
 * #ifndef NDEBUG some code.
 * 
 * Revision 1.200  1994/12/13  13:28:34  yuan
 * Fixed type.
 * 
 * Revision 1.199  1994/12/13  13:25:00  matt
 * Made bump hack compile out if so desired
 * 
 * Revision 1.198  1994/12/13  12:02:39  matt
 * Added hack to bump player a little if stuck
 * 
 * Revision 1.197  1994/12/12  00:32:23  matt
 * When objects other than player go out of mine, jerk to center of segment
 * 
 * Revision 1.196  1994/12/10  22:52:42  mike
 * make physics left-the-mine checking always be in.
 * 
 * Revision 1.195  1994/12/08  00:53:01  mike
 * oops...phys rot bug.
 * 
 * Revision 1.194  1994/12/07  12:54:54  mike
 * tweak rotvel applied from collisions.
 * 
 * Revision 1.193  1994/12/07  00:36:08  mike
 * fix phys_apply_rot for robots -- ai was bashing effect in next frame.
 * 
 * Revision 1.192  1994/12/05  17:23:10  matt
 * Made a bunch of debug code compile out
 * 
 * Revision 1.191  1994/12/05  16:30:10  matt
 * Was illegally changing an object's segment...shoot me.
 * 
 * Revision 1.190  1994/12/05  11:58:51  mike
 * fix stupid apply_force_rot() bug.
 * 
 * Revision 1.189  1994/12/05  09:42:17  mike
 * fix 0 mag problem when object applies force to itself.
 * 
 * Revision 1.188  1994/12/04  22:48:40  matt
 * Physics & FVI now only build seglist for player objects, and they 
 * responsilby deal with buffer full conditions
 * 
 * Revision 1.187  1994/12/04  22:14:07  mike
 * apply instantaneous rotation to an object due to a force blow.
 * 
 * Revision 1.186  1994/12/04  18:51:30  matt
 * When weapons get stuck, delete them!
 * 
 * Revision 1.185  1994/12/04  18:38:56  matt
 * Added better handling of point-not-in-seg problem
 * 
 * Revision 1.184  1994/11/27  23:13:42  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.183  1994/11/25  23:46:18  matt
 * Fixed drag problems with framerates over 60Hz
 * 
 * Revision 1.182  1994/11/25  22:15:52  matt
 * Added asserts to try to trap frametime < 0 bug
 * 
 * Revision 1.181  1994/11/21  11:42:44  mike
 * ndebug stuff.
 * 
 * Revision 1.180  1994/11/19  15:15:04  mike
 * remove unused code and data
 * 
 * Revision 1.179  1994/11/16  11:25:22  matt
 * Abort physics if negative frametime
 * 
 * Revision 1.178  1994/10/05  19:50:41  rob
 * Removed a non-critical Int3 where an object's segnum is checked.
 * Left mprintf message.
 * 
 * Revision 1.177  1994/10/03  22:57:50  matt
 * Fixed problem with matrix corruption of non-moving (but rotating) objects
 * 
 * Revision 1.176  1994/09/28  09:23:28  mike
 * Add useful information to mprintf(1,... error messages.
 * 
 * Revision 1.175  1994/09/21  17:16:54  mike
 * Make objects stuck in doors go away when door opens.
 * 
 * Revision 1.174  1994/09/12  14:19:06  matt
 * Drag & thrust now handled differently
 * 
 * Revision 1.173  1994/09/09  14:21:12  matt
 * Use new thrust flag
 * 
 * Revision 1.172  1994/09/08  16:21:57  matt
 * Cleaned up player-hit-wall code, and added object scrape handling
 * Also added weapon-on-weapon hit sound
 * 
 * Revision 1.171  1994/09/02  12:30:37  matt
 * Fixed weapons which go through objects
 * 
 * Revision 1.170  1994/09/02  11:55:14  mike
 * Kill redefinition of a constant which is properly defined in object.h
 * 
 * Revision 1.169  1994/09/02  11:35:01  matt
 * Fixed typo
 * 
 * Revision 1.168  1994/09/02  11:32:48  matt
 * Fixed object/object collisions, so you can't fly through robots anymore.
 * Cleaned up object damage system.
 * 
 * Revision 1.167  1994/08/30  21:58:15  matt
 * Made phys_apply_force() do nothing to an object if it's not a phys object
 * 
 * Revision 1.166  1994/08/26  10:47:01  john
 * New version of controls.
 * 
 * Revision 1.165  1994/08/25  21:53:57  mike
 * Prevent counts of -1 which eventually add up to a positive number in do_ai_frame, causing
 * the too-many-retries behavior.
 * 
 * Revision 1.164  1994/08/25  18:43:33  john
 * First revision of new control code.
 * 
 * Revision 1.163  1994/08/17  22:18:05  mike
 * Make robots which have rotvel or rotthrust, but not movement, move.
 * 
 * Revision 1.162  1994/08/13  17:31:18  mike
 * retry count stuff.
 * 
 * Revision 1.161  1994/08/11  18:59:16  mike
 * *** empty log message ***
 * 
 * Revision 1.160  1994/08/10  19:53:47  mike
 * Debug code (which is still in...)
 * and adapt to changed interface to create_path_to_player.
 * 
 * Revision 1.159  1994/08/08  21:38:43  matt
 * Cleaned up a code a little and optimized a little
 * 
 * Revision 1.158  1994/08/08  15:21:50  mike
 * Trap retry count >= 4, but don't do AI hack unless >= 6.
 * 
 * Revision 1.157  1994/08/08  11:47:15  matt
 * Cleaned up fvi and physics a little
 * 
 * Revision 1.156  1994/08/05  10:10:10  yuan
 * Commented out debug stuff that was killing framerate.
 * 
 * Revision 1.155  1994/08/04  19:12:36  matt
 * Changed a bunch of vecmat calls to use multiple-function routines, and to
 * allow the use of C macros for some functions
 * 
 * Revision 1.154  1994/08/04  16:33:57  mike
 * Kill a pile of RCS stuff.
 * Call create_path_to_player for a stuck object.
 * 
 * Revision 1.153  1994/08/04  00:21:02  matt
 * Cleaned up fvi & physics error handling; put in code to make sure objects
 * are in correct segment; simplified segment finding for objects and points
 * 
 * Revision 1.152  1994/08/01  16:25:34  matt
 * Check for moved_time == 0 when computing hit speed
 * 
 * Revision 1.151  1994/08/01  13:30:32  matt
 * Made fvi() check holes in transparent walls, and changed fvi() calling
 * parms to take all input data in query structure.
 * 
 * Revision 1.150  1994/07/29  23:41:46  matt
 * Fixed turn banking, which changed when I added rotational velocity
 * 
 * Revision 1.149  1994/07/27  20:53:23  matt
 * Added rotational drag & thrust, so turning now has momemtum like moving
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: physics.c 2.2 1995/03/24 14:48:54 john Exp $";
#pragma on (unreferenced)

//@@#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>

#include "joy.h"
#include "mono.h"
#include "error.h"

#include "inferno.h"
#include "segment.h"
#include "object.h"
#include "physics.h"
#include "key.h"
#include "game.h"
#include "collide.h"
#include "fvi.h"
#include "newdemo.h"
#include "timer.h"
#include "ai.h"
#include "wall.h"
#include "laser.h"

//Global variables for physics system

#define ROLL_RATE 		0x2000
#define DAMP_ANG 			0x400                  //min angle to bank

#define TURNROLL_SCALE	(0x4ec4/2)

#define BUMP_HACK	1		//if defined, bump player when he gets stuck

//--unused-- int mike_mode=0;

//check point against each side of segment. return bitmask, where bit
//set means behind that side

int Physics_cheat_flag = 0;

//##//returns the distance of a point (checkp) from a plane (defined by norm & planep) 
//##fix dist_to_plane(vms_vector *checkp,vms_vector *norm,vms_vector *planep)
//##{
//##    vms_vector deltap;
//##
//##    vm_vec_sub(&deltap,checkp,planep);
//##
//##    return vm_vec_dot(&deltap,norm);
//##}
 
//--unused-- int dpjm_old_joy_x, dpjm_old_joy_y;

int floor_levelling=0;

//--unused-- level_with_floor()
//--unused-- {
//--unused-- 	floor_levelling=1;
//--unused-- }

//make sure matrix is orthogonal
check_and_fix_matrix(vms_matrix *m)
{
	vms_matrix tempm;

	vm_vector_2_matrix(&tempm,&m->fvec,&m->uvec,NULL);
	*m  = tempm;
}


void do_physics_align_object( object * obj )
{
	vms_vector desired_upvec;
	fixang delta_ang,roll_ang;
	//vms_vector forvec = {0,0,f1_0};
	vms_matrix temp_matrix;
	fix d,largest_d=-f1_0;
	int i,best_side;


	// bank player according to segment orientation

	//find side of segment that player is most alligned with

	for (i=0;i<6;i++) {
		#ifdef COMPACT_SEGS
			vms_vector _tv1;
			get_side_normal( &Segments[obj->segnum], i, 0, &_tv1 );
			d = vm_vec_dot(&_tv1,&obj->orient.uvec);
		#else					
			d = vm_vec_dot(&Segments[obj->segnum].sides[i].normals[0],&obj->orient.uvec);
		#endif

		if (d > largest_d) {largest_d = d; best_side=i;}
	}

	if (floor_levelling) {

		// old way: used floor's normal as upvec
		#ifdef COMPACT_SEGS
			get_side_normal(&Segments[obj->segnum], 3, 0, &desired_upvec );			
		#else
			desired_upvec = Segments[obj->segnum].sides[3].normals[0];
		#endif

	}
	else  // new player leveling code: use normal of side closest to our up vec
		if (get_num_faces(&Segments[obj->segnum].sides[best_side])==2) {
			#ifdef COMPACT_SEGS
				vms_vector normals[2];
				get_side_normals(&Segments[obj->segnum], best_side, &normals[0], &normals[1] );			

				desired_upvec.x = (normals[0].x + normals[1].x) / 2;
				desired_upvec.y = (normals[0].y + normals[1].y) / 2;
				desired_upvec.z = (normals[0].z + normals[1].z) / 2;

				vm_vec_normalize(&desired_upvec);
			#else
				side *s = &Segments[obj->segnum].sides[best_side];
				desired_upvec.x = (s->normals[0].x + s->normals[1].x) / 2;
				desired_upvec.y = (s->normals[0].y + s->normals[1].y) / 2;
				desired_upvec.z = (s->normals[0].z + s->normals[1].z) / 2;
		
				vm_vec_normalize(&desired_upvec);
			#endif
		}
		else
			#ifdef COMPACT_SEGS
				get_side_normal(&Segments[obj->segnum], best_side, 0, &desired_upvec );			
			#else
				desired_upvec = Segments[obj->segnum].sides[best_side].normals[0];
			#endif

	if (labs(vm_vec_dot(&desired_upvec,&obj->orient.fvec)) < f1_0/2) {
		fixang save_delta_ang;
		vms_angvec tangles;
		
		vm_vector_2_matrix(&temp_matrix,&obj->orient.fvec,&desired_upvec,NULL);

		save_delta_ang = delta_ang = vm_vec_delta_ang(&obj->orient.uvec,&temp_matrix.uvec,&obj->orient.fvec);

		delta_ang += obj->mtype.phys_info.turnroll;

		if (abs(delta_ang) > DAMP_ANG) {
			vms_matrix rotmat, new_pm;

			roll_ang = fixmul(FrameTime,ROLL_RATE);

			if (abs(delta_ang) < roll_ang) roll_ang = delta_ang;
			else if (delta_ang<0) roll_ang = -roll_ang;

			tangles.p = tangles.h = 0;  tangles.b = roll_ang;
			vm_angles_2_matrix(&rotmat,&tangles);

			vm_matrix_x_matrix(&new_pm,&obj->orient,&rotmat);
			obj->orient = new_pm;
		}
		else floor_levelling=0;
	}

}

set_object_turnroll(object *obj)
{
	fixang desired_bank;

	desired_bank = -fixmul(obj->mtype.phys_info.rotvel.y,TURNROLL_SCALE);

	if (obj->mtype.phys_info.turnroll != desired_bank) {
		fixang delta_ang,max_roll;

		max_roll = fixmul(ROLL_RATE,FrameTime);

		delta_ang = desired_bank - obj->mtype.phys_info.turnroll;

		if (labs(delta_ang) < max_roll)
			max_roll = delta_ang;
		else
			if (delta_ang < 0)
				max_roll = -max_roll;

		obj->mtype.phys_info.turnroll += max_roll;
	}

}

//list of segments went through
int phys_seglist[MAX_FVI_SEGS],n_phys_segs;


#define MAX_IGNORE_OBJS 100

#ifndef NDEBUG
#define EXTRA_DEBUG 1		//no extra debug when NDEBUG is on
#endif

#ifdef EXTRA_DEBUG
object *debug_obj=NULL;
#endif

#define XYZ(v) (v)->x,(v)->y,(v)->z

int	Total_retries=0, Total_sims=0;

#ifndef NDEBUG
int	Dont_move_ai_objects=0;
#endif

#define FT (f1_0/64)

extern int disable_new_fvi_stuff;
//	-----------------------------------------------------------------------------------------------------------
// add rotational velocity & acceleration
void do_physics_sim_rot(object *obj)
{
	vms_angvec	tangles;
	vms_matrix	rotmat,new_orient;
	//fix			rotdrag_scale;
	physics_info *pi;

	Assert(FrameTime > 0);	//Get MATT if hit this!

	pi = &obj->mtype.phys_info;

	if (!(pi->rotvel.x || pi->rotvel.y || pi->rotvel.z || pi->rotthrust.x || pi->rotthrust.y || pi->rotthrust.z))
		return;

	if (obj->mtype.phys_info.drag) {
		int count;
		vms_vector accel;
		fix drag,r,k;

		count = FrameTime / FT;
		r = FrameTime % FT;
		k = fixdiv(r,FT);

		drag = (obj->mtype.phys_info.drag*5)/2;

		if (obj->mtype.phys_info.flags & PF_USES_THRUST) {

			vm_vec_copy_scale(&accel,&obj->mtype.phys_info.rotthrust,fixdiv(f1_0,obj->mtype.phys_info.mass));

			while (count--) {

				vm_vec_add2(&obj->mtype.phys_info.rotvel,&accel);

				vm_vec_scale(&obj->mtype.phys_info.rotvel,f1_0-drag);
			}

			//do linear scale on remaining bit of time

			vm_vec_scale_add2(&obj->mtype.phys_info.rotvel,&accel,k);
			vm_vec_scale(&obj->mtype.phys_info.rotvel,f1_0-fixmul(k,drag));
		}
		else {
			fix total_drag=f1_0;

			while (count--)
				total_drag = fixmul(total_drag,f1_0-drag);

			//do linear scale on remaining bit of time

			total_drag = fixmul(total_drag,f1_0-fixmul(k,drag));

			vm_vec_scale(&obj->mtype.phys_info.rotvel,total_drag);
		}

	}

	//mprintf( (0, "Rot vel = %.3f,%.3f,%.3f\n", f2fl(obj->mtype.phys_info.rotvel.x),f2fl(obj->mtype.phys_info.rotvel.y), f2fl(obj->mtype.phys_info.rotvel.z) ));

	//now rotate object 

	//unrotate object for bank caused by turn
	if (obj->mtype.phys_info.turnroll) {
		vms_matrix new_pm;

		tangles.p = tangles.h = 0;
		tangles.b = -obj->mtype.phys_info.turnroll;
		vm_angles_2_matrix(&rotmat,&tangles);
		vm_matrix_x_matrix(&new_pm,&obj->orient,&rotmat);
		obj->orient = new_pm;
	}
  
	tangles.p = fixmul(obj->mtype.phys_info.rotvel.x,FrameTime);
	tangles.h = fixmul(obj->mtype.phys_info.rotvel.y,FrameTime);
	tangles.b  = fixmul(obj->mtype.phys_info.rotvel.z,FrameTime);

	vm_angles_2_matrix(&rotmat,&tangles);
	vm_matrix_x_matrix(&new_orient,&obj->orient,&rotmat);
	obj->orient = new_orient;

	if (obj->mtype.phys_info.flags & PF_TURNROLL)
		set_object_turnroll(obj);

	//re-rotate object for bank caused by turn
	if (obj->mtype.phys_info.turnroll) {
		vms_matrix new_pm;

		tangles.p = tangles.h = 0;
		tangles.b = obj->mtype.phys_info.turnroll;
		vm_angles_2_matrix(&rotmat,&tangles);
		vm_matrix_x_matrix(&new_pm,&obj->orient,&rotmat);
		obj->orient = new_pm;
	}

	check_and_fix_matrix(&obj->orient);
}

//	-----------------------------------------------------------------------------------------------------------
//Simulate a physics object for this frame
do_physics_sim(object *obj)
{
	int ignore_obj_list[MAX_IGNORE_OBJS],n_ignore_objs;
	int iseg;
	int try_again;
	int fate;
	vms_vector frame_vec;			//movement in this frame
	vms_vector new_pos,ipos;		//position after this frame
	int count=0;
	int objnum;
	int WallHitSeg, WallHitSide;
	fvi_info hit_info;
	fvi_query fq;
	vms_vector save_pos;
	int save_seg;
	fix drag;
	fix sim_time;
	vms_vector start_pos;
	int obj_stopped=0;
	fix moved_time;			//how long objected moved before hit something
	vms_vector save_p0,save_p1;
	physics_info *pi;
	int orig_segnum = obj->segnum;

	Assert(obj->type != OBJ_NONE);
	Assert(obj->movement_type == MT_PHYSICS);

#ifndef NDEBUG
if (Dont_move_ai_objects)
	if (obj->control_type == CT_AI)
		return;
#endif

	pi = &obj->mtype.phys_info;

	do_physics_sim_rot(obj);

	if (!(pi->velocity.x || pi->velocity.y || pi->velocity.z || pi->thrust.x || pi->thrust.y || pi->thrust.z))
		return;

	objnum = obj-Objects;

	n_phys_segs = 0;

	disable_new_fvi_stuff = (obj->type != OBJ_PLAYER);

	sim_time = FrameTime;

//debug_obj = obj;

	#ifdef EXTRA_DEBUG
	if (obj == debug_obj) {
		printf("object %d:\n  start pos = %x %x %x\n",objnum,XYZ(&obj->pos));
		printf("  thrust = %x %x %x\n",XYZ(&obj->mtype.phys_info.thrust));
		printf("  sim_time = %x\n",sim_time);
	}

	//check for correct object segment 
	if(!get_seg_masks(&obj->pos,obj->segnum,0).centermask==0) {
		#ifndef NDEBUG
		mprintf((0,"Warning: object %d not in given seg!\n",objnum));
		#endif
		//Int3();  Removed by Rob 10/5/94
		if (!update_object_seg(obj)) {
			#ifndef NDEBUG
			mprintf((0,"Warning: can't find seg for object %d - moving\n",objnum));
			#endif
			if (!(Game_mode & GM_MULTI))
				Int3();
			compute_segment_center(&obj->pos,&Segments[obj->segnum]);
			obj->pos.x += objnum;
		}
	}
	#endif

	start_pos = obj->pos;

	n_ignore_objs = 0;

	Assert(obj->mtype.phys_info.brakes==0);		//brakes not used anymore?

		//if uses thrust, cannot have zero drag
	Assert(!(obj->mtype.phys_info.flags&PF_USES_THRUST) || obj->mtype.phys_info.drag!=0);

//mprintf((0,"thrust=%x  speed=%x\n",vm_vec_mag(&obj->mtype.phys_info.thrust),vm_vec_mag(&obj->mtype.phys_info.velocity)));

	//do thrust & drag
	
	if ((drag = obj->mtype.phys_info.drag) != 0) {

		int count;
		vms_vector accel;
		fix r,k;

		count = sim_time / FT;
		r = sim_time % FT;
		k = fixdiv(r,FT);

		if (obj->mtype.phys_info.flags & PF_USES_THRUST) {

			vm_vec_copy_scale(&accel,&obj->mtype.phys_info.thrust,fixdiv(f1_0,obj->mtype.phys_info.mass));

			while (count--) {

				vm_vec_add2(&obj->mtype.phys_info.velocity,&accel);

				vm_vec_scale(&obj->mtype.phys_info.velocity,f1_0-drag);
			}

			//do linear scale on remaining bit of time

			vm_vec_scale_add2(&obj->mtype.phys_info.velocity,&accel,k);

			vm_vec_scale(&obj->mtype.phys_info.velocity,f1_0-fixmul(k,drag));
		}
		else {
			fix total_drag=f1_0;

			while (count--)
				total_drag = fixmul(total_drag,f1_0-drag);

			//do linear scale on remaining bit of time

			total_drag = fixmul(total_drag,f1_0-fixmul(k,drag));

			vm_vec_scale(&obj->mtype.phys_info.velocity,total_drag);
		}
	}

	#ifdef EXTRA_DEBUG
	if (obj == debug_obj)
		printf("  velocity = %x %x %x\n",XYZ(&obj->mtype.phys_info.velocity));
	#endif

	do {
		try_again = 0;

		//Move the object
		vm_vec_copy_scale(&frame_vec, &obj->mtype.phys_info.velocity, sim_time);

		#ifdef EXTRA_DEBUG
		if (obj == debug_obj)
			printf("  pass %d, frame_vec = %x %x %x\n",count,XYZ(&frame_vec));
		#endif

		if ( (frame_vec.x==0) && (frame_vec.y==0) && (frame_vec.z==0) )	
			break;

		count++;

		//	If retry count is getting large, then we are trying to do something stupid.
		if ( count > 3) 	{
			if (obj->type == OBJ_PLAYER) {
				if (count > 8)
					break;
			} else
				break;
		}

		vm_vec_add(&new_pos,&obj->pos,&frame_vec);

		#ifdef EXTRA_DEBUG
		if (obj == debug_obj)
			printf("   desired_pos  = %x %x %x\n",XYZ(&new_pos));
		#endif

		ignore_obj_list[n_ignore_objs] = -1;

		#ifdef EXTRA_DEBUG
		if (obj == debug_obj) {
			printf("   FVI parms: p0 = %8x %8x %8x, segnum=%x, size=%x\n",XYZ(&obj->pos),obj->segnum,obj->size);
			printf("              p1 = %8x %8x %8x\n",XYZ(&new_pos));
		}
		#endif

		fq.p0						= &obj->pos;
		fq.startseg				= obj->segnum;
		fq.p1						= &new_pos;
		fq.rad					= obj->size;
		fq.thisobjnum			= objnum;
		fq.ignore_obj_list	= ignore_obj_list;
		fq.flags					= FQ_CHECK_OBJS;

		if (obj->type == OBJ_WEAPON)
			fq.flags |= FQ_TRANSPOINT;

		if (obj->type == OBJ_PLAYER)
			fq.flags |= FQ_GET_SEGLIST;

//@@			if (get_seg_masks(&obj->pos,obj->segnum,0).centermask!=0)
//@@				Int3();

save_p0 = *fq.p0;
save_p1 = *fq.p1;


		fate = find_vector_intersection(&fq,&hit_info);
		//	Matt: Mike's hack.
		if (fate == HIT_OBJECT) {
			object	*objp = &Objects[hit_info.hit_object];

			if ((objp->type == OBJ_WEAPON) && (objp->id == PROXIMITY_ID))
				count--;
		}

		#ifndef NDEBUG
		if (fate == HIT_BAD_P0) {
			mprintf((0,"Warning: Bad p0 in physics!  Object = %i, type = %i [%s]\n", obj-Objects, obj->type, Object_type_names[obj->type]));
			Int3();
		}
		#endif

		if (obj->type == OBJ_PLAYER) {
			int i;

			if (n_phys_segs && phys_seglist[n_phys_segs-1]==hit_info.seglist[0])
				n_phys_segs--;

			for (i=0;(i<hit_info.n_segs) && (n_phys_segs<MAX_FVI_SEGS-1);  )
				phys_seglist[n_phys_segs++] = hit_info.seglist[i++];
		}

		#ifdef EXTRA_DEBUG
		if (obj == debug_obj)
			printf("   fate  = %d, hit_pnt = %8x %8x %8x\n",fate,XYZ(&hit_info.hit_pnt));;
		#endif

		ipos = hit_info.hit_pnt;
		iseg = hit_info.hit_seg;
		WallHitSide = hit_info.hit_side;
		WallHitSeg = hit_info.hit_side_seg;

		if (iseg==-1) {		//some sort of horrible error
			#ifndef NDEBUG
			mprintf((1,"iseg==-1 in physics!  Object = %i, type = %i (%s)\n", obj-Objects, obj->type, Object_type_names[obj->type]));
			#endif
			//Int3();
			//compute_segment_center(&ipos,&Segments[obj->segnum]);
			//ipos.x += objnum;
			//iseg = obj->segnum;
			//fate = HIT_NONE;
			if (obj->type == OBJ_WEAPON)
				obj->flags |= OF_SHOULD_BE_DEAD;
			break;
		}

		Assert(!((fate==HIT_WALL) && ((WallHitSeg == -1) || (WallHitSeg > Highest_segment_index))));

		//if(!get_seg_masks(&hit_info.hit_pnt,hit_info.hit_seg,0).centermask==0)
		//	Int3();

		save_pos = obj->pos;			//save the object's position
		save_seg = obj->segnum;

		// update object's position and segment number
		obj->pos = ipos;

		#ifdef EXTRA_DEBUG
		if (obj == debug_obj)
			printf("   new pos = %x %x %x\n",XYZ(&obj->pos));
		#endif

		if ( iseg != obj->segnum )
			obj_relink(objnum, iseg );

		//if start point not in segment, move object to center of segment
		if (get_seg_masks(&obj->pos,obj->segnum,0).centermask!=0) {
			int n;

			if ((n=find_object_seg(obj))==-1) {
				//Int3();
				if (obj->type==OBJ_PLAYER && (n=find_point_seg(&obj->last_pos,obj->segnum))!=-1) {
					obj->pos = obj->last_pos;
					obj_relink(objnum, n );
				}
				else {
					compute_segment_center(&obj->pos,&Segments[obj->segnum]);
					obj->pos.x += objnum;
				}
				if (obj->type == OBJ_WEAPON)
					obj->flags |= OF_SHOULD_BE_DEAD;
			}
			return;
		}

		//calulate new sim time
		{
			//vms_vector moved_vec;
			vms_vector moved_vec_n;
			fix attempted_dist,actual_dist;

			actual_dist = vm_vec_normalized_dir(&moved_vec_n,&obj->pos,&save_pos);

			if (fate==HIT_WALL && vm_vec_dot(&moved_vec_n,&frame_vec) < 0) {		//moved backwards

				//don't change position or sim_time

//*******					mprintf((0,"Obj %d moved backwards\n",obj-Objects));

				#ifdef EXTRA_DEBUG
				if (obj == debug_obj)
					printf("   Warning: moved backwards!\n");
				#endif

				obj->pos = save_pos;
		
				//iseg = obj->segnum;		//don't change segment

				obj_relink(objnum, save_seg );

				moved_time = 0;
			}
			else {
				fix old_sim_time;

				//if (obj == debug_obj)
				//	printf("   moved_vec = %x %x %x\n",XYZ(&moved_vec));
			
				attempted_dist = vm_vec_mag(&frame_vec);

				old_sim_time = sim_time;

				sim_time = fixmuldiv(sim_time,attempted_dist-actual_dist,attempted_dist);

				moved_time = old_sim_time - sim_time;

				if (sim_time < 0 || sim_time>old_sim_time) {
					#ifndef NDEBUG
					mprintf((0,"Bogus sim_time = %x, old = %x\n",sim_time,old_sim_time));
					if (obj == debug_obj)
						printf("   Bogus sim_time = %x, old = %x, attempted_dist = %x, actual_dist = %x\n",sim_time,old_sim_time,attempted_dist,actual_dist);
					//Int3(); Removed by Rob
					#endif
					sim_time = old_sim_time;
					//WHY DOES THIS HAPPEN??

					moved_time = 0;
				}
			}

			#ifdef EXTRA_DEBUG
			if (obj == debug_obj)
				printf("   new sim_time = %x\n",sim_time);
			#endif

		}


		switch( fate )		{

			case HIT_WALL:		{
				vms_vector moved_v;
				//@@fix total_d,moved_d;
				fix hit_speed,wall_part;
	
				// Find hit speed	

				vm_vec_sub(&moved_v,&obj->pos,&save_pos);

				wall_part = vm_vec_dot(&moved_v,&hit_info.hit_wallnorm);

				if (wall_part != 0 && moved_time>0 && (hit_speed=-fixdiv(wall_part,moved_time))>0)
					collide_object_with_wall( obj, hit_speed, WallHitSeg, WallHitSide, &hit_info.hit_pnt );
				else
					scrape_object_on_wall(obj, WallHitSeg, WallHitSide, &hit_info.hit_pnt );

				Assert( WallHitSeg > -1 );				
				Assert( WallHitSide > -1 );				

				if ( !(obj->flags&OF_SHOULD_BE_DEAD) )	{


					Assert(! (obj->mtype.phys_info.flags & PF_STICK && obj->mtype.phys_info.flags & PF_BOUNCE));	//can't be bounce and stick

					if (obj->mtype.phys_info.flags & PF_STICK) {		//stop moving

						// mprintf((0, "Object %i stuck at %i:%i\n", obj-Objects, WallHitSeg, WallHitSide));
						add_stuck_object(obj, WallHitSeg, WallHitSide);

						vm_vec_zero(&obj->mtype.phys_info.velocity);
						obj_stopped = 1;
						try_again = 0;
					}
					else {					// Slide object along wall

						//We're constrained by wall, so subtract wall part from 
						//velocity vector

						wall_part = vm_vec_dot(&hit_info.hit_wallnorm,&obj->mtype.phys_info.velocity);

						if (obj->mtype.phys_info.flags & PF_BOUNCE)		//bounce off wall
							wall_part *= 2;	//Subtract out wall part twice to achieve bounce

						vm_vec_scale_add2(&obj->mtype.phys_info.velocity,&hit_info.hit_wallnorm,-wall_part);

						#ifdef EXTRA_DEBUG
						if (obj == debug_obj) {
							printf("   sliding - wall_norm %x %x %x\n",wall_part,XYZ(&hit_info.hit_wallnorm));
							printf("   wall_part %x, new velocity = %x %x %x\n",wall_part,XYZ(&obj->mtype.phys_info.velocity));
						}
						#endif

						try_again = 1;
					}
				}
				break;
			}

			case HIT_OBJECT:		{
				vms_vector old_vel;

				// Mark the hit object so that on a retry the fvi code
				// ignores this object.

				Assert(hit_info.hit_object != -1);

				//	Calculcate the hit point between the two objects.
				{	vms_vector	*ppos0, *ppos1, pos_hit;
					fix			size0, size1;
					ppos0 = &Objects[hit_info.hit_object].pos;
					ppos1 = &obj->pos;
					size0 = Objects[hit_info.hit_object].size;
					size1 = obj->size;
					Assert(size0+size1 != 0);	// Error, both sizes are 0, so how did they collide, anyway?!?
					//vm_vec_scale(vm_vec_sub(&pos_hit, ppos1, ppos0), fixdiv(size0, size0 + size1));
					//vm_vec_add2(&pos_hit, ppos0);
					vm_vec_sub(&pos_hit, ppos1, ppos0);
					vm_vec_scale_add(&pos_hit,ppos0,&pos_hit,fixdiv(size0, size0 + size1));

					old_vel = obj->mtype.phys_info.velocity;

					collide_two_objects( obj, &Objects[hit_info.hit_object], &pos_hit);

				}

				// Let object continue its movement
				if ( !(obj->flags&OF_SHOULD_BE_DEAD)  )	{
					//obj->pos = save_pos;

					if (obj->mtype.phys_info.flags&PF_PERSISTENT || (old_vel.x == obj->mtype.phys_info.velocity.x && old_vel.y == obj->mtype.phys_info.velocity.y && old_vel.z == obj->mtype.phys_info.velocity.z)) {
						//if (Objects[hit_info.hit_object].type == OBJ_POWERUP)
							ignore_obj_list[n_ignore_objs++] = hit_info.hit_object;
						try_again = 1;
					}
				}

				break;
			}	
			case HIT_NONE:		
				break;

			#ifndef NDEBUG
			case HIT_BAD_P0:
				Int3();		// Unexpected collision type: start point not in specified segment.
				mprintf((0,"Warning: Bad p0 in physics!!!\n"));
				break;
			default:
				// Unknown collision type returned from find_vector_intersection!!
				Int3();
				break;
			#endif
		}

	} while ( try_again );

	//	Pass retry count info to AI.
	if (obj->control_type == CT_AI) {
		if (count > 0) {
			Ai_local_info[objnum].retry_count = count-1;
			Total_retries += count-1;
			Total_sims++;
		}
	}

	if (! obj_stopped)	{	//Set velocity from actual movement
		vms_vector moved_vec;
		vm_vec_sub(&moved_vec,&obj->pos,&start_pos);
		vm_vec_copy_scale(&obj->mtype.phys_info.velocity,&moved_vec,fixdiv(f1_0,FrameTime));

		#ifdef BUMP_HACK
		if (obj==ConsoleObject && (obj->mtype.phys_info.velocity.x==0 && obj->mtype.phys_info.velocity.y==0 && obj->mtype.phys_info.velocity.z==0) &&
			  !(obj->mtype.phys_info.thrust.x==0 && obj->mtype.phys_info.thrust.y==0 && obj->mtype.phys_info.thrust.z==0)) {
			vms_vector center,bump_vec;

			//bump player a little towards center of segment to unstick

			compute_segment_center(&center,&Segments[obj->segnum]);
			vm_vec_normalized_dir_quick(&bump_vec,&center,&obj->pos);
			vm_vec_scale_add2(&obj->pos,&bump_vec,obj->size/5);
		}
		#endif
	}

	//Assert(check_point_in_seg(&obj->pos,obj->segnum,0).centermask==0);

	//if (obj->control_type == CT_FLYING)
	if (obj->mtype.phys_info.flags & PF_LEVELLING)
		do_physics_align_object( obj );


	//hack to keep player from going through closed doors
	if (obj->type==OBJ_PLAYER && obj->segnum!=orig_segnum && (Physics_cheat_flag!=0xBADA55) ) {
		int sidenum;

		sidenum = find_connect_side(&Segments[obj->segnum],&Segments[orig_segnum]);

		if (sidenum != -1) {

			if (! (WALL_IS_DOORWAY(&Segments[orig_segnum],sidenum) & WID_FLY_FLAG)) {
				side *s;
				int vertnum,num_faces,i;
				fix dist;
				int vertex_list[6];

				//bump object back

				s = &Segments[orig_segnum].sides[sidenum];

				create_abs_vertex_lists( &num_faces, vertex_list, orig_segnum, sidenum);

				//let's pretend this wall is not triangulated
				vertnum = vertex_list[0];
				for (i=1;i<4;i++)
					if (vertex_list[i] < vertnum)
						vertnum = vertex_list[i];

				#ifdef COMPACT_SEGS
					{
					vms_vector _vn;
					get_side_normal(&Segments[orig_segnum], sidenum, 0, &_vn );
					dist = vm_dist_to_plane(&start_pos, &_vn, &Vertices[vertnum]);
					vm_vec_scale_add(&obj->pos,&start_pos,&_vn,obj->size-dist);
					}
				#else
					dist = vm_dist_to_plane(&start_pos, &s->normals[0], &Vertices[vertnum]);
					vm_vec_scale_add(&obj->pos,&start_pos,&s->normals[0],obj->size-dist);
				#endif
				update_object_seg(obj);

			}
		}
	}

//--WE ALWYS WANT THIS IN, MATT AND MIKE DECISION ON 12/10/94, TWO MONTHS AFTER FINAL 	#ifndef NDEBUG
	//if end point not in segment, move object to last pos, or segment center
	if (get_seg_masks(&obj->pos,obj->segnum,0).centermask!=0) {
		if (find_object_seg(obj)==-1) {
			int n;

			//Int3();
			if (obj->type==OBJ_PLAYER && (n=find_point_seg(&obj->last_pos,obj->segnum))!=-1) {
				obj->pos = obj->last_pos;
				obj_relink(objnum, n );
			}
			else {
				compute_segment_center(&obj->pos,&Segments[obj->segnum]);
				obj->pos.x += objnum;
			}
			if (obj->type == OBJ_WEAPON)
				obj->flags |= OF_SHOULD_BE_DEAD;
		}
	}
//--WE ALWYS WANT THIS IN, MATT AND MIKE DECISION ON 12/10/94, TWO MONTHS AFTER FINAL 	#endif


}

//--unused-- //tell us what the given object will do (as far as hiting walls) in
//--unused-- //the given time (in seconds) t.  Igores acceleration (sorry) 
//--unused-- //if check_objects is set, check with objects, else just with walls
//--unused-- //returns fate, fills in hit time.  If fate==HIT_NONE, hit_time undefined
//--unused-- int physics_lookahead(object *obj,fix t,int fvi_flags,fix *hit_time, fvi_info *hit_info)
//--unused-- {
//--unused-- 	vms_vector new_pos;
//--unused-- 	int objnum,fate;
//--unused-- 	fvi_query fq;
//--unused-- 
//--unused-- 	Assert(obj->movement_type == MT_PHYSICS);
//--unused-- 
//--unused-- 	objnum = obj-Objects;
//--unused-- 
//--unused-- 	vm_vec_scale_add(&new_pos, &obj->pos, &obj->mtype.phys_info.velocity, t);
//--unused-- 
//--unused-- 	fq.p0						= &obj->pos;
//--unused-- 	fq.startseg				= obj->segnum;
//--unused-- 	fq.p1						= &new_pos;
//--unused-- 	fq.rad					= obj->size;
//--unused-- 	fq.thisobjnum			= objnum;
//--unused-- 	fq.ignore_obj_list	= NULL;
//--unused-- 	fq.flags					= fvi_flags;
//--unused-- 
//--unused-- 	fate = find_vector_intersection(&fq,hit_info);
//--unused-- 
//--unused-- 	if (fate != HIT_NONE) {
//--unused-- 		fix dist,speed;
//--unused-- 
//--unused-- 		dist = vm_vec_dist(&obj->pos, &hit_info->hit_pnt);
//--unused-- 
//--unused-- 		speed = vm_vec_mag(&obj->mtype.phys_info.velocity);
//--unused-- 
//--unused-- 		*hit_time = fixdiv(dist,speed);
//--unused-- 
//--unused-- 	}
//--unused-- 
//--unused-- 	return fate;
//--unused-- 
//--unused-- }

//Applies an instantaneous force on an object, resulting in an instantaneous
//change in velocity.
phys_apply_force(object *obj,vms_vector *force_vec)
{

	if (obj->movement_type != MT_PHYSICS)
		return;

	//Add in acceleration due to force
	vm_vec_scale_add2(&obj->mtype.phys_info.velocity,force_vec,fixdiv(f1_0,obj->mtype.phys_info.mass));


}

//	----------------------------------------------------------------
//	Do *dest = *delta unless:
//				*delta is pretty small
//		and	they are of different signs.
void physics_set_rotvel_and_saturate(fix *dest, fix delta)
{
	if ((delta ^ *dest) < 0) {
		if (abs(delta) < F1_0/8) {
			// mprintf((0, "D"));
			*dest = delta/4;
		} else
			// mprintf((0, "d"));
			*dest = delta;
	} else {
		// mprintf((0, "!"));
		*dest = delta;
	}
}

//	------------------------------------------------------------------------------------------------------
//	Note: This is the old ai_turn_towards_vector code.
//	phys_apply_rot used to call ai_turn_towards_vector until I fixed it, which broke phys_apply_rot.
void physics_turn_towards_vector(vms_vector *goal_vector, object *obj, fix rate)
{
	vms_angvec	dest_angles, cur_angles;
	fix			delta_p, delta_h;
	vms_vector	*rotvel_ptr = &obj->mtype.phys_info.rotvel;

	// Make this object turn towards the goal_vector.  Changes orientation, doesn't change direction of movement.
	// If no one moves, will be facing goal_vector in 1 second.

	//	Detect null vector.
	if ((goal_vector->x == 0) && (goal_vector->y == 0) && (goal_vector->z == 0))
		return;

	//	Make morph objects turn more slowly.
	if (obj->control_type == CT_MORPH)
		rate *= 2;

	vm_extract_angles_vector(&dest_angles, goal_vector);
	vm_extract_angles_vector(&cur_angles, &obj->orient.fvec);

	delta_p = (dest_angles.p - cur_angles.p);
	delta_h = (dest_angles.h - cur_angles.h);

	if (delta_p > F1_0/2) delta_p = dest_angles.p - cur_angles.p - F1_0;
	if (delta_p < -F1_0/2) delta_p = dest_angles.p - cur_angles.p + F1_0;
	if (delta_h > F1_0/2) delta_h = dest_angles.h - cur_angles.h - F1_0;
	if (delta_h < -F1_0/2) delta_h = dest_angles.h - cur_angles.h + F1_0;

	delta_p = fixdiv(delta_p, rate);
	delta_h = fixdiv(delta_h, rate);

	if (abs(delta_p) < F1_0/16) delta_p *= 4;
	if (abs(delta_h) < F1_0/16) delta_h *= 4;

	physics_set_rotvel_and_saturate(&rotvel_ptr->x, delta_p);
	physics_set_rotvel_and_saturate(&rotvel_ptr->y, delta_h);
	rotvel_ptr->z = 0;
}

//	-----------------------------------------------------------------------------
//	Applies an instantaneous whack on an object, resulting in an instantaneous
//	change in orientation.
phys_apply_rot(object *obj,vms_vector *force_vec)
{
	fix	rate, vecmag;

	if (obj->movement_type != MT_PHYSICS)
		return;

	vecmag = vm_vec_mag(force_vec)/8;
	if (vecmag < F1_0/256)
		rate = 4*F1_0;
	else if (vecmag < obj->mtype.phys_info.mass >> 14)
		rate = 4*F1_0;
	else {
		rate = fixdiv(obj->mtype.phys_info.mass, vecmag);
		if (obj->type == OBJ_ROBOT) {
			if (rate < F1_0/4)
				rate = F1_0/4;
			obj->ctype.ai_info.SKIP_AI_COUNT = 2;
		} else {
			if (rate < F1_0/2)
				rate = F1_0/2;
		}
	}

	//	Turn amount inversely proportional to mass.  Third parameter is seconds to do 360 turn.
	physics_turn_towards_vector(force_vec, obj, rate);


}


//this routine will set the thrust for an object to a value that will
//(hopefully) maintain the object's current velocity
set_thrust_from_velocity(object *obj)
{
	fix k;

	Assert(obj->movement_type == MT_PHYSICS);

	k = fixmuldiv(obj->mtype.phys_info.mass,obj->mtype.phys_info.drag,(f1_0-obj->mtype.phys_info.drag));

	vm_vec_copy_scale(&obj->mtype.phys_info.thrust,&obj->mtype.phys_info.velocity,k);

}

//------------------------------------------------------------------------
// ASCENT: new, to the end of the file. The movement of the player of
// Descent to Undermountain: a person who walks on the floor.
//
// DTUGAME.EXE 1.3 does not move the player with do_physics_sim(). The
// function object_move_one() (0x69B93) calls pcons.c (0x9DC66) for the
// player, pmon.c (0x9F153) for a monster and pweapon.c (0xA03A8) for a
// weapon. pcons.c uses new functions of physics.c (0x6C30E to 0x6CD45).
// The code below follows these functions. The address of the original
// code is at each function. See docs/re/physics.md for the facts, the
// guesses and the parts that are not in the port.

#include "bm.h"
#include "gameseg.h"
#include "render.h"
#include "dtuobj.h"
#include "dtugame.h"		// ASCENT: Dtu_objects_enabled
#include "gauges.h"
#include "player.h"
#include "controls.h"
#include "kconfig.h"
#include "dtuthief.h"		// ASCENT: the climb and the slow sneak of the thief
#include "dtuopts.h"		// ASCENT: Dtu_improved_physics, Dtu_improved_jump
#include "digi.h"			// ASCENT: the sound of a fall
#include "dtusolid.h"		// ASCENT: Dtu_improved_physics, scenery as solid bodies
#include <math.h>

// The setting for the movement: 1 is the person of the target game, 0 is
// the ship of Descent 1 (option -fly).
int Dtu_player_walk = 1;

// The settings for the places where the original movement depends on the
// frame rate. 1 (default) is the corrected rule, 0 is the rule of the
// executable. The corrected rules give the movement of the executable at
// a frame time of DTU_REF_FRAMETIME. The option -origphys clears them all.
int Dtu_fix_physics_steps = 1;	// thrust, drag and gravity steps: all steps of 1/64 second, not 2 at most
int Dtu_fix_frame_drag = 1;		// the extra drag while gravity acts: one time per reference frame, not per frame
int Dtu_fix_stop_decay = 1;		// velocity/3 with no forward input (and velocity/4 in levitation): per reference frame
int Dtu_fix_floor_push = 1;		// lift out of the floor: 2.0 per reference frame, not per frame
int Dtu_fix_look_return = 1;		// return of the view after a glance: per reference frame
int Dtu_fix_fly_bob = 1;			// phase of the up and down motion in flight: per reference frame
int Dtu_fix_turn_ramp = 1;			// the turn keys reach the whole turn rate after 8 reference frames, not 8 frames
int Dtu_fix_jump_exit = 1;			// the velocity after a jump: the last step divided by the reference frame time, not by the frame time
int Dtu_fix_jump_path = 1;			// the jump: a throw with a constant gravity, with the height and the air time of the table (15 steps, 28 per second)
int Dtu_fix_ladder = 1;			// a ladder: no velocity into the wall, the hold of its top edge, the step onto the floor, the hold when the player steps down onto it
int Dtu_fix_wall_slide = 1;		// a wall takes away the velocity into it, with no bounce back
int Dtu_fix_wall_overlap = 1;	// a body that starts inside the reach of a wall is moved out of it (no stuck player)

// The volume of the land_sound of a fall (D-178). 1.3 (0x6C9FF to 0x6CA1E)
// takes 0x10000 / min(200 - fell, 1): 1.0 for a fall below 200, a division
// by zero at 200 and a negative volume above it (the sound code refuses
// it, 0x2E65B). Corrected (1, default): 1.0 for every fall. -origphys
// clears it: the volume of 1.3, and no sound at 200 in place of the fault.
int Dtu_fix_fall_sound = 1;

// The end of a fall of the player (0x6C662), the sibling of
// Dtu_fix_fall_land of the creatures (dtuobj.h). 1.3 ends a fall with no
// damage when a frame starts less than 1.0 above the floor (0x6C871,
// 0x6C89A); the damage of 0x6C8C8 comes only when the feet are in the
// floor or the move hits it. So whether a long fall hurts depends on the
// frame rate and on where the frames fall. Corrected (1, default): that end
// of a fall takes the damage too. -origphys clears it.
int Dtu_fix_fall_land_player = 1;

// The state of the player (the byte at 0x1AE of the object in the
// executable) and the values that belong to it.
ubyte Dtu_player_state = 0;
fixang Dtu_look_pitch = 0;			// the word at 0x13D9AE
int Dtu_move_mode = 0;				// the byte at 0x1452F0: 0 stands, 1 walks, 2 runs
fix Dtu_floor_dist = 0;				// result of the last dtu_apply_gravity()
int Dtu_jump_skill = 0;				// the word at 0x1D33DF of the player structure. Its source is not known

static int Dtu_bob_mode = 0;		// the byte at 0x14530D
fix Dtu_bob_phase = 0;		// the float at 0x38DFAC, as a fix angle
static fix Dtu_fly_bob_phase = 0;	// the float at 0x13D435, as a fix angle
static fix Dtu_fall_start_y = 0;	// the table at 0x30E2D4, for the player
static fix Dtu_jump_time = 0;		// 0x13D43A
static fix Dtu_jump_base_y = 0;	// 0x1E9C58
static int Dtu_jump_last_step = -1;	// 0x13D434
static vms_vector Dtu_jump_vec;	// 0x3BCB0C
static int Dtu_gravity_drag = 0;	// the byte at 0x13D470, inverted
static int Dtu_jump_set_vel = 0;	// the jump set the velocity of this frame

// The state of the corrections Dtu_fix_jump_path, Dtu_fix_ladder,
// Dtu_fix_wall_slide and of the Improvements (dtuopts.h). Not in the
// executable.
static fix Dtu_stop_time = 0;			// time since the movement keys were released
static int Dtu_ladder_seg = -1;		// the ladder that the player holds
static int Dtu_ladder_side = -1;
static fix Dtu_ladder_exit = 0;		// time left of the step from the top of a ladder
static vms_vector Dtu_ladder_exit_dir;
static fix Dtu_air_time = 0;			// time since the player stood or held a ladder
static int Dtu_jump_from_climb = 0;	// the jump starts from a ladder or a wall
static fix Dtu_jump_vy = 0;			// the velocity up of the jump at the end of the last frame
static vms_vector Dtu_jump_hvel;		// the velocity in x and z of the jump
static fix Dtu_jump_hmax = 0;			// the most speed in x and z of the steering in the air
static fix Dtu_wall_time = F1_0;		// time since the player touched a wall

// The height of the jump for each of the 15 steps (table at 0x14530E).
static fix Dtu_jump_table[DTU_JUMP_STEPS+1] = {
	0x18000, 0x30000, 0x48000, 0x60000, 0x6c000, 0x72000, 0x78000, 0x78000,
	0x72000, 0x6c000, 0x60000, 0x48000, 0x30000, 0x18000, 0, 0
};

#define DTU_HIT_FLOOR	5			// the new result of find_vector_intersection() of the executable

// ASCENT: Dtu_improved_physics, the capsule of the player (after
// dtu_player_body() below)
typedef struct dtu_capsule {
	fix rad;		// the radius: move_rad + 1.0, the reach of the walls of the original
	fix stand;		// the center above the feet: floor_height + floor_rad/2
	fix head;		// the top of the head above the center: body_half_height[0]
	fix depth;		// the depth of the round bottom: stand - rad - DTU_CAP_MARGIN, rad at most
} dtu_capsule;
typedef struct dtu_cap_support {
	fix y;			// the height of the feet on the support
	int edge;		// 1: an edge or a corner of a floor holds the body, not the face under the center
	fix slope;		// the slope of the bottom at the edge (dy/dx)
	fix gap;		// how much higher the support is than the floor under the axis (DTU_FLOOR_PROBE: no floor there)
	vms_vector dir;	// the direction in x and z from the edge to the axis
} dtu_cap_support;
static int dtu_capsule_on(void);
static void dtu_capsule_get(dtu_body *b,dtu_capsule *c);
static int dtu_capsule_support(vms_vector *axis,int segnum,dtu_capsule *c,fix feet,fix hi_edge,dtu_cap_support *s);
static fix dtu_capsule_floor_dist(object *obj);
static void dtu_relink(object *obj,int seg);
static dtu_cap_support Dtu_cap_sup;		// the support of the last dtu_capsule_floor_dist()
static int Dtu_cap_grounded = 0;		// the capsule stood on the floor at the end of the last gravity step
static int Dtu_stand_obj = -1;			// the scenery that holds the capsule (dtusolid.c), or -1

// Returns how many times a rule that the executable applies one time per
// frame must be applied in this frame. *acc is the time that is left.
static int dtu_ref_ticks(int corrected, fix *acc)
{
	int n;

	if (!corrected)
		return 1;

	*acc += FrameTime;
	n = *acc / DTU_REF_FRAMETIME;
	*acc -= n * DTU_REF_FRAMETIME;
	if (n > 8)
		n = 8;
	return n;
}

// The type of the floor: the two low bits of the byte at offset 1 of the
// bottom side (0x9DCC2). Type 1 makes the player slow and halves the
// damage of a fall. The segment below is used if there is one.
static int dtu_floor_type(object *obj)
{
	int segnum = obj->segnum;
	int below = Segments[segnum].children[WBOTTOM];

	if (below > -1)
		segnum = below;
	return Segments[segnum].sides[WBOTTOM].pad & 3;
}

// 0x6C30E. The distance from the feet of the object to the floor. It is
// negative when the feet are in the floor, and 8.0 when no floor is found.
// ASCENT: the radius and the height are those of the model of the hand
// and its frame (0x6C52D, dtu_player_body()).
fix dtu_get_floor_dist(object *obj)
{
	static fix result = 0;
	fvi_query fq;
	fvi_info hit;
	vms_vector p1;
	int fate;
	dtu_body b;

	// ASCENT: Dtu_improved_physics, the round bottom of the capsule
	if (dtu_capsule_on())
		return result = dtu_capsule_floor_dist(obj);

	dtu_player_body(&b);
	p1 = obj->pos;
	p1.y -= DTU_FLOOR_PROBE + b.floor_height;

	fq.p0 = &obj->pos;
	fq.p1 = &p1;
	fq.startseg = obj->segnum;
	fq.rad = b.floor_rad;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	Fvi_dtu_body = 2;
	fate = find_vector_intersection(&fq,&hit);
	Fvi_dtu_body = 0;

	if (fate == HIT_WALL)
		result = (obj->pos.y - hit.hit_pnt.y) - b.floor_height;
	else if (fate == HIT_BAD_P0)
		result = -abs(result);		// "RLH: %d has bad gravity!"
	else
		result = DTU_FLOOR_PROBE;

	if (abs(result) < DTU_FLOOR_EPS)
		result = 0;

	return result;
}

// 0x6CB0F
static void dtu_start_fall(object *obj)
{
	Dtu_player_state |= DTU_PS_FALLING;
	Dtu_fall_start_y = obj->pos.y;
}

int player_type_sound(char *key);		// dtuobj.c

// 0x6C9E8 to 0x6CA3D: the sound of a fall of the player. 0x2E816 stops the
// sounds linked to the player object, 0x2E610 links the land_sound of the
// player line (P+0x1D4E37, read by 0x1FA0B) to it with the distance 200.0.
// The solo flag of 0x2E610 (no sound while the last sample of the driver
// plays) belongs to the DOS sound driver and is left out. For the volume
// see Dtu_fix_fall_sound.
static void dtu_fall_sound(object *obj,int fell)
{
	int sound = player_type_sound("land_sound");
	int objnum = obj-Objects;
	fix volume = F1_0;

	digi_kill_sound_linked_to_object(objnum);
	if (!Dtu_fix_fall_sound) {
		int divisor = 200 - fell < 1 ? 200 - fell : 1;

		if (divisor == 0)
			return;		// 1.3: divide fault at 0x6CA1E
		volume = F1_0 / divisor;
	}
	if (sound >= 0)
		digi_link_sound_to_object2(sound,objnum,0,volume,i2f(200));
}

// 0x6C8C8. The damage of a fall, and the end of the fall. The executable
// lets a thief take hold of a wall here. That part is not in the port.
static void dtu_fall_damage(object *obj)
{
	if ((Dtu_player_state & DTU_PS_FALLING) && !(obj->dtu.m_88 & DTU_OF_NO_GRAVITY)) {
		int fell = (Dtu_fall_start_y - obj->pos.y) >> 17;		// "descent feet": half units

		// ASCENT: the log of -objlog (test aid)
		if (fell > 0)
			dtu_obj_log("fall: the player fell %d descent feet (0x6C8C8)",fell);
		if (fell > 10) {
			int base = fell/10;
			int damage = rand() % (base*5 + 1) + base;

			if ((Segments[obj->segnum].sides[WBOTTOM].pad & 3) == 1)
				damage >>= 1;

			mprintf((0,"RLH: Object %d fell %d descent feet.\n",(int)(obj-Objects),fell));
			if (obj == ConsoleObject)
				dtu_fall_sound(obj,fell);
			HUD_init_message("You fell %d feet!",fell);
			dtu_damage_player(obj,NULL,i2f(damage));		// ASCENT: was apply_damage_to_player() of Descent 1, see dtuobj.c
		}
	}

	Dtu_fall_start_y = obj->pos.y;
	Dtu_player_state &= ~DTU_PS_FALLING;
}

// 0x6C871, 0x6C89A: less than 1.0 above the floor (or on it) ends a fall.
// 1.3 clears the fall bit with no damage; Dtu_fix_fall_land_player gives
// the damage of 0x6C8C8.
static void dtu_fall_land(object *obj)
{
	if (Dtu_fix_fall_land_player && (Dtu_player_state & DTU_PS_FALLING))
		dtu_fall_damage(obj);
	Dtu_player_state &= ~DTU_PS_FALLING;
}

// 0x6C662. Gravity pulls with 3.0 times the distance to the floor for each
// step of 1/64 second. An object that is in the floor moves up.
static void dtu_apply_gravity(object *obj,vms_vector *accel)
{
	static fix push_acc = 0;
	fix d = dtu_get_floor_dist(obj);
	fix ad = abs(d);
	int no_gravity = (obj->dtu.m_88 & DTU_OF_NO_GRAVITY) || (Dtu_player_state & DTU_PS_CLIMBING) ||
		Dtu_ladder_exit > 0;		// ASCENT: Dtu_fix_ladder, the step from the top of a ladder

	// ASCENT: Dtu_improved_physics. The capsule that stood on the floor in
	// the last frame stays on it: a floor up to DTU_KNEE_HIGH above the
	// feet (a step, the edge of a step under the round bottom) lifts it at
	// once, a floor up to DTU_CAP_SNAP_DOWN below the feet (a step down, a
	// slope) takes it down at once. So it does not bounce down stairs and
	// slopes, and it is not pushed up in steps of 2.0.
	if (dtu_capsule_on()) {
		int was_grounded = Dtu_cap_grounded;

		Dtu_cap_grounded = 0;
		if (was_grounded && d != 0 && d != DTU_FLOOR_PROBE && d >= -DTU_KNEE_HIGH && d <= DTU_CAP_SNAP_DOWN &&
				!(Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_CLIMBING|DTU_PS_FALLING)) &&
				!(obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE)) && Dtu_ladder_exit <= 0 &&
				(d < 0 || obj->mtype.phys_info.velocity.y <= 0)) {
			vms_vector new_pos = obj->pos, head;
			dtu_body b;
			int seg;

			dtu_player_body(&b);
			new_pos.y -= d;
			head = new_pos;
			head.y += b.half_height;
			if ((seg = find_point_seg(&new_pos,obj->segnum)) >= 0 && (d > 0 || find_point_seg(&head,seg) >= 0)) {
				obj->pos = new_pos;
				dtu_relink(obj,seg);
				if (obj->mtype.phys_info.velocity.y < 0)
					obj->mtype.phys_info.velocity.y = 0;
				d = 0;
			}
		}
		if (d == 0 && !(Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_CLIMBING)) && !(obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE)))
			Dtu_cap_grounded = 1;
		ad = abs(d);
	}

	Dtu_floor_dist = d;
	Dtu_gravity_drag = 0;

	if (d < 0) {
		vms_vector new_pos,head;
		fix push = DTU_PUSH_UP_MAX;

		// The executable moves the object 2.0 at most on each frame.
		if (Dtu_fix_floor_push) {
			push_acc += fixmuldiv(DTU_PUSH_UP_MAX,FrameTime,DTU_REF_FRAMETIME);
			if (push_acc > DTU_PUSH_UP_MAX*4)
				push_acc = DTU_PUSH_UP_MAX*4;
			push = push_acc;
		}
		if (push > ad)
			push = ad;
		if (Dtu_fix_floor_push)
			push_acc -= push;

		new_pos = obj->pos;
		new_pos.y += push;
		head = new_pos;
		{
			// 0x6C7BE: body_half_height[0] of the model of the hand
			dtu_body b;

			dtu_player_body(&b);
			head.y += b.half_height;
		}
		if (find_point_seg(&head,obj->segnum) >= 0)
			obj->pos = new_pos;

		Dtu_player_state &= ~DTU_PS_CLIMBING;

		if (!no_gravity)
			dtu_fall_damage(obj);
	}
	else {
		push_acc = 0;

		if (no_gravity)
			;
		else if (d != 0) {
			accel->y -= fixmul(ad,DTU_GRAVITY_K);
			Dtu_gravity_drag = 1;
			if (ad < F1_0)
				dtu_fall_land(obj);
			else if (!(Dtu_player_state & DTU_PS_FALLING))
				dtu_start_fall(obj);
		}
		else
			dtu_fall_land(obj);
	}
}

// 0x6CB68. Thrust and drag in steps of 1/64 second, as do_physics_sim()
// of Descent 1. The executable does 2 steps at most on each frame, and it
// drops the rest of the frame when the frame has more than 2 steps. While
// gravity acts, the part step uses the whole drag.
static void dtu_integrate(object *obj,vms_vector *accel,fix sim_time)
{
	static fix step_acc = 0,drag_acc = 0;
	physics_info *pi = &obj->mtype.phys_info;
	fix drag = pi->drag;
	int count;
	fix k;

	if (Dtu_fix_physics_steps) {
		step_acc += sim_time;
		count = step_acc / DTU_FT;
		step_acc -= count * DTU_FT;
		if (count > 32)
			count = 32;
		k = 0;
	}
	else {
		count = sim_time / DTU_FT;
		k = fixdiv(sim_time % DTU_FT,DTU_FT);
		if (count > 2) {
			count = 2;
			k = 0;
		}
	}

	while (count--) {
		vm_vec_add2(&pi->velocity,accel);
		vm_vec_scale(&pi->velocity,F1_0-drag);
	}

	if (!Dtu_fix_physics_steps) {
		vm_vec_scale_add2(&pi->velocity,accel,k);
		if (!Dtu_gravity_drag || Dtu_fix_frame_drag)
			vm_vec_scale(&pi->velocity,F1_0-fixmul(k,drag));
	}

	if (Dtu_gravity_drag) {
		int n = dtu_ref_ticks(Dtu_fix_frame_drag,&drag_acc);

		while (n--)
			vm_vec_scale(&pi->velocity,F1_0-drag);
	}

	if (pi->velocity.y < DTU_TERMINAL_VEL)
		pi->velocity.y = DTU_TERMINAL_VEL;
}

// 0x9F093 "Jump stopped."
static void dtu_stop_jump(object *obj)
{
	if ((Dtu_player_state & DTU_PS_JUMPING) && obj->mtype.phys_info.velocity.y < 0) {
		dtu_fall_damage(obj);
		vm_vec_zero(&obj->mtype.phys_info.velocity);
		Dtu_player_state &= ~(DTU_PS_JUMPING|DTU_PS_FALLING);
	}
}

// 0x546CC. The jump key.
void dtu_player_jump(object *obj)
{
	if (obj->dtu.m_88 & DTU_OF_LEVITATE) {
		obj->dtu.m_88 &= ~DTU_OF_LEVITATE;
		HUD_init_message("Levitate cancelled.");
		return;
	}

	if ((Dtu_player_state & DTU_PS_JUMPING) || (obj->dtu.m_88 & DTU_OF_FLY))
		return;

	// ASCENT: Dtu_fix_jump_path, the jump with a constant gravity. From a
	// ladder or a wall the jump goes up and toward the wall, as the jump
	// of the executable does (its direction is that of the climb thrust).
	// Dtu_improved_jump: a jump is also possible a short time after the
	// player left the floor.
	if (Dtu_fix_jump_path) {
		if (Dtu_ladder_exit > 0)
			return;
		Dtu_jump_from_climb = (Dtu_player_state & DTU_PS_CLIMBING) != 0;
		if (!Dtu_jump_from_climb && dtu_get_floor_dist(obj) > DTU_JUMP_FLOOR_MAX &&
			!(Dtu_improved_jump && Dtu_air_time < DTU_IMP_COYOTE_TIME))
			return;
		Dtu_player_state &= ~DTU_PS_CLIMBING;
		Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
		Dtu_ladder_seg = -1;
		Dtu_jump_time = 0;
		Dtu_jump_base_y = obj->pos.y;
		Dtu_player_state |= DTU_PS_JUMPING;
		return;
	}

	if (dtu_get_floor_dist(obj) > DTU_JUMP_FLOOR_MAX && !(Dtu_player_state & DTU_PS_CLIMBING))
		return;

	Dtu_jump_time = 0;
	Dtu_jump_base_y = obj->pos.y;
	Dtu_player_state |= DTU_PS_JUMPING;
}

// 0x9E9D8. One frame of a jump. The jump has 15 steps and 28 steps for
// each second. The height comes from a table. The velocity is the way to
// the place of the step, divided by the frame time.
static void dtu_jump_frame(object *obj,fix sim_time)
{
	physics_info *pi = &obj->mtype.phys_info;
	int step,prev;

	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))
		return;
	if (Dtu_player_state & DTU_PS_CLIMBING)
		return;

	if (Dtu_jump_time == 0) {
		Dtu_jump_vec = pi->thrust;
		if (Dtu_jump_vec.x || Dtu_jump_vec.y || Dtu_jump_vec.z)
			vm_vec_normalize(&Dtu_jump_vec);
		Dtu_jump_vec.y = 0;
		vm_vec_scale(&Dtu_jump_vec,(Dtu_jump_skill << 13) + 0x8000);
		if (Dtu_move_mode == 2)
			vm_vec_scale(&Dtu_jump_vec,0x18000);
		if (obj->dtu.m_88 & DTU_OF_JUMP_SPELL)
			vm_vec_scale(&Dtu_jump_vec,0x20000);
		Dtu_jump_last_step = -1;
	}

	Dtu_jump_time += sim_time;
	step = fixmul(Dtu_jump_time,DTU_JUMP_RATE) >> 16;

	if (step == Dtu_jump_last_step) {
		vm_vec_zero(&pi->velocity);
		return;
	}

	prev = Dtu_jump_last_step;
	Dtu_jump_last_step = step;

	if (step >= DTU_JUMP_STEPS) {
		Dtu_player_state &= ~(DTU_PS_JUMPING|DTU_PS_FALLING);
		Dtu_jump_time = 0;
		vm_vec_copy_scale(&pi->velocity,&Dtu_jump_vec,i2f(DTU_JUMP_STEPS - prev));
		Dtu_jump_last_step = -1;
		pi->velocity.y = Dtu_jump_base_y - obj->pos.y;
		if (abs(dtu_get_floor_dist(obj) + pi->velocity.y) > DTU_JUMP_FLOOR_MAX)
			pi->velocity.y -= 0x18000;
	}
	else {
		vm_vec_copy_scale(&pi->velocity,&Dtu_jump_vec,i2f(step - prev));
		pi->velocity.y = Dtu_jump_table[step] - (obj->pos.y - Dtu_jump_base_y);
	}

	vm_vec_scale(&pi->velocity,fixdiv(F1_0,sim_time));
	Dtu_jump_set_vel = 1;
}

//------------------------------------------------------------------------
// ASCENT: the corrections Dtu_fix_jump_path and Dtu_fix_ladder, and the
// Improvements of the movement (dtuopts.h). New code, not in
// the executable. See docs/re/physics.md, sections 12 and 13.

// The factor exp(-t / tau): the part of a velocity that is left after the
// time t, independent of the frame rate.
static fix dtu_decay(fix t, fix tau)
{
	return fl2f(exp(-f2fl(t) / f2fl(tau)));
}

// The direction of the thrust in x and z, normalized. Returns 0 if there
// is no thrust in x and z.
static int dtu_thrust_dir(object *obj, vms_vector *dir)
{
	dir->x = obj->mtype.phys_info.thrust.x;
	dir->y = 0;
	dir->z = obj->mtype.phys_info.thrust.z;
	if (abs(dir->x) < 0x10 && abs(dir->z) < 0x10)
		return 0;
	vm_vec_normalize(dir);
	return 1;
}

// Dtu_fix_jump_path: the jump is a throw with a constant gravity. It has
// the height (7.5) and the air time (15/28 s) of the jump table of the
// executable at all frame rates, and the speed forward of the original
// jump (14.0, 1.5 times in a run, 2 times with the Jump spell). The jump
// ends on a floor, or becomes a fall of the game when the player is below
// the start of the jump. Dtu_improved_jump: the keys steer the jump a
// little.
static void dtu_jump_frame_ballistic(object *obj,fix sim_time)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector dir;
	fix vy0,vy1,vmove,d = Dtu_floor_dist;

	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))
		return;

	if (Dtu_jump_time == 0) {
		fix speed = fixmul((Dtu_jump_skill << 13) + 0x8000,i2f(28));

		if (Dtu_move_mode == 2)
			speed = fixmul(speed,0x18000);
		if (obj->dtu.m_88 & DTU_OF_JUMP_SPELL)
			speed = fixmul(speed,0x20000);

		vm_vec_zero(&Dtu_jump_hvel);
		if (Dtu_jump_from_climb) {
			// over the top of the ladder or the wall in front
			dir = obj->orient.fvec;
			dir.y = 0;
			vm_vec_normalize(&dir);
			vm_vec_copy_scale(&Dtu_jump_hvel,&dir,speed);
		}
		else if (dtu_thrust_dir(obj,&dir))
			vm_vec_copy_scale(&Dtu_jump_hvel,&dir,speed);

		Dtu_jump_hmax = speed > DTU_FIX_JUMP_HSPEED ? speed : DTU_FIX_JUMP_HSPEED;
		Dtu_jump_vy = DTU_FIX_JUMP_V0;
		Dtu_jump_last_step = -1;
	}
	Dtu_jump_time += sim_time;

	// Dtu_improved_jump: the steering in the air
	if (Dtu_improved_jump && dtu_thrust_dir(obj,&dir)) {
		fix m;

		vm_vec_scale_add2(&Dtu_jump_hvel,&dir,fixmul(DTU_IMP_AIR_ACCEL,sim_time));
		m = vm_vec_mag(&Dtu_jump_hvel);
		if (m > Dtu_jump_hmax)
			vm_vec_scale(&Dtu_jump_hvel,fixdiv(Dtu_jump_hmax,m));
	}

	// Constant gravity. The mean velocity of the frame gives the exact
	// path at each frame rate.
	vy0 = Dtu_jump_vy;
	vy1 = vy0 - fixmul(DTU_FIX_JUMP_GRAVITY,sim_time);
	vmove = (vy0 + vy1) / 2;

	if (vmove < 0 && d < DTU_FLOOR_PROBE && d + fixmul(vmove,sim_time) <= 0) {
		// the feet reach the floor in this frame: the jump ends on it
		vmove = d > 0 ? -fixdiv(d,sim_time) : 0;
		vy1 = 0;
		Dtu_player_state &= ~DTU_PS_JUMPING;
		Dtu_jump_time = 0;
		dtu_fall_damage(obj);
	}
	else if (vy1 < 0 && obj->pos.y + fixmul(vmove,sim_time) < Dtu_jump_base_y) {
		// below the start and no floor: a fall with the gravity of the game
		Dtu_player_state &= ~DTU_PS_JUMPING;
		Dtu_jump_time = 0;
		if (!(Dtu_player_state & DTU_PS_FALLING))
			dtu_start_fall(obj);
	}

	Dtu_jump_vy = vy1;
	pi->velocity.x = Dtu_jump_hvel.x;
	pi->velocity.y = vmove;
	pi->velocity.z = Dtu_jump_hvel.z;
}

// Dtu_improved_physics on the floor: no slide. The velocity in x and z
// that is not in the direction of the keys goes away with the time
// constant DTU_IMP_STOP_TAU, and all of it when no key is down. After
// DTU_IMP_STOP_TIME with no key the velocity in x and z is 0. The speed
// in the direction of the keys is that of the original rules.
static void dtu_improved_walk(object *obj,fix sim_time)
{
	physics_info *pi = &obj->mtype.phys_info;
	fix k = dtu_decay(sim_time,DTU_IMP_STOP_TAU);
	vms_vector dir;

	if (dtu_thrust_dir(obj,&dir)) {
		fix par = fixmul(pi->velocity.x,dir.x) + fixmul(pi->velocity.z,dir.z);
		fix px = pi->velocity.x - fixmul(dir.x,par);
		fix pz = pi->velocity.z - fixmul(dir.z,par);

		// Along a wall the velocity to the side stays: the player slides
		// along the wall as in the original.
		if (Dtu_wall_time < DTU_IMP_STOP_TIME)
			k = F1_0;
		if (par < 0)
			par = fixmul(par,k);
		pi->velocity.x = fixmul(dir.x,par) + fixmul(px,k);
		pi->velocity.z = fixmul(dir.z,par) + fixmul(pz,k);
		Dtu_stop_time = 0;
	}
	else {
		Dtu_stop_time += sim_time;
		if (Dtu_stop_time >= DTU_IMP_STOP_TIME) {
			pi->velocity.x = 0;
			pi->velocity.z = 0;
		}
		else {
			pi->velocity.x = fixmul(pi->velocity.x,k);
			pi->velocity.z = fixmul(pi->velocity.z,k);
		}
	}
}

// Dtu_fix_ladder: a ladder (not a wall that the thief climbs).
// The executable ends the climb when the center of the player is in a
// segment with no ladder in reach: at the top the feet are still a body
// height under the floor of the top, and the push of 0x9EBEB does not
// lift the player that far. Here the player holds the ladder of the last
// frame until the feet are above its top edge, and then steps onto the
// floor behind it. Returns 1 if this replaces the climb frame of
// dtuthief.c.
static int dtu_improved_ladder(object *obj)
{
	segment *segp;
	side *sidep;
	vms_vector c,rel;
	fix top,dist;
	int side,i;
	dtu_body b;

	side = dtu_thief_ladder_side(obj,obj->segnum);
	if (side >= 0) {
		Dtu_ladder_seg = obj->segnum;
		Dtu_ladder_side = side;
		return 0;
	}
	if (Dtu_ladder_seg < 0 || Dtu_ladder_seg > Highest_segment_index)
		return 0;

	segp = &Segments[Dtu_ladder_seg];
	sidep = &segp->sides[Dtu_ladder_side];
	compute_center_point_on_side(&c,segp,Dtu_ladder_side);
	vm_vec_sub(&rel,&obj->pos,&c);
	dist = vm_vec_dot(&rel,&sidep->normals[0]);
	if (abs(dist) > i2f(10))
		return 0;

	top = Vertices[segp->verts[Side_to_verts[Dtu_ladder_side][0]]].y;
	for (i=1; i<4; i++) {
		fix y = Vertices[segp->verts[Side_to_verts[Dtu_ladder_side][i]]].y;
		if (y > top)
			top = y;
	}

	Dtu_player_state &= ~DTU_PS_FALLING;

	// Nearer than 4.0: out of the wall, as 0x9EBEB does
	if (dist >= 0 && dist < i2f(4))
		vm_vec_scale_add2(&obj->pos,&sidep->normals[0],i2f(4) - dist);

	// The feet are above the top and the player climbs up: the step.
	dtu_player_body(&b);
	if (obj->pos.y - (b.floor_height + b.floor_rad/2) >= top && obj->mtype.phys_info.thrust.y > 0) {
		Dtu_player_state &= ~DTU_PS_CLIMBING;
		Dtu_ladder_seg = -1;
		Dtu_ladder_exit = DTU_FIX_LADDER_EXIT_TIME;
		Dtu_ladder_exit_dir = sidep->normals[0];
		Dtu_ladder_exit_dir.y = 0;
		vm_vec_negate(&Dtu_ladder_exit_dir);
		vm_vec_normalize(&Dtu_ladder_exit_dir);
	}
	return 1;
}

// Dtu_fix_ladder: a player who starts to fall near a ladder and
// faces it takes hold of it. So the player can climb down from the top:
// a step back over the edge.
static void dtu_improved_ladder_grab(object *obj,vms_vector *accel)
{
	int seg = obj->segnum,side;

	if (Dtu_player_state & (DTU_PS_CLIMBING|DTU_PS_JUMPING))
		return;
	if ((obj->dtu.m_88 & DTU_OF_NO_GRAVITY) || Dtu_ladder_exit > 0)
		return;
	if (Dtu_floor_dist < F1_0 || !(Dtu_player_state & DTU_PS_FALLING) || Dtu_fall_start_y - obj->pos.y > DTU_FIX_LADDER_GRAB_FALL)
		return;

	side = dtu_thief_ladder_side(obj,seg);
	if (side < 0) {
		seg = Segments[obj->segnum].children[WBOTTOM];
		side = seg >= 0 ? dtu_thief_ladder_side(obj,seg) : -1;
	}
	if (side < 0)
		return;

	Dtu_player_state |= DTU_PS_CLIMBING;
	Dtu_player_state &= ~DTU_PS_FALLING;
	Dtu_thief_bits &= ~DTU_TS_CLIMB_SKILL;
	Dtu_ladder_seg = seg;
	Dtu_ladder_side = side;
	vm_vec_zero(&obj->mtype.phys_info.velocity);
	accel->y = 0;
}

// 0x6B354. The rotation: only the heading. The drag is 5 times the drag
// of the object for each step of 1/64 second (Descent 1: 2.5 times).
static void dtu_physics_sim_rot(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_angvec tangles;
	vms_matrix rotmat,new_orient;

	// ASCENT: Dtu_improved_physics. The turn has no inertia: the turn rate
	// is the rate that the turn input gives at the end of the ramp of the
	// executable (rotthrust / mass * (1 - 5 drag) / (5 drag)), so the turn
	// stops when the input stops. The drag of the executable lets the view
	// turn on by 5 to 18 degrees after a turn key at the whole rate.
	if (Dtu_improved_physics && Dtu_player_walk && pi->drag && pi->mass) {
		fix drag = pi->drag*5;

		pi->rotvel.x = pi->rotvel.z = 0;
		pi->rotvel.y = fixmul(fixdiv(pi->rotthrust.y,pi->mass),fixdiv(F1_0-drag,drag));
		goto turn;
	}

	if (!(pi->rotvel.x || pi->rotvel.y || pi->rotvel.z || pi->rotthrust.x || pi->rotthrust.y || pi->rotthrust.z))
		return;

	// ASCENT: Dtu_fix_turn_ramp, the turn with inertia (no Improved
	// physics). The executable does the steps of a frame and then turns by
	// the new rate for the whole frame. At the reference frame time (3
	// steps) that gives the shape of the turn that the corrected rules keep;
	// at other frame rates the angle after a time was up to 30% different.
	// Here the steps run on a clock of reference frames: at the start of a
	// reference frame the 3 steps give its rate, and the angle grows by
	// that rate over the reference frame, in the game frames that it spans.
	// A press, a release or a change of the direction starts a reference
	// frame at once, as the first frame of a key does. The thrust of the
	// turn keys in reference frame k is k/8 of the whole thrust (the ramp of
	// 0x533DC, controls.c), from the time the key is down; the thrust of the
	// mouse and the joystick is that of the frame.
	if (Dtu_fix_turn_ramp && obj == ConsoleObject && pi->drag && pi->mass) {
		static fix ref_t = DTU_REF_FRAMETIME;
		static int last_dir = 0;
		static int64_t rest = 0;
		fix drag = pi->drag*5, left = FrameTime, accel = 0;
		int dir = Dtu_turn_dir, i;
		int64_t h = rest;

		if (dir != last_dir)
			ref_t = DTU_REF_FRAMETIME;
		last_dir = dir;
		pi->rotvel.x = pi->rotvel.z = 0;
		while (left > 0) {
			fix dt;

			if (ref_t >= DTU_REF_FRAMETIME) {
				// the time the key is down at the start of this reference frame
				fix held = Dtu_turn_hold + (FrameTime - left);
				int k = (held + DTU_REF_FRAMETIME/2) / DTU_REF_FRAMETIME + 1;
				fix thrust = Dtu_turn_other_thrust;

				if (k > 8)
					k = 8;
				if (dir)
					thrust += dir * (fix) (((int64_t) Dtu_turn_full_thrust * k) / 8);
				accel = fixdiv(thrust,pi->mass);
				for (i = 0; i < DTU_REF_FRAMETIME / DTU_FT; i++) {
					pi->rotvel.y += accel;
					pi->rotvel.y = fixmul(pi->rotvel.y,F1_0-drag);
				}
				ref_t = 0;
			}
			dt = DTU_REF_FRAMETIME - ref_t;
			if (dt > left)
				dt = left;
			h += (int64_t) pi->rotvel.y * dt;
			ref_t += dt;
			left -= dt;
		}
		tangles.p = tangles.b = 0;
		tangles.h = (fixang) (h >> 16);
		rest = h - ((h >> 16) << 16);
		if (!pi->rotvel.y)
			rest = 0;
		goto apply;
	}

	if (pi->drag) {
		static fix step_acc = 0;
		int count = FrameTime / DTU_FT;
		fix k = fixdiv(FrameTime % DTU_FT,DTU_FT);
		fix drag = pi->drag*5;

		// The part step is linear. With it the turn rate grows with the
		// frame rate (1.25 turns for each second at 21 frames, 1.76 at 200).
		// The corrected rule has only whole steps.
		if (Dtu_fix_physics_steps) {
			step_acc += FrameTime;
			count = step_acc / DTU_FT;
			step_acc -= count * DTU_FT;
			if (count > 32)
				count = 32;
			k = 0;
		}

		if (pi->flags & PF_USES_THRUST) {
			vms_vector accel;

			vm_vec_copy_scale(&accel,&pi->rotthrust,fixdiv(F1_0,pi->mass));

			while (count--) {
				vm_vec_add2(&pi->rotvel,&accel);
				vm_vec_scale(&pi->rotvel,F1_0-drag);
			}

			vm_vec_scale_add2(&pi->rotvel,&accel,k);
			vm_vec_scale(&pi->rotvel,F1_0-fixmul(k,drag));
		}
		else {
			fix total_drag = F1_0;

			while (count--)
				total_drag = fixmul(total_drag,F1_0-drag);
			total_drag = fixmul(total_drag,F1_0-fixmul(k,drag));
			vm_vec_scale(&pi->rotvel,total_drag);
		}
	}

turn:
	tangles.p = 0;
	tangles.b = 0;
	tangles.h = fixmul(pi->rotvel.y,FrameTime);
	// ASCENT: Dtu_fix_turn_ramp. The part of the angle below one unit is
	// kept for the next frame. Without it the turn lost 0.25% at 300
	// frames per second, and less at fewer frames.
	if (Dtu_fix_turn_ramp && obj == ConsoleObject) {
		static int64_t rest = 0;
		int64_t h = (int64_t) pi->rotvel.y * FrameTime + rest;

		tangles.h = (fixang) (h >> 16);
		rest = h - ((h >> 16) << 16);
		if (!pi->rotvel.y)
			rest = 0;
	}

apply:
	vm_angles_2_matrix(&rotmat,&tangles);
	vm_matrix_x_matrix(&new_orient,&obj->orient,&rotmat);
	obj->orient = new_orient;

	check_and_fix_matrix(&obj->orient);
}

// 0x6C186 "obj %d hit a kneeHigh Segment." The player hit a floor side.
// When the center of that side is above the center of the floor below the
// player by less than 5.5, the player gets a velocity up.
static int dtu_knee_high(object *obj,int hit_seg,int hit_side)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector c1,c2,p1;
	fix diff;
	dtu_body b;

	if (hit_seg < 0 || hit_seg > Highest_segment_index || hit_side < 0 || hit_side > 5)
		return 0;

	compute_center_point_on_side(&c1,&Segments[hit_seg],hit_side);

	// 0x6C234: body_half_height[0] of the model of the hand
	dtu_player_body(&b);
	p1 = obj->pos;
	p1.y -= DTU_FLOOR_PROBE + b.half_height;

	fq.p0 = &obj->pos;
	fq.p1 = &p1;
	fq.startseg = obj->segnum;
	fq.rad = 0x1000;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	if (find_vector_intersection(&fq,&hit) != HIT_WALL)
		return 0;
	if (hit.hit_side_seg < 0 || hit.hit_side < 0)
		return 0;

	compute_center_point_on_side(&c2,&Segments[hit.hit_side_seg],hit.hit_side);

	diff = c1.y - c2.y;
	if (diff > 0 && diff < DTU_KNEE_HIGH) {
		obj->mtype.phys_info.velocity.y = c1.y - obj->pos.y + DTU_KNEE_LIFT;
		return 1;
	}

	return 0;
}

// The rule of read_flying_controls() of the executable (0x2C025): with no
// forward input the velocity and the thrust in x and z become one third,
// one time on each frame.
static fix Dtu_stop_decay_acc = 0;

// The player has forward input. The next dtu_stop_decay() acts at once,
// as the first frame with no input does in the executable.
void dtu_stop_decay_reset(void)
{
	Dtu_stop_decay_acc = DTU_REF_FRAMETIME - 1;
}

void dtu_stop_decay(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	int n = dtu_ref_ticks(Dtu_fix_stop_decay,&Dtu_stop_decay_acc);

	while (n--) {
		pi->thrust.x /= 3;
		pi->thrust.z /= 3;
		pi->velocity.x /= 3;
		pi->velocity.z /= 3;
	}
}

// 0x76E9C in render.c of the executable: the eye of the player is above
// the center of the body, the view goes up and down with the steps, and
// the view has the pitch of the look keys.
void dtu_player_eye(vms_vector *eye)
{
	fix eye_offset = DTU_EYE_OFFSET_DEFAULT;

	if (!Dtu_player_walk || Player_is_dead) {
		Use_player_head_angles = 0;
		return;
	}

	if (N_player_types > 0 && Player_type_info[0].eye_offset)
		eye_offset = Player_type_info[0].eye_offset;

	vm_vec_scale_add2(eye,&ConsoleObject->orient.uvec,eye_offset);

	// 0x76EC8: the y of frame_eye of the frame of the model of the hand
	{
		dtu_body b;

		dtu_player_body(&b);
		eye->y += b.eye_y;
	}

	if (Dtu_bob_mode) {
		fix s;

		fix_sincos(Dtu_bob_phase,&s,NULL);
		eye->y += fixmul(DTU_BOB_HEIGHT,s);
	}

	Use_player_head_angles = 1;
	Player_head_angles.p = Dtu_look_pitch;
	Player_head_angles.b = 0;
	Player_head_angles.h = 0;
}

// 0x9DC66 (pcons.c). The movement of the player for one frame.
// ASCENT: the body of the player from its polygon models (0x748D9,
// 0x6C30E, 0x6C662, 0x6C186, 0x76E9C; docs/re/physics.md 4.1 and 4.8).
// "The model of the hand" is the model of the player object (object 0x162,
// dtu_combat_view_model(): race, sex, class and the weapon in the hand),
// "the frame" its animation frame (object 0x172, absolute).
//   move (0x748D9, 0x9DF34): model 16, not the model of the hand (0x748F4).
//     rad  = body_radius[0] / 2
//     rad2 = torso_half_height[frame] (box_flags & 8), else body_half_height[0]
//   floor search (0x6C52D): rad = body_radius[frame] / 2 and the height
//     torso_half_height[frame] of the model of the hand. The index is the
//     frame, not frame + 1 as in the other arrays of the frames (entry 0 is
//     the box of all frames), as in the executable.
//   head test of the lift out of the floor (0x6C7BE) and the knee-high
//     query (0x6C234): body_half_height[0] of the model of the hand.
//   eye (0x76EC8): frame_eye[frame].y of the model of the hand.
// The player models are the first models (piggy.c, 0x70303), so model 16
// is the 17th model of the archive of the player.
// Without the models the port takes the constants of physics.h.
#include "polyobj.h"
#include "args.h"
#include "dtucombat.h"
#include <string.h>

#define DTU_PLAYER_BODY_MODEL	16

static polymodel *dtu_body_model(int n)
{
	polymodel *po;

	if (n < 0 || n >= N_polygon_models)
		return NULL;
	po = &Polygon_models[n];
	if (!po->dtu_block || !(po->box_flags & DTU_BOX_BODY) || !po->body_radius || !po->body_half_height || po->n_frames <= 0)
		return NULL;
	return po;
}

void dtu_player_body(dtu_body *b)
{
	polymodel *hand = dtu_body_model(dtu_combat_view_model());
	polymodel *body = dtu_body_model(DTU_PLAYER_BODY_MODEL);
	int f = dtu_combat_view_frame();

	b->move_rad = DTU_BODY_RAD;
	b->move_rad2 = DTU_BODY_HALF_HEIGHT;
	b->floor_rad = DTU_BODY_RAD;
	b->floor_height = DTU_BODY_HALF_HEIGHT;
	b->half_height = DTU_BODY_HALF_HEIGHT;
	b->eye_y = 0;
	b->model = -1;
	b->frame = f;

	if (!Dtu_player_walk)
		return;

	if (hand && f >= 0 && f < hand->n_frames) {
		b->model = dtu_combat_view_model();
		b->floor_rad = fixmul(hand->body_radius[f],F1_0/2);
		b->floor_height = hand->torso_half_height ? hand->torso_half_height[f] : hand->body_half_height[0];
		b->half_height = hand->body_half_height[0];
		if (hand->frame_eye)
			b->eye_y = hand->frame_eye[f].y;
	}

	if (body) {
		b->move_rad = fixmul(body->body_radius[0],F1_0/2);
		if ((body->box_flags & DTU_BOX_TORSO) && body->torso_half_height && f >= 0 && f < body->n_frames)
			b->move_rad2 = body->torso_half_height[f];
		else
			b->move_rad2 = body->body_half_height[0];
	}
}

// -bodylog FILE: one line for each frame of the player with its body.
static void dtu_body_log(object *obj)
{
	static FILE *log = NULL;
	static int init = 0, frame = 0;
	dtu_body b;
	int t;

	if (!init) {
		init = 1;
		if ((t = FindArg("-bodylog")) != 0 && t+1 < Num_args)
			log = !strcmp(Args[t+1],"-") ? stdout : fopen(Args[t+1],"wt");
		// -startseg SEG.SIDE: a test starts at the center of the segment,
		// the view toward the center of the side (level y)
		if ((t = FindArg("-startseg")) != 0 && t+1 < Num_args) {
			int seg = atoi(Args[t+1]), side = 0;
			char *dot = strchr(Args[t+1],'.');

			if (dot)
				side = atoi(dot+1);
			if (seg >= 0 && seg <= Highest_segment_index && side >= 0 && side < 6) {
				vms_vector c, dir;

				compute_segment_center(&obj->pos,&Segments[seg]);
				compute_center_point_on_side(&c,&Segments[seg],side);
				vm_vec_sub(&dir,&c,&obj->pos);
				dir.y = 0;
				if (vm_vec_normalize(&dir) > 0)
					vm_vector_2_matrix(&obj->orient,&dir,NULL,NULL);
				vm_vec_zero(&obj->mtype.phys_info.velocity);
				if (seg != obj->segnum)
					obj_relink(obj-Objects,seg);
			}
		}
	}
	frame++;
	if (!log)
		return;
	dtu_player_body(&b);
	if (frame == 1) {
		polymodel *body = dtu_body_model(DTU_PLAYER_BODY_MODEL);
		fprintf(log,"BODYINFO model16 %s eye_offset %.4f\n",body ? Pof_names[DTU_PLAYER_BODY_MODEL] : "-",
			f2fl(N_player_types > 0 && Player_type_info[0].eye_offset ? Player_type_info[0].eye_offset : DTU_EYE_OFFSET_DEFAULT));
	}
	fprintf(log,"BODY %d t %.4f hand %d %s frame %d move_rad %.4f rad2 %.4f floor_rad %.4f floor_h %.4f stand %.4f half %.4f eye_y %.4f floor %.4f pos %.4f %.4f %.4f seg %d\n",
		frame,f2fl(GameTime),b.model,b.model >= 0 ? Pof_names[b.model] : "-",b.frame,
		f2fl(b.move_rad),f2fl(b.move_rad2),f2fl(b.floor_rad),f2fl(b.floor_height),f2fl(b.floor_height + b.floor_rad/2),
		f2fl(b.half_height),f2fl(b.eye_y),f2fl(Dtu_floor_dist),f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),obj->segnum);
	fflush(log);
}

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_physics. The capsule of the player.
//
// The body of the original (4.1) is a sphere at the center with a radius
// for each kind of side (fvi.c): the walls stop the center at rad + 1.0
// (4.6 for a human), the ceiling at rad2 * 1.5, and the floor is searched
// with a much smaller sphere (floor_rad/2, 1.8). So the body stands on a
// floor that is 1.8 wide while it is 9.2 wide for the walls. At the edge
// of a ledge the player falls while the body is still in the reach of
// the wall under the edge, and that start stuck the player (stuck save,
// Dtu_fix_wall_overlap). The edge of a step stops the body before the
// floor search finds the step (a dwarf at the step of 3.5 in BANE).
//
// The capsule: a vertical body with one radius R = rad + 1.0, from the
// feet to the top of the head.
// - The walls: the sphere of the original at the center (the query of the
//   original, with its floor and ceiling radii, the objects and the list
//   of the segments for the triggers), and a sphere at the head (center
//   at head - R, walls only, fvi mode 3). The earliest hit counts.
// - The floor: the round bottom. It stands on each solid side that faces
//   up and is under the axis (the floor search of the original, a line
//   down, finds the same), and on each edge and corner of a solid side
//   that is within R of the axis in x and z. The bottom is a half ellipsoid:
//   R wide and stand - R - 0.25 deep (2.65 for a human, 1.59 for a
//   dwarf), so the sphere at the center is always more than R above an
//   edge that holds the body. fvi stops a sphere at R from the plane of
//   a side when its center is less than R from the edge of the side
//   (check_sphere_to_face: the test is a slab, not round). A full round
//   bottom (depth R) would put the center into the reach of the wall
//   under the edge again.
// There is no sphere at the feet for the walls: in the fvi test a sphere
// there would stop at each step and edge (any riser within R of its
// center), and it would lower the reach of a jump onto a ledge by
// stand - R (2.9 for a human) against the original.
//
// Results: the player stands on a ledge until the axis is R past its
// edge, sinks with the round bottom on the way (at most the depth), and
// then falls clear of the wall under the edge. A step or the edge of a
// floor up to DTU_KNEE_HIGH above the feet lifts the player as soon as
// the bottom reaches it (dtu_capsule_step for the first contact), with no
// separate step rule. A player who stands on an edge with no floor under
// the axis and does not walk toward the edge slides off it
// (dtu_capsule_slide). The objects (creatures, scenery) see the height
// of the capsule, from the feet to the head (fvi.c), and the radius of
// the move query of the original (rad): with R the player stops 1.0
// farther from a creature, and the melee test of docs/re/combat.md C5
// lost its hits at 60 frames per second.

static int dtu_capsule_on(void)
{
	return Dtu_improved_physics && Dtu_player_walk;
}

static void dtu_capsule_get(dtu_body *b,dtu_capsule *c)
{
	c->rad = b->move_rad + F1_0;
	c->stand = b->floor_height + b->floor_rad/2;
	c->head = b->half_height;
	c->depth = c->stand - c->rad - DTU_CAP_MARGIN;
	if (c->depth > c->rad)
		c->depth = c->rad;
	if (c->depth < 0)
		c->depth = 0;
}

#define DTU_CAP_MAX_SEGS	48

typedef struct cap_search {
	double px,pz;		// the axis
	double r,a;			// the radius and the depth of the bottom
	double lo;			// the lowest support (the feet - DTU_FLOOR_PROBE)
	double hi_face;		// the highest support from a face under the axis (the center)
	double hi_edge;		// the highest support from an edge (the feet + DTU_KNEE_HIGH)
	double best;
	int found,edge;
	double slope,dx,dz;
	double face_best;	// the highest face under the axis
	int face_found;
} cap_search;

static void cap_take(cap_search *s,double y,int edge,double slope,double dx,double dz)
{
	if (y < s->lo || y > (edge ? s->hi_edge : s->hi_face))
		return;
	if (!edge && (!s->face_found || y > s->face_best)) {
		s->face_found = 1;
		s->face_best = y;
	}
	if (s->found && y <= s->best)
		return;
	s->found = 1;
	s->best = y;
	s->edge = edge;
	s->slope = slope;
	s->dx = dx;
	s->dz = dz;
}

// The bottom over the edge a-b: the highest position of the feet at which
// the bottom touches it. The height of the bottom above the feet at the
// distance h from the axis is a - a*sqrt(1 - h^2/r^2), so the feet are at
// y(t) - that, a concave function of the place t on the edge.
static void cap_edge(cap_search *s,const double *a,const double *b)
{
	double dx = b[0]-a[0], dz = b[2]-a[2], ex = a[0]-s->px, ez = a[2]-s->pz;
	double qa = dx*dx + dz*dz, qb = 2*(ex*dx + ez*dz), qc = ex*ex + ez*ez, r2 = s->r*s->r;
	double t0 = 0, t1 = 1, t, h2, k, y, cx, cz, hl;
	int i;

	if (qa < 1e-9) {
		if (qc > r2)
			return;
		t0 = t1 = b[1] > a[1] ? 1 : 0;
	}
	else {
		double disc = qb*qb - 4*qa*(qc - r2), q;

		if (disc < 0)
			return;
		q = sqrt(disc);
		if ((-qb - q)/(2*qa) > t0)
			t0 = (-qb - q)/(2*qa);
		if ((-qb + q)/(2*qa) < t1)
			t1 = (-qb + q)/(2*qa);
		if (t0 > t1)
			return;
	}
	for (i=0; i<40 && t1 - t0 > 1e-7; i++) {
		double m1 = t0 + (t1 - t0)/3, m2 = t1 - (t1 - t0)/3, f1, f2;

		h2 = qa*m1*m1 + qb*m1 + qc;
		f1 = a[1] + m1*(b[1]-a[1]) + s->a*sqrt(h2 < r2 ? 1 - h2/r2 : 0);
		h2 = qa*m2*m2 + qb*m2 + qc;
		f2 = a[1] + m2*(b[1]-a[1]) + s->a*sqrt(h2 < r2 ? 1 - h2/r2 : 0);
		if (f1 < f2)
			t0 = m1;
		else
			t1 = m2;
	}
	t = (t0 + t1)/2;
	h2 = qa*t*t + qb*t + qc;
	if (h2 > r2)
		h2 = r2;
	k = sqrt(1 - h2/r2);
	y = a[1] + t*(b[1]-a[1]) - s->a + s->a*k;
	// the slope of the bottom there, and the direction to the axis
	cx = s->px - (a[0] + t*dx);
	cz = s->pz - (a[2] + t*dz);
	hl = sqrt(cx*cx + cz*cz);
	if (hl > 1e-6) {
		cx /= hl;
		cz /= hl;
	}
	else
		cx = cz = 0;
	cap_take(s,y,hl > 1e-3,k > 1e-6 ? s->a*sqrt(h2)/(r2*k) : 1e6,cx,cz);
}

static void cap_vert(double *d,int v)
{
	d[0] = f2fl(Vertices[v].x);
	d[1] = f2fl(Vertices[v].y);
	d[2] = f2fl(Vertices[v].z);
}

// A solid side of a segment: its faces that face up (the normal into the
// segment has y > 0.1) under the axis, and its edges in reach. The floor
// is side 3 in most segments, but a ramp or the floor of a turned segment
// can be another side (the floor search of the original, a line, finds
// them too: fvi.c mode 2 tests the other sides with the radius 0). The
// top edge of a wall is the edge of a step.
static void cap_side(cap_search *s,segment *seg,int sidenum,const double *center)
{
	static const int tri[3][2][3] = {
		{ {0,1,2}, {0,2,3} },		// SIDE_IS_QUAD, SIDE_IS_TRI_02
		{ {0,1,2}, {0,2,3} },
		{ {0,1,3}, {1,2,3} },		// SIDE_IS_TRI_13
	};
	side *sp = &seg->sides[sidenum];
	double v[4][3];
	int i,f,k;

	for (i=0; i<4; i++)
		cap_vert(v[i],seg->verts[Side_to_verts[sidenum][i]]);
	k = sp->type == SIDE_IS_TRI_13 ? 2 : 0;
	for (f=0; f<2; f++) {
		const double *p0 = v[tri[k][f][0]], *p1 = v[tri[k][f][1]], *p2 = v[tri[k][f][2]];
		double ux = p1[0]-p0[0], uy = p1[1]-p0[1], uz = p1[2]-p0[2];
		double wx = p2[0]-p0[0], wy = p2[1]-p0[1], wz = p2[2]-p0[2];
		double nx = uy*wz - uz*wy, ny = uz*wx - ux*wz, nz = ux*wy - uy*wx;
		double len = sqrt(nx*nx + ny*ny + nz*nz);

		if (len < 1e-9)
			continue;
		nx /= len; ny /= len; nz /= len;
		// the normal into the segment
		if (nx*(center[0]-p0[0]) + ny*(center[1]-p0[1]) + nz*(center[2]-p0[2]) < 0) {
			nx = -nx; ny = -ny; nz = -nz;
		}
		// the face under the axis: the height of its plane there
		if (ny > 0.1) {
			double d0 = (p1[0]-p0[0])*(s->pz-p0[2]) - (p1[2]-p0[2])*(s->px-p0[0]);
			double d1 = (p2[0]-p1[0])*(s->pz-p1[2]) - (p2[2]-p1[2])*(s->px-p1[0]);
			double d2 = (p0[0]-p2[0])*(s->pz-p2[2]) - (p0[2]-p2[2])*(s->px-p2[0]);

			if ((d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0))
				cap_take(s,p0[1] - (nx*(s->px-p0[0]) + nz*(s->pz-p0[2]))/ny,0,0,0,0);
		}
		cap_edge(s,p0,p1);
		cap_edge(s,p1,p2);
		cap_edge(s,p2,p0);
	}
}

// The highest support of the capsule with the axis at axis.x, axis.z,
// below feet + DTU_KNEE_HIGH for an edge (hi_edge) and below the center
// for a face under the axis, and not lower than the feet - DTU_FLOOR_PROBE.
// The solid sides of the segments that are open to segnum within reach.
// Returns 0 if there is none.
static int dtu_capsule_support(vms_vector *axis,int segnum,dtu_capsule *c,fix feet,fix hi_edge,dtu_cap_support *out)
{
	short list[DTU_CAP_MAX_SEGS];
	int n = 1,i,j,sd;
	cap_search s;

	s.px = f2fl(axis->x);
	s.pz = f2fl(axis->z);
	s.r = f2fl(c->rad);
	s.a = f2fl(c->depth);
	s.lo = f2fl(feet - DTU_FLOOR_PROBE);
	s.hi_face = f2fl(feet + c->stand);
	s.hi_edge = f2fl(hi_edge);
	s.found = 0;
	s.best = 0;
	s.face_found = 0;
	s.face_best = 0;

	list[0] = segnum;
	for (i=0; i<n; i++) {
		segment *seg = &Segments[list[i]];
		double center[3] = { 0, 0, 0 };

		for (j=0; j<MAX_VERTICES_PER_SEGMENT; j++) {
			double v[3];

			cap_vert(v,seg->verts[j]);
			center[0] += v[0]/MAX_VERTICES_PER_SEGMENT;
			center[1] += v[1]/MAX_VERTICES_PER_SEGMENT;
			center[2] += v[2]/MAX_VERTICES_PER_SEGMENT;
		}
		for (sd=0; sd<MAX_SIDES_PER_SEGMENT; sd++)
			if (!(WALL_IS_DOORWAY(seg,sd) & WID_FLY_FLAG))
				cap_side(&s,seg,sd,center);
		for (sd=0; sd<MAX_SIDES_PER_SEGMENT; sd++) {
			int ch = seg->children[sd];
			double mnx=1e30,mxx=-1e30,mny=1e30,mxy=-1e30,mnz=1e30,mxz=-1e30;

			if (ch < 0 || n >= DTU_CAP_MAX_SEGS || !(WALL_IS_DOORWAY(seg,sd) & WID_FLY_FLAG))
				continue;
			for (j=0; j<n && list[j] != ch; j++)
				;
			if (j < n)
				continue;
			for (j=0; j<4; j++) {
				double v[3];

				cap_vert(v,seg->verts[Side_to_verts[sd][j]]);
				if (v[0] < mnx) mnx = v[0];
				if (v[0] > mxx) mxx = v[0];
				if (v[1] < mny) mny = v[1];
				if (v[1] > mxy) mxy = v[1];
				if (v[2] < mnz) mnz = v[2];
				if (v[2] > mxz) mxz = v[2];
			}
			// the opening must be within reach of the column of the body
			if (s.px < mnx - s.r || s.px > mxx + s.r || s.pz < mnz - s.r || s.pz > mxz + s.r)
				continue;
			if (mxy < s.lo - 1 || mny > s.hi_face + 1)
				continue;
			list[n++] = ch;
		}
	}
	if (!s.found)
		return 0;
	out->y = fl2f(s.best);
	out->edge = s.edge;
	out->slope = s.slope > 1000 ? fl2f(1000) : fl2f(s.slope);
	out->dir.x = fl2f(s.dx);
	out->dir.y = 0;
	out->dir.z = fl2f(s.dz);
	out->gap = s.face_found && s.best - s.face_best < f2fl(DTU_FLOOR_PROBE) ? fl2f(s.best - s.face_best) : DTU_FLOOR_PROBE;
	return 1;
}

// The distance of the feet above the support of the capsule (as
// dtu_get_floor_dist(): negative in the floor, DTU_FLOOR_PROBE for no
// floor, 0 below DTU_FLOOR_EPS).
static fix dtu_capsule_floor_dist(object *obj)
{
	dtu_body b;
	dtu_capsule c;
	fix feet,d;
	int found;

	dtu_player_body(&b);
	dtu_capsule_get(&b,&c);
	feet = obj->pos.y - c.stand;
	found = dtu_capsule_support(&obj->pos,obj->segnum,&c,feet,feet + DTU_KNEE_HIGH,&Dtu_cap_sup);
	// The top of scenery holds the body too (dtusolid.c). An axis that is
	// outside of the top is on its edge: dtu_capsule_slide() takes the
	// player off it.
	Dtu_stand_obj = -1;
	if (dtu_solid_on()) {
		vms_vector dir;
		fix top,dist;
		int n = dtu_solid_support(&obj->pos,b.move_rad,feet - DTU_FLOOR_PROBE,feet + DTU_SOLID_STEP,&top,&dir,&dist);

		if (n >= 0 && (!found || top > Dtu_cap_sup.y)) {
			fix gap = found ? top - Dtu_cap_sup.y : DTU_FLOOR_PROBE;

			Dtu_cap_sup.y = top;
			Dtu_cap_sup.edge = dist > DTU_SOLID_RIM;
			Dtu_cap_sup.slope = F1_0;
			Dtu_cap_sup.gap = gap < DTU_FLOOR_PROBE ? gap : DTU_FLOOR_PROBE;
			Dtu_cap_sup.dir = dir;
			Dtu_stand_obj = n;
			found = 1;
		}
	}
	if (!found) {
		Dtu_cap_sup.edge = 0;
		return DTU_FLOOR_PROBE;
	}
	d = feet - Dtu_cap_sup.y;
	if (abs(d) < DTU_FLOOR_EPS)
		d = 0;
	return d;
}

// The player goes into the segment seg after a lift or a step down. The
// two segments go to the list of the move, for the triggers (object.c).
static void dtu_relink(object *obj,int seg)
{
	if (seg == obj->segnum)
		return;
	if ((n_phys_segs == 0 || phys_seglist[n_phys_segs-1] != obj->segnum) && n_phys_segs < MAX_FVI_SEGS-1)
		phys_seglist[n_phys_segs++] = obj->segnum;
	if (n_phys_segs < MAX_FVI_SEGS-1)
		phys_seglist[n_phys_segs++] = seg;
	obj_relink(obj-Objects,seg);
}

// The move query of the capsule. The query of the original body (the
// sphere at the center, fq) and the head sphere; the earliest hit counts.
// A hit of the head sphere counts only when the move goes into the side
// (a start in its reach does not hold the player).
static int dtu_capsule_query(object *obj,dtu_body *body,dtu_capsule *c,fvi_query *fq,fvi_info *hit)
{
	fvi_query hq;
	fvi_info hh;
	vms_vector h0,h1,move;
	fix dy = c->head - c->rad;
	int fate,hfate,seg;

	Fvi_dtu_capsule = 1;
	Fvi_dtu_cap_down = c->stand;
	Fvi_dtu_cap_up = c->head;
	Fvi_dtu_body = 1;
	Fvi_dtu_rad2 = body->move_rad2;
	fate = find_vector_intersection(fq,hit);
	Fvi_dtu_body = 0;
	Fvi_dtu_capsule = 0;

	if (fate == HIT_BAD_P0 || dy < DTU_CAP_HEAD_MIN)
		return fate;

	h0 = *fq->p0;
	h0.y += dy;
	h1 = *fq->p1;
	h1.y += dy;
	if ((seg = find_point_seg(&h0,fq->startseg)) < 0)
		return fate;

	hq.p0 = &h0;
	hq.p1 = &h1;
	hq.startseg = seg;
	hq.rad = fq->rad;
	hq.thisobjnum = fq->thisobjnum;
	hq.ignore_obj_list = NULL;
	hq.flags = 0;
	Fvi_dtu_body = 3;
	hfate = find_vector_intersection(&hq,&hh);
	Fvi_dtu_body = 0;

	if (hfate != HIT_WALL || hh.hit_side_seg < 0 || hh.hit_side < 0)
		return fate;
	vm_vec_sub(&move,fq->p1,fq->p0);
	if (vm_vec_dot(&move,&hh.hit_wallnorm) >= 0)
		return fate;
	if (fate != HIT_NONE && vm_vec_dist(&hit->hit_pnt,fq->p0) <= vm_vec_dist(&hh.hit_pnt,&h0))
		return fate;

	hit->hit_type = HIT_WALL;
	hit->hit_pnt = hh.hit_pnt;
	hit->hit_pnt.y -= dy;
	seg = find_point_seg(&hit->hit_pnt,fq->startseg);
	hit->hit_seg = seg >= 0 ? seg : fq->startseg;
	hit->hit_side = hh.hit_side;
	hit->hit_side_seg = hh.hit_side_seg;
	hit->hit_object = -1;
	hit->hit_wallnorm = hh.hit_wallnorm;
	return HIT_WALL;
}

// The first contact of the round bottom with a step or the edge of a
// floor: the sphere of the center hits the side under the edge before
// the bottom stands on the edge (the test of fvi is a slab). When the
// support at the end of the query is above the feet by DTU_KNEE_HIGH or
// less, the player is lifted onto it and the move is tried again. It
// replaces the step rule of the body of the original (a lift onto the top
// of the wall side that was hit). Returns 1 if the player was lifted.
static int dtu_capsule_step(object *obj,dtu_body *body,dtu_capsule *c,fvi_info *hit,vms_vector *p1)
{
	dtu_cap_support s;
	vms_vector axis,new_pos,head;
	fix feet;
	int seg;

	if (Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_CLIMBING|DTU_PS_FALLING))
		return 0;
	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))
		return 0;
	if (!Dtu_cap_grounded || Dtu_floor_dist != 0)
		return 0;
	if (hit->hit_side_seg < 0 || hit->hit_side < 0 || hit->hit_side == WTOP)
		return 0;
	if (vm_vec_dot(&obj->mtype.phys_info.velocity,&hit->hit_wallnorm) >= 0)
		return 0;

	feet = obj->pos.y - c->stand;
	axis = *p1;
	axis.y = obj->pos.y;
	// The search starts in the segment of the player: the end of the query
	// can be behind the wall, in a segment of another room.
	if (!dtu_capsule_support(&axis,obj->segnum,c,feet,feet + DTU_KNEE_HIGH,&s))
		return 0;
	// A smaller lift does not take the sphere of the center over the edge
	// of a side (that needs at least DTU_CAP_MARGIN), the floor snap of
	// dtu_apply_gravity() does it.
	if (s.y <= feet + DTU_FLOOR_EPS)
		return 0;

	new_pos = obj->pos;
	new_pos.y = s.y + c->stand + F1_0/64;
	head = new_pos;
	head.y += body->half_height;
	if ((seg = find_point_seg(&new_pos,obj->segnum)) < 0 || find_point_seg(&head,seg) < 0)
		return 0;
	{
		char text[160];

		snprintf(text,sizeof(text),"CAPSTEP side %d.%d lift %.4f pos %.4f %.4f %.4f\n",hit->hit_side_seg,hit->hit_side,
			f2fl(new_pos.y - obj->pos.y),f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z));
		dtu_move_log_line(text);
	}
	obj->pos = new_pos;
	dtu_relink(obj,seg);
	return 1;
}

// The bottom stands on an edge with no floor under the axis (none, or one
// more than DTU_CAP_SLIDE_GAP lower), the axis is clearly past the edge
// (the bottom is steeper than DTU_CAP_SLIDE_SLOPE there), and the keys do
// not move the player toward the edge: the player slides off it at
// DTU_CAP_SLIDE_SPEED and does not stand in the air beside a ledge. A
// floor that slopes keeps the floor under the axis near the edge, so the
// player does not slide there.
static void dtu_capsule_slide(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector dir;
	fix v;

	if (!Dtu_cap_grounded || !Dtu_cap_sup.edge || Dtu_cap_sup.slope <= DTU_CAP_SLIDE_SLOPE || Dtu_cap_sup.gap <= DTU_CAP_SLIDE_GAP)
		return;
	if (Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_CLIMBING|DTU_PS_FALLING))
		return;
	if (dtu_thrust_dir(obj,&dir) && fixmul(dir.x,Dtu_cap_sup.dir.x) + fixmul(dir.z,Dtu_cap_sup.dir.z) < 0)
		return;
	v = fixmul(pi->velocity.x,Dtu_cap_sup.dir.x) + fixmul(pi->velocity.z,Dtu_cap_sup.dir.z);
	if (v < DTU_CAP_SLIDE_SPEED) {
		pi->velocity.x += fixmul(Dtu_cap_sup.dir.x,DTU_CAP_SLIDE_SPEED - v);
		pi->velocity.z += fixmul(Dtu_cap_sup.dir.z,DTU_CAP_SLIDE_SPEED - v);
	}
}

// ASCENT: Dtu_fix_wall_overlap. The move query of the body has another
// radius for each side (fvi.c, dtu_side_rad: the floor rad/2, the walls
// rad + 1.0). A segment below the player is entered through its top side
// with the small radius, so the body can already be inside the reach of a
// wall of that segment when it comes into the query: the player who walks
// off the edge of a ledge falls with the center less than rad + 1.0 from
// the wall under the edge. From such a start each move hits the wall (the
// hit point is behind the start), and the player cannot move any more
// (the saved game "stuck": HUB1, segment 179, the wall of segment 70 side
// 0, 0.44 from the center). This moves the center out of the reach of the
// wall along its normal, when the start is in front of the wall and the
// hit is not ahead of the start. Returns 1 if the player was moved.
static int dtu_push_out_of_wall(object *obj,dtu_body *body,fvi_info *hit,vms_vector *move,vms_vector *start_pos)
{
	segment *segp;
	vms_vector d, np;
	fvi_query fq;
	fvi_info h2;
	fix dist, reach;
	int k, seg;

	if (hit->hit_side_seg < 0 || hit->hit_side < 0 || hit->hit_side == WBOTTOM)
		return 0;
	vm_vec_sub(&d,&hit->hit_pnt,&obj->pos);
	if (vm_vec_dot(&d,move) > 0)
		return 0;						// an ordinary hit, ahead of the start

	reach = (hit->hit_side == WTOP) ? fixmul(body->move_rad2,0x18000) : body->move_rad + F1_0;
	segp = &Segments[hit->hit_side_seg];
	dist = 0x7fffffff;
	for (k=0;k<4;k++) {
		fix dk;

		vm_vec_sub(&d,&obj->pos,&Vertices[segp->verts[Side_to_verts[hit->hit_side][k]]]);
		dk = vm_vec_dot(&d,&hit->hit_wallnorm);
		if (dk < dist)
			dist = dk;
	}
	if (dist < 0 || dist > reach - F1_0/64)
		return 0;						// behind the wall, or not inside its reach

	vm_vec_scale_add(&np,&obj->pos,&hit->hit_wallnorm,reach - dist + F1_0/32);

	fq.p0 = &obj->pos;
	fq.p1 = &np;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&h2) != HIT_NONE)
		return 0;
	seg = find_point_seg(&np,obj->segnum);
	if (seg < 0)
		return 0;

	{
		char text[200];

		snprintf(text,sizeof(text),"WALLOVERLAP seg %d side %d dist %.4f reach %.4f pos %.4f %.4f %.4f to %.4f %.4f %.4f\n",
			hit->hit_side_seg,hit->hit_side,f2fl(dist),f2fl(reach),f2fl(obj->pos.x),f2fl(obj->pos.y),f2fl(obj->pos.z),
			f2fl(np.x),f2fl(np.y),f2fl(np.z));
		dtu_move_log_line(text);
	}
	obj->pos = np;
	if (seg != obj->segnum)
		obj_relink(obj-Objects,seg);
	*start_pos = np;
	return 1;
}

// ASCENT: Dtu_improved_physics. The push of scenery (dtusolid.c,
// dtuobj_scenery.c). The player who stands on a floor and walks against a
// side of scenery for DTU_PUSH_DELAY moves it, when the strength is above
// "pushable" (the test of 0x9E5BE). The scenery moves away from the side
// that the player touches, with DTU_PUSH_SPEED at most (one half of the
// walk speed limit: the mean speed of the push cycle of the executable,
// docs/re/objects.md) and slower when "pushable" is near to the strength.
// The player keeps the speed of the scenery. There is no push in a jump or
// a fall, so a jump at a crate does not move it.
#define DTU_PUSH_DELAY		0x4ccc		// 0.3 s
#define DTU_PUSH_IDLE		0x1999		// 0.1 s with no contact ends the push
#define DTU_PUSH_SPEED		0x180000	// 24.0
#define DTU_PUSH_RAMP		0x4000		// 0.25 s, the push speed grows from 0 in this time
#define DTU_PUSH_REACH		0x2000		// 0.125, the player leans on a side that is this near
#define DTU_SOLID_OUT_SPEED	0x100000	// 16.0, the speed out of scenery that the body is inside of

static int Dtu_push_obj = -1;
static fix Dtu_push_time = 0,Dtu_push_idle = 0;

// One time in each frame, before the move: the player stands on a floor
// and the keys move the player in the direction dir. A side of scenery
// that is DTU_PUSH_REACH or less ahead is the side that the player leans
// on. (The test does not wait for a hit of the move: at a high frame rate
// most frames have no hit, because the velocity into the side is 0 after a
// hit and grows again only with a thrust step.) After DTU_PUSH_DELAY the
// scenery moves, and the player gets its speed into the side. Returns 1
// while the player leans on scenery that the player can push.
static int dtu_solid_lean(object *obj,dtu_body *body,dtu_capsule *c,vms_vector *dir,fix sim_time)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector probe,n,delta;
	fix t,speed,f;
	int kind,objnum,strength,pushable;

	if (!Dtu_cap_grounded || (Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_FALLING|DTU_PS_CLIMBING)))
		return 0;
	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))
		return 0;
	vm_vec_copy_scale(&probe,dir,DTU_PUSH_REACH);
	objnum = dtu_solid_sweep(&obj->pos,&probe,body->move_rad,c->stand,c->head,&t,&n,&kind);
	if (objnum < 0 || kind != DTU_SOLID_SIDE || objnum == Dtu_stand_obj)
		return 0;
	strength = dtu_player_push_strength();
	pushable = dressing_pushable(&Objects[objnum]);
	// the keys must move the player against the side
	if (vm_vec_dot(dir,&n) > -F1_0/2 || strength <= pushable)
		return 0;

	if (Dtu_push_obj != objnum) {
		Dtu_push_obj = objnum;
		Dtu_push_time = 0;
	}
	if (Dtu_push_time <= DTU_PUSH_DELAY || sim_time <= 0)
		return 1;

	// The speed grows to its value in DTU_PUSH_RAMP. It is in proportion
	// to the part of the key direction that goes into the side.
	speed = fixmul(DTU_PUSH_SPEED,F1_0 - fixmuldiv(0x9999,i2f(pushable),i2f(strength)));
	if (Dtu_push_time - DTU_PUSH_DELAY < DTU_PUSH_RAMP)
		speed = fixmuldiv(speed,Dtu_push_time - DTU_PUSH_DELAY,DTU_PUSH_RAMP);
	speed = fixmul(speed,-vm_vec_dot(dir,&n));
	vm_vec_copy_scale(&delta,&n,-fixmul(speed,sim_time));
	f = dtu_dressing_push(&Objects[objnum],&delta,strength);
	// the velocity of the player into the side is that of the scenery
	vm_vec_scale_add2(&pi->velocity,&n,-vm_vec_dot(&pi->velocity,&n) - fixmul(speed,f));
	return 1;
}

// The body is inside of scenery: it moves out of it, DTU_SOLID_OUT_SPEED
// at most, when the way is free.
static void dtu_solid_move_out(object *obj,dtu_body *body,dtu_capsule *c,fix sim_time)
{
	vms_vector n,np;
	fvi_query fq;
	fvi_info hit;
	fix depth,out = fixmul(DTU_SOLID_OUT_SPEED,sim_time);
	int seg;

	if (dtu_solid_overlap(&obj->pos,body->move_rad,c->stand,c->head,&n,&depth) < 0)
		return;
	if (out > depth)
		out = depth;
	vm_vec_scale_add(&np,&obj->pos,&n,out);
	fq.p0 = &obj->pos;
	fq.p1 = &np;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) != HIT_NONE || (seg = find_point_seg(&np,obj->segnum)) < 0)
		return;
	obj->pos = np;
	dtu_relink(obj,seg);
}

// 0x14534B, 0x14534F: the object that stopped the player in the last
// tries, and the number of its hits in a row.
static int Dtu_block_obj = -1;
static int Dtu_block_hits = 0;

int dtu_player_physics_sim(object *obj)
{
	static fix levitate_acc = 0;
	physics_info *pi = &obj->mtype.phys_info;
	int objnum = obj-Objects;
	fix sim_time = FrameTime;
	vms_vector accel,start_pos,frame_vec,new_pos,p1;
	int floor_type,tries = 0,fate,hit_an_object = 0;
	int lava_seg = -1,lava_side = -1;	// the wall or floor of the last move query (dtu_lava_frame)
	int ignore_obj_list[MAX_IGNORE_OBJS],n_ignore_objs = 0;
	fvi_query fq;
	fvi_info hit;
	dtu_body body;		// ASCENT: the body of the frame (0x748D9)
	dtu_capsule cap;	// ASCENT: Dtu_improved_physics, the capsule
	int capsule;
	// ASCENT: Dtu_improved_physics, scenery as solid bodies (dtusolid.c)
	int solids,have_push_dir,push_seen = 0;
	vms_vector push_dir;
	fix move_time;

	if (obj->dtu.m_88 & DTU_OF_HELD)
		return 1;

	dtu_player_body(&body);
	dtu_capsule_get(&body,&cap);
	capsule = dtu_capsule_on();
	solids = capsule && dtu_solid_on();
	have_push_dir = dtu_thrust_dir(obj,&push_dir);

	n_phys_segs = 0;
	floor_type = dtu_floor_type(obj);

	dtu_physics_sim_rot(obj);

	// pcons.c 0x9EBEB(obj, 10.0): the climb (dtuthief.c). It changes the
	// thrust and adds a push away from the wall.
	{
		vms_vector climb_accel = { 0, 0, 0 };

		// ASCENT: Dtu_fix_ladder holds the top of a ladder
		if (Dtu_player_state & DTU_PS_CLIMBING) {
			if (!(Dtu_fix_ladder && Dtu_player_walk && !(Dtu_thief_bits & DTU_TS_CLIMB_SKILL) &&
				!(Dtu_player_state & DTU_PS_JUMPING) && dtu_improved_ladder(obj)))
				dtu_thief_climb_frame(obj,&climb_accel);
		}
		else
			Dtu_ladder_seg = -1;
		vm_vec_copy_scale(&accel,&pi->thrust,fixdiv(F1_0,pi->mass));
		vm_vec_add2(&accel,&climb_accel);
	}

	if (!(obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))) {
		if (!(obj->dtu.m_88 & DTU_OF_1000)) {
			if (floor_type == 1 || (obj->dtu.m_88 & DTU_OF_SLOW)) {
				accel.x >>= 1;
				accel.z >>= 1;
			}
			// 0x9DDE3: one third while the player sneaks or searches (the
			// bits 0x1C of P+0xA0, dtuthief.c)
			if (Dtu_thief_bits & (DTU_TS_SILENT|DTU_TS_SILENT_FAILED|DTU_TS_SEARCHING)) {
				accel.x /= 3;
				accel.z /= 3;
			}
		}
		if (obj->dtu.m_88 & DTU_OF_FAST) {
			accel.x *= 2;
			accel.z *= 2;
		}
	}

	dtu_apply_gravity(obj,&accel);

	// ASCENT: Dtu_fix_ladder, and the time in the air for Dtu_improved_jump
	if (Dtu_fix_ladder && Dtu_player_walk)
		dtu_improved_ladder_grab(obj,&accel);
	if (Dtu_floor_dist <= DTU_JUMP_FLOOR_MAX || (Dtu_player_state & DTU_PS_CLIMBING))
		Dtu_air_time = 0;
	else if (Dtu_air_time < F1_0)
		Dtu_air_time += sim_time;

	// 0x6CCDB: the player moves up and down in flight and in levitation.
	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE)) {
		fix s;

		fix_sincos(Dtu_fly_bob_phase,&s,NULL);
		accel.y += s/5;
		if (Dtu_fix_fly_bob)
			Dtu_fly_bob_phase += fixmuldiv(DTU_FLY_BOB_STEP,sim_time,DTU_REF_FRAMETIME);
		else
			Dtu_fly_bob_phase += DTU_FLY_BOB_STEP;
	}

	Dtu_bob_mode = Dtu_move_mode;
	Dtu_jump_set_vel = 0;

	if ((Dtu_player_state & DTU_PS_JUMPING) && Dtu_fix_jump_path)
		dtu_jump_frame_ballistic(obj,sim_time);
	else if (Dtu_player_state & DTU_PS_JUMPING)
		dtu_jump_frame(obj,sim_time);
	else
		dtu_integrate(obj,&accel,sim_time);

	// ASCENT: Dtu_fix_ladder. On a ladder: no velocity into the wall (the
	// executable moves the player into the wall and the wall throws the
	// player back on each frame). The step from the top of a ladder:
	// forward, with no gravity, until the floor is under the feet.
	// Dtu_improved_physics: on the floor, no slide.
	if (Dtu_player_walk && !(obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE))) {
		if (!Dtu_fix_ladder)
			Dtu_ladder_exit = 0;
		if (Dtu_ladder_exit > 0) {
			Dtu_ladder_exit -= sim_time;
			if (Dtu_floor_dist < DTU_FLOOR_PROBE && Dtu_floor_dist <= F1_0)
				Dtu_ladder_exit = 0;
			else {
				vm_vec_copy_scale(&pi->velocity,&Dtu_ladder_exit_dir,DTU_FIX_LADDER_EXIT_SPEED);
				Dtu_player_state &= ~DTU_PS_FALLING;
			}
		}
		else if ((Dtu_player_state & DTU_PS_CLIMBING) && Dtu_fix_ladder) {
			vms_vector h = obj->orient.fvec;
			fix d;

			h.y = 0;
			vm_vec_normalize(&h);
			d = vm_vec_dot(&pi->velocity,&h);
			vm_vec_scale_add2(&pi->velocity,&h,-d);
		}
		else if (Dtu_improved_physics && !(Dtu_player_state & (DTU_PS_CLIMBING|DTU_PS_JUMPING|DTU_PS_FALLING)) && Dtu_floor_dist <= F1_0) {
			dtu_improved_walk(obj,sim_time);
			if (capsule)
				dtu_capsule_slide(obj);
		}
	}

	vm_vec_zero(&pi->thrust);

	if (obj->dtu.m_88 & DTU_OF_LEVITATE) {
		int n = dtu_ref_ticks(Dtu_fix_stop_decay,&levitate_acc);

		if (!(Dtu_player_state & DTU_PS_CLIMBING))
			pi->velocity.x = pi->velocity.z = 0;
		while (n--)
			pi->velocity.y >>= 2;
	}

	if ((obj->dtu.m_88 & DTU_OF_80) && pi->velocity.y < -DTU_SLOW_FALL_VEL)
		pi->velocity.y = -DTU_SLOW_FALL_VEL;

	// The view goes up and down one time for each second of a walk and two
	// times for each second of a run (0x76F30).
	Dtu_bob_phase += sim_time * Dtu_bob_mode;

	start_pos = obj->pos;
	if (Dtu_wall_time < F1_0)
		Dtu_wall_time += sim_time;

	// ASCENT: Dtu_improved_physics. A body that is inside of scenery (a
	// saved game, scenery that a creature pushed) moves out of it.
	if (solids) {
		dtu_solid_move_out(obj,&body,&cap,sim_time);
		push_seen = have_push_dir && dtu_solid_lean(obj,&body,&cap,&push_dir,sim_time);
	}
	start_pos = obj->pos;
	move_time = sim_time;

	// The move. A hit puts the player back to the start of the frame and
	// changes the velocity. Then the move starts again, 8 times at most.
	for (;;) {

		if (vm_vec_mag(&pi->velocity) <= 0x72 || move_time <= 0)
			break;

		frame_vec.x = fixmul(pi->velocity.x,move_time);
		frame_vec.y = fixmul(pi->velocity.y,move_time);
		frame_vec.z = fixmul(pi->velocity.z,move_time);

		if (++tries > 8) {
			mprintf((0,"Stupid human. "));
			obj->pos = start_pos;
			break;
		}

		vm_vec_add(&new_pos,&obj->pos,&frame_vec);

		// The query goes 1.2 times as far in x and z as the move.
		p1.x = obj->pos.x + fixmul(frame_vec.x,0x13333);
		p1.y = obj->pos.y + frame_vec.y;
		p1.z = obj->pos.z + fixmul(frame_vec.z,0x13333);

		ignore_obj_list[n_ignore_objs] = -1;

		// 0x9E11A: after more than 30 hits of the same object in a row the
		// object is ignored when the move goes away from it, so that a
		// body inside of it can get out.
		if (Dtu_block_obj >= 0 && Dtu_block_hits > 30 && Dtu_block_obj <= Highest_object_index &&
				vm_vec_dist(&Objects[Dtu_block_obj].pos,&new_pos) > vm_vec_dist(&Objects[Dtu_block_obj].pos,&start_pos) &&
				n_ignore_objs < MAX_IGNORE_OBJS-1) {
			ignore_obj_list[n_ignore_objs++] = Dtu_block_obj;
			ignore_obj_list[n_ignore_objs] = -1;
		}

		fq.p0 = &obj->pos;
		fq.p1 = &p1;
		fq.startseg = obj->segnum;
		fq.rad = body.move_rad;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = ignore_obj_list;
		fq.flags = FQ_CHECK_OBJS | FQ_GET_SEGLIST;

		if (capsule)
			fate = dtu_capsule_query(obj,&body,&cap,&fq,&hit);
		else {
			Fvi_dtu_body = 1;
			Fvi_dtu_rad2 = body.move_rad2;
			fate = find_vector_intersection(&fq,&hit);
			Fvi_dtu_body = 0;
		}

		// 0x9E2BA: the hit side of the query goes to 0x30F494 (the side
		// that the lava test 0x3D2BD reads)
		Dtu_lava_hit_side = hit.hit_side;
		lava_seg = (fate == HIT_WALL) ? hit.hit_side_seg : -1;
		lava_side = hit.hit_side;

		// 0x3FA08: a hit of the bottom side of a segment is a hit of the floor.
		if (fate == HIT_WALL && hit.hit_side == WBOTTOM)
			fate = DTU_HIT_FLOOR;

		{
			int i = 0;

			if (n_phys_segs && hit.n_segs && phys_seglist[n_phys_segs-1]==hit.seglist[0])
				n_phys_segs--;
			while (i<hit.n_segs && n_phys_segs<MAX_FVI_SEGS-1)
				phys_seglist[n_phys_segs++] = hit.seglist[i++];
		}

		// ASCENT: Dtu_improved_physics. The scenery, when the move reaches
		// it before the hit of the query. The player moves up to the
		// scenery, and the rest of the frame goes on with the velocity
		// that is left.
		if (solids && fate != HIT_BAD_P0) {
			vms_vector sn;
			fix st;
			int skind;
			int sobj = dtu_solid_sweep(&obj->pos,&frame_vec,body.move_rad,cap.stand,cap.head,&st,&sn,&skind);

			if (sobj >= 0 && (fate == HIT_NONE || fixmul(st,vm_vec_mag(&frame_vec)) <= vm_vec_dist(&hit.hit_pnt,&obj->pos))) {
				vms_vector np;
				int seg;

				{
					char text[160];

					snprintf(text,sizeof(text),"SOLID obj %d %s at %.4f of the move, away %.3f %.3f %.3f\n",sobj,
						skind == DTU_SOLID_TOP ? "top" : skind == DTU_SOLID_BOTTOM ? "bottom" : "side",f2fl(st),f2fl(sn.x),f2fl(sn.y),f2fl(sn.z));
					dtu_move_log_line(text);
				}
				vm_vec_scale_add(&np,&obj->pos,&frame_vec,st);
				if (skind == DTU_SOLID_SIDE)
					vm_vec_scale_add2(&np,&sn,DTU_SOLID_SKIN);
				if ((seg = find_point_seg(&np,obj->segnum)) >= 0) {
					obj->pos = np;
					dtu_relink(obj,seg);
					start_pos = np;
				}
				move_time = fixmul(move_time,F1_0 - st);

				if (skind == DTU_SOLID_TOP) {
					// the feet come down on the top: the end of a jump or of
					// a fall, as on a floor (0x6CD45)
					if (Dtu_player_state & DTU_PS_JUMPING) {
						Dtu_jump_time = 0;
						Dtu_jump_vy = 0;
					}
					Dtu_player_state &= ~(DTU_PS_JUMPING|DTU_PS_CLIMBING);
					if (pi->velocity.y < 0)
						pi->velocity.y = 0;
					dtu_fall_damage(obj);
				}
				else if (skind == DTU_SOLID_BOTTOM) {
					if (pi->velocity.y > 0)
						pi->velocity.y = 0;
					if ((Dtu_player_state & DTU_PS_JUMPING) && Dtu_jump_vy > 0)
						Dtu_jump_vy = 0;
				}
				else {
					fix vn = -vm_vec_dot(&pi->velocity,&sn);

					if (vn > 0)
						vm_vec_scale_add2(&pi->velocity,&sn,vn);
					else
						vm_vec_scale_add2(&pi->velocity,&sn,F1_0);
					if ((Dtu_player_state & DTU_PS_JUMPING) && Dtu_fix_jump_path) {
						fix d = vm_vec_dot(&Dtu_jump_hvel,&sn);

						if (d < 0)
							vm_vec_scale_add2(&Dtu_jump_hvel,&sn,-d);
					}
				}
				if (obj->flags & OF_SHOULD_BE_DEAD)
					break;
				continue;
			}
		}

		if (fate == HIT_NONE) {
			int segnum = find_point_seg(&new_pos,obj->segnum);

			if (segnum != -1) {
				obj->pos = new_pos;
				if (segnum != obj->segnum)
					obj_relink(objnum,segnum);
			}
			break;
		}
		else if (fate == DTU_HIT_FLOOR && (Dtu_player_state & DTU_PS_JUMPING) && Dtu_improved_jump && Dtu_jump_vy > 0) {
			// ASCENT: Dtu_improved_jump. The body touches the edge of a higher
			// floor on the way up: no move forward in this frame, the jump
			// goes on and passes over the edge when it is high enough.
			obj->pos = start_pos;
			pi->velocity.x = pi->velocity.z = 0;
		}
		else if (fate == DTU_HIT_FLOOR) {			// 0x6CD45

			obj->pos = start_pos;
			Dtu_player_state &= ~(DTU_PS_JUMPING|DTU_PS_CLIMBING);

			if (!dtu_knee_high(obj,hit.hit_side_seg,hit.hit_side)) {
				fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

				vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
			}

			dtu_fall_damage(obj);
		}
		else if (fate == HIT_WALL && Dtu_fix_wall_overlap && dtu_push_out_of_wall(obj,&body,&hit,&frame_vec,&start_pos)) {
			// ASCENT: Dtu_fix_wall_overlap. The move starts again from the
			// position out of the reach of the wall.
			continue;
		}
		else if (fate == HIT_WALL && capsule && dtu_capsule_step(obj,&body,&cap,&hit,&p1)) {
			// ASCENT: Dtu_improved_physics, the bottom of the capsule on a
			// step. The move starts again from the new height.
			continue;
		}
		else if (fate == HIT_WALL) {					// 0x9E47C and 0x9E88D
			vms_vector moved;
			fix wall_part,d;
			int ballistic = (Dtu_player_state & DTU_PS_JUMPING) && Dtu_fix_jump_path;
			// Dtu_improved_jump, but not when the body touches the edge of the
			// side (no velocity into it): then the rules of the base.
			int improved_jump = ballistic && Dtu_improved_jump &&
				vm_vec_dot(&pi->velocity,&hit.hit_wallnorm) < -DTU_WALL_UNSTICK_SPEED/8;

			if (improved_jump) {
				// ASCENT: Dtu_improved_jump. A wall does not end the jump: the
				// part of the velocity into the wall goes away. A ceiling
				// turns the jump down.
				if (hit.hit_side == WTOP && Dtu_jump_vy > 0)
					Dtu_jump_vy = 0;
				d = vm_vec_dot(&Dtu_jump_hvel,&hit.hit_wallnorm);
				if (d < 0) {
					vms_vector nh = hit.hit_wallnorm;

					nh.y = 0;
					d = vm_vec_dot(&Dtu_jump_hvel,&nh);
					vm_vec_scale_add2(&Dtu_jump_hvel,&nh,-d);
				}
				if (hit.hit_side == WTOP && pi->velocity.y > 0)
					pi->velocity.y = 0;
			}
			else if (ballistic) {
				// ASCENT: Dtu_fix_jump_path, the rules of the executable for
				// the jump with a constant gravity. A ceiling ends the jump. A
				// wall ends it on the way down (0x9F093). On the way up the
				// move of this frame stops: the executable sets the velocity
				// to 0 and puts the player back to the height of the table in
				// the next step, so the height does not change.
				if (hit.hit_side == WTOP) {
					Dtu_player_state &= ~DTU_PS_JUMPING;
					Dtu_jump_time = 0;
					Dtu_jump_vy = 0;
					pi->velocity.y = 0;
				}
				else if (pi->velocity.y < 0) {
					dtu_stop_jump(obj);
					Dtu_jump_time = 0;
				}
				else
					pi->velocity.x = pi->velocity.z = 0;
			}
			else {
				if (Dtu_player_state & DTU_PS_JUMPING) {
					dtu_stop_jump(obj);
					vm_vec_zero(&pi->velocity);
				}

				if (hit.hit_side == WTOP) {
					Dtu_player_state &= ~DTU_PS_JUMPING;
					pi->velocity.y = 0;
				}
				else
					dtu_stop_jump(obj);
			}

			vm_vec_sub(&moved,&new_pos,&start_pos);
			wall_part = vm_vec_dot(&moved,&hit.hit_wallnorm);

			if (hit.hit_side_seg >= 0 && hit.hit_side >= 0)
				collide_object_with_wall(obj,abs(wall_part),hit.hit_side_seg,hit.hit_side,&hit.hit_pnt);

			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;

			// The velocity into the wall turns back with 0.75 of its size.
			// ASCENT: Dtu_fix_wall_slide. The executable throws the player
			// back on each frame of a walk into a wall or along it, and the
			// player loses the move of that frame: the position goes back
			// and forth by 0.6 to 0.7 and the size of the steps changes with
			// the frame rate. The corrected rule takes away the velocity
			// into the wall and keeps the velocity along it.
			d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);
			if (Dtu_fix_wall_slide && Dtu_player_walk) {
				if (d < -DTU_WALL_UNSTICK_SPEED/8)
					vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,fixmul(-d,DTU_WALL_SLIDE));
				else {
					// The body touches the edge of the side, not the side: the
					// velocity is along the side, and the move would hit the
					// edge again on each try. The player moves out from it.
					vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,DTU_WALL_UNSTICK_SPEED - d);
					if ((Dtu_player_state & DTU_PS_JUMPING) && Dtu_fix_jump_path)
						vm_vec_scale_add2(&Dtu_jump_hvel,&hit.hit_wallnorm,DTU_WALL_UNSTICK_SPEED);
				}
			}
			else
				vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,fixmul(-d,DTU_WALL_BOUNCE));

			obj->pos = start_pos;
			Dtu_bob_mode = 0;
			Dtu_wall_time = 0;
		}
		else if (fate == HIT_OBJECT) {				// 0x9E53D
			object *other = &Objects[hit.hit_object];

			obj->pos = start_pos;
			hit_an_object = 1;

			// 0x9E54B: the hits of the same object in a row (0x14534B,
			// 0x14534F). The executable counts a hit only when rad2 of the
			// body is not below hit_pnt.y (a compare of a size with a
			// coordinate); the port counts each hit.
			if (hit.hit_object != Dtu_block_obj) {
				Dtu_block_obj = hit.hit_object;
				Dtu_block_hits = 0;
			}
			Dtu_block_hits++;

			collide_two_objects(obj,other,&hit.hit_pnt);

			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;

			// A monster stops the player. The player goes through all
			// other objects after the collision (an item that is taken).
			if (other->type == OBJ_ROBOT || other->type == OBJ_PLAYER || other->type == OBJ_CNTRLCEN)
				break;
			// ASCENT: scenery stops the player too (0x9E5A1: the player
			// goes back to the start of the frame and the move is tried
			// again, the object is not ignored). The push of 0x9E5BE has
			// moved it. Before this the player went through the scenery
			// and pushed it from the inside in each frame.
			if (other->type == OBJ_DRESSING && Dtu_objects_enabled)
				continue;
			if (n_ignore_objs < MAX_IGNORE_OBJS-1)
				ignore_obj_list[n_ignore_objs++] = hit.hit_object;
		}
		else													// HIT_BAD_P0 "bad p0 "
			break;
	}

	// The velocity of a step of the jump is the way of the step divided by
	// the frame time. The executable keeps this velocity when the jump
	// ends in this frame: the player is fast after a jump at a high frame
	// rate. The corrected rule keeps the velocity of the reference frame.
	// 0x9E797: a frame with no hit of an object ends the row of hits.
	if (!hit_an_object) {
		Dtu_block_obj = -1;
		Dtu_block_hits = 0;
	}

	// ASCENT: Dtu_improved_physics, how long the player leans on scenery
	if (push_seen) {
		Dtu_push_time += sim_time;
		Dtu_push_idle = 0;
	}
	else if ((Dtu_push_idle += sim_time) > DTU_PUSH_IDLE) {
		Dtu_push_obj = -1;
		Dtu_push_time = Dtu_push_idle = 0;
	}

	if (Dtu_jump_set_vel && !(Dtu_player_state & DTU_PS_JUMPING) && Dtu_fix_jump_exit)
		vm_vec_scale(&pi->velocity,fixdiv(sim_time,DTU_REF_FRAMETIME));

	// The object must be in its segment.
	if (get_seg_masks(&obj->pos,obj->segnum,0).centermask != 0) {
		if (!update_object_seg(obj)) {
			int n = find_point_seg(&start_pos,obj->segnum);

			if (n != -1) {
				obj->pos = start_pos;
				obj_relink(objnum,n);
			}
		}
	}

	dtu_body_log(obj);
	dtu_move_log(obj);

	// 0x9E7B8: the lava
	dtu_lava_frame(obj,lava_seg,lava_side,Dtu_floor_dist < F1_0);

	return 1;
}
