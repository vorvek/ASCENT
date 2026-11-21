# LVL and LV2 level files

Status: layout verified against the five level files of the 1.3 install
(`YPINN.LVL`, `BANE.LV2`, `FLAME.LV2`, `IBBALAR.LV2`, `THIEF2.LV2`) and against the
loader code in `DTUGAME.EXE` 1.3. Addresses below are for the LE image with object 1
at 0x10000: `load_level` 0x494EB, `load_mine_data` 0x45CDC, `load_game_data` 0x489EE,
`read_object` 0x47D90.

Later the 35 levels of the disc were unpacked (see `packed.md`) and all 39 files (35
`.LVL`, 4 `.LV2`) were parsed by `tools/lvl.py` and loaded by the level loader of the
engine. The section "All 39 level files" below has the facts that the larger set adds or
corrects. The counts in the other sections are for the five files of the install.

Tools: `tools/lvl.py` (summary, checks, OBJ export, `--summary` for the comparison with
the engine), `tools/lvlcheck.py` (runs the comparison).

## All 39 level files

Checked with `tools/lvl.py` over `gamedata/unpacked/*.LVL` and the four `.LV2` files.

- Mine version 18 in all files. Game data version 41 in 36 files and version 40 in
  three files of the disc that the mission does not use (`IBBALAR1.LVL`, `PRISON1.LVL`,
  `THIEF.LVL`). The version 40 object record has no value for 0x1B0 (4 bytes less: the
  common part is 201 bytes). `object_sizeof` in the game fileinfo is 475 for version 41
  and 473 or 424 for version 40.
- Largest counts: 800 segments (`BEHOLDER`, `DROW2`, `IBBALAR`), 2642 vertices
  (`BEHOLDER`), 182 objects (`DROW5`), 278 walls (`PRISON`), 250 POF names (the
  smallest list has 69).
- Segment `segnum` is never -1. The byte values at 516 to 533 have only the two
  patterns of the table below (all zero in 12375 records, the conversion defaults in
  9965).
- Segment `special` is 0, or 2 in 31 segments (one to three per level, 22 levels). Those
  segments have `matcen_num` 4 and `value` 0, 1, 2 in the order of the segments, which is
  the station number that `fuelcen_create` of Descent 1 gives to them.
- `THIEF1.LVL` has two active door records (16 bytes each: `n_parts` 1, front wall 0
  and back wall 1, front wall 3 and back wall 2, time 0 and 2684). The walls have the
  state 0 (closed). All other tables that are empty in the five files are empty in all
  files.
- Wall `clip_num` goes up to 81. Wall flags are 0, 8, 16 or 24 (in Descent 1, 8 is
  "door locked" and 16 is "door auto").
- Object types and their form in all files: 2 (ai, physics, polyobj) 1110 objects in the
  35 `.LVL` files, 4 (slew, physics, polyobj) 35, 5 (none, none, render 8) 115, 15 (same)
  82, 16 (same) 220, 17 (same) 68, 18 (none, none, polyobj) 938 and (none, none, render
  8) 29.
- The short at object offset 8 is -1 for the player in 24 files and 0 in 15 files. It is
  0 for all other objects.
- The int at object offset 85 (0x88) is 0, in 39 records 0x1000000, in two records
  0x8000. Render type 8 values are 34 to 1095.
- Nine objects (seven in `BEHOLDR2.LVL`, two in `MYRKUL.LVL`) have a position that is in
  no segment for the test of Descent 1 (`find_point_seg`). `fix_object_segs` of Descent 1
  moves such an object to the center of its segment.

### Names and limits from DTUGAME.EXE

The table `Object_type_names` of the executable (9 bytes per name, at file offset
0x197738) has 19 names: the Descent 1 names for 0 to 14, with `(unused)` for 3 (hostage)
and 9 (cntrlcen), and then `ARMOR` (15), `MISC` (16), `SPELL` (17), `DRESSING` (18). The
executable has one table for each new type (asserts `N_armor_types < MAX_ARMOR_TYPES`,
`N_misc_types < MAX_MISC_TYPES`, `N_spell_types < MAX_SPELL_TYPES`,
`N_dressing_types < MAX_DRESSING_TYPES`).

