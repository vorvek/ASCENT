/*
 * ASCENT: new file. It replaces the fread() and cfread() calls that read a
 * whole C structure from a game data file. The structures of the 1995 code
 * had 1 byte packing (the Watcom default of that time) and 32-bit pointers,
 * so the layout on disk is not the layout in memory now.
 *
 * The functions here read and write one field at a time, little endian,
 * from a byte buffer or from a CFILE. The record functions (walls, triggers,
 * segments and so on) convert one record on disk to the structure in memory.
 * They are "static inline" so that a test can use them without the game.
 *
 * The sizes on disk are named constants: <NAME>_DISK_SIZE.
 */

#ifndef _DATAIO_H
#define _DATAIO_H

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "cfile.h"
#include "error.h"

//------------------------------------------------------------------------
// Fields from a byte buffer. Each function moves the buffer pointer.

static inline ubyte dio_get_ubyte(const ubyte **pp)
{
	ubyte b = (*pp)[0];
	*pp += 1;
	return b;
}

static inline byte dio_get_byte(const ubyte **pp)
{
	return (byte)dio_get_ubyte(pp);
}

static inline ushort dio_get_ushort(const ubyte **pp)
{
	const ubyte *p = *pp;
	*pp += 2;
	return (ushort)(p[0] | (p[1] << 8));
}

static inline short dio_get_short(const ubyte **pp)
{
	return (short)dio_get_ushort(pp);
}

static inline int dio_get_int(const ubyte **pp)
{
	const ubyte *p = *pp;
	*pp += 4;
	return (int)((unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24));
}

static inline fix dio_get_fix(const ubyte **pp)
{
	return (fix)dio_get_int(pp);
}

static inline fixang dio_get_fixang(const ubyte **pp)
{
	return (fixang)dio_get_ushort(pp);
}

static inline void dio_get_bytes(const ubyte **pp, void *dest, int n)
{
	memcpy(dest, *pp, n);
	*pp += n;
}

static inline void dio_get_vector(const ubyte **pp, vms_vector *v)
{
	v->x = dio_get_fix(pp);
	v->y = dio_get_fix(pp);
	v->z = dio_get_fix(pp);
}

static inline void dio_get_angvec(const ubyte **pp, vms_angvec *v)
{
	v->p = dio_get_fixang(pp);
	v->b = dio_get_fixang(pp);
	v->h = dio_get_fixang(pp);
}

static inline void dio_get_matrix(const ubyte **pp, vms_matrix *m)
{
	dio_get_vector(pp, &m->rvec);
	dio_get_vector(pp, &m->uvec);
	dio_get_vector(pp, &m->fvec);
}

//------------------------------------------------------------------------
// Fields to a byte buffer.

static inline void dio_put_ubyte(ubyte **pp, int b)
{
	(*pp)[0] = (ubyte)b;
	*pp += 1;
}

static inline void dio_put_short(ubyte **pp, int s)
{
	(*pp)[0] = (ubyte)(s & 0xff);
	(*pp)[1] = (ubyte)((s >> 8) & 0xff);
	*pp += 2;
}

static inline void dio_put_int(ubyte **pp, int i)
{
	unsigned int u = (unsigned int)i;
	(*pp)[0] = (ubyte)(u & 0xff);
	(*pp)[1] = (ubyte)((u >> 8) & 0xff);
	(*pp)[2] = (ubyte)((u >> 16) & 0xff);
	(*pp)[3] = (ubyte)((u >> 24) & 0xff);
	*pp += 4;
}

static inline void dio_put_vector(ubyte **pp, vms_vector *v)
{
	dio_put_int(pp, v->x);
	dio_put_int(pp, v->y);
	dio_put_int(pp, v->z);
}

//------------------------------------------------------------------------
// Fields from a CFILE. A short read gives zero bytes for the missing part,
// like a cfread() into a cleared variable. The callers that must detect the
// end of the file use cfread() and the buffer functions.

static inline ubyte cf_get_ubyte(CFILE *fp)
{
	ubyte b[1] = {0};
	cfread(b, 1, 1, fp);
	return b[0];
}

static inline byte cf_get_byte(CFILE *fp)
{
	return (byte)cf_get_ubyte(fp);
}

static inline ushort cf_get_ushort(CFILE *fp)
{
	ubyte b[2] = {0, 0};
	const ubyte *p = b;
	cfread(b, 1, 2, fp);
	return dio_get_ushort(&p);
}

