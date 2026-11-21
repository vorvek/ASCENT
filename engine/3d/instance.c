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
 * ASCENT: this file replaces instance.asm.
 *
 * Code for instancing in the 3d system
 */

#include "globvars.h"

//ASCENT: was 5. The models of Descent to Undermountain have up to 64
//submodels in a chain, and the game draws them with one instance for
//each joint (main/polyobj.c, draw_model_sorted).
#define MAX_INSTANCE_DEPTH	72

static struct instance_context {
	vms_vector view_pos;
	vms_matrix view_matrix;
} instance_stack[MAX_INSTANCE_DEPTH];

static int instance_depth = 0;

//start instancing, using a matrix
//takes position, matrix.
//if matrix==NULL, don't modify matrix.  This will be like doing an offset
void g3_start_instance_matrix(vms_vector *pos,vms_matrix *orient)
{
	vms_vector tempv;
	vms_matrix tempm,tempm2;

	//save current context

	//ASCENT: the assembly had only a debug check for the max depth, and
	//wrote past the stack. Here a context past the max depth is not saved.
	if (instance_depth >= 0 && instance_depth < MAX_INSTANCE_DEPTH) {
		instance_stack[instance_depth].view_pos = View_position;
		instance_stack[instance_depth].view_matrix = View_matrix;
	}

	instance_depth++;

	//step 1: subtract object position from view position

	vm_vec_sub2(&View_position,pos);

	if (orient) {

		//step 2: rotate view vector through object matrix

		vm_vec_rotate(&tempv,&View_position,orient);
		View_position = tempv;

		//step 3: rotate object matrix through view_matrix (vm = ob * vm)

		vm_copy_transpose_matrix(&tempm2,orient);

		vm_matrix_x_matrix(&tempm,&tempm2,&View_matrix);
		View_matrix = tempm;
	}

	//now we are done!
}

//start instancing, using angles (calls vm_angles_2_matrix)
//takes position, angvec.
//if angles==NULL, don't modify matrix.  This will be like doing an offset
void g3_start_instance_angles(vms_vector *pos,vms_angvec *angles)
{
	vms_matrix instmat;

	if (angles == NULL) {		//no new matrix
		g3_start_instance_matrix(pos,NULL);
		return;
	}

	vm_angles_2_matrix(&instmat,angles);

	g3_start_instance_matrix(pos,&instmat);
}

//we are done instancing
void g3_done_instance(void)
{
	instance_depth--;

	//ASCENT: the assembly had only a debug check for stack underflow
	if (instance_depth >= 0 && instance_depth < MAX_INSTANCE_DEPTH) {
		View_position = instance_stack[instance_depth].view_pos;
		View_matrix = instance_stack[instance_depth].view_matrix;
	}
}

//ASCENT: new. DTUGAME.EXE has these two functions (0xEA919, 0xEA93A): they
//copy the view position and the view matrix of the current instance (48
//bytes) to and from a buffer. The sorted model draw uses them.
void g3_get_instance_state(g3s_instance_state *state)
{
	state->view_pos = View_position;
	state->view_matrix = View_matrix;
}

void g3_set_instance_state(g3s_instance_state *state)
{
	View_position = state->view_pos;
	View_matrix = state->view_matrix;
}
