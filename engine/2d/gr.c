/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/2d/rcs/gr.c $
 * $Revision: 1.56 $
 * $Author: john $
 * $Date: 1995/05/08 11:22:47 $
 *
 * Graphical routines for setting video modes, etc.
 *
 * $Log: gr.c $
 * Revision 1.56  1995/05/08  11:22:47  john
 * Added 320x400 3dbios mode.
 * 
 * Revision 1.55  1995/02/02  16:44:05  john
 * Again with prev.
 * 
 * Revision 1.54  1995/02/02  16:42:18  john
 * Fixed palette with text fading out.
 * 
 * Revision 1.53  1995/02/02  14:26:20  john
 * Made palette fades work better with gamma thingy..
 * 
 * Revision 1.52  1995/02/02  14:08:59  john
 * Made palette gamma reset to 0 before exiting to Dos.
 * 
 * Revision 1.51  1995/01/30  18:06:35  john
 * Added text screen fade in/out, and restored video mode properly.
 * 
 * Revision 1.50  1995/01/24  17:58:16  john
 * Added code to return to 80x25 when graphics close.
 * 
 * Revision 1.49  1994/11/30  20:16:05  john
 * Fixed bug that the grd_curscreen flags were never initialized.
 * 
 * Revision 1.48  1994/11/24  13:24:43  john
 * Made sure that some rep movs had the cld set first.
 * Took some unused functions out.
 * 
 * Revision 1.47  1994/11/18  22:50:20  john
 * Changed shorts to ints in parameters.
 * 
 * Revision 1.46  1994/11/15  18:28:36  john
 * Made text screen fade in.
 * 
 * Revision 1.45  1994/11/15  17:55:11  john
 * Made text palette fade in when game over.
 * 
 * Revision 1.44  1994/11/07  12:07:30  john
 * Made save/restore cursor work a bit better.
 * 
 * Revision 1.43  1994/11/05  12:46:44  john
 * Changed palette stuff a bit.
 * 
 * Revision 1.42  1994/10/26  23:55:50  john
 * Took out roller; Took out inverse table.
 * 
 * Revision 1.41  1994/10/10  13:59:50  john
 * *** empty log message ***
 * 
 * Revision 1.40  1994/10/10  13:58:50  john
 * Added better VGA detection scheme.
 * 
 * Revision 1.39  1994/09/29  10:09:15  john
 * Hacked out VGA card detection for now.
 * 
 * Revision 1.38  1994/09/22  17:35:35  john
 * Fixed bug with palette not reloading in
 * gr_set_mode
 * 
 * Revision 1.37  1994/09/22  16:08:42  john
 * Fixed some palette stuff.
 * 
 * Revision 1.36  1994/09/19  11:44:23  john
 * Changed call to allocate selector to the dpmi module.
 * 
 * Revision 1.35  1994/09/12  19:28:11  john
 * Fixed bug with unclipped fonts clipping.
 * 
 * Revision 1.34  1994/09/12  18:20:18  john
 * Made text fade out
 * 
 * Revision 1.33  1994/09/12  14:40:15  john
 * Neatend.
 * 
 * Revision 1.32  1994/08/15  15:01:01  matt
 * Set ptr to NULL after freeing
 * 
 * Revision 1.31  1994/07/27  18:30:28  john
 * Took away the blending table.
 * 
 * Revision 1.30  1994/06/24  17:26:59  john
 * Made rowsizes bigger than actual screen work with SVGA.
 * 
 * Revision 1.29  1994/05/12  17:33:16  john
 * Added circle code.
 * 
 * Revision 1.28  1994/05/10  19:51:49  john
 * Added 320x100 mode.
 * 
 * Revision 1.27  1994/05/06  12:50:23  john
 * Added supertransparency; neatend things up; took out warnings.
 * 
 * Revision 1.26  1994/05/03  19:39:00  john
 * *** empty log message ***
 * 
 * Revision 1.25  1994/04/08  16:59:34  john
 * Add fading poly's; Made palette fade 32 instead of 16.
 * 
 * Revision 1.24  1994/03/14  16:56:16  john
 * Changed grs_bitmap structure to include bm_flags.
 * 
 * Revision 1.23  1994/02/18  15:32:27  john
 * *** empty log message ***
 * 
 * Revision 1.22  1994/01/25  11:40:44  john
 * Added gr_check_mode function.
 * 
 * Revision 1.21  1993/12/21  19:57:48  john
 * added selector stuff.
 * 
 * Revision 1.20  1993/12/21  11:40:32  john
 * *** empty log message ***
 * 
 * Revision 1.19  1993/12/09  15:02:13  john
 * Changed palette stuff majorly
 * 
 * Revision 1.18  1993/11/16  11:28:36  john
 * *** empty log message ***
 * 
 * Revision 1.17  1993/10/26  13:17:53  john
 * *** empty log message ***
 * 
 * Revision 1.16  1993/10/15  16:23:42  john
 * y
 * 
 * Revision 1.15  1993/09/29  16:15:21  john
 * optimized
 * 
 * Revision 1.14  1993/09/28  19:06:51  john
 * made gr_set_mode change the grd_cursreen->sc_mode variable.
 * 
 * Revision 1.13  1993/09/28  12:52:12  matt
 * Set aspect ratio of current screen in gr_init() and gr_set_mode().
 * 
 * Revision 1.12  1993/09/27  13:00:24  john
 * made gr_set_mode not call mode_reset on fail
 * 
 * Revision 1.11  1993/09/27  12:51:27  john
 * fixed gr_set_mode to return values
 * 
 * Revision 1.10  1993/09/26  18:59:12  john
 * fade in/out stuff
 * 
 * Revision 1.9  1993/09/21  14:00:41  john
 * added code to save 43/50 line modes.
 * 
 * Revision 1.8  1993/09/20  14:48:48  john
 * *** empty log message ***
 * 
 * Revision 1.7  1993/09/16  17:27:48  john
 * Added code to save/restore video mode.
 * 
 * Revision 1.6  1993/09/16  16:30:15  john
 * Made gr_close retore Mode 3 always.
 * 
 * Revision 1.5  1993/09/14  18:40:07  john
 * Made it so that gr_setmode doesn't change curcanv font and colors.
 * 
 * Revision 1.4  1993/09/14  16:27:01  matt
 * Changes gr_change_mode() to be gr_set_mode()
 * After gr_set_mode(), grd_curcanv is the canvas of the new screen
 * Made gr_set_mode() work with the new grd_curcanv, not the old one
 * 
 * Revision 1.3  1993/09/14  13:08:37  john
 * Added gr_changemode
 * 
 * Revision 1.2  1993/09/08  17:36:37  john
 * Looking for error for Yuan... Neatened the nested ifs in setmode.
 * 
 * Revision 1.1  1993/09/08  11:43:51  john
 * Initial revision
 * 
 *
 */

