/*
 * ASCENT: new file. The cheat codes of DTUGAME.EXE 1.3. They take the
 * place of the Descent 1 codes of game.c, which do not run in the game of
 * the port.
 *
 * The key function 0x42F9C tests each key against seven tables of scan
 * codes (0x13B74F to 0x13B7A2). Each table has its own index (0x1C7F10 to
 * 0x1C7F34). A key that matches advances the index; any other key sets it
 * to 0. When the index reaches the length of the table, the code acts. No
 * code needs the first one: only "zazb" is skipped in a multiplayer game
 * (Game_mode & 0x26, 0x43241).
 *
 * Port decisions:
 * - "glassy" writes the int at +0x16A of each object (tmap_override). The
 *   port changes only polygon objects; for a vclip object the executable
 *   writes over the frame number, which has no lasting effect.
 * - The value 14 to 18 is a texture number of the level (as the level
 *   state stores it, dtustate.c), so it goes through Level_tmap_xlate.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtulang.h"		// ASCENT: the language of the text
#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "key.h"
#include "game.h"
#include "object.h"
#include "digi.h"
#include "bm.h"
#include "gauges.h"
#include "gamemine.h"
#include "dtucheat.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"
#include "dtuscript.h"
#include "dtulog.h"

#define CHEAT_SOUND		200		// 0x2D955(0xC8, 0x10000)

static const ubyte Zazb[] =			{ KEY_Z, KEY_A, KEY_Z, KEY_B };	// 0x13B74F
static const ubyte Glassy[] =			{ KEY_G, KEY_L, KEY_A, KEY_S, KEY_S, KEY_Y };	// 0x13B753
static const ubyte Shrink[] =			{ KEY_S, KEY_H, KEY_R, KEY_I, KEY_N, KEY_K, KEY_I, KEY_W,
											  KEY_A, KEY_N, KEY_N, KEY_A, KEY_K, KEY_I, KEY_L, KEY_L };	// 0x13B782
static const ubyte Gadzooks[] =		{ KEY_G, KEY_A, KEY_D, KEY_Z, KEY_O, KEY_O, KEY_K, KEY_S };	// 0x13B79B
static const ubyte Eight[] =			{ KEY_E, KEY_I, KEY_G, KEY_H, KEY_T, KEY_I, KEY_S, KEY_G,
											  KEY_R, KEY_E, KEY_A, KEY_T };	// 0x13B776
static const ubyte Kevorkian[] =		{ KEY_K, KEY_E, KEY_V, KEY_O, KEY_R, KEY_K, KEY_I, KEY_A, KEY_N };	// 0x13B792
static const ubyte Mesmart[] =		{ KEY_M, KEY_E, KEY_S, KEY_M, KEY_A, KEY_R, KEY_T };	// 0x13B759

static int Zazb_i, Glassy_i, Shrink_i, Gadzooks_i, Eight_i, Kevorkian_i, Mesmart_i;

// Returns 1 when the key completes the code.
static int step(const ubyte *code, int len, int *i, int key)
{
	if (key != code[*i]) {
		*i = 0;
		return 0;
	}
	if (++*i < len)
		return 0;
	*i = 0;
	return 1;
}

// 0xB6390(type, id): a new item for the player. The uses of an item
// without a stack are its charges (as dtuscript.c gives a MISC item).
static void give(int type, int id)
{
	dtu_item_data d;
	int count = 1;

	if (dtu_item_get(type, id, &d) && !d.stackable && d.uses > 0)
		count = d.uses;
	dtu_inventory_add(type, id, count);
}

void dtu_cheat_key(int key)
{
	int i, j;

	// 0x4324E: the cheater flag 0x13B7A3 (Cheats_enabled: the map shows
	// all with Alt+F, 0x189A0; no experience is gained, 0x4EA85) and the
	// experience of both classes (P+0x77, P+0x7B) is 0
	if (!(Game_mode & GM_MULTI)) {
		if (step(Zazb, sizeof(Zazb), &Zazb_i, key)) {
			digi_play_sample(CHEAT_SOUND, F1_0);
			Cheats_enabled = 1;
			Dtu_char.experience[0] = Dtu_char.experience[1] = 0;
			dtulog_event("cheat zazb");
		}
	}
	else
		Zazb_i = 0;

	// 0x428B9: "glassy": each object but the last gets texture 14 to 18
	if (step(Glassy, sizeof(Glassy), &Glassy_i, key)) {
		for (i = 0; i <= Highest_object_index - 1; i++) {
			object *o = &Objects[i];
			int t = rand() % 5 + 14;

			if (o->render_type != RT_POLYOBJ)
				continue;
			o->dtu.tmap_override_level = t;
			o->rtype.pobj_info.tmap_override = t < Num_level_tmap_names ? Level_tmap_xlate[t] : -1;
		}
		digi_play_sample(CHEAT_SOUND, F1_0);
		dtulog_event("cheat glassy");
	}

	// 0x4292A: weapon 0x36 into the inventory (0xB65BB with the fields of
	// Weapon_info entry 0x36), message 0x125757
	if (step(Shrink, sizeof(Shrink), &Shrink_i, key)) {
		give(OBJ_WEAPON, 0x36);
		HUD_init_message("The sword of win!");
		dtulog_event("cheat shrinkiwannakill");
	}

	// 0x429A8: message 0x125769, then global script 0x1FF (0xAC055(0,
	// 0x1FF)), the level select menu
	if (step(Gadzooks, sizeof(Gadzooks), &Gadzooks_i, key)) {
		HUD_init_message("Ready or not, here we go!");
		dtulog_event("cheat gadzooks");
		dtu_script_run_id(0x1FF);
	}

	// 0x429E9: message 0x125783, MISC items 0x12, 0x13, 0x14 and 0x16 to
	// 0x1A (0xB6390(0x10, id))
	if (step(Eight, sizeof(Eight), &Eight_i, key)) {
		static const ubyte ids[] = { 0x12, 0x13, 0x14, 0x16, 0x17, 0x18, 0x19, 0x1A };

		HUD_init_message("The Queen of the Demonweb Pits awaits!");
		for (i = 0; i < (int) sizeof(ids); i++)
			give(OBJ_MISC, ids[i]);
		dtulog_event("cheat eightisgreat");
	}

	// 0x42A9E: message 0x1257AA; each creature whose segment sees the
	// segment of the player (0x9810D) gets all its hit points as damage
	// with no attacker (0x29A9C)
	if (step(Kevorkian, sizeof(Kevorkian), &Kevorkian_i, key)) {
		HUD_init_message("They were sick anyway....");
		for (i = 0; i <= Highest_object_index; i++) {
			object *o = &Objects[i];

			if (o->type == OBJ_ROBOT && dtu_brain_sees_player(o))
				dtu_damage_creature_direct(o, NULL, o->shields);
		}
		dtulog_event("cheat kevorkian");
	}

	// 0x42B35: every spell (0xB6390(0x11, n) for n < N_spell_types, 0x13A853),
	// 100 spell slots of each level (maximum and left, P+0x23 to P+0x42), and
	// the class levels 4 (P+0xAF, P+0xB1) and the byte P+0x9C 4 in the four
	// player records
	if (step(Mesmart, sizeof(Mesmart), &Mesmart_i, key)) {
		for (i = 0; i < N_spell_types; i++)
			give(OBJ_SPELL, i);
		for (i = 0; i < 2; i++)
			for (j = 0; j < (i ? 7 : 9); j++)
				Dtu_spell_slots_max[i][j] = Dtu_spell_slots_left[i][j] = 100;
		Dtu_char.pa.unk_05 = 4;
		Dtu_char.pa.class1_level = 4;
		Dtu_char.pa.class2_level = 4;
		digi_play_sample(CHEAT_SOUND, F1_0);
		dtulog_event("cheat mesmart");
	}
}