static inline short cf_get_short(CFILE *fp)
{
	return (short)cf_get_ushort(fp);
}

static inline int cf_get_int(CFILE *fp)
{
	ubyte b[4] = {0, 0, 0, 0};
	const ubyte *p = b;
	cfread(b, 1, 4, fp);
	return dio_get_int(&p);
}

static inline fix cf_get_fix(CFILE *fp)
{
	return (fix)cf_get_int(fp);
}

static inline fixang cf_get_fixang(CFILE *fp)
{
	return (fixang)cf_get_ushort(fp);
}

static inline void cf_get_vector(CFILE *fp, vms_vector *v)
{
	v->x = cf_get_fix(fp);
	v->y = cf_get_fix(fp);
	v->z = cf_get_fix(fp);
}

static inline void cf_get_angvec(CFILE *fp, vms_angvec *v)
{
	v->p = cf_get_fixang(fp);
	v->b = cf_get_fixang(fp);
	v->h = cf_get_fixang(fp);
}

// Reads n ints into an array.
static inline void cf_get_ints(CFILE *fp, int *dest, int n)
{
	int i;
	for (i = 0; i < n; i++)
		dest[i] = cf_get_int(fp);
}

// Reads n shorts into an array.
static inline void cf_get_shorts(CFILE *fp, short *dest, int n)
{
	int i;
	for (i = 0; i < n; i++)
		dest[i] = cf_get_short(fp);
}

// Reads n vectors into an array.
static inline void cf_get_vectors(CFILE *fp, vms_vector *dest, int n)
{
	int i;
	for (i = 0; i < n; i++)
		cf_get_vector(fp, &dest[i]);
}

#endif	// _DATAIO_H

//------------------------------------------------------------------------
// Level records. Include segment.h, wall.h, switch.h and fuelcen.h before
// this file to get the record functions of their structures. Each section
// has its own guard, so a later include of this file adds the sections of
// the headers that came in between.

#if defined(_SEGMENT_H) && !defined(_DATAIO_SEGMENT_RECORDS)
#define _DATAIO_SEGMENT_RECORDS

#define VERTEX_DISK_SIZE			12		// vms_vector
#define UVL_DISK_SIZE				12
#define SIDE_DISK_SIZE				80		// side, with the normals
#define SEGMENT_EDITOR_DISK_SIZE	522	// segment of the editor build (mine versions 16 and 17)
#define SEGMENT_GAME_DISK_SIZE	520	// segment of the game build (no segnum, no group, with pad)

// One side record (80 bytes): type, pad, wall_num, tmap_num, tmap_num2,
// uvls[4], normals[2].
static inline void side_from_disk(side *s, const ubyte **pp)
{
	int i;

	s->type = dio_get_byte(pp);
	s->pad = dio_get_ubyte(pp);
	s->wall_num = dio_get_short(pp);
	s->tmap_num = dio_get_short(pp);
	s->tmap_num2 = dio_get_short(pp);
	for (i = 0; i < 4; i++) {
		s->uvls[i].u = dio_get_fix(pp);
		s->uvls[i].v = dio_get_fix(pp);
		s->uvls[i].l = dio_get_fix(pp);
	}
#ifdef COMPACT_SEGS
	*pp += 24;
#else
	dio_get_vector(pp, &s->normals[0]);
	dio_get_vector(pp, &s->normals[1]);
#endif
}

// One segment record of the editor mine format (522 bytes): segnum,
// sides[6], children[6], verts[8], group, objects, special, matcen_num,
// value, static_light. The game structure has no segnum and no group. The
// function returns them in *segnum and *group when the pointers are not
// NULL. The editor structure has them, so they are stored there too.
static inline void segment_from_disk_editor(segment *seg, const ubyte *buf, short *segnum, short *group)
{
	const ubyte *p = buf;
	short t;
	int i;

	t = dio_get_short(&p);
	if (segnum) *segnum = t;
	#ifdef EDITOR
	seg->segnum = t;
	#endif
	for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++)
		side_from_disk(&seg->sides[i], &p);
	for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++)
		seg->children[i] = dio_get_short(&p);
	for (i = 0; i < MAX_VERTICES_PER_SEGMENT; i++)
		seg->verts[i] = dio_get_short(&p);
	t = dio_get_short(&p);
	if (group) *group = t;
	#ifdef EDITOR
	seg->group = t;
	#endif
	seg->objects = dio_get_short(&p);
	seg->special = dio_get_ubyte(&p);
	seg->matcen_num = dio_get_byte(&p);
	seg->value = dio_get_short(&p);
	seg->static_light = dio_get_fix(&p);
	#ifndef EDITOR
	seg->pad = 0;
	#endif
}

