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
 * $Source: f:/miner/source/bios/rcs/mouse.c $
 * $Revision: 1.11 $
 * $Author: john $
 * $Date: 1995/02/10 18:52:17 $
 * 
 * Functions to access Mouse and Cyberman...
 * 
 * $Log: mouse.c $
 * Revision 1.11  1995/02/10  18:52:17  john
 * Fixed bug with mouse not getting closed.
 * 
 * Revision 1.10  1995/02/02  11:10:33  john
 * Changed a bunch of mouse stuff around to maybe get
 * around PS/2 mouse hang.
 * 
 * Revision 1.9  1995/01/14  19:19:52  john
 * Fixed signed short error cmp with -1 that caused mouse
 * to break under Watcom 10.0
 * 
 * Revision 1.8  1994/12/27  12:38:23  john
 * Made mouse use temporary dos buffer instead of
 * 
 * allocating its own.
 * 
 * 
 * Revision 1.7  1994/12/05  23:54:53  john
 * Fixed bug with mouse_get_delta only returning positive numbers..
 * 
 * Revision 1.6  1994/11/18  23:18:18  john
 * Changed some shorts to ints.
 * 
 * Revision 1.5  1994/09/13  12:34:02  john
 * Added functions to get down count and state.
 * 
 * Revision 1.4  1994/08/29  20:52:19  john
 * Added better cyberman support; also, joystick calibration
 * value return funcctiionn,
 * 
 * Revision 1.3  1994/08/24  18:54:32  john
 * *** empty log message ***
 * 
 * Revision 1.2  1994/08/24  18:53:46  john
 * Made Cyberman read like normal mouse; added dpmi module; moved
 * mouse from assembly to c. Made mouse buttons return time_down.
 * 
 * Revision 1.1  1994/08/24  13:56:37  john
 * Initial revision
 * 
 * 
 */


static char rcsid[] = "$Id: mouse.c 1.11 1995/02/10 18:52:17 john Exp $";

// ASCENT: the DOS mouse driver (INT 33h), its event handler and the
// Cyberman (SWIFT) code are replaced. The platform layer calls
// mouse_button_handler and mouse_motion_handler from plat_pump.
// The button logic is the logic of the original event handler.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fix.h"
#include "mouse.h"
#include "timer.h"
#include "platform.h"

#define MOUSE_MAX_BUTTONS	11

typedef struct mouse_info {
	fix		ctime;
	ubyte		cyberman;
	int		num_buttons;
	ubyte		pressed[MOUSE_MAX_BUTTONS];
	fix		time_went_down[MOUSE_MAX_BUTTONS];
	fix		time_held_down[MOUSE_MAX_BUTTONS];
	uint		num_downs[MOUSE_MAX_BUTTONS];
	uint		num_ups[MOUSE_MAX_BUTTONS];
	// ASCENT: the state that the DOS mouse driver held.
	int		x, y;							// position
	int		delta_x, delta_y;			// motion since the last mouse_get_delta
	int		min_x, min_y, max_x, max_y;	// limits of the position
} mouse_info;

static mouse_info Mouse;

static int Mouse_installed = 0;

static void mouse_clip_pos()
{
	if (Mouse.x < Mouse.min_x) Mouse.x = Mouse.min_x;
	if (Mouse.x > Mouse.max_x) Mouse.x = Mouse.max_x;
	if (Mouse.y < Mouse.min_y) Mouse.y = Mouse.min_y;
	if (Mouse.y > Mouse.max_y) Mouse.y = Mouse.max_y;
}

// ASCENT: one button event. The original handler got a mask of events
// from the mouse driver and had this block once for each button.
void mouse_button_handler(int button, int down)
{
	if (button < 0 || button >= MOUSE_MAX_BUTTONS)
		return;

	Mouse.ctime = timer_get_fixed_secondsX();

	if (down)	{	// button pressed
		if (!Mouse.pressed[button])	{
			Mouse.pressed[button] = 1;
			Mouse.time_went_down[button] = Mouse.ctime;
		}
		Mouse.num_downs[button]++;
	} else {  // button released
		if (Mouse.pressed[button])	{
			Mouse.pressed[button] = 0;
			Mouse.time_held_down[button] += Mouse.ctime-Mouse.time_went_down[button];
		}
		Mouse.num_ups[button]++;
	}
}

// ASCENT: one motion event. dx and dy are the motion counts (the mickeys
// of the DOS driver). x and y are the pointer position in frame pixels.
// In relative mode the system pointer is hidden and does not move, so the
// position follows the motion counts inside the limits, as the DOS driver
// did. If not in relative mode, the position is the pointer position.
void mouse_motion_handler(int dx, int dy, int x, int y)
{
	Mouse.delta_x += dx;
	Mouse.delta_y += dy;

	if (plat_mouse_get_relative())	{
		Mouse.x += dx;
		Mouse.y += dy;
	} else {
		Mouse.x = x;
		Mouse.y = y;
	}
	mouse_clip_pos();
}

