/*
 * ASCENT: new file, split from dtuobj.c. The missiles and the bursts:
 * their creation, their flight and their hits. The shared declarations
 * are in dtuobj_int.h.
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

//------------------------------------------------------------------------
// Missiles (laser.c 0x55511, pweapon.c 0xA03A8, collide.c 0x284E0, 0x272DD,
// 0x26C2F). A missile is an object of the type OBJ_WEAPON. The port moves
// it here and not with the weapon code of Descent 1: it flies in a
// straight line with the same speed, with no gravity and no drag.

static void missile_remove(object *obj)
{
	Missile[obj-Objects].signature = 0;
	obj->flags |= OF_SHOULD_BE_DEAD;
}

static fix Missile_life[MAX_OBJECTS];		// lifeleft (object +0x84)

// A value of the $SPELL line of a spell missile, or def
static fix spell_line_fix(int spell,char *key,fix def)
{
	char v[32];

	if (spell < 0 || spell >= N_spell_types || !bm_tbl_get(Spell_info[spell].record,key,v,sizeof(v)))
		return def;
	return fl2f(atof(v));
}

// The trail of a missile: the vclip that 0x5656C puts at the missile in 95
// of 100 frames (0x568EE to 0x56A03: object_create_explosion at the
// position, with the size of the missile). Weapon_info +0xD5 is the
// vclip_trail of the weapon line (0x209F3); 0x56FF6 and 0x946FA copy the one
// of the spell line into the template of a spell missile. -1: none.
static int missile_trail(object *obj,int spell)
{
	if (spell >= 0)
		return f2i(spell_line_fix(spell,"vclip_trail",-F1_0));
	return weapon_int(obj->id,"vclip_trail",-1);
}

// The look and the size of a missile (part of 0x55511): the render type,
// the model or the vclip, and the size.
static fix missile_look(int weapon_type,int spell,int *rtype,int *model,int *vclip)
{
	weapon_info *wi = &Weapon_info[weapon_type];
	fix size = F1_0;

	*rtype = RT_NONE;
	*model = *vclip = -1;
	if (wi->render_type == WEAPON_RENDER_POLYMODEL)
		*model = wi->model_num;
	else if (wi->render_type == WEAPON_RENDER_VCLIP)
		*vclip = wi->weapon_vclip;

	// A spell missile has the look of the spell: the executable copies
	// the render values of the spell to a weapon entry (0x56FF6, 0x946FA).
	if (spell >= 0 && spell < N_spell_types) {
		if (Spell_info[spell].model_num >= 0) {
			*model = Spell_info[spell].model_num;
			*vclip = -1;
		}
		else if (Spell_info[spell].vclip_num >= 0) {
			*vclip = Spell_info[spell].vclip_num;
			*model = -1;
		}
	}

	if (*model >= 0 && *model < N_polygon_models && Polygon_models[*model].dtu_block) {
		// the radius of the model divided by lw_ratio
		*rtype = RT_POLYOBJ;
		size = fixdiv(Polygon_models[*model].rad,wi->po_len_to_width_ratio > 0 ? wi->po_len_to_width_ratio : i2f(10));
	}
	else if (*vclip >= 0 && *vclip < VCLIP_MAXNUM) {
		*rtype = RT_WEAPON_VCLIP;
		size = wi->blob_size;
		// ASCENT: 0x56FF6 (the player) and 0x946FA (a creature) copy
		// weapon_vclip_size of the spell line (Spell_info +0x68, 1.0 when
		// the line has none, 0x242F6) to the blob size of the template
		// (Weapon_info +0x6C). The port took the blob size of the template
		// (1/16): a Fireball (size 3.0) was a dot.
		if (spell >= 0 && spell < N_spell_types)
			size = spell_line_fix(spell,"weapon_vclip_size",F1_0);
	}
	if (size < F1_0/16)
		size = F1_0/16;
	return size;
}

// The speed of the difficulty level, one half for a weapon with thrust.
static fix missile_speed(weapon_info *wi)
{
	fix speed = wi->speed[Difficulty_level];

	if (wi->thrust)
		speed /= 2;
	return speed;
}

// Laser_create_new (0x55511)
int dtu_create_missile(vms_vector *dir, vms_vector *pos, int segnum, int parent, int weapon_type, int spell, int make_sound)
{
	weapon_info *wi;
	int objnum, rtype, model, vclip, bounce;
	fix size, speed;
	object *obj;
	dtu_missile *m;
	vms_matrix orient = vmd_identity_matrix;

	if (weapon_type < 0 || weapon_type >= N_weapon_types)
		weapon_type = 0;
	if (N_weapon_types <= 0 || segnum < 0 || segnum > Highest_segment_index)
		return -1;
	wi = &Weapon_info[weapon_type];

	size = missile_look(weapon_type,spell,&rtype,&model,&vclip);
	if (rtype == RT_POLYOBJ)
		vm_vector_2_matrix(&orient,dir,(parent >= 0) ? &Objects[parent].orient.uvec : NULL,NULL);

	objnum = obj_create(OBJ_WEAPON,weapon_type,segnum,pos,&orient,size,CT_NONE,MT_NONE,rtype);
	if (objnum < 0)
		return -1;
	obj = &Objects[objnum];
	if (rtype == RT_POLYOBJ) {
		obj->rtype.pobj_info.model_num = model;
		obj->rtype.pobj_info.tmap_override = -1;
	}
	else if (rtype == RT_WEAPON_VCLIP)
		obj->rtype.vclip_info.vclip_num = vclip;

	// weapon +0x0A: bounce 1, stick 2, thrown 3
	bounce = 0;
	if (weapon_int(weapon_type,"bounce",0)) bounce = 1;
	if (weapon_int(weapon_type,"stick",0)) bounce = 2;
	if (weapon_int(weapon_type,"thrown",0)) bounce = 3;
	// ASCENT: the cast of a creature or of the player (parent >= 0) goes
	// through 0x56FF6, which writes 0 to the bounce byte of the template and
	// 1 for Lightning Bolt (0x571C5); 0x55511 turns byte 1 into the physics
	// flag 4 (0x55736), so the bolt bounces at a wall (0xA0731). The trap
	// cast 0x946FA keeps the byte of its template.
	if (spell >= 0 && parent >= 0)
		bounce = (spell == 32) ? 1 : 0;

	// ASCENT: Color Spray: 0x56FF6 sets the vclip of the template to
	// 175 + rand() % 6 once for each cast, so all the missiles of
	// a cast have one color. The first missile of a cast is the one with
	// the sound (make_sound). The port took the vclip 180 of the line.
	if (spell == 6 && parent >= 0 && rtype == RT_WEAPON_VCLIP) {
		static int spray_vclip = 180;

		if (make_sound)
			spray_vclip = 175 + rand() % 6;
		if (spray_vclip < VCLIP_MAXNUM)
			obj->rtype.vclip_info.vclip_num = spray_vclip;
	}

	m = &Missile[objnum];
	memset(m,0,sizeof(*m));
	m->signature = obj->signature;
	m->parent = parent;
	m->parent_sig = (parent >= 0) ? Objects[parent].signature : -1;
	m->spell = spell;
	if (spell >= 0)
		dtu_spell_template_used(weapon_type);		// 0x571B2: the template is magic
	m->bounce = bounce;
	m->pass_obj = -1;
	// ASCENT: 0x56FF6 writes player_light of the spell (0 when the line has
	// none, 0x242F6) to lightcast of the template, which 0x577FE adds to
	// the light of the segment. The port kept the light of the template
	// (0 for the hands, 1.0 for a wand or a scroll). The template is shared,
	// so the port keeps the value with the missile.
	m->light = (spell >= 0 && parent >= 0) ? spell_line_fix(spell,"player_light",0) : -1;

	// The time to live. The port keeps it here: the object code of
	// Descent 1 would explode a weapon with a damage radius at its end.
	Missile_life[objnum] = wi->lifetime;
	// 0x56FF6 writes 30.0 to the template of a spell missile (the caster
	// sets the life of Lightning Bolt and Acid Arrow, dtu_missile_set_life)
	if (spell >= 0)
		Missile_life[objnum] = i2f(30);
	if (bounce == 2)
		Missile_life[objnum] += (rand() * 4) - F1_0;

	// The velocity of the parent is not added.
	speed = missile_speed(wi);
	vm_vec_copy_scale(&obj->mtype.phys_info.velocity,dir,speed);

	vclip_at(obj->segnum,&obj->pos,wi->flash_size,wi->flash_vclip);
	if (make_sound && wi->flash_sound > -1)
		sound_at(wi->flash_sound,obj->segnum,&obj->pos,F1_0);

	dtu_obj_log("missile: obj %d weapon %d '%s' spell %d from obj %d, render %d size %.2f speed %.1f life %.1f, vclip %d light %.1f bounce %d, at %.1f %.1f %.1f",
		objnum,weapon_type,Weapon_item_info[weapon_type].name,spell,parent,rtype,f2fl(size),f2fl(speed),f2fl(Missile_life[objnum]),
		rtype == RT_WEAPON_VCLIP ? obj->rtype.vclip_info.vclip_num : -1,f2fl(dtu_missile_light(obj)),bounce,
		f2fl(pos->x),f2fl(pos->y),f2fl(pos->z));
	return objnum;
}

fix dtu_missile_light(object *obj)
{
	int n = obj-Objects;

	if (obj->type == OBJ_WEAPON && Missile[n].signature == obj->signature && Missile[n].light >= 0)
		return Missile[n].light;
	return (obj->id < N_weapon_types) ? Weapon_info[obj->id].light : 0;
}

int dtu_missile_spell(object *obj)
{
	int n = obj-Objects;

	if (obj->type != OBJ_WEAPON || Missile[n].signature != obj->signature)
		return -1;
	return Missile[n].spell;
}

void dtu_missile_set_life(int objnum, fix life)
{
	if (objnum >= 0 && objnum < MAX_OBJECTS)
		Missile_life[objnum] = life;
}

// ASCENT: Dtu_caster_ai: the missile passes the object (the scenery at the
// body of its caster)
void dtu_missile_set_pass(int objnum, int pass)
{
	if (objnum >= 0 && objnum < MAX_OBJECTS && pass >= 0 && pass <= Highest_object_index) {
		Missile[objnum].pass_obj = pass;
		Missile[objnum].pass_sig = Objects[pass].signature;
	}
}

// 0x93294: the burst of a weapon with a min_speed (the bomb of a trap):
// "magnitude" fragments that are drawn as the vclip, with a size of 8.0 to
// 17.0, in random directions. They bounce.
// ASCENT: each fragment has the vclip base + rand() % vclip_range (0x932EE
// to 0x93306: the value goes to Weapon_info.weapon_vclip before obj_create,
// and 0x93481 to 0x93496 write the base back after the last fragment). The
// executable divides by a vclip_range of 0 (idiv at 0x93300); the port takes
// the base then. A failed obj_create leaves at 0x9333B with the random value
// in the table (D-182); Dtu_fix_burst_vclip (dtuobj.c) writes the base back.
void dtu_create_burst(int weapon_type, int segnum, vms_vector *pos)
{
	int n = weapon_int(weapon_type,"magnitude",0), i;
	int min_speed = weapon_int(weapon_type,"min_speed",0), range = weapon_int(weapon_type,"speed_range",0);
	int vrange = weapon_int(weapon_type,"vclip_range",0);
	short base;

	if (weapon_type < 0 || weapon_type >= N_weapon_types)
		return;
	base = Weapon_info[weapon_type].weapon_vclip;
	if (n > 40)
		n = 40;
	for (i=0;i<n;i++) {
		vms_vector dir;
		int objnum;

		if (vrange > 0)
			Weapon_info[weapon_type].weapon_vclip = base + rand() % vrange;
		make_random_vector(&dir);
		objnum = dtu_create_missile(&dir,pos,segnum,-1,weapon_type,-1,0);
		if (objnum < 0) {
			if (Dtu_fix_burst_vclip)
				Weapon_info[weapon_type].weapon_vclip = base;
			else
				dtu_obj_log("burst: weapon %d keeps the vclip %d (Dtu_fix_burst_vclip 0)",weapon_type,Weapon_info[weapon_type].weapon_vclip);
			dtu_obj_log("burst: weapon %d, %d of %d fragments",weapon_type,i,n);
			return;
		}
		vm_vec_copy_scale(&Objects[objnum].mtype.phys_info.velocity,&dir,i2f(min_speed + (range > 0 ? rand() % range : 0)) / 4);
		Objects[objnum].size = i2f(8 + rand() % 10);
		Missile[objnum].bounce = 1;
		Missile_life[objnum] -= rand() * 2;
	}
	Weapon_info[weapon_type].weapon_vclip = base;
	dtu_obj_log("burst: weapon %d, %d fragments",weapon_type,n);
}

// collide_weapon_and_wall (0x26C2F) and the end of 0xA0731
static void missile_hit_wall(object *obj,vms_vector *point,vms_vector *wallnorm,int segnum,int side)
{
	dtu_missile *m = &Missile[obj-Objects];
	weapon_info *wi = &Weapon_info[obj->id];

	// 0x26C2F: the spell of the missile. Knock (28) calls the door function
	// 0x7E859(seg, side, 1, parent); an area spell has its area here
	// (0x90CEF at 0x26D63), which also removes the missile.
	if (m->spell >= 0 && !m->harmless) {
		object *parent = (m->parent >= 0 && Objects[m->parent].signature == m->parent_sig) ? &Objects[m->parent] : NULL;

		if (m->spell == 28 && side >= 0 && segnum >= 0) {
			dtu_obj_log("missile: Knock hits seg %d side %d",segnum,side);
			dtu_wall_use_door(&Segments[segnum],side,parent,1);
		}
		else if (dtu_spell_is_area(m->spell)) {
			vms_vector p = *point;
			int seg = segnum;

			// the point just in front of the wall
			if (wallnorm) {
				vm_vec_scale_add2(&p,wallnorm,F1_0/4);
				if ((seg = find_point_seg(&p,segnum)) < 0) { p = *point; seg = segnum; }
			}
			dtu_obj_log("missile: the area spell %d of obj %d hits a wall",m->spell,(int)(obj-Objects));
			missile_remove(obj);
			// ASCENT: 0x90CEF computes the caster level itself (0x90D01: the
			// player 0x91EC1, else the creature level 0xAE3BB), as
			// dtu_spell_hit does for a missile that hits a body. The port
			// passed caster_level(), which is 1 for the player: a Fireball of
			// the player that hit a wall did 1d6 at each level.
			dtu_spell_area(m->spell,&p,seg,parent,parent ? dtu_spell_object_level(parent,m->spell) : 1);
			return;
		}
	}

	// ASCENT: Lightning Bolt (32) at a wall: 0x26DC3 writes -1 to the
	// parent signature (+0x144, the dword that laser_are_related 0x55369
	// compares) and leaves before the removal at 0x26FAF, with no sound and
	// no vclip; 0xA0731 then bounces it (physics flag 4). So the bolt flies
	// for its 2.0 seconds and can hit its caster after the bounce. At the
	// scenery 0x2641C keeps it in the same way, after the hit vclip; the
	// port lets it pass that object. The port removed the bolt.
	if (m->spell == 32 && m->bounce == 1) {
		m->unlinked = 1;
		if (!wallnorm) {
			if (obj->render_type == RT_POLYOBJ)
				vclip_at(segnum,point,obj->size*2,f2i(spell_line_fix(32,"wall_hit_vclip",-F1_0)));
			return;
		}
		vm_vec_negate(&obj->mtype.phys_info.velocity);
		dtu_obj_log("missile: Lightning Bolt obj %d bounces at %.1f %.1f %.1f",(int)(obj-Objects),f2fl(point->x),f2fl(point->y),f2fl(point->z));
		return;
	}
	if (m->bounce == 1 && wallnorm) {
		// 0xA07D7 negates the velocity of a missile that bounces
		vm_vec_negate(&obj->mtype.phys_info.velocity);
		return;
	}
	// A missile that is not a model goes away with no sound and no vclip.
	if (obj->render_type == RT_POLYOBJ) {
		int sound = wi->wall_hit_sound, vclip = wi->wall_hit_vclip;

		// ASCENT: for a missile with a spell 0x26C2F takes the wall_hit_vclip
		// and the wall_hit_sound of the spell line (0x26EC7 to 0x26EE1;
		// 0x56FF6 also copies them to the template). The port played those
		// of the template weapon, which has none: no ring of Magic Missile
		// (101) and no splash of Melf's Acid Arrows (116).
		if (m->spell >= 0) {
			sound = f2i(spell_line_fix(m->spell,"wall_hit_sound",-F1_0));
			vclip = f2i(spell_line_fix(m->spell,"wall_hit_vclip",-F1_0));
		}
		sound_at(sound,segnum,point,F1_0/2);
		vclip_at(segnum,point,obj->size*2,vclip);
		dtu_obj_log("missile: obj %d of spell %d at a wall: vclip %d sound %d",(int)(obj-Objects),m->spell,vclip,sound);
		// 0x26F73, 0x2653C: the alert function 0x160AB(missile, 2) after the
		// sound of the wall or of the scenery
		dtu_creature_alert(obj,2);
	}
	dtu_obj_log("missile: obj %d hits a wall at %.1f %.1f %.1f",(int)(obj-Objects),f2fl(point->x),f2fl(point->y),f2fl(point->z));
	missile_remove(obj);
}

// collide_player_and_weapon (0x284E0) and collide_robot_and_weapon
// (0x272DD). The executable lets a missile with "stick" or "thrown" drop
// after the hit. The port takes each missile away.
static void missile_hit_object(object *obj,object *other,vms_vector *point)
{
	dtu_missile *m = &Missile[obj-Objects];
	weapon_info *wi = &Weapon_info[obj->id];
	int weapon = obj->id, spell, sound;
	object *parent = NULL;
	fix dealt = 0, hp, spell_dealt;

	if (m->parent >= 0 && Objects[m->parent].signature == m->parent_sig)
		parent = &Objects[m->parent];

	// ASCENT: Dtu_fix_revive: a spell missile on the fallen body of a
	// creature that can get up (missile_path_object). An area spell has its
	// area at the body (dtu_spell_area reaches the body), another spell its
	// dice on the body; fire or acid destroys it (dtu_creature_down_spell).
	if (m->spell >= 0 && !m->harmless && dtu_creature_down(other)) {
		int L = parent ? dtu_spell_object_level(parent,m->spell) : 1;

		dtu_obj_log("missile: obj %d of spell %d hits the fallen body of obj %d",(int)(obj-Objects),m->spell,(int)(other-Objects));
		vclip_at(obj->segnum,point,wi->impact_size,f2i(spell_line_fix(m->spell,"wall_hit_vclip",-F1_0)));
		missile_remove(obj);
		if (dtu_spell_is_area(m->spell))
			dtu_spell_area(m->spell,point,obj->segnum,parent,L);
		else
			dtu_creature_down_spell(other,parent,m->spell,L);
		return;
	}

	if (other->type == OBJ_DRESSING) {
		// 0x2641C: the missile casts its spell on the scenery (the spells
		// decide whether it breaks); the weapon does no damage. The missile
		// stops like at a wall. (guess for the stop)
		if (m->spell >= 0 && !m->harmless) {
			spell_hit(m->spell,other,parent);
			if (obj->flags & OF_SHOULD_BE_DEAD)
				return;
			if (dtu_spell_is_area(m->spell)) {
				missile_remove(obj);
				return;
			}
		}
		missile_hit_wall(obj,point,NULL,obj->segnum,-1);
		// ASCENT: a Lightning Bolt that goes on (0x2641C) passes the object
		if (!(obj->flags & OF_SHOULD_BE_DEAD)) {
			m->pass_obj = other-Objects;
			m->pass_sig = other->signature;
		}
		return;
	}
	if (m->harmless)
		return;

	// A spell missile does the damage of the spell (dtu_spell_hit). A
	// weapon with enemy_spell does the effect of the spell and the damage
	// of the weapon.
	spell = (m->spell >= 0) ? m->spell : weapon_int(weapon,"enemy_spell",-1);
	// ASCENT: the cause of a death for the session log (dtuxp.h)
	if (other->type == OBJ_ROBOT)
		dtu_xp_cause(!parent ? "missile with no source (a trap or a lost parent)" : parent->type == OBJ_ROBOT ? "creature missile" : "missile of the player");
	spell_dealt = (spell >= 0) ? spell_hit(spell,other,parent) : 0;

	if (other->type == OBJ_PLAYER) {
		Hit_by_missile = 1;		// ASCENT: Dtu_fix_shield_ac
		dealt = (m->spell >= 0) ? spell_dealt : damage_player_w(other,parent,0,weapon);
		Hit_by_missile = 0;
		hp = dtu_player_hit_points();
		if (dealt > 0) {
			sound = weapon_int(weapon,"monster_hit_sound",-1);
			if (hp < F1_0 && weapon_int(weapon,"monster_kill_sound",-1) >= 0)
				sound = weapon_int(weapon,"monster_kill_sound",-1);
			sound_at(sound,other->segnum,point,0x5555);
			vclip_at(obj->segnum,point,wi->impact_size,wi->robot_hit_vclip);
		}
		else {
			sound_at(weapon_int(weapon,"monster_miss_sound",-1),other->segnum,point,0x5555);
			vclip_at(obj->segnum,point,wi->impact_size,weapon_int(weapon,"monster_miss_vclip",-1));
		}
	}
	else if (other->type == OBJ_ROBOT) {
		// 0x274B7: a missile of the player calls the alert function
		// 0x160AB(creature, 3) before the activity
		if (parent && parent == ConsoleObject)
			dtu_creature_alert(other,3);
		Ail[other-Objects].activity = 2;
		Hit_by_missile = 1;		// ASCENT: Dtu_fix_shield_ac
		dealt =(m->spell >= 0) ? spell_dealt : damage_creature_w(other,parent,0,weapon);
		Hit_by_missile = 0;
		hp = other->shields;
		if (dealt > 0) {
			blood(other);
			if (hp > i2f(2) && Monster_info[other->id].hit_sound > 0)
				sound_at(Monster_info[other->id].hit_sound,other->segnum,point,F1_0);
			// ASCENT: the original plays no weapon sound when the hit
			// kills (0x27650: die_sound, or monster_kill_sound, which no
			// shipped table sets). The port plays the impact sound on the
			// killing hit too (docs/re/bugs.md), Dtu_fix_kill_sound of
			// dtucombat.c; -origcombat plays monster_kill_sound.
			else
				sound_at(weapon_int(weapon,(hp < F1_0 && !Dtu_fix_kill_sound) ? "monster_kill_sound" : "monster_hit_sound",-1),
					other->segnum,point,F1_0/4);
		}
		else
			sound_at(weapon_int(weapon,"monster_miss_sound",-1),other->segnum,point,F1_0/4);
	}
	dtu_xp_cause(NULL);
	// D-180, Dtu_fix_hit_keep (dtuobj.c): with the rule of the executable a
	// hit that takes hit points with a weapon whose monster_hit_vclip is
	// below 0 (0x277CD) skips maybe_kill_weapon, and 0x27883 removes only a
	// missile that is a model. The rule is applied to a plain missile (no
	// spell, no bounce, stick or thrown: those take other ways of 0x272DD in
	// the executable). The kept missile passes the creature that it hit, so
	// that the port does not hit it again in each frame (guess).
	if (!Dtu_fix_hit_keep && other->type == OBJ_ROBOT && m->spell < 0 && m->bounce == 0 && dealt > 0 &&
			wi->robot_hit_vclip < 0 && obj->render_type != RT_POLYOBJ) {
		m->pass_obj = other-Objects;
		m->pass_sig = other->signature;
		dtu_obj_log("missile: obj %d goes on after the hit of obj %d (no monster_hit_vclip, Dtu_fix_hit_keep 0)",(int)(obj-Objects),m->pass_obj);
		return;
	}
	missile_remove(obj);
}

// The objects on the way of a missile from its position to end (part of
// 0xA03A8): the number of the object that it hits first, or -1, and the
// hit point. The executable does this in the same query as the walls. The
// port tests each body, see weapon_test(). step is the length of the move
// in this frame. The missile can be a stand in object that is not in the
// list (dtu_missile_path_target). spell: the spell of the missile or -1.
static int missile_path_object(object *obj,vms_vector *end,fix step,int parent,int parent_sig,int pass,int pass_sig,int spell,vms_vector *best_ip)
{
	int j, best = -1, down;
	fix best_d = 0x7fffffff, rad_h, rad_v;
	vms_vector ip;

	// ASCENT: the query of the missile (0xA05AD) has the radii of 0x748D9
	// for the missile, and fvi_sub tests a creature with 0x3EE70: the ray
	// against the capped cylinder of its body (0x4F7D5, dtu_body_query_test),
	// so a steep shot down at a low creature goes into the top of the body.
	dtu_query_radii(obj,&rad_h,&rad_v);

	for (j=0;j<=Highest_object_index;j++) {
		object *o = &Objects[j];
		fix d;

		if (o == obj || (o->type != OBJ_ROBOT && o->type != OBJ_PLAYER && o->type != OBJ_DRESSING))
			continue;
		if (j == parent && o->signature == parent_sig)
			continue;
		if (j == pass && o->signature == pass_sig)		// Dtu_fix_hit_keep
			continue;
		// ASCENT: Dtu_caster_ai (dtuopts.h): the spell missile of a creature
		// passes the creatures of its side (its spell does not act on them,
		// apply_spell of dtuspell.c)
		if (spell >= 0 && o->type == OBJ_ROBOT && parent > 0 && parent <= Highest_object_index && Objects[parent].type == OBJ_ROBOT &&
			 Objects[parent].signature == parent_sig && dtu_caster_ai() &&
			 !dtu_creature_is_hostile(&Objects[parent]) == !dtu_creature_is_hostile(o))
			continue;
		// ASCENT: Dtu_fix_revive: a spell missile of the player hits the
		// fallen body of a creature that can get up (dtu_creature_down_spell),
		// as the area of a spell and the swing do. The port let it fly through
		// the body. The missiles of the creatures still pass it.
		down = spell >= 0 && ConsoleObject && parent == ConsoleObject-Objects && dtu_creature_down(o);
		if (o->flags & OF_SHOULD_BE_DEAD)
			continue;
		if ((o->flags & OF_DESTROYED) && !down)
			continue;
		if (o->type == OBJ_ROBOT && DTU_ANIM_STATE(o) == DAS_DIE && !down)
			continue;
		if (o->type == OBJ_DRESSING && (o->id >= N_dressing_types || Dressing_info[o->id].pass_thru))
			continue;
		if (vm_vec_dist_quick(&o->pos,&obj->pos) > step + o->size + i2f(30))
			continue;
		Fvi_dtu_player_attack = ConsoleObject && parent == ConsoleObject-Objects;	// ASCENT: Dtu_fix_rat_hit
		if (o->type == OBJ_ROBOT) {
			int dead = o->flags & OF_DESTROYED;

			Fvi_dtu_missile = 1;		// Dtu_fix_body_ray
			o->flags &= ~OF_DESTROYED;		// the fallen body (down)
			d = dtu_body_query_test(&ip,&obj->pos,end,rad_h,rad_v,o);
			o->flags |= dead;
			Fvi_dtu_missile = 0;
		}
		else
			d = dtu_object_hit_test(&ip,&obj->pos,end,obj->size,o,obj);
		Fvi_dtu_player_attack = 0;
		// ASCENT: Dtu_improved_ai: not a body behind a wall or a door that
		// is not open (attack_wall_between(), dtuobj_aware.c)
		if (d > 0 && attack_wall_between(&ip,find_point_seg(&ip,obj->segnum),o,-1)) {
			if (obj >= Objects && obj < Objects + MAX_OBJECTS)		// not the stand in of the target hint
				dtu_obj_log("missile: a wall or a closed door is between the missile and obj %d %s",j,dtu_object_name(o));
			continue;
		}
		if (d > 0 && d < best_d) {
			best_d = d;
			best = j;
			*best_ip = ip;
		}
	}
	return best;
}

// ASCENT: the target hint (dtuobj.h). The way of a new missile as
// missiles_frame moves it: a straight line at the same speed, the wall
// test with the radius of the missile up to 1.0, then the bodies on the
// line up to the wall.
int dtu_missile_path_target(vms_vector *pos, int segnum, vms_vector *dir, int weapon_type, int spell, int parent, fix range)
{
	return dtu_missile_path_target_pass(pos,segnum,dir,weapon_type,spell,parent,range,-1);
}

// ASCENT: the same with an object that the missile passes (pass, -1: none),
// for Dtu_caster_ai: the scenery at the body of a caster. step above 0: the
// bodies are tested in pieces of this length, as the flight tests them in
// each frame. One long piece can miss a body that the missile grazes (a
// missile that comes down over a table: the test of the whole line takes
// the height at the edge of the body, the flight is inside its radius
// later, at the height of its top).
int dtu_missile_path_target_pass(vms_vector *pos, int segnum, vms_vector *dir, int weapon_type, int spell, int parent, fix range, int pass)
{
	return dtu_missile_path_target_step(pos,segnum,dir,weapon_type,spell,parent,range,pass,0);
}

int dtu_missile_path_target_step(vms_vector *pos, int segnum, vms_vector *dir, int weapon_type, int spell, int parent, fix range, int pass, fix step)
{
	static object stand_in;
	int rtype, model, vclip, fate;
	vms_vector p1, end, ip;
	fvi_query fq;
	fvi_info hit;

	if (weapon_type < 0 || weapon_type >= N_weapon_types || segnum < 0 || parent < 0 || range <= 0)
		return -1;
	memset(&stand_in,0,sizeof(stand_in));
	stand_in.type = OBJ_WEAPON;
	stand_in.id = weapon_type;
	stand_in.pos = *pos;
	stand_in.segnum = segnum;
	stand_in.size = missile_look(weapon_type,spell,&rtype,&model,&vclip);
	// ASCENT: the look of the missile, for its query radii (dtu_query_radii)
	stand_in.render_type = rtype;
	if (rtype == RT_POLYOBJ)
		stand_in.rtype.pobj_info.model_num = model;

	vm_vec_scale_add(&p1,pos,dir,range);
	fq.p0 = pos;
	fq.p1 = &p1;
	fq.startseg = segnum;
	fq.rad = (stand_in.size < F1_0) ? stand_in.size : F1_0;
	fq.thisobjnum = parent;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate != HIT_NONE && fate != HIT_WALL)
		return -1;
	end = (fate == HIT_NONE) ? p1 : hit.hit_pnt;
	if (step > 0) {
		fix len = vm_vec_dist(pos,&end), done = 0;

		while (done < len) {
			fix piece = len - done < step ? len - done : step;
			vms_vector e;
			int best, seg;

			vm_vec_scale_add(&e,&stand_in.pos,dir,piece);
			best = missile_path_object(&stand_in,&e,piece,parent,Objects[parent].signature,pass,pass >= 0 ? Objects[pass].signature : 0,spell,&ip);
			if (best >= 0)
				return best;
			stand_in.pos = e;
			if ((seg = find_point_seg(&e,stand_in.segnum)) >= 0)
				stand_in.segnum = seg;
			done += piece;
		}
		return -1;
	}
	return missile_path_object(&stand_in,&end,range,parent,Objects[parent].signature,pass,pass >= 0 ? Objects[pass].signature : 0,spell,&ip);
}

// The range of a missile weapon: its speed for its life.
fix dtu_missile_weapon_range(int weapon_type)
{
	if (weapon_type < 0 || weapon_type >= N_weapon_types)
		return 0;
	return fixmul(missile_speed(&Weapon_info[weapon_type]),Weapon_info[weapon_type].lifetime);
}

// The movement of all missiles for one frame (0xA03A8 and 0x5656C).
void missiles_frame(void)
{
	int i;

	for (i=0;i<=Highest_object_index;i++) {
		object *obj = &Objects[i];
		dtu_missile *m = &Missile[i];
		vms_vector p1, end, best_ip, *vel;
		fvi_query fq;
		fvi_info hit;
		int fate, best;
		fix step;

		if (m->signature == 0)
			continue;
		if (obj->type != OBJ_WEAPON || obj->signature != m->signature || (obj->flags & OF_SHOULD_BE_DEAD)) {
			m->signature = 0;
			continue;
		}

		Missile_life[i] -= FrameTime;
		if (Missile_life[i] < 0) {
			if (m->spell >= 0)
				dtu_obj_log("missile: obj %d of spell %d ends its life",i,m->spell);
			missile_remove(obj);
			// ASCENT: Dtu_fix_spell_range: an area spell takes effect at the
			// end of its range (AD&D 2e); in 1.3 the missile goes away
			if (m->spell >= 0 && !m->harmless && dtu_spell_ends_at_range(m->spell)) {
				object *parent = (m->parent >= 0 && Objects[m->parent].signature == m->parent_sig) ? &Objects[m->parent] : NULL;

				dtu_spell_area(m->spell,&obj->pos,obj->segnum,parent,parent ? dtu_spell_object_level(parent,m->spell) : 1);
			}
			continue;
		}

		vel = &obj->mtype.phys_info.velocity;
		// 0x5656C: a missile that is slower than 2.0 goes away
		if (m->bounce < 2 && vm_vec_mag_quick(vel) < i2f(2)) {
			missile_remove(obj);
			continue;
		}

		// A thrown weapon turns by 45 degrees in each frame (0x6BD68). The
		// corrected rule: in each reference frame time.
		if (m->bounce == 3 && obj->render_type == RT_POLYOBJ) {
			int turns = 1;

			if (Dtu_fix_creature_move) {
				m->tumble_acc += FrameTime;
				turns = m->tumble_acc / DTU_REF_FRAMETIME;
				m->tumble_acc -= turns * DTU_REF_FRAMETIME;
			}
			while (turns-- > 0) {
				vms_angvec a;
				vms_matrix rot, t;

				a.p = 0x2000; a.b = a.h = 0;
				vm_angles_2_matrix(&rot,&a);
				vm_matrix_x_matrix(&t,&obj->orient,&rot);
				obj->orient = t;
			}
		}

		vm_vec_scale_add(&p1,&obj->pos,vel,FrameTime);
		step = vm_vec_dist(&p1,&obj->pos);

		fq.p0 = &obj->pos;
		fq.p1 = &p1;
		fq.startseg = obj->segnum;
		fq.rad = (obj->size < F1_0) ? obj->size : F1_0;
		fq.thisobjnum = i;
		fq.ignore_obj_list = NULL;
		fq.flags = 0;
		fate = find_vector_intersection(&fq,&hit);
		end = (fate == HIT_NONE) ? p1 : hit.hit_pnt;

		// ASCENT: with the parent signature cleared (m->unlinked) the
		// missile is no longer related to its parent and can hit it
		best = missile_path_object(obj,&end,step,m->parent,m->unlinked ? -1 : m->parent_sig,m->pass_obj,m->pass_sig,
			m->harmless ? -1 : m->spell,&best_ip);
		if (best >= 0) {
			missile_hit_object(obj,&Objects[best],&best_ip);
			if (obj->flags & OF_SHOULD_BE_DEAD)
				continue;
		}

		if (fate == HIT_WALL)
			missile_hit_wall(obj,&hit.hit_pnt,&hit.hit_wallnorm,hit.hit_seg >= 0 ? hit.hit_seg : obj->segnum,hit.hit_side_seg >= 0 ? hit.hit_side : -1);
		else if (fate == HIT_NONE) {
			int seg = (hit.hit_seg >= 0) ? hit.hit_seg : find_point_seg(&p1,obj->segnum);

			if (seg < 0)
				missile_remove(obj);
			else {
				int trail = missile_trail(obj,m->spell), n = 1;

				obj->pos = p1;
				if (seg != obj->segnum)
					obj_relink(i,seg);
				// one try in each frame of the executable; with
				// Dtu_fix_creature_move in each reference frame time, as the
				// turn of a thrown weapon above
				if (trail > 0 && Dtu_fix_creature_move) {
					m->trail_acc += FrameTime;
					n = m->trail_acc / DTU_REF_FRAMETIME;
					m->trail_acc -= n * DTU_REF_FRAMETIME;
				}
				if (trail > 0 && n > 0 && rand() % 100 < 95)
					vclip_at(obj->segnum,&obj->pos,obj->size,trail);
			}
		}
		else
			missile_remove(obj);
	}
}
