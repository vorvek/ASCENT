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
 * $Source: f:/miner/source/main/rcs/gamesave.c $
 * $Revision: 2.2 $
 * $Author: john $
 * $Date: 1995/04/23 14:53:12 $
 * 
 * Save game information
 * 
 * $Log: gamesave.c $
 * Revision 2.2  1995/04/23  14:53:12  john
 * Made some mine structures read in with no structure packing problems.
 * 
 * Revision 2.1  1995/03/20  18:15:43  john
 * Added code to not store the normals in the segment structure.
 * 
 * Revision 2.0  1995/02/27  11:29:50  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.207  1995/02/23  10:17:36  allender
 * fixed parameter mismatch with compute_segment_center
 * 
 * Revision 1.206  1995/02/22  14:51:17  allender
 * fixed some things that I missed
 * 
 * Revision 1.205  1995/02/22  13:31:38  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.204  1995/02/01  20:58:08  john
 * Made editor check hog.
 * 
 * Revision 1.203  1995/01/28  17:40:34  mike
 * correct level names (use rdl, sdl) for dumpmine stuff.
 * 
 * Revision 1.202  1995/01/25  20:03:46  matt
 * Moved matrix check to avoid orthogonalizing an uninitialize matrix
 * 
 * Revision 1.201  1995/01/20  16:56:53  mike
 * remove some mprintfs.
 * 
 * Revision 1.200  1995/01/15  19:42:13  matt
 * Ripped out hostage faces for registered version
 * 
 * Revision 1.199  1995/01/05  16:59:09  yuan
 * Make it so if editor is loaded, don't get error from typo
 * in filename.
 * 
 * Revision 1.198  1994/12/19  12:49:46  mike
 * Change fgets to cfgets.  fgets was getting a pointer mismatch warning.
 * 
 * Revision 1.197  1994/12/12  01:20:03  matt
 * Took out object size hack for green claw guys
 * 
 * Revision 1.196  1994/12/11  13:19:37  matt
 * Restored calls to fix_object_segs() when debugging is turned off, since
 * it's not a big routine, and could fix some possibly bad problems.
 * 
 * Revision 1.195  1994/12/10  16:17:24  mike
 * fix editor bug that was converting transparent walls into rock.
 * 
 * Revision 1.194  1994/12/09  14:59:27  matt
 * Added system to attach a fireball to another object for rendering purposes,
 * so the fireball always renders on top of (after) the object.
 * 
 * Revision 1.193  1994/12/08  17:19:02  yuan
 * Cfiling stuff.
 * 
 * Revision 1.192  1994/12/02  20:01:05  matt
 * Always give vulcan cannon powerup same amount of ammo, regardless of
 * how much it was saved with
 * 
 * Revision 1.191  1994/11/30  17:45:57  yuan
 * Saving files now creates RDL/SDLs instead of CDLs.
 * 
 * Revision 1.190  1994/11/30  17:22:14  matt
 * Ripped out hostage faces in shareware version
 * 
 * Revision 1.189  1994/11/28  00:09:30  allender
 * commented out call to newdemo_record_start_demo in load_level...what is
 * this doing here anyway?????
 * 
 * Revision 1.188  1994/11/27  23:13:48  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.187  1994/11/27  18:06:20  matt
 * Cleaned up LVL/CDL file loading
 * 
 * Revision 1.186  1994/11/25  22:46:29  matt
 * Allow ESC out of compiled/normal menu (esc=compiled).
 * 
 * Revision 1.185  1994/11/23  12:18:35  mike
 * move level names here...a more logical place than dumpmine.
 * 
 * Revision 1.184  1994/11/21  20:29:19  matt
 * If hostage info is bad, fix it.
 * 
 * Revision 1.183  1994/11/21  20:26:07  matt
 * Fixed bug, I hope
 * 
 * Revision 1.182  1994/11/21  20:20:37  matt
 * Fixed stupid mistake
 * 
 * Revision 1.181  1994/11/21  20:18:40  matt
 * Fixed (hopefully) totally bogus writing of hostage data
 * 
 * Revision 1.180  1994/11/20  14:11:56  matt
 * Gracefully handle two hostages having same id
 * 
 * Revision 1.179  1994/11/19  23:55:05  mike
 * remove Assert, put in comment for Matt.
 * 
 * Revision 1.178  1994/11/19  19:53:24  matt
 * Added code to full support different hostage head clip & message for
 * each hostage.
 * 
 * Revision 1.177  1994/11/19  15:15:21  mike
 * remove unused code and data
 * 
 * Revision 1.176  1994/11/19  10:28:28  matt
 * Took out write routines when editor compiled out
 * 
 * Revision 1.175  1994/11/17  20:38:25  john
 * Took out warning.
 * 
 * Revision 1.174  1994/11/17  20:36:34  john
 * Made it so that saving a mine will write the .cdl even
 * if .lvl gets error.
 * 
 * Revision 1.173  1994/11/17  20:26:19  john
 * Made the game load whichever of .cdl or .lvl exists,
 * and if they both exist, prompt the user for which one.
 * 
 * Revision 1.172  1994/11/17  20:11:20  john
 * Fixed warning.
 * 
 * Revision 1.171  1994/11/17  20:09:26  john
 * Added new compiled level format.
 * 
 * Revision 1.170  1994/11/17  14:57:21  mike
 * moved segment validation functions from editor to main.
 * 
 * Revision 1.169  1994/11/17  11:39:21  matt
 * Ripped out code to load old mines
 * 
 * Revision 1.168  1994/11/16  11:24:53  matt
 * Made attack-type robots have smaller radius, so they get closer to player
 * 
 * Revision 1.167  1994/11/15  21:42:47  mike
 * better error messages.
 * 
 * Revision 1.166  1994/11/15  15:30:41  matt
 * Save ptr to name of level being loaded
 * 
 * Revision 1.165  1994/11/14  20:47:46  john
 * Attempted to strip out all the code in the game 
 * directory that uses any ui code.
 * 
 * Revision 1.164  1994/11/14  14:34:23  matt
 * Fixed up handling when textures can't be found during remap
 * 
 * Revision 1.163  1994/11/10  14:02:49  matt
 * Hacked in support for player ships with different textures
 * 
 * Revision 1.162  1994/11/06  14:38:17  mike
 * Remove an apparently unnecessary mprintf.
 * 
 * Revision 1.161  1994/10/30  14:11:28  mike
 * ripout local segments stuff.
 * 
 * Revision 1.160  1994/10/28  12:10:41  matt
 * Check that was supposed to happen only when editor was in was happening
 * only when editor was out.
 * 
 * Revision 1.159  1994/10/27  11:25:32  matt
 * Only do connectivity error check when editor in
 * 
 * Revision 1.158  1994/10/27  10:54:00  matt
 * Made connectivity error checking put up warning if errors found
 * 
 * Revision 1.157  1994/10/25  10:50:54  matt
 * Vulcan cannon powerups now contain ammo count
 * 
 * Revision 1.156  1994/10/23  02:10:43  matt
 * Got rid of obsolete hostage_info stuff
 * 
 * Revision 1.155  1994/10/22  18:57:26  matt
 * Added call to check_segment_connections()
 * 
 * Revision 1.154  1994/10/21  12:19:23  matt
 * Clear transient objects when saving (& loading) games
 * 
 * Revision 1.153  1994/10/21  11:25:10  mike
 * Use new constant IMMORTAL_TIME.
 * 
 * Revision 1.152  1994/10/20  12:46:59  matt
 * Replace old save files (MIN/SAV/HOT) with new LVL files
 * 
 * Revision 1.151  1994/10/19  19:26:32  matt
 * Fixed stupid bug
 * 
 * Revision 1.150  1994/10/19  16:46:21  matt
 * Made tmap overrides for robots remap texture numbers
 * 
 * Revision 1.149  1994/10/18  08:50:27  yuan
 * Fixed correct variable this time.
 * 
 * Revision 1.148  1994/10/18  08:45:02  yuan
 * Oops. forgot load function.
 * 
 * Revision 1.147  1994/10/18  08:42:10  yuan
 * Avoid the int3.
 * 
 * Revision 1.146  1994/10/17  21:34:57  matt
 * Added support for new Control Center/Main Reactor
 * 
 * Revision 1.145  1994/10/15  19:06:34  mike
 * Fix bug, maybe, having to do with something or other, ...
 * 
 * Revision 1.144  1994/10/12  21:07:33  matt
 * Killed unused field in object structure
 * 
 * Revision 1.143  1994/10/06  14:52:55  mike
 * Put check in to detect possibly bogus walls in last segment which leaked through an earlier check
 * due to misuse of Highest_segment_index.
 * 
 * Revision 1.142  1994/10/05  22:12:44  mike
 * Put in cleanup for matcen/fuelcen links.
 * 
 * Revision 1.141  1994/10/03  11:30:05  matt
 * Make sure player in a valid segment before saving
 * 
 * Revision 1.140  1994/09/28  11:14:41  mike
 * Better error messaging on bogus mines: Only bring up dialog box if a "real" (level??.*) level.
 * 
 * Revision 1.139  1994/09/28  09:22:58  mike
 * Comment out a mprintf.
 * 
 * Revision 1.138  1994/09/27  17:08:36  mike
 * Message boxes when you load bogus mines.
 * 
 * Revision 1.137  1994/09/27  15:43:45  mike
 * Move the dump stuff to dumpmine.
 * 
 * Revision 1.136  1994/09/27  00:02:31  mike
 * Dump text files (".txm") when loading a mine, showing all kinds of useful mine info.
 * 
 * Revision 1.135  1994/09/26  11:30:41  matt
 * Took out code which loaded bogus player structure
 * 
 * Revision 1.134  1994/09/26  11:18:44  john
 * Fixed some conflicts with newseg.
 * 
 * Revision 1.133  1994/09/26  10:56:58  matt
 * Fixed inconsistancies in lifeleft for immortal objects
 * 
 * Revision 1.132  1994/09/25  23:41:10  matt
 * Changed the object load & save code to read/write the structure fields one
 * at a time (rather than the whole structure at once).  This mean that the
 * object structure can be changed without breaking the load/save functions.
 * As a result of this change, the local_object data can be and has been 
 * incorporated into the object array.  Also, timeleft is now a property 
 * of all objects, and the object structure has been otherwise cleaned up.
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: gamesave.c 2.2 1995/04/23 14:53:12 john Exp $";
#pragma on (unreferenced)


// ASCENT: io.h (DOS) and cflib.h (not in the tree) are removed.
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mono.h"
#include "key.h"
#include "gr.h"
#include "palette.h"
#include "newmenu.h"

#include "inferno.h"
#ifdef EDITOR
#include "editor\editor.h"
#endif
#include "error.h"
#include "object.h"
#include "game.h"
#include "screens.h"
#include "wall.h"
#include "gamemine.h"
#include "robot.h"


#include "cfile.h"
#include "bm.h"
#include "menu.h"
#include "switch.h"
#include "fuelcen.h"
#include "powerup.h"
#include "hostage.h"
#include "weapon.h"
#include "newdemo.h"
#include "gameseq.h"
#include "automap.h"
#include "polyobj.h"
#include "text.h"
#include "gamefont.h"
#include "gamesave.h"
#include "gameseg.h"		// ASCENT: find_connect_side() and others were implicit
#include "textures.h"
#include "dataio.h"		// ASCENT: little endian field reads and the record sizes
#include "dtufile.h"		// ASCENT: dtu_load_file()
#include "mem.h"			// ASCENT: dtu_load_file() memory is released through the debug allocator
#include <stdint.h>

// ASCENT: see gamesave.h. Selects the mine format for load_level().
int Gamesave_level_format = LEVEL_FORMAT_AUTO;

// ASCENT: prototypes of functions that the original called without one
int load_game_data(CFILE *LoadFile);
void dump_mine_info(void);

#ifdef EDITOR
#ifdef SHAREWARE
char *Shareware_level_names[NUM_SHAREWARE_LEVELS] = {
	"level01.sdl",
	"level02.sdl",
	"level03.sdl",
	"level04.sdl",
	"level05.sdl",
	"level06.sdl",
	"level07.sdl"
};
#else
char *Shareware_level_names[NUM_SHAREWARE_LEVELS] = {
	"level01.rdl",
	"level02.rdl",
	"level03.rdl",
	"level04.rdl",
	"level05.rdl",
	"level06.rdl",
	"level07.rdl"
};
#endif

char *Registered_level_names[NUM_REGISTERED_LEVELS] = {
	"level08.rdl",
	"level09.rdl",
	"level10.rdl",
	"level11.rdl",
	"level12.rdl",
	"level13.rdl",
	"level14.rdl",
	"level15.rdl",
	"level16.rdl",
	"level17.rdl",
	"level18.rdl",
	"level19.rdl",
	"level20.rdl",
	"level21.rdl",
	"level22.rdl",
	"level23.rdl",
	"level24.rdl",
	"level25.rdl",
	"level26.rdl",
	"level27.rdl",
	"levels1.rdl",
	"levels2.rdl",
	"levels3.rdl"
};
#endif

char Gamesave_current_filename[128];

#define GAME_VERSION					25
#define GAME_COMPATIBLE_VERSION	22

// ASCENT: the sizes of game_top_fileinfo and game_fileinfo in the file
// (1 byte packing). The loader reads the fields one at a time.
#define GAME_TOP_FILEINFO_DISK_SIZE	8
#define GAME_FILEINFO_DISK_SIZE		119	// 2+2+4+15+24*4

#define MENU_CURSOR_X_MIN			MENU_X
#define MENU_CURSOR_X_MAX			MENU_X+6

#define HOSTAGE_DATA_VERSION	0

//Start old wall structures

typedef struct v16_wall {
	byte  type; 			  	// What kind of special wall.
	byte	flags;				// Flags for the wall.		
	fix   hps;				  	// "Hit points" of the wall. 
	byte	trigger;				// Which trigger is associated with the wall.
	byte	clip_num;			// Which	animation associated with the wall. 
	byte	keys;
	} v16_wall;

typedef struct v19_wall {
	int	segnum,sidenum;	// Seg & side for this wall
	byte	type; 			  	// What kind of special wall.
	byte	flags;				// Flags for the wall.		
	fix   hps;				  	// "Hit points" of the wall. 
	byte	trigger;				// Which trigger is associated with the wall.
	byte	clip_num;			// Which	animation associated with the wall. 
	byte	keys;
	int	linked_wall;		// number of linked wall
	} v19_wall;

