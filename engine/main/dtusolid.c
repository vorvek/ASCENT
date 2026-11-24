/*
 * ASCENT: new file. Dtu_improved_physics: scenery as solid bodies for the
 * player. New code, not in the executable.
 *
 * The executable (and the port with Improved physics off) tests scenery
 * as a cylinder of body_radius[0] in the move query of the player, and a
 * hit puts the player back to the start of the frame and pushes the
 * scenery. The floor search does not see objects. So the player cannot
 * stand on a crate: a player who comes down on one hangs in the air, hits
 * it in each try of each frame and pushes it.
 *
 * Here scenery is an upright box (the box of the model, turned with the
 * object) or an upright cylinder (no box, or not upright), from its
 * bottom to its top. The body of the player is an upright cylinder with
 * the radius of the move query, from the feet to the head.
 * - The top holds the player (dtu_solid_support, used by the floor search
 *   of the capsule in physics.c).
 * - A side stops the move into it and lets the player slide along it
 *   (dtu_solid_sweep). A top that is DTU_SOLID_STEP or less above the feet
 *   is no side: the player steps onto it.
 * - The bottom stops the head.
 * See docs/re/physics.md, section 13.2.
 */

#include <math.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "object.h"
#include "bm.h"
#include "polyobj.h"
#include "dtugame.h"
#include "dtuobj.h"
#include "dtuobj_int.h"
#include "dtusolid.h"

int dtu_solid_get(object *o,dtu_solid *s)
{
	polymodel *po;
	vms_vector mn, mx;

	if (o->type != OBJ_DRESSING || o->render_type != RT_POLYOBJ)
		return 0;
	if (o->flags & (OF_SHOULD_BE_DEAD|OF_DESTROYED))
		return 0;
	if (o->id < N_dressing_types && Dressing_info[o->id].pass_thru)
		return 0;

	po = obj_model(o);
	s->cx = f2fl(o->pos.x);
	s->cz = f2fl(o->pos.z);
	s->ax = 1; s->az = 0;
	s->bx = 0; s->bz = 1;
	s->r = 0;
	// The box of the drawn points of the model (model_points_box), not box
	// 1 of the file: the box of chair.pof is 2.95 above the drawn chair, so
	// a chair stood on its seat 2.95 under the floor after a push.
	if (po && o->orient.uvec.y >= F1_0*9/10 && model_points_box(o,-1,&mn,&mx)) {
		double ax = f2fl(o->orient.rvec.x), az = f2fl(o->orient.rvec.z);
		double bx = f2fl(o->orient.fvec.x), bz = f2fl(o->orient.fvec.z);
		double la = sqrt(ax*ax + az*az), lb = sqrt(bx*bx + bz*bz);

		if (la > 1e-6 && lb > 1e-6) {
			s->ax = ax/la; s->az = az/la;
			s->bx = bx/lb; s->bz = bz/lb;
		}
		s->lo[0] = f2fl(mn.x); s->hi[0] = f2fl(mx.x);
		s->lo[1] = f2fl(mn.z); s->hi[1] = f2fl(mx.z);
		s->bot = f2fl(o->pos.y + mn.y);
		s->top = f2fl(o->pos.y + mx.y);
	}
	else {
		vms_vector center;
		fix radius,half_height;

		dtu_get_body_cylinder(o,&center,&radius,&half_height);
		s->cx = f2fl(center.x);
		s->cz = f2fl(center.z);
		s->lo[0] = s->hi[0] = s->lo[1] = s->hi[1] = 0;
		s->r = f2fl(radius);
		s->bot = f2fl(center.y - half_height);
		s->top = f2fl(center.y + half_height);
	}
	return s->top > s->bot;
}

int dtu_solid_is(object *o)
{
	dtu_solid s;

	return dtu_solid_get(o,&s);
}

static void solid_local(dtu_solid *s,double x,double z,double *u,double *v)
{
	double dx = x - s->cx, dz = z - s->cz;

	*u = dx*s->ax + dz*s->az;
	*v = dx*s->bx + dz*s->bz;
}

