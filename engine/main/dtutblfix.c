/*
 * ASCENT: new file. The table fixup of DTUGAME.EXE 1.3 (0x1A344): the list
 * of its writes and the code that applies them to the text of the table
 * records. See dtutblfix.h and docs/re/objects.md, "The table fixup".
 *
 * Fixups[] has the writes of 0x1A344 in the order of the executable. The
 * address is the first instruction of each group. The fields of the
 * records and their keywords (reader 0x209F3 weapons, 0x22BB1 armor,
 * 0x23780 misc, 0x242F6 spells):
 *
 *   Spell_info (127 bytes at 0x1B5676, damage_bonus at -2): +0x00 class
 *   mask, +0x02 icon, +0x04 cast_time, +0x06 range, +0x08 radius, +0x0A
 *   level_range, +0x0C level_radius, +0x0E cast_sound, +0x10 to +0x18
 *   monster_hit_sound, monster_miss_sound, monster_kill_sound,
 *   wall_hit_sound, armor_hit_sound, +0x1A cast_vclip, +0x2E to_hit_bonus,
 *   +0x30 the model of weapon_pof, +0x32 weapon_vclip, +0x36 flags (word),
 *   +0x38 render type (2 weapon_pof, 3 weapon_vclip), +0x39 level, +0x3A
 *   to +0x48 the five dice (sides, #, _level: damage, fire, cold, acid,
 *   elec), +0x5A player_light, +0x6E combustion, +0x70 value.
 *
 *   Weapon_info (217 bytes at 0x1E9D08): +0x0D damage_dice, +0x0E
 *   #damage_dice, +0x0F fire_damage_dice, +0x10 #fire_damage_dice, +0x12
 *   attack_type (0 projectile, 1 melee), +0x13 weapon_char, +0x20
 *   flash_sound, +0x2A wall_hit_sound, +0x34 value, +0x36 to_hit_bonus,
 *   +0x38 damage_bonus, +0x3A PC_spell, +0x3E flags (magic 0x10), +0x52
 *   plot_item.
 *
 *   Misc_info (68 bytes at 0x19BABC): +0x00 class mask, +0x02 icon, +0x04
 *   wear mask, +0x06 value, +0x10 uses. Armor_info (58 bytes at
 *   0x19FD24): +0x00 class mask, +0x06 value, +0x10 flags (magic 1), +0x24
 *   plot_item. The names are tables of 32 bytes.
 *
 * tools/tblfix.py reads Fixups[] and Guard[]. Keep one row on one line,
 * with the fields in this order and the strings without escapes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dtutblfix.h"

int Dtu_raw_tables = 0;

#define M	DTU_TK_MISC
#define A	DTU_TK_ARMOR
#define W	DTU_TK_WEAPON
#define S	DTU_TK_SPELL

enum {
	T_SET,				// key=val ("" removes the key)
	T_COPY,				// key= of line src (of the table val when val is a letter)
	T_NAME,				// the name: val
	T_CLASS,				// the class mask: val
	T_CLASS_OR,			// the class mask |= val
	T_WEAR_OR,			// the wear mask |= val
	T_FLAGS,				// the spell flags (+0x36, a word): val
	T_FLAGS_OR,			// the spell flags |= val
	T_FLAGS_AND,		// the spell flags &= val
	T_FLAGS_COPY,		// the spell flags of line src
	T_RENDER,			// the missile of line src: +0x30 and +0x38, and +0x32 when val is "1"
	T_COPYREC,			// the whole line src, not the name (memcpy 0xD7CD2)
	T_MULVAL,			// key= of line src times val
	T_CAST_TIMES,		// cast_time= of each spell line from the words at 0x13A8DB
	T_MAGIC_WEAPONS,	// each weapon line with plot_item or a '+' in the name gets magic
	T_MAGIC_ARMOR,		// each armor line with plot_item gets magic
};

//------------------------------------------------------------------------
// The writes of 0x1A344, in order. Addresses: DTUGAME.EXE 1.3, object 1
// at 0x10000.

static const dtu_tblfix_op Fixups[] = {
	// 0x1A46D: misc values and names
	{ T_SET, M, 2, -1, "uses", "-1" },
	{ T_SET, M, 2, -1, "value", "5000" },
	{ T_SET, M, 72, -1, "value", "900" },
	{ T_SET, M, 73, -1, "value", "2400" },
	{ T_SET, M, 74, -1, "value", "700" },
	{ T_SET, M, 75, -1, "value", "1000" },
	{ T_SET, M, 76, -1, "value", "400" },
	{ T_SET, M, 77, -1, "value", "750" },
	{ T_SET, M, 78, -1, "value", "900" },
	{ T_SET, M, 79, -1, "value", "400" },
	{ T_SET, M, 80, -1, "value", "900" },
	{ T_CLASS, M, 80, -1, "", "4" },
	{ T_SET, M, 82, -1, "value", "500" },
	{ T_SET, M, 83, -1, "value", "500" },
	{ T_NAME, M, 83, -1, "", "Potion of Invisibility" },
	{ T_SET, M, 84, -1, "value", "500" },
	{ T_CLASS, M, 84, -1, "", "4" },
	{ T_SET, M, 86, -1, "value", "1500" },
	{ T_SET, M, 87, -1, "value", "450" },
	{ T_SET, M, 88, -1, "value", "600" },
	// 0x1A528: the four lockpicks: thief class bit and lockpick wear bit
	{ T_CLASS_OR, M, 10, -1, "", "0x20" },
	{ T_WEAR_OR, M, 10, -1, "", "0x800" },
	{ T_CLASS_OR, M, 11, -1, "", "0x20" },
	{ T_WEAR_OR, M, 11, -1, "", "0x800" },
	{ T_CLASS_OR, M, 12, -1, "", "0x20" },
	{ T_WEAR_OR, M, 12, -1, "", "0x800" },
	{ T_CLASS_OR, M, 13, -1, "", "0x20" },
	{ T_WEAR_OR, M, 13, -1, "", "0x800" },
	{ T_COPY, M, 106, 127, "icon", "" },
	// 0x1A56C: the spider pieces: every class, one use
	{ T_CLASS_OR, M, 18, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 19, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 23, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 22, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 24, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 26, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 20, -1, "", "0x3F" },
	{ T_CLASS_OR, M, 25, -1, "", "0x3F" },
	{ T_SET, M, 18, -1, "uses", "1" },
	{ T_SET, M, 19, -1, "uses", "1" },
	{ T_SET, M, 23, -1, "uses", "1" },
	{ T_SET, M, 22, -1, "uses", "1" },
	{ T_SET, M, 24, -1, "uses", "1" },
	{ T_SET, M, 26, -1, "uses", "1" },
	{ T_SET, M, 20, -1, "uses", "1" },
	{ T_SET, M, 25, -1, "uses", "1" },
	// 0x1A5EB: Aid
	{ T_SET, S, 1, -1, "range", "0" },
	{ T_SET, S, 1, -1, "radius", "0" },
	{ T_SET, S, 1, -1, "level_range", "0" },
	{ T_SET, S, 1, -1, "level_radius", "0" },
	{ T_FLAGS, S, 1, -1, "", "0x102" },
	// 0x1A615: Abjure, Burning Hands, Color Spray, Confusion
	{ T_SET, S, 0, -1, "cast_sound", "2129" },
	{ T_SET, S, 0, -1, "damage_dice", "8" },
	{ T_SET, S, 0, -1, "#damage_dice", "2" },
	{ T_SET, S, 0, -1, "damage_level", "3" },
	{ T_SET, S, 4, -1, "fire_damage_dice", "3" },
	{ T_SET, S, 4, -1, "#fire_damage_dice", "1" },
	{ T_SET, S, 4, -1, "fire_damage_level", "2" },
	{ T_SET, S, 6, -1, "cast_sound", "2104" },
	{ T_FLAGS, S, 7, -1, "", "0x2" },
	// 0x1A65E: Chill Touch: cast on the caster (+0x37 bit 0), not AREA
	{ T_FLAGS_OR, S, 5, -1, "", "0x100" },
	{ T_FLAGS_AND, S, 5, -1, "", "0xFFFE" },
	{ T_SET, S, 5, -1, "cast_sound", "2131" },
	{ T_SET, S, 5, -1, "radius", "3" },
	// 0x1A67D: Detect Magic: a mage spell, copied to 54 as a cleric spell
	{ T_CLASS, S, 12, -1, "", "2" },
	{ T_COPYREC, S, 54, 12, "", "" },
	{ T_CLASS, S, 54, -1, "", "1" },
	// 0x1A6A3: Fear, Haste (the missile of Light (Mage) before its change)
	{ T_SET, S, 14, -1, "range", "0" },
	{ T_SET, S, 14, -1, "radius", "90" },
	{ T_SET, S, 14, -1, "level_range", "0" },
	{ T_SET, S, 14, -1, "level_radius", "0" },
	{ T_FLAGS, S, 14, -1, "", "0x103" },
	{ T_RENDER, S, 14, 30, "", "1" },
	{ T_SET, S, 21, -1, "range", "0" },
	{ T_SET, S, 21, -1, "radius", "80" },
	{ T_SET, S, 21, -1, "level_range", "0" },
	{ T_SET, S, 21, -1, "level_radius", "0" },
	{ T_FLAGS, S, 21, -1, "", "0x103" },
	{ T_RENDER, S, 21, 30, "", "1" },
	// 0x1A73F: Knock (+0x30 and +0x38 only), the Light spells
	{ T_SET, S, 28, -1, "range", "360" },
	{ T_SET, S, 28, -1, "radius", "0" },
	{ T_SET, S, 28, -1, "level_range", "0" },
	{ T_SET, S, 28, -1, "level_radius", "20" },
	{ T_RENDER, S, 28, 17, "", "0" },
	{ T_FLAGS, S, 28, -1, "", "0x2" },
	{ T_SET, S, 30, -1, "range", "360" },
	{ T_SET, S, 30, -1, "radius", "3" },
	{ T_SET, S, 30, -1, "level_range", "0" },
	{ T_SET, S, 30, -1, "level_radius", "0" },
	{ T_RENDER, S, 30, 17, "", "1" },
	{ T_FLAGS, S, 30, -1, "", "0x3" },
	{ T_SET, S, 30, -1, "player_light", "20" },
	{ T_CLASS, S, 30, -1, "", "2" },
	{ T_SET, S, 31, -1, "range", "720" },
	{ T_SET, S, 31, -1, "radius", "3" },
	{ T_SET, S, 31, -1, "level_range", "0" },
	{ T_SET, S, 31, -1, "level_radius", "0" },
	{ T_RENDER, S, 31, 17, "", "1" },
	{ T_FLAGS, S, 31, -1, "", "0x3" },
	{ T_SET, S, 31, -1, "player_light", "20" },
	{ T_CLASS, S, 31, -1, "", "1" },
	{ T_SET, S, 9, -1, "range", "360" },
	{ T_SET, S, 9, -1, "radius", "3" },
	{ T_SET, S, 9, -1, "level_range", "0" },
	{ T_SET, S, 9, -1, "level_radius", "0" },
	{ T_RENDER, S, 9, 17, "", "1" },
	{ T_FLAGS, S, 9, -1, "", "0x3" },
	{ T_SET, S, 9, -1, "player_light", "60" },
	{ T_CLASS, S, 9, -1, "", "2" },
	{ T_SET, S, 8, -1, "range", "720" },
	{ T_SET, S, 8, -1, "radius", "3" },
	{ T_SET, S, 8, -1, "level_range", "0" },
	{ T_SET, S, 8, -1, "level_radius", "0" },
	{ T_RENDER, S, 8, 17, "", "1" },
	{ T_FLAGS, S, 8, -1, "", "0x3" },
	{ T_SET, S, 8, -1, "player_light", "60" },
	{ T_CLASS, S, 8, -1, "", "1" },
	// 0x1A8ED: Dispel Magic, copied to 55 as a cleric spell
	{ T_SET, S, 13, -1, "range", "720" },
	{ T_SET, S, 13, -1, "radius", "60" },
	{ T_SET, S, 13, -1, "level_range", "0" },
	{ T_SET, S, 13, -1, "level_radius", "0" },
	{ T_RENDER, S, 13, 6, "", "0" },
	{ T_SET, S, 13, -1, "weapon_vclip", "176" },
	{ T_FLAGS, S, 13, -1, "", "0x803" },
	{ T_COPY, S, 13, 42, "cast_sound", "" },
	{ T_SET, S, 13, -1, "player_light", "0" },
	{ T_CLASS, S, 13, -1, "", "2" },
	{ T_COPYREC, S, 55, 13, "", "" },
	{ T_CLASS, S, 55, -1, "", "1" },
	// 0x1A972: Feather Fall, Fireball
	{ T_SET, S, 15, -1, "range", "60" },
	{ T_SET, S, 15, -1, "radius", "20" },
	{ T_SET, S, 15, -1, "level_range", "6" },
	{ T_SET, S, 15, -1, "level_radius", "0" },
	{ T_FLAGS, S, 15, -1, "", "0x103" },
	{ T_SET, S, 16, -1, "range", "60" },
	{ T_SET, S, 16, -1, "radius", "40" },
	{ T_SET, S, 16, -1, "level_range", "60" },
	{ T_SET, S, 16, -1, "level_radius", "0" },
	{ T_FLAGS, S, 16, -1, "", "0x803" },
	{ T_SET, S, 16, -1, "fire_damage_dice", "6" },
	{ T_SET, S, 16, -1, "#fire_damage_dice", "1" },
	{ T_SET, S, 16, -1, "fire_damage_level", "0" },
	{ T_SET, S, 16, -1, "damage_dice", "0" },
	{ T_SET, S, 16, -1, "#damage_dice", "0" },
	{ T_SET, S, 16, -1, "damage_level", "0" },
	// 0x1A9F5: Hold Person, copied to 52 as a cleric spell of level 2
	{ T_SET, S, 22, -1, "range", "720" },
	{ T_SET, S, 22, -1, "radius", "40" },
	{ T_SET, S, 22, -1, "level_range", "0" },
	{ T_SET, S, 22, -1, "level_radius", "0" },
	{ T_FLAGS, S, 22, -1, "", "0x3" },
	{ T_CLASS, S, 22, -1, "", "2" },
	{ T_COPYREC, S, 52, 22, "", "" },
	{ T_SET, S, 52, -1, "level", "2" },
	{ T_CLASS, S, 52, -1, "", "1" },
	// 0x1AA4F: Hold Undead, Invisibility, Improved Invisibility
	{ T_SET, S, 23, -1, "range", "120" },
	{ T_SET, S, 23, -1, "radius", "80" },
	{ T_SET, S, 23, -1, "level_range", "0" },
	{ T_SET, S, 23, -1, "level_radius", "0" },
	{ T_FLAGS, S, 23, -1, "", "0x3" },
	{ T_SET, S, 26, -1, "range", "3" },
	{ T_SET, S, 26, -1, "radius", "3" },
	{ T_SET, S, 26, -1, "level_range", "0" },
	{ T_SET, S, 26, -1, "level_radius", "0" },
	{ T_SET, S, 24, -1, "range", "3" },
	{ T_SET, S, 24, -1, "radius", "3" },
	{ T_SET, S, 24, -1, "level_range", "0" },
	{ T_SET, S, 24, -1, "level_radius", "0" },
	// 0x1AABA: Lightning Bolt, Magic Missile, Magical Stone
	{ T_SET, S, 32, -1, "range", "240" },
	{ T_SET, S, 32, -1, "radius", "5" },
	{ T_SET, S, 32, -1, "level_range", "60" },
	{ T_SET, S, 32, -1, "level_radius", "0" },
	{ T_FLAGS, S, 32, -1, "", "0x802" },
	{ T_SET, S, 32, -1, "elec_damage_dice", "6" },
	{ T_SET, S, 32, -1, "#elec_damage_dice", "1" },
	{ T_SET, S, 32, -1, "elec_damage_level", "0" },
	{ T_SET, S, 32, -1, "damage_dice", "0" },
	{ T_SET, S, 32, -1, "#damage_dice", "0" },
	{ T_SET, S, 32, -1, "damage_level", "0" },
	{ T_SET, S, 32, -1, "combustion", "-1" },
	{ T_SET, S, 33, -1, "range", "360" },
	{ T_SET, S, 33, -1, "radius", "3" },
	{ T_SET, S, 33, -1, "level_range", "60" },
	{ T_SET, S, 33, -1, "level_radius", "0" },
	{ T_SET, S, 33, -1, "damage_dice", "4" },
	{ T_SET, S, 33, -1, "#damage_dice", "1" },
	{ T_SET, S, 33, -1, "damage_level", "1" },
	{ T_SET, S, 34, -1, "range", "180" },
	{ T_SET, S, 34, -1, "radius", "0" },
	{ T_SET, S, 34, -1, "level_range", "0" },
	{ T_SET, S, 34, -1, "level_radius", "0" },
	{ T_SET, S, 34, -1, "damage_dice", "4" },
	{ T_SET, S, 34, -1, "#damage_dice", "1" },
	{ T_SET, S, 34, -1, "damage_level", "0" },
	// 0x1AB90: Melf's Acid Arrows, Melf's Minute Meteors
	{ T_SET, S, 36, -1, "range", "1080" },
	{ T_SET, S, 36, -1, "radius", "3" },
	{ T_SET, S, 36, -1, "level_range", "0" },
	{ T_SET, S, 36, -1, "level_radius", "0" },
	{ T_SET, S, 36, -1, "damage_dice", "0" },
	{ T_SET, S, 36, -1, "#damage_dice", "0" },
	{ T_SET, S, 36, -1, "damage_level", "0" },
	{ T_SET, S, 36, -1, "fire_damage_dice", "0" },
	{ T_SET, S, 36, -1, "#fire_damage_dice", "0" },
	{ T_SET, S, 36, -1, "fire_damage_level", "0" },
	{ T_SET, S, 36, -1, "acid_damage_dice", "4" },
	{ T_SET, S, 36, -1, "#acid_damage_dice", "2" },
	{ T_SET, S, 36, -1, "acid_damage_level", "0" },
	{ T_SET, S, 37, -1, "range", "420" },
	{ T_SET, S, 37, -1, "radius", "6" },
	{ T_SET, S, 37, -1, "level_range", "60" },
	{ T_SET, S, 37, -1, "level_radius", "0" },
	{ T_FLAGS, S, 37, -1, "", "0x802" },
	{ T_SET, S, 37, -1, "fire_damage_dice", "4" },
	{ T_SET, S, 37, -1, "#fire_damage_dice", "1" },
	{ T_SET, S, 37, -1, "fire_damage_level", "0" },
	{ T_SET, S, 37, -1, "damage_dice", "0" },
	{ T_SET, S, 37, -1, "#damage_dice", "0" },
	{ T_SET, S, 37, -1, "damage_level", "0" },
	// 0x1AC4A: Prayer, Protection From Evil (copied to 53), Protection From Fire
	{ T_SET, S, 38, -1, "range", "0" },
	{ T_SET, S, 38, -1, "radius", "120" },
	{ T_SET, S, 38, -1, "level_range", "0" },
	{ T_SET, S, 38, -1, "level_radius", "0" },
	{ T_FLAGS, S, 38, -1, "", "0x102" },
	{ T_SET, S, 39, -1, "range", "3" },
	{ T_SET, S, 39, -1, "radius", "3" },
	{ T_SET, S, 39, -1, "level_range", "0" },
	{ T_SET, S, 39, -1, "level_radius", "0" },
	{ T_FLAGS, S, 39, -1, "", "0x102" },
	{ T_CLASS, S, 39, -1, "", "0x26" },
	{ T_COPYREC, S, 53, 39, "", "" },
	{ T_CLASS, S, 53, -1, "", "0x25" },
	{ T_CLASS, S, 40, -1, "", "0x27" },
	// 0x1ACD4: Scare, Sleep, Slow, Spider Climb
	{ T_SET, S, 44, -1, "range", "180" },
	{ T_SET, S, 44, -1, "radius", "30" },
	{ T_SET, S, 44, -1, "level_range", "60" },
	{ T_SET, S, 44, -1, "level_radius", "0" },
	{ T_FLAGS, S, 44, -1, "", "0x3" },
	{ T_SET, S, 46, -1, "range", "180" },
	{ T_SET, S, 46, -1, "radius", "60" },
	{ T_SET, S, 46, -1, "level_range", "0" },
	{ T_SET, S, 46, -1, "level_radius", "0" },
	{ T_FLAGS, S, 46, -1, "", "0x3" },
	{ T_SET, S, 47, -1, "range", "540" },
	{ T_SET, S, 47, -1, "radius", "80" },
	{ T_SET, S, 47, -1, "level_range", "60" },
	{ T_SET, S, 47, -1, "level_radius", "0" },
	{ T_FLAGS, S, 47, -1, "", "0x3" },
	{ T_COPY, S, 48, 5, "icon", "M" },
	// 0x1AD67: Flaming Sphere becomes the cleric spell Produce Fire
	{ T_NAME, S, 17, -1, "", "Produce Fire" },
	{ T_SET, S, 17, -1, "damage_bonus", "0" },
	{ T_CLASS, S, 17, -1, "", "1" },
	{ T_SET, S, 17, -1, "range", "240" },
	{ T_SET, S, 17, -1, "radius", "24" },
	{ T_SET, S, 17, -1, "level_range", "0" },
	{ T_SET, S, 17, -1, "level_radius", "0" },
	{ T_COPY, S, 17, 16, "cast_sound", "" },
	{ T_COPY, S, 17, 16, "monster_hit_sound", "" },
	{ T_COPY, S, 17, 16, "monster_miss_sound", "" },
	{ T_COPY, S, 17, 16, "monster_kill_sound", "" },
	{ T_COPY, S, 17, 16, "wall_hit_sound", "" },
	{ T_COPY, S, 17, 16, "armor_hit_sound", "" },
	{ T_SET, S, 17, -1, "to_hit_bonus", "0" },
	{ T_FLAGS_COPY, S, 17, 16, "", "" },
	{ T_SET, S, 17, -1, "damage_dice", "0" },
	{ T_SET, S, 17, -1, "#damage_dice", "0" },
	{ T_SET, S, 17, -1, "damage_level", "0" },
	{ T_SET, S, 17, -1, "fire_damage_dice", "4" },
	{ T_SET, S, 17, -1, "#fire_damage_dice", "1" },
	{ T_SET, S, 17, -1, "fire_damage_level", "1" },
	{ T_SET, S, 17, -1, "combustion", "26" },
	{ T_COPY, S, 17, 0, "value", "" },
	{ T_SET, S, 17, -1, "level", "4" },
	// 0x1AE54: Resist Cold becomes Cure Blindness or Deafness
	{ T_NAME, S, 42, -1, "", "Cure Blindness or Deafness" },
	{ T_CLASS, S, 42, -1, "", "1" },
	{ T_FLAGS, S, 42, -1, "", "0x102" },
	{ T_SET, S, 42, -1, "level", "3" },
	{ T_COPY, S, 42, 38, "value", "" },
	{ T_COPY, S, 42, 13, "cast_vclip", "" },
	// 0x1AE9A: the second Burning Hands becomes Find Traps
	{ T_NAME, S, 51, -1, "", "Find Traps" },
	{ T_CLASS, S, 51, -1, "", "1" },
	{ T_COPY, S, 51, 0, "icon", "M" },
	{ T_FLAGS, S, 51, -1, "", "0x102" },
	{ T_SET, S, 51, -1, "level", "2" },
	{ T_COPY, S, 51, 43, "value", "" },
	// 0x1AEDC: the cast times, 0x1AF11: the magic flag of the weapons
	{ T_CAST_TIMES, 0, -1, -1, "", "" },
	{ T_MAGIC_WEAPONS, 0, -1, -1, "", "" },
	// 0x1AF4B: Bomb, SLow Bomb, fast Bomb: fire 1d6, sounds 2105
	{ T_SET, W, 15, -1, "damage_dice", "0" },
	{ T_SET, W, 15, -1, "#damage_dice", "0" },
	{ T_SET, W, 15, -1, "fire_damage_dice", "6" },
	{ T_SET, W, 15, -1, "#fire_damage_dice", "1" },
	{ T_SET, W, 15, -1, "flash_sound", "2105" },
	{ T_SET, W, 15, -1, "wall_hit_sound", "2105" },
	{ T_SET, W, 25, -1, "damage_dice", "0" },
	{ T_SET, W, 25, -1, "#damage_dice", "0" },
	{ T_SET, W, 25, -1, "fire_damage_dice", "6" },
	{ T_SET, W, 25, -1, "#fire_damage_dice", "1" },
	{ T_SET, W, 25, -1, "flash_sound", "2105" },
	{ T_SET, W, 25, -1, "wall_hit_sound", "2105" },
	{ T_SET, W, 26, -1, "damage_dice", "0" },
	{ T_SET, W, 26, -1, "#damage_dice", "0" },
	{ T_SET, W, 26, -1, "fire_damage_dice", "6" },
	{ T_SET, W, 26, -1, "#fire_damage_dice", "1" },
	{ T_SET, W, 26, -1, "flash_sound", "2105" },
	{ T_SET, W, 26, -1, "wall_hit_sound", "2105" },
	// 0x1AFC5: Warhammer 1d4+1, Warhammer +1 1d4+2 (+1 to hit), Club 1d6, Club+1 1d6+1
	{ T_COPY, W, 28, 24, "weapon_char", "" },
	{ T_SET, W, 28, -1, "#damage_dice", "1" },
	{ T_SET, W, 28, -1, "damage_dice", "4" },
	{ T_SET, W, 28, -1, "to_hit_bonus", "0" },
	{ T_SET, W, 28, -1, "damage_bonus", "1" },
	{ T_COPY, W, 40, 53, "weapon_char", "" },
	{ T_SET, W, 40, -1, "#damage_dice", "1" },
	{ T_SET, W, 40, -1, "damage_dice", "4" },
	{ T_SET, W, 40, -1, "to_hit_bonus", "1" },
	{ T_SET, W, 40, -1, "damage_bonus", "2" },
	{ T_SET, W, 29, -1, "damage_dice", "6" },
	{ T_SET, W, 29, -1, "to_hit_bonus", "0" },
	{ T_SET, W, 29, -1, "damage_bonus", "0" },
	{ T_SET, W, 41, -1, "damage_dice", "6" },
	{ T_SET, W, 41, -1, "to_hit_bonus", "1" },
	{ T_SET, W, 41, -1, "damage_bonus", "1" },
	{ T_SET, W, 41, -1, "value", "1000" },
	// 0x1B04A: Short Sword of Quickness, Mace of Disruption, Flame Tongue, Scimitar
	{ T_SET, W, 69, -1, "value", "8000" },
	{ T_SET, W, 70, -1, "value", "17500" },
	{ T_SET, W, 70, -1, "to_hit_bonus", "1" },
	{ T_SET, W, 70, -1, "damage_bonus", "1" },
	{ T_SET, W, 71, -1, "damage_dice", "0" },
	{ T_SET, W, 71, -1, "#damage_dice", "0" },
	{ T_SET, W, 71, -1, "fire_damage_dice", "8" },
	{ T_SET, W, 71, -1, "#fire_damage_dice", "1" },
	{ T_NAME, W, 73, -1, "", "Dwarven Scimitar" },
	{ T_SET, W, 45, -1, "value", "1500" },
	{ T_SET, W, 45, -1, "to_hit_bonus", "2" },
	{ T_SET, W, 45, -1, "damage_bonus", "2" },
	// 0x1B0B4: the wands and the rod: the spell, a missile
	{ T_SET, W, 55, -1, "PC_spell", "33" },
	{ T_SET, W, 55, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 56, -1, "PC_spell", "7" },
	{ T_SET, W, 56, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 57, -1, "PC_spell", "14" },
	{ T_SET, W, 57, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 58, -1, "PC_spell", "16" },
	{ T_SET, W, 58, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 59, -1, "PC_spell", "32" },
	{ T_SET, W, 59, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 60, -1, "PC_spell", "12" },
	{ T_SET, W, 60, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 61, -1, "PC_spell", "22" },
	{ T_SET, W, 61, -1, "attack_type", "PROJECTILE" },
	{ T_SET, W, 62, -1, "PC_spell", "32" },
	{ T_SET, W, 62, -1, "attack_type", "PROJECTILE" },
	// 0x1B134: two copies of the Long Sword +2: Drowslayer and Longsword +3
	{ T_COPYREC, W, 13, 33, "", "" },
	{ T_NAME, W, 13, -1, "", "Drowslayer" },
	{ T_SET, W, 13, -1, "value", "9000" },
	{ T_COPYREC, W, 14, 33, "", "" },
	{ T_NAME, W, 14, -1, "", "Longsword +3" },
	{ T_MULVAL, W, 14, 32, "value", "3" },
	{ T_SET, W, 14, -1, "to_hit_bonus", "3" },
	{ T_SET, W, 14, -1, "damage_bonus", "3" },
	// 0x1B1B6: the magic flag of the armor, then armor values and names
	{ T_MAGIC_ARMOR, 0, -1, -1, "", "" },
	{ T_SET, A, 16, -1, "value", "10000" },
	{ T_SET, A, 42, -1, "value", "4000" },
	{ T_SET, A, 19, -1, "value", "8000" },
	{ T_NAME, A, 27, -1, "", "Ring of Wizardry" },
	{ T_SET, A, 27, -1, "value", "32000" },
	{ T_CLASS, A, 27, -1, "", "2" },
	{ T_SET, A, 47, -1, "value", "10000" },
	{ T_SET, A, 48, -1, "value", "20000" },
	{ T_SET, A, 49, -1, "value", "30000" },
	{ T_SET, A, 34, -1, "value", "20000" },
	{ T_SET, A, 35, -1, "value", "7000" },
	{ T_SET, A, 36, -1, "value", "15000" },
	{ T_SET, A, 37, -1, "value", "20000" },
	{ T_SET, A, 38, -1, "value", "20000" },
	{ T_NAME, A, 50, -1, "", "Gauntlets of Dexterity" },
	{ T_SET, A, 50, -1, "value", "10000" },
	{ T_SET, A, 52, -1, "value", "10000" },
	{ T_SET, A, 43, -1, "value", "6000" },
	{ T_NAME, A, 45, -1, "", "Cloak of Lathandar" },
	{ T_SET, A, 45, -1, "value", "17500" },
	{ T_SET, A, 44, -1, "value", "4500" },
	{ T_SET, A, 46, -1, "value", "6000" },
};
#define N_FIXUPS	((int) (sizeof(Fixups) / sizeof(Fixups[0])))

// The words at 0x13A8DB: the cast time of the spells 0 to 55 (0x1AEDC).
static const short Cast_times[56] = {
	7, 5, 60, 60, 1, 1, 1, 4, 6, 2,
	5, 7, 1, 3, 4, 1, 3, 7, 5, 3,
	7, 3, 3, 5, 4, 60, 2, 1, 1, 2,
	1, 4, 3, 1, 4, 60, 2, 3, 6, 1,
	6, 600, 60, 5, 2, 1, 1, 3, 1, 1,
	600, 5, 5, 4, 60, 6,
};

// The names of the lines that the fixup changes or reads, in the 39
// mission tables. Each must match, or the fixup is not applied.
typedef struct dtu_tblfix_guard {
	int			kind, id;
	const char	*name;
} dtu_tblfix_guard;

static const dtu_tblfix_guard Guard[] = {
	{ M, 0, "Lantern" },
	{ M, 2, "Gem of Curing" },
	{ M, 5, "Spider Amulet" },
	{ M, 10, "Lockpicks" },
	{ M, 11, "Journeyman Lockpicks" },
	{ M, 12, "Master Lockpicks" },
	{ M, 13, "Apprentice Lockpicks" },
	{ M, 18, "Spider Piece" },
	{ M, 19, "Spider Piece" },
	{ M, 20, "Spider Piece" },
	{ M, 22, "Spider Piece" },
	{ M, 23, "Spider Piece" },
	{ M, 24, "Spider Piece" },
	{ M, 25, "Assembled Spider" },
	{ M, 26, "Spider Piece" },
	{ M, 72, "Potion of Agility" },
	{ M, 73, "Potion of Anti-Magic" },
	{ M, 74, "Potion of Dragon's Blood" },
	{ M, 75, "Potion of Extra-Healing" },
	{ M, 76, "Potion of Fire Resistance" },
	{ M, 77, "Potion of Flying" },
	{ M, 78, "Potion of Freedom" },
	{ M, 79, "Potion of Healing" },
	{ M, 80, "Potion of Hill Giant Strength" },
	{ M, 82, "Potion of Infravision" },
	{ M, 83, "Potion of Agility" },
	{ M, 84, "Potion of Invulnerability" },
	{ M, 86, "Potion of Luck" },
	{ M, 87, "Potion of Speed" },
	{ M, 88, "Potion of Toughening" },
	{ M, 106, "Cursed Scroll" },
	{ M, 127, "thief scroll" },
	{ S, 0, "Abjure" },
	{ S, 1, "Aid" },
	{ S, 4, "Burning Hands" },
	{ S, 5, "Chill Touch" },
	{ S, 6, "Color Spray" },
	{ S, 7, "Confusion" },
	{ S, 8, "Continual Light (Cleric)" },
	{ S, 9, "Continual Light (Mage)" },
	{ S, 12, "Detect Magic" },
	{ S, 13, "Dispel Magic" },
	{ S, 14, "Fear" },
	{ S, 15, "Feather Fall" },
	{ S, 16, "Fireball" },
	{ S, 17, "Flaming Sphere" },
	{ S, 21, "Haste" },
	{ S, 22, "Hold Person" },
	{ S, 23, "Hold Undead" },
	{ S, 24, "Improved Invisibility" },
	{ S, 26, "Invisibility" },
	{ S, 28, "Knock" },
	{ S, 30, "Light (Mage)" },
	{ S, 31, "Light (Cleric)" },
	{ S, 32, "Lightning Bolt" },
	{ S, 33, "Magic Missile" },
	{ S, 34, "Magical Stone" },
	{ S, 36, "Melf's Acid Arrows" },
	{ S, 37, "Melf's Minute Meteors" },
	{ S, 38, "Prayer" },
	{ S, 39, "Protection From Evil" },
	{ S, 40, "Protection From Fire" },
	{ S, 42, "Resist Cold" },
	{ S, 43, "Resist Fire" },
	{ S, 44, "Scare" },
	{ S, 46, "Sleep" },
	{ S, 47, "Slow" },
	{ S, 48, "Spider Climb" },
	{ S, 51, "Burning Hands" },
	{ S, 52, "Hold Person" },
	{ S, 53, "Protection From Evil" },
	{ S, 54, "Detect Magic" },
	{ S, 55, "Dispel Magic" },
	{ W, 13, "Wand of Lightning Bolts" },
	{ W, 14, "Black Lightning" },
	{ W, 15, "Bomb" },
	{ W, 24, "Throwing Hammer" },
	{ W, 25, "SLow Bomb" },
	{ W, 26, "fast Bomb" },
	{ W, 28, "Warhammer" },
	{ W, 29, "Club" },
	{ W, 32, "Long Sword +1" },
	{ W, 33, "Long Sword +2" },
	{ W, 40, "Warhammer +1" },
	{ W, 41, "Club+1" },
	{ W, 45, "Dagger of Impaling" },
	{ W, 53, "Throwing Hammer +1" },
	{ W, 55, "Wand of Magic Missiles" },
	{ W, 56, "Wand of Confusion" },
	{ W, 57, "Wand of Fear" },
	{ W, 58, "Wand of Fireballs" },
	{ W, 59, "Wand of Lightning Bolts" },
	{ W, 60, "Wand of Magic Detection" },
	{ W, 61, "Wand of Paralyzation" },
	{ W, 62, "Rod of Lightning" },
	{ W, 69, "Short Sword of Quickness" },
	{ W, 70, "Mace of Disruption" },
	{ W, 71, "Flame Tongue" },
	{ W, 73, "Scimitar" },
	{ A, 16, "Armoring" },
	{ A, 19, "Ring of Flying" },
	{ A, 27, "Ring of Warmth" },
	{ A, 34, "Winged Boots" },
	{ A, 35, "Boots of Feather Falling" },
	{ A, 36, "Boots of Levitation" },
	{ A, 37, "Boots of Speed" },
	{ A, 38, "Girdle of Hill Giant Strength" },
	{ A, 42, "Bracers of Defense AC2" },
	{ A, 43, "Cloak of Elvenkind" },
	{ A, 44, "Chameleon Cloak" },
	{ A, 45, "Cloak of Elvenkind" },
	{ A, 46, "Elven Cloak" },
	{ A, 47, "Cloak of Protection +1" },
	{ A, 48, "Cloak of Protection +2" },
	{ A, 49, "Cloak of Protection +3" },
	{ A, 50, "Gloves of Dexterity" },
	{ A, 52, "Gauntlets of Ogre Power" },
};
#define N_GUARD	((int) (sizeof(Guard) / sizeof(Guard[0])))

int dtu_tblfix_n_ops(void)
{
	return N_FIXUPS;
}

const dtu_tblfix_op *dtu_tblfix_op_row(int i)
{
	return (i >= 0 && i < N_FIXUPS) ? &Fixups[i] : NULL;
}

//------------------------------------------------------------------------
// The text of a record: words separated by spaces or tabs, a quoted part
// can have spaces. The keys and the words are not case sensitive (the
// readers use stricmp).

static int lower(int c)
{
	return (c >= 'A' && c <= 'Z') ? c + 'a' - 'A' : c;
}

static int same_n(const char *a, const char *b, int n)
{
	int i;

	for (i = 0; i < n; i++)
		if (lower((unsigned char) a[i]) != lower((unsigned char) b[i]) || !a[i])
			return 0;
	return 1;
}

// The next word at or after p: its start, *end its end. NULL at the end.
static const char *next_word(const char *p, const char **end)
{
	int in_quote = 0;

	while (*p == ' ' || *p == '\t')
		p++;
	if (!*p)
		return NULL;
	*end = p;
	for (; **end; (*end)++) {
		if (**end == '"')
			in_quote = !in_quote;
		else if ((**end == ' ' || **end == '\t') && !in_quote)
			break;
	}
	return p;
}

static int is_key(const char *w, const char *e, const char *key)
{
	int n = (int) strlen(key);

	return e - w > n && same_n(w, key, n) && w[n] == '=';
}

static int is_word(const char *w, const char *e, const char *word)
{
	int n = (int) strlen(word);

	return e - w == n && same_n(w, word, n);
}

// A growing text
typedef struct { char *s; int n, size; } buf;

static void put(buf *b, const char *s, int n)
{
	if (n < 0)
		n = (int) strlen(s);
	if (b->n + n + 1 > b->size) {
		int size = (b->n + n + 1) * 2 + 64;
		char *s2 = (char *) realloc(b->s, size);

		if (!s2)
			return;
		b->s = s2;
		b->size = size;
	}
	memcpy(b->s + b->n, s, n);
	b->n += n;
	b->s[b->n] = 0;
}

// The text, or NULL when it is the same as old (then it is freed).
static char *done(buf *b, const char *old)
{
	if (!b->s)
		put(b, "", 0);
	if (b->s && old && !strcmp(b->s, old)) {
		free(b->s);
		return NULL;
	}
	return b->s;
}

int dtu_tblfix_get(const char *text, const char *key, char *value, int size)
{
	const char *w, *e, *p = text;

	if (size > 0)
		value[0] = 0;
	if (!text)
		return 0;
	while ((w = next_word(p, &e)) != NULL) {
		if (is_key(w, e, key)) {
			int n = 0;

			for (w += strlen(key) + 1; w < e && n < size - 1; w++)
				if (*w != '"')
					value[n++] = *w;
			if (size > 0)
				value[n] = 0;
			return 1;
		}
		p = e;
	}
	return 0;
}

int dtu_tblfix_has(const char *text, const char *word)
{
	const char *w, *e, *p = text;

	if (!text)
		return 0;
	while ((w = next_word(p, &e)) != NULL) {
		if (is_word(w, e, word))
			return 1;
		p = e;
	}
	return 0;
}

static void put_kv(buf *b, const char *key, const char *value, int quote)
{
	put(b, key, -1);
	put(b, "=", 1);
	if (!quote)
		quote = !value[0] || strchr(value, ' ') != NULL || strchr(value, '\t') != NULL;
	if (quote) put(b, "\"", 1);
	put(b, value, -1);
	if (quote) put(b, "\"", 1);
}

// key=value in place of the first key=, the others go. value NULL: all go.
static char *set_key(const char *text, const char *key, const char *value, int quote)
{
	buf b = { NULL, 0, 0 };
	const char *w, *e, *p = text ? text : "";
	int found = 0;

	while ((w = next_word(p, &e)) != NULL) {
		if (is_key(w, e, key)) {
			put(&b, p, (int) (w - p));
			if (value && !found)
				put_kv(&b, key, value, quote);
			found = 1;
		} else
			put(&b, p, (int) (e - p));
		p = e;
	}
	put(&b, p, -1);
	if (!found && value) {
		put(&b, " ", 1);
		put_kv(&b, key, value, quote);
	}
	return done(&b, text);
}

char *dtu_tblfix_set(const char *text, const char *key, const char *value)
{
	return set_key(text, key, value, 0);
}

// on: the word is there (added at the end if it is not). Else no copy of it.
char *dtu_tblfix_word(const char *text, const char *word, int on)
{
	buf b = { NULL, 0, 0 };
	const char *w, *e, *p = text ? text : "";

	if (on) {
		if (dtu_tblfix_has(text, word))
			return NULL;
		put(&b, p, -1);
		put(&b, " ", 1);
		put(&b, word, -1);
		return done(&b, text);
	}
	while ((w = next_word(p, &e)) != NULL) {
		put(&b, p, (int) (w - p));
		if (!is_word(w, e, word))
			put(&b, w, (int) (e - w));
		p = e;
	}
	put(&b, p, -1);
	return done(&b, text);
}

//------------------------------------------------------------------------
// The spell flags, the word +0x36 of Spell_info. The reader 0x242F6 makes
// it of the words in the order of the line. The bit 0x800 has no table
// word; the port uses the word hit_objects for it (0x8CFFF and 0x90CEF
// then also act on objects that are not creatures or the player).

static const struct { const char *word; int bit; } Spell_bits[] = {
	{ "stackable", 0x002 },
	{ "cast_with_wand", 0x004 },
	{ "cast_with_scroll", 0x008 },
	{ "cast_with_arrow", 0x010 },
	{ "potion", 0x020 },
	{ "always", 0x040 },
	{ "homing", 0x080 },
	{ "self_cast_ok", 0x100 },
	{ "non_hotkey_item", 0x200 },
	{ "hit_objects", 0x800 },
};
#define N_SPELL_BITS	((int) (sizeof(Spell_bits) / sizeof(Spell_bits[0])))

int dtu_tblfix_spell_flags(const char *text)
{
	const char *w, *e, *p = text;
	int f = 0;

	if (!text)
		return 0;
	while ((w = next_word(p, &e)) != NULL) {
		if (is_word(w, e, "non_hotkey_item"))			f |= 0x200;
		else if (is_word(w, e, "cast_with_hands"))	f &= ~0x1C;
		else if (is_word(w, e, "cast_in_front"))		f |= 0x500;
		else if (is_word(w, e, "cast_with_wand"))		f = (f & ~0x1C) | 0x04;
		else if (is_word(w, e, "cast_with_scroll"))	f = (f & ~0x14) | 0x08;
		else if (is_word(w, e, "cast_with_arrow"))	f = (f & ~0x0C) | 0x10;
		else if (is_word(w, e, "stackable"))			f |= 0x02;
		else if (is_word(w, e, "potion"))				f |= 0x20;
		else if (is_word(w, e, "always"))				f |= 0x40;
		else if (is_word(w, e, "homing"))				f |= 0x80;
		else if (is_word(w, e, "self_cast_ok"))		f |= 0x100;
		else if (is_word(w, e, "hit_objects"))			f |= 0x800;
		else if (is_key(w, e, "type")) {
			if (same_n(w + 5, "target", 6) && e - w == 11)	f &= ~1;
			else if (same_n(w + 5, "area", 4) && e - w == 9)	f |= 1;
		}
		p = e;
	}
	return f;
}

// Replace t by the result of f (when f made a new text).
static void step(char **t, char *n)
{
	if (n) {
		free(*t);
		*t = n;
	}
}

char *dtu_tblfix_set_spell_flags(const char *text, int flags)
{
	char *t = strdup(text ? text : "");
	int i, have = dtu_tblfix_spell_flags(t);

	if (!t)
		return NULL;
	if ((have & 1) != (flags & 1))
		step(&t, dtu_tblfix_set(t, "type", (flags & 1) ? "AREA" : "TARGET"));
	// The bits 0x100 and 0x400 come from self_cast_ok and cast_in_front
	if ((have & 0x500) != (flags & 0x500)) {
		step(&t, dtu_tblfix_word(t, "cast_in_front", (flags & 0x500) == 0x500));
		step(&t, dtu_tblfix_word(t, "self_cast_ok", (flags & 0x500) == 0x100));
	}
	if ((have & 0x1C) != (flags & 0x1C))
		step(&t, dtu_tblfix_word(t, "cast_with_hands", 0));
	for (i = 0; i < N_SPELL_BITS; i++) {
		int bit = Spell_bits[i].bit;

		if (bit == 0x100)
			continue;
		if ((have & bit) != (flags & bit) || (bit & 0x1C))
			step(&t, dtu_tblfix_word(t, Spell_bits[i].word, (flags & bit) != 0));
	}
	if (!strcmp(t, text ? text : "")) {
		free(t);
		return NULL;
	}
	return t;
}

//------------------------------------------------------------------------
// The class mask (+0x00): cleric_use 1, mage_use 2, fighter_use 4,
// thief_use 0x20 (also lockpick for a misc line), all_use 0xFFFF.

static const struct { const char *word; int bit; } Class_bits[] = {
	{ "cleric_use", 0x01 },
	{ "mage_use", 0x02 },
	{ "fighter_use", 0x04 },
	{ "thief_use", 0x20 },
};

int dtu_tblfix_class(const char *text)
{
	int i, m = 0;

	if (dtu_tblfix_has(text, "all_use"))
		m |= 0xFFFF;
	for (i = 0; i < 4; i++)
		if (dtu_tblfix_has(text, Class_bits[i].word))
			m |= Class_bits[i].bit;
	if (dtu_tblfix_has(text, "lockpick"))
		m |= 0x20;
	return m;
}

char *dtu_tblfix_set_class(const char *text, int mask)
{
	char *t = strdup(text ? text : "");
	int i;

	if (!t)
		return NULL;
	if ((mask & 0xFFFF) != 0xFFFF)
		step(&t, dtu_tblfix_word(t, "all_use", 0));
	for (i = 0; i < 4; i++)
		step(&t, dtu_tblfix_word(t, Class_bits[i].word, (mask & Class_bits[i].bit) && (mask & 0xFFFF) != 0xFFFF));
	if ((mask & 0xFFFF) == 0xFFFF)
		step(&t, dtu_tblfix_word(t, "all_use", 1));
	if (!strcmp(t, text ? text : "")) {
		free(t);
		return NULL;
	}
	return t;
}

// The wear mask (+0x04 of Misc_info): the words of the reader 0x23780
static const char *Wear_words[12] = {
	"helmet", "cloak", "necklace", "armor", "gauntlet", "shield",
	"ring", "belt", "boot", "bow", "crossbow", "lockpick",
};

//------------------------------------------------------------------------
// Apply

typedef struct { const dtu_tblfix_io *io; int n; } ctx;

static void logf_(const ctx *c, const char *fmt, int a, int b, const char *s)
{
	char line[200];

	if (!c->io->log)
		return;
	snprintf(line, sizeof(line), fmt, a, b, s ? s : "");
	c->io->log(line);
}

static void put_text(ctx *c, int kind, int id, char *t)
{
	if (!t)
		return;
	c->io->set_text(kind, id, t);
	c->io->sync(kind, id);
	c->n++;
}

static int to_int(const char *s)
{
	return (int) strtol(s, NULL, 0);
}

static void set_name(ctx *c, int kind, int id, const char *name)
{
	static const char *keys[3] = { "name", "weapon_name", "spell_name" };
	char *t = c->io->text(kind, id), *n, *buf = c->io->name(kind, id);
	char *copy;
	int i, found = 0;

	if (!t)
		return;
	copy = strdup(t);
	if (!copy)
		return;
	for (i = 0; i < 3; i++)
		if (dtu_tblfix_get(copy, keys[i], NULL, 0)) {
			found = 1;
			step(&copy, set_key(copy, keys[i], name, 1));
		}
	if (!found)
		step(&copy, set_key(copy, "name", name, 1));
	if (buf && c->io->name_size > 0) {
		strncpy(buf, name, c->io->name_size - 1);
		buf[c->io->name_size - 1] = 0;
	}
	n = strcmp(copy, t) ? copy : NULL;
	if (!n)
		free(copy);
	put_text(c, kind, id, n);
}

// The text of src with the number word of dst ("$SPELL 54 ...")
static char *line_copy(const char *src, const char *dst, int kind)
{
	buf b = { NULL, 0, 0 };
	const char *ws, *we, *wd, *ed;

	if (kind != DTU_TK_SPELL)
		return strdup(src);
	ws = next_word(src, &we);
	wd = next_word(dst, &ed);
	if (!ws || !wd || *ws < '0' || *ws > '9' || *wd < '0' || *wd > '9')
		return strdup(src);
	put(&b, src, (int) (ws - src));
	put(&b, wd, (int) (ed - wd));
	put(&b, we, -1);
	return b.s;
}

static int guard_ok(const ctx *c)
{
	int i;

	for (i = 0; i < N_GUARD; i++) {
		const dtu_tblfix_guard *g = &Guard[i];
		char *name = g->id < c->io->count(g->kind) ? c->io->name(g->kind, g->id) : NULL;
		int n;

		if (!name || !c->io->text(g->kind, g->id)) {
			logf_(c, "Table fixup: line %d of table %c missing, not applied%s", g->id, g->kind, "");
			return 0;
		}
		n = (int) strlen(name);
		while (n > 0 && name[n - 1] == ' ')
			n--;
		if (n != (int) strlen(g->name) || memcmp(name, g->name, n)) {
			logf_(c, "Table fixup: line %d of table %c is not '%s', not applied", g->id, g->kind, g->name);
			return 0;
		}
	}
	return 1;
}

int dtu_tblfix_apply(const dtu_tblfix_io *io)
{
	ctx c;
	int i, j;
	char v[64], num[32];

	c.io = io;
	c.n = 0;
	if (!guard_ok(&c))
		return -1;
	for (i = 0; i < N_FIXUPS; i++) {
		const dtu_tblfix_op *o = &Fixups[i];
		char *t = o->kind ? io->text(o->kind, o->id) : NULL;
		char *s = NULL;

		if (o->kind && !t)
			continue;
		if (o->src >= 0) {
			int sk = (o->op == T_COPY && o->val[0]) ? o->val[0] : o->kind;

			s = o->src < io->count(sk) ? io->text(sk, o->src) : NULL;
			if (!s)
				continue;
		}
		switch (o->op) {
			case T_SET:
				put_text(&c, o->kind, o->id, dtu_tblfix_set(t, o->key, o->val[0] ? o->val : NULL));
				break;
			case T_COPY:
				put_text(&c, o->kind, o->id, dtu_tblfix_set(t, o->key, dtu_tblfix_get(s, o->key, v, sizeof(v)) ? v : NULL));
				break;
			case T_NAME:
				set_name(&c, o->kind, o->id, o->val);
				break;
			case T_CLASS:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_class(t, to_int(o->val)));
				break;
			case T_CLASS_OR:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_class(t, dtu_tblfix_class(t) | to_int(o->val)));
				break;
			case T_WEAR_OR: {
				char *n = strdup(t);

				for (j = 0; j < 12 && n; j++)
					if (to_int(o->val) & (1 << j))
						step(&n, dtu_tblfix_word(n, Wear_words[j], 1));
				if (n && !strcmp(n, t)) {
					free(n);
					n = NULL;
				}
				put_text(&c, o->kind, o->id, n);
				break;
			}
			case T_FLAGS:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_spell_flags(t, to_int(o->val)));
				break;
			case T_FLAGS_OR:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_spell_flags(t, dtu_tblfix_spell_flags(t) | to_int(o->val)));
				break;
			case T_FLAGS_AND:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_spell_flags(t, dtu_tblfix_spell_flags(t) & to_int(o->val)));
				break;
			case T_FLAGS_COPY:
				put_text(&c, o->kind, o->id, dtu_tblfix_set_spell_flags(t, dtu_tblfix_spell_flags(s)));
				break;
			case T_RENDER: {
				// +0x30 (the model of weapon_pof) and +0x38 (the render type)
				// of src, and +0x32 (weapon_vclip) when val is 1. The tables
				// give every source line weapon_vclip and no weapon_pof.
				char *n = strdup(t);

				if (!n)
					break;
				step(&n, dtu_tblfix_set(n, "weapon_pof", dtu_tblfix_get(s, "weapon_pof", v, sizeof(v)) ? v : NULL));
				if (o->val[0] == '1')
					step(&n, dtu_tblfix_set(n, "weapon_vclip", dtu_tblfix_get(s, "weapon_vclip", v, sizeof(v)) ? v : NULL));
				if (!strcmp(n, t)) {
					free(n);
					n = NULL;
				}
				put_text(&c, o->kind, o->id, n);
				break;
			}
			case T_COPYREC: {
				char name[64];
				char *n = line_copy(s, t, o->kind);

				// The name tables are not part of the copy: the line keeps
				// its name.
				snprintf(name, sizeof(name), "%s", io->name(o->kind, o->id) ? io->name(o->kind, o->id) : "");
				if (!n)
					break;
				io->copy_line(o->kind, o->id, o->src);
				io->set_text(o->kind, o->id, n);
				c.n++;
				set_name(&c, o->kind, o->id, name);
				io->sync(o->kind, o->id);
				break;
			}
			case T_MULVAL:
				snprintf(num, sizeof(num), "%d", (dtu_tblfix_get(s, o->key, v, sizeof(v)) ? atoi(v) : 0) * to_int(o->val));
				put_text(&c, o->kind, o->id, dtu_tblfix_set(t, o->key, num));
				break;
			case T_CAST_TIMES:
				for (j = 0; j < io->count(DTU_TK_SPELL) && j < (int) (sizeof(Cast_times) / sizeof(Cast_times[0])); j++) {
					char *st = io->text(DTU_TK_SPELL, j);

					if (!st)
						continue;
					snprintf(num, sizeof(num), "%d", Cast_times[j]);
					put_text(&c, DTU_TK_SPELL, j, dtu_tblfix_set(st, "cast_time", num));
				}
				break;
			case T_MAGIC_WEAPONS:
			case T_MAGIC_ARMOR: {
				int k = o->op == T_MAGIC_WEAPONS ? DTU_TK_WEAPON : DTU_TK_ARMOR;

				for (j = 0; j < io->count(k); j++) {
					char *wt = io->text(k, j), *name = io->name(k, j);
					int plot = dtu_tblfix_get(wt, "plot_item", v, sizeof(v)) && atoi(v) != 0;

					if (!wt)
						continue;
					if (plot || (k == DTU_TK_WEAPON && name && strchr(name, '+')))
						put_text(&c, k, j, dtu_tblfix_word(wt, "magic", 1));
				}
				break;
			}
		}
	}
	logf_(&c, "Table fixup of 1.3 (0x1A344): %d of %d writes changed the tables%s", c.n, N_FIXUPS, "");
	return c.n;
}

void dtu_tblfix_free(char *text)
{
	free(text);
}
