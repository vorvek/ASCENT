/*
 * ASCENT: the session log of the game.
 *
 * Each run writes a log in the directory "logs" of the directory of the
 * user (%APPDATA%\ASCENT-DTU\logs for an installed game, the -userdir
 * directory or the current directory for a development run). The log has
 * the version, the system, the settings, the main events of the game, the
 * messages of the data loaders (dtu_log), Error(), Warning() and Int3(),
 * and a crash report. The platform layer writes the file (plat_log.c).
 * docs/debug_logs.md describes the log for the players and the developers.
 *
 * The option -nolog turns the log off. The test option -testcrash N [KIND]
 * makes a crash at game frame N: KIND is null (the default, a write to the
 * address 0), abort, error, overflow (a stack overflow) or divide.
 */
#ifndef _DTULOG_H
#define _DTULOG_H

// Open the log and write the start lines. Call it after dtu_init_dirs().
void dtulog_start(void);
// The display, the window and the OpenGL renderer. Call it after plat_init.
void dtulog_video(void);
// One event of the game (a line in the log).
void dtulog_event(const char *format, ...);
// The character of the player: name, race, class and level. why says when.
void dtulog_character(const char *why);
// A message of dtu_log() (dtufile.c). The number of these lines is limited.
void dtulog_message(const char *text);
// Each game frame (dtu_frame_done): the test option -testcrash.
void dtulog_frame(int frame);

#endif
