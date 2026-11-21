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
 * $Source: f:/miner/source/bios/rcs/joyc.c $
 * $Revision: 1.37 $
 * $Author: john $
 * $Date: 1995/10/07 13:22:31 $
 * 
 * Routines for joystick reading.
 * 
 * $Log: joyc.c $
 * Revision 1.37  1995/10/07  13:22:31  john
 * Added new method of reading joystick that allows higher-priority
 * interrupts to go off.
 * 
 * Revision 1.36  1995/03/30  11:03:40  john
 * Made -JoyBios read buttons using BIOS.
 * 
 * Revision 1.35  1995/02/14  11:39:25  john
 * Added polled/bios joystick readers..
 * 
 * Revision 1.34  1995/02/10  17:06:12  john
 * Fixed bug with plugging in a joystick not getting detected.
 * 
 * Revision 1.33  1995/01/27  16:39:42  john
 * Made so that if no joystick detected, it wont't
 * read buttons.
 * 
 * Revision 1.32  1995/01/12  13:16:40  john
 * Made it so that joystick can't lose an axis
 * by 1 weird reading. Reading has to occurr during
 * calibration for this to happen.
 * 
 * Revision 1.31  1994/12/28  15:56:03  john
 * Fixed bug that refused to read joysticks whose 
 * min,cen,max were less than 100 apart.
 * 
 * Revision 1.30  1994/12/28  15:31:53  john
 * Added code to read joystick axis not all at one time.
 * 
 * Revision 1.29  1994/12/27  15:44:36  john
 * Made the joystick timeout be at 1/100th of a second, 
 * regardless of CPU speed.
 * 
 * Revision 1.28  1994/12/04  11:54:54  john
 * Made stick read at whatever rate the clock is at, not
 * at 18.2 times/second.
 * 
 * Revision 1.27  1994/11/29  02:25:40  john
 * Made it so that the scaled reading returns 0 
 * if the calibration factors look funny..
 * 
 * Revision 1.26  1994/11/22  11:08:07  john
 * Commented out the ARCADE joystick.
 * 
 * Revision 1.25  1994/11/14  19:40:26  john
 * Fixed bug with no joystick being detected.
 * 
 * Revision 1.24  1994/11/14  19:36:40  john
 * Took out initial cheapy calibration.
 * 
 * Revision 1.23  1994/11/14  19:13:27  john
 * Took out the calibration in joy_init
 * 
 * Revision 1.22  1994/10/17  10:09:57  john
 * Made the state look at last_State, so that a joy_flush
 * doesn't cause a new down state to be added next reading.
 * 
 * Revision 1.21  1994/10/13  11:36:23  john
 * Made joy_down_time be kept track of in fixed seconds,
 * not ticks.
 * 
 * Revision 1.20  1994/10/12  16:58:50  john
 * Fixed bug w/ previous comment.
 * 
 * Revision 1.19  1994/10/12  16:57:44  john
 * Added function to set a joystick button's state.
 * 
 * Revision 1.18  1994/10/11  10:20:13  john
 * Fixed Flightstick Pro/
 * ..
 * 
 * Revision 1.17  1994/09/29  18:29:20  john
 * *** empty log message ***
 * 
 * Revision 1.16  1994/09/27  19:17:23  john
 * Added code so that is joy_init is never called, joystick is not
 * used at all.
 * 
 * Revision 1.15  1994/09/22  16:09:23  john
 * Fixed some virtual memory lockdown problems with timer and
 * joystick.
 * 
 * Revision 1.14  1994/09/16  11:44:42  john
 * Fixed bug with slow joystick.
 * 
 * Revision 1.13  1994/09/16  11:36:15  john
 * Fixed bug with reading non-present channels.
 * 
 * Revision 1.12  1994/09/15  20:52:48  john
 * rme john
 * Added support for the Arcade style joystick.
 * 
 * Revision 1.11  1994/09/13  20:04:49  john
 * Fixed bug with joystick button down_time.
 * 
 * Revision 1.10  1994/09/10  13:48:07  john
 * Made all 20 buttons read.
 * 
 * Revision 1.9  1994/08/31  09:55:02  john
 * *** empty log message ***
 * 
 * Revision 1.8  1994/08/29  21:02:14  john
 * Added joy_set_cal_values...
 * 
 * Revision 1.7  1994/08/29  20:52:17  john
 * Added better cyberman support; also, joystick calibration
 * value return funcctiionn,
 * 
 * Revision 1.6  1994/08/24  18:53:12  john
 * Made Cyberman read like normal mouse; added dpmi module; moved
 * mouse from assembly to c. Made mouse buttons return time_down.
 * 
 * Revision 1.5  1994/07/14  22:12:23  john
 * Used intrinsic forms of outp to fix vmm error.
 * 
 * Revision 1.4  1994/07/07  19:52:59  matt
 * Made joy_init() return success/fail flag
 * Made joy_init() properly detect a stick if one is plugged in after joy_init()
 * was called the first time.
 * 
 * Revision 1.3  1994/07/01  10:55:55  john
 * Fixed some bugs... added support for 4 axis.
 * 
 * Revision 1.2  1994/06/30  20:36:55  john
 * Revamped joystick code.
 * 
 * Revision 1.1  1994/06/30  15:42:15  john
 * Initial revision
 * 
 * 
 */


