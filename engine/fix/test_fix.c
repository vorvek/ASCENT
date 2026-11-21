/*
 * Test for fix.c, the C replacement for fix.asm.
 *
 * Three kinds of checks:
 *  1. Exact values that were derived by hand from the assembly.
 *  2. Exact values and checksums from test_fix_data.h. asm_model.py, an
 *     integer model of fix.asm, writes that file. The checksum streams use
 *     the same random number generator here and in the model.
 *  3. Loose checks against floating point math.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "fix.h"
#include "test_fix_data.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

static void sum_add(uint32_t w)
{
	sum = (sum ^ w) * 16777619u;
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

static double ang_to_rad(double a)
{
	return a * 2.0 * M_PI / 65536.0;
}

//difference of two angles in units of 1/65536 of a circle, from -32768 to 32767
static int ang_diff(int a,int b)
{
	return (int) (int16_t) (uint16_t) (a - b);
}

static void test_sincos(void)
{
	fix s,c,fs,fc;
	int i,a;
	double max_err = 0;

	//by hand: table entries 0, 64, 128, 192 times 4
	fix_sincos(0,&s,&c);			CHECK_EQ(s,0);			CHECK_EQ(c,65536);
	fix_sincos(0x4000,&s,&c);	CHECK_EQ(s,65536);	CHECK_EQ(c,0);
	fix_sincos(0x8000,&s,&c);	CHECK_EQ(s,0);			CHECK_EQ(c,-65536);
	fix_sincos(0xc000,&s,&c);	CHECK_EQ(s,-65536);	CHECK_EQ(c,0);

	//by hand: sin = (0 + ((402-0)*1 >> 8)) * 4 = 1*4
	//cos = (16384 + ((16379-16384)*1 >> 8)) * 4 = (16384 - 1) * 4, since sar rounds -5/256 down to -1
	fix_sincos(1,&s,&c);			CHECK_EQ(s,4);			CHECK_EQ(c,65532);

	//by hand: sin = (16384 + (-5 >> 8)) * 4 = 65532
	//cos = (0 + ((-402-0)*1 >> 8)) * 4 = -2 * 4, since sar rounds -402/256 down to -2
	fix_sincos(0x4001,&s,&c);	CHECK_EQ(s,65532);	CHECK_EQ(c,-8);

	//by hand: sin = (402 + ((804-402)*128 >> 8)) * 4 = (402+201)*4
	fix_sincos(0x0180,&s,&c);	CHECK_EQ(s,2412);

	//no interpolation: the low byte has no effect
	fix_fastsincos(0x40ff,&s,&c);	CHECK_EQ(s,65536);	CHECK_EQ(c,0);
	fix_fastsincos(0x01ff,&s,&c);	CHECK_EQ(s,402*4);	CHECK_EQ(c,16379*4);

	//angle wrap: only the low 16 bits of the angle count
	for (a=-0x28000;a<=0x28000;a+=0x777) {
		fix_sincos(a,&s,&c);
		fix_sincos(a & 0xffff,&fs,&fc);
		CHECK_EQ(s,fs);
		CHECK_EQ(c,fc);
		fix_fastsincos(a,&s,&c);
		fix_fastsincos(a & 0xffff,&fs,&fc);
		CHECK_EQ(s,fs);
		CHECK_EQ(c,fc);
	}
	fix_sincos((fixang) 0xc000,&s,&c);	CHECK_EQ(s,-65536);	CHECK_EQ(c,0);		//-0x4000

	//either (or both) pointers can be null
	s = c = 1234;
	fix_sincos(0x4000,NULL,NULL);
	fix_sincos(0x4000,&s,NULL);		CHECK_EQ(s,65536);
	fix_sincos(0x8000,NULL,&c);		CHECK_EQ(c,-65536);
	fix_fastsincos(0x4000,NULL,NULL);
	fix_fastsincos(0xc000,&s,NULL);	CHECK_EQ(s,-65536);
	fix_fastsincos(0,NULL,&c);			CHECK_EQ(c,65536);

	//model: table of cases
	for (i=0;i<COUNT(sincos_cases);i++) {
		fix_sincos(sincos_cases[i].a,&s,&c);
		fix_fastsincos(sincos_cases[i].a,&fs,&fc);
		CHECK_EQ(s,(fix) sincos_cases[i].s);
		CHECK_EQ(c,(fix) sincos_cases[i].c);
		CHECK_EQ(fs,(fix) sincos_cases[i].fs);
		CHECK_EQ(fc,(fix) sincos_cases[i].fc);
	}

	//model: each angle. floating point: each angle
	stream_start(0);
	for (a=0;a<N_SINCOS;a++) {
		double e;

		fix_sincos(a,&s,&c);
		fix_fastsincos(a,&fs,&fc);
		sum_add(s); sum_add(c); sum_add(fs); sum_add(fc);

		e = fabs(s - sin(ang_to_rad(a)) * 65536.0);
		if (e > max_err) max_err = e;
		e = fabs(c - cos(ang_to_rad(a)) * 65536.0);
		if (e > max_err) max_err = e;
	}
	CHECK_EQ(sum,SUM_SINCOS);

	//the table has 14 bits of fraction (steps of 4), and the linear
	//interpolation between 256 entries is low by up to 5
	printf("sincos: max difference from floating point %.2f\n",max_err);
	CHECK(max_err < 12.0);
}

static void test_asin_acos(void)
{
	const int *skip;
	int i,v;
	double max_err_mid = 0,max_err_all = 0;

	//by hand: table entries 0 and 256
	CHECK_EQ(fix_asin(0),0);
	CHECK_EQ(fix_asin(f1_0),0x4000);
	CHECK_EQ(fix_asin(-f1_0),-0x4000);
	CHECK_EQ(fix_acos(0),0x4000);
	CHECK_EQ(fix_acos(f1_0),0);
	CHECK_EQ((ushort) fix_acos(-f1_0),0x8000);		//-0 + 1/2

	//by hand: acos(1) = 16384 + ((16343-16384)*1 >> 8) = 16384 - 1
	//acos(-1) = -16383 + 0x8000.  asin(1) = 0 + (41*1 >> 8) = 0
	CHECK_EQ(fix_acos(1),16383);
	CHECK_EQ(fix_acos(-1),16385);
	CHECK_EQ(fix_asin(1),0);
	CHECK_EQ(fix_asin(-1),0);

	//by hand: asin(0x80) = 0 + (41*128 >> 8) = 20
	CHECK_EQ(fix_asin(0x80),20);
	CHECK_EQ(fix_asin(-0x80),-20);

	//saturate at 1 instead of generate error
	CHECK_EQ(fix_asin(f1_0+1),0x4000);
	CHECK_EQ(fix_asin(0x7fffffff),0x4000);
	CHECK_EQ(fix_asin(-0x7fffffff),-0x4000);
	CHECK_EQ(fix_acos(0x7fffffff),0);
	CHECK_EQ((ushort) fix_acos(-0x7fffffff),0x8000);

	//port only: the assembly reads outside the table for 0x80000000
	CHECK_EQ(fix_asin(INT32_MIN),-0x4000);
	CHECK_EQ((ushort) fix_acos(INT32_MIN),0x8000);

	//model: table of cases
	for (i=0;i<COUNT(asincos_cases);i++) {
		CHECK_EQ((ushort) fix_asin(asincos_cases[i].v),asincos_cases[i].asin);
		CHECK_EQ((ushort) fix_acos(asincos_cases[i].v),asincos_cases[i].acos);
	}

	//model: each value from -1.0 to 1.0 and a short distance past the ends
	stream_start(0);
	for (v=-0x10400;v<=0x10400;v++) {
		fixang as = fix_asin(v),ac = fix_acos(v);

		sum_add((ushort) as);
		sum_add((ushort) ac);

		if (v >= -f1_0 && v <= f1_0) {
			double e,e2;

			e = fabs(as - asin(v / 65536.0) * 65536.0 / (2.0 * M_PI));
			e2 = fabs((ushort) ac - acos(v / 65536.0) * 65536.0 / (2.0 * M_PI));
			if (e2 > e) e = e2;
			if (e > max_err_all) max_err_all = e;
			if (abs(v) <= 0xe000 && e > max_err_mid) max_err_mid = e;
		}

		//asin + acos is a quarter circle (the tables are complements).
		//the two interpolations round down, so the sum can be off by one
		CHECK(abs((ushort) (as + ac) - 0x4000) <= 1);
	}
	CHECK_EQ(sum,SUM_ASINCOS);

	//the last table step (255/256 to 1) is from 15462 to 16384. the
	//interpolation is poor there, as in the original
	printf("asin/acos: max difference from floating point %.2f (%.2f for |v| <= 0.875)\n",
		max_err_all,max_err_mid);
	CHECK(max_err_mid < 3.0);
	CHECK(max_err_all < 400.0);

	//model: values with random magnitudes
	stream_start(0x1001);
	skip = SKIP_ASINCOS_RND;
	for (i=0;i<N_ASINCOS_RND;i++) {
		fix r = rng_fix();

		if (skip_case(&skip,i))
			continue;
		sum_add((ushort) fix_asin(r));
		sum_add((ushort) fix_acos(r));
	}
	CHECK_EQ(sum,SUM_ASINCOS_RND);
}

//true if r is the square root of n, within 1
static int near_root(uint64_t n,uint64_t r)
{
	uint64_t lo = (r > 0) ? (r-1)*(r-1) : 0;
	uint64_t hi = (r+1)*(r+1);		//r is less than 2^32 - 1

	return lo <= n && n <= hi;
}

static void test_sqrt(void)
{
	const int *skip;
	int i;

	//zero or negative. return zero
	CHECK_EQ(long_sqrt(0),0);
	CHECK_EQ(long_sqrt(-1),0);
	CHECK_EQ(long_sqrt(INT32_MIN),0);
	CHECK_EQ(fix_sqrt(0),0);
	CHECK_EQ(fix_sqrt(-f1_0),0);

	//by hand. n=1: guess 1, quotient 1, correct
	CHECK_EQ(long_sqrt(1),1);

	//by hand. n=2: guess 1. 2/1=2, guess (1+2)/2=1, two times. loop: 2/1=2
	//is not the guess, the new guess 1 is the old guess, remainder 0: 1
	CHECK_EQ(long_sqrt(2),1);

	//by hand. n=3: guess 1. 3/1=3, guess 2. 3/2=1, guess 1. loop: 3/1=3,
	//guess 2. 3/2=1 rem 1, guess 1, which is the quotient: 1, and the
	//remainder is not zero: 2.  The routine rounds up here.
	CHECK_EQ(long_sqrt(3),2);

	//by hand. n=4: guess 2 (from the table), quotient 2, correct
	CHECK_EQ(long_sqrt(4),2);
	CHECK_EQ(long_sqrt(16),4);

	//by hand. n=0x10000: guess 1<<8, quotient 256, correct. fix_sqrt shifts by 8
	CHECK_EQ(long_sqrt(0x10000),256);
	CHECK_EQ(fix_sqrt(f1_0),f1_0);
	CHECK_EQ(fix_sqrt(4*f1_0),2*f1_0);		//guess 2<<8
	CHECK_EQ(fix_sqrt(f1_0/4),f1_0/2);

	//by hand. quad 2^32: guess 1<<16, quotient 65536, correct
	CHECK_EQ(quad_sqrt(0,1),0x10000);

	//by hand. quad 2^62: guess 8<<28, quotient 2^31, correct
	CHECK_EQ(quad_sqrt(0,0x40000000),0x80000000u);

	//can't do negative number!
	CHECK_EQ(quad_sqrt(0,-1),0);
	CHECK_EQ(quad_sqrt(-1,-1),0);
	CHECK_EQ(quad_sqrt(0,INT32_MIN),0);

	//high longword zero: the longword version, if the high bit of the low longword is clear
	CHECK_EQ(quad_sqrt(0,0),0);
	CHECK_EQ(quad_sqrt(3,0),2);
	CHECK_EQ(quad_sqrt(0x10000,0),256);
	CHECK(near_root(0x80000000u,quad_sqrt((int32_t) 0x80000000u,0)));
	CHECK(near_root(0xffffffffu,quad_sqrt(-1,0)));

	//model: tables of cases
	for (i=0;i<COUNT(sqrt_cases);i++) {
		CHECK_EQ(long_sqrt(sqrt_cases[i].v),sqrt_cases[i].lsqrt);
		CHECK_EQ(fix_sqrt(sqrt_cases[i].v),sqrt_cases[i].fsqrt);
	}
	for (i=0;i<COUNT(quad_cases);i++)
		CHECK_EQ(quad_sqrt(quad_cases[i].lo,quad_cases[i].hi),quad_cases[i].r);

	//model: each value below 3.0
	stream_start(0);
	for (i=0;i<N_SQRT_LOW;i++) {
		ushort r = long_sqrt(i);

		sum_add(r);
		sum_add(fix_sqrt(i));
		CHECK(near_root(i,r) && fabs(r - sqrt(i)) <= 1.0);
	}
	CHECK_EQ(sum,SUM_SQRT_LOW);

	//model: values with random magnitudes
	stream_start(0x1002);
	skip = SKIP_SQRT_RND;
	for (i=0;i<N_SQRT_RND;i++) {
		fix v = rng_fix();
		ushort r;

		if (skip_case(&skip,i))
			continue;
		r = long_sqrt(v);
		sum_add(r);
		sum_add(fix_sqrt(v));
		CHECK(v <= 0 || (near_root(v,r) && fabs(r - sqrt(v)) <= 1.0));
	}
	CHECK_EQ(sum,SUM_SQRT_RND);

	stream_start(0x1003);
	skip = SKIP_QUAD_RND;
	for (i=0;i<N_QUAD_RND;i++) {
		uint32_t lo = rng_next();
		uint32_t hi = rng_next() >> 1;
		uint32_t r;
		uint64_t n;

		hi >>= rng_next() >> 27;
		if (skip_case(&skip,i))
			continue;
		r = quad_sqrt((int32_t) lo,(int32_t) hi);
		sum_add(r);
		n = ((uint64_t) hi << 32) | lo;
		CHECK(near_root(n,r) && fabs(r - sqrt((double) n)) <= 1.0);
	}
	CHECK_EQ(sum,SUM_QUAD_RND);
}

static void test_atan2(void)
{
	const int *skip;
	int i;
	int max_err = 0;

	//by hand. (1,0): arcsin of 0/65536 is 0, cos is not negative
	CHECK_EQ(fix_atan2(f1_0,0),0);
	//(0,1): cos is smaller, arccos of 0 is 0x4000, sin is not negative
	CHECK_EQ(fix_atan2(0,f1_0),0x4000);
	//(-1,0): arcsin 0, cos is negative: 0x8000 - 0
	CHECK_EQ((ushort) fix_atan2(-f1_0,0),0x8000);
	//(0,-1): arccos 0x4000, sin is negative
	CHECK_EQ(fix_atan2(0,-f1_0),-0x4000);
	//left-handed: sin is the first axis that a positive angle turns to
	CHECK(fix_atan2(f1_0,f1_0) > 0x1ff0 && fix_atan2(f1_0,f1_0) < 0x2010);
	CHECK(fix_atan2(f1_0,-f1_0) < -0x1ff0 && fix_atan2(f1_0,-f1_0) > -0x2010);

	//parms need not be normalized
	CHECK_EQ(fix_atan2(1,0),0);
	CHECK_EQ(fix_atan2(0,7),0x4000);
	CHECK_EQ(fix_atan2(0x7fffffff,0),0);
	CHECK_EQ(fix_atan2(0,0x7fffffff),0x4000);
	CHECK_EQ(fix_atan2(0,-0x7fffffff),-0x4000);
	CHECK_EQ((ushort) fix_atan2(-0x7fffffff,0),0x8000);

	//both zero: the magnitude is zero, the assembly returns the low word of sin
	CHECK_EQ(fix_atan2(0,0),0);

	//model: table of cases
	for (i=0;i<COUNT(atan2_cases);i++)
		CHECK_EQ((ushort) fix_atan2(atan2_cases[i].c,atan2_cases[i].s),atan2_cases[i].r);

	//model: values with random magnitudes
	stream_start(0x1004);
	skip = SKIP_ATAN2_RND;
	for (i=0;i<N_ATAN2_RND;i++) {
		fix c = rng_fix();
		fix s = rng_fix();
		fixang a;

		if (skip_case(&skip,i))
			continue;
		a = fix_atan2(c,s);
		sum_add((ushort) a);

		//floating point, for values that are large enough for the quotient
		//to have some bits, and small enough for the magnitude to be less
		//than 2^31 (a larger magnitude is a negative divisor, as in the
		//assembly, and the sum of the squares can overflow)
		if ((llabs((long long) c) >= 0x1000 || llabs((long long) s) >= 0x1000) &&
				llabs((long long) c) < 0x40000000 && llabs((long long) s) < 0x40000000) {
			int want = (int) floor(atan2((double) s,(double) c) * 65536.0 / (2.0 * M_PI) + 0.5);
			int e = abs(ang_diff(a,want));

			if (e > max_err) max_err = e;
		}
	}
	CHECK_EQ(sum,SUM_ATAN2_RND);

	printf("atan2: max difference from floating point %d\n",max_err);
	CHECK(max_err <= 16);
}

static void test_fixdiv(void)
{
	//fix.h: the multiply shifts with no rounding (shrd), the divide
	//truncates toward zero (idiv)
	CHECK_EQ(fixmul(f1_0,f1_0),f1_0);
	CHECK_EQ(fixmul(-1,1),-1);			//-1 >> 16 is -1
	CHECK_EQ(fixmul(1,1),0);
	CHECK_EQ(fixmul(-f1_0,3),-3);
	CHECK_EQ(fixdiv(f1_0,3*f1_0),0x5555);
	CHECK_EQ(fixdiv(-f1_0,3*f1_0),-0x5555);	//toward zero
	CHECK_EQ(fixmuldiv(7,-3,2),-10);				//toward zero

	//port only: the processor exceptions saturate
	CHECK_EQ(fixdiv(f1_0,0),INT32_MAX);
	CHECK_EQ(fixdiv(-f1_0,0),INT32_MIN);
	CHECK_EQ(fixdiv(0x7fffffff,1),INT32_MAX);
	CHECK_EQ(fixdiv(-0x7fffffff,1),INT32_MIN);
}

int main(void)
{
	test_sincos();
	test_asin_acos();
	test_sqrt();
	test_atan2();
	test_fixdiv();

	printf("test_fix: %d checks, %d failures\n",n_checks,n_fails);

	return n_fails ? 1 : 0;
}
