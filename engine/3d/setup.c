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
 * ASCENT: this file replaces setup.asm.
 *
 * Source for setup,close,start & end frame routines
 */

#include <stdlib.h>

#include "globvars.h"
#include "clipper.h"

//from the texture mapper (texmap/ntmap.c)
extern void init_interface_vars_to_assembler(void);

//initialize the 3d system
//ASCENT: the assembly also installed the divide overflow handler here
//(div0_init). The C code checks before each divide, or saturates, and
//does not use a handler.
void g3_init(void)
{
	atexit(g3_close);
}

//close down the 3d system
void g3_close(void)
{
}

//start rendering a frame. sets up window vars
void g3_start_frame(void)
{
	fix s;

	Canvas_width = (ushort) grd_curcanv->cv_bitmap.bm_w;		//store width
	Canv_w2 = (fix) ((uint32_t) Canvas_width << 15);			//get fixed-point width/2

	Canvas_height = (ushort) grd_curcanv->cv_bitmap.bm_h;		//store height
	Canv_h2 = (fix) ((uint32_t) Canvas_height << 15);			//get fixed-point height/2

	//compute aspect ratio for this canvas

	//get pixel ratio, figure in window size
	s = g3_idiv((int64_t) grd_curscreen->sc_aspect * Canvas_height,Canvas_width);

	if (s > f1_0) {		// > 1?
		Window_scale.y = fixdiv(f1_0,s);
		Window_scale.x = f1_0;
	}
	else {
		Window_scale.x = s;
		Window_scale.y = f1_0;
	}

	Window_scale.z = f1_0;		//always 1

	init_interface_vars_to_assembler();
}

//this doesn't do anything, but is here for completeness
void g3_end_frame(void)
{
	//the assembly had a (disabled) check here: free_point_num must be 0
}
