/*
 * ASCENT: test program for the 3d library (the C code that replaces the
 * assembly files of engine/3d).
 *
 * The expected values come from two sources:
 *  - values derived by hand from the assembly (see the comment at each test)
 *  - an integer model of the assembly instructions (imul, add/adc, shrd,
 *    idiv, the divcheck macro), which is in this file and does not use the
 *    code of the library.
 *
 * The program has its own small versions of the 2d and texture mapper
 * functions that the library calls. They record their parameters.
 */

#include <stdio.h>
#include <string.h>

#include "globvars.h"
#include "clipper.h"

extern short highest_texture_num;

//------------------------------------------------------------------------
//Test support

static int n_checks,n_fails;

#define CHECK(cond) do { \
	n_checks++; \
	if (!(cond)) { n_fails++; printf("FAIL line %d: %s\n",__LINE__,#cond); } \
} while (0)

#define CHECK_EQ(a,b) do { \
	long long va_ = (long long) (a), vb_ = (long long) (b); \
	n_checks++; \
	if (va_ != vb_) { n_fails++; printf("FAIL line %d: %s = %lld (0x%llx), expected %lld (0x%llx)\n",__LINE__,#a,va_,va_ & 0xffffffffLL,vb_,vb_ & 0xffffffffLL); } \
} while (0)

//a is within tol of b
#define CHECK_NEAR(a,b,tol) do { \
	long long va_ = (long long) (a), vb_ = (long long) (b); \
	n_checks++; \
	if (va_ - vb_ > (tol) || vb_ - va_ > (tol)) { n_fails++; printf("FAIL line %d: %s = %lld, expected %lld +-%d\n",__LINE__,#a,va_,vb_,(int) (tol)); } \
} while (0)

static uint32_t rnd_state = 0x12345678;

static uint32_t rnd(void)
{
	rnd_state = rnd_state * 1664525u + 1013904223u;
	return rnd_state;
}

//random value in the range -range..range
static fix rnd_range(fix range)
{
	return (fix) (rnd() % (uint32_t) (range*2+1)) - range;
}

//------------------------------------------------------------------------
//The functions of the other libraries that the 3d library calls

static grs_canvas test_canvas;
static grs_screen test_screen;

grs_canvas *grd_curcanv = &test_canvas;
grs_screen *grd_curscreen = &test_screen;

static int n_init_interface;

void init_interface_vars_to_assembler(void)
{
	n_init_interface++;
}

//a record of each call of a drawer, in order

#define REC_SETCOLOR	1
#define REC_LINE		2
#define REC_DISK		3
#define REC_CLEAR		4
#define REC_FLAT		5
#define REC_TMAP		6
#define REC_SCALE		7

#define MAX_REC		64
#define MAX_REC_PTS	16

typedef struct rec {
	int type;
	int nv;
	int color;
	void *bm;
	fix v[MAX_REC_PTS*2];				//2d coords (flat, line, disk, scale)
	g3s_point pts[MAX_REC_PTS];		//copies of the points (tmap)
	g3s_point *ptrs[MAX_REC_PTS];		//the pointers to the points (tmap)
} rec;

static rec recs[MAX_REC];
static int n_recs;

static rec *new_rec(int type)
{
	rec *r = &recs[(n_recs < MAX_REC) ? n_recs : MAX_REC-1];

	n_recs++;
	memset(r,0,sizeof(*r));
	r->type = type;

	return r;
}

static void clear_recs(void)
{
	n_recs = 0;
}

void gr_setcolor(int color)
{
	new_rec(REC_SETCOLOR)->color = color;
}

static int line_ret = 1;

int gr_line(fix x0,fix y0,fix x1,fix y1)
{
	rec *r = new_rec(REC_LINE);

	r->v[0] = x0; r->v[1] = y0; r->v[2] = x1; r->v[3] = y1;

	return line_ret;
}

int gr_disk(fix x,fix y,fix r_)
{
	rec *r = new_rec(REC_DISK);

	r->v[0] = x; r->v[1] = y; r->v[2] = r_;

	return 0;
}

void gr_clear_canvas(int color)
{
	new_rec(REC_CLEAR)->color = color;
}

void gr_upoly_tmap(int nverts,int *vert)
{
	rec *r = new_rec(REC_FLAT);
	int i;

	r->nv = nverts;
	for (i=0;i<nverts*2 && i<MAX_REC_PTS*2;i++)
		r->v[i] = vert[i];
}

void draw_tmap(grs_bitmap *bp,int nverts,g3s_point **vertbuf)
{
	rec *r = new_rec(REC_TMAP);
	int i;

	r->nv = nverts;
	r->bm = bp;
	for (i=0;i<nverts && i<MAX_REC_PTS;i++) {
		r->pts[i] = *vertbuf[i];
		r->ptrs[i] = vertbuf[i];
	}
}

void scale_bitmap(grs_bitmap *bp,grs_point *vertbuf)
{
	rec *r = new_rec(REC_SCALE);

	r->bm = bp;
	memcpy(r->v,vertbuf,6*sizeof(fix));
}

//the test version of the color search: a value that the test can check
int gr_find_closest_color_15bpp(int rgb)
{
	return (rgb ^ 0x5a) & 0xff;
}

//alternate drawers for g3_set_special_render()

static int n_special_tmap,n_special_flat,n_special_line;

static void special_tmap(grs_bitmap *bm,int nv,g3s_point **vertlist) { n_special_tmap++; }
static void special_flat(int nv,int *vertlist) { n_special_flat++; }
static int special_line(fix x0,fix y0,fix x1,fix y1) { n_special_line++; return 7; }

//------------------------------------------------------------------------
//The integer model of the assembly. Registers are 32 bits. A product is
//the pair edx:eax.

typedef struct reg64 { uint32_t lo,hi; } reg64;		//eax, edx

//imul: signed 32x32 -> 64. Made from 16-bit parts, with no 64-bit multiply
static reg64 m_imul(fix a,fix b)
{
	uint32_t ua = (uint32_t) a,ub = (uint32_t) b;
	uint32_t a0 = ua & 0xffff,a1 = ua >> 16,b0 = ub & 0xffff,b1 = ub >> 16;
	uint32_t p00 = a0*b0,p01 = a0*b1,p10 = a1*b0,p11 = a1*b1;
	uint32_t mid,carry;
	reg64 r;

	mid = (p00 >> 16) + (p01 & 0xffff) + (p10 & 0xffff);
	r.lo = (p00 & 0xffff) | (mid << 16);
	carry = mid >> 16;
	r.hi = p11 + (p01 >> 16) + (p10 >> 16) + carry;

	//unsigned to signed product
	if (a < 0) r.hi -= ub;
	if (b < 0) r.hi -= ua;

	return r;
}

//add eax,..  adc edx,..
static reg64 m_add(reg64 a,reg64 b)
{
	reg64 r;

	r.lo = a.lo + b.lo;
	r.hi = a.hi + b.hi + (r.lo < a.lo);

	return r;
}

//shrd eax,edx,16
static fix m_shrd16(reg64 a)
{
	return (fix) ((a.lo >> 16) | (a.hi << 16));
}

//the fixmul macro
static fix m_fixmul(fix a,fix b)
{
	return m_shrd16(m_imul(a,b));
}

//idiv, for the cases with no fault
static fix m_idiv(reg64 n,fix d)
{
	int64_t num = (int64_t) (((uint64_t) n.hi << 32) | n.lo);

	return (fix) (num / d);
}

//the fixdiv macro: mov edx,eax / sar edx,16 / shl eax,16 / idiv
static fix m_fixdiv(fix a,fix b)
{
	reg64 n;

	n.hi = (uint32_t) (a >> 16);
	n.lo = (uint32_t) a << 16;

	return m_idiv(n,b);
}

//the divcheck macro. returns true for overflow
static int m_divcheck(reg64 n,fix den)
{
	uint32_t ecx = n.hi;

	if (ecx & 0x80000000u)		//or ecx,ecx / jns / neg ecx
		ecx = ~ecx + 1;

	ecx = (ecx << 1) | (n.lo >> 31);		//shld ecx,eax,1

	return ecx >= (uint32_t) den;		//jae
}

//vm_vec_rotate (the vv_mul macro): three products, 64-bit sum, shrd 16
static fix m_dot3(fix a0,fix b0,fix a1,fix b1,fix a2,fix b2)
{
	return m_shrd16(m_add(m_add(m_imul(a0,b0),m_imul(a1,b1)),m_imul(a2,b2)));
}

//the model of the view: position, scaled matrix (as nine values), canvas
static vms_vector m_pos;
static vms_matrix m_mat;
static fix m_w2,m_h2;

//scale_matrix of matrix.asm
static void m_set_view(vms_vector *pos,vms_matrix *mat,fix zoom,vms_vector *win_scale,vms_vector *scale_out)
{
	vms_vector s = *win_scale;
	fix t;

	m_pos = *pos;
	m_mat = *mat;

	if (zoom > f1_0) {
		t = m_fixdiv(f1_0,zoom);
		s.x = m_fixmul(s.x,t);
		s.y = m_fixmul(s.y,t);
	}
	else
		s.z = m_fixmul(s.z,zoom);

	m_mat.rvec.x = m_fixmul(m_mat.rvec.x,s.x);
	m_mat.rvec.y = m_fixmul(m_mat.rvec.y,s.x);
	m_mat.rvec.z = m_fixmul(m_mat.rvec.z,s.x);

	m_mat.uvec.x = m_fixmul(m_mat.uvec.x,s.y);
	m_mat.uvec.y = m_fixmul(m_mat.uvec.y,s.y);
	m_mat.uvec.z = m_fixmul(m_mat.uvec.z,s.y);

	m_mat.fvec.x = m_fixmul(m_mat.fvec.x,s.z);
	m_mat.fvec.y = m_fixmul(m_mat.fvec.y,s.z);
	m_mat.fvec.z = m_fixmul(m_mat.fvec.z,s.z);

	*scale_out = s;
}

//code_point of points.asm
static ubyte m_code(fix x,fix y,fix z)
{
	ubyte bl = 0;
	uint32_t ecx = (uint32_t) z;

	if (x > (fix) ecx) bl |= 2;
	if (y > (fix) ecx) bl |= 8;
	ecx = ~ecx + 1;					//neg ecx
	if (!(ecx & 0x80000000u)) bl |= 0x80;		//js not_behind
	if (x < (fix) ecx) bl |= 1;
	if (y < (fix) ecx) bl |= 4;

	return bl;
}

//g3_rotate_point and g3_project_point of points.asm
static void m_rotate_project(g3s_point *p,vms_vector *src)
{
	fix tx = (fix) ((uint32_t) src->x - (uint32_t) m_pos.x);
	fix ty = (fix) ((uint32_t) src->y - (uint32_t) m_pos.y);
	fix tz = (fix) ((uint32_t) src->z - (uint32_t) m_pos.z);
	reg64 n;

	memset(p,0,sizeof(*p));

	//x = x*m1 + y*m4 + z*m7 and so on. m1,m4,m7 are the right vector
	p->x = m_dot3(tx,m_mat.rvec.x,ty,m_mat.rvec.y,tz,m_mat.rvec.z);
	p->y = m_dot3(tx,m_mat.uvec.x,ty,m_mat.uvec.y,tz,m_mat.uvec.z);
	p->z = m_dot3(tx,m_mat.fvec.x,ty,m_mat.fvec.y,tz,m_mat.fvec.z);

	p->p3_flags = 0;
	p->p3_codes = m_code(p->x,p->y,p->z);

	if (p->p3_codes & 0x80)
		return;

	n = m_imul(p->x,m_w2);
	if (m_divcheck(n,p->z)) { p->p3_flags = 2; return; }
	p->p3_sx = (fix) ((uint32_t) m_idiv(n,p->z) + (uint32_t) m_w2);

	n = m_imul(p->y,m_h2);
	if (m_divcheck(n,p->z)) { p->p3_flags = 2; return; }
	p->p3_sy = (fix) (0u - (uint32_t) m_idiv(n,p->z) + (uint32_t) m_h2);

	p->p3_flags |= 1;
}

//------------------------------------------------------------------------
//Helpers

static vms_matrix ident = {{f1_0,0,0},{0,f1_0,0},{0,0,f1_0}};
static vms_vector zero_vec = {0,0,0};

//set a 320x200 canvas. With the aspect value 0x1999a the window scale is 1:
//0x1999a * 200 / 320 = 65536.25, and idiv truncates to 0x10000
static void set_canvas(int w,int h,fix aspect)
{
	test_canvas.cv_bitmap.bm_w = w;
	test_canvas.cv_bitmap.bm_h = h;
	test_screen.sc_aspect = aspect;

	g3_start_frame();

	m_w2 = w << 15;
	m_h2 = h << 15;
}

//the view for the hand-made cases: 320x200, eye at 0,0,0, no rotation, zoom 1
static void set_simple_view(void)
{
	set_canvas(320,200,0x1999a);
	g3_set_view_matrix(&zero_vec,&ident,f1_0);
	g3_set_special_render(NULL,NULL,NULL);
}

static void set_point(g3s_point *p,int x,int y,int z)
{
	vms_vector v;

	v.x = i2f(x); v.y = i2f(y); v.z = i2f(z);
	g3_rotate_point(p,&v);
}

#define SIDE_CODES (CC_OFF_LEFT+CC_OFF_RIGHT+CC_OFF_BOT+CC_OFF_TOP)

//------------------------------------------------------------------------
//The tests

static void test_setup(void)
{
	n_init_interface = 0;

	set_canvas(320,200,0x1999a);

	CHECK_EQ(Canvas_width,320);
	CHECK_EQ(Canvas_height,200);
	CHECK_EQ(Canv_w2,0xa00000);		//160.0
	CHECK_EQ(Canv_h2,0x640000);		//100.0
	CHECK_EQ(Window_scale.x,f1_0);
	CHECK_EQ(Window_scale.y,f1_0);
	CHECK_EQ(Window_scale.z,f1_0);
	CHECK_EQ(n_init_interface,1);

	//aspect 2.0: 0x20000 * 200 / 320 = 0x14000 > 1, so y = 1/1.25:
	//0x100000000 / 0x14000 = 52428.8, truncated to 0xcccc
	set_canvas(320,200,0x20000);

	CHECK_EQ(Window_scale.x,f1_0);
	CHECK_EQ(Window_scale.y,0xcccc);
	CHECK_EQ(Window_scale.z,f1_0);

	//aspect 1.0: 0x10000 * 200 / 320 = 0xa000
	set_canvas(320,200,0x10000);

	CHECK_EQ(Window_scale.x,0xa000);
	CHECK_EQ(Window_scale.y,f1_0);
}

static void test_view_matrix(void)
{
	vms_vector pos = {i2f(1),i2f(2),i2f(3)};
	vms_vector scale;
	vms_matrix mat;
	int i,j;

	set_canvas(320,200,0x1999a);

	//zoom 1: nothing is scaled
	g3_set_view_matrix(&pos,&ident,f1_0);

	CHECK(memcmp(&View_matrix,&ident,sizeof(ident)) == 0);
	CHECK(memcmp(&Unscaled_matrix,&ident,sizeof(ident)) == 0);
	CHECK(memcmp(&View_position,&pos,sizeof(pos)) == 0);
	CHECK_EQ(View_zoom,f1_0);
	CHECK_EQ(Matrix_scale.x,f1_0);
	CHECK_EQ(Matrix_scale.y,f1_0);
	CHECK_EQ(Matrix_scale.z,f1_0);

	//zoom 2 (> 1): x and y are scaled by 1/zoom = 0x8000
	g3_set_view_matrix(&pos,&ident,f2_0);

	CHECK_EQ(Matrix_scale.x,0x8000);
	CHECK_EQ(Matrix_scale.y,0x8000);
	CHECK_EQ(Matrix_scale.z,f1_0);
	CHECK_EQ(View_matrix.rvec.x,0x8000);
	CHECK_EQ(View_matrix.uvec.y,0x8000);
	CHECK_EQ(View_matrix.fvec.z,f1_0);
	CHECK(memcmp(&Unscaled_matrix,&ident,sizeof(ident)) == 0);

	//zoom 0.5 (<= 1): z is scaled by zoom
	g3_set_view_matrix(&pos,&ident,0x8000);

	CHECK_EQ(Matrix_scale.x,f1_0);
	CHECK_EQ(Matrix_scale.y,f1_0);
	CHECK_EQ(Matrix_scale.z,0x8000);
	CHECK_EQ(View_matrix.rvec.x,f1_0);
	CHECK_EQ(View_matrix.uvec.y,f1_0);
	CHECK_EQ(View_matrix.fvec.z,0x8000);

	//random matrices, zooms and window scales against the model
	for (i=0;i<200;i++) {
		fix zoom = (fix) (rnd() % 0x40000) + 0x1000;
		fix *m = (fix *) &mat;

		for (j=0;j<9;j++)
			m[j] = rnd_range(f1_0);

		set_canvas(320,200,(fix) (rnd() % 0x30000) + 0x4000);

		g3_set_view_matrix(&pos,&mat,zoom);
		m_set_view(&pos,&mat,zoom,&Window_scale,&scale);

		CHECK(memcmp(&View_matrix,&m_mat,sizeof(mat)) == 0);
		CHECK(memcmp(&Unscaled_matrix,&mat,sizeof(mat)) == 0);
		CHECK(memcmp(&Matrix_scale,&scale,sizeof(scale)) == 0);
	}
}

static void test_codes(void)
{
	//each line: x,y,z, expected codes. derived by hand from code_point
	static const struct { fix x,y,z; ubyte codes; } t[] = {
		{0,0,1,						0},
		{1,1,1,						0},								//on the planes is not off
		{-1,-1,1,					0},
		{2,0,1,						CC_OFF_RIGHT},
		{-2,0,1,						CC_OFF_LEFT},
		{0,2,1,						CC_OFF_TOP},
		{0,-2,1,						CC_OFF_BOT},
		{2,2,1,						CC_OFF_RIGHT+CC_OFF_TOP},
		{-2,-2,1,					CC_OFF_LEFT+CC_OFF_BOT},
		{0,0,0,						CC_BEHIND},
		{0,0,-1,						CC_BEHIND+CC_OFF_RIGHT+CC_OFF_TOP+CC_OFF_LEFT+CC_OFF_BOT},
		{5,-5,-3,					CC_BEHIND+CC_OFF_RIGHT+CC_OFF_BOT},
		{-5,5,-3,					CC_BEHIND+CC_OFF_LEFT+CC_OFF_TOP},
		{i2f(10),i2f(20),i2f(100),	0},
		//the negation of 0x80000000 is negative, so the point is not behind
		{0,0,INT32_MIN,			CC_OFF_RIGHT+CC_OFF_TOP},
	};
	int i;
	g3s_point p;

	for (i=0;i<(int) (sizeof(t)/sizeof(t[0]));i++) {
		memset(&p,0,sizeof(p));
		p.x = t[i].x; p.y = t[i].y; p.z = t[i].z;

		CHECK_EQ(g3_code_point(&p),t[i].codes);
		CHECK_EQ(p.p3_codes,t[i].codes);
		CHECK_EQ(m_code(t[i].x,t[i].y,t[i].z),t[i].codes);		//the model agrees with the hand values
	}
}

static void test_rotate_project(void)
{
	g3s_point p,mp;
	vms_vector v,pos;
	vms_matrix mat;
	vms_vector scale;
	int i,j,n_proj=0,n_ovf=0,n_behind=0;
	g3s_point *list[3];
	g3s_point pa,pb,pc;
	g3s_codes cc;

	set_simple_view();

	//By hand: x,y,z do not change. sx = 10 * 160 / 100 + 160 = 176,
	//sy = -(20 * 100 / 100) + 100 = 80. The divides are exact.
	set_point(&p,10,20,100);

	CHECK_EQ(p.x,i2f(10)); CHECK_EQ(p.y,i2f(20)); CHECK_EQ(p.z,i2f(100));
	CHECK_EQ(p.p3_codes,0);
	CHECK_EQ(p.p3_flags,0);

	g3_project_point(&p);

	CHECK_EQ(p.p3_flags,PF_PROJECTED);
	CHECK_EQ(p.p3_sx,i2f(176));
	CHECK_EQ(p.p3_sy,i2f(80));

	//a second call does not project again
	p.p3_sx = 1; p.p3_sy = 2;
	g3_project_point(&p);
	CHECK_EQ(p.p3_sx,1); CHECK_EQ(p.p3_sy,2);

	//By hand, with truncation: x = 1, y = 1, z = 3 (raw values).
	//1 * 0xa00000 / 3 = 3495253.33, truncated to 0x355555.
	//1 * 0x640000 / 3 = 2184533.33, truncated to 0x215555, negated and
	//added to 0x640000
	memset(&p,0,sizeof(p));
	p.x = 1; p.y = 1; p.z = 3;
	g3_code_point(&p);
	g3_project_point(&p);

	CHECK_EQ(p.p3_flags,PF_PROJECTED);
	CHECK_EQ(p.p3_sx,0xa00000 + 0x355555);
	CHECK_EQ(p.p3_sy,0x640000 - 0x215555);

	//The overflow check is not the same for a negative numerator. By hand:
	//y = -1, z = 3: the product is -0x640000 = 0xffffffff:ff9c0000. The
	//negated high dword is 1. Shifted left with the top bit of the low
	//dword (1) it is 3, not below z = 3: overflow, and no divide.
	//With z = 4 the divide is done: -0x640000 / 4 = -0x190000
	memset(&p,0,sizeof(p));
	p.x = 1; p.y = -1; p.z = 3;
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,PF_OVERFLOW);

	memset(&p,0,sizeof(p));
	p.x = 1; p.y = -1; p.z = 4;
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,PF_PROJECTED);
	CHECK_EQ(p.p3_sx,0xa00000 + 0x280000);
	CHECK_EQ(p.p3_sy,0x640000 + 0x190000);

	//a point behind the eye is not projected
	set_point(&p,0,0,-5);
	CHECK(p.p3_codes & CC_BEHIND);
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,0);

	//divide overflow by hand: x = 1000.0, z = 1 (raw). The product is
	//0x3e80000 * 0xa00000 = 0x271000:00000000. The high dword * 2 is
	//bigger than z, so the divide is not done and the flags are PF_OVERFLOW
	memset(&p,0,sizeof(p));
	p.x = i2f(1000); p.y = 0; p.z = 1;
	p.p3_flags = PF_TEMP_POINT;		//the assembly stores the flag byte, it does not or it
	g3_code_point(&p);
	g3_project_point(&p);

	CHECK_EQ(p.p3_flags,PF_OVERFLOW);

	//the limit of the check, by hand: z = 0x10000.
	//x = 0x333: 0x333 * 0xa00000 = 0x1:ffe00000. hi*2 + top bit of lo = 3,
	//less than z, the divide is done.
	//With z = 3 the same numerator gives 3 >= 3, overflow.
	memset(&p,0,sizeof(p));
	p.x = 0x333; p.z = 0x10000;
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,PF_PROJECTED);
	CHECK_EQ(p.p3_sx,0xa00000 + 0x1ffe0);		//0x1ffe00000 / 0x10000

	memset(&p,0,sizeof(p));
	p.x = 0x333; p.z = 3;
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,PF_OVERFLOW);

	//overflow in the y divide: sx is stored, then the flags are PF_OVERFLOW
	//0x400 * 0x640000 = 0x1:90000000, the check value is 3
	memset(&p,0,sizeof(p));
	p.x = 0; p.y = 0x400; p.z = 3;
	g3_project_point(&p);
	CHECK_EQ(p.p3_flags,PF_OVERFLOW);
	CHECK_EQ(p.p3_sx,0xa00000);

	//random views and points against the model
	for (i=0;i<100;i++) {
		fix zoom = (fix) (rnd() % 0x30000) + 0x2000;
		fix *m = (fix *) &mat;

		for (j=0;j<9;j++)
			m[j] = rnd_range(f1_0);

		pos.x = rnd_range(i2f(500)); pos.y = rnd_range(i2f(500)); pos.z = rnd_range(i2f(500));

		set_canvas(320 + (int) (rnd() % 320),200 + (int) (rnd() % 280),(fix) (rnd() % 0x20000) + 0x8000);

		g3_set_view_matrix(&pos,&mat,zoom);
		m_set_view(&pos,&mat,zoom,&Window_scale,&scale);

		for (j=0;j<200;j++) {
			fix range = (j & 1) ? i2f(1000) : (j & 2) ? i2f(20) : 0x7fffffff;
			ubyte codes;

			v.x = rnd_range(range); v.y = rnd_range(range); v.z = rnd_range(range);

			memset(&p,0,sizeof(p));
			codes = g3_rotate_point(&p,&v);
			g3_project_point(&p);

			m_rotate_project(&mp,&v);

			CHECK_EQ(p.x,mp.x); CHECK_EQ(p.y,mp.y); CHECK_EQ(p.z,mp.z);
			CHECK_EQ(codes,mp.p3_codes);
			CHECK_EQ(p.p3_codes,mp.p3_codes);
			CHECK_EQ(p.p3_flags,mp.p3_flags);
			if (mp.p3_flags & PF_PROJECTED) {
				CHECK_EQ(p.p3_sx,mp.p3_sx); CHECK_EQ(p.p3_sy,mp.p3_sy);
				n_proj++;
			}
			else if (mp.p3_flags & PF_OVERFLOW)
				n_ovf++;
			else
				n_behind++;

			//g3_calc_point_depth is the z of the rotated point
			CHECK_EQ(g3_calc_point_depth(&v),mp.z);
		}
	}

	//the random points must reach each of the three cases
	CHECK(n_proj > 1000); CHECK(n_ovf > 0); CHECK(n_behind > 1000);

	//small z values, to reach the overflow check near its limit
	set_simple_view();
	m_set_view(&zero_vec,&ident,f1_0,&Window_scale,&scale);
	n_proj = n_ovf = 0;

	for (i=0;i<20000;i++) {
		v.z = (fix) (rnd() % 0x400) + 1;
		v.x = rnd_range(v.z); v.y = rnd_range(v.z);
		if (i & 1) v.z = (fix) (rnd() % 8) + 1;

		g3_rotate_point(&p,&v);
		g3_project_point(&p);
		m_rotate_project(&mp,&v);

		CHECK_EQ(p.p3_flags,mp.p3_flags);
		if (mp.p3_flags & PF_PROJECTED) {
			CHECK_EQ(p.p3_sx,mp.p3_sx); CHECK_EQ(p.p3_sy,mp.p3_sy);
			n_proj++;
		}
		else if (mp.p3_flags & PF_OVERFLOW)
			n_ovf++;
	}

	CHECK(n_proj > 1000); CHECK(n_ovf > 1000);

	//g3_check_codes
	set_simple_view();
	set_point(&pa,0,0,10);		//on screen
	set_point(&pb,20,0,10);		//off right
	set_point(&pc,20,20,10);	//off right, off top
	list[0] = &pa; list[1] = &pb; list[2] = &pc;

	cc = g3_check_codes(3,list);
	CHECK_EQ(cc.or,CC_OFF_RIGHT+CC_OFF_TOP);
	CHECK_EQ(cc.and,0);

	cc = g3_check_codes(2,list+1);
	CHECK_EQ(cc.or,CC_OFF_RIGHT+CC_OFF_TOP);
	CHECK_EQ(cc.and,CC_OFF_RIGHT);
}

