/*
 * ASCENT: new file. Test of the table fixup of 1.3 (dtutblfix.c, 0x1A344)
 * without the game data: the lines have the names of the mission tables
 * and a few keys of the real lines, then the test checks a sample of each
 * group of writes (names, class masks and levels, the cleric copies, spell
 * fields, item values, weapon fields, misc fields, the two magic loops)
 * and the guard (a table with another line at a number stays as it is).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtutblfix.h"

#define NAME_SIZE	40
#define MAXN		128

typedef struct { char name[NAME_SIZE]; char *text; int value; int copied_from; } line;

static line Misc[MAXN], Armor[MAXN], Weapon[MAXN], Spell[MAXN];
static int N_misc = 128, N_armor = 53, N_weapon = 89, N_spell = 56;
static int Fails = 0;

static line *tab(int kind, int *n)
{
	switch (kind) {
		case DTU_TK_MISC:		*n = N_misc; return Misc;
		case DTU_TK_ARMOR:	*n = N_armor; return Armor;
		case DTU_TK_WEAPON:	*n = N_weapon; return Weapon;
		case DTU_TK_SPELL:	*n = N_spell; return Spell;
	}
	*n = 0;
	return NULL;
}

static line *get(int kind, int id)
{
	int n;
	line *t = tab(kind, &n);

	return (t && id >= 0 && id < n) ? &t[id] : NULL;
}

static int io_count(int kind) { int n; tab(kind, &n); return n; }
static char *io_text(int kind, int id) { line *l = get(kind, id); return l ? l->text : NULL; }
static void io_set_text(int kind, int id, char *t) { line *l = get(kind, id); if (!l) { free(t); return; } free(l->text); l->text = t; }
static char *io_name(int kind, int id) { line *l = get(kind, id); return l ? l->name : NULL; }
static void io_copy(int kind, int dst, int src) { line *d = get(kind, dst), *s = get(kind, src); if (d && s) { d->value = s->value; d->copied_from = src; } }
static void io_sync(int kind, int id)
{
	line *l = get(kind, id);
	char v[32];

	if (l && dtu_tblfix_get(l->text, "value", v, sizeof(v)))
		l->value = atoi(v);
}

static const dtu_tblfix_io Io = { io_count, io_text, io_set_text, io_name, NAME_SIZE, io_copy, io_sync, NULL };

// A line: the name and the keys. Spell lines start with their number.
static void mk(int kind, int id, const char *name, const char *keys)
{
	line *l = get(kind, id);
	char buf[600];

	if (kind == DTU_TK_SPELL)
		snprintf(buf, sizeof(buf), "%d name=\"%s\" %s", id, name, keys);
	else if (kind == DTU_TK_WEAPON)
		snprintf(buf, sizeof(buf), "weapon_name=\"%s\" %s", name, keys);
	else
		snprintf(buf, sizeof(buf), "name=\"%s\" %s", name, keys);
	free(l->text);
	l->text = strdup(buf);
	snprintf(l->name, NAME_SIZE, "%s", name);
	l->value = 0;
	l->copied_from = -1;
	io_sync(kind, id);
}

// The names of the mission tables (YPINN.BIN) at the numbers that the
// fixup changes or reads. The other lines get "Item N".
static const struct { int kind, id; const char *name, *keys; } Lines[] = {
	{ 'M', 0, "Lantern", "icon=78 all_use" },
	{ 'M', 2, "Gem of Curing", "value=1500 all_use stackable" },
	{ 'M', 5, "Spider Amulet", "icon=149 plot_item=1" },
	{ 'M', 10, "Lockpicks", "thief_use value=10" },
	{ 'M', 11, "Journeyman Lockpicks", "thief_use" },
	{ 'M', 12, "Master Lockpicks", "thief_use" },
	{ 'M', 13, "Apprentice Lockpicks", "thief_use" },
	{ 'M', 18, "Spider Piece", "all_use" }, { 'M', 19, "Spider Piece", "all_use" },
	{ 'M', 20, "Spider Piece", "all_use" }, { 'M', 22, "Spider Piece", "all_use" },
	{ 'M', 23, "Spider Piece", "all_use" }, { 'M', 24, "Spider Piece", "all_use" },
	{ 'M', 25, "Assembled Spider", "all_use" }, { 'M', 26, "Spider Piece", "all_use" },
	{ 'M', 72, "Potion of Agility", "" }, { 'M', 73, "Potion of Anti-Magic", "" },
	{ 'M', 74, "Potion of Dragon's Blood", "" }, { 'M', 75, "Potion of Extra-Healing", "" },
	{ 'M', 76, "Potion of Fire Resistance", "" }, { 'M', 77, "Potion of Flying", "" },
	{ 'M', 78, "Potion of Freedom", "" }, { 'M', 79, "Potion of Healing", "" },
	{ 'M', 80, "Potion of Hill Giant Strength", "all_use value=300" },
	{ 'M', 82, "Potion of Infravision", "" },
	{ 'M', 83, "Potion of Agility", "value=300" },
	{ 'M', 84, "Potion of Invulnerability", "" }, { 'M', 86, "Potion of Luck", "" },
	{ 'M', 87, "Potion of Speed", "" }, { 'M', 88, "Potion of Toughening", "" },
	{ 'M', 106, "Cursed Scroll", "icon=132" },
	{ 'M', 127, "thief scroll  ", "icon=141" },
	{ 'S', 0, "Abjure", "cleric_use level=4 value=1200 cast_time=60" },
	{ 'S', 1, "Aid", "" },
	{ 'S', 4, "Burning Hands", "" },
	{ 'S', 5, "Chill Touch", "mage_use range=6 type=TARGET radius=0 cast_with_hands cast_sound=2106 stackable" },
	{ 'S', 6, "Color Spray", "type=AREA weapon_vclip=180 cast_sound=2107" },
	{ 'S', 7, "Confusion", "" },
	{ 'S', 8, "Continual Light (Cleric)", "" }, { 'S', 9, "Continual Light (Mage)", "" },
	{ 'S', 12, "Detect Magic", "level=1 mage_use type=TARGET radius=16 stackable" },
	{ 'S', 13, "Dispel Magic", "level=3 range=360 type=TARGET mage_use weapon_vclip=88 cast_in_front stackable always" },
	{ 'S', 14, "Fear", "" }, { 'S', 15, "Feather Fall", "" },
	{ 'S', 16, "Fireball", "level=3 mage_use range=240 type=TARGET radius=10 weapon_vclip=14 fire_damage_dice=0 #fire_damage_dice=1 fire_damage_level=6 cast_with_hands cast_sound=2115 stackable" },
	{ 'S', 17, "Flaming Sphere", "level=2 mage_use range=60 type=TARGET radius=0 cast_with_hands stackable weapon_vclip=39 value=300" },
	{ 'S', 21, "Haste", "" }, { 'S', 22, "Hold Person", "level=3 mage_use" },
	{ 'S', 23, "Hold Undead", "" }, { 'S', 24, "Improved Invisibility", "" },
	{ 'S', 26, "Invisibility", "" },
	{ 'S', 28, "Knock", "level=2 mage_use range=360 self_cast_ok type=TARGET radius=16 weapon_vclip=84 cast_in_front stackable cast_with_hands" },
	{ 'S', 30, "Light (Mage)", "level=1 mage_use type=AREA weapon_vclip=103 cast_in_front stackable" },
	{ 'S', 31, "Light (Cleric)", "" },
	{ 'S', 32, "Lightning Bolt", "" },
	{ 'S', 33, "Magic Missile", "damage_dice=4 #damage_dice=1" },
	{ 'S', 34, "Magical Stone", "" },
	{ 'S', 36, "Melf's Acid Arrows", "" }, { 'S', 37, "Melf's Minute Meteors", "" },
	{ 'S', 38, "Prayer", "value=600" },
	{ 'S', 39, "Protection From Evil", "level=1 mage_use" },
	{ 'S', 40, "Protection From Fire", "level=3 cleric_use" },
	{ 'S', 42, "Resist Cold", "level=2 cleric_use cast_sound=2126" },
	{ 'S', 43, "Resist Fire", "value=300" },
	{ 'S', 44, "Scare", "" }, { 'S', 46, "Sleep", "" }, { 'S', 47, "Slow", "" },
	{ 'S', 48, "Spider Climb", "icon=127" },
	{ 'S', 51, "Burning Hands", "level=1 mage_use level=2" },
	{ 'S', 52, "Hold Person", "level=2 cleric_use" },
	{ 'S', 53, "Protection From Evil", "level=1 cleric_use" },
	{ 'S', 54, "Detect Magic", "level=1 cleric_use" },
	{ 'S', 55, "Dispel Magic", "level=3 cleric_use" },
	{ 'W', 13, "Wand of Lightning Bolts", "value=5000" },
	{ 'W', 14, "Black Lightning", "value=5000" },
	{ 'W', 15, "Bomb", "damage_dice=6 #damage_dice=2" },
	{ 'W', 24, "Throwing Hammer", "weapon_char=t" },
	{ 'W', 25, "SLow Bomb", "" }, { 'W', 26, "fast Bomb", "" },
	{ 'W', 28, "Warhammer", "damage_dice=10 #damage_dice=3 value=2 weapon_char=w" },
	{ 'W', 29, "Club", "damage_dice=4 damage_bonus=1" },
	{ 'W', 32, "Long Sword +1", "value=1000" },
	{ 'W', 33, "Long Sword +2", "value=2000 damage_dice=8 to_hit_bonus=2 damage_bonus=2" },
	{ 'W', 40, "Warhammer +1", "" }, { 'W', 41, "Club+1", "damage_dice=4 damage_bonus=2" },
	{ 'W', 45, "Dagger of Impaling", "" }, { 'W', 53, "Throwing Hammer +1", "weapon_char=h" },
	{ 'W', 55, "Wand of Magic Missiles", "" }, { 'W', 56, "Wand of Confusion", "" },
	{ 'W', 57, "Wand of Fear", "" }, { 'W', 58, "Wand of Fireballs", "" },
	{ 'W', 59, "Wand of Lightning Bolts", "attack_type=MELEE PC_spell=1" },
	{ 'W', 60, "Wand of Magic Detection", "" }, { 'W', 61, "Wand of Paralyzation", "" },
	{ 'W', 62, "Rod of Lightning", "" }, { 'W', 69, "Short Sword of Quickness", "" },
	{ 'W', 70, "Mace of Disruption", "plot_item=0" }, { 'W', 71, "Flame Tongue", "" },
	{ 'W', 73, "Scimitar", "thief_use" },
	{ 'A', 16, "Armoring", "" }, { 'A', 19, "Ring of Flying", "plot_item=1" },
	{ 'A', 27, "Ring of Warmth", "all_use value=1000" },
	{ 'A', 34, "Winged Boots", "" }, { 'A', 35, "Boots of Feather Falling", "" },
	{ 'A', 36, "Boots of Levitation", "" }, { 'A', 37, "Boots of Speed", "" },
	{ 'A', 38, "Girdle of Hill Giant Strength", "" }, { 'A', 42, "Bracers of Defense AC2", "" },
	{ 'A', 43, "Cloak of Elvenkind", "" }, { 'A', 44, "Chameleon Cloak", "" },
	{ 'A', 45, "Cloak of Elvenkind", "value=5000" }, { 'A', 46, "Elven Cloak", "" },
	{ 'A', 47, "Cloak of Protection +1", "" }, { 'A', 48, "Cloak of Protection +2", "" },
	{ 'A', 49, "Cloak of Protection +3", "" }, { 'A', 50, "Gloves of Dexterity", "" },
	{ 'A', 52, "Gauntlets of Ogre Power", "" },
};

static void setup(void)
{
	static const int kinds[4] = { 'M', 'A', 'W', 'S' };
	int k, i;
	char name[32];

	for (k = 0; k < 4; k++)
		for (i = 0; i < io_count(kinds[k]); i++) {
			snprintf(name, sizeof(name), "Item %d", i);
			mk(kinds[k], i, name, "");
		}
	for (i = 0; i < (int) (sizeof(Lines) / sizeof(Lines[0])); i++)
		mk(Lines[i].kind, Lines[i].id, Lines[i].name, Lines[i].keys);
}

static void expect_key(int kind, int id, const char *key, const char *want)
{
	char v[64];
	int have = dtu_tblfix_get(io_text(kind, id), key, v, sizeof(v));

	if (want ? (!have || strcmp(v, want)) : have) {
		printf("FAIL %c %d: %s is '%s', expected '%s'\n", kind, id, key, have ? v : "(none)", want ? want : "(none)");
		Fails++;
	}
}

static void expect_word(int kind, int id, const char *word, int want)
{
	if (dtu_tblfix_has(io_text(kind, id), word) != want) {
		printf("FAIL %c %d: word %s is %d, expected %d (%s)\n", kind, id, word, !want, want, io_text(kind, id));
		Fails++;
	}
}

static void expect_int(const char *what, int have, int want)
{
	if (have != want) {
		printf("FAIL %s: %d (0x%X), expected %d (0x%X)\n", what, have, have, want, want);
		Fails++;
	}
}

static void expect_name(int kind, int id, const char *want)
{
	char v[64];

	if (strcmp(io_name(kind, id), want)) {
		printf("FAIL %c %d: name '%s', expected '%s'\n", kind, id, io_name(kind, id), want);
		Fails++;
	}
	if (!(dtu_tblfix_get(io_text(kind, id), "name", v, sizeof(v)) || dtu_tblfix_get(io_text(kind, id), "weapon_name", v, sizeof(v))) || strcmp(v, want)) {
		printf("FAIL %c %d: the text has the name '%s', expected '%s'\n", kind, id, v, want);
		Fails++;
	}
}

static void test_text(void)
{
	char *t;

	t = dtu_tblfix_set("a=1 name=\"x y\" b=2 a=3", "a", "5");
	expect_int("set replaces the first a=", strcmp(t, "a=5 name=\"x y\" b=2 ") == 0, 1);
	free(t);
	t = dtu_tblfix_set("damage_dice=3 #damage_dice=2", "damage_dice", NULL);
	expect_int("remove does not touch #damage_dice", strstr(t, "#damage_dice=2") != NULL && !strstr(t, " damage_dice"), 1);
	free(t);
	expect_int("set to the same value gives NULL", dtu_tblfix_set("a=1", "a", "1") == NULL, 1);
	expect_int("flags cast_in_front", dtu_tblfix_spell_flags("type=AREA stackable cast_in_front"), 0x503);
	expect_int("flags cast_with_scroll then hands", dtu_tblfix_spell_flags("cast_with_scroll cast_with_hands"), 0);
	t = dtu_tblfix_set_spell_flags("type=TARGET self_cast_ok cast_in_front stackable cast_with_hands", 0x803);
	expect_int("set flags 0x803", dtu_tblfix_spell_flags(t), 0x803);
	free(t);
	expect_int("class all_use", dtu_tblfix_class("all_use"), 0xFFFF);
	t = dtu_tblfix_set_class("all_use", 0x27);
	expect_int("set class 0x27", dtu_tblfix_class(t), 0x27);
	free(t);
}

// test_dtutblfix IN OUT: the lines of IN (tools/check_tblfix.py writes
// them: letter, number, name and text, separated by tabs) after the fixup
// go to OUT in the same form. The tool compares them with tools/tblfix.py.
static int file_mode(const char *in, const char *out)
{
	static char row[4096];
	FILE *f = fopen(in, "r"), *g;
	int n, k, i;

	if (!f)
		return 2;
	N_misc = N_armor = N_weapon = N_spell = 0;
	while (fgets(row, sizeof(row), f)) {
		char *a = strtok(row, "\t"), *b = strtok(NULL, "\t"), *c = strtok(NULL, "\t"), *d = strtok(NULL, "\r\n");
		line *l;
		int id, *cnt;

		if (!a || !b || !c)
			continue;
		id = atoi(b);
		cnt = a[0] == 'M' ? &N_misc : a[0] == 'A' ? &N_armor : a[0] == 'W' ? &N_weapon : &N_spell;
		if (id >= MAXN)
			continue;
		if (id + 1 > *cnt)
			*cnt = id + 1;
		l = get(a[0], id);
		free(l->text);
		l->text = strdup(d ? d : "");
		snprintf(l->name, NAME_SIZE, "%s", strcmp(c, "-") ? c : "");
		io_sync(a[0], id);
	}
	fclose(f);
	n = dtu_tblfix_apply(&Io);
	g = fopen(out, "w");
	if (!g)
		return 2;
	fprintf(g, "%d\n", n);
	for (k = 0; k < 4; k++) {
		int kind = "MAWS"[k];

		for (i = 0; i < io_count(kind); i++)
			if (get(kind, i)->text)
				fprintf(g, "%c\t%d\t%s\t%s\n", kind, i, get(kind, i)->name, get(kind, i)->text);
	}
	fclose(g);
	return 0;
}

int main(int argc, char **argv)
{
	int n;

	if (argc == 3)
		return file_mode(argv[1], argv[2]);

	test_text();

	setup();
	n = dtu_tblfix_apply(&Io);
	if (n <= 0) {
		printf("FAIL the fixup was not applied (%d)\n", n);
		return 1;
	}

	// Names
	expect_name('M', 83, "Potion of Invisibility");
	expect_name('S', 17, "Produce Fire");
	expect_name('S', 42, "Cure Blindness or Deafness");
	expect_name('S', 51, "Find Traps");
	expect_name('W', 73, "Dwarven Scimitar");
	expect_name('A', 27, "Ring of Wizardry");
	expect_name('A', 45, "Cloak of Lathandar");
	expect_name('A', 50, "Gauntlets of Dexterity");

	// Class masks and levels
	expect_int("class of spell 40", dtu_tblfix_class(io_text('S', 40)), 0x27);
	expect_int("class of spell 39", dtu_tblfix_class(io_text('S', 39)), 0x26);
	expect_int("class of spell 17", dtu_tblfix_class(io_text('S', 17)), 1);
	expect_key('S', 17, "level", "4");
	expect_key('S', 51, "level", "2");
	expect_int("class of armor 27", dtu_tblfix_class(io_text('A', 27)), 2);

	// The cleric copies: the text of the source with the own name and number
	expect_name('S', 54, "Detect Magic");
	expect_int("class of spell 54", dtu_tblfix_class(io_text('S', 54)), 1);
	expect_int("class of spell 12", dtu_tblfix_class(io_text('S', 12)), 2);
	expect_key('S', 54, "radius", "16");
	expect_int("spell 54 has its number", strncmp(io_text('S', 54), "54 ", 3), 0);
	expect_key('S', 55, "range", "720");
	expect_key('S', 55, "weapon_vclip", "176");
	expect_int("class of spell 53", dtu_tblfix_class(io_text('S', 53)), 0x25);
	expect_key('S', 52, "level", "2");
	expect_key('S', 52, "radius", "40");

	// Spell fields
	expect_int("flags of Chill Touch", dtu_tblfix_spell_flags(io_text('S', 5)), 0x102);
	expect_word('S', 5, "self_cast_ok", 1);
	expect_key('S', 5, "radius", "3");
	expect_int("flags of Fireball", dtu_tblfix_spell_flags(io_text('S', 16)), 0x803);
	expect_word('S', 16, "hit_objects", 1);
	expect_key('S', 16, "radius", "40");
	expect_key('S', 16, "fire_damage_dice", "6");
	expect_key('S', 16, "fire_damage_level", "0");
	expect_int("flags of Knock", dtu_tblfix_spell_flags(io_text('S', 28)), 0x2);
	expect_word('S', 28, "self_cast_ok", 0);
	expect_word('S', 28, "cast_in_front", 0);
	expect_key('S', 28, "weapon_vclip", "84");
	expect_key('S', 14, "weapon_vclip", "103");		// Light (Mage) before its change
	expect_key('S', 30, "weapon_vclip", "39");
	expect_int("flags of Produce Fire (from Fireball)", dtu_tblfix_spell_flags(io_text('S', 17)), 0x803);
	expect_key('S', 17, "cast_sound", "2115");
	expect_key('S', 17, "value", "1200");
	expect_key('S', 17, "damage_bonus", "0");
	expect_key('S', 13, "cast_sound", "2126");
	expect_key('S', 33, "damage_level", "1");
	expect_key('S', 17, "cast_time", "7");
	expect_key('S', 0, "cast_time", "7");

	// Item values
	expect_int("value of misc 2", get('M', 2)->value, 5000);
	expect_int("value of armor 27", get('A', 27)->value, 32000);
	expect_int("value of armor 45", get('A', 45)->value, 17500);

	// Weapon fields
	expect_key('W', 28, "damage_dice", "4");
	expect_key('W', 28, "#damage_dice", "1");
	expect_key('W', 28, "damage_bonus", "1");
	expect_key('W', 28, "to_hit_bonus", "0");
	expect_key('W', 28, "weapon_char", "t");
	expect_key('W', 40, "weapon_char", "h");
	expect_key('W', 15, "fire_damage_dice", "6");
	expect_key('W', 15, "damage_dice", "0");
	expect_key('W', 59, "PC_spell", "32");
	expect_key('W', 59, "attack_type", "PROJECTILE");
	expect_name('W', 13, "Drowslayer");
	expect_int("weapon 13 is a copy of 33", get('W', 13)->copied_from, 33);
	expect_key('W', 13, "damage_dice", "8");
	expect_int("value of weapon 13", get('W', 13)->value, 9000);
	expect_name('W', 14, "Longsword +3");
	expect_int("value of weapon 14", get('W', 14)->value, 3000);
	expect_key('W', 14, "to_hit_bonus", "3");

	// Misc fields
	expect_word('M', 10, "lockpick", 1);
	expect_word('M', 13, "lockpick", 1);
	expect_key('M', 106, "icon", "141");
	expect_key('M', 18, "uses", "1");
	expect_key('M', 2, "uses", "-1");
	expect_int("class of misc 80", dtu_tblfix_class(io_text('M', 80)), 4);
	expect_key('S', 48, "icon", "149");
	expect_key('S', 51, "icon", "78");

	// The magic loops: '+' in the name, plot_item
	expect_word('W', 41, "magic", 1);
	expect_word('W', 32, "magic", 1);
	expect_word('W', 70, "magic", 0);
	expect_word('W', 14, "magic", 1);		// copied from 33 after the loop
	expect_word('W', 28, "magic", 0);
	expect_word('A', 19, "magic", 1);
	expect_word('A', 27, "magic", 0);

	// The guard: another line at a number, nothing changes
	setup();
	mk('W', 28, "Long Bow", "damage_dice=10");
	n = dtu_tblfix_apply(&Io);
	expect_int("the fixup on another table", n, -1);
	expect_key('W', 28, "damage_dice", "10");
	expect_name('M', 83, "Potion of Agility");

	printf("%s: %d fixup rows, %d failures\n", Fails ? "FAILED" : "ok", dtu_tblfix_n_ops(), Fails);
	return Fails ? 1 : 0;
}
