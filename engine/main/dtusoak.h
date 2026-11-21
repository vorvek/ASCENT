/*
 * ASCENT: new file. The soak test of the port (option -soak SEED), see
 * dtusoak.c. It has no part in DTUGAME.EXE.
 */
#ifndef _DTUSOAK_H
#define _DTUSOAK_H

// 1 while the soak test runs
extern int Dtu_soak;

// Reads the options. Called one time before the first frame.
void dtu_soak_init(void);
// Called after each game frame: the checks and the next action.
void dtu_soak_frame(void);
// The summary line at the end.
void dtu_soak_done(void);

#endif
