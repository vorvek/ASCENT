# Object setup at level load (DTUGAME.EXE 1.3)

Status: read from the code of `DTUGAME.EXE` 1.3 and checked against the 34 level files
of the disc that have a table. Addresses are linear addresses of the LE image with
object 1 at 0x10000 (`gamedata/re/dtu_flat.bin`). No decompiler was used. Each fact has
one of three marks:

- **Code**: read from the disassembly at the address given.
- **Data**: checked by `python -I tools/objcheck.py gamedata/unpacked`.
- **Guess**: not proved.

Names of functions are the Descent 1 names where the code is the Descent 1 code
(`engine/main`). Other names are made up from the log strings.

Related documents: [lvl.md](../formats/lvl.md) (object record of the file),
[tbl.md](../formats/tbl.md) (table text), [egg.md](../formats/egg.md) (archives).

## Summary

- `obj->id` is the index of the table line of the object class, counted from 0 in table
  order. There is no other mapping. (Code, Data)
- The loader does not trust the model number and the bitmap number of the file. After
  the read it sets them again from the table entry `[id]`. (Code)
- Size and hit points (`shields`) come from the file for all types, with two
  exceptions: a creature with a model gets the radius of the model as size, and a
  dressing object gets 1.0 as shields. (Code)
- An item on the floor is an object with render type 8. It is drawn as one bitmap of
  the object bitmap table `ObjBitmaps[]`, as a rod of Descent 1: height and width are
  two times `obj->size`. (Code)
- The player picks an item up by collision. There is no use key in this path. (Code)
- The 124 byte block of the file record holds the spell effects of the object: one
  flag word and one timer for each effect. The loader clears the flag word. (Code)
- A creature is a monster or an NPC by the int at 0x1B0: bit 22 is "hostile", the low
  9 bits are a script number. There is no NPC object type. (Code)

## Object struct (479 bytes)

`Objects[]` is at 0x2B0614, 800 entries of 0x1DF bytes (0x75D30). The highest used
index is the int at 0x13D324.

Offsets that object setup uses. "D1" means: same field as in Descent 1, found by the
read order of `read_object` (0x47D90) and by use.

| Offset | Type | Field | Source |
|---|---|---|---|
| 0x00 | int | signature. The loader counts it up from 0 (counter at 0x13D3EF) | Code 0x48C74 |
| 0x04 | u8 | type | D1 |
| 0x05 | u8 | id | D1 |
| 0x0A | u8 | control_type | D1 |
| 0x0B | u8 | movement_type | D1 |
| 0x0C | u8 | render_type | D1 |
| 0x0D | u8 | flags. 2 is "should be dead" (set on pickup). 0x40: the object queries skip the object (0x3F38F), so no move, shot or use ray touches it, and it is still drawn; set for a creature whose table entry has `pass_thru` and by the level fixes (see "Level fixes (0xACCDB)") | Code 0x2846B, 0x6A102, 0x3F38F |
| 0x0E | s16 | segnum | D1 |
| 0x10 | s16 | file value (`attached_obj`, guess) | Code 0x47E09 |
| 0x12 | s16 | set to -1 by the loader | Code 0x47E15 |
| 0x14 | vec | pos | D1 |
| 0x20 | matrix | orient (rvec 0x20, uvec 0x2C, fvec 0x38) | D1 |
| 0x44 | matrix | copy of orient | Code 0x47E31 |
| 0x68 | fix | size | D1 |
| 0x6C | fix | shields (hit points) | D1 |
| 0x70 | vec | last_pos | D1 |
| 0x7C | s8 | contains_type | D1 |
| 0x7D | s8 | contains_id | D1 |
| 0x7E | s8 | contains_count | D1 |
| 0x80 to 0xF8 | | spell effects, see "The 124 byte block" | Code |
| 0x84 | fix | lifeleft. Not in the file. The loader sets 0x3FFFFFFF (the `IMMORTAL_TIME` of Descent 1) | Code 0x477E1 |
| 0xFC | 64 bytes | physics_info of Descent 1: velocity 0xFC, thrust 0x108, mass 0x114, drag 0x118, brakes 0x11C, rotvel 0x120, rotthrust 0x12C, turnroll 0x138, flags 0x13E | D1 |
| 0x140 | | control part. For AI: behavior 0x140, flags[11] 0x141, hide_segment 0x14C, hide_index 0x14E, path_length 0x150, cur_path_index 0x152, two path segments 0x154, 0x156 | D1 |
| 0x162 | int or s16 | render part. Model number (int) for a polygon object. Bitmap number (s16, `obj->bitmap_num` in the assert text) for render type 8 | Code 0x67E3C |
| 0x166 | int | subobj_flags | D1 |
| 0x16A | int | tmap_override | D1 |
| 0x16E, 0x170, 0x172, 0x176 | s16, s16, int, int | animation values of the model. Not covered here | |
| 0x17A | fix[9] | copy of the nine speed values of the creature table entry (0xAC) | Code 0x6A7E3 |
| 0x19E | u8 | `spell_points` of the creature table entry | Code 0xAE4FA |
| 0x19F | u8 | `cleric_points` of the creature table entry | Code 0xAE506 |
| 0x1A2 | int | 0 for a hostile creature, -1 for one that is not | Code 0xAC6F1 |
| 0x1A8, 0x1C0 | fix | set to the game time at setup | Code 0xAE4EE |
| 0x1AE | u8 | low byte of the second flag word of the table entry (2 is `pass_thru`). Bit 4 is set when the object is picked up | Code 0x4782C, 0x47A31, 0x2848A |
| 0x1B0 | int | creature and script flags, see "Monsters and NPCs" | Code |
| 0x1CD, 0x1D1 | int | cleared by `fix_object_segs` | Code 0x6A015 |
| 0x1D5 | s8 | count of an item, see "Pick up" | Code |
| 0x1DB | int | set by `init_ai_object` | Code 0x10551 |

The parts of the struct that are not in this table were not analyzed.

## Tables

All readers are in `bm.c` (assert strings). Each reader takes the next free entry, so
entry n is line n of its section. Each reader also appends one entry to `ObjType[]`
(0x1B75E6) and `ObjId[]` (0x1B7438), 430 entries each, counter at 0x1B5650.

| Class | Type | Section | Reader | Table base | Entry size | Count at | Limit | Names (32 bytes each) |
|---|---|---|---|---|---|---|---|---|
| creature | 2 | `$MONSTER` | 0x1DDE6 | `Robot_info` 0x38E344 | 242 | 0x13DA54 | 40 | 0x1B50EC |
| weapon | 5 | `$WEAPON` | 0x209F3 | `Weapon_info` 0x1E9D08 | 217 | 0x13CC6C | 90 | 0x1B45AC |
| armor | 15 | `$ARMOR` | 0x22BB1 | `Armor_info` 0x19FD24 | 58 | 0x13A84B | 80 | 0x1B3BAC |
| misc | 16 | `$OBJECT` | 0x23780 | `Misc_info` 0x19BABC | 68 | 0x13A84F | 250 | 0x1B14EC |
| spell | 17 | `$SPELL` | 0x242F6 | `Spell_info` 0x1B5676 | 127 | 0x13A853 | 60 | 0x1B0D6C |
| dressing | 18 | `$DRESSING` | 0x225DF | `Dressing_info` 0x19B33C | 32 | 0x13A85B | 60 | 0x1B342C |

The entry size is the index scaling in the code (for example 17 * 4 for `Misc_info`).
The limit is the constant of the assert. `MAX_SPELL_TYPES` is 60 (compare at 0x24316;
tbl.md did not have it). The six name tables lie one after the other, and each has
exactly limit * 32 bytes (Code: the address differences).

`$SPELL` lines carry their number. The reader stops with "Spell number is incorrect"
if the number is not the index of the line (0x24575).

Other globals:

| Address | Meaning |
|---|---|
| 0x18F19C | `ObjBitmaps[]`, 1700 entries of 4 bytes. One entry is the bitmap that the bitmap loader 0x19DEF returns for a name |
| 0x18D704 | `ObjBitmapPtrs[]`, 1700 entries of 4 bytes. One entry is the address of an `ObjBitmaps[]` entry |
| 0x13A8A9 | s16 `N_ObjBitmaps` (the code reads it also as the int at 0x13A8A7 shifted right by 16) |
| 0x13A897 | int `N_ObjBitmapPtrs` |
| 0x3880D0 | `Polygon_models[]` as pointers. The radius of a model is at +0x44, its texture pointer at +0x4C |
| 0x3878B0 | `Pof_names[]`, 13 bytes each. Count at 0x13D4F8 |
| 0x1D27A8 | `Save_pof_names[]`, the 13 byte names of the level file |
| 0x18777C, 0x1874D4, 0x187614 | `stack_limit=` of misc, armor and weapon types, one int each, default 10 |

### How picture= becomes a bitmap number

`picture=NAME` of a `$OBJECT`, `$ARMOR`, `$SPELL` or `$WEAPON` line and `bitmap=NAME` of
a `$DRESSING` line call 0x1DCA9 (Code: 0x239B2, 0x22DE0, 0x24673, 0x21EBD, 0x2287B).

0x1DCA9 does this:

1. Assert `bitmap_count < MAX_OBJ_BITMAPS` (1700).
2. For a plain name: load the bitmap (0x19DEF), store it in
   `ObjBitmaps[N_ObjBitmaps]`, store the address of that entry in
   `ObjBitmapPtrs[N_ObjBitmapPtrs]`, add 1 to both counters.
3. For `%N`: use the slot of eclip N (int at 0x1BD802 + N * 194). If the eclip has no
   slot yet, give it `ObjBitmaps[N_ObjBitmaps]` and add 1 to `N_ObjBitmaps`. Then store
   the slot address in `ObjBitmapPtrs[]` and add 1 to `N_ObjBitmapPtrs`.

The reader then stores `N_ObjBitmaps - 1` as the bitmap number of the entry. So the
bitmap number of an item is an index into `ObjBitmaps[]`, not into `ObjBitmapPtrs[]`.

The texture names after `model=`, `pof=` and `weapon_pof=` go through the same function
(0x1EE29, 0x22A2B, 0x2249F). So items and model textures share one numbering, and the
number of an item depends on all lines before it, also on the lines of the player
table. `picture=` of a `$MONSTER` line does not take a slot: it calls the bitmap loader
0x19DEF directly (0x1EAD7).

### Misc_info (68 bytes), reader 0x23780

| Offset | Type | Keyword or meaning | Default |
|---|---|---|---|
| 0x00 | u16 | class mask: `cleric_use` 1, `mage_use` 2, `fighter_use` 4, `thief_use` 0x20, `lockpick` 0x20, `all_use` 0xFFFF | 0 |
| 0x02 | s16 | `icon` | 0 |
| 0x04 | u16 | wear slot: `helmet` 1, `cloak` 2, `necklace` 4, `armor` 8, `gauntlet` 0x10, `shield` 0x20, `ring` 0x40, `belt` 0x80, `boot` 0x100, `bow` 0x200, `crossbow` 0x400, `lockpick` 0x800 | 0 |
| 0x06 | s16 | `value` | 0 |
| 0x08 | s16 | `PC_spell` | -1 |
| 0x0A | s16 | `PC_spell_2` | -1 |
| 0x0C | s16 | `to_hit_bonus` | 0 |
| 0x0E | s16 | `damage_bonus` | 0 |
| 0x10 | s16 | `uses` | 1 |
| 0x12 to 0x20 | s16 each | `adjust_strength`, `adjust_dexterity`, `adjust_intelligence`, `adjust_wisdom`, `adjust_constitution`, `adjust_charisma`, `adjust_hit_points`, `adjust_thief_ability` | 0 |
| 0x22 | u16 | flags: `stackable` 1, `bow` 2, `crossbow` 4, `non_hotkey_item` 8, `khelben` 0x200 | 0 |
| 0x24 | s16 | bitmap_num, from `picture`. The reader asserts that it is not -1 | -1 |
| 0x26 | s16 | `plot_item` | 0 |
| 0x28 | fix | `size` | 5.0 |
| 0x2C | fix | `duration` | 1.0 |
| 0x30 | fix | `mass` or `weight` (0x23A25, 0x2389E) | 1.0 |
| 0x34 | fix | `player_light` (0x23B5B) | 0 |
| 0x38 | int | `speed` | 1.0 |
| 0x3C | fix | `modifier` | 1.0 |
| 0x40 | int | flags 2: `pass_thru` 2 | 0 |

### Armor_info (58 bytes), reader 0x22BB1

| Offset | Type | Keyword or meaning |
|---|---|---|
| 0x00 | u16 | class mask, same bits as `Misc_info` |
| 0x02 | s16 | `icon` |
| 0x04 | u16 | wear slot, same bits as `Misc_info` (up to `boot` 0x100) |
| 0x06 | s16 | `value` |
| 0x08 | s16 | `to_hit_bonus` |
| 0x0A | s16 | `damage_bonus` |
| 0x0C | s16 | `PC_spell` |
| 0x0E | s16 | `armor_class_decrement` |
| 0x10 | u16 | flags: `magic` 1, `MAGIC_OK` 2, `metal` 4, `thief_ok` 8, `stackable` 0x10, `non_hotkey_item` 0x20 |
| 0x12 to 0x20 | s16 each | the eight `adjust_` values, same order as `Misc_info` |
| 0x22 | s16 | bitmap_num, from `picture`. Asserted not -1 |
| 0x24 | s16 | `plot_item` |
| 0x26 | fix | `size` |
| 0x2A | int | `speed` |
| 0x2E | fix | `player_light` |
| 0x32 | fix | `mass` or `weight` |
| 0x36 | fix | `modifier` |

### Spell_info (127 bytes), reader 0x242F6

Only the fields that object setup uses.

| Offset | Type | Keyword or meaning |
|---|---|---|
| 0x00 | u16 | class mask: `cleric_use` 1, `mage_use` 2 (reader 0x242F6) |
| 0x02 | s16 | `icon` |
| 0x24 | s16 | bitmap_num, from `picture` |
| 0x26 | u8 | `plot_item` |
| 0x28 | s16 | `uses`, default 1 |
| 0x2A | s16 | `uses_per_level` |
| 0x2C | s16 | `levels_per_use` |
| 0x36 | u8 | flags. The mask 2 is tested as "stackable" by 0xB61B2. All bits: "The table fixup" below |
| 0x38 | u8 | render type of the missile: 2 for `weapon_pof`, 3 for `weapon_vclip` |
| 0x4E | fix | `duration`, default 30.0 |
| 0x56 | fix | `size` |
| 0x39 | u8 | `level` |
| 0x3A to 0x48 | 5 x 3 u8 | damage dice: `damage_dice`, `fire_`, `cold_`, `acid_`, `elec_damage_dice`, each as sides, `#` number, `_damage_level` add per level (addresses 0x1B56B0 to 0x1B56BE for entry 0; the table starts at 0x1B5676). The reader stores at 0x24E87 to 0x25049, the damage function is 0x91BC3 (see ai.md, "Spell damage") |

### Weapon_info (217 bytes), reader 0x209F3

The entry starts like the `weapon_info` of Descent 1 (`render_type` at 0). Only the
fields that object setup uses.

| Offset | Type | Keyword or meaning |
|---|---|---|
| 0x00 | u8 | render type of the missile (3 for `weapon_vclip`) |
| 0x1C | s16 | `ammo_initial` |
| 0x1E | s16 | bitmap_num, from `picture` |
| 0x2E | s16 | `uses` |
| 0x32 | s16 | `icon` |
| 0x34 | s16 | `value` |
| 0x3E | u8 | flags (the reader, 0x222E0 to 0x223FA): `2_handed_weapon` 1, `stackable` 2, `arrow` 4, `bolt` 8, `magic` 0x10, `bonus_exempt` 0x20, `auto_hit` 0x40, `non_hotkey_item` 0x80 |
| 0x58 | fix | `size` |
| 0x8C | fix | `mass` or `weight` |

### Dressing_info (32 bytes), reader 0x225DF

| Offset | Type | Keyword or meaning | Default |
|---|---|---|---|
| 0x00 | s16 | number: model number for `pof=`, vclip number for `vclip=`, bitmap_num for `bitmap=` | -1 |
| 0x02 | s16 | render type: 1 for `pof=`, 7 for `vclip=`, 8 for `bitmap=`. The reader asserts that it is not 0 | 0 |
| 0x04 | u16 | flags: `magic` 1, `animates` 2. `cant_break` is accepted and sets nothing | 0 |
| 0x06 | s16 | `first_frame` | -1 |
| 0x08 | s16 | `last_frame` | -1 |
| 0x0A | s16 | `explode_sound`. The level start code replaces -1 by 2704 (0xAD732) | -1 |
| 0x0C | fix | `size` | 5.0 |
| 0x10 | fix | `lightcast` or `light_cast` | 0 |
| 0x14 | int | flags 2: `pass_thru` 2 | 0 |
| 0x18 | int | `pushable` | 999 |
| 0x1C | int | `break_vclip` | 137 |

