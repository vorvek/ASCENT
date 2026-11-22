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
 * $Source: f:/miner/source/main/rcs/render.c $
 * $Revision: 2.5 $
 * $Author: john $
 * $Date: 1995/12/19 15:31:36 $
 *
 * Sample setup for RCS header
 *
 * $Log: render.c $
 * Revision 2.5  1995/12/19  15:31:36  john
 * Made stereo mode only record 1 eye in demo.
 * 
 * Revision 2.4  1995/03/20  18:15:53  john
 * Added code to not store the normals in the segment structure.
 * 
 * Revision 2.3  1995/03/13  16:11:05  john
 * Maybe fixed bug that lighting didn't work with vr helmets.
 * 
 * Revision 2.2  1995/03/09  15:33:49  john
 * Fixed bug with iglasses timeout too long, and objects
 * disappearing from left eye.
 * 
 * Revision 2.1  1995/03/06  15:23:59  john
 * New screen techniques.
 * 
 * Revision 2.0  1995/02/27  11:31:01  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.252  1995/02/22  13:49:38  allender
 * remove anonymous unions from object structure
 * 
 * Revision 1.251  1995/02/11  15:07:26  matt
 * Took out code which was mostly intended as part of a larger renderer
 * change which never happened.  This new code was causing problems with
 * the level 4 control center.
 * 
 * Revision 1.250  1995/02/07  16:28:53  matt
 * Fixed problem with new code
 * 
 * Revision 1.249  1995/02/06  14:38:58  matt
 * Took out some code that didn't compile when editor in
 * 
 * Revision 1.248  1995/02/06  13:45:25  matt
 * Structural changes, plus small sorting improvements
 * 
 * Revision 1.247  1995/02/02  15:59:26  matt
 * Changed assert to int3.
 * 
 * Revision 1.246  1995/02/01  21:02:27  matt
 * Added partial fix for rendering bugs
 * Ripped out laser hack system
 * 
 * Revision 1.245  1995/01/20  15:14:30  matt
 * Added parens to fix precedence bug
 * 
 * Revision 1.244  1995/01/14  19:16:59  john
 * First version of new bitmap paging code.
 * 
 * Revision 1.243  1995/01/03  20:19:25  john
 * Pretty good working version of game save.
 * 
 * Revision 1.242  1994/12/29  13:51:05  john
 * Made the floating reticle draw in the spot
 * regardless of the eye offset.
 * 
 * Revision 1.241  1994/12/23  15:02:55  john
 * Tweaked floating reticle.
 * 
 * Revision 1.240  1994/12/23  14:27:45  john
 * Changed offset of floating reticle to line up with
 * lasers a bit better.
 * 
 * Revision 1.239  1994/12/23  14:22:50  john
 * Added floating reticle for VR helments.  
 * 
 * Revision 1.238  1994/12/13  14:07:50  matt
 * Fixed tmap_num2 bug in search mode
 * 
 * Revision 1.237  1994/12/11  00:45:53  matt
 * Fixed problem when object sort buffer got full
 * 
 * Revision 1.236  1994/12/09  18:46:06  matt
 * Added a little debugging
 * 
 * Revision 1.235  1994/12/09  14:59:16  matt
 * Added system to attach a fireball to another object for rendering purposes,
 * so the fireball always renders on top of (after) the object.
 * 
 * Revision 1.234  1994/12/08  15:46:54  matt
 * Fixed buffer overflow that caused seg depth screwup
 * 
 * Revision 1.233  1994/12/08  11:51:53  matt
 * Took out some unused stuff
 * 
 * Revision 1.232  1994/12/06  16:31:48  mike
 * fix detriangulation problems.
 * 
 * Revision 1.231  1994/12/05  15:32:51  matt
 * Changed an assert to an int3 & return
 * 
 * Revision 1.230  1994/12/04  17:28:04  matt
 * Got rid of unused no_render_flag array, and took out box clear when searching
 * 
 * Revision 1.229  1994/12/04  15:51:14  matt
 * Fixed linear tmap transition for objects
 * 
 * Revision 1.228  1994/12/03  20:16:50  matt
 * Turn off window clip for objects
 * 
 * Revision 1.227  1994/12/03  14:48:00  matt
 * Restored some default settings
 * 
 * Revision 1.226  1994/12/03  14:44:32  matt
 * Fixed another difficult bug in the window clip system
 * 
 * Revision 1.225  1994/12/02  13:19:56  matt
 * Fixed rect clears at terminus of rendering
 * Made a bunch of debug code compile out
 * 
 * Revision 1.224  1994/12/02  11:58:21  matt
 * Fixed window clip bug
 * 
 * Revision 1.223  1994/11/28  21:50:42  mike
 * optimizations.
 * 
 * Revision 1.222  1994/11/28  01:32:15  mike
 * turn off window clearing.
 * 
 * Revision 1.221  1994/11/27  23:11:52  matt
 * Made changes for new mprintf calling convention
 * 
 * Revision 1.220  1994/11/20  15:58:55  matt
 * Don't migrate the control center, since it doesn't move out of its segment
 * 
 * Revision 1.219  1994/11/19  23:54:36  mike
 * change window colors.
 * 
 * Revision 1.218  1994/11/19  15:20:25  mike
 * rip out unused code and data
 * 
 * Revision 1.217  1994/11/18  13:21:24  mike
 * Clear only view portals into rest of world based on value of Clear_window.
 * 
 * Revision 1.216  1994/11/15  17:02:10  matt
 * Re-added accidentally deleted variable
 * 
 * Revision 1.215  1994/11/15  16:51:50  matt
 * Made rear view only switch to rear cockpit if cockpit on in front view
 * 
 * Revision 1.214  1994/11/14  20:47:57  john
 * Attempted to strip out all the code in the game 
 * directory that uses any ui code.
 * 
 * Revision 1.213  1994/11/11  15:37:07  mike
 * write orange for background to show render bugs.
 * 
 * Revision 1.212  1994/11/09  22:57:18  matt
 * Keep tract of depth of segments rendered, for detail level optimization
 * 
 * Revision 1.211  1994/11/01  23:40:14  matt
 * Elegantly handler buffer getting full
 * 
 * Revision 1.210  1994/10/31  22:28:13  mike
 * Fix detriangulation bug.
 * 
 * Revision 1.209  1994/10/31  11:48:56  mike
 * Optimize detriangulation, speedup of about 4% in many cases, 0% in many.
 * 
 * Revision 1.208  1994/10/30  20:08:34  matt
 * For endlevel: added big explosion at tunnel exit; made lights in tunnel 
 * go out; made more explosions on walls.
 * 
 * Revision 1.207  1994/10/27  14:14:35  matt
 * Don't do light flash during endlevel sequence
 * 
 * Revision 1.206  1994/10/11  12:05:42  mike
 * Improve detriangulation.
 * 
 * Revision 1.205  1994/10/07  15:27:00  john
 * Commented out the code that moves your eye
 * forward.
 * 
 * Revision 1.204  1994/10/05  16:07:38  mike
 * Don't detriangulate sides if in player's segment.  Prevents player going behind a wall,
 * though there are cases in which it would be ok to detriangulate these.
 * 
 * Revision 1.203  1994/10/03  12:44:05  matt
 * Took out unreferenced code
 * 
 * Revision 1.202  1994/09/28  14:08:45  john
 * Added Zoom stuff back in, but ifdef'd it out.
 * 
 * Revision 1.201  1994/09/25  23:41:49  matt
 * Changed the object load & save code to read/write the structure fields one
 * at a time (rather than the whole structure at once).  This mean that the
 * object structure can be changed without breaking the load/save functions.
 * As a result of this change, the local_object data can be and has been 
 * incorporated into the object array.  Also, timeleft is now a property 
 * of all objects, and the object structure has been otherwise cleaned up.
 * 
 * Revision 1.200  1994/09/25  15:50:10  mike
 * Integrate my debug changes which shows how many textures were rendered
 * this frame.
 * 
 * Revision 1.199  1994/09/25  15:45:22  matt
 * Added OBJ_LIGHT, a type of object that casts light
 * Added generalized lifeleft, and moved it to local_object
 * 
 * Revision 1.198  1994/09/15  21:23:32  matt
 * Changed system to keep track of whether & what cockpit is up
 * 
 * Revision 1.197  1994/09/15  16:30:12  mike
 * Comment out call to object_render_targets, which did nothing.
 * 
 * Revision 1.196  1994/09/07  22:25:51  matt
 * Don't migrate through semi-transparent walls
 * 
 * Revision 1.195  1994/09/07  19:16:21  mike
 * Homing missile.
 * 
 * Revision 1.194  1994/08/31  20:54:17  matt
 * Don't do flash effect while whiting out
 * 
 * Revision 1.193  1994/08/23  17:20:12  john
 * Added rear-view cockpit.
 * 
 * Revision 1.192  1994/08/22  14:36:35  john
 * Made R key make a "reverse" view render.
 * 
 * Revision 1.191  1994/08/19  20:09:26  matt
 * Added end-of-level cut scene with external scene
 * 
 * Revision 1.190  1994/08/10  19:56:17  john
 * Changed font stuff; Took out old menu; messed up lots of
 * other stuff like game sequencing messages, etc.
 * 
 * Revision 1.189  1994/08/10  14:45:05  john
 * *** empty log message ***
 * 
 * Revision 1.188  1994/08/09  16:04:06  john
 * Added network players to editor.
 * 
 * Revision 1.187  1994/08/05  17:07:05  john
 * Made lasers be two objects, one drawing after the other
 * all the time.
 * 
 * Revision 1.186  1994/08/05  10:07:57  matt
 * Disable window check checking (i.e., always use window check)
 * 
 * Revision 1.185  1994/08/04  19:11:30  matt
 * Changed a bunch of vecmat calls to use multiple-function routines, and to
 * allow the use of C macros for some functions
 * 
 * Revision 1.184  1994/08/04  00:21:14  matt
 * Cleaned up fvi & physics error handling; put in code to make sure objects
 * are in correct segment; simplified segment finding for objects and points
 * 
 * Revision 1.183  1994/08/02  19:04:28  matt
 * Cleaned up vertex list functions
 * 
 * Revision 1.182  1994/07/29  15:13:33  matt
 * When window check turned off, cut render depth in half
 * 
 * Revision 1.181  1994/07/29  11:03:50  matt
 * Use highest_segment_index instead of num_segments so render works from
 * the editor
 * 
 * Revision 1.180  1994/07/29  10:04:34  mike
 * Update Cursegp when an object is selected.
 * 
 * Revision 1.179  1994/07/25  00:02:50  matt
 * Various changes to accomodate new 3d, which no longer takes point numbers
 * as parms, and now only takes pointers to points.
 * 
 * Revision 1.178  1994/07/24  14:37:49  matt
 * Added angles for player head
 * 
 * Revision 1.177  1994/07/20  19:08:07  matt
 * If in editor, don't move eye from center of viewer object
 * 
 * 
 */

#pragma off (unreferenced)
static char rcsid[] = "$Id: render.c 2.5 1995/12/19 15:31:36 john Exp $";
#pragma on (unreferenced)

#include <stdlib.h>
#include <string.h>

// ASCENT: the Watcom stdlib.h supplied min() and max(). The macros evaluate
// an argument two times, as the Watcom macros did.
#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

#include "inferno.h"
#include "segment.h"
#include "error.h"
#include "bm.h"
#include "texmap.h"
#include "mono.h"
#include "render.h"
#include "game.h"
#include "object.h"
#include "laser.h"
#include "textures.h"
#include "screens.h"
#include "segpoint.h"
#include "wall.h"
#include "texmerge.h"
#include "physics.h"
#include "3d.h"
#include "gameseg.h"
#include "vclip.h"
#include "lighting.h"
#include	"fuelcen.h"
#include "newdemo.h"
#include "automap.h"
#include "endlevel.h"
#include "key.h"
#include "newmenu.h"
#include "mem.h"
#include "piggy.h"
#include "dtugame.h"		// ASCENT: the flat color of a side with no texture, Dtu_objects_enabled
#include "dtucombat.h"	// ASCENT: dtu_combat_draw_view()
#include "dtuopts.h"		// ASCENT: the GPU renderer settings
#include "dtuinfra.h"		// ASCENT: infravision
#include "dtuobj.h"		// ASCENT: the tint of Detect Magic
#include "dtuspell.h"		// ASCENT: DTU_FX_DETECT_MAGIC
#include "dtudisp.h"		// ASCENT: Dtu_hires, Dtu_display_active
#include "palette.h"		// ASCENT: gr_palette for the GPU renderer
#include "platform.h"		// ASCENT: plat_gpu_bind_settings
#include "args.h"			// ASCENT: -lightlog
#include "tmapfx.h"		// ASCENT: Tmap_fx_row
#include "dtudlg.h"		// ASCENT: the dark view of the conversation window
#include "effects.h"		// ASCENT: the preload of the upscaled textures
#include "dtutrig.h"
#include "gamemine.h"
#include "gameseq.h"
#include <stdio.h>
#include <ctype.h>

#define	INITIAL_LOCAL_LIGHT	(F1_0/4)		// local light value in segment of occurence (of light emission)

#ifdef EDITOR
#include "editor\editor.h"
#endif

//used for checking if points have been rotated
int	Clear_window_color=-1;
int	Clear_window=2;			//	1 = Clear whole background window, 2 = clear view portals into rest of world, 0 = no clear

int RL_framecount=-1;
short Rotated_last[MAX_VERTICES];

// When any render function needs to know what's looking at it, it should 
// access Viewer members.
object * Viewer = NULL;

vms_vector Viewer_eye;	//valid during render

int	N_render_segs;

fix Render_zoom = 0x9000;							//the player's zoom factor

