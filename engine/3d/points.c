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
 * ASCENT: this file replaces points.asm.
 *
 * Source for point definition, rotation, etc.
 */

#include "globvars.h"

//finds clipping codes for a point. fills in p3_codes, and returns codes
ubyte g3_code_point(g3s_point *point)
{
	ubyte cc = 0;
	fix z = point->z;

	if (point->x > z)		//x>z?
		cc |= CC_OFF_RIGHT;

	if (point->y > z)		//y>z?
		cc |= CC_OFF_TOP;

	z = g3_neg(z);

	//the assembly tests the sign of -z, so z == 0x80000000 is not behind
	if (z >= 0)
		cc |= CC_BEHIND;

	if (point->x < z)		//x<-z?
		cc |= CC_OFF_LEFT;

	if (point->y < z)		//y<-z
		cc |= CC_OFF_BOT;

	return point->p3_codes = cc;
}

//rotate a point. don't look at rotated flags
//returns codes
ubyte g3_rotate_point(g3s_point *dest,vms_vector *src)
{
	vms_vector tempv;

	vm_vec_sub(&tempv,src,&View_position);

	vm_vec_rotate(&dest->p3_vec,&tempv,&View_matrix);

	dest->p3_flags = 0;		//not projected

	return g3_code_point(dest);
}

//projects a point
void g3_project_point(g3s_point *point)
{
	int64_t n;

	if ((point->p3_flags & PF_PROJECTED) || (point->p3_codes & CC_BEHIND))
		return;

	//the assembly checks for overflow before each divide. On overflow it
	//stores PF_OVERFLOW in the flags. That clears the other flags.

	n = (int64_t) point->x * Canv_w2;
	if (g3_div_overflow(n,point->z)) {
		point->p3_flags = PF_OVERFLOW;
		return;
	}
	point->p3_sx = g3_idiv(n,point->z) + Canv_w2;

	n = (int64_t) point->y * Canv_h2;
	if (g3_div_overflow(n,point->z)) {
		point->p3_flags = PF_OVERFLOW;
		return;
	}
	point->p3_sy = g3_neg(g3_idiv(n,point->z)) + Canv_h2;

	point->p3_flags |= PF_PROJECTED;		//projected
}

//from a 2d point on the screen, compute the vector in 3-space through that point
//the 2d point is relative to the upper left of the canvas
void g3_point_2_vec(vms_vector *v,short sx,short sy)
{
	vms_vector tempv;
	vms_matrix tempm;
	fix t;

	t = fixdiv((fix) ((uint32_t) sx << 16) - Canv_w2,Canv_w2);
	tempv.x = g3_idiv((int64_t) t * Matrix_scale.z,Matrix_scale.x);

	t = fixdiv((fix) ((uint32_t) sy << 16) - Canv_h2,Canv_h2);
	tempv.y = g3_neg(g3_idiv((int64_t) t * Matrix_scale.z,Matrix_scale.y));

	tempv.z = f1_0;

	vm_vec_normalize(&tempv);		//get normalized rotated vec

	vm_copy_transpose_matrix(&tempm,&Unscaled_matrix);

	vm_vec_rotate(v,&tempv,&tempm);
}

//rotate a delta y vector. takes dest vec, delta y
vms_vector *g3_rotate_delta_y(vms_vector *dest,fix dy)
{
	dest->x = fixmul(View_matrix.rvec.y,dy);		//m4
	dest->y = fixmul(View_matrix.uvec.y,dy);		//m5
	dest->z = fixmul(View_matrix.fvec.y,dy);		//m6

	return dest;
}

//rotate a delta x vector. takes dest vec, delta x
vms_vector *g3_rotate_delta_x(vms_vector *dest,fix dx)
{
	dest->x = fixmul(View_matrix.rvec.x,dx);		//m1
	dest->y = fixmul(View_matrix.uvec.x,dx);		//m2
	dest->z = fixmul(View_matrix.fvec.x,dx);		//m3

	return dest;
}

//rotate a delta z vector. takes dest vec, delta z
vms_vector *g3_rotate_delta_z(vms_vector *dest,fix dz)
{
	dest->x = fixmul(View_matrix.rvec.z,dz);		//m7
	dest->y = fixmul(View_matrix.uvec.z,dz);		//m8
	dest->z = fixmul(View_matrix.fvec.z,dz);		//m9

	return dest;
}

//rotate a delta vector. takes dest vec, src vec
vms_vector *g3_rotate_delta_vec(vms_vector *dest,vms_vector *src)
{
	return vm_vec_rotate(dest,src,&View_matrix);
}

//adds a delta vector to a point. takes dest point, src pnt, delta vec
//returns codes.
ubyte g3_add_delta_vec(g3s_point *dest,g3s_point *src,vms_vector *deltav)
{
	vm_vec_add(&dest->p3_vec,&src->p3_vec,deltav);

	dest->p3_flags = 0;		//not projected

	return g3_code_point(dest);
}

//calculate the depth of a point - returns the z coord of the rotated point
fix g3_calc_point_depth(vms_vector *pnt)
{
	uint64_t q;

	//64-bit sum of three products, as the add/adc pairs of the assembly
	q  = (uint64_t) ((int64_t) (fix) (pnt->x - View_position.x) * View_matrix.fvec.x);
	q += (uint64_t) ((int64_t) (fix) (pnt->y - View_position.y) * View_matrix.fvec.y);
	q += (uint64_t) ((int64_t) (fix) (pnt->z - View_position.z) * View_matrix.fvec.z);

	return (fix) (uint32_t) (q >> 16);		//shrd eax,edx,16
}
