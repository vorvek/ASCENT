/*
 * ASCENT: new file. See dtutrig.h.
 *
 * It replaces the trigger code of switch.c (Descent 1 wall triggers) with
 * the triggers, traps, messages and warps of Descent to Undermountain.
 * The addresses in the comments are functions of DTUGAME.EXE 1.3.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "error.h"
#include "args.h"
#include "mono.h"
#include "segment.h"
#include "gameseg.h"
#include "gamemine.h"
#include "object.h"
#include "wall.h"
#include "fvi.h"
#include "game.h"
#include "gameseq.h"
#include "player.h"
#include "mission.h"
#include "digi.h"
#include "platform.h"
#include "sounds.h"
#include "text.h"
#include "mem.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtudisp.h"
#include "dtuobj.h"
#include "dtutrig.h"
#include "dtustate.h"
#include "dtulog.h"		// ASCENT: the session log
#include "dtuchar.h"		// the choice box and the character record (the well passage)

char *fe_text(int n);		// dtufe.c: Text_string[n]

#define DTU_WEAK __attribute__((weak))

// Sound numbers of the executable
#define SND_TRIGGER_STONE		3108		// default sound, side flag (pad & 3) == 2
#define SND_TRIGGER_DEFAULT	2001		// default sound, other sides
#define SND_SAY_AT_TRIGGER		20			// 0x94DA6
#define SND_SAY_MESSAGE			3001		// 0x94F6F

// The warp of a segment (4 bytes for each segment at 0x296494)
typedef struct dtu_warp {
	byte		dest_level;			// -1 no line, -2 command
	short		dest_seg;			// or the command code
	ubyte		value;				// facing for a warp, argument for a command
} dtu_warp;

dtu_trigger	Dtu_triggers[DTU_MAX_TRIGGERS];
int			Dtu_num_triggers = 0;
short			Dtu_trigger_seg_table[DTU_TRIG_SEG_TABLE];
dtu_trap		Dtu_traps[DTU_MAX_TRAPS];
int			Dtu_num_traps = 0;

int Dtu_fix_trigger_table = 1;
int Dtu_fix_trap_seconds = 0;
int Dtu_fix_trap_close_door = 1;
int Dtu_fix_trigger_repeat = 1;
int Dtu_trig_log = 0;

static short	Active_traps[DTU_MAX_ACTIVE_TRAPS];		// 0x3A7CD0
static int		Num_active_traps = 0;						// 0x14221C
static int		Trap_chain_stop = 0;							// 0x142248

static ubyte	*Say_data = NULL;
static int		Say_size = 0;
static char		Say_text[4096];								// 0x3A65C8
static char		Say_level[16];			// ASCENT: the name of the level of the SAY file, for the ids of the translations

static dtu_warp	Warps[MAX_SEGMENTS];
static int			Warps_enabled = 0;						// 0x13CE28
static int			Level_index = -1;
static int			Last_player_seg = -1;					// 0x13D41B
static int			Floor_trigger_seg = -1;					// see Dtu_fix_trigger_repeat
static fix			Last_use_time = -F1_0;					// 0x14224C
static int			Options_read = 0;

static void tlog(const char *format, ...)
{
	va_list args;

	if (!Dtu_trig_log)
		return;
	va_start(args, format);
	printf("TRIG: ");
	vprintf(format, args);
	printf("\n");
	va_end(args);
	fflush(stdout);
}

//------------------------------------------------------------------------
// Hooks

DTU_WEAK int dtu_hook_player_has_item(int player_num, int item_class, int item_id)
{
	tlog("hook player_has_item(player %d, class %d, id %d): no inventory, answer 0", player_num, item_class, item_id);
	return 0;
}

DTU_WEAK void dtu_hook_player_take_item(int item_class, int item_id)
{
	tlog("hook player_take_item(class %d, id %d)", item_class, item_id);
}

DTU_WEAK int dtu_hook_player_masks_ok(int player_num, int mask_a, int mask_b)
{
	if (mask_a != 0xff || mask_b != 0xff)
		tlog("hook player_masks_ok(player %d, mask_a 0x%x, mask_b 0x%x): no character, answer 1", player_num, mask_a, mask_b);
	return 1;
}

DTU_WEAK void dtu_hook_trigger_object(int objnum, int arg)
{
	tlog("hook trigger_object(object %d, arg %d)", objnum, arg);
}

DTU_WEAK void dtu_hook_trap_weapon(int weapon_type, int segnum, int side, vms_vector *pos, vms_vector *dir)
{
	tlog("hook trap_weapon(type %d, seg %d, side %d, pos %d %d %d)", weapon_type, segnum, side, f2i(pos->x), f2i(pos->y), f2i(pos->z));
}

DTU_WEAK void dtu_hook_trap_spawn_object(int item_num, int segnum, int side, vms_vector *pos)
{
	tlog("hook trap_spawn_object(item %d, seg %d, side %d, pos %d %d %d)", item_num, segnum, side, f2i(pos->x), f2i(pos->y), f2i(pos->z));
}

DTU_WEAK void dtu_hook_trap_gate_robot(int robot_type, int robot_arg, int segnum, vms_vector *pos)
{
	tlog("hook trap_gate_robot(type %d, arg %d, seg %d)", robot_type, robot_arg, segnum);
}

DTU_WEAK void dtu_hook_trap_hit_points(object *obj, int delta)
{
	tlog("hook trap_hit_points(object %d, delta %d)", obj ? (int)(obj-Objects) : -1, delta);
}

DTU_WEAK void dtu_hook_drop_objects(int segnum)
{
	tlog("hook drop_objects(seg %d)", segnum);
}

DTU_WEAK void dtu_hook_use_wall(object *obj, int segnum, int side)
{
	tlog("hook use_wall(seg %d, side %d)", segnum, side);
}

DTU_WEAK void dtu_hook_use_nothing(object *obj)
{
	tlog("hook use_nothing()");
}

DTU_WEAK void dtu_hook_warp_command(int code, int value)
{
	static int last_code = -1, last_value = -1, last_level = -1;

	// the executable calls this each frame while the player is in the segment
	if (code != last_code || value != last_value || Level_index != last_level)
		tlog("hook warp_command(code %d, value %d)", code, value);
	last_code = code;
	last_value = value;
	last_level = Level_index;
}

DTU_WEAK int dtu_hook_warp_allowed(int from_level, int to_level)
{
	return 1;
}

//------------------------------------------------------------------------
// Helpers

static short get_s16(ubyte *p)
{
	return (short)(p[0] | (p[1] << 8));
}

static ushort get_u16(ubyte *p)
{
	return (ushort)(p[0] | (p[1] << 8));
}

static uint get_u32(ubyte *p)
{
	return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24);
}

static void get_vec(vms_vector *v, ubyte *p)
{
	v->x = (fix)get_u32(p);
	v->y = (fix)get_u32(p+4);
	v->z = (fix)get_u32(p+8);
}

static int seg_ok(int segnum)
{
	return segnum >= 0 && segnum <= Highest_segment_index;
}

static int side_ok(int segnum, int side)
{
	return seg_ok(segnum) && side >= 0 && side < MAX_SIDES_PER_SEGMENT;
}

static side *get_side(int segnum, int sidenum)
{
	return &Segments[segnum].sides[sidenum];
}

// 0x2E900 in the executable is digi_link_sound_to_pos()
static void trig_sound(int soundnum, int segnum, int sidenum, vms_vector *pos, fix volume)
{
	vms_vector center;

	if (soundnum < 0 || soundnum >= MAX_SOUNDS || !side_ok(segnum, sidenum))
		return;
	if (!pos) {
		compute_center_point_on_side(&center, &Segments[segnum], sidenum);
		pos = &center;
	}
	digi_link_sound_to_pos(soundnum, segnum, sidenum, pos, 0, volume);
}

// 0x93D0A and 0x944BF: the sound of a trigger or trap that has no sound
// number. The volume is 0x8000 + rand() % 0x8001.
static void trig_default_sound(int segnum, int sidenum, vms_vector *pos)
{
	fix volume = 0x8000 + rand() % 0x8001;

	if (!side_ok(segnum, sidenum))
		return;
	if ((get_side(segnum, sidenum)->pad & 3) == 2)
		trig_sound(SND_TRIGGER_STONE, segnum, sidenum, pos, volume);
	else
		trig_sound(SND_TRIGGER_DEFAULT, segnum, sidenum, pos, volume);
}

// The second texture of a side. The texture numbers of the TIG and TAP
// files are numbers of the texture table of the level file, as the
// numbers of the sides in the level file (verified: the tex_on of 94 of
// the 95 triggers with a texture is the number of the side in the level
// file). The executable keeps these numbers in the side. The port keeps
// them in dtu_tmap_num2 and gives the side the number of the game, as
// level_translate_textures() does it.
static void set_side_tmap2(int segnum, int sidenum, short tmap)
{
	segment *segp;
	int tmap2 = tmap & 0x3FFF, orient = tmap & (~0x3FFF);

	if (!side_ok(segnum, sidenum))
		return;
	segp = &Segments[segnum];
	segp->dtu_tmap_num2[sidenum] = tmap;
	segp->sides[sidenum].tmap_num2 = 0;
	segp->dtu_tmap2_missing &= ~(1 << sidenum);
	if (tmap2 != 0) {
		if (tmap2 < Num_level_tmap_names && Level_tmap_xlate[tmap2] > 0)
			segp->sides[sidenum].tmap_num2 = Level_tmap_xlate[tmap2] | orient;
		else
			segp->dtu_tmap2_missing |= 1 << sidenum;
	}
	tlog("  texture 2 of seg %d side %d = %d (%s)", segnum, sidenum, tmap,
		(tmap2 > 0 && tmap2 < Num_level_tmap_names) ? Level_tmap_names[tmap2] : "none");
}

// 0x9565C: the player number of an object. The port has one player.
static int player_index(object *obj)
{
	return Player_num;
}

// The executable reads the clock() of the C library (0xEDC4C to 0x10A0F3:
// DOS function 2Ch, hours, minutes, seconds and hundredths), which counts
// 1/100 second of real time from the program start. The port counts the
// game time, so that a run with -fixedfps gives the same result each time.
// The offset stands for the time before the level: the stamp of a trap
// that never ran is 0xFFFFFFFF, and the first interval test must pass.
uint dtu_trap_clock(void)
{
	return (uint)(((int64_t)GameTime * 100) >> 16) + 360000;
}

//------------------------------------------------------------------------
// List of active traps

// 0x954A0
static void active_add(int trap_num)
{
	if (Num_active_traps & 0x20)
		return;
	Active_traps[Num_active_traps++] = trap_num;
}

// 0x954C8
static void active_remove(int trap_num)
{
	int i, j;

	for (i=0; i<Num_active_traps; i++)
		if (Active_traps[i] == trap_num) {
			for (j=i; j<Num_active_traps-1; j++)
				Active_traps[j] = Active_traps[j+1];
			Num_active_traps--;
		}
}

//------------------------------------------------------------------------
// Messages (0x94D8B)

char *dtu_trig_say_text(int say_index)
{
	uint flag, count, start, end;
	int len, i;

	if (!Say_data || Say_size < 12)
		return NULL;

	flag = get_u32(Say_data);
	count = get_u32(Say_data+4);
	if (flag != 0 && flag != 1)
		return NULL;
	if (say_index < 0 || (uint)say_index >= count)
		return NULL;
	if (8 + 4*(say_index+2) > Say_size)
		return NULL;

	start = get_u32(Say_data + 8 + 4*say_index);
	end = get_u32(Say_data + 12 + 4*say_index);
	len = (int)(end - start);
	if (len < 0 || len >= 0xfff)
		return NULL;
	if (start > (uint)Say_size || end > (uint)Say_size)
		return NULL;

	memcpy(Say_text, Say_data + start, len);
	Say_text[len] = 0;

	// the text of a file with the flag 1 has the two halves of each byte swapped
	if (flag)
		for (i=0; Say_text[i]; i++)
			Say_text[i] = (char)(((ubyte)Say_text[i] >> 4) | ((ubyte)Say_text[i] << 4));

	{
		// ASCENT: the translation of the message (dtulang.h): id say.LEVEL.N
		char id[40];
		snprintf(id, sizeof(id), "say.%s.%d", Say_level, say_index);
		return dtu_tr_id(id, Say_text);
	}
}

static void trig_say(dtu_trigger *t)
{
	char *text;

	// 0x94DA6: digi_link_sound_to_pos(20, seg, 0, pos, 0, 5)
	trig_sound(SND_SAY_AT_TRIGGER, t->seg, 0, &t->pos, 5);

	text = dtu_trig_say_text(t->say_index);
	if (!text) {
		tlog("say %d: no message with this number", t->say_index);
		return;
	}

	tlog("say %d: %s", t->say_index, text);
	dtu_display_add_trigger_message(text);
	if (SND_SAY_MESSAGE < MAX_SOUNDS)
		digi_play_sample(SND_SAY_MESSAGE, F1_0);
}

//------------------------------------------------------------------------
// Trap actions

// 0x94897: sets the type of the wall of a side and of the wall of the
// connected side.
static void trap_set_wall_type(int segnum, int sidenum, int type)
{
	segment *seg, *cseg;
	int wall_num, cside;

	if (!side_ok(segnum, sidenum))
		return;
	seg = &Segments[segnum];
	if (!seg_ok(seg->children[sidenum]))
		return;
	cseg = &Segments[seg->children[sidenum]];
	cside = find_connect_side(seg, cseg);

	wall_num = seg->sides[sidenum].wall_num;
	if (wall_num >= 0 && wall_num < Num_walls)
		Walls[wall_num].type = type;
	if (cside != -1) {
		wall_num = cseg->sides[cside].wall_num;
		if (wall_num >= 0 && wall_num < Num_walls)
			Walls[wall_num].type = type;
	}
	tlog("  wall type of seg %d side %d = %d (%s)", segnum, sidenum, type, type == WALL_OPEN ? "open" : "closed");
}

// 0x94A0C: the sound of a wall that a trap opens (type 4) or closes. The
// sound comes from the animation of the wall. A wall with no animation
// takes one that the texture number selects.
static void trap_wall_sound(int segnum, int sidenum, int type)
{
	side *s;
	vms_vector pos;
	int wall_num, clip, sound;

	if (!side_ok(segnum, sidenum) || Num_wall_anims <= 0)
		return;
	s = get_side(segnum, sidenum);
	wall_num = s->wall_num;
	if (wall_num < 0 || wall_num >= Num_walls)
		return;

	compute_center_point_on_side(&pos, &Segments[segnum], sidenum);

	clip = Walls[wall_num].clip_num;
	if (clip < 0 || clip >= Num_wall_anims) {
		// the texture numbers of the level file, see set_side_tmap2()
		short t1 = Segments[segnum].dtu_tmap_num[sidenum], t2 = Segments[segnum].dtu_tmap_num2[sidenum];

		clip = (t2 ? t2 : t1) % Num_wall_anims;
	}
	if (clip < 0)
		clip = (segnum + sidenum + wall_num) % Num_wall_anims;

	sound = (type == WALL_OPEN) ? WallAnims[clip].open_sound : WallAnims[clip].close_sound;
	if (sound > -1)
		trig_sound(sound, segnum, sidenum, &pos, F1_0);
}

// 0x9493A: only a floor makes the objects fall
static void trap_drop_objects(int segnum, int sidenum)
{
	if (sidenum == WBOTTOM)
		dtu_hook_drop_objects(segnum);
}

static void trap_open_door(int segnum, int sidenum)
{
	int wall_num;

	if (!side_ok(segnum, sidenum))
		return;
	wall_num = get_side(segnum, sidenum)->wall_num;
	if (wall_num < 0 || wall_num >= Num_walls || !seg_ok(Segments[segnum].children[sidenum])) {
		tlog("  open door: seg %d side %d has no wall", segnum, sidenum);
		return;
	}
	tlog("  open door: seg %d side %d wall %d", segnum, sidenum, wall_num);
	wall_open_door(&Segments[segnum], sidenum);
}

// 0x94B27: the actions of the flags
static void trap_do_flags(dtu_trap *tp)
{
	int segnum = tp->seg, sidenum = tp->side;
	int wall_num = -1;

	if (side_ok(segnum, sidenum))
		wall_num = get_side(segnum, sidenum)->wall_num;
	if (wall_num >= Num_walls)
		wall_num = -1;

	if (tp->flags & DTU_TPF_TOGGLE_WALL) {
		if (tp->flags & DTU_TPF_STATE) {
			trap_set_wall_type(segnum, sidenum, WALL_OPEN);
			trap_wall_sound(segnum, sidenum, WALL_OPEN);
			trap_drop_objects(segnum, sidenum);
		}
		else {
			trap_set_wall_type(segnum, sidenum, WALL_CLOSED);
			trap_wall_sound(segnum, sidenum, WALL_CLOSED);
		}
	}

	if (tp->flags & DTU_TPF_OPEN_DOOR) {
		trap_open_door(segnum, sidenum);
		trap_drop_objects(segnum, sidenum);
	}

	if (tp->flags & DTU_TPF_CLOSE_DOOR) {
		// 0x94C16 calls wall_close_door() (0x7DCC3) with the wall number.
		// The function takes a number of the list of moving doors.
		if (Dtu_fix_trap_close_door && wall_num >= 0) {
			tlog("  close door: seg %d side %d wall %d", segnum, sidenum, wall_num);
			dtu_wall_start_close(&Segments[segnum], sidenum);
		}
		else
			tlog("  close door: no action");
	}

	if (tp->flags & DTU_TPF_WALL_CLOSED) {
		trap_set_wall_type(segnum, sidenum, WALL_CLOSED);
		trap_wall_sound(segnum, sidenum, WALL_CLOSED);
	}

	if (tp->flags & DTU_TPF_WALL_OPEN) {
		trap_set_wall_type(segnum, sidenum, WALL_OPEN);
		trap_wall_sound(segnum, sidenum, WALL_OPEN);
		trap_drop_objects(segnum, sidenum);
	}

	if ((tp->flags & DTU_TPF_REPEAT) && !(tp->flags & DTU_TPF_ACTIVE)) {
		tp->flags |= DTU_TPF_ACTIVE;
		active_add(tp->id);
		tlog("  trap %d is active now (%d active)", tp->id, Num_active_traps);
	}

	if (tp->flags & DTU_TPF_TOGGLE_LOCK) {
		if (wall_num >= 0) {
			if (tp->flags & DTU_TPF_STATE)
				Walls[wall_num].flags ^= WALL_DOOR_LOCKED;
			else
				Walls[wall_num].flags &= ~WALL_DOOR_LOCKED;
			tlog("  lock of seg %d side %d wall %d: %s", segnum, sidenum, wall_num,
				(Walls[wall_num].flags & WALL_DOOR_LOCKED) ? "locked" : "not locked");
		}
		else
			tlog("  lock: seg %d side %d has no wall", segnum, sidenum);
	}

	tp->flags ^= DTU_TPF_STATE;
}

// 0x94F89: the texture and the light of the side of the trap
static void trap_do_side(dtu_trap *tp)
{
	int i;

	if (tp->tex_enabled >= 0)
		set_side_tmap2(tp->seg, tp->side, tp->tex_enabled);

	if (!(tp->flags & DTU_TPF_LIGHT_DONE)) {
		if (side_ok(tp->seg, tp->side))
			for (i=0; i<4; i++)
				get_side(tp->seg, tp->side)->uvls[i].l += tp->light_delta;
		tp->flags |= DTU_TPF_LIGHT_DONE;
	}
}

// 0x94363
void dtu_trap_run(int trap_num, object *obj)
{
	dtu_trap *tp;
	uint now;
	int steps = 0;

	while (trap_num >= 0 && trap_num < Dtu_num_traps) {
		tp = &Dtu_traps[trap_num];

		// The executable has no limit here. A chain that returns to
		// itself would not end.
		if (++steps > DTU_MAX_TRAPS)
			break;

		if (!tp->on)
			goto next;

		Trap_chain_stop = 0;
		now = dtu_trap_clock();

		if (tp->delay != 0 && !(tp->flags & DTU_TPF_ACTIVE) && tp->charges != 0) {
			if (!(tp->flags & DTU_TPF_DELAY_RUNS)) {
				// the delay starts. The trap runs each frame until it ends.
				tp->flags |= DTU_TPF_DELAY_RUNS;
				active_add(tp->id);
				tp->timestamp = now;
				tlog("trap %d: delay of %d clock units starts", trap_num, tp->delay);
				goto next;
			}
			if (now - tp->timestamp <= tp->delay) {
				Trap_chain_stop = 1;
				goto next;
			}
			active_remove(tp->id);
			tp->delay = tp->delay_init;
			tp->flags &= ~DTU_TPF_DELAY_RUNS;
			Trap_chain_stop = 1;
			tp->timestamp = now;
		}
		else if (now - tp->timestamp < tp->interval)
			goto next;

		tp->timestamp = now;

		if (tp->charges != -1) {
			short old = tp->charges;

			tp->charges = old - 1;
			if (old <= 0) {
				tp->charges = 0;
				if (tp->flags & DTU_TPF_ACTIVE) {
					tp->flags ^= DTU_TPF_ACTIVE;
					active_remove(trap_num);
				}
				if (tp->flags & DTU_TPF_RECHARGE)
					tp->charges = tp->charges_init;
				tlog("trap %d: no charges%s", trap_num, (tp->flags & DTU_TPF_RECHARGE) ? ", loaded again" : "");
				goto next;
			}
		}

		tlog("trap %d runs: seg %d side %d flags 0x%x charges %d", trap_num, tp->seg, tp->side, tp->flags, tp->charges);

		if (tp->sound != -1)
			trig_sound(tp->sound, tp->seg, tp->side, &tp->pos, F1_0);
		else
			trig_default_sound(tp->seg, tp->side, &tp->pos);

		if (tp->weapon_type != -1)
			dtu_hook_trap_weapon(tp->weapon_type, tp->seg, tp->side, &tp->pos, &tp->dir);

		if (tp->spawn_object != -1)
			dtu_hook_trap_spawn_object(tp->spawn_object, tp->seg, tp->side, &tp->pos);

		if (tp->flags != 0)
			trap_do_flags(tp);

		if (tp->robot_type != -1)
			dtu_hook_trap_gate_robot(tp->robot_type, tp->robot_arg, tp->seg, &tp->pos);

		trap_do_side(tp);

		// The executable takes the player from the object with no test.
		// A trap of the active list runs with no object.
		if (tp->stat_delta != 0 && obj)
			dtu_hook_trap_hit_points(obj, tp->stat_delta);

next:
		if (tp->next_trap == -1 || Trap_chain_stop)
			break;
		trap_num = tp->next_trap;
	}
}

//------------------------------------------------------------------------
// Triggers

// 0x942ED: the two item conditions
static int trig_items_ok(int player_num, dtu_trigger *t)
{
	int has_needed = 1, lacks = 1;

	if (t->need_item_class == -1 && t->lack_item_class == -1)
		return 1;
	if (t->need_item_class != -1)
		has_needed = dtu_hook_player_has_item(player_num, t->need_item_class, t->need_item_id);
	if (t->lack_item_class != -1)
		lacks = !dtu_hook_player_has_item(player_num, t->lack_item_class, t->lack_item_id);
	return has_needed && lacks;
}

// 0x93B29
void dtu_trig_fire(int trigger_num, int type, object *obj)
{
	dtu_trigger *t;
	dtu_trap *tp;
	int is_player, n, steps;

	if (trigger_num < 0 || trigger_num >= Dtu_num_triggers || !obj)
		return;
	t = &Dtu_triggers[trigger_num];

	tlog("trigger %d (on=%d type=%d mask=%d) seg %d side %d", trigger_num, t->on, type, t->activation_mask, t->seg, t->side);

	if (!t->on)
		return;
	if (!(t->activation_mask & type))
		return;

	is_player = (obj->type == OBJ_PLAYER);

	// A trigger that counts the players inside. One trigger with the
	// flag ENTER and one without it stand at the two ends of the place.
	if ((obj->type == OBJ_PLAYER || obj->type == OBJ_COOP) && (t->flags & DTU_TGF_OCCUPANCY)) {
		int inside = 0, p = player_index(obj);
		uint bit = (uint)DTU_TPF_PLAYER_INSIDE << p;

		if (t->first_trap < 0 || t->first_trap >= Dtu_num_traps)
			return;
		tp = &Dtu_traps[t->first_trap];
		if (t->flags & DTU_TGF_ENTER)
			tp->flags |= bit;
		else
			tp->flags &= ~bit;
		for (n=0; n<4; n++)
			if (tp->flags & ((uint)DTU_TPF_PLAYER_INSIDE << n))
				inside++;

		if (!(t->flags & DTU_TGF_ENTER)) {
			if (inside)
				return;
			// the last player left: the traps of the chain stop
			n = t->first_trap;
			for (steps=0; n >= 0 && n < Dtu_num_traps && steps <= DTU_TRIG_CHAIN_LIMIT; steps++) {
				Dtu_traps[n].flags &= ~DTU_TPF_ACTIVE;
				Dtu_traps[n].timestamp = 0xffffffff;
				active_remove(n);
				n = Dtu_traps[n].next_trap;
			}
			tlog("  the last player left: the traps from %d stop", t->first_trap);
			return;
		}
	}

	if (t->fire_count < 0x7fff)
		t->fire_count++;

	if (t->skip_count > 0) {
		t->skip_count--;
		if (t->skip_count > 0)
			return;
	}

	if (is_player) {
		int p = player_index(obj);

		// a switch that changes its texture or runs traps makes a sound
		if (t->first_trap >= 0 && (t->tex_off >= 0 || (t->flags & DTU_TGF_TOGGLE_TEX)))
			trig_default_sound(t->seg, t->side, &t->pos);

		if (t->unk30)
			tlog("  field 0x30 = %u (0xAF3CF, not known)", t->unk30);

		if (!dtu_hook_player_masks_ok(p, t->player_mask_a, t->player_mask_b)) {
			tlog("  stopped by the player masks");
			return;
		}

		if (!trig_items_ok(p, t)) {
			tlog("  stopped by the item conditions (need %d,%d lack %d,%d)",
				t->need_item_class, t->need_item_id, t->lack_item_class, t->lack_item_id);
			return;
		}

		// the three conditions at 0x35, 0x37 and 0x39 call 0x9435D, which
		// answers 1

		if (t->say_index != -1)
			trig_say(t);

		if (t->object_num > 1 && t->object_num <= Highest_object_index) {
			dtu_hook_trigger_object(t->object_num, 3);
			if (!t->on)
				return;
		}
	}
	else {
		// an object that is no player fires only a trigger with no condition
		if (t->need_item_class >= 0 || t->lack_item_class >= 0 || t->cond35 >= 0 || t->cond37 >= 0 || t->cond39 >= 0)
			return;
	}

	if (t->tex_off >= 0) {
		if (t->flags & DTU_TGF_TOGGLE_TEX) {
			if (t->first_trap != -1 && t->first_trap < Dtu_num_traps) {
				tp = &Dtu_traps[t->first_trap];
				if (dtu_trap_clock() - tp->timestamp < tp->interval)
					return;
			}
			if (side_ok(t->seg, t->side)) {
				if (Segments[t->seg].dtu_tmap_num2[t->side] == t->tex_on) {
					tlog("  Using texoff.");
					set_side_tmap2(t->seg, t->side, t->tex_off);
				}
				else {
					tlog("  Using texon");
					set_side_tmap2(t->seg, t->side, t->tex_on);
				}
			}
		}
		else
			set_side_tmap2(t->seg, t->side, t->tex_off);
	}

	if (t->sound != -1)
		trig_sound(t->sound, t->seg, t->side, &t->pos, F1_0);

	if (t->take_item)
		dtu_hook_player_take_item(t->need_item_class, t->need_item_id);

	// The chains: the executable counts a step only when a trigger names
	// itself. The port counts each step.
	n = t->enable_trigger;
	for (steps=0; n >= 0 && n < Dtu_num_triggers && steps <= DTU_TRIG_CHAIN_LIMIT; steps++) {
		dtu_trigger *t2 = &Dtu_triggers[n];

		if (!t2->on)
			tlog("  trigger %d on", n);
		t2->on = 1;
		if (t2->tex_on != -1)
			set_side_tmap2(t2->seg, t2->side, t2->tex_on);
		if (t2->enable_trigger == n)
			break;
		n = t2->enable_trigger;
	}

	if (t->enable_trap >= 0 && t->enable_trap < Dtu_num_traps) {
		tp = &Dtu_traps[t->enable_trap];
		tp->on = 1;
		if (tp->tex_enabled != -1)
			set_side_tmap2(tp->seg, tp->side, tp->tex_enabled);
		tlog("  trap %d on", t->enable_trap);
	}

	n = t->disable_trigger;
	for (steps=0; n >= 0 && n < Dtu_num_triggers && steps <= DTU_TRIG_CHAIN_LIMIT; steps++) {
		dtu_trigger *t2 = &Dtu_triggers[n];

		if (t2->on)
			tlog("  trigger %d off", n);
		t2->on = 0;
		// side flag 4: the thief skill found the trigger
		if (side_ok(t2->seg, t2->side))
			get_side(t2->seg, t2->side)->pad &= ~4;
		if (t2->tex_off != -1)
			set_side_tmap2(t2->seg, t2->side, t2->tex_off);
		if (t2->disable_trigger == n)
			break;
		n = t2->disable_trigger;
	}

	if (t->disable_trap >= 0 && t->disable_trap < Dtu_num_traps) {
		tp = &Dtu_traps[t->disable_trap];
		tp->on = 0;
		// 0x94231 clears the side flags 0x08 and 0x20 of the side of the
		// trap with the number of this trigger (not of the trap that it
		// switches off). No shipped trigger has a disable_trap. The port
		// takes the side of the trap that is switched off.
		if (side_ok(tp->seg, tp->side))
			get_side(tp->seg, tp->side)->pad &= ~0x28;
		if (tp->tex_disabled != -1)
			set_side_tmap2(tp->seg, tp->side, tp->tex_disabled);
		active_remove(t->disable_trap);
		tlog("  trap %d off", t->disable_trap);
	}

	if (t->first_trap != -1)
		dtu_trap_run(t->first_trap, obj);
}

// 0x7DBF5, the end of wall_open_door: in level 32 (flame) the door of
// segment 478 side 4 stays open (wall flag 0x10, the automatic close, off
// on both sides) while the 4-bit variable of object 36 (Lolth) is below 3,
// and 5 seconds later object 36 talks (0xAC055(object, 0)). The executable
// waits in a loop of game frames (0x4391D); the port counts the time in
// dtu_trig_frame().
static fix Flame_talk_time = -1;

void dtu_trig_door_opened(int segnum, int side)
{
	int c, wall_num;

	if (Level_index != 32 || segnum != 478 || side != 4)
		return;
	if (36 > Highest_object_index || Objects[36].type == OBJ_NONE
			|| ((Objects[36].dtu.m_1b0 >> 24) & 15) >= 3)
		return;
	wall_num = Segments[segnum].sides[side].wall_num;
	if (wall_num >= 0 && wall_num < Num_walls)
		Walls[wall_num].flags &= ~WALL_DOOR_AUTO;
	c = Segments[segnum].children[side];
	if (seg_ok(c)) {
		int cs = find_connect_side(&Segments[segnum], &Segments[c]);

		if (cs >= 0 && (wall_num = Segments[c].sides[cs].wall_num) >= 0 && wall_num < Num_walls)
			Walls[wall_num].flags &= ~WALL_DOOR_AUTO;
	}
	Flame_talk_time = GameTime + i2f(5);
	tlog("level rule of flame: the door of seg 478 stays open, object 36 talks in 5 seconds");
}

static void flame_talk_frame(void)
{
	if (Flame_talk_time < 0 || GameTime < Flame_talk_time)
		return;
	Flame_talk_time = -1;
	if (Level_index == 32)
		dtu_hook_trigger_object(36, 0);
}

// The end of 0x939AD has fixed rules for two levels. Level 7 (thief2):
// the triggers 46 and 43 take the lock from a door and open it. Level 32
// (flame): after the triggers 33, 32 and 26 (0x93ADE), object 36 (the
// avatar of Lolth) talks (0xAC055(object, 0): run_script(object, 0)) while
// the 4-bit variable of its script (bits 24 to 27 of +0x1B0) is below 1,
// 2 and 3. The script FLAME 320 sets the variable to 1, 2 and 3 itself, so
// this runs only when the trigger run (mode 3) did not start.
static void trig_level_rules(int last_trigger)
{
	int door_seg, lock_seg, wall_num;

	if (Level_index == 32) {
		int stage = last_trigger == 33 ? 1 : last_trigger == 32 ? 2 : last_trigger == 26 ? 3 : 0;

		if (stage && 36 <= Highest_object_index && Objects[36].type != OBJ_NONE
				&& (int) ((Objects[36].dtu.m_1b0 >> 24) & 15) < stage) {
			tlog("level rule of flame: trigger %d, object 36 talks", last_trigger);
			dtu_hook_trigger_object(36, 0);
		}
		return;
	}
	if (Level_index != 7)
		return;
	if (last_trigger == 46) {
		door_seg = 607; lock_seg = 608;
	}
	else if (last_trigger == 43) {
		door_seg = 609; lock_seg = 610;
	}
	else
		return;

	if (!seg_ok(door_seg) || !seg_ok(lock_seg))
		return;
	wall_num = Segments[lock_seg].sides[WBACK].wall_num;
	if (wall_num >= 0 && wall_num < Num_walls)
		Walls[wall_num].flags &= ~WALL_DOOR_LOCKED;
	tlog("level rule of thief2: trigger %d opens the door of seg %d side 5", last_trigger, door_seg);
	trap_open_door(door_seg, WFRONT);
}

// 0x5A855(0): after the restore of a level state (0x5AA75), fixed changes
// of the triggers of five levels. level_index is the index in DTU.MSN.
// - 2 (kobolds): trigger 1 (the one-time message of seg 167) is off.
// - 6 (thief1): the executable clears byte 6 of trigger 34, a byte of the
//   y coordinate of its position (sound position only, no effect seen). The
//   port does not change the position.
// - 7 (thief2): the triggers 43 and 46 are on, and trigger 46 gets the
//   flags, masks, textures, count, traps and message of trigger 43 (0x5A95A).
// - 21 (drow2): trigger 20 runs the script of object 42 (object_num).
// - 32 (flame): the triggers 26, 32 and 33 (Lolth, object 36) are on again.
void dtu_trig_state_rules(int level_index)
{
	dtu_trigger *a, *b;

	switch (level_index) {
		case 2:
			if (Dtu_num_triggers > 1)
				Dtu_triggers[1].on = 0;
			break;
		case 7:
			if (Dtu_num_triggers <= 46)
				break;
			a = &Dtu_triggers[43];
			b = &Dtu_triggers[46];
			a->on = b->on = 1;
			b->flags = a->flags;
			b->activation_mask = a->activation_mask;
			b->player_mask_a = a->player_mask_a;
			b->player_mask_b = a->player_mask_b;
			b->sound = a->sound;
			b->tex_on = a->tex_on;
			b->tex_off = a->tex_off;
			b->fire_count = a->fire_count;
			b->skip_count = a->skip_count;
			b->first_trap = a->first_trap;
			b->enable_trigger = a->enable_trigger;
			b->enable_trap = a->enable_trap;
			b->disable_trigger = a->disable_trigger;
			b->disable_trap = a->disable_trap;
			b->object_num = a->object_num;
			b->say_index = a->say_index;
			break;
		case 21:
			if (Dtu_num_triggers > 20)
				Dtu_triggers[20].object_num = 42;
			break;
		case 32:
			if (Dtu_num_triggers > 33)
				Dtu_triggers[33].on = Dtu_triggers[32].on = Dtu_triggers[26].on = 1;
			break;
		default:
			return;
	}
	tlog("level rule of the restore (0x5A855), level %d", level_index);
}

// 0x939AD
void dtu_trig_check(int segnum, int side, int type, object *obj)
{
	int n, prev = -1, steps = 0, last = -1;

	if (!seg_ok(segnum)) {
		tlog("illegal trigger seg (%d)", segnum);
		return;
	}
	if (side < 0 || side > 5) {
		tlog("illegal trigger side (%d)", side);
		return;
	}
	if (segnum >= DTU_TRIG_SEG_TABLE)
		return;

	n = Dtu_trigger_seg_table[segnum];
	while (n != -1 && steps < DTU_TRIG_CHAIN_LIMIT) {
		if (n < 0 || n >= Dtu_num_triggers)
			break;
		if (Dtu_triggers[n].side == side) {
			dtu_trig_fire(n, type, obj);
			last = n;
		}
		// the executable counts a step only when a trigger names itself
		if (n == prev)
			steps++;
		else if (++steps > DTU_MAX_TRIGGERS)
			break;
		prev = n;
		n = Dtu_triggers[n].next_in_seg;
	}

	trig_level_rules(last);
}

// 0x95019
void dtu_trig_touch(int segnum, int side, int type, object *obj)
{
	if (!Dtu_num_triggers || !obj)
		return;
	if (segnum < 0 || segnum >= DTU_TRIG_SEG_TABLE || Dtu_trigger_seg_table[segnum] == -1)
		return;
	if (side == WBOTTOM && segnum != obj->segnum)
		return;
	dtu_trig_check(segnum, side, type, obj);
}

// 0x29802 (collide_object_with_wall): the type of the object gives the
// activation type
void dtu_trig_object_hit_wall(object *obj, int segnum, int side)
{
	int type = 0;

	switch (obj->type) {
		case OBJ_PLAYER:	type = DTU_ACT_PLAYER; break;
		case OBJ_WEAPON:	type = DTU_ACT_WEAPON; break;
		case OBJ_DEBRIS:	type = DTU_ACT_DEBRIS; break;
		case OBJ_ROBOT:	type = DTU_ACT_ROBOT; break;
	}

	if (obj->flags & OF_SHOULD_BE_DEAD)
		return;
	// 0x29970: not while the byte 0x1AE of the object has the bit 2
	dtu_trig_touch(segnum, side, type, obj);
}

//------------------------------------------------------------------------
// Warps (0x59C3C, mission.c of the executable)

int dtu_warp_lookup(int segnum, int *dest_level, int *dest_seg, int *value)
{
	if (!Warps_enabled || segnum < 0 || segnum >= MAX_SEGMENTS || Warps[segnum].dest_level == -1)
		return 0;
	*dest_level = Warps[segnum].dest_level;
	*dest_seg = Warps[segnum].dest_seg;
	*value = Warps[segnum].value;
	return 1;
}

// The next data line of the warp file: v gets level, segment, dest_level,
// dest_segment and value. Returns 0 at the end of the file.
static int warp_next_line(char **pp, char *end, int v[5])
{
	char *p = *pp;

	while (p < end) {
		char line[128];
		int len = 0, i;

		while (p < end && *p != '\n' && *p != '\r') {
			if (len < (int)sizeof(line)-1)
				line[len++] = *p;
			p++;
		}
		while (p < end && (*p == '\n' || *p == '\r'))
			p++;
		line[len] = 0;
		for (i=0; i<len; i++)
			if (line[i] == ';')
				line[i] = 0;

		if (sscanf(line, "%d,%d %d,%d %d", &v[0], &v[1], &v[2], &v[3], &v[4]) == 5) {
			*pp = p;
			return 1;
		}
	}
	*pp = p;
	return 0;
}

// 0x599C3: the lines of the warp file for one level. A line is
// "level,segment dest_level,dest_segment value".
static void warp_load(int level_index)
{
	ubyte *data;
	char *p, *end, name[32];
	int size, i, n = 0, v[5];

	for (i=0; i<MAX_SEGMENTS; i++)
		Warps[i].dest_level = -1;
	Warps_enabled = 0;

	// the name of the warp file is the name of the mission file with "wrp"
	strcpy(name, "DTU.WRP");
	data = dtu_load_file(name, &size);
	if (!data) {
		tlog("no file %s", name);
		return;
	}

	p = (char *)data;
	end = p + size;
	while (warp_next_line(&p, end, v)) {
		int seg = v[1];

		if (v[0] != level_index || seg < 0 || seg >= MAX_SEGMENTS)
			continue;
		Warps[seg].dest_level = v[2];
		Warps[seg].dest_seg = v[3];
		Warps[seg].value = v[4];
		n++;
	}
	free(data);

	Warps_enabled = 1;
	tlog("%s: %d lines for level %d", name, n, level_index);
}

// 0x5A5DE: the direction of the player after a warp
static void warp_set_facing(object *obj, int value)
{
	static char *names[4] = { "EAST", "WEST", "NORTH", "SOUTH" };
	vms_matrix *m = &obj->orient;

	if (value < 0 || value > 3)
		return;

	vm_vec_zero(&m->rvec);
	vm_vec_zero(&m->uvec);
	vm_vec_zero(&m->fvec);
	m->uvec.y = F1_0;
	switch (value) {
		case 0:	m->rvec.z = -F1_0;	m->fvec.x = F1_0;		break;
		case 1:	m->rvec.z = F1_0;		m->fvec.x = -F1_0;	break;
		case 2:	m->rvec.x = F1_0;		m->fvec.z = F1_0;		break;
		case 3:	m->rvec.x = -F1_0;	m->fvec.z = -F1_0;	break;
	}
	tlog("Facing %s", names[value]);
}

// Moves the player to a segment (0x5A268 to 0x5A376)
static void warp_place_player(int dest_seg, int value)
{
	object *obj = ConsoleObject;
	vms_vector center;

	if (!seg_ok(dest_seg)) {
		tlog("warp: the level has no segment %d", dest_seg);
		return;
	}
	compute_segment_center(&center, &Segments[dest_seg]);
	obj->pos = center;
	obj->last_pos = center;
	obj_relink(obj-Objects, dest_seg);
	vm_vec_zero(&obj->mtype.phys_info.velocity);
	warp_set_facing(obj, value);
}

// The passage gold of the well (0x59F0B to 0x5A026). A warp from level 0
// (the Yawning Portal) or to level 0 costs 1 gold when the mission index
// (0x297F2C, set by the mission loader 0x59644) is 0; the port has only
// that mission. From the inn: 0 gold is refused (text 0x13CE3A), 1 gold
// asks first (0x13CEBE, Yes is choice 0). To the inn: 0 gold is refused
// (0x13CF25). The game takes 1 gold from the player and from the character
// record, each clamped at 0 (0x5A026). A warp between dungeon levels is
// free. The texts are in the executable, not in DTUE.TEX. The check runs
// when the player enters the segment (dtu_trig_frame), so a refusal asks
// again only at the next entry.
// D:\dev\DTU-Decomp recovered/world-warp.md and src/world_warp.c.
static int well_pay_passage(int from_level, int to_level)
{
	if (from_level != 0 && to_level != 0)
		return 1;
	if (from_level == 0) {
		if (Dtu_gold < 1) {
			dtu_char_choice(LN("Passage in and out of\nUndermountain costs one gold.\nYou will need 2 gold\npieces to venture into\nthe dungeon and return to\nWaterdeep"), 1, fe_text(34));
			dtulog_event("Well: no gold for the passage into Undermountain, no warp");
			return 0;
		}
		if (Dtu_gold < 2 && dtu_char_choice(LN("If you enter the well\nnow you will not have\nenough gold for passage\nback to Waterdeep.\nProceed anyway?"), 2, fe_text(32), fe_text(33)) != 0) {
			dtulog_event("Well: the player does not enter with 1 gold, no warp");
			return 0;
		}
	} else if (Dtu_gold < 1) {
		dtu_char_choice(LN("It costs 1 gold piece\nto ascend back to Waterdeep.\nYou do not have any gold.\nYou will need to search\nthe dungeon for gold"), 1, fe_text(34));
		dtulog_event("Well: no gold for the passage back to the inn, no warp");
		return 0;
	}
	Dtu_gold = (Dtu_gold > 0) ? Dtu_gold - 1 : 0;
	Dtu_char.pa.gold = (Dtu_char.pa.gold > 0) ? Dtu_char.pa.gold - 1 : 0;
	dtulog_event("Well: the passage costs 1 gold, %d left", Dtu_gold);
	return 1;
}

// Returns 0 for no warp, 1 for a warp and 2 for a command line.
static int warp_check(int segnum, object *obj)
{
	int dest_level, dest_seg, value;
	char file[20], *dot;
	ubyte *data;
	int size;

	if (!dtu_warp_lookup(segnum, &dest_level, &dest_seg, &value))
		return 0;

	// 0x59D0C: a command line and a warp inside the level fire the floor
	// triggers of the segment first. A warp to another level goes to the
	// check at 0x59D2D first (an item of the player, monsters near the
	// player), which the port does not have.
	// The executable runs this function again in each frame while the
	// player is in a segment with a line (see dtu_trig_frame()), so the
	// floor triggers of such a segment fire in each frame.
	if (obj == ConsoleObject && (!Dtu_fix_trigger_repeat || obj->segnum != Floor_trigger_seg))
		dtu_trig_touch(obj->segnum, WBOTTOM, DTU_ACT_PLAYER, obj);

	if (dest_level == -2) {
		// The music commands (codes 100 to 198) are run by dtumusic.c,
		// which reads the warp file for them itself.
		if (dest_seg < 100 || dest_seg > 198)
			dtu_hook_warp_command(dest_seg, value);
		return 2;
	}

	if (dest_level < 0 || dest_level >= Last_level) {
		tlog("warp: no level %d", dest_level);
		return 0;
	}

	if (!dtu_hook_warp_allowed(Level_index, dest_level))
		return 0;

	if (obj == ConsoleObject && !well_pay_passage(Level_index, dest_level))
		return 0;

	if (dest_level == Level_index) {
		tlog("solo-warp! seg %d to seg %d", segnum, dest_seg);
		warp_place_player(dest_seg, value);
		return 1;
	}

	// The port stops with an error when a level does not load. Five
	// levels of the mission file are not in the game data.
	strncpy(file, Level_names[dest_level], sizeof(file)-1);
	file[sizeof(file)-1] = 0;
	if ((dot = strchr(file, '.')) != NULL)
		strcpy(dot, ".LVL");
	data = dtu_load_file(file, &size);
	if (!data) {
		tlog("warp: the level file %s is not in the game data", file);
		return 0;
	}
	free(data);

	tlog("Warping to %s, Segment %d.", Level_names[dest_level], dest_seg);
	printf("Warping to %s, Segment %d.\n", Level_names[dest_level], dest_seg);
	// ASCENT: the session log names the line of the warp file
	dtulog_event("Level change: DTU.WRP line %d,%d %d,%d %d (%s segment %d to %s segment %d)",
		Level_index, segnum, dest_level, dest_seg, value,
		Level_names[Level_index], segnum, Level_names[dest_level], dest_seg);

	// 0x5A059 to 0x5A070: the executable waits for the sounds and stops
	// them. The load of a level replaces the sound data.
	digi_stop_all();
	plat_audio_flush();			// the mixer must not read sample data that the load releases
	// ASCENT: 0x5A0F6 closes the doors and 0x5A756 saves the state of the
	// level, 0x5AA75 restores the state of the new level (dtustate.c).
	dtustate_warp_leave(Level_index);
	StartNewLevelSub(dest_level+1, 1);
	dtustate_warp_enter(dest_level);
	warp_place_player(dest_seg, value);
	return 1;
}

// ASCENT: new. The executable looks at the segment of the player only
// when a move takes the player into another segment (0x69BC8). A level
// state that puts the player into a segment (dtustate.c) must not count as
// such a move.
void dtu_trig_player_placed(void)
{
	if (ConsoleObject && seg_ok(ConsoleObject->segnum))
		Last_player_seg = Floor_trigger_seg = ConsoleObject->segnum;
}

// ASCENT: new, for the direct start (-level NAME). The game starts a new
// character in the Yawning Portal (level 0) at the player object of the
// level file (0x5895E). It comes into each other level only by a warp
// (0x59C3C, which puts the player at the center of the destination
// segment, 0x5A268) or by a save game. The player object of some levels
// is in the segment of a warp out of the level (KOBOLDS: segment 12, the
// warp to HUB1), so a start there leaves the level in the first frame.
// This function puts the player where the first warp line of DTU.WRP
// into the level (file order) from another level puts it. Returns 0 if
// the file has no such line.
int dtu_trig_place_at_arrival(void)
{
	ubyte *data;
	char *p, *end;
	int size, v[5], found = 0, start_seg;

	if (!ConsoleObject || Level_index <= 0)
		return 0;
	data = dtu_load_file("DTU.WRP", &size);
	if (!data)
		return 0;
	p = (char *)data;
	end = p + size;
	while (warp_next_line(&p, end, v))
		if (v[2] == Level_index && v[0] >= 0 && v[0] != Level_index && v[0] < Last_level && seg_ok(v[3])) {
			found = 1;
			break;
		}
	free(data);
	if (!found)
		return 0;

	start_seg = ConsoleObject->segnum;
	warp_place_player(v[3], v[4]);
	// as after a warp in dtu_trig_frame(): the lines of the destination
	// segment act in the next frame
	Last_player_seg = -1;
	Floor_trigger_seg = -1;
	dtulog_event("Level start: segment %d as the DTU.WRP line %d,%d %d,%d %d from %s (the player object is in segment %d)",
		v[3], v[0], v[1], v[2], v[3], v[4], Level_names[v[0]], start_seg);
	return 1;
}

//------------------------------------------------------------------------
// Frame

void dtu_trig_frame(void)
{
	object *obj = ConsoleObject;
	int i;

	dtu_test_place_player();	// ASCENT: the test option -testpos
	flame_talk_frame();

	// 0x9547A: the traps that wait for a delay or that repeat
	for (i=0; i<Num_active_traps; i++)
		dtu_trap_run(Active_traps[i], NULL);

	// 0x69BC8: the player is in another segment than in the last frame
	if (!obj || obj->type != OBJ_PLAYER || !seg_ok(obj->segnum))
		return;
	if (obj->segnum != Last_player_seg) {
		int segnum = obj->segnum, r;

		Last_player_seg = segnum;
		r = warp_check(segnum, obj);
		if (r) {
			// the executable forgets the segment here, so the lines of
			// the segment act again in the next frame
			Last_player_seg = -1;
			// after a warp the player is in another segment
			Floor_trigger_seg = (r == 2) ? segnum : -1;
			return;
		}
		Floor_trigger_seg = segnum;
		// The executable goes through the segments that the move of the
		// frame touched. Only the segment of the player passes the floor
		// test of 0x95019.
		dtu_trig_touch(obj->segnum, WBOTTOM, DTU_ACT_PLAYER, obj);
	}
}

//------------------------------------------------------------------------
// Use action (0x95050)

void dtu_player_use(void)
{
	object *obj = ConsoleObject;
	fvi_query fq;
	fvi_info hit;
	vms_vector end;
	int hit_type, result = WHP_NOT_SPECIAL;
	fix dt;

	if (!obj || obj->type != OBJ_PLAYER)
		return;
	// 0x95079: not while the byte 0x89 of the object has the bit 0x40

	// one action in each half second
	dt = GameTime - Last_use_time;
	if (dt < 0)
		dt = -dt;
	if (dt < F1_0/2)
		return;
	Last_use_time = GameTime;

	// 0xB30FD: a line of 20 units in the direction of the player
	vm_vec_scale_add(&end, &obj->pos, &obj->orient.fvec, i2f(20));
	fq.p0 = &obj->pos;
	fq.p1 = &end;
	fq.startseg = obj->segnum;
	fq.rad = 0;
	fq.thisobjnum = obj-Objects;
	fq.ignore_obj_list = NULL;
	fq.flags = 0;
	hit_type = find_vector_intersection(&fq, &hit);

	if (hit_type == HIT_WALL && hit.hit_side >= 0 && side_ok(hit.hit_seg, hit.hit_side)) {
		tlog("use: seg=%d side=%d", hit.hit_seg, hit.hit_side);
		dtu_trig_check(hit.hit_seg, hit.hit_side, DTU_ACT_USE, obj);
		// a trigger can load another level
		if (!seg_ok(hit.hit_seg))
			return;
		result = dtu_wall_use_door(&Segments[hit.hit_seg], hit.hit_side, obj, 0);
		if (result != WHP_NOT_SPECIAL) {
			wall *w = &Walls[Segments[hit.hit_seg].sides[hit.hit_side].wall_num];

			tlog("use: door wall %d clip %d flags 0x%x keys %d state %d: %s", (int)(w-Walls), w->clip_num, w->flags, w->keys, w->state,
				result == WHP_DOOR ? "opens" : "locked");
		}
		if (result != WHP_DOOR)
			dtu_hook_use_wall(obj, hit.hit_seg, hit.hit_side);
	}
	else {
		tlog("use: no side in reach (seg %d, direction %.2f %.2f %.2f)", obj->segnum,
			f2fl(obj->orient.fvec.x), f2fl(obj->orient.fvec.y), f2fl(obj->orient.fvec.z));
		dtu_hook_use_nothing(obj);
	}
	// 0x951D1: 0xAC155() when the action opened no door (not known)
}

//------------------------------------------------------------------------
// Load (0x9550D, loaders 0x95B08 and 0x95797)

// ASCENT: the records as they were read (the level file or a state
// file). The packers start from them, so the bytes that the port does not
// use go back into a state file unchanged.
static ubyte Trig_raw[DTU_MAX_TRIGGERS][DTU_TIG_RECORD];
static ubyte Trap_raw[DTU_MAX_TRAPS][DTU_TAP_RECORD];

static void trig_parse(dtu_trigger *t, ubyte *p)
{
	memcpy(Trig_raw[t - Dtu_triggers], p, DTU_TIG_RECORD);
	t->unk00 = get_s16(p);
	get_vec(&t->pos, p+0x02);
	t->on = p[0x0e];
	t->unk0f = get_u32(p+0x0f);
	t->flags = get_u16(p+0x13);
	t->activation_mask = get_u16(p+0x15);
	t->unk17 = get_s16(p+0x17);
	t->need_item_class = get_s16(p+0x19);
	t->need_item_id = get_s16(p+0x1b);
	t->unk1d = get_s16(p+0x1d);
	t->lack_item_class = get_s16(p+0x1f);
	t->lack_item_id = get_s16(p+0x21);
	t->take_item = p[0x23];
	t->player_mask_a = get_u16(p+0x24);
	t->player_mask_b = get_u16(p+0x26);
	t->sound = get_s16(p+0x28);
	t->tex_on = get_s16(p+0x2a);
	t->tex_off = get_s16(p+0x2c);
	t->fire_count = get_s16(p+0x2e);
	t->unk30 = get_u32(p+0x30);
	t->skip_count = p[0x34];
	t->cond35 = get_s16(p+0x35);
	t->cond37 = get_s16(p+0x37);
	t->cond39 = get_s16(p+0x39);
	t->first_trap = get_s16(p+0x3b);
	t->enable_trigger = get_s16(p+0x3d);
	t->enable_trap = get_s16(p+0x3f);
	t->disable_trigger = get_s16(p+0x41);
	t->disable_trap = get_s16(p+0x43);
	t->object_num = p[0x45];
	t->seg = get_s16(p+0x46);
	t->side = (byte)p[0x48];
	t->next_in_seg = get_s16(p+0x49);
	t->say_index = get_s16(p+0x4b);
}

static void trap_parse(dtu_trap *tp, ubyte *p)
{
	memcpy(Trap_raw[tp - Dtu_traps], p, DTU_TAP_RECORD);
	tp->id = get_s16(p);
	tp->on = p[0x02];
	tp->charges_init = get_s16(p+0x05);
	tp->charges = get_s16(p+0x07);
	tp->sound = get_s16(p+0x0b);
	tp->next_trap = get_s16(p+0x0d);
	tp->flags = get_u32(p+0x0f);
	tp->weapon_type = get_s16(p+0x13);
	get_vec(&tp->pos, p+0x15);
	get_vec(&tp->dir, p+0x21);
	tp->spawn_object = get_s16(p+0x2d);
	tp->stat_delta = get_s16(p+0x31);
	tp->robot_type = get_s16(p+0x33);
	tp->robot_arg = get_s16(p+0x35);
	tp->tex_disabled = get_s16(p+0x37);
	tp->tex_enabled = get_s16(p+0x39);
	tp->light_delta = get_s16(p+0x3f);
	tp->delay_init = get_u16(p+0x41);
	tp->seg = get_s16(p+0x43);
	tp->side = get_s16(p+0x45);
	tp->interval = get_u16(p+0x47);
	tp->delay = get_u16(p+0x49);
	tp->timestamp = get_u32(p+0x4b);
}

static void put_s16(ubyte *p, int v)
{
	p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8);
}

static void put_u32(ubyte *p, uint v)
{
	p[0] = (ubyte)v; p[1] = (ubyte)(v >> 8); p[2] = (ubyte)(v >> 16); p[3] = (ubyte)(v >> 24);
}

static void put_vec(ubyte *p, vms_vector *v)
{
	put_u32(p, (uint)v->x);
	put_u32(p+4, (uint)v->y);
	put_u32(p+8, (uint)v->z);
}

static int trig_table_check(int repair);

// ASCENT: the time units of a trap. The executable keeps the times of a
// trap as seconds * 18.2 (0x9596F) and stores them so in the state files.
// With Dtu_fix_trap_seconds the port counts seconds * 100; the state files
// keep the unit of the executable.
static ushort trap_time_from_state(ushort v)
{
	int t;

	if (!Dtu_fix_trap_seconds)
		return v;
	t = (int)v * 1000 / 182;
	return (ushort)(t > 65500 ? 65500 : t);
}

static ushort trap_time_to_state(ushort v)
{
	if (!Dtu_fix_trap_seconds)
		return v;
	return (ushort)((int)v * 182 / 1000);
}

int dtu_trig_read_tig(ubyte *data, int size, int from_state)
{
	int i, n;

	if (size < 6)
		return 0;
	n = get_s16(data+4);
	if (get_u32(data) != 2 || n < (from_state ? 0 : 1) || n > DTU_MAX_TRIGGERS || size < DTU_TIG_HEADER + DTU_TIG_RECORD*n)
		return 0;
	for (i=0; i<DTU_TRIG_SEG_TABLE; i++)
		Dtu_trigger_seg_table[i] = get_s16(data + 6 + 2*i);
	for (i=0; i<n; i++)
		trig_parse(&Dtu_triggers[i], data + DTU_TIG_HEADER + DTU_TIG_RECORD*i);
	Dtu_num_triggers = n;
	if (from_state && trig_table_check(Dtu_fix_trigger_table))
		tlog("state: the segment table of the triggers is %s", Dtu_fix_trigger_table ? "made again" : "left as in the file");
	return 1;
}

int dtu_trig_read_tap(ubyte *data, int size, int from_state)
{
	int i, n;

	if (size < 6)
		return 0;
	n = get_s16(data+4);
	if (get_u32(data) != 2 || n < 0 || n > DTU_MAX_TRAPS || size < 6 + DTU_TAP_RECORD*n)
		return 0;
	for (i=0; i<n; i++) {
		dtu_trap *tp = &Dtu_traps[i];

		trap_parse(tp, data + 6 + DTU_TAP_RECORD*i);
		if (from_state) {
			// 0x9550D with 0x13CE31 set: the times of a state file are
			// clock units already, and +0x41 is the delay.
			tp->interval = trap_time_from_state(tp->interval);
			tp->delay = trap_time_from_state(tp->delay);
			tp->delay_init = trap_time_from_state(tp->delay_init);
			continue;
		}
		// 0x9596F: seconds to clock units. The executable
		// multiplies by 18.2 and cuts the fraction, and its clock
		// counts 1/100 second.
		if (Dtu_fix_trap_seconds) {
			tp->interval = tp->interval > 655 ? 65500 : tp->interval * 100;
			tp->delay = tp->delay > 655 ? 65500 : tp->delay * 100;
		}
		else {
			tp->interval = (ushort)(int)(tp->interval * 18.2);
			tp->delay = (ushort)(int)(tp->delay * 18.2);
		}
		tp->delay_init = tp->delay;
	}
	Dtu_num_traps = n;
	return 1;
}

// 0x95E0A: u32 2, s16 count, the segment table, the records.
ubyte *dtu_trig_trap_raw(int t)
{
	return (t >= 0 && t < Dtu_num_traps) ? Trap_raw[t] : NULL;
}

ubyte *dtu_trig_write_tig(int *size)
{
	int i, n = Dtu_num_triggers;
	ubyte *b;

	*size = DTU_TIG_HEADER + DTU_TIG_RECORD*n;
	b = calloc(1, *size);
	if (!b)
		return NULL;
	put_u32(b, 2);
	put_s16(b+4, n);
	for (i=0; i<DTU_TRIG_SEG_TABLE; i++)
		put_s16(b + 6 + 2*i, Dtu_trigger_seg_table[i]);
	for (i=0; i<n; i++) {
		dtu_trigger *t = &Dtu_triggers[i];
		ubyte *p = b + DTU_TIG_HEADER + DTU_TIG_RECORD*i;

		memcpy(p, Trig_raw[i], DTU_TIG_RECORD);
		put_s16(p, t->unk00);
		put_vec(p+0x02, &t->pos);
		p[0x0e] = t->on;
		put_u32(p+0x0f, t->unk0f);
		put_s16(p+0x13, t->flags);
		put_s16(p+0x15, t->activation_mask);
		put_s16(p+0x17, t->unk17);
		put_s16(p+0x19, t->need_item_class);
		put_s16(p+0x1b, t->need_item_id);
		put_s16(p+0x1d, t->unk1d);
		put_s16(p+0x1f, t->lack_item_class);
		put_s16(p+0x21, t->lack_item_id);
		p[0x23] = t->take_item;
		put_s16(p+0x24, t->player_mask_a);
		put_s16(p+0x26, t->player_mask_b);
		put_s16(p+0x28, t->sound);
		put_s16(p+0x2a, t->tex_on);
		put_s16(p+0x2c, t->tex_off);
		put_s16(p+0x2e, t->fire_count);
		put_u32(p+0x30, t->unk30);
		p[0x34] = t->skip_count;
		put_s16(p+0x35, t->cond35);
		put_s16(p+0x37, t->cond37);
		put_s16(p+0x39, t->cond39);
		put_s16(p+0x3b, t->first_trap);
		put_s16(p+0x3d, t->enable_trigger);
		put_s16(p+0x3f, t->enable_trap);
		put_s16(p+0x41, t->disable_trigger);
		put_s16(p+0x43, t->disable_trap);
		p[0x45] = t->object_num;
		put_s16(p+0x46, t->seg);
		p[0x48] = (ubyte)t->side;
		put_s16(p+0x49, t->next_in_seg);
		put_s16(p+0x4b, t->say_index);
	}
	return b;
}

// 0x95FB8: u32 2, s16 count, the records. Each trap gets its field +0x41
// back from +0x49 before the write.
ubyte *dtu_trig_write_tap(int *size)
{
	int i, n = Dtu_num_traps;
	ubyte *b;

	*size = 6 + DTU_TAP_RECORD*n;
	b = calloc(1, *size);
	if (!b)
		return NULL;
	put_u32(b, 2);
	put_s16(b+4, n);
	for (i=0; i<n; i++) {
		dtu_trap *tp = &Dtu_traps[i];
		ubyte *p = b + 6 + DTU_TAP_RECORD*i;

		tp->delay_init = tp->delay;
		memcpy(p, Trap_raw[i], DTU_TAP_RECORD);
		put_s16(p, tp->id);
		p[0x02] = tp->on;
		put_s16(p+0x05, tp->charges_init);
		put_s16(p+0x07, tp->charges);
		put_s16(p+0x0b, tp->sound);
		put_s16(p+0x0d, tp->next_trap);
		put_u32(p+0x0f, tp->flags);
		put_s16(p+0x13, tp->weapon_type);
		put_vec(p+0x15, &tp->pos);
		put_vec(p+0x21, &tp->dir);
		put_s16(p+0x2d, tp->spawn_object);
		put_s16(p+0x31, tp->stat_delta);
		put_s16(p+0x33, tp->robot_type);
		put_s16(p+0x35, tp->robot_arg);
		put_s16(p+0x37, tp->tex_disabled);
		put_s16(p+0x39, tp->tex_enabled);
		put_s16(p+0x3f, tp->light_delta);
		put_s16(p+0x41, trap_time_to_state(tp->delay_init));
		put_s16(p+0x43, tp->seg);
		put_s16(p+0x45, tp->side);
		put_s16(p+0x47, trap_time_to_state(tp->interval));
		put_s16(p+0x49, trap_time_to_state(tp->delay));
		put_u32(p+0x4b, tp->timestamp);
	}
	return b;
}

// Counts the triggers that the table of the file does not reach, or
// reaches from another segment than their own. With repair set, makes
// the table and the chains again from the seg fields.
static int trig_table_check(int repair)
{
	static ubyte reached[DTU_MAX_TRIGGERS];
	short last[DTU_TRIG_SEG_TABLE];
	int i, n, steps, bad = 0;

	memset(reached, 0, sizeof(reached));
	for (i=0; i<DTU_TRIG_SEG_TABLE; i++) {
		n = Dtu_trigger_seg_table[i];
		for (steps=0; n >= 0 && n < Dtu_num_triggers && steps < DTU_MAX_TRIGGERS; steps++) {
			if (Dtu_triggers[n].seg == i)
				reached[n] = 1;
			n = Dtu_triggers[n].next_in_seg;
		}
	}
	for (i=0; i<Dtu_num_triggers; i++)
		if (!reached[i]) {
			bad++;
			tlog("trigger %d (seg %d side %d) is not in the chain of its segment", i, Dtu_triggers[i].seg, Dtu_triggers[i].side);
		}

	if (!repair || !bad)
		return bad;

	for (i=0; i<DTU_TRIG_SEG_TABLE; i++)
		Dtu_trigger_seg_table[i] = last[i] = -1;
	for (i=0; i<Dtu_num_triggers; i++) {
		int seg = Dtu_triggers[i].seg;

		Dtu_triggers[i].next_in_seg = -1;
		if (seg < 0 || seg >= DTU_TRIG_SEG_TABLE)
			continue;
		if (last[seg] == -1)
			Dtu_trigger_seg_table[seg] = i;
		else
			Dtu_triggers[last[seg]].next_in_seg = i;
		last[seg] = i;
	}
	return bad;
}

static void trig_free(void)
{
	if (Say_data)
		free(Say_data);
	Say_data = NULL;
}

static void read_options(void)
{
	if (Options_read)
		return;
	Options_read = 1;
	atexit(trig_free);
	if (FindArg("-triglog"))
		Dtu_trig_log = 1;
	if (FindArg("-origtrig"))		// the rules of the executable
		Dtu_fix_trigger_table = Dtu_fix_trap_seconds = Dtu_fix_trap_close_door = Dtu_fix_trigger_repeat = 0;
	if (FindArg("-trapseconds"))
		Dtu_fix_trap_seconds = 1;
}

static void make_name(char *dest, char *level_name, char *ext)
{
	int i;

	for (i=0; i<8 && level_name[i] && level_name[i] != '.'; i++)
		dest[i] = toupper(level_name[i]);
	dest[i] = 0;
	strcat(dest, ext);
}

void dtu_trig_load_level(char *level_name, int level_index)
{
	char name[20];
	ubyte *data;
	int size, i, n, bad;

	read_options();

	Flame_talk_time = -1;
	// 0x955F5, 0x95459
	Dtu_num_triggers = 0;
	Dtu_num_traps = 0;
	Num_active_traps = 0;
	Trap_chain_stop = 0;
	for (i=0; i<DTU_TRIG_SEG_TABLE; i++)
		Dtu_trigger_seg_table[i] = -1;
	if (Say_data) {
		free(Say_data);
		Say_data = NULL;
	}
	Say_size = 0;
	Level_index = level_index;
	Last_player_seg = -1;
	Floor_trigger_seg = -1;
	Last_use_time = -F1_0;

	make_name(name, level_name, ".TIG");
	data = dtu_load_file(name, &size);
	if (data) {
		if (!dtu_trig_read_tig(data, size, 0))
			tlog("%s: no triggers (version %u, count %d, size %d)", name, size >= 4 ? get_u32(data) : 0,
				size >= 6 ? get_s16(data+4) : 0, size);
		free(data);
	}
	else
		tlog("no file %s", name);

	make_name(name, level_name, ".TAP");
	data = dtu_load_file(name, &size);
	if (data) {
		if (!dtu_trig_read_tap(data, size, 0))
			tlog("%s: no traps (count %d, size %d)", name, size >= 6 ? get_s16(data+4) : -1, size);
		free(data);
	}
	else
		tlog("no file %s", name);

	make_name(name, level_name, ".SAY");
	make_name(Say_level, level_name, "");
	Say_data = dtu_load_file(name, &Say_size);
	if (!Say_data)
		tlog("no file %s", name);

	bad = trig_table_check(Dtu_fix_trigger_table);
	if (bad)
		tlog("%d triggers are not in the chain of their segment: %s", bad,
			Dtu_fix_trigger_table ? "table made again from the records" : "left as in the file");

	warp_load(level_index);

	tlog("level %s (%d): %d triggers, %d traps, %d messages", level_name, level_index,
		Dtu_num_triggers, Dtu_num_traps, (Say_data && Say_size >= 8) ? (int)get_u32(Say_data+4) : 0);
}
