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
#include "bm.h"
#include "key.h"
#include "joy.h"
#include "gauges.h"

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

static struct { char *name; int mask; } Test_action_names[] = {
	{"forward",TM_FORWARD}, {"back",TM_BACK}, {"left",TM_LEFT}, {"right",TM_RIGHT},
	{"slideleft",TM_SLIDELEFT}, {"slideright",TM_SLIDERIGHT}, {"run",TM_RUN},
	{"jump",TM_JUMP}, {"lookup",TM_LOOKUP}, {"lookdown",TM_LOOKDOWN},
	{"up",TM_UP}, {"down",TM_DOWN}, {"use",TM_USE}, {"attack",TM_ATTACK}, {"wait",0}
};

#define MAX_TEST_STEPS	64

static struct {
	int	mask;
	int	frames;		// length in frames, or
	fix	time;			// length in game time when frames is 0
} Test_steps[MAX_TEST_STEPS];
static int N_test_steps = 0;
static FILE *Move_log = NULL;

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

	if (FindArg("-origphys"))
		Dtu_fix_physics_steps = Dtu_fix_frame_drag = Dtu_fix_stop_decay =
			Dtu_fix_floor_push = Dtu_fix_look_return = Dtu_fix_fly_bob = Dtu_fix_turn_ramp = Dtu_fix_jump_exit = 0;

	if ((t = FindArg("-fixedfps")) != 0 && t+1 < Num_args && atoi(Args[t+1]) > 0)
		fixed_frametime = F1_0 / atoi(Args[t+1]);

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
		fprintf(Move_log,"MOVEINFO mass %.4f drag %.4f max_thrust %.4f max_rotthrust %.4f eye_offset %.4f size %.4f flags %02x walk %d fix %d%d%d%d%d%d%d%d\n",
			f2fl(obj->mtype.phys_info.mass), f2fl(obj->mtype.phys_info.drag), f2fl(Player_ship->max_thrust), f2fl(Player_ship->max_rotthrust),
			f2fl(N_player_types > 0 ? Player_type_info[0].eye_offset : 0), f2fl(obj->size), obj->mtype.phys_info.flags, Dtu_player_walk,
			Dtu_fix_physics_steps, Dtu_fix_frame_drag, Dtu_fix_stop_decay, Dtu_fix_floor_push, Dtu_fix_look_return, Dtu_fix_fly_bob, Dtu_fix_turn_ramp, Dtu_fix_jump_exit);

	fprintf(Move_log,"MOVE %d t %.4f pos %.4f %.4f %.4f seg %d vel %.4f %.4f %.4f floor %.4f state %02x mode %d pitch %d\n",
		frame, f2fl(GameTime),
		f2fl(obj->pos.x), f2fl(obj->pos.y), f2fl(obj->pos.z), obj->segnum,
		f2fl(obj->mtype.phys_info.velocity.x), f2fl(obj->mtype.phys_info.velocity.y), f2fl(obj->mtype.phys_info.velocity.z),
		f2fl(Dtu_floor_dist), Dtu_player_state, Dtu_move_mode, Dtu_look_pitch);
	fflush(Move_log);
}

