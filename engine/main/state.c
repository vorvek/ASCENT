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
 * $Source: f:/miner/source/main/rcs/state.c $
 * $Revision: 2.14 $
 * $Author: john $
 * $Date: 1995/05/26 16:16:10 $
 * 
 * Functions to save/restore game state.
 * 
 * $Log: state.c $
 * Revision 2.14  1995/05/26  16:16:10  john
 * Split SATURN into define's for requiring cd, using cd, etc.
 * Also started adding all the Rockwell stuff.
 * 
 * Revision 2.13  1995/04/06  15:12:20  john
 * Fixed bug with lunacy not working.
 * 
 * Revision 2.12  1995/04/04  13:33:05  john
 * Removed multiplayer save.
 * 
 * Revision 2.11  1995/03/31  13:42:10  john
 * Made saved games from the bogus saturn version read in
 * correctly.
 * 
 * Revision 2.10  1995/03/31  12:45:28  john
 * Fixed bug with previous.
 * 
 * Revision 2.9  1995/03/31  12:24:40  john
 * I had changed alt_textures from a pointer to a byte. This hosed old
 * saved games, so I restored it to an int.
 * 
 * Revision 2.8  1995/03/28  11:22:47  john
 * Added cheats to save file. Changed lunacy text.
 * 
 * Revision 2.7  1995/03/27  21:41:03  john
 * Added code to verify that the proper multi save file
 * is used when restoring a network game.
 * 
 * Revision 2.6  1995/03/27  18:04:18  john
 * Made multi save/restore require the -multisave command line arg.
 * 
 * Revision 2.5  1995/03/27  17:01:52  john
 * Made deafult choice work better.
 * 
 * Revision 2.4  1995/03/27  15:49:44  john
 * Added slots to save games.
 * 
 * Revision 2.3  1995/03/27  12:59:19  john
 * Initial version of multiplayer save games.
 * 
 * Revision 2.2  1995/03/24  13:11:35  john
 * Added save game during briefing screens.
 * 
 * Revision 2.1  1995/03/21  14:38:36  john
 * Ifdef'd out the NETWORK code.
 * 
 * Revision 2.0  1995/02/27  11:27:00  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.43  1995/02/22  14:32:41  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.42  1995/02/13  20:34:33  john
 * Lintized
 * 
 * Revision 1.41  1995/02/13  10:37:30  john
 * Saved Buggin' cheat mode to save file.
 * 
 * Revision 1.40  1995/02/09  10:24:25  john
 * *** empty log message ***
 * 
 * Revision 1.39  1995/02/09  10:22:20  john
 * Fixed bug with callsign getting trashed if you copy somebody else
 * 's save game file into your directory.
 * 
 * Revision 1.38  1995/02/08  21:01:27  john
 * Closed state file around the code that shows briefing screens so that the
 * code works on machines with clean boot with only 5 file handles.
 * 
 * Revision 1.37  1995/02/07  14:02:33  john
 * Added code to verify game restore.
 * 
 * Revision 1.36  1995/02/07  11:07:43  john
 * Added hooks for confirm on game state restore.
 * 
 * Revision 1.35  1995/02/03  11:27:36  john
 * Made inbetween level save's thumbnail's have correct aspect.
 * 
 * Revision 1.34  1995/02/03  10:58:43  john
 * Added code to save shareware style saved games into new format...
 * Also, made new player file format not have the saved game array in it.
 * 
 * Revision 1.33  1995/02/02  19:40:52  john
 * Added 10 save game slots.
 * 
 * Revision 1.32  1995/02/02  12:23:20  john
 * Made between level saves have picture.
 * 
 * Revision 1.31  1995/01/31  11:21:43  john
 * Added code for fixed with menus.
 * 
 * Revision 1.30  1995/01/29  21:37:29  mike
 * initialize variables on game load so you don't drain your energy when you fire.
 * 
 * Revision 1.29  1995/01/29  13:47:58  mike
 * Restore some variables on game load (in game).
 * 
 * Revision 1.28  1995/01/26  10:46:57  john
 * Fixed bug with state names getting hosed.
 * 
 * Revision 1.27  1995/01/26  09:51:23  john
 * Fixed bug with game descriptions getting hosed.
 * 
 * Revision 1.26  1995/01/25  16:35:49  john
 * Made so that when you hit enter during
 * game save, -empty- goes away.
 * 
 * Revision 1.25  1995/01/25  15:01:39  john
 * Upped the save file version.
 * 
 * Revision 1.24  1995/01/24  20:35:35  john
 * *** empty log message ***
 * 
 * Revision 1.23  1995/01/24  20:34:24  john
 * Fixed bug with player stats not being set right for in
 * between level saves.
 * 
 * Revision 1.22  1995/01/23  10:39:03  john
 * Added mission stuff to game saves.
 * 
 * Revision 1.21  1995/01/22  16:07:12  mike
 * localization.
 * 
 * Revision 1.20  1995/01/22  15:58:32  mike
 * localization
 * 
 * Revision 1.19  1995/01/20  11:04:40  john
 * Upped state save version.
 * 
 * Revision 1.18  1995/01/19  17:00:44  john
 * Made save game work between levels.
 * 
 * Revision 1.17  1995/01/17  14:27:33  john
 * *** empty log message ***
 * 
 * Revision 1.16  1995/01/17  13:36:37  john
 * Moved pig loading into StartNewLevelSub.
 * 
 * Revision 1.15  1995/01/16  16:53:38  john
 * Added code to save cheat state during save game.
 * 
 * Revision 1.14  1995/01/15  16:55:22  john
 * Improved mine texture parsing.
 * 
 * Revision 1.13  1995/01/12  10:45:15  john
 * Added difficulty level to save/restore game.
 * 
 * Revision 1.12  1995/01/05  15:46:55  john
 * Made weapons not rearm when starting a saved game.
 * 
 * Revision 1.11  1995/01/05  11:51:45  john
 * Added better Abort game menu.
 * Made save state return success or nopt.
 * 
 * Revision 1.10  1995/01/05  11:34:51  john
 * Took out endlevel save stuff for registered.
 * 
 * Revision 1.9  1995/01/04  18:19:52  john
 * Added automap visited list saving.
 * 
 * Revision 1.8  1995/01/04  17:29:56  john
 * Made save/restore ALT+F?. Also made them not work
 * in network mode, and if recording a demo, will 
 * quit recording.
 * 
 * Revision 1.7  1995/01/04  13:18:31  john
 * Added cool 6 game save.
 * 
 * Revision 1.6  1995/01/03  20:38:46  john
 * Saved morph objects.
 * 
 * Revision 1.5  1995/01/03  20:19:29  john
 * Pretty good working version of game save.
 * 
 * Revision 1.4  1995/01/03  14:18:18  matt
 * ifdefs added to compile code add.  Added by Mike, I think.
 * 
 * Revision 1.3  1994/12/29  18:40:19  john
 * Initial version.
 * 
 * Revision 1.2  1994/12/29  15:26:40  john
 * Put in hooks for saving/restoring game state.
 * 
 * Revision 1.1  1994/12/29  15:16:02  john
 * Initial revision
 * 
 * 
 */


