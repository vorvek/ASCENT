/*
 * ASCENT: C replacement for vecmat.asm (vector and matrix library).
 *
 * Each function follows the assembly in vecmat.asm step by step, so that the
 * results are the same bit for bit: the 64-bit sums of the products, the
 * shift by 16 with no rounding (shrd), the arithmetic shifts (sar), and the
 * divides that truncate toward zero (idiv). vecmat.asm stays in the tree as
 * the reference. It is not built.
 *
 * Differences from the assembly:
 *  - A divide by zero or a quotient overflow was a processor exception.
 *    Here fixdiv() and fixmuldiv() saturate (see fix.h). This can occur in
 *    vm_vec_scale2(), vm_vec_ang_2_matrix() (forward vector straight up or
 *    down, or not normalized), vm_extract_angles_matrix() (matrix that is
 *    not a rotation matrix), and in the normalize functions that use the
 *    approximate magnitude when a component is 0x80000000.
 *  - vm_vec_scale2() with d == 0: the assembly jumps over the pops and
 *    returns with a bad stack. Here the function returns with the vector
 *    not changed, which is what the check in the assembly was for.
 *  - vm_vec_perp() and vm_vec_normal(): when a component of an edge vector
 *    is 0x80000000, the shift loop in check_vec does not end in the
 *    assembly (sar keeps the sign bit). Here the loop uses a logical shift
 *    and ends. The result is the same for each other input.
 *  - The debug breaks (int 3) for dest==src and for overflow in the cross
 *    product are not here. The release build of the assembly did not have
 *    them.
 *  - The temporary vectors are local variables, not static data.
 *  - vm_vec_avg4() is here. The assembly had it, but did not assemble it
 *    (AVG4_ENABLED = 0). vm_vector_2_matrix_norm() was not assembled
 *    (V2MN_ENABLED = 0) and is not here.
 */

#include "fix.h"
#include "vecmat.h"

//These should never be changed!
vms_vector vmd_zero_vector = {0,0,0};

vms_matrix vmd_identity_matrix = {	{f1_0,0,0},
												{0,f1_0,0},
												{0,0,f1_0} };

//The names of the matrix elements in the assembly.
//Ordering is across then down, so <m1,m2,m3> is the first row
#define M1(m) ((m)->rvec.x)
#define M4(m) ((m)->rvec.y)
#define M7(m) ((m)->rvec.z)
#define M2(m) ((m)->uvec.x)
#define M5(m) ((m)->uvec.y)
#define M8(m) ((m)->uvec.z)
#define M3(m) ((m)->fvec.x)
#define M6(m) ((m)->fvec.y)
#define M9(m) ((m)->fvec.z)

//imul: the 64-bit signed product, as an unsigned value so that sums wrap
static inline uint64_t mul64(fix a,fix b)
{
	return (uint64_t) ((int64_t) a * b);
}

//shrd eax,edx,16: the low 32 bits of the quad shifted right by 16
static inline fix quad_to_fix(uint64_t q)
{
	return (fix) (uint32_t) (q >> 16);
}

//square root of a quad. the result is an unsigned long in the assembly
static inline fix quad_mag(uint64_t q)
{
	return (fix) quad_sqrt((int32_t) (uint32_t) q,(int32_t) (q >> 32));
}

//adds two vectors, fills in dest, returns ptr to dest
//ok for dest to equal either source, but should use vm_vec_add2() if so
vms_vector *vm_vec_add(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = (fix) ((uint32_t) src0->x + (uint32_t) src1->x);
	dest->y = (fix) ((uint32_t) src0->y + (uint32_t) src1->y);
	dest->z = (fix) ((uint32_t) src0->z + (uint32_t) src1->z);

	return dest;
}

//subs two vectors, fills in dest, returns ptr to dest
//ok for dest to equal either source, but should use vm_vec_sub2() if so
vms_vector *vm_vec_sub(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = (fix) ((uint32_t) src0->x - (uint32_t) src1->x);
	dest->y = (fix) ((uint32_t) src0->y - (uint32_t) src1->y);
	dest->z = (fix) ((uint32_t) src0->z - (uint32_t) src1->z);

	return dest;
}

//adds one vector to another. returns ptr to dest
//dest can equal source
vms_vector *vm_vec_add2(vms_vector *dest,vms_vector *src)
{
	return vm_vec_add(dest,dest,src);
}

