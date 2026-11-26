/*
 * ASCENT: new file, split from dtuobj.c. The AI of a creature: the
 * attacks, the turn and the moves, the paths, the blocked moves, the
 * morale, the animation, the AI frame (dtu_do_ai_frame) and the calls of
 * the scripts. The shared declarations are in dtuobj_int.h.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include "dtuitemfix.h"	// ASCENT: the corrected item names
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

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
#include "dtutrig.h"
#include "dtuspell.h"	// ASCENT: the spells of the creatures, the area of a missile
#include "dtuthief.h"	// ASCENT: Hide In Shadows and Move Silently in the AI
#include "dtuscript.h"	// ASCENT: quest flag 67 in the creature setup
#include "textures.h"	// ASCENT: the lava textures of BANE
#include "tmapfx.h"	// ASCENT: the tint of Detect Magic
#include "dtuchar.h"
#include "digi.h"
#include "sounds.h"
#include "dtufoot.h"		// ASCENT: the footsteps of the creatures
#include "fireball.h"
#include "palette.h"
#include "dtumusic.h"
#include "key.h"		// ASCENT: the key that ends the wait of the death
#include "dtuchar.h"
#include "dtuopts.h"	// ASCENT: Dtu_improved_pathfinding
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()
#include "dtulog.h"		// ASCENT: the session log
#include "dtuxp.h"		// ASCENT: the kill line, the death lines of the session log
#include "dtumiss.h"		// ASCENT: the miss sound of a creature
#include "dtuobj_int.h"

// 0x10040: the special attack of a creature with a weapon_type below 0. It
// casts the spell with the number -weapon_type as a missile with the
// weapon entry 5 (0x56FF6). The next cast: 180.0 seconds later for the
// spell 16 (Fireball), else 0 to 10 seconds plus 4 seconds for each level
// of the spell.
static void creature_cast(object *obj,int spell,vms_vector *dir,vms_vector *fire_point)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int seg;

	if (!is_hostile(obj) || ail->spell_time > GameTime || ail->activity != 2)
		return;
	if (spell < 0 || spell >= N_spell_types)
		return;
	seg = find_point_seg(fire_point,obj->segnum);
	if (seg < 0)
		return;
	dtu_obj_log("attack: obj %d %s casts spell %d '%s'",(int)(obj-Objects),dtu_object_name(obj),spell,Spell_info[spell].name);
	dtu_create_missile(dir,fire_point,seg,obj-Objects,5,spell,1);
	dtu_spell_creature_weapon_cast(obj,1);		// 0x10040: ailp+0x2C = -2 (the creature is active)
	if (spell == 16)
		ail->spell_time = GameTime + i2f(180);
	else
		ail->spell_time = GameTime + (rand() % 11) * F1_0 + tbl_int(Spell_info[spell].record,"level",1) * 4 * F1_0;
}

// ASCENT: the believed position of a hidden player (audit C20, Ai_cloak_info
// 0x153A30: 8 entries of time and position, the entry objnum % 8 for a
// creature). compute_vis_and_vec (0x12BE8): when the player is hidden from
// the creature (0x165C4) or the creature has the status bit 4, the entry
// moves each 2.0 seconds by a random vector (flat, 0x127F3; in three
// directions, 0x127BF, when the player flies: effects 0x200800 or +0x1AE
// bit 0x80) times 8 times the time since its last move (16.0 for 2.0 s), and
// the creature uses it in place of the player (0x146CE): the distance, the
// turn, the walk and the attack. 0x15FBC (the awareness events of the types 2
// to 4, 0x16055) puts all entries at the player. The creature never sees the
// player: its sight is the ray to the believed position, and a path never
// goes to the segment of the player (the port's rule that keeps the
// invisibility of the player, audit C1).
static struct { fix last_time; vms_vector pos; } Ai_cloak[8];
static vms_vector *Ai_believed = NULL;		// the aim of ai_fire for a hidden player

void cloak_reset(void)
{
	int i;

	if (ConsoleObject == NULL)
		return;
	for (i=0;i<8;i++) {
		Ai_cloak[i].pos = ConsoleObject->pos;		// 0x15FBC: Believed_player_pos
		Ai_cloak[i].last_time = GameTime;
	}
	dtu_obj_log("believed position: at the player %.1f %.1f %.1f (0x15FBC)",f2fl(ConsoleObject->pos.x),f2fl(ConsoleObject->pos.y),
		f2fl(ConsoleObject->pos.z));
}

// 1 if the creature uses the believed position (0x12C11 to 0x12C25)
static int cloak_applies(object *obj)
{
	return dtu_thief_hidden_from(obj) || (obj->dtu.m_88 & 4);
}

// 0x12C2B to 0x12CB9: the move of the entry of the creature
static vms_vector *cloak_position(object *obj)
{
	int k = (obj-Objects) % 8;
	fix elapsed = GameTime - Ai_cloak[k].last_time;

	if (elapsed < 0) {
		Ai_cloak[k].last_time = GameTime;
		elapsed = 0;
	}
	if (elapsed > 0x20000) {
		vms_vector r;

		Ai_cloak[k].last_time = GameTime;
		// (the bit 0x80 of the player +0x1AE has no place in the port)
		if (ConsoleObject->dtu.m_88 & 0x200800)
			make_random_vector(&r);
		else {
			r.x = (rand() - 0x4000) | 1;
			r.y = 0;
			r.z = rand() - 0x4000;
			vm_vec_normalize_quick(&r);
		}
		vm_vec_scale_add2(&Ai_cloak[k].pos,&r,elapsed << 3);
		dtu_obj_log("obj %d %s: believes the hidden player at %.1f %.1f %.1f",(int)(obj-Objects),dtu_object_name(obj),
			f2fl(Ai_cloak[k].pos.x),f2fl(Ai_cloak[k].pos.y),f2fl(Ai_cloak[k].pos.z));
	}
	return &Ai_cloak[k].pos;
}

void melee_hit(object *target,object *attacker);

// 0x11B06 ai_fire. "AI: critter fires."
static void ai_fire_body(object *obj,fix dist);

static void ai_fire(object *obj,fix dist)
{
	if (!Dtu_creatures_attack || !is_hostile(obj) || Robot_info[obj->id].attack_type == 1)
		return;
	ai_fire_body(obj,dist);
	// ASCENT: 0x11EB7, the end of ai_fire calls the alert function
	// 0x160AB(creature, 1)
	dtu_creature_alert(obj,1);
}

static void ai_fire_body(object *obj,fix dist)
{
	int objnum = obj-Objects;
	robot_info *ri = &Robot_info[obj->id];
	int w = Monster_ext[obj->id].weapon_type;
	object *target = ConsoleObject;
	vms_vector fire_point, aim, dir;
	fix reach, r1, h1, r2, h2;
	fvi_query fq;
	fvi_info hit;
	int fate;

	if (!Dtu_creatures_attack || !is_hostile(obj) || ri->attack_type == 1)
		return;

	dtu_get_body_size(obj,&r1,&h1);
	dtu_get_body_size(target,&r2,&h2);
	reach = r1 + r2 + i2f(2);

	// The fire point: 1.0 in front of the creature (0x11018, step 9)
	vm_vec_scale_add(&fire_point,&obj->pos,&obj->orient.fvec,F1_0);

	// The aim. A target that is slower than 1.0 in x and z: its position.
	// Else the position plus the velocity (x and z) times one half of the
	// time of the flight. The y part of the lead is not set in the
	// executable (0x11CC4). It is 0 here.
	aim = target->pos;
	// ASCENT: a hidden player: the believed position, with no lead (C20).
	// The touch below needs the real distance.
	if (Ai_believed) {
		aim = *Ai_believed;
		dist = vm_vec_dist(&obj->pos,&target->pos);
	}
	else if (w >= 0 && !(target->mtype.phys_info.velocity.x < F1_0 && target->mtype.phys_info.velocity.z < F1_0)) {
		fix speed = Weapon_info[w < N_weapon_types ? w : 0].speed[Difficulty_level];

		if (speed > 0) {
			fix t = fixdiv(dist,speed) / 2;

			aim.x += fixmul(target->mtype.phys_info.velocity.x,t);
			aim.z += fixmul(target->mtype.phys_info.velocity.z,t);
		}
	}
	vm_vec_sub(&dir,&aim,&fire_point);
	if (vm_vec_normalize(&dir) < 0x100)
		dir = obj->orient.fvec;

	if (w < 0) {
		if (dist <= reach) {
			dtu_obj_log("attack: obj %d %s touches the player",objnum,dtu_object_name(obj));
			melee_hit(target,obj);
		}
		else
			creature_cast(obj,-w,&dir,&fire_point);
		return;
	}

	if (obj->control_type == CT_MORPH || Dtu_player_dead || Player_is_dead)
		return;

	// A monster with a mage or cleric level does not shoot (0x11DE9). It
	// touches a target in reach. Its spells come from 0x92D99 and 0x91FDC
	// (dtuspell.c). The levels are Robot_info+0xE2 and +0xE3: the NPC table,
	// else spell_level= of the line (Monster_ext.spell_level is only the
	// line, which prstsprt and hatchlng do not have).
	if (dtu_spell_creature_caster(obj)) {
		if (dist <= reach) {
			dtu_obj_log("attack: obj %d %s (a caster) touches the player",objnum,dtu_object_name(obj));
			melee_hit(target,obj);
		}
		return;
	}

	// Laser_create_new_easy (0x55B23): the way from the creature to the
	// fire point must be free.
	fq.p0 = &obj->pos;
	fq.p1 = &fire_point;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = objnum;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_NONE || hit.hit_seg < 0) {
		dtu_obj_log("attack: obj %d %s cannot fire, the fire point is in a wall",objnum,dtu_object_name(obj));
		return;
	}

	dtu_obj_log("attack: obj %d %s fires weapon %d, frame %d",objnum,dtu_object_name(obj),w,DTU_ANIM_FRAME(obj));
	dtu_create_missile(&dir,&fire_point,hit.hit_seg,objnum,w,-1,1);
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

// ASCENT: Dtu_fix_chase_run (dtuobj.h): 1 if the creature walked on a path
// to the player (chase_by_path()) in the last 0.5 second
static int chase_runs(dtu_ai_local *ail)
{
	return Dtu_fix_chase_run && ail->chase_path_time && ail->chase_path_time + F1_0/2 >= GameTime;
}

// 0x10148. The animation state that belongs to an AI state.
static int ai_to_anim_state(object *obj,int ai_state,fix dist)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int station = (obj->ctype.ai_info.behavior == DAIB_STATION);

	switch (ai_state) {
		case DAIS_SRCH:	return station ? DAS_STAND : DAS_WALK;
		case DAIS_LOCK:
			// ASCENT: keep_off_player(), a creature that waits for the player
			if (station || ail->keep_off == 2)
				return DAS_STAND;
			if (dist > i2f(30) && (ail->time_player_seen + F1_0/2 >= GameTime || chase_runs(ail)))
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
		// 0x12000 move_around_player with fast_flag -1: the creature moves
		// across the line to the player and looks at the player, or walks
		// that way. The executable takes one of four directions from the
		// frame count. The port takes it from the game time: a new one each
		// 1.5 seconds. (the timing is a rule of the port)
		if (obj->ctype.ai_info.CURRENT_STATE != DAIS_ATAK) {
			int d = ((GameTime / (F1_0*3/2)) + (int)(obj-Objects)*13) & 3;
			vms_vector e, b;

			e.y = 0;
			switch (d) {
				case 0:	e.x = vec_to_player->z;		e.z = -vec_to_player->x;	break;
				case 1:	e.x = -vec_to_player->z;	e.z = vec_to_player->x;		break;
				case 2:	e.x = -vec_to_player->x;	e.z = vec_to_player->z;		break;
				default:	e.x = vec_to_player->x;		e.z = -vec_to_player->z;	break;
			}
			b.x = vec_to_player->x; b.y = 0; b.z = vec_to_player->z;
			if (vm_vec_normalize(&e) > 0x100 && vm_vec_normalize(&b) > 0x100) {
				fix dot = vm_vec_dot(&e,&b);
				int limit = (Robot_info[obj->id].attack_type == 1) ? -F1_0/2 : F1_0/2;		// 120 or 60 degrees

				if (dot > limit) {
					// in front: step to the side with the face to the player
					int s = (vm_vec_dot(&e,&obj->orient.rvec) < 0) ? DAS_SIDLE_L : DAS_SIDLE_R;

					if (DTU_ANIM_STATE(obj) != s)
						set_anim(obj,s);
					b.y = obj->orient.fvec.y;
					vm_vector_2_matrix(&obj->orient,&b,NULL,NULL);
				}
				else {
					if (DTU_ANIM_STATE(obj) != DAS_WALK)
						set_anim(obj,DAS_WALK);
					e.y = obj->orient.fvec.y;
					vm_vector_2_matrix(&obj->orient,&e,NULL,NULL);
				}
				check_and_fix_matrix(&obj->orient);
				e.y = 0;
				add_move(obj,&e);
			}
		}
		ail->flags |= AILF_AROUND;
	}
	else {
		move_towards(obj,vec_to_player);
		ail->flags = (ail->flags & ~(AILF_BACKWARD|AILF_TOWARD)) | AILF_TOWARD;
	}
}

//------------------------------------------------------------------------
// Paths (aipath.c of the executable, 0x1686D to 0x17FFF)

// 0x1320F ai_door_is_openable: a creature with the door bit can go through
// a door with no key and no lock. The state of the door is not tested.
// ASCENT: 1 while dtu_script_walk_start() makes the path of a creature
// with no door bit that walks for its script (Dtu_fix_script_walk)
static int Script_walk_doors;

// ASCENT: Dtu_improved_pathfinding (a decision of the project). In the
// executable only a type with the door bit opens doors: door_opening=1 or
// the word "humanoid" of the monster line (0x1061B), which the tables give
// to orcs, goblins, kobolds, ogres and liches only. The humans, drow,
// dwarves, halflings, elves, Banegards and ghouls have no door bit. With
// the setting a hostile creature whose model is a person (by the model
// name of the monster line) opens a door as a creature with the door bit
// does: with no key and no lock (door_is_openable(), 0x25CAA). Animals,
// skeletons, zombies, spirits and monsters (beholders, dragons, mephits,
// beetles, spiders) do not. Gargoyles do (they have hands and are not
// mindless, a decision of the project). The door opener of the brain code
// (0x14AF2) stays for the door bit only.
int dtu_model_is_person(const char *pof)
{
	static const char *const prefix[] = {
		"hum_",		// humans: the thieves of THIEF*, hmfg1, the people of the inn
		"haf",		// halflings
		"dwf_",		// dwarves
		"drw",		// drow men (drw_mfg5, drwmmk7b)
		"dlf_",		// drow women
		"elf_",		// elves
		"orc_", "gob_", "kob",		// door bit in the tables already
		"ogr", "ognew", "armogr",	// ogres
		"troll", "armtroll",			// trolls
		"lic_",		// the lich (door bit already)
		"min_",		// the mind flayer
		"ban_",		// Banegards
		"ghu_", "gha_",				// ghouls and ghasts (intelligent in AD&D 2e)
		"mum_",		// mummies (the intelligence of ghouls in AD&D 2e)
		"gar_",		// gargoyles (MYRKUL, IBBALAR; they fly)
		"durnan", "daland", "thashasa", "hal.", "ibba",		// named people
	};
	int k;

	for (k=0;k<(int)(sizeof(prefix)/sizeof(*prefix));k++)
		if (!strnicmp(pof,prefix[k],strlen(prefix[k])))
			return 1;
	return 0;
}

static int person_opens_doors(object *obj)
{
	return improved_paths() && obj->type == OBJ_ROBOT && obj->id < N_robot_types && Monster_ext[obj->id].person && is_hostile(obj);
}

// 1 if the creature can open a door: the door bit, or a walk of its script
int opens_doors(object *obj)
{
	return Monster_ext[obj->id].door_opening || Script_walk_doors || Ail[obj-Objects].script_walk == 1 || person_opens_doors(obj);
}

// ASCENT: Dtu_fix_door_pair (dtuobj.h). The executable tests the lock and
// the key of the wall on the side of the creature only (0x1320F, 0x132AA,
// 0x25CAA), and wall_open_door (0x7D77E) opens both walls of the door. In
// 133 of the 153 locked doors of the 39 level files only one wall has the
// lock: the cells of PRISON have it on the wall of the corridor (wall 50,
// seg 347 side 4, opened by trigger 45 with the key, item 16.59), not on
// the wall of the cell (wall 51, seg 589 side 5). So the goblin of the cell
// (object 34) opened its locked door. Corrected: a creature does not open
// a door with a key or a lock on either wall. A creature on the path of
// its script keeps the rule of 1.3: the script lets it out (PRISON script
// 103, object 21). -origobj and -origdoorpair clear it.
int Dtu_fix_door_pair = 1;

// 1 if the wall WN has no key and no lock and, with Dtu_fix_door_pair, the
// wall on the other side of the door has none either
int dtu_creature_door_unlocked(object *obj,int segnum,int side)
{
	segment *segp = &Segments[segnum];
	int wn = segp->sides[side].wall_num, child, cside, cwn;

	if (wn < 0 || Walls[wn].keys != KEY_NONE || (Walls[wn].flags & WALL_DOOR_LOCKED))
		return 0;
	if (!Dtu_fix_door_pair || Script_walk_doors || (obj->type == OBJ_ROBOT && Ail[obj-Objects].script_walk == 1))
		return 1;
	child = segp->children[side];
	if (child < 0)
		return 1;
	cside = find_connect_side(segp,&Segments[child]);
	if (cside < 0 || (cwn = Segments[child].sides[cside].wall_num) < 0)
		return 1;
	return Walls[cwn].keys == KEY_NONE && !(Walls[cwn].flags & WALL_DOOR_LOCKED);
}

static int door_is_openable(object *obj,segment *segp,int side)
{
	int wn = segp->sides[side].wall_num;

	if (wn < 0 || obj->type != OBJ_ROBOT || !opens_doors(obj))
		return 0;
	return Walls[wn].type == WALL_DOOR && dtu_creature_door_unlocked(obj,segp-Segments,side);
}

// 0x132AA openable_doors_in_segment: the first side of the segment of the
// object with a closed door that has no key and no lock, or -1.
static int openable_door_in_segment(object *obj)
{
	int side;

	for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
		int wn = Segments[obj->segnum].sides[side].wall_num;

		if (wn >= 0 && Walls[wn].type == WALL_DOOR && Walls[wn].state == WALL_DOOR_CLOSED &&
			 dtu_creature_door_unlocked(obj,obj->segnum,side))
			return side;
	}
	return -1;
}

// 0x1686D create_path_points. A breadth first search from "start" to
// "end". end -2: the search ends at the depth max_depth. As in Descent 1
// the last segment of the queue is the end if the depth is reached or the
// queue is empty, so the caller must test the last segment of the path.
// New in the executable: a segment with special 1 or 2 is never on a
// path, and a creature with no door bit gets no path through a segment
// that has a wall on any side (but for the first and the last segment).
// Dtu_improved_pathfinding changes the last rule for a hostile creature
// (side_filter below).
// Returns the number of segments in out[], 0 if there is no path.
static int find_path(object *obj,int start,int end,short *out,int max_out,int max_depth,int random_flag,int avoid_seg)
{
	static ubyte visited[MAX_SEGMENTS];
	static short parent[MAX_SEGMENTS], depth[MAX_SEGMENTS], queue[MAX_SEGMENTS];
	int head = 0, tail = 0, found = -1, i, n, cur;
	int has_door_bit = (obj->type != OBJ_ROBOT) || Monster_ext[obj->id].door_opening || Script_walk_doors || person_opens_doors(obj);
	int walker = improved_paths() && obj->type == OBJ_ROBOT && !is_flying(obj);
	// Dtu_improved_pathfinding: a hostile creature with no door bit keeps
	// out of the sides that its body cannot pass (a door that is not open,
	// a closed wall), not out of each segment with a wall. The filter of
	// the executable leaves out a segment with an open door, an illusion
	// wall or a closed wall on any side, so in many rooms such a creature
	// has no path to the player and walks straight into a wall. It still
	// opens no door.
	int side_filter = improved_paths() && obj->type == OBJ_ROBOT && is_hostile(obj);

	if (start < 0 || start > Highest_segment_index)
		return 0;

	// 0x168D9: a creature in the mode RUN_FROM takes a random way that
	// does not go through the segment of the player.
	if (obj->type == OBJ_ROBOT && Ail[obj-Objects].mode == DAIM_RUN_FROM) {
		random_flag = 1;
		avoid_seg = ConsoleObject->segnum;
	}
	if (max_depth < 0)
		max_depth = 30;

	memset(visited,0,Highest_segment_index+1);
	if (avoid_seg >= 0 && avoid_seg != start && avoid_seg != end)
		visited[avoid_seg] = 1;

	// 0x16999 to 0x16AEE. The loop of the executable leaves out the last
	// segment of the level.
	for (i=0;i<Highest_segment_index;i++) {
		if (i == start || i == end)
			continue;
		if (Segments[i].special == 1 || Segments[i].special == 2)
			visited[i] = 1;
		else if (!has_door_bit && !side_filter) {
			int side;

			for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
				if (Segments[i].sides[side].wall_num != -1)
					visited[i] = 1;
		}
	}

	queue[tail++] = start;
	visited[start] = 1;
	depth[start] = 0;
	parent[start] = -1;

	while (head < tail) {
		segment *segp;
		int k, first_side = random_flag ? rand() % MAX_SIDES_PER_SEGMENT : 0;

		cur = queue[head++];
		if (cur == end) {
			found = cur;
			break;
		}
		if (depth[cur] >= max_depth) {
			found = queue[tail-1];
			break;
		}
		segp = &Segments[cur];
		for (k=0;k<MAX_SIDES_PER_SEGMENT;k++) {
			int side = (first_side + k) % MAX_SIDES_PER_SEGMENT;
			int child = segp->children[side];

			if (child < 0 || visited[child])
				continue;
			// Dtu_improved_pathfinding: a creature that walks does not go
			// up through the top side of a segment (an opening in the
			// ceiling, side 1 in the levels of the game). The search of the
			// executable is the one of the ships of Descent 1.
			// A shallow segment (a pit or a step under the floor, BONES
			// segment 53) has its top side less than a knee high above its
			// floor (dtu_knee_high() of physics.c): a body walks out of it.
			if (side == WTOP && walker) {
				vms_vector top, bottom;

				compute_center_point_on_side(&top,segp,WTOP);
				compute_center_point_on_side(&bottom,segp,WBOTTOM);
				if (top.y - bottom.y > DTU_KNEE_HIGH)
					continue;
			}
			if ((WALL_IS_DOORWAY(segp,side) & WID_FLY_FLAG) || door_is_openable(obj,segp,side)) {
				visited[child] = 1;
				parent[child] = cur;
				depth[child] = depth[cur] + 1;
				queue[tail++] = child;
			}
		}
	}
	if (found < 0) {
		if (tail < 2)
			return 0;
		found = queue[tail-1];
	}

	n = depth[found] + 1;
	while (n > max_out) {			// keep the first part
		found = parent[found];
		n--;
	}
	for (i=n-1,cur=found;i>=0;i--,cur=parent[cur])
		out[i] = cur;
	return n;
}

static void path_set(object *obj,int len,int flag)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	ail->path_len = len;
	ail->path_idx = 0;
	ail->path_dir = 1;
	if (flag != 2)
		ail->path_flag = flag;
	ail->path_tgt = 0;
	ail->prog_tgt = -1;
	ail->replans = 0;
	ail->stall_tgt = -1;
}

// 0x172DE create_n_segment_path
static void create_n_segment_path(object *obj,int n,int avoid_seg)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int len = 0;

	while (n > 0 && (len = find_path(obj,obj->segnum,-2,ail->path,DTU_PATH_MAX,n,1,avoid_seg)) == 0) {
		n--;
		avoid_seg = -1;
	}
	path_set(obj,len,-1);
	ail->mode = DAIM_FOL_PATH;
}

// 0x16FD9 create_path: to the goal segment, 30 segments at most
static void create_path(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (ail->goal_segment < 0) {
		create_n_segment_path(obj,3,-1);
		return;
	}
	path_set(obj,find_path(obj,obj->segnum,ail->goal_segment,ail->path,DTU_PATH_MAX,-1,0,-1),1);
}

// ASCENT: test (-chartest aipath=N.SEG): creature N gets a random path in
// mode 2 with the goal segment SEG, as the door opener (0x14AF2) leaves it
void dtu_obj_test_ai_path(int objnum,int seg)
{
	object *obj = &Objects[objnum];

	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	Ail[objnum].goal_segment = seg;
	create_n_segment_path(obj,8,-1);
	dtu_obj_log("test: obj %d %s: mode %d, goal seg %d",objnum,dtu_object_name(obj),Ail[objnum].mode,seg);
}

// 0x170BC create_path_to_player
static void create_path_to_player(object *obj,int max_length)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (max_length < 0)
		max_length = 10;
	ail->time_player_seen = GameTime;
	ail->goal_segment = ConsoleObject->segnum;
	path_set(obj,find_path(obj,obj->segnum,ail->goal_segment,ail->path,DTU_PATH_MAX,max_length,1,-1),0);
	ail->mode = DAIM_FOL_PATH;
	ail->awareness = 0;
}

// 0x171F0 create_path_to_station: the way home
static void create_path_to_station(object *obj,int max_length)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int home = obj->ctype.ai_info.hide_segment;

	if (home < 0 || home > Highest_segment_index)
		return;
	ail->time_player_seen = GameTime;
	ail->goal_segment = home;
	path_set(obj,find_path(obj,obj->segnum,home,ail->path,DTU_PATH_MAX,max_length,1,-1),2);
	ail->mode = DAIM_FOL_PATH;
	ail->awareness = 0;
}

// The point of entry k of a path. The executable has the centers of the
// segments. The port has the center of the side to the next segment, and
// the center of the last segment: a straight line from a point of a
// segment to the center of the next segment can leave the two segments.
static void path_point(dtu_ai_local *ail,int k,vms_vector *p)
{
	if (k < ail->path_len-1) {
		segment *segp = &Segments[ail->path[k]];
		int side;

		for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
			if (segp->children[side] == ail->path[k+1]) {
				compute_center_point_on_side(p,segp,side);
				return;
			}
	}
	compute_segment_center(p,&Segments[ail->path[k]]);
}

static void move_towards(object *obj,vms_vector *vec_to_player);
static void add_move(object *obj,vms_vector *dir);
static void turn_towards_vector(vms_vector *goal,object *obj,fix rate);

//------------------------------------------------------------------------
// ASCENT: Dtu_improved_pathfinding, paths for the body of a person.
//
// The path of the executable goes through the centers of the segments, and
// the port of Dtu_fix_path_follow through the centers of the sides between
// two segments. A creature that walks to the center of an opening from the
// side touches the corner of the opening with its body, and the move of
// the executable stops a body at a wall. Here each opening of the path has
// two points: one in front of the opening and one behind it, both on the
// line through the center of the opening that goes from the center of one
// segment to the center of the next, at the radius of the body from the
// opening. So the body goes through the opening straight. A creature goes
// to a later point at once when the line to it is free for its body
// (body_line_free()). When it does not come nearer to its point for 1.5
// seconds it goes back to the point in front of the opening, and after
// that it plans a new path that leaves out the segment behind the opening.
// The move of the body slides along a wall (dtu_creature_physics_sim()).

// The radius of the body for the paths: the torso cylinder of the model,
// 1.0 to 6.0.
static fix path_body_radius(object *obj)
{
	vms_vector c;
	fix r, hh;

	dtu_get_body_cylinder(obj,&c,&r,&hh);
	if (r < F1_0)
		r = F1_0;
	if (r > i2f(6))
		r = i2f(6);
	return r;
}

// 1 if a body of the radius of obj can go in a straight line from its
// center to p (walls only, objects do not count). The walls get the radius
// of the body, the floor one half of it, the ceiling nothing (the query of
// fvi.c with Fvi_dtu_body 1 adds 1.0 to the radius for the walls).
static int body_line_free(object *obj,vms_vector *p)
{
	fvi_query fq;
	fvi_info hit;
	fix r = path_body_radius(obj) - F1_0, cyl_r, hh;
	vms_vector c;
	int fate;
	int k;

	// not less than the radius of the move (dtu_creature_physics_sim(): a
	// fourth of the cylinder, plus 1.0 for the walls) and a small margin.
	// For a small body (a rat) the line was free where its move hit the
	// corner of an opening.
	dtu_get_body_cylinder(obj,&c,&cyl_r,&hh);
	if (r < cyl_r/4 + F1_0/4)
		r = cyl_r/4 + F1_0/4;

	// The line to p, and the line at the height of the center of the body
	// (the body walks level, a line that goes up a stair can be free where
	// the body is not)
	for (k=0;k<2;k++) {
		vms_vector q = *p;

		if (k)
			q.y = obj->pos.y;
		fq.p0 = &obj->pos;
		fq.p1 = &q;
		fq.startseg = obj->segnum;
		fq.rad = r;
		fq.thisobjnum = obj-Objects;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		Fvi_dtu_body = 1;
		Fvi_dtu_rad2 = 0;
		fate = find_vector_intersection(&fq,&hit);
		Fvi_dtu_body = 0;
		if (fate != HIT_NONE)
			return 0;
		if (is_flying(obj))
			break;
	}
	return 1;
}

// The point t of the path of obj: 2k is in front of the opening from
// path[k] to path[k+1], 2k+1 behind it, and 2(len-1) is the center of the
// last segment.
static void path_target(object *obj,int t,vms_vector *p)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int k = t >> 1, side;
	segment *segp;
	vms_vector c, a, b, d;
	fix off, lim;

	if (k >= ail->path_len-1) {
		compute_segment_center(p,&Segments[ail->path[ail->path_len-1]]);
		return;
	}
	segp = &Segments[ail->path[k]];
	for (side=0;side<MAX_SIDES_PER_SEGMENT;side++)
		if (segp->children[side] == ail->path[k+1])
			break;
	if (side == MAX_SIDES_PER_SEGMENT) {
		compute_segment_center(p,segp);
		return;
	}
	compute_center_point_on_side(&c,segp,side);
	compute_segment_center(&a,segp);
	compute_segment_center(&b,&Segments[ail->path[k+1]]);
	vm_vec_sub(&d,&b,&a);
	d.y = 0;
	if (vm_vec_normalize(&d) < 0x100) {
		*p = c;
		return;
	}
	// the radius of the body plus 0.5, but not farther than 0.4 of the way
	// to the center of the segment
	off = path_body_radius(obj) + F1_0/2;
	vm_vec_sub(&a,(t & 1) ? &b : &a,&c);
	a.y = 0;
	lim = fixmul(vm_vec_mag(&a),F1_0*2/5);
	if (off > lim)
		off = lim;
	vm_vec_scale_add(p,&c,&d,(t & 1) ? off : -off);
}

static int find_path(object *obj,int start,int end,short *out,int max_out,int max_depth,int random_flag,int avoid_seg);

// path_advance() of Dtu_improved_pathfinding. Returns 0 at the end of the
// path, else 1 with the point to walk to in *goal.
static int path_advance_body(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	int last = 2*(ail->path_len-1), t = ail->path_tgt, k, j;
	fix threshold = fixmul(vm_vec_mag_quick(&obj->mtype.phys_info.velocity),FrameTime)*2 + F1_0 + F1_0/2;
	vms_vector v;
	fix dist;

	if (ail->path_len < 2)
		return 0;
	if (t < 0)
		t = 0;

	// A creature that is in a later segment of the path is through the
	// openings before it (it still walks to the point behind the last one).
	for (k=ail->path_len-1;k>0 && 2*k-1>t;k--)
		if (ail->path[k] == obj->segnum) {
			t = 2*k-1;
			break;
		}

	// the points that the creature has reached
	for (;;) {
		if (t > last)
			return 0;
		path_target(obj,t,goal);
		vm_vec_sub(&v,goal,&obj->pos);
		if (!is_flying(obj))
			v.y = 0;
		if (vm_vec_mag(&v) >= threshold)
			break;
		t++;
		if (t > ail->stall_tgt)
			ail->replans = 0;
	}

	// A later point that the body can walk to in a straight line: the
	// creature takes the short way (three points at most, on a reference
	// tick).
	if (Ref_ticks_now > 0)
		for (j=(t+3 < last) ? t+3 : last;j>t;j--) {
			vms_vector p;

			path_target(obj,j,&p);
			if (body_line_free(obj,&p)) {
				t = j;
				*goal = p;
				break;
			}
		}

	// The progress to the point. A creature that does not get 1.0 nearer
	// in 1.5 seconds while it walks is blocked.
	vm_vec_sub(&v,goal,&obj->pos);
	if (!is_flying(obj))
		v.y = 0;
	dist = vm_vec_mag(&v);
	if (t != ail->prog_tgt || anim_speed(obj,-1) == 0 || obj->ctype.ai_info.CURRENT_STATE == DAIS_ATAK) {
		ail->prog_tgt = t;
		ail->prog_time = GameTime;
		ail->prog_dist = dist;
	}
	else if (dist < ail->prog_dist - F1_0) {
		ail->prog_dist = dist;
		ail->prog_time = GameTime;
	}
	else if (GameTime - ail->prog_time > F1_0*3/2) {
		int goal_seg = ail->path[ail->path_len-1], avoid = -1, len;

		ail->replans++;
		if ((t & 1) && ail->replans == 1) {
			// back to the point in front of the opening
			ail->stall_tgt = t;
			t--;
			dtu_obj_log("path: obj %d %s: blocked at the opening from %d to %d, goes back in front of it",
				(int)(obj-Objects),dtu_object_name(obj),ail->path[t>>1],ail->path[(t>>1)+1]);
		}
		else if (ail->replans > 3) {
			// no way for the body: the AI goes on without the path
			dtu_obj_log("path: obj %d %s: blocked, gives up the path",(int)(obj-Objects),dtu_object_name(obj));
			ail->replans = 0;
			ail->stall_tgt = -1;
			ail->path_len = 0;
			ail->path_tgt = 0;
			return 0;
		}
		else {
			// a new path, without the segment behind the opening
			if ((t >> 1) + 1 < ail->path_len && ail->path[(t >> 1) + 1] != goal_seg)
				avoid = ail->path[(t >> 1) + 1];
			len = find_path(obj,obj->segnum,goal_seg,ail->path,DTU_PATH_MAX,-1,0,avoid);
			if (len < 2 || ail->path[len-1] != goal_seg)
				len = find_path(obj,obj->segnum,goal_seg,ail->path,DTU_PATH_MAX,-1,0,-1);
			dtu_obj_log("path: obj %d %s: blocked, new path of %d segments from %d to %d without %d",
				(int)(obj-Objects),dtu_object_name(obj),len,obj->segnum,goal_seg,avoid);
			ail->path_len = len;
			ail->path_dir = 1;
			ail->stall_tgt = 2*DTU_PATH_MAX;
			t = 0;
			if (len < 2) {
				ail->path_tgt = 0;
				return 0;
			}
		}
		ail->prog_tgt = -1;
		path_target(obj,t,goal);
	}

	ail->path_tgt = t;
	ail->path_idx = t >> 1;
	return 1;
}

// 1 if the corrected rule of Dtu_fix_path_follow holds for this creature:
// a creature that is not hostile and does not flee keeps the rule of the
// executable (it turns to the player), so that the people of a level stay
// where they are.
static int path_fix_applies(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (!Dtu_fix_path_follow)
		return 0;
	// ASCENT: a creature on a path of statement E5 of its script walks to
	// the points of the path. With the rule of the executable it walks to
	// the player and does not come to the end of the path, where its script
	// goes on (the blocks CB E7 of the scripts 21 and 60).
	if (ail->script_walk == 1)
		return 1;
	return is_hostile(obj) || ail->mode == DAIM_RUN_FROM || obj->ctype.ai_info.behavior == DAIB_PATROL;
}

// 0x17CA8 ai_path_set_orient_and_vel. A creature that does not fly walks
// to the player and not to the point (0x17CFC). The corrected rule: it
// walks to the point.
static void path_steer(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	robot_info *ri = &Robot_info[obj->id];
	physics_info *pi = &obj->mtype.phys_info;
	vms_vector to_goal, vel;

	if (DTU_ANIM_STATE(obj) == DAS_DIE)
		return;

	if (!(Monster_info[obj->id].flags & MONSTER_F_FLYING)) {
		if (path_fix_applies(obj))
			vm_vec_sub(&to_goal,goal,&obj->pos);
		else
			vm_vec_sub(&to_goal,&ConsoleObject->pos,&obj->pos);
		to_goal.y = 0;
		if (vm_vec_normalize(&to_goal) > 0x100) {
			move_towards(obj,&to_goal);
			// Dtu_fix_npc_walk: a peaceful creature faces the player and
			// does not walk to the player (dtuobj.h)
			if (Dtu_fix_npc_walk && !path_fix_applies(obj) && !is_hostile(obj))
				ail->has_move = 0;
		}
		return;
	}

	vm_vec_sub(&to_goal,goal,&obj->pos);
	vm_vec_normalize_quick(&to_goal);
	vel = pi->velocity;
	vm_vec_normalize_quick(&vel);
	if (vm_vec_dot(&to_goal,&obj->orient.fvec) < -15*F1_0/16)
		vel = to_goal;
	else
		vm_vec_scale_add2(&vel,&to_goal,F1_0/2);
	vm_vec_normalize_quick(&vel);
	add_move(obj,&vel);
	turn_towards_vector(&to_goal,obj,(ail->mode == DAIM_RUN_FROM) ? ri->firing_wait[4]/2 : ri->firing_wait[Difficulty_level]);
}

// Moves the index of the path to the point that the creature walks to
// next. Returns 0 at the end of the path.
static int path_advance(object *obj,vms_vector *goal)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix threshold = fixmul(vm_vec_mag_quick(&obj->mtype.phys_info.velocity),FrameTime)*2 + i2f(2);
	int tries = 0, k;

	// Dtu_improved_pathfinding: the points of the body (not on the way back
	// of a patrol)
	if (improved_paths() && ail->path_dir > 0 && path_fix_applies(obj))
		return path_advance_body(obj,goal);

	// A creature that is in a later segment of the path is past the
	// points before it. (port)
	if (ail->path_dir > 0)
		for (k=ail->path_len-1;k>ail->path_idx;k--)
			if (ail->path[k] == obj->segnum) {
				ail->path_idx = k;
				break;
			}

	while (tries++ <= DTU_PATH_MAX) {
		vms_vector v;

		if (ail->path_idx < 0 || ail->path_idx >= ail->path_len)
			return 0;
		path_point(ail,ail->path_idx,goal);
		vm_vec_sub(&v,goal,&obj->pos);
		// The executable takes the distance in three dimensions. A
		// creature that walks is never as near as that to the center of a
		// high segment.
		if (!is_flying(obj))
			v.y = 0;
		if (vm_vec_mag(&v) >= threshold)
			return 1;
		ail->path_idx += ail->path_dir;
	}
	return 0;
}

// 0x17505 ai_follow_path
static void follow_path(object *obj,int visible,fix dist_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	int A = Monster_info[obj->id].ai_values[0][Difficulty_level];
	int afraid = (ail->mode == DAIM_RUN_FROM || A >= ail->morale);
	int pseg = ConsoleObject->segnum, beh = aip->behavior, k;
	vms_vector goal;


	if (ail->path_len == 0) {
		if (afraid) {
			create_n_segment_path(obj,5,-1);
			ail->mode = DAIM_RUN_FROM;
		}
		else
			create_path(obj);
	}

	if (ail->path_len < 2) {
		if (ail->mode == DAIM_RUN_FROM) {
			create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
			ail->mode = DAIM_RUN_FROM;
		}
		else
			ail->mode = DAIM_STILL;
		return;
	}

	if (ail->path_flag == 1 && (beh == DAIB_GUARD || beh == DAIB_HOME || (beh == DAIB_STATION && !visible)))
		return;

	// 0x17734: a creature that is farther than 200.0 from the player does
	// not move. The executable still counts its path index up (0x17411).
	// ASCENT: a creature on a path of statement E5 of its script
	// (Dtu_fix_script_walk) walks on, so that it comes to the end of the
	// path where its script goes on.
	if (dist_to_player > i2f(200) && ail->script_walk != 1) {
		if (ail->mode == DAIM_HOME)
			ail->mode = DAIM_STILL;
		return;
	}

	if (ail->mode == DAIM_RUN_FROM) {
		if (!visible && ail->awareness == 0) {
			fix f = F1_0 - FrameTime/2;

			vm_vec_scale(&obj->mtype.phys_info.velocity,f < F1_0/2 ? F1_0/2 : f);
			return;
		}
		for (k=ail->path_idx;k<ail->path_len;k++)
			if (k >= 0 && ail->path[k] == pseg) {
				create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
				ail->mode = DAIM_RUN_FROM;
				break;
			}
		if (visible) {
			ail->awareness = 1;
			ail->awareness_time = F1_0;
			morale_check(obj);			// 0x178AE
		}
		if (ail->path_len < 2)
			return;
	}

	if (ail->path_idx < 0)
		ail->path_idx = 0;

	if (!path_advance(obj,&goal)) {
		// ASCENT: the end of a path of statement E5 of a script (0x174C3
		// in the path index function 0x17411: the creature turns back on
		// the path and, with script flag 0, the script runs in mode 2).
		// The port stops the creature here; dtuscript.c runs the script.
		if (ail->script_walk == 1) {
			ail->script_walk = 2;
			ail->mode = DAIM_STILL;
			vm_vec_zero(&obj->mtype.phys_info.velocity);
			aip->GOAL_STATE = aip->CURRENT_STATE = DAIS_NONE;
			set_anim(obj,DAS_STAND);
			dtu_obj_log("obj %d %s: at the end of the path of its script (seg %d)",(int)(obj-Objects),dtu_object_name(obj),obj->segnum);
			return;
		}
		// 0x179C0: the end of the path
		if (ail->mode == DAIM_HOME) {
			ail->mode = DAIM_STILL;
			return;
		}
		if (beh == DAIB_GUARD || beh == DAIB_HOME || (beh == DAIB_STATION && !visible)) {
			create_path_to_station(obj,15);
			if (ail->path_len == 0 || ail->path[ail->path_len-1] != aip->hide_segment)
				ail->mode = DAIM_STILL;
			return;
		}
		if (ail->mode == DAIM_FOL_PATH && beh != DAIB_PATROL) {
			create_path_to_player(obj,10);
			return;
		}
		if (afraid) {
			create_n_segment_path(obj,7,(obj->segnum == pseg) ? -1 : pseg);
			ail->mode = DAIM_RUN_FROM;
			return;
		}
		// PATROL: back along the path. ASCENT: an empty path (cleared by
		// the step code) has no point to go back to; the index -1 read
		// path[-1], before the array.
		if (ail->path_len <= 0)
			return;
		ail->path_dir = -ail->path_dir;
		ail->path_idx += 2*ail->path_dir;
		if (ail->path_idx < 0) ail->path_idx = 0;
		if (ail->path_idx >= ail->path_len) ail->path_idx = ail->path_len-1;
		path_point(ail,ail->path_idx,&goal);
	}

	path_steer(obj,&goal);
}

// The corrected rule of Dtu_fix_path_follow for the mode CHASE_OB: a
// creature that does not see the player walks along a path to the segment
// of the player. The executable walks along the straight line and stops
// at the wall. Returns 1 if the creature has a path and moves on it.
static int chase_by_path(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	int pseg = ConsoleObject->segnum, need, k, len;
	vms_vector goal;

	if (obj->segnum == pseg) {
		ail->path_len = 0;
		return 0;
	}

	need = (ail->path_len < 2 || ail->path[ail->path_len-1] != pseg || ail->path_dir != 1);
	if (!need) {
		need = 1;
		for (k=0;k<ail->path_len;k++)
			if (ail->path[k] == obj->segnum)
				need = 0;
	}
	if (need) {
		if (ail->path_fail_time && GameTime >= ail->path_fail_time && GameTime - ail->path_fail_time < F1_0)
			return 0;
		len = find_path(obj,obj->segnum,pseg,ail->path,DTU_PATH_MAX,-1,0,-1);
		if (len < 2 || ail->path[len-1] != pseg) {
			dtu_obj_log("path: obj %d %s: no way from %d to the player in %d",objnum,dtu_object_name(obj),obj->segnum,pseg);
			ail->path_len = 0;
			ail->path_fail_time = GameTime ? GameTime : 1;
			return 0;
		}
		path_set(obj,len,0);
		ail->path_fail_time = 0;
		dtu_obj_log("path: obj %d %s: %d segments from %d to the player in %d",objnum,dtu_object_name(obj),len,obj->segnum,pseg);
	}

	if (!path_advance(obj,&goal))
		return 0;
	path_steer(obj,&goal);
	ail->chase_path_time = GameTime ? GameTime : 1;		// ASCENT: Dtu_fix_chase_run
	return 1;
}

// 0x123EF: the creature runs away from the player. If it moved less than
// 3.0 in three calls it walks to the side for 27 calls. The executable
// has one counter for all creatures and counts calls. The port has one
// for each creature, and with Dtu_fix_creature_move it counts reference
// frame times.
static void flee_from_player(object *obj,vms_vector *vec_to_player)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	static int shared_counter;
	static vms_vector shared_pos, shared_dir;
	int *counter = Dtu_fix_creature_move ? &ail->flee_counter : &shared_counter;
	vms_vector *pos = Dtu_fix_creature_move ? &ail->flee_pos : &shared_pos;
	vms_vector *dir = Dtu_fix_creature_move ? &ail->flee_dir : &shared_dir;
	int steps = Dtu_fix_creature_move ? Ref_ticks_now : 1;

	if (steps == 0 && vm_vec_mag_quick(dir) < 0x100) {
		*dir = *vec_to_player;
		vm_vec_negate(dir);
	}

	while (steps-- > 0) {
		if (*counter >= 30) {
			*pos = obj->pos;
			*counter = 0;
			DTU_ANIM_STATE(obj) = DAS_RUN;
		}
		if (*counter < 3) {
			if (vm_vec_dist(pos,&obj->pos) < i2f(3))
				(*counter)++;
			else {
				*pos = obj->pos;
				*counter = 0;
			}
			*dir = *vec_to_player;
			vm_vec_negate(dir);
		}
		else if (*counter == 3) {
			ail->retry_count = 1;
			if (is_flying(obj))
				make_random_vector(dir);
			else {
				*dir = obj->orient.rvec;
				if (rand() & 1)
					vm_vec_negate(dir);
				DTU_ANIM_STATE(obj) = DAS_WALK;
			}
			(*counter)++;
		}
		else
			(*counter)++;
	}

	turn_towards_vector(dir,obj,Robot_info[obj->id].firing_wait[Difficulty_level]);
	add_move(obj,dir);
}

static void move_relative_to_player(object *obj,fix dist,vms_vector *vec_to_player,fix circle_distance,int evade_only);
static int ai_to_anim_state(object *obj,int ai_state,fix dist);

// The blocked move of do_ai_frame (0x14BA1). Returns 1: the mode switch
// does not run in this frame.
// Dtu_improved_pathfinding: a hostile creature on a path of the body
// (path_advance_body()) slides along the wall and plans again by itself.
// The blocked move would make a new path (which starts the path again) or
// send it to a random side. Scenery in the way is still attacked.
static int body_path_handles_block(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (!improved_paths() || !path_fix_applies(obj) || ail->path_len < 2 || ail->path_dir <= 0)
		return 0;
	if (ail->mode != DAIM_CHASE_OB && ail->mode != DAIM_FOL_PATH)
		return 0;
	if (ail->blocker >= 0 && Objects[ail->blocker].type == OBJ_DRESSING && ail->blocked_count >= 5)
		return 0;
	return 1;
}

static int blocked_move(object *obj,fix dist,vms_vector *vec_to_player,int visible)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	robot_info *ri = &Robot_info[obj->id];
	physics_info *pi = &obj->mtype.phys_info;
	object *blocker = (ail->blocker < 0) ? ConsoleObject : &Objects[ail->blocker];
	fix rate = ri->firing_wait[Difficulty_level];
	vms_vector vec;
	int have_vec = 0, r, left;

	ail->retry_count = 0;

	// Scenery in the way for 5 blocked frames: the creature attacks it,
	// and after 10 it breaks.
	if (blocker->type == OBJ_DRESSING && ail->blocked_count >= 5) {
		vm_vec_sub(&vec,&blocker->pos,&obj->pos);
		vm_vec_normalize_quick(&vec);
		turn_towards_vector(&vec,obj,rate);
		set_anim(obj,ai_tick(objnum,2) ? DAS_ATTACK2 : DAS_ATTACK);
		ail->anim_start = GameTime;
		if (blocker->id < N_dressing_types && !(Dressing_info[blocker->id].flags & DRESSING_F_CANT_BREAK) && ail->blocked_count >= 10) {
			dtu_break_dressing(blocker,obj,0);
			aip->GOAL_STATE = DAIS_SRCH;
			ail->blocker = -1;
			ail->blocked_count = 0;
		}
		move_relative_to_player(obj,dist,vec_to_player,ri->turn_time[Difficulty_level],0);
		return 1;
	}

	if (ail->mode == DAIM_STILL)
		return 1;
	if (ail->mode == DAIM_RUN_FROM) {
		flee_from_player(obj,vec_to_player);
		return 1;
	}
	if (aip->behavior == DAIB_STATION)
		return 1;

	if (!visible || ail->activity == 0) {
		// "AI: %s looking for player!"
		ail->goal_segment = ConsoleObject->segnum;
		create_path(obj);
		ail->mode = DAIM_FOL_PATH;
		dtu_obj_log("obj %d %s: blocked, looks for the player, path of %d segments",objnum,dtu_object_name(obj),ail->path_len);
		return 1;
	}

	// The player is on the left side of the creature
	left = vm_vec_dot(vec_to_player,&obj->orient.rvec) < 0;

	r = Dtu_fix_ai_ticks ? rand() % 100 : (int)(((uint)Dtu_clock + (uint)GameTime + objnum) % 100);
	switch (r) {
		case 0:		// 0x14E8D: a new orientation with the right vector in front (guess: a turn of 90 degrees)
			vec = obj->orient.rvec;
			vm_vector_2_matrix(&obj->orient,&vec,NULL,NULL);
			check_and_fix_matrix(&obj->orient);
			DTU_ANIM_STATE(obj) = DAS_WALK;
			break;
		case 1:		// a random direction
			make_random_vector(&vec);
			if (!is_flying(obj))
				vec.y = 0;
			vm_vec_normalize_quick(&vec);
			turn_towards_vector(&vec,obj,rate);
			DTU_ANIM_STATE(obj) = DAS_WALK;
			have_vec = 1;
			break;
		case 2:
			move_relative_to_player(obj,dist,vec_to_player,ri->turn_time[Difficulty_level]*2,0);
			break;
		case 3:		// a turn to the side of the player
			if (is_flying(obj))
				make_random_vector(&vec);
			else {
				vec = obj->orient.rvec;
				if (left)
					vm_vec_negate(&vec);
				vec.y = 0;
			}
			DTU_ANIM_STATE(obj) = DAS_WALK;
			turn_towards_vector(&vec,obj,rate);
			have_vec = 1;
			break;
		case 4:
			aip->GOAL_STATE = DAIS_SRCH;
			DTU_ANIM_GOAL(obj) = ai_to_anim_state(obj,aip->CURRENT_STATE,dist);
			ail->anim_start = GameTime;
			break;
		default:		// 5 to 99: the step to the side
			if (is_flying(obj))
				make_random_vector(&vec);
			else {
				vec = obj->orient.rvec;
				DTU_ANIM_STATE(obj) = DAS_SIDLE_R;
				if (left) {
					vm_vec_negate(&vec);
					DTU_ANIM_STATE(obj) = DAS_SIDLE_L;
				}
				vec.y = 0;
			}
			have_vec = 1;
			break;
	}

	// 0x15030: the velocity is the vector times the speed of the
	// animation state (set, not added). The executable does this for the
	// cases 0, 2 and 4 too, with a vector that it did not set.
	if (have_vec) {
		vm_vec_normalize_quick(&vec);
		vm_vec_copy_scale(&pi->velocity,&vec,anim_speed(obj,-1));
		ail->move_add = pi->velocity;
		ail->has_move = 1;
		if (!is_flying(obj))
			pi->thrust = pi->velocity;
	}
	return 1;
}

// The sound of a creature that sees the player, see Dtu_fix_attack_sound
// (compute_vis_and_vec 0x12E2F to 0x12F3C). Call it before the time of the
// last sight is set for this frame.
static void creature_sight_sound(object *obj,int visible,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	robot_info *ri = &Robot_info[obj->id];
	int in_fov;

	if (!Dtu_fix_attack_sound || !is_hostile(obj) || Dtu_player_dead)
		return;
	in_fov = visible && vm_vec_dot(&obj->orient.fvec,vec_to_player) > ri->field_of_view[Difficulty_level];

	if (in_fov && !ail->prev_fov) {
		if (!ail->previous_visibility) {
			if (ail->time_player_seen + F1_0/2 < GameTime) {
				ail->time_attack_sound = GameTime;
				ail->next_misc_sound = GameTime + F1_0 + rand()*4;
			}
		}
		else if (ail->time_attack_sound + F1_0/4 < GameTime) {
			sound_at(ri->attack_sound,obj->segnum,&obj->pos,F1_0);
			ail->time_attack_sound = GameTime;
		}
	}
	if (in_fov && ail->next_misc_sound < GameTime) {
		ail->next_misc_sound = GameTime + (7 - Difficulty_level) * (rand() + F1_0) / 2;
		sound_at(ri->attack_sound,obj->segnum,&obj->pos,F1_0);
		dtu_obj_log("sound: obj %d %s attack sound %d",(int)(obj-Objects),dtu_object_name(obj),ri->attack_sound);
	}
	ail->prev_fov = in_fov;
}

// 0x10763: the "random turn". In 9 of 100 calls the creature turns to the
// player if it saw the player before. The rest of the function changes a
// rotation velocity that is 0 and stays 0.
static void random_turn(object *obj,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (ail->previous_visibility && rand() > 0x7400)
		turn_towards_vector(vec_to_player,obj,Robot_info[obj->id].firing_wait[Difficulty_level]);
}

// 0x10ADF. The morale check. The parts with values that the port does not
// have (the level of the creature 0xAE3BB, the level of the player, the
// function 0x359F4) count as 0.
void morale_check(object *obj)
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

	// ASCENT: 0x10ADF: a confused creature (effect bit 0x20, Confusion)
	// rolls rand() % 10 first: 0 "wanders" and 1 to 5 "stands confused"
	// (morale (A + B) / 2, goal state none, peaceful, not hostile and not
	// fleeing; standing also activity 0 and +0x1DB = GameTime + 60.0, whose
	// reader is not known), 6 to 8 "attacks" (morale B, chase, hostile),
	// 9 "acts normal" (the check below). The end of Confusion makes it
	// hostile again (dtu_creature_confusion_end).
	if (obj->dtu.m_88 & DTU_FX_CONFUSION) {
		int r = rand() % 10;

		if (r < 6) {
			ail->morale = (A + B) / 2;
			aip->GOAL_STATE = DAIS_NONE;
			obj->dtu.m_1b0 = (obj->dtu.m_1b0 & ~(DTU_HOSTILE|DTU_FLEEING)) | DTU_PEACEFUL;
			if (r >= 1)
				ail->activity = 0;
			dtu_obj_log("morale: obj %d %s is confused: %s",(int)(obj-Objects),dtu_object_name(obj),r < 1 ? "wanders" : "stands confused");
			return;
		}
		if (r < 9) {
			ail->morale = B;
			ail->mode = DAIM_CHASE_OB;
			aip->GOAL_STATE = DAIS_SRCH;
			obj->dtu.m_1b0 |= DTU_HOSTILE;
			dtu_obj_log("morale: obj %d %s is confused: attacks",(int)(obj-Objects),dtu_object_name(obj));
			return;
		}
		dtu_obj_log("morale: obj %d %s is confused: acts normal",(int)(obj-Objects),dtu_object_name(obj));
	}

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

// ASCENT: Dtu_fix_caster_melee, Dtu_fix_caster_cast and
// Dtu_fix_caster_idle (dtuobj.h, the last two are used in dtuspell.c).
// -origobj, -origcastermelee, -origcastercast and -origcasteridle clear
// them.
int Dtu_fix_caster_melee = 1;
int Dtu_fix_caster_cast = 1;
int Dtu_fix_caster_idle = 1;

// The choice of step 25 point 4 of do_ai_frame (docs/re/ai.md), also of
// do_firing_stuff (0x12916): the goal state of a creature that wants to
// attack. A melee creature, or one whose spell choice is -2 (ailp+0x2C: no
// spell, 0x92FAE and 0x91FDC): ATAK in reach, else LOCK. A creature with a
// mage or cleric level: ATAK only when it has chosen a spell (ailp+0x2D,
// cleared here). Any other: ATAK. reco 1: the choice of step 25 point 6
// (0x15A2A, both states RECO): 1.3 tests only the attack type there
// (0x15A60) and not -2, so a caster with -2 stays in RECO after its attack.
static void choose_attack_goal(object *obj,fix dist,int reco)
{
	ai_static *aip = &obj->ctype.ai_info;

	if (Robot_info[obj->id].attack_type == 1 || ((!reco || Dtu_fix_caster_melee) && dtu_spell_creature_melee(obj))) {
		fix reach = obj->size + ConsoleObject->size + i2f(2);

		aip->GOAL_STATE = (dist <= reach) ? DAIS_ATAK : DAIS_LOCK;
	}
	else if (dtu_spell_creature_caster(obj)) {
		if (dtu_spell_creature_take_ready(obj)) {
			aip->GOAL_STATE = DAIS_ATAK;
			dtu_obj_log("obj %d %s: has chosen a spell, attacks",(int)(obj-Objects),dtu_object_name(obj));
		}
	}
	else
		aip->GOAL_STATE = DAIS_ATAK;
}

// ASCENT: Dtu_improved_pathfinding. A creature that walks and fights in
// melee and sees the player above it, out of the reach of its attack (on a
// ladder, a ledge or a crate, where it cannot walk), backs away out of the
// reach of the player and waits there, facing the player. The executable
// has no rule for this: the creature walks under the player and stands
// there, and the player hits it with no risk. Creatures do not climb in the
// executable either (the ladder code 0x9EBEB is for the player only).
// Returns 1 if the rule moved the creature (ail->keep_off 1: it backs away,
// 2: it waits).
#define KEEP_OFF_AWAY	(F1_0*4)		// beyond the attack distance
#define KEEP_OFF_BAND	(F1_0*10)		// the creature waits in this band
static int keep_off_player(object *obj,fix dist,vms_vector *vec_to_player)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	fix attack, r, hh, hd;
	vms_vector d;

	if (!improved_paths() || Robot_info[obj->id].attack_type != 1 || is_flying(obj) || ail->script_walk == 1)
		return 0;
	// a jump or a fall of the player ends soon
	if (Dtu_player_state & (DTU_PS_JUMPING|DTU_PS_FALLING))
		return 0;
	attack = obj->size + ConsoleObject->size + i2f(2);		// choose_attack_goal()
	if (dist <= attack)
		return 0;
	// the feet of the player above the top of the body
	dtu_get_body_size(obj,&r,&hh);
	if (ConsoleObject->pos.y - DTU_BODY_HALF_HEIGHT <= obj->pos.y + hh)
		return 0;
	vm_vec_sub(&d,&ConsoleObject->pos,&obj->pos);
	d.y = 0;
	hd = vm_vec_normalize(&d);
	if (hd > attack + KEEP_OFF_AWAY + KEEP_OFF_BAND)
		return 0;
	if (hd < 0x100)
		d = obj->orient.fvec;			// right under the player
	d.y = 0;
	turn_towards_vector(vec_to_player,obj,Robot_info[obj->id].firing_wait[Difficulty_level]);
	if (hd < attack + KEEP_OFF_AWAY) {
		vm_vec_negate(&d);
		add_move(obj,&d);
		ail->keep_off = 1;
	}
	else
		ail->keep_off = 2;
	return 1;
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
	if (is_hostile(obj) && aip->GOAL_STATE != DAIS_DEAD && DTU_ANIM_STATE(obj) != DAS_DIE && !ail->keep_off &&
		 Robot_info[obj->id].attack_type == 1 && dist > i2f(30) && (ail->time_player_seen + F1_0/2 >= GameTime || chase_runs(ail))) {
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

		// 0x110F0: a ranged creature that attacks does not jump over the
		// fire frame of the state.
		if (Robot_info[obj->id].attack_type != 1 && aip->CURRENT_STATE == DAIS_ATAK && (s == DAS_ATTACK || s == DAS_ATTACK2)) {
			int ff = po->fire_frames[s - DAS_ATTACK];

			if (old_frame < ff && DTU_ANIM_FRAME(obj) > ff && old_frame >= first)
				DTU_ANIM_FRAME(obj) = ff;
		}
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
	ail->fired = 0;

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

// ASCENT: Dtu_fix_fall_active (dtuobj.h). -origobj clears it.
int Dtu_fix_fall_active = 1;

// 1 if a creature that does not fly has its feet 1.0 or more above the
// floor (the end of a fall in 0x6C662)
static int creature_in_air(object *obj)
{
	fix r, hh;

	if (is_flying(obj) || DTU_ANIM_STATE(obj) == DAS_DIE)
		return 0;
	dtu_get_body_size(obj,&r,&hh);
	return creature_floor_dist(obj,r,hh) >= F1_0;
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
	// 0x13D1A: no test while the player is hidden from the creature
	// (0x165C4: invisible or hidden in shadows)
	if (dtu_thief_hidden_from(obj))
		return 0;
	if (!brain_bit(obj->segnum,ConsoleObject->segnum))
		return 0;

	vm_vec_sub(&v,&ConsoleObject->pos,&obj->pos);
	vm_vec_normalize_quick(&v);
	dot = vm_vec_dot(&v,&obj->orient.fvec);

	// The player is behind the creature: it notices the player with a
	// chance that grows with its iq.
	if (dot <= 0) {
		// 0x13D8E: Move Silently (P+0xA0 bit 4) and a deaf creature
		if ((Dtu_thief_bits & DTU_TS_SILENT) || (obj->dtu.m_88 & 0x10))
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
	vms_vector vec_to_player, *believed;
	fix dist;
	int A, B, visible = -1, dying, blocked, hidden, keep_off;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return;
	// ASCENT: 0x13C8B, object flag 0x40 (FLAME object 35)
	if (dtu_creature_frozen(obj))
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
			// 0x13EFF: the alert 0x160AB(creature, 1); 0x13F09 and the
			// alert: the conditions of the spells
			dtu_creature_alert(obj,1);
			dtu_spell_creature_event(obj,DTU_CSC_WAKE|DTU_CSC_ALERT);
			// 0x13E25: the see sound, volume str * 0x1000
			sound_at(ri->see_sound,obj->segnum,&obj->pos,mi->str * 0x1000);
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
		// Dtu_improved_pathfinding: a hostile creature that follows a path
		// to the segment of the player stays active. The test above fails at
		// random when the player is behind the creature, which is often the
		// case on a path around a wall, and the creature then stands for
		// one animation before it sees the player again.
		// ASCENT: not for a hidden player (C20: the creature does not know
		// the segment of the player)
		if (ail->activity == 0 && improved_paths() && is_hostile(obj) && ail->path_len >= 2 && !cloak_applies(obj) &&
			 ail->path[ail->path_len-1] == ConsoleObject->segnum && (ail->mode == DAIM_CHASE_OB || ail->mode == DAIM_FOL_PATH) &&
			 ail->target != -1 && !Player_is_dead && !Dtu_player_dead)
			ail->activity = 2;
		// ASCENT: a creature on a path of statement E5 of its script stays
		// active to the end of the path (a port decision: the wake test fails
		// for a creature that walks away from the player, and the script
		// waits for the end of the path).
		if (ail->activity == 0 && ail->script_walk == 1)
			ail->activity = 2;
		// Dtu_fix_fall_active: a creature in the air stays active until it
		// lands, so that its physics runs (an inactive creature stands on
		// the bottom side of its segment, 0x13F96, also an open one)
		if (ail->activity == 0 && Dtu_fix_fall_active && creature_in_air(obj))
			ail->activity = 2;
		if (ail->activity == 0) {
			ail->anim_start = GameTime + FrameTime;
			aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
			DTU_ANIM_GOAL(obj) = DAS_REST;
			set_anim(obj,DAS_REST);
			dtu_obj_log("obj %d %s: lost the player",objnum,dtu_object_name(obj));
		}
	}

	// 0x1440C: a creature with "regeneration" gets one hit point each
	// "regeneration" seconds. The damage function clears the status bit
	// when the creature dies (0x2A1D0), so the part of this code that lets
	// a dead creature get up never runs for a creature that was killed.
	if (Monster_ext[obj->id].regeneration > 0 && (obj->dtu.m_88 & 0x01000000) && !dying) {
		if (GameTime < ail->regen_time)
			ail->regen_time = GameTime;
		if (GameTime - ail->regen_time >= i2f(Monster_ext[obj->id].regeneration)) {
			ail->regen_time = GameTime;
			if (obj->shields < i2f(mi->max_hit_points)) {
				obj->shields += F1_0;
				dtu_obj_log("obj %d %s: regenerates, hp %d",objnum,dtu_object_name(obj),f2i(obj->shields));
			}
		}
	}

	// 0x146D0: the distance to the player. ASCENT: 0x146CE, a hidden
	// player: to the believed position (cloak_position, C20)
	ail->time_since_processed += FrameTime;
	hidden = !dying && cloak_applies(obj);
	believed = hidden ? cloak_position(obj) : &ConsoleObject->pos;
	vm_vec_sub(&vec_to_player,believed,&obj->pos);
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

	// 0x1490B: a creature with a mage or a cleric level casts (dtuspell.c)
	dtu_spell_creature_frame(obj,dist,ail->activity == 2);

	// 0x1498B: a creature that is far away is not processed on each frame.
	// ASCENT: a creature on a path of its script is (Dtu_fix_script_walk),
	// else it walks only in the frames of the AI.
	// ASCENT: Dtu_fix_far_chase (dtuobj.h): also a hostile creature that is
	// active (it chases the player) is.
	if (ail->awareness < 3 && ail->script_walk != 1 && !(Dtu_fix_far_chase && ail->activity == 2 && is_hostile(obj))) {
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

		// ASCENT: a hidden player (C20): the creature turns to the believed
		// position when it sees that place (0x14A1C calls compute_vis_and_vec)
		if (hidden)
			visible = ray_is_free(&obj->pos,obj->segnum,believed,objnum);
		else
			visible = player_is_visible(obj);
		if (visible)
			ail->time_player_seen = GameTime;
		if (visible || ail->previous_visibility)
			turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
		// else: the random turn (0x10763), which does nothing here
		ail->previous_visibility = visible;
		log_creature_state(obj,"idle");
		return;
	}

	// 0x15074: the sight. ASCENT: a hidden player (C20): the ray to the
	// believed position (0x12CEF tests the brain bit there); the player is
	// not seen, the place where the creature believes it is.
	if (hidden)
		visible = ray_is_free(&obj->pos,obj->segnum,believed,objnum);
	else
		visible = player_is_visible(obj);
	creature_sight_sound(obj,hidden ? 0 : visible,&vec_to_player);
	if (visible)
		ail->time_player_seen = GameTime;

	// The awareness of a creature goes up by the awareness events
	// (awareness_events_frame, 0x16366). An earlier guess of the port in
	// their place (a hostile creature that sees the player has the
	// awareness 1 at least) is gone.

	// 0x14AF2: the door opener, for each creature with the door bit. It
	// only sends the creature to a door. The door opens when the creature
	// walks into it (dtu_creature_physics_sim). ASCENT: a creature on the
	// path of its script (Dtu_fix_script_walk) is not sent to a door: the
	// mode OPENDOOR would end the walk, and a door on its path opens when it
	// walks into it.
	if (Monster_ext[obj->id].door_opening && aip->behavior != DAIB_STATION && ail->mode != DAIM_CHASE_OB && ail->script_walk != 1) {
		if (ail->mode == DAIM_STILL) {
			if (visible)
				create_n_segment_path(obj,8+Difficulty_level,-1);
		}
		else {
			int side = openable_door_in_segment(obj);

			if (side != -1) {
				ail->mode = DAIM_OPENDOOR;
				aip->GOALSIDE = side;
			}
			else if (ail->mode != DAIM_FOL_PATH)
				create_n_segment_path(obj,8+Difficulty_level,-1);
		}
	}

	// 0x14BA1: the blocked move. The corrected rule runs it one time for
	// each reference frame time.
	blocked = 0;
	if (ail->retry_count && (DTU_ANIM_STATE(obj) == DAS_WALK || DTU_ANIM_STATE(obj) == DAS_RUN) && ail->blocker != 0 &&
		 (!Dtu_fix_creature_move || Ref_ticks_now > 0) && !body_path_handles_block(obj))
		blocked = blocked_move(obj,dist,&vec_to_player,visible);

	// 0x15074: the mode
	keep_off = ail->keep_off;
	if (!blocked)
		ail->keep_off = 0;
	if (!blocked)
	switch (ail->mode) {

		case DAIM_RUN_FROM:			// 0x14D65, 0x123EF
			flee_from_player(obj,&vec_to_player);
			break;

		case DAIM_FOL_PATH:			// 0x153B0
			follow_path(obj,visible,dist);
			if (aip->behavior != DAIB_PATROL && ail->morale >= B && is_hostile(obj) && aip->GOAL_STATE != DAIS_FLIN)
				aip->GOAL_STATE = DAIS_LOCK;
			// ASCENT: Dtu_fix_script_walk, the walk animation (the goal SRCH
			// gives WALK, 0x10148) for a creature on a path of its script
			else if (ail->script_walk == 1 && aip->GOAL_STATE != DAIS_FLIN)
				aip->GOAL_STATE = DAIS_SRCH;
			break;

		case DAIM_HOME:				// 0x1541A
			follow_path(obj,visible,dist);
			if (ail->morale >= B && is_hostile(obj) && !(ri->attack_type != 1 && aip->GOAL_STATE == DAIS_ATAK))
				aip->GOAL_STATE = DAIS_LOCK;
			break;

		case DAIM_OPENDOOR: {		// 0x156A5
			vms_vector c, v;

			compute_center_point_on_side(&c,&Segments[obj->segnum],aip->GOALSIDE % MAX_SIDES_PER_SEGMENT);
			vm_vec_sub(&v,&c,&obj->pos);
			vm_vec_normalize_quick(&v);
			turn_towards_vector(&v,obj,ri->firing_wait[Difficulty_level]);
			add_move(obj,&v);
			break;
		}

		case DAIM_WANDER:				// 0x15096
			if (!is_hostile(obj)) {
				if (ail->path_len == 0) {
					ail->goal_segment = rand() % (Highest_segment_index+1);
					create_path(obj);
				}
				follow_path(obj,visible,dist);
				break;
			}
			// a hostile creature: as CHASE_OB

		case DAIM_CHASE_OB:			// 0x150DD
		default: {
			fix circle = ConsoleObject->size;

			if (!is_hostile(obj))
				break;
			if (ri->attack_type != 1)
				circle = ri->turn_time[Difficulty_level] + ConsoleObject->size;		// the third group of $MONSTER_AI

			if (!visible && dist > i2f(80) && aip->behavior != DAIB_STATION) {
				// Dtu_improved_pathfinding: a hostile creature that knows
				// where the player is walks to the player on a path. The
				// executable makes a random path of 5 segments here on each
				// frame and goes back to CHASE_OB on the next frame (the
				// awareness is 1 or more), so the creature never moves.
				// ASCENT: no path to the segment of a hidden player (C20)
				if (!hidden && improved_paths() && Dtu_fix_path_follow && aip->behavior != DAIB_HOME && aip->behavior != DAIB_GUARD &&
					 ail->morale >= B && chase_by_path(obj))
					;
				else if (aip->behavior == DAIB_HOME || aip->behavior == DAIB_GUARD)
					create_path_to_station(obj,15);
				else if (improved_paths() && Dtu_fix_path_follow) {
					// with no way to the player: the creature walks along
					// the random path, and keeps it until its end
					int k, on = 0;

					for (k=0;k<ail->path_len;k++)
						if (ail->path[k] == obj->segnum)
							on = 1;
					if (!on || ail->path_flag != -1 || ail->path_len < 2)
						create_n_segment_path(obj,5,-1);
					follow_path(obj,visible,dist);
				}
				else
					create_n_segment_path(obj,5,-1);
				aip->GOAL_STATE = DAIS_SRCH;
			}
			else if (aip->CURRENT_STATE == DAIS_REST && aip->GOAL_STATE == DAIS_REST && visible && ail->morale >= B)
				aip->GOAL_STATE = DAIS_SRCH;
			else if (!hidden && GameTime - ail->time_player_seen > i2f(8)) {
				if (ail->morale >= B)
					create_path_to_player(obj,10);
			}
			else if (aip->CURRENT_STATE != DAIS_REST && aip->GOAL_STATE != DAIS_REST) {
				// Dtu_improved_pathfinding: also a creature that sees the
				// player but cannot walk to the player in a straight line
				// (the line is not free for its body) takes the path.
				// ASCENT: a hidden player: the line to the believed position,
				// and no path (C20)
				if (improved_paths() && Ref_ticks_now > 0)
					ail->body_line = hidden ? body_line_free(obj,believed) :
						(obj->segnum == ConsoleObject->segnum) || body_line_free(obj,&ConsoleObject->pos);
				if (!hidden && Dtu_fix_path_follow && (!visible || (improved_paths() && !ail->body_line)) && aip->behavior != DAIB_STATION && chase_by_path(obj)) {
					// as in the mode FOL_PATH (0x153F0)
					if (ail->morale >= B && aip->GOAL_STATE != DAIS_FLIN)
						aip->GOAL_STATE = DAIS_LOCK;
				}
				else if (hidden || !visible || !keep_off_player(obj,dist,&vec_to_player))
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
				else if (!visible && obj->segnum != aip->hide_segment && dist > i2f(80) &&
							(aip->behavior == DAIB_PATROL || aip->behavior == DAIB_HOME))
					create_path_to_station(obj,15);
			}
			else if (visible)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			break;
	}

	if (keep_off != ail->keep_off)
		dtu_obj_log("obj %d %s: %s",objnum,dtu_object_name(obj),ail->keep_off == 1 ? "the player is above and out of reach, backs away" :
			ail->keep_off == 2 ? "the player is above and out of reach, waits" : "the player is in reach again");

	// do_firing_stuff (0x12916)
	if (visible && is_hostile(obj) && ail->morale >= B) {
		fix dot = vm_vec_dot(&obj->orient.fvec,&vec_to_player);

		// 0x1293D: or the player is hidden (0x165C4), C20
		if ((dot >= 3*F1_0/4 || hidden) && aip->GOAL_STATE <= DAIS_ATAK && aip->GOAL_STATE != DAIS_FLIN) {
			choose_attack_goal(obj,dist,0);
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
			choose_attack_goal(obj,dist,0);
		if (aip->GOAL_STATE == DAIS_FLIN && aip->CURRENT_STATE == DAIS_FLIN)
			aip->GOAL_STATE = DAIS_LOCK;
		if (aip->GOAL_STATE == DAIS_RECO && aip->CURRENT_STATE == DAIS_RECO && ail->morale >= B)
			choose_attack_goal(obj,dist,1);
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
					choose_attack_goal(obj,dist,0);
				else if (!is_hostile(obj))
					aip->GOAL_STATE = DAIS_NONE;
			}
			break;

		case DAIS_SRCH:
		case DAIS_LOCK:
		case DAIS_RECO:
			// ASCENT: a creature on a path of its script faces the way it
			// walks (path_steer turns it)
			if (ail->script_walk == 1)
				break;
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[Difficulty_level]);
			else
				random_turn(obj,&vec_to_player);
			break;

		case DAIS_ATAK:
			if (visible && ail->morale >= B)
				turn_towards_vector(&vec_to_player,obj,ri->firing_wait[(ri->attack_type == 1) ? Difficulty_level : 4]);
			else
				random_turn(obj,&vec_to_player);
			// ai_do_actual_firing_stuff (0x1396E): a melee creature hits in
			// weapon_test(). A ranged creature fires in each call in which
			// the animation is on a fire frame of the model (0x13A2A). The
			// corrected rule of Dtu_fix_fire_once: one shot for each attack.
			if (is_hostile(obj)) {
				if (visible && vm_vec_dot(&obj->orient.fvec,&vec_to_player) >= 3*F1_0/4) {
					polymodel *po = obj_model(obj);

					if (ri->attack_type != 1 && po &&
						 (DTU_ANIM_FRAME(obj) == po->fire_frames[0] || DTU_ANIM_FRAME(obj) == po->fire_frames[1]) &&
						 !(Dtu_fix_fire_once && ail->fired)) {
						ail->fired = 1;
						Ai_believed = hidden ? believed : NULL;		// ASCENT: C20
						ai_fire(obj,dist);
						Ai_believed = NULL;
					}
					if (aip->behavior != DAIB_PATROL && (ail->mode == DAIM_STILL || ail->mode == DAIM_FOL_PATH)) {
						if (ail->morale >= B)
							ail->mode = DAIM_CHASE_OB;
						else if (ail->morale <= A)
							ail->mode = DAIM_RUN_FROM;
					}
				}
				aip->GOAL_STATE = DAIS_RECO;
			}
			break;
	}

	log_creature_state(obj,"ai");
}

//------------------------------------------------------------------------
// Calls from dtuhook.c

// ASCENT: for the scripts (dtuscript.c). The anger byte of the scripts is
// ai_local +0x00 (docs/re/scripts.md).
ubyte *dtu_script_anger(object *obj)
{
	return &Ail[obj-Objects].awareness;
}

// ASCENT: the end of the prison sequence of the scripts (0xAA61E to
// 0xAA661, dtuscript.c): behavior 0x80 and AI mode 0 for a creature of
// THIEF2; the new guard also has the player as its target (+0x1A2 = 0).
void dtu_script_prison_guard(object *obj,int set_target)
{
	if (obj->type != OBJ_ROBOT)
		return;
	obj->ctype.ai_info.behavior = DAIB_GUARD;
	Ail[obj-Objects].mode = DAIM_STILL;
	if (set_target)
		Ail[obj-Objects].target = 0;
}

// ASCENT: statement E5 of the scripts (walk a path, dtuscript.c). 0xAA74E
// for each segment of the statement: the segment is the station (+0x14C)
// and the goal (ai_local +0x06) of the creature and create_path (0x16FD9)
// makes the path to it, so the path of the last segment stays. 0xAA810
// after the list: no target, AI mode 2 (follow the path), the bits 0x30 of
// ai_local +0x05 and the morale (A + B) / 2.
void dtu_script_walk_segment(object *obj,int seg)
{
	if (obj->type != OBJ_ROBOT || seg < 0 || seg > Highest_segment_index)
		return;
	obj->ctype.ai_info.hide_segment = seg;
	Ail[obj-Objects].goal_segment = seg;
	create_path(obj);
}

// fix 1 (Dtu_fix_script_walk): the creature walks along the path with the
// walk animation (goal SRCH, animation WALK) and stays active to its end,
// also farther than 200.0 from the player. With fix 0 the rules of 1.3
// stay: the goal NONE of a creature that is not hostile gives the
// animation STAND and no speed, and the path follower turns it to the
// player (0x17CFC).
void dtu_script_walk_start(object *obj,int fix)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	char segs[96];
	int k, n = 0;

	if (obj->type != OBJ_ROBOT)
		return;
	ail->target = -1;
	ail->mode = DAIM_FOL_PATH;
	ail->flags |= 0x30;
	if (obj->id < N_robot_types)
		ail->morale = (Monster_info[obj->id].ai_values[0][Difficulty_level] + Monster_info[obj->id].ai_values[1][Difficulty_level]) / 2;
	ail->script_walk = 0;
	// ASCENT: a creature with no door bit gets no path through a segment
	// with a wall (0x16999), so a prisoner of PRISON (script 103) cannot
	// leave its cell. On the path of its script it opens a door with no key
	// and no lock as a creature with the door bit does.
	if (fix && ail->path_len < 2 && obj->id < N_robot_types && !Monster_ext[obj->id].door_opening &&
		 ail->goal_segment >= 0 && obj->segnum != ail->goal_segment) {
		Script_walk_doors = 1;
		create_path(obj);
		Script_walk_doors = 0;
	}
	if (fix && ail->path_len >= 2 && DTU_ANIM_STATE(obj) != DAS_DIE) {
		ail->script_walk = 1;
		ail->activity = 2;
		obj->ctype.ai_info.GOAL_STATE = obj->ctype.ai_info.CURRENT_STATE = DAIS_SRCH;
		DTU_ANIM_GOAL(obj) = DAS_WALK;
		set_anim(obj,DAS_WALK);
		ail->anim_start = GameTime;
	}
	else if (fix && DTU_ANIM_STATE(obj) != DAS_DIE && obj->segnum == ail->goal_segment)
		ail->script_walk = 2;		// in the segment already: at the end of the path
	segs[0] = 0;
	for (k=0;k<ail->path_len && n < (int)sizeof(segs)-8;k++)
		n += sprintf(segs+n,"%s%d",k ? " " : "",ail->path[k]);
	dtu_obj_log("obj %d %s: walks a path of %d segments to seg %d for its script (%s): %s",(int)(obj-Objects),dtu_object_name(obj),
		ail->path_len,ail->goal_segment,ail->script_walk ? "walk" : "1.3 rule",segs);
}

// 0: the creature is not on a path of a script. 1: it walks. 2: it is at
// the end of the path (the state goes back to 0).
int dtu_script_walk_state(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];

	if (obj->type != OBJ_ROBOT)
		return 0;
	if (ail->script_walk == 2) {
		ail->script_walk = 0;
		return 2;
	}
	if (ail->script_walk == 1 && (ail->mode != DAIM_FOL_PATH || ail->path_len < 2)) {
		// another rule of the AI took the creature off the path
		ail->script_walk = 0;
		return 0;
	}
	return ail->script_walk;
}

// ASCENT: the test of the warp check at 0x59DF2 (WEAPON 13 in the hand,
// dtuscript.c): the flee value A of the creature (Robot_info +0x80 for the
// difficulty) is below its morale (ai_local +0x28).
int dtu_script_creature_brave(object *obj)
{
	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	return Monster_info[obj->id].ai_values[0][Difficulty_level] < Ail[obj-Objects].morale;
}

// ASCENT: 0xA9418, a script makes the object hostile: chase mode, goal
// SRCH, animation RUN, anger 4, hostile bit (0xA9477 to 0xA9489 also clear
// the peaceful and the fleeing bit), target the player, the alert of level
// 4, morale. It does not wake the creature (no call of 0xA6505, no write
// of the activity +0x1CC): a creature that is not active stays where it is
// until its wake test sees the player (do_ai_frame, 0x13CE0). So the
// goblins of THIEF0, which the trigger of segment 207 makes hostile (script
// 50, label 900), wait in their camp.
void dtu_script_make_hostile(object *obj)
{
	dtu_ai_local *ail;

	if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types || obj->shields < F1_0 || DTU_ANIM_STATE(obj) == DAS_DIE)
		return;
	obj->dtu.m_1b0 |= DTU_HOSTILE;
	obj->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
	ail = &Ail[obj-Objects];
	ail->mode = DAIM_CHASE_OB;
	obj->ctype.ai_info.GOAL_STATE = DAIS_SRCH;
	set_anim(obj,DAS_RUN);
	ail->awareness = 4;
	ail->target = 0;
	dtu_creature_alert(obj,4);		// 0xA94A1
	morale_check(obj);
}

//------------------------------------------------------------------------
// ASCENT: the AI state of a creature in a state file of the original
// (SAVE\TEMP<L>.LVL, docs/re/levelstate.md): the 47-byte ai_local record
// (0x153EB0) and the fields of the 479-byte object image that the AI
// uses (docs/re/ai.md).

static int img_s16(const ubyte *p) { return (short)(p[0] | (p[1] << 8)); }
static int img_s32(const ubyte *p) { return (int)((uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24)); }
static void img_put16(ubyte *p, int v) { p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8); }
static void img_put32(ubyte *p, int v) { p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8); p[2] = (ubyte)(v >> 16); p[3] = (ubyte)(v >> 24); }

void dtu_ai_to_image(int objnum, ubyte *al, ubyte *img)
{
	dtu_ai_local *ail = &Ail[objnum];

	al[0x00] = ail->awareness;
	al[0x01] = ail->retry_count;
	// ASCENT: the port marks a script walk in the byte that 1.3 does not
	// use (consecutive_retries), see dtu_ai_from_image
	al[0x02] = ail->script_walk == 1 ? 1 : 0;
	al[0x03] = ail->mode;
	al[0x04] = ail->previous_visibility;
	al[0x05] = ail->flags;
	img_put16(al+0x06,ail->goal_segment);
	img_put32(al+0x10,ail->awareness_time);
	img_put32(al+0x14,ail->time_player_seen);
	img_put32(al+0x18,ail->time_attack_sound);
	img_put32(al+0x1C,ail->next_misc_sound);
	img_put32(al+0x20,ail->time_since_processed);
	img_put32(al+0x24,ail->anim_start);
	img_put32(al+0x28,ail->morale);

	img[0x1CC] = ail->activity;
	img[0x1AF] = ail->swing_done;
	img[0x1AE] = (img[0x1AE] & ~0x20) | (ail->falling ? 0x20 : 0);
	img_put16(img+0x138,ail->blocker);
	img_put16(img+0x13A,ail->blocked_count);
	img_put32(img+0x1A2,ail->target);
	img_put32(img+0x8C,ail->regen_time);
	img_put32(img+0x1C0,ail->spell_time);
}

void dtu_ai_from_image(int objnum, const ubyte *al, const ubyte *img)
{
	object *obj = &Objects[objnum];
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	int s = img_s16(img+0x170), f = img_s32(img+0x172), g = img_s32(img+0x176);

	// A creature that setup_creature() found dead stays so.
	if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI)
		return;
	ail->awareness = al[0x00] > 5 ? 5 : al[0x00];
	ail->retry_count = al[0x01];
	ail->previous_visibility = al[0x04];
	ail->flags = al[0x05];
	ail->goal_segment = img_s16(al+0x06);
	ail->awareness_time = img_s32(al+0x10);
	ail->time_player_seen = img_s32(al+0x14);
	ail->time_attack_sound = img_s32(al+0x18);
	ail->next_misc_sound = img_s32(al+0x1C);
	ail->time_since_processed = img_s32(al+0x20);
	ail->anim_start = img_s32(al+0x24);
	ail->morale = img_s32(al+0x28);
	// The port keeps the path in its own list, not in Point_segs: a
	// creature that walked a path plans a new one.
	if (al[0x03] == DAIM_STILL || al[0x03] == DAIM_WANDER || al[0x03] == DAIM_CHASE_OB || al[0x03] == DAIM_RUN_FROM)
		ail->mode = al[0x03];
	ail->path_len = ail->path_idx = 0;

	ail->activity = img[0x1CC] == 2 ? 2 : 0;
	ail->swing_done = img[0x1AF];
	ail->falling = (img[0x1AE] & 0x20) != 0;
	ail->target = img_s32(img+0x1A2);
	if (ail->target != 0)
		ail->target = -1;
	ail->regen_time = img_s32(img+0x8C);
	// 0xAE3F2 at the restore (0xBB1A7) sets +0x1A8 and +0x1C0 to the time
	// of the restore, after the copy of the saved object
	ail->spell_time = GameTime;
	// The hostile and script bits as the game left them (setup_creature()
	// made them again from the script of the level).
	obj->dtu.m_1b0 = img_s32(img+0x1B0);

	// The AI state and the animation, if the values fit the model
	if (img[0x142] <= DAIS_RECO && img[0x143] <= DAIS_RECO) {
		aip->CURRENT_STATE = img[0x142];
		aip->GOAL_STATE = img[0x143];
	}
	if (s >= 0 && s < DTU_N_ANIM_STATES && s != DAS_DIE && f >= Dtu_anim_first[s] && f < Dtu_anim_first[s+1]
		&& g >= 0 && g < DTU_N_ANIM_STATES && g != DAS_DIE) {
		DTU_ANIM_STATE(obj) = s;
		DTU_ANIM_FRAME(obj) = f;
		DTU_ANIM_GOAL(obj) = g;
	}

	// ASCENT: a creature on a path (AI mode 2). 1.3 keeps the path in
	// Point_segs of the state file, so the creature walks on after a load.
	// The port makes the path to the goal segment again, and a walk of a
	// script walks on and runs the script at its end. Statement E5
	// (0xAA74E) makes the segment both the station (+0x14C) and the goal
	// (ai_local +0x06) of a creature with a script; the bits 0x30 that
	// 0xAA810 sets do not mark it, the end of an animation state clears
	// them. So the port marks the walk in ai_local +0x02 (dtu_ai_to_image).
	// The station and the goal alone do not tell a walk: the door opener
	// (0x14AF2) gives a creature mode 2 with the goal segment 0, and the
	// creatures of the inn with the station 0 then walked to segment 0, in
	// front of the exit, after each restore of YPINN.
	if (al[0x03] == DAIM_FOL_PATH && ail->goal_segment >= 0 && ail->goal_segment <= Highest_segment_index &&
		 DTU_ANIM_STATE(obj) != DAS_DIE) {
		ail->mode = DAIM_FOL_PATH;
		create_path(obj);
		if (al[0x02] == 1 && aip->hide_segment == ail->goal_segment && (obj->dtu.m_1b0 & DTU_SCRIPT_MASK) != 0) {
			extern int Dtu_fix_script_walk;		// dtuscript.c

			dtu_script_walk_start(obj,Dtu_fix_script_walk);
		}
	}
}