#pragma off (unreferenced)
static char rcsid[] = "$Id: state.c 2.14 1995/05/26 16:16:10 john Exp $";
#pragma on (unreferenced)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "mono.h"
#include "inferno.h"
#include "segment.h"
#include "textures.h"
#include "wall.h"
#include "object.h"
#include "gamemine.h"
#include "error.h"
#include "gameseg.h"
#include "switch.h"
#include "game.h"
#include "newmenu.h"
#include "cfile.h"		
#include "fuelcen.h"
#include "hash.h"
#include "key.h"
#include "piggy.h"
#include "player.h"
#include "cntrlcen.h"
#include "morph.h"
#include "weapon.h"
#include "render.h"
#include "gameseq.h"
#include "gauges.h"
#include "newdemo.h"
#include "automap.h"
#include "piggy.h"
#include "paging.h"
#include "titles.h"
#include "text.h"
#include "mission.h"
#include "pcx.h"
#include "mem.h"
#include "network.h"
#include "args.h"
#include "state.h"		// ASCENT: for the prototypes of this file
#include "playsave.h"	// ASCENT: for player_from_disk and player_to_disk
#include "ai.h"
#include "switch.h"

#ifndef SHAREWARE

#define STATE_VERSION 7
#define STATE_COMPATIBLE_VERSION 6
// 0 - Put DGSS (Descent Game State Save) id at tof.
// 1 - Added Difficulty level save
// 2 - Added Cheats_enabled flag
// 3 - Added between levels save.
// 4 - Added mission support
// 5 - Mike changed ai and object structure.
// 6 - Added buggin' cheat save
// 7 - Added other cheat saves and game_id.

#define NUM_SAVES 10
#define THUMBNAIL_W 100
#define THUMBNAIL_H 50
#define DESC_LENGTH 20

extern int ai_save_state( FILE * fp );
extern int ai_restore_state( FILE * fp );

extern void multi_initiate_save_game();
extern void multi_initiate_restore_game();

extern int Do_appearance_effect;
extern fix Fusion_next_sound_time;

extern int Laser_rapid_fire, Ugly_robot_cheat, Ugly_robot_texture;
extern int Physics_cheat_flag;
extern int	Lunacy;
extern void do_lunacy_on(void);
extern void do_lunacy_off(void);

int sc_last_item= 0;
grs_bitmap *sc_bmp[NUM_SAVES];

char dgss_id[4] = "DGSS";

int state_default_item = 0;

uint state_game_id;

//	-----------------------------------------------------------------------------
// ASCENT: the save game file of the 1995 game is a dump of C variables and
// of arrays of C structures. The structures had 1 byte packing (the Watcom
// default) and the numbers are little endian. The functions below read and
// write that format one field at a time, so the file does not depend on
// the padding and the byte order of the structures in memory, and a file
// of the DOS game stays valid. No structure of the file has a pointer.
//
// The sizes of the records in the file:
#define OBJECT_DISK_SIZE			264
#define WALL_DISK_SIZE				24
#define ACTIVE_DOOR_DISK_SIZE		16
#define TRIGGER_DISK_SIZE			54
#define MATCEN_INFO_DISK_SIZE		16
#define CC_TRIGGERS_DISK_SIZE		42
#define FUELCEN_DISK_SIZE			40

_Static_assert(MAX_SUBMODELS == 10 && MAX_AI_FLAGS == 11 && MAX_WALLS_PER_LINK == 10,
	"the record sizes of the save game file depend on these constants");

// Fields in a byte buffer. Each function moves the buffer pointer.
static int st_get_byte(const ubyte **pp)
{
	return (byte)*(*pp)++;
}

static int st_get_ubyte(const ubyte **pp)
{
	return *(*pp)++;
}

static int st_get_short(const ubyte **pp)
{
	const ubyte *p = *pp;
	*pp += 2;
	return (short)(p[0] | (p[1]<<8));
}

static int st_get_int(const ubyte **pp)
{
	const ubyte *p = *pp;
	*pp += 4;
	return (int)((uint)p[0] | ((uint)p[1]<<8) | ((uint)p[2]<<16) | ((uint)p[3]<<24));
}

static void st_get_vector(const ubyte **pp, vms_vector *v)
{
	v->x = st_get_int(pp);
	v->y = st_get_int(pp);
	v->z = st_get_int(pp);
}

static void st_put_byte(ubyte **pp, int v)
{
	*(*pp)++ = v & 0xff;
}

static void st_put_short(ubyte **pp, int v)
{
	ubyte *p = *pp;
	p[0] = v & 0xff;
	p[1] = (v>>8) & 0xff;
	*pp += 2;
}

static void st_put_int(ubyte **pp, int v)
{
	ubyte *p = *pp;
	p[0] = v & 0xff;
	p[1] = (v>>8) & 0xff;
	p[2] = (v>>16) & 0xff;
	p[3] = (v>>24) & 0xff;
	*pp += 4;
}

static void st_put_vector(ubyte **pp, vms_vector *v)
{
	st_put_int(pp, v->x);
	st_put_int(pp, v->y);
	st_put_int(pp, v->z);
}

// One int of the file. A short read gives 0, as a failed fread into a
// cleared variable.
static int st_read_int(FILE *fp)
{
	ubyte buf[4];
	const ubyte *p = buf;

	memset(buf, 0, 4);
	fread(buf, 4, 1, fp);
	return st_get_int(&p);
}

static void st_write_int(FILE *fp, int v)
{
	ubyte buf[4], *p = buf;

	st_put_int(&p, v);
	fwrite(buf, 4, 1, fp);
}

// The player structure (116 bytes, see playsave.c).
static void st_read_player(FILE *fp, player *pl)
{
	ubyte buf[PLAYER_DISK_SIZE];

	memset(buf, 0, sizeof(buf));
	fread(buf, PLAYER_DISK_SIZE, 1, fp);
	player_from_disk(pl, buf);
}

static void st_write_player(FILE *fp, player *pl)
{
	ubyte buf[PLAYER_DISK_SIZE];

	player_to_disk(pl, buf);
	fwrite(buf, PLAYER_DISK_SIZE, 1, fp);
}