static void test_deltas(void)
{
	vms_vector pos = {i2f(5),i2f(-3),i2f(7)},scale,d,v;
	vms_matrix mat;
	fix *m = (fix *) &mat;
	g3s_point p,q,mp;
	int i,j;

	for (i=0;i<50;i++) {
		for (j=0;j<9;j++)
			m[j] = rnd_range(f1_0);

		set_canvas(320,200,0x1999a);
		g3_set_view_matrix(&pos,&mat,f1_0);
		m_set_view(&pos,&mat,f1_0,&Window_scale,&scale);

		for (j=0;j<20;j++) {
			fix delta = rnd_range(i2f(100));

			//delta x uses m1,m2,m3: the x parts of the three vectors
			CHECK(g3_rotate_delta_x(&d,delta) == &d);
			CHECK_EQ(d.x,m_fixmul(m_mat.rvec.x,delta));
			CHECK_EQ(d.y,m_fixmul(m_mat.uvec.x,delta));
			CHECK_EQ(d.z,m_fixmul(m_mat.fvec.x,delta));

			//delta y uses m4,m5,m6
			g3_rotate_delta_y(&d,delta);
			CHECK_EQ(d.x,m_fixmul(m_mat.rvec.y,delta));
			CHECK_EQ(d.y,m_fixmul(m_mat.uvec.y,delta));
			CHECK_EQ(d.z,m_fixmul(m_mat.fvec.y,delta));

			//delta z uses m7,m8,m9
			g3_rotate_delta_z(&d,delta);
			CHECK_EQ(d.x,m_fixmul(m_mat.rvec.z,delta));
			CHECK_EQ(d.y,m_fixmul(m_mat.uvec.z,delta));
			CHECK_EQ(d.z,m_fixmul(m_mat.fvec.z,delta));

			//delta vec is a rotate with no translation
			v.x = rnd_range(i2f(100)); v.y = rnd_range(i2f(100)); v.z = rnd_range(i2f(100));
			g3_rotate_delta_vec(&d,&v);
			CHECK_EQ(d.x,m_dot3(v.x,m_mat.rvec.x,v.y,m_mat.rvec.y,v.z,m_mat.rvec.z));
			CHECK_EQ(d.y,m_dot3(v.x,m_mat.uvec.x,v.y,m_mat.uvec.y,v.z,m_mat.uvec.z));
			CHECK_EQ(d.z,m_dot3(v.x,m_mat.fvec.x,v.y,m_mat.fvec.y,v.z,m_mat.fvec.z));

			//add delta: a plain add, flags cleared, new codes
			m_rotate_project(&mp,&v);
			p = mp;
			p.p3_flags = PF_PROJECTED;
			CHECK_EQ(g3_add_delta_vec(&q,&p,&d),m_code(mp.x+d.x,mp.y+d.y,mp.z+d.z));
			CHECK_EQ(q.x,mp.x+d.x); CHECK_EQ(q.y,mp.y+d.y); CHECK_EQ(q.z,mp.z+d.z);
			CHECK_EQ(q.p3_flags,0);
		}
	}
}

