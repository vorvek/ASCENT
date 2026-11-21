/*
 * ASCENT: new file. The spells of Descent to Undermountain (DTUGAME.EXE
 * 1.3: spells.c 0x8CFFF, 0x90CEF, 0x9002C, 0x91BC3, 0x911AA; game.c
 * 0x41A4D; laser.c 0x56A0D, 0x56FF6; displays.c 0x3575B, 0x3551B, 0xB7084;
 * collide.c 0xAE596, 0x2A849; 0xAFB55). See dtuspell.h and
 * docs/re/combat.md, part B.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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
#include "gamefont.h"
#include "gameseq.h"
#include "digi.h"
#include "fireball.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtucombat.h"
#include "dtuspell.h"

void make_random_vector(vms_vector *vec);		// ai.c

int Dtu_fix_zero_dice = 1;
int Dtu_fix_stoneskin_word = 1;

int Dtu_spell_current = -1;
int Dtu_spell_book_n[2];
short Dtu_spell_book[2][DTU_SPELL_BOOK_MAX];
int Dtu_spell_slots_max[2][9];
int Dtu_spell_slots_left[2][9];

#define MINUTE		(60*F1_0)		// one round of AD&D: 60.0 seconds of game time

static fix Cast_timer = DTU_FX_NO_END;	// 0x153EBC
static int Cast_spell = -1;
static int Hand_ref = -2;				// Dtu_char.current.n when the spell was taken in the hand
static int Screen_sel = 0;

void dtu_spell_log(char *format, ...)
{
	char buf[400];
	va_list args;

	va_start(args,format);
	vsnprintf(buf,sizeof(buf),format,args);
	va_end(args);
	dtu_obj_log("%s",buf);
}

//------------------------------------------------------------------------
// The spell table

static int spell_ok(int spell)
{
	return spell >= 0 && spell < N_spell_types && Spell_info[spell].record >= 0;
}

static int spell_int(int spell,char *key,int def)
{
	char v[32];

	if (!spell_ok(spell) || !bm_tbl_get(Spell_info[spell].record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

static fix spell_fix(int spell,char *key,fix def)
{
	char v[32];

	if (!spell_ok(spell) || !bm_tbl_get(Spell_info[spell].record,key,v,sizeof(v)))
		return def;
	return fl2f(atof(v));
}

static int spell_flag(int spell,char *flag)
{
	return spell_ok(spell) && bm_tbl_has_flag(Spell_info[spell].record,flag);
}

static char *spell_name(int spell)
{
	return spell_ok(spell) ? Spell_info[spell].name : "?";
}

// Spell_info +0x36 bit 0: type=AREA
static int spell_area(int spell)
{
	char v[16];

	return spell_ok(spell) && bm_tbl_get(Spell_info[spell].record,"type",v,sizeof(v)) && !stricmp(v,"AREA");
}

static int spell_level(int spell)
{
	return spell_int(spell,"level",1);
}

// 0 mage, 1 cleric, -1 none (the class bits of +0x00)
static int spell_book_of(int spell)
{
	if (spell_flag(spell,"mage_use")) return 0;
	if (spell_flag(spell,"cleric_use")) return 1;
	return -1;
}

static int timer_ms(fix t)
{
	return f2i(t);
}

//------------------------------------------------------------------------
// The character

static int char_is(int cls)
{
	return Dtu_char.created && (Dtu_char.pa.class1 == cls || Dtu_char.pa.class2 == cls);
}

// The level (0 based) of a class of the character, or -1.
static int char_class_level(int cls)
{
	if (!Dtu_char.created) return -1;
	if (Dtu_char.pa.class1 == cls) return Dtu_char.pa.class1_level;
	if (Dtu_char.pa.class2 == cls) return Dtu_char.pa.class2_level;
	return -1;
}

// 0x91EC1: the caster level of the player for a spell, from 1
int dtu_spell_player_level(int spell)
{
	int m = char_class_level(DTU_CLASS_MAGE), c = char_class_level(DTU_CLASS_CLERIC);
	int mb = spell_flag(spell,"mage_use"), cb = spell_flag(spell,"cleric_use");

	if (mb && cb)
		return (m > c ? m : c) + 1;
	if (cb)
		return (c >= 0 ? c : 0) + 1;
	return (m >= 0 ? m : 0) + 1;
}

// 0xAE3BB: the level of a creature: max hit points / 8, 1 to 18
static int creature_level(object *obj)
{
	int l = 1;

	if (obj->id < N_robot_types)
		l = Monster_info[obj->id].max_hit_points / 8;
	if (l < 1) l = 1;
	if (l > 18) l = 18;
	return l;
}

static int object_level(object *obj,int spell)
{
	if (obj == NULL) return 1;
	if (obj->type == OBJ_PLAYER) return dtu_spell_player_level(spell);
	if (obj->type == OBJ_ROBOT) return creature_level(obj);
	return 1;
}

static int monster_flags(object *obj)
{
	return (obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types) ? Monster_info[obj->id].flags : 0;
}

static int monster_has(object *obj,char *flag)
{
	return obj && obj->type == OBJ_ROBOT && obj->id < N_robot_types && bm_tbl_has_flag(Monster_info[obj->id].record,flag);
}

// A number of the $MONSTER line of a creature. (The key of the cleric
// level, Robot_info+0xE3, is a guess.)
static int monster_int(object *obj,char *key,int def)
{
	char v[32];

	if (!obj || obj->type != OBJ_ROBOT || obj->id >= N_robot_types || !bm_tbl_get(Monster_info[obj->id].record,key,v,sizeof(v)))
		return def;
	return atoi(v);
}

static char *target_name(object *obj)
{
	if (obj == NULL) return "nothing";
	if (obj->type == OBJ_PLAYER) return "you";
	return dtu_object_name(obj);
}

//------------------------------------------------------------------------
// Spell slots (0x3575B, 0x3551B)

// 0x145CD8: the AD&D wizard table, 18 rows of 9 (row = mage level, 0 based)
static const ubyte Mage_slots[18][9] = {
	{1},{2},{2,1},{3,2},{4,2,1},{4,2,2},{4,3,2,1},{4,3,3,2},{4,3,3,2,1},
	{4,4,3,2,2},{4,4,4,3,3},{4,4,4,4,4,1},{5,5,5,4,4,2},{5,5,5,4,4,2,1},
	{5,5,5,5,5,2,1},{5,5,5,5,5,3,2,1},{5,5,5,5,5,3,3,2},{5,5,5,5,5,3,3,2,1}
};
// 0x145DF8: the AD&D priest table, 18 rows of 7
static const ubyte Cleric_slots[18][7] = {
	{1},{2},{2,1},{3,2},{3,3,1},{3,3,2},{3,3,2,1},{3,3,3,2},{4,4,3,2,1},
	{4,4,3,3,2},{5,4,4,3,2,1},{6,5,5,3,2,2},{6,6,6,4,2,2},{6,6,6,5,3,2,1},
	{6,6,6,6,4,2,1},{7,7,7,6,4,3,1},{7,7,7,7,5,3,2},{8,8,8,8,6,4,2}
};

// 0x1403D2 to 0x1404C2: the bonus of the wisdom for the cleric spell
// levels 1 to 4 (index wisdom 0 to 19)
static int wisdom_bonus(int level,int wis)
{
	if (wis < 0) wis = 0;
	if (wis > 19) wis = 19;
	switch (level) {
		case 1: return wis >= 19 ? 3 : wis >= 14 ? 2 : wis >= 13 ? 1 : 0;
		case 2: return wis >= 16 ? 2 : wis >= 15 ? 1 : 0;
		case 3: return wis >= 19 ? 2 : wis >= 17 ? 1 : 0;
		case 4: return wis >= 18 ? 1 : 0;
	}
	return 0;
}

void dtu_spell_compute_slots(void)
{
	int m = char_class_level(DTU_CLASS_MAGE), c = char_class_level(DTU_CLASS_CLERIC), i;

	memset(Dtu_spell_slots_max,0,sizeof(Dtu_spell_slots_max));
	if (m >= 0) {
		if (m > 17) m = 17;
		for (i=0;i<9;i++)
			Dtu_spell_slots_max[0][i] = Mage_slots[m][i];
	}
	if (c >= 0) {
		if (c > 17) c = 17;
		for (i=0;i<7;i++) {
			Dtu_spell_slots_max[1][i] = Cleric_slots[c][i];
			if (Cleric_slots[c][i] > 0 && i < 4)
				Dtu_spell_slots_max[1][i] += wisdom_bonus(i+1,Dtu_char.pa.wisdom);
		}
	}
	for (i=0;i<9;i++) {
		if (Dtu_spell_slots_left[0][i] > Dtu_spell_slots_max[0][i]) Dtu_spell_slots_left[0][i] = Dtu_spell_slots_max[0][i];
		if (Dtu_spell_slots_left[1][i] > Dtu_spell_slots_max[1][i]) Dtu_spell_slots_left[1][i] = Dtu_spell_slots_max[1][i];
	}
}

void dtu_spell_restore_slots(void)
{
	memcpy(Dtu_spell_slots_left,Dtu_spell_slots_max,sizeof(Dtu_spell_slots_left));
	dtu_spell_log("spell: the slots are full again (mage %d %d %d, cleric %d %d %d)",
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_left[0][2],
		Dtu_spell_slots_left[1][0],Dtu_spell_slots_left[1][1],Dtu_spell_slots_left[1][2]);
}

// 0xB69AD
int dtu_spell_learn(int spell)
{
	int b, i;

	if (!spell_ok(spell))
		return 1;
	if (spell_flag(spell,"mage_use") && char_is(DTU_CLASS_MAGE))
		b = 0;
	else if (spell_flag(spell,"cleric_use") && char_is(DTU_CLASS_CLERIC))
		b = 1;
	else
		return 1;
	for (i=0;i<Dtu_spell_book_n[b];i++)
		if (Dtu_spell_book[b][i] == spell)
			return 2;
	if (Dtu_spell_book_n[b] >= DTU_SPELL_BOOK_MAX)
		return 1;
	Dtu_spell_book[b][Dtu_spell_book_n[b]++] = spell;
	return 0;
}

static int in_book(int spell)
{
	int b, i;

	for (b=0;b<2;b++)
		for (i=0;i<Dtu_spell_book_n[b];i++)
			if (Dtu_spell_book[b][i] == spell)
				return 1;
	return 0;
}

int dtu_spell_select(int spell)
{
	if (!in_book(spell))
		return 0;
	Dtu_spell_current = spell;
	Hand_ref = Dtu_char.current.n;
	HUD_init_message("%s",spell_name(spell));
	dtu_spell_log("spell: '%s' (%d) is in the hand",spell_name(spell),spell);
	return 1;
}

// The two spell lists of a save game of the original (P+0x8F3 and
// P+0x10E3, counts at P+0xF7 and P+0xFB, 16 bytes for each entry with the
// spell number in byte 1).
static void load_books_from_save(char *filename)
{
	FILE *f = fopen(filename,"rb");
	ubyte buf[DTU_SG_PLAYER_OFFSET + DTU_PLAYER_RECORD_SIZE];
	int b;

	if (f == NULL)
		return;
	if (fread(buf,1,sizeof(buf),f) != sizeof(buf)) {
		fclose(f);
		return;
	}
	fclose(f);
	for (b=0;b<2;b++) {
		ubyte *p = buf + DTU_SG_PLAYER_OFFSET;
		int n = p[0xF7 + 4*b] | (p[0xF8 + 4*b] << 8), i;
		ubyte *list = p + (b ? 0x10E3 : 0x8F3);

		if (n < 0 || n > DTU_SPELL_BOOK_MAX) n = 0;
		Dtu_spell_book_n[b] = 0;
		for (i=0;i<n;i++)
			if (list[16*i] == OBJ_SPELL && spell_ok(list[16*i+1]))
				Dtu_spell_book[b][Dtu_spell_book_n[b]++] = list[16*i+1];
	}
	dtu_spell_log("spell: %d mage spells and %d cleric spells from %s",Dtu_spell_book_n[0],Dtu_spell_book_n[1],filename);
}

//------------------------------------------------------------------------
// The effect block (object 0x80 to 0xF8)

static fix *timer_ref(object *obj,int off)
{
	if (off >= 0x8C && off <= 0xBC) return &obj->dtu.m_8c[(off - 0x8C) / 4];
	if (off >= 0xC0 && off <= 0xC4) return &obj->dtu.m_c0[(off - 0xC0) / 4];
	if (off >= 0xC8 && off <= 0xDC) return &obj->dtu.m_c8[(off - 0xC8) / 4];
	if (off >= 0xE0 && off <= 0xE4) return &obj->dtu.m_e0[(off - 0xE0) / 4];
	if (off >= 0xE8 && off <= 0xF8) return &obj->dtu.m_e8[(off - 0xE8) / 4];
	Int3();
	return &obj->dtu.m_8c[0];
}

// The timer of each bit (0x9002C). 0: no timer.
static const short Fx_timer[32] = {
	0, 0x94, 0x98, 0x9C, 0, 0xA0, 0xA4, 0xA8, 0xAC, 0, 0xB0, 0xB4, 0xB8, 0xBC, 0xC0, 0xC4,
	0xC8, 0xC8, 0xCC, 0xD0, 0xD4, 0xD8, 0, 0xE0, 0, 0xE4, 0xE8, 0xEC, 0xF0, 0, 0xF4, 0xF8
};

static const char *Fx_name[32] = {
	"Armor", "Bless", "Blindness", "Chill Touch", "Deafness", "Confusion", "Detect Magic", "Feather Fall",
	"Fear", "Protection From Fire", "Resist Fire", "Flying", "Free Action", "Haste", "Hold Person", "Infravision",
	"Improved Invisibility", "Invisibility", "Invulnerability", "Jump", "Find Traps", "Levitate", "Rary's Mnemonic Enhancer", "Protection From Evil",
	"Regeneration", "Shield", "Sleep", "Slow", "Spider Climb", "Stoneskin", "Strength", "Magical Vestment"
};

static int fx_bit(int mask)
{
	int b = 0;

	while (b < 32 && !(mask & (1 << b))) b++;
	return b;
}

// Sets an effect with a duration. Returns 0 if the effect is on already
// (each case of 0x8CFFF returns at once then).
static int fx_set(object *obj,int mask,fix duration)
{
	int b = fx_bit(mask);

	if (obj->dtu.m_88 & mask)
		return 0;
	obj->dtu.m_88 |= mask;
	if (Fx_timer[b])
		*timer_ref(obj,Fx_timer[b]) = (duration >= DTU_FX_NO_END) ? DTU_FX_NO_END : GameTime + duration;
	if (obj->type == OBJ_PLAYER) {
		if (duration >= DTU_FX_NO_END)
			HUD_init_message("%s.",Fx_name[b]);
		else if (duration >= MINUTE)
			HUD_init_message("%s for %d minutes.",Fx_name[b],timer_ms(duration) / 60);
		else
			HUD_init_message("%s for %d seconds.",Fx_name[b],timer_ms(duration));
	}
	dtu_spell_log("spell: effect %s on obj %d %s for %.1f seconds",Fx_name[b],(int)(obj-Objects),target_name(obj),
		duration >= DTU_FX_NO_END ? -1.0 : f2fl(duration));
	return 1;
}

// The end of an effect (the end actions of 0x9002C)
static void fx_end(object *obj,int b)
{
	int mask = (b == 31) ? DTU_FX_VESTMENT : (1 << b);

	obj->dtu.m_88 &= ~mask;
	if (Fx_timer[b])
		*timer_ref(obj,Fx_timer[b]) = 0;
	switch (b) {
		case 1:		// Bless: the hit points of Aid go
			if (obj->type == OBJ_PLAYER && dtu_char_hit_points() > dtu_char_max_hit_points())
				dtu_player_damage(i2f(dtu_char_hit_points() - dtu_char_max_hit_points()));
			break;
		case 16: case 17:	// the two invisibilities end together
			obj->dtu.m_88 &= ~(DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE);
			break;
		case 26:	// Sleep also ends the blindness
			obj->dtu.m_88 &= ~DTU_FX_BLIND;
			break;
	}
	if (obj->type == OBJ_PLAYER) {
		HUD_init_message("%s expired.",Fx_name[b]);
		dtu_char_update();
	}
	dtu_spell_log("spell: effect %s of obj %d %s ends at %.2f",Fx_name[b],(int)(obj-Objects),target_name(obj),f2fl(GameTime));
}

// 0x9002C: the timers of one object. force: all effects end (Dispel Magic).
static void fx_expire(object *obj,int force)
{
	int b, w = obj->dtu.m_88;

	if (w == 0)
		return;
	for (b=0;b<32;b++) {
		int mask = (b == 31) ? DTU_FX_VESTMENT : (1 << b), end = force;

		if (!(w & mask) || b == 24)		// bit 24, regeneration, has no end here
			continue;
		if (!end) {
			if (Fx_timer[b]) {
				fix t = *timer_ref(obj,Fx_timer[b]);
				end = (t != DTU_FX_NO_END && t < GameTime);
			}
			else if (b == 0)
				end = obj->dtu.m_8c[1] <= 0;		// the Armor pool +0x90
			else if (b == 9)
				end = obj->dtu.m_82 < 1;				// Protection From Fire
			else if (b == 29)
				end = obj->dtu.m_80 <= 0;				// Stoneskin
		}
		if (end && (obj->dtu.m_88 & mask))
			fx_end(obj,b);
	}
}

//------------------------------------------------------------------------
// Saving throws (0xAE596)

// 0x145692 and on: rows of 5 numbers by level band. Divisors (0x1456E2):
// cleric 3, mage 5, fighter 2, thief 4.
static const ubyte Save_cleric[4][5] = { {10,14,13,16,15}, {9,13,12,15,14}, {7,11,10,13,12}, {6,10,9,12,11} };
static const ubyte Save_mage[3][5] = { {14,11,13,15,12}, {13,9,11,13,10}, {11,7,9,11,8} };
static const ubyte Save_fighter[6][5] = { {14,16,15,17,17}, {13,15,14,16,16}, {11,13,12,13,14}, {10,12,11,12,13}, {8,10,9,9,11}, {7,9,8,8,10} };
static const ubyte Save_thief[3][5] = { {13,14,12,16,15}, {12,12,11,15,13}, {11,10,10,14,11} };

static int save_number(int cls,int level,int cat)
{
	if (level < 1) level = 1;
	if (level > 12) level = 12;
	switch (cls) {
		case DTU_CLASS_CLERIC:	return Save_cleric[(level-1)/3][cat];
		case DTU_CLASS_MAGE:		return Save_mage[(level-1)/5][cat];
		case DTU_CLASS_THIEF:	return Save_thief[(level-1)/4][cat];
		default:						return Save_fighter[(level-1)/2][cat];
	}
}

int dtu_spell_save(object *obj,int cat)
{
	int need = 20, roll = rand() % 20 + 1, level;

	if (cat < 0 || cat > 4)
		cat = 4;
	if (obj->type == OBJ_PLAYER) {
		dtu_player_attr *pa = &Dtu_char.pa;
		int l1 = pa->class1_level + 1, l2 = pa->class2_level + 1;

		level = l1 > l2 ? l1 : l2;
		need = save_number(pa->class1,level,cat);
		if (pa->class2 >= 0 && pa->class2 <= 3) {
			int n2 = save_number(pa->class2,level,cat);
			if (n2 < need) need = n2;
		}
	}
	else if (obj->type == OBJ_ROBOT) {
		level = creature_level(obj);
		need = save_number(monster_int(obj,"cleric_level",0) > 0 ? DTU_CLASS_CLERIC : DTU_CLASS_FIGHTER,level,cat);
		if (monster_has(obj,"magic_resistant"))
			need -= (level > 12 ? 12 : level) / 3;
	}
	dtu_spell_log("save: obj %d %s, category %d: d20 %d against %d: %s",(int)(obj-Objects),target_name(obj),cat,roll,need,roll >= need ? "saved" : "failed");
	return roll >= need;
}

//------------------------------------------------------------------------
// Sounds and effects

static void sound_at(int sound,int segnum,vms_vector *pos,fix volume)
{
	if (sound < 0 || sound >= MAX_SOUNDS || segnum < 0 || segnum > Highest_segment_index)
		return;
	if (volume > F1_0/2)
		volume = F1_0/2;
	digi_link_sound_to_pos2(sound,segnum,0,pos,0,volume,i2f(256));
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
// The damage of a spell (0x911AA with the roll 0x91BC3)

#define GROUP_NONE	-1
#define GROUP_FIRE	1
#define GROUP_ACID	2
#define GROUP_ELEC	3

// "group" and "count" are the dice count that the case of 0x8CFFF writes
// into the table before the roll (Fireball, Lightning Bolt, Melf's Acid
// Arrows).
static fix spell_damage(int spell,object *target,object *caster,int level,int group,int count,int extra)
{
	static char *keys[4][2] = {
		{ "damage_dice", "#damage_dice" }, { "fire_damage_dice", "#fire_damage_dice" },
		{ "acid_damage_dice", "#acid_damage_dice" }, { "elec_damage_dice", "#elec_damage_dice" } };
	int dmg, g;
	fix dealt = 0;

	if (target->type != OBJ_ROBOT && target->type != OBJ_PLAYER)
		return 0;

	// A group with dice and 0 sides: the executable divides by 0 (0x91C8E)
	for (g=0;g<4;g++) {
		int sides = spell_int(spell,keys[g][0],0), n = (g == group) ? count : spell_int(spell,keys[g][1],0);

		if (sides == 0 && n > 0) {
			if (!Dtu_fix_zero_dice) {
				dtu_spell_log("spell: '%s': %d dice with 0 sides (%s): the executable stops with a divide error here; no damage",
					spell_name(spell),n,keys[g][0]);
				return 0;
			}
			dtu_spell_log("spell: '%s': %d dice with 0 sides (%s): no dice, the part for each level only (Dtu_fix_zero_dice)",
				spell_name(spell),n,keys[g][0]);
		}
	}

	dmg = dtu_spell_damage_roll(spell,target,level) + extra;
	if (target->type == OBJ_PLAYER && (target->dtu.m_88 & DTU_FX_INVULNERABLE))
		dmg = 0;		// 0x9178B
	if (dmg > 0 && dtu_spell_save(target,DTU_SAVE_SPELL))
		dmg /= 2;
	if (target->dtu.m_88 & DTU_FX_ARMOR)
		target->dtu.m_8c[1] -= i2f(dmg);		// the Armor pool (0x913AF)

	if (target->type == OBJ_ROBOT) {
		sound_at(spell_int(spell,"monster_hit_sound",-1),target->segnum,&target->pos,F1_0/2);
		vclip_at(target->segnum,&target->pos,i2f(3),spell_int(spell,"monster_hit_vclip",-1));
		if (caster && caster->type == OBJ_PLAYER)
			HUD_init_message("You cast %s on %s for %d damage.",spell_name(spell),dtu_object_name(target),dmg);
		if (dmg > 0)
			dealt = dtu_damage_creature_direct(target,caster,i2f(dmg));
	}
	else {
		if (dmg > 0)
			dealt = dtu_damage_player_direct(target,caster,i2f(dmg));
		if (caster == NULL || caster == target)
			HUD_init_message("You took %d damage.",dmg);
		else
			HUD_init_message("%s cast %s on you for %d damage.",dtu_object_name(caster),spell_name(spell),dmg);
	}
	dtu_spell_log("spell: '%s' level %d on obj %d %s: damage %d, done %d, hp now %d",spell_name(spell),level,
		(int)(target-Objects),target_name(target),dmg,f2i(dealt),
		target->type == OBJ_PLAYER ? dtu_char_hit_points() : f2i(target->shields));
	return dealt;
}

static void heal(object *target,int points,int spell)
{
	int before;

	if (target->type == OBJ_PLAYER) {
		before = dtu_char_hit_points();
		dtu_char_heal(points);
		HUD_init_message("You feel better.");
		dtu_spell_log("spell: '%s' heals the player by %d: %d -> %d of %d",spell_name(spell),points,before,dtu_char_hit_points(),dtu_char_max_hit_points());
	}
	else if (target->type == OBJ_ROBOT && target->id < N_robot_types) {
		fix max = i2f(Monster_info[target->id].max_hit_points);

		before = f2i(target->shields);
		target->shields += i2f(points);
		if (target->shields > max)
			target->shields = max;
		dtu_spell_log("spell: '%s' heals obj %d %s by %d: %d -> %d",spell_name(spell),(int)(target-Objects),target_name(target),points,before,f2i(target->shields));
	}
}

// The wisdom of the player shortens Fear, Scare, Hold and the stun of
// Color Spray: d / (wisdom / 2) (0x8DAA2, 0x8E338, 0x8E8FA, 0x8F57F)
static fix wisdom_cut(object *target,fix d)
{
	int w = Dtu_char.pa.wisdom;

	if (target->type != OBJ_PLAYER || w < 2)
		return d;
	return fixdiv(d,w << 15);
}

static int rnd(int n)
{
	return rand() % n + 1;
}

// The spells that the port does not have. The cast stops with a message
// before a slot or an item is used.
static int spell_not_in_port(int spell)
{
	return spell == 0 || spell == 41;
}

//------------------------------------------------------------------------
// The effect of a spell on one target (0x8CFFF, jump table 0x8CF1F)

static fix apply_spell(int spell,object *target,object *caster,int L)
{
	int T, self = (target == caster), mf;
	fix dealt = 0;

	if (!spell_ok(spell) || target == NULL)
		return 0;
	if (target->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 0;
	if (target->type != OBJ_PLAYER && target->type != OBJ_ROBOT)
		return 0;
	if (target->type == OBJ_ROBOT && (target->shields < F1_0 || DTU_ANIM_STATE(target) == DAS_DIE))
		return 0;
	if (target->type == OBJ_PLAYER && (Player_is_dead || Dtu_player_dead))
		return 0;

	T = object_level(target,spell);
	mf = monster_flags(target);
	if (L < 1) L = 1;

	// A creature that is the target of the player wakes and is hostile
	// (0xA6505 with 1, then 0x12AD9)
	if (target->type == OBJ_ROBOT && !self && caster && caster->type == OBJ_PLAYER)
		dtu_creature_wake(target,1);

	dtu_spell_log("spell: '%s' (%d) level %d on obj %d %s (level %d)",spell_name(spell),spell,L,(int)(target-Objects),target_name(target),T);

	switch (spell) {
		case 1:		// Aid: 1d8 hit points and Bless for L+1 rounds
			heal(target,rnd(8),spell);
			fx_set(target,DTU_FX_BLESS,(L+1)*MINUTE);
			break;
		case 2:		// Armor: a pool of 8 + L hit points, armor class 6
			if (!fx_set(target,DTU_FX_ARMOR,0))
				break;
			target->dtu.m_8c[1] = i2f(8 + L);
			break;
		case 3:		// Bless: 6 rounds
			fx_set(target,DTU_FX_BLESS,i2f(360));
			break;
		case 4:		// Burning Hands: fire 1d3 + 2 L
		case 17:		// Flaming Sphere (no dice in the line)
		case 37:		// Melf's Minute Meteors: 1d4
			if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			break;
		case 5:		// Chill Touch: on the caster only, L+3 rounds
			if (self)
				fx_set(target,DTU_FX_CHILL_TOUCH,(L+3)*MINUTE);
			break;
		case 6:		// Color Spray
			if (self || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & DTU_FX_BLIND))
				break;
			if ((T > L || T >= 6) && dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			if (T <= L) {
				if (fx_set(target,DTU_FX_SLEEP,(2 + rand() % 7)*MINUTE) && target->type == OBJ_PLAYER) {
					target->dtu.m_88 |= DTU_FX_BLIND;
					target->dtu.m_8c[3] = target->dtu.m_e8[0];
				}
			}
			else if (T <= L + 2)
				fx_set(target,DTU_FX_BLIND,rnd(4)*MINUTE);
			else if (!(target->dtu.m_88 & (DTU_FX_FREE_ACTION|DTU_FX_HOLD)))
				fx_set(target,DTU_FX_HOLD,wisdom_cut(target,MINUTE));
			break;
		case 7:		// Confusion: L+2 rounds
			if (!dtu_spell_save(target,DTU_SAVE_SPELL))
				fx_set(target,DTU_FX_CONFUSION,(L+2)*MINUTE);
			break;
		case 8: case 9:		// Continual Light: blind with no end
		case 30: case 31:		// Light: blind for 10 L or 10 (L+6) rounds
			if (self || (mf & MONSTER_F_BLIND) || (target->dtu.m_88 & DTU_FX_BLIND))
				break;
			if (!dtu_spell_save(target,DTU_SAVE_SPELL))
				fx_set(target,DTU_FX_BLIND,(spell == 30) ? 10*L*MINUTE : (spell == 31) ? 10*(L+6)*MINUTE : DTU_FX_NO_END);
			if (mf & MONSTER_F_LIGHT_SENSITIVE)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,0);
			break;
		case 10:		// Cure Light Wounds: 1d8, not undead or extraplanar
		case 11:		// Cure Serious Wounds: 3 to 17
			if (mf & (MONSTER_F_UNDEAD|MONSTER_F_EXTRAPLANAR))
				break;
			heal(target,spell == 10 ? rnd(8) : rand() % 15 + 3,spell);
			break;
		case 12:		// Detect Magic (mage): 2 L rounds
			fx_set(target,DTU_FX_DETECT_MAGIC,2*L*MINUTE);
			break;
		case 54:		// Detect Magic (cleric): 10 rounds
			fx_set(target,DTU_FX_DETECT_MAGIC,i2f(600));
			break;
		case 13: case 55:	// Dispel Magic: all effects of the target end
			fx_expire(target,1);
			break;
		case 14:		// Fear: L rounds
		case 44:		// Scare: 1d4 + L rounds, targets of level 6 or less
			if (self || (mf & (MONSTER_F_NO_SCARE|MONSTER_F_UNDEAD)) || (target->dtu.m_88 & DTU_FX_FEAR))
				break;
			if (spell == 44 && (T > 6 || (mf & MONSTER_F_EXTRAPLANAR)))
				break;
			if ((spell == 14 || target->type == OBJ_PLAYER) && dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			{
				fix d = wisdom_cut(target,(spell == 14 ? L : rnd(4) + L)*MINUTE);
				fx_set(target,DTU_FX_FEAR,d);
				if (target->type == OBJ_PLAYER)
					fx_set(target,DTU_FX_HOLD,d);
			}
			break;
		case 15:		// Feather Fall: L rounds
			fx_set(target,DTU_FX_FEATHER_FALL,L*MINUTE);
			break;
		case 16:		// Fireball: min(L, 10) fire dice + 6 L
			dealt = spell_damage(spell,target,caster,L,GROUP_FIRE,L < 10 ? L : 10,0);
			break;
		case 18:		// Flame Walk: L+1 rounds
			fx_set(target,DTU_FX_RESIST_FIRE,(L+1)*MINUTE);
			break;
		case 19:		// Fly: 10 (1d6 + L) rounds
			fx_set(target,DTU_FX_FLY,10*(rnd(6) + L)*MINUTE);
			break;
		case 20:		// Free Action: ends Hold, 10 L rounds
			if (!self)
				break;
			if (target->dtu.m_88 & DTU_FX_HOLD)
				fx_end(target,14);
			fx_set(target,DTU_FX_FREE_ACTION,10*L*MINUTE);
			break;
		case 21:		// Haste: ends Slow, L+3 rounds
			if (target->dtu.m_88 & DTU_FX_SLOW)
				fx_end(target,27);
			fx_set(target,DTU_FX_HASTE,(L+3)*MINUTE);
			break;
		case 22: case 52:	// Hold Person: 2 L rounds, a humanoid or the player
			if (self || (target->dtu.m_88 & (DTU_FX_HOLD|DTU_FX_FREE_ACTION)) || (mf & MONSTER_F_UNDEAD))
				break;
			if (target->type == OBJ_ROBOT && !(mf & MONSTER_F_HUMANOID))
				break;
			if (!dtu_spell_save(target,DTU_SAVE_SPELL))
				fx_set(target,DTU_FX_HOLD,wisdom_cut(target,2*L*MINUTE));
			break;
		case 23:		// Hold Undead: 1d4 + L rounds, undead of level L or less
			if (self || (target->dtu.m_88 & DTU_FX_HOLD) || !(mf & MONSTER_F_UNDEAD) || T > L)
				break;
			if (T > 2 && dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			fx_set(target,DTU_FX_HOLD,(rnd(4) + L)*MINUTE);
			break;
		case 24:		// Improved Invisibility: L+4 rounds
			fx_set(target,DTU_FX_IMP_INVIS,(L+4)*MINUTE);
			break;
		case 25:		// Infravision: humans only, 2 hours and 1 hour for each level
			if (target->type == OBJ_PLAYER && Dtu_char.pa.race != DTU_RACE_HUMAN)
				break;
			if (monster_has(target,"infravision"))
				break;
			fx_set(target,DTU_FX_INFRAVISION,(6*L + 12)*10*MINUTE);
			break;
		case 26:		// Invisibility: until an attack
			if (target->dtu.m_88 & (DTU_FX_IMP_INVIS|DTU_FX_INVISIBLE))
				break;
			fx_set(target,DTU_FX_INVISIBLE,DTU_FX_NO_END);
			break;
		case 27:		// Jump: 1d3 + L rounds
			fx_set(target,DTU_FX_JUMP,(rnd(3) + L)*MINUTE);
			break;
		case 28:		// Knock: the case has no code (the table entry is the common end)
			dtu_spell_log("spell: Knock has no effect in the executable");
			break;
		case 29:		// Levitate: 10 L rounds
			fx_set(target,DTU_FX_LEVITATE,10*L*MINUTE);
			break;
		case 32:		// Lightning Bolt: min(L, 10) electric dice + 6 L
			dealt = spell_damage(spell,target,caster,L,GROUP_ELEC,L < 10 ? L : 10,0);
			break;
		case 33:		// Magic Missile: 1d4 with the level 1, Shield stops it
			if (self || (target->dtu.m_88 & DTU_FX_SHIELD))
				break;
			dealt = spell_damage(spell,target,caster,1,GROUP_NONE,0,0);
			break;
		case 34:		// Magical Stone: 1d4, two dice against undead
			if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_NONE,0,(mf & MONSTER_F_UNDEAD) ? rnd(4) : 0);
			break;
		case 35:		// Magical Vestment: 5 L rounds
			fx_set(target,DTU_FX_VESTMENT,5*L*MINUTE);
			break;
		case 36:		// Melf's Acid Arrows: 2 (L/3) + 2 acid dice + 4 L
			if (!self)
				dealt = spell_damage(spell,target,caster,L,GROUP_ACID,2*(L/3) + 2,0);
			break;
		case 38:		// Prayer: Bless for L rounds on the caster and on the others
			// whose bit 0x200 of 0x1B0 is clear (as read)
			if (self || !(target->dtu.m_1b0 & DTU_PEACEFUL))
				fx_set(target,DTU_FX_BLESS,L*MINUTE);
			break;
		case 39:		// Protection From Evil (mage): 2 L rounds
			fx_set(target,DTU_FX_PROT_EVIL,2*L*MINUTE);
			break;
		case 53:		// Protection From Evil (cleric): 3 L rounds
			fx_set(target,DTU_FX_PROT_EVIL,3*L*MINUTE);
			break;
		case 40:		// Protection From Fire: a pool of 12 L hit points
			target->dtu.m_82 += 12*L;
			target->dtu.m_88 |= DTU_FX_PROT_FIRE;
			if (target->type == OBJ_PLAYER)
				HUD_init_message("Protection From Fire for %d hit points.",12*L);
			break;
		case 42:		// "Resist Cold": the code cures blindness and deafness
			if (!(target->dtu.m_88 & (DTU_FX_BLIND|DTU_FX_DEAF)))
				break;
			target->dtu.m_88 &= ~(DTU_FX_BLIND|DTU_FX_DEAF);
			target->dtu.m_8c[3] = 0;
			if (target->type == OBJ_PLAYER)
				HUD_init_message("You are no longer blind or deaf.");
			break;
		case 43:		// Resist Fire: L rounds
			fx_set(target,DTU_FX_RESIST_FIRE,L*MINUTE);
			break;
		case 45:		// Shield: 5 L rounds
			fx_set(target,DTU_FX_SHIELD,5*L*MINUTE);
			break;
		case 46:		// Sleep: 5 L rounds, targets of level 3 or less
			if (self || (target->dtu.m_88 & DTU_FX_SLEEP) || (mf & (MONSTER_F_NO_SLEEP|MONSTER_F_UNDEAD)) || T > 3)
				break;
			if (target->type == OBJ_PLAYER) {
				if (Dtu_char.pa.race == DTU_RACE_ELF && rnd(10) <= 9) break;
				if ((Dtu_char.pa.flags & DTU_PF_HALF_ELF) && rnd(10) <= 3) break;
			}
			if (fx_set(target,DTU_FX_SLEEP,5*L*MINUTE) && target->type == OBJ_PLAYER) {
				target->dtu.m_88 |= DTU_FX_BLIND;
				target->dtu.m_8c[3] = target->dtu.m_e8[0];
			}
			break;
		case 47:		// Slow: ends Haste, L+3 rounds
			if (self || (target->dtu.m_88 & DTU_FX_SLOW) || dtu_spell_save(target,DTU_SAVE_SPELL))
				break;
			if (target->dtu.m_88 & DTU_FX_HASTE)
				fx_end(target,13);
			fx_set(target,DTU_FX_SLOW,(L+3)*MINUTE);
			break;
		case 48:		// Spider Climb: L+3 rounds
			fx_set(target,DTU_FX_SPIDER_CLIMB,(L+3)*MINUTE);
			break;
		case 49:		// Stone Skin: 1d4 + L/2 hits
			if (target->dtu.m_88 & DTU_FX_STONESKIN)
				break;
			if (Dtu_fix_stoneskin_word)
				target->dtu.m_88 |= DTU_FX_STONESKIN;
			else
				target->dtu.m_88 = DTU_FX_STONESKIN;
			target->dtu.m_80 = rnd(4) + L/2;
			if (target->type == OBJ_PLAYER)
				HUD_init_message("Stoneskin for %d attacks.",target->dtu.m_80);
			break;
		case 50:		// Strength: L hours. The gain of strength is not in the port.
			fx_set(target,DTU_FX_STRENGTH,L*3600*F1_0);
			break;
		case 51:		// "Burning Hands" (cold): the code is Find Traps, 30 rounds
			fx_set(target,DTU_FX_FIND_TRAPS,i2f(1800));
			break;
		default:
			dtu_spell_log("spell: '%s' (%d) is not in the port",spell_name(spell),spell);
			if (caster && caster->type == OBJ_PLAYER)
				HUD_init_message("%s is not in this version.",spell_name(spell));
			break;
	}

	// The common end 0x8FFE5: the player gets the new armor class
	if (target->type == OBJ_PLAYER) {
		dtu_char_update();
		dtu_spell_log("spell: the player has armor class %d, effects 0x%08x",dtu_char_armor_class(),target->dtu.m_88);
	}
	return dealt;
}

//------------------------------------------------------------------------
// Area effects (0x90CEF)

static int point_sees(vms_vector *p0,int seg0,object *obj)
{
	fvi_query fq;
	fvi_info hit;

	fq.p0 = p0;
	fq.p1 = &obj->pos;
	fq.startseg = seg0;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = FQ_TRANSWALL;
	return find_vector_intersection(&fq,&hit) == HIT_NONE;
}

// The light object of the light spells: 20.0 (Light) or 60.0 (Continual
// Light), lifetime 10 L or 10 (L+6) rounds, no end for Continual Light.
static void light_object(int spell,vms_vector *pos,int segnum,int L)
{
	int objnum = obj_create(OBJ_LIGHT,0,segnum,pos,NULL,0,CT_LIGHT,MT_NONE,RT_NONE);

	if (objnum < 0)
		return;
	Objects[objnum].ctype.light_info.intensity = (spell == 30 || spell == 31) ? i2f(20) : i2f(60);
	Objects[objnum].lifeleft = (spell == 30) ? 10*L*MINUTE : (spell == 31) ? 10*(L+6)*MINUTE : IMMORTAL_TIME;
	dtu_spell_log("spell: light object %d, light %.0f, life %.0f",objnum,f2fl(Objects[objnum].ctype.light_info.intensity),
		Objects[objnum].lifeleft == IMMORTAL_TIME ? -1.0 : f2fl(Objects[objnum].lifeleft));
}

int dtu_spell_area(int spell,vms_vector *pos,int segnum,object *caster,int level)
{
	fix radius = i2f(spell_int(spell,"radius",0) + level * spell_int(spell,"level_radius",0));
	int i, n = 0;

	if (!spell_ok(spell) || segnum < 0)
		return 0;
	dtu_spell_log("spell: area of '%s' at %.1f %.1f %.1f, radius %.1f",spell_name(spell),f2fl(pos->x),f2fl(pos->y),f2fl(pos->z),f2fl(radius));
	if (spell == 8 || spell == 9 || spell == 30 || spell == 31)
		light_object(spell,pos,segnum,level);
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];

		if (o->type != OBJ_PLAYER && o->type != OBJ_ROBOT)
			continue;
		if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED|OF_EXPLODING))
			continue;
		if (o->type == OBJ_PLAYER && o != ConsoleObject)
			continue;
		if (vm_vec_dist(&o->pos,pos) > radius)
			continue;
		// The executable tests the brain bit of the two segments (0x9810D).
		// The BRN files fit no level (ai.md): the port tests a ray.
		if (!point_sees(pos,segnum,o))
			continue;
		apply_spell(spell,o,caster,level);
		n++;
	}
	return n;
}

//------------------------------------------------------------------------
// The missiles of the player: homing and the area at the end

#define MAX_TRACKED	32

static struct {
	int		objnum, signature, spell, level;
	int		target, target_sig;		// homing
	int		area_done;
	vms_vector last_pos;
	int		last_seg;
} Tracked[MAX_TRACKED];

static void track_missile(int objnum,int spell,int level,int target)
{
	int i;

	for (i=0;i<MAX_TRACKED;i++)
		if (Tracked[i].signature == 0)
			break;
	if (i >= MAX_TRACKED)
		return;
	Tracked[i].objnum = objnum;
	Tracked[i].signature = Objects[objnum].signature;
	Tracked[i].spell = spell;
	Tracked[i].level = level;
	Tracked[i].target = target;
	Tracked[i].target_sig = target >= 0 ? Objects[target].signature : 0;
	Tracked[i].area_done = 0;
	Tracked[i].last_pos = Objects[objnum].pos;
	Tracked[i].last_seg = Objects[objnum].segnum;
}

// 0x55D91 (guess for the rule): the nearest creature in front that the
// player sees, within 200.0.
static int find_homing_target(object *from,vms_vector *dir)
{
	int i, best = -1;
	fix best_d = i2f(200);

	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		vms_vector v;
		fix d;

		if (o->type != OBJ_ROBOT || (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)) || o->shields < F1_0 || DTU_ANIM_STATE(o) == DAS_DIE)
			continue;
		vm_vec_sub(&v,&o->pos,&from->pos);
		d = vm_vec_normalize_quick(&v);
		if (d >= best_d || vm_vec_dot(&v,dir) < F1_0/2)
			continue;
		if (!point_sees(&from->pos,from->segnum,o))
			continue;
		best = i;
		best_d = d;
	}
	return best;
}

static void tracked_frame(void)
{
	int i;

	for (i=0;i<MAX_TRACKED;i++) {
		object *obj;

		if (Tracked[i].signature == 0)
			continue;
		obj = &Objects[Tracked[i].objnum];
		if (obj->signature != Tracked[i].signature || obj->type != OBJ_WEAPON || (obj->flags & OF_SHOULD_BE_DEAD)) {
			// The missile is gone: it hit a wall or its time ended. An area
			// spell has its effect at the last point (0x90CEF from the wall
			// hit of the missile).
			if (spell_area(Tracked[i].spell) && !Tracked[i].area_done)
				dtu_spell_area(Tracked[i].spell,&Tracked[i].last_pos,Tracked[i].last_seg,ConsoleObject,Tracked[i].level);
			Tracked[i].signature = 0;
			continue;
		}
		Tracked[i].last_pos = obj->pos;
		Tracked[i].last_seg = obj->segnum;

		// Homing (Weapon_info+0x0B): turn towards the target. The rule of
		// the turn was not read (guess: the direction moves by 4 times
		// the frame time towards the target, a rule of time).
		if (Tracked[i].target >= 0) {
			object *t = &Objects[Tracked[i].target];
			vms_vector *vel = &obj->mtype.phys_info.velocity, dir, to;
			fix speed;

			if (t->signature != Tracked[i].target_sig || t->type != OBJ_ROBOT || t->shields < F1_0) {
				Tracked[i].target = -1;
				continue;
			}
			speed = vm_vec_mag_quick(vel);
			dir = *vel;
			vm_vec_normalize_quick(&dir);
			vm_vec_sub(&to,&t->pos,&obj->pos);
			vm_vec_normalize_quick(&to);
			vm_vec_scale_add2(&dir,&to,FrameTime*4);
			vm_vec_normalize_quick(&dir);
			vm_vec_copy_scale(vel,&dir,speed);
			vm_vector_2_matrix(&obj->orient,&dir,NULL,NULL);
		}
	}
}

//------------------------------------------------------------------------
// The function of dtuobj.h: a missile with a spell hits an object

fix dtu_spell_hit(int spell, object *target, object *caster, int level)
{
	if (!spell_ok(spell) || target == NULL)
		return 0;
	if (spell_area(spell)) {
		int i;
		fix hp = (target->type == OBJ_ROBOT) ? target->shields : 0;

		// The missile that hit is done (the missile code removes it)
		for (i=0;i<MAX_TRACKED;i++)
			if (Tracked[i].signature && Tracked[i].spell == spell &&
				 vm_vec_dist_quick(&Tracked[i].last_pos,&target->pos) < i2f(40)) {
				Tracked[i].area_done = 1;
				break;
			}
		dtu_spell_area(spell,&target->pos,target->segnum,caster,level);
		return (target->type == OBJ_ROBOT && hp > target->shields) ? hp - target->shields : 0;
	}
	return apply_spell(spell,target,caster,level);
}

//------------------------------------------------------------------------
// The cast of the player (0x41A4D, 0x56A0D, 0x56FF6)

// The source of the cast: 0 a spell of a book, 1 a scroll (a spell item
// of the inventory), 2 a wand (a weapon with PC_spell).
static int hand_spell(int *source)
{
	int n = Dtu_char.current.n;
	dtu_inv_item *e = (n >= 0 && n < Dtu_n_inventory) ? &Dtu_inventory[n] : NULL;

	if (Dtu_spell_current >= 0) {
		*source = 0;
		return Dtu_spell_current;
	}
	if (e && e->type == OBJ_SPELL && spell_ok(e->id)) {
		*source = 1;
		return e->id;
	}
	if (e && e->type == OBJ_WEAPON && e->id < N_weapon_types) {
		char v[16];
		if (bm_tbl_get(Weapon_item_info[e->id].record,"pc_spell",v,sizeof(v)) && atoi(v) >= 0 && spell_ok(atoi(v))) {
			*source = 2;
			return atoi(v);
		}
	}
	return -1;
}

int dtu_spell_in_hand(void)
{
	int source;

	return hand_spell(&source) >= 0;
}

// 0x356E0 and 0x41A4D: the checks of a spell of a book
static int cast_checks(int spell)
{
	int b = spell_book_of(spell), lvl = spell_level(spell), cl;

	if (!char_is(DTU_CLASS_MAGE) && !char_is(DTU_CLASS_CLERIC)) {
		HUD_init_message("You are not a spell caster.");
		return 0;
	}
	if (b == 0 && !char_is(DTU_CLASS_MAGE)) { HUD_init_message("You are not a Mage."); return 0; }
	if (b == 1 && !char_is(DTU_CLASS_CLERIC)) { HUD_init_message("You are not a Cleric."); return 0; }
	if (b < 0)
		return 0;
	cl = char_class_level(b == 0 ? DTU_CLASS_MAGE : DTU_CLASS_CLERIC);
	if (lvl - 1 > cl) {
		HUD_init_message("Spell is of too high a level.");
		return 0;
	}
	if (lvl < 1 || lvl > 9 || Dtu_spell_slots_left[b][lvl-1] <= 0) {
		HUD_init_message("Out of spell points for this spell level.");
		return 0;
	}
	// A mage spell in armor that is not MAGIC_OK
	if (b == 0) {
		int a = Dtu_char.slot[DTU_SLOT_ARMOR].n;
		if (a >= 0 && a < Dtu_n_inventory && Dtu_inventory[a].type == OBJ_ARMOR && Dtu_inventory[a].id < N_armor_types &&
			 !bm_tbl_has_flag(Armor_info[Dtu_inventory[a].id].record,"MAGIC_OK")) {
			HUD_init_message("You cannot cast a spell while wearing %s.",Dtu_inventory[a].name);
			return 0;
		}
	}
	if (ConsoleObject->dtu.m_88 & DTU_FX_LEVITATE) {
		HUD_init_message("You cannot cast a spell while climbing.");
		return 0;
	}
	return 1;
}

// The number of missiles of a spell (0x56FF6)
static int missile_count(int spell,int L)
{
	switch (spell) {
		case 4:	return 6;
		case 6:	return rnd(6);
		case 7:	return rnd(4) + L;
		case 33:	return (L/2 + 1 < 5) ? L/2 + 1 : 5;
		case 34:	return rnd(3);
		case 37:	return L < 5 ? L : 5;
	}
	return 1;
}

static void cast_now(int spell,int source)
{
	object *player = ConsoleObject;
	int L = dtu_spell_player_level(spell), i, n, template_w, target = -1;
	fix speed = spell_fix(spell,"speed",i2f(100));
	vms_vector dir, pos;
	vms_angvec a;
	vms_matrix head, view;

	sound_at(spell_int(spell,"cast_sound",10),player->segnum,&player->pos,F1_0/2);
	dtu_spell_log("spell: the player casts '%s' (%d) at level %d, source %d, at %.3f",spell_name(spell),spell,L,source,f2fl(GameTime));

	if (spell_flag(spell,"self_cast_ok")) {
		// Always on the player himself (0x56A0D)
		if (spell_area(spell) || spell == 38)
			dtu_spell_area(spell,&player->pos,player->segnum,player,L);
		else
			apply_spell(spell,player,player,L);
		vclip_at(player->segnum,&player->pos,i2f(3),spell_int(spell,"cast_vclip",-1));
		return;
	}

	// The missile template (byte 4 of the entry): 2 hands, 3 wand, 4
	// scroll, 5 arrow
	template_w = (source == 2) ? 3 : (source == 1) ? 4 : spell_flag(spell,"cast_with_arrow") ? 5 : 2;
	if (template_w >= N_weapon_types)
		template_w = 0;

	// Casting a missile ends Invisibility (0x5705B)
	if (player->dtu.m_88 & DTU_FX_INVISIBLE)
		fx_end(player,17);

	a.p = Dtu_look_pitch; a.b = 0; a.h = 0;
	vm_angles_2_matrix(&head,&a);
	vm_matrix_x_matrix(&view,&player->orient,&head);
	dir = view.fvec;
	pos = player->pos;
	pos.y += i2f(2);		// 0x562BB

	if (spell_flag(spell,"homing") && (spell != 33 || char_is(DTU_CLASS_MAGE)))
		target = find_homing_target(player,&dir);

	n = missile_count(spell,L);
	for (i=0;i<n;i++) {
		vms_vector d = dir;
		int seg = find_point_seg(&pos,player->segnum), objnum;

		if (seg < 0) seg = player->segnum;
		// The spread of more than one missile (0x56EB0 was not read: guess)
		if (n > 1 && target < 0) {
			vms_vector r;
			make_random_vector(&r);
			vm_vec_scale_add2(&d,&r,F1_0/8);
			vm_vec_normalize_quick(&d);
		}
		objnum = dtu_create_missile(&d,&pos,seg,player-Objects,template_w,spell,i == 0);
		if (objnum < 0)
			break;
		vm_vec_copy_scale(&Objects[objnum].mtype.phys_info.velocity,&d,speed);
		track_missile(objnum,spell,L,target);
		dtu_spell_log("spell: missile obj %d of '%s', speed %.0f, target %d",objnum,spell_name(spell),f2fl(speed),target);
	}
}

// 0xB7084 for a spell of a book (0x35573: one slot of the level when the
// sub uses of the entry are used up)
static void use_slot(int spell)
{
	int b = spell_book_of(spell), lvl = spell_level(spell);

	if (b < 0 || lvl < 1 || lvl > 9)
		return;
	if (Dtu_spell_slots_left[b][lvl-1] > 0)
		Dtu_spell_slots_left[b][lvl-1]--;
	dtu_spell_log("spell: %s slots of level %d left: %d",b ? "cleric" : "mage",lvl,Dtu_spell_slots_left[b][lvl-1]);
}

int dtu_spell_fire(void)
{
	int source, spell = hand_spell(&source);

	if (spell < 0)
		return 1;
	if (spell_not_in_port(spell)) {
		HUD_init_message("%s is not in this version.",spell_name(spell));
		dtu_spell_log("spell: '%s' (%d) is not in the port",spell_name(spell),spell);
		return 1;
	}

	if (source == 0) {
		if (Cast_timer == DTU_FX_NO_END || Cast_spell != spell) {
			int t;

			if (!cast_checks(spell)) {
				Cast_timer = DTU_FX_NO_END;
				return 1;
			}
			t = spell_int(spell,"cast_time",0);
			Cast_timer = GameTime + i2f(t);
			Cast_spell = spell;
			if (t > 60) HUD_init_message("Casting time: %d minutes ...",t / 60);
			else if (t > 1) HUD_init_message("Casting time: %d seconds ...",t);
			else HUD_init_message("Casting time: 1 second ...");
			dtu_spell_log("spell: the cast of '%s' starts, cast time %d seconds",spell_name(spell),t);
			return 0;
		}
		if (Cast_timer > GameTime)
			return 0;
		Cast_timer = DTU_FX_NO_END;
		Cast_spell = -1;
		cast_now(spell,0);
		use_slot(spell);
		return 1;
	}

	// A scroll or a wand: no cast time and no slot. One item or charge.
	cast_now(spell,source);
	dtu_char_use_up(Dtu_char.current.n);
	return 1;
}

//------------------------------------------------------------------------
// The armor class and the to-hit bonus of the effects (0xAFB55, 0x2A849)

int dtu_spell_player_ac(int ac,int stage)
{
	object *p = ConsoleObject;
	int w;

	if (p == NULL || p->type != OBJ_PLAYER)
		return ac;
	w = p->dtu.m_88;
	if (stage == 0) {
		if ((w & DTU_FX_ARMOR) && ac > 6) ac = 6;
		if (w & DTU_FX_VESTMENT) {
			int L = dtu_spell_player_level(35), v = 5 - (L - 5) / 3;
			if (v < 1) v = 1;
			if (v > 5) v = 5;
			if (ac > v) ac = v;
		}
		if ((w & DTU_FX_SHIELD) && ac > 3) ac = 3;
		if (w & DTU_FX_SLOW) ac += 4;
	}
	else if (w & DTU_FX_INVULNERABLE)
		ac -= 2;
	return ac;
}

int dtu_spell_creature_ac(object *c,int ac)
{
	int w = c->dtu.m_88;

	if ((w & DTU_FX_VESTMENT) && ac > 5) ac = 5;
	if ((w & DTU_FX_SHIELD) && ac > 3) ac = 3;
	if ((w & DTU_FX_ARMOR) && ac > 6) ac = 6;
	if (w & DTU_FX_SLOW) {
		ac += 4;
		if (ac > 10) ac = 10;
	}
	return ac;
}

int dtu_spell_to_hit_bonus(object *attacker)
{
	int b = 0;

	if (attacker == NULL)
		return 0;
	if (attacker->dtu.m_88 & DTU_FX_STRENGTH) b += 1;
	if (attacker->dtu.m_88 & DTU_FX_BLESS) b += 1;
	if (attacker->dtu.m_88 & DTU_FX_SLOW) b -= 4;
	return b;
}

//------------------------------------------------------------------------
// The frame

static void spell_init(void)
{
	static int done = 0;
	int t;

	if (done)
		return;
	done = 1;
	if ((t = FindArg("-char")) != 0 && t+1 < Num_args)
		load_books_from_save(Args[t+1]);
}

void dtu_spell_frame(void)
{
	static int last_level = -1;
	int i, t;

	spell_init();
	if (ConsoleObject == NULL || !Dtu_objects_enabled)
		return;

	// The level start (0x4225A): the slots are full
	if (last_level != Current_level_num) {
		last_level = Current_level_num;
		dtu_spell_compute_slots();
		dtu_spell_restore_slots();
		Cast_timer = DTU_FX_NO_END;
		memset(Tracked,0,sizeof(Tracked));
		// Test options: -testslots N gives N slots for each spell level,
		// -testspell N puts spell N into the book and into the hand.
		if ((t = FindArg("-testslots")) != 0 && t+1 < Num_args) {
			int b, l;
			for (b=0;b<2;b++) for (l=0;l<9;l++) Dtu_spell_slots_left[b][l] = Dtu_spell_slots_max[b][l] = atoi(Args[t+1]);
		}
		if ((t = FindArg("-testspell")) != 0 && t+1 < Num_args) {
			int s = atoi(Args[t+1]);
			dtu_spell_learn(s);
			if (!dtu_spell_select(s))
				dtu_spell_log("spell: -testspell %d: the character cannot learn the spell",s);
		}
	}

	// A new item in the hand takes the place of the spell
	if (Dtu_spell_current >= 0 && Dtu_char.current.n != Hand_ref)
		Dtu_spell_current = -1;

	// 0x9002C for the player (game.c 0x438BD) and the creatures (0x14386)
	for (i=0;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		if ((o->type == OBJ_PLAYER || o->type == OBJ_ROBOT) && o->dtu.m_88)
			fx_expire(o,0);
	}

	tracked_frame();
}

//------------------------------------------------------------------------
// The spell list of the statistics screen. The executable shows the spell
// tables of the statistics screen and the spell lists as a view of the
// inventory list (0x35ABD). Their places were not decoded: the port draws
// a list of the books in the empty panel at the lower left (guess).

static int screen_count(void)
{
	return Dtu_spell_book_n[0] + Dtu_spell_book_n[1];
}

static int screen_spell(int i)
{
	if (i < Dtu_spell_book_n[0])
		return Dtu_spell_book[0][i];
	return Dtu_spell_book[1][i - Dtu_spell_book_n[0]];
}

void dtu_spell_screen_keys(int prev,int next,int take)
{
	int n = screen_count();

	if (n <= 0)
		return;
	if (prev && Screen_sel > 0) Screen_sel--;
	if (next && Screen_sel < n - 1) Screen_sel++;
	if (Screen_sel >= n) Screen_sel = n - 1;
	if (take) {
		int s = screen_spell(Screen_sel);
		if (Dtu_spell_current == s)
			Dtu_spell_current = -1;
		else
			dtu_spell_select(s);
	}
}

void dtu_spell_screen_draw(int hires)
{
	int n = screen_count(), i, x = hires ? 40 : 20, y = hires ? 284 : 141, dy = hires ? 12 : 6, first;
	int white = gr_find_closest_color(63,63,63), gray = gr_find_closest_color(40,40,40), orange = gr_find_closest_color(63,40,10);
	char text[80];

	if (!GAME_FONT)
		return;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(gray,-1);
	if (n == 0) {
		gr_string(x,y,"No spells");
		return;
	}
	sprintf(text,"Slots M %d/%d %d/%d %d/%d C %d/%d %d/%d %d/%d",
		Dtu_spell_slots_left[0][0],Dtu_spell_slots_max[0][0],Dtu_spell_slots_left[0][1],Dtu_spell_slots_max[0][1],
		Dtu_spell_slots_left[0][2],Dtu_spell_slots_max[0][2],Dtu_spell_slots_left[1][0],Dtu_spell_slots_max[1][0],
		Dtu_spell_slots_left[1][1],Dtu_spell_slots_max[1][1],Dtu_spell_slots_left[1][2],Dtu_spell_slots_max[1][2]);
	gr_string(x,y,text);
	first = Screen_sel - 4;
	if (first > n - 8) first = n - 8;
	if (first < 0) first = 0;
	for (i=first;i<n && i<first+8;i++) {
		int s = screen_spell(i);
		gr_set_fontcolor(s == Dtu_spell_current ? orange : (i == Screen_sel ? white : gray),-1);
		sprintf(text,"%c %d %s",i < Dtu_spell_book_n[0] ? 'M' : 'C',spell_level(s),spell_name(s));
		gr_string(x,y + dy*(1 + i - first),text);
	}
}
