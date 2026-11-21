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
 * ASCENT: this file replaces draw.asm.
 *
 * Source for drawing routines
 */

#include "globvars.h"
#include "clipper.h"

//from the texture mapper. texmap.h includes 3d.h, so the 3d library
//declares the two functions here
extern void draw_tmap(grs_bitmap *bp,int nverts,g3s_point **vertbuf);
extern void gr_upoly_tmap(int nverts,int *vert);

static g3_tmap_drawer_func *tmap_drawer_ptr = draw_tmap;
static g3_flat_drawer_func *flat_drawer_ptr = gr_upoly_tmap;
static g3_line_drawer_func *line_drawer_ptr = gr_line;

//specifies new routines to call to draw polygons
//takes ptr to tmap routine, ptr to flat routine, line rtn
//NULL restores the default
void g3_set_special_render(g3_tmap_drawer_func *tmap_drawer,g3_flat_drawer_func *flat_drawer,g3_line_drawer_func *line_drawer)
{
	tmap_drawer_ptr = (tmap_drawer)?tmap_drawer:draw_tmap;
	flat_drawer_ptr = (flat_drawer)?flat_drawer:gr_upoly_tmap;
	line_drawer_ptr = (line_drawer)?line_drawer:gr_line;
}

//returns codes_and & codes_or of a list of points
//ASCENT: 3d.h declares this function, and no assembly file has it. The
//code here does what the first loop of g3_draw_poly() does.
g3s_codes g3_check_codes(int nv,g3s_point **pointlist)
{
	int i;
	g3s_codes cc;

	cc.or = 0; cc.and = 0xff;

	for (i=0;i<nv;i++) {
		cc.and &= pointlist[i]->p3_codes;
		cc.or  |= pointlist[i]->p3_codes;
	}

	return cc;
}

//draws a line. takes two points.
//returns 0 if both points are off the same side. returns true if the line
//is all on screen, else the value from the 2d line drawer.
bool g3_draw_line(g3s_point *p0,g3s_point *p1)
{
	ubyte codes_or;
	bool ret;
	int clipped = 0;

	//The assembly called itself again after a 3d clip. This loop does the
	//same steps.

	for (;;) {

		//check codes for reject, clip, or no clip

		if (p0->p3_codes & p1->p3_codes) {		//check codes_and
			ret = 0;		//both off same side, not drawn
			break;
		}

		codes_or = p0->p3_codes | p1->p3_codes;

		if (! (codes_or & CC_BEHIND)) {		//neg z means must clip

			if (! (p0->p3_flags & PF_PROJECTED))
				g3_project_point(p0);

			if (! (p0->p3_flags & PF_OVERFLOW)) {

				if (! (p1->p3_flags & PF_PROJECTED))
					g3_project_point(p1);

				if (! (p1->p3_flags & PF_OVERFLOW)) {

					if (codes_or)		//call clipping line drawer
						ret = (bool) (*line_drawer_ptr)(p0->p3_sx,p0->p3_sy,p1->p3_sx,p1->p3_sy);
					else {				//we know this one is on screen
						(*line_drawer_ptr)(p0->p3_sx,p0->p3_sy,p1->p3_sx,p1->p3_sy);
						ret = 1;			//definitely drew
					}

					break;
				}
			}
		}

		//must clip the line

		//ASCENT: when a clipped line still needs a 3d clip and has no code
		//for the four side planes, the clipper can do no more. The
		//assembly called itself without end in this case. Here the line
		//is not drawn. The limit on the number of clips is for safety.
		if (clipped && (clipped >= 4 || !(codes_or & (CC_OFF_LEFT+CC_OFF_RIGHT+CC_OFF_BOT+CC_OFF_TOP)))) {
			ret = 0;
			break;
		}

		clip_line(&p0,&p1,codes_or);		//do the 3d clip
		clipped++;
	}

	//free up temp points
	//ASCENT: the assembly freed the temp points at each level of the
	//recursion, so a line that was clipped two times freed the same points
	//two times. Here each temp point is freed one time.

	if (clipped) {
		if (p0->p3_flags & PF_TEMP_POINT)
			free_temp_point(p0);

		if (p1->p3_flags & PF_TEMP_POINT)
			free_temp_point(p1);
	}

	return ret;
}

