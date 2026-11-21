# Small text and table files

Short notes on DTU.WRP, DTU.SNG, DTU.MSN, the .TEX string tables, THESTORE.1,
SG0.SG and the .FNT fonts. Each section says what was checked.

## DTU.MSN: mission file

Plain text, CRLF. Same idea as the Descent 1 `.MSN` file. `;` starts a comment.

    name = Undermountain
    type = normal
    num_levels = 33
    ypinn.lvl       ;0
    ...
    flame.lvl       ;32

Keys seen in DTUGAME.EXE (`mission.c`): `name`, `type` (`normal` or `anarchy`),
`briefing`, `ending`, `num_levels`, `num_secrets`. After `num_levels = N` follow
N level file names, one per line. The game also derives the name of the warp
file from the mission name (extension `wrp`).

Verified: the file has 33 level lines. Only 28 of these levels have side files
in the install (no clan2, beholdr2, madlands, ibbalar2, playgrnd).

## DTU.WRP: warp and music table

Plain text, CRLF, 872 lines. `;` starts a comment. Each data line is parsed
with the format `"%d,%d %d,%d %d"` (string at 0x12875E):

    level,segment   dest_level,dest_segment   value

- `level` is the index in DTU.MSN (0..32). `segment` is a segment of that level.
- If `dest_level >= 0` the line is a warp: entering the segment moves the
  player to `dest_level`, `dest_segment`. Log text: "found a warp (seg %d ) to level %d,".
- If `dest_level` is -2 the second number is a command code, not a segment.

Command codes seen, with meaning taken from the comments in the file (guess, not traced in code):

| Code | Lines | Comment in file | value |
|---|---|---|---|
| 0 | 1 | "YPInn to Waterdeep" | 0 |
| 1..19 | 24 | "Mission n check" | 0 |
| 106 | 78 | "Fade out current song" (MUSIC) | -2 |
| 151 | 235 | start music | music item number |
| 206 | 59 | "Fade ambient" (AMB) | -2 |
| 223 | 134 | start ambient sound | sound number |
| 255 | 23 | start ambient sound | sound number |

Verified: all 628 data lines match the five-number form. No other line forms exist.

## DTU.SNG: CD track table

Plain text, CRLF. `;` starts a comment (also after the numbers). Each data
line is parsed with `"%hd %u"` (string at 0x12B256, `songs.c`):

    track   seconds

Lines are in music item order: title, briefing, end level, end game, credits,
then level 1 to 22. `track` is the CD audio track, `seconds` is the play time
before the track restarts. Verified: 27 data lines.

## DTU*.TEX, SETUPE.TEX: string tables

Plain text, CRLF, one string per line, same as the Descent 1 `.TEX` format.
`\n`, `\t` and `\\` are escapes (code at 0x7AF59). The game loads
`<name>.tex` and falls back to `<name>.txb` (the encoded form, no such file ships).
The name depends on the language: `dtue`, `dtuf`, `dtug`, `dtui`, `dtus`, `dtun`,
`dtum` (table at 0x131C04).

The game expects at least 979 strings ("Not enough strings in text file").

| File | Lines | Note |
|---|---|---|
| DTUE.TEX | 985 | English, the file the game loads |
| DTUF, DTUG, DTUI, DTUM, DTUN, DTUS | 984 | other languages |
| DTU.TEX | 836 | too short for this executable, older leftover |
| DTU1.TEX | 781 | too short, older leftover |
| SETUPE.TEX | 75 | strings of SETUP.EXE |

Verified: all are plain text with CRLF, no control bytes, at most 6 bytes above 0x7F.

String numbers: the first 621 strings of DTUE.TEX stand at the places of the 621
strings of Descent 1 (`TXT_` names of `text.h`). Descent to Undermountain changed the
words and kept the number. Sample: 0 "New game" (`TXT_NEW_GAME`), 2 "Quit", 101 "KEYS"
(`TXT_KEYS`), 103 "Abort Game?", 300 "Your level", 501 "Error writing player file.",
502 "Killed!" (`TXT_SHIP_DESTROYED_0`). Some strings have a new use: 104 to 107 are
"Longbow", "Crossbow", "Wand of Magic Missiles", "Wand of Fire" (Descent 1: the names of
the first four weapons), 617 to 619 are class descriptions. The strings from 621 on are
new (900 is "Durnan"). The port loads 979 strings (`text.c`). The new strings have no
names yet.

## THESTORE.1: shop inventory

Read and written by the code at 0x87C05 (file name format `TheStore.%d`, log
names "Encoded Market save" and "Market Inv save"). The file is a list of
10-byte records with no header. Size 210 = 21 records.

Record as stored in the file:

| Offset | Type | Meaning |
|---|---|---|
| 0 | u32 | always 1 in the file (meaning unknown) |
| 4 | u8 | item id |
| 5 | u8 | item class |
| 6 | s32 | quantity, -1 = unlimited (guess from values) |

