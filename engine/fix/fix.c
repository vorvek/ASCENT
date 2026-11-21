/*
 * ASCENT: C replacement for fix.asm (fixed-point routines).
 *
 * Each function follows the assembly in fix.asm step by step, so that the
 * results are the same bit for bit. The tables are the tables of fix.asm.
 * fix.asm stays in the tree as the reference. It is not built.
 *
 * Differences from the assembly:
 *  - fix_asin() and fix_acos() with the input 0x80000000: the assembly
 *    computes a negative table index and reads memory before the table.
 *    Here the input saturates at 1.0, the same as each other large input.
 *  - fix_atan2() uses fixdiv() from fix.h. A divide overflow was a processor
 *    exception. Here the quotient saturates. The divisor is the magnitude of
 *    the two parameters, so the quotient can not overflow for any input
 *    that did not fault in the assembly.
 */

#include "fix.h"

//sine table: 256 entries for the circle, 64 more for the cosine, and one
//more for the interpolation.  The values are 2.14 fixed point.
static const int16_t sin_table[321] = {
	0,402,804,1205,1606,2006,2404,2801,
	3196,3590,3981,4370,4756,5139,5520,5897,
	6270,6639,7005,7366,7723,8076,8423,8765,
	9102,9434,9760,10080,10394,10702,11003,11297,
	11585,11866,12140,12406,12665,12916,13160,13395,
	13623,13842,14053,14256,14449,14635,14811,14978,
	15137,15286,15426,15557,15679,15791,15893,15986,
	16069,16143,16207,16261,16305,16340,16364,16379,
	16384,16379,16364,16340,16305,16261,16207,16143,
	16069,15986,15893,15791,15679,15557,15426,15286,
	15137,14978,14811,14635,14449,14256,14053,13842,
	13623,13395,13160,12916,12665,12406,12140,11866,
	11585,11297,11003,10702,10394,10080,9760,9434,
	9102,8765,8423,8076,7723,7366,7005,6639,
	6270,5897,5520,5139,4756,4370,3981,3590,
	3196,2801,2404,2006,1606,1205,804,402,
	0,-402,-804,-1205,-1606,-2006,-2404,-2801,
	-3196,-3590,-3981,-4370,-4756,-5139,-5520,-5897,
	-6270,-6639,-7005,-7366,-7723,-8076,-8423,-8765,
	-9102,-9434,-9760,-10080,-10394,-10702,-11003,-11297,
	-11585,-11866,-12140,-12406,-12665,-12916,-13160,-13395,
	-13623,-13842,-14053,-14256,-14449,-14635,-14811,-14978,
	-15137,-15286,-15426,-15557,-15679,-15791,-15893,-15986,
	-16069,-16143,-16207,-16261,-16305,-16340,-16364,-16379,
	-16384,-16379,-16364,-16340,-16305,-16261,-16207,-16143,
	-16069,-15986,-15893,-15791,-15679,-15557,-15426,-15286,
	-15137,-14978,-14811,-14635,-14449,-14256,-14053,-13842,
	-13623,-13395,-13160,-12916,-12665,-12406,-12140,-11866,
	-11585,-11297,-11003,-10702,-10394,-10080,-9760,-9434,
	-9102,-8765,-8423,-8076,-7723,-7366,-7005,-6639,
	-6270,-5897,-5520,-5139,-4756,-4370,-3981,-3590,
	-3196,-2801,-2404,-2006,-1606,-1205,-804,-402,
	0,402,804,1205,1606,2006,2404,2801,
	3196,3590,3981,4370,4756,5139,5520,5897,
	6270,6639,7005,7366,7723,8076,8423,8765,
	9102,9434,9760,10080,10394,10702,11003,11297,
	11585,11866,12140,12406,12665,12916,13160,13395,
	13623,13842,14053,14256,14449,14635,14811,14978,
	15137,15286,15426,15557,15679,15791,15893,15986,
	16069,16143,16207,16261,16305,16340,16364,16379,
	16384
};

