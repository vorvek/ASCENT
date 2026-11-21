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
 * $Source: f:/miner/source/main/rcs/piggy.c $
 * $Revision: 2.10 $
 * $Author: john $
 * $Date: 1995/10/07 13:17:26 $
 * 
 * Functions for managing the pig files.
 * 
 * $Log: piggy.c $
 * Revision 2.10  1995/10/07  13:17:26  john
 * Made all bitmaps paged out by default.
 * 
 * Revision 2.9  1995/04/14  14:05:24  john
 * *** empty log message ***
 * 
 * Revision 2.8  1995/04/12  13:39:37  john
 * Fixed bug with -lowmem not working.
 * 
 * Revision 2.7  1995/03/29  23:23:17  john
 * Fixed major bug with sounds not building into pig right.
 * 
 * Revision 2.6  1995/03/28  18:05:00  john
 * Fixed it so you don't have to delete pig after changing bitmaps.tbl
 * 
 * Revision 2.5  1995/03/16  23:13:06  john
 * Fixed bug with piggy paging in bitmap not checking for disk
 * error, hence bogifying textures if you pull the CD out.
 * 
 * Revision 2.4  1995/03/14  16:22:27  john
 * Added cdrom alternate directory stuff.
 * 
 * Revision 2.3  1995/03/06  15:23:20  john
 * New screen techniques.
 * 
 * Revision 2.2  1995/02/27  13:13:40  john
 * Removed floating point.
 * 
 * Revision 2.1  1995/02/27  12:31:25  john
 * Made work without editor.
 * 
 * Revision 2.0  1995/02/27  11:28:02  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.85  1995/02/09  12:54:24  john
 * Made paged out bitmaps have bm_data be a valid pointer
 * instead of NULL, in case anyone accesses it.
 * 
 * Revision 1.84  1995/02/09  12:50:59  john
 * Bullet-proofed the piggy loading code.
 * 
 * Revision 1.83  1995/02/07  17:08:51  john
 * Added some error handling stuff instead of asserts.
 * 
 * Revision 1.82  1995/02/03  17:06:48  john
 * Changed sound stuff to allow low memory usage.
 * Also, changed so that Sounds isn't an array of digi_sounds, it
 * is a ubyte pointing into GameSounds, this way the digi.c code that
 * locks sounds won't accidentally unlock a sound that is already playing, but
 * since it's Sounds[soundno] is different, it would erroneously be unlocked.
 * 
 * Revision 1.81  1995/02/02  21:56:39  matt
 * Added data for new gauge bitmaps
 * 
 * Revision 1.80  1995/02/01  23:31:57  john
 * Took out loading bar.
 * 
 * Revision 1.79  1995/01/28  15:13:18  allender
 * bumped up Piggy_bitmap_cache_size
 * 
 * Revision 1.78  1995/01/26  12:30:43  john
 * Took out prev.
 * 
 * Revision 1.77  1995/01/26  12:12:17  john
 * Made buffer be big for bitmaps.
 * 
 * Revision 1.76  1995/01/25  20:15:38  john
 * Made editor allocate all mem.
 * 
 * Revision 1.75  1995/01/25  14:52:56  john
 * Made bitmap buffer be 1.5 MB.
 * 
 * Revision 1.74  1995/01/22  16:03:19  mike
 * localization.
 * 
 * Revision 1.73  1995/01/22  15:58:36  mike
 * localization
 * 
 * Revision 1.72  1995/01/18  20:51:20  john
 * Took out warnings.
 * 
 * Revision 1.71  1995/01/18  20:47:21  john
 * Added code to allocate sounds & bitmaps into diff
 * buffers, also made sounds not be compressed for registered.
 * 
 * Revision 1.70  1995/01/18  15:08:41  john
 * Added start/stop time around paging.
 * Made paging clear screen around globe.
 * 
 * Revision 1.69  1995/01/18  10:07:51  john
 * 
 * Took out debugging mprintfs.
 * 
 * Revision 1.68  1995/01/17  14:27:42  john
 * y
 * 
 * Revision 1.67  1995/01/17  12:14:39  john
 * Made walls, object explosion vclips load at level start.
 * 
 * Revision 1.66  1995/01/15  13:15:44  john
 * Made so that paging always happens, lowmem just loads less.
 * Also, make KB load print to hud.
 * 
 * Revision 1.65  1995/01/15  11:56:28  john
 * Working version of paging.
 * 
 * Revision 1.64  1995/01/14  19:17:07  john
 * First version of new bitmap paging code.
 * 
 * Revision 1.63  1994/12/15  12:26:44  john
 * Added -nolowmem function.
 * 
 * Revision 1.62  1994/12/14  21:12:26  john
 * Fixed bug with page fault when exiting and using
 * -nosound.
 * 
 * Revision 1.61  1994/12/14  11:35:31  john
 * Evened out thermometer for pig read.
 * 
 * Revision 1.60  1994/12/14  10:51:00  john
 * Sped up sound loading.
 * 
 * Revision 1.59  1994/12/14  10:12:08  john
 * Sped up pig loading.
 * 
 * Revision 1.58  1994/12/13  09:14:47  john
 * *** empty log message ***
 * 
 * Revision 1.57  1994/12/13  09:12:57  john
 * Made the bar always fill up.
 * 
 * Revision 1.56  1994/12/13  03:49:08  john
 * Made -lowmem not load the unnecessary bitmaps.
 * 
 * Revision 1.55  1994/12/06  16:06:35  john
 * Took out piggy sorting.
 * 
 * Revision 1.54  1994/12/06  15:11:14  john
 * Fixed bug with reading pigs.
 * 
 * Revision 1.53  1994/12/06  14:14:47  john
 * Added code to set low mem based on memory.
 * 
 * Revision 1.52  1994/12/06  14:01:10  john
 * Fixed bug that was causing -lowmem all the time..
 * 
 * Revision 1.51  1994/12/06  13:33:48  john
 * Added lowmem option.
 * 
 * Revision 1.50  1994/12/05  19:40:10  john
 * If -nosound or no sound card selected, don't load sounds from pig.
 * 
 * Revision 1.49  1994/12/05  12:17:44  john
 * Added code that locks/unlocks digital sounds on demand.
 * 
 * Revision 1.48  1994/12/05  11:39:03  matt
 * Fixed little mistake
 * 
 * Revision 1.47  1994/12/05  09:29:22  john
 * Added clength to the sound field.
 * 
 * Revision 1.46  1994/12/04  15:27:15  john
 * Fixed my stupid bug that looked at -nosound instead of digi_driver_card
 * to see whether or not to lock down sound memory.
 * 
 * Revision 1.45  1994/12/03  14:17:00  john
 * Took out my debug mprintf.
 * 
 * Revision 1.44  1994/12/03  13:32:37  john
 * Fixed bug with offscreen bitmap.
 * 
 * Revision 1.43  1994/12/03  13:07:13  john
 * Made the pig read/write compressed sounds.
 * 
 * Revision 1.42  1994/12/03  11:48:51  matt
 * Added option to not dump sounds to pigfile
 * 
 * Revision 1.41  1994/12/02  20:02:20  matt
 * Made sound files constant match constant for table
 * 
 * Revision 1.40  1994/11/29  11:03:09  adam
 * upped # of sounds
 * 
 * Revision 1.39  1994/11/27  23:13:51  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.38  1994/11/20  18:40:34  john
 * MAde the piggy.lst and piggy.all not dump for release.
 * 
 * Revision 1.37  1994/11/19  23:54:45  mike
 * up number of bitmaps for shareware version.
 * 
 * Revision 1.36  1994/11/19  19:53:05  mike
 * change MAX_BITMAP_FILES
 * 
 * Revision 1.35  1994/11/19  10:42:56  matt
 * Increased number of bitmaps for non-shareware version
 * 
 * Revision 1.34  1994/11/19  09:11:52  john
 * Added avg_color to bitmaps saved in pig.
 * 
 * Revision 1.33  1994/11/19  00:07:05  john
 * Fixed bug with 8 char sound filenames not getting read from pig.
 * 
 * Revision 1.32  1994/11/18  22:24:54  john
 * Added -bigpig command line that doesn't rle your pig.
 * 
 * Revision 1.31  1994/11/18  21:56:53  john
 * Added a better, leaner pig format.
 * 
 * Revision 1.30  1994/11/16  12:06:16  john
 * Fixed bug with calling .bbms abms.
 * 
 * Revision 1.29  1994/11/16  12:00:56  john
 * Added piggy.all dump.
 * 
 * Revision 1.28  1994/11/10  21:16:02  adam
 * nothing important
 * 
 * Revision 1.27  1994/11/10  13:42:00  john
 * Made sounds not lock down if using -nosound.
 * 
 * Revision 1.26  1994/11/09  19:55:40  john
 * Added full rle support with texture rle caching.
 * 
 * Revision 1.25  1994/11/09  16:36:42  john
 * First version with RLE bitmaps in Pig.
 * 
 * Revision 1.24  1994/10/27  19:42:59  john
 * Disable the piglet option.
 * 
 * Revision 1.23  1994/10/27  18:51:40  john
 * Added -piglet option that only loads needed textures for a 
 * mine.  Only saved ~1MB, and code still doesn't free textures
 * before you load a new mine.
 * 
 * Revision 1.22  1994/10/25  13:11:42  john
 * Made the sounds sort. Dumped piggy.lst.
 * 
 * Revision 1.21  1994/10/06  17:06:23  john
 * Took out rle stuff.
 * 
 * Revision 1.20  1994/10/06  15:45:36  adam
 * bumped MAX_BITMAP_FILES again!
 * 
 * Revision 1.19  1994/10/06  11:01:17  yuan
 * Upped MAX_BITMAP_FILES
 * 
 * Revision 1.18  1994/10/06  10:44:45  john
 * Added diagnostic message and psuedo run-length-encoder
 * to see how much memory we would save by storing bitmaps
 * in a RLE method.  Also, I commented out the code that
 * stores 4K bitmaps on a 4K boundry to reduce pig size 
 * a bit.
 * 
 * Revision 1.17  1994/10/04  20:03:13  matt
 * Upped maximum number of bitmaps
 * 
 * Revision 1.16  1994/10/03  18:04:20  john
 * Fixed bug with data_offset not set right for bitmaps
 * that are 64x64 and not aligned on a 4k boundry.
 * 
 * Revision 1.15  1994/09/28  11:30:55  john
 * changed inferno.pig to descent.pig, changed the way it
 * is read.
 * 
 * Revision 1.14  1994/09/22  16:14:17  john
 * Redid intro sequecing.
 * 
 * Revision 1.13  1994/09/19  14:42:47  john
 * Locked down sounds with Virtual memory.
 * 
 * Revision 1.12  1994/09/10  17:31:52  mike
 * Increase number of loadable bitmaps.
 * 
 * Revision 1.11  1994/09/01  19:32:49  mike
 * Boost texture map allocation.
 * 
 * Revision 1.10  1994/08/16  11:51:02  john
 * Added grwased pigs.
 * 
 * Revision 1.9  1994/07/06  09:18:03  adam
 * upped bitmap #s
 * 
 * Revision 1.8  1994/06/20  22:02:15  matt
 * Fixed bug from last change
 * 
 * Revision 1.7  1994/06/20  21:33:18  matt
 * Made bm.h not include sounds.h, to reduce dependencies
 * 
 * Revision 1.6  1994/06/20  16:52:19  john
 * cleaned up init output a bit.
 * 
 * Revision 1.5  1994/06/08  14:20:57  john
 * Made piggy dump before going into game.
 * 
 * Revision 1.4  1994/06/02  18:59:22  matt
 * Clear selector field of bitmap loaded from pig file
 * 
 * Revision 1.3  1994/05/06  15:31:41  john
 * Made name field a bit longer.
 * 
 * Revision 1.2  1994/05/06  13:02:44  john
 * Added piggy stuff; worked on supertransparency
 * 
 * Revision 1.1  1994/05/06  11:47:26  john
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: piggy.c 2.10 1995/10/07 13:17:26 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
// ASCENT: conio.h, io.h, cflib.h and dpmi.h (DOS) are removed.