//subs one vector from another, returns ptr to dest
//dest can equal source
vms_vector *vm_vec_sub2(vms_vector *dest,vms_vector *src)
{
	return vm_vec_sub(dest,dest,src);
}

//averages two vectors. returns ptr to dest
//dest can equal either source
vms_vector *vm_vec_avg(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	//the sum wraps to 32 bits before the shift, as in the assembly
	dest->x = (fix) ((uint32_t) src0->x + (uint32_t) src1->x) >> 1;
	dest->y = (fix) ((uint32_t) src0->y + (uint32_t) src1->y) >> 1;
	dest->z = (fix) ((uint32_t) src0->z + (uint32_t) src1->z) >> 1;

	return dest;
}

//averages four vectors. returns ptr to dest
//dest can equal any source
vms_vector *vm_vec_avg4(vms_vector *dest,vms_vector *src0,vms_vector *src1,vms_vector *src2,vms_vector *src3)
{
	dest->x = (fix) ((uint32_t) src0->x + (uint32_t) src1->x + (uint32_t) src2->x + (uint32_t) src3->x) >> 2;
	dest->y = (fix) ((uint32_t) src0->y + (uint32_t) src1->y + (uint32_t) src2->y + (uint32_t) src3->y) >> 2;
	dest->z = (fix) ((uint32_t) src0->z + (uint32_t) src1->z + (uint32_t) src2->z + (uint32_t) src3->z) >> 2;

	return dest;
}

//scales a vector in place.  returns ptr to vector
vms_vector *vm_vec_scale(vms_vector *dest,fix s)
{
	dest->x = fixmul(dest->x,s);
	dest->y = fixmul(dest->y,s);
	dest->z = fixmul(dest->z,s);

	return dest;
}

//scales and copies a vector.  returns ptr to dest
vms_vector *vm_vec_copy_scale(vms_vector *dest,vms_vector *src,fix s)
{
	dest->x = fixmul(src->x,s);
	dest->y = fixmul(src->y,s);
	dest->z = fixmul(src->z,s);

	return dest;
}

//scales a vector, adds it to another, and stores in a 3rd vector
//dest = src1 + k * src2
vms_vector *vm_vec_scale_add(vms_vector *dest,vms_vector *src1,vms_vector *src2,fix k)
{
	dest->x = (fix) ((uint32_t) fixmul(src2->x,k) + (uint32_t) src1->x);
	dest->y = (fix) ((uint32_t) fixmul(src2->y,k) + (uint32_t) src1->y);
	dest->z = (fix) ((uint32_t) fixmul(src2->z,k) + (uint32_t) src1->z);

	return dest;
}

//scales a vector and adds it to another
//dest += k * src
vms_vector *vm_vec_scale_add2(vms_vector *dest,vms_vector *src,fix k)
{
	dest->x = (fix) ((uint32_t) dest->x + (uint32_t) fixmul(src->x,k));
	dest->y = (fix) ((uint32_t) dest->y + (uint32_t) fixmul(src->y,k));
	dest->z = (fix) ((uint32_t) dest->z + (uint32_t) fixmul(src->z,k));

	return dest;
}

//scales a vector in place, taking n/d for scale.  returns ptr to vector
//dest *= n/d
vms_vector *vm_vec_scale2(vms_vector *dest,fix n,fix d)
{
	if (d == 0)		// @mk, 01/04/94, prevent divide overflow
		return dest;

	// ASCENT: a quotient overflow saturates. It was a processor exception.
	dest->x = fixmuldiv(dest->x,n,d);
	dest->y = fixmuldiv(dest->y,n,d);
	dest->z = fixmuldiv(dest->z,n,d);

	return dest;
}

//the sum of the squares of the three values, as a quad that wraps
static inline uint64_t sum_squares(fix x,fix y,fix z)
{
	return mul64(x,x) + mul64(y,y) + mul64(z,z);
}

//returns magnitude of a vector
fix vm_vec_mag(vms_vector *v)
{
	return quad_mag(sum_squares(v->x,v->y,v->z));
}

//computes the distance between two points. (does sub and mag)
fix vm_vec_dist(vms_vector *v0,vms_vector *v1)
{
	vms_vector t;

	vm_vec_sub(&t,v0,v1);

	return vm_vec_mag(&t);
}