static char rcsid[] = "$Id: joyc.c 1.37 1995/10/07 13:22:31 john Exp $";

// ASCENT: the game port code is replaced. The original read the 4 axes
// with timing loops (joy.asm) and read the buttons in the timer interrupt.
// Here the axes come from joy_read_stick_plat (joy.c) and the platform
// layer reports each button and hat change from plat_pump.
//
// Raw axis values: the original values were PIT tick counts, from near 0
// to about 1300 for a common stick. Here a raw value is from 0 to 1023 and
// the center is 512. The calibration code of the game accepts the range.
//
// Buttons: slots 0 to 3 are the 4 buttons of the game port. The hat uses
// the slots of the original (Flightstick Pro and Thrustmaster FCS):
// 19 up, 15 right, 11 down, 7 left. More buttons of the device go to the
// free slots 4,5,6,8,9,10,12,13,14,16,17,18.

#include <stdlib.h>
#include <stdio.h>

#include "types.h"
#include "mono.h"
#include "joy.h"
#include "timer.h"
#include "platform.h"

// In joy.c
int joy_read_stick_plat( int read_masks, int * axis );
int joy_read_buttons_bios();

char joy_installed = 0;
char joy_present = 0;

#define MAX_BUTTONS 20

typedef struct Button_info {
	ubyte		ignore;
	ubyte		state;
	ubyte		last_state;
	fix		timedown;			// ASCENT: fixed point seconds. It was PIT ticks.
	fix		time_went_down;	// ASCENT: new. The time of the last press or the last read.
	ubyte		downcount;
	ubyte		upcount;
} Button_info;

typedef struct Joy_info {
	ubyte			present_mask;
	ubyte			slow_read;
	int			max_timer;
	int			read_count;
	ubyte			last_value;
	Button_info	buttons[MAX_BUTTONS];
	int			axis_min[4];
	int			axis_center[4];
	int			axis_max[4];
} Joy_info;

Joy_info joystick;

extern int joy_bogus_reading;
extern int joy_retries;

void joy_get_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for (i=0; i<4; i++)		{
		axis_min[i] = joystick.axis_min[i];
		axis_center[i] = joystick.axis_center[i];
		axis_max[i] = joystick.axis_max[i];
	}
}

void joy_set_cal_vals(int *axis_min, int *axis_center, int *axis_max)
{
	int i;

	for (i=0; i<4; i++)		{
		joystick.axis_min[i] = axis_min[i];
		joystick.axis_center[i] = axis_center[i];
		joystick.axis_max[i] = axis_max[i];
	}
}


ubyte joy_get_present_mask()	{
	return joystick.present_mask;
}

// ASCENT: the timer rate has no effect on the readings now. The value is kept.
void joy_set_timer_rate(int max_value )	{
	joystick.max_timer = max_value;
}

int joy_get_timer_rate()	{
	return joystick.max_timer;
}


void joy_flush()	{
	int i;

	if (!joy_installed) return;

	for (i=0; i<MAX_BUTTONS; i++ )	{
		joystick.buttons[i].ignore = 0;
		joystick.buttons[i].state = 0;
		joystick.buttons[i].timedown = 0;
		joystick.buttons[i].downcount = 0;
		joystick.buttons[i].upcount = 0;
	}
}