typedef struct v19_door {
	int		n_parts;					// for linked walls
	short 	seg[2]; 					// Segment pointer of door.
	short 	side[2];					// Side number of door.
	short 	type[2];					// What kind of door animation.
	fix 		open;						//	How long it has been open.
} v19_door;

//End old wall structures

struct {
	ushort 	fileinfo_signature;
	ushort	fileinfo_version;
	int		fileinfo_sizeof;
} game_top_fileinfo;    // Should be same as first two fields below...

struct {
	ushort 	fileinfo_signature;
	ushort	fileinfo_version;
	int		fileinfo_sizeof;
	char		mine_filename[15];
	int		level;
	int		player_offset;				// Player info
	int		player_sizeof;
	int		object_offset;				// Object info
	int		object_howmany;    	
	int		object_sizeof;  
	int		walls_offset;
	int		walls_howmany;
	int		walls_sizeof;
	int		doors_offset;
	int		doors_howmany;
	int		doors_sizeof;
	int		triggers_offset;
	int		triggers_howmany;
	int		triggers_sizeof;
	int		links_offset;
	int		links_howmany;
	int		links_sizeof;
	int		control_offset;
	int		control_howmany;
	int		control_sizeof;
	int		matcen_offset;
	int		matcen_howmany;
	int		matcen_sizeof;
} game_fileinfo;

#ifdef EDITOR
extern char mine_filename[];
extern int save_mine_data_compiled(FILE * SaveFile);
//--unused-- #else
//--unused-- char mine_filename[128];
#endif

int Gamesave_num_org_robots = 0;
//--unused-- grs_bitmap * Gamesave_saved_bitmap = NULL;

#ifdef EDITOR
//	Return true if this level has a name of the form "level??"
//	Note that a pathspec can appear at the beginning of the filename.
int is_real_level(char *filename)
{
	int	len = strlen(filename);

	if (len < 6)
		return 0;

	//mprintf((0, "String = [%s]\n", &filename[len-11]));
	return !strnicmp(&filename[len-11], "level", 5);

}

void convert_name_to_CDL( char *dest, char *src )
{
	int i;

	strcpy (dest, src);

#ifdef SHAREWARE
	for (i=1; i<strlen(dest); i++ )
	{
		if (dest[i]=='.'||dest[i]==' '||dest[i]==0)
		{
			dest[i]='.';
			dest[i+1]='S';
			dest[i+2]= 'D';
			dest[i+3]= 'L';
			dest[i+4]=0;
			return;
		}
	}

	if (i < 123)
	{
		dest[i]='.';
		dest[i+1]='S';
		dest[i+2]= 'D';
		dest[i+3]= 'L';
		dest[i+4]=0;
		return;
	}
#else
	for (i=1; i<strlen(dest); i++ )
	{
		if (dest[i]=='.'||dest[i]==' '||dest[i]==0)
		{
			dest[i]='.';
			dest[i+1]='R';
			dest[i+2]= 'D';
			dest[i+3]= 'L';
			dest[i+4]=0;
			return;
		}
	}

	if (i < 123)
	{
		dest[i]='.';
		dest[i+1]='R';
		dest[i+2]= 'D';
		dest[i+3]= 'L';
		dest[i+4]=0;
		return;
	}
#endif




}
#endif

void convert_name_to_LVL( char *dest, char *src )
{
	int i;

	strcpy (dest, src);

	for (i=1; i<strlen(dest); i++ )
	{
		if (dest[i]=='.'||dest[i]==' '||dest[i]==0)
		{
			dest[i]='.';
			dest[i+1]='L';
			dest[i+2]= 'V';
			dest[i+3]= 'L';
			dest[i+4]=0;
			return;
		}
	}

	if (i < 123)
	{
		dest[i]='.';
		dest[i+1]='L';
		dest[i+2]= 'V';
		dest[i+3]= 'L';
		dest[i+4]=0;
		return;
	}
}

//--unused-- vms_angvec zero_angles={0,0,0};

#define vm_angvec_zero(v) do {(v)->p=(v)->b=(v)->h=0;} while (0)

int Gamesave_num_players=0;

int N_save_pof_names=25;
char Save_pof_names[MAX_POLYGON_MODELS][13];

int check_and_fix_matrix(vms_matrix *m);		// ASCENT: "int" added, the type was implicit

void verify_object( object * obj )	{

	obj->lifeleft = IMMORTAL_TIME;		//all loaded object are immortal, for now

	if ( obj->type == OBJ_ROBOT )	{
		Gamesave_num_org_robots++;

		// Make sure valid id...
		if ( obj->id >= N_robot_types )
			obj->id = obj->id % N_robot_types;

		// Make sure model number & size are correct...		
		if ( obj->render_type == RT_POLYOBJ ) {
			obj->rtype.pobj_info.model_num = Robot_info[obj->id].model_num;
			obj->size = Polygon_models[obj->rtype.pobj_info.model_num].rad;

			//@@if (obj->control_type==CT_AI && Robot_info[obj->id].attack_type)
			//@@	obj->size = obj->size*3/4;
		}

		if (obj->movement_type == MT_PHYSICS) {
			obj->mtype.phys_info.mass = Robot_info[obj->id].mass;
			obj->mtype.phys_info.drag = Robot_info[obj->id].drag;
		}
	}
	else {		//Robots taken care of above

		if ( obj->render_type == RT_POLYOBJ ) {
			int i;
			char *name = Save_pof_names[obj->rtype.pobj_info.model_num];

			for (i=0;i<N_polygon_models;i++)
				if (!stricmp(Pof_names[i],name)) {		//found it!	
					// mprintf((0,"Mapping <%s> to %d (was %d)\n",name,i,obj->rtype.pobj_info.model_num));
					obj->rtype.pobj_info.model_num = i;
					break;
				}
		}
	}

	if ( obj->type == OBJ_POWERUP ) {
		if ( obj->id >= N_powerup_types )	{
			obj->id = 0;
			Assert( obj->render_type != RT_POLYOBJ );
		}
		obj->control_type = CT_POWERUP;

		obj->size = Powerup_info[obj->id].size;
	}

	//if ( obj->type == OBJ_HOSTAGE )	{
	//	if ( obj->id >= N_hostage_types )	{
	//		obj->id = 0;
	//		Assert( obj->render_type == RT_POLYOBJ );
	//	}
	//}

	if ( obj->type == OBJ_WEAPON )	{
		if ( obj->id >= N_weapon_types )	{
			obj->id = 0;
			Assert( obj->render_type != RT_POLYOBJ );
		}
	}

	if ( obj->type == OBJ_CNTRLCEN )	{
		int i;

		obj->render_type = RT_POLYOBJ;
		obj->control_type = CT_CNTRLCEN;

		// Make model number is correct...	
		for (i=0; i<Num_total_object_types; i++ )	
			if ( ObjType[i] == OL_CONTROL_CENTER )		{
				obj->rtype.pobj_info.model_num = ObjId[i];
				obj->shields = ObjStrength[i];
				break;		
			}
	}

	if ( obj->type == OBJ_PLAYER )	{
		//int i;

		//Assert(obj == Player);

		if ( obj == ConsoleObject )		
			init_player_object();
		else
			if (obj->render_type == RT_POLYOBJ)	//recover from Matt's pof file matchup bug
				obj->rtype.pobj_info.model_num = Player_ship->model_num;

		//Make sure orient matrix is orthogonal
		check_and_fix_matrix(&obj->orient);

		obj->id = Gamesave_num_players++;
	}

	if (obj->type == OBJ_HOSTAGE) {

		if (obj->id > N_hostage_types)
			obj->id = 0;

		obj->render_type = RT_HOSTAGE;
		obj->control_type = CT_POWERUP;
		//obj->vclip_info.vclip_num = Hostage_vclip_num[Hostages[obj->id].type];
		//obj->vclip_info.frametime = Vclip[obj->vclip_info.vclip_num].frame_time;
		//obj->vclip_info.framenum = 0;
	}

}

static int read_int(CFILE *file)
{
	// ASCENT: little endian field read. The same change is in the other
	// read_ functions below.
	ubyte b[4];
	const ubyte *p = b;

	if (cfread( b, 4, 1, file) != 1)
		Error( "Error reading int in gamesave.c" );

	return dio_get_int(&p);
}

static fix read_fix(CFILE *file)
{
	ubyte b[4];
	const ubyte *p = b;

	if (cfread( b, 4, 1, file) != 1)
		Error( "Error reading fix in gamesave.c" );

	return dio_get_fix(&p);
}

static short read_short(CFILE *file)
{
	ubyte b[2];
	const ubyte *p = b;

	if (cfread( b, 2, 1, file) != 1)
		Error( "Error reading short in gamesave.c" );

	return dio_get_short(&p);
}

static short read_fixang(CFILE *file)
{
	ubyte b[2];
	const ubyte *p = b;

	if (cfread( b, 2, 1, file) != 1)
		Error( "Error reading fixang in gamesave.c" );

	return dio_get_fixang(&p);
}

static byte read_byte(CFILE *file)
{
	byte b;

	if (cfread( &b, sizeof(b), 1, file) != 1)
		Error( "Error reading byte in gamesave.c" );

	return b;
}

static void read_vector(vms_vector *v,CFILE *file)
{
	v->x = read_fix(file);
	v->y = read_fix(file);
	v->z = read_fix(file);
}

static void read_matrix(vms_matrix *m,CFILE *file)
{
	read_vector(&m->rvec,file);
	read_vector(&m->uvec,file);
	read_vector(&m->fvec,file);
}

static void read_angvec(vms_angvec *v,CFILE *file)
{
	v->p = read_fixang(file);
	v->b = read_fixang(file);
	v->h = read_fixang(file);
}

//static gs_skip(int len,CFILE *file)
//{
//
//	cfseek(file,len,SEEK_CUR);
//}

#ifdef EDITOR
static void gs_write_int(int i,FILE *file)
{
	if (fwrite( &i, sizeof(i), 1, file) != 1)
		Error( "Error reading int in gamesave.c" );

}

static void gs_write_fix(fix f,FILE *file)
{
	if (fwrite( &f, sizeof(f), 1, file) != 1)
		Error( "Error reading fix in gamesave.c" );

}

static void gs_write_short(short s,FILE *file)
{
	if (fwrite( &s, sizeof(s), 1, file) != 1)
		Error( "Error reading short in gamesave.c" );

}

static void gs_write_fixang(fixang f,FILE *file)
{
	if (fwrite( &f, sizeof(f), 1, file) != 1)
		Error( "Error reading fixang in gamesave.c" );

}

static void gs_write_byte(byte b,FILE *file)
{
	if (fwrite( &b, sizeof(b), 1, file) != 1)
		Error( "Error reading byte in gamesave.c" );

}

static void gr_write_vector(vms_vector *v,FILE *file)
{
	gs_write_fix(v->x,file);
	gs_write_fix(v->y,file);
	gs_write_fix(v->z,file);
}

static void gs_write_matrix(vms_matrix *m,FILE *file)
{
	gr_write_vector(&m->rvec,file);
	gr_write_vector(&m->uvec,file);
	gr_write_vector(&m->fvec,file);
}

static void gs_write_angvec(vms_angvec *v,FILE *file)
{
	gs_write_fixang(v->p,file);
	gs_write_fixang(v->b,file);
	gs_write_fixang(v->h,file);
}

#endif

//reads one object of the given version from the given file
read_object(object *obj,CFILE *f,int version)
{
	obj->type				= read_byte(f);
	obj->id					= read_byte(f);

	obj->control_type		= read_byte(f);
	obj->movement_type	= read_byte(f);
	obj->render_type		= read_byte(f);
	obj->flags				= read_byte(f);

	obj->segnum				= read_short(f);
	obj->attached_obj		= -1;

	read_vector(&obj->pos,f);
	read_matrix(&obj->orient,f);

	obj->size				= read_fix(f);
	obj->shields			= read_fix(f);

	read_vector(&obj->last_pos,f);

	obj->contains_type	= read_byte(f);
	obj->contains_id		= read_byte(f);
	obj->contains_count	= read_byte(f);

	switch (obj->movement_type) {

		case MT_PHYSICS:

			read_vector(&obj->mtype.phys_info.velocity,f);
			read_vector(&obj->mtype.phys_info.thrust,f);

			obj->mtype.phys_info.mass		= read_fix(f);
			obj->mtype.phys_info.drag		= read_fix(f);
			obj->mtype.phys_info.brakes	= read_fix(f);

			read_vector(&obj->mtype.phys_info.rotvel,f);
			read_vector(&obj->mtype.phys_info.rotthrust,f);

			obj->mtype.phys_info.turnroll	= read_fixang(f);
			obj->mtype.phys_info.flags		= read_short(f);

			break;

		case MT_SPINNING:

			read_vector(&obj->mtype.spin_rate,f);
			break;

		case MT_NONE:
			break;

		default:
			Int3();
	}

	switch (obj->control_type) {

		case CT_AI: {
			int i;

			obj->ctype.ai_info.behavior				= read_byte(f);

			for (i=0;i<MAX_AI_FLAGS;i++)
				obj->ctype.ai_info.flags[i]			= read_byte(f);

			obj->ctype.ai_info.hide_segment			= read_short(f);
			obj->ctype.ai_info.hide_index				= read_short(f);
			obj->ctype.ai_info.path_length			= read_short(f);
			obj->ctype.ai_info.cur_path_index		= read_short(f);

			obj->ctype.ai_info.follow_path_start_seg	= read_short(f);
			obj->ctype.ai_info.follow_path_end_seg		= read_short(f);

			break;
		}

		case CT_EXPLOSION:

			obj->ctype.expl_info.spawn_time		= read_fix(f);
			obj->ctype.expl_info.delete_time		= read_fix(f);
			obj->ctype.expl_info.delete_objnum	= read_short(f);
			obj->ctype.expl_info.next_attach = obj->ctype.expl_info.prev_attach = obj->ctype.expl_info.attach_parent = -1;

			break;

		case CT_WEAPON:

			//do I really need to read these?  Are they even saved to disk?

			obj->ctype.laser_info.parent_type		= read_short(f);
			obj->ctype.laser_info.parent_num			= read_short(f);
			obj->ctype.laser_info.parent_signature	= read_int(f);

			break;

		case CT_LIGHT:

			obj->ctype.light_info.intensity = read_fix(f);
			break;

		case CT_POWERUP:

			if (version >= 25)
				obj->ctype.powerup_info.count = read_int(f);
			else
				obj->ctype.powerup_info.count = 1;

			if (obj->id == POW_VULCAN_WEAPON)
					obj->ctype.powerup_info.count = VULCAN_WEAPON_AMMO_AMOUNT;

			break;


		case CT_NONE:
		case CT_FLYING:
		case CT_DEBRIS:
			break;

		case CT_SLEW:		//the player is generally saved as slew
			break;

		case CT_CNTRLCEN:
			break;

		case CT_MORPH:
		case CT_FLYTHROUGH:
		case CT_REPAIRCEN:
		default:
			Int3();
	
	}

	switch (obj->render_type) {

		case RT_NONE:
			break;

		case RT_MORPH:
		case RT_POLYOBJ: {
			int i,tmo;

			obj->rtype.pobj_info.model_num		= read_int(f);

			for (i=0;i<MAX_SUBMODELS;i++)
				read_angvec(&obj->rtype.pobj_info.anim_angles[i],f);

			obj->rtype.pobj_info.subobj_flags	= read_int(f);

			tmo = read_int(f);

			// ASCENT: the translation of the texture number was under EDITOR.
			// The editor mine format needs it, and that format is always built
			// now. For a compiled mine the table maps each number to itself.
			// A number outside the table is kept as it is, as in the game
			// build of the original.
			if (tmo<0 || tmo>=MAX_TEXTURES)		//includes -1, no override
				obj->rtype.pobj_info.tmap_override	= tmo;
			else {
				int xlated_tmo = tmap_xlate_table[tmo];
				if (xlated_tmo < 0)	{
					mprintf( (0, "Couldn't find texture for demo object, model_num = %d\n", obj->rtype.pobj_info.model_num));
					Int3();
					xlated_tmo = 0;
				}
				obj->rtype.pobj_info.tmap_override	= xlated_tmo;
			}

			obj->rtype.pobj_info.alt_textures	= 0;

			break;
		}

		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:

			obj->rtype.vclip_info.vclip_num	= read_int(f);
			obj->rtype.vclip_info.frametime	= read_fix(f);
			obj->rtype.vclip_info.framenum	= read_byte(f);

			break;

		case RT_LASER:
			break;

		default:
			Int3();

	}

}

