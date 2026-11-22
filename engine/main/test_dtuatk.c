/*
 * ASCENT: test of the attack timing rules of the player (dtuatk.c). The
 * expected values of the executable come from the code of DTUGAME.EXE 1.3
 * (0x441E9, 0x41E36, 0x56A0D, 0x44300, 0x44589, 0xAFB55; docs/re/combat.md,
 * section A8). The fire_wait values are those of the $WEAPON lines of BANE.BIN and
 * the model time is state_times[6] of the $PLAYER line.
 *
 * The level and the class of the character are no input of any of these
 * rules: a fighter of level 12 attacks at the rate of a fighter of level 1
 * (the gate 0x41E36 reads the model time, the game time and the Haste bit
 * only). This is why the functions have no such argument.
 */

#include <stdio.h>

#include "fix.h"
#include "dtuatk.h"

#define SEC(x)	((fix) ((x) * 65536.0 + 0.5))

// fire_wait= of BANE.BIN
#define FW_LONG_SWORD	SEC(1.0)
#define FW_DAGGER			SEC(0.6)		// a fast weapon
#define FW_QUICKNESS		SEC(0.5)		// Short Sword of Quickness
#define FW_WARHAMMER		SEC(1.5)		// a slow weapon
#define FW_ARROW			SEC(2.0)
#define FW_BOLT			SEC(3.0)
#define FW_DART			SEC(0.5)
#define MODEL_ATTACK		SEC(1.0)		// state_times[6] of the player models

static int failures;

static void check(const char *what, fix got, fix expect)
{
	if (got != expect) {
		printf("FAIL: %s = %.4f, expected %.4f\n", what, got / 65536.0, expect / 65536.0);
		failures++;
	}
}

// The time from one accepted melee attack to the next, with the item
// bits of the weapon in the hand (pc_spell of its line, -1 for none)
static fix melee_item(fix fire_wait, int effects, int pc_spell, int fixes)
{
	fix st;

	if (dtu_atk_item_bits(pc_spell, fixes) & (1 << 9))
		effects |= DTU_ATK_HASTE;
	st = dtu_atk_state_time(1, fire_wait, MODEL_ATTACK, effects, fixes);
	return dtu_atk_melee_wait(st, effects, fixes);
}

static fix melee(fix fire_wait, int effects, int fixes)
{
	fix st = dtu_atk_state_time(1, fire_wait, MODEL_ATTACK, effects, fixes);

	return dtu_atk_melee_wait(st, effects, fixes);
}

// The time from one shot to the next, with the item bits of the bow in
// the bow slot (pc_spell1 of its line, -1 for a bow without the keyword)
static fix shot(fix fire_wait, int effects, int bow_pc_spell1, int fixes)
{
	if (dtu_atk_item_bits(bow_pc_spell1, fixes) & (1 << 9))
		effects |= DTU_ATK_HASTE;
	return dtu_atk_missile_wait(fire_wait, effects, fixes, fixes);
}

