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

The table of the loader, the command codes and their bits are in
[wrp.md](wrp.md).

Verified: all 628 data lines match the five-number form. No other line forms exist.

Verified: the warp lines into KOBOLDS (level 2) and out of it are `1,91 2,1 2`
(Hub1 to Kobold) and `2,12 1,102 0` (Kobold to Hub1). The player object of
KOBOLDS.LVL is in segment 12, the segment of the warp out. The first line into
HUB1 is `0,118 1,136 2` (from YPINN); the HUB1 player object is in segment 136.
The port starts `-level NAME` at the destination of the first line into the
level (see `docs/audit/world.md`, W20).

## DTU.SNG: CD track table

Plain text, CRLF. `;` starts a comment (also after the numbers). Each data
line is parsed with `"%hd %u"` (string at 0x12B256, `songs.c`):

    track   seconds

Lines are in music item order: title, briefing, end level, end game, credits,
then level 1 to 22. `track` is the CD audio track, `seconds` is the play time
before the track restarts. Verified: 27 data lines. The code that uses the table plays
CD audio tracks and not the `.TUN` music (see [../re/music.md](../re/music.md)).

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

The game "decodes" each record (0x87BB7) by swapping bytes 0..3 with 6..9 and
byte 4 with byte 5, and writes the result to `geldor.dat`. The decoded record
is: s32 quantity (-1: no limit), u8 class, u8 id, u32 count of one item (1,
or -1 for "full", the charges of the item for a type that is not
stackable). The (class, id) pair is the same item key that triggers use
(`need_item_class`, `need_item_id` in `tig.md`).

Verified: 21 records, classes 15, 5 and 16. `THESTORE.1` decodes to
`GELDOR.DAT` of the install byte for byte, and the stock that the market of
the port writes for the first stock is equal to `GELDOR.DAT`. Tool:
`sidefiles.py store`. The market is in `docs/re/frontend.md` (section 6a).

Class 17 (a spell, the scroll of the spell) is also a stock line: 0x873EE
puts it into the misc list, and the market writes it back.

Two records of the same good (class, id, and for a stackable type any
count) become one line of the market: 0x870A0 adds the quantities, and a
quantity of -1 (no limit, shown as "many") stays -1. Verified in the port
(tools/uitest.py market_arrow_stock: a stock with the records 50 and -1 of
the Arrows +1 is written back by the market as one record -1). The new gear
uses this to make a good "many" at a later reward.

### The ASCENT trailer (the port only)

The port (2026-11-27, `Dtu_fix_market_reward`) ends `GELDOR.DAT` and
`THESTORE.<n>` with 8 bytes after the last record. They are not a record and
are not coded by 0x87BB7:

| Offset | Type | Meaning |
|---|---|---|
| 0 | 4 bytes | `ASRW` |
| 4 | u16 | the rewards of Khelben (0x13F6A0) that the stock holds: bit n for reward n, 0 to 8 |
| 6 | u16 | 0 |

So the size of a file of the port is 10 x records + 8, and the size of a 1.3
file is 10 x records. A file of no trailer (1.3, or the port before
2026-11-27) holds the rewards of its mission if it has a record of count -1
(only the rewards have one), else none: that is the rule of the port for
such a file (dtufe_town.c fe_market_check_rewards).

1.3 reads a file of the port: 0x872C1 reads 10-byte records while the EOF
flag (0x10) of the stream is clear; the short read of the 8 bytes returns 0
records, and the fill of the buffer (0xF3104) sets the EOF flag when the
read returns 0 bytes (0xD82DD sets it too), so the stock stays and the
trailer is not read. 0x879A7 seeks to `size - size % 10` and writes its
records over the trailer; the market of 1.3 (0x86846) and the save copy
(0x87BB7) write records only. Read in the pseudocode of these functions;
not run in 1.3. Verified in the port: a stock with the trailer goes through
a save (`THESTORE.<n>`) and a load with it (tools/uitest.py
`test_market_reward_repair`, `test_market_reward_khelben`). Tools:
`sidefiles.py store` and `jrg.py list` print the trailer.

## SG0.SG: save game

Magic `DGSS`, written by the state code derived from Descent 1 `state.c`
(writer at 0xBAA2A). Size of the shipped file: 7386 bytes.