#include "types.h"
#include "inferno.h"
#include "gr.h"
//#include "mem.h"
#include "mono.h"
#include "error.h"
#include "sounds.h"
#include "bm.h"
#include "hash.h"
#include "args.h"
#include "palette.h"
#include "gamefont.h"
#include "rle.h"
#include "screens.h"

//#include "soscomp.h"
//The above include is part of a commercial 
//sound library, so it cannot be included in a public 
//release of the source code. -KRB
//#include "no_sos.h" //Added by KRB
// ASCENT: no_sos.h is not in the source release. This file needs nothing from it.

#include "piggy.h"
#include "texmerge.h"
#include "paging.h"
#include "game.h"
#include "text.h"
#include "cfile.h"
#include "newmenu.h"
#include "dataio.h"		// ASCENT: little endian field reads

//#define NO_DUMP_SOUNDS	1		//if set, dump bitmaps but not sounds

#include <ctype.h>
#include "mem.h"			// ASCENT: malloc and free of the game, as in dtufile.c and acm.c
#include "dtufile.h"		// ASCENT: the game data files
#include "acm.h"			// ASCENT: the sounds of an EGG archive are ACM streams
#include "platform.h"		// ASCENT: plat_audio_flush
#include "polyobj.h"

// ASCENT: from here on this file is new. Descent 1 read one PIG file
// (DESCENT.PIG) and paged its bitmaps into a cache of 2 MB. Descent to
// Undermountain reads EGG archives: one for the level and one for each
// player character. An EGG archive holds bitmaps, sounds and polygon
// models. The layout is in docs/formats/egg.md.
//
// What is the same as in Descent 1: the bitmap header (17 bytes), the RLE
// bitmap data, the names (piggy_find_bitmap, "name#frame" for a frame of
// an animation), bitmap 0 (the "bogus" bitmap, a red X).
//
// What this port does in another way than DTUGAME.EXE: the whole archive
// is loaded into memory and every bitmap points into that memory. There
// is no cache and no paging. PIGGY_PAGE_IN still compiles: no bitmap has
// BM_FLAG_PAGED_OUT, so it does nothing.

