/*
 * ASCENT: new file. The footstep sounds of the player (dtufoot.c).
 */
#ifndef _DTUFOOT_H
#define _DTUFOOT_H

// 1 (the default): the player has footstep sounds. 0: none.
extern int Dtu_footsteps;

// Call it one time on each frame, after the objects have moved.
void dtufoot_frame(void);

#endif