#ifdef EDITOR

//writes one object to the given file
write_object(object *obj,FILE *f)
{
	gs_write_byte(obj->type,f);
	gs_write_byte(obj->id,f);

	gs_write_byte(obj->control_type,f);
	gs_write_byte(obj->movement_type,f);
	gs_write_byte(obj->render_type,f);
	gs_write_byte(obj->flags,f);

	gs_write_short(obj->segnum,f);

	gr_write_vector(&obj->pos,f);
	gs_write_matrix(&obj->orient,f);

	gs_write_fix(obj->size,f);
	gs_write_fix(obj->shields,f);

	gr_write_vector(&obj->last_pos,f);

	gs_write_byte(obj->contains_type,f);
	gs_write_byte(obj->contains_id,f);
	gs_write_byte(obj->contains_count,f);

	switch (obj->movement_type) {

		case MT_PHYSICS:

	 		gr_write_vector(&obj->mtype.phys_info.velocity,f);
			gr_write_vector(&obj->mtype.phys_info.thrust,f);

			gs_write_fix(obj->mtype.phys_info.mass,f);
			gs_write_fix(obj->mtype.phys_info.drag,f);
			gs_write_fix(obj->mtype.phys_info.brakes,f);

			gr_write_vector(&obj->mtype.phys_info.rotvel,f);
			gr_write_vector(&obj->mtype.phys_info.rotthrust,f);

			gs_write_fixang(obj->mtype.phys_info.turnroll,f);
			gs_write_short(obj->mtype.phys_info.flags,f);

			break;

		case MT_SPINNING:

			gr_write_vector(&obj->mtype.spin_rate,f);
			break;

		case MT_NONE:
			break;

		default:
			Int3();
	}

	switch (obj->control_type) {

		case CT_AI: {
			int i;

			gs_write_byte(obj->ctype.ai_info.behavior,f);

			for (i=0;i<MAX_AI_FLAGS;i++)
				gs_write_byte(obj->ctype.ai_info.flags[i],f);

			gs_write_short(obj->ctype.ai_info.hide_segment,f);
			gs_write_short(obj->ctype.ai_info.hide_index,f);
			gs_write_short(obj->ctype.ai_info.path_length,f);
			gs_write_short(obj->ctype.ai_info.cur_path_index,f);

			gs_write_short(obj->ctype.ai_info.follow_path_start_seg,f);
			gs_write_short(obj->ctype.ai_info.follow_path_end_seg,f);

			break;
		}

		case CT_EXPLOSION:

			gs_write_fix(obj->ctype.expl_info.spawn_time,f);
			gs_write_fix(obj->ctype.expl_info.delete_time,f);
			gs_write_short(obj->ctype.expl_info.delete_objnum,f);

			break;

		case CT_WEAPON:

			//do I really need to write these objects?

			gs_write_short(obj->ctype.laser_info.parent_type,f);
			gs_write_short(obj->ctype.laser_info.parent_num,f);
			gs_write_int(obj->ctype.laser_info.parent_signature,f);

			break;

			break;

		case CT_LIGHT:

			gs_write_fix(obj->ctype.light_info.intensity,f);
			break;

		case CT_POWERUP:

			gs_write_int(obj->ctype.powerup_info.count,f);
			break;

		case CT_NONE:
		case CT_FLYING:
		case CT_DEBRIS:
			break;

		case CT_SLEW:		//the player is generally saved as slew
			break;

		case CT_CNTRLCEN:
			break;			//control center object.

		case CT_MORPH:
		case CT_REPAIRCEN:
		case CT_FLYTHROUGH:
		default:
			Int3();
	
	}

	switch (obj->render_type) {

		case RT_NONE:
			break;

		case RT_MORPH:
		case RT_POLYOBJ: {
			int i;

			gs_write_int(obj->rtype.pobj_info.model_num,f);

			for (i=0;i<MAX_SUBMODELS;i++)
				gs_write_angvec(&obj->rtype.pobj_info.anim_angles[i],f);

			gs_write_int(obj->rtype.pobj_info.subobj_flags,f);

			gs_write_int(obj->rtype.pobj_info.tmap_override,f);

			break;
		}

		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_FIREBALL:

			gs_write_int(obj->rtype.vclip_info.vclip_num,f);
			gs_write_fix(obj->rtype.vclip_info.frametime,f);
			gs_write_byte(obj->rtype.vclip_info.framenum,f);

			break;

		case RT_LASER:
			break;

		default:
			Int3();

	}

}
#endif

