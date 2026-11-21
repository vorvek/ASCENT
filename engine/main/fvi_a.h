/*
 * ASCENT: C version of the inline assembly helpers of fvi.c.
 *
 * The original helpers were Watcom "#pragma aux" routines inside fvi.c.
 * This file replaces them. The assembly text stays in fvi.c as a comment.
 */

#ifndef _FVI_A_H
#define _FVI_A_H

#include <stdint.h>

#include "fix.h"

//returns true if the fixed point product of a and b is too large.
//
//The assembly:
//
//   cdq / xor eax,edx / sub eax,edx      eax = abs(a)
//   xchg eax,ebx
//   cdq / xor eax,edx / sub eax,edx      eax = abs(b)
//   imul ebx                             edx:eax = abs(b) * abs(a), signed
//   sar  edx,15
//   or   dx,dx                           test the low 16 bits of edx
//   setnz al
//   movzx eax,al
//
//The result is 1 when one of the bits 47 to 62 of the 64-bit product is set.
//Bit 63 is not part of the test. abs() of 0x80000000 stays 0x80000000 (a
//negative number), as in the assembly, so the product can be negative.
static inline int oflow_check(fix a,fix b)
{
	uint32_t ua,ub,sa,sb;
	int32_t hi;
	int64_t prod;

	sa = (uint32_t) (a >> 31);			//cdq: all ones if a is negative
	ua = ((uint32_t) a ^ sa) - sa;		//xor eax,edx / sub eax,edx

	sb = (uint32_t) (b >> 31);
	ub = ((uint32_t) b ^ sb) - sb;

	prod = (int64_t) (int32_t) ub * (int64_t) (int32_t) ua;		//imul ebx

	hi = (int32_t) (prod >> 32);		//edx

	return ((hi >> 15) & 0xffff) != 0;	//sar edx,15 / or dx,dx / setnz al
}

#endif
