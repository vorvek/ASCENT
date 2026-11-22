/*
 * ASCENT: new file. The thief skill point screen (thiefpts.c of
 * DTUGAME.EXE 1.3, 0xB4419 to 0xB611B). Descent 1 has nothing like it. See
 * dtuthiefpts.c and docs/re/combat.md, section B7.
 */
#ifndef _DTUTHIEFPTS_H
#define _DTUTHIEFPTS_H

// 0xB5B49: with skill points (Dtu_char.thief_points) and a base skill
// below 95, the choice box "You have some thief skill points
// available. Spend them Now?"; Yes opens the screen (0xB5BB9). Returns 1
// if the screen was shown and closed, else 0.
int dtu_thief_points_offer(void);

#endif