typedef struct BitmapFile	{
	char			name[15];
} BitmapFile;

typedef struct SoundFile	{
	char			name[15];
} SoundFile;

hashtable AllBitmapsNames;
hashtable AllDigiSndNames;

int Num_bitmap_files = 0;
int Num_sound_files = 0;

digi_sound GameSounds[MAX_SOUND_FILES];
grs_bitmap GameBitmaps[MAX_BITMAP_FILES];

static BitmapFile AllBitmaps[ MAX_BITMAP_FILES ];
static SoundFile AllSounds[ MAX_SOUND_FILES ];

// The flags byte of the bitmap header as it is in the archive.
static ubyte GameBitmapDiskFlags[MAX_BITMAP_FILES];

// ASCENT: the low memory translation table of Descent 1. It maps each
// bitmap to itself. Only the PSX tool code of paging.c names it.
ushort GameBitmapXlat[MAX_BITMAP_FILES];

// Sound data. GameSounds[i].data is unsigned 8-bit mono (what the mixer of
// digi.c plays). GameSounds16[i] is the decoded stream: signed 16-bit,
// interleaved if it has two channels.
static short *GameSounds16[MAX_SOUND_FILES];
static int GameSoundRate[MAX_SOUND_FILES];
static int GameSoundChannels[MAX_SOUND_FILES];
static int GameSoundValues[MAX_SOUND_FILES];
static int GameSoundDiskFlags[MAX_SOUND_FILES];