static void test_point_2_vec(void)
{
	vms_vector v;

	set_simple_view();

	//the center of the canvas: the vector 0,0,1 before normalize and
	//rotate, and the matrix is the identity
	g3_point_2_vec(&v,160,100);
	CHECK_EQ(v.x,0); CHECK_EQ(v.y,0); CHECK_EQ(v.z,f1_0);

	//the right edge at the center line: x = (320-160)/160 = 1, so the
	//vector is 1,0,1 normalized. The value is from vm_vec_normalize, so
	//the check has a tolerance.
	g3_point_2_vec(&v,320,100);
	CHECK_NEAR(v.x,0xb505,2); CHECK_EQ(v.y,0); CHECK_NEAR(v.z,0xb505,2);

	//the top edge: y is inverted, so the vector points up
	g3_point_2_vec(&v,160,0);
	CHECK_EQ(v.x,0); CHECK_NEAR(v.y,0xb505,2); CHECK_NEAR(v.z,0xb505,2);
}

static void test_facing(void)
{
	vms_vector pnt = {0,0,i2f(10)};
	vms_vector n_toward = {0,0,-f1_0},n_away = {0,0,f1_0},n_side = {f1_0,0,0};
	vms_vector eye = {0,0,i2f(20)};

	set_simple_view();

	//the eye is at 0,0,0. the view vector is eye - point = 0,0,-10.
	//the dot product with 0,0,-1 is 10 > 0: facing
	CHECK_EQ(g3_check_normal_facing(&pnt,&n_toward),1);
	CHECK_EQ(g3_check_normal_facing(&pnt,&n_away),0);
	CHECK_EQ(g3_check_normal_facing(&pnt,&n_side),0);		//dot product 0 is not facing

	//the eye on the other side of the plane
	g3_set_view_matrix(&eye,&ident,f1_0);
	CHECK_EQ(g3_check_normal_facing(&pnt,&n_toward),0);
	CHECK_EQ(g3_check_normal_facing(&pnt,&n_away),1);
}

