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
 * $Source: f:/miner/source/3d/rcs/3d.h $
 * $Revision: 1.34 $
 * $Author: matt $
 * $Date: 1994/11/11 19:22:14 $
 *
 * Header file for 3d library
 *
 * $Log: 3d.h $
 * Revision 1.34  1994/11/11  19:22:14  matt
 * Added new function, g3_calc_point_depth()
 * 
 * Revision 1.33  1994/09/09  14:23:58  matt
 * Added support for glowing textures, to add engine glow to Descent.
 * 
 * Revision 1.32  1994/09/01  10:42:27  matt
 * Blob routine, renamed g3_draw_bitmap(), now takes seperate 3d width & height.
 * 
 * Revision 1.31  1994/07/29  18:16:14  matt
 * Added instance by angles, and corrected parms for g3_init()
 * 
 * Revision 1.30  1994/07/25  00:00:00  matt
 * Made 3d no longer deal with point numbers, but only with pointers.
 * 
 * Revision 1.29  1994/07/22  17:57:27  matt
 * Changed the name of the rod functions, and took out some debugging code
 * 
 * Revision 1.28  1994/06/07  16:49:12  matt
 * Made interpreter take lighting value as parm, rather than in global var
 * 
 * Revision 1.27  1994/05/31  18:35:28  matt
 * Added light value to g3_draw_facing_bitmap()
 * 
 * Revision 1.26  1994/05/30  22:48:04  matt
 * Added support for morph effect
 * 
 * Revision 1.25  1994/05/30  11:34:57  matt
 * Added g3_set_special_render() to allow a user to specify functions to
 * call for 2d draws.
 * 
 * Revision 1.24  1994/05/19  21:46:31  matt
 * Moved texture lighting out of 3d and into the game
 * 
 * Revision 1.23  1994/05/14  15:26:48  matt
 * Added extern for polyobj outline flag
 * 
 * Revision 1.22  1994/04/19  18:26:33  matt
 * Added g3_draw_sphere() function.
 * 
 * Revision 1.21  1994/03/25  18:22:28  matt
 * g3_draw_polygon_model() now takes ptr to list of angles
 * 
 * Revision 1.20  1994/03/15  21:23:23  matt
 * Added interpreter functions
 * 
 * Revision 1.19  1994/02/15  17:37:34  matt
 * New function, g3_draw_blob()
 * 
 * Revision 1.18  1994/02/09  11:47:47  matt
 * Added rod & delta point functions
 * 
 * Revision 1.17  1994/01/26  12:38:11  matt
 * Added function g3_compute_lighting_value()
 * 
 * Revision 1.16  1994/01/25  18:00:02  yuan
 * Fixed variable beam_brightness...
 * 
 * Revision 1.15  1994/01/24  14:08:34  matt
 * Added instancing functions
 * 
 * Revision 1.14  1994/01/22  18:21:48  matt
 * New lighting stuff now done in 3d; g3_draw_tmap() takes lighting parm
 * 
 * Revision 1.13  1994/01/20  17:21:24  matt
 * New function g3_compute_sky_polygon()
 * 
 * Revision 1.12  1994/01/14  17:20:25  matt
 * Added prototype for new function g3_draw_horizon()
 * 
 * Revision 1.10  1993/12/20  20:21:52  matt
 * Added g3_point_2_vec()
 * 
 * Revision 1.9  1993/12/07  23:05:47  matt
 * Fixed mistyped function name.
 * 
 * Revision 1.8  1993/12/05  23:47:03  matt
 * Added function g3_draw_line_ptrs()
 * 
 * Revision 1.7  1993/12/05  23:13:22  matt
 * Added prototypes for g3_rotate_point() and g3_project_point()
 * 
 * Revision 1.6  1993/12/05  23:03:28  matt
 * Changed uvl structs to g3s_uvl
 * 
 * Revision 1.5  1993/11/22  10:51:09  matt
 * Moved uvl structure here from segment.h, made texture map functions use it
 * 
 * Revision 1.4  1993/11/21  20:08:31  matt
 * Added function g3_draw_object()
 * 
 * Revision 1.3  1993/11/04  18:49:19  matt
 * Added system to only rotate points once per frame
 * 
 * Revision 1.2  1993/11/04  08:16:06  mike
 * Add light field (p3_l) to g3s_point.
 * 
 * Revision 1.1  1993/10/29  22:20:56  matt
 * Initial revision
 * 
 *
 */

#ifndef _3D_H
#define _3D_H

#include "fix.h"
#include "vecmat.h"	//the vector/matrix library
#include "gr.h"

