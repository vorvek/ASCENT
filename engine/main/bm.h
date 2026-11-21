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
 * $Source: f:/miner/source/main/rcs/bm.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:32:59 $
 *
 * Bitmap and Palette loading functions.
 *
 * $Log: bm.h $
 * Revision 2.0  1995/02/27  11:32:59  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.60  1994/12/06  13:24:58  matt
 * Made exit model come out of bitmaps.tbl
 * 
 * Revision 1.59  1994/11/10  14:02:48  matt
 * Hacked in support for player ships with different textures
 * 
 * Revision 1.58  1994/10/27  13:32:17  john
 * Made TmapList only be in if editor
 * 
 * Revision 1.57  1994/10/11  12:25:20  matt
 * Added "hot rocks" that create badass explosion when hit by weapons
 * 
 * Revision 1.56  1994/09/29  12:38:13  matt
 * Took out include of digi.h, saving hours of unneeded compiles
 * 
 * Revision 1.55  1994/09/25  00:39:51  matt
 * Added the ability to make eclips (monitors, fans) which can be blown up
 * 
 * Revision 1.54  1994/09/17  01:40:59  matt
 * Added status bar/sizable window mode, and in the process revamped the
 * whole cockpit mode system.
 * 
 * Revision 1.53  1994/09/08  16:20:48  matt
 * Gave textures the ability to cause damage when scraped
 * 
 * Revision 1.52  1994/08/30  22:23:43  matt
 * Added cabability for player ship to have alternate model to use to
 * create debris.
 * 
 * Revision 1.51  1994/08/26  15:35:57  matt
 * Made eclips usable on more than one object at a time
 * 
 * Revision 1.50  1994/08/23  16:59:51  john
 * Added 3 cockpuits
 * 
 * Revision 1.49  1994/08/19  20:09:43  matt
 * Added end-of-level cut scene with external scene
 * 
 * Revision 1.48  1994/08/12  22:20:45  matt
 * Generalized polygon objects (such as control center)
 * 
 * Revision 1.47  1994/08/09  16:04:21  john
 * Added network players to editor.
 * 
 * Revision 1.46  1994/08/09  09:01:31  john
 * Increase MAX_TEXTURES
 * 
 * Revision 1.45  1994/07/13  00:14:57  matt
 * Moved all (or nearly all) of the values that affect player movement to 
 * bitmaps.tbl
 * 
 * Revision 1.44  1994/06/20  21:33:15  matt
 * Made bm.h not include sounds.h, to reduce dependencies
 * 
 * Revision 1.43  1994/06/15  11:55:58  matt
 * Added 3d model for player
 * 
 * Revision 1.42  1994/06/13  16:09:11  adam
 * increased max textures to 600
 * 
 * Revision 1.41  1994/06/06  15:38:58  john
 * Made fullscreen view be just another cockpit, but the "hole"
 * in the cockpit is just bigger.
 * 
 * Revision 1.40  1994/05/26  21:08:59  matt
 * Moved robot stuff out of polygon model and into robot_info struct
 * Made new file, robot.c, to deal with robots
 * 
 * Revision 1.39  1994/05/18  11:00:05  mike
 * Add robot_info stuff.
 * 
 * Revision 1.38  1994/05/17  14:44:56  mike
 * Get object type and id from ObjType and ObjId.
 * 
 * Revision 1.37  1994/05/17  12:04:45  mike
 * Deal with little known fact that polygon object != robot.
 * 
 * Revision 1.36  1994/05/16  16:17:35  john
 * Bunch of stuff on my Inferno Task list May16-23
 * 
 * Revision 1.35  1994/04/27  11:43:42  john
 * First version of sound! Yay!
 * 
 * Revision 1.34  1994/04/22  10:53:48  john
 * Increased MAX_TEXTURES to 500.
 * 
 * Revision 1.33  1994/04/01  11:15:05  yuan
 * Added multiple bitmap functionality to all objects...
 * (hostages, powerups, lasers, etc.)  
 * Hostages and powerups are implemented in the object system,
 * just need to finish function call to "affect" player.
 * 
 * Revision 1.32  1994/03/25  17:30:37  yuan
 * Checking in hostage stuff.
 * 
 * Revision 1.31  1994/03/17  18:07:28  yuan
 * Removed switch code... Now we just have Walls, Triggers, and Links...
 * 
 * Revision 1.30  1994/03/15  17:03:51  yuan
 * Added Robot/object bitmap capability
 * 
 * Revision 1.29  1994/03/15  16:32:58  yuan
 * Cleaned up bm-loading code.
 * (Fixed structures too)
 * 
 * Revision 1.28  1994/03/04  17:09:13  yuan
 * New door stuff.
 * 
 * Revision 1.27  1994/01/31  14:50:09  yuan
 * Added Robotex
 * 
 * Revision 1.26  1994/01/31  12:27:14  yuan
 * Added demo stuff (menu, etc.)
 * 
 * Revision 1.25  1994/01/25  17:11:43  john
 * New texmaped lasers.
 * 
 * Revision 1.24  1994/01/24  11:48:06  yuan
 * Lighting stuff
 * 
 * Revision 1.23  1994/01/22  13:40:15  yuan
 * Modified the bmd_bitmap structure a bit.
 * (Saves some memory, and added reflection)
 * 
 * Revision 1.22  1994/01/11  10:58:38  yuan
 * Added effects system
 * 
 * Revision 1.21  1994/01/06  17:13:12  john
 * Added Video clip functionality
 * 
 * Revision 1.20  1993/12/21  20:00:15  john
 * moved selector stuff to grs_bitmap
 * 
 * Revision 1.19  1993/12/21  19:33:58  john
 * Added selector to bmd_bitmap.
 * 
 * Revision 1.18  1993/12/07  12:28:48  john
 * moved bmd_palette to gr_palette
 * 
 * Revision 1.17  1993/12/06  18:40:37  matt
 * Changed object loading & handling
 * 
 * Revision 1.16  1993/12/05  23:05:03  matt
 * Added include of gr.h
 * 
 * Revision 1.15  1993/12/03  17:38:04  yuan
 * Ooops. meant to say:
 * Moved MAX variables to bm.c, Arrays left open.
 * 
 * Revision 1.14  1993/12/03  17:37:26  yuan
 * Added Asserts.
 * 
 * Revision 1.13  1993/12/02  17:22:54  yuan
 * New global var. Num_object_types
 * 
 * Revision 1.12  1993/12/02  16:34:39  yuan
 * Added fireball hack stuff.
 * 
 * Revision 1.11  1993/12/02  15:45:14  yuan
 * Added a buncha constants, variables, and function prototypes
 * for the new bitmaps.tbl format.
 * 
 * Revision 1.10  1993/12/01  11:25:11  yuan
 * Changed MALLOC'd buffers for filename and type in
 * the bmd_bitmap structure into arrays... Saves time
 * at load up.
 * 
 * Revision 1.9  1993/12/01  00:28:09  yuan
 * New bitmap system structure.
 * 
 * Revision 1.8  1993/11/03  11:34:08  john
 * made it use bitmaps.tbl
 * 
 * Revision 1.7  1993/10/26  18:11:03  john
 * made all palette data be statically allocated
 * 
 * Revision 1.6  1993/10/19  12:17:51  john
 * *** empty log message ***
 * 
 * Revision 1.5  1993/10/16  20:02:41  matt
 * Changed name of backdrop bitmap file
 * 
 * Revision 1.4  1993/10/12  15:08:52  matt
 * Added a bunch of new textures
 * 
 * Revision 1.3  1993/10/12  12:30:41  john
 * *** empty log message ***
 * 
 * Revision 1.2  1993/10/12  11:27:58  john
 * added more bitmaps
 * 
 * Revision 1.1  1993/09/23  13:09:10  john
 * Initial revision
 * 
 *
 */

