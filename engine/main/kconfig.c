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
 * $Source: f:/miner/source/main/rcs/kconfig.c $
 * $Revision: 2.11 $
 * $Author: john $
 * $Date: 1995/08/23 16:08:04 $
 * 
 * Routines to configure keyboard, joystick, etc..
 * 
 * $Log: kconfig.c $
 * Revision 2.11  1995/08/23  16:08:04  john
 * Added version 2 of external controls that passes the ship
 * position and orientation the drivers.
 * 
 * Revision 2.10  1995/07/07  16:48:01  john
 * Fixed bug with new interface.
 * 
 * Revision 2.9  1995/07/03  15:02:32  john
 * Added new version of external controls for Cybermouse absolute position.
 * 
 * Revision 2.8  1995/06/30  12:30:28  john
 * Added -Xname command line.
 * 
 * Revision 2.7  1995/03/30  16:36:56  mike
 * text localization.
 * 
 * Revision 2.6  1995/03/21  14:39:31  john
 * Ifdef'd out the NETWORK code.
 * 
 * Revision 2.5  1995/03/16  10:53:07  john
 * Move VFX center to Shift+Z instead of Enter because
 * it conflicted with toggling HUD on/off.
 * 
 * Revision 2.4  1995/03/10  13:47:24  john
 * Added head tracking sensitivity.
 * 
 * Revision 2.3  1995/03/09  18:07:06  john
 * Fixed bug with iglasses tracking not "centering" right.
 * Made VFX have bright headlight lighting.
 * 
 * Revision 2.2  1995/03/08  15:32:39  john
 * Made VictorMaxx head tracking use Greenleaf code.
 * 
 * Revision 2.1  1995/03/06  15:23:31  john
 * New screen techniques.
 * 
 * Revision 2.0  1995/02/27  11:29:26  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.105  1995/02/22  14:11:58  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.104  1995/02/13  12:01:56  john
 * Fixed bug with buggin not mmaking player faster.
 * 
 * Revision 1.103  1995/02/09  22:00:46  john
 * Added i-glasses tracking.
 * 
 * Revision 1.102  1995/01/24  21:25:47  john
 * Fixed bug with slide/bank on not working with
 * Cyberman heading.,
 * 
 * Revision 1.101  1995/01/24  16:09:56  john
 * Fixed bug with Wingman extreme customize text overwriting title.
 * 
 * Revision 1.100  1995/01/24  12:37:46  john
 * Made Esc exit key define menu.
 * 
 * Revision 1.99  1995/01/23  23:54:43  matt
 * Made keypad enter work
 * 
 * Revision 1.98  1995/01/23  16:42:00  john
 * Made the external controls always turn banking off, leveling off
 * and passed automap state thru to the tsr.
 * 
 * Revision 1.97  1995/01/12  11:41:33  john
 * Added external control reading.
 * 
 * Revision 1.96  1995/01/05  10:43:58  mike
 * Handle case when timer_get_fixed_seconds() goes negative.  Happens at 9.1
 * hours.  Previously, joystick would stop functioning.  Now will work.
 * 
 * Revision 1.95  1994/12/29  11:17:38  john
 * Took out some warnings and mprintf.
 * 
 * Revision 1.94  1994/12/29  11:07:41  john
 * Fixed Thrustmaster and Logitech Wingman extreme
 * Hat by reading the y2 axis during the center stage
 * of the calibration, and using 75, 50, 27, and 3 %
 * as values for the 4 positions.
 * 
 * Revision 1.93  1994/12/27  12:16:20  john
 * Fixed bug with slide on not working with joystick or mouse buttons.
 * 
 * Revision 1.92  1994/12/20  10:34:15  john
 * Made sensitivity work for mouse & joystick and made
 * it only affect, pitch, heading, and roll.
 * 
 * Revision 1.91  1994/12/16  00:11:23  matt
 * Made delete key act normally when debug out
 * 
 * Revision 1.90  1994/12/14  17:41:15  john
 * Added more buttons so that  Yoke would work.
 * 
 * Revision 1.89  1994/12/13  17:25:35  allender
 * Added Assert for bogus time for joystick reading.
 * 
 * Revision 1.88  1994/12/13  14:48:01  john
 * Took out some debugging mprintf's
 * 
 * 
 * Revision 1.87  1994/12/13  14:43:02  john
 * Took out the code in kconfig to build direction array.
 * Called kc_set_controls after selecting a new control type.
 * 
 * Revision 1.86  1994/12/13  01:11:32  john
 * Fixed bug with message clearing overwriting 
 * right border.
 * 
 * Revision 1.85  1994/12/12  00:35:58  john
 * Added or thing for keys.
 * 
 * Revision 1.84  1994/12/09  17:08:06  john
 * Made mouse a bit less sensitive.
 * 
 * Revision 1.83  1994/12/09  16:04:00  john
 * Increased mouse sensitivity.
 * 
 * Revision 1.82  1994/12/09  00:41:26  mike
 * fix hang in automap print screen
 * 
 * Revision 1.81  1994/12/08  11:50:37  john
 * Made strcpy only copy corect number of chars,.
 * 
 * Revision 1.80  1994/12/07  16:16:06  john
 * Added command to check to see if a joystick axes has been used.
 * 
 * Revision 1.79  1994/12/07  14:52:28  yuan
 * Localization 492
 * 
 * Revision 1.78  1994/12/07  13:37:40  john
 * Made the joystick thrust work in reverse.
 * 
 * Revision 1.77  1994/12/07  11:28:24  matt
 * Did a little localization support
 * 
 * Revision 1.76  1994/12/04  12:30:03  john
 * Made the Thrustmaster stick read every frame, not every 10 frames,
 * because it uses analog axis as buttons.
 * 
 * Revision 1.75  1994/12/03  22:35:25  yuan
 * Localization 412
 * 
 * Revision 1.74  1994/12/03  15:39:24  john
 * Made numeric keypad move in conifg.
 * 
 * Revision 1.73  1994/12/01  16:23:39  john
 * Fixed include mistake.
 * 
 * Revision 1.72  1994/12/01  16:07:57  john
 * Fixed bug that disabled joystick in automap because it used gametime, which is
 * paused during automap. Fixed be used timer_Get_fixed_seconds instead of GameTime.
 * 
 * Revision 1.71  1994/12/01  12:30:49  john
 * Made Ctrl+D delete, not Ctrl+E
 * 
 * Revision 1.70  1994/12/01  11:52:52  john
 * Added default values for GamePad.
 * 
 * Revision 1.69  1994/11/30  00:59:12  mike
 * optimizations.
 * 
 * Revision 1.68  1994/11/29  03:45:50  john
 * Added joystick sensitivity; Added sound channels to detail menu.  Removed -maxchannels
 * command line arg.
 * 
 * Revision 1.67  1994/11/27  23:13:44  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.66  1994/11/27  19:52:12  matt
 * Made screen shots work in a few more places
 * 
 * Revision 1.65  1994/11/22  16:54:50  mike
 * autorepeat on missiles.
 * 
 * Revision 1.64  1994/11/21  11:16:17  rob
 * Changed calls to GameLoop to calls to multi_menu_poll and changed
 * conditions under which they are called.
 * 
 * Revision 1.63  1994/11/19  15:14:48  mike
 * remove unused code and data
 * 
 * Revision 1.62  1994/11/18  23:37:56  john
 * Changed some shorts to ints.
 * 
 * Revision 1.61  1994/11/17  13:36:35  rob
 * Added better network hook in kconfig menu.
 * 
 * Revision 1.60  1994/11/14  20:09:13  john
 * Made Tab be default for automap.
 * 
 * Revision 1.59  1994/11/13  16:34:07  matt
 * Fixed victormaxx angle conversions
 * 
 * Revision 1.58  1994/11/12  14:47:05  john
 * Added support for victor head tracking.
 * 
 * Revision 1.57  1994/11/08  15:14:55  john
 * Added more calls so net doesn't die in net game.
 * 
 * Revision 1.56  1994/11/07  14:01:07  john
 * Changed the gamma correction sequencing.
 * 
 * Revision 1.55  1994/11/01  16:40:08  john
 * Added Gamma correction.
 * 
 * Revision 1.54  1994/10/25  23:09:26  john
 * Made the automap key configurable.
 * 
 * Revision 1.53  1994/10/25  13:11:59  john
 * Made keys the way Adam speced 'em for final game.
 * 
 * Revision 1.52  1994/10/24  17:44:22  john
 * Added stereo channel reversing.
 * 
 * Revision 1.51  1994/10/22  13:23:18  john
 * Made default rear view key be R.
 * 
 * Revision 1.50  1994/10/22  13:20:09  john
 * Took out toggle primary/secondary weapons.  Fixed black
 * background for 'axes' and 'buttons' text.
 * 
 * Revision 1.49  1994/10/21  15:20:15  john
 * Made PrtScr do screen dump, not F2.
 * 
 * Revision 1.48  1994/10/21  13:41:36  john
 * Allowed F2 to screen dump.
 * 
 * Revision 1.47  1994/10/17  13:07:05  john
 * Moved the descent.cfg info into the player config file.
 * 
 * Revision 1.46  1994/10/14  15:30:22  john
 * Added Cyberman default positions.
 * 
 * Revision 1.45  1994/10/14  15:24:54  john
 * Made Cyberman work with config.
 * 
 * Revision 1.44  1994/10/14  12:46:04  john
 * Added the ability to reset all to default.
 * 
 * Revision 1.43  1994/10/14  12:18:31  john
 * Made mouse invert axis always be 0 or 1.
 * 
 * Revision 1.42  1994/10/14  12:16:03  john
 * Changed code so that by doing DEL+F12 saves the current kconfig
 * values as default. Added support for drop_bomb key.  Took out
 * unused slots for keyboard.  Made keyboard use control_type of 0
 * save slots.
 * 
 * Revision 1.41  1994/10/13  21:27:02  john
 * Made axis invert value always be 0 or 1.
n * 
 * Revision 1.40  1994/10/13  20:18:15  john
 * Added some more system keys, such as F? and CAPSLOCK.
 * 
 * Revision 1.39  1994/10/13  19:22:29  john
 * Added separate config saves for different devices.
 * Made all the devices work together better, such as mice won't
 * get read when you're playing with the joystick.
 * 
 * Revision 1.38  1994/10/13  15:41:57  mike
 * Remove afterburner.
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: kconfig.c 2.11 1995/08/23 16:08:04 john Exp $";
#pragma on (unreferenced)

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// ASCENT: removed dos.h and io.h (DOS only headers).
#include <stdarg.h>
#include <ctype.h>

#include "error.h"
#include "types.h"
#include "gr.h"
#include "mono.h"
#include "key.h"
#include "palette.h"
#include "game.h"
#include "gamefont.h"
#include "iff.h"
#include "mem.h"
#include "joy.h"
#include "mouse.h"
#include "kconfig.h"
#include "gauges.h"
#include "joydefs.h"
// ASCENT: removed vfx1.h, victor.h and iglasses.h. The virtual reality
// headsets are not part of the port. arcade.h stays: without the ARCADE
// macro it turns the arcade code into nothing.
#include "arcade.h"
#include "render.h"
#include "digi.h"
#include "newmenu.h"
#include "endlevel.h"
#include "multi.h"
#include "timer.h"
#include "text.h"
#include "player.h"
#include "menu.h"
#include "args.h"
#include "platform.h"		// ASCENT: plat_pump reports that the window is closed

//#define TABLE_CREATION 1

int     sense_function1=0;
int	  vfx1_installed=0;
int     SenseStatus1( void );

// ASCENT: the characters above 126 in the key names of this file are
// written as octal escapes, so that the file is plain ASCII.

// ASCENT: playsave.c reads and writes kconfig_settings as one block of
// bytes. The array has only bytes, so it has no padding.
_Static_assert( sizeof(kconfig_settings) == CONTROL_MAX_TYPES*MAX_CONTROLS, "kconfig_settings layout" );

// Array used to 'blink' the cursor while waiting for a keypress.
byte fades[64] = { 1,1,1,2,2,3,4,4,5,6,8,9,10,12,13,15,16,17,19,20,22,23,24,26,27,28,28,29,30,30,31,31,31,31,31,30,30,29,28,28,27,26,24,23,22,20,19,17,16,15,13,12,10,9,8,6,5,4,4,3,2,2,1,1 };

//char * invert_text[2] = { "N", "Y" };
//char * joybutton_text[28] = { "BTN 1", "BTN 2", "BTN 3", "BTN 4", "", "TRIG", "LEFT", "HAT ?", "RIGHT", "", "", "HAT ?", "MID", "", "", "HAT ?", "", "", "", "HAT ?", "TRIG", "LEFT", "RIGHT", "", "UP","DOWN","LEFT", "RIGHT" };
//char * joyaxis_text[4] = { "X1", "Y1", "X2", "Y2" };
//char * mouseaxis_text[2] = { "L/R", "F/B" };
//char * mousebutton_text[3] = { "Left", "Right", "Mid" };

int invert_text[2] = { TNUM_N, TNUM_Y };
int joybutton_text[28] = 
{ TNUM_BTN_1, TNUM_BTN_2, TNUM_BTN_3, TNUM_BTN_4,
  -1, TNUM_TRIG, TNUM_LEFT, TNUM_HAT_L,
 TNUM_RIGHT, -1, TNUM_HAT2_D, TNUM_HAT_R,
 TNUM_MID, -1, TNUM_HAT2_R, TNUM_HAT_U,
 TNUM_HAT2_L, -1, TNUM_HAT2_U, TNUM_HAT_D,
 TNUM_TRIG, TNUM_LEFT, TNUM_RIGHT, -1, 
  TNUM_UP, TNUM_DOWN, TNUM_LEFT, TNUM_RIGHT };
int joyaxis_text[4] = { TNUM_X1, TNUM_Y1, TNUM_X2, TNUM_Y2 };
int mouseaxis_text[2] = { TNUM_L_R, TNUM_F_B };
int mousebutton_text[3] = { TNUM_LEFT, TNUM_RIGHT, TNUM_MID };

char * key_text[256] = {         \
"","ESC","1","2","3","4","5","6","7","8","9","0","-", 			\
"=","BSPC","TAB","Q","W","E","R","T","Y","U","I","O",				\
"P","[","]","\203","LCTRL","A","S","D","F",        \
"G","H","J","K","L",";","'","`",        \
"LSHFT","\\","Z","X","C","V","B","N","M",",",      \
".","/","RSHFT","PAD*","LALT","SPC",      \
"CPSLK","F1","F2","F3","F4","F5","F6","F7","F8","F9",        \
"F10","NMLCK","SCLK","PAD7","PAD8","PAD9","PAD-",   \
"PAD4","PAD5","PAD6","PAD+","PAD1","PAD2","PAD3","PAD0", \
"PAD.","","","","F11","F12","","","","","","","","","",         \
"","","","","","","","","","","","","","","","","","","","",     \
"","","","","","","","","","","","","","","","","","","","",     \
"","","","","","","","","","","","","","","","","","",           \
"PAD\203","RCTRL","","","","","","","","","","","","","", \
"","","","","","","","","","","PAD/","","","RALT","",      \
"","","","","","","","","","","","","","HOME","\202","PGUP",     \
"","\201","","\177","","END","\200","PGDN","INS",       \
"DEL","","","","","","","","","","","","","","","","","",     \
"","","","","","","","","","","","","","","","","","","","",     \
"","","","","","","" };

ubyte system_keys[] = { KEY_ESC, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_PRINT_SCREEN };

//extern void GameLoop(int, int );

control_info Controls;

ubyte Config_digi_volume = 16;
ubyte Config_midi_volume = 16;
ubyte Config_control_type = 0;
ubyte Config_channels_reversed = 0;
ubyte Config_joystick_sensitivity = 8;

fix Cruise_speed=0;

#define BT_KEY 				0
#define BT_MOUSE_BUTTON 	1
#define BT_MOUSE_AXIS		2
#define BT_JOY_BUTTON 		3
#define BT_JOY_AXIS			4
#define BT_INVERT				5

char *btype_text[] = { "BT_KEY", "BT_MOUSE_BUTTON", "BT_MOUSE_AXIS", "BT_JOY_BUTTON", "BT_JOY_AXIS", "BT_INVERT" };

#define INFO_Y 28

typedef struct kc_item {
	short id;				// The id of this item
	short x, y;				
	short w1;
	short w2;
	short u,d,l,r;
	short text_num1;
	ubyte type;
	ubyte value;		// what key,button,etc
} kc_item;

int Num_items=23;
kc_item *All_items;

ubyte kconfig_settings[CONTROL_MAX_TYPES][MAX_CONTROLS];

//----------- WARNING!!!!!!! -------------------------------------------
// THESE NEXT FOUR BLOCKS OF DATA ARE GENERATED BY PRESSING DEL+F12 WHEN
// IN THE KEYBOARD CONFIG SCREEN.  BASICALLY, THAT PROCEDURE MODIFIES THE
// U,D,L,R FIELDS OF THE ARRAYS AND DUMPS THE NEW ARRAYS INTO KCONFIG.COD
//-------------------------------------------------------------------------

