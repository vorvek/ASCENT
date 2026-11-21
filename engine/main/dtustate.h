/*
 * ASCENT: new file. The level state files of Descent to Undermountain
 * (the state of each level that the player has left, and the copies of
 * these files in the save games). It replaces the code at 0x5A756,
 * 0x5AA75, 0x5ABC6, 0x5AC6F, 0xBAD08, 0xBB1A7, 0xBC1EC and 0xBC51D of
 * DTUGAME.EXE 1.3. See docs/re/levelstate.md.
 *
 * The files are in the directory SAVE under the current directory. The
 * port never writes into the directories of the game data.
 */
#ifndef _DTUSTATE_H
#define _DTUSTATE_H

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

// Calls of the game code (warp, start of a game).
void dtustate_warp_leave(int level_index);		// before the load of the new level
void dtustate_warp_enter(int level_index);		// after the load
void dtustate_game_started(int level_num);		// StartNewGame (0x4DD5A): level_num is 1 for YPINN

// The loading picture (0x7C1E5) before the tables of a level are loaded.
// gameseq.c calls it before dtu_load_level_data(). dtustate_loading_end()
// ends a picture that is shown (0x7C2BB).
void dtustate_level_loading(char *level_name);
void dtustate_loading_end(void);

// The test option -statetest (see dtustate.c). The front end calls it in
// each frame of the game.
void dtustate_test_frame(void);

#endif
