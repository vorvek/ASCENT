/*
 * ASCENT: new file. The scripts of Descent to Undermountain (dialogue.c of
 * DTUGAME.EXE 1.3): the interpreter of the GABx.DAT records, the global
 * flags, the 19 mission flags and the mission switch of Khelben
 * (frontend.c, mission.c). Descent 1 has nothing like it.
 *
 * The facts are in docs/re/scripts.md. dtudlg.c draws the conversation
 * window. tools/script.py disassembles the scripts with the same grammar.
 */
#ifndef _DTUSCRIPT_H
#define _DTUSCRIPT_H

#include "types.h"
#include "object.h"

//------------------------------------------------------------------------
// Settings

// 1 (default): a new character starts with all global flags cleared. The
// executable clears them only at the program start (0xAD8E6), so a second
// new game in one session kept the flags of the first character.
extern int Dtu_fix_script_new_char;

// 1: each statement and each event of the scripts is printed to stdout
// (option -scriptlog).
extern int Dtu_script_log;

//------------------------------------------------------------------------
// Run modes of run_script (0xABBE5)

#define DTU_RUN_TALK		0		// the player uses the object
#define DTU_RUN_STATUS	1		// the status line (no caller in the executable)
#define DTU_RUN_PATH		2		// the object reached the end of its path
#define DTU_RUN_TRIGGER	3		// a trigger names the object
#define DTU_RUN_CALL		4		// statement EE of another script

// Reads GABx.RLH and GABx.DAT (the letter of the language). Called by
// dtu_script_level_start(); safe to call again.
void dtu_script_init(void);

// ASCENT: the contents of GABE.RLH and GABE.DAT (dtu_script_init loads
// them; they stay until the exit). Returns 0 if they are not available.
int dtu_script_gab_files(ubyte **rlh, int *rlh_size, ubyte **dat, int *dat_size);

// At the start of a level, after the objects are set up. Ends a script
// that still waits.
void dtu_script_level_start(void);
// The test option -scriptflags before the first creature setup of the
// level start (dtuobj.c reads quest flag 67). It runs one time.
void dtu_script_test_flags(void);

// run_script(object, mode) (0xABBE5). Returns 1 if the script started.
// The script may wait for the player (a reply, statement EC); it goes on
// in dtu_script_frame().
int dtu_script_run(object *obj, int mode);

// run_script_id(0, id) (0xAC055): runs a global script (0x1FC to 0x1FF)
// with a temporary object at the player.
void dtu_script_run_id(int id);

// The talk of the use action (0xAC155 from step 3 on): obj is the object
// that the use ray found. Returns 1 if the use action was taken.
int dtu_script_talk(object *obj);

// 1 while a script waits (the world does not move).
int dtu_script_waiting(void);

// ASCENT: test commands of -chartest (tools/questtest.py)
void dtu_script_test_warp(int level, int seg);
void dtu_script_test_prison(void);

// Called by GameLoop after the controls and the clock. Reads the keys of
// the conversation window and goes on with a waiting script. Returns 1
// while a script waits: the caller does not move the objects in this frame.
int dtu_script_frame(void);

//------------------------------------------------------------------------
// Global flags (256 bits at P+0x1A33) and mission flags (0x13CDD4)

int dtu_script_gflag(int n);					// 0xADAFE
void dtu_script_set_gflag(int n, int value);	// 0xADB3B

#define DTU_N_MISSION_FLAGS	19

// Mission flag k (1 to 19), see docs/re/scripts.md section 7.
int dtu_mission_flag(int k);
void dtu_mission_set_flag(int k, int value);

// The command k (1 to 19) of DTU.WRP (0x5B3AC): the check of mission k.
void dtu_mission_check(int k);

// 0x83E36: the visit of Khelben. Advances the mission while the switch
// 0x83EF5 allows it and gives the rewards (0x844ED). Sets *blurb to 0 when
// the mission changed. Returns the new mission number.
int dtu_mission_advance(int mission, int *blurb);

// MakePlayer (0x8BEBC): a new character. Clears the mission flags
// (0x5ADC9), and the global flags with Dtu_fix_script_new_char.
void dtu_mission_new_character(void);

// Save games (docs/formats/misc.md, SG0.SG): writes the 19 mission flags
// (offset 57) and the global flags (offset 141 + 0x1A33) into the file
// image, and reads them from a save file.
void dtu_mission_write_save(ubyte *sg);
void dtu_mission_read_save(const char *path);

//------------------------------------------------------------------------
// Weak hooks for systems of other files. dtuscript.c has a weak definition
// that prints the call.

// 0x879A7(n): the stock file n of the market is added (a reward of Khelben).
void dtu_hook_market_add_stock(int n);

#endif
