/*
 * ASCENT: C replacement for modex.asm (VGA "Mode X" planar modes).
 *
 * modex.asm stays in the tree as the reference. It is not built.
 *
 * There is no planar video memory now. A Mode X screen mode is a linear
 * 8-bit frame in memory, the same as each other mode (see gr.c). The
 * functions here keep their names and parameters, and operate on that
 * frame. The 2d library does not call the drawing functions: it routes
 * BM_MODEX bitmaps to the linear code.
 */

#include <string.h>

#include "types.h"
#include "gr.h"
#include "grdef.h"
#include "platform.h"

// The sizes of the Mode X modes 1 to 12 (SM_320x200U to SM_376x564U),
// from the mode table in tweak.inc.
static const short modex_sizes[12][2] = {
	{ 320, 200 }, { 320, 240 }, { 360, 200 }, { 360, 240 },
	{ 376, 282 }, { 320, 400 }, { 320, 480 }, { 360, 400 },
	{ 360, 480 }, { 360, 360 }, { 376, 308 }, { 376, 564 },
};

// Returns the width in the high 16 bits and the height in the low 16 bits.
// As in the assembly, a mode number that is not in the table gives mode 1.
int gr_modex_setmode(short mode)
{
	unsigned int index = (unsigned int)((int)mode - 1);

	if (index > 11)
		index = 0;

	return ((int)modex_sizes[index][0] << 16) | modex_sizes[index][1];
}

// There are no planes.
void gr_modex_setplane(short plane)
{
	plane = plane;
}

// Sets the first pixel that the display shows (page flip), then shows
// the frame. x and y are pixel coordinates in the video memory.
void gr_modex_setstart(short x, short y, int wait_for_retrace)
{
	// ASCENT: the assembly waited for the vertical retrace if
	// wait_for_retrace was set. The platform layer synchronizes
	// gr_present with the display, so there is no second wait here.
	wait_for_retrace = wait_for_retrace;
	gr_display_start = (int)y * grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize + x;
	gr_present();
}

// Waits for the next vertical retrace of the emulated display.
// The retraces are at multiples of 1/gr_refresh_hz seconds on the
// platform clock. The function also processes the window events.
void gr_sync_display(void)
{
	uint64_t period = 1000000u / (unsigned int)(gr_refresh_hz > 0 ? gr_refresh_hz : 70);
	uint64_t target = (plat_time_us() / period + 1) * period;

	plat_pump();
	while (plat_time_us() < target)
		plat_sleep_ms(1);
}

// x1 and x2 don't need to be sorted. The coordinates are relative to
// the start of the video memory.
void gr_modex_uscanline( short x1, short x2, short y, unsigned char color )
{
	int rowsize = grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize;

	if (x2 < x1)	{
		short t = x1; x1 = x2; x2 = t;
	}
	memset( gr_video_memory + (int)y * rowsize + x1, color, (size_t)(x2 - x1 + 1) );
}

// in line.c
extern int modex_line_x1;
extern int modex_line_y1;
extern int modex_line_x2;
extern int modex_line_y2;
extern ubyte modex_line_Color;

// Draws a line between the points in the modex_line_ variables.
// The coordinates are relative to the start of the video memory.
// ASCENT: the assembly used a Bresenham line. This function uses the
// line of linear.c, so some pixels between the end points can differ.
void gr_modex_line(void)
{
	unsigned int save_color = gr_var_color;
	unsigned char * save_bitmap = gr_var_bitmap;
	unsigned int save_bwidth = gr_var_bwidth;

	gr_var_color = modex_line_Color;
	gr_var_bitmap = gr_video_memory;
	gr_var_bwidth = grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize;

	gr_linear_line( modex_line_x1, modex_line_y1, modex_line_x2, modex_line_y2 );

	gr_var_color = save_color;
	gr_var_bitmap = save_bitmap;
	gr_var_bwidth = save_bwidth;
}