static void test_clip_polygon(void)
{
	g3s_point a,b,c,d;
	g3s_point *src[8],*dest[8],**res;
	g3s_point *list[4];
	g3s_codes cc;
	g3s_uvl uvl[3];
	grs_bitmap bm;
	int nv,i;
	rec *r;

	set_simple_view();
	CHECK_EQ(free_point_num,0);

	//Triangle A 0,0,10  B 20,0,10  C 0,5,10. B is off right (x > z).
	//Clip against the right plane, by hand from clip_edge:
	// edge A-B: num = xs-zs = -10, den = xs-xe-zs+ze = -20.
	//   x = (20-0)*-10/-20 + 0 = 10, y = 0, z = x = 10
	// edge C-B: num = -10, den = -20.
	//   x = 10, y = (0-5)*-10/-20 + 5 = 2.5, z = x = 10
	//clip_plane starts at the second point, so the order of the new list
	//is: B clipped to A, B clipped to C, C, A
	set_point(&a,0,0,10);
	set_point(&b,20,0,10);
	set_point(&c,0,5,10);

	CHECK_EQ(a.p3_codes,0); CHECK_EQ(b.p3_codes,CC_OFF_RIGHT); CHECK_EQ(c.p3_codes,0);

	src[0] = &a; src[1] = &b; src[2] = &c;
	nv = 3;
	cc.or = CC_OFF_RIGHT; cc.and = 0;

	res = clip_polygon(src,dest,&nv,&cc);

	CHECK(res == dest);
	CHECK_EQ(nv,4);
	CHECK_EQ(cc.or,0); CHECK_EQ(cc.and,0);
	CHECK_EQ(free_point_num,2);

	CHECK_EQ(res[0]->x,i2f(10)); CHECK_EQ(res[0]->y,0);      CHECK_EQ(res[0]->z,i2f(10));
	CHECK_EQ(res[1]->x,i2f(10)); CHECK_EQ(res[1]->y,0x28000); CHECK_EQ(res[1]->z,i2f(10));
	CHECK(res[2] == &c);
	CHECK(res[3] == &a);
	CHECK_EQ(res[0]->p3_flags,PF_TEMP_POINT); CHECK_EQ(res[1]->p3_flags,PF_TEMP_POINT);
	CHECK_EQ(res[0]->p3_codes,0); CHECK_EQ(res[1]->p3_codes,0);

	free_temp_point(res[1]);
	free_temp_point(res[0]);
	CHECK_EQ(free_point_num,0);

	//no plane selected: the list and the codes do not change
	nv = 3;
	cc.or = 0; cc.and = 0;
	res = clip_polygon(src,dest,&nv,&cc);
	CHECK(res == src); CHECK_EQ(nv,3); CHECK_EQ(cc.or,0); CHECK_EQ(cc.and,0);

	//The same triangle through g3_draw_poly. The 2d coords by hand:
	// 10,0,10:   sx = 160+160 = 320, sy = 100
	// 10,2.5,10: sx = 320, sy = 100 - 2.5*100/10 = 75
	// C 0,5,10:  sx = 160, sy = 100 - 50 = 50
	// A 0,0,10:  sx = 160, sy = 100
	clear_recs();
	list[0] = &a; list[1] = &b; list[2] = &c;

	CHECK_EQ(g3_draw_poly(3,list),0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_FLAT);
	CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],i2f(320)); CHECK_EQ(r->v[1],i2f(100));
	CHECK_EQ(r->v[2],i2f(320)); CHECK_EQ(r->v[3],i2f(75));
	CHECK_EQ(r->v[4],i2f(160)); CHECK_EQ(r->v[5],i2f(50));
	CHECK_EQ(r->v[6],i2f(160)); CHECK_EQ(r->v[7],i2f(100));
	CHECK_EQ(free_point_num,0);

	//The same triangle with u,v,l through g3_draw_tmap. The fraction of
	//the two edges is 1/2:
	// A-B: u = (1-0)/2 = 0.5, v = 0, l = (0-1)/2 + 1 = 0.5
	// C-B: u = (1-0)/2 = 0.5, v = (0-1)/2 + 1 = 0.5, l = (0-0.5)/2 + 0.5 = 0.25
	uvl[0].u = 0;    uvl[0].v = 0;    uvl[0].l = f1_0;
	uvl[1].u = f1_0; uvl[1].v = 0;    uvl[1].l = 0;
	uvl[2].u = 0;    uvl[2].v = f1_0; uvl[2].l = 0x8000;

	clear_recs();
	CHECK_EQ(g3_draw_tmap(3,list,uvl,&bm),0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_TMAP);
	CHECK(r->bm == &bm);
	CHECK_EQ(r->nv,4);

	CHECK_EQ(r->pts[0].x,i2f(10)); CHECK_EQ(r->pts[0].y,0); CHECK_EQ(r->pts[0].z,i2f(10));
	CHECK_EQ(r->pts[0].p3_u,0x8000); CHECK_EQ(r->pts[0].p3_v,0); CHECK_EQ(r->pts[0].p3_l,0x8000);
	CHECK_EQ(r->pts[0].p3_sx,i2f(320)); CHECK_EQ(r->pts[0].p3_sy,i2f(100));
	CHECK_EQ(r->pts[0].p3_flags,PF_TEMP_POINT+PF_UVS+PF_LVS+PF_PROJECTED);

	CHECK_EQ(r->pts[1].x,i2f(10)); CHECK_EQ(r->pts[1].y,0x28000); CHECK_EQ(r->pts[1].z,i2f(10));
	CHECK_EQ(r->pts[1].p3_u,0x8000); CHECK_EQ(r->pts[1].p3_v,0x8000); CHECK_EQ(r->pts[1].p3_l,0x4000);
	CHECK_EQ(r->pts[1].p3_sx,i2f(320)); CHECK_EQ(r->pts[1].p3_sy,i2f(75));

	CHECK(r->ptrs[2] == &c);
	CHECK_EQ(r->pts[2].p3_u,0); CHECK_EQ(r->pts[2].p3_v,f1_0); CHECK_EQ(r->pts[2].p3_l,0x8000);
	CHECK_EQ(r->pts[2].p3_flags,PF_UVS+PF_LVS+PF_PROJECTED);

	CHECK(r->ptrs[3] == &a);
	CHECK_EQ(r->pts[3].p3_u,0); CHECK_EQ(r->pts[3].p3_v,0); CHECK_EQ(r->pts[3].p3_l,f1_0);
	CHECK_EQ(free_point_num,0);

	//A square at z = 10 that crosses the right plane and the top plane:
	//A 0,0  B 20,0  C 20,20  D 0,20. By hand:
	//right plane: B is clipped to A: P1 10,0,10. C is clipped to D:
	//  P2 10,20,10 (off top). List: P1 P2 D A
	//top plane: P2 is clipped to P1: 10,10,10. D is clipped to A: 0,10,10.
	//  List: 10,10  0,10  A  P1
	//2d: 320,0  160,0  160,100  320,100
	set_point(&a,0,0,10);
	set_point(&b,20,0,10);
	set_point(&c,20,20,10);
	set_point(&d,0,20,10);

	CHECK_EQ(c.p3_codes,CC_OFF_RIGHT+CC_OFF_TOP); CHECK_EQ(d.p3_codes,CC_OFF_TOP);

	clear_recs();
	list[0] = &a; list[1] = &b; list[2] = &c; list[3] = &d;

	CHECK_EQ(g3_draw_poly(4,list),0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],i2f(320)); CHECK_EQ(r->v[1],0);
	CHECK_EQ(r->v[2],i2f(160)); CHECK_EQ(r->v[3],0);
	CHECK_EQ(r->v[4],i2f(160)); CHECK_EQ(r->v[5],i2f(100));
	CHECK_EQ(r->v[6],i2f(320)); CHECK_EQ(r->v[7],i2f(100));
	CHECK_EQ(free_point_num,0);

	//the 3d result of the same square, from the clipper
	set_point(&a,0,0,10); set_point(&b,20,0,10); set_point(&c,20,20,10); set_point(&d,0,20,10);
	src[0] = &a; src[1] = &b; src[2] = &c; src[3] = &d;
	nv = 4;
	cc.or = CC_OFF_RIGHT+CC_OFF_TOP; cc.and = 0;

	res = clip_polygon(src,dest,&nv,&cc);

	CHECK(res == src);		//two planes: the result is in the first list again
	CHECK_EQ(nv,4);
	CHECK_EQ(cc.or,0); CHECK_EQ(cc.and,0);
	CHECK_EQ(res[0]->x,i2f(10)); CHECK_EQ(res[0]->y,i2f(10)); CHECK_EQ(res[0]->z,i2f(10));
	CHECK_EQ(res[1]->x,0);       CHECK_EQ(res[1]->y,i2f(10)); CHECK_EQ(res[1]->z,i2f(10));
	CHECK(res[2] == &a);
	CHECK_EQ(res[3]->x,i2f(10)); CHECK_EQ(res[3]->y,0);       CHECK_EQ(res[3]->z,i2f(10));
	CHECK_EQ(free_point_num,3);

	for (i=nv-1;i>=0;i--)
		if (res[i]->p3_flags & PF_TEMP_POINT)
			free_temp_point(res[i]);
	CHECK_EQ(free_point_num,0);

	//left and bottom planes: the mirror of the square. By hand the result
	//is the square -10..0. The left clip stores z = -x, and the new point
	//has z = 10.
	//left: B -20,0 is clipped to A: -10,0,10. C -20,-20 is clipped to D:
	//  -10,-20,10. List: P1 P2 D A
	//bottom: P2 is clipped to P1: -10,-10,10. D 0,-20 is clipped to A:
	//  0,-10,10. List: -10,-10  0,-10  A  P1
	//2d: 0,200  160,200  160,100  0,100
	set_point(&a,0,0,10); set_point(&b,-20,0,10); set_point(&c,-20,-20,10); set_point(&d,0,-20,10);

	CHECK_EQ(c.p3_codes,CC_OFF_LEFT+CC_OFF_BOT);

	clear_recs();
	list[0] = &a; list[1] = &b; list[2] = &c; list[3] = &d;

	CHECK_EQ(g3_draw_poly(4,list),0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],0);         CHECK_EQ(r->v[1],i2f(200));
	CHECK_EQ(r->v[2],i2f(160));  CHECK_EQ(r->v[3],i2f(200));
	CHECK_EQ(r->v[4],i2f(160));  CHECK_EQ(r->v[5],i2f(100));
	CHECK_EQ(r->v[6],0);         CHECK_EQ(r->v[7],i2f(100));
	CHECK_EQ(free_point_num,0);

	//all points off the same side: return 1, no draw
	set_point(&a,20,0,10); set_point(&b,30,0,10); set_point(&c,20,5,10);
	clear_recs();
	list[0] = &a; list[1] = &b; list[2] = &c;
	CHECK_EQ(g3_draw_poly(3,list),1);
	CHECK_EQ(g3_draw_tmap(3,list,uvl,&bm),1);
	CHECK_EQ(n_recs,0);

	//all on screen: no clip, the points in the same order
	set_point(&a,0,0,10); set_point(&b,5,0,10); set_point(&c,0,5,10);
	clear_recs();
	CHECK_EQ(g3_draw_poly(3,list),0);
	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].nv,3);
	CHECK_EQ(recs[0].v[0],i2f(160)); CHECK_EQ(recs[0].v[1],i2f(100));
	CHECK_EQ(recs[0].v[2],i2f(240)); CHECK_EQ(recs[0].v[3],i2f(100));
	CHECK_EQ(recs[0].v[4],i2f(160)); CHECK_EQ(recs[0].v[5],i2f(50));

	//A triangle that is all outside near the top right corner, with no
	//common code: A off right, B off top, C off right and top. By hand:
	//the right clip gives B 0,30  10,30  10,20. All are off top, so the
	//new codes_and is not zero and the face is clipped away.
	set_point(&a,30,0,10); set_point(&b,0,30,10); set_point(&c,30,30,10);
	CHECK_EQ(a.p3_codes & b.p3_codes & c.p3_codes,0);
	clear_recs();
	CHECK_EQ(g3_draw_poly(3,list),1);
	CHECK_EQ(g3_draw_tmap(3,list,uvl,&bm),1);
	CHECK_EQ(n_recs,0);
	CHECK_EQ(free_point_num,0);

	//a triangle with a point behind the eye. Each point of the clipped
	//polygon is on screen. All the temp points are free.
	set_point(&a,0,0,10); set_point(&b,3,0,-10); set_point(&c,0,3,10);
	CHECK(b.p3_codes & CC_BEHIND);
	clear_recs();
	g3_draw_tmap(3,list,uvl,&bm);
	CHECK_EQ(free_point_num,0);
	CHECK_EQ(n_recs,1);
	if (n_recs == 1) {
		CHECK(recs[0].nv >= 3);
		for (i=0;i<recs[0].nv;i++) {
			CHECK_EQ(recs[0].pts[i].p3_codes,0);
			CHECK(recs[0].pts[i].p3_flags & PF_PROJECTED);
			CHECK(recs[0].pts[i].p3_sx >= 0 && recs[0].pts[i].p3_sx <= i2f(320));
			CHECK(recs[0].pts[i].p3_sy >= 0 && recs[0].pts[i].p3_sy <= i2f(200));
		}
	}

	//random polygons: the temp points are all free after each draw, and
	//each drawn point is inside the view pyramid
	for (i=0;i<3000;i++) {
		g3s_point pts[5];
		g3s_point *pl[5];
		g3s_uvl uv[5];
		int n = 3 + (int) (rnd() % 3),j;

		for (j=0;j<n;j++) {
			vms_vector v;

			v.x = rnd_range(i2f(30)); v.y = rnd_range(i2f(30)); v.z = rnd_range(i2f(30));
			g3_rotate_point(&pts[j],&v);
			pl[j] = &pts[j];
			uv[j].u = rnd_range(f1_0); uv[j].v = rnd_range(f1_0); uv[j].l = rnd_range(f1_0);
		}

		clear_recs();

		if (i & 1)
			g3_draw_tmap(n,pl,uv,&bm);
		else
			g3_draw_poly(n,pl);

		CHECK_EQ(free_point_num,0);
		CHECK(n_recs <= 1);

		if (n_recs == 1 && recs[0].type == REC_TMAP)
			for (j=0;j<recs[0].nv && j<MAX_REC_PTS;j++) {
				CHECK(!(recs[0].pts[j].p3_codes & CC_BEHIND));
				CHECK(recs[0].pts[j].p3_flags & PF_PROJECTED);
			}
	}
}

