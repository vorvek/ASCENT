# EGG archives and BIN tables

Status: layout verified against `HMC.EGG`, `HMT.EGG` and `PORTAL.EGG` of version 1.3,
and against the loader code in `DTUGAME.EXE` (function at linear address 0x6DD20 for
the header tables, 0x6E972 for the model page-in; addresses are for the LE image with
object 1 at 0x10000).

Tool: `tools/egg.py` (list, check, extract, bin).

The port loads these files with `engine/main/piggy.c` (archive), `polyobj.c` (models),
`acm.c` (sounds) and `bmread.c` (tables). `engine/main/test_dtudata.c` is the check
program. It loads all 57 archives and 85 tables of the disc (see "Check with the port
loader" at the end). The table text has its own document: [tbl.md](tbl.md).

An EGG file is the Undermountain version of the Descent 1 PIG file. The source file
is still `piggy.c`. An EGG file holds three kinds of entries: bitmaps, sounds and
polygon models. All values are little endian. `fix` is 16.16 fixed point, `vec` is
three `fix` values (12 bytes).

## Files in the install

| File | Bitmaps | Sounds | Models | Size |
|---|---|---|---|---|
| `HMC.EGG` | 31 | 6 | 21 | 1462164 |
| `HMT.EGG` | 31 | 6 | 21 | 1466446 |
| `PORTAL.EGG` | 3580 | 121 | 58 | 12253364 |

`PORTAL.EGG` is the main game archive. `HMC.EGG` and `HMT.EGG` are player character
archives (body part models `hmc?_p.pof`, 192 animation frames each, plus skin textures
and six voice sounds). Guess from the names: HMC is human male cleric, HMT is human
male thief. The executable also names `DTU.EGG`, `FRONTEND.EGG`, `DEMO.EGG` and
`SHORT.EGG`. None of these exist in the install.

## Overall layout

| Offset | Size | Content |
|---|---|---|
| 0 | 4 | `N_bitmaps` (u32) |
| 4 | 4 | `N_sounds` (u32) |
| 8 | 4 | `N_models` (u32) |
| 12 | 17 * N_bitmaps | bitmap headers |
| ... | 24 * N_sounds | sound headers |
| ... | 218 * N_models | model headers |
| `data_start` | ... | bitmap data, then sound data, then model data |
| end - 18 | 18 | trailer, not read by the game (see Unknown) |

`data_start = 12 + 17 * N_bitmaps + 24 * N_sounds + 218 * N_models`

Every `offset` field in a header is relative to `data_start`. The loader computes
`file position = offset + header table size + 12`. There is no magic number and no
version field.

In all three files the data is stored in header order without gaps: all bitmaps, then
all sounds, then all models. The format does not require this order.

## Bitmap header (17 bytes)

Same as the Descent 1 `DiskBitmapHeader`.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 8 | `name` | NUL padded, no extension. Bytes after the first NUL can be garbage. |
| 8 | 1 | `dflags` | bit 6 (0x40): animation frame, bits 0..5 are the frame number. bit 7 (0x80): add 256 to `width`. |
| 9 | 1 | `width` | |
| 10 | 1 | `height` | |
| 11 | 1 | `flags` | see below |
| 12 | 1 | `avg_color` | palette index of the average color |
| 13 | 4 | `offset` | relative to `data_start` |

The game registers an animation frame under the name `name#frame` (for example
`potion#1`). Names with the frame suffix are unique inside each file.

`dflags` bit 7 is in the loader code. No bitmap in the three files sets it (largest
bitmap is 168 x 239), so the tool path for it is not tested on real data.

Flags:

| Bit | Name | Meaning | Status |
|---|---|---|---|
| 0x01 | TRANSPARENT | color 255 is transparent | Descent 1 meaning, copied by the loader |
| 0x02 | SUPER_TRANSPARENT | color 254 is see-through | Descent 1 meaning, copied by the loader |
| 0x04 | NO_LIGHTING | | Descent 1 meaning, copied by the loader. Set on one bitmap (`miss` in `PORTAL.EGG`). |
| 0x08 | RLE | data is run length encoded | verified by decoding |
| 0x10 | PAGED_OUT | Descent 1 `BM_FLAG_PAGED_OUT`. The Descent 1 archive writer sets it on a frame that its low memory mode replaces by the frame before it. The header loader ignores it. | verified by data, see below |
| 0x40 | not remapped | set on 149 bitmaps of `PORTAL.EGG` (interface art: buttons, portraits). The loader copies it to the in-memory bitmap flags. | verified by data, see below |

Flag 0x10: in `PORTAL.EGG` it is on 1143 bitmaps. These are exactly the animation
frames with an odd frame number that have a next frame (0 exceptions in 3580 bitmaps).
This is the rule of `piggy_is_substitutable_bitmap` in Descent 1 `piggy.c`.

Flag 0x40: the 149 bitmaps with this flag are exactly the 149 bitmaps that `YPINN.BIN`
lists between the lines `!BITMAP_REMAP_OFF` and `!BITMAP_REMAP_ON` (second `$TEXTURES`
section). So the tool that built the archive did not remap these bitmaps to the game
palette. They decode to the correct shapes but show wrong colors with `PALETTE.256`.
The palette for them is not identified yet.

One bitmap of the 57 archives is damaged: `bless#6` in `MULHOR.EGG`. Its data block
has 27904 bytes (the size field says so too), but the row sizes add up to 1851 and
row 0 has size 0. The frames before and after it are valid (589 to 1114 bytes). The
port shows its "bogus" bitmap for it.

### Bitmap data

Without RLE: `width * height` bytes, one palette index per pixel, top row first.

With RLE (same as Descent 1 `rle.c`):

| Offset | Size | Content |
|---|---|---|
| 0 | 4 | total size in bytes, includes this field |
| 4 | height | encoded size of each row (u8) |
| 4 + height | ... | rows |

Row encoding: a byte with the top three bits set (`b & 0xE0 == 0xE0`) is a code. If
its low five bits are zero the row ends. Otherwise the low five bits are a run length
and the next byte is the color. Any other byte is one literal pixel.

Verified: all 3364 RLE bitmaps of the three files decode to exactly `width` pixels per
row, each row uses exactly its listed size, and the rows end exactly at the total
size. All 278 raw bitmaps have exactly `width * height` bytes before the next entry.

In-game textures are 64 x 64. Sprites and interface art have other sizes.

## Sound header (24 bytes)

Descent 1 `DiskSoundHeader` plus one field.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 8 | `name` | NUL padded, garbage can follow the NUL |
| 8 | 4 | `length` | bytes in the file (Descent 1: sample count) |
| 12 | 4 | `data_length` | bytes in the file. Equal to `length` for all sounds. |
| 16 | 4 | `offset` | relative to `data_start` |
| 20 | 4 | `flags` | 0, or 2 for 20 sounds of `PORTAL.EGG` (`cmbt*`, `foot*`, `gen09`) |

Sound data is one Interplay ACM stream, the format of the `.TUN` music files (see
[tun.md](tun.md)): signature `97 28 03 01`, then the count of 16-bit values, the
channel count, the sample rate and the block shape. The source files have the
extension `.RCA` ("Raw Compressed Audio"). DTUGAME.EXE refuses other data. Its
message (string at 0x123924) starts with "DTU no longer supports uncompressed
sounds" and names the extension .RCA.

Verified: every sound of the 55 archives with this layout starts with the signature
and decodes to its full count (the port loader counts 0 bad sounds). All have one
channel and 22050 Hz. The C decoder of the port (`engine/main/acm.c`) gives the same
46380 values as `tools/tun.py` for `fireball` of `PORTAL.EGG`.

### Old layout (DEMO.EGG, FFM.EGG)

Two archives of the disc have an older layout. The sound header is the 20 bytes of
Descent 1 (no `flags` field), and the sound data is raw unsigned 8-bit PCM (it starts
with `80 80 80 80`). Everything else is the same. With 20-byte sound headers all
bitmaps of both files are valid RLE data and all model names end in `.pof` (DEMO:
2772 bitmaps, 15 sounds, 238 models. FFM: 68, 12, 48). With 24 bytes nothing fits.
This is why `tools/egg.py check` fails on these two files. The file has no version
field. The port tells the layouts apart by the name field of the first model header.
The sample rate of the raw sounds is not known (guess: 11025 Hz as in Descent 1).

The 1.3 game does not use these two files as they are: `FFM.HEN` (the table of the
character) is empty, and DTUGAME.EXE handles the name `DEMO.EGG` in a separate code
path (string list at 0x120D64, option `-forceold`), which is not analyzed.

Flag 2: the loader skips these sounds when it adds up the sound memory size if a
global switch is set. The meaning of the switch is unknown.

## Model header (218 bytes)

This replaces the Descent 1 `polymodel` struct dump. The per-submodel arrays moved out
of the struct into the model data block. The loader reads the 218 bytes and copies
fields into a 332 byte memory struct.

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0x00 | 4 | `n_models` | number of submodels |
| 0x04 | 4 | `model_data_size` | size of the interpreter data |
| 0x08 | 12 | `mins` | vec |
| 0x14 | 12 | `maxs` | vec |
| 0x20 | 4 | `rad` | fix |
| 0x24 | 4 | `n_textures` | |
| 0x28 | 4 | `first_texture` | index into a global table of 4 byte entries (Descent 1: `ObjBitmapPtrs`), see below |
| 0x2C | 4 | unknown | -1 in all 100 models |
| 0x30 | 4 | unknown | -1 in all 100 models |
| 0x34 | 4 | `n_frames` | animation frame count (assert text: `pm->n_frames <= N_ANIM_FRAMES`) |
| 0x38 | 4 | `offset` | relative to `data_start` |
| 0x3C | 1 | `flags` | bits 1, 2, 4, 8 select optional arrays in the data block |
| 0x3D | 13 | `name` | POF file name, NUL terminated, for example `hmc__p.pof` |
| 0x4A | 1 | unknown | copied by the loader (memory 0xB6) |
| 0x4B | 1 | unknown | copied by the loader (memory 0xB7) |
| 0x4C | 2 | unknown | s16, copied by the loader (memory 0x130) |
| 0x4E | 12 | unknown | vec, copied by the loader (memory 0x134) |
| 0x5A | 12 | unknown | vec, copied by the loader (memory 0x140) |
| 0x66 | 8 | `fire_frames` | four s16, the values of `fire_frames=` of the table line, -1 if not given |
| 0x6E | 32 | `speeds` | eight fix, the first eight values of `speeds=` of the table line |
| 0x8E | 76 | `state_times` | nineteen fix. Entry 0 is 0. Entries 1 to 18 are the values of `state_times=` of the table line |

Verified for 0x66 to 0x8E: `hmc__p.pof` holds 45, 61, 109, 125, then 10, 4, 20, 5, 7,
25, 2, 10, then 0, 0.5, 2, 0.5, ... which are the values of the `$PLAYER` line of
`HMC.BIN`. `durnan.pof` holds the values of its `$MONSTER` line. `load_polygon_model`
in DTUGAME.EXE (0x72DC5) takes three arrays of 4, 9 and 19 values and stores them at
these places of the memory struct. A model whose table line has no such keywords
(scenery) holds -1 in 0x66 and garbage in the last nine entries of 0x8E.

The fields 0x4A to 0x65 are copied but their meaning is unknown. In scenery models
these bytes look like uninitialized memory.

`first_texture` is the number of the first object bitmap slot of the model, as the
table parser counted the slots when the archive was built (see tbl.md, "Object bitmap
slots"). In `HMC.EGG` it runs up by `n_textures` from model to model (0, 7, 15, ...
total 174), because the `$PLAYER` line of `HMC.BIN` is all that its table has. In
`PORTAL.EGG` the models of the `$PLAYER` line have 0 to 29 and the first model after
it has 213: the `picture=` values of the item lines between them take the 183
slots from 30 to 212.

DTUGAME.EXE does not rebase the value. For a model of a player archive it sets the
texture pointer to slot table + 4 * `first_texture` (0x70B89). For a model of the
level archive it adds one more slot (0x70EED: `first_texture * 4 + 4`). It registers
the models of the player archives first, then the models of the level archive (strings
"done with player polys" and "polys done"). How the slots of a player archive and of
a level archive stay apart in the running game is not analyzed.

The port does not use `first_texture` of the file. Its table parser gives each model
the slots of the texture list of the first table line that names the model. The
parser of the port and the header agree on `n_textures` for every model that a table
names, in all 81 tables that load (the loader prints a message if they differ, and
the sweep gives none). One case needs care: `hum_ffg.pof` in `PRISON.EGG` has 13
textures, and its table line lists 12 file names and one name without extension
(`g_a01`).

### Model data block

The arrays follow each other in this order. `n` is `n_models`, `F` is `n_frames`.
Order and sizes come from the page-in code and match every model in all three files
(the 100 blocks are contiguous and end 18 bytes before the end of the file).

| # | Size | Content | Condition |
|---|---|---|---|
| 1 | `model_data_size` | interpreter data, Descent 1 opcode stream | always |
| 2 | 4n | `submodel_ptrs`, offsets into the interpreter data | always |
| 3 | 12n | `submodel_offsets` | always |
| 4 | 12n | `submodel_norms` | always |
| 5 | 12n | `submodel_pnts` | always |
| 6 | 4n | `submodel_rads` | always |
| 7 | n | `submodel_parents` (0xFF for the root) | always |
| 8 | 12n | `submodel_mins` | always |
| 9 | 12n | `submodel_maxs` | always |
| 10 | 12(F+1) | vec per frame | always |
| 11 | 12(F+1) | vec per frame | always |
| 12 | 6nF | animation angles, one angvec (3 x s16) per submodel per frame | F > 0 |
| 13 | 7 x 12F | seven arrays of one vec per frame | F > 0 |
| 14 | 12(F+1), 4(F+1), 4(F+1) | three arrays | flags & 1 |
| 15 | 12F, 4F, 4F | three arrays | flags & 8 |
| 16 | 12F, 4F | two arrays | flags & 2 |
| 17 | 12F, 4F | two arrays | flags & 4 |
| 18 | n(n+1) | child lists | always |
| 19 | n | one byte per submodel | always |

Note the order of the flag tests: 1, 8, 2, 4.

Verified by data: every `submodel_ptrs` value lies inside the interpreter data. The
interpreter data starts with opcode 4 (SORTNORM) or 7 (DEFP_START), as in Descent 1.
Names of arrays 2 to 9 are the Descent 1 `polymodel` names, matched by position and
size. The port draws models with these arrays and the interpreter data (pictures of
`barrel01.pof`, `kettle3.pof`, `durnan.pof` and `hmc__p.pof` from `test_dtudata`), so
the opcode stream is the Descent 1 one: only the opcodes 0 to 8 occur in the
archives, with the Descent 1 record sizes. The port does not call
`g3_init_polygon_model` on the data. Guess: the flat polygon colors are already
palette indexes, as in the Descent 1 PIG file. Guess: arrays 10 and 11 are per-frame bounding box mins and maxs (the values
repeat the model bounds). Guess: array 18 holds, for each submodel, a list of child
submodel numbers ended by 0xFF (checked by hand against `submodel_parents` for
`kettle3.pof` and `bed_rd.pof`), the row width was not confirmed in code. The meaning
of arrays 13 to 17 and 19 is unknown.

## BIN files

`HMC.BIN`, `HMT.BIN` and `YPINN.BIN` are encoded table files, the Undermountain form of
Descent 1 `BITMAPS.BIN`. The table text and the way the game picks the table and the
archive of a level are in [tbl.md](tbl.md).

| Offset | Size | Content |
|---|---|---|
| 0 | 13 | name of the EGG file this table belongs to, NUL padded (`hmc.egg`, `hmt.egg`, `portal.egg`) |
| 13 | rest | encoded table text |

Decoding: byte 0x0A is a line end and is stored as is. Every other byte `b` decodes to
`rol8(rol8(b) ^ 0xD3)`, where `rol8` rotates left by one bit. Verified: all three
files decode to printable text with no stray bytes.

The text is a `BITMAPS.TBL` style table with `$` sections. `YPINN.BIN` has 2499 lines
with `$PLAYER`, `$OBJECT`, `$WEAPON`, `$ARMOR`, `$SPELL`, `$MONSTER`, `$MONSTER_AI`,
`$DRESSING`, `$VCLIP`, `$WCLIP`, `$ECLIP`, `$TEXTURES`, `$WALL_ANIMS`, `$SOUND`,
`$INTERFACE`, `$ICONS`, `$EFFECTS`. `HMC.BIN` and `HMT.BIN` have one `$PLAYER` section
and six `$SOUND` lines.

The table refers to EGG entries by file name:

- bitmaps as `name.bbm` (single) or `name.abm` (animation). The EGG name is the file
  name without extension. Of 1097 bitmap names in `YPINN.BIN`, 1095 exist in
  `PORTAL.EGG`. 45 EGG bitmap names are not named in the table text.
- models as `name.pof`, equal to the model header name. 55 of 56 match.
- sounds as `$SOUND <number> <number> name.rca`. The EGG sound name is the file name
  without extension.

So the EGG has no numeric texture ids. Texture numbers used by levels come from the
order of the entries in the table, as in Descent 1. The rules are in tbl.md. They are
verified against the texture name table of all 34 level files that have a table.

## FRONTEND.TBL and FEBITMAP.TBL

Both are plain text (CRLF, `;` comments), not encoded. `FRONTEND.TBL` starts with the
EGG name `frontend.egg` and then lists include files (`febitmap.tbl`, `feweapon.glb`,
`fearmor.glb`, ...). `FEBITMAP.TBL` lists the frontend bitmaps. Its comments say that
these bitmaps do not follow the 64 x 64 rule. `FRONTEND.EGG` is not in the install.
Guess: these two files are source tables left over from the build and the frontend art
went into `PORTAL.EGG` (the 0x40 flag bitmaps).

## PALETTE.256

9472 bytes: 768 bytes of palette (256 x RGB, 6 bit VGA values, maximum value 63) and
34 fade tables of 256 bytes, same layout as Descent 1. The last fade table is the
identity table. Verified by size, value range, and by rendering in-game textures.

## Unknown

- The last 18 bytes of each EGG file are not covered by any entry and the loader does
  not read them. `HMC.EGG` 0x164F82..0x164F94, `HMT.EGG` 0x16603C..0x16604E,
  `PORTAL.EGG` 0xBAF8A2..0xBAF8B4. The content looks like uninitialized memory. In
  HMC and HMT it is `BF 05 00 FF` repeated.
- The palette for the bitmaps with flag 0x40.
- Sound flag 2. The sample rate of the raw sounds of the old layout.
- Model header fields 0x2C, 0x30 and 0x4A to 0x65, and model data arrays 13 to 17
  and 19. The order of array 12 (frame by frame or submodel by submodel).
- Where a model keeps its gun points (Descent 1 read them from the POF file).
- How the object bitmap slots of a player archive and of a level archive stay apart
  in DTUGAME.EXE (see "Model header").
- The old loader path of DTUGAME.EXE for `DEMO.EGG`, `FRONTEND.EGG`, `SHORT.EGG` and
  `DTU.EGG`.

## Check with the port loader

`build-d1/engine/main/test_dtudata -sweep DIR DIR` loads each table of a directory
with its archive and prints the counts. Result for the 85 tables and 57 archives of
the disc, read from the unpacked files and again from the packed files:

- 81 tables load. Their archives have 0 invalid bitmaps, sounds and models, but for
  `bless#6` of `MULHOR.EGG`.
- `DMM`, `FFM`, `FMM`: the table file is empty (the `.HEN` file has 2 bytes). The
  archives load alone: 44, 0, 33 entries, 68, 12, 48 (old layout) and 68, 0, 48.
- `MOIN1.BIN` names `mon1.egg`. The disc has no such archive.
- `BASE.EGG`, `SPIDER.EGG` and `DEMO.EGG` have 215, 222 and 238 models. The limit of
  DTUGAME.EXE is 160 (`N_polygon_models < MAX_POLYGON_MODELS`), so the 1.3 game cannot
  load them. The port has a limit of 256.
- The counts of `HMC.EGG` (31, 6, 21) and `PORTAL.EGG` (3580, 121, 58) are those of
  `tools/egg.py list`.

Limits in DTUGAME.EXE (constants of the asserts): 4000 bitmaps, 5500 sounds, 160
models, 64 submodels in a model, 50 textures in a model, 2 guns.