// -----------------------------------------------------------------------------
// Load game 
// Loads all the relevant data for a level.
// If level != -1, it loads the filename with extension changed to .min
// Otherwise it loads the appropriate level mine.
// returns 0=everything ok, 1=old version, -1=error
load_game_data(CFILE *LoadFile)
{
	int i,j;
	int start_offset;

	start_offset = cftell(LoadFile);

	//===================== READ FILE INFO ========================

	// Set default values
	game_fileinfo.level					=	-1;
	game_fileinfo.player_offset		=	-1;
	// ASCENT: the sizes on disk, not sizeof() of the structures in memory
	game_fileinfo.player_sizeof		=	PLAYER_DISK_SIZE;
 	game_fileinfo.object_offset		=	-1;
	game_fileinfo.object_howmany		=	0;
	game_fileinfo.object_sizeof		=	OBJECT_DISK_SIZE;  
	game_fileinfo.walls_offset			=	-1;
	game_fileinfo.walls_howmany		=	0;
	game_fileinfo.walls_sizeof			=	WALL_DISK_SIZE;  
	game_fileinfo.doors_offset			=	-1;
	game_fileinfo.doors_howmany		=	0;
	game_fileinfo.doors_sizeof			=	ACTIVE_DOOR_DISK_SIZE;  
	game_fileinfo.triggers_offset		=	-1;
	game_fileinfo.triggers_howmany	=	0;
	game_fileinfo.triggers_sizeof		=	TRIGGER_DISK_SIZE;  
	game_fileinfo.control_offset		=	-1;
	game_fileinfo.control_howmany		=	0;
	game_fileinfo.control_sizeof		=	CONTROL_CENTER_TRIGGERS_DISK_SIZE;
	game_fileinfo.matcen_offset		=	-1;
	game_fileinfo.matcen_howmany		=	0;
	game_fileinfo.matcen_sizeof		=	MATCEN_INFO_DISK_SIZE;

	// Read in game_top_fileinfo to get size of saved fileinfo.

	if (cfseek( LoadFile, start_offset, SEEK_SET )) 
		Error( "Error seeking in gamesave.c" ); 

	// ASCENT: field reads of the 8 bytes of game_top_fileinfo
	{
		ubyte buf[GAME_TOP_FILEINFO_DISK_SIZE];
		const ubyte *p = buf;

		if (cfread( buf, GAME_TOP_FILEINFO_DISK_SIZE, 1, LoadFile) != 1)
			Error( "Error reading game_top_fileinfo in gamesave.c" );
		game_top_fileinfo.fileinfo_signature = dio_get_ushort(&p);
		game_top_fileinfo.fileinfo_version = dio_get_ushort(&p);
		game_top_fileinfo.fileinfo_sizeof = dio_get_int(&p);
	}

	// Check signature
	if (game_top_fileinfo.fileinfo_signature != 0x6705)
		return -1;

	// Check version number
	if (game_top_fileinfo.fileinfo_version < GAME_COMPATIBLE_VERSION )
		return -1;

	// Now, Read in the fileinfo
	if (cfseek( LoadFile, start_offset, SEEK_SET )) 
		Error( "Error seeking to game_fileinfo in gamesave.c" );

	game_fileinfo.fileinfo_signature = read_short(LoadFile);

	game_fileinfo.fileinfo_version = read_short(LoadFile);
	game_fileinfo.fileinfo_sizeof = read_int(LoadFile);
	for(i=0; i<15; i++)
		game_fileinfo.mine_filename[i] = read_byte(LoadFile);
	game_fileinfo.level = read_int(LoadFile);
	game_fileinfo.player_offset = read_int(LoadFile);				// Player info
	game_fileinfo.player_sizeof = read_int(LoadFile);
	game_fileinfo.object_offset = read_int(LoadFile);				// Object info
	game_fileinfo.object_howmany = read_int(LoadFile);    	
	game_fileinfo.object_sizeof = read_int(LoadFile);  
	game_fileinfo.walls_offset = read_int(LoadFile);
	game_fileinfo.walls_howmany = read_int(LoadFile);
	game_fileinfo.walls_sizeof = read_int(LoadFile);
	game_fileinfo.doors_offset = read_int(LoadFile);
	game_fileinfo.doors_howmany = read_int(LoadFile);
	game_fileinfo.doors_sizeof = read_int(LoadFile);
	game_fileinfo.triggers_offset = read_int(LoadFile);
	game_fileinfo.triggers_howmany = read_int(LoadFile);
	game_fileinfo.triggers_sizeof = read_int(LoadFile);
	game_fileinfo.links_offset = read_int(LoadFile);
	game_fileinfo.links_howmany = read_int(LoadFile);
	game_fileinfo.links_sizeof = read_int(LoadFile);
	game_fileinfo.control_offset = read_int(LoadFile);
	game_fileinfo.control_howmany = read_int(LoadFile);
	game_fileinfo.control_sizeof = read_int(LoadFile);
	game_fileinfo.matcen_offset = read_int(LoadFile);
	game_fileinfo.matcen_howmany = read_int(LoadFile);
	game_fileinfo.matcen_sizeof = read_int(LoadFile);

//	if (cfread( &game_fileinfo, game_top_fileinfo.fileinfo_sizeof, 1, LoadFile )!=1)
//		Error( "Error reading game_fileinfo in gamesave.c" );

	if (game_top_fileinfo.fileinfo_version >= 14) {	//load mine filename
		char *p=Current_level_name;
		//must do read one char at a time, since no cfgets()
		do *p = cfgetc(LoadFile); while (*p++!=0);
	}
	else
		Current_level_name[0]=0;

	if (game_top_fileinfo.fileinfo_version >= 19) {	//load pof names
		// ASCENT: the original read 2 bytes into the int. The count is an
		// unsigned short. The original did not check the count against the
		// size of Save_pof_names.
		N_save_pof_names = (ushort)read_short(LoadFile);
		if (N_save_pof_names > MAX_POLYGON_MODELS)
			Error( "Too many POF names (%d, maximum %d) in the level file", N_save_pof_names, MAX_POLYGON_MODELS );
		cfread(Save_pof_names,N_save_pof_names,13,LoadFile);
	}

	//===================== READ PLAYER INFO ==========================
	Object_next_signature = 0;

	//===================== READ OBJECT INFO ==========================

	Gamesave_num_org_robots = 0;
	Gamesave_num_players = 0;

	if (game_fileinfo.object_offset > -1) {
		if (cfseek( LoadFile, game_fileinfo.object_offset, SEEK_SET )) 
			Error( "Error seeking to object_offset in gamesave.c" );
	
		for (i=0;i<game_fileinfo.object_howmany;i++)	{

			read_object(&Objects[i],LoadFile,game_top_fileinfo.fileinfo_version);

			Objects[i].signature = Object_next_signature++;
			verify_object( &Objects[i] );
		}

	}

	//===================== READ WALL INFO ============================

	// ASCENT: the wall and door records are read into a byte buffer and
	// converted one field at a time (see dataio.h). The size checks compare
	// the size in the file header with the size of the record on disk, not
	// with sizeof() of the structure in memory.

	if (game_fileinfo.walls_offset > -1)
	{

		if (!cfseek( LoadFile, game_fileinfo.walls_offset,SEEK_SET ))	{
			for (i=0;i<game_fileinfo.walls_howmany;i++) {

				if (game_top_fileinfo.fileinfo_version >= 20) {
					ubyte buf[WALL_DISK_SIZE];

					Assert(WALL_DISK_SIZE == game_fileinfo.walls_sizeof);

					if (cfread(buf, WALL_DISK_SIZE, 1,LoadFile)!=1)
						Error( "Error reading Walls[%d] in gamesave.c", i);

					wall_from_disk(&Walls[i], buf);
				}
				else if (game_top_fileinfo.fileinfo_version >= 17) {
					v19_wall w;
					ubyte buf[V19_WALL_DISK_SIZE];
					const ubyte *p = buf;

					Assert(V19_WALL_DISK_SIZE == game_fileinfo.walls_sizeof);

					if (cfread(buf, V19_WALL_DISK_SIZE, 1,LoadFile)!=1)
						Error( "Error reading Walls[%d] in gamesave.c", i);

					w.segnum			= dio_get_int(&p);
					w.sidenum		= dio_get_int(&p);
					w.type			= dio_get_byte(&p);
					w.flags			= dio_get_byte(&p);
					w.hps				= dio_get_fix(&p);
					w.trigger		= dio_get_byte(&p);
					w.clip_num		= dio_get_byte(&p);
					w.keys			= dio_get_byte(&p);
					w.linked_wall	= dio_get_int(&p);

					Walls[i].segnum		= w.segnum;
					Walls[i].sidenum		= w.sidenum;
					Walls[i].linked_wall	= w.linked_wall;

					Walls[i].type			= w.type;
					Walls[i].flags			= w.flags;
					Walls[i].hps			= w.hps;
					Walls[i].trigger		= w.trigger;
					Walls[i].clip_num		= w.clip_num;
					Walls[i].keys			= w.keys;

					Walls[i].state			= WALL_DOOR_CLOSED;
				}
				else {
					v16_wall w;
					ubyte buf[V16_WALL_DISK_SIZE];
					const ubyte *p = buf;

					Assert(V16_WALL_DISK_SIZE == game_fileinfo.walls_sizeof);

					if (cfread(buf, V16_WALL_DISK_SIZE, 1,LoadFile)!=1)
						Error( "Error reading Walls[%d] in gamesave.c", i);

					w.type			= dio_get_byte(&p);
					w.flags			= dio_get_byte(&p);
					w.hps				= dio_get_fix(&p);
					w.trigger		= dio_get_byte(&p);
					w.clip_num		= dio_get_byte(&p);
					w.keys			= dio_get_byte(&p);

					Walls[i].segnum = Walls[i].sidenum = Walls[i].linked_wall = -1;

					Walls[i].type		= w.type;
					Walls[i].flags		= w.flags;
					Walls[i].hps		= w.hps;
					Walls[i].trigger	= w.trigger;
					Walls[i].clip_num	= w.clip_num;
					Walls[i].keys		= w.keys;
				}

			}
		}
	}

	//===================== READ DOOR INFO ============================

	if (game_fileinfo.doors_offset > -1)
	{
		if (!cfseek( LoadFile, game_fileinfo.doors_offset,SEEK_SET ))	{

			for (i=0;i<game_fileinfo.doors_howmany;i++) {

				if (game_top_fileinfo.fileinfo_version >= 20) {
					ubyte buf[ACTIVE_DOOR_DISK_SIZE];

					Assert(ACTIVE_DOOR_DISK_SIZE == game_fileinfo.doors_sizeof);

					if (cfread(buf, ACTIVE_DOOR_DISK_SIZE,1,LoadFile)!=1)
						Error( "Error reading ActiveDoors[%d] in gamesave.c", i);

					active_door_from_disk(&ActiveDoors[i], buf);
				}
				else {
					v19_door d;
					ubyte buf[V19_DOOR_DISK_SIZE];
					const ubyte *bp = buf;
					int p;

					Assert(V19_DOOR_DISK_SIZE == game_fileinfo.doors_sizeof);

					if (cfread(buf, V19_DOOR_DISK_SIZE, 1,LoadFile)!=1)
						Error( "Error reading Doors[%d] in gamesave.c", i);

					d.n_parts	= dio_get_int(&bp);
					d.seg[0]		= dio_get_short(&bp);
					d.seg[1]		= dio_get_short(&bp);
					d.side[0]	= dio_get_short(&bp);
					d.side[1]	= dio_get_short(&bp);
					d.type[0]	= dio_get_short(&bp);
					d.type[1]	= dio_get_short(&bp);
					d.open		= dio_get_fix(&bp);

					ActiveDoors[i].n_parts = d.n_parts;

					for (p=0;p<d.n_parts;p++) {
						int cseg,cside;

						cseg = Segments[d.seg[p]].children[d.side[p]];
						cside = find_connect_side(&Segments[d.seg[p]],&Segments[cseg]);

						ActiveDoors[i].front_wallnum[p] = Segments[d.seg[p]].sides[d.side[p]].wall_num;
						ActiveDoors[i].back_wallnum[p] = Segments[cseg].sides[cside].wall_num;
					}
				}

			}
		}
	}

	//==================== READ TRIGGER INFO ==========================

	if (game_fileinfo.triggers_offset > -1)		{
		if (!cfseek( LoadFile, game_fileinfo.triggers_offset,SEEK_SET ))	{
			for (i=0;i<game_fileinfo.triggers_howmany;i++)	{
				//Assert( sizeof(Triggers[i]) == game_fileinfo.triggers_sizeof );
				//if (cfread(&Triggers[i], game_fileinfo.triggers_sizeof,1,LoadFile)!=1)
				//	Error( "Error reading Triggers[%d] in gamesave.c", i);
				Triggers[i].type = read_byte(LoadFile);
				Triggers[i].flags = read_short(LoadFile);
				Triggers[i].value = read_int(LoadFile);
				Triggers[i].time = read_int(LoadFile);
				Triggers[i].link_num = read_byte(LoadFile);
				Triggers[i].num_links = read_short(LoadFile);
				for (j=0; j<MAX_WALLS_PER_LINK; j++ )	
					Triggers[i].seg[j] = read_short(LoadFile);
				for (j=0; j<MAX_WALLS_PER_LINK; j++ )
					Triggers[i].side[j] = read_short(LoadFile);
			}
		}
	}

	//================ READ CONTROL CENTER TRIGGER INFO ===============

	if (game_fileinfo.control_offset > -1)
	{
		if (!cfseek( LoadFile, game_fileinfo.control_offset,SEEK_SET ))	{
			for (i=0;i<game_fileinfo.control_howmany;i++)
				// ASCENT: was a compare with sizeof() and a cfread() of the structure
				if ( CONTROL_CENTER_TRIGGERS_DISK_SIZE == game_fileinfo.control_sizeof )	{
					ubyte buf[CONTROL_CENTER_TRIGGERS_DISK_SIZE];

					if (cfread(buf, CONTROL_CENTER_TRIGGERS_DISK_SIZE,1,LoadFile)!=1)
					 	Error( "Error reading ControlCenterTriggers in gamesave.c", i);

					control_center_triggers_from_disk(&ControlCenterTriggers, buf);
				} else {
					// ASCENT: the original code for a record of another size has
					// a ";" after each "for", so it read three shorts in all and
					// stored the second one in seg[MAX_WALLS_PER_LINK] and the
					// third one in side[MAX_WALLS_PER_LINK], both past the end of
					// the arrays. With 1 byte packing seg[MAX_WALLS_PER_LINK] is
					// side[0], and side[MAX_WALLS_PER_LINK] is the memory after
					// the structure. The three shorts are still read. The second
					// one goes to side[0]. The third one is dropped.
					short past_seg, past_side;

					ControlCenterTriggers.num_links = read_short( LoadFile );
					past_seg = read_short( LoadFile );
					past_side = read_short( LoadFile );
					ControlCenterTriggers.side[0] = past_seg;
					(void)past_side;
				}
		}
	}


	//================ READ MATERIALOGRIFIZATIONATORS INFO ===============

	if (game_fileinfo.matcen_offset > -1)
	{	int	j;

		if (!cfseek( LoadFile, game_fileinfo.matcen_offset,SEEK_SET ))	{
			// mprintf((0, "Reading %i materialization centers.\n", game_fileinfo.matcen_howmany));
			for (i=0;i<game_fileinfo.matcen_howmany;i++) {
				// ASCENT: field read of the record
				ubyte buf[MATCEN_INFO_DISK_SIZE];

				Assert( MATCEN_INFO_DISK_SIZE == game_fileinfo.matcen_sizeof );
				if (cfread(buf, MATCEN_INFO_DISK_SIZE,1,LoadFile)!=1)
					Error( "Error reading RobotCenters in gamesave.c", i);
				matcen_info_from_disk(&RobotCenters[i], buf);
				//	Set links in RobotCenters to Station array
				for (j=0; j<=Highest_segment_index; j++)
					if (Segments[j].special == SEGMENT_IS_ROBOTMAKER)
						if (Segments[j].matcen_num == i)
							RobotCenters[i].fuelcen_num = Segments[j].value;

				// mprintf((0, "   %i: flags = %08x\n", i, RobotCenters[i].robot_flags));
			}
		}
	}


	//========================= UPDATE VARIABLES ======================

	reset_objects(game_fileinfo.object_howmany);

	for (i=0; i<MAX_OBJECTS; i++) {
		Objects[i].next = Objects[i].prev = -1;
		if (Objects[i].type != OBJ_NONE) {
			int objsegnum = Objects[i].segnum;

			if (objsegnum > Highest_segment_index)		//bogus object
				Objects[i].type = OBJ_NONE;
			else {
				Objects[i].segnum = -1;			//avoid Assert()
				obj_link(i,objsegnum);
			}
		}
	}

	clear_transient_objects(1);		//1 means clear proximity bombs

	// Make sure non-transparent doors are set correctly.
	for (i=0; i< Num_segments; i++)
		for (j=0;j<MAX_SIDES_PER_SEGMENT;j++) {
			side	*sidep = &Segments[i].sides[j];
			if ((sidep->wall_num != -1) && (Walls[sidep->wall_num].clip_num != -1)) {
				//mprintf((0, "Checking Wall %d\n", Segments[i].sides[j].wall_num));
				if (WallAnims[Walls[sidep->wall_num].clip_num].flags & WCF_TMAP1) {
					//mprintf((0, "Fixing non-transparent door.\n"));
					sidep->tmap_num = WallAnims[Walls[sidep->wall_num].clip_num].frames[0];
					sidep->tmap_num2 = 0;
				}
			}
		}


	Num_walls = game_fileinfo.walls_howmany;
	reset_walls();

	Num_open_doors = game_fileinfo.doors_howmany;
	Num_triggers = game_fileinfo.triggers_howmany;

	Num_robot_centers = game_fileinfo.matcen_howmany;

	//fix old wall structs
	if (game_top_fileinfo.fileinfo_version < 17) {
		int segnum,sidenum,wallnum;

		for (segnum=0; segnum<=Highest_segment_index; segnum++)
			for (sidenum=0;sidenum<6;sidenum++)
				if ((wallnum=Segments[segnum].sides[sidenum].wall_num) != -1) {
					Walls[wallnum].segnum = segnum;
					Walls[wallnum].sidenum = sidenum;
				}
	}

	#ifndef NDEBUG
	{
		int	sidenum;
		for (sidenum=0; sidenum<6; sidenum++) {
			int	wallnum = Segments[Highest_segment_index].sides[sidenum].wall_num;
			if (wallnum != -1)
				if ((Walls[wallnum].segnum != Highest_segment_index) || (Walls[wallnum].sidenum != sidenum))
					Int3();	//	Error.  Bogus walls in this segment.
								// Consult Yuan or Mike.
		}
	}
	#endif

	//create_local_segment_data();

	fix_object_segs();

	#ifndef NDEBUG
	dump_mine_info();
	#endif

	if (game_top_fileinfo.fileinfo_version < GAME_VERSION)
		return 1;		//means old version
	else
		return 0;
}


int check_segment_connections(void);

// -----------------------------------------------------------------------------
//loads from an already-open file
// returns 0=everything ok, 1=old version, -1=error
int load_mine_data(CFILE *LoadFile);
int load_mine_data_compiled(CFILE *LoadFile);

#define LEVEL_FILE_VERSION		1

#ifndef RELEASE
char *Level_being_loaded=NULL;
#endif

#ifdef COMPACT_SEGS
extern void ncache_flush();
#endif

// ASCENT: the load_level() of Descent 1 is not built. The new load_level()
// for the level files of the target game is below it. load_mine_data(),
// load_mine_data_compiled() and load_game_data() of Descent 1 are still
// built, and nothing calls them.
#if 0
//loads a level (.LVL) file from disk
int load_level(char * filename_passed)
{
	// ASCENT: use_compiled_level was under EDITOR. Both mine loaders are
	// always built now. See Gamesave_level_format.
	int use_compiled_level=1;
	CFILE * LoadFile;
	char filename[128];
	int sig,version,minedata_offset,gamedata_offset,hostagetext_offset;
	int mine_err,game_err;

	#ifdef COMPACT_SEGS
	ncache_flush();
	#endif

	#ifndef RELEASE
	Level_being_loaded = filename_passed;
	#endif

	strcpy(filename,filename_passed);

	#ifdef EDITOR
	//check extension to see what file type this is
	strupr(filename);

	if (strstr(filename,".LVL"))
		use_compiled_level = 0;
		#ifdef SHAREWARE
	else if (!strstr(filename,".SDL")) {
		convert_name_to_CDL(filename,filename_passed);
		use_compiled_level = 1;
	}
		#else
	else if (!strstr(filename,".RDL")) {
		convert_name_to_CDL(filename,filename_passed);
		use_compiled_level = 1;
	}
		#endif

	// If we're trying to load a CDL, and we can't find it, and we have
	// the editor compiled in, then load the LVL.
	if ( (!cfexist(filename)) && use_compiled_level )	{
		convert_name_to_LVL(filename,filename_passed);
		use_compiled_level = 0;
	}
	#endif

	// ASCENT: the caller selects the mine format with Gamesave_level_format.
	// The automatic choice of the game build looks at the extension, as the
	// editor build did, but it does not change the file name.
	if (Gamesave_level_format == LEVEL_FORMAT_COMPILED)
		use_compiled_level = 1;
	else if (Gamesave_level_format == LEVEL_FORMAT_EDITOR)
		use_compiled_level = 0;
	#ifndef EDITOR
	else {
		char *ext = strrchr(filename,'.');
		use_compiled_level = !(ext && !stricmp(ext,".LVL"));
	}
	#endif

	LoadFile = cfopen( filename, "rb" );
//CF_READ_MODE );

#ifdef EDITOR
	if (!LoadFile)	{
		mprintf((0,"Can't open file <%s>\n", filename));
		return 1;
	}
#else
	if (!LoadFile)
		Error("Can't open file <%s>\n",filename);
#endif

	strcpy( Gamesave_current_filename, filename );

//	#ifdef NEWDEMO
//	if ( Newdemo_state == ND_STATE_RECORDING )
//		newdemo_record_start_demo();
//	#endif

	sig						= read_int(LoadFile);
	version					= read_int(LoadFile);
	minedata_offset		= read_int(LoadFile);
	gamedata_offset		= read_int(LoadFile);
	hostagetext_offset	= read_int(LoadFile);

	Assert(sig == 'PLVL');

	cfseek(LoadFile,minedata_offset,SEEK_SET);
	// ASCENT: was under EDITOR
	if (!use_compiled_level)
		mine_err = load_mine_data(LoadFile);
	else
		//NOTE LINK TO ABOVE!!
		mine_err = load_mine_data_compiled(LoadFile);

	if (mine_err == -1)	//error!!
		return 1;

	cfseek(LoadFile,gamedata_offset,SEEK_SET);
	game_err = load_game_data(LoadFile);

	if (game_err == -1)	//error!!
		return 1;

	#ifdef HOSTAGE_FACES
	cfseek(LoadFile,hostagetext_offset,SEEK_SET);
	load_hostage_data(LoadFile,(version>=1));
	#endif

	//======================== CLOSE FILE =============================

	cfclose( LoadFile );

	#ifdef EDITOR
	write_game_text_file(filename);
	if (Errors_in_mine) {
		if (is_real_level(filename)) {
			char  ErrorMessage[200];

			sprintf( ErrorMessage, "Warning: %i errors in %s!\n", Errors_in_mine, Level_being_loaded );
			stop_time();
			gr_palette_load(gr_palette);
			nm_messagebox( NULL, 1, "Continue", ErrorMessage );
			start_time();
		} else
			mprintf((1, "Error: %i errors in %s.\n", Errors_in_mine, Level_being_loaded));
	}
	#endif

	#ifdef EDITOR
	//If an old version, ask the use if he wants to save as new version
	if (((LEVEL_FILE_VERSION>1) && version<LEVEL_FILE_VERSION) || mine_err==1 || game_err==1) {
		char  ErrorMessage[200];

		sprintf( ErrorMessage, "You just loaded a old version level.  Would\n"
						"you like to save it as a current version level?");

		stop_time();
		gr_palette_load(gr_palette);
		if (nm_messagebox( NULL, 2, "Don't Save", "Save", ErrorMessage )==1)
			save_level(filename);
		start_time();
	}
	#endif

	#ifdef EDITOR
	if (Function_mode == FMODE_EDITOR)
		editor_status("Loaded NEW mine %s, \"%s\"",filename,Current_level_name);
	#endif

	#ifdef EDITOR
	if (check_segment_connections())
		nm_messagebox( "ERROR", 1, "Ok", 
				"Connectivity errors detected in\n"
				"mine.  See monochrome screen for\n"
				"details, and contact Matt or Mike." );
	#endif

	return 0;
}
#endif	// ASCENT: the load_level() of Descent 1