static void test_line(void)
{
	g3s_point a,b;
	rec *r;

	set_simple_view();

	//both on screen: 0,0,10 -> 160,100 and 5,5,10 -> 240,50. returns 1
	set_point(&a,0,0,10);
	set_point(&b,5,5,10);
	clear_recs();
	line_ret = 0;		//the return value of the 2d drawer is not used here

	CHECK_EQ(g3_draw_line(&a,&b),1);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_LINE);
	CHECK_EQ(r->v[0],i2f(160)); CHECK_EQ(r->v[1],i2f(100));
	CHECK_EQ(r->v[2],i2f(240)); CHECK_EQ(r->v[3],i2f(50));

	//B off right, in front: no 3d clip, the 2d drawer clips. The return
	//value is from the 2d drawer. B 20,0,10 -> sx = 480
	set_point(&b,20,0,10);
	clear_recs();
	line_ret = 5;

	CHECK_EQ(g3_draw_line(&a,&b),5);

	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].v[0],i2f(160)); CHECK_EQ(recs[0].v[1],i2f(100));
	CHECK_EQ(recs[0].v[2],i2f(480)); CHECK_EQ(recs[0].v[3],i2f(100));
	CHECK_EQ(free_point_num,0);

	//both off the same side: not drawn
	set_point(&a,30,0,10);
	clear_recs();
	CHECK_EQ(g3_draw_line(&a,&b),0);
	CHECK_EQ(n_recs,0);

	//B behind the eye: A 0,0,10  B 0,0,-10 has all the codes.
	//left plane, by hand: num = -xs-zs = -10, den = -10-0-10 = -20,
	//new point 0,0,0 (z = -x = 0). Its code is CC_BEHIND only. The loop
	//goes on with codes_or = CC_BEHIND, no more plane. The line still
	//needs a clip and no side plane is left: not drawn (the assembly did
	//not come back from this case).
	set_point(&a,0,0,10);
	set_point(&b,0,0,-10);
	clear_recs();
	CHECK_EQ(g3_draw_line(&a,&b),0);
	CHECK_EQ(n_recs,0);
	CHECK_EQ(free_point_num,0);

	//B behind and to the side: A 0,0,10  B 20,0,-10.
	//B codes: right, top, behind, bottom (not left: 20 < 10 is false).
	//right plane: num = 0-10 = -10, den = -10-20-10 = -40,
	//x = 20*-10/-40 = 5, y = 0, z = x = 5. Code 0. Drawn all on screen:
	//A 160,100 to 5/5*160+160 = 320,100. returns 1.
	set_point(&b,20,0,-10);
	CHECK_EQ(b.p3_codes,CC_BEHIND+CC_OFF_RIGHT+CC_OFF_TOP+CC_OFF_BOT);
	clear_recs();
	line_ret = 0;

	CHECK_EQ(g3_draw_line(&a,&b),1);

	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].v[0],i2f(160)); CHECK_EQ(recs[0].v[1],i2f(100));
	CHECK_EQ(recs[0].v[2],i2f(320)); CHECK_EQ(recs[0].v[3],i2f(100));
	CHECK_EQ(free_point_num,0);

	//the same line with the points in the other order. The clipper puts
	//the on-screen point first, so the 2d line is the same
	clear_recs();
	CHECK_EQ(g3_draw_line(&b,&a),1);
	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].v[0],i2f(160)); CHECK_EQ(recs[0].v[1],i2f(100));
	CHECK_EQ(recs[0].v[2],i2f(320)); CHECK_EQ(recs[0].v[3],i2f(100));
	CHECK_EQ(free_point_num,0);

	line_ret = 1;
}

static void test_special_render(void)
{
	g3s_point a,b,c;
	g3s_point *list[3];
	g3s_uvl uvl[3];
	grs_bitmap bm;

	set_simple_view();
	memset(uvl,0,sizeof(uvl));

	set_point(&a,0,0,10); set_point(&b,5,0,10); set_point(&c,0,5,10);
	list[0] = &a; list[1] = &b; list[2] = &c;

	g3_set_special_render(special_tmap,special_flat,special_line);
	n_special_tmap = n_special_flat = n_special_line = 0;
	clear_recs();

	CHECK_EQ(g3_draw_poly(3,list),0);
	CHECK_EQ(g3_draw_tmap(3,list,uvl,&bm),0);
	CHECK_EQ(g3_draw_line(&a,&b),1);

	CHECK_EQ(n_special_flat,1); CHECK_EQ(n_special_tmap,1); CHECK_EQ(n_special_line,1);
	CHECK_EQ(n_recs,0);

	//NULL restores each default
	g3_set_special_render(NULL,special_flat,NULL);
	g3_draw_poly(3,list);
	g3_draw_tmap(3,list,uvl,&bm);
	g3_draw_line(&a,&b);

	CHECK_EQ(n_special_flat,2); CHECK_EQ(n_special_tmap,1); CHECK_EQ(n_special_line,1);
	CHECK_EQ(n_recs,2);

	g3_set_special_render(NULL,NULL,NULL);
}

static void test_check_and_draw(void)
{
	g3s_point a,b,c;
	g3s_point *list[3];
	g3s_uvl uvl[3];
	grs_bitmap bm;
	vms_vector pnt = {0,0,i2f(10)};
	vms_vector n_toward = {0,0,-f1_0},n_away = {0,0,f1_0};

	set_simple_view();
	memset(uvl,0,sizeof(uvl));

	set_point(&a,-2,-2,10); set_point(&b,2,-2,10); set_point(&c,2,2,10);

	//with a normal
	list[0] = &a; list[1] = &b; list[2] = &c;
	clear_recs();
	CHECK_EQ(g3_check_and_draw_poly(3,list,&n_toward,&pnt),0);
	CHECK_EQ(g3_check_and_draw_poly(3,list,&n_away,&pnt),(bool) -1);
	CHECK_EQ(g3_check_and_draw_tmap(3,list,uvl,&bm,&n_toward,&pnt),0);
	CHECK_EQ(g3_check_and_draw_tmap(3,list,uvl,&bm,&n_away,&pnt),(bool) -1);
	CHECK_EQ(n_recs,2);

	//with no normal: the normal is (p1-p0) x (p2-p1), and the face is
	//facing when the dot product with p1 is negative.
	//a,b,c: (4,0,0) x (0,4,0) = 0,0,16. dot with b is positive: not facing
	//a,c,b: (4,4,0) x (0,-4,0) = 0,0,-16: facing
	clear_recs();
	CHECK_EQ(g3_check_and_draw_poly(3,list,NULL,NULL),(bool) -1);
	CHECK_EQ(n_recs,0);

	list[1] = &c; list[2] = &b;
	CHECK_EQ(g3_check_and_draw_poly(3,list,NULL,NULL),0);
	CHECK_EQ(g3_check_and_draw_tmap(3,list,uvl,&bm,NULL,NULL),0);
	CHECK_EQ(n_recs,2);
}

static void test_instance(void)
{
	vms_vector pos = {i2f(1),i2f(2),i2f(3)},opos = {i2f(10),i2f(-20),i2f(30)};
	vms_matrix mat,omat,save_mat,t,expect_mat;
	vms_vector save_pos,expect_pos,tv;
	vms_angvec ang = {0x1000,0x2000,0x3000};
	fix *m = (fix *) &mat,*om = (fix *) &omat;
	int i,j;

	for (i=0;i<20;i++) {
		for (j=0;j<9;j++) {
			m[j] = rnd_range(f1_0);
			om[j] = rnd_range(f1_0);
		}

		set_canvas(320,200,0x1999a);
		g3_set_view_matrix(&pos,&mat,f1_0);

		save_pos = View_position;
		save_mat = View_matrix;

		//no matrix: only the position changes
		g3_start_instance_matrix(&opos,NULL);

		CHECK_EQ(View_position.x,save_pos.x - opos.x);
		CHECK_EQ(View_position.y,save_pos.y - opos.y);
		CHECK_EQ(View_position.z,save_pos.z - opos.z);
		CHECK(memcmp(&View_matrix,&save_mat,sizeof(save_mat)) == 0);

		//a second level, with a matrix:
		//pos = rotate(pos - opos,omat), matrix = transpose(omat) * matrix
		tv = View_position;
		vm_vec_sub2(&tv,&opos);
		vm_vec_rotate(&expect_pos,&tv,&omat);
		vm_copy_transpose_matrix(&t,&omat);
		vm_matrix_x_matrix(&expect_mat,&t,&View_matrix);

		g3_start_instance_matrix(&opos,&omat);

		CHECK(memcmp(&View_position,&expect_pos,sizeof(expect_pos)) == 0);
		CHECK(memcmp(&View_matrix,&expect_mat,sizeof(expect_mat)) == 0);

		//a third level, with angles
		tv = View_position;
		vm_vec_sub2(&tv,&opos);
		vm_angles_2_matrix(&omat,&ang);
		vm_vec_rotate(&expect_pos,&tv,&omat);
		vm_copy_transpose_matrix(&t,&omat);
		vm_matrix_x_matrix(&expect_mat,&t,&View_matrix);

		g3_start_instance_angles(&opos,&ang);

		CHECK(memcmp(&View_position,&expect_pos,sizeof(expect_pos)) == 0);
		CHECK(memcmp(&View_matrix,&expect_mat,sizeof(expect_mat)) == 0);

		//NULL angles are the same as a NULL matrix
		expect_mat = View_matrix;
		g3_start_instance_angles(&opos,NULL);
		CHECK(memcmp(&View_matrix,&expect_mat,sizeof(expect_mat)) == 0);
		g3_done_instance();

		g3_done_instance();
		g3_done_instance();
		g3_done_instance();

		CHECK(memcmp(&View_position,&save_pos,sizeof(save_pos)) == 0);
		CHECK(memcmp(&View_matrix,&save_mat,sizeof(save_mat)) == 0);
	}
}

