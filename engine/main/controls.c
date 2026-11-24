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
 * $Source: f:/miner/source/main/rcs/controls.c $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:27:11 $
 * 
 * Code for controlling player movement
 * 
 * $Log: controls.c $
 * Revision 2.0  1995/02/27  11:27:11  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.50  1995/02/22  14:11:19  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.49  1994/12/15  13:04:10  mike
 * Replace Players[Player_num].time_total references with GameTime.
 * 
 * Revision 1.48  1994/11/27  23:12:13  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.47  1994/11/25  22:15:54  matt
 * Added asserts to try to trap frametime < 0 bug
 * 
 * Revision 1.46  1994/11/16  11:25:40  matt
 * Took out int3's since I determined that the errors are caused by neg frametime
 * 
 * Revision 1.45  1994/11/15  13:07:23  matt
 * Added int3's to try to trap bug
 * 
 * Revision 1.44  1994/10/14  16:18:12  john
 * Made Assert that the object was player just nicely exit
 * the function.
 * 
 * Revision 1.43  1994/10/13  11:35:25  john
 * Made Thrustmaster FCS Hat work.  Put a background behind the
 * keyboard configure.  Took out turn_sensitivity.  Changed sound/config
 * menu to new menu. Made F6 be calibrate joystick.
 * 
 * Revision 1.42  1994/09/29  11:22:02  mike
 * Zero thrust when player dies.
 * 
 * Revision 1.41  1994/09/16  13:10:30  mike
 * Hook in afterburner stuff.
 * 
 * Revision 1.40  1994/09/14  22:21:54  matt
 * Avoid post-death assert
 * 
 * Revision 1.39  1994/09/11  20:30:27  matt
 * Cleaned up thrust vars, changing a few names
 * 
 * Revision 1.38  1994/09/10  15:46:31  john
 * First version of new keyboard configuration.
 * 
 * Revision 1.37  1994/09/07  15:58:12  mike
 * Check for player dead in controls so you can't fire or move after dead, logical, huh?
 * 
 * Revision 1.36  1994/09/06  14:51:56  john
 * Added sensitivity adjustment, fixed bug with joystick button not
 * staying down.
 * 
 * Revision 1.35  1994/09/01  15:43:26  john
 * Put pitch bak like it was.
 * 
 * Revision 1.34  1994/08/31  18:59:35  john
 * Made rotthrust back like it was.
 * 
 * Revision 1.33  1994/08/31  18:49:17  john
 * Slowed Maxrothrust a bit,
 * ..
 * 
 * Revision 1.32  1994/08/31  18:32:05  john
 * Lower max rotational thrust
 * 
 * Revision 1.31  1994/08/29  21:18:27  john
 * First version of new keyboard/oystick remapping stuff.
 * 
 * Revision 1.30  1994/08/29  16:18:30  mike
 * trap divide overflow.
 * 
 * Revision 1.29  1994/08/26  14:40:45  john
 * *** empty log message ***
 * 
 * Revision 1.28  1994/08/26  12:23:50  john
 * MAde joystick read up to 15 times per second max.
 * 
 * Revision 1.27  1994/08/26  10:50:59  john
 * Took out Controls_always_stopped.
 * 
 * Revision 1.26  1994/08/26  10:47:27  john
 * *** empty log message ***
 * 
 * Revision 1.25  1994/08/26  10:46:50  john
 * New version of controls.
 * 
 * Revision 1.24  1994/08/25  19:41:44  john
 * *** empty log message ***
 * 
 * Revision 1.23  1994/08/25  18:44:55  john
 * *** empty log message ***
 * 
 * Revision 1.22  1994/08/25  18:43:46  john
 * First revision of new control code.
 * 
 * Revision 1.21  1994/08/24  20:02:46  john
 * Added cyberman support; made keys work key_down_time
 * returning seconds instead of milliseconds,.
 * 
 * 
 * Revision 1.20  1994/08/24  19:00:27  john
 * Changed key_down_time to return fixed seconds instead of
 * milliseconds.
 * 
 * Revision 1.19  1994/08/19  15:22:12  mike
 * Fix divide overflow in sliding.
 * 
 * Revision 1.18  1994/08/19  14:42:50  john
 * Added joystick sensitivity.
 * 
 * Revision 1.17  1994/08/17  16:50:01  john
 * Added damaging fireballs, missiles.
 * 
 * Revision 1.16  1994/08/12  22:41:54  john
 * Took away Player_stats; added Players array.
 * 
 * Revision 1.15  1994/08/09  16:03:56  john
 * Added network players to editor.
 * 
 * Revision 1.14  1994/07/28  12:33:31  matt
 * Made sliding use thrust, rather than changing velocity directly
 * 
 * Revision 1.13  1994/07/27  20:53:21  matt
 * Added rotational drag & thrust, so turning now has momemtum like moving
 * 
 * Revision 1.12  1994/07/25  10:24:06  john
 * Victor stuff.
 * 
 * Revision 1.11  1994/07/22  17:53:16  john
 * Added better victormax support
 * 
 * Revision 1.10  1994/07/21  21:31:29  john
 * First cheapo version of VictorMaxx tracking.
 * 
 * Revision 1.9  1994/07/15  15:16:18  john
 * Fixed some joystick stuff.
 * 
 * Revision 1.8  1994/07/15  09:32:09  john
 * Changes player movement.
 * 
 * Revision 1.7  1994/07/13  00:14:58  matt
 * Moved all (or nearly all) of the values that affect player movement to 
 * bitmaps.tbl
 * 
 * Revision 1.6  1994/07/12  12:40:14  matt
 * Revamped physics system
 * 
 * Revision 1.5  1994/07/02  13:50:39  matt
 * Cleaned up includes
 * 
 * Revision 1.4  1994/07/01  10:55:25  john
 * Added analog joystick throttle
 * 
 * Revision 1.3  1994/06/30  20:04:28  john
 * Added -joydef support.
 * 
 * Revision 1.2  1994/06/30  19:01:58  matt
 * Moved flying controls code from physics.c to controls.c
 * 
 * Revision 1.1  1994/06/30  18:41:25  matt
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: controls.c 2.0 1995/02/27 11:27:11 john Exp $";
#pragma on (unreferenced)