ubyte default_kconfig_settings[CONTROL_MAX_TYPES][MAX_CONTROLS] = {
{0xc8,0x48,0xd0,0x50,0xcb,0x4b,0xcd,0x4d,0x38,0xff,0xff,0x4f,0xff,0x51,0xff,0x4a,0xff,0x4e,0xff,0xff,0x10,0x47,0x12,0x49,0x1d,0x9d,0x39,0xff,0x21,0xff,0x1e,0xff,0x2c,0xff,0x30,0xff,0x13,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xf,0xff,0xff,0xff,0xff,0xff},
{0x0,0x1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x1,0x0,0x0,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
{0x5,0xc,0xff,0xff,0xff,0xff,0x7,0xf,0x13,0xb,0xff,0x6,0x8,0x1,0x0,0x0,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
{0x0,0x1,0xff,0xff,0x2,0xff,0x7,0xf,0x13,0xb,0xff,0xff,0xff,0x1,0x0,0x0,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x3,0xff,0x3,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
{0x3,0x0,0x1,0x2,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x1,0x0,0x0,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
{0x0,0x1,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x1,0x0,0x0,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
{0x0,0x1,0xff,0xff,0x2,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0x0,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
};

kc_item kc_keyboard[NUM_KEY_CONTROLS] = {
	{  0, 15, 49, 71, 26, 43,  2, 23,  1,TNUM_PITCH_FORWARD, BT_KEY, 255 },
	{  1, 15, 49,100, 26, 22,  3,  0, 24,TNUM_PITCH_FORWARD, BT_KEY, 255 },
	{  2, 15, 57, 71, 26,  0,  4, 25,  3,TNUM_PITCH_BACKWARD, BT_KEY, 255 },
	{  3, 15, 57,100, 26,  1,  5,  2, 26,TNUM_PITCH_BACKWARD, BT_KEY, 255 },
	{  4, 15, 65, 71, 26,  2,  6, 27,  5,TNUM_TURN_LEFT, BT_KEY, 255 },
	{  5, 15, 65,100, 26,  3,  7,  4, 28,TNUM_TURN_LEFT, BT_KEY, 255 },
	{  6, 15, 73, 71, 26,  4,  8, 29,  7,TNUM_TURN_RIGHT, BT_KEY, 255 },
	{  7, 15, 73,100, 26,  5,  9,  6, 34,TNUM_TURN_RIGHT, BT_KEY, 255 },
	{  8, 15, 85, 71, 26,  6, 10, 35,  9,TNUM_SLIDE_ON, BT_KEY, 255 },
	{  9, 15, 85,100, 26,  7, 11,  8, 36,TNUM_SLIDE_ON, BT_KEY, 255 },
	{ 10, 15, 93, 71, 26,  8, 12, 37, 11,TNUM_SLIDE_LEFT, BT_KEY, 255 },
	{ 11, 15, 93,100, 26,  9, 13, 10, 44,TNUM_SLIDE_LEFT, BT_KEY, 255 },
	{ 12, 15,101, 71, 26, 10, 14, 45, 13,TNUM_SLIDE_RIGHT, BT_KEY, 255 },
	{ 13, 15,101,100, 26, 11, 15, 12, 30,TNUM_SLIDE_RIGHT, BT_KEY, 255 },
	{ 14, 15,109, 71, 26, 12, 16, 31, 15,TNUM_SLIDE_UP, BT_KEY, 255 },
	{ 15, 15,109,100, 26, 13, 17, 14, 32,TNUM_SLIDE_UP, BT_KEY, 255 },
	{ 16, 15,117, 71, 26, 14, 18, 33, 17,TNUM_SLIDE_DOWN, BT_KEY, 255 },
	{ 17, 15,117,100, 26, 15, 19, 16, 38,TNUM_SLIDE_DOWN, BT_KEY, 255 },
	{ 18, 15,129, 71, 26, 16, 20, 39, 19,TNUM_BANK_ON, BT_KEY, 255 },
	{ 19, 15,129,100, 26, 17, 21, 18, 40,TNUM_BANK_ON, BT_KEY, 255 },
	{ 20, 15,137, 71, 26, 18, 22, 41, 21,TNUM_BANK_LEFT, BT_KEY, 255 },
	{ 21, 15,137,100, 26, 19, 23, 20, 42,TNUM_BANK_LEFT, BT_KEY, 255 },
	{ 22, 15,145, 71, 26, 20,  1, 43, 23,TNUM_BANK_RIGHT, BT_KEY, 255 },
	{ 23, 15,145,100, 26, 21, 24, 22,  0,TNUM_BANK_RIGHT, BT_KEY, 255 },
	{ 24,158, 49, 83, 26, 23, 26,  1, 25,TNUM_FIRE_PRIMARY, BT_KEY, 255 },
	{ 25,158, 49,112, 26, 42, 27, 24,  2,TNUM_FIRE_PRIMARY, BT_KEY, 255 },
	{ 26,158, 57, 83, 26, 24, 28,  3, 27,TNUM_FIRE_SECONDARY, BT_KEY, 255 },
	{ 27,158, 57,112, 26, 25, 29, 26,  4,TNUM_FIRE_SECONDARY, BT_KEY, 255 },
	{ 28,158, 65, 83, 26, 26, 34,  5, 29,TNUM_FIRE_FLARE, BT_KEY, 255 },
	{ 29,158, 65,112, 26, 27, 35, 28,  6,TNUM_FIRE_FLARE, BT_KEY, 255 },
	{ 30,158,105, 83, 26, 44, 32, 13, 31,TNUM_ACCELERATE, BT_KEY, 255 },
	{ 31,158,105,112, 26, 45, 33, 30, 14,TNUM_ACCELERATE, BT_KEY, 255 },
	{ 32,158,113, 83, 26, 30, 38, 15, 33,TNUM_REVERSE, BT_KEY, 255 },
	{ 33,158,113,112, 26, 31, 39, 32, 16,TNUM_REVERSE, BT_KEY, 255 },
	{ 34,158, 73, 83, 26, 28, 36,  7, 35,TNUM_DROP_BOMB, BT_KEY, 255 },
	{ 35,158, 73,112, 26, 29, 37, 34,  8,TNUM_DROP_BOMB, BT_KEY, 255 },
	{ 36,158, 85, 83, 26, 34, 44,  9, 37,TNUM_REAR_VIEW, BT_KEY, 255 },
	{ 37,158, 85,112, 26, 35, 45, 36, 10,TNUM_REAR_VIEW, BT_KEY, 255 },
	{ 38,158,125, 83, 26, 32, 40, 17, 39,TNUM_CRUISE_FASTER, BT_KEY, 255 },
	{ 39,158,125,112, 26, 33, 41, 38, 18,TNUM_CRUISE_FASTER, BT_KEY, 255 },
	{ 40,158,133, 83, 26, 38, 42, 19, 41,TNUM_CRUISE_SLOWER, BT_KEY, 255 },
	{ 41,158,133,112, 26, 39, 43, 40, 20,TNUM_CRUISE_SLOWER, BT_KEY, 255 },
	{ 42,158,141, 83, 26, 40, 25, 21, 43,TNUM_CRUISE_OFF, BT_KEY, 255 },
	{ 43,158,141,112, 26, 41,  0, 42, 22,TNUM_CRUISE_OFF, BT_KEY, 255 },
	{ 44,158, 93, 83, 26, 36, 30, 11, 45,TNUM_AUTOMAP, BT_KEY, 255 },
	{ 45,158, 93,112, 26, 37, 31, 44, 12,TNUM_AUTOMAP, BT_KEY, 255 },
};
kc_item kc_joystick[NUM_OTHER_CONTROLS] = {
	{  0, 25, 46, 85, 26, 15,  1, 24,  5,TNUM_FIRE_PRIMARY, BT_JOY_BUTTON, 255 },
	{  1, 25, 54, 85, 26,  0,  4,  5,  6,TNUM_FIRE_SECONDARY, BT_JOY_BUTTON, 255 },
	{  2, 25, 85, 85, 26, 26,  3,  9, 10,TNUM_ACCELERATE, BT_JOY_BUTTON, 255 },
	{  3, 25, 93, 85, 26,  2, 25, 10, 11,TNUM_REVERSE, BT_JOY_BUTTON, 255 },
	{  4, 25, 62, 85, 26,  1, 26,  6,  7,TNUM_FIRE_FLARE, BT_JOY_BUTTON, 255 },
	{  5,180, 46, 59, 26, 23,  6,  0,  1,TNUM_SLIDE_ON, BT_JOY_BUTTON, 255 },
	{  6,180, 54, 59, 26,  5,  7,  1,  4,TNUM_SLIDE_LEFT, BT_JOY_BUTTON, 255 },
	{  7,180, 62, 59, 26,  6,  8,  4, 26,TNUM_SLIDE_RIGHT, BT_JOY_BUTTON, 255 },
	{  8,180, 70, 59, 26,  7,  9, 26,  9,TNUM_SLIDE_UP, BT_JOY_BUTTON, 255 },
	{  9,180, 78, 59, 26,  8, 10,  8,  2,TNUM_SLIDE_DOWN, BT_JOY_BUTTON, 255 },
	{ 10,180, 90, 59, 26,  9, 11,  2,  3,TNUM_BANK_ON, BT_JOY_BUTTON, 255 },
	{ 11,180, 98, 59, 26, 10, 12,  3, 12,TNUM_BANK_LEFT, BT_JOY_BUTTON, 255 },
	{ 12,180,106, 59, 26, 11, 18, 11, 25,TNUM_BANK_RIGHT, BT_JOY_BUTTON, 255 },
	{ 13, 22,146, 51, 26, 24, 15, 25, 14,TNUM_PITCH_UD, BT_JOY_AXIS, 255 },
	{ 14, 22,146, 99,  8, 25, 16, 13, 17,TNUM_PITCH_UD, BT_INVERT, 255 },
	{ 15, 22,154, 51, 26, 13,  0, 18, 16,TNUM_TURN_LR, BT_JOY_AXIS, 255 },
	{ 16, 22,154, 99,  8, 14, 17, 15, 19,TNUM_TURN_LR, BT_INVERT, 255 },
	{ 17,164,146, 58, 26, 16, 19, 14, 18,TNUM_SLIDE_LR, BT_JOY_AXIS, 255 },
	{ 18,164,146,106,  8, 12, 20, 17, 15,TNUM_SLIDE_LR, BT_INVERT, 255 },
	{ 19,164,154, 58, 26, 17, 21, 16, 20,TNUM_SLIDE_UD, BT_JOY_AXIS, 255 },
	{ 20,164,154,106,  8, 18, 22, 19, 21,TNUM_SLIDE_UD, BT_INVERT, 255 },
	{ 21,164,162, 58, 26, 19, 23, 20, 22,TNUM_BANK_LR, BT_JOY_AXIS, 255 },
	{ 22,164,162,106,  8, 20, 24, 21, 23,TNUM_BANK_LR, BT_INVERT, 255 },
	{ 23,164,174, 58, 26, 21,  5, 22, 24,TNUM_THROTTLE, BT_JOY_AXIS, 255 },
	{ 24,164,174,106,  8, 22, 13, 23,  0,TNUM_THROTTLE, BT_INVERT, 255 },
	{ 25, 25,109, 85, 26,  3, 14, 12, 13,TNUM_REAR_VIEW, BT_JOY_BUTTON, 255 },
	{ 26, 25, 70, 85, 26,  4,  2,  7,  8,TNUM_DROP_BOMB, BT_JOY_BUTTON, 255 },
};
kc_item kc_mouse[NUM_OTHER_CONTROLS] = {
	{  0, 25, 46, 85, 26, 12,  1, 24,  5,TNUM_FIRE_PRIMARY, BT_MOUSE_BUTTON, 255 },
	{  1, 25, 54, 85, 26,  0,  4,  5,  6,TNUM_FIRE_SECONDARY, BT_MOUSE_BUTTON, 255 },
	{  2, 25, 85, 85, 26, 26,  3,  9, 10,TNUM_ACCELERATE, BT_MOUSE_BUTTON, 255 },
	{  3, 25, 93, 85, 26,  2, 25, 10, 11,TNUM_REVERSE, BT_MOUSE_BUTTON, 255 },
	{  4, 25, 62, 85, 26,  1, 26,  6,  7,TNUM_FIRE_FLARE, BT_MOUSE_BUTTON, 255 },
	{  5,180, 46, 59, 26, 24,  6,  0,  1,TNUM_SLIDE_ON, BT_MOUSE_BUTTON, 255 },
	{  6,180, 54, 59, 26,  5,  7,  1,  4,TNUM_SLIDE_LEFT, BT_MOUSE_BUTTON, 255 },
	{  7,180, 62, 59, 26,  6,  8,  4, 26,TNUM_SLIDE_RIGHT, BT_MOUSE_BUTTON, 255 },
	{  8,180, 70, 59, 26,  7,  9, 26,  9,TNUM_SLIDE_UP, BT_MOUSE_BUTTON, 255 },
	{  9,180, 78, 59, 26,  8, 10,  8,  2,TNUM_SLIDE_DOWN, BT_MOUSE_BUTTON, 255 },
	{ 10,180, 90, 59, 26,  9, 11,  2,  3,TNUM_BANK_ON, BT_MOUSE_BUTTON, 255 },
	{ 11,180, 98, 59, 26, 10, 12,  3, 12,TNUM_BANK_LEFT, BT_MOUSE_BUTTON, 255 },
	{ 12,180,106, 59, 26, 11,  0, 11, 25,TNUM_BANK_RIGHT, BT_MOUSE_BUTTON, 255 },
	{ 13,103,138, 58, 26, 25, 15, 25, 14,TNUM_PITCH_UD, BT_MOUSE_AXIS, 255 },
	{ 14,103,138,106,  8, 23, 16, 13, 15,TNUM_PITCH_UD, BT_INVERT, 255 },
	{ 15,103,146, 58, 26, 13, 17, 14, 16,TNUM_TURN_LR, BT_MOUSE_AXIS, 255 },
	{ 16,103,146,106,  8, 14, 18, 15, 17,TNUM_TURN_LR, BT_INVERT, 255 },
	{ 17,103,154, 58, 26, 15, 19, 16, 18,TNUM_SLIDE_LR, BT_MOUSE_AXIS, 255 },
	{ 18,103,154,106,  8, 16, 20, 17, 19,TNUM_SLIDE_LR, BT_INVERT, 255 },
	{ 19,103,162, 58, 26, 17, 21, 18, 20,TNUM_SLIDE_UD, BT_MOUSE_AXIS, 255 },
	{ 20,103,162,106,  8, 18, 22, 19, 21,TNUM_SLIDE_UD, BT_INVERT, 255 },
	{ 21,103,170, 58, 26, 19, 23, 20, 22,TNUM_BANK_LR, BT_MOUSE_AXIS, 255 },
	{ 22,103,170,106,  8, 20, 24, 21, 23,TNUM_BANK_LR, BT_INVERT, 255 },
	{ 23,103,182, 58, 26, 21, 14, 22, 24,TNUM_THROTTLE, BT_MOUSE_AXIS, 255 },
	{ 24,103,182,106,  8, 22,  5, 23,  0,TNUM_THROTTLE, BT_INVERT, 255 },
	{ 25, 25,109, 85, 26,  3, 13, 12, 13,TNUM_REAR_VIEW, BT_MOUSE_BUTTON, 255 },
	{ 26, 25, 70, 85, 26,  4,  2,  7,  8,TNUM_DROP_BOMB, BT_MOUSE_BUTTON, 255 },
};

int kconfig_is_axes_used(int axis)
{
	int i;
	for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
		if (( kc_joystick[i].type == BT_JOY_AXIS ) && (kc_joystick[i].value == axis ))
			return 1;
	}
	return 0;
}

#ifdef TABLE_CREATION
int find_item_at( kc_item * items, int nitems, int x, int y )
{
	int i;
	
	for (i=0; i<nitems; i++ )	{
		if ( ((items[i].x+items[i].w1)==x) && (items[i].y==y))
			return i;
	}
	return -1;
}

int find_next_item_up( kc_item * items, int nitems, int citem )
{
	int x, y, i;

	y = items[citem].y;
	x = items[citem].x+items[citem].w1;
	
	do {	
		y--;
		if ( y < 0 ) {
			y = grd_curcanv->cv_bitmap.bm_h-1;
			x--;
			if ( x < 0 ) {
				x = grd_curcanv->cv_bitmap.bm_w-1;
			}
		}
		i = find_item_at( items, nitems, x, y );
	} while ( i < 0 );
	
	return i;
}

int find_next_item_down( kc_item * items, int nitems, int citem )
{
	int x, y, i;

	y = items[citem].y;
	x = items[citem].x+items[citem].w1;
	
	do {	
		y++;
		if ( y > grd_curcanv->cv_bitmap.bm_h-1 ) {
			y = 0;
			x++;
			if ( x > grd_curcanv->cv_bitmap.bm_w-1 ) {
				x = 0;
			}
		}
		i = find_item_at( items, nitems, x, y );
	} while ( i < 0 );
	
	return i;
}

int find_next_item_right( kc_item * items, int nitems, int citem )
{
	int x, y, i;

	y = items[citem].y;
	x = items[citem].x+items[citem].w1;
	
	do {	
		x++;
		if ( x > grd_curcanv->cv_bitmap.bm_w-1 ) {
			x = 0;
			y++;
			if ( y > grd_curcanv->cv_bitmap.bm_h-1 ) {
				y = 0;
			}
		}
		i = find_item_at( items, nitems, x, y );
	} while ( i < 0 );
	
	return i;
}

int find_next_item_left( kc_item * items, int nitems, int citem )
{
	int x, y, i;

	y = items[citem].y;
	x = items[citem].x+items[citem].w1;
	
	do {	
		x--;
		if ( x < 0 ) {
			x = grd_curcanv->cv_bitmap.bm_w-1;
			y--;
			if ( y < 0 ) {
				y = grd_curcanv->cv_bitmap.bm_h-1;
			}
		}
		i = find_item_at( items, nitems, x, y );
	} while ( i < 0 );
	
	return i;
}
#endif



// ASCENT: prototypes for functions that this file calls before their
// definition. Watcom C accepted the implicit declarations.
struct kc_item;
void kc_drawitem( struct kc_item *item, int is_current );
void kc_change_key( struct kc_item * item );
void kc_change_joybutton( struct kc_item * item );
void kc_change_mousebutton( struct kc_item * item );
void kc_change_joyaxis( struct kc_item * item );
void kc_change_mouseaxis( struct kc_item * item );
void kc_change_invert( struct kc_item * item );
void kconfig_read_fcs( int raw_axis );
void kconfig_set_fcs_button( int btn, int button );

void kconfig_sub(kc_item * items,int nitems, char * title)
{
	grs_canvas * save_canvas;
	grs_font * save_font;
	int old_keyd_repeat;

	int i,k,ocitem,citem;
	int time_stopped = 0;


	All_items = items;
	Num_items = nitems;

	if (!((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence)) )
	{
		time_stopped = 1;
		stop_time();
	}

	save_canvas = grd_curcanv;
	gr_set_current_canvas( NULL );			
	save_font = grd_curcanv->cv_font;
	game_flush_inputs();
	old_keyd_repeat = keyd_repeat;
	keyd_repeat = 1;

	//gr_clear_canvas( BM_XRGB(0,0,0) );

	nm_draw_background(0,0,grd_curcanv->cv_bitmap.bm_w, grd_curcanv->cv_bitmap.bm_h );

	grd_curcanv->cv_font = Gamefonts[GFONT_MEDIUM_3];

	{
		char * p;
		p = strchr( title, '\n' );
		if ( p ) *p = 32;
		gr_string( 0x8000, 8, title );
		if ( p ) *p = '\n';
	}


//	if ( items == kc_keyboard )	{
//		gr_string( 0x8000, 8, "Keyboard" );
//	} else if ( items == kc_joystick )	{
//		gr_string( 0x8000, 8, "Joysticks" );
//	} else if ( items == kc_mouse )	{
//		gr_string( 0x8000, 8, "Mouse" );
//	}

	grd_curcanv->cv_font = GAME_FONT;
	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );

	gr_string( 0x8000, 20, TXT_KCONFIG_STRING_1 );
	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	if ( items == kc_keyboard )	{
		gr_set_fontcolor( BM_XRGB(31,27,6), -1 );
		gr_setcolor( BM_XRGB(31,27,6) );
		
		gr_scanline( 98, 106, 42 );
		gr_scanline( 120, 128, 42 );
		gr_pixel( 98, 43 );						
		gr_pixel( 98, 44 );						
		gr_pixel( 128, 43 );						
		gr_pixel( 128, 44 );						
		
		gr_string( 109, 40, "OR" );

		gr_scanline( 253, 261, 42 );
		gr_scanline( 274, 283, 42 );
		gr_pixel( 253, 43 );						
		gr_pixel( 253, 44 );						
		gr_pixel( 283, 43 );						
		gr_pixel( 283, 44 );						

		gr_string( 264, 40, "OR" );

	} if ( items == kc_joystick )	{
		gr_set_fontcolor( BM_XRGB(31,27,6), -1 );
		gr_setcolor( BM_XRGB(31,27,6) );
		gr_scanline( 18, 135, 37 );
		gr_scanline( 181, 294, 37 );
		gr_scanline( 18, 144, 119+10 );
		gr_scanline( 174, 294, 119+10 );
		gr_string( 0x8000, 35, TXT_BUTTONS );
		gr_string( 0x8000,117+10, TXT_AXES );
		gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
		gr_string( 81, 137, TXT_AXIS );
		gr_string( 111, 137, TXT_INVERT );
		gr_string( 222, 137, TXT_AXIS );
		gr_string( 252, 137, TXT_INVERT );
	} else if ( items == kc_mouse )	{
		gr_set_fontcolor( BM_XRGB(31,27,6), -1 );
		gr_setcolor( BM_XRGB(31,27,6) );
		gr_scanline( 18, 135, 37 );
		gr_scanline( 181, 294, 37 );
		gr_scanline( 18, 144, 119+5 );
		gr_scanline( 174, 294, 119+5 );
		gr_string( 0x8000, 35, TXT_BUTTONS );
		gr_string( 0x8000,117+5, TXT_AXES );
		gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
		gr_string( 169, 129, TXT_AXIS );
		gr_string( 199, 129, TXT_INVERT );
	}
	
	for (i=0; i<nitems; i++ )	{
		kc_drawitem( &items[i], 0 );
	}

	citem = 0;
	kc_drawitem( &items[citem], 1 );

	while(1)		{
		k = key_inkey();
		// ASCENT: show the screen and wait for one display period. If the
		// window is closed, leave the screen as the ESC key does.
		if ( !nm_show_frame() )
			k = KEY_ESC;
		if ( !time_stopped ) {
			#ifdef NETWORK
			if (multi_menu_poll() == -1)
				k = -2;
			#endif
		}
		ocitem = citem;
		switch( k )	{
		case KEY_BACKSP:
			Int3();
			break;
		case KEY_PRINT_SCREEN:
			save_screen_shot(0);
			break;							
		case KEY_CTRLED+KEY_D:
			items[citem].value = 255;
			kc_drawitem( &items[citem], 1 );
			break;
		case KEY_CTRLED+KEY_R:	
			if ( items==kc_keyboard )	{
				for (i=0; i<NUM_KEY_CONTROLS; i++ )		{
					items[i].value=default_kconfig_settings[0][i];
					kc_drawitem( &items[i], 0 );
				}
			} else {
				for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
					items[i].value = default_kconfig_settings[Config_control_type][i];
					kc_drawitem( &items[i], 0 );
				}
			}
			kc_drawitem( &items[citem], 1 );
			break;
		case KEY_UP: 		
		case KEY_PAD8:
#ifdef TABLE_CREATION
			if (items[citem].u==-1) items[citem].u=find_next_item_up( items,nitems, citem);
#endif
			citem = items[citem].u; 
			break;
		case KEY_DOWN: 	
		case KEY_PAD2:
#ifdef TABLE_CREATION
			if (items[citem].d==-1) items[citem].d=find_next_item_down( items,nitems, citem);
#endif
			citem = items[citem].d; 
			break;
		case KEY_LEFT: 	
		case KEY_PAD4:
#ifdef TABLE_CREATION
			if (items[citem].l==-1) items[citem].l=find_next_item_left( items,nitems, citem);
#endif
			citem = items[citem].l; 
			break;
		case KEY_RIGHT: 	
		case KEY_PAD6:
#ifdef TABLE_CREATION
			if (items[citem].r==-1) items[citem].r=find_next_item_right( items,nitems, citem);
#endif
			citem = items[citem].r; 
			break;
		case KEY_ENTER:	
		case KEY_PADENTER:	
			switch( items[citem].type )	{
			case BT_KEY:				kc_change_key( &items[citem] ); break;
			case BT_MOUSE_BUTTON:	kc_change_mousebutton( &items[citem] ); break;
			case BT_MOUSE_AXIS: 		kc_change_mouseaxis( &items[citem] ); break;
			case BT_JOY_BUTTON: 		kc_change_joybutton( &items[citem] ); break;
			case BT_JOY_AXIS: 		kc_change_joyaxis( &items[citem] ); break;
			case BT_INVERT: 			kc_change_invert( &items[citem] ); break;
			}
			break;
		case -2:	
		case KEY_ESC:
			grd_curcanv->cv_font	= save_font;
			gr_set_current_canvas( save_canvas );			
			keyd_repeat = old_keyd_repeat;
			game_flush_inputs();
			if (time_stopped)
				start_time();
			return;
#ifdef TABLE_CREATION
		case KEY_DEBUGGED+KEY_F12:	{
			FILE * fp;
			for (i=0; i<NUM_KEY_CONTROLS; i++ )	{
				kc_keyboard[i].u = find_next_item_up( kc_keyboard,NUM_KEY_CONTROLS, i);
				kc_keyboard[i].d = find_next_item_down( kc_keyboard,NUM_KEY_CONTROLS, i);
				kc_keyboard[i].l = find_next_item_left( kc_keyboard,NUM_KEY_CONTROLS, i);
				kc_keyboard[i].r = find_next_item_right( kc_keyboard,NUM_KEY_CONTROLS, i);
			}
			for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
				kc_joystick[i].u = find_next_item_up( kc_joystick,NUM_OTHER_CONTROLS, i);
				kc_joystick[i].d = find_next_item_down( kc_joystick,NUM_OTHER_CONTROLS, i);
				kc_joystick[i].l = find_next_item_left( kc_joystick,NUM_OTHER_CONTROLS, i);
				kc_joystick[i].r = find_next_item_right( kc_joystick,NUM_OTHER_CONTROLS, i);
			}
			for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
				kc_mouse[i].u = find_next_item_up( kc_mouse,NUM_OTHER_CONTROLS, i);
				kc_mouse[i].d = find_next_item_down( kc_mouse,NUM_OTHER_CONTROLS, i);
				kc_mouse[i].l = find_next_item_left( kc_mouse,NUM_OTHER_CONTROLS, i);
				kc_mouse[i].r = find_next_item_right( kc_mouse,NUM_OTHER_CONTROLS, i);
			}
			fp = fopen( "kconfig.cod", "wt" );

			fprintf( fp, "ubyte default_kconfig_settings[CONTROL_MAX_TYPES][MAX_CONTROLS] = {\n" );
			for (i=0; i<CONTROL_MAX_TYPES; i++ )	{
				int j;
				fprintf( fp, "{0x%x", kconfig_settings[i][0] );
				for (j=1; j<MAX_CONTROLS; j++ )
					fprintf( fp, ",0x%x", kconfig_settings[i][j] );
				fprintf( fp, "},\n" );
			}
			fprintf( fp, "};\n" );
		
			fprintf( fp, "\nkc_item kc_keyboard[NUM_KEY_CONTROLS] = {\n" );
			for (i=0; i<NUM_KEY_CONTROLS; i++ )	{
				fprintf( fp, "\t{ %2d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%c%s%c, %s, 255 },\n", 
					kc_keyboard[i].id, kc_keyboard[i].x, kc_keyboard[i].y, kc_keyboard[i].w1, kc_keyboard[i].w2,
					kc_keyboard[i].u, kc_keyboard[i].d, kc_keyboard[i].l, kc_keyboard[i].r,
					34, Text_string[kc_keyboard[i].text_num1], 34, btype_text[kc_keyboard[i].type] );
			}
			fprintf( fp, "};" );

			fprintf( fp, "\nkc_item kc_joystick[NUM_OTHER_CONTROLS] = {\n" );
			for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
				fprintf( fp, "\t{ %2d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%c%s%c, %s, 255 },\n", 
					kc_joystick[i].id, kc_joystick[i].x, kc_joystick[i].y, kc_joystick[i].w1, kc_joystick[i].w2,
					kc_joystick[i].u, kc_joystick[i].d, kc_joystick[i].l, kc_joystick[i].r,
					34, Text_string[kc_joystick[i].text_num1], 34, btype_text[kc_joystick[i].type] );
			}
			fprintf( fp, "};" );

			fprintf( fp, "\nkc_item kc_mouse[NUM_OTHER_CONTROLS] = {\n" );
			for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
				fprintf( fp, "\t{ %2d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%3d,%c%s%c, %s, 255 },\n", 
					kc_mouse[i].id, kc_mouse[i].x, kc_mouse[i].y, kc_mouse[i].w1, kc_mouse[i].w2,
					kc_mouse[i].u, kc_mouse[i].d, kc_mouse[i].l, kc_mouse[i].r,
					34, Text_string[kc_mouse[i].text_num1], 34, btype_text[kc_mouse[i].type] );
			}
			fprintf( fp, "};" );

			fclose(fp);

			}
			break;
#endif
		}
		if (ocitem!=citem)	{
			kc_drawitem( &items[ocitem], 0 );
			kc_drawitem( &items[citem], 1 );
		}
	}
}