// The distance of the point u, v from the box (negative inside of it: the
// depth below the nearest edge), and the direction away from the box.
static double solid_rect_dist(dtu_solid *s,double u,double v,double *nu,double *nv)
{
	double du = u < s->lo[0] ? u - s->lo[0] : u > s->hi[0] ? u - s->hi[0] : 0;
	double dv = v < s->lo[1] ? v - s->lo[1] : v > s->hi[1] ? v - s->hi[1] : 0;
	double d = sqrt(du*du + dv*dv);

	if (d > 1e-9) {
		*nu = du/d;
		*nv = dv/d;
		return d;
	}
	{
		double e[4] = { u - s->lo[0], s->hi[0] - u, v - s->lo[1], s->hi[1] - v };
		int k = 0,i;

		for (i=1; i<4; i++)
			if (e[i] < e[k])
				k = i;
		*nu = k == 0 ? -1 : k == 1 ? 1 : 0;
		*nv = k == 2 ? -1 : k == 3 ? 1 : 0;
		if (s->lo[0] == s->hi[0] && s->lo[1] == s->hi[1]) {
			// a cylinder, the point at its axis
			*nu = 1;
			*nv = 0;
		}
		return -e[k];
	}
}

// 1 if the point x, z is within rad of the shape
int dtu_solid_inside(dtu_solid *s,double x,double z,double rad)
{
	double u,v,nu,nv;

	solid_local(s,x,z,&u,&v);
	return solid_rect_dist(s,u,v,&nu,&nv) < rad + s->r;
}

// The move m of the point p against the box lo, hi. The part of the move
// (0 to 1) at which the point enters the box, and the side of the entry.
static int solid_enter_box(const double *p,const double *m,const double *lo,const double *hi,double *t,double *nu,double *nv)
{
	double tin = -1e30, tout = 1e30;
	int axis = -1,k;

	for (k=0; k<2; k++) {
		if (fabs(m[k]) < 1e-12) {
			if (p[k] <= lo[k] || p[k] >= hi[k])
				return 0;
		}
		else {
			double t0 = (lo[k] - p[k])/m[k], t1 = (hi[k] - p[k])/m[k];

			if (t0 > t1) { double x = t0; t0 = t1; t1 = x; }
			if (t0 > tin) { tin = t0; axis = k; }
			if (t1 < tout) tout = t1;
			if (tin > tout)
				return 0;
		}
	}
	if (axis < 0 || tin < 0 || tin > 1)
		return 0;
	*t = tin;
	*nu = axis == 0 ? (m[0] > 0 ? -1 : 1) : 0;
	*nv = axis == 1 ? (m[1] > 0 ? -1 : 1) : 0;
	return 1;
}

static int solid_enter_circle(const double *p,const double *m,double cu,double cv,double R,double *t,double *nu,double *nv)
{
	double eu = p[0] - cu, ev = p[1] - cv;
	double a = m[0]*m[0] + m[1]*m[1], b = eu*m[0] + ev*m[1], c = eu*eu + ev*ev - R*R, disc, x;

	if (a < 1e-18 || c < 0 || b >= 0)
		return 0;
	disc = b*b - a*c;
	if (disc < 0)
		return 0;
	x = (-b - sqrt(disc))/a;
	if (x < 0 || x > 1)
		return 0;
	*t = x;
	*nu = (eu + m[0]*x)/R;
	*nv = (ev + m[1]*x)/R;
	return 1;
}

