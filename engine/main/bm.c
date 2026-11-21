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

ubyte Sounds[MAX_SOUNDS];
ubyte AltSounds[MAX_SOUNDS];

int Num_total_object_types;

byte	ObjType[MAX_OBJTYPE];
byte	ObjId[MAX_OBJTYPE];
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

//-----------------------------------------------------------------
// Initializes all bitmaps from BITMAPS.TBL file.
int bm_init()
{
	init_polygon_models();
	piggy_init();				// This calls bm_read_all
	piggy_read_sounds();
	return 0;
}

// ASCENT: the original bm_read_all() read each table with one cfread() of the
// whole array of structures. The structures had 1 byte packing and polymodel
// has a pointer, so the layout in memory is different now. Each table has a
// function that reads one record from its bytes on disk, one field at a time,
// little endian. The sizes on disk (<NAME>_DISK_SIZE) are in bm.h. The PIG
// file has no size fields for these tables. Each function checks that it
// used the number of bytes that the constant gives.

static bitmap_index dio_get_bitmap_index(const ubyte **pp)
{
	bitmap_index bi;
	bi.index = dio_get_ushort(pp);
	return bi;
}

void tmap_info_from_disk(tmap_info *t, const ubyte *buf)
{
	const ubyte *p = buf;

	dio_get_bytes(&p, t->filename, 13);
	t->flags = dio_get_ubyte(&p);
	t->lighting = dio_get_fix(&p);
	t->damage = dio_get_fix(&p);
	t->eclip_num = dio_get_int(&p);
	Assert(p - buf == TMAP_INFO_DISK_SIZE);
}

static void vclip_get(vclip *vc, const ubyte **pp)
{
	int i;

	vc->play_time = dio_get_fix(pp);
	vc->num_frames = dio_get_int(pp);
	vc->frame_time = dio_get_fix(pp);
	vc->flags = dio_get_int(pp);
	vc->sound_num = dio_get_short(pp);
	for (i=0; i<VCLIP_MAX_FRAMES; i++)
		vc->frames[i] = dio_get_bitmap_index(pp);
	vc->light_value = dio_get_fix(pp);
}

void vclip_from_disk(vclip *vc, const ubyte *buf)
{
	const ubyte *p = buf;

	vclip_get(vc, &p);
	Assert(p - buf == VCLIP_DISK_SIZE);
}

void eclip_from_disk(eclip *ec, const ubyte *buf)
{
	const ubyte *p = buf;

	vclip_get(&ec->vc, &p);
	ec->time_left = dio_get_fix(&p);
	ec->frame_count = dio_get_int(&p);
	ec->changing_wall_texture = dio_get_short(&p);
	ec->changing_object_texture = dio_get_short(&p);
	ec->flags = dio_get_int(&p);
	ec->crit_clip = dio_get_int(&p);
	ec->dest_bm_num = dio_get_int(&p);
	ec->dest_vclip = dio_get_int(&p);
	ec->dest_eclip = dio_get_int(&p);
	ec->dest_size = dio_get_fix(&p);
	ec->sound_num = dio_get_int(&p);
	ec->segnum = dio_get_int(&p);
	ec->sidenum = dio_get_int(&p);
	Assert(p - buf == ECLIP_DISK_SIZE);
}

void wclip_from_disk(wclip *wc, const ubyte *buf)
{
	const ubyte *p = buf;
	int i;

	wc->play_time = dio_get_fix(&p);
	wc->num_frames = dio_get_short(&p);
	for (i=0; i<MAX_CLIP_FRAMES; i++)
		wc->frames[i] = dio_get_short(&p);
	wc->open_sound = dio_get_short(&p);
	wc->close_sound = dio_get_short(&p);
	wc->flags = dio_get_short(&p);
	dio_get_bytes(&p, wc->filename, 13);
	wc->pad = dio_get_ubyte(&p);
	Assert(p - buf == WCLIP_DISK_SIZE);
}