extern int g3d_interp_outline;		//if on, polygon models outlined in white

//Structure for storing u,v,light values.  This structure doesn't have a
//prefix because it was defined somewhere else before it was moved here
typedef struct g3s_uvl {
	fix u,v,l;
} g3s_uvl;

//Stucture to store clipping codes in a word
typedef struct g3s_codes {
	ubyte or,and;	//or is low byte, and is high byte
} g3s_codes;

//flags for point structure
#define PF_PROJECTED 	1	//has been projected, so sx,sy valid
#define PF_OVERFLOW		2	//can't project
#define PF_TEMP_POINT	4	//created during clip
#define PF_UVS				8	//has uv values set
#define PF_LVS				16	//has lighting values set (ASCENT: from 3d.inc)

//clipping codes flags

#define CC_OFF_LEFT	1
#define CC_OFF_RIGHT	2
#define CC_OFF_BOT	4
#define CC_OFF_TOP	8
#define CC_BEHIND		0x80

//Used to store rotated points for mines.  Has frame count to indictate
//if rotated, and flag to indicate if projected.
typedef struct g3s_point {
	union {                   //post rotation point
		vms_vector p3_vec;         //reference as vector...
		struct {fix x,y,z;};   //...or by element name
		fix xyz[3];            //...or by element number
	};
	union {					//u,v,light values for texture mapper
		struct {fix p3_u,p3_v,p3_l;};
		g3s_uvl p3_uvl;
	};
	fix p3_sx,p3_sy;		//screen x&y
	ubyte p3_codes;		//clipping codes
	ubyte p3_flags;		//projected?
	short p3_pad;			//keep structure longwork aligned
} g3s_point;

//An object, such as a robot
typedef struct g3s_object {
	vms_vector o3_pos;       //location of this object
	vms_angvec o3_orient;    //orientation of this object
	int o3_nverts;           //number of points in the object
	int o3_nfaces;           //number of faces in the object

	//this will be filled in later

} g3s_object;

//Functions in library

//3d system startup and shutdown:

//initialize the 3d system
void g3_init(void);

//close down the 3d system
void g3_close(void);


//Frame setup functions:

//start the frame
void g3_start_frame(void);

//set view from x,y,z & p,b,h, zoom.  Must call one of g3_set_view_*() 
void g3_set_view_angles(vms_vector *view_pos,vms_angvec *view_orient,fix zoom);

//set view from x,y,z, viewer matrix, and zoom.  Must call one of g3_set_view_*() 
void g3_set_view_matrix(vms_vector *view_pos,vms_matrix *view_matrix,fix zoom);

//end the frame
void g3_end_frame(void);

//draw a horizon
void g3_draw_horizon(int sky_color,int ground_color);

//get vectors that are edge of horizon
int g3_compute_sky_polygon(fix *points_2d,vms_vector *vecs);

//Instancing

//instance at specified point with specified orientation
void g3_start_instance_matrix(vms_vector *pos,vms_matrix *orient);

//instance at specified point with specified orientation
void g3_start_instance_angles(vms_vector *pos,vms_angvec *angles);

//pops the old context
void g3_done_instance(void);

//ASCENT: new, see instance.c. The view position and view matrix of the
//current instance.
typedef struct g3s_instance_state {
	vms_vector view_pos;
	vms_matrix view_matrix;
} g3s_instance_state;

void g3_get_instance_state(g3s_instance_state *state);
void g3_set_instance_state(g3s_instance_state *state);

//Misc utility functions:

//get current field of view.  Fills in angle for x & y
void g3_get_FOV(fixang *fov_x,fixang *fov_y);

//get zoom.  For a given window size, return the zoom which will achieve
//the given FOV along the given axis.
fix g3_get_zoom(char axis,fixang fov,short window_width,short window_height);

//returns the normalized, unscaled view vectors
void g3_get_view_vectors(vms_vector *forward,vms_vector *up,vms_vector *right);

//returns true if a plane is facing the viewer. takes the unrotated surface 
//normal of the plane, and a point on it.  The normal need not be normalized
bool g3_check_normal_facing(vms_vector *v,vms_vector *norm);

//Point definition and rotation functions:

//specify the arrays refered to by the 'pointlist' parms in the following
//functions.  I'm not sure if we will keep this function, but I need
//it now.
//void g3_set_points(g3s_point *points,vms_vector *vecs);

//returns codes_and & codes_or of a list of points numbers
g3s_codes g3_check_codes(int nv,g3s_point **pointlist);