// ASCENT: removed <dos.h>, <conio.h>, <malloc.h> and "dpmi.h". This file
// had the BIOS calls and the VGA register writes that set the screen
// mode and that saved and restored the DOS text screen. Now each screen
// mode is a linear 8-bit frame in memory, and gr_present gives the frame
// to the platform layer.
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "mem.h"
#include "gr.h"
#include "grdef.h"
#include "error.h"
#include "mono.h"
#include "palette.h"
#include "platform.h"

// ASCENT: the video memory. It has the size of the memory of a 1 MB SVGA
// card, for each mode. It is larger than one screen, so the game can make
// sub canvases below the visible page and flip to them (gr_show_canvas),
// as it did in the Mode X and SVGA modes. The address does not change
// when the mode changes, as the address of the VGA memory did not.
#define GR_VIDEO_MEMORY_SIZE	(1024*1024)

static unsigned char gr_frame_memory[GR_VIDEO_MEMORY_SIZE];

unsigned char * gr_video_memory = gr_frame_memory;

// ASCENT: see grdef.h
int gr_display_start = 0;
int gr_refresh_hz = 70;

char gr_pal_default[768];

int gr_installed = 0;

// ASCENT: shows the visible part of the frame. See gr.h for the places
// where the game must call this function.
void gr_present(void)
{
	int w, h, rowsize, start;

	if ( grd_curscreen == NULL )
		return;

	w = grd_curscreen->sc_w;
	h = grd_curscreen->sc_h;
	rowsize = grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize;

	if ( (w <= 0) || (h <= 0) )
		return;

	// The VGA wrapped the display at the end of its memory. The port
	// shows the first page for a start address that is not valid.
	start = gr_display_start;
	if ( (start < 0) || (start + rowsize*(h-1) + w > GR_VIDEO_MEMORY_SIZE) )
		start = 0;

	plat_present( gr_video_memory + start, w, h, rowsize, gr_dac_palette );
}

