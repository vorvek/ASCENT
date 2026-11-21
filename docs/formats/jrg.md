# JRG index files, their DAT data files, and related small files

Status: layouts verified against the files of the version 1.3 install with
`tools/jrg.py check`. Every index is consumed to its last byte and every data
file is covered by its index with no gap and no overlap, except where a
section says otherwise.

All integers are little endian unless a section says otherwise.
"Verified" means checked against every file of that type in the install.
"Guess" means not verified.

A `.JRG` file is an index. The data is in the `.DAT` file with the same base
name. There is no common JRG header: each index has its own record layout and
the game selects the layout by file name. "JRG" also appears as a prefix of
debug messages in DTUGAME.EXE (`JRG: Switching to mode 13h`), so it is probably
the initials of a programmer (guess).

| Index | Data | Content | Entries |
|---|---|---|---|
| FEBGND.JRG | FEBGND.DAT | LZSS compressed 8-bit images with palette | 196 |
| FESEQ.JRG | FESEQ.DAT | IFF ANIM brushes | 45 |
| FESTREAM.JRG | FESTREAM.DAT | raw 8-bit animation frames | 45 sequences, 533 frames |
| FETXTx.JRG | FETXTx.DAT | nibble-swapped text strings | 489 per language |
| WERDZ.JRG | WERDZ.DAT | Interplay ACM audio, mono | 302 |
| WHAMMO.JRG | WHAMMO.DAT | Interplay ACM audio, stereo | 74 |
| DISCO.JRG | (the .TUN files) | music file name table | 40 |
| FESOUND.JRG | not in the install | index only | 300 |
| GABx.RLH | GABx.DAT | encrypted dialogue scripts | 87 (E), 86 (others) |

The language letter `x` is one of E, F, G, I, S, N, M. DTUGAME.EXE holds the
string `efgisnm` next to `gabe.dat` and `gabe.rlh`, and replaces the fourth
letter of the file name with the letter for the current language.

## FEBGND.JRG and FEBGND.DAT (front end images)

Index:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | entry count (196) |
| 4 | 16 * count | entries |

Entry:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | offset of the entry in FEBGND.DAT |
| 4 | u32 | image width in pixels |
| 8 | u32 | image height in pixels |
| 12 | u32 | stored size of the entry in bytes (palette plus compressed pixels) |

Data entry:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 768 | palette, 256 times (red, green, blue), 6-bit VGA values 0 to 63, not compressed |
| 768 | size - 768 | pixels, LZSS compressed, width * height bytes after decompression, row by row from the top, one palette index per pixel |

The compression is the classic Okumura LZSS (the game source file is `felz.c`):

- Ring buffer of 4096 bytes, filled with 0x20, write position starts at 0xFEE.
- Read one flag byte, then use its bits from bit 0 to bit 7.
- Flag bit 1: copy one literal byte.
- Flag bit 0: read two bytes `lo`, `hi`. Ring position = `lo | ((hi & 0xF0) << 4)`,
  length = `(hi & 0x0F) + 3`. Copy `length` bytes from the ring.
- Every output byte is also written to the ring.
- The stream ends at the end of the entry. Unused bits of the last flag byte are ignored.

Verified: all 196 entries decode to exactly width * height bytes and use
exactly `size - 768` input bytes. All palette bytes are 63 or less. Entries 0,
2, 3, 40 and others were written to PNG and inspected: they are correct
pictures (character portraits, 640x480 menu screens, buttons).

Image sizes found: 640x480 (35 entries), 100x90 (32), 50x45 (32), 160x160 (30)
and 22 other sizes.

Unknown: which entry number is used for which screen. The exe message
`JRG: Error loading PCX file %u  (LZ %u)` shows that the game addresses entries
by number.

Known entries of the in-game interface (screens of each mode, icons, cursor,
portraits, button sheets): see [../re/ingame-ui.md](../re/ingame-ui.md).

## FESEQ.JRG and FESEQ.DAT (front end animations)

Index:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | entry count (45) |
| 4 | 8 * count | entries: u32 offset in FESEQ.DAT, u32 size |

Each data entry is one complete IFF file of type `FORM ANIM` as written by
Deluxe Paint for an animated brush. IFF integers are big endian. The size
field of the outer FORM plus 8 equals the entry size (verified for all 45).

Structure of an entry:

```
FORM ANIM
  FORM PBM            first frame
    BMHD (20)         bitmap header
    CMAP (768)        palette, 256 * RGB, 8-bit values
    GRAB (4)          hot spot x, y (u16 each)
    CRNG (8) * 16     colour cycling ranges
    DPAN (8)          Deluxe Paint private data
    TINY              thumbnail
    BODY              pixels of the first frame
  FORM PBM            one per further frame
    ANHD (40)         animation header, first byte (operation) is 0x4B
    DLTA              difference to the previous frame
```

BMHD fields used: width (u16), height (u16), x (s16), y (s16), planes (u8,
always 8), masking (u8, always 2), compression (u8, 0 in 40 entries and 1 in
5 entries), pad (u8), transparent colour (u16), aspect x and y (u8, 5 and 6),
page width and height (s16, 320 and 200).