// One object (264 bytes). The object has three unions. The member that is
// read or written is selected by movement_type, control_type and
// render_type, as in the code that uses the object. For a type that uses
// no member, the largest member is taken, so that no byte of the record
// is lost. spin_rate is at the place of phys_info.velocity, so the
// movement union is always handled as phys_info.
static void object_from_disk(object *obj, const ubyte *buf)
{
	const ubyte *p = buf;
	const ubyte *u;
	int i;

	memset(obj, 0, sizeof(*obj));

	obj->signature = st_get_int(&p);
	obj->type = st_get_ubyte(&p);
	obj->id = st_get_ubyte(&p);
	obj->next = st_get_short(&p);
	obj->prev = st_get_short(&p);
	obj->control_type = st_get_ubyte(&p);
	obj->movement_type = st_get_ubyte(&p);
	obj->render_type = st_get_ubyte(&p);
	obj->flags = st_get_ubyte(&p);
	obj->segnum = st_get_short(&p);
	obj->attached_obj = st_get_short(&p);
	st_get_vector(&p, &obj->pos);
	st_get_vector(&p, &obj->orient.rvec);
	st_get_vector(&p, &obj->orient.uvec);
	st_get_vector(&p, &obj->orient.fvec);
	obj->size = st_get_int(&p);
	obj->shields = st_get_int(&p);
	st_get_vector(&p, &obj->last_pos);
	obj->contains_type = st_get_byte(&p);
	obj->contains_id = st_get_byte(&p);
	obj->contains_count = st_get_byte(&p);
	obj->matcen_creator = st_get_byte(&p);
	obj->lifeleft = st_get_int(&p);

	// movement info: 64 bytes at offset 94
	u = buf + 94;
	st_get_vector(&u, &obj->mtype.phys_info.velocity);
	st_get_vector(&u, &obj->mtype.phys_info.thrust);
	obj->mtype.phys_info.mass = st_get_int(&u);
	obj->mtype.phys_info.drag = st_get_int(&u);
	obj->mtype.phys_info.brakes = st_get_int(&u);
	st_get_vector(&u, &obj->mtype.phys_info.rotvel);
	st_get_vector(&u, &obj->mtype.phys_info.rotthrust);
	obj->mtype.phys_info.turnroll = st_get_short(&u);
	obj->mtype.phys_info.flags = st_get_short(&u);

	// control info: 30 bytes at offset 158
	u = buf + 158;
	switch (obj->control_type) {
		case CT_WEAPON:
			obj->ctype.laser_info.parent_type = st_get_short(&u);
			obj->ctype.laser_info.parent_num = st_get_short(&u);
			obj->ctype.laser_info.parent_signature = st_get_int(&u);
			obj->ctype.laser_info.creation_time = st_get_int(&u);
			obj->ctype.laser_info.last_hitobj = st_get_short(&u);
			obj->ctype.laser_info.track_goal = st_get_short(&u);
			obj->ctype.laser_info.multiplier = st_get_int(&u);
			break;
		case CT_EXPLOSION:
		case CT_DEBRIS:
			obj->ctype.expl_info.spawn_time = st_get_int(&u);
			obj->ctype.expl_info.delete_time = st_get_int(&u);
			obj->ctype.expl_info.delete_objnum = st_get_short(&u);
			obj->ctype.expl_info.attach_parent = st_get_short(&u);
			obj->ctype.expl_info.prev_attach = st_get_short(&u);
			obj->ctype.expl_info.next_attach = st_get_short(&u);
			break;
		case CT_LIGHT:
			obj->ctype.light_info.intensity = st_get_int(&u);
			break;
		case CT_POWERUP:
			obj->ctype.powerup_info.count = st_get_int(&u);
			break;
		case CT_AI:
		default:
			obj->ctype.ai_info.behavior = st_get_ubyte(&u);
			for (i=0; i<MAX_AI_FLAGS; i++)
				obj->ctype.ai_info.flags[i] = st_get_byte(&u);
			obj->ctype.ai_info.hide_segment = st_get_short(&u);
			obj->ctype.ai_info.hide_index = st_get_short(&u);
			obj->ctype.ai_info.path_length = st_get_short(&u);
			obj->ctype.ai_info.cur_path_index = st_get_short(&u);
			obj->ctype.ai_info.follow_path_start_seg = st_get_short(&u);
			obj->ctype.ai_info.follow_path_end_seg = st_get_short(&u);
			obj->ctype.ai_info.danger_laser_signature = st_get_int(&u);
			obj->ctype.ai_info.danger_laser_num = st_get_short(&u);
			break;
	}

	// render info: 76 bytes at offset 188
	u = buf + 188;
	switch (obj->render_type) {
		case RT_FIREBALL:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_WEAPON_VCLIP:
			obj->rtype.vclip_info.vclip_num = st_get_int(&u);
			obj->rtype.vclip_info.frametime = st_get_int(&u);
			obj->rtype.vclip_info.framenum = st_get_byte(&u);
			break;
		case RT_POLYOBJ:
		case RT_MORPH:
		default:
			obj->rtype.pobj_info.model_num = st_get_int(&u);
			for (i=0; i<MAX_SUBMODELS; i++) {
				obj->rtype.pobj_info.anim_angles[i].p = st_get_short(&u);
				obj->rtype.pobj_info.anim_angles[i].b = st_get_short(&u);
				obj->rtype.pobj_info.anim_angles[i].h = st_get_short(&u);
			}
			obj->rtype.pobj_info.subobj_flags = st_get_int(&u);
			obj->rtype.pobj_info.tmap_override = st_get_int(&u);
			obj->rtype.pobj_info.alt_textures = st_get_int(&u);
			break;
	}
}

static void object_to_disk(object *obj, ubyte *buf)
{
	ubyte *p = buf;
	ubyte *u;
	int i;

	memset(buf, 0, OBJECT_DISK_SIZE);

	st_put_int(&p, obj->signature);
	st_put_byte(&p, obj->type);
	st_put_byte(&p, obj->id);
	st_put_short(&p, obj->next);
	st_put_short(&p, obj->prev);
	st_put_byte(&p, obj->control_type);
	st_put_byte(&p, obj->movement_type);
	st_put_byte(&p, obj->render_type);
	st_put_byte(&p, obj->flags);
	st_put_short(&p, obj->segnum);
	st_put_short(&p, obj->attached_obj);
	st_put_vector(&p, &obj->pos);
	st_put_vector(&p, &obj->orient.rvec);
	st_put_vector(&p, &obj->orient.uvec);
	st_put_vector(&p, &obj->orient.fvec);
	st_put_int(&p, obj->size);
	st_put_int(&p, obj->shields);
	st_put_vector(&p, &obj->last_pos);
	st_put_byte(&p, obj->contains_type);
	st_put_byte(&p, obj->contains_id);
	st_put_byte(&p, obj->contains_count);
	st_put_byte(&p, obj->matcen_creator);
	st_put_int(&p, obj->lifeleft);

	// movement info: 64 bytes at offset 94
	u = buf + 94;
	st_put_vector(&u, &obj->mtype.phys_info.velocity);
	st_put_vector(&u, &obj->mtype.phys_info.thrust);
	st_put_int(&u, obj->mtype.phys_info.mass);
	st_put_int(&u, obj->mtype.phys_info.drag);
	st_put_int(&u, obj->mtype.phys_info.brakes);
	st_put_vector(&u, &obj->mtype.phys_info.rotvel);
	st_put_vector(&u, &obj->mtype.phys_info.rotthrust);
	st_put_short(&u, obj->mtype.phys_info.turnroll);
	st_put_short(&u, obj->mtype.phys_info.flags);

	// control info: 30 bytes at offset 158
	u = buf + 158;
	switch (obj->control_type) {
		case CT_WEAPON:
			st_put_short(&u, obj->ctype.laser_info.parent_type);
			st_put_short(&u, obj->ctype.laser_info.parent_num);
			st_put_int(&u, obj->ctype.laser_info.parent_signature);
			st_put_int(&u, obj->ctype.laser_info.creation_time);
			st_put_short(&u, obj->ctype.laser_info.last_hitobj);
			st_put_short(&u, obj->ctype.laser_info.track_goal);
			st_put_int(&u, obj->ctype.laser_info.multiplier);
			break;
		case CT_EXPLOSION:
		case CT_DEBRIS:
			st_put_int(&u, obj->ctype.expl_info.spawn_time);
			st_put_int(&u, obj->ctype.expl_info.delete_time);
			st_put_short(&u, obj->ctype.expl_info.delete_objnum);
			st_put_short(&u, obj->ctype.expl_info.attach_parent);
			st_put_short(&u, obj->ctype.expl_info.prev_attach);
			st_put_short(&u, obj->ctype.expl_info.next_attach);
			break;
		case CT_LIGHT:
			st_put_int(&u, obj->ctype.light_info.intensity);
			break;
		case CT_POWERUP:
			st_put_int(&u, obj->ctype.powerup_info.count);
			break;
		case CT_AI:
		default:
			st_put_byte(&u, obj->ctype.ai_info.behavior);
			for (i=0; i<MAX_AI_FLAGS; i++)
				st_put_byte(&u, obj->ctype.ai_info.flags[i]);
			st_put_short(&u, obj->ctype.ai_info.hide_segment);
			st_put_short(&u, obj->ctype.ai_info.hide_index);
			st_put_short(&u, obj->ctype.ai_info.path_length);
			st_put_short(&u, obj->ctype.ai_info.cur_path_index);
			st_put_short(&u, obj->ctype.ai_info.follow_path_start_seg);
			st_put_short(&u, obj->ctype.ai_info.follow_path_end_seg);
			st_put_int(&u, obj->ctype.ai_info.danger_laser_signature);
			st_put_short(&u, obj->ctype.ai_info.danger_laser_num);
			break;
	}

	// render info: 76 bytes at offset 188
	u = buf + 188;
	switch (obj->render_type) {
		case RT_FIREBALL:
		case RT_HOSTAGE:
		case RT_POWERUP:
		case RT_WEAPON_VCLIP:
			st_put_int(&u, obj->rtype.vclip_info.vclip_num);
			st_put_int(&u, obj->rtype.vclip_info.frametime);
			st_put_byte(&u, obj->rtype.vclip_info.framenum);
			break;
		case RT_POLYOBJ:
		case RT_MORPH:
		default:
			st_put_int(&u, obj->rtype.pobj_info.model_num);
			for (i=0; i<MAX_SUBMODELS; i++) {
				st_put_short(&u, obj->rtype.pobj_info.anim_angles[i].p);
				st_put_short(&u, obj->rtype.pobj_info.anim_angles[i].b);
				st_put_short(&u, obj->rtype.pobj_info.anim_angles[i].h);
			}
			st_put_int(&u, obj->rtype.pobj_info.subobj_flags);
			st_put_int(&u, obj->rtype.pobj_info.tmap_override);
			st_put_int(&u, obj->rtype.pobj_info.alt_textures);
			break;
	}
}

