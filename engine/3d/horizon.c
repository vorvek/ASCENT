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
 * ASCENT: this file replaces horizon.asm.
 *
 * Routines to draw the horizon
 */

#include "globvars.h"

//from the texture mapper
extern void gr_upoly_tmap(int nverts,int *vert);

//an end of the horizon line
typedef struct horz_point {
	fix x,y;		//coords of point
	int edge;	//which edge point is on
} horz_point;

static int sky_color,ground_color;
static int top_color,bot_color;

static short color_swap;			//flag for if we swapped
static short sky_ground_flag;		//0=both, 1=all sky, -1=all gnd

static vms_vector horizon_vec;	//unscaled up vector

static fix up_right,down_right,down_left,up_left;

static horz_point left_point,right_point;

//the coordinates of the four corners of the screen (values are placeholders)
static fix corners[8] = {
	0,0,			//up left
	319,0,		//up right
	319,199,		//down right
	0,199};		//down left

//draw a polygon (one half of horizon) from the horizon line
static void draw_horz_poly(horz_point *start,horz_point *end)
{
	fix horizon_poly[5*2];		//max of 5 points
	int n,i,c;

	//copy horizon line as first points in poly

	horizon_poly[0] = end->x;
	horizon_poly[1] = end->y;

	horizon_poly[2] = start->x;
	horizon_poly[3] = start->y;

	//add corners to polygon

	n = end->edge - start->edge;		//number of edges
	if (n < 0)
		n += 4;

	//ASCENT: the two ends are never on the same edge. In that case (n==0)
	//the assembly loop would not stop. Here no corner is added.

	for (i=0,c=start->edge;i<n;i++) {		//copy a corner
		horizon_poly[4+i*2]   = corners[c*2];
		horizon_poly[4+i*2+1] = corners[c*2+1];

		if (++c == 4)		//end of list?
			c = 0;
	}

	//now draw the polygon

	gr_upoly_tmap(n+2,horizon_poly);		//corner count plus horz line end points
}

//draws a horizon. takes sky_color, ground_color
void g3_draw_horizon(int sky,int ground)
{
	fix x,y,z,t;
	ubyte flags_and,flags_or,s;
	horz_point pts[4],*p;

	sky_color = sky;
	ground_color = ground;

	color_swap = 0;			//assume no swap
	sky_ground_flag = 0;		//assume both

	//check up. if y is zero, check x
	if (View_matrix.uvec.y > 0 || (View_matrix.uvec.y == 0 && View_matrix.uvec.x < 0)) {		//sky up
		top_color = sky;
		bot_color = ground;
	}
	else {		//make sky down
		top_color = ground;
		bot_color = sky;
		color_swap = -1;		//we swapped
	}

	//compute horizon_vector

	x = horizon_vec.x = fixmul(fixmul(Unscaled_matrix.rvec.y,Matrix_scale.y),Matrix_scale.z);
	y = horizon_vec.y = fixmul(fixmul(Unscaled_matrix.uvec.y,Matrix_scale.x),Matrix_scale.z);
	z = horizon_vec.z = fixmul(fixmul(Unscaled_matrix.fvec.y,Matrix_scale.x),Matrix_scale.y);

	//now compute values & flag for 4 corners.

	flags_and = 0xff; flags_or = 0;

	t = up_right = z + x + y;		//x+y+z
	s = (t < 0) ? 0xff : 0; flags_and &= s; flags_or |= s;

	x = (fix) ((uint32_t) x << 1);
	y = (fix) ((uint32_t) y << 1);		//get x*2, y*2

	t = down_right = t - y;			//x-y+z
	s = (t < 0) ? 0xff : 0; flags_and &= s; flags_or |= s;

	t = down_left = t - x;			//-x-y+z
	s = (t < 0) ? 0xff : 0; flags_and &= s; flags_or |= s;

	t = up_left = t + y;				//-x+y+z
	s = (t < 0) ? 0xff : 0; flags_and &= s; flags_or |= s;

	//check flags for all sky or all ground.

	//we see all sky or all ground.  clear screen with appropriate color

	if (flags_and) {
		sky_ground_flag = -1;		//all ground
		gr_clear_canvas(ground_color);
		return;
	}

	if (! flags_or) {
		sky_ground_flag = 1;			//all sky
		gr_clear_canvas(sky_color);
		return;
	}

	//fill in values for corners

	corners[2] = corners[4] = (fix) ((uint32_t) Canvas_width << 16);		//make a fix
	corners[5] = corners[7] = (fix) ((uint32_t) Canvas_height << 16);		//make a fix

	//check for intesection with each of four edges & compute horizon line
	//(the divides are fixmuldiv in the assembly: imul, idiv, no check)

	p = pts;

	//check intersection with left edge

	if ((up_left ^ down_left) < 0) {		//signs different?
		p->y = g3_abs(g3_idiv((int64_t) corners[5] * up_left,y));		//up_left * height / y*2
		p->x = 0;
		p->edge = 0;
		p++;
	}

	//check intersection with top edge

	if ((up_left ^ up_right) < 0) {		//signs different?
		p->x = g3_abs(g3_idiv((int64_t) corners[4] * up_left,x));		//up_left * width / x*2
		p->y = 0;
		p->edge = 1;
		p++;
	}

	//check intersection with right edge

	if ((up_right ^ down_right) < 0) {		//signs different?
		p->y = g3_abs(g3_idiv((int64_t) corners[5] * up_right,y));		//up_right * height / y*2
		p->x = corners[4];
		p->edge = 2;
		p++;
	}

	//check intersection with bottom edge

	if ((down_right ^ down_left) < 0) {		//signs different?
		p->x = g3_abs(g3_idiv((int64_t) corners[4] * down_left,x));		//down_left * width / x*2
		p->y = corners[5];
		p->edge = 3;
		p++;
	}

	//we wrote two and only two ends of horizon line. (The assembly stored
	//the ends directly in left_point and right_point, and had a debug
	//check for the number of ends.)

	if (p - pts >= 1) left_point = pts[0];
	if (p - pts >= 2) right_point = pts[1];

	//make sure first edge is left

	if (left_point.x > right_point.x) {		//swap left and right
		horz_point tmp = left_point;
		left_point = right_point;
		right_point = tmp;
	}

	//draw two polygons from horizon line

	if (top_color != -1) {		//-1 is no draw
		gr_setcolor(top_color);
		draw_horz_poly(&left_point,&right_point);
	}

	if (bot_color != -1) {		//-1 is no draw
		gr_setcolor(bot_color);
		draw_horz_poly(&right_point,&left_point);
	}
}