int piggy_low_memory = 0;
int piggy_page_flushed = 0;

// ASCENT: gauges.c shows this value (the fill of the Descent 1 bitmap
// cache). There is no cache now.
int Piggy_bitmap_cache_next = 0;

#define DBM_FLAG_LARGE 	128		// Flags added onto the flags struct in b
#define DBM_FLAG_ABM		64

#define DISK_BITMAP_HEADER_SIZE	17		// 8+1+1+1+1+1+4
#define DISK_SOUND_HEADER_SIZE	24		// 8+4+4+4+4
#define DISK_SOUND_HEADER_SIZE_OLD	20		// 8+4+4+4, the header of Descent 1
#define DISK_MODEL_HEADER_SIZE	DTU_MODEL_HEADER_SIZE

// The archives that are loaded now.
#define MAX_EGG_FILES	8

typedef struct egg_file {
	char		name[16];
	ubyte		*data;
	int		size;
	int		n_bitmaps, n_sounds, n_models;
	int		first_bitmap, first_sound, first_model;
	int		bad_bitmaps, bad_sounds, bad_models;
} egg_file;

static egg_file Eggs[MAX_EGG_FILES];
static int Num_eggs = 0;

static int Piggy_initialized = 0;

ubyte bogus_data[64*64];
grs_bitmap bogus_bitmap;
ubyte bogus_bitmap_initialized=0;
digi_sound bogus_sound;

void piggy_get_bitmap_name( int i, char * name )
{
	strncpy( name, AllBitmaps[i].name, 12 );
	name[12] = 0;
}

int piggy_get_bitmap_disk_flags( int i )
{
	if ( i < 0 || i >= Num_bitmap_files ) return 0;
	return GameBitmapDiskFlags[i];
}

bitmap_index piggy_register_bitmap( grs_bitmap * bmp, char * name, int in_file )
{
	bitmap_index temp;
	Assert( Num_bitmap_files < MAX_BITMAP_FILES );

	temp.index = Num_bitmap_files;

	strncpy( AllBitmaps[Num_bitmap_files].name, name, 12 );
	AllBitmaps[Num_bitmap_files].name[12] = 0;
	// ASCENT: two archives can hold a bitmap of the same name (a player
	// archive and a level archive). The first one keeps the name. The
	// second one still gets a number.
	if ( hashtable_search( &AllBitmapsNames, AllBitmaps[Num_bitmap_files].name ) < 0 )
		hashtable_insert( &AllBitmapsNames, AllBitmaps[Num_bitmap_files].name, Num_bitmap_files );
	GameBitmaps[Num_bitmap_files] = *bmp;
	if ( !in_file )
		GameBitmapDiskFlags[Num_bitmap_files] = bmp->bm_flags;
	Num_bitmap_files++;

	return temp;
}

int piggy_register_sound( digi_sound * snd, char * name, int in_file )
{
	int i;

	Assert( Num_sound_files < MAX_SOUND_FILES );

	strncpy( AllSounds[Num_sound_files].name, name, 12 );
	AllSounds[Num_sound_files].name[12] = 0;
	if ( hashtable_search( &AllDigiSndNames, AllSounds[Num_sound_files].name ) < 0 )
		hashtable_insert( &AllDigiSndNames, AllSounds[Num_sound_files].name, Num_sound_files );
	GameSounds[Num_sound_files] = *snd;

	i = Num_sound_files;
	Num_sound_files++;
	return i;
}

bitmap_index piggy_find_bitmap( char * name )
{
	bitmap_index bmp;
	int i;

	bmp.index = 0;

	i = hashtable_search( &AllBitmapsNames, name );
	Assert( i != 0 );
	if ( i < 0 )
		return bmp;

	bmp.index = i;
	return bmp;
}

// ASCENT: returns SOUND_FILE_NONE if there is no such sound. Descent 1
// returned 255, which is a valid sound file number now.
int piggy_find_sound( char * name )
{
	int i;

	i = hashtable_search( &AllDigiSndNames, name );

	if ( i < 0 )
		return SOUND_FILE_NONE;

	return i;
}

int piggy_get_sound_rate( int i )
{
	if ( i < 0 || i >= Num_sound_files ) return 0;
	return GameSoundRate[i];
}

