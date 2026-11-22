/*
 * ASCENT: new file. The level state files of Descent to Undermountain. It
 * replaces the code of DTUGAME.EXE 1.3 at 0x5A756 (save the state of a
 * level), 0x5AA75 (restore it), 0x5ABC6 and 0x5AC6F (new game), 0xBAD08
 * and 0xBB1A7 (the state file), 0xBC1EC and 0xBC51D (the copies in the
 * save slots), the door code of the warp (0x5A0F6) and the loading
 * picture of the level loader (0x7C1E5). See docs/re/levelstate.md and
 * docs/re/frontend.md.
 *
 * The files have the names, the roles and the format of the original
 * files, so the level states of the DOS game and of the port go both ways
 * (docs/re/levelstate.md, section 10).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <dirent.h>
#include <strings.h>
#endif

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "mem.h"				// dtu_load_file() allocates with mem_malloc
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "wall.h"
#include "switch.h"
#include "fuelcen.h"
#include "gamemine.h"
#include "bm.h"
#include "polyobj.h"
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

// The messages of the state code go to the log and to stdout
#define SLOG(...)	do { dtu_log(__VA_ARGS__); printf(__VA_ARGS__); } while (0)

#define LS_LEVELS		100		// records of the level table (fread(buf, 6, 100))
#define LS_RECORD		6
#define STATE_DIR		"SAVE"


typedef struct ls_record {
	int	temp;					// byte 0: temp<L>.* hold the state
	int	saved;				// byte 1: sg<slot>_<L>.* hold the state
	int	slot;					// s32 at 2
} ls_record;

int Dtu_level_has_state[LS_LEVELS];	// 0x296304

static int Cleanup_registered;

//------------------------------------------------------------------------
// Files of the user in any case of the letters. The DOS game writes upper
// case names (SG0.SG, SAVE\SG1_0.LVL); a copy of its files keeps them. On
// a file system that tells the case apart, each part of the name is
// looked up with no regard to case.

static int path_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0;
}

int dtu_user_find(const char *rel, char *out, int size)
{
	dtu_user_path(out, size, rel);
	if (path_exists(out))
		return 1;
#ifndef _WIN32
	{
		const char *base = dtu_user_dir(), *p = rel;
		char cur[600], part[256];

		snprintf(cur, sizeof(cur), "%s", base ? base : ".");
		while (*p) {
			const char *q = strchr(p, '/');
			int n = q ? (int) (q - p) : (int) strlen(p);
			char next[600];
			DIR *dir;
			struct dirent *e;

			if (n >= (int) sizeof(part))
				return 0;
			memcpy(part, p, n);
			part[n] = 0;
			snprintf(next, sizeof(next), "%s/%s", cur, part);
			if (!path_exists(next) && (dir = opendir(cur)) != NULL) {
				while ((e = readdir(dir)) != NULL)
					if (!strcasecmp(e->d_name, part)) {
						snprintf(next, sizeof(next), "%s/%s", cur, e->d_name);
						break;
					}
				closedir(dir);
			}
			if (!path_exists(next)) {
				// the rest of the name as it is given
				snprintf(out, size, "%s/%s", cur, p);
				return 0;
			}
			snprintf(cur, sizeof(cur), "%s", next);
			p = q ? q + 1 : p + n;
		}
		snprintf(out, size, "%s", cur);
		return 1;
	}
#else
	return 0;
#endif
}

FILE *dtu_user_fopen(const char *rel, const char *mode)
{
	char path[600];

	dtu_user_find(rel, path, sizeof(path));
	return fopen(path, mode);
}

void dtu_user_remove(const char *rel)
{
	char path[600];

	if (dtu_user_find(rel, path, sizeof(path)))
		remove(path);
}

static void state_rel(char *out, int size, const char *name)
{
	snprintf(out, size, "%s/%s", STATE_DIR, name);
}

static void make_dir(void)
{
	char dir[600];

	if (dtu_user_find(STATE_DIR, dir, sizeof(dir)))
		return;
#ifdef _WIN32
	_mkdir(dir);
#else
	mkdir(dir, 0777);
#endif
}

static FILE *state_open(const char *name, const char *mode)
{
	char rel[64];

	state_rel(rel, sizeof(rel), name);
	return dtu_user_fopen(rel, mode);
}

static void state_delete(const char *name)
{
	char rel[64];

	state_rel(rel, sizeof(rel), name);
	dtu_user_remove(rel);
}

static int state_exists(const char *name)
{
	char rel[64], path[600];

	state_rel(rel, sizeof(rel), name);
	return dtu_user_find(rel, path, sizeof(path));
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
	char rel[64], a[600], b[600];

	state_rel(rel, sizeof(rel), from);
	if (!dtu_user_find(rel, a, sizeof(a)))
		return 0;
	state_delete(to);
	state_rel(rel, sizeof(rel), to);
	dtu_user_find(rel, b, sizeof(b));
	if (rename(a, b) == 0)
		return 1;
	if (!copy_file(from, to))
		return 0;
	remove(a);
	return 1;
}

// The names of the executable ("temp%d.lvl" 0x128886, "sg%d_%d.lvl"
// 0x1288A0, "sg%d.ls" 0x133CAA, "CSUF.ls"), in the upper case of the DOS
// directory.
static const char *Ext[3] = { "LVL", "TIG", "TAP" };

static void temp_name(char *out, int level, int e)
{
	sprintf(out, "TEMP%d.%s", level, Ext[e]);
}

static void slot_name(char *out, int s, int level, int e)
{
	sprintf(out, "SG%d_%d.%s", s, level, Ext[e]);
}

#define CSUF_NAME	"CSUF.LS"

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
	state_delete(CSUF_NAME);
}

static void cleanup(void)
{
	// The close of the program (0x426FA) deletes the running state.
	if (state_exists(CSUF_NAME))
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
	if (!ls_write(CSUF_NAME, r))
		SLOG("State: cannot write %s/" CSUF_NAME "\n", STATE_DIR);
}

//------------------------------------------------------------------------
// The state file of a level (0xBAD08 writes, 0xBB1A7 reads). The port
// writes and reads the format of the executable (docs/re/levelstate.md,
// section 3): the memory images of the objects (479 bytes each), the
// walls, the moving doors, the sides, the side flags, the AI state and the
// automap. It also reads the files of older builds of the port (header
// "ASLS", the structures of the port).

#define IMG_SIZE			479		// the object struct of the executable (0x1DF)
#define AI_LOCAL_SIZE	47			// Ai_local_info (0x153EB0)
#define AI_N_LOCAL		800		// MAX_OBJECTS of the executable
#define AI_N_POINTS		2500		// Point_segs (0x15D190), 16 bytes each
#define AI_CLOAK_SIZE	128		// 0x153A30
#define AI_BLOCK_SIZE	(4 + 4 + AI_LOCAL_SIZE * AI_N_LOCAL + 16 * AI_N_POINTS + AI_CLOAK_SIZE + 4 + 4)
#define AI_LOCAL_OFF		8
#define AUTOMAP_SIZE		900		// 0x185FEC
#define STATE_VERSION_DGSS	12

// The images of the objects and the AI block of the state file that the
// running level came from. The writer starts from them, so the bytes that
// the port does not use go back into the file. An image belongs to the
// object with the same number and signature.
static ubyte Img[MAX_OBJECTS][IMG_SIZE];
static int Img_sig[MAX_OBJECTS];
static ubyte Img_ok[MAX_OBJECTS];
static ubyte Ai_block[AI_BLOCK_SIZE];
static int Ai_block_ok;

static void images_forget(void)
{
	memset(Img_ok, 0, sizeof(Img_ok));
	Ai_block_ok = 0;
}

static int gs16(const ubyte *p) { return (short) (p[0] | (p[1] << 8)); }
static int gu16(const ubyte *p) { return p[0] | (p[1] << 8); }
static int gs32(const ubyte *p) { return get32((ubyte *) p); }
static void ps16(ubyte *p, int v) { p[0] = (ubyte) v; p[1] = (ubyte) (v >> 8); }

static void gvec(vms_vector *v, const ubyte *p)
{
	v->x = gs32(p); v->y = gs32(p + 4); v->z = gs32(p + 8);
}

static void pvec(ubyte *p, const vms_vector *v)
{
	put32(p, v->x); put32(p + 4, v->y); put32(p + 8, v->z);
}

static void gmat(vms_matrix *m, const ubyte *p)
{
	gvec(&m->rvec, p); gvec(&m->uvec, p + 12); gvec(&m->fvec, p + 24);
}

static void pmat(ubyte *p, const vms_matrix *m)
{
	pvec(p, &m->rvec); pvec(p + 12, &m->uvec); pvec(p + 24, &m->fvec);
}

// The texture numbers. The executable keeps the numbers of the texture
// table of the level in the sides (verified: the sides of SAVE\SG1_0.LVL
// and SG1_3.LVL are those of YPINN.LVL and BONES.LVL). The port keeps the
// numbers of its texture table in the side and the numbers of the level
// in dtu_tmap_num, dtu_tmap_num2 (level_translate_textures()).
static short Tmap_rev[MAX_TEXTURES];

static void tmap_rev_make(void)
{
	int j;

	for (j = 0; j < MAX_TEXTURES; j++)
		Tmap_rev[j] = -1;
	for (j = 0; j < Num_level_tmap_names; j++)
		if (Level_tmap_xlate[j] >= 0 && Level_tmap_xlate[j] < MAX_TEXTURES && Tmap_rev[Level_tmap_xlate[j]] < 0)
			Tmap_rev[Level_tmap_xlate[j]] = j;
}

// game texture number to level texture number; keep is the level number
// that the side had
static int tmap_to_level(int tmap, int keep)
{
	if (keep >= 0 && keep < Num_level_tmap_names && Level_tmap_xlate[keep] == tmap)
		return keep;
	if (tmap >= 0 && tmap < MAX_TEXTURES && Tmap_rev[tmap] >= 0)
		return Tmap_rev[tmap];
	return keep;
}

static int tmap_from_level(int t)
{
	return (t >= 0 && t < Num_level_tmap_names) ? Level_tmap_xlate[t] : -1;
}

//------------------------------------------------------------------------
// The object image

// The level object with this signature (the level loader numbers the
// objects of the level file from 0, as 0x48C74), or NULL.
static object *Level_objs;
static int Level_n_objs;

static object *level_object(int signature, int type, int id)
{
	if (!Level_objs || signature < 0 || signature >= Level_n_objs)
		return NULL;
	if (Level_objs[signature].signature != signature || Level_objs[signature].type != type || Level_objs[signature].id != id)
		return NULL;
	return &Level_objs[signature];
}

static int Img_model_same, Img_model_other;

static void obj_from_image(object *o, const ubyte *b)
{
	object *lv;
	int i;

	memset(o, 0, sizeof(*o));
	o->signature = gs32(b);
	o->type = b[0x04];
	o->id = b[0x05];
	o->next = o->prev = -1;
	o->control_type = b[0x0A];
	o->movement_type = b[0x0B];
	o->render_type = b[0x0C];
	o->flags = b[0x0D];
	o->segnum = gs16(b + 0x0E);
	o->dtu.m_10 = gs16(b + 0x10);
	o->attached_obj = -1;
	gvec(&o->pos, b + 0x14);
	gmat(&o->orient, b + 0x20);
	gmat(&o->dtu.orient_copy, b + 0x44);
	o->size = gs32(b + 0x68);
	o->shields = gs32(b + 0x6C);
	gvec(&o->last_pos, b + 0x70);
	o->contains_type = b[0x7C];
	o->contains_id = b[0x7D];
	o->contains_count = b[0x7E];
	o->matcen_creator = b[0x7F];
	o->dtu.m_80 = gs16(b + 0x80);
	o->dtu.m_82 = gs16(b + 0x82);
	o->lifeleft = gs32(b + 0x84);
	o->dtu.m_88 = gs32(b + 0x88);
	for (i = 0; i < 13; i++) o->dtu.m_8c[i] = gs32(b + 0x8C + 4 * i);
	for (i = 0; i < 2; i++) o->dtu.m_c0[i] = gs32(b + 0xC0 + 4 * i);
	for (i = 0; i < 6; i++) o->dtu.m_c8[i] = gs32(b + 0xC8 + 4 * i);
	for (i = 0; i < 2; i++) o->dtu.m_e0[i] = gs32(b + 0xE0 + 4 * i);
	for (i = 0; i < 5; i++) o->dtu.m_e8[i] = gs32(b + 0xE8 + 4 * i);
	o->dtu.m_1b0 = gs32(b + 0x1B0);
	o->dtu.m_1d5 = b[0x1D5];

	switch (o->movement_type) {
		case MT_PHYSICS:
			gvec(&o->mtype.phys_info.velocity, b + 0xFC);
			gvec(&o->mtype.phys_info.thrust, b + 0x108);
			o->mtype.phys_info.mass = gs32(b + 0x114);
			o->mtype.phys_info.drag = gs32(b + 0x118);
			o->mtype.phys_info.brakes = gs32(b + 0x11C);
			gvec(&o->mtype.phys_info.rotvel, b + 0x120);
			gvec(&o->mtype.phys_info.rotthrust, b + 0x12C);
			o->mtype.phys_info.turnroll = gs16(b + 0x138);
			o->mtype.phys_info.flags = gu16(b + 0x13E);
			break;
		case MT_SPINNING:
			gvec(&o->mtype.spin_rate, b + 0xFC);
			break;
	}

	switch (o->control_type) {
		case CT_AI: {
			ai_static *a = &o->ctype.ai_info;
			a->behavior = b[0x140];
			for (i = 0; i < MAX_AI_FLAGS; i++)
				a->flags[i] = b[0x141 + i];
			a->hide_segment = gs16(b + 0x14C);
			a->hide_index = gs16(b + 0x14E);
			a->path_length = gs16(b + 0x150);
			a->cur_path_index = gs16(b + 0x152);
			a->follow_path_start_seg = gs16(b + 0x154);
			a->follow_path_end_seg = gs16(b + 0x156);
			a->danger_laser_signature = gs32(b + 0x158);
			a->danger_laser_num = gs16(b + 0x15C);
			break;
		}
		case CT_EXPLOSION:
		case CT_DEBRIS:
			o->ctype.expl_info.spawn_time = gs32(b + 0x140);
			o->ctype.expl_info.delete_time = gs32(b + 0x144);
			o->ctype.expl_info.delete_objnum = gs16(b + 0x148);
			o->ctype.expl_info.attach_parent = o->ctype.expl_info.prev_attach = o->ctype.expl_info.next_attach = -1;
			break;
		case CT_WEAPON:
			o->ctype.laser_info.parent_type = gs16(b + 0x140);
			o->ctype.laser_info.parent_num = gs16(b + 0x142);
			o->ctype.laser_info.parent_signature = gs32(b + 0x144);
			break;
		case CT_LIGHT:
			o->ctype.light_info.intensity = gs32(b + 0x140);
			o->dtu.light_m = gs16(b + 0x144);
			break;
		case CT_POWERUP:
			o->ctype.powerup_info.count = gs32(b + 0x140);
			break;
	}

	switch (o->render_type) {
		case RT_POLYOBJ:
		case RT_MORPH:
			o->rtype.pobj_info.model_num = gs32(b + 0x162);
			o->rtype.pobj_info.subobj_flags = gs32(b + 0x166);
			o->dtu.tmap_override_level = gs32(b + 0x16A);
			o->rtype.pobj_info.tmap_override = o->dtu.tmap_override_level;
			if (o->dtu.tmap_override_level >= 0)
				o->rtype.pobj_info.tmap_override = tmap_from_level(o->dtu.tmap_override_level);
			o->rtype.pobj_info.alt_textures = 0;
			o->dtu.m_16e = gs16(b + 0x16E);
			o->dtu.m_170 = gs16(b + 0x170);
			o->dtu.m_172 = gs32(b + 0x172);
			o->dtu.m_176 = gs32(b + 0x176);
			break;
		case RT_FIREBALL:
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
			o->rtype.vclip_info.vclip_num = gs32(b + 0x162);
			o->rtype.vclip_info.frametime = gs32(b + 0x166);
			o->rtype.vclip_info.framenum = b[0x16A];
			break;
		case RT_ITEM:
			o->dtu.item_bitmap = gs16(b + 0x162);
			break;
	}

	// The executable gives all scenery the movement type 1 (physics) and
	// moves pushed scenery itself (0x9FE68); the port moves it in dtuobj.c
	// with the movement type of the level file (none).
	if (o->type == OBJ_DRESSING)
		o->movement_type = MT_NONE;

	// The values that the port takes from the level file: the record of
	// the same object, or the player of the level.
	lv = level_object(o->signature, o->type, o->id);
	if (!lv && o->type == OBJ_PLAYER && Level_objs && Level_n_objs > 0 && Level_objs[0].type == OBJ_PLAYER)
		lv = &Level_objs[0];
	if (lv) {
		o->dtu.m_88_level = lv->dtu.m_88_level;
		o->dtu.pos_level = lv->dtu.pos_level;
		o->dtu.model_num_level = lv->dtu.model_num_level;
		o->dtu.record_size = lv->dtu.record_size;
		if ((o->render_type == RT_POLYOBJ || o->render_type == RT_MORPH)
			&& (lv->render_type == RT_POLYOBJ || lv->render_type == RT_MORPH)) {
			if (o->rtype.pobj_info.model_num + Bm_level_first_model == lv->rtype.pobj_info.model_num)
				Img_model_same++;
			else {
				Img_model_other++;
				if (FindArg("-statelog"))
					printf("STATELOG model: obj %d type %d id %d image %d port %d (%s)\n", o->signature, o->type, o->id,
						o->rtype.pobj_info.model_num, lv->rtype.pobj_info.model_num,
						lv->rtype.pobj_info.model_num >= 0 && lv->rtype.pobj_info.model_num < N_polygon_models ? Pof_names[lv->rtype.pobj_info.model_num] : "?");
			}
			o->rtype.pobj_info.model_num = lv->rtype.pobj_info.model_num;
		}
	}
	else
		o->dtu.pos_level = o->pos;
	// DTUGAME.EXE numbers the models of the level archive from 0; the port
	// has the models of the player archive first (Bm_level_first_model,
	// verified with SAVE\SG1_0.LVL, SG1_2.LVL and SG1_3.LVL). Creatures and
	// scenery get the model of their table entry again (setup_creature(),
	// dtu_objects_level_setup()).
	if ((o->render_type == RT_POLYOBJ || o->render_type == RT_MORPH) && !lv) {
		o->rtype.pobj_info.model_num += Bm_level_first_model;
		if (o->rtype.pobj_info.model_num < 0 || o->rtype.pobj_info.model_num >= N_polygon_models)
			o->rtype.pobj_info.model_num = 0;
	}
}

static void obj_to_image(int objnum, ubyte *b)
{
	object *o = &Objects[objnum];
	int i;

	if (Img_ok[objnum] && Img_sig[objnum] == o->signature)
		memcpy(b, Img[objnum], IMG_SIZE);
	else {
		memset(b, 0, IMG_SIZE);
		ps16(b + 0x12, -1);
	}
	put32(b, o->signature);
	b[0x04] = o->type;
	b[0x05] = o->id;
	ps16(b + 0x06, o->next);
	ps16(b + 0x08, o->prev);
	b[0x0A] = o->control_type;
	b[0x0B] = o->movement_type;
	b[0x0C] = o->render_type;
	b[0x0D] = o->flags;
	ps16(b + 0x0E, o->segnum);
	ps16(b + 0x10, o->dtu.m_10);
	pvec(b + 0x14, &o->pos);
	pmat(b + 0x20, &o->orient);
	pmat(b + 0x44, &o->dtu.orient_copy);
	put32(b + 0x68, o->size);
	put32(b + 0x6C, o->shields);
	pvec(b + 0x70, &o->last_pos);
	b[0x7C] = o->contains_type;
	b[0x7D] = o->contains_id;
	b[0x7E] = o->contains_count;
	b[0x7F] = o->matcen_creator;
	ps16(b + 0x80, o->dtu.m_80);
	ps16(b + 0x82, o->dtu.m_82);
	put32(b + 0x84, o->lifeleft);
	put32(b + 0x88, o->dtu.m_88);
	for (i = 0; i < 13; i++) put32(b + 0x8C + 4 * i, o->dtu.m_8c[i]);
	for (i = 0; i < 2; i++) put32(b + 0xC0 + 4 * i, o->dtu.m_c0[i]);
	for (i = 0; i < 6; i++) put32(b + 0xC8 + 4 * i, o->dtu.m_c8[i]);
	for (i = 0; i < 2; i++) put32(b + 0xE0 + 4 * i, o->dtu.m_e0[i]);
	for (i = 0; i < 5; i++) put32(b + 0xE8 + 4 * i, o->dtu.m_e8[i]);
	put32(b + 0x1B0, o->dtu.m_1b0);
	b[0x1D5] = o->dtu.m_1d5;

	// scenery: movement type 1 as in the executable (see obj_from_image)
	if (o->type == OBJ_DRESSING && o->movement_type == MT_NONE)
		b[0x0B] = MT_PHYSICS;

	switch (o->movement_type) {
		case MT_PHYSICS:
			pvec(b + 0xFC, &o->mtype.phys_info.velocity);
			pvec(b + 0x108, &o->mtype.phys_info.thrust);
			put32(b + 0x114, o->mtype.phys_info.mass);
			put32(b + 0x118, o->mtype.phys_info.drag);
			put32(b + 0x11C, o->mtype.phys_info.brakes);
			pvec(b + 0x120, &o->mtype.phys_info.rotvel);
			pvec(b + 0x12C, &o->mtype.phys_info.rotthrust);
			ps16(b + 0x138, o->mtype.phys_info.turnroll);
			ps16(b + 0x13E, o->mtype.phys_info.flags);
			break;
		case MT_SPINNING:
			pvec(b + 0xFC, &o->mtype.spin_rate);
			break;
	}

	switch (o->control_type) {
		case CT_AI: {
			ai_static *a = &o->ctype.ai_info;
			b[0x140] = a->behavior;
			for (i = 0; i < MAX_AI_FLAGS; i++)
				b[0x141 + i] = a->flags[i];
			ps16(b + 0x14C, a->hide_segment);
			ps16(b + 0x14E, a->hide_index);
			ps16(b + 0x150, a->path_length);
			ps16(b + 0x152, a->cur_path_index);
			ps16(b + 0x154, a->follow_path_start_seg);
			ps16(b + 0x156, a->follow_path_end_seg);
			put32(b + 0x158, a->danger_laser_signature);
			ps16(b + 0x15C, a->danger_laser_num);
			break;
		}
		case CT_EXPLOSION:
		case CT_DEBRIS:
			put32(b + 0x140, o->ctype.expl_info.spawn_time);
			put32(b + 0x144, o->ctype.expl_info.delete_time);
			ps16(b + 0x148, o->ctype.expl_info.delete_objnum);
			ps16(b + 0x14A, -1);
			ps16(b + 0x14C, -1);
			ps16(b + 0x14E, -1);
			break;
		case CT_WEAPON:
			ps16(b + 0x140, o->ctype.laser_info.parent_type);
			ps16(b + 0x142, o->ctype.laser_info.parent_num);
			put32(b + 0x144, o->ctype.laser_info.parent_signature);
			break;
		case CT_LIGHT:
			put32(b + 0x140, o->ctype.light_info.intensity);
			ps16(b + 0x144, o->dtu.light_m);
			break;
		case CT_POWERUP:
			put32(b + 0x140, o->ctype.powerup_info.count);
			break;
	}

	switch (o->render_type) {
		case RT_POLYOBJ:
		case RT_MORPH:
			// the number of the level archive (see obj_from_image). A model of
			// the player archive has another number in the executable: the
			// number of the file the level came from stays (the game sets
			// the model of the player from the weapon in the hand).
			if (o->rtype.pobj_info.model_num >= Bm_level_first_model)
				put32(b + 0x162, o->rtype.pobj_info.model_num - Bm_level_first_model);
			else if (!(Img_ok[objnum] && Img_sig[objnum] == o->signature))
				put32(b + 0x162, 0);
			put32(b + 0x166, o->rtype.pobj_info.subobj_flags);
			put32(b + 0x16A, o->rtype.pobj_info.tmap_override < 0 ? o->rtype.pobj_info.tmap_override
				: tmap_to_level(o->rtype.pobj_info.tmap_override, o->dtu.tmap_override_level));
			ps16(b + 0x16E, o->dtu.m_16e);
			ps16(b + 0x170, o->dtu.m_170);
			put32(b + 0x172, o->dtu.m_172);
			put32(b + 0x176, o->dtu.m_176);
			break;
		case RT_FIREBALL:
		case RT_WEAPON_VCLIP:
		case RT_HOSTAGE:
		case RT_POWERUP:
			put32(b + 0x162, o->rtype.vclip_info.vclip_num);
			put32(b + 0x166, o->rtype.vclip_info.frametime);
			b[0x16A] = o->rtype.vclip_info.framenum;
			break;
		case RT_ITEM:
			ps16(b + 0x162, o->dtu.item_bitmap);
			break;
	}
}

//------------------------------------------------------------------------
// The writer (0xBAD08)

typedef struct membuf {
	ubyte	*p;
	int	n, size, ok;
} membuf;

static void mb_add(membuf *m, const void *data, int n)
{
	if (!m->ok)
		return;
	if (m->n + n > m->size) {
		int size = (m->n + n) * 2 + 4096;
		ubyte *p = malloc(size);			// mem.h has no realloc
		if (!p) {
			m->ok = 0;
			return;
		}
		if (m->p) {
			memcpy(p, m->p, m->n);
			free(m->p);
		}
		m->p = p;
		m->size = size;
	}
	if (data)
		memcpy(m->p + m->n, data, n);
	else
		memset(m->p + m->n, 0, n);
	m->n += n;
}

static void mb_int(membuf *m, int v)
{
	ubyte b[4];

	put32(b, v);
	mb_add(m, b, 4);
}

static int write_buffer(const char *name, membuf *m)
{
	FILE *f;
	int ok = m->ok;

	make_dir();
	f = state_open(name, "wb");
	if (!f)
		return 0;
	if (ok && fwrite(m->p, 1, m->n, f) != (size_t) m->n)
		ok = 0;
	if (fclose(f))
		ok = 0;
	return ok;
}

static int write_lvl(const char *name)
{
	membuf m = { NULL, 0, 0, 1 };
	ubyte b[IMG_SIZE], *ai;
	int i, s, n, ok;

	tmap_rev_make();
	mb_add(&m, "DGSS", 4);					// the start of the version text of autosave.c (0x146938)
	mb_int(&m, STATE_VERSION_DGSS);

	// 1. the objects (0x5EDEB: a morph object gets its model back; the port
	//    has no morph objects in DTU levels)
	n = Highest_object_index + 1;
	mb_int(&m, n);
	for (i = 0; i < n; i++) {
		obj_to_image(i, b);
		mb_add(&m, b, IMG_SIZE);
	}

	// 2. the walls (24 bytes) and the moving doors (16 bytes). The port
	//    takes a door out of the list when it is open; the executable keeps
	//    it there (docs/re/levelstate.md, section 6), so an open door goes
	//    into the list of the file.
	mb_int(&m, Num_walls);
	for (i = 0; i < Num_walls; i++) {
		wall *w = &Walls[i];
		ubyte r[24];

		put32(r, w->segnum);
		put32(r + 4, w->sidenum);
		put32(r + 8, w->hps);
		put32(r + 12, w->linked_wall);
		r[16] = w->type;
		r[17] = w->flags;
		r[18] = w->state;
		r[19] = (ubyte) w->trigger;
		r[20] = (ubyte) w->clip_num;
		r[21] = w->keys;
		ps16(r + 22, w->pad);
		mb_add(&m, r, 24);
	}
	{
		active_door doors[MAX_DOORS];
		int nd = Num_open_doors, j;

		memcpy(doors, ActiveDoors, sizeof(active_door) * Num_open_doors);
		for (i = 0; i < Num_walls && nd < MAX_DOORS; i++) {
			wall *w = &Walls[i];
			segment *seg, *csegp;
			int cside, listed = 0;

			if (w->type != WALL_DOOR || w->state != WALL_DOOR_OPENING || !(w->flags & WALL_DOOR_OPENED) || w->segnum < 0 || w->segnum > Highest_segment_index)
				continue;
			for (j = 0; j < nd; j++)
				if (doors[j].front_wallnum[0] == i || doors[j].back_wallnum[0] == i)
					listed = 1;
			if (listed)
				continue;
			seg = &Segments[w->segnum];
			doors[nd].n_parts = 1;
			doors[nd].front_wallnum[0] = i;
			doors[nd].back_wallnum[0] = -1;
			doors[nd].front_wallnum[1] = doors[nd].back_wallnum[1] = -1;
			doors[nd].time = (w->clip_num >= 0 && w->clip_num < Num_wall_anims) ? WallAnims[w->clip_num].play_time : 0;
			if (seg->children[w->sidenum] >= 0) {
				csegp = &Segments[seg->children[w->sidenum]];
				cside = find_connect_side(seg, csegp);
				if (cside >= 0)
					doors[nd].back_wallnum[0] = csegp->sides[cside].wall_num;
			}
			nd++;
		}
		mb_int(&m, nd);
		for (i = 0; i < nd; i++) {
			ubyte r[16];

			put32(r, doors[i].n_parts);
			ps16(r + 4, doors[i].front_wallnum[0]);
			ps16(r + 6, doors[i].front_wallnum[1]);
			ps16(r + 8, doors[i].back_wallnum[0]);
			ps16(r + 10, doors[i].back_wallnum[1]);
			put32(r + 12, doors[i].time);
			mb_add(&m, r, 16);
		}
	}

	// 3. the sides: wall, texture 1, texture 2 (0xBAEEE), with the texture
	//    numbers of the level; then the side flags (0x4AB04)
	for (i = 0; i <= Highest_segment_index; i++) {
		segment *seg = &Segments[i];

		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++) {
			side *sp = &seg->sides[s];
			ubyte r[6];
			int t1, t2, base = sp->tmap_num2 & 0x3FFF, orient = sp->tmap_num2 & ~0x3FFF;

			t1 = (seg->dtu_tmap_missing & (1 << s)) && sp->tmap_num == 0 ? seg->dtu_tmap_num[s]
				: tmap_to_level(sp->tmap_num, seg->dtu_tmap_num[s]);
			if (base == 0)
				t2 = (seg->dtu_tmap2_missing & (1 << s)) ? seg->dtu_tmap_num2[s] : 0;
			else
				t2 = tmap_to_level(base, seg->dtu_tmap_num2[s] & 0x3FFF) | orient;
			ps16(r, sp->wall_num);
			ps16(r + 2, t1);
			ps16(r + 4, t2);
			mb_add(&m, r, 6);
		}
	}
	mb_int(&m, Highest_segment_index);
	for (i = 0; i <= Highest_segment_index; i++)
		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++)
			mb_add(&m, &Segments[i].sides[s].pad, 1);

	// 4. the AI state (0x16413): u32 0x13A6C0, u32 Overall_agitation
	//    (0x167174), Ai_local_info, Point_segs, Ai_cloak_info, two u32.
	//    The port keeps the AI state of a creature in dtuobj.c and its path
	//    in its own list: the block of the file that the level came from
	//    stays, with the records of the creatures of the port. A level with
	//    no such file gets the values of a new level (0x13A6C0 = 1, the
	//    last u32 = 6.0 as in all files of the install) and no paths.
	mb_add(&m, NULL, AI_BLOCK_SIZE);
	ai = m.ok ? m.p + m.n - AI_BLOCK_SIZE : NULL;
	if (ai) {
		if (Ai_block_ok)
			memcpy(ai, Ai_block, AI_BLOCK_SIZE);
		else {
			put32(ai, 1);
			put32(ai + AI_BLOCK_SIZE - 4, 6 * F1_0);
		}
		for (i = 0; i < n && i < AI_N_LOCAL; i++) {
			object *o = &Objects[i];
			ubyte *img = m.p + 12 + (size_t) i * IMG_SIZE;
			ubyte *al = ai + AI_LOCAL_OFF + i * AI_LOCAL_SIZE;

			if (o->type != OBJ_ROBOT)
				continue;
			if (!(Img_ok[i] && Img_sig[i] == o->signature)) {
				// 0xAE3F2 and 0x1038B: the values of a new creature
				memset(al, 0, AI_LOCAL_SIZE);
				al[0x2C] = 0xFF;
			}
			if (!Ai_block_ok && o->control_type == CT_AI) {
				ps16(img + 0x150, 0);		// path_length: no points in Point_segs
				ps16(img + 0x152, 0);
			}
			if (o->control_type == CT_AI)
				dtu_ai_to_image(i, al, img);
		}
	}

	// 5. the automap (900 bytes, one for each segment)
	{
		ubyte am[AUTOMAP_SIZE];

		memset(am, 0, sizeof(am));
		for (i = 0; i <= Highest_segment_index && i < AUTOMAP_SIZE && i < MAX_SEGMENTS; i++)
			am[i] = Automap_visited[i];
		mb_add(&m, am, AUTOMAP_SIZE);
	}

	ok = write_buffer(name, &m);
	free(m.p);
	return ok;
}

static int write_tig(const char *name)
{
	membuf m = { NULL, 0, 0, 1 };
	int ok;

	m.p = dtu_trig_write_tig(&m.n);
	if (!m.p)
		return 0;
	m.size = m.n;
	ok = write_buffer(name, &m);
	free(m.p);
	return ok;
}

static int write_tap(const char *name)
{
	membuf m = { NULL, 0, 0, 1 };
	int ok;

	m.p = dtu_trig_write_tap(&m.n);
	if (!m.p)
		return 0;
	m.size = m.n;
	ok = write_buffer(name, &m);
	free(m.p);
	return ok;
}

//------------------------------------------------------------------------
// The reader of the format of the executable (0xBB1A7)

static ubyte *load_state_file(const char *name, int *size)
{
	FILE *f = state_open(name, "rb");
	ubyte *data;
	long n;

	if (!f)
		return NULL;
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = (n > 0) ? malloc(n) : NULL;
	if (data && fread(data, 1, n, f) != (size_t) n) {
		free(data);
		data = NULL;
	}
	fclose(f);
	*size = (int) n;
	return data;
}

static int read_lvl_dgss(const char *name, ubyte *d, int size)
{
	int o = 8, n, nw, nd, nseg, rest, i, s, segnum, ok = 1;
	const ubyte *objs, *sides, *flags, *ai, *am;

	// The layout (the counts tell the sizes; the number of segments is
	// known only from the size of the rest)
	n = gs32(d + 8);
	if (gs32(d + 4) != STATE_VERSION_DGSS || n < 1 || n > MAX_OBJECTS || o + 4 + n * IMG_SIZE > size)
		return 0;
	objs = d + o + 4;
	o += 4 + n * IMG_SIZE;
	nw = gs32(d + o);
	if (nw < 0 || nw > MAX_WALLS || o + 4 + nw * 24 + 4 > size)
		return 0;
	o += 4;
	{
		int wo = o;

		o += nw * 24;
		nd = gs32(d + o);
		if (nd < 0 || nd > MAX_DOORS || o + 4 + nd * 16 > size)
			return 0;
		o += 4;
		rest = size - o - nd * 16 - 4 - AI_BLOCK_SIZE - AUTOMAP_SIZE;
		if (rest <= 0 || rest % 42)
			return 0;
		nseg = rest / 42;
		if (nseg != Highest_segment_index + 1) {
			SLOG("State: %s has %d segments, the level %d\n", name, nseg, Highest_segment_index + 1);
			return 0;
		}
		sides = d + o + nd * 16;
		if (gs32(sides + nseg * 36) != Highest_segment_index)
			return 0;
		flags = sides + nseg * 36 + 4;
		ai = flags + nseg * 6;
		am = ai + AI_BLOCK_SIZE;

		// The objects. The level is loaded: keep its objects for the values
		// of the level file.
		Level_n_objs = Highest_object_index + 1;
		Level_objs = malloc(sizeof(object) * Level_n_objs);
		if (Level_objs)
			memcpy(Level_objs, Objects, sizeof(object) * Level_n_objs);
		tmap_rev_make();
		for (i = 0; i <= Highest_segment_index; i++)
			Segments[i].objects = -1;
		reset_objects(1);
		images_forget();
		Img_model_same = Img_model_other = 0;
		if (FindArg("-statelog"))
			for (i = 0; i < N_polygon_models; i++)
				printf("STATELOG pof %d %s\n", i, Pof_names[i]);
		Highest_object_index = n - 1;
		Object_next_signature = 0;
		for (i = 0; i < n; i++) {
			object *obj = &Objects[i];
			const ubyte *b = objs + i * IMG_SIZE;

			obj_from_image(obj, b);
			memcpy(Img[i], b, IMG_SIZE);
			Img_sig[i] = obj->signature;
			Img_ok[i] = 1;
			if (obj->signature > Object_next_signature)
				Object_next_signature = obj->signature;
			segnum = obj->segnum;
			obj->segnum = -1;
			if (obj->type == OBJ_NONE)
				continue;
			// 0xBB2E6: an object of type 1 (fireball) or with no segment is killed
			if (obj->type == OBJ_FIREBALL || segnum < 0 || segnum > Highest_segment_index) {
				SLOG("STATE: killing object %d -- illegal segment\n", i);
				obj->type = OBJ_NONE;
				continue;
			}
			// 0xBAFE0 with 0x6A3CC: a dead creature with the flag 0x40 (not
			// drawn, not moved) stays in the list of the executable and does
			// nothing. The port deletes a dead creature (OF_SHOULD_BE_DEAD);
			// it is deleted here, before it is placed.
			if (obj->type == OBJ_ROBOT && (obj->flags & 0x40) && (obj->flags & OF_SHOULD_BE_DEAD)) {
				obj->type = OBJ_NONE;
				continue;
			}
			obj_link(i, segnum);
		}
		special_reset_objects();
		Object_next_signature++;
		if (Level_objs)
			free(Level_objs);
		Level_objs = NULL;
		Level_n_objs = 0;
		SLOG("State: %s: %d objects, model numbers: %d as in the level, %d others\n", name, n, Img_model_same, Img_model_other);

		// The walls and the moving doors
		Num_walls = nw;
		for (i = 0; i < nw; i++) {
			const ubyte *r = d + wo + i * 24;
			wall *w = &Walls[i];

			w->segnum = gs32(r);
			w->sidenum = gs32(r + 4);
			w->hps = gs32(r + 8);
			w->linked_wall = gs32(r + 12);
			w->type = r[16];
			w->flags = r[17];
			w->state = r[18];
			w->trigger = (byte) r[19];
			w->clip_num = (byte) r[20];
			w->keys = r[21];
			w->pad = gs16(r + 22);
		}
		Num_open_doors = nd;
		for (i = 0; i < nd; i++) {
			const ubyte *r = d + o + i * 16;
			active_door *ad = &ActiveDoors[i];

			ad->n_parts = gs32(r);
			ad->front_wallnum[0] = gs16(r + 4);
			ad->front_wallnum[1] = gs16(r + 6);
			ad->back_wallnum[0] = gs16(r + 8);
			ad->back_wallnum[1] = gs16(r + 10);
			ad->time = gs32(r + 12);
		}
	}

	// The sides: the numbers of the level texture table
	for (i = 0; i <= Highest_segment_index; i++) {
		segment *seg = &Segments[i];

		seg->dtu_tmap_missing = seg->dtu_tmap2_missing = 0;
		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++) {
			const ubyte *r = sides + (i * 6 + s) * 6;
			side *sp = &seg->sides[s];
			int t1 = gs16(r + 2), t2 = gs16(r + 4), x;

			sp->wall_num = gs16(r);
			seg->dtu_tmap_num[s] = t1;
			seg->dtu_tmap_num2[s] = t2;
			x = tmap_from_level(t1);
			if (x >= 0)
				sp->tmap_num = x;
			else {
				sp->tmap_num = 0;
				seg->dtu_tmap_missing |= 1 << s;
			}
			sp->tmap_num2 = 0;
			if (t2 & 0x3FFF) {
				x = tmap_from_level(t2 & 0x3FFF);
				if (x > 0)
					sp->tmap_num2 = x | (t2 & ~0x3FFF);
				else
					seg->dtu_tmap2_missing |= 1 << s;
			}
			sp->pad = flags[i * 6 + s];
		}
	}

	// The AI state: the block stays for the writer; the creatures get it
	// after dtu_objects_level_setup() (restore_files())
	memcpy(Ai_block, ai, AI_BLOCK_SIZE);
	Ai_block_ok = 1;

	// The automap
	for (i = 0; i <= Highest_segment_index && i < AUTOMAP_SIZE && i < MAX_SEGMENTS; i++)
		Automap_visited[i] = am[i];
	return ok;
}

// The AI records of the creatures (after dtu_objects_level_setup())
static void ai_restore(void)
{
	int i;

	if (!Ai_block_ok)
		return;
	for (i = 0; i <= Highest_object_index && i < AI_N_LOCAL; i++)
		if (Img_ok[i] && Img_sig[i] == Objects[i].signature && Objects[i].type == OBJ_ROBOT)
			dtu_ai_from_image(i, Ai_block + AI_LOCAL_OFF + i * AI_LOCAL_SIZE, Img[i]);
	if (FindArg("-statelog"))
		for (i = 0; i <= Highest_object_index; i++) {
			object *o = &Objects[i];
			if (o->type != OBJ_NONE && find_point_seg(&o->pos, o->segnum) < 0)
				printf("STATELOG outside: obj %d type %d id %d flags 0x%x seg %d render %d size %.2f\n", i, o->type, o->id, o->flags,
					o->segnum, o->render_type, f2fl(o->size));
		}
}

//------------------------------------------------------------------------
// The files of older builds of the port (header "ASLS", "ASLT", "ASLP":
// the structures of the port). Only read.

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
	h->version = 1;
	h->object_size = sizeof(object);
	h->wall_size = sizeof(wall);
	h->door_size = sizeof(active_door);
	h->trigger_size = sizeof(dtu_trigger);
	h->trap_size = sizeof(dtu_trap);
	h->n_segments = Highest_segment_index + 1;
}

static int header_check(const ubyte **pp, const ubyte *end, const char *magic)
{
	state_header want;

	header_init(&want, magic);
	if (end - *pp < (int) sizeof(want) || memcmp(&want, *pp, sizeof(want))) {
		SLOG("State: the file is not a state file of this level and this program\n");
		return 0;
	}
	*pp += sizeof(want);
	return 1;
}

static int take(const ubyte **pp, const ubyte *end, void *dest, size_t n)
{
	if (end - *pp < (long) n)
		return 0;
	memcpy(dest, *pp, n);
	*pp += n;
	return 1;
}

static int read_lvl_port(ubyte *d, int size)
{
	const ubyte *p = d, *end = d + size;
	int i, s, n, segnum, ok = 1;

	if (!header_check(&p, end, "ASLS") || !take(&p, end, &n, sizeof(n)) || n < 1 || n > MAX_OBJECTS)
		return 0;

	for (i = 0; i <= Highest_segment_index; i++)
		Segments[i].objects = -1;
	reset_objects(1);
	images_forget();
	if (!take(&p, end, Objects, sizeof(object) * n))
		ok = 0;
	Highest_object_index = n - 1;
	Object_next_signature = 0;
	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];

		segnum = obj->segnum;
		obj->next = obj->prev = obj->segnum = -1;
		if (obj->type == OBJ_NONE)
			continue;
		if (obj->type == OBJ_FIREBALL || segnum < 0 || segnum > Highest_segment_index) {
			SLOG("STATE: killing object %d -- illegal segment\n", i);
			obj->type = OBJ_NONE;
			continue;
		}
		obj_link(i, segnum);
		if (obj->signature > Object_next_signature)
			Object_next_signature = obj->signature;
	}
	special_reset_objects();
	Object_next_signature++;

	if (!take(&p, end, &Num_walls, sizeof(int)) || Num_walls < 0 || Num_walls > MAX_WALLS) {
		Num_walls = 0;
		ok = 0;
	} else if (!take(&p, end, Walls, sizeof(wall) * Num_walls))
		ok = 0;
	if (!take(&p, end, &Num_open_doors, sizeof(int)) || Num_open_doors < 0 || Num_open_doors > MAX_DOORS) {
		Num_open_doors = 0;
		ok = 0;
	} else if (!take(&p, end, ActiveDoors, sizeof(active_door) * Num_open_doors))
		ok = 0;
	for (i = 0; i <= Highest_segment_index && ok; i++)
		for (s = 0; s < MAX_SIDES_PER_SEGMENT; s++) {
			side *sp = &Segments[i].sides[s];
			if (!take(&p, end, &sp->wall_num, sizeof(short)) || !take(&p, end, &sp->tmap_num, sizeof(short))
				|| !take(&p, end, &sp->tmap_num2, sizeof(short)) || !take(&p, end, &sp->pad, 1))
				ok = 0;
		}
	if (!take(&p, end, Automap_visited, Highest_segment_index + 1))
		ok = 0;
	return ok;
}

static int read_tig_port(ubyte *d, int size)
{
	const ubyte *p = d, *end = d + size;
	int n;

	if (header_check(&p, end, "ASLT") && take(&p, end, &n, sizeof(n)) && n >= 0 && n <= DTU_MAX_TRIGGERS
		&& take(&p, end, Dtu_trigger_seg_table, sizeof(Dtu_trigger_seg_table))
		&& take(&p, end, Dtu_triggers, sizeof(dtu_trigger) * n)) {
		Dtu_num_triggers = n;
		return 1;
	}
	return 0;
}

static int read_tap_port(ubyte *d, int size)
{
	const ubyte *p = d, *end = d + size;
	int n;

	if (header_check(&p, end, "ASLP") && take(&p, end, &n, sizeof(n)) && n >= 0 && n <= DTU_MAX_TRAPS
		&& take(&p, end, Dtu_traps, sizeof(dtu_trap) * n)) {
		Dtu_num_traps = n;
		return 1;
	}
	return 0;
}

static int read_lvl(const char *name)
{
	int size, ok = 0;
	ubyte *d = load_state_file(name, &size);

	if (!d)
		return 0;
	if (size >= 12 && !memcmp(d, "DGSS", 4))
		ok = read_lvl_dgss(name, d, size);
	else if (size >= 4 && !memcmp(d, "ASLS", 4))
		ok = read_lvl_port(d, size);
	if (!ok)
		SLOG("State: %s is not a state file of this level\n", name);
	free(d);
	return ok;
}

static int read_tig(const char *name)
{
	int size, ok = 0;
	ubyte *d = load_state_file(name, &size);

	if (!d)
		return 0;
	if (size >= 4 && !memcmp(d, "ASLT", 4))
		ok = read_tig_port(d, size);
	else
		ok = dtu_trig_read_tig(d, size, 1);
	free(d);
	return ok;
}

static int read_tap(const char *name)
{
	int size, ok = 0;
	ubyte *d = load_state_file(name, &size);

	if (!d)
		return 0;
	if (size >= 4 && !memcmp(d, "ASLP", 4))
		ok = read_tap_port(d, size);
	else
		ok = dtu_trig_read_tap(d, size, 1);
	free(d);
	return ok;
}

//------------------------------------------------------------------------
// Save and restore a level

int dtustate_save_level(int level_index)
{
	ls_record r[LS_LEVELS];
	char name[32];
	int ok;

	if (level_index < 0 || level_index >= LS_LEVELS || !ls_read(CSUF_NAME, r)) {
		printf("State: no level table %s/" CSUF_NAME "\n", STATE_DIR);
		return 0;
	}
	r[level_index].temp = 1;
	Dtu_level_has_state[level_index] = 1;
	if (!ls_write(CSUF_NAME, r))
		return 0;
	temp_name(name, level_index, 0);
	ok = write_lvl(name);
	temp_name(name, level_index, 1);
	ok &= write_tig(name);
	temp_name(name, level_index, 2);
	ok &= write_tap(name);
	SLOG("State: %s the state of level %d\n", ok ? "saved" : "cannot save", level_index);
	return ok;
}

static int restore_files(char names[3][32], int level_index)
{
	if (!read_lvl(names[0]))
		return 0;
	read_tig(names[1]);
	read_tap(names[2]);
	// 0x5AA75 calls 0x5A855(0) after the read of the triggers and traps
	dtu_trig_state_rules(level_index);
	// 0xBAFE0 sets each object up again from the tables (0xAE3F2). The
	// port does it for all objects of the level with the function of the
	// level start.
	dtu_objects_level_setup_restore();
	// the AI state of the creatures of a file of the executable
	ai_restore();
	return 1;
}

int dtustate_restore_level(int level_index)
{
	ls_record r[LS_LEVELS];
	char names[3][32];
	int e;

	if (level_index < 0 || level_index >= LS_LEVELS || !ls_read(CSUF_NAME, r))
		return 0;
	if (r[level_index].temp) {
		for (e = 0; e < 3; e++)
			temp_name(names[e], level_index, e);
	} else if (r[level_index].saved && r[level_index].slot >= 0) {
		for (e = 0; e < 3; e++)
			slot_name(names[e], r[level_index].slot, level_index, e);
	} else
		return 0;
	SLOG("Restoring %s.\n", names[0]);
	if (!restore_files(names, level_index)) {
		SLOG("State: cannot read %s\n", names[0]);
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

	if (!ls_read(CSUF_NAME, cs))
		return 0;
	sprintf(sname, "SG%d.LS", s);					// "sg%d.ls" (0x133CAA)
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
	return ls_write(sname, cs) && ls_write(CSUF_NAME, cs);
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
	sprintf(sname, "SG%d.LS", s);
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
	return ls_write(CSUF_NAME, r);
}

//------------------------------------------------------------------------
// The calls of the game

void dtustate_warp_leave(int level_index)
{
	dtustate_close_doors();
	dtustate_save_level(level_index);
}

static void st_level_start(void);
static void apply_save_lists(void);

void dtustate_warp_enter(int level_index)
{
	images_forget();
	dtustate_restore_level(level_index);
	dtustate_loading_end();
	st_level_start();
}

void dtustate_game_started(int level_num)
{
	// 0x4DD5A: StartNewGame restores level 0. A load of a save game in a
	// level (0xBBDE5) restores the level of the save; the port starts that
	// level through StartNewGame.
	images_forget();
	apply_save_lists();
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

//------------------------------------------------------------------------
// ASCENT: the three lists of a save in a level (SG file offset 7374,
// 0xBAA2A writes, 0xBBCF2 reads): Num_triggers and the Descent 1 triggers
// (54 bytes), Num_robot_centers and the matcens (16 bytes), Num_fuelcenters
// and the fuel centers (40 bytes). The load applies them after the level
// start and before the level state, as 0xBB780 does.

#define TRIGGER_SIZE	54
#define MATCEN_SIZE		16
#define FUELCEN_SIZE	40

static ubyte *Save_lists;
static int Save_lists_size;

void dtustate_set_save_lists(const ubyte *data, int size)
{
	if (Save_lists)
		free(Save_lists);
	Save_lists = NULL;
	Save_lists_size = 0;
	if (data && size >= 12 && (Save_lists = malloc(size)) != NULL) {
		memcpy(Save_lists, data, size);
		Save_lists_size = size;
	}
}

static void apply_save_lists(void)
{
	const ubyte *p = Save_lists, *end = Save_lists + Save_lists_size;
	int i, k, n;

	if (!Save_lists)
		return;
	// Three counts of 0: the level keeps its lists. The saves of older
	// builds of the port have three counts of 0 for each level.
	if (Save_lists_size >= 12 && !gs32(p) && !gs32(p + 4) && !gs32(p + 8)) {
		dtustate_set_save_lists(NULL, 0);
		return;
	}
	n = gs32(p);
	p += 4;
	if (n < 0 || n > MAX_TRIGGERS || end - p < n * TRIGGER_SIZE)
		goto bad;
	Num_triggers = n;
	for (i = 0; i < n; i++, p += TRIGGER_SIZE) {
		trigger *t = &Triggers[i];

		t->type = p[0];
		t->flags = gs16(p + 1);
		t->value = gs32(p + 3);
		t->time = gs32(p + 7);
		t->link_num = p[11];
		t->num_links = gs16(p + 12);
		for (k = 0; k < 10 && k < MAX_WALLS_PER_LINK; k++) {
			t->seg[k] = gs16(p + 14 + 2 * k);
			t->side[k] = gs16(p + 34 + 2 * k);
		}
	}
	if (end - p < 4)
		goto bad;
	n = gs32(p);
	p += 4;
	if (n < 0 || n > MAX_ROBOT_CENTERS || end - p < n * MATCEN_SIZE)
		goto bad;
	Num_robot_centers = n;
	for (i = 0; i < n; i++, p += MATCEN_SIZE) {
		RobotCenters[i].robot_flags = gs32(p);
		RobotCenters[i].hit_points = gs32(p + 4);
		RobotCenters[i].interval = gs32(p + 8);
		RobotCenters[i].segnum = gs16(p + 12);
		RobotCenters[i].fuelcen_num = gs16(p + 14);
	}
	if (end - p < 4)
		goto bad;
	n = gs32(p);
	p += 4;
	if (n < 0 || n > MAX_NUM_FUELCENS || end - p < n * FUELCEN_SIZE)
		goto bad;
	Num_fuelcenters = n;
	for (i = 0; i < n; i++, p += FUELCEN_SIZE) {
		FuelCenter *f = &Station[i];

		f->Type = gs32(p);
		f->segnum = gs32(p + 4);
		f->Flag = p[8];
		f->Enabled = p[9];
		f->Lives = p[10];
		f->dum1 = p[11];
		f->Capacity = gs32(p + 12);
		f->MaxCapacity = gs32(p + 16);
		f->Timer = gs32(p + 20);
		f->Disable_time = gs32(p + 24);
		gvec(&f->Center, p + 28);
	}
	SLOG("State: the lists of the save game: %d triggers, %d matcens, %d fuel centers\n", Num_triggers, Num_robot_centers, Num_fuelcenters);
	dtustate_set_save_lists(NULL, 0);
	return;
bad:
	SLOG("State: the lists of the save game are damaged\n");
	dtustate_set_save_lists(NULL, 0);
}

int dtustate_write_save_lists(ubyte *b, int size)
{
	int i, k, n = 12 + Num_triggers * TRIGGER_SIZE + Num_robot_centers * MATCEN_SIZE + Num_fuelcenters * FUELCEN_SIZE;
	ubyte *p = b;

	if (n > size)
		return 0;
	memset(b, 0, n);
	put32(p, Num_triggers);
	p += 4;
	for (i = 0; i < Num_triggers; i++, p += TRIGGER_SIZE) {
		trigger *t = &Triggers[i];

		p[0] = t->type;
		ps16(p + 1, t->flags);
		put32(p + 3, t->value);
		put32(p + 7, t->time);
		p[11] = t->link_num;
		ps16(p + 12, t->num_links);
		for (k = 0; k < 10 && k < MAX_WALLS_PER_LINK; k++) {
			ps16(p + 14 + 2 * k, t->seg[k]);
			ps16(p + 34 + 2 * k, t->side[k]);
		}
	}
	put32(p, Num_robot_centers);
	p += 4;
	for (i = 0; i < Num_robot_centers; i++, p += MATCEN_SIZE) {
		put32(p, RobotCenters[i].robot_flags);
		put32(p + 4, RobotCenters[i].hit_points);
		put32(p + 8, RobotCenters[i].interval);
		ps16(p + 12, RobotCenters[i].segnum);
		ps16(p + 14, RobotCenters[i].fuelcen_num);
	}
	put32(p, Num_fuelcenters);
	p += 4;
	for (i = 0; i < Num_fuelcenters; i++, p += FUELCEN_SIZE) {
		FuelCenter *f = &Station[i];

		put32(p, f->Type);
		put32(p + 4, f->segnum);
		p[8] = f->Flag;
		p[9] = f->Enabled;
		p[10] = f->Lives;
		p[11] = f->dum1;
		put32(p + 12, f->Capacity);
		put32(p + 16, f->MaxCapacity);
		put32(p + 20, f->Timer);
		put32(p + 24, f->Disable_time);
		pvec(p + 28, &f->Center);
	}
	return n;
}
