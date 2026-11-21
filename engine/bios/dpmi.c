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
 * $Source: f:/miner/source/bios/rcs/dpmi.c $
 * $Revision: 1.19 $
 * $Author: john $
 * $Date: 1995/02/23 09:02:57 $
 * 
 * Routines that access DPMI services...
 * 
 * $Log: dpmi.c $
 * Revision 1.19  1995/02/23  09:02:57  john
 * Fixed bug with dos_selector.
 * 
 * Revision 1.18  1995/02/02  11:10:22  john
 * Made real mode calls have a 2K stack.
 * 
 * Revision 1.17  1995/01/14  19:20:28  john
 * Added function to set a selector's base address.
 * 
 * Revision 1.16  1994/12/14  16:11:40  john
 * Locked down the memory referenced by GETDS.
 * 
 * Revision 1.15  1994/12/06  16:08:06  john
 * MAde memory checking return better results.
 * 
 * Revision 1.14  1994/12/05  23:34:54  john
 * Made dpmi_init lock down GETDS and chain_intr.
 * 
 * Revision 1.13  1994/11/28  21:19:02  john
 * Made memory checking a bit better.
 * 
 * Revision 1.12  1994/11/28  20:22:18  john
 * Added some variables that return the amount of available 
 * memory.
 * 
 * Revision 1.11  1994/11/15  18:27:21  john
 * *** empty log message ***
 * 
 * Revision 1.10  1994/11/15  18:26:45  john
 * Added verbose flag.
 * 
 * Revision 1.9  1994/10/27  19:54:37  john
 * Added unlock region function,.
 * 
 * Revision 1.8  1994/10/05  16:17:31  john
 * Took out locked down message.
 * 
 * Revision 1.7  1994/10/03  17:21:20  john
 * Added the code that allocates a 1K DOS buffer.
 * 
 * Revision 1.6  1994/09/29  18:29:40  john
 * Shorted mem info printout
 * 
 * Revision 1.5  1994/09/27  11:54:35  john
 * Added DPMI init function.
 * 
 * Revision 1.4  1994/09/19  14:50:43  john
 * Took out mono debug.
 * 
 * Revision 1.3  1994/09/19  14:41:23  john
 * Fixed some bugs with allocating selectors.
 * 
 * Revision 1.2  1994/08/24  18:53:51  john
 * Made Cyberman read like normal mouse; added dpmi module; moved
 * mouse from assembly to c. Made mouse buttons return time_down.
 * 
 * Revision 1.1  1994/08/24  10:22:34  john
 * Initial revision
 * 
 * 
 */


static char rcsid[] = "$Id: dpmi.c 1.19 1995/02/23 09:02:57 john Exp $";

// ASCENT: there is no DPMI host and no real mode. Each function is a stub
// that lets the callers compile, link and continue:
// - The memory lock functions return success. The memory is never paged
//   out under an interrupt handler, because there are no interrupt handlers.
// - The real mode interrupt and call functions do nothing and set the
//   carry flag in the register structure. The other registers stay as the
//   caller set them. A caller that tests a result register for a change
//   sees no service (for example, no CD-ROM driver).
// - The DOS memory functions return normal memory.
// - The selector functions return success. A selector has no meaning.
// - The memory sizes are constants that pass the checks of the game.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "dpmi.h"

unsigned int dpmi_virtual_memory=0;
unsigned int dpmi_available_memory=0;
unsigned int dpmi_physical_memory=0;
unsigned int dpmi_dos_memory = 0;

#define MAX_REAL_BLOCKS 64

static void *Real_blocks[MAX_REAL_BLOCKS];
static ubyte Temp_low_buffer[1024];

int dpmi_init(int verbose)
{
	dpmi_dos_memory = 600*1024;					// 600 K of DOS memory
	dpmi_physical_memory = 64*1024*1024;		// 64 MB
	dpmi_available_memory = 64*1024*1024;
	dpmi_virtual_memory = 0;						// no swap file

	if (verbose)
		printf( "DPMI: not used in this port.\n" );

	return 1;
}

// Returns a pointer to a temporary dos memory block. Size must be < 1024 bytes.
void *dpmi_get_temp_low_buffer( int size )
{
	if ( size < 0 || size > (int) sizeof(Temp_low_buffer) ) return NULL;
	return Temp_low_buffer;
}

void *dpmi_real_malloc( int size, ushort *selector )
{
	int i;

	// The selector is the index of the block plus 1.
	for (i=0; i<MAX_REAL_BLOCKS; i++ )	{
		if ( Real_blocks[i] == NULL )	{
			Real_blocks[i] = malloc( size > 0 ? size : 1 );
			if ( Real_blocks[i] == NULL ) return NULL;
			if ( selector ) *selector = (ushort) (i+1);
			return Real_blocks[i];
		}
	}
	return NULL;
}

void dpmi_real_free( ushort selector )
{
	if ( selector < 1 || selector > MAX_REAL_BLOCKS ) return;
	free( Real_blocks[selector-1] );
	Real_blocks[selector-1] = NULL;
}

void dpmi_real_int386x( ubyte intno, dpmi_real_regs * rregs )
{
	rregs->flags |= 1;		// carry: the call failed
}

void dpmi_real_call(dpmi_real_regs * rregs)
{
	rregs->flags |= 1;		// carry: the call failed
}

int dpmi_lock_region(void *address, unsigned length)
{
	return 1;
}

int dpmi_unlock_region(void *address, unsigned length)
{
	return 1;
}

// returns 0 if failed...
int dpmi_allocate_selector( void * address, int size, ushort * selector )
{
	if ( selector ) *selector = 0;
	return 1;
}

int dpmi_modify_selector_base( ushort selector, void * address )
{
	return 1;
}

int dpmi_modify_selector_limit( ushort selector, int size  )
{
	return 1;
}

// Sets the PM handler. Returns 0 if succssful
// ASCENT: it always fails. No handler is installed.
int dpmi_set_pm_handler(unsigned intnum, void * isr )
{
	return 1;
}
