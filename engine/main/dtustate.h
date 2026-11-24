/*
 * ASCENT: new file. The level state files of Descent to Undermountain
 * (the state of each level that the player has left, and the copies of
 * these files in the save games). It replaces the code at 0x5A756,
 * 0x5AA75, 0x5ABC6, 0x5AC6F, 0xBAD08, 0xBB1A7, 0xBC1EC and 0xBC51D of
 * DTUGAME.EXE 1.3. See docs/re/levelstate.md.
 *
 * The files are in the directory SAVE of the directory of the user. The
 * port never writes into the directories of the game data. The files have
 * the format of the executable, so the saves of the original and of the
 * port go both ways (docs/re/levelstate.md, docs/formats/misc.md).
 */
#ifndef _DTUSTATE_H
#define _DTUSTATE_H

#include <stdio.h>

// ASCENT: a file of the directory of the user (rel "SG0.SG" or
// "SAVE/SG1_0.LVL"), found with no regard to the case of the letters.
// dtu_user_find writes the path and returns 1 if the file exists; else the
// path is the name as given (in the directories that exist).
int dtu_user_find(const char *rel, char *out, int size);
FILE *dtu_user_fopen(const char *rel, const char *mode);
void dtu_user_remove(const char *rel);

// A new game (0x5AC6F("CSUF.ls") and 0x5ABC6): deletes the state of the
// running game and writes an empty level table.
void dtustate_new_game(void);

// 0x5A756: writes the state of the level (index of DTU.MSN, 0 = YPINN)
// and marks it in the level table. Returns 1 if the state was written.
int dtustate_save_level(int level_index);

// 0x5AA75: reads the state of the level if the level table has one.
// Returns 1 if a state was read. The level must be loaded.
int dtustate_restore_level(int level_index);

// 0x5A0F6: closes the moving doors and the open doors and clears the
// flag "door opened" of all walls. The warp calls it before it saves.
void dtustate_close_doors(void);

// 0xBC1EC: the level table and the level files of save slot S (the slot
// of the menu plus 1). Returns 1 if successful.
int dtustate_save_slot(int s);

// 0xBC51D: the state of save slot S becomes the state of the running
// game. Returns 1 if the slot has a level table.
int dtustate_load_slot(int s);

// ASCENT: the three lists of a save game in a level (SG file offset 7374:
// the Descent 1 triggers, the matcens, the fuel centers). The load of a
// save gives them (NULL: none); the level start applies them. The writer
// returns the number of bytes it wrote into b (0 if size is too small).
void dtustate_set_save_lists(const unsigned char *data, int size);
int dtustate_write_save_lists(unsigned char *b, int size);

// Calls of the game code (warp, start of a game).
void dtustate_warp_leave(int level_index);		// before the load of the new level
void dtustate_warp_enter(int level_index);		// after the load
void dtustate_game_started(int level_num);		// StartNewGame (0x4DD5A): level_num is 1 for YPINN

// The loading picture (0x7C1E5) before the tables of a level are loaded.
// gameseq.c calls it before dtu_load_level_data(). dtustate_loading_end()
// ends a picture that is shown (0x7C2BB).
void dtustate_level_loading(char *level_name);
void dtustate_loading_end(void);
// 1 while the loading picture is on the screen (its palette is loaded).
int dtustate_loading_picture_shown(void);

// The test option -statetest (see dtustate.c). The front end calls it in
// each frame of the game.
void dtustate_test_frame(void);

// D-134: a restored creature (0xAE3F2 at 0xBB1A7) gets the spell points of
// its type again. 0: as 1.3. 1 (default): it keeps the saved points.
// -origobj clears it.
extern int Dtu_fix_restore_spells;
// The cast of the player at a save: 0: as 1.3, the save starts it again
// (0x164B3 after the write of 0x153EBC). 1 (default): it runs on.
// -origobj clears it. A load gives back the cast of the save in both.
extern int Dtu_fix_save_cast;

#endif