For `pof=` the reader loads the model at the end of the line (0x72DC5) and stores the
model number at 0x00 (0x22B60).

### Robot_info (242 bytes), reader 0x1DDE6

The reader collects the values in local variables and stores them at the end of the
line (0x1EF9F to 0x1F35E).

| Offset | Type | Keyword or meaning |
|---|---|---|
| 0x00 | int | model number of the first `model=` (0x1EEDA) |
| 0x08 | s8 | `contains_id` |
| 0x09 | s8 | `contains_count` |
| 0x0A | s8 | `contains_prob` |
| 0x0B | s8 | `contains_type`: `monster` 2, `armor` 15, `misc` 16, `spell` 17 |
| 0x0C | int | `score_value` |
| 0x10 | fix | `lighting` |
| 0x14 | fix | `mass` or `weight` |
| 0x18 | fix | `drag` |
| 0x1C | fix | `gravity` |
| 0x20 to 0x60 | | values of `$MONSTER_AI` (reader 0x1DC02). Not covered here |
| 0x61 | s8 | `cloak_type` |
| 0x62 | s8 | `attack_type`: `projectile` 0, `melee` 1 |
| 0x63 | u8 | `door_opening`. Bit 1 is also set at setup for a `humanoid` |
| 0x64, 0x66, 0x68 | s16 | `see_sound`, `attack_sound`, `claw_sound` |
| 0x6A | s16 | `weapon_type` |
| 0x6C | s16 | `damage_dice` |
| 0x6E | s16 | `#damage_dice` |
| 0x70 | s16 | `to_hit_bonus` |
| 0x72 | s16 | `damage_bonus` |
| 0x74 | s16 | `thaco` or `thac0` |
| 0x76 | s16 | `armor_class` |
| 0x78 | s16 | `max_hit_points` |
| 0x7A | s16 | `max_hit_points` again (the keyword sets two variables) |
| 0x7E | s16 | `regeneration` |
| 0x80, 0x8A | s16[5] each | two groups of five values, one for each difficulty level. `init_ai_object` reads them (0x104FD). Guess: the two 16-bit groups of `$MONSTER_AI` |
| 0x94 | int | bitmap of `picture` (result of 0x19DEF) |
| 0x98 | u16 | flags: `blind` 1, `electricity_immune` 2, `extraplanar` 4, `fire_immune` 8, `fire_resistant` 0x10, `flying` 0x20, `humanoid` 0x40, `infravision` 0x80, `light_sensitive` 0x100, `light_sensitive_2` 0x200, `magic_resistant` 0x400, `no_scare` 0x800, `no_sleep` 0x1000, `undead` 0x2000, `weapon_resistant` 0x4000 |
| 0x9A, 0x9C | s16 | `hit_sound`, `die_sound` |
| 0x9E | s8 | `see_sound_max` |
| 0x9F | s8 | `bossiness` |
| 0xA0 | int | `hit_vclip` |
| 0xA4 | int | `blood` |
| 0xA8 | int | flags 2: `pass_thru` 2 |
| 0xAC | fix[9] | three copies of the three `speeds` values, each times 2 |
| 0xD0 | int | behavior flags: `dark_lover` 1, `light_lover` 2, `prison_boy` 4, `pain_flee` 8, `jumping_jack` 0x10, `impatient` 0x20, `patient` 0x40 |
| 0xD4 | int | second copy of `weapon_type` |
| 0xD8 | u8 | 100. Plus 75 with `patient`. Minus 75 with `impatient` |
| 0xD9 | u8 | flags 3: `noattack` 1, `rat` 4. 8 is set by the reader for some names (one prefix is `dro`) |
| 0xDA | int | `monster_hit_vclip` |
| 0xDE | int | `str` |
| 0xE2 | u8 | `spell_level` |
| 0xE3 | u8 | `spell_level` again, only if `cleric_points` is not 0 |
| 0xE4 | u8 | `spell_points` |
| 0xE5 | u8 | `cleric_points` |
| 0xE6 | fix | `dead_adjust` |
| 0xEA | int | `moola` |
| 0xEE | fix | `regonzo` |

The reader does not store a size. A creature has the radius of its model.

## The table fixup (0x1A344)

`bm_read_all` (0x1B2F5) calls 0x1A344 once at its end (0x1C8F9), after the level table
and the player table. 0x1A344 has no condition. It writes fixed values into fixed line
numbers of `Misc_info`, `Spell_info`, `Weapon_info`, `Armor_info` and the name tables.
So 1.3 never shows the table text of these fields. Checked in the Ghidra C
(`functions/0001a344.c`) and in the instructions of `dtu13.elf` (the size of each
write, the arguments of the copies).

The first loop of 0x1A344 sets the misc flag bits 0x10 and 0x20 by name. The port has
it in `pickup_flag_10` (dtuobj_item.c). The rest is the list below.

### The fields that the fixup writes

The keyword of each offset comes from the readers (0x242F6 spells, 0x209F3 weapons,
0x23780 misc, 0x22BB1 armor).

`Spell_info`, 127 bytes at 0x1B5676. The reader stores `damage_bonus` at -2 (0x1B5674),
so one entry is the bytes -2 to +0x7C. The copies (memcpy 0xD7CD2, 127 bytes) start
at -2.

| Offset | Size | Keyword |
|---|---|---|
| +0x00 | u16 | class mask: `cleric_use` 1, `mage_use` 2 (the reader knows no other bit) |
| +0x02 | s16 | `icon` |
| +0x04 | s16 | `cast_time` |
| +0x06, +0x08 | s16 | `range`, `radius` (default 20) |
| +0x0A, +0x0C | s16 | `level_range`, `level_radius` |
| +0x0E | s16 | `cast_sound` (default 10) |
| +0x10 to +0x18 | s16 | `monster_hit_sound`, `monster_miss_sound`, `monster_kill_sound`, `wall_hit_sound`, `armor_hit_sound` |
| +0x1A to +0x22 | s16 | `cast_vclip`, `monster_hit_vclip`, `monster_miss_vclip`, `wall_hit_vclip`, `armor_hit_vclip` |
| +0x24 | s16 | bitmap of `picture` |
| +0x26, +0x27 | u8 | `plot_item`, `type=sub_use_count` |
| +0x28, +0x2A, +0x2C | s16 | `uses`, `uses_per_level`, `levels_per_use` |
| +0x2E | s16 | `to_hit_bonus` |
| +0x30 | s16 | the model of `weapon_pof` |
| +0x32 | s16 | `weapon_vclip` |
| +0x36 | u8 | flags: `type=AREA` 1 (`type=TARGET` clears it), `stackable` 2, `cast_with_wand` 4, `cast_with_scroll` 8, `cast_with_arrow` 0x10 (`cast_with_hands` clears the three), `potion` 0x20, `always` 0x40, `homing` 0x80 |
| +0x37 | u8 | flags: `self_cast_ok` 1, `non_hotkey_item` 2, `cast_in_front` 1 and 4. Bit 8 has no keyword: only the fixup sets it |
| +0x38 | u8 | render type: 2 for `weapon_pof`, 3 for `weapon_vclip` |
| +0x39 | u8 | `level` (default 1) |
| +0x3A to +0x48 | u8 | `damage_dice`, `#damage_dice`, `damage_level`, then the same three for `fire_`, `cold_`, `acid_`, `elec_` |
| +0x5A | fix | `player_light` |
| +0x6E | s16 | `combustion` |
| +0x70 | s16 | `value` (default 10000) |

Bit 8 of +0x37: the effect code 0x8CFFF skips its test "the target is the player or a
creature" when the bit is set (0x8D053), and the area code 0x90CEF takes each object,
not only the player and the creatures (0x90DA6). The port calls it the word
`hit_objects`.

`Weapon_info`, 217 bytes at 0x1E9D08: +0x0D `damage_dice`, +0x0E `#damage_dice`, +0x0F
`fire_damage_dice`, +0x10 `#fire_damage_dice`, +0x12 `attack_type` (`projectile` 0,
`melee` 1), +0x13 `weapon_char` (one letter), +0x20 `flash_sound`, +0x24 to +0x2C
`monster_hit_sound`, `monster_miss_sound`, `monster_kill_sound`, `wall_hit_sound`,
`armor_hit_sound`, +0x34 `value`, +0x36 `to_hit_bonus`, +0x38 `damage_bonus`, +0x3A
`PC_spell`, +0x3C `enemy_spell`, +0x3E flags (`magic` 0x10), +0x52 `plot_item`.

`Misc_info` and `Armor_info`: see their tables above. The fixup writes the class mask,
the wear mask (bit 0x800 is the wear bit of `lockpick`), `icon`, `value` and `uses`.

Example: the Warhammer (weapon 28) has `damage_dice=10 #damage_dice=3` in every table.
The fixup writes +0x0E 1, +0x0D 4, +0x36 0, +0x38 1 (0x1AFC5): 1.3 has a Warhammer of
1d4+1.

### Order

The order matters, because some writes copy fields of lines that a later write changes.
Fear and Haste take the missile of Light (Mage) before the fixup changes it, and Produce
Fire takes the flags of Fireball after the fixup changes them. The weapon magic loop
runs before the two copies of weapon 33, so Drowslayer and Longsword +3 have the magic
bit of the Long Sword +2.

### The lines

The record numbers are fixed. The 39 tables of the mission levels have the same lines at
these numbers. Seven older tables (BASE, DEMO, MINES1, MOIN1, PRISON1, SPIDER, TRAIN1)
have other lines at some numbers, and 1.3 writes onto those other items there. The port
applies the writes of a table section (items, armor, weapons, spells) only when each line
of the section that the fixup changes or reads has the name of the mission tables (Guard[]
in dtutblfix.c). A write that copies from another section needs that section too. BASE,
MINES1, PRISON1, SPIDER and TRAIN1 have the weapon and spell lines of the mission tables,
so they get the weapon and spell writes; DEMO and MOIN1 get none. `tools/check_tblfix.py
--tables` checks that the 39 tables pass, the five pass with weapons and spells, and the
two do not.

Results that a player sees:

- New names: misc 83 Potion of Invisibility (the second Potion of Agility), spell 17
  Produce Fire (Flaming Sphere), spell 42 Cure Blindness or Deafness (Resist Cold),
  spell 51 Find Traps (the second Burning Hands), weapon 13 Drowslayer, weapon 14
  Longsword +3, weapon 73 Dwarven Scimitar, armor 27 Ring of Wizardry (Ring of Warmth),
  armor 45 Cloak of Lathandar (the second Cloak of Elvenkind), armor 50 Gauntlets of
  Dexterity (Gloves of Dexterity).
- The cleric spells 52 to 55 are copies of the mage spells 22, 39, 12 and 13 with
  their own name and the cleric class.
- Area spells: Fear, Feather Fall, Haste (around the caster), Dispel Magic,
  Fireball, Produce Fire, Hold Person, Hold Undead, Sleep, Slow, Scare and the four
  Light spells (where the missile lands). Knock loses the cast on the caster and fires
  its missile. Chill Touch gets the cast on the caster.
- Protection From Fire (40) gets class 0x27 (cleric and mage). The cast check 0x35683
  tests the cleric bit first, so 1.3 casts it as a cleric spell. A mage can learn it
  (0xB65BB tests the mage bit) but cannot cast it. The port: `Dtu_fix_learn_class` (bugs.md D31).
- Weapons 13 and 14 become copies of the Long Sword +2. The creatures with
  `weapon_type=13` (drowfem, thashasa, Mage Spirit, prstsprt) or 14 (flayer) fire
  this weapon in 1.3. Not checked in the game.

### The port

`engine/main/dtutblfix.c` has the writes as one list (Fixups[]) in the order of
0x1A344. The port reads the table text (`bm_tbl_get`, `bm_tbl_has_flag`), so each
write changes the text of the record: `key=value`, a flag word, the class words, the
name keys. A copy copies the text and the decoded fields (`item_info`, and
`Weapon_info` for a weapon) but not the name. `bm_load_level_tables` (bmread.c) applies
the list after both tables. The debug option `-rawtables` skips it.
`tools/tblfix.py` applies the same list to a table for the tools (`wiki_rules.Table`
does it by default). `test_dtutblfix` checks a sample of each group, and
`tools/check_tblfix.py` prints the change of each line of a real table and compares the
C code with `tools/tblfix.py` (ctest `check_tblfix`).

The writes in order (the rows of Fixups[], the address of the first write of each
group):