// ASCENT: there is no DOS screen mode to save or to restore.
int gr_save_mode()
{
	return 0;
}

// ASCENT: the original function also set the DOS text mode again and
// restored the text. The fade to black stays.
void gr_restore_mode()
{
	gr_palette_fade_out( gr_palette, 32, 0 );
	gr_palette_set_gamma(0);
}

int gr_close()
{
	if (gr_installed==1)
	{
		gr_installed = 0;
		gr_restore_mode();
		free(grd_curscreen);
	}

	return 0;
}

// ASCENT: atexit needs a function that returns void.
static void gr_close_atexit(void)
{
	gr_close();
}

int gr_vesa_setmode( int mode )
{
	int retcode;

	retcode=gr_vesa_checkmode( mode );
	if ( retcode ) return retcode;

	return gr_vesa_setmodea( mode );
}


// ASCENT: each mode gives a linear frame of w by h pixels at the start
// of gr_video_memory. The screen canvas has the type BM_LINEAR and a row
// size of w for each mode (Mode X had w/4, the 15-bit modes had w*2).
// The mode numbers and the sizes are the same as in the original.
// The two 15-bit modes give error 4.
int gr_set_mode(int mode)
{
	int retcode;
	unsigned int w,h,t,r;
	unsigned char * data;

	//JOHNgr_disable_default_palette_loading();

	switch(mode)
	{
	case SM_ORIGINAL:
		return 0;
	case 0:
		w = 320; r = 320; h = 200;
		break;
	case SM_640x400V:
		retcode = gr_vesa_setmode( 0x100 );
		if (retcode !=0 ) return retcode;
		w = 640; r = 640; h = 400;
		break;
	case SM_640x480V:
		retcode = gr_vesa_setmode( 0x101 );
		if (retcode !=0 ) return retcode;
		w = 640; r = 640; h = 480;
		break;
	case SM_800x600V:
		retcode = gr_vesa_setmode( 0x103 );
		if (retcode !=0 ) return retcode;
		w = 800; r = 800; h = 600;
		break;
	case SM_1024x768V:
		retcode = gr_vesa_setmode( 0x105 );
		if (retcode !=0 ) return retcode;
		w = 1024; r = 1024; h = 768;
		break;
	case SM_640x480V15:
		retcode = gr_vesa_setmode( 0x110 );
		if (retcode !=0 ) return retcode;
		w = 640; r = 640*2; h=480;
		break;
	case SM_800x600V15:
		retcode = gr_vesa_setmode( 0x113 );
		if (retcode !=0 ) return retcode;
		w = 800; r = 800*2; h=600;
		break;
	case 19:
		w = 320; r = 320; h = 100;
		break;
	case 20:
		retcode = gr_vesa_setmode( 0x102 );
		//gr_enable_default_palette_loading();
		if (retcode !=0 ) return retcode;
		w = 400; r = 400; h = 600;
		break;
	case 21:
		w = 160; r = 160; h = 100;
		break;
	case 22:			// 3dmax 320x400
		w = 320; r = 320; h = 400;
		break;
	default:
		w = gr_modex_setmode( mode );
		//gr_enable_default_palette_loading();
		h = w & 0xffff; w = w >> 16; r = w;
		break;
	}

	// ASCENT: all the modes are linear frames. See the comment above.
	t = BM_LINEAR;
	data = gr_video_memory;

	// The BIOS cleared the video memory when it set a mode.
	memset( gr_video_memory, 0, GR_VIDEO_MEMORY_SIZE );
	gr_display_start = 0;

	// The VGA modes with 200 or 400 lines have 70 frames in each second.
	// The modes with 480 lines, and the larger SVGA modes, have 60.
	if ( (h==200) || (h==400) || (h==100) )
		gr_refresh_hz = 70;
	else
		gr_refresh_hz = 60;

	memset( grd_curscreen, 0, sizeof(grs_screen));
	grd_curscreen->sc_mode = mode;
	grd_curscreen->sc_w = w;
	grd_curscreen->sc_h = h;
	grd_curscreen->sc_aspect = fixdiv(grd_curscreen->sc_w*3,grd_curscreen->sc_h*4);
	grd_curscreen->sc_canvas.cv_bitmap.bm_x = 0;
	grd_curscreen->sc_canvas.cv_bitmap.bm_y = 0;
	grd_curscreen->sc_canvas.cv_bitmap.bm_w = w;
	grd_curscreen->sc_canvas.cv_bitmap.bm_h = h;
	grd_curscreen->sc_canvas.cv_bitmap.bm_rowsize = r;
	grd_curscreen->sc_canvas.cv_bitmap.bm_type = t;
	grd_curscreen->sc_canvas.cv_bitmap.bm_data = data;
	gr_set_current_canvas(NULL);

	// ASCENT: the original cleared the palette before it set the screen
	// structure. Now gr_palette_clear shows the frame, so it must come
	// after the new size is known.
	gr_palette_clear();

	//gr_enable_default_palette_loading();

	return 0;
}

