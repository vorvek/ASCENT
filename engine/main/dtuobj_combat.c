/*
 * ASCENT: new file, split from dtuobj.c. The fight: the hit test, the
 * to-hit roll, the damage of weapons and spells (with the rules of the
 * trolls and the rats), the death of a creature and of the player, and
 * the weapon test of a melee swing. The shared declarations are in
 * dtuobj_int.h.
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

static void player_dies(object *player,object *attacker);

// 0x13A956: the bonus of the strength for the to-hit roll
static const signed char Str_to_hit[20] = {
	0, -5, -3, -3, -2, -2, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 3 };

//------------------------------------------------------------------------
// Damage

// 0x2A789 without the percent part of strength 18.
// ASCENT: D-003 (verified in the bytes): the creature path calls 0x2A789
// with the percent 100 (0x2ACA2), and 0x2A789 adds the 18/91-100 part when
// the player (not the creature) is a fighter (0x2A7AB to 0x2A7B7). 1
// (default): the plain bonus of the strength. 0 (-origcombat, dtuchar.c):
// the rule of the executable.
int Dtu_fix_creature_strength = 1;

static int str_bonus(int str)
{
	if (!Dtu_fix_creature_strength)
		return dtu_char_str_hit_bonus(str,100);
	if (str < 1) str = 1;
	if (str > 19) str = 19;
	return Str_to_hit[str];
}

/// 0x2AA5B to 0x2AF4F of 0x2A7F3: the effects of the attacker and of the
// target, the armor class of the target, the test, and Stone Skin. roll is
// the d20 with the bonuses of the attacker. Returns 1 for a hit, 0 for a
// miss; *ac_out is the armor class of the test.
int dtu_hit_test(object *target,object *attacker,int roll,int thac0,int *ac_out)
{
	return dtu_hit_test_vs(target,attacker,roll,thac0,ac_out,0);
}

// ASCENT: 1 while the hit of a weapon missile is applied (missile_hit_object),
// for the armor class against missiles (Dtu_fix_shield_ac)
int Hit_by_missile = 0;

int dtu_hit_test_vs(object *target,object *attacker,int roll,int thac0,int *ac_out,int missile)
{
	int tw = target->dtu.m_88, aw = attacker ? attacker->dtu.m_88 : 0, ac;
	int robot_attacker = attacker && attacker->type == OBJ_ROBOT && attacker->id < N_robot_types;
	int robot_target = target->type == OBJ_ROBOT && target->id < N_robot_types;

	// 0x2AA5B: an attacker that levitates: 1 plus 4 for each unit of its
	// speed, at most 5
	if (aw & DTU_FX_LEVITATE) {
		int m = (vm_vec_mag(&attacker->mtype.phys_info.velocity) >> 14) + 1;
		roll -= (m < 5) ? m : 5;
		dtu_xp_hit_mod(-((m < 5) ? m : 5),LN("levitating"));		// ASCENT: the verbose combat log (dtuxp.h)
	}
	if (target->type == OBJ_PLAYER) {
		// ASCENT: the value for the attack type (Shield, Dtu_fix_shield_ac)
		ac = missile ? dtu_char_armor_class_vs(1) : dtu_player_armor_class();
		// 0x2ACE8: the player is hidden from the attacker (0x165C4)
		if (robot_attacker && dtu_thief_hidden_from(attacker)) {
			roll -= 4;
			dtu_xp_hit_mod(-4,LN("hidden"));
		}
	}
	else {
		// 0x2ACFC: an invisible creature that the player does not see (0x16636)
		if (dtu_spell_unseen(target)) {
			roll -= 4;
			dtu_xp_hit_mod(-4,LN("unseen"));
		}
		ac = robot_target ? Monster_info[target->id].armor_class : 10;
	}
	// 0x2AD27: Bless of the attacker
	if (aw & DTU_FX_BLESS) {
		roll++;
		dtu_xp_hit_mod(1,LN("Bless"));
	}
	// 0x2AD36: Protection From Evil of the target: an extraplanar creature
	// always misses, another creature has -2
	if ((tw & DTU_FX_PROT_EVIL) && robot_attacker) {
		int r0 = roll;

		roll = (Monster_info[attacker->id].flags & MONSTER_F_EXTRAPLANAR) ? 0 : roll - 2;
		dtu_xp_hit_mod(roll - r0,LN("Protection From Evil"));
	}
	// 0x2AD70: the attack on an extraplanar creature ends Protection From
	// Evil of the attacker (its timer +0xE0 is 0)
	if ((aw & DTU_FX_PROT_EVIL) && robot_target && (Monster_info[target->id].flags & MONSTER_F_EXTRAPLANAR))
		attacker->dtu.m_e0[0] = 0;
	// 0x2ADA3: the effects that change the armor class of a creature
	if (robot_target)
		ac = dtu_spell_creature_ac_vs(target,ac,missile);
	dtu_obj_log("attack: armor class %d of obj %d %s against %s",ac,(int)(target-Objects),dtu_object_name(target),
		missile ? "a missile" : "melee");
	// 0x2AE22: Slow of the attacker
	if (aw & DTU_FX_SLOW) {
		roll -= 4;
		dtu_xp_hit_mod(-4,LN("Slow"));
	}
	*ac_out = ac;
	if (thac0 - ac > roll) {
		dtu_xp_hit_end(roll,thac0 - ac,0);
		return 0;
	}
	// 0x2AE44: Stone Skin takes the hit
	if (tw & DTU_FX_STONESKIN) {
		target->dtu.m_80--;
		if (target->type == OBJ_PLAYER) {
			HUD_init_message("You are protected by StoneSkin Spell.");
			PALETTE_FLASH_ADD(0,0,16);
		}
		else
			HUD_init_message("%s protected by StoneSkin Spell.",dtu_object_display_name(target));
		dtu_obj_log("attack: the stone skin of obj %d %s takes the hit, %d left",(int)(target-Objects),
			dtu_object_name(target),target->dtu.m_80);
		dtu_xp_hit_end(roll,thac0 - ac,3);
		return 0;
	}
	dtu_xp_hit_end(roll,thac0 - ac,1);
	return 1;
}

// 0x2A7F3. The to-hit roll. Returns 2 for a critical hit, 1 for a hit, 0
// for a miss, -1 for a fumble.
static int to_hit_roll(object *target,object *attacker,int weapon,int *roll_out)
{
	int roll, thac0, ac;

	// 0x2A839: a sleeping target is hit, and its sleep ends (the timer
	// +0xE8 is 0)
	if (target->dtu.m_88 & DTU_FX_SLEEP) {
		target->dtu.m_e8[0] = 0;
		*roll_out = 0;
		dtu_xp_roll_clear();		// ASCENT: no roll (dtuxp.h)
		return 1;
	}
	// The roll of the player (0x2AAB5): the character code
	if (attacker && attacker->type == OBJ_PLAYER) {
		*roll_out = 0;
		return dtu_player_attack_roll(target,weapon);
	}

	roll = rand() % 20 + 1;
	*roll_out = roll;
	dtu_xp_hit_start(roll);		// ASCENT: the verbose combat log (dtuxp.h)
	if (roll == 20) {
		dtu_xp_hit_end(roll,0,2);
		return 2;
	}
	if (roll == 1) {
		dtu_xp_hit_end(roll,0,-1);
		// 0x2A9EB: a creature that fumbles flinches when a d20 is above its
		// str (Robot_info +0xDE)
		if (attacker && attacker->type == OBJ_ROBOT && attacker->id < N_robot_types &&
			 rand() % 20 + 1 > Monster_info[attacker->id].str && attacker->ctype.ai_info.CURRENT_STATE != DAIS_DEAD) {
			attacker->ctype.ai_info.CURRENT_STATE = DAIS_FLIN;
			Ail[attacker-Objects].anim_start = GameTime + FrameTime;
			dtu_obj_log("attack: obj %d %s fumbles and flinches",(int)(attacker-Objects),dtu_object_name(attacker));
		}
		return -1;
	}

	if (attacker == NULL || attacker->type != OBJ_ROBOT || attacker->id >= N_robot_types) {
		// The missile of a trap is its own parent (0x934A8). What the
		// executable reads as the THAC0 of a weapon object is not known. 20
		// is the value of an attacker of the lowest level. (guess)
		thac0 = 20;
		roll += weapon_int(weapon,"to_hit_bonus",0);
		dtu_xp_hit_mod(weapon_int(weapon,"to_hit_bonus",0),LN("weapon"));
	}
	else {
		int b;

		thac0 = Monster_info[attacker->id].thaco;
		// the bonus of the weapon (Weapon_info + 0x36), or of the creature
		// for a natural weapon
		if (weapon >= 0 && weapon < N_weapon_types && Weapon_item_info[weapon].record >= 0)
			b = tbl_int(Weapon_item_info[weapon].record,"to_hit_bonus",0);
		else
			b = Monster_ext[attacker->id].to_hit_bonus;
		roll += b;
		dtu_xp_hit_mod(b,LN("weapon"));
		roll += str_bonus(Monster_info[attacker->id].str);
		dtu_xp_hit_mod(str_bonus(Monster_info[attacker->id].str),LN("strength"));
		// 0x2ACBA: Strength of the creature
		if (attacker->dtu.m_88 & DTU_FX_STRENGTH) {
			roll++;
			dtu_xp_hit_mod(1,LN("Strength spell"));
		}
	}

	*roll_out = roll;
	return dtu_hit_test_vs(target,attacker,roll,thac0,&ac,Hit_by_missile);
}

int dtu_hit_by_missile(void)
{
	return Hit_by_missile;
}

// The weapon of a melee attack of an object: the weapon_type of the
// monster table, or -1 for a natural weapon.
static int attack_weapon(object *attacker)
{
	if (attacker && attacker->type == OBJ_ROBOT)
		return Monster_ext[attacker->id].weapon_type;
	return -1;
}

// The fire part of 0x2AF5E and of 0x91BC3: 0 for a monster with
// fire_immune, one half with fire_resistant, one half with the effect
// Resist Fire (bit 10), and the effect Protection From Fire (bit 9) takes
// it from its counter (+0x82).
static int fire_part(object *target,int fire)
{
	if (target->type == OBJ_ROBOT && target->id < N_robot_types) {
		monster_info *mi = &Monster_info[target->id];

		if (mi->flags & MONSTER_F_FIRE_IMMUNE)
			fire = 0;
		else if (mi->flags & MONSTER_F_FIRE_RESISTANT)
			fire /= 2;
	}
	if (target->dtu.m_88 & DTU_FX_RESIST_FIRE)
		fire /= 2;
	if (target->dtu.m_88 & DTU_FX_PROT_FIRE) {
		target->dtu.m_82 -= fire;
		fire = (target->dtu.m_82 < 0) ? -target->dtu.m_82 : 0;
		if (target->dtu.m_82 < 0)
			target->dtu.m_82 = 0;
	}
	return fire;
}

// 0xAE3BB: the level of a creature, max hit points / 8, 1 to 18
static int creature_level(object *obj)
{
	int l = (obj->type == OBJ_ROBOT && obj->id < N_robot_types) ? Monster_info[obj->id].max_hit_points / 8 : 1;

	return l < 1 ? 1 : l > 18 ? 18 : l;
}

// 0x2B1F2 to 0x2B456 of 0x2AF5E, for every attacker: the weapons Flame
// Tongue (71) and Mace of Disruption (70), then the fire dice of the
// weapon (+0x0F sides, +0x10 number) with the resistances of the target.
// *damage is the normal part (the dice and the bonus); the function can
// change it. Returns the fire part.
int dtu_weapon_extra_damage(object *attacker,object *target,int weapon,int *damage)
{
	int fire = 0, fn, fs, i, undead;

	if (weapon < 0 || weapon >= N_weapon_types || Weapon_item_info[weapon].record < 0 || target == NULL)
		return 0;
	undead = target->type == OBJ_ROBOT && target->id < N_robot_types && (Monster_info[target->id].flags & MONSTER_F_UNDEAD);
	if (weapon == 71) {
		// 0x2B1FA: Flame Tongue: 2 fire against a regenerating target, 4
		// against an undead one; the fire condition of the creature spells
		if (target->dtu.m_88 & DTU_FX_REGENERATE)
			fire += 2;
		if (undead)
			fire += 4;
		if (target->type == OBJ_ROBOT)
			dtu_spell_creature_event(target,DTU_CSC_FIRE);
	}
	else if (weapon == 70 && undead && attacker) {
		// 0x2B244: Mace of Disruption against an undead creature: double
		// damage when a d100 is at most 5 * (level - 4); else the creature
		// is destroyed: its hit points are the damage, it has no effects
		// left, and it flies along the up and forward vectors of the
		// attacker with the strength of the player times 4.0
		int L = creature_level(target);

		if (5 * (L - 4) >= rand() % 100 + 1)
			*damage *= 2;
		else {
			vms_vector v;

			vm_vec_add(&v,&attacker->orient.uvec,&attacker->orient.fvec);
			vm_vec_copy_scale(&target->mtype.phys_info.velocity,&v,(int)Dtu_char.pa.strength << 18);
			*damage = f2i(target->shields);
			target->shields = 0;
			target->dtu.m_88 = 0;
			dtu_obj_log("attack: the Mace of Disruption destroys obj %d %s",(int)(target-Objects),dtu_object_name(target));
		}
	}
	// 0x2B327: the fire dice; the resistances only apply to a weapon with
	// fire dice
	fs = weapon_int(weapon,"fire_damage_dice",0);
	fn = weapon_int(weapon,"#fire_damage_dice",0);
	if (fs > 0 && fn > 0) {
		for (i=0;i<fn;i++)
			fire += rand() % fs + 1;
		if (target->type == OBJ_ROBOT)
			dtu_spell_creature_event(target,DTU_CSC_FIRE);
		fire = fire_part(target,fire);
	}
	// 0x2B5D4: fire damage ends the regeneration (bit 24) of the target
	if (fire > 0)
		target->dtu.m_88 &= ~DTU_FX_REGENERATE;
	return fire;
}

// 0x2B58D: the creature type with rat= does 1 damage (normal: the part
// without the fire)
static int rat_damage(object *attacker,int normal,int damage)
{
	if (normal > 0 && attacker && attacker->type == OBJ_ROBOT && dtu_rat_type(attacker->id))
		return 1;
	return damage;
}

// ASCENT: the monster type with the word "rat" (bit 4 of +0xD9 of the
// monster table, dtuobj.h)
int dtu_rat_type(int id)
{
	// the answer for each type, with the table record it was read from
	static int rec[MAX_ROBOT_TYPES], rat[MAX_ROBOT_TYPES], init = 0;
	char value[16];
	int r;

	if (id < 0 || id >= N_robot_types || id >= MAX_ROBOT_TYPES || (r = Monster_info[id].record) < 0)
		return 0;
	if (!init) {
		memset(rec,-1,sizeof(rec));
		init = 1;
	}
	if (rec[id] != r) {
		rec[id] = r;
		rat[id] = bm_tbl_get(r,"rat",value,sizeof(value)) ? 1 : 0;
	}
	return rat[id];
}

// ASCENT: object flag 0x40 on a creature (dtuobj.h)
int dtu_creature_frozen(object *obj)
{
	if (obj->type != OBJ_ROBOT || !(obj->flags & OF_DTU_NOHIT) || obj->id >= N_robot_types)
		return 0;
	// 0x13C96 and 0x9F17E: +0x7E (regeneration) of 1 or more; 0x13C9D and
	// 0x9F18A: the rat type
	return Monster_ext[obj->id].regeneration < 1 && !dtu_rat_type(obj->id);
}

// 0x2AF5E. Returns the damage as an integer. A weapon of 0 or more gives
// the dice of the weapon table (+0x0D sides, +0x0E number, +0x38 bonus),
// the weapon cases and the fire dice. Else the dice of the creature. The
// player: the character code (dtu_char_weapon_damage).
int damage_dice(object *attacker,object *target,int weapon)
{
	int n, sides, bonus, i, sum = 0, fire = 0;

	if (attacker && attacker->type == OBJ_PLAYER)
		return dtu_player_weapon_damage(weapon,target);
	if (weapon >= 0 && weapon < N_weapon_types && Weapon_item_info[weapon].record >= 0) {
		n = weapon_int(weapon,"#damage_dice",1);
		sides = weapon_int(weapon,"damage_dice",6);
		bonus = weapon_int(weapon,"damage_bonus",0);
	}
	else if (attacker && attacker->type == OBJ_ROBOT) {
		n = Monster_ext[attacker->id].n_damage_dice;
		sides = Monster_ext[attacker->id].damage_dice;
		bonus = Monster_ext[attacker->id].damage_bonus;
		// 0x2B053: Strength of the creature
		if (attacker->dtu.m_88 & DTU_FX_STRENGTH)
			bonus++;
	}
	else
		return 0;
	// The executable stops with "RF.damage_dice > 0".
	if (sides < 1) sides = 1;
	if (n < 1) n = 1;
	for (i=0;i<n;i++)
		sum += rand() % sides + 1;
	dtu_xp_dmg_start(n,sides,sum);		// ASCENT: the verbose combat log (dtuxp.h)
	// ASCENT: Dtu_fix_creature_missile 0: the launcher and the Potion of
	// Luck of the player (0x2B0AE, 0x2B107)
	if (weapon >= 0 && !Dtu_fix_creature_missile) {
		int s0 = sum;

		sum = dtu_char_creature_missile_damage(weapon,n,sides,sum);
		dtu_xp_dmg_mod(sum - s0,LN("launcher"));
	}
	sum += bonus;
	dtu_xp_dmg_mod(bonus,NULL);
	if (weapon >= 0) {
		int s0 = sum;

		fire = dtu_weapon_extra_damage(attacker,target,weapon,&sum);
		dtu_xp_dmg_mod(sum - s0,NULL);
		dtu_xp_dmg_mod(fire,LN("fire"));
	}
	// 0x2B50C: the player is hurt: hard_sound when the damage as a fix is
	// above the maximum hit points / 16 as an integer (nearly always), else
	// hit_sound, at the player, volume 1/3, at most every 0.25 s (0x25C50)
	if (target && target->type == OBJ_PLAYER) {
		static fix next_hurt_sound = 0;

		if (next_hurt_sound > GameTime + F1_0)
			next_hurt_sound = 0;
		if (next_hurt_sound < GameTime) {
			next_hurt_sound = GameTime + F1_0/4;
			sound_at(player_type_sound(i2f(sum) > f2i(dtu_player_max_hit_points()) / 16 ? "hard_sound" : "hit_sound"),
				target->segnum,&target->pos,0x5555);
		}
	}
	{
		int d = rat_damage(attacker,sum,sum + fire);

		dtu_xp_dmg_mod(d - (sum + fire),LN("rat"));
		dtu_xp_dmg_end(d);
		return d;
	}
}

//------------------------------------------------------------------------
// The values of the player character: the calls of the fight code go
// through these functions to the character code (dtuchar.c).

DTU_WEAK int dtu_player_thac0(void)
{
	int t = dtu_char_thac0();

	return (t > 0) ? t : 20;
}

DTU_WEAK int dtu_player_armor_class(void)
{
	return dtu_char_armor_class();
}

DTU_WEAK int dtu_player_attack_roll(object *target, int weapon)
{
	return dtu_char_attack_roll(target, weapon);
}

DTU_WEAK int dtu_player_weapon_damage(int weapon, object *target)
{
	return dtu_char_weapon_damage(weapon, target);
}

DTU_WEAK int dtu_player_level(void)
{
	return 1;
}

DTU_WEAK fix dtu_player_hit_points(void)
{
	return Players[Player_num].shields;
}

DTU_WEAK fix dtu_player_max_hit_points(void)
{
	int m = dtu_char_max_hit_points();

	return (m > 0) ? i2f(m) : Player_max_hp;
}

DTU_WEAK fix dtu_player_damage(fix amount)
{
	fix hp = Players[Player_num].shields - amount;

	if (amount < 0 && hp > dtu_player_max_hit_points())
		hp = dtu_player_max_hit_points();
	if (hp < 0)
		hp = 0;
	Players[Player_num].shields = hp;
	return hp;
}

DTU_WEAK void dtu_player_award_xp(int amount)
{
	// 0xAF3CF adds the points to the experience of the character
	// (0x1D3393) and tests the level.
	dtu_char_award_xp(amount);
	dtu_obj_log("player: %d experience points",amount);
}

DTU_WEAK int dtu_hook_player_death_screen(void)
{
	return 0;
}

// ASCENT: 0x6A374, the death of Lolth in FLAME. Object 3 (the Flamesword)
// loses the flag 0x40 of the level fixes and +0x1AE bit 2, each creature
// gets control type 0 (it stops), and 0xAC055(object 3, 0) runs script 322
// of the sword: "YOU HAVE DESTROYED LOLTH'S AVATAR", the move to segment
// 482 and the end sequence. No use action is needed.
static void flame_lolth_dead(void)
{
	int i;

	if (Highest_object_index < 3)
		return;
	Objects[3].flags &= ~OF_DTU_NOHIT;
	for (i=1;i<=Highest_object_index;i++)
		if (Objects[i].type == OBJ_ROBOT)
			Objects[i].control_type = CT_NONE;
	dtu_obj_log("flame: Lolth is dead (0x6A374): obj 3 can be touched, the creatures stop");
	dtu_hook_trigger_object(3,0);
}

void creature_is_corpse(object *obj)
{
	polymodel *po = obj_model(obj);
	int frame = DTU_ANIM_FRAME(obj);
	monster_info *mi = &Monster_info[obj->id];

	// 0x6A48B to 0x6A510: the body is put on the floor ("RLH: pushing (%d)
	// into floor by %.2f"). The executable takes d from 0x6C30E (the
	// distance from the bottom of the torso box of the frame to the floor,
	// a negative d is in the floor) and moves the body up by -d, or down by
	// d plus torso_radius[0] of the model (the first value on the stack
	// that 0x748D9 fills). 0x6C30E finds no floor below a body high in the
	// air (a dead flyer), and the body stays there. The port uses the half
	// height of the box of the body of the last frame (guess for the
	// height). Dtu_fix_corpse_floor: corpse_on_floor().
	if (Dtu_fix_corpse_floor && corpse_on_floor(obj))
		;
	else if (po && po->body_half_height && frame >= 0 && frame < po->n_frames) {
		fix d = creature_floor_dist(obj,0,po->body_half_height[frame+1]);

		if (d != DTU_FLOOR_PROBE)
			obj->pos.y -= d;
	}

	// 0x6A639: a HUMANOID leaves gold in 4 of 10 cases (clock % 10 >= 6,
	// here a random number): 1 to moola / 4 pieces, a misc object with
	// the id 1 and the amount as its hit points.
	// Dtu_fix_revive: one time only for a creature that gets up again
	if ((mi->flags & MONSTER_F_HUMANOID) && (mi->moola >> 2) > 0 && !Ail[obj-Objects].gold_done) {
		int r = Dtu_fix_ai_ticks ? (rand() % 10) : (int)((uint)Dtu_clock % 10);

		if (r >= 6) {
			int amount = rand() % (mi->moola >> 2) + 1;
			int n = dtu_create_item(OBJ_MISC,1,1,obj->segnum,&obj->pos);

			if (n >= 0) {
				Objects[n].shields = i2f(amount);
				dtu_obj_log("drop: obj %d %s leaves %d gold (obj %d)",(int)(obj-Objects),dtu_object_name(obj),amount,n);
			}
		}
	}

	// 0x6A45A: the alert function 0x160AB(body, 4)
	dtu_creature_alert(obj,4);

	// MONSTER_F_EXTRAPLANAR: the body is not drawn (0x6A5A0)
	if (mi->flags & MONSTER_F_EXTRAPLANAR)
		obj->render_type = RT_NONE;

	obj->shields = 0;
	obj->flags |= OF_DESTROYED;
	obj->control_type = CT_NONE;
	obj->movement_type = MT_NONE;
	Ail[obj-Objects].gold_done = 1;
	// ASCENT: regen_hp starts at the death (creature_dies), so that the hits
	// on the dying body count (Dtu_fix_revive)
	Ail[obj-Objects].regen_time = GameTime;
	dtu_obj_log("obj %d %s: is a corpse",(int)(obj-Objects),dtu_object_name(obj));
	if (Obj_log)
		log_corpse_height(obj,"corpse");

	// ASCENT: 0x6A579 to 0x6A59B: in level 0x20 (flame) the death of
	// object 36 (the avatar of Lolth) calls 0x6A374
	if (Dtu_script_level == 32 && obj-Objects == 36)
		flame_lolth_dead();
}

// The death of a creature in the damage function (0x2A091 to 0x2A245).
static void creature_dies(object *obj,object *attacker,int weapon)
{
	dtu_ai_local *ail = &Ail[obj-Objects];
	ai_static *aip = &obj->ctype.ai_info;
	monster_info *mi = &Monster_info[obj->id];
	// ASCENT: Dtu_fix_revive: the creature falls and gets up again (the
	// regeneration bit stays, see below). No experience and no kill now:
	// both come when the fallen body is destroyed (creature_destroyed).
	// Only a troll gets up: mirt and Halaster also regenerate hit points
	// (their table lines and setup), and a dead one must stay dead.
	int falls = Dtu_fix_revive && Monster_ext[obj->id].regeneration > 0 && (obj->dtu.m_88 & 0x01000000)
		&& strnicmp(mi->name, "troll", 5) == 0;

	if (falls) {
		if (attacker && attacker->type == OBJ_PLAYER)
			HUD_init_message("%s falls, but it is not dead.",dtu_object_display_name(obj));
		dtulog_event("Creature falls: obj %d %s, attacker %s, segment %d (Dtu_fix_revive)",(int)(obj-Objects),
			dtu_object_name(obj),attacker ? dtu_object_name(attacker) : "none",obj->segnum);	// ASCENT: the session log
		dtu_xp_cause(NULL);
	}
	else {
	// ASCENT: the kill line, the line of a death with no experience and the
	// session log line (dtuxp.c). xp -1: the player is not the attacker.
	int xp = -1;

	if (attacker && attacker->type == OBJ_PLAYER)
		xp = (obj->dtu.m_88 & 0x01000000) ? 0 : Robot_info[obj->id].score_value;
	dtu_xp_creature_death(obj,attacker,weapon,xp);
	if (xp > 0) {
		// 0x4EA50 with the score value. Not for a creature that
		// regenerates (status bit 0x01000000).
		dtu_player_award_xp(Robot_info[obj->id].score_value);
		dtu_obj_log("xp: obj %d %s gives %d experience",(int)(obj-Objects),dtu_object_name(obj),Robot_info[obj->id].score_value);
	}
	// the words at 0x1D33AB and 0x1D33AD count each death
	Players[Player_num].num_kills_level++;
	Players[Player_num].num_kills_total++;
	}

	// 0x2A157: the die sound, if the number is not 0
	if (mi->die_sound > 0)
		sound_at(mi->die_sound,obj->segnum,&obj->pos,F1_0);

	aip->CURRENT_STATE = aip->GOAL_STATE = DAIS_DEAD;
	DTU_ANIM_GOAL(obj) = DAS_DIE;
	set_anim(obj,DAS_DIE);
	ail->flags = 0;
	ail->anim_start = GameTime;
	ail->mode = DAIM_STILL;
	ail->activity = 2;
	ail->path_len = 0;
	// 0x2A1D0, 0x916D9: all effects end. This clears the regeneration bit
	// too, so the code of do_ai_frame that lets a dead creature get up
	// (0x1440C) never runs for a creature that was killed, and the kill gave
	// no experience (the test above). Dtu_fix_revive: the bit stays unless
	// fire or acid took it, and the corpse regenerates (corpse_regen_frame).
	if (falls) {
		obj->dtu.m_88 = 0x01000000;
		ail->regen_hp = 0;
		dtu_obj_log("obj %d %s: keeps its regeneration (Dtu_fix_revive)",(int)(obj-Objects),dtu_object_name(obj));
	}
	else
		obj->dtu.m_88 = 0;
	obj->shields = 0;
	vm_vec_zero(&obj->mtype.phys_info.velocity);

	dtu_obj_log("obj %d %s: dies",(int)(obj-Objects),dtu_object_name(obj));

	dtu_creature_drop_contents(obj);

	log_creature_state(obj,"death");
}

// object_create_egg (0x3C4BD): what the object of the level file
// contains. One object with the count in its byte 0x1D5. The defaults
// of the monster table are not used. The weapons 10, 28, 40, 74 and 75
// are not dropped.
void dtu_creature_drop_contents(object *obj)
{
	if (obj->contains_count > 0) {
		int type = obj->contains_type, id = obj->contains_id, count = obj->contains_count, n = -1;

		if (type == OBJ_ROBOT)
			n = dtu_gate_in_creature(id,obj->segnum);
		else if (!(type == OBJ_WEAPON && (id == 10 || id == 0x1c || id == 0x28 || id == 0x4a || id == 0x4b)))
			n = dtu_create_item(type,id,count,obj->segnum,&obj->pos);
		dtu_obj_log("drop: obj %d %s leaves type %d id %d count %d (obj %d)",(int)(obj-Objects),dtu_object_name(obj),type,id,count,n);
		obj->contains_type = obj->contains_id = -1;
		obj->contains_count = 0;
	}
}

// 0xA9F58: the creature leaves the level (Abjure): no control, no
// movement, no picture, the flags 0xC0. The port takes the object away.
void dtu_creature_vanish(object *obj)
{
	dtu_obj_log("obj %d %s: leaves the level (0xA9F58)",(int)(obj-Objects),dtu_object_name(obj));
	obj->control_type = CT_NONE;
	obj->movement_type = MT_NONE;
	obj->render_type = RT_NONE;
	obj->flags |= OF_SHOULD_BE_DEAD;
}

int dtu_creature_is_active(object *obj)
{
	return obj && obj->type == OBJ_ROBOT && Ail[obj-Objects].activity == 2;
}

// ASCENT: Dtu_fix_revive, the fallen body of a creature that can get up.
// 1.3 offers no way to end the revive but fire or acid before the death:
// the code of 0x1440C tests only the regeneration bit, the hit points and
// the distance of the player (no damage type, no count, no time limit, no
// hit on the body). The Monstrous Compendium (Troll): "Only fire and acid
// cause permanent damage", and a troll at 0 hit points or less that is
// burned or put in acid is killed for good; a troll that is cut apart
// regrows only from its largest piece. The port: fire or acid on the
// fallen body destroys it; other damage on the fallen body takes away the
// hit points that it regrew, and damage equal to its full hit points cuts
// it apart for good (the game has no oil or torch for a fighter or a thief,
// see docs/audit/monsters.md).
int dtu_creature_down(object *obj)
{
	if (!Dtu_fix_revive || obj == NULL || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return 0;
	if (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD))
		return 0;
	return Monster_ext[obj->id].regeneration > 0 && Monster_info[obj->id].max_hit_points > 0
		&& (obj->dtu.m_88 & 0x01000000) && obj->ctype.ai_info.CURRENT_STATE == DAIS_DEAD;
}

// The fallen body is destroyed: the experience and the kill that the fall
// did not give (creature_dies), one time.
static void creature_destroyed(object *obj,object *attacker,char *how)
{
	obj->dtu.m_88 &= ~0x01000000;
	Ail[obj-Objects].regen_hp = 0;
	dtu_obj_log("obj %d %s: the fallen body is destroyed (%s, Dtu_fix_revive)",(int)(obj-Objects),dtu_object_name(obj),how);
	// ASCENT: the kill line and the session log line (dtuxp.c)
	dtu_xp_cause(how);
	dtu_xp_creature_death(obj,attacker,-1,attacker && attacker->type == OBJ_PLAYER ? Robot_info[obj->id].score_value : -1);
	if (attacker && attacker->type == OBJ_PLAYER) {
		dtu_player_award_xp(Robot_info[obj->id].score_value);
		dtu_obj_log("xp: obj %d %s gives %d experience",(int)(obj-Objects),dtu_object_name(obj),Robot_info[obj->id].score_value);
	}
	Players[Player_num].num_kills_level++;
	Players[Player_num].num_kills_total++;
}

void dtu_creature_down_hit(object *obj, object *attacker, int damage, int burn)
{
	dtu_ai_local *ail;
	int max;

	if (!dtu_creature_down(obj))
		return;
	ail = &Ail[obj-Objects];
	max = Monster_info[obj->id].max_hit_points;
	if (burn) {
		creature_destroyed(obj,attacker,"fire or acid");
		return;
	}
	if (damage <= 0)
		return;
	ail->regen_hp -= i2f(damage);
	dtu_obj_log("obj %d %s: the fallen body takes %d, regrown hp %d (cut apart at %d)",(int)(obj-Objects),dtu_object_name(obj),
		damage,f2i(ail->regen_hp),-max);
	if (ail->regen_hp <= -i2f(max))
		creature_destroyed(obj,attacker,"cut apart");
}

void dtu_creature_down_spell(object *obj, object *caster, int spell, int level)
{
	int damage, burn;

	if (!dtu_creature_down(obj) || spell < 0 || spell >= N_spell_types)
		return;
	// 0x91BC3 clears the bit for fire or acid damage
	damage = dtu_spell_damage_roll(spell,obj,level);
	burn = !(obj->dtu.m_88 & 0x01000000);
	obj->dtu.m_88 |= 0x01000000;
	dtu_obj_log("spell: spell %d '%s' level %d on the fallen body of obj %d %s: damage %d%s",spell,Spell_info[spell].name,level,
		(int)(obj-Objects),dtu_object_name(obj),damage,burn ? ", fire or acid" : "");
	dtu_creature_down_hit(obj,caster,damage,burn);
}

void dtu_creature_down_swing(object *obj, int weapon)
{
	int damage, burn;

	if (!dtu_creature_down(obj))
		return;
	// the damage roll of the weapon (dtu_weapon_extra_damage clears the bit
	// for fire, 0x2B5D4)
	damage = dtu_char_weapon_damage(weapon,obj);
	burn = !(obj->dtu.m_88 & 0x01000000);
	obj->dtu.m_88 |= 0x01000000;
	dtu_obj_log("combat: the player hits the fallen body of obj %d %s: damage %d%s",(int)(obj-Objects),dtu_object_name(obj),damage,
		burn ? ", fire" : "");
	if (damage > 0)
		HUD_init_message("You hit %s for %d damage.",dtu_object_display_name(obj),damage);
	dtu_creature_down_hit(obj,ConsoleObject,damage,burn);
}

fix dtu_creature_saved_shields(object *obj)
{
	if (!dtu_creature_down(obj))
		return obj->shields;
	return Ail[obj-Objects].regen_hp - i2f(Monster_info[obj->id].max_hit_points + 1);
}

// 0x748D9 for the moving object of a query (0xA05AD for a missile): a
// model with the body box (flag 1) gives body_radius[0] (0x800 for a
// weapon, type 5) and the half height (the torso of the frame with the
// torso box); other objects give the size twice.
void dtu_query_radii(object *obj, fix *rad_h, fix *rad_v)
{
	polymodel *po = (obj->render_type == RT_POLYOBJ) ? obj_model(obj) : NULL;

	if (po && po->dtu_block && (po->box_flags & DTU_BOX_BODY) && po->body_radius && po->body_half_height) {
		int frame = DTU_ANIM_FRAME(obj);

		*rad_h = (obj->type == OBJ_WEAPON) ? 0x800 : po->body_radius[0];
		*rad_v = po->body_half_height[0];
		if ((po->box_flags & DTU_BOX_TORSO) && po->torso_half_height && po->n_frames > 0) {
			if (frame < 0 || frame >= po->n_frames)
				frame = 0;
			*rad_v = po->torso_half_height[frame];
		}
		return;
	}
	*rad_h = *rad_v = obj->size;
}

// The weapon number of the damage functions for damage with no to-hit
// roll and no dice (spells, traps, dtu_damage_creature_direct).
#define DTU_DIRECT	(-2)

// One group of dice of a spell (0x91BC3): nothing when the sides and the
// count are 0, else "count" rolls of rand() % sides + 1, plus per_level *
// level. count < 0: the count of the table line; else the count that the
// case of 0x8CFFF writes into the table first (Fireball min(L, 10) at
// 0x8E592, Lightning Bolt 0x8EEAB, Melf's Acid Arrows 2 (L/3) + 2 at
// 0x8EFE0).
// A die of 0 sides adds 0, as in 1.3. The table text of these three
// spells has 0 sides, but the table fixup gives them 6, 6 and 4 sides
// (0x1A9C7, 0x1AAE9, dtutblfix.c), so only a line that the fixup does not
// write can have it: the idiv by 0 (0x91C25, 0x91C8E, 0x91CEC, 0x91D4E) goes to the
// divide error handler of div0.asm (div0_init(1) at 0xB8323, handler
// 0xF6D94, saturate mode). Its int3 at 0xF6E17 returns without a debugger
// and its decoder takes the SIB byte of `idiv dword ptr [esp]` (F7 3C 24)
// for a ModRM byte, so it resumes after the `inc edx` with EDX 0. Checked
// in DOSBox-X with the bytes of 0x91C25 run after div0_init: EDX 0, EBX,
// ECX, ESI, EDI and EBP kept. The part for each level is added after the
// loop.
// ASCENT: D-104. The cases of Fireball (0x8E592), Lightning Bolt
// (0x8EEAB) and Melf's Acid Arrows (0x8F00E) of 0x8CFFF write the die
// count of the cast into the spell table record before the roll; the record
// keeps it until the next table read. A roll that reads the count of the
// table (0x3D2BD, Fireball on an object in a lava segment, which the port
// does not have; in the port the fallen body of Dtu_fix_revive) then gets
// the count of the last cast of any caster. 1 (default): the count of the
// table line. -origcombat clears it (dtucombat.c).
int Dtu_fix_spell_dice = 1;
static int Spell_dice_written[MAX_SPELL_TYPES][4];		// the count + 1, 0: none

void dtu_spell_dice_clear(void)
{
	memset(Spell_dice_written,0,sizeof(Spell_dice_written));
}

static int spell_dice(int spell,int group,char *sides_key,char *count_key,char *level_key,int level,int count)
{
	int record = Spell_info[spell].record, sides = tbl_int(record,sides_key,0), sum = 0, i;

	if (count >= 0)
		Spell_dice_written[spell][group] = count + 1;
	else if (!Dtu_fix_spell_dice && Spell_dice_written[spell][group])
		count = Spell_dice_written[spell][group] - 1;		// Dtu_fix_spell_dice
	else
		count = tbl_int(record,count_key,0);
	if (sides == 0 && count == 0)
		return 0;
	if (sides > 0)
		for (i=0;i<count;i++)
			sum += rand() % sides + 1;
	// ASCENT: the verbose combat log (dtuxp.h)
	if (sides > 0 && count > 0)
		dtu_xp_dmg_start(count,sides,sum);
	dtu_xp_dmg_mod(tbl_int(record,level_key,0) * level,LN("level"));
	return sum + tbl_int(record,level_key,0) * level;
}

// 0x91BC3 (spells.c), verified: the damage of a spell on a target. Four
// groups of dice of the spell table: damage_dice, fire_damage_dice,
// acid_damage_dice and elec_damage_dice (Spell_info +0x3A, +0x3D, +0x43,
// +0x46: sides, number, plus for each level). cold_damage_dice (+0x40) is
// not used here. Fire: 0 for a monster with fire_immune, one half with
// fire_resistant, one half with the effect Resist Fire (bit 10), and the
// effect Protection From Fire (bit 9) takes it from its counter (+0x82).
// Electricity: 0 for a monster with electricity_immune. Fire or acid
// damage ends the regeneration (bit 24) of the target.
int dtu_spell_damage_roll(int spell, object *target, int level)
{
	return dtu_spell_damage_roll_count(spell,target,level,-1,0);
}

int dtu_spell_damage_roll_count(int spell, object *target, int level, int group, int count)
{
	int base, fire, acid, elec, raw;

	if (spell < 0 || spell >= N_spell_types || target == NULL)
		return 0;
	dtu_xp_roll_clear();		// ASCENT: the verbose combat log (dtuxp.h)
	base = spell_dice(spell,0,"damage_dice","#damage_dice","damage_level",level,group == 0 ? count : -1);
	fire = spell_dice(spell,1,"fire_damage_dice","#fire_damage_dice","fire_damage_level",level,group == 1 ? count : -1);
	acid = spell_dice(spell,2,"acid_damage_dice","#acid_damage_dice","acid_damage_level",level,group == 2 ? count : -1);
	elec = spell_dice(spell,3,"elec_damage_dice","#elec_damage_dice","elec_damage_level",level,group == 3 ? count : -1);
	raw = base + fire + acid + elec;

	if (target->type == OBJ_ROBOT && target->id < N_robot_types) {
		monster_info *mi = &Monster_info[target->id];

		if (mi->flags & MONSTER_F_ELECTRICITY_IMMUNE)
			elec = 0;
		if (mi->flags & MONSTER_F_FIRE_IMMUNE)
			fire = 0;
		else if (mi->flags & MONSTER_F_FIRE_RESISTANT)
			fire /= 2;
	}
	if (target->dtu.m_88 & (1 << 10))			// Resist Fire
		fire /= 2;
	if (target->dtu.m_88 & (1 << 9)) {			// Protection From Fire
		target->dtu.m_82 -= fire;
		fire = (target->dtu.m_82 < 0) ? -target->dtu.m_82 : 0;
	}
	if (fire || acid)
		target->dtu.m_88 &= ~0x01000000;
	dtu_xp_dmg_mod(base + fire + acid + elec - raw,LN("resistance"));		// ASCENT: the verbose combat log
	dtu_xp_dmg_end(base + fire + acid + elec);
	return base + fire + acid + elec;
}

// The level of the caster of a spell missile: the mage level of the
// monster table for a creature, the level of the character for the
// player, 1 for a trap.
static int caster_level(object *caster)
{
	if (caster && caster->type == OBJ_ROBOT && caster->id < N_robot_types && Monster_ext[caster->id].spell_level > 0)
		return Monster_ext[caster->id].spell_level;
	if (caster && caster->type == OBJ_PLAYER)
		return dtu_player_level();
	return 1;
}

fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon);
fix damage_player_w(object *player, object *attacker, fix damage, int weapon);

// spells.c 0x8CFFF and 0x90CEF: the effect of a spell on a target. The
// weak definition has the damage of the spell (0x91BC3, through the
// damage function with no to-hit roll) and none of the other effects
// (sleep, hold, fear, poison and the rest), which belong to the spell
// code (dtuspell.c can define this function). The saving throw of the
// executable (0xAE596, one half of the damage) is not read.
DTU_WEAK fix dtu_spell_hit(int spell, object *target, object *caster, int level)
{
	fix damage;

	if (spell < 0 || spell >= N_spell_types || target == NULL)
		return 0;
	damage = i2f(dtu_spell_damage_roll(spell,target,level));
	dtu_obj_log("spell: spell %d '%s' level %d hits %s: damage %d (the other effects are not in the port)",spell,
		Spell_info[spell].name,level,dtu_object_name(target),f2i(damage));
	if (damage <= 0)
		return 0;
	if (target->type == OBJ_PLAYER)
		return damage_player_w(target,caster,damage,DTU_DIRECT);
	if (target->type == OBJ_ROBOT)
		return damage_creature_w(target,caster,damage,DTU_DIRECT);
	return 0;
}

fix spell_hit(int spell,object *target,object *attacker)
{
	return dtu_spell_hit(spell,target,attacker,caster_level(attacker));
}

// 0x29A9C for a creature as the target. damage is used if there is no
// roll. With an attacker or a weapon the function throws the dice.
// Returns the damage that was done.
fix damage_creature_w(object *creature, object *attacker, fix damage, int weapon)
{
	dtu_ai_local *ail = &Ail[creature-Objects];
	ai_static *aip = &creature->ctype.ai_info;

	if (creature->type != OBJ_ROBOT || creature->shields < F1_0 || (creature->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return 0;

	if (weapon != DTU_DIRECT && (attacker || weapon >= 0)) {
		int roll, hit = to_hit_roll(creature,attacker,weapon,&roll);

		if (hit <= 0) {
			dtu_obj_log("attack: %s misses obj %d %s (roll %d, result %d)",attacker ? dtu_object_name(attacker) : "a missile",
				(int)(creature-Objects),dtu_object_name(creature),roll,hit);
			dtu_xp_roll_say(attacker,creature);		// ASCENT: the verbose combat log (dtuxp.h)
			return 0;
		}
		// 0x29D9D: a creature with weapon_resistant takes no damage from
		// a natural weapon or from a weapon that is not magic.
		if ((Monster_info[creature->id].flags & MONSTER_F_WEAPON_RESISTANT) &&
			 (weapon < 0 || !bm_tbl_has_flag(Weapon_item_info[weapon].record,"magic"))) {
			dtu_obj_log("attack: obj %d %s is weapon resistant",(int)(creature-Objects),dtu_object_name(creature));
			return 0;
		}
		damage = i2f(damage_dice(attacker,creature,weapon));
		if (hit == 2)
			damage += i2f(damage_dice(attacker,creature,weapon));
		dtu_xp_dmg_end(f2i(damage));
	}

	if (damage <= 0)
		return 0;

	creature->shields -= damage;
	// 0x29F3E: a weapon hit lowers the Armor pool too
	if (weapon != DTU_DIRECT)
		dtu_spell_armor_hit(creature,damage);

	dtu_obj_log("damage: obj %d %s takes %d from %s, hp %d",(int)(creature-Objects),dtu_object_name(creature),
		f2i(damage),attacker ? dtu_object_name(attacker) : "nothing",f2i(creature->shields));

	// 0x29FB8: the creature flinches
	if (aip->CURRENT_STATE != DAIS_DEAD) {
		aip->CURRENT_STATE = DAIS_FLIN;
		ail->anim_start = GameTime + FrameTime;
	}
	if (attacker && attacker->type == OBJ_PLAYER) {
		HUD_init_message("You hit %s for %d damage.",dtu_object_display_name(creature),f2i(damage));		// 0x2A05F: TEX 831 with TEX 70
		// A creature that is hit knows where the player is, and a creature
		// that was not hostile is hostile now. (guess: the executable does
		// this in the script code 0xA94AD and in the alert function)
		creature->dtu.m_1b0 |= DTU_HOSTILE;
		creature->dtu.m_1b0 &= ~(DTU_PEACEFUL|DTU_FLEEING);
		ail->target = 0;
		if (ail->awareness < 4) ail->awareness = 4;
		ail->awareness_time = i2f(3);
		ail->morale = Monster_info[creature->id].ai_values[1][Difficulty_level];
	}

	if (creature->shields < F1_0)
		creature_dies(creature,attacker,weapon);
	if (weapon != DTU_DIRECT)
		dtu_xp_roll_say(attacker,creature);		// ASCENT: the verbose combat log (dtuxp.h)
	return damage;
}

void dtu_damage_creature(object *creature, object *attacker, fix damage)
{
	damage_creature_w(creature,attacker,damage,attack_weapon(attacker));
}

fix dtu_damage_creature_direct(object *creature, object *attacker, fix damage)
{
	return damage_creature_w(creature,attacker,damage,DTU_DIRECT);
}

// The death of the player in the damage function (0x2A4D0 and after): the
// sound land_and_die_sound of the player type, the hit points and the
// status flags become 0, and the object is taken out of the game
// (0x6A3CC). The executable has no death camera. The rest of the
// sequence is dtu_player_death_frame().
static void player_dies(object *player,object *attacker)
{
	sound_at(player_type_sound("land_and_die_sound"),player->segnum,&player->pos,F1_0);
	Players[Player_num].killer_objnum = attacker ? (int)(attacker-Objects) : -1;
	dtu_player_damage(dtu_player_hit_points());
	player->dtu.m_88 = 0;
	player->control_type = CT_NONE;
	player->movement_type = MT_NONE;
	vm_vec_zero(&player->mtype.phys_info.velocity);
	Dtu_player_dead = 1;
	Player_death_stage = 1;
	Player_death_time = GameTime;
	dtu_obj_log("player: dies, killed by %s",attacker ? dtu_object_name(attacker) : "nothing");
	dtulog_event("Player dies on level %d, killed by %s", Current_level_num, attacker ? dtu_object_name(attacker) : "nothing");	// ASCENT: the session log
}

// 0x33C5E: the status display finds the hit points of the player below
// 1.0 in the next frame and starts the death. This catches the damage
// that does not go through the damage function (the hit points of a trap,
// the code of other files).
void dtu_player_check_death(object *killer)
{
	if (!Dtu_objects_enabled || Dtu_player_dead || Player_is_dead || ConsoleObject == NULL || ConsoleObject->type != OBJ_PLAYER)
		return;
	if (dtu_player_hit_points() < F1_0)
		player_dies(ConsoleObject,killer);
}

// The game after the death of the player (game() 0x423F5): the streams
// stop, the game waits 3.0 seconds or for a key, the screen fades out, the
// death picture comes (febgnd.jrg record 161), the death music plays
// (0x4249D) and the death menu offers "Load Game" and "Abort Game". The
// port waits 3.0 seconds, fades out and plays the music. Then the front
// end takes over (dtu_hook_player_death_screen), or the level starts
// again after DTU_DEATH_RESTART_WAIT.
// ASCENT: 0x33C96 ends the game loop at the death (0x13D3F7), so in 1.3 the
// world stops at once: game() stops the streams (0xA48D0) and waits 3 s or
// for a key (0x4240B to 0x42452) with no game frame. The game loop calls
// this before the world moves: 1 while the wait runs (the world does not
// move), and a key ends the wait.
int dtu_player_death_wait(void)
{
	static int streams_stopped;

	if (!Dtu_objects_enabled || !Dtu_player_dead || Player_death_stage != 1) {
		streams_stopped = 0;
		return 0;
	}
	if (!streams_stopped) {
		streams_stopped = 1;
		dtumusic_stop();
		key_flush();
	} else if (key_inkey()) {
		Player_death_time = GameTime - DTU_DEATH_WAIT;
		dtulog_event("Player death: a key ends the wait");
	}
	return 1;
}

int dtu_player_death_frame(void)
{
	if (!Dtu_objects_enabled || !Dtu_player_dead || Player_death_stage == 0)
		return 0;

	if (GameTime < Player_death_time)
		Player_death_time = GameTime;

	if (Player_death_stage == 1) {
		if (GameTime - Player_death_time < DTU_DEATH_WAIT)
			return 0;
		dtu_obj_log("player: death screen: fade out, death music");
		digi_stop_all();
		gr_palette_fade_out(gr_palette,32,0);
		dtumusic_play(DTUMUSIC_DEATH,0,100);
		Player_death_time = GameTime;
		if (dtu_hook_player_death_screen()) {
			dtu_obj_log("player: the front end has the death screen");
			Player_death_stage = 3;
			return 0;
		}
		Player_death_stage = 2;
		return 0;
	}

	if (Player_death_stage == 2 && GameTime - Player_death_time >= DTU_DEATH_RESTART_WAIT) {
		int level = Current_level_num;

		Player_death_stage = 0;
		Dtu_player_dead = 0;
		init_player_stats_new_ship();
		dtu_player_damage(-dtu_player_max_hit_points());
		StartNewLevelSub(level,0);
		gr_palette_load(gr_palette);
		dtu_obj_log("player: the level starts again, hp %d",f2i(dtu_player_hit_points()));
		return 1;
	}
	return 0;
}

// 0x29A9C for the player as the target (0x2A24A). Without an attacker and
// without a weapon the damage is taken as it is: no roll, no armor. Returns
// the damage that was done.
fix damage_player_w(object *player, object *attacker, fix damage, int weapon)
{
	fix hp;

	if (player->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead || dtu_player_hit_points() < F1_0)
		return 0;

	if (weapon != DTU_DIRECT && ((attacker && attacker != player) || weapon >= 0)) {
		int roll, hit, invulnerable = 0;

		// 0x2A270: Invulnerability keeps off the attacks of a creature of a
		// level below 4 and the weapons that are not magic
		if ((player->dtu.m_88 & DTU_FX_INVULNERABLE) && attacker && attacker != player &&
			 (creature_level(attacker) < 4 || (weapon >= 0 && weapon < N_weapon_types && !bm_tbl_has_flag(Weapon_item_info[weapon].record,"magic"))))
			invulnerable = 1;
		// 0x2A29A: Stone Skin takes the attack of an attacker
		if ((player->dtu.m_88 & DTU_FX_STONESKIN) && attacker && attacker != player) {
			PALETTE_FLASH_ADD(245,126,10);
			HUD_init_message("You are protected by StoneSkin Spell.");
			player->dtu.m_80--;
			dtu_obj_log("attack: the stone skin of the player takes the attack of %s, %d left",dtu_object_name(attacker),player->dtu.m_80);
			return 0;
		}
		if (invulnerable) {
			PALETTE_FLASH_ADD(0,0,16);
			HUD_init_message("You are protected by Invulnerability.");
			dtu_obj_log("attack: the invulnerability of the player keeps off %s",dtu_object_name(attacker));
			return 0;
		}

		hit = to_hit_roll(player,attacker,weapon,&roll);
		if (hit <= 0) {
			dtu_obj_log("attack: %s misses the player (roll %d, result %d)",
				attacker ? dtu_object_name(attacker) : "a missile",roll,hit);
			dtu_xp_roll_say(attacker,player);		// ASCENT: the verbose combat log (dtuxp.h)
			// ASCENT: the miss sound of a melee attack (dtumiss.c)
			if (attacker && attacker->type == OBJ_ROBOT && (weapon < 0 || weapon_is_melee(weapon)))
				dtu_miss_sound_creature(attacker,weapon >= 0 && weapon < N_weapon_types ? weapon : -1);
			return 0;
		}
		// 0x2A45F: a hit stops the cast of the player (0x9305D)
		dtu_spell_interrupt(player,1);
		damage = i2f(damage_dice(attacker,player,weapon));
		if (hit == 2)
			damage += i2f(damage_dice(attacker,player,weapon));
		dtu_xp_dmg_end(f2i(damage));
		// 0x2A49B: Chill Touch of the attacker with a natural or a melee
		// weapon: 1d4 more unless the player saves (category 4)
		if (damage > 0 && attacker && (attacker->dtu.m_88 & DTU_FX_CHILL_TOUCH))
			dtu_char_save_cause(-1,"Chill Touch",dtu_object_name(attacker));		// ASCENT: the session log
		if (damage > 0 && attacker && (attacker->dtu.m_88 & DTU_FX_CHILL_TOUCH) &&
			 (weapon < 0 || weapon_is_melee(weapon)) && !dtu_spell_save(player,DTU_SAVE_SPELL)) {
			int c = rand() % 4 + 1;

			damage += i2f(c);
			dtu_xp_dmg_mod(c,LN("Chill Touch"));
			dtu_xp_dmg_end(f2i(damage));
		}
	}

	if (damage <= 0)
		return 0;

	if (attacker && attacker != player)
		HUD_init_message("%s hit You for %d damage.",dtu_object_display_name(attacker),f2i(damage));		// 0x2A54E: TEX 831 with TEX 70
	else
		HUD_init_message("You took %d damage.",f2i(damage));
	if (weapon != DTU_DIRECT)
		dtu_xp_roll_say(attacker,player);		// ASCENT: the verbose combat log (dtuxp.h)

	// 0x2A4FB: a weapon hit lowers the Armor pool too (guess: not the damage
	// with no attacker and no weapon)
	if (weapon != DTU_DIRECT && ((attacker && attacker != player) || weapon >= 0))
		dtu_spell_armor_hit(player,damage);

	// 0x2A3C0: red plus 4 for each point, green and blue minus one half
	hp = dtu_player_damage(damage);
	PALETTE_FLASH_ADD(f2i(damage)*4,-f2i(damage/2),-f2i(damage/2));

	dtu_obj_log("damage: the player takes %d from %s%s%s, hp %d",f2i(damage),
		attacker ? dtu_object_name(attacker) : "nothing",weapon >= 0 ? ", weapon " : "",
		weapon >= 0 ? Weapon_item_info[weapon].name : "",f2i(hp));

	// 0x2A637: the touch spell of the type of the attacker (+0x7C, set only
	// by the creature setup 0xAC7B2: 22, Hold Person, for the lich). The
	// damage function casts it on the player after each attack that did
	// damage, before the death test (0x2A680). 0x8CFFF(spell, player,
	// creature): the save and the duration are the ones of the spell. The
	// executable sets the counter Spell_info+0x49 of 22 and 52 to 1 first, so
	// that the cast is not refused. Spell damage does not come here (0x911AA).
	if (weapon != DTU_DIRECT && attacker && attacker != player && attacker->type == OBJ_ROBOT &&
		 attacker->id < N_robot_types && Monster_ext[attacker->id].touch_spell >= 0) {
		dtu_obj_log("attack: the touch of %s casts spell %d on the player",dtu_object_name(attacker),Monster_ext[attacker->id].touch_spell);
		// ASCENT: the counter of Hold Person is 1 (0x2A670)
		dtu_spell_touch_counter(Monster_ext[attacker->id].touch_spell);
		dtu_spell_cast_on(Monster_ext[attacker->id].touch_spell,player,attacker);
	}

	if (hp < F1_0)
		player_dies(player,attacker);
	return damage;
}

void dtu_damage_player(object *player, object *attacker, fix damage)
{
	damage_player_w(player,attacker,damage,attack_weapon(attacker));
}

fix dtu_damage_player_direct(object *player, object *attacker, fix damage)
{
	return damage_player_w(player,attacker,damage,DTU_DIRECT);
}

// 0x27D82. A melee weapon touched the target.
void melee_hit(object *target,object *attacker)
{
	int weapon = attack_weapon(attacker);
	fix dealt, hp;
	int sound = -1;

	if (target->flags & OF_EXPLODING)
		return;

	if (weapon >= 0 && weapon_int(weapon,"enemy_spell",-1) >= 0)
		spell_hit(weapon_int(weapon,"enemy_spell",-1),target,attacker);

	if (target->type == OBJ_PLAYER) {
		dealt = damage_player_w(target,attacker,0,weapon);
		hp = dtu_player_hit_points();
		if (attacker->type == OBJ_ROBOT) {
			// The creature is active after a hit (0x28B34)
			Ail[attacker-Objects].activity = 2;
		}
	}
	else if (target->type == OBJ_ROBOT) {
		dealt = damage_creature_w(target,attacker,0,weapon);
		hp = target->shields;
	}
	else
		return;

	// The push of 0.2 along the line of the attack (0x6B70D) is not in
	// the port.
	if (dealt > 0) {
		blood(target);
		if (weapon < 0) {
			// A natural weapon: the hit sound of the target. The die sound
			// of a creature is played by its death. The key of the hit
			// sound of a player type (0x1D4E2B) is a guess.
			if (target->type == OBJ_ROBOT)
				sound = (hp >= F1_0) ? Monster_info[target->id].hit_sound : -1;
			else
				sound = player_type_sound("hit_sound");
		}
		else {
			sound = weapon_int(weapon,"monster_hit_sound",-1);
			if (hp < F1_0 && weapon_int(weapon,"monster_kill_sound",-1) >= 0)
				sound = weapon_int(weapon,"monster_kill_sound",-1);
		}
		if (attacker->type == OBJ_PLAYER && target->type == OBJ_ROBOT)
			sound_on_object(sound,target,F1_0/2);		// 0x27C0E
		else
			sound_at(sound,target->segnum,&target->pos,F1_0);
	}
	else if (weapon >= 0) {
		vclip_at(target->segnum,&target->pos,Weapon_info[weapon].impact_size,weapon_int(weapon,"monster_miss_vclip",-1));
		sound_at(weapon_int(weapon,"monster_miss_sound",-1),target->segnum,&target->pos,F1_0/4);
	}
}

// 0x6CFF8 (physics.c of the executable). The test of the weapon in an
// attack state. The executable calls it from the movement of each
// creature (0x9F2B4) and of the player. The weapon sphere of the frame
// moves from its center away from the body by its radius. The first
// object that it touches is hit, one time for each swing.
static void weapon_sphere_test(object *obj)
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
			// 0x6D4EC: scenery that can break: its hit points are cut to
			// 20.0, then 0x29A9C with no attacker takes 100.0 and 0xA6ACB
			// breaks it. One hit breaks it.
			if (!dtu_dressing_unbreakable(other)) {
				if (other->shields > i2f(20))
					other->shields = i2f(20);
				other->shields -= i2f(100);
				dtu_break_dressing(other,obj,0);
			}
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

// ASCENT: Dtu_fix_attack_commit (dtucombat.h). The test of the start of a
// melee attack on the player (each frame of the attack state before frame
// 8, until it passes): a hostile melee creature with the AI, the
// player alive and not hidden from it, in the reach of the goal state
// choice (both radii plus 2.0, 0x1599F), in front of it (the facing dot
// 0.75 of 0x1396E) and with no wall between the two centers. At the hit
// (at_hit 1): the creature is not dying, not held, the player is not hidden,
// not farther than twice the reach and no wall is between them.
static int attack_commit_valid(object *obj,int at_hit)
{
	object *pl = ConsoleObject;
	vms_vector v;
	fix dist, reach;
	fvi_query fq;
	fvi_info hit;

	if (pl == NULL || pl->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead || !Dtu_creatures_attack)
		return 0;
	if (obj->control_type != CT_AI || !is_hostile(obj) || Robot_info[obj->id].attack_type != 1)
		return 0;
	if ((obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)) || obj->shields < F1_0 || DTU_ANIM_STATE(obj) == DAS_DIE)
		return 0;
	if ((obj->dtu.m_88 & DTU_OF_HELD) || dtu_thief_hidden_from(obj))
		return 0;
	vm_vec_sub(&v,&pl->pos,&obj->pos);
	dist = vm_vec_normalize(&v);
	reach = obj->size + pl->size + i2f(2);
	if (dist > (at_hit ? 2*reach : reach))
		return 0;
	if (!at_hit && vm_vec_dot(&v,&obj->orient.fvec) < 0xC000)
		return 0;
	fq.p0 = &obj->pos;
	fq.p1 = &pl->pos;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) != HIT_NONE)
		return 0;
	return 1;
}

// ASCENT: test (-chartest stepback=N.D): when the next attack of creature
// N becomes valid (attack_commit_valid, also with -origcombat), the player
// moves D units away from it (horizontally) at once.
static int Test_stepback_obj = -1, Test_stepback_dist = 0;

void dtu_obj_test_stepback(int objnum,int dist)
{
	Test_stepback_obj = objnum;
	Test_stepback_dist = dist;
}

static void test_stepback(object *obj)
{
	object *pl = ConsoleObject;
	vms_vector v, p;
	int seg;

	if (Test_stepback_obj != obj-Objects || pl == NULL)
		return;
	Test_stepback_obj = -1;
	vm_vec_sub(&v,&pl->pos,&obj->pos);
	v.y = 0;
	if (vm_vec_normalize(&v) < 0x100)
		return;
	vm_vec_scale_add(&p,&pl->pos,&v,i2f(Test_stepback_dist));
	seg = find_point_seg(&p,pl->segnum);
	if (seg < 0) {
		dtu_obj_log("test: stepback of %d from obj %d leaves the mine",Test_stepback_dist,(int)(obj-Objects));
		return;
	}
	pl->pos = p;
	obj_relink(pl-Objects,seg);
	dtu_obj_log("test: the player steps back %d from obj %d, distance now %.2f",Test_stepback_dist,(int)(obj-Objects),
		f2fl(vm_vec_dist(&pl->pos,&obj->pos)));
}

// 0x6CFF8 with Dtu_fix_attack_commit (dtucombat.h): a melee attack of a
// creature that starts on the player in reach is resolved when the sphere
// of the weapon misses the player because the player moved: at the first
// hit frame (8 or later) when the player is out of reach, or at the last
// two frames of the attack state. The roll against the armor class and the
// damage are those of the hit of the sphere (melee_hit, 0x27D82).
void weapon_test(object *obj)
{
	int objnum = obj-Objects, s, n, rel;
	dtu_ai_local *ail;

	weapon_sphere_test(obj);
	if (obj->type != OBJ_ROBOT || objnum < 0 || objnum >= MAX_OBJECTS)
		return;
	ail = &Ail[objnum];
	s = DTU_ANIM_STATE(obj);
	if (!(s == DAS_ATTACK || s == DAS_ATTACK2 || s == DAS_ATTACK3 || s == DAS_ATTACK4)) {
		ail->commit = 0;
		return;
	}
	n = Dtu_anim_first[s+1] - Dtu_anim_first[s];
	rel = DTU_ANIM_FRAME(obj) - Dtu_anim_first[s];
	// the test runs in each frame before the hit frames until it passes:
	// the creature may turn to the player while it winds up
	if (ail->commit != 2 && rel < 8) {
		ail->commit = attack_commit_valid(obj,0) ? 2 : 1;
		if (ail->commit == 2) {
			dtu_obj_log("attack: obj %d %s starts a melee attack on the player in reach, frame %d",objnum,dtu_object_name(obj),rel);
			test_stepback(obj);
		}
	}
	if (!Dtu_fix_attack_commit || ail->commit != 2 || ail->swing_done || rel < 8)
		return;
	{
		fix dist = vm_vec_dist(&obj->pos,&ConsoleObject->pos);
		fix reach = obj->size + ConsoleObject->size + i2f(2);

		if (dist <= reach && rel < n - 2)
			return;		// the sphere may still reach the player
		ail->commit = 1;
		if (!attack_commit_valid(obj,1)) {
			dtu_obj_log("attack: obj %d %s: the committed attack is lost, distance %.2f, reach %.2f",objnum,dtu_object_name(obj),
				f2fl(dist),f2fl(reach));
			return;
		}
		ail->swing_done = 1;
		dtu_obj_log("attack: obj %d %s swings at the player, frame %d (committed, distance %.2f, reach %.2f)",objnum,
			dtu_object_name(obj),DTU_ANIM_FRAME(obj),f2fl(dist),f2fl(reach));
		melee_hit(ConsoleObject,obj);
	}
}