| Offset | Size | Meaning | Status |
|---|---|---|---|
| 0 | 4 | `DGSS` | verified |
| 4 | 4 | u32 version, 12 | verified |
| 8 | 20 | description, NUL terminated ("Empezamos"), at most 19 characters (the menu item of 0xBA486). 1.3 can type only the ASCII characters of its US key table but `&`; the port also writes the Latin-1 letters of the fonts, one byte each (`docs/re/controls.md`, "Typed text"; checked in a save of the port: `Pe F1 ED scola`) | verified |
| 28 | 4 | u32 "between levels": 1 = saved in Waterdeep, 0 = saved in a level (the loader 0xBB780 goes to Waterdeep or starts the level) | value verified, meaning read in the loader |
| 32 | 9 | mission name, "DTU" | verified |
| 41 | 4 | u32 Current_level_num (0x13B91C), 1 = YPINN | value verified |
| 45 | 4 | u32, 0 | name unknown |
| 49 | 4 | u32, 933299 (Descent 1: game time as fix, 14.24 s) | name is a guess |
| 53 | 4 | u32 count of the mission flags, 19 | verified |
| 57 | 76 | 19 u32 mission flags (0x13CDD4 on, in address order), all 0. Mission flag k is at index k - 1 for k = 1..9, k + 1 for k = 10..17, 9 for k = 18, 10 for k = 19 (docs/re/scripts.md section 7) | values verified, order verified in the code |
| 133 | 8 | u32 Khelben mission (0x13E588), u32 blurb (0x13E58C): 0, 1 | verified (writer 0x83D27, reader 0x83D5E) |
| 141 | 6967 | the player record: one element of the player array, 0x1B37 bytes. The layout is in `docs/re/character.md`: name at +0, hit points at +0x53, the 80 attribute bytes at +0x97, the item list at +0x103 (16 bytes for each entry, count at +0xF3), hot keys at +0x1AC1 | verified: name, attributes, item lists and references decode to a consistent new character |
| 7108 | 80 | a second copy of the 80 attribute bytes: the values without the effect of items (0x187B90 in the executable) | verified (equal to the first copy in this file) |
| 7188 | 20 | `RLPD`, u16 14, u16 25, u32 count of mission records (1), u32 difficulty (2), u32 (1) | values verified |
| 7208 | 10 | per mission record: name, 9 bytes ("DTU"), u8 (1) | values verified |
| 7218 | 144 | all 0 | not decoded |
| 7362 | 12 | u32 difficulty (0x13CDCA, 2), u32 0, u32 0 | values verified |
| 7374 | 12 + records | only when "between levels" is 0: three lists, each a u32 count and its records: `Num_triggers` (0x390DCC) and the Descent 1 triggers (0x390DD0, 54 bytes), `Num_robot_centers` (0x1C7D6C) and the matcens (0x1C5CE8, 16 bytes), `Num_fuelcenters` (0x13B1BC) and the fuel centers (0x1C5E28, 40 bytes). All three are 0 in this file | verified: reader 0xBBD1A to 0xBBDB8; the records have the Descent 1 layout (see below) |

The records of the three lists (verified in the two saves of a played 1.3 game, 7666
bytes each, saved in BONES: counts 0, 0, 7):

- trigger, 54 bytes: u8 type, s16 flags, fix value, fix time, u8 link_num, s16
  num_links, s16 seg[10], s16 side[10] (Descent 1 `trigger`).
- matcen, 16 bytes: int robot_flags, fix hit_points, fix interval, s16 segnum, s16
  fuelcen_num.
- fuel center, 40 bytes: int Type, int segnum, u8 Flag, Enabled, Lives, dum1, fix
  Capacity, MaxCapacity, Timer, Disable_time, vector Center. The saves have one record
  of type 2 (the `special` 2 segment of the level file), one of type 3 and five of type
  5. The level file has only `special` 2; the code that makes the types 3 and 5 was not
  found.

Before the lists the reader sets the s16 at segment +0x210 to -1 for each segment, and
after them the four timers 0x215D74 to 0x215D80 to the game time.

Other fields of the header that the reader uses: offset 49 is read into `GameTime`
(0x13CD30), and 7362 into 0x13CDCA (difficulty), 0x13B7A3 and 0x13B697 (4 bytes each).

A played save of 1.3 (`SG0.SG`, 7666 bytes, level 4) has 20 items, 7 mage spells and
one key item (misc 107 in the key list at P+0x18D3); the hot keys 3 to 8 hold
references to the mage spell list (list 1, index 0 to 5). These parts are in
`docs/re/character.md` (section 6: spells in the hot keys).

Unlike Descent 1 there is no thumbnail image in the file.
The level state is not in this file. It is stored in `SAVE\SGn.LS`,
`SAVE\SGn_m.LVL`, `SAVE\SGn_m.TIG` and `SAVE\SGn_m.TAP`.

The 256 global flags of the scripts are in the player record at P+0x1A33 (file offset 6852), bit n - 1 for flag n; all 0 in SG0.SG (docs/re/scripts.md section 7).

Unexplained ranges: the fields of the player record that `character.md` does not list, 7218..7361 and the records after 7374.

`tools/save_fix.py` lists and edits the item list (count P+0xF3, entries P+0x103) and the key item list (count P+0xFF, entries P+0x18D3). A removal moves the later entries up and changes the references to list 0 (P+0x1AB9, P+0x1ABB, the hot keys P+0x1AC1, the hand P+0x1AD7, the paper doll P+0x1AD9). Verified: the save of 2026-11-26 (`SG1.SG`, 81 entries in the port) without misc 126 loads in the port with 80 entries, the same hot keys, the same worn items (the Ring of Wizardry moved from entry 72 to 71 with its paper doll reference) and the same armor class.