#ifndef _BM_H
#define _BM_H

#include "gr.h"
#include "piggy.h"

// ASCENT: was 800. 2500 is the constant of the MAX_TEXTURES asserts in
// DTUGAME.EXE. A level table of the game defines 1489 textures.
#define MAX_TEXTURES		2500
#define BM_MAX_ARGS		10

//tmapinfo flags
#define TMI_VOLATILE		1		//this material blows up when hit

typedef struct {
	char			filename[13];
	ubyte			flags;
	fix			lighting;		// 0 to 1
	fix			damage;			//how much damage being against this does
	int			eclip_num;		//if not -1, the eclip that changes this   
} tmap_info;

extern int Num_object_types;

#define N_COCKPIT_BITMAPS 4
extern int Num_cockpits;
extern bitmap_index cockpit_bitmap[N_COCKPIT_BITMAPS];

extern int Num_tmaps;
extern int TmapList[MAX_TEXTURES];		// ASCENT: was under EDITOR, bmread.c is always built

extern tmap_info TmapInfo[MAX_TEXTURES];

//for each model, a model number for dying & dead variants, or -1 if none
extern int Dying_modelnums[];
extern int Dead_modelnums[];

// Initializes the palette, bitmap system...
int bm_init();
void bm_close();

// ASCENT: the game tables of Descent to Undermountain. Descent 1 read its
// tables from the PIG file (bm_read_all, removed). In Descent to
// Undermountain each level has a table file LEVEL.BIN (packed: LEVEL.HEN).
// It names the EGG archive of the level in its first 13 bytes. The rest
// is the encoded text of a BITMAPS.TBL file. See docs/formats/tbl.md.
//
// Start code: bm_init() once, after the palette is loaded.
// Level loader, before the mine is read:
//
//   bm_load_level_tables( "ypinn", "hmc" );
//
// The first name is the level name without extension. The second is the
// 3-letter name of the player character archive, or NULL. The function
// releases the archives and tables of the level before, loads the player
// archive (HMC.EGG), then the archive that LEVEL.BIN names, then parses
// the table text. Where the table has its $PLAYER line it reads the lines
// of the player table (HMC.BIN) in place of that line, as DTUGAME.EXE
// does. Returns 1, or 0 if a file is missing or not valid. A call with
// the names of the tables that are loaded already returns 1 at once.
int bm_load_level_tables( char * level_name, char * player_name );