#include <stdlib.h>

#include "mono.h"
#include "key.h"
#include "joy.h"
#include "timer.h"
#include "error.h"

#include "inferno.h"
#include "game.h"
#include "object.h"
#include "player.h"

#include "controls.h"
#include "joydefs.h"
// ASCENT: victor.h removed (virtual reality headset, not built)
#include "render.h"
#include "args.h"
#include "palette.h"
#include "mouse.h"
#include "kconfig.h"

//look at keyboard, mouse, joystick, CyberMan, whatever, and set 
//physics vars rotvel, velocity

// ASCENT: the controls of the target game, see the end of the file
#include <stdio.h>
#include <string.h>
#include "physics.h"
#include "dtuobj.h"		// ASCENT: the objects of the target game
#include "dtucombat.h"	// ASCENT: the attack of the player
#include "dtutrig.h"		// ASCENT: dtu_player_use()
#include "dtuthief.h"		// ASCENT: the thief keys and the climb
#include "dtuspell.h"		// ASCENT: the jump key stops a cast
#include "dtustep.h"		// ASCENT: Dtu_fixed_step
#include "dtuopts.h"		// ASCENT: the Improvements of the movement
#include "dtudisp.h"		// ASCENT: Dtu_immersive, the key V
#include "dtulang.h"		// ASCENT: LN(), the Auto Run messages
#include "dtuchar.h"		// ASCENT: the hand slot for Disarm
#include "bm.h"
#include "key.h"
#include "joy.h"
#include "gauges.h"
#include "segment.h"
#include "automap.h"		// ASCENT: the map of the map screen takes the controls

static void dtu_move_init(void);
static void dtu_read_flying_controls(object *obj);

void read_flying_controls( object * obj )
{
	fix	afterburner_thrust;

	Assert(FrameTime > 0); 		//Get MATT if hit this!

	if (Player_is_dead) {
		vm_vec_zero(&obj->mtype.phys_info.rotthrust);
		vm_vec_zero(&obj->mtype.phys_info.thrust);
		return;
	}

	if ((obj->type!=OBJ_PLAYER) || (obj->id!=Player_num)) return;	//references to player_ship require that this obj be the player

	// ASCENT: the player of the target game is a person who walks
	dtu_move_init();
	if (Dtu_player_walk) {
		dtu_read_flying_controls(obj);
		return;
	}

	//	Couldn't the "50" in the next three lines be changed to "64" with no ill effect?
	obj->mtype.phys_info.rotthrust.x = Controls.pitch_time;
	obj->mtype.phys_info.rotthrust.y = Controls.heading_time;
	obj->mtype.phys_info.rotthrust.z = Controls.bank_time;

//	mprintf( (0, "Rot thrust = %.3f,%.3f,%.3f\n", f2fl(obj->mtype.phys_info.rotthrust.x),f2fl(obj->mtype.phys_info.rotthrust.y), f2fl(obj->mtype.phys_info.rotthrust.z) ));

	afterburner_thrust = 0;
	if (Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER)
		afterburner_thrust = FrameTime;

	// Set object's thrust vector for forward/backward
	vm_vec_copy_scale(&obj->mtype.phys_info.thrust,&obj->orient.fvec, Controls.forward_thrust_time + afterburner_thrust );
	
	// slide left/right
	vm_vec_scale_add2(&obj->mtype.phys_info.thrust,&obj->orient.rvec, Controls.sideways_thrust_time );

	// slide up/down
	vm_vec_scale_add2(&obj->mtype.phys_info.thrust,&obj->orient.uvec, Controls.vertical_thrust_time );

	if (obj->mtype.phys_info.flags & PF_WIGGLE) {
		fix swiggle;
		fix_fastsincos(GameTime, &swiggle, NULL);
		vm_vec_scale_add2(&obj->mtype.phys_info.velocity,&obj->orient.uvec,fixmul(swiggle,Player_ship->wiggle));
	}

	// As of now, obj->mtype.phys_info.thrust & obj->mtype.phys_info.rotthrust are 
	// in units of time... In other words, if thrust==FrameTime, that
	// means that the user was holding down the Max_thrust key for the
	// whole frame.  So we just scale them up by the max, and divide by
	// FrameTime to make them independant of framerate

	//	Prevent divide overflows on high frame rates.
	//	In a signed divide, you get an overflow if num >= div<<15
	{
		fix	ft = FrameTime;

		//	Note, you must check for ft < F1_0/2, else you can get an overflow  on the << 15.
		if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_thrust)) {
			mprintf((0, "Preventing divide overflow in controls.c for Max_thrust!\n"));
			ft = (Player_ship->max_thrust >> 15) + 1;
		}

		vm_vec_scale( &obj->mtype.phys_info.thrust, fixdiv(Player_ship->max_thrust,ft) );

		if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_rotthrust)) {
			mprintf((0, "Preventing divide overflow in controls.c for max_rotthrust!\n"));
			ft = (Player_ship->max_thrust >> 15) + 1;
		}

		vm_vec_scale( &obj->mtype.phys_info.rotthrust, fixdiv(Player_ship->max_rotthrust,ft) );
	}

}