//returns true if a plane is facing the viewer. takes the unrotated surface
//normal of the plane, and a point on it.  The normal need not be normalized
bool g3_check_normal_facing(vms_vector *v,vms_vector *norm)
{
	vms_vector tempv;

	vm_vec_sub(&tempv,&View_position,v);		//view vector

	return (vm_vec_dotprod(&tempv,norm) > 0);		//true if positive
}

//takes normal or NULL, list of points, pnt
//returns facing?
static bool do_facing_check(g3s_point **pointlist,vms_vector *norm,vms_vector *pnt)
{
	if (norm)		//normal passed?

		//we have the normal. check if facing
		return g3_check_normal_facing(pnt,norm);

	else {	//normal not specified, so must compute
		vms_vector tempv;

		//get three points (rotated) and compute normal

		vm_vec_perp(&tempv,&pointlist[0]->p3_vec,&pointlist[1]->p3_vec,&pointlist[2]->p3_vec);

		return (vm_vec_dotprod(&pointlist[1]->p3_vec,&tempv) < 0);
	}
}

//see if face is visible and draw if it is.
//takes nv, point list, normal, point.
//normal can be NULL, which will for compution here (which will be slow).
//returns -1 if not facing, 1 if off screen, 0 if drew
bool g3_check_and_draw_poly(int nv,g3s_point **pointlist,vms_vector *norm,vms_vector *pnt)
{
	if (do_facing_check(pointlist,norm,pnt))
		return g3_draw_poly(nv,pointlist);
	else
		return (bool) -1;
}

bool g3_check_and_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm,vms_vector *norm,vms_vector *pnt)
{
	if (do_facing_check(pointlist,norm,pnt))
		return g3_draw_tmap(nv,pointlist,uvl_list,bm);
	else
		return (bool) -1;
}

//project the points of a polygon (clipped or all on screen), make the
//list of 2d coords, and call the 2d drawer.
//returns 0 if called 2d, 1 if a point could not be projected
static bool draw_poly_2d(int nv,g3s_point **bufptr)
{
	int i;
	g3s_point *p;

	//now make list of 2d coords (& check for overflow)

	for (i=0;i<nv;i++) {
		p = bufptr[i];

		if (! (p->p3_flags & PF_PROJECTED))
			g3_project_point(p);

		//ASCENT: the assembly jumped back to the 3d clipper on overflow.
		//The clipper cannot remove the overflow, and that code ran until
		//the stack was full. Here the face is not drawn, as in
		//g3_draw_tmap().
		if (p->p3_flags & PF_OVERFLOW)
			return 1;

		Vertex_list[i*2]   = p->p3_sx;
		Vertex_list[i*2+1] = p->p3_sy;
	}

	//ASCENT: the GPU renderer draws the face (globvars.h)
	if (G3_GPU_ROUTE()) {
		G3_gpu->poly(nv,bufptr);
		return 0;
	}

	//DONT_USE_UPOLY was 1 in the assembly, so there is no trivial accept
	//test here: the flat drawer is called for each face
	(*flat_drawer_ptr)(nv,Vertex_list);

	return 0;		//say it drew
}

