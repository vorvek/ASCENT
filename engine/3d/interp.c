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
 * ASCENT: this file replaces interp.asm.
 *
 * Polygon object interpreter
 *
 * ASCENT: the model data is a byte buffer. The 16-bit and 32-bit values
 * in it are little endian and have no alignment. This code reads and
 * writes each value with the functions below, and gives the other 3d
 * functions aligned copies of the vectors and the uvl values. The
 * interpreter stores no pointer in the model data.
 */

#include <string.h>

#include "globvars.h"

//from the 2d library (palette.c)
extern int gr_find_closest_color_15bpp(int rgb);

#define OP_EOF				0	//eof
#define OP_DEFPOINTS		1	//defpoints
#define OP_FLATPOLY		2	//flat-shaded polygon
#define OP_TMAPPOLY		3	//texture-mapped polygon
#define OP_SORTNORM		4	//sort by normal
#define OP_RODBM			5	//rod bitmap
#define OP_SUBCALL		6	//call a subobject
#define OP_DEFP_START	7	//defpoints with start
#define OP_GLOW			8	//glow value for next poly

static grs_bitmap **bitmap_ptr;
static vms_angvec *anim_angles;		//pointer to angle data
static vms_vector *morph_points;		//alternate points for morph

//light value for the next tmap
static int glow_num = -1;				//-1 means off
static fix *glow_values;				//ptr to array of values

short highest_texture_num = 0;

static vms_angvec zero_angles = {0,0,0};		//for if no angles specified

static g3s_point rod_top_p,rod_bot_p;

int g3d_interp_outline = 0;			//if on, polygon models outlined in white

static g3s_point *morph_pointlist[3];
static g3s_uvl morph_uvls[3];

//the light for the current model
static fix model_light;

//ptr to array of points
static g3s_point *Interp_point_list;

//ASCENT: the corner normals of the current model, or NULL (3d.h)
static ubyte *Interp_smooth_data;
static ubyte *Interp_smooth;

#define MAX_POINTS_PER_POLY 25

//ASCENT: the assembly had a list of MAX_POINTS_PER_POLY pointers, and only
//a debug check for the count. The lists here have the size of the vertex
//buffers of the 3d, and a polygon with more points is not drawn.
static g3s_point *point_list[MAX_POINTS_IN_POLY];
static g3s_uvl uvl_copy[MAX_POINTS_IN_POLY];

//set when the morph versions of the opcodes are in use. The assembly
//changed four entries of the opcode table for this
static int morph_mode;

//Access to the model data

static inline uint get_w(ubyte *p)		//16 bits, zero extended
{
	return (uint) p[0] | ((uint) p[1] << 8);
}

static inline void put_w(ubyte *p,uint v)
{
	p[0] = (ubyte) v;
	p[1] = (ubyte) (v >> 8);
}

static inline fix get_fix(ubyte *p)
{
	return (fix) ((uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24));
}

static inline void put_fix(ubyte *p,fix v)
{
	p[0] = (ubyte) v;
	p[1] = (ubyte) ((uint32_t) v >> 8);
	p[2] = (ubyte) ((uint32_t) v >> 16);
	p[3] = (ubyte) ((uint32_t) v >> 24);
}

static inline vms_vector *get_vec(vms_vector *v,ubyte *p)
{
	v->x = get_fix(p);
	v->y = get_fix(p+4);
	v->z = get_fix(p+8);

	return v;
}

//the number of bytes of a list of point numbers: the count is made odd,
//so that the data after the list (which starts at offset 30) is aligned
static inline int pointnum_list_size(int nv)
{
	return ((nv & ~1) + 1) * 2;
}

//give the interpreter an array of points to use
void g3_set_interp_points(g3s_point *pointlist)
{
	Interp_point_list = pointlist;
}

//rotate a list of points from the model data
static void rotate_point_list(g3s_point *dest,ubyte *src,int n)
{
	vms_vector v;

	while (n--) {
		g3_rotate_point(dest++,get_vec(&v,src));
		src += 12;
	}
}

//rotate a list of points from the alternate (morph) points
static void morph_rotate_list(g3s_point *dest,int n)
{
	vms_vector *src = morph_points;		//get alternate points

	while (n--)
		g3_rotate_point(dest++,src++);
}

//the light value for a face, from its surface normal and the model light
static fix calc_face_light(ubyte *normp)
{
	vms_vector norm;
	fix light;

	light = g3_neg(vm_vec_dotprod(&View_matrix.fvec,get_vec(&norm,normp)));

	//scale light by model light

	light = (fix) (light + light + light) >> 2;		//light *= 3/4
	light += f1_0/4;										//light = 1/4 + light * 3/4

	return fixmul(light,model_light);
}