//------------------------------------------------------------------------
// ASCENT: new, to the end of the file. The controls of the player of
// Descent to Undermountain, the scripted input for tests and the log of
// the movement. See docs/re/physics.md.

extern fix fixed_frametime;		// in game.c

// The rate of the turn keys, as a part of the rate of the executable
// (1.25 turns per second). A default of the port, set by the owner of the
// project: 0.75. -origphys gives the rate of the executable. The mouse
// and the joystick are not scaled.
fix Dtu_turn_scale = 0xc000;

// ASCENT: Dtu_fix_turn_ramp with the turn inertia (physics.h)
int Dtu_turn_dir = 0;
fix Dtu_turn_hold = 0, Dtu_turn_full_thrust = 0, Dtu_turn_other_thrust = 0;
static fix Turn_other_heading = 0;

// The actions of the option -testmove
#define TM_FORWARD	0x0001
#define TM_BACK		0x0002
#define TM_LEFT		0x0004
#define TM_RIGHT		0x0008
#define TM_SLIDELEFT	0x0010
#define TM_SLIDERIGHT 0x0020
#define TM_RUN			0x0040
#define TM_JUMP		0x0080
#define TM_LOOKUP		0x0100
#define TM_LOOKDOWN	0x0200
#define TM_UP			0x0400
#define TM_DOWN		0x0800
#define TM_USE			0x1000
#define TM_ATTACK		0x2000		// ASCENT: the attack of the player, see dtuobj.c
#define TM_PICKLOCK	0x4000		// ASCENT: the thief keys O, F and M (dtuthief.c)
#define TM_SEARCH		0x8000
#define TM_SNEAK		0x10000
#define TM_DISARM		0x20000		// ASCENT: the key H, the empty hand
#define TM_CLIMB		0x40000		// ASCENT: Climb Wall (the key Q)

static struct { char *name; int mask; } Test_action_names[] = {
	{"forward",TM_FORWARD}, {"back",TM_BACK}, {"left",TM_LEFT}, {"right",TM_RIGHT},
	{"slideleft",TM_SLIDELEFT}, {"slideright",TM_SLIDERIGHT}, {"run",TM_RUN},
	{"jump",TM_JUMP}, {"lookup",TM_LOOKUP}, {"lookdown",TM_LOOKDOWN},
	{"up",TM_UP}, {"down",TM_DOWN}, {"use",TM_USE}, {"attack",TM_ATTACK},
	{"picklock",TM_PICKLOCK}, {"search",TM_SEARCH}, {"sneak",TM_SNEAK}, {"disarm",TM_DISARM}, {"climb",TM_CLIMB}, {"wait",0}
};

#define MAX_TEST_STEPS	64

static struct {
	int	mask;
	int	frames;		// length in frames, or
	fix	time;			// length in game time when frames is 0
} Test_steps[MAX_TEST_STEPS];
static int N_test_steps = 0;
static FILE *Move_log = NULL;

// ASCENT: a line of the physics code in the log of -movelog.
void dtu_move_log_line(char *text)
{
	if (Move_log) {
		fputs(text,Move_log);
		fflush(Move_log);
	}
}

// -testmove ACTION[+ACTION]:LENGTH[,...]. LENGTH is a number of frames, or
// a time in milliseconds with "ms" after the number. Example:
// -testmove forward+run:2000ms,left:30,wait:10,jump:1
static void dtu_parse_testmove(char *s)
{
	while (*s && N_test_steps < MAX_TEST_STEPS) {
		int mask = 0, n;

		while (*s && *s != ':' && *s != ',') {
			int i, len = 0;

			while (s[len] && s[len] != '+' && s[len] != ':' && s[len] != ',')
				len++;
			for (i=0; i<(int)(sizeof(Test_action_names)/sizeof(*Test_action_names)); i++)
				if ((int)strlen(Test_action_names[i].name) == len && !strnicmp(s,Test_action_names[i].name,len))
					mask |= Test_action_names[i].mask;
			s += len;
			if (*s == '+')
				s++;
		}

		n = 1;
		if (*s == ':') {
			s++;
			n = atoi(s);
			while (*s >= '0' && *s <= '9')
				s++;
		}

		Test_steps[N_test_steps].mask = mask;
		if (s[0] == 'm' && s[1] == 's') {
			Test_steps[N_test_steps].frames = 0;
			Test_steps[N_test_steps].time = fixmuldiv(n,F1_0,1000);
			s += 2;
		}
		else {
			Test_steps[N_test_steps].frames = n;
			Test_steps[N_test_steps].time = 0;
		}
		N_test_steps++;

		while (*s && *s != ',')
			s++;
		if (*s == ',')
			s++;
	}
}