// ASCENT: this is the loop body of the original joy_handler, which the
// timer interrupt called. It now runs once for each change of one button
// slot. The time a button is down is measured with the timer and not
// counted in interrupt periods.
static void joy_button_change( int i, int state )
{
	Button_info * button;

	if ( i < 0 || i >= MAX_BUTTONS ) return;

	button = &joystick.buttons[i];
	if (!button->ignore) {
		if ( button->last_state != state )	{
			if (state)	{
				button->downcount += state;
				button->state = 1;
				button->time_went_down = timer_get_fixed_secondsX();
			} else {
				button->upcount += button->state;
				if (button->state)
					button->timedown += timer_get_fixed_secondsX() - button->time_went_down;
				button->state = 0;
			}
			button->last_state = state;
		}
	}
}

// ASCENT: called by the platform layer. button is the button number of the device.
void joy_button_handler( int button, int down )
{
	static const ubyte extra_slots[] = { 4,5,6,8,9,10,12,13,14,16,17,18 };

	if ( button < 0 ) return;
	if ( button < 4 )
		joy_button_change( button, down );
	else if ( button-4 < (int) sizeof(extra_slots) )
		joy_button_change( extra_slots[button-4], down );
}

// ASCENT: called by the platform layer. hat is a mask of PLAT_HAT_ bits.
void joy_hat_handler( int hat )
{
	joy_button_change( 19, (hat & PLAT_HAT_UP) != 0 );
	joy_button_change( 15, (hat & PLAT_HAT_RIGHT) != 0 );
	joy_button_change( 11, (hat & PLAT_HAT_DOWN) != 0 );
	joy_button_change( 7, (hat & PLAT_HAT_LEFT) != 0 );
}

ubyte joy_read_raw_buttons()	{
	return joy_read_buttons_bios();
}

void joy_set_slow_reading(int flag)
{
	joystick.slow_read |= flag;
	joy_set_cen();
}

ubyte joystick_read_raw_axis( ubyte mask, int * axis )
{
	ubyte read_masks;
	int raw[4];
	int e, i;

	axis[0] = 0; axis[1] = 0;
	axis[2] = 0; axis[3] = 0;

	if (!joy_installed) return 0;

	read_masks = 0;
	mask &= joystick.present_mask;			// Don't read non-present channels
	if ( mask==0 )	{
		return 0;		// Don't read if no stick connected.
	}

	// ASCENT: one read replaces the four methods of the original
	// (assembly timing loop, friendly, polled, BIOS).
	e = joy_read_stick_plat( mask, raw );
	for (i=0; i<4; i++ )	{
		if ( e & (1<<i) ) { axis[i] = raw[i]; read_masks |= (1<<i); }
	}

	return read_masks;
}

int joy_init()
{
	int i;
	int temp_axis[4];

	joy_flush();

	for (i=0; i<MAX_BUTTONS; i++ )
		joystick.buttons[i].last_state = 0;

	if ( !joy_installed )	{
		joy_present = 0;
		joy_installed = 1;
		//joystick.max_timer = 65536;
		joystick.slow_read = 0;
		joystick.read_count = 0;
		joystick.last_value = 0;

		// ASCENT: open the device and get the button and hat events.
		plat_joy_init();
		plat_set_joy_handlers( joy_button_handler, joy_hat_handler );
	}

	// Do initial cheapy calibration...
	joystick.present_mask = JOY_ALL_AXIS;		// Assume they're all present
	do	{
		joystick.present_mask = joystick_read_raw_axis( JOY_ALL_AXIS, temp_axis );
	} while( joy_bogus_reading );

	if ( joystick.present_mask & 3 )
		joy_present = 1;
	else
		joy_present = 0;

	// ASCENT: the original left an axis without calibration values (all 0)
	// until the user calibrated the stick, and such an axis reads 0.
	// The range of a raw value is known here, so an axis that has no
	// usable values gets the full range.
	for (i=0; i<4; i++ )	{
		if ( (joystick.axis_center[i] - joystick.axis_min[i] < 5) || (joystick.axis_max[i] - joystick.axis_center[i] < 5) )	{
			joystick.axis_min[i] = 0;
			joystick.axis_center[i] = 512;
			joystick.axis_max[i] = 1023;
		}
	}

	return joy_present;
}

