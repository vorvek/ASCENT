/*
 * ASCENT: new file. The encounter with Tamaramagil Swiftclaw, the adult
 * red dragon of DROW2 (level 21 of DTU.MSN). See dtudragon.h and
 * docs/re/dragon.md. Nothing of this file is in DTUGAME.EXE: 1.3 has her
 * creature type, her model and her dialogue, and no object for her.
 *
 * The parts:
 * - the model: dragn2.pof is larger than her room in each direction. It is
 *   drawn at DR_SCALE of its size (polyobj.h), and the arrays of the model
 *   are scaled and given one body center and one collision cylinder.
 * - her object, the hatchlings, the keys, the doors, the treasure
 *   (dtu_dragon_level_setup, dtu_dragon_level_rules).
 * - her AI (dragon_ai): she rests while she is peaceful; hostile, she
 *   turns to the player, walks in a small area around the middle of the
 *   room, and attacks with two claws, a bite or her breath.
 * - the events of the level (dtu_dragon_frame): the lever, the key of the
 *   alcove of the lever, the door of her hoard.
 * - the magic resistance and the text of her dialogue.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "args.h"
#include "dtuobj_int.h"
#include "game.h"
#include "player.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "fvi.h"
#include "fireball.h"
#include "vclip.h"
#include "gauges.h"
#include "physics.h"
#include "digi.h"
#include "dtuopts.h"
#include "dtulang.h"
#include "dtutrig.h"
#include "dtuscript.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtudisp.h"
#include "dtuxp.h"
#include "dtulog.h"
#include "dtudragon.h"

//------------------------------------------------------------------------
// The facts of the level (DROW2.LVL, DROW2.TIG, DROW2.TAP, checked with
// tools/lvl.py and tools/sidefiles.py; docs/re/dragon.md)

#define DR_LEVEL			21			// Dtu_script_level and the level index of DTU.MSN
#define DR_SCRIPT			210		// GABE record 210, her dialogue
#define DR_KEY_HOARD		42			// MISC 42, the Dragon Key of trigger 27 (the door of the hoard)
#define DR_KEY_ALCOVE	43			// MISC 43, a Dragon Key that no level uses: the key of the alcove
#define DR_KEY_ALCOVE_NAME	"Jailer's Key"

// The global flags of the port (the scripts of 1.3 use 1 to 160)
#define DR_GF_PLACED		241		// the key and the treasure are in the level
#define DR_GF_LEVER		242		// the lever is thrown: the hatchery is open

// The room: x -90 to 90, z -197.5 to -7.5, floor -110, ceiling -45
#define DR_FLOOR			i2f(-110)
#define DR_CENTER_X		0
#define DR_CENTER_Z		fl2f(-102.5)
#define DR_LEASH_X		i2f(18)		// her position stays this near to the middle
#define DR_LEASH_Z		i2f(24)

#define DR_LEVER_TRIGGER	20
#define DR_LEVER_TRAP		12			// opens the ceiling hatch (segment 445 side 1)
#define DR_LEVER_OBJECT		42			// the object of script 213 (the rule 0x5A855 of 1.3)

// The doors: the wall of each side of a door
static const short Dr_alcove[2][2] = { { 481, 0 }, { 482, 5 } };		// walls 133 and 132
static const short Dr_hatchery[2][2] = { { 398, 4 }, { 790, 5 } };	// walls 138 and 139
static const short Dr_hoard[2][2] = { { 449, 4 }, { 783, 5 } };		// walls 136 and 137
#define DR_ALCOVE_SEG	481			// the end of the corridor in front of the alcove

static const short Dr_hatch[4] = { 48, 49, 61, 63 };		// the hatchlings (type hatchlng)
static const short Dr_hatch_pos[4][2] = { { 135, -88 }, { 165, -88 }, { 135, -117 }, { 165, -117 } };
// The segments of her room where the level file has the four: they fly
// there when the hatchery opens
static const short Dr_hatch_goal[4] = { 430, 436, 471, 391 };

// The key of the alcove: on the floor of cell 407 (x 10 to 30, z -247.5 to
// -227.5), in the corner that is far from the door of the cell room
#define DR_KEY_X			fl2f(26.5)
#define DR_KEY_Z			fl2f(-244.0)

//------------------------------------------------------------------------
// The treasure of the hoard that the port adds (segments 783 to 789, 782
// and 409: x -180 to -120, z -132.5 to -72.5, floor -110). One line for
// each item: type, id, count (for gold: the amount), x, z. A line with the
// type -1 is a free place: a later task adds an item there (a ring of
// regeneration, a belt of dwarvenkind, plate mail of fire resistance) by
// writing the type and the id of the item into the line.
// The items of the level file stay (objects 50 to 60: 3 diamonds, Plate
// Mail +2, Chain Mail +2, Bracers AC4, Leather +2, Bastard Sword +1, Mace
// +2, Dagger +2, Scimitar +1).
#define DR_GOLD_ID		1			// MISC 1: gold, the amount is in the hit points

static const struct { short type, id, count, x, z; } Dr_treasure[] = {
	{ OBJ_WEAPON, 71, 1, -134, -126 },		// Flame Tongue
	{ OBJ_ARMOR,  11, 1, -146, -126 },		// Plate Mail +3
	{ OBJ_ARMOR,  38, 1, -166, -126 },		// Girdle of Hill Giant Strength
	{ OBJ_ARMOR,  17, 1, -174, -108 },		// Ring of Coolness
	{ OBJ_MISC,   76, 2, -174,  -96 },		// Potion of Fire Resistance
	{ OBJ_MISC,    3, 4, -166,  -79 },		// Diamond
	{ OBJ_MISC,    3, 3, -146,  -79 },		// Diamond
	{ OBJ_MISC, DR_GOLD_ID, 4000, -134, -79 },	// gold
	{ OBJ_MISC, DR_GOLD_ID, 3000, -154, -108 },	// gold
	{ -1, 0, 0, -154,  -96 },					// free: a ring of regeneration
	{ -1, 0, 0, -140,  -96 },					// free: a belt of dwarvenkind
	{ -1, 0, 0, -140, -108 },					// free: plate mail of fire resistance
};
#define DR_N_TREASURE	((int) (sizeof(Dr_treasure) / sizeof(Dr_treasure[0])))

//------------------------------------------------------------------------
// Her values: a red dragon of age 6 of AD&D 2nd edition (Monstrous Manual)

#define DR_HIT_POINTS	90
#define DR_ARMOR_CLASS	(-5)
#define DR_THAC0			7
#define DR_EXPERIENCE	15000
#define DR_MAGIC_RESIST	35			// percent
#define DR_CLAW_SIDES	10			// 2 claws, 1d10 each
#define DR_BITE_DICE		3			// the bite, 3d10
#define DR_BITE_SIDES	10
#define DR_BITE_CHANCE	30			// percent of the melee actions in which she bites
#define DR_BREATH_DICE	12			// the breath, 12d10+6, a save against breath for half
#define DR_BREATH_SIDES	10
#define DR_BREATH_BONUS	6

// The model
#define DR_SCALE			0x6666		// 0.4
#define DR_CYL_RADIUS	i2f(21)		// her collision cylinder: from the hips to the tip of the snout
#define DR_CYL_HALF		i2f(12)		// from the floor to 24 above it
#define DR_CYL_AHEAD		i2f(11)		// its axis and her position, in front of the origin of the model

// The frames of the model (Dtu_anim_first: ATTACK 44 to 59, the swipe of
// the right front claw, hit frame 56; ATTACK2 60 to 75, she rears and
// lunges, hit frame 72; the head is highest in the frames 64 to 66)
#define DR_CLAW_FRAME	56
#define DR_BITE_FRAME	72
#define DR_REAR_FIRST	60
#define DR_REAR_TOP		65

// The times, seconds of game time
#define DR_MELEE_TIME	F1_0			// one attack animation
#define DR_RECOVER_TIME	F1_0			// the pause after an attack: one melee action each 2 seconds
#define DR_WIND_TIME		(2*F1_0)		// the wind-up of the breath: she rears, flames at her jaws
#define DR_AIM_TIME		F1_0			// she turns with the player for the first part of the wind-up, then the aim is fixed
#define DR_FIRE_TIME		F1_0			// the stream
#define DR_FIRE_END		(F1_0*6/10)	// she comes down
#define DR_BREATH_PAUSE	(15*F1_0)	// from the end of a breath to the next one
#define DR_BREATH_FIRST	(4*F1_0)		// from the start of the fight to the first breath

#define DR_TURN_PEACE	0x1000		// fixang in a second: 22.5 degrees
#define DR_TURN_FIGHT	0x5000		// 112.5 degrees
#define DR_TURN_AIM		0x3000		// 67.5 degrees
#define DR_WALK_SPEED	i2f(12)

// The reach of an attack, from the weapon box of the hit frame to the
// axis of the body of the player: the radius of the box, the radius of the
// player and this margin
#define DR_CLAW_MARGIN	i2f(20)
#define DR_BITE_MARGIN	i2f(10)
#define DR_BITE_RADIUS	i2f(6)

// The breath: a cone from her jaws
#define DR_BREATH_RANGE	i2f(120)
#define DR_BREATH_TAN	0x5D00		// the half angle, 20 degrees (tan 0.364)
#define DR_BREATH_MIN	i2f(8)

#define DM_REST		0		// peaceful
#define DM_IDLE		1		// hostile: turns, walks, chooses
#define DM_CLAW		2
#define DM_BITE		3
#define DM_WIND		4
#define DM_FIRE		5
#define DM_FIRE_END	6
#define DM_RECOVER	7

static char *Dr_mode_names[] = { "rest", "idle", "claws", "bite", "breath wind-up", "breath", "breath end", "recover" };

static struct {
	int		objnum, sig;
	int		mode;
	fix		t0;					// the game time of the start of the mode
	fixang	heading;
	int		hit_done;
	fix		next_breath;		// the game time from which she can breathe
	int		aim_fixed;
	vms_vector aim, mouth, dir;
	int		mouth_seg;
	fix		puff_acc;
	int		force;				// test: the next action
	int		hoard_seen;
	fix		door_time;
} D;

static int Dr_type = -1, Dr_model = -1;
static vms_vector Dr_c0;			// the body center of each frame (scaled): the origin of the model is at position - c0
static fix Dr_lift[128];			// the lift of the drawn model for each frame, -1 not known

extern int Dtu_level_has_state[];

// dtuobj_ai.c: the path of statement E5 of a script
void dtu_script_walk_segment(object *obj,int seg);
void dtu_script_walk_start(object *obj,int fix);

//------------------------------------------------------------------------

int dtu_dragon_on(void)
{
	return Dtu_restored_dragon && Dtu_fix_tamaramagil;
}

static int in_level(void)
{
	return Dtu_script_level == DR_LEVEL;
}

int dtu_dragon_is(object *obj)
{
	return obj && obj->type == OBJ_ROBOT && in_level() && (obj->dtu.m_1b0 & DTU_SCRIPT_MASK) == DR_SCRIPT && dtu_dragon_on();
}

static object *dragon_object(void)
{
	int i;

	if (D.objnum > 0 && D.objnum <= Highest_object_index && Objects[D.objnum].signature == D.sig && dtu_dragon_is(&Objects[D.objnum]))
		return &Objects[D.objnum];
	for (i=1;i<=Highest_object_index;i++)
		if (dtu_dragon_is(&Objects[i]))
			return &Objects[i];
	return NULL;
}

static int dragon_alive(object *obj)
{
	return obj && obj->shields >= F1_0 && DTU_ANIM_STATE(obj) != DAS_DIE && !(obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED));
}

static wall *side_wall(int seg, int side)
{
	int wn;

	if (seg < 0 || seg > Highest_segment_index)
		return NULL;
	wn = Segments[seg].sides[side].wall_num;
	return (wn >= 0 && wn < Num_walls) ? &Walls[wn] : NULL;
}

static object *hatchling(int k)
{
	object *o;

	if (Dr_hatch[k] > Highest_object_index)
		return NULL;
	o = &Objects[Dr_hatch[k]];
	if (o->type != OBJ_ROBOT || o->control_type != CT_AI || o->id >= N_robot_types || stricmp(Monster_info[o->id].name,"hatchlng")
			|| (o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) || o->shields < F1_0)
		return NULL;
	return o;
}

// A message of the level, as the message of a trigger
static void say(char *text)
{
	dtu_display_add_trigger_message(text);
	dtu_obj_log("dragon: message \"%s\"",text);
}

//------------------------------------------------------------------------
// The model

static void scale_vecs(vms_vector *v, int n)
{
	int i;

	if (v)
		for (i=0;i<n;i++)
			vm_vec_scale(&v[i],DR_SCALE);
}

static void scale_fixes(fix *v, int n)
{
	int i;

	if (v)
		for (i=0;i<n;i++)
			v[i] = fixmul(v[i],DR_SCALE);
}

// The arrays of dragn2.pof are changed one time for a load of the model
// (the body radius of the rest pose is 0 in the file and not 0 after it):
// - Each frame gets one body center: the point DR_CYL_AHEAD in front of
//   the origin of the model, at the height of the center of frame 0. The
//   position of an object is the body center of its frame (0x729C1), and
//   the centers of her frames are up to 34 apart (the tail curls), so the
//   model would jump at each change of the animation. The weapon centers
//   are relative to the body center and move with it. The center is the
//   axis of her collision cylinder, because the swing of the player and
//   the use ray test a cylinder about the position of the object (0x4F7D5)
//   and not about the torso center.
// - All lengths are scaled by DR_SCALE.
// - The collision cylinder (the torso arrays): the file has a torso of
//   radius 70.8, as wide as her wings. It becomes a cylinder around her
//   body and shoulders that stands on the floor.
static void model_setup(polymodel *po, int model_num)
{
	int F = po->n_frames, f, die = Dtu_anim_first[DAS_DIE], die_end = Dtu_anim_first[DAS_DIE+1];
	vms_vector c0, d;

	if (po->body_radius[0] != 0)
		return;
	c0 = po->body_center[1];
	c0.x = 0;
	c0.z = fixdiv(DR_CYL_AHEAD,DR_SCALE);
	for (f=0;f<F;f++) {
		vm_vec_sub(&d,&po->body_center[f+1],&c0);
		if (po->weapon_center)
			vm_vec_add2(&po->weapon_center[f],&d);
		if (po->weapon2_center)
			vm_vec_add2(&po->weapon2_center[f],&d);
		if (po->torso_center)
			vm_vec_add2(&po->torso_center[f],&d);
		po->body_center[f+1] = c0;
	}
	scale_vecs(po->body_center,F+1);
	scale_vecs(po->box1_min,F+1);	scale_vecs(po->box1_max,F+1);
	scale_vecs(po->box2_min,F);		scale_vecs(po->box2_max,F);
	scale_vecs(po->box4_min,F);		scale_vecs(po->box4_max,F);
	scale_vecs(po->box8_min,F);		scale_vecs(po->box8_max,F);
	scale_vecs(po->frame_eye,F);
	scale_fixes(po->body_half_height,F+1);
	scale_fixes(po->body_radius,F+1);
	scale_vecs(po->torso_center,F);
	scale_fixes(po->torso_half_height,F);
	scale_fixes(po->torso_radius,F);
	scale_vecs(po->weapon_center,F);
	scale_fixes(po->weapon_radius,F);
	scale_vecs(po->weapon2_center,F);
	scale_fixes(po->weapon2_radius,F);
	scale_vecs(po->gun_points,2);
	vm_vec_scale(&po->mins,DR_SCALE);
	vm_vec_scale(&po->maxs,DR_SCALE);
	po->rad = fixmul(po->rad,DR_SCALE);

	// the cylinder, in the frames before the death and after it (the
	// frames of the death keep the torso of the file: the body lies)
	if (po->torso_center && po->torso_radius && po->torso_half_height)
		for (f=0;f<F;f++) {
			if (f >= die && f < die_end)
				continue;
			po->torso_center[f].x = 0;
			po->torso_center[f].y = DR_CYL_HALF - po->body_half_height[0];
			po->torso_center[f].z = 0;
			po->torso_radius[f] = DR_CYL_RADIUS;
			po->torso_half_height[f] = DR_CYL_HALF;
		}
	po->body_radius[0] = DR_CYL_RADIUS;
	dtu_obj_log("dragon: model %d at %.2f of its size: body center %.2f %.2f %.2f, half height %.2f, cylinder radius %.2f half height %.2f at %.2f %.2f %.2f",
		model_num,f2fl(DR_SCALE),f2fl(po->body_center[1].x),f2fl(po->body_center[1].y),f2fl(po->body_center[1].z),
		f2fl(po->body_half_height[0]),f2fl(DR_CYL_RADIUS),f2fl(DR_CYL_HALF),
		po->torso_center ? f2fl(po->torso_center[0].x) : 0.0,po->torso_center ? f2fl(po->torso_center[0].y) : 0.0,
		po->torso_center ? f2fl(po->torso_center[0].z) : 0.0);
}

// The lift of the drawn model in a frame: Dtu_fix_creature_feet draws the
// lowest point of the frame on the floor (feet_lift_frame of dtuobj.c)
static fix frame_lift(int frame)
{
	polymodel *po;
	vms_vector mn, mx;

	if (Dr_model < 0 || frame < 0 || frame >= 128)
		return 0;
	if (Dr_lift[frame] < 0) {
		po = &Polygon_models[Dr_model];
		Dr_lift[frame] = 0;
		if (Dtu_fix_creature_feet && polyobj_frame_points_box(Dr_model,frame,&mn,&mx) && mn.y < -po->body_half_height[0])
			Dr_lift[frame] = -po->body_half_height[0] - mn.y;
	}
	return Dr_lift[frame];
}

// The place in the world of a point that is relative to her position
// (the body center), in the drawn frame
static void body_point(object *obj, int frame, vms_vector *rel, vms_vector *out)
{
	vms_matrix m;
	vms_vector w;

	vm_copy_transpose_matrix(&m,&obj->orient);
	vm_vec_rotate(&w,rel,&m);
	vm_vec_add(out,&obj->pos,&w);
	out->y += frame_lift(frame);
}

// The place in the world of a point of the model, in the units of the file
static void model_point(object *obj, int frame, double x, double y, double z, vms_vector *out)
{
	vms_vector p;

	p.x = fixmul(fl2f(x),DR_SCALE) - Dr_c0.x;
	p.y = fixmul(fl2f(y),DR_SCALE) - Dr_c0.y;
	p.z = fixmul(fl2f(z),DR_SCALE) - Dr_c0.z;
	body_point(obj,frame,&p,out);
}

// Her jaws in the frames 60 to 66 of ATTACK2 (the gun point of submodel
// 15, tools/model.py): she rears, the head goes up and back
static void mouth_point(object *obj, int frame, vms_vector *out)
{
	static const double pts[4][4] = {
		{ 60, 1.5, 0.8, 70.6 }, { 62, 1.6, -8.7, 66.5 }, { 64, -0.3, 54.0, 24.9 }, { 66, -0.2, 56.4, 12.4 } };
	int i;
	double t;

	if (frame <= 60)
		i = 0, t = 0;
	else if (frame >= 66)
		i = 2, t = 1;
	else {
		i = (frame - 60) / 2;
		t = (frame - pts[i][0]) / 2.0;
	}
	model_point(obj,frame,pts[i][1] + (pts[i+1][1] - pts[i][1]) * t,pts[i][2] + (pts[i+1][2] - pts[i][2]) * t,
		pts[i][3] + (pts[i+1][3] - pts[i][3]) * t,out);
}

//------------------------------------------------------------------------
// The setup of a level

static void type_setup(int type)
{
	monster_info *mi = &Monster_info[type];

	if (mi->max_hit_points != DR_HIT_POINTS || mi->armor_class != DR_ARMOR_CLASS || mi->thaco != DR_THAC0 ||
		 Robot_info[type].score_value != DR_EXPERIENCE)
		dtu_obj_log("dragon: type %d %s: hit points %d -> %d, armor class %d -> %d, THAC0 %d -> %d, experience %d -> %d, fire immune %d",
			type,mi->name,mi->max_hit_points,DR_HIT_POINTS,mi->armor_class,DR_ARMOR_CLASS,mi->thaco,DR_THAC0,
			Robot_info[type].score_value,DR_EXPERIENCE,(mi->flags & MONSTER_F_FIRE_IMMUNE) != 0);
	mi->max_hit_points = DR_HIT_POINTS;
	mi->armor_class = DR_ARMOR_CLASS;
	mi->thaco = DR_THAC0;
	mi->flags |= MONSTER_F_FIRE_IMMUNE;
	Robot_info[type].score_value = DR_EXPERIENCE;
}

// The name of the key of the alcove. The three Dragon Keys of the table
// (MISC 42, 43, 44) have one name; 43 gets its own in each level, so that
// the inventory shows it everywhere.
static void key_name(void)
{
	item_info *it = item_table_entry(OBJ_MISC,DR_KEY_ALCOVE);

	if (it && !stricmp(it->name,"Dragon Key") && strlen(DR_KEY_ALCOVE_NAME) < sizeof(it->name))
		strcpy(it->name,DR_KEY_ALCOVE_NAME);
}

void dtu_dragon_level_setup(int has_state)
{
	int i, type = -1, seg, objnum, pass;
	vms_vector pos;
	object *obj;
	polymodel *po = NULL;

	memset(&D,0,sizeof(D));
	D.objnum = -1;
	for (i=0;i<128;i++)
		Dr_lift[i] = -1;
	Polyobj_scale_model = -1;
	Polyobj_scale = F1_0;
	Dr_type = Dr_model = -1;

	if (!dtu_dragon_on()) {
		// With the setting off the level has no dragon. An object of a level
		// state that was saved with the setting on goes away.
		if (in_level())
			for (i=1;i<=Highest_object_index;i++)
				if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & DTU_SCRIPT_MASK) == DR_SCRIPT &&
					 Objects[i].id < N_robot_types && !stricmp(Monster_info[Objects[i].id].name,"dragon")) {
					dtu_obj_log("dragon: the setting is off, obj %d goes away",i);
					dtu_creature_remove(&Objects[i],"the setting The dragon Tamaramagil is off");
				}
		return;
	}
	key_name();
	if (!in_level())
		return;

	for (i=0;i<N_robot_types;i++)
		if (!stricmp(Monster_info[i].name,"dragon"))
			type = i;
	if (type < 0)
		return;
	i = Robot_info[type].model_num;
	if (i >= 0 && i < N_polygon_models)
		po = &Polygon_models[i];
	if (po == NULL || po->dtu_block == NULL || !(po->box_flags & DTU_BOX_BODY) || po->body_radius == NULL || po->body_center == NULL ||
		 po->body_half_height == NULL || po->n_frames < Dtu_anim_first[DAS_ATTACK3])
		return;
	model_setup(po,i);
	Dr_type = type;
	Dr_model = i;
	Dr_c0 = po->body_center[1];
	Polyobj_scale_model = i;
	Polyobj_scale = DR_SCALE;
	type_setup(type);

	for (i=1;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & DTU_SCRIPT_MASK) == DR_SCRIPT) {
			// an object of a level state: the values of this file
			obj = &Objects[i];
			if (obj->shields > i2f(DR_HIT_POINTS))
				obj->shields = i2f(DR_HIT_POINTS);
			if (dragon_alive(obj))
				obj->pos.y = DR_FLOOR + po->body_half_height[0];
			return;
		}

	// With a saved state the restore follows and replaces the objects. She
	// is made here also, so the setup by name of her type runs as for a
	// creature of the level file. A state with no object for her (a state of
	// 1.3) gets her at its restore.
	(void) has_state;
	pos.x = DR_CENTER_X;
	pos.y = DR_FLOOR + po->body_half_height[0];
	pos.z = DR_CENTER_Z;
	seg = find_point_seg(&pos,-1);
	if (seg < 0)
		return;
	objnum = obj_create(OBJ_ROBOT,type,seg,&pos,&vmd_identity_matrix,i2f(4),CT_AI,MT_PHYSICS,RT_POLYOBJ);
	if (objnum < 0)
		return;
	obj = &Objects[objnum];
	obj->shields = i2f(DR_HIT_POINTS);
	obj->ctype.ai_info.behavior = DAIB_STATION;
	obj->ctype.ai_info.hide_segment = seg;
	// message 17: "It does not attack, but instead watches you intently."
	obj->dtu.m_1b0 = DR_SCRIPT | DTU_PEACEFUL;
	// No object of 1.3 has the key of the door of her hoard and her script
	// gives none. She carries it.
	obj->contains_type = OBJ_MISC;
	obj->contains_id = DR_KEY_HOARD;
	obj->contains_count = 1;
	pass = Level_setup_pass;
	Level_setup_pass = 1;		// add_npc comes with level_start_npcs
	setup_creature(obj);
	Level_setup_pass = pass;
	dtu_obj_log("dragon: obj %d %s (type %d) script %d in segment %d at %.2f %.2f %.2f, hp %d",objnum,dtu_object_name(obj),type,
		DR_SCRIPT,seg,f2fl(pos.x),f2fl(pos.y),f2fl(pos.z),f2i(obj->shields));

	// message 19 (trigger 25, segment 790, the east room): "Uh oh, baby
	// dragons." The level file has the four in her room.
	for (i=0;i<4;i++) {
		object *o = hatchling(i);

		if (!o)
			continue;
		pos.x = i2f(Dr_hatch_pos[i][0]);
		pos.y = i2f(-100);
		pos.z = i2f(Dr_hatch_pos[i][1]);
		seg = find_point_seg(&pos,-1);
		if (seg < 0)
			continue;
		o->pos = pos;
		obj_relink(Dr_hatch[i],seg);
		o->ctype.ai_info.hide_segment = seg;
		Ail[Dr_hatch[i]].fall_start_y = pos.y;
		dtu_obj_log("dragon: obj %d %s to segment %d (the hatchery)",Dr_hatch[i],dtu_object_name(o),seg);
	}
}

static void lock_door(const short d[2][2], int lock)
{
	int k;

	for (k=0;k<2;k++) {
		wall *w = side_wall(d[k][0],d[k][1]);

		if (!w || w->type != WALL_DOOR)
			continue;
		if (lock) {
			// KEY_RED: no thief skill and no spell opens the lock
			w->flags |= WALL_DOOR_LOCKED;
			w->keys = KEY_RED;
		}
		else {
			w->flags &= ~WALL_DOOR_LOCKED;
			w->keys = KEY_NONE;
		}
	}
}

static int door_locked(const short d[2][2])
{
	wall *w = side_wall(d[0][0],d[0][1]);

	return w && (w->flags & WALL_DOOR_LOCKED);
}

// The door of the hatchery opens and stays open
static void open_hatchery(void)
{
	int k;

	for (k=0;k<2;k++) {
		wall *w = side_wall(Dr_hatchery[k][0],Dr_hatchery[k][1]);

		if (!w || w->type != WALL_DOOR)
			continue;
		w->flags &= ~(WALL_DOOR_LOCKED|WALL_DOOR_AUTO);
		w->keys = KEY_NONE;
	}
	{
		wall *w = side_wall(Dr_hatchery[0][0],Dr_hatchery[0][1]);

		if (w && w->type == WALL_DOOR && w->state == WALL_DOOR_CLOSED && !(w->flags & WALL_DOOR_OPENED))
			wall_open_door(&Segments[Dr_hatchery[0][0]],Dr_hatchery[0][1]);
	}
}

static int place_item(int type, int id, int count, fix x, fix z)
{
	vms_vector pos;
	int seg, n;

	pos.x = x;
	pos.y = DR_FLOOR + i2f(5);
	pos.z = z;
	seg = find_point_seg(&pos,-1);
	if (seg < 0)
		return -1;
	if (type == OBJ_MISC && id == DR_GOLD_ID) {
		n = dtu_create_item(type,id,1,seg,&pos);
		if (n >= 0)
			Objects[n].shields = i2f(count);
	}
	else
		n = dtu_create_item(type,id,count,seg,&pos);
	dtu_obj_log("dragon: item type %d id %d count %d \"%s\": obj %d in segment %d, picture %d, size %.1f",type,id,count,
		n >= 0 ? dtu_object_name(&Objects[n]) : "none",n,seg,n >= 0 && Objects[n].render_type == RT_ITEM ? Objects[n].dtu.item_bitmap : -1,
		n >= 0 ? f2fl(Objects[n].size) : 0.0);
	return n;
}

void dtu_dragon_level_rules(int restore)
{
	int on = dtu_dragon_on(), i;

	if (Dtu_num_triggers > DR_LEVER_TRIGGER) {
		dtu_trigger *t = &Dtu_triggers[DR_LEVER_TRIGGER];

		// 1.3: the lever opens the hatch of the ceiling (trap 12) and runs
		// script 213 of object 42 (the rule 0x5A855), which sends the
		// hatchlings away and sets global flag 107 (the market doubles its
		// prices, 0x86F5F). With the encounter the lever opens the hatchery
		// (dtu_dragon_frame); the hatch stays closed, and script 213 does not
		// run: object_num has the value of the level file.
		t->object_num = on ? 1 : DR_LEVER_OBJECT;
		t->first_trap = on ? -1 : DR_LEVER_TRAP;
	}

	if (!on) {
		// the locks of the encounter of a level state go away
		for (i=0;i<2;i++) {
			const short (*d)[2] = i ? Dr_hatchery : Dr_alcove;
			wall *w = side_wall(d[0][0],d[0][1]);

			if (w && w->type == WALL_DOOR && (w->flags & WALL_DOOR_LOCKED) && w->keys == KEY_RED) {
				lock_door(d,0);
				dtu_obj_log("dragon: the setting is off, the door of segment %d side %d is not locked",d[0][0],d[0][1]);
			}
		}
		return;
	}
	if (Dr_type < 0)
		return;
	// A load with a level state: the restore follows and replaces the
	// objects and the walls, and this runs again after it.
	if (!restore && Dtu_level_has_state[DR_LEVEL])
		return;

	if (dtu_script_gflag(DR_GF_LEVER))
		open_hatchery();

	if (!restore)
		dtu_script_set_gflag(DR_GF_PLACED,0);		// a new level: its objects are those of the level file
	if (dtu_script_gflag(DR_GF_PLACED))
		return;
	dtu_script_set_gflag(DR_GF_PLACED,1);

	// 1.3 after its lever (global flag 107): the hatchlings are gone and
	// the level is as that game left it
	if (dtu_script_gflag(107)) {
		dtu_obj_log("dragon: global flag 107 is set (the lever of 1.3), no locks");
	}
	else {
		lock_door(Dr_alcove,1);
		if (!dtu_script_gflag(DR_GF_LEVER))
			lock_door(Dr_hatchery,1);
		dtu_obj_log("dragon: the doors of the alcove of the lever (seg %d side %d) and of the hatchery (seg %d side %d) are locked",
			Dr_alcove[0][0],Dr_alcove[0][1],Dr_hatchery[0][0],Dr_hatchery[0][1]);
		place_item(OBJ_MISC,DR_KEY_ALCOVE,1,DR_KEY_X,DR_KEY_Z);
	}
	for (i=0;i<DR_N_TREASURE;i++)
		if (Dr_treasure[i].type >= 0)
			place_item(Dr_treasure[i].type,Dr_treasure[i].id,Dr_treasure[i].count,i2f(Dr_treasure[i].x),i2f(Dr_treasure[i].z));
	dtu_obj_log("dragon: the key and the treasure are placed (%s)",restore ? "a level state without them" : "a new level");
}

//------------------------------------------------------------------------
// The magic resistance

int dtu_dragon_spell_resisted(object *target, object *caster, int spell)
{
	int roll;

	if (!dtu_dragon_is(target) || caster == target || caster == NULL)
		return 0;
	roll = rand() % 100 + 1;
	dtu_obj_log("dragon: magic resistance, spell %d '%s' of obj %d: d100 %d against %d: %s",spell,dtu_spell_name(spell),
		(int)(caster-Objects),roll,DR_MAGIC_RESIST,roll <= DR_MAGIC_RESIST ? "resisted" : "not resisted");
	if (roll > DR_MAGIC_RESIST)
		return 0;
	if (caster->type == OBJ_PLAYER)
		HUD_init_message(LT("%s resists %s."),dtu_object_display_name(target),dtu_spell_name(spell));
	return 1;
}

//------------------------------------------------------------------------
// The text of her dialogue. 1.3: the lever is "in the corridor from whence
// you came", and she promises the key of her hoard. With the encounter the
// door of the alcove of the lever is locked and its key lies in the cells
// behind her. The offsets are those of the strings in record 210.

const char *dtu_dragon_script_text(int script, int offset)
{
	if (script != DR_SCRIPT || !in_level() || !dtu_dragon_on())
		return NULL;
	switch (offset) {
		case 3390:
			return LN("In the corridor from whence you came, there is a lever that will release my children. The drow keep it behind a locked door, and its key lies in the cells behind me, where they held the wretches they fed to me. Take the key, open that door and throw the lever, and I shall be grateful. I shall give you the key to my hoard, and let you have all my treasures. At my age, the wealth of life, and the immortality of one's offspring, is more important than gold or jewels and magic. My children must be free! Help me, please!");
		case 627:
			return LN("You have not yet thrown the lever. Its door is locked, and the key lies in the cells behind me. Oh, that the fate of my loved ones should depend on the twist of a single piece of metal! Please help me, please! The drow will regret it, but you shall not, I promise you!");
		case 945:
			return LN("Welcome again, wanderer, Champion of Quests, slayer of drow. You have done so many great deeds, could you not do a small favor for me? Take the key from the cells behind me, find the lever to the cage that imprisons my offspring, and free them from the tyranny of the spider-worshippers!");
	}
	return NULL;
}

//------------------------------------------------------------------------
// Her attacks

static object *player_target(void)
{
	object *p = ConsoleObject;

	if (!p || p->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead || dtu_player_hit_points() < F1_0)
		return NULL;
	return p;
}

static fix dist_xz(vms_vector *a, vms_vector *b)
{
	vms_vector v;

	vm_vec_sub(&v,a,b);
	v.y = 0;
	return vm_vec_mag(&v);
}

// The player stands on the floor of her room or near it in height
static int height_ok(object *p)
{
	fix dy = p->pos.y - (DR_FLOOR + DTU_BODY_HALF_HEIGHT);

	return dy > -i2f(8) && dy < i2f(16);
}

// The claw: the weapon box of the hit frame of ATTACK (the right front
// claw). Returns the distance from the box to the axis of the player.
static fix claw_dist(object *obj, object *p, fix *reach)
{
	polymodel *po = &Polygon_models[Dr_model];
	vms_vector c;

	if (po->weapon_center == NULL || po->weapon_radius == NULL) {
		model_point(obj,DR_CLAW_FRAME,-3.5,-8.6,33.5,&c);
		*reach = fixmul(i2f(15),DR_SCALE) + DTU_BODY_RAD + DR_CLAW_MARGIN;
	}
	else {
		body_point(obj,DR_CLAW_FRAME,&po->weapon_center[DR_CLAW_FRAME],&c);
		*reach = po->weapon_radius[DR_CLAW_FRAME] + DTU_BODY_RAD + DR_CLAW_MARGIN;
	}
	return dist_xz(&c,&p->pos);
}

// The bite: the tip of her snout in the hit frame of ATTACK2 (the gun
// point of the jaw, 0.4 5.6 60.1, and 11 along the jaw)
static fix bite_dist(object *obj, object *p, fix *reach)
{
	vms_vector c;

	model_point(obj,DR_BITE_FRAME,0.4,-1.8,68.2,&c);
	*reach = DR_BITE_RADIUS + DTU_BODY_RAD + DR_BITE_MARGIN;
	return dist_xz(&c,&p->pos);
}

static void melee_sound(object *obj, object *p, fix dealt)
{
	if (dealt > 0) {
		blood(p);
		sound_at(player_type_sound("hit_sound"),p->segnum,&p->pos,F1_0);
	}
	else if (Robot_info[obj->id].claw_sound > 0)
		sound_at(Robot_info[obj->id].claw_sound,obj->segnum,&obj->pos,F1_0/2);
}

// One melee action: two claws of 1d10 or one bite of 3d10. Each attack
// has its own attack roll (damage_player_w: her THAC0 against the armor
// class of the player).
static void melee_hit_now(object *obj, int bite)
{
	object *p = player_target();
	monster_ext *me = &Monster_ext[obj->id];
	fix d, reach, dealt;
	int k, n = bite ? 1 : 2;

	if (!p)
		return;
	d = bite ? bite_dist(obj,p,&reach) : claw_dist(obj,p,&reach);
	if (d > reach || !height_ok(p)) {
		dtu_obj_log("dragon: %s: the player is out of reach (%.2f, reach %.2f)",bite ? "bite" : "claws",f2fl(d),f2fl(reach));
		return;
	}
	me->n_damage_dice = bite ? DR_BITE_DICE : 1;
	me->damage_dice = bite ? DR_BITE_SIDES : DR_CLAW_SIDES;
	me->damage_bonus = 0;
	for (k=0;k<n;k++) {
		if (!player_target())
			break;
		dealt = damage_player_w(p,obj,0,-1);
		dtu_obj_log("dragon: %s %d of %d (%dd%d) at %.2f, reach %.2f: %s %d",bite ? "bite" : "claw",k+1,n,me->n_damage_dice,me->damage_dice,
			f2fl(d),f2fl(reach),dealt > 0 ? "damage" : "no damage",f2i(dealt));
		melee_sound(obj,p,dealt);
	}
}

// The cone of the breath from her jaws along D.dir: 1 if the player is in
// it and no wall is between
static int in_breath(object *obj, object *p)
{
	vms_vector v, w;
	fix t, perp;

	vm_vec_sub(&v,&p->pos,&D.mouth);
	t = vm_vec_dot(&v,&D.dir);
	if (t < 0 || t > DR_BREATH_RANGE)
		return 0;
	vm_vec_scale_add(&w,&v,&D.dir,-t);
	perp = vm_vec_mag(&w);
	if (perp > fixmul(t,DR_BREATH_TAN) + DTU_BODY_RAD + i2f(2))
		return 0;
	return ray_is_free(&D.mouth,D.mouth_seg,&p->pos,obj-Objects);
}

// 12d10+6 of fire. A save against breath (category 3) halves it; then the
// fire protections of the player (Resist Fire: half; Protection From Fire:
// its pool), as for each fire damage (0x2AF5E).
static void breath_hit(object *obj, object *p)
{
	int i, roll = DR_BREATH_BONUS, dmg, saved, after;

	for (i=0;i<DR_BREATH_DICE;i++)
		roll += rand() % DR_BREATH_SIDES + 1;
	dtu_char_save_cause(-1,"Dragon breath",dtu_object_name(obj));
	saved = dtu_spell_save_from(p,DTU_SAVE_BREATH,obj);
	dmg = saved ? roll / 2 : roll;
	after = dtu_fire_part(p,dmg);
	dtu_obj_log("dragon: breath on the player: %dd%d+%d = %d, save against breath %s: %d, after the fire protections: %d",
		DR_BREATH_DICE,DR_BREATH_SIDES,DR_BREATH_BONUS,roll,saved ? "made" : "failed",dmg,after);
	HUD_init_message(saved ? LT("You throw yourself aside from the worst of the dragon's fire.") : LT("The dragon's fire engulfs you."));
	if (after > 0) {
		dtu_spell_interrupt(p,1);
		dtu_damage_player_direct(p,obj,i2f(after));
	}
	else
		HUD_init_message(LT("Your protection from fire takes the flames."));
}

// The flames of the stream: explosions of the vclip of Burning Hands
// (spell 4, weapon_vclip) between her jaws and the end of the cone
static int fire_vclip(void)
{
	item_info *it = item_table_entry(OBJ_SPELL,4);
	int v = it ? it->vclip_num : -1;

	if (v < 0 || v >= VCLIP_MAXNUM || Vclip[v].num_frames <= 0 || Vclip[v].play_time <= 0) {
		it = item_table_entry(OBJ_SPELL,16);		// Fireball
		v = it ? it->vclip_num : -1;
	}
	if (v < 0 || v >= VCLIP_MAXNUM || Vclip[v].num_frames <= 0 || Vclip[v].play_time <= 0)
		return -1;
	return v;
}

static void flame(object *obj, vms_vector *from, int seg, vms_vector *dir, fix r, fix size)
{
	vms_vector p, j;
	fvi_query fq;
	fvi_info hit;
	int v = fire_vclip();

	if (v < 0 || seg < 0)
		return;
	make_random_vector(&j);
	vm_vec_scale_add(&p,from,dir,r);
	vm_vec_scale_add2(&p,&j,fixmul(r,DR_BREATH_TAN) * 2 / 3);
	fq.p0 = from;
	fq.p1 = &p;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) != HIT_NONE || hit.hit_seg < 0)
		return;
	object_create_explosion(hit.hit_seg,&p,size,v);
}

static int spell_sound(int spell, char *key)
{
	item_info *it = item_table_entry(OBJ_SPELL,spell);

	return (it && it->record >= 0) ? tbl_int(it->record,key,-1) : -1;
}

//------------------------------------------------------------------------
// Her AI

static void set_mode(object *obj, int mode)
{
	if (D.mode != mode)
		dtu_obj_log("dragon: obj %d: %s -> %s, hp %d",(int)(obj-Objects),Dr_mode_names[D.mode],Dr_mode_names[mode],f2i(obj->shields));
	D.mode = mode;
	D.t0 = GameTime;
	D.hit_done = 0;
}

static void set_frame(object *obj, int state, int frame)
{
	DTU_ANIM_STATE(obj) = state;
	DTU_ANIM_GOAL(obj) = state;
	DTU_ANIM_FRAME(obj) = frame;
}

// The frame of a state at the time t of the length len, one pass
static int frame_at(int state, fix t, fix len)
{
	int first = Dtu_anim_first[state], n = Dtu_anim_first[state+1] - first, k;

	if (t < 0) t = 0;
	if (t > len) t = len;
	k = (int)(((int64_t)t * n) / len);
	if (k >= n) k = n - 1;
	return first + k;
}

static void loop_anim(object *obj, int state, fix len)
{
	fix t = (GameTime - D.t0) % len;

	if (t < 0) t += len;
	set_frame(obj,state,frame_at(state,t,len));
}

static void set_orient(object *obj)
{
	vms_angvec a;

	a.p = a.b = 0;
	a.h = D.heading;
	vm_angles_2_matrix(&obj->orient,&a);
}

// Turns her toward a point at rate (fixang in a second). Returns the
// angle that is left, as a positive number.
static int turn_to(object *obj, vms_vector *to, int rate)
{
	vms_vector v;
	vms_angvec a;
	int diff, step;

	vm_vec_sub(&v,to,&obj->pos);
	v.y = 0;
	if (vm_vec_mag(&v) < F1_0)
		return 0;
	vm_extract_angles_vector(&a,&v);
	diff = (short)(a.h - D.heading);
	step = (int)(((int64_t)rate * FrameTime) >> 16);
	if (step < 1) step = 1;
	if (diff > step) diff = step;
	if (diff < -step) diff = -step;
	D.heading = (fixang)(D.heading + diff);
	set_orient(obj);
	diff = (short)(a.h - D.heading);
	return diff < 0 ? -diff : diff;
}

// One step toward the player, in the area around the middle of the room
// and not into the player. Returns 1 if she moved.
static int walk(object *obj, object *p)
{
	vms_vector np = obj->pos, axis;
	fix step = fixmul(DR_WALK_SPEED,FrameTime);
	int seg;

	np.x += fixmul(obj->orient.fvec.x,step);
	np.z += fixmul(obj->orient.fvec.z,step);
	if (np.x < DR_CENTER_X - DR_LEASH_X) np.x = DR_CENTER_X - DR_LEASH_X;
	if (np.x > DR_CENTER_X + DR_LEASH_X) np.x = DR_CENTER_X + DR_LEASH_X;
	if (np.z < DR_CENTER_Z - DR_LEASH_Z) np.z = DR_CENTER_Z - DR_LEASH_Z;
	if (np.z > DR_CENTER_Z + DR_LEASH_Z) np.z = DR_CENTER_Z + DR_LEASH_Z;
	if (dist_xz(&np,&obj->pos) < step / 4)
		return 0;
	// the axis of her cylinder at the new place
	axis = np;
	if (dist_xz(&axis,&p->pos) < DR_CYL_RADIUS + DTU_BODY_RAD + i2f(3))
		return 0;
	seg = find_point_seg(&np,obj->segnum);
	if (seg < 0)
		return 0;
	obj->pos = np;
	if (seg != obj->segnum)
		obj_relink(obj-Objects,seg);
	return 1;
}

static void become_hostile(object *obj, char *why)
{
	if (!dragon_alive(obj))
		return;
	if (!(obj->dtu.m_1b0 & DTU_HOSTILE))
		dtu_obj_log("dragon: obj %d is hostile: %s",(int)(obj-Objects),why);
	obj->dtu.m_1b0 |= DTU_HOSTILE;
	obj->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
}

// The choice of an action. Returns the new mode, or DM_IDLE.
static int choose(object *obj, object *p, int facing)
{
	fix cd, cr, bd, br;
	int claw, bite, breath, los, ms;
	vms_vector m;

	cd = claw_dist(obj,p,&cr);
	bd = bite_dist(obj,p,&br);
	claw = height_ok(p) && cd <= cr;
	bite = height_ok(p) && bd <= br;
	if (D.force) {
		int f = D.force;

		D.force = 0;
		return f == 1 ? DM_CLAW : f == 2 ? DM_BITE : DM_WIND;
	}
	if (!Dtu_creatures_attack)
		return DM_IDLE;
	breath = 0;
	if (GameTime >= D.next_breath && facing < 0x2000) {
		mouth_point(obj,DR_REAR_TOP,&m);
		ms = find_point_seg(&m,obj->segnum);
		los = ms >= 0 && ray_is_free(&m,ms,&p->pos,obj-Objects);
		if (los && vm_vec_dist(&m,&p->pos) < DR_BREATH_RANGE)
			breath = 1;
	}
	if (breath && (!(claw || bite) || rand() % 100 < 50))
		return DM_WIND;
	if (facing > 0x1800)
		return DM_IDLE;
	if (bite && (!claw || rand() % 100 < DR_BITE_CHANCE))
		return DM_BITE;
	if (claw)
		return DM_CLAW;
	return DM_IDLE;
}

static void dragon_ai(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	polymodel *po = &Polygon_models[Dr_model];
	object *p = player_target();
	int hostile = is_hostile(obj), sees = 0, facing = 0, seg;
	fix t;

	if (D.objnum != objnum || D.sig != obj->signature) {
		vms_angvec a;

		memset(&D,0,sizeof(D));
		D.objnum = objnum;
		D.sig = obj->signature;
		vm_extract_angles_vector(&a,&obj->orient.fvec);
		D.heading = a.h;
		D.mode = hostile ? DM_IDLE : DM_REST;
		D.t0 = GameTime;
		D.next_breath = GameTime + DR_BREATH_FIRST;
	}

	// the values that the rest of the game reads
	ail->activity = 2;
	ail->mode = DAIM_STILL;
	ail->target = hostile ? 0 : -1;
	ail->awareness = hostile ? 4 : 0;
	ail->path_len = 0;
	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
	vm_vec_zero(&obj->mtype.phys_info.velocity);
	vm_vec_zero(&obj->mtype.phys_info.rotvel);
	obj->pos.y = DR_FLOOR + po->body_half_height[0];
	set_orient(obj);
	if ((seg = find_point_seg(&obj->pos,obj->segnum)) >= 0 && seg != obj->segnum)
		obj_relink(objnum,seg);

	// an effect that holds her (Hold Monster, Sleep, Web)
	if (obj->dtu.m_88 & DTU_OF_HELD)
		return;

	if (p) {
		// from her position: the segment of the object is known
		sees = vm_vec_dist(&obj->pos,&p->pos) < i2f(260) && ray_is_free(&obj->pos,obj->segnum,&p->pos,objnum);
	}

	if (!hostile) {
		// She rests (the rest pose of the model, dtu_dragon_draw_frame) and
		// follows the player with a slow turn.
		if (D.mode != DM_REST)
			set_mode(obj,DM_REST);
		set_frame(obj,DAS_STAND,Dtu_anim_first[DAS_STAND]);
		if (sees)
			turn_to(obj,&p->pos,DR_TURN_PEACE);
		return;
	}
	if (D.mode == DM_REST) {
		// the start of the fight: she rises and roars
		set_mode(obj,DM_RECOVER);
		D.next_breath = GameTime + DR_BREATH_FIRST;
		sound_at(Robot_info[obj->id].see_sound,obj->segnum,&obj->pos,F1_0);
		creature_combat_mark(obj);
	}
	t = GameTime - D.t0;
	if (t < 0) t = 0;

	switch (D.mode) {
		case DM_IDLE:
			if (!p || !sees) {
				loop_anim(obj,DAS_STAND,2*F1_0);
				break;
			}
			facing = turn_to(obj,&p->pos,DR_TURN_FIGHT);
			{
				int m = choose(obj,p,facing);

				if (m != DM_IDLE) {
					set_mode(obj,m);
					if (m == DM_WIND) {
						D.aim = p->pos;
						D.aim_fixed = 0;
						D.puff_acc = 0;
						sound_at(Robot_info[obj->id].see_sound,obj->segnum,&obj->pos,F1_0);
						HUD_init_message(LT("%s rears and draws a deep breath."),dtu_object_display_name(obj));
					}
					else if (Robot_info[obj->id].attack_sound > 0)
						sound_at(Robot_info[obj->id].attack_sound,obj->segnum,&obj->pos,F1_0*3/4);
					break;
				}
			}
			{
				fix cd, cr;

				cd = claw_dist(obj,p,&cr);
				if (facing < 0x2000 && cd > cr && walk(obj,p))
					loop_anim(obj,DAS_WALK,2*F1_0);
				else
					loop_anim(obj,DAS_STAND,2*F1_0);
			}
			break;

		case DM_CLAW:
		case DM_BITE: {
			int state = D.mode == DM_CLAW ? DAS_ATTACK : DAS_ATTACK2;
			int hit = D.mode == DM_CLAW ? DR_CLAW_FRAME : DR_BITE_FRAME;

			set_frame(obj,state,frame_at(state,t,DR_MELEE_TIME));
			if (!D.hit_done && DTU_ANIM_FRAME(obj) >= hit) {
				D.hit_done = 1;
				melee_hit_now(obj,D.mode == DM_BITE);
			}
			if (t >= DR_MELEE_TIME)
				set_mode(obj,DM_RECOVER);
			break;
		}

		case DM_WIND: {
			// She rears (the frames 60 to 65 of ATTACK2) and flames come
			// from her jaws. She turns with the player until DR_AIM_TIME;
			// then the aim is fixed, and the player has the rest of the
			// wind-up to leave the cone or the line of sight.
			int frame = DR_REAR_FIRST + (int)(((int64_t)t * (DR_REAR_TOP - DR_REAR_FIRST)) / DR_WIND_TIME);

			if (frame > DR_REAR_TOP) frame = DR_REAR_TOP;
			set_frame(obj,DAS_ATTACK2,frame);
			if (!D.aim_fixed) {
				if (p && sees) {
					turn_to(obj,&p->pos,DR_TURN_AIM);
					D.aim = p->pos;
				}
				if (t >= DR_AIM_TIME) {
					D.aim_fixed = 1;
					dtu_obj_log("dragon: the aim of the breath is fixed at %.2f %.2f %.2f",f2fl(D.aim.x),f2fl(D.aim.y),f2fl(D.aim.z));
				}
			}
			D.puff_acc += FrameTime;
			if (D.puff_acc >= F1_0/6) {
				vms_vector m, up = { 0, F1_0, 0 };

				D.puff_acc = 0;
				mouth_point(obj,frame,&m);
				flame(obj,&m,find_point_seg(&m,obj->segnum),&up,F1_0,i2f(1) + t);
			}
			if (t >= DR_WIND_TIME) {
				mouth_point(obj,DR_REAR_TOP,&D.mouth);
				D.mouth_seg = find_point_seg(&D.mouth,obj->segnum);
				vm_vec_sub(&D.dir,&D.aim,&D.mouth);
				if (vm_vec_normalize(&D.dir) < F1_0)
					D.dir = obj->orient.fvec;
				set_mode(obj,DM_FIRE);
				D.puff_acc = 0;
				sound_at(spell_sound(16,"cast_sound"),obj->segnum,&D.mouth,F1_0);
				dtu_obj_log("dragon: breath from %.2f %.2f %.2f (seg %d) along %.2f %.2f %.2f",f2fl(D.mouth.x),f2fl(D.mouth.y),f2fl(D.mouth.z),
					D.mouth_seg,f2fl(D.dir.x),f2fl(D.dir.y),f2fl(D.dir.z));
			}
			break;
		}

		case DM_FIRE: {
			fix front = DR_BREATH_MIN + fixmul(DR_BREATH_RANGE,fixdiv(t > F1_0/3 ? F1_0/3 : t,F1_0/3));

			set_frame(obj,DAS_ATTACK2,DR_REAR_TOP);
			D.puff_acc += FrameTime;
			while (D.puff_acc >= F1_0/40) {
				fix r = fixmul(front,(rand() & 0x7fff) * 2);

				D.puff_acc -= F1_0/40;
				if (r < DR_BREATH_MIN/2) r = DR_BREATH_MIN/2;
				flame(obj,&D.mouth,D.mouth_seg,&D.dir,r,i2f(2) + r / 16);
			}
			// the fire reaches the player one time for a breath
			if (!D.hit_done && p && D.mouth_seg >= 0 && in_breath(obj,p)) {
				vms_vector v;

				vm_vec_sub(&v,&p->pos,&D.mouth);
				if (vm_vec_dot(&v,&D.dir) <= front) {
					D.hit_done = 1;
					breath_hit(obj,p);
				}
			}
			if (t >= DR_FIRE_TIME) {
				if (!D.hit_done)
					dtu_obj_log("dragon: the breath misses the player");
				set_mode(obj,DM_FIRE_END);
				D.next_breath = GameTime + DR_FIRE_END + DR_BREATH_PAUSE;
			}
			break;
		}

		case DM_FIRE_END: {
			// the frames 66 to 75: she comes down
			int frame = DR_REAR_TOP + 1 + (int)(((int64_t)t * (Dtu_anim_first[DAS_ATTACK2+1] - DR_REAR_TOP - 1)) / DR_FIRE_END);

			if (frame >= Dtu_anim_first[DAS_ATTACK2+1]) frame = Dtu_anim_first[DAS_ATTACK2+1] - 1;
			set_frame(obj,DAS_ATTACK2,frame);
			if (t >= DR_FIRE_END)
				set_mode(obj,DM_RECOVER);
			break;
		}

		case DM_RECOVER:
		default:
			loop_anim(obj,DAS_STAND,2*F1_0);
			if (p && sees)
				turn_to(obj,&p->pos,DR_TURN_FIGHT);
			if (t >= DR_RECOVER_TIME)
				set_mode(obj,DM_IDLE);
			break;
	}
}

int dtu_dragon_ai_frame(object *obj)
{
	int k;

	if (!in_level() || !dtu_dragon_on() || Dr_type < 0 || obj->type != OBJ_ROBOT)
		return 0;
	if (dtu_dragon_is(obj)) {
		if (!dragon_alive(obj) || obj->id != Dr_type)
			return 0;
		if (dtu_creature_frozen(obj))
			return 1;
		dragon_ai(obj);
		return 1;
	}
	// A hatchling behind the locked door of the hatchery does nothing:
	// it does not sense the player and casts no spell through the door.
	// (-dragonhatchai, test: the AI of the game runs for them, to see what
	// its senses do behind the door)
	if (!dtu_script_gflag(DR_GF_LEVER) && door_locked(Dr_hatchery) && !FindArg("-dragonhatchai"))
		for (k=0;k<4;k++)
			if (obj-Objects == Dr_hatch[k] && hatchling(k)) {
				Ail[obj-Objects].activity = 0;
				return 1;
			}
	return 0;
}

int dtu_dragon_no_physics(object *obj)
{
	return dtu_dragon_is(obj) && Dr_type >= 0 && obj->id == Dr_type && dragon_alive(obj);
}

int dtu_dragon_draw_frame(object *obj, int frame)
{
	if (frame >= 0 && dtu_dragon_is(obj) && Dr_type >= 0 && dragon_alive(obj) && !is_hostile(obj))
		return -1;
	return frame;
}

int dtu_dragon_drop_place(object *obj, vms_vector *pos, int *seg)
{
	vms_vector p;
	int s;

	if (!dtu_dragon_is(obj))
		return 0;
	p = obj->pos;
	p.x += fixmul(obj->orient.fvec.x,i2f(30));
	p.z += fixmul(obj->orient.fvec.z,i2f(30));
	p.y = DR_FLOOR + i2f(5);
	if ((s = find_point_seg(&p,obj->segnum)) < 0) {
		p = obj->pos;
		p.y = DR_FLOOR + i2f(5);
		if ((s = find_point_seg(&p,obj->segnum)) < 0)
			return 0;
	}
	*pos = p;
	*seg = s;
	return 1;
}

//------------------------------------------------------------------------
// The player cannot stand in her. The move of the player tests only the
// objects of the segments that it goes through (fvi_sub), and she is in
// the list of one segment while her body reaches into its neighbors, so
// the player walked into her from most sides. Each frame a player whose
// axis is in her cylinder is moved out of it, away from its axis.
static void keep_player_out(object *dr, object *p)
{
	vms_vector center, v, np;
	fix radius, half, d, want;
	int seg;

	dtu_get_body_cylinder(dr,&center,&radius,&half);
	if (p->pos.y - DTU_BODY_HALF_HEIGHT > center.y + half || p->pos.y + DTU_BODY_HALF_HEIGHT < center.y - half)
		return;
	vm_vec_sub(&v,&p->pos,&center);
	v.y = 0;
	d = vm_vec_mag(&v);
	want = radius + DTU_BODY_RAD;
	if (d >= want)
		return;
	if (d < F1_0/16) {
		v = dr->orient.fvec;
		v.y = 0;
		d = vm_vec_mag(&v);
	}
	np = p->pos;
	np.x = center.x + fixmuldiv(v.x,want,d);
	np.z = center.z + fixmuldiv(v.z,want,d);
	seg = find_point_seg(&np,p->segnum);
	if (seg < 0)
		return;
	p->pos = np;
	if (seg != p->segnum)
		obj_relink(p-Objects,seg);
}

//------------------------------------------------------------------------
// The events of the level

// The lever (trigger 20). 1.3: the hatch of the ceiling opens and the
// hatchlings leave the level. Here: the door of the hatchery opens and
// stays open, the hatchlings come for the player, and she laughs at the
// player and attacks. The key of her hoard stays with her.
static void lever_thrown(void)
{
	object *dr = dragon_object();
	int k, n = 0;

	dtu_script_set_gflag(DR_GF_LEVER,1);
	open_hatchery();
	for (k=0;k<4;k++) {
		object *o = hatchling(k);

		if (!o)
			continue;
		n++;
		dtu_creature_wake(o,1);
		// into her room, by the path of a script walk (statement E5): the
		// AI of the game takes over at its end, or when they sense the player
		dtu_script_walk_segment(o,Dr_hatch_goal[k]);
		dtu_script_walk_start(o,1);
	}
	dtu_obj_log("dragon: the lever (trigger %d, fired %d): the hatchery (seg %d side %d) is open, %d hatchlings, the dragon %s",
		DR_LEVER_TRIGGER,Dtu_triggers[DR_LEVER_TRIGGER].fire_count,Dr_hatchery[0][0],Dr_hatchery[0][1],n,
		dragon_alive(dr) ? "attacks" : "is dead");
	if (dragon_alive(dr)) {
		say(LT("A heavy door grinds open far down the hall, and a deep laugh rolls through the corridor: \"Little fool! That was no cage, it was my hatchery, and you have opened it. The hoard stays mine. Come, my children, our dinner has let itself in!\""));
		become_hostile(dr,"the lever");
	}
	else if (n)
		say(LT("A heavy door grinds open far down the hall. You hear the beat of small wings."));
	else
		say(LT("A heavy door grinds open far down the hall."));
}

void dtu_dragon_frame(void)
{
	object *p = ConsoleObject, *dr;
	wall *w;

	if (!in_level() || !dtu_dragon_on() || Dr_type < 0 || !p || p->type != OBJ_PLAYER)
		return;

	dr = dragon_object();
	if (dragon_alive(dr) && dr->id == Dr_type)
		keep_player_out(dr,p);

	// the lever
	if (!dtu_script_gflag(DR_GF_LEVER) && !dtu_script_gflag(107) && Dtu_num_triggers > DR_LEVER_TRIGGER &&
		 Dtu_triggers[DR_LEVER_TRIGGER].fire_count > 0)
		lever_thrown();

	// the door of the hatchery stays open
	if (dtu_script_gflag(DR_GF_LEVER) && (GameTime - D.door_time > F1_0 || GameTime < D.door_time)) {
		D.door_time = GameTime;
		open_hatchery();
	}

	// the key of the alcove: the player with the key at the end of the
	// corridor, in front of the door
	if (p->segnum == DR_ALCOVE_SEG && door_locked(Dr_alcove) && (w = side_wall(Dr_alcove[0][0],Dr_alcove[0][1])) != NULL &&
		 w->keys == KEY_RED && dtu_char_has(OBJ_MISC,DR_KEY_ALCOVE)) {
		vms_vector c;

		lock_door(Dr_alcove,0);
		compute_center_point_on_side(&c,&Segments[Dr_alcove[0][0]],Dr_alcove[0][1]);
		digi_link_sound_to_pos(2500,Dr_alcove[0][0],Dr_alcove[0][1],&c,0,F1_0);
		wall_open_door(&Segments[Dr_alcove[0][0]],Dr_alcove[0][1]);
		dtu_obj_log("dragon: the key of the alcove (MISC %d) opens the door of seg %d side %d",DR_KEY_ALCOVE,Dr_alcove[0][0],Dr_alcove[0][1]);
		say(LT("You use the jailer's key to unlock the door."));
	}

	// the door of her hoard: she sees the player open it (a thief picks
	// the lock, or a key) while she lives and is at peace
	if (!D.hoard_seen && dragon_alive(dr) && !is_hostile(dr) && (w = side_wall(Dr_hoard[0][0],Dr_hoard[0][1])) != NULL && w->type == WALL_DOOR &&
		 (!(w->flags & WALL_DOOR_LOCKED) || w->state == WALL_DOOR_OPENING || (w->flags & WALL_DOOR_OPENED))) {
		if (ray_is_free(&dr->pos,dr->segnum,&p->pos,dr-Objects)) {
			D.hoard_seen = 1;
			dtu_obj_log("dragon: she sees the player open the door of her hoard (wall flags 0x%x state %d)",w->flags,w->state);
			say(LT("Tamaramagil's roar shakes the hall: \"THIEF! You dare open my hoard before my very eyes? My riches are MINE! You shall burn for this!\""));
			become_hostile(dr,"the door of the hoard");
		}
	}
}

//------------------------------------------------------------------------
// Test

void dtu_dragon_test(int what)
{
	object *dr = dragon_object();

	if (!dr) {
		dtu_obj_log("dragon: test %d: no dragon",what);
		return;
	}
	if (what >= 1 && what <= 3) {
		become_hostile(dr,"test");
		D.force = what;
	}
	else if (what == 5)
		become_hostile(dr,"test");
	dtu_obj_log("dragon: test %d: obj %d %s hp %d ac %d thac0 %d xp %d seg %d pos %.2f %.2f %.2f heading %d mode %s hostile %d peaceful %d frame %d",
		what,(int)(dr-Objects),dtu_object_name(dr),f2i(dr->shields),Monster_info[dr->id].armor_class,Monster_info[dr->id].thaco,
		Robot_info[dr->id].score_value,dr->segnum,f2fl(dr->pos.x),f2fl(dr->pos.y),f2fl(dr->pos.z),(int)(unsigned short)D.heading,
		Dr_mode_names[D.mode],is_hostile(dr),(dr->dtu.m_1b0 & DTU_PEACEFUL) != 0,DTU_ANIM_FRAME(dr));
}