#ifndef NDEBUG
ubyte object_rendered[MAX_OBJECTS];
#endif

#define DEFAULT_RENDER_DEPTH 16
int Render_depth=DEFAULT_RENDER_DEPTH;			//how many segments deep to render

int	Detriangulation_on = 1;					// 1 = allow rendering of triangulated side as a quad, 0 = don't allow

#ifdef EDITOR
int	Render_only_bottom=0;
int	Bottom_bitmap_num = 9;
#endif

fix	Face_reflectivity = (F1_0/2);

#if 0		//this stuff could probably just be deleted

int inc_render_depth(void)
{
	return ++Render_depth;
}

int dec_render_depth(void)
{
	return Render_depth==1?Render_depth:--Render_depth;
}

int reset_render_depth(void)
{
	return Render_depth = DEFAULT_RENDER_DEPTH;
}

#endif

#ifdef EDITOR
int _search_mode = 0;			//true if looking for curseg,side,face
short _search_x,_search_y;	//pixel we're looking at
int found_seg,found_side,found_face,found_poly;
#else
#define _search_mode 0
#endif

#ifdef NDEBUG		//if no debug code, set these vars to constants

#define Outline_mode 0
#define Show_only_curside 0

#else

int Outline_mode=0,Show_only_curside=0;

int toggle_outline_mode(void)
{
	return Outline_mode = !Outline_mode;
}

int toggle_show_only_curside(void)
{
	return Show_only_curside = !Show_only_curside;
}

draw_outline(int nverts,g3s_point **pointlist)
{
	int i;

	gr_setcolor(BM_XRGB(63,63,63));

	for (i=0;i<nverts-1;i++)
		g3_draw_line(pointlist[i],pointlist[i+1]);

	g3_draw_line(pointlist[i],pointlist[0]);

}
#endif

grs_canvas * reticle_canvas = NULL;

void free_reticle_canvas()
{
	if (reticle_canvas)	{
		free( reticle_canvas->cv_bitmap.bm_data );
		free( reticle_canvas );
		reticle_canvas	= NULL;
	}
}

extern void show_reticle(int force_big);

// Draw the reticle in 3D for head tracking
void draw_3d_reticle(fix eye_offset)
{
	g3s_point 	reticle_points[4];
	g3s_uvl		uvl[4];
	g3s_point	*pointlist[4];
	int 			i;
	vms_vector v1, v2;
	grs_canvas *saved_canvas;
	int saved_interp_method;

//	if (!Use_player_head_angles) return;
	
	for (i=0; i<4; i++ )	{
		pointlist[i] = &reticle_points[i];
		uvl[i].l = MAX_LIGHT;
	}
	uvl[0].u = 0; uvl[0].v = 0;
	uvl[1].u = F1_0; uvl[1].v = 0;
	uvl[2].u = F1_0; uvl[2].v = F1_0;
	uvl[3].u = 0; uvl[3].v = F1_0;

	vm_vec_scale_add( &v1, &Viewer->pos, &Viewer->orient.fvec, F1_0*4 );
	vm_vec_scale_add2(&v1,&Viewer->orient.rvec,eye_offset);

	vm_vec_scale_add( &v2, &v1, &Viewer->orient.rvec, -F1_0*1 );
	vm_vec_scale_add2( &v2, &Viewer->orient.uvec, F1_0*1 );
	g3_rotate_point(&reticle_points[0],&v2);

	vm_vec_scale_add( &v2, &v1, &Viewer->orient.rvec, +F1_0*1 );
	vm_vec_scale_add2( &v2, &Viewer->orient.uvec, F1_0*1 );
	g3_rotate_point(&reticle_points[1],&v2);

	vm_vec_scale_add( &v2, &v1, &Viewer->orient.rvec, +F1_0*1 );
	vm_vec_scale_add2( &v2, &Viewer->orient.uvec, -F1_0*1 );
	g3_rotate_point(&reticle_points[2],&v2);

	vm_vec_scale_add( &v2, &v1, &Viewer->orient.rvec, -F1_0*1 );
	vm_vec_scale_add2( &v2, &Viewer->orient.uvec, -F1_0*1 );
	g3_rotate_point(&reticle_points[3],&v2);

	if ( reticle_canvas == NULL )	{
		reticle_canvas = gr_create_canvas(64,64);
		if ( !reticle_canvas )
			Error( "Couldn't malloc reticle_canvas" );
		atexit( free_reticle_canvas );
		reticle_canvas->cv_bitmap.bm_selector = 0;
		reticle_canvas->cv_bitmap.bm_flags = BM_FLAG_TRANSPARENT;
	}

	saved_canvas = grd_curcanv;
	gr_set_current_canvas(reticle_canvas);
	gr_clear_canvas( 255 );		// Clear to Xparent
	show_reticle(1);
	gr_set_current_canvas(saved_canvas);
	
	saved_interp_method=Interpolation_method;
	Interpolation_method	= 3;		// The best, albiet slowest.
	g3_draw_tmap(4,pointlist,uvl,&reticle_canvas->cv_bitmap);
	Interpolation_method	= saved_interp_method;
}


fix flash_scale;

#define FLASH_CYCLE_RATE f1_0

fix flash_rate = FLASH_CYCLE_RATE;

//cycle the flashing light for when mine destroyed
flash_frame()
{
	static fixang flash_ang=0;

	if (!Fuelcen_control_center_destroyed)
		return;

	if (Endlevel_sequence)
		return;

	if (PaletteBlueAdd > 10 )		//whiting out
		return;

//	flash_ang += fixmul(FLASH_CYCLE_RATE,FrameTime);
	flash_ang += fixmul(flash_rate,FrameTime);

	fix_fastsincos(flash_ang,&flash_scale,NULL);

	flash_scale = (flash_scale + f1_0)/2;

}

// -----------------------------------------------------------------------------------
// ASCENT: test option -lightlog FILE. For each frame one line that compares
// the light of each drawn corner (segment, side, corner, triangle) with the
// light of the same corner in the frame before: the number of corners drawn
// in both frames, how many changed, the largest and the mean change, for the
// total light and for its parts (headlight, dynamic light), and the mean
// light. A change of 0x800 is one of the 32 light levels of the fade table.
#define LL_N	(MAX_SEGMENTS*6*4*2)
typedef struct { fix total[2], head[2], dyn[2]; int stamp[2]; fix ref; int has_ref; } ll_entry;
static ll_entry *Ll;
static FILE *Light_log;
static int Ll_frame = 1;

static void light_log_record(int segnum, int sidenum, int nv, short *vp, vms_vector *norm, int i, fix total, fix head, fix dyn)
{
	int c, tri, k, s;
	ll_entry *e;

	if (!Light_log) return;
	for (c = 0; c < 4; c++)
		if (Segments[segnum].verts[Side_to_verts[sidenum][c]] == vp[i]) break;
	if (c == 4) return;
	tri = nv == 3 && !memcmp(norm, &Segments[segnum].sides[sidenum].normals[1], sizeof(*norm))
		&& memcmp(norm, &Segments[segnum].sides[sidenum].normals[0], sizeof(*norm));
	k = ((segnum * 6 + sidenum) * 4 + c) * 2 + tri;
	e = &Ll[k];
	s = Ll_frame & 1;
	e->total[s] = total; e->head[s] = head; e->dyn[s] = dyn; e->stamp[s] = Ll_frame;
	if (!e->has_ref) { e->ref = total; e->has_ref = 1; }
}

static void light_log_frame(void)
{
	static int init = 0;
	int k, s, n = 0, both = 0, ch[3] = {0,0,0}, vis[3] = {0,0,0};
	fix maxd[3] = {0,0,0};
	double sumd[3] = {0,0,0}, sum = 0;
	int drift = 0; fix maxdrift = 0;

	if (!init) {
		int t;
		init = 1;
		if ((t = FindArg("-lightlog")) != 0 && t+1 < Num_args) {
			Light_log = fopen(Args[t+1], "w");
			if (Light_log) Ll = calloc(LL_N, sizeof(ll_entry));
			if (!Ll && Light_log) { fclose(Light_log); Light_log = NULL; }
		}
		return;
	}
	if (!Light_log) return;
	s = Ll_frame & 1;
	for (k = 0; k < LL_N; k++) {
		ll_entry *e = &Ll[k];
		if (e->stamp[s] != Ll_frame) continue;
		n++;
		sum += e->total[s];
		if (abs(e->total[s] - e->ref) >= 0x800) drift++;
		if (abs(e->total[s] - e->ref) > maxdrift) maxdrift = abs(e->total[s] - e->ref);
		if (e->stamp[!s] == Ll_frame - 1) {
			fix d[3];
			int j;
			both++;
			d[0] = abs(e->total[s] - e->total[!s]);
			d[1] = abs(e->head[s] - e->head[!s]);
			d[2] = abs(e->dyn[s] - e->dyn[!s]);
			for (j = 0; j < 3; j++) {
				if (d[j] >= 0x100) ch[j]++;
				if (d[j] >= 0x800) vis[j]++;
				if (d[j] > maxd[j]) maxd[j] = d[j];
				sumd[j] += d[j];
			}
		}
	}
	fprintf(Light_log, "frame %d drawn %d both %d mean %.4f drift %d maxdrift %.4f | total ch %d vis %d max %.4f mean %.5f | head ch %d vis %d max %.4f | dyn ch %d vis %d max %.4f | pos %.2f %.2f %.2f fvec %.3f %.3f\n",
		Ll_frame, n, both, n ? sum / n / 65536.0 : 0.0, drift, maxdrift / 65536.0,
		ch[0], vis[0], maxd[0] / 65536.0, both ? sumd[0] / both / 65536.0 : 0.0,
		ch[1], vis[1], maxd[1] / 65536.0, ch[2], vis[2], maxd[2] / 65536.0,
		f2fl(Viewer->pos.x), f2fl(Viewer->pos.y), f2fl(Viewer->pos.z),
		f2fl(Viewer->orient.fvec.x), f2fl(Viewer->orient.fvec.z));
	fflush(Light_log);
	Ll_frame++;
}

// -----------------------------------------------------------------------------------
//	Render a face.
//	It would be nice to not have to pass in segnum and sidenum, but they are used for our
//	hideously hacked in headlight system.
//	vp is a pointer to vertex ids.
//	tmap1, tmap2 are texture map ids.  tmap2 is the pasty one.
void render_face(int segnum, int sidenum, int nv, short *vp, int tmap1, int tmap2, uvl *uvlp, vms_vector *norm)
{
	fix			face_light;
	grs_bitmap	*bm;
	fix			reflect;
	uvl			uvl_copy[8];
	int			i;
	g3s_point	*pointlist[8];

	Assert(nv <= 8);

	for (i=0; i<nv; i++) {
		uvl_copy[i] = uvlp[i];
		pointlist[i] = &Segment_points[vp[i]];
	}

	face_light = -vm_vec_dot(&Viewer->orient.fvec,norm);

	// ASCENT: a side whose texture the game does not have (the bit in
	// dtu_tmap_missing, see level_translate_textures) or whose bitmap is not
	// loaded is drawn with a flat color, so that the level geometry shows
	// in any case. The color depends on the texture number of the level
	// file and on the light of the side.
	if ((Segments[segnum].dtu_tmap_missing & (1 << sidenum)) || !dtu_texture_ready(tmap1)) {
		fix light = 0;

		for (i=0; i<nv; i++)
			light += uvlp[i].l;
		gr_setcolor(dtu_flat_color(Segments[segnum].dtu_tmap_num[sidenum], light / nv));
		g3_draw_poly(nv,pointlist);
		return;
	}

	if (tmap1 >= NumTextures) {
		mprintf((0,"Invalid tmap number %d, NumTextures=%d, changing to 0\n",tmap1,NumTextures));
		Int3();
		Segments[segnum].sides[sidenum].tmap_num = 0;
	}

	// New code for overlapping textures...
	if (tmap2 != 0)
		bm = texmerge_get_cached_bitmap( tmap1, tmap2 );
	else	{
		bm = &GameBitmaps[Textures[tmap1].index];
		PIGGY_PAGE_IN(Textures[tmap1]);
	}

	Assert( !(bm->bm_flags & BM_FLAG_PAGED_OUT) );

	//reflect = fl2f((1.0-TmapInfo[p->tmap_num].reflect)/2.0 + 0.5);
	//reflect = fl2f((1.0-TmapInfo[p->tmap_num].reflect));

	reflect = Face_reflectivity;		// f1_0;	//until we figure this stuff out...

	//set light values for each vertex & build pointlist
	{
		int i;

		face_light = fixmul(face_light,reflect);

		for (i=0;i<nv;i++) {

			//the uvl struct has static light already in it

			//scale static light for destruction effect
			if (Fuelcen_control_center_destroyed)	//make lights flash
				uvl_copy[i].l = fixmul(flash_scale,uvl_copy[i].l);

			{
				fix head, vertex_face_light = face_light;

				//add in dynamic light (from explosions, etc.)
				uvl_copy[i].l += Dynamic_light[vp[i]];

				// ASCENT: Dtu_fix_light_stable (lighting.h). The original
				// scales the player light by the angle between the view
				// direction and the face, so a wall got lighter when the
				// player turned to it. Here the angle is between the face
				// and the direction from the corner to the eye: a light that
				// the player carries. At the center of the view the original
				// gives the full angle term; over all the view directions in
				// which a corner is drawn it gives less. One half of the angle
				// term keeps the mean light of the views (turns in place in
				// YPINN, BANE, DROW1: -0.3% to +2%).
				if (Dtu_fix_light_stable) {
					vms_vector to_eye;

					vm_vec_normalized_dir(&to_eye, &Viewer_eye, &Vertices[vp[i]]);
					vertex_face_light = fixmul(vm_vec_dot(&to_eye, norm), reflect) / 2;
				}

				//add in light from player's headlight
				head = compute_headlight_light(&Segment_points[vp[i]].p3_vec,vertex_face_light);
				uvl_copy[i].l += head;

				//saturate at max value
				if (uvl_copy[i].l > MAX_LIGHT)
					uvl_copy[i].l = MAX_LIGHT;

				light_log_record(segnum, sidenum, nv, vp, norm, i, uvl_copy[i].l, head, Dynamic_light[vp[i]]);
			}

		}
	}


#ifdef EDITOR
	if ((Render_only_bottom) && (sidenum == WBOTTOM))
		g3_draw_tmap(nv,pointlist,(g3s_uvl *) uvl_copy,&GameBitmaps[Textures[Bottom_bitmap_num].index]);
	else
#endif
	{
		// ASCENT: the heat of the face for the improved infravision
		if (Dtu_infra_heat)
			G3_gpu_heat = dtu_infra_face_heat(tmap1, tmap2);
		g3_draw_tmap(nv,pointlist,(g3s_uvl *) uvl_copy,bm);
		G3_gpu_heat = 0;
	}

	#ifndef NDEBUG
	if (Outline_mode) draw_outline(nv, pointlist);
	#endif
}

