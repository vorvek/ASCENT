/*
 * ASCENT: new file. The functions that the trigger and trap code of
 * dtutrig.c calls for the creatures and the world (the hooks that
 * dtutrig.c defines weak), the use action on an object, and the start of
 * a talk with a creature that has a script (dialogue.c of DTUGAME.EXE).
 *
 * The facts are in docs/re/triggers.md (section 11) and docs/re/ai.md
 * ("Scripts and NPCs"). Addresses are linear addresses of the image of
 * DTUGAME.EXE 1.3 with object 1 at 0x10000. The marks in the comments:
 *   (code)  read in the disassembly
 *   (guess) not in the executable in this form
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "error.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "physics.h"
#include "fvi.h"
#include "robot.h"
#include "bm.h"
#include "player.h"
#include "gauges.h"
#include "wall.h"
#include "textures.h"
#include "gameseq.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuthief.h"
#include "dtuobj.h"
#include "dtutrig.h"
#include "dtuscript.h"

#define DTU_WEAK __attribute__((weak))

//------------------------------------------------------------------------
// Values of the player character that the hooks need. Weak: the character
// code can define them.

// The skill "climb walls" in percent (the int at 0x1D33EF). 0: the
// character cannot climb.
DTU_WEAK int dtu_player_climb_skill(void)
{
	return 0;
}

// The class (0x1D33B3: the terms 0x8B to 0x8E of a script), the race (0x8F
// to 0x94) and the sex (0x95, 0x96) of the character for the conditions of
// a script.
DTU_WEAK int dtu_player_script_class(void) { return 0; }
DTU_WEAK int dtu_player_script_race(void) { return 0; }
DTU_WEAK int dtu_player_script_sex(void) { return 0; }

//------------------------------------------------------------------------
// Scripts: GABE.RLH and GABE.DAT (docs/formats/jrg.md). One pair of files
// for all levels. The letter is the language (0x1456F8).

static ubyte *Gab_rlh = NULL, *Gab_dat = NULL;
static int Gab_rlh_size = 0, Gab_dat_size = 0, Gab_tried = 0;

// The global flags of the scripts (256 bits, all 0 at the start)
static ubyte Script_gflags[32];

static const char Gab_key[] = "Although curiosity killed the cat, it still had 8 lives left to live.";

static void gab_load(void)
{
	if (Gab_tried)
		return;
	Gab_tried = 1;
	// ASCENT: the files of dtuscript.c (loaded one time, freed at the exit)
	if (!dtu_script_gab_files(&Gab_rlh,&Gab_rlh_size,&Gab_dat,&Gab_dat_size))
		Gab_rlh_size = Gab_dat_size = 0;
}

// The record of a script id, decoded. The caller frees it. GABE.RLH has
// records of 8 bytes: "u16 id, u16 size, u32 offset" into GABE.DAT. Each
// record is decoded by itself (0xA74F8):
// plain[i] = c[i] ^ key[i % 70] ^ key[(0xFFFFFFFF - i) % 70].
static ubyte *gab_record(int id,int *size_out)
{
	int i, n;

	gab_load();
	n = Gab_rlh_size / 8;
	for (i=0;i<n;i++) {
		ubyte *r = Gab_rlh + i*8;
		int rid = (r[0] | (r[1] << 8)) & 0x1ff, size = r[2] | (r[3] << 8);
		uint ofs = r[4] | (r[5] << 8) | (r[6] << 16) | ((uint)r[7] << 24);

		if (rid == id && size > 0 && ofs + size <= (uint)Gab_dat_size) {
			ubyte *p = malloc(size+2);
			uint k;

			if (p == NULL)
				return NULL;
			for (k=0;k<(uint)size;k++)
				p[k] = Gab_dat[ofs+k] ^ Gab_key[k % 70] ^ Gab_key[(0xFFFFFFFFu - k) % 70];
			p[size] = p[size+1] = 0;
			*size_out = size;
			return p;
		}
	}
	return NULL;
}

// 0xAC431, the test of add_npc: 1 if the script of the id has a "say"
// statement (a byte 0xCD), 0 if it has none, -1 if there is no script.
int dtu_script_has_say(int id)
{
	int size, i, found = 0;
	ubyte *p = gab_record(id,&size);

	if (p == NULL)
		return -1;
	for (i=0;i<size;i++)
		if (p[i] == 0xCD) {
			found = 1;
			break;
		}
	free(p);
	return found;
}

// The state of the interpreter (0xAAE3E) for one run
typedef struct script_run {
	ubyte		*s;
	int		size, p, depth;
	object	*obj;
	int		var;
} script_run;

static char *sr_cstr(script_run *r)
{
	char *v = (char *)r->s + r->p;

	while (r->p < r->size && r->s[r->p])
		r->p++;
	r->p++;
	return v;
}

// 0xA76B9: a number is a string of hex digits
static int sr_hexnum(script_run *r)
{
	return (int)(strtol(sr_cstr(r),NULL,16) & 0xffff);
}

// 0xA7DF0
static int sr_number(script_run *r)
{
	int b = r->s[r->p];

	if (b == 0xD2) {				// a random number between two values: the port takes the first
		int a;

		r->p++;
		a = sr_hexnum(r);
		sr_hexnum(r);
		return a;
	}
	if (b == 0xE0) {				// the variable of the object
		r->p++;
		if (r->s[r->p] == 0x15) {
			r->p++;
			sr_number(r);
			return 0;
		}
		return r->var;
	}
	return sr_hexnum(r);
}

// 0xA77BE: the lines of a text end at a byte of 0x7A or more
static void sr_texts(script_run *r,char *out,int out_size)
{
	int n = 0;

	out[0] = 0;
	while (r->p < r->size && r->s[r->p] < 0x7A) {
		char *t = sr_cstr(r);

		for (;*t && n < out_size-2;t++)
			if ((ubyte)*t >= 0x20)			// the tokens 0x09 to 0x11 (name, class, race and sex words) are left out
				out[n++] = *t;
		if (n < out_size-2)
			out[n++] = ' ';
	}
	while (n > 0 && out[n-1] == ' ')
		n--;
	out[n] = 0;
}

// 0xA8C49
static int sr_find_byte(script_run *r,int code)
{
	int cl = code, bh = -1;

	if (code == 0xF4 || code == 0xF9) {
		cl = code + r->depth - 1;
		bh = r->depth + 0xF8;
	}
	while (r->p < r->size) {
		int b = r->s[r->p++];

		if (b == cl)
			return 1;
		if (code == 0xF4 && b == bh) {
			r->p--;
			return 1;
		}
	}
	return 0;
}

// 0xA7872
static int sr_goto(script_run *r,int label)
{
	r->p = 0;
	r->depth = 0;
	while (r->p < r->size)
		if (sr_find_byte(r,0xCB) && sr_hexnum(r) == label)
			return 1;
	return 0;
}

// 0xA7BEE
static int sr_compare(script_run *r,int left)
{
	int op = r->s[r->p++], right;

	if (r->s[r->p] == 0xEF) {
		r->p++;
		right = f2i(dtu_player_max_hit_points());		// the player word 0x1D33BD
	}
	else
		right = sr_number(r);
	switch (op) {
		case 0x7F:	return left < right;
		case 0x80:	return left <= right;
		case 0x81:	return left == right;
		case 0x82:	return left != right;
		case 0x83:	return left > right;
		case 0x84:	return left >= right;
	}
	return 0;
}

// 0xA8516: one term of a condition. The terms that need values that the
// port does not have are false, or compare with a default: an ability is
// 10, an item is not there, a trigger did not fire. (the defaults are
// choices of the port)
static int sr_term(script_run *r)
{
	int neg = 0, b, v = 0;
	int m_1b0 = r->obj->dtu.m_1b0;

	if (r->s[r->p] == 0xE9) {
		neg = 1;
		r->p++;
	}
	b = r->s[r->p++];

	if (b <= 7) {
		if (r->s[r->p] == 0x15) {
			r->p++;
			sr_number(r);
		}
		else
			v = (m_1b0 >> (20 + b)) & 1;
	}
	else if (b >= 0x85 && b <= 0x8A)	v = (b == 0x85);
	else if (b >= 0x8B && b <= 0x8E)	v = (dtu_player_script_class() == b - 0x8B);
	else if (b >= 0x8F && b <= 0x94)	v = (dtu_player_script_race() == b - 0x8F);
	else if (b == 0x95 || b == 0x96)	v = (dtu_player_script_sex() == b - 0x95);
	else if (b == 0x97 || b == 0xDF || b == 0xE0) {
		int other = 0, left;

		if (r->s[r->p] == 0x15) {
			r->p++;
			sr_number(r);
			other = 1;
		}
		left = (b == 0x97) ? ((m_1b0 >> 28) & 15) : (b == 0xDF) ? ((m_1b0 >> 10) & 0x3ff) : r->var;
		v = sr_compare(r,other ? 0 : left);
	}
	else if ((b >= 0x98 && b <= 0xA3) || b == 0xC2 || b == 0xE7) {
		int left = 0;

		if (b >= 0x99 && b <= 0x9E) left = 10;
		if (b == 0x9F) left = dtu_player_level();
		if (b == 0xA0) left = 10;
		if (b == 0xA1) left = Current_level_num;
		v = sr_compare(r,left);
	}
	else if (b == 0xC3) {
		r->p++;
		if (r->s[r->p] == 0xCA)
			r->p++;
		else
			sr_number(r);
		v = sr_compare(r,0);
	}
	else if (b == 0xC4 || b == 0xE8) {
		sr_number(r);
		r->p++;
		if (b == 0xE8)
			v = sr_compare(r,0);
	}
	else if (b == 0xC9) {
		int n = sr_number(r);

		v = (n > 0 && n <= Highest_object_index && Objects[n].type == OBJ_ROBOT && Objects[n].shields < F1_0);
	}
	else if (b == 0xD0 || b == 0xD1) {
		int cls = r->s[r->p++], n = sr_number(r);

		v = (b == 0xD1) ? dtu_hook_player_has_item(Player_num,cls,n) : 0;
	}
	else if (b == 0xF0)
		sr_number(r);
	else if (b == 0xF1) {
		int n = sr_number(r) & 0xff;

		v = (Script_gflags[n >> 3] >> (n & 7)) & 1;
	}
	else {
		r->p--;
		v = sr_compare(r,sr_number(r));
	}
	return neg ? !v : (v != 0);
}

// 0xA8BDB and 0xA8C3A: "IF condition THEN". A false condition jumps to the
// ELSE or ENDIF of the same depth.
static void sr_if(script_run *r)
{
	int v = sr_term(r);

	while (r->p < r->size) {
		int b = r->s[r->p++];

		if (b == 0xE1)			v = sr_term(r) && v;
		else if (b == 0xE2)	v = sr_term(r) || v;
		else {
			if (b == 0xF3)
				r->depth++;
			break;
		}
	}
	if (!v)
		sr_find_byte(r,0xF4);
}

#define DTU_MAX_REPLIES	10

// Runs the script from the current place to the first text. Returns 0xCD
// for a "say" (with its replies), 0xBE for a narration, 0 if the script
// ends with no text, -1 at a statement that the port does not have.
static int sr_run(script_run *r,char *text,int text_size,char replies[DTU_MAX_REPLIES][160],int *n_replies)
{
	int steps;

	*n_replies = 0;
	for (steps=0;steps<400;steps++) {
		int b;

		if (r->p >= r->size)
			return 0;
		b = r->s[r->p++];
		if (b > 0xF4 && b < 0xF9) b = 0xF4;
		if (b > 0xF9 && b < 0xFE) b = 0xF9;

		if (b == 0xBE) {
			sr_texts(r,text,text_size);
			return 0xBE;
		}
		if (b == 0xCD) {
			sr_texts(r,text,text_size);
			while (r->p < r->size) {
				int c = r->s[r->p++];

				if (c > 0xF4 && c < 0xF9) c = 0xF4;
				if (c > 0xF9 && c < 0xFE) c = 0xF9;
				if (c == 0xF2)			sr_if(r);
				else if (c == 0xF4)	{ sr_find_byte(r,0xF9); r->p--; }
				else if (c == 0xF9)	r->depth--;
				else if (c == 0xCF) {
					char buf[160];

					sr_number(r);
					sr_texts(r,buf,sizeof(buf));
					if (*n_replies < DTU_MAX_REPLIES)
						strcpy(replies[(*n_replies)++],buf);
				}
				else
					break;
			}
			return 0xCD;
		}

		if (b == 0xC7) {
			if (!sr_goto(r,sr_number(r)))
				return 0;
		}
		else if (b == 0xC8 || b == 0xCC)
			return 0;
		else if (b == 0xF2)
			sr_if(r);
		else if (b == 0xF4) {
			sr_find_byte(r,0xF9);
			r->p--;
		}
		else if (b == 0xF9)
			r->depth--;
		else if (b == 0xC5 || b == 0xC6) {
			int k = r->s[r->p++];

			if (k <= 7) {
				if (r->s[r->p] == 0x15) {
					r->p++;
					sr_number(r);
				}
				else if (b == 0xC5)
					r->obj->dtu.m_1b0 |= 1 << (20 + k);
				else
					r->obj->dtu.m_1b0 &= ~(1 << (20 + k));
			}
			else {
				int n = sr_number(r) & 0xff;

				if (b == 0xC5)	Script_gflags[n >> 3] |= 1 << (n & 7);
				else				Script_gflags[n >> 3] &= ~(1 << (n & 7));
			}
		}
		else if (b == 0xDF || b == 0xE0 || b == 0xC2 || b == 0xC1 || (b >= 0x99 && b <= 0xA0)) {
			int other = 0, op, n;

			if ((b == 0xDF || b == 0xE0) && r->s[r->p] == 0x15) {
				r->p++;
				sr_number(r);
				other = 1;
			}
			if (b == 0xDF && !(r->s[r->p] >= 0x7A && r->s[r->p] <= 0x7E))
				continue;
			op = r->s[r->p++];
			n = sr_number(r);
			if (b == 0xE0 && !other && op == 0x7E)
				r->var = n;
		}
		else if (b >= 0xB3 && b <= 0xBD)
			;									// an animation state for the creature
		else if (b == 0xEC)
			sr_number(r);					// wait
		else if (b == 0xD3) {			// sound
			sr_number(r);
			sr_number(r);
			if (r->s[r->p] == 0xD8)
				r->p++;
		}
		else
			return -1;
	}
	return 0;
}

// The text 926 of DTUE.TEX
#define TXT_NO_RESPONSE "No response..."

// run_script(object, 0) (0xABBE5): the player talks to an object with a
// script. The executable opens the dialogue window (dlgmenu.c) over the
// view, stops the game and lets the player choose a reply. The port shows
// the first text and the replies in the message box and in the log, and
// does not go on with the script.
static void script_talk(object *obj)
{
	int id = obj->dtu.m_1b0 & DTU_SCRIPT_MASK, size, result, n_replies, i, label;
	script_run r;
	char text[600];
	static char replies[DTU_MAX_REPLIES][160];

	// 0xAAA32: a hostile creature does not talk. In half of the cases it
	// says a line of the label 0x3EA, else "No response...".
	if (obj->dtu.m_1b0 & (DTU_HOSTILE|DTU_FLEEING)) {
		HUD_init_message(TXT_NO_RESPONSE);
		dtu_obj_log("talk: obj %d %s is hostile and does not talk",(int)(obj-Objects),dtu_object_name(obj));
		return;
	}

	memset(&r,0,sizeof(r));
	r.s = gab_record(id,&size);
	if (r.s == NULL) {
		HUD_init_message(TXT_NO_RESPONSE);
		dtu_obj_log("talk: obj %d %s: no script record %d",(int)(obj-Objects),dtu_object_name(obj),id);
		return;
	}
	r.size = size;
	r.obj = obj;

	// The start: the resume label (bits 10 to 19), else the first label
	// below 0x3E8.
	label = (obj->dtu.m_1b0 >> 10) & 0x3ff;
	if (label)
		result = sr_goto(&r,label);
	else {
		result = 0;
		while (r.p < r.size)
			if (sr_find_byte(&r,0xCB) && (label = sr_hexnum(&r)) < 0x3E8) {
				result = 1;
				break;
			}
	}

	// 0xABDC5: the creature turns to the player
	if (obj->type == OBJ_ROBOT) {
		vms_vector v;

		vm_vec_sub(&v,&ConsoleObject->pos,&obj->pos);
		v.y = 0;
		if (vm_vec_normalize(&v) > 0x100)
			vm_vector_2_matrix(&obj->orient,&v,NULL,NULL);
	}
	dtu_creature_wake(obj,0);

	if (result)
		result = sr_run(&r,text,sizeof(text),replies,&n_replies);
	if (result == 0xCD || result == 0xBE) {
		// The message box takes a short line. The whole text is in the log.
		dtu_obj_log("talk: obj %d %s script %d label 0x%X %s: %s",(int)(obj-Objects),dtu_object_name(obj),id,label,
			result == 0xCD ? "says" : "narration",text);
		HUD_init_message("%.100s",text);
		for (i=0;i<n_replies;i++) {
			dtu_obj_log("talk: reply %d) %s",i+1,replies[i]);
			HUD_init_message("%d) %.90s",i+1,replies[i]);
		}
		// 0xABFxx: the talk counter (bits 28 to 31) goes up to 15
		if (((obj->dtu.m_1b0 >> 28) & 15) < 15)
			obj->dtu.m_1b0 += 1 << 28;
	}
	else {
		HUD_init_message(TXT_NO_RESPONSE);
		dtu_obj_log("talk: obj %d %s script %d: no text (result %d at offset 0x%X)",(int)(obj-Objects),dtu_object_name(obj),id,result,r.p);
	}
	free(r.s);
}

// 0xAC155: the use action on an object. A ray of 30.0 along the view with
// the object test. A creature with no script: "No response...". A dead
// object: nothing. Else the script of the object runs (the talk).
int dtu_use_object(object *player)
{
	vms_vector p1, ip;
	fvi_query fq;
	fvi_info hit;
	int i, best = -1, fate;
	fix best_d = 0x7fffffff;
	object *obj;

	if (!Dtu_objects_enabled || Dtu_player_dead)
		return 0;
	// not in an attack (the states 6, 7, 11 and 12 of the player)
	if (DTU_ANIM_STATE(player) == DAS_ATTACK || DTU_ANIM_STATE(player) == DAS_ATTACK2 ||
		 DTU_ANIM_STATE(player) == DAS_ATTACK3 || DTU_ANIM_STATE(player) == DAS_ATTACK4)
		return 0;

	vm_vec_scale_add(&p1,&player->pos,&player->orient.fvec,i2f(30));
	fq.p0 = &player->pos;
	fq.p1 = &p1;
	fq.startseg = player->segnum;
	fq.rad = 0;
	fq.thisobjnum = player-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	fate = find_vector_intersection(&fq,&hit);
	if (fate == HIT_WALL)
		p1 = hit.hit_pnt;
	else if (fate != HIT_NONE)
		return 0;

	for (i=1;i<=Highest_object_index;i++) {
		object *o = &Objects[i];
		fix d;

		if (o == player || (o->type != OBJ_ROBOT && o->type != OBJ_DRESSING) || (o->flags & OF_SHOULD_BE_DEAD))
			continue;
		if (vm_vec_dist_quick(&o->pos,&player->pos) > i2f(60))
			continue;
		d = dtu_object_hit_test(&ip,&player->pos,&p1,F1_0/4,o,player);
		if (d > 0 && d < best_d) {
			best_d = d;
			best = i;
		}
	}
	if (best < 0)
		return 0;
	obj = &Objects[best];

	if ((obj->dtu.m_1b0 & DTU_SCRIPT_MASK) == 0 || obj->type != OBJ_ROBOT) {
		if (obj->type != OBJ_ROBOT)
			return 0;
		if (obj->shields >= F1_0 && !(obj->flags & OF_DESTROYED))
			HUD_init_message(TXT_NO_RESPONSE);
		dtu_obj_log("talk: obj %d %s has no script",best,dtu_object_name(obj));
		return 1;
	}
	if (obj->shields < F1_0 || (obj->flags & OF_DESTROYED))
		return 1;

	dtu_script_talk(obj);		// ASCENT: the interpreter of dtuscript.c
	return 1;
}

//------------------------------------------------------------------------
// The hooks of dtutrig.c

// 0xABBE5(object, 3): a trigger runs the script of an object. The object
// wakes up (0xA6505 does not change the hostile bit). arg 0: the level rule
// of FLAME (0x93B1D) runs the script in mode 0 (talk).
void dtu_hook_trigger_object(int objnum, int arg)
{
	object *obj;

	if (!Dtu_objects_enabled || objnum <= 0 || objnum > Highest_object_index)
		return;
	obj = &Objects[objnum];
	if ((obj->dtu.m_1b0 & DTU_SCRIPT_MASK) == 0) {
		dtu_obj_log("trigger: obj %d has no script",objnum);
		return;
	}
	dtu_obj_log("trigger: script %d of obj %d %s (mode %d)",
		obj->dtu.m_1b0 & DTU_SCRIPT_MASK,objnum,dtu_object_name(obj),arg == 0 ? 0 : 3);
	dtu_script_run(obj,arg == 0 ? DTU_RUN_TALK : DTU_RUN_TRIGGER);		// ASCENT: dtuscript.c
}

// 0x946AE. A weapon with a min_speed (Weapon_info +0xC6) above 0 bursts
// (0x93294). Else one missile (0x934A8): it starts at the position of the
// trap. Its velocity is the direction of the trap times the speed of the
// weapon plus a random vector with whole parts from -4.0 to 5.0, made a
// unit vector and scaled to the speed. The missile is its own parent.
void dtu_hook_trap_weapon(int weapon_type, int segnum, int side, vms_vector *pos, vms_vector *dir)
{
	vms_vector v, p = *pos;
	fix speed;
	int seg;

	(void)side;
	if (!Dtu_objects_enabled || weapon_type < 0 || weapon_type >= N_weapon_types)
		return;

	seg = find_point_seg(&p,segnum);
	if (seg < 0) {
		// a point on the side: a small step along the direction
		vm_vec_scale_add2(&p,dir,F1_0/2);
		seg = find_point_seg(&p,segnum);
		if (seg < 0)
			seg = segnum;
	}

	{
		char value[32];

		if (bm_tbl_get(Weapon_item_info[weapon_type].record,"min_speed",value,sizeof(value)) && atof(value) > 0) {
			dtu_create_burst(weapon_type,seg,&p);
			return;
		}
	}

	speed = Weapon_info[weapon_type].speed[Difficulty_level];
	vm_vec_copy_scale(&v,dir,speed);
	v.x += i2f(rand() % 10 - 4);
	v.y += i2f(rand() % 10 - 4);
	v.z += i2f(rand() % 10 - 4);
	if (vm_vec_normalize(&v) < 0x100)
		v = *dir;
	dtu_create_missile(&v,&p,seg,-1,weapon_type,-1,1);
}

// 0x946FA: the number is a spell (the table at 0x1B5684 is Spell_info +
// 0x0E). The spell flies as a missile of the last weapon entry from the
// position of the trap along the normal of the side.
void dtu_hook_trap_spawn_object(int item_num, int segnum, int side, vms_vector *pos)
{
	vms_vector dir, p = *pos;
	int seg;

	if (!Dtu_objects_enabled || item_num < 0 || item_num >= N_spell_types || N_weapon_types <= 0)
		return;
	if (segnum < 0 || segnum > Highest_segment_index || side < 0 || side >= MAX_SIDES_PER_SEGMENT)
		return;
	dir = Segments[segnum].sides[side].normals[1];
	seg = find_point_seg(&p,segnum);
	if (seg < 0) {
		vm_vec_scale_add2(&p,&dir,F1_0/2);
		seg = find_point_seg(&p,segnum);
		if (seg < 0)
			seg = segnum;
	}
	dtu_obj_log("trap: spell %d '%s' from seg %d side %d",item_num,Spell_info[item_num].name,segnum,side);
	dtu_create_missile(&dir,&p,seg,-1,N_weapon_types-1,item_num,1);
}

// 0x94D61. With an argument below 4 the executable calls a function that
// does nothing (0xAC141). Else gate_in_robot(type, segment, 1).
void dtu_hook_trap_gate_robot(int robot_type, int robot_arg, int segnum, vms_vector *pos)
{
	(void)pos;
	if (!Dtu_objects_enabled || robot_arg < 4)
		return;
	dtu_gate_in_creature(robot_type,segnum);
}

// Step 11 of a trap (0x94598): whole hit points for the player, up to the
// maximum and down to 0. The executable has no death test here: its
// status display finds the death in the next frame (0x33C5E,
// dtu_player_check_death).
void dtu_hook_trap_hit_points(object *obj, int delta)
{
	fix hp;

	(void)obj;
	if (!Dtu_objects_enabled || Dtu_player_dead || delta == 0)
		return;
	hp = dtu_player_damage(-i2f(delta));
	dtu_obj_log("trap: %d hit points for the player, hp %d",delta,f2i(hp));
	dtu_player_check_death(NULL);
}

// 0x9493A
void dtu_hook_drop_objects(int segnum)
{
	if (Dtu_objects_enabled)
		dtu_drop_objects_in_segment(segnum);
}

// 0x951DE: the use action on a side that is no door: the player climbs
// (dtuthief.c).
void dtu_hook_use_wall(object *obj, int segnum, int side)
{
	if (!Dtu_objects_enabled || obj != ConsoleObject)
		return;
	// 0x951D1: the talk comes first in the port, see dtu_use_object()
	if (dtu_use_object(obj))
		return;
	dtu_thief_climb_start(obj,segnum,side);
}

// 0x7EDFA: the use action that hit no side. The use ray goes through an
// open door, so this closes a door in front of the player: a side of the
// segment of the player (not the floor, not the ceiling) that faces the
// player and has a door that is open starts to close. The executable
// looks in the first two segments of the last move of the player and in
// the list of the moving doors. The port looks in the segment of the
// player and in the segment in front of it. (guess for the segments)
void dtu_hook_use_nothing(object *obj)
{
	int pass, side, segnum;

	if (!Dtu_objects_enabled || obj != ConsoleObject)
		return;
	if (dtu_use_object(obj))
		return;

	segnum = obj->segnum;
	for (pass=0;pass<2 && segnum>=0;pass++) {
		segment *segp = &Segments[segnum];
		int next = -1;

		for (side=0;side<MAX_SIDES_PER_SEGMENT;side++) {
			int wn = segp->sides[side].wall_num;
			fix dot;

			if (side == WTOP || side == WBOTTOM)
				continue;
			dot = vm_vec_dot(&obj->orient.fvec,&segp->sides[side].normals[1]);
			if (dot > F1_0/4)
				continue;
			if (wn >= 0 && Walls[wn].type == WALL_DOOR && (Walls[wn].flags & WALL_DOOR_OPENED)) {
				dtu_obj_log("use: the open door seg %d side %d starts to close",segnum,side);
				dtu_wall_start_close(segp,side);
				return;
			}
			if (dot < -F1_0/2 && segp->children[side] >= 0)
				next = segp->children[side];
		}
		segnum = next;
	}
}