int main(void)
{
	int fixes;

	// The executable (-origcombat)
	fixes = 0;
	check("1.3 long sword", melee(FW_LONG_SWORD, 0, fixes), SEC(1.0));
	check("1.3 fast weapon (dagger)", melee(FW_DAGGER, 0, fixes), SEC(1.0));
	check("1.3 Short Sword of Quickness", melee_item(FW_QUICKNESS, 0, 21, fixes), SEC(1.0));
	check("1.3 slow weapon (warhammer)", melee(FW_WARHAMMER, 0, fixes), SEC(1.5));
	check("1.3 long sword, Haste", melee(FW_LONG_SWORD, DTU_ATK_HASTE, fixes), SEC(1.5));
	check("1.3 long sword, Haste, swing", dtu_atk_state_time(1, FW_LONG_SWORD, MODEL_ATTACK, DTU_ATK_HASTE, fixes), SEC(1.0));
	check("1.3 long sword, Slow", melee(FW_LONG_SWORD, DTU_ATK_SLOW, fixes), SEC(2.0));
	check("1.3 short bow", shot(FW_ARROW, 0, -1, fixes), 0);
	check("1.3 Short Bow of Speed", shot(FW_ARROW, 0, 21, fixes), 0);
	check("1.3 bow, Haste", shot(FW_ARROW, DTU_ATK_HASTE, -1, fixes), 0);
	check("1.3 Short Bow of Speed, item bits", dtu_atk_item_bits(21, fixes), 0);
	check("1.3 arrow, attack state", dtu_atk_state_time(0, FW_ARROW, MODEL_ATTACK, 0, fixes), SEC(1.0));
	check("1.3 arrow, attack state, Slow", dtu_atk_state_time(0, FW_ARROW, MODEL_ATTACK, DTU_ATK_SLOW, fixes), SEC(2.0));
	check("1.3 state time 0.5 animates", dtu_atk_state_animates(SEC(0.5), 1, fixes), 0);
	check("1.3 state time 1.0 animates", dtu_atk_state_animates(SEC(1.0), 1, fixes), 1);

	// The port with its corrections (the default)
	fixes = 1;
	check("long sword", melee(FW_LONG_SWORD, 0, fixes), SEC(1.0));
	check("fast weapon (dagger)", melee(FW_DAGGER, 0, fixes), SEC(1.0));
	check("short sword +2", melee_item(SEC(0.6), 0, -1, fixes), SEC(1.0));
	check("Short Sword of Quickness", melee_item(FW_QUICKNESS, 0, 21, fixes), SEC(0.5));
	check("slow weapon (warhammer)", melee(FW_WARHAMMER, 0, fixes), SEC(1.5));
	check("long sword, Haste", melee(FW_LONG_SWORD, DTU_ATK_HASTE, fixes), SEC(0.5));
	check("warhammer, Haste", melee(FW_WARHAMMER, DTU_ATK_HASTE, fixes), SEC(0.75));
	check("long sword, Slow", melee(FW_LONG_SWORD, DTU_ATK_SLOW, fixes), SEC(2.0));
	check("long sword, Slow and Haste", melee(FW_LONG_SWORD, DTU_ATK_SLOW | DTU_ATK_HASTE, fixes), SEC(1.0));
	check("short bow", shot(FW_ARROW, 0, -1, fixes), SEC(2.0));
	check("Short Bow of Speed", shot(FW_ARROW, 0, 21, fixes), SEC(1.0));
	check("short bow, Haste", shot(FW_ARROW, DTU_ATK_HASTE, -1, fixes), SEC(1.0));
	check("Short Bow of Speed, Haste", shot(FW_ARROW, DTU_ATK_HASTE, 21, fixes), SEC(1.0));
	check("short bow, Slow", shot(FW_ARROW, DTU_ATK_SLOW, -1, fixes), SEC(4.0));
	check("crossbow", shot(FW_BOLT, 0, -1, fixes), SEC(3.0));
	check("dart", shot(FW_DART, 0, -1, fixes), SEC(0.5));
	check("Short Bow of Speed, item bits", dtu_atk_item_bits(21, fixes), 1 << 9);
	check("bow with another spell, item bits", dtu_atk_item_bits(2, fixes), 0);
	check("arrow, attack state", dtu_atk_state_time(0, FW_ARROW, MODEL_ATTACK, 0, fixes), SEC(1.0));
	check("arrow, attack state, Haste", dtu_atk_state_time(0, FW_ARROW, MODEL_ATTACK, DTU_ATK_HASTE, fixes), SEC(1.0));
	// The fire frame of the attack state: frames 44 to 59, fire frame 56
	// ($PLAYER of BANE.BIN): 11.5 of 16 frames; fire frame 45 (HMC.BIN)
	check("fire lead, frame 56", dtu_atk_fire_lead(SEC(1.0), 44, 60, 56), SEC(11.5 / 16));
	check("fire lead, frame 45", dtu_atk_fire_lead(SEC(1.0), 44, 60, 45), SEC(0.5 / 16));
	check("fire lead, frame 56, Slow", dtu_atk_fire_lead(SEC(2.0), 44, 60, 56), SEC(23.0 / 16));
	check("fire lead, first frame", dtu_atk_fire_lead(SEC(1.0), 44, 60, 44), 0);
	// The halved swing must animate, or the swing never ends
	check("state time 0.5 of an attack animates", dtu_atk_state_animates(SEC(0.5), 1, fixes), 1);
	check("state time 0.5 of another state animates", dtu_atk_state_animates(SEC(0.5), 0, fixes), 0);
	check("state time 0 animates", dtu_atk_state_animates(0, 1, fixes), 0);

	if (failures) {
		printf("%d failures\n", failures);
		return 1;
	}
	printf("test_dtuatk: ok\n");
	return 0;
}
