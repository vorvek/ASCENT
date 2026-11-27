/*
 * ASCENT: new file, split from dtuobj.c. The setup of the creatures at
 * the start of a level: init_ai_object, add_npc, the level fixes
 * (0xACCDB) and the creature setup by name (0xAC7B2). The shared
 * declarations are in dtuobj_int.h.
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
#include "dtuopts.h"	// ASCENT: Dtu_improved_ai
#include "dtucombat.h"	// ASCENT: the frame of the player for its body
#include "dtumovie.h"
#include "text.h"		// ASCENT: Text_string, the names of the creatures
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()
#include "dtulog.h"		// ASCENT: the session log
#include "dtuxp.h"		// ASCENT: the kill line, the death lines of the session log
#include "dtumiss.h"		// ASCENT: the miss sound of a creature
#include "dtuobj_int.h"

// init_ai_object (0x1038B) for a creature of a level, with the values that
// place_creatures (0x6A6FB) and add_npc (0xAC431) set.
int ail_keep_bits = 0;		// 1 while a creature is made in the game
int Dtu_script_level = 0;	// the level byte of the player (0x1D338D): the number of the level in DTU.MSN, from 0
int Level_setup_pass = 0;	// 1 while dtu_objects_level_setup sets up the objects of a level
static void add_npc(object *obj);

void init_creature_ai(object *obj)
{
	int objnum = obj-Objects;
	dtu_ai_local *ail = &Ail[objnum];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];
	int old_morale = ail->morale;

	memset(ail,0,sizeof(*ail));
	ail->last_logged_state = -1;
	ail->blocker = -1;
	// 0xAC764 (add_npc) and 0x103F8 (0x1038B): the height for the fall damage
	// (0x6C8C8). The restore (0xBB552) sets it for the player only, so a
	// restored creature keeps the height of the level file.
	ail->fall_start_y = obj->pos.y;

	if (aip->behavior < DAIB_GUARD || aip->behavior > DAIB_STATION)
		aip->behavior = DAIB_WANDER;
	if (aip->behavior == DAIB_STATION && Robot_info[obj->id].attack_type == 1)
		aip->behavior = DAIB_WANDER;

	ail->mode = (aip->behavior == DAIB_PATROL) ? DAIM_FOL_PATH : DAIM_STILL;
	ail->activity = 0;
	ail->awareness = 0;
	ail->awareness_time = i2f(3);
	ail->morale = (mi->ai_values[0][Difficulty_level] + mi->ai_values[1][Difficulty_level]) / 2;
	// Dtu_fix_ai_init (D-111): 0x1065D skips the robots at 0x10685, so a
	// creature of the level file keeps the morale of its object number
	if (!Dtu_fix_ai_init && Level_setup_pass && !ail_keep_bits)
		ail->morale = old_morale;
	ail->anim_start = ail->time_player_seen = ail->regen_time = GameTime;

	vm_vec_zero(&obj->mtype.phys_info.velocity);
	vm_vec_zero(&obj->mtype.phys_info.thrust);
	vm_vec_zero(&obj->mtype.phys_info.rotvel);

	// At the level start add_npc runs later, after the creature setup by
	// name (level_start_npcs, in the order of level_start 0xAD5BF).
	if (Level_setup_pass && !ail_keep_bits)
		return;
	add_npc(obj);
}

// add_npc (0xAC431) for a creature: its hostility, awareness and target.
static void add_npc(object *obj)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;

	// The entry of the NPC list (dtucombat.c). With Dtu_fix_npc_list 0 a
	// full list ends 0xAC431 at once (0xAC448): no hostility, awareness,
	// target and states for the new creature.
	if (!dtu_combat_npc_list_add(obj) && !Dtu_fix_npc_list) {
		dtu_obj_log("add_npc (0xAC431): obj %d %s, the NPC list is full (Dtu_fix_npc_list 0)",(int)(obj-Objects),dtu_object_name(obj));
		return;
	}

	// add_npc (0xAC431, code):
	// 1. A script id that does not belong to the level is cleared: the id
	//    divided by 10 is the number of the level in the mission file. The
	//    ids 0x37, 0x3F and 0x1FC to 0x1FF belong to each level.
	// 2. The creature is peaceful only if it has a script id and the
	//    script has a "say" statement (a byte 0xCD), or the bit "peaceful"
	//    is set.
	// 3. Each other creature is hostile, with the awareness 4.
	// A creature that is made in the game (a trap, a drop) has no level
	// record and keeps its bits.
	if (!ail_keep_bits) {
		int id = obj->dtu.m_1b0 & DTU_SCRIPT_MASK;
		int has_say;

		if (id != 0 && id != 0x37 && id != 0x3f && id < 0x1fc && id / 10 != Dtu_script_level) {
			obj->dtu.m_1b0 &= ~DTU_SCRIPT_MASK;
			id = 0;
		}
		// With no script file each creature with a script id is peaceful.
		has_say = (id != 0) ? dtu_script_has_say(id) : 0;
		if (id != 0 && (has_say != 0 || (obj->dtu.m_1b0 & DTU_PEACEFUL)))
			obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		else
			obj->dtu.m_1b0 |= DTU_HOSTILE;
	}
	if (is_hostile(obj)) {
		ail->target = 0;
		ail->awareness = ail_keep_bits ? 0 : 4;
	}
	else {
		ail->target = -1;
		ail->awareness = 0;
	}

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_NONE;
}

void setup_creature(object *obj)
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

	// The status bit of a creature that regenerates (set at creation if
	// "regeneration" of the monster table is not 0).
	// ASCENT: Dtu_fix_revive: a dead creature of a restored level keeps the
	// bit of the state file. A body that fire, acid or weapons destroyed
	// stays dead; a fallen body that can get up keeps it.
	if (Monster_ext[obj->id].regeneration && !(Dtu_fix_revive && Level_setup_restore && obj->shields < F1_0
		&& strnicmp(Monster_info[obj->id].name, "troll", 5) == 0))	// only a troll is a fallen body
		obj->dtu.m_88 |= 0x01000000;

	// A creature with less than one hit point is loaded dead (0x4787A).
	if (obj->shields < F1_0) {
		// ASCENT: Dtu_fix_revive: the hit points that the fallen body regrew
		// (dtu_creature_saved_shields), and its gold is left
		if (Dtu_fix_revive && Level_setup_restore) {
			dtu_ai_local *ail = &Ail[obj-Objects];

			ail->regen_hp = 0;
			if (obj->shields < 0 && (obj->dtu.m_88 & 0x01000000) && Monster_ext[obj->id].regeneration > 0)
				ail->regen_hp = obj->shields + i2f(Monster_info[obj->id].max_hit_points + 1);
			ail->regen_time = GameTime;
			ail->gold_done = 1;
			ail->flags = 0;
			obj->shields = 0;
			if (obj->dtu.m_88 & 0x01000000)
				dtu_obj_log("loaded fallen: obj %d %s, regrown hp %d (Dtu_fix_revive)",(int)(obj-Objects),dtu_object_name(obj),f2i(ail->regen_hp));
		}
		obj->ctype.ai_info.CURRENT_STATE = obj->ctype.ai_info.GOAL_STATE = DAIS_DEAD;
		DTU_ANIM_STATE(obj) = DAS_DIE;
		DTU_ANIM_FRAME(obj) = Dtu_anim_first[DAS_DIE+1] - 1;
		obj->flags |= OF_DESTROYED;
		obj->control_type = CT_NONE;
		obj->movement_type = MT_NONE;
		// Dtu_fix_corpse_floor: a corpse of a saved level (of the
		// executable or of an older port) lies on the floor.
		if (Obj_log) {
			dtu_obj_log("loaded dead: obj %d %s",(int)(obj-Objects),dtu_object_name(obj));
			log_corpse_height(obj,"corpse");
		}
		if (Dtu_fix_corpse_floor && corpse_on_floor(obj) && Obj_log)
			log_corpse_height(obj,"corpse");
	}
}

//------------------------------------------------------------------------
// 0xACCDB (called by level_start 0xAD5BF before the creature setup by name
// and add_npc): fixed changes of single objects and table entries for 16
// levels, by the level byte (the index in DTU.MSN). The argument is
// 0x5A742(level): 1 if the level has a saved state. The level start runs
// before the restore of the state (0x5AA75), so a restore keeps what the
// state file holds. The executable writes fixed object numbers with no test
// for most of them; the port tests the type that the level files have
// there. docs/re/objects.md, "Level fixes (0xACCDB)", has the list and what
// is not in the port (object flag 0x40, +0x1AE bit 2, YPINN object 41,
// +0x1A8, control type 0 of MYRKUL object 3).

int Level_setup_restore = 0;		// 1 while dtustate.c restores a level

// The flag word +0x98 of the monster table as MONSTER_F_ bits (ai.md, "flag
// words"). infravision has no bit.
static int monster_flags_from_table(int w)
{
	int f = 0;

	if (w & 0x0001) f |= MONSTER_F_BLIND;
	if (w & 0x0002) f |= MONSTER_F_ELECTRICITY_IMMUNE;
	if (w & 0x0004) f |= MONSTER_F_EXTRAPLANAR;
	if (w & 0x0008) f |= MONSTER_F_FIRE_IMMUNE;
	if (w & 0x0010) f |= MONSTER_F_FIRE_RESISTANT;		// ASCENT
	if (w & 0x0400) f |= MONSTER_F_MAGIC_RESISTANT;		// ASCENT: a table line (0xAE77F); the lich word 0x7042 has no such bit
	if (w & 0x0020) f |= MONSTER_F_FLYING;
	if (w & 0x0040) f |= MONSTER_F_HUMANOID;
	if (w & 0x0300) f |= MONSTER_F_LIGHT_SENSITIVE;
	if (w & 0x0800) f |= MONSTER_F_NO_SCARE;
	if (w & 0x1000) f |= MONSTER_F_NO_SLEEP;
	if (w & 0x2000) f |= MONSTER_F_UNDEAD;
	if (w & 0x4000) f |= MONSTER_F_WEAPON_RESISTANT;
	return f;
}

// The object n if it has the type (and the id when id >= 0)
static object *fix_obj(int n, int type, int id)
{
	object *o;

	if (n < 0 || n > Highest_object_index)
		return NULL;
	o = &Objects[n];
	if (o->type != type || (id >= 0 && o->id != id))
		return NULL;
	return o;
}

static void fix_log(int n, char *what)
{
	dtu_obj_log("level fix (0xACCDB), level %d: obj %d %s",Dtu_script_level,n,what);
}

static void fix_peaceful(int n)
{
	object *o = fix_obj(n,OBJ_ROBOT,-1);

	if (o) {
		o->dtu.m_1b0 |= DTU_PEACEFUL;
		fix_log(n,"peaceful");
	}
}

static void fix_script(object *o, int n, int script)
{
	o->dtu.m_1b0 = (o->dtu.m_1b0 & ~DTU_SCRIPT_MASK) | script;
	dtu_obj_log("level fix (0xACCDB), level %d: obj %d script %d",Dtu_script_level,n,script);
}

static void fix_delete(int n, int type, int id)
{
	object *o = fix_obj(n,type,id);

	if (o) {
		dtu_creature_remove(o,"a level fix (0xACCDB)");		// 0x6A3CC
		fix_log(n,"removed");
	}
}

// Object flag 0x40: the object queries skip the object (0x3F38F), so the
// player cannot walk into it, take it or use it, and shots pass it. It is
// still drawn. On a creature the AI frame (0x13C8B) and the move (0x9F174)
// skip it too (dtu_creature_frozen): FLAME object 35, the image of Lolth
// that script 320 moves and that speaks.
static void fix_nohit(int n, int type)
{
	object *o = fix_obj(n,type,-1);

	if (o) {
		o->flags |= OF_DTU_NOHIT;
		fix_log(n,"flag 0x40 (cannot be touched)");
	}
}

static void fix_spin(object *o, fix rate)
{
	o->movement_type = MT_SPINNING;
	vm_vec_zero(&o->mtype.spin_rate);
	o->mtype.spin_rate.y = rate;
}

static monster_info *fix_monster(int n, int *id)
{
	object *o = fix_obj(n,OBJ_ROBOT,-1);

	if (!o || o->id >= N_robot_types)
		return NULL;
	*id = o->id;
	return &Monster_info[o->id];
}

// The changes of the tables: at each setup, also after a restore, because
// the port reads the extra values of the monster table again at each setup.
void level_fixes_tables(int has_state)
{
	monster_info *mi;
	object *o;
	int id;

	switch (Dtu_script_level) {
		case 2:		// kobolds (0xACF23): the weapon of the type of object 19,
					// only with no state (then the creature setup gives the
					// weapon -33 of the table the weapon 33 and 1d4)
			// Dtu_fix_kobold_weapon: the weapon also with a state
			if ((!has_state || Dtu_fix_kobold_weapon) && fix_monster(19,&id))
				Monster_ext[id].weapon_type = 0x12;
			break;
		case 19:	// clan3 (0xAD157): the type of object 10
			if ((mi = fix_monster(10,&id)) != NULL) {
				mi->flags |= monster_flags_from_table(0x7042);
				Monster_ext[id].damage_dice = 1;
				Monster_ext[id].n_damage_dice = 10;
				Monster_ext[id].touch_spell = 0x16;
			}
			break;
		case 27:	// drow5 (0xAD25A): the type of object 83
			if ((mi = fix_monster(83,&id)) != NULL) {
				mi->armor_class = -3;
				mi->max_hit_points = 100;
				Robot_info[id].score_value = 9000;
				Monster_ext[id].damage_dice = 10;
				Monster_ext[id].n_damage_dice = 5;
			}
			if ((o = fix_obj(13,OBJ_DRESSING,-1)) != NULL && o->id < N_dressing_types)
				Dressing_info[o->id].pushable = 0xE;
			break;
		case 29:	// ibbalar (0xAD3AF): the scenery type of object 2 cannot break
			if ((o = fix_obj(2,OBJ_DRESSING,-1)) != NULL && o->id < N_dressing_types)
				Dressing_info[o->id].flags |= DRESSING_F_LEVEL_FIX;
			break;
		case 32:	// flame (0xAD3BB)
			if ((o = fix_obj(2,OBJ_DRESSING,-1)) != NULL && o->id < N_dressing_types) {
				Dressing_info[o->id].flags |= DRESSING_F_LEVEL_FIX;
				Dressing_info[o->id].pushable = 0x7FFF;
			}
			if ((o = fix_obj(3,OBJ_DRESSING,-1)) != NULL && o->id < N_dressing_types) {
				Dressing_info[o->id].flags |= DRESSING_F_LEVEL_FIX;
				Dressing_info[o->id].pushable = 0x7FFF;
			}
			// object 17 (Halaster) and object 36 (the avatar of Lolth)
			if ((mi = fix_monster(17,&id)) != NULL) {
				mi->max_hit_points = 0x100;
				mi->flags |= monster_flags_from_table(0x5CBA);
				Robot_info[id].score_value = 10000;
				Monster_ext[id].weapon_type = 0x21;
				Monster_ext[id].damage_dice = 10;
				Monster_ext[id].n_damage_dice = 10;
				Monster_ext[id].regeneration = 1;
			}
			if ((mi = fix_monster(36,&id)) != NULL) {
				mi->max_hit_points = 0x100;
				mi->flags |= monster_flags_from_table(0x5C01);
				Robot_info[id].attack_type = 1;
				Monster_ext[id].weapon_type = 0x21;
				Monster_ext[id].damage_dice = 10;
				Monster_ext[id].n_damage_dice = 5;
			}
			break;
	}
}

// 0xACD3B, ypinn: the eight guests of the inn that are not dead (low three
// flags clear) lose the hostility, the awareness goes one step to zero and
// the AI starts again. The flag of object 41 (+0x144 bit 0, a piece of
// scenery) is not in the port.
static void level_fixes_inn(void)
{
	static const int guests[8] = { 1, 2, 16, 17, 27, 28, 30, 32 };
	int k;

	for (k=0;k<8;k++) {
		object *o = fix_obj(guests[k],OBJ_ROBOT,-1);
		dtu_ai_local *ail;

		if (!o || (o->flags & 7) || o->id >= N_robot_types)
			continue;
		ail = &Ail[guests[k]];
		o->dtu.m_1b0 &= ~DTU_HOSTILE;
		ail->awareness = (ail->awareness > 0 && ail->awareness < 128) ? ail->awareness - 1 : 0;
		ail->target = -1;
		ail->morale = (Monster_info[o->id].ai_values[0][Difficulty_level] + Monster_info[o->id].ai_values[1][Difficulty_level]) / 2;
		o->ctype.ai_info.CURRENT_STATE = o->ctype.ai_info.GOAL_STATE = DAIS_NONE;
		DTU_ANIM_STATE(o) = DAS_STAND;
		DTU_ANIM_GOAL(o) = DAS_STAND;
		DTU_ANIM_FRAME(o) = Dtu_anim_first[DAS_STAND];
		ail->anim_start = GameTime;
		ail->mode = DAIM_STILL;
	}
}

// 0xAD744 after the level fixes: the texture numbers of lava02, lava,
// lavaflo and magma (0x1B2B1 gives 0 for a name that the level does not
// have). 0xACCDB of BANE reads them before this lookup.
static int Lava_textures[4];

void lava_textures_lookup(void)
{
	static const char *names[4] = { "lava02", "lava", "lavaflo", "magma" };
	int k, i;

	for (k=0;k<4;k++) {
		int n = strlen(names[k]);

		Lava_textures[k] = 0;
		for (i=0;i<NumTextures;i++)
			if (!strnicmp(TmapInfo[i].filename,names[k],n) && (TmapInfo[i].filename[n] == 0 || TmapInfo[i].filename[n] == '.')) {
				Lava_textures[k] = i;
				break;
			}
	}
}

// The changes of the objects: only at the load of the level, before the
// restore of a state. They run after the setup of the objects (setup_object
// at the load in the executable) and before the creature setup by name.
void level_fixes_objects(int has_state)
{
	object *o;

	switch (Dtu_script_level) {
		case 0:		// ypinn (0xACD3B): the eight guests that are not dead
			level_fixes_inn();
			break;
		case 2:		// kobolds (0xACF0C), only with no state
			if (has_state)
				break;
			if ((o = fix_obj(19,OBJ_ROBOT,-1)) != NULL)
				Ail[19].spell_time = GameTime;		// +0x1C0 (+0x1A8 is not in the port)
			if ((o = fix_obj(6,OBJ_ARMOR,-1)) != NULL) {
				o->id = 0x27;
				fix_log(6,"armor id 0x27");
			}
			fix_delete(69,OBJ_WEAPON,0x37);
			break;
		case 3:		// bones (0xACF6F)
			if ((o = fix_obj(55,OBJ_ROBOT,-1)) != NULL) {
				o->dtu.m_1b0 |= DTU_PEACEFUL;
				o->shields = i2f(40);
				fix_log(55,"peaceful, 40 hit points");
			}
			// init_ai_object(n, 0x80, segment): the behavior guard (the
			// mode of init_creature_ai for a guard)
			if ((o = fix_obj(11,OBJ_ROBOT,-1)) != NULL) {
				o->ctype.ai_info.behavior = DAIB_GUARD;
				Ail[11].mode = DAIM_STILL;
			}
			if ((o = fix_obj(23,OBJ_ROBOT,-1)) != NULL) {
				o->ctype.ai_info.behavior = DAIB_GUARD;
				Ail[23].mode = DAIM_STILL;
			}
			break;
		case 5:		// thief0 (0xACFBE)
			fix_peaceful(48);
			break;
		case 7:		// thief2 (0xACFD3)
			fix_peaceful(93);
			if ((o = fix_obj(1,OBJ_ROBOT,-1)) != NULL)
				fix_script(o,1,0x47);
			fix_peaceful(1);
			break;
		case 9:		// bane (0xAD003): the weapon 13 is removed, two pieces
					// of scenery spin
			{
				// One of the four lava textures that 0xAD744 looked up
				// (the cache of the previous level start in the
				// executable; Dtu_fix_bane_texture: of this level)
				int choice = rand() % 4, k;

				if (Dtu_fix_bane_texture)
					lava_textures_lookup();
				fix_delete(53,OBJ_WEAPON,13);
				for (k=26;k<=27;k++)
					if ((o = fix_obj(k,OBJ_DRESSING,-1)) != NULL) {
						fix_spin(o,0xCCC);
						if (o->render_type == RT_POLYOBJ)
							o->rtype.pobj_info.tmap_override = Lava_textures[choice];
						dtu_obj_log("level fix (0xACCDB), level 9: obj %d texture %d (lava texture %d)",k,Lava_textures[choice],choice);
					}
			}
			break;
		case 11:	// mine (0xAD0A1)
			fix_peaceful(1);
			break;
		case 12:	// crypts (0xAD0AB)
			fix_peaceful(74);
			break;
		case 13:	// mulhor1 (0xAD0C0): six of the seven Ankhs (misc 14)
					// cannot be touched; object 33 (segment 585) is the one
					// that the player can take
			fix_nohit(91,OBJ_MISC);
			fix_nohit(73,OBJ_MISC);
			fix_nohit(1,OBJ_MISC);
			fix_nohit(2,OBJ_MISC);
			fix_nohit(34,OBJ_MISC);
			fix_nohit(60,OBJ_MISC);
			break;
		case 16:	// myrkul (0xAD129): object 3 is held with no end
			if ((o = fix_obj(3,OBJ_ROBOT,-1)) != NULL) {
				o->dtu.m_88 |= DTU_FX_HOLD;
				o->dtu.m_c0[0] = DTU_FX_NO_END;
				o->dtu.m_1b0 |= DTU_PEACEFUL;
				fix_log(3,"held, peaceful");
			}
			break;
		case 21:	// drow2 (0xAD198)
			if ((o = fix_obj(43,OBJ_ROBOT,-1)) != NULL) {
				o->dtu.m_1b0 &= ~DTU_PEACEFUL;
				fix_log(43,"not peaceful");
			}
			if ((o = fix_obj(42,OBJ_DRESSING,-1)) != NULL)
				fix_script(o,42,0xD5);
			break;
		case 22:	// drow3: no level fix in 1.3
			// ASCENT, Dtu_fix_illithid_scroll (dtuobj.h): the lone mind
			// flayer (object 128, "flayer") carries the Scroll of the
			// Illithids that lies next to it (object 129, misc 126). Only
			// with no state: a saved level keeps its objects.
			if (Dtu_fix_illithid_scroll && !has_state) {
				object *s = fix_obj(129,OBJ_MISC,126);

				o = fix_obj(128,OBJ_ROBOT,-1);
				if (o && s && o->segnum == s->segnum && o->contains_type <= 0 && o->id < N_robot_types
						&& !stricmp(Monster_info[o->id].name,"flayer")) {
					o->contains_type = OBJ_MISC;
					o->contains_id = 126;
					o->contains_count = 1;
					fix_log(128,"contains misc 126 (Dtu_fix_illithid_scroll)");
					fix_delete(129,OBJ_MISC,126);
				}
			}
			break;
		case 25:	// beholder (0xAD1B4)
			if ((o = fix_obj(109,OBJ_ROBOT,-1)) != NULL) {
				fix_script(o,109,0xFF);
				o->ctype.ai_info.behavior = DAIB_GUARD;
				Ail[109].mode = DAIM_STILL;
				o->ctype.ai_info.flags[6] = 1;
				o->shields = i2f(120);
				o->dtu.m_88 |= (int)0xE2013A09u;
				o->dtu.m_8c[1] = i2f(23);			// +0x90
				o->dtu.m_8c[4] = DTU_FX_NO_END;	// +0x9C
				o->dtu.m_8c[10] = DTU_FX_NO_END;	// +0xB4
				o->dtu.m_8c[11] = DTU_FX_NO_END;	// +0xB8
				o->dtu.m_8c[12] = DTU_FX_NO_END;	// +0xBC
				o->dtu.m_c8[0] = DTU_FX_NO_END;	// +0xC8
				o->dtu.m_e0[1] = DTU_FX_NO_END;	// +0xE4
				o->dtu.m_e8[3] = DTU_FX_NO_END;	// +0xF4
				o->dtu.m_e8[4] = DTU_FX_NO_END;	// +0xF8
				o->dtu.m_82 += 0xB4;
				o->dtu.m_80 = 9 + rand() % 4;
			}
			break;
		case 27:	// drow5 (0xAD2A1): the shields of object 83 (the hatchling)
			if ((o = fix_obj(83,OBJ_ROBOT,-1)) != NULL && o->id < N_robot_types) {
				// The executable reads the hit points of the type with the
				// object number (Robot_info[83] + 0x78, past the table: a
				// word that is 0 in each run). Dtu_fix_drow5_shields: the
				// hit points of the type of the object (100, the fix above).
				o->shields = Dtu_fix_drow5_shields ? i2f(Monster_info[o->id].max_hit_points) : 0;
				dtu_obj_log("level fix (0xACCDB), level %d: obj 83 shields %.0f",Dtu_script_level,f2fl(o->shields));
			}
			// (0xAD2B1)
			if ((o = fix_obj(13,OBJ_DRESSING,-1)) != NULL) {
				fix_script(o,13,0x111);
				fix_spin(o,0x147);
			}
			fix_delete(84,OBJ_ROBOT,-1);
			break;
		case 29:	// ibbalar (0xAD320)
			if ((o = fix_obj(90,OBJ_ROBOT,-1)) != NULL && o->shields >= F1_0
					&& !(o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) && o->contains_type <= 0) {
				o->contains_type = OBJ_WEAPON;
				o->contains_id = 0x3A;
				o->contains_count = 1;
				fix_log(90,"contains weapon 0x3A");
			}
			fix_peaceful(82);
			if ((o = fix_obj(2,OBJ_DRESSING,-1)) != NULL) {
				fix_script(o,2,0x126);
				fix_spin(o,1);
			}
			break;
		case 32:	// flame (0xAD3BB)
			// the altar and the Flamesword cannot be touched until the
			// death of Lolth (dtu_flame_lolth_dead)
			fix_nohit(2,OBJ_DRESSING);
			if ((o = fix_obj(3,OBJ_DRESSING,-1)) != NULL) {
				fix_script(o,3,0x142);			// the Flamesword
				o->control_type = CT_NONE;
				fix_spin(o,0x147);
				fix_nohit(3,OBJ_DRESSING);
			}
			if ((o = fix_obj(4,OBJ_MISC,-1)) != NULL)
				fix_script(o,4,0x143);			// the statue puzzle
			if ((o = fix_obj(17,OBJ_ROBOT,-1)) != NULL) {
				o->shields = i2f(0x100);
				o->dtu.m_88 |= 0x01000000;		// +0x8B bit 0
				o->ctype.ai_info.behavior = DAIB_WANDER;
			}
			if ((o = fix_obj(36,OBJ_ROBOT,-1)) != NULL) {
				if (!has_state)
					o->shields = i2f(0x100);
				o->dtu.m_88 = 0;
				o->dtu.m_1b0 &= ~DTU_FLEEING;
				Ail[36].spell_time = GameTime;		// +0x1C0 (+0x1A8 is not in the port)
				Ail[36].awareness = 4;
				Ail[36].target = 0;
				fix_log(36,"Lolth: 256 hit points, no effects");
			}
			// ASCENT: 0xAD4F2 to 0xAD4FB: object 35, if a creature
			fix_nohit(35,OBJ_ROBOT);
			break;
	}
}

// The hit points of scenery: the setup of the port gives each piece of
// scenery 1.0 (as setup_object does before level_start), so the fixes of
// the scenery hit points come after it.
void level_fixes_scenery_shields(void)
{
	object *o;

	if (Dtu_script_level == 27 && (o = fix_obj(13,OBJ_DRESSING,-1)) != NULL)
		o->shields = i2f(256);
	if (Dtu_script_level == 29 && (o = fix_obj(2,OBJ_DRESSING,-1)) != NULL)
		o->shields = i2f(256);
}

//------------------------------------------------------------------------
// The creature setup by name (0xAC7B2) and add_npc (0xAC431) in the
// object loop of level_start (0xAD5BF, calls 0xAD7A1 and 0xAD7C4). The
// setup writes into the shared entry of the creature type (Robot_info,
// Monster_info, Monster_ext), so a later creature of the same type sees
// what an earlier one changed: the weapon fallback runs only for the first
// creature of a type. docs/re/ai.md, "Creature setup by name".

// The names of the creature types that quest flag 67 (a member of the
// Shadow Thieves) makes peaceful (0xACA21)
static const char *Guild_names[15] = {
	"sfmtd2", "sfmtf2", "shmfb1", "shmfb5", "shmfc3", "shmfg1", "shmfg5",
	"shmtd2", "shmtd4", "shmtd6", "shmtf2", "shmtf4", "shmtf6", "shdleadf", "shdleadg"
};

// The name compare of 0xAC7B2: ASCII case folding, at most n characters
static int setup_name(const char *name, const char *key, int n)
{
	int i;

	for (i=0;i<n;i++) {
		int a = (ubyte) name[i], b = (ubyte) key[i];

		if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
		if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
		if (a != b)
			return 0;
		if (a == 0)
			return 1;
	}
	return 1;
}

static int guild_name(const char *name)
{
	int i;

	for (i=0;i<15;i++)
		if (setup_name(name,Guild_names[i],32))
			return 1;
	return 0;
}

// The mage level (Robot_info+0xE2) and the cleric level (+0xE3) of a
// type: the NPC table (0xA7E97), else spell_level= of the line (the
// cleric level only with cleric_points=)
static void type_caster_levels(int id, int *mage, int *cleric)
{
	int v[6], rec = Monster_info[id].record;

	if (dtu_npc_row(Monster_info[id].name,v)) {
		*mage = v[2];
		*cleric = v[4];
	} else {
		*mage = tbl_int(rec,"spell_level",0);
		*cleric = tbl_int(rec,"cleric_points",0) ? *mage : 0;
	}
	*mage &= 0xFF;
	*cleric &= 0xFF;
}

static int type_caster_level(int id)
{
	int mage, cleric;

	type_caster_levels(id,&mage,&cleric);
	return mage > cleric ? mage : cleric;
}

// The values of a type that the setup writes, for the log
static void type_setup_state(int id, char *s, int size)
{
	int mage, cleric;

	type_caster_levels(id,&mage,&cleric);
	snprintf(s,size,"weapon %d attack %d dice %dd%d score %d hp %d touch %d regen %d str %d door %d levels %d/%d",
		Monster_ext[id].weapon_type,Robot_info[id].attack_type,Monster_ext[id].n_damage_dice,Monster_ext[id].damage_dice,
		Robot_info[id].score_value,Monster_info[id].max_hit_points,Monster_ext[id].touch_spell,Monster_ext[id].regeneration,
		Monster_info[id].str,Monster_ext[id].door_opening,mage,cleric);
}

// 0xAC7B2 for one creature. objects 0 (a restored level): only the
// entry of the type, the state file has the object and its AI record.
static void creature_setup_by_name(object *obj, int objects)
{
	int id = obj->id, script = obj->dtu.m_1b0 & DTU_SCRIPT_MASK;
	monster_info *mi;
	robot_info *ri;
	monster_ext *me;
	char *name;
	char what[160], before[160];

	if (id >= N_robot_types)
		return;
	type_setup_state(id,before,sizeof(before));
	mi = &Monster_info[id];
	ri = &Robot_info[id];
	me = &Monster_ext[id];
	name = mi->name;
	what[0] = 0;

	// 0xAC80E: a spell as the weapon (a negative weapon type), not for six
	// names: weapon 0x21, projectile, d4 with the higher caster level as
	// the number of dice
	if (me->weapon_type < 0 && !setup_name(name,"hatchlng",32) && !setup_name(name,"fmephit",32)
			&& !setup_name(name,"imephit",32) && !setup_name(name,"lmephit",32)
			&& !setup_name(name,"flayer",32) && !setup_name(name,"flaynpc",32)) {
		snprintf(what+strlen(what),sizeof(what)-strlen(what)," weapon %d -> 33, %dd4",me->weapon_type,type_caster_level(id));
		me->weapon_type = 0x21;
		me->damage_dice = 4;
		me->n_damage_dice = type_caster_level(id);
		ri->attack_type = 0;
	}

	if (setup_name(name,"spectatr",32)) {
		ri->attack_type = 0;
		me->weapon_type = 0;
		me->damage_dice = 3;
		me->n_damage_dice = 2;
		strcat(what," spectatr");
	} else if (setup_name(name,"hatchlng",32)) {
		// 0x18: fire_immune and fire_resistant (fire_immune is tested first)
		mi->flags |= monster_flags_from_table(0x18);
		ri->score_value = 7000;
		strcat(what," hatchlng");
	} else if (setup_name(name,"gargoyle",32)) {
		me->weapon_type = 0x21;
		me->damage_dice = 4;
		me->n_damage_dice = 4;
		strcat(what," gargoyle");
	} else if (setup_name(name,"flayer",32) || setup_name(name,"flaynpc",32)) {
		ri->attack_type = 0;
		me->weapon_type = -22;
		ri->score_value = setup_name(name,"flayer",32) ? 3000 : 6000;
		me->regeneration = 0;
		me->damage_dice = 4;
		me->n_damage_dice = 2;
		if (objects)
			obj->dtu.m_88 &= ~0x01000000;		// the regeneration bit of the object
		strcat(what," flayer");
	} else if (setup_name(name,"lich",4)) {
		mi->flags |= monster_flags_from_table(0x7042);
		me->damage_dice = 1;
		me->n_damage_dice = 10;
		me->touch_spell = 22;
		strcat(what," lich");
	}

	// 0xACA21: quest flag 67 (the player is in the guild of the Shadow
	// Thieves). add_npc runs after it and can make the creature hostile again.
	if (objects && dtu_script_gflag(67) && guild_name(name)) {
		Ail[obj-Objects].awareness = 0;
		obj->dtu.m_1b0 &= ~DTU_HOSTILE;
		Ail[obj-Objects].target = -1;
		strcat(what," flag67 peaceful");
	}

	// 0xACC3C: the inn (level 0), each creature
	if (Dtu_script_level == 0) {
		me->door_opening = 1;
		mi->str = 19;
		ri->score_value = 10000;
		mi->max_hit_points = 200;
		if (objects) {
			obj->shields = i2f(200);
			Ail[obj-Objects].mode = DAIM_STILL;
		}
		switch (script) {		// the jump table 0xACC7B
			case 1: case 2: case 5: case 7:
				ri->attack_type = 1;
				me->weapon_type = 14;
				break;
			case 3: case 4: case 8:
				ri->attack_type = 0;
				me->weapon_type = (script == 3) ? 39 : 44;
				// +0x1C0 (and +0x1A8, which the port does not have)
				if (objects)
					Ail[obj-Objects].spell_time = GameTime;
				break;
		}
		snprintf(what+strlen(what),sizeof(what)-strlen(what)," inn script %d",script);
	}

	if (Obj_log || Setup_log) {
		char line[600], after[160];

		type_setup_state(id,after,sizeof(after));
		snprintf(line,sizeof(line),"creature setup (0xAC7B2), level %d: obj %d %s type %d script %d%s:%s; before %s; after %s",
			Dtu_script_level,(int)(obj-Objects),name,id,script,objects ? "" : " (type only)",what[0] ? what : " no change",
			before,after);
		dtu_obj_log("%s",line);
		if (Setup_log)
			dtulog_event("%s",line);
	}
}

// 0xAC3C3: an object with the same signature and script id is in the list
static int npc_listed(short *list, int n, object *obj)
{
	int i;

	for (i=0;i<n;i++) {
		object *o = &Objects[list[i]];

		if (o->signature == obj->signature && !((o->dtu.m_1b0 ^ obj->dtu.m_1b0) & DTU_SCRIPT_MASK))
			return 1;
	}
	return 0;
}

#define DTU_NPC_LIMIT	100		// 0xAC431: "I can't handle more than %d NPCs in a level"

// The setup by name and add_npc of the level start (0xAD786 to 0xAD7D6).
// setup 0: a restored level that has the entries of the types already.
void level_start_npcs(int setup)
{
	short list[DTU_NPC_LIMIT];
	int i, n = 0;

	dtu_combat_npc_list_clear();		// 0xAD779

	for (i=1;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_ROBOT && setup)
			creature_setup_by_name(obj,!Level_setup_restore);
		if (obj->type != OBJ_ROBOT && !(obj->dtu.m_1b0 & DTU_SCRIPT_MASK))
			continue;
		if (npc_listed(list,n,obj)) {
			dtu_obj_log("add_npc (0xAC431): obj %d %s is in the list, not added",i,dtu_object_name(obj));
			continue;
		}
		if (n >= DTU_NPC_LIMIT) {
			// the loop ends: the other objects get no setup
			dtu_obj_log("add_npc (0xAC431): more than %d NPCs, the setup ends at obj %d",DTU_NPC_LIMIT,i);
			break;
		}
		list[n++] = i;
		if (obj->type != OBJ_ROBOT || obj->control_type != CT_AI) {
			dtu_combat_npc_list_add(obj);		// 0xAD7C4 for an object with a script
			continue;
		}
		add_npc(obj);
		// Dtu_fix_guild_peace: the peace of quest flag 67 stays
		if (Dtu_fix_guild_peace && !Level_setup_restore && dtu_script_gflag(67) && obj->id < N_robot_types
				&& guild_name(Monster_info[obj->id].name) && is_hostile(obj)) {
			Ail[i].awareness = 0;
			obj->dtu.m_1b0 &= ~DTU_HOSTILE;
			Ail[i].target = -1;
			dtu_obj_log("add_npc (0xAC431): obj %d %s stays peaceful (quest flag 67, Dtu_fix_guild_peace)",i,dtu_object_name(obj));
		}
		if (Obj_log || Setup_log) {
			char line[160];

			snprintf(line,sizeof(line),"add_npc (0xAC431): obj %d %s script %d %s awareness %d target %d",
				i,dtu_object_name(obj),obj->dtu.m_1b0 & DTU_SCRIPT_MASK,is_hostile(obj) ? "hostile" : "peaceful",
				Ail[i].awareness,Ail[i].target);
			dtu_obj_log("%s",line);
			if (Setup_log)
				dtulog_event("%s",line);
		}
	}
}
