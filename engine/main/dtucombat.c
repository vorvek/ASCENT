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
#include "dtudisp.h"		// ASCENT: the color of a message
#include "dtuchar.h"
#include "dtutrig.h"
#include "kconfig.h"		// ASCENT: the key table
#include "dtucombat.h"
#include "dtuspell.h"
#include "dtuopts.h"
#include "dtuthief.h"
#include "text.h"
#include "key.h"
#include "dtuscroll.h"		// ASCENT: the scroll text screen skips the animation
#include "dtuatk.h"			// ASCENT: the attack timing rules
#include "dtuxp.h"			// ASCENT: the cause of a kill in the session log
#include "dtumiss.h"			// ASCENT: the miss sound

int Dtu_fix_one_hit = 1;
int Dtu_fix_haste_attack = 1;
int Dtu_fix_missile_rate = 1;
int Dtu_fix_rat_hit = 1;
int Dtu_fix_body_ray = 1;
int Dtu_fix_swing_pitch = 1;
int Dtu_fix_attack_commit = 1;

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
static fix Fire_next = 0, Fire_last = -0x7fffffff;	// 0x215D80, 0x215D78 (hand 1)
static fix Missile_last = -0x7fffffff;	// ASCENT: the time of the last shot (Dtu_fix_missile_rate)
static int Missile_passed = 0;			// ASCENT: the request has passed the wait of Dtu_fix_missile_rate
static fix Swing_started = 0;			// for the log: the game time of the start of the swing
static int Swing_touch = 0;				// ASCENT: the swing touched something (the miss sound, dtumiss.c)
static int Swing_air = 0;				// ASCENT: the miss sound of the swing was tried: 1, it plays: 2
static int Reset_pending = 0;			// ASCENT: a load (dtu_combat_reset_view)
static int Lift_model = -1;			// ASCENT: the model of Swing_lift (Dtu_improved_body)
static fix Swing_lift = 0;				// ASCENT: the lift of the arms in a swing (Dtu_improved_body)
static int Sword_model = -2;			// ASCENT: the model measured for Dtu_fix_sword_view (-2: none)

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

// The time of the attack state (state_times[6] of the model of the
// player, 0x441E9 to 0x44258): max(fire_wait, 1.0) for a melee weapon and
// the empty hand, two times that with the bit 0x08000000 (Slow).
// ASCENT: a missile weapon, a thrown weapon and a spell keep the time of
// the model, as in the executable (0x43A5A writes the time only for
// attack_type 1). The port took max(fire_wait, 1.0) for them too: 2.0 for
// an arrow, 3.0 for a bolt. Haste: Dtu_fix_haste_attack (dtuatk.c).
static int hand_is_melee(void)
{
	int w = hand_weapon();

	if (dtu_spell_in_hand())
		return 0;
	return w < 0 || weapon_int(w,"attack_type",1) == 1;
}

// ASCENT: Dtu_fix_speed_items. 1 if the attack of the hand is twice as
// fast: the Short Sword of Quickness in the hand, or an arrow with the
// Short Bow of Speed in the bow slot. It is no Haste effect: the speed
// counts only for the attack rate (dtuatk.c) and does not add to Haste.
static int hand_fast(void)
{
	int w = hand_weapon(), n = Dtu_char.slot[DTU_SLOT_BOW].n;

	if (w < 0 || dtu_spell_in_hand())
		return 0;
	if (hand_is_melee())
		return dtu_char_item_fast(OBJ_WEAPON,w);
	if (weapon_flag(w,"arrow") && n >= 0 && n < Dtu_n_inventory && Dtu_inventory[n].type)
		return dtu_char_item_fast(Dtu_inventory[n].type,Dtu_inventory[n].id);
	return 0;
}

// ASCENT: D-098 (verified in part in the bytes: 0x44243 doubles the swing
// time with Slow when the item is taken into the hand, 0x8FB9B doubles it at
// the cast). Nothing halves it at the end of Slow, so the item stays slow
// until the item in the hand changes. 1 (default): the time follows the
// effects of each frame. 0 (-origcombat): Slow stays in the swing time
// until the next change of the item in the hand.
int Dtu_fix_slow_swing = 1;
static int Slow_stuck = 0;

// ASCENT: D-012. The hit of the player that kills a creature plays the
// monster_kill_sound of the weapon in place of monster_hit_sound (0x27CCF
// to 0x27D08, the missile 0x27650), and no shipped table sets it, so the
// killing blow is silent at the hit point. 1 (default): the hit sound
// plays on that blow too. 0 (-origcombat): the rule of 1.3.
int Dtu_fix_kill_sound = 1;

// The effects and the Haste rule for the attack rate (dtuatk.c)
static int rate_effects(void)
{
	int fx = ConsoleObject ? ConsoleObject->dtu.m_88 : 0;

	if (fx & DTU_FX_SLOW)
		Slow_stuck = 1;
	if (!Dtu_fix_slow_swing && Slow_stuck)
		fx |= DTU_FX_SLOW;
	return dtu_atk_rate_effects(fx,hand_fast());
}

static int rate_fix_haste(void)
{
	return dtu_atk_rate_fix_haste(ConsoleObject ? ConsoleObject->dtu.m_88 : 0,hand_fast(),Dtu_fix_haste_attack);
}

fix dtu_combat_swing_time(void)
{
	int w = hand_weapon();
	fix model_time = F1_0;

	if (View_model >= 0 && View_model < N_polygon_models && Polygon_models[View_model].dtu_block)
		model_time = Polygon_models[View_model].state_times[DAS_ATTACK];
	return dtu_atk_state_time(hand_is_melee(),w >= 0 ? Weapon_info[w].fire_wait : 0,model_time,
		rate_effects(),rate_fix_haste());
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
	Sword_model = -2;					// ASCENT: Dtu_fix_sword_view, measure again
	Slow_stuck = 0;					// ASCENT: Dtu_fix_slow_swing, 0x44243 takes the Slow of now
	Swing_time = dtu_combat_swing_time();
	// 0x43A8D and 0x43BDA: the pitch offset of the arms is off for an
	// arrow or a bolt (the bow and the crossbow are held differently)
	{
		int w = hand_weapon();
		Pitch_offset = !(weapon_flag(w,"arrow") || weapon_flag(w,"bolt"));
	}
	dtu_obj_log("combat: player model %s (%d), letter %c, swing time %.3f",name,m,letter,f2fl(Swing_time));
}

// The arming after a thrown weapon is used up (D-179). Original: 0x446DA
// empties the hand with 0x43A5A(-3 or -4), which also forgets the box that
// was chosen last, and 0x4448D takes the first box whose index byte (P+
// 0x1AC2 + 2i) is not -1. With the hand index (P+0x1AD8) below 1 it
// writes the number of the box, not the index of its entry, to the hand
// (list 0 at 0x444E4, index at 0x444DC), and then calls 0x43A5A with the
// box (0x444F1 mov eax, edx; 0x444F3). 0x43A5A puts the reference of the
// box into the hand only when its tests pass (0x43DF0); when it refuses
// (an attack state of the arms, an attack that waits, no launcher), the
// hand keeps the item of the list whose index is the box number.
// Corrected (1, default): the item or the spell of the box goes into the
// hand with no tests. The option -origcombat clears it.
int Dtu_fix_autoarm = 1;