// One wall (24 bytes).
static void wall_from_disk(wall *w, const ubyte *p)
{
	w->segnum = st_get_int(&p);
	w->sidenum = st_get_int(&p);
	w->hps = st_get_int(&p);
	w->linked_wall = st_get_int(&p);
	w->type = st_get_ubyte(&p);
	w->flags = st_get_ubyte(&p);
	w->state = st_get_ubyte(&p);
	w->trigger = st_get_byte(&p);
	w->clip_num = st_get_byte(&p);
	w->keys = st_get_ubyte(&p);
	w->pad = st_get_short(&p);
}

static void wall_to_disk(wall *w, ubyte *p)
{
	st_put_int(&p, w->segnum);
	st_put_int(&p, w->sidenum);
	st_put_int(&p, w->hps);
	st_put_int(&p, w->linked_wall);
	st_put_byte(&p, w->type);
	st_put_byte(&p, w->flags);
	st_put_byte(&p, w->state);
	st_put_byte(&p, w->trigger);
	st_put_byte(&p, w->clip_num);
	st_put_byte(&p, w->keys);
	st_put_short(&p, w->pad);
}

// One active door (16 bytes).
static void active_door_from_disk(active_door *d, const ubyte *p)
{
	d->n_parts = st_get_int(&p);
	d->front_wallnum[0] = st_get_short(&p);
	d->front_wallnum[1] = st_get_short(&p);
	d->back_wallnum[0] = st_get_short(&p);
	d->back_wallnum[1] = st_get_short(&p);
	d->time = st_get_int(&p);
}

static void active_door_to_disk(active_door *d, ubyte *p)
{
	st_put_int(&p, d->n_parts);
	st_put_short(&p, d->front_wallnum[0]);
	st_put_short(&p, d->front_wallnum[1]);
	st_put_short(&p, d->back_wallnum[0]);
	st_put_short(&p, d->back_wallnum[1]);
	st_put_int(&p, d->time);
}

// One trigger (54 bytes).
static void trigger_from_disk(trigger *t, const ubyte *p)
{
	int i;

	t->type = st_get_byte(&p);
	t->flags = st_get_short(&p);
	t->value = st_get_int(&p);
	t->time = st_get_int(&p);
	t->link_num = st_get_byte(&p);
	t->num_links = st_get_short(&p);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		t->seg[i] = st_get_short(&p);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		t->side[i] = st_get_short(&p);
}

static void trigger_to_disk(trigger *t, ubyte *p)
{
	int i;

	st_put_byte(&p, t->type);
	st_put_short(&p, t->flags);
	st_put_int(&p, t->value);
	st_put_int(&p, t->time);
	st_put_byte(&p, t->link_num);
	st_put_short(&p, t->num_links);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		st_put_short(&p, t->seg[i]);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		st_put_short(&p, t->side[i]);
}

// One robot maker (16 bytes).
static void matcen_info_from_disk(matcen_info *m, const ubyte *p)
{
	m->robot_flags = st_get_int(&p);
	m->hit_points = st_get_int(&p);
	m->interval = st_get_int(&p);
	m->segnum = st_get_short(&p);
	m->fuelcen_num = st_get_short(&p);
}

static void matcen_info_to_disk(matcen_info *m, ubyte *p)
{
	st_put_int(&p, m->robot_flags);
	st_put_int(&p, m->hit_points);
	st_put_int(&p, m->interval);
	st_put_short(&p, m->segnum);
	st_put_short(&p, m->fuelcen_num);
}

// The control center triggers (42 bytes).
static void cc_triggers_from_disk(control_center_triggers *t, const ubyte *p)
{
	int i;

	t->num_links = st_get_short(&p);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		t->seg[i] = st_get_short(&p);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		t->side[i] = st_get_short(&p);
}

static void cc_triggers_to_disk(control_center_triggers *t, ubyte *p)
{
	int i;

	st_put_short(&p, t->num_links);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		st_put_short(&p, t->seg[i]);
	for (i=0; i<MAX_WALLS_PER_LINK; i++)
		st_put_short(&p, t->side[i]);
}

// One fuel center (40 bytes).
static void fuelcen_from_disk(FuelCenter *f, const ubyte *p)
{
	f->Type = st_get_int(&p);
	f->segnum = st_get_int(&p);
	f->Flag = st_get_byte(&p);
	f->Enabled = st_get_byte(&p);
	f->Lives = st_get_byte(&p);
	f->dum1 = st_get_byte(&p);
	f->Capacity = st_get_int(&p);
	f->MaxCapacity = st_get_int(&p);
	f->Timer = st_get_int(&p);
	f->Disable_time = st_get_int(&p);
	st_get_vector(&p, &f->Center);
}

static void fuelcen_to_disk(FuelCenter *f, ubyte *p)
{
	st_put_int(&p, f->Type);
	st_put_int(&p, f->segnum);
	st_put_byte(&p, f->Flag);
	st_put_byte(&p, f->Enabled);
	st_put_byte(&p, f->Lives);
	st_put_byte(&p, f->dum1);
	st_put_int(&p, f->Capacity);
	st_put_int(&p, f->MaxCapacity);
	st_put_int(&p, f->Timer);
	st_put_int(&p, f->Disable_time);
	st_put_vector(&p, &f->Center);
}

// Reads n records of size bytes. Each record is converted and stored in
// the array, which has room for max records. The records after max are
// read and dropped, so that a bad count does not write outside the array
// (the original code did not test the counts). Returns the count of the
// records that are in the array.
#define ST_READ_ARRAY(fp, n, max, array, size, from_disk) do {	\
		ubyte st_buf[size];												\
		int st_i;															\
		for (st_i=0; st_i<(n); st_i++) {								\
			memset(st_buf, 0, size);									\
			fread(st_buf, size, 1, fp);								\
			if (st_i < (max))												\
				from_disk(&(array)[st_i], st_buf);					\
		}																		\
	} while (0)