BODY: one byte per pixel, rows from the top, each row padded to an even
number of bytes. Compression 1 is ByteRun1: control byte `c`; `c` < 128 copies
the next `c + 1` bytes; `c` > 128 repeats the next byte `257 - c` times;
`c` = 128 does nothing.

DLTA (the same coding that the Descent 1 function `parse_delta` in `iff.c` reads):

| Size | Meaning |
|---|---|
| u32 BE | always 4 in this file, meaning unknown |
| per row | u8 item count, then that many items |

Item, first byte `code`:

- `code` = 0: run. Next byte = repeat count, next byte = value.
- `code` > 0x80: skip `code - 0x80` pixels (they keep the value of the previous frame).
- otherwise: `code` literal bytes follow.

The items of one row add up to the image width. When the width is odd they
add up to width + 1 and the last pixel of the row is dropped (for a literal
the extra byte is still present in the stream).

Verified: all 45 BODY chunks decode to the exact size. All 231 DLTA chunks
decode with every row adding up as described, and end at the chunk end or one
pad byte before it. Frames of entry 21 (four portraits, 108x120) were written
to PNG and inspected.

Not parsed: GRAB, CRNG, DPAN, TINY and the rest of ANHD. They are standard
Deluxe Paint chunks and are not needed to get the pixels. Whether the game
uses the BMHD transparent colour was not checked.

## FESTREAM.JRG and FESTREAM.DAT (streamed front end animations)

Index:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | value 1, meaning unknown (guess: version) |
| 4 | u32 | sequence count (45) |
| 8 | ... | sequences, variable size |

Sequence:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | x position on the 640x480 screen |
| 4 | u32 | y position on the screen |
| 8 | u32 | frame width |
| 12 | u32 | frame height |
| 16 | u32 | always 0, meaning unknown |
| 20 | u32 | frame count (10 to 13) |
| 24 | 8 * frame count | frames: u32 offset in FESTREAM.DAT, u32 size |

A frame is width * height bytes of raw palette indexes, rows from the top,
not compressed. FESTREAM has no palette of its own.

Verified: the index parses to its last byte with 45 sequences and 533 frames.
The frames are contiguous and cover all 23089582 bytes of FESTREAM.DAT. Every
frame size equals width * height.

Palette and position: comparing the first frame of each sequence with the
same rectangle of every 640x480 FEBGND image gives one clear match per group:
sequences 0 to 35 match FEBGND entry 2, sequences 36 to 43 match entry 3,
sequence 44 matches entry 4. Frames of sequences 0 and 36 drawn with those
palettes were inspected and look correct. So x and y are screen positions on
that background and the palette is the one of the background. This is derived
from the pixel data, not from the game code.

## FETXTx.JRG and FETXTx.DAT (front end text)

Index: no header. The file is a list of 8-byte entries (u32 offset in the DAT
file, u32 size) and the entry count is the file size divided by 8 (489 in all
seven languages).

Data: every byte has its two nibbles swapped (`b = (b << 4 | b >> 4) & 0xFF`).
After the swap each entry is one string with a terminating NUL byte. The size
includes the NUL. Line breaks are single LF bytes (0x0A).

Verified for all seven languages: entries are contiguous and cover the whole
DAT file, every entry ends in NUL and has no other NUL, and the only bytes
outside 0x20 to 0x7E are LF and NUL. Example, entry 0 of FETXTE:
"The most populous and\nstrongest of the major\nraces, humans are ...".
The F, I and S files are not translated: they hold the English text with a
marker such as `FRENCH FRENCH FRENCH` in front of entry 0.

DTUGAME.EXE also holds the names `fetxt.jrg` and `fetxt.dat` with no language
letter and the pattern `%s.jrg`. How the name is built was not traced.

## WERDZ.JRG, WERDZ.DAT, WHAMMO.JRG, WHAMMO.DAT (speech and sound effects)

Index:

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | entry count (WERDZ 302, WHAMMO 74) |
| 4 | 8 * count | entries: u32 offset in the DAT file, u32 size |

Each data entry is one complete Interplay ACM stream, the same format as a
`.TUN` file. See [tun.md](tun.md).

- WERDZ.DAT: 302 streams, all mono, 22050 Hz, level 6, 8 rows. The string
  `werdz.jrg` is next to `str_dlg.c` in the exe (guess: dialogue speech).
- WHAMMO.DAT: 74 streams, all stereo, 22050 Hz, level 7, 16 rows. The string
  `whammo.jrg` is next to `str_sfx.c` in the exe (guess: streamed sound effects).

Verified: entries are contiguous and cover the whole DAT file, and every entry
starts with the ACM signature. For the result of decoding every entry see
tun.md.

Unknown: which entry number belongs to which line of speech or which effect.

## DISCO.JRG (music table)

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | entry count, 40. The game stops with an error when it is not 40 (compare at exe address 0xA3760) |
| 4 | 20 * count | entries |

Entry:

| Offset | Size | Meaning |
|---|---|---|
| 0 | 16 | file name, NUL terminated, NUL padded |
| 16 | u32 | not read by the version 1.3 executable (see [../re/music.md](../re/music.md)). Values found: 0 (35 entries), 1 (2), 2 (2), 4 (1) |

Verified: 804 bytes = 4 + 40 * 20, no bytes after the NUL of a name other
than NUL, and every name that is not empty is a file in the install (compared
without case). Entry 3 has an empty name. `khelben.tun` is listed twice with
values 1 and 2. 15 of the 52 .TUN files are not in this table (FECGEN,
FECREDIT, FEWDEEP, IGACT1 to 3, IGBGND1 to 3 and the six TAG files), so the
game must name those in another way.

The entry number is the track number of the music code: the game code and the music
commands of `DTU.WRP` name a track by it (verified, see [../re/music.md](../re/music.md)).
The executable has no other source of `.TUN` names, so it does not play the 15 files
that are not in this table.

## FESOUND.JRG (index without data)

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | entry count (300) |
| 4 | u32 | 22050 (guess: sample rate) |
| 8 | 8 * count | entries: u32 offset, u32 size |

Verified: 2408 bytes = 8 + 300 * 8. The first offset is 8 and each entry
starts where the previous one ends. The data file would be 127846604 bytes.
The offsets starting at 8 suggest that the data file began with the same
8-byte header (guess).

There is no FESOUND.DAT in the install, and DTUGAME.EXE does not contain the
string `fesound` in any case. Guess: this is a leftover of the uncompressed
sound data from development, and WERDZ/WHAMMO replaced it. The entry count is
300 while WERDZ has 302 entries, so the two are not simply the same list.

## GABx.RLH and GABx.DAT (dialogue scripts)

GABx.RLH is the index. No header. Entry count = file size / 8.

| Offset | Size | Meaning |
|---|---|---|
| 0 | u16 | record id. Only the low 9 bits are an id in the file (ids 1 to 511). The game uses bit 13 and bits 9 to 12 of this word in memory as "loaded" flag and slot number (exe address 0xAB9C9) |
| 2 | u16 | record size in bytes |
| 4 | u32 | record offset in GABx.DAT |

Each record in GABx.DAT is encrypted on its own. To decrypt byte `i` of a
record (i counts from 0 at the start of the record):

```
key = "Although curiosity killed the cat, it still had 8 lives left to live." + NUL   (70 bytes)
plain[i] = cipher[i] ^ key[i % 70] ^ key[(0xFFFFFFFF - i) % 70]
```

The routine is at exe address 0xA74F8 and the key at 0xA63CA. The second
index is the unsigned 32-bit value of `~i` modulo 70.

A decrypted record is a script: control bytes of 0x80 and above mixed with
NUL-terminated text, for example
`CB "3e7" 00 CD "Lolth has warned me of your coming. ..." 00 CF "1" 00 "Did Lolth tell you ..." 00`.

Verified: for all seven languages the records are contiguous and cover the
whole DAT file, and every decrypted record starts with byte 0xCB and ends
with byte 0xCC. The decrypted text is readable English. GABE has 87 records
(369929 bytes). The other six languages have 86 records and their DAT and RLH
files are identical to each other.

Unknown: the meaning of the control bytes. From the text alone, 0xCB seems to
introduce a label, 0xCD a line spoken by the character, 0xCF a numbered
answer of the player and 0xCC the end of the record (all guesses). The tool
writes the decrypted record and a text file with the printable strings. It
does not parse the script.

## GELDOR.DAT

210 bytes, 21 records of 10 bytes. The game reads 10-byte records until the
end of the file (exe address 0x872C3) and passes the four fields to one
function.

| Offset | Size | Meaning |
|---|---|---|
| 0 | i32 | unknown, -1 or a small positive number |
| 4 | u8 | unknown (values 15, 5, 16) |
| 5 | u8 | unknown |
| 6 | i32 | unknown, 1 in every record |

Guess: the stock of a shop (the string `geldor.dat` is next to `femarket.c`
in the exe): item class, item number, price or limit, count.

## KCONFIG.DAT

386 bytes. The game transfers a 384-byte table (exe data address 0x1E9A88)
and then two single bytes (0x13BFED and 0x13BFEF) with three calls in the
function at exe address 0x7190F.

| Offset | Size | Meaning |
|---|---|---|
| 0 | 384 | control configuration table. Most bytes are 0xFF (guess: "not bound"). The start looks like pairs of key scan codes |
| 384 | u8 | unknown (5 in the install) |
| 385 | u8 | unknown (8 in the install) |

The layout inside the 384 bytes is unknown. This file is written by the game
(user settings), so it is not game data that the port must read.

## Tool

`tools/jrg.py` (Python 3, standard library only):

```
python -I tools/jrg.py list FILE
python -I tools/jrg.py extract FILE OUTDIR [--raw] [--only N,N,...] [--palette N]
python -I tools/jrg.py check GAMEDIR [--deep]
```

`check` reports OK for all 23 index and record files of the install.