//neg if the sign is set. 0x80000000 stays negative, as in the assembly
static inline fix neg_if_sign(fix a)
{
	return (a < 0) ? (fix) (0 - (uint32_t) a) : a;
}

//uses dist = largest + next_largest*3/8 + smallest*3/16
//takes the absolute values of the three components
static fix mag_quick(fix a,fix b,fix c)
{
	fix t;

	if (!(a > b)) {
		t = a; a = b; b = t;
	}

	if (!(b > c)) {
		t = b; b = c; c = t;
		if (!(a > b)) {
			t = a; a = b; b = t;
		}
	}

	b >>= 2;									//    b*1/4
	c >>= 3;									//            c*1/8
	b = (fix) ((uint32_t) b + (uint32_t) c);	//    b*1/4 + c*1/8
	a = (fix) ((uint32_t) a + (uint32_t) b);	//a + b*1/4 + c*1/8
	b >>= 1;									//    b*1/8 + c*1/16
	a = (fix) ((uint32_t) a + (uint32_t) b);	//a + b*3/8 + c*3/16

	return a;
}

//computes an approximation of the magnitude of the vector
//uses dist = largest + next_largest*3/8 + smallest*3/16
fix vm_vec_mag_quick(vms_vector *v)
{
	return mag_quick(neg_if_sign(v->x),neg_if_sign(v->y),neg_if_sign(v->z));
}

//computes an approximation of the distance between two points.
//uses dist = largest + next_largest*3/8 + smallest*3/16
fix vm_vec_dist_quick(vms_vector *v0,vms_vector *v1)
{
	vms_vector t;

	vm_vec_sub(&t,v0,v1);

	return mag_quick(neg_if_sign(t.z),neg_if_sign(t.x),neg_if_sign(t.y));
}

//divide the three components of src by mag, and store in dest.
//the assembly divides with idiv, so a magnitude of 0x80000000 or more is a
//negative divisor.
static inline void div_by_mag(vms_vector *dest,vms_vector *src,fix mag)
{
	dest->x = fixdiv(src->x,mag);
	dest->y = fixdiv(src->y,mag);
	dest->z = fixdiv(src->z,mag);
}

//normalize a vector. returns mag of source vec
//if the magnitude is zero, dest is not changed
fix vm_vec_copy_normalize(vms_vector *dest,vms_vector *src)
{
	fix mag = vm_vec_mag(src);

	if (mag != 0)
		div_by_mag(dest,src,mag);

	return mag;
}

//normalize a vector in place.  returns mag of source vec
fix vm_vec_normalize(vms_vector *v)
{
	return vm_vec_copy_normalize(v,v);
}

//normalize a vector. returns mag of source vec. uses approx mag
//if the magnitude is zero, dest is not changed
fix vm_vec_copy_normalize_quick(vms_vector *dest,vms_vector *src)
{
	fix mag = vm_vec_mag_quick(src);

	if (mag != 0)
		div_by_mag(dest,src,mag);

	return mag;
}

//normalize a vector in place.  returns mag of source vec. used approx mag.
fix vm_vec_normalize_quick(vms_vector *v)
{
	return vm_vec_copy_normalize_quick(v,v);
}

//return the normalized direction vector between two points
//dest = normalized(end - start).  Returns mag of direction vector
//NOTE: the order of the parameters matches the vector subtraction
fix vm_vec_normalized_dir(vms_vector *dest,vms_vector *end,vms_vector *start)
{
	vm_vec_sub(dest,end,start);

	return vm_vec_normalize(dest);
}

//save as vm_vec_normalized_dir, but with quick sqrt
fix vm_vec_normalized_dir_quick(vms_vector *dest,vms_vector *end,vms_vector *start)
{
	vm_vec_sub(dest,end,start);

	return vm_vec_normalize_quick(dest);
}

//returns dot product of two vectors
//on overflow, returns the saturated value 0x7fffffff or -0x7fffffff
fix vm_vec_dotprod(vms_vector *v0,vms_vector *v1)
{
	uint64_t q = mul64(v0->x,v1->x) + mul64(v0->y,v1->y) + mul64(v0->z,v1->z);
	fix r = quad_to_fix(q);
	int16_t high = (int16_t) (q >> 48);		//real sign from high word

	//always do overflow check, and return saturated value
	if (high != ((r < 0) ? -1 : 0))			//same sign?
		r = ((int64_t) q < 0) ? -0x7fffffff : 0x7fffffff;

	return r;
}