// The names of the tables that are loaded ("" if none), and the name of
// the level archive.
extern char Bm_level_name[16];
extern char Bm_player_name[16];
extern char Bm_egg_name[16];

// Number of table lines that the parser did not understand in the last
// bm_load_level_tables(). Each one gives a line on the message window.
extern int Bm_tbl_warnings;

// ASCENT: the name of the old function. It loads the tables of the first
// level ("ypinn") with the default player character. For code that still
// calls it.
int bm_init_use_tbl();

// Decode the text of a BIN table file in place: all bytes after the first
// 13. See docs/formats/egg.md.
void bm_decode_bin_text( ubyte * text, int len );

// Initializes the Texture[] array of bmd_bitmap structures.
void init_textures();

#define OL_ROBOT 				1
#define OL_HOSTAGE 			2
#define OL_POWERUP 			3
#define OL_CONTROL_CENTER	4
#define OL_PLAYER				5
#define OL_CLUTTER			6		//some sort of misc object
#define OL_EXIT				7		//the exit model for external scenes
// ASCENT: the classes of Descent to Undermountain. The numbers are those
// of this port. The numbers of DTUGAME.EXE are not known.
#define OL_WEAPON				8
#define OL_ARMOR				9
#define OL_MISC				10
#define OL_SPELL				11
#define OL_DRESSING			12

// ASCENT: was 100. 430 is the constant of the MAX_OBJTYPE asserts in
// DTUGAME.EXE.
#define	MAX_OBJTYPE			430