| # | Code | Writes, in order |
|---|---|---|
| 1 | 0x1A46D: misc values and names | misc 2 `uses=-1`; misc 2 `value=5000`; misc 72 `value=900`; misc 73 `value=2400`; misc 74 `value=700`; misc 75 `value=1000`; misc 76 `value=400`; misc 77 `value=750`; misc 78 `value=900`; misc 79 `value=400`; misc 80 `value=900`; misc 80 class 4; misc 82 `value=500`; misc 83 `value=500`; misc 83 name "Potion of Invisibility"; misc 84 `value=500`; misc 84 class 4; misc 86 `value=1500`; misc 87 `value=450`; misc 88 `value=600` |
| 2 | 0x1A528: the four lockpicks: thief class bit and lockpick wear bit | misc 10 class \|= 0x20; misc 10 wear \|= 0x800; misc 11 class \|= 0x20; misc 11 wear \|= 0x800; misc 12 class \|= 0x20; misc 12 wear \|= 0x800; misc 13 class \|= 0x20; misc 13 wear \|= 0x800; misc 106 `icon` of misc 127 |
| 3 | 0x1A56C: the spider pieces: every class, one use | misc 18 class \|= 0x3F; misc 19 class \|= 0x3F; misc 23 class \|= 0x3F; misc 22 class \|= 0x3F; misc 24 class \|= 0x3F; misc 26 class \|= 0x3F; misc 20 class \|= 0x3F; misc 25 class \|= 0x3F; misc 18 `uses=1`; misc 19 `uses=1`; misc 23 `uses=1`; misc 22 `uses=1`; misc 24 `uses=1`; misc 26 `uses=1`; misc 20 `uses=1`; misc 25 `uses=1` |
| 4 | 0x1A5EB: Aid | spell 1 `range=0`; spell 1 `radius=0`; spell 1 `level_range=0`; spell 1 `level_radius=0`; spell 1 flags 0x102 |
| 5 | 0x1A615: Abjure, Burning Hands, Color Spray, Confusion | spell 0 `cast_sound=2129`; spell 0 `damage_dice=8`; spell 0 `#damage_dice=2`; spell 0 `damage_level=3`; spell 4 `fire_damage_dice=3`; spell 4 `#fire_damage_dice=1`; spell 4 `fire_damage_level=2`; spell 6 `cast_sound=2104`; spell 7 flags 0x2 |
| 6 | 0x1A65E: Chill Touch: cast on the caster (+0x37 bit 0), not AREA | spell 5 flags \|= 0x100; spell 5 flags &= 0xFFFE; spell 5 `cast_sound=2131`; spell 5 `radius=3` |
| 7 | 0x1A67D: Detect Magic: a mage spell, copied to 54 as a cleric spell | spell 12 class 2; spell 54 = copy of spell 12 (not the name); spell 54 class 1 |
| 8 | 0x1A6A3: Fear, Haste (the missile of Light (Mage) before its change) | spell 14 `range=0`; spell 14 `radius=90`; spell 14 `level_range=0`; spell 14 `level_radius=0`; spell 14 flags 0x103; spell 14 missile of spell 30 (+0x30, +0x32, +0x38); spell 21 `range=0`; spell 21 `radius=80`; spell 21 `level_range=0`; spell 21 `level_radius=0`; spell 21 flags 0x103; spell 21 missile of spell 30 (+0x30, +0x32, +0x38) |
| 9 | 0x1A73F: Knock (+0x30 and +0x38 only), the Light spells | spell 28 `range=360`; spell 28 `radius=0`; spell 28 `level_range=0`; spell 28 `level_radius=20`; spell 28 missile of spell 17 (+0x30, +0x38); spell 28 flags 0x2; spell 30 `range=360`; spell 30 `radius=3`; spell 30 `level_range=0`; spell 30 `level_radius=0`; spell 30 missile of spell 17 (+0x30, +0x32, +0x38); spell 30 flags 0x3; spell 30 `player_light=20`; spell 30 class 2; spell 31 `range=720`; spell 31 `radius=3`; spell 31 `level_range=0`; spell 31 `level_radius=0`; spell 31 missile of spell 17 (+0x30, +0x32, +0x38); spell 31 flags 0x3; spell 31 `player_light=20`; spell 31 class 1; spell 9 `range=360`; spell 9 `radius=3`; spell 9 `level_range=0`; spell 9 `level_radius=0`; spell 9 missile of spell 17 (+0x30, +0x32, +0x38); spell 9 flags 0x3; spell 9 `player_light=60`; spell 9 class 2; spell 8 `range=720`; spell 8 `radius=3`; spell 8 `level_range=0`; spell 8 `level_radius=0`; spell 8 missile of spell 17 (+0x30, +0x32, +0x38); spell 8 flags 0x3; spell 8 `player_light=60`; spell 8 class 1 |
| 10 | 0x1A8ED: Dispel Magic, copied to 55 as a cleric spell | spell 13 `range=720`; spell 13 `radius=60`; spell 13 `level_range=0`; spell 13 `level_radius=0`; spell 13 missile of spell 6 (+0x30, +0x38); spell 13 `weapon_vclip=176`; spell 13 flags 0x803; spell 13 `cast_sound` of spell 42; spell 13 `player_light=0`; spell 13 class 2; spell 55 = copy of spell 13 (not the name); spell 55 class 1 |
| 11 | 0x1A972: Feather Fall, Fireball | spell 15 `range=60`; spell 15 `radius=20`; spell 15 `level_range=6`; spell 15 `level_radius=0`; spell 15 flags 0x103; spell 16 `range=60`; spell 16 `radius=40`; spell 16 `level_range=60`; spell 16 `level_radius=0`; spell 16 flags 0x803; spell 16 `fire_damage_dice=6`; spell 16 `#fire_damage_dice=1`; spell 16 `fire_damage_level=0`; spell 16 `damage_dice=0`; spell 16 `#damage_dice=0`; spell 16 `damage_level=0` |
| 12 | 0x1A9F5: Hold Person, copied to 52 as a cleric spell of level 2 | spell 22 `range=720`; spell 22 `radius=40`; spell 22 `level_range=0`; spell 22 `level_radius=0`; spell 22 flags 0x3; spell 22 class 2; spell 52 = copy of spell 22 (not the name); spell 52 `level=2`; spell 52 class 1 |
| 13 | 0x1AA4F: Hold Undead, Invisibility, Improved Invisibility | spell 23 `range=120`; spell 23 `radius=80`; spell 23 `level_range=0`; spell 23 `level_radius=0`; spell 23 flags 0x3; spell 26 `range=3`; spell 26 `radius=3`; spell 26 `level_range=0`; spell 26 `level_radius=0`; spell 24 `range=3`; spell 24 `radius=3`; spell 24 `level_range=0`; spell 24 `level_radius=0` |
| 14 | 0x1AABA: Lightning Bolt, Magic Missile, Magical Stone | spell 32 `range=240`; spell 32 `radius=5`; spell 32 `level_range=60`; spell 32 `level_radius=0`; spell 32 flags 0x802; spell 32 `elec_damage_dice=6`; spell 32 `#elec_damage_dice=1`; spell 32 `elec_damage_level=0`; spell 32 `damage_dice=0`; spell 32 `#damage_dice=0`; spell 32 `damage_level=0`; spell 32 `combustion=-1`; spell 33 `range=360`; spell 33 `radius=3`; spell 33 `level_range=60`; spell 33 `level_radius=0`; spell 33 `damage_dice=4`; spell 33 `#damage_dice=1`; spell 33 `damage_level=1`; spell 34 `range=180`; spell 34 `radius=0`; spell 34 `level_range=0`; spell 34 `level_radius=0`; spell 34 `damage_dice=4`; spell 34 `#damage_dice=1`; spell 34 `damage_level=0` |
| 15 | 0x1AB90: Melf's Acid Arrows, Melf's Minute Meteors | spell 36 `range=1080`; spell 36 `radius=3`; spell 36 `level_range=0`; spell 36 `level_radius=0`; spell 36 `damage_dice=0`; spell 36 `#damage_dice=0`; spell 36 `damage_level=0`; spell 36 `fire_damage_dice=0`; spell 36 `#fire_damage_dice=0`; spell 36 `fire_damage_level=0`; spell 36 `acid_damage_dice=4`; spell 36 `#acid_damage_dice=2`; spell 36 `acid_damage_level=0`; spell 37 `range=420`; spell 37 `radius=6`; spell 37 `level_range=60`; spell 37 `level_radius=0`; spell 37 flags 0x802; spell 37 `fire_damage_dice=4`; spell 37 `#fire_damage_dice=1`; spell 37 `fire_damage_level=0`; spell 37 `damage_dice=0`; spell 37 `#damage_dice=0`; spell 37 `damage_level=0` |
| 16 | 0x1AC4A: Prayer, Protection From Evil (copied to 53), Protection From Fire | spell 38 `range=0`; spell 38 `radius=120`; spell 38 `level_range=0`; spell 38 `level_radius=0`; spell 38 flags 0x102; spell 39 `range=3`; spell 39 `radius=3`; spell 39 `level_range=0`; spell 39 `level_radius=0`; spell 39 flags 0x102; spell 39 class 0x26; spell 53 = copy of spell 39 (not the name); spell 53 class 0x25; spell 40 class 0x27 |
| 17 | 0x1ACD4: Scare, Sleep, Slow, Spider Climb | spell 44 `range=180`; spell 44 `radius=30`; spell 44 `level_range=60`; spell 44 `level_radius=0`; spell 44 flags 0x3; spell 46 `range=180`; spell 46 `radius=60`; spell 46 `level_range=0`; spell 46 `level_radius=0`; spell 46 flags 0x3; spell 47 `range=540`; spell 47 `radius=80`; spell 47 `level_range=60`; spell 47 `level_radius=0`; spell 47 flags 0x3; spell 48 `icon` of misc 5 |
| 18 | 0x1AD67: Flaming Sphere becomes the cleric spell Produce Fire | spell 17 name "Produce Fire"; spell 17 `damage_bonus=0`; spell 17 class 1; spell 17 `range=240`; spell 17 `radius=24`; spell 17 `level_range=0`; spell 17 `level_radius=0`; spell 17 `cast_sound` of spell 16; spell 17 `monster_hit_sound` of spell 16; spell 17 `monster_miss_sound` of spell 16; spell 17 `monster_kill_sound` of spell 16; spell 17 `wall_hit_sound` of spell 16; spell 17 `armor_hit_sound` of spell 16; spell 17 `to_hit_bonus=0`; spell 17 flags of spell 16; spell 17 `damage_dice=0`; spell 17 `#damage_dice=0`; spell 17 `damage_level=0`; spell 17 `fire_damage_dice=4`; spell 17 `#fire_damage_dice=1`; spell 17 `fire_damage_level=1`; spell 17 `combustion=26`; spell 17 `value` of spell 0; spell 17 `level=4` |
| 19 | 0x1AE54: Resist Cold becomes Cure Blindness or Deafness | spell 42 name "Cure Blindness or Deafness"; spell 42 class 1; spell 42 flags 0x102; spell 42 `level=3`; spell 42 `value` of spell 38; spell 42 `cast_vclip` of spell 13 |
| 20 | 0x1AE9A: the second Burning Hands becomes Find Traps | spell 51 name "Find Traps"; spell 51 class 1; spell 51 `icon` of misc 0; spell 51 flags 0x102; spell 51 `level=2`; spell 51 `value` of spell 43 |
| 21 | 0x1AEDC: the cast times, 0x1AF11: the magic flag of the weapons | each spell: `cast_time` from the words at 0x13A8DB; each weapon with `plot_item` or a `+` in the name: `magic` |
| 22 | 0x1AF4B: Bomb, SLow Bomb, fast Bomb: fire 1d6, sounds 2105 | weapon 15 `damage_dice=0`; weapon 15 `#damage_dice=0`; weapon 15 `fire_damage_dice=6`; weapon 15 `#fire_damage_dice=1`; weapon 15 `flash_sound=2105`; weapon 15 `wall_hit_sound=2105`; weapon 25 `damage_dice=0`; weapon 25 `#damage_dice=0`; weapon 25 `fire_damage_dice=6`; weapon 25 `#fire_damage_dice=1`; weapon 25 `flash_sound=2105`; weapon 25 `wall_hit_sound=2105`; weapon 26 `damage_dice=0`; weapon 26 `#damage_dice=0`; weapon 26 `fire_damage_dice=6`; weapon 26 `#fire_damage_dice=1`; weapon 26 `flash_sound=2105`; weapon 26 `wall_hit_sound=2105` |
| 23 | 0x1AFC5: Warhammer 1d4+1, Warhammer +1 1d4+2 (+1 to hit), Club 1d6, Club+1 1d6+1 | weapon 28 `weapon_char` of weapon 24; weapon 28 `#damage_dice=1`; weapon 28 `damage_dice=4`; weapon 28 `to_hit_bonus=0`; weapon 28 `damage_bonus=1`; weapon 40 `weapon_char` of weapon 53; weapon 40 `#damage_dice=1`; weapon 40 `damage_dice=4`; weapon 40 `to_hit_bonus=1`; weapon 40 `damage_bonus=2`; weapon 29 `damage_dice=6`; weapon 29 `to_hit_bonus=0`; weapon 29 `damage_bonus=0`; weapon 41 `damage_dice=6`; weapon 41 `to_hit_bonus=1`; weapon 41 `damage_bonus=1`; weapon 41 `value=1000` |
| 24 | 0x1B04A: Short Sword of Quickness, Mace of Disruption, Flame Tongue, Scimitar | weapon 69 `value=8000`; weapon 70 `value=17500`; weapon 70 `to_hit_bonus=1`; weapon 70 `damage_bonus=1`; weapon 71 `damage_dice=0`; weapon 71 `#damage_dice=0`; weapon 71 `fire_damage_dice=8`; weapon 71 `#fire_damage_dice=1`; weapon 73 name "Dwarven Scimitar"; weapon 45 `value=1500`; weapon 45 `to_hit_bonus=2`; weapon 45 `damage_bonus=2` |
| 25 | 0x1B0B4: the wands and the rod: the spell, a missile | weapon 55 `PC_spell=33`; weapon 55 `attack_type=PROJECTILE`; weapon 56 `PC_spell=7`; weapon 56 `attack_type=PROJECTILE`; weapon 57 `PC_spell=14`; weapon 57 `attack_type=PROJECTILE`; weapon 58 `PC_spell=16`; weapon 58 `attack_type=PROJECTILE`; weapon 59 `PC_spell=32`; weapon 59 `attack_type=PROJECTILE`; weapon 60 `PC_spell=12`; weapon 60 `attack_type=PROJECTILE`; weapon 61 `PC_spell=22`; weapon 61 `attack_type=PROJECTILE`; weapon 62 `PC_spell=32`; weapon 62 `attack_type=PROJECTILE` |
| 26 | 0x1B134: two copies of the Long Sword +2: Drowslayer and Longsword +3 | weapon 13 = copy of weapon 33 (not the name); weapon 13 name "Drowslayer"; weapon 13 `value=9000`; weapon 14 = copy of weapon 33 (not the name); weapon 14 name "Longsword +3"; weapon 14 `value` = value of weapon 32 x 3; weapon 14 `to_hit_bonus=3`; weapon 14 `damage_bonus=3` |
| 27 | 0x1B1B6: the magic flag of the armor, then armor values and names | each armor with `plot_item`: `magic`; armor 16 `value=10000`; armor 42 `value=4000`; armor 19 `value=8000`; armor 27 name "Ring of Wizardry"; armor 27 `value=32000`; armor 27 class 2; armor 47 `value=10000`; armor 48 `value=20000`; armor 49 `value=30000`; armor 34 `value=20000`; armor 35 `value=7000`; armor 36 `value=15000`; armor 37 `value=20000`; armor 38 `value=20000`; armor 50 name "Gauntlets of Dexterity"; armor 50 `value=10000`; armor 52 `value=10000`; armor 43 `value=6000`; armor 45 name "Cloak of Lathandar"; armor 45 `value=17500`; armor 44 `value=4500`; armor 46 `value=6000` |

## Order of operations at level load

`load_level` (0x494EB) loads the table first (call 0x499E7), then the mine, then the
game data (`load_game_data`, 0x489EE, call at 0x49B10).

1. Object loop (0x48C4E to 0x48C9A). For each record:
   1. `read_object` (0x47D90): clears the 479 bytes, then reads the record.
   2. `signature` = counter (0x48C74).
   3. `verify_object` (0x477D9).
   4. `setup_object` (0xAE3F2, name made up).
2. Walls, doors, triggers and so on.
3. "reset objects" (0x69E03), then the test that each object is in its segment and the
   link into the segment list (0x69FB8, 0x684DD), "clear transient objects" (0x6A282).