//rotates a point. returns codes.  does not check if already rotated
ubyte g3_rotate_point(g3s_point *dest,vms_vector *src);

//projects a point
void g3_project_point(g3s_point *point);

//calculate the depth of a point - returns the z coord of the rotated point
fix g3_calc_point_depth(vms_vector *pnt);

//from a 2d point, compute the vector through that point
void g3_point_2_vec(vms_vector *v,short sx,short sy);

//code a point.  fills in the p3_codes field of the point, and returns the codes
ubyte g3_code_point(g3s_point *point);

//delta rotation functions
vms_vector *g3_rotate_delta_x(vms_vector *dest,fix dx);
vms_vector *g3_rotate_delta_y(vms_vector *dest,fix dy);
vms_vector *g3_rotate_delta_z(vms_vector *dest,fix dz);
vms_vector *g3_rotate_delta_vec(vms_vector *dest,vms_vector *src);
ubyte g3_add_delta_vec(g3s_point *dest,g3s_point *src,vms_vector *deltav);

//Drawing functions:

//draw a flat-shaded face.
//returns 1 if off screen, 0 if drew
bool g3_draw_poly(int nv,g3s_point **pointlist);

//draw a texture-mapped face.
//returns 1 if off screen, 0 if drew
bool g3_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm);

//draw a sortof sphere - i.e., the 2d radius is proportional to the 3d
//radius, but not to the distance from the eye
void g3_draw_sphere(g3s_point *pnt,fix rad);

//@@//return ligting value for a point
//@@fix g3_compute_lighting_value(g3s_point *rotated_point,fix normval);


//like g3_draw_poly(), but checks to see if facing.  If surface normal is
//NULL, this routine must compute it, which will be slow.  It is better to 
//pre-compute the normal, and pass it to this function.  When the normal
//is passed, this function works like g3_check_normal_facing() plus
//g3_draw_poly().
//returns -1 if not facing, 1 if off screen, 0 if drew
bool g3_check_and_draw_poly(int nv,g3s_point **pointlist,vms_vector *norm,vms_vector *pnt);
bool g3_check_and_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm,vms_vector *norm,vms_vector *pnt);

//draws a line. takes two points.
bool g3_draw_line(g3s_point *p0,g3s_point *p1);

//draw a polygon that is always facing you
void g3_draw_rod_flat(g3s_point *bot_point,fix bot_width,g3s_point *top_point,fix top_width);

//draw a bitmap object that is always facing you
void g3_draw_rod_tmap(grs_bitmap *bitmap,g3s_point *bot_point,fix bot_width,g3s_point *top_point,fix top_width,fix light);

//draws a bitmap with the specified 3d width & height 
void g3_draw_bitmap(vms_vector *pos,fix width,fix height,grs_bitmap *bm);

//specifies 2d drawing routines to use instead of defaults.  Passing
//NULL for either or both restores defaults
//ASCENT: the drawers have real function types. The original prototype
//took untyped function pointers.
typedef void g3_tmap_drawer_func(grs_bitmap *bm,int nv,g3s_point **vertlist);
typedef void g3_flat_drawer_func(int nv,int *vertlist);
typedef int g3_line_drawer_func(fix x0,fix y0,fix x1,fix y1);
void g3_set_special_render(g3_tmap_drawer_func *tmap_drawer,g3_flat_drawer_func *flat_drawer,g3_line_drawer_func *line_drawer);

//Object functions:

//gives the interpreter an array of points to use
void g3_set_interp_points(g3s_point *pointlist);

//ASCENT: gives the interpreter the corner normals of a model (polyobj.c,
//polyobj_make_smooth), or NULL for the light of the faces. data is the
//start of the model data.
void g3_set_interp_smooth(ubyte *data,ubyte *smooth);

//calls the object interpreter to render an object.  The object renderer
//is really a seperate pipeline. returns true if drew
bool g3_draw_polygon_model(void *model_ptr,grs_bitmap **model_bitmaps,vms_angvec *anim_angles,fix light,fix *glow_values);

//init code for bitmap models
void g3_init_polygon_model(void *model_ptr);

//alternate interpreter for morphing object
bool g3_draw_morphing_model(void *model_ptr,grs_bitmap **model_bitmaps,vms_angvec *anim_angles,fix light,vms_vector *new_points);

//ASCENT: the #pragma aux lines (register calling conventions for the
//assembly routines) are removed. All the functions are C functions now.
//The functions that had no return type (implicit int, with no defined
//value in the assembly) return void.

//ASCENT: the GPU renderer (gpu3d.c). The software path is used when the
//GPU is not available or not enabled.

