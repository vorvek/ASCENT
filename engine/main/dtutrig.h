/*
 * ASCENT: new file. The level mechanics of Descent to Undermountain that
 * replace the Descent 1 triggers of switch.c: triggers (NAME.TIG), traps
 * (NAME.TAP), trigger messages (NAME.SAY), the use action of the player
 * and the level changes of DTU.WRP ("warps").
 *
 * The rules come from DTUGAME.EXE (traps.c, mission.c, wall.c of the
 * executable). See docs/re/triggers.md for the addresses and for what is
 * verified. The file formats are in docs/formats/tig.md, tap.md, say.md
 * and misc.md.
 */

#ifndef _DTUTRIG_H
#define _DTUTRIG_H

#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "segment.h"
#include "object.h"

//------------------------------------------------------------------------
// Limits of the executable

#define DTU_MAX_TRIGGERS			254		// the loader accepts 1..254
#define DTU_MAX_TRAPS				1024		// the loader accepts 0..1024
#define DTU_TRIG_SEG_TABLE			900		// segment table of the TIG file
#define DTU_MAX_ACTIVE_TRAPS		32			// list at 0x3A7CD0
#define DTU_TRIG_CHAIN_LIMIT		50			// steps in a chain of triggers or traps

// The activation type that the caller passes. A trigger fires when
// (type & activation_mask) is not 0.
#define DTU_ACT_USE			0x01		// the use action of the player (0x95050)
#define DTU_ACT_ROBOT		0x02		// a monster touches the side, or a monster script names it
#define DTU_ACT_WEAPON		0x04		// a missile touches the side
#define DTU_ACT_PLAYER		0x08		// the player touches the side, or enters the segment (floor, side 3)
#define DTU_ACT_DEBRIS		0x10		// an object of type 8 touches the side
#define DTU_ACT_FORCE		0xff		// the thief skill failed: the trigger fires

// Trigger flags (offset 0x13)
#define DTU_TGF_OCCUPANCY	0x0001	// counts the players inside
#define DTU_TGF_ENTER		0x0002	// with OCCUPANCY: 1 = a player enters, 0 = a player leaves
#define DTU_TGF_TOGGLE_TEX	0x0004	// each firing switches the side between tex_on and tex_off

// Trap flags (offset 0x0F)
#define DTU_TPF_OPEN_DOOR		0x00000001
#define DTU_TPF_CLOSE_DOOR		0x00000002
#define DTU_TPF_TOGGLE_LOCK	0x00000008
#define DTU_TPF_WALL_CLOSED	0x00000010
#define DTU_TPF_WALL_OPEN		0x00000020
#define DTU_TPF_REPEAT			0x00000040	// after the first run the trap runs each frame
#define DTU_TPF_ACTIVE			0x00000080	// runtime: the trap is in the list of active traps
#define DTU_TPF_TOGGLE_WALL	0x00000100
#define DTU_TPF_STATE			0x00000200	// runtime: toggle state
#define DTU_TPF_LIGHT_DONE		0x00000400	// runtime: light_delta is applied
#define DTU_TPF_PLAYER_INSIDE	0x00000800	// runtime: << player number
#define DTU_TPF_RECHARGE		0x00008000
#define DTU_TPF_DELAY_RUNS		0x00010000	// runtime: the delay runs

typedef struct dtu_trigger {
	short			unk00;
	vms_vector	pos;
	ubyte			on;
	uint			unk0f;				// the thief skills use the bytes 0x0F and 0x10
	ushort		flags;
	ushort		activation_mask;
	short			unk17;
	short			need_item_class, need_item_id;
	short			unk1d;
	short			lack_item_class, lack_item_id;
	ubyte			take_item;
	ushort		player_mask_a, player_mask_b;
	short			sound;
	short			tex_on, tex_off;
	short			fire_count;
	uint			unk30;
	ubyte			skip_count;
	short			cond35, cond37, cond39;
	short			first_trap;
	short			enable_trigger, enable_trap;
	short			disable_trigger, disable_trap;
	ubyte			object_num;
	short			seg;
	byte			side;
	short			next_in_seg;
	short			say_index;
} dtu_trigger;

typedef struct dtu_trap {
	short			id;
	ubyte			on;
	short			charges_init, charges;
	short			sound;
	short			next_trap;
	uint			flags;
	short			weapon_type;
	vms_vector	pos;
	vms_vector	dir;					// unit vector at 0x21. Not read by the traced code.
	short			spawn_object;
	short			stat_delta;
	short			robot_type, robot_arg;
	short			tex_disabled, tex_enabled;
	short			light_delta;
	ushort		delay_init;
	short			seg, side;
	ushort		interval, delay;	// in clock units, see dtu_trap_clock()
	uint			timestamp;			// clock of the last run, 0xFFFFFFFF at the start
} dtu_trap;

extern dtu_trigger	Dtu_triggers[DTU_MAX_TRIGGERS];
extern int				Dtu_num_triggers;
extern short			Dtu_trigger_seg_table[DTU_TRIG_SEG_TABLE];
extern dtu_trap		Dtu_traps[DTU_MAX_TRAPS];
extern int				Dtu_num_traps;

//------------------------------------------------------------------------
// Settings

// 1 (default): the segment table and the chains of the TIG file are made
// again from the seg field of the trigger records. Seven shipped files have
// a table that does not agree with the records, so some triggers cannot
// fire in the original. 0: the table and the chains of the file.
extern int Dtu_fix_trigger_table;