fix vm_vec_dot(vms_vector *v0,vms_vector *v1)
{
	return vm_vec_dotprod(v0,v1);
}

//computes cross product of two vectors. returns ptr to dest
//dest CANNOT equal either source
//Note: this magnitude of the resultant vector is the
//product of the magnitudes of the two source vectors.  This means it is
//quite easy for this routine to overflow and underflow.  Be careful that
//your inputs are ok.
vms_vector *vm_vec_crossprod(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	dest->x = quad_to_fix(mul64(src1->z,src0->y) - mul64(src1->y,src0->z));
	dest->y = quad_to_fix(mul64(src1->x,src0->z) - mul64(src1->z,src0->x));
	dest->z = quad_to_fix(mul64(src1->y,src0->x) - mul64(src1->x,src0->y));

	return dest;
}

vms_vector *vm_vec_cross(vms_vector *dest,vms_vector *src0,vms_vector *src1)
{
	return vm_vec_crossprod(dest,src0,src1);
}

//absolute value, as the abs_eax macro (0x80000000 stays as it is)
static inline uint32_t abs_u32(fix a)
{
	return (a < 0) ? 0 - (uint32_t) a : (uint32_t) a;
}

//make sure a vector is reasonably sized to go into a cross product
static void check_vec(vms_vector *v)
{
	uint32_t bits = abs_u32(v->x) | abs_u32(v->y) | abs_u32(v->z);
	int shift = 0;		//init shift count

	if (bits == 0)		//null vector
		return;

	if (bits & 0xfffc0000) {		//too big

		// ASCENT: logical shifts. See the top of this file.
		while (bits & 0x00f00000) {
			shift += 4;
			bits >>= 4;
		}
		while (bits & 0xfffc0000) {
			shift += 2;
			bits >>= 2;
		}

		v->x >>= shift;
		v->y >>= shift;
		v->z >>= shift;
	}
	else if (!(bits & 0xffff8000)) {		//maybe too small...

		while (!(bits & 0xfffff000)) {
			shift += 4;
			bits <<= 4;
		}
		while (!(bits & 0xffff8000)) {
			shift += 2;
			bits <<= 2;
		}

		v->x = (fix) ((uint32_t) v->x << shift);
		v->y = (fix) ((uint32_t) v->y << shift);
		v->z = (fix) ((uint32_t) v->z << shift);
	}
}

//computes non-normalized surface normal from three points.
//returns ptr to dest
//dest CANNOT equal either source
//this routine does make an effort that cross product does not overflow or
//underflow
vms_vector *vm_vec_perp(vms_vector *dest,vms_vector *p0,vms_vector *p1,vms_vector *p2)
{
	vms_vector tempv0,tempv1;

	vm_vec_sub(&tempv1,p2,p1);		//src2 - src1
	vm_vec_sub(&tempv0,p1,p0);		//src1 - src0

	check_vec(&tempv0);		//make sure reasonable value
	check_vec(&tempv1);		//make sure reasonable value

	return vm_vec_crossprod(dest,&tempv0,&tempv1);
}

//computes surface normal from three points. result is normalized
//returns ptr to dest
//dest CANNOT equal either source
vms_vector *vm_vec_normal(vms_vector *dest,vms_vector *p0,vms_vector *p1,vms_vector *p2)
{
	vm_vec_perp(dest,p0,p1,p2);		//get unnormalized
	vm_vec_normalize(dest);

	return dest;
}

