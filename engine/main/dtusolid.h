/*
 * ASCENT: new file. Dtu_improved_physics: scenery as solid bodies for the
 * player. See dtusolid.c and docs/re/physics.md, section 13.2.
 */

#ifndef _DTUSOLID_H
#define _DTUSOLID_H

#include "fix.h"
#include "vecmat.h"
#include "object.h"

// The shape of scenery: an upright box (the box of the model, turned by
// the heading of the object) or an upright cylinder.
typedef struct dtu_solid {
	double cx,cz;			// the position of the object in x and z
	double ax,az,bx,bz;		// the two axes of the box in x and z (unit vectors)
	double lo[2],hi[2];		// the box along the two axes, relative to cx, cz
	double r;				// the radius of a cylinder (the box is then one point), else 0
	double bot,top;			// the height of the bottom and of the top
} dtu_solid;

// The kinds of a hit of dtu_solid_sweep()
#define DTU_SOLID_SIDE		1
#define DTU_SOLID_TOP		2		// the feet come down on the top
#define DTU_SOLID_BOTTOM	3		// the head comes up against the bottom

#define DTU_SOLID_STEP		0x18000		// 1.5: a top that is this much or less above the feet does not stop the body, the body steps onto it
#define DTU_SOLID_RIM		0x10000		// 1.0: an axis that is farther than this outside of the top slides off it
#define DTU_SOLID_SKIN		0x0800		// 1/32: the body stops this far before a side

int dtu_solid_get(object *o,dtu_solid *s);
int dtu_solid_is(object *o);
int dtu_solid_inside(dtu_solid *s,double x,double z,double rad);
int dtu_solid_support(vms_vector *axis,fix rad,fix lo,fix hi,fix *top,vms_vector *out_dir,fix *out_dist);
int dtu_solid_sweep(vms_vector *p0,vms_vector *move,fix rad,fix down,fix up,fix *t,vms_vector *norm,int *kind);
int dtu_solid_overlap(vms_vector *pos,fix rad,fix down,fix up,vms_vector *norm,fix *depth);

// In dtuobj_scenery.c
int dtu_solid_on(void);
fix dtu_dressing_push(object *d,vms_vector *delta,int strength);
int dressing_pushable(object *d);
int dtu_player_push_strength(void);
void dressing_level_reset(void);

#endif
