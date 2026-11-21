/*
 * ASCENT: test program for the 2d library (d_2d).
 *
 * The program draws into canvases in memory and checks exact pixels.
 * It does not open a window: it has its own platform functions, which
 * count the calls and have a clock that the test controls.
 *
 * Usage: test_2d [data directory] [output picture (.ppm)]
 *
 * The data directory has the font files (*.FNT) and PALETTE.256 of the
 * game. Without an argument the program reads the environment variable
 * ASCENT_DATA_DIR. If the files are absent, the data tests are skipped.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "types.h"
#include "mem.h"
#include "error.h"
#include "cfile.h"
#include "fix.h"
#include "gr.h"
#include "grdef.h"
#include "palette.h"
#include "rle.h"
#include "pcx.h"
#include "ibitblt.h"
#include "platform.h"

// Functions of the library that have no declaration in a header.
void scale_bitmap(grs_bitmap *bp, grs_point *vertbuf );
void gr_bm_bitblt(int w, int h, int dx, int dy, int sx, int sy, grs_bitmap * src, grs_bitmap * dest);
void gr_linear_line( int x0, int y0, int x1, int y1 );
void gr_update_buffer( void * sbuf1, void * sbuf2, void * dbuf, int size );
ubyte * gr_rle_decode_asm( ubyte * src, ubyte * dest );
void get_char_width(int c,int c2,int *width,int *spacing);

//---------------------------------------------------------------------
// Test support

static int Num_checks = 0;
static int Num_failed = 0;

#define CHECK(cond) do { \
		Num_checks++; \
		if (!(cond)) { \
			Num_failed++; \
			if (Num_failed <= 40) \
				printf( "FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond ); \
		} \
	} while (0)

//---------------------------------------------------------------------
// Platform functions for the test. No window. The clock moves only
// when the library sleeps.

static uint64_t Fake_time_us = 1000;
static int Num_presents = 0;
static int Num_pumps = 0;
static const uint8_t * Present_pixels;
static int Present_w, Present_h, Present_pitch;
static uint8_t Present_palette[768];
static int Present_cost_us = 0;		// the time that one plat_present takes

int plat_init(const char *title, int w, int h, int scale) { return 0; }
void plat_shutdown(void) { }
int plat_set_frame_size(int w, int h) { return 0; }

void plat_present(const uint8_t *pixels, int w, int h, int pitch, const uint8_t *pal6)
{
	Num_presents++;
	Present_pixels = pixels;
	Present_w = w;
	Present_h = h;
	Present_pitch = pitch;
	memcpy( Present_palette, pal6, 768 );
	Fake_time_us += Present_cost_us;
}

int plat_pump(void) { Num_pumps++; return 1; }
uint64_t plat_time_us(void) { return Fake_time_us; }
void plat_sleep_ms(int ms) { Fake_time_us += (uint64_t)ms * 1000; }

//---------------------------------------------------------------------
// A canvas in memory with guard bytes around the pixels, and a row size
// that is larger than the width.

#define TW		64
#define TH		48
#define TGUARD	16
#define TROW	(TW + 2*TGUARD)
#define TGUARD_BYTE 0xEE

static ubyte Test_memory[TROW * (TH + 2*TGUARD)];
static grs_canvas Test_canvas;

static void test_canvas_init(int color)
{
	int y;

	memset( Test_memory, TGUARD_BYTE, sizeof(Test_memory) );
	gr_init_canvas( &Test_canvas, Test_memory + TGUARD*TROW + TGUARD, BM_LINEAR, TW, TH );
	Test_canvas.cv_bitmap.bm_rowsize = TROW;
	for (y=0; y<TH; y++ )
		memset( Test_canvas.cv_bitmap.bm_data + y*TROW, color, TW );
	gr_set_current_canvas( &Test_canvas );
}

static int tpix(int x, int y)
{
	return Test_canvas.cv_bitmap.bm_data[y*TROW + x];
}

// Returns the number of pixels of the canvas with this color.
static int tcount(int color)
{
	int x, y, n = 0;
	for (y=0; y<TH; y++ )
		for (x=0; x<TW; x++ )
			if (tpix(x,y) == color) n++;
	return n;
}

// Returns 1 if no function wrote outside the canvas.
static int tguard_ok(void)
{
	int x, y;
	for (y=0; y<TH + 2*TGUARD; y++ )
		for (x=0; x<TROW; x++ )	{
			int inside = (y >= TGUARD) && (y < TGUARD+TH) && (x >= TGUARD) && (x < TGUARD+TW);
			if (!inside && Test_memory[y*TROW + x] != TGUARD_BYTE)
				return 0;
		}
	return 1;
}

//---------------------------------------------------------------------

static void test_modes(void)
{
	grs_canvas page;

	CHECK( gr_init( SM_320x200C ) == 0 );
	CHECK( gr_init( SM_320x200C ) == 1 );		// only one time
	CHECK( grd_curscreen != NULL );
	CHECK( grd_curscreen->sc_w == 320 && grd_curscreen->sc_h == 200 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_type == BM_LINEAR );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize == 320 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_data == gr_video_memory );
	CHECK( grd_curscreen->sc_aspect == fixdiv(320*3,200*4) );
	CHECK( grd_curcanv == &grd_curscreen->sc_canvas );
	CHECK( gr_refresh_hz == 70 );

	CHECK( gr_set_mode( SM_640x480V ) == 0 );
	CHECK( grd_curscreen->sc_w == 640 && grd_curscreen->sc_h == 480 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize == 640 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_type == BM_LINEAR );
	CHECK( gr_refresh_hz == 60 );

	CHECK( gr_set_mode( SM_800x600V ) == 0 );
	CHECK( grd_curscreen->sc_w == 800 && grd_curscreen->sc_h == 600 );
	CHECK( gr_set_mode( SM_1024x768V ) == 0 );
	CHECK( grd_curscreen->sc_w == 1024 && grd_curscreen->sc_h == 768 );
	CHECK( gr_set_mode( SM_640x400V ) == 0 );
	CHECK( grd_curscreen->sc_w == 640 && grd_curscreen->sc_h == 400 );

	// The Mode X modes are linear frames with a row size of the width.
	CHECK( gr_set_mode( SM_320x240U ) == 0 );
	CHECK( grd_curscreen->sc_w == 320 && grd_curscreen->sc_h == 240 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize == 320 );
	CHECK( grd_curscreen->sc_canvas.cv_bitmap.bm_type == BM_LINEAR );
	CHECK( gr_set_mode( SM_320x400U ) == 0 );
	CHECK( grd_curscreen->sc_w == 320 && grd_curscreen->sc_h == 400 );
	CHECK( gr_set_mode( SM_376x564U ) == 0 );
	CHECK( grd_curscreen->sc_w == 376 && grd_curscreen->sc_h == 564 );

	// The 15-bit modes are not supported.
	CHECK( gr_set_mode( SM_640x480V15 ) == 4 );
	CHECK( grd_curscreen->sc_w == 376 );		// the mode did not change
	CHECK( gr_check_mode( SM_800x600V ) == 0 );
	CHECK( gr_check_mode( SM_320x200C ) == 0 );
	CHECK( gr_check_mode( SM_800x600V15 ) == 4 );
	CHECK( gr_check_mode( 99 ) == 11 );

	// gr_present gives the frame to the platform.
	CHECK( gr_set_mode( SM_320x200C ) == 0 );
	Num_presents = 0;
	gr_present();
	CHECK( Num_presents == 1 );
	CHECK( Present_pixels == gr_video_memory );
	CHECK( Present_w == 320 && Present_h == 200 && Present_pitch == 320 );

	// A set mode clears the frame.
	gr_setcolor( 9 );
	gr_rect( 0, 0, 319, 199 );
	CHECK( gr_video_memory[0] == 9 && gr_video_memory[320*200-1] == 9 );
	CHECK( gr_set_mode( SM_320x200C ) == 0 );
	CHECK( gr_video_memory[0] == 0 && gr_video_memory[320*200-1] == 0 );

	// Page flip: a sub canvas below the visible page, as the game makes
	// in the Mode X modes. gr_show_canvas shows that page.
	gr_init_sub_canvas( &page, &grd_curscreen->sc_canvas, 0, 200, 320, 200 );
	CHECK( page.cv_bitmap.bm_data == gr_video_memory + 200*320 );
	gr_set_current_canvas( &page );
	gr_setcolor( 7 );
	gr_rect( 0, 0, 319, 199 );
	CHECK( gr_video_memory[200*320-1] == 0 );			// last pixel of page 0
	CHECK( gr_video_memory[200*320] == 7 );				// first pixel of page 1
	Num_presents = 0;
	gr_show_canvas( &page );
	CHECK( Num_presents == 1 );
	CHECK( Present_pixels == gr_video_memory + 200*320 );
	gr_show_canvas( &grd_curscreen->sc_canvas );
	CHECK( Num_presents == 2 );
	CHECK( Present_pixels == gr_video_memory );
	gr_set_current_canvas( NULL );
}

static void test_pixels_and_rects(void)
{
	int x, y, n;

	test_canvas_init( 1 );
	CHECK( tcount(1) == TW*TH );

	gr_setcolor( 5 );
	gr_pixel( 3, 4 );
	CHECK( tpix(3,4) == 5 );
	CHECK( tcount(5) == 1 );
	gr_pixel( -1, 0 ); gr_pixel( 0, -1 ); gr_pixel( TW, 0 ); gr_pixel( 0, TH );
	CHECK( tcount(5) == 1 );
	CHECK( gr_gpixel( &Test_canvas.cv_bitmap, 3, 4 ) == 5 );
	CHECK( gr_gpixel( &Test_canvas.cv_bitmap, -1, 4 ) == 0 );
	CHECK( gr_ugpixel( &Test_canvas.cv_bitmap, 2, 4 ) == 1 );
	gr_bm_pixel( &Test_canvas.cv_bitmap, 4, 4, 6 );
	CHECK( tpix(4,4) == 6 );

	// The SVGA and Mode X types use the linear code.
	Test_canvas.cv_bitmap.bm_type = BM_SVGA;
	gr_upixel( 5, 4 );
	CHECK( tpix(5,4) == 5 );
	CHECK( gr_ugpixel( &Test_canvas.cv_bitmap, 5, 4 ) == 5 );
	Test_canvas.cv_bitmap.bm_type = BM_MODEX;
	gr_upixel( 6, 4 );
	CHECK( tpix(6,4) == 5 );
	gr_uscanline( 7, 9, 4 );
	CHECK( tpix(7,4) == 5 && tpix(9,4) == 5 && tpix(10,4) == 1 );
	Test_canvas.cv_bitmap.bm_type = BM_LINEAR;

	// Filled rectangle
	test_canvas_init( 1 );
	gr_setcolor( 5 );
	gr_rect( 10, 10, 19, 14 );
	n = 0;
	for (y=10; y<=14; y++ )
		for (x=10; x<=19; x++ )
			if (tpix(x,y) == 5) n++;
	CHECK( n == 50 );
	CHECK( tcount(5) == 50 );

	// Clipped rectangle
	gr_setcolor( 6 );
	gr_rect( -5, 40, 3, 60 );
	CHECK( tcount(6) == 4*8 );
	CHECK( tpix(0,40) == 6 && tpix(3,47) == 6 && tpix(4,40) == 1 && tpix(0,39) == 1 );

	// Box (outline)
	gr_setcolor( 7 );
	gr_box( 30, 5, 39, 12 );
	CHECK( tcount(7) == 2*10 + 2*8 - 4 );
	CHECK( tpix(30,5) == 7 && tpix(39,5) == 7 && tpix(30,12) == 7 && tpix(39,12) == 7 );
	CHECK( tpix(35,5) == 7 && tpix(35,12) == 7 && tpix(30,8) == 7 && tpix(39,8) == 7 );
	CHECK( tpix(31,6) == 1 && tpix(38,11) == 1 );

	// Clipped box
	gr_setcolor( 8 );
	gr_box( 50, 30, 80, 60 );
	CHECK( tpix(50,30) == 8 && tpix(63,30) == 8 && tpix(50,47) == 8 && tpix(63,47) == 8 );
	CHECK( tpix(51,31) == 1 );

	// Scan line that darkens through the fade table
	{
		int i;
		for (i=0; i<256; i++ )
			gr_fade_table[3*256+i] = (ubyte)(i ^ 0x80);
		Gr_scanline_darkening_level = 3;
		gr_scanline( 20, 22, 20 );
		Gr_scanline_darkening_level = GR_FADE_LEVELS;
		CHECK( tpix(20,20) == 0x81 && tpix(22,20) == 0x81 && tpix(23,20) == 1 );
	}

	CHECK( tguard_ok() );
}

// Draws a line with gr_linear_line and checks the properties of a run
// length slice line: the two end points, one pixel for each step of
// the major axis, and the same pixels when the end points are swapped.
static void check_line_properties(int x0, int y0, int x1, int y1)
{
	static ubyte first[TW*TH];
	int dx = abs(x1-x0), dy = abs(y1-y0);
	int x, y, n, major_ok = 1;

	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(x0), i2f(y0), i2f(x1), i2f(y1) );
	CHECK( tpix(x0,y0) == 9 );
	CHECK( tpix(x1,y1) == 9 );
	CHECK( tcount(9) == (dx > dy ? dx : dy) + 1 );

	if (dx >= dy)	{
		for (x=(x0<x1?x0:x1); x<=(x0<x1?x1:x0); x++ )	{
			n = 0;
			for (y=0; y<TH; y++ )
				if (tpix(x,y) == 9) n++;
			if (n != 1) major_ok = 0;
		}
	} else {
		for (y=(y0<y1?y0:y1); y<=(y0<y1?y1:y0); y++ )	{
			n = 0;
			for (x=0; x<TW; x++ )
				if (tpix(x,y) == 9) n++;
			if (n != 1) major_ok = 0;
		}
	}
	CHECK( major_ok );
	CHECK( tguard_ok() );

	for (y=0; y<TH; y++ )
		for (x=0; x<TW; x++ )
			first[y*TW+x] = tpix(x,y);

	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(x1), i2f(y1), i2f(x0), i2f(y0) );
	n = 0;
	for (y=0; y<TH; y++ )
		for (x=0; x<TW; x++ )
			if (first[y*TW+x] != tpix(x,y)) n++;
	CHECK( n == 0 );
}

// Checks that row y has the color from x0 to x1 and no other pixel of it.
static int row_is(int y, int x0, int x1, int color)
{
	int x;
	for (x=0; x<TW; x++ )
		if ( (tpix(x,y) == color) != (x >= x0 && x <= x1) )
			return 0;
	return 1;
}

static int col_is(int x, int y0, int y1, int color)
{
	int y;
	for (y=0; y<TH; y++ )
		if ( (tpix(x,y) == color) != (y >= y0 && y <= y1) )
			return 0;
	return 1;
}

static void test_lines(void)
{
	int i, ret;

	// X-major line with runs of equal length
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(0), i2f(0), i2f(7), i2f(3) );
	CHECK( row_is(0,0,1,9) && row_is(1,2,3,9) && row_is(2,4,5,9) && row_is(3,6,7,9) );
	CHECK( tcount(9) == 8 );

	// X-major line with runs of 2, 3, 4 and 2 pixels
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(0), i2f(0), i2f(10), i2f(3) );
	CHECK( row_is(0,0,1,9) && row_is(1,2,4,9) && row_is(2,5,8,9) && row_is(3,9,10,9) );

	// The same line from right to left (mirror image)
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(10), i2f(0), i2f(0), i2f(3) );
	CHECK( row_is(0,9,10,9) && row_is(1,6,8,9) && row_is(2,2,5,9) && row_is(3,0,1,9) );

	// Y-major line (the transposed line)
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(0), i2f(0), i2f(3), i2f(10) );
	CHECK( col_is(0,0,1,9) && col_is(1,2,4,9) && col_is(2,5,8,9) && col_is(3,9,10,9) );

	// Horizontal, vertical and diagonal lines
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(12), i2f(20), i2f(3), i2f(20) );
	CHECK( row_is(20,3,12,9) && tcount(9) == 10 );
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(5), i2f(30), i2f(5), i2f(21) );
	CHECK( col_is(5,21,30,9) && tcount(9) == 10 );
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(10), i2f(10), i2f(4), i2f(16) );
	CHECK( tcount(9) == 7 );
	for (i=0; i<=6; i++ )
		CHECK( tpix(10-i,10+i) == 9 );

	// A single point
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	gr_uline( i2f(8), i2f(8), i2f(8), i2f(8) );
	CHECK( tcount(9) == 1 && tpix(8,8) == 9 );

	// Properties of lines in each direction
	{
		static const int ends[][4] = {
			{ 0,0,63,47 }, { 63,0,0,47 }, { 0,0,63,1 }, { 0,0,1,47 }, { 5,7,60,9 },
			{ 60,7,5,40 }, { 30,2,33,45 }, { 33,2,30,45 }, { 1,1,62,30 }, { 2,46,61,3 },
			{ 10,10,25,20 }, { 10,10,20,25 }, { 0,47,63,46 }, { 17,5,18,44 }, { 3,3,50,36 },
			{ 0,0,2,1 }, { 0,0,1,2 }, { 0,0,3,1 }, { 0,0,1,3 }, { 0,0,5,2 }, { 0,0,2,5 },
		};
		for (i=0; i<(int)(sizeof(ends)/sizeof(ends[0])); i++ )
			check_line_properties( ends[i][0], ends[i][1], ends[i][2], ends[i][3] );
	}

	// Clipped line
	test_canvas_init( 0 );
	gr_setcolor( 9 );
	ret = gr_line( i2f(-10), i2f(5), i2f(100), i2f(5) );
	CHECK( ret == 1 );
	CHECK( row_is(5,0,TW-1,9) && tcount(9) == TW );
	ret = gr_line( i2f(-10), i2f(-5), i2f(100), i2f(-5) );
	CHECK( ret == 2 );
	CHECK( tcount(9) == TW );
	ret = gr_line( i2f(-20), i2f(-20), i2f(200), i2f(200) );
	CHECK( ret == 1 );
	CHECK( tpix(0,0) == 9 && tpix(47,47) == 9 && tpix(20,20) == 9 );
	ret = gr_line( i2f(2), i2f(40), i2f(9), i2f(40) );
	CHECK( ret == 0 );
	CHECK( tguard_ok() );
}

static void test_circles(void)
{
	// The half width of each row of a disk of radius 5 (rows 0 to 5 from
	// the center), from the midpoint algorithm of disc.c.
	static const int half[6] = { 5, 5, 5, 4, 3, 2 };
	int dy, ok, n;
	int x, y;

	test_canvas_init( 0 );
	gr_setcolor( 3 );
	CHECK( gr_disk( i2f(32), i2f(24), i2f(5) ) == 0 );
	ok = 1;
	n = 0;
	for (dy=-5; dy<=5; dy++ )	{
		int h = half[abs(dy)];
		if (!row_is( 24+dy, 32-h, 32+h, 3 )) ok = 0;
		n += 2*h + 1;
	}
	CHECK( ok );
	CHECK( tcount(3) == n );

	// The circle: each pixel is on the edge of the disk.
	gr_setcolor( 4 );
	CHECK( gr_circle( i2f(32), i2f(24), i2f(5) ) == 0 );
	CHECK( tpix(37,24) == 4 && tpix(27,24) == 4 );
	CHECK( tpix(37,26) == 4 && tpix(27,22) == 4 );
	CHECK( tpix(34,29) == 4 && tpix(30,19) == 4 );
	CHECK( tpix(36,27) == 4 && tpix(35,28) == 4 );
	CHECK( tpix(32,29) == 3 );		// the algorithm does not draw this point
	CHECK( tcount(4) == 22 );
	CHECK( tcount(3) + tcount(4) == n );		// no circle pixel outside the disk
	for (y=0; y<TH; y++ )
		for (x=0; x<TW; x++ )
			if (tpix(x,y) == 4)	{
				// a pixel of the circle has a neighbour outside the disk
				CHECK( tpix(x-1,y) == 0 || tpix(x+1,y) == 0 || tpix(x,y-1) == 0 || tpix(x,y+1) == 0 );
			}

	// A disk that is partly outside the canvas
	test_canvas_init( 0 );
	gr_setcolor( 3 );
	gr_disk( i2f(0), i2f(0), i2f(5) );
	CHECK( row_is(0,0,5,3) && row_is(3,0,4,3) && row_is(5,0,2,3) && row_is(6,0,-1,3) );
	gr_circle( i2f(63), i2f(47), i2f(5) );
	gr_disk( i2f(-20), i2f(10), i2f(5) );		// outside
	CHECK( tguard_ok() );
}

static void test_polygons(void)
{
	int square[8] = { i2f(10), i2f(30), i2f(20), i2f(30), i2f(20), i2f(40), i2f(10), i2f(40) };
	int triangle[6] = { i2f(30), i2f(30), i2f(40), i2f(40), i2f(30), i2f(40) };
	int big[8] = { i2f(-30), i2f(-30), i2f(200), i2f(-30), i2f(200), i2f(200), i2f(-30), i2f(200) };
	int k, ok;

	test_canvas_init( 0 );
	gr_setcolor( 2 );
	gr_upoly( 4, square );
	ok = 1;
	for (k=30; k<=40; k++ )
		if (!row_is( k, 10, 20, 2 )) ok = 0;
	CHECK( ok );
	CHECK( tcount(2) == 11*11 );

	test_canvas_init( 0 );
	gr_setcolor( 2 );
	gr_upoly( 3, triangle );
	ok = 1;
	for (k=0; k<=9; k++ )
		if (!row_is( 30+k, 30, 30+k, 2 )) ok = 0;
	if (!row_is( 40, 30, 40, 2 )) ok = 0;
	CHECK( ok );
	CHECK( tcount(2) == 55 + 11 );

	// The clipped function gives the same pixels for a polygon inside
	test_canvas_init( 0 );
	gr_setcolor( 2 );
	gr_poly( 3, triangle );
	CHECK( tcount(2) == 55 + 11 );
	CHECK( row_is( 35, 30, 35, 2 ) );

	// and fills the canvas for a polygon around it
	test_canvas_init( 0 );
	gr_setcolor( 2 );
	gr_poly( 4, big );
	CHECK( tcount(2) == TW*TH );
	CHECK( tguard_ok() );
}

// Makes a bitmap of w by h pixels. Pixel x,y has the value base+y*w+x.
static grs_bitmap * make_bitmap(int w, int h, int base)
{
	grs_bitmap * bm = gr_create_bitmap( w, h );
	int i;
	for (i=0; i<w*h; i++ )
		bm->bm_data[i] = (ubyte)(base + i);
	return bm;
}

static void test_bitblt(void)
{
	grs_bitmap * bm = make_bitmap( 8, 6, 100 );
	int x, y, ok;

	// Whole bitmap
	test_canvas_init( 1 );
	gr_bitmap( 5, 7, bm );
	ok = 1;
	for (y=0; y<6; y++ )
		for (x=0; x<8; x++ )
			if (tpix(5+x,7+y) != 100 + y*8 + x) ok = 0;
	CHECK( ok );
	CHECK( tcount(1) == TW*TH - 48 );

	// Clipped at the top left and at the bottom right
	test_canvas_init( 1 );
	gr_bitmap( -3, -2, bm );
	ok = 1;
	for (y=0; y<4; y++ )
		for (x=0; x<5; x++ )
			if (tpix(x,y) != 100 + (y+2)*8 + (x+3)) ok = 0;
	CHECK( ok );
	CHECK( tcount(1) == TW*TH - 20 );
	gr_bitmap( TW-2, TH-3, bm );
	CHECK( tpix(TW-2,TH-3) == 100 && tpix(TW-1,TH-1) == 100 + 2*8 + 1 );
	gr_bitmap( TW, 0, bm );		// outside
	gr_bitmap( 0, -6, bm );		// outside
	CHECK( tguard_ok() );

	// Transparent pixels are not drawn by gr_bitmapm
	bm->bm_data[0] = 255;
	bm->bm_data[2*8+3] = 255;
	test_canvas_init( 1 );
	gr_bitmapm( 5, 7, bm );
	CHECK( tpix(5,7) == 1 && tpix(8,9) == 1 );
	CHECK( tpix(6,7) == 101 && tpix(12,12) == 100 + 5*8 + 7 );
	CHECK( tcount(1) == TW*TH - 46 );
	// but gr_bitmap draws them
	gr_bitmap( 5, 7, bm );
	CHECK( tpix(5,7) == 255 );

	// A part of a bitmap, into a bitmap with the SVGA type
	test_canvas_init( 1 );
	Test_canvas.cv_bitmap.bm_type = BM_SVGA;
	gr_bm_bitblt( 3, 2, 20, 20, 4, 3, bm, &Test_canvas.cv_bitmap );
	CHECK( tpix(20,20) == 100 + 3*8 + 4 && tpix(22,21) == 100 + 4*8 + 6 );
	CHECK( tcount(1) == TW*TH - 6 );
	gr_ubitmapm( 40, 20, bm );
	CHECK( tpix(40,20) == 1 && tpix(41,20) == 101 );
	Test_canvas.cv_bitmap.bm_type = BM_LINEAR;

	// Pixel doubling (the game uses it for the low detail modes)
	{
		extern int gr_bitblt_double;
		test_canvas_init( 1 );
		gr_bitblt_double = 1;
		gr_bm_ubitblt( 6, 1, 10, 30, 0, 1, bm, &Test_canvas.cv_bitmap );
		gr_bitblt_double = 0;
		CHECK( tpix(10,30) == 108 && tpix(11,30) == 108 && tpix(12,30) == 109 && tpix(15,30) == 110 && tpix(16,30) == 1 );
		gr_bitblt_double = 1;
		gr_bm_ubitblt( 5, 1, 10, 32, 0, 1, bm, &Test_canvas.cv_bitmap );
		gr_bitblt_double = 0;
		CHECK( tpix(10,32) == 108 && tpix(11,32) == 109 && tpix(12,32) == 109 && tpix(14,32) == 110 && tpix(15,32) == 1 );
	}
	CHECK( tguard_ok() );

	gr_free_bitmap( bm );
}

static void test_scale(void)
{
	// The column and row of the source for each pixel of the 9x9 result.
	// (run length slice: 2, 2, 3 and 2 pixels; rows from v = 0.375 * y)
	static const int src_col[9] = { 0, 0, 1, 1, 2, 2, 2, 3, 3 };
	static const int src_row[9] = { 0, 0, 0, 1, 1, 1, 2, 2, 3 };
	grs_bitmap * bm = make_bitmap( 4, 4, 100 );
	grs_point verts[3];
	int x, y, ok;

	// Enlarge 4x4 to the pixels 8..16
	test_canvas_init( 1 );
	verts[0].x = i2f(8); verts[0].y = i2f(8);
	verts[1].x = i2f(16); verts[1].y = i2f(8);
	verts[2].x = i2f(16); verts[2].y = i2f(16);
	scale_bitmap( bm, verts );
	ok = 1;
	for (y=0; y<9; y++ )
		for (x=0; x<9; x++ )
			if (tpix(8+x,8+y) != 100 + src_row[y]*4 + src_col[x]) ok = 0;
	CHECK( ok );
	CHECK( tcount(1) == TW*TH - 81 );

	// A transparent source pixel keeps the background
	bm->bm_data[1*4+2] = 255;
	test_canvas_init( 1 );
	scale_bitmap( bm, verts );
	ok = 1;
	for (y=0; y<9; y++ )
		for (x=0; x<9; x++ )	{
			int expect = 100 + src_row[y]*4 + src_col[x];
			if (src_row[y] == 1 && src_col[x] == 2) expect = 1;
			if (tpix(8+x,8+y) != expect) ok = 0;
		}
	CHECK( ok );
	bm->bm_data[1*4+2] = 106;

	// Make smaller: 4x4 to the pixels 20..22 (u = 0, 1.5, 3)
	test_canvas_init( 1 );
	verts[0].x = i2f(20); verts[0].y = i2f(20);
	verts[2].x = i2f(22); verts[2].y = i2f(22);
	scale_bitmap( bm, verts );
	CHECK( tpix(20,20) == 100 && tpix(21,20) == 101 && tpix(22,20) == 103 );
	CHECK( tpix(20,21) == 104 && tpix(21,21) == 105 && tpix(22,21) == 107 );
	CHECK( tpix(20,22) == 112 && tpix(21,22) == 113 && tpix(22,22) == 115 );
	CHECK( tcount(1) == TW*TH - 9 );

	// Clipped at the edges of the canvas
	test_canvas_init( 1 );
	verts[0].x = i2f(-8); verts[0].y = i2f(-8);
	verts[2].x = i2f(8); verts[2].y = i2f(8);
	scale_bitmap( bm, verts );
	CHECK( tpix(0,0) != 1 && tpix(8,8) == 115 );
	verts[0].x = i2f(TW-6); verts[0].y = i2f(TH-6);
	verts[2].x = i2f(TW+2); verts[2].y = i2f(TH+2);
	scale_bitmap( bm, verts );
	CHECK( tpix(TW-6,TH-6) == 100 && tpix(TW-1,TH-1) != 1 );
	// As in the original, a bitmap is not drawn when the clipping leaves
	// less than two source columns for a larger destination.
	test_canvas_init( 1 );
	verts[2].x = i2f(TW+10); verts[2].y = i2f(TH+10);
	scale_bitmap( bm, verts );
	CHECK( tcount(1) == TW*TH );
	CHECK( tguard_ok() );

	gr_free_bitmap( bm );
}

static void test_rle(void)
{
	static ubyte original[16*8];
	static ubyte row[64];
	grs_bitmap * bm = gr_create_bitmap( 16, 8 );
	grs_bitmap * tex = gr_create_bitmap( 64, 64 );
	grs_bitmap * expanded;
	static ubyte tex_original[64*64];
	grs_point verts[3];
	static ubyte scaled[TW*TH];
	int x, y, i, ok, offset;

	// Rows with runs, single pixels, transparent runs and pixels with
	// values that look like run codes (0xE0 and above).
	for (y=0; y<8; y++ )
		for (x=0; x<16; x++ )	{
			ubyte c;
			if (y == 0) c = 20;
			else if (y == 1) c = (ubyte)(30 + x/4);
			else if (y == 2) c = (x < 5 || x > 11) ? 255 : 40;
			else if (y == 3) c = (ubyte)(0xE0 + (x & 1));
			else if (y == 4) c = (x % 3 == 0) ? 255 : 50;
			else c = (ubyte)(60 + ((x/4 + y) % 3));
			original[y*16+x] = c;
		}
	memcpy( bm->bm_data, original, sizeof(original) );

	CHECK( gr_bitmap_rle_compress( bm ) == 1 );
	CHECK( bm->bm_flags & BM_FLAG_RLE );
	CHECK( bm->bm_data[4+0] == 3 );		// row 0: one run of 16 and the end code
	CHECK( bm->bm_data[4+8] == (0xE0|16) && bm->bm_data[4+8+1] == 20 && bm->bm_data[4+8+2] == 0xE0 );

	// Decode each row
	ok = 1;
	offset = 4 + 8;
	for (y=0; y<8; y++ )	{
		memset( row, 0xAA, sizeof(row) );
		CHECK( gr_rle_decode_asm( &bm->bm_data[offset], row ) == row + 16 );
		if (memcmp( row, &original[y*16], 16 ) != 0) ok = 0;
		if (row[16] != 0xAA) ok = 0;
		offset += bm->bm_data[4+y];
	}
	CHECK( ok );

	// Draw the RLE bitmap
	test_canvas_init( 1 );
	gr_bitmap( 10, 12, bm );
	ok = 1;
	for (y=0; y<8; y++ )
		for (x=0; x<16; x++ )
			if (tpix(10+x,12+y) != original[y*16+x]) ok = 0;
	CHECK( ok );
	CHECK( tguard_ok() );

	// Draw it with transparency
	test_canvas_init( 1 );
	gr_bitmapm( 10, 12, bm );
	ok = 1;
	for (y=0; y<8; y++ )
		for (x=0; x<16; x++ )	{
			int expect = original[y*16+x];
			if (expect == 255) expect = 1;
			if (tpix(10+x,12+y) != expect) ok = 0;
		}
	CHECK( ok );

	// Draw it clipped on each side
	test_canvas_init( 1 );
	gr_bitmap( -5, -3, bm );
	gr_bitmap( TW-7, TH-4, bm );
	ok = 1;
	for (y=0; y<5; y++ )
		for (x=0; x<11; x++ )
			if (tpix(x,y) != original[(y+3)*16+(x+5)]) ok = 0;
	for (y=0; y<4; y++ )
		for (x=0; x<7; x++ )
			if (tpix(TW-7+x,TH-4+y) != original[y*16+x]) ok = 0;
	CHECK( ok );
	CHECK( tguard_ok() );

	// Draw it on a canvas that has the SVGA type
	test_canvas_init( 1 );
	Test_canvas.cv_bitmap.bm_type = BM_SVGA;
	gr_ubitmap( 3, 3, bm );
	Test_canvas.cv_bitmap.bm_type = BM_LINEAR;
	ok = 1;
	for (y=0; y<8; y++ )
		for (x=0; x<16; x++ )
			if (tpix(3+x,3+y) != original[y*16+x]) ok = 0;
	CHECK( ok );

	// Scale the RLE bitmap: the same pixels as the scaled plain bitmap
	{
		grs_bitmap * plain = gr_create_bitmap( 16, 8 );
		memcpy( plain->bm_data, original, sizeof(original) );
		verts[0].x = i2f(2); verts[0].y = i2f(2);
		verts[2].x = i2f(50); verts[2].y = i2f(30);
		test_canvas_init( 1 );
		scale_bitmap( plain, verts );
		for (y=0; y<TH; y++ )
			for (x=0; x<TW; x++ )
				scaled[y*TW+x] = tpix(x,y);
		test_canvas_init( 1 );
		scale_bitmap( bm, verts );
		ok = 1;
		for (y=0; y<TH; y++ )
			for (x=0; x<TW; x++ )
				if (scaled[y*TW+x] != tpix(x,y)) ok = 0;
		CHECK( ok );
		CHECK( tpix(2,2) == 20 && tcount(1) < TW*TH - 1000 );

		// and smaller
		verts[2].x = i2f(9); verts[2].y = i2f(6);
		test_canvas_init( 1 );
		scale_bitmap( plain, verts );
		for (y=0; y<TH; y++ )
			for (x=0; x<TW; x++ )
				scaled[y*TW+x] = tpix(x,y);
		test_canvas_init( 1 );
		scale_bitmap( bm, verts );
		ok = 1;
		for (y=0; y<TH; y++ )
			for (x=0; x<TW; x++ )
				if (scaled[y*TW+x] != tpix(x,y)) ok = 0;
		CHECK( ok );
		gr_free_bitmap( plain );
	}

	// The texture cache expands a 64x64 RLE texture
	for (i=0; i<64*64; i++ )
		tex_original[i] = (ubyte)(((i/64)/8)*8 + ((i%64)/16));
	memcpy( tex->bm_data, tex_original, 64*64 );
	CHECK( gr_bitmap_rle_compress( tex ) == 1 );
	expanded = rle_expand_texture( tex );
	CHECK( expanded != NULL && expanded != tex );
	CHECK( !(expanded->bm_flags & BM_FLAG_RLE) );
	CHECK( memcmp( expanded->bm_data, tex_original, 64*64 ) == 0 );
	CHECK( rle_expand_texture( tex ) == expanded );		// from the cache

	gr_free_bitmap( bm );
	gr_free_bitmap( tex );
}

static void test_ibitblt_and_merge(void)
{
	grs_bitmap * cockpit = make_bitmap( 16, 8, 0 );
	grs_bitmap * view = make_bitmap( 16, 8, 100 );
	static ubyte before[16*8];
	ubyte * mask;
	int x, y, ok, minx, miny, maxx, maxy;
	static ubyte lower[64*64], upper[64*64], merged[64*64];
	static ubyte b1[64], b2[64], b3[64];

	// A hole (transparent pixels) with a shape that is not a rectangle
	for (y=2; y<=5; y++ )
		for (x=3+(y&1); x<=12-y; x++ )
			cockpit->bm_data[y*16+x] = 255;
	cockpit->bm_data[7*16+15] = 255;
	memcpy( before, cockpit->bm_data, sizeof(before) );

	gr_ibitblt_find_hole_size( cockpit, &minx, &miny, &maxx, &maxy );
	CHECK( minx == 3 && miny == 2 && maxx == 15 && maxy == 7 );

	mask = gr_ibitblt_create_mask( cockpit, 0, 0, 16, 8, 16 );
	CHECK( mask != NULL );
	gr_ibitblt( view, cockpit, mask );
	ok = 1;
	for (y=0; y<8; y++ )
		for (x=0; x<16; x++ )	{
			int expect = before[y*16+x];
			if (expect == 255) expect = view->bm_data[y*16+x];
			if (cockpit->bm_data[y*16+x] != expect) ok = 0;
		}
	CHECK( ok );
	free( mask );
	gr_free_bitmap( cockpit );
	gr_free_bitmap( view );

	// Texture merge: the pixel x=1,y=0 of the upper texture in each rotation
	memset( lower, 5, sizeof(lower) );
	memset( upper, 255, sizeof(upper) );
	upper[1] = 7;
	gr_merge_textures( lower, upper, merged );
	CHECK( merged[1] == 7 && merged[0] == 5 && merged[64*64-1] == 5 );
	gr_merge_textures_1( lower, upper, merged );
	CHECK( merged[62*64+0] == 7 && merged[1] == 5 );
	gr_merge_textures_2( lower, upper, merged );
	CHECK( merged[63*64+62] == 7 && merged[1] == 5 );
	gr_merge_textures_3( lower, upper, merged );
	CHECK( merged[1*64+63] == 7 && merged[1] == 5 );

	// gr_update_buffer copies the units of 4 bytes that changed
	for (x=0; x<64; x++ )	{
		b1[x] = (ubyte)x; b2[x] = (ubyte)x; b3[x] = 0xAA;
	}
	b1[9] = 200; b1[40] = 201;
	gr_update_buffer( b1, b2, b3, 64 );
	CHECK( b3[8] == 8 && b3[9] == 200 && b3[11] == 11 && b3[40] == 201 && b3[43] == 43 );
	CHECK( b3[0] == 0xAA && b3[12] == 0xAA && b3[39] == 0xAA && b3[44] == 0xAA );
	CHECK( b3[60] == 60 && b3[63] == 63 );		// the last unit is always written
}

// The closest color in a palette, as the 2d library defines it: the
// first entry (0 to 253) with the smallest sum of the squared distances.
static int ref_closest(ubyte * pal, int r, int g, int b)
{
	int i, best = 0, best_value = -1;
	for (i=0; i<254; i++ )	{
		int dr = r - pal[i*3], dg = g - pal[i*3+1], db = b - pal[i*3+2];
		int value = dr*dr + dg*dg + db*db;
		if (best_value < 0 || value < best_value)	{
			best_value = value;
			best = i;
		}
	}
	return best;
}

static void test_palette(void)
{
	static ubyte pal[768], pal2[768];
	uint64_t t0, period = 1000000 / 70;
	int i, ok;

	// A palette with known colors
	for (i=0; i<256; i++ )	{
		pal[i*3+0] = (ubyte)(i & 63);
		pal[i*3+1] = (ubyte)((i >> 2) & 63);
		pal[i*3+2] = (ubyte)(63 - (i & 63));
	}
	pal[3*10+0] = 63; pal[3*10+1] = 0; pal[3*10+2] = 0;		// 10 = red
	pal[3*11+0] = 0; pal[3*11+1] = 63; pal[3*11+2] = 0;		// 11 = green
	memcpy( gr_palette, pal, 768 );

	gr_set_mode( SM_320x200C );		// 70 Hz
	gr_palette_set_gamma( 0 );

	// Load: the display palette is the palette
	Num_presents = 0;
	gr_palette_load( gr_palette );
	CHECK( gr_palette_faded_out == 0 );
	CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );
	CHECK( Num_presents == 1 );
	CHECK( memcmp( Present_palette, pal, 768 ) == 0 );
	gr_palette_read( pal2 );
	CHECK( memcmp( pal2, pal, 768 ) == 0 );

	// Closest color
	CHECK( gr_find_closest_color( 63, 0, 0 ) == 10 );
	CHECK( gr_find_closest_color( 0, 63, 0 ) == 11 );
	CHECK( gr_find_closest_color( 62, 1, 0 ) == 10 );
	CHECK( gr_find_closest_color( 62, 1, 0 ) == 10 );		// from the cache
	ok = 1;
	for (i=0; i<200; i++ )	{
		int r = (i*7) & 63, g = (i*13) & 63, b = (i*29) & 63;
		if (gr_find_closest_color( r, g, b ) != ref_closest( pal, r, g, b )) ok = 0;
		if (gr_find_closest_color_current( r, g, b ) != ref_closest( pal, r, g, b )) ok = 0;
	}
	CHECK( ok );
	CHECK( BM_XRGB( 31, 0, 0 ) == ref_closest( pal, 62, 0, 0 ) );

	// Gamma: added to each component, with a limit of 63
	gr_palette_set_gamma( 4 );
	CHECK( gr_palette_get_gamma() == 4 );
	ok = 1;
	for (i=0; i<768; i++ )
		if (gr_dac_palette[i] != (pal[i]+4 > 63 ? 63 : pal[i]+4)) ok = 0;
	CHECK( ok );
	gr_palette_set_gamma( 0 );
	CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );

	// Step up: adds to the display palette, does not show the frame
	Num_presents = 0;
	gr_palette_step_up( 10, 0, -5 );
	CHECK( Num_presents == 0 );
	ok = 1;
	for (i=0; i<256; i++ )	{
		int r = pal[i*3] + 10, b = pal[i*3+2] - 5;
		if (r > 63) r = 63;
		if (b < 0) b = 0;
		if (gr_dac_palette[i*3] != r || gr_dac_palette[i*3+1] != pal[i*3+1] || gr_dac_palette[i*3+2] != b) ok = 0;
	}
	CHECK( ok );
	gr_palette_step_up( 0, 0, 0 );
	CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );
	CHECK( memcmp( gr_palette, pal, 768 ) == 0 );		// gr_palette does not change

	// Fade out: 32 steps, one for each retrace of a 70 Hz display
	Num_presents = 0;
	Num_pumps = 0;
	t0 = plat_time_us();
	CHECK( gr_palette_fade_out( gr_palette, 32, 0 ) == 0 );
	CHECK( gr_palette_faded_out == 1 );
	CHECK( Num_presents == 32 );
	CHECK( Num_pumps == 32 );
	CHECK( plat_time_us() - t0 >= 32*period && plat_time_us() - t0 < 32*period + 1000 );
	ok = 1;
	for (i=0; i<768; i++ )
		if (gr_dac_palette[i] != 0) ok = 0;
	CHECK( ok );
	CHECK( gr_palette_fade_out( gr_palette, 32, 0 ) == 0 );		// no second fade
	CHECK( Num_presents == 32 );

	// Fade in: the same time, and the palette at the end
	Num_presents = 0;
	t0 = plat_time_us();
	CHECK( gr_palette_fade_in( gr_palette, 32, 0 ) == 0 );
	CHECK( gr_palette_faded_out == 0 );
	CHECK( Num_presents == 32 );
	CHECK( plat_time_us() - t0 >= 32*period && plat_time_us() - t0 < 32*period + 1000 );
	CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );

	// The middle of a fade: half of each component
	gr_palette_clear();
	CHECK( gr_palette_faded_out == 1 );
	gr_palette_fade_in( gr_palette, 2, 0 );
	gr_palette_fade_out( gr_palette, 2, 0 );
	gr_palette_fade_in( gr_palette, 2, 0 );
	CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );

	// A display that is slower than the emulated one (each present takes
	// 1/60 second): the fade takes the same time, and shows fewer steps.
	gr_palette_fade_out( gr_palette, 32, 0 );
	{
		Present_cost_us = 16667;
		Num_presents = 0;
		t0 = plat_time_us();
		gr_palette_fade_in( gr_palette, 32, 0 );
		Present_cost_us = 0;
		CHECK( Num_presents < 32 && Num_presents >= 24 );
		CHECK( plat_time_us() - t0 >= 32*period && plat_time_us() - t0 < 32*period + 17000 );
		CHECK( memcmp( gr_dac_palette, pal, 768 ) == 0 );
		CHECK( memcmp( Present_palette, pal, 768 ) == 0 );		// the last step is shown
	}

	// Clear
	Num_presents = 0;
	gr_palette_clear();
	CHECK( Num_presents == 1 );
	CHECK( gr_dac_palette[0] == 0 && gr_dac_palette[767] == 0 );
	gr_palette_load( gr_palette );

	// gr_sync_display waits for the next retrace
	t0 = plat_time_us();
	gr_sync_display();
	CHECK( plat_time_us() > t0 && plat_time_us() - t0 <= period + 1000 );
	CHECK( plat_time_us() % period < 1000 );
}

static void test_pcx(const char * picture_name)
{
	static char name[1024];
	static ubyte pal[768], pal2[768];
	grs_bitmap * bm = gr_create_bitmap( 70, 9 );
	grs_bitmap in;
	int i, ret;
	FILE * fp;

	// The file goes next to the output picture.
	snprintf( name, sizeof(name), "%s", picture_name );
	if (strlen(name) > 4 && name[strlen(name)-4] == '.')
		name[strlen(name)-4] = 0;
	strcat( name, "_tmp.pcx" );

	// Runs longer than 63, single pixels, and values 0xC0 and above
	for (i=0; i<70*9; i++ )	{
		int y = i / 70, x = i % 70;
		if (y < 2) bm->bm_data[i] = 17;
		else if (y < 4) bm->bm_data[i] = (ubyte)(0xC0 + (x & 31));
		else bm->bm_data[i] = (ubyte)((x*3 + y) & 255);
	}
	for (i=0; i<768; i++ )
		pal[i] = (ubyte)((i*5) & 63);

	ret = pcx_write_bitmap( name, bm, pal );
	CHECK( ret == PCX_ERROR_NONE );

	// The header of the file
	fp = fopen( name, "rb" );
	CHECK( fp != NULL );
	if (fp)	{
		ubyte header[128];
		CHECK( fread( header, 1, 128, fp ) == 128 );
		CHECK( header[0] == 10 && header[1] == 5 && header[2] == 1 && header[3] == 8 );
		CHECK( header[8] == 69 && header[9] == 0 && header[10] == 8 && header[11] == 0 );	// Xmax, Ymax
		CHECK( header[65] == 1 && header[66] == 70 && header[67] == 0 );						// planes, bytes per line
		fclose( fp );
	}

	memset( &in, 0, sizeof(in) );
	memset( pal2, 0, sizeof(pal2) );
	ret = pcx_read_bitmap( name, &in, BM_LINEAR, pal2 );
	CHECK( ret == PCX_ERROR_NONE );
	if (ret == PCX_ERROR_NONE)	{
		CHECK( in.bm_w == 70 && in.bm_h == 9 && in.bm_rowsize == 70 );
		CHECK( memcmp( in.bm_data, bm->bm_data, 70*9 ) == 0 );
		CHECK( memcmp( pal, pal2, 768 ) == 0 );
		free( in.bm_data );
	}
	CHECK( pcx_read_bitmap( "no_such_file.pcx", &in, BM_LINEAR, pal2 ) == PCX_ERROR_OPENING );
	CHECK( strcmp( pcx_errormsg( PCX_ERROR_OPENING ), "Error opening file." ) == 0 );

	remove( name );
	gr_free_bitmap( bm );
}

//---------------------------------------------------------------------
// Tests with the files of the game

// A font file, read by the test itself (not by the library).
typedef struct test_font {
	ubyte * file;			// the whole file
	int file_size;
	ubyte * data;			// the font data (after the 8 bytes of id and size)
	int datasize;
	int w, h, flags, baseline, minchar, maxchar;
	int data_offset, widths_offset, kern_offset;
} test_font;

static int rd16(ubyte * p) { return (short)(p[0] | (p[1] << 8)); }
static int rd32(ubyte * p) { return (int)((unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24)); }

static ubyte * read_file(const char * name, int * size)
{
	FILE * fp = fopen( name, "rb" );
	ubyte * buf;
	long n;

	if (!fp) return NULL;
	fseek( fp, 0, SEEK_END );
	n = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	buf = (ubyte *)malloc( n > 0 ? n : 1 );
	if (fread( buf, 1, n, fp ) != (size_t)n)	{
		fclose( fp );
		return NULL;
	}
	fclose( fp );
	*size = (int)n;
	return buf;
}

static int test_font_read(test_font * tf, const char * name)
{
	tf->file = read_file( name, &tf->file_size );
	if (!tf->file || tf->file_size < 36 || memcmp( tf->file, "PSFN", 4 ) != 0)
		return 0;
	tf->datasize = rd32( tf->file+4 );
	tf->data = tf->file + 8;
	tf->w = rd16( tf->data+0 );
	tf->h = rd16( tf->data+2 );
	tf->flags = rd16( tf->data+4 );
	tf->baseline = rd16( tf->data+6 );
	tf->minchar = tf->data[8];
	tf->maxchar = tf->data[9];
	tf->data_offset = rd32( tf->data+12 );
	tf->widths_offset = rd32( tf->data+20 );
	tf->kern_offset = rd32( tf->data+24 );
	return 1;
}

static int test_font_width(test_font * tf, int c)
{
	if (tf->flags & FT_PROPORTIONAL)
		return rd16( tf->data + tf->widths_offset + (c - tf->minchar)*2 );
	return tf->w;
}

// Returns pixel x,y of the character c from the file: for a color font
// the color number in the palette of the font, else 1 (set) or 0.
static int test_font_pixel(test_font * tf, int c, int x, int y)
{
	int i, w = test_font_width( tf, c );
	ubyte * p;

	if (tf->flags & FT_PROPORTIONAL)	{
		p = tf->data + tf->data_offset;
		for (i=tf->minchar; i<c; i++ )	{
			int wi = test_font_width( tf, i );
			p += ((tf->flags & FT_COLOR) ? wi : (wi+7)/8) * tf->h;
		}
	} else
		p = tf->data + 28 + (c - tf->minchar) * ((w+7)/8) * tf->h;

	if (tf->flags & FT_COLOR)
		return p[y*w + x];
	return (p[y*((w+7)/8) + x/8] >> (7 - (x & 7))) & 1;
}

static char Data_dir[900];

static char * data_path(const char * name)
{
	static char path[1024];
	snprintf( path, sizeof(path), "%s/%s", Data_dir, name );
	return path;
}

static int file_exists(const char * name)
{
	FILE * fp = fopen( name, "rb" );
	if (fp) fclose( fp );
	return fp != NULL;
}

// Draws a string with a font that has one bit for each pixel and
// compares each pixel with the bits of the file.
static void check_mono_string(grs_font * font, test_font * tf, char * s, int x0, int y0, int fg, int bg)
{
	int x, y, i, ok = 1, set = 0;
	int cx = x0;
	int back = 1;

	test_canvas_init( back );
	gr_set_curfont( font );
	gr_set_fontcolor( fg, bg );
	gr_ustring( x0, y0, s );

	for (i=0; s[i]; i++ )	{
		int w = test_font_width( tf, s[i] );
		for (y=0; y<tf->h; y++ )
			for (x=0; x<w; x++ )	{
				int expect = test_font_pixel( tf, s[i], x, y ) ? fg : (bg == -1 ? back : bg);
				if (tpix(cx+x,y0+y) != expect) ok = 0;
				if (expect == fg) set++;
			}
		cx += w;
	}
	CHECK( ok );
	CHECK( set > 10 );						// the string has pixels
	CHECK( tcount(fg) == set );				// and no other pixel has the color
	CHECK( tguard_ok() );
}

static void test_fonts(void)
{
	test_font tf;
	grs_font * font;
	int w, h, aw, i, x, y, ok, sum;
	static ubyte unclipped[TW*TH];

	//--- FNTEMFM_.FNT: proportional, one bit for each pixel
	CHECK( test_font_read( &tf, data_path("FNTEMFM_.FNT") ) );
	font = gr_init_font( data_path("FNTEMFM_.FNT") );
	CHECK( font != NULL );
	CHECK( font->ft_w == tf.w && font->ft_h == tf.h && font->ft_flags == tf.flags );
	CHECK( font->ft_w == 17 && font->ft_h == 11 && font->ft_flags == FT_PROPORTIONAL );
	CHECK( font->ft_minchar == 32 && font->ft_maxchar == 135 && font->ft_baseline == tf.baseline );
	CHECK( font->ft_widths != NULL && font->ft_chars != NULL );
	CHECK( grd_curcanv->cv_font == font );
	ok = 1;
	for (i=tf.minchar; i<=tf.maxchar; i++ )
		if (font->ft_widths[i-tf.minchar] != test_font_width( &tf, i )) ok = 0;
	CHECK( ok );
	CHECK( font->ft_chars[0] - font->ft_data == 0 );
	CHECK( font->ft_chars['B'-32] - font->ft_chars['A'-32] == ((test_font_width(&tf,'A')+7)/8) * tf.h );

	check_mono_string( font, &tf, "Ab1", 3, 5, 7, -1 );
	check_mono_string( font, &tf, "Ab1", 3, 5, 7, 2 );
	check_mono_string( font, &tf, "ASC", 5, 30, 9, -1 );

	// Text on a canvas with the SVGA type gives the same pixels
	test_canvas_init( 1 );
	Test_canvas.cv_bitmap.bm_type = BM_SVGA;
	gr_set_curfont( font );
	gr_set_fontcolor( 7, -1 );
	gr_ustring( 3, 5, "Ab1" );
	Test_canvas.cv_bitmap.bm_type = BM_LINEAR;
	ok = 1;
	for (y=0; y<tf.h; y++ )
		for (x=0; x<test_font_width(&tf,'A'); x++ )
			if ((tpix(3+x,5+y) == 7) != test_font_pixel( &tf, 'A', x, y )) ok = 0;
	CHECK( ok );
	CHECK( tcount(7) > 10 );

	// String size, with a new line
	gr_get_string_size( "Ab1", &w, &h, &aw );
	CHECK( w == test_font_width(&tf,'A') + test_font_width(&tf,'b') + test_font_width(&tf,'1') );
	CHECK( h == tf.h && aw == tf.w );
	gr_get_string_size( "A\nAb", &w, &h, &aw );
	CHECK( w == test_font_width(&tf,'A') + test_font_width(&tf,'b') && h == 2*tf.h );

	// A string that is clipped at the left: the same pixels as the
	// string that is not clipped, moved 4 pixels to the left.
	test_canvas_init( 1 );
	gr_set_curfont( font );
	gr_set_fontcolor( 7, -1 );
	gr_string( 10, 5, "ASCENT" );
	for (y=0; y<TH; y++ )
		for (x=0; x<TW; x++ )
			unclipped[y*TW+x] = tpix(x,y);
	test_canvas_init( 1 );
	gr_set_curfont( font );
	gr_set_fontcolor( 7, -1 );
	gr_string( -4, 5, "ASCENT" );
	ok = 1;
	for (y=0; y<TH; y++ )
		for (x=0; x<TW-14; x++ )
			if (tpix(x,y) != unclipped[y*TW+x+14]) ok = 0;
	CHECK( ok );
	CHECK( tcount(7) > 20 );
	// clipped at the bottom right, and outside
	gr_string( TW-9, TH-5, "ASCENT" );
	gr_string( TW+5, 5, "ASCENT" );
	gr_string( 5, -40, "ASCENT" );
	gr_printf( 1, TH-4, "%d", 2026 );
	CHECK( tguard_ok() );

	// Centered string (x = 0x8000)
	test_canvas_init( 1 );
	gr_set_curfont( font );
	gr_set_fontcolor( 7, -1 );
	gr_get_string_size( "Ab1", &w, &h, &aw );
	gr_string( 0x8000, 20, "Ab1" );
	ok = 1;
	for (y=0; y<tf.h; y++ )
		for (x=0; x<test_font_width(&tf,'A'); x++ )
			if ((tpix((TW-w)/2+x,20+y) == 7) != test_font_pixel( &tf, 'A', x, y )) ok = 0;
	CHECK( ok );
	gr_close_font( font );
	free( tf.file );

	//--- FNTEMFS_.FNT: fixed width, 256 characters
	CHECK( test_font_read( &tf, data_path("FNTEMFS_.FNT") ) );
	font = gr_init_font( data_path("FNTEMFS_.FNT") );
	CHECK( font->ft_w == 6 && font->ft_h == 8 && font->ft_flags == 0 );
	CHECK( font->ft_minchar == 0 && font->ft_maxchar == 255 );
	CHECK( font->ft_widths == NULL && font->ft_chars == NULL && font->ft_kerndata == NULL );
	check_mono_string( font, &tf, "Hi 2d!", 2, 2, 7, -1 );
	check_mono_string( font, &tf, "Hi 2d!", 2, 2, 7, 3 );
	gr_get_string_size( "Hi 2d!", &w, &h, &aw );
	CHECK( w == 36 && h == 8 );
	gr_close_font( font );
	free( tf.file );

	//--- FNTEMFL_.FNT: proportional with a kerning table
	CHECK( test_font_read( &tf, data_path("FNTEMFL_.FNT") ) );
	font = gr_init_font( data_path("FNTEMFL_.FNT") );
	CHECK( font->ft_flags == (FT_PROPORTIONAL|FT_KERNED) );
	CHECK( font->ft_kerndata != NULL );
	CHECK( font->ft_kerndata - font->ft_data == tf.kern_offset - tf.data_offset );
	CHECK( memcmp( font->ft_kerndata, tf.data + tf.kern_offset, tf.datasize - tf.kern_offset ) == 0 );
	// Each pair of the kerning table changes the spacing
	{
		ubyte * k = tf.data + tf.kern_offset;
		int spacing;
		gr_set_curfont( font );
		for ( ; *k != 255; k += 3 )	{
			get_char_width( k[0]+tf.minchar, k[1]+tf.minchar, &w, &spacing );
			CHECK( spacing == k[2] && w == test_font_width( &tf, k[0]+tf.minchar ) );
		}
	}
	sum = test_font_width(&tf,'D') + test_font_width(&tf,'T') + test_font_width(&tf,'U');
	gr_get_string_size( "DTU", &w, &h, &aw );
	CHECK( h == tf.h && w > 0 && w <= sum );
	check_mono_string( font, &tf, "9", 4, 4, 7, -1 );
	gr_close_font( font );
	free( tf.file );

	//--- FNTECHMS.FNT: proportional color font with its own palette
	CHECK( test_font_read( &tf, data_path("FNTECHMS.FNT") ) );
	CHECK( tf.file_size == 8 + tf.datasize + 768 );
	font = gr_init_font( data_path("FNTECHMS.FNT") );
	CHECK( font->ft_flags == (FT_PROPORTIONAL|FT_COLOR) );
	CHECK( font->ft_w == 15 && font->ft_h == 10 );
	{
		ubyte * fpal = tf.file + 8 + tf.datasize;
		char * s = "Dt9";
		int cx = 2, drawn = 0;

		// The loader maps the colors of the font to the game palette.
		test_canvas_init( 1 );
		gr_set_curfont( font );
		gr_string( 2, 8, s );
		ok = 1;
		for (i=0; s[i]; i++ )	{
			int cw = test_font_width( &tf, s[i] );
			for (y=0; y<tf.h; y++ )
				for (x=0; x<cw; x++ )	{
					int c = test_font_pixel( &tf, s[i], x, y );
					int expect;
					if (c == 255)
						expect = 1;			// transparent
					else {
						expect = ref_closest( gr_palette, fpal[c*3], fpal[c*3+1], fpal[c*3+2] );
						drawn++;
					}
					if (tpix(cx+x,8+y) != expect) ok = 0;
				}
			cx += cw;
		}
		CHECK( ok );
		CHECK( drawn > 40 );
		CHECK( tguard_ok() );
	}
	gr_close_font( font );
	free( tf.file );
}

static void test_palette_file(void)
{
	int size, i, ok;
	ubyte * file = read_file( data_path("PALETTE.256"), &size );

	CHECK( file != NULL && size == 9472 );
	gr_use_palette_table( data_path("PALETTE.256") );
	if (!file)
		return;
	CHECK( memcmp( gr_palette, file, 768 ) == 0 );
	ok = 1;
	for (i=0; i<768; i++ )
		if (gr_palette[i] > 63) ok = 0;
	CHECK( ok );
	// The fade table, except the transparent color of each level
	ok = 1;
	for (i=0; i<256*34; i++ )	{
		int expect = ((i & 255) == 255) ? 255 : file[768+i];
		if (gr_fade_table[i] != expect) ok = 0;
	}
	CHECK( ok );
	free( file );
}

// Draws one frame on the 320x200 screen with primitives, bitmaps and
// text, and writes it to a picture file.
static void test_frame(const char * picture_name, int have_data)
{
	static const char * font_names[] = { "FNTECHL_.FNT", "FNTEMFL_.FNT", "FNTECHMS.FNT", "FNTEMFM_.FNT", "FNTEMFS_.FNT", "FNTEMHS_.FNT", "FNTEMLS_.FNT" };
	grs_bitmap * bm;
	grs_point verts[3];
	int i, x, y;
	int white, red, green, blue, yellow, grey, dark;
	FILE * fp;

	gr_set_mode( SM_320x200C );
	gr_set_current_canvas( NULL );

	if (!have_data)	{
		// A simple palette: color i has the components of its bits.
		for (i=0; i<256; i++ )	{
			gr_palette[i*3+0] = (ubyte)(((i >> 5) & 7) * 9);
			gr_palette[i*3+1] = (ubyte)(((i >> 2) & 7) * 9);
			gr_palette[i*3+2] = (ubyte)((i & 3) * 21);
		}
	}
	gr_palette_load( gr_palette );

	white = gr_find_closest_color( 63, 63, 63 );
	red = gr_find_closest_color( 63, 0, 0 );
	green = gr_find_closest_color( 0, 63, 0 );
	blue = gr_find_closest_color( 0, 0, 63 );
	yellow = gr_find_closest_color( 63, 63, 0 );
	grey = gr_find_closest_color( 32, 32, 32 );
	dark = gr_find_closest_color( 6, 6, 12 );

	gr_clear_canvas( dark );

	// All the colors of the palette
	for (i=0; i<256; i++ )	{
		gr_setcolor( i );
		gr_rect( 4 + (i%64)*3, 180 + (i/64)*4, 4 + (i%64)*3 + 2, 180 + (i/64)*4 + 3 );
	}

	// Primitives
	gr_setcolor( grey );
	gr_box( 2, 2, 317, 197 );
	gr_setcolor( red );
	gr_disk( i2f(230), i2f(40), i2f(18) );
	gr_setcolor( yellow );
	gr_circle( i2f(230), i2f(40), i2f(24) );
	gr_setcolor( green );
	for (i=0; i<=10; i++ )
		gr_line( i2f(260), i2f(70), i2f(260 + i*5), i2f(110) );
	gr_setcolor( blue );
	{
		int poly[8] = { i2f(270), i2f(15), i2f(310), i2f(25), i2f(300), i2f(60), i2f(265), i2f(50) };
		gr_poly( 4, poly );
	}

	// A bitmap, the same bitmap scaled, and as an RLE bitmap
	bm = gr_create_bitmap( 16, 16 );
	for (y=0; y<16; y++ )
		for (x=0; x<16; x++ )	{
			int c = ((x/4 + y/4) & 1) ? white : red;
			if ((x-8)*(x-8) + (y-8)*(y-8) < 12) c = blue;
			if (x == 0 || y == 0 || x == 15 || y == 15) c = yellow;
			bm->bm_data[y*16+x] = (ubyte)c;
		}
	gr_bitmap( 200, 70, bm );
	verts[0].x = i2f(200); verts[0].y = i2f(90);
	verts[2].x = i2f(247); verts[2].y = i2f(137);
	scale_bitmap( bm, verts );
	CHECK( gr_video_memory[70*320+200] == yellow );
	CHECK( gr_video_memory[90*320+200] == yellow && gr_video_memory[137*320+247] == yellow );
	CHECK( gr_video_memory[(90+24)*320+200+24] == blue );
	if (gr_bitmap_rle_compress( bm ))	{
		gr_bitmap( 222, 70, bm );
		CHECK( memcmp( &gr_video_memory[75*320+222], &gr_video_memory[75*320+200], 16 ) == 0 );
	}
	gr_free_bitmap( bm );

	// Text with each font of the game
	if (have_data)	{
		int ty = 6;
		for (i=0; i<(int)(sizeof(font_names)/sizeof(font_names[0])); i++ )	{
			grs_font * font;
			if (!file_exists( data_path(font_names[i]) ))
				continue;
			font = gr_init_font( data_path(font_names[i]) );
			gr_set_curfont( font );
			gr_set_fontcolor( (i & 1) ? yellow : white, -1 );
			if (font->ft_w > 20)
				gr_printf( 8, ty, "ASCENT 2D" );
			else
				gr_printf( 8, ty, "ASCENT 2D %s", font_names[i] );
			ty += font->ft_h + 3;
			if (font->ft_h <= 11 && !(font->ft_flags & FT_COLOR))	{
				gr_set_fontcolor( green, -1 );
				gr_string( 8, ty, "The quick brown fox 0123" );
				ty += font->ft_h + 3;
			}
			grd_curcanv->cv_font = NULL;
			gr_close_font( font );
		}
	}

	Num_presents = 0;
	gr_present();
	CHECK( Num_presents == 1 && Present_pixels == gr_video_memory );

	// Write the frame as a PPM picture, with the palette of the display.
	fp = fopen( picture_name, "wb" );
	CHECK( fp != NULL );
	if (fp)	{
		fprintf( fp, "P6\n%d %d\n255\n", Present_w, Present_h );
		for (y=0; y<Present_h; y++ )
			for (x=0; x<Present_w; x++ )	{
				int c = Present_pixels[y*Present_pitch + x];
				for (i=0; i<3; i++ )	{
					int v = Present_palette[c*3+i] & 63;
					fputc( (v << 2) | (v >> 4), fp );
				}
			}
		fclose( fp );
		printf( "wrote %s\n", picture_name );
	}
}

int main(int argc, char ** argv)
{
	const char * dir = NULL;
	const char * picture_name = "test_2d.ppm";
	int have_data;

	if (argc > 1 && argv[1][0])
		dir = argv[1];
	else
		dir = getenv( "ASCENT_DATA_DIR" );
	if (argc > 2)
		picture_name = argv[2];
	snprintf( Data_dir, sizeof(Data_dir), "%s", dir ? dir : "." );

	have_data = file_exists( data_path("PALETTE.256") ) && file_exists( data_path("FNTEMFM_.FNT") )
		&& file_exists( data_path("FNTEMFS_.FNT") ) && file_exists( data_path("FNTEMFL_.FNT") )
		&& file_exists( data_path("FNTECHMS.FNT") );

	test_modes();
	test_pixels_and_rects();
	test_lines();
	test_circles();
	test_polygons();
	test_bitblt();
	test_scale();
	test_rle();
	test_ibitblt_and_merge();
	test_palette();
	test_pcx( picture_name );

	if (have_data)	{
		test_palette_file();
		gr_palette_load( gr_palette );
		test_fonts();
	} else
		printf( "SKIPPED the data tests (fonts, palette file): PALETTE.256 and the *.FNT files\n"
				"are not in the directory '%s'. Give the directory of the game files as the\n"
				"first argument, or set ASCENT_DATA_DIR.\n", Data_dir );

	test_frame( picture_name, have_data );

	printf( "%d checks, %d failed%s\n", Num_checks, Num_failed, have_data ? "" : " (data tests skipped)" );
	return Num_failed ? 1 : 0;
}

//---------------------------------------------------------------------
// Stubs for the functions of the other libraries. Used only when the
// test is built without those libraries (ASCENT_2D_TEST_STUBS).

#ifdef ASCENT_2D_TEST_STUBS

void Error(char *fmt,...)
{
	va_list args;
	va_start( args, fmt );
	vprintf( fmt, args );
	va_end( args );
	printf( "\n" );
	exit(1);
}
void Warning(char *fmt,...) { }
void _Assert(int expr,char *expr_text,char *filename,int linenum) { if (!expr) { printf( "Assert failed: %s\n", expr_text ); exit(1); } }
void _Int3(char *filename,int linenum) { }
void _mprintf( int n, char * format, ... ) { }

#undef malloc
#undef free
void * mem_malloc( unsigned int size, char * var, char * file, int line, int fill_zero )
{
	return fill_zero ? calloc( 1, size ) : malloc( size );
}
void mem_free( void * buffer ) { free( buffer ); }

CFILE * cfopen(char * filename, char * mode)
{
	FILE * fp = fopen( filename, "rb" );
	CFILE * cf;
	if (!fp) return NULL;
	cf = malloc( sizeof(CFILE) );
	cf->file = fp;
	fseek( fp, 0, SEEK_END );
	cf->size = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	cf->lib_offset = 0;
	cf->raw_position = 0;
	return cf;
}
int cfilelength( CFILE *fp ) { return fp->size; }
size_t cfread( void * buf, size_t elsize, size_t nelem, CFILE * fp ) { return fread( buf, elsize, nelem, fp->file ); }
void cfclose( CFILE * cfile ) { fclose( cfile->file ); free( cfile ); }

#endif