//fill in a matrix from the sines & cosines of the three angles
static vms_matrix *sincos_2_matrix(vms_matrix *m,fix sinp,fix cosp,fix sinb,fix cosb,fix sinh,fix cosh)
{
	fix sbsh,cbch,cbsh,sbch;

	sbsh = fixmul(sinb,sinh);
	cbch = fixmul(cosb,cosh);
	M1(m) = (fix) ((uint32_t) cbch + (uint32_t) fixmul(sbsh,sinp));	//m1=cbch+sbspsh
	M8(m) = (fix) ((uint32_t) fixmul(cbch,sinp) + (uint32_t) sbsh);	//m8=sbsh+cbchsp

	cbsh = fixmul(cosb,sinh);
	sbch = fixmul(sinb,cosh);
	M2(m) = (fix) ((uint32_t) fixmul(cbsh,sinp) - (uint32_t) sbch);	//m2=cbshsp-sbch
	M7(m) = (fix) ((uint32_t) fixmul(sbch,sinp) - (uint32_t) cbsh);	//m7=sbchsp-cbsh

	M3(m) = fixmul(sinh,cosp);		//m3=shcp
	M4(m) = fixmul(sinb,cosp);		//m4=sbcp
	M5(m) = fixmul(cosb,cosp);		//m5=cbcp
	M6(m) = (fix) (0 - (uint32_t) sinp);	//m6=-sp
	M9(m) = fixmul(cosh,cosp);		//m9=chcp

	return m;
}

//computes a matrix from a set of three angles.  returns ptr to matrix
vms_matrix *vm_angles_2_matrix(vms_matrix *m,vms_angvec *a)
{
	fix sinp,cosp,sinb,cosb,sinh,cosh;

	fix_sincos(a->p,&sinp,&cosp);
	fix_sincos(a->b,&sinb,&cosb);
	fix_sincos(a->h,&sinh,&cosh);

	return sincos_2_matrix(m,sinp,cosp,sinb,cosb,sinh,cosh);
}

//computes a matrix from a forward vector and an angle
vms_matrix *vm_vec_ang_2_matrix(vms_matrix *m,vms_vector *v,fixang a)
{
	fix sinp,cosp,sinb,cosb,sinh,cosh;

	fix_sincos(a,&sinb,&cosb);

	//extract heading & pitch from vector

	sinp = (fix) (0 - (uint32_t) v->y);		//m6=-sp
	cosp = fix_sqrt((fix) ((uint32_t) f1_0 - (uint32_t) fixmul(sinp,sinp)));

	// ASCENT: cosp is zero when the vector is straight up or down. The
	// divide was a processor exception. Here the result saturates.
	sinh = fixdiv(v->x,cosp);
	cosh = fixdiv(v->z,cosp);

	return sincos_2_matrix(m,sinp,cosp,sinb,cosb,sinh,cosh);
}

//only the forward vector is present.  src can be the address of zvec
//returns 0 if the vector has no length. the matrix is not changed then
static int forward_vec_2_matrix(vms_matrix *m,vms_vector *src)
{
	vms_vector xvec,yvec,zvec;

	zvec = *src;
	if (vm_vec_normalize(&zvec) == 0)
		return 0;		//0-len vec in vec_2_mat

	m->fvec = zvec;

	if ((zvec.x | zvec.z) == 0) {		//check both x & z == 0

		//forward vector is straight up (or down)

		M1(m) = f1_0;
		M8(m) = (zvec.y < 0) ? f1_0 : -f1_0;
		M4(m) = 0;
		M7(m) = 0;
		M2(m) = 0;
		M5(m) = 0;
	}
	else {
		xvec.x = zvec.z;
		xvec.y = 0;
		xvec.z = (fix) (0 - (uint32_t) zvec.x);
		vm_vec_normalize(&xvec);

		m->rvec = xvec;

		vm_vec_crossprod(&yvec,&zvec,&xvec);

		m->uvec = yvec;
	}

	return 1;
}