void kc_drawitem( kc_item *item, int is_current )
{
	int x, w, h, aw;
	char btext[10];

	if (is_current)
		gr_set_fontcolor( BM_XRGB(20,20,29), -1 );
	else
		gr_set_fontcolor( BM_XRGB(15,15,24), -1 );
	gr_string( item->x, item->y, Text_string[item->text_num1] );

	if (item->value==255) {
		btext[0] = 0;		// ASCENT: was sprintf with an empty format (compiler warning)
	} else {
		switch( item->type )	{
			case BT_KEY:
				strncpy( btext, key_text[item->value], 10 ); break;
			case BT_MOUSE_BUTTON:
				strncpy( btext, Text_string[mousebutton_text[item->value]], 10 ); break;
			case BT_MOUSE_AXIS:
				strncpy( btext, Text_string[mouseaxis_text[item->value]], 10 ); break;
			case BT_JOY_BUTTON:
				if ( joybutton_text[item->value] !=-1 )
					strncpy( btext, Text_string[ joybutton_text[item->value]  ], 10 );
				else
					sprintf( btext, "BTN%d", item->value );
				break;
			case BT_JOY_AXIS:
				strncpy( btext, Text_string[joyaxis_text[item->value]], 10 ); break;
			case BT_INVERT:
				strncpy( btext, Text_string[invert_text[item->value]], 10 ); break;
		}
	}
	gr_get_string_size(btext, &w, &h, &aw  );

	if (is_current)
		gr_setcolor( BM_XRGB(21,0,24) );
	else
		gr_setcolor( BM_XRGB(16,0,19) );
	gr_urect( item->w1+item->x, item->y-1, item->w1+item->x+item->w2, item->y+h );
	
	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );

	x = item->w1+item->x+((item->w2-w)/2);

	gr_string( x, item->y, btext );
}


static int looper=0;

void kc_drawquestion( kc_item *item )
{
	int c, x, w, h, aw;
	
	gr_get_string_size("?", &w, &h, &aw  );

	c = BM_XRGB(21,0,24);

	gr_setcolor( gr_fade_table[fades[looper]*256+c] );
	looper++;
	if (looper>63) looper=0;

	gr_urect( item->w1+item->x, item->y-1, item->w1+item->x+item->w2, item->y+h );
	
	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );

	x = item->w1+item->x+((item->w2-w)/2);

	gr_string( x, item->y, "?" );
}

void kc_change_key( kc_item * item )
{
	int i,n,f,k;
	ubyte keycode;

	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	
	gr_string( 0x8000, INFO_Y, TXT_PRESS_NEW_KEY );
	
	game_flush_inputs();
	keycode=255;
	k=255;
	while( (k!=KEY_ESC) && (keycode==255) )	{				
		#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
			multi_menu_poll();
		#endif
//		if ( Game_mode & GM_MULTI )
//			GameLoop( 0, 0 );				// Continue
		k = key_inkey();
		// ASCENT: timer_delay replaces delay(10) of dos.h. The loop also
		// shows the frame, and ends as with ESC if the window is closed.
		timer_delay(F1_0/100);
		if ( !plat_pump() ) k = KEY_ESC;
		kc_drawquestion( item );
		gr_present();
		
		for (i=0; i<256; i++ )	{
			if (keyd_pressed[i] && (strlen(key_text[i])>0))	{
				f = 0;
				for (n=0; n<sizeof(system_keys); n++ )
					if ( system_keys[n] == i )
						f=1;
				if (!f)	
					keycode=i;
			}
		}
	}

	if (k!=KEY_ESC)	{
		for (i=0; i<Num_items; i++ )	{
			n = item - All_items;
			if ( (i!=n) && (All_items[i].type==BT_KEY) && (All_items[i].value==keycode) )		{
				All_items[i].value = 255;
				kc_drawitem( &All_items[i], 0 );
			}
		}
		item->value = keycode;
	}
	kc_drawitem( item, 1 );
	gr_set_fontcolor( BM_XRGB(28,28,28), BM_XRGB(0,0,0) );

	nm_restore_background( 0, INFO_Y, 310, grd_curcanv->cv_font->ft_h );

	game_flush_inputs();

}

void kc_change_joybutton( kc_item * item )
{
	int n,i,k;
	ubyte code;

	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	
	gr_string( 0x8000, INFO_Y, TXT_PRESS_NEW_JBUTTON );
	
	game_flush_inputs();
	code=255;
	k=255;
	while( (k!=KEY_ESC) && (code==255))	{				
		#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
			multi_menu_poll();
		#endif
//		if ( Game_mode & GM_MULTI )
//			GameLoop( 0, 0 );				// Continue
		k = key_inkey();
		// ASCENT: timer_delay replaces delay(10) of dos.h. The loop also
		// shows the frame, and ends as with ESC if the window is closed.
		timer_delay(F1_0/100);
		if ( !plat_pump() ) k = KEY_ESC;

		if (k == KEY_PRINT_SCREEN)
			save_screen_shot(0);

		kc_drawquestion( item );
		gr_present();

		if (Config_control_type==CONTROL_THRUSTMASTER_FCS)	{
			int axis[4];
			joystick_read_raw_axis( JOY_ALL_AXIS, axis );
			kconfig_read_fcs( axis[3] );
			if ( joy_get_button_state(7) ) code = 7;
			if ( joy_get_button_state(11) ) code = 11;
			if ( joy_get_button_state(15) ) code = 15;
			if ( joy_get_button_state(19) ) code = 19;
			for (i=0; i<4; i++ )	{
				if ( joy_get_button_state(i) )
					code = i;
			}
		} else if (Config_control_type==CONTROL_FLIGHTSTICK_PRO) {
			for (i=4; i<20; i++ )	{
				if ( joy_get_button_state(i)  )	{
					code = i;
					mprintf(( 0, "JB: %d\n", code ));
				}
			}
		} else {
			for (i=0; i<4; i++ )	{
				if ( joy_get_button_state(i) )
					code = i;
			}
		}
	}
	if (code!=255)	{
		for (i=0; i<Num_items; i++ )	{
			n = item - All_items;
			if ( (i!=n) && (All_items[i].type==BT_JOY_BUTTON) && (All_items[i].value==code) )		{
				All_items[i].value = 255;
				kc_drawitem( &All_items[i], 0 );
			}
		}
		item->value = code;
	}
	kc_drawitem( item, 1 );
	nm_restore_background( 0, INFO_Y, 310, grd_curcanv->cv_font->ft_h );
	game_flush_inputs();
}

void kc_change_mousebutton( kc_item * item )
{
	int n,i,b,k;
	ubyte code;

	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	
	gr_string( 0x8000, INFO_Y, TXT_PRESS_NEW_MBUTTON );
	
	game_flush_inputs();
	code=255;
	k=255;
	while( (k!=KEY_ESC) && (code==255))	{				
		#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
			multi_menu_poll();
		#endif
//		if ( Game_mode & GM_MULTI )
//			GameLoop( 0, 0 );				// Continue
		k = key_inkey();
		// ASCENT: timer_delay replaces delay(10) of dos.h. The loop also
		// shows the frame, and ends as with ESC if the window is closed.
		timer_delay(F1_0/100);
		if ( !plat_pump() ) k = KEY_ESC;

		if (k == KEY_PRINT_SCREEN)
			save_screen_shot(0);

		kc_drawquestion( item );
		gr_present();

		b = mouse_get_btns();
		for (i=0; i<3; i++ )	{
			if ( b & (1<<i) )	
				code = i;
		}
	}
	if (code!=255)	{
		for (i=0; i<Num_items; i++ )	{
			n = item - All_items;
			if ( (i!=n) && (All_items[i].type==BT_MOUSE_BUTTON) && (All_items[i].value==code) )		{
				All_items[i].value = 255;
				kc_drawitem( &All_items[i], 0 );
			}
		}
		item->value = code;
	}
	kc_drawitem( item, 1 );
	nm_restore_background( 0, INFO_Y, 310, grd_curcanv->cv_font->ft_h );
	game_flush_inputs();

}

void kc_change_joyaxis( kc_item * item )
{
	int axis[4];
	int old_axis[4];
	int n,i,k;
	ubyte code;

	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	
	gr_string( 0x8000, INFO_Y, TXT_MOVE_NEW_JOY_AXIS );
	
	game_flush_inputs();
	code=255;
	k=255;

	joystick_read_raw_axis( JOY_ALL_AXIS, old_axis );

	while( (k!=KEY_ESC) && (code==255))	{				
		#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
			multi_menu_poll();
		#endif
//		if ( Game_mode & GM_MULTI )
//			GameLoop( 0, 0 );				// Continue
		k = key_inkey();
		// ASCENT: timer_delay replaces delay(10) of dos.h. The loop also
		// shows the frame, and ends as with ESC if the window is closed.
		timer_delay(F1_0/100);
		if ( !plat_pump() ) k = KEY_ESC;

		if (k == KEY_PRINT_SCREEN)
			save_screen_shot(0);

		kc_drawquestion( item );
		gr_present();

		joystick_read_raw_axis( JOY_ALL_AXIS, axis );
		
		for (i=0; i<4; i++ )	{
  			if ( abs(axis[i]-old_axis[i])>20 )	{
				code = i;
			}
			old_axis[i] = axis[i];
		}
	}
	if (code!=255)	{
		for (i=0; i<Num_items; i++ )	{
			n = item - All_items;
			if ( (i!=n) && (All_items[i].type==BT_JOY_AXIS) && (All_items[i].value==code) )		{
				All_items[i].value = 255;
				kc_drawitem( &All_items[i], 0 );
			}
		}

		item->value = code;					 
	}
	kc_drawitem( item, 1 );
	nm_restore_background( 0, INFO_Y, 310, grd_curcanv->cv_font->ft_h );
	game_flush_inputs();

}

void kc_change_mouseaxis( kc_item * item )
{
	int i,n,k;
	ubyte code;
	int dx,dy;

	gr_set_fontcolor( BM_XRGB(28,28,28), -1 );
	
	gr_string( 0x8000, INFO_Y, TXT_MOVE_NEW_MSE_AXIS );
	
	game_flush_inputs();
	code=255;
	k=255;

	mouse_get_delta( &dx, &dy );

	while( (k!=KEY_ESC) && (code==255))	{				
		#ifdef NETWORK
		if ((Game_mode & GM_MULTI) && (Function_mode == FMODE_GAME) && (!Endlevel_sequence))
			multi_menu_poll();
		#endif
//		if ( Game_mode & GM_MULTI )
//			GameLoop( 0, 0 );				// Continue
		k = key_inkey();
		// ASCENT: timer_delay replaces delay(10) of dos.h. The loop also
		// shows the frame, and ends as with ESC if the window is closed.
		timer_delay(F1_0/100);
		if ( !plat_pump() ) k = KEY_ESC;

		if (k == KEY_PRINT_SCREEN)
			save_screen_shot(0);

		kc_drawquestion( item );
		gr_present();

		mouse_get_delta( &dx, &dy );
		if ( abs(dx)>20 ) code = 0;
		if ( abs(dy)>20 )	code = 1;
	}
	if (code!=255)	{
		for (i=0; i<Num_items; i++ )	{
			n = item - All_items;
			if ( (i!=n) && (All_items[i].type==BT_MOUSE_AXIS) && (All_items[i].value==code) )		{
				All_items[i].value = 255;
				kc_drawitem( &All_items[i], 0 );
			}
		}
		item->value = code;
	}
	kc_drawitem( item, 1 );
	nm_restore_background( 0, INFO_Y, 310, grd_curcanv->cv_font->ft_h );
	game_flush_inputs();

}


void kc_change_invert( kc_item * item )
{
	game_flush_inputs();

	if (item->value)
		item->value = 0;
	else 
		item->value = 1;

	kc_drawitem( item, 1 );

}

#include "screens.h"

void kconfig(int n, char * title)
{
	int i;
	set_screen_mode( SCREEN_MENU );

	kc_set_controls();

	switch(n)	{
	case 0:kconfig_sub( kc_keyboard, NUM_KEY_CONTROLS, title );break;
	case 1:kconfig_sub( kc_joystick, NUM_OTHER_CONTROLS, title );break;
	case 2:kconfig_sub( kc_mouse, NUM_OTHER_CONTROLS, title ); break;
	default:
		Int3();
		return;
	}

	reset_cockpit();		//force cockpit redraw next time

	// Update save values...
	
	for (i=0; i<NUM_KEY_CONTROLS; i++ )	
		kconfig_settings[0][i] = kc_keyboard[i].value;

	if ( (Config_control_type>0) && (Config_control_type<5))	{
		for (i=0; i<NUM_OTHER_CONTROLS; i++ )	
			kconfig_settings[Config_control_type][i] = kc_joystick[i].value;
	} else if (Config_control_type>4) {
		for (i=0; i<NUM_OTHER_CONTROLS; i++ )	
			kconfig_settings[Config_control_type][i] = kc_mouse[i].value;
	}

}


void kconfig_read_fcs( int raw_axis )
{
	int raw_button, button, axis_min[4], axis_center[4], axis_max[4];

	if (Config_control_type!=CONTROL_THRUSTMASTER_FCS) return;

	joy_get_cal_vals(axis_min, axis_center, axis_max);

	if ( axis_max[3] > 1 )
		raw_button = (raw_axis*100)/axis_max[3];
	else
		raw_button = 0;

	if ( raw_button > 88 )
		button = 0;
	else if ( raw_button > 63 )
		button = 7;
	else if ( raw_button > 39 )
		button = 11;
	else if ( raw_button > 15 )
		button = 15;
	else	
		button = 19;

	kconfig_set_fcs_button( 19, button );
	kconfig_set_fcs_button( 15, button );
	kconfig_set_fcs_button( 11, button );
	kconfig_set_fcs_button( 7, button );
}
		

void kconfig_set_fcs_button( int btn, int button )
{
	int state,time_down,upcount,downcount;
	state = time_down = upcount = downcount = 0;

	if ( joy_get_button_state(btn) ) {
		if ( btn==button )	{
			state = 1;
			time_down = FrameTime;
		} else {
			upcount=1;
		}
	} else {
		if ( btn==button )	{
			state = 1;
			time_down = FrameTime;
			downcount=1;
		} else {
			upcount=1;
		}
	}				
			
	joy_set_btn_values( btn, state, time_down, downcount, upcount );
					
}



fix Last_angles_p = 0;
fix Last_angles_b = 0;
fix Last_angles_h = 0;
ubyte Last_angles_read = 0;

extern int			VR_sensitivity;
						
int VR_sense_range[3] = { 25, 50, 75 };

// ASCENT: the virtual reality headsets (VFX1, i-glasses, Victor) are not
// part of the port. The function does nothing. The original code stays
// below for reference and is not compiled.
void read_head_tracker()
{
}

#if 0	// ASCENT: headset code, not compiled
read_head_tracker()
{
	fix yaw, pitch, roll;
	int buttons;
//------ read vfx1 helmet --------
	if (vfx1_installed) {
		SenseGetData( sense_function1, DCHTD,&yaw,&pitch, &roll, &buttons);
	} else if (iglasses_headset_installed)	{
		iglasses_read_headset( &yaw, &pitch, &roll );
	} else if (Victor_headset_installed)   {
		victor_read_headset_filtered( &yaw, &pitch, &roll );
	} else {
		return;
	}

	Use_player_head_angles = 0;
	if ( Last_angles_read )	{
		fix yaw1 = yaw;
		
		yaw1 = yaw;
		if ( (Last_angles_h < (F1_0/4) ) && (yaw > ((F1_0*3)/4) ) )	
			yaw1 -= F1_0;
		else if ( (yaw < (F1_0/4) ) && (Last_angles_h > ((F1_0*3)/4) ) )	
			yaw1 += F1_0;
	
		Controls.pitch_time	+= fixmul((pitch- Last_angles_p)*VR_sense_range[VR_sensitivity],FrameTime);
		Controls.heading_time+= fixmul((yaw1 -  Last_angles_h)*VR_sense_range[VR_sensitivity],FrameTime);
		Controls.bank_time	+= fixmul((roll - Last_angles_b)*VR_sense_range[VR_sensitivity],FrameTime);
	}
	Last_angles_read = 1;
	Last_angles_p = pitch;
	Last_angles_h = yaw;
	Last_angles_b = roll;
}
#endif	// ASCENT: end of the headset code

#define	PH_SCALE	8
#define	JOYSTICK_READ_TIME	(F1_0/10)		//	Read joystick at 10 Hz.
fix	LastReadTime = 0;

fix	joy_axis[4];

ubyte 			kc_use_external_control = 0;
ubyte				kc_enable_external_control = 1;
ubyte 			kc_external_intno = 0;
control_info	*kc_external_control = NULL;
ubyte				*kc_external_name = NULL;
ubyte				kc_external_version = 0;

// ASCENT: the external control interface called a real mode interrupt of a
// DOS driver that wrote into the memory of the game (-xcontrol). It cannot
// work in the port. kconfig_init_external_controls does nothing, so
// kc_use_external_control stays 0 and nothing reads the external controls.
// The original code stays below for reference and is not compiled.
void kconfig_init_external_controls(int intno, int address)
{
	intno = intno;
	address = address;
}

void kconfig_read_external_controls()
{
}

#if 0	// ASCENT: external control code, not compiled
void kconfig_init_external_controls(int intno, int address)
{
	int i;
	kc_external_intno = intno;
	kc_external_control	= (control_info *)address;
	kc_use_external_control = 1;
	kc_enable_external_control  = 1;

	i = FindArg( "-xname" );
	if ( i )	
		kc_external_name = Args[i+1];
	else
		kc_external_name = "External Controller";

	i = FindArg( "-xver" );
	if ( i )
		kc_external_version = atoi(Args[i+1]);
	
	printf( "%s int: 0x%x, data: 0x%x, ver:%d\n", kc_external_name, kc_external_intno, kc_external_control, kc_external_version );

}

void kconfig_read_external_controls()
{
	union REGS r;

	if ( !kc_enable_external_control ) return;

	if ( kc_external_version == 0 ) 
		memset( kc_external_control, 0, sizeof(control_info) );
	else if ( kc_external_version > 0 ) 	{
		memset( kc_external_control, 0, sizeof(control_info)+sizeof(vms_angvec) + 64 );
		if ( kc_external_version > 1 ) {
			// Write ship pos and angles to external controls...
			ubyte *temp_ptr = (ubyte *)kc_external_control;
			vms_vector *ship_pos;
			vms_matrix *ship_orient;
			memset( kc_external_control, 0, sizeof(control_info)+sizeof(vms_angvec) + 64 + sizeof(vms_vector)+sizeof(vms_matrix) );
			temp_ptr += sizeof(control_info)+sizeof(vms_angvec) + 64;
			ship_pos = (vms_vector *)temp_ptr;
			temp_ptr += sizeof(vms_vector);
			ship_orient = (vms_matrix *)temp_ptr;
			// Fill in ship postion...
			*ship_pos = Objects[Players[Player_num].objnum].pos;
			// Fill in ship orientation...
			*ship_orient = Objects[Players[Player_num].objnum].orient;
		}
	}

	if ( grd_curscreen->sc_mode != SM_320x200C )			// (If in automap...)
		kc_external_control->automap_state = 1;
	memset(&r,0,sizeof(r));

	int386 ( kc_external_intno, &r, &r);		// Read external info...

	if ( Player_num > -1 )	{
		Objects[Players[Player_num].objnum].mtype.phys_info.flags &= (~PF_TURNROLL);	// Turn off roll when turning
		Objects[Players[Player_num].objnum].mtype.phys_info.flags &= (~PF_LEVELLING);	// Turn off leveling to nearest side.
		Auto_leveling_on = 0;

		if ( kc_external_version > 0 ) {		
			vms_matrix tempm, ViewMatrix;
			vms_angvec * Kconfig_abs_movement;
			char * oem_message;
	
			Kconfig_abs_movement = (vms_angvec *)((uint)kc_external_control + sizeof(control_info));
	
			if ( Kconfig_abs_movement->p || Kconfig_abs_movement->b || Kconfig_abs_movement->h )	{
				vm_angles_2_matrix(&tempm,Kconfig_abs_movement);
				vm_matrix_x_matrix(&ViewMatrix,&Objects[Players[Player_num].objnum].orient,&tempm);
				Objects[Players[Player_num].objnum].orient = ViewMatrix;		
			}
			oem_message = (char *)((uint)Kconfig_abs_movement + sizeof(vms_angvec));
			if (oem_message[0] != '\0' )
				HUD_init_message( oem_message );
		}
	}



	Controls.pitch_time += fixmul(kc_external_control->pitch_time,FrameTime);						
	Controls.vertical_thrust_time += fixmul(kc_external_control->vertical_thrust_time,FrameTime);
	Controls.heading_time += fixmul(kc_external_control->heading_time,FrameTime);
	Controls.sideways_thrust_time += fixmul(kc_external_control->sideways_thrust_time ,FrameTime);
	Controls.bank_time += fixmul(kc_external_control->bank_time ,FrameTime);
	Controls.forward_thrust_time += fixmul(kc_external_control->forward_thrust_time ,FrameTime);
	Controls.rear_view_down_count += kc_external_control->rear_view_down_count;	
	Controls.rear_view_down_state |= kc_external_control->rear_view_down_state;	
	Controls.fire_primary_down_count += kc_external_control->fire_primary_down_count;
	Controls.fire_primary_state |= kc_external_control->fire_primary_state;
	Controls.fire_secondary_state |= kc_external_control->fire_secondary_state;
	Controls.fire_secondary_down_count += kc_external_control->fire_secondary_down_count;
	Controls.fire_flare_down_count += kc_external_control->fire_flare_down_count;
	Controls.drop_bomb_down_count += kc_external_control->drop_bomb_down_count;	
	Controls.automap_down_count += kc_external_control->automap_down_count;
	Controls.automap_state |= kc_external_control->automap_state;
}
#endif	// ASCENT: end of the external control code