A file with "between levels" 1 ends at 7374. The port writes such a file for the saves of the main menu and of Waterdeep (`engine/main/dtufe.c`) and reads both kinds (docs/re/frontend.md). The port writes the format of the executable: it starts from the bytes of the file that was loaded (so the fields that it does not decode stay) and writes the fields that it has, the three lists included. Verified: a load of the 1.3 save and a save in the port at once give a file that differs from the original only in the description, the game time and the spell slots that the level start fills again.

Correction: an earlier version of this table put the player record at 53. The writer stores 6967 bytes, but the record starts at 141.

## SAVE\CSUF.LS, SAVE\SGn.LS: level state table

600 bytes: 100 records of 6 bytes, one for each level of the mission (the
index in DTU.MSN). The executable reads it with `fread(buf, 6, 100)`
(0xBC2A7, 0x5A7AD). `CSUF.LS` belongs to the running game, `SGn.LS` to save
slot n - 1 (the slot in the menu plus 1).

| Offset | Size | Meaning | Status |
|---|---|---|---|
| 0 | 1 | u8: 1 = the current state of the level is in `SAVE	emp<level>.*` (changed since the last save) | meaning from the save code 0xBC1EC |
| 1 | 1 | u8: 1 = a saved level file exists | meaning from the save code |
| 2 | 4 | s32: the slot number + 1 whose `SAVE\sg<s>_<level>.LVL`, `.TIG`, `.TAP` hold the level | meaning from the save code |

Verified: both files of the install are 600 bytes; record 0 is (0, 1, 1) and
records 1 to 99 are (0, 0, 1). The only level files are `SG1_0.*`. Character
generation (0x87D73) deletes `CSUF.LS` and the `temp*` files and writes a new
`CSUF.LS` with 100 records (0, 0, -1) (0x5AC6F, 0x5ABC6). The code that uses
the files is in `docs/re/levelstate.md`. The port writes and reads the `.LS`
tables and the `.LVL`, `.TIG` and `.TAP` level files in the format of the
executable (`engine/main/dtustate.c`, docs/re/levelstate.md section 3).

Slots: the save menu slot n is `SGn.SG`, the level files `SAVE\SG<n+1>.LS`,
`SAVE\SG<n+1>_<L>.*` and the stock `THESTORE.<n+1>`. Verified in a played 1.3
install: `SG0.SG`, `SAVE\SG1.*` and `THESTORE.1` have the same time stamp, and so
have `SG1.SG`, `SAVE\SG2.*` and `THESTORE.2`. The DOS game writes the names in upper
case.

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

The glyphs at 127 to 135 of the proportional fonts are icons (checked in
FNTEMFM_, FNTECLMS, FNTEMLS_), not letters of code page 437. FNTEMFS_ has the
256 characters of code page 437 (160 is a with acute, 164 n with tilde, 168 the
inverted question mark). The port adds the Latin-1 letters of the translations
at load time: see [lang.md](lang.md).

## KCFG.ENG, KCFG.FRE, KCFG.GER, KCFG.ITA, KCFG.SPA, KCFG.NON, KCFG.MAR

Text files of the CD (1285 to 1327 bytes), CR LF lines. A line that starts with `;` is
a comment. Each other line has a key index (a DOS scan code, 0 to 255), spaces and one
word: the name of that key on the key configuration screen, for example `28  Enter`.
Verified in the files and in the code: 0xAEEE8 reads the file of the language
(0x1456F8) from the table 0x1457AB, which has no entry for English (0): the English
names are those of the code, and `KCFG.ENG` is not read. The port uses the names of
the code (`docs/re/controls.md`).

## DTU.ICO: the icon of the game

On the disc (`DTU\DTU.ICO`, 2998 bytes). The installed game in `E:\UNDAMONTAN\DTU` does
not have it. A Windows icon file. Verified in the file:

| Offset | Size | Value |
|---|---|---|
| 0 | 2 | 0 (reserved) |
| 2 | 2 | 1 (icon) |
| 4 | 2 | 2 (number of images) |
| 6 | 16 for each image | width 32, height 32, colors (16, then 0 for 256), reserved 0, planes 0, bits 0, size in bytes, offset of the image |

| Image | Offset | Size | BITMAPINFOHEADER |
|---|---|---|---|
| 0 | 38 | 744 | 40 bytes, 32 x 64 (the image and the mask), 1 plane, 4 bits for each pixel, no compression |
| 1 | 782 | 2216 | 40 bytes, 32 x 64, 1 plane, 8 bits for each pixel, no compression |

After the header: the palette (16 or 256 entries of blue, green, red, 0), the image
(rows from the bottom up, each row a multiple of 4 bytes: 16 or 32 bytes), then the
mask (1 bit for each pixel, 1 is transparent, rows of 4 bytes from the bottom up). The
sizes add up: 40 + 64 + 512 + 128 = 744 and 40 + 1024 + 1024 + 128 = 2216.

The port (`apply_window_icon`, `engine/platform/video.c`) uses the image with the most
colors for the game window.