// -----------------------------------------------------------------------------
// ASCENT: new. The level files of Descent to Undermountain (LVL and LV2,
// see docs/formats/lvl.md): the LVLP container, the mine data of version 18
// (load_mine_data_dtu in gamemine.c) and the game data of version 40 or 41
// below. The functions read from the file in memory, which dtu_load_file()
// supplies.

#define GAME_VERSION_DTU				41		// all levels of the game and of the 1.3 patch
#define GAME_VERSION_DTU_OLDEST		40		// three unused level files of the disc. They have no m_1b0 in the object.

char Level_pof_names[MAX_LEVEL_POF_NAMES][13];
int Num_level_pof_names = 0;

int Level_file_size = 0;
int Level_mine_version = 0;
int Level_game_version = 0;
int Level_objects_moved = 0;
int Level_objects_centered = 0;

// The record of a table at "offset". Stops the program if the table is not
// inside the data.
static const ubyte *game_dtu_range(const ubyte *data, int size, int offset, int count, int record_size, char *what)
{
	if (offset < 0 || count < 0 || offset > size || (int64_t)offset + (int64_t)count * record_size > size)
		Error( "The level file is damaged: %s (offset %d, %d records of %d bytes, file size %d)", what, offset, count, record_size, size );
	return data + offset;
}

// The largest object record: the common part, physics, AI and a polygon model.
#define OBJECT_DTU_MAX_DISK_SIZE	(205+64+24+24)

// Reads one object record of the given version. The read order is the one
// of read_object in the executable of the target game.
// Returns 0, or -1 for a record that the target game does not load.
static int read_object_dtu(object *obj, const ubyte **pp, int version)
{
	const ubyte *start = *pp;
	int i;

	memset(obj, 0, sizeof(*obj));

	obj->type				= dio_get_ubyte(pp);
	obj->id					= dio_get_ubyte(pp);

	obj->control_type		= dio_get_ubyte(pp);
	obj->movement_type	= dio_get_ubyte(pp);
	obj->render_type		= dio_get_ubyte(pp);
	obj->flags				= dio_get_ubyte(pp);

	obj->segnum				= dio_get_short(pp);
	// The record has -1 for the player and 0 for all other objects here.
	// In the code of Descent 1, 0 is "object 0 is attached", so the value
	// of the file is kept in dtu.m_10 and not used.
	obj->dtu.m_10			= dio_get_short(pp);
	obj->attached_obj		= -1;

	dio_get_vector(pp, &obj->pos);
	dio_get_matrix(pp, &obj->orient);
	obj->dtu.pos_level = obj->pos;
	obj->dtu.orient_copy = obj->orient;

	obj->size				= dio_get_fix(pp);
	obj->shields			= dio_get_fix(pp);

	dio_get_vector(pp, &obj->last_pos);

	obj->contains_type	= dio_get_byte(pp);
	obj->contains_id		= dio_get_byte(pp);
	obj->contains_count	= dio_get_byte(pp);

	// The block of 124 bytes that Descent 1 does not have (120 bytes in
	// version 40).
	obj->dtu.m_80 = dio_get_short(pp);
	obj->dtu.m_82 = dio_get_short(pp);
	obj->dtu.m_88_level = dio_get_int(pp);
	for (i=0;i<13;i++)
		obj->dtu.m_8c[i] = dio_get_fix(pp);
	for (i=0;i<2;i++)
		obj->dtu.m_c0[i] = dio_get_fix(pp);
	for (i=0;i<6;i++)
		obj->dtu.m_c8[i] = dio_get_fix(pp);
	for (i=0;i<2;i++)
		obj->dtu.m_e0[i] = dio_get_fix(pp);
	for (i=0;i<5;i++)
		obj->dtu.m_e8[i] = dio_get_fix(pp);
	if (version > 40)
		obj->dtu.m_1b0 = dio_get_fix(pp);

	// What the loader of the target game does with the block after the read.
	obj->dtu.m_88 = obj->dtu.m_c8[5];
	if (obj->type == OBJ_WEAPON || obj->type == OBJ_ARMOR || obj->type == OBJ_MISC || obj->type == OBJ_SPELL) {
		obj->dtu.m_1d5 = (ubyte)(obj->dtu.m_8c[8] & 0xff);
		obj->dtu.m_8c[8] = 0;
	}

	switch (obj->movement_type) {

		case MT_PHYSICS:

			dio_get_vector(pp, &obj->mtype.phys_info.velocity);
			dio_get_vector(pp, &obj->mtype.phys_info.thrust);

			obj->mtype.phys_info.mass		= dio_get_fix(pp);
			obj->mtype.phys_info.drag		= dio_get_fix(pp);
			obj->mtype.phys_info.brakes	= dio_get_fix(pp);

			dio_get_vector(pp, &obj->mtype.phys_info.rotvel);
			dio_get_vector(pp, &obj->mtype.phys_info.rotthrust);

			obj->mtype.phys_info.turnroll	= dio_get_fixang(pp);
			obj->mtype.phys_info.flags		= dio_get_ushort(pp);

			break;

		case MT_SPINNING:

			dio_get_vector(pp, &obj->mtype.spin_rate);
			break;

		case MT_NONE:
			break;

		default:
			printf( "Object with the unknown movement type %d\n", obj->movement_type );
			return -1;
	}

	switch (obj->control_type) {

		case CT_AI: {

			obj->ctype.ai_info.behavior				= dio_get_ubyte(pp);

			for (i=0;i<MAX_AI_FLAGS;i++)
				obj->ctype.ai_info.flags[i]			= dio_get_byte(pp);

			obj->ctype.ai_info.hide_segment			= dio_get_short(pp);
			obj->ctype.ai_info.hide_index				= dio_get_short(pp);
			obj->ctype.ai_info.path_length			= dio_get_short(pp);
			obj->ctype.ai_info.cur_path_index		= dio_get_short(pp);

			obj->ctype.ai_info.follow_path_start_seg	= dio_get_short(pp);
			obj->ctype.ai_info.follow_path_end_seg		= dio_get_short(pp);

			break;
		}

		case CT_EXPLOSION:

			obj->ctype.expl_info.spawn_time		= dio_get_fix(pp);
			obj->ctype.expl_info.delete_time		= dio_get_fix(pp);
			obj->ctype.expl_info.delete_objnum	= dio_get_short(pp);
			obj->ctype.expl_info.next_attach = obj->ctype.expl_info.prev_attach = obj->ctype.expl_info.attach_parent = -1;

			break;

		case CT_WEAPON:

			// read, then reset (the target game does the same)
			*pp += 8;
			obj->ctype.laser_info.parent_type		= -1;
			obj->ctype.laser_info.parent_num			= -1;
			obj->ctype.laser_info.parent_signature	= -1;

			break;

		case CT_LIGHT:

			obj->ctype.light_info.intensity = dio_get_fix(pp);
			obj->dtu.light_m = dio_get_short(pp);		// in the versions above 38
			break;

		case CT_POWERUP:

			// read and not used by the target game
			*pp += 4;
			obj->ctype.powerup_info.count = 1;
			break;

		case CT_NONE:
		case CT_FLYING:
		case CT_DEBRIS:
			break;

		case CT_SLEW:		//the player is generally saved as slew
			break;

		case CT_CNTRLCEN:
			break;

		default:
			printf( "Object with the unknown control type %d\n", obj->control_type );
			return -1;
	}

	switch (obj->render_type) {

		case RT_NONE:
			break;

		case RT_MORPH:
		case RT_POLYOBJ: {

			// The model number is an index into the POF name list of the
			// level, and the texture number is an index into the texture
			// name table of the level. level_resolve_models() and
			// level_translate_textures() set the numbers of the game.
			obj->dtu.model_num_level				= dio_get_int(pp);
			obj->rtype.pobj_info.model_num		= 0;

			obj->dtu.m_16e								= dio_get_short(pp);
			obj->dtu.m_170								= dio_get_short(pp);
			obj->dtu.m_172								= dio_get_int(pp);
			obj->dtu.m_176								= dio_get_int(pp);

			for (i=0;i<MAX_SUBMODELS;i++)
				vm_angvec_zero(&obj->rtype.pobj_info.anim_angles[i]);

			obj->rtype.pobj_info.subobj_flags	= dio_get_int(pp);

			obj->dtu.tmap_override_level			= dio_get_int(pp);
			obj->rtype.pobj_info.tmap_override	= -1;

			obj->rtype.pobj_info.alt_textures	= 0;

			break;
		}

		case RT_WEAPON_VCLIP:
		case RT_FIREBALL:

			obj->rtype.vclip_info.vclip_num	= dio_get_int(pp);
			obj->rtype.vclip_info.frametime	= dio_get_fix(pp);
			obj->rtype.vclip_info.framenum	= dio_get_byte(pp);

			break;

		case RT_POWERUP:

			// read and not used by the target game
			*pp += 9;
			break;

		case RT_ITEM:

			obj->dtu.item_bitmap = (short)dio_get_int(pp);
			break;

		case RT_LASER:
			break;

		case RT_HOSTAGE:		// the target game stops at a hostage
		default:
			printf( "Object with the unknown render type %d\n", obj->render_type );
			return -1;

	}

	obj->dtu.record_size = (int)(*pp - start);

	return 0;
}

// What verify_object() does that needs no table of the game. The tables
// of the target game (monsters, items, models) have another form than the
// ones of Descent 1, so the rest of verify_object() is not used.
static void verify_object_dtu( object * obj )
{
	obj->lifeleft = IMMORTAL_TIME;		//all loaded object are immortal, for now

	if ( obj->type == OBJ_ROBOT )
		Gamesave_num_org_robots++;

	if ( obj->type == OBJ_PLAYER )	{
		//Make sure orient matrix is orthogonal
		check_and_fix_matrix(&obj->orient);

		obj->id = Gamesave_num_players++;
	}
}

// ASCENT: new. Sets the model number of each polygon object of the loaded
// level from the name in the POF name list of the level, as verify_object()
// does it with Save_pof_names. Call it after load_level() when the models
// of the game are loaded (N_polygon_models, Pof_names).
// An object with a name that the game does not have gets model 0.
// If "report" is not 0, the function prints each name that it cannot find.
// Returns the number of those names.
int level_resolve_models(int report)
{
	static ubyte missing[MAX_LEVEL_POF_NAMES];
	int i, j, n_missing = 0;

	memset(missing, 0, sizeof(missing));

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];
		int n;

		if (obj->type == OBJ_NONE || (obj->render_type != RT_POLYOBJ && obj->render_type != RT_MORPH))
			continue;

		n = obj->dtu.model_num_level;
		obj->rtype.pobj_info.model_num = 0;

		if (n < 0 || n >= Num_level_pof_names)
			continue;

		for (j=0;j<N_polygon_models;j++)
			if (!stricmp(Pof_names[j],Level_pof_names[n])) {		//found it!
				obj->rtype.pobj_info.model_num = j;
				break;
			}

		if (j >= N_polygon_models && !missing[n]) {
			missing[n] = 1;
			n_missing++;
			if (report && N_polygon_models > 0)
				printf( "Level model %d '%s' is not in the model table of the game\n", n, Level_pof_names[n] );
		}
	}

	if (report && n_missing && N_polygon_models > 0)
		printf( "%d model names of the level are not in the model table of the game (%d models)\n", n_missing, N_polygon_models );

	return n_missing;
}