//ASCENT: the light value for a corner of a face, from the normal of the
//corner. A normal that points away from the viewer gives the base light.
static fix calc_corner_light(ubyte *normp)
{
	vms_vector norm;
	fix light;

	light = g3_neg(vm_vec_dotprod(&View_matrix.fvec,get_vec(&norm,normp)));
	if (light < 0)
		light = 0;

	light = (fix) (light + light + light) >> 2;
	light += f1_0/4;

	return fixmul(light,model_light);
}

void g3_set_interp_smooth(ubyte *data,ubyte *smooth)
{
	Interp_smooth_data = data;
	Interp_smooth = smooth;
}

//takes count, ptr to point number list
static void draw_outline(int nv,ubyte *pointnums)
{
	int i;

	gr_setcolor(255);		//bright white

	for (i=0;i<nv-1;i++)
		g3_draw_line(&Interp_point_list[get_w(pointnums+i*2)],&Interp_point_list[get_w(pointnums+i*2+2)]);

	//the last point to the first point
	g3_draw_line(&Interp_point_list[get_w(pointnums+(nv-1)*2)],&Interp_point_list[get_w(pointnums)]);
}

//the opcode handlers. Each takes the pointer to its opcode

//draw a flat-shaded polygon
static void op_flatpoly(ubyte *p)
{
	int nv = get_w(p+2);		//num verts
	vms_vector pnt,norm;
	int i;

	if (nv <= 0 || nv > MAX_POINTS_IN_POLY)
		return;

	if (! g3_check_normal_facing(get_vec(&pnt,p+4),get_vec(&norm,p+16)))
		return;

	//polygon is facing, so draw it

	gr_setcolor(get_w(p+28));		//get color, set it

	//make list of point pointers

	for (i=0;i<nv;i++)
		point_list[i] = &Interp_point_list[get_w(p+30+i*2)];

	g3_draw_poly(nv,point_list);

	if (g3d_interp_outline)
		draw_outline(nv,p+30);
}

//draw a texture map
static void op_tmappoly(ubyte *p)
{
	int nv = get_w(p+2);		//num verts
	vms_vector pnt,norm;
	grs_bitmap *bm;
	ubyte *uvlp;
	fix light;
	int i,smooth = 0;

	if (nv <= 0 || nv > MAX_POINTS_IN_POLY)
		return;

	if (! g3_check_normal_facing(get_vec(&pnt,p+4),get_vec(&norm,p+16)))
		return;

	//polygon is facing, so draw it

	bm = bitmap_ptr[get_w(p+28)];		//get bitmap number

	uvlp = p + 30 + pointnum_list_size(nv);		//get uvl list

	//calculate light from surface normal

	if (glow_num >= 0) {		//glow override?

		//special glow lighting, which doesn't care about surface normal

		light = glow_values[glow_num];
		glow_num = -1;
	}
	else {
		light = calc_face_light(p+16);
		smooth = Interp_smooth != NULL;
	}

	//now poke light into l values. The assembly wrote the value into the
	//model data and gave the uvl list in the model data to g3_draw_tmap().
	//This code writes the value too, and gives a copy of the list.

	for (i=0;i<nv;i++) {
		put_fix(uvlp+i*12+8,light);

		uvl_copy[i].u = get_fix(uvlp+i*12);
		uvl_copy[i].v = get_fix(uvlp+i*12+4);
		uvl_copy[i].l = light;
	}

	//ASCENT: an improved model has a normal for each corner
	if (smooth)
		for (i=0;i<nv;i++)
			uvl_copy[i].l = calc_corner_light(Interp_smooth + (uvlp - Interp_smooth_data) + i*12);

	//now draw it

	//make list of point pointers

	for (i=0;i<nv;i++)
		point_list[i] = &Interp_point_list[get_w(p+30+i*2)];

	g3_draw_tmap(nv,point_list,uvl_copy,bm);

	if (g3d_interp_outline)
		draw_outline(nv,p+30);
}

//Special code to draw morphing objects

//draw a flat-shaded polygon
static void morph_flatpoly(ubyte *p)
{
	int nv = get_w(p+2);		//num verts
	ubyte *pn = p+30;			//point number list
	int i;

	//set color

	gr_setcolor(get_w(p+28));		//get color, set it

	//check and draw

	for (i=0;i<3;i++,pn+=2)
		morph_pointlist[i] = &Interp_point_list[get_w(pn)];

	if (nv == 3)		//3 points is good!

		g3_check_and_draw_poly(3,morph_pointlist,NULL,NULL);		//no normal, must compute

	else

		//ASCENT: with less than 3 points the assembly loop would not
		//stop. Here nothing is drawn. The assembly also read one point
		//number past the list after the last triangle. This code does not.

		for (i=nv-2;i>0;i--) {		//tri count

			g3_check_and_draw_poly(3,morph_pointlist,NULL,NULL);		//always draw triangle

			morph_pointlist[1] = morph_pointlist[2];

			if (i > 1) {
				morph_pointlist[2] = &Interp_point_list[get_w(pn)];
				pn += 2;
			}
		}
}