short *piggy_get_sound_16( int i, int *n_values, int *channels )
{
	if ( i < 0 || i >= Num_sound_files ) return NULL;
	if ( n_values ) *n_values = GameSoundValues[i];
	if ( channels ) *channels = GameSoundChannels[i];
	return GameSounds16[i];
}

void piggy_get_sound_name( int i, char * name )
{
	strncpy( name, AllSounds[i].name, 12 );
	name[12] = 0;
}

static void piggy_reset_tables()
{
	int i;

	for (i=0; i<MAX_SOUND_FILES; i++ )	{
		GameSounds[i].length = 0;
		GameSounds[i].data = NULL;
		GameSounds16[i] = NULL;
		GameSoundRate[i] = 0;
		GameSoundChannels[i] = 0;
		GameSoundValues[i] = 0;
		GameSoundDiskFlags[i] = 0;
	}

	for (i=0; i<MAX_BITMAP_FILES; i++ )		{
		GameBitmapXlat[i] = i;
		GameBitmapDiskFlags[i] = 0;
		memset( &GameBitmaps[i], 0, sizeof(grs_bitmap) );
	}

	Num_bitmap_files = 0;
	Num_sound_files = 0;

	// Bitmap 0 is the bogus bitmap, as in Descent 1.
	if ( !bogus_bitmap_initialized )	{
		ubyte c;
		bogus_bitmap_initialized = 1;
		memset( &bogus_bitmap, 0, sizeof(grs_bitmap) );
		bogus_bitmap.bm_w = bogus_bitmap.bm_h = bogus_bitmap.bm_rowsize = 64;
		bogus_bitmap.bm_data = bogus_data;
		c = gr_find_closest_color( 0, 0, 63 );
		for (i=0; i<4096; i++ ) bogus_data[i] = c;
		c = gr_find_closest_color( 63, 0, 0 );
		// Make a big red X !
		for (i=0; i<64; i++ )	{
			bogus_data[i*64+i] = c;
			bogus_data[i*64+(63-i)] = c;
		}
		bogus_sound.length = 64*64;
		bogus_sound.data = bogus_data;
	}
	piggy_register_bitmap( &bogus_bitmap, "bogus", 1 );
}

// ASCENT: Descent 1 opened DESCENT.PIG here and read all the headers. Now
// this function only sets up the empty tables. piggy_load_egg() adds the
// contents of an archive.
int piggy_init()
{
	if ( Piggy_initialized )
		return 0;
	Piggy_initialized = 1;

	hashtable_init( &AllBitmapsNames, MAX_BITMAP_FILES*2 );
	hashtable_init( &AllDigiSndNames, MAX_SOUND_FILES*2 );

	if ( FindArg( "-lowmem" ))
		piggy_low_memory = 0;		// ASCENT: there is no low memory mode

	piggy_reset_tables();

	atexit( piggy_close );

	return 0;
}

// Release all archives. The bitmap, sound and model tables are empty
// afterwards (bitmap 0 is the bogus bitmap again).
void piggy_free_all()
{
	int i;

	if ( !Piggy_initialized )	{
		piggy_init();
		return;
	}

	// Bitmaps that the texture merge cache and the RLE cache made from
	// archive bitmaps are not valid any more.
	texmerge_flush();
	rle_cache_flush();

	// ASCENT: the mixer thread reads the sound data. Stop all sounds
	// before the data is freed.
	plat_audio_flush();

	for (i=0; i<Num_sound_files; i++ )	{
		if ( GameSounds[i].data && GameSounds[i].data != bogus_data )
			free( GameSounds[i].data );
		if ( GameSounds16[i] )
			free( GameSounds16[i] );
	}

	for (i=0; i<Num_eggs; i++ )	{
		if ( Eggs[i].data )
			free( Eggs[i].data );
		Eggs[i].data = NULL;
	}
	Num_eggs = 0;

	polyobj_free_all_models();

	hashtable_free( &AllBitmapsNames );
	hashtable_free( &AllDigiSndNames );
	hashtable_init( &AllBitmapsNames, MAX_BITMAP_FILES*2 );
	hashtable_init( &AllDigiSndNames, MAX_SOUND_FILES*2 );

	piggy_reset_tables();
}

// Check the data of one RLE bitmap: the total size, one size byte for each
// row, and rows that end exactly at the total size. Returns 0 if not valid.
static int piggy_check_rle( const ubyte *p, int avail, int w, int h )
{
	int total, i, sum, x, row_end;
	const ubyte *row;

	if ( avail < 4 + h )
		return 0;
	total = p[0] | (p[1]<<8) | (p[2]<<16) | ((int)p[3]<<24);
	if ( total < 4 + h || total > avail )
		return 0;
	sum = 4 + h;
	for (i=0; i<h; i++ )
		sum += p[4+i];
	if ( sum != total )
		return 0;

	// Each row must give exactly w pixels.
	row = p + 4 + h;
	for (i=0; i<h; i++ )	{
		row_end = p[4+i];
		x = 0;
		{
			int k = 0;
			while ( k < row_end )	{
				ubyte c = row[k++];
				if ( (c & 0xE0) == 0xE0 )	{
					int count = c & 0x1F;
					if ( count == 0 )
						break;
					if ( k >= row_end )
						return 0;
					k++;
					x += count;
				} else
					x++;
			}
		}
		if ( x != w )
			return 0;
		row += row_end;
	}
	return 1;
}