// Loads the game data from the level file in memory.
// returns 0=everything ok, -1=error
int load_game_data_dtu(const ubyte *data, int size, int start_offset)
{
	const ubyte *p, *end = data + size;
	int i,j;
	int version;

	//===================== READ FILE INFO ========================

	p = game_dtu_range(data, size, start_offset, 1, GAME_FILEINFO_DISK_SIZE, "game fileinfo");

	game_fileinfo.fileinfo_signature = dio_get_ushort(&p);
	game_fileinfo.fileinfo_version = dio_get_ushort(&p);
	game_fileinfo.fileinfo_sizeof = dio_get_int(&p);
	game_top_fileinfo.fileinfo_signature = game_fileinfo.fileinfo_signature;
	game_top_fileinfo.fileinfo_version = game_fileinfo.fileinfo_version;
	game_top_fileinfo.fileinfo_sizeof = game_fileinfo.fileinfo_sizeof;

	// Check signature
	if (game_fileinfo.fileinfo_signature != 0x6705)
		return -1;

	// Check version number. The executable of the target game loads the
	// versions 22 and up. The object record of each version below 40 has
	// another form, and no level file has such a version.
	version = game_fileinfo.fileinfo_version;
	if (version < GAME_VERSION_DTU_OLDEST || version > GAME_VERSION_DTU) {
		printf( "Game data version %d is not supported (only %d and %d)\n", version, GAME_VERSION_DTU_OLDEST, GAME_VERSION_DTU );
		return -1;
	}
	if (game_fileinfo.fileinfo_sizeof != GAME_FILEINFO_DISK_SIZE) {
		printf( "Game fileinfo size %d is not supported\n", game_fileinfo.fileinfo_sizeof );
		return -1;
	}

	dio_get_bytes(&p, game_fileinfo.mine_filename, 15);
	game_fileinfo.level = dio_get_int(&p);
	game_fileinfo.player_offset = dio_get_int(&p);				// Player info
	game_fileinfo.player_sizeof = dio_get_int(&p);
	game_fileinfo.object_offset = dio_get_int(&p);				// Object info
	game_fileinfo.object_howmany = dio_get_int(&p);
	game_fileinfo.object_sizeof = dio_get_int(&p);
	game_fileinfo.walls_offset = dio_get_int(&p);
	game_fileinfo.walls_howmany = dio_get_int(&p);
	game_fileinfo.walls_sizeof = dio_get_int(&p);
	game_fileinfo.doors_offset = dio_get_int(&p);
	game_fileinfo.doors_howmany = dio_get_int(&p);
	game_fileinfo.doors_sizeof = dio_get_int(&p);
	game_fileinfo.triggers_offset = dio_get_int(&p);
	game_fileinfo.triggers_howmany = dio_get_int(&p);
	game_fileinfo.triggers_sizeof = dio_get_int(&p);
	game_fileinfo.links_offset = dio_get_int(&p);
	game_fileinfo.links_howmany = dio_get_int(&p);
	game_fileinfo.links_sizeof = dio_get_int(&p);
	game_fileinfo.control_offset = dio_get_int(&p);
	game_fileinfo.control_howmany = dio_get_int(&p);
	game_fileinfo.control_sizeof = dio_get_int(&p);
	game_fileinfo.matcen_offset = dio_get_int(&p);
	game_fileinfo.matcen_howmany = dio_get_int(&p);
	game_fileinfo.matcen_sizeof = dio_get_int(&p);

	//load the level name. It ends with a zero byte.
	{
		int n = 0;

		while (p < end && *p != 0) {
			if (n < LEVEL_NAME_LEN-1)
				Current_level_name[n++] = *p;
			p++;
		}
		Current_level_name[n] = 0;
		if (p < end)
			p++;
	}

	//load pof names
	// The count of the file (N_save_pof_names) is 25 in all levels. The
	// list of the file is longer: it runs up to player_offset, and the
	// model numbers of the objects are indexes into the whole list. The
	// executable of the target game reads only the count times 13 bytes.
	Num_level_pof_names = 0;
	if (end - p >= 2) {
		N_save_pof_names = dio_get_ushort(&p);

		if (game_fileinfo.player_offset > (int)(p - data) && game_fileinfo.player_offset <= size)
			Num_level_pof_names = (game_fileinfo.player_offset - (int)(p - data)) / 13;
		else
			Num_level_pof_names = N_save_pof_names;
		if (Num_level_pof_names > MAX_LEVEL_POF_NAMES)
			Error( "Too many POF names (%d, maximum %d) in the level file", Num_level_pof_names, MAX_LEVEL_POF_NAMES );
		game_dtu_range(data, size, (int)(p - data), Num_level_pof_names, 13, "POF names");
		for (i=0;i<Num_level_pof_names;i++) {
			dio_get_bytes(&p, Level_pof_names[i], 13);
			Level_pof_names[i][12] = 0;
		}
	}

	//===================== READ PLAYER INFO ==========================
	Object_next_signature = 0;

	//===================== READ OBJECT INFO ==========================

	Gamesave_num_org_robots = 0;
	Gamesave_num_players = 0;

	if (game_fileinfo.object_howmany < 0 || game_fileinfo.object_howmany > MAX_OBJECTS)
		Error( "Too many objects (%d, maximum %d) in the level file", game_fileinfo.object_howmany, MAX_OBJECTS );

	if (game_fileinfo.object_offset > -1) {
		p = game_dtu_range(data, size, game_fileinfo.object_offset, 0, 0, "objects");

		for (i=0;i<game_fileinfo.object_howmany;i++)	{
			ubyte tail[OBJECT_DTU_MAX_DISK_SIZE];
			const ubyte *rp = p;
			int at_tail = 0;

			// The size of a record is known only after the read. Near the
			// end of the file the record is read from a copy with zeros
			// after the data.
			if (end - p < OBJECT_DTU_MAX_DISK_SIZE) {
				memset(tail, 0, sizeof(tail));
				memcpy(tail, p, end - p);
				rp = tail;
				at_tail = 1;
			}

			if (read_object_dtu(&Objects[i],&rp,version) != 0) {
				printf( "Error in object %d of the level file\n", i );
				return -1;
			}

			p += Objects[i].dtu.record_size;
			if (p > end || (at_tail && Objects[i].dtu.record_size > OBJECT_DTU_MAX_DISK_SIZE))
				Error( "The level file is damaged: object %d ends after the end of the file", i );

			Objects[i].signature = Object_next_signature++;
			verify_object_dtu( &Objects[i] );
		}

		// In all level files the walls follow the last object record.
		if (game_fileinfo.walls_offset > -1 && game_fileinfo.object_howmany > 0 && (int)(p - data) != game_fileinfo.walls_offset)
			printf( "Warning: the object records end at offset %d, the walls start at %d\n", (int)(p - data), game_fileinfo.walls_offset );
	}

	//===================== READ WALL INFO ============================

	if (game_fileinfo.walls_howmany < 0 || game_fileinfo.walls_howmany > MAX_WALLS)
		Error( "Too many walls (%d, maximum %d) in the level file", game_fileinfo.walls_howmany, MAX_WALLS );

	if (game_fileinfo.walls_offset > -1 && game_fileinfo.walls_howmany > 0)
	{
		if (WALL_DISK_SIZE != game_fileinfo.walls_sizeof)
			Error( "Bad wall record size (%d) in the level file", game_fileinfo.walls_sizeof );

		p = game_dtu_range(data, size, game_fileinfo.walls_offset, game_fileinfo.walls_howmany, WALL_DISK_SIZE, "walls");

		for (i=0;i<game_fileinfo.walls_howmany;i++) {
			wall_from_disk(&Walls[i], p);
			p += WALL_DISK_SIZE;

			if (Walls[i].segnum < 0 || Walls[i].segnum > Highest_segment_index || Walls[i].sidenum < 0 || Walls[i].sidenum >= MAX_SIDES_PER_SEGMENT)
				Error( "Wall %d is on segment %d, side %d", i, Walls[i].segnum, Walls[i].sidenum );
			if (Walls[i].linked_wall < -1 || Walls[i].linked_wall >= game_fileinfo.walls_howmany)
				Error( "Wall %d is linked to wall %d (%d walls)", i, Walls[i].linked_wall, game_fileinfo.walls_howmany );
		}
	}

	for (i=0; i<=Highest_segment_index; i++)
		for (j=0;j<MAX_SIDES_PER_SEGMENT;j++)
			if (Segments[i].sides[j].wall_num >= game_fileinfo.walls_howmany)
				Error( "Segment %d, side %d has the wall number %d (%d walls)", i, j, Segments[i].sides[j].wall_num, game_fileinfo.walls_howmany );

	//===================== READ DOOR INFO ============================

	if (game_fileinfo.doors_howmany < 0 || game_fileinfo.doors_howmany > MAX_DOORS)
		Error( "Too many active doors (%d, maximum %d) in the level file", game_fileinfo.doors_howmany, MAX_DOORS );

	if (game_fileinfo.doors_offset > -1 && game_fileinfo.doors_howmany > 0)
	{
		if (ACTIVE_DOOR_DISK_SIZE != game_fileinfo.doors_sizeof)
			Error( "Bad door record size (%d) in the level file", game_fileinfo.doors_sizeof );

		p = game_dtu_range(data, size, game_fileinfo.doors_offset, game_fileinfo.doors_howmany, ACTIVE_DOOR_DISK_SIZE, "active doors");

		for (i=0;i<game_fileinfo.doors_howmany;i++) {
			active_door_from_disk(&ActiveDoors[i], p);
			p += ACTIVE_DOOR_DISK_SIZE;
		}
	}

	//==================== READ TRIGGER INFO ==========================

	if (game_fileinfo.triggers_howmany < 0 || game_fileinfo.triggers_howmany > MAX_TRIGGERS)
		Error( "Too many triggers (%d, maximum %d) in the level file", game_fileinfo.triggers_howmany, MAX_TRIGGERS );

	if (game_fileinfo.triggers_offset > -1 && game_fileinfo.triggers_howmany > 0)	{
		if (TRIGGER_DISK_SIZE != game_fileinfo.triggers_sizeof)
			Error( "Bad trigger record size (%d) in the level file", game_fileinfo.triggers_sizeof );

		p = game_dtu_range(data, size, game_fileinfo.triggers_offset, game_fileinfo.triggers_howmany, TRIGGER_DISK_SIZE, "triggers");

		for (i=0;i<game_fileinfo.triggers_howmany;i++)	{
			trigger_from_disk(&Triggers[i], p);
			p += TRIGGER_DISK_SIZE;
		}
	}

	//================ READ CONTROL CENTER TRIGGER INFO ===============

	if (game_fileinfo.control_offset > -1 && game_fileinfo.control_howmany > 0)
	{
		if (CONTROL_CENTER_TRIGGERS_DISK_SIZE != game_fileinfo.control_sizeof)
			Error( "Bad control center record size (%d) in the level file", game_fileinfo.control_sizeof );

		p = game_dtu_range(data, size, game_fileinfo.control_offset, game_fileinfo.control_howmany, CONTROL_CENTER_TRIGGERS_DISK_SIZE, "control center triggers");

		for (i=0;i<game_fileinfo.control_howmany;i++) {
			control_center_triggers_from_disk(&ControlCenterTriggers, p);
			p += CONTROL_CENTER_TRIGGERS_DISK_SIZE;
		}
	}

	//================ READ MATERIALOGRIFIZATIONATORS INFO ===============

	if (game_fileinfo.matcen_howmany < 0 || game_fileinfo.matcen_howmany > MAX_ROBOT_CENTERS)
		Error( "Too many robot makers (%d, maximum %d) in the level file", game_fileinfo.matcen_howmany, MAX_ROBOT_CENTERS );

	if (game_fileinfo.matcen_offset > -1 && game_fileinfo.matcen_howmany > 0)
	{
		if (MATCEN_INFO_DISK_SIZE != game_fileinfo.matcen_sizeof)
			Error( "Bad robot maker record size (%d) in the level file", game_fileinfo.matcen_sizeof );

		p = game_dtu_range(data, size, game_fileinfo.matcen_offset, game_fileinfo.matcen_howmany, MATCEN_INFO_DISK_SIZE, "robot makers");

		for (i=0;i<game_fileinfo.matcen_howmany;i++) {
			matcen_info_from_disk(&RobotCenters[i], p);
			p += MATCEN_INFO_DISK_SIZE;
			//	Set links in RobotCenters to Station array
			for (j=0; j<=Highest_segment_index; j++)
				if (Segments[j].special == SEGMENT_IS_ROBOTMAKER)
					if (Segments[j].matcen_num == i)
						RobotCenters[i].fuelcen_num = Segments[j].value;
		}
	}

	//========================= UPDATE VARIABLES ======================

	reset_objects(game_fileinfo.object_howmany > 0 ? game_fileinfo.object_howmany : 1);
	if (game_fileinfo.object_howmany <= 0)
		Objects[0].type = OBJ_NONE;

	for (i=0; i<MAX_OBJECTS; i++) {
		Objects[i].next = Objects[i].prev = -1;
		if (Objects[i].type != OBJ_NONE) {
			int objsegnum = Objects[i].segnum;

			if (objsegnum < 0 || objsegnum > Highest_segment_index)		//bogus object
				Objects[i].type = OBJ_NONE;
			else {
				Objects[i].segnum = -1;			//avoid Assert()
				obj_link(i,objsegnum);
			}
		}
	}

	// The original called clear_transient_objects(1) here. It deletes each
	// object of the type OBJ_WEAPON. In the target game an object of that
	// type in a level file is a weapon item that lies in the level, so the
	// call is removed.

	// "Make sure non-transparent doors are set correctly" is in
	// level_translate_textures() now.

	Num_walls = game_fileinfo.walls_howmany;
	reset_walls();

	Num_open_doors = game_fileinfo.doors_howmany;
	Num_triggers = game_fileinfo.triggers_howmany;

	Num_robot_centers = game_fileinfo.matcen_howmany;

	// fix_object_segs() puts an object into the segment that its position
	// is in. If the position is in no segment, it moves the object to the
	// center of its segment (and gives an Int3 message). The counters tell
	// how many objects got another segment and another position.
	{
		static short file_segnum[MAX_OBJECTS];

		for (i=0;i<=Highest_object_index;i++)
			file_segnum[i] = Objects[i].segnum;

		fix_object_segs();

		Level_objects_moved = 0;
		Level_objects_centered = 0;
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type != OBJ_NONE) {
				if (file_segnum[i] != Objects[i].segnum)
					Level_objects_moved++;
				if (Objects[i].pos.x != Objects[i].dtu.pos_level.x || Objects[i].pos.y != Objects[i].dtu.pos_level.y || Objects[i].pos.z != Objects[i].dtu.pos_level.z)
					Level_objects_centered++;
			}
	}

	return 0;
}

