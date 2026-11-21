/*
 * ASCENT: new file. The level state files of Descent to Undermountain. It
 * replaces the code of DTUGAME.EXE 1.3 at 0x5A756 (save the state of a
 * level), 0x5AA75 (restore it), 0x5ABC6 and 0x5AC6F (new game), 0xBAD08
 * and 0xBB1A7 (the state file), 0xBC1EC and 0xBC51D (the copies in the
 * save slots), the door code of the warp (0x5A0F6) and the loading
 * picture of the level loader (0x7C1E5). See docs/re/levelstate.md and
 * docs/re/frontend.md.
 *
 * The files have the names and the roles of the original files, but the
 * content is the data of the port (docs/re/levelstate.md, section 10).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "mem.h"				// dtu_load_file() allocates with mem_malloc
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "wall.h"
#include "automap.h"
#include "gameseq.h"
#include "player.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtutrig.h"
#include "dtufe.h"
#include "dtustate.h"
#include "args.h"

#define LS_LEVELS		100		// records of the level table (fread(buf, 6, 100))
#define LS_RECORD		6
#define STATE_DIR		"SAVE"

#define STATE_VERSION	1

typedef struct ls_record {
	int	temp;					// byte 0: temp<L>.* hold the state
	int	saved;				// byte 1: sg<slot>_<L>.* hold the state
	int	slot;					// s32 at 2
} ls_record;

int Dtu_level_has_state[LS_LEVELS];	// 0x296304

static int Cleanup_registered;

//------------------------------------------------------------------------
// Files

static void state_path(char *out, int size, const char *name)
{
	char rel[64];

	snprintf(rel, sizeof(rel), "%s/%s", STATE_DIR, name);
	dtu_user_path(out, size, rel);		// ASCENT: the directory of the user (dtufile.h)
}

static void make_dir(void)
{
	char dir[600];

	dtu_user_path(dir, sizeof(dir), STATE_DIR);	// ASCENT: the directory of the user
#ifdef _WIN32
	_mkdir(dir);
#else
	mkdir(dir, 0777);
#endif
}

static FILE *state_open(const char *name, const char *mode)
{
	char path[600];

	state_path(path, sizeof(path), name);
	return fopen(path, mode);
}

static void state_delete(const char *name)
{
	char path[600];

	state_path(path, sizeof(path), name);
	remove(path);
}

static int state_exists(const char *name)
{
	FILE *f = state_open(name, "rb");

	if (!f)
		return 0;
	fclose(f);
	return 1;
}

static int copy_file(const char *from, const char *to)
{
	FILE *in = state_open(from, "rb"), *out;
	char buf[4096];
	size_t n;
	int ok = 1;

	if (!in)
		return 0;
	out = state_open(to, "wb");
	if (!out) {
		fclose(in);
		return 0;
	}
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		if (fwrite(buf, 1, n, out) != n)
			ok = 0;
	fclose(in);
	if (fclose(out))
		ok = 0;
	return ok;
}

// 0xBA428: the move of a file (rename, else copy and delete)
static int move_file(const char *from, const char *to)
{
	char a[600], b[600];

	state_path(a, sizeof(a), from);
	state_path(b, sizeof(b), to);
	remove(b);
	if (rename(a, b) == 0)
		return 1;
	if (!copy_file(from, to))
		return 0;
	remove(a);
	return 1;
}

static const char *Ext[3] = { "lvl", "tig", "tap" };

static void temp_name(char *out, int level, int e)
{
	sprintf(out, "temp%d.%s", level, Ext[e]);		// "temp%d.lvl" (0x128886)
}

static void slot_name(char *out, int s, int level, int e)
{
	sprintf(out, "sg%d_%d.%s", s, level, Ext[e]);	// "sg%d_%d.lvl" (0x1288A0)
}

//------------------------------------------------------------------------
// The level table

static void put32(ubyte *p, int v)
{
	p[0] = (ubyte) v; p[1] = (ubyte) (v >> 8); p[2] = (ubyte) (v >> 16); p[3] = (ubyte) (v >> 24);
}

static int get32(ubyte *p)
{
	return (int) ((uint) p[0] | ((uint) p[1] << 8) | ((uint) p[2] << 16) | ((uint) p[3] << 24));
}

static int ls_read(const char *name, ls_record *r)
{
	ubyte b[LS_LEVELS * LS_RECORD];
	FILE *f = state_open(name, "rb");
	int i, n;

	if (!f)
		return 0;
	n = (int) fread(b, LS_RECORD, LS_LEVELS, f);
	fclose(f);
	if (n < LS_LEVELS)
		return 0;
	for (i = 0; i < LS_LEVELS; i++) {
		r[i].temp = b[i * LS_RECORD];
		r[i].saved = b[i * LS_RECORD + 1];
		r[i].slot = get32(b + i * LS_RECORD + 2);
	}
	return 1;
}

static int ls_write(const char *name, ls_record *r)
{
	ubyte b[LS_LEVELS * LS_RECORD];
	FILE *f;
	int i, ok;

	for (i = 0; i < LS_LEVELS; i++) {
		b[i * LS_RECORD] = (ubyte) r[i].temp;
		b[i * LS_RECORD + 1] = (ubyte) r[i].saved;
		put32(b + i * LS_RECORD + 2, r[i].slot);
	}
	make_dir();
	f = state_open(name, "wb");
	if (!f)
		return 0;
	ok = fwrite(b, LS_RECORD, LS_LEVELS, f) == LS_LEVELS;
	if (fclose(f))
		ok = 0;
	return ok;
}

// 0x5AC6F: the executable deletes "temp<i>" with no extension and the
// .tig and .tap files, so the .lvl files stay. Their records are new, so
// they are not read again. The port deletes all three.
static void delete_running_state(void)
{
	char name[32];
	int i, e;

	for (i = 0; i < LS_LEVELS; i++)
		for (e = 0; e < 3; e++) {
			temp_name(name, i, e);
			state_delete(name);
		}
	state_delete("CSUF.ls");
}

static void cleanup(void)
{
	// The close of the program (0x426FA) deletes the running state.
	if (state_exists("CSUF.ls"))
		delete_running_state();
}

void dtustate_new_game(void)
{
	ls_record r[LS_LEVELS];
	int i;

	if (!Cleanup_registered) {
		Cleanup_registered = 1;
		atexit(cleanup);
	}
	delete_running_state();							// 0x5AC6F("CSUF.ls")
	memset(Dtu_level_has_state, 0, sizeof(Dtu_level_has_state));
	for (i = 0; i < LS_LEVELS; i++) {			// 0x5ABC6
		r[i].temp = r[i].saved = 0;
		r[i].slot = -1;
	}
	if (!ls_write("CSUF.ls", r))
		dtu_log("State: cannot write %s/CSUF.ls\n", STATE_DIR);
}

//------------------------------------------------------------------------
// The state file of a level (0xBAD08, 0xBB1A7)

typedef struct state_header {
	char	magic[4];
	int	version;
	int	object_size, wall_size, door_size, trigger_size, trap_size;
	int	n_segments;
} state_header;

static void header_init(state_header *h, const char *magic)
{
	memset(h, 0, sizeof(*h));
	memcpy(h->magic, magic, 4);
	h->version = STATE_VERSION;
	h->object_size = sizeof(object);
	h->wall_size = sizeof(wall);
	h->door_size = sizeof(active_door);
	h->trigger_size = sizeof(dtu_trigger);
	h->trap_size = sizeof(dtu_trap);
	h->n_segments = Highest_segment_index + 1;
}

static int header_check(FILE *f, const char *magic)
{
	state_header want, have;

	header_init(&want, magic);
	if (fread(&have, sizeof(have), 1, f) != 1 || memcmp(&want, &have, sizeof(have))) {
		dtu_log("State: the file is not a state file of this level and this program\n");
		return 0;
	}
	return 1;
}

static int write_lvl(const char *name)
{
	state_header h;
	FILE *f;
	int i, s, n, ok = 1;

	make_dir();
	f = state_open(name, "wb");
	if (!f)
		return 0;
	header_init(&h, "ASLS");
	ok &= fwrite(&h, sizeof(h), 1, f) == 1;
	n = Highest_object_index + 1;
	ok &= fwrite(&n, sizeof(n), 1, f) == 1;
	ok &= fwrite(Objects, sizeof(object), n, f) == (size_t) n;
	ok &= fwrite(&Num_walls, sizeof(int), 1, f) == 1;
	ok &= fwrite(Walls, sizeof(wall), Num_walls, f) == (size_t) Num_walls;
	ok &= fwrite(&Num_open_doors, sizeof(int), 1, f) == 1;
	ok &= fwrite(ActiveDoors, sizeof(active_door), Num_open_doors, f) == (size_t) Num_open_doors;
	// the sides: wall, texture 1, texture 2 (0xBAEEE), the flags (0x4AB04)
	for (i = 0; i <= Highest_segment_index; i++)
		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++) {
			side *sp = &Segments[i].sides[s];
			ok &= fwrite(&sp->wall_num, sizeof(short), 1, f) == 1;
			ok &= fwrite(&sp->tmap_num, sizeof(short), 1, f) == 1;
			ok &= fwrite(&sp->tmap_num2, sizeof(short), 1, f) == 1;
			ok &= fwrite(&sp->pad, 1, 1, f) == 1;
		}
	ok &= fwrite(Automap_visited, 1, Highest_segment_index + 1, f) == (size_t) (Highest_segment_index + 1);
	if (fclose(f))
		ok = 0;
	return ok;
}

static int write_tig(const char *name)
{
	state_header h;
	FILE *f;
	int ok = 1;

	f = state_open(name, "wb");
	if (!f)
		return 0;
	header_init(&h, "ASLT");
	ok &= fwrite(&h, sizeof(h), 1, f) == 1;
	ok &= fwrite(&Dtu_num_triggers, sizeof(int), 1, f) == 1;
	ok &= fwrite(Dtu_trigger_seg_table, sizeof(Dtu_trigger_seg_table), 1, f) == 1;
	ok &= fwrite(Dtu_triggers, sizeof(dtu_trigger), Dtu_num_triggers, f) == (size_t) Dtu_num_triggers;
	if (fclose(f))
		ok = 0;
	return ok;
}

static int write_tap(const char *name)
{
	state_header h;
	FILE *f;
	int i, ok = 1;

	// 0x95FB8: each trap gets its delay back (+0x41 = +0x49)
	for (i = 0; i < Dtu_num_traps; i++)
		Dtu_traps[i].delay_init = Dtu_traps[i].delay;
	f = state_open(name, "wb");
	if (!f)
		return 0;
	header_init(&h, "ASLP");
	ok &= fwrite(&h, sizeof(h), 1, f) == 1;
	ok &= fwrite(&Dtu_num_traps, sizeof(int), 1, f) == 1;
	ok &= fwrite(Dtu_traps, sizeof(dtu_trap), Dtu_num_traps, f) == (size_t) Dtu_num_traps;
	if (fclose(f))
		ok = 0;
	return ok;
}

// 0xBB1A7
static int read_lvl(const char *name)
{
	FILE *f = state_open(name, "rb");
	int i, s, n, segnum, ok = 1;

	if (!f)
		return 0;
	if (!header_check(f, "ASLS") || fread(&n, sizeof(n), 1, f) != 1 || n < 1 || n > MAX_OBJECTS) {
		fclose(f);
		return 0;
	}

	for (i = 0; i <= Highest_segment_index; i++)
		Segments[i].objects = -1;
	reset_objects(1);
	if (fread(Objects, sizeof(object), n, f) != (size_t) n)
		ok = 0;
	Highest_object_index = n - 1;
	Object_next_signature = 0;
	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		segnum = obj->segnum;
		obj->next = obj->prev = obj->segnum = -1;
		if (obj->type == OBJ_NONE)
			continue;
		// 0xBB2E6: an object of type 1 (fireball) or with no segment is killed
		if (obj->type == OBJ_FIREBALL || segnum < 0 || segnum > Highest_segment_index) {
			dtu_log("STATE: killing object %d -- illegal segment\n", i);
			obj->type = OBJ_NONE;
			continue;
		}
		obj_link(i, segnum);
		if (obj->signature > Object_next_signature)
			Object_next_signature = obj->signature;
	}
	special_reset_objects();
	Object_next_signature++;

	if (fread(&Num_walls, sizeof(int), 1, f) != 1 || Num_walls < 0 || Num_walls > MAX_WALLS) {
		Num_walls = 0;
		ok = 0;
	} else if (fread(Walls, sizeof(wall), Num_walls, f) != (size_t) Num_walls)
		ok = 0;
	if (fread(&Num_open_doors, sizeof(int), 1, f) != 1 || Num_open_doors < 0 || Num_open_doors > MAX_DOORS) {
		Num_open_doors = 0;
		ok = 0;
	} else if (fread(ActiveDoors, sizeof(active_door), Num_open_doors, f) != (size_t) Num_open_doors)
		ok = 0;
	for (i = 0; i <= Highest_segment_index && ok; i++)
		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++) {
			side *sp = &Segments[i].sides[s];
			if (fread(&sp->wall_num, sizeof(short), 1, f) != 1 || fread(&sp->tmap_num, sizeof(short), 1, f) != 1
				|| fread(&sp->tmap_num2, sizeof(short), 1, f) != 1 || fread(&sp->pad, 1, 1, f) != 1)
				ok = 0;
		}
	if (fread(Automap_visited, 1, Highest_segment_index + 1, f) != (size_t) (Highest_segment_index + 1))
		ok = 0;
	fclose(f);
	return ok;
}

static int read_tig(const char *name)
{
	FILE *f = state_open(name, "rb");
	int n, ok = 0;

	if (!f)
		return 0;
	if (header_check(f, "ASLT") && fread(&n, sizeof(n), 1, f) == 1 && n >= 0 && n <= DTU_MAX_TRIGGERS
		&& fread(Dtu_trigger_seg_table, sizeof(Dtu_trigger_seg_table), 1, f) == 1
		&& fread(Dtu_triggers, sizeof(dtu_trigger), n, f) == (size_t) n) {
		Dtu_num_triggers = n;
		ok = 1;
	}
	fclose(f);
	return ok;
}

static int read_tap(const char *name)
{
	FILE *f = state_open(name, "rb");
	int n, ok = 0;

	if (!f)
		return 0;
	if (header_check(f, "ASLP") && fread(&n, sizeof(n), 1, f) == 1 && n >= 0 && n <= DTU_MAX_TRAPS
		&& fread(Dtu_traps, sizeof(dtu_trap), n, f) == (size_t) n) {
		Dtu_num_traps = n;
		ok = 1;
	}
	fclose(f);
	return ok;
}

//------------------------------------------------------------------------
// Save and restore a level

int dtustate_save_level(int level_index)
{
	ls_record r[LS_LEVELS];
	char name[32];
	int ok;

	if (level_index < 0 || level_index >= LS_LEVELS || !ls_read("CSUF.ls", r)) {
		printf("State: no level table %s/CSUF.ls\n", STATE_DIR);
		return 0;
	}
	r[level_index].temp = 1;
	Dtu_level_has_state[level_index] = 1;
	if (!ls_write("CSUF.ls", r))
		return 0;
	temp_name(name, level_index, 0);
	ok = write_lvl(name);
	temp_name(name, level_index, 1);
	ok &= write_tig(name);
	temp_name(name, level_index, 2);
	ok &= write_tap(name);
	dtu_log("State: %s the state of level %d\n", ok ? "saved" : "cannot save", level_index);
	printf("State: %s the state of level %d\n", ok ? "saved" : "cannot save", level_index);
	return ok;
}

static int restore_files(char names[3][32])
{
	if (!read_lvl(names[0]))
		return 0;
	read_tig(names[1]);
	read_tap(names[2]);
	// 0xBAFE0 sets each object up again from the tables (0xAE3F2). The
	// port does it for all objects of the level with the function of the
	// level start.
	dtu_objects_level_setup();
	return 1;
}

int dtustate_restore_level(int level_index)
{
	ls_record r[LS_LEVELS];
	char names[3][32];
	int e;

	if (level_index < 0 || level_index >= LS_LEVELS || !ls_read("CSUF.ls", r))
		return 0;
	if (r[level_index].temp) {
		for (e = 0; e < 3; e++)
			temp_name(names[e], level_index, e);
	} else if (r[level_index].saved && r[level_index].slot >= 0) {
		for (e = 0; e < 3; e++)
			slot_name(names[e], r[level_index].slot, level_index, e);
	} else
		return 0;
	printf("Restoring %s.\n", names[0]);
	dtu_log("Restoring %s.\n", names[0]);
	if (!restore_files(names)) {
		dtu_log("State: cannot read %s\n", names[0]);
		return 0;
	}
	return 1;
}

// 0x5A0F6
void dtustate_close_doors(void)
{
	int i, n = Num_open_doors;

	for (i = 0; i < n && Num_open_doors > 0; i++)
		wall_close_door(0);

	// ASCENT: the doors of the executable stay in the list when they are
	// open (do_door_open 0x7DE76), so the loop above closes them. The port
	// takes an open door out of the list (Descent 1), so the open doors
	// are closed here.
	for (i = 0; i < Num_walls; i++) {
		wall *w = &Walls[i];
		segment *seg, *csegp;
		int cside;

		if (w->type != WALL_DOOR || (w->state == WALL_DOOR_CLOSED && !(w->flags & WALL_DOOR_OPENED)))
			continue;
		if (w->segnum < 0 || w->segnum > Highest_segment_index)
			continue;
		seg = &Segments[w->segnum];
		w->state = WALL_DOOR_CLOSED;
		if (seg->children[w->sidenum] < 0)
			continue;
		csegp = &Segments[seg->children[w->sidenum]];
		cside = find_connect_side(seg, csegp);
		if (cside >= 0 && csegp->sides[cside].wall_num >= 0)
			Walls[csegp->sides[cside].wall_num].state = WALL_DOOR_CLOSED;
		if (cside >= 0 && w->clip_num >= 0 && w->clip_num < Num_wall_anims && WallAnims[w->clip_num].num_frames > 0)
			wall_set_tmap_num(seg, w->sidenum, csegp, cside, w->clip_num, 0);
	}

	for (i = 0; i < Num_walls; i++)
		Walls[i].flags &= ~WALL_DOOR_OPENED;
}

//------------------------------------------------------------------------
// Save slots (0xBC1EC, 0xBC51D)

static void slot_delete(int s, int level)
{
	char name[32];
	int e;

	for (e = 0; e < 3; e++) {
		slot_name(name, s, level, e);
		state_delete(name);
	}
}

int dtustate_save_slot(int s)
{
	ls_record cs[LS_LEVELS], ss[LS_LEVELS];
	char sname[32], from[32], to[32];
	int i, e;

	if (!ls_read("CSUF.ls", cs))
		return 0;
	sprintf(sname, "sg%d.ls", s);					// "sg%d.ls" (0x133CAA)
	if (!ls_read(sname, ss))
		for (i = 0; i < LS_LEVELS; i++) {
			ss[i].temp = ss[i].saved = 0;
			ss[i].slot = s;
		}
	for (i = 0; i < LS_LEVELS; i++) {
		if (cs[i].temp) {
			if (ss[i].saved)
				slot_delete(s, i);
			for (e = 0; e < 3; e++) {
				temp_name(from, i, e);
				slot_name(to, s, i, e);
				move_file(from, to);
			}
			cs[i].temp = 0;
			cs[i].saved = 1;
		} else if (cs[i].saved && cs[i].slot != s) {
			for (e = 0; e < 3; e++) {
				slot_name(from, cs[i].slot, i, e);
				slot_name(to, s, i, e);
				copy_file(from, to);
			}
			cs[i].temp = 0;
			cs[i].saved = 1;
		} else if (!cs[i].saved) {
			slot_delete(s, i);
			if (ss[i].saved && cs[i].slot != s)
				slot_delete(cs[i].slot, i);
		}
		cs[i].slot = s;
	}
	return ls_write(sname, cs) && ls_write("CSUF.ls", cs);
}

int dtustate_load_slot(int s)
{
	ls_record r[LS_LEVELS];
	char sname[32];
	int i;

	if (!Cleanup_registered) {
		Cleanup_registered = 1;
		atexit(cleanup);
	}
	sprintf(sname, "sg%d.ls", s);
	if (!ls_read(sname, r)) {
		// A save of an older version of the port has no table: the game
		// goes on with no level state.
		dtustate_new_game();
		return 0;
	}
	delete_running_state();
	for (i = 0; i < LS_LEVELS; i++) {
		r[i].temp = 0;
		Dtu_level_has_state[i] = r[i].saved != 0;
	}
	return ls_write("CSUF.ls", r);
}

//------------------------------------------------------------------------
// The calls of the game

void dtustate_warp_leave(int level_index)
{
	dtustate_close_doors();
	dtustate_save_level(level_index);
}

static void st_level_start(void);

void dtustate_warp_enter(int level_index)
{
	dtustate_restore_level(level_index);
	dtustate_loading_end();
	st_level_start();
}

void dtustate_game_started(int level_num)
{
	// 0x4DD5A: StartNewGame restores level 0. A load of a save game in a
	// level (0xBBDE5) restores the level of the save; the port starts that
	// level through StartNewGame.
	if (dtustate_restore_level(level_num - 1))
		dtu_trig_player_placed();
	dtustate_loading_end();
	st_level_start();
}

//------------------------------------------------------------------------
// The loading picture (0x494EB, 0x7C1E5, 0x7C2BB)

extern char Bm_level_name[16];
extern char Bm_egg_name[16];

static int Picture_shown;

// The table at 0x13E1C9: a level shows FEBGND table[(rand() & 0xFF) % 6].
static int Loading_pictures[6] = { 156, 157, 158, 159, 160, 155 };

void dtustate_level_loading(char *level_name)
{
	char name[16], bin[20], egg[16];
	ubyte *data;
	int i, size, entry, from_fe = dtufe_loading_from_frontend();

	for (i = 0; i < 15 && level_name[i] && level_name[i] != '.'; i++)
		name[i] = level_name[i];
	name[i] = 0;
	// The table of the front end of the executable is FRONTEND.TBL, so the
	// first level after the front end always loads its tables. The port
	// keeps the tables of YPINN in the front end.
	if (!from_fe) {
		// 0x4971B: the tables are loaded only for another table name
		if (!stricmp(name, Bm_level_name))
			return;
		// 0x498C9: and only for another archive (or SHORT.EGG)
		sprintf(bin, "%s.BIN", name);
		data = dtu_load_file(bin, &size);
		if (!data)
			return;
		memset(egg, 0, sizeof(egg));
		if (size >= 13)
			memcpy(egg, data, 13);
		free(data);
		if (!stricmp(egg, Bm_egg_name) && Bm_egg_name[0] && stricmp(Bm_egg_name, "SHORT.EGG"))
			return;
	}
	// 0x4991C: 12 after the Yawning Portal of the front end (0x13E324,
	// set by Function_mode 8), else a picture of the table
	if (from_fe == 1)
		entry = 12;
	else
		entry = Loading_pictures[(rand() & 0xff) % 6];
	Picture_shown = dtufe_loading_picture(entry);
}

void dtustate_loading_end(void)
{
	if (Picture_shown)
		dtufe_loading_picture_end();
	Picture_shown = 0;
}

//------------------------------------------------------------------------
// The test option -statetest DOORSEG,DOORSIDE,OBJ[,WARPSEG,BACKSEG]. In the
// game frame 30 the door of the side opens and object OBJ (a creature)
// gets 1000 points of damage. If WARPSEG is given, frame 150 puts the
// player into segment WARPSEG (a warp segment of DTU.WRP) and the first
// frame 60 frames after the next level start puts it into BACKSEG (the
// warp back). Each level start and each event prints a line "STATETEST"
// with the state of the door, of the object and of the player. With -1 as
// DOORSEG the doors and creatures of the level are listed.

static int St_on, St_door_seg, St_door_side, St_obj, St_warp_seg = -1, St_back_seg = -1;
static int St_frame, St_level_starts, St_back_done;

static void st_print(char *why)
{
	object *o = (St_obj >= 0 && St_obj <= Highest_object_index) ? &Objects[St_obj] : NULL;
	object *p = ConsoleObject;

	printf("STATETEST %s: level %d frame %d", why, Current_level_num, St_frame);
	if (St_door_seg >= 0 && St_door_seg <= Highest_segment_index) {
		int w = Segments[St_door_seg].sides[St_door_side].wall_num;
		if (w >= 0)
			printf("; door seg %d side %d wall %d type %d state %d flags 0x%x tmap2 %d", St_door_seg, St_door_side, w,
				Walls[w].type, Walls[w].state, Walls[w].flags, Segments[St_door_seg].sides[St_door_side].tmap_num2);
	}
	if (o)
		printf("; object %d type %d id %d shields %.1f flags 0x%x control %d", St_obj, o->type, o->id, f2fl(o->shields), o->flags, o->control_type);
	if (p)
		printf("; player seg %d pos %.2f %.2f %.2f", p->segnum, f2fl(p->pos.x), f2fl(p->pos.y), f2fl(p->pos.z));
	printf("\n");
	fflush(stdout);
}

static void st_list(void)
{
	int i;

	for (i = 0; i < Num_walls; i++)
		if (Walls[i].type == WALL_DOOR)
			printf("STATETEST door wall %d seg %d side %d flags 0x%x\n", i, Walls[i].segnum, Walls[i].sidenum, Walls[i].flags);
	for (i = 0; i <= Highest_object_index; i++)
		if (Objects[i].type == OBJ_ROBOT)
			printf("STATETEST creature %d id %d seg %d shields %.1f\n", i, Objects[i].id, Objects[i].segnum, f2fl(Objects[i].shields));
}

static void st_teleport(int segnum)
{
	vms_vector c;

	if (!ConsoleObject || segnum < 0 || segnum > Highest_segment_index)
		return;
	compute_segment_center(&c, &Segments[segnum]);
	ConsoleObject->pos = c;
	obj_relink(ConsoleObject - Objects, segnum);
}

static void st_level_start(void)
{
	static int checked;

	if (!checked) {
		int t = FindArg("-statetest");
		checked = 1;
		if (t && t + 1 < Num_args) {
			St_on = 1;
			sscanf(Args[t + 1], "%d,%d,%d,%d,%d", &St_door_seg, &St_door_side, &St_obj, &St_warp_seg, &St_back_seg);
		}
	}
	if (!St_on)
		return;
	St_level_starts++;
	St_frame = 0;
	if (St_door_seg < 0)
		st_list();
	st_print("level start");
}

void dtustate_test_frame(void)
{
	if (!St_on)
		return;
	St_frame++;
	if (St_level_starts == 1 && St_frame == 30 && St_door_seg >= 0) {
		segment *seg = &Segments[St_door_seg];
		if (seg->sides[St_door_side].wall_num >= 0)
			wall_open_door(seg, St_door_side);
		if (St_obj >= 0 && St_obj <= Highest_object_index && Objects[St_obj].type == OBJ_ROBOT)
			dtu_damage_creature_direct(&Objects[St_obj], ConsoleObject, i2f(1000));
		st_print("door opened, creature hit");
	}
	if (St_level_starts == 1 && St_frame == 149)
		st_print("before the warp");
	if (St_level_starts == 1 && St_frame == 150 && St_warp_seg >= 0)
		st_teleport(St_warp_seg);
	if (St_level_starts == 2 && St_frame == 60 && St_back_seg >= 0 && !St_back_done) {
		St_back_done = 1;
		st_teleport(St_back_seg);
	}
	if (St_frame % 100 == 0)
		st_print("frame");
}
