/*
 * ASCENT: new file. The weapon attack of the player and the arms of the
 * first person view (DTUGAME.EXE 1.3: game.c 0x44300, 0x41A4D, 0x4450C,
 * 0x43A5A; physics.c 0x6CFF8; collide.c 0x278B5, 0x29A9C; laser.c 0x56A0D,
 * 0x561DA; object.c 0x67C5A). See dtucombat.h and docs/re/combat.md.
 */

#include "dtulang.h"		// ASCENT: the language of the text
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
#include "kconfig.h"		// ASCENT: the key table
#include "dtucombat.h"
#include "dtuspell.h"
#include "dtuopts.h"
#include "dtuthief.h"
#include "text.h"
#include "key.h"

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
static int Lift_model = -1;			// ASCENT: the model of Swing_lift (Dtu_improved_body)
static fix Swing_lift = 0;				// ASCENT: the lift of the arms in a swing (Dtu_improved_body)

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
// empty hand, 'G' a weapon with no letter. A spell in the hand: byte 5 of
// its spell list entry ('K' for the hands).
static int hand_letter(void)
{
	int w = hand_weapon();
	char v[8];

	if (dtu_spell_hand_letter())
		return tolower(dtu_spell_hand_letter());

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
	Lift_model = -1;					// ASCENT: the swing lift of the improved body
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
// arms its item or its spell.
static void autoarm(void)
{
	int i;

	for (i=0;i<DTU_N_HOT_KEYS;i++)
		if (dtu_char_hot_key_item(i) >= 0 || dtu_char_hot_key_spell(i) >= 0) {
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
	ac = dtu_spell_creature_ac(creature,mi->armor_class);
	// 0x29D5B: 0xB337E sets the backstab flag 0x13A955 (dtuthief.c)
	Dtu_backstab = dtu_thief_backstab_check(creature,weapon);
	if (creature->dtu.m_88 & DTU_FX_SLEEP) {
		creature->dtu.m_88 &= ~(DTU_FX_SLEEP|DTU_FX_BLIND);
		roll = 1;
		dtu_obj_log("combat: obj %d %s sleeps: hit, it wakes",(int)(creature-Objects),dtu_object_name(creature));
	}
	else
		roll = dtu_char_attack_roll(creature,weapon);

	if (roll > 0) {
		// 0x29D80: the hit stops the cast of the creature (0x9305D); 0x29D85
		// sets the condition bit 0x80 of its spells
		dtu_spell_interrupt(creature,1);
		dtu_spell_creature_event(creature,DTU_CSC_MELEE_HIT);
		// 0x29D9D: weapon_resistant
		if ((mi->flags & MONSTER_F_WEAPON_RESISTANT) && !(weapon >= 0 && weapon_flag(weapon,"magic"))) {
			HUD_init_message("Your weapon has no effect on %s.",dtu_object_display_name(creature));
			dtu_obj_log("combat: obj %d %s is weapon resistant",(int)(creature-Objects),dtu_object_name(creature));
		}
		else {
			// Stone Skin takes the hit in the roll (0x2AE44)
			damage = dtu_char_weapon_damage(weapon,creature);
			if (roll == 2)
				damage += dtu_char_weapon_damage(weapon,creature);		// a critical hit: a second roll
			// 0x29E3D: Chill Touch of the attacker (the swing is a melee hit), a
			// failed save (category 4): an undead creature
			// gets Fear (bit 8, the timer +0xAC) for 1d4 plus the caster
			// level (0x91EC1 of spell 5) minutes and the morale function
			// 0x10ADF runs; another creature takes 1d4 more
			if ((player->dtu.m_88 & DTU_FX_CHILL_TOUCH) &&
				 !dtu_spell_save(creature,DTU_SAVE_SPELL)) {
				if (creature_is_undead(creature)) {
					dtu_spell_set_effect(creature,DTU_FX_FEAR,i2f(60 * (rand() % 4 + 1 + dtu_spell_player_level(5))),0,0);
					dtu_creature_morale(creature);
				}
				else
					damage += rand() % 4 + 1;
			}
		}
	}

	Dtu_backstab = 0;
	hp0 = creature->shields;
	if (damage > 0) {
		dtu_spell_armor_hit(creature,i2f(damage));		// 0x29F3E: the Armor pool
		dealt = dtu_damage_creature_direct(creature,player,i2f(damage));
	}
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
			// ASCENT: the original plays monster_hit_sound only if the
			// creature lives, and monster_kill_sound if it dies. No weapon
			// of the shipped tables sets monster_kill_sound, so the killing
			// blow had no impact sound at the hit point (docs/re/bugs.md,
			// "Impact sound of the killing blow"). The port plays the hit
			// sound on each hit, and the kill sound as well if a table sets it.
			sound_at(weapon_int(weapon,"monster_hit_sound",-1),creature->segnum,hit_pnt,F1_0/2);
			if (!alive)
				sound_at(weapon_int(weapon,"monster_kill_sound",-1),creature->segnum,hit_pnt,F1_0/2);
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

// The result of the sphere test of one frame of the swing.
typedef struct {
	int		object;		// the nearest object in the sphere, or -1
	vms_vector ip;		// the hit point on that object
	int		fate;			// of the wall query
	fvi_info	hit;
	vms_vector center;	// the sphere
	fix		radius;
} swing_contact;

// 1 if the swing stops at the contact: an object, or a wall whose side is
// not 1 or 3 ((guess) not the ceiling, not the floor).
static int swing_stops(swing_contact *sc)
{
	return sc->object >= 0 || (sc->fate == HIT_WALL && sc->hit.hit_side != 1 && sc->hit.hit_side != 3);
}

// The weapon sphere test of 0x6CFF8 for one frame of the arms model, with no
// effect on the game. The hit test of the swing and the target hint use it.
// Returns 0 if the frame has no test (no weapon sphere).
static int swing_test(int frame,int weapon,swing_contact *sc)
{
	object *obj = ConsoleObject;
	polymodel *po = view_po();
	int fate, i, seg;
	vms_vector test_pos, center, ofs, dir, p1, ip;
	vms_matrix m;
	fix radius, best_d = 0x7fffffff;
	fvi_query fq;

	if (po == NULL || !(po->box_flags & 1) || !(po->box_flags & 2) || po->weapon_center == NULL)
		return 0;
	if (frame < 0 || frame >= po->n_frames)
		return 0;

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
	vm_vec_rotate(&ofs,&po->weapon_center[frame],&m);
	vm_vec_add(&center,&test_pos,&ofs);
	radius = po->weapon_radius[frame];
	if (big_sphere_weapon(weapon))
		radius *= 8;

	seg = find_point_seg(&center,obj->segnum);
	if (seg < 0)
		return 0;

	vm_vec_sub(&dir,&center,&test_pos);
	vm_vec_normalize_quick(&dir);
	vm_vec_scale_add(&p1,&center,&dir,radius);

	// 0x6D312 to 0x6D353: the query has the horizontal radius 0 (+0x14)
	// and the vertical radius r (+0x18). fvi_sub (0x3F58C) then tests the
	// walls with 1.0, the floor with 0 and the ceiling with 1.5 r.
	fq.p0 = &center;
	fq.p1 = &p1;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	Fvi_dtu_body = 4;
	Fvi_dtu_rad2 = radius;
	fate = find_vector_intersection(&fq,&sc->hit);
	Fvi_dtu_body = 0;
	Fvi_dtu_rad2 = 0;
	sc->fate = fate;
	sc->object = -1;
	sc->center = center;
	sc->radius = radius;

	// The objects (flag 1 of the query of the executable): each body, with
	// the object test of the query (0x3EE70, dtu_body_query_test in fvi.c).
	// The nearest contact wins, as in fvi_sub.
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix d;

		if (o == obj || (o->type != OBJ_ROBOT && o->type != OBJ_DRESSING))
			continue;
		if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
			continue;
		if (vm_vec_dist_quick(&o->pos,&center) > i2f(40))
			continue;
		d = dtu_body_query_test(&ip,&center,&p1,0,radius,o);
		if (d > 0 && d < best_d) {
			best_d = d;
			sc->object = i;
			sc->ip = ip;
		}
	}
	// fvi_sub 0x3F8FF: a wall nearer than the object wins, but not the floor
	// (side 3) against an object. The distance of an object is the return
	// of the object test (the radius of the body for a start point in it).
	if (sc->object >= 0 && fate == HIT_WALL && sc->hit.hit_side != 3 && vm_vec_dist(&sc->hit.hit_pnt,&center) < best_d)
		sc->object = -1;
	return 1;
}

static void swing_hit_test(void)
{
	object *obj = ConsoleObject;
	int s = View_state, n, rel, weapon = hand_weapon(), best;
	swing_contact sc;
	fvi_info *hit = &sc.hit;
	int ended = 0;

	if (!is_attack_state(s)) {
		Smack = 0;
		return;
	}
	n = Dtu_anim_first[s+1] - Dtu_anim_first[s];
	rel = View_frame - Dtu_anim_first[s];
	if (rel < 6 || rel > n - rel)
		return;
	// A missile weapon does not hit with the swing (guess: 0x7473F gives the
	// item in the hand, the shot is in 0x56A0D)
	if (weapon >= 0 && weapon_int(weapon,"attack_type",1) != 1)
		return;
	if (!swing_test(View_frame,weapon,&sc))
		return;
	best = sc.object;

	{
		vms_vector rel_c;
		vm_vec_sub(&rel_c,&sc.center,&obj->pos);
		dtu_obj_log("combat: test frame %d, sphere %.2f %.2f %.2f from the body, radius %.2f, wall %d side %d, object %d",
			View_frame,f2fl(rel_c.x),f2fl(rel_c.y),f2fl(rel_c.z),f2fl(sc.radius),sc.fate,sc.fate == HIT_WALL ? hit->hit_side : -1,best);
		if (best >= 0) {
			vms_vector r = Objects[best].pos;
			vm_vec_sub2(&r,&obj->pos);
			dtu_obj_log("combat: contact obj %d at %.2f across, %.2f up from the body",best,f2fl(fix_sqrt(fixmul(r.x,r.x) + fixmul(r.z,r.z))),f2fl(r.y));
		}
	}

	if (!swing_stops(&sc))
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
				player_hits_creature(other,weapon,&sc.ip);
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
		dtu_obj_log("combat: the weapon hits the wall of segment %d side %d",hit->hit_seg,hit->hit_side);
		if (hit->hit_seg >= 0)
			dtu_trig_touch(hit->hit_seg,hit->hit_side,4,obj);
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

// The start of the shot (0x562BB): the position plus 2.0 up, along the view
// with the look pitch. Returns the segment.
static int missile_start(vms_vector *pos,vms_vector *dir)
{
	object *obj = ConsoleObject;
	int seg;

	*pos = obj->pos;
	pos->y += i2f(2);
	seg = find_point_seg(pos,obj->segnum);
	if (seg < 0)
		seg = obj->segnum;
	view_dir(dir);
	return seg;
}

// 0x41A4D step 3: an arrow needs a bow, a bolt a crossbow. Returns the
// message, or NULL if the weapon can be fired.
static char *missile_needs(int w)
{
	if (weapon_flag(w,"arrow") && Dtu_char.slot[DTU_SLOT_BOW].n < 0)
		return LT("You need a bow to fire arrows.");
	if (weapon_flag(w,"bolt") && Dtu_char.slot[DTU_SLOT_CROSSBOW].n < 0)
		return LT("You need a crossbow to fire bolts.");
	return NULL;
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
		seg = missile_start(&pos,&dir);
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
		char *need = missile_needs(w);

		if (need) {
			HUD_init_message("%s",need);
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
// Turn undead (key T, 0xADC46; the Spider Piece 0x12 through dtuchar.c)

int Dtu_fix_turn_undead = 1;

// 0x145602: the names of the undead classes, compared with the start of the
// robot name (strnicmp 0xDDE29) by the level init of the NPC list (0xAC4B4).
static const char *Turn_names[9] = { "ske_", "zombie", "ghoul", "banegard", "ghast", "mummy", "gskel", "lich", "sfiend" };

// 0xA6487: the number that the d20 must reach, by class and cleric level 1
// to 14. -1: turned, -2 and -3: destroyed, -4: no effect (the creature can
// be tried again).
static const signed char Turn_table[9][14] = {
	{ 10,  7,  4, -1, -1, -2, -2, -3, -3, -3, -3, -3, -3, -3 },
	{ 13, 10,  7,  4, -1, -1, -2, -2, -3, -3, -3, -3, -3, -3 },
	{ 16, 13, 10,  7,  4, -1, -1, -2, -2, -3, -3, -3, -3, -3 },
	{ 20, 19, 16, 13, 10,  7,  4, -1, -1, -2, -2, -2, -2, -3 },
	{ -4, 20, 19, 16, 13, 10,  7,  4, -1, -1, -1, -2, -2, -2 },
	{ -4, -4, -4, 20, 19, 16, 13, 10,  7,  4,  4, -1, -1, -1 },
	{ -4, -4, -4, -4, -4, 20, 19, 16, 13, 10, 10,  7,  7,  4 },
	{ -4, -4, -4, -4, -4, -4, -4, 20, 19, 16, 16, 13, 13, 10 },
	{ -4, -4, -4, -4, -4, -4, -4, -4, 20, 19, 19, 16, 16, 13 },
};

// The NPC list entry byte +3 of the executable: the class (low nibble) and
// bit 0x10, "can be tried". The executable fills it at the level start for
// the objects of the NPC list and never takes an entry out, so after a
// death the object number can belong to a new object. The port keeps the
// state by object number and signature, so a new object starts fresh.
static struct { int signature; ubyte bits; } Turn_state[MAX_OBJECTS];
static int Turn_level = -1;
static fix Turn_last = 0;			// 0x145626

// strnicmp(prefix, name, strlen(prefix)) == 0
static int turn_prefix(const char *prefix,const char *name)
{
	for (; *prefix; prefix++, name++)
		if (tolower((unsigned char)*prefix) != tolower((unsigned char)*name))
			return 0;
	return 1;
}

static int turn_bits(object *obj)
{
	int n = obj-Objects, i;

	if (Turn_level != Current_level_num) {
		Turn_level = Current_level_num;
		for (i=0; i<MAX_OBJECTS; i++)
			Turn_state[i].signature = -1;
	}
	if (Turn_state[n].signature != obj->signature) {
		Turn_state[n].signature = obj->signature;
		// 0xAC4C1: without a name match the class is from the levels of
		// the player and bit 0x10 stays clear ("couldn't find undead")
		Turn_state[n].bits = 8;
		for (i=0; i<9; i++)
			if (turn_prefix(Turn_names[i],Robot_names[obj->id])) {
				Turn_state[n].bits = i | 0x10;
				break;
			}
	}
	return Turn_state[n].bits;
}

// 0xA66C9: the creature is alive
static int turn_alive(object *obj)
{
	return obj->type == OBJ_ROBOT && obj->shields >= F1_0 && !(obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) &&
		obj->ctype.ai_info.CURRENT_STATE != DAIS_DEAD && obj->ctype.ai_info.GOAL_STATE != DAIS_DEAD && DTU_ANIM_STATE(obj) != DAS_DIE;
}

// 0xADE2A to 0xADEED: the brain bit (always 1 in the port, Dtu_fix_brain)
// and a ray from the creature to the player that hits nothing
static int turn_sees_player(object *obj)
{
	fvi_query fq;
	fvi_info hit;

	if (obj->segnum < 0 || obj->segnum > Highest_segment_index)
		return 0;
	fq.p0 = &obj->pos;
	fq.startseg = obj->segnum;
	fq.p1 = &ConsoleObject->pos;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	return find_vector_intersection(&fq,&hit) == HIT_NONE;
}

static void turn_message(int tex,char *def,object *obj)
{
	char *f = (tex >= 0 && tex < N_TEXT_STRINGS && Text_string[tex] && Text_string[tex][0]) ? Text_string[tex] : def;

	HUD_init_message(f,dtu_object_display_name(obj));
}

// level: the cleric level from 1, or 0 for the class and the cloak of the
// character (the key T). Returns the number of creatures that it changed.
int dtu_combat_turn_undead(int level)
{
	object *pl = ConsoleObject;
	int order[MAX_OBJECTS], n = 0, i, j, roll, destroys, done = 0;
	fix d;

	if (!Dtu_objects_enabled || pl == NULL || pl->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead)
		return 0;

	// 0xADC54: half a second between two uses
	d = GameTime - Turn_last;
	if (d < 0) d = -d;
	if (d < F1_0/2)
		return 0;
	Turn_last = GameTime;

	if (level == 0) {
		dtu_inv_item *cloak = NULL;
		int allowed = Dtu_char.pa.class1 == DTU_CLASS_CLERIC || Dtu_char.pa.class2 == DTU_CLASS_CLERIC;	// 0x35A09

		// 0xADC91: the cleric level is the level of class 2 if class 2 is
		// the cleric, else the level of class 1
		level = (Dtu_char.pa.class2 == DTU_CLASS_CLERIC ? Dtu_char.pa.class2_level : Dtu_char.pa.class1_level) + 1;
		// 0xADCB3: armor 45 in the cloak place turns as a cleric of level 7
		if (Dtu_char.slot[DTU_SLOT_CLOAK].list == 0 && Dtu_char.slot[DTU_SLOT_CLOAK].n >= 0 && Dtu_char.slot[DTU_SLOT_CLOAK].n < Dtu_n_inventory)
			cloak = &Dtu_inventory[(int)Dtu_char.slot[DTU_SLOT_CLOAK].n];
		if (cloak && cloak->id == 45) {
			allowed = 1;
			level = 7;
		}
		if (!allowed) {
			turn_message(931,"You cannot perform this action.",pl);
			return 0;
		}
	}

	roll = rand() % 20 + 1;		// 0xADCFF
	destroys = rand() % 11 + 2;	// 0xADD18: at most this many are destroyed
	if (N_player_types > 0)
		sound_at(tbl_int(Player_type_info[0].record,"banish_sound",-1),pl->segnum,&pl->pos,F1_0/6);	// 0x2AAA
	if (level > 14)
		level = 14;					// 0xADF0F
	if (level < 1)
		level = 1;

	// The NPC list in the order of the object numbers, sorted by class
	// (0xADB7D, the same exchange sort)
	for (i=0; i<=Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT && Objects[i].id < N_robot_types)
			order[n++] = i;
	for (i=0; i<n-1; i++)
		for (j=i+1; j<n; j++)
			if ((turn_bits(&Objects[order[j]]) & 0x0F) < (turn_bits(&Objects[order[i]]) & 0x0F)) {
				int t = order[i]; order[i] = order[j]; order[j] = t;
			}

	dtu_obj_log("turn undead: level %d, roll %d, at most %d destroyed",level,roll,destroys);

	for (i=0; i<n; i++) {
		object *obj = &Objects[order[i]];
		int bits, cls, t;

		if (obj->type != OBJ_ROBOT || !(Monster_info[obj->id].flags & MONSTER_F_UNDEAD) || !turn_alive(obj) || !turn_sees_player(obj))
			continue;
		bits = turn_bits(obj);
		cls = bits & 0x0F;
		if (!(bits & 0x10)) {
			HUD_init_message("You can no longer attempt to turn the %s.",dtu_object_display_name(obj));
			continue;
		}
		// 0xADF57: the destroys are used up. The executable clears bit 0x10
		// of this creature first (0xADF09), so it can never be tried
		// again although nothing was tried. Dtu_fix_turn_undead keeps it.
		if (destroys <= 0) {
			if (!Dtu_fix_turn_undead)
				Turn_state[obj-Objects].bits &= ~0x10;
			break;
		}
		Turn_state[obj-Objects].bits &= ~0x10;
		t = Turn_table[cls < 9 ? cls : 8][level-1];

		if (t == -4) {
			// 0xAE0DC: "unable to affect", the creature can be tried again
			dtu_creature_turn(obj,0);
			turn_message(927,"You are currently unable to affect the %s.",obj);
			Turn_state[obj-Objects].bits |= 0x10;
		}
		else if (t == -3 || t == -2) {
			// 0xAE14C, 0xAE158: destroyed. 0xA6ACB(obj, no attacker, its
			// hit points) through the damage function with no attacker:
			// the executable reads the attacker fields at address 0
			// (0x29E3D, 0x2A054) there, the port gives no experience.
			char name[64];

			strncpy(name,dtu_object_name(obj),sizeof(name)-1);
			name[sizeof(name)-1] = 0;
			dtu_creature_wake(obj,0);
			dtu_damage_creature_direct(obj,NULL,obj->shields + F1_0);
			HUD_init_message(Text_string[928] && Text_string[928][0] ? Text_string[928] : "You destroyed the %s.",name);
			destroys--;
			done++;
		}
		else if (t == -1 || roll >= t) {
			// 0xAE1AC, 0xAE1DD: turned
			dtu_creature_turn(obj,1);
			turn_message(929,"You successfully turned the %s.",obj);
			done++;
		}
		else {
			// 0xAE22B: failed
			dtu_creature_turn(obj,0);
			turn_message(930,"You failed to turn the %s.",obj);
		}
	}
	return done;
}

//------------------------------------------------------------------------
// ASCENT: the target hint (Dtu_target_hint, dtuopts.h). Not in the
// executable. In each frame the attack of the item in the hand is tested as
// the game would do it now, with no effect: the swing of a melee weapon or
// of the empty hand (the sphere test of the hit frames, swing_test), the
// flight of a missile (dtu_missile_path_target) or of the missile of a
// spell (dtu_spell_hint_target). The hint is on if the first object that
// the attack reaches is a creature that it can hurt: hostile, alive, not in
// the death animation, and not weapon_resistant against a weapon that is not
// magic. A wall, scenery, a corpse or a creature that is not hostile in
// front of it turns the hint off. Movement after the test is not predicted.

static int Target_hint = 0;

// The rule of player_hits_creature and of the missile hit (dtuobj.c).
// weapon: the weapon type, -1 for the empty hand, -2 for a spell.
static int hint_creature(int objnum,int weapon)
{
	object *o;

	if (objnum < 0 || objnum > Highest_object_index)
		return 0;
	o = &Objects[objnum];
	if (!dtu_creature_is_hostile(o) || o->id >= N_robot_types)
		return 0;
	if ((o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) || o->shields < F1_0 || DTU_ANIM_STATE(o) == DAS_DIE)
		return 0;
	if (weapon != -2 && (Monster_info[o->id].flags & MONSTER_F_WEAPON_RESISTANT) && !(weapon >= 0 && weapon_flag(weapon,"magic")))
		return 0;
	return 1;
}

// The swing: the frames of the hit window of the attack state (rel 6 to
// n/2, swing_hit_test) in their order. The first frame with a contact
// decides, since the swing ends there.
static int melee_hint(int weapon)
{
	int s = DAS_ATTACK, n = Dtu_anim_first[s+1] - Dtu_anim_first[s], rel;
	swing_contact sc;

	for (rel = 6; rel <= n - rel; rel++) {
		if (!swing_test(Dtu_anim_first[s] + rel,weapon,&sc))
			continue;
		if (sc.object >= 0)
			return hint_creature(sc.object,weapon);
		if (swing_stops(&sc))
			return 0;
	}
	return 0;
}

static int target_hint(void)
{
	int w;

	if (dtu_spell_in_hand())
		return hint_creature(dtu_spell_hint_target(),-2);
	w = hand_weapon();
	if (w >= 0 && weapon_int(w,"attack_type",1) != 1) {
		vms_vector pos, dir;
		int seg;

		if (missile_needs(w))
			return 0;
		seg = missile_start(&pos,&dir);
		return hint_creature(dtu_missile_path_target(&pos,seg,&dir,w,-1,ConsoleObject-Objects,dtu_missile_weapon_range(w)),w);
	}
	return melee_hint(w);
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
		Dtu_fix_stoneskin_word = 0;
		Dtu_fix_chill_touch = 0;
		Dtu_fix_rary_manual = 0;
		Dtu_fix_melf_acid = 0;
		Dtu_fix_turn_undead = 0;
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
	Target_hint = 0;
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

	// Turn undead: items 52 and 53 of the key table (default T, kconfig.c).
	// The key code of controls.c does not read it, so it is read here.
	{
		static int t_last = 0;
		int t = dtu_action_down(DTU_ACT_TURN_UNDEAD);

		if (t && !t_last)
			dtu_combat_turn_undead(0);
		t_last = t;
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
	Target_hint = Dtu_target_hint ? target_hint() : 0;
	{
		static int last = -1;

		if (Target_hint != last)
			dtu_obj_log("combat: target hint %d",Target_hint);
		last = Target_hint;
	}
}

int dtu_combat_target_hint(void)
{
	return Dtu_target_hint && Target_hint && ConsoleObject && !Player_is_dead && !Dtu_player_dead;
}

int dtu_combat_view_state(void) { return View_state; }
int dtu_combat_attack_pending(void) { return Request; }

// 0x9305D, the player branch: the request of the attack key (0x13B69F and
// 0x13B6A3) is cleared and the arms go at once to the flinch (+0x176 and
// +0x170 = 8, P+0x57 = GameTime + FrameTime)
void dtu_combat_cast_stopped(void)
{
	Request = 0;
	View_state = View_goal = DAS_FLINCH;
	View_frame = Dtu_anim_first[DAS_FLINCH];
	View_anim_start = GameTime + FrameTime;
}
// 0x2A949: a fumble of the player that does not drop the weapon: the
// request of the attack key ends and the arms go at once to state 10
// (+0x176 and +0x170, P+0x57 = GameTime)
void dtu_combat_fumble(void)
{
	Request = 0;
	View_state = View_goal = DAS_FIDGET;
	View_frame = Dtu_anim_first[DAS_FIDGET];
	View_anim_start = GameTime;
}
int dtu_combat_view_frame(void) { return View_frame; }
int dtu_combat_view_model(void) { return View_model; }

//------------------------------------------------------------------------
// ASCENT: the Improvements setting Dtu_improved_body (dtuopts.h). Not in
// the executable.
//
// The arms model of the player (<archive><letter>_p.pof) is the upper body
// only: submodel 0 is the root, 1 the chest and pelvis (flag 0x02), 2 to 5
// the right arm (shoulder, upper arm, forearm, hand with the weapon), 7 to
// 10 the left arm, and a submodel with flag 0x04 and no polygons marks the
// end of the weapon (6 in hmch_p.pof). There are no legs. So the whole
// model turns with the view.
//
// With the setting on, the model is placed as the executable places it at
// look pitch 0 (1.2207 above the center of the body, less for a bow) and
// then turned with the view pitch about the eye (the center of the body
// plus eye_offset along uvec, without the view bob, which the arms do not
// have in the executable either). The orientation is the view matrix of
// render_frame (orient * angles_2_matrix(pitch,0,0)). So the arms, the
// chest and the weapon stay at the same place on the screen at every
// pitch, and the distance of each point of the arms from the eye is the
// distance at pitch 0: the arms do not reach further into a near wall.
//
// The swing lift. The stab of the short sword and of the dagger stays
// under the view in all frames of the attack (docs/re/combat.md, C1). For
// a model whose weapon is never in the view during the attack state, the
// arms are moved up along the view uvec during the swing by the height
// that puts the weapon of the frames of the hit window (6 to 8) at 0.6 of
// the way from the center of the view to its lower edge. The lift rises
// over the first 3/16 of the swing, holds, and falls over the last 4/16.
// The hit test, the swing time and the missile start are not changed.

#define BODY_LIFT_TARGET	(F1_0*6/10)		// the weapon at 0.6 of the half height under the center
#define BODY_LIFT_MAX		(F1_0*4)


// v = m * a with the columns rvec, uvec, fvec (the order of tools/model.py:
// a.x * rvec + a.y * uvec + a.z * fvec)
static void body_unrotate(vms_vector *v,vms_vector *a,vms_matrix *m)
{
	v->x = fixmul(a->x,m->rvec.x) + fixmul(a->y,m->uvec.x) + fixmul(a->z,m->fvec.x);
	v->y = fixmul(a->x,m->rvec.y) + fixmul(a->y,m->uvec.y) + fixmul(a->z,m->fvec.y);
	v->z = fixmul(a->x,m->rvec.z) + fixmul(a->y,m->uvec.z) + fixmul(a->z,m->fvec.z);
}

// The origin of submodel s in the space of the model, in the given frame
// (the joint chain of models.md: origin = sum of parent matrix * offset).
static void body_joint(polymodel *po,int frame,int s,vms_vector *out)
{
	int chain[MAX_POLYMODEL_SUBMODELS], n = 0, i;
	vms_matrix m = vmd_identity_matrix;

	vm_vec_zero(out);
	while (s >= 0 && s < po->n_models && n < MAX_POLYMODEL_SUBMODELS) {
		chain[n++] = s;
		if (po->submodel_parents[s] == 0xff || po->submodel_parents[s] == s)
			break;
		s = po->submodel_parents[s];
	}
	for (i=n-1;i>=0;i--) {
		int c = chain[i];
		vms_vector t;
		vms_matrix lm, nm;

		body_unrotate(&t,&po->submodel_offsets[c],&m);
		vm_vec_add2(out,&t);
		vm_angles_2_matrix(&lm,&po->anim_angles[frame * po->n_models + c]);
		body_unrotate(&nm.rvec,&lm.rvec,&m);
		body_unrotate(&nm.uvec,&lm.uvec,&m);
		body_unrotate(&nm.fvec,&lm.fvec,&m);
		m = nm;
	}
}

static fix body_eye_offset(void)
{
	if (N_player_types > 0 && Player_type_info[0].eye_offset)
		return Player_type_info[0].eye_offset;
	return DTU_EYE_OFFSET_DEFAULT;
}

// The middle of the weapon (between the hand joint and the end marker) of
// a frame, relative to the eye, in the space of the player at pitch 0.
// Returns 0 if the model has no end marker.
static int body_weapon_point(polymodel *po,int frame,vms_vector *p)
{
	vms_vector tip, hand, bc;
	int s, tip_sub = -1;

	if (!po->submodel_flags || !po->anim_angles || frame < 0 || frame >= po->n_frames)
		return 0;
	for (s=0;s<po->n_models;s++)
		if (po->submodel_flags[s] & 0x04) { tip_sub = s; break; }
	if (tip_sub < 1 || po->submodel_parents[tip_sub] >= po->n_models)
		return 0;
	body_joint(po,frame,tip_sub,&tip);
	body_joint(po,frame,po->submodel_parents[tip_sub],&hand);
	polyobj_get_body_center(View_model,frame,&bc);
	p->x = (tip.x + hand.x) / 2 - bc.x;
	p->y = (tip.y + hand.y) / 2 - bc.y + (Pitch_offset ? 0x13880 : 0) - body_eye_offset();
	p->z = (tip.z + hand.z) / 2 - bc.z;
	return 1;
}

// The lift of the arms for the swing of the model in the hand (see above).
static void body_compute_lift(polymodel *po)
{
	int first = Dtu_anim_first[DAS_ATTACK], end = Dtu_anim_first[DAS_ATTACK+1], f;
	fix best = -1;
	vms_vector p;

	Lift_model = View_model;
	Swing_lift = 0;
	if (end <= first || end > po->n_frames || !Pitch_offset)
		return;
	// No lift if the weapon is in the view in some frame of the attack.
	for (f=first;f<end;f++) {
		if (!body_weapon_point(po,f,&p))
			return;
		if (p.z > F1_0/2 && abs(p.y) <= fixmul(Render_zoom,p.z) && abs(p.x) <= p.z)
			return;
	}
	// The hit window: frames 6 to 8 of the state (0x6CFF8)
	for (f=first+6;f<=first+8 && f<end;f++) {
		fix need;

		body_weapon_point(po,f,&p);
		if (p.z <= F1_0/2)
			continue;
		need = -fixmul(fixmul(BODY_LIFT_TARGET,Render_zoom),p.z) - p.y;
		if (best < 0 || need < best)
			best = need;
	}
	if (best > 0)
		Swing_lift = best < BODY_LIFT_MAX ? best : BODY_LIFT_MAX;
	dtu_obj_log("combat: body lift of model %d for the swing %.3f",View_model,f2fl(Swing_lift));
}

// The lift at this moment of the swing
static fix body_swing_lift(polymodel *po)
{
	fix t, r, k;

	if (Lift_model != View_model)
		body_compute_lift(po);
	if (Swing_lift == 0 || !is_attack_state(View_state) || Swing_time <= 0)
		return 0;
	t = GameTime - View_anim_start;
	if (t <= 0)
		return 0;
	r = fixdiv(t,Swing_time) * 16;		// the frame of the state, with the fraction
	if (r >= 16*F1_0)
		return 0;
	if (r < 3*F1_0)
		k = r / 3;
	else if (r > 12*F1_0)
		k = (16*F1_0 - r) / 4;
	else
		k = F1_0;
	k = fixmul(fixmul(k,k),3*F1_0 - 2*k);	// smooth start and end
	return fixmul(Swing_lift,k);
}

// The position and the orientation of the arms model with the setting on
static void improved_body_place(object *obj,polymodel *po,vms_vector *pos,vms_matrix *orient)
{
	vms_matrix headm;
	vms_angvec a;
	vms_vector eye, l, w;
	fix eye_offset = body_eye_offset();

	a.p = Dtu_look_pitch; a.b = 0; a.h = 0;
	vm_angles_2_matrix(&headm,&a);
	vm_matrix_x_matrix(orient,&obj->orient,&headm);	// the view matrix of render_frame

	eye = obj->pos;
	vm_vec_scale_add2(&eye,&obj->orient.uvec,eye_offset);

	// the place at pitch 0 relative to the eye, in the space of the player
	l.x = 0;
	l.y = (Pitch_offset ? 0x13880 : 0) - eye_offset + body_swing_lift(po);
	l.z = 0;
	body_unrotate(&w,&l,orient);
	vm_vec_add(pos,&eye,&w);
}

// render_object 0x67C5A for the player object: the arms and the weapon.
// The executable draws the player object like any object of its segment
// (it has no test for the viewer). The port draws it after the mine, which
// is the same picture for the segment of the viewer (it is drawn last in
// the painter order of render_mine).
void dtu_combat_draw_view(void)
{
	object *obj = ConsoleObject;
	vms_vector pos;
	vms_matrix orient;
	fix light, glow[2] = { 0, 0 };
	int show_hidden = 1;
	polymodel *po = view_po();

	if (!Dtu_objects_enabled || obj == NULL || Viewer != obj || obj->type != OBJ_PLAYER || po == NULL)
		return;
	if (Player_is_dead || Dtu_player_dead || !Dtu_player_walk)
		return;

	pos = obj->pos;
	orient = obj->orient;
	// The floor type 1 lowers the arms by 0x1388 (not in the port: the
	// floor type function is in physics.c and is not public).
	if (Dtu_improved_body)
		improved_body_place(obj,po,&pos,&orient);
	else if (Pitch_offset) {
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

	// ASCENT: animation interpolation (Dtu_anim_interp, polyobj.h). The
	// frame of the swing rules (View_frame) stays. view_animation() counts
	// the frames by the time for a state time of 1.0 or more, so the time
	// of one frame is known then.
	if (Dtu_anim_interp && View_frame >= 0 && View_frame < po->n_frames && View_state >= 0 && View_state < DTU_N_ANIM_STATES) {
		static polyobj_anim_track track;
		int first = Dtu_anim_first[View_state], end = Dtu_anim_first[View_state+1], next;
		fix st = state_time(View_state), est = 0, frac;

		if (f2i(st) >= 1 && end > first)
			est = st / (end - first);
		frac = polyobj_anim_track_update(&track,View_model+1,View_state,View_frame,first,end,est,1,GameTime,&next);
		draw_polygon_model_frame_lerp(&pos,&orient,View_model,View_frame,next,frac,show_hidden,light,glow,NULL);
		return;
	}

	draw_polygon_model_frame(&pos,&orient,View_model,View_frame < po->n_frames ? View_frame : -1,show_hidden,light,glow,NULL);
}

//------------------------------------------------------------------------
// The fire trap of the Yawning Portal and the blindness and deafness of
// Fireball (spells.c 0x90CEF, scenery break 0xA6ACB)

// 0x90DDF to 0x91058, in 0x90CEF after the spell hit 0x8CFFF: only for
// spell 16 (Fireball) and a player or creature within 6.0 of the point.
// Every caller of 0x90CEF tests the area flag of the spell, which Fireball
// does not have, except the fire trap (0xA6F51, 0xA6F61).
void dtu_fireball_blind_deaf(object *o, fix dist)
{
	int player = (o->type == OBJ_PLAYER);
	int sound = -1;

	if ((o->type != OBJ_PLAYER && o->type != OBJ_ROBOT) || dist > i2f(6))
		return;
	if (!player && o->id < N_robot_types)
		sound = Monster_info[o->id].hit_sound;		// Robot_info+0x9A
	else if (player && N_player_types > 0) {
		char value[32];

		// P+0x1B0B, the hard_sound of the player type (combat.md)
		if (bm_tbl_get(Player_type_info[0].record, "hard_sound", value, sizeof(value)))
			sound = atoi(value);
	}
	if (!(o->dtu.m_88 & DTU_FX_BLIND)) {
		// 0xAE596(obj, 4): a save skips the message, the flash and the
		// sound, but not the blindness (0x90E22 jumps to 0x90F73)
		if (!dtu_spell_save(o, 4)) {
			if (player) {
				// 0x90E32 also sets 0x13D50C, 0x1F3B00, 0x1F3AF8, 0x46C688
				// and 0x46C68C to 0 (not in the port)
				PALETTE_FLASH_ADD(254, 254, 254);
				HUD_init_message("%s", LN("You lost your eyesight!"));
			}
			if (sound >= 0)
				digi_link_sound_to_pos(sound, o->segnum, 0, &o->pos, 0, 0x5555);
			dtu_char_log("fire trap: obj %d is blind", (int) (o - Objects));
		}
		// 0x90F73: blind for 1 to 1800 seconds
		o->dtu.m_8c[3] = GameTime + i2f(rand() % 1800 + 1);
		o->dtu.m_88 |= DTU_FX_BLIND;
	}
	if (!(o->dtu.m_88 & DTU_FX_DEAF) && !dtu_spell_save(o, 2)) {
		if (player) {
			// 0x2DF7E: the volume of the sounds, rand() % 0x1800 of 0x8000
			digi_set_digi_volume(rand() % 0x1800);
			HUD_init_message("%s", LN("You lost your hearing!"));
		}
		if (sound >= 0)
			digi_link_sound_to_pos(sound, o->segnum, 0, &o->pos, 0, 0x5555);
		o->dtu.m_88 |= DTU_FX_DEAF;
		dtu_char_log("fire trap: obj %d is deaf", (int) (o - Objects));
	}
}

// 0xA6D5A to 0xA6F66 of 0xA6ACB: in level 0 (the Yawning Portal) the
// scenery object 0x29 is a fire trap. When it breaks: a light object, the
// sound 2000, a white flash, the burst of weapon 25 (0x93294), a push of
// the player backwards, then Fireball (spell 16) two times through 0x90CEF.
void dtu_fire_trap(object *d)
{
	object *p = ConsoleObject, *caster = &Objects[1];		// 0x2B07F3: object 1 if no light
	vms_vector pos = d->pos, v;
	int seg = d->segnum, objnum, level, i;

	if (Current_level_num - 1 != 0 || d - Objects != 0x29)
		return;
	dtu_char_log("fire trap: obj %d", (int) (d - Objects));
	// 0x68AC8: type 13 (light), control 14 (light); life 3.0, light 300.0
	objnum = obj_create(OBJ_LIGHT, 0, seg, &pos, NULL, 0, CT_LIGHT, MT_NONE, RT_NONE);
	if (objnum > 0) {
		caster = &Objects[objnum];
		caster->lifeleft = i2f(3);
		caster->ctype.light_info.intensity = i2f(300);
	}
	digi_link_sound_to_pos(2000, seg, 3, &pos, 0, F1_0);		// 0x2E900
	HUD_init_message("%s", LN("You set off a fire trap!!!"));
	PALETTE_FLASH_ADD(254, 254, 254);
	dtu_create_burst(25, seg, &pos);		// 0x93294(25, seg, 3)
	if (p) {
		// the velocity of the player: (up - forward) * 500 to 1000, then
		// y = rand() % 30 (as a fix)
		vm_vec_sub(&v, &p->orient.uvec, &p->orient.fvec);
		vm_vec_copy_scale(&p->mtype.phys_info.velocity, &v, i2f(rand() % 501 + 500));
		p->mtype.phys_info.velocity.y = rand() % 30;
	}
	// The spell hit 0x8CFFF takes the level of a caster of type 13 (a
	// trap) as 2 * the higher class level of the player (0x8D239). The
	// radius of 0x90CEF uses 0xAE3BB, but Fireball has no level_radius.
	level = Dtu_char.pa.class1_level + 1;
	if (Dtu_char.pa.class2_level + 1 > level)
		level = Dtu_char.pa.class2_level + 1;
	level *= 2;
	if (caster->type != OBJ_LIGHT)		// object 1: 0xAE3BB, max hit points / 8, 1 to 18
		level = (caster->id < N_robot_types) ? Monster_info[caster->id].max_hit_points / 8 : 1;
	if (level < 1) level = 1;
	if (caster->type != OBJ_LIGHT && level > 18) level = 18;
	dtu_char_log("fire trap: light object %d, caster %d type %d, level %d", objnum, (int) (caster - Objects), caster->type, level);
	for (i = 0; i < 2; i++)
		dtu_spell_area(16, &pos, seg, caster, level);
}

//------------------------------------------------------------------------
// The view of a frightened player (spells.c 0x8E3C4, 0x8F5F4, 0x903D4)

static int Fear_viewer = -1, Fear_viewer_sig;
static ubyte Fear_render_type, Fear_movement_type;

// 0x8E3C4 (Fear) and 0x8F5F4 (Scare), the player only: 0x13407(segment of
// the player, type 0, flag 0) gates a creature of Robot_info[0] at the
// center of the segment. If there is one: Viewer (0x13D628) = it, 0x69ED3,
// its Fear timer +0xAC = the end of the Fear of the player and its lifeleft
// +0x84 the same value (a time, not a span, so it lasts longer than the
// Fear), 0xA6505(creature, 0), 0x10ADF, and the render type 0 for it and
// for the player. Flag 0 of 0x13407 makes it not hostile (+0x1B0 = 0).
void dtu_fear_view_start(object *player, fix duration)
{
	object *v;
	int objnum;

	if (player != ConsoleObject || Fear_viewer >= 0)
		return;
	objnum = dtu_gate_in_creature(0, player->segnum);
	if (objnum <= 0) {
		dtu_char_log("fear: no creature for the view");
		return;
	}
	v = &Objects[objnum];
	v->dtu.m_1b0 = 0;
	Viewer = v;
	dtu_spell_set_effect(v, DTU_FX_FEAR, duration, 0, 0);
	v->lifeleft = GameTime + duration;
	dtu_creature_wake(v, 0);
	v->render_type = RT_NONE;
	Fear_viewer = objnum;
	Fear_viewer_sig = v->signature;
	Fear_render_type = player->render_type;
	Fear_movement_type = player->movement_type;
	player->render_type = RT_NONE;
	dtu_char_log("fear: the view moves to obj %d in segment %d for %.1f s", objnum, v->segnum, f2fl(duration));
}

// 0x903D4, in 0x9002C at the end of Fear on the player: if Viewer is not
// the player: the creature becomes type 13, should be dead, and 0x6A3CC
// deletes it; 0x13899 puts the player at the center of its segment; the
// player takes its orientation (+0x20 and the copy +0x44); Viewer = the
// player, movement type 1, render type 1. Then "Fear expired.".
void dtu_fear_view_end(object *player)
{
	object *v;
	int seg;

	if (player != ConsoleObject || Fear_viewer < 0)
		return;
	v = &Objects[Fear_viewer];
	Fear_viewer = -1;
	Viewer = player;
	player->render_type = Fear_render_type;
	player->movement_type = Fear_movement_type;
	// ASCENT: the executable uses the object even when it is gone; the port
	// leaves the player in place then
	if (v->signature != Fear_viewer_sig || v->type != OBJ_ROBOT || v->segnum < 0 || v->segnum > Highest_segment_index) {
		dtu_char_log("fear: the view creature is gone, the player stays");
		return;
	}
	seg = v->segnum;
	player->orient = v->orient;
	compute_segment_center(&player->pos, &Segments[seg]);
	obj_relink(player - Objects, seg);
	obj_delete(v - Objects);
	dtu_char_log("fear: the view ends, the player is in segment %d", seg);
}