//loads a level (.LVL) file from disk
// ASCENT: the file comes from dtu_load_file(): a name with the extension
// LVL gives the LV2 file of the 1.3 patch if there is one, and the packed
// level of the disc is unpacked. A name with no extension gets ".LVL".
// The texture numbers and the model numbers of the level are not
// translated here. The caller does that with level_translate_textures()
// and level_resolve_models() when the tables of the game are loaded.
// Returns 0, or 1 if the file is not a level that this program can load.
int load_level(char * filename_passed)
{
	ubyte *data;
	const ubyte *p;
	char filename[128];
	int size;
	int sig,version,minedata_offset,gamedata_offset,hostagetext_offset;
	int mine_err,game_err;

	#ifndef RELEASE
	Level_being_loaded = filename_passed;
	#endif

	Assert(strlen(filename_passed) < 120);
	strcpy(filename,filename_passed);
	if (!strchr(filename,'.'))
		strcat(filename,".LVL");

	data = dtu_load_file( filename, &size );

	if (!data)
		Error("Can't open file <%s>\n",filename);

	strcpy( Gamesave_current_filename, filename );

	if (size < 20) {
		free(data);
		return 1;
	}

	p = data;
	sig						= dio_get_int(&p);
	version					= dio_get_int(&p);
	minedata_offset		= dio_get_int(&p);
	gamedata_offset		= dio_get_int(&p);
	hostagetext_offset	= dio_get_int(&p);		// the offset of the end of the game data. Not used.
	(void)hostagetext_offset;

	if (sig != 0x504C564C) {		// 'PLVL', the bytes "LVLP"
		printf( "The file %s is not a level (no LVLP signature)\n", filename );
		free(data);
		return 1;
	}

	Level_file_size = size;

	mine_err = load_mine_data_dtu(data, size, minedata_offset);

	if (mine_err == -1) {	//error!!
		free(data);
		return 1;
	}

	game_err = load_game_data_dtu(data, size, gamedata_offset);

	free(data);

	if (game_err == -1)	//error!!
		return 1;

	Level_mine_version = mine_top_fileinfo.fileinfo_version;
	Level_game_version = game_top_fileinfo.fileinfo_version;
	(void)version;

	return 0;
}

// ASCENT: new. The summary of the loaded level for the option -checklevel
// of the program. tools/lvl.py prints the same text from the level file
// (option --summary), so that the two can be compared. Each checksum is
// the 32-bit FNV-1a hash of the values, each value as 1, 2 or 4 bytes in
// little endian order.

static unsigned int sum_u8(unsigned int h, int v)
{
	h ^= (unsigned int)(v & 0xff);
	return h * 16777619u;
}

static unsigned int sum_u16(unsigned int h, int v)
{
	h = sum_u8(h, v);
	return sum_u8(h, v >> 8);
}

static unsigned int sum_u32(unsigned int h, int v)
{
	h = sum_u16(h, v);
	return sum_u16(h, (int)((unsigned int)v >> 16));
}

static unsigned int sum_vec(unsigned int h, vms_vector *v)
{
	h = sum_u32(h, v->x);
	h = sum_u32(h, v->y);
	return sum_u32(h, v->z);
}

#define SUM_START	2166136261u

void level_print_summary(FILE *f, char *name)
{
	char base[128];
	char *t;
	int i, j, k, n;
	int type_count[256];
	vms_vector lo, hi;
	unsigned int h;

	// the name of the level: upper case, no directory, no extension
	t = name;
	for (i=0; name[i]; i++)
		if (name[i] == '/' || name[i] == '\\' || name[i] == ':')
			t = name + i + 1;
	strncpy(base, t, sizeof(base)-1);
	base[sizeof(base)-1] = 0;
	if ((t = strchr(base,'.')) != NULL)
		*t = 0;
	strupr(base);

	fprintf(f, "level %s\n", base);
	fprintf(f, "file_size %d\n", Level_file_size);
	fprintf(f, "mine_version %d\n", Level_mine_version);
	fprintf(f, "game_version %d\n", Level_game_version);
	fprintf(f, "level_name %s\n", Current_level_name);
	fprintf(f, "vertices %d\n", Num_vertices);
	fprintf(f, "segments %d\n", Num_segments);
	fprintf(f, "texture_names %d\n", Num_level_tmap_names);
	fprintf(f, "pof_names %d\n", Num_level_pof_names);

	memset(type_count, 0, sizeof(type_count));
	n = 0;
	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type != OBJ_NONE) {
			type_count[Objects[i].type]++;
			n++;
		}
	fprintf(f, "objects %d\n", n);
	for (i=0; i<256; i++)
		if (type_count[i])
			fprintf(f, "objects_type %d %d\n", i, type_count[i]);

	// the objects that are in the object lists of the segments
	n = 0;
	for (i=0; i<=Highest_segment_index; i++) {
		int count = 0;
		for (j=Segments[i].objects; j!=-1 && count<=MAX_OBJECTS; j=Objects[j].next) {
			if (Objects[j].segnum == i)
				n++;
			count++;
		}
	}
	fprintf(f, "objects_linked %d\n", n);

	memset(type_count, 0, sizeof(type_count));
	for (i=0; i<Num_walls; i++)
		type_count[Walls[i].type]++;
	fprintf(f, "walls %d\n", Num_walls);
	for (i=0; i<256; i++)
		if (type_count[i])
			fprintf(f, "walls_type %d %d\n", i, type_count[i]);

	fprintf(f, "doors %d\n", Num_open_doors);
	fprintf(f, "triggers %d\n", Num_triggers);
	fprintf(f, "matcens %d\n", Num_robot_centers);

	// the bounding box of the vertices, as fix values
	vm_vec_zero(&lo);
	vm_vec_zero(&hi);
	for (i=0; i<Num_vertices; i++) {
		vms_vector *v = &Vertices[i];
		if (i == 0) {
			lo = hi = *v;
		}
		if (v->x < lo.x) lo.x = v->x;
		if (v->y < lo.y) lo.y = v->y;
		if (v->z < lo.z) lo.z = v->z;
		if (v->x > hi.x) hi.x = v->x;
		if (v->y > hi.y) hi.y = v->y;
		if (v->z > hi.z) hi.z = v->z;
	}
	fprintf(f, "bbox %d %d %d %d %d %d\n", lo.x, lo.y, lo.z, hi.x, hi.y, hi.z);

	// the player start: the first object of the type OBJ_PLAYER
	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_PLAYER)
			break;
	if (i <= Highest_object_index)
		fprintf(f, "player_start %d %d %d %d %d\n", i, Objects[i].segnum, Objects[i].dtu.pos_level.x, Objects[i].dtu.pos_level.y, Objects[i].dtu.pos_level.z);
	else
		fprintf(f, "player_start none\n");

	h = SUM_START;
	for (i=0; i<Num_vertices; i++)
		h = sum_vec(h, &Vertices[i]);
	fprintf(f, "checksum_vertices %08x\n", h);

	h = SUM_START;
	for (i=0; i<Num_segments; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++)
			h = sum_u16(h, Segments[i].children[j]);
	fprintf(f, "checksum_children %08x\n", h);

	h = SUM_START;
	for (i=0; i<Num_segments; i++)
		for (j=0; j<MAX_VERTICES_PER_SEGMENT; j++)
			h = sum_u16(h, Segments[i].verts[j]);
	fprintf(f, "checksum_segment_verts %08x\n", h);

	// the sides: the byte at offset 1, the wall number, the texture
	// numbers of the level file, the uvl values. The type and the normals
	// are not in the sum, because validate_segment() computes them.
	h = SUM_START;
	for (i=0; i<Num_segments; i++)
		for (j=0; j<MAX_SIDES_PER_SEGMENT; j++) {
			side *s = &Segments[i].sides[j];
			h = sum_u8(h, s->pad);
			h = sum_u16(h, s->wall_num);
			h = sum_u16(h, Segments[i].dtu_tmap_num[j]);
			h = sum_u16(h, Segments[i].dtu_tmap_num2[j]);
			for (k=0; k<4; k++) {
				h = sum_u32(h, s->uvls[k].u);
				h = sum_u32(h, s->uvls[k].v);
				h = sum_u32(h, s->uvls[k].l);
			}
		}
	fprintf(f, "checksum_sides %08x\n", h);

	// the other values of the segments. "value" is not in the sum, because
	// fuelcen_activate() sets it.
	h = SUM_START;
	for (i=0; i<Num_segments; i++) {
		h = sum_u16(h, Segments[i].segnum);
		h = sum_u32(h, Segments[i].static_light);
		for (j=0; j<DTU_SEGMENT_UNKNOWN_BYTES; j++)
			h = sum_u8(h, Segments[i].dtu_unknown[j]);
		h = sum_u8(h, Segments[i].special);
		h = sum_u8(h, Segments[i].matcen_num);
	}
	fprintf(f, "checksum_segment_data %08x\n", h);

	h = SUM_START;
	for (i=0; i<Num_walls; i++) {
		wall *w = &Walls[i];
		h = sum_u32(h, w->segnum);
		h = sum_u32(h, w->sidenum);
		h = sum_u32(h, w->hps);
		h = sum_u32(h, w->linked_wall);
		h = sum_u8(h, w->type);
		h = sum_u8(h, w->flags);
		h = sum_u8(h, w->state);
		h = sum_u8(h, w->trigger);
		h = sum_u8(h, w->clip_num);
		h = sum_u8(h, w->keys);
	}
	fprintf(f, "checksum_walls %08x\n", h);

	// the objects. The position and the orientation are the ones of the
	// file (fix_object_segs() can move an object, and the loader makes the
	// matrix of the player orthogonal). The id of a player is not in the
	// sum (the loader numbers the players).
	h = SUM_START;
	for (i=0; i<=Highest_object_index; i++) {
		object *o = &Objects[i];
		dtu_object_info *d = &o->dtu;

		if (o->type == OBJ_NONE)
			continue;

		h = sum_u8(h, o->type);
		h = sum_u8(h, o->type == OBJ_PLAYER ? 0 : o->id);
		h = sum_u8(h, o->control_type);
		h = sum_u8(h, o->movement_type);
		h = sum_u8(h, o->render_type);
		h = sum_u8(h, o->flags);
		h = sum_u16(h, o->segnum);
		h = sum_u16(h, d->m_10);
		h = sum_vec(h, &d->pos_level);
		h = sum_vec(h, &d->orient_copy.rvec);
		h = sum_vec(h, &d->orient_copy.uvec);
		h = sum_vec(h, &d->orient_copy.fvec);
		h = sum_u32(h, o->size);
		h = sum_u32(h, o->shields);
		h = sum_vec(h, &o->last_pos);
		h = sum_u8(h, o->contains_type);
		h = sum_u8(h, o->contains_id);
		h = sum_u8(h, o->contains_count);

		h = sum_u16(h, d->m_80);
		h = sum_u16(h, d->m_82);
		h = sum_u32(h, d->m_88_level);
		for (j=0; j<13; j++) h = sum_u32(h, d->m_8c[j]);
		for (j=0; j<2; j++) h = sum_u32(h, d->m_c0[j]);
		for (j=0; j<6; j++) h = sum_u32(h, d->m_c8[j]);
		for (j=0; j<2; j++) h = sum_u32(h, d->m_e0[j]);
		for (j=0; j<5; j++) h = sum_u32(h, d->m_e8[j]);
		h = sum_u32(h, d->m_1b0);
		h = sum_u32(h, d->m_88);
		h = sum_u8(h, d->m_1d5);

		if (o->movement_type == MT_PHYSICS) {
			physics_info *pi = &o->mtype.phys_info;
			h = sum_vec(h, &pi->velocity);
			h = sum_vec(h, &pi->thrust);
			h = sum_u32(h, pi->mass);
			h = sum_u32(h, pi->drag);
			h = sum_u32(h, pi->brakes);
			h = sum_vec(h, &pi->rotvel);
			h = sum_vec(h, &pi->rotthrust);
			h = sum_u16(h, pi->turnroll);
			h = sum_u16(h, pi->flags);
		} else if (o->movement_type == MT_SPINNING)
			h = sum_vec(h, &o->mtype.spin_rate);

		if (o->control_type == CT_AI) {
			ai_static *ai = &o->ctype.ai_info;
			h = sum_u8(h, ai->behavior);
			for (j=0; j<MAX_AI_FLAGS; j++) h = sum_u8(h, ai->flags[j]);
			h = sum_u16(h, ai->hide_segment);
			h = sum_u16(h, ai->hide_index);
			h = sum_u16(h, ai->path_length);
			h = sum_u16(h, ai->cur_path_index);
			h = sum_u16(h, ai->follow_path_start_seg);
			h = sum_u16(h, ai->follow_path_end_seg);
		}

		if (o->render_type == RT_POLYOBJ || o->render_type == RT_MORPH) {
			h = sum_u32(h, d->model_num_level);
			h = sum_u16(h, d->m_16e);
			h = sum_u16(h, d->m_170);
			h = sum_u32(h, d->m_172);
			h = sum_u32(h, d->m_176);
			h = sum_u32(h, o->rtype.pobj_info.subobj_flags);
			h = sum_u32(h, d->tmap_override_level);
		} else if (o->render_type == RT_ITEM)
			h = sum_u16(h, d->item_bitmap);

		h = sum_u32(h, d->record_size);
	}
	fprintf(f, "checksum_objects %08x\n", h);
}

#ifdef EDITOR
void get_level_name()
{
//NO_UI!!!	UI_WINDOW 				*NameWindow = NULL;
//NO_UI!!!	UI_GADGET_INPUTBOX	*NameText;
//NO_UI!!!	UI_GADGET_BUTTON 		*QuitButton;
//NO_UI!!!
//NO_UI!!!	// Open a window with a quit button
//NO_UI!!!	NameWindow = ui_open_window( 20, 20, 300, 110, WIN_DIALOG );
//NO_UI!!!	QuitButton = ui_add_gadget_button( NameWindow, 150-24, 60, 48, 40, "Done", NULL );
//NO_UI!!!
//NO_UI!!!	ui_wprintf_at( NameWindow, 10, 12,"Please enter a name for this mine:" );
//NO_UI!!!	NameText = ui_add_gadget_inputbox( NameWindow, 10, 30, LEVEL_NAME_LEN, LEVEL_NAME_LEN, Current_level_name );
//NO_UI!!!
//NO_UI!!!	NameWindow->keyboard_focus_gadget = (UI_GADGET *)NameText;
//NO_UI!!!	QuitButton->hotkey = KEY_ENTER;
//NO_UI!!!
//NO_UI!!!	ui_gadget_calc_keys(NameWindow);
//NO_UI!!!
//NO_UI!!!	while (!QuitButton->pressed && last_keypress!=KEY_ENTER) {
//NO_UI!!!		ui_mega_process();
//NO_UI!!!		ui_window_do_gadgets(NameWindow);
//NO_UI!!!	}
//NO_UI!!!
//NO_UI!!!	strcpy( Current_level_name, NameText->text );
//NO_UI!!!
//NO_UI!!!	if ( NameWindow!=NULL )	{
//NO_UI!!!		ui_close_window( NameWindow );
//NO_UI!!!		NameWindow = NULL;
//NO_UI!!!	}
//NO_UI!!!

	newmenu_item m[2];

	m[0].type = NM_TYPE_TEXT; m[0].text = "Please enter a name for this mine:";
	m[1].type = NM_TYPE_INPUT; m[1].text = Current_level_name; m[1].text_len = LEVEL_NAME_LEN;

	newmenu_do( NULL, "Enter mine name", 2, m, NULL );

}
#endif


