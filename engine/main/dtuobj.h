/*
 * ASCENT: new file. The objects of Descent to Undermountain other than the
 * player: the setup of creatures, scenery and items after a level is
 * loaded, the animation of the polygon models, the behavior of the
 * creatures (ai.c, pmon.c of DTUGAME.EXE), the melee fight, the pickup of
 * items and the inventory.
 *
 * The facts are in docs/re/models.md, docs/re/objects.md and docs/re/ai.md.
 * The address of the original code is at each function of dtuobj.c.
 */
#ifndef _DTUOBJ_H
#define _DTUOBJ_H

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"

//------------------------------------------------------------------------
// Animation states (the names of the table at 0x13A764 of DTUGAME.EXE).
// The states 11 to 18 have no name. 11 and 12 are attacks of the player.

#define DAS_REST			0
#define DAS_STAND			1
#define DAS_WALK			2
#define DAS_SIDLE_R		3
#define DAS_SIDLE_L		4
#define DAS_RUN			5
#define DAS_ATTACK		6
#define DAS_ATTACK2		7
#define DAS_FLINCH		8
#define DAS_DIE			9
#define DAS_FIDGET		10
#define DAS_ATTACK3		11
#define DAS_ATTACK4		12
#define DTU_N_ANIM_STATES	19

// The first frame of each state. Entry s+1 is the end of state s (the
// table at 0x13D9E0).
extern short Dtu_anim_first[DTU_N_ANIM_STATES+1];

// The animation values of an object (0x170, 0x172, 0x176 of the object of
// the executable).
#define DTU_ANIM_STATE(obj)	((obj)->dtu.m_170)
#define DTU_ANIM_FRAME(obj)	((obj)->dtu.m_172)
#define DTU_ANIM_GOAL(obj)		((obj)->dtu.m_176)

// AI states (0x13A73C), AI modes (0x13A6D0), behaviors (0x13A70F)
#define DAIS_NONE		0
#define DAIS_REST		1
#define DAIS_SRCH		2
#define DAIS_LOCK		3
#define DAIS_FLIN		4
#define DAIS_ATAK		5
#define DAIS_RECO		6
#define DAIS_DEAD		7

#define DAIM_STILL		0
#define DAIM_WANDER		1
#define DAIM_FOL_PATH	2
#define DAIM_CHASE_OB	3
#define DAIM_RUN_FROM	4
#define DAIM_HOME			5
#define DAIM_OPENDOOR	6

#define DAIB_GUARD		0x80
#define DAIB_WANDER		0x81
#define DAIB_HOME			0x82
#define DAIB_PATROL		0x83
#define DAIB_STATION		0x84

// Bits of the int at 0x1B0 of the object (dtu.m_1b0)
#define DTU_SCRIPT_MASK		0x000001ff
#define DTU_PEACEFUL			0x00000200
#define DTU_HOSTILE			0x00400000
#define DTU_FLEEING			0x00800000

//------------------------------------------------------------------------
// Settings. 1 (default) is the corrected rule, 0 is the rule of the
// executable, which depends on the frame rate. The corrected rules give the
// behavior of the executable at a frame time of DTU_REF_FRAMETIME (3/64
// second, physics.h). The option -origobj clears them all.

// The animation of a creature that is not active (0x14013). Original: one
// step for each game frame (STAND: for each tenth game frame). Corrected:
// one step for each reference frame time.
extern int Dtu_fix_idle_anim;

// The tests "(clock + objnum) % N == 0" of the AI, which are true in one
// game frame of N. Corrected: true one time in N reference frame times.
extern int Dtu_fix_ai_ticks;

// The movement of a creature (pmon.c). Original: the velocity becomes one
// half on each frame, two steps of thrust, drag and gravity at most on each
// frame, the lift out of the floor is 2.0 on each frame, the blocked move
// counter counts frames. Corrected: each of these per reference frame time.
extern int Dtu_fix_creature_move;

// The sight of a creature. Original: the bit of the BRN file of the level
// for the two segments (0x9810D). The BRN files of the disc are copies of
// one matrix that fits no level. Corrected: a ray from the creature to the
// player.
extern int Dtu_fix_brain;

