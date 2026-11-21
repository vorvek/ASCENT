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
| 0x0D | u8 | flags. 2 is "should be dead" (set on pickup). 0x40 is set for a creature whose table entry has `pass_thru` | Code 0x2846B, 0x6A102 |
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
| 0x30 | fix | `player_light` | 0 |
| 0x34 | fix | `mass` or `weight` | 1.0 |
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
| 0x00 | u16 | class mask (guess from the use at 0x28302) |
| 0x02 | s16 | `icon` |
| 0x24 | s16 | bitmap_num, from `picture` |
| 0x26 | u8 | `plot_item` |
| 0x28 | s16 | `uses`, default 1 |
| 0x2A | s16 | `uses_per_level` |
| 0x2C | s16 | `levels_per_use` |
| 0x36 | u8 | flags. The mask 2 is tested as "stackable" by 0xB61B2 |
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
| 0x3E | u8 | flags: `stackable` 2, `magic` 0x10 |
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
   (for example 0xAD1B4 for level 25). Not covered here.
3. Each dressing type with `explode_sound` -1 gets sound 2704.
4. Open the script data file (the "GAB data file").
5. For each object 1 to the highest index:
   - Type 2: `creature_fixes` (0xAC7B2). It changes table entries by the name of the
     creature type (`hatchlng`, `fmephit`, `imephit`, `lmephit`, `flayer`, `flaynpc`,
     `spectatr`, `gargoyle`, names that start with `lich`, and a list of `s...` names).
     On level 0 it also gives each creature 200 hit points (`shields` = 200.0, table
     `max_hit_points` = 200, `str` = 19, `score_value` = 10000) (0xACC3C).
   - Type 2, or any type with a script number: if the object is not yet in the NPC
     table (0xAC3C3), add it (`add_npc`, 0xAC431).

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

Port: `pickup_sound()` and `pickup_flag_10()` in `dtuobj.c`. Setup 1.0 of the port
played no sound for a pick up.

### The byte at 0x1D5

The loader fills it from the low byte of 0xAC of the file (0xAC is the timer of the
Fear effect for a creature, so the editor used the same field for two things).

- It is the count of a stack or the charges of an item. 0 means the default of step 2.
- In the level files it is not 0 for 10 weapon records, all wands (10 to 56 charges),
  and for 26 misc records: 24 with id 102 (`Scroll`), one `Ruldegost Document` and one
  `Scroll of the Illithids` (Data). The 24 scrolls have 24 different values from 2
  to 27. Guess: for a scroll the value is the number of its text.
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
  What takes the corpse away later was not read.
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
- `level_start` (the fixes for single levels, `creature_fixes`, the NPC table and the
  scripts) is not in the port. A creature with a script number or with bit 9 of 0x1B0 is
  not hostile.
- Collision bodies: the port tests a moving object against the cylinder of 0x748D9 in
  the x-z plane and then compares the heights (`dtu_check_vector_to_object` in `fvi.c`).
  The test of the executable inside 0x3EFD9 was not read.
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