4. `place_creatures` (0x6A6FB, name made up, in `object.c`).
5. "reset doors", "fix walls".
6. `fix_object_segs` (0x69FFB). In this game it also sets model and bitmap numbers.
7. 0x4A24E (not analyzed).
8. `level_start` (0xAD5BF, name made up; log text "RLH: found %d objects, %d NPCs and
   %d scripts").

### read_object, 0x47D90 (Code)

See lvl.md for the read order. Facts that lvl.md does not have:

- The value read for 0xDC (file offset 169) is also written to 0x88 (0x47FC4). 0x88 is
  the effect flag word, and 0xDC is 0 in all 2562 records. So the loader clears the
  effect flags of every object, also the value that the file has at offset 85.
- For types 5, 15, 16 and 17 the low byte of 0xAC goes to 0x1D5 and 0xAC is cleared
  (0x48046).

### verify_object, 0x477D9 (Code)

Compare with `verify_object` in `engine/main/gamesave.c`.

1. `lifeleft` (0x84) = 0x3FFFFFFF.
2. Type 2 (creature):
   - If `id >= N_robot_types`: `id = id % N_robot_types`.
   - 0x1AE = low byte of `Robot_info[id]` flags 2 (0xA8).
   - If render type is 1: model number = `Robot_info[id]` model number, and
     `size` = radius of that model.
   - If movement type is 1 (physics): mass = table 0x14, drag = table 0x18, and the
     value at 0x11C (brakes in Descent 1) = table 0x1C (`gravity`).
   - If `shields` is less than 1.0: the creature is loaded dead. The two AI flag bytes
     0x142 and 0x143 get 7, the animation value 0x172 gets the int at 0x13D9F2 shifted
     right by 16 minus 1, 0x88 gets 0, and 0x6B9B0 is called.
   - `shields` is not set here. The hit points of a creature are the value of the
     file.
3. Any other type with render type 1: the model number is translated by name, from
   `Save_pof_names[]` to `Pof_names[]` (as in Descent 1).
4. Type 7 (powerup): the object is killed (flag 2, 0x6A3CC), control type 0, size 1.0.
5. Type 5 (weapon): if `id >= N_weapon_types` then `id = 0` (with the assert
   `obj->render_type != RT_POLYOBJ`). Else bitmap number = `Weapon_info[id]` 0x1E.
6. Type 17 (spell): bitmap number = `Spell_info[id]` 0x24.
7. Type 15 (armor): bitmap number = `Armor_info[id]` 0x22.
8. Type 18 (dressing): 0x1AE = low byte of `Dressing_info[id]` flags 2. If the render
   type is not 1: bitmap number = `Armor_info[id]` 0x22.
9. Type 16 (misc), render type not 1: bitmap number = `Armor_info[id]` 0x22.
10. Type 4 (player): as in Descent 1 (console player init 0x67FF4, or the model of the
    player type), orientation made orthogonal, `id` = player counter.

Steps 8 and 9 read the armor table for a dressing and a misc object (0x47A54,
0x47A80). This looks like a copy error in the original source. It has no effect in the
end, because step 6 of the load order writes the right value.

### setup_object, 0xAE3F2 (Code)

1. Type 18: `shields` = 1.0.
2. Type 2: mass (0x114) = `size`. So the mass of the table is not used for a creature
   that comes from a level file.
3. Type 4: mass = a float table value (0x1453EE, index by a player value, or 5) times
   the float at 0x131B99.
4. Mass is at least 4.0.
5. Type 2 only:
   - If behavior (0x140) is 0x84 and the table `attack_type` is 1 (melee): behavior =
     0x81.
   - 0x1C0 and 0x1A8 = game time. 0x19E = table `spell_points`. 0x19F = table
     `cleric_points`.
   - If `tmap_override` is one of the four texture numbers at 0x3CE0D4 to 0x3CE0E0:
     the table entry (not the object) gets the flag `fire_immune`.
   - If the table entry has `humanoid`: table `door_opening` gets bit 1.

### place_creatures, 0x6A6FB (Code)

For each of the 800 object slots with type 2:

1. If the position is not inside the segment (0x4BE0B): move the object to the center
   of its segment (0x4AD9C). Then, if object flag 0x20 is not set, call 0x6B9B0.
2. Animation start: 0x170 = 1, 0x176 = 1, 0x172 = the int at 0x13D9E0 shifted right
   by 16, 0x13A = 0, 0x138 = -1, 0x1AF = 0, AI flag bytes 0x142 and 0x143 = 1.
3. Copy the nine speed values of the table entry (0xAC) to 0x17A.
4. 0x1A2 = 0. Make the orientation orthogonal and copy it to 0x44.

This answers the last open point of lvl.md: the game moves a creature that is in no
segment to the center of its segment. It does this for creatures only.

### fix_object_segs, 0x69FFB (Code)

For each object 0 to the highest index:

1. 0x1CD = 0, 0x1D1 = 0.
2. Type 16, render 8: bitmap number = `Misc_info[id]` 0x24.
3. Type 2, render 1: model number = `Robot_info[id]` 0x00. The nine speed values are
   copied again. 0x1AE = low byte of flags 2. If that is not 0: object flag 0x40.
4. Type 15, render 8: bitmap number = `Armor_info[id]` 0x22.
5. Type 5, render 8: bitmap number = `Weapon_info[id]` 0x1E.
6. Type 17, render 8: bitmap number = `Spell_info[id]` 0x24.
7. Type 18, render 8: bitmap number = `Dressing_info[id]` 0x00.
8. Type 18, render 1: model number = `Dressing_info[id]` 0x00.
9. `update_object_seg` (0x69FB8), as in Descent 1.

So after the load, the model of a creature or dressing object and the bitmap of an
item are the values of table entry `[id]`. The values of the file are not used.

### level_start, 0xAD5BF (Code)

1. `init_robots` (0x16408) and resets of script state.
2. Fixes for single levels: 0xACCDB is a switch on the level number (the byte at
   0x1D338D + Player_num * 6967). It changes fixed object numbers and table entries
   (for example 0xAD1B4 for level 25). See "Level fixes (0xACCDB)" below.
3. Each dressing type with `explode_sound` -1 gets sound 2704.
4. Open the script data file (the "GAB data file").
5. For each object 1 to the highest index:
   - Type 2: `creature_fixes` (0xAC7B2). It changes table entries by the name of the
     creature type and the object. See ai.md, "Creature setup by name".
   - Type 2, or any type with a script number: if the object is not yet in the NPC
     table (0xAC3C3), add it (`add_npc`, 0xAC431). A full table (100) ends the loop.
   Steps 4 and 5 run only if the script data file opens (0xAD771 skips them).

`init_ai_objects` (0x1065D) still exists, but it skips type 2 (0x10685). It calls
`init_ai_object` (0x1038B) only for an AI object that is not a creature, and that
function then prints "RLH: trying to init a non-critter!". The other callers of
`init_ai_object` create or change single creatures at run time (0x13664, 0x25BEA,
0x3DBF0, 0xACF9C, 0xBE55C, 0xCA05E).

`init_ai_object(objnum, behavior, hide_segment)` is the Descent 1 function with these
differences (Code, 0x1038B to 0x1065C):

- `ai_local` is at 0x153EB0, 47 bytes for each object.
- A behavior of 0 becomes 0x81. A stored behavior outside 0x80 to 0x84 becomes 0x81
  with the log text "[obj %i -> WANDER]". So 0x81 is "wander". Behavior 0x84 with a
  melee creature becomes 0x81.
- For a creature with a model it sets the animation start (same values as
  `place_creatures`).
- `ai_local` 0x28 = the mean of the two table values 0x80 and 0x8A for the difficulty
  level.
- 0x1DB = game time + 0x100 - table 0xD4. 0x8C, 0x1C0, 0x1A8 = game time. 0x19E and
  0x19F = table `spell_points` and `cleric_points`.
- Physics flag 4 is set (0x13E).

In the level files the behavior is 0x81 in 1015 creature records and 0x84 in 95
(Data).

### Level fixes (0xACCDB), verified (code)

level_start calls 0xACCDB(0x5A742(level)) at 0xAD716, before the loop of
`creature_fixes` and `add_npc`. The argument is 1 when the level has a saved
state (0x296304). The function is a switch on the level byte (the index in
DTU.MSN). It writes fixed object numbers (`Objects` 0x2B0614, 479 bytes
each) and table entries (`Robot_info` 0x38E344, 242 bytes; `Dressing_info`
0x19B33C, 32 bytes). The level loader runs it at each load, before the
restore of a state (0x5AA75), so the values of a state file win.

The object types in the right column are those of the level files of the
disc (`tools/lvl.py`); the executable tests the type only where the table
says "if".

| Level | Address | Change | Object in the level file |
|---|---|---|---|
| 0 ypinn | 0xACDB2 | objects 1, 2, 16, 17, 27, 28, 30, 32 (list 0xA645F), if not dead: hostile bit off, awareness - 1, +0x1A2 = -1, morale, animation values | the 8 NPCs of the inn |
| 2 kobolds | 0xACF0C | only with no state: times +0x1A8, +0x1C0 of object 19; `weapon_type` of its type = 0x12; object 6 id = 0x27; object 69 removed if weapon 0x37 | 19 creature id 0, 6 armor id 41, 69 weapon id 55 |
| 3 bones | 0xACF6F | object 55 peaceful (bit 9), 40 hit points; `init_ai_object(11 and 23, 0x80, segment)` | 55 creature (script 31), 11 and 23 creatures |
| 5 thief0 | 0xACFBE | object 48 peaceful | creature (script 53) |
| 7 thief2 | 0xACFD3 | object 93 peaceful; object 1 script 0x47 (71) and peaceful | 93 creature (script 70), 1 creature id 17 with no script |
| 9 bane | 0xAD003 | object 53 removed if weapon 13; objects 26, 27 if scenery: movement 3 (spin 0xCCC about y), `tmap_override` from a run time table | 53 weapon 13, 26 and 27 scenery |
| 11 mine | 0xAD0A1 | object 1 peaceful | creature |
| 12 crypts | 0xAD0AB | object 74 peaceful | creature (script 121) |
| 13 mulhor1 | 0xAD0C0 | objects 91, 73, 1, 2, 34, 60 if misc: object flag 0x40 | misc id 14 |
| 16 myrkul | 0xAD129 | object 3: control type 0, Hold (+0x89 bit 0x40) with the timer +0xC0 0x3FFFFFFF, peaceful | creature (script 160) |
| 19 clan3 | 0xAD157 | the type of object 10: flags `|= 0x7042`, damage 1 x 10, +0x7C = 0x16 | creature (script 190) |
| 21 drow2 | 0xAD198 | object 43 peaceful bit off; object 42 script 0xD5 (213) | 43 creature (script 212), 42 scenery with no script |
| 25 beholder | 0xAD1B4 | object 109 script 0xFF (255), `init_ai_object(109, 0x80, hide segment)`, AI flag 6 = 1, 120 hit points, effect bits `|= 0xE2013A09` with timers 0x3FFFFFFF, armor pool 23, +0x82 += 180, +0x80 = 9 + rand() % 4 | creature id 5 with no script |
| 27 drow5 | 0xAD25A | the type of object 83: armor class -3, score 9000, damage 10 x 5, 100 hit points; object 83 hit points from 0x393230 (past the end of the monster table: a defect); object 13 if scenery: script 0x111 (273), spin 0x147, 256 hit points, `pushable` 14; object 84 removed if creature | 13 scenery, 83 creature, 84 dead creature |
| 29 ibbalar | 0xAD320 | object 90, if alive and it contains nothing: contains weapon 0x3A; object 82 peaceful; object 2 if scenery: script 0x126, spin 1, 256 hit points, its type cannot break | 90 creature, 82 creature, 2 scenery (script 294 in the file) |
| 32 flame | 0xAD3BB | object 2 (altar) and 3 (Flamesword) if scenery: cannot break, `pushable` 0x7FFF, +0x1AE bit 2, object flag 0x40; object 3 script 0x142 (322), spin 0x147; object 4 if misc: script 0x143 (323); object 17 (Halaster) if creature: 256 hit points, table values; object 35 flag 0x40; object 36 (Lolth) if creature: 256 hit points (no state only), melee, table values, no effects, not fleeing | 2, 3 scenery, 4 misc id 25 with no script, 17, 35, 36 creatures |

Script 323 of object 4 in FLAME is the statue puzzle: trigger 27 (segment 279
side 5) runs it, it checks the eight spider pieces of the triggers 10 to 16
and opens the wall of segment 278. Script 322 of the Flamesword (trigger 28,
segment 486) ends the game after the death of Lolth. Without these two
script numbers the last level cannot be finished.

The port: `level_fixes_tables` and `level_fixes_objects` in dtuobj_setup.c, called
by `dtu_objects_level_setup`. The table part runs before the object setup,
the object part after it (setup_creature is the object setup of the load in
the executable) and before the creature setup by name; the object part is
not in the setup of a restore (`dtu_objects_level_setup_restore`). The
saved-state test of KOBOLDS also guards the weapon of the type: with a state
the creature setup gives the weapon -33 of the table the weapon 33 and 1d4
(checked with the decompilation, recovered/level_fixes.md). A restore keeps
the type entries of the setup of the level file (dtuobj.c `Setup_ext`), as
the executable reads no type table from the state.

DROW5 object 83 (the hatchling): the executable reads the hit points of the
type with the object number as the index (Robot_info[83] + 0x78, 0x393232,
past the table: a word that is 0 in each run, recovered/level_fixes.md), so
the hatchling starts with 0 hit points. `Dtu_fix_drow5_shields` (on;
`-origobj` and `-origdrow5` clear it) gives it the 100 hit points of its type.

DROW2, Tamaramagil Swiftclaw (**V**, checked in the files). 1.3 has the adult
red dragon in its data but not in the level. The creature type `Dragon`
(type 1, `dragn2.pof` of `DROW.EGG`, 120 hit points) is in the tables of the
five drow levels. Record 210 of the script file is her dialogue. Message 17
of `DROW2.SAY` (trigger 23, the entrance of the arena) is "An adult red dragon
gazes at you with surprise in its eyes. It does not attack", message 19
(trigger 25, segment 790, the east room) is "Uh oh, baby dragons", message 16
is the sign "Take care when feeding Tam". Trigger 20 (the lever) has
`object_num` 1 in the level file, and object 1 is a barrel. No level file has
an object of the type or an object with script 210, and no code makes one.
The four hatchlings (objects 48, 49, 61, 63) stand in the arena. Trigger 27
(the door of the hoard, segment 448) takes item 42 of class 16, the Dragon
Key: no object of the game has it and no script gives it.

The port has an encounter with her: `engine/main/dtudragon.c`,
[dragon.md](dragon.md). `Dtu_fix_tamaramagil` (on; `-origobj` and
`-origtamaramagil` clear it) and the setting "The dragon Tamaramagil" of the
Improvements menu turn it on. With one of them off the level has the objects
of the level file.

**Object flag 0x40** (verified, code). The object loop of `fvi_sub` skips an object with flag 2 or flag 0x40
(0x3F380 to 0x3F392: `test dh,2`, `test dh,0x40`), before any shape test
and for each query. The player move, the creature move, the shot flight
and the use ray (0xB30FD) are queries, and a pick up is a hit of the player
move (`collide_two_objects` 0x29455 with the hit object 0x30EF70). So the
player walks through an object with the flag and cannot take or use it,
creatures do not grab it and shots pass it. No draw code reads the flag:
the object is drawn as before. For a creature the flag also stops the AI
frame (0x13C8B, when `regeneration` is below 1) and the creature move
(0x9F174). The list of the readers is in
`DTU-Decomp/recovered/object_flag_40.md`.

- MULHOR1: six of the seven Ankhs (MISC 14) get the flag. Object 33
  (segment 585) is the only one that can be taken. No code chooses one at
  random. docs/re/quests.md, section 6.
- FLAME: the altar (object 2) and the Flamesword (object 3) get the flag.
  The death of object 36 (Lolth) in level 0x20 calls 0x6A374 from
  `remove_level_object` (0x6A579 to 0x6A59B): object 3 loses flag 0x40 and
  +0x1AE bit 2, each creature gets control type 0, and 0xAC055(object 3, 0)
  runs script 322, which ends the game.

The port: `OF_DTU_NOHIT` (object.h). The object loop of
`find_vector_intersection` and `dtu_body_query_test` (the use ray) skip an
object with it that is not a creature. `fix_nohit` sets it in the level
fixes, `flame_lolth_dead` (dtuobj_combat.c, called at the corpse of object 36) is
0x6A374. Tests `mulhor1_ankh`, `mulhor1_ankh_flagged`,
`flame_sword_lolth_alive`, `flame_lolth_death`, `flame_ending`
(tools/questtest.py).

Flag 0x40 of a creature (2026-11-23, verified in the bytes): 0xAD4F2 tests
that FLAME object 35 is a creature and 0xAD4FB sets the flag. Object 35 is
rffk9, the image of Lolth that script 320 moves and that speaks. The AI
frame (0x13C8B) and the move (0x9F174) skip a creature with the flag
unless its type has +0x7E (regeneration) of 1 or more or is the rat type,
and the query (0x3F38F) skips it: the image stands still and cannot be
hit or used. The port: `dtu_creature_frozen` (dtuobj_combat.c) and `dtu_nohit`
(fvi.c), test `flame_lolth_image`. The 22 creature records of BONES,
CLAN1, MINE and THIEF0 with the flag in the level file are all rats, which
pass_thru covers. +0x1AE value 2 is set only on FLAME objects 2 and 3
(0xAD3DF, 0xAD45A; MULHOR1 0xAD0C0 to 0xAD124 writes only flag 0x40). Its
readers (0x29966, 0x6CE53) test a moving object, and the altar and the
sword do not move, so it has no effect for players; 0x6A374 clears it.

Not in the port: +0x144
bit 0 of YPINN object 41 (a piece of scenery),
the texture of BANE objects 26 and 27 (the executable takes one of four
texture numbers that the previous level start looked up: `lava02`, `lava`,
`lavaflo`, `magma`, 0xAD744, so the value depends on the level before),
+0x1A8 (the port has one spell timer, +0x1C0), and control type 0 of MYRKUL
object 3 (the damage and AI code of the port test CT_AI; the hold with no end
keeps it in place).

Test: `-level FLAME -testpos 0,-7.92,452.5,0 -testmove wait:20,use:5,wait:100
-scriptlog -objlog FILE`: the object log has "level fix (0xACCDB), level 32:
obj 4 script 323", and the use of the wall runs "script 323 of obj 4
(Assembled Spider) in mode 3", which lists the pieces that are not in place.
Before the fix the trigger found no script ("trigger: obj 4 has no script").

## Hit points

- Creature from a level file: `shields` of the file. No dice are thrown and the table
  is not read (Code: no store to 0x6C in the load path, but for the dead test and for
  level 0). In the files, `shields` equals `max_hit_points` of the table line in 714 of
  1110 creature records and differs in 380 (Data). The files were saved with older
  tables.
- Dressing: 1.0 after the load. The file has the size of the object as `shields` in
  966 of 967 records (Data).
- Item (types 5, 15, 16, 17): the file value. It is 1.0 in all 485 records (Data).
- Gold that a dying creature drops (0x6A639, log text "make gold"): a misc object with
  id 1, render type 8, bitmap number of `Misc_info[1]`, and the amount as `shields`.
  `lifeleft` is 300 minus the amount (as fix). The amount is random, at most
  `moola` / 4 of the creature type.

## Size

| Type | Size after the load | Source |
|---|---|---|
| creature with a model | radius of the model of the table entry | Code 0x4785D |
| dressing | file value | Code |
| item | file value. It equals `size=` of the table line, or 5.0 with no `size=`, in 485 of 485 records | Code, Data |
| item made at run time | `size` of the table entry | Code 0x3C55C, 0x3C663, 0x3C700 |

The dressing size of the file is not `size=` of the table line (29 of 967 are equal).
Guess: the editor stored the radius of the model.

## Items in the world: render type 8

`render_object` is at 0x67C5A (`object.c`). It jumps by render type (table at
0x67C36):

| Render type | Code |
|---|---|
| 0 none | nothing |
| 1 polyobj | 0x6746A |
| 2 fireball | 0x3C081 |
| 3 laser | if `Weapon_info[id]` render type is 1: 0x671D6 |
| 4, 5 | error "Unknown render_type <%d,%d>". Hostage and powerup rendering are gone |
| 6 morph | 0x5F859 |
| 7 vclip | type 5: 0x7C6AC. Type 18: 0x7C734. Other types: error "Unrecognized object type %d with render type VCLIP" |
| 8 item | see below |

Render type 8 (0x67E2D):

1. Assert `obj->bitmap_num != -1` (the s16 at 0x162).
2. `bitmap = ObjBitmaps[obj->bitmap_num]` (the int at 0x18F19C + 4 * number).
3. Call 0x67273 (object, bitmap, 1).

0x67273 is `draw_object_tmap_rod` of Descent 1 (`object.c`) with a new light value:

1. If the bitmap is paged out (flag 0x10 at bitmap + 9): page it in (0x6E5BF).
2. `delta` = `orient.uvec` (0x2C) times `size`. Top point = `pos + delta`, bottom point
   = `pos - delta`. Both are rotated to view space.
3. Light = `static_light` of the segment (segment + 0x200) / 2, plus the mean of the
   int table at 0x1F0254 over the 8 vertices of the segment. Guess: that table is the
   dynamic light of the vertices. The result of the Descent 1 light function
   (0x57D60) is not used.
4. 0x670D4 (object) can set two draw state bytes (0x14C4CD = 0x40, 0x14C4D0 = 15).
   Not analyzed.
5. `g3_draw_rod_tmap` (0xEA982) with bottom width `size` and top width `size`.

So an item is a flat picture that stands on the up vector of the object and turns to
the viewer around that axis. Its height is 2 * `size` and its width is 2 * `size`. The
bitmap is stretched to that square. With the default size 5.0 an item is 10 by 10
units.

The render type 8 value of the file (lvl.md: "the table that it indexes was not
identified") is a bitmap number of the table that the editor had. The game replaces
it. Data: the file value is the same for each (type, id) of a level (381 of 381) and
grows with the id (259 of 273), and the step from one misc or armor id to the next is
the number of `picture=` lines between them in 126 of 168 cases. The rest fits older
tables.

## Pick up

Pick up is a collision. `collide_two_objects` is at 0x29455 (`collide.c`). It looks up
the pair of types in a table of 16-bit keys (0x292D7) and jumps (table 0x29355).
Pairs with new types:

| Pair | Function |
|---|---|
| player and weapon | 0x284E0 |
| player and armor | 0x283DC |
| player and misc | 0x2805B |
| player and spell | 0x28273 |
| player and dressing | 0x25FB0 |
| creature and armor, misc, spell | 0x28FC9 (log text "%d grabbing item": creatures take items too) |
| weapon and dressing | 0x2641C |

No handler runs if the first object has flag 2 (dead).

The four player handlers do the same steps (Code, misc at 0x280D7 to 0x2826B):

1. `count` = the signed byte at 0x1D5.
2. If `count` is 0: if the type is stackable (0xB61B2 tests these masks: `Misc_info`
   0x22 with 1, `Armor_info` 0x10 with 0x10, `Weapon_info` 0x3E with 2, `Spell_info`
   0x36 with 2) then
   `count` = 1, else `count` = `uses` of the table entry (misc 0x10, weapon 0x2E, spell
   0x28). The armor handler uses 1.
3. If `count` is 0 the item is not taken. Misc id 102 is the exception: it is taken
   with any value.
4. 0x31A0E must return 1. It compares a clock (0xDFAE0) with a time in the player
   struct (0x1D4DD9) and is a delay between two pick ups.
5. A weapon is an item only if it does not move (velocity 0 or movement type 0), the
   player is the console player, and `lifeleft` is 0x3FFFFFFF (0x28502, 0x2857C).
6. Inventory insert: 0xB65BB(type, table icon, table flags, table class mask, and on
   the stack: id, count, level number, a letter, table `modifier`, table `plot_item`).
   The letter for misc is `_`, or `B` for a `bow` and `C` for a `crossbow`.
7. If the insert works: object flag 2 (dead), 0x6A3CC(objnum), bit 4 at 0x1AE, a
   message made of the text pointer at 0x396A94 and the object name (0xB8C0F), and
   0xB772D(type, id, flag word).
8. If not: a message with another text (0x396A90 to 0x396AA0), and the object number
   is put in the list at 0x30F10C (count 0x30F49C).

Gold is misc id 1 (0x2806B): the message is "You pick up %d gold.", the amount is
`shields` as an integer, and it is added to the int at 0x1D33F7 + Player_num * 6967.
No level file has a gold object (Data). The gold path comes before the pick up delay
0x31A0E, and it calls 0xB772D(0x10, 1, 0) (0x28089 to 0x28092).

The gold of a dead humanoid (drop_gold 0x6A639, V): object_create_explosion
(0x3BD4C) with the size 0x38000 (3.5, at 0x6A6AC) and vclip 10, then render
type 8, type 0x10, id 1, the bitmap of misc 1, `shields` the amount, `lifeleft`
|300 - amount| seconds, and 0x6B9B0 puts it on the floor. The move query of the
player touches an item with `size + rad` (0x3ED76). The port gave the gold the
size of the misc table (2) until 2026-11-25, and a body over the gold on the floor
missed it by a few hundredths (KOBOLDS). The `lifeleft` is not ported.

### The pick up sound (0xB772D)

Verified in DTUGAME.EXE. 0xB772D(type, id, flags) is the only sound of a pick up:

- If bit 0x10 of `flags` is set: `digi_play_sample(2216, 0.5)` (0x2D955(0x8A8,
  0x8000), 0xB7732).
- Then always `digi_play_sample(2215, 0.5)` (0x2D955(0x8A7, 0x8000), 0xB7741).

The table of YPINN has `$SOUND 2215 2215 pugenr.rca` and `$SOUND 2216 2216
pumagic.rca` (Data). Port test (YPINN, `-testpos 45,2,90,0 -testmove
wait:20,forward:40 -objlog`): the Potion of Healing gives the log line "pickup: obj 4
type 16 id 79 'Potion of Healing' ... sound 2216 2215".

The flag word of each caller (the int at the address shifted right by 16, so the
s16 at +2):

| Caller | Type | Flag word | Bit 0x10 |
|---|---|---|---|
| 0x28092 gold | misc | 0 | never |
| 0x281CC misc | 16 | `Misc_info` 0x22 (0x19BADC >> 16) | set by the fixup 0x1A344, see below |
| 0x28387 spell | 17 | `Spell_info` 0x36 (word 0x1B56AC) | `cast_with_arrow` |
| 0x284CC armor | 15 | `Armor_info` 0x10 (0x19FD32 >> 16) | `stackable` |
| 0x2866A weapon | 5 | `Weapon_info` 0x3E (0x1E9D44 >> 16) | `magic`, also set by 0x1A344 |

The table fixup 0x1A344 (called at the end of the table read, 0x1C8F9) sets bit
0x10 of the misc flags for each misc type with `plot_item` not 0, a `+` in the name
(0xDABAE) or one of the name parts `uring`, `yrkul`, `rystal`, `carab`, `speed`,
`nsight`, `anegem`, `ursed`, `lamesword`, `otion` (0xDDA28, case sensitive), and bit
0x20 for a name with ` Key`. For weapons (0x1AEF8) it sets bit 0x10 (`magic`) for
`plot_item` not 0 or a `+` in the name. So a magic item plays 2216 and 2215, any
other item and gold only 2215. For armor the bit is `stackable` (`magic` is bit 1),
probably a mistake of the code; the port keeps it.

The spell flag byte 0x36 has the cast field in bits 2 to 4: `cast_with_hands` clears
it (0xE3 mask), `cast_with_wand` 4, `cast_with_scroll` 8, `cast_with_arrow` 0x10
(0x253A8 to 0x2545D), so the last `cast_with_` word of a line decides.

Port: `pickup_sound()` and `pickup_flag_10()` in `dtuobj_item.c`. Setup 1.0 of the port
played no sound for a pick up.

### Weapon items and the parent test of the port

A weapon item on the floor is an object of type 5 (`OBJ_WEAPON`), the type of a
shot. The object test of `find_vector_intersection` skips an object that
`laser_are_related` calls a child of the mover: a weapon whose parent number is
the mover and whose parent signature is the signature of the mover. A weapon
item of a level file has control type 0, so its parent number and signature are
0 (`-objlog` prints them as `parent 0/0`). The level loader gives object 0 the
signature 0, and object 0 is the player of the level file (Data: `kobolds.lvl`).
So the port never let the player touch a weapon item on the floor, with and
without Improved physics, and a wand or a sword could not be picked up. Whether
the executable has the same test with the same fields was not read (G: it picks
weapons up, help text 761).

Port (P): `laser_are_related` returns 0 when one of the two objects is a weapon
item (`dtu_weapon_is_item`: render type 8 and `lifeleft` 0x3FFFFFFF, the test of
0x2857C). Test: the save game of the user with the `Wand of Magic Missiles`
(obj 69, weapon 55, 46 charges) in front of the player in `kobolds.lvl`, load
with `-fescript`, `-testmove idle:30,forward:90`: before, no pick up; after,
"pickup: obj 69 type 5 id 55 'Wand of Magic Missiles' count 46, inventory entry
11", with and without `-noimprovements`. A save in the level and a load keep the
wand in the list (12 entries) and the level state has one weapon object less.

The name in the message is the weapon name of 0xB8C0F (table 0x1B45AC). The
reader of `$WEAPON` writes `weapon_name=` and `name=` to that table (0x20AD7 to
0x20B56, the last one wins, V). The port read only `name=`, so the message of a
wand was "You pick up ." The port reads both now.

### The byte at 0x1D5

The loader fills it from the low byte of 0xAC of the file (0xAC is the timer of the
Fear effect for a creature, so the editor used the same field for two things).

- It is the count of a stack or the charges of an item. 0 means the default of step 2.
- In the level files it is not 0 for 10 weapon records, all wands (10 to 56 charges),
  and for 26 misc records: 24 with id 102 (`Scroll`), one `Ruldegost Document` and one
  `Scroll of the Illithids` (Data). The 24 scrolls have 24 different values from 2
  to 27. Guess: for a scroll the value is the number of its text.
- The two plot items have the number of a scroll text too (Data: the text of
  scroll number n is FETXTE entry 13 + n - 2 in the high resolution, 0x9CEA3).
  DROW3 object 129, the `Scroll of the Illithids` (misc 126), has 18: FETXTE 29,
  the letter of Kauzelhoon about the Pool of the Elders. DROW5 object 176, the
  `Ruldegost Document` (misc 104), has 21: FETXTE 32, the letter to Severa. Only
  misc 102 and misc 127 show a text on use (0x9DC42), so 1.3 never shows these.
  The pick up 0x2805B reads the byte as the count for every misc id but 1
  (gold), so 1.3 gives 18 and 21 copies (`(byte) 0x1D5`, the `>> 0x18` of the
  dword at 0x1D2), and the inventory shows the count. The scripts that take the
  two items (Sangalor, YPINN script 6, label 17; Mirt, YPINN script 1, label 16)
  take one and set a local flag (flag5) that ends the turn-in, so the other
  copies stay in the bag and give nothing.
  Port (P): `Dtu_fix_plot_count` (on; `-origobj` and `-origplotcount` clear it)
  in `pick_up_item` (`dtuobj_item.c`): a misc item with `plot_item` from a
  record with a count above 1 gives one copy (misc 102 keeps its count). Test:
  `questtest.py --only drow3_illithid_scroll` (DROW3), and a pick up of DROW5
  object 176 (log line "count 21 of the level file -> 1").
- The lone mind flayer of DROW3 (object 128, type `flayer`, segment 712) stands
  next to object 129, and message 16 (triggers 27 and 28, segments 711 and 713)
  says "You come upon a lone mind flayer reading a scroll. Upon seeing you he
  drops the scroll and attacks." (Data). New in the port, a request of the
  user: `Dtu_fix_illithid_scroll` (on; `-origobj` and `-origillithidscroll` clear
  it). At the first load of DROW3 (no saved state) object 128 contains misc 126,
  count 1, and object 129 is removed (`level_fixes_objects`, `dtuobj_setup.c`).
  The creature drops the scroll when it dies. Message 16 says "... Upon seeing
  you he hides the scroll and attacks." (`dtu_trig_say_text`, `dtutrig.c`;
  Spanish: "Te encuentras con un azotamentes solo que lee un pergamino. Al verte,
  lo esconde y te ataca."). A level with a saved state keeps its objects.
- `object_create_egg` (0x3C4BD) makes the items that a creature or dressing object
  contains. It creates the object with `obj_create` (0x68AC8: type, id, segment,
  position, then identity matrix 0x148B82, size of the table entry, control 0,
  movement 0, render 8), sets 0x1D5 = `contains_count` of the parent, sets the bitmap
  number from the table entry, and calls 0x6B9B0. For `contains_type` 2 it creates a
  creature (0x1374D).
- A weapon that comes to rest becomes an item in 0x2B5EB: render type 8, size =
  `Weapon_info[id]` 0x58, bitmap number = 0x1E, `shields` = 11000, movement and control
  type 0.

Contents in the level files (Data): creatures contain weapons (109 records), misc
items (21), armor (10), creatures (5) and spells (2). Dressing objects contain misc
items (7), creatures (4), weapons (3) and armor (2).

## The 124 byte block: spell effects

File offsets 81 to 204, memory 0x80 to 0xF8 and 0x1B0.

The function at 0x9002C runs the timers. For each effect it tests one bit of the flag
word at 0x88, compares one timer, clears the bit and prints "NAME expired." So each
name below is from a string of the executable (Code). The function at 0x8ED49 sets
the effects (strings like "%s %s Shielded for %d minutes.").

| Memory | File offset | Type | Meaning |
|---|---|---|---|
| 0x80 | 81 | s16 | counter of Stoneskin (bit 29) |
| 0x82 | 83 | s16 | hit points of Protection From Fire (bit 9) |
| 0x88 | 85 | int | effect flags, see below. Cleared by the loader |
| 0x8C | 89 | fix | set to the game time by `init_ai_object`. Guess: timer of bit 4 |
| 0x90 | 93 | fix | timer of Armor |
| 0x94 | 97 | fix | timer of Bless |
| 0x98 | 101 | fix | timer of Blinded |
| 0x9C | 105 | fix | timer of Chill Touch |
| 0xA0 | 109 | fix | timer of Confusion |
| 0xA4 | 113 | fix | timer of Detect Magic |
| 0xA8 | 117 | fix | timer of bit 7, no string |
| 0xAC | 121 | fix | timer of Fear. For an item: the count, moved to 0x1D5 |
| 0xB0 | 125 | fix | timer of Resist Fire |
| 0xB4 | 129 | fix | timer of Flying |
| 0xB8 | 133 | fix | timer of Free Action |
| 0xBC | 137 | fix | timer of Haste |
| 0xC0 | 141 | fix | timer of Hold Person |
| 0xC4 | 145 | fix | timer of Infravision |
| 0xC8 | 149 | fix | timer of Invisibility and Improved Invisibility |
| 0xCC | 153 | fix | timer of Invulnerability |
| 0xD0 | 157 | fix | timer of Jump |
| 0xD4 | 161 | fix | timer of Find Traps |
| 0xD8 | 165 | fix | timer of Levitate |
| 0xDC | 169 | fix | Guess: timer of Rary's Mnemonic Enhancer. The loader copies it to 0x88 |
| 0xE0 | 173 | fix | timer of Protection From Evil |
| 0xE4 | 177 | fix | timer of Shield |
| 0xE8 | 181 | fix | timer of Sleep |
| 0xEC | 185 | fix | timer of Slow |
| 0xF0 | 189 | fix | timer of Spider Climb |
| 0xF4 | 193 | fix | timer of Strength |
| 0xF8 | 197 | fix | timer of Magical Vestment |
| 0x1B0 | 201 | int | creature and script flags, see the next section |

Bits of the flag word at 0x88:

| Bit | Effect | Bit | Effect |
|---|---|---|---|
| 0 | Armor | 16, 17 | Invisibility, Improved Invisibility |
| 1 | Bless (the end changes `shields`) | 18 | Invulnerability |
| 2 | Blinded | 19 | Jump |
| 3 | Chill Touch | 20 | Find Traps |
| 4 | unknown | 21 | Levitate |
| 5 | Confusion | 22 | Rary's Mnemonic Enhancer (guess for the bit number) |
| 6 | Detect Magic | 23 | Protection From Evil |
| 7 | unknown, timer 0xA8 | 24 | unknown, no timer in 0x9002C |
| 8 | Fear | 25 | Shield |
| 9 | Protection From Fire | 26 | Sleep (the end also clears bit 2) |
| 10 | Resist Fire | 27 | Slow |
| 11 | Flying | 28 | Spider Climb |
| 12 | Free Action | 29 | Stoneskin |
| 13 | Haste | 30 | Strength |
| 14 | Hold Person | 31 | Magical Vestment |
| 15 | Infravision | | |

Values in the 34 level files (Data):

- 0x88 is 0x1000000 (bit 24) in 39 creature records: all are `Troll`, `armTroll`,
  `flayer` or `flaynpc`. Guess: bit 24 is regeneration. `creature_fixes` clears this
  bit for a `flayer` (0xAC9AA). The value is 0x8000 (Infravision) in two player
  records. The loader clears all of them (see `read_object`).
- 0x8C is not 0 in 5 creature records, 0xC4 and 0xCC are 0x3FFFFFFF in one player
  record.
- 0xAC is not 0 only for the 36 items named above.
- All other values but 0x1B0 are 0 in all 2562 records.

`write_object` (0x48478) writes the block in the same order, and for an item it writes
the byte at 0x1D5 (0x48499).

## Monsters and NPCs: the int at 0x1B0

There is one object type for both (2). The difference is in the int at 0x1B0 (file
offset 201, game data version 41 only).

| Bits | Meaning | Source |
|---|---|---|
| 0 to 8 (0x1FF) | script number. 0 is none | Code 0xAC3CF, 0xAD7AC |
| 9 (0x200) | not hostile | Code 0xAC693 |
| 22 (0x400000) | hostile. `add_npc` sets or clears it | Code 0xAC6DB |
| others | the effect code changes more bits (Confusion, Fear). Not analyzed | |

`add_npc` (0xAC431):

1. The NPC table is at 0x3D42CA, 7 bytes for each entry, at most 100 entries (count
   byte at 0x145520). More gives "RLH: I can't handle more than %d NPCs in a level".
   The object number is at entry + 2.
2. For an `undead` creature type it stores a turn level and looks for the type name in
   a list of nine name pointers at 0x145602.
3. Script number n: if n is not 0, not 0x37, not 0x3F, not 0x1FC to 0x1FF, and n / 10
   is not the level number, the script number is cleared. So a script number is level
   number * 10 + a digit. For n from 1 to 999 the script is loaded (0xAB9C9).
4. If the script check 0xA8C49(0xCD) says so, or bit 9 is set: the creature is not
   hostile. `ai_local` byte 0 = 0, bit 22 cleared, 0x1A2 = -1.
   Else it is hostile: `ai_local` byte 0 = 4, bit 22 set, 0x1A2 = 0.
5. The AI flag bytes 0x142 and 0x143 = 0. The int at 0x30E2D4 + 4 * objnum = the int
   at object + 0x18.

So every creature of the level is in the NPC table. An NPC in the sense of the game is
a creature with a script number or with bit 9. A dressing object or an item can also
have a script number, and then it is in the table too.

Values in the level files (Data): of 1110 creature records, 827 have 0x400000 with
bit 9 clear, 51 have 0x400200, 3 have bit 9 only and 229 have neither bit. The player
record has -1. Of the other objects 219 have a value, always below 0x400 (one dressing
record has bit 9).

## Rats (Code)

Checked in the raw bytes of DTUGAME.EXE 1.3 (objdump of the image) and in the
tables of all levels (`tools/egg.py bin`).

- The only monster with `pass_thru` in the tables is "Rat" (`RAT_S.POF`,
  `regeneration=0`, `max_hit_points=4`). Most tables also give it `rat=1`; the
  tables of TRAIN1, PRISON1 and MINES1 do not. "GiantRat" has no
  `pass_thru`.
- The table parser (0x1DDE6) sets bit 4 of +0xD9 for the word `rat`
  (string at 0x1218EE) and keeps the entry index in 0x13A820.
- fix_object_segs (0x69FFB, at 0x6A102) sets object flag 0x40 of each creature
  whose table byte (`pass_thru`) is not 0. It runs at each level load and
  restore (callers 0x41F4A, 0x489EE, 0x49F36, 0xA97B3). The rat records of the
  level files also have the flag (BONES 12, CLAN1 3, MINE 4, THIEF0 3).
- fvi_sub skips each object with flag 0x40 (0x3F38F). So the move of the
  player, the swing (0x6CFF8 calls `find_vector_intersection`), the shots and
  the use ray never touch a rat: the player walks through it and no weapon or
  missile hits it.
- The AI frame (0x13C8B) and the move (0x9F174) of a flagged creature run only
  for the type in 0x13A820, so rats with `rat=` live and move; rats without it
  stand still.
- The move query of a rat sees the player. A start point in the body of the
  player is a hit (0x3EE70), so a rat that the player walks over touches the
  player on its next move. collide_robot_and_player (0x26FE0): with the `rat`
  bit and a boot item worn (byte P+0x1AEC not negative), vclip 9 at the rat
  with its size (0x3BD4C), then 0x25ADF.
- 0x25ADF: the rat becomes hostile (+0x1B0 bit 22), inactive (+0x1CC = 0) and
  tries 100 random segments (`rand() % Highest_segment_index`). It takes the
  first one whose side 3 has `pad & 3` not 1 or 3 and texture number 0 or less,
  and whose center is in the segment (0x4BE0B). There it goes to the center
  (0x4AD9C, 0x13899), gets physics flag 2, `init_ai_object(n, 0x81, seg)`,
  animation state 1, activity 0 and object flags 0, so flag 0x40 is clear until
  the next load. With no segment it plays its die sound (0x2E900) and is
  removed (0x6A3CC). There is no experience and no corpse.
- level start (0x59C3C, 0x5A50C to 0x5A5CD) puts each rat at the center of its
  segment and counts the rats.

So in 1.3 a rat can be killed only by stepping on it with boots (and it comes
back somewhere else), or by an effect that does not use a query. The port:
`dtu_rat_type`, `rat_squash` and the contact test in
`dtu_creature_physics_sim` (dtuobj_move.c). The port tests the type for
`pass_thru`, not the object flag, so a squashed rat stays pass_thru. The
correction `Dtu_fix_rat_hit` (combat.md, C3) lets the swing, the missiles and
the target hint of the player reach a rat. `tools/check_reach.py` checks it.

## Covered and not covered

Covered: the tables and their entry sizes, the fields that give model, size, hit
points and bitmap, the id mapping, all steps of the load path for an object, the
render path of an item, the pick up path, the meaning of the 124 byte block.

Not covered:

- `$MONSTER_AI` and the fields 0x20 to 0x60 of `Robot_info` (see ai.md).
- The animation values 0x16E to 0x176 and the model loader (see models.md).
- Most fields of `Weapon_info` and `Spell_info`.
- The inventory (0xB65BB) and what a used item does.
- The handler for creatures that take items (0x28FC9).
- 0x6B9B0, called after an object is created or moved. Guess: it puts the object on
  the floor.
- The scripts (0xAB9C9) and the per level fixes (0xACCDB).
- The object struct from 0x1A2 to 0x1DE, but for the fields in the table above.
- Bits 4, 7 and 24 of the effect flags and the other bits of 0x1B0.
- The value at object 0x10 and 0x12.

## How to check

```
python -I tools/objcheck.py gamedata/unpacked        (add -v for the failed cases)
```

Result for the 34 level files that have a table (2562 objects with a table class):

| Check | Passed |
|---|---|
| id below the number of table lines | 2562 / 2562 |
| creature: file model name is `model=` of `$MONSTER` line id | 1054 / 1110 |
| dressing: file model name is `pof=` of `$DRESSING` line id | 926 / 938 |
| dressing: render type of the file fits the table line | 967 / 967 |
| render 8: one value for each (type, id) in a level | 381 / 381 |
| render 8: value grows with id inside a type | 259 / 273 |
| misc, armor: value step is the count of `picture=` lines | 126 / 168 |
| item: file size is `size=` of the table line (default 5) | 485 / 485 |
| dressing: file shields is the file size | 966 / 967 |

The model name checks fail where the file has an older name of the same thing
(`armogr.pof` for `armogre.pof`, `chest.pof` for `chest5.pof`, `rat_l.pof` for
`rat_l2.pof`). The game takes the model of the table, so these objects load with the
newer model.

## Corrections and additions from the port of the objects

Read in the code with the work on `engine/main/dtuobj.c`.

- 0x6A3CC is the end of a creature (called at the end of the DIE animation, 0x11373, and
  for an object that is picked up). For a creature (type 2): control type 11 becomes 0.
  A HUMANOID creature drops gold (0x6A639). `shields` = 0. Alert function with level 4.
  The body is put on the floor (0x6C30E). Object flags 0xC0 are set. With the
  regeneration status bit (0x01000000) GOAL_STATE becomes SRCH and the function ends.
  Else: status flags 0, `lifeleft` 0, the object number goes to the list at 0x2AEF54
  (count 0x13D334, limit 800), object flags 6 are set, control type and movement type 0.
  What takes the corpse away later was not read. The height of the corpse: ai.md,
  "The corpse" (the port: `Dtu_fix_corpse_floor`).
- `collide_two_objects` (0x29455) builds the key (type A * 256 + type B - 0x101), looks
  for it in the list of 64 words at 0x292D7 with `repne scasw` and jumps through the table
  at 0x29355 with the count that is left. Player and creature: 0x26FC1 in both orders.
- 0x6B70D (object, vector) is the "bump": for an object with movement type 1 and a mass
  above 0 it adds the vector divided by the mass to the thrust.
- 0x2AF5E (damage dice): a natural weapon of a creature as in ai.md. For a weapon type of
  0 or more the dice are the bytes at `Weapon_info` + 0x0A (sides) and + 0x0B (count).
  The player with no weapon uses the words at 0x1D33C1 (sides), 0x1D33C3 (count) and
  0x1D33C7 (bonus) of the player structure. The result is a fix value.
- The speed values that count are those of the model (models.md, 0x74AA7 reads the model
  at +0xC0), not the copy at object 0x17A. (data) `durnan.pof` has 15.0, 6.0, 30.0, the
  values of `speeds=` of its table line, not two times these values.

### The port

`dtu_objects_level_setup` does the steps of `verify_object`, `setup_object`,
`place_creatures` and `fix_object_segs` for the types 2, 5, 15, 16, 17 and 18 after
`level_resolve_models`. Differences:

- The bitmap number of an item is a slot of `ObjBitmapPtrs` in the port (the number of
  the executable is an index of `ObjBitmaps`). The picture is the same.
- A creature that is in no segment is moved by `fix_object_segs` of Descent 1, not by
  `place_creatures`.
- `creature_fixes` of `level_start` is not in the port. The fixes for single levels
  (0xACCDB) are, with the exceptions in "Level fixes (0xACCDB)". A creature with a
  script number or with bit 9 of 0x1B0 is not hostile.
- Collision bodies: the port tests a moving object against the cylinder of 0x748D9 in
  the x-z plane and then compares the heights (`dtu_check_vector_to_object` in `fvi.c`).
  The test of the executable inside 0x3EFD9 (0x3EE70) is in physics.md section 3; the
  port uses it for the swing of the player and for a missile against a creature
  (`dtu_body_query_test`). The query of a missile (0xA05AD) takes its radii from
  0x748D9 for the missile: the horizontal radius (edx, query +0x14) is 0x800 for a
  weapon model with the body box and the size for other missiles; the vertical radius
  (ebx, query +0x18) is the half height of the body box or the size
  (`dtu_query_radii`). The ray against the capped cylinder lets a steep shot down hit
  the top of a low body; the older test of the port took the height at the edge of
  the body and missed it.
- Pickup: the four player handlers are one function. The inventory is the list
  `Dtu_inventory[]` of `dtuobj.h`, not the structure of 0xB65BB. The list of items
  that did not fit (0x30F10C) is not in the port.

## Dynamic light

Verified in DTUGAME.EXE. `set_dynamic_light` (start before 0x57873, end
0x57C38) builds the vertex list of the render list, clears one half of it in
each frame (`FrameCount & 1`, 0x578EF) and calls the muzzle flash light
(0x5777E). Then for each segment of the render list:

1. Each side with a light texture (0x57963 to 0x57A76): the center of the
   side (0x4AD3E) plus a random offset in each axis (`rand() % 0x8000`), the
   light `table[(segment + side) & 7] & 0x1FFFF` (table 0x13CCC0, changed at
   0x5783C), positive to 0x57642, negative to 0x574FE. Not in the port.
2. Each object of the segment (0x57A80): type 1 fireball as in Descent 1;
   type 5 weapon `Weapon_info[id].light` (0x1E9DA8, written from `lightcast=`
   at 0x214BD), skipped when bit 4 of byte 0x1AE is set; for id 1 it adds
   `2 * (min(light, lifeleft) + ((GameTime ^ xlate[objnum & 15]) & 0x3FFF))`;
   type 8 debris 0x4000; type 13 light the intensity at 0x140; type 18
   dressing the `lightcast=` value (0x19B34C + 32 * id, written at 0x22918).
   Robots (the creatures) and powerups cast no light.

`apply_light` (0x57642): a source with `intensity * 64 <= 8.0` lights the 8
vertices of its own segment in each frame, a brighter one the vertices of the
list with the parity of the frame. Both use `d = (dist / 4)^2` (Descent 1
uses the plain distance for the bright sources), skip `d >= intensity * 64`,
and add `intensity / max(d, 4.0)`.

The player light (0x57C39, called from the face draw at 0x7550B) is the
Descent 1 `compute_headlight_light`: `Beam_brightness` 0.5 (0x13CC78),
`Face_reflectivity` 0.5 (0x13D640), `use_beam` 0 (0x1F3AB8). The face term is
the dot product of the view direction and the face normal, so the light of a
wall depends on the view direction. `Dtu_fix_light_stable` (lighting.h)
changes that in the port.

## Pushed scenery

### The rule of the executable (Code)

The push itself is in ai.md, "Push of scenery (0x9FA41)": the player (0x9E5BE)
or a creature (0x9FA41) whose strength is above `pushable` sets the velocity and
the thrust x and z of the scenery to 8 times its own and the movement type to
1. The player goes back to the start of the try and the next try hits the
scenery again (up to 8 tries in a frame).

0x69B4A (`object_move_one`) calls 0x9FE68 in each frame for a type 0x12 object
with movement type 1. A polygon object with movement type 0 gets the same call
in turn: the pointer 0x13D41F goes up by one at each call of `object_move_one`
(0x69830) and skips objects with a movement type (up to 20), so one resting
object a call is set down on the floor.

0x9FE68, in each frame:

1. Returns for the model number at 0x13A85F and for object flags 0x47.
2. `velocity *= 0.5` (0xD6F25 with 0x8000), movement type 1, then
   `vel.x >>= 2`, `vel.z >>= 2`: x and z go down by 8 times in each frame.
3. The step count of the frame (FrameTime >> 10, at most 2) and the rest.
4. With the fall bit 0x20 (+0x1AE): gravity 0x6C662, then `vel.x >>= 3`,
   `vel.z >>= 3`. The fall bit is set when the floor is farther than 6.0
   (0x6C30E). Thrust and drag 0x6CB68 with the thrust as the acceleration (0
   after a push by the player, whose thrust is 0 at that time).
5. The body 0x748D9 of the scenery: `rad` = `body_radius[0]`, then halved
   (0x9FFD6); `rad2` = `body_half_height[0]`. Query flags 9 (objects), the
   ignore list is empty.
6. Up to 5 tries (0xA00C7): the move is `vel.x * FrameTime`, `vel.y` (not
   scaled), `vel.z * FrameTime`; x and z are each doubled in 1 of 7 tries
   (`rand() % 7 == 2`, 0xA007F). Result 0: the end point, done. Result 1: with
   0x13D448 & 0xA the velocity is negated, else 0x9E88D (the velocity into the
   wall comes back 0.75 times, the position goes back), movement type 0.
   Results 2 and 4 (an object): the position goes back; an object of type
   0x12 gets 8 times the velocity and the thrust of this one and movement
   type 1, with no test of `pushable` (0xA02A8); movement type 0. Result 3:
   movement type 0. Result 5: gravity. After 5 tries: the start position and
   velocity 0.
7. Sound 2706 while the speed is above 0x70, 0x69FB8.

### Why a chair does not move at a high frame rate

The object test of the query (0x3EE70 with 0x4F7D5, physics.md section 3)
takes a start point that is inside of the body of an object as a hit. The body
of the player there is model 16 with `body_radius[0]` (7.19 for `hmcr_p.pof`,
twice the radius of the move query of the player), so the move of a chair
(`body_radius[0]` 3.64, query radius 1.82) stops when its center is closer
than 7.19 + 1.82 = 9.01 to the player. The player stops at the chair when its
move query (radius 3.59, x and z of the move times 1.2) reaches the chair
(radius 3.64): at 7.23 plus the rest of the last step, 0.2 to 1.2 steps. So the
chair moves only when the player stops at least 1.78 short of the contact,
which needs a step of 1.48 or more (1.2 steps above 1.78): below 33 frames per
second at the walk speed limit (49.3 units a second), below 50 when the player
runs, and then only for a part of the approaches. Once it moves, the gap stays
and the push goes on; when it closes, the chair stays where it is. At 60
frames per second and more the chair never moves. A table (`body_radius[0]`
11.18) is never inside of the player (the player stops at 14.77, the test is
7.19 + 5.59 = 12.78), so a table moves at every frame rate.

The executable also tests the walls with the center of the scenery and half
of `body_radius[0]`: a wall stops the center at 1.82 + 1.0 (chair) or 5.59 +
1.0 (table), so a table (10 from the center to an end) goes 3.4 into a wall.

### The port

`dressing_move_frame`, `push_dressing` and the helpers above them in
`dtuobj.c`. Three corrections, on by default; `-origobj` sets them to 0 and
then the port has the rules of the executable above (also the start test in
the player, the half radius, and the doubled moves), except the wall bounce of
0x9E88D (a wall stops the move) and the y move (the port moves y by the
velocity times the frame time and has its own gravity to the floor).

- `Dtu_fix_push_body`: a body that the scenery starts in does not stop a move
  away from it. The player has the radius of its move query (3.59), so the
  player and the scenery stop at the same distance. Other scenery is the box
  of its model (the box of the drawn points of the rest pose, an upright
  object only; box 1 of the file is wrong for `chair.pof`): the corners
  and the mid points of the edges of each box are moved against the other box
  (`dressing_box_hit`), not the circles of `body_radius[0]` (a chair at a
  table is already inside of the circle of the table). Creatures are circles
  of `body_radius[0]`. Scenery that hits other scenery pushes it only when the
  strength of the first push is above `pushable` of the other one.
- `Dtu_fix_push_rate`: see below.
- `Dtu_fix_push_wall`: the corners and the mid points of the edges of the box
  of the model, at the height of the middle of the box, move with the
  scenery; the first wall that one of them hits stops the scenery 0.25 before
  it (`dressing_wall_frac`). A point that is outside of the mine at the start
  (scenery that the level puts into a wall) is not tested. A wall stops the
  scenery: no slide along it.

Before this, the port had moved the center with the radius `size / 2` and no
object test (the chair went through other scenery), and it slowed the scenery
down once in each reference frame (`Dtu_fix_push_frame`, removed): above 21
frames per second the scenery kept the speed of the player for some frames and
ran ahead, so the push grew with the frame rate. In the corner of segment 20
of YPINN the center of a chair stopped 0.2 from both walls, and the center of a
table at a wall stopped at the wall.

The gravity of the port (Improved physics off) takes scenery down until its
lowest drawn point is on the floor under its position. Before, it took the size
of the object (a radius) as the depth of the bottom: a pushed chair (size 7.69,
lowest point 8.59 below the position) went 0.9 into the floor, and a crate pushed
off a ledge stopped 3.66 above the floor (computed). 1.3 takes `body_half_height[0]` (0x6C30E,
the box of the file) and starts a fall only when the floor is farther than 6.0;
that rule is not in the port.

### Scenery in the floor (Dtu_fix_scenery_floor)

1.3 moves no scenery at the load of a level (a resting object falls only when its
floor is farther than 6.0, see above), so it draws scenery where the level file
puts it. Some is partly in the floor (`-objlog`, line "floor (level start)": the
lowest drawn point against the floor straight under the position):

| Level | Object | In the floor |
|---|---|---|
| YPINN | chair 18 | 2.59 |
| YPINN | chair 9 | 2.06 |
| YPINN | chairs 20, 26 | 0.99, 1.09 |
| YPINN | other chairs, tables, beds, barrels | 0.09 to 0.59 |
| THIEF1 | chairs 121, 123, benches 88 to 90, crate 73 | 0.25 to 0.51 |
| THIEF2 | table 81 | 1.10 |
| THIEF2 | benches 95, 96, barrels 77 to 79 | 0.25 to 0.46 |

Saved games of the port before this change can also have chairs 2.95 in the floor
(the shape of Improved physics above): THIEF1 chair 66 of a saved game of the
stream of 2026-11-23.

`Dtu_fix_scenery_floor` (1, the default): at the setup of a level, also of a loaded
level, each upright scenery object whose lowest drawn point is 1/16 to 4.0 below the
floor under its position goes up onto the floor (`dressing_floor_fix`, a line
"floor: obj N ... up onto it" in the `-objlog` file). Scenery higher than the floor
stays where it is: scenery on a table, a lantern on a wall. A barrel on its side is
not upright and stays. `-origobj` sets it to 0: the places of the level file.

With Improved physics on, the player does not push scenery with these rules:
scenery is a solid body with a top, and the player moves it directly
(physics.md section 13.2). The rules here stay for the creatures and for
Improved physics off. The sound 2706 is one linked sound for each object in
both cases (physics.md section 13.2).

### Frame rate

With the slow down in each frame the push is a cycle of two frames: the mover
stays where it is in the frame of the hit, the scenery moves the step of the
mover in the next frame and one eighth of it in the one after, and the mover
walks one step. Mover and scenery go on at one half of the walk speed, and all
distances of the cycle are in proportion to the step, so the cycle is the same
at each frame rate. The chosen behavior is that of the executable at about 21
frames per second (the reference frame of the port, 3/64 second), when the
push works: the corrected body test makes it work at each frame rate.

What is left is the end of the push: the scenery ends ahead of the mover by
the gap of the last hit and the moves after it, which are parts of a step:
about 4 units at 20 frames per second and 0.4 at 200. `Dtu_fix_push_rate`: when
the scenery stops (speed 0x70 or less), it slides
`DTU_PUSH_LEAD * v * (DTU_REF_FRAMETIME - FrameTime)` along the fastest push
of the row (the mover slows down at the end), with a velocity that goes down
by e in each reference frame time. `DTU_PUSH_LEAD` is 1.3, measured: the mean
lead of the scenery over the place where the player stopped, at 20 and 25
frames per second, was 2.5 units in 10 walks, and 2.2 needs to be added at 144
and 200 frames per second. (The expected value from the cycle, 0.7 + 8/7 = 1.84
steps, is too large because the player slows down at the end.)

### Measurements (Data)

YPINN, strength 17, `-fixedfps N`. The chair (obj 5, `pushable` 1) at -80,
-2, 44 with the table out of the way (`-testobjpos "3,-115,-6.5,30,0;5,-80,-2,44,0"`),
or the table (obj 3, `pushable` 16) at -80, -6.5, 52 with the chair out of the
way (`-testobjpos "3,-80,-6.5,52,0;5,-115,-2,40,0"`). The player at -80, -2.46,
33 looks along +z: `-testmove wait:300ms,forward:Wms,wait:3000ms`. Distance the
object moves, mean of the walks W = 400, 450, 500, 550 and 650 ms:

| Frames per second | 20 | 25 | 60 | 144 | 200 |
|---|---|---|---|---|---|
| Chair, before | 8.39 | 9.22 | 9.62 | 11.99 | 12.70 |
| Chair, after | 8.39 | 8.60 | 8.96 | 9.14 | 9.16 |
| Chair, `-origobj -origphys -noimprovements` | 0 | 0 | 0 | 0 | 0 |
| Table, before | 8.39 | 9.22 | 9.12 | 11.75 | 12.33 |
| Table, after | 8.39 | 8.60 | 8.55 | 8.90 | 8.97 |
| Table, `-origobj -origphys -noimprovements` | 6.87 | 9.97 | 8.15 | 7.04 | 7.38 |

The walk of 450 ms alone, chair after: 6.33, 7.33, 7.54, 7.79, 7.63. One walk at
20 or 25 frames per second has the phase of the last hit (0.2 to 1.2 steps of 2
units): single values at these rates go from 5.3 to 11.5 for the same mean.

The executable at a low frame rate, with its rules (`-origobj -origphys
-noimprovements`, the chair at its place 48 in front of the table, 16 starts
from z 30.0 to 33.0, walk 1.5 s): the chair moved in 5 of 16 approaches at 15
frames per second, 1 at 20, 2 at 25, 0 at 30 and 60; when it moved, it went 6 to
10 units and pushed the table (the push of other scenery has no strength
test). The random doubling of the moves makes single runs differ.

The original layout (chair 5 at 48 in front of table 3 at 59,
`-testpos -80,-2.46,33,0`, walk 0.6 s), at 20, 60, 144 and 200 frames per
second: before, the chair went into the table (chair 55.2, 56.1, 59.3, 59.9;
table 61.6, 63.6, 65.2, 65.9: the player pushed the table through the chair).
After, the chair stops at the box of the table and pushes it (strength 17 above
16): chair 53.7, 55.6, 56.2, 56.2; table 64.1, 63.4, 64.0, 63.9. With a walk of
1.5 s: chair 57.9, 57.9, 58.2 and table 65.7, 65.7, 66.0 at 20, 60 and 200
frames per second (the table stops at the creature obj 17).

Walls (positions from `-objlog`, screenshots): the wall at x -124.9 of
segment 8 and the corner with the wall at z 12.7 in segment 20.

| Test | Before | After (20, 60, 200 frames per second) |
|---|---|---|
| Table (heading 0, 10 from the center to the end) pushed along -x into the wall | center at x -124.0 to -124.9: half of the table in the wall | -114.75, -114.75, -114.99 |
| Chair pushed along -x and -z (heading 250) into the wall | center at x -124.3 to -125.0 | -122.27, -122.27, -122.43 |
| Chair pushed into the corner (heading 225) | center at -124.4, 12.6 (in both walls) | -121.6, 15.4 (-121.78, 15.22 at 200) |

Not verified: no run of DTUGAME.EXE (the numbers of the executable are those of
the port with `-origobj`); the y move and the wall bounce of 0x9FE68; whether the
player of the executable stops at a table where the port stops it (the move of
the player in the port finds the objects of the segments that the query goes
through, so a body in the next segment is found late, physics.c); creatures
that push scenery were not measured.

## Item effects (DTUGAME.EXE 1.3)

The effects of the items are code for single item numbers. The keywords
`pc_spell`, `pc_spell2`, `set_strength`, `duration` and `armor_class_decrement`
of the `$OBJECT` and `$ARMOR` lines are not read by these functions. The
details of each case are in combat.md B6 and B6a and character.md sections 5
to 8; the audit of each item id is [gear.md](../audit/gear.md). (Code)

- Use: 0x37A6A(index). A potion goes through 0xB7084 (use one); the Gem of
  Curing, the Spider Pieces (charges in the count), the Scroll, the thief
  scroll, the Orb of Insight and the Cursed Scroll are not used up.
- Before each use 0xB6FE3 cuts each stack of a stackable type to its
  `stack_limit` (and makes a count below 1 into 1).
- Worn items: 0xAFB55 after each change of the equipment. Armor class =
  10 + the sum of the table values (0x145C04 armor, 0x145A04 misc, 0x1458B8
  weapons; the table value is minus `armor_class_decrement` for the armor
  lines that have it), then the caps of Armor (6), Magical Vestment and
  Shield (3), Slow +4, the dexterity value (0x14019A), Invulnerability -2,
  limit -10 to 10. The boot and armor places read only armor; the necklace
  skips number 33; a Cloak of Protection counts only without shield, magic
  armor and heavy armor; no ring counts with magic armor; of two Rings of
  Protection the better counts.
- 0xAFAD1: with Ring 16 (Armoring) in a ring place, a mage gets 1 better
  armor class after each of the three caps.
- The effect bits of worn items (rings, boots, cloaks, Amulet of Shield,
  Glim-Gauntlet, Sword of W'hin in the hand) are set with no end by 0xB08A8
  and cleared by 0xB0A98 when the item goes.
- The Sword of W'hin in the hand sets Find/Remove Traps and Climb Walls to
  250 (95 after the limit) and gives 9 better armor class.
- Potion timers: 0x396B2 each frame. The four timers of intelligence, Hill
  Giant Strength, Dragon's Blood and Agility are saved as seconds left
  (0xBA87E) and loaded as now + seconds (0xBA911). The Potion of Luck sets
  global flag 77; its end and its use in a hit roll clear it.
- Drop: 0xB717B. No drop with 700 objects; no drop of a `plot_item`
  (0xB611C); weapon 13 refuses on a d18 above intelligence or a d7 above the
  class 1 level (flags 0x8A, 0x90, script 0x1FD). The item is made with
  0x3C93B (object_create_egg from the player). No pick up for 5 seconds.
- Stacks: 0xB65BB adds a picked up count to the stack of the same item and
  cuts it to `stack_limit`; a full stack takes nothing. The port takes what
  fits and leaves the rest (`Dtu_fix_partial_stack`).
- Wands: a weapon with `PC_spell` in the hand casts the spell with template 3
  and uses one charge (0x56A0D).
- There is no identification, no weight rule and no cursed item other than
  the Cursed Scroll.

## New gear (ASCENT, not in 1.3)

The setting "New gear" (`Dtu_new_gear`, on by default; `NewGear` of
ascent.cfg; `-newgear`, `-nonewgear`; `-noimprovements` and `-origitems`
clear it) adds items, wands and loot that DTUGAME.EXE 1.3 does not have. The
code is `engine/main/dtugear.c` (the tables) with small hooks in the item,
drop, missile and script code. With the setting off the tables, the drops,
the market goods and the object log of a run are those of 1.3 (with the
`Dtu_fix_*` corrections): four runs with a fixed seed (BAT with four kills,
KOBOLDS, DROW2 and THIEF1 with `-testkill`) give the same `-objlog` bytes as
the build before the change. The setting is read when the tables of a level
are loaded, so a change takes effect at the next level.

### The table lines

After the table fixup of 1.3 (`dtu_gear_tables`, called by
`bm_load_level_tables`) and only for the mission tables (53 `$ARMOR` lines,
128 `$OBJECT` lines and the five generic wands, checked for each table):

| Line | Item | Effect (code) | Value | Classes |
|---|---|---|---|---|
| armor 53 | Mirror Shield | Shield +1 (armor class 2 better); sends spell missiles back (`dtu_gear_mirror`) | 9000 | as Shield +1: fighter, thief, cleric |
| armor 54 | Boots of Free Action | Free Action while worn (boot slot bit 5) | 3500 | all |
| armor 55 | Gloves of the Archer | +1 to hit, +2 damage for arrows and bolts (`archer_bonus`) | 3500 | all |
| armor 56 | Cloak of Fire Resistance | Resist Fire while worn (cloak bit 1): fire damage halved | 3000 | all |
| armor 57 | Belt of Dwarvenkind | Constitution 18, Infravision for a human (belt bit 6) | 7500 | as the Girdle: fighter, thief, cleric |
| armor 58 | Ring of Regeneration | 1 hit point each 60 seconds of game time (`regen_frame`), the effect Regeneration in the effect list (worn bit 15) | 15000 | all |
| armor 59 | Plate Mail of Fire Resistance | armor class 1 as Plate Mail +2, magic and heavy armor, Resist Fire (armor slot bit 1) | 7500 | fighter, cleric |
| misc 128 | Bloodstone | gem | 50 | all |
| misc 129 | Pearl | gem | 100 | all |
| misc 130 | Star Sapphire | gem | 5000 | all |

The pictures and the icons are those of the items of the same kind, which
each mission archive has (checked with `tools/egg.py list` on the mission
archives: gensheld, genboots, genguant, gencloak, genbelt, genring,
genpmail, gengem, gengem2, gengem3). The gems: Bloodstone gengem2 with icon
131, Pearl gengem3 with icon 129, Star Sapphire gengem with icon 130. The
three gem pictures of the archives are the same red gem. 1.3 reads no item
effect of the belt or the armor slot (0xAFB55); the port reads both for
these numbers only.

Weapons 63 to 67 ("Wand of Genericness" 63 to 65, "Rod of Genericness" 66
and 67, with `enemy_spell=-1` and no spell) become the Wand of Sleep (46),
Wand of Slow (47), Wand of Hold Undead (23), Rod of Minute Meteors (37) and
Rod of Dispel Magic (13): `PC_spell`, the mage class, value 3000, 6
charges. The table values of the Mace of Disruption (70, 17500 in 1.3) and
the Flame Tongue (71, 4500) become 4500 and 6000.

The Rod of Kelemvor (Spanish "Vara de Kelemvor") is a new weapon line at the
end of the weapon table, after the Dagger of Venom (90 in the build of
2026-11-27; the code finds it by its name, `dtu_gear_rod_weapon`): a copy of
the Rod of Dispel Magic (67) with `uses=1`, value 50, `all_use` and no
`mage_use`. The two names differ on purpose: the Rod of Dispel Magic (mage
only, 6 charges, used up, sold at stage 7) and the Rod of Kelemvor (all
classes, one charge for each rest, never used up, not sold).

### The Rod of Kelemvor

A decision of the user of 2026-11-27: a rod of dispelling that any class
can use, so that a fighter or a thief can free Daland of MYRKUL and take
away the contingency of the lich of CLAN3 without a scroll in a hot key box.

- Each use casts Dispel Magic (13) at caster level 12 (`DTU_GEAR_ROD_LEVEL`;
  `dtu_gear_rod_fire` in the wand path of `dtu_spell_fire`, the level goes
  to the missile with `dtu_missile_set_level`). Dispel Magic on a creature
  ends all its effects with no save and no comparison of levels (0x8CFFF,
  the case of spells 13 and 55; `apply_spell` calls `fx_expire(target, 1)`,
  which also ends Mirror Image, the Minor Globe and the Spiritual Hammer of
  the port). So the hold with no end of Daland (level fix 0xAD129) and the
  contingency of Lich3 (caster level 12, `dtucaster.h`: Stone Skin, Shield,
  Minor Globe, Mirror Image) end at any level. The level counts for a
  potion on the floor (d20 against 23 - level: 11 or more at level 12) and
  for the range (spell 13: 360 feet at each level).
- One charge. The use sets global flag 250 of the player record
  (`DTU_GEAR_FLAG_ROD_SPENT`, saved with the game); a rest that fills the
  spell slots (`dtu_char_rest`, the rest places) clears it. A level start
  does not. The item keeps the count 1, so a drop, a pick up, a sale or a
  save do not charge it. With no charge the use shows "The Rod of Kelemvor
  has no power left. A rest will renew it." and casts nothing.
- The place: MYRKUL, the floor of the hidden crypts at the feet of Daland
  (109.5, 19, 579, segment 326, 11 units in front of him; he faces the way
  in from segment 17), `dtu_gear_place_rod` (dtuobj_setup.c), once in a game
  (global flag 249, `DTU_GEAR_FLAG_ROD`). His own rod, out of his reach:
  each player who finds him finds the means to free him, by any way through
  the temple. The other place that was looked at, the altar of Kelemvor of
  the living quarters (object 23, segment 308, message 11), is as far from
  the start as the crypts and a player can miss it.
- The rest places on the way: MYRKUL, CLAN1 and CLAN2 have none (segments
  with the special value 2 or 6); HUB3 has segment 96; CLAN3 has segment 273
  (center 75, 12.5, 405), 17 segments from the start and on the way to the
  lich (segment 443, 29 segments from the start). So the rod is found
  charged, frees Daland, and is charged again at the rest of HUB3 or of
  CLAN3 before the lich.
- With the rod in the inventory the talk to the held Daland says "Some magic
  seems to hold Daland completely paralyzed. Perhaps your Rod of Kelemvor
  can dispel it." (without it: "... Perhaps it can be dispelled.").
- A trap spell takes the template of the last weapon line of the table
  (`dtu_hook_trap_spawn_object`); the rod line, like the Spiritual Hammer line, is
  skipped there.

### Drops

`gear_drops` (dtuobj_combat.c) replaces the gold rule of 0x6A639 (a
HUMANOID type in 4 of 10 deaths, 1 to moola / 4):

- Gold: each type with a `moola` above 0, except the named people
  (Durnan, Mirt, Aleena and the other people of YPINN, Daland, Thashasa,
  Halaster, Ibbalar, Jeryth, the avatar), in 6 of 10 deaths: moola + 1d(2 x
  moola).
- Gems: an undead or intelligent type in 1 of 2 deaths, by the experience
  of the type: under 100 a Bloodstone; 100 to 499 a Pearl 3 times in 4, else
  a Ruby; 500 to 1999 a Ruby 2 times in 3, else a Diamond; 2000 and more a
  Diamond, 1 time in 10 a Star Sapphire. The gem lies 3.0 to the right of
  the body.

Intelligent (`dtu_gear_gem_class`): the door bit (door_opening=1 or
HUMANOID), a person model of Improved AI (`dtu_model_is_person`), or a
spell caster (spell_level, a negative weapon_type, the eye ray weapons 81
to 88). `iq` is the number of the sight and the morale (ai.md), not the
intelligence. The types of the level files by class (`tools/gear_income.py
--classes`):

- Undead: Banegard, Ghast, Ghoul, gskelpus, gskelsci, Lich3, Mummy,
  prstsprt, ske_axe, ske_bow, ske_sci, ske_swd, Zombie.
- Intelligent: armOgre, armTroll, Beholder, dmfw1, flayer, fMephit,
  Gargoyle, the goblins (gbloser, gbscsswd, gbscthxe, gobascim), the orcs
  (gorc_axe, gorcaaxe, gorcasci, gorcatax, Orc_axe, Orc_taxe, Orcascim,
  Orcataxe), hatchlng, the humans (hffg1, hmfg1, hmft4, hmft7, the shadow
  thieves sfmtd2 to shmtf6, shdleadf, shdleadg), the kobolds (kob_bow,
  kob_spel, kob_swrd, kobaswrd), Ogre2, the drow (rffk5, rffk8b, rffk9,
  rmfb3, rmfb5, rmfb9, rmfg3, rmfg5, rmfg9, rmmk5, rmmk9, rmmk9b),
  Spectatr, Troll, xabash.
- None: Beetle, blackbat, GiantBat, GiantRat, gSpider, Mage Spirit, Rat,
  Scarab, sSpider.

The approved rule was gold moola/2 + 1d(moola) and gems in 1 of 4. It gave
64000 in all (table values), about half of the target (one item of 8000 to
13000 for each two or three missions), so the dice are the ones above. The
income of the creatures that the level files place (`tools/gear_income.py`;
the makers, the summons and the `contains` items are not in it; "as gold"
counts the gems at the 75 percent that the market pays at Charisma 12):

| Missions | Kills | Gold 1.3 | Gold new | Gems new | Total | As gold |
|---|---|---|---|---|---|---|
| 0-2 | 57 | 16 | 373 | 1150 | 1523 | 1236 |
| 3-6 | 136 | 10 | 3412 | 6783 | 10195 | 8499 |
| 7-11 | 251 | 71 | 5167 | 13625 | 18792 | 15385 |
| 12-19 | 299 | 63 | 12025 | 26183 | 38208 | 31662 |
| 20-23 | 96 | 10 | 6562 | 17825 | 24387 | 19931 |
| 24-31 | 110 | 19 | 6457 | 27892 | 34349 | 27376 |
| all | 949 | 189 | 33995 | 93458 | 127453 | 104089 |

The first missions stay below the target: their creatures are kobolds,
rats and skeletons with a small moola and little experience.

### The Mirror Shield

`dtu_gear_mirror` (from `missile_hit_object`, dtuobj_missile.c): a missile
of a creature that hits the player who wears the shield goes back to the
creature when it carries a spell that is not an area spell, or is an eye
ray weapon (81 to 88), and it comes from the front: at most 60 degrees from
the view direction (the cosine 0.5). The missile flies at the center of its
source at its own speed; the player becomes its parent (it passes the
player, and a kill gives experience), and the spell keeps the caster level
of the source (`dtu_spell_hit_level`). Area spells (Fireball and the
others) burst at the shield as before: their area has no single way back.
Arrows, bolts, thrown weapons and the missiles of traps (no source) are not
sent back. The eye rays of a beholder in 1.3 are its spell missiles: a
beholder has a spell level and does not fire its weapon 81 (0x11DE9), and
the setup by name gives the spectator weapon 0, so no creature fires the
weapons 81 to 88.

A decision of the user of 2026-11-27: the shield turns a spell missile that
hits the front whoever owns it at that moment. A Lightning Bolt goes on
through bodies and comes back from walls (`Dtu_fix_bolt_pass`, spells.md
section 14), so the shield also turns the bolt of the player that came
back from a wall, and a bolt that it turned before and that a wall sent
back again: as often as the life of the bolt allows. A missile of the
player goes straight back the way it came (its velocity reversed) and keeps
its caster level; a missile of a creature still flies at the creature.
Each turn starts a new leg of the bolt (it can cross each body again), and
the move of the missile ends in that frame (it does not reach the wall or
the other bodies of the old way). A missile whose source is gone (a trap, a
dead caster) is not turned. From behind the shield turns nothing. Tests:
`bolt_creature_mirror_again`, `bolt_near_wall_mirror`,
`bolt_creature_mirror_behind` (tools/questtest.py).

### Places and market

- The Mace of Disruption: Daland (MYRKUL object 3, script 160) gives it
  with the Potion of Extra-Healing after the consecration of the altar of
  Kelemvor (trigger 31, segment 460), `dtu_gear_script_give`. In 1.3 the
  level fix 0xAD129 holds Daland with no end, and a held creature cannot
  talk (0xABBE5 tests +0x89 bit 0x40: "tries to speak, but nothing
  happens"); a Dispel Magic of the player frees him but makes him hostile.
  That is a defect of the quest group, not of the new gear.
- The Flame Tongue of the dungeon: DROW2, the cell of segment 167 (-172.5,
  -60, 435), the dead end of the cell block farthest from its entry
  (`dtu_gear_place_flame_tongue`, dtuobj_setup.c). The cell block (18 cells
  behind doors) has the three trolls of DROW2 (objects 32, 33, 35); DROW1
  and DROW4 have none, DROW3 has eight more. MINE has one troll (mission
  10). The sword is made once in a game: global flag 0xF0 of the player
  record. A saved state of DROW2 from before gets it at its restore.
  The rule of the sword is the one of 1.3, unchanged: after the table
  fixup the line has 0d0, damage_bonus 1, to_hit_bonus 1 and 1d8 fire;
  0x2AB9F adds 2 to the roll against a target with the regeneration bit
  (24) and 4 against an undead type; 0x2B1FA adds 2 fire against the
  regeneration bit and 4 against undead, the fire dice go through the fire
  resistance of the target (`fire_part`), and fire damage above 0 clears
  the regeneration bit (0x2B5D4), so a troll that the sword burns does not
  get up again. So against a troll: the first hit +3 to hit and 1 + 1d8 + 2
  fire, then +1 and 1 + 1d8 (the bit is gone). Against other creatures:
  +1 to hit, 1 + 1d8 fire, plus Strength. AD&D 2e (DMG): a sword +1 (1d8
  for a long sword), +2 against regenerating creatures, +3 against
  cold-using, inflammable or avian creatures, +4 against undead, to hit and
  damage. The 2e rule would need the sword die in place of the fire die
  (the fixup line), the bonuses on the damage instead of as fire, a list of
  the cold-using, inflammable and avian types of the tables, and a
  `Dtu_fix_*` or rulebook setting.
- The market goods by reward stage (`dtu_gear_market_stock`), the 1.3 good
  that the setting moves (the Flame Tongue of stage 5 goes to stage 7,
  `dtu_gear_market_removed`) and the treasury of Tamaramagil (six items,
  below; `dtu_gear_dragon_treasure`) are tables of dtugear.c that the market and
  the dragon code read. A stock file that took reward 5 before keeps its
  Flame Tongue. The 1.3 magic items with rules but no place in 1.3 that
  the market sells: Club +1, Darts +1 and +2, Bolts +2, Wands of Confusion
  and of Magic Detection, Rings of Protection +3, Coolness, Free Action,
  Infravision and Quickness, Amulets of Protection +3 and +4, Boots of
  Levitation, Cloak of Elvenkind, Chameleon Cloak, Elven Cloak,
  Glim-Gauntlet, the Potions of Anti-Magic, Invisibility and Hill Giant
  Intelligence, Crossbow +3. The arrows (2026-11-27, docs/re/combat.md "Bows
  and arrows of the new gear"): Arrows +1 (50, reward 2; many from reward 4),
  Fire Arrows (20, reward 3; many from 5), Arrows +2 (50, reward 4; many from
  6), Acid Arrows (20, reward 5; many from 7), Arrows +3 (50, reward 6; many
  from 8), Dispelling Arrows (10 at reward 7 and 10 at reward 8, never many);
  Bolt +1 many from reward 4, Bolts +2 many from 6. The Arrows +1 and +2 get
  a value (25, 60): 1.3 gives them none. A "many" row is a record of
  quantity -1 of a good that the stock has: the market adds the quantities
  of two records of one good (0x870A0, -1 stays -1), so the reward changes
  no earlier record and the trailer of the file keeps it from coming twice.

### The treasury of Tamaramagil

`Gear_dragon` (dtugear.c) gives six items to the free places of
`Dr_treasure` (dtudragon.c fills them in order; two free places were added
at -126, -108 and -126, -96): the Frost Brand, the Cloak of Displacement,
the Sash of the Magi, the Belt of Dwarvenkind, the Ring of Regeneration and
the Plate Mail of Fire Resistance. The user's design: she is an optional
challenge, and a player who wins early keeps these for the whole game.

- Frost Brand (weapon 68, value 20000): the last "Rod of Genericness" line
  of 1.3 becomes a copy of the Longsword +3 (line 14, `copy_line` of the
  table fixup: picture, model, sounds, 1d8 +3, +3 to hit, fighter and
  thief, as the long swords of the game) with the new name. Against a fire
  creature (`dtu_gear_fire_creature`: fire immune or fire resistant, or a
  fire spell as its weapon: the red dragon, the hatchlings, the fire
  mephits) 3 more to hit and damage (+6 in all). Resist Fire while it is in
  the hand (item bit 1 of the hand). Checked: the roll bonus against a
  hatchling is 3 more than with the Longsword +3.
- Cloak of Displacement (armor 60, value 18000, all classes): the user's
  rule, not the one of AD&D 2e: the first weapon attack (melee, an arrow, a
  bolt, a thrown weapon) whose attack roll hits the wearer misses
  (`dtu_gear_displace`, called by `damage_player_w` after the roll), with a
  message; the cloak is ready again 30 seconds later (`dtu_time_past`) and
  says so ("Your Cloak of Displacement shimmers again."). Spell missiles
  have no attack roll and are not fooled. The timer is not saved: a loaded
  game starts with the cloak ready.
- Sash of the Magi (armor 61, a belt, value 22000, mage only): one more
  mage slot at each spell level that the slot table of the mage level has
  (`dtu_spell_set_sash`, after the doublings of the Ring of Wizardry and
  the bonus of Rary's Mnemonic Enhancer) and saving throws against spells 2
  better (the save line "+2 (Sash of the Magi)"). As with the ring, the
  slots left are cut to the new maximum when it comes off (a memorized
  spell over the maximum is lost). The mage maximum of levels 1 to 3 for a
  mage of level 5 (table 4 2 1; checked):

| Worn | Levels 1 2 3 |
|---|---|
| none | 4 2 1 |
| Sash | 5 3 2 |
| Ring of Wizardry | 8 4 2 |
| both | 9 5 3 |

### The daggers for thieves

The weapon table holds 90 lines (MAX_WEAPON_TYPES); the mission tables have
89. Free or unused: line 89 (the last place), 68 (the last "Rod of
Genericness", now the Frost Brand) and 76 ("Temp", value 4500, a test line:
no creature type has it as weapon_type, no level file places it, and no
code reads it). Line 10 ("DO NOT USE") stays: KOBOLDS and THIEF2 creatures
carry it in their `contains`.

- Dagger of Venom +2 (line 89, a new line, value 9000; classes of the
  Dagger: fighter, thief, mage): a copy of the Dagger +2 (44: the line is
  read by the table reader, then `copy_line` gives it the picture and the
  fields of 44). A melee hit of the player that does damage
  (`dtu_gear_poison_hit`, from the hit of dtucombat.c) poisons the
  creature unless it is immune or saves against poison (category 0,
  paralyzation, poison and death; `dtu_spell_save_from`): 1 hit point each
  second for 15 seconds (`dtu_gear_poison_frame`, the clock helpers), the
  damage of the player with no "You hit" line (`Dtu_damage_quiet`), so a
  kill gives the experience; the first hit already made the creature
  hostile, so the poison wakes nothing more. A hit while it runs starts the
  15 seconds again (no second poison). Immune: undead and extraplanar types
  (the data has no poison flag; AD&D 2e: undead; the extraplanar types of
  the tables are undead too): Banegard, Ghast, Ghoul, gskelpus, gskelsci,
  Lich3, Mummy, prstsprt, the skeletons, Zombie. The game has no golems,
  elementals or oozes. Poison does not stop the regeneration of a troll
  (only fire and acid do, 0x2B5D4). The state is per object and not saved:
  a load or a level change ends the poison. Place: DROW5, the closet of
  segment 676 (-90, 0, 347.5) behind the locked door of segment 675 side 4
  (wall 95, flags 0x8, no key): a thief opens it with Open Locks (the key
  O), the other classes with Knock or the Gloves of Dexterity (Open Locks
  for other classes). The closet holds a glowing ankh of the level. Once in
  a game (global flag 0xF8).
- Assassin's Dagger +3 (line 76, value 15000, thieves only): a copy of the
  Dagger +2 with +3. A backstab multiplies the damage by one more: (thief
  level + 7) / 4 + 1 (a thief of level 1: 3 instead of 2). In the market at
  reward stage 7 (the beholder caves).

### Saves

An item is a type and a number in the save (16 byte entry,
docs/formats/misc.md) and in the stock files (10 byte record, the number in
one byte). With the setting a save with new items loads
(`tools/check_saveload.py`, fourth run). Without the setting the load leaves
each new item out ("does not fit the inventory", `dtu_inventory_append`),
the effects that the worn new items had on the player object of the level
state end at the first frame (`gear_lost_effects`), the wands 63 to 67 are
the generic wands again, and the market leaves a new item out of its stock
(`mk_insert`) and out of the next write of geldor.dat. Not verified: the
study says that DOS 1.3 asserts on an item number above the count of its
table, so a save with a new item would not load in 1.3.