#ifdef EDITOR
// -----------------------------------------------------------------------------------
//	Only called if editor active.
//	Used to determine which face was clicked on.
void check_face(int segnum, int sidenum, int facenum, int nv, short *vp, int tmap1, int tmap2, uvl *uvlp)
{
	int	i;

	if (_search_mode) {
		int save_lighting;
		grs_bitmap *bm;
		uvl uvl_copy[8];
		g3s_point *pointlist[4];

		if (tmap2 > 0 )
			bm = texmerge_get_cached_bitmap( tmap1, tmap2 );
		else
			bm = &GameBitmaps[Textures[tmap1].index];

		for (i=0; i<nv; i++) {
			uvl_copy[i] = uvlp[i];
			pointlist[i] = &Segment_points[vp[i]];
		}

		gr_setcolor(0);
		gr_pixel(_search_x,_search_y);	//set our search pixel to color zero
		gr_setcolor(1);					//and render in color one
 save_lighting = Lighting_on;
 Lighting_on = 2;
		//g3_draw_poly(nv,vp);
		g3_draw_tmap(nv,pointlist, uvl_copy, bm);
 Lighting_on = save_lighting;

		if (gr_ugpixel(&grd_curcanv->cv_bitmap,_search_x,_search_y) == 1) {
			found_seg = segnum;
			found_side = sidenum;
			found_face = facenum;
		}
		
	}
}
#endif

fix	Tulate_min_dot = (F1_0/4);
//--unused-- fix	Tulate_min_ratio = (2*F1_0);
fix	Min_n0_n1_dot	= (F1_0*15/16);

// -----------------------------------------------------------------------------------
//	Render a side.
//	Check for normal facing.  If so, render faces on side dictated by sidep->type.
// ASCENT: Detect Magic on a side (0x75707 to 0x75827): with the effect bit
// 0x40 of the player object, the tint 0x40 for a side with bit 0x20 of the
// side byte 1 (pad; no level file has it), a side of a segment of special
// 2, 3 or 6, or an illusion wall that is on (wall type 3, no flag 0x20)
// whose child segment connects back and that is an illusory wall
// (WALL_IS_DOORWAY 3). The tint of the faces of the side; the value of
// before is given back after the side (0x75C1E).
static int side_shows_magic(segment *segp, int sidenum)
{
	side *sidep = &segp->sides[sidenum];
	int w = sidep->wall_num, child = segp->children[sidenum];

	if (ConsoleObject == NULL || !(ConsoleObject->dtu.m_88 & DTU_FX_DETECT_MAGIC))
		return 0;
	if ((sidep->pad & 0x20) || segp->special == 6 || segp->special == 2 || segp->special == 3)
		return 1;
	if (w < 0 || w >= Num_walls || Walls[w].type != WALL_ILLUSION || (Walls[w].flags & WALL_ILLUSION_OFF))
		return 0;
	if (child <= -1 || child > Highest_segment_index || find_connect_side(segp, &Segments[child]) == -1)
		return 0;
	return WALL_IS_DOORWAY(segp,sidenum) == WID_ILLUSORY_WALL;
}

static void render_side_faces(segment *segp, int sidenum);

void render_side(segment *segp, int sidenum)
{
	int tint;

	if (!(WALL_IS_DOORWAY(segp,sidenum) & WID_RENDER_FLAG))
		return;
	tint = dtu_get_tint();
	if (side_shows_magic(segp,sidenum))
		dtu_set_tint(0x40);
	render_side_faces(segp,sidenum);
	dtu_set_tint(tint);
}

