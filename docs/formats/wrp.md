# DTU.WRP: warps, events, music and ambient sounds

Status: verified against DTU.WRP of the version 1.3 install (43813 bytes,
dated 11-5-97 in its header comment) with a Python scan of every line, and
against the loader 0x599C3, the line reader 0x25636, the warp check 0x59C3C
and the command function 0x5B3AC of DTUGAME.EXE 1.3.

The game takes the name from the mission name with the extension `wrp`
(see [misc.md](misc.md), DTU.MSN).

## Lines

Plain text, CRLF, 872 lines: 244 comment lines and 628 data lines. A line
that starts with `;` is a comment (0x25636 reads lines until the first
character is not `;`). A data line is read with the format
`"%d,%d %d,%d %d"` (string at 0x12875E):

    level,segment   dest_level,code_or_segment   value   ;comment

- `level`: the index of the level in DTU.MSN, 0 to 32.
- `segment`: a segment of that level. The highest in the file is 799.
- `dest_level`: a level index for a warp, or -2 for a command.
- `code_or_segment`: the destination segment of a warp, or the command code.
- `value`: the direction of the player after a warp (0 to 3), or the
  argument of the command.

Every data line has the five numbers and a comment after them. The scanf
stops at the `;`. There is no blank line and no line of another form.

The loader stops at the first line, after the comments, for which the scanf
gives less than 4 values. A line with 4 values keeps the `value` of the line
before it (the local variable is not cleared). DTU.WRP has neither case.

## The table of the loader

0x599C3 fills one entry of 4 bytes for each segment, 900 entries at
0x296494, before it reads the file: dest_level 0xFF, code 0xFFFF, value
0xFF. For each line whose `level` is the current level (the low 16 bits are
compared with the level byte), it writes the entry of `segment`:

| Offset | Size | Meaning |
|---|---|---|
| 0 | s8 | dest_level (-1: no line) |
| 1 | s16 | destination segment or command code |
| 3 | u8 | value |

There is no bounds test on `segment`. A later line for the same segment
replaces the earlier one. The file has 17 segments with two lines:

- 13 have a music line and then an ambient line: 10 times 151 and 223,
  for example `7,8 -2,151 35` and `7,8 -2,223 41`, once 106 and 206, twice
  106 and 223. Only the ambient line runs.
- 4 have a sound line and then a game event line (codes 4, 11, 12, 13).
  Only the event runs.

The port keeps both sound lines of a segment by default
(`Dtu_fix_wrp_sound`, engine/main/dtumusic.c; `-origmusic` gives the rule
of the original).

After the read, a segment whose entry has a dest_level of 0 or more gets
the special type 3 (a warp) and an entry in the list at 0x1C5E28, at most
200.

## What the entries do

The warp check 0x59C3C runs when the segment of the player changes
(docs/re/triggers.md section 7):

- dest_level -1: nothing.
- dest_level 0 or more: a warp to `dest_level`, segment `code_or_segment`,
  direction `value` (0 east, 1 west, 2 north, 3 south).
- dest_level -2: 0x5B3AC(code, value). Code 0 leaves the dungeon, codes 1
  to 19 are game events (a jump table), codes 100 to 198 are music commands
  and codes 200 to 298 are ambient commands. 0x5B3AC ignores other codes.

## Music and ambient commands

For a code from 100 to 198, `bits = code - 100`. For a code from 200 to
298, `bits = code - 200`. The two branches of 0x5B3AC (0x5B679 and 0x5B5F2)
test the bits in the same order:

| Bit | Music (str_mus.c) | Ambient (str_sfx.c) |
|---|---|---|
| 0x10 | Stop if `value` plays now (0xA3C37) | Stop if `value` plays now (0xA20E7) |
| 0x08 | Stop if `value` is in the queue (0xA3E5D) | Stop if `value` is in the queue (0xA2406) |
| 0x20 | Loop count -1 (forever), else 0 | Loop count -1 (forever), else 0 |
| 0x02 | Empty the queue (0xA3AB0) | Empty the queue (0xA1FB7) |
| 0x01 | Queue `value` with the volume 100 (0xA3F2E) | Queue `value` with the effects volume (0xA22B9) |
| 0x04 | Fade to 0 (0xA451E, eax 0) | Fade to 0 (0xA291F, eax 0) |

`value` is read as a byte. For the music it is an entry of DISCO.JRG, for
the ambient sounds an entry of WHAMMO.JRG, 0 to 73 (see [jrg.md](jrg.md)).

Codes in the file:

| Code | Bits | Lines | Meaning | Comment in the file |
|---|---|---|---|---|
| 0 | | 1 | leave the dungeon | "YPInn to Waterdeep" |
| 1 to 19 | | 24 | game events | "Mission n check" |
| 106 | 0x06 | 78 | empty the music queue, fade the music out | "Fade out current song" |
| 151 | 0x33 | 235 | if not playing: empty the queue, play in a loop | MUSIC |
| 206 | 0x06 | 59 | empty the ambient queue, fade the ambient sound out | "Fade ambient" |
| 223 | 0x17 | 134 | if not playing: empty the queue, play once, fade the current sound out | AMB |
| 255 | 0x37 | 23 | as 223, but play in a loop | AMB |

The values of the 223 lines are 37 different entries of WHAMMO, those of
the 255 lines are 15, 31, 34, 36, 37, 48 and 52. The value of the 106 and
206 lines is -2.

With 223 and 255 the sound that plays fades out in 1.6 s (from the full
volume 0x7FFF, 0x800 each tenth of a second), and the new sound starts when
it stops. If no sound plays the new one starts at the next service call.