#define cos_table (sin_table+64)

static const int16_t asin_table[258] = {
	0,41,81,122,163,204,244,285,
	326,367,408,448,489,530,571,612,
	652,693,734,775,816,857,897,938,
	979,1020,1061,1102,1143,1184,1225,1266,
	1307,1348,1389,1431,1472,1513,1554,1595,
	1636,1678,1719,1760,1802,1843,1884,1926,
	1967,2009,2050,2092,2134,2175,2217,2259,
	2300,2342,2384,2426,2468,2510,2551,2593,
	2636,2678,2720,2762,2804,2847,2889,2931,
	2974,3016,3059,3101,3144,3187,3229,3272,
	3315,3358,3401,3444,3487,3530,3573,3617,
	3660,3704,3747,3791,3834,3878,3922,3965,
	4009,4053,4097,4142,4186,4230,4275,4319,
	4364,4408,4453,4498,4543,4588,4633,4678,
	4723,4768,4814,4859,4905,4951,4997,5043,
	5089,5135,5181,5228,5274,5321,5367,5414,
	5461,5508,5556,5603,5651,5698,5746,5794,
	5842,5890,5938,5987,6035,6084,6133,6182,
	6231,6281,6330,6380,6430,6480,6530,6580,
	6631,6681,6732,6783,6835,6886,6938,6990,
	7042,7094,7147,7199,7252,7306,7359,7413,
	7466,7521,7575,7630,7684,7740,7795,7851,
	7907,7963,8019,8076,8133,8191,8249,8307,
	8365,8424,8483,8543,8602,8663,8723,8784,
	8846,8907,8970,9032,9095,9159,9223,9288,
	9353,9418,9484,9551,9618,9686,9754,9823,
	9892,9963,10034,10105,10177,10251,10324,10399,
	10475,10551,10628,10706,10785,10866,10947,11029,
	11113,11198,11284,11371,11460,11550,11642,11736,
	11831,11929,12028,12130,12234,12340,12449,12561,
	12677,12796,12919,13046,13178,13315,13459,13610,
	13770,13939,14121,14319,14538,14786,15079,15462,
	16384,16384
};

static const int16_t acos_table[258] = {
	16384,16343,16303,16262,16221,16180,16140,16099,
	16058,16017,15976,15936,15895,15854,15813,15772,
	15732,15691,15650,15609,15568,15527,15487,15446,
	15405,15364,15323,15282,15241,15200,15159,15118,
	15077,15036,14995,14953,14912,14871,14830,14789,
	14748,14706,14665,14624,14582,14541,14500,14458,
	14417,14375,14334,14292,14250,14209,14167,14125,
	14084,14042,14000,13958,13916,13874,13833,13791,
	13748,13706,13664,13622,13580,13537,13495,13453,
	13410,13368,13325,13283,13240,13197,13155,13112,
	13069,13026,12983,12940,12897,12854,12811,12767,
	12724,12680,12637,12593,12550,12506,12462,12419,
	12375,12331,12287,12242,12198,12154,12109,12065,
	12020,11976,11931,11886,11841,11796,11751,11706,
	11661,11616,11570,11525,11479,11433,11387,11341,
	11295,11249,11203,11156,11110,11063,11017,10970,
	10923,10876,10828,10781,10733,10686,10638,10590,
	10542,10494,10446,10397,10349,10300,10251,10202,
	10153,10103,10054,10004,9954,9904,9854,9804,
	9753,9703,9652,9601,9549,9498,9446,9394,
	9342,9290,9237,9185,9132,9078,9025,8971,
	8918,8863,8809,8754,8700,8644,8589,8533,
	8477,8421,8365,8308,8251,8193,8135,8077,
	8019,7960,7901,7841,7782,7721,7661,7600,
	7538,7477,7414,7352,7289,7225,7161,7096,
	7031,6966,6900,6833,6766,6698,6630,6561,
	6492,6421,6350,6279,6207,6133,6060,5985,
	5909,5833,5756,5678,5599,5518,5437,5355,
	5271,5186,5100,5013,4924,4834,4742,4648,
	4553,4455,4356,4254,4150,4044,3935,3823,
	3707,3588,3465,3338,3206,3069,2925,2774,
	2614,2445,2263,2065,1846,1598,1305,922,
	0,0
};