static void render_side_faces(segment *segp, int sidenum)
{
	short			vertnum_list[4];
	side			*sidep = &segp->sides[sidenum];
	vms_vector	tvec;
	fix			v_dot_n0, v_dot_n1;
	uvl			temp_uvls[3];
	fix			min_dot, max_dot;
	vms_vector  normals[2];

	if (!(WALL_IS_DOORWAY(segp,sidenum) & WID_RENDER_FLAG))		//if (WALL_IS_DOORWAY(segp, sidenum) == WID_NO_WALL)
		return;

	#ifdef COMPACT_SEGS	
		get_side_normals(segp, sidenum, &normals[0], &normals[1] );
	#else
		normals[0] = segp->sides[sidenum].normals[0];
		normals[1] = segp->sides[sidenum].normals[1];
	#endif

	//	Regardless of whether this side is comprised of a single quad, or two triangles, we need to know one normal, so
	//	deal with it, get the dot product.
	if (sidep->type == SIDE_IS_TRI_13)
		vm_vec_normalized_dir(&tvec, &Viewer_eye, &Vertices[segp->verts[Side_to_verts[sidenum][1]]]);
	else
		vm_vec_normalized_dir(&tvec, &Viewer_eye, &Vertices[segp->verts[Side_to_verts[sidenum][0]]]);

	get_side_verts(vertnum_list,segp-Segments,sidenum);

	v_dot_n0 = vm_vec_dot(&tvec, &normals[0]);

	if (sidep->type == SIDE_IS_QUAD) {
		if (v_dot_n0 >= 0) {
			render_face(segp-Segments, sidenum, 4, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls, &normals[0]);
			#ifdef EDITOR
			check_face(segp-Segments, sidenum, 0, 4, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
			#endif
		}
	} else {
		//	Although this side has been triangulated, because it is not planar, see if it is acceptable
		//	to render it as a single quadrilateral.  This is a function of how far away the viewer is, how non-planar
		//	the face is, how normal to the surfaces the view is.
		//	Now, if both dot products are close to 1.0, then render two triangles as a single quad.
		v_dot_n1 = vm_vec_dot(&tvec, &normals[1]);

		if (v_dot_n0 < v_dot_n1) {
			min_dot = v_dot_n0;
			max_dot = v_dot_n1;
		} else {
			min_dot = v_dot_n1;
			max_dot = v_dot_n0;
		}

		//	Determine whether to detriangulate side: (speed hack, assumes Tulate_min_ratio == F1_0*2, should fixmul(min_dot, Tulate_min_ratio))
		if (Detriangulation_on && ((min_dot+F1_0/256 > max_dot) || ((Viewer->segnum != segp-Segments) &&  (min_dot > Tulate_min_dot) && (max_dot < min_dot*2)))) {
			fix	n0_dot_n1;

			//	The other detriangulation code doesn't deal well with badly non-planar sides.
			n0_dot_n1 = vm_vec_dot(&normals[0], &normals[1]);
			if (n0_dot_n1 < Min_n0_n1_dot)
				goto im_so_ashamed;

			render_face(segp-Segments, sidenum, 4, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls, &normals[0]);
			#ifdef EDITOR
			check_face(segp-Segments, sidenum, 0, 4, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
			#endif
		} else {
im_so_ashamed: ;
			if (sidep->type == SIDE_IS_TRI_02) {
				if (v_dot_n0 >= 0) {
					render_face(segp-Segments, sidenum, 3, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls, &normals[0]);
					#ifdef EDITOR
					check_face(segp-Segments, sidenum, 0, 3, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
					#endif
				}

				if (v_dot_n1 >= 0) {
					temp_uvls[0] = sidep->uvls[0];		temp_uvls[1] = sidep->uvls[2];		temp_uvls[2] = sidep->uvls[3];
					vertnum_list[1] = vertnum_list[2];	vertnum_list[2] = vertnum_list[3];	// want to render from vertices 0, 2, 3 on side
					render_face(segp-Segments, sidenum, 3, &vertnum_list[0], sidep->tmap_num, sidep->tmap_num2, temp_uvls, &normals[1]);
					#ifdef EDITOR
					check_face(segp-Segments, sidenum, 1, 3, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
					#endif
				}
			} else if (sidep->type ==  SIDE_IS_TRI_13) {
				if (v_dot_n1 >= 0) {
					render_face(segp-Segments, sidenum, 3, &vertnum_list[1], sidep->tmap_num, sidep->tmap_num2, &sidep->uvls[1], &normals[1]);	// rendering 1,2,3, so just skip 0
					#ifdef EDITOR
					check_face(segp-Segments, sidenum, 1, 3, &vertnum_list[1], sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
					#endif
				}

				if (v_dot_n0 >= 0) {
					temp_uvls[0] = sidep->uvls[0];		temp_uvls[1] = sidep->uvls[1];		temp_uvls[2] = sidep->uvls[3];
					vertnum_list[2] = vertnum_list[3];		// want to render from vertices 0,1,3
					render_face(segp-Segments, sidenum, 3, vertnum_list, sidep->tmap_num, sidep->tmap_num2, temp_uvls, &normals[0]);
					#ifdef EDITOR
					check_face(segp-Segments, sidenum, 0, 3, vertnum_list, sidep->tmap_num, sidep->tmap_num2, sidep->uvls);
					#endif
				}

			} else
				Error("Illegal side type in render_side, type = %i, segment # = %i, side # = %i\n", sidep->type, segp-Segments, sidenum);
		}
	}

}

#ifdef EDITOR
render_object_search(object *obj)
{
	int changed=0;

	//note that we draw each pixel object twice, since we cannot control
	//what color the object draws in, so we try color 0, then color 1,
	//in case the object itself is rendering color 0

	gr_setcolor(0);
	gr_pixel(_search_x,_search_y);	//set our search pixel to color zero
	render_object(obj);
	if (gr_ugpixel(&grd_curcanv->cv_bitmap,_search_x,_search_y) != 0)
		changed=1;

	gr_setcolor(1);
	gr_pixel(_search_x,_search_y);	//set our search pixel to color zero
	render_object(obj);
	if (gr_ugpixel(&grd_curcanv->cv_bitmap,_search_x,_search_y) != 1)
		changed=1;

	if (changed) {
		if (obj->segnum != -1)
			Cursegp = &Segments[obj->segnum];
		found_seg = -(obj-Objects+1);
	}
}
#endif

do_render_object(int objnum)
{
	#ifdef EDITOR
	int save_3d_outline;
	#endif
	object *obj = &Objects[objnum];
	int count = 0;
	int n;

	// ASCENT: no object is drawn while the tables of the monsters and
	// items are not loaded, see Dtu_objects_enabled in dtugame.h
	if (!Dtu_objects_enabled)
		return;

	Assert(objnum < MAX_OBJECTS);

	#ifndef NDEBUG
	if (object_rendered[objnum]) {		//already rendered this...
		Int3();		//get Matt!!!
		return;
	}
	object_rendered[objnum] = 1;
	#endif

	//	Added by MK on 09/07/94 (at about 5:28 pm, CDT, on a beautiful, sunny late summer day!) so
	//	that the guided missile system will know what objects to look at.
	// ASCENT: only for an object that the original depth draws (see
	// Render_orig_visible)
	if (Render_orig_visible && ((Objects[objnum].type == OBJ_ROBOT) || (Objects[objnum].type == OBJ_PLAYER))) {
		//Assert(Num_rendered_objects < MAX_RENDERED_OBJECTS);
		//	This peculiar piece of code makes us keep track of the most recently rendered objects, which
		//	are probably the higher priority objects, without overflowing the buffer
		if (Num_rendered_objects >= MAX_RENDERED_OBJECTS) {
			Int3();
			Num_rendered_objects /= 2;
		}
		Ordered_rendered_object_list[Num_rendered_objects++] = objnum;
	}

	if ((count++ > MAX_OBJECTS) || (obj->next == objnum)) {
		Int3();					// infinite loop detected
		obj->next = -1;		// won't this clean things up?
		return;					// get out of this infinite loop!
	}

		//g3_draw_object(obj->class_id,&obj->pos,&obj->orient,obj->size);

	//check for editor object

	#ifdef EDITOR
	if (Function_mode==FMODE_EDITOR && objnum==Cur_object_index) {
		save_3d_outline = g3d_interp_outline;
		g3d_interp_outline=1;
	}
	#endif

	#ifdef EDITOR
	if (_search_mode)
		render_object_search(obj);
	else
	#endif
		//NOTE LINK TO ABOVE
		render_object(obj);

	for (n=obj->attached_obj;n!=-1;n=Objects[n].ctype.expl_info.next_attach) {

		Assert(Objects[n].type == OBJ_FIREBALL);
		Assert(Objects[n].control_type == CT_EXPLOSION);
		Assert(Objects[n].flags & OF_ATTACHED);

		render_object(&Objects[n]);
	}


	#ifdef EDITOR
	if (Function_mode==FMODE_EDITOR && objnum==Cur_object_index)
		g3d_interp_outline = save_3d_outline;
	#endif


	//DEBUG mprintf( (0, "%d ", objnum ));

}

#ifndef NDEBUG
int	draw_boxes=0;
int window_check=1,draw_edges=0,new_seg_sorting=1,pre_draw_segs=0;
int no_migrate_segs=1,migrate_objects=1,behind_check=1;
int check_window_check=0;
#else
#define draw_boxes			0
#define window_check			1
#define draw_edges			0
#define new_seg_sorting		1
#define pre_draw_segs		0
#define no_migrate_segs		1
#define migrate_objects		1
#define behind_check			1
#define check_window_check	0
#endif

//increment counter for checking if points rotated
//This must be called at the start of the frame if rotate_list() will be used
void render_start_frame()
{
	RL_framecount++;

	if (RL_framecount==0) {		//wrap!

		memset(Rotated_last,0,sizeof(Rotated_last));		//clear all to zero
		RL_framecount=1;											//and set this frame to 1
	}
}

//Given a lit of point numbers, rotate any that haven't been rotated this frame
g3s_codes rotate_list(int nv,short *pointnumlist)
{
	int i,pnum;
	g3s_point *pnt;
	g3s_codes cc;

	cc.and = 0xff;  cc.or = 0;

	for (i=0;i<nv;i++) {

		pnum = pointnumlist[i];

		pnt = &Segment_points[pnum];

		if (Rotated_last[pnum] != RL_framecount) {

			g3_rotate_point(pnt,&Vertices[pnum]);

			Rotated_last[pnum] = RL_framecount;
		}

		cc.and &= pnt->p3_codes;
		cc.or  |= pnt->p3_codes;
	}

	return cc;

}

//Given a lit of point numbers, project any that haven't been projected
void project_list(int nv,short *pointnumlist)
{
	int i,pnum;

	for (i=0;i<nv;i++) {

		pnum = pointnumlist[i];

		if (!(Segment_points[pnum].p3_flags & PF_PROJECTED))

			g3_project_point(&Segment_points[pnum]);

	}
}


// -----------------------------------------------------------------------------------
void render_segment(int segnum)
{
	segment		*seg = &Segments[segnum];
	g3s_codes 	cc;
	int			sn;

	Assert(segnum!=-1 && segnum<=Highest_segment_index);

	cc=rotate_list(8,seg->verts);	// ASCENT: was &seg->verts (pointer to the array)

	if (! cc.and) {		//all off screen?

//mprintf( (0, "!"));
		//DEBUG mprintf( (0, "[Segment %d: ", segnum ));

		// set_segment_local_light_value(segnum,INITIAL_LOCAL_LIGHT);

		// ASCENT: only a segment that the original depth draws is marked
		// (the scripts read the map, see Render_orig_visible)
		if (Render_orig_visible)
			Automap_visited[segnum]=1;

		for (sn=0; sn<MAX_SIDES_PER_SEGMENT; sn++)
			render_side(seg, sn);
	}

	//draw any objects that happen to be in this segment

	//sort objects!
	//object_sort_segment_objects( seg );
		
	#ifndef NDEBUG
	if (!migrate_objects) {
		int objnum;
		for (objnum=seg->objects;objnum!=-1;objnum=Objects[objnum].next)
			do_render_object(objnum);
	}
	#endif

	//DEBUG mprintf( (0, "]\n", segnum ));

}

// ----- This used to be called when Show_only_curside was set.
// ----- It is wholly and superiorly replaced by render_side.
// -- //render one side of one segment
// -- void render_seg_side(segment *seg,int _side)
// -- {
// -- 	g3s_codes cc;
// -- 	short vertnum_list[4];
// -- 
// -- 	cc=g3_rotate_list(8,&seg->verts);
// -- 
// -- 	if (! cc.and) {		//all off screen?
// -- 		int fn,pn,i;
// -- 		side *s;
// -- 		face *f;
// -- 		poly *p;
// -- 
// -- 		s=&seg->sides[_side];
// -- 
// -- 		for (f=s->faces,fn=s->num_faces;fn;fn--,f++)
// -- 			for (p=f->polys,pn=f->num_polys;pn;pn--,p++) {
// -- 				grs_bitmap *tmap;
// -- 	
// -- 				for (i=0;i<p->num_vertices;i++) vertnum_list[i] = seg->verts[p->verts[i]];
// -- 	
// -- 				if (p->tmap_num >= NumTextures) {
// -- 					Warning("Invalid tmap number %d, NumTextures=%d\n...Changing in poly structure to tmap 0",p->tmap_num,NumTextures);
// -- 					p->tmap_num = 0;		//change it permanantly
// -- 				}
// -- 	
// -- 				tmap = Textures[p->tmap_num];
// -- 	
// -- 				g3_check_and_draw_tmap(p->num_vertices,vertnum_list,(g3s_uvl *) &p->uvls,tmap,&f->normal);
// -- 	
// -- 				if (Outline_mode) draw_outline(p->num_vertices,vertnum_list);
// -- 			}
// -- 		}
// -- 
// -- }

#define CROSS_WIDTH  i2f(8)
#define CROSS_HEIGHT i2f(8)

#ifndef NDEBUG

//draw outline for curside
outline_seg_side(segment *seg,int _side,int edge,int vert)
{
	g3s_codes cc;

	cc=rotate_list(8,seg->verts);	// ASCENT: was &seg->verts (pointer to the array)

	if (! cc.and) {		//all off screen?
		side *s;
		g3s_point *pnt;

		s=&seg->sides[_side];

		//render curedge of curside of curseg in green

		gr_setcolor(BM_XRGB(0,63,0));
		g3_draw_line(&Segment_points[seg->verts[Side_to_verts[_side][edge]]],&Segment_points[seg->verts[Side_to_verts[_side][(edge+1)%4]]]);

		//draw a little cross at the current vert

		pnt = &Segment_points[seg->verts[Side_to_verts[_side][vert]]];

		g3_project_point(pnt);		//make sure projected

//		gr_setcolor(BM_XRGB(0,0,63));
//		gr_line(pnt->p3_sx-CROSS_WIDTH,pnt->p3_sy,pnt->p3_sx+CROSS_WIDTH,pnt->p3_sy);
//		gr_line(pnt->p3_sx,pnt->p3_sy-CROSS_HEIGHT,pnt->p3_sx,pnt->p3_sy+CROSS_HEIGHT);

		gr_line(pnt->p3_sx-CROSS_WIDTH,pnt->p3_sy,pnt->p3_sx,pnt->p3_sy-CROSS_HEIGHT);
		gr_line(pnt->p3_sx,pnt->p3_sy-CROSS_HEIGHT,pnt->p3_sx+CROSS_WIDTH,pnt->p3_sy);
		gr_line(pnt->p3_sx+CROSS_WIDTH,pnt->p3_sy,pnt->p3_sx,pnt->p3_sy+CROSS_HEIGHT);
		gr_line(pnt->p3_sx,pnt->p3_sy+CROSS_HEIGHT,pnt->p3_sx-CROSS_WIDTH,pnt->p3_sy);
	}
}

#endif

#if 0		//this stuff could probably just be deleted

#define DEFAULT_PERSPECTIVE_DEPTH 6

int Perspective_depth=DEFAULT_PERSPECTIVE_DEPTH;	//how many levels deep to render in perspective

int inc_perspective_depth(void)
{
	return ++Perspective_depth;

}

int dec_perspective_depth(void)
{
	return Perspective_depth==1?Perspective_depth:--Perspective_depth;

}

int reset_perspective_depth(void)
{
	return Perspective_depth = DEFAULT_PERSPECTIVE_DEPTH;

}
#endif

typedef struct window {
	short left,top,right,bot;
} window;

ubyte code_window_point(fix x,fix y,window *w)
{
	ubyte code=0;

	if (x <= w->left)  code |= 1;
	if (x >= w->right) code |= 2;

	if (y <= w->top) code |= 4;
	if (y >= w->bot) code |= 8;

	return code;
}

#ifndef NDEBUG
draw_window_box(int color,short left,short top,short right,short bot)
{
	short l,t,r,b;

	gr_setcolor(color);

	l=left; t=top; r=right; b=bot;

	if ( r<0 || b<0 || l>=grd_curcanv->cv_bitmap.bm_w || t>=grd_curcanv->cv_bitmap.bm_h && b>=grd_curcanv->cv_bitmap.bm_h)
		return;

	if (l<0) l=0;
	if (t<0) t=0;
	if (r>=grd_curcanv->cv_bitmap.bm_w) r=grd_curcanv->cv_bitmap.bm_w-1;
	if (b>=grd_curcanv->cv_bitmap.bm_h) b=grd_curcanv->cv_bitmap.bm_h-1;

	gr_line(i2f(l),i2f(t),i2f(r),i2f(t));
	gr_line(i2f(r),i2f(t),i2f(r),i2f(b));
	gr_line(i2f(r),i2f(b),i2f(l),i2f(b));
	gr_line(i2f(l),i2f(b),i2f(l),i2f(t));

}
#endif

int matt_find_connect_side(int seg0,int seg1);

#ifndef NDEBUG
char visited2[MAX_SEGMENTS];
#endif

char visited[MAX_SEGMENTS];
// ASCENT: the arrays have the room of the full list of the GPU renderer, see
// RENDER_LIST_SIZE in render.h (were MAX_RENDER_SEGS)
short Render_list[RENDER_LIST_SIZE];
short Seg_depth[RENDER_LIST_SIZE];		//depth for each seg in Render_list
ubyte processed[RENDER_LIST_SIZE];		//whether each entry has been processed
int	lcnt_save,scnt_save;
//@@short *persp_ptr;
short render_pos[MAX_SEGMENTS];	//where in render_list does this segment appear?
//ubyte no_render_flag[MAX_RENDER_SEGS];
window render_windows[RENDER_LIST_SIZE];

// ASCENT: the extra object lists start at RENDER_LIST_SIZE (was
// MAX_RENDER_SEGS). The limits of this frame: the original ones for the
// software renderer, the full level for the GPU renderer (render_frame).
#define OBJ_LISTS_SIZE	(RENDER_LIST_SIZE+N_EXTRA_OBJ_LISTS_FULL)
short render_obj_list[OBJ_LISTS_SIZE][OBJS_PER_SEG];
static int Render_full_depth;					// 1: the GPU renderer draws the view
// The entries of Render_list before Orig_list_end are the list of the
// original depth: the search adds the segments in the same order, and the
// passes after Render_depth only add entries after them. Render_orig_visible
// is 1 while render_mine draws such a segment and its objects. The game
// state that the drawing writes (Automap_visited, which the scripts read,
// the list of the drawn robots for the homing weapons, the warning of a
// robot in the line of fire) only changes for those, as in the original.
static int Orig_list_end;
int Render_orig_visible = 1;
static int Render_seg_limit = MAX_RENDER_SEGS;	// the size limit of Render_list
static int Obj_lists_end = RENDER_LIST_SIZE+N_EXTRA_OBJ_LISTS;	// the end of the extra object lists

//for objects



#define RED   BM_XRGB(63,0,0)
#define WHITE BM_XRGB(63,63,63)

//Global vars for window clip test
int Window_clip_left,Window_clip_top,Window_clip_right,Window_clip_bot;

//Given two sides of segment, tell the two verts which form the 
//edge between them
int Two_sides_to_edge[6][6][2] = {	// ASCENT: the type was implicit int
	{ {-1,-1}, {3,7}, {-1,-1}, {2,6}, {6,7}, {2,3} },
	{ {3,7}, {-1,-1}, {0,4}, {-1,-1}, {4,7}, {0,3} },
	{ {-1,-1}, {0,4}, {-1,-1}, {1,5}, {4,5}, {0,1} },
	{ {2,6}, {-1,-1}, {1,5}, {-1,-1}, {5,6}, {1,2} },
	{ {6,7}, {4,7}, {4,5}, {5,6}, {-1,-1}, {-1,-1} },
	{ {2,3}, {0,3}, {0,1}, {1,2}, {-1,-1}, {-1,-1} }
};

//given an edge specified by two verts, give the two sides on that edge
int Edge_to_sides[8][8][2] = {	// ASCENT: the type was implicit int
	{ {-1,-1}, {2,5}, {-1,-1}, {1,5}, {1,2}, {-1,-1}, {-1,-1}, {-1,-1} },
	{ {2,5}, {-1,-1}, {3,5}, {-1,-1}, {-1,-1}, {2,3}, {-1,-1}, {-1,-1} },
	{ {-1,-1}, {3,5}, {-1,-1}, {0,5}, {-1,-1}, {-1,-1}, {0,3}, {-1,-1} },
	{ {1,5}, {-1,-1}, {0,5}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {0,1} },
	{ {1,2}, {-1,-1}, {-1,-1}, {-1,-1}, {-1,-1}, {2,4}, {-1,-1}, {1,4} },
	{ {-1,-1}, {2,3}, {-1,-1}, {-1,-1}, {2,4}, {-1,-1}, {3,4}, {-1,-1} },
	{ {-1,-1}, {-1,-1}, {0,3}, {-1,-1}, {-1,-1}, {3,4}, {-1,-1}, {0,4} },
	{ {-1,-1}, {-1,-1}, {-1,-1}, {0,1}, {1,4}, {-1,-1}, {0,4}, {-1,-1} },
};

//@@//perform simple check on tables
//@@check_check()
//@@{
//@@	int i,j;
//@@
//@@	for (i=0;i<8;i++)
//@@		for (j=0;j<8;j++)
//@@			Assert(Edge_to_sides[i][j][0] == Edge_to_sides[j][i][0] && 
//@@					Edge_to_sides[i][j][1] == Edge_to_sides[j][i][1]);
//@@
//@@	for (i=0;i<6;i++)
//@@		for (j=0;j<6;j++)
//@@			Assert(Two_sides_to_edge[i][j][0] == Two_sides_to_edge[j][i][0] && 
//@@					Two_sides_to_edge[i][j][1] == Two_sides_to_edge[j][i][1]);
//@@
//@@
//@@}


//given an edge, tell what side is on that edge
find_seg_side(segment *seg,short *verts,int notside)
{
	int i;
	int vv0=-1,vv1=-1;
	int side0,side1;
	int *eptr;
	int	v0,v1;
	short	*vp;

//@@	check_check();

	v0 = verts[0];
	v1 = verts[1];
	vp = seg->verts;

	for (i=0; i<8; i++) {
		int svv = *vp++;	// seg->verts[i];

		if (vv0==-1 && svv == v0) {
			vv0 = i;
			if (vv1 != -1)
				break;
		}

		if (vv1==-1 && svv == v1) {
			vv1 = i;
			if (vv0 != -1)
				break;
		}
	}

	Assert(vv0!=-1 && vv1!=-1);

	eptr = Edge_to_sides[vv0][vv1];

	side0 = eptr[0];
	side1 = eptr[1];

	Assert(side0!=-1 && side1!=-1);

	if (side0 != notside) {
		Assert(side1==notside);
		return side0;
	}
	else {
		Assert(side0==notside);
		return side1;
	}

}

//find the two segments that join a given seg though two sides, and
//the sides of those segments the abut. 
find_joining_side_norms(vms_vector *norm0_0,vms_vector *norm0_1,vms_vector *norm1_0,vms_vector *norm1_1,vms_vector **pnt0,vms_vector **pnt1,segment *seg,int s0,int s1)
{
	segment *seg0,*seg1;
	short edge_verts[2];
	int notside0,notside1;
	int edgeside0,edgeside1;

	Assert(s0!=-1 && s1!=-1);

	seg0 = &Segments[seg->children[s0]];
	seg1 = &Segments[seg->children[s1]];

	edge_verts[0] = seg->verts[Two_sides_to_edge[s0][s1][0]];
	edge_verts[1] = seg->verts[Two_sides_to_edge[s0][s1][1]];

	Assert(edge_verts[0]!=-1 && edge_verts[1]!=-1);

	notside0 = find_connect_side(seg,seg0);
	Assert(notside0 != -1);
	notside1 = find_connect_side(seg,seg1);
	Assert(notside1 != -1);

	edgeside0 = find_seg_side(seg0,edge_verts,notside0);
	edgeside1 = find_seg_side(seg1,edge_verts,notside1);

	//deal with the case where an edge is shared by more than two segments

//@@	if (IS_CHILD(seg0->children[edgeside0])) {
//@@		segment *seg00;
//@@		int notside00;
//@@
//@@		seg00 = &Segments[seg0->children[edgeside0]];
//@@
//@@		if (seg00 != seg1) {
//@@
//@@			notside00 = find_connect_side(seg0,seg00);
//@@			Assert(notside00 != -1);
//@@
//@@			edgeside0 = find_seg_side(seg00,edge_verts,notside00);
//@@	 		seg0 = seg00;
//@@		}
//@@
//@@	}
//@@
//@@	if (IS_CHILD(seg1->children[edgeside1])) {
//@@		segment *seg11;
//@@		int notside11;
//@@
//@@		seg11 = &Segments[seg1->children[edgeside1]];
//@@
//@@		if (seg11 != seg0) {
//@@			notside11 = find_connect_side(seg1,seg11);
//@@			Assert(notside11 != -1);
//@@
//@@			edgeside1 = find_seg_side(seg11,edge_verts,notside11);
//@@ 			seg1 = seg11;
//@@		}
//@@	}

//	if ( IS_CHILD(seg0->children[edgeside0]) ||
//		  IS_CHILD(seg1->children[edgeside1])) 
//		return 0;

	#ifdef COMPACT_SEGS
		get_side_normals(seg0, edgeside0, norm0_0, norm0_1 );
		get_side_normals(seg1, edgeside1, norm1_0, norm1_1 );
	#else 
		*norm0_0 = seg0->sides[edgeside0].normals[0];
		*norm0_1 = seg0->sides[edgeside0].normals[1];
		*norm1_0 = seg1->sides[edgeside1].normals[0];
		*norm1_1 = seg1->sides[edgeside1].normals[1];
	#endif

	*pnt0 = &Vertices[seg0->verts[Side_to_verts[edgeside0][seg0->sides[edgeside0].type==3?1:0]]];
	*pnt1 = &Vertices[seg1->verts[Side_to_verts[edgeside1][seg1->sides[edgeside1].type==3?1:0]]];

	return 1;
}

//see if the order matters for these two children.
//returns 0 if order doesn't matter, 1 if c0 before c1, -1 if c1 before c0
compare_children(segment *seg,short c0,short c1)
{
	vms_vector norm0_0,norm0_1,*pnt0,temp;
	vms_vector norm1_0,norm1_1,*pnt1;
	fix d0_0,d0_1,d1_0,d1_1,d0,d1;
int t;

	if (Side_opposite[c0] == c1) return 0;

	Assert(c0!=-1 && c1!=-1);

	//find normals of adjoining sides

	t = find_joining_side_norms(&norm0_0,&norm0_1,&norm1_0,&norm1_1,&pnt0,&pnt1,seg,c0,c1);

//if (!t)
// return 0;

	vm_vec_sub(&temp,&Viewer_eye,pnt0);
	d0_0 = vm_vec_dot(&norm0_0,&temp);
	d0_1 = vm_vec_dot(&norm0_1,&temp);

	vm_vec_sub(&temp,&Viewer_eye,pnt1);
	d1_0 = vm_vec_dot(&norm1_0,&temp);
	d1_1 = vm_vec_dot(&norm1_1,&temp);

	d0 = (d0_0 < 0 || d0_1 < 0)?-1:1;
	d1 = (d1_0 < 0 || d1_1 < 0)?-1:1;

	if (d0 < 0 && d1 < 0)
		return 0;

	if (d0 < 0)
		return 1;
	else if (d1 < 0)
		return -1;
	else
		return 0;

}

int ssc_total=0,ssc_swaps=0;

//short the children of segment to render in the correct order
//returns non-zero if swaps were made
int sort_seg_children(segment *seg,int n_children,short *child_list)
{
	int i,j;
	int r;
	int made_swaps,count;

	if (n_children == 0) return 0;

 ssc_total++;

	//for each child,  compare with other children and see if order matters
	//if order matters, fix if wrong

	count = 0;

	do {
		made_swaps = 0;

		for (i=0;i<n_children-1;i++)
			for (j=i+1;child_list[i]!=-1 && j<n_children;j++)
				if (child_list[j]!=-1) {
					r = compare_children(seg,child_list[i],child_list[j]);

					if (r == 1) {
						int temp = child_list[i];
						child_list[i] = child_list[j];
						child_list[j] = temp;
						made_swaps=1;
					}
				}

	} while (made_swaps && ++count<n_children);

 if (count)
  ssc_swaps++;

	return count;
}

add_obj_to_seglist(int objnum,int listnum)
{
	int i,checkn,marker;

	checkn = listnum;

	//first, find a slot

//mprintf((0,"adding obj %d to %d",objnum,listnum));

	do {

		for (i=0;render_obj_list[checkn][i] >= 0;i++);
	
		Assert(i < OBJS_PER_SEG);
	
		marker = render_obj_list[checkn][i];

		if (marker != -1) {
			checkn = -marker;
			//Assert(checkn < MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS);
			if (checkn >= Obj_lists_end) {		// ASCENT: was MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS
				Int3();
				return;
			}
		}

	} while (marker != -1);

//mprintf((0,"  slot %d,%d",checkn,i));


	//now we have found a slot.  put object in it

	if (i != OBJS_PER_SEG-1) {

		render_obj_list[checkn][i] = objnum;
		render_obj_list[checkn][i+1] = -1;
	}
	else {				//chain to additional list
		int lookn;

		//find an available sublist

		// ASCENT: the extra lists are RENDER_LIST_SIZE to Obj_lists_end (were
		// MAX_RENDER_SEGS to MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS). The test of
		// lookn comes first: the original read one list past the end.
		for (lookn=RENDER_LIST_SIZE;lookn<Obj_lists_end && render_obj_list[lookn][0]!=-1;lookn++);

		//Assert(lookn<MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS);
		if (lookn >= Obj_lists_end) {
			Int3();
			return;
		}

		render_obj_list[checkn][i] = -lookn;
		render_obj_list[lookn][0] = objnum;
		render_obj_list[lookn][1] = -1;

	}

//mprintf((0,"  added!\n"));

}

#define SORT_LIST_SIZE 50

typedef struct sort_item {
	int objnum;
	fix dist;
} sort_item;

sort_item sort_list[SORT_LIST_SIZE];
int n_sort_items;

//compare function for object sort. 
int sort_func(sort_item *a,sort_item *b)
{
	fix delta_dist;
	object *obj_a,*obj_b;

	delta_dist = a->dist - b->dist;

	obj_a = &Objects[a->objnum];
	obj_b = &Objects[b->objnum];

	if (abs(delta_dist) < (obj_a->size + obj_b->size)) {		//same position

		//these two objects are in the same position.  see if one is a fireball
		//or laser or something that should plot on top

		if (obj_a->type == OBJ_WEAPON || obj_a->type == OBJ_FIREBALL)
			if (!(obj_b->type == OBJ_WEAPON || obj_b->type == OBJ_FIREBALL))
				return -1;	//a is weapon, b is not, so say a is closer
			else;				//both are weapons 
		else
			if (obj_b->type == OBJ_WEAPON || obj_b->type == OBJ_FIREBALL)
				return 1;	//b is weapon, a is not, so say a is farther

		//no special case, fall through to normal return
	}

	return delta_dist;	//return distance
}

build_object_lists(int n_segs)
{
	int nn;

//mprintf((0,"build n_segs=%d",n_segs));

	for (nn=0;nn<OBJ_LISTS_SIZE;nn++)		// ASCENT: was MAX_RENDER_SEGS+N_EXTRA_OBJ_LISTS
		render_obj_list[nn][0] = -1;

	for (nn=0;nn<n_segs;nn++) {
		int segnum;

		segnum = Render_list[nn];

//mprintf((0,"nn=%d seg=%d ",nn,segnum));

		if (segnum != -1) {
			int objnum;
			object *obj;

			for (objnum=Segments[segnum].objects;objnum!=-1;objnum = obj->next) {
				int new_segnum,did_migrate,list_pos;

				obj = &Objects[objnum];

				Assert( obj->segnum == segnum );

				if (obj->flags & OF_ATTACHED)
					continue;		//ignore this object

				new_segnum = segnum;
				list_pos = nn;

//mprintf((0,"objnum=%d ",objnum));
				if (obj->type != OBJ_CNTRLCEN)		//don't migrate controlcen
				do {
					segmasks m;

					did_migrate = 0;
	
					m = get_seg_masks(&obj->pos,new_segnum,obj->size);
	
					if (m.sidemask) {
						int sn,sf;

						for (sn=0,sf=1;sn<6;sn++,sf<<=1)
							if (m.sidemask & sf) {
								segment *seg = &Segments[obj->segnum];
		
								if (WALL_IS_DOORWAY(seg,sn) & WID_FLY_FLAG) {		//can explosion migrate through
									int child = seg->children[sn];
									int checknp;
		
									for (checknp=list_pos;checknp--;)
										if (Render_list[checknp] == child) {
//mprintf((0,"mig from %d to %d ",new_segnum,child));
											new_segnum = child;
											list_pos = checknp;
											did_migrate = 1;
										}
								}
							}
					}
	
				} while (did_migrate);

				add_obj_to_seglist(objnum,list_pos);
	
			}

		}
	}

//mprintf((0,"done build "));

	//now that there's a list for each segment, sort the items in those lists
	for (nn=0;nn<n_segs;nn++) {
		int segnum;

		segnum = Render_list[nn];

//mprintf((0,"nn=%d seg=%d ",nn,segnum));

		if (segnum != -1) {
			int t,lookn,i,n;

			//first count the number of objects & copy into sort list

			lookn = nn;
			i = n_sort_items = 0;
			while ((t=render_obj_list[lookn][i++])!=-1)
				if (t<0)
					{lookn = -t; i=0;}
				else
					if (n_sort_items < SORT_LIST_SIZE-1) {		//add if room
						sort_list[n_sort_items].objnum = t;
						//NOTE: maybe use depth, not dist - quicker computation
						sort_list[n_sort_items].dist = vm_vec_dist_quick(&Objects[t].pos,&Viewer_eye);
						n_sort_items++;
					}


			//now call qsort

			qsort(sort_list,n_sort_items,sizeof(*sort_list),(int (*)(const void *,const void *)) sort_func);	// ASCENT: cast to the type that qsort() takes

			//now copy back into list

			lookn = nn;
			i = 0;
			n = n_sort_items;
			while ((t=render_obj_list[lookn][i])!=-1 && n>0)
				if (t<0)
					{lookn = -t; i=0;}
				else
					render_obj_list[lookn][i++] = sort_list[--n].objnum;
			render_obj_list[lookn][i] = -1;	//mark (possibly new) end
		}
	}
}

int Use_player_head_angles = 0;
vms_angvec Player_head_angles;

extern int Num_tmaps_drawn;
extern int Total_pixels;
//--unused-- int Total_num_tmaps_drawn=0;

int Rear_view=0;

#ifdef JOHN_ZOOM
fix Zoom_factor=F1_0;
#endif
//renders onto current canvas
void render_mine(int start_seg_num,fix eye_offset);	// ASCENT: prototype, used before the definition

// ASCENT: the source of a bitmap for the upscaled texture set of the GPU
// renderer (g3_gpu_options in 3d.h): the archive and the name of a bitmap
// of GameBitmaps, found by the address of its data, or the two bitmaps of
// a merged wall texture.
typedef struct gpu_bm_ref {
	const ubyte *data;
	int index, egg;
} gpu_bm_ref;

extern ubyte bogus_data[64*64];		// piggy.c: the data of the bitmaps that are not valid

static gpu_bm_ref Gpu_refs[MAX_BITMAP_FILES];
static int Gpu_n_refs, Gpu_refs_generation = -1;

static int gpu_ref_cmp(const void *a, const void *b)
{
	uintptr_t x = (uintptr_t) ((const gpu_bm_ref *) a)->data, y = (uintptr_t) ((const gpu_bm_ref *) b)->data;
	return x < y ? -1 : x > y;
}

static void gpu_refs_update(void)
{
	int ne = piggy_get_num_eggs(), e, i, n = 0;

	// The table is made again after each change of the bitmap tables. A
	// test of the counts only missed a level change that loads an archive
	// with the same number of bitmaps at new addresses (the same level
	// archive again after a warp, or a level of the same set): the bitmaps
	// of the new level then had no source and stayed 8-bit.
	if (Piggy_generation == Gpu_refs_generation)
		return;
	Gpu_refs_generation = Piggy_generation;
	Gpu_n_refs = 0;
	for (e = 0; e < ne; e++) {
		int nb = 0, first = 0;
		if (piggy_get_egg_counts(e, &nb, NULL, NULL, &first, NULL, NULL) < 0)
			continue;
		for (i = first; i < first + nb && i < Num_bitmap_files && i < MAX_BITMAP_FILES; i++)
			if (GameBitmaps[i].bm_data && GameBitmaps[i].bm_data != bogus_data) {
				Gpu_refs[n].data = GameBitmaps[i].bm_data;
				Gpu_refs[n].index = i;
				Gpu_refs[n].egg = e;
				n++;
			}
	}
	qsort(Gpu_refs, n, sizeof(gpu_bm_ref), gpu_ref_cmp);
	Gpu_n_refs = n;
}

static int gpu_bitmap_source(grs_bitmap *bm, g3_gpu_source *src)
{
	gpu_bm_ref key, *r;
	const char *egg, *slash;
	char name[16];
	int i, k;

	memset(src, 0, sizeof(*src));
	if (texmerge_get_sources(bm, &src->bottom, &src->top, &src->orient)) {
		src->kind = 2;
		return 1;
	}
	gpu_refs_update();
	key.data = bm->bm_data;
	r = (gpu_bm_ref *) bsearch(&key, Gpu_refs, Gpu_n_refs, sizeof(gpu_bm_ref), gpu_ref_cmp);
	if (!r || GameBitmaps[r->index].bm_w != bm->bm_w || GameBitmaps[r->index].bm_h != bm->bm_h)
		return 0;
	egg = piggy_get_egg_name(r->egg);
	if (!egg)
		return 0;
	// the base name of the archive in lower case
	slash = egg;
	for (i = 0; egg[i]; i++)
		if (egg[i] == '/' || egg[i] == 0x5C || egg[i] == ':')
			slash = egg + i + 1;
	for (i = 0, k = 0; slash[i] && slash[i] != '.' && k < (int) sizeof(src->egg) - 1; i++)
		src->egg[k++] = (char) tolower((unsigned char) slash[i]);
	src->egg[k] = 0;
	// the name of the bitmap in lower case, '#' replaced by '_'
	piggy_get_bitmap_name(r->index, name);
	for (i = 0, k = 0; name[i] && k < (int) sizeof(src->name) - 1; i++)
		src->name[k++] = name[i] == '#' ? '_' : (char) tolower((unsigned char) name[i]);
	src->name[k] = 0;
	src->kind = 1;
	return src->egg[0] && src->name[0];
}

// ASCENT: the settings of the GPU renderer for this frame. The RGB form is
// the GPU renderer of the game. The 8-bit form (palette indexes, the
// colors of the software renderer) is kept as a reference for the tests:
// environment variable ASCENT_GPU_8BIT=1.
static int gpu_set_options(void)
{
	static int bound = 0;
	g3_gpu_options o;
	const char *e = getenv("ASCENT_GPU_8BIT");

	if (!bound) {
		plat_gpu_bind_settings(&Dtu_gpu_renderer, &Dtu_shader);
		bound = 1;
	}
	// A test aid: ASCENT_GPU_CYCLE=N changes one graphics setting each N
	// frames, as the Graphics menu does during play.
	{
		const char *c = getenv("ASCENT_GPU_CYCLE");
		static int frame = 0;
		int n = c ? atoi(c) : 0;
		if (n > 0 && ++frame % n == 0) {
			int step = frame / n;
			switch (step % 5) {
			case 0: Dtu_gpu_renderer = !Dtu_gpu_renderer; break;
			case 1: Dtu_upscaled_textures = !Dtu_upscaled_textures; break;
			case 2: Dtu_anisotropic = !Dtu_anisotropic; break;
			case 3: Dtu_shader = (Dtu_shader + 1) % 3; break;
			case 4: Dtu_render_scale = Dtu_render_scale % DTU_RENDER_SCALE_MAX + 1; break;
			}
		}
	}
	memset(&o, 0, sizeof(o));
	o.rgb = !(e && e[0] == '1');
	o.anisotropic = Dtu_anisotropic;
	o.upscaled = Dtu_upscaled_textures;
	o.hd_dir = Dtu_upscaled_textures ? dtu_hd_dir() : NULL;
	o.palette = gr_palette;
	o.source = gpu_bitmap_source;
	g3_gpu_set_options(&o);
	return o.rgb;		// ASCENT: the form of the view (infravision)
}

// ASCENT: the preload of the upscaled textures that a level shows later.
// The GPU renderer makes the texture of a bitmap the first time that a
// view draws it and loads the upscaled form in the background
// (gputex.c), so that view shows the 8-bit form, and the upscaled form
// comes a frame or more later. A door or a switch changes its texture
// during play: the first opening of a door showed each frame of the
// animation in the 8-bit form first, a blurred door that went sharp, and
// the next door of the same kind was sharp. Descent 1 pages in the frames
// of all wall animations at the level start (paging_touch_walls,
// paging.c); this does the same for the GPU textures: the frames of the
// door animations of the walls, the textures of the triggers and the
// traps (the switches), and the frames of the animated textures of the
// sides (the effects), each merged with the other texture of its side as
// the view merges it (texmerge.c), and then the textures of all the other
// sides that a view can draw. It runs after the first view of a
// level, so that the upscaled loads of that view come first, and again
// when the RGB textures are made again (g3_gpu_texture_generation). It
// makes all the textures at one time: in the tests a texture made between
// the draws of the views cost several times more than in one batch.
// ASCENT_GPU_NOPRELOAD=1 turns it off (a test aid).

#define GPU_PRELOAD_PAIRS	8192		// power of 2

// the pairs of textures done (bottom << 16 | top, plus 1), a hash set, so
// that the many sides with the same textures are merged one time
static uint32_t Gpu_preload_set[GPU_PRELOAD_PAIRS];
static int Gpu_preload_pairs, Gpu_preload_count;

// the effect that animates texture t, or -1
static int gpu_preload_effect(int t)
{
	int i;

	for (i = 0; i < Num_effects; i++)
		if (Effects[i].changing_wall_texture == t && Effects[i].vc.num_frames > 0)
			return i;
	return -1;
}

static int gpu_preload_tmap_ok(int t)
{
	return t >= 0 && t < MAX_TEXTURES && Textures[t].index > 0 && Textures[t].index < MAX_BITMAP_FILES;
}

// The bottom texture t1 and the top texture t2 (with the rotation in the
// top 2 bits, 0 if none) of a side, with each frame of their effects.
static void gpu_preload_pair(int t1, int t2)
{
	int top = t2 & 0x3FFF;
	int e1, e2, n1, n2, i, j, n;
	uint32_t k, h;
	bitmap_index s1, s2;

	if (!gpu_preload_tmap_ok(t1) || (top && !gpu_preload_tmap_ok(top)) || Gpu_preload_pairs >= GPU_PRELOAD_PAIRS / 2)
		return;
	k = ((uint32_t) t1 << 16 | (uint32_t) (t2 & 0xFFFF)) + 1;
	h = (k * 2654435761u) >> 19 & (GPU_PRELOAD_PAIRS - 1);
	for (n = 0; n < GPU_PRELOAD_PAIRS && Gpu_preload_set[h]; n++, h = (h + 1) & (GPU_PRELOAD_PAIRS - 1))
		if (Gpu_preload_set[h] == k)
			return;
	Gpu_preload_set[h] = k;
	Gpu_preload_pairs++;

	e1 = gpu_preload_effect(t1);
	e2 = top ? gpu_preload_effect(top) : -1;
	n1 = e1 >= 0 ? Effects[e1].vc.num_frames : 1;
	n2 = e2 >= 0 ? Effects[e2].vc.num_frames : 1;
	s1 = Textures[t1];
	s2 = Textures[top];
	for (i = 0; i < n1; i++)
		for (j = 0; j < n2; j++) {
			grs_bitmap *bm;

			if (e1 >= 0)
				Textures[t1] = Effects[e1].vc.frames[i];
			if (e2 >= 0)
				Textures[top] = Effects[e2].vc.frames[j];
			bm = top ? texmerge_get_cached_bitmap(t1, t2) : &GameBitmaps[Textures[t1].index];
			Gpu_preload_count += g3_gpu_preload(bm);
		}
	Textures[top] = s2;
	Textures[t1] = s1;
}

// a texture number of the level file (a trigger or a trap) as a top
// texture of the game, as set_side_tmap2 (dtutrig.c) converts it
static int gpu_preload_level_tmap(int tex)
{
	int t = tex & 0x3FFF;

	if (tex < 0 || t == 0 || t >= Num_level_tmap_names || Level_tmap_xlate[t] <= 0)
		return 0;
	return Level_tmap_xlate[t] | (tex & ~0x3FFF & 0xFFFF);
}

static void gpu_preload_side_tex(int seg, int side, int tex)
{
	int t2 = gpu_preload_level_tmap(tex);

	if (seg < 0 || seg > Highest_segment_index || side < 0 || side >= MAX_SIDES_PER_SEGMENT || !t2)
		return;
	gpu_preload_pair(Segments[seg].sides[side].tmap_num, t2);
}

static void gpu_preload_level(void)
{
	static int done_level = -1, done_piggy = -1, done_gen = -1, wait_loads = 0;
	int gen = g3_gpu_texture_generation();
	int i, j, s, n_walls, n_trig, n_anim;
	uint64_t t0;
	const char *off = getenv("ASCENT_GPU_NOPRELOAD");
	int log = getenv("ASCENT_GPU_HDLOG") != NULL;

	if (!Dtu_upscaled_textures || (off && off[0] == '1'))
		return;
	if (done_level == Current_level_num && done_piggy == Piggy_generation && done_gen == gen) {
		if (wait_loads && plat_gpu_hd_pending() == 0) {
			wait_loads = 0;
			if (log)
				printf("GPU hd: preload: the upscaled loads are done at frame %d\n", FrameCount);
		}
		return;
	}
	done_level = Current_level_num;
	done_piggy = Piggy_generation;
	done_gen = gen;
	wait_loads = 1;
	Gpu_preload_pairs = Gpu_preload_count = 0;
	memset(Gpu_preload_set, 0, sizeof(Gpu_preload_set));
	t0 = plat_time_us();

	// the frames of the door animations (paging_touch_walls), as
	// wall_set_tmap_num (wall.c) sets them
	for (i = 0; i < Num_walls; i++) {
		wall *w = &Walls[i];
		wclip *anim;
		side *sp;

		if (w->clip_num < 0 || w->clip_num >= Num_wall_anims || w->segnum < 0 || w->segnum > Highest_segment_index
				|| w->sidenum < 0 || w->sidenum >= MAX_SIDES_PER_SEGMENT)
			continue;
		anim = &WallAnims[w->clip_num];
		sp = &Segments[w->segnum].sides[w->sidenum];
		for (j = 0; j < anim->num_frames && j < MAX_CLIP_FRAMES; j++) {
			if (anim->flags & WCF_TMAP1)
				gpu_preload_pair(anim->frames[j], sp->tmap_num2);
			else
				gpu_preload_pair(sp->tmap_num, anim->frames[j]);
		}
	}
	n_walls = Gpu_preload_pairs;
	// the textures that the triggers and the traps put on their sides
	for (i = 0; i < Dtu_num_triggers; i++) {
		gpu_preload_side_tex(Dtu_triggers[i].seg, Dtu_triggers[i].side, Dtu_triggers[i].tex_on);
		gpu_preload_side_tex(Dtu_triggers[i].seg, Dtu_triggers[i].side, Dtu_triggers[i].tex_off);
	}
	for (i = 0; i < Dtu_num_traps; i++) {
		gpu_preload_side_tex(Dtu_traps[i].seg, Dtu_traps[i].side, Dtu_traps[i].tex_enabled);
		gpu_preload_side_tex(Dtu_traps[i].seg, Dtu_traps[i].side, Dtu_traps[i].tex_disabled);
	}
	n_trig = Gpu_preload_pairs - n_walls;
	// the frames of the animated textures of the sides
	for (s = 0; s <= Highest_segment_index; s++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			side *sp = &Segments[s].sides[j];

			if (!(WALL_IS_DOORWAY(&Segments[s], j) & WID_RENDER_FLAG))
				continue;
			if (gpu_preload_effect(sp->tmap_num) >= 0 || (sp->tmap_num2 && gpu_preload_effect(sp->tmap_num2 & 0x3FFF) >= 0))
				gpu_preload_pair(sp->tmap_num, sp->tmap_num2);
		}
	n_anim = Gpu_preload_pairs - n_walls - n_trig;
	// the other textures of the sides: a wall seen for the first time
	// after a turn showed its 8-bit form for a frame or more. A level has
	// 30 to 100 such pairs more (10 to 30 MB of upscaled textures).
	// ASCENT_GPU_NOPRELOAD=2 leaves them out (a test aid)
	for (s = 0; s <= Highest_segment_index && !(off && off[0] == '2'); s++)
		for (j = 0; j < MAX_SIDES_PER_SEGMENT; j++) {
			side *sp = &Segments[s].sides[j];

			if (!(WALL_IS_DOORWAY(&Segments[s], j) & WID_RENDER_FLAG))
				continue;
			gpu_preload_pair(sp->tmap_num, sp->tmap_num2);
		}
	if (log)
		printf("GPU hd: preload of level %d at frame %d: %d pairs (%d of the doors, %d of the triggers, %d of the effects), %d bitmaps, %.1f ms\n",
			Current_level_num, FrameCount, Gpu_preload_pairs, n_walls, n_trig, n_anim, Gpu_preload_count,
			(plat_time_us() - t0) / 1000.0);
}

// ASCENT: a test aid. ASCENT_RENDER_STATS=1 prints at exit the size of the
// render list (the segments drawn), the deepest level of the segment
// search and the CPU time of render_mine (the search, the light and the
// draw calls of the segments and the objects).
static int Rstats_on = -1;
static uint64_t Rstats_frames, Rstats_segs, Rstats_segs_max, Rstats_us, Rstats_us_max;
static int Rstats_depth_max;

static void rstats_print(void)
{
	if (!Rstats_frames)
		return;
	printf("Render stats: %d frames, segments avg %.1f max %d, search depth max %d, render_mine avg %.3f ms max %.3f ms\n",
		(int) Rstats_frames, (double) Rstats_segs / Rstats_frames, (int) Rstats_segs_max, Rstats_depth_max,
		Rstats_us / 1000.0 / Rstats_frames, Rstats_us_max / 1000.0);
	fflush(stdout);
}

static void rstats_frame(uint64_t us)
{
	int i, n = 0;

	for (i = 0; i < N_render_segs; i++)
		if (Render_list[i] != -1) {
			n++;
			if (Seg_depth[i] > Rstats_depth_max)
				Rstats_depth_max = Seg_depth[i];
		}
	Rstats_frames++;
	Rstats_segs += n;
	if (n > Rstats_segs_max)
		Rstats_segs_max = n;
	Rstats_us += us;
	if (us > Rstats_us_max)
		Rstats_us_max = us;
}

void render_frame(fix eye_offset)
{
	int start_seg_num;
	int gpu_view;		// ASCENT: the GPU renderer draws this view
	uint64_t t0;		// ASCENT: ASCENT_RENDER_STATS
	int gpu_view_full;	// ASCENT: the full depth of the GPU renderer
	int infra;			// ASCENT: the infravision mode of the view (dtuinfra.h)

//Total_num_tmaps_drawn += Num_tmaps_drawn;
//if ((FrameCount > 0) && (Total_num_tmaps_drawn))
//	mprintf((0, "Frame: %4i, total = %6i, Avg = %7.3f, Avgpix=%7.3f\n", Num_tmaps_drawn, Total_num_tmaps_drawn, (float) Total_num_tmaps_drawn/FrameCount, (float) Total_pixels/Total_num_tmaps_drawn));
//Num_tmaps_drawn = 0;

	if (Endlevel_sequence) {
		render_endlevel_frame(eye_offset);
		FrameCount++;
		return;
	}

#ifdef NEWDEMO
	if ( Newdemo_state == ND_STATE_RECORDING )	{
		if (eye_offset >= 0 )	{
			newdemo_record_start_frame(FrameCount, FrameTime );
			newdemo_record_viewer_object(Viewer);
		}
	}
#endif

	g3_start_frame();

	// ASCENT: the pulse of the effect color 0xCF (tmapfx.c), 0x72818 to
	// 0x72867: one step each 6 views, from 0 up to 25, then between 24 and 31.
	{
		static int value = 0, dir = 1, count = 5;

		if (--count < 0) {
			value += dir;
			count = 5;
		}
		if (value < 25)
			dir = 1;
		if (value > 30)
			dir = -1;
		Tmap_fx_row = value;
		G3_gpu_fx_row = value;
	}

	Viewer_eye = Viewer->pos;

//	if (Viewer->type == OBJ_PLAYER && (Cockpit_mode!=CM_REAR_VIEW))
//		vm_vec_scale_add2(&Viewer_eye,&Viewer->orient.fvec,(Viewer->size*3)/4);

	if (eye_offset)	{
		vm_vec_scale_add2(&Viewer_eye,&Viewer->orient.rvec,eye_offset);
	}

	// ASCENT: the eye of the player of the target game is above the center
	// of the body (DTUGAME.EXE 0x76E9C). The function also sets the pitch
	// of the view in Player_head_angles.
	if (Viewer==ConsoleObject)
		dtu_player_eye(&Viewer_eye);

	#ifdef EDITOR
	if (Function_mode==FMODE_EDITOR)
		Viewer_eye = Viewer->pos;
	#endif

	start_seg_num = find_point_seg(&Viewer_eye,Viewer->segnum);

	if (start_seg_num==-1)
		start_seg_num = Viewer->segnum;

	if (Viewer==ConsoleObject && Use_player_head_angles) {
		vms_matrix headm,viewm;
		vm_angles_2_matrix(&headm,&Player_head_angles);
		vm_matrix_x_matrix(&viewm,&Viewer->orient,&headm);
		g3_set_view_matrix(&Viewer_eye,&viewm,Render_zoom);
	//@@} else if ((Cockpit_mode==CM_REAR_VIEW) && (Viewer==ConsoleObject)) {
	} else if (Rear_view && (Viewer==ConsoleObject)) {
		vms_matrix headm,viewm;
		Player_head_angles.p = Player_head_angles.b = 0;
		Player_head_angles.h = 0x7fff;
		vm_angles_2_matrix(&headm,&Player_head_angles);
		vm_matrix_x_matrix(&viewm,&Viewer->orient,&headm);
		g3_set_view_matrix(&Viewer_eye,&viewm,Render_zoom);
	} else	{
#ifdef JOHN_ZOOM
		if (keyd_pressed[KEY_RSHIFT] )	{
			Zoom_factor += FrameTime*4;
			if (Zoom_factor > F1_0*5 ) Zoom_factor=F1_0*5;
		} else {
			Zoom_factor -= FrameTime*4;
			if (Zoom_factor < F1_0 ) Zoom_factor = F1_0;
		}
		g3_set_view_matrix(&Viewer_eye,&Viewer->orient,fixdiv(Render_zoom,Zoom_factor));
#else
		g3_set_view_matrix(&Viewer_eye,&Viewer->orient,Render_zoom);
#endif
	}

	// ASCENT: the GPU renderer (gpu3d.c). The 3D view is drawn at
	// Dtu_render_scale times the size of the canvas; in the high resolution
	// form each line of the canvas covers two lines of the screen. With
	// Dtu_improved_occlusion a depth buffer hides the parts of the objects
	// behind the walls. The GPU clears the image.
	// Infravision (dtuinfra.c) sets the fade table or the heat colors
	// before the view starts: the GPU reads both at g3_gpu_begin_view.
	// The dark view of the conversation window (tmapfx.c, dtudlg.c): the
	// table is made from the fade table before infravision changes it, as
	// the original tests the dark view first.
	{
		int dim = dtu_dlg_view_dim();

		tmap_dim_set_view(dim);
		G3_gpu_dim = dim;
	}
	infra = dtu_infra_begin_view(gpu_set_options() && Dtu_gpu_renderer && plat_gpu_available());
	gpu_view = g3_gpu_begin_view(Dtu_gpu_renderer, Dtu_render_scale,
		(Dtu_display_active && Dtu_hires) ? 2 : 1, Dtu_improved_occlusion,
		(Clear_window == 1 && Clear_window_color != -1) ? Clear_window_color : BM_XRGB(0, 0, 0));
	if (infra == DTU_INFRA_IMPROVED && !gpu_view)
		dtu_infra_fallback();

	if (Clear_window == 1 && !gpu_view) {
		if (Clear_window_color == -1)
			Clear_window_color = BM_XRGB(0, 0, 0);	//BM_XRGB(31, 15, 7);
		gr_clear_canvas(Clear_window_color);
	}

	// ASCENT: the GPU renderer draws every visible segment and its objects
	// to the full distance of the level: the GPU draws the few polygons of a
	// level fast. The software renderer keeps Render_depth, MAX_RENDER_SEGS
	// and N_EXTRA_OBJ_LISTS (endlevel.c and terrain.c call render_mine with
	// these). The flat and linear far textures of the software texture
	// mapper (Max_linear_depth, Max_perspective_depth) are not used by the
	// GPU renderer, and no model of the game has a simpler model.
	// ASCENT_RENDER_ORIGDEPTH=1 keeps the original depth on the GPU (a test
	// aid for comparisons).
	{
		static int orig_depth = -1;
		if (orig_depth < 0) {
			const char *e = getenv("ASCENT_RENDER_ORIGDEPTH");
			orig_depth = (e && e[0] == '1');
		}
		if (orig_depth)
			gpu_view_full = 0;
		else
			gpu_view_full = gpu_view;
	}
	if (gpu_view_full) {
		Render_full_depth = 1;
		Render_seg_limit = RENDER_LIST_SIZE;
		Obj_lists_end = OBJ_LISTS_SIZE;
	}

	if (Rstats_on < 0) {
		const char *e = getenv("ASCENT_RENDER_STATS");
		Rstats_on = (e && e[0] == '1');
		if (Rstats_on)
			atexit(rstats_print);
	}
	t0 = Rstats_on ? plat_time_us() : 0;

	render_mine(start_seg_num,eye_offset);

	if (Rstats_on)
		rstats_frame(plat_time_us() - t0);

	Render_full_depth = 0;
	Render_seg_limit = MAX_RENDER_SEGS;
	Obj_lists_end = RENDER_LIST_SIZE+N_EXTRA_OBJ_LISTS;

	// ASCENT: the arms and the weapon are drawn over the level, as the
	// original draws them after it: the depth buffer is cleared.
	if (gpu_view)
		g3_gpu_clear_depth();

	// ASCENT: the arms and the weapon of the player of the target game
	// (render_object 0x67C5A draws the viewer, see dtucombat.c)
	// The improved infravision keeps the first person model (the arms
	// and the weapon in one model) cold: the weapon does not glow.
	if (Viewer == ConsoleObject) {
		G3_gpu_heat = PLAT_GPU_HEAT_COLD;
		dtu_combat_draw_view();
	}

	if (Use_player_head_angles ) 
		draw_3d_reticle(eye_offset);

	g3_gpu_end_view();		// ASCENT: see g3_gpu_begin_view
	dtu_infra_end_view(gpu_view);
	if (gpu_view)
		gpu_preload_level();	// ASCENT: after the textures of the first view

	g3_end_frame();

	FrameCount++;		//we have rendered a frame
}

int first_terminal_seg;

//build a list of segments to be rendered
//fills in Render_list & N_render_segs
build_segment_list(int start_seg_num)
{
	int	lcnt,scnt,ecnt;
	int	l,c;
	int	ch;

	memset(visited, 0, sizeof(visited[0])*(Highest_segment_index+1));
	memset(render_pos, -1, sizeof(render_pos[0])*(Highest_segment_index+1));
	//memset(no_render_flag, 0, sizeof(no_render_flag[0])*(MAX_RENDER_SEGS));
	memset(processed, 0, sizeof(processed));

	#ifndef NDEBUG
	memset(visited2, 0, sizeof(visited2[0])*(Highest_segment_index+1));
	#endif

	lcnt = scnt = 0;

	Render_list[lcnt] = start_seg_num; visited[start_seg_num]=1;
	Seg_depth[lcnt] = 0;
	lcnt++;
	ecnt = lcnt;
	render_pos[start_seg_num] = 0;

	#ifndef NDEBUG
	if (pre_draw_segs)
		render_segment(start_seg_num);
	#endif

	render_windows[0].left=render_windows[0].top=0;
	render_windows[0].right=grd_curcanv->cv_bitmap.bm_w-1;
	render_windows[0].bot=grd_curcanv->cv_bitmap.bm_h-1;

	//breadth-first renderer

	Orig_list_end = -1;		// ASCENT: see Render_orig_visible

	//build list

	// ASCENT: the GPU renderer lists the segments to the full distance of
	// the level (Render_full_depth, see render_frame). The search ends when
	// a pass processes no entry: then no later pass changes the list.
	for (l=0;l<(Render_full_depth ? MAX_SEGMENTS : Render_depth);l++) {
		int n_processed = 0;	// ASCENT: the entries processed in this pass

		if (l == Render_depth)
			Orig_list_end = lcnt;

		//while (scnt < ecnt) {
		for (scnt=0;scnt < ecnt;scnt++) {
			int rotated,segnum;
			window *check_w;
			short child_list[MAX_SIDES_PER_SEGMENT];		//list of ordered sides to process
			int n_children;										//how many sides in child_list
			segment *seg;

			if (processed[scnt])
				continue;

			processed[scnt]=1;
			n_processed++;

			segnum = Render_list[scnt];
			check_w = &render_windows[scnt];

			#ifndef NDEBUG
			if (draw_boxes)
				draw_window_box(RED,check_w->left,check_w->top,check_w->right,check_w->bot);
			#endif

			if (segnum == -1) continue;

			seg = &Segments[segnum];
			rotated=0;

			//look at all sides of this segment.
			//tricky code to look at sides in correct order follows

			for (c=n_children=0;c<MAX_SIDES_PER_SEGMENT;c++) {		//build list of sides
				int wid;

				wid = WALL_IS_DOORWAY(seg, c);

				ch=seg->children[c];

				if ( (window_check || !visited[ch]) && (wid & WID_RENDPAST_FLAG) ) {
					if (behind_check) {
						byte *sv = Side_to_verts[c];
						ubyte codes_and=0xff;
						int i;

						rotate_list(8,seg->verts);	// ASCENT: was &seg->verts (pointer to the array)
						rotated=1;

						for (i=0;i<4;i++)
							codes_and &= Segment_points[seg->verts[sv[i]]].p3_codes;

						if (codes_and & CC_BEHIND) continue;

					}
					child_list[n_children++] = c;
				}
			}

			//now order the sides in some magical way

			if (new_seg_sorting)
				sort_seg_children(seg,n_children,child_list);

			//for (c=0;c<MAX_SIDES_PER_SEGMENT;c++)	{
			//	ch=seg->children[c];

			for (c=0;c<n_children;c++) {
				int siden;

				siden = child_list[c];
				ch=seg->children[siden];
				//if ( (window_check || !visited[ch])&& (WALL_IS_DOORWAY(seg, c))) {
				{
					if (window_check) {
						int i;
						ubyte codes_and_3d,codes_and_2d;
						short _x,_y,min_x=32767,max_x=-32767,min_y=32767,max_y=-32767;
						int no_proj_flag=0;	//a point wasn't projected

						if (rotated<2) {
							if (!rotated)
								rotate_list(8,seg->verts);	// ASCENT: was &seg->verts (pointer to the array)
							project_list(8,seg->verts);	// ASCENT: was &seg->verts (pointer to the array)
							rotated=2;
						}

						for (i=0,codes_and_3d=codes_and_2d=0xff;i<4;i++) {
							int p = seg->verts[Side_to_verts[siden][i]];
							g3s_point *pnt = &Segment_points[p];

							if (! (pnt->p3_flags&PF_PROJECTED)) {no_proj_flag=1; break;}

							_x = f2i(pnt->p3_sx);
							_y = f2i(pnt->p3_sy);

							codes_and_3d &= pnt->p3_codes;
							codes_and_2d &= code_window_point(_x,_y,check_w);

							#ifndef NDEBUG
							if (draw_edges) {
								gr_setcolor(BM_XRGB(31,0,31));
								gr_line(pnt->p3_sx,pnt->p3_sy,
									Segment_points[seg->verts[Side_to_verts[siden][(i+1)%4]]].p3_sx,
									Segment_points[seg->verts[Side_to_verts[siden][(i+1)%4]]].p3_sy);
							}
							#endif

							if (_x < min_x) min_x = _x;
							if (_x > max_x) max_x = _x;

							if (_y < min_y) min_y = _y;
							if (_y > max_y) max_y = _y;

						}

						#ifndef NDEBUG
						if (draw_boxes)
							draw_window_box(WHITE,min_x,min_y,max_x,max_y);
						#endif

						if (no_proj_flag || (!codes_and_3d && !codes_and_2d)) {	//maybe add this segment
							int rp = render_pos[ch];
							window *new_w = &render_windows[lcnt];

							if (no_proj_flag) *new_w = *check_w;
							else {
								new_w->left  = max(check_w->left,min_x);
								new_w->right = min(check_w->right,max_x);
								new_w->top   = max(check_w->top,min_y);
								new_w->bot   = min(check_w->bot,max_y);
							}

							//see if this seg already visited, and if so, does current window
							//expand the old window?
							if (rp != -1) {
								if (new_w->left < render_windows[rp].left ||
										 new_w->top < render_windows[rp].top ||
										 new_w->right > render_windows[rp].right ||
										 new_w->bot > render_windows[rp].bot) {

									new_w->left  = min(new_w->left,render_windows[rp].left);
									new_w->right = max(new_w->right,render_windows[rp].right);
									new_w->top   = min(new_w->top,render_windows[rp].top);
									new_w->bot   = max(new_w->bot,render_windows[rp].bot);

									if (no_migrate_segs) {
										//no_render_flag[lcnt] = 1;
										Render_list[lcnt] = -1;
										render_windows[rp] = *new_w;		//get updated window
										processed[rp] = 0;		//force reprocess
										goto no_add;
									}
									else
										Render_list[rp]=-1;
								}
								else goto no_add;
							}

							#ifndef NDEBUG
							if (draw_boxes)
								draw_window_box(5,new_w->left,new_w->top,new_w->right,new_w->bot);
							#endif

							render_pos[ch] = lcnt;
							Render_list[lcnt] = ch;
							Seg_depth[lcnt] = l;
							lcnt++;
							if (lcnt >= Render_seg_limit) {mprintf((0,"Too many segs in render list!!\n")); goto done_list;}	// ASCENT: was MAX_RENDER_SEGS
							visited[ch] = 1;

							#ifndef NDEBUG
							if (pre_draw_segs)
								render_segment(ch);
							#endif
no_add:
	;

						}
					}
					else {
						Render_list[lcnt] = ch;
						Seg_depth[lcnt] = l;
						lcnt++;
						if (lcnt >= Render_seg_limit) {mprintf((0,"Too many segs in render list!!\n")); goto done_list;}	// ASCENT: was MAX_RENDER_SEGS
						visited[ch] = 1;
					}
				}
			}
		}

		scnt = ecnt;
		ecnt = lcnt;

		if (Render_full_depth && n_processed == 0)
			break;

	}
done_list:

	// ASCENT: see Render_orig_visible
	if (Orig_list_end < 0 || Orig_list_end > lcnt)
		Orig_list_end = lcnt;
	if (Orig_list_end > MAX_RENDER_SEGS)
		Orig_list_end = MAX_RENDER_SEGS;

	lcnt_save = lcnt;
	scnt_save = scnt;

	first_terminal_seg = scnt;
	N_render_segs = lcnt;

}

//renders onto current canvas
void render_mine(int start_seg_num,fix eye_offset)
{
	int		i;
	int		nn;

	//	Initialize number of objects (actually, robots!) rendered this frame.
	Num_rendered_objects = 0;

	if (eye_offset <= 0 && !_search_mode)
		light_log_frame();		// ASCENT: -lightlog


#ifdef LASER_HACK
	Hack_nlasers = 0;
#endif

	#ifndef NDEBUG
	for (i=0;i<=Highest_object_index;i++)
		object_rendered[i] = 0;
	#endif

	//set up for rendering

	render_start_frame();


	#if defined(EDITOR) && !defined(NDEUBG)
	if (Show_only_curside) {
		rotate_list(8,Cursegp->verts);	// ASCENT: was &seg->verts (pointer to the array)
		render_side(Cursegp,Curside);
		goto done_rendering;
	}
	#endif


	#ifdef EDITOR
	if (_search_mode || eye_offset>0)	{
		//lcnt = lcnt_save;
		//scnt = scnt_save;
	}
	else
	#endif
		//NOTE LINK TO ABOVE!!
		build_segment_list(start_seg_num);		//fills in Render_list & N_render_segs

	//render away

	#ifndef NDEBUG
	if (!window_check) {
		Window_clip_left  = Window_clip_top = 0;
		Window_clip_right = grd_curcanv->cv_bitmap.bm_w-1;
		Window_clip_bot   = grd_curcanv->cv_bitmap.bm_h-1;
	}
	#endif

	#ifndef NDEBUG
	if (!(_search_mode || eye_offset>0)) {
		int i;

		for (i=0;i<N_render_segs;i++) {
			int segnum;

			segnum = Render_list[i];

			if (segnum != -1)
				if (visited2[segnum])
					Int3();		//get Matt
				else
					visited2[segnum] = 1;
		}
	}
	#endif

//	if (!(_search_mode || eye_offset>0) && migrate_objects)
	if (!(_search_mode))
		build_object_lists(N_render_segs);

	if (eye_offset<=0)		// Do for left eye or zero.
		set_dynamic_light();

	if (!_search_mode && Clear_window == 2) {
		if (first_terminal_seg < N_render_segs) {
			int i;

			if (Clear_window_color == -1)
				Clear_window_color = BM_XRGB(0, 0, 0);	//BM_XRGB(31, 15, 7);
	
			gr_setcolor(Clear_window_color);
	
			for (i=first_terminal_seg; i<N_render_segs; i++) {
				if (Render_list[i] != -1) {
					#ifndef NDEBUG
					if ((render_windows[i].left == -1) || (render_windows[i].top == -1) || (render_windows[i].right == -1) || (render_windows[i].bot == -1))
						Int3();
					else
					#endif
						//NOTE LINK TO ABOVE!
						gr_rect(render_windows[i].left, render_windows[i].top, render_windows[i].right, render_windows[i].bot);
				}
			}
		}
	}

	for (nn=N_render_segs;nn--;) {
		int segnum;
		int objnp;

		// Interpolation_method = 0;
		segnum = Render_list[nn];
		Current_seg_depth = Seg_depth[nn];

		//if (!no_render_flag[nn])
		if (segnum!=-1 && (_search_mode || eye_offset>0 || visited[segnum]!=255)) {
			//set global render window vars

			if (window_check) {
				Window_clip_left  = render_windows[nn].left;
				Window_clip_top   = render_windows[nn].top;
				Window_clip_right = render_windows[nn].right;
				Window_clip_bot   = render_windows[nn].bot;
			}

			//mprintf((0," %d",segnum));

			Render_orig_visible = nn < Orig_list_end;		// ASCENT

			render_segment(segnum); 
			visited[segnum]=255;

			if (window_check) {		//reset for objects
				Window_clip_left  = Window_clip_top = 0;
				Window_clip_right = grd_curcanv->cv_bitmap.bm_w-1;
				Window_clip_bot   = grd_curcanv->cv_bitmap.bm_h-1;
			}

			if (migrate_objects) {
				//int n_expl_objs=0,expl_objs[5],i;
				int listnum;
				int save_linear_depth = Max_linear_depth;

				Max_linear_depth = Max_linear_depth_objects;

				listnum = nn;

				//mprintf((0,"render objs seg %d",segnum));

				for (objnp=0;render_obj_list[listnum][objnp]!=-1;)	{
					int ObjNumber = render_obj_list[listnum][objnp];

					if (ObjNumber >= 0) {

						//mprintf( (0, "Type: %d\n", Objects[ObjNumber].type ));
	
						//if (Objects[ObjNumber].type == OBJ_FIREBALL && n_expl_objs<5)	{
						//	expl_objs[n_expl_objs++] = ObjNumber;
	 					//} else
						#ifdef LASER_HACK
						if ( 	(Objects[ObjNumber].type==OBJ_WEAPON) && 								//if its a weapon
								(Objects[ObjNumber].lifeleft==Laser_max_time ) && 	//  and its in it's first frame
								(Hack_nlasers< MAX_HACKED_LASERS) && 									//  and we have space for it
								(Objects[ObjNumber].laser_info.parent_num>-1) &&					//  and it has a parent
								((Viewer-Objects)==Objects[ObjNumber].laser_info.parent_num)	//  and it's parent is the viewer
						   )		{
							Hack_laser_list[Hack_nlasers++] = ObjNumber;								//then make it draw after everything else.
							//mprintf( (0, "O%d ", ObjNumber ));
						} else	
						#endif
							do_render_object(ObjNumber);	// note link to above else

						objnp++;
					}
					else {

						listnum = -ObjNumber;
						objnp = 0;

					}

				}

				//for (i=0;i<n_expl_objs;i++)
				//	do_render_object(expl_objs[i]);

				//mprintf((0,"done seg %d\n",segnum));

				Max_linear_depth = save_linear_depth;

			}

		}
	}

	Render_orig_visible = 1;		// ASCENT

	//mprintf((0,"\n"));

								
#ifdef LASER_HACK								
	// Draw the hacked lasers last
	for (i=0; i < Hack_nlasers; i++ )	{
		//mprintf( (0, "D%d ", Hack_laser_list[i] ));
		do_render_object(Hack_laser_list[i]);
	}
#endif

	// -- commented out by mk on 09/14/94...did i do a good thing??  object_render_targets();

#ifdef EDITOR
	#ifndef NDEUBG
	//draw curedge stuff
	if (Outline_mode) outline_seg_side(Cursegp,Curside,Curedge,Curvert);
	#endif

done_rendering:
	;

#endif

}
#ifdef EDITOR

extern int render_3d_in_big_window;

//finds what segment is at a given x&y -  seg,side,face are filled in
//works on last frame rendered. returns true if found
//if seg<0, then an object was found, and the object number is -seg-1
int find_seg_side_face(short x,short y,int *seg,int *side,int *face,int *poly)
{
	_search_mode = -1;

	_search_x = x; _search_y = y;

	found_seg = -1;

	if (render_3d_in_big_window) {
		grs_canvas temp_canvas;

		gr_init_sub_canvas(&temp_canvas,canv_offscreen,0,0,
			LargeView.ev_canv->cv_bitmap.bm_w,LargeView.ev_canv->cv_bitmap.bm_h);

		gr_set_current_canvas(&temp_canvas);

		render_frame(0);
	}
	else {
		gr_set_current_canvas(&VR_render_sub_buffer[0]);	//render off-screen
		render_frame(0);
	}

	_search_mode = 0;

	*seg = found_seg;
	*side = found_side;
	*face = found_face;
	*poly = found_poly;

//	mprintf((0,"found seg=%d, side=%d, face=%d, poly=%d\n",found_seg,found_side,found_face,found_poly));

	return (found_seg!=-1);

}

#endif
