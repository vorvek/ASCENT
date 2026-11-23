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
 * $Source: f:/miner/source/main/rcs/polyobj.h $
 * $Revision: 2.1 $
 * $Author: john $
 * $Date: 1995/02/27 18:21:54 $
 * 
 * Header for polyobj.c, the polygon object code
 * 
 * $Log: polyobj.h $
 * Revision 2.1  1995/02/27  18:21:54  john
 * Added extern for robot_points.
 * 
 * Revision 2.0  1995/02/27  11:29:58  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.28  1995/01/12  12:10:16  adam
 * *** empty log message ***
 * 
 * Revision 1.27  1994/11/11  19:28:58  matt
 * Added code to show low detail polygon models
 * 
 * Revision 1.26  1994/11/10  14:03:05  matt
 * Hacked in support for player ships with different textures
 * 
 * Revision 1.25  1994/11/02  16:18:24  matt
 * Moved draw_model_picture() out of editor
 * 
 * Revision 1.24  1994/10/18  10:55:57  adam
 * bumped MAX_POLYGON_OBJECTS up
 * 
 * Revision 1.23  1994/10/17  21:35:01  matt
 * Added support for new Control Center/Main Reactor
 * 
 * Revision 1.22  1994/09/09  14:23:42  matt
 * Added glow code to polygon models for engine glow
 * 
 * Revision 1.21  1994/08/26  18:03:43  matt
 * Added code to remap polygon model numbers by matching filenames
 * 
 * Revision 1.20  1994/08/26  15:36:00  matt
 * Made eclips usable on more than one object at a time
 * 
 * Revision 1.19  1994/07/22  20:44:23  matt
 * Killed unused fields in polygon model structure
 * 
 * Revision 1.18  1994/06/16  17:52:11  matt
 * Made submodels rotate around their centers, not their pivot points
 * 
 * Revision 1.17  1994/06/14  12:22:05  matt
 * Integrated with drive code, using #defines to switch versions
 * 
 * Revision 1.16  1994/06/09  16:25:01  matt
 * Fixed confusion with two constants, MAX_SUBOBJS & MAX_SUBMODELS, which
 * were used for the same things, but had different values.
 * 
 * Revision 1.15  1994/06/08  10:56:38  matt
 * Improved debris: now get submodel size from new POF files; debris now has
 * limited life; debris can now be blown up.
 * 
 * Revision 1.14  1994/06/07  16:51:57  matt
 * Made object lighting work correctly; changed name of Ambient_light to
 * Dynamic_light; cleaned up polygobj object rendering a little.
 * 
 * Revision 1.13  1994/05/26  21:08:59  matt
 * Moved robot stuff out of polygon model and into robot_info struct
 * Made new file, robot.c, to deal with robots
 * 
 * Revision 1.12  1994/05/18  19:35:05  matt
 * Added fields for the rest of the subobj data
 * 
 * Revision 1.11  1994/05/16  16:17:13  john
 * Bunch of stuff on my Inferno Task list May16-23
 * 
 * Revision 1.10  1994/05/13  11:08:31  matt
 * Added support for multiple gun positions on polygon models
 * 
 * Revision 1.9  1994/04/29  09:18:04  matt
 * Added support for multiple-piece explosions
 * 
 * Revision 1.8  1994/04/28  18:44:18  matt
 * Took out code for old-style (non-interpreted) objects.
 * 
 * Revision 1.7  1994/03/25  16:54:38  matt
 * draw_polygon_object() now takes pointer to animation data
 * 
 * Revision 1.6  1994/03/15  17:44:33  matt
 * Changed a bunch of names
 * 
 * Revision 1.5  1994/03/07  20:02:29  matt
 * Added pointer to normals in polyobj struct
 * Added prototype for draw_polygon_object()
 * 
 * Revision 1.4  1994/03/01  17:16:19  matt
 * Lots of changes to support loadable binary ".pof" robot files
 * 
 * Revision 1.3  1994/01/31  15:51:20  matt
 * Added ptr for rgb table for robot colors
 * 
 * Revision 1.2  1994/01/28  13:52:01  matt
 * Added flesh to this previously skeletal file.
 * 
 * Revision 1.1  1994/01/28  13:47:42  matt
 * Initial revision
 * 
 * 
 */