void controls_read_all()
{
	int i;
	int slide_on, bank_on;
	int dx, dy;
	int idx, idy;
	fix ctime;
	fix mouse_axis[2];
	int raw_joy_axis[4];
	int mouse_buttons;
	fix k0, k1, k2, k3, kp;
	fix k4, k5, k6, k7, kh;
	ubyte channel_masks;
	int use_mouse, use_joystick;
	int speed_factor=1;

	if (Game_turbo_mode)
		speed_factor = 2;
	
	if (Arcade_mode)	{
		arcade_read_controls();
		return;
	}

	{
		fix temp = Controls.heading_time;
		fix temp1 = Controls.pitch_time;
		memset( &Controls, 0, sizeof(control_info) );
		Controls.heading_time = temp;
		Controls.pitch_time = temp1;
	}
	slide_on = 0;
	bank_on = 0;

	ctime = timer_get_fixed_seconds();

	//---------  Read Joystick -----------
	if ( (LastReadTime + JOYSTICK_READ_TIME > ctime) && (Config_control_type!=CONTROL_THRUSTMASTER_FCS) ) {
		if ((ctime < 0) && (LastReadTime > 0))
			LastReadTime = ctime;
		use_joystick=1;
	} else if ((Config_control_type>0) && (Config_control_type<5) ) {
		LastReadTime = ctime;
		channel_masks = joystick_read_raw_axis( JOY_ALL_AXIS, raw_joy_axis );
		
		for (i=0; i<4; i++ )	{
			if (channel_masks&(1<<i))	{
				int joy_null_value = 10;

				if ( (i==3) && (Config_control_type==CONTROL_THRUSTMASTER_FCS) )	{
					kconfig_read_fcs( raw_joy_axis[i] );
				} else {
					raw_joy_axis[i] = joy_get_scaled_reading( raw_joy_axis[i], i );
	
					if (kc_joystick[23].value==i)		// If this is the throttle
						joy_null_value = 20;				// Then use a larger dead-zone
	
					if (raw_joy_axis[i] > joy_null_value) 
						raw_joy_axis[i] = ((raw_joy_axis[i]-joy_null_value)*128)/(128-joy_null_value);
				  	else if (raw_joy_axis[i] < -joy_null_value)
						raw_joy_axis[i] = ((raw_joy_axis[i]+joy_null_value)*128)/(128-joy_null_value);
					else
						raw_joy_axis[i] = 0;
					joy_axis[i]	= (raw_joy_axis[i]*FrameTime)/128;	
				}
			} else {
				joy_axis[i] = 0;
			}
		}	
		use_joystick=1;
	} else {
		for (i=0; i<4; i++ )
			joy_axis[i] = 0;
		use_joystick=0;
	}

	if (Config_control_type==5 ) {
		//---------  Read Mouse -----------
		mouse_get_delta( &dx, &dy );
		mouse_axis[0] = (dx*FrameTime)/35;
		mouse_axis[1] = (dy*FrameTime)/25;
		mouse_buttons = mouse_get_btns();
		//mprintf(( 0, "Mouse %d,%d b:%d, 0x%x\n", mouse_axis[0], mouse_axis[1], mouse_buttons, FrameTime ));
		use_mouse=1;
	} else if (Config_control_type==6 ) {
		//---------  Read Cyberman -----------
		mouse_get_cyberman_pos(&idx,&idy );
		mouse_axis[0] = (idx*FrameTime)/128;
		mouse_axis[1] = (idy*FrameTime)/128;
		mouse_buttons = mouse_get_btns();
		use_mouse=1;
	} else {
		mouse_axis[0] = 0;
		mouse_axis[1] = 0;
		mouse_buttons = 0;
		use_mouse=0;
	}


//------------- Read slide_on -------------
	
	// From keyboard...
	if ( kc_keyboard[8].value < 255 ) slide_on |= keyd_pressed[ kc_keyboard[8].value ];
	if ( kc_keyboard[9].value < 255 ) slide_on |= keyd_pressed[ kc_keyboard[9].value ];
	// From joystick...
	if ((use_joystick)&&(kc_joystick[5].value<255)) slide_on |= joy_get_button_state( kc_joystick[5].value );
	// From mouse...
	if ((use_mouse)&&(kc_mouse[5].value<255)) slide_on |= mouse_buttons & (1<<kc_mouse[5].value);

//------------- Read bank_on ---------------

	// From keyboard...
	if ( kc_keyboard[18].value < 255 ) bank_on |= keyd_pressed[ kc_keyboard[18].value ];
	if ( kc_keyboard[19].value < 255 ) bank_on |= keyd_pressed[ kc_keyboard[19].value ];
	// From joystick...
	if ( (use_joystick)&&(kc_joystick[10].value < 255 )) bank_on |= joy_get_button_state( kc_joystick[10].value );
	// From mouse...
	if ( (use_mouse)&&(kc_mouse[10].value < 255 )) bank_on |= mouse_buttons & (1<<kc_mouse[10].value);

//------------ Read pitch_time -----------
	if ( !slide_on )	{
		// mprintf((0, "pitch: %7.3f %7.3f: %7.3f\n", f2fl(k4), f2fl(k6), f2fl(Controls.heading_time)));
		kp = 0;
		k0 = speed_factor*key_down_time( kc_keyboard[0].value )/2;	// Divide by two since we want pitch to go slower
		k1 = speed_factor*key_down_time( kc_keyboard[1].value )/2;
		k2 = speed_factor*key_down_time( kc_keyboard[2].value )/2;
		k3 = speed_factor*key_down_time( kc_keyboard[3].value )/2;

		// From keyboard...
		if ( kc_keyboard[0].value < 255 ) kp += k0/PH_SCALE;
		if ( kc_keyboard[1].value < 255 ) kp += k1/PH_SCALE;
		if ( kc_keyboard[2].value < 255 ) kp -= k2/PH_SCALE;
		if ( kc_keyboard[3].value < 255 ) kp -= k3/PH_SCALE;

		// From Cyberman...
		if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
			kp += mouse_button_down_time(MB_PITCH_FORWARD)/(PH_SCALE*2);
			kp -= mouse_button_down_time(MB_PITCH_BACKWARD)/(PH_SCALE*2);
		}
	
		if (kp == 0)
			Controls.pitch_time = 0;
		else if (kp > 0) {
			if (Controls.pitch_time < 0)
				Controls.pitch_time = 0;
		} else // kp < 0
			if (Controls.pitch_time > 0)
				Controls.pitch_time = 0;
		Controls.pitch_time += kp;
	
		// From joystick...
		if ( (use_joystick)&&(kc_joystick[13].value < 255 ))	{
			if ( !kc_joystick[14].value )		// If not inverted...
				Controls.pitch_time -= (joy_axis[kc_joystick[13].value]*Config_joystick_sensitivity)/8;
			else
				Controls.pitch_time += (joy_axis[kc_joystick[13].value]*Config_joystick_sensitivity)/8;
		}
	
		// From mouse...
		//mprintf(( 0, "UM: %d, PV: %d\n", use_mouse, kc_mouse[13].value ));
		if ( (use_mouse)&&(kc_mouse[13].value < 255) )	{
			if ( !kc_mouse[14].value )		// If not inverted...
				Controls.pitch_time -= (mouse_axis[kc_mouse[13].value]*Config_joystick_sensitivity)/8;
			else
				Controls.pitch_time += (mouse_axis[kc_mouse[13].value]*Config_joystick_sensitivity)/8;
		}
	} else {
		Controls.pitch_time = 0;
	}


//----------- Read vertical_thrust_time -----------------

	if ( slide_on )	{
		k0 = speed_factor*key_down_time( kc_keyboard[0].value );
		k1 = speed_factor*key_down_time( kc_keyboard[1].value );
		k2 = speed_factor*key_down_time( kc_keyboard[2].value );
		k3 = speed_factor*key_down_time( kc_keyboard[3].value );

		// From keyboard...
		if ( kc_keyboard[0].value < 255 ) Controls.vertical_thrust_time += k0;
		if ( kc_keyboard[1].value < 255 ) Controls.vertical_thrust_time += k1;
		if ( kc_keyboard[2].value < 255 ) Controls.vertical_thrust_time -= k2;
		if ( kc_keyboard[3].value < 255 ) Controls.vertical_thrust_time -= k3;

		// From Cyberman...
		if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
			Controls.vertical_thrust_time -= mouse_button_down_time(MB_PITCH_FORWARD);
			Controls.vertical_thrust_time += mouse_button_down_time(MB_PITCH_BACKWARD);
		}
	
		// From joystick...
		if ((use_joystick)&&( kc_joystick[13].value < 255 ))	{
			if ( !kc_joystick[14].value )		// If not inverted...
				Controls.vertical_thrust_time += joy_axis[kc_joystick[13].value];
			else
				Controls.vertical_thrust_time -= joy_axis[kc_joystick[13].value];
		}
	
		// From mouse...
		if ( (use_mouse)&&(kc_mouse[13].value < 255 ))	{
			if ( !kc_mouse[14].value )		// If not inverted...
				Controls.vertical_thrust_time -= mouse_axis[kc_mouse[13].value];
			else
				Controls.vertical_thrust_time += mouse_axis[kc_mouse[13].value];
		}
	}

	// From keyboard...
	if ( kc_keyboard[14].value < 255 ) Controls.vertical_thrust_time += speed_factor*key_down_time( kc_keyboard[14].value );
	if ( kc_keyboard[15].value < 255 ) Controls.vertical_thrust_time += speed_factor*key_down_time( kc_keyboard[15].value );
	if ( kc_keyboard[16].value < 255 ) Controls.vertical_thrust_time -= speed_factor*key_down_time( kc_keyboard[16].value );
	if ( kc_keyboard[17].value < 255 ) Controls.vertical_thrust_time -= speed_factor*key_down_time( kc_keyboard[17].value );
	
	// From joystick...
	if ((use_joystick)&&( kc_joystick[19].value < 255 ))	{
		if ( !kc_joystick[20].value )		// If not inverted...
			Controls.vertical_thrust_time += joy_axis[kc_joystick[19].value];
		else
			Controls.vertical_thrust_time -= joy_axis[kc_joystick[19].value];
	}

	// From joystick buttons
	if ( (use_joystick)&&(kc_joystick[8].value < 255 )) Controls.vertical_thrust_time += joy_get_button_down_time( kc_joystick[8].value );
	if ( (use_joystick)&&(kc_joystick[9].value < 255 )) Controls.vertical_thrust_time -= joy_get_button_down_time( kc_joystick[9].value );

	// From mouse buttons
	if ( (use_mouse)&&(kc_mouse[8].value < 255 )) Controls.vertical_thrust_time += mouse_button_down_time( kc_mouse[8].value );
	if ( (use_mouse)&&(kc_mouse[9].value < 255 )) Controls.vertical_thrust_time -= mouse_button_down_time( kc_mouse[9].value );

	// From mouse...
	if ( (use_mouse)&&(kc_mouse[19].value < 255 ))	{
		if ( !kc_mouse[20].value )		// If not inverted...
			Controls.vertical_thrust_time += mouse_axis[kc_mouse[19].value];
		else
			Controls.vertical_thrust_time -= mouse_axis[kc_mouse[19].value];
	}

	// From Cyberman...
	if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
		Controls.vertical_thrust_time += mouse_button_down_time(MB_Z_UP)/2;
		Controls.vertical_thrust_time -= mouse_button_down_time(MB_Z_DOWN)/2;
	}

//---------- Read heading_time -----------

	if (!slide_on && !bank_on)	{
		//mprintf((0, "heading: %7.3f %7.3f: %7.3f\n", f2fl(k4), f2fl(k6), f2fl(Controls.heading_time)));
		kh = 0;
		k4 = speed_factor*key_down_time( kc_keyboard[4].value );
		k5 = speed_factor*key_down_time( kc_keyboard[5].value );
		k6 = speed_factor*key_down_time( kc_keyboard[6].value );
		k7 = speed_factor*key_down_time( kc_keyboard[7].value );

		// From keyboard...
		if ( kc_keyboard[4].value < 255 ) kh -= k4/PH_SCALE;
		if ( kc_keyboard[5].value < 255 ) kh -= k5/PH_SCALE;
		if ( kc_keyboard[6].value < 255 ) kh += k6/PH_SCALE;
		if ( kc_keyboard[7].value < 255 ) kh += k7/PH_SCALE;

		// From Cyberman...
		if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
			kh -= mouse_button_down_time(MB_HEAD_LEFT)/PH_SCALE;
			kh += mouse_button_down_time(MB_HEAD_RIGHT)/PH_SCALE;
		}
	
		if (kh == 0)
			Controls.heading_time = 0;
		else if (kh > 0) {
			if (Controls.heading_time < 0)
				Controls.heading_time = 0;
		} else // kh < 0
			if (Controls.heading_time > 0)
				Controls.heading_time = 0;
		Controls.heading_time += kh;

		// From joystick...
		if ( (use_joystick)&&(kc_joystick[15].value < 255 ))	{
			if ( !kc_joystick[16].value )		// If not inverted...
				Controls.heading_time += (joy_axis[kc_joystick[15].value]*Config_joystick_sensitivity)/8;
			else
				Controls.heading_time -= (joy_axis[kc_joystick[15].value]*Config_joystick_sensitivity)/8;
		}
	
		// From mouse...
		if ( (use_mouse)&&(kc_mouse[15].value < 255 ))	{
			if ( !kc_mouse[16].value )		// If not inverted...
				Controls.heading_time += (mouse_axis[kc_mouse[15].value]*Config_joystick_sensitivity)/8;
			else
				Controls.heading_time -= (mouse_axis[kc_mouse[15].value]*Config_joystick_sensitivity)/8;
		}
	} else {
		Controls.heading_time = 0;
	}

//----------- Read sideways_thrust_time -----------------

	if ( slide_on )	{
		k0 = speed_factor*key_down_time( kc_keyboard[4].value );
		k1 = speed_factor*key_down_time( kc_keyboard[5].value );
		k2 = speed_factor*key_down_time( kc_keyboard[6].value );
		k3 = speed_factor*key_down_time( kc_keyboard[7].value );

		// From keyboard...
		if ( kc_keyboard[4].value < 255 ) Controls.sideways_thrust_time -= k0;
		if ( kc_keyboard[5].value < 255 ) Controls.sideways_thrust_time -= k1;
		if ( kc_keyboard[6].value < 255 ) Controls.sideways_thrust_time += k2;
		if ( kc_keyboard[7].value < 255 ) Controls.sideways_thrust_time += k3;
	
		// From joystick...
		if ( (use_joystick)&&(kc_joystick[15].value < 255 ))	{
			if ( !kc_joystick[16].value )		// If not inverted...
				Controls.sideways_thrust_time += joy_axis[kc_joystick[15].value];
			else
				Controls.sideways_thrust_time -= joy_axis[kc_joystick[15].value];
		}
		
		// From cyberman
		if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
			Controls.sideways_thrust_time -= mouse_button_down_time(MB_HEAD_LEFT);
			Controls.sideways_thrust_time += mouse_button_down_time(MB_HEAD_RIGHT);
		}
	
		// From mouse...
		if ( (use_mouse)&&(kc_mouse[15].value < 255 ))	{
			if ( !kc_mouse[16].value )		// If not inverted...
				Controls.sideways_thrust_time += mouse_axis[kc_mouse[15].value];
			else
				Controls.sideways_thrust_time -= mouse_axis[kc_mouse[15].value];
		}
	}

	// From keyboard...
	if ( kc_keyboard[10].value < 255 ) Controls.sideways_thrust_time -= speed_factor*key_down_time( kc_keyboard[10].value );
	if ( kc_keyboard[11].value < 255 ) Controls.sideways_thrust_time -= speed_factor*key_down_time( kc_keyboard[11].value );
	if ( kc_keyboard[12].value < 255 ) Controls.sideways_thrust_time += speed_factor*key_down_time( kc_keyboard[12].value );
	if ( kc_keyboard[13].value < 255 ) Controls.sideways_thrust_time += speed_factor*key_down_time( kc_keyboard[13].value );
	
	// From joystick...
	if ( (use_joystick)&&(kc_joystick[17].value < 255 ))	{
		if ( !kc_joystick[18].value )		// If not inverted...
			Controls.sideways_thrust_time -= joy_axis[kc_joystick[17].value];
		else
			Controls.sideways_thrust_time += joy_axis[kc_joystick[17].value];
	}

	// From joystick buttons
	if ( (use_joystick)&&(kc_joystick[6].value < 255 )) Controls.sideways_thrust_time -= joy_get_button_down_time( kc_joystick[6].value );
	if ( (use_joystick)&&(kc_joystick[7].value < 255 )) Controls.sideways_thrust_time += joy_get_button_down_time( kc_joystick[7].value );

	// From mouse buttons
	if ( (use_mouse)&&(kc_mouse[6].value < 255 )) Controls.sideways_thrust_time -= mouse_button_down_time( kc_mouse[6].value );
	if ( (use_mouse)&&(kc_mouse[7].value < 255 )) Controls.sideways_thrust_time += mouse_button_down_time( kc_mouse[7].value );

	// From mouse...
	if ( (use_mouse)&&(kc_mouse[17].value < 255 ))	{
		if ( !kc_mouse[18].value )		// If not inverted...
			Controls.sideways_thrust_time += mouse_axis[kc_mouse[17].value];
		else
			Controls.sideways_thrust_time -= mouse_axis[kc_mouse[17].value];
	}

//----------- Read bank_time -----------------

	if ( bank_on )	{
		k0 = speed_factor*key_down_time( kc_keyboard[4].value );
		k1 = speed_factor*key_down_time( kc_keyboard[5].value );
		k2 = speed_factor*key_down_time( kc_keyboard[6].value );
		k3 = speed_factor*key_down_time( kc_keyboard[7].value );

		// From keyboard...
		if ( kc_keyboard[4].value < 255 ) Controls.bank_time += k0;
		if ( kc_keyboard[5].value < 255 ) Controls.bank_time += k1;
		if ( kc_keyboard[6].value < 255 ) Controls.bank_time -= k2;
		if ( kc_keyboard[7].value < 255 ) Controls.bank_time -= k3;

		// From Cyberman...
		if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
			Controls.bank_time -= mouse_button_down_time(MB_HEAD_LEFT);
			Controls.bank_time += mouse_button_down_time(MB_HEAD_RIGHT);
		}

		// From joystick...
		if ( (use_joystick)&&(kc_joystick[15].value < 255) )	{
			if ( !kc_joystick[16].value )		// If not inverted...
				Controls.bank_time -= (joy_axis[kc_joystick[15].value]*Config_joystick_sensitivity)/8;
			else
				Controls.bank_time += (joy_axis[kc_joystick[15].value]*Config_joystick_sensitivity)/8;
		}
	
		// From mouse...
		if ( (use_mouse)&&(kc_mouse[15].value < 255 ))	{
			if ( !kc_mouse[16].value )		// If not inverted...
				Controls.bank_time += (mouse_axis[kc_mouse[15].value]*Config_joystick_sensitivity)/8;
			else
				Controls.bank_time -= (mouse_axis[kc_mouse[15].value]*Config_joystick_sensitivity)/8;
		}
	}

	// From keyboard...
	if ( kc_keyboard[20].value < 255 ) Controls.bank_time += speed_factor*key_down_time( kc_keyboard[20].value );
	if ( kc_keyboard[21].value < 255 ) Controls.bank_time += speed_factor*key_down_time( kc_keyboard[21].value );
	if ( kc_keyboard[22].value < 255 ) Controls.bank_time -= speed_factor*key_down_time( kc_keyboard[22].value );
	if ( kc_keyboard[23].value < 255 ) Controls.bank_time -= speed_factor*key_down_time( kc_keyboard[23].value );

	// From joystick...
	if ( (use_joystick)&&(kc_joystick[21].value < 255) )	{
		if ( !kc_joystick[22].value )		// If not inverted...
			Controls.bank_time -= joy_axis[kc_joystick[21].value];
		else
			Controls.bank_time += joy_axis[kc_joystick[21].value];
	}

	// From joystick buttons
	if ( (use_joystick)&&(kc_joystick[11].value < 255 )) Controls.bank_time += joy_get_button_down_time( kc_joystick[11].value );
	if ( (use_joystick)&&(kc_joystick[12].value < 255 )) Controls.bank_time -= joy_get_button_down_time( kc_joystick[12].value );

	// From mouse buttons
	if ( (use_mouse)&&(kc_mouse[11].value < 255 )) Controls.bank_time += mouse_button_down_time( kc_mouse[11].value );
	if ( (use_mouse)&&(kc_mouse[12].value < 255 )) Controls.bank_time -= mouse_button_down_time( kc_mouse[12].value );

	// From mouse...
	if ( (use_mouse)&&(kc_mouse[21].value < 255 ))	{
		if ( !kc_mouse[22].value )		// If not inverted...
			Controls.bank_time += mouse_axis[kc_mouse[21].value];
		else
			Controls.bank_time -= mouse_axis[kc_mouse[21].value];
	}

	// From Cyberman
	if ((use_mouse)&&(Config_control_type==CONTROL_CYBERMAN))	{
		Controls.bank_time += mouse_button_down_time(MB_BANK_LEFT);
		Controls.bank_time -= mouse_button_down_time(MB_BANK_RIGHT);
	}

//----------- Read forward_thrust_time -------------

	// From keyboard...
	if ( kc_keyboard[30].value < 255 ) Controls.forward_thrust_time += speed_factor*key_down_time( kc_keyboard[30].value );
	if ( kc_keyboard[31].value < 255 ) Controls.forward_thrust_time += speed_factor*key_down_time( kc_keyboard[31].value );
	if ( kc_keyboard[32].value < 255 ) Controls.forward_thrust_time -= speed_factor*key_down_time( kc_keyboard[32].value );
	if ( kc_keyboard[33].value < 255 ) Controls.forward_thrust_time -= speed_factor*key_down_time( kc_keyboard[33].value );

	// From joystick...
	if ( (use_joystick)&&(kc_joystick[23].value < 255 ))	{
		if ( !kc_joystick[24].value )		// If not inverted...
			Controls.forward_thrust_time -= joy_axis[kc_joystick[23].value];
		else
			Controls.forward_thrust_time += joy_axis[kc_joystick[23].value];
	}

	// From joystick buttons
	if ( (use_joystick)&&(kc_joystick[2].value < 255 )) Controls.forward_thrust_time += joy_get_button_down_time( kc_joystick[2].value );
	if ( (use_joystick)&&(kc_joystick[3].value < 255 )) Controls.forward_thrust_time -= joy_get_button_down_time( kc_joystick[3].value );

	// From mouse...
	if ( (use_mouse)&&(kc_mouse[23].value < 255 ))	{
		if ( !kc_mouse[24].value )		// If not inverted...
			Controls.forward_thrust_time -= mouse_axis[kc_mouse[23].value];
		else
			Controls.forward_thrust_time += mouse_axis[kc_mouse[23].value];
	}

	// From mouse buttons
	if ( (use_mouse)&&(kc_mouse[2].value < 255 )) Controls.forward_thrust_time += mouse_button_down_time( kc_mouse[2].value );
	if ( (use_mouse)&&(kc_mouse[3].value < 255 )) Controls.forward_thrust_time -= mouse_button_down_time( kc_mouse[3].value );

//----------- Read fire_primary_down_count
	if (kc_keyboard[24].value < 255 ) Controls.fire_primary_down_count += key_down_count(kc_keyboard[24].value);
	if (kc_keyboard[25].value < 255 ) Controls.fire_primary_down_count += key_down_count(kc_keyboard[25].value);
	if ((use_joystick)&&(kc_joystick[0].value < 255 )) Controls.fire_primary_down_count += joy_get_button_down_cnt(kc_joystick[0].value);
	if ((use_mouse)&&(kc_mouse[0].value < 255 )) Controls.fire_primary_down_count += mouse_button_down_count(kc_mouse[0].value);

//----------- Read fire_primary_state
	if (kc_keyboard[24].value < 255 ) Controls.fire_primary_state |= keyd_pressed[kc_keyboard[24].value];
	if (kc_keyboard[25].value < 255 ) Controls.fire_primary_state |= keyd_pressed[kc_keyboard[25].value];
	if ((use_joystick)&&(kc_joystick[0].value < 255 )) Controls.fire_primary_state |= joy_get_button_state(kc_joystick[0].value);
	if ((use_mouse)&&(kc_mouse[0].value < 255) ) Controls.fire_primary_state |= mouse_button_state(kc_mouse[0].value);