//--------------------------------------------------------
// returns 0 if no mouse
//           else number of buttons
// ASCENT: there is always a mouse with 3 buttons. enable_cyberman has no
// effect. The call starts the relative mouse mode (see mouse_set_relative).
int mouse_init(int enable_cyberman)
{
	if (Mouse_installed)
		return Mouse.num_buttons;

	Mouse.num_buttons = 3;
	Mouse.cyberman = 0;
	Mouse.x = Mouse.y = 0;
	Mouse.delta_x = Mouse.delta_y = 0;
	// ASCENT: the first limits are the 320x200 frame. The unit is the
	// frame pixel. (The DOS driver had an x range of 0 to 639 in that mode.)
	Mouse.min_x = Mouse.min_y = 0;
	Mouse.max_x = 319;
	Mouse.max_y = 199;

	// Install mouse handler
	plat_set_mouse_handlers( mouse_button_handler, mouse_motion_handler );
	plat_mouse_set_relative( 1 );

	Mouse_installed = 1;

	atexit( mouse_close );

	mouse_flush();

	return Mouse.num_buttons;
}

void mouse_close()
{
	if (Mouse_installed)	{
		Mouse_installed = 0;
		plat_set_mouse_handlers( NULL, NULL );
		plat_mouse_set_relative( 0 );
	}
}

// ASCENT: new function. on=1: the pointer is hidden and held in the window
// (the state after mouse_init, for the game). on=0: the system pointer is
// free and mouse_get_pos returns its position in frame pixels.
void mouse_set_relative( int on )
{
	plat_mouse_set_relative( on );
}

int mouse_set_limits( int x1, int y1, int x2, int y2 )
{
	if (!Mouse_installed) return 0;

	Mouse.min_x = x1;
	Mouse.max_x = x2;
	Mouse.min_y = y1;
	Mouse.max_y = y2;
	mouse_clip_pos();
	return 0;
}

void mouse_get_pos( int *x, int *y)
{
	if (!Mouse_installed) {
		*x = *y = 0;
		return;
	}
	*x = Mouse.x;
	*y = Mouse.y;
}

void mouse_get_delta( int *dx, int *dy )
{
	if (!Mouse_installed) {
		*dx = *dy = 0;
		return;
	}
	// Read Mouse motion counters
	*dx = Mouse.delta_x;
	*dy = Mouse.delta_y;
	Mouse.delta_x = 0;
	Mouse.delta_y = 0;
}

int mouse_get_btns()
{
	int i;
	uint flag=1;
	int status = 0;

	if (!Mouse_installed)
		return 0;

	for (i=0; i<MOUSE_MAX_BUTTONS; i++ )	{
		if (Mouse.pressed[i])
			status |= flag;
		flag <<= 1;
	}
	return status;
}

// ASCENT: this sets the position that mouse_get_pos returns. It does not
// move the system pointer. If not in relative mode, the next motion event
// sets the position to the system pointer again.
void mouse_set_pos( int x, int y)
{
	if (!Mouse_installed)
		return;

	Mouse.x = x;
	Mouse.y = y;
	mouse_clip_pos();
}

void mouse_flush()
{
	int i;
	fix CurTime;

	if (!Mouse_installed)
		return;

	//Clear the mouse data
	CurTime =timer_get_fixed_secondsX();
	for (i=0; i<MOUSE_MAX_BUTTONS; i++ )	{
		Mouse.pressed[i] = 0;
		Mouse.time_went_down[i] = CurTime;
		Mouse.time_held_down[i] = 0;
		Mouse.num_downs[i]=0;
		Mouse.num_ups[i]=0;
	}
}


// Returns how many times this button has went down since last call.
int mouse_button_down_count(int button)
{
	int count;

	if (!Mouse_installed)
		return 0;
	if (button < 0 || button >= MOUSE_MAX_BUTTONS)		// ASCENT: range check
		return 0;

	count = Mouse.num_downs[button];
	Mouse.num_downs[button]=0;

	return count;
}

// Returns 1 if this button is currently down
int mouse_button_state(int button)
{
	int state;

	if (!Mouse_installed)
		return 0;
	if (button < 0 || button >= MOUSE_MAX_BUTTONS)		// ASCENT: range check
		return 0;

	state = Mouse.pressed[button];

	return state;
}



// Returns how long this button has been down since last call.
fix mouse_button_down_time(int button)
{
	fix time_down, time;

	if (!Mouse_installed)
		return 0;
	if (button < 0 || button >= MOUSE_MAX_BUTTONS)		// ASCENT: range check
		return 0;

	if ( !Mouse.pressed[button] )	{
		time_down = Mouse.time_held_down[button];
		Mouse.time_held_down[button] = 0;
	} else	{
		time = timer_get_fixed_secondsX();
		time_down =  time - Mouse.time_went_down[button];
		Mouse.time_went_down[button] = time;
	}

	return time_down;
}

// ASCENT: there is no Cyberman.
void mouse_get_cyberman_pos( int *x, int *y )
{
	*x = *y = 0;
}