int gr_init(int mode)
{
	int retcode;

	// Only do this function once!
	if (gr_installed==1)
		return 1;

	if (gr_init_A0000())
		return 10;

	// Save the current text screen mode
	if (gr_save_mode()==1)
		return 1;

	// ASCENT: the original saved the palette of the DOS screen and faded
	// that screen to black. There is no DOS screen. The palette that the
	// display shows starts as black.
	gr_palette_read( gr_pal_default );
	gr_palette_clear();

	//MALLOC( grd_curscreen,grs_screen,1 );//Hack by KRB
	grd_curscreen=(grs_screen*)malloc(1*sizeof(grs_screen));
	memset( grd_curscreen, 0, sizeof(grs_screen));

	// Set the mode.
	if ((retcode=gr_set_mode(mode)))
	{
		gr_restore_mode();
		return retcode;
	}
	//JOHNgr_disable_default_palette_loading();

	// Set all the screen, canvas, and bitmap variables that
	// aren't set by the gr_set_mode call:
	grd_curscreen->sc_canvas.cv_color = 0;
	grd_curscreen->sc_canvas.cv_drawmode = 0;
	grd_curscreen->sc_canvas.cv_font = NULL;
	grd_curscreen->sc_canvas.cv_font_fg_color = 0;
	grd_curscreen->sc_canvas.cv_font_bg_color = 0;
	gr_set_current_canvas( &grd_curscreen->sc_canvas );

	// ASCENT: no selectors for the fade table and the palette.
	gr_fade_table_selector = 0;
	gr_palette_selector = 0;

	// Set flags indicating that this is installed.
	gr_installed = 1;
	atexit(gr_close_atexit);

	return 0;
}

int gr_mode13_checkmode()
{
	// ASCENT: there is always a "VGA adapter".
	return 0;
}

//  0=Mode set OK
//  1=No VGA adapter installed
//  2=Program doesn't support this VESA granularity
//  3=Monitor doesn't support that VESA mode.:
//  4=Video card doesn't support that VESA mode.
//  5=No VESA driver found.
//  6=Bad Status after VESA call/
//  7=Not enough DOS memory to call VESA functions.
//  8=Error using DPMI.
//  9=Error setting logical line width.
// 10=Error allocating selector for A0000h
// 11=Not a valid mode support by gr.lib

int gr_check_mode(int mode)
{
	switch(mode)
	{
	case 19:
	case SM_320x200C:
	case SM_320x200U:
	case SM_320x240U:
	case SM_360x200U:
	case SM_360x240U:
	case SM_376x282U:
	case SM_320x400U:
	case SM_320x480U:
	case SM_360x400U:
	case SM_360x480U:
	case SM_360x360U:
	case SM_376x308U:
	case SM_376x564U:		return gr_mode13_checkmode();
	case SM_640x400V:		return gr_vesa_checkmode( 0x100 );
	case SM_640x480V: 	return gr_vesa_checkmode( 0x101 );
	case SM_800x600V: 	return gr_vesa_checkmode( 0x103 );
	// ASCENT: the original set the mode for the next three (gr_vesa_setmode).
	case SM_1024x768V:	return gr_vesa_checkmode( 0x105 );
	case SM_640x480V15:	return gr_vesa_checkmode( 0x110 );
	case SM_800x600V15:	return gr_vesa_checkmode( 0x113 );
	}
	return 11;
}
