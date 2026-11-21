/*
 * ASCENT: new file. A soak test of the port, with no part in DTUGAME.EXE.
 * The option -soak SEED plays the level with random actions that a player
 * can do (walk, turn, jump, attack, use, the thief keys, turn undead,
 * spells, items, dialogue replies) and checks the state of the objects
 * after each frame. Use it with -level NAME -frames N -mute. A problem is
 * printed as a line that starts with "SOAK FAIL". A crash signal prints
 * "SOAK CRASH" with the last action. See docs/re/bugs.md.
 *
 * Options:
 *   -soak SEED        start the test (SEED > 0 selects the random numbers)
 *   -soakdeath        do not heal the player (the death and the new start
 *                     of the level are tested too)
 *   -soakabort        abort() on the first problem (for a debugger)
 *   -soaklog FILE     each action in FILE (to find the last one of a hang)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "vecmat.h"
#include "error.h"
#include "args.h"
#include "game.h"
#include "segment.h"
#include "gameseg.h"
#include "object.h"
#include "player.h"
#include "bm.h"
#include "key.h"
#include "weapon.h"
#include "robot.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtucombat.h"
#include "dtudlg.h"
#include "dtusoak.h"
#include "kconfig.h"		// ASCENT: the key table

// object.c: the number of objects in use
extern int num_objects;

// key.c: a key event, as the platform layer sends it
extern void key_handler(int scancode_in, int down);

int Dtu_soak = 0;

static unsigned int Rng = 1;
static int Soak_death = 0, Soak_abort = 0;
static FILE *Soak_log = NULL;
static int Frame = 0, Fails = 0, Checks = 0;
static char Last_action[160] = "start";
static int Done = 0;

// Keys that the test holds, and the frame of their release
static struct { int key, until; } Held[16];
static int N_held = 0;

static int rnd(int n)
{
	// xorshift32: the random numbers of the test do not use rand(), so
	// the game code sees the same rand() sequence for one seed
	Rng ^= Rng << 13;
	Rng ^= Rng >> 17;
	Rng ^= Rng << 5;
	return n > 0 ? (int)(Rng % (unsigned)n) : 0;
}

static void action(char *format, ...)
{
	va_list ap;

	va_start(ap, format);
	vsnprintf(Last_action, sizeof(Last_action), format, ap);
	va_end(ap);
	if (Soak_log) {
		fprintf(Soak_log, "%d %.3f %s\n", Frame, f2fl(GameTime), Last_action);
		fflush(Soak_log);
	}
}

static void fail(char *format, ...)
{
	static char last[256];
	static int repeats = 0;
	char text[256];
	va_list ap;

	va_start(ap, format);
	vsnprintf(text, sizeof(text), format, ap);
	va_end(ap);
	Fails++;
	// the same problem in each frame is printed a few times only
	if (!strcmp(text, last)) {
		if (++repeats > 3)
			return;
	}
	else {
		repeats = 0;
		strcpy(last, text);
	}
	printf("SOAK FAIL frame %d time %.3f: %s (last action: %s)\n", Frame, f2fl(GameTime), text, Last_action);
	fflush(stdout);
	if (Soak_log) {
		fprintf(Soak_log, "FAIL %d %s\n", Frame, text);
		fflush(Soak_log);
	}
	if (Soak_abort)
		abort();
}

static void crash_handler(int sig)
{
	printf("SOAK CRASH signal %d frame %d time %.3f, last action: %s\n", sig, Frame, f2fl(GameTime), Last_action);
	fflush(stdout);
	if (Soak_log) {
		fprintf(Soak_log, "CRASH signal %d frame %d\n", sig, Frame);
		fflush(Soak_log);
	}
	_exit(100 + sig);
}

// The end of the program before the frame limit: the reason
static void soak_exit(void)
{
	if (!Done)
		printf("SOAK exit at frame %d: player dead %d, Function_mode %d (last action: %s)\n", Frame, Dtu_player_dead || Player_is_dead, Function_mode, Last_action);
}

void dtu_soak_init(void)
{
	int t;

	if ((t = FindArg("-soak")) == 0 || t+1 >= Num_args)
		return;
	Dtu_soak = 1;
	Rng = (unsigned) atoi(Args[t+1]) * 2654435761u + 1;
	if (Rng == 0) Rng = 1;
	srand(atoi(Args[t+1]));
	Soak_death = FindArg("-soakdeath") != 0;
	Soak_abort = FindArg("-soakabort") != 0;
	if ((t = FindArg("-soaklog")) != 0 && t+1 < Num_args)
		Soak_log = fopen(Args[t+1], "wt");
	signal(SIGSEGV, crash_handler);
	signal(SIGFPE, crash_handler);
	signal(SIGILL, crash_handler);
	signal(SIGABRT, crash_handler);
	atexit(soak_exit);
	// the choice boxes of the items answer without a window
	Dtu_auto_choice = 0;
	printf("SOAK start seed %s\n", Args[FindArg("-soak")+1]);
}

//------------------------------------------------------------------------
// Checks

static void check_objects(void)
{
	static int in_list[MAX_OBJECTS];
	int i, n = 0, s;
	char msg[200];

	Checks++;
	if (Highest_object_index < 0 || Highest_object_index >= MAX_OBJECTS)
		fail("Highest_object_index %d", Highest_object_index);
	if (ConsoleObject == NULL || ConsoleObject != &Objects[Players[Player_num].objnum])
		fail("ConsoleObject is not the object of the player");

	memset(in_list, 0, sizeof(in_list));
	for (s=0; s<=Highest_segment_index; s++) {
		int o = Segments[s].objects, steps = 0, prev = -1;

		while (o != -1) {
			if (o < 0 || o >= MAX_OBJECTS) {
				fail("segment %d: object number %d in its list", s, o);
				break;
			}
			if (++steps > MAX_OBJECTS) {
				fail("segment %d: the object list has a loop", s);
				break;
			}
			if (Objects[o].type == OBJ_NONE)
				fail("segment %d: free object %d in its list", s, o);
			if (Objects[o].segnum != s)
				fail("segment %d: object %d (type %d) has segnum %d", s, o, Objects[o].type, Objects[o].segnum);
			if (Objects[o].prev != prev)
				fail("segment %d: object %d prev %d, not %d", s, o, Objects[o].prev, prev);
			if (in_list[o]++)
				fail("object %d is in two segment lists", o);
			prev = o;
			o = Objects[o].next;
		}
	}

	for (i=0; i<=Highest_object_index; i++) {
		object *obj = &Objects[i];

		if (obj->type == OBJ_NONE)
			continue;
		n++;
		if (obj->segnum < 0 || obj->segnum > Highest_segment_index) {
			fail("object %d (type %d id %d) is in segment %d", i, obj->type, obj->id, obj->segnum);
			continue;
		}
		if (!in_list[i])
			fail("object %d (type %d id %d) is not in the list of segment %d", i, obj->type, obj->id, obj->segnum);
		if (obj->type == OBJ_ROBOT && obj->id >= N_robot_types)
			fail("object %d: robot id %d", i, obj->id);
		if (obj->type == OBJ_WEAPON && obj->id >= N_weapon_types)
			fail("object %d: weapon id %d", i, obj->id);
		// an object outside its segment (every 16 frames: the test is slow)
		if (((Frame + i) & 15) == 0 && (obj->type == OBJ_PLAYER || obj->type == OBJ_ROBOT)) {
			segmasks m = get_seg_masks(&obj->pos, obj->segnum, 0);

			if (m.centermask != 0 && find_point_seg(&obj->pos, obj->segnum) < 0)
				fail("object %d (type %d %s) at %.2f %.2f %.2f is outside the mine (segment %d)", i, obj->type,
					dtu_object_name(obj), f2fl(obj->pos.x), f2fl(obj->pos.y), f2fl(obj->pos.z), obj->segnum);
		}
	}
	if (n != num_objects)
		fail("num_objects %d, counted %d", num_objects, n);

	if (Dtu_n_inventory < 0 || Dtu_n_inventory > DTU_MAX_INVENTORY)
		fail("Dtu_n_inventory %d", Dtu_n_inventory);
	for (i=0; i<Dtu_n_inventory && i<DTU_MAX_INVENTORY; i++)
		if (Dtu_inventory[i].type == 0 || Dtu_inventory[i].count < 0)
			fail("inventory %d: type %d count %d", i, Dtu_inventory[i].type, Dtu_inventory[i].count);
	if (Dtu_gold < 0)
		fail("gold %d", Dtu_gold);

	if (dtu_objects_check(msg, sizeof(msg)))
		fail("%s", msg);
}

//------------------------------------------------------------------------
// Actions

static void hold(int key, int frames)
{
	int i;

	for (i=0; i<N_held; i++)
		if (Held[i].key == key) {
			Held[i].until = Frame + frames;
			return;
		}
	if (N_held >= (int)(sizeof(Held)/sizeof(Held[0])))
		return;
	Held[N_held].key = key;
	Held[N_held].until = Frame + frames;
	N_held++;
	keyd_pressed[key] = 1;
}

// ASCENT: holds the first key of an action of the key table (kconfig.c),
// so that the test follows a changed table
static void hold_act(int act, int frames)
{
	int key = dtu_action_key(act, 0);

	if (key == 255)
		key = dtu_action_key(act, 1);
	if (key != 255)
		hold(key, frames);
}

static void release_keys(void)
{
	int i;

	for (i=0; i<N_held; ) {
		if (Held[i].until <= Frame) {
			keyd_pressed[Held[i].key] = 0;
			Held[i] = Held[--N_held];
		}
		else
			i++;
	}
}

static object *random_creature(int alive)
{
	int tries;

	for (tries=0; tries<50; tries++) {
		object *o = &Objects[rnd(Highest_object_index + 1)];

		if (o->type != OBJ_ROBOT)
			continue;
		if (alive && (o->shields < F1_0 || DTU_ANIM_STATE(o) == DAS_DIE))
			continue;
		return o;
	}
	return NULL;
}

static void teleport(void)
{
	object *pl = ConsoleObject;
	object *c = rnd(2) ? random_creature(1) : NULL;
	int seg = c ? c->segnum : rnd(Highest_segment_index + 1);
	vms_vector pos;

	if (seg < 0 || seg > Highest_segment_index)
		return;
	compute_segment_center(&pos, &Segments[seg]);
	action("teleport to segment %d%s", seg, c ? " (a creature)" : "");
	pl->pos = pos;
	obj_relink(pl-Objects, seg);
	vm_vec_zero(&pl->mtype.phys_info.velocity);
}

static void random_action(void)
{
	static const int move_acts[] = { DTU_ACT_FORWARD, DTU_ACT_BACK, DTU_ACT_TURN_LEFT, DTU_ACT_TURN_RIGHT,
		DTU_ACT_SLIDE_LEFT, DTU_ACT_SLIDE_RIGHT, DTU_ACT_RUN, DTU_ACT_LOOK_UP, DTU_ACT_LOOK_DOWN };
	object *pl = ConsoleObject, *c;
	int r = rnd(100), i, k;

	if (r < 30) {
		k = rnd(3) + 1;
		for (i=0; i<k; i++) {
			hold_act(move_acts[rnd(sizeof(move_acts)/sizeof(move_acts[0]))], 5 + rnd(60));
		}
		action("move, %d keys", k);
	}
	else if (r < 38) {
		hold_act(DTU_ACT_JUMP, 2);
		action("jump");
	}
	else if (r < 50) {
		hold_act(DTU_ACT_ATTACK, 2 + rnd(40));
		action("attack");
	}
	else if (r < 55) {
		hold_act(DTU_ACT_ACTIVATE, 2 + rnd(10));
		action("use (E)");
	}
	else if (r < 58) {
		static const int tk[] = { DTU_ACT_OPEN_LOCKS, DTU_ACT_FIND_TRAPS, DTU_ACT_MOVE_SILENTLY, DTU_ACT_TURN_UNDEAD };
		int act = tk[rnd(4)];
		k = dtu_action_key(act, 0);
		hold_act(act, 2);
		action("key %d", k);
	}
	else if (r < 62) {
		int level = 1 + rnd(14);
		action("turn undead, level %d", level);
		dtu_combat_turn_undead(level);
	}
	else if (r < 70 && N_spell_types > 0 && (c = random_creature(1)) != NULL) {
		int spell = rnd(N_spell_types);
		action("player casts spell %d on obj %d", spell, (int)(c-Objects));
		dtu_spell_cast_on(spell, c, pl);
	}
	else if (r < 74 && N_spell_types > 0 && (c = random_creature(1)) != NULL) {
		int spell = rnd(N_spell_types);
		object *t = rnd(2) ? pl : random_creature(1);
		if (t) {
			action("obj %d casts spell %d on obj %d", (int)(c-Objects), spell, (int)(t-Objects));
			dtu_spell_cast_on(spell, t, c);
		}
	}
	else if (r < 77 && N_spell_types > 0) {
		int spell = rnd(N_spell_types);
		action("learn and select spell %d, attack", spell);
		dtu_spell_learn(spell);
		dtu_spell_select(spell);
		hold_act(DTU_ACT_ATTACK, 2 + rnd(20));
	}
	else if (r < 81 && Dtu_n_inventory > 0) {
		i = rnd(Dtu_n_inventory);
		Dtu_auto_choice = rnd(3);
		action("use item %d (type %d id %d)", i, Dtu_inventory[i].type, Dtu_inventory[i].id);
		dtu_char_use(i);
	}
	else if (r < 83 && Dtu_n_inventory > 0) {
		i = rnd(Dtu_n_inventory);
		action("equip item %d (type %d id %d)", i, Dtu_inventory[i].type, Dtu_inventory[i].id);
		if (rnd(2))
			dtu_char_equip_auto(i);
		else
			dtu_char_set_current(i);
	}
	else if (r < 85 && Dtu_n_inventory > 0) {
		i = rnd(Dtu_n_inventory);
		action("drop item %d (type %d id %d)", i, Dtu_inventory[i].type, Dtu_inventory[i].id);
		dtu_char_drop(i);
	}
	else if (r < 88) {
		static const int types[] = { OBJ_WEAPON, OBJ_ARMOR, OBJ_MISC, OBJ_SPELL };
		int type = types[rnd(4)];
		int n = type == OBJ_WEAPON ? N_weapon_types : type == OBJ_ARMOR ? N_armor_types : type == OBJ_MISC ? N_misc_types : N_spell_types;
		if (n > 0) {
			int id = rnd(n);
			action("take item type %d id %d", type, id);
			dtu_char_take(type, id, 1 + rnd(3));
		}
	}
	else if (r < 91 && (c = random_creature(1)) != NULL) {
		fix d = i2f(1 + rnd(30));
		action("damage obj %d by %d", (int)(c-Objects), f2i(d));
		dtu_damage_creature_direct(c, rnd(2) ? pl : NULL, d);
	}
	else if (r < 93 && (c = random_creature(1)) != NULL) {
		action("wake obj %d", (int)(c-Objects));
		dtu_creature_wake(c, rnd(2));
	}
	else if (r < 95)
		teleport();
	else
		action("wait");
}

// The dialogue window: the reply keys
static void answer_dialogue(void)
{
	static int next = 0;

	if (Frame < next)
		return;
	next = Frame + 10 + rnd(20);
	switch (rnd(4)) {
		case 0:
			key_handler(KEY_SPACEBAR, 1); key_handler(KEY_SPACEBAR, 0);
			action("dialogue: space");
			break;
		case 1:
			key_handler(KEY_DOWN, 1); key_handler(KEY_DOWN, 0);
			action("dialogue: down");
			break;
		default:
			key_handler(KEY_ENTER, 1); key_handler(KEY_ENTER, 0);
			action("dialogue: enter");
			break;
	}
}

void dtu_soak_frame(void)
{
	static int next_action = 0;
	object *pl = ConsoleObject;

	if (!Dtu_soak)
		return;
	Frame++;
	check_objects();
	release_keys();

	if (pl == NULL || pl->type != OBJ_PLAYER || Player_is_dead || Dtu_player_dead)
		return;

	// keep the player alive
	if (!Soak_death && dtu_char_hit_points() < dtu_char_max_hit_points())
		dtu_char_heal(dtu_char_max_hit_points());

	if (dtu_dlg_is_open()) {
		answer_dialogue();
		return;
	}

	if (Frame >= next_action) {
		random_action();
		next_action = Frame + 3 + rnd(30);
	}
}

void dtu_soak_done(void)
{
	if (!Dtu_soak)
		return;
	Done = 1;
	printf("SOAK end: %d frames, %d checks, %d problems\n", Frame, Checks, Fails);
	fflush(stdout);
	if (Soak_log)
		fclose(Soak_log);
}