static void test_sphere_bitmap_rod(void)
{
	g3s_point p,bot,top;
	vms_vector pos = {0,0,i2f(10)},behind = {0,0,i2f(-10)};
	grs_bitmap bm;
	rec *r;
	int i;

	set_simple_view();

	//sphere at 0,0,10 with radius 1: the 2d radius is
	//1 * scale.x (1) * 160 / 10 = 16. The center is 160,100.
	set_point(&p,0,0,10);
	clear_recs();
	g3_draw_sphere(&p,f1_0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_DISK);
	CHECK_EQ(r->v[0],i2f(160)); CHECK_EQ(r->v[1],i2f(100)); CHECK_EQ(r->v[2],i2f(16));
	CHECK(p.p3_flags & PF_PROJECTED);

	//behind: no draw
	set_point(&p,0,0,-10);
	clear_recs();
	g3_draw_sphere(&p,f1_0);
	CHECK_EQ(n_recs,0);

	//radius divide overflow: no draw. z = 1 (raw), radius 1000
	memset(&p,0,sizeof(p));
	p.z = 1;
	g3_code_point(&p);
	clear_recs();
	g3_draw_sphere(&p,i2f(1000));
	CHECK_EQ(n_recs,0);

	//bitmap at 0,0,10, width 1, height 2:
	//w = 1 * 160 / 10 = 16, h = 2 * 100 / 10 = 20, center 160,100
	//corners: 144,80  176,80  176,120
	clear_recs();
	g3_draw_bitmap(&pos,f1_0,f2_0,&bm);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_SCALE);
	CHECK(r->bm == &bm);
	CHECK_EQ(r->v[0],i2f(144)); CHECK_EQ(r->v[1],i2f(80));
	CHECK_EQ(r->v[2],i2f(176)); CHECK_EQ(r->v[3],i2f(80));
	CHECK_EQ(r->v[4],i2f(176)); CHECK_EQ(r->v[5],i2f(120));
	CHECK_EQ(blob_vertices[0],i2f(144)); CHECK_EQ(blob_vertices[5],i2f(120));

	//with zoom 2 the scale is 0.5 for x and y. The rotated x and y are
	//scaled, and the 2d size is scaled too: w = 8, h = 10
	g3_set_view_matrix(&zero_vec,&ident,f2_0);
	clear_recs();
	g3_draw_bitmap(&pos,f1_0,f2_0,&bm);
	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].v[0],i2f(152)); CHECK_EQ(recs[0].v[1],i2f(90));
	CHECK_EQ(recs[0].v[4],i2f(168)); CHECK_EQ(recs[0].v[5],i2f(110));

	//behind: no draw
	clear_recs();
	g3_draw_bitmap(&behind,f1_0,f2_0,&bm);
	CHECK_EQ(n_recs,0);

	//A rod from 0,-1,10 to 0,1,10 with width 1 at the two ends. The edge
	//vector is the cross product of bot-top (0,-1,0 normalized) and the
	//top point (normalized): its direction is -1,0,0. So the corners are
	//-1,1  1,1  1,-1  -1,-1 at z = 10. The values go through
	//vm_vec_normalize three times, so the check has a small tolerance.
	set_simple_view();
	set_point(&bot,0,-1,10);
	set_point(&top,0,1,10);
	clear_recs();

	g3_draw_rod_tmap(&bm,&bot,f1_0,&top,f1_0,0x6000);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_TMAP);
	CHECK(r->bm == &bm);
	CHECK_EQ(r->nv,4);
	{
		static const int ex[4] = {-1,1,1,-1},ey[4] = {1,1,-1,-1};
		static const fix eu[4] = {0x200,0xfe00,0xfe00,0x200},ev[4] = {0x200,0x200,0xfe00,0xfe00};

		for (i=0;i<4;i++) {
			CHECK_NEAR(r->pts[i].x,i2f(ex[i]),8);
			CHECK_NEAR(r->pts[i].y,i2f(ey[i]),8);
			CHECK_EQ(r->pts[i].z,i2f(10));
			CHECK_EQ(r->pts[i].p3_u,eu[i]);
			CHECK_EQ(r->pts[i].p3_v,ev[i]);
			CHECK_EQ(r->pts[i].p3_l,0x6000);
			CHECK_NEAR(r->pts[i].p3_sx,i2f(160 + 16*ex[i]),256);
			CHECK_NEAR(r->pts[i].p3_sy,i2f(100 - 10*ey[i]),256);
		}
	}

	//the flat rod has the same corners
	clear_recs();
	g3_draw_rod_flat(&bot,f1_0,&top,f2_0);

	CHECK_EQ(n_recs,1);
	r = &recs[0];
	CHECK_EQ(r->type,REC_FLAT);
	CHECK_EQ(r->nv,4);
	CHECK_NEAR(r->v[0],i2f(160-32),512); CHECK_NEAR(r->v[1],i2f(90),256);		//top, width 2
	CHECK_NEAR(r->v[2],i2f(160+32),512); CHECK_NEAR(r->v[3],i2f(90),256);
	CHECK_NEAR(r->v[4],i2f(160+16),256); CHECK_NEAR(r->v[5],i2f(110),256);	//bot, width 1
	CHECK_NEAR(r->v[6],i2f(160-16),256); CHECK_NEAR(r->v[7],i2f(110),256);

	//a rod that is all off screen is not drawn
	set_point(&bot,100,-1,10);
	set_point(&top,100,1,10);
	clear_recs();
	g3_draw_rod_tmap(&bm,&bot,f1_0,&top,f1_0,0x6000);
	g3_draw_rod_flat(&bot,f1_0,&top,f1_0);
	CHECK_EQ(n_recs,0);
	CHECK_EQ(free_point_num,0);
}

static void test_horizon(void)
{
	fix pts[10];
	vms_vector vecs[5];
	vms_matrix mat;
	rec *r;
	int n,i;

	set_simple_view();

	//The eye is level. By hand: horizon_vec = 0,1,0. The corner values are
	//up_right = 1, down_right = -1, down_left = -1, up_left = 1.
	//left edge: y = |200 * 1 / 2| = 100. right edge: y = |200 * 1 / 2| = 100.
	//The line is 0,100 (edge 0) to 320,100 (edge 2).
	//top polygon: 320,100  0,100  and the corners 0 and 1: 0,0  320,0
	//bottom polygon: 0,100  320,100  and the corners 2 and 3: 320,200  0,200
	clear_recs();
	g3_draw_horizon(11,22);

	CHECK_EQ(n_recs,4);
	CHECK_EQ(recs[0].type,REC_SETCOLOR); CHECK_EQ(recs[0].color,11);
	r = &recs[1];
	CHECK_EQ(r->type,REC_FLAT); CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],i2f(320)); CHECK_EQ(r->v[1],i2f(100));
	CHECK_EQ(r->v[2],0);        CHECK_EQ(r->v[3],i2f(100));
	CHECK_EQ(r->v[4],0);        CHECK_EQ(r->v[5],0);
	CHECK_EQ(r->v[6],i2f(320)); CHECK_EQ(r->v[7],0);
	CHECK_EQ(recs[2].type,REC_SETCOLOR); CHECK_EQ(recs[2].color,22);
	r = &recs[3];
	CHECK_EQ(r->type,REC_FLAT); CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],0);        CHECK_EQ(r->v[1],i2f(100));
	CHECK_EQ(r->v[2],i2f(320)); CHECK_EQ(r->v[3],i2f(100));
	CHECK_EQ(r->v[4],i2f(320)); CHECK_EQ(r->v[5],i2f(200));
	CHECK_EQ(r->v[6],0);        CHECK_EQ(r->v[7],i2f(200));

	//The sky polygon is the top polygon. The vectors:
	//right end: x/z = 1, so 1,0,1 normalized. left end: -1,0,1 normalized.
	//corner 0 (up left): -1,1,1 normalized, corner 1: 1,1,1 normalized.
	//0xb505 = 1/sqrt(2), 0x93cd = 1/sqrt(3). The values come from
	//quad_sqrt and vm_vec_normalize, so the checks have a tolerance.
	memset(pts,0,sizeof(pts));
	n = g3_compute_sky_polygon(pts,vecs);

	CHECK_EQ(n,4);
	CHECK_EQ(pts[0],i2f(320)); CHECK_EQ(pts[1],i2f(100));
	CHECK_EQ(pts[2],0);        CHECK_EQ(pts[3],i2f(100));
	CHECK_EQ(pts[4],0);        CHECK_EQ(pts[5],0);
	CHECK_EQ(pts[6],i2f(320)); CHECK_EQ(pts[7],0);

	CHECK_NEAR(vecs[0].x,0xb505,3);  CHECK_EQ(vecs[0].y,0); CHECK_NEAR(vecs[0].z,0xb505,3);
	CHECK_NEAR(vecs[1].x,-0xb505,3); CHECK_EQ(vecs[1].y,0); CHECK_NEAR(vecs[1].z,0xb505,3);
	CHECK_NEAR(vecs[2].x,-0x93cd,3); CHECK_NEAR(vecs[2].y,0x93cd,3); CHECK_NEAR(vecs[2].z,0x93cd,3);
	CHECK_NEAR(vecs[3].x,0x93cd,3);  CHECK_NEAR(vecs[3].y,0x93cd,3); CHECK_NEAR(vecs[3].z,0x93cd,3);

	//a color of -1 is not drawn
	clear_recs();
	g3_draw_horizon(-1,22);
	CHECK_EQ(n_recs,2);
	CHECK_EQ(recs[0].color,22);
	CHECK_EQ(recs[1].v[5],i2f(200));

	//The eye is upside down (bank 180 degrees: right and up are negated).
	//The up vector y is negative, so the colors change places: the top
	//polygon has the ground color. The sky polygon is the bottom polygon.
	memset(&mat,0,sizeof(mat));
	mat.rvec.x = -f1_0; mat.uvec.y = -f1_0; mat.fvec.z = f1_0;
	g3_set_view_matrix(&zero_vec,&mat,f1_0);

	clear_recs();
	g3_draw_horizon(11,22);

	CHECK_EQ(n_recs,4);
	CHECK_EQ(recs[0].color,22);
	CHECK_EQ(recs[1].v[5],0);				//top polygon
	CHECK_EQ(recs[2].color,11);
	CHECK_EQ(recs[3].v[5],i2f(200));		//bottom polygon

	n = g3_compute_sky_polygon(pts,vecs);
	CHECK_EQ(n,4);
	CHECK_EQ(pts[0],0);        CHECK_EQ(pts[1],i2f(100));
	CHECK_EQ(pts[2],i2f(320)); CHECK_EQ(pts[3],i2f(100));
	CHECK_EQ(pts[4],i2f(320)); CHECK_EQ(pts[5],i2f(200));
	CHECK_EQ(pts[6],0);        CHECK_EQ(pts[7],i2f(200));
	for (i=0;i<4;i++)
		CHECK(vecs[i].y >= 0);		//each vector points to the sky or the horizon

	//A tilted horizon, by hand. The up vector of the eye is 0.6,0.8,0 and
	//the right vector is 0.8,-0.6,0 (raw values 0x999a, 0xcccd).
	//horizon_vec = rvec.y, uvec.y, 0 = -0x999a, 0xcccd, 0.
	//up_right = 0x3333, down_right = 0x3333 - 0x1999a = -0x16667,
	//down_left = -0x16667 + 0x13334 = -0x3333, up_left = 0x16667.
	//left edge: y = |0xc80000 * 0x16667 / 0x1999a| = 0xaf001f.
	//right edge: y = |0xc80000 * 0x3333 / 0x1999a| = 0x18ffe0.
	//Line 0,0xaf001f (edge 0) to 320,0x18ffe0 (edge 2).
	memset(&mat,0,sizeof(mat));
	mat.rvec.x = 0xcccd; mat.rvec.y = -0x999a;
	mat.uvec.x = 0x999a; mat.uvec.y = 0xcccd;
	mat.fvec.z = f1_0;
	g3_set_view_matrix(&zero_vec,&mat,f1_0);

	clear_recs();
	g3_draw_horizon(11,22);

	CHECK_EQ(n_recs,4);
	r = &recs[1];
	CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],i2f(320)); CHECK_EQ(r->v[1],(fix) (((int64_t) 0xc80000 * 0x3333) / 0x1999a));
	CHECK_EQ(r->v[2],0);        CHECK_EQ(r->v[3],(fix) (((int64_t) 0xc80000 * 0x16667) / 0x1999a));
	CHECK_EQ(r->v[1],0x18ffe0);
	CHECK_EQ(r->v[3],0xaf001f);
	CHECK_EQ(r->v[4],0);        CHECK_EQ(r->v[5],0);
	CHECK_EQ(r->v[6],i2f(320)); CHECK_EQ(r->v[7],0);

	//The eye looks up: the forward vector is 0,1,0. All four corner
	//values are 1: all sky. The canvas is cleared with the sky color, and
	//the sky polygon is the four corners.
	memset(&mat,0,sizeof(mat));
	mat.rvec.x = f1_0; mat.uvec.z = -f1_0; mat.fvec.y = f1_0;
	g3_set_view_matrix(&zero_vec,&mat,f1_0);

	clear_recs();
	g3_draw_horizon(11,22);
	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].type,REC_CLEAR); CHECK_EQ(recs[0].color,11);

	n = g3_compute_sky_polygon(pts,vecs);
	CHECK_EQ(n,4);
	CHECK_EQ(pts[0],0);        CHECK_EQ(pts[1],0);
	CHECK_EQ(pts[2],i2f(320)); CHECK_EQ(pts[3],0);
	CHECK_EQ(pts[4],i2f(320)); CHECK_EQ(pts[5],i2f(200));
	CHECK_EQ(pts[6],0);        CHECK_EQ(pts[7],i2f(200));
	for (i=0;i<4;i++)
		CHECK(vecs[i].y > 0);		//each corner vector points up

	//The eye looks down: all ground, no sky polygon
	mat.uvec.z = f1_0; mat.fvec.y = -f1_0;
	g3_set_view_matrix(&zero_vec,&mat,f1_0);

	clear_recs();
	g3_draw_horizon(11,22);
	CHECK_EQ(n_recs,1);
	CHECK_EQ(recs[0].type,REC_CLEAR); CHECK_EQ(recs[0].color,22);
	CHECK_EQ(g3_compute_sky_polygon(pts,vecs),0);
}