static int egg_get_int( const ubyte *p )
{
	return (int)( p[0] | (p[1]<<8) | (p[2]<<16) | ((unsigned)p[3]<<24) );
}

// Convert a decoded sound to the unsigned 8-bit mono form of digi_sound.
static ubyte *piggy_make_8bit( const short *s16, int n_values, int channels, int *length )
{
	int n = n_values / channels;
	ubyte *d;
	int i, v;

	d = (ubyte *) malloc( n + 1 );
	if ( d == NULL )
		return NULL;
	for (i=0; i<n; i++ )	{
		if ( channels == 2 )
			v = (s16[i*2] + s16[i*2+1]) / 2;
		else
			v = s16[i];
		d[i] = (ubyte)((v >> 8) + 128);
	}
	*length = n;
	return d;
}

// 1 if a model header can start at ofs: the name field holds "name.pof".
static int egg_is_model_header( const ubyte *data, int size, int ofs )
{
	const ubyte *name;
	int i;

	if ( ofs < 0 || ofs + DISK_MODEL_HEADER_SIZE > size )
		return 0;
	name = data + ofs + 0x3D;
	for (i=1; i<9; i++ )
		if ( name[i] == '.' )
			return tolower(name[i+1]) == 'p' && tolower(name[i+2]) == 'o' && tolower(name[i+3]) == 'f' && name[i+4] == 0;
	return 0;
}

