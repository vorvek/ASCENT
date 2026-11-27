/*
 * ASCENT: new file. The phylactery of Morik Stormhand, the lich of CLAN3
 * (object 10, type Lich3, script 190). It is not in 1.3. See dtulich.c and
 * docs/re/lich.md.
 *
 * The phylactery is an altar of Velsharoon in the empty crypt cell of
 * segment 557 of the east wing, with a Dispel Magic scroll in front of it.
 * When the player breaks it, the lich starts his fights without his
 * contingency (Stone Skin, Shield, Minor Globe of Invulnerability, Mirror
 * Image: the caster AI, dtucaster.h), and a contingency that runs ends at
 * once.
 *
 * All of it is behind one setting: the line "The phylactery of the lich"
 * of the group "Restored and new content" of the Improvements menu
 * (Dtu_lich_phylactery, dtuopts.h), on by default. The options -origobj and
 * -nophylactery clear it for one run.
 */
#ifndef _DTULICH_H
#define _DTULICH_H

#include "object.h"

// The global flags of the player record (dtu_script_gflag), saved with the
// game. The scripts of 1.3 use 1 to 160, the port 240 to 246 (dtugear.h,
// dtudragon.c).
#define DTU_LICH_GF_BROKEN	247		// the phylactery is broken
#define DTU_LICH_GF_PLACED	248		// the phylactery and its scroll are in the level

// 1 if the setting is on
int dtu_lich_on(void);

// bmread.c, at the end of the load of the tables of a level: the scenery
// line of the phylactery for CLAN3 (after the 33 lines of its table),
// read with the reader of the tables (parse_line). The line is there with
// the setting off too, so that a level state with the object keeps its
// model.
void dtu_lich_tables(const char *level_name, int n_dressing, void (*parse_line)(char *line));

// dtu_objects_level_setup(), each load and each restore of a level: the
// phylactery and the scroll, once in a game (DTU_LICH_GF_PLACED), while the
// lich lives. has_state: the level has a saved state; restore: the state
// was read.
void dtu_lich_level_setup(int has_state, int restore);

// dtu_break_dressing() (dtuobj_scenery.c): a piece of scenery breaks
void dtu_lich_dressing_broken(object *d, object *by);

// 1 if the phylactery is broken (the setting on)
int dtu_lich_phylactery_destroyed(void);

// The caster AI (dtuspell.c, the start of a fight): 1 if the caster is the
// lich of CLAN3 and its phylactery is broken: no contingency
int dtu_lich_no_contingency(object *obj);

// dtuscript.c, a string of a script: the text of the port for the string at
// offset of the record of script (en: the text of the record), or NULL.
// Barander (script 170) tells of the phylactery; the lich (script 190)
// knows that it is broken.
const char *dtu_lich_script_text(int script, int offset, const char *en);

// dtutrig.c, a message of the level: the text of the port for message n of
// the level, or NULL. CLAN3 message 1 (the arrival) points to the east
// crypts.
const char *dtu_lich_say_text(const char *level, int n);

// The test command phylactery=N (dtuchar.c): 0 breaks it (as a blow of the
// player), 1 logs its state, 2 puts the player in front of it
void dtu_lich_test(int n);

#endif
