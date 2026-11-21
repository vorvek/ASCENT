/*
 * ASCENT integration demo: a rotating texture mapped cube.
 *
 * This program links the real fix, vecmat, 2d, 3d, texmap, bios and platform
 * libraries together, with no substitutes. It is not part of the game.
 *
 * Usage: cube PALETTE.256 [frames] [out.ppm]
 * Without a frame count, the program runs until Escape or window close.
 * With out.ppm, the program writes the last frame to that file.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "palette.h"
#include "3d.h"
#include "texmap.h"
#include "key.h"
#include "timer.h"
#include "platform.h"

// The game defines these in main/render.c. The texture mapper sets them.
int Window_clip_left, Window_clip_top, Window_clip_right, Window_clip_bot;

static ubyte Texture_data[64 * 64];
static grs_bitmap Texture;

static vms_vector Cube_verts[8] = {
	{-f1_0, -f1_0, -f1_0}, {f1_0, -f1_0, -f1_0}, {f1_0, f1_0, -f1_0}, {-f1_0, f1_0, -f1_0},
	{-f1_0, -f1_0, f1_0}, {f1_0, -f1_0, f1_0}, {f1_0, f1_0, f1_0}, {-f1_0, f1_0, f1_0},
};

// Vertex order is clockwise when a person looks at the face from outside.
static int Cube_faces[6][4] = {
	{0, 3, 2, 1}, {5, 6, 7, 4}, {4, 7, 3, 0}, {1, 2, 6, 5}, {3, 7, 6, 2}, {4, 0, 1, 5},
};

static void make_texture(void)
{
	int x, y;
	int light = gr_find_closest_color(50, 50, 40);
	int dark = gr_find_closest_color(25, 12, 6);
	int mark = gr_find_closest_color(63, 10, 10);

	for (y = 0; y < 64; y++)
		for (x = 0; x < 64; x++) {
			int c = (((x >> 3) ^ (y >> 3)) & 1) ? light : dark;
			if (x < 16 && y < 16)
				c = mark;
			Texture_data[y * 64 + x] = (ubyte) c;
		}
	memset(&Texture, 0, sizeof(Texture));
	Texture.bm_w = Texture.bm_h = 64;
	Texture.bm_rowsize = 64;
	Texture.bm_type = BM_LINEAR;
	Texture.bm_data = Texture_data;
}

static void draw_cube(fixang angle)
{
	static g3s_uvl uvls[4] = {{0, 0, 0}, {f1_0, 0, 0}, {f1_0, f1_0, 0}, {0, f1_0, 0}};
	vms_angvec ang;
	vms_matrix obj_orient, view_orient = IDENTITY_MATRIX;
	vms_vector view_pos = {0, 0, -5 * f1_0};
	vms_vector world[8];
	g3s_point points[8];
	int f, i;

	ang.p = angle;
	ang.b = angle / 2;
	ang.h = angle * 2;
	vm_angles_2_matrix(&obj_orient, &ang);

	g3_start_frame();
	g3_set_view_matrix(&view_pos, &view_orient, f1_0);
	for (i = 0; i < 8; i++) {
		vm_vec_rotate(&world[i], &Cube_verts[i], &obj_orient);
		g3_rotate_point(&points[i], &world[i]);
	}
	for (f = 0; f < 6; f++) {
		g3s_point *list[4];
		vms_vector e1, e2, normal;

		vm_vec_sub(&e1, &world[Cube_faces[f][1]], &world[Cube_faces[f][0]]);
		vm_vec_sub(&e2, &world[Cube_faces[f][2]], &world[Cube_faces[f][1]]);
		vm_vec_crossprod(&normal, &e1, &e2);
		if (!g3_check_normal_facing(&world[Cube_faces[f][0]], &normal))
			continue;
		for (i = 0; i < 4; i++) {
			list[i] = &points[Cube_faces[f][i]];
			uvls[i].l = f1_0 - f * (f1_0 / 10);
		}
		g3_draw_tmap(4, list, uvls, &Texture);
	}
	g3_end_frame();
}

int main(int argc, char **argv)
{
	FILE *f;
	int frames = -1;
	int running = 1;
	int drawn = 0;
	fix start;

	if (argc < 2) {
		fprintf(stderr, "usage: cube PALETTE.256 [frames] [out.ppm]\n");
		return 2;
	}
	if (argc > 2)
		frames = atoi(argv[2]);

	f = fopen(argv[1], "rb");
	if (!f || fread(gr_palette, 1, 768, f) != 768 ||
			fread(gr_fade_table, 1, 256 * GR_FADE_LEVELS, f) != 256 * GR_FADE_LEVELS) {
		fprintf(stderr, "cannot read the palette and the fade table from %s\n", argv[1]);
		return 1;
	}
	fclose(f);

	if (plat_init("ASCENT cube demo", 320, 200, 3) != 0) {
		fprintf(stderr, "plat_init failed\n");
		return 1;
	}
	timer_init();
	key_init();
	if (gr_init(SM_320x200C) != 0) {
		fprintf(stderr, "gr_init failed\n");
		return 1;
	}
	gr_palette_load(gr_palette);
	g3_init();
	make_texture();

	Lighting_on = 1;
	Interpolation_method = 0;

	start = timer_get_fixed_seconds();
	while (running && frames != 0) {
		fix t = timer_get_fixed_seconds() - start;

		gr_set_current_canvas(NULL);
		gr_clear_canvas(0);
		// With a frame limit the angle depends on the frame number,
		// so that the output file is the same on each run.
		draw_cube((fixang) (frames > 0 ? drawn * 700 : t / 4));
		gr_present();
		drawn++;

		if (!plat_pump() || key_inkey() == KEY_ESC)
			running = 0;
		if (frames > 0)
			frames--;
	}

	if (argc > 3) {
		grs_bitmap *bm = &grd_curscreen->sc_canvas.cv_bitmap;
		int x, y;

		f = fopen(argv[3], "wb");
		if (f) {
			fprintf(f, "P6\n%d %d\n255\n", bm->bm_w, bm->bm_h);
			for (y = 0; y < bm->bm_h; y++)
				for (x = 0; x < bm->bm_w; x++) {
					ubyte *c = &gr_palette[bm->bm_data[y * bm->bm_rowsize + x] * 3];
					fputc(c[0] * 4, f);
					fputc(c[1] * 4, f);
					fputc(c[2] * 4, f);
				}
			fclose(f);
		}
	}

	printf("%d frames in %.2f seconds\n", drawn, f2fl(timer_get_fixed_seconds() - start));
	gr_close();
	plat_shutdown();
	return 0;
}