// Reads the keyboard, the mouse and the joystick for the movement of the
// player. The keys are the default keys of the manual of the game:
//   8 or up, 2 or down       forward, backward
//   4 or left, 6 or right    turn (side step in the side step mode)
//   Z, X                     side step left, right
//   Alt                      side step mode on and off
//   left Shift               run.  Caps Lock: run always, on and off
//   Space                    jump
//   Page Up, Page Down       look up, look down
//   Home, End                glance up, glance down (the view returns)
//   Insert, Delete           up, down in flight and in levitation
//   V                        mouse look on and off ("immersive mode")
//   A                        use ("Activate": doors and triggers in front)
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

	if (keyd_pressed[KEY_UP] || keyd_pressed[KEY_PAD8] || (t & TM_FORWARD))	fwd += ft;
	if (keyd_pressed[KEY_DOWN] || keyd_pressed[KEY_PAD2] || (t & TM_BACK))		fwd -= ft;
	if (keyd_pressed[KEY_LEFT] || keyd_pressed[KEY_PAD4] || (t & TM_LEFT))		turn--;
	if (keyd_pressed[KEY_RIGHT] || keyd_pressed[KEY_PAD6] || (t & TM_RIGHT))	turn++;
	if (keyd_pressed[KEY_Z] || (t & TM_SLIDELEFT))		side -= ft;
	if (keyd_pressed[KEY_X] || (t & TM_SLIDERIGHT))		side += ft;

	key = keyd_pressed[KEY_LALT];
	if (key && !alt_last)
		sidestep_mode = !sidestep_mode;
	alt_last = key;

	key = keyd_pressed[KEY_CAPSLOCK];
	if (key && !caps_last)
		autorun = !autorun;
	caps_last = key;

	key = keyd_pressed[KEY_V];
	if (key && !v_last) {
		mouse_look = !mouse_look;
		HUD_init_message(mouse_look ? "Immersive Mode" : "Non-Immersive Mode");
	}
	v_last = key;

	// 0x533DC, as in Descent 1: a turn key adds one eighth of its time to
	// the heading time of the frame before. The turn has its whole rate
	// after 8 frames. The corrected rule: after 8 reference frames.
	if (sidestep_mode || turn == 0)
		turn_time = turn_ramp = 0;
	else if (Dtu_fix_turn_ramp) {
		if ((turn > 0 && turn_ramp < 0) || (turn < 0 && turn_ramp > 0))
			turn_ramp = 0;
		{
			// The mean of the ramp over the frame. The ramp of the executable
			// has steps: its mean is one half step (1/16) above a line.
			fix old_ramp = turn_ramp, mean;

			turn_ramp += turn * fixdiv(ft,8*DTU_REF_FRAMETIME);
			if (turn_ramp > F1_0) turn_ramp = F1_0;
			if (turn_ramp < -F1_0) turn_ramp = -F1_0;
			mean = (old_ramp + turn_ramp)/2 + turn * (F1_0/16);
			if (mean > F1_0) mean = F1_0;
			if (mean < -F1_0) mean = -F1_0;
			turn_time = fixmul(mean,ft);
		}
	}
	else {
		if ((turn > 0 && turn_time < 0) || (turn < 0 && turn_time > 0))
			turn_time = 0;
		turn_time += turn * ft / 8;
		if (turn_time > ft) turn_time = ft;
		if (turn_time < -ft) turn_time = -ft;
	}

	if (sidestep_mode)
		side += turn * ft;
	else
		heading += turn_time;

	// The joystick: left and right turn, forward and back walk. Guess.
	if (joy_present) {
		int jx, jy;

		joy_get_pos(&jx,&jy);
		if (abs(jx) > 16) heading += fixmuldiv(ft,jx,128);
		if (abs(jy) > 16) fwd -= fixmuldiv(ft,jy,128);
	}

	// The mouse in the mouse look mode: left and right turn, up and down
	// look. The scale is a guess.
	if (mouse_look) {
		int dx, dy;

		mouse_get_delta(&dx,&dy);
		heading += fixmuldiv(ft,dx,16);
		look += dy * 32;
	}

	// The look keys: the pitch changes by the key time, as an angle
	// (0x52BF9: time * sensitivity / 8, the sensitivity at 0x13BFEF is 8).
	if (keyd_pressed[KEY_PAGEUP] || (t & TM_LOOKUP))		look -= ft;
	if (keyd_pressed[KEY_PAGEDOWN] || (t & TM_LOOKDOWN))	look += ft;
	if (keyd_pressed[KEY_HOME])	{ look -= ft; glance = 1; }
	if (keyd_pressed[KEY_END])		{ look += ft; glance = 1; }

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
		if (keyd_pressed[KEY_INSERT] || (t & TM_UP))		vert += ft;
		if (keyd_pressed[KEY_DELETE] || (t & TM_DOWN))	vert -= ft;
	}

	key = keyd_pressed[KEY_SPACEBAR] || (t & TM_JUMP);
	if (key && !space_last)
		dtu_player_jump(obj);
	space_last = key;

	// The use action: "Activate" of the key table of the executable
	// (entries 39 and 40, default scan code 0x1E at 0x13C037). 0x53D6A
	// calls it in each frame while the key is down, and the action itself
	// waits half a second between two uses.
	if (keyd_pressed[KEY_A] || (t & TM_USE))
		dtu_player_use();

	// The attack with the item in the hand: Left Ctrl and Right Ctrl
	// (keyboard items 18 and 19 of the key table, 0x13C010), on the key
	// down (0x2FCBE). See dtucombat.c.
	dtu_combat_attack_key(keyd_pressed[KEY_LCTRL] || keyd_pressed[KEY_RCTRL] || (t & TM_ATTACK));
	dtu_combat_frame();

	// 0x54033: the move mode. A run is 1.5 times the walk.
	run = keyd_pressed[KEY_LSHIFT] || autorun || (t & TM_RUN);
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
		else if (Dtu_player_state & DTU_PS_CLIMBING)
			vm_vec_zero(&dir);		// the climb of a thief is not in the port
		else if (!(obj->dtu.m_88 & DTU_OF_LEVITATE))
			vm_vec_scale_add(&dir,&obj->orient.fvec,&up,vm_vec_dot(&up,&obj->orient.fvec));
		else {
			dir = obj->orient.fvec;
			dir.y = 0;
			vm_vec_normalize(&dir);
		}
		vm_vec_scale_add2(&pi->thrust,&dir,Controls.forward_thrust_time);
	}

	if (Controls.sideways_thrust_time && !(Dtu_player_state & DTU_PS_CLIMBING)) {
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
}
