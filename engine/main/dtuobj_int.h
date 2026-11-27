/*
 * ASCENT: new file. The internal interface of the object code: the
 * declarations that dtuobj.c and the files split from it share (see
 * docs/architecture.md). The public interface is dtuobj.h.
 */
#ifndef _DTUOBJ_INT_H
#define _DTUOBJ_INT_H

#include <stdio.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "dtuobj.h"

#define DTU_WEAK __attribute__((weak))

int check_and_fix_matrix(vms_matrix *m);		// gamesave.c
void make_random_vector(vms_vector *vec);		// ai.c

// The values of a $MONSTER line that monster_info does not have.
typedef struct monster_ext {
	int	damage_dice, n_damage_dice, to_hit_bonus, damage_bonus;
	int	regeneration;
	int	weapon_type;			// -1 if the line has none (a natural weapon). Below 0: the spell with the number -weapon_type
	int	spell_level;
	int	door_opening;			// +0x63 bit 0, set for each HUMANOID too (0x1061B)
	int	person;					// ASCENT: a person by its model (dtu_model_is_person()), opens doors with Dtu_improved_ai
	int	family;					// ASCENT: the kind of the creature by its model (dtu_model_family()), for creature_join_fight()
	int	blood_vclip;			// monster_hit_vclip, +0xDA
	int	touch_spell;			// +0x7C: the spell of a hit on the player, -1 for none. Set by the creature setup 0xAC7B2 (lich), cast by the damage function (0x2A637)
} monster_ext;

// The data of the AI for each object (Ai_local_info at 0x153EB0, and
// fields of the object of the executable that the port has no place for).
#define AILF_BACKWARD	0x02		// play the animation backward (revive)
#define AILF_AROUND		0x04
#define AILF_TOWARD		0x08
#define AILF_AWAY			0x10
#define AILF_HIT			0x20		// the hit landed, end the animation state early

#define DTU_PATH_MAX		32

typedef struct dtu_ai_local {
	ubyte		awareness;				// +0x00
	ubyte		retry_count;			// +0x01
	ubyte		mode;						// +0x03
	ubyte		previous_visibility;	// +0x04
	ubyte		flags;					// +0x05, AILF_
	ubyte		activity;				// object +0x1CC: 0 inactive, 2 active
	ubyte		swing_done;				// object +0x1AF: the swing has hit
	ubyte		commit;					// ASCENT: Dtu_fix_attack_commit: 0 not tested, 1 no, 2 the attack is committed
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
	fix		regen_hp;				// Dtu_fix_revive: the hit points that a corpse has regenerated
	ubyte		gold_done;				// Dtu_fix_revive: the corpse has left its gold
	fix		fall_start_y;			// the table 0x30E2D4: the height of the start of a fall (0x6C8C8)
	fix		lava_time;				// Dtu_fix_lava: the time of the last burn (0x13B1B4 in 1.3 is one for all objects)
	// values of the corrected rules
	fix		idle_acc;
	int		idle_ticks;
	fix		push_acc;
	fix		step_acc;
	// the velocity that the AI adds in this frame (0x11EC9)
	vms_vector move_add;
	int		has_move;
	int		last_logged_state;
	// The path (the executable keeps the points in Point_segs, 0x16FD9).
	// path[] has the segments, the first is the segment of the creature.
	short		path[DTU_PATH_MAX];
	short		path_len, path_idx;
	signed char	path_dir;			// object +0x144 PATH_DIR
	signed char	path_flag;			// object +0x145: 1 after create_path, 0 path to the player, -1 path of n segments
	short		goal_segment;			// +0x06
	fix		path_fail_time;		// corrected rule: the time of the last search with no result
	fix		time_attack_sound;	// +0x18
	fix		next_misc_sound;		// +0x1C
	fix		spell_time;				// object +0x1C0
	ubyte		fired;					// corrected rule: the shot of this attack animation is done
	ubyte		gated;					// made by a trap (matcen_creator -1 of the executable)
	ubyte		script_walk;			// ASCENT: statement E5 of a script: 1 on the path, 2 at its end
	ubyte		prev_fov;				// corrected rule: the player was in the field of view (visibility 2 of Descent 1)
	// 0x123EF: the executable has one counter, position and direction for
	// all creatures (0x13A79C, 0x13A790, 0x167184)
	int		flee_counter;
	vms_vector flee_pos, flee_dir;
	// Dtu_improved_ai: the point of the path that the creature
	// walks to (see path_target()), the last progress to it, and the number
	// of new plans since the last point that it reached.
	short		path_tgt;
	short		prog_tgt;
	fix		prog_time, prog_dist;
	ubyte		replans;
	short		stall_tgt;				// the point where the creature was blocked
	ubyte		body_line;				// the line to the player is free for the body
	ubyte		keep_off;				// the player is above and out of reach (keep_off_player())
	// Dtu_fix_creature_crowd (crowd_block()): the creature that blocks the
	// move and the time that it did, and the creature that this one walks
	// through to end a jam. Object number + 1, 0: none. Not in a saved game.
	short		crowd_obj, crowd_pass;
	fix		crowd_time;
	// test log of -objlogpos: the hits of the move since the last line
	short		log_hits[4];			// wall, top, floor, object
	short		log_hit_seg, log_hit_side;
	fix		log_wall_time;			// the game time of the last wall hit (-testkill N,1)
	short		log_hit_obj;			// the object of the last object hit
	fix		chase_path_time;		// ASCENT: Dtu_fix_chase_run, the last time on a path to the player
	// ASCENT: Dtu_improved_ai (dtuobj_aware.c). The last time the creature
	// noticed the player (sight, hearing, a hit, an alarm), 0: never. Not in
	// a saved game.
	fix		combat_time;
	fix		join_time;				// the next test of creature_join_fight()
} dtu_ai_local;

