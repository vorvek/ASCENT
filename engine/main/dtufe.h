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

// Plays the start movies (dtumovie_title) and shows the title picture.
void dtufe_title(void);

// Runs the front end until the player enters the dungeon or leaves the
// program. Returns the number of the level to start (1 is the first level
// of the mission), or 0 to leave the program. The graphics, the fonts, the
// tables and the sound must be loaded.
int dtufe_run(void);

// The game calls it after each frame. It asks "Leave the Yawning Portal?"
// after the player entered the exit of the inn (command 0 of DTU.WRP).
// Returns 1 if the game must end and the front end must run: the player
// leaves the inn, or the player is dead. The front end defines the hooks
// dtu_hook_warp_command (dtutrig.h) and dtu_hook_player_death_screen
// (dtuobj.h) for this.
int dtufe_game_frame(void);

// Call it after game() has returned. It shows the death screen if the
// player is dead. Returns 1 if the front end must run again, and 0 if the
// program must stop (the window was closed, or the option -frames).
int dtufe_game_ended(void);

// The direct start (-level NAME) calls this before the game, so that the
// death of the player goes to the death screen and the death menu.
void dtufe_direct_start(void);

// The script of -fescript for the menus of newmenu.c. nm_show_frame() calls
// it. Returns 0 after the "quit" command of the script.
int dtufe_menu_script_frame(void);

// The Escape key in the game (game.c). Returns 1 if the front end shows its
// ESC menu (0x4225A) after this frame, 0 if the game must do what Descent 1
// does.
int dtufe_escape_key(void);

// ASCENT: the keys Alt+F2 (save) and Alt+F3 (load) in the game. Return 1 if
// the front end shows its slot menu after this frame, as Save Game and Load
// Game of the ESC menu, else 0.
int dtufe_save_key(void);
int dtufe_load_key(void);

// The loading pictures (0x7C1E5, 0x7C2BB). dtufe_loading_picture shows
// FEBGND entry "entry" over the screen of the game and returns 1 if it is
// shown. dtufe_loading_picture_end fades it out and gives the screen back
// to the game. dtufe_loading_from_frontend returns 1 once after the front
// end started the Yawning Portal (0x13E324), 2 once after it started a
// level of a save game, else 0.
int dtufe_loading_picture(int entry);
void dtufe_loading_picture_end(void);
int dtufe_loading_from_frontend(void);

// The menu "Improvements" (dtuopts.h) in the game, with newmenu. The ESC
// menu and the options menu of F2 (menu.c) open it. It writes the
// configuration file when it closes.
void dtufe_improvements_menu(void);

// The menu "Graphics" (dtuopts.h) in the game, with newmenu. The ESC menu,
// the options menu of F2 (menu.c) and the key F5 open it. Escape and F5
// close it. It writes the configuration file when it closes.
void dtufe_graphics_menu(void);

// The key F5 in the game: returns 1 and opens the Graphics menu after the
// frame when the front end runs the game, else returns 0.
int dtufe_graphics_key(void);

#endif