extern int Num_total_object_types;		//	Total number of object types, including robots, hostages, powerups, control centers, faces
extern byte	ObjType[MAX_OBJTYPE];		// Type of an object, such as Robot, eg if ObjType[11] == OL_ROBOT, then object #11 is a robot
extern ubyte	ObjId[MAX_OBJTYPE];			// ID of a robot, within its class, eg if ObjType[11] == 3, then object #11 is the third robot
extern fix	ObjStrength[MAX_OBJTYPE];	// initial strength of each object

// ASCENT: was 210. 1700 is the constant of the MAX_OBJ_BITMAPS and
// MAX_OBJ_BITMAP_PTRS asserts in DTUGAME.EXE.
#define MAX_OBJ_BITMAPS				1700
extern bitmap_index ObjBitmaps[MAX_OBJ_BITMAPS];
extern ushort ObjBitmapPtrs[MAX_OBJ_BITMAPS];
extern int First_multi_bitmap_num;

// ---------------------------------------------------------------------
// ASCENT: tables of Descent to Undermountain that Descent 1 does not have.
// The limits with "EXE" are assert constants of DTUGAME.EXE. The others
// are sized from the data.

#define MAX_PLAYER_TYPES		4			// four $PLAYER lines at most in a table
#define MAX_PLAYER_MODELS		64			// largest $PLAYER line: 49 models
#define MAX_DRESSING_TYPES		60			// EXE
#define MAX_ARMOR_TYPES			80			// EXE
#define MAX_MISC_TYPES			250		// EXE
#define MAX_SPELL_TYPES			100		// data: 56
#define MAX_INTERFACE_BITMAPS	400		// data: 149
#define MAX_ICON_BITMAPS		16
#define MAX_TBL_RECORDS			1024		// data: 420 lines of the classes below
#define DTU_N_STATE_TIMES		19			// model header 0x8E has 19 values
#define DTU_N_SPEEDS				9
#define DTU_N_FIRE_FRAMES		4
#define DTU_NAME_LENGTH			40
#define DTU_NDL					5			// same as NDL of game.h

// One table line of an object class ($PLAYER, $OBJECT, $ARMOR, $SPELL,
// $WEAPON, $MONSTER, $MONSTER_AI, $DRESSING), kept as text. The parser
// decodes only a part of the keywords. bm_tbl_get() finds any keyword.
typedef struct tbl_record {
	char		section[12];		// "$MONSTER" and so on
	int		linenum;
	int		id;					// number of the entry in its class
	char		*text;				// the line without the section word. Spaces inside quotes are kept.
} tbl_record;

extern int N_tbl_records;
extern tbl_record Tbl_records[MAX_TBL_RECORDS];

// Find "key=value" in a record. Copies the value (without quotes; a list
// of numbers up to the next keyword) to value. Returns 1 if the key is
// there. bm_tbl_has_flag() looks for a word without "=".
int bm_tbl_get( int record, char * key, char * value, int value_size );
int bm_tbl_has_flag( int record, char * flag );
// The word number of the last "flag" of the record, or -1.
int bm_tbl_flag_pos( int record, char * flag );

// $PLAYER
typedef struct player_type_info {
	char			name[DTU_NAME_LENGTH];
	int			n_models;
	int			model_nums[MAX_PLAYER_MODELS];	// the first one is the normal body
	bitmap_index picture;
	fix			eye_offset;
	fix			gravity;
	fix			state_times[DTU_N_STATE_TIMES];
	fix			speeds[DTU_N_SPEEDS];
	short			fire_frames[DTU_N_FIRE_FRAMES];
	int			record;				// index of Tbl_records
} player_type_info;

extern int N_player_types;
extern player_type_info Player_type_info[MAX_PLAYER_TYPES];