// The move mu, mv of a point at u, v that is outside of the shape grown by
// R: the part of the move at which it enters, and the direction away from
// the shape there. The grown shape is the box grown along each axis and
// a circle at each corner.
static int solid_enter(dtu_solid *s,double u,double v,double mu,double mv,double R,double *t,double *nu,double *nv)
{
	double p[2] = { u, v }, m[2] = { mu, mv }, lo[2], hi[2], x, a, b;
	int found = 0,i;

	lo[0] = s->lo[0] - R; hi[0] = s->hi[0] + R; lo[1] = s->lo[1]; hi[1] = s->hi[1];
	if (hi[1] > lo[1] && solid_enter_box(p,m,lo,hi,&x,&a,&b)) {
		found = 1; *t = x; *nu = a; *nv = b;
	}
	lo[0] = s->lo[0]; hi[0] = s->hi[0]; lo[1] = s->lo[1] - R; hi[1] = s->hi[1] + R;
	if (hi[0] > lo[0] && solid_enter_box(p,m,lo,hi,&x,&a,&b) && (!found || x < *t)) {
		found = 1; *t = x; *nu = a; *nv = b;
	}
	for (i=0; i<4; i++) {
		double cu = (i & 1) ? s->hi[0] : s->lo[0], cv = (i & 2) ? s->hi[1] : s->lo[1];

		if (solid_enter_circle(p,m,cu,cv,R,&x,&a,&b) && (!found || x < *t)) {
			found = 1; *t = x; *nu = a; *nv = b;
		}
	}
	return found;
}

// The move m of the body (the center at p, the radius rad, the feet down
// below the center and the head up above it) against one shape.
static int solid_test(dtu_solid *s,const double *p,const double *m,double rad,double down,double up,double *t,double *n,int *kind)
{
	double R = rad + s->r, step = f2fl(DTU_SOLID_STEP);
	double u,v,mu,mv,nu,nv,d,tin = 0,feet0 = p[1] - down,head0 = p[1] + up,feet,head,tl;

	solid_local(s,p[0],p[2],&u,&v);
	mu = m[0]*s->ax + m[2]*s->az;
	mv = m[0]*s->bx + m[2]*s->bz;
	d = solid_rect_dist(s,u,v,&nu,&nv);

	if (d < R - 1e-3) {
		// The start is over, under or inside of the shape
		tin = 0;
		feet = feet0;
		head = head0;
	}
	else {
		if (!solid_enter(s,u,v,mu,mv,R,&tin,&nu,&nv))
			return 0;
		feet = feet0 + m[1]*tin;
		head = head0 + m[1]*tin;
	}

	if (feet >= s->top - step) {
		// Over the top: the feet can come down on it in this move
		if (m[1] >= 0 || feet0 + m[1] >= s->top)
			return 0;
		tl = feet > s->top ? (s->top - feet0)/m[1] : tin;
		if (tl < tin || tl > 1 || !dtu_solid_inside(s,p[0] + m[0]*tl,p[2] + m[2]*tl,rad))
			return 0;
		*t = tl;
		n[0] = 0; n[1] = 1; n[2] = 0;
		*kind = DTU_SOLID_TOP;
		return 1;
	}
	if (head <= s->bot + 1e-3) {
		// Under the bottom: the head can come up against it
		if (m[1] <= 0 || head0 + m[1] <= s->bot)
			return 0;
		tl = head < s->bot ? (s->bot - head0)/m[1] : tin;
		if (tl < tin || tl > 1 || !dtu_solid_inside(s,p[0] + m[0]*tl,p[2] + m[2]*tl,rad))
			return 0;
		*t = tl;
		n[0] = 0; n[1] = -1; n[2] = 0;
		*kind = DTU_SOLID_BOTTOM;
		return 1;
	}
	if (d < R - 1e-3) {
		// Inside of the shape: only a move out of it is free
		if (mu*nu + mv*nv >= 0)
			return 0;
	}
	*t = tin;
	n[0] = nu*s->ax + nv*s->bx;
	n[1] = 0;
	n[2] = nu*s->az + nv*s->bz;
	*kind = DTU_SOLID_SIDE;
	return 1;
}

// 1 if the scenery o can be nearer than reach to the point x, z
static int solid_near(object *o,double x,double z,double reach)
{
	double dx,dz,r;

	if (o->type != OBJ_DRESSING)
		return 0;
	dx = f2fl(o->pos.x) - x;
	dz = f2fl(o->pos.z) - z;
	r = reach + 2*f2fl(o->size) + 4;
	return dx*dx + dz*dz < r*r;
}