//----------- Read fire_secondary_down_count
	if (kc_keyboard[26].value < 255 ) Controls.fire_secondary_down_count += key_down_count(kc_keyboard[26].value);
	if (kc_keyboard[27].value < 255 ) Controls.fire_secondary_down_count += key_down_count(kc_keyboard[27].value);
	if ((use_joystick)&&(kc_joystick[1].value < 255 )) Controls.fire_secondary_down_count += joy_get_button_down_cnt(kc_joystick[1].value);
	if ((use_mouse)&&(kc_mouse[1].value < 255 )) Controls.fire_secondary_down_count += mouse_button_down_count(kc_mouse[1].value);

//----------- Read fire_secondary_state
	if (kc_keyboard[26].value < 255 ) Controls.fire_secondary_state |= keyd_pressed[kc_keyboard[26].value];
	if (kc_keyboard[27].value < 255 ) Controls.fire_secondary_state |= keyd_pressed[kc_keyboard[27].value];
	if ((use_joystick)&&(kc_joystick[1].value < 255 )) Controls.fire_secondary_state |= joy_get_button_state(kc_joystick[1].value);
	if ((use_mouse)&&(kc_mouse[1].value < 255) ) Controls.fire_secondary_state |= mouse_button_state(kc_mouse[1].value);

//----------- Read fire_flare_down_count
	if (kc_keyboard[28].value < 255 ) Controls.fire_flare_down_count += key_down_count(kc_keyboard[28].value);
	if (kc_keyboard[29].value < 255 ) Controls.fire_flare_down_count += key_down_count(kc_keyboard[29].value);
	if ((use_joystick)&&(kc_joystick[4].value < 255 )) Controls.fire_flare_down_count += joy_get_button_down_cnt(kc_joystick[4].value);
	if ((use_mouse)&&(kc_mouse[4].value < 255 )) Controls.fire_flare_down_count += mouse_button_down_count(kc_mouse[4].value);

//----------- Read drop_bomb_down_count
	if (kc_keyboard[34].value < 255 ) Controls.drop_bomb_down_count += key_down_count(kc_keyboard[34].value);
	if (kc_keyboard[35].value < 255 ) Controls.drop_bomb_down_count += key_down_count(kc_keyboard[35].value);
	if ((use_joystick)&&(kc_joystick[26].value < 255 )) Controls.drop_bomb_down_count += joy_get_button_down_cnt(kc_joystick[26].value);
	if ((use_mouse)&&(kc_mouse[26].value < 255 )) Controls.drop_bomb_down_count += mouse_button_down_count(kc_mouse[26].value);

//----------- Read rear_view_down_count
	if (kc_keyboard[36].value < 255 ) Controls.rear_view_down_count += key_down_count(kc_keyboard[36].value);
	if (kc_keyboard[37].value < 255 ) Controls.rear_view_down_count += key_down_count(kc_keyboard[37].value);
	if ((use_joystick)&&(kc_joystick[25].value < 255 )) Controls.rear_view_down_count += joy_get_button_down_cnt(kc_joystick[25].value);
	if ((use_mouse)&&(kc_mouse[25].value < 255 )) Controls.rear_view_down_count += mouse_button_down_count(kc_mouse[25].value);

//----------- Read rear_view_down_state
	if (kc_keyboard[36].value < 255 ) Controls.rear_view_down_state |= keyd_pressed[kc_keyboard[36].value];
	if (kc_keyboard[37].value < 255 ) Controls.rear_view_down_state |= keyd_pressed[kc_keyboard[37].value];
	if ((use_joystick)&&(kc_joystick[25].value < 255 )) Controls.rear_view_down_state |= joy_get_button_state(kc_joystick[25].value);
	if ((use_mouse)&&(kc_mouse[25].value < 255 )) Controls.rear_view_down_state |= mouse_button_state(kc_mouse[25].value);

//----------- Read automap_down_count
	if (kc_keyboard[44].value < 255 ) Controls.automap_down_count += key_down_count(kc_keyboard[44].value);
	if (kc_keyboard[45].value < 255 ) Controls.automap_down_count += key_down_count(kc_keyboard[45].value);

//----------- Read automap_state
	if (kc_keyboard[44].value < 255 ) Controls.automap_state |= keyd_pressed[kc_keyboard[44].value];
	if (kc_keyboard[45].value < 255 ) Controls.automap_state |= keyd_pressed[kc_keyboard[45].value];

//----------- Read stupid-cruise-control-type of throttle.
	{
		if ( kc_keyboard[38].value < 255 ) Cruise_speed += fixdiv(speed_factor*key_down_time(kc_keyboard[38].value)*5,FrameTime);
		if ( kc_keyboard[39].value < 255 ) Cruise_speed += fixdiv(speed_factor*key_down_time(kc_keyboard[39].value)*5,FrameTime);
		if ( kc_keyboard[40].value < 255 ) Cruise_speed -= fixdiv(speed_factor*key_down_time(kc_keyboard[40].value)*5,FrameTime);
		if ( kc_keyboard[41].value < 255 ) Cruise_speed -= fixdiv(speed_factor*key_down_time(kc_keyboard[41].value)*5,FrameTime);
		if ( (kc_keyboard[42].value < 255) && (key_down_count(kc_keyboard[42].value)) )
			Cruise_speed = 0;
		if ( (kc_keyboard[43].value < 255) && (key_down_count(kc_keyboard[43].value)) )
			Cruise_speed = 0;
	
		if (Cruise_speed > i2f(100) ) Cruise_speed = i2f(100);
		if (Cruise_speed < 0 ) Cruise_speed = 0;
	
		if (Controls.forward_thrust_time==0)
			Controls.forward_thrust_time = fixmul(Cruise_speed,FrameTime)/100;
	}

	read_head_tracker();

	// Read external controls
	if (kc_use_external_control)
		kconfig_read_external_controls();

//----------- Clamp values between -FrameTime and FrameTime
	if (FrameTime > F1_0 )
		mprintf( (1, "Bogus frame time of %.2f seconds\n", f2fl(FrameTime) ));

	if (Controls.pitch_time > FrameTime/2 ) Controls.pitch_time = FrameTime/2;
	if (Controls.vertical_thrust_time > FrameTime ) Controls.vertical_thrust_time = FrameTime;
	if (Controls.heading_time > FrameTime ) Controls.heading_time = FrameTime;
	if (Controls.sideways_thrust_time > FrameTime ) Controls.sideways_thrust_time = FrameTime;
	if (Controls.bank_time > FrameTime ) Controls.bank_time = FrameTime;
	if (Controls.forward_thrust_time > FrameTime ) Controls.forward_thrust_time = FrameTime;
//	if (Controls.afterburner_time > FrameTime ) Controls.afterburner_time = FrameTime;

	if (Controls.pitch_time < -FrameTime/2 ) Controls.pitch_time = -FrameTime/2;
	if (Controls.vertical_thrust_time < -FrameTime ) Controls.vertical_thrust_time = -FrameTime;
	if (Controls.heading_time < -FrameTime ) Controls.heading_time = -FrameTime;
	if (Controls.sideways_thrust_time < -FrameTime ) Controls.sideways_thrust_time = -FrameTime;
	if (Controls.bank_time < -FrameTime ) Controls.bank_time = -FrameTime;
	if (Controls.forward_thrust_time < -FrameTime ) Controls.forward_thrust_time = -FrameTime;
//	if (Controls.afterburner_time < -FrameTime ) Controls.afterburner_time = -FrameTime;


//--------- Don't do anything if in debug mode
	#ifndef NDEBUG
	if ( keyd_pressed[KEY_DELETE] )	{
		memset( &Controls, 0, sizeof(control_info) );
	}
	#endif
}

void reset_cruise(void)
{
	Cruise_speed=0;
}


void kc_set_controls()
{
	int i;

	for (i=0; i<NUM_KEY_CONTROLS; i++ )	
		kc_keyboard[i].value = kconfig_settings[0][i];

	if ( (Config_control_type>0) && (Config_control_type<5))	{
		for (i=0; i<NUM_OTHER_CONTROLS; i++ ) {
			kc_joystick[i].value = kconfig_settings[Config_control_type][i];
			if (kc_joystick[i].type == BT_INVERT )	{
				if (kc_joystick[i].value!=1)
					kc_joystick[i].value	= 0;
				kconfig_settings[Config_control_type][i] = kc_joystick[i].value;
			}
		}
	} else if (Config_control_type>4) {
		for (i=0; i<NUM_OTHER_CONTROLS; i++ )	{
			kc_mouse[i].value = kconfig_settings[Config_control_type][i];
			if (kc_mouse[i].type == BT_INVERT )	{
				if (kc_mouse[i].value!=1)
					kc_mouse[i].value	= 0;
				kconfig_settings[Config_control_type][i] = kc_mouse[i].value;
			}
		}
	}
}


// ASCENT: the VFX1 driver was called through a DOS interrupt. The headset
// is not part of the port: kconfig_sense_init and kconfig_center_headset do
// nothing and vfx1_installed stays 0. The original code stays below for
// reference and is not compiled.
void kconfig_center_headset()
{
}

void kconfig_sense_init()
{
}

#if 0	// ASCENT: VFX1 code, not compiled
int SenseStatus1( void )
{
	union  REGS     regs;
	struct SREGS    sregs;
	int function, result, i;

	for( i=MIN_SENSE_FUNCTION; i <= MAX_SENSE_FUNCTION; i++ )	{
		result    = i;
		result   |= (SENSE_DRIVER_STATUS << 8);
		function  = SENSE_DRIVER_STATUS;
		function |= (i << 8);        
		memset( &regs, 0, sizeof(regs));
		memset( &sregs, 0, sizeof(sregs));
		regs.x.ecx = 0;
		regs.x.edx = 0;
		regs.x.ebx = 0;
		regs.x.eax = function;
		int386( SENSE_VECTOR, &regs, &regs );
        
		if( regs.x.eax == result )
			return( function & 0xFF00 );
	}
    
	return( 0 );
}

int SenseGetData( int function, int cls, fix *yaw, fix *pitch, fix *roll, int *buttons )
{
	union  REGS     regs;
	struct SREGS    sregs;
	memset( &regs, 0, sizeof(regs));
	memset( &sregs, 0, sizeof(sregs));
	regs.x.eax = function | GET_DEVICE_DATA;
	regs.x.ebx = 1 | (cls << 8);

	int386x( SENSE_VECTOR, &regs, &regs, &sregs);
    
	*yaw     = (short)(regs.x.ebx & 0xffff);
	*pitch   = (short)(regs.x.ecx & 0xffff);
	*roll    = (short)(regs.x.edx & 0xffff);
	*yaw *= -1;
	*pitch *= -1;
	*roll *= -1;
	*buttons = regs.x.eax & 0x00FF;
    
	return( (int)(regs.x.eax >> 8) );
}

//--unused-- int SenseSetVideo( int function, int cls, int mode )
//--unused-- {
//--unused-- 	union  REGS     regs;
//--unused-- 	struct SREGS    sregs;
//--unused-- 	memset( &regs, 0, sizeof(regs));
//--unused-- 	memset( &sregs, 0, sizeof(sregs));
//--unused-- 	regs.x.eax = function | SET_DEVICE_DATA;
//--unused-- 	regs.x.ebx = 1 | (cls << 8);
//--unused-- 	regs.x.ecx = mode;
//--unused-- 
//--unused-- 	int386x( SENSE_VECTOR, &regs, &regs, &sregs);
//--unused--     
//--unused-- 	return( (int)(regs.x.eax >> 8) );
//--unused-- }

void kconfig_center_headset()
{
	if (vfx1_installed)
		SenseSetZero( sense_function1, DCHTD );                    
//	} else if (iglasses_headset_installed)	{
//	} else if (Victor_headset_installed)   {
//	} else {
//	}

}

int SenseSetZero( int function, int cls )
{
	union  REGS     regs;
	struct SREGS    sregs;
	memset( &regs, 0, sizeof(regs));
	memset( &sregs, 0, sizeof(sregs));
	regs.x.eax = function | SET_ZERO;
	regs.x.ebx = 1 | (cls << 8);

	int386x( SENSE_VECTOR, &regs, &regs, &sregs);
    
	return( (int)(regs.x.eax >> 8) );
}

//--unused-- int SenseReSetZero( int function, int cls )
//--unused-- {
//--unused-- 	union  REGS     regs;
//--unused-- 	struct SREGS    sregs;
//--unused-- 	memset( &regs, 0, sizeof(regs));
//--unused-- 	memset( &sregs, 0, sizeof(sregs));
//--unused-- 	regs.x.eax = function | RESET_ZERO;
//--unused-- 	regs.x.ebx = 1 | (cls << 8);
//--unused-- 
//--unused-- 	int386x( SENSE_VECTOR, &regs, &regs, &sregs);
//--unused--     
//--unused-- 	return( (int)(regs.x.eax >> 8) );
//--unused-- }

void kconfig_sense_init()
{
	int ret;
	fix htd_y,htd_p,htd_r;
	int htd_b;

	sense_function1 = SenseStatus1();
//	printf("Sense installed %x\n", sense_function1 );

	if(sense_function1)	{
		ret = SenseGetData( sense_function1,DCHTD,&htd_y,&htd_p,&htd_r,&htd_b);
		if (!ret)	{
			vfx1_installed = 1;
			printf("%s\n", TXT_USING_VFX1 );
		} else {
			printf("%s\n", TXT_VFX1_ERROR1);
		}
	} else {
		printf( TXT_VFX1_ERROR2 );
	}
}
#endif	// ASCENT: end of the VFX1 code

//------------------------------------------------------------------------
// ASCENT: new, to the end of the file. The key configuration of Descent to
// Undermountain (DTUGAME.EXE 1.3). See docs/re/controls.md.
//
// The executable keeps the Descent 1 code of this file with new tables:
// 62 keyboard items (31 actions with two keys each) at 0x13C190, 27
// joystick items at 0x13C6E4 and 29 mouse items at 0x13C978, each item of
// 22 bytes as kc_item above. The defaults are 6 rows of 64 bytes at
// 0x13C010: row 0 the keyboard, rows 1 to 4 the joysticks, row 5 the
// mouse. kconfig.dat holds the 384 bytes, the control type (0x13BFED) and
// the look sensitivity (0x13BFEF).
//
// The port keeps the table in ascent.cfg (dtuopts.c), not in kconfig.dat,
// with the defaults of the owner of the project for the keyboard, and adds
// a page of 10 hot keys (the executable has the keys 1 to 0 fixed). The
// walk controls (controls.c), the play screen (dtudisp.c), the attack
// (dtucombat.c) and the test keys (dtusoak.c) read the table through
// dtu_action_down() and dtu_action_has_key().

#include "dtudisp.h"		// Dtu_hires, Dtu_immersive
#include "dtufile.h"		// dtu_find_file
#include "dtuopts.h"		// dtu_opts_save
#include "dtuhdui.h"		// dtu_hdui_save, dtu_hdui_restore
#include "dtulog.h"		// ASCENT: dtu_kconfig_log

#define DTU_KC_N_KEYS	64
#define DTU_KC_N_HOT		(2*DTU_N_HOT_ACTIONS)
#define DTU_KC_N_JOY		27
#define DTU_KC_N_MOUSE	29
#define DTU_KC_ROW		64

// The items of the executable (0x13C190, 0x13C6E4, 0x13C978): id, x, y,
// w1, w2, up, down, left, right, text number, type, value.
static kc_item Dtu_kc_keys[DTU_KC_N_KEYS] = {
	{  0, 15, 49, 71, 26, 58,  2, 59,  1, 378, BT_KEY, 255 },
	{  1, 15, 49,100, 26, 59,  3,  0, 18, 378, BT_KEY, 255 },
	{  2, 15, 57, 71, 26,  0,  4, 19,  3, 561, BT_KEY, 255 },
	{  3, 15, 57,100, 26,  1,  5,  2, 20, 561, BT_KEY, 255 },
	{  4, 15, 65, 71, 26,  2,  6, 21,  5, 380, BT_KEY, 255 },
	{  5, 15, 65,100, 26,  3,  7,  4, 22, 380, BT_KEY, 255 },
	{  6, 15, 73, 71, 26,  4,  8, 23,  7, 381, BT_KEY, 255 },
	{  7, 15, 73,100, 26,  5,  9,  6, 28, 381, BT_KEY, 255 },
	{  8, 15, 85, 71, 26,  6, 10, 29,  9, 382, BT_KEY, 255 },
	{  9, 15, 85,100, 26,  7, 11,  8, 39, 382, BT_KEY, 255 },
	{ 10, 15, 93, 71, 26,  8, 12, 40, 11, 383, BT_KEY, 255 },
	{ 11, 15, 93,100, 26,  9, 13, 10, 38, 383, BT_KEY, 255 },
	{ 12, 15,101, 71, 26, 10, 14, 49, 13, 384, BT_KEY, 255 },
	{ 13, 15,101,100, 26, 11, 15, 12, 14, 384, BT_KEY, 255 },
	{ 14, 15,109, 71, 26, 12, 16, 13, 15, 385, BT_KEY, 255 },
	{ 15, 15,109,100, 26, 13, 17, 14, 24, 385, BT_KEY, 255 },
	{ 16, 15,117, 71, 26, 14, 41, 25, 17, 386, BT_KEY, 255 },
	{ 17, 15,117,100, 26, 15, 42, 16, 26, 386, BT_KEY, 255 },
	{ 18,158, 49, 83, 26, 62, 20,  1, 19, 390, BT_KEY, 255 },
	{ 19,158, 49,112, 26, 63, 21, 18,  2, 390, BT_KEY, 255 },
	// ASCENT: 1.3 has the text 56 "(unused)" in 20 and 21 and does not
	// draw them (18 down goes to 22, 3 right to 22, 4 left to 19). The
	// port: Climb Wall (the text number -1 - the action, as the hot key
	// page), and the moves of 3, 4, 18, 19, 22 and 23 go through them.
	{ 20,158, 57, 83, 26, 18, 22,  3, 21, -1-DTU_ACT_CLIMB, BT_KEY, 255 },
	{ 21,158, 57,112, 26, 19, 23, 20,  4, -1-DTU_ACT_CLIMB, BT_KEY, 255 },
	{ 22,158, 65, 83, 26, 20, 28,  5, 23, 562, BT_KEY, 255 },
	{ 23,158, 65,112, 26, 21, 29, 22,  6, 562, BT_KEY, 255 },
	{ 24,158,109, 83, 26, 38, 26, 15, 25, 393, BT_KEY, 255 },
	{ 25,158,109,112, 26, 49, 27, 24, 16, 393, BT_KEY, 255 },
	{ 26,158,117, 83, 26, 24, 30, 17, 27, 394, BT_KEY, 255 },
	{ 27,158,117,112, 26, 25, 31, 26, 41, 394, BT_KEY, 255 },
	{ 28,158, 73, 83, 26, 22, 39,  7, 29, 395, BT_KEY, 255 },
	{ 29,158, 73,112, 26, 23, 40, 28,  8, 395, BT_KEY, 255 },
	{ 30,158,125, 83, 26, 26, 32, 42, 31, 556, BT_KEY, 255 },
	{ 31,158,125,112, 26, 27, 33, 30, 43, 556, BT_KEY, 255 },
	{ 32,158,133, 83, 26, 30, 34, 44, 33, 557, BT_KEY, 255 },
	{ 33,158,133,112, 26, 31, 35, 32, 45, 557, BT_KEY, 255 },
	{ 34,158,141, 83, 26, 32, 36, 46, 35, 558, BT_KEY, 255 },
	{ 35,158,141,112, 26, 33, 37, 34, 47, 558, BT_KEY, 255 },
	{ 36,158,149, 83, 26, 34, 54, 48, 37, 559, BT_KEY, 255 },
	{ 37,158,149,112, 26, 35, 55, 36, 50, 559, BT_KEY, 255 },
	{ 38,158, 93, 83, 26, 39, 24, 11, 49, 374, BT_KEY, 255 },
	{ 39,158, 85, 83, 26, 28, 38,  9, 40, 555, BT_KEY, 255 },
	{ 40,158, 85,112, 26, 29, 49, 39, 10, 555, BT_KEY, 255 },
	{ 41, 15,125, 71, 26, 16, 43, 27, 42, 560, BT_KEY, 255 },
	{ 42, 15,125,100, 26, 17, 44, 41, 30, 560, BT_KEY, 255 },
	{ 43, 15,133, 71, 26, 41, 45, 31, 44, 563, BT_KEY, 255 },
	{ 44, 15,133,100, 26, 42, 46, 43, 32, 563, BT_KEY, 255 },
	{ 45, 15,141, 71, 26, 43, 47, 31, 46, 564, BT_KEY, 255 },
	{ 46, 15,141,100, 26, 44, 48, 45, 34, 564, BT_KEY, 255 },
	{ 47, 15,149, 71, 26, 45, 50, 41, 48, 565, BT_KEY, 255 },
	{ 48, 15,149,100, 26, 46, 51, 47, 36, 565, BT_KEY, 255 },
	{ 49,158, 93,112, 26, 40, 25, 38, 12, 374, BT_KEY, 255 },
	{ 50, 15,160, 71, 26, 47, 52, 37, 51, 776, BT_KEY, 255 },
	{ 51, 15,160,100, 26, 48, 53, 50, 54, 776, BT_KEY, 255 },
	{ 52, 15,168, 71, 26, 50, 60, 55, 53, 777, BT_KEY, 255 },
	{ 53, 15,168,100, 26, 51, 61, 52, 56, 777, BT_KEY, 255 },
	{ 54,158,160, 83, 26, 36, 56, 51, 55, 778, BT_KEY, 255 },
	{ 55,158,160,112, 26, 37, 57, 54, 52, 778, BT_KEY, 255 },
	{ 56,158,168, 83, 26, 54, 58, 53, 57, 779, BT_KEY, 255 },
	{ 57,158,168,112, 26, 55, 59, 56, 60, 779, BT_KEY, 255 },
	{ 58,158,176, 83, 26, 56,  0, 61, 59, 780, BT_KEY, 255 },
	{ 59,158,176,112, 26, 57,  1, 58,  0, 780, BT_KEY, 255 },
	{ 60, 15,176, 71, 26, 52, 62, 57, 61, 961, BT_KEY, 255 },
	{ 61, 15,176,100, 26, 53, 63, 60, 58, 961, BT_KEY, 255 },
	// ASCENT: Crouch (port), a row below the items of 1.3: 60 and 61 go
	// down to it, 18 and 19 up to it
	{ 62, 15,184, 71, 26, 60, 18, 59, 63, -1-DTU_ACT_CROUCH, BT_KEY, 255 },
	{ 63, 15,184,100, 26, 61, 19, 62, 58, -1-DTU_ACT_CROUCH, BT_KEY, 255 },
};