void robot_info_from_disk(robot_info *ri, const ubyte *buf)
{
	const ubyte *p = buf;
	int i, j;

	ri->model_num = dio_get_int(&p);
	ri->n_guns = dio_get_int(&p);
	for (i=0; i<MAX_GUNS; i++)
		dio_get_vector(&p, &ri->gun_points[i]);
	for (i=0; i<MAX_GUNS; i++)
		ri->gun_submodels[i] = dio_get_ubyte(&p);
	ri->exp1_vclip_num = dio_get_short(&p);
	ri->exp1_sound_num = dio_get_short(&p);
	ri->exp2_vclip_num = dio_get_short(&p);
	ri->exp2_sound_num = dio_get_short(&p);
	ri->weapon_type = dio_get_short(&p);
	ri->contains_id = dio_get_byte(&p);
	ri->contains_count = dio_get_byte(&p);
	ri->contains_prob = dio_get_byte(&p);
	ri->contains_type = dio_get_byte(&p);
	ri->score_value = dio_get_int(&p);
	ri->lighting = dio_get_fix(&p);
	ri->strength = dio_get_fix(&p);
	ri->mass = dio_get_fix(&p);
	ri->drag = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->field_of_view[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->firing_wait[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->turn_time[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->fire_power[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->shield[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->max_speed[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->circle_distance[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) ri->rapidfire_count[i] = dio_get_byte(&p);
	for (i=0; i<NDL; i++) ri->evade_speed[i] = dio_get_byte(&p);
	ri->cloak_type = dio_get_byte(&p);
	ri->attack_type = dio_get_byte(&p);
	ri->boss_flag = dio_get_byte(&p);
	ri->see_sound = dio_get_ubyte(&p);
	ri->attack_sound = dio_get_ubyte(&p);
	ri->claw_sound = dio_get_ubyte(&p);
	for (i=0; i<MAX_GUNS+1; i++)
		for (j=0; j<N_ANIM_STATES; j++) {
			ri->anim_states[i][j].n_joints = dio_get_short(&p);
			ri->anim_states[i][j].offset = dio_get_short(&p);
		}
	ri->always_0xabcd = dio_get_int(&p);
	Assert(p - buf == ROBOT_INFO_DISK_SIZE);
}

void jointpos_from_disk(jointpos *jp, const ubyte *buf)
{
	const ubyte *p = buf;

	jp->jointnum = dio_get_short(&p);
	dio_get_angvec(&p, &jp->angles);
	Assert(p - buf == JOINTPOS_DISK_SIZE);
}

void weapon_info_from_disk(weapon_info *wi, const ubyte *buf)
{
	const ubyte *p = buf;
	int i;

	wi->render_type = dio_get_byte(&p);
	wi->model_num = dio_get_byte(&p);
	wi->model_num_inner = dio_get_byte(&p);
	wi->persistent = dio_get_byte(&p);
	wi->flash_vclip = dio_get_byte(&p);
	wi->flash_sound = dio_get_short(&p);
	wi->robot_hit_vclip = dio_get_byte(&p);
	wi->robot_hit_sound = dio_get_short(&p);
	wi->wall_hit_vclip = dio_get_byte(&p);
	wi->wall_hit_sound = dio_get_short(&p);
	wi->fire_count = dio_get_byte(&p);
	wi->ammo_usage = dio_get_byte(&p);
	wi->weapon_vclip = dio_get_byte(&p);
	wi->destroyable = dio_get_byte(&p);
	wi->matter = dio_get_byte(&p);
	wi->bounce = dio_get_byte(&p);
	wi->homing_flag = dio_get_byte(&p);
	wi->dum1 = dio_get_byte(&p);
	wi->dum2 = dio_get_byte(&p);
	wi->dum3 = dio_get_byte(&p);
	wi->energy_usage = dio_get_fix(&p);
	wi->fire_wait = dio_get_fix(&p);
	wi->bitmap = dio_get_bitmap_index(&p);
	wi->blob_size = dio_get_fix(&p);
	wi->flash_size = dio_get_fix(&p);
	wi->impact_size = dio_get_fix(&p);
	for (i=0; i<NDL; i++) wi->strength[i] = dio_get_fix(&p);
	for (i=0; i<NDL; i++) wi->speed[i] = dio_get_fix(&p);
	wi->mass = dio_get_fix(&p);
	wi->drag = dio_get_fix(&p);
	wi->thrust = dio_get_fix(&p);
	wi->po_len_to_width_ratio = dio_get_fix(&p);
	wi->light = dio_get_fix(&p);
	wi->lifetime = dio_get_fix(&p);
	wi->damage_radius = dio_get_fix(&p);
	wi->picture = dio_get_bitmap_index(&p);
	Assert(p - buf == WEAPON_INFO_DISK_SIZE);
}

void powerup_type_info_from_disk(powerup_type_info *pi, const ubyte *buf)
{
	const ubyte *p = buf;

	pi->vclip_num = dio_get_int(&p);
	pi->hit_sound = dio_get_int(&p);
	pi->size = dio_get_fix(&p);
	pi->light = dio_get_fix(&p);
	Assert(p - buf == POWERUP_TYPE_INFO_DISK_SIZE);
}

// The model_data field on disk is a 32-bit pointer of the program that wrote
// the file. It has no meaning. The function sets model_data to NULL.
void polymodel_from_disk(polymodel *pm, const ubyte *buf)
{
	const ubyte *p = buf;
	int i;

	pm->n_models = dio_get_int(&p);
	pm->model_data_size = dio_get_int(&p);
	p += 4;
	pm->model_data = NULL;
	for (i=0; i<MAX_SUBMODELS; i++) pm->submodel_ptrs[i] = dio_get_int(&p);
	for (i=0; i<MAX_SUBMODELS; i++) dio_get_vector(&p, &pm->submodel_offsets[i]);
	for (i=0; i<MAX_SUBMODELS; i++) dio_get_vector(&p, &pm->submodel_norms[i]);
	for (i=0; i<MAX_SUBMODELS; i++) dio_get_vector(&p, &pm->submodel_pnts[i]);
	for (i=0; i<MAX_SUBMODELS; i++) pm->submodel_rads[i] = dio_get_fix(&p);
	for (i=0; i<MAX_SUBMODELS; i++) pm->submodel_parents[i] = dio_get_ubyte(&p);
	for (i=0; i<MAX_SUBMODELS; i++) dio_get_vector(&p, &pm->submodel_mins[i]);
	for (i=0; i<MAX_SUBMODELS; i++) dio_get_vector(&p, &pm->submodel_maxs[i]);
	dio_get_vector(&p, &pm->mins);
	dio_get_vector(&p, &pm->maxs);
	pm->rad = dio_get_fix(&p);
	pm->n_textures = dio_get_ubyte(&p);
	pm->first_texture = dio_get_ushort(&p);
	pm->simpler_model = dio_get_ubyte(&p);
	Assert(p - buf == POLYMODEL_DISK_SIZE);
}

void player_ship_from_disk(player_ship *ps, const ubyte *buf)
{
	const ubyte *p = buf;
	int i;

	ps->model_num = dio_get_int(&p);
	ps->expl_vclip_num = dio_get_int(&p);
	ps->mass = dio_get_fix(&p);
	ps->drag = dio_get_fix(&p);
	ps->max_thrust = dio_get_fix(&p);
	ps->reverse_thrust = dio_get_fix(&p);
	ps->brakes = dio_get_fix(&p);
	ps->wiggle = dio_get_fix(&p);
	ps->max_rotthrust = dio_get_fix(&p);
	for (i=0; i<N_PLAYER_GUNS; i++)
		dio_get_vector(&p, &ps->gun_points[i]);
	Assert(p - buf == PLAYER_SHIP_DISK_SIZE);
}

// Reads one record of "size" bytes from the file into buf. A short read
// leaves zero bytes, as the cfread() of the original left the old contents
// of a table that was cleared at program start.
static void bm_read_record(ubyte *buf, int size, CFILE *fp)
{
	memset(buf, 0, size);
	cfread(buf, size, 1, fp);
}

// Reads n records of the given size and converts each one with the function
// from_disk into the array arr.
#define BM_READ_TABLE(arr, n, size, from_disk, fp) do {	\
		ubyte bm_rec_buf[size];									\
		int bm_rec_i;												\
		for (bm_rec_i=0; bm_rec_i<(n); bm_rec_i++) {		\
			bm_read_record(bm_rec_buf, size, fp);			\
			from_disk(&(arr)[bm_rec_i], bm_rec_buf);		\
		}																\
	} while (0)

static void bm_read_bitmap_indexes(bitmap_index *bi, int n, CFILE *fp)
{
	int i;
	for (i=0; i<n; i++)
		bi[i].index = cf_get_ushort(fp);
}

void bm_read_all(CFILE * fp)
{
	int i;

	NumTextures = cf_get_int(fp);
	bm_read_bitmap_indexes( Textures, MAX_TEXTURES, fp );
	BM_READ_TABLE( TmapInfo, MAX_TEXTURES, TMAP_INFO_DISK_SIZE, tmap_info_from_disk, fp );

	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	Num_vclips = cf_get_int(fp);
	BM_READ_TABLE( Vclip, VCLIP_MAXNUM, VCLIP_DISK_SIZE, vclip_from_disk, fp );

	Num_effects = cf_get_int(fp);
	BM_READ_TABLE( Effects, MAX_EFFECTS, ECLIP_DISK_SIZE, eclip_from_disk, fp );

	Num_wall_anims = cf_get_int(fp);
	BM_READ_TABLE( WallAnims, MAX_WALL_ANIMS, WCLIP_DISK_SIZE, wclip_from_disk, fp );

	N_robot_types = cf_get_int(fp);
	BM_READ_TABLE( Robot_info, MAX_ROBOT_TYPES, ROBOT_INFO_DISK_SIZE, robot_info_from_disk, fp );

	N_robot_joints = cf_get_int(fp);
	BM_READ_TABLE( Robot_joints, MAX_ROBOT_JOINTS, JOINTPOS_DISK_SIZE, jointpos_from_disk, fp );

	N_weapon_types = cf_get_int(fp);
	BM_READ_TABLE( Weapon_info, MAX_WEAPON_TYPES, WEAPON_INFO_DISK_SIZE, weapon_info_from_disk, fp );

	N_powerup_types = cf_get_int(fp);
	BM_READ_TABLE( Powerup_info, MAX_POWERUP_TYPES, POWERUP_TYPE_INFO_DISK_SIZE, powerup_type_info_from_disk, fp );

	N_polygon_models = cf_get_int(fp);
	// ASCENT: the original did not check the count. A count above the size
	// of the array is an error in the file.
	if ( N_polygon_models < 0 || N_polygon_models > MAX_POLYGON_MODELS )
		Error( "Bad number of polygon models (%d) in the PIG file", N_polygon_models );
	BM_READ_TABLE( Polygon_models, N_polygon_models, POLYMODEL_DISK_SIZE, polymodel_from_disk, fp );

	for (i=0; i<N_polygon_models; i++ )	{
		Polygon_models[i].model_data = malloc(Polygon_models[i].model_data_size);
		Assert( Polygon_models[i].model_data != NULL );
		cfread( Polygon_models[i].model_data, sizeof(ubyte), Polygon_models[i].model_data_size, fp );
	}

	bm_read_bitmap_indexes( Gauges, MAX_GAUGE_BMS, fp );

	cf_get_ints( fp, Dying_modelnums, MAX_POLYGON_MODELS );
	cf_get_ints( fp, Dead_modelnums, MAX_POLYGON_MODELS );

	bm_read_bitmap_indexes( ObjBitmaps, MAX_OBJ_BITMAPS, fp );
	for (i=0; i<MAX_OBJ_BITMAPS; i++ )
		ObjBitmapPtrs[i] = cf_get_ushort(fp);

	BM_READ_TABLE( &only_player_ship, 1, PLAYER_SHIP_DISK_SIZE, player_ship_from_disk, fp );

	Num_cockpits = cf_get_int(fp);
	bm_read_bitmap_indexes( cockpit_bitmap, N_COCKPIT_BITMAPS, fp );

	cfread( Sounds, sizeof(ubyte), MAX_SOUNDS, fp );
	cfread( AltSounds, sizeof(ubyte), MAX_SOUNDS, fp );

	Num_total_object_types = cf_get_int(fp);
	cfread( ObjType, sizeof(byte), MAX_OBJTYPE, fp );
	cfread( ObjId, sizeof(byte), MAX_OBJTYPE, fp );
	cf_get_ints( fp, ObjStrength, MAX_OBJTYPE );

	First_multi_bitmap_num = cf_get_int(fp);

	N_controlcen_guns = cf_get_int(fp);
	cf_get_vectors( fp, controlcen_gun_points, MAX_CONTROLCEN_GUNS );
	cf_get_vectors( fp, controlcen_gun_dirs, MAX_CONTROLCEN_GUNS );
	exit_modelnum = cf_get_int(fp);
	destroyed_exit_modelnum = cf_get_int(fp);
}
