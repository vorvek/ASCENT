/*
 * ASCENT: replacement for joy.asm.
 *
 * The original measured each axis of the game port (port 201h) with a
 * timing loop: it started the one-shots and counted PIT ticks until each
 * axis bit returned to 0. It had four versions of the loop (interrupts
 * off, friendly, polled, BIOS INT 15h). This file reads the axes and the
 * buttons from the platform layer.
 */
#include "types.h"
#include "joy.h"
#include "platform.h"

// The original set this when an interrupt disturbed the timing loop.
// It is always 0 now.
int joy_bogus_reading = 0;
int joy_retries = 0;

// Read the axes in read_masks (bit n for axis n) into axis[0..3].
// Returns the mask of the axes that were read.
// A value is from 0 to 1023 and the center is 512. The original values
// were PIT tick counts in about the same range.
int joy_read_stick_plat( int read_masks, int * axis )
{
	int raw[PLAT_JOY_AXES];
	int present, i, result = 0;

	present = plat_joy_read_axes( raw );
	for (i=0; i<4; i++ )	{
		axis[i] = 0;
		if ( (read_masks & present) & (1<<i) )	{
			axis[i] = (raw[i] + 32768) >> 6;
			result |= (1<<i);
		}
	}
	return result;
}

// The 4 buttons of the game port: bit n is 1 if button n is down.
int joy_read_buttons_bios()
{
	return (int) (plat_joy_read_buttons() & 0xf);
}
