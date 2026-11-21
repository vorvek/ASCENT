/*
 * Test for vecmat.c, the C replacement for vecmat.asm.
 *
 * Three kinds of checks:
 *  1. Exact values that were derived by hand from the assembly.
 *  2. Exact values and checksums from test_vecmat_data.h. asm_model.py
 *     (in engine/fix), an integer model of fix.asm and vecmat.asm, writes
 *     that file. The checksum streams use the same random number generator
 *     here and in the model.
 *  3. Loose checks against floating point math.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "fix.h"
#include "vecmat.h"
#include "test_vecmat_data.h"

static int n_checks,n_fails;

static void check_eq(long long got,long long want,const char *what,int line)
{
	n_checks++;
	if (got != want) {
		n_fails++;
		if (n_fails <= 40)
			printf("FAIL line %d: %s: got %lld (0x%llx), want %lld (0x%llx)\n",
				line,what,got,(unsigned long long) got,want,(unsigned long long) want);
	}
}

#define CHECK_EQ(got,want) check_eq((long long) (got),(long long) (want),#got,__LINE__)
#define CHECK(cond) check_eq(!!(cond),1,#cond,__LINE__)
#define CHECK_VEC(v,_x,_y,_z) do { CHECK_EQ((v).x,_x); CHECK_EQ((v).y,_y); CHECK_EQ((v).z,_z); } while (0)

#define COUNT(a) ((int) (sizeof(a)/sizeof((a)[0])))

//The generator and the checksum of asm_model.py

static uint32_t rng_state,sum;

static uint32_t rng_next(void)
{
	rng_state = rng_state * 1664525u + 1013904223u;
	return rng_state;
}

static fix rng_fix(void)
{
	int32_t r = (int32_t) rng_next();
	int sh = rng_next() >> 27;

	return r >> sh;
}

//a signed value, shifted right
static fix rng_shift(int sh)
{
	return (int32_t) rng_next() >> sh;
}

static vms_vector rng_vec(void)
{
	vms_vector v;

	v.x = rng_fix();
	v.y = rng_fix();
	v.z = rng_fix();

	return v;
}

static fixang rng_ang(void)
{
	return (fixang) (rng_next() >> 16);
}

static void sum_add(uint32_t w)
{
	sum = (sum ^ w) * 16777619u;
}

static void sum_vec(const vms_vector *v)
{
	sum_add(v->x); sum_add(v->y); sum_add(v->z);
}

static void sum_mat(const vms_matrix *m)
{
	sum_vec(&m->rvec); sum_vec(&m->uvec); sum_vec(&m->fvec);
}

static void sum_ang(const vms_angvec *a)
{
	sum_add((ushort) a->p); sum_add((ushort) a->b); sum_add((ushort) a->h);
}

static void stream_start(uint32_t seed)
{
	rng_state = seed;
	sum = 2166136261u;
}

//true if the model did not compute case i (the assembly faults)
static int skip_case(const int **skip,int i)
{
	if (**skip == i) {
		(*skip)++;
		return 1;
	}
	return 0;
}

static const vms_vector sent_vec = {0x11111111,0x11111111,0x11111111};
static const vms_matrix sent_mat = {	{0x22222222,0x22222222,0x22222222},
													{0x22222222,0x22222222,0x22222222},
													{0x22222222,0x22222222,0x22222222} };
static const vms_angvec sent_ang = {0x3333,0x3333,0x3333};

static vms_vector vec_from(const uint32_t *w)
{
	vms_vector v;

	v.x = (fix) w[0]; v.y = (fix) w[1]; v.z = (fix) w[2];

	return v;
}

static int vec_eq(const vms_vector *a,const vms_vector *b)
{
	return a->x == b->x && a->y == b->y && a->z == b->z;
}

static int mat_eq_words(const vms_matrix *m,const uint32_t *w)
{
	return memcmp(m,w,sizeof(*m)) == 0;
}

//floating point helpers

static double dmag(const vms_vector *v)
{
	return sqrt((double) v->x * v->x + (double) v->y * v->y + (double) v->z * v->z);
}

static double ddot(const vms_vector *a,const vms_vector *b)
{
	return ((double) a->x * b->x + (double) a->y * b->y + (double) a->z * b->z) / 65536.0;
}

static double max_d(double a,double b)
{
	return (a > b) ? a : b;
}

//largest difference between the elements of two matrices
static double mat_diff(const vms_matrix *a,const vms_matrix *b)
{
	const fix *pa = (const fix *) a,*pb = (const fix *) b;
	double d = 0;
	int i;

	for (i=0;i<9;i++)
		d = max_d(d,fabs((double) pa[i] - pb[i]));

	return d;
}

//how far the matrix is from a rotation matrix: rows of length 1 that are
//perpendicular
static double mat_ortho_err(vms_matrix *m)
{
	double e = 0;

	e = max_d(e,fabs(dmag(&m->rvec) - 65536.0));
	e = max_d(e,fabs(dmag(&m->uvec) - 65536.0));
	e = max_d(e,fabs(dmag(&m->fvec) - 65536.0));
	e = max_d(e,fabs(ddot(&m->rvec,&m->uvec)));
	e = max_d(e,fabs(ddot(&m->rvec,&m->fvec)));
	e = max_d(e,fabs(ddot(&m->uvec,&m->fvec)));

	return e;
}

static void test_simple(void)
{
	vms_vector a = {i2f(1),i2f(2),i2f(3)},b = {i2f(4),i2f(-5),i2f(6)},d,e;
	vms_vector big = {0x7fffffff,INT32_MIN,0x40000000},one = {1,-1,0x40000000};
	vms_vector neg = {-1,-3,-5},zero = {0,0,0},c = {3,4,7};
	vms_angvec av;

	CHECK(vm_vec_make(&d,1,2,3) == &d);
	CHECK_VEC(d,1,2,3);
	CHECK(vm_angvec_make(&av,1,-2,3) == &av);
	CHECK_EQ(av.p,1); CHECK_EQ(av.b,-2); CHECK_EQ(av.h,3);

	CHECK_VEC(vmd_zero_vector,0,0,0);
	CHECK_VEC(vmd_identity_matrix.rvec,f1_0,0,0);
	CHECK_VEC(vmd_identity_matrix.uvec,0,f1_0,0);
	CHECK_VEC(vmd_identity_matrix.fvec,0,0,f1_0);

	CHECK(vm_vec_add(&d,&a,&b) == &d);
	CHECK_VEC(d,i2f(5),i2f(-3),i2f(9));
	CHECK(vm_vec_sub(&d,&a,&b) == &d);
	CHECK_VEC(d,i2f(-3),i2f(7),i2f(-3));

	//the add and the sub wrap
	vm_vec_add(&d,&big,&one);
	CHECK_VEC(d,INT32_MIN,0x7fffffff,INT32_MIN);
	vm_vec_sub(&d,&one,&big);
	CHECK_VEC(d,(fix) 0x80000002u,0x7fffffff,0);

	d = a;
	CHECK(vm_vec_add2(&d,&b) == &d);
	CHECK_VEC(d,i2f(5),i2f(-3),i2f(9));
	CHECK(vm_vec_sub2(&d,&b) == &d);
	CHECK_VEC(d,i2f(1),i2f(2),i2f(3));

	//the average shifts with sar: -1/2 is -1, -3+4 = 1 is 0
	CHECK(vm_vec_avg(&d,&neg,&zero) == &d);
	CHECK_VEC(d,-1,-2,-3);
	vm_vec_avg(&d,&neg,&c);
	CHECK_VEC(d,1,0,1);
	//the sum wraps before the shift: 0x40000000+0x40000000 is negative
	vm_vec_avg(&d,&one,&one);
	CHECK_VEC(d,1,-1,(fix) 0xc0000000u);

	vm_vec_avg4(&d,&neg,&neg,&c,&c);
	CHECK_VEC(d,1,0,1);

	//the scale shifts with no rounding: -3 * 0.5 = -1.5 is -2
	d = neg;
	CHECK(vm_vec_scale(&d,f0_5) == &d);
	CHECK_VEC(d,-1,-2,-3);
	CHECK(vm_vec_copy_scale(&d,&a,f0_5) == &d);
	CHECK_VEC(d,f0_5,f1_0,f1_0+f0_5);
	CHECK(vm_vec_scale_add(&d,&a,&b,2*f1_0) == &d);
	CHECK_VEC(d,i2f(9),i2f(-8),i2f(15));
	d = a;
	CHECK(vm_vec_scale_add2(&d,&b,-f1_0) == &d);
	CHECK_VEC(d,i2f(-3),i2f(7),i2f(-3));

	//scale2 divides with idiv: toward zero. 6*1/2, -7*1/2, 9*1/2
	vm_vec_make(&d,6,-7,9);
	CHECK(vm_vec_scale2(&d,1,2) == &d);
	CHECK_VEC(d,3,-3,4);
	//port only: d == 0 leaves the vector as it is, and an overflow saturates
	vm_vec_scale2(&d,5,0);
	CHECK_VEC(d,3,-3,4);
	vm_vec_make(&d,0x40000000,-0x40000000,1);
	vm_vec_scale2(&d,8,1);
	CHECK_VEC(d,INT32_MAX,INT32_MIN,8);

	(void) e;
}

static void test_mag(void)
{
	vms_vector v,w,d;
	fix mag;
	int i;

	//by hand. 3,4,0: the sum of the squares is 25 * 2^32. quad_sqrt:
	//guess 5<<16, quotient 5<<16, correct
	vm_vec_make(&v,i2f(3),i2f(4),0);
	CHECK_EQ(vm_vec_mag(&v),i2f(5));
	vm_vec_make(&w,0,i2f(8),0);
	CHECK_EQ(vm_vec_dist(&v,&w),i2f(5));		//3,-4,0
	CHECK_EQ(vm_vec_dist(&w,&v),i2f(5));

	//by hand. quick: 4 + (3/4 + 0) + (3/4)/2 = 5.125
	CHECK_EQ(vm_vec_mag_quick(&v),0x52000);
	CHECK_EQ(vm_vec_dist_quick(&v,&w),0x52000);
	vm_vec_make(&v,0,i2f(-3),i2f(4));
	CHECK_EQ(vm_vec_mag_quick(&v),0x52000);
	//by hand. 8,4,2: b = 4/4 + 2/8 = 1.25, a = 8 + 1.25 + 0.625
	vm_vec_make(&v,i2f(-2),i2f(8),i2f(-4));
	CHECK_EQ(vm_vec_mag_quick(&v),i2f(9) + 0xe000);
	//the shifts are done before the add: 7>>2 + 7>>3 = 1, then 1>>1 = 0
	vm_vec_make(&v,7,7,7);
	CHECK_EQ(vm_vec_mag_quick(&v),8);

	//by hand. the normalize divides with idiv: toward zero.
	//3*65536/5 = 39321.6, 4*65536/5 = 52428.8
	vm_vec_make(&v,i2f(3),i2f(-4),0);
	d = sent_vec;
	CHECK_EQ(vm_vec_copy_normalize(&d,&v),i2f(5));
	CHECK_VEC(d,39321,-52428,0);
	CHECK_VEC(v,i2f(3),i2f(-4),0);
	CHECK_EQ(vm_vec_normalize(&v),i2f(5));
	CHECK_VEC(v,39321,-52428,0);

	//by hand. quick: divide by 5.125. 3*65536*65536/0x52000 = 38362.5,
	//4*65536*65536/0x52000 = 51150.05
	vm_vec_make(&v,i2f(3),i2f(-4),0);
	CHECK_EQ(vm_vec_normalize_quick(&v),0x52000);
	CHECK_VEC(v,38362,-51150,0);
	vm_vec_make(&v,i2f(3),i2f(-4),0);
	d = sent_vec;
	CHECK_EQ(vm_vec_copy_normalize_quick(&d,&v),0x52000);
	CHECK_VEC(d,38362,-51150,0);

	//zero vector: returns zero, and the dest is not changed
	vm_vec_make(&v,0,0,0);
	CHECK_EQ(vm_vec_mag(&v),0);
	CHECK_EQ(vm_vec_mag_quick(&v),0);
	CHECK_EQ(vm_vec_normalize(&v),0);
	CHECK_VEC(v,0,0,0);
	CHECK_EQ(vm_vec_normalize_quick(&v),0);
	CHECK_VEC(v,0,0,0);
	d = sent_vec;
	CHECK_EQ(vm_vec_copy_normalize(&d,&v),0);
	CHECK(vec_eq(&d,&sent_vec));
	CHECK_EQ(vm_vec_copy_normalize_quick(&d,&v),0);
	CHECK(vec_eq(&d,&sent_vec));

	//normalized_dir of a point with itself: zero, and the dest is the difference
	vm_vec_make(&w,i2f(7),i2f(-7),7);
	d = sent_vec;
	CHECK_EQ(vm_vec_normalized_dir(&d,&w,&w),0);
	CHECK_VEC(d,0,0,0);
	d = sent_vec;
	CHECK_EQ(vm_vec_normalized_dir_quick(&d,&w,&w),0);
	CHECK_VEC(d,0,0,0);

	//dest = normalized(end - start)
	vm_vec_make(&v,i2f(4),i2f(-5),i2f(1));
	vm_vec_make(&w,i2f(1),i2f(-1),i2f(1));
	CHECK_EQ(vm_vec_normalized_dir(&d,&v,&w),i2f(5));
	CHECK_VEC(d,39321,-52428,0);
	CHECK_EQ(vm_vec_normalized_dir_quick(&d,&v,&w),0x52000);
	CHECK_VEC(d,38362,-51150,0);

	//by hand. a small vector: mag of (1,0,0) is long_sqrt(1) = 1, and
	//1.0/1 is 65536
	vm_vec_make(&v,0,-1,0);
	CHECK_EQ(vm_vec_normalize(&v),1);
	CHECK_VEC(v,0,-f1_0,0);

	//by hand. large magnitudes, the quad path of the square root.
	//0x40000000: the square is 2^60, guess 4<<28, quotient 2^30, correct
	vm_vec_make(&v,0x40000000,0,0);
	CHECK_EQ(vm_vec_mag(&v),0x40000000);
	CHECK_EQ(vm_vec_normalize(&v),0x40000000);
	CHECK_VEC(v,f1_0,0,0);

	//0x80000000: the square is 2^62, guess 8<<28, quotient 2^31, correct.
	//the magnitude 0x80000000 is a negative divisor for idiv, and
	//-2^31 * 65536 / -2^31 = 65536: the sign of the result is wrong
	vm_vec_make(&v,0,INT32_MIN,0);
	CHECK_EQ((uint32_t) vm_vec_mag(&v),0x80000000u);
	CHECK_EQ((uint32_t) vm_vec_normalize(&v),0x80000000u);
	CHECK_VEC(v,0,f1_0,0);

	//three times 2^62 does not fit a signed quad: the square root is zero
	vm_vec_make(&v,INT32_MIN,INT32_MIN,INT32_MIN);
	CHECK_EQ(vm_vec_mag(&v),0);
	CHECK_EQ(vm_vec_normalize(&v),0);
	CHECK_VEC(v,INT32_MIN,INT32_MIN,INT32_MIN);

	//model: table of cases
	for (i=0;i<COUNT(norm_cases);i++) {
		v = vec_from(norm_cases[i].v);
		CHECK_EQ((uint32_t) vm_vec_mag(&v),norm_cases[i].mag);
		CHECK_EQ((uint32_t) vm_vec_mag_quick(&v),norm_cases[i].magq);

		d = sent_vec;
		mag = vm_vec_copy_normalize(&d,&v);
		CHECK_EQ((uint32_t) mag,norm_cases[i].mag);
		CHECK_VEC(d,(fix) norm_cases[i].n[0],(fix) norm_cases[i].n[1],(fix) norm_cases[i].n[2]);

		d = sent_vec;
		mag = vm_vec_copy_normalize_quick(&d,&v);
		CHECK_EQ((uint32_t) mag,norm_cases[i].magq);
		CHECK_VEC(d,(fix) norm_cases[i].nq[0],(fix) norm_cases[i].nq[1],(fix) norm_cases[i].nq[2]);
	}
}

static void test_dot_cross(void)
{
	vms_vector a,b,d,x = {f1_0,0,0},y = {0,f1_0,0},z = {0,0,f1_0};
	vms_vector p0,p1,p2;
	int i;

	vm_vec_make(&a,i2f(1),i2f(2),i2f(3));
	vm_vec_make(&b,i2f(4),i2f(-5),i2f(6));
	CHECK_EQ(vm_vec_dotprod(&a,&b),i2f(12));
	CHECK_EQ(vm_vec_dot(&a,&b),i2f(12));

	//by hand. the shift has no rounding: 1*1 >> 16 = 0, -1*1 >> 16 = -1
	vm_vec_make(&a,1,0,0);
	vm_vec_make(&b,-1,0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&a),0);
	CHECK_EQ(vm_vec_dotprod(&a,&b),-1);
	//the three products are added as quads, then shifted: 3 * 0x8000 >> 16 = 1
	vm_vec_make(&a,0x8000,0x8000,0x8000);
	vm_vec_make(&b,1,1,1);
	CHECK_EQ(vm_vec_dotprod(&a,&b),1);

	//by hand. 181*181 = 32761 fits. 182*182 = 33124 does not: saturated value
	vm_vec_make(&a,i2f(181),0,0);
	vm_vec_make(&b,i2f(-181),0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&a),i2f(32761));
	CHECK_EQ(vm_vec_dotprod(&a,&b),i2f(-32761));
	vm_vec_make(&a,i2f(182),0,0);
	vm_vec_make(&b,i2f(-182),0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&a),0x7fffffff);
	CHECK_EQ(vm_vec_dotprod(&a,&b),-0x7fffffff);
	//-32768.0 fits, and one bit less than 32768.0 fits
	vm_vec_make(&a,i2f(-128),i2f(128),0);
	vm_vec_make(&b,i2f(128),i2f(-128),0);
	CHECK_EQ(vm_vec_dotprod(&a,&b),INT32_MIN);
	CHECK_EQ(vm_vec_dotprod(&a,&a),0x7fffffff);		//32768.0: saturated
	vm_vec_make(&a,0x7fffffff,0,0);
	vm_vec_make(&b,0x8000,0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&b),0x3fffffff);
	vm_vec_make(&b,0x10000,0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&b),0x7fffffff);		//fits
	vm_vec_make(&b,0x10001,0,0);
	CHECK_EQ(vm_vec_dotprod(&a,&b),0x7fffffff);		//saturated

	//left-handed axes: x cross y = z, and so on
	CHECK(vm_vec_crossprod(&d,&x,&y) == &d);
	CHECK(vec_eq(&d,&z));
	vm_vec_cross(&d,&y,&z);
	CHECK(vec_eq(&d,&x));
	vm_vec_crossprod(&d,&z,&x);
	CHECK(vec_eq(&d,&y));
	vm_vec_crossprod(&d,&y,&x);
	CHECK_VEC(d,0,0,-f1_0);

	//by hand. (1,2,3) cross (4,5,6) = (-3,6,-3)
	vm_vec_make(&a,i2f(1),i2f(2),i2f(3));
	vm_vec_make(&b,i2f(4),i2f(5),i2f(6));
	vm_vec_crossprod(&d,&a,&b);
	CHECK_VEC(d,i2f(-3),i2f(6),i2f(-3));

	//by hand. the difference of the two quad products is shifted, with no
	//rounding: (0,1,0) cross (0,0,1) = 1 >> 16 = 0. the other way: -1 >> 16 = -1
	vm_vec_make(&a,0,1,0);
	vm_vec_make(&b,0,0,1);
	vm_vec_crossprod(&d,&a,&b);
	CHECK_VEC(d,0,0,0);
	vm_vec_crossprod(&d,&b,&a);
	CHECK_VEC(d,-1,0,0);

	//perp: (p1-p0) cross (p2-p1), with no shift if the edges are in range
	vm_vec_make(&p0,i2f(1),i2f(1),i2f(1));
	vm_vec_make(&p1,i2f(2),i2f(1),i2f(1));
	vm_vec_make(&p2,i2f(2),i2f(3),i2f(1));
	CHECK(vm_vec_perp(&d,&p0,&p1,&p2) == &d);
	CHECK_VEC(d,0,0,i2f(2));
	CHECK(vm_vec_normal(&d,&p0,&p1,&p2) == &d);
	CHECK_VEC(d,0,0,f1_0);

	//by hand. edges that are too small go up: 1 becomes 1<<12, then <<2
	//and <<2 again to reach bit 15 (shift 16), 2 becomes 2<<12 then <<2
	//(shift 14).  cross of (0x10000,0,0) and (0,0x8000,0) = (0,0,0x8000)
	vm_vec_make(&p0,0,0,0);
	vm_vec_make(&p1,1,0,0);
	vm_vec_make(&p2,1,2,0);
	vm_vec_perp(&d,&p0,&p1,&p2);
	CHECK_VEC(d,0,0,0x8000);
	vm_vec_normal(&d,&p0,&p1,&p2);
	CHECK_VEC(d,0,0,f1_0);

	//by hand. edges that are too large go down: 0x10000000 has no bits in
	//0x00f00000, so it goes down by 2 until 0xfffc0000 is clear: shift 12,
	//to 0x10000.  0x00300000 goes down by 4 (0x30000), which has no bits
	//in 0xfffc0000.  cross of (0x10000,0,0) and (0,0x30000,0) = (0,0,0x30000)
	vm_vec_make(&p1,0x10000000,0,0);
	vm_vec_make(&p2,0x10000000,0x00300000,0);
	vm_vec_perp(&d,&p0,&p1,&p2);
	CHECK_VEC(d,0,0,0x30000);

	//a null edge
	vm_vec_perp(&d,&p0,&p0,&p2);
	CHECK_VEC(d,0,0,0);
	vm_vec_normal(&d,&p0,&p0,&p2);
	CHECK_VEC(d,0,0,0);

	//port only: the assembly does not end for an edge component of 0x80000000
	vm_vec_make(&p1,INT32_MIN,0,0);
	vm_vec_make(&p2,INT32_MIN,i2f(1),0);
	vm_vec_perp(&d,&p0,&p1,&p2);
	CHECK_EQ(d.x,0);
	CHECK_EQ(d.y,0);

	//dist_to_plane: (checkp - planep) dot norm
	vm_vec_make(&a,0,i2f(5),i2f(9));
	vm_vec_make(&b,i2f(3),i2f(2),i2f(-4));
	CHECK_EQ(vm_dist_to_plane(&a,&y,&b),i2f(3));
	CHECK_EQ(vm_dist_to_plane(&b,&y,&a),i2f(-3));

	//model: table of cases
	for (i=0;i<COUNT(dot_cases);i++) {
		a = vec_from(dot_cases[i].a);
		b = vec_from(dot_cases[i].b);
		CHECK_EQ(vm_vec_dotprod(&a,&b),(fix) dot_cases[i].dot);
		vm_vec_crossprod(&d,&a,&b);
		CHECK_VEC(d,(fix) dot_cases[i].cross[0],(fix) dot_cases[i].cross[1],(fix) dot_cases[i].cross[2]);
	}
}

static void test_matrix(void)
{
	vms_matrix m,m2,m3;
	vms_angvec av,ev;
	vms_vector v,d,x = {f1_0,0,0},y = {0,f1_0,0},z = {0,0,f1_0};
	int i;

	//no angles: identity
	vm_angvec_make(&av,0,0,0);
	m = sent_mat;
	CHECK(vm_angles_2_matrix(&m,&av) == &m);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);

	//by hand. pitch 0x4000: sinp=1, cosp=0, the other sines 0 and cosines 1.
	//m1=cbch=1, m8=cbch*sp=1, m6=-sp=-1, the others 0
	vm_angvec_make(&av,0x4000,0,0);
	vm_angles_2_matrix(&m,&av);
	CHECK_VEC(m.rvec,f1_0,0,0);
	CHECK_VEC(m.uvec,0,0,f1_0);
	CHECK_VEC(m.fvec,0,-f1_0,0);

	//by hand. heading 0x4000: sinh=1, cosh=0. m3=sh*cp=1, m7=-cbsh=-1, m5=cbcp=1
	vm_angvec_make(&av,0,0,0x4000);
	vm_angles_2_matrix(&m,&av);
	CHECK_VEC(m.rvec,0,0,-f1_0);
	CHECK_VEC(m.uvec,0,f1_0,0);
	CHECK_VEC(m.fvec,f1_0,0,0);

	//by hand. bank 0x4000: sinb=1, cosb=0. m2=-sbch=-1, m4=sbcp=1, m9=chcp=1
	vm_angvec_make(&av,0,0x4000,0);
	vm_angles_2_matrix(&m,&av);
	CHECK_VEC(m.rvec,0,f1_0,0);
	CHECK_VEC(m.uvec,-f1_0,0,0);
	CHECK_VEC(m.fvec,0,0,f1_0);

	//rotate: the dot products with rvec, uvec, fvec
	vm_vec_make(&v,i2f(1),i2f(2),i2f(3));
	CHECK(vm_vec_rotate(&d,&v,&m) == &d);
	CHECK_VEC(d,i2f(2),i2f(-1),i2f(3));
	vm_vec_rotate(&d,&v,&vmd_identity_matrix);
	CHECK_VEC(d,i2f(1),i2f(2),i2f(3));
	//by hand. no rounding and no saturation: -1 * 1.0 >> 16 = -1
	vm_vec_make(&v,-1,0x7fffffff,0);
	vm_vec_rotate(&d,&v,&vmd_identity_matrix);
	CHECK_VEC(d,-1,0x7fffffff,0);

	//transpose
	vm_vec_make(&m.rvec,1,2,3);
	vm_vec_make(&m.uvec,4,5,6);
	vm_vec_make(&m.fvec,7,8,9);
	CHECK(vm_copy_transpose_matrix(&m2,&m) == &m2);
	CHECK_VEC(m2.rvec,1,4,7);
	CHECK_VEC(m2.uvec,2,5,8);
	CHECK_VEC(m2.fvec,3,6,9);
	m3 = sent_mat;
	CHECK(vm_copy_transpose(&m3,&m) == &m3);
	CHECK(memcmp(&m2,&m3,sizeof(m2)) == 0);
	m3 = m;
	CHECK(vm_transpose_matrix(&m3) == &m3);
	CHECK(memcmp(&m2,&m3,sizeof(m2)) == 0);
	CHECK(vm_transpose(&m3) == &m3);
	CHECK(memcmp(&m,&m3,sizeof(m)) == 0);

	//matrix times identity, and a matrix times its transpose
	vm_angvec_make(&av,0x1234,0x5678,(fixang) 0x9abc);
	vm_angles_2_matrix(&m,&av);
	CHECK(vm_matrix_x_matrix(&m2,&m,&vmd_identity_matrix) == &m2);
	CHECK(memcmp(&m,&m2,sizeof(m)) == 0);
	vm_matrix_x_matrix(&m2,&vmd_identity_matrix,&m);
	CHECK(memcmp(&m,&m2,sizeof(m)) == 0);
	vm_copy_transpose_matrix(&m2,&m);
	vm_matrix_x_matrix(&m3,&m,&m2);
	CHECK(mat_diff(&m3,&vmd_identity_matrix) < 64.0);

	//by hand. m = rows (1,2,3),(4,5,6),(7,8,9) in units of 1.0, squared:
	//(30,36,42),(66,81,96),(102,126,150). the first row of the matrix is
	//rvec.x,uvec.x,fvec.x
	vm_vec_make(&m.rvec,i2f(1),i2f(4),i2f(7));
	vm_vec_make(&m.uvec,i2f(2),i2f(5),i2f(8));
	vm_vec_make(&m.fvec,i2f(3),i2f(6),i2f(9));
	vm_matrix_x_matrix(&m2,&m,&m);
	CHECK_VEC(m2.rvec,i2f(30),i2f(66),i2f(102));
	CHECK_VEC(m2.uvec,i2f(36),i2f(81),i2f(126));
	CHECK_VEC(m2.fvec,i2f(42),i2f(96),i2f(150));

	//extract the angles of the identity, and of single angles
	ev = sent_ang;
	CHECK(vm_extract_angles_matrix(&ev,&vmd_identity_matrix) == &ev);
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0);
	vm_angvec_make(&av,0,0,0x4000);
	vm_extract_angles_matrix(&ev,vm_angles_2_matrix(&m,&av));
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0x4000);
	vm_angvec_make(&av,0,-0x4000,0);
	vm_extract_angles_matrix(&ev,vm_angles_2_matrix(&m,&av));
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,-0x4000); CHECK_EQ(ev.h,0);
	//pitched straight up: the cosine of pitch is zero. say no bank
	vm_angvec_make(&av,0x4000,0x1000,0);
	vm_extract_angles_matrix(&ev,vm_angles_2_matrix(&m,&av));
	CHECK_EQ(ev.p,0x4000); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0);

	//model: table of cases
	for (i=0;i<COUNT(angle_cases);i++) {
		vm_angvec_make(&av,(fixang) angle_cases[i].a[0],(fixang) angle_cases[i].a[1],(fixang) angle_cases[i].a[2]);
		m = sent_mat;
		vm_angles_2_matrix(&m,&av);
		CHECK(mat_eq_words(&m,angle_cases[i].m));
		ev = sent_ang;
		vm_extract_angles_matrix(&ev,&m);
		CHECK_EQ((ushort) ev.p,angle_cases[i].e[0]);
		CHECK_EQ((ushort) ev.b,angle_cases[i].e[1]);
		CHECK_EQ((ushort) ev.h,angle_cases[i].e[2]);
	}

	//vector_2_matrix, forward only. by hand: forward = z gives the
	//identity: x = (z.z,0,-z.x) = (1,0,0), y = z cross x = (0,1,0)
	vm_vec_make(&v,0,0,i2f(7));
	m = sent_mat;
	CHECK(vm_vector_2_matrix(&m,&v,NULL,NULL) == &m);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);

	//forward vector is straight up (or down)
	vm_vec_make(&v,0,i2f(5),0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,NULL,NULL);
	CHECK_VEC(m.rvec,f1_0,0,0);
	CHECK_VEC(m.uvec,0,0,-f1_0);
	CHECK_VEC(m.fvec,0,f1_0,0);
	vm_vec_make(&v,0,-3,0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,NULL,NULL);
	CHECK_VEC(m.rvec,f1_0,0,0);
	CHECK_VEC(m.uvec,0,0,f1_0);
	CHECK_VEC(m.fvec,0,-f1_0,0);

	//forward vector with no length: the matrix is not changed
	vm_vec_make(&v,0,0,0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,NULL,NULL);
	CHECK(memcmp(&m,&sent_mat,sizeof(m)) == 0);
	vm_vector_2_matrix(&m,&v,&y,NULL);
	CHECK(memcmp(&m,&sent_mat,sizeof(m)) == 0);
	vm_vector_2_matrix(&m,&v,NULL,&x);
	CHECK(memcmp(&m,&sent_mat,sizeof(m)) == 0);

	//forward and up, forward and right: the axes give the identity
	vm_vec_make(&v,0,0,i2f(2));
	vm_vec_make(&d,0,i2f(3),0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,&d,NULL);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	vm_vec_make(&d,i2f(3),0,0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,NULL,&d);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	//if both up & right vectors are passed, the up vector is used
	vm_vec_make(&d,0,i2f(3),0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,&d,&z);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	//up vector with no length, or along the forward vector: use just the forward vector
	vm_vec_make(&d,0,0,0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,&d,NULL);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,&z,NULL);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	m = sent_mat;
	vm_vector_2_matrix(&m,&v,NULL,&z);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);

	//vec_ang_2_matrix: forward = z, no bank: identity. by hand: sinp=0,
	//cosp=sqrt(1.0)=1.0, sinh=0/1, cosh=1/1
	m = sent_mat;
	CHECK(vm_vec_ang_2_matrix(&m,&z,0) == &m);
	CHECK(memcmp(&m,&vmd_identity_matrix,sizeof(m)) == 0);
	//forward = x, bank 0x4000: heading 0x4000, the same as the angles
	vm_angvec_make(&av,0,0x4000,0x4000);
	vm_angles_2_matrix(&m2,&av);
	vm_vec_ang_2_matrix(&m,&x,0x4000);
	CHECK(memcmp(&m,&m2,sizeof(m)) == 0);
	//port only: straight up was a divide by zero. it must not crash
	vm_vec_ang_2_matrix(&m,&y,0);
	CHECK_EQ(m.fvec.y,f1_0);
}

static void test_angles(void)
{
	vms_vector a,b,x = {f1_0,0,0},y = {0,f1_0,0},z = {0,0,f1_0},nz = {0,0,-f1_0};
	vms_angvec ev;

	//by hand. x to y: the dot product is 0, arccos is 0x4000. x cross y is
	//z, so the angle around z is positive and around -z is negative
	CHECK_EQ(vm_vec_delta_ang_norm(&x,&y,NULL),0x4000);
	CHECK_EQ(vm_vec_delta_ang_norm(&x,&y,&z),0x4000);
	CHECK_EQ(vm_vec_delta_ang_norm(&x,&y,&nz),-0x4000);
	CHECK_EQ(vm_vec_delta_ang_norm(&y,&x,&z),-0x4000);
	CHECK_EQ(vm_vec_delta_ang_norm(&x,&x,NULL),0);
	//opposite vectors: half a circle, which is 0x8000 with either sign
	CHECK_EQ((ushort) vm_vec_delta_ang_norm(&z,&nz,NULL),0x8000);
	CHECK_EQ((ushort) vm_vec_delta_ang_norm(&z,&nz,&x),0x8000);

	//the version for vectors that are not normalized normalizes them in place
	vm_vec_make(&a,i2f(3),0,0);
	vm_vec_make(&b,0,-5,0);
	CHECK_EQ(vm_vec_delta_ang(&a,&b,NULL),0x4000);
	CHECK_VEC(a,f1_0,0,0);
	CHECK_VEC(b,0,-f1_0,0);
	vm_vec_make(&a,i2f(3),0,0);
	vm_vec_make(&b,0,-5,0);
	CHECK_EQ(vm_vec_delta_ang(&a,&b,&z),-0x4000);

	//extract_angles_vector. by hand: forward z: p = asin(0), h = atan2(1,0) = 0
	ev = sent_ang;
	CHECK(vm_extract_angles_vector(&ev,&z) == &ev);
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0);
	//x, any length: heading 0x4000
	vm_vec_make(&a,i2f(9),0,0);
	vm_extract_angles_vector(&ev,&a);
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0x4000);
	CHECK_VEC(a,i2f(9),0,0);		//the source is not changed
	//-z: heading 0x8000
	vm_extract_angles_vector(&ev,&nz);
	CHECK_EQ(ev.p,0); CHECK_EQ(ev.b,0); CHECK_EQ((ushort) ev.h,0x8000);
	//up: p = asin(-1) = -0x4000, and the heading is zero
	ev = sent_ang;
	vm_extract_angles_vector(&ev,&y);
	CHECK_EQ(ev.p,-0x4000); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0);
	vm_extract_angles_vector_normalized(&ev,&y);
	CHECK_EQ(ev.p,-0x4000); CHECK_EQ(ev.b,0); CHECK_EQ(ev.h,0);
	//no length: the angles are not changed
	vm_vec_make(&a,0,0,0);
	ev = sent_ang;
	vm_extract_angles_vector(&ev,&a);
	CHECK_EQ(ev.p,0x3333); CHECK_EQ(ev.b,0x3333); CHECK_EQ(ev.h,0x3333);
}

//the calculations of the BASIC streams of the model
static void sum_basic(vms_vector *a,vms_vector *b,fix k)
{
	vms_vector d,e;

	vm_vec_add(&d,a,b);		sum_vec(&d);
	e = *a;  vm_vec_add2(&e,b);	CHECK(vec_eq(&d,&e));
	vm_vec_sub(&d,a,b);		sum_vec(&d);
	e = *a;  vm_vec_sub2(&e,b);	CHECK(vec_eq(&d,&e));
	vm_vec_avg(&d,a,b);		sum_vec(&d);
	vm_vec_copy_scale(&d,a,k);	sum_vec(&d);
	e = *a;  vm_vec_scale(&e,k);	CHECK(vec_eq(&d,&e));
	vm_vec_scale_add(&d,a,b,k);	sum_vec(&d);
	e = *a;  vm_vec_scale_add2(&e,b,k);	CHECK(vec_eq(&d,&e));
	sum_add(vm_vec_mag(a));
	sum_add(vm_vec_dist(a,b));
	sum_add(vm_vec_mag_quick(a));
	sum_add(vm_vec_dist_quick(a,b));
	sum_add(vm_vec_dotprod(a,b));
	vm_vec_crossprod(&d,a,b);	sum_vec(&d);
}

static void test_streams(void)
{
	const int *skip;
	int i;
	double e,max_mag = 0,max_dot = 0,max_cross = 0,max_norm = 0,max_quick = 0;
	double max_ortho = 0,max_rot = 0,max_extract = 0,max_v2m = 0;

	stream_start(0x2001);
	skip = SKIP_BASIC;
	for (i=0;i<N_BASIC;i++) {
		vms_vector a = rng_vec(),b = rng_vec();
		fix k = rng_fix();

		if (skip_case(&skip,i))
			continue;
		sum_basic(&a,&b,k);

		//floating point: the square root is within 1 for each magnitude
		//that fits (the sum of the squares is less than 2^63)
		if (dmag(&a) < 3.0e9)
			max_mag = max_d(max_mag,fabs((double) (uint32_t) vm_vec_mag(&a) - dmag(&a)));
	}
	CHECK_EQ(sum,SUM_BASIC);

	stream_start(0x2002);
	skip = SKIP_BASIC_SMALL;
	for (i=0;i<N_BASIC_SMALL;i++) {
		vms_vector a,b,d;
		fix k;

		a.x = rng_shift(14); a.y = rng_shift(14); a.z = rng_shift(14);
		b.x = rng_shift(12); b.y = rng_shift(12); b.z = rng_shift(12);
		k = rng_shift(13);

		if (skip_case(&skip,i))
			continue;
		sum_basic(&a,&b,k);

		//floating point. a is up to 2.0 and b is up to 8.0 on each axis,
		//so there is no overflow
		max_mag = max_d(max_mag,fabs(vm_vec_mag(&a) - dmag(&a)));
		max_dot = max_d(max_dot,fabs(vm_vec_dotprod(&a,&b) - ddot(&a,&b)));
		vm_vec_crossprod(&d,&a,&b);
		max_cross = max_d(max_cross,fabs(d.x - ((double) a.y * b.z - (double) a.z * b.y) / 65536.0));
		max_cross = max_d(max_cross,fabs(d.y - ((double) a.z * b.x - (double) a.x * b.z) / 65536.0));
		max_cross = max_d(max_cross,fabs(d.z - ((double) a.x * b.y - (double) a.y * b.x) / 65536.0));

		//the approximate magnitude is from 10 percent low (three equal
		//components) to 9 percent high
		if (dmag(&b) > 4096.0) {
			e = vm_vec_mag_quick(&b) / dmag(&b);
			max_quick = max_d(max_quick,fabs(e - 1.0));
		}

		//a normalized vector has length 1.0
		if (vm_vec_copy_normalize(&d,&b) >= 0x1000)
			max_norm = max_d(max_norm,fabs(dmag(&d) - 65536.0));
	}
	CHECK_EQ(sum,SUM_BASIC_SMALL);

	printf("mag: max difference from floating point %.3f\n",max_mag);
	printf("dot: %.3f  cross: %.3f  length of normalized vector: %.3f\n",max_dot,max_cross,max_norm);
	printf("mag_quick / mag - 1: %.3f\n",max_quick);
	CHECK(max_mag <= 1.0);
	CHECK(max_dot <= 1.0);
	CHECK(max_cross <= 1.0);
	CHECK(max_norm <= 32.0);
	CHECK(max_quick <= 0.11);

	stream_start(0x2003);
	skip = SKIP_NORM;
	for (i=0;i<N_NORM;i++) {
		vms_vector a = rng_vec(),b = rng_vec(),d;
		fix mag;

		if (skip_case(&skip,i))
			continue;
		d = sent_vec;  mag = vm_vec_copy_normalize(&d,&a);			sum_vec(&d);  sum_add(mag);
		d = sent_vec;  mag = vm_vec_copy_normalize_quick(&d,&a);		sum_vec(&d);  sum_add(mag);
		d = sent_vec;  mag = vm_vec_normalized_dir(&d,&a,&b);			sum_vec(&d);  sum_add(mag);
		d = sent_vec;  mag = vm_vec_normalized_dir_quick(&d,&a,&b);	sum_vec(&d);  sum_add(mag);
	}
	CHECK_EQ(sum,SUM_NORM);

	stream_start(0x2004);
	skip = SKIP_PERP;
	for (i=0;i<N_PERP;i++) {
		vms_vector p0 = rng_vec(),p1 = rng_vec(),p2 = rng_vec(),d;

		if (skip_case(&skip,i))
			continue;
		vm_vec_perp(&d,&p0,&p1,&p2);		sum_vec(&d);
		vm_vec_normal(&d,&p0,&p1,&p2);	sum_vec(&d);
	}
	CHECK_EQ(sum,SUM_PERP);

	stream_start(0x2005);
	skip = SKIP_MATRIX;
	for (i=0;i<N_MATRIX;i++) {
		vms_angvec a1,a2,e1,e2;
		vms_matrix m1,m2,mm,mt,mt2,back;
		vms_vector v,d;

		a1.p = rng_ang(); a1.b = rng_ang(); a1.h = rng_ang();
		a2.p = rng_ang(); a2.b = rng_ang(); a2.h = rng_ang();
		v = rng_vec();

		if (skip_case(&skip,i))
			continue;
		vm_angles_2_matrix(&m1,&a1);			sum_mat(&m1);
		vm_vec_rotate(&d,&v,&m1);				sum_vec(&d);
		vm_angles_2_matrix(&m2,&a2);
		vm_matrix_x_matrix(&mm,&m1,&m2);		sum_mat(&mm);
		vm_copy_transpose_matrix(&mt,&mm);	sum_mat(&mt);
		mt2 = mm;  vm_transpose_matrix(&mt2);
		CHECK(memcmp(&mt,&mt2,sizeof(mt)) == 0);
		e1 = sent_ang;  vm_extract_angles_matrix(&e1,&m1);	sum_ang(&e1);
		e2 = sent_ang;  vm_extract_angles_matrix(&e2,&mm);	sum_ang(&e2);

		//floating point: the matrices are rotation matrices, a rotation
		//keeps the length, and the extracted angles give the matrix back
		//(not near pitch straight up or down, where the bank is lost)
		max_ortho = max_d(max_ortho,mat_ortho_err(&m1));
		max_ortho = max_d(max_ortho,mat_ortho_err(&mm));
		if (dmag(&v) < 1.0e9 && dmag(&v) > 65536.0)
			max_rot = max_d(max_rot,fabs(dmag(&d) / dmag(&v) - 1.0));
		if (abs(m1.fvec.y) < 0xf000) {
			vm_angles_2_matrix(&back,&e1);
			max_extract = max_d(max_extract,mat_diff(&back,&m1));
		}
	}
	CHECK_EQ(sum,SUM_MATRIX);

	printf("matrix: rotation matrix error %.2f, rotate length error %.6f, extract error %.2f\n",
		max_ortho,max_rot,max_extract);
	//the sines and cosines are up to 10 off (see test_fix), and an element
	//of the matrix is a sum of products of them
	CHECK(max_ortho <= 64.0);
	CHECK(max_rot <= 0.001);
	CHECK(max_extract <= 256.0);

	stream_start(0x2006);
	skip = SKIP_V2M;
	for (i=0;i<N_V2M;i++) {
		vms_vector f = rng_vec(),u = rng_vec(),r = rng_vec();
		int mode = rng_next() >> 30;
		int sel = rng_next() >> 28;
		vms_matrix m = sent_mat;

		if (sel == 0)
			f.x = f.z = 0;
		else if (sel == 1)
			u = vmd_zero_vector;
		else if (sel == 2)
			r = vmd_zero_vector;
		else if (sel == 3)
			u = f;
		else if (sel == 4)
			r = f;
		else if (sel == 5)
			f = vmd_zero_vector;

		if (skip_case(&skip,i))
			continue;
		vm_vector_2_matrix(&m,&f,(mode & 1) ? &u : NULL,(mode & 2) ? &r : NULL);
		sum_mat(&m);

		//floating point: a rotation matrix, if the inputs are not too
		//small, not too large, and not close to the cases where the
		//routine loses precision: forward vector close to straight up or
		//down (forward only), or close to the other vector
		if (sel > 5 && dmag(&f) > 1.0e6 && dmag(&u) > 1.0e6 && dmag(&r) > 1.0e6 &&
				dmag(&f) < 1.0e9 && dmag(&u) < 1.0e9 && dmag(&r) < 1.0e9) {
			vms_vector *other = (mode & 1) ? &u : &r;
			int ok;

			if (mode == 0)
				ok = sqrt((double) f.x * f.x + (double) f.z * f.z) > 0.1 * dmag(&f);
			else
				ok = fabs(ddot(&f,other)) * 65536.0 < 0.9 * dmag(&f) * dmag(other);

			if (ok)
				max_v2m = max_d(max_v2m,mat_ortho_err(&m));
		}
	}
	CHECK_EQ(sum,SUM_V2M);

	printf("vector_2_matrix: rotation matrix error %.2f\n",max_v2m);
	CHECK(max_v2m <= 64.0);

	stream_start(0x2007);
	skip = SKIP_DELTA_ANG;
	for (i=0;i<N_DELTA_ANG;i++) {
		vms_vector v0 = rng_vec(),v1 = rng_vec(),f = rng_vec();
		int usef = rng_next() >> 31;
		fixang a;

		if (skip_case(&skip,i))
			continue;
		a = vm_vec_delta_ang(&v0,&v1,usef ? &f : NULL);
		sum_add((ushort) a);
		sum_vec(&v0);
		sum_vec(&v1);
		sum_add((ushort) vm_vec_delta_ang_norm(&v0,&v1,usef ? &f : NULL));
	}
	CHECK_EQ(sum,SUM_DELTA_ANG);

	stream_start(0x2008);
	skip = SKIP_ANG_2_MATRIX;
	for (i=0;i<N_ANG_2_MATRIX;i++) {
		vms_vector v;
		vms_matrix m = sent_mat;
		fixang a;

		v.x = rng_shift(15); v.y = rng_shift(16); v.z = rng_shift(15);
		a = rng_ang();

		if (skip_case(&skip,i))
			continue;
		vm_vec_ang_2_matrix(&m,&v,a);
		sum_mat(&m);
	}
	CHECK_EQ(sum,SUM_ANG_2_MATRIX);

	stream_start(0x2009);
	skip = SKIP_EXTRACT_VEC;
	for (i=0;i<N_EXTRACT_VEC;i++) {
		vms_vector v = rng_vec();
		vms_angvec a = sent_ang;

		if (skip_case(&skip,i))
			continue;
		vm_extract_angles_vector(&a,&v);
		sum_ang(&a);
	}
	CHECK_EQ(sum,SUM_EXTRACT_VEC);

	stream_start(0x200a);
	skip = SKIP_PLANE;
	for (i=0;i<N_PLANE;i++) {
		vms_vector c = rng_vec(),n = rng_vec(),p = rng_vec();

		if (skip_case(&skip,i))
			continue;
		sum_add(vm_dist_to_plane(&c,&n,&p));
	}
	CHECK_EQ(sum,SUM_PLANE);

	stream_start(0x200b);
	skip = SKIP_SCALE2;
	for (i=0;i<N_SCALE2;i++) {
		vms_vector v = rng_vec();
		fix n = rng_shift(16);
		fix d = (fix) (rng_next() | 0x10000);

		if (skip_case(&skip,i))
			continue;
		vm_vec_scale2(&v,n,d);
		sum_vec(&v);
	}
	CHECK_EQ(sum,SUM_SCALE2);
}

int main(void)
{
	test_simple();
	test_mag();
	test_dot_cross();
	test_matrix();
	test_angles();
	test_streams();

	printf("test_vecmat: %d checks, %d failures\n",n_checks,n_fails);

	return n_fails ? 1 : 0;
}
