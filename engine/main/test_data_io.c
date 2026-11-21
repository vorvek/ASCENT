/*
 * ASCENT: new file. Test of the record functions of dataio.h against byte
 * buffers that are made by hand. The test needs only the headers of the game
 * code, not the game itself.
 */

#include <stdio.h>
#include <string.h>

#include "segment.h"
#include "wall.h"
#include "switch.h"
#include "fuelcen.h"
#include "dataio.h"

static int errors = 0;

#define CHECK(expr) do { if (!(expr)) { printf("FAILED line %d: %s\n", __LINE__, #expr); errors++; } } while (0)

// Appends fields to a buffer.
static ubyte *wp;
static void w8(int b) { dio_put_ubyte(&wp, b); }
static void w16(int s) { dio_put_short(&wp, s); }
static void w32(int i) { dio_put_int(&wp, i); }

static void test_fields(void)
{
	static const ubyte src[] = { 0x80, 0xfe, 0xff, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12, 0xff, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x00, 0x80 };
	const ubyte *p = src;
	ubyte out[8];
	ubyte *q = out;

	CHECK(dio_get_byte(&p) == -128);
	CHECK(dio_get_short(&p) == -2);
	CHECK(dio_get_ushort(&p) == 0x1234);
	CHECK(dio_get_int(&p) == 0x12345678);
	CHECK(dio_get_fix(&p) == 0x7fffffff);
	CHECK(dio_get_int(&p) == (int)0x80000000);
	CHECK(p - src == sizeof(src));

	dio_put_short(&q, -2);
	dio_put_int(&q, 0x12345678);
	dio_put_ubyte(&q, 0x80);
	CHECK(q - out == 7);
	CHECK(memcmp(out, "\xfe\xff\x78\x56\x34\x12\x80", 7) == 0);
}

static void test_wall(void)
{
	ubyte buf[WALL_DISK_SIZE], buf2[WALL_DISK_SIZE];
	wall w;

	wp = buf;
	w32(0x123);			// segnum
	w32(5);				// sidenum
	w32(0x00640000);	// hps
	w32(-1);				// linked_wall
	w8(2);				// type
	w8(0x81);			// flags
	w8(3);				// state
	w8(0xff);			// trigger
	w8(0xfe);			// clip_num
	w8(6);				// keys
	w16(-3);				// pad
	CHECK(wp - buf == WALL_DISK_SIZE);

	memset(&w, 0x55, sizeof(w));
	wall_from_disk(&w, buf);
	CHECK(w.segnum == 0x123);
	CHECK(w.sidenum == 5);
	CHECK(w.hps == 0x00640000);
	CHECK(w.linked_wall == -1);
	CHECK(w.type == 2);
	CHECK(w.flags == 0x81);
	CHECK(w.state == 3);
	CHECK(w.trigger == -1);
	CHECK(w.clip_num == -2);
	CHECK(w.keys == 6);
	CHECK(w.pad == -3);

	wall_to_disk(&w, buf2);
	CHECK(memcmp(buf, buf2, WALL_DISK_SIZE) == 0);
}

static void test_active_door(void)
{
	ubyte buf[ACTIVE_DOOR_DISK_SIZE];
	active_door d;

	wp = buf;
	w32(2);
	w16(10); w16(11);
	w16(-1); w16(13);
	w32(0x18000);
	CHECK(wp - buf == ACTIVE_DOOR_DISK_SIZE);

	active_door_from_disk(&d, buf);
	CHECK(d.n_parts == 2);
	CHECK(d.front_wallnum[0] == 10 && d.front_wallnum[1] == 11);
	CHECK(d.back_wallnum[0] == -1 && d.back_wallnum[1] == 13);
	CHECK(d.time == 0x18000);
}

static void test_trigger(void)
{
	ubyte buf[TRIGGER_DISK_SIZE];
	trigger t;
	int j;

	wp = buf;
	w8(0xff);			// type
	w16(0x0102);		// flags
	w32(0x50000);		// value
	w32(-1);				// time
	w8(7);				// link_num
	w16(3);				// num_links
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) w16(100 + j);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) w16(j - 2);
	CHECK(wp - buf == TRIGGER_DISK_SIZE);

	trigger_from_disk(&t, buf);
	CHECK(t.type == -1);
	CHECK(t.flags == 0x0102);
	CHECK(t.value == 0x50000);
	CHECK(t.time == -1);
	CHECK(t.link_num == 7);
	CHECK(t.num_links == 3);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) {
		CHECK(t.seg[j] == 100 + j);
		CHECK(t.side[j] == j - 2);
	}
}

