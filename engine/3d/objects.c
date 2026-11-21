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
 * ASCENT: this file replaces objects.asm.
 *
 * Code to draw a bitmap object (an object that has a bitmap for each view
 * direction).
 *
 * ASCENT: objects.asm was not a part of 3d.lib (see 3D_OBJS in the
 * makefile), and the game does not call g3_draw_object(). The routine
 * needs render_object() with three parameters and the view tables of
 * upvecs.c and vecview.c. The game has a different render_object(). So
 * the build does not compile this file, the same as the original build.
 * The code is here to keep the port of the directory complete.
 */

#include "globvars.h"

static vms_matrix new_obj_matrix;

static vms_vector obj_vec_g;			//object->eye, global frame
static vms_vector obj_vec_obj;		//in objects frame of ref.

static vms_matrix vec_matrix_g;

//3d points for corners of bitmap
static vms_vector up_left_v,up_right_v,down_right_v;

static g3s_point up_left_p,up_right_p,down_right_p;

static g3s_point *bitmap_points[] = {&up_left_p,&up_right_p,&down_right_p};

static int bitmap_number;

static vms_vector up_vec,right_vec;

int draw_bitmap_lines = 0;

extern char Vector_to_viewnum[],Vector_to_viewnum2[];
extern vms_vector up_vecs[],up_vecs2[];

extern void render_object(int obj_type,int bitmap_number,g3s_point **bitmap_points);

//get five bits of a vector component, which is in the range -1..1
static int view_index_bits(fix a)
{
	a += f1_0;		//make in range 0..2

	if (a >= f2_0)
		a--;			//make less than 2

	return (a >> 12) & 0x1f;
}

//draws a bitmap object.  takes obj type, pos, matrix, size
void g3_draw_object(int obj_type,vms_vector *obj_pos,vms_matrix *obj_matrix,fix size)
{
	int index;
	vms_vector *up;

	//get vector from object to eye

	vm_vec_sub(&obj_vec_g,&View_position,obj_pos);

	//normalize vector

	vm_vec_normalize(&obj_vec_g);

	//rotate into object's frame of reference

	vm_vec_rotate(&obj_vec_obj,&obj_vec_g,obj_matrix);

	//select the right bitmap by looking in a table.
	//get five bits for each of x,y,z, make a 15-bit index, and look in table

	index = (view_index_bits(obj_vec_obj.x) << 10) |
			  (view_index_bits(obj_vec_obj.y) << 5) |
			   view_index_bits(obj_vec_obj.z);

	if (obj_type == 1) {		//other object?
		bitmap_number = (ubyte) Vector_to_viewnum2[index];
		up = &up_vecs2[bitmap_number];
	}
	else {
		bitmap_number = (ubyte) Vector_to_viewnum[index];
		up = &up_vecs[bitmap_number];
	}

	//build matrix with vec from obj to eye & bitmap's up vector
	//(the assembly gave a register with no pointer in it as the right vector)

	vm_vector_2_matrix(&vec_matrix_g,&obj_vec_obj,up,NULL);

	//rotate object's matrix through new matrix

	vm_matrix_x_matrix(&new_obj_matrix,obj_matrix,&vec_matrix_g);

	//now, up and right describe edges

	up_vec = new_obj_matrix.uvec;
	right_vec = new_obj_matrix.rvec;

	//calculate corner points of bitmap

	vm_vec_scale(&up_vec,size >> 1);
	vm_vec_scale(&right_vec,size >> 1);

	//upper left

	vm_vec_add(&up_left_v,obj_pos,&right_vec);
	vm_vec_add2(&up_left_v,&up_vec);

	//upper right

	vm_vec_sub(&up_right_v,&up_left_v,&right_vec);
	vm_vec_sub2(&up_right_v,&right_vec);

	//lower right

	vm_vec_sub(&down_right_v,&up_right_v,&up_vec);
	vm_vec_sub2(&down_right_v,&up_vec);

	//now take three points, rotate & project

	if (g3_rotate_point(&up_left_p,&up_left_v) & CC_BEHIND)
		return;

	if (g3_rotate_point(&up_right_p,&up_right_v) & CC_BEHIND)
		return;

	if (g3_rotate_point(&down_right_p,&down_right_v) & CC_BEHIND)
		return;

	//set all z values same before project

	up_right_p.z = down_right_p.z = up_left_p.z;

	g3_project_point(&up_left_p);
	g3_project_point(&up_right_p);
	g3_project_point(&down_right_p);

	render_object(obj_type,bitmap_number,bitmap_points);

	if (draw_bitmap_lines) {
		gr_setcolor(15);

		gr_line(up_left_p.p3_sx,up_left_p.p3_sy,up_right_p.p3_sx,up_right_p.p3_sy);
		gr_line(down_right_p.p3_sx,down_right_p.p3_sy,up_right_p.p3_sx,up_right_p.p3_sy);
	}
}