//------------------------------------------------------------------------
//The polygon model interpreter

//the model is made at an odd address, so no value in it is aligned
static ubyte model_buf[1024+1];
static ubyte *model = model_buf+1;
static int model_len;

static void put16(int ofs,int v) { model[ofs] = (ubyte) v; model[ofs+1] = (ubyte) (v >> 8); }
static void put32(int ofs,fix v) { put16(ofs,v & 0xffff); put16(ofs+2,((uint32_t) v >> 16) & 0xffff); }
static void putvec(int ofs,int x,int y,int z) { put32(ofs,i2f(x)); put32(ofs+4,i2f(y)); put32(ofs+8,i2f(z)); }
static int get16(int ofs) { return model[ofs] | (model[ofs+1] << 8); }
static fix get32(int ofs) { return (fix) ((uint32_t) get16(ofs) | ((uint32_t) get16(ofs+2) << 16)); }

//offsets of the parts of the test model
static int ofs_sort,ofs_front,ofs_back,ofs_sub,ofs_tmap,ofs_flat,ofs_subflat;

//makes a flat polygon. returns the offset past it
static int make_flatpoly(int o,int nz,int color,int nv,const int *nums)
{
	int i;

	put16(o,2); put16(o+2,nv);
	putvec(o+4,0,0,10);				//a point on the face
	putvec(o+16,0,0,nz);				//the normal
	put16(o+28,color);
	for (i=0;i<nv;i++)
		put16(o+30+i*2,nums[i]);
	if (!(nv & 1))
		put16(o+30+nv*2,0x7777);	//pad

	return o + 30 + ((nv & ~1) + 1)*2;
}

/*
 * The model:
 *
 *   DEFPOINTS 4:  0: -2,-2,10   1: 2,-2,10   2: 2,2,10   3: -2,2,10
 *   SORTNORM  normal 0,0,-1  point 0,0,10  front -> F  back -> B
 *   SUBCALL   object 1  position 1,0,0  -> S
 *   EOF
 *   F: FLATPOLY 4 points 0,3,2,1  color 0x1234  normal 0,0,-1
 *      EOF
 *   B: GLOW 1
 *      TMAPPOLY 3 points 0,2,1  bitmap 2  normal 0,0,-1
 *      EOF
 *   S: DEFP_START 1 point from point number 4: 0,0,10
 *      FLATPOLY 3 points 0,1,4  color 0x4321  normal 0,0,-1
 *      EOF
 */
static void make_model(void)
{
	static const int quad[4] = {0,3,2,1},tri[3] = {0,1,4};
	int o = 0,i;

	memset(model_buf,0xee,sizeof(model_buf));

	//DEFPOINTS
	put16(o,1); put16(o+2,4);
	putvec(o+4,-2,-2,10); putvec(o+16,2,-2,10); putvec(o+28,2,2,10); putvec(o+40,-2,2,10);
	o += 4 + 4*12;

	//SORTNORM
	ofs_sort = o;
	put16(o,4); put16(o+2,0);
	putvec(o+4,0,0,-1);				//normal
	putvec(o+16,0,0,10);				//point
	o += 32;

	//SUBCALL
	ofs_sub = o;
	put16(o,6); put16(o+2,1);		//object number
	putvec(o+4,1,0,0);				//position
	put16(o+18,0);
	o += 20;

	put16(o,0); o += 2;				//EOF

	//F
	ofs_front = o;
	put16(ofs_sort+28,ofs_front - ofs_sort);
	ofs_flat = o;
	o = make_flatpoly(o,-1,0x1234,4,quad);
	put16(o,0); o += 2;

	//B
	ofs_back = o;
	put16(ofs_sort+30,ofs_back - ofs_sort);
	put16(o,8); put16(o+2,1); o += 4;		//GLOW
	ofs_tmap = o;
	put16(o,3); put16(o+2,3);
	putvec(o+4,0,0,10);				//a point on the face
	putvec(o+16,0,0,-1);				//normal
	put16(o+28,2);						//bitmap number
	put16(o+30,0); put16(o+32,2); put16(o+34,1);		//3 is odd: no pad
	for (i=0;i<3;i++) {
		put32(o+36+i*12,0x1000*(i+1));		//u
		put32(o+36+i*12+4,0x2000*(i+1));		//v
		put32(o+36+i*12+8,0x55555555);		//l, the interpreter stores it
	}
	o += 36 + 3*12;
	put16(o,0); o += 2;

	//S
	put16(ofs_sub+16,o - ofs_sub);
	put16(o,7); put16(o+2,1); put16(o+4,4); put16(o+6,0);
	putvec(o+8,0,0,10);
	o += 8 + 12;
	ofs_subflat = o;
	o = make_flatpoly(o,-1,0x4321,3,tri);
	put16(o,0); o += 2;

	model_len = o;
}