void joy_close()
{
	if (!joy_installed) return;
	joy_installed = 0;
	plat_set_joy_handlers( NULL, NULL );
	plat_joy_close();
}

void joy_set_ul()
{
	joystick.present_mask = JOY_ALL_AXIS;		// Assume they're all present
	do	{
		joystick.present_mask = joystick_read_raw_axis( JOY_ALL_AXIS, joystick.axis_min );
	} while( joy_bogus_reading );
	if ( joystick.present_mask & 3 )
		joy_present = 1;
	else
		joy_present = 0;
}

void joy_set_lr()
{
	joystick.present_mask = JOY_ALL_AXIS;		// Assume they're all present
	do {
		joystick.present_mask = joystick_read_raw_axis( JOY_ALL_AXIS, joystick.axis_max );
	} while( joy_bogus_reading );

	if ( joystick.present_mask & 3 )
		joy_present = 1;
	else
		joy_present = 0;
}

void joy_set_cen()
{
	joystick.present_mask = JOY_ALL_AXIS;		// Assume they're all present
	do {
		joystick.present_mask = joystick_read_raw_axis( JOY_ALL_AXIS, joystick.axis_center );
	} while( joy_bogus_reading );

	if ( joystick.present_mask & 3 )
		joy_present = 1;
	else
		joy_present = 0;
}

void joy_set_cen_fake(int channel)
{

	int i,n=0;
	int minx, maxx, cenx;

	minx=maxx=cenx=0;

	for (i=0; i<4; i++ )	{
		if ( (joystick.present_mask & (1<<i)) && (i!=channel) )	{
			n++;
			minx += joystick.axis_min[i];
			maxx += joystick.axis_max[i];
			cenx += joystick.axis_center[i];
		}
	}
	// ASCENT: the original divided by zero if no other axis was present.
	if ( n == 0 ) return;
	minx /= n;
	maxx /= n;
	cenx /= n;

	joystick.axis_min[channel] = minx;
	joystick.axis_max[channel] = maxx;
	joystick.axis_center[channel] = cenx;
}

int joy_get_scaled_reading( int raw, int axn )
{
	int x, d;

	// Make sure it's calibrated properly.
	if ( joystick.axis_center[axn] - joystick.axis_min[axn] < 5 ) return 0;
	if ( joystick.axis_max[axn] - joystick.axis_center[axn] < 5 ) return 0;

	raw -= joystick.axis_center[axn];

	if ( raw < 0 )	{
		d = joystick.axis_center[axn]-joystick.axis_min[axn];
	} else {
		d = joystick.axis_max[axn]-joystick.axis_center[axn];
	}

	if ( d )
		x = (raw * 128) / d;		// ASCENT: was raw << 7. The result is the same.
	else
		x = 0;

	if ( x < -128 ) x = -128;
	if ( x > 127 ) x = 127;

	return x;
}

int last_reading[4] = { 0, 0, 0, 0 };

void joy_get_pos( int *x, int *y )
{
	ubyte flags;
	int axis[4];

	if ((!joy_installed)||(!joy_present)) { *x=*y=0; return; }

	flags=joystick_read_raw_axis( JOY_1_X_AXIS+JOY_1_Y_AXIS, axis );

	if ( joy_bogus_reading )	{
		axis[0] = last_reading[0];
		axis[1] = last_reading[1];
		flags = JOY_1_X_AXIS+JOY_1_Y_AXIS;
	} else {
		last_reading[0] = axis[0];
		last_reading[1] = axis[1];
	}

	if ( flags & JOY_1_X_AXIS )
		*x = joy_get_scaled_reading( axis[0], 0 );
	else
		*x = 0;

	if ( flags & JOY_1_Y_AXIS )
		*y = joy_get_scaled_reading( axis[1], 1 );
	else
		*y = 0;
}

