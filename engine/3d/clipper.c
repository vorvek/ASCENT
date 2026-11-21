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
 * ASCENT: this file replaces clipper.asm.
 *
 * Source for the 3d clipper
 */

#include "globvars.h"
#include "clipper.h"

//buffer of temp points for when clipping creates a new point
static g3s_point temp_points[MAX_POINTS_IN_POLY];

//stack of pointers to the free temp points. The assembly filled this
//table at assembly time. Here the first use fills it.
static g3s_point *free_points[MAX_POINTS_IN_POLY];
static int free_points_inited;

int free_point_num = 0;

static void init_free_points(void)
{
	int i;

	for (i=0;i<MAX_POINTS_IN_POLY;i++)
		free_points[i] = &temp_points[i];

	free_points_inited = 1;
}

//get a temporary point.
static g3s_point *get_temp_point(void)
{
	g3s_point *p;

	if (!free_points_inited)
		init_free_points();

	//ASCENT: the assembly had only a debug check for an empty buffer, and
	//read past the table. Here the last point is used again.
	if (free_point_num >= MAX_POINTS_IN_POLY)
		free_point_num = MAX_POINTS_IN_POLY-1;

	p = free_points[free_point_num++];

	p->p3_flags = PF_TEMP_POINT;		//clear proj,set temp

	return p;
}

//free a temporary point.
void free_temp_point(g3s_point *p)
{
	if (!free_points_inited)
		init_free_points();

	//ASCENT: no write before the start of the table
	if (free_point_num <= 0)
		return;

	free_points[--free_point_num] = p;
}

//clip a particular value (eg. x, y, u).
//takes the start and end values, and the fraction num/den
//The assembly does imul and idiv with no check. See g3_idiv().
static inline fix clip_value(fix start,fix end,fix num,fix den)
{
	return g3_idiv((int64_t) (fix) (end - start) * num,den) + start;
}

//clips an edge against one plane.
//takes on_pnt (on) ,off_pnt (off), plane_flag (1,2,4,8 = left,right,bot,top)
//returns the new point, with its codes set
static g3s_point *clip_edge(int plane_flag,g3s_point *on_pnt,g3s_point *off_pnt)
{
	fix a,b,num,den,t;
	g3s_point *tmp;

	//compute clipping value k = (xs-zs) / (xs-xe-zs+ze)
	//use x or y as appropriate, and negate x/y value as appropriate

	if (plane_flag & (CC_OFF_TOP+CC_OFF_BOT)) {		//top or bot (y)?
		a = on_pnt->y;
		b = off_pnt->y;
	}
	else {
		a = on_pnt->x;
		b = off_pnt->x;
	}

	if (plane_flag & (CC_OFF_LEFT+CC_OFF_BOT)) {		//left or bot (neg)?
		a = g3_neg(a);
		b = g3_neg(b);
	}

	num = a - on_pnt->z;					//xs-zs
	den = num - b + off_pnt->z;		//xs-xe-zs+ze

	//now frac=num/den

	tmp = get_temp_point();

	t = tmp->x = clip_value(on_pnt->x,off_pnt->x,num,den);
	tmp->z = t;		//assume z=x

	t = tmp->y = clip_value(on_pnt->y,off_pnt->y,num,den);

	if (plane_flag & (CC_OFF_TOP+CC_OFF_BOT))			//top or bot (y)?
		tmp->z = t;		//z=y

	//check if uv values present, and clip if so

	if (on_pnt->p3_flags & PF_UVS) {
		tmp->p3_u = clip_value(on_pnt->p3_u,off_pnt->p3_u,num,den);
		tmp->p3_v = clip_value(on_pnt->p3_v,off_pnt->p3_v,num,den);

		tmp->p3_flags |= PF_UVS;		//new point has uv set
	}

	//check if lv values present, and clip if so

	if (on_pnt->p3_flags & PF_LVS) {
		tmp->p3_l = clip_value(on_pnt->p3_l,off_pnt->p3_l,num,den);

		tmp->p3_flags |= PF_LVS;		//new point has lv set
	}

	//negate z if clipping against left or bot

	if (plane_flag & (CC_OFF_LEFT+CC_OFF_BOT))
		tmp->z = g3_neg(tmp->z);		//z=-x (or -y)

	g3_code_point(tmp);

	return tmp;
}

