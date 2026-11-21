/*
 * ASCENT: access to the Descent to Undermountain data files.
 *
 * The game data can be in more than one directory: the installed game
 * (with the files of the 1.3 patch) and the DTU directory of the disc.
 * The disc stores levels, archives and tables in packed form
 * (NAME.DOG, NAME.COW, NAME.HEN). See docs/formats/packed.md.
 */
#ifndef _DTUFILE_H
#define _DTUFILE_H

#include "types.h"

// Add a directory to the end of the search list.
// The first directory that has a file supplies that file.
void dtu_add_data_dir(const char *path);

// ASCENT: the directories. dtu_init_dirs() reads -datadir (each one is a
// data directory), -userdir and -debug. Without -datadir the game is an
// installed game: the data directory is "data" next to the executable and
// the files of the player (ascent.cfg, the save games, the SAVE directory
// of the level states, the market stock) go to the directory of the user
// (%APPDATA%/ASCENT on Windows). With -datadir they stay in the current
// directory. -userdir DIR changes the directory of the user.
void dtu_init_dirs(void);
// The directory of the executable of an installed game, or NULL.
const char *dtu_install_dir(void);
// The directory of the user, or NULL for the current directory.
const char *dtu_user_dir(void);
// The path of a file of the player: name in the directory of the user.
char *dtu_user_path(char *out, int size, const char *name);
// 1 in the debug build or with -debug: the messages for the developers.
extern int Dtu_debug;

// Find a file in the search list. The name match ignores case.
// Writes the full path to path_out. Returns 1 if the file exists.
int dtu_find_file(const char *name, char *path_out, int path_size);

// Load a complete file into memory that the caller releases with free().
// For NAME.LVL, NAME.EGG and NAME.BIN, each directory is examined for the
// plain file first and then for the packed file (NAME.DOG, NAME.COW,
// NAME.HEN), which is unpacked. For NAME.LVL, a NAME.LV2 file in the same
// directory has priority (the 1.3 patch levels).
// Returns NULL if no directory has the file. Writes the size to size_out.
ubyte *dtu_load_file(const char *name, int *size_out);

// Unpack the LZSS data of a packed file. Returns memory that the caller
// releases with free(), or NULL if the data is not valid.
ubyte *dtu_unpack(const ubyte *src, int src_size, int *size_out);

// Messages of the data loaders (a file that is missing, a table line that
// the parser does not understand). dtu_log() adds one message to Dtu_log,
// a text that keeps the first messages since the last dtu_log_clear(). If
// Dtu_log_to_stderr is not 0, it also prints the message to stderr.
#define DTU_LOG_SIZE 16384
extern char Dtu_log[DTU_LOG_SIZE];
extern int Dtu_log_count;				// number of messages, also those that did not fit
extern int Dtu_log_to_stderr;
void dtu_log(const char *format, ...);
void dtu_log_clear(void);

#endif