// $MONSTER and $MONSTER_AI: the values that robot_info has no place for.
// Entry i belongs to Robot_info[i].
#define MONSTER_F_UNDEAD				0x0001
#define MONSTER_F_HUMANOID				0x0002
#define MONSTER_F_FLYING				0x0004
#define MONSTER_F_NO_SCARE				0x0008
#define MONSTER_F_NO_SLEEP				0x0010
#define MONSTER_F_FIRE_IMMUNE			0x0020
#define MONSTER_F_ELECTRICITY_IMMUNE	0x0040
#define MONSTER_F_WEAPON_RESISTANT	0x0080
#define MONSTER_F_EXTRAPLANAR			0x0100
#define MONSTER_F_BLIND					0x0200
#define MONSTER_F_LIGHT_SENSITIVE	0x0400
#define MONSTER_F_DARK_LOVER			0x0800
#define MONSTER_F_LIGHT_LOVER			0x1000
#define MONSTER_F_PASS_THRU			0x2000
#define MONSTER_F_NOATTACK				0x4000

typedef struct monster_info {
	char			name[DTU_NAME_LENGTH];
	bitmap_index picture;
	int			flags;				// MONSTER_F_
	int			thaco, armor_class, max_hit_points, str, iq, moola;
	int			die_sound, hit_sound;
	fix			gravity;
	fix			state_times[DTU_N_STATE_TIMES];
	fix			speeds[DTU_N_SPEEDS];
	short			fire_frames[DTU_N_FIRE_FRAMES];
	short			ai_values[2][DTU_NDL];	// the last two groups of $MONSTER_AI, meaning unknown
	int			ai_first_number;		// the first number of $MONSTER_AI, read and not used by DTUGAME.EXE
	int			record;					// $MONSTER line, index of Tbl_records
	int			ai_record;				// $MONSTER_AI line
} monster_info;

extern monster_info Monster_info[];

// $DRESSING: scenery objects (OBJ_DRESSING). An entry draws as a polygon
// model, as a vclip or as one bitmap.
#define DRESSING_F_MAGIC			1
#define DRESSING_F_CANT_BREAK		2

typedef struct dressing_info {
	char			name[DTU_NAME_LENGTH];
	int			model_num;			// pof=, or -1
	int			vclip_num;			// vclip=, or -1
	int			obj_bitmap;			// bitmap=: slot of ObjBitmapPtrs, or -1. It can be an animating slot (%N).
	fix			size;
	fix			lightcast;
	int			explode_sound;
	int			break_vclip;
	int			pushable;
	int			pass_thru;
	int			flags;				// DRESSING_F_
	int			record;
} dressing_info;

extern int N_dressing_types;
extern dressing_info Dressing_info[MAX_DRESSING_TYPES];

// $OBJECT (OBJ_MISC), $ARMOR (OBJ_ARMOR), $SPELL (OBJ_SPELL) and the item
// part of $WEAPON. Only the values that every item has are decoded.
typedef struct item_info {
	char			name[DTU_NAME_LENGTH];
	int			obj_bitmap;			// picture=: slot of ObjBitmapPtrs, or -1
	int			icon;					// icon=, or -1
	int			inv_icon;			// inv_icon=, or -1
	int			value;
	fix			mass;					// mass= or weight=
	int			stack_limit;
	int			model_num;			// weapon_pof= of a spell, or -1
	int			vclip_num;			// weapon_vclip= of a spell, or -1
	int			record;
} item_info;

extern int N_misc_types;
extern item_info Misc_info[MAX_MISC_TYPES];
extern int N_armor_types;
extern item_info Armor_info[MAX_ARMOR_TYPES];
extern int N_spell_types;
extern item_info Spell_info[MAX_SPELL_TYPES];
extern item_info Weapon_item_info[];		// entry i belongs to Weapon_info[i]

// $INTERFACE and $ICONS bitmaps, in table order.
extern int N_interface_bitmaps;
extern bitmap_index Interface_bitmaps[MAX_INTERFACE_BITMAPS];
extern int N_icon_bitmaps;
extern bitmap_index Icon_bitmaps[MAX_ICON_BITMAPS];


#endif