//clips a line to the viewing pyramid.
void clip_line(g3s_point **p0,g3s_point **p1,ubyte codes_or)
{
	int plane_flag;
	g3s_point *old_p1;
	ubyte new_codes;

	//p0 is the on-screen point and p1 the off-screen point for each plane.
	//The swap stays in effect for the caller, as in the assembly, which
	//exchanged esi and edi.

	for (plane_flag=1;plane_flag<16;plane_flag<<=1)
		if (codes_or & plane_flag) {		//off this plane?

			if ((*p0)->p3_codes & plane_flag) {		//this one on?  ..nope
				g3s_point *t = *p0; *p0 = *p1; *p1 = t;
			}

			old_p1 = *p1;		//save old off-screen point

			*p1 = clip_edge(plane_flag,*p0,*p1);
			new_codes = (*p1)->p3_codes;

			//see if must free rejected point
			if (old_p1->p3_flags & PF_TEMP_POINT)
				free_temp_point(old_p1);

			if ((*p0)->p3_codes & new_codes)		//clipped away?
				return;

			codes_or = (*p0)->p3_codes | new_codes;		//get new codes_or
		}
}

//clips a polygon against one plane.
//takes src and dest lists, the number of vertices
//returns the end of the new list, and fills in the new codes.
static g3s_point **clip_plane(int plane_flag,g3s_point **src,g3s_point **dest,int nverts,g3s_codes *cc)
{
	int i;
	g3s_point *cur,*prev,*next,*t;

	//copy first two verts to end
	src[nverts] = src[0];
	src[nverts+1] = src[1];

	cc->or = 0; cc->and = 0xff;		//initialize codes

	//go though list of points, from the second point.
	//Note: a temp point that is freed here stays in the source list. When
	//its slot is used again for a new point in the same pass, a later
	//look at the list (the two copied points at the end) sees the new
	//point. The assembly does the same, and this code keeps the behavior.

	for (i=1;i<=nverts;i++) {

		cur = src[i];		//get current point

		if (cur->p3_codes & plane_flag) {		//cur point off?

			//cur point is off. check prev and next

			prev = src[i-1];

			if (! (prev->p3_codes & plane_flag)) {		//prev not off?

				//must clip cur (off screen) to prev (on screen)

				t = clip_edge(plane_flag,prev,cur);

				*dest++ = t;		//store in dest list

				cc->or  |= t->p3_codes;		//update codes_or
				cc->and &= t->p3_codes;		//update codes_and
			}

			next = src[i+1];

			if (! (next->p3_codes & plane_flag)) {		//next not off?

				//must clip cur (off screen) to next (on screen)

				t = clip_edge(plane_flag,next,cur);

				*dest++ = t;		//store in dest list

				cc->or  |= t->p3_codes;		//update codes_or
				cc->and &= t->p3_codes;		//update codes_and
			}

			//see if must free discarded point

			if (cur->p3_flags & PF_TEMP_POINT)
				free_temp_point(cur);
		}
		else {		//cur not off, copy to dest buffer

			*dest++ = cur;

			cc->or  |= cur->p3_codes;		//update codes_or
			cc->and &= cur->p3_codes;		//update codes_and
		}
	}

	return dest;
}

//3d clip a polygon.
//takes src list, dest list, nverts, codes. cc->or selects the planes. When
//no plane is selected the codes do not change.
//returns the list of clipped points, some of them new, the new nverts, and
//the new codes
//the returned list will be either of src,dest at entry
g3s_point **clip_polygon(g3s_point **src,g3s_point **dest,int *nv,g3s_codes *cc)
{
	int plane_flag;
	g3s_point **t;

	//now loop through each plane, a-clipping as we go
	//clipping from [src] -> [dest]

	for (plane_flag=1;plane_flag<16;plane_flag<<=1)
		if (cc->or & plane_flag) {		//off this plane?

			//clip against this plane
			*nv = (int) (clip_plane(plane_flag,src,dest,*nv,cc) - dest);		//get new vert count

			t = src; src = dest; dest = t;		//new dest = old src

			if (cc->and)		//check new codes_and
				break;			//polygon all off screen
		}

	return src;
}
