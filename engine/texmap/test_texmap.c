/*
 * ASCENT: test program for the texture mapper library (d_texmap).
 * It replaces the DOS test program textest of the original makefile.
 *
 * The program draws into a canvas in memory. It needs no window.
 * It supplies small local versions of the symbols that the texture mapper
 * takes from the other libraries (2d, main, misc), thus it links only the
 * texture mapper sources.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fix.h"
#include "gr.h"
#include "3d.h"
#include "texmap.h"
#include "texmapl.h"

// ---------------------------------------------------------------------------
// Local versions of symbols from other libraries.

grs_canvas	*grd_curcanv;
ubyte	gr_fade_table[256*GR_FADE_LEVELS];
ubyte	gr_palette[256*3];		// tmapfx.c tmap_fx_solid_bitmap
int	Gr_scanline_darkening_level = GR_FADE_LEVELS;
int	Window_clip_left, Window_clip_top, Window_clip_right, Window_clip_bot;

void gr_setcolor(int color)
{
	grd_curcanv->cv_color = color;
}

grs_bitmap *rle_expand_texture(grs_bitmap *bmp)
{
	return bmp;
}

// ASCENT: Assert and Int3 come from the misc library.

// ---------------------------------------------------------------------------
// Canvas with guard bands.

#define ROWSIZE	320		// bytes between rows
#define CW	300		// canvas width. The 20 bytes after each row are guard bytes.
#define CH	200		// canvas height
#define GUARD_ROWS	16		// guard rows above and below the canvas
#define GUARD	0xEE

static ubyte	screen_mem[(CH + 2*GUARD_ROWS) * ROWSIZE + 16];
static ubyte	*screen;		// first pixel of the canvas. Its address is a multiple of 4.
static grs_canvas	canvas;

static int	failures;
static int	quiet;		// count failures and do not print them
static const char	*test_name = "";

static void fail(const char *fmt, int a, int b, int c, int d)
{
	failures++;
	if (failures <= 40 && !quiet) {
		printf("FAIL [%s] ", test_name);
		printf(fmt, a, b, c, d);
		printf("\n");
	}
}

#define PIX(x,y)	screen[(y)*ROWSIZE + (x)]

static void setup_canvas(void)
{
	memset(screen_mem, GUARD, sizeof(screen_mem));
	// The perspective renderer does a divide for each pixel until the write
	// address is a multiple of 4. The exact pixel tests need a known alignment.
	screen = screen_mem + GUARD_ROWS*ROWSIZE;
	while ((uintptr_t) screen & 3)
		screen++;

	memset(&canvas, 0, sizeof(canvas));
	canvas.cv_bitmap.bm_w = CW;
	canvas.cv_bitmap.bm_h = CH;
	canvas.cv_bitmap.bm_rowsize = ROWSIZE;
	canvas.cv_bitmap.bm_data = screen;
	grd_curcanv = &canvas;

	init_interface_vars_to_assembler();		// also sets the Window_clip variables
}

//	Background pattern. Its colors (200..207) are not in the textures.
static ubyte background(int x, int y)
{
	return 200 + ((x + 3*y) & 7);
}

static void clear_canvas(void)
{
	int	x, y;

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++)
			PIX(x,y) = background(x,y);
}

//	Checks that no byte outside the canvas has changed.
static void check_guards(void)
{
	int	x, y, bad = 0;
	size_t	i;
	size_t	first = screen - screen_mem;
	size_t	last = first + (size_t) CH*ROWSIZE;

	for (i = 0; i < first; i++)
		if (screen_mem[i] != GUARD) bad++;
	for (i = last; i < sizeof(screen_mem); i++)
		if (screen_mem[i] != GUARD) bad++;
	for (y = 0; y < CH; y++)
		for (x = CW; x < ROWSIZE; x++)
			if (PIX(x,y) != GUARD) bad++;

	if (bad) {
		fail("%d bytes written outside the canvas", bad, 0, 0, 0);
		setup_canvas();
	}
}

// ---------------------------------------------------------------------------
// Textures and fade tables.

static ubyte	tex_checker[64*64];		// checker, cells of 8x8, marked corner at u,v < 16
static ubyte	tex_holes[64*64];		// same, the light cells are transparent (255)
static ubyte	tex_u[64*64];			// color = 1 + u
static ubyte	tex_v[64*64];			// color = 1 + v
static grs_bitmap	bm_checker, bm_holes, bm_u, bm_v;

#define MARK_BASE	129		// colors of the marked corner are MARK_BASE..MARK_BASE+63

static void init_bitmap(grs_bitmap *bm, ubyte *data, int flags)
{
	memset(bm, 0, sizeof(*bm));
	bm->bm_w = bm->bm_h = bm->bm_rowsize = 64;
	bm->bm_flags = flags;
	bm->bm_data = data;
	bm->avg_color = 77;
}

static void make_textures(void)
{
	int	u, v;

	for (v = 0; v < 64; v++)
		for (u = 0; u < 64; u++) {
			int	parity = ((u >> 3) ^ (v >> 3)) & 1;
			int	c = 1 + ((u + 5*v) & 63) + parity*64;	// 1..64 dark cell, 65..128 light cell

			if (u < 16 && v < 16)
				c = MARK_BASE + ((u + 5*v) & 63);

			tex_checker[v*64+u] = c;
			tex_holes[v*64+u] = (parity && !(u < 16 && v < 16)) ? 255 : c;
			tex_u[v*64+u] = 1 + u;
			tex_v[v*64+u] = 1 + v;
		}

	init_bitmap(&bm_checker, tex_checker, 0);
	init_bitmap(&bm_holes, tex_holes, BM_FLAG_TRANSPARENT);
	init_bitmap(&bm_u, tex_u, 0);
	init_bitmap(&bm_v, tex_v, 0);
}

//	identity != 0: each row maps a color to itself.
//	identity == 0: row l maps color c to c+l. Color 255 stays 255.
static void make_fade_table(int identity)
{
	int	l, c;

	for (l = 0; l < GR_FADE_LEVELS; l++)
		for (c = 0; c < 256; c++)
			gr_fade_table[l*256+c] = (identity || c == 255) ? c : (ubyte) (c + l);
}

// ---------------------------------------------------------------------------
// Points.

static g3s_point	pts[8];
static g3s_point	*ptlist[8];

static void set_point(int i, double sx, double sy, double z, double u, double v, double l)
{
	memset(&pts[i], 0, sizeof(pts[i]));
	pts[i].p3_sx = (fix) floor(sx * 65536.0 + 0.5);
	pts[i].p3_sy = (fix) floor(sy * 65536.0 + 0.5);
	pts[i].z = (fix) floor(z * 65536.0 + 0.5);
	pts[i].p3_u = (fix) floor(u * 65536.0 + 0.5);
	pts[i].p3_v = (fix) floor(v * 65536.0 + 0.5);
	pts[i].p3_l = (fix) floor(l * 65536.0 + 0.5);
	ptlist[i] = &pts[i];
}

//	Screen aligned square of 65x65 pixels. One texture pixel for each screen pixel.
#define SQ_X	10
#define SQ_Y	10
#define SQ_N	64

static void set_square(double light)
{
	set_point(0, SQ_X,      SQ_Y,      12.0, 0.0, 0.0, light);
	set_point(1, SQ_X+SQ_N, SQ_Y,      12.0, 1.0, 0.0, light);
	set_point(2, SQ_X+SQ_N, SQ_Y+SQ_N, 12.0, 1.0, 1.0, light);
	set_point(3, SQ_X,      SQ_Y+SQ_N, 12.0, 0.0, 1.0, light);
}

static void set_modes(int method, int lighting)
{
	Interpolation_method = method;
	Lighting_on = lighting;
	Current_seg_depth = 0;
	Max_perspective_depth = 1;
	Max_linear_depth = 3;
	Max_flat_depth = 100;
	Gr_scanline_darkening_level = GR_FADE_LEVELS;
	tmap_flat_cthru_table = NULL;
}

// ---------------------------------------------------------------------------
// Exact pixels of the screen aligned square.
//	xlast = last column that the renderer draws (SQ_X+SQ_N, or one less)
//	light_row = row of the fade table, or -1 for no lighting
static void check_square(ubyte *tex, int xlast, int light_row)
{
	int	x, y;

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++) {
			int	expect = background(x,y);

			if (x >= SQ_X && x <= xlast && y >= SQ_Y && y <= SQ_Y+SQ_N) {
				int	c = tex[((y-SQ_Y) & 63)*64 + ((x-SQ_X) & 63)];

				if (c != 255 || !Transparency_on) {
					if (light_row >= 0)
						c = gr_fade_table[light_row*256 + c];
					expect = c;
				}
			}
			if (PIX(x,y) != expect)
				fail("pixel %d,%d is %d, expected %d", x, y, PIX(x,y), expect);
		}
	check_guards();
}

static void test_square(const char *name, grs_bitmap *bm, int method, int lighting, int xlast)
{
	test_name = name;
	set_modes(method, lighting);
	set_square(0.5);		// lighting value 0.5 selects row 16 of the fade table
	clear_canvas();
	draw_tmap(bm, 4, ptlist);
	Transparency_on = bm->bm_flags & BM_FLAG_TRANSPARENT;
	check_square(bm->bm_data, xlast, lighting ? 16 : -1);
}

static void test_exact_pixels(void)
{
	int	xl = SQ_X + SQ_N;

	make_fade_table(0);

	test_square("linear", &bm_checker, 1, 0, xl);
	test_square("linear lighted", &bm_checker, 1, 1, xl);
	test_square("perspective", &bm_checker, 2, 0, xl);
	test_square("perspective lighted", &bm_checker, 2, 1, xl);
	test_square("perspective divide/pixel", &bm_checker, 3, 0, xl);
	test_square("perspective divide/pixel lighted", &bm_checker, 3, 1, xl);

	// Transparency. The linear renderer with no lighting draws one pixel
	// less on the right when the texture is transparent (as the assembly).
	test_square("linear transparent", &bm_holes, 1, 0, xl - 1);
	test_square("linear lighted transparent", &bm_holes, 1, 1, xl);
	test_square("perspective transparent", &bm_holes, 2, 0, xl);
	test_square("perspective lighted transparent", &bm_holes, 2, 1, xl);
	test_square("perspective divide/pixel transparent", &bm_holes, 3, 0, xl);
	test_square("perspective divide/pixel lighted transparent", &bm_holes, 3, 1, xl);

	// Interpolation_method 0 selects the renderer from the segment depth.
	test_name = "method 0, near";
	set_modes(0, 0);
	set_square(0.5);
	clear_canvas();
	draw_tmap(&bm_checker, 4, ptlist);
	check_square(tex_checker, xl, -1);

	test_name = "method 0, middle";
	set_modes(0, 1);
	Current_seg_depth = 2;
	clear_canvas();
	draw_tmap(&bm_checker, 4, ptlist);
	check_square(tex_checker, xl, 16);
}

// ---------------------------------------------------------------------------
// Flat shading.

static void check_flat(int xlast, const ubyte *table, int constant)
{
	int	x, y;

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++) {
			int	expect = background(x,y);

			if (x >= SQ_X && x <= xlast && y >= SQ_Y && y <= SQ_Y+SQ_N)
				expect = table ? table[expect] : constant;
			if (PIX(x,y) != expect)
				fail("pixel %d,%d is %d, expected %d", x, y, PIX(x,y), expect);
		}
	check_guards();
}

static void test_flat(void)
{
	static ubyte	cthru[256];
	int	i, color;

	make_fade_table(0);
	color = gr_fade_table[16*256 + bm_checker.avg_color];

	test_name = "draw_tmap_flat";
	set_modes(1, 1);
	set_square(0.5);
	clear_canvas();
	draw_tmap_flat(&bm_checker, 4, ptlist);
	check_flat(SQ_X+SQ_N, NULL, color);

	test_name = "draw_tmap, far segment is flat";
	set_modes(0, 1);
	Current_seg_depth = 5;
	clear_canvas();
	draw_tmap(&bm_checker, 4, ptlist);
	check_flat(SQ_X+SQ_N, NULL, color);

	test_name = "flat, darken through fade table";
	set_modes(1, 1);
	Gr_scanline_darkening_level = 5;
	clear_canvas();
	draw_tmap_flat(&bm_checker, 4, ptlist);
	check_flat(SQ_X+SQ_N, gr_fade_table + 5*256, 0);

	// The cthru loop draws one pixel less on the right (as the assembly).
	test_name = "flat, cthru table";
	set_modes(1, 1);
	for (i = 0; i < 256; i++)
		cthru[i] = 255 - i;
	tmap_flat_cthru_table = cthru;
	clear_canvas();
	draw_tmap_flat(&bm_checker, 4, ptlist);
	check_flat(SQ_X+SQ_N-1, cthru, 0);
	tmap_flat_cthru_table = NULL;

	// A texture that is smaller than 64x64 is drawn flat.
	{
		grs_bitmap	small = bm_checker;

		test_name = "small bitmap is flat";
		small.bm_h = 32;
		set_modes(2, 1);
		clear_canvas();
		draw_tmap(&small, 4, ptlist);
		check_flat(SQ_X+SQ_N, NULL, color);
	}
}

// ---------------------------------------------------------------------------
// A scene in perspective: a floor, a wall on the right and a wall at the back.
// The eye is at the origin and looks along +z.
//	sx = CX + FOCAL*X/Z,  sy = CY - FOCAL*Y/Z

#define CX	150.0
#define CY	100.0
#define FOCAL	160.0
#define ZNEAR	2.5
#define ZFAR	6.0
#define HALF_W	2.0
#define HALF_H	1.5

static void set_point_3d(int i, double X, double Y, double Z, double u, double v, double l)
{
	set_point(i, CX + FOCAL*X/Z, CY - FOCAL*Y/Z, Z, u, v, l);
}

//	u goes from left (0) to right (1), v goes from far (0) to near (1).
static void set_floor(void)
{
	set_point_3d(0, -HALF_W, -HALF_H, ZFAR,  0.0, 0.0, 0.2);
	set_point_3d(1,  HALF_W, -HALF_H, ZFAR,  1.0, 0.0, 0.2);
	set_point_3d(2,  HALF_W, -HALF_H, ZNEAR, 1.0, 1.0, 0.9);
	set_point_3d(3, -HALF_W, -HALF_H, ZNEAR, 0.0, 1.0, 0.9);
}

//	u goes from far (0) to near (1), v goes from top (0) to bottom (1).
static void set_right_wall(void)
{
	set_point_3d(0, HALF_W,  HALF_H, ZFAR,  0.0, 0.0, 0.2);
	set_point_3d(1, HALF_W,  HALF_H, ZNEAR, 1.0, 0.0, 0.9);
	set_point_3d(2, HALF_W, -HALF_H, ZNEAR, 1.0, 1.0, 0.9);
	set_point_3d(3, HALF_W, -HALF_H, ZFAR,  0.0, 1.0, 0.2);
}

static void set_back_wall(void)
{
	set_point_3d(0, -HALF_W,  HALF_H, ZFAR, 0.0, 0.0, 0.2);
	set_point_3d(1,  HALF_W,  HALF_H, ZFAR, 1.0, 0.0, 0.2);
	set_point_3d(2,  HALF_W, -HALF_H, ZFAR, 1.0, 1.0, 0.2);
	set_point_3d(3, -HALF_W, -HALF_H, ZFAR, 0.0, 1.0, 0.2);
}

//	Distance between two texture coordinates on the 64 pixel circle.
static double wrap_dist(double a, double b)
{
	double	d = fmod(fabs(a - b), 64.0);

	return (d > 32.0) ? 64.0 - d : d;
}

#define TOLERANCE	3.0		// texture pixels

//	which = 0: floor, 1: right wall. Texture coordinates of screen pixel x,y.
static void expected_uv(int which, int x, int y, double *u, double *v)
{
	double	X, Y, Z;

	if (which == 0) {
		Z = HALF_H * FOCAL / (y - CY);
		X = (x - CX) * Z / FOCAL;
		*u = (X + HALF_W) / (2*HALF_W) * 64.0;
		*v = (ZFAR - Z) / (ZFAR - ZNEAR) * 64.0;
	} else {
		Z = HALF_W * FOCAL / (x - CX);
		Y = (CY - y) * Z / FOCAL;
		*u = (ZFAR - Z) / (ZFAR - ZNEAR) * 64.0;
		*v = (HALF_H - Y) / (2*HALF_H) * 64.0;
	}
}

//	Draws the polygon with the u texture and with the v texture and compares
//	each pixel with the exact perspective mapping.
static void check_perspective_mapping(int which, int method, int lighting)
{
	static ubyte	got_u[CH][CW];
	int	x, y, n = 0, bad = 0;
	double	worst = 0;

	set_modes(method, lighting);

	if (which == 0) set_floor(); else set_right_wall();
	clear_canvas();
	draw_tmap(&bm_u, 4, ptlist);
	check_guards();
	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++)
			got_u[y][x] = PIX(x,y);

	if (which == 0) set_floor(); else set_right_wall();
	clear_canvas();
	draw_tmap(&bm_v, 4, ptlist);
	check_guards();

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++) {
			double	eu, ev, du, dv;
			int	cu = got_u[y][x], cv = PIX(x,y);

			if ((cu >= 200) != (cv >= 200)) {
				fail("pixel %d,%d is drawn with one texture only", x, y, 0, 0);
				continue;
			}
			if (cu >= 200)
				continue;		// background

			n++;
			expected_uv(which, x, y, &eu, &ev);
			du = wrap_dist(cu - 1, eu);
			dv = wrap_dist(cv - 1, ev);
			if (du > worst) worst = du;
			if (dv > worst) worst = dv;
			if (du > TOLERANCE || dv > TOLERANCE) {
				bad++;
				if (bad <= 5)
					fail("pixel %d,%d has u,v %d,%d, far from the exact mapping", x, y, cu-1, cv-1);
			}
		}

	if (n < 3000)
		fail("only %d pixels drawn", n, 0, 0, 0);
	printf("  %-12s method %d lighting %d: %d pixels, largest error %.2f texture pixels%s\n",
		which ? "right wall" : "floor", method, lighting, n, worst,
		quiet ? " (linear mapper, must be large)" : "");
}

//	The marked corner of the checker texture is at the vertex with u,v = 0,0.
static void check_orientation(int method)
{
	int	x, y;

	set_modes(method, 0);
	set_floor();
	clear_canvas();
	draw_tmap(&bm_checker, 4, ptlist);
	check_guards();

	// Texture coordinates 8,8 / 56,8 / 56,56 / 8,56 on the floor.
	{
		static const double	uv[4][2] = { {8,8}, {56,8}, {56,56}, {8,56} };
		int	i;

		for (i = 0; i < 4; i++) {
			double	Z = ZFAR - (ZFAR - ZNEAR) * uv[i][1] / 64.0;
			double	X = -HALF_W + 2*HALF_W * uv[i][0] / 64.0;
			int	marked;

			x = (int) (CX + FOCAL*X/Z);
			y = (int) (CY + FOCAL*HALF_H/Z);
			marked = PIX(x,y) >= MARK_BASE && PIX(x,y) < MARK_BASE+64;
			if (marked != (i == 0))
				fail("corner %d: pixel %d,%d is %d", i, x, y, PIX(x,y));
		}
	}
}

//	Coverage: mask[y][x] = 1 where the canvas differs from the background.
static int get_coverage(ubyte mask[CH][CW])
{
	int	x, y, n = 0;

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++) {
			mask[y][x] = (PIX(x,y) != background(x,y));
			n += mask[y][x];
		}
	return n;
}

//	Two polygons that share an edge must cover each pixel of the polygon
//	that they form together.
static void check_no_gaps(int method, int lighting, int split)
{
	static ubyte	whole[CH][CW], parts[CH][CW];
	g3s_point	save[4];
	int	x, y, nwhole, nparts, extra = 0;

	set_modes(method, lighting);
	set_floor();
	memcpy(save, pts, sizeof(save));

	clear_canvas();
	draw_tmap(&bm_checker, 4, ptlist);
	nwhole = get_coverage(whole);

	clear_canvas();
	if (split == 0) {
		// two triangles that share the diagonal 0-2
		pts[0] = save[0]; pts[1] = save[1]; pts[2] = save[2];
		draw_tmap(&bm_checker, 3, ptlist);
		pts[0] = save[0]; pts[1] = save[2]; pts[2] = save[3];
		draw_tmap(&bm_checker, 3, ptlist);
	} else {
		// two quads that share a slanted edge from the far side to the near side
		set_floor();
		set_point_3d(1,  0.5, -HALF_H, ZFAR,  0.625, 0.0, 0.2);
		set_point_3d(2, -0.5, -HALF_H, ZNEAR, 0.375, 1.0, 0.9);
		draw_tmap(&bm_checker, 4, ptlist);
		set_floor();
		set_point_3d(0,  0.5, -HALF_H, ZFAR,  0.625, 0.0, 0.2);
		set_point_3d(3, -0.5, -HALF_H, ZNEAR, 0.375, 1.0, 0.9);
		draw_tmap(&bm_checker, 4, ptlist);
	}
	nparts = get_coverage(parts);
	check_guards();

	for (y = 0; y < CH; y++) {
		int	first = -1, last = -1, count = 0;

		for (x = 0; x < CW; x++) {
			if (whole[y][x] && !parts[y][x])
				fail("gap at pixel %d,%d (split %d)", x, y, split, 0);
			if (parts[y][x] && !whole[y][x])
				extra++;
			if (parts[y][x]) {
				if (first < 0) first = x;
				last = x;
				count++;
			}
		}
		if (count && count != last - first + 1)
			fail("row %d has a hole (split %d)", y, split, 0, 0);
	}
	if (extra)
		fail("%d pixels outside the whole polygon (split %d)", extra, split, 0, 0);
	if (nwhole < 3000 || nparts < 3000)
		fail("only %d, %d pixels drawn", nwhole, nparts, 0, 0);
}

static void test_perspective(void)
{
	int	lighting, method;

	make_fade_table(1);

	for (method = 2; method <= 3; method++)
		for (lighting = 0; lighting <= 1; lighting++) {
			test_name = "perspective mapping";
			check_perspective_mapping(0, method, lighting);
			check_perspective_mapping(1, method, lighting);
			test_name = "no gaps";
			check_no_gaps(method, lighting, 0);
			check_no_gaps(method, lighting, 1);
		}

	test_name = "no gaps, linear";
	check_no_gaps(1, 0, 0);
	check_no_gaps(1, 1, 1);

	test_name = "orientation";
	check_orientation(1);
	check_orientation(2);
	check_orientation(3);

	// The linear mapper must NOT pass the perspective check on the floor.
	// This shows that the check can tell the two mappers apart.
	{
		int	before = failures, found;

		test_name = "linear is not perspective";
		quiet = 1;
		check_perspective_mapping(0, 1, 0);
		quiet = 0;
		found = failures - before;
		failures = before;
		test_name = "check of the check";
		if (found == 0)
			fail("the perspective check accepts the linear mapper", 0, 0, 0, 0);
	}
}

// ---------------------------------------------------------------------------
// Polygons and spans outside the canvas must not write outside the canvas.

static void test_bad_input(void)
{
	static const double	xy[3][4][2] = {
		{ {-50,-30}, {400,-20}, {380,300}, {-40,260} },		// larger than the canvas
		{ {250,150}, {390,160}, {380,290}, {240,280} },		// over the bottom right corner
		{ {-90,-70}, {40,-60}, {50,30}, {-80,40} },			// over the top left corner
	};
	static ubyte	cthru[256];
	int	poly, method, lighting, i, mode;

	make_fade_table(0);

	test_name = "polygon outside the canvas";
	for (poly = 0; poly < 3; poly++)
		for (method = 1; method <= 3; method++)
			for (lighting = 0; lighting <= 2; lighting++)
				for (mode = 0; mode < 4; mode++) {
					grs_bitmap	*bm = (mode & 1) ? &bm_holes : &bm_checker;

					set_modes(method, lighting);
					for (i = 0; i < 4; i++)
						set_point(i, xy[poly][i][0], xy[poly][i][1], 3.0 + 2*i, i == 1 || i == 2, i >= 2, 0.1 + 0.3*i);
					clear_canvas();
					if (mode < 2)
						draw_tmap(bm, 4, ptlist);
					else {
						if (mode == 3) Gr_scanline_darkening_level = 7;
						draw_tmap_flat(bm, 4, ptlist);
					}
					check_guards();
				}
	set_modes(1, 0);

	// Direct calls of the scanline renderers with spans outside the canvas.
	test_name = "span outside the canvas";
	for (i = 0; i < 256; i++)
		cthru[i] = i ^ 1;
	{
		static const int	ys[] = { -1000, -1, 0, 1, CH-1, CH, CH+5, 100000 };
		static const int	xs[] = { -100000, -400, -1, 0, 1, CW-1, CW, CW+1, ROWSIZE+50, 100000 };
		int	iy, il, ir, t, f;

		for (t = 0; t <= 1; t++)
		for (f = 0; f <= 1; f++)
		for (iy = 0; iy < 8; iy++)
			for (il = 0; il < 10; il++)
				for (ir = 0; ir < 10; ir++) {
					Transparency_on = t;
					Lighting_on = f;
					per2_flag = f;
					pixptr = t ? tex_holes : tex_checker;
					fx_y = ys[iy];
					fx_xleft = xs[il];
					fx_xright = xs[ir];
					fx_u = 0x123456; fx_v = 0x654321;
					fx_z = (ir == 3) ? 0 : 0x8000;		// z = 0 was a divide exception
					fx_du_dx = 0x9000; fx_dv_dx = -0x7000;
					fx_dz_dx = t ? -0x100 : -0x133;		// z reaches 0, or goes below 0
					fx_l = (iy & 1) ? 40*F1_0 : -3*F1_0;		// outside the fade table
					fx_dl_dx = (il & 1) ? 0x30000 : -0x30000;
					tmap_flat_color = 9;
					tmap_flat_shade_value = 3;
					tmap_flat_cthru_table = f ? cthru : NULL;

					asm_tmap_scanline_lin();
					asm_tmap_scanline_lin_lighted();
					asm_tmap_scanline_per();
					Lighting_on = !f;
					asm_tmap_scanline_per();
					asm_tmap_scanline_flat();
					asm_tmap_scanline_shaded();
					asm_tmap_scanline_matt();
				}
		tmap_flat_cthru_table = NULL;
		check_guards();
	}
}

// ---------------------------------------------------------------------------
// PNG file with stored (not compressed) deflate blocks.

static uint32_t	crc_table[256];

static uint32_t crc32_update(uint32_t crc, const ubyte *p, size_t n)
{
	while (n--)
		crc = crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	return crc;
}

static void put32(ubyte *p, uint32_t v)
{
	p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v;
}

static void write_chunk(FILE *f, const char *type, const ubyte *data, size_t n)
{
	ubyte	b[4];
	uint32_t	crc;

	put32(b, (uint32_t) n);
	fwrite(b, 1, 4, f);
	fwrite(type, 1, 4, f);
	if (n) fwrite(data, 1, n, f);
	crc = crc32_update(0xffffffffu, (const ubyte *) type, 4);
	crc = crc32_update(crc, data, n);
	put32(b, ~crc);
	fwrite(b, 1, 4, f);
}

//	rgb = w*h*3 bytes
static int write_png(const char *filename, const ubyte *rgb, int w, int h)
{
	FILE	*f;
	ubyte	hdr[13], *raw, *z, *zp;
	size_t	rawsize = (size_t) h * (w*3 + 1), pos;
	uint32_t	a = 1, b = 0;
	int	i, k, y;

	for (i = 0; i < 256; i++) {
		uint32_t	c = i;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
		crc_table[i] = c;
	}

	raw = malloc(rawsize);
	z = malloc(rawsize + rawsize/65535*5 + 16);
	if (!raw || !z) return 0;

	for (y = 0; y < h; y++) {
		raw[(size_t) y * (w*3+1)] = 0;		// filter type none
		memcpy(raw + (size_t) y * (w*3+1) + 1, rgb + (size_t) y*w*3, w*3);
	}

	zp = z;
	*zp++ = 0x78; *zp++ = 0x01;
	for (pos = 0; pos < rawsize; ) {
		size_t	n = rawsize - pos;
		if (n > 65535) n = 65535;
		*zp++ = (pos + n == rawsize);
		*zp++ = n & 0xff; *zp++ = n >> 8;
		*zp++ = ~n & 0xff; *zp++ = (~n >> 8) & 0xff;
		memcpy(zp, raw + pos, n);
		zp += n;
		pos += n;
	}
	for (pos = 0; pos < rawsize; pos++) {
		a = (a + raw[pos]) % 65521;
		b = (b + a) % 65521;
	}
	put32(zp, (b << 16) | a);
	zp += 4;

	f = fopen(filename, "wb");
	if (!f) return 0;
	fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
	put32(hdr, w); put32(hdr+4, h);
	hdr[8] = 8; hdr[9] = 2; hdr[10] = 0; hdr[11] = 0; hdr[12] = 0;
	write_chunk(f, "IHDR", hdr, 13);
	write_chunk(f, "IDAT", z, zp - z);
	write_chunk(f, "IEND", NULL, 0);
	fclose(f);
	free(raw);
	free(z);
	return 1;
}

//	Colors of the test picture. Fade row l darkens color c in the picture:
//	the fade table for the picture maps (l,c) to a color with 3 bits of
//	brightness and 5 bits of texture class.
static void picture_rgb(int c, ubyte *rgb)
{
	int	bright = 3 + (c >> 5);		// 3..10
	int	cls = c & 31;
	int	r, g, b;

	switch (cls) {
		case 1:	r = 40;  g = 70;  b = 150; break;		// dark cell
		case 2:	r = 230; g = 225; b = 190; break;		// light cell
		case 3:	r = 230; g = 50;  b = 40;  break;		// marked corner
		case 4:	r = 60;  g = 190; b = 80;  break;		// flat polygon
		default:	r = 25; g = 25; b = 30; bright = 10; break;	// background
	}
	rgb[0] = r * bright / 10;
	rgb[1] = g * bright / 10;
	rgb[2] = b * bright / 10;
}

#define SCALE	3

static void test_picture(void)
{
	static ubyte	tex[64*64];
	static ubyte	rgb[CH*SCALE * CW*SCALE * 3];
	grs_bitmap	bm;
	char	filename[1024];
	int	u, v, l, c, x, y;

	test_name = "picture";

	// texture colors: class 1, 2, 3 with brightness field 0
	for (v = 0; v < 64; v++)
		for (u = 0; u < 64; u++) {
			int	parity = ((u >> 3) ^ (v >> 3)) & 1;
			tex[v*64+u] = (u < 16 && v < 16) ? 3 : 1 + parity;
		}
	init_bitmap(&bm, tex, 0);
	bm.avg_color = 4;

	// fade table: 32 lighting values to 8 brightness values
	for (l = 0; l < GR_FADE_LEVELS; l++)
		for (c = 0; c < 256; c++)
			gr_fade_table[l*256+c] = (c & 31) | (((l > 31 ? 31 : l) >> 2) << 5);

	for (y = 0; y < CH; y++)
		for (x = 0; x < CW; x++)
			PIX(x,y) = 0;

	set_modes(2, 1);
	set_back_wall();
	draw_tmap(&bm, 4, ptlist);
	set_floor();
	draw_tmap(&bm, 4, ptlist);
	set_right_wall();
	draw_tmap(&bm, 4, ptlist);

	// a flat polygon on the left
	set_point(0, 20, 30, 4.0, 0, 0, 0.9);
	set_point(1, 80, 50, 4.0, 0, 0, 0.9);
	set_point(2, 40, 120, 4.0, 0, 0, 0.9);
	draw_tmap_flat(&bm, 3, ptlist);
	check_guards();

	for (y = 0; y < CH*SCALE; y++)
		for (x = 0; x < CW*SCALE; x++)
			picture_rgb(PIX(x/SCALE, y/SCALE), &rgb[(y*CW*SCALE + x)*3]);

#ifdef TEST_OUTPUT_DIR
	snprintf(filename, sizeof(filename), "%s/test_texmap_frame.png", TEST_OUTPUT_DIR);
#else
	snprintf(filename, sizeof(filename), "test_texmap_frame.png");
#endif
	if (!write_png(filename, rgb, CW*SCALE, CH*SCALE))
		fail("cannot write the picture", 0, 0, 0, 0);
	else
		printf("  picture: %s\n", filename);
}

// ---------------------------------------------------------------------------

int main(void)
{
	setup_canvas();
	make_textures();

	printf("test_texmap\n");

	test_exact_pixels();
	test_flat();
	test_perspective();
	test_bad_input();
	test_picture();

	if (failures) {
		printf("%d failures\n", failures);
		return 1;
	}
	printf("all tests passed\n");
	return 0;
}
