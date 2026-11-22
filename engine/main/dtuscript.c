/*
 * ASCENT: new file. The scripts of Descent to Undermountain (dialogue.c of
 * DTUGAME.EXE 1.3), the global flags, the mission flags and the mission
 * switch of Khelben (frontend.c 0x83E36, mission.c 0x5B3AC). Descent 1 has
 * nothing like it.
 *
 * The facts are in docs/re/scripts.md. Addresses are linear addresses of
 * the image of DTUGAME.EXE 1.3 with object 1 at 0x10000. tools/script.py
 * disassembles the scripts with the same grammar: its listing is the
 * reference for this interpreter.
 *
 * The executable runs a script to its end and waits for a reply or a
 * statement EC in a loop of reduced frames (0x4391D). The port runs a
 * script until it must wait, keeps the state of the interpreter, and goes
 * on in dtu_script_frame() in a later frame; while it waits the world does
 * not move (game.c). A script that calls another (EE) keeps its state on a
 * stack, as the executable keeps it on its stack.
 *
 * Not in the port:
 * - The turn of the object to the player in steps with frames between
 *   them (0xABE01): the port turns it at once.
 * - The executable runs script 0x1FD (the talk of WEAPON 13) to its end
 *   inside the warp check and the talk. The port goes on with the warp or
 *   the talk in the frame in which that script ends (After_kind).
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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
#include "robot.h"
#include "bm.h"
#include "player.h"
#include "gauges.h"
#include "wall.h"
#include "gameseq.h"
#include "gamemine.h"
#include "automap.h"
#include "digi.h"
#include "text.h"
#include "args.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtutrig.h"
#include "dtuspell.h"
#include "dtudlg.h"
#include "dtuscroll.h"		// ASCENT: dtu_scroll_reset at a new level
#include "dtuthiefpts.h"	// ASCENT: the thief skill point screen
#include "dtuscript.h"
#include "dtumovie.h"
#include "dtulog.h"		// ASCENT: the session log
#include "mem.h"		// ASCENT: dtu_load_file() allocates with mem_malloc, so free() must be mem_free()

#define DTU_WEAK __attribute__((weak))

int Dtu_fix_script_new_char = 1;
int Dtu_script_log = 0;

// 1 (default): the corrected rule. Statements 99, 9A and 9B change the
// ability in the values with items (P+0xB7) and in the values without
// items (0x187B90). The executable changes only the first copy, and the
// next change of the equipment (0xAFB55, dtu_char_update) puts the value
// without items back, so the change is lost.
int Dtu_fix_script_abilities = 1;

// E5 (walk). In the executable the creature gets a path of the AI to the
// last segment of the statement (0xAA74E) and its script runs in mode 2 at
// the end of the path (0x174FC). A creature that does not fly and is not
// hostile walks to the player and not to the points of its path there
// (0x17CFC), so the end of the path is not reached while the player
// stands near: a defect of 1.3.
// Dtu_script_walk_teleport 1 (default, a port decision): the creature is
// moved to the last segment at the end of the run and the script runs in
// mode 2 at once, so that a script that goes on in its segment block (CB
// E7) cannot stop. 0 (option -scriptwalk): the creature gets the path
// (dtuobj.c) and the script runs in mode 2 at its end; in the tests of
// 2026-11-22 the kobold of script 21 stood still on its path, so this is
// not the default.
int Dtu_script_walk_teleport = 1;

// 1 (default): the corrected rule of the prison sequence 0xAA3F3. The
// executable takes entry i of the item list with 0xB6D24, which moves the
// later entries down (0xB6F01), and then reads class, id and count at the
// same place: the values of the next entry (or of the empty entry after
// the last). Then it goes on with entry i + 1. So each second item that is
// not worn is lost, a copy of the item after it goes to the thieves, and
// that item stays with the player. With the fix each item that is not
// worn goes to the thieves. Option -origprison.
int Dtu_fix_prison_items = 1;

// The anger byte (ai_local +0x00) and 0xA9418 are in dtuobj.c.
ubyte *dtu_script_anger(object *obj);
void dtu_script_make_hostile(object *obj);

extern int Fe_khelben_mission;			// 0x13E588 (dtufe_town.c)

#define SC_MAX_RECORDS	512
#define SC_MAX_NEST		8
#define SC_MAX_REPLIES	10
#define SC_TEXT_SIZE		0x31E			// 0x3D4588
#define SC_MAX_ALTS		20				// 0xA77BE

// Bits of the object field +0x1B0 (dtu.m_1b0)
#define SB_SCRIPT			0x000001ff
#define SB_NO_HOSTILE	0x00000200	// bit 9
#define SB_RESUME_SHIFT	10
#define SB_RESUME			0x000ffc00
#define SB_FLAG(k)		(1u << (20 + (k)))
#define SB_VAR_SHIFT		24
#define SB_VAR				0x0f000000
#define SB_TALKS_SHIFT	28
#define SB_TALKS			0xf0000000u

// Results of the statement loop
#define SC_DONE	0
#define SC_WAIT	1
#define SC_CALL	2

//------------------------------------------------------------------------
// Log and strings

// Compares at most n characters without case (n < 0: whole strings)
static int sc_strcmpi(const char *a, const char *b, int n)
{
	for (; n != 0; a++, b++, n--) {
		int x = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
		int y = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
		if (x != y)
			return x - y;
		if (!x)
			return 0;
	}
	return 0;
}

static void slog(char *format, ...)
{
	va_list args;

	if (!Dtu_script_log)
		return;
	va_start(args, format);
	printf("script: ");
	vprintf(format, args);
	printf("\n");
	va_end(args);
}

//------------------------------------------------------------------------
// The files (0xAD84D, 0xAB9C9). The executable keeps 20 loaded scripts;
// the port decodes a record when it is first needed.

static ubyte *Gab_rlh = NULL, *Gab_dat = NULL;
static int Gab_rlh_size = 0, Gab_dat_size = 0, Gab_tried = 0;
static ubyte *Rec_data[SC_MAX_RECORDS];
static int Rec_size[SC_MAX_RECORDS];

static const char Gab_key[] = "Although curiosity killed the cat, it still had 8 lives left to live.";

void dtu_script_init(void)
{
	if (Gab_tried)
		return;
	Gab_tried = 1;
	if (FindArg("-scriptlog"))
		Dtu_script_log = 1;
	if (FindArg("-scriptwalk"))
		Dtu_script_walk_teleport = 0;
	if (FindArg("-origprison"))
		Dtu_fix_prison_items = 0;
	// 0xA6467: "gabe.rlh", the fourth letter is the language. The port
	// has the English text.
	Gab_rlh = dtu_load_file("GABE.RLH", &Gab_rlh_size);
	Gab_dat = dtu_load_file("GABE.DAT", &Gab_dat_size);
	if (Gab_rlh == NULL || Gab_dat == NULL) {
		Gab_rlh_size = Gab_dat_size = 0;
		dtu_log("Scripts: GABE.RLH or GABE.DAT not found\n");
	}
}

// 0xAB9C9: the record of a script id, decoded (0xA74F8)
static ubyte *script_data(int id, int *size)
{
	int i, n;

	dtu_script_init();
	id &= SB_SCRIPT;
	if (Rec_data[id]) {
		*size = Rec_size[id];
		return Rec_data[id];
	}
	n = Gab_rlh_size / 8;
	for (i = 0; i < n; i++) {
		ubyte *r = Gab_rlh + i * 8;
		int rid = (r[0] | (r[1] << 8)) & SB_SCRIPT, sz = r[2] | (r[3] << 8);
		uint ofs = r[4] | (r[5] << 8) | (r[6] << 16) | ((uint) r[7] << 24);

		if (rid == id && sz > 0 && ofs + sz <= (uint) Gab_dat_size) {
			ubyte *p = malloc(sz + 1);
			uint k;

			if (!p)
				return NULL;
			for (k = 0; k < (uint) sz; k++)
				p[k] = Gab_dat[ofs + k] ^ Gab_key[k % 70] ^ Gab_key[(0xFFFFFFFFu - k) % 70];
			p[sz] = 0;
			Rec_data[id] = p;
			Rec_size[id] = sz;
			*size = sz;
			return p;
		}
	}
	return NULL;
}

//------------------------------------------------------------------------
// Global flags (0xADAFE, 0xADB3B): 256 bits in the player record at
// P+0x1A33, bit n - 1 for flag n.

static ubyte Gflags[32];

int dtu_script_gflag(int n)
{
	n = (n - 1) & 0xff;
	return (Gflags[n >> 3] >> (n & 7)) & 1;
}

void dtu_script_set_gflag(int n, int value)
{
	n = (n - 1) & 0xff;
	Gflags[n >> 3] = (Gflags[n >> 3] & ~(1 << (n & 7))) | ((value & 1) << (n & 7));
}

//------------------------------------------------------------------------
// Mission flags (0x13CDD4). Mission_dw[] is in the order of the
// addresses, as in the save file.

static int Mission_dw[DTU_N_MISSION_FLAGS];

// The index of mission flag k (1 to 19) in Mission_dw (the tables at
// 0xAADF2, 0xA84CA and 0x5B360 agree).
static int mslot(int k)
{
	if (k >= 1 && k <= 9)	return k - 1;
	if (k >= 10 && k <= 17)	return k + 1;		// 0x13CE00 is index 11
	if (k == 18)				return 9;			// 0x13CDF8
	if (k == 19)				return 10;			// 0x13CDFC
	return -1;
}

int dtu_mission_flag(int k)
{
	int i = mslot(k);
	return i >= 0 ? Mission_dw[i] : 0;
}

void dtu_mission_set_flag(int k, int value)
{
	int i = mslot(k);

	if (i < 0)
		return;
	if (Mission_dw[i] != value)
		dtu_log("Mission flag %d = %d\n", k, value);
	Mission_dw[i] = value;
}

// 0x5B70A: the live robots with this script id
static int robots_with_script(int id)
{
	int i, n = 0;

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];
		if (o->type == OBJ_ROBOT && (o->dtu.m_1b0 & SB_SCRIPT) == id && o->shields >= F1_0)
			n++;
	}
	return n;
}

// 0x5B760: the dressing objects with this name
static int dressing_named(char *name)
{
	int i, n = 0;

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];
		if (o->type == OBJ_DRESSING && !sc_strcmpi(dtu_object_name(o), name, -1))
			n++;
	}
	return n;
}

// 0x5B3AC, commands 1 to 19 of DTU.WRP
void dtu_mission_check(int k)
{
	static short ids[10] = { 0, 0x17, 0x20, 0x46, 0xBE, 0xC9, 0xD4, 0xDE, 0xFC, 0xFE };
	int ok;

	if (k >= 1 && k <= 9)
		ok = robots_with_script(ids[k]) == 0;
	else if (k == 18)
		ok = dressing_named("Cloning Machine") == 0;
	else if (k == 19)
		ok = dressing_named("Spectator Stone") == 0;
	else
		ok = (k >= 10 && k <= 17);
	if (ok && !dtu_mission_flag(k)) {
		slog("mission check %d: done", k);
		dtu_mission_set_flag(k, 1);
	}
}

static int has_misc(int id)
{
	return dtu_char_has(OBJ_MISC, id);
}

// 0x83EF5: the switch of the mission. Returns 1 if *next changed.
static int mission_switch(int m, int *next)
{
	static short pieces[7] = { 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x1A };
	int r = 0, i, all;

	*next = m;
	switch (m) {
	case 0:
		if (dtu_mission_flag(1)) { *next = 1; r = 1; }
		break;
	case 1:
		if (dtu_mission_flag(2)) {
			*next = 2;
			if (has_misc(0x12)) *next = 3;
			r = 1;
		}
		break;
	case 2:
		if (has_misc(0x12)) { *next = 4; r = 1; }
		break;
	case 3: case 4:
		if (dtu_mission_flag(10)) { r = 1; *next = 5; }
		// fall through
	case 5:
		if (has_misc(0x7F)) {
			r = 1; *next = 6;
			if (dtu_mission_flag(3)) *next = 7;
		}
		break;
	case 6:
		if (dtu_mission_flag(3)) { r = 1; *next = 8; }
		break;
	case 7: case 8:
		if (dtu_mission_flag(11)) { r = 1; *next = 9; }
		// fall through
	case 9:
		if (has_misc(0x10)) { r = 1; *next = 10; dtu_char_take(OBJ_MISC, 0x10, 1); }
		break;
	case 10:
		if (has_misc(0x13)) { r = 1; *next = 11; }
		break;
	case 11:
		if (has_misc(0x0F)) { r = 1; *next = 12; dtu_char_take(OBJ_MISC, 0x0F, 1); }
		break;
	case 12:
		if (dtu_mission_flag(12)) { r = 1; *next = 13; }
		// fall through
	case 13:
		if (dtu_mission_flag(4)) { r = 1; *next = 14; }
		break;
	case 14:
		if (dtu_mission_flag(13)) { r = 1; *next = 15; }
		// fall through
	case 15:
		if (dtu_mission_flag(5)) { r = 1; *next = 16; }
		break;
	case 16:
		if (has_misc(0x17)) { r = 1; *next = 17; }
		break;
	case 17:
		if (dtu_mission_flag(6)) { r = 1; *next = 18; }
		break;
	case 18:
		if (dtu_mission_flag(14)) { r = 1; *next = 19; }
		// fall through
	case 19:
		if (dtu_mission_flag(7)) {
			r = 1; *next = 21;
			if (has_misc(0x18)) *next = 20;
		}
		break;
	case 20: case 21:
		if (dtu_mission_flag(15)) { r = 1; *next = 22; }
		// fall through
	case 22:
		if (dtu_mission_flag(8)) { r = 1; *next = 23; }
		// fall through
	case 23:
		if (dtu_mission_flag(9)) { r = 1; *next = 24; }
		break;
	case 24:
		if (has_misc(0x11)) { r = 1; *next = 25; dtu_char_take(OBJ_MISC, 0x11, 1); }
		break;
	case 25: case 26:
		r = 1; *next = 27;
		break;
	case 27:
		r = 1; *next = 28;
		break;
	case 28:
		r = 1; *next = 29;
		break;
	case 29:
	case 30:
		if (m == 29) {
			if (!dtu_mission_flag(19))
				break;
			r = 1; *next = 30;
		}
		for (i = 0, all = 1; i < 7; i++)
			if (!has_misc(pieces[i]))
				all = 0;
		if (all) { r = 1; *next = (m == 29) ? 31 : 32; }
		break;
	case 31: case 32:
		if (dtu_mission_flag(17)) { r = 1; *next = 33; }
		break;
	}
	return r;
}

// 0xB6B86 and 0xB6390: a MISC item for the player
static void give_misc(int id)
{
	dtu_item_data d;
	int count = 1;

	if (dtu_item_get(OBJ_MISC, id, &d) && !d.stackable && d.uses > 0)
		count = d.uses;
	dtu_inventory_add(OBJ_MISC, id, count);
	dtu_log("Khelben gives MISC %d (%s)\n", id, dtu_item_get(OBJ_MISC, id, &d) ? d.name : "?");
	slog("Khelben gives MISC %d (%s)", id, dtu_item_get(OBJ_MISC, id, &d) ? d.name : "?");
}

DTU_WEAK void dtu_hook_market_add_stock(int n)
{
	dtu_log("Khelben: market stock %d is added (not in the port)\n", n);
}

// 0x844ED: the rewards of mission m
static void mission_rewards(int m)
{
	int gold = 0, xp = 0, market = -1, item = -1, item2 = -1;

	switch (m) {
	case 1:  gold = 200; item = 0x26; xp = 200; market = 0; break;
	case 2:  gold = 100; xp = 200; break;
	case 3:  gold = 200; item = 0x6B; xp = 200; market = 1; break;
	case 4:  item = 0x6B; market = 1; break;
	case 5:  gold = 200; xp = 200; break;
	case 6:  gold = 200; break;
	case 7:  gold = 400; item = 0x6C; xp = 400; market = 2; break;
	case 8:  gold = 200; item = 0x6C; xp = 400; market = 2; break;
	case 10: gold = 1500; item = 0x6D; xp = 1000; market = 3; break;
	case 11: item = 0x6E; market = 4; break;
	case 12: gold = 1500; item = 0x6F; xp = 600; break;
	case 13: xp = 250; break;
	case 14: market = 5; item = 0x70; xp = 1000; break;
	case 15: xp = 500; break;
	case 16: item = 0x71; xp = 1000; market = 6; break;
	case 17: gold = 1200; item = 0x72; item2 = 0x73; break;
	case 18: case 19: xp = 1200; break;
	case 20: case 21: gold = 2000; item = 0x74; xp = 1200; market = 7; break;
	case 22: xp = 1400; break;
	case 24: gold = 1200; item = 0x75; xp = 1400; break;
	case 25: case 26:
		dtu_char_take(OBJ_MISC, 0x11, 1);
		gold = 2000; item = 0x76; xp = 1400;
		break;
	case 27: item = 0x77; xp = 1400; break;
	case 29: gold = 2000; item = 0x78; xp = 1600; market = 8; break;
	case 30: xp = 1600; break;
	case 31: case 32: item = 0x79; xp = 1400; break;
	}
	Dtu_gold += gold;
	Dtu_char.pa.gold = Dtu_gold;
	if (item >= 0) give_misc(item);
	if (item2 >= 0) give_misc(item2);
	if (xp) dtu_char_award_xp(xp);
	if (market >= 0) dtu_hook_market_add_stock(market);
	dtu_log("Khelben: mission %d, rewards gold %d xp %d\n", m, gold, xp);
	slog("Khelben: mission %d, rewards gold %d (now %d) item %d xp %d market %d", m, gold, Dtu_gold, item, xp, market);
}

// 0x83E36
int dtu_mission_advance(int mission, int *blurb)
{
	int next, guard = 0;

	dtu_script_init();
	while (guard++ < 40 && mission_switch(mission, &next)) {
		*blurb = 0;
		mission_rewards(next);
		mission = next;
	}
	return mission;
}

static void test_flags(void);

void dtu_mission_new_character(void)
{
	memset(Mission_dw, 0, sizeof(Mission_dw));		// 0x5ADC9
	if (Dtu_fix_script_new_char)
		memset(Gflags, 0, sizeof(Gflags));
	test_flags();
}

#define SG_MISSION		57
#define SG_GFLAGS		(141 + 0x1A33)

void dtu_mission_write_save(ubyte *sg)
{
	int i;

	for (i = 0; i < DTU_N_MISSION_FLAGS; i++) {
		uint v = (uint) Mission_dw[i];
		ubyte *p = sg + SG_MISSION + 4 * i;
		p[0] = (ubyte) v; p[1] = (ubyte) (v >> 8); p[2] = (ubyte) (v >> 16); p[3] = (ubyte) (v >> 24);
	}
	memcpy(sg + SG_GFLAGS, Gflags, sizeof(Gflags));
}

void dtu_mission_read_save(const char *path)
{
	ubyte b[SG_GFLAGS + 32];
	FILE *f = fopen(path, "rb");
	int i;

	if (!f)
		return;
	if (fread(b, 1, sizeof(b), f) == sizeof(b)) {
		for (i = 0; i < DTU_N_MISSION_FLAGS; i++) {
			ubyte *p = b + SG_MISSION + 4 * i;
			Mission_dw[i] = p[0] | (p[1] << 8) | (p[2] << 16) | ((int) p[3] << 24);
		}
		memcpy(Gflags, b + SG_GFLAGS, sizeof(Gflags));
	}
	fclose(f);
}

//------------------------------------------------------------------------
// The state of a run (0x145526 and the other globals of section 2)

typedef struct sc_run {
	int		objnum, signature;
	int		temp;				// 1: the temporary object of run_script_id
	int		id;
	ubyte		*s;
	int		size, p, depth, label;
	int		after_flag0;	// 0x145535
	int		result;			// the result of 0xAAE3E: 0 after DB to another level
	int		pending_reply;	// the reply list of a CD is complete
	int		n_replies;		// 0x145531 + 1
	int		reply_label[SC_MAX_REPLIES];
	int		walk_seg;		// the last segment of an E5
} sc_run;

static sc_run Run[SC_MAX_NEST];
static int N_run = 0;
static int Cur_mode = 0;					// 0x145536 (statement EE does not restore it)
static int Wait_timer = 0;					// an EC waits
static uint Wait_until = 0;

// A change of level of DB, done after the run
static int Warp_level = -1, Warp_seg = -1, Warp_facing = 0;
static int Warp_prison = 0;				// the warp of the prison sequence 0xAA3F3
static int Warp_prison_now = 0;			// 1 while do_warp() runs that warp
// ASCENT: what goes on after script 0x1FD (the talk of WEAPON 13) when it
// had to wait: 1 the talk with an object (0xAC262), 2 a warp (0x59E9D)
static int After_kind = 0, After_obj = -1, After_sig = 0;
static int After_level = -1, After_seg = -1, After_value = 0, After_prison = 0;

// dtuobj.c: behavior 0x80 and AI mode 0 of a creature (0xAA61E)
void dtu_script_prison_guard(object *obj, int set_target);
// dtuobj.c: 1 if the flee value A of the creature (Robot_info +0x80) is
// below its morale (0x59DF2)
int dtu_script_creature_brave(object *obj);
// A run in mode 2 after a walk (Dtu_script_walk_teleport)
static int Walk_obj = -1, Walk_sig = 0, Walk_seg = -1;
static uint Walk_until = 0;

// dtuobj.c: the path of statement E5
void dtu_script_walk_segment(object *obj, int seg);
void dtu_script_walk_start(object *obj);
int dtu_script_walk_state(object *obj);

static object *run_obj(sc_run *r)
{
	object *o;

	if (r->objnum < 0 || r->objnum > Highest_object_index)
		return NULL;
	o = &Objects[r->objnum];
	if (o->signature != r->signature || o->type == OBJ_NONE)
		return NULL;
	return o;
}

//------------------------------------------------------------------------
// Objects

// 0xA66B7, 0xA7962
static object *obj_num(int n)
{
	if (n < 0 || n > Highest_object_index)
		return NULL;
	return &Objects[n];
}

// 0xA66C9: 1 if the object is dead or gone
static int obj_dead(int n)
{
	object *o = obj_num(n);

	if (!o || o->type == OBJ_NONE)
		return 1;
	if (o->shields < F1_0 || (o->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		return 1;
	if (o->type == OBJ_ROBOT && (o->ctype.ai_info.CURRENT_STATE == 7 || o->ctype.ai_info.GOAL_STATE == 7))
		return 1;
	return 0;
}

// 0xA6ACB: kill an object
static void obj_kill(object *o)
{
	if (!o)
		return;
	if (o->type == OBJ_ROBOT)
		dtu_damage_creature_direct(o, NULL, o->shields > 0 ? o->shields + F1_0 : F1_0);
	else if (o->type == OBJ_PLAYER)
		dtu_damage_player_direct(o, NULL, o->shields + F1_0);
	else if (o->type == OBJ_DRESSING)
		// ASCENT: 0xA6B62: scenery breaks. In FLAME object 3 (the
		// Flamesword) ends the game here (0xA6AF8, 0xA6786).
		dtu_break_dressing(o, NULL, 0);
	else if (o != ConsoleObject)
		o->flags |= OF_SHOULD_BE_DEAD;
}

// ASCENT: the end of a run (0xAC033, 0xAA2EE): an object with less than
// 1.0 hit points is killed with 0xA6ACB. The executable has no test of the
// type there: script 322 of FLAME sets the hit points of the Flamesword
// (scenery, object 3) to 0 with the animation statement BC, and this kill
// starts the end of the game. The port does it for creatures and scenery.
static void kill_after_run(object *o)
{
	if (o && o->shields < F1_0 && (o->type == OBJ_ROBOT || o->type == OBJ_DRESSING) &&
		 !(o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED)))
		obj_kill(o);
}

// 0x5A5DE: the facing after a teleport
static void set_facing(object *obj, int value)
{
	vms_matrix *m = &obj->orient;

	if (value < 0 || value > 3)
		return;
	vm_vec_zero(&m->rvec);
	vm_vec_zero(&m->uvec);
	vm_vec_zero(&m->fvec);
	m->uvec.y = F1_0;
	switch (value) {
		case 0:	m->rvec.z = -F1_0;	m->fvec.x = F1_0;		break;
		case 1:	m->rvec.z = F1_0;		m->fvec.x = -F1_0;	break;
		case 2:	m->rvec.x = F1_0;		m->fvec.z = F1_0;		break;
		case 3:	m->rvec.x = -F1_0;	m->fvec.z = -F1_0;	break;
	}
}

// 0x13899, 0x6B9B0, 0x5A5DE: an object to the center of a segment
static void move_to_segment(object *o, int seg, int facing)
{
	vms_vector center;

	if (!o || seg < 0 || seg > Highest_segment_index)
		return;
	compute_segment_center(&center, &Segments[seg]);
	o->pos = center;
	o->last_pos = center;
	obj_relink(o - Objects, seg);
	vm_vec_zero(&o->mtype.phys_info.velocity);
	set_facing(o, facing);
}

// 0xA9F58: the object leaves the level
static void remove_object(object *o)
{
	if (!o || o == ConsoleObject)
		return;
	o->flags |= OF_SHOULD_BE_DEAD;
}

//------------------------------------------------------------------------
// Triggers (0xA829E, 0xA8324, 0xA83BD)

static int find_trigger(int seg, int side)
{
	int n, count = 0, prev = -1;

	if (seg < 0 || seg > Highest_segment_index || seg >= DTU_TRIG_SEG_TABLE)
		return -1;
	n = Dtu_trigger_seg_table[seg];
	while (n >= 0 && n < Dtu_num_triggers && count < 50) {
		if (Dtu_triggers[n].side == side)
			return n;
		if (prev == n)
			count++;
		prev = n;
		n = Dtu_triggers[n].next_in_seg;
	}
	slog("trigger not found: seg %d side %d", seg, side);
	return -1;
}

// The second texture of a side: the same rule as set_side_tmap2 of dtutrig.c
static void side_tmap2(int segnum, int sidenum, short tmap)
{
	segment *segp;
	int tmap2 = tmap & 0x3FFF, orient = tmap & (~0x3FFF);

	if (segnum < 0 || segnum > Highest_segment_index || sidenum < 0 || sidenum >= MAX_SIDES_PER_SEGMENT)
		return;
	segp = &Segments[segnum];
	segp->dtu_tmap_num2[sidenum] = tmap;
	segp->sides[sidenum].tmap_num2 = 0;
	segp->dtu_tmap2_missing &= ~(1 << sidenum);
	if (tmap2 != 0) {
		if (tmap2 < Num_level_tmap_names && Level_tmap_xlate[tmap2] > 0)
			segp->sides[sidenum].tmap_num2 = Level_tmap_xlate[tmap2] | orient;
		else
			segp->dtu_tmap2_missing |= 1 << sidenum;
	}
}

// 0xA8324 (on) and 0xA83BD (off): the trigger and its chain
static void trigger_switch(int n, int on)
{
	int steps = 0, prev = -1;

	while (n >= 0 && n < Dtu_num_triggers && steps < 50) {
		dtu_trigger *t = &Dtu_triggers[n];
		short tex = on ? t->tex_on : t->tex_off;

		t->on = on;
		if (tex != -1)
			side_tmap2(t->seg, t->side, tex);
		if (n == prev)
			steps++;
		prev = n;
		n = on ? t->enable_trigger : t->disable_trigger;
	}
}

//------------------------------------------------------------------------
// Operands (section 3.1)

static int at_end(sc_run *r)
{
	return r->p >= r->size;
}

static int rd(sc_run *r)
{
	return r->p < r->size ? r->s[r->p++] : 0;
}

static int peek(sc_run *r)
{
	return r->p < r->size ? r->s[r->p] : 0;
}

// 0xA76B9
static int hexnum(sc_run *r)
{
	char buf[16];
	int n = 0;
	unsigned v = 0;

	while (r->p < r->size && r->s[r->p] != 0) {
		if (n < 15)
			buf[n++] = r->s[r->p];
		r->p++;
	}
	if (r->p < r->size)
		r->p++;
	buf[n] = 0;
	if (sscanf(buf, "%x", &v) != 1)
		v = 0;
	return v & 0xffff;
}

static int num(sc_run *r);

static object *cur_obj(sc_run *r)
{
	object *o = run_obj(r);
	return o ? o : ConsoleObject;
}

// An optional "15 NUM" object prefix
static object *obj_prefix(sc_run *r)
{
	if (peek(r) == 0x15) {
		r->p++;
		return obj_num(num(r));
	}
	return cur_obj(r);
}

// 0xA7DF0
static int num(sc_run *r)
{
	int b = rd(r);

	if (b == 0xD2) {
		int a = hexnum(r), c = hexnum(r), lo, hi;
		if (c == 0) { lo = 1; hi = a; }
		else { lo = a; hi = c; }
		return (rand() % ((hi - lo) & 0xffff ? (hi - lo + 1) : 1) + lo) & 0xffff;
	}
	if (b == 0xE0) {
		object *o = obj_prefix(r);
		return o ? (o->dtu.m_1b0 & SB_VAR) >> SB_VAR_SHIFT : 0;
	}
	r->p--;
	return hexnum(r);
}

// 0xA8DB2
static int arith(sc_run *r, int old)
{
	int op = rd(r), v = num(r);

	switch (op) {
		case 0x7A: return old + v;
		case 0x7B: return old - v;
		case 0x7C: return old * v;
		case 0x7D: return old / (v > 1 ? v : 1);
		case 0x7E: return v;
	}
	return old;
}

// 0xA7BEE: unsigned 16-bit compare
static int compare(sc_run *r, int left)
{
	int op = rd(r), right;
	unsigned a = left & 0xffff, b;

	if (peek(r) == 0xEF) {
		r->p++;
		right = Dtu_char.pa.max_hit_points;
	} else
		right = num(r);
	b = right & 0xffff;
	switch (op) {
		case 0x7F: return a < b;
		case 0x80: return a <= b;
		case 0x81: return a == b;
		case 0x82: return a != b;
		case 0x83: return a > b;
		case 0x84: return a >= b;
	}
	return 0;
}

// 0xA7582: the words of the tokens 0x09 to 0x11
static char *token_word(int code)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int c = pa->class1 >= 0 && pa->class1 <= 3 ? pa->class1 : 2;
	int race = (pa->flags & DTU_PF_HALF_ELF) ? 5 : (pa->race >= 0 && pa->race <= 4 ? pa->race : 0);
	int sex = pa->female ? 1 : 0;
	static short cls_a[4] = { 642, 643, 644, 647 }, cls_b[4] = { 906, 907, 908, 909 };
	static short cls_c[4] = { 692, 910, 911, 912 }, cls_d[4] = { 913, 914, 915, 916 };
	static short race1[6] = { 649, 650, 651, 957, 653, 917 }, race2[6] = { 954, 955, 956, 957, 958, 959 };

	switch (code) {
		case 0x09: return Dtu_char.name;
		case 0x0A: return Text_string[(rand() % 2) ? cls_a[c] : cls_c[c]];
		case 0x0B: return Text_string[(rand() % 2) ? cls_b[c] : cls_d[c]];
		case 0x0C:
			// ASCENT: TEX 957 "drow" is the word of the drow for {race} and
			// for {races}. A translation has a singular and a plural: the
			// singular is the text of the code "drow" (when the file has the
			// English text of the catalog), TEX 957 the plural.
			if (race1[race] == 957 && !strcmp(dtu_lang_text_en(957), "drow"))
				return LT("drow");
			return Text_string[race1[race]];
		case 0x0D: return Text_string[race2[race]];
		case 0x0E: case 0x0F: case 0x10: case 0x11:
			return Text_string[918 + 2 * (code - 0x0E) + sex];
	}
	return "";
}

// 0xA76ED: one string with the tokens replaced
static void expand(const char *s, char *out)
{
	int n = 0, start = 1;

	while (*s && n < SC_TEXT_SIZE - 1) {
		int c = (ubyte) *s++;
		if (c >= 9 && c <= 0x11) {
			char *w = token_word(c);
			int k;
			for (k = 0; w[k] && n < SC_TEXT_SIZE - 1; k++) {
				char ch = w[k];
				if (k == 0 && (start || c == 9) && ch >= 'a' && ch <= 'z')
					ch -= 'a' - 'A';
				out[n++] = ch;
			}
			start = 0;
			continue;
		}
		start = (c == '.' || c == '!' || c == '?' || (start && c == ' '));
		out[n++] = (char) c;
	}
	out[n] = 0;
}

// 0xA77BE: a text operand, one string chosen at random
static void text(sc_run *r, char *out)
{
	int starts[SC_MAX_ALTS], n = 0;

	while (r->p < r->size && r->s[r->p] < 0x7A) {
		if (n < SC_MAX_ALTS)
			starts[n++] = r->p;
		while (r->p < r->size && r->s[r->p] != 0)
			r->p++;
		if (r->p < r->size)
			r->p++;
	}
	if (n == 0) {
		out[0] = 0;
		return;
	}
	{
		// ASCENT: the translation of the string (dtulang.h): the id is
		// gab.SCRIPT.OFFSET, the offset of the string in the record
		int k = starts[rand() % n];
		char id[32];
		snprintf(id, sizeof(id), "gab.%d.%d", r->id & SB_SCRIPT, k);
		expand(dtu_tr_id(id, (const char *) r->s + k), out);
		dtu_lang_sex_text(out);		// the tokens {sex:} (the lookup resolved those of the catalog)
	}
}

//------------------------------------------------------------------------
// Messages

// 0x347A2: a HUD message (the executable gives it the color of the
// speaker, signature % 8).
static void hud_text(char *t)
{
	if (t[0])
		HUD_init_message("%s", t);
	slog("message \"%s\"", t);
}

// 0xA7872: goto a label. Returns 1 if it was found.
static int sc_goto(sc_run *r, int label)
{
	r->p = 0;
	r->depth = 0;
	while (r->p < r->size) {
		if (r->s[r->p++] == 0xCB) {
			if (hexnum(r) == label) {
				r->label = label;
				return 1;
			}
		}
	}
	return 0;
}

// 0xA78CF: the label "CB E7 seg"
static int sc_goto_seg(sc_run *r, int seg)
{
	r->p = 0;
	r->depth = 0;
	while (r->p < r->size) {
		if (r->s[r->p++] == 0xCB && peek(r) == 0xE7) {
			r->p++;
			if (hexnum(r) == seg) {
				r->label = 0;
				return 1;
			}
		}
	}
	return 0;
}

// 0xA8D72: one line of the current label as a HUD message
static void show_line(sc_run *r)
{
	char t[SC_TEXT_SIZE];

	text(r, t);
	hud_text(t);
}

// 0xA7A57: the exit label, one of its lines half of the time
static void exit_label(sc_run *r)
{
	object *o = run_obj(r);
	int lab = 0x3EC;

	if (o) {
		if (o->dtu.m_1b0 & SB_FLAG(2))		lab = 0x3EA;
		else if (o->dtu.m_1b0 & SB_FLAG(3))	lab = 0x3EB;
		else if (o->dtu.m_1b0 & SB_FLAG(1))	lab = 0x3E9;
	}
	if (sc_goto(r, lab) && rand() % 2)
		show_line(r);
}

// 0xA8C49: skips to the ELSE (after it) or the ENDIF (on it) of the depth,
// or to the code "code".
static int sc_skip(sc_run *r, int code)
{
	int target = code, endif = 0xF8 + r->depth;

	if (code == 0xF4 || code == 0xF9)
		target = code + r->depth - 1;
	while (r->p < r->size) {
		int b = r->s[r->p++];
		if (b == target)
			return 1;
		if (code == 0xF4 && b == endif) {
			r->p--;
			return 1;
		}
	}
	return 0;
}

//------------------------------------------------------------------------
// Conditions (0xA8BDB, 0xA8516)

static int sc_term(sc_run *r);

static int item_type(int c)
{
	static short t[5] = { OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC, OBJ_SPELL, 0xA8 };
	return (c >= 0xA4 && c <= 0xA8) ? t[c - 0xA4] : -1;
}

// 0xA7C81: the current object carries the item
static int carries(sc_run *r, int type, int id)
{
	object *o = run_obj(r);
	return o && o->contains_type == type && o->contains_id == id && o->contains_count >= 1;
}

static int anger_of(object *o)
{
	return (o && o->type == OBJ_ROBOT) ? (signed char) *dtu_script_anger(o) : 0;
}

// The NPC list (0x3D42CC): every robot, and every other object with a script
static int npc_in_list(object *o)
{
	return o->type == OBJ_ROBOT || (o->dtu.m_1b0 & SB_SCRIPT) != 0;
}

// 0xA7F9B: the robot id of a critter index, by the name. -1 if none.
static int critter_robot(int who);

static int range_of(sc_run *r)
{
	if (peek(r) == 0xCA) {
		r->p++;
		return 0xFFFF;
	}
	return num(r);
}

// 0xA80F6: any NPC in range with that anger
static int anger_test(sc_run *r)
{
	int who = rd(r), range = range_of(r), op = rd(r), v = num(r), i, res = 0;
	int rid = who == 0xCA ? -1 : critter_robot(who);
	object *me = cur_obj(r);

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];
		int a;
		if (o->type == OBJ_NONE || !npc_in_list(o))
			continue;
		if (f2i(vm_vec_dist(&o->pos, &me->pos)) > range)
			continue;
		if (who != 0xCA && o->id != rid)
			continue;
		a = anger_of(o);
		switch (op) {
			case 0x7F: res |= a < v; break;
			case 0x80: res |= a <= v; break;
			case 0x81: res |= a == v; break;
			case 0x82: res |= a != v; break;
			case 0x83: res |= a > v; break;
			case 0x84: res |= a >= v; break;
		}
	}
	return res;
}

static int sc_term(sc_run *r)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int neg = 0, b, v;
	object *o;

	if (peek(r) == 0xE9) {
		r->p++;
		neg = 1;
	}
	b = rd(r);
	if (b <= 7) {
		o = obj_prefix(r);
		v = o ? (o->dtu.m_1b0 & SB_FLAG(b)) != 0 : 0;
	}
	else if (b >= 0x85 && b <= 0x8A)
		v = pa->alignment == b - 0x85;
	else if (b >= 0x8B && b <= 0x8E)
		v = pa->class1 == b - 0x8B || pa->class2 == b - 0x8B;
	else if (b >= 0x8F && b <= 0x94)
		v = ((pa->flags & DTU_PF_HALF_ELF) ? 5 : pa->race) == b - 0x8F;
	else if (b == 0x95 || b == 0x96)
		v = (pa->female ? 1 : 0) == b - 0x95;
	else switch (b) {
		case 0x97: o = obj_prefix(r); v = compare(r, o ? (o->dtu.m_1b0 & SB_TALKS) >> SB_TALKS_SHIFT : 0); break;
		case 0x98: o = run_obj(r); v = compare(r, 500); break;		// morale + 500: the morale is in dtuobj.c, the port uses 0
		case 0x99: v = compare(r, pa->strength); break;
		case 0x9A: v = compare(r, pa->dexterity); break;
		case 0x9B: v = compare(r, pa->constitution); break;
		case 0x9C: v = compare(r, pa->intelligence); break;
		case 0x9D: v = compare(r, pa->wisdom); break;
		case 0x9E: v = compare(r, pa->charisma); break;
		case 0x9F: v = compare(r, pa->class1_level + 1); break;
		case 0xA0: v = compare(r, dtu_char_hit_points()); break;
		case 0xA1: v = compare(r, Fe_khelben_mission + 1); break;
		case 0xA2: v = compare(r, r->objnum); break;
		case 0xA3: v = compare(r, Cur_mode); break;
		case 0xC2: v = compare(r, anger_of(run_obj(r))); break;
		case 0xC3: v = anger_test(r); break;
		case 0xC4: {
			int seg = num(r), side = rd(r) - 0xAD, t = find_trigger(seg, side);
			v = t >= 0 ? Dtu_triggers[t].on != 0 : 0;
			break;
		}
		case 0xC9: v = obj_dead(num(r)); break;
		case 0xD0: {
			int t = item_type(rd(r)), id = num(r);
			v = (t == 0xA8) ? 1 : (t >= 0 ? carries(r, t, id) : 0);
			break;
		}
		case 0xD1: {
			int t = item_type(rd(r)), id = num(r);
			v = (t == 0xA8) ? (id <= Dtu_gold) : (t >= 0 ? dtu_char_has(t, id) : 0);
			break;
		}
		case 0xDF: o = obj_prefix(r); v = compare(r, o ? (o->dtu.m_1b0 & SB_RESUME) >> SB_RESUME_SHIFT : 0); break;
		case 0xE0: o = obj_prefix(r); v = compare(r, o ? (o->dtu.m_1b0 & SB_VAR) >> SB_VAR_SHIFT : 0); break;
		case 0xE7: o = run_obj(r); v = compare(r, o ? o->segnum : 0); break;
		case 0xE8: {
			int seg = num(r), side = rd(r) - 0xAD, t = find_trigger(seg, side);
			// 0xA8456: with no trigger the compare is not read
			v = t >= 0 ? compare(r, Dtu_triggers[t].fire_count) : 0;
			break;
		}
		case 0xF0: {
			int seg = num(r);
			v = seg <= Highest_segment_index ? Automap_visited[seg] != 0 : 0;
			break;
		}
		case 0xF1: {
			int n = num(r);
			// 0xA8904: flags 1 to 19 are copies of the mission flags
			if (n >= 1 && n <= 19)
				dtu_script_set_gflag(n, dtu_mission_flag(n) & 0xff);
			v = dtu_script_gflag(n);
			break;
		}
		default:
			r->p--;
			v = num(r);
			v = compare(r, v);
			break;
	}
	v = v != 0;
	return neg ? !v : v;
}

static int sc_cond(sc_run *r)
{
	int v = sc_term(r);

	while (!at_end(r)) {
		int b = rd(r);
		if (b == 0xE1)
			v &= sc_term(r);
		else if (b == 0xE2)
			v |= sc_term(r);
		else {
			if (b == 0xF3)
				r->depth++;
			break;
		}
	}
	return v;
}

// 0xA8C3A
static void sc_if(sc_run *r)
{
	if (!sc_cond(r))
		sc_skip(r, 0xF4);
}

//------------------------------------------------------------------------
// Statements

// 0xA8F77 (GIVE) and 0xA9191 (TAKE)
static void give_take(sc_run *r, int give)
{
	int t = item_type(rd(r)), id = num(r), k = peek(r), seg = 0xFFFF, i;
	object *o = run_obj(r);

	if (k == 0xE4 || k == 0xE3) {
		r->p++;
		seg = num(r);
	} else
		k = 0;
	slog("%s class %d id %d %s seg %d", give ? "give" : "take", t, id, k == 0xE4 ? "E4" : k == 0xE3 ? "E3" : "", seg);
	if (t == 0xA8) {
		int before = Dtu_gold;
		if (give)
			Dtu_gold += id;
		else if (id <= Dtu_gold)
			Dtu_gold -= id;
		Dtu_char.pa.gold = Dtu_gold;
		slog("gold %d -> %d", before, Dtu_gold);
		return;
	}
	if (t < 0)
		return;
	if (give) {
		if (k == 0xE4) {
			if (seg == 0xFFFF) {
				dtu_item_data d;
				int count = 1;
				if (dtu_item_get(t, id, &d) && !d.stackable && d.uses > 0)
					count = d.uses;
				dtu_inventory_add(t, id, count);			// 0xB6390
				return;
			}
			if (seg > Highest_segment_index)
				return;
			for (i = Segments[seg].objects; i != -1; i = Objects[i].next)
				if (Objects[i].type == t && Objects[i].id == id) {
					// 0x29455: the player picks the object up
					dtu_inventory_add(t, id, Objects[i].contains_count > 0 ? 1 : 1);
					Objects[i].flags |= OF_SHOULD_BE_DEAD;
					return;
				}
			return;
		}
		if (!carries(r, t, id))
			return;
		if (k == 0xE3) {
			if (seg <= Highest_segment_index) {
				vms_vector c;
				compute_segment_center(&c, &Segments[seg]);
				dtu_create_item(t, id, 1, seg, &c);
			}
		} else {
			dtu_item_data d;
			int count = 1;
			if (dtu_item_get(t, id, &d) && !d.stackable && d.uses > 0)
				count = d.uses;
			dtu_inventory_add(t, id, count);
		}
		if (o)
			o->contains_count--;
		return;
	}
	// take
	if (k == 0xE4) {
		if (seg <= Highest_segment_index)
			for (i = Segments[seg].objects; i != -1; i = Objects[i].next)
				if (Objects[i].type == t && Objects[i].id == id) {
					Objects[i].flags |= OF_SHOULD_BE_DEAD;		// 0x68F45
					break;
				}
		if (o) {											// 0xA7CB3
			o->contains_type = t;
			o->contains_id = id;
			o->contains_count++;
		}
		return;
	}
	if (!dtu_char_has(t, id))
		return;
	if (k == 0xE3) {
		if (seg == 0xFFFE) {
			// a random robot or dressing object with no item carries it
			for (i = 0; i <= Highest_object_index; i++) {
				object *x = &Objects[(rand() % (Highest_object_index + 1))];
				if (!(x->flags & 7) && (x->type == OBJ_ROBOT || x->type == OBJ_DRESSING) && x->contains_count <= 0) {
					x->contains_count = 1;
					x->contains_type = t;
					x->contains_id = id;
					slog("%s has it at %d", dtu_object_name(x), x->segnum);
					break;
				}
			}
		} else if (seg != 0xFFFF && seg <= Highest_segment_index) {
			vms_vector c;
			compute_segment_center(&c, &Segments[seg]);
			dtu_create_item(t, id, 1, seg, &c);
		}
	} else if (o) {
		o->contains_type = t;
		o->contains_id = id;
		o->contains_count++;
	}
	dtu_char_take(t, id, 1);							// 0xB6D0C
}

// 0xA8E77
static void experience(sc_run *r)
{
	int op = rd(r), v = num(r), classes = 1, i;
	dtu_player_attr *pa = &Dtu_char.pa;

	if (pa->class2 >= 0 && pa->class2 <= 3)
		classes = 2;
	if (op == 0x7A) {
		dtu_char_award_xp(v);				// the same split and level check
		return;
	}
	if (op == 0x7B) {
		v /= classes;
		if (v < 1) v = 1;
	}
	for (i = 0; i < classes; i++) {
		int x = Dtu_char.experience[i];
		switch (op) {
			case 0x7B: x -= v; break;
			case 0x7C: x *= v; break;
			case 0x7D: x /= (v > 1 ? v : 1); break;
			case 0x7E: x = v; break;
		}
		Dtu_char.experience[i] = x;
	}
	dtu_char_award_xp(0);
}

// The anger of an object changed (0xAAE9D, 0xA94CE)
static void anger_changed(object *o)
{
	int a = anger_of(o);

	if (a >= 5) {
		if (!(o->dtu.m_1b0 & SB_FLAG(3)) && !(o->dtu.m_1b0 & SB_NO_HOSTILE))
			dtu_script_make_hostile(o);
	} else if (a == 0) {
		o->dtu.m_1b0 &= ~SB_FLAG(2);
	}
}

static void set_anger(object *o, int v)
{
	if (o && o->type == OBJ_ROBOT)
		*dtu_script_anger(o) = (ubyte) v;
}

// 0xA94CE
static void anger_npcs(sc_run *r)
{
	int who = rd(r), range = range_of(r), op = rd(r), v = num(r), i;
	int rid = who == 0xCA ? -1 : critter_robot(who);
	object *me = cur_obj(r);

	for (i = 0; i <= Highest_object_index; i++) {
		object *o = &Objects[i];
		int a;
		if (o->type != OBJ_ROBOT || !npc_in_list(o))
			continue;
		if (f2i(vm_vec_dist(&o->pos, &me->pos)) > range)
			continue;
		if (who != 0xCA && o->id != rid)
			continue;
		a = anger_of(o);
		switch (op) {
			case 0x7A: a += v; if (a > 5) a = 5; break;
			case 0x7B: a = a > v ? a - v : 0; break;
			case 0x7C: a *= v; if (a > 5) a = 5; break;
			case 0x7D: a /= (v > 1 ? v : 1); break;
			case 0x7E: a = v < 5 ? v : 5; break;
		}
		set_anger(o, a);
		if (a >= 5) {
			if (!(o->dtu.m_1b0 & SB_FLAG(3)) && !(o->dtu.m_1b0 & SB_NO_HOSTILE))
				dtu_script_make_hostile(o);
		} else if (a == 0) {
			o->dtu.m_1b0 &= ~SB_FLAG(2);
		}
	}
}

// C5, C6 (0xAAFAA)
static void set_flag(sc_run *r, int set)
{
	int k = rd(r);

	if (k <= 7) {
		object *o = obj_prefix(r);
		if (!o)
			return;
		if (set)
			o->dtu.m_1b0 |= SB_FLAG(k);
		else
			o->dtu.m_1b0 &= ~SB_FLAG(k);
		slog("%s flag %d of obj %d", set ? "set" : "clear", k, (int) (o - Objects));
		if (k == 2) {
			if (set) {
				if (!(o->dtu.m_1b0 & SB_FLAG(3)))
					dtu_script_make_hostile(o);
			} else
				set_anger(o, 0);
		}
		return;
	}
	k = num(r);
	if (k <= 0x20)
		return;
	dtu_script_set_gflag(k, set);
	slog("%s global flag %d", set ? "set" : "clear", k);
	if (k >= 0x21 && k <= 0x33)
		dtu_mission_set_flag(k - 0x20, set);
}

// The ability statements 99 to 9E
static void ability(sc_run *r, int code)
{
	dtu_player_attr *pa = &Dtu_char.pa, *base = &Dtu_char.base;
	ushort *p = NULL, *q = NULL;
	int old, v;

	switch (code) {
		case 0x99: p = &pa->strength; q = &base->strength; break;
		case 0x9A: p = &pa->dexterity; q = &base->dexterity; break;
		case 0x9B: p = &pa->constitution; q = &base->constitution; break;
		case 0x9C: p = &pa->intelligence; q = &base->intelligence; break;
		case 0x9D: p = &pa->wisdom; q = &base->wisdom; break;
		case 0x9E: p = &pa->charisma; q = &base->charisma; break;
	}
	old = *p;
	v = arith(r, old) & 0xffff;
	*p = (ushort) v;
	if (Dtu_fix_script_abilities)
		*q = (ushort) (*q + (v - old));
	slog("ability %02X: %d -> %d", code, old, v);
	if (code == 0x99 && (pa->class1 == DTU_CLASS_FIGHTER || pa->class2 == DTU_CLASS_FIGHTER)) {
		// 0xAB514
		if (old < 18 && v == 18)
			pa->strength_extra = rand() % 100;
		else if (old == 18 && pa->strength_extra > 0) {
			pa->strength_extra += 10;
			if (pa->strength_extra > 99)
				pa->strength_extra = 0;
		}
		if (Dtu_fix_script_abilities)
			base->strength_extra = pa->strength_extra;
	}
	if (code == 0x9B) {
		// 0x8C01B: the hit point bonus of the constitution
		static signed char fighter[20] = { 0,-3,-2,-2,-1,-1,-1,0,0,0,0,0,0,0,0,1,2,3,4,5 };
		static signed char other[20]   = { 0,-3,-2,-2,-1,-1,-1,0,0,0,0,0,0,0,0,1,2,2,2,2 };
		int c = v < 1 ? 1 : v > 19 ? 19 : v;
		int f = pa->class1 == DTU_CLASS_FIGHTER || pa->class2 == DTU_CLASS_FIGHTER;
		pa->max_hit_points += f ? fighter[c] : other[c];
	}
}

// A0 (0xAB7AA)
static void hit_points(sc_run *r)
{
	int old = dtu_char_hit_points(), v = arith(r, old);

	slog("hit points %d -> %d", old, v);
	if (v < old) {
		if (ConsoleObject)
			dtu_damage_player_direct(ConsoleObject, NULL, i2f(old - v));
	} else {
		if (v > dtu_char_max_hit_points())
			v = dtu_char_max_hit_points();
		dtu_char_heal(v - old);
	}
}

// B3 to BD (0xA9E2B)
static void anim(sc_run *r, int state)
{
	object *o = run_obj(r);

	if (!o)
		return;
	if (state == DAS_DIE) {
		o->shields = 0;
		return;
	}
	if (o->type == OBJ_ROBOT)
		o->control_type = CT_AI;
	o->dtu.m_88 &= ~0x4000;
	DTU_ANIM_STATE(o) = state;
	DTU_ANIM_GOAL(o) = state;
	DTU_ANIM_FRAME(o) = Dtu_anim_first[state];
}

// The volume of D3 to D5 (0xA9C59): 0x10000 / (11 - volume), volume 1 to 10
static fix sound_volume(int vol)
{
	if (vol < 1) vol = 1;
	if (vol > 10) vol = 10;
	return F1_0 / (10 - vol + 1);
}

// D3 (0xA97B3)
static void sound(int snd, int vol)
{
	if (snd < 0 || snd >= MAX_SOUNDS)
		return;
	digi_play_sample(snd, sound_volume(vol));
}

// D4 (0xA9BE6): a sound at an object (0x2E627), "dist" is the distance in
// whole units at which it is heard; with D9 it plays until D6 stops it
static void sound_at_object(int snd, int vol, int objnum, int dist, int loop)
{
	if (snd < 0 || snd >= MAX_SOUNDS || !obj_num(objnum) || Objects[objnum].type == OBJ_NONE)
		return;
	digi_link_sound_to_object2(snd, objnum, loop, sound_volume(vol), i2f(dist));
}

static void prison_sequence(sc_run *r);

// DB (0xAA1FC)
static int teleport_player(sc_run *r)
{
	int level = num(r), seg = num(r), facing = rd(r) - 0xA9;
	int cur = Current_level_num - 1;

	slog("teleport PC to level %d seg %d facing %d", level, seg, facing);
	if (seg == 0xFFFF) {
		prison_sequence(r);						// ASCENT: 0xAA3F3
		return 0;
	}
	if (level == cur) {
		move_to_segment(ConsoleObject, seg, facing);
		return 1;
	}
	Warp_level = level;
	Warp_seg = seg;
	Warp_facing = facing;
	Warp_prison = 0;
	return 0;
}

static int carries_nothing(object *o)
{
	return o->contains_type < 0 || o->contains_count <= 0;
}

// ASCENT: the prison sequence 0xAA3F3 (statement DB with segment 0xFFFF;
// script 60 of THIEF1, the Shadow Thieves catch the player). The player
// loses the gold and has 1 hit point, the hand is empty (0x43A5A(-3)), and
// each item of the item list that is not worn (byte +0x0A of the entry is
// 0) is taken: with one chance in two the object of the script carries it
// if it carries nothing, else with one chance in two object 99 (the chest
// of segment 635) if it is whole and carries nothing, else an item object
// lies in one of eight segments in turn (table 0xA643F). Then the player
// goes to level 7 (THIEF2), segment 612, facing west, and the rest of
// 0xAA5BA runs there (prison_arrival). The key items (MISC 0x6B to 0x7C,
// the list at P+0x18D4) are not in the item list and stay.
static void prison_sequence(sc_run *r)
{
	static short segs[8] = { 297, 53, 65, 52, 331, 330, 329, 328 };		// 0xA643F
	object *o = run_obj(r), *chest = obj_num(99);
	int i, turn = 0, n_taken = 0;

	slog("the prison sequence 0xAA3F3");
	dtulog_event("Script: the prison sequence of the Shadow Thieves (0xAA3F3)");
	Dtu_gold = 0;
	Dtu_char.pa.gold = 0;
	if (dtu_char_hit_points() > 1 && ConsoleObject)
		dtu_damage_player_direct(ConsoleObject, NULL, i2f(dtu_char_hit_points() - 1));
	if (dtu_char_hit_points() < 1)
		dtu_char_add_hit_points(1 - dtu_char_hit_points());
	dtu_char_set_current(-1);
	for (i = 0; i < Dtu_n_inventory; i++) {
		dtu_inv_item e = Dtu_inventory[i];
		int count;

		if (dtu_char_slot_of(i) >= 0)
			continue;
		dtu_inventory_remove(i, e.count > 0 ? e.count : 1);		// 0xB6D24: the entries after it move down
		if (!Dtu_fix_prison_items) {
			// the executable reads the entry that is now at this place
			if (i >= Dtu_n_inventory)
				continue;
			e = Dtu_inventory[i];
		} else
			i--;
		n_taken++;
		count = e.count > 0 ? e.count : 1;
		if (rand() % 2 == 0 && o && carries_nothing(o)) {
			o->contains_type = e.type;
			o->contains_id = e.id;
			o->contains_count = count;
			slog("prison: type %d id %d goes to obj %d", e.type, e.id, r->objnum);
		} else if (rand() % 2 == 0 && !obj_dead(99) && chest && carries_nothing(chest)) {
			chest->contains_type = e.type;
			chest->contains_id = e.id;
			chest->contains_count = count;
			slog("prison: type %d id %d goes to obj 99", e.type, e.id);
		} else if (segs[turn] <= Highest_segment_index) {
			vms_vector c;

			compute_segment_center(&c, &Segments[segs[turn]]);
			if (dtu_create_item(e.type, e.id, count, segs[turn], &c) >= 0) {
				slog("prison: type %d id %d lies in seg %d", e.type, e.id, segs[turn]);
				turn = (turn + 1) % 8;
			}
		}
	}
	dtu_char_update();
	slog("prison: %d items taken", n_taken);
	Warp_level = 7;
	Warp_seg = 0x264;
	Warp_facing = 1;
	Warp_prison = 1;
}

// The end of 0xAA3F3, in THIEF2 after the warp: a creature of critter row
// 0xA5 comes into segment 631, facing west, with script 0x37, the player
// as its target, behavior 0x80, AI mode 0 and not hostile. Object 20 gets
// behavior 0x80, AI mode 0 and is not hostile.
static void prison_arrival(void)
{
	object *g;
	int rid = critter_robot(0xA5), n = -1;

	if (rid >= 0 && 0x277 <= Highest_segment_index)
		n = dtu_gate_in_creature(rid, 0x277);
	slog("prison: guard critter 0xA5 (robot %d) is obj %d", rid, n);
	if (n >= 0) {
		g = &Objects[n];
		set_facing(g, 1);
		g->dtu.m_1b0 = 0x37;
		dtu_script_prison_guard(g, 1);
	}
	g = obj_num(20);
	if (g && g->type == OBJ_ROBOT) {
		g->dtu.m_1b0 &= ~SB_FLAG(2);
		dtu_script_prison_guard(g, 0);
	}
}

// The critter table (0xA504A): index -> name. The table of the NPC rows
// that dtuspell.c includes has the same order.
static struct { char *name; int v[6]; } Npc_rows[] = {
#include "dtunpc.h"
};

static int critter_robot(int who)
{
	int i, n;

	if (who < 0 || who >= (int) (sizeof(Npc_rows) / sizeof(Npc_rows[0])))
		return -1;
	n = (int) strlen(Npc_rows[who].name);
	for (i = 0; i < N_robot_types; i++)
		if (!sc_strcmpi(Monster_info[i].name, Npc_rows[who].name, n))
			return i;
	return -1;
}

// DE (0xAA14D)
static void summon(int who, int seg, int facing)
{
	int rid, n;

	if (seg > Highest_segment_index)
		return;
	if (who >= 0xBE) {
		slog("summon %d: the stub 0xA66B4", who);
		return;
	}
	rid = critter_robot(who);
	slog("summon npc %d (robot %d) seg %d", who, rid, seg);
	if (rid < 0)
		return;
	n = dtu_gate_in_creature(rid, seg);
	if (n >= 0)
		set_facing(&Objects[n], facing);
	else
		slog("couldn't gate in critter");
}

static int start_run(object *obj, int id, int mode, int temp);

// The statement loop (0xAAE3E). Returns SC_DONE, SC_WAIT or SC_CALL.
static int sc_exec(sc_run *r)
{
	char t[SC_TEXT_SIZE];
	object *o;
	int b, v;

	while (r->p < r->size) {
		int at = r->p;

		b = rd(r);
		if (b >= 0xF4 && b < 0xF9) b = 0xF4;		// 0xA8CA6
		if (b >= 0xF9 && b < 0xFE) b = 0xF9;
		o = run_obj(r);

		if (b >= 0x99 && b <= 0x9E) {
			ability(r, b);
			continue;
		}
		if (b >= 0xB3 && b <= 0xBD) {
			anim(r, b - 0xB3);
			continue;
		}
		switch (b) {
		case 0x9F: {
			dtu_player_attr *pa = &Dtu_char.pa;
			int lv = (pa->class1_level > pa->class2_level ? pa->class1_level : pa->class2_level) + 1;
			v = arith(r, lv) & 0xffff;
			slog("level %d -> %d", lv, v);
			dtu_char_set_level(v);					// ASCENT: 0xAB799, 0x8BA3A
			break;
		}
		case 0xA0:
			hit_points(r);
			break;
		case 0xBE:								// 0xA79FD
			text(r, t);
			if (!dtu_dlg_is_open() && Cur_mode == 0)
				dtu_dlg_open();
			if (dtu_dlg_is_open())
				dtu_dlg_add_text(t, 1);
			else
				hud_text(t);
			break;
		case 0xBF:
		case 0xC0:
			give_take(r, b == 0xBF);
			break;
		case 0xC1:
			experience(r);
			break;
		case 0xC2:								// 0xAAE9D
			v = arith(r, anger_of(o));
			if ((v & 0xffff) >= 5) v = 5;
			set_anger(o, v);
			if (o) {
				if (v >= 5) {
					if (!(o->dtu.m_1b0 & SB_FLAG(3)))
						dtu_script_make_hostile(o);
				} else if (v == 0) {
					o->dtu.m_1b0 &= ~SB_FLAG(2);
				}
			}
			slog("anger of obj %d = %d", r->objnum, v);
			break;
		case 0xC3:
			anger_npcs(r);
			break;
		case 0xC4: {								// 0xA9755
			int seg = num(r), side = rd(r) - 0xAD, n = find_trigger(seg, side);
			if (n < 0)
				break;
			slog("trigger seg %d side %d (%s)", seg, side, Dtu_triggers[n].on ? "fire" : "switch on");
			if (!Dtu_triggers[n].on)
				trigger_switch(n, 1);
			else
				dtu_trig_check(seg, side, DTU_ACT_ROBOT, o ? o : ConsoleObject);
			break;
		}
		case 0xC5:
		case 0xC6:
			set_flag(r, b == 0xC5);
			break;
		case 0xC7:
			v = num(r);
			slog("goto %X", v);
			sc_goto(r, v);
			break;
		case 0xC8:
		case 0xCC:								// 0xAB256
			exit_label(r);
			return SC_DONE;
		case 0xCD:								// 0xA8CBC
			text(r, t);
			if (!dtu_dlg_is_open())
				dtu_dlg_open();
			dtu_dlg_add_text(t, 0);
			r->n_replies = 0;
			while (1) {
				if (at_end(r))
					return SC_DONE;
				b = rd(r);
				if (b >= 0xF4 && b < 0xF9) b = 0xF4;
				if (b >= 0xF9 && b < 0xFE) b = 0xF9;
				if (b == 0xF2)
					sc_if(r);
				else if (b == 0xF4) {
					sc_skip(r, 0xF9);
					r->p--;
				}
				else if (b == 0xF9)
					r->depth--;
				else if (b == 0xCF) {				// 0xA7B56
					int lab = num(r);
					text(r, t);
					if (r->n_replies < SC_MAX_REPLIES) {
						r->reply_label[r->n_replies++] = lab;
						dtu_dlg_add_reply(t);
					}
				}
				else if (b == 0xCE) {				// 0xA7ADC
					dtu_dlg_start_choice();
					r->pending_reply = 1;
					return SC_WAIT;
				}
				else {
					printf("RLH: * Don't know say code %d (%X) *\n", b, b);
					return SC_DONE;
				}
			}
		case 0xD3: {
			int snd = num(r), vol = num(r);
			if (peek(r) == 0xD8) r->p++;
			slog("sound %d volume %d", snd, vol);
			if (snd == 0xFFFF) {
				dtumovie_flame();		// ASCENT: 0xA9A5B, the movie FLAM1.MVE (dtumovie.c)
				break;
			}
			sound(snd, vol);
			break;
		}
		case 0xD4: {
			int snd = num(r), vol = num(r), on = num(r), dist = num(r), loop = 0;
			if (peek(r) == 0xD9) { r->p++; loop = 1; }
			slog("sound %d volume %d at obj %d", snd, vol, on);
			sound_at_object(snd, vol, on, dist, loop);
			break;
		}
		case 0xD5: {
			int snd = num(r), vol = num(r), seg = num(r), side = rd(r) - 0xAD, dist = num(r), loop = 0;
			if (peek(r) == 0xD9) { r->p++; loop = 1; }
			slog("sound %d volume %d at seg %d side %d", snd, vol, seg, side);
			// ASCENT: 0xA9CBE: at the center of the segment (0x4AD9C, 0x2E94B)
			if (snd >= 0 && snd < MAX_SOUNDS && seg <= Highest_segment_index && side >= 0 && side < 6) {
				vms_vector c;
				compute_segment_center(&c, &Segments[seg]);
				digi_link_sound_to_pos2(snd, seg, side, &c, loop, sound_volume(vol), i2f(dist));
			}
			break;
		}
		case 0xD6:
			v = num(r);
			slog("stop the sound of obj %d", v);
			if (obj_num(v))
				digi_kill_sound_linked_to_object(v);		// ASCENT: 0xA9DD2, 0x2E816
			break;
		case 0xD7: {
			int snd = num(r), seg = num(r), side = rd(r) - 0xAD;
			slog("stop sound %d of seg %d side %d", snd, seg, side);
			if (seg <= Highest_segment_index && side >= 0 && side < 6)
				digi_kill_sound_linked_to_segment(seg, side, snd);		// ASCENT: 0xA9DEC, 0x2EB77
			break;
		}
		case 0xDA: {								// 0xA9F70
			int seg = num(r), facing = rd(r) - 0xA9;
			slog("teleport NPC to seg %d facing %d", seg, facing);
			if (seg == 0xFFFF)
				remove_object(o);
			else if (seg <= Highest_segment_index)
				move_to_segment(o, seg, facing);
			break;
		}
		case 0xDB:
			if (!teleport_player(r)) {
				r->result = 0;
				// 0xAA274: the run ends first; the statements after it run
				// with the window closed
				if (o) {
					o->dtu.m_1b0 = (o->dtu.m_1b0 & ~SB_FLAG(0)) | (r->after_flag0 ? SB_FLAG(0) : 0);
					if (Cur_mode == 0 && ((o->dtu.m_1b0 & SB_TALKS) >> SB_TALKS_SHIFT) < 15)
						o->dtu.m_1b0 += 1u << SB_TALKS_SHIFT;
				}
				dtu_dlg_close();
				kill_after_run(o);
			}
			break;
		case 0xDC: {								// 0xA9FDF
			int n = num(r), seg = num(r), facing = rd(r) - 0xA9;
			object *x = obj_num(n);
			slog("teleport obj %d to seg %d facing %d", n, seg, facing);
			if (!x)
				break;
			if (seg <= Highest_segment_index)
				move_to_segment(x, seg, facing);
			else if (seg == 0xFFFF)
				remove_object(x);
			break;
		}
		case 0xDD: {								// 0xAA04B
			int t2 = item_type(rd(r)), id = num(r), s1 = num(r), s2 = num(r), i;
			if (s1 > Highest_segment_index || t2 < 0 || t2 == 0xA8)
				break;
			for (i = Segments[s1].objects; i != -1; i = Objects[i].next)
				if (Objects[i].type == t2 && Objects[i].id == id) {
					if (s2 == 0xFFFF)
						Objects[i].flags |= OF_SHOULD_BE_DEAD;
					else if (s2 <= Highest_segment_index)
						move_to_segment(&Objects[i], s2, -1);
					break;
				}
			break;
		}
		case 0xDE: {
			int who = rd(r), seg = num(r), facing = rd(r) - 0xA9;
			summon(who, seg, facing);
			break;
		}
		case 0xDF: {								// 0xAA670
			int k = peek(r);
			object *x;
			if (k == 0x15) {
				r->p++;
				x = obj_num(num(r));
			} else if (k >= 0x7A && k <= 0x7E)
				x = o;
			else {
				if (o)
					o->dtu.m_1b0 = (o->dtu.m_1b0 & ~SB_RESUME) | ((r->label & 0x3ff) << SB_RESUME_SHIFT);
				slog("resume of obj %d = %X", r->objnum, r->label);
				break;
			}
			v = arith(r, x ? (x->dtu.m_1b0 & SB_RESUME) >> SB_RESUME_SHIFT : 0);
			if (x)
				x->dtu.m_1b0 = (x->dtu.m_1b0 & ~SB_RESUME) | ((v & 0x3ff) << SB_RESUME_SHIFT);
			slog("resume of obj %d = %X", x ? (int) (x - Objects) : -1, v & 0x3ff);
			break;
		}
		case 0xE0: {								// 0xAB3CA
			object *x = obj_prefix(r);
			v = arith(r, x ? (x->dtu.m_1b0 & SB_VAR) >> SB_VAR_SHIFT : 0);
			if (x)
				x->dtu.m_1b0 = (x->dtu.m_1b0 & ~SB_VAR) | ((v & 15) << SB_VAR_SHIFT);
			break;
		}
		case 0xE5: {								// 0xAA79F
			int last = -1;
			while (!at_end(r) && peek(r) != 0xE6) {
				v = hexnum(r);
				if (v <= Highest_segment_index)
					last = v;
			}
			if (!at_end(r)) r->p++;
			r->after_flag0 = 1;
			r->walk_seg = last;
			slog("walk to seg %d", last);
			// ASCENT: 0xAA74E and 0xAA810: the path of the AI (dtuobj.c)
			if (!Dtu_script_walk_teleport && o && o->type == OBJ_ROBOT && last >= 0) {
				dtu_script_walk_segment(o, last);
				dtu_script_walk_start(o);
			}
			break;
		}
		case 0xEA: {								// 0xAA86C
			int spell = num(r), n = num(r);
			object *x = obj_num(n);
			slog("spell %d from obj %d at obj %d", spell, r->objnum, n);
			if (spell <= 50 && x && o)
				dtu_spell_hit(spell, x, o, o->type == OBJ_PLAYER ? Dtu_char.pa.class1_level + 1 : 1);
			break;
		}
		case 0xEB: {
			int spell = num(r), seg = num(r);
			slog("area spell %d from obj %d (segment %d)", spell, r->objnum, seg);
			// ASCENT: 0xAA8B3: 0x90CEF(spell, object, object), the area
			// effect around the object of the script. The segment is only
			// checked against the highest segment (a defect of the
			// executable with no fix: the place of the effect that the
			// scripts were tested with is the object).
			if (spell <= 50 && seg <= Highest_segment_index && o)
				dtu_spell_area(spell, &o->pos, o->segnum, o, dtu_spell_object_level(o, spell));
			break;
		}
		case 0xEC:								// 0xAA8E4
			v = num(r);
			slog("wait %d", v);
			Wait_timer = 1;
			Wait_until = dtu_trap_clock() + v;
			return SC_WAIT;
		case 0xED:								// 0xAA908
			if (peek(r) == 0xC4) {
				int seg, side, n;
				r->p++;
				seg = num(r);
				side = rd(r) - 0xAD;
				n = find_trigger(seg, side);
				slog("trigger seg %d side %d off", seg, side);
				if (n >= 0)
					trigger_switch(n, 0);
			} else {
				v = num(r);
				slog("kill obj %d", v);
				obj_kill(obj_num(v));
			}
			break;
		case 0xEE: {								// 0xAA989
			object *x;
			v = num(r);
			x = obj_num(v);
			slog("call the script of obj %d", v);
			if (x && N_run < SC_MAX_NEST && start_run(x, x->dtu.m_1b0 & SB_SCRIPT, DTU_RUN_CALL, 0))
				return SC_CALL;
			break;
		}
		case 0xF2:
			sc_if(r);
			break;
		case 0xF4:								// 0xAB449
			sc_skip(r, 0xF9);
			r->p--;
			break;
		case 0xF9:
			r->depth--;
			break;
		default:
			printf("RLH: * Don't know statement code %d (%X) * at 0x%X of script %d\n", b, b, at, r->id);
			return SC_DONE;
		}
	}
	return SC_DONE;
}

//------------------------------------------------------------------------
// run_script (0xABBE5)

// 0xABB53: talking to Mirt in YPINN as a thief. The byte P+0x1A53 is the
// thief level + 1 of the last visit (0 before the first); the points
// (word P+0x1AB3) are set, not added. The screen (dtuthiefpts.c) returns 1
// when it was shown; else the level is not paid.
static void thief_points(void)
{
	dtu_player_attr *pa = &Dtu_char.pa;
	int lv = (pa->class2 == DTU_CLASS_THIEF ? pa->class2_level : pa->class1_level) + 1;
	int paid = Dtu_char.thief_paid & 0xff;

	if (lv <= paid)
		return;
	Dtu_char.thief_points = ((lv - paid) * 20 + (paid == 0 ? 20 : 0)) & 0xffff;
	slog("thief points: level %d, paid %d, %d points", lv, paid, Dtu_char.thief_points);
	if (dtu_thief_points_offer())
		Dtu_char.thief_paid = lv & 0xff;
}

// 0xAAA32: the start label. Returns 1 if the script runs.
static int start_label(sc_run *r, object *obj, int mode)
{
	int status = 0x3E8;

	r->p = 0;
	r->depth = 0;
	if (obj->dtu.m_1b0 & SB_FLAG(2))		status = 0x3EA;
	else if (obj->dtu.m_1b0 & SB_FLAG(3))	status = 0x3EB;
	else if (obj->dtu.m_1b0 & SB_FLAG(1))	status = 0x3E9;

	if (status == 0x3EA || (status == 0x3E9 && rand() % 20 + 1 > Dtu_char.pa.charisma)) {
		if (sc_goto(r, status) && rand() % 2) {
			show_line(r);
			// 0xAAB25: the attack sound of the robot (not in the port)
		}
		else if (mode == 0 && obj->type == OBJ_ROBOT)
			HUD_init_message("%s", Text_string[926]);		// "No response..."
		slog("obj %d: status %X, no run", r->objnum, status);
		return 0;
	}
	switch (mode) {
	case DTU_RUN_STATUS:
		if (sc_goto(r, status))
			show_line(r);
		return 0;
	case DTU_RUN_PATH:
		return sc_goto_seg(r, obj->segnum);
	default: {
		int resume = (obj->dtu.m_1b0 & SB_RESUME) >> SB_RESUME_SHIFT;
		if (resume)
			return sc_goto(r, resume);
		while (r->p < r->size) {
			if (r->s[r->p++] == 0xCB && peek(r) != 0xE7) {
				int lab = hexnum(r);
				if (lab < 0x3E8) {
					r->label = lab;
					return 1;
				}
			}
		}
		return 0;
	}
	}
}

// The steps of run_script before the interpreter. Pushes a run. Returns 1
// if the script runs.
static int start_run(object *obj, int id, int mode, int temp)
{
	int objnum = obj - Objects, size;
	sc_run *r;

	if (mode == DTU_RUN_TRIGGER && dtu_dlg_is_open())
		return 0;
	if (id == 0) {
		slog("* RLH: object %d is NULL or has no script *", objnum);
		return 0;
	}
	if (id < 0x1FC && (mode == DTU_RUN_TALK || mode == DTU_RUN_PATH) && !temp && obj_dead(objnum)) {
		slog("* RLH: object %d is dead *", objnum);
		return 0;
	}
	if (N_run >= SC_MAX_NEST)
		return 0;
	r = &Run[N_run];
	memset(r, 0, sizeof(*r));
	r->s = script_data(id, &size);
	if (!r->s) {
		slog("* RLH: Script %d not in table2 *", id);
		return 0;
	}
	// The level rule of the ids
	if (id < 0x1FC && id != 0x37 && id != 0x3F && id / 10 != Current_level_num - 1) {
		obj->dtu.m_1b0 &= ~SB_SCRIPT;
		dtu_script_make_hostile(obj);
		return 0;
	}
	r->size = size;
	r->id = id;
	r->objnum = objnum;
	r->signature = obj->signature;
	r->temp = temp;
	r->result = 1;
	r->walk_seg = -1;
	Cur_mode = mode;
	if (obj->type == OBJ_ROBOT && !obj_dead(objnum))
		dtu_creature_wake(obj, 0);							// 0xA6505(object, 1)
	if (!start_label(r, obj, mode))
		return 0;

	// 0xABD91: the player stops; the object turns to the player
	if (ConsoleObject)
		vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
	if (obj->type == OBJ_ROBOT && !obj_dead(objnum) && id < 0x14A && ConsoleObject) {
		vms_vector v;

		vm_vec_sub(&v, &ConsoleObject->pos, &obj->pos);
		v.y = 0;
		if (vm_vec_normalize(&v) > 0x100)
			vm_vector_2_matrix(&obj->orient, &v, NULL, NULL);
	}
	if (obj->type == OBJ_ROBOT)
		DTU_ANIM_STATE(obj) = DAS_STAND;					// 0xABF1B

	// 0xABF24: silenced (bit 0x40 of the byte +0x89)
	if (obj->dtu.m_88 & 0x4000) {
		HUD_init_message("%s tries to speak, but nothing happens.", dtu_object_display_name(obj));
		return 0;
	}
	// 0xABF58: Mirt and a thief in YPINN
	if (mode == DTU_RUN_TALK && Current_level_num == 1 && obj->type == OBJ_ROBOT &&
		 (Dtu_char.pa.class1 == DTU_CLASS_THIEF || Dtu_char.pa.class2 == DTU_CLASS_THIEF) &&
		 !sc_strcmpi(dtu_object_name(obj), Text_string[905], -1))
		thief_points();

	obj->dtu.m_1b0 |= SB_FLAG(0);
	r->after_flag0 = 0;
	N_run++;
	slog("run script %d of obj %d (%s) in mode %d at label %X", id, objnum, dtu_object_name(obj), mode, r->label);
	return 1;
}

// The steps of run_script after the interpreter (0xABFC4)
static void finish_run(sc_run *r)
{
	object *o = run_obj(r);

	if (r->result && o) {
		o->dtu.m_1b0 = (o->dtu.m_1b0 & ~SB_FLAG(0)) | (r->after_flag0 ? SB_FLAG(0) : 0);
		if (Cur_mode == DTU_RUN_TALK && ((o->dtu.m_1b0 & SB_TALKS) >> SB_TALKS_SHIFT) < 15)
			o->dtu.m_1b0 += 1u << SB_TALKS_SHIFT;
	}
	if (r->result) {
		dtu_dlg_close();
		kill_after_run(o);
	}
	if (r->temp && o)
		o->flags |= OF_SHOULD_BE_DEAD;						// 0xA74D0 deletes the temporary object
	if (o && r->walk_seg >= 0) {
		// ASCENT: the object walks (statement E5); dtu_script_frame()
		// runs the script in mode 2 at the end of the path
		Walk_obj = r->objnum;
		Walk_sig = o->signature;
		Walk_seg = r->walk_seg;
		Walk_until = dtu_trap_clock() + 6000;
	}
	slog("end of script %d of obj %d", r->id, r->objnum);
}

// Runs the stack until it is empty or a run waits. Returns 1 if a run waits.
static int continue_runs(void)
{
	while (N_run > 0) {
		sc_run *r = &Run[N_run - 1];
		int res = sc_exec(r);

		if (res == SC_WAIT)
			return 1;
		if (res == SC_CALL)
			continue;
		finish_run(r);
		N_run--;
	}
	return 0;
}

int dtu_script_run(object *obj, int mode)
{
	if (!obj || N_run > 0)
		return 0;
	if (!start_run(obj, obj->dtu.m_1b0 & SB_SCRIPT, mode, 0))
		return 0;
	continue_runs();
	return 1;
}

// 0xAC055 with object 0
void dtu_script_run_id(int id)
{
	int n;
	object *o;

	if (!ConsoleObject || N_run > 0)
		return;
	n = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, NULL, 0, CT_NONE, MT_NONE, RT_NONE);
	if (n < 0) {
		slog("RLH: couldn't create temp object");
		return;
	}
	o = &Objects[n];
	o->dtu.m_1b0 = id & SB_SCRIPT;
	o->shields = 0x3fffffff;
	if (!start_run(o, id, DTU_RUN_TALK, 1)) {
		o->flags |= OF_SHOULD_BE_DEAD;
		return;
	}
	continue_runs();
}

// WEAPON 13 (Drowslayer) is the item in the hand (0x59D2D, 0xAC206)
static int weapon13_in_hand(void)
{
	return Dtu_char.current.n >= 0 && Dtu_char.current.n < Dtu_n_inventory &&
		Dtu_inventory[Dtu_char.current.n].type == OBJ_WEAPON && Dtu_inventory[Dtu_char.current.n].id == 13;
}

// The name flag 0x08 of Robot_info +0xD9, set by the table reader at
// 0x1F448 when the name of the creature starts with "rf", "rm", "av" or
// "dro" (docs/re/ai.md).
static int drow_type(object *o)
{
	char *n;

	if (!o || o->type != OBJ_ROBOT || o->id >= N_robot_types)
		return 0;
	n = Monster_info[o->id].name;
	return !sc_strcmpi(n, "rf", 2) || !sc_strcmpi(n, "rm", 2) || !sc_strcmpi(n, "av", 2) || !sc_strcmpi(n, "dro", 3);
}

// 0xAC155 from the test of the script id on
int dtu_script_talk(object *obj)
{
	object *pl = ConsoleObject;

	if (!obj || !pl)
		return 0;
	if ((obj->dtu.m_1b0 & SB_SCRIPT) == 0) {
		if (obj->type != OBJ_ROBOT)
			return 0;
		if (!obj_dead(obj - Objects))
			HUD_init_message("%s", Text_string[926]);		// "No response..."
		return 1;
	}
	if (obj_dead(obj - Objects))
		return 1;
	if (N_run > 0)
		return 1;
	// 0xAC206: WEAPON 13 in the hand
	if (weapon13_in_hand()) {
		// Global flag 0x81 (0xAC22E): the creature has the name flag of
		// Robot_info +0xD9 (drow_type).
		slog("talk with WEAPON 13 in the hand: script 0x1FD");
		if (drow_type(obj))
			dtu_script_set_gflag(0x81, 1);
		dtu_script_set_gflag(0x8F, 1);
		dtu_script_run_id(0x1FD);
		if (N_run > 0) {
			// ASCENT: the talk goes on when script 0x1FD has ended
			After_kind = 1;
			After_obj = obj - Objects;
			After_sig = obj->signature;
			return 1;
		}
		if (dtu_script_gflag(0xA0))
			return 1;
	}
	dtu_script_run(obj, DTU_RUN_TALK);
	return 1;
}

// ASCENT: the check of the warp check 0x59C3C at 0x59D2D (the strong
// definition of the hook of dtutrig.c). With WEAPON 13 (Drowslayer) in the
// hand, a warp to another level sets global flags for script 0x1FD: 0x8C
// when the level has a creature with the name flag (drow_type) that is not
// dead, 0x8B when such a creature has none of the effects 0x4004100, has
// not flag 3 and its flee value A (Robot_info +0x80) is below its morale;
// 0x93 to 0x98 for the levels 0, 0x14, 0x15, 0x16, 0x1B and 0x20; 0x8E.
// Then script 0x1FD runs, and global flag 0xA0 after it drops the warp.
// Returns 1 if the warp goes on now. When the script waits for a reply
// the warp goes on in dtu_script_frame().
int dtu_hook_warp_allowed(int from_level, int to_level, int dest_seg, int value)
{
	static const ubyte level_flag[6][2] = { {0, 0x93}, {0x14, 0x94}, {0x15, 0x95}, {0x16, 0x96}, {0x1B, 0x97}, {0x20, 0x98} };
	int i;

	if (!ConsoleObject || N_run > 0 || !weapon13_in_hand())
		return 1;
	for (i = 1; i <= Highest_object_index; i++) {
		object *o = &Objects[i];

		if (o->type != OBJ_ROBOT || !drow_type(o) || (o->flags & 7))
			continue;
		dtu_script_set_gflag(0x8C, 1);
		if (!(o->dtu.m_88 & 0x4004100) && !(o->dtu.m_1b0 & SB_FLAG(3)) && dtu_script_creature_brave(o))
			dtu_script_set_gflag(0x8B, 1);
	}
	for (i = 0; i < 6; i++)
		if (level_flag[i][0] == from_level)
			dtu_script_set_gflag(level_flag[i][1], 1);
	dtu_script_set_gflag(0x8E, 1);
	slog("warp with WEAPON 13 in the hand: script 0x1FD");
	dtu_script_run_id(0x1FD);
	if (N_run > 0) {
		After_kind = 2;
		After_level = to_level;
		After_seg = dest_seg;
		After_value = value;
		After_prison = Warp_prison_now;
		return 0;
	}
	if (dtu_script_gflag(0xA0)) {
		slog("WEAPON 13: global flag 0xA0, the warp is dropped");
		return 0;
	}
	return 1;
}

// ASCENT: test commands of -chartest (tools/questtest.py): the warp of
// statement DB, and the prison sequence with the first creature that has
// script 60 as the object of the script.
void dtu_script_test_warp(int level, int seg)
{
	if (N_run > 0 || Warp_level >= 0)
		return;
	Warp_level = level;
	Warp_seg = seg;
	Warp_facing = 2;
	Warp_prison = 0;
}

void dtu_script_test_prison(void)
{
	sc_run r;
	int i;

	if (N_run > 0 || Warp_level >= 0)
		return;
	memset(&r, 0, sizeof(r));
	r.objnum = -1;
	for (i = 1; i <= Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT && (Objects[i].dtu.m_1b0 & SB_SCRIPT) == 60) {
			r.objnum = i;
			r.signature = Objects[i].signature;
			break;
		}
	prison_sequence(&r);
}

int dtu_script_waiting(void)
{
	return N_run > 0;
}

// Test option -scriptflags LIST: global flags (numbers) and mission flags
// (m1 to m19) that are set at the first level start and for a new
// character.
static void test_flags(void)
{
	int t;
	char *s;

	if ((t = FindArg("-scriptflags")) == 0 || t+1 >= Num_args)
		return;
	for (s = Args[t+1]; *s; ) {
		if (*s == 'm')
			dtu_mission_set_flag(atoi(s + 1), 1);
		else
			dtu_script_set_gflag(atoi(s), 1);
		while (*s && *s != ',') s++;
		if (*s == ',') s++;
	}
}

void dtu_script_test_flags(void)
{
	static int first = 1;

	if (first) {
		first = 0;
		test_flags();
	}
}

void dtu_script_level_start(void)
{
	static int first = 1;

	dtu_script_init();
	if (first) {
		first = 0;
		test_flags();
	}
	N_run = 0;
	Wait_timer = 0;
	Warp_level = -1;
	Walk_obj = -1;
	After_kind = 0;
	dtu_dlg_close();
	dtu_dlg_reset_view();
	dtu_scroll_reset();
}

// A warp of DB to another level (0x59C3C after the run)
static void do_warp(int checked)
{
	char file[20], *dot;
	ubyte *data;
	int size, level = Warp_level, seg = Warp_seg, facing = Warp_facing, prison = Warp_prison;

	Warp_level = -1;
	Warp_prison = 0;
	if (level == 0xFFFF) {
		dtu_trig_script_warp(level, seg, facing, checked);		// 0xAA33A: a command of DTU.WRP
		return;
	}
	if (level < 0 || level >= Last_level) {
		slog("teleport: no level %d", level);
		return;
	}
	strncpy(file, Level_names[level], sizeof(file) - 1);
	file[sizeof(file) - 1] = 0;
	if ((dot = strchr(file, '.')) != NULL)
		strcpy(dot, ".LVL");
	data = dtu_load_file(file, &size);
	if (!data) {
		slog("teleport: the level file %s is not in the game data", file);
		return;
	}
	free(data);
	printf("Script: warping to %s, segment %d.\n", Level_names[level], seg);
	// ASCENT: 0xAA3A5: the warp check 0x59C3C does the warp, with the
	// check of WEAPON 13, the passage gold and the level states (dtutrig.c)
	Warp_prison_now = prison;
	if (dtu_trig_script_warp(level, seg, facing, checked) == 1 && prison)
		prison_arrival();
	Warp_prison_now = 0;
}

int dtu_script_frame(void)
{
	static int level_seen = -1;

	// A new level (the port has no call from the level start)
	if (Current_level_num != level_seen && Warp_level < 0) {
		level_seen = Current_level_num;
		dtu_script_level_start();
	}
	if (Warp_level >= 0 && N_run == 0)
		do_warp(0);
	// ASCENT: script 0x1FD has ended: the talk or the warp that waited
	if (After_kind && N_run == 0 && Warp_level < 0) {
		int kind = After_kind;

		After_kind = 0;
		if (dtu_script_gflag(0xA0))
			slog("WEAPON 13: global flag 0xA0, the %s is dropped", kind == 1 ? "talk" : "warp");
		else if (kind == 1) {
			object *o = obj_num(After_obj);
			if (o && o->signature == After_sig && !obj_dead(After_obj))
				dtu_script_run(o, DTU_RUN_TALK);
		} else if (dtu_trig_script_warp(After_level, After_seg, After_value, 1) == 1 && After_prison)
			prison_arrival();
	}
	if (Walk_obj >= 0 && N_run == 0) {
		object *o = obj_num(Walk_obj);
		int state = 0, arrived = 0;

		if (!o || o->signature != Walk_sig || obj_dead(Walk_obj))
			Walk_obj = -1;
		else {
			state = dtu_script_walk_state(o);
			if (state == 2)
				arrived = 1;
			else if (Dtu_script_walk_teleport || state == 0 || dtu_trap_clock() >= Walk_until) {
				// the default; or the creature has no path, left it or did
				// not reach its end in 60 seconds
				slog("walk: obj %d is moved to seg %d (state %d)", Walk_obj, Walk_seg, state);
				move_to_segment(o, Walk_seg, -1);
				arrived = 1;
			}
		}
		if (arrived) {
			Walk_obj = -1;
			if (o->dtu.m_1b0 & SB_FLAG(0))
				dtu_script_run(o, DTU_RUN_PATH);				// 0x174FC
		}
	}
	if (N_run == 0)
		return 0;
	if (Wait_timer) {
		if (dtu_trap_clock() < Wait_until)
			return 1;
		Wait_timer = 0;
	} else {
		sc_run *r = &Run[N_run - 1];
		int c = dtu_dlg_poll();

		if (c == -1)
			return 1;
		if (r->pending_reply) {
			r->pending_reply = 0;
			// 0xA7ADC
			if (c < 0) {
				if (r->n_replies > 0) {
					exit_label(r);
					finish_run(r);
					N_run--;
				}
			} else if (r->n_replies > 0) {
				if (c >= r->n_replies)
					c = r->n_replies - 1;
				slog("reply %d -> label %X", c + 1, r->reply_label[c]);
				sc_goto(r, r->reply_label[c]);
			}
		}
	}
	continue_runs();
	return N_run > 0;
}