//The source of a bitmap for the upscaled texture set (see
//g3_gpu_options). kind 1: the bitmap egg/name of an EGG archive (lower
//case, '#' of an animation frame replaced by '_'). kind 2: a merged wall
//texture (texmerge.c): top drawn over bottom with the rotation orient.
typedef struct g3_gpu_source {
	int kind;
	char egg[16];
	char name[20];
	grs_bitmap *bottom,*top;
	int orient;
} g3_gpu_source;

//The settings of the GPU view, read at each g3_gpu_begin_view().
typedef struct g3_gpu_options {
	int rgb;				//the RGB form (else the 8-bit form, platform.h)
	int anisotropic;		//RGB: mipmaps and 8x anisotropic filtering
	int ssao;				//RGB: ambient occlusion (ASCENT, platform.h)
	int upscaled;			//RGB: the textures of the upscaled set when they exist
	const char *hd_dir;		//the directory of the upscaled files (<hd>/textures/<egg>/<name>.png), or NULL
	const ubyte *palette;	//the palette of the bitmaps (gr_palette)
	//fills src for a bitmap, returns 0 if the bitmap has no source
	int (*source)(grs_bitmap *bm,g3_gpu_source *src);
} g3_gpu_options;
void g3_gpu_set_options(const g3_gpu_options *o);
//Make the RGB texture of a bitmap now and ask for its upscaled form, so
//that the first view that shows the bitmap has the upscaled form (the
//frames of a door animation, the texture of a switch). Only with the RGB
//form and the upscaled set (g3_gpu_set_options). Returns 1 if the bitmap
//has a texture.
int g3_gpu_preload(grs_bitmap *bm);
//Changes each time that the RGB textures must be made again (the palette,
//the upscaled setting or the directory of the set changed).
int g3_gpu_texture_generation(void);

//Start the 3D view on the current canvas. enable 0 selects the software
//renderer. The GPU image is canvas width * scale by canvas height *
//line_mul * scale pixels (line_mul is 2 when each line of the canvas is
//shown two times), except for scale 1: then the image is the size of the
//canvas, the original resolution (each line shown line_mul times), and
//scale 0: half of canvas width by canvas height * line_mul (ASCENT).
//depth_test 1: per pixel depth test. clear_color: the palette index of
//the background. Returns 1 if the GPU draws the view:
//then the 3d drawers on this canvas go to the GPU until g3_gpu_end_view(),
//and the canvas is filled with color 255, the color that shows the GPU
//image when the frame is shown.
int g3_gpu_begin_view(int enable,int scale,int line_mul,int depth_test,int clear_color);
//The polygons after this call are drawn over the earlier ones.
void g3_gpu_clear_depth(void);
void g3_gpu_end_view(void);
//The view canvas is shown at (x,y) in the screen with w by h pixels.
//Call it each time the canvas is copied to the screen.
void g3_gpu_view_dest(int x,int y,int w,int h);
//Darken the GPU image in a rectangle of screen pixels (x1, y1 not
//included) with the fade table row level, as the 2d code darkens the
//pixels of the screen (the fade table keeps color 255, the color that
//shows the GPU image). Call it after g3_gpu_view_dest() of the frame.
void g3_gpu_view_darken(int x0,int y0,int x1,int y1,int level);
//The heat of the polygons drawn now, for the improved infravision of the
//GPU renderer (PLAT_GPU_HEAT_* of platform.h: 0 cold, 1 warm, 2 hot, 3 glow). The
//game sets it before it draws an object or a face and sets 0 after.
extern int G3_gpu_heat;
//1 while the game draws a face of a side with no segment behind it: the
//GPU draws its transparent texels opaque (they look into the void; 1.3
//shows the frames before there, the GPU showed black).
extern int G3_gpu_opaque;
//The pulsing fade table row of the effect color 0xCF (Tmap_fx_row of the
//texture mapper, tmapfx.c). The game sets it at the start of each view.
extern int G3_gpu_fx_row;
//ASCENT: 0, or the row between two steps of the pulse: 1 + 256 times the
//row (plat_gpu_material fx_fine). The RGB form mixes the two rows.
extern int G3_gpu_fx_fine;
//The dark view of the conversation window (Tmap_dim of the texture mapper,
//tmapfx.c): 0 to 15 for the lit textures. The game sets it at the start
//of each view.
extern int G3_gpu_dim;
//The tint of the current object for the GPU renderer (texmap/tmapfx.c,
//Tmap_tint; 0x40: Detect Magic). The game sets both.
extern int G3_gpu_tint;

#endif