static void test_fuelcen(void)
{
	ubyte buf[CONTROL_CENTER_TRIGGERS_DISK_SIZE];
	ubyte mbuf[MATCEN_INFO_DISK_SIZE];
	control_center_triggers c;
	matcen_info m;
	int j;

	wp = buf;
	w16(4);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) w16(200 + j);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) w16(5 - j);
	CHECK(wp - buf == CONTROL_CENTER_TRIGGERS_DISK_SIZE);

	control_center_triggers_from_disk(&c, buf);
	CHECK(c.num_links == 4);
	for (j = 0; j < MAX_WALLS_PER_LINK; j++) {
		CHECK(c.seg[j] == 200 + j);
		CHECK(c.side[j] == 5 - j);
	}

	wp = mbuf;
	w32(0x80000001);
	w32(0x1f40000);
	w32(0x50000);
	w16(321);
	w16(-1);
	CHECK(wp - mbuf == MATCEN_INFO_DISK_SIZE);

	matcen_info_from_disk(&m, mbuf);
	CHECK(m.robot_flags == (int)0x80000001);
	CHECK(m.hit_points == 0x1f40000);
	CHECK(m.interval == 0x50000);
	CHECK(m.segnum == 321);
	CHECK(m.fuelcen_num == -1);
}

static void test_segment(void)
{
	ubyte buf[SEGMENT_EDITOR_DISK_SIZE];
	segment s;
	short segnum, group;
	int i, j;

	wp = buf;
	w16(77);										// segnum
	for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++) {
		ubyte *side_start = wp;
		w8(1 + (i % 3));						// type
		w8(i);									// pad
		w16(i == 2 ? -1 : 40 + i);			// wall_num
		w16(300 + i);							// tmap_num
		w16(0x4000 | i);						// tmap_num2
		for (j = 0; j < 4; j++) {
			w32(0x10000 * j + i);			// u
			w32(-0x10000 * j - i);			// v
			w32(0x8000 + j);					// l
		}
		for (j = 0; j < 2; j++) {
			w32(0x10000 + i);
			w32(j);
			w32(-i);
		}
		CHECK(wp - side_start == SIDE_DISK_SIZE);
	}
	for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++) w16(i - 2);			// children
	for (i = 0; i < MAX_VERTICES_PER_SEGMENT; i++) w16(1000 + i);	// verts
	w16(-1);										// group
	w16(55);										// objects
	w8(0x84);									// special
	w8(0xff);									// matcen_num
	w16(-7);										// value
	w32(0x12345);								// static_light
	CHECK(wp - buf == SEGMENT_EDITOR_DISK_SIZE);

	memset(&s, 0x55, sizeof(s));
	segnum = group = 0;
	segment_from_disk_editor(&s, buf, &segnum, &group);
	CHECK(segnum == 77);
	CHECK(group == -1);
	for (i = 0; i < MAX_SIDES_PER_SEGMENT; i++) {
		CHECK(s.sides[i].type == 1 + (i % 3));
		CHECK(s.sides[i].pad == i);
		CHECK(s.sides[i].wall_num == (i == 2 ? -1 : 40 + i));
		CHECK(s.sides[i].tmap_num == 300 + i);
		CHECK(s.sides[i].tmap_num2 == (0x4000 | i));
		for (j = 0; j < 4; j++) {
			CHECK(s.sides[i].uvls[j].u == 0x10000 * j + i);
			CHECK(s.sides[i].uvls[j].v == -0x10000 * j - i);
			CHECK(s.sides[i].uvls[j].l == 0x8000 + j);
		}
#ifndef COMPACT_SEGS
		for (j = 0; j < 2; j++) {
			CHECK(s.sides[i].normals[j].x == 0x10000 + i);
			CHECK(s.sides[i].normals[j].y == j);
			CHECK(s.sides[i].normals[j].z == -i);
		}
#endif
		CHECK(s.children[i] == i - 2);
	}
	for (i = 0; i < MAX_VERTICES_PER_SEGMENT; i++)
		CHECK(s.verts[i] == 1000 + i);
	CHECK(s.objects == 55);
	CHECK(s.special == 0x84);
	CHECK(s.matcen_num == -1);
	CHECK(s.value == -7);
	CHECK(s.static_light == 0x12345);
}

int main(void)
{
	test_fields();
	test_wall();
	test_active_door();
	test_trigger();
	test_fuelcen();
	test_segment();

	if (errors) {
		printf("test_data_io: %d errors\n", errors);
		return 1;
	}
	printf("test_data_io: all tests passed\n");
	return 0;
}