// Load one EGG archive (NAME.EGG, or the packed NAME.COW) and add its
// bitmaps, sounds and polygon models to the tables. Returns the number of
// the archive (0 for the first one), or -1 if the file is missing or not
// valid. piggy_get_egg_counts() gives the counts.
int piggy_load_egg( char * filename )
{
	egg_file *egg;
	ubyte *data;
	int size, n_bitmaps, n_sounds, n_models, header_size, data_start;
	int i, read_sounds = 1;
	int sound_header_size = DISK_SOUND_HEADER_SIZE;
	const ubyte *h;
	char temp_name_read[16];
	char temp_name[16];
	grs_bitmap temp_bitmap;
	digi_sound temp_sound;

	piggy_init();

	if ( Num_eggs >= MAX_EGG_FILES )	{
		dtu_log( "Too many EGG archives\n" );
		return -1;
	}

	data = dtu_load_file( filename, &size );
	if ( data == NULL )	{
		dtu_log( "Cannot load EGG archive '%s'\n", filename );
		return -1;
	}

	if ( size < 12 )	{
		free( data );
		return -1;
	}

	n_bitmaps = egg_get_int( data );
	n_sounds = egg_get_int( data+4 );
	n_models = egg_get_int( data+8 );

	// ASCENT: the file has no magic number. Check the counts against the
	// limits of the game and against the file size.
	if ( n_bitmaps < 0 || n_sounds < 0 || n_models < 0 ||
		  n_bitmaps > MAX_BITMAP_FILES || n_sounds > MAX_SOUND_FILES || n_models > MAX_POLYGON_MODELS )	{
		dtu_log( "EGG archive '%s': bad counts %d, %d, %d\n", filename, n_bitmaps, n_sounds, n_models );
		free( data );
		return -1;
	}
	// ASCENT: two archives of the disc (DEMO.EGG and FFM.EGG) have an older
	// layout: the sound header is the 20 bytes of Descent 1, without the
	// flags, and the sounds are raw 8-bit samples. The file does not say
	// which layout it has. The model headers do: the name of a model ends
	// with ".pof" only if the size of the sound headers before it is right.
	if ( n_sounds > 0 && n_models > 0 )	{
		int ofs24 = 12 + n_bitmaps*DISK_BITMAP_HEADER_SIZE + n_sounds*DISK_SOUND_HEADER_SIZE;
		int ofs20 = 12 + n_bitmaps*DISK_BITMAP_HEADER_SIZE + n_sounds*DISK_SOUND_HEADER_SIZE_OLD;
		if ( !egg_is_model_header( data, size, ofs24 ) && egg_is_model_header( data, size, ofs20 ) )	{
			sound_header_size = DISK_SOUND_HEADER_SIZE_OLD;
			dtu_log( "EGG archive '%s' has the old sound headers (20 bytes, raw samples)", filename );
		}
	}
	header_size = n_bitmaps*DISK_BITMAP_HEADER_SIZE + n_sounds*sound_header_size + n_models*DISK_MODEL_HEADER_SIZE;
	data_start = header_size + 12;
	if ( data_start > size ||
		  Num_bitmap_files + n_bitmaps > MAX_BITMAP_FILES ||
		  Num_sound_files + n_sounds > MAX_SOUND_FILES ||
		  N_polygon_models + n_models > MAX_POLYGON_MODELS )	{
		dtu_log( "EGG archive '%s': tables too large (%d, %d, %d)\n", filename, n_bitmaps, n_sounds, n_models );
		free( data );
		return -1;
	}

	egg = &Eggs[Num_eggs];
	memset( egg, 0, sizeof(*egg) );
	strncpy( egg->name, filename, 15 );
	egg->data = data;
	egg->size = size;
	egg->n_bitmaps = n_bitmaps;
	egg->n_sounds = n_sounds;
	egg->n_models = n_models;
	egg->first_bitmap = Num_bitmap_files;
	egg->first_sound = Num_sound_files;
	egg->first_model = N_polygon_models;

	if ( FindArg( "-nosound" ) )
		read_sounds = 0;

	// Bitmaps
	h = data + 12;
	for (i=0; i<n_bitmaps; i++, h += DISK_BITMAP_HEADER_SIZE )	{
		int dflags = h[8];
		int width = h[9];
		int height = h[10];
		int flags = h[11];
		int offset = egg_get_int( h+13 ) + data_start;
		int ok;

		memcpy( temp_name_read, h, 8 );
		temp_name_read[8] = 0;
		if ( dflags & DBM_FLAG_ABM )
			sprintf( temp_name, "%s#%d", temp_name_read, dflags & 63 );
		else
			strcpy( temp_name, temp_name_read );

		if ( dflags & DBM_FLAG_LARGE )
			width += 256;

		memset( &temp_bitmap, 0, sizeof(grs_bitmap) );
		temp_bitmap.bm_w = temp_bitmap.bm_rowsize = width;
		temp_bitmap.bm_h = height;
		temp_bitmap.avg_color = h[12];
		temp_bitmap.bm_flags = 0;
		if ( flags & BM_FLAG_TRANSPARENT ) temp_bitmap.bm_flags |= BM_FLAG_TRANSPARENT;
		if ( flags & BM_FLAG_SUPER_TRANSPARENT ) temp_bitmap.bm_flags |= BM_FLAG_SUPER_TRANSPARENT;
		if ( flags & BM_FLAG_NO_LIGHTING ) temp_bitmap.bm_flags |= BM_FLAG_NO_LIGHTING;
		if ( flags & BM_FLAG_RLE ) temp_bitmap.bm_flags |= BM_FLAG_RLE;
		// Bit 16 of the file (BM_FLAG_PAGED_OUT) is not copied: the data is
		// in memory. Bit 64 has no place in the 8 flag bits of the 2d
		// library. piggy_get_bitmap_disk_flags() returns all the bits.

		ok = (offset >= data_start && offset < size && width > 0 && height > 0);
		if ( ok )	{
			if ( flags & BM_FLAG_RLE )
				ok = piggy_check_rle( data+offset, size-offset, width, height );
			else
				ok = (offset + width*height <= size);
		}

		if ( ok )
			temp_bitmap.bm_data = data + offset;
		else	{
			// The bitmap is not valid. Show the bogus bitmap in its place.
			temp_bitmap = bogus_bitmap;
			egg->bad_bitmaps++;
			dtu_log( "EGG archive '%s': bitmap %d '%s' is not valid\n", filename, i, temp_name );
		}

		GameBitmapDiskFlags[Num_bitmap_files] = flags;
		piggy_register_bitmap( &temp_bitmap, temp_name, 1 );
	}

	// Sounds
	for (i=0; i<n_sounds; i++, h += sound_header_size )	{
		int data_length = egg_get_int( h+12 );
		int offset = egg_get_int( h+16 ) + data_start;
		int n = Num_sound_files;
		acm_info info;

		memcpy( temp_name_read, h, 8 );
		temp_name_read[8] = 0;

		temp_sound.length = 0;
		temp_sound.data = NULL;
		GameSoundDiskFlags[n] = (sound_header_size == DISK_SOUND_HEADER_SIZE) ? egg_get_int( h+20 ) : 0;
		GameSounds16[n] = NULL;
		GameSoundRate[n] = 0;
		GameSoundChannels[n] = 0;
		GameSoundValues[n] = 0;

		if ( offset < data_start || data_length < 0 || offset > size || data_length > size - offset )	{
			egg->bad_sounds++;
			dtu_log( "EGG archive '%s': sound %d '%s' is outside the file\n", filename, i, temp_name_read );
		} else if ( sound_header_size == DISK_SOUND_HEADER_SIZE_OLD )	{
			// Old layout: unsigned 8-bit samples, as in the Descent 1 PIG file.
			// Guess: the sample rate is the 11025 of Descent 1. No 16-bit copy.
			GameSoundRate[n] = 11025;
			GameSoundChannels[n] = 1;
			if ( read_sounds && data_length > 0 )	{
				temp_sound.data = (ubyte *) malloc( data_length );
				if ( temp_sound.data )	{
					memcpy( temp_sound.data, data+offset, data_length );
					temp_sound.length = data_length;
				}
			}
		} else if ( acm_get_info( data+offset, data_length, &info ) )	{
			GameSoundRate[n] = info.rate;
			GameSoundChannels[n] = info.channels;
			if ( read_sounds )	{
				GameSounds16[n] = acm_decode( data+offset, data_length, &info );
				if ( GameSounds16[n] )	{
					GameSoundValues[n] = info.total;
					temp_sound.data = piggy_make_8bit( GameSounds16[n], info.total, info.channels, &temp_sound.length );
				} else {
					egg->bad_sounds++;
					dtu_log( "EGG archive '%s': sound %d '%s' does not decode\n", filename, i, temp_name_read );
				}
			}
		} else {
			// Not an ACM stream. DTUGAME.EXE refuses such a sound ("DTU no
			// longer supports uncompressed sounds"). No shipped archive has one.
			egg->bad_sounds++;
			dtu_log( "EGG archive '%s': sound %d '%s' is not an ACM stream\n", filename, i, temp_name_read );
		}

		piggy_register_sound( &temp_sound, temp_name_read, 1 );
	}

	// Polygon models
	for (i=0; i<n_models; i++, h += DISK_MODEL_HEADER_SIZE )	{
		int offset = egg_get_int( h+0x38 ) + data_start;
		int mn = -1;

		if ( offset >= data_start && offset < size )
			mn = polyobj_load_dtu_model( h, data+offset, size-offset );
		if ( mn < 0 )	{
			// Keep the numbering of the models that follow: register an
			// empty model with the name of the bad one.
			egg->bad_models++;
			dtu_log( "EGG archive '%s': model %d is not valid\n", filename, i );
			if ( N_polygon_models < MAX_POLYGON_MODELS )	{
				memset( &Polygon_models[N_polygon_models], 0, sizeof(polymodel) );
				Pof_names[N_polygon_models][0] = 0;
				N_polygon_models++;
			}
		}
	}

	// ASCENT: the sounds of all archives have one sample rate (22050 in all
	// the shipped files). The mixer of digi.c plays every sound at
	// digi_sample_rate.
	for (i=egg->first_sound; i<Num_sound_files; i++ )
		if ( GameSoundRate[i] > 0 )	{
			digi_sample_rate = GameSoundRate[i];
			break;
		}

	dtu_log( "EGG archive '%s': %d bitmaps, %d sounds, %d models\n", filename, n_bitmaps, n_sounds, n_models );

	return Num_eggs++;
}