// Reads the options one time.
static void dtu_move_init(void)
{
	static int done = 0;
	int t;

	if (done)
		return;
	done = 1;

	if (FindArg("-fly"))
		Dtu_player_walk = 0;

	// ASCENT: Dtu_fixed_step (dtustep.c)
	if (FindArg("-origphys") || FindArg("-nofixedstep"))
		Dtu_fixed_step = 0;
	if (FindArg("-origphys"))
		Dtu_fix_physics_steps = Dtu_fix_frame_drag = Dtu_fix_stop_decay =
			Dtu_fix_floor_push = Dtu_fix_look_return = Dtu_fix_fly_bob = Dtu_fix_turn_ramp = Dtu_fix_jump_exit =
			Dtu_fix_jump_path = Dtu_fix_ladder = Dtu_fix_wall_slide = Dtu_fix_wall_overlap =
			Dtu_fix_fall_sound = Dtu_fix_fall_land_player = 0;
	if (FindArg("-origphys"))
		Dtu_turn_scale = F1_0;

	// The Improvements of the movement are not the rules of the executable
	if (FindArg("-origphys"))
		Dtu_improved_physics = Dtu_improved_jump = 0;

	// -improved PJ: the two Improvements of the movement for a test, for
	// example -improved 01 (physics off, jump on)
	if ((t = FindArg("-improved")) != 0 && t+1 < Num_args && strlen(Args[t+1]) == 2) {
		Dtu_improved_physics = Args[t+1][0] == '1';
		Dtu_improved_jump = Args[t+1][1] == '1';
	}

	// fixed_frametime exists only in a debug build (game.c)
	#ifndef NDEBUG
	if ((t = FindArg("-fixedfps")) != 0 && t+1 < Num_args && atoi(Args[t+1]) > 0)
		fixed_frametime = F1_0 / atoi(Args[t+1]);
	#endif

	if ((t = FindArg("-testmove")) != 0 && t+1 < Num_args)
		dtu_parse_testmove(Args[t+1]);

	if ((t = FindArg("-movelog")) != 0 && t+1 < Num_args) {
		if (!strcmp(Args[t+1],"-"))
			Move_log = stdout;
		else
			Move_log = fopen(Args[t+1],"wt");
	}
}

// Returns the actions of the test script for this frame.
static int dtu_test_keys(void)
{
	static int step = 0, frames = 0;
	static fix time = 0;
	int mask;

	if (step >= N_test_steps)
		return 0;

	mask = Test_steps[step].mask;

	if (Test_steps[step].frames) {
		if (++frames >= Test_steps[step].frames) {
			step++;
			frames = 0;
		}
	}
	else {
		time += FrameTime;
		if (time >= Test_steps[step].time) {
			time -= Test_steps[step].time;
			step++;
		}
	}

	return mask;
}

// Prints one line for each frame of the player: the frame number, the game
// time, the position, the segment, the velocity, the distance of the feet
// to the floor, the state, the move mode and the look pitch.
void dtu_move_log(object *obj)
{
	static int frame = 0;

	frame++;
	if (!Move_log)
		return;

	if (frame == 1)
		fprintf(Move_log,"MOVEINFO mass %.4f drag %.4f max_thrust %.4f max_rotthrust %.4f eye_offset %.4f size %.4f flags %02x walk %d fix %d%d%d%d%d%d%d%d%d%d%d improved %d%d\n",
			f2fl(obj->mtype.phys_info.mass), f2fl(obj->mtype.phys_info.drag), f2fl(Player_ship->max_thrust), f2fl(Player_ship->max_rotthrust),
			f2fl(N_player_types > 0 ? Player_type_info[0].eye_offset : 0), f2fl(obj->size), obj->mtype.phys_info.flags, Dtu_player_walk,
			Dtu_fix_physics_steps, Dtu_fix_frame_drag, Dtu_fix_stop_decay, Dtu_fix_floor_push, Dtu_fix_look_return, Dtu_fix_fly_bob, Dtu_fix_turn_ramp, Dtu_fix_jump_exit,
			Dtu_fix_jump_path, Dtu_fix_ladder, Dtu_fix_wall_slide, Dtu_improved_physics, Dtu_improved_jump);

	fprintf(Move_log,"MOVE %d t %.4f pos %.4f %.4f %.4f seg %d vel %.4f %.4f %.4f floor %.4f state %02x mode %d pitch %d fvec %.4f %.4f\n",
		frame, f2fl(GameTime),
		f2fl(obj->pos.x), f2fl(obj->pos.y), f2fl(obj->pos.z), obj->segnum,
		f2fl(obj->mtype.phys_info.velocity.x), f2fl(obj->mtype.phys_info.velocity.y), f2fl(obj->mtype.phys_info.velocity.z),
		f2fl(Dtu_floor_dist), Dtu_player_state, Dtu_move_mode, Dtu_look_pitch,
		f2fl(obj->orient.fvec.x), f2fl(obj->orient.fvec.z));
	fflush(Move_log);
}