// 0x4448D "Autoarming anything": the first hot key box that is not empty
// arms its item or its spell.
static void autoarm(void)
{
	int i;

	if (!Dtu_fix_autoarm) {
		dtu_char_set_current(-1);		// 0x446DA
		dtu_char_forget_hot_key();
		for (i=0;i<DTU_N_HOT_KEYS;i++)
			if (Dtu_char.hot_key[i].n >= 0)
				break;
		if (i < DTU_N_HOT_KEYS) {
			Dtu_char.current.list = 0;		// 0x444DC, 0x444E4: the box number
			Dtu_char.current.n = i;
			dtu_char_update();
			dtu_obj_log("combat: autoarm box %d, the hand holds entry %d (Dtu_fix_autoarm 0)",i,i);
			dtu_char_press_hot_key(i);		// 0x444F3: 0x43A5A(box)
		}
		return;
	}
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

// ASCENT: the miss sound of a melee swing that has touched nothing yet
// (dtumiss.c), one time for each swing. It starts on the first frame of the
// hit test (relative frame 6, 0.34 s into a swing of 1.0 s) and stops if
// the swing then touches something (swing_air_touch). Not for a missile or
// a thrown weapon: they have the sounds of the shot.
static void swing_air_miss(void)
{
	int w = hand_weapon();

	if (Swing_touch || Swing_air)
		return;
	Swing_air = 1;
	if (w < 0 || weapon_int(w,"attack_type",1) == 1)
		if (dtu_miss_sound_player("air",w))
			Swing_air = 2;
}

// ASCENT: the swing touches a creature, scenery or a wall: no miss sound,
// and the one of this swing stops
static void swing_air_touch(void)
{
	Swing_touch = 1;
	if (Swing_air == 2)
		dtu_miss_sound_player_stop("contact");
	Swing_air = 1;
}

static void view_animation(void)
{
	int s = View_state, first = Dtu_anim_first[s], end = Dtu_anim_first[s+1];
	fix st = state_time(s);
	int ended;

	// 0x44589: the frame is computed only for a state time of 1.0 or more
	// (the test is on the integer part) and a state with frames
	// ASCENT: with Dtu_fix_haste_attack the halved swing of Haste (0.5 for
	// most weapons) is animated too. Before this the swing of a player with
	// Haste never left its first frame and never ended: no attack at all.
	if (dtu_atk_state_animates(st,is_attack_state(s),rate_fix_haste()) && end > first) {
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

	if (is_attack_state(s)) {
		dtu_obj_log("combat: swing ends at %.3f, %.3f after its start",f2fl(GameTime),f2fl(GameTime - Swing_started));
		swing_air_miss();		// ASCENT: if the frames of the hit test were skipped
	}

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
		Swing_touch = Swing_air = 0;		// ASCENT: the miss sound
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

// The sounds of the contact of a melee weapon with a creature (0x27AF5,
// 0x27B38)
static void contact_sounds(object *creature,int weapon)
{
	if (weapon >= 0)
		sound_on_object(weapon_int(weapon,"monster_hit_sound",-1),creature,F1_0/2);
	sound_on_object(Monster_info[creature->id].hit_sound,creature,F1_0/2);
}

static void player_hits_creature(object *creature,int weapon,vms_vector *hit_pnt)
{
	object *player = ConsoleObject;
	monster_info *mi;
	int roll, ac, damage = 0, alive, miss_sound;
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

	// The sounds of the contact, before the roll (0x27AF5, 0x27B38).
	// ASCENT: with the miss sound (dtumiss.c) they wait for the roll: a hit
	// plays them, a miss plays the miss sound in their place.
	miss_sound = dtu_miss_sound_ready();
	if (!miss_sound)
		contact_sounds(creature,weapon);
	dtu_creature_alert(player,3);		// ASCENT: 0x27B3F, the alert function 0x160AB

	// 0x29A9C: the to-hit roll (0x2A7F3). A sleeping creature is hit and
	// wakes (0x2A839).
	ac = dtu_spell_creature_ac(creature,mi->armor_class);
	// 0x29D5B: 0xB337E sets the backstab flag 0x13A955 (dtuthief.c)
	Dtu_backstab = dtu_thief_backstab_check(creature,weapon);
	if (creature->dtu.m_88 & DTU_FX_SLEEP) {
		creature->dtu.m_88 &= ~(DTU_FX_SLEEP|DTU_FX_BLIND);
		roll = 1;
		dtu_xp_roll_clear();		// ASCENT: no roll (dtuxp.h)
		dtu_obj_log("combat: obj %d %s sleeps: hit, it wakes",(int)(creature-Objects),dtu_object_name(creature));
	}
	else
		roll = dtu_char_attack_roll(creature,weapon);

	if (miss_sound) {
		if (roll > 0)
			contact_sounds(creature,weapon);
		else
			dtu_miss_sound_player("roll",weapon);
	}

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
			dtu_xp_dmg_end(damage);		// ASCENT: the verbose combat log (dtuxp.h)
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
				else {
					int c = rand() % 4 + 1;

					damage += c;
					dtu_xp_dmg_mod(c,LN("Chill Touch"));
					dtu_xp_dmg_end(damage);
				}
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
			// Dtu_fix_kill_sound (D-012, verified in the bytes at 0x27CCF to
			// 0x27D08); -origcombat gives the rule of 1.3.
			if (alive || Dtu_fix_kill_sound)
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
	dtu_xp_roll_say(player,creature);		// ASCENT: the verbose combat log (dtuxp.h)
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
	vms_vector p1;		// the end of its ray
	fix		radius;
	int		view_ray;	// ASCENT: the object was found by the view ray (Dtu_fix_swing_pitch)
} swing_contact;

// 1 if the swing stops at the contact: an object, or a wall whose side is
// not 1 or 3 ((guess) not the ceiling, not the floor).
static int swing_stops(swing_contact *sc)
{
	return sc->object >= 0 || (sc->fate == HIT_WALL && sc->hit.hit_side != 1 && sc->hit.hit_side != 3);
}

static fix body_eye_offset(void);

// The eye of the arms: the center of the body plus eye_offset along uvec
// (as improved_body_place, without the view bob)
static void swing_eye(object *obj,vms_vector *eye)
{
	*eye = obj->pos;
	vm_vec_scale_add2(eye,&obj->orient.uvec,body_eye_offset());
}

// The view matrix of render_frame: orient * angles_2_matrix(pitch,0,0)
static void swing_view(object *obj,fixang pitch,vms_matrix *view)
{
	vms_angvec a;
	vms_matrix head;

	a.p = pitch; a.b = 0; a.h = 0;
	vm_angles_2_matrix(&head,&a);
	vm_matrix_x_matrix(view,&obj->orient,&head);
}

// ASCENT: Dtu_fix_swing_pitch (dtucombat.h), looking up. The center of the
// sphere and the end of its ray (p1) are turned with the look pitch about
// the eye: the point at pitch 0 in the space of the player is placed in the
// view matrix. The distance from the eye and the length of the ray stay. If
// a wall, the floor or the ceiling is between the eye and the center, the
// center moves to 0.25 before it on that line and the ray keeps its
// direction and length. Returns the segment of the center, -1 for none.
static int swing_pitch_turn(object *obj,fixang pitch,vms_vector *center,vms_vector *p1)
{
	vms_matrix view;
	vms_vector eye, d, l, ray;
	fvi_query fq;
	fvi_info hit;
	int i, seg;

	swing_view(obj,pitch,&view);
	swing_eye(obj,&eye);
	for (i=0;i<2;i++) {
		vms_vector *p = i ? p1 : center;

		vm_vec_sub(&d,p,&eye);
		l.x = vm_vec_dot(&d,&obj->orient.rvec);
		l.y = vm_vec_dot(&d,&obj->orient.uvec);
		l.z = vm_vec_dot(&d,&obj->orient.fvec);
		*p = eye;
		vm_vec_scale_add2(p,&view.rvec,l.x);
		vm_vec_scale_add2(p,&view.uvec,l.y);
		vm_vec_scale_add2(p,&view.fvec,l.z);
	}
	vm_vec_sub(&ray,p1,center);

	seg = find_point_seg(&eye,obj->segnum);
	if (seg < 0)
		seg = obj->segnum;
	fq.p0 = &eye;
	fq.p1 = center;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) == HIT_WALL) {
		fix len;

		vm_vec_sub(&d,&hit.hit_pnt,&eye);
		len = vm_vec_normalize(&d);
		len = len > F1_0/4 ? len - F1_0/4 : 0;
		vm_vec_scale_add(center,&eye,&d,len);
		vm_vec_add(p1,center,&ray);
	}
	return find_point_seg(center,seg);
}

// The objects of the swing query (flag 1 of the query of the executable):
// each body, with the object test of the query (0x3EE70,
// dtu_body_query_test in fvi.c), horizontal radius 0 and vertical radius
// rad_v. The nearest contact wins, as in fvi_sub. Returns the object or -1.
static int swing_objects(object *obj,vms_vector *p0,vms_vector *p1,fix rad_v,vms_vector *ip_out,fix *d_out)
{
	int i, best = -1;
	fix best_d = 0x7fffffff;
	vms_vector ip;

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix d;

		if (o == obj || (o->type != OBJ_ROBOT && o->type != OBJ_DRESSING))
			continue;
		// ASCENT: Dtu_fix_revive: the fallen body of a creature that can get
		// up is hit too (dtu_creature_down_swing)
		if (o->flags & OF_SHOULD_BE_DEAD)
			continue;
		if ((o->flags & OF_DESTROYED) && !dtu_creature_down(o))
			continue;
		if (vm_vec_dist_quick(&o->pos,p0) > i2f(40))
			continue;
		Fvi_dtu_player_attack = (obj == ConsoleObject);	// ASCENT: Dtu_fix_rat_hit
		{
			int dead = o->flags & OF_DESTROYED;

			o->flags &= ~OF_DESTROYED;
			d = dtu_body_query_test(&ip,p0,p1,0,rad_v,o);
			o->flags |= dead;
		}
		Fvi_dtu_player_attack = 0;
		if (d > 0 && d < best_d) {
			best_d = d;
			best = i;
			*ip_out = ip;
		}
	}
	*d_out = best_d;
	return best;
}