static kc_item Dtu_kc_joy[DTU_KC_N_JOY] = {
	{  0, 25, 46, 85, 26, 19,  1, 20,  5, 390, BT_JOY_BUTTON, 255 },
	{  1, 25, 54, 85, 26,  0,  4,  5,  6, 562, BT_JOY_BUTTON, 255 },
	{  2, 25, 78, 85, 26, 26,  3,  8,  9, 393, BT_JOY_BUTTON, 255 },
	{  3, 25, 86, 85, 26,  2, 13,  9, 11, 394, BT_JOY_BUTTON, 255 },
	{  4, 25, 62, 85, 26,  1, 26,  6,  7, 555, BT_JOY_BUTTON, 255 },
	{  5,180, 46, 59, 26, 23,  6,  0,  1, 382, BT_JOY_BUTTON, 255 },
	{  6,180, 54, 59, 26,  5,  7,  1,  4, 383, BT_JOY_BUTTON, 255 },
	{  7,180, 62, 59, 26,  6,  8,  4, 26, 384, BT_JOY_BUTTON, 255 },
	{  8,180, 70, 59, 26,  7,  9, 26,  2, 385, BT_JOY_BUTTON, 255 },
	{  9,180, 78, 59, 26,  8, 11,  2,  3, 386, BT_JOY_BUTTON, 255 },
	{ 10, 25, 94, 85, 26,  3, 13, 11, 13,  56, BT_JOY_BUTTON, 255 },
	{ 11,180, 86, 59, 26,  9, 17,  3, 13, 560, BT_JOY_BUTTON, 255 },
	{ 12,180,106, 59, 26, 11, 18, 11, 13,  56, BT_JOY_BUTTON, 255 },
	{ 13, 22,146, 51, 26,  3, 15,  3, 14, 399, BT_JOY_AXIS, 255 },
	{ 14, 22,146, 99,  8, 13, 16, 13, 17, 399, BT_INVERT, 255 },
	{ 15, 22,154, 51, 26, 13,  5, 18, 16, 400, BT_JOY_AXIS, 255 },
	{ 16, 22,154, 99,  8, 14, 17, 15, 19, 400, BT_INVERT, 255 },
	{ 17,164,146, 58, 26, 11, 19, 14, 18, 401, BT_JOY_AXIS, 255 },
	{ 18,164,146,106,  8, 11, 20, 17, 15, 401, BT_INVERT, 255 },
	{ 19,164,154, 58, 26, 17,  0, 16, 20, 402, BT_JOY_AXIS, 255 },
	{ 20,164,154,106,  8, 18,  0, 19,  0, 402, BT_INVERT, 255 },
	{ 21,164,162, 58, 26, 19, 23, 20, 22,  56, BT_JOY_AXIS, 255 },
	{ 22,164,162,106,  8, 20, 24, 21, 23,  56, BT_INVERT, 255 },
	{ 23,164,174, 58, 26, 21,  5, 22, 24,  56, BT_JOY_AXIS, 255 },
	{ 24,164,174,106,  8, 22, 13, 23,  0,  56, BT_INVERT, 255 },
	{ 25, 25,109, 85, 26,  3, 14, 12, 13,  56, BT_JOY_BUTTON, 255 },
	{ 26, 25, 70, 85, 26,  4,  2,  7,  8, 395, BT_JOY_BUTTON, 255 },
};

static kc_item Dtu_kc_mouse[DTU_KC_N_MOUSE] = {
	{  0, 25, 46, 85, 26, 19,  1, 20,  5, 390, BT_MOUSE_BUTTON, 255 },
	{  1, 25, 54, 85, 26,  0,  4,  5,  6, 561, BT_MOUSE_BUTTON, 255 },
	{  2, 25, 78, 85, 26, 26,  3,  8,  9, 393, BT_MOUSE_BUTTON, 255 },
	{  3, 25, 86, 85, 26,  2, 27,  9, 10, 394, BT_MOUSE_BUTTON, 255 },
	{  4, 25, 62, 85, 26,  1, 26,  6,  7, 562, BT_MOUSE_BUTTON, 255 },
	{  5,180, 46, 59, 26, 19,  6,  0,  1, 382, BT_MOUSE_BUTTON, 255 },
	{  6,180, 54, 59, 26,  5,  7,  1,  4, 383, BT_MOUSE_BUTTON, 255 },
	{  7,180, 62, 59, 26,  6,  8,  4, 26, 384, BT_MOUSE_BUTTON, 255 },
	{  8,180, 70, 59, 26,  7,  9, 26,  2, 385, BT_MOUSE_BUTTON, 255 },
	{  9,180, 78, 59, 26,  8, 10,  2,  3, 386, BT_MOUSE_BUTTON, 255 },
	{ 10,180, 86, 59, 26,  9, 14,  3, 27, 560, BT_MOUSE_BUTTON, 255 },
	{ 11,180, 98, 59, 26, 10, 12,  3, 12,  56, BT_MOUSE_BUTTON, 255 },
	{ 12,180,106, 59, 26, 11,  0, 11, 13,  56, BT_MOUSE_BUTTON, 255 },
	{ 13,103,138, 58, 26, 27, 15, 27, 14, 399, BT_MOUSE_AXIS, 255 },
	{ 14,103,138,106,  8, 10, 16, 13, 15, 399, BT_INVERT, 255 },
	{ 15,103,146, 58, 26, 13, 17, 14, 16, 400, BT_MOUSE_AXIS, 255 },
	{ 16,103,146,106,  8, 14, 18, 15, 17, 400, BT_INVERT, 255 },
	{ 17,103,154, 58, 26, 15, 19, 16, 18, 401, BT_MOUSE_AXIS, 255 },
	{ 18,103,154,106,  8, 16, 20, 17, 19, 401, BT_INVERT, 255 },
	{ 19,103,162, 58, 26, 17,  5, 18, 20, 402, BT_MOUSE_AXIS, 255 },
	{ 20,103,162,106,  8, 18,  0, 19,  0, 402, BT_INVERT, 255 },
	{ 21,103,170, 58, 26, 19, 23, 20, 22,  56, BT_MOUSE_AXIS, 255 },
	{ 22,103,170,106,  8, 20, 24, 21, 23,  56, BT_INVERT, 255 },
	{ 23,103,182, 58, 26, 21, 14, 22, 24,  56, BT_MOUSE_AXIS, 255 },
	{ 24,103,182,106,  8, 22,  5, 23,  0,  56, BT_INVERT, 255 },
	{ 25, 25,109, 85, 26,  3, 13, 12, 13,  56, BT_MOUSE_BUTTON, 255 },
	{ 26, 25, 70, 85, 26,  4,  2,  7,  8, 395, BT_MOUSE_BUTTON, 255 },
	{ 27, 25, 94, 85, 26,  3, 13, 10, 13, 555, BT_MOUSE_BUTTON, 255 },
	{ 28, 25,102, 85, 26, 27, 13, 27, 13,  56, BT_MOUSE_BUTTON, 255 },
};

// The page of the hot keys (port). The items are those of the left and
// right half of the keyboard page: hot keys 1 to 5 on the left, 6 to 10
// on the right, two keys each. The text number is -1 - the action.
// dtu_kc_init() sets the fields.
static kc_item Dtu_kc_hot[DTU_KC_N_HOT];

// The gamepad page (port): the 30 actions of the keyboard page, the
// previous and next hot key and Climb Wall, two bindings each
#define BT_PAD			6
#define DTU_PAD_N_ACTS	(DTU_ACT_HOT_KEY_1 + 4)
static kc_item Dtu_kc_pad[2 * DTU_PAD_N_ACTS];
static int Pad_page_acts[DTU_PAD_N_ACTS];

// The defaults of the executable (0x13C010)
static const ubyte Dtu_kc_orig_defaults[6][DTU_KC_ROW] = {
	{ 0x3a,0xff,0x39,0xff,0xcb,0x4b,0xcd,0x4d,0x38,0xb8,0x2c,0x4f,0x2d,0x51,0xd2,0xff,
	  0xd3,0xff,0x1d,0x9d,0xff,0xff,0x2f,0xff,0xc8,0x48,0xd0,0x50,0x2e,0xff,0xc9,0xff,
	  0xd1,0xff,0xc7,0xff,0xcf,0xff,0x0f,0x1e,0xff,0x2a,0x36,0x17,0xff,0x1f,0xff,0x31,
	  0xff,0xff,0x20,0xff,0x14,0xff,0x32,0xff,0x21,0xff,0x18,0xff,0x13,0xff,0xff,0xff },
	{ 0x00,0xff,0xff,0xff,0x01,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01,0xff,0x00,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff },
	{ 0x05,0xff,0x13,0x0b,0x06,0xff,0x07,0x0f,0xff,0xff,0xff,0xff,0xff,0x01,0x00,0x00,
	  0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x0c,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff },
	{ 0x00,0xff,0x13,0x0b,0x03,0xff,0x07,0x0f,0xff,0xff,0xff,0xff,0xff,0x01,0xff,0x00,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x02,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff },
	{ 0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff },
	{ 0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01,0x00,0x00,
	  0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0x00,0xff,0xff,0x01,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	  0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff },
};

// The keyboard defaults of the port (the owner of the project): W and S
// forward and back (with the arrows), A and D side steps (the executable:
// Z and X), E Activate (A), H Disarm (D), P Stats (S), R and Z Rest, Q
// Climb Wall (a port action in the unused items 20 and 21), Left Ctrl
// Crouch (a port action, items 62 and 63) and Right Ctrl Attack (the
// executable: both Ctrl keys). The other keys are those of the
// executable. The keypad 8 and 2 of the executable gave way to W and S
// (two keys for each action).
static const ubyte Dtu_kc_port_keys[DTU_KC_N_KEYS] = {
	KEY_CAPSLOCK,255,	KEY_SPACEBAR,255,	KEY_LEFT,KEY_PAD4,	KEY_RIGHT,KEY_PAD6,
	KEY_LALT,KEY_RALT,	KEY_A,KEY_PAD1,		KEY_D,KEY_PAD3,		KEY_INSERT,255,
	KEY_DELETE,255,		KEY_RCTRL,255,		KEY_Q,255,			KEY_V,255,
	KEY_W,KEY_UP,			KEY_S,KEY_DOWN,		KEY_C,255,				KEY_PAGEUP,255,
	KEY_PAGEDOWN,255,		KEY_HOME,255,			KEY_END,255,
	KEY_TAB,					// 38 Automap
	KEY_E,255,				// 39, 40 Activate
	KEY_LSHIFT,KEY_RSHIFT,	KEY_I,255,			KEY_P,255,				KEY_N,255,
	255,						// 49 Automap, second key
	KEY_H,255,				KEY_T,255,				KEY_M,255,				KEY_F,255,
	KEY_O,255,				KEY_R,KEY_Z,
	KEY_LCTRL,255,			// 62, 63 Crouch (port)
};

static const ubyte Dtu_kc_hot_keys[DTU_N_HOT_ACTIONS] = { KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0 };

// The settings: rows as in the executable (0 keyboard, 1 to 4 joysticks,
// 5 mouse), and the hot keys of the port.
static ubyte Dtu_kc_settings[6][DTU_KC_ROW];
static ubyte Dtu_kc_hot_settings[DTU_KC_N_HOT];
int Dtu_control_type = 5;		// 0x13BFED: 1 to 4 a joystick, 5 the mouse (the default of 0x71A07)
static int Dtu_kc_ready = 0;

// The two keyboard items of each action (the second -1 for none). The
// actions of the keyboard page are in the order of the items of the
// executable; Automap has the items 38 and 49.
static const signed char Dtu_act_items[DTU_N_ACTIONS][2] = {
	{ 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 8, 9 }, { 10, 11 }, { 12, 13 }, { 14, 15 },
	{ 16, 17 }, { 18, 19 }, { 22, 23 }, { 24, 25 }, { 26, 27 }, { 28, 29 }, { 30, 31 },
	{ 32, 33 }, { 34, 35 }, { 36, 37 }, { 38, 49 }, { 39, 40 }, { 41, 42 }, { 43, 44 },
	{ 45, 46 }, { 47, 48 }, { 50, 51 }, { 52, 53 }, { 54, 55 }, { 56, 57 }, { 58, 59 },
	{ 60, 61 },
	// the hot keys: items of the hot key page, after the keyboard items
	{ 64, 65 }, { 66, 67 }, { 68, 69 }, { 70, 71 }, { 72, 73 }, { 74, 75 }, { 76, 77 },
	{ 78, 79 }, { 80, 81 }, { 82, 83 },
	// the previous and the next hot key: no keyboard items, only the gamepad
	{ -1, -1 }, { -1, -1 },
	// Climb Wall: the items 20 and 21 ("(unused)" in 1.3)
	{ 20, 21 },
	// Crouch: the items 62 and 63 (port)
	{ 62, 63 },
};

// The text numbers of the actions (the hot keys have no text)
static const short Dtu_act_text[DTU_N_ACTIONS] = {
	378, 561, 380, 381, 382, 383, 384, 385, 386, 390, 562, 393, 394, 395, 556, 557,
	558, 559, 374, 555, 560, 563, 564, 565, 776, 777, 778, 779, 780, 961,
};

static char *Dtu_hot_names[DTU_N_ACTIONS - DTU_ACT_HOT_KEY_1] = {
	LN("Hot key 1"), LN("Hot key 2"), LN("Hot key 3"), LN("Hot key 4"), LN("Hot key 5"),
	LN("Hot key 6"), LN("Hot key 7"), LN("Hot key 8"), LN("Hot key 9"), LN("Hot key 10"),
	LN("Prev. hot key"), LN("Next hot key"), LN("Climb Wall"), LN("Crouch"),
};

//------------------------------------------------------------------------
// The gamepad (port, SDL_Gamepad through the platform layer). Each action
// has two bindings: a button (0 to 25, SDL_GamepadButton), a direction of
// a stick or a trigger (PAD_AXIS + 2 * axis, + 1 for the positive
// direction), or 255.

#define PAD_AXIS		0x40
#define PAD_NEG(a)	(PAD_AXIS + 2*(a))
#define PAD_POS(a)	(PAD_AXIS + 2*(a) + 1)

static ubyte Dtu_pad_settings[DTU_N_ACTIONS][2];
int Dtu_pad_dead_zone = 6;			// 0 to 8: 4% each (6: 24%, about the XInput value)
int Dtu_pad_speed = 8;				// 1 to 16: the turn and the look; 8 is the rate of the turn keys
int Dtu_pad_invert_look = 0;

static void dtu_pad_defaults(void)
{
	static const struct { int act; ubyte b0, b1; } def[] = {
		{ DTU_ACT_FORWARD,      PAD_NEG(PLAT_PAD_LEFTY),  255 },
		{ DTU_ACT_BACK,         PAD_POS(PLAT_PAD_LEFTY),  255 },
		{ DTU_ACT_SLIDE_LEFT,   PAD_NEG(PLAT_PAD_LEFTX),  255 },
		{ DTU_ACT_SLIDE_RIGHT,  PAD_POS(PLAT_PAD_LEFTX),  255 },
		{ DTU_ACT_TURN_LEFT,    PAD_NEG(PLAT_PAD_RIGHTX), 255 },
		{ DTU_ACT_TURN_RIGHT,   PAD_POS(PLAT_PAD_RIGHTX), 255 },
		{ DTU_ACT_LOOK_UP,      PAD_NEG(PLAT_PAD_RIGHTY), 255 },
		{ DTU_ACT_LOOK_DOWN,    PAD_POS(PLAT_PAD_RIGHTY), 255 },
		{ DTU_ACT_JUMP,         PLAT_PAD_A,               255 },
		{ DTU_ACT_ATTACK,       PLAT_PAD_X,               PAD_POS(PLAT_PAD_RTRIGGER) },
		{ DTU_ACT_ACTIVATE,     PLAT_PAD_B,               255 },
		{ DTU_ACT_CYCLE_WEAPON, PLAT_PAD_Y,               255 },
		{ DTU_ACT_RUN,          PAD_POS(PLAT_PAD_LTRIGGER), 255 },
		{ DTU_ACT_AUTO_RUN,     PLAT_PAD_LSTICK,          255 },
		{ DTU_ACT_CLIMB,        PLAT_PAD_RSTICK,          255 },	// Quick Down until the climb key
		{ DTU_ACT_AUTOMAP,      PLAT_PAD_BACK,            255 },
		{ DTU_ACT_INVENTORY,    PLAT_PAD_DPAD_UP,         255 },
		{ DTU_ACT_REST,         PLAT_PAD_DPAD_DOWN,       255 },
		{ DTU_ACT_FIND_TRAPS,   PLAT_PAD_DPAD_LEFT,       255 },
		{ DTU_ACT_STATS,        PLAT_PAD_DPAD_RIGHT,      255 },
		{ DTU_ACT_PREV_HOT_KEY, PLAT_PAD_LSHOULDER,       255 },
		{ DTU_ACT_NEXT_HOT_KEY, PLAT_PAD_RSHOULDER,       255 },
	};
	int i;

	memset(Dtu_pad_settings, 255, sizeof(Dtu_pad_settings));
	for (i = 0; i < (int) (sizeof(def) / sizeof(def[0])); i++) {
		Dtu_pad_settings[def[i].act][0] = def[i].b0;
		Dtu_pad_settings[def[i].act][1] = def[i].b1;
	}
	Dtu_pad_dead_zone = 6;
	Dtu_pad_speed = 8;
	Dtu_pad_invert_look = 0;
}

static int pad_is_analog(int act)
{
	switch (act) {
	case DTU_ACT_FORWARD: case DTU_ACT_BACK: case DTU_ACT_SLIDE_LEFT: case DTU_ACT_SLIDE_RIGHT:
	case DTU_ACT_TURN_LEFT: case DTU_ACT_TURN_RIGHT: case DTU_ACT_LOOK_UP: case DTU_ACT_LOOK_DOWN:
	case DTU_ACT_FLOAT_UP: case DTU_ACT_FLOAT_DOWN:
		return 1;
	}
	return 0;
}

// The amount of one binding, 0 to F1_0, after the dead zone. invert turns
// the direction of a stick.
static fix pad_binding_amount(int b, int invert)
{
	int axis, v, dz;

	if (b == 255)
		return 0;
	if (b < PAD_AXIS)
		return plat_pad_button(b) ? F1_0 : 0;
	axis = (b - PAD_AXIS) / 2;
	v = plat_pad_axis(axis);
	if (((b - PAD_AXIS) & 1) == invert)
		v = -v;							// the negative direction
	dz = Dtu_pad_dead_zone * 32768 * 4 / 100;
	if (v <= dz)
		return 0;
	if (v > 32767) v = 32767;
	return (fix) (((int64_t) (v - dz) << 16) / (32767 - dz));
}

fix dtu_pad_amount(int act)
{
	fix a = 0, b;
	int w, inv;

	if (act < 0 || act >= DTU_N_ACTIONS || !plat_pad_present())
		return 0;
	inv = Dtu_pad_invert_look && (act == DTU_ACT_LOOK_UP || act == DTU_ACT_LOOK_DOWN);
	for (w = 0; w < 2; w++) {
		b = pad_binding_amount(Dtu_pad_settings[act][w], inv);
		if (b > a) a = b;
	}
	if (act == DTU_ACT_TURN_LEFT || act == DTU_ACT_TURN_RIGHT || act == DTU_ACT_LOOK_UP || act == DTU_ACT_LOOK_DOWN)
		a = (fix) (((int64_t) a * Dtu_pad_speed) / 8);
	return a;
}

// 1 if a gamepad binding of the action is down: a button, or for an action
// that is not analog a stick or trigger past one half
static int pad_down(int act)
{
	int w;

	if (act < 0 || act >= DTU_N_ACTIONS || !plat_pad_present())
		return 0;
	for (w = 0; w < 2; w++) {
		int b = Dtu_pad_settings[act][w];
		if (b == 255)
			continue;
		if (b < PAD_AXIS) {
			if (plat_pad_button(b))
				return 1;
		} else if (!pad_is_analog(act) && pad_binding_amount(b, 0) > F1_0/2)
			return 1;
	}
	return 0;
}

static int Kc_args_pending;

void dtu_pad_frame(void)
{
	plat_pad_game_frame();
	if (Kc_args_pending)
		dtu_kconfig_args();		// -keyconfig needs the text table
}

static char *dtu_pad_name(int b, char *buf, int size)
{
	static char *buttons[] = { "A", "B", "X", "Y", "BACK", "GUIDE", "START", "LS", "RS", "LB", "RB",
		"DUP", "DDN", "DLT", "DRT" };		// the d-pad: short names for the boxes
	static char *axes[] = { "LX", "LY", "RX", "RY", "LT", "RT" };

	buf[0] = 0;
	if (b == 255)
		return buf;
	if (b < PAD_AXIS) {
		if (b < (int) (sizeof(buttons) / sizeof(buttons[0])))
			snprintf(buf, size, "%s", buttons[b]);
		else
			snprintf(buf, size, "BTN%d", b);
	} else {
		int a = (b - PAD_AXIS) / 2;
		if (a >= PLAT_PAD_LTRIGGER && a < PLAT_PAD_AXES)
			snprintf(buf, size, "%s", axes[a]);		// a trigger
		else if (a < PLAT_PAD_AXES)
			snprintf(buf, size, "%s%c", axes[a], (b & 1) ? '+' : '-');
	}
	return buf;
}

// The system keys of the executable (0x13BFD0) are those of Descent 1:
// Escape, F1 to F12, 0 to 9, minus, equal, Print Screen. The port lets
// the digits be keys: the hot keys are in the table.
static ubyte Dtu_system_keys[] = { KEY_ESC, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12, KEY_MINUS, KEY_EQUAL, KEY_PRINT_SCREEN };

static ubyte *kc_key_value(int item)
{
	if (item < DTU_KC_N_KEYS)
		return &Dtu_kc_settings[0][item];
	return &Dtu_kc_hot_settings[item - DTU_KC_N_KEYS];
}

void dtu_kconfig_defaults(void)
{
	int i;

	memcpy(Dtu_kc_settings, Dtu_kc_orig_defaults, sizeof(Dtu_kc_settings));
	memcpy(Dtu_kc_settings[0], Dtu_kc_port_keys, DTU_KC_N_KEYS);
	for (i = 0; i < DTU_N_HOT_ACTIONS; i++) {
		Dtu_kc_hot_settings[2*i] = Dtu_kc_hot_keys[i];
		Dtu_kc_hot_settings[2*i+1] = 255;
	}
	Dtu_control_type = 5;
	dtu_pad_defaults();
}

// The items of a page of actions in two halves (the hot keys, the gamepad):
// rows of 8 pixels from y 49 in the columns of the keyboard page, two
// items for each action. text_num1 is -1 - the action.
static void kc_build_grid(kc_item *items, const int *acts, int n_acts)
{
	int rows = (n_acts + 1) / 2, n = 2 * n_acts, i;

	for (i = 0; i < n; i++) {
		kc_item *it = &items[i];
		int a = i / 2, c = i & 1, right = a >= rows, row = right ? a - rows : a;
		int col_n = right ? n_acts - rows : rows;		// the actions in this half
		int other = right ? row : row + rows;			// the action at this row in the other half

		if (other >= n_acts)
			other = n_acts - 1;
		it->id = i;
		it->x = right ? 158 : 15;
		it->y = 49 + 8 * row;
		it->w1 = right ? (c ? 112 : 83) : (c ? 100 : 71);
		it->w2 = 26;
		// up and down in the same column, round at the ends
		it->u = (row == 0) ? i + 2 * (col_n - 1) : i - 2;
		it->d = (row == col_n - 1) ? i - 2 * (col_n - 1) : i + 2;
		// left and right along the row, from one half to the other
		it->l = c ? i - 1 : 2 * other + 1;
		it->r = c ? 2 * other : i + 1;
		it->text_num1 = -1 - acts[a];
		it->type = BT_KEY;
		it->value = 255;
	}
}

// ASCENT: D-159. In 1.3 an unbound first key (0xFF) of Attack (items 18,
// 19: 0x538D5 jumps to 0x5390D) and of Cycle Weapon (items 28, 29: 0x53D0E
// jumps to 0x53D3A) skips the test of the second key, so the second key
// works only while a first key is bound. All other pairs test the second
// key. Corrected (1, default): each key of the pair acts alone. -origui
// clears it.
int Dtu_fix_key_pair = 1;

