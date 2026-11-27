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
 * $Source: f:/miner/source/main/rcs/piggy.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:31:21 $
 * 
 * Interface to piggy functions.
 * 
 * $Log: piggy.h $
 * Revision 2.0  1995/02/27  11:31:21  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.10  1995/02/03  17:08:29  john
 * Changed sound stuff to allow low memory usage.
 * Also, changed so that Sounds isn't an array of digi_sounds, it
 * is a ubyte pointing into GameSounds, this way the digi.c code that
 * locks sounds won't accidentally unlock a sound that is already playing, but
 * since it's Sounds[soundno] is different, it would erroneously be unlocked.
 * 
 * Revision 1.9  1995/01/24  14:33:49  john
 * *** empty log message ***
 * 
 * Revision 1.8  1995/01/24  14:32:35  john
 * Took out paging in code.
 * 
 * Revision 1.7  1995/01/23  12:30:17  john
 * Made debug code that mprintf what bitmap gets paged in.
 * 
 * Revision 1.6  1995/01/17  14:11:37  john
 * Added function that is called after level loaded.
 * 
 * Revision 1.5  1995/01/14  19:16:58  john
 * First version of new bitmap paging code.
 * 
 * Revision 1.4  1994/10/27  18:51:57  john
 * Added -piglet option that only loads needed textures for a 
 * mine.  Only saved ~1MB, and code still doesn't free textures
 * before you load a new mine.
 * 
 * Revision 1.3  1994/06/08  14:20:47  john
 * Made piggy dump before going into game.
 * 
 * Revision 1.2  1994/05/06  13:02:40  john
 * Added piggy stuff; worked on supertransparency
 * 
 * Revision 1.1  1994/05/06  11:47:46  john
 * Initial revision
 * 
 * 
 */



#ifndef _PIGGY_H
#define _PIGGY_H

#include "digi.h"
#include "sounds.h"

typedef struct bitmap_index {
	ushort	index;
} bitmap_index;

int piggy_init();
void piggy_close();
void piggy_dump_all();
bitmap_index piggy_register_bitmap( grs_bitmap * bmp, char * name, int in_file );
int piggy_register_sound( digi_sound * snd, char * name, int in_file );
bitmap_index piggy_find_bitmap( char * name );
int piggy_find_sound( char * name );

#ifdef PIGGY_USE_PAGING
	#define PIGGY_PAGE_IN(bmp) 							\
do { 																\
	if ( GameBitmaps[(bmp).index].bm_flags & BM_FLAG_PAGED_OUT )	{	\
		piggy_bitmap_page_in( bmp ); 						\
	}																\
} while(0)
//		mprintf(( 0, "Paging in '%s' from file '%s', line %d\n", #bmp, __FILE__,__LINE__ ));	\

	extern void piggy_bitmap_page_in( bitmap_index bmp );
	extern void piggy_bitmap_page_out_all();
	extern int piggy_page_flushed;
#else
	#define PIGGY_PAGE_IN(bmp) 
#endif

void piggy_read_bitmap_data(grs_bitmap * bmp);
void piggy_read_sound_data(digi_sound	*snd);

void piggy_load_level_data();

// ASCENT: the limits of DTUGAME.EXE (constants of the asserts in its
// piggy.c): 4000 bitmaps and 5500 sounds. Descent 1 had 1800 and 250.
#define MAX_BITMAP_FILES	4000
#define MAX_SOUND_FILES		5500

// ASCENT: the value of piggy_find_sound() and of Sounds[] for "no sound".
// DTUGAME.EXE fills its sound tables with 10000. Descent 1 used 255.
#define SOUND_FILE_NONE		10000

extern digi_sound GameSounds[MAX_SOUND_FILES];
extern grs_bitmap GameBitmaps[MAX_BITMAP_FILES];

void piggy_read_sounds();

// ASCENT: the EGG archives of Descent to Undermountain replace the PIG
// file. piggy_init() makes the empty tables. piggy_load_egg() loads one
// archive (NAME.EGG, or the packed NAME.COW, through dtufile.c) and adds
// its bitmaps, sounds and polygon models. It returns the number of the
// archive or -1. The first archive gets the bitmap numbers from 1, the
// sound numbers from 0 and the model numbers from 0. Each later archive
// continues the numbers. DTUGAME.EXE loads the archive of the player
// character before the archive of the level.
// piggy_free_all() releases all archives and empties the tables.
int piggy_load_egg( char * filename );
// ASCENT: one polygon model of an archive that is not loaded (Dtu_caster_ai:
// the drow priestess with a mace). Returns the model number or -1.
int piggy_load_egg_model( char * filename, char * model_name );
void piggy_free_all();
int piggy_get_num_eggs();
extern int Piggy_generation;	// ASCENT: changes when the tables change
const char *piggy_get_egg_name( int egg_num );		// ASCENT: the file name of an archive
int piggy_get_egg_counts( int egg_num, int *n_bitmaps, int *n_sounds, int *n_models, int *first_bitmap, int *first_sound, int *first_model );

extern int Num_bitmap_files;
extern int Num_sound_files;

void piggy_get_bitmap_name( int i, char * name );
void piggy_get_sound_name( int i, char * name );

// ASCENT: the flags byte of a bitmap as it is in the archive. The 2d
// library gets bits 1, 2, 4 and 8. Bit 16 marks a bitmap that the low
// memory mode of Descent 1 replaced. Bit 64 is on the interface bitmaps.
int piggy_get_bitmap_disk_flags( int i );

// ASCENT: the sample rate of a sound (22050 in all shipped archives), and
// the decoded sound: signed 16-bit values, interleaved for two channels.
// GameSounds[i].data holds the same sound as unsigned 8-bit mono.
int piggy_get_sound_rate( int i );
short *piggy_get_sound_16( int i, int *n_values, int *channels );


#endif