// ASCENT: Dtu_fix_swing_pitch (dtucombat.h), the view ray. A ray from the
// eye along the view (with the look pitch) to the distance from the eye of
// the end of the swing ray of this frame at pitch 0 (reach), against the
// bodies as the swing query tests them (horizontal radius 0, the vertical
// radius of the sphere, rad_v), and the ray enters a body through its cap,
// as a missile with Dtu_fix_body_ray. Any wall, the floor or the ceiling
// before the body stops it. Returns the object or -1.
static int swing_view_ray(object *obj,fix reach,fix rad_v,vms_vector *ip)
{
	vms_matrix view;
	vms_vector eye, end;
	fvi_query fq;
	fvi_info hit;
	int seg, best;
	fix d;

	swing_eye(obj,&eye);
	seg = find_point_seg(&eye,obj->segnum);
	if (seg < 0)
		return -1;
	swing_view(obj,Dtu_look_pitch,&view);
	vm_vec_scale_add(&end,&eye,&view.fvec,reach);
	Fvi_dtu_missile = 1;
	best = swing_objects(obj,&eye,&end,rad_v,ip,&d);
	Fvi_dtu_missile = 0;
	if (best < 0)
		return -1;
	fq.p0 = &eye;
	fq.p1 = &end;
	fq.startseg = seg;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	if (find_vector_intersection(&fq,&hit) == HIT_WALL && vm_vec_dist(&hit.hit_pnt,&eye) < d)
		return -1;
	return best;
}

