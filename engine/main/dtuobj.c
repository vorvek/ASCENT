/*
 * ASCENT: new file. See dtuobj.h.
 *
 * The functions follow DTUGAME.EXE 1.3. The address of the original code
 * is at each function. Addresses are linear addresses of the image with
 * object 1 at 0x10000. The marks in the comments:
 *   (code)  read in the disassembly
 *   (doc)   taken from docs/re and not read again
 *   (guess) not in the executable in this form
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "mono.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "collide.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "piggy.h"
#include "vclip.h"
#include "weapon.h"
#include "player.h"
#include "gauges.h"
#include "lighting.h"
#include "wall.h"
#include "gamesave.h"
#include "gameseq.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuobj.h"

int check_and_fix_matrix(vms_matrix *m);		// gamesave.c

//------------------------------------------------------------------------
// Settings

int Dtu_fix_idle_anim = 1;
int Dtu_fix_ai_ticks = 1;
int Dtu_fix_creature_move = 1;
int Dtu_creatures_attack = 1;
int Dtu_idle_anim_near = 0;
int Dtu_player_dead = 0;

// 1: a creature sees the player if a ray from it to the player is free.
// 0: the rule of the executable: the bit of the BRN file of the level for
// the two segments (0x9810D). The BRN files of the disc are copies of one
// matrix that fits no level (docs/formats/brn.md), so the default is 1.
int Dtu_fix_brain = 1;

//------------------------------------------------------------------------
// Tables

// 0x13D9E0
short Dtu_anim_first[DTU_N_ANIM_STATES+1] = {
	0, 0, 4, 20, 24, 28, 44, 60, 76, 84, 96, 108, 124, 140, 152, 164, 168, 176, 180, 192 };

// 0x13A956: the bonus of the strength for the to-hit roll
static const signed char Str_to_hit[20] = {
	0, -5, -3, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3 };

static char *Anim_state_names[] = { "REST", "STAND", "WALK", "SIDLE_R", "SIDLE_L", "RUN",
	"ATTACK", "ATTACK2", "FLINCH", "DIE", "FIDGET" };
static char *Ai_state_names[] = { "NONE", "REST", "SRCH", "LOCK", "FLIN", "ATAK", "RECO", "DEAD" };
static char *Ai_mode_names[] = { "STILL", "WANDER", "FOL_PATH", "CHASE_OB", "RUN_FROM", "HOME", "OPENDOOR" };

// The values of a $MONSTER line that monster_info does not have.
typedef struct monster_ext {
	int	damage_dice, n_damage_dice, to_hit_bonus, damage_bonus;
	int	regeneration;
	int	weapon_type;			// -1 if the line has none (guess: a natural weapon)
	int	spell_level;
} monster_ext;

static monster_ext Monster_ext[MAX_ROBOT_TYPES];

// The data of the AI for each object (Ai_local_info at 0x153EB0, and
// fields of the object of the executable that the port has no place for).
#define AILF_BACKWARD	0x02		// play the animation backward (revive)
#define AILF_AROUND		0x04
#define AILF_TOWARD		0x08
#define AILF_AWAY			0x10
#define AILF_HIT			0x20		// the hit landed, end the animation state early

typedef struct dtu_ai_local {
	ubyte		awareness;				// +0x00
	ubyte		retry_count;			// +0x01
	ubyte		mode;						// +0x03
	ubyte		previous_visibility;	// +0x04
	ubyte		flags;					// +0x05, AILF_
	ubyte		activity;				// object +0x1CC: 0 inactive, 2 active
	ubyte		swing_done;				// object +0x1AF: the swing has hit
	ubyte		falling;					// object +0x1AE bit 0x20
	short		blocker;					// object +0x138
	short		blocked_count;			// object +0x13A
	int		target;					// object +0x1A2: 0 the player, -1 none
	fix		awareness_time;		// +0x10
	fix		time_player_seen;		// +0x14
	fix		time_since_processed;	// +0x20
	fix		anim_start;				// +0x24
	int		morale;					// +0x28
	fix		regen_time;				// object +0x8C
	// values of the corrected rules
	fix		idle_acc;
	int		idle_ticks;
	fix		push_acc;
	fix		step_acc;
	// the velocity that the AI adds in this frame (0x11EC9)
	vms_vector move_add;
	int		has_move;
	int		last_logged_state;
} dtu_ai_local;

static dtu_ai_local Ail[MAX_OBJECTS];

dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
int Dtu_n_inventory = 0;
int Dtu_gold = 0;

// The clock of the player structure (0x1D339F): the sum of the frame times,
// wraps at 3600.0.
static fix Dtu_clock = 0;
// The reference ticks (3/64 second) since the start of the level, and how
// many of them belong to this frame.
static int Ref_tick_count = 0, Ref_ticks_now = 0;
static fix Ref_tick_acc = 0;

static ubyte *Brain = NULL;			// 800 rows of 100 bytes, or NULL
static int Player_armor_class = 10;

// The attack of the player
static fix Player_anim_start = 0;
static int Player_swing_done = 0;

static FILE *Obj_log = NULL;
static int Obj_log_frames = 0;		// option -objlogframes: a line for each change of the animation frame
static int Test_pos_pending;

#define MAX_IGNORE_OBJS 100		// as in physics.c

//------------------------------------------------------------------------
// Log

void dtu_obj_log(char *format, ...)
{
	va_list args;

	if (!Obj_log)
		return;
	fprintf(Obj_log,"%9.3f %6d ",f2fl(GameTime),FrameCount);
	va_start(args,format);
	vfprintf(Obj_log,format,args);
	va_end(args);
	fprintf(Obj_log,"\n");
	fflush(Obj_log);
}

char *dtu_object_name(object *obj)
{
	static char unknown[] = "something";

	switch (obj->type) {
		case OBJ_ROBOT:		if (obj->id < N_robot_types) return Monster_info[obj->id].name; break;
		case OBJ_DRESSING:	if (obj->id < N_dressing_types) return Dressing_info[obj->id].name; break;
		case OBJ_MISC:			if (obj->id < N_misc_types) return Misc_info[obj->id].name; break;
		case OBJ_ARMOR:		if (obj->id < N_armor_types) return Armor_info[obj->id].name; break;
		case OBJ_SPELL:		if (obj->id < N_spell_types) return Spell_info[obj->id].name; break;
		case OBJ_WEAPON:		if (obj->id < N_weapon_types) return Weapon_item_info[obj->id].name; break;
		case OBJ_PLAYER:		return "player";
	}
	return unknown;
}

static char *anim_name(int s)
{
	static char buf[16];

	if (s >= 0 && s <= DAS_FIDGET)
		return Anim_state_names[s];
	sprintf(buf,"%d",s);
	return buf;
}

// Writes the state of a creature to the log if it changed.
static void log_creature_state(object *obj,char *why)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int key = 0;

	if (!Obj_log)
		return;
	if (Obj_log_frames)
		key = DTU_ANIM_FRAME(obj) * 0x01000193;
	key ^= DTU_ANIM_STATE(obj) | (aip->CURRENT_STATE << 8) | (aip->GOAL_STATE << 12) | (ail->mode << 16) | (ail->activity << 20) | (ail->awareness << 24);
	if (key == ail->last_logged_state)
		return;
	ail->last_logged_state = key;
	dtu_obj_log("obj %d %s: anim %s frame %d cur %s goal %s mode %s act %d awr %d morale %d hp %d (%s)",
		(int)(obj-Objects),dtu_object_name(obj),anim_name(DTU_ANIM_STATE(obj)),DTU_ANIM_FRAME(obj),
		Ai_state_names[aip->CURRENT_STATE & 7],Ai_state_names[aip->GOAL_STATE & 7],
		Ai_mode_names[ail->mode % 7],ail->activity,ail->awareness,ail->morale,f2i(obj->shields),why);
}

//------------------------------------------------------------------------
// Helpers

static polymodel *obj_model(object *obj)
{
	int n = obj->rtype.pobj_info.model_num;

	if (obj->render_type != RT_POLYOBJ || n < 0 || n >= N_polygon_models)
		return NULL;
	if (Polygon_models[n].dtu_block == NULL)
		return NULL;
	return &Polygon_models[n];
}

static int is_hostile(object *obj)
{
	return (obj->dtu.m_1b0 & DTU_HOSTILE) != 0;
}

static int is_flying(object *obj)
{
	return (Monster_info[obj->id].flags & MONSTER_F_FLYING) || (obj->dtu.m_88 & DTU_OF_FLY);
}

static int tbl_int(int record,char *key,int def)
{
	char value[64];

	if (record < 0 || !bm_tbl_get(record,key,value,sizeof(value)) || !value[0])
		return def;
	return atoi(value);
}

// The time of an animation state of a model in seconds (0xE4 of the model).
static fix state_time(polymodel *po,int state)
{
	if (po == NULL || state < 0 || state >= DTU_N_ANIM_STATES)
		return 0;
	return po->state_times[state];
}

// 0x74AA7. The speed of the movement of an animation state.
static fix anim_speed(object *obj,int state)
{
	polymodel *po = obj_model(obj);
	fix v;

	if (po == NULL)
		return 0;
	if (state == -1)
		state = DTU_ANIM_STATE(obj);
	if (state >= DAS_WALK && state <= DAS_SIDLE_L)
		v = po->speeds[0];
	else if (state == DAS_RUN)
		v = po->speeds[2];
	else if (state >= 13 && state <= 18)
		v = po->speeds[state-10];
	else
		return 0;
	if (obj->dtu.m_88 & DTU_OF_FAST)
		v *= 2;
	else if (obj->dtu.m_88 & DTU_OF_SLOW)
		v >>= 1;
	return v;
}

static void set_anim(object *obj,int state)
{
	DTU_ANIM_STATE(obj) = state;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[state];
}

// 0x746B3
void dtu_get_body_size(object *obj, fix *radius, fix *half_height)
{
	polymodel *po = obj_model(obj);

	if (obj->type == OBJ_PLAYER) {
		*radius = DTU_BODY_RAD;
		*half_height = DTU_BODY_HALF_HEIGHT;
	}
	else if (po && po->body_radius) {
		*radius = po->body_radius[0];
		*half_height = po->body_half_height[0];
	}
	else
		*radius = *half_height = obj->size;
}

// 0x748D9. The cylinder of the body for collisions. A model with a torso
// box gives the torso. The executable uses model 16 for a player. The port
// has the constants of physics.h for the player.
void dtu_get_body_cylinder(object *obj, vms_vector *center, fix *radius, fix *half_height)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);

	*center = obj->pos;

	if (obj->type == OBJ_PLAYER) {
		*radius = DTU_BODY_RAD;
		*half_height = DTU_BODY_HALF_HEIGHT;
		return;
	}

	if (po && po->torso_center && po->n_frames > 0) {
		vms_matrix m;
		vms_vector ofs;

		if (frame < 0 || frame >= po->n_frames)
			frame = 0;
		vm_copy_transpose_matrix(&m,&obj->orient);
		vm_vec_rotate(&ofs,&po->torso_center[0],&m);
		vm_vec_add2(center,&ofs);
		*radius = po->torso_radius[0];
		*half_height = po->torso_half_height[frame];
		return;
	}

	dtu_get_body_size(obj,radius,half_height);
}

// 0x7473F. The sphere of the weapon of the current frame. Returns 0 if
// the model has no weapon box.
static int get_weapon_sphere(object *obj,int frame,vms_vector *center,fix *radius)
{
	polymodel *po = obj_model(obj);
	vms_matrix m;
	vms_vector ofs;

	if (po == NULL || !(po->box_flags & DTU_BOX_BODY) || po->weapon_center == NULL)
		return 0;
	if (frame < 0 || frame >= po->n_frames)
		return 0;

	vm_copy_transpose_matrix(&m,&obj->orient);
	vm_vec_rotate(&ofs,&po->weapon_center[frame],&m);
	vm_vec_add(center,&obj->pos,&ofs);
	*radius = po->weapon_radius[frame];
	return 1;
}

// A test of the AI that the executable writes as "(clock + objnum) % n ==
// 0". The clock is a time in 16.16 seconds, so the test is true in one
// game frame of n.
static int ai_tick(int objnum,int n)
{
	int i;

	if (!Dtu_fix_ai_ticks)
		return ((Dtu_clock + objnum) % n) == 0;

	for (i=0;i<Ref_ticks_now;i++)
		if (((Ref_tick_count - i + objnum) % n) == 0)
			return 1;
	return 0;
}

// The distance from the feet of an object down to the floor (0x6C30E).
// A negative value: the feet are in the floor.
static fix creature_floor_dist(object *obj,fix radius,fix half_height)
{
	fvi_query fq;
	fvi_info hit;
	vms_vector p1;
	int fate;
	fix result;

	p1 = obj->pos;
	p1.y -= DTU_FLOOR_PROBE + half_height;

	// A creature stands with its center at the floor plus the half height
	// of the rest pose (0x13FCD puts a creature that is not active there).
	// So the probe is thin. The radius of the body is not used.
	(void)radius;
	fq.p0 = &obj->pos;
	fq.p1 = &p1;
	fq.startseg = obj->segnum;
	fq.rad = 0x2000;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	Fvi_dtu_body = 2;
	fate = find_vector_intersection(&fq,&hit);
	Fvi_dtu_body = 0;

	if (fate == HIT_WALL)
		result = (obj->pos.y - (hit.hit_pnt.y - fq.rad/2)) - half_height;
	else if (fate == HIT_BAD_P0)
		result = 0;
	else
		result = DTU_FLOOR_PROBE;

	if (abs(result) < DTU_FLOOR_EPS)
		result = 0;
	return result;
}

// 1 if a ray from p0 (in segment seg0) to p1 hits no wall.
static int ray_is_free(vms_vector *p0,int seg0,vms_vector *p1,int objnum)
{
	fvi_query fq;
	fvi_info hit;

	fq.p0 = p0;
	fq.p1 = p1;
	fq.startseg = seg0;
	fq.rad = 0;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	return find_vector_intersection(&fq,&hit) == HIT_NONE;
}

// 0x9810D. The bit of the BRN file: segment a sees segment b.
static int brain_bit(int a,int b)
{
	if (Dtu_fix_brain || Brain == NULL)
		return 1;
	if (a < 0 || a >= 800 || b < 0 || b >= 800)
		return 0;
	return (Brain[a*100 + b/8] >> (b & 7)) & 1;
}

// 0x10815. The ray from a creature to the player. A creature that does not
// fly looks from 5.0 above its center. A blocked ray counts as free for a
// melee creature that is closer than 100.0.
static int creature_ray_to_player(object *obj)
{
	vms_vector p0 = obj->pos;
	int seg;

	if (!is_flying(obj))
		p0.y += i2f(5);
	seg = find_point_seg(&p0,obj->segnum);
	if (seg < 0) {
		p0 = obj->pos;
		seg = obj->segnum;
	}
	if (ray_is_free(&p0,seg,&ConsoleObject->pos,obj-Objects))
		return 1;
	if (Robot_info[obj->id].attack_type == 1 && Monster_ext[obj->id].spell_level == 0)
		if (vm_vec_dist(&obj->pos,&ConsoleObject->pos) < i2f(100))
			return 1;
	return 0;
}

// 0x109E7 player_is_visible_from_object: the brain bit and nothing else in
// the executable. The corrected rule casts a ray.
static int player_is_visible(object *obj)
{
	if (!Dtu_fix_brain && Brain)
		return brain_bit(obj->segnum,ConsoleObject->segnum);
	return ray_is_free(&obj->pos,obj->segnum,&ConsoleObject->pos,obj-Objects);
}

//------------------------------------------------------------------------
// Inventory

void dtu_inventory_clear(void)
{
	memset(Dtu_inventory,0,sizeof(Dtu_inventory));
	Dtu_n_inventory = 0;
	Dtu_gold = 0;
}

static item_info *item_table_entry(int type,int id)
{
	switch (type) {
		case OBJ_MISC:		return (id < N_misc_types) ? &Misc_info[id] : NULL;
		case OBJ_ARMOR:	return (id < N_armor_types) ? &Armor_info[id] : NULL;
		case OBJ_SPELL:	return (id < N_spell_types) ? &Spell_info[id] : NULL;
		case OBJ_WEAPON:	return (id < N_weapon_types) ? &Weapon_item_info[id] : NULL;
	}
	return NULL;
}

// 0xB61B2: the type has the word "stackable" in its table line.
static int item_is_stackable(item_info *it)
{
	return it && it->record >= 0 && bm_tbl_has_flag(it->record,"stackable");
}

int dtu_inventory_add(int type, int id, int count)
{
	item_info *it = item_table_entry(type,id);
	dtu_inv_item *inv;
	int i;

	if (it == NULL || count <= 0)
		return -1;

	if (item_is_stackable(it))
		for (i=0;i<Dtu_n_inventory;i++)
			if (Dtu_inventory[i].type == type && Dtu_inventory[i].id == id) {
				int limit = it->stack_limit > 0 ? it->stack_limit : 10;

				if (Dtu_inventory[i].count + count > limit)
					return -1;
				Dtu_inventory[i].count += count;
				return i;
			}

	if (Dtu_n_inventory >= DTU_MAX_INVENTORY)
		return -1;

	inv = &Dtu_inventory[Dtu_n_inventory];
	memset(inv,0,sizeof(*inv));
	inv->type = type;
	inv->id = id;
	inv->count = count;
	inv->icon = it->icon;
	inv->inv_icon = it->inv_icon;
	inv->obj_bitmap = it->obj_bitmap;
	inv->level = Current_level_num;
	strncpy(inv->name,it->name,sizeof(inv->name)-1);
	return Dtu_n_inventory++;
}

void dtu_inventory_remove(int index, int count)
{
	if (index < 0 || index >= Dtu_n_inventory)
		return;
	Dtu_inventory[index].count -= count;
	if (Dtu_inventory[index].count <= 0) {
		memmove(&Dtu_inventory[index],&Dtu_inventory[index+1],(Dtu_n_inventory-index-1)*sizeof(dtu_inv_item));
		Dtu_n_inventory--;
		memset(&Dtu_inventory[Dtu_n_inventory],0,sizeof(dtu_inv_item));
	}
}

//------------------------------------------------------------------------
// Setup

void dtu_objects_init(void)
{
	int t;

	if (FindArg("-noobjects"))
		Dtu_objects_enabled = 0;
	if (FindArg("-origobj"))
		Dtu_fix_idle_anim = Dtu_fix_ai_ticks = Dtu_fix_creature_move = Dtu_fix_brain = 0;
	if (FindArg("-noattack"))
		Dtu_creatures_attack = 0;
	if (FindArg("-idleanim"))
		Dtu_idle_anim_near = 1;
	if ((t = FindArg("-objlog")) != 0 && t+1 < Num_args)
		Obj_log = fopen(Args[t+1],"w");
	if (FindArg("-objlogframes"))
		Obj_log_frames = 1;

	if (!Dtu_objects_enabled)
		return;

	// The pairs of the collision table of the executable (0x292D7) that
	// the port handles. All other pairs do nothing.
	memset(CollisionResult, RESULT_NOTHING, sizeof(CollisionResult));
	#define DTU_COLLIDE(a,b) do { CollisionResult[a][b] = RESULT_CHECK; CollisionResult[b][a] = RESULT_CHECK; } while (0)
	DTU_COLLIDE(OBJ_PLAYER,OBJ_WALL);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_WALL);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_ROBOT);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_ROBOT);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_WEAPON);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_ARMOR);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_MISC);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_SPELL);
	DTU_COLLIDE(OBJ_PLAYER,OBJ_DRESSING);
	DTU_COLLIDE(OBJ_ROBOT,OBJ_DRESSING);
	#undef DTU_COLLIDE
}

// init_ai_object (0x1038B) for a creature of a level, with the values that
// place_creatures (0x6A6FB) and add_npc (0xAC431) set.
static void init_creature_ai(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];

	memset(ail,0,sizeof(*ail));
	ail->last_logged_state = -1;
	ail->blocker = -1;

	if (aip->behavior < DAIB_GUARD || aip->behavior > DAIB_STATION)
		aip->behavior = DAIB_WANDER;
	if (aip->behavior == DAIB_STATION && Robot_info[obj->id].attack_type == 1)
		aip->behavior = DAIB_WANDER;

	ail->mode = (aip->behavior == DAIB_PATROL) ? DAIM_FOL_PATH : DAIM_STILL;
	ail->activity = 0;
	ail->awareness = 0;
	ail->awareness_time = i2f(3);
	ail->morale = (mi->ai_values[0][Difficulty_level] + mi->ai_values[1][Difficulty_level]) / 2;
	ail->anim_start = ail->time_player_seen = ail->regen_time = GameTime;

	vm_vec_zero(&obj->mtype.phys_info.velocity);
	vm_vec_zero(&obj->mtype.phys_info.thrust);
	vm_vec_zero(&obj->mtype.phys_info.rotvel);

	// add_npc: a creature with the bit "peaceful" is not hostile. The
	// executable also asks the script of the creature (0xA8C49). The
	// scripts are not in the port, so a creature with a script number is
	// not hostile here. (guess)
	if ((obj->dtu.m_1b0 & DTU_PEACEFUL) || (obj->dtu.m_1b0 & DTU_SCRIPT_MASK) || !(obj->dtu.m_1b0 & DTU_HOSTILE)) {
		obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		ail->target = -1;
	}
	else {
		obj->dtu.m_1b0 |= DTU_HOSTILE;
		ail->target = 0;
	}

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
}

static void setup_creature(object *obj)
{
	robot_info *ri;
	polymodel *po;

	if (N_robot_types <= 0) {
		obj->render_type = RT_NONE;
		obj->control_type = CT_NONE;
		obj->movement_type = MT_NONE;
		return;
	}

	// verify_object 0x477D9
	if (obj->id >= N_robot_types)
		obj->id = obj->id % N_robot_types;
	ri = &Robot_info[obj->id];

	if (obj->render_type == RT_POLYOBJ) {
		obj->rtype.pobj_info.model_num = ri->model_num;
		po = obj_model(obj);
		if (po == NULL) {
			obj->render_type = RT_NONE;
			obj->control_type = CT_NONE;
			obj->movement_type = MT_NONE;
			return;
		}
		obj->size = po->rad;
	}
	obj->rtype.pobj_info.tmap_override = -1;

	if (obj->movement_type == MT_PHYSICS) {
		obj->mtype.phys_info.drag = ri->drag;
		// setup_object 0xAE3F2: the mass is the size, 4.0 at least
		obj->mtype.phys_info.mass = obj->size;
		if (obj->mtype.phys_info.mass < i2f(4))
			obj->mtype.phys_info.mass = i2f(4);
	}

	// place_creatures 0x6A6FB
	DTU_ANIM_STATE(obj) = DAS_STAND;
	DTU_ANIM_GOAL(obj) = DAS_STAND;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_STAND];
	check_and_fix_matrix(&obj->orient);

	if (obj->control_type == CT_AI)
		init_creature_ai(obj);

	// A creature with less than one hit point is loaded dead (0x4787A).
	if (obj->shields < F1_0) {
		obj->ctype.ai_info.CURRENT_STATE = obj->ctype.ai_info.GOAL_STATE = DAIS_DEAD;
		DTU_ANIM_STATE(obj) = DAS_DIE;
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_DIE+1] - 1;
		obj->flags |= OF_DESTROYED;
		obj->control_type = CT_NONE;
		obj->movement_type = MT_NONE;
	}
}

void dtu_objects_level_setup(void)
{
	int i;
	ubyte *data;
	int size;
	char name[32];

	if (!Dtu_objects_enabled)
		return;

	Dtu_clock = 0;
	Ref_tick_count = Ref_ticks_now = 0;
	Ref_tick_acc = 0;
	Player_anim_start = 0;
	Player_swing_done = 0;
	Test_pos_pending = 1;
	Dtu_player_dead = 0;

	for (i=0;i<N_robot_types;i++) {
		int rec = Monster_info[i].record;

		Monster_ext[i].damage_dice = tbl_int(rec,"damage_dice",0);
		Monster_ext[i].n_damage_dice = tbl_int(rec,"#damage_dice",0);
		Monster_ext[i].to_hit_bonus = tbl_int(rec,"to_hit_bonus",0);
		Monster_ext[i].damage_bonus = tbl_int(rec,"damage_bonus",0);
		Monster_ext[i].regeneration = tbl_int(rec,"regeneration",0);
		Monster_ext[i].weapon_type = tbl_int(rec,"weapon_type",-1);
		Monster_ext[i].spell_level = tbl_int(rec,"spell_level",0);
	}

	Player_armor_class = 10;
	if (N_player_types > 0)
		Player_armor_class = tbl_int(Player_type_info[0].record,"armor_class",10);

	// The BRN file of the level (loader 0x97774)
	if (Brain) {
		free(Brain);
		Brain = NULL;
	}
	{
		char *p;

		strncpy(name,Gamesave_current_filename,sizeof(name)-5);
		name[sizeof(name)-5] = 0;
		if ((p = strchr(name,'.')) != NULL)
			*p = 0;
		strcat(name,".BRN");
		data = dtu_load_file(name,&size);
		if (data && size >= 80000)
			Brain = data;
		else if (data)
			free(data);
	}

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		switch (obj->type) {

			case OBJ_ROBOT:
				setup_creature(obj);
				break;

			case OBJ_DRESSING:
				// setup_object: shields 1.0. fix_object_segs 0x69FFB: the model
				// or the bitmap of the table entry.
				obj->shields = F1_0;
				if (obj->id >= N_dressing_types) {
					obj->render_type = RT_NONE;
					break;
				}
				if (obj->render_type == RT_POLYOBJ) {
					obj->rtype.pobj_info.model_num = Dressing_info[obj->id].model_num;
					obj->rtype.pobj_info.tmap_override = -1;
					if (obj_model(obj) == NULL)
						obj->render_type = RT_NONE;
				}
				else if (obj->render_type == RT_ITEM) {
					obj->dtu.item_bitmap = Dressing_info[obj->id].obj_bitmap;
					if (obj->dtu.item_bitmap < 0)
						obj->render_type = RT_NONE;
				}
				else if (obj->render_type == RT_WEAPON_VCLIP) {
					if (Dressing_info[obj->id].vclip_num >= 0)
						obj->rtype.vclip_info.vclip_num = Dressing_info[obj->id].vclip_num;
				}
				break;

			case OBJ_MISC:
			case OBJ_ARMOR:
			case OBJ_SPELL:
			case OBJ_WEAPON:
				if (obj->render_type == RT_ITEM) {
					item_info *it = item_table_entry(obj->type,obj->id);

					obj->dtu.item_bitmap = it ? it->obj_bitmap : -1;
					if (obj->dtu.item_bitmap < 0)
						obj->render_type = RT_NONE;
				}
				else if (obj->type != OBJ_WEAPON)
					obj->render_type = RT_NONE;
				break;

			case OBJ_PLAYER:
				DTU_ANIM_STATE(obj) = DAS_STAND;
				DTU_ANIM_GOAL(obj) = DAS_STAND;
				DTU_ANIM_FRAME(obj) = 0;
				break;

			case OBJ_POWERUP:
				// verify_object: a powerup of a level file is deleted
				obj->flags |= OF_SHOULD_BE_DEAD;
				break;
		}
	}

	if (Obj_log) {
		int n[MAX_OBJECT_TYPES];

		memset(n,0,sizeof(n));
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type < MAX_OBJECT_TYPES)
				n[Objects[i].type]++;
		dtu_obj_log("level %s: %d creatures, %d dressing, %d misc, %d armor, %d spell, %d weapon objects, brain %s",
			Gamesave_current_filename,n[OBJ_ROBOT],n[OBJ_DRESSING],n[OBJ_MISC],n[OBJ_ARMOR],n[OBJ_SPELL],n[OBJ_WEAPON],
			Brain ? (Dtu_fix_brain ? "loaded, not used" : "used") : "none");
		for (i=0;i<=Highest_object_index;i++)
			if (Objects[i].type == OBJ_ROBOT)
				dtu_obj_log("obj %d %s: hp %d hostile %d behavior 0x%x script %d model %d size %.2f pos %.2f %.2f %.2f",
					i,dtu_object_name(&Objects[i]),f2i(Objects[i].shields),is_hostile(&Objects[i]),
					Objects[i].ctype.ai_info.behavior,Objects[i].dtu.m_1b0 & DTU_SCRIPT_MASK,
					Objects[i].rtype.pobj_info.model_num,f2fl(Objects[i].size),
					f2fl(Objects[i].pos.x),f2fl(Objects[i].pos.y),f2fl(Objects[i].pos.z));
	}
}

//------------------------------------------------------------------------
// Rendering

// The normal case of draw_polygon_object (0x676ED to 0x67A1C).
void dtu_draw_polygon_object(object *obj, fix light, fix *glow, bitmap_index *alt_textures)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);
	int show_hidden = 1;

	if (po == NULL)
		return;

	// "The missile in the hand" (0x677EB to 0x67982): a submodel with the
	// flag 0x10 is not drawn from the fire frame of an attack to its end.
	// The test of the weapon table (Weapon_info + 0x0A is 3) is not in the
	// port: each creature with no weapon type counts.
	if (obj->type == OBJ_ROBOT && obj->id < N_robot_types && Monster_ext[obj->id].weapon_type < 0) {
		int s = DTU_ANIM_STATE(obj);

		if (s == DAS_ATTACK && frame >= po->fire_frames[0])
			show_hidden = 0;
		if (s == DAS_ATTACK2 && frame >= po->fire_frames[1])
			show_hidden = 0;
	}

	if (obj->type == OBJ_DRESSING)
		frame = -1;

	draw_polygon_model_frame(&obj->pos,&obj->orient,obj->rtype.pobj_info.model_num,frame,show_hidden,light,glow,alt_textures);
}

// Render type 8 (0x67E2D): one bitmap of the object bitmaps as a rod that
// stands on the up vector of the object. Height and width are two times
// the size. The executable takes the light of the segment and not the
// light function of Descent 1 (0x67273).
void dtu_draw_item(object *obj)
{
	int slot = obj->dtu.item_bitmap;

	if (slot < 0 || slot >= MAX_OBJ_BITMAPS)
		return;
	draw_object_tmap_rod(obj,ObjBitmaps[ObjBitmapPtrs[slot]],1);
}

// Render type 7 of scenery (0x7C734, not read): the frames of the vclip
// by the game time. (guess)
void dtu_draw_dressing_vclip(object *obj)
{
	int n = obj->rtype.vclip_info.vclip_num;
	vclip *vc;
	int frame;

	if (n < 0 || n >= VCLIP_MAXNUM)
		return;
	vc = &Vclip[n];
	if (vc->num_frames <= 0 || vc->frame_time <= 0)
		return;
	frame = (GameTime / vc->frame_time) % vc->num_frames;
	draw_object_blob(obj,vc->frames[frame]);
}

//------------------------------------------------------------------------
// Damage

// 0x2A789 without the percent part of strength 18
static int str_bonus(int str)
{
	if (str < 1) str = 1;
	if (str > 19) str = 19;
	return Str_to_hit[str];
}

// 0x2A7F3. The to-hit roll. Returns 2 for a critical hit, 1 for a hit, 0
// for a miss, -1 for a fumble.
static int to_hit_roll(object *target,object *attacker,int *roll_out)
{
	int roll = rand() % 20 + 1;
	int thac0, ac;

	*roll_out = roll;
	if (roll == 20)
		return 2;
	if (roll == 1)
		return -1;

	if (attacker->type == OBJ_PLAYER) {
		// The byte at 0x1D4D78 of the player structure. The port has no
		// character record. 20 is the value of a first level character. (guess)
		thac0 = 20;
		if (N_player_types > 0)
			roll += str_bonus(tbl_int(Player_type_info[0].record,"strength",10));
	}
	else {
		int w = Monster_ext[attacker->id].weapon_type;

		thac0 = Monster_info[attacker->id].thaco;
		// the bonus of the weapon (Weapon_info + 0x36), or of the creature
		// for a natural weapon
		if (w >= 0 && w < N_weapon_types && Weapon_item_info[w].record >= 0)
			roll += tbl_int(Weapon_item_info[w].record,"to_hit_bonus",0);
		else
			roll += Monster_ext[attacker->id].to_hit_bonus;
		roll += str_bonus(Monster_info[attacker->id].str);
	}

	if (target->type == OBJ_PLAYER)
		ac = Player_armor_class;
	else
		ac = Monster_info[target->id].armor_class;

	*roll_out = roll;
	return (thac0 - ac > roll) ? 0 : 1;
}

// 0x2AF5E for a natural weapon. Returns the damage as an integer.
static int damage_dice(object *attacker)
{
	int n, sides, bonus, i, sum = 0;

	if (attacker->type == OBJ_PLAYER) {
		// The dice of the empty hand are in the player structure (0x1D33C1
		// to 0x1D33C7). 1d2 is the fist of the rules of the game. (guess)
		n = 1; sides = 2; bonus = 0;
	}
	else {
		int w = Monster_ext[attacker->id].weapon_type;

		if (w >= 0 && w < N_weapon_types && Weapon_item_info[w].record >= 0) {
			// The dice of the weapon (Weapon_info + 0x0A and + 0x0B). That
			// the bonus of the weapon line is added is a guess.
			int rec = Weapon_item_info[w].record;

			n = tbl_int(rec,"#damage_dice",1);
			sides = tbl_int(rec,"damage_dice",1);
			bonus = tbl_int(rec,"damage_bonus",0);
		}
		else {
			n = Monster_ext[attacker->id].n_damage_dice;
			sides = Monster_ext[attacker->id].damage_dice;
			bonus = Monster_ext[attacker->id].damage_bonus;
		}
		// The executable stops with "RF.damage_dice > 0".
		if (sides < 1) sides = 1;
		if (n < 1) n = 1;
	}
	for (i=0;i<n;i++)
		sum += rand() % sides + 1;
	return sum + bonus;
}

// The end of the DIE animation (0x6A3CC): the creature is a corpse. The
// executable puts it into a list of dead creatures and can take it away
// later (0x696E9, not read). In the port the corpse stays.
static fix creature_floor_dist(object *obj,fix radius,fix half_height);

static void creature_is_corpse(object *obj)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);

	// 0x6A48B to 0x6A510: the body is put on the floor ("RLH: pushing (%d)
	// into floor by %.2f"). The executable measures with the cylinder of
	// 0x748D9. The port uses the box of the body of the last frame, so
	// that the body lies on the floor. (guess for the height)
	if (po && po->body_half_height && frame >= 0 && frame < po->n_frames) {
		fix d = creature_floor_dist(obj,0,po->body_half_height[frame+1]);

		if (d != DTU_FLOOR_PROBE)
			obj->pos.y -= d;
	}

	obj->shields = 0;
	obj->flags |= OF_DESTROYED;
	obj->control_type = CT_NONE;
	obj->movement_type = MT_NONE;
	dtu_obj_log("obj %d %s: is a corpse",(int)(obj-Objects),dtu_object_name(obj));
}

// The death of a creature in the damage function (0x2A091 to 0x2A245).
static void creature_dies(object *obj,object *attacker)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;

	if (attacker && attacker->type == OBJ_PLAYER) {
		HUD_init_message("You killed %s.",dtu_object_name(obj));
		Players[Player_num].score += Robot_info[obj->id].score_value;
		Players[Player_num].num_kills_level++;
		Players[Player_num].num_kills_total++;
	}

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_DEAD;
	DTU_ANIM_GOAL(obj) = DAS_DIE;
	set_anim(obj,DAS_DIE);
	ail->flags = 0;
	ail->anim_start = GameTime;
	ail->mode = DAIM_STILL;
	ail->activity = 2;
	obj->dtu.m_88 = 0;
	obj->shields = 0;
	vm_vec_zero(&obj->mtype.phys_info.velocity);

	dtu_obj_log("obj %d %s: dies",(int)(obj-Objects),dtu_object_name(obj));
	log_creature_state(obj,"death");
}

// 0x29A9C for a creature as the target. damage is used if there is no
// attacker. With an attacker the function throws the dice.
void dtu_damage_creature(object *creature, object *attacker, fix damage)
{
	dtu_ai_local *ail = &Ail[creature-Objects];
	ai_static *aip = &creature->ctype.ai_info;

	if (creature->type != OBJ_ROBOT || creature->shields < F1_0 || (creature->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return;

	if (attacker) {
		int roll, hit = to_hit_roll(creature,attacker,&roll);

		if (hit <= 0) {
			dtu_obj_log("attack: %s misses obj %d %s (roll %d, result %d)",dtu_object_name(attacker),
				(int)(creature-Objects),dtu_object_name(creature),roll,hit);
			return;
		}
		damage = i2f(damage_dice(attacker));
		if (hit == 2)
			damage += i2f(damage_dice(attacker));
	}

	creature->shields -= damage;

	dtu_obj_log("damage: obj %d %s takes %d from %s, hp %d",(int)(creature-Objects),dtu_object_name(creature),
		f2i(damage),attacker ? dtu_object_name(attacker) : "nothing",f2i(creature->shields));

	if (damage > 0) {
		// 0x29FB8: the creature flinches
		if (aip->CURRENT_STATE != DAIS_DEAD) {
			aip->CURRENT_STATE = DAIS_FLIN;
			ail->anim_start = GameTime + FrameTime;
		}
		if (attacker && attacker->type == OBJ_PLAYER)
			HUD_init_message("You hit %s for %d.",dtu_object_name(creature),f2i(damage));
		// A creature that is hit knows where the player is, and a creature
		// that was not hostile is hostile now. (guess: the executable does
		// this in the script code 0xA94AD and in the alert function)
		if (attacker && attacker->type == OBJ_PLAYER) {
			creature->dtu.m_1b0 |= DTU_HOSTILE;
			creature->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
			ail->target = 0;
			if (ail->awareness < 4) ail->awareness = 4;
			ail->awareness_time = i2f(3);
			ail->morale = Monster_info[creature->id].ai_values[1][Difficulty_level];
		}
	}

	if (creature->shields < F1_0)
		creature_dies(creature,attacker);
}

// 0x29A9C for the player as the target (0x2A24A).
void dtu_damage_player(object *player, object *attacker, fix damage)
{
	if (player->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead || Players[Player_num].shields <= 0)
		return;

	if (attacker && attacker != player) {
		int roll, hit = to_hit_roll(player,attacker,&roll);

		if (hit <= 0) {
			dtu_obj_log("attack: obj %d %s misses the player (roll %d, result %d)",(int)(attacker-Objects),
				dtu_object_name(attacker),roll,hit);
			return;
		}
		damage = i2f(damage_dice(attacker));
		if (hit == 2)
			damage += i2f(damage_dice(attacker));
	}

	if (damage <= 0)
		return;

	if (attacker && attacker != player)
		HUD_init_message("%s hits you for %d.",dtu_object_name(attacker),f2i(damage));
	else
		HUD_init_message("You took %d damage.",f2i(damage));

	// The rest of the function of the executable (the death of the
	// player, 0x2A577 and after) is not read. The death sequence of
	// Descent 1 (the ship explodes, the level starts again) does not fit
	// the target game. The port takes the hit points away, and at less
	// than one hit point the player stays where he is and can do nothing.
	Players[Player_num].shields -= damage;
	PALETTE_FLASH_ADD(f2i(damage)*4,-f2i(damage/2),-f2i(damage/2));
	if (Players[Player_num].shields < F1_0) {
		Players[Player_num].shields = 0;
		Dtu_player_dead = 1;
		player->control_type = CT_NONE;
		player->movement_type = MT_NONE;
		HUD_init_message("You have died.");
		dtu_obj_log("player: dies");
	}

	dtu_obj_log("damage: the player takes %d from %s, hp %d",f2i(damage),
		attacker ? dtu_object_name(attacker) : "nothing",f2i(Players[Player_num].shields));
}

// 0x27D82. A melee weapon touched the target.
static void melee_hit(object *target,object *attacker)
{
	if (target->flags & OF_EXPLODING)
		return;
	if (target->type == OBJ_PLAYER) {
		dtu_damage_player(target,attacker,0);
		if (attacker->type == OBJ_ROBOT) {
			// The creature is active after a hit (0x28B34)
			Ail[attacker-Objects].activity = 2;
		}
	}
	else if (target->type == OBJ_ROBOT)
		dtu_damage_creature(target,attacker,0);
}

// 0x6CFF8 (physics.c of the executable). The test of the weapon in an
// attack state. The executable calls it from the movement of each
// creature (0x9F2B4) and of the player. The weapon sphere of the frame
// moves from its center away from the body by its radius. The first
// object that it touches is hit, one time for each swing.
static void weapon_test(object *obj)
{
	int objnum = obj-Objects;
	int s = DTU_ANIM_STATE(obj);
	int n, rel, fate;
	ubyte *swing_done = (obj->type == OBJ_PLAYER) ? (ubyte *)NULL : &Ail[objnum].swing_done;
	vms_vector center, dir, p1, test_pos, save_pos;
	fix radius;
	fvi_query fq;
	fvi_info hit;
	int seg;

	if (!(s == DAS_ATTACK || s == DAS_ATTACK2 || s == DAS_ATTACK3 || s == DAS_ATTACK4)) {
		if (swing_done) *swing_done = 0; else Player_swing_done = 0;
		return;
	}

	n = Dtu_anim_first[s+1] - Dtu_anim_first[s];
	rel = DTU_ANIM_FRAME(obj) - Dtu_anim_first[s];

	// A creature hits from frame 8 of the attack to its end, the player
	// in the frames 6 to 8 (0x6D0CF to 0x6D127).
	if (obj->type == OBJ_ROBOT) {
		if (rel < 8)
			return;
	}
	else if (rel < 6 || rel > n - rel)
		return;

	// 0x6D01A to 0x6D066: for the test the position of the object is
	// moved forward in the x-z plane by the square root of 2 (the code
	// adds sin + cos and cos - sin of the heading minus 45 degrees).
	{
		vms_angvec a;
		fix sn, cs;

		vm_extract_angles_vector(&a,&obj->orient.fvec);
		fix_sincos(a.h - 0x2000,&sn,&cs);
		test_pos = obj->pos;
		test_pos.x += sn + cs;
		test_pos.z -= sn - cs;
	}
	save_pos = obj->pos;
	obj->pos = test_pos;

	if (!get_weapon_sphere(obj,DTU_ANIM_FRAME(obj),&center,&radius)) {
		obj->pos = save_pos;
		if (obj->type != OBJ_PLAYER)
			return;
		// A player model with no weapon box: a sphere in front of the body
		// takes the place of the weapon box. (guess)
		vm_vec_scale_add(&center,&test_pos,&obj->orient.fvec,i2f(4));
		radius = i2f(3);
	}
	obj->pos = save_pos;

	seg = find_point_seg(&center,obj->segnum);
	if (seg < 0)
		return;

	vm_vec_sub(&dir,&center,&test_pos);
	vm_vec_normalize_quick(&dir);
	vm_vec_scale_add(&p1,&center,&dir,radius);

	// The walls: the weapon stops at a wall.
	fq.p0 = &center;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = radius;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;

	fate = find_vector_intersection(&fq,&hit);
	if (fate == HIT_WALL)
		p1 = hit.hit_pnt;
	else if (fate != HIT_NONE)
		return;

	// The objects. The executable does this in the same query (flag 1 of
	// 0x3EFD9). The port tests each body by itself, see
	// dtu_object_hit_test() in fvi.c.
	{
		int i, best = -1;
		fix best_d = 0x7fffffff;
		vms_vector ip;

		for (i=0;i<=Highest_object_index;i++) {
			object *o = &Objects[i];
			fix d;

			if (i == objnum || (o->type != OBJ_ROBOT && o->type != OBJ_PLAYER && o->type != OBJ_DRESSING))
				continue;
			if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
				continue;
			if (vm_vec_dist_quick(&o->pos,&center) > i2f(40))
				continue;
			d = dtu_object_hit_test(&ip,&center,&p1,radius,o,obj);
			if (d > 0 && d < best_d) {
				best_d = d;
				best = i;
			}
		}

		if (Obj_log_frames && obj->type == OBJ_PLAYER)
			dtu_obj_log("player: weapon test frame %d, radius %.2f, hits object %d",DTU_ANIM_FRAME(obj),f2fl(radius),best);

		if (best < 0)
			return;
		hit.hit_object = best;
	}

	{
		object *other = &Objects[hit.hit_object];

		if (other->type == OBJ_DRESSING) {
			// 0x6D4EA: scenery that can break takes 100.0 damage against
			// 20.0 hit points at most. Breaking is not in the port.
			if (swing_done) *swing_done = 0;
		}
		else if (other->type == OBJ_ROBOT) {
			if (obj->type == OBJ_ROBOT)
				return;
			// 0x278B5: the player hits a creature
			if (!Player_swing_done) {
				Player_swing_done = 1;
				melee_hit(other,obj);
			}
		}
		else if (other->type == OBJ_PLAYER) {
			if (swing_done && !*swing_done && Dtu_creatures_attack) {
				*swing_done = 1;
				dtu_obj_log("attack: obj %d %s swings at the player, frame %d",objnum,dtu_object_name(obj),DTU_ANIM_FRAME(obj));
				melee_hit(other,obj);
				// The function 0x1153A (do_ai_robot_hit_attack) sets the flag
				// AILF_HIT here, which ends the swing early. No code of
				// version 1.3 calls that function, so the flag is never set
				// and the attack animation plays to its end.
			}
		}
	}
}

//------------------------------------------------------------------------
// Movement of a creature (pmon.c, 0x9F153)

// 0x6C662 for a creature: gravity and the lift out of the floor.
static void creature_gravity(object *obj,vms_vector *accel,fix radius,fix half_height)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix d = creature_floor_dist(obj,radius,half_height);

	if (d < 0) {
		fix push = DTU_PUSH_UP_MAX, ad = -d;
		vms_vector new_pos;

		if (Dtu_fix_creature_move) {
			ail->push_acc += fixmuldiv(DTU_PUSH_UP_MAX,FrameTime,DTU_REF_FRAMETIME);
			if (ail->push_acc > DTU_PUSH_UP_MAX*4)
				ail->push_acc = DTU_PUSH_UP_MAX*4;
			push = ail->push_acc;
		}
		if (push > ad)
			push = ad;
		if (Dtu_fix_creature_move)
			ail->push_acc -= push;

		new_pos = obj->pos;
		new_pos.y += push;
		if (find_point_seg(&new_pos,obj->segnum) >= 0)
			obj->pos = new_pos;
		if (obj->mtype.phys_info.velocity.y < 0)
			obj->mtype.phys_info.velocity.y = 0;
	}
	else {
		ail->push_acc = 0;
		if (d != 0 && !is_flying(obj))
			accel->y -= fixmul(d,DTU_GRAVITY_K);
	}
}

// 0x6CB68 for a creature, see dtu_integrate() in physics.c.
static void creature_integrate(object *obj,vms_vector *accel)
{
	physics_info *pi = &obj->mtype.phys_info;
	int count;

	if (Dtu_fix_creature_move) {
		// All steps of 1/64 second. The part of a step that is left goes
		// to the next frame of this object.
		fix *acc = &Ail[obj-Objects].step_acc;

		*acc += FrameTime;
		count = *acc / DTU_FT;
		*acc -= count * DTU_FT;
		if (count > 32)
			count = 32;
	}
	else {
		count = FrameTime / DTU_FT;
		if (count > 2)
			count = 2;
	}

	while (count--) {
		vm_vec_add2(&pi->velocity,accel);
		vm_vec_scale(&pi->velocity,F1_0-pi->drag);
	}

	if (pi->velocity.y < DTU_TERMINAL_VEL)
		pi->velocity.y = DTU_TERMINAL_VEL;
}

void dtu_creature_physics_sim(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector accel, start_pos, frame_vec, new_pos, center;
	fix radius, half_height, cyl_radius, cyl_half;
	int tries = 0, fate;
	int ignore_obj_list[MAX_IGNORE_OBJS], n_ignore_objs = 0;
	fvi_query fq;
	fvi_info hit;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	if (obj->dtu.m_88 & DTU_OF_HELD)
		return;
	// An inactive creature does not move (0x9F196).
	if (ail->activity == 0)
		return;
	if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return;

	// 0x9F286: the velocity becomes one half on each frame, and the AI
	// adds direction * speed on each frame (0x11EC9). So the velocity goes
	// to the speed of the animation state: v' = (v + add) / 2. The
	// corrected rule does the same step for the reference frame time.
	if (Dtu_fix_creature_move) {
		// factor = 0.5 ^ (FrameTime / DTU_REF_FRAMETIME), from 16 steps
		static const fix half_pow[17] = { 0x10000, 0xf525, 0xeac0, 0xe0cc, 0xd744, 0xce24, 0xc567, 0xbd08,
			0xb504, 0xad58, 0xa5fe, 0x9ef5, 0x9837, 0x91c3, 0x8b95, 0x85aa, 0x8000 };
		fix t = fixdiv(FrameTime,DTU_REF_FRAMETIME);
		fix f = F1_0;

		while (t >= F1_0) {
			f >>= 1;
			t -= F1_0;
		}
		f = fixmul(f,half_pow[(t >> 12) & 15]);
		vm_vec_scale(&pi->velocity,f);
		if (ail->has_move)
			vm_vec_scale_add2(&pi->velocity,&ail->move_add,F1_0-f);
	}
	else {
		if (ail->has_move)
			vm_vec_add2(&pi->velocity,&ail->move_add);
		vm_vec_scale(&pi->velocity,F1_0/2);
	}
	ail->has_move = 0;

	// 0x6CFF8
	weapon_test(obj);
	if (obj->control_type != CT_AI)
		return;

	dtu_get_body_size(obj,&radius,&half_height);
	dtu_get_body_cylinder(obj,&center,&cyl_radius,&cyl_half);

	// A body that dies goes down with its box. (guess: the executable
	// calls the gravity function from the animation step of DIE, 0x112A5)
	if (DTU_ANIM_STATE(obj) == DAS_DIE) {
		polymodel *po = obj_model(obj);
		int frame = DTU_ANIM_FRAME(obj);

		if (po && po->body_half_height && frame >= 0 && frame < po->n_frames)
			half_height = po->body_half_height[frame+1];
	}

	vm_vec_copy_scale(&accel,&pi->thrust,fixdiv(F1_0,pi->mass));

	creature_gravity(obj,&accel,cyl_radius,half_height);

	// A dying creature that does not fly falls: one eighth of the velocity
	// in x and z on each frame (0x9F4E2).
	if (!is_flying(obj) && obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD)
		ail->falling = 1;
	if (ail->falling) {
		pi->velocity.x /= 8;
		pi->velocity.z /= 8;
	}

	creature_integrate(obj,&accel);

	if (!is_flying(obj))
		vm_vec_zero(&pi->thrust);

	start_pos = obj->pos;

	for (;;) {

		// 0x9F67C
		if (vm_vec_mag(&pi->velocity) <= 10) {
			ail->retry_count = 0;
			break;
		}

		frame_vec.x = fixmul(pi->velocity.x,FrameTime);
		frame_vec.y = fixmul(pi->velocity.y,FrameTime);
		frame_vec.z = fixmul(pi->velocity.z,FrameTime);

		if (++tries > 5) {
			ail->retry_count = 1;
			break;
		}

		vm_vec_add(&new_pos,&obj->pos,&frame_vec);

		ignore_obj_list[n_ignore_objs] = -1;

		// The radius of the query is one fourth of the radius of the body
		// (0x9F605).
		fq.p0 = &obj->pos;
		fq.p1 = &new_pos;
		fq.startseg = obj->segnum;
		fq.rad = cyl_radius/4;
		fq.thisobjnum = objnum;
		fq.ignore_obj_list = ignore_obj_list;
		fq.flags = (DTU_ANIM_STATE(obj) == DAS_DIE) ? 0 : FQ_CHECK_OBJS;

		Fvi_dtu_body = 1;
		Fvi_dtu_rad2 = half_height;
		fate = find_vector_intersection(&fq,&hit);
		Fvi_dtu_body = 0;

		if (fate == HIT_WALL && hit.hit_side == WBOTTOM)
			fate = 5;		// the floor

		if (fate == HIT_NONE) {
			int segnum = find_point_seg(&new_pos,obj->segnum);

			if (segnum != -1) {
				obj->pos = new_pos;
				if (segnum != obj->segnum)
					obj_relink(objnum,segnum);
			}
			ail->retry_count = 0;
			ail->blocked_count = 0;
			ail->blocker = -1;
			break;
		}
		else if (fate == 5) {				// 0x6CD45
			vms_vector c1;
			fix feet = obj->pos.y - half_height, diff;

			obj->pos = start_pos;

			// A floor that is less than 5.5 above the feet lifts the
			// creature (0x6C186, see dtu_knee_high() in physics.c).
			compute_center_point_on_side(&c1,&Segments[hit.hit_side_seg],hit.hit_side);
			diff = c1.y - feet;
			if (diff > 0 && diff < DTU_KNEE_HIGH)
				pi->velocity.y = c1.y - obj->pos.y + half_height + DTU_KNEE_LIFT;
			else {
				fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

				vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,-d);
			}
		}
		else if (fate == HIT_WALL) {		// 0x9E88D
			fix d = vm_vec_dot(&pi->velocity,&hit.hit_wallnorm);

			vm_vec_scale_add2(&pi->velocity,&hit.hit_wallnorm,fixmul(-d,DTU_WALL_BOUNCE));
			obj->pos = start_pos;
			ail->retry_count = 1;
			break;
		}
		else if (fate == HIT_OBJECT) {	// 0x9F9C0 and 0x9FB56
			object *other = &Objects[hit.hit_object];

			obj->pos = start_pos;
			ail->retry_count = ai_tick(objnum,2);

			if (ail->blocker != hit.hit_object) {
				ail->blocker = hit.hit_object;
				ail->blocked_count = 1;
			}
			else if (!Dtu_fix_creature_move || Ref_ticks_now > 0)
				ail->blocked_count++;

			// The executable pushes scenery here if the strength of the
			// creature is above the value "pushable" (0x9FA41). Not in the port.

			collide_two_objects(obj,other,&hit.hit_pnt);

			if (obj->flags & OF_SHOULD_BE_DEAD)
				break;
			if (other->type == OBJ_ROBOT && n_ignore_objs < MAX_IGNORE_OBJS-1) {
				ignore_obj_list[n_ignore_objs++] = hit.hit_object;
				continue;
			}
			// The body stops at the player and at scenery.
			pi->velocity.x = pi->velocity.z = 0;
			break;
		}
		else {									// "bad p0"
			vm_vec_negate(&pi->velocity);
			ail->retry_count = 1;
			break;
		}
	}

	if (get_seg_masks(&obj->pos,obj->segnum,0).centermask != 0) {
		if (!update_object_seg(obj)) {
			int n = find_point_seg(&start_pos,obj->segnum);

			if (n != -1) {
				obj->pos = start_pos;
				obj_relink(objnum,n);
			}
		}
	}
}

//------------------------------------------------------------------------
// The AI

// 0x102B8. The AI state that belongs to the animation state.
static int anim_to_ai_state(object *obj)
{
	switch (DTU_ANIM_STATE(obj)) {
		case DAS_REST:			return DAIS_REST;
		case DAS_WALK:
		case DAS_SIDLE_R:
		case DAS_SIDLE_L:
		case DAS_RUN:			return is_hostile(obj) ? DAIS_SRCH : DAIS_NONE;
		case DAS_ATTACK:
		case DAS_ATTACK2:		return DAIS_ATAK;
		case DAS_FLINCH:		return DAIS_FLIN;
		case DAS_DIE:			return DAIS_DEAD;
	}
	return DAIS_NONE;
}

// 0x10148. The animation state that belongs to an AI state.
static int ai_to_anim_state(object *obj,int ai_state,fix dist)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int station = (obj->ctype.ai_info.behavior == DAIB_STATION);

	switch (ai_state) {
		case DAIS_SRCH:	return station ? DAS_STAND : DAS_WALK;
		case DAIS_LOCK:
			if (station)
				return DAS_STAND;
			if (dist > i2f(30) && ail->time_player_seen + F1_0/2 >= GameTime)
				return DAS_RUN;
			return DAS_WALK;
		case DAIS_FLIN:	return DAS_FLINCH;
		case DAIS_ATAK:	return (rand() & 0x100) ? DAS_ATTACK2 : DAS_ATTACK;
		case DAIS_DEAD:	return DAS_DIE;
	}
	return DAS_STAND;
}

// 0x106BA ai_turn_towards_vector. The creature keeps the y part of its
// forward vector.
static void turn_towards_vector(vms_vector *goal,object *obj,fix rate)
{
	vms_vector new_fvec;
	fix dot;

	new_fvec = *goal;
	new_fvec.y = obj->orient.fvec.y;

	dot = vm_vec_dot(&new_fvec,&obj->orient.fvec);

	if (dot < F1_0 - FrameTime/2 && rate > 0) {
		fix mag;

		vm_vec_scale(&new_fvec,fixdiv(FrameTime,rate));
		vm_vec_add2(&new_fvec,&obj->orient.fvec);
		new_fvec.y = obj->orient.fvec.y;
		mag = vm_vec_normalize(&new_fvec);
		if (mag < 0x100) {
			new_fvec = *goal;
			new_fvec.y = obj->orient.fvec.y;
		}
	}

	if (vm_vec_mag(&new_fvec) < 0x100)
		return;
	vm_vector_2_matrix(&obj->orient,&new_fvec,NULL,NULL);
	check_and_fix_matrix(&obj->orient);
}

// 0x11EC9. The AI adds direction * speed of the animation state to the
// velocity. A creature that does not fly moves in x and z only.
static void add_move(object *obj,vms_vector *dir)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v = *dir;

	if (!is_flying(obj))
		v.y = 0;
	vm_vec_normalize_quick(&v);
	vm_vec_copy_scale(&ail->move_add,&v,anim_speed(obj,-1));
	ail->has_move = 1;
}

// 0x11F32 move_towards_player. The creature turns to the direction at once.
static void move_towards(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v;

	if (obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK)
		if (!((Monster_info[obj->id].flags & MONSTER_F_FLYING) && Robot_info[obj->id].attack_type == 1))
			return;

	v.x = vec_to_player->x;
	v.y = obj->orient.fvec.y;
	v.z = vec_to_player->z;
	if (vm_vec_normalize(&v) > 0x100) {
		vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
		check_and_fix_matrix(&obj->orient);
	}
	ail->flags &= ~AILF_BACKWARD;
	add_move(obj,vec_to_player);
}

// 0x12547 move_away_from_player
static void move_away(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	vms_vector v;

	if (obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK || (ail->flags & AILF_AWAY))
		return;
	v = *vec_to_player;
	vm_vec_negate(&v);
	add_move(obj,&v);
}

// 0x125B9 ai_move_relative_to_player without the part that evades a
// weapon of the player.
static void move_relative_to_player(object *obj,fix dist,vms_vector *vec_to_player,fix circle_distance,int evade_only)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (obj->ctype.ai_info.behavior == DAIB_STATION || (obj->dtu.m_88 & DTU_OF_HELD))
		return;
	if (evade_only)
		return;

	if (dist < circle_distance || (ail->flags & AILF_AWAY)) {
		move_away(obj,vec_to_player);
		ail->flags |= AILF_AWAY;
	}
	else if (dist < circle_distance*2 || (ail->flags & AILF_AROUND)) {
		// 0x12000 move_around_player is not in the port: the creature stays.
		ail->flags |= AILF_AROUND;
	}
	else {
		move_towards(obj,vec_to_player);
		ail->flags = (ail->flags & ~(AILF_BACKWARD|AILF_TOWARD)) | AILF_TOWARD;
	}
}

// 0x10ADF. The morale check. The parts with values that the port does not
// have (the level of the creature 0xAE3BB, the level of the player, the
// function 0x359F4) count as 0.
static void morale_check(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];
	int A = mi->ai_values[0][Difficulty_level], B = mi->ai_values[1][Difficulty_level];
	int mor, roll;

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI || mi->max_hit_points <= 0)
		return;
	if (aip->GOAL_STATE == DAIS_DEAD || aip->CURRENT_STATE == DAIS_DEAD || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;

	if (mi->flags & MONSTER_F_UNDEAD)
		mor = (obj->dtu.m_1b0 & DTU_FLEEING) ? 0 : 20;
	else {
		int wound;

		mor = (300 - mi->iq) / 8;
		wound = 4 - (int)(((int64_t)obj->shields * 4) / i2f(mi->max_hit_points));
		if (wound < 0) wound = 0;
		if (wound > 4) wound = 4;
		mor -= wound;
		if (aip->behavior != DAIB_WANDER && aip->behavior != DAIB_HOME)
			mor += 3;
		mor += Monster_ext[obj->id].spell_level;
		mor += 2 * ail->awareness;
		mor += (Player_armor_class - mi->armor_class) / 2;
		mor += Current_level_num / 2;
	}

	roll = rand() % 19 + 2;

	if (roll <= mor) {
		obj->dtu.m_1b0 &= ~DTU_FLEEING;
		if (is_hostile(obj) || ail->target > 0) {
			ail->morale = B;
			ail->mode = DAIM_CHASE_OB;
			aip->GOAL_STATE = DAIS_SRCH;
			obj->dtu.m_1b0 |= DTU_HOSTILE;
		}
		else {
			ail->morale = (A + B) / 2;
			aip->GOAL_STATE = DAIS_NONE;
		}
	}
	else {
		ail->morale = A;
		ail->mode = DAIM_RUN_FROM;
		obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		obj->dtu.m_1b0 |= DTU_FLEEING;
		DTU_ANIM_STATE(obj) = DAS_RUN;
	}

	dtu_obj_log("morale: obj %d %s: number %d roll %d, %s",(int)(obj-Objects),dtu_object_name(obj),mor,roll,
		roll <= mor ? "passed" : "failed");
}

// The choice of step 25 point 4 of do_ai_frame (docs/re/ai.md): the goal
// state of a creature that wants to attack.
static void choose_attack_goal(object *obj,fix dist)
{
	ai_static *aip = &obj->ctype.ai_info;

	if (Robot_info[obj->id].attack_type == 1) {
		fix reach = obj->size + ConsoleObject->size + i2f(2);

		aip->GOAL_STATE = (dist <= reach) ? DAIS_ATAK : DAIS_LOCK;
	}
	else if (Monster_ext[obj->id].spell_level == 0)
		aip->GOAL_STATE = DAIS_ATAK;
}

// 0x11018. The animation of an active creature, computed from the game
// time. Returns 1 if the creature is dead and done.
static void ai_animation_step(object *obj,fix dist)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	polymodel *po = obj_model(obj);
	fix elapsed = GameTime - ail->anim_start;
	int s, first, last, n, old_frame, ended;
	fix st;

	if (po == NULL)
		return;
	if (elapsed < 0)
		elapsed = 0;

	// A hostile melee creature that is far from a player that it sees runs.
	if (is_hostile(obj) && aip->GOAL_STATE != DAIS_DEAD && DTU_ANIM_STATE(obj) != DAS_DIE &&
		 Robot_info[obj->id].attack_type == 1 && dist > i2f(30) && ail->time_player_seen + F1_0/2 >= GameTime) {
		if (aip->behavior != DAIB_STATION)
			DTU_ANIM_STATE(obj) = DAS_RUN;
		aip->GOAL_STATE = DAIS_LOCK;
	}

	old_frame = DTU_ANIM_FRAME(obj);
	(void)old_frame;
	s = DTU_ANIM_STATE(obj);
	first = Dtu_anim_first[s];
	last = Dtu_anim_first[s+1];
	n = last - first - 1;
	st = state_time(po,s);

	// The executable divides by the state time with no test. A state with
	// the time 0 (REST) or with no frames ends at once here.
	if (st > 0 && n >= 0) {
		int step = (int)((((int64_t)elapsed * ((int64_t)n << 16)) / st + 0x8000) >> 16);

		if (ail->flags & AILF_BACKWARD)
			DTU_ANIM_FRAME(obj) = last - 1 - step;
		else
			DTU_ANIM_FRAME(obj) = first + step;
	}
	else
		DTU_ANIM_FRAME(obj) = (ail->flags & AILF_BACKWARD) ? first - 1 : last;

	if (ail->flags & AILF_BACKWARD)
		ended = (DTU_ANIM_FRAME(obj) < first);
	else
		ended = (DTU_ANIM_FRAME(obj) >= last);

	if (!ended) {
		if (aip->CURRENT_STATE == aip->GOAL_STATE)
			return;
		if (aip->GOAL_STATE != DAIS_FLIN && !(ail->flags & AILF_HIT))
			return;
	}

	if (DTU_ANIM_STATE(obj) == DAS_DIE) {
		// The last frame stays (0x11350).
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_DIE+1] - 1;
		creature_is_corpse(obj);
		return;
	}

	// The end of the state (0x1137D)
	ail->flags &= ~(AILF_BACKWARD|AILF_AROUND|AILF_TOWARD|AILF_AWAY|AILF_HIT);
	aip->CURRENT_STATE = aip->GOAL_STATE;
	ail->swing_done = 0;

	s = ai_to_anim_state(obj,aip->CURRENT_STATE,dist);
	if (ail->mode == DAIM_RUN_FROM)
		s = DAS_RUN;
	else if (s == DAS_ATTACK || s == DAS_ATTACK2) {
		// ATTACK2 if the player is higher or lower by more than half of
		// the distance in x and z (0x113F9).
		fix dy = abs(ConsoleObject->pos.y - obj->pos.y);
		vms_vector p = ConsoleObject->pos;
		fix hd;

		s = DAS_ATTACK;
		p.y = obj->pos.y;
		hd = vm_vec_dist(&p,&obj->pos);
		if (hd < 0x100 || fixdiv(dy,hd) > F1_0/2)
			s = DAS_ATTACK2;
	}
	set_anim(obj,s);
	ail->anim_start = GameTime;
}

// The animation of a creature that is not active (0x14013). One call is
// one step of the executable.
static void idle_animation_tick(object *obj,int tenth)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int s = DTU_ANIM_STATE(obj);

	if (s < 0 || s > DAS_FIDGET) {
		set_anim(obj,DAS_STAND);
		s = DAS_STAND;
	}

	if ((obj->flags & OF_SHOULD_BE_DEAD) || s == DAS_WALK || s > DAS_SIDLE_L || tenth)
		DTU_ANIM_FRAME(obj)++;

	if (DTU_ANIM_FRAME(obj) == Dtu_anim_first[s+1]) {
		int div = Monster_info[obj->id].iq >> (ail->awareness + 1);
		int next = DAS_STAND;

		// The executable takes the remainder of FrameTime + objnum, which
		// is a random number where the frame time changes. With a divisor
		// of 0 it stops with a division error.
		if (div > 0) {
			if (Dtu_fix_idle_anim) {
				if ((rand() % div) == 0)
					next = DAS_FIDGET;
			}
			else if (((FrameTime + (int)(obj-Objects)) % div) == 0)
				next = DAS_FIDGET;
		}

		aip->GOAL_STATE = aip->CURRENT_STATE = DAIS_NONE;
		DTU_ANIM_GOAL(obj) = next;
		set_anim(obj,next);
	}

	aip->CURRENT_STATE = anim_to_ai_state(obj);
}

static void idle_animation(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];

	if (!Dtu_fix_idle_anim) {
		idle_animation_tick(obj,((Dtu_clock + objnum) % 10) == 0);
		if (Obj_log_frames)
			log_creature_state(obj,"idle step");
		return;
	}

	// One step for each reference frame time, and the tenth step is the
	// one that moves the states that are slow.
	ail->idle_acc += FrameTime;
	while (ail->idle_acc >= DTU_REF_FRAMETIME) {
		ail->idle_acc -= DTU_REF_FRAMETIME;
		ail->idle_ticks++;
		idle_animation_tick(obj,((ail->idle_ticks + objnum) % 10) == 0);
	}
	if (Obj_log_frames)
		log_creature_state(obj,"idle step");
}

// The test of do_ai_frame that makes a creature active (0x13CE0, and
// 0x14230 for a creature that is active).
static int wake_test(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	vms_vector v;
	fix dot;

	if (ail->target == -1 || (obj->dtu.m_88 & 4) || Player_is_dead || Dtu_player_dead)
		return 0;
	if (!brain_bit(obj->segnum,ConsoleObject->segnum))
		return 0;

	vm_vec_sub(&v,&ConsoleObject->pos,&obj->pos);
	vm_vec_normalize_quick(&v);
	dot = vm_vec_dot(&v,&obj->orient.fvec);

	// The player is behind the creature: it notices the player with a
	// chance that grows with its iq.
	if (dot <= 0) {
		if (obj->dtu.m_88 & 0x10)
			return 0;
		if (Dtu_fix_ai_ticks) {
			if ((rand() % 255) > Monster_info[obj->id].iq)
				return 0;
		}
		else if (((Dtu_clock + objnum + dot) % 255) > Monster_info[obj->id].iq)
			return 0;
	}

	return creature_ray_to_player(obj);
}

// 0x13BF4
void dtu_do_ai_frame(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	robot_info *ri;
	monster_info *mi;
	vms_vector vec_to_player;
	fix dist;
	int A, B, visible = -1, dying;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return;
	ri = &Robot_info[obj->id];
	mi = &Monster_info[obj->id];
	A = mi->ai_values[0][Difficulty_level];
	B = mi->ai_values[1][Difficulty_level];

	dying = (DTU_ANIM_STATE(obj) == DAS_DIE);

	if (dying)
		ail->activity = 2;
	else if (ail->activity != 2) {

		// The test that makes the creature active (0x13CE0)
		if (wake_test(obj)) {
			ail->target = is_hostile(obj) ? 0 : -1;
			ail->activity = 2;
			set_anim(obj,DAS_STAND);
			if (is_hostile(obj)) {
				ail->awareness = 4;
				ail->mode = DAIM_CHASE_OB;
				aip->CURRENT_STATE = DAIS_REST;
				aip->GOAL_STATE = DAIS_SRCH;
			}
			else {
				ail->awareness = 0;
				aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
			}
			dtu_obj_log("obj %d %s: sees the player",objnum,dtu_object_name(obj));
			morale_check(obj);
		}

		if (ail->activity != 2) {
			// 0x13F96: a creature that is not active stands on the floor
			// of its segment: the center of the bottom side plus the half
			// height of the body (0x746B3). The executable deletes the
			// creature if that point is outside of the mine ("pushed
			// critter %d through mine"). The port leaves it where it is.
			if (obj->control_type != CT_MORPH) {
				vms_vector c, new_pos;
				fix radius, half_height;

				dtu_get_body_size(obj,&radius,&half_height);
				compute_center_point_on_side(&c,&Segments[obj->segnum],WBOTTOM);
				new_pos = obj->pos;
				new_pos.y = c.y + half_height;
				if (find_point_seg(&new_pos,obj->segnum) >= 0)
					obj->pos = new_pos;
			}
			idle_animation(obj);
		}

		// 0x14134
		ail->time_player_seen = GameTime;
		ail->awareness_time = i2f(3);
		ail->anim_start = GameTime;
	}
	else if (ai_tick(objnum,20)) {
		// 0x14206: an active creature looks for the player again
		ail->activity = 0;
		if (wake_test(obj))
			ail->activity = 2;
		if (ail->activity == 0) {
			ail->anim_start = GameTime + FrameTime;
			aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
			DTU_ANIM_GOAL(obj) = DAS_REST;
			set_anim(obj,DAS_REST);
			dtu_obj_log("obj %d %s: lost the player",objnum,dtu_object_name(obj));
		}
	}

	// Regeneration (0x1440C) is not in the port.

	// 0x146D0: the distance to the player
	ail->time_since_processed += FrameTime;
	vm_vec_sub(&vec_to_player,&ConsoleObject->pos,&obj->pos);
	dist = vm_vec_mag(&vec_to_player);
	if (dist > 0x100)
		vm_vec_normalize(&vec_to_player);
	else
		vec_to_player = obj->orient.fvec;

	// 0x14825: the awareness goes down by one each 2.0 seconds
	if (ail->awareness) {
		ail->awareness_time -= FrameTime;
		if (ail->awareness_time <= 0) {
			ail->awareness_time = i2f(2);
			ail->awareness--;
		}
	}

	// 0x1487A
	if (ail->awareness >= 1 && ail->target != -1 && !(obj->dtu.m_1b0 & DTU_FLEEING))
		ail->mode = DAIM_CHASE_OB;
	if (aip->GOAL_STATE == DAIS_FLIN && aip->CURRENT_STATE == DAIS_FLIN)
		aip->GOAL_STATE = DAIS_LOCK;

	if (!dying && (obj->dtu.m_88 & DTU_OF_HELD))
		return;

	// 0x148D5
	if (aip->GOAL_STATE != DAIS_NONE || aip->CURRENT_STATE != DAIS_NONE)
		ai_animation_step(obj,dist);

	if (DTU_ANIM_STATE(obj) == DAS_DIE || obj->control_type != CT_AI) {
		log_creature_state(obj,"dying");
		return;
	}

	// Spell casting (0x1490B) is not in the port.

	// 0x1498B: a creature that is far away is not processed on each frame
	if (ail->awareness < 3) {
		if (dist > i2f(150) && ail->time_since_processed <= F1_0)
			return;
		if (dist > i2f(100) && ail->time_since_processed <= F1_0/2)
			return;
	}
	ail->time_since_processed = -((objnum & 3) * FrameTime) / 2;

	// 0x14A1C: a creature that is not active stands and looks at the player
	if (ail->activity == 0) {
		aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
		// The executable sets STAND and its first frame here (0x14A54).
		// A creature that is nearer than 100.0 comes here on each frame,
		// so its idle animation never leaves the first frame of STAND. A
		// creature that is farther away comes here each half second or
		// second and plays the idle animation between two visits.
		if (!Dtu_idle_anim_near)
			set_anim(obj,DAS_STAND);

		visible = player_is_visible(obj);
		if (visible)
			ail->time_player_seen = GameTime;
		if (visible || ail->previous_visibility)
			turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
		// else: a random turn (0x10763), not in the port
		ail->previous_visibility = visible;
		log_creature_state(obj,"idle");
		return;
	}

	// The door opener (0x14AF2) and the blocked move (0x14BA1) are not in
	// the port.

	// 0x15074: the mode
	visible = player_is_visible(obj);
	if (visible)
		ail->time_player_seen = GameTime;

	// The awareness of a creature goes up by the awareness events of the
	// executable (0x16055, 0x16366: noise of the player), which are not in
	// the port. In their place a hostile creature that sees the player has
	// the awareness 1 at least. (guess)
	if (visible && is_hostile(obj) && ail->awareness < 1) {
		ail->awareness = 1;
		ail->awareness_time = i2f(3);
	}

	switch (ail->mode) {

		case DAIM_RUN_FROM:			// 0x14D65, 0x123EF
			move_away(obj,&vec_to_player);
			break;

		case DAIM_CHASE_OB:			// 0x150DD
		case DAIM_WANDER:
		default: {
			fix circle = ConsoleObject->size;

			if (!is_hostile(obj))
				break;
			if (ri->attack_type != 1)
				circle = ri->turn_time[Difficulty_level] + ConsoleObject->size;		// the third group of $MONSTER_AI

			if (aip->CURRENT_STATE == DAIS_REST && aip->GOAL_STATE == DAIS_REST && visible && ail->morale >= B)
				aip->GOAL_STATE = DAIS_SRCH;
			else if (aip->CURRENT_STATE != DAIS_REST && aip->GOAL_STATE != DAIS_REST) {
				move_relative_to_player(obj,dist,&vec_to_player,circle,0);
				if (ail->morale < B) {
					// "Bad moralle (%d)"
					ail->activity = 0;
					morale_check(obj);
				}
			}
			break;
		}

		case DAIM_STILL:				// 0x154BC
			if ((dist < i2f(120) + i2f(20)*Difficulty_level || ail->awareness >= 3) && is_hostile(obj)) {
				if ((visible || ail->previous_visibility) && ail->morale >= B)
					turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
				if (visible && ri->attack_type == 1)
					move_relative_to_player(obj,dist,&vec_to_player,0,0);
			}
			else if (visible)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			break;
	}

	// do_firing_stuff (0x12916)
	if (visible && is_hostile(obj) && ail->morale >= B) {
		fix dot = vm_vec_dot(&obj->orient.fvec,&vec_to_player);

		if (dot >= 3*F1_0/4 && aip->GOAL_STATE <= DAIS_ATAK && aip->GOAL_STATE != DAIS_FLIN) {
			choose_attack_goal(obj,dist);
			if (ail->awareness < 1) {
				ail->awareness = 1;
				ail->awareness_time = i2f(3);
			}
		}
		else if (dot >= F1_0/2 && aip->GOAL_STATE <= DAIS_SRCH)
			aip->GOAL_STATE = DAIS_LOCK;
	}

	ail->previous_visibility = visible;

	// 0x15876: the choice of the goal state
	if (ail->awareness) {
		if (ail->awareness == 5) {
			aip->GOAL_STATE = (aip->CURRENT_STATE == DAIS_FLIN) ? DAIS_LOCK : DAIS_FLIN;
			ail->awareness = 2;
			ail->awareness_time = i2f(3);
		}
		if (aip->CURRENT_STATE == DAIS_DEAD)
			aip->GOAL_STATE = DAIS_DEAD;
		if ((obj->dtu.m_88 & 0x100) || ail->morale <= A)
			ail->mode = DAIM_RUN_FROM;
		if ((aip->GOAL_STATE == DAIS_REST || aip->GOAL_STATE == DAIS_SRCH || aip->GOAL_STATE == DAIS_LOCK || aip->GOAL_STATE == DAIS_ATAK) && ail->morale >= B)
			choose_attack_goal(obj,dist);
		if (aip->GOAL_STATE == DAIS_FLIN && aip->CURRENT_STATE == DAIS_FLIN)
			aip->GOAL_STATE = DAIS_LOCK;
		if (aip->GOAL_STATE == DAIS_RECO && aip->CURRENT_STATE == DAIS_RECO && ail->morale >= B)
			choose_attack_goal(obj,dist);
	}

	if (aip->GOAL_STATE == DAIS_FLIN) {
		log_creature_state(obj,"flinch");
		return;
	}

	// 0x15B2F: the current state
	switch (aip->CURRENT_STATE) {

		case DAIS_NONE:
			if (is_hostile(obj))
				aip->GOAL_STATE = DAIS_SRCH;
			break;

		case DAIS_REST:
			if (aip->GOAL_STATE == DAIS_REST) {
				if (visible && ail->morale >= B)
					choose_attack_goal(obj,dist);
				else if (!is_hostile(obj))
					aip->GOAL_STATE = DAIS_NONE;
			}
			break;

		case DAIS_SRCH:
		case DAIS_LOCK:
		case DAIS_RECO:
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			break;

		case DAIS_ATAK:
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[(ri->attack_type == 1) ? Difficulty_level : 4]);
			// ai_do_actual_firing_stuff (0x1396E): a melee creature hits in
			// weapon_test(). The shot of a ranged creature (ai_fire 0x11B06)
			// is not in the port. The goal is the same in both cases.
			if (is_hostile(obj))
				aip->GOAL_STATE = DAIS_RECO;
			break;
	}

	log_creature_state(obj,"ai");
}

//------------------------------------------------------------------------
// Collisions

// The four pickup functions of collide.c (0x2805B misc, 0x283DC armor,
// 0x28273 spell, 0x284E0 weapon).
static void pick_up_item(object *player,object *item)
{
	item_info *it = item_table_entry(item->type,item->id);
	int count = (signed char) item->dtu.m_1d5;
	int index;

	if (player->id != Player_num || (item->flags & OF_SHOULD_BE_DEAD) || it == NULL)
		return;

	// A weapon is an item only if it does not move (0x28502).
	if (item->type == OBJ_WEAPON && (item->render_type != RT_ITEM || item->lifeleft != IMMORTAL_TIME))
		return;

	// Gold is misc id 1 (0x2806B). The amount is the hit points.
	if (item->type == OBJ_MISC && item->id == 1) {
		int amount = f2i(item->shields);

		Dtu_gold += amount;
		HUD_init_message("You pick up %d gold.",amount);
		item->flags |= OF_SHOULD_BE_DEAD;
		dtu_obj_log("pickup: obj %d, %d gold",(int)(item-Objects),amount);
		return;
	}

	if (count == 0) {
		if (item_is_stackable(it) || item->type == OBJ_ARMOR)
			count = 1;
		else
			count = tbl_int(it->record,"uses",1);
	}
	if (count == 0 && !(item->type == OBJ_MISC && item->id == 102))
		return;
	if (count <= 0)
		count = 1;

	index = dtu_inventory_add(item->type,item->id,count);
	if (index >= 0) {
		item->flags |= OF_SHOULD_BE_DEAD;
		HUD_init_message("You pick up %s.",it->name);
		dtu_obj_log("pickup: obj %d type %d id %d '%s' count %d, inventory entry %d",(int)(item-Objects),
			item->type,item->id,it->name,count,index);
	}
	else {
		static fix last_message = 0;

		if (GameTime - last_message > F1_0*2 || GameTime < last_message) {
			HUD_init_message("You cannot carry %s.",it->name);
			last_message = GameTime;
		}
	}
}

int dtu_collide_two_objects(object *a, object *b, vms_vector *collision_point)
{
	object *t;

	if (!Dtu_objects_enabled)
		return 0;

	if (a->flags & OF_SHOULD_BE_DEAD)
		return 1;

	// the player is "a"
	if (b->type == OBJ_PLAYER && a->type != OBJ_PLAYER) {
		t = a; a = b; b = t;
	}

	if (a->type == OBJ_PLAYER) {
		switch (b->type) {
			case OBJ_MISC:
			case OBJ_ARMOR:
			case OBJ_SPELL:
			case OBJ_WEAPON:
				pick_up_item(a,b);
				break;
			case OBJ_ROBOT:
				// collide_robot_and_player (0x26FC1): a hostile creature
				// that the player touches is alerted. The bump (0x2627C) is
				// not in the port: the bodies stop in the movement code.
				if (is_hostile(b) && Ail[b-Objects].awareness < 3) {
					Ail[b-Objects].awareness = 3;
					Ail[b-Objects].awareness_time = i2f(3);
				}
				break;
			case OBJ_DRESSING:
				// 0x25FB0: the player pushes scenery. Not in the port.
				break;
		}
	}

	(void)collision_point;
	return 1;
}

//------------------------------------------------------------------------
// The player

// The attack of the player. The executable starts it in pweapon.c and
// plays the states 6, 7, 11 and 12 of the player model with the function
// 0x4450C. The port plays state ATTACK on the player object, which is not
// drawn, and tests the weapon as the executable does (weapon_test).
int dtu_player_attack(void)
{
	object *obj = ConsoleObject;

	if (!Dtu_objects_enabled || Player_is_dead || Dtu_player_dead || obj->type != OBJ_PLAYER)
		return 0;
	if (DTU_ANIM_STATE(obj) == DAS_ATTACK)
		return 0;

	DTU_ANIM_STATE(obj) = DAS_ATTACK;
	DTU_ANIM_GOAL(obj) = DAS_STAND;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_ATTACK];
	Player_anim_start = GameTime;
	Player_swing_done = 0;
	dtu_obj_log("player: starts an attack");
	return 1;
}

// 0x4450C for the attack state
static void player_animation(void)
{
	object *obj = ConsoleObject;
	int s = DTU_ANIM_STATE(obj);
	fix st = F1_0, elapsed;
	polymodel *po = NULL;

	if (obj->type != OBJ_PLAYER || s != DAS_ATTACK)
		return;

	if (N_player_types > 0 && Player_type_info[0].n_models > 0 && Player_type_info[0].model_nums[0] >= 0)
		po = &Polygon_models[Player_type_info[0].model_nums[0]];
	if (po && po->state_times[s] > 0)
		st = po->state_times[s];

	elapsed = GameTime - Player_anim_start;
	DTU_ANIM_FRAME(obj) = Dtu_anim_first[s] +
		(int)((((int64_t)elapsed * ((int64_t)(Dtu_anim_first[s+1] - Dtu_anim_first[s]) << 16)) / st + 0x8000) >> 16);

	if (DTU_ANIM_FRAME(obj) >= Dtu_anim_first[s+1]) {
		DTU_ANIM_STATE(obj) = DTU_ANIM_GOAL(obj) = DAS_STAND;
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_STAND];
		Player_swing_done = 0;
		return;
	}

	weapon_test(obj);
}

// The option -testpos X,Y,Z[,HEADING]: puts the player at a position of the
// level at the first frame, for tests and screenshots. The heading is in
// degrees, 0 looks along +z, 90 along +x.
static void test_place_player(void)
{
	int t = FindArg("-testpos");
	float x, y, z, h = 0;
	vms_vector pos;
	vms_angvec a;
	int seg;

	Test_pos_pending = 0;
	if (!t || t+1 >= Num_args)
		return;
	if (sscanf(Args[t+1],"%f,%f,%f,%f",&x,&y,&z,&h) < 3)
		return;
	pos.x = fl2f(x); pos.y = fl2f(y); pos.z = fl2f(z);
	seg = find_point_seg(&pos,-1);
	if (seg < 0) {
		printf("-testpos: the point is in no segment\n");
		return;
	}
	ConsoleObject->pos = pos;
	obj_relink(ConsoleObject-Objects,seg);
	a.p = a.b = 0;
	a.h = (fixang)(int)(h * 65536.0 / 360.0);
	vm_angles_2_matrix(&ConsoleObject->orient,&a);
	vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
}

void dtu_objects_frame(void)
{
	if (!Dtu_objects_enabled)
		return;

	if (Test_pos_pending)
		test_place_player();

	// 0x4D81A
	Dtu_clock += FrameTime;
	if (Dtu_clock >= i2f(3600))
		Dtu_clock -= i2f(3600);

	Ref_tick_acc += FrameTime;
	Ref_ticks_now = Ref_tick_acc / DTU_REF_FRAMETIME;
	Ref_tick_acc -= Ref_ticks_now * DTU_REF_FRAMETIME;
	if (Ref_ticks_now > 8)
		Ref_ticks_now = 8;
	Ref_tick_count += Ref_ticks_now;

	player_animation();
}
