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
 * $Source: f:/miner/source/bios/rcs/mono.c $
 * $Revision: 1.12 $
 * $Author: john $
 * $Date: 1995/02/23 11:59:57 $
 *
 * Library functions for printing to mono card.
 *
 * $Log: mono.c $
 * Revision 1.12  1995/02/23  11:59:57  john
 * Made the windows smaller so they don't overwrite the debug file menus.
 * 
 * Revision 1.11  1994/11/27  23:07:50  matt
 * Made changes needed to be able to compile out monochrome debugging code
 * 
 * Revision 1.10  1994/10/26  22:23:43  john
 * Limited windows to 2.  Took away saving what was under
 * a window.
 * 
 * Revision 1.9  1994/07/14  23:25:44  matt
 * Allow window 0 to be opened; don't allow mono to be initialized twice
 * 
 * Revision 1.8  1994/03/09  10:45:38  john
 * Sped up scroll.
 * 
 * Revision 1.7  1994/01/26  08:56:55  mike
 * Comment out int3 in mputc.
 * 
 * Revision 1.6  1994/01/12  15:56:34  john
 * made backspace do an int3 during mono stuff.
 * .,
 * 
 * Revision 1.5  1993/12/07  12:33:23  john
 * *** empty log message ***
 * 
 * Revision 1.4  1993/10/15  10:10:25  john
 * *** empty log message ***
 * 
 * Revision 1.3  1993/09/14  20:55:13  matt
 * Made minit() and mopen() check for presence of mono card in machine.
 * 
 * Revision 1.2  1993/07/22  13:10:21  john
 * *** empty log message ***
 * 
 * Revision 1.1  1993/07/10  13:10:38  matt
 * Initial revision
 *
 *
 */

static char rcsid[] = "$Id: mono.c 1.12 1995/02/23 11:59:57 john Exp $";

// ASCENT: the original wrote to the text memory of a monochrome display
// adapter (segment B000h) and kept windows with scrolling there. There is
// no such display now. The calls do nothing, or they write the text to
// stderr if the switch is on. The switch is on if the environment variable
// ASCENT_MONO is set to a value other than 0 when minit is called, or
// after mono_set_stderr(1).
// A release build (NDEBUG or NMONO) replaces the calls with empty macros
// in mono.h, and this file is then not used.

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// ASCENT: get the declarations of the functions also in a release build.
#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef NMONO
#undef NMONO
#endif
#include "mono.h"

static int Mono_to_stderr = 0;
static int Mono_initialized = 0;

void mono_set_stderr( int on )
{
	Mono_to_stderr = on;
}

void mputc( int n, char c )
{
	if (Mono_to_stderr)
		fputc( c, stderr );
}

void mputc_at( int n, int row, int col, char c )
{
	if (Mono_to_stderr)
		fputc( c, stderr );
}

void msetcursor( int row, int col )
{
}

void _mprintf( int n, char * format, ... )
{
	va_list args;

	if (!Mono_to_stderr) return;

	va_start(args, format );
	vfprintf(stderr,format,args);
	va_end(args);
}

void _mprintf_at( int n, int row, int col, char * format, ... )
{
	va_list args;

	if (!Mono_to_stderr) return;

	va_start(args, format );
	vfprintf(stderr,format,args);
	va_end(args);
	fputc( '\n', stderr );		// ASCENT: the text had a position. Here it gets a line.
}

void mclear( int n )
{
}

void mclose( int n )
{
}

void mrefresh( short n )
{
}

void mopen( int n, int row, int col, int width, int height, char * title )
{
}

// ASCENT: the original returned true if the machine had a mono card.
// This returns true if the text goes to stderr.
int minit()
{
	if (!Mono_initialized)	{
		char * s = getenv( "ASCENT_MONO" );
		Mono_initialized = 1;
		if ( s && *s && atoi(s) != 0 )
			Mono_to_stderr = 1;
	}
	return Mono_to_stderr;
}