#define ST_WRITE_ARRAY(fp, n, array, size, to_disk) do {			\
		ubyte st_buf[size];												\
		int st_i;															\
		for (st_i=0; st_i<(n); st_i++) {								\
			to_disk(&(array)[st_i], st_buf);							\
			fwrite(st_buf, size, 1, fp);								\
		}																		\
	} while (0)

// Limits a count that was read from the file to the size of its array.
static int st_limit_count(int n, int max)
{
	if (n < 0)
		return 0;
	if (n > max)
		return max;
	return n;
}

void state_callback(int nitems,newmenu_item * items, int * last_key, int citem)
{
	nitems = nitems;
	last_key = last_key;
	
//	if ( sc_last_item != citem )	{
//		sc_last_item = citem;
		if ( citem > 0 )	{
			if ( sc_bmp[citem-1] )	{
				gr_set_current_canvas( NULL );
				gr_bitmap( (grd_curcanv->cv_bitmap.bm_w-THUMBNAIL_W)/2,items[0].y-5, sc_bmp[citem-1] );
			}
		}
//	}	
}

void rpad_string( char * string, int max_chars )
{
	int i, end_found;

	end_found = 0;
	for( i=0; i<max_chars; i++ )	{
		if ( *string == 0 )
			end_found = 1;
		if ( end_found )
			*string = ' ';
		string++;
	}
	*string = 0;		// NULL terminate
}

int state_get_save_file(char * fname, char * dsc, int multi )
{
	FILE * fp;
	int i, choice, version;
	newmenu_item m[NUM_SAVES+1];
	char filename[NUM_SAVES][20];
	char desc[NUM_SAVES][DESC_LENGTH+16];
	char id[5];
	int valid=0;
	
	for (i=0;i<NUM_SAVES; i++ )	{
		sc_bmp[i] = NULL;
		if ( !multi )
			sprintf( filename[i], "%s.sg%d", Players[Player_num].callsign, i );
		else
			sprintf( filename[i], "%s.mg%d", Players[Player_num].callsign, i );
		valid = 0;
		fp = fopen( filename[i], "rb" );
		if ( fp ) {
			//Read id
			fread( id, sizeof(char)*4, 1, fp );
			if ( !memcmp( id, dgss_id, 4 )) {
				//Read version
				version = st_read_int( fp );		// ASCENT: little endian int
				if (version >= STATE_COMPATIBLE_VERSION)	{
					// Read description
					fread( desc[i], sizeof(char)*DESC_LENGTH, 1, fp );
					//rpad_string( desc[i], DESC_LENGTH-1 );
					// Read thumbnail
					//sc_bmp[i] = gr_create_bitmap(THUMBNAIL_W,THUMBNAIL_H );
					//fread( sc_bmp[i]->bm_data, THUMBNAIL_W * THUMBNAIL_H, 1, fp );
					valid = 1;
				}
			} 
			fclose(fp);
		}
		if (!valid) {
			strcpy( desc[i], TXT_EMPTY );
			//rpad_string( desc[i], DESC_LENGTH-1 );
		}
		m[i].type = NM_TYPE_INPUT_MENU; m[i].text = desc[i]; m[i].text_len = DESC_LENGTH-1;
	}

	sc_last_item = -1;
	choice = newmenu_do1( NULL, "Save Game", NUM_SAVES, m, NULL, state_default_item );

	for (i=0; i<NUM_SAVES; i++ )	{
		if ( sc_bmp[i] )
			gr_free_bitmap( sc_bmp[i] );
	}

	if (choice > -1) {
		strcpy( fname, filename[choice] );
		strcpy( dsc, desc[choice] );
		state_default_item = choice;
		return choice+1;
	}
	return 0;
}

int state_get_restore_file(char * fname, int multi )
{
	FILE * fp;
	int i, choice, version, nsaves;
	newmenu_item m[NUM_SAVES+1];
	char filename[NUM_SAVES][20];
	char desc[NUM_SAVES][DESC_LENGTH + 16];
	char id[5];
	int valid;

	nsaves=0;
	m[0].type = NM_TYPE_TEXT; m[0].text = "\n\n\n\n";	
	for (i=0;i<NUM_SAVES; i++ )	{
		sc_bmp[i] = NULL;
		if (!multi)
			sprintf( filename[i], "%s.sg%d", Players[Player_num].callsign, i );
		else
			sprintf( filename[i], "%s.mg%d", Players[Player_num].callsign, i );
		valid = 0;
		fp = fopen( filename[i], "rb" );
		if ( fp ) {
			//Read id
			fread( id, sizeof(char)*4, 1, fp );
			if ( !memcmp( id, dgss_id, 4 )) {
				//Read version
				version = st_read_int( fp );		// ASCENT: little endian int
				if (version >= STATE_COMPATIBLE_VERSION)	{
					// Read description
					fread( desc[i], sizeof(char)*DESC_LENGTH, 1, fp );
					//rpad_string( desc[i], DESC_LENGTH-1 );
					m[i+1].type = NM_TYPE_MENU; m[i+1].text = desc[i];;
					// Read thumbnail
					sc_bmp[i] = gr_create_bitmap(THUMBNAIL_W,THUMBNAIL_H );
					fread( sc_bmp[i]->bm_data, THUMBNAIL_W * THUMBNAIL_H, 1, fp );
					nsaves++;
					valid = 1;
				} 
			}
			fclose(fp);
		} 
		if (!valid) {
			strcpy( desc[i], TXT_EMPTY );
			//rpad_string( desc[i], DESC_LENGTH-1 );
			m[i+1].type = NM_TYPE_TEXT; m[i+1].text = desc[i];
		}
	}

	if ( nsaves < 1 )	{
		nm_messagebox( NULL, 1, "Ok", "No saved games were found!" );
		return 0;
	}

	sc_last_item = -1;
	choice = newmenu_do3( NULL, "Select Game to Restore", NUM_SAVES+1, m, state_callback, state_default_item+1, NULL, 190, -1 );

	for (i=0; i<NUM_SAVES; i++ )	{
		if ( sc_bmp[i] )
			gr_free_bitmap( sc_bmp[i] );
	}

	if (choice > 0) {
		strcpy( fname, filename[choice-1] );
		state_default_item = choice - 1;
		return choice;
	}
	return 0;
}