// The integral of the turn ramp of the executable from the key down to the
// time t (fix seconds), at the reference frame time T: the rate is k/8 in
// frame k (k = 1 to 8, from (k-1)T to kT), then 1.
static fix dtu_turn_ramp_integral(fix t)
{
	const int64_t T = DTU_REF_FRAMETIME;
	int64_t n;

	if (t <= 0)
		return 0;
	if (t >= 8*T)
		return (fix) (T*36/8 + (t - 8*T));		// 36/8 T for the 8 steps
	n = t / T;										// whole frames
	return (fix) (T*n*(n+1)/16 + (n+1)*(t - n*T)/8);
}

// Reads the keyboard, the mouse and the joystick for the movement of the
// player. The keys come from the key table (kconfig.c, docs/re/controls.md),
// which the Controls menu changes. The defaults of the port (the owner of
// the project; the executable in brackets):
//   W or up, S or down       forward, backward (up or keypad 8, down or keypad 2)
//   left or keypad 4, right or keypad 6   turn (side step in the side step mode)
//   A or keypad 1, D or keypad 3          side step left, right (Z, X)
//   Alt                      side step mode on and off
//   Shift                    run.  Caps Lock: run always, on and off
//   Space                    jump
//   Page Up, Page Down       look up, look down
//   Home, End                glance up, glance down (the view returns)
//   Insert, Delete           up, down in flight and in levitation
//   V                        mouse look on and off ("immersive mode")
//   E                        use ("Activate": doors and triggers in front) (A)
//   H                        disarm: the empty hand (D)
//   Ctrl                     attack.  C: the next hot key (Cycle Weapon)
//   M, F, O                  Move silently, Find Traps, Open Locks
// The time of a key is the frame time when the key is down. The
// executable uses the time that the key was down in the frame.
void dtu_read_controls(void)
{
	static int sidestep_mode = 0, autorun = 0, mouse_look = 0, glance_return = 0;
	static int alt_last = 0, caps_last = 0, space_last = 0, v_last = 0;
	static fix return_acc = 0;
	static fix turn_time = 0, turn_ramp = 0;
	object *obj = ConsoleObject;
	fix ft = FrameTime;
	fix fwd = 0, side = 0, heading = 0, vert = 0;
	int t = dtu_test_keys();
	int turn = 0, run, look = 0, glance = 0, key;
	int sens = Config_joystick_sensitivity > 8 ? 8 : Config_joystick_sensitivity;	// ASCENT: 0x13BFEF

	// ASCENT: while the map of the map screen has the controls, the player
	// gets none (0x40E50)
	if (dtu_automap_read_controls()) {
		Dtu_move_mode = 0;
		return;
	}

	// ASCENT: the keys of the key table (kconfig.c, the items of the
	// executable at 0x13C190), and the buttons of the mouse or joystick.
	if (dtu_action_down(DTU_ACT_FORWARD) || (t & TM_FORWARD))		fwd += ft;
	if (dtu_action_down(DTU_ACT_BACK) || (t & TM_BACK))				fwd -= ft;
	if (dtu_action_down(DTU_ACT_TURN_LEFT) || (t & TM_LEFT))		turn--;
	if (dtu_action_down(DTU_ACT_TURN_RIGHT) || (t & TM_RIGHT))		turn++;
	if (dtu_action_down(DTU_ACT_SLIDE_LEFT) || (t & TM_SLIDELEFT))	side -= ft;
	if (dtu_action_down(DTU_ACT_SLIDE_RIGHT) || (t & TM_SLIDERIGHT))	side += ft;
	// ASCENT: the sticks of the gamepad (kconfig.c): the amount of the
	// direction, after the dead zone
	dtu_pad_frame();
	fwd += fixmul(ft,dtu_pad_amount(DTU_ACT_FORWARD) - dtu_pad_amount(DTU_ACT_BACK));
	side += fixmul(ft,dtu_pad_amount(DTU_ACT_SLIDE_RIGHT) - dtu_pad_amount(DTU_ACT_SLIDE_LEFT));
	if (fwd > ft) fwd = ft;
	if (fwd < -ft) fwd = -ft;
	if (side > ft) side = ft;
	if (side < -ft) side = -ft;

	key = dtu_action_down(DTU_ACT_SLIDE_ON);
	if (key && !alt_last)
		sidestep_mode = !sidestep_mode;
	alt_last = key;

	key = dtu_action_down(DTU_ACT_AUTO_RUN);
	if (key && !caps_last) {
		autorun = !autorun;
		// ASCENT: 0x53176 to 0x531A0: "Auto Run is on." (0x127F0C) or "Auto
		// Run is off." (0x127F1C) with 0x347A2 and Player_num, the color of
		// the player. The port had no message. The literals are not in the
		// string table (D-096); the port translates them (dtulang.h).
		if (Dtu_display_active) {
			dtu_display_next_message_color(0);
			dtu_display_add_message(autorun ? LN("Auto Run is on.") : LN("Auto Run is off."));
		}
	}
	caps_last = key;

	// ASCENT: the display of the game (dtudisp.c, 0x54439) owns the
	// Immersive Mode key: it toggles Dtu_immersive and adds the message. A
	// second toggle here gave two messages for each press. This one acts
	// only without the display.
	key = dtu_action_down(DTU_ACT_IMMERSIVE);
	if (Dtu_display_active)
		mouse_look = Dtu_immersive;
	else if (key && !v_last) {
		mouse_look = !mouse_look;
		HUD_init_message(mouse_look ? "Immersive Mode" : "Non-Immersive Mode");
	}
	v_last = key;

	// 0x533DC, as in Descent 1: a turn key adds one eighth of its time to
	// the heading time of the frame before. The turn has its whole rate
	// after 8 frames. The corrected rule: the ramp is a function of the
	// time that the key is down (turn_ramp), with the steps of the
	// executable at the reference frame time: the rate is k/8 in reference
	// frame k, 1 from frame 8 on. The heading time of a frame is the
	// integral of that function over the frame, so the angle after a time
	// does not depend on the frame rate.
	if (sidestep_mode || turn == 0)
		turn_time = turn_ramp = 0;
	else if (Dtu_fix_turn_ramp) {
		if ((turn > 0 && turn_time < 0) || (turn < 0 && turn_time > 0))
			turn_ramp = 0;
		turn_time = turn * (dtu_turn_ramp_integral(turn_ramp + ft) - dtu_turn_ramp_integral(turn_ramp));
		Dtu_turn_hold = turn_ramp;			// for the turn with inertia (physics.c)
		if (turn_ramp < 9*DTU_REF_FRAMETIME)
			turn_ramp += ft;
	}
	else {
		if ((turn > 0 && turn_time < 0) || (turn < 0 && turn_time > 0))
			turn_time = 0;
		turn_time += turn * ft / 8;
		if (turn_time > ft) turn_time = ft;
		if (turn_time < -ft) turn_time = -ft;
	}
	Dtu_turn_dir = sidestep_mode ? 0 : turn;

	if (sidestep_mode)
		side += turn * ft;
	else if (Dtu_fix_turn_ramp)
		// rounded: the truncation lost 0.3% of the turn at 300 frames
		heading += (fix) (((int64_t) turn_time * Dtu_turn_scale + (turn_time < 0 ? -0x8000 : 0x8000)) / 65536);
	else
		heading += fixmul(turn_time,Dtu_turn_scale);
	Turn_other_heading = heading;		// the turn keys; the rest is added below

	// ASCENT: the axes of the mouse (control type 5, only in the mouse look
	// mode) or of the joystick (types 1 to 4), as the key table sets them:
	// look up and down, turn, side step, up and down. The defaults of the
	// executable: the mouse x turns and y looks, the joystick x turns and
	// y looks. The scales are guesses.
	{
		int a_look, a_turn, a_slide, a_up;
		int src = dtu_control_axes(&a_look, &a_turn, &a_slide, &a_up);

		// ASCENT: Joystick/Mouse Sensitivity of F2 (byte 0x13BFEF, 0 to
		// 8, 8 by default at 0x71580): the executable scales the look and
		// turn axes of the mouse and the joystick by it / 8 (0x52BCE to
		// 0x53667).
		a_look = a_look * sens / 8;
		a_turn = a_turn * sens / 8;

		if (src == 1 && mouse_look) {
			heading += fixmuldiv(ft,a_turn,16);
			look += a_look * 32;
			side += fixmuldiv(ft,a_slide,16);
			vert -= fixmuldiv(ft,a_up,16);
		} else if (src == 2) {
			if (abs(a_turn) > 16) heading += fixmuldiv(ft,a_turn,128);
			if (abs(a_look) > 16) look += fixmuldiv(ft,a_look,128);
			if (abs(a_slide) > 16) side += fixmuldiv(ft,a_slide,128);
			if (abs(a_up) > 16) vert -= fixmuldiv(ft,a_up,128);
		}
	}

	// ASCENT: the gamepad turns with no ramp (as the mouse) and looks. The
	// whole stick turns at the rate of a turn key, times the speed setting.
	// ASCENT: the gamepad is a joystick for the sensitivity.
	heading += fixmul(fixmul(ft,dtu_pad_amount(DTU_ACT_TURN_RIGHT) - dtu_pad_amount(DTU_ACT_TURN_LEFT)),Dtu_turn_scale) * sens / 8;
	look += fixmul(ft,dtu_pad_amount(DTU_ACT_LOOK_DOWN) - dtu_pad_amount(DTU_ACT_LOOK_UP)) * sens / 8;

	// The look keys: the pitch changes by the key time * sensitivity / 8,
	// as an angle (0x539E1 and 0x53A8C, the key items 0x1F to 0x21).
	if (dtu_action_down(DTU_ACT_LOOK_UP) || (t & TM_LOOKUP))		look -= ft * sens / 8;
	if (dtu_action_down(DTU_ACT_LOOK_DOWN) || (t & TM_LOOKDOWN))	look += ft * sens / 8;
	if (dtu_action_down(DTU_ACT_QUICK_UP))		{ look -= ft * sens / 8; glance = 1; }
	if (dtu_action_down(DTU_ACT_QUICK_DOWN))		{ look += ft * sens / 8; glance = 1; }

	if (look) {
		int p = Dtu_look_pitch + look;

		if (p < DTU_LOOK_MIN) p = DTU_LOOK_MIN;
		if (p > DTU_LOOK_MAX) p = DTU_LOOK_MAX;
		Dtu_look_pitch = p;
		glance_return = glance;
		return_acc = 0;
	}
	else if (glance_return) {
		// 0x53C9F: the view returns after a glance.
		int step, p = Dtu_look_pitch;

		if (Dtu_fix_look_return) {
			return_acc += fixmuldiv((DTU_LOOK_RETURN + DTU_REF_FRAMETIME) >> 2,ft,DTU_REF_FRAMETIME) << 8;
			step = return_acc >> 8;
			return_acc &= 0xff;
		}
		else
			step = (DTU_LOOK_RETURN + ft) >> 2;

		if (p < 0)	{ p += step; if (p >= 0) { p = 0; glance_return = 0; } }
		else			{ p -= step; if (p <= 0) { p = 0; glance_return = 0; } }
		Dtu_look_pitch = p;
	}

	// Up and down, only in flight and in levitation
	if (obj->dtu.m_88 & (DTU_OF_FLY|DTU_OF_LEVITATE)) {
		if (dtu_action_down(DTU_ACT_FLOAT_UP) || (t & TM_UP))		vert += ft;
		if (dtu_action_down(DTU_ACT_FLOAT_DOWN) || (t & TM_DOWN))	vert -= ft;
		vert += fixmul(ft,dtu_pad_amount(DTU_ACT_FLOAT_UP) - dtu_pad_amount(DTU_ACT_FLOAT_DOWN));
	}
	else
		vert = 0;			// the axis of the mouse or the joystick too

	key = dtu_action_down(DTU_ACT_JUMP) || (t & TM_JUMP);
	if (key && !space_last) {
		// 0x546CC: the jump key stops a cast when the player does not
		// levitate (0x9305D with 0, "Your spell was aborted."); then no jump
		if (!(obj->dtu.m_88 & DTU_OF_LEVITATE) && dtu_spell_interrupt(obj,0))
			;
		else
			dtu_player_jump(obj);
	}
	space_last = key;

	// The thief keys of the key table (kconfig items 54 to 59, defaults
	// 0x13C010): M Move silently, F Find Traps, O Open Locks. Each action
	// waits half a second between two uses.
	{
		static int o_last = 0, f_last = 0, m_last = 0;
		int ko = dtu_action_down(DTU_ACT_OPEN_LOCKS) || (t & TM_PICKLOCK), kf = dtu_action_down(DTU_ACT_FIND_TRAPS) || (t & TM_SEARCH),
			km = dtu_action_down(DTU_ACT_MOVE_SILENTLY) || (t & TM_SNEAK);

		if (ko && !o_last) dtu_thief_open_locks();
		if (kf && !f_last) dtu_thief_search_toggle();
		if (km && !m_last) dtu_thief_move_silently();
		o_last = ko; f_last = kf; m_last = km;
	}

	// The use action: "Activate" of the key table of the executable
	// (entries 39 and 40, default scan code 0x1E at 0x13C037, the key A;
	// the port has E). 0x53D6A calls it in each frame while the key is
	// down, and the action itself waits half a second between two uses.
	if (dtu_action_down(DTU_ACT_ACTIVATE) || (t & TM_USE))
		dtu_player_use();

	// ASCENT: Climb Wall (the port, default Q): the climb of 0x951DE on
	// its own key, so Activate at a lever gives no climb message
	// (Dtu_fix_climb_key, kconfig.h). As Activate: in each frame while the
	// key is down, half a second between two tries.
	if (dtu_action_down(DTU_ACT_CLIMB) || (t & TM_CLIMB))
		dtu_thief_climb_key(obj);

	// Disarm: entry 50 of the key table (default D at 0x13C010; the port
	// has H). 0x53E91: on each frame while the key is down, 0x43A5A(-3)
	// "KCFG: Disarming": the empty hand (hand slot -1, model letter '_').
	// 0x43A5A does nothing in the attack states 6 and 11.
	if ((dtu_action_down(DTU_ACT_DISARM) || (t & TM_DISARM)) && Dtu_char.current.n >= 0 &&
		dtu_combat_view_state() != DAS_ATTACK && dtu_combat_view_state() != DAS_ATTACK3) {
		dtu_char_log("KCFG: Disarming");
		dtu_char_set_current(-1);
	}

	// The attack with the item in the hand: Left Ctrl and Right Ctrl
	// (keyboard items 18 and 19 of the key table, 0x13C010), on the key
	// down (0x2FCBE). See dtucombat.c. Outside the immersive mode also the
	// left button pressed in the view (0x2FB10, dtu_display_view_attack).
	dtu_combat_attack_key(dtu_action_down(DTU_ACT_ATTACK) || (t & TM_ATTACK) || dtu_display_view_attack());
	dtu_combat_frame();

	// Cycle Weapon: items 28 and 29 of the key table (default C), on the
	// key down. 0x544F2 goes through the hot keys from the one after the
	// last chosen and takes the first that holds a weapon it can use
	// (0x43A5A). The port takes the next hot key that holds an item that
	// is not in the hand. The weapon tests of 0x544F2 are not ported.
	{
		static int c_last = 0;
		int kc = dtu_action_down(DTU_ACT_CYCLE_WEAPON);

		if (kc && !c_last) {
			int i, h = -1;

			for (i = 0; i < DTU_N_HOT_KEYS; i++)
				if (dtu_char_hot_key_in_hand(i))
					h = i;
			// ASCENT: 0x544F2 takes weapons only: the boxes with a spell
			// are passed over
			for (i = 1; i <= DTU_N_HOT_KEYS; i++) {
				int k = (h + i + DTU_N_HOT_KEYS) % DTU_N_HOT_KEYS;
				if (dtu_char_hot_key_item(k) >= 0 && k != h) {
					dtu_char_press_hot_key(k);
					break;
				}
			}
		}
		c_last = kc;
	}

	// 0x54033: the move mode. A run is 1.5 times the walk.
	run = dtu_action_down(DTU_ACT_RUN) || autorun || (t & TM_RUN);
	Dtu_move_mode = 0;
	if (fwd && !(Dtu_player_state & (DTU_PS_FALLING|DTU_PS_JUMPING|DTU_PS_CLIMBING))) {
		Dtu_move_mode = 1;
		if (run) {
			fwd = fixmul(fwd,DTU_RUN_SCALE);
			Dtu_move_mode = 2;
		}
	}
	if (side && !(Dtu_player_state & (DTU_PS_FALLING|DTU_PS_JUMPING|DTU_PS_CLIMBING))) {
		Dtu_move_mode = 1;
		if (run) {
			side = fixmul(side,DTU_RUN_SCALE);
			Dtu_move_mode = 2;
		}
	}

	if (heading > ft) heading = ft;
	if (heading < -ft) heading = -ft;
	Turn_other_heading = heading - Turn_other_heading;		// the mouse and the joystick

	Controls.pitch_time = 0;
	Controls.bank_time = 0;
	Controls.heading_time = heading;
	Controls.forward_thrust_time = fwd;
	Controls.sideways_thrust_time = side;
	Controls.vertical_thrust_time = vert;
}