// ASCENT: D-162. In 1.3 the mouse items 8, 9 (Float up, Float down) and
// 19, 20 (Float U/D axis and its invert) are drawn and can be bound, but
// controls_read_all (0x52737) never reads them: no code refers to their
// values (0x13CA3D, 0x13CA53, 0x13CB2F, 0x13CB45; the mouse table is at
// 0x13C978). The joystick items of the same commands are read (0x52B17).
// Corrected (1, default): the mouse items act. -origui clears it.
int Dtu_fix_mouse_float = 1;

// ASCENT: Climb Wall (kconfig.h): read by dtu_hook_use_wall (dtuhook.c).
// With -origui Activate climbs as in 1.3 and the Climb Wall key stays.
int Dtu_fix_climb_key = 1;

static void dtu_kc_init(void)
{
	int i;

	if (Dtu_kc_ready)
		return;
	Dtu_kc_ready = 1;
	dtu_kconfig_defaults();
	if (FindArg("-origui"))
		Dtu_fix_key_pair = Dtu_fix_mouse_float = Dtu_fix_climb_key = 0;

	// The hot key page
	{
		int acts[DTU_N_HOT_ACTIONS];
		for (i = 0; i < DTU_N_HOT_ACTIONS; i++)
			acts[i] = DTU_ACT_HOT_KEY_1 + i;
		kc_build_grid(Dtu_kc_hot, acts, DTU_N_HOT_ACTIONS);
	}
	// The gamepad page: the actions of the keyboard page, the previous and
	// next hot key, Climb Wall and Crouch (DTU_ACT_PREV_HOT_KEY + 2 and + 3)
	for (i = 0; i < DTU_PAD_N_ACTS; i++)
		Pad_page_acts[i] = i < DTU_ACT_HOT_KEY_1 ? i : DTU_ACT_PREV_HOT_KEY + (i - DTU_ACT_HOT_KEY_1);
	kc_build_grid(Dtu_kc_pad, Pad_page_acts, DTU_PAD_N_ACTS);
	for (i = 0; i < 2 * DTU_PAD_N_ACTS; i++)
		Dtu_kc_pad[i].type = BT_PAD;
}

int dtu_action_key(int act, int which)
{
	int item;

	dtu_kc_init();
	if (act < 0 || act >= DTU_N_ACTIONS || which < 0 || which > 1)
		return 255;
	item = Dtu_act_items[act][which];
	if (item < 0)
		return 255;
	return *kc_key_value(item);
}

int dtu_action_has_key(int act, int scancode)
{
	return scancode != 255 && (dtu_action_key(act, 0) == scancode || dtu_action_key(act, 1) == scancode);
}

// The mouse or joystick item of an action, by the text number of the item
static int dtu_button_down(int act)
{
	int i, text;

	if (act < 0 || act >= DTU_ACT_HOT_KEY_1)
		return 0;
	text = Dtu_act_text[act];
	if (Dtu_control_type == 5) {
		// 0x52DE2 to 0x52E13: the attack and the immersive mode button act
		// in both modes; the others (0x52E19) only in the immersive mode.
		// The port: the attack too, as the left button clicks the
		// interface in the other mode.
		if (act != DTU_ACT_IMMERSIVE && !Dtu_immersive)
			return 0;
		for (i = 0; i < DTU_KC_N_MOUSE; i++)
			if (Dtu_kc_mouse[i].type == BT_MOUSE_BUTTON && Dtu_kc_mouse[i].text_num1 == text &&
				(Dtu_fix_mouse_float || (i != 8 && i != 9)) &&		// D-162

				Dtu_kc_settings[5][i] != 255 && mouse_button_state(Dtu_kc_settings[5][i]))
				return 1;
	} else if (Dtu_control_type >= 1 && Dtu_control_type <= 4 && joy_present) {
		for (i = 0; i < DTU_KC_N_JOY; i++)
			if (Dtu_kc_joy[i].type == BT_JOY_BUTTON && Dtu_kc_joy[i].text_num1 == text &&
				Dtu_kc_settings[Dtu_control_type][i] != 255 && joy_get_button_state(Dtu_kc_settings[Dtu_control_type][i]))
				return 1;
	}
	return 0;
}

int dtu_action_down(int act)
{
	int k0 = dtu_action_key(act, 0), k1 = dtu_action_key(act, 1);

	// D-159: -origui, the second key of Attack and Cycle Weapon needs a
	// first key
	if (!Dtu_fix_key_pair && k0 == 255 && (act == DTU_ACT_ATTACK || act == DTU_ACT_CYCLE_WEAPON))
		k1 = 255;
	if ((k0 != 255 && keyd_pressed[k0]) || (k1 != 255 && keyd_pressed[k1]))
		return 1;
	return dtu_button_down(act) || pad_down(act);
}

// The presses of the keys of an action since the last call (key_down_count
// takes them away). A press and a release within one frame count.
int dtu_action_presses(int act)
{
	int k0 = dtu_action_key(act, 0), k1 = dtu_action_key(act, 1), n = 0;

	if (k0 != 255) n += key_down_count(k0);
	if (k1 != 255) n += key_down_count(k1);
	return n;
}

static char *dtu_key_name(int k)
{
	// 0x13BBB8: the names of the executable are those of Descent 1, but
	// for the arrows (words, not arrow characters).
	switch (k) {
	case KEY_UP: return "UP";
	case KEY_DOWN: return "DOWN";
	case KEY_LEFT: return "LEFT";
	case KEY_RIGHT: return "RIGHT";
	}
	if (k < 0 || k > 255 || !key_text[k])		// the table has fewer than 256 names
		return "";
	return key_text[k];
}

char *dtu_action_name(int act)
{
	if (act >= DTU_ACT_HOT_KEY_1 && act < DTU_N_ACTIONS)
		return dtu_tr(Dtu_hot_names[act - DTU_ACT_HOT_KEY_1]);
	if (act >= 0 && act < DTU_N_ACTIONS && Text_string[Dtu_act_text[act]])
		return Text_string[Dtu_act_text[act]];
	return "";
}

// The names of the keys of an action, for example "W or UP". sep is put
// between the names.
char *dtu_action_key_names(int act, char *buf, int size, char *sep)
{
	int k0 = dtu_action_key(act, 0), k1 = dtu_action_key(act, 1);

	buf[0] = 0;
	if (k0 != 255)
		snprintf(buf, size, "%s", dtu_key_name(k0));
	if (k1 != 255)
		snprintf(buf + strlen(buf), size - strlen(buf), "%s%s", buf[0] ? sep : "", dtu_key_name(k1));
	return buf;
}

int dtu_hot_keys_are_default(void)
{
	int i;

	dtu_kc_init();
	for (i = 0; i < DTU_N_HOT_ACTIONS; i++)
		if (Dtu_kc_hot_settings[2*i] != Dtu_kc_hot_keys[i] || Dtu_kc_hot_settings[2*i+1] != 255)
			return 0;
	return 1;
}

// The axes of the mouse (0 x, 1 y) or of the joystick (0 to 3) for the
// look, the turn, the side step and up and down. The mouse is read only
// in the immersive mode (0x52E19). Values: the mouse movement in pixels,
// the joystick position from -128 to 127.
int dtu_control_axes(int *look, int *turn, int *slide, int *updown)
{
	static const short texts[4] = { 399, 400, 401, 402 };
	int *out[4] = { look, turn, slide, updown };
	int raw[4] = { 0, 0, 0, 0 }, i, j, src = 0;

	dtu_kc_init();
	for (j = 0; j < 4; j++)
		*out[j] = 0;
	if (Dtu_control_type == 5) {
		if (!Dtu_immersive)
			return 0;
		mouse_get_delta(&raw[0], &raw[1]);
		src = 1;
		for (i = 0; i < DTU_KC_N_MOUSE; i++)
			for (j = 0; j < 4; j++)
				if (Dtu_kc_mouse[i].type == BT_MOUSE_AXIS && Dtu_kc_mouse[i].text_num1 == texts[j] &&
					(Dtu_fix_mouse_float || i != 19)) {		// D-162
					int v = Dtu_kc_settings[5][i];
					if (v < 2) {
						*out[j] = raw[v];
						if (Dtu_kc_settings[5][i+1] == 1)		// the invert item follows the axis item
							*out[j] = -*out[j];
					}
				}
	} else if (Dtu_control_type >= 1 && Dtu_control_type <= 4 && joy_present) {
		int axis[4];
		ubyte mask = joystick_read_raw_axis(JOY_ALL_AXIS, axis);

		src = 2;
		for (i = 0; i < 4; i++)
			raw[i] = (mask & (1 << i)) ? joy_get_scaled_reading(axis[i], i) : 0;
		for (i = 0; i < DTU_KC_N_JOY; i++)
			for (j = 0; j < 4; j++)
				if (Dtu_kc_joy[i].type == BT_JOY_AXIS && Dtu_kc_joy[i].text_num1 == texts[j]) {
					int v = Dtu_kc_settings[Dtu_control_type][i];
					if (v < 4) {
						*out[j] = raw[v];
						if (Dtu_kc_settings[Dtu_control_type][i+1] == 1)
							*out[j] = -*out[j];
					}
				}
	}
	return src;
}

//------------------------------------------------------------------------
// The screen (0x51170, kconfig_sub of Descent 1). The executable draws it
// in the coordinates of a 320x200 screen; in the high resolution form
// (0x13A94B) each coordinate is doubled (0x50DC7 to 0x50DEB, 0x51668,
// 0x5171E) and the items use the font at 0x13E380 (FNTEMFM_.FNT).

static int Kc_s = 1;					// 2 in the high resolution form
static grs_font *Kc_item_font;
static kc_item *Kc_items;
static int Kc_nitems;
static int Kc_page;						// 0 keyboard, 1 joystick, 2 mouse, 3 hot keys

static ubyte *kc_value(int i)
{
	switch (Kc_page) {
	case 0: return &Dtu_kc_settings[0][i];
	case 1: return &Dtu_kc_settings[Dtu_control_type][i];
	case 2: return &Dtu_kc_settings[5][i];
	case 4: return &Dtu_pad_settings[Pad_page_acts[i / 2]][i & 1];
	default: return &Dtu_kc_hot_settings[i];
	}
}

static char *kc_label(kc_item *it)
{
	if (it->text_num1 < 0)
		return dtu_action_name(-1 - it->text_num1);
	return Text_string[it->text_num1];
}

static int kc_font_h(void)
{
	return grd_curcanv->cv_font->ft_h / Kc_s;
}

static void kc_string(int x, int y, char *s)
{
	gr_string(x == 0x8000 ? x : x * Kc_s, y * Kc_s, s);
}

// 0x51668: a string in a box (x, y, w, h); centered when xoff is 0x8000
static void kc_box_string(char *s, int center, int x, int y, int w, int h)
{
	grs_canvas *save = grd_curcanv, sub;

	if (!s || !s[0] || w <= 0)
		return;
	gr_init_sub_canvas(&sub, save, x * Kc_s, y * Kc_s, w * Kc_s, h * Kc_s);
	gr_set_current_canvas(&sub);
	sub.cv_font = save->cv_font;
	sub.cv_font_fg_color = save->cv_font_fg_color;
	sub.cv_font_bg_color = save->cv_font_bg_color;
	gr_string(center ? 0x8000 : 0, 0, s);
	gr_set_current_canvas(save);
}

static void kc_draw_item(int i, int is_current)
{
	kc_item *it = &Kc_items[i];
	int v = *kc_value(i), h;
	char btext[16];

	// 0x5178A: an item with the text "(unused)" is not drawn
	if (it->text_num1 >= 0 && !strcmp(Text_string[it->text_num1], "(unused)"))
		return;

	grd_curcanv->cv_font = Kc_item_font;
	h = kc_font_h();
	if (is_current)
		gr_set_fontcolor(gr_find_closest_color(0x28,0x28,0x3a), -1);
	else
		gr_set_fontcolor(gr_find_closest_color(0x1e,0x1e,0x30), -1);
	kc_box_string(kc_label(it), 0, it->x, it->y, it->w1 - 1, h);

	btext[0] = 0;
	if (v != 255) {
		switch (it->type) {
		case BT_KEY:				snprintf(btext, sizeof(btext), "%s", dtu_key_name(v)); break;
		case BT_MOUSE_BUTTON:	if (v < 3) snprintf(btext, sizeof(btext), "%s", Text_string[mousebutton_text[v]]); break;
		case BT_MOUSE_AXIS:		if (v < 2) snprintf(btext, sizeof(btext), "%s", Text_string[mouseaxis_text[v]]); break;
		case BT_JOY_BUTTON:
			if (v < 28 && joybutton_text[v] != -1)
				snprintf(btext, sizeof(btext), "%s", Text_string[joybutton_text[v]]);
			else
				snprintf(btext, sizeof(btext), "BTN%d", v);
			break;
		case BT_JOY_AXIS:			if (v < 4) snprintf(btext, sizeof(btext), "%s", Text_string[joyaxis_text[v]]); break;
		case BT_INVERT:			if (v < 2) snprintf(btext, sizeof(btext), "%s", Text_string[invert_text[v]]); break;
		case BT_PAD:				dtu_pad_name(v, btext, sizeof(btext)); break;
		}
	}

	if (is_current)
		gr_setcolor(gr_find_closest_color(0x2a,0,0x30));
	else
		gr_setcolor(gr_find_closest_color(0x20,0,0x26));
	gr_urect((it->x + it->w1) * Kc_s, (it->y - 1) * Kc_s, (it->x + it->w1 + it->w2) * Kc_s, (it->y + h) * Kc_s);

	gr_set_fontcolor(gr_find_closest_color(0x38,0x38,0x38), -1);
	kc_box_string(btext, 1, it->x + it->w1, it->y, it->w2, h);
}

static void kc_draw_question(int i)
{
	static int looper = 0;
	kc_item *it = &Kc_items[i];
	int c = gr_find_closest_color(0x2a,0,0x30), h;

	grd_curcanv->cv_font = Kc_item_font;
	h = kc_font_h();
	gr_setcolor(gr_fade_table[fades[looper]*256+c]);
	if (++looper > 63)
		looper = 0;
	gr_urect((it->x + it->w1) * Kc_s, (it->y - 1) * Kc_s, (it->x + it->w1 + it->w2) * Kc_s, (it->y + h) * Kc_s);
	gr_set_fontcolor(gr_find_closest_color(0x38,0x38,0x38), -1);
	kc_box_string("?", 1, it->x + it->w1, it->y, it->w2, h);
}

// Waits for a new key, button or axis. Returns the value or -1 (Escape).
static int kc_wait_input(int i, char *prompt)
{
	kc_item *it = &Kc_items[i];
	int k = 0, value = -1, n, axis[4], old_axis[4], first = 1;

	grd_curcanv->cv_font = Kc_item_font;
	gr_set_fontcolor(gr_find_closest_color(0x38,0x38,0x38), -1);
	kc_string(0x8000, INFO_Y, prompt);
	game_flush_inputs();
	if (it->type == BT_PAD) {
		// the button that chose the item (A) must go up first
		while (nm_show_frame()) {
			for (n = 0; n < PLAT_PAD_BUTTONS && !plat_pad_button(n); n++)
				;
			if (n == PLAT_PAD_BUTTONS)
				break;
			kc_draw_question(i);
		}
		for (n = 0; n < PLAT_PAD_BUTTONS; n++)
			plat_pad_button_presses(n);
	}
	while (value < 0) {
		k = key_inkey();
		kc_draw_question(i);
		if (it->type == BT_PAD) {
			// a button, or a stick or trigger past three quarters. This
			// comes first: B also gives Escape in the menus.
			for (n = 0; n < PLAT_PAD_BUTTONS && value < 0; n++)
				if (plat_pad_button_presses(n))
					value = n;
			for (n = 0; n < PLAT_PAD_AXES && value < 0; n++) {
				int a = plat_pad_axis(n);
				if (a > 24576) value = PAD_POS(n);
				else if (a < -24576 && n < PLAT_PAD_LTRIGGER) value = PAD_NEG(n);
			}
			if (value >= 0)
				break;
		}
		if (!nm_show_frame() || k == KEY_ESC)
			break;
		switch (it->type) {
		case BT_KEY:
			for (n = 0; n < 256; n++)
				if ((keyd_pressed[n] || n == (k & 0xff)) && n && strlen(dtu_key_name(n)) > 0) {
					int sys = 0, j;
					for (j = 0; j < (int) sizeof(Dtu_system_keys); j++)
						if (Dtu_system_keys[j] == n)
							sys = 1;
					if (!sys)
						value = n;
				}
			break;
		case BT_MOUSE_BUTTON:
			for (n = 0; n < 3; n++)
				if (mouse_button_state(n))
					value = n;
			break;
		case BT_JOY_BUTTON:
			for (n = 0; n < 28; n++)
				if (joy_get_button_state(n))
					value = n;
			break;
		case BT_MOUSE_AXIS: {
			int dx, dy;
			mouse_get_delta(&dx, &dy);
			if (!first && (abs(dx) > 20 || abs(dy) > 20))
				value = abs(dx) > abs(dy) ? 0 : 1;
			break;
		}
		case BT_JOY_AXIS: {
			ubyte mask = joystick_read_raw_axis(JOY_ALL_AXIS, axis);
			if (!first)
				for (n = 0; n < 4; n++)
					if ((mask & (1 << n)) && abs(axis[n] - old_axis[n]) > 20)
						value = n;
			memcpy(old_axis, axis, sizeof(axis));
			break;
		}
		}
		first = 0;
	}
	// clear the line of the prompt
	nm_draw_background(0, (INFO_Y - 1) * Kc_s, grd_curcanv->cv_bitmap.bm_w - 1, (INFO_Y + kc_font_h() + 1) * Kc_s);
	game_flush_inputs();
	return value;
}

static void kc_change(int i)
{
	kc_item *it = &Kc_items[i];
	ubyte *v = kc_value(i);
	int n;

	if (it->type == BT_INVERT) {
		*v = (*v == 1) ? 0 : 1;
		kc_draw_item(i, 1);
		return;
	}
	n = kc_wait_input(i, it->type == BT_PAD ? LT("Press a button or move a stick") :
		it->type == BT_KEY ? TXT_PRESS_NEW_KEY :
		it->type == BT_JOY_BUTTON ? TXT_PRESS_NEW_JBUTTON :
		it->type == BT_MOUSE_BUTTON ? TXT_PRESS_NEW_MBUTTON :
		it->type == BT_JOY_AXIS ? TXT_MOVE_NEW_JOY_AXIS : TXT_MOVE_NEW_MSE_AXIS);
	if (n >= 0) {
		int j;
		// 0x51C29 (Descent 1 kc_change_key): the key leaves each other item
		// that has it. The port: on both keyboard pages.
		if (it->type == BT_KEY) {
			for (j = 0; j < DTU_KC_N_KEYS + DTU_KC_N_HOT; j++) {
				ubyte *o = kc_key_value(j);
				if (o != v && *o == n)
					*o = 255;
			}
		} else
			for (j = 0; j < Kc_nitems; j++)
				if (j != i && Kc_items[j].type == it->type && *kc_value(j) == n)
					*kc_value(j) = 255;
		*v = (ubyte) n;
		for (j = 0; j < Kc_nitems; j++)
			kc_draw_item(j, j == i);
	} else
		kc_draw_item(i, 1);
}

static void kc_draw_page(char *title)
{
	int i;
	char *p;

	nm_draw_background(0, 0, grd_curcanv->cv_bitmap.bm_w, grd_curcanv->cv_bitmap.bm_h);

	grd_curcanv->cv_font = Gamefonts[GFONT_MEDIUM_3];
	p = strchr(title, '\n');
	if (p) *p = 32;
	kc_string(0x8000, 8, title);
	if (p) *p = '\n';

	grd_curcanv->cv_font = Kc_item_font;
	gr_set_fontcolor(gr_find_closest_color(0x38,0x38,0x38), -1);
	kc_string(0x8000, 20, TXT_KCONFIG_STRING_1);

	gr_set_fontcolor(gr_find_closest_color(0x3e,0x36,0x0c), -1);
	gr_setcolor(gr_find_closest_color(0x3e,0x36,0x0c));
	if (Kc_page == 0 || Kc_page == 3 || Kc_page == 4) {
		// 0x50DF8: "or" over the two key columns
		static const short sl[4][3] = { { 98, 106, 42 }, { 120, 128, 42 }, { 253, 261, 42 }, { 274, 283, 42 } };
		static const short px[4] = { 98, 128, 253, 283 };
		for (i = 0; i < 4; i++) {
			gr_scanline(sl[i][0] * Kc_s, sl[i][1] * Kc_s, sl[i][2] * Kc_s);
			gr_pixel(px[i] * Kc_s, 43 * Kc_s);
			gr_pixel(px[i] * Kc_s, 44 * Kc_s);
		}
		kc_string(109, 40, Text_string[816]);
		kc_string(264, 40, Text_string[816]);
	} else {
		// 0x50F34 (joystick) and 0x5105C (mouse)
		int axes_y = Kc_page == 1 ? 129 : 124;
		gr_scanline(18 * Kc_s, 135 * Kc_s, 37 * Kc_s);
		gr_scanline(181 * Kc_s, 294 * Kc_s, 37 * Kc_s);
		gr_scanline(18 * Kc_s, 144 * Kc_s, axes_y * Kc_s);
		gr_scanline(174 * Kc_s, 294 * Kc_s, axes_y * Kc_s);
		kc_string(0x8000, 35, TXT_BUTTONS);
		kc_string(0x8000, axes_y - 2, TXT_AXES);
		gr_set_fontcolor(gr_find_closest_color(0x38,0x38,0x38), -1);
		if (Kc_page == 1) {
			kc_string(81, 137, TXT_AXIS);
			kc_string(111, 137, TXT_INVERT);
			kc_string(222, 137, TXT_AXIS);
			kc_string(252, 137, TXT_INVERT);
		} else {
			kc_string(169, 129, TXT_AXIS);
			kc_string(199, 129, TXT_INVERT);
		}
	}
	for (i = 0; i < Kc_nitems; i++)
		kc_draw_item(i, 0);
}

static void kc_reset_page(void)
{
	int i;

	switch (Kc_page) {
	case 0: memcpy(Dtu_kc_settings[0], Dtu_kc_port_keys, DTU_KC_N_KEYS); break;
	case 1: memcpy(Dtu_kc_settings[Dtu_control_type], Dtu_kc_orig_defaults[Dtu_control_type], DTU_KC_ROW); break;
	case 2: memcpy(Dtu_kc_settings[5], Dtu_kc_orig_defaults[5], DTU_KC_ROW); break;
	case 4: {
		int dz = Dtu_pad_dead_zone, sp = Dtu_pad_speed, inv = Dtu_pad_invert_look;
		dtu_pad_defaults();
		Dtu_pad_dead_zone = dz; Dtu_pad_speed = sp; Dtu_pad_invert_look = inv;
		return;
	}
	default:
		for (i = 0; i < DTU_N_HOT_ACTIONS; i++) {
			Dtu_kc_hot_settings[2*i] = Dtu_kc_hot_keys[i];
			Dtu_kc_hot_settings[2*i+1] = 255;
		}
	}
	// a default key leaves the items of the other keyboard page
	if (Kc_page == 0 || Kc_page == 3) {
		int j;
		for (i = 0; i < Kc_nitems; i++)
			for (j = 0; j < DTU_KC_N_KEYS + DTU_KC_N_HOT; j++) {
				ubyte *o = kc_key_value(j);
				if (o != kc_value(i) && *o == *kc_value(i))
					*o = 255;
			}
	}
}