int state_save_old_game(int slotnum, char * sg_name, player * sg_player, 
                        int sg_difficulty_level, int sg_primary_weapon, 
                        int sg_secondary_weapon, int sg_next_level_num  	)
{
	int i;
	int temp_int;
	ubyte temp_byte;
	char desc[DESC_LENGTH+1];
	char filename[128];
	grs_canvas * cnv;
	FILE * fp;

	sprintf( filename, "%s.sg%d", sg_player->callsign, slotnum );
	fp = fopen( filename, "wb" );
	if ( !fp ) return 0;

//Save id
	fwrite( dgss_id, sizeof(char)*4, 1, fp );

// ASCENT: in this function each fwrite of an int is st_write_int (little
// endian), and the player structure is written field by field.

//Save version
	temp_int = STATE_VERSION;
	st_write_int( fp, temp_int );

//Save description
	// ASCENT: desc is cleared, so that no byte of the stack goes to the file
	memset( desc, 0, sizeof(desc) );
	strncpy( desc, sg_name, DESC_LENGTH );
	fwrite( desc, sizeof(char)*DESC_LENGTH, 1, fp );
	
// Save the current screen shot...
	cnv = gr_create_canvas( THUMBNAIL_W, THUMBNAIL_H );
	if ( cnv )	{
		char * pcx_file;
		ubyte pcx_palette[768];
		grs_bitmap bmp;

		gr_set_current_canvas( cnv );

		gr_clear_canvas( BM_XRGB(0,0,0) );
		pcx_file = get_briefing_screen( sg_next_level_num );
		if ( pcx_file != NULL )	{
			bmp.bm_data = NULL;
			if (pcx_read_bitmap( pcx_file, &bmp, BM_LINEAR, pcx_palette )==PCX_ERROR_NONE)	{
				grs_point vertbuf[3];
				gr_clear_canvas( 255 );
				vertbuf[0].x = vertbuf[0].y = -F1_0*6;		// -6 pixel rows for ascpect
				vertbuf[1].x = vertbuf[1].y = 0;
				vertbuf[2].x = i2f(THUMBNAIL_W); vertbuf[2].y = i2f(THUMBNAIL_H+7);	// + 7 pixel rows for ascpect
				scale_bitmap(&bmp, vertbuf );
				gr_remap_bitmap_good( &cnv->cv_bitmap, pcx_palette, -1, -1 );
				free( bmp.bm_data );
			}
		}
		fwrite( cnv->cv_bitmap.bm_data, THUMBNAIL_W*THUMBNAIL_H, 1, fp );
		gr_free_canvas( cnv );
	} else {
	 	ubyte color = 0;
	 	for ( i=0; i<THUMBNAIL_W*THUMBNAIL_H; i++ )
	 		fwrite( &color, sizeof(ubyte), 1, fp );		
	}

// Save the Between levels flag...
	temp_int = 1;
	st_write_int( fp, temp_int );

// Save the mission info...
	fwrite( Mission_list[0].filename, sizeof(char)*9, 1, fp );

//Save level info
	temp_int = sg_player->level;
	st_write_int( fp, temp_int );
	temp_int = sg_next_level_num;
	st_write_int( fp, temp_int );

//Save GameTime
	temp_int = 0;
	st_write_int( fp, temp_int );

//Save player info
	st_write_player( fp, sg_player );

// Save the current weapon info
	temp_byte = sg_primary_weapon;
	fwrite( &temp_byte, sizeof(byte), 1, fp );
	temp_byte = sg_secondary_weapon;
	fwrite( &temp_byte, sizeof(byte), 1, fp );

// Save the difficulty level
	temp_int = sg_difficulty_level;
	st_write_int( fp, temp_int );

// Save the Cheats_enabled
	temp_int = 0;
	st_write_int( fp, temp_int );
	temp_int = 0;		// turbo mode
	st_write_int( fp, temp_int );

	st_write_int( fp, state_game_id );
	st_write_int( fp, Laser_rapid_fire );
	st_write_int( fp, Ugly_robot_cheat );
	st_write_int( fp, Ugly_robot_texture );
	st_write_int( fp, Physics_cheat_flag );
	st_write_int( fp, Lunacy );

	fclose(fp);

	return 1;
}


int state_save_all(int between_levels)
{
	char filename[128], desc[DESC_LENGTH+1];

	if ( Game_mode & GM_MULTI )	{
#ifdef MULTI_SAVE
		if ( FindArg( "-multisave" ) )
			multi_initiate_save_game();
		else
#endif  
			HUD_init_message( "Can't save in a multiplayer game!" );
		return 0;
	}

	mprintf(( 0, "CL=%d, NL=%d\n", Current_level_num, Next_level_num ));
	
	stop_time();

	if (!state_get_save_file(filename,desc,0))	{
		start_time();
		return 0;
	}
		
	return state_save_all_sub(filename, desc, between_levels);
}

int state_save_all_sub(char *filename, char *desc, int between_levels)
{
	int i,j;
	FILE * fp;
	grs_canvas * cnv;

	if ( Game_mode & GM_MULTI )	{
#ifdef MULTI_SAVE
		if ( !FindArg( "-multisave" ) ) 
#endif  
			return 0;
	}

	fp = fopen( filename, "wb" );
	if ( !fp ) {
		start_time();
		return 0;
	}

//Save id
	fwrite( dgss_id, sizeof(char)*4, 1, fp );

// ASCENT: in this function each fwrite of an int is st_write_int (little
// endian), and each fwrite of a structure or of an array of structures
// writes the fields one by one (see the functions at the top of the file).

//Save version
	i = STATE_VERSION;
	st_write_int( fp, i );

//Save description
	fwrite( desc, sizeof(char)*DESC_LENGTH, 1, fp );
	
// Save the current screen shot...
	cnv = gr_create_canvas( THUMBNAIL_W, THUMBNAIL_H );
	if ( cnv )	{
		gr_set_current_canvas( cnv );
		if ( between_levels )	{
			char * pcx_file;
			ubyte pcx_palette[768];
			grs_bitmap bmp;

			gr_clear_canvas( BM_XRGB(0,0,0) );
			pcx_file = get_briefing_screen( Next_level_num );
			if ( pcx_file != NULL )	{
				bmp.bm_data = NULL;
				if (pcx_read_bitmap( pcx_file, &bmp, BM_LINEAR, pcx_palette )==PCX_ERROR_NONE)	{
					grs_point vertbuf[3];
					gr_clear_canvas( 255 );
					vertbuf[0].x = vertbuf[0].y = -F1_0*6;		// -6 pixel rows for ascpect
					vertbuf[1].x = vertbuf[1].y = 0;
					vertbuf[2].x = i2f(THUMBNAIL_W); vertbuf[2].y = i2f(THUMBNAIL_H+7);	// + 7 pixel rows for ascpect
					scale_bitmap(&bmp, vertbuf );
					gr_remap_bitmap_good( &cnv->cv_bitmap, pcx_palette, -1, -1 );
					free( bmp.bm_data );
				}
			}
		} else {
			render_frame(0);
		}
		fwrite( cnv->cv_bitmap.bm_data, THUMBNAIL_W*THUMBNAIL_H, 1, fp );
		gr_free_canvas( cnv );
	} else {
	 	ubyte color = 0;
	 	for ( i=0; i<THUMBNAIL_W*THUMBNAIL_H; i++ )
	 		fwrite( &color, sizeof(ubyte), 1, fp );		
	}

// Save the Between levels flag...
	st_write_int( fp, between_levels );

// Save the mission info...
	fwrite( Mission_list[Current_mission_num].filename, sizeof(char)*9, 1, fp );

//Save level info
	st_write_int( fp, Current_level_num );
	st_write_int( fp, Next_level_num );

//Save GameTime
	st_write_int( fp, GameTime );

//Save player info
	st_write_player( fp, &Players[Player_num] );

// Save the current weapon info
	fwrite( &Primary_weapon, sizeof(byte), 1, fp );
	fwrite( &Secondary_weapon, sizeof(byte), 1, fp );

// Save the difficulty level
	st_write_int( fp, Difficulty_level );

// Save the Cheats_enabled
	st_write_int( fp, Cheats_enabled );
	st_write_int( fp, Game_turbo_mode );


	if ( !between_levels )	{

	//Finish all morph objects
		for (i=0; i<=Highest_object_index; i++ )	{
			if ( (Objects[i].type != OBJ_NONE) && (Objects[i].render_type==RT_MORPH))	{
				morph_data *md;
				md = find_morph_data(&Objects[i]);
				if (md) {					
					md->obj->control_type = md->morph_save_control_type;
					md->obj->movement_type = md->morph_save_movement_type;
					md->obj->render_type = RT_POLYOBJ;
					md->obj->mtype.phys_info = md->morph_save_phys_info;
					md->obj = NULL;
				} else {						//maybe loaded half-morphed from disk
					Objects[i].flags |= OF_SHOULD_BE_DEAD;
					Objects[i].render_type = RT_POLYOBJ;
					Objects[i].control_type = CT_NONE;
					Objects[i].movement_type = MT_NONE;
				}
			}
		}
	
	//Save object info
		i = Highest_object_index+1;
		st_write_int( fp, i );
		ST_WRITE_ARRAY( fp, i, Objects, OBJECT_DISK_SIZE, object_to_disk );

	//Save wall info
		i = Num_walls;
		st_write_int( fp, i );
		ST_WRITE_ARRAY( fp, i, Walls, WALL_DISK_SIZE, wall_to_disk );

	//Save door info
		i = Num_open_doors;
		st_write_int( fp, i );
		ST_WRITE_ARRAY( fp, i, ActiveDoors, ACTIVE_DOOR_DISK_SIZE, active_door_to_disk );

	//Save trigger info
		st_write_int( fp, Num_triggers );
		ST_WRITE_ARRAY( fp, Num_triggers, Triggers, TRIGGER_DISK_SIZE, trigger_to_disk );

	//Save tmap info
		for (i=0; i<=Highest_segment_index; i++ )	{
			for (j=0; j<6; j++ )	{
				ubyte side_buf[6], *sp = side_buf;
				st_put_short( &sp, Segments[i].sides[j].wall_num );
				st_put_short( &sp, Segments[i].sides[j].tmap_num );
				st_put_short( &sp, Segments[i].sides[j].tmap_num2 );
				fwrite( side_buf, 6, 1, fp );
			}
		}

	// Save the fuelcen info
		st_write_int( fp, Fuelcen_control_center_destroyed );
		st_write_int( fp, Fuelcen_seconds_left );
		st_write_int( fp, Num_robot_centers );
		ST_WRITE_ARRAY( fp, Num_robot_centers, RobotCenters, MATCEN_INFO_DISK_SIZE, matcen_info_to_disk );
		ST_WRITE_ARRAY( fp, 1, &ControlCenterTriggers, CC_TRIGGERS_DISK_SIZE, cc_triggers_to_disk );
		st_write_int( fp, Num_fuelcenters );
		ST_WRITE_ARRAY( fp, Num_fuelcenters, Station, FUELCEN_DISK_SIZE, fuelcen_to_disk );

	// Save the control cen info
		st_write_int( fp, Control_center_been_hit );
		st_write_int( fp, Control_center_player_been_seen );
		st_write_int( fp, Control_center_next_fire_time );
		st_write_int( fp, Control_center_present );
		st_write_int( fp, Dead_controlcen_object_num );

	// Save the AI state
		ai_save_state( fp );

	// Save the automap visited info
		fwrite( Automap_visited, sizeof(ubyte)*MAX_SEGMENTS, 1, fp );
	}
	st_write_int( fp, state_game_id );
	st_write_int( fp, Laser_rapid_fire );
	st_write_int( fp, Ugly_robot_cheat );
	st_write_int( fp, Ugly_robot_texture );
	st_write_int( fp, Physics_cheat_flag );
	st_write_int( fp, Lunacy );

	fclose(fp);

	start_time();

	return 1;
}