// The weapon sphere test of 0x6CFF8 for one frame of the arms model, with no
// effect on the game. The hit test of the swing and the target hint use it.
// Returns 0 if the frame has no test (no weapon sphere).
static int swing_test(int frame,int weapon,swing_contact *sc)
{
	object *obj = ConsoleObject;
	polymodel *po = view_po();
	int fate = HIT_NONE, seg;
	vms_vector test_pos, center, ofs, dir, p1, eye;
	vms_matrix m;
	fix radius, best_d = 0x7fffffff, reach;

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

	vm_vec_sub(&dir,&center,&test_pos);
	vm_vec_normalize_quick(&dir);
	vm_vec_scale_add(&p1,&center,&dir,radius);
	swing_eye(obj,&eye);
	reach = vm_vec_dist(&p1,&eye);

	// ASCENT: Dtu_fix_swing_pitch (dtucombat.h): looking up, the sphere and
	// its ray turn with the look pitch about the eye
	if (Dtu_fix_swing_pitch && Dtu_look_pitch < 0)
		seg = swing_pitch_turn(obj,Dtu_look_pitch,&center,&p1);
	else
		seg = find_point_seg(&center,obj->segnum);
	if (seg < 0 && !Dtu_fix_swing_pitch)
		return 0;

	sc->object = -1;
	sc->center = center;
	sc->p1 = p1;
	sc->radius = radius;
	sc->view_ray = 0;
	sc->hit.hit_type = HIT_NONE;
	sc->hit.hit_side = -1;
	sc->hit.hit_seg = -1;
	if (seg >= 0) {
		fvi_query fq;

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

		sc->object = swing_objects(obj,&center,&p1,radius,&sc->ip,&best_d);
		// fvi_sub 0x3F8FF: a wall nearer than the object wins, but not the floor
		// (side 3) against an object. The distance of an object is the return
		// of the object test (the radius of the body for a start point in it).
		if (sc->object >= 0 && fate == HIT_WALL && sc->hit.hit_side != 3 && vm_vec_dist(&sc->hit.hit_pnt,&center) < best_d)
			sc->object = -1;
	}
	sc->fate = fate;

	// ASCENT: Dtu_fix_swing_pitch: the body at the center of the view, within
	// the reach of this frame, is hit when the sphere touches no object
	if (Dtu_fix_swing_pitch && sc->object < 0) {
		vms_vector ip;
		int o = swing_view_ray(obj,reach,radius,&ip);

		if (o >= 0) {
			sc->object = o;
			sc->ip = ip;
			sc->view_ray = 1;
		}
	}
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
	if (rel < 6)
		return;
	if (rel > n - rel) {
		swing_air_miss();		// ASCENT: the frames of the hit test are over (skipped)
		return;
	}
	// A missile weapon does not hit with the swing (guess: 0x7473F gives the
	// item in the hand, the shot is in 0x56A0D)
	if (weapon >= 0 && weapon_int(weapon,"attack_type",1) != 1)
		return;
	if (!swing_test(View_frame,weapon,&sc)) {
		swing_air_miss();		// ASCENT: no contact on this frame of the hit test
		return;
	}
	best = sc.object;

	{
		vms_vector rel_c;
		vm_vec_sub(&rel_c,&sc.center,&obj->pos);
		dtu_obj_log("combat: test frame %d, sphere %.2f %.2f %.2f from the body, radius %.2f, wall %d side %d, object %d%s, pitch %d",
			View_frame,f2fl(rel_c.x),f2fl(rel_c.y),f2fl(rel_c.z),f2fl(sc.radius),sc.fate,sc.fate == HIT_WALL ? hit->hit_side : -1,best,
			sc.view_ray ? " (view ray)" : "",Dtu_look_pitch);
		if (best >= 0) {
			vms_vector r = Objects[best].pos;
			vm_vec_sub2(&r,&obj->pos);
			dtu_obj_log("combat: contact obj %d at %.2f across, %.2f up from the body",best,f2fl(fix_sqrt(fixmul(r.x,r.x) + fixmul(r.z,r.z))),f2fl(r.y));
		}
	}

	if (!swing_stops(&sc)) {
		swing_air_miss();		// ASCENT: no contact on this frame of the hit test
		return;
	}
	swing_air_touch();		// ASCENT: no miss sound for a swing that touches something

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
			if (dtu_creature_down(other)) {
				// ASCENT: Dtu_fix_revive: a hit on the fallen body
				if (!Smack) {
					Smack = 8;
					View_goal = DAS_FIDGET;
					dtu_creature_down_swing(other,weapon);
				}
			}
			else if (!(other->type == OBJ_ROBOT && DTU_ANIM_STATE(other) == DAS_DIE))
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

// ASCENT: the time from one shot of the weapon to the next
// (Dtu_fix_missile_rate, dtuatk.c). 0 with the rule of the executable.
static fix missile_wait(int w)
{
	return dtu_atk_missile_wait(Weapon_info[w].fire_wait,rate_effects(),Dtu_fix_missile_rate,rate_fix_haste());
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
	// ASCENT: Dtu_fix_missile_rate: not before the end of the wait
	{
		fix since = GameTime - Missile_last;

		if (since < 0) since = -since;
		if (since < missile_wait(w))
			return 0;
	}

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
	Missile_last = GameTime;		// ASCENT: Dtu_fix_missile_rate
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
			dtu_display_next_message_color(1);		// 0x43CA7: index 1
			HUD_init_message("%s",need);
			dtu_display_next_message_color(-1);
			Request = 0;
			return;
		}
		// ASCENT: Dtu_fix_missile_rate. The executable has no wait from one
		// shot to the next: the fire timer of 0x56A0D starts again with each
		// key press (0x56BB0), so fire_wait (2.0 for an arrow, 3.0 for a
		// bolt) never counts and each press fires. Corrected: the shots are
		// fire_wait apart. A press starts the attack state when its fire
		// frame comes at or after the end of the wait; an earlier press is
		// consumed, like a press at the gate of a melee weapon. Haste halves
		// the wait (Dtu_fix_haste_attack). The Short Bow of Speed halves it
		// too, but not again with Haste (Dtu_fix_speed_items, hand_fast).
		if (!Missile_passed) {
			fix wait = missile_wait(w), lead = 0;
			fix since = GameTime - Missile_last;
			polymodel *po = view_po();

			if (since < 0) since = -since;
			if (!is_attack_state(View_state) && po)
				lead = dtu_atk_fire_lead(Swing_time,Dtu_anim_first[DAS_ATTACK],Dtu_anim_first[DAS_ATTACK+1],po->fire_frames[0]);
			if (since + lead < wait) {
				Request = 0;
				return;
			}
			Missile_passed = 1;
			dtu_obj_log("combat: shot request accepted at %.3f, wait %.3f",f2fl(GameTime),f2fl(wait));
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
		// 0x41EC3: with Haste the executable adds half of the swing time
		// (Haste is slower); dtuatk.c has the rule and its correction
		Last_attack = GameTime + (dtu_atk_melee_wait(t,rate_effects(),rate_fix_haste()) - t);
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

// The byte +3 that 0xAC431 gives a creature
static int turn_class_bits(object *obj)
{
	int i, bits;

	// 0xAC4C1: without a name match the class is from the levels of
	// the player and bit 0x10 stays clear ("couldn't find undead")
	// ASCENT: 0xAC4C1: the higher of the two class levels (0 based,
	// signed) plus 1, 8 from level 7 on. It only sets the place in
	// the order of 0xADB7D. The executable takes the levels at the
	// level start, the port at the first use in the level.
	{
		int lv = Dtu_char.pa.class1_level;

		if (Dtu_char.pa.class2_level > lv)
			lv = Dtu_char.pa.class2_level;
		bits = (lv >= 7 ? 8 : lv + 1) & 0x0F;
	}
	for (i=0; i<9; i++)
		if (turn_prefix(Turn_names[i],Robot_names[obj->id])) {
			bits = i | 0x10;
			break;
		}
	return bits;
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
		Turn_state[n].bits = turn_class_bits(obj);
	}
	return Turn_state[n].bits;
}

// The NPC list of the executable (D-020). Original: npc_register_object
// (0xAC431) adds an entry of 7 bytes (object number, turn byte) to the
// list at 0x3D42CC for each object of the level start (0xAD7C4), for a
// gated creature (0x13407) and at the end of a morph (0x5F25C). No code
// takes an entry out: after a death the object number can belong to a new
// creature, which turn undead (0xADC46, the loop at 0xADD94) then handles
// with the class and the bit 0x10 of the old entry, and a gated creature
// that gets a used number has two entries. After 100 entries (0x145520)
// 0xAC431 does nothing at all, so a new creature also gets no hostility,
// awareness and target. Corrected (1, default): turn undead takes each
// creature by object number and signature (Turn_state), and add_npc sets
// up each creature. The option -origcombat clears it. With 0 the port
// keeps the list; an entry whose object is no creature now is skipped
// (0xA66C9 is not reproduced for such objects).
int Dtu_fix_npc_list = 1;

#define NPC_LIST_MAX	100
static struct { short objnum; ubyte bits; } Npc_list[NPC_LIST_MAX];
static int Npc_list_count;

static void combat_init(void);

// 0xAD779: the list is empty at each level start (also a restored level)
void dtu_combat_npc_list_clear(void)
{
	combat_init();
	Npc_list_count = 0;
}

// 0xAC431: the entry of an object. 0 when the list is full.
int dtu_combat_npc_list_add(object *obj)
{
	int bits = 0x0F;		// 0xAC4A0 to 0xAC4AE: class 15, bit 0x10 clear

	if (Npc_list_count >= NPC_LIST_MAX)
		return 0;
	// 0xAC4B4: the class only for a type with the undead flag (0x2000)
	if (obj->type == OBJ_ROBOT && obj->id < N_robot_types && (Monster_info[obj->id].flags & MONSTER_F_UNDEAD))
		bits = turn_class_bits(obj);
	Npc_list[Npc_list_count].objnum = obj-Objects;
	Npc_list[Npc_list_count].bits = bits;
	Npc_list_count++;
	return 1;
}

// ASCENT: for the checks of dtu_spell_priest_check(): the byte +3 of the
// NPC list entry (class and bit 0x10) of a creature
int dtu_combat_priest_turn_bits(object *obj)
{
	int k;

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types)
		return -1;
	if (!Dtu_fix_npc_list) {
		// the first entry of the NPC list with the object number
		for (k=0; k<Npc_list_count; k++)
			if (Npc_list[k].objnum == obj-Objects)
				return Npc_list[k].bits;
		return -1;
	}
	return turn_bits(obj);
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
	ubyte *bitp[MAX_OBJECTS];
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
	destroys = rand() % 11 + 2;	// 0xADD18: at most this many are turned or destroyed
	if (N_player_types > 0)
		sound_at(tbl_int(Player_type_info[0].record,"banish_sound",-1),pl->segnum,&pl->pos,F1_0/6);	// 0x2AAA
	if (level > 14)
		level = 14;					// 0xADF0F
	if (level < 1)
		level = 1;

	// The NPC list in the order of the object numbers, sorted by class
	// (0xADB7D, the same exchange sort). Dtu_fix_npc_list 0: the entries
	// of the list of the executable in their order.
	if (Dtu_fix_npc_list) {
		for (i=0; i<=Highest_object_index; i++)
			if (Objects[i].type == OBJ_ROBOT && Objects[i].id < N_robot_types) {
				turn_bits(&Objects[i]);
				bitp[n] = &Turn_state[i].bits;
				order[n++] = i;
			}
	}
	else
		for (i=0; i<Npc_list_count; i++) {
			bitp[n] = &Npc_list[i].bits;
			order[n++] = Npc_list[i].objnum;
		}
	for (i=0; i<n-1; i++)
		for (j=i+1; j<n; j++)
			if ((*bitp[j] & 0x0F) < (*bitp[i] & 0x0F)) {
				int t = order[i]; ubyte *b = bitp[i];

				order[i] = order[j]; order[j] = t;
				bitp[i] = bitp[j]; bitp[j] = b;
			}

	dtu_obj_log("turn undead: level %d, roll %d, at most %d destroyed",level,roll,destroys);

	for (i=0; i<n; i++) {
		object *obj = &Objects[order[i]];
		int bits, cls, t;

		if (obj->type != OBJ_ROBOT || obj->id >= N_robot_types || !(Monster_info[obj->id].flags & MONSTER_F_UNDEAD) || !turn_alive(obj) || !turn_sees_player(obj))
			continue;
		bits = *bitp[i];
		cls = bits & 0x0F;
		if (!(bits & 0x10)) {
			HUD_init_message("You can no longer attempt to turn the %s.",dtu_object_display_name(obj));
			continue;
		}
		// 0xADF57: the count (turned and destroyed) is used up. The executable clears bit 0x10
		// of this creature first (0xADF09), so it can never be tried
		// again although nothing was tried. Dtu_fix_turn_undead keeps it.
		if (destroys <= 0) {
			if (!Dtu_fix_turn_undead)
				*bitp[i] &= ~0x10;
			break;
		}
		*bitp[i] &= ~0x10;
		t = Turn_table[cls < 9 ? cls : 8][level-1];

		if (t == -4) {
			// 0xAE0DC: "unable to affect", the creature can be tried again
			dtu_creature_turn(obj,0);
			turn_message(927,"You are currently unable to affect the %s.",obj);
			*bitp[i] |= 0x10;
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
			dtu_xp_cause("Turn Undead");		// ASCENT: the session log (dtuxp.h)
			dtu_damage_creature_direct(obj,NULL,obj->shields + F1_0);
			dtu_xp_cause(NULL);
			HUD_init_message(Text_string[928] && Text_string[928][0] ? Text_string[928] : "You destroyed the %s.",name);
			destroys--;
			done++;
		}
		else if (t == -1 || roll >= t) {
			// 0xAE1AC, 0xAE1DD: turned
			dtu_creature_turn(obj,1);
			turn_message(929,"You successfully turned the %s.",obj);
			// 0xAE226 jumps into the end of the destroy case (0xAE17F), so
			// a turn also takes one of the count at 0xAE1A0: the 2 to 12
			// are the undead that one use can turn or destroy.
			destroys--;
			// 0xAE1B8: the T entry writes the table value (0xFF) over the
			// d20 at esp+0x2C4, and 0xAE1C8 reads the d20 without a sign.
			// So after the first T every later undead with a number is
			// turned (255 >= number). Dtu_fix_turn_undead keeps the d20.
			if (t == -1 && !Dtu_fix_turn_undead)
				roll = 255;
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
	if (weapon != -2 && (Monster_info[o->id].flags & MONSTER_F_WEAPON_RESISTANT) && !(weapon >= 0 && weapon_flag(weapon,"magic")) &&
			!dtu_char_launcher_magic(weapon))
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
		Dtu_fix_missile_rate = 0;
		Dtu_fix_rat_hit = 0;
		Dtu_fix_body_ray = 0;
		Dtu_fix_swing_pitch = 0;
		Dtu_fix_attack_commit = 0;
		Dtu_fix_stoneskin_word = 0;
		Dtu_fix_rary_manual = 0;
		Dtu_fix_melf_acid = 0;
		Dtu_fix_damage_save = 0;
		Dtu_fix_dex_missile = 0;
		Dtu_fix_thrown_launcher = 0;
		Dtu_fix_launcher_magic = 0;
		Dtu_fix_thrown_str_hit = 0;
		Dtu_fix_spell_hours = 0;
		Dtu_fix_rest_effects = 0;
		Dtu_fix_turn_undead = 0;
		Dtu_fix_npc_list = 0;
		Dtu_fix_autoarm = 0;
		Dtu_fix_cure_aid = 0;
		Dtu_fix_spell_dice = 0;
		Dtu_fix_prayer_flags = 0;
		Dtu_fix_slow_swing = 0;
		Dtu_fix_kill_sound = 0;
		Dtu_fix_fireball_blind = 0;
		Dtu_fix_burning_hands = 0;
		Dtu_fix_haste_friends = 0;
		Dtu_fix_hold_undead_hd = 0;
		Dtu_fix_haste_count = 0;
		Dtu_fix_sword_view = 0;
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

void dtu_combat_ignore_held_attack(void)
{
	Key_last = 1;
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
	if (!Request)
		Missile_passed = 0;		// ASCENT: a new request (Dtu_fix_missile_rate)
	Request = 1;
	Key_held = 1;
	return 1;
}

static void tour_frame(void);

void dtu_combat_reset_view(void)
{
	Reset_pending = 1;
}

void dtu_combat_frame(void)
{
	static int last_level = -1;

	combat_init();
	Target_hint = 0;
	if (!Dtu_objects_enabled || ConsoleObject == NULL || ConsoleObject->type != OBJ_PLAYER)
		return;

	// ASCENT: also after a load (dtu_combat_reset_view) and if the game
	// time went back (a load by another path): the times of the swing and
	// of the attack count from the game time
	if (last_level != Current_level_num || View_model < 0 || Reset_pending ||
			GameTime < View_anim_start || GameTime < Swing_started) {
		if (Reset_pending || last_level == Current_level_num)
			dtu_obj_log("combat: the arms start again at %.3f (a load)",f2fl(GameTime));
		last_level = Current_level_num;
		Reset_pending = 0;
		View_state = View_goal = DAS_STAND;
		View_frame = Dtu_anim_first[DAS_STAND];
		View_anim_start = GameTime;
		Swing_started = GameTime;
		Last_attack = GameTime - i2f(100);
		Missile_last = GameTime - i2f(100);
		Fire_next = 0;
		Fire_last = -0x7fffffff;
		Turn_last = 0;
		Smack = 0;
		Swing_touch = Swing_air = 0;
		Request = 0;
		change_player_model(hand_letter());
	}

	dtu_spell_frame();
	tour_frame();		// ASCENT: the frame loop of the Spider Pieces 0x14 and 0x17

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
	// The scroll text screen skips the animation (0x43823).
	if (!dtu_scroll_is_open())
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

// ASCENT: test: swingat=N, the weapon in the hand hits creature N as a
// swing that touches it (the roll, the damage and the messages of
// player_hits_creature)
void dtu_combat_test_swing(int objnum)
{
	dtu_attack a;

	if (objnum < 0 || objnum > Highest_object_index || Objects[objnum].type != OBJ_ROBOT || !ConsoleObject)
		return;
	dtu_char_get_attack(&a);
	Smack = 0;
	player_hits_creature(&Objects[objnum],a.weapon_id,&Objects[objnum].pos);
	Smack = 0;
}

// ASCENT: test: the first object that the swing of weapon w touches in the
// hit window (as melee_hint), with a log line for each frame if log is set
static int reach_contact(int objnum,int w,vms_vector *eye,int log)
{
	object *player = ConsoleObject;
	int s = DAS_ATTACK, n = Dtu_anim_first[s+1] - Dtu_anim_first[s], rel;
	swing_contact sc;

	for (rel = 6; rel <= n - rel; rel++) {
		if (!swing_test(Dtu_anim_first[s] + rel,w,&sc))
			continue;
		if (log) {
			// right, up, forward of the body, from the eye
			vms_vector c = sc.center, e = sc.p1;
			vms_matrix *om = &player->orient;

			vm_vec_sub2(&c,eye);
			vm_vec_sub2(&e,eye);
			dtu_char_log("swing %d: frame %d center %.2f %.2f %.2f end %.2f %.2f %.2f (right up forward from the eye), r %.2f, wall %d side %d, object %d%s",
				objnum,rel,f2fl(vm_vec_dot(&c,&om->rvec)),f2fl(vm_vec_dot(&c,&om->uvec)),f2fl(vm_vec_dot(&c,&om->fvec)),
				f2fl(vm_vec_dot(&e,&om->rvec)),f2fl(vm_vec_dot(&e,&om->uvec)),f2fl(vm_vec_dot(&e,&om->fvec)),f2fl(sc.radius),
				sc.fate == HIT_WALL,sc.fate == HIT_WALL ? sc.hit.hit_side : -1,sc.object,sc.view_ray ? " (view ray)" : "");
		}
		if (sc.object >= 0)
			return sc.object;
		if (swing_stops(&sc))
			break;
	}
	return -1;
}

// ASCENT: test (-chartest reach=N.M, tools/check_reach.py). The player
// turns to creature N (the heading only, as in play) and looks at it with
// the look pitch: M 0 at its center from the eye, 1 level, 2 fully up, 3
// fully down. The log line gives whether a wall is between the body and
// the creature, the first object that the swing of the item in the hand
// touches (as melee_hint), the object that a missile of the player on the
// line from the body to the creature meets (dtu_missile_path_target with
// weapon type 0), the target hint, whether the item in the hand can hurt
// the creature (the rule of the hint), the first contact of the swing with
// the rule of 1.3 (orig, Dtu_fix_swing_pitch off), the height of the top of
// the body above the eye and the distance of its center from the eye.
void dtu_combat_reach_test(int objnum,int mode)
{
	object *player = ConsoleObject, *o;
	vms_vector to, end, eye, flat;
	fvi_query fq;
	fvi_info hit;
	int fate, contact, orig, missile, w, pitch = 0;

	if (player == NULL || objnum < 0 || objnum > Highest_object_index)
		return;
	o = &Objects[objnum];
	// ASCENT: the body turns with the heading only, as in play; the look
	// pitch turns the view (it was the body orientation before 2026-11-23)
	flat = o->pos;
	vm_vec_sub2(&flat,&player->pos);
	flat.y = 0;
	if (vm_vec_normalize(&flat) > 0x100)
		vm_vector_2_matrix(&player->orient,&flat,NULL,NULL);
	eye = player->pos;
	vm_vec_scale_add2(&eye,&player->orient.uvec,body_eye_offset());
	if (mode == 0) {
		vms_vector v;
		fix h;

		vm_vec_sub(&v,&o->pos,&eye);
		h = fix_sqrt(fixmul(v.x,v.x) + fixmul(v.z,v.z));
		pitch = fix_atan2(h,-v.y);		// + is down (vm_extract_angles_matrix)
	}
	else if (mode == 2)
		pitch = DTU_LOOK_MIN;
	else if (mode == 3)
		pitch = DTU_LOOK_MAX;
	dtu_combat_set_pitch(pitch);
	pitch = Dtu_look_pitch;

	vm_vec_sub(&to,&o->pos,&player->pos);
	vm_vec_normalize(&to);

	end = o->pos;
	fq.p0 = &player->pos;
	fq.p1 = &end;
	fq.startseg = player->segnum;
	fq.rad = 0;
	fq.thisobjnum = player-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);

	w = hand_weapon();
	contact = reach_contact(objnum,w,&eye,1);
	{
		int save = Dtu_fix_swing_pitch;

		Dtu_fix_swing_pitch = 0;
		orig = reach_contact(objnum,w,&eye,0);
		Dtu_fix_swing_pitch = save;
	}
	missile = dtu_missile_path_target(&player->pos,player->segnum,&to,0,-1,player-Objects,i2f(100));
	{
		fix br, bh;

		dtu_get_body_size(o,&br,&bh);
		dtu_char_log("reach %d: %s id %d dist %.2f wall %d contact %d missile %d hint %d hurt %d up %.2f seg %d pitch %d mode %d orig %d top %.2f eyedist %.2f",
			objnum,dtu_object_name(o),o->id,f2fl(vm_vec_dist(&o->pos,&player->pos)),fate == HIT_WALL,contact,missile,target_hint(),
			hint_creature(objnum,w),f2fl(o->pos.y - player->pos.y),player->segnum,pitch,mode,orig,f2fl(o->pos.y + bh - eye.y),
			f2fl(vm_vec_dist(&o->pos,&eye)));
	}
}

void dtu_combat_set_pitch(int pitch)
{
	if (pitch < DTU_LOOK_MIN) pitch = DTU_LOOK_MIN;
	if (pitch > DTU_LOOK_MAX) pitch = DTU_LOOK_MAX;
	Dtu_look_pitch = pitch;
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
	polyobj_get_body_center((int)(po - Polygon_models),frame,&bc);
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

//------------------------------------------------------------------------
// ASCENT: Dtu_fix_sword_view (dtucombat.h). The model of the arms of the
// player is <archive><letter>_p.pof, with the letter weapon_char of the
// weapon in the hand (0x43A5A, the name at 0x43EB2 to 0x43F96). Every long
// sword of the tables (weapons 6, 13, 14, 32, 33, 71) has the letter g.
// 0x67C5A draws the arms 0x13880 above the center of the body (less 16 per
// unit of look pitch) with no sideways offset, 0x729C1 takes the center of
// the body box of the frame away from the position, and 0x4450C puts the
// arms into state 1 (STAND) when the player does not move. The body box of
// the g models (the long blade is likely in it) has its center in the
// stand frames 0.85 further right and 0.5 further forward than the broad
// sword model
// (hmcg_p.pof 1.27, 1.20, 1.58; hmch_p.pof 0.43, 0.28, 1.06). The whole
// arms model is drawn that much left and back: the blade stands upright
// in the middle of the view, its middle 0.07 left of the eye. The broad
// sword (letter h) has its middle 0.69 right of the eye; every other melee
// model holds its weapon further right or lower.
//
// 1 (default): in the states that are not an attack, the arms model moves
// right along the right vector of the view, so that the middle of the
// weapon (between the hand joint and the end marker, as body_compute_lift)
// in the stand and walk frames is at least as far right of the eye as that
// of the broad sword model of the same archive. The measure is the offset
// to the right, not x/z: the middle of the long sword is only 0.3 in front
// of the eye. Only a melee weapon in the hand. Models that already hold the
// weapon that far right do not move. The shift goes out over the first 3/16 of the swing
// and comes back over 0.25 s after the attack; in the rest of the swing the
// arms are where 1.3 draws them. The hit test and the swing time do not
// use the place of the drawn model, so they do not change. 0 (-origcombat):
// the place of 1.3.
int Dtu_fix_sword_view = 1;

#define SWORD_VIEW_REF_LETTER	'h'			// weapon_char of the Broad Sword (weapon 20) in all 46 tables
#define SWORD_VIEW_MAX			(F1_0*2)
#define SWORD_VIEW_IN			(F1_0/4)	// the time to come back after an attack

static int Sword_weapon = -2;			// the weapon in the hand at the measure
static fix Sword_shift = 0;			// the full shift of the model
static fix Sword_k = F1_0;				// the part of the shift now
static fix Sword_last = 0;				// GameTime of the last update of Sword_k

// The smallest x (right of the eye) of the weapon middle over the stand and
// walk frames of model m, or with shift set the largest ref - x. Returns 0
// if the model has no weapon end marker.
static int sword_view_measure(int m,int shift,fix ref,fix *out)
{
	static const int states[2] = { DAS_STAND, DAS_WALK };
	polymodel *po;
	int i, f, n = 0;
	fix best = 0;
	vms_vector p;

	if (m < 0 || m >= N_polygon_models || !Polygon_models[m].dtu_block)
		return 0;
	po = &Polygon_models[m];
	for (i=0;i<2;i++)
		for (f=Dtu_anim_first[states[i]];f<Dtu_anim_first[states[i]+1] && f<po->n_frames;f++) {
			if (!body_weapon_point(po,f,&p))
				continue;
			if (!shift) {
				if (n == 0 || p.x < best)
					best = p.x;
			} else if (n == 0 || ref - p.x > best)
				best = ref - p.x;
			n++;
		}
	*out = best;
	return n > 0;
}

static void sword_view_compute(void)
{
	char name[40];
	int w = hand_weapon(), ref_m;
	fix mine = 0, ref = 0, shift = 0;
	int ok;

	Sword_model = View_model;
	Sword_weapon = w;
	Sword_shift = 0;
	if (w < 0 || !hand_is_melee())
		return;
	ok = sword_view_measure(View_model,0,0,&mine);
	snprintf(name,sizeof(name),"%s%c_p.pof",Dtu_player_archive,SWORD_VIEW_REF_LETTER);
	ref_m = polyobj_find_model(name);
	if (ok && sword_view_measure(ref_m,0,0,&ref) && mine < ref) {
		sword_view_measure(View_model,1,ref,&shift);
		if (shift > SWORD_VIEW_MAX)
			shift = SWORD_VIEW_MAX;
		if (shift < 0)
			shift = 0;
	}
	if (Dtu_fix_sword_view)
		Sword_shift = shift;
	dtu_obj_log("combat: sword view of model %d: weapon middle %.3f, broad sword %.3f, shift %.3f, applied %d",
		View_model,ok ? f2fl(mine) : -1.0,ref_m >= 0 ? f2fl(ref) : -1.0,f2fl(shift),Dtu_fix_sword_view);
}

// The shift of this frame (0 with the setting off)
static fix sword_view_shift(void)
{
	fix dt, rate, k;

	if (Sword_model != View_model || Sword_weapon != hand_weapon())
		sword_view_compute();
	if (Sword_shift == 0)
		return 0;
	dt = GameTime - Sword_last;
	Sword_last = GameTime;
	if (is_attack_state(View_state)) {
		// out over the first 3/16 of the swing
		rate = Swing_time > 0 ? fixdiv(F1_0*16,Swing_time*3) : 0;
		if (dt < 0 || dt > F1_0 || rate == 0)
			Sword_k = 0;
		else if ((Sword_k -= fixmul(dt,rate)) < 0)
			Sword_k = 0;
	} else {
		rate = fixdiv(F1_0,SWORD_VIEW_IN);
		if (dt < 0 || dt > F1_0)
			Sword_k = F1_0;
		else if ((Sword_k += fixmul(dt,rate)) > F1_0)
			Sword_k = F1_0;
	}
	k = fixmul(fixmul(Sword_k,Sword_k),3*F1_0 - 2*Sword_k);	// smooth start and end
	return fixmul(Sword_shift,k);
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
	// ASCENT: Dtu_fix_sword_view, along the right vector of the view (the
	// look pitch turns about it, so it is the same in both places)
	{
		fix s = sword_view_shift();

		if (s)
			vm_vec_scale_add2(&pos,&orient.rvec,s);
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
// The callers of 0x90CEF test the area flag of the spell; the table fixup
// gives Fireball the flags 0x803 (0x1A972, AREA), so the Fireball of the
// player or of a creature reaches this code, and the fire trap (0xA6F51,
// 0xA6F61) too.
//
// Dtu_fix_fireball_blind: 1.3 rolls the save of the blindness (0xAE596, 4)
// but a made save only skips the message, the flash and the sound: 0x90E22
// jumps to 0x90F73, which sets the blindness. The save of the deafness
// (0x90FB5) negates it. The fix makes the save negate the blindness too,
// and spares a creature of the type `blind` (Robot_info +0x98 bit 0x0001, MONSTER_F_BLIND),
// which 1.3 spares from Light and Color Spray (0x8DC1D, 0x8D6D4) but not
// here. -origcombat clears it.
int Dtu_fix_fireball_blind = 1;

void dtu_fireball_blind_deaf(object *o, fix dist)
{
	int player = (o->type == OBJ_PLAYER);
	int sound = -1;

	if ((o->type != OBJ_PLAYER && o->type != OBJ_ROBOT) || dist > i2f(DTU_FIREBALL_BLIND_DIST))
		return;
	if (!player && o->id < N_robot_types)
		sound = Monster_info[o->id].hit_sound;		// Robot_info+0x9A
	else if (player && N_player_types > 0) {
		char value[32];

		// P+0x1B0B, the hard_sound of the player type (combat.md)
		if (bm_tbl_get(Player_type_info[0].record, "hard_sound", value, sizeof(value)))
			sound = atoi(value);
	}
	if (!(o->dtu.m_88 & DTU_FX_BLIND) &&
			!(Dtu_fix_fireball_blind && !player && o->id < N_robot_types && (Monster_info[o->id].flags & MONSTER_F_BLIND))) {
		int saved;

		// 0xAE596(obj, 4): a save skips the message, the flash and the
		// sound, but not the blindness (0x90E22 jumps to 0x90F73)
		if (player)
			dtu_char_save_cause(-1, "the blindness of the fire", NULL);		// ASCENT: the session log
		saved = dtu_spell_save(o, 4);
		if (!saved) {
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
		if (!saved || !Dtu_fix_fireball_blind) {
			o->dtu.m_8c[3] = GameTime + i2f(rand() % DTU_FIREBALL_BLIND_SECONDS + 1);
			o->dtu.m_88 |= DTU_FX_BLIND;
		}
	}
	if (player)
		dtu_char_save_cause(-1, "the deafness of the fire", NULL);		// ASCENT: the session log
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
	dtu_creature_remove_cause(v, "the end of the Fear view (0x903D4)");
	obj_delete(v - Objects);
	dtu_char_log("fear: the view ends, the player is in segment %d", seg);
}

//------------------------------------------------------------------------
// The Spider Pieces 0x14 and 0x17 (0x385D3, 0x38268 of 0x37A6A). The
// executable runs its own frame loop (0x435A6(1, 0): a whole game frame
// with the drawing) inside the use code; the port steps the same loop by
// one game frame at each call of dtu_combat_frame.

static int Tour_piece = 0;			// 0x14, 0x17 or 0 (none)
static int Tour_index;				// the object number of the loop
static fix Tour_end;					// 0x17: the end of the view of Tour_index
static ubyte Tour_render_type;

// 0x17: the next creature from Tour_index on (type 2, flag 2 clear)
static int tour_next_creature(void)
{
	for (; Tour_index <= Highest_object_index; Tour_index++) {
		object *o = &Objects[Tour_index];

		if (o->type == OBJ_ROBOT && !(o->flags & OF_SHOULD_BE_DEAD))
			return 1;
	}
	return 0;
}

// 0x385D3 to 0x3881C for the object Tour_index: a creature of the people
// (Robot_info+0xD9 bit 3, the names rf, rm, av, dro) that the segment of
// the player sees (0x9810D): the player turns to it (0xD7110, 0xD74C3,
// 0x6B088), then one of three at random: save 0 or killed (0x29A9C with no
// attacker and its hit points); a homing missile of a spell of the list
// 0x2FA94; save 2 or "frozen": movement type 3 (spin), spin (0, 0.5, 0),
// lifeleft = its level (0xAE3BB) in seconds.
static void tour_piece14_object(object *o)
{
	static const int spells[14] = { 4, 6, 7, 9, 13, 16, 22, 32, 33, 34, 36, 37, 44, 47 };	// 0x2FA94
	object *p = ConsoleObject;
	vms_vector d;
	int r, level;

	if (o->type != OBJ_ROBOT || (o->flags & OF_SHOULD_BE_DEAD) || o->id >= N_robot_types)
		return;
	{
		// Robot_info+0xD9 bit 3, set by 0x1F448 for these names
		char *n = Monster_info[o->id].name;

		if (strnicmp(n, "rf", 2) && strnicmp(n, "rm", 2) && strnicmp(n, "av", 2) && strnicmp(n, "dro", 3))
			return;
	}
	if (!dtu_brain_sees_player(o))
		return;
	vm_vec_sub(&d, &o->pos, &p->pos);
	vm_vector_2_matrix(&p->orient, &d, NULL, NULL);
	r = rand() % 3;
	if (r == 0) {
		if (!dtu_spell_save(o, 0)) {
			dtu_char_log("use: Spider Piece 0x14 kills obj %d", (int) (o - Objects));
			dtu_damage_creature_direct(o, NULL, o->shields);
		}
	}
	else if (r == 1) {
		int s = spells[rand() % 14];

		dtu_char_log("use: Spider Piece 0x14 fires spell %d at obj %d", s, (int) (o - Objects));
		dtu_spell_piece_missile(s);
	}
	else if (!dtu_spell_save(o, 2)) {
		level = (o->id < N_robot_types) ? Monster_info[o->id].max_hit_points / 8 : 1;
		if (level < 1) level = 1;
		if (level > 18) level = 18;
		o->movement_type = MT_SPINNING;
		o->mtype.spin_rate.x = o->mtype.spin_rate.z = 0;
		o->mtype.spin_rate.y = 0x8000;
		o->lifeleft = i2f(level);
		dtu_char_log("use: Spider Piece 0x14 freezes obj %d for %d s", (int) (o - Objects), level);
	}
}

void dtu_spider_tour_start(int piece)
{
	object *p = ConsoleObject;

	if (Tour_piece || p == NULL)
		return;
	// 0x40974 and 0x34B26: the hands of the interface are drawn again
	// (the port draws them each frame)
	Tour_piece = piece;
	Tour_index = 1;
	if (piece == 0x17) {
		dtu_spell_cast_vclip(12, p);		// 0x3827D
		Tour_render_type = p->render_type;
		p->render_type = RT_NONE;			// 0x382E0
		Tour_end = 0;
	}
	dtu_char_log("use: Spider Piece 0x%x starts its loop (highest object %d)", piece, Highest_object_index);
}

// One step of the loop at each game frame
static void tour_frame(void)
{
	object *p = ConsoleObject;

	if (!Tour_piece)
		return;
	if (Tour_piece == 0x14) {
		if (Tour_index <= Highest_object_index) {
			tour_piece14_object(&Objects[Tour_index]);
			Tour_index++;			// then 0x435A6: this frame
			return;
		}
		// 0x38840: 0xD74C3 with a NULL vector (the assert int 3 of
		// vm_vector_2_matrix, then the bytes at address 0); the port keeps
		// the last orientation. 0x38864: the cast vclip of spell 16.
		dtu_spell_cast_vclip(16, p);
		Tour_piece = 0;
		dtu_char_log("use: Spider Piece 0x14 ends");
		return;
	}
	// 0x17: each creature is the viewer for 1.0 + rand() % 2.0 seconds
	if (Tour_end && GameTime < Tour_end)
		return;
	if (Tour_end)
		Tour_index++;
	if (tour_next_creature()) {
		Viewer = &Objects[Tour_index];		// 0x38309
		Tour_end = GameTime + F1_0 + rand() % 0x20000;
		dtu_char_log("use: Spider Piece 0x17 views obj %d", Tour_index);
		return;
	}
	Viewer = p;								// 0x3834C
	p->render_type = Tour_render_type;	// 0x38356: render type 1
	Tour_piece = 0;
	dtu_char_log("use: Spider Piece 0x17 ends");
}