//computes a matrix from one or more vectors. The forward vector is required,
//with the other two being optional.  If both up & right vectors are passed,
//the up vector is used.  If only the forward vector is passed, a bank of
//zero is assumed
//returns ptr to matrix
//up/right vector need not be exactly perpendicular to forward vec
//Note: this routine loses precision as the forward vector approaches
//straigt up or down (I think)
vms_matrix *vm_vector_2_matrix(vms_matrix *m,vms_vector *fvec,vms_vector *uvec,vms_vector *rvec)
{
	vms_vector xvec,yvec,zvec;

	if (uvec != NULL) {		//use forward and up vectors

		zvec = *fvec;
		if (vm_vec_normalize(&zvec) == 0)
			goto bad_vector2;

		yvec = *uvec;
		if (vm_vec_normalize(&yvec) == 0)
			goto bad_vector2;

		vm_vec_crossprod(&xvec,&yvec,&zvec);	//get x vector

		//normalize new perpendicular vector
		if (vm_vec_normalize(&xvec) == 0)
			goto bad_vector2;

		//now recompute up vector, in case it wasn't entirely perpendiclar
		vm_vec_crossprod(&yvec,&zvec,&xvec);
	}
	else if (rvec != NULL) {	//use forward and right vectors

		zvec = *fvec;
		if (vm_vec_normalize(&zvec) == 0)
			goto bad_vector2;

		xvec = *rvec;
		if (vm_vec_normalize(&xvec) == 0)
			goto bad_vector2;

		vm_vec_crossprod(&yvec,&zvec,&xvec);	//get y = z cross x

		//normalize new perpendicular vector
		if (vm_vec_normalize(&yvec) == 0)
			goto bad_vector2;

		//now recompute right vector, in case it wasn't entirely perpendiclar
		vm_vec_crossprod(&xvec,&yvec,&zvec);	//x = y cross z
	}
	else {		//only the forward vector is present
		forward_vec_2_matrix(m,fvec);
		return m;
	}

	m->rvec = xvec;
	m->uvec = yvec;
	m->fvec = zvec;

	return m;

	//one of the non-forward vectors caused a problem, so ignore them and
	//use just the forward vector.  As in the assembly, this starts from the
	//forward vector that is normalized already, and normalizes it again.
bad_vector2:
	forward_vec_2_matrix(m,&zvec);

	return m;
}

//rotates a vector through a matrix. returns ptr to dest vector
//dest CANNOT equal either source
vms_vector *vm_vec_rotate(vms_vector *dest,vms_vector *src,vms_matrix *m)
{
	dest->x = quad_to_fix(mul64(src->x,M1(m)) + mul64(src->y,M4(m)) + mul64(src->z,M7(m)));
	dest->y = quad_to_fix(mul64(src->x,M2(m)) + mul64(src->y,M5(m)) + mul64(src->z,M8(m)));
	dest->z = quad_to_fix(mul64(src->x,M3(m)) + mul64(src->y,M6(m)) + mul64(src->z,M9(m)));

	return dest;
}

//transpose a matrix in place. returns ptr to matrix
vms_matrix *vm_transpose_matrix(vms_matrix *m)
{
	fix t;

	t = M2(m);  M2(m) = M4(m);  M4(m) = t;
	t = M3(m);  M3(m) = M7(m);  M7(m) = t;
	t = M6(m);  M6(m) = M8(m);  M8(m) = t;

	return m;
}

vms_matrix *vm_transpose(vms_matrix *m)
{
	return vm_transpose_matrix(m);
}

//copy and transpose a matrix. returns ptr to matrix
//dest CANNOT equal source. use vm_transpose_matrix() if this is the case
vms_matrix *vm_copy_transpose_matrix(vms_matrix *dest,vms_matrix *src)
{
	M1(dest) = M1(src);
	M4(dest) = M2(src);
	M7(dest) = M3(src);

	M2(dest) = M4(src);
	M5(dest) = M5(src);
	M8(dest) = M6(src);

	M3(dest) = M7(src);
	M6(dest) = M8(src);
	M9(dest) = M9(src);

	return dest;
}

vms_matrix *vm_copy_transpose(vms_matrix *dest,vms_matrix *src)
{
	return vm_copy_transpose_matrix(dest,src);
}

//multiply (dot) a row of src0 with a column of src1
#define vv_mul(dest,x0,y0,z0,x1,y1,z1) \
	dest = quad_to_fix(mul64(x0(src0),x1(src1)) + mul64(y0(src0),y1(src1)) + mul64(z0(src0),z1(src1)))

//mulitply 2 matrices, fill in dest.  returns ptr to dest
//dest CANNOT equal either source
vms_matrix *vm_matrix_x_matrix(vms_matrix *dest,vms_matrix *src0,vms_matrix *src1)
{
	vv_mul(M1(dest), M1,M2,M3, M1,M4,M7);
	vv_mul(M2(dest), M1,M2,M3, M2,M5,M8);
	vv_mul(M3(dest), M1,M2,M3, M3,M6,M9);

	vv_mul(M4(dest), M4,M5,M6, M1,M4,M7);
	vv_mul(M5(dest), M4,M5,M6, M2,M5,M8);
	vv_mul(M6(dest), M4,M5,M6, M3,M6,M9);

	vv_mul(M7(dest), M7,M8,M9, M1,M4,M7);
	vv_mul(M8(dest), M7,M8,M9, M2,M5,M8);
	vv_mul(M9(dest), M7,M8,M9, M3,M6,M9);

	return dest;
}

