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
 * $Source: f:/miner/source/main/rcs/bm.c $
 * $Revision: 2.3 $
 * $Author: john $
 * $Date: 1995/03/14 16:22:04 $
 *
 * Bitmap and palette loading functions.
 *
 * $Log: bm.c $
 * Revision 2.3  1995/03/14  16:22:04  john
 * Added cdrom alternate directory stuff.
 * 
 * Revision 2.2  1995/03/07  16:51:48  john
 * Fixed robots not moving without edtiro bug.
 * 
 * Revision 2.1  1995/03/06  15:23:06  john
 * New screen techniques.
 * 
 * Revision 2.0  1995/02/27  11:27:05  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: bm.c 2.3 1995/03/14 16:22:04 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>

#include "types.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "mem.h"
// ASCENT: cflib.h (not in the tree, not needed) is removed.
#include "mono.h"
#include "error.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff.h"
#include "cfile.h"
#include "hostage.h"
#include "powerup.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "dataio.h"		// ASCENT: little endian field reads
// ASCENT: was ubyte, see sounds.h
ushort Sounds[MAX_SOUNDS];
ushort AltSounds[MAX_SOUNDS];

int Num_total_object_types;

byte	ObjType[MAX_OBJTYPE];
ubyte	ObjId[MAX_OBJTYPE];		// ASCENT: was byte. An item type number can be above 127.
fix	ObjStrength[MAX_OBJTYPE];

//for each model, a model number for dying & dead variants, or -1 if none
int Dying_modelnums[MAX_POLYGON_MODELS];
int Dead_modelnums[MAX_POLYGON_MODELS];

//right now there's only one player ship, but we can have another by
//adding an array and setting the pointer to the active ship.
player_ship only_player_ship,*Player_ship=&only_player_ship;

//----------------- Miscellaneous bitmap pointers ---------------
int					Num_cockpits = 0;
bitmap_index		cockpit_bitmap[N_COCKPIT_BITMAPS];

//---------------- Variables for wall textures ------------------
int 					Num_tmaps;
tmap_info 			TmapInfo[MAX_TEXTURES];

//---------------- Variables for object textures ----------------

int					First_multi_bitmap_num=-1;

bitmap_index		ObjBitmaps[MAX_OBJ_BITMAPS];
ushort				ObjBitmapPtrs[MAX_OBJ_BITMAPS];		// These point back into ObjBitmaps, since some are used twice.

// ASCENT: the tables of Descent to Undermountain that Descent 1 does not
// have. bmread.c fills them from the table text. See bm.h.
int					N_player_types = 0;
player_type_info	Player_type_info[MAX_PLAYER_TYPES];

monster_info		Monster_info[MAX_ROBOT_TYPES];

int					N_dressing_types = 0;
dressing_info		Dressing_info[MAX_DRESSING_TYPES];

int					N_misc_types = 0;
item_info			Misc_info[MAX_MISC_TYPES];

int					N_armor_types = 0;
item_info			Armor_info[MAX_ARMOR_TYPES];

int					N_spell_types = 0;
item_info			Spell_info[MAX_SPELL_TYPES];

item_info			Weapon_item_info[MAX_WEAPON_TYPES];

int					N_interface_bitmaps = 0;
bitmap_index		Interface_bitmaps[MAX_INTERFACE_BITMAPS];

int					N_icon_bitmaps = 0;
bitmap_index		Icon_bitmaps[MAX_ICON_BITMAPS];

int					N_tbl_records = 0;
tbl_record			Tbl_records[MAX_TBL_RECORDS];

//-----------------------------------------------------------------
// ASCENT: Descent 1 read the game tables from the PIG file here
// (bm_read_all). Descent to Undermountain has no tables in its archives.
// The tables come from the table text of a level (bm_load_level_tables in
// bmread.c). This function makes the empty archive tables and stays as
// the call of the start code.
int bm_init()
{
	static int done = 0;

	if (done)
		return 0;
	done = 1;

	init_polygon_models();
	piggy_init();
	return 0;
}