#ifndef _POLYOBJ_H
#define _POLYOBJ_H

#include "vecmat.h"
#include "gr.h"
#include "3d.h"

#ifndef DRIVE
#include "robot.h"
#endif
#include "piggy.h"

// ASCENT: Descent 1 had 85 models and 10 submodels in a model. The
// constants of the asserts in DTUGAME.EXE are 160 and 64. Three archives
// of the disc that the game does not use (BASE, SPIDER, DEMO) have up to
// 238 models, so the limit here is 256. MAX_SUBMODELS (10, inferno.h)
// still sizes the animation angles of an object, so the arrays of the
// model have their own constant.
#ifndef DRIVE
#define MAX_POLYGON_MODELS 256
#else
#define MAX_POLYGON_MODELS 300
#define MAX_SUBMODELS 10
#endif
#define MAX_POLYMODEL_SUBMODELS	64

// ASCENT: the model header of an EGG archive and the number of arrays in
// the data block of a model. See docs/formats/egg.md.
#define DTU_MODEL_HEADER_SIZE		218
#define DTU_MODEL_SECTIONS			20		// section numbers 1 to 19 of egg.md

//used to describe a polygon model
typedef struct polymodel {
	int n_models;
	int model_data_size;
	ubyte *model_data;
	int submodel_ptrs[MAX_POLYMODEL_SUBMODELS];
	vms_vector submodel_offsets[MAX_POLYMODEL_SUBMODELS];
	vms_vector submodel_norms[MAX_POLYMODEL_SUBMODELS];		//norm for sep plane
	vms_vector submodel_pnts[MAX_POLYMODEL_SUBMODELS];		//point on sep plane
	fix submodel_rads[MAX_POLYMODEL_SUBMODELS];				//radius for each submodel
	ubyte submodel_parents[MAX_POLYMODEL_SUBMODELS];		//what is parent for each submodel
	vms_vector submodel_mins[MAX_POLYMODEL_SUBMODELS];
	vms_vector submodel_maxs[MAX_POLYMODEL_SUBMODELS];
	vms_vector mins,maxs;							//min,max for whole model
	fix rad;
	ubyte		n_textures;
	ushort	first_texture;
	ubyte		simpler_model;		//alternate model with less detail (0 if none, model_num+1 else)
//	vms_vector min,max;

	// ASCENT: the parts that Descent to Undermountain added. The loader
	// keeps the whole data block of the model and the header as they are in
	// the EGG archive, for the parts that are not decoded yet.
	int		n_frames;			// animation frames (header 0x34)
	ubyte		dtu_flags;			// header 0x3C: optional arrays in the block
	ubyte		dtu_bound;			// 1 after the table gave this model its textures
	ubyte		dtu_textures_used;	// highest texture number in the model data, plus 1
	ubyte		dtu_n_textures_file;	// n_textures of the header
	int		dtu_first_texture_file;	// first_texture of the header (not used, see polyobj.c)
	ubyte		*dtu_block;			// copy of the data block. model_data points to its start
	int		dtu_block_size;
	int		dtu_section_ofs[DTU_MODEL_SECTIONS];	// offset of each array in dtu_block, -1 if absent
	int		dtu_section_size[DTU_MODEL_SECTIONS];
	ubyte		dtu_header[DTU_MODEL_HEADER_SIZE];

	// ASCENT: the decoded arrays of the data block and the rest of the
	// header. The names are those of docs/re/models.md ("The model struct
	// in memory"). F is n_frames, n is n_models. All pointers point into
	// one block of memory (dtu_arrays). A pointer is NULL if the model
	// does not have the array.
	void			*dtu_arrays;
	vms_angvec	*anim_angles;			// F*n: entry frame*n + submodel
	vms_vector	*frame_eye;				// F: the y value is added to the eye height
	vms_vector	*box1_min, *box1_max;	// F+1: box of the whole body. Entry 0: rest pose. Entry f+1: frame f
	vms_vector	*box2_min, *box2_max;	// F: box of the weapon (box_flags & 2)
	vms_vector	*box4_min, *box4_max;	// F: box of the second weapon (box_flags & 4)
	vms_vector	*box8_min, *box8_max;	// F: box of the torso (box_flags & 8)
	vms_vector	*body_center;			// F+1, box_flags & 1
	fix			*body_half_height;	// F+1
	fix			*body_radius;			// F+1, in the xz plane
	vms_vector	*torso_center;			// F, box_flags & 8, relative to body_center
	fix			*torso_half_height;	// F
	fix			*torso_radius;			// F
	vms_vector	*weapon_center;		// F, box_flags & 2, relative to body_center
	fix			*weapon_radius;		// F
	vms_vector	*weapon2_center;		// F, box_flags & 4
	fix			*weapon2_radius;		// F
	ubyte			*submodel_flags;		// n, DTU_SUBMODEL_F_
	ubyte			*submodel_children;	// n rows of n bytes. Row s: the children of submodel s, ended by 0xFF
	ubyte			box_flags;				// DTU_BOX_ (the same value as dtu_flags)
	ubyte			gun_submodels[2];
	short			fire_frames[4];		// frame of the hit or shot in the states ATTACK, ATTACK2, 11, 12
	fix			speeds[9];
	fix			state_times[19];		// seconds, the index is the animation state
	int			n_guns;
	vms_vector	gun_points[2];
} polymodel;

