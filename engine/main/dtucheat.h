/*
 * ASCENT: new file. The cheat codes of DTUGAME.EXE 1.3 (key function
 * 0x42F9C: 0x4324E and 0x428B1 to 0x42BED). See dtucheat.c.
 */
#ifndef _DTUCHEAT_H
#define _DTUCHEAT_H

// Each key of the game (the key code with its modifiers) goes here once.
void dtu_cheat_key(int key);

// D-123: 1 (default): a cheat code clears its index after it acted, so
// it can be typed again at once. 0 (-origui): five codes keep it (1.3).
extern int Dtu_fix_cheat_index;

#endif