// 0 (default) is the executable: do_ai_frame puts a creature that is not
// active back to the first frame of STAND each time it is processed
// (0x14A54), so the idle animation of a creature that is nearer than 100.0
// does not run. 1: the idle animation runs at all distances. This is not a
// rule that depends on the frame rate. Option -idleanim.
extern int Dtu_idle_anim_near;

// 1 after the player has less than one hit point. The player object stays
// in the level and does not move. The death of the executable is not ported.
extern int Dtu_player_dead;

// 0 stops all attacks of creatures (the switch at 0x13A6B4 of the
// executable). Option -noattack.
extern int Dtu_creatures_attack;

//------------------------------------------------------------------------
// The inventory of the player. The executable keeps it in the player
// structure (insert function 0xB65BB, not ported). This is a plain list
// for the play screen: one entry for each stack of items.

#define DTU_MAX_INVENTORY		64

typedef struct dtu_inv_item {
	ubyte		type;				// OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC or OBJ_SPELL. 0: the entry is free
	ubyte		id;				// index of the table of the type (Weapon_item_info, Armor_info, Misc_info, Spell_info)
	short		count;			// number of items of the stack, or the charges of one item
	short		icon;				// icon= of the table line, or -1
	short		inv_icon;		// inv_icon= of the table line, or -1
	short		obj_bitmap;		// picture= of the table line: slot of ObjBitmapPtrs, or -1
	short		level;			// number of the level where the item was found
	char		name[40];		// name= of the table line
} dtu_inv_item;

extern dtu_inv_item Dtu_inventory[DTU_MAX_INVENTORY];
extern int Dtu_n_inventory;		// entries in use, they are at the start of the list
extern int Dtu_gold;					// the gold of the player (misc id 1)

// Adds "count" items. Returns the index of the entry, or -1 if the list is
// full. A stackable type goes to the entry that has it already.
int dtu_inventory_add(int type, int id, int count);
// Takes "count" items from entry "index". The entry goes away at count 0.
void dtu_inventory_remove(int index, int count);
void dtu_inventory_clear(void);

//------------------------------------------------------------------------
// Calls from the game.

// One time, after init_game(): the collision table for the new types and
// the options -objlog FILE, -origobj, -noattack, -noobjects.
void dtu_objects_init(void);

// After a level is loaded and its models have their numbers: sets the
// model, the bitmap, the size and the animation of each object from the
// tables (verify_object 0x477D9, setup_object 0xAE3F2, place_creatures
// 0x6A6FB, fix_object_segs 0x69FFB).
void dtu_objects_level_setup(void);

// The control of a creature for one frame (do_ai_frame 0x13BF4).
void dtu_do_ai_frame(object *obj);

// The movement of a creature for one frame (pmon.c 0x9F153).
void dtu_creature_physics_sim(object *obj);

// Called one time per frame before the objects move (the animation and the
// melee attack of the player).
void dtu_objects_frame(void);

// Rendering. dtu_draw_polygon_object() is the normal case of
// draw_polygon_object (0x676ED). dtu_draw_item() is render type 8
// (0x67E2D). dtu_draw_dressing_vclip() is render type 7 of scenery.
void dtu_draw_polygon_object(object *obj, fix light, fix *glow, bitmap_index *alt_textures);
void dtu_draw_item(object *obj);
void dtu_draw_dressing_vclip(object *obj);

// Collision handlers (collide.c of the executable). Each returns 1 if it
// handled the pair.
int dtu_collide_two_objects(object *a, object *b, vms_vector *collision_point);

// Damage (0x29A9C). "attacker" can be NULL.
void dtu_damage_player(object *player, object *attacker, fix damage);
void dtu_damage_creature(object *creature, object *attacker, fix damage);

// The player starts a melee attack (pweapon.c). Returns 1 if it started.
int dtu_player_attack(void);

// The body cylinder of an object for collisions (0x748D9) and the size for
// physics (0x746B3).
void dtu_get_body_cylinder(object *obj, vms_vector *center, fix *radius, fix *half_height);
void dtu_get_body_size(object *obj, fix *radius, fix *half_height);

// The name of an object for messages ("a chair", "durnan").
char *dtu_object_name(object *obj);

// The log of the option -objlog (state changes of creatures, damage,
// pickup). The text starts with the game time and the frame number.
void dtu_obj_log(char *format, ...);

#endif
