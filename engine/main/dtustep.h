/*
 * ASCENT: new file. Dtu_fixed_step: the game runs in steps of 1/60 second
 * and the picture shows a state between the last two steps. See dtustep.c
 * and docs/re/physics.md, section 14.
 */

#ifndef _DTUSTEP_H
#define _DTUSTEP_H

#include "fix.h"

#define DTU_STEP_TIME		(F1_0/60)	// the time of one step of the game
#define DTU_STEP_MAX		8			// the most steps for one picture; more time is dropped

// 1 (default): the fixed step. 0 (-nofixedstep, -origphys): one step for
// each picture with the time of the picture, as the executable. The test
// option -fixedfps N also gives one step for each picture, with 1/N second.
extern int Dtu_fixed_step;
// The place of the next picture between the last two steps, 0 to 1
extern fix Dtu_step_alpha;
// 1 while the frames use the fixed step (game.c)
extern int Dtu_step_running;

void dtu_step_reset(void);
void dtu_step_store(void);
void dtu_step_view_begin(void);
void dtu_step_view_end(void);

#endif