#ifdef EDITOR

int	Errors_in_mine;

// -----------------------------------------------------------------------------
// Save game
int save_game_data(FILE * SaveFile)
{
	int  player_offset, object_offset, walls_offset, doors_offset, triggers_offset, control_offset, matcen_offset; //, links_offset;
	int start_offset,end_offset;

	start_offset = ftell(SaveFile);

	//===================== SAVE FILE INFO ========================

	game_fileinfo.fileinfo_signature =	0x6705;
	game_fileinfo.fileinfo_version	=	GAME_VERSION;
	game_fileinfo.level					=  Current_level_num;
	game_fileinfo.fileinfo_sizeof		=	sizeof(game_fileinfo);
	game_fileinfo.player_offset		=	-1;
	game_fileinfo.player_sizeof		=	sizeof(player);
	game_fileinfo.object_offset		=	-1;
	game_fileinfo.object_howmany		=	Highest_object_index+1;
	game_fileinfo.object_sizeof		=	sizeof(object);
	game_fileinfo.walls_offset			=	-1;
	game_fileinfo.walls_howmany		=	Num_walls;
	game_fileinfo.walls_sizeof			=	sizeof(wall);
	game_fileinfo.doors_offset			=	-1;
	game_fileinfo.doors_howmany		=	Num_open_doors;
	game_fileinfo.doors_sizeof			=	sizeof(active_door);
	game_fileinfo.triggers_offset		=	-1;
	game_fileinfo.triggers_howmany	=	Num_triggers;
	game_fileinfo.triggers_sizeof		=	sizeof(trigger);
	game_fileinfo.control_offset		=	-1;
	game_fileinfo.control_howmany		=  1;
	game_fileinfo.control_sizeof		=  sizeof(control_center_triggers);
 	game_fileinfo.matcen_offset		=	-1;
	game_fileinfo.matcen_howmany		=	Num_robot_centers;
	game_fileinfo.matcen_sizeof		=	sizeof(matcen_info);

	// Write the fileinfo
	fwrite( &game_fileinfo, sizeof(game_fileinfo), 1, SaveFile );

	// Write the mine name
	fprintf(SaveFile,Current_level_name);
	fputc(0,SaveFile);		//terminator for string

	fwrite(&N_save_pof_names,2,1,SaveFile);
	fwrite(Pof_names,N_polygon_models,13,SaveFile);

	//==================== SAVE PLAYER INFO ===========================

	player_offset = ftell(SaveFile);
	fwrite( &Players[Player_num], sizeof(player), 1, SaveFile );

	//==================== SAVE OBJECT INFO ===========================

	object_offset = ftell(SaveFile);
	//fwrite( &Objects, sizeof(object), game_fileinfo.object_howmany, SaveFile );
	{
		int i;
		for (i=0;i<game_fileinfo.object_howmany;i++)
			write_object(&Objects[i],SaveFile);
	}

	//==================== SAVE WALL INFO =============================

	walls_offset = ftell(SaveFile);
	fwrite( Walls, sizeof(wall), game_fileinfo.walls_howmany, SaveFile );

	//==================== SAVE DOOR INFO =============================

	doors_offset = ftell(SaveFile);
	fwrite( ActiveDoors, sizeof(active_door), game_fileinfo.doors_howmany, SaveFile );

	//==================== SAVE TRIGGER INFO =============================

	triggers_offset = ftell(SaveFile);
	fwrite( Triggers, sizeof(trigger), game_fileinfo.triggers_howmany, SaveFile );

	//================ SAVE CONTROL CENTER TRIGGER INFO ===============

	control_offset = ftell(SaveFile);
	fwrite( &ControlCenterTriggers, sizeof(control_center_triggers), 1, SaveFile );


	//================ SAVE MATERIALIZATION CENTER TRIGGER INFO ===============

	matcen_offset = ftell(SaveFile);
	// mprintf((0, "Writing %i materialization centers\n", game_fileinfo.matcen_howmany));
	// { int i;
	// for (i=0; i<game_fileinfo.matcen_howmany; i++)
	// 	mprintf((0, "   %i: robot_flags = %08x\n", i, RobotCenters[i].robot_flags));
	// }
	fwrite( RobotCenters, sizeof(matcen_info), game_fileinfo.matcen_howmany, SaveFile );

	//============= REWRITE FILE INFO, TO SAVE OFFSETS ===============

	// Update the offset fields
	game_fileinfo.player_offset		=	player_offset;
	game_fileinfo.object_offset		=	object_offset;
	game_fileinfo.walls_offset			=	walls_offset;
	game_fileinfo.doors_offset			=	doors_offset;
	game_fileinfo.triggers_offset		=	triggers_offset;
	game_fileinfo.control_offset		=	control_offset;
	game_fileinfo.matcen_offset		=	matcen_offset;


	end_offset = ftell(SaveFile);

	// Write the fileinfo
	fseek(  SaveFile, start_offset, SEEK_SET );  // Move to TOF
	fwrite( &game_fileinfo, sizeof(game_fileinfo), 1, SaveFile );

	// Go back to end of data
	fseek(SaveFile,end_offset,SEEK_SET);

	return 0;
}

int save_mine_data(FILE * SaveFile);

// -----------------------------------------------------------------------------
// Save game
int save_level_sub(char * filename, int compiled_version)
{
	FILE * SaveFile;
	char temp_filename[128];
	int sig = 'PLVL',version=LEVEL_FILE_VERSION;
	int minedata_offset,gamedata_offset,hostagetext_offset;

	if ( !compiled_version )	{
		write_game_text_file(filename);

		if (Errors_in_mine) {
			if (is_real_level(filename)) {
				char  ErrorMessage[200];
	
				sprintf( ErrorMessage, "Warning: %i errors in this mine!\n", Errors_in_mine );
				stop_time();
				gr_palette_load(gr_palette);
	 
				if (nm_messagebox( NULL, 2, "Cancel Save", "Save", ErrorMessage )!=1)	{
					start_time();
					return 1;
				}
				start_time();
			} else
				mprintf((1, "Error: %i errors in this mine.  See the 'txm' file.\n", Errors_in_mine));
		}
		convert_name_to_LVL(temp_filename,filename);
	} else {
		convert_name_to_CDL(temp_filename,filename);
	}

	SaveFile = fopen( temp_filename, "wb" );
	if (!SaveFile)
	{
		char ErrorMessage[256];

		char fname[20];
		_splitpath( temp_filename, NULL, NULL, fname, NULL );

		sprintf( ErrorMessage, \
			"ERROR: Cannot write to '%s'.\nYou probably need to check out a locked\nversion of the file. You should save\nthis under a different filename, and then\ncheck out a locked copy by typing\n\'co -l %s.lvl'\nat the DOS prompt.\n" 
			, temp_filename, fname );
		stop_time();
		gr_palette_load(gr_palette);
		nm_messagebox( NULL, 1, "Ok", ErrorMessage );
		start_time();
		return 1;
	}

	if (Current_level_name[0] == 0)
		strcpy(Current_level_name,"Untitled");

	clear_transient_objects(1);		//1 means clear proximity bombs

	compress_objects();		//after this, Highest_object_index == num objects

	//make sure player is in a segment
	if (update_object_seg(&Objects[Players[0].objnum]) == 0) {
		if (ConsoleObject->segnum > Highest_segment_index)
			ConsoleObject->segnum = 0;
		compute_segment_center(&ConsoleObject->pos,&(Segments[ConsoleObject->segnum]));
	}
 
	fix_object_segs();

	//Write the header

	gs_write_int(sig,SaveFile);
	gs_write_int(version,SaveFile);

	//save placeholders
	gs_write_int(minedata_offset,SaveFile);
	gs_write_int(gamedata_offset,SaveFile);
	gs_write_int(hostagetext_offset,SaveFile);

	//Now write the damn data

	minedata_offset = ftell(SaveFile);
	if ( !compiled_version )	
		save_mine_data(SaveFile);
	else
		save_mine_data_compiled(SaveFile);
	gamedata_offset = ftell(SaveFile);
	save_game_data(SaveFile);
	hostagetext_offset = ftell(SaveFile);

	#ifdef HOSTAGE_FACES
	save_hostage_data(SaveFile);
	#endif

	fseek(SaveFile,sizeof(sig)+sizeof(version),SEEK_SET);
	gs_write_int(minedata_offset,SaveFile);
	gs_write_int(gamedata_offset,SaveFile);
	gs_write_int(hostagetext_offset,SaveFile);

	//==================== CLOSE THE FILE =============================
	fclose(SaveFile);

	if ( !compiled_version )	{
		if (Function_mode == FMODE_EDITOR)
			editor_status("Saved mine %s, \"%s\"",filename,Current_level_name);
	}

	return 0;

}

int save_level(char * filename)
{
	int r1;

	// Save normal version...
	r1 = save_level_sub(filename, 0);

	// Save compiled version...
	save_level_sub(filename, 1);

	return r1;
}


#ifdef HOSTAGE_FACES
void save_hostage_data(FILE * fp)
{
	int i,num_hostages=0;

	// Find number of hostages in mine...
	for (i=0; i<=Highest_object_index; i++ )	{
		int num;
		if ( Objects[i].type == OBJ_HOSTAGE )	{
			num = Objects[i].id;
			#ifndef SHAREWARE
			if (num<0 || num>=MAX_HOSTAGES || Hostage_face_clip[Hostages[num].vclip_num].num_frames<=0)
				num=0;
			#else
			num = 0;
			#endif
			if (num+1 > num_hostages)
				num_hostages = num+1;
		}
	}

	gs_write_int(HOSTAGE_DATA_VERSION,fp);

	for (i=0; i<num_hostages; i++ )	{
		gs_write_int(Hostages[i].vclip_num,fp);
		fputs(Hostages[i].text,fp);
		fputc('\n',fp);		//fgets wants a newline
	}
}
#endif	//HOSTAGE_FACES

#endif	//EDITOR

#ifndef NDEBUG
void dump_mine_info(void)
{
	int	segnum, sidenum;
	fix	min_u, max_u, min_v, max_v, min_l, max_l, max_sl;

	min_u = F1_0*1000;
	min_v = min_u;
	min_l = min_u;

	max_u = -min_u;
	max_v = max_u;
	max_l = max_u;

	max_sl = 0;

	for (segnum=0; segnum<=Highest_segment_index; segnum++) {
		for (sidenum=0; sidenum<MAX_SIDES_PER_SEGMENT; sidenum++) {
			int	vertnum;
			side	*sidep = &Segments[segnum].sides[sidenum];

			if (Segments[segnum].static_light > max_sl)
				max_sl = Segments[segnum].static_light;

			for (vertnum=0; vertnum<4; vertnum++) {
				if (sidep->uvls[vertnum].u < min_u)
					min_u = sidep->uvls[vertnum].u;
				else if (sidep->uvls[vertnum].u > max_u)
					max_u = sidep->uvls[vertnum].u;

				if (sidep->uvls[vertnum].v < min_v)
					min_v = sidep->uvls[vertnum].v;
				else if (sidep->uvls[vertnum].v > max_v)
					max_v = sidep->uvls[vertnum].v;

				if (sidep->uvls[vertnum].l < min_l)
					min_l = sidep->uvls[vertnum].l;
				else if (sidep->uvls[vertnum].l > max_l)
					max_l = sidep->uvls[vertnum].l;
			}

		}
	}

//	mprintf((0, "Smallest uvl = %7.3f %7.3f %7.3f.  Largest uvl = %7.3f %7.3f %7.3f\n", f2fl(min_u), f2fl(min_v), f2fl(min_l), f2fl(max_u), f2fl(max_v), f2fl(max_l)));
//	mprintf((0, "Static light maximum = %7.3f\n", f2fl(max_sl)));
//	mprintf((0, "Number of walls: %i\n", Num_walls));

}

#endif

#ifdef HOSTAGE_FACES
void load_hostage_data(CFILE * fp,int do_read)
{
	int version,i,num,num_hostages;

	hostage_init_all();

	num_hostages = 0;

	// Find number of hostages in mine...
	for (i=0; i<=Highest_object_index; i++ )	{
		if ( Objects[i].type == OBJ_HOSTAGE )	{
			num = Objects[i].id;
			if (num+1 > num_hostages)
				num_hostages = num+1;

			if (Hostages[num].objnum != -1) {		//slot already used
				num = hostage_get_next_slot();		//..so get new slot
				if (num+1 > num_hostages)
					num_hostages = num+1;
				Objects[i].id = num;
			}

			if ( num > -1 && num < MAX_HOSTAGES )	{
				Assert(Hostages[num].objnum == -1);		//make sure not used
				// -- Matt -- commented out by MK on 11/19/94, hit often in level 3, level 4.  Assert(Hostages[num].objnum == -1);		//make sure not used
				Hostages[num].objnum = i;
				Hostages[num].objsig = Objects[i].signature;
			}
		}
	}

	if (do_read) {
		version = read_int(fp);

		for (i=0;i<num_hostages;i++) {

			Assert(Hostages[i].objnum != -1);		//make sure slot filled in

			Hostages[i].vclip_num = read_int(fp);

			#ifndef SHAREWARE
			if (Hostages[i].vclip_num<0 || Hostages[i].vclip_num>=MAX_HOSTAGES || Hostage_face_clip[Hostages[i].vclip_num].num_frames<=0)
				Hostages[i].vclip_num=0;

			Assert(Hostage_face_clip[Hostages[i].vclip_num].num_frames);
			#endif

			cfgets(Hostages[i].text, HOSTAGE_MESSAGE_LEN, fp);

			if (Hostages[i].text[strlen(Hostages[i].text)-1]=='\n')
				Hostages[i].text[strlen(Hostages[i].text)-1] = 0;
		}
	}
	else
		for (i=0;i<num_hostages;i++) {
			Assert(Hostages[i].objnum != -1);		//make sure slot filled in
			Hostages[i].vclip_num = 0;
		}

}
#endif	//HOSTAGE_FACES