int piggy_get_num_eggs()
{
	return Num_eggs;
}

// The counts and the first table numbers of a loaded archive. Any pointer
// can be NULL. Returns the number of entries that were not valid.
int piggy_get_egg_counts( int egg_num, int *n_bitmaps, int *n_sounds, int *n_models, int *first_bitmap, int *first_sound, int *first_model )
{
	egg_file *egg;

	if ( egg_num < 0 || egg_num >= Num_eggs )
		return -1;
	egg = &Eggs[egg_num];
	if ( n_bitmaps ) *n_bitmaps = egg->n_bitmaps;
	if ( n_sounds ) *n_sounds = egg->n_sounds;
	if ( n_models ) *n_models = egg->n_models;
	if ( first_bitmap ) *first_bitmap = egg->first_bitmap;
	if ( first_sound ) *first_sound = egg->first_sound;
	if ( first_model ) *first_model = egg->first_model;
	return egg->bad_bitmaps + egg->bad_sounds + egg->bad_models;
}

int piggy_is_needed(int soundnum)
{
	return 1;
}

// ASCENT: the sounds are decoded when the archive is loaded.
void piggy_read_sounds()
{
}

// ASCENT: no bitmap is paged out. See the comment at the top.
void piggy_bitmap_page_in( bitmap_index bitmap )
{
}

void piggy_bitmap_page_out_all()
{
	// The bitmaps stay in memory. The two caches are still flushed, as in
	// Descent 1, because this is called when a new level starts.
	texmerge_flush();
	rle_cache_flush();
}

void piggy_load_level_data()
{
	piggy_bitmap_page_out_all();
	paging_touch_all();
}

void piggy_close()
{
	int i;

	if ( !Piggy_initialized )
		return;

	// ASCENT: stop the mixer before the sound data is freed (see above).
	plat_audio_flush();

	for (i=0; i<Num_sound_files; i++ )	{
		if ( GameSounds[i].data && GameSounds[i].data != bogus_data )
			free( GameSounds[i].data );
		GameSounds[i].data = NULL;
		if ( GameSounds16[i] )
			free( GameSounds16[i] );
		GameSounds16[i] = NULL;
	}
	Num_sound_files = 0;

	for (i=0; i<Num_eggs; i++ )
		if ( Eggs[i].data )	{
			free( Eggs[i].data );
			Eggs[i].data = NULL;
		}
	Num_eggs = 0;

	hashtable_free( &AllBitmapsNames );
	hashtable_free( &AllDigiSndNames );

	Piggy_initialized = 0;
}

int piggy_does_bitmap_exist_slow( char * name )
{
	int i;

	for (i=0; i<Num_bitmap_files; i++ )	{
		if ( !strcmp( AllBitmaps[i].name, name) )
			return 1;
	}
	return 0;
}