The game "decodes" each record (0x87BE0) by swapping bytes 0..3 with 6..9 and
byte 4 with byte 5, and writes the result to `geldor.dat`. The decoded record
is: s32 quantity, u8 class, u8 id, u32. The (class, id) pair is the same item
key that triggers use (`need_item_class`, `need_item_id` in `tig.md`).

Verified: 21 records, classes 15, 5 and 16. Tool: `sidefiles.py store`.

## SG0.SG: save game

Magic `DGSS`, written by the state code derived from Descent 1 `state.c`
(writer at 0xBAA2A). Size of the shipped file: 7386 bytes.

| Offset | Size | Meaning | Status |
|---|---|---|---|
| 0 | 4 | `DGSS` | verified |
| 4 | 4 | u32 version, 12 | verified |
| 8 | 20 | description, NUL terminated ("Empezamos") | verified |
| 28 | 4 | u32, 0 (Descent 1: between-levels flag) | value verified, name is a guess |
| 32 | 9 | mission name, "DTU" | verified |
| 41 | 4 | u32, 1 (Descent 1: level number) | value verified, name is a guess |
| 45 | 4 | u32, 0 | name unknown |
| 49 | 4 | u32, 933299 (Descent 1: game time as fix, 14.24 s) | name is a guess |
| 53 | 6967 | player record. The writer stores one element of the player array, 0x1B37 bytes. The player name "Elmac" is at file offset 141 | size from code, content not decoded |
| 7020 | 366 | written by two helper functions (0x4E8C0, 0x71729) and an optional tail of three counted arrays. Contains the strings `hmtd_p.pof` and `RLPD` | not decoded |

Unlike Descent 1 there is no thumbnail image in the file.
The level state is not in this file. It is stored in `SAVE\SGn.LS`,
`SAVE\SGn_m.LVL`, `SAVE\SGn_m.TIG` and `SAVE\SGn_m.TAP`.

Unexplained ranges: 53..7385 (structure known only as described above).

## *.FNT: fonts

Magic `PSFN`. The format is the Descent 1 font format, unchanged.
Tool: `sidefiles.py fnt`.

| Offset | Size | Type | Meaning |
|---|---|---|---|
| 0 | 4 | | `PSFN` |
| 4 | 4 | u32 | datasize: bytes from offset 8 to the end of the font data |
| 8 | 2 | s16 | ft_w, cell width |
| 10 | 2 | s16 | ft_h, cell height |
| 12 | 2 | s16 | ft_flags: 1 = color, 2 = proportional, 4 = kerned |
| 14 | 2 | s16 | ft_baseline |
| 16 | 1 | u8 | ft_minchar |
| 17 | 1 | u8 | ft_maxchar |
| 18 | 2 | s16 | ft_bytewidth (0 in all DTU fonts) |
| 20 | 4 | u32 | offset of glyph data, relative to file offset 8 |
| 24 | 4 | u32 | ft_chars, runtime pointer, ignore |
| 28 | 4 | u32 | offset of the width table, relative to file offset 8 (proportional only) |
| 32 | 4 | u32 | offset of the kerning table, relative to file offset 8 (kerned only) |

- Width table: s16 per character, `maxchar - minchar + 1` entries.
- Glyph data, color font: 1 byte per pixel, `width * ft_h` bytes per glyph.
- Glyph data, mono font: 1 bit per pixel, `((width + 7) / 8) * ft_h` bytes per glyph.
- Kerning table: 3-byte entries (first char, second char, spacing), ended by one 0xFF byte.
- Color fonts end with a 768-byte palette (6-bit VGA values, maximum 63) at `8 + datasize`.

Verified: all 13 files parse with every byte accounted for.

DTUGAME.EXE has a table of ten font names at 0x13B848, two sets in the order of the
five Descent 1 game fonts (big, three medium fonts, small): `FNTECLL_`, `FNTECLMU`,
`FNTECLMS`, `FNTECLMT`, `FNTEMLS_` and `FNTECHL_`, `FNTECHMU`, `FNTECHMS`,
`FNTECHMT`, `FNTEMHS_`. Guess: L and H are the low and the high screen resolution.
`FNTEMFL_`, `FNTEMFM_` and `FNTEMFS_` are loaded by other code (0x45AE0). The port
loads the first set with the Descent 1 font code (`gamefont.c`) and draws text with
it (`test_dtudata` writes a picture).

| File | Size | Cell | Flags | Chars |
|---|---|---|---|---|
| FNTECHL_, FNTECLL_ | 44842 | 35x24 | color, proportional | 32..126 |
| FNTECHMS/MT/MU, FNTECLMS/MT/MU | 9192 | 15x10 | color, proportional | 32..135 |
| FNTEMFL_ | 3769 | 21x18 | proportional, kerned (0 entries) | 32..133 |
| FNTEMFM_ | 2048 | 17x11 | proportional | 32..135 |
| FNTEMFS_ | 2084 | 6x8 | mono, fixed | 0..255 |
| FNTEMHS_ | 1070 | 10x8 | proportional | 32..132 |
| FNTEMLS_ | 844 | 7x6 | proportional | 32..132 |