// A missile (object type OBJ_WEAPON that flies). The entry belongs to the
// object with the same number while the signature is the same.
typedef struct dtu_missile {
	int		signature;				// 0: no missile
	int		parent, parent_sig;	// object +0x142, +0x144. -1: no parent
	short		spell;					// the spell that the missile carries (enemy_spell), or -1
	ubyte		bounce;					// weapon +0x0A: 1 bounce, 2 stick, 3 thrown
	ubyte		harmless;				// object flag 0x20
	fix		tumble_acc;
	fix		trail_acc;				// ASCENT: the frame time of the trail (missile_trail)
	int		pass_obj, pass_sig;	// ASCENT: Dtu_fix_hit_keep, the creature that a kept missile passes. -1: none
	fix		light;					// ASCENT: lightcast of the missile (player_light of the spell, 0x56FF6), or -1: that of the weapon
	ubyte		unlinked;				// ASCENT: +0x144 cleared (Lightning Bolt after a wall): the parent can be hit
} dtu_missile;

//------------------------------------------------------------------------
// dtuobj.c

int improved_ai(void);
extern fix Model_reach[MAX_POLYGON_MODELS];
extern monster_ext Monster_ext[MAX_ROBOT_TYPES];
extern dtu_ai_local Ail[MAX_OBJECTS];
extern dtu_missile Missile[MAX_OBJECTS];
extern fix Dtu_clock;
extern int Ref_tick_count, Ref_ticks_now;
extern int Player_armor_class;
extern fix Player_max_hp;
extern int Player_death_stage;
extern fix Player_death_time;
extern int N_aware_events;
extern int Player_swing_done;
extern FILE *Obj_log;
extern int Obj_log_frames;
extern int Setup_log;
extern int Test_pos_pending;
void log_creature_state(object *obj,char *why);
polymodel *obj_model(object *obj);
int is_hostile(object *obj);
int is_flying(object *obj);
int tbl_int(int record,char *key,int def);
fix state_time(polymodel *po,int state);
fix anim_speed(object *obj,int state);
void set_anim(object *obj,int state);
int get_weapon_sphere(object *obj,int frame,vms_vector *center,fix *radius);
int ai_tick(int objnum,int n);
fix creature_floor_dist(object *obj,fix radius,fix half_height);
int ray_is_free(vms_vector *p0,int seg0,vms_vector *p1,int objnum);
int brain_bit(int a,int b);
int creature_ray_to_player(object *obj);
// dtuobj_aware.c, Dtu_improved_ai
int creature_senses_player(object *obj);
void creature_combat_mark(object *obj);
int creature_in_combat(object *obj);
void creature_join_fight(object *obj);
int attack_wall_between(vms_vector *p,int seg,object *target,int thisobjnum);
int player_is_visible(object *obj);
void sound_at(int sound,int segnum,vms_vector *pos,fix volume);
void sound_on_object(int sound,object *obj,fix volume);
void vclip_at(int segnum,vms_vector *pos,fix size,int vclip);
int weapon_int(int w,char *key,int def);
int weapon_is_melee(int w);
void blood(object *obj);
int player_type_sound(char *key);
void log_corpse_height(object *obj,const char *what);
fix corpse_depth(object *obj,int frame,vms_vector *mn,vms_vector *mx);
int model_points_box(object *obj,int frame,vms_vector *mn,vms_vector *mx);
int corpse_on_floor(object *obj);

//------------------------------------------------------------------------
// dtuobj_setup.c

extern int ail_keep_bits;
extern int Dtu_script_level;
extern int Level_setup_pass;
void init_creature_ai(object *obj);
void setup_creature(object *obj);
extern int Level_setup_restore;
void level_fixes_tables(int has_state);
void lava_textures_lookup(void);
void level_fixes_objects(int has_state);
void level_fixes_scenery_shields(void);
void level_start_npcs(int setup);

//------------------------------------------------------------------------
// dtuobj_item.c

item_info *item_table_entry(int type,int id);
void pick_up_item(object *player,object *item);

//------------------------------------------------------------------------
// dtuobj_combat.c

extern int Hit_by_missile;
int damage_dice(object *attacker,object *target,int weapon);
void creature_is_corpse(object *obj);
fix spell_hit(int spell,object *target,object *attacker);
fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
fix damage_player_w(object *player, object *attacker, fix damage, int weapon);
void melee_hit(object *target,object *attacker);
void weapon_test(object *obj);

//------------------------------------------------------------------------
// dtuobj_missile.c

void missiles_frame(void);

//------------------------------------------------------------------------
// dtuobj_ai.c

void cloak_reset(void);
int opens_doors(object *obj);
int dtu_model_is_person(const char *pof);
void morale_check(object *obj);

//------------------------------------------------------------------------
// dtuobj_move.c

fix model_weapon_reach(object *obj);
fix creature_stop_dist(object *obj);

//------------------------------------------------------------------------
// dtuobj_scenery.c

void end_sequence_reset(void);
void end_sequence_frame(void);
int dressing_pushable(object *d);
void push_dressing(object *mover,object *d,int strength);
void dressing_move_frame(void);
void dressing_floor_log(object *d,const char *when);
void dressing_floor_fix(void);

//------------------------------------------------------------------------
// dtuobj_aware.c

void awareness_events_frame(void);

//------------------------------------------------------------------------
// dtuobj_test.c

void test_place_player(void);

#endif