// The screen under the key screen (dtu_kconfig): its pixels and its
// upscaled pictures.
static grs_bitmap *Kc_saved;
static dtu_hdui_state *Kc_saved_hd;
static int Kc_saved_game;			// the screen had the view of the game

static void kc_screen_save(void)
{
	grs_bitmap *scr = &grd_curscreen->sc_canvas.cv_bitmap;

	Kc_saved = gr_create_bitmap(scr->bm_w, scr->bm_h);
	if (!Kc_saved)
		return;
	gr_bm_ubitblt(scr->bm_w, scr->bm_h, 0, 0, 0, 0, scr, Kc_saved);
	Kc_saved_hd = dtu_hdui_save();
	Kc_saved_game = (Function_mode == FMODE_GAME);
}

static void kc_screen_restore(void)
{
	grs_bitmap *scr = &grd_curscreen->sc_canvas.cv_bitmap;

	if (!Kc_saved)
		return;
	gr_bm_ubitblt(scr->bm_w, scr->bm_h, 0, 0, 0, 0, Kc_saved, scr);
	gr_free_bitmap(Kc_saved);
	Kc_saved = NULL;
	dtu_hdui_restore(Kc_saved_hd);
	Kc_saved_hd = NULL;
	// the GPU image of the view in the pixels of color 255 (gpu.c)
	if (Kc_saved_game)
		dtu_display_view_again();
}

// page: 0 keyboard, 1 joystick, 2 mouse, 3 hot keys, 4 gamepad (port)
void dtu_kconfig(int page, char *title)
{
	grs_canvas *save_canvas = grd_curcanv;
	grs_font *save_font = grd_curcanv->cv_font;
	int old_keyd_repeat = keyd_repeat, citem = 0, ocitem, k;
	int time_stopped = 0;
	static grs_font *hires_font;

	dtu_kc_init();
	Kc_page = page;
	switch (page) {
	case 0: Kc_items = Dtu_kc_keys; Kc_nitems = DTU_KC_N_KEYS; break;
	case 1: Kc_items = Dtu_kc_joy; Kc_nitems = DTU_KC_N_JOY; break;
	case 2: Kc_items = Dtu_kc_mouse; Kc_nitems = DTU_KC_N_MOUSE; break;
	case 4: Kc_items = Dtu_kc_pad; Kc_nitems = 2 * DTU_PAD_N_ACTS; break;
	default: Kc_items = Dtu_kc_hot; Kc_nitems = DTU_KC_N_HOT; Kc_page = 3; break;
	}
	if (page == 1 && (Dtu_control_type < 1 || Dtu_control_type > 4))
		return;

	if (Function_mode == FMODE_GAME) {
		time_stopped = 1;
		stop_time();
	}

	gr_set_current_canvas(NULL);
	// 0x5225D: in the game the executable copies the screen to a canvas
	// before the key screen (0x5226D to 0x52281) and copies it back after
	// it (0x522DB to 0x522EF), so the menus that come back (Controls,
	// Options) are drawn over the play screen, not over the key screen.
	// ASCENT: the port does it in the front end too (its Controls menu is
	// not in the executable), and keeps the upscaled pictures of the
	// screen with it (dtuhdui.c).
	kc_screen_save();
	Kc_s = (grd_curcanv->cv_bitmap.bm_w >= 640) ? 2 : 1;
	Kc_item_font = GAME_FONT;
	if (Kc_s == 2) {
		char path[600];
		if (!hires_font && dtu_find_file("FNTEMFM_.FNT", path, sizeof(path)))
			hires_font = gr_init_font(path);
		if (hires_font)
			Kc_item_font = hires_font;
	}
	game_flush_inputs();
	keyd_repeat = 1;

	kc_draw_page(title);
	kc_draw_item(citem, 1);

	while (1) {
		k = key_inkey();
		if (!nm_show_frame())
			k = KEY_ESC;
		ocitem = citem;
		switch (k) {
		case KEY_PRINT_SCREEN:
			save_screen_shot(0);
			break;
		case KEY_CTRLED+KEY_D:		// 0x514BE
			*kc_value(citem) = 255;
			kc_draw_item(citem, 1);
			break;
		case KEY_CTRLED+KEY_R:		// 0x514E1: the defaults of the page
			kc_reset_page();
			kc_draw_page(title);
			kc_draw_item(citem, 1);
			break;
		case KEY_UP: case KEY_PAD8:		citem = Kc_items[citem].u; break;
		case KEY_DOWN: case KEY_PAD2:		citem = Kc_items[citem].d; break;
		case KEY_LEFT: case KEY_PAD4:		citem = Kc_items[citem].l; break;
		case KEY_RIGHT: case KEY_PAD6:	citem = Kc_items[citem].r; break;
		case KEY_ENTER: case KEY_PADENTER:
			kc_change(citem);
			break;
		case -2:
		case KEY_ESC:
			grd_curcanv->cv_font = save_font;
			kc_screen_restore();
			gr_set_current_canvas(save_canvas);
			keyd_repeat = old_keyd_repeat;
			game_flush_inputs();
			if (time_stopped)
				start_time();
			return;
		}
		if (ocitem != citem) {
			kc_draw_item(ocitem, 0);
			kc_draw_item(citem, 1);
		}
	}
}

//------------------------------------------------------------------------
// The Controls menu (0x50B4F, joydefs_config of Descent 1): the control
// types 1 to 5 of the executable (no "Keyboard only"), "Customize Above",
// a blank line and "Customize Keyboard". The port adds "Customize Hot
// Keys" and "Reset to defaults". The settings are written to ascent.cfg
// when the menu closes.

static void dtu_controls_menuset(int nitems, newmenu_item *items, int *last_key, int citem)
{
	int i;

	for (i = 0; i < 5; i++)
		if (items[i].value)
			Dtu_control_type = i + 1;
}

void dtu_kconfig_menu(void)
{
	newmenu_item m[11];
	int i, c = 7;

	dtu_kc_init();
	plat_pad_init();
	do {
		memset(m, 0, sizeof(m));
		for (i = 0; i < 5; i++) {
			m[i].type = NM_TYPE_RADIO;
			m[i].text = Text_string[143 + i];		// Joystick to Mouse
			m[i].value = (Dtu_control_type == i + 1);
			m[i].group = 0;
		}
		m[5].type = NM_TYPE_MENU; m[5].text = TXT_CUST_ABOVE;
		m[6].type = NM_TYPE_TEXT; m[6].text = "";
		m[7].type = NM_TYPE_MENU; m[7].text = TXT_CUST_KEYBOARD;
		m[8].type = NM_TYPE_MENU; m[8].text = LN("Customize Hot Keys");
		m[9].type = NM_TYPE_MENU; m[9].text = LN("Customize Gamepad");
		m[10].type = NM_TYPE_MENU; m[10].text = LN("Reset to defaults");
		c = newmenu_do1(NULL, TXT_CONTROLS, 11, m, dtu_controls_menuset, c);
		for (i = 0; i < 5; i++)
			if (m[i].value)
				Dtu_control_type = i + 1;
		switch (c) {
		case 5:
			dtu_kconfig(Dtu_control_type == 5 ? 2 : 1, Text_string[142 + Dtu_control_type]);
			break;
		case 7:
			dtu_kconfig(0, TXT_KEYBOARD);
			break;
		case 8:
			dtu_kconfig(3, LT("Hot Keys"));
			break;
		case 9:
			dtu_pad_menu();
			break;
		case 10:
			if (nm_messagebox(NULL, 2, TXT_YES, TXT_NO, "Reset all keys\nto the defaults?") == 0)
				dtu_kconfig_defaults();
			break;
		}
	} while (c > -1);
	dtu_opts_save();
}

//------------------------------------------------------------------------
// The Customize Gamepad menu (port): the bindings, the dead zone, the speed
// of the turn and the look, the inverted look, and the defaults.

static void dtu_pad_menuset(int nitems, newmenu_item *items, int *last_key, int citem)
{
	Dtu_pad_dead_zone = items[2].value;
	Dtu_pad_speed = items[3].value + 1;
	Dtu_pad_invert_look = items[4].value;
}

void dtu_pad_menu(void)
{
	newmenu_item m[7];
	int c = 0;

	dtu_kc_init();
	plat_pad_init();
	do {
		memset(m, 0, sizeof(m));
		m[0].type = NM_TYPE_TEXT; m[0].text = plat_pad_present() ? LN("A gamepad is connected.") : LN("No gamepad found.");
		m[1].type = NM_TYPE_MENU; m[1].text = LN("Customize Buttons and Sticks");
		m[2].type = NM_TYPE_SLIDER; m[2].text = LN("Dead zone"); m[2].value = Dtu_pad_dead_zone; m[2].min_value = 0; m[2].max_value = 8;
		m[3].type = NM_TYPE_SLIDER; m[3].text = LN("Turn and look speed"); m[3].value = Dtu_pad_speed - 1; m[3].min_value = 0; m[3].max_value = 15;
		m[4].type = NM_TYPE_CHECK; m[4].text = LN("Invert look"); m[4].value = Dtu_pad_invert_look;
		m[5].type = NM_TYPE_TEXT; m[5].text = "";
		m[6].type = NM_TYPE_MENU; m[6].text = LN("Reset gamepad to defaults");
		c = newmenu_do1(NULL, "Gamepad", 7, m, dtu_pad_menuset, c < 1 ? 1 : c);
		dtu_pad_menuset(7, m, NULL, c);
		if (c == 1)
			dtu_kconfig(4, LT("Gamepad"));
		else if (c == 6)
			dtu_pad_defaults();
	} while (c > -1);
}

//------------------------------------------------------------------------
// The settings in ascent.cfg (dtuopts.c): "KeyConfigN=" with the 64 bytes
// of row N in hex, "KeyConfigHot=" with the 20 bytes of the hot keys and
// "ControlType=". Returns 1 if the line was a line of the key table.
//
// Climb Wall (bytes 20 and 21 of row 0) is in "KeyConfigClimb=", as a file
// of an earlier version has 0xFF there: KeyConfig0 leaves the two bytes as
// they are. With no KeyConfigClimb line (a file of an earlier version) the
// default key stays, unless the file gives it to another item.
static int Kc_climb_read;

// Crouch (bytes 62 and 63 of row 0, port) is in "KeyConfigCrouch=", which
// the port writes before the rows. A file of an earlier version has no
// such line and has Left Ctrl on Attack (item 18): the key goes to Crouch
// (the default) and Attack keeps Right Ctrl. A Crouch key that the file
// gives to another item leaves Crouch.
static int Kc_crouch_read;

static void kc_crouch_free_key(ubyte *keys, int n, int row0)
{
	int i, w;

	if (Kc_crouch_read)
		return;
	for (i = 0; i < n; i++)
		for (w = 62; w <= 63; w++) {
			if (row0 && (i == 62 || i == 63))
				continue;
			if (keys[i] == 255 || keys[i] != Dtu_kc_settings[0][w])
				continue;
			if (row0 && (i == 18 || i == 19))
				keys[i] = 255;		// Attack gives the key to Crouch
			else
				Dtu_kc_settings[0][w] = 255;
		}
	if (row0 && keys[18] == 255 && keys[19] != 255) {
		keys[18] = keys[19];
		keys[19] = 255;
	}
}

static void kc_climb_free_key(const ubyte *keys, int n, int row0)
{
	int i, w;

	if (Kc_climb_read)
		return;
	for (i = 0; i < n; i++)
		for (w = 20; w <= 21; w++)
			if (!(row0 && (i == 20 || i == 21)) && keys[i] != 255 && keys[i] == Dtu_kc_settings[0][w])
				Dtu_kc_settings[0][w] = 255;
}

static int hex_bytes(const char *s, ubyte *out, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		unsigned v;
		if (sscanf(s + 2*i, "%2x", &v) != 1)
			return 0;
		out[i] = (ubyte) v;
	}
	return 1;
}

int dtu_kconfig_cfg_line(const char *name, const char *value)
{
	ubyte row[DTU_KC_ROW];
	int n;

	dtu_kc_init();
	if (!strcmp(name, "ControlType")) {
		n = atoi(value);
		if (n >= 1 && n <= 5)
			Dtu_control_type = n;
		return 1;
	}
	// ASCENT: Joystick/Mouse Sensitivity (byte 0x13BFEF of kconfig.dat),
	// 1 to 8. A file of an earlier version can have 0 (its options menu
	// stored the slider value): 0 gave no look and no mouse turn, so it
	// becomes 1, the lowest value of the executable.
	if (!strcmp(name, "Sensitivity")) {
		n = atoi(value);
		if (n >= 0 && n <= 8)
			Config_joystick_sensitivity = (ubyte) (n < 1 ? 1 : n);
		return 1;
	}
	if (!strcmp(name, "PadConfig")) {
		// A file of an earlier version has no Climb Wall (the last
		// action). Its R3 on Quick Down (the old default) goes to Climb
		// Wall; an R3 on another action stays there and Climb Wall gets
		// no button; a free R3 goes to Climb Wall (the default).
		ubyte pad[2 * DTU_N_ACTIONS];
		int len = (int) (strlen(value) / 4) * 2;
		if (len > (int) sizeof(pad))
			len = sizeof(pad);
		if (len >= 2 * DTU_ACT_CLIMB && hex_bytes(value, pad, len)) {
			memcpy(Dtu_pad_settings, pad, len);
			// a file with Climb Wall and no Crouch keeps its Climb Wall;
			// Crouch keeps its default (no button)
			if (len < 2 * (DTU_ACT_CLIMB + 1)) {
				int a, w, r3_other = 0;

				for (w = 0; w < 2; w++)
					if (Dtu_pad_settings[DTU_ACT_QUICK_DOWN][w] == PLAT_PAD_RSTICK)
						Dtu_pad_settings[DTU_ACT_QUICK_DOWN][w] = 255;
				for (a = 0; a < DTU_ACT_CLIMB; a++)
					for (w = 0; w < 2; w++)
						if (Dtu_pad_settings[a][w] == PLAT_PAD_RSTICK)
							r3_other = 1;
				Dtu_pad_settings[DTU_ACT_CLIMB][0] = r3_other ? 255 : PLAT_PAD_RSTICK;
				Dtu_pad_settings[DTU_ACT_CLIMB][1] = 255;
			}
		}
		return 1;
	}
	if (!strcmp(name, "PadDeadZone")) {
		n = atoi(value);
		if (n >= 0 && n <= 8) Dtu_pad_dead_zone = n;
		return 1;
	}
	if (!strcmp(name, "PadSpeed")) {
		n = atoi(value);
		if (n >= 1 && n <= 16) Dtu_pad_speed = n;
		return 1;
	}
	if (!strcmp(name, "PadInvertLook")) {
		Dtu_pad_invert_look = atoi(value) != 0;
		return 1;
	}
	if (!strcmp(name, "KeyConfigHot")) {
		if (strlen(value) >= 2*DTU_KC_N_HOT && hex_bytes(value, row, DTU_KC_N_HOT)) {
			memcpy(Dtu_kc_hot_settings, row, DTU_KC_N_HOT);
			kc_climb_free_key(row, DTU_KC_N_HOT, 0);
			kc_crouch_free_key(row, DTU_KC_N_HOT, 0);
		}
		return 1;
	}
	if (!strcmp(name, "KeyConfigCrouch")) {
		if (strlen(value) >= 4 && hex_bytes(value, row, 2)) {
			Dtu_kc_settings[0][62] = row[0];
			Dtu_kc_settings[0][63] = row[1];
			Kc_crouch_read = 1;
		}
		return 1;
	}
	if (!strcmp(name, "KeyConfigClimb")) {
		if (strlen(value) >= 4 && hex_bytes(value, row, 2)) {
			Dtu_kc_settings[0][20] = row[0];
			Dtu_kc_settings[0][21] = row[1];
			Kc_climb_read = 1;
		}
		return 1;
	}
	if (!strncmp(name, "KeyConfig", 9) && name[9] >= '0' && name[9] <= '5' && !name[10]) {
		if (strlen(value) >= 2*DTU_KC_ROW && hex_bytes(value, row, DTU_KC_ROW)) {
			n = name[9] - '0';
			if (n == 0) {
				row[20] = Dtu_kc_settings[0][20];
				row[21] = Dtu_kc_settings[0][21];
				row[62] = Dtu_kc_settings[0][62];
				row[63] = Dtu_kc_settings[0][63];
				kc_crouch_free_key(row, DTU_KC_N_KEYS, 1);
				memcpy(Dtu_kc_settings[0], row, DTU_KC_ROW);
				kc_climb_free_key(row, DTU_KC_N_KEYS, 1);
			} else
				memcpy(Dtu_kc_settings[n], row, DTU_KC_ROW);
		}
		return 1;
	}
	return 0;
}

void dtu_kconfig_cfg_write(FILE *f)
{
	int r, i;

	dtu_kc_init();
	fprintf(f, "ControlType=%d\n", Dtu_control_type);
	fprintf(f, "Sensitivity=%d\n", Config_joystick_sensitivity);
	// before the rows: see kc_crouch_free_key
	fprintf(f, "KeyConfigCrouch=%02x%02x\n", Dtu_kc_settings[0][62], Dtu_kc_settings[0][63]);
	for (r = 0; r < 6; r++) {
		fprintf(f, "KeyConfig%d=", r);
		for (i = 0; i < DTU_KC_ROW; i++)
			fprintf(f, "%02x", Dtu_kc_settings[r][i]);
		fprintf(f, "\n");
	}
	fprintf(f, "KeyConfigHot=");
	for (i = 0; i < DTU_KC_N_HOT; i++)
		fprintf(f, "%02x", Dtu_kc_hot_settings[i]);
	fprintf(f, "\n");
	fprintf(f, "KeyConfigClimb=%02x%02x\n", Dtu_kc_settings[0][20], Dtu_kc_settings[0][21]);
	fprintf(f, "PadConfig=");
	for (i = 0; i < DTU_N_ACTIONS; i++)
		fprintf(f, "%02x%02x", Dtu_pad_settings[i][0], Dtu_pad_settings[i][1]);
	fprintf(f, "\nPadDeadZone=%d\nPadSpeed=%d\nPadInvertLook=%d\n", Dtu_pad_dead_zone, Dtu_pad_speed, Dtu_pad_invert_look);
}

// For the tests: -keyconfig ACTION=KEY[,KEY][;...] sets keys, for example
// -keyconfig "forward=Q;back=Y,DOWN". The names are those of the screen
// (dtu_action_name, ignoring case and spaces) and of the keys.
static int name_eq(const char *a, const char *b, int n)
{
	while (n > 0 && *b) {
		if (*a == ' ') { a++; continue; }
		if (toupper((ubyte) *a) != toupper((ubyte) *b))
			return 0;
		a++; b++; n--;
	}
	while (*a == ' ') a++;
	return n == 0 && *a == 0;
}

void dtu_kconfig_args(void)
{
	int t = FindArg("-keyconfig");
	char *s;

	dtu_kc_init();
	plat_pad_init();			// the gamepad, also for the menus of the front end
	Kc_args_pending = 0;
	if (!t || t + 1 >= Num_args)
		return;
	if (!Text_string[0]) {		// the names of the actions are not loaded yet
		Kc_args_pending = 1;
		return;
	}
	for (s = Args[t+1]; *s; ) {
		char *eq = strchr(s, '='), *end = strchr(s, ';');
		int act, n = 0;

		if (!end) end = s + strlen(s);
		if (!eq || eq > end)
			break;
		for (act = 0; act < DTU_N_ACTIONS; act++)
			if (name_eq(dtu_action_name(act), s, (int) (eq - s)))
				break;
		if (act < DTU_N_ACTIONS) {
			char *k = eq + 1;
			int w;
			for (w = 0; w < 2; w++) {
				int item = Dtu_act_items[act][w];
				*kc_key_value(item) = 255;
			}
			while (k < end && n < 2) {
				char *c = k;
				int key;
				while (c < end && *c != ',') c++;
				for (key = 1; key < 256; key++)
					if (dtu_key_name(key)[0] && name_eq(dtu_key_name(key), k, (int) (c - k)))
						break;
				if (key < 256) {
					int j;
					for (j = 0; j < DTU_KC_N_KEYS + DTU_KC_N_HOT; j++)
						if (*kc_key_value(j) == key)
							*kc_key_value(j) = 255;
					*kc_key_value(Dtu_act_items[act][n++]) = (ubyte) key;
				}
				k = (c < end) ? c + 1 : c;
			}
		} else
			printf("-keyconfig: no action '%.*s'\n", (int) (eq - s), s);
		s = *end ? end + 1 : end;
	}
}

// ASCENT: the control settings in the session log (docs/debug_logs.md):
// the control type, the sensitivity, the keys of the look, turn and
// immersive actions, and the mouse row of the key table (the axes and
// their invert items). The caller writes it at the first frame of the
// controls and after each change.
void dtu_kconfig_log(void)
{
	static const int acts[] = { DTU_ACT_LOOK_UP, DTU_ACT_LOOK_DOWN, DTU_ACT_QUICK_UP, DTU_ACT_QUICK_DOWN,
		DTU_ACT_TURN_LEFT, DTU_ACT_TURN_RIGHT, DTU_ACT_IMMERSIVE, DTU_ACT_SLIDE_ON, DTU_ACT_AUTOMAP };
	static const char *names[] = { "look up", "look down", "quick up", "quick down", "turn left", "turn right",
		"immersive", "slide on", "automap" };
	char row[2 * DTU_KC_ROW + 1];
	int i;

	dtu_kc_init();
	dtulog_event("Controls: control type %d, sensitivity %d, immersive %d, gamepad present %d, dead zone %d, speed %d, invert look %d",
		Dtu_control_type, Config_joystick_sensitivity, Dtu_immersive, plat_pad_present(), Dtu_pad_dead_zone, Dtu_pad_speed, Dtu_pad_invert_look);
	for (i = 0; i < (int) (sizeof(acts) / sizeof(acts[0])); i++) {
		int k0 = dtu_action_key(acts[i], 0), k1 = dtu_action_key(acts[i], 1);
		dtulog_event("Controls: %s: keys 0x%02x '%s' and 0x%02x '%s', gamepad %d %d", names[i],
			k0, dtu_key_name(k0), k1, dtu_key_name(k1), Dtu_pad_settings[acts[i]][0], Dtu_pad_settings[acts[i]][1]);
	}
	dtulog_event("Controls: mouse look axis %d invert %d, turn axis %d invert %d, slide axis %d invert %d, up axis %d invert %d (0 x, 1 y, 255 none)",
		Dtu_kc_settings[5][13], Dtu_kc_settings[5][14], Dtu_kc_settings[5][15], Dtu_kc_settings[5][16],
		Dtu_kc_settings[5][17], Dtu_kc_settings[5][18], Dtu_kc_settings[5][19], Dtu_kc_settings[5][20]);
	for (i = 0; i < DTU_KC_ROW; i++)
		snprintf(row + 2 * i, 3, "%02x", Dtu_kc_settings[5][i]);
	dtulog_event("Controls: KeyConfig5=%s", row);
	for (i = 0; i < DTU_KC_ROW; i++)
		snprintf(row + 2 * i, 3, "%02x", Dtu_kc_settings[0][i]);
	dtulog_event("Controls: KeyConfig0=%s", row);
}