//values for first guess in square root routines.  Note that the first entry
//is useful in quad_sqrt when edx=0 and high bit of eax is set.
static const ubyte guess_table[256] = {
	1,1,1,1,2,2,2,2,2,3,3,3,3,3,3,3,
	4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,
	5,5,5,5,6,6,6,6,6,6,6,6,6,6,6,6,
	6,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,
	10,10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,
	11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
	12,12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,
	13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
	13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,
	14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
	14,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
	15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15
};

//no interpolation. takes the low 16 bits of a as the angle.
//either (or both) pointers can be null
void fix_fastsincos(fix a,fix *s,fix *c)
{
	int i = (a >> 8) & 0xff;	//get high byte

	if (s)
		*s = sin_table[i] * 4;	//make a fix
	if (c)
		*c = cos_table[i] * 4;
}

//table[i] plus the fraction (0..255) of the step to table[i+1].
//the shift is an arithmetic shift (sar), so a negative step rounds down.
static int32_t table_interp(const int16_t *table,int i,int frac)
{
	int32_t a = table[i];
	int32_t b = table[i+1];

	return a + (((b - a) * frac) >> 8);
}

//with interpolation. takes the low 16 bits of a as the angle.
//either (or both) pointers can be null
void fix_sincos(fix a,fix *s,fix *c)
{
	int i = (a >> 8) & 0xff;	//get high byte
	int frac = a & 0xff;		//low byte

	if (s)
		*s = table_interp(sin_table,i,frac) * 4;	//make a fix
	if (c)
		*c = table_interp(cos_table,i,frac) * 4;
}

//shared part of fix_asin() & fix_acos(): take the absolute value, saturate
//at 1.0, and look up the table. *sign is all ones if v is negative.
static int32_t arc_lookup(const int16_t *table,fix v,uint32_t *sign)
{
	uint32_t u = (uint32_t) v;
	int32_t t;

	*sign = (v < 0) ? 0xffffffffu : 0;
	u = (u ^ *sign) - *sign;	//get abs value
	t = (int32_t) u;

	// ASCENT: t < 0 is the input 0x80000000. See the top of this file.
	if (t > 0x10000 || t < 0)
		t = 0x10000;

	return table_interp(table,t >> 8,t & 0xff);
}

//takes cos angle, returns angle
fixang fix_acos(fix v)
{
	uint32_t sign;
	uint32_t a = (uint32_t) arc_lookup(acos_table,v,&sign);

	a = (a ^ sign) - sign;	//make correct sign
	a += sign & 0x8000;		//zero or 1/2

	return (fixang) a;
}

//takes sin angle, returns angle
fixang fix_asin(fix v)
{
	uint32_t sign;
	uint32_t a = (uint32_t) arc_lookup(asin_table,v,&sign);

	a = (a ^ sign) - sign;	//make sign correct

	return (fixang) a;
}

//standard Newtonian-iteration square root routine.
//The assembly does a fixed number of iterations with no test (two for a
//long, three for a quad), then iterates until the guess is correct or stops
//changing. The order of the tests and the round up on a remainder are the
//same as in the assembly.
//
//The quotient of each divide fits the register (16 bits for a long, 32 bits
//for a quad): the first guess is not less than a quarter of the root, and
//each later guess is not less than the root rounded down.
ushort long_sqrt(int32_t a)
{
	uint32_t n = (uint32_t) a;
	uint32_t bx,ax,dx,di;
	int i;

	if (a <= 0)		//zero or negative. return zero
		return 0;

	//get a good first quess by checking which byte most significant bit is in
	if (n >> 24)
		bx = (uint32_t) guess_table[n >> 24] << 12;
	else if (n >> 16)
		bx = (uint32_t) guess_table[n >> 16] << 8;
	else if (n >> 8)
		bx = (uint32_t) guess_table[n >> 8] << 4;
	else
		bx = guess_table[n];

	for (i=0;i<2;i++) {
		ax = (n / bx) & 0xffff;
		bx = (bx + ax) >> 1;	//next guess = (d + q)/2
	}

	for (;;) {
		ax = (n / bx) & 0xffff;
		dx = n % bx;
		if (ax == bx)		//correct?
			return (ushort) ax;
		di = bx;				//save for compare
		bx = (bx + ax) >> 1;	//next guess = (d + q)/2
		if (bx == ax || bx == di)
			break;
	}

	if (dx)		//check remainder
		bx++;

	return (ushort) bx;
}

