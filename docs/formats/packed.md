# Disc layout and packed files (DOG, COW, HEN)

## Disc layout

The disc is one ISO 9660 data track with 2048-byte sectors.
`tools/iso.py` lists and extracts the files.

| Directory | Files | Content |
|---|---|---|
| `DTU` | 519 | The game data. |
| `DTU/STUFF` | 121 | WAV, PCX, IFF, TGA and BMP files. Their use is unknown. |
| `MANUAL` | 15 | The manual and the Acrobat readers. |
| `DEMOS` | 13 | A demo of a different product. |

The installer copies only a part of `DTU` to the hard disk.
The game reads the other files from the path in `CDPath` of `DTU.CFG`.

## Packed file types

The disc has no `.LVL`, `.EGG` or `.BIN` files. It stores them in packed form.

| Disc file | Unpacked file | Count on disc | Content |
|---|---|---|---|
| `NAME.DOG` | `NAME.LVL` | 35 | Level. See `lvl.md`. |
| `NAME.COW` | `NAME.EGG` | 57 | Bitmap, sound and model archive. See `egg.md`. |
| `NAME.HEN` | `NAME.BIN` | 85 | Encoded `BITMAPS.TBL` text. See `egg.md`. |

Verified: the unpacked `YPINN.DOG`, `YPINN.HEN`, `HMC.COW`, `HMC.HEN`, `HMT.COW` and `PORTAL.COW`
are equal to the files of a version 1.3 install, without the last 18 bytes of each install file.

The 18 bytes at the end of each install file are not in the packed data.
18 is the maximum match length of the compression.
Guess: the unpack code of the game writes one buffer of 18 bytes too many.

## Compression

The compression is a variant of LZSS. `tools/unpack.py` unpacks it.

| Offset | Size | Meaning |
|---|---|---|
| 0 | 2 | Low 16 bits of the unpacked size, little endian. Verified on all 177 files. |
| 2 | to end | Compressed stream. |

The stream is a sequence of groups. A group is one flag byte and then 8 items.
The decoder reads the flag bits from bit 0 to bit 7.

- Flag bit 1: the item is one literal byte.
- Flag bit 0: the item is a 16-bit little endian word.
  The match distance is `word >> 4` (1 to 4095), counted back from the end of the output.
  The match length is `(word & 15) + 3` (3 to 18).
  A match can overlap the bytes that it writes.

The stream has no end marker. The decoder stops at the end of the file.
The full unpacked size is not stored.

This variant is different from the LZSS of the front end data (`jrg.md`).
That variant uses a ring position. This variant uses a distance.

## Level set

`DTU.MSN` lists 33 levels. The disc has 35 `.DOG` files.

- 31 of the 33 mission levels are on the disc.
- `MADLANDS.LVL` and `PLAYGRND.LVL` are in the mission list but not on the disc.
- `IBBALAR1`, `PRISON1`, `SPIDER` and `THIEF` are on the disc but not in the mission list.
- `IBBALAR1`, `PRISON1` and `THIEF` have game data version 40. All the other levels have version 41.
- The four `.LVV` files of the disc (`BANE`, `FLAME`, `IBBALAR`, `THIEF2`) are equal to the `.LV2` files of the 1.3 patch.

`tools/lvl.py` parses the 32 levels of version 41 with zero unexplained bytes.

## Archive set

The 57 `.COW` files are of two groups.

- Level texture sets: `BANE`, `BASE`, `BAT`, `BEHOLDER`, `BONES`, `CLAN`, `DROW`, `FLAME`, `IBBALAR`,
  `KOBOLDS`, `MULHOR`, `MYRKUL`, `NETHER`, `PORTAL`, `PRISON`, `SPIDER`, `THIEF`, `DEMO`.
- Player character sets with a 3-letter name, for example `HMC` and `HMT`.
  Guess: the letters are the race, the sex and the class.

Each `.HEN` file starts with the name of the EGG file that it uses.
For example `YPINN.HEN` names `portal.egg`.

`tools/egg.py check` passes on 54 of the 57 unpacked archives. These three fail:

| File | Error | Note |
|---|---|---|
| `FFM.EGG` | RLE data outside file | The table counts are 68, 12, 48. The other character sets have 31, 6, 21. The sound table does not parse. |
| `MULHOR.EGG` | Bitmap `bless#6` row 0 has 0 pixels | One bitmap of 3340. |
| `DEMO.EGG` | RLE data outside file | Not examined. |

The cause is unknown for each of the three files.

## Unknown

- `NAME.EOI`: 26 files, 12800 bytes each.
- `*.THN`: 8 files, 9480 bytes each, magic `THNL`.
- The files in `DTU/STUFF`.