static void test_interp(void)
{
	g3s_point points[8];
	grs_bitmap bm0,bm1,bm2;
	grs_bitmap *bitmaps[3];
	fix glow[2] = {0x1111,0x2345};
	vms_angvec angles[2] = {{0x1000,0x2000,0x3000},{0,0,0}};
	vms_vector morph[4];
	ubyte save[sizeof(model_buf)];
	rec *r;
	int i;

	bitmaps[0] = &bm0; bitmaps[1] = &bm1; bitmaps[2] = &bm2;

	make_model();

	//g3_init_polygon_model: the colors of the two flat polygons are
	//translated, and the highest bitmap number is found
	g3_init_polygon_model(model);

	CHECK_EQ(highest_texture_num,2);
	CHECK_EQ(get16(ofs_flat+28),(0x1234 ^ 0x5a) & 0xff);
	CHECK_EQ(get16(ofs_subflat+28),(0x4321 ^ 0x5a) & 0xff);
	CHECK_EQ(model_buf[0],0xee);						//no write in front of the model
	CHECK_EQ(model[model_len],0xee);					//no write past the model
	CHECK_EQ(get16(ofs_flat+30+8),0x7777);			//the pad word is not changed

	//the rest of the model data is not changed by the init
	memcpy(save,model_buf,sizeof(save));
	make_model();
	put16(ofs_flat+28,(0x1234 ^ 0x5a) & 0xff);
	put16(ofs_subflat+28,(0x4321 ^ 0x5a) & 0xff);
	CHECK(memcmp(save,model_buf,sizeof(save)) == 0);

	//a model with no texture
	{
		static ubyte m2[] = {0,0};
		highest_texture_num = 5;
		g3_init_polygon_model(m2);
		CHECK_EQ(highest_texture_num,-1);
	}

	//Draw. The eye is at 0,0,0. The sortnorm plane faces the eye (the
	//view vector 0,0,-10 dot the normal 0,0,-1 is positive), so the back
	//list is drawn first, then the front list.
	//
	//Expected calls, by hand:
	// 1. tmap: bitmap 2, points 0,2,1. The glow opcode is in front of it:
	//    with glow values the light is glow[1]. With no glow values the
	//    light is (-(fvec . normal) * 3/4 + 1/4) * model light
	//    = (1 * 3/4 + 1/4) * 0.5 = 0x8000
	// 2. setcolor(translated 0x1234), flat: points 0,3,2,1:
	//    128,120  128,80  192,80  192,120
	//    (x: +-2 * 160 / 10 = +-32 from 160. y: +-2 * 100 / 10 = +-20 from 100)
	// 3. the subobject at 1,0,0 with zero angles: the eye is at -1,0,0 in
	//    its frame. Point 4 = 0,0,10 is rotated to 1,0,10: sx = 176, sy = 100.
	//    setcolor(translated 0x4321), flat: points 0,1,4: 128,120  192,120  176,100
	set_simple_view();
	memset(points,0,sizeof(points));
	g3_set_interp_points(points);
	g3d_interp_outline = 0;

	clear_recs();
	CHECK_EQ(g3_draw_polygon_model(model,bitmaps,angles,0x8000,glow),1);

	CHECK_EQ(n_recs,5);

	r = &recs[0];
	CHECK_EQ(r->type,REC_TMAP);
	CHECK(r->bm == &bm2);
	CHECK_EQ(r->nv,3);
	CHECK(r->ptrs[0] == &points[0]); CHECK(r->ptrs[1] == &points[2]); CHECK(r->ptrs[2] == &points[1]);
	for (i=0;i<3;i++) {
		CHECK_EQ(r->pts[i].p3_u,0x1000*(i+1));
		CHECK_EQ(r->pts[i].p3_v,0x2000*(i+1));
		CHECK_EQ(r->pts[i].p3_l,0x2345);
		CHECK_EQ(get32(ofs_tmap+36+i*12+8),0x2345);		//the light is stored in the model too
		CHECK_EQ(get32(ofs_tmap+36+i*12),0x1000*(i+1));	//u and v are not changed
		CHECK_EQ(get32(ofs_tmap+36+i*12+4),0x2000*(i+1));
	}
	CHECK_EQ(r->pts[0].p3_sx,i2f(128)); CHECK_EQ(r->pts[0].p3_sy,i2f(120));
	CHECK_EQ(r->pts[1].p3_sx,i2f(192)); CHECK_EQ(r->pts[1].p3_sy,i2f(80));
	CHECK_EQ(r->pts[2].p3_sx,i2f(192)); CHECK_EQ(r->pts[2].p3_sy,i2f(120));

	CHECK_EQ(recs[1].type,REC_SETCOLOR); CHECK_EQ(recs[1].color,(0x1234 ^ 0x5a) & 0xff);

	r = &recs[2];
	CHECK_EQ(r->type,REC_FLAT);
	CHECK_EQ(r->nv,4);
	CHECK_EQ(r->v[0],i2f(128)); CHECK_EQ(r->v[1],i2f(120));
	CHECK_EQ(r->v[2],i2f(128)); CHECK_EQ(r->v[3],i2f(80));
	CHECK_EQ(r->v[4],i2f(192)); CHECK_EQ(r->v[5],i2f(80));
	CHECK_EQ(r->v[6],i2f(192)); CHECK_EQ(r->v[7],i2f(120));

	CHECK_EQ(recs[3].type,REC_SETCOLOR); CHECK_EQ(recs[3].color,(0x4321 ^ 0x5a) & 0xff);

	r = &recs[4];
	CHECK_EQ(r->type,REC_FLAT);
	CHECK_EQ(r->nv,3);
	CHECK_EQ(r->v[0],i2f(128)); CHECK_EQ(r->v[1],i2f(120));
	CHECK_EQ(r->v[2],i2f(192)); CHECK_EQ(r->v[3],i2f(120));
	CHECK_EQ(r->v[4],i2f(176)); CHECK_EQ(r->v[5],i2f(100));

	//the rotated points
	CHECK_EQ(points[0].x,i2f(-2)); CHECK_EQ(points[0].y,i2f(-2)); CHECK_EQ(points[0].z,i2f(10));
	CHECK_EQ(points[3].x,i2f(-2)); CHECK_EQ(points[3].y,i2f(2));  CHECK_EQ(points[3].z,i2f(10));
	CHECK_EQ(points[4].x,i2f(1));  CHECK_EQ(points[4].y,0);       CHECK_EQ(points[4].z,i2f(10));

	//the instance is done: the view is the same as before
	CHECK(memcmp(&View_position,&zero_vec,sizeof(zero_vec)) == 0);
	CHECK(memcmp(&View_matrix,&ident,sizeof(ident)) == 0);
	CHECK_EQ(free_point_num,0);
	CHECK_EQ(model_buf[0],0xee);
	CHECK_EQ(model[model_len],0xee);

	//No glow values and no angles: the light is from the normal (see
	//above), 0x8000. The same five calls.
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);

	CHECK_EQ(n_recs,5);
	CHECK_EQ(recs[0].type,REC_TMAP);
	for (i=0;i<3;i++) {
		CHECK_EQ(recs[0].pts[i].p3_l,0x8000);
		CHECK_EQ(get32(ofs_tmap+36+i*12+8),0x8000);
	}
	CHECK_EQ(recs[4].v[4],i2f(176));

	//model light 1.0 gives the light 1.0
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,f1_0,NULL);
	CHECK_EQ(recs[0].pts[0].p3_l,f1_0);

	//The sortnorm plane does not face the eye when its normal is 0,0,1:
	//the front list is drawn first. The faces are the same.
	putvec(ofs_sort+4,0,0,1);
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);

	CHECK_EQ(n_recs,5);
	CHECK_EQ(recs[0].type,REC_SETCOLOR);
	CHECK_EQ(recs[1].type,REC_FLAT); CHECK_EQ(recs[1].nv,4);
	CHECK_EQ(recs[2].type,REC_TMAP);
	CHECK_EQ(recs[3].type,REC_SETCOLOR);
	CHECK_EQ(recs[4].type,REC_FLAT); CHECK_EQ(recs[4].nv,3);
	putvec(ofs_sort+4,0,0,-1);

	//A face whose normal points away is not drawn. The glow value stays
	//for the next texture-mapped face that is drawn.
	putvec(ofs_flat+16,0,0,1);
	putvec(ofs_tmap+16,0,0,1);
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,0x8000,glow);

	CHECK_EQ(n_recs,2);
	CHECK_EQ(recs[0].type,REC_SETCOLOR);
	CHECK_EQ(recs[1].type,REC_FLAT); CHECK_EQ(recs[1].nv,3);
	putvec(ofs_flat+16,0,0,-1);
	putvec(ofs_tmap+16,0,0,-1);

	//the outline: after each face, white (255) and one line for each edge
	g3d_interp_outline = 1;
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);

	//tmap, setcolor, 3 lines / setcolor, flat, setcolor, 4 lines /
	//setcolor, flat, setcolor, 3 lines
	CHECK_EQ(n_recs,1+1+3 + 2+1+4 + 2+1+3);
	CHECK_EQ(recs[1].type,REC_SETCOLOR); CHECK_EQ(recs[1].color,255);
	//the edges of the tmap face 0,2,1: 0-2, 2-1, 1-0
	CHECK_EQ(recs[2].type,REC_LINE);
	CHECK_EQ(recs[2].v[0],i2f(128)); CHECK_EQ(recs[2].v[1],i2f(120));
	CHECK_EQ(recs[2].v[2],i2f(192)); CHECK_EQ(recs[2].v[3],i2f(80));
	CHECK_EQ(recs[3].v[0],i2f(192)); CHECK_EQ(recs[3].v[1],i2f(80));
	CHECK_EQ(recs[3].v[2],i2f(192)); CHECK_EQ(recs[3].v[3],i2f(120));
	CHECK_EQ(recs[4].v[0],i2f(192)); CHECK_EQ(recs[4].v[1],i2f(120));
	CHECK_EQ(recs[4].v[2],i2f(128)); CHECK_EQ(recs[4].v[3],i2f(120));
	g3d_interp_outline = 0;

	//The eye is moved to 1,0,0 and looks at the model from there: the
	//points are 1 to the left. Point 0: -3,-2,10: sx = 160 - 48 = 112
	{
		vms_vector eye = {i2f(1),0,0};

		g3_set_view_matrix(&eye,&ident,f1_0);
		clear_recs();
		g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);
		CHECK_EQ(n_recs,5);
		CHECK_EQ(recs[2].v[0],i2f(112)); CHECK_EQ(recs[2].v[1],i2f(120));
		set_simple_view();
	}

	//The morphing model. The points come from the alternate list (two
	//times the size). A face is cut into triangles, and each triangle is
	//drawn when the normal from its rotated points faces the eye.
	// tmap 0,2,1: one triangle, facing (see test_check_and_draw).
	//   light 0x8000 as above, the glow is not used.
	// flat 0,3,2,1: triangles 0,3,2 and 0,2,1, the two are facing.
	//   points: 0: 96,140  3: 96,60  2: 224,60  1: 224,140
	// subobject: DEFP_START takes its point from the start of the
	//   alternate list: -4,-4,10, rotated to -3,-4,10. The triangle 0,1,4
	//   has all its points on the line y = -4: its normal is zero, not
	//   drawn. The color is set before the check.
	for (i=0;i<4;i++) {
		morph[i].x = get32(4+i*12)*2;
		morph[i].y = get32(4+i*12+4)*2;
		morph[i].z = get32(4+i*12+8);
	}

	clear_recs();
	CHECK_EQ(g3_draw_morphing_model(model,bitmaps,NULL,0x8000,morph),1);

	CHECK_EQ(n_recs,1+1+2+1);

	r = &recs[0];
	CHECK_EQ(r->type,REC_TMAP);
	CHECK(r->bm == &bm2);
	CHECK_EQ(r->nv,3);
	CHECK(r->ptrs[0] == &points[0]); CHECK(r->ptrs[1] == &points[2]); CHECK(r->ptrs[2] == &points[1]);
	for (i=0;i<3;i++) {
		CHECK_EQ(r->pts[i].p3_u,0x1000*(i+1));
		CHECK_EQ(r->pts[i].p3_v,0x2000*(i+1));
		CHECK_EQ(r->pts[i].p3_l,0x8000);
	}
	CHECK_EQ(r->pts[0].p3_sx,i2f(96)); CHECK_EQ(r->pts[0].p3_sy,i2f(140));

	CHECK_EQ(recs[1].type,REC_SETCOLOR);

	r = &recs[2];
	CHECK_EQ(r->type,REC_FLAT); CHECK_EQ(r->nv,3);
	CHECK_EQ(r->v[0],i2f(96));  CHECK_EQ(r->v[1],i2f(140));
	CHECK_EQ(r->v[2],i2f(96));  CHECK_EQ(r->v[3],i2f(60));
	CHECK_EQ(r->v[4],i2f(224)); CHECK_EQ(r->v[5],i2f(60));

	r = &recs[3];
	CHECK_EQ(r->type,REC_FLAT); CHECK_EQ(r->nv,3);
	CHECK_EQ(r->v[0],i2f(96));  CHECK_EQ(r->v[1],i2f(140));
	CHECK_EQ(r->v[2],i2f(224)); CHECK_EQ(r->v[3],i2f(60));
	CHECK_EQ(r->v[4],i2f(224)); CHECK_EQ(r->v[5],i2f(140));

	CHECK_EQ(recs[4].type,REC_SETCOLOR); CHECK_EQ(recs[4].color,(0x4321 ^ 0x5a) & 0xff);

	CHECK_EQ(points[4].x,i2f(-3)); CHECK_EQ(points[4].y,i2f(-4)); CHECK_EQ(points[4].z,i2f(10));

	//after the morphing model, the normal opcodes are in use again
	clear_recs();
	g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);
	CHECK_EQ(n_recs,5);
	CHECK_EQ(recs[2].nv,4);
	CHECK_EQ(recs[2].v[0],i2f(128));

	CHECK(memcmp(&View_position,&zero_vec,sizeof(zero_vec)) == 0);
	CHECK(memcmp(&View_matrix,&ident,sizeof(ident)) == 0);
	CHECK_EQ(free_point_num,0);
	CHECK_EQ(model_buf[0],0xee);
	CHECK_EQ(model[model_len],0xee);

	//the rod opcode: a rod bitmap from 0,-1,10 (bot) to 0,1,10 (top),
	//width 1 at each end, bitmap 1. See test_sphere_bitmap_rod for the corners.
	{
		static ubyte rod_buf[64+1];
		ubyte *save_model = model;

		model = rod_buf+1;
		put16(0,5); put16(2,1);
		putvec(4,0,1,10);			//top point
		put32(16,f1_0);			//bot width
		putvec(20,0,-1,10);		//bot point
		put32(32,f1_0);			//top width
		put16(36,0);

		g3_init_polygon_model(model);
		CHECK_EQ(highest_texture_num,-1);		//the rod bitmap is not counted, as in the assembly

		clear_recs();
		g3_draw_polygon_model(model,bitmaps,NULL,0x8000,NULL);

		CHECK_EQ(n_recs,1);
		CHECK_EQ(recs[0].type,REC_TMAP);
		CHECK(recs[0].bm == &bm1);
		CHECK_EQ(recs[0].nv,4);
		CHECK_NEAR(recs[0].pts[0].x,i2f(-1),8); CHECK_NEAR(recs[0].pts[0].y,i2f(1),8);
		CHECK_NEAR(recs[0].pts[2].x,i2f(1),8);  CHECK_NEAR(recs[0].pts[2].y,i2f(-1),8);

		model = save_model;
	}
}

int main(void)
{
	g3_init();

	test_setup();
	test_view_matrix();
	test_codes();
	test_rotate_project();
	test_deltas();
	test_point_2_vec();
	test_facing();
	test_clip_polygon();
	test_line();
	test_special_render();
	test_check_and_draw();
	test_instance();
	test_sphere_bitmap_rod();
	test_horizon();
	test_interp();

	g3_end_frame();

	printf("test_3d: %d checks, %d failed\n",n_checks,n_fails);

	return n_fails ? 1 : 0;
}