//compute vector describing horizon intersection with a point.
//takes 2d point, vec.
static void compute_horz_end_vec(horz_point *pnt,vms_vector *v)
{
	fix xfrac,yfrac,sum,a,b,ratio,mag;
	int64_t q;

	//compute rotated x/z & y/z ratios

	xfrac = fixdiv(pnt->x - Canv_w2,Canv_w2);
	yfrac = g3_neg(fixdiv(pnt->y - Canv_h2,Canv_h2));		//y inversion

	//compute fraction unrotated x/z

	sum = xfrac + yfrac;

	b = fixmul(sum,View_matrix.fvec.z) - View_matrix.rvec.z - View_matrix.uvec.z;		//m9, m7, m8

	a = View_matrix.rvec.x + View_matrix.uvec.x - fixmul(sum,View_matrix.fvec.x);		//m1, m2, m3

	//now a/b = z/x. do divide in way to give result < 0

	if (g3_abs(b) < g3_abs(a)) {		//z is bigger, so do as x/z

		ratio = fixdiv(b,a);

		//now ratio = x/z.  Compute vector by normalizing and correcting sign

		q = (int64_t) ratio * ratio;		//compute x*x, + z*z (z==1)
		mag = (fix) quad_sqrt((int32_t) (uint32_t) q,(int32_t) ((uint32_t) ((uint64_t) q >> 32) + 1));

		v->x = fixdiv(ratio,mag);
		v->z = fixdiv(f1_0,mag);
	}
	else {		//x is bigger, so do as z/x

		ratio = fixdiv(a,b);

		//now ratio = z/x.  Compute vector by normalizing and correcting sign

		q = (int64_t) ratio * ratio;		//compute z*z, + x*x (x==1)
		mag = (fix) quad_sqrt((int32_t) (uint32_t) q,(int32_t) ((uint32_t) ((uint64_t) q >> 32) + 1));

		v->z = fixdiv(ratio,mag);
		v->x = fixdiv(f1_0,mag);
	}

	v->y = 0;

	//now make sure that this vector is in front of you, not behind

	q = (int64_t) ((uint64_t) ((int64_t) v->x * View_matrix.fvec.x) + (uint64_t) ((int64_t) v->z * View_matrix.fvec.z));

	if (q < 0) {		//z is neg, flip vector
		v->x = g3_neg(v->x);
		v->z = g3_neg(v->z);
	}
}

