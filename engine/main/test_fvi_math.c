/*
 * ASCENT: test of the C versions of the inline assembly helpers of fvi.c
 * (see fvi_a.h). The expected values are derived by hand from the assembly.
 */

#include <stdio.h>
#include <stdint.h>

#include "fix.h"
#include "fvi_a.h"

static int failures;

static void check(fix a, fix b, int expect)
{
	int got = oflow_check(a, b);

	if (got != expect) {
		printf("FAIL: oflow_check(0x%08x,0x%08x) = %d, expected %d\n",
			(unsigned) a, (unsigned) b, got, expect);
		failures++;
	}
}

int main(void)
{
	fix int_min = (fix) 0x80000000u;

	//The routine computes edx:eax = abs(a) * abs(b) and returns 1 when
	//one of the bits 47 to 62 of the product is set (sar edx,15 / or dx,dx).

	check(0, 0, 0);
	check(0x10000, 0x10000, 0);				//2^32: edx = 1
	check(1, 0x7fffffff, 0);

	//2^23 * 2^24 = 2^47: edx = 0x8000, the first value that overflows
	check(0x00800000, 0x01000000, 1);
	check(0x01000000, 0x00800000, 1);

	//(2^23 - 1) * 2^24 = 2^47 - 2^24: edx = 0x7fff
	check(0x007fffff, 0x01000000, 0);

	//0x10000 * 0x7fffffff = 0x7fffffff0000: edx = 0x7fff
	check(0x10000, 0x7fffffff, 0);
	//0x10001 * 0x7fffffff = 0x80007ffeffff: edx = 0x8000
	check(0x10001, 0x7fffffff, 1);

	//the signs do not matter
	check(-0x00800000, 0x01000000, 1);
	check(0x00800000, -0x01000000, 1);
	check(-0x00800000, -0x01000000, 1);
	check(-0x007fffff, 0x01000000, 0);
	check(-0x10000, -0x10000, 0);

	//0x7fffffff squared = 0x3fffffff00000001: edx = 0x3fffffff, dx after the shift = 0x7fff
	check(0x7fffffff, 0x7fffffff, 1);

	//2^30 * 2^17 = 2^47
	check(0x40000000, 0x00020000, 1);
	//2^30 * (2^17 - 1) = 2^47 - 2^30: edx = 0x7fff
	check(0x40000000, 0x0001ffff, 0);

	//abs(0x80000000) stays 0x80000000, a negative number.
	//-2^31 * 1 = 0xffffffff80000000: edx = 0xffffffff, dx after the shift = 0xffff
	check(int_min, 1, 1);
	check(1, int_min, 1);
	//-2^31 * 0 = 0
	check(int_min, 0, 0);
	check(0, int_min, 0);
	//-2^31 * -2^31 = 2^62: edx = 0x40000000, dx after the shift = 0x8000
	check(int_min, int_min, 1);
	//-2^31 * 0x10000 = -2^47: edx = 0xffff8000, dx after the shift = 0xffff
	check(int_min, 0x10000, 1);

	if (failures) {
		printf("test_fvi_math: %d failures\n", failures);
		return 1;
	}

	printf("test_fvi_math: all tests passed\n");
	return 0;
}