The limits are the constants in the compare instructions of the asserts (address of the
compare in the LE image, object 1 at 0x10000):

| Limit | Value | Where |
|---|---|---|
| `MAX_SEGMENTS` | 900 | `Num_segments <= MAX_SEGMENTS`, 0x46B2C |
| `MAX_VERTICES` | 3608 | `Num_vertices <= MAX_VERTICES`, 0x46AEB (3600 and 8 for the editor's new segment) |
| segments for the game | 800 | `Too many segments (%i > %i) for game (not editor)`, 0xCC3EA |
| `MAX_OBJECTS` | 800 | `objnum < MAX_OBJECTS`, 0x75D52, and the test before `Object creation failed`, 0x6875B |
| `MAX_OBJECT_TYPES` | 19 | `type < MAX_OBJECT_TYPES`, 0xB8BE6 |
| `MAX_WALLS` | 300 | `Num_walls < MAX_WALLS`, 0xBB38D |
| `MAX_DOORS` | 200 | `Num_open_doors < MAX_DOORS`, 0x7D914 |
| `MAX_TRIGGERS` | 255 | `trigger_num < MAX_TRIGGERS`, 0xC0485 |
| `MAX_TEXTURES` | 2500 | `NumTextures < MAX_TEXTURES`, 0x46046 |
| `MAX_POLYGON_MODELS` | 160 | `N_polygon_models < MAX_POLYGON_MODELS`, 0x6DA3F |

The object struct of the executable has 479 bytes and the segment struct has 536 bytes
(the index scaling in the code at 0x75D30 and 0x4D040). So the segment in memory is the
segment record of the file.

### What the loader does after the read

The executable is an editor build. Its `load_mine_data` ends like the editor version of
Descent 1 (0x469F5 to 0x46A33): `reset_objects(1)`, `set_vertex_counts`,
`warn_if_concave_segments`, `validate_segment_all` (0x4D039). The two middle calls are
named by their place in the Descent 1 code. `validate_segment_all` is identified by its
code (the `segnum` test and the string `Segment %i has invalid segnum`). It
computes the type and the normals of each side again and leaves out a segment with
`segnum` -1. So the game does not use the side types and normals of the file.

The engine does the same with the `validate_segment` of Descent 1. Over all 39 files it
gives the type of the file for every side, and no normal component differs by more than
1/256.

### Check of the engine loader

`ascent -datadir DIR -checklevel NAME` loads a level with no window and prints a summary:
counts, the bounding box, the player start, and FNV-1a checksums over the vertices, the
segment children, the segment vertex numbers, the sides, the other segment values, the
walls and the objects (all fields of the record, with the 124 byte block).
`lvl.py --summary FILE` prints the same text from the file. `tools/lvlcheck.py EXE DIR`
compares the two. Result: the summaries are equal for all 39 files and for the
`YPINN.LVL` of the install (which differs from the one of the disc only by the 18 byte
trailer).

A level file is the Descent 1 editor level format (`gamesave.c`, `gamemine.c`), not the
compiled RDL format. The mine data is the editor mine (`load_mine_data`) and the game
data is the Descent 1 game save block (`load_game_data`), both with newer version
numbers and larger records. All values are little endian. `fix` is 16.16 fixed point,
`vec` is three `fix` values (12 bytes), `matrix` is three `vec` (36 bytes).

## Where the levels are

`DTU.MSN` lists 33 levels. The hard disk install holds one `.LVL` file and four `.LV2`
files:

| File | Size | Level name | Segments | Vertices | Objects | Walls |
|---|---|---|---|---|---|---|
| `YPINN.LVL` | 180425 | The Yawning Portal Inn | 227 | 698 | 49 | 74 |
| `BANE.LV2` | 388907 | Temple of Bane | 587 | 1710 | 55 | 94 |
| `FLAME.LV2` | 425259 | Temple of the Flamesword | 663 | 1894 | 37 | 108 |
| `IBBALAR.LV2` | 528587 | Halls of Ibbalar | 800 | 2303 | 127 | 126 |
| `THIEF2.LV2` | 435966 | Thieve's Hall | 640 | 2253 | 105 | 128 |

No other file in the install contains the `LVLP` signature. Every file was scanned,
including all `.DAT`, `.EGG` and `.TUN` files. The only other hits are two constants in
the code of `DTUGAME.EXE`. The level names appear only in `DTU.MSN`, `DTU.WRP`, the
`.SAY` files and `UNINST.LOG`.

The other 28 levels are not on the hard disk. `UNINST.LOG` shows that the installer
copied only `YPINN.LVL` and the per level `.SAY`, `.TIG`, `.TAP` and `.BRN` files.
`DTU.CFG` has `CDPath=D:\DTU` and the executable has the strings `%c:\DTU` and
`Can't open file %s %s on HD or CD`. The levels are in the `\DTU` directory of the disc
in packed form (`NAME.DOG`), see `packed.md`.

`SAVE\SG1_0.LVL` is a save game level. It starts with `DGSS`, not `LVLP`, and is a
different format that this document does not cover.

### LV2

The four `.LV2` files come from the 1.3 patch (the same files are in the patch
directory, dated February and March 1998). The format is identical to `.LVL`: same
`LVLP` header, same mine version 18, same game version 41.

The executable has no `.LV2` string. It builds the name in code: the helper at 0x47693
copies the level file name and replaces the extension with `.LV2`. `load_level` calls it
first and tries to open that name. If the open fails it goes on with the `.LVL` name.
So a `.LV2` file is a patched level that overrides the `.LVL` file of the same name.

## Container header

Offset 0, 20 bytes. Read by `load_level` as five ints.

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | char[4] | signature `LVLP` (the int 0x504C564C) |
| 4 | 4 | int | version, 1 in all files |
| 8 | 4 | int | mine data offset, 20 in all files |
| 12 | 4 | int | game data offset |
| 16 | 4 | int | trailer offset (`hostagetext_offset` in Descent 1). Read, then not used |

In the four `.LV2` files the trailer offset is the file size. In `YPINN.LVL` it is the
file size minus 18. Those 18 bytes are not read by the loader and are the only
unexplained bytes in the five files:
`ff ff ff ff ff 00 00 00 00 ff ff ff ff 12 12 00 00 01`.

All offsets in the mine fileinfo and the game fileinfo are absolute file offsets.

## Mine data

### Mine fileinfo

At the mine data offset, 144 bytes. Same layout as `struct mfi` in Descent 1
`gamemine.h`. The loader reads 8 bytes first (signature, version, size), then seeks
back and reads `fileinfo_sizeof` bytes.

| Offset | Size | Type | Field | Value in the five files |
|---|---|---|---|---|
| 0 | 2 | u16 | fileinfo_signature | 0x2884 |
| 2 | 2 | u16 | fileinfo_version | 18 |
| 4 | 4 | int | fileinfo_sizeof | 144 |
| 8 | 4 | int | header_offset | 164 |
| 12 | 4 | int | header_size | 8 |
| 16 | 4 | int | editor_offset | 172 |
| 20 | 4 | int | editor_size | 112 |
| 24 | 4 | int | segment_offset | varies |
| 28 | 4 | int | segment_howmany | varies |
| 32 | 4 | int | segment_sizeof | 536 |
| 36 | 4 | int | newseg_verts_offset | varies |
| 40 | 4 | int | newseg_verts_howmany | 8 |
| 44 | 4 | int | newseg_verts_sizeof | 12 |
| 48 | 12 | 3 int | group offset, howmany, sizeof | 0, 0, 0 |
| 60 | 4 | int | vertex_offset | 19641 |
| 64 | 4 | int | vertex_howmany | varies |
| 68 | 4 | int | vertex_sizeof | 12 |
| 72 | 4 | int | texture_offset | 284 |
| 76 | 4 | int | texture_howmany | 1489 |
| 80 | 4 | int | texture_sizeof | 13 |
| 84 | 12 | 3 int | walls offset, howmany, sizeof | stale, see below |
| 96 | 12 | 3 int | triggers offset, howmany, sizeof | stale, see below |
| 108 | 12 | 3 int | links offset, howmany, sizeof | 0, 0, 0 |
| 120 | 12 | 3 int | object offset, howmany, sizeof | 0, 0, 0 |
| 132 | 12 | 3 int | unused offset, howmany, sizeof | 0, 0, 0 |

The walls and triggers entries are not used by `load_mine_data`. The walls offset
equals the game data offset, the walls count equals the real wall count, and the
triggers offset is walls offset plus count times 24. No wall data is stored there, the
real walls are in the game data.

Version rules in the loader: a version below 16 is rejected. Versions 16 and 17 use a
522 byte segment record that the loader converts. Version 18 and up read the 536 byte
record directly. Only version 18 exists in the files.

Loader limits: 3600 vertices, 900 segments.

### Mine header

At `header_offset`, 8 bytes: int `num_vertices`, int `num_segments`. Both match the
fileinfo counts in all files.

### Mine editor block

At `editor_offset`, 112 bytes. Same layout as `struct me` in Descent 1.

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | int | current_seg |
| 4 | 4 | int | newsegment_offset: file offset of the editor "new segment" template |
| 8 | 4 | int | newsegment_size, 536 |
| 12 | 4 | int | Curside |
| 16 | 4 | int | Markedsegp, -1 for none |
| 20 | 4 | int | Markedside |
| 24 | 40 | int[10] | Groupsegp. Holds the same stale pointer value ten times |
| 64 | 40 | int[10] | Groupside |
| 104 | 4 | int | num_groups, 0 |
| 108 | 4 | int | current_group, 0 |

### Texture names

At `texture_offset`: `texture_howmany` records of 13 bytes, each a NUL terminated
name. All five files hold the same count, 1489 names (1467 unique). Examples: `green`,
`ladder`, `stone223`, `wood16`, `idor2#12`. Names with `#n` are animation frames.

`tmap_num` and `tmap_num2` of a side are indexes into this table. The loader compares
the table with the texture table of the running game. If they differ it builds a
translation table by name and rewrites every `tmap_num` and `tmap_num2` (this is
`tmap_xlate_table` in Descent 1).

### Vertices

At `vertex_offset`: `vertex_howmany` records of 12 bytes, one `vec` each.

### Segment record, 536 bytes

At `segment_offset`: `segment_howmany` records. The executable calls the type
`v18_seg`. It is the Descent 1 editor `segment` struct with 14 more bytes.

| Offset | Size | Type | Field | Descent 1 | Notes |
|---|---|---|---|---|---|
| 0 | 2 | s16 | segnum | segnum (editor) | Not the record index in 68% of the records, and not unique. `validate_segment_all` leaves out a segment with -1 here. No record has -1 |
| 2 | 2 | s16 | group | group (editor) | -1 in all records. The loader sets it to -1 |
| 4 | 480 | side[6] | sides | sides | see below |
| 484 | 12 | s16[6] | children | children | -1 none, -2 exit, else segment index |
| 496 | 16 | s16[8] | verts | verts | vertex indexes |
| 512 | 4 | fix | static_light | static_light | 0.004 to 13.82 in the files |
| 516 | 4 | int | unknown | new | 0 in all records |
| 520 | 4 | fix | unknown | new | -1.5 (0xFFFE8000) in 1525 records, 0 in 1392 |
| 524 | 4 | int | unknown | new | 0 in all records |
| 528 | 2 | s16 | objects | objects | first object in the segment. The loader sets it to -1 |
| 530 | 2 | s16 | value | value | 0, twice 1 |
| 532 | 1 | u8 | unknown | new | 1 in exactly the records where offset 520 is -1.5, else 0 |
| 533 | 1 | u8 | unknown | new | 0 in all records |
| 534 | 1 | u8 | special | special | 0, six times 2 |
| 535 | 1 | s8 | matcen_num | matcen_num | -1, 13 times 0, 8 times 4 |

How the field names were found: the loader still has the conversion from the 522 byte
version 16 record. That record is the Descent 1 layout (segnum, sides, children,
verts, objects, special, matcen_num, value, static_light, pad). The conversion copies
each old field to the offset shown above. For the fields that the old record does not
have it writes these defaults: 0 at 516, -1.5 at 520, 0 at 524, 1 at 532, and a
register value at 533. So a record with -1.5 at 520 and 1 at 532 carries the conversion
defaults. The meaning of these four fields is unknown.

Compared with Descent 1 the order changed: `group` moved to offset 2, and
`static_light` moved in front of `objects`.

The name `static_light` is from the conversion code only. The value is not the average
and not the sum of the 24 side light values.

Side order and the vertex numbers of each side are the same as in Descent 1
(`Side_to_verts`): left 7,6,2,3; top 0,4,7,3; right 0,1,5,4; bottom 2,6,5,1; back
4,5,6,7; front 3,2,1,0. This is verified by the normal check below.

### Side record, 80 bytes

Same size and layout as the Descent 1 editor `side` struct.

| Offset | Size | Type | Field | Notes |
|---|---|---|---|---|
| 0 | 1 | s8 | type | 1 quad, 2 triangulated 0-2, 3 triangulated 1-3. No other value occurs |
| 1 | 1 | u8 | unknown | `pad` in Descent 1. Here 0 (81%), 2 (19%), 1 or 3 (under 1%). Guess: flag bits |
| 2 | 2 | s16 | wall_num | -1 for none, else index into the wall table |
| 4 | 2 | s16 | tmap_num | index into the texture name table |
| 6 | 2 | s16 | tmap_num2 | low 14 bits: overlay texture index, 0 for none. Top 2 bits: overlay rotation |
| 8 | 48 | uvl[4] | uvls | per corner: fix u, fix v, fix l (light) |
| 56 | 24 | vec[2] | normals | one normal per triangle, pointing into the segment |

Texture index 0 to 4 (all named `green`) is on 4921 sides. All but 5 of them are sides
that are not drawn (a child segment and no wall). Guess: it is the editor default
texture.

### New segment template

At `newsegment_offset` (directly after the segment array): one 536 byte segment record,
the editor "new segment". At `newseg_verts_offset` (directly after it): 8 vertices of
12 bytes. The loader reads both into editor state. They are not part of the level
geometry.

## Game data

### Game fileinfo

At the game data offset, 119 bytes. Same layout as `game_fileinfo` in Descent 1
`gamesave.c`.

| Offset | Size | Type | Field | Value in the five files |
|---|---|---|---|---|
| 0 | 2 | u16 | fileinfo_signature | 0x6705 |
| 2 | 2 | u16 | fileinfo_version | 41 |
| 4 | 4 | int | fileinfo_sizeof | 119 |
| 8 | 15 | char[15] | mine_filename | all zero |
| 23 | 4 | int | level | 0 |
| 27 | 4 | int | player_offset | varies |
| 31 | 4 | int | player_sizeof | 13181 |
| 35 | 4 | int | object_offset | varies |
| 39 | 4 | int | object_howmany | varies |
| 43 | 4 | int | object_sizeof | 475 |
| 47 | 4 | int | walls_offset | varies |
| 51 | 4 | int | walls_howmany | varies |
| 55 | 4 | int | walls_sizeof | 24 |
| 59 | 4 | int | doors_offset | file end or trailer offset |
| 63 | 4 | int | doors_howmany | 0 |
| 67 | 4 | int | doors_sizeof | 16 |
| 71 | 4 | int | triggers_offset | file end or trailer offset |
| 75 | 4 | int | triggers_howmany | 0 |
| 79 | 4 | int | triggers_sizeof | 54 |
| 83 | 12 | 3 int | links offset, howmany, sizeof | 0, 0, 0 |
| 95 | 4 | int | control_offset | -1 |
| 99 | 4 | int | control_howmany | 0 |
| 103 | 4 | int | control_sizeof | 42 |
| 107 | 4 | int | matcen_offset | file end or trailer offset |
| 111 | 4 | int | matcen_howmany | 0 |
| 115 | 4 | int | matcen_sizeof | 16 |

`object_sizeof` and `player_sizeof` are the sizes of the structs in the editor build
that saved the file. `object_sizeof` is not the size of an object record in the file:
object records have a variable size, see below. The 1.3 executable has an object struct
of 479 bytes and a default `player_sizeof` of 6967.

A version below 22 is rejected by the loader.

### Level name and POF names

Directly after the fileinfo:

| Size | Type | Field |
|---|---|---|
| varies | char[] | level name, NUL terminated (Descent 1 ends it with a newline) |
| 2 | u16 | N_save_pof_names. 25 in all five files |
| 13 each | char[13] | POF model file names |

The count is wrong for the data that follows. The name list runs up to
`player_offset` and holds 86 to 238 names (232 in `YPINN.LVL`). The length of the
list is always a whole number of 13 byte records and every record is a valid `.pof`
name. The loader reads only `count` times 13 bytes and then seeks to `object_offset`.

`model_num` of a polygon object is an index into the full list. This is not proved by
code, but the names fit: the one robot in the inn uses `durnan.pof`, the scenery
objects use `chair.pof`, `barrel01.pof`, and all 292 model numbers are inside the
list while 213 or 231 are far past the count of 25.

### Player block

At `player_offset`, `player_sizeof` (13181) bytes. The loader never reads it. It is
almost empty: bytes 151 and 154 are 2 and 3, offset 13107 holds the string
`a01temp2.pof`, the last 48 bytes are 0xFF, the rest is zero. Guess: it is the
`player` struct of the editor, written as in Descent 1. The tool treats it as opaque.

### Object records, variable size

At `object_offset`: `object_howmany` records, one after the other. The size of a record
depends on its movement, control and render type. The layout below is the read order
of `read_object` in the executable for version 41. "Mem" is the offset of the field in
the object struct in memory.

Common part, 205 bytes:

| Offset | Size | Type | Mem | Field | Notes |
|---|---|---|---|---|---|
| 0 | 1 | u8 | 0x04 | type | see type table |
| 1 | 1 | u8 | 0x05 | id | |
| 2 | 1 | u8 | 0x0A | control_type | |
| 3 | 1 | u8 | 0x0B | movement_type | |
| 4 | 1 | u8 | 0x0C | render_type | |
| 5 | 1 | u8 | 0x0D | flags | 0 in all records |
| 6 | 2 | s16 | 0x0E | segnum | |
| 8 | 2 | s16 | 0x10 | attached_obj (guess from the Descent 1 struct order) | new in the file. -1 for the player, 0 for all others. Present if version is above 32 |
| 10 | 12 | vec | 0x14 | pos | |
| 22 | 36 | matrix | 0x20 | orient | the loader also copies it to 0x44 |
| 58 | 4 | fix | 0x68 | size | |
| 62 | 4 | fix | 0x6C | shields | |
| 66 | 12 | vec | 0x70 | last_pos | |
| 78 | 1 | s8 | 0x7C | contains_type | -1 for none |
| 79 | 1 | s8 | 0x7D | contains_id | |
| 80 | 1 | s8 | 0x7E | contains_count | |
| 81 | 2 | s16 | 0x80 | unknown | new. 0 in all records |
| 83 | 2 | s16 | 0x82 | unknown | new. 0 in all records |
| 85 | 4 | int | 0x88 | unknown | new. 0 in 372 records, 0x1000000 in one. The loader overwrites it with the value at file offset 169 |
| 89 | 52 | fix[13] | 0x8C to 0xBC | unknown | new. The value for 0xAC (file offset 121) is 0 to 30 for types 5 and 16, else 0 |
| 141 | 8 | fix[2] | 0xC0, 0xC4 | unknown | new. Present if version is above 35 |
| 149 | 24 | fix[6] | 0xC8 to 0xDC | unknown | new |
| 173 | 8 | fix[2] | 0xE0, 0xE4 | unknown | new |
| 181 | 20 | fix[5] | 0xE8 to 0xF8 | unknown | new. Present if version is above 36 |
| 201 | 4 | fix | 0x1B0 | unknown | new. Present if version is above 40. -1 for the player, 0x400000 plus low bits (0x400000 to 0x400340) for all robots, 0 in 209 of 236 other records, else a small value |

The 124 bytes from offset 81 to 204 do not exist in Descent 1. The executable reads
them with the "read fix" helper, so the type is from the code, the meaning is unknown.
In the level files 29 of the 32 values are zero in every record. Guess: these are the
role playing statistics of the object, filled in at run time.

After the read, for object types 5, 15, 16 and 17 the loader moves the low byte of
0xAC to the byte at 0x1D5 and clears 0xAC.

Movement part, by `movement_type`:

| Type | Size | Content |
|---|---|---|
| 0 none | 0 | |
| 1 physics | 64 | vec velocity, vec thrust, fix mass, fix drag, fix brakes, vec rotvel, vec rotthrust, s16 turnroll (fixang), u16 flags |
| 3 spinning | 12 | vec spin_rate |

Control part, by `control_type`:

| Type | Size | Content |
|---|---|---|
| 0 none, 4 flying, 5 slew, 12 debris, 16 cntrlcen | 0 | |
| 1 ai | 24 | u8 behavior, s8 flags[11], s16 hide_segment, s16 hide_index, s16 path_length, s16 cur_path_index, s16 follow_path_start_seg, s16 follow_path_end_seg |
| 2 explosion | 10 | fix spawn_time, fix delete_time, s16 delete_objnum |
| 9 weapon | 8 | s16 parent_type, s16 parent_num, int parent_signature. Read, then reset to -1 |
| 13 powerup | 4 | int count. Read and discarded |
| 14 light | 6 | fix intensity, s16 unknown (present if version is above 38) |

Any other control type stops the loader. Only types 0, 1 and 5 occur in the files.

Render part, by `render_type`:

| Type | Size | Content |
|---|---|---|
| 0 none, 3 laser | 0 | |
| 1 polyobj, 6 morph | 24 | int model_num, s16 unknown (mem 0x16E), s16 unknown (0x170), int unknown (0x172), int unknown (0x176), int subobj_flags (0x166), int tmap_override (0x16A) |
| 2 fireball, 7 weapon_vclip | 9 | int vclip_num, fix frametime, u8 framenum |
| 5 powerup | 9 | int, fix, u8. Read and discarded |
| 8 (new) | 4 | int, stored as a 16 bit value at mem 0x162 |

Render type 4 (hostage) stops the loader. Version 41 files store no animation angles
in the polyobj part (versions below 30 did).

The fields 0x170, 0x172 and 0x176 are the animation state, the animation frame and the
goal state of the model (see `docs/re/models.md`). State 1 is STAND. Frame -1 means no
animation.

Polyobj values seen: 0x16E is 0 (twice 5). 0x170, 0x172, 0x176 are 1, 0, 1 for all
robots and players and 0, -1, -1 for type 18 scenery (one type 18 record has 1, 0, -1). `subobj_flags` is 0. `tmap_override` is -1
in 281 of 292 records, else a texture index that the loader translates.

Render type 8 values are 87 to 733. Guess: an index of the bitmap that draws an item.
The table that it indexes was not identified.

Record sizes that occur:

| Size | Composition | Objects |
|---|---|---|
| 209 | common + render 8 | types 5, 15, 16, 17, and 3 of type 18 |
| 229 | common + polyobj | type 18 |
| 293 | common + physics + polyobj | player (control slew) |
| 317 | common + physics + ai + polyobj | robots |

Object types. 0 to 14 are the Descent 1 values (0 wall, 1 fireball, 2 robot, 3
hostage, 4 player, 5 weapon, 6 camera, 7 powerup, 8 debris, 9 cntrlcen, 10 flare, 11
clutter, 12 ghost, 13 light, 14 coop). Types 15 to 18 are new. Counts over the five
files:

| Type | Count | Control, movement, render | Notes |
|---|---|---|---|
| 2 robot | 132 | ai, physics, polyobj | monsters and characters |
| 4 player | 5 | slew, physics, polyobj | one per level, object 0 |
| 5 weapon | 16 | none, none, 8 | guess: weapon items on the floor |
| 15 armor | 14 | none, none, 8 | item. Name from the executable, see above |
| 16 misc | 40 | none, none, 8 | item |
| 17 spell | 8 | none, none, 8 | item |
| 18 dressing | 158 | none, none, polyobj (155) or 8 (3) | scenery: chairs, beds, barrels |

### Wall record, 24 bytes

At `walls_offset`. Same as the Descent 1 `wall` struct. The loader asserts the size.

| Offset | Size | Type | Field |
|---|---|---|---|
| 0 | 4 | int | segnum |
| 4 | 4 | int | sidenum |
| 8 | 4 | fix | hps |
| 12 | 4 | int | linked_wall, -1 for none |
| 16 | 1 | u8 | type: 2 door, 3 illusion, 5 closed occur |
| 17 | 1 | u8 | flags |
| 18 | 1 | u8 | state |
| 19 | 1 | u8 | trigger, 255 for none |
| 20 | 1 | s8 | clip_num |
| 21 | 1 | u8 | keys |
| 22 | 2 | s16 | pad |

### Tables that are empty in the five files

The loader reads these with the Descent 1 struct sizes. The counts are 0 in all five
files, so the layouts below are from the Descent 1 structs and the size checks in the
loader only. They are not verified against data.

- Active doors, 16 bytes: int n_parts, s16 front_wallnum[2], s16 back_wallnum[2], fix
  time.
- Triggers, 54 bytes: s8 type, s16 flags, fix value, fix time, s8 link_num, s16
  num_links, s16 seg[10], s16 side[10].
- Control center triggers, 42 bytes: s16 num_links, s16 seg[10], s16 side[10].
- Matcens, 16 bytes: int robot_flags, fix hit_points, fix interval, s16 segnum, s16
  fuelcen_num.

No level uses Descent 1 triggers. Guess: the `.TIG` and `.TAP` files of each level
replace them.

## Verification

`python -I tools/lvl.py FILE` parses a file, accounts for every byte and runs the
checks. Results over the five files (1,959,144 bytes in total):

Byte coverage: 1,893,221 bytes parsed, 65,905 bytes opaque (the five player blocks,
which the game does not read), 18 bytes unexplained (the trailer of `YPINN.LVL`). No
overlapping ranges. In every file the variable size object records end exactly at
`walls_offset`.

| Check | Passed | Rate |
|---|---|---|
| children index in range (-2 to count-1) | 17502 / 17502 | 100% |
| children symmetric (the child points back) | 8280 / 8280 | 100% |
| connected sides share the same 4 vertices | 8280 / 8280 | 100% |
| vertex index in range | 23336 / 23336 | 100% |
| texture index in range (tmap_num and tmap_num2) | 35004 / 35004 | 100% |
| side type is 1, 2 or 3 | 17502 / 17502 | 100% |
| stored side normal matches the normal computed from the vertices | 17501 / 17502 | 99.99% |
| side wall_num in range | 17502 / 17502 | 100% |
| wall segnum and sidenum point to a side with that wall_num | 530 / 530 | 100% |
| side wall_num points to a wall with that segnum and sidenum | 530 / 530 | 100% |
| wall linked_wall and trigger in range | 530 / 530 | 100% |
| object segnum in range | 373 / 373 | 100% |
| object position inside the bounding box of its segment | 371 / 373 | 99.46% |
| object position on the inner side of all 6 side planes (0.05 tolerance) | 373 / 373 | 100% |
| object orientation rows are unit vectors | 373 / 373 | 100% |
| mine header counts match fileinfo counts | 10 / 10 | 100% |

The two objects outside the bounding box are in `THIEF2.LV2` and are outside by less
than 0.0005 units. The one normal mismatch is in `BANE.LV2`.

## Unknown

- Segment: the fields at 516, 520, 524, 532, 533 (14 bytes). Side: the byte at
  offset 1.
- Segment `static_light`: the name is from the old record conversion, how the value
  is computed is unknown.
- Object: the 124 byte block at offset 81, the `attached_obj` name at offset 8, the
  three values at mem 0x16E to 0x176 of the polyobj part, the render type 8 value, the
  tables that the ids of the types 15 to 18 are indexes of.
- Segment `special` 2 with `matcen_num` 4: what the game does in such a segment.
- The 18 byte trailer of `YPINN.LVL`.
- The content of the player block.
- Why the POF name count is 25.
- Whether the game moves the nine objects that are in no segment, as Descent 1 does.