//compute vector decribing a corner of the screen.
//takes vector, corner num
static void compute_corner_vec(vms_vector *v,int corner_num)
{
	fix m13,m46,m79,m56,m23,m89;
	fix b,c,d,den;

	if (corner_num >= 4)
		corner_num -= 4;

	//compute all deltas

	b = View_matrix.rvec.x;		//m1
	c = View_matrix.rvec.y;		//m4
	d = View_matrix.rvec.z;		//m7

	if (corner_num == 0 || corner_num == 3) {		//neg x
		b = g3_neg(b);
		c = g3_neg(c);
		d = g3_neg(d);
	}

	m13 = b - View_matrix.fvec.x;		//m1-m3
	m46 = c - View_matrix.fvec.y;		//m4-m6
	m79 = d - View_matrix.fvec.z;		//m7-m9

	b = View_matrix.uvec.y;		//m5
	c = View_matrix.uvec.x;		//m2
	d = View_matrix.uvec.z;		//m8

	if (corner_num >= 2) {		//neg y
		b = g3_neg(b);
		c = g3_neg(c);
		d = g3_neg(d);
	}

	m56 = b - View_matrix.fvec.y;		//m5-m6
	m23 = c - View_matrix.fvec.x;		//m2-m3
	m89 = d - View_matrix.fvec.z;		//m8-m9

	//compute x/z ratio

	//compute denomonator

	den = fixmul(m56,m13) - fixmul(m46,m23);

	//now we have the denominator.  (The assembly has a test for a small
	//denominator here, with no code for that case.)

	//now do x/z numerator, x/z ratio

	v->x = fixdiv(fixmul(m89,m46) - fixmul(m79,m56),den);

	//now do y/z

	v->y = fixdiv(fixmul(m79,m23) - fixmul(m89,m13),den);

	v->z = f1_0;

	vm_vec_normalize(v);

	//make sure this vec is pointing in right direction

	if (vm_vec_dotprod(v,&View_matrix.fvec) <= 0) {
		v->x = g3_neg(v->x);
		v->y = g3_neg(v->y);
		v->z = g3_neg(v->z);
	}
}

//return information on the polygon that is the sky.
//takes ptr to x,y pairs, ptr to vecs for each point
//returns number of points
//IMPORTANT: g3_draw_horizon() must be called before this routine.
int g3_compute_sky_polygon(fix *points_2d,vms_vector *vecs)
{
	horz_point *start,*end;
	int n,i,c,corner_num;

	if (sky_ground_flag < 0)		//we drew all ground, so there was no horizon drawn
		return 0;						//no points in poly

	if (sky_ground_flag > 0) {		//we drew all sky, so find 4 corners

		for (i=0;i<8;i++)
			points_2d[i] = corners[i];

		for (i=0;i<4;i++)
			compute_corner_vec(&vecs[i],i);

		return 4;						//4 corners
	}

	start = &left_point;
	end = &right_point;

	if (color_swap) {		//sky isn't top
		start = &right_point;
		end = &left_point;
	}

	//copy horizon line as first points in poly

	points_2d[0] = end->x;		//copy end point
	points_2d[1] = end->y;

	points_2d[2] = start->x;	//copy start point
	points_2d[3] = start->y;

	compute_horz_end_vec(end,&vecs[0]);		//end point is first point
	compute_horz_end_vec(start,&vecs[1]);

	//add corners to polygon

	corner_num = start->edge;		//edge number of start edge

	n = end->edge - start->edge;	//number of edges
	if (n < 0)
		n += 4;

	//ASCENT: see draw_horz_poly() for n==0

	for (i=0,c=start->edge;i<n;i++) {		//copy a corner
		points_2d[4+i*2]   = corners[c*2];
		points_2d[4+i*2+1] = corners[c*2+1];

		if (++c == 4)		//end of list?
			c = 0;

		compute_corner_vec(&vecs[2+i],corner_num);
		corner_num++;
	}

	//now return with count

	return n+2;		//corner count plus horz line end points
}
