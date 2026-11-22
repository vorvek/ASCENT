# DTUGAME.EXE: the layout of the linear executable

Status: verified on the `DTUGAME.EXE` of version 1.3 (1760375 bytes, the file of the
install and of the 1.3 patch archive). The file of the disc (1713347 bytes) is an older
version and was not checked. `tools/wiki_rules.py` (class `Exe`) reads tables of the
program with this layout.

The program is a DOS/4GW linear executable (LE) made by Watcom C. The analysis image of
the reverse engineering notes (`gamedata/re/dtu_flat.bin`, docs/re) is this file loaded
at its object bases with the fixups applied.

## Header

The MZ stub has the offset of the LE header in the dword at 0x3C: 0x2C90. The header
starts with `LE`. Fields used (offsets from the LE header):

| Offset | Value | Meaning |
|---|---|---|
| 0x14 | 320 | number of pages |
| 0x28 | 4096 | page size |
| 0x2C | 2167 | bytes of the last page |
| 0x40 | 196 | object table (from the LE header) |
| 0x44 | 2 | number of objects |
| 0x48 | 244 | page map (from the LE header) |
| 0x80 | 451584 (0x6E400) | first data page (from the start of the file) |

## Objects

Object table entries of 24 bytes: virtual size, base address, flags, first page (from 1),
number of pages, reserved.

| Object | Virtual size | Base | Flags | Pages |
|---|---|---|---|---|
| 1 (code) | 0x10B9EE | 0x10000 | 0x2045 | 1 to 268 |
| 2 (data) | 0x3706E0 | 0x120000 | 0x2043 | 269 to 320 |

The page map has one entry of 4 bytes for each page. Bytes 0 to 2 are the page number,
high byte first; byte 3 is 0. In this file the entries are in order (entry 1 is page 1,
entry 269 is page 269).

## A linear address to a file offset

For an address A in object k: page = (A - base) / 4096, the file offset is
`0x6E400 + (pagemap[first + page] - 1) * 4096 + (A - base) % 4096`. Object 2 has 52
pages (212992 bytes) of its 0x3706E0: the rest is zero filled memory with no bytes in
the file.

Verified: the bytes at 0x145C04 (53 ints), 0x1458B8, 0x13A956, 0x145CD8, 0x1461C8 and
0x1403D2 read this way are the same as in `dtu_flat.bin`.

## Fixups

A pointer in the data has the offset inside its target object in the file; the loader
adds the base. Example: the dword at 0x145692 (the first row pointer of the cleric
saving throws) is 0x00025642 in the file and 0x145642 in the loaded image (+0x120000).
So tables of pointers (0x146590, 0x13FD8F, 0x145692 ...) differ between the file and the
image, and a reader takes the rows they point to by address. Tables of numbers have no
fixups.