// The read_flying_controls() of DTUGAME.EXE (0x2BFAE).
static void dtu_read_flying_controls(object *obj)
{
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector dir, up;
	fix ft = FrameTime;

	dtu_read_controls();

	if (obj->dtu.m_88 & (DTU_OF_HELD & 0x4000))
		return;

	if (Controls.forward_thrust_time == 0)
		dtu_stop_decay(obj);
	else
		dtu_stop_decay_reset();

	pi->rotthrust.x = fixmul(Controls.pitch_time,DTU_ROTTHRUST_SCALE);
	pi->rotthrust.y = fixmul(Controls.heading_time,DTU_ROTTHRUST_SCALE);
	pi->rotthrust.z = 0;

	up.x = 0; up.y = F1_0; up.z = 0;

	if (Controls.forward_thrust_time) {
		if (obj->dtu.m_88 & DTU_OF_FLY) {
			// in the direction of the view
			vms_angvec a;
			vms_matrix headm, viewm;

			a.p = Dtu_look_pitch; a.b = 0; a.h = 0;
			vm_angles_2_matrix(&headm,&a);
			vm_matrix_x_matrix(&viewm,&obj->orient,&headm);
			dir = viewm.fvec;
		}
		else if (Dtu_player_state & DTU_PS_CLIMBING) {
			// 0x2C144: the climb (dtuthief.c)
			dtu_thief_climb_thrust(obj,Controls.forward_thrust_time,0);
			vm_vec_zero(&dir);
		}
		else if (!(obj->dtu.m_88 & DTU_OF_LEVITATE))
			vm_vec_scale_add(&dir,&obj->orient.fvec,&up,vm_vec_dot(&up,&obj->orient.fvec));
		else {
			dir = obj->orient.fvec;
			dir.y = 0;
			vm_vec_normalize(&dir);
		}
		vm_vec_scale_add2(&pi->thrust,&dir,Controls.forward_thrust_time);
	}

	if (Controls.sideways_thrust_time && (Dtu_player_state & DTU_PS_CLIMBING))
		dtu_thief_climb_thrust(obj,0,Controls.sideways_thrust_time);
	else if (Controls.sideways_thrust_time) {
		vm_vec_scale_add(&dir,&obj->orient.rvec,&up,vm_vec_dot(&up,&obj->orient.rvec));
		vm_vec_scale_add2(&pi->thrust,&dir,Controls.sideways_thrust_time);
		pi->rotthrust.z = Controls.sideways_thrust_time;
	}

	pi->thrust.y += Controls.vertical_thrust_time;

	//	Prevent divide overflows on high frame rates, as in Descent 1.
	if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_thrust))
		ft = (Player_ship->max_thrust >> 15) + 1;
	vm_vec_scale( &pi->thrust, fixdiv(Player_ship->max_thrust,ft) );

	if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_rotthrust))
		ft = (Player_ship->max_thrust >> 15) + 1;
	vm_vec_scale( &pi->rotthrust, fixdiv(Player_ship->max_rotthrust,ft) );

	// ASCENT: Dtu_fix_turn_ramp: the rotthrust of a whole turn key and that
	// of the mouse and the joystick, for the turn with inertia (physics.c)
	Dtu_turn_full_thrust = fixmul(fixmul(Dtu_turn_scale,DTU_ROTTHRUST_SCALE),Player_ship->max_rotthrust);
	Dtu_turn_other_thrust = fixmul(fixmul(Turn_other_heading,DTU_ROTTHRUST_SCALE),fixdiv(Player_ship->max_rotthrust,ft));
}