//computes the delta angle between two normalized vectors.
//if the forward vector is NULL, the absolute values of the delta angle
//is returned.  If it is specified, the rotation around that vector from
//v0 to v1 is returned.
fixang vm_vec_delta_ang_norm(vms_vector *v0,vms_vector *v1,vms_vector *fvec)
{
	int32_t a = fix_acos(vm_vec_dotprod(v0,v1));

	if (fvec != NULL) {
		vms_vector t;

		//do cross product to find sign of angle
		vm_vec_crossprod(&t,v0,v1);
		if (vm_vec_dotprod(&t,fvec) < 0)
			a = -a;
	}

	return (fixang) a;
}

//computes the delta angle between two vectors.
//vectors need not be normalized. if they are, call vm_vec_delta_ang_norm()
//the forward vector (third parameter) can be NULL, in which case the absolute
//value of the angle in returned.  Otherwise the angle around that vector is
//returned.
//NOTE: as in the assembly, this normalizes the two vectors in place
fixang vm_vec_delta_ang(vms_vector *v0,vms_vector *v1,vms_vector *fvec)
{
	vm_vec_normalize(v0);
	vm_vec_normalize(v1);

	return vm_vec_delta_ang_norm(v0,v1,fvec);
}

//compute the distance from a point to a plane.  takes the normalized normal
//of the plane, a point on the plane, and the point to check.
//returns distance
//distance is signed, so negative dist is on the back of the plane
fix vm_dist_to_plane(vms_vector *checkp,vms_vector *norm,vms_vector *planep)
{
	vms_vector t;

	vm_vec_sub(&t,checkp,planep);		//vector plane -> point

	return vm_vec_dotprod(&t,norm);
}

//extract angles from a matrix
vms_angvec *vm_extract_angles_matrix(vms_angvec *a,vms_matrix *m)
{
	fix sinh,cosh,cp,sp,sb,cb;

	//extract heading & pitch from forward vector

	if ((m->fvec.x | m->fvec.z) == 0)	//check for z==x==0
		a->h = 0;								//zero, use head=0
	else
		a->h = fix_atan2(m->fvec.z,m->fvec.x);

	fix_sincos(a->h,&sinh,&cosh);		//get back sh

	// ASCENT: the divides saturate. They were processor exceptions.
	if (abs_u32(cosh) > abs_u32(sinh))		//which is larger?
		cp = fixdiv(m->fvec.z,cosh);		//cp = chcp / ch
	else
		cp = fixdiv(m->fvec.x,sinh);		//cp = shcp / sh

	sp = (fix) (0 - (uint32_t) m->fvec.y);	//fvec.y = -sp

	if ((sp | cp) == 0)			//bogus vec, set p=0
		a->p = 0;
	else
		a->p = fix_atan2(cp,sp);

	if (cp == 0)			//the cosine of pitch is zero.  we're pitched straight up. say no bank
		a->b = 0;
	else {
		sb = fixdiv(m->rvec.y,cp);		//m4 = sbcp
		cb = fixdiv(m->uvec.y,cp);		//m5 = cbcp

		if ((sb | cb) == 0)		//bogus vec, set b=0
			a->b = 0;
		else
			a->b = fix_atan2(cb,sb);
	}

	return a;
}

//extract heading and pitch from a normalized vector, assuming bank==0
vms_angvec *vm_extract_angles_vector_normalized(vms_angvec *a,vms_vector *v)
{
	a->b = 0;		//always zero bank

	a->p = fix_asin((fix) (0 - (uint32_t) v->y));	//p = asin(-y)

	if ((v->x | v->z) == 0)		//check for up vector
		a->h = 0;
	else
		a->h = fix_atan2(v->z,v->x);	//h = atan2(x,z) (or zero)

	return a;
}

//extract heading and pitch from a vector, assuming bank==0
//if the vector has no length, the angles are not changed
vms_angvec *vm_extract_angles_vector(vms_angvec *a,vms_vector *v)
{
	vms_vector t;

	if (vm_vec_copy_normalize(&t,v) != 0)
		vm_extract_angles_vector_normalized(a,&t);

	return a;
}
