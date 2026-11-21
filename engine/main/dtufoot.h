/*
 * ASCENT: new file. The footstep sounds of the player and of the creatures
 * (dtufoot.c).
 */
#ifndef _DTUFOOT_H
#define _DTUFOOT_H

#include "object.h"

// 1 (the default): the player and the creatures have footstep sounds. 0: none.
extern int Dtu_footsteps;

// Corrections, 1 by default, 0 with -origsound (see dtufoot.c).
extern int Dtu_fix_foot_pan;
extern int Dtu_fix_creature_steps;

// Call it one time on each frame, after the objects have moved.
void dtufoot_frame(void);

// Call it at the end of the move of an active creature on each frame.
void dtufoot_creature(object *obj);

#endif