int state_restore_all(int in_game)
{
	char filename[128];

	if ( Game_mode & GM_MULTI )	{
#ifdef MULTI_SAVE
		if ( FindArg( "-multisave" ) )
			multi_initiate_restore_game();
		else	
#endif
			HUD_init_message( "Can't restore in a multiplayer game!" );
		return 0;
	}

	if ( Newdemo_state == ND_STATE_RECORDING )
		newdemo_stop_recording();

	if ( Newdemo_state != ND_STATE_NORMAL )
		return 0;

	stop_time();
	if (!state_get_restore_file(filename,0))	{
		start_time();
		return 0;
	}

	if ( in_game )		{
		int choice;
		choice =  nm_messagebox( NULL, 2, "Yes", "No", "Restore Game?" );
		if ( choice != 0 )	{
			start_time();
			return 0;
		}
	}

	start_time();

	return state_restore_all_sub(filename, 0);
}

int state_restore_all_sub(char *filename, int multi)
{
	int ObjectStartLocation;
	int BogusSaturnShit = 0;
	int version,i, j, segnum;
	object * obj;
	FILE *fp;
	int current_level, next_level;
	int between_levels;
	char mission[16];
	char desc[DESC_LENGTH+1];
	char id[5];
	char org_callsign[CALLSIGN_LEN+16];

	if ( Game_mode & GM_MULTI )	{
#ifdef MULTI_SAVE
		if ( !FindArg( "-multisave" ) ) 
#endif
			return 0;
	}

	fp = fopen( filename, "rb" );
	if ( !fp ) return 0;

//Read id
	fread( id, sizeof(char)*4, 1, fp );
	if ( memcmp( id, dgss_id, 4 )) {
		fclose(fp);
		return 0;
	}

// ASCENT: in this function each fread of an int is st_read_int (little
// endian), and each fread of a structure or of an array of structures
// reads the fields one by one (see the functions at the top of the file).

//Read version
	version = st_read_int( fp );
	if (version < STATE_COMPATIBLE_VERSION)	{
		fclose(fp);
		return 0;
	}

// Read description
	fread( desc, sizeof(char)*DESC_LENGTH, 1, fp );

// Skip the current screen shot...
	fseek( fp, THUMBNAIL_W*THUMBNAIL_H, SEEK_CUR );

// Read the Between levels flag...
	between_levels = st_read_int( fp );

// Read the mission info...
	fread( mission, sizeof(char)*9, 1, fp );
	mission[9] = 0;		// ASCENT: the name always ends

	if (!load_mission_by_name( mission ))	{
		nm_messagebox( NULL, 1, "Ok", "Error!\nUnable to load mission\n'%s'\n", mission );
		fclose(fp);
		return 0;
	}

//Read level info
	current_level = st_read_int( fp );
	next_level = st_read_int( fp );

//Restore GameTime
	GameTime = st_read_int( fp );

// Start new game....
	if (!multi)	{
		Game_mode = GM_NORMAL;
		Function_mode = FMODE_GAME;
		#ifdef NETWORK
		change_playernum_to(0);
		#endif
		strcpy( org_callsign, Players[0].callsign );
		N_players = 1;
		InitPlayerObject();				//make sure player's object set up
		init_player_stats_game();		//clear all stats
	} else {
		strcpy( org_callsign, Players[Player_num].callsign );
	}
//Read player info

	if ( between_levels )	{
		int saved_offset;
		st_read_player( fp, &Players[Player_num] );
		saved_offset = ftell(fp);
		fclose( fp );
		do_briefing_screens(next_level);
		fp = fopen( filename, "rb" );
		fseek( fp, saved_offset, SEEK_SET );
 		StartNewLevelSub( next_level, 0 );		
	} else {
		StartNewLevelSub(current_level, 0);
		st_read_player( fp, &Players[Player_num] );
	}
	strcpy( Players[Player_num].callsign, org_callsign );

// Set the right level
	if ( between_levels )
		Players[Player_num].level = next_level;

// Restore the weapon states
	fread( &Primary_weapon, sizeof(byte), 1, fp );
	fread( &Secondary_weapon, sizeof(byte), 1, fp );

	select_weapon(Primary_weapon, 0, 0, 0);
	select_weapon(Secondary_weapon, 1, 0, 0);

// Restore the difficulty level
	Difficulty_level = st_read_int( fp );

// Restore the cheats enabled flag
	Cheats_enabled = st_read_int( fp );
	Game_turbo_mode = st_read_int( fp );

	if ( !between_levels )	{
		Do_appearance_effect = 0;			// Don't do this for middle o' game stuff.

		ObjectStartLocation = ftell( fp );
RetryObjectLoading:
		//Clear out all the objects from the lvl file
		for (segnum=0; segnum <= Highest_segment_index; segnum++)
			Segments[segnum].objects = -1;
		reset_objects(1);
	
		//Read objects, and pop 'em into their respective segments.
		// ASCENT: the objects are read one by one and converted from the
		// 264 bytes of the file. The count is limited to the array (the
		// objects after it are read and dropped).
		i = st_read_int( fp );
		Highest_object_index = st_limit_count( i, MAX_OBJECTS ) - 1;
		if ( !BogusSaturnShit )
			ST_READ_ARRAY( fp, i, MAX_OBJECTS, Objects, OBJECT_DISK_SIZE, object_from_disk );
		else {
			ubyte tmp_object[OBJECT_DISK_SIZE];
			int n_objects = i;
			for (i=0; i<n_objects; i++ )	{
				memset( tmp_object, 0, OBJECT_DISK_SIZE );
				fread( tmp_object, OBJECT_DISK_SIZE-3, 1, fp );
				// Insert 3 bytes after the read in obj->rtype.pobj_info.alt_textures field.
				if ( i < MAX_OBJECTS )	{
					object_from_disk( &Objects[i], tmp_object );
					Objects[i].rtype.pobj_info.alt_textures = -1;
				}
			}
		}
	
		Object_next_signature = 0;
		for (i=0; i<=Highest_object_index; i++ )	{
			obj = &Objects[i];
			obj->rtype.pobj_info.alt_textures = -1;
			segnum = obj->segnum;
			obj->next = obj->prev = obj->segnum = -1;
			if ( obj->type != OBJ_NONE )	{
				// Check for a bogus Saturn version!!!!
				if (!BogusSaturnShit )	{
					if ( (segnum<0) || (segnum>Highest_segment_index) ) {
						BogusSaturnShit = 1;
						mprintf(( 1, "READING BOGUS SATURN VERSION OBJECTS!!! (Object:%d)\n", i ));
						fseek( fp, ObjectStartLocation, SEEK_SET );
						goto RetryObjectLoading;
					}
				}
				obj_link(i,segnum);
				if ( obj->signature > Object_next_signature )
					Object_next_signature = obj->signature;
			}
		}	
		special_reset_objects();
		Object_next_signature++;
	
		//Restore wall info
		i = st_read_int( fp );
		Num_walls = i;
		// Check for a bogus Saturn version!!!!
		if (!BogusSaturnShit )	{
			if ( (Num_walls<0) || (Num_walls>MAX_WALLS) ) {
				BogusSaturnShit = 1;
				mprintf(( 1, "READING BOGUS SATURN VERSION OBJECTS!!! (Num_walls)\n" ));
				fseek( fp, ObjectStartLocation, SEEK_SET );
				goto RetryObjectLoading;
			}
		}

		ST_READ_ARRAY( fp, Num_walls, MAX_WALLS, Walls, WALL_DISK_SIZE, wall_from_disk );
		Num_walls = st_limit_count( Num_walls, MAX_WALLS );		// ASCENT: see ST_READ_ARRAY
		// Check for a bogus Saturn version!!!!
		if (!BogusSaturnShit )	{
			for (i=0; i<Num_walls; i++ )	{
				if ( (Walls[i].segnum<0) || (Walls[i].segnum>Highest_segment_index) || (Walls[i].sidenum<-1) || (Walls[i].sidenum>5) ) {
					BogusSaturnShit = 1;
					mprintf(( 1, "READING BOGUS SATURN VERSION OBJECTS!!! (Wall %d)\n", i ));
					fseek( fp, ObjectStartLocation, SEEK_SET );
					goto RetryObjectLoading;
				}
			}
		}
	
		//Restore door info
		i = st_read_int( fp );
		Num_open_doors = i;
		ST_READ_ARRAY( fp, Num_open_doors, MAX_DOORS, ActiveDoors, ACTIVE_DOOR_DISK_SIZE, active_door_from_disk );
		Num_open_doors = st_limit_count( Num_open_doors, MAX_DOORS );		// ASCENT: see ST_READ_ARRAY

		//Restore trigger info
		Num_triggers = st_read_int( fp );
		ST_READ_ARRAY( fp, Num_triggers, MAX_TRIGGERS, Triggers, TRIGGER_DISK_SIZE, trigger_from_disk );
		Num_triggers = st_limit_count( Num_triggers, MAX_TRIGGERS );		// ASCENT: see ST_READ_ARRAY

		//Restore tmap info
		for (i=0; i<=Highest_segment_index; i++ )	{
			for (j=0; j<6; j++ )	{
				ubyte side_buf[6];
				const ubyte *sp = side_buf;
				// ASCENT: a short read keeps the values of the level
				if ( fread( side_buf, 6, 1, fp ) != 1 )
					continue;
				Segments[i].sides[j].wall_num = st_get_short( &sp );
				Segments[i].sides[j].tmap_num = st_get_short( &sp );
				Segments[i].sides[j].tmap_num2 = st_get_short( &sp );
			}
		}

		//Restore the fuelcen info
		Fuelcen_control_center_destroyed = st_read_int( fp );
		Fuelcen_seconds_left = st_read_int( fp );
		Num_robot_centers = st_read_int( fp );
		ST_READ_ARRAY( fp, Num_robot_centers, MAX_ROBOT_CENTERS, RobotCenters, MATCEN_INFO_DISK_SIZE, matcen_info_from_disk );
		Num_robot_centers = st_limit_count( Num_robot_centers, MAX_ROBOT_CENTERS );		// ASCENT: see ST_READ_ARRAY
		ST_READ_ARRAY( fp, 1, 1, &ControlCenterTriggers, CC_TRIGGERS_DISK_SIZE, cc_triggers_from_disk );
		Num_fuelcenters = st_read_int( fp );
		ST_READ_ARRAY( fp, Num_fuelcenters, MAX_NUM_FUELCENS, Station, FUELCEN_DISK_SIZE, fuelcen_from_disk );
		Num_fuelcenters = st_limit_count( Num_fuelcenters, MAX_NUM_FUELCENS );		// ASCENT: see ST_READ_ARRAY

		// Restore the control cen info
		Control_center_been_hit = st_read_int( fp );
		Control_center_player_been_seen = st_read_int( fp );
		Control_center_next_fire_time = st_read_int( fp );
		Control_center_present = st_read_int( fp );
		Dead_controlcen_object_num = st_read_int( fp );
	
		// Restore the AI state
		ai_restore_state( fp );
	
		// Restore the automap visited info
		fread( Automap_visited, sizeof(ubyte)*MAX_SEGMENTS, 1, fp );

		//	Restore hacked up weapon system stuff.
		Fusion_next_sound_time = GameTime;
		Auto_fire_fusion_cannon_time = 0;
		Next_laser_fire_time = GameTime;
		Next_missile_fire_time = GameTime;
		Last_laser_fired_time = GameTime;

	}
	state_game_id = 0;

	if ( version >= 7 )	{
		int tmp_Lunacy;
		state_game_id = st_read_int( fp );
		Laser_rapid_fire = st_read_int( fp );
		Ugly_robot_cheat = st_read_int( fp );
		Ugly_robot_texture = st_read_int( fp );
		Physics_cheat_flag = st_read_int( fp );
		tmp_Lunacy = st_read_int( fp );
		if ( tmp_Lunacy )
			do_lunacy_on();
	}

	fclose(fp);

// Load in bitmaps, etc..
	piggy_load_level_data();

	return 1;
}


#endif
