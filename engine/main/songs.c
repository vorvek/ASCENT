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
 * $Source: f:/miner/source/main/rcs/songs.c $
 * $Revision: 2.1 $
 * $Author: john $
 * $Date: 1995/05/02 16:15:21 $
 * 
 * Routines to manage the songs in Descent.
 * 
 * $Log: songs.c $
 * Revision 2.1  1995/05/02  16:15:21  john
 * Took out printf.
 * 
 * Revision 2.0  1995/02/27  11:27:13  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.2  1995/02/11  12:42:12  john
 * Added new song method, with FM bank switching..
 * 
 * Revision 1.1  1995/02/11  10:20:33  john
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: songs.c 2.1 1995/05/02 16:15:21 john Exp $";
#pragma on (unreferenced)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "error.h"
#include "types.h"
#include "songs.h"
#include "mono.h"
#include "cfile.h"
#include "digi.h"
#include "mem.h"
#include "dtufile.h"

// ASCENT: this file read DESCENT.SNG, the list of the HMP song files and
// their FM bank files, and played a song with digi_play_midi_song. The
// songs.c of Descent to Undermountain (0x78D87, 0x78EB1) is a later form
// of the file: it reads DTU.SNG, a table of CD audio tracks ("%hd %u":
// track, seconds), and songs_play_song starts the CD track of a music item
// if the CD audio driver is present ("RedBook Audio enabled"). The disc of
// the game has one data track and no audio track, so that code plays
// nothing with the shipped disc. The music of the game is the streamed
// NAME.TUN files (dtumusic.c). See docs/re/music.md.
//
// The port reads the table as the original does and keeps the functions
// for their callers. They play nothing: there is no CD audio.

song_info Songs[MAX_SONGS];
int Songs_initialized = 0;

void songs_init()
{
	char *text, *p, *end;
	char inputline[80+1];
	int size, n;
	int track;
	unsigned seconds;

	if ( Songs_initialized ) return;

	// ASCENT: the original stops with "Couldn't open DTU.SNG". The port
	// goes on with an empty table, because nothing plays from it.
	text = (char *)dtu_load_file( "DTU.SNG", &size );
	if ( text == NULL )	{
		Songs_initialized = 1;
		return;
	}

	p = text;
	end = text + size;
	while ( p < end && Songs_initialized < MAX_SONGS )	{
		n = 0;
		while ( p < end && *p != '\n' )	{
			if ( n < 80 ) inputline[n++] = *p;
			p++;
		}
		p++;
		inputline[n] = 0;
		if ( inputline[0] == ';' ) continue;
		if ( sscanf( inputline, "%d %u", &track, &seconds ) == 2 )	{
			Songs[Songs_initialized].track = (short)track;
			Songs[Songs_initialized].seconds = seconds;
			sprintf( Songs[Songs_initialized].filename, "CD track %d", track );
			Songs_initialized++;
		}
	}
	free( text );
	if ( Songs_initialized == 0 )
		Songs_initialized = 1;
}

void songs_play_song( int songnum, int repeat )
{
	if ( !Songs_initialized ) songs_init();
	// ASCENT: was the start of the CD track Songs[songnum].track.
	songnum = songnum;
	repeat = repeat;
}

void songs_play_level_song( int levelnum )
{
	int songnum;

	Assert( levelnum != 0 );

	if ( !Songs_initialized ) songs_init();

	if (levelnum < 0)	
		songnum = (-levelnum) % NUM_GAME_SONGS;
	else 
		songnum = (levelnum-1) % NUM_GAME_SONGS;
	
	songnum += SONG_LEVEL_MUSIC;
	songs_play_song( songnum, 1 );
}