//draw a texture map
static void morph_tmappoly(ubyte *p)
{
	int nv = get_w(p+2);		//num verts
	ubyte *pn = p+30;			//point number list
	ubyte *uvlp;
	grs_bitmap *bm;
	fix light;
	int i;

	//get bitmap

	bm = bitmap_ptr[get_w(p+28)];		//get bitmap number

	uvlp = p + 30 + pointnum_list_size(nv);		//get uvl list

	//calculate light from surface normal

	light = calc_face_light(p+16);

	if (light < 0)
		light = 0;

	//now light=plane light value

	morph_uvls[0].l = morph_uvls[1].l = morph_uvls[2].l = light;

	for (i=0;i<3;i++,pn+=2)
		morph_pointlist[i] = &Interp_point_list[get_w(pn)];

	if (nv < 3)		//something is bad, abort
		return;

	if (nv == 3) {		//3 points is good!

		//poke in light values. See op_tmappoly() for the copy.

		for (i=0;i<3;i++) {
			put_fix(uvlp+i*12+8,light);

			uvl_copy[i].u = get_fix(uvlp+i*12);
			uvl_copy[i].v = get_fix(uvlp+i*12+4);
			uvl_copy[i].l = light;
		}

		g3_check_and_draw_tmap(3,morph_pointlist,uvl_copy,bm,NULL,NULL);		//no normal
	}
	else {

		for (i=0;i<3;i++) {
			morph_uvls[i].u = get_fix(uvlp+i*12);
			morph_uvls[i].v = get_fix(uvlp+i*12+4);
		}

		uvlp += 3*12;

		//ASCENT: the assembly read one point number and one uv pair past
		//the lists after the last triangle. This code does not.

		for (i=nv-2;i>0;i--) {		//tri count

			g3_check_and_draw_tmap(3,morph_pointlist,morph_uvls,bm,NULL,NULL);		//always draw triangle

			morph_pointlist[1] = morph_pointlist[2];

			morph_uvls[1].u = morph_uvls[2].u;
			morph_uvls[1].v = morph_uvls[2].v;

			if (i > 1) {
				morph_pointlist[2] = &Interp_point_list[get_w(pn)];
				pn += 2;

				morph_uvls[2].u = get_fix(uvlp);
				morph_uvls[2].v = get_fix(uvlp+4);
				uvlp += 12;
			}
		}
	}
}

//the interpreter. takes the pointer to the first opcode, and runs until
//the eof opcode (the end of a model or of a sub-routine)
static void interp_run(ubyte *p)
{
	vms_vector v,norm;
	int n;

	for (;;)

		switch (get_w(p)) {

			case OP_EOF:
				return;

			case OP_DEFPOINTS:		//define a list of points

				n = get_w(p+2);		//num points

				if (morph_mode)
					morph_rotate_list(Interp_point_list,n);
				else
					rotate_point_list(Interp_point_list,p+4,n);

				p += 4 + n*12;
				break;

			case OP_DEFP_START:		//define a list of points, with starting point num specified

				n = get_w(p+2);		//num points

				//get_w(p+4) is the starting point num

				if (morph_mode)
					morph_rotate_list(&Interp_point_list[get_w(p+4)],n);
				else
					rotate_point_list(&Interp_point_list[get_w(p+4)],p+8,n);

				p += 8 + n*12;
				break;

			case OP_FLATPOLY:

				if (morph_mode)
					morph_flatpoly(p);
				else
					op_flatpoly(p);

				//polygon is not facing (or we've plotted it). go to next opcode

				p += 30 + pointnum_list_size(get_w(p+2));
				break;

			case OP_TMAPPOLY:

				if (morph_mode)
					morph_tmappoly(p);
				else
					op_tmappoly(p);

				//polygon is not facing (or we've plotted it). go to next opcode

				n = get_w(p+2);
				p += 30 + pointnum_list_size(n) + n*12;		//point past uvls
				break;

			case OP_SORTNORM:			//sort based on surface normal

				if (g3_check_normal_facing(get_vec(&v,p+16),get_vec(&norm,p+4))) {

					//is facing.  draw back then front

					interp_run(p + get_w(p+30));
					interp_run(p + get_w(p+28));
				}
				else {

					//is not facing.  draw front then back

					interp_run(p + get_w(p+28));
					interp_run(p + get_w(p+30));
				}

				p += 32;
				break;

			case OP_RODBM:				//draw a rod bitmap

				g3_rotate_point(&rod_bot_p,get_vec(&v,p+20));		//bot point
				g3_rotate_point(&rod_top_p,get_vec(&v,p+4));			//top point

				//ASCENT: the assembly gave no light value to
				//g3_draw_rod_tmap(). The register for the light value had
				//the address of the bitmap list. Here the light value is 1.0.

				g3_draw_rod_tmap(bitmap_ptr[get_w(p+2)],&rod_bot_p,get_fix(p+16),&rod_top_p,get_fix(p+32),f1_0);

				p += 36;
				break;

			case OP_SUBCALL: {		//draw a subobject
				vms_angvec *a;

				//get ptr to angles. If not specified, use zero angles

				if (anim_angles)
					a = &anim_angles[get_w(p+2)];		//get object number
				else
					a = &zero_angles;

				g3_start_instance_angles(get_vec(&v,p+4),a);		//get position

				interp_run(p + get_w(p+16));		//draw the subobject

				g3_done_instance();

				p += 20;
				break;
			}

			case OP_GLOW:				//set the glow value for the next tmap

				if (glow_values)
					glow_num = get_w(p+2);

				p += 4;
				break;

			default:
				//ASCENT: the assembly had only a debug check for an invalid
				//opcode, and jumped to an address from past the opcode table.
				//Here the interpreter stops.
				return;
		}
}

