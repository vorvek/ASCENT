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
 * ASCENT: this file replaces matrix.asm.
 *
 * Source for matrix setup & manipulation routines
 */

#include "globvars.h"

static void scale_matrix(void);

//set view from x,y,z, p,b,h, & zoom.
void g3_set_view_angles(vms_vector *view_pos,vms_angvec *view_orient,fix zoom)
{
	View_zoom = zoom;
	View_position = *view_pos;

	vm_angles_2_matrix(&View_matrix,view_orient);

	scale_matrix();
}

//set view from x,y,z, matrix, & zoom.
void g3_set_view_matrix(vms_vector *view_pos,vms_matrix *view_matrix,fix zoom)
{
	View_zoom = zoom;
	View_position = *view_pos;

	View_matrix = *view_matrix;

	scale_matrix();
}

//scale for aspect ratio, zoom, etc.
static void scale_matrix(void)
{
	fix s;

	Unscaled_matrix = View_matrix;		//save before scaling

	Matrix_scale = Window_scale;			//get aspect ratio scale

	//set scale for zoom

	if (View_zoom > f1_0) {		//> 1?  zoom out by scaling x&y
		s = fixdiv(f1_0,View_zoom);		//get 1/zoom
		Matrix_scale.x = fixmul(Matrix_scale.x,s);
		Matrix_scale.y = fixmul(Matrix_scale.y,s);
	}
	else									//zoom in by scaling z
		Matrix_scale.z = fixmul(Matrix_scale.z,View_zoom);

	//now scale matrix elements

	s = Matrix_scale.x;
	View_matrix.rvec.x = fixmul(View_matrix.rvec.x,s);		//m1
	View_matrix.rvec.y = fixmul(View_matrix.rvec.y,s);		//m4
	View_matrix.rvec.z = fixmul(View_matrix.rvec.z,s);		//m7

	s = Matrix_scale.y;
	View_matrix.uvec.x = fixmul(View_matrix.uvec.x,s);		//m2
	View_matrix.uvec.y = fixmul(View_matrix.uvec.y,s);		//m5
	View_matrix.uvec.z = fixmul(View_matrix.uvec.z,s);		//m8

	s = Matrix_scale.z;
	View_matrix.fvec.x = fixmul(View_matrix.fvec.x,s);		//m3
	View_matrix.fvec.y = fixmul(View_matrix.fvec.y,s);		//m6
	View_matrix.fvec.z = fixmul(View_matrix.fvec.z,s);		//m9
}
