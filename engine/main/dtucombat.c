/*
 * ASCENT: new file. The weapon attack of the player and the arms of the
 * first person view (DTUGAME.EXE 1.3: game.c 0x44300, 0x41A4D, 0x4450C,
 * 0x43A5A; physics.c 0x6CFF8; collide.c 0x278B5, 0x29A9C; laser.c 0x56A0D,
 * 0x561DA; object.c 0x67C5A). See dtucombat.h and docs/re/combat.md.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "robot.h"
#include "polyobj.h"
#include "bm.h"
#include "vclip.h"
#include "weapon.h"
#include "player.h"
#include "gauges.h"
#include "lighting.h"
#include "render.h"
#include "gameseq.h"
#include "digi.h"
#include "fireball.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtutrig.h"
#include "dtucombat.h"
#include "dtuspell.h"
#include "dtuthief.h"

int Dtu_fix_one_hit = 1;
int Dtu_fix_haste_attack = 1;

extern char Dtu_player_archive[16];		// dtugame.c

//------------------------------------------------------------------------
// State. The executable keeps these values in the player object (0x170
// state, 0x172 frame, 0x176 goal, 0x1AF one hit byte) and in the player
// structure (0x1D3373 animation start). The port keeps them here: the
// player object of the port keeps the state STAND, so that the
// placeholder animation of dtuobj.c (player_animation) never runs.

static int Init_done = 0;
static int View_model = -1;			// object 0x162 of the player
static int View_letter = -1;			// the weapon letter of View_model
static int View_state = DAS_STAND;
static int View_goal = DAS_STAND;
static int View_frame = 0;
static fix View_anim_start = 0;
static int Pitch_offset = 1;			// 0x13B250: the arms move with the look pitch
static int Smack = 0;					// 0x1AF: one hit per swing
static int Request = 0;					// 0x13B69F: the attack key was pressed
static int Key_held = 0;				// Controls+0x19
static int Key_last = 0;				// 0x13AD7E
static fix Last_attack = -0x7fffffff;	// 0x13B74B
static fix Swing_time = F1_0;			// state_times[6] of the player model after 0x43A5A
static fix Fire_next = 0, Fire_last = -0x7fffffff;	// 0x215D74, 0x215D7C (hand 1)
static fix Swing_started = 0;			// for the log: the game time of the start of the swing

static int is_attack_state(int s)
{
	return s == DAS_ATTACK || s == DAS_ATTACK2 || s == DAS_ATTACK3 || s == DAS_ATTACK4;
}

static int tbl_int(int record,char *key,int def)
{
	char v[32];

	if (record < 0 || !bm_tbl_get(record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

static int weapon_int(int w,char *key,int def)
{
	if (w < 0 || w >= N_weapon_types)
		return def;
	return tbl_int(Weapon_item_info[w].record,key,def);
}

static int weapon_flag(int w,char *flag)
{
	if (w < 0 || w >= N_weapon_types || Weapon_item_info[w].record < 0)
		return 0;
	return bm_tbl_has_flag(Weapon_item_info[w].record,flag);
}

// 0x2E900 and 0x2E600, as in dtuobj.c: 256.0 and 200.0, volume 0.5 at most
static void sound_at(int sound,int segnum,vms_vector *pos,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS || segnum < 0 || segnum > Highest_segment_index)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_pos2(sound,segnum,0,pos,0,volume,i2f(256));
}

static void sound_on_object(int sound,object *obj,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_object2(sound,obj-Objects,0,volume,i2f(200));
}

static void vclip_at(int segnum,vms_vector *pos,fix size,int vclip)
{
	if (vclip < 0 || vclip >= VCLIP_MAXNUM || size <= 0 || segnum < 0)
		return;
	if (Vclip[vclip].num_frames <= 0 || Vclip[vclip].play_time <= 0)
		return;
	object_create_explosion(segnum,pos,size,vclip);
}

//------------------------------------------------------------------------
// The item in the hand

// The inventory entry of the item in the hand, or NULL (the empty hand).
static dtu_inv_item *hand_entry(void)
{
	int n = Dtu_char.current.n;

	if (n < 0 || n >= Dtu_n_inventory || Dtu_inventory[n].type == 0)
		return NULL;
	return &Dtu_inventory[n];
}

// The weapon type of the item in the hand, or -1.
static int hand_weapon(void)
{
	dtu_inv_item *e = hand_entry();

	if (e && e->type == OBJ_WEAPON && e->id < N_weapon_types)
		return e->id;
	return -1;
}

// weapon +0x0A: 3 for "thrown"
static int is_thrown(int w)
{
	return w >= 0 && weapon_int(w,"thrown",0) != 0;
}

// The letter of the model of the player (0x43EB2 to 0x43F96): byte 5 of
// the inventory record, which is weapon_char= of the weapon. '_' is the
// empty hand, 'G' a weapon with no letter.
static int hand_letter(void)
{
	int w = hand_weapon();
	char v[8];

	if (w < 0)
		return '_';
	if (Weapon_item_info[w].record >= 0 && bm_tbl_get(Weapon_item_info[w].record,"weapon_char",v,sizeof(v)) && v[0])
		return tolower(v[0]);
	return 'g';
}

// The swing time (0x441E9 to 0x44258): t = max(fire_wait, 1.0), 2 t with
// the bit 0x08000000 (Slow).
fix dtu_combat_swing_time(void)
{
	int w = hand_weapon();
	fix t = F1_0;

	if (w >= 0 && Weapon_info[w].fire_wait > t)
		t = Weapon_info[w].fire_wait;
	if (ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_SLOW))
		t *= 2;
	if (Dtu_fix_haste_attack && ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_HASTE))
		t /= 2;
	return t;
}

// 0x43A5A: the model of the player for the item in the hand. The name is
// <race><sex><class><letter>_P.POF; then <...><letter>_M.POF,
// <...>_<letter>M.POF, <...>_<letter>P.POF, the first model of the player
// type.
static void change_player_model(int letter)
{
	char name[40];
	int m;

	snprintf(name,sizeof(name),"%s%c_p.pof",Dtu_player_archive,letter);
	m = polyobj_find_model(name);
	if (m < 0) { snprintf(name,sizeof(name),"%s%c_m.pof",Dtu_player_archive,letter); m = polyobj_find_model(name); }
	if (m < 0) { snprintf(name,sizeof(name),"%s_%cm.pof",Dtu_player_archive,letter); m = polyobj_find_model(name); }
	if (m < 0) { snprintf(name,sizeof(name),"%s_%cp.pof",Dtu_player_archive,letter); m = polyobj_find_model(name); }
	if (m < 0 && N_player_types > 0 && Player_type_info[0].n_models > 0)
		m = Player_type_info[0].model_nums[0];
	if (m < 0 || m >= N_polygon_models || !Polygon_models[m].dtu_block)
		m = -1;

	View_model = m;
	View_letter = letter;
	Swing_time = dtu_combat_swing_time();
	// 0x43A8D and 0x43BDA: the pitch offset of the arms is off for an
	// arrow or a bolt (the bow and the crossbow are held differently)
	{
		int w = hand_weapon();
		Pitch_offset = !(weapon_flag(w,"arrow") || weapon_flag(w,"bolt"));
	}
	dtu_obj_log("combat: player model %s (%d), letter %c, swing time %.3f",name,m,letter,f2fl(Swing_time));
}

// 0x4448D "Autoarming anything": the first hot key box that is not empty
// arms its item.
static void autoarm(void)
{
	int i;

	for (i=0;i<DTU_N_HOT_KEYS;i++)
		if (Dtu_char.hot_key[i].n >= 0 && Dtu_char.hot_key[i].n < Dtu_n_inventory) {
			dtu_char_select_hot_key(i);
			return;
		}
	dtu_char_set_current(-1);
}

//------------------------------------------------------------------------
// The animation of the player (0x4450C), for the arms

static polymodel *view_po(void)
{
	if (View_model < 0 || View_model >= N_polygon_models || !Polygon_models[View_model].dtu_block)
		return NULL;
	return &Polygon_models[View_model];
}

static fix state_time(int s)
{
	polymodel *po = view_po();

	if (is_attack_state(s))
		return Swing_time;
	if (po && s >= 0 && s < 19)
		return po->state_times[s];
	return 0;
}

static void view_animation(void)
{
	int s = View_state, first = Dtu_anim_first[s], end = Dtu_anim_first[s+1];
	fix st = state_time(s);
	int ended;

	// 0x44589: the frame is computed only for a state time of 1.0 or more
	// (the test is on the integer part) and a state with frames
	if (f2i(st) >= 1 && end > first) {
		fix elapsed = GameTime - View_anim_start;
		View_frame = first + (int)((((int64_t)elapsed * ((int64_t)(end - first) << 16)) / st + 0x8000) >> 16);
	}

	// 0x445B5: byte 0x1AF counts down in each game frame from 8 to 1. Its
	// end branch (the end frame at 0) cannot run, see combat.md.

	ended = View_frame >= end;
	if (!ended) {
		if (s == View_goal)
			return;
		if (!(is_attack_state(View_goal) || View_goal == DAS_DIE))
			return;
	}

	if (is_attack_state(s))
		dtu_obj_log("combat: swing ends at %.3f, %.3f after its start",f2fl(GameTime),f2fl(GameTime - Swing_started));

	// 0x44653 to 0x446DF: a thrown weapon that is used up: the empty hand
	if (is_attack_state(s) && is_thrown(hand_weapon())) {
		dtu_inv_item *e = hand_entry();
		if (e && e->count == 0)
			autoarm();
	}

	View_anim_start = GameTime;
	View_state = View_goal;
	View_frame = Dtu_anim_first[View_state];
	if (is_attack_state(View_state)) {
		Smack = 0;
		Swing_started = GameTime;
		// The executable leaves the goal at the attack state (guess: the
		// movement code sets it back). The port plays one swing.
		View_goal = DAS_STAND;
		dtu_obj_log("combat: swing starts at %.3f, swing time %.3f",f2fl(GameTime),f2fl(Swing_time));
	}
}

//------------------------------------------------------------------------
// The hit of a creature (0x278B5 and 0x29A9C for the player)

static int creature_is_undead(object *obj)
{
	return obj->id < N_robot_types && (Monster_info[obj->id].flags & MONSTER_F_UNDEAD);
}

static void player_hits_creature(object *creature,int weapon,vms_vector *hit_pnt)
{
	object *player = ConsoleObject;
	monster_info *mi;
	int roll, ac, damage = 0, alive;
	fix hp0, dealt = 0;

	if (creature->id >= N_robot_types)
		return;
	mi = &Monster_info[creature->id];

	// the "one hit per smack" guard
	if (Smack) {
		dtu_obj_log("combat: Register only one collision per smack");
		return;
	}
	Smack = 8;
	View_goal = DAS_FIDGET;

	if (creature->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED) || creature->shields < F1_0)
		return;

	// The sounds of the contact, before the roll (0x27AF5, 0x27B38)
	if (weapon >= 0)
		sound_on_object(weapon_int(weapon,"monster_hit_sound",-1),creature,F1_0/2);
	sound_on_object(mi->hit_sound,creature,F1_0/2);

	// 0x29A9C: the to-hit roll (0x2A7F3). A sleeping creature is hit and
	// wakes (0x2A839).
	ac = dtu_spell_creature_ac(creature,mi->armor_class) + dtu_spell_to_hit_bonus(player);
	// 0x29D5B: 0xB337E sets the backstab flag 0x13A955 (dtuthief.c)
	Dtu_backstab = dtu_thief_backstab_check(creature,weapon);
	if (creature->dtu.m_88 & DTU_FX_SLEEP) {
		creature->dtu.m_88 &= ~(DTU_FX_SLEEP|DTU_FX_BLIND);
		roll = 1;
		dtu_obj_log("combat: obj %d %s sleeps: hit, it wakes",(int)(creature-Objects),dtu_object_name(creature));
	}
	else
		roll = dtu_char_attack_roll(ac);

	if (roll > 0) {
		// 0x29D80: the hit stops the cast of the creature (0x9305D); 0x29D85
		// sets the condition bit 0x80 of its spells
		dtu_spell_interrupt(creature,1);
		dtu_spell_creature_event(creature,DTU_CSC_MELEE_HIT);
		// 0x29D9D: weapon_resistant
		if ((mi->flags & MONSTER_F_WEAPON_RESISTANT) && !(weapon >= 0 && weapon_flag(weapon,"magic"))) {
			HUD_init_message("Your weapon has no effect on %s.",dtu_object_name(creature));
			dtu_obj_log("combat: obj %d %s is weapon resistant",(int)(creature-Objects),dtu_object_name(creature));
		}
		else if (creature->dtu.m_88 & DTU_FX_STONESKIN) {
			// 0x29DF9: Stone Skin takes the hit
			if (--creature->dtu.m_80 <= 0)
				creature->dtu.m_88 &= ~DTU_FX_STONESKIN;
			dtu_obj_log("combat: the stone skin of obj %d takes the hit",(int)(creature-Objects));
		}
		else {
			damage = dtu_char_damage_roll();
			if (roll == 2)
				damage += dtu_char_damage_roll();		// a critical hit: a second roll
			// 0x29E3D: Chill Touch of the attacker
			if ((player->dtu.m_88 & DTU_FX_CHILL_TOUCH) && !dtu_spell_save(creature,DTU_SAVE_SPELL)) {
				if (creature_is_undead(creature))
					dtu_spell_hit(14,creature,player,dtu_spell_player_level(5));	// Fear (guess: the effect of case 14)
				else
					damage += rand() % 4 + 1;
			}
		}
	}

	Dtu_backstab = 0;
	hp0 = creature->shields;
	if (damage > 0)
		dealt = dtu_damage_creature_direct(creature,player,i2f(damage));
	alive = creature->shields >= F1_0 && !(creature->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED));

	dtu_obj_log("combat: player %s obj %d %s: result %d against AC %d, damage %d, hp %d -> %d%s",
		roll > 0 ? "hits" : "misses",(int)(creature-Objects),dtu_object_name(creature),roll,ac,damage,
		f2i(hp0),f2i(creature->shields),alive ? "" : ", dead");

	if (dealt > 0) {
		vms_vector v;

		// the push of 20.0 away from the player (0x6B70D: thrust += v / mass)
		vm_vec_sub(&v,&creature->pos,&player->pos);
		v.y = 0;
		if (vm_vec_normalize_quick(&v) > 0 && creature->mtype.phys_info.mass > 0)
			vm_vec_scale_add2(&creature->mtype.phys_info.thrust,&v,fixdiv(i2f(20),creature->mtype.phys_info.mass));
		if (weapon >= 0) {
			vclip_at(creature->segnum,hit_pnt,Weapon_info[weapon].impact_size,weapon_int(weapon,"monster_hit_vclip",-1));
			sound_at(weapon_int(weapon,alive ? "monster_hit_sound" : "monster_kill_sound",-1),creature->segnum,hit_pnt,F1_0/2);
		}
		if (alive)
			sound_on_object(mi->hit_sound,creature,F1_0/2);
	}
	else if (weapon >= 0) {
		vclip_at(creature->segnum,hit_pnt,Weapon_info[weapon].impact_size,weapon_int(weapon,"monster_miss_vclip",-1));
		sound_at(weapon_int(weapon,"monster_miss_sound",-1),creature->segnum,hit_pnt,F1_0/2);
	}
}

//------------------------------------------------------------------------
// The hit test of the swing (0x6CFF8 for the player)

// The weapon types whose sphere is 8 times larger (0x6D2A0): Dagger, Short
// Sword and their magic forms in the tables of 1.3 (by number).
static int big_sphere_weapon(int w)
{
	static const int list[] = { 0x12, 0x13, 0x22, 0x23, 0x2B, 0x2C, 0x2D, 0x45 };
	int i;

	for (i=0;i<(int)(sizeof(list)/sizeof(*list));i++)
		if (list[i] == w)
			return 1;
	return 0;
}

static void swing_hit_test(void)
{
	object *obj = ConsoleObject;
	polymodel *po = view_po();
	int s = View_state, n, rel, weapon = hand_weapon(), fate, i, best = -1;
	vms_vector test_pos, center, ofs, dir, p1, ip, best_ip;
	vms_matrix m;
	fix radius, best_d = 0x7fffffff;
	fvi_query fq;
	fvi_info hit;
	int seg, ended = 0;

	if (!is_attack_state(s)) {
		Smack = 0;
		return;
	}
	n = Dtu_anim_first[s+1] - Dtu_anim_first[s];
	rel = View_frame - Dtu_anim_first[s];
	if (rel < 6 || rel > n - rel)
		return;
	if (po == NULL || !(po->box_flags & 1) || !(po->box_flags & 2) || po->weapon_center == NULL)
		return;
	if (View_frame < 0 || View_frame >= po->n_frames)
		return;
	// A missile weapon does not hit with the swing (guess: 0x7473F gives the
	// item in the hand, the shot is in 0x56A0D)
	if (weapon >= 0 && weapon_int(weapon,"attack_type",1) != 1)
		return;

	// 0x6D017 to 0x6D066: the position moves forward by the square root of
	// 2 in the x-z plane for the test
	{
		vms_angvec a;
		fix sn, cs;

		vm_extract_angles_vector(&a,&obj->orient.fvec);
		fix_sincos(a.h - 0x2000,&sn,&cs);
		test_pos = obj->pos;
		test_pos.x += sn + cs;
		test_pos.z -= sn - cs;
	}

	// 0x7473F: the weapon sphere of the frame
	vm_copy_transpose_matrix(&m,&obj->orient);
	vm_vec_rotate(&ofs,&po->weapon_center[View_frame],&m);
	vm_vec_add(&center,&test_pos,&ofs);
	radius = po->weapon_radius[View_frame];
	if (big_sphere_weapon(weapon))
		radius *= 8;

	seg = find_point_seg(&center,obj->segnum);
	if (seg < 0)
		return;

	vm_vec_sub(&dir,&center,&test_pos);
	vm_vec_normalize_quick(&dir);
	vm_vec_scale_add(&p1,&center,&dir,radius);

	fq.p0 = &center;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = radius;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);

	// The objects (flag 1 of the query of the executable): each body
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix d;

		if (o == obj || (o->type != OBJ_ROBOT && o->type != OBJ_DRESSING))
			continue;
		if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
			continue;
		if (vm_vec_dist_quick(&o->pos,&center) > i2f(40))
			continue;
		d = dtu_object_hit_test(&ip,&center,fate == HIT_WALL ? &hit.hit_pnt : &p1,radius,o,obj);
		if (d > 0 && d < best_d) {
			best_d = d;
			best = i;
			best_ip = ip;
		}
	}

	{
		vms_vector rel_c;
		vm_vec_sub(&rel_c,&center,&obj->pos);
		dtu_obj_log("combat: test frame %d, sphere %.2f %.2f %.2f from the body, radius %.2f, wall %d side %d, object %d",
			View_frame,f2fl(rel_c.x),f2fl(rel_c.y),f2fl(rel_c.z),f2fl(radius),fate,fate == HIT_WALL ? hit.hit_side : -1,best);
	}

	if (best < 0 && !(fate == HIT_WALL && hit.hit_side != 1 && hit.hit_side != 3))
		return;

	// 0x6D3D0: a hit. The byte 0x1AF is cleared before the hit (the defect
	// that the correction removes), and the swing ends at once on the
	// frames before the middle: the start of the animation moves by
	// (2 rel - n) n / t seconds (-64 s and -32 s for t = 1.0), 0 on frame 8.
	if (!Dtu_fix_one_hit)
		Smack = 0;
	if (2*rel - n < 0)
		ended = 1;

	if (best >= 0) {
		object *other = &Objects[best];

		if (other->type == OBJ_ROBOT) {
			if (!(other->type == OBJ_ROBOT && DTU_ANIM_STATE(other) == DAS_DIE))
				player_hits_creature(other,weapon,&best_ip);
		}
		else if (other->type == OBJ_DRESSING) {
			// 0x6D4EC: scenery that can break: hit points cut to 20.0, 100.0
			// damage with no attacker (0x29A9C), then 0xA6ACB: one hit
			// breaks it
			dtu_obj_log("combat: the weapon hits scenery obj %d %s",best,dtu_object_name(other));
			if (!dtu_dressing_unbreakable(other)) {
				if (other->shields > i2f(20))
					other->shields = i2f(20);
				other->shields -= i2f(100);
				dtu_break_dressing(other,obj,0);
			}
		}
	}
	else {
		dtu_obj_log("combat: the weapon hits the wall of segment %d side %d",hit.hit_seg,hit.hit_side);
		if (hit.hit_seg >= 0)
			dtu_trig_touch(hit.hit_seg,hit.hit_side,4,obj);
	}

	if (ended)
		View_anim_start -= i2f(64);
}

//------------------------------------------------------------------------
// The shot of a missile weapon (0x56A0D, 0x561DA)

static void view_dir(vms_vector *dir)
{
	vms_angvec a;
	vms_matrix head, view;

	a.p = Dtu_look_pitch; a.b = 0; a.h = 0;
	vm_angles_2_matrix(&head,&a);
	vm_matrix_x_matrix(&view,&ConsoleObject->orient,&head);
	*dir = view.fvec;
}

// Returns 1 if the request is done.
static int fire_missile_weapon(int w)
{
	object *obj = ConsoleObject;
	polymodel *po = view_po();
	int s = View_state, fired = 0, fire_frame;
	dtu_inv_item *e = hand_entry();

	if (!is_attack_state(s) || po == NULL)
		return 0;
	fire_frame = po->fire_frames[s == DAS_ATTACK ? 0 : s == DAS_ATTACK2 ? 1 : s == DAS_ATTACK3 ? 2 : 3];
	if (View_frame < fire_frame)
		return 0;

	// The fire timer of the hand (Descent 1 style)
	if (Fire_last + 2*FrameTime < GameTime)
		Fire_next = GameTime;
	Fire_last = GameTime;
	while (Fire_next <= GameTime) {
		vms_vector dir, pos;
		int seg, objnum;

		Fire_next += Weapon_info[w].fire_wait > 0 ? Weapon_info[w].fire_wait : F1_0;
		// 0x562BB: the start is the position plus 2.0 up
		pos = obj->pos;
		pos.y += i2f(2);
		seg = find_point_seg(&pos,obj->segnum);
		if (seg < 0)
			seg = obj->segnum;
		view_dir(&dir);
		objnum = dtu_create_missile(&dir,&pos,seg,obj-Objects,w,-1,1);
		dtu_obj_log("combat: the player fires weapon %d '%s', missile obj %d",w,Weapon_item_info[w].name,objnum);
		if (objnum >= 0)
			fired++;
		if (fired >= 1)
			break;
	}
	if (!fired)
		return 0;
	// 0xB7084: one item of the stack in the hand
	if (e && e->count > 0)
		dtu_char_use_up(Dtu_char.current.n);
	View_goal = DAS_FIDGET;
	return 1;
}

//------------------------------------------------------------------------
// The start of the attack (0x44300 with the gate 0x41A4D)

static void start_attack(void)
{
	int w;

	if (!Request)
		return;

	// The spell, the scroll and the wand in the hand (0x41A4D spell part,
	// 0x56A0D spell path)
	if (dtu_spell_in_hand()) {
		if (dtu_spell_fire())
			Request = 0;
		return;
	}

	w = hand_weapon();
	if (w >= 0 && weapon_int(w,"attack_type",1) != 1) {
		// 0x41A4D step 3: an arrow needs a bow, a bolt a crossbow
		if (weapon_flag(w,"arrow") && Dtu_char.slot[DTU_SLOT_BOW].n < 0) {
			HUD_init_message("You need a bow to fire arrows.");
			Request = 0;
			return;
		}
		if (weapon_flag(w,"bolt") && Dtu_char.slot[DTU_SLOT_CROSSBOW].n < 0) {
			HUD_init_message("You need a crossbow to fire bolts.");
			Request = 0;
			return;
		}
		if (!is_attack_state(View_state)) {
			Smack = 0;
			View_goal = DAS_ATTACK;
		}
		if (fire_missile_weapon(w))
			Request = 0;
		return;
	}

	// 0x41E36: a melee weapon, or the empty hand: more than the swing time
	// since the last attack, and the key still down
	{
		fix since = GameTime - Last_attack;
		fix t = Swing_time;

		if (since < 0) since = -since;
		if (since <= t || !Key_held) {
			Request = 0;
			return;
		}
		Last_attack = GameTime;
		if (!Dtu_fix_haste_attack && (ConsoleObject->dtu.m_88 & DTU_FX_HASTE))
			Last_attack += t / 2;
	}

	// 0x44300: the goal becomes ATTACK (state 6; the states 7, 11 and 12
	// are never chosen in 1.3). An attack ends Invisibility (0x4440E).
	if (!is_attack_state(View_state)) {
		Smack = 0;
		View_goal = DAS_ATTACK;
	}
	if (ConsoleObject->dtu.m_88 & DTU_FX_INVISIBLE) {
		ConsoleObject->dtu.m_88 &= ~DTU_FX_INVISIBLE;
		ConsoleObject->dtu.m_c8[0] = 0;
	}
	// 0x44402: the bits 0, 1 and 4 of P+0xA0 (hiding, searching) end
	dtu_thief_attack();
	Request = 0;
	dtu_obj_log("combat: attack request accepted at %.3f",f2fl(GameTime));
}

//------------------------------------------------------------------------
// Calls

static void combat_init(void)
{
	if (Init_done)
		return;
	Init_done = 1;
	if (FindArg("-origcombat")) {
		Dtu_fix_one_hit = 0;
		Dtu_fix_haste_attack = 0;
		Dtu_fix_zero_dice = 0;
		Dtu_fix_stoneskin_word = 0;
		Dtu_fix_rary_manual = 0;
	}
}

void dtu_combat_attack_key(int held)
{
	combat_init();
	Key_held = held;
	// 0x2FCBE: a request on the key down only
	if (held && !Key_last)
		dtu_combat_player_attack();
	Key_last = held;
}

int dtu_combat_player_attack(void)
{
	if (!Dtu_objects_enabled || Player_is_dead || Dtu_player_dead || ConsoleObject == NULL || ConsoleObject->type != OBJ_PLAYER)
		return 0;
	// 0x2FAF0: a misc item in the hand is used, not swung
	{
		dtu_inv_item *e = hand_entry();
		if (e && e->type == OBJ_MISC && Dtu_spell_current < 0) {
			dtu_char_use_current();
			return 1;
		}
	}
	Request = 1;
	Key_held = 1;
	return 1;
}

void dtu_combat_frame(void)
{
	static int last_level = -1;

	combat_init();
	if (!Dtu_objects_enabled || ConsoleObject == NULL || ConsoleObject->type != OBJ_PLAYER)
		return;

	if (last_level != Current_level_num || View_model < 0) {
		last_level = Current_level_num;
		View_state = View_goal = DAS_STAND;
		View_frame = Dtu_anim_first[DAS_STAND];
		View_anim_start = GameTime;
		Last_attack = GameTime - i2f(100);
		Request = 0;
		change_player_model(hand_letter());
	}

	dtu_spell_frame();

	if (Player_is_dead || Dtu_player_dead) {
		Request = 0;
		return;
	}

	// 0x43A5A: a new item in the hand changes the model, not during the
	// states 6 and 11
	if (hand_letter() != View_letter && View_state != DAS_ATTACK && View_state != DAS_ATTACK3)
		change_player_model(hand_letter());
	else if (!is_attack_state(View_state))
		Swing_time = dtu_combat_swing_time();

	// The walk drives the animation of the arms: the request of the
	// controls (0x13B258) is WALK (0x2BFF2, guess for the stand: the port
	// asks for STAND when the player does not move); a state that is not an
	// attack ends before the next one starts (0x4450C)
	if (!is_attack_state(View_state) && !is_attack_state(View_goal) && (View_goal != DAS_FIDGET || View_state == DAS_FIDGET) && View_goal != DAS_DIE) {
		int g = (Dtu_move_mode != 0 && !(ConsoleObject->dtu.m_88 & DTU_OF_HELD)) ? DAS_WALK : DAS_STAND;
		if (Dtu_anim_first[g+1] > Dtu_anim_first[g] && g != View_goal) {
			View_goal = g;
			dtu_obj_log("combat: the arms ask for state %d",g);
		}
	}

	// The order of the game frame (0x435A6): the animation, the movement
	// of the objects with the hit test, then the start of the attack.
	view_animation();
	swing_hit_test();
	start_attack();
}

int dtu_combat_view_state(void) { return View_state; }
int dtu_combat_view_frame(void) { return View_frame; }
int dtu_combat_view_model(void) { return View_model; }

// render_object 0x67C5A for the player object: the arms and the weapon.
// The executable draws the player object like any object of its segment
// (it has no test for the viewer). The port draws it after the mine, which
// is the same picture for the segment of the viewer (it is drawn last in
// the painter order of render_mine).
void dtu_combat_draw_view(void)
{
	object *obj = ConsoleObject;
	vms_vector pos;
	fix light, glow[2] = { 0, 0 };
	int show_hidden = 1;
	polymodel *po = view_po();

	if (!Dtu_objects_enabled || obj == NULL || Viewer != obj || obj->type != OBJ_PLAYER || po == NULL)
		return;
	if (Player_is_dead || Dtu_player_dead || !Dtu_player_walk)
		return;

	pos = obj->pos;
	// The floor type 1 lowers the arms by 0x1388 (not in the port: the
	// floor type function is in physics.c and is not public).
	if (Pitch_offset) {
		pos.y += 0x13880 - 16 * (fix)Dtu_look_pitch;
		if (Dtu_look_pitch > 0) {
			vms_angvec a;
			fix k = 2 * (fix)Dtu_look_pitch, sn, cs;

			vm_extract_angles_vector(&a,&obj->orient.fvec);
			fix_sincos(a.h - 0x2000,&sn,&cs);
			pos.x -= fixmul(sn,k) + fixmul(cs,k);
			pos.z += fixmul(sn,k) - fixmul(cs,k);
		}
	}

	// "The missile in the hand" (0x677EB): a thrown weapon is not drawn
	// from the fire frame of the attack to its end
	if (is_attack_state(View_state) && is_thrown(hand_weapon())) {
		int f = po->fire_frames[View_state == DAS_ATTACK ? 0 : View_state == DAS_ATTACK2 ? 1 : View_state == DAS_ATTACK3 ? 2 : 3];
		if (View_frame >= f)
			show_hidden = 0;
	}

	light = compute_object_light(obj,NULL);
	draw_polygon_model_frame(&pos,&obj->orient,View_model,View_frame < po->n_frames ? View_frame : -1,show_hidden,light,glow,NULL);
}