// The first scenery that the move of the body of the player hits. The
// center of the body is at p0, its radius is rad, the feet are down below
// the center and the head is up above it. *t is the part of the move (0
// to 1) up to the hit, *norm the direction away from the scenery, *kind
// DTU_SOLID_SIDE, _TOP or _BOTTOM. Returns the object number, or -1.
int dtu_solid_sweep(vms_vector *p0,vms_vector *move,fix rad,fix down,fix up,fix *t,vms_vector *norm,int *kind)
{
	double p[3] = { f2fl(p0->x), f2fl(p0->y), f2fl(p0->z) };
	double m[3] = { f2fl(move->x), f2fl(move->y), f2fl(move->z) };
	double reach = f2fl(rad) + sqrt(m[0]*m[0] + m[2]*m[2]), best = 2, n[3], x;
	int best_obj = -1,i,k;

	for (i=0; i<=Highest_object_index; i++) {
		dtu_solid s;

		if (!solid_near(&Objects[i],p[0],p[2],reach) || !dtu_solid_get(&Objects[i],&s))
			continue;
		if (!solid_test(&s,p,m,f2fl(rad),f2fl(down),f2fl(up),&x,n,&k) || x >= best)
			continue;
		best = x;
		best_obj = i;
		*t = fl2f(x);
		norm->x = fl2f(n[0]);
		norm->y = fl2f(n[1]);
		norm->z = fl2f(n[2]);
		*kind = k;
	}
	return best_obj;
}

// The highest top of scenery that holds a body with the axis at axis.x,
// axis.z and the radius rad: a top between lo and hi. *out_dist is the
// distance of the axis from the top in x and z (0 over it), *out_dir the
// direction away from it. Returns the object number, or -1.
int dtu_solid_support(vms_vector *axis,fix rad,fix lo,fix hi,fix *top,vms_vector *out_dir,fix *out_dist)
{
	double x = f2fl(axis->x), z = f2fl(axis->z), best = 0;
	int best_obj = -1,i;

	for (i=0; i<=Highest_object_index; i++) {
		dtu_solid s;
		double u,v,nu,nv,d;

		if (!solid_near(&Objects[i],x,z,f2fl(rad)) || !dtu_solid_get(&Objects[i],&s))
			continue;
		if (s.top < f2fl(lo) || s.top > f2fl(hi) || (best_obj >= 0 && s.top <= best))
			continue;
		solid_local(&s,x,z,&u,&v);
		d = solid_rect_dist(&s,u,v,&nu,&nv) - s.r;
		if (d >= f2fl(rad))
			continue;
		best = s.top;
		best_obj = i;
		*top = fl2f(s.top);
		*out_dist = d > 0 ? fl2f(d) : 0;
		out_dir->x = fl2f(nu*s.ax + nv*s.bx);
		out_dir->y = 0;
		out_dir->z = fl2f(nu*s.az + nv*s.bz);
	}
	return best_obj;
}

// The scenery that the body is inside of (not over its top, not under its
// bottom), the direction out of it and the depth. The deepest one. Returns
// the object number, or -1.
int dtu_solid_overlap(vms_vector *pos,fix rad,fix down,fix up,vms_vector *norm,fix *depth)
{
	double x = f2fl(pos->x), z = f2fl(pos->z), feet = f2fl(pos->y - down), head = f2fl(pos->y + up), best = 0;
	int best_obj = -1,i;

	for (i=0; i<=Highest_object_index; i++) {
		dtu_solid s;
		double u,v,nu,nv,d;

		if (!solid_near(&Objects[i],x,z,f2fl(rad)) || !dtu_solid_get(&Objects[i],&s))
			continue;
		if (feet >= s.top - f2fl(DTU_SOLID_STEP) || head <= s.bot + 1e-3)
			continue;
		solid_local(&s,x,z,&u,&v);
		d = f2fl(rad) + s.r - solid_rect_dist(&s,u,v,&nu,&nv);
		if (d <= 2*f2fl(DTU_SOLID_SKIN) || d <= best)
			continue;
		best = d;
		best_obj = i;
		*depth = fl2f(d);
		norm->x = fl2f(nu*s.ax + nv*s.bx);
		norm->y = 0;
		norm->z = fl2f(nu*s.az + nv*s.bz);
	}
	return best_obj;
}