#endif	// _SEGMENT_H

#if defined(_WALL_H) && !defined(_DATAIO_WALL_RECORDS)
#define _DATAIO_WALL_RECORDS

#define WALL_DISK_SIZE				24		// wall, game versions 20 and up
#define V19_WALL_DISK_SIZE			21		// game versions 17 to 19
#define V16_WALL_DISK_SIZE			9		// game versions below 17
#define ACTIVE_DOOR_DISK_SIZE		16		// active_door, game versions 20 and up
#define V19_DOOR_DISK_SIZE			20		// game versions below 20

// One wall record (24 bytes).
static inline void wall_from_disk(wall *w, const ubyte *buf)
{
	const ubyte *p = buf;

	w->segnum = dio_get_int(&p);
	w->sidenum = dio_get_int(&p);
	w->hps = dio_get_fix(&p);
	w->linked_wall = dio_get_int(&p);
	w->type = dio_get_ubyte(&p);
	w->flags = dio_get_ubyte(&p);
	w->state = dio_get_ubyte(&p);
	w->trigger = dio_get_byte(&p);
	w->clip_num = dio_get_byte(&p);
	w->keys = dio_get_ubyte(&p);
	w->pad = dio_get_short(&p);
}

static inline void wall_to_disk(wall *w, ubyte *buf)
{
	ubyte *p = buf;

	dio_put_int(&p, w->segnum);
	dio_put_int(&p, w->sidenum);
	dio_put_int(&p, w->hps);
	dio_put_int(&p, w->linked_wall);
	dio_put_ubyte(&p, w->type);
	dio_put_ubyte(&p, w->flags);
	dio_put_ubyte(&p, w->state);
	dio_put_ubyte(&p, w->trigger);
	dio_put_ubyte(&p, w->clip_num);
	dio_put_ubyte(&p, w->keys);
	dio_put_short(&p, w->pad);
}

// One active door record (16 bytes).
static inline void active_door_from_disk(active_door *d, const ubyte *buf)
{
	const ubyte *p = buf;

	d->n_parts = dio_get_int(&p);
	d->front_wallnum[0] = dio_get_short(&p);
	d->front_wallnum[1] = dio_get_short(&p);
	d->back_wallnum[0] = dio_get_short(&p);
	d->back_wallnum[1] = dio_get_short(&p);
	d->time = dio_get_fix(&p);
}

#endif	// _WALL_H

#if defined(_SWITCH_H) && !defined(_DATAIO_SWITCH_RECORDS)
#define _DATAIO_SWITCH_RECORDS

#define TRIGGER_DISK_SIZE			54		// 1+2+4+4+1+2+20+20

// One trigger record (54 bytes).
static inline void trigger_from_disk(trigger *t, const ubyte *buf)
{
	const ubyte *p = buf;
	int j;

	t->type = dio_get_byte(&p);
	t->flags = dio_get_short(&p);
	t->value = dio_get_fix(&p);
	t->time = dio_get_fix(&p);
	t->link_num = dio_get_byte(&p);
	t->num_links = dio_get_short(&p);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		t->seg[j] = dio_get_short(&p);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		t->side[j] = dio_get_short(&p);
}

#endif	// _SWITCH_H

#if defined(_FUELCEN_H) && !defined(_DATAIO_FUELCEN_RECORDS)
#define _DATAIO_FUELCEN_RECORDS

#define CONTROL_CENTER_TRIGGERS_DISK_SIZE	42		// 2+20+20
#define MATCEN_INFO_DISK_SIZE					16

// The control center triggers record (42 bytes).
static inline void control_center_triggers_from_disk(control_center_triggers *t, const ubyte *buf)
{
	const ubyte *p = buf;
	int j;

	t->num_links = dio_get_short(&p);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		t->seg[j] = dio_get_short(&p);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++)
		t->side[j] = dio_get_short(&p);
}

// One robot maker record (16 bytes).
static inline void matcen_info_from_disk(matcen_info *m, const ubyte *buf)
{
	const ubyte *p = buf;

	m->robot_flags = dio_get_int(&p);
	m->hit_points = dio_get_fix(&p);
	m->interval = dio_get_fix(&p);
	m->segnum = dio_get_short(&p);
	m->fuelcen_num = dio_get_short(&p);
}

#endif	// _FUELCEN_H