// The executable multiplies the trap times of the TAP file (seconds) by
// 18.2 and compares the result with a clock that counts 1/100 second. A
// time of 10 seconds is 1.82 seconds in the game. 0 (default): this rule.
// 1: the times are seconds.
extern int Dtu_fix_trap_seconds;

// The trap action "close door" of the executable passes a wall number to
// a function that takes a number of the list of moving doors. No shipped
// trap has this action. 1 (default): the action starts the closing of the
// door of the trap. 0: the action does nothing (the original call damages
// the list of moving doors, which the port does not copy).
extern int Dtu_fix_trap_close_door;

// The executable looks at the warp line of the segment of the player
// when the segment changes. After a command line (music, ambient sound)
// it forgets the segment, so the line acts again in each frame, and with
// it the floor triggers of the segment: a message trigger that does not
// switch itself off adds its message in each frame. 1 (default): the
// floor triggers of such a segment fire one time when the player enters
// it, as in a segment with no line. 0: the rule of the executable.
extern int Dtu_fix_trigger_repeat;

// 1: each trigger and trap event is printed to stdout (option -triglog).
extern int Dtu_trig_log;

//------------------------------------------------------------------------
// Functions

// Loads NAME.TIG, NAME.TAP, NAME.SAY and the lines of DTU.WRP for the
// level. level_index is the index in the mission file (0 = ypinn).
void dtu_trig_load_level(char *level_name, int level_index);

// 0x939AD: fires the triggers on a side. obj is the object that acts.
void dtu_trig_check(int segnum, int side, int type, object *obj);

// 0x95019: the same for an object that touches a side or stands on a
// floor. A floor (side 3) counts only for the segment of the object.
void dtu_trig_touch(int segnum, int side, int type, object *obj);

// 0x29966: called by collide_object_with_wall(). The type comes from the
// type of the object.
void dtu_trig_object_hit_wall(object *obj, int segnum, int side);

// 0x93B29: fires one trigger.
void dtu_trig_fire(int trigger_num, int type, object *obj);

// 0x94363: runs a trap and the traps after it in the chain.
void dtu_trap_run(int trap_num, object *obj);

// Each frame: the active traps (0x9547A), then the segment change of the
// player (0x69BC8: warp, floor triggers).
void dtu_trig_frame(void);

// 0x95050: the use action of the player ("Activate", default key A). The
// caller calls it while the key is down. The function acts one time in
// each half second.
void dtu_player_use(void);

// The clock of the trap times: 1/100 second units of the game time.
uint dtu_trap_clock(void);

// The message of a trigger (0x94D8B). Returns the text or NULL.
char *dtu_trig_say_text(int say_index);

// The warp of a segment of the current level. Returns 1 and writes the
// values when the segment has a line in DTU.WRP.
int dtu_warp_lookup(int segnum, int *dest_level, int *dest_seg, int *value);

//------------------------------------------------------------------------
// Hooks. Systems that the port does not have yet (inventory, weapons,
// object creation, monsters, damage). dtutrig.c has a weak definition of
// each hook that prints the call when Dtu_trig_log is set. Another file
// can define the function without the weak attribute.

// 0xB6BB3: 1 if the player holds the item (class, id). Default: 0.
int dtu_hook_player_has_item(int player_num, int item_class, int item_id);

// 0xB6D0C: the trigger takes the item that it needs.
void dtu_hook_player_take_item(int item_class, int item_id);

// The two player masks of a trigger (tables at 0x142220 and 0x142230,
// indexed by values of the player character). Return 0 to stop the
// trigger. Default: 1.
int dtu_hook_player_masks_ok(int player_num, int mask_a, int mask_b);

// 0xABBE5(object, 3): the trigger names an object (object_num > 1).
void dtu_hook_trigger_object(int objnum, int arg);

// 0x946AE: the trap creates a weapon or spell effect at its position.
void dtu_hook_trap_weapon(int weapon_type, int segnum, int side, vms_vector *pos, vms_vector *dir);

// 0x946FA: the trap creates an object from entry n of the item table.
void dtu_hook_trap_spawn_object(int item_num, int segnum, int side, vms_vector *pos);

// 0x94D61: the trap gates in a monster. If robot_arg is below 4 the
// executable calls 0xAC141 in place of gate_in_robot (0x1374D).
void dtu_hook_trap_gate_robot(int robot_type, int robot_arg, int segnum, vms_vector *pos);

// 0x94598: the trap adds delta (whole points) to the hit points of the
// player object and clamps the result to 0 and to the maximum.
void dtu_hook_trap_hit_points(object *obj, int delta);

// 0x9493A: a floor (side 3) of the segment opened: the objects of the
// segment fall.
void dtu_hook_drop_objects(int segnum);

// 0x951DE: the use action on a side that is no door (climb), and 0x7EDFA:
// the use action with no side in reach (objects).
void dtu_hook_use_wall(object *obj, int segnum, int side);
void dtu_hook_use_nothing(object *obj);

// 0x5B3AC: a command line of DTU.WRP (dest_level -2): code 0 leaves the
// dungeon, 1..19 are mission checks, the others start and stop music and
// ambient sound.
void dtu_hook_warp_command(int code, int value);

// The conditions of a warp that the port does not have (gold for the
// passage between the inn and the dungeon, the check at 0x59D2D). Return
// 0 to stop the warp. Default: 1.
int dtu_hook_warp_allowed(int from_level, int to_level);

#endif