//interpreter to draw polygon model
//takes ptr to object, ptr to array of bitmap pointers,
//ptr to anim angles, light value, ptr to array of glow values
//ASCENT: the assembly gave back no defined value. This function returns 1.
bool g3_draw_polygon_model(void *model_ptr,grs_bitmap **model_bitmaps,vms_angvec *angles,fix light,fix *glow)
{
	bitmap_ptr = model_bitmaps;		//save ptr to bitmap array
	anim_angles = angles;
	model_light = light;
	glow_values = glow;

	glow_num = -1;

	interp_run((ubyte *) model_ptr);

	return 1;
}

//interpreter to draw morphing polygon model
//takes ptr to object, ptr to array of bitmap pointers,
//ptr to anim angles, light value, alternate points
//ASCENT: the assembly gave back no defined value. This function returns 1.
bool g3_draw_morphing_model(void *model_ptr,grs_bitmap **model_bitmaps,vms_angvec *angles,fix light,vms_vector *new_points)
{
	int save_morph_mode = morph_mode;

	bitmap_ptr = model_bitmaps;		//save ptr to bitmap array
	anim_angles = angles;
	morph_points = new_points;
	model_light = light;

	//set alternate opcode handlers

	morph_mode = 1;

	interp_run((ubyte *) model_ptr);

	morph_mode = save_morph_mode;

	return 1;
}

//walks the opcodes of a model, or of a sub-routine of the model
static void init_loop(ubyte *p)
{
	int n;

	for (;;)

		switch (get_w(p)) {

			case OP_EOF:
				return;

			case OP_DEFPOINTS:
				n = get_w(p+2);		//get count
				p += 4 + n*12;
				break;

			case OP_FLATPOLY:

				//translate the 15 bit color to a palette index
				put_w(p+28,(uint) gr_find_closest_color_15bpp(get_w(p+28)));		//get color, store new color

				p += 30 + pointnum_list_size(get_w(p+2));
				break;

			case OP_TMAPPOLY:

				n = (short) get_w(p+28);		//get bitmap number

				if (n > highest_texture_num)
					highest_texture_num = n;

				n = get_w(p+2);			//get nverts
				p += 30 + pointnum_list_size(n) + n*12;		//skip the uvls
				break;

			case OP_SORTNORM:
				init_loop(p + get_w(p+28));		//get front offset
				init_loop(p + get_w(p+30));		//get back offset
				p += 32;
				break;

			case OP_RODBM:
				p += 36;
				break;

			case OP_SUBCALL:
				init_loop(p + get_w(p+16));		//get subobj offset
				p += 20;
				break;

			case OP_DEFP_START:
				n = get_w(p+2);		//get count
				p += 8 + n*12;
				break;

			case OP_GLOW:
				p += 4;
				break;

			default:
				//ASCENT: the assembly did not go past an invalid opcode, and
				//did not stop. Here the walk stops.
				return;
		}
}

//initialize a polygon object
//translate colors, find the highest texture number
//takes ptr to model
void g3_init_polygon_model(void *model_ptr)
{
	highest_texture_num = -1;

	init_loop((ubyte *) model_ptr);
}
