/*
 * ASCENT: internal header of the 3d library. It replaces the data and
 * routine declarations of 3d.inc for the C files, and the divcheck macro.
 * Only the files of the 3d library include it.
 */

#ifndef _GLOBVARS_H
#define _GLOBVARS_H

#include "fix.h"
#include "vecmat.h"
#include "gr.h"
#include "3d.h"

#define MAX_POINTS_IN_POLY 100

//Global variables (globvars.c)

extern vms_vector View_position;
extern fix View_zoom;

extern vms_matrix Unscaled_matrix;	//before scaling
extern vms_matrix View_matrix;

extern vms_vector Window_scale;		//scaling for window aspect
extern vms_vector Matrix_scale;		//how the matrix is scaled, window_scale * zoom

extern int Canvas_width;				//the actual width
extern int Canvas_height;				//the actual height

extern fix Canv_w2;						//fixed-point width/2
extern fix Canv_h2;						//fixed-point height/2

//vertex buffers for polygon drawing and clipping
extern g3s_point *Vbuf0[MAX_POINTS_IN_POLY];
extern g3s_point *Vbuf1[MAX_POINTS_IN_POLY];

//list of 2d coords
extern fix Vertex_list[MAX_POINTS_IN_POLY*2];

//from rod.c: the 2d corners of the last bitmap that g3_draw_bitmap() drew
extern fix blob_vertices[8];

//The divcheck macro of 3d.inc. The assembly checks for overflow before
//each projection divide: it takes the absolute value of the high dword of
//the numerator, shifts it left one bit together with the top bit of the
//low dword, and compares the result (unsigned) with the denominator.
//Returns true if the divide must not be done.
static inline int g3_div_overflow(int64_t num,fix den)
{
	uint32_t hi = (uint32_t) ((uint64_t) num >> 32);
	uint32_t lo = (uint32_t) num;

	if ((int32_t) hi < 0)
		hi = 0u - hi;

	hi = (hi << 1) | (lo >> 31);		//shld ecx,eax,1

	return hi >= (uint32_t) den;
}

//The idiv instruction: 64-bit numerator, 32-bit denominator, the quotient
//truncates toward zero.
//ASCENT: a zero denominator or a quotient that does not fit in 32 bits was
//a processor exception (divide overflow) in the assembly. Here the result
//saturates. The result is the same in each case where the assembly did
//not fault.
static inline fix g3_idiv(int64_t num,fix den)
{
	int64_t q;

	if (den == 0)
		return (num < 0) ? INT32_MIN : INT32_MAX;

	if (num == INT64_MIN && den == -1)
		return INT32_MAX;

	q = num / den;

	if (q > INT32_MAX) return INT32_MAX;
	if (q < INT32_MIN) return INT32_MIN;

	return (fix) q;
}

//negate with wrap, as the neg instruction does
static inline fix g3_neg(fix a)
{
	return (fix) (0u - (uint32_t) a);
}

//the abs_eax macro (cdq, xor, sub). The most negative value stays negative
static inline fix g3_abs(fix a)
{
	return (a < 0) ? g3_neg(a) : a;
}

#endif