//takes the quad high:low, returns a long
uint32_t quad_sqrt(int32_t low,int32_t high)
{
	uint64_t n = ((uint64_t) (uint32_t) high << 32) | (uint32_t) low;
	uint32_t h = (uint32_t) high;
	uint64_t bx,ax,dx,di;
	int i;

	if (high < 0)		//can't do negative number!
		return 0;

	if (high == 0 && low >= 0)	//we can use longword version
		return long_sqrt(low);

	//get a good first quess by checking which byte most significant bit is in
	if (h >> 24)
		bx = (uint64_t) guess_table[h >> 24] << 28;
	else if (h >> 16)
		bx = (uint64_t) guess_table[h >> 16] << 24;
	else if (h >> 8)
		bx = (uint64_t) guess_table[h >> 8] << 20;
	else
		bx = (uint64_t) guess_table[h] << 16;

	//quad loop usually executes 4 times
	for (i=0;i<3;i++) {
		ax = (n / bx) & 0xffffffffu;
		bx = (bx + ax) >> 1;	//next guess = (d + q)/2
	}

	for (;;) {
		ax = (n / bx) & 0xffffffffu;
		dx = n % bx;
		if (ax == bx)		//correct?
			return (uint32_t) ax;
		di = bx;				//save for compare
		bx = (bx + ax) >> 1;	//next guess = (d + q)/2
		if (bx == ax || bx == di)
			break;
	}

	if (dx)		//check remainder
		bx++;

	return (uint32_t) bx;
}

//fixed-point square root
fix fix_sqrt(fix a)
{
	return (fix) long_sqrt(a) << 8;
}

//given cos & sin of an angle, return that angle.
//parms need not be normalized, that is, the ratio of the parms cos/sin must
//equal the ratio of the actual cos & sin for the result angle, but the parms
//need not be the actual cos & sin.
//NOTE: this is different from the standard C atan2, since it is left-handed.
//uses either asin or acos, to get better precision
fixang fix_atan2(fix cos,fix sin)
{
	uint32_t acos_v = (uint32_t) cos, asin_v = (uint32_t) sin;
	uint64_t q;
	fix mag;
	int32_t a;

	//the absolute values. 0x80000000 stays negative, as in the assembly
	if (cos < 0) acos_v = 0 - acos_v;
	if (sin < 0) asin_v = 0 - asin_v;

	q = (uint64_t) ((int64_t) cos * cos) + (uint64_t) ((int64_t) sin * sin);

	//the assembly divides by the magnitude with idiv, so a magnitude of
	//0x80000000 or more is a negative divisor.
	mag = (fix) quad_sqrt((int32_t) (uint32_t) q,(int32_t) (q >> 32));

	if ((int32_t) acos_v < (int32_t) asin_v) {

		//cos is smaller, use arccos

		a = fix_acos(fixdiv(cos,mag));
		if (sin < 0)
			a = -a;
	}
	else {

		//sin is smaller, use arcsin

		if (mag == 0)		//abort! the assembly returns the low word of sin
			return (fixang) sin;

		a = fix_asin(fixdiv(sin,mag));
		if (cos < 0)
			a = 0x8000 - a;
	}

	return (fixang) a;
}
