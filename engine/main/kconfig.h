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
 * $Source: f:/miner/source/main/rcs/kconfig.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:29:38 $
 * 
 * Prototypes for reading controls
 * 
 * $Log: kconfig.h $
 * Revision 2.0  1995/02/27  11:29:38  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.23  1995/01/12  11:41:44  john
 * Added external control reading.
 * 
 * Revision 1.22  1994/12/07  16:15:30  john
 * Added command to check to see if a joystick axes has been used.
 * 
 * Revision 1.21  1994/11/29  03:46:29  john
 * Added joystick sensitivity; Added sound channels to detail menu.  Removed -maxchannels
 * command line arg.
 * 
 * Revision 1.20  1994/11/22  16:54:45  mike
 * autorepeat on missiles.
 * 
 * Revision 1.19  1994/11/07  14:01:27  john
 * Changed the gamma correction sequencing.
 * 
 * Revision 1.18  1994/11/01  16:40:02  john
 * Added Gamma correction.
 * 
 * Revision 1.17  1994/10/25  23:09:24  john
 * Made the automap key configurable.
 * 
 * Revision 1.16  1994/10/24  19:56:51  john
 * Made the new user setup prompt for config options.
 * 
 * Revision 1.15  1994/10/24  17:44:18  john
 * Added stereo channel reversing.
 * 
 * Revision 1.14  1994/10/22  13:19:33  john
 * Took out toggle primary/secondary weapons.  Fixed black
 * background for 'axes' and 'buttons' text.
 * 
 * Revision 1.13  1994/10/17  13:06:51  john
 * Moved the descent.cfg info into the player config file.
 * 
 * Revision 1.12  1994/10/14  12:14:47  john
 * Changed code so that by doing DEL+F12 saves the current kconfig
 * values as default. Added support for drop_bomb key.  Took out
 * unused slots for keyboard.  Made keyboard use control_type of 0
 * save slots.
 * 
 * Revision 1.11  1994/10/13  19:21:33  john
 * Added separate config saves for different devices.
 * Made all the devices work together better, such as mice won't
 * get read when you're playing with the joystick.
 * 
 * Revision 1.10  1994/10/13  15:18:41  john
 * Started ripping out old afterburner, show message, show automap
 * keys in the keyboard config stuff.
 * 
 * Revision 1.9  1994/10/13  11:35:27  john
 * Made Thrustmaster FCS Hat work.  Put a background behind the
 * keyboard configure.  Took out turn_sensitivity.  Changed sound/config
 * menu to new menu. Made F6 be calibrate joystick.
 * 
 * Revision 1.8  1994/10/06  14:10:50  matt
 * New function reset_cruise()
 * 
 * Revision 1.7  1994/10/03  14:58:25  john
 * Added rear_view_down_state so that the rear view can
 * work like the automap.
 * 
 * Revision 1.6  1994/09/30  12:37:25  john
 * Added midi,digi volume to configuration.
 * 
 * Revision 1.5  1994/09/19  18:49:59  john
 * Added switch to disable joystick.
 * 
 * Revision 1.4  1994/09/15  16:11:21  john
 * Added support for VFX1 head tracking. Fixed bug with memory over-
 * write when using stereo mode.
 * 
 * Revision 1.3  1994/09/12  11:47:38  john
 * Made stupid cruise work better.  Make kconfig values get
 * read/written to disk.
 * 
 * Revision 1.2  1994/09/10  15:46:55  john
 * First version of new keyboard configuration.
 * 
 * Revision 1.1  1994/09/10  13:51:40  john
 * Initial revision
 * 
 * 
 */



#ifndef _KCONFIG_H
#define _KCONFIG_H

typedef struct control_info {
	fix	pitch_time;						
	fix	vertical_thrust_time;
	fix	heading_time;
	fix	sideways_thrust_time;
	fix	bank_time;
	fix	forward_thrust_time;
	
	ubyte	rear_view_down_count;	
	ubyte	rear_view_down_state;	
	
	ubyte	fire_primary_down_count;
	ubyte	fire_primary_state;
	ubyte	fire_secondary_state;
	ubyte	fire_secondary_down_count;
	ubyte	fire_flare_down_count;

	ubyte	drop_bomb_down_count;	

	ubyte	automap_down_count;
	ubyte	automap_state;
		
} control_info;

extern control_info Controls;
extern void controls_read_all();
extern void kconfig(int n, char * title );

extern ubyte Config_digi_volume;
extern ubyte Config_midi_volume;
extern ubyte Config_control_type;
extern ubyte Config_channels_reversed;
extern ubyte Config_joystick_sensitivity;

#define CONTROL_NONE 0
#define CONTROL_JOYSTICK 1
#define CONTROL_FLIGHTSTICK_PRO 2
#define CONTROL_THRUSTMASTER_FCS 3
#define CONTROL_GRAVIS_GAMEPAD 4
#define CONTROL_MOUSE 5
#define CONTROL_CYBERMAN 6
#define CONTROL_MAX_TYPES 7