#define DTU_BOX_BODY			1
#define DTU_BOX_WEAPON		2
#define DTU_BOX_WEAPON2		4
#define DTU_BOX_TORSO		8

#define DTU_SUBMODEL_F_PITCH		0x01		// the caller can add a pitch angle (always 0 in 1.3)
#define DTU_SUBMODEL_F_HIDDEN		0x10		// the missile in the hand
#define DTU_SUBMODEL_F_MAGIC		0x20		// the part that gets the magic effect

// ASCENT: the draw function of DTUGAME.EXE (0x727A5) for a model of an EGG
// archive. "frame" is the animation frame, or -1 for the pose of the model
// data. The position is the center of the body box of the frame (if the
// model has body boxes). A model with more than one submodel is drawn by
// the sorted routine (0x66E13). "show_hidden" draws the submodels with the
// flag DTU_SUBMODEL_F_HIDDEN.
void draw_polygon_model_frame(vms_vector *pos,vms_matrix *orient,int model_num,int frame,int show_hidden,fix light,fix *glow_values,bitmap_index alt_textures[]);

// ASCENT: animation interpolation (Dtu_anim_interp, dtuopts.h). The same
// draw with the joint angles and the body center between the frames
// "frame" and "next", frac (0 to 1.0) of the way to "next". Each angle
// takes the short way around. next -1 or frac 0 is draw_polygon_model_frame.
void draw_polygon_model_frame_lerp(vms_vector *pos,vms_matrix *orient,int model_num,int frame,int next,fix frac,int show_hidden,fix light,fix *glow_values,bitmap_index alt_textures[]);

// ASCENT: D-050. 1.3 draws each model but the one of the viewer with the
// linear texture mapper (0x675D4 to 0x675EC: Linear_tmap_polygon_objects,
// the byte 0x13D3E3, is 1 and no code writes it), which bends the textures
// of large polygons near the eye. Corrected (1, default): the software
// renderer draws all models with the perspective mapper. -origobj clears
// it. The GPU renderer is perspective correct in both cases.
extern int Dtu_fix_model_mapper;

// ASCENT: D-054, the texture slots of $WEAPON and $SPELL models (bmread.c).
// 1: the slots of the table line. 0 (-origobj): the header rule of 1.3.
extern int Dtu_fix_model_slots;

