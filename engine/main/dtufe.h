/*
 * ASCENT: new file. The front end of Descent to Undermountain: the title
 * picture, the main menu, the character generation, the Waterdeep screens
 * and the death screen. It replaces the title screens and the menus of
 * Descent 1 (titles.c, menu.c) when the program starts without -level.
 *
 * The original code is in frontend.c, feanim.c, fehelp.c, feinput.c,
 * felz.c, femarket.c, fenuchar.c, ferbtn.c, fetext.c, fetorch.c and
 * fewdeep.c of DTUGAME.EXE. docs/re/frontend.md has the addresses and
 * says which facts are verified.
 */
#ifndef _DTUFE_H
#define _DTUFE_H

// 0 (default): a key or a mouse button acts one time for each press, and
// a held button repeats with a delay only on the controls where that is
// useful (portrait arrows, sliders). 1: as the original, the action of a
// control runs again in each frame while the button is down. The option
// -feoriginput sets it to 1.
extern int Dtu_fe_original_input;

// The game sets this to 1 when the player leaves the dungeon through the
// exit of the Yawning Portal (command 0 of DTU.WRP). The front end then
// shows Waterdeep. If it is 0 the front end shows the main menu.
extern int Dtu_fe_return_waterdeep;

// Shows the title picture. The movies of the original are not played.
void dtufe_title(void);

// Runs the front end until the player enters the dungeon or leaves the
// program. Returns the number of the level to start (1 is the first level
// of the mission), or 0 to leave the program. The graphics, the fonts, the
// tables and the sound must be loaded.
int dtufe_run(void);

// Call it after game() has returned. It shows the death screen if the
// player is dead. Returns 1 if the front end must run again, and 0 if the
// program must stop (the window was closed, or the option -frames).
int dtufe_game_ended(void);

#endif