#define NUM_KEY_CONTROLS 46
#define NUM_OTHER_CONTROLS 27
#define MAX_CONTROLS 50

extern ubyte kconfig_settings[CONTROL_MAX_TYPES][MAX_CONTROLS];
extern ubyte default_kconfig_settings[CONTROL_MAX_TYPES][MAX_CONTROLS];

extern char *control_text[CONTROL_MAX_TYPES];

extern void kc_set_controls();

// Tries to use vfx1 head tracking.
void kconfig_sense_init();

//set the cruise speed to zero
extern void reset_cruise(void);

extern int kconfig_is_axes_used(int axis);

extern void kconfig_init_external_controls(int intno, int address);

// ASCENT: the key table of Descent to Undermountain (kconfig.c, see
// docs/re/controls.md). The actions of the keyboard page in the order of
// the items of the executable (two items each), then the hot keys of the
// port.
enum {
	DTU_ACT_AUTO_RUN, DTU_ACT_JUMP, DTU_ACT_TURN_LEFT, DTU_ACT_TURN_RIGHT,
	DTU_ACT_SLIDE_ON, DTU_ACT_SLIDE_LEFT, DTU_ACT_SLIDE_RIGHT, DTU_ACT_FLOAT_UP,
	DTU_ACT_FLOAT_DOWN, DTU_ACT_ATTACK, DTU_ACT_IMMERSIVE, DTU_ACT_FORWARD,
	DTU_ACT_BACK, DTU_ACT_CYCLE_WEAPON, DTU_ACT_LOOK_UP, DTU_ACT_LOOK_DOWN,
	DTU_ACT_QUICK_UP, DTU_ACT_QUICK_DOWN, DTU_ACT_AUTOMAP, DTU_ACT_ACTIVATE,
	DTU_ACT_RUN, DTU_ACT_INVENTORY, DTU_ACT_STATS, DTU_ACT_INFRAVISION,
	DTU_ACT_DISARM, DTU_ACT_TURN_UNDEAD, DTU_ACT_MOVE_SILENTLY, DTU_ACT_FIND_TRAPS,
	DTU_ACT_OPEN_LOCKS, DTU_ACT_REST,
	DTU_ACT_HOT_KEY_1,
	// the port: the previous and the next hot key (the gamepad shoulders)
	DTU_ACT_PREV_HOT_KEY = DTU_ACT_HOT_KEY_1 + 10, DTU_ACT_NEXT_HOT_KEY,
	// the port: Climb Wall, the keyboard items 20 and 21 ("(unused)" in
	// 1.3). 1.3 climbs with Activate (0x951DE). At the end, so the
	// PadConfig of an older ascent.cfg stays valid.
	DTU_ACT_CLIMB,
	// the port: Crouch (Dtu_crouch, dtuopts.h), the keyboard items 62 and
	// 63 (bytes 62 and 63 of the row, unused in 1.3), a row below the page
	DTU_ACT_CROUCH,
	DTU_N_ACTIONS
};
#define DTU_N_HOT_ACTIONS	10

// ASCENT: Climb Wall is a key of its own (1, default): Activate takes hold
// of a ladder but does not climb a wall, so a lever or a button gives no
// climb message. 1.3 climbs with Activate (0x95050 calls 0x951DE for each
// side that is no door). -origui clears it: Activate climbs too.
extern int Dtu_fix_climb_key;

extern int Dtu_control_type;		// 1 to 4 a joystick, 5 the mouse
int dtu_action_key(int act, int which);			// scan code of key 0 or 1, 255 for none
int dtu_action_has_key(int act, int scancode);
int dtu_action_down(int act);						// a key or a button of the action is down
int dtu_action_presses(int act);					// presses since the last call (key_down_count)
char *dtu_action_name(int act);
char *dtu_action_key_names(int act, char *buf, int size, char *sep);
int dtu_hot_keys_are_default(void);
int dtu_control_axes(int *look, int *turn, int *slide, int *updown);	// 0 none, 1 mouse, 2 joystick
void dtu_kconfig(int page, char *title);			// 0 keyboard, 1 joystick, 2 mouse, 3 hot keys
void dtu_kconfig_menu(void);						// the Controls menu
void dtu_kconfig_defaults(void);
void dtu_kconfig_args(void);						// the test option -keyconfig
// The gamepad (port): the amount of an action from a stick or a trigger
// (0 to F1_0, after the dead zone; the turn and the look with the speed
// setting). dtu_action_down() has the buttons and, for the actions that
// are not analog, a stick or trigger past one half.
fix dtu_pad_amount(int act);
void dtu_pad_frame(void);							// once per game frame
void dtu_pad_menu(void);							// the Customize Gamepad menu
#include <stdio.h>
int dtu_kconfig_cfg_line(const char *name, const char *value);
void dtu_kconfig_cfg_write(FILE *f);
void dtu_kconfig_log(void);			// ASCENT: the control settings in the session log


#endif
