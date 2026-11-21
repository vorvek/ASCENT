/*
 * ASCENT: internal header of the 3d library. Prototypes for the routines
 * of clipper.c (clipper.asm), which 3d.inc declared for the assembly.
 */

#ifndef _CLIPPER_H
#define _CLIPPER_H

#include "globvars.h"

//the number of temporary points in use
extern int free_point_num;

//free a temporary point
void free_temp_point(g3s_point *p);

//clips a line to the viewing pyramid. takes the two points and codes_or.
//on return *p0 and *p1 are the clipped points, one or both new. The two
//points can change places.
void clip_line(g3s_point **p0,g3s_point **p1,ubyte codes_or);

//3d clip a polygon. takes the source list, the destination list, the
//number of vertices and the codes. Each list must have room for two more
//pointers than the number of vertices.
//returns the list of clipped points (either src or dest), some of them
//new, and fills in the new number of vertices and the new codes.
g3s_point **clip_polygon(g3s_point **src,g3s_point **dest,int *nv,g3s_codes *cc);

#endif
