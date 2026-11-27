/*
 * ASCENT: new file. The encounter with Tamaramagil Swiftclaw, the adult
 * red dragon of DROW2. It is not in 1.3: the game has her creature type
 * ("Dragon", dragn2.pof), her dialogue (script 210) and the messages of
 * the level, but no object for her. See docs/re/dragon.md.
 *
 * All of it is behind one setting: the line "The dragon Tamaramagil" of
 * the group "Restored and new content" of the Improvements menu
 * (Dtu_restored_dragon, dtuopts.h), on by default. The options -origobj
 * and -origtamaramagil clear Dtu_fix_tamaramagil for one run. With the
 * setting off DROW2 is as 1.3 ships it.
 */
#ifndef _DTUDRAGON_H
#define _DTUDRAGON_H

#include "object.h"

// Her name in the messages of the game (the HUD lines of a hit, the kill
// line): the type name of the text file is "Red Dragon". She says her name
// in her first speech, and the lines show it from the start.
#define DTU_DRAGON_NAME	"Tamaramagil"

// 1 if the encounter is on (the menu setting and Dtu_fix_tamaramagil)
int dtu_dragon_on(void);

// 1 if the object is Tamaramagil: a creature with script 210 in DROW2,
// with the encounter on
int dtu_dragon_is(object *obj);

// dtu_objects_level_setup(), each load and each restore of a level: the
// model scale and the type values, her object, the hatchlings, the names
// of the keys. has_state: a restore of the level state follows.
void dtu_dragon_level_setup(int has_state);

// dtu_trig_state_rules(), DROW2, after each load of the triggers and after
// each restore of a level state: the lever (trigger 20), the doors, the
// key in the cells and the treasure. restore: 1 after a restore.
void dtu_dragon_level_rules(int restore);

// object.c, the AI of a creature. Returns 1 if the object was handled
// here (Tamaramagil while she lives, a hatchling behind the closed door of
// the hatchery) and the AI of the game must not run for it.
int dtu_dragon_ai_frame(object *obj);

// object.c, the movement of a creature. Returns 1 if the object does not
// move by the creature physics of the game (Tamaramagil while she lives).
int dtu_dragon_no_physics(object *obj);

// dtu_objects_frame(), each frame: the lever, the key of the alcove, the
// door of the hoard, the stream of her breath.
void dtu_dragon_frame(void);

// dtu_draw_polygon_object(): the frame that is drawn. It is the frame of
// the object: at peace she stands still in the first frame of STAND.
int dtu_dragon_draw_frame(object *obj, int frame);

// apply_spell() of dtuspell.c, before the effect of a spell on a creature:
// the magic resistance of the target, a percent roll against each spell
// that another object casts on it. Returns 1 if the spell has no effect.
int dtu_dragon_spell_resisted(object *target, object *caster, int spell);

// text() of dtuscript.c: the English text that replaces the string at
// offset of the record of script, or NULL. The caller translates it.
const char *dtu_dragon_script_text(int script, int offset);

// The place where the item that she carries drops (the Dragon Key): on the
// floor in front of her. Returns 0 if the object is not her.
int dtu_dragon_drop_place(object *obj, vms_vector *pos, int *seg);

// Test (-chartest, command "dragon"): 1 claws, 2 bite, 3 breath as her
// next action, 4 the log line of her state, 5 hostile now
void dtu_dragon_test(int what);

#endif