// ASCENT: the frame times that the draw of an animated model sees, for the
// interpolation. The game code sets the frame numbers as the original does
// (by the game time, by steps or by frames); the tracker learns the time
// that one frame of the current state lasts from the frame changes, and
// gives the next frame and the fraction of the way to it. A change of the
// state, or a frame change by more than 2 or against the direction of the
// state, starts again: the new frame is drawn at once, with no blend.
typedef struct polyobj_anim_track {
	int	key;				// the object (signature). A zeroed tracker is empty
	short	state, frame;
	short	dir;				// +1 or -1 once seen, 0 before
	short	n_adv;			// 0: no change seen, else 1 + the frames advanced since t_first
	fix	t_frame;			// the game time when "frame" began
	fix	t_first;			// the game time of the first frame change of the state
	fix	dur;				// the time of one frame, 0 if not known
} polyobj_anim_track;

// Updates the tracker with the frame that the game shows now and returns
// the fraction (0 to 1.0) of the way to *next. first and last are the
// first frame of the state and the first frame after it; est is the time
// of one frame if the caller knows it (else 0); def_dir is the direction
// of the state if no change was seen yet (+1, or -1 for a state that runs
// backward). *next is -1 when there is nothing to interpolate.
fix polyobj_anim_track_update(polyobj_anim_track *t,int key,int state,int frame,int first,int last,fix est,int def_dir,fix now,int *next);

// ASCENT: the offset from the origin of the model to the position of an
// object in the given frame, in the space of the model (body_center[frame+1],
// or zero for a model with no body boxes).
void polyobj_get_body_center(int model_num,int frame,vms_vector *center);

// ASCENT: load one model of an EGG archive. header points to the 218-byte
// header, block to the data block of the model, block_avail is the number
// of bytes from block to the end of the archive. Returns the model number,
// or -1 if the model is not valid.
int polyobj_load_dtu_model(const ubyte *header, const ubyte *block, int block_avail);

// ASCENT: find a loaded model by its POF name (no case). Returns -1 if none.
int polyobj_find_model(const char *name);

// ASCENT: the box of the points of a model in the frame (-1: the rest
// pose) as draw_polygon_model_frame() draws them, in the axes of the
// model, relative to the position of the object (the body center of the
// frame is taken away). The joint angles are applied, the hidden
// submodels count. Returns 0 if the model has no points.
int polyobj_frame_points_box(int model_num,int frame,vms_vector *mn,vms_vector *mx);

// ASCENT: release all models. The next model gets number 0.
void polyobj_free_all_models(void);

// ASCENT: animation angles of frame "frame" of a model: n_models angvecs
// in the memory of the model, or NULL if the model has no such frame.
// The bytes are in file order (3 x s16, little endian).
const ubyte *polyobj_get_frame_angles(int model_num, int frame);

//array of pointers to polygon objects
extern polymodel Polygon_models[];

//switch to simpler model when the object has depth 
//greater than this value times its radius.   
extern int Simple_model_threshhold_scale;

//how many polygon objects there are
extern int N_polygon_models;


//array of names of currently-loaded models
extern char Pof_names[MAX_POLYGON_MODELS][13];

int init_polygon_models();		// ASCENT: "int" added, the type was implicit

#ifndef DRIVE
int load_polygon_model(char *filename,int n_textures,int first_texture,robot_info *r);
#else
int load_polygon_model(char *filename,int n_textures,grs_bitmap ***textures);
#endif

//draw a polygon model
void draw_polygon_model(vms_vector *pos,vms_matrix *orient,vms_angvec *anim_angles,int model_num,int flags,fix light,fix *glow_values,bitmap_index alt_textures[]);

//fills in arrays gun_points & gun_dirs, returns the number of guns read
int read_model_guns(char *filename,vms_vector *gun_points, vms_vector *gun_dirs, int *gun_submodels);

//draws the given model in the current canvas.  The distance is set to
//more-or-less fill the canvas.  Note that this routine actually renders
//into an off-screen canvas that it creates, then copies to the current
//canvas.
int draw_model_picture(int mn,vms_angvec *orient_angles);		// ASCENT: "int" added, the type was implicit

#define MAX_POLYOBJ_TEXTURES 50
extern grs_bitmap *texture_list[MAX_POLYOBJ_TEXTURES];
extern bitmap_index texture_list_index[MAX_POLYOBJ_TEXTURES];
extern g3s_point robot_points[];

#endif