ubyte joy_read_stick( ubyte masks, int *axis )
{
	ubyte flags;
	int raw_axis[4];

	if ((!joy_installed)||(!joy_present)) {
		axis[0] = 0; axis[1] = 0;
		axis[2] = 0; axis[3] = 0;
		return 0;
	}

	flags=joystick_read_raw_axis( masks, raw_axis );

	if ( joy_bogus_reading )	{
		axis[0] = last_reading[0];
		axis[1] = last_reading[1];
		axis[2] = last_reading[2];
		axis[3] = last_reading[3];
		flags = masks;
	} else {
		last_reading[0] = axis[0];
		last_reading[1] = axis[1];
		last_reading[2] = axis[2];
		last_reading[3] = axis[3];
	}

	if ( flags & JOY_1_X_AXIS )
		axis[0] = joy_get_scaled_reading( raw_axis[0], 0 );
	else
		axis[0] = 0;

	if ( flags & JOY_1_Y_AXIS )
		axis[1] = joy_get_scaled_reading( raw_axis[1], 1 );
	else
		axis[1] = 0;

	if ( flags & JOY_2_X_AXIS )
		axis[2] = joy_get_scaled_reading( raw_axis[2], 2 );
	else
		axis[2] = 0;

	if ( flags & JOY_2_Y_AXIS )
		axis[3] = joy_get_scaled_reading( raw_axis[3], 3 );
	else
		axis[3] = 0;

	return flags;
}


int joy_get_btns()
{
	if ((!joy_installed)||(!joy_present)) return 0;

	return joy_read_raw_buttons();
}

void joy_get_btn_down_cnt( int *btn0, int *btn1 )
{
	if ((!joy_installed)||(!joy_present)) { *btn0=*btn1=0; return; }

	*btn0 = joystick.buttons[0].downcount;
	joystick.buttons[0].downcount = 0;
	*btn1 = joystick.buttons[1].downcount;
	joystick.buttons[1].downcount = 0;
}

int joy_get_button_state( int btn )
{
	int count;

	if ((!joy_installed)||(!joy_present)) return 0;

	if ( btn < 0 || btn >= MAX_BUTTONS ) return 0;

	count = joystick.buttons[btn].state;

	return  count;
}

int joy_get_button_up_cnt( int btn )
{
	int count;

	if ((!joy_installed)||(!joy_present)) return 0;

	if ( btn < 0 || btn >= MAX_BUTTONS ) return 0;

	count = joystick.buttons[btn].upcount;
	joystick.buttons[btn].upcount = 0;

	return count;
}

int joy_get_button_down_cnt( int btn )
{
	int count;

	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn < 0 || btn >= MAX_BUTTONS ) return 0;

	count = joystick.buttons[btn].downcount;
	joystick.buttons[btn].downcount = 0;

	return count;
}


fix joy_get_button_down_time( int btn )
{
	fix count;
	Button_info * button;

	if ((!joy_installed)||(!joy_present)) return 0;
	if ( btn < 0 || btn >= MAX_BUTTONS ) return 0;

	button = &joystick.buttons[btn];

	// ASCENT: add the time of the press that is still in progress. The
	// original interrupt added it in steps. A slot that the game sets
	// with joy_set_btn_values (ignore) has only the time that the game gave.
	if ( button->state && !button->ignore )	{
		fix time = timer_get_fixed_secondsX();
		button->timedown += time - button->time_went_down;
		button->time_went_down = time;
	}

	count = button->timedown;
	button->timedown = 0;

	return count;
}

void joy_get_btn_up_cnt( int *btn0, int *btn1 )
{
	if ((!joy_installed)||(!joy_present)) { *btn0=*btn1=0; return; }

	*btn0 = joystick.buttons[0].upcount;
	joystick.buttons[0].upcount = 0;
	*btn1 = joystick.buttons[1].upcount;
	joystick.buttons[1].upcount = 0;
}

void joy_set_btn_values( int btn, int state, fix timedown, int downcount, int upcount )
{
	if ( btn < 0 || btn >= MAX_BUTTONS ) return;		// ASCENT: range check

	joystick.buttons[btn].ignore = 1;
	joystick.buttons[btn].state = state;
	joystick.buttons[btn].timedown = timedown;
	joystick.buttons[btn].downcount = downcount;
	joystick.buttons[btn].upcount = upcount;
}

void joy_poll()
{
	if ( joystick.slow_read & JOY_BIOS_READINGS )
		joystick.last_value = joy_read_buttons_bios();
}