//draw a flat-shaded face.
//takes nv, list of pointers to points
//returns 0 if called 2d, 1 if all points off screen
bool g3_draw_poly(int nv,g3s_point **pointlist)
{
	int i;
	g3s_point **bufptr = Vbuf0;		//list of ptrs
	g3s_codes cc;
	bool ret;

	cc.or = 0; cc.and = 0xff;		//init codes

	for (i=0;i<nv;i++) {
		bufptr[i] = pointlist[i];		//store in ptr array

		cc.and &= bufptr[i]->p3_codes;	//update codes_and
		cc.or  |= bufptr[i]->p3_codes;	//update codes_or
	}

	if (cc.and)		//check codes_and
		return 1;	//all the way off screen, not visible at all

	if (cc.or == 0)		//no clip needed
		return draw_poly_2d(nv,bufptr);

	//we require a 3d clip

	bufptr = clip_polygon(Vbuf0,Vbuf1,&nv,&cc);

	//ASCENT: when the face was clipped away, the assembly returned a
	//value that was not defined (what the clipper left in al). Here the
	//return value is 1 (off screen).

	if (nv && !cc.and && !(cc.or & CC_BEHIND))		//not clipped away? (some points still behind eye is clipped away too)
		ret = draw_poly_2d(nv,bufptr);
	else
		ret = 1;

	//free temp points. (The count is not zero here: the clipper stops
	//when all the points are off one plane, and that list is not empty.)

	for (i=0;i<nv;i++)
		if (bufptr[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(bufptr[i]);

	return ret;
}

//make sure all points of a polygon are projected, and call the texture
//mapper.
//returns 0 if called the texture mapper, 1 if a point could not be projected
static bool draw_tmap_2d(int nv,g3s_point **bufptr,grs_bitmap *bm)
{
	int i;
	g3s_point *p;

	//make sure all points projected

	for (i=0;i<nv;i++) {
		p = bufptr[i];

		if (! (p->p3_flags & PF_PROJECTED))
			g3_project_point(p);

		if (p->p3_flags & PF_OVERFLOW)		//Should not overflow after clip
			return 1;
	}

	//ASCENT: the GPU renderer draws the face (globvars.h)
	if (G3_GPU_ROUTE()) {
		G3_gpu->tmap(nv,bufptr,bm,tmap_drawer_ptr != draw_tmap);
		return 0;
	}

	//now call the texture mapper

	(*tmap_drawer_ptr)(bm,nv,bufptr);

	return 0;		//say it drew
}

//draw a texture-mapped face.
//takes nv, point list, uvl_list, bitmap
//returns 0 if called 2d, 1 if all points off screen
bool g3_draw_tmap(int nv,g3s_point **pointlist,g3s_uvl *uvl_list,grs_bitmap *bm)
{
	int i;
	g3s_point **bufptr = Vbuf0;		//list of ptrs
	g3s_codes cc;
	bool ret;

	cc.or = 0; cc.and = 0xff;		//init codes

	//loop to check codes, make list of point ptrs, and copy uvl's into points

	for (i=0;i<nv;i++) {
		g3s_point *p;

		p = bufptr[i] = pointlist[i];		//store in ptr array

		cc.and &= p->p3_codes;		//update codes_and
		cc.or  |= p->p3_codes;		//update codes_or

		p->p3_u = uvl_list[i].u;
		p->p3_v = uvl_list[i].v;
		p->p3_l = uvl_list[i].l;

		p->p3_flags |= PF_UVS + PF_LVS;		//this point's got em
	}

	if (cc.and)		//check codes_and
		return 1;	//all the way off screen, not visible at all

	//ASCENT: the GPU renderer gets the face as it is and clips it itself,
	//so that the light of the face does not depend on the clip (gpu_tmap
	//in gpu3d.c).
	if (G3_GPU_ROUTE()) {
		G3_gpu->tmap(nv,bufptr,bm,tmap_drawer_ptr != draw_tmap);
		return 0;
	}

	if (cc.or == 0)		//no clip needed
		return draw_tmap_2d(nv,bufptr,bm);

	//we require a 3d clip

	bufptr = clip_polygon(Vbuf0,Vbuf1,&nv,&cc);

	//ASCENT: when the face was clipped away, the assembly returned a
	//value that was not defined. Here the return value is 1 (off screen).

	if (nv && !cc.and && !(cc.or & CC_BEHIND))		//not clipped away? (some points still behind eye is clipped away too)
		ret = draw_tmap_2d(nv,bufptr,bm);
	else
		ret = 1;

	//free temp points, from the end of the list

	for (i=nv-1;i>=0;i--)
		if (bufptr[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(bufptr[i]);

	return ret;
}

//draw a sortof-sphere. takes point, radius
void g3_draw_sphere(g3s_point *pnt,fix rad)
{
	int64_t n;

	if (pnt->p3_codes & CC_BEHIND)
		return;

	if (! (pnt->p3_flags & PF_PROJECTED))
		g3_project_point(pnt);

	if (pnt->p3_flags & PF_OVERFLOW)
		return;

	//calc radius.  since disk doesn't take width & height, let's just
	//say the the radius is the width

	n = (int64_t) fixmul(rad,Matrix_scale.x) * Canv_w2;

	if (g3_div_overflow(n,pnt->z))		//on overflow, no draw
		return;

	//ASCENT: the GPU renderer draws the disk (globvars.h)
	if (G3_GPU_ROUTE()) {
		G3_gpu->sphere(pnt,g3_idiv(n,pnt->z));
		return;
	}

	gr_disk(pnt->p3_sx,pnt->p3_sy,g3_idiv(n,pnt->z));
}
