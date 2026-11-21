# .MVE: Interplay movies

The movies of the game are Interplay MVE files. The decoder of the port is
`engine/main/mvedec.c`. The public description of the format is the
MultimediaWiki (pages "Interplay MVE", "Interplay Video" and "Interplay
DPCM"). This page lists only what the six files of the game use. Each fact
was checked against the real files with a scan of all chunks and opcodes,
and with a compare of every decoded picture and every sound sample against
ffmpeg (section 6).

## 1. The files

| File | Where | Size | Pictures | Displays | Frame time | Sound |
|---|---|---|---|---|---|---|
| LOGO22S.MVE | install and disc | 5617814 | 385 | 385 | 12500 * 8 = 100000 us (10 fps) | 8-bit PCM, stereo, 22050 Hz |
| TLOGOS.MVE | install and disc | 4015684 | 422 | 422 | 8341 * 8 = 66728 us (14.99 fps) | DPCM, stereo, 22050 Hz |
| INTRO2.MVE | install and disc | 12913540 | 693 | 728 | 66728 us | DPCM, stereo, 22050 Hz |
| FLAM1.MVE | disc | 15248226 | 979 | 979 | 66728 us | DPCM, stereo, 22050 Hz |
| CREDITV1.MVE | disc | 49926640 | 4344 | 4344 | 66728 us | DPCM, stereo, 22050 Hz |
| IPLOGO.MVE | disc | 5617814 | | | | a byte copy of LOGO22S.MVE; no code names it |

The copies in the install (`E:\UNDAMONTAN\DTU`) and on the disc are equal
byte for byte. The disc also has `DEMOS/ENGAGE/*/ENGAGE.MVE` (not part of
the game). Every picture is 432x320 with 8 bits per pixel.

## 2. Header

| Offset | Size | Value |
|---|---|---|
| 0 | 20 | `"Interplay MVE File"`, 0x1A, 0x00 |
| 20 | 2 | 0x001A |
| 22 | 2 | 0x0100 |
| 24 | 2 | 0x1133 |

All values are little endian. The chunks start at offset 26.

## 3. Chunks and opcodes

A chunk is `u16 length, u16 type`, then `length` bytes of opcodes. An opcode
is `u16 length, u8 type, u8 version`, then `length` bytes. The opcodes of a
chunk fill it exactly in every file. The last chunk (type 5) is empty and
ends the file exactly.

Chunk types: 0 sound setup, 1 sound only, 2 video setup, 3 frame, 4 end of
the stream, 5 end. The sequences in the files:

| Chunk | Opcodes | Count |
|---|---|---|
| 0 | 03 01 | 1 |
| 2 | 0A 05 0C (15) 01 | 1 (INTRO2: 7, one for each scene) |
| 1 | 08 09 01 | 9 (LOGO22S) or 14 (the others): sound before the first picture |
| 3 | (02) 0F 08 09 11 13 04 07 01 | the frames; 02 only in the first one |
| 3 | 0F 09 11 13 04 07 01 | the last 9 (LOGO22S) or 14 frames (TLOGOS, FLAM1, CREDITV1): silence instead of sound |
| 3 | 08 09 04 07 01 | INTRO2 only, 21 times: the picture is shown again (no 0F, 11) |
| 3 | 09 04 07 01 | INTRO2 only: its last 14 frames, silence and no new picture |
| 4 | 00 01 | 1 |

Opcodes (only versions that occur):

| Op | Ver | Data | Meaning |
|---|---|---|---|
| 00 | 0 | none | end of the stream |
| 01 | 0 | none | end of the chunk |
| 02 | 0 | u32 rate, u16 sub | timer: a frame lasts rate * sub microseconds |
| 03 | 0 | u16 ?, u16 flags, u16 rate, u16 min buffer | sound format. LOGO22S: flags 1 (stereo, 8-bit), 22050 Hz |
| 03 | 1 | u16 ?, u16 flags, u16 rate, u32 min buffer | the others: flags 0xAA9F or 0xAA57: bit 0 stereo, bit 1 16-bit, bit 2 compressed. The high bits are not flags |
| 04 | 0 | none | start the sound |
| 05 | 2 | u16 w, u16 h, u16 count, u16 true color | the picture size in 8x8 blocks: 54 x 40 = 432x320; count 1; true color 0 (8-bit) |
| 07 | 1 | u16 first, u16 count, u16 ? | show the picture. The first frame: 1, 254, 0; the others 0, 0, 0 |
| 08 | 0 | u16 index, u16 mask, u16 length, data | sound data. Mask 1 (stream 0). length is the size of the decoded sound in bytes |
| 09 | 0 | u16 index, u16 mask, u16 length | silence of length bytes for the streams of the mask: 0xFFFE (the other streams, ignored) or 0xFFFF (stream 0 too) |
| 0A | 0 | u16 w, u16 h, u16 flags | video mode 640x480, flags 0x0101 |
| 0C | 0 | u16 first, u16 count, count * 3 bytes | palette: entries 1 to 254, 6-bit VGA values. Entries 0 and 255 stay black |
| 0F | 0 | 1080 bytes | the block map: 4 bits for each 8x8 block |
| 11 | 3 | 14 bytes, then the block data | the picture |
| 13 | 0 | 132 bytes | not used by the player |
| 15 | 0 | 4 bytes | not used by the player |

The 14 bytes before the block data of opcode 11 are 7 words: the picture
number (from 1), the previous number, x 0, y 0, width 54, height 40 (in
blocks) and a flags word. Bit 0 of the flags is 0 in the first picture of
each scene and 1 in all the others; the other bits vary (LOGO22S 0x32/0x33,
TLOGOS 0/1, the others look random). Bit 0 = 1: the two picture buffers
change places before the decode.

## 4. Video (8 bits per pixel)

The decoder keeps two buffers. A picture is drawn into the buffer that
holds the picture before the previous one; the other buffer is the
previous picture. Opcode 05 clears both buffers. The blocks are in rows
from the top left. The block map gives 4 bits for each block, the low 4
bits of a byte first. The block data is read in order from the data of
opcode 11.

| Code | Data | Block |
|---|---|---|
| 0 | | the same block of the previous picture |
| 1 | | no change (the block of the picture before the previous one) |
| 2 | B | copy from the new picture at (x, y): B < 56: x = 8 + B % 7, y = B / 7; else x = -14 + (B - 56) % 29, y = 8 + (B - 56) / 29 |
| 3 | B | the same as 2 with -x, -y |
| 4 | B | copy from the previous picture at x = (B & 15) - 8, y = (B >> 4) - 8 |
| 5 | x, y | copy from the previous picture at the signed bytes x, y |
| 6 | | not used by the files |
| 7 | P0 P1, then 8 or 2 bytes | 2 colors. P0 <= P1: one byte for each row. P0 > P1: 16 bits (little endian), one bit for each 2x2 cell |
| 8 | P0 P1 ... | P0 <= P1: four 4x4 quadrants (top left, bottom left, top right, bottom right), each 2 colors and 16 bits (the first quadrant uses P0 P1). P0 > P1: 32 bits, P2 P3, 32 bits: P2 <= P3 left and right halves (4x8), else top and bottom halves (8x4) |
| 9 | P0 P1 P2 P3 ... | 2 bits for each pixel or cell: P0 <= P1 and P2 <= P3: 16 bytes, 1x1; P0 <= P1, P2 > P3: 4 bytes, 2x2; P0 > P1, P2 <= P3: 8 bytes, 2x1 (2 wide); P0 > P1, P2 > P3: 8 bytes, 1x2 (2 high) |
| A | P0 .. P3 ... | P0 <= P1: four quadrants with 4 colors and 32 bits each; P0 > P1: 64 bits, P4..P7, 64 bits: P4 <= P5 left and right halves, else top and bottom halves |
| B | 64 bytes | the pixels |
| C | 16 bytes | one color for each 2x2 cell |
| D | 4 bytes | one color for each 4x4 quadrant |
| E | 1 byte | one color |
| F | 2 bytes | checkerboard: P0 where (x + y) is even |

The order of the bits (verified by the compare with ffmpeg): the bits of a
pattern are taken from the lowest bit up, in rows from the top, from left
to right in a row. The multi-byte patterns are little endian. In code 9
with 1x1 cells each row is a little endian word. The MultimediaWiki text
says that the leftmost pixel is the high bit: that is wrong for these files.

Use of the codes over all pictures of the five movies: every code except 6.
No copy (codes 0, 2, 3, 4, 5) reads outside the picture.

## 5. Sound

- 8-bit PCM (LOGO22S): unsigned bytes, left and right interleaved.
- DPCM (the others, opcode 03 version 1 with bit 2): the data starts with a
  signed 16-bit start value for each channel (left, right). The start
  values are the first sample. Each byte after them is an index into the
  256-entry delta table of the format (MultimediaWiki "Interplay DPCM");
  the delta is added to the value of its channel (the channels alternate)
  and the value is clamped to -32768..32767. A chunk of 2946 bytes gives
  (2946 - 4) / 2 + 1 = 1472 stereo samples = 5888 bytes, the length field.
- The sound chunks before the first picture (9 or 14) put the sound one
  second (LOGO22S: 0.9 s) ahead in the buffer. The player starts the sound
  at opcode 04 of the first frame and shows frame k at k times the frame
  time after that; the samples then line up with the pictures as the
  encoder intended.

## 6. Verification

`build-mve/engine/main/test_mvedec -nosilence FILE.MVE FRAMES AUDIO` writes
the pictures as palette indices plus the palette (the layout of
`ffmpeg -f rawvideo -pix_fmt pal8`) and the sound as 16-bit stereo. Compared
with `ffmpeg -i FILE.MVE -f rawvideo -pix_fmt pal8` and `-f s16le` (ffmpeg
n8.0-3-g08a81b090b):

| File | Pictures (mine / ffmpeg) | Different pictures | Sound samples (mine / ffmpeg) | Different samples |
|---|---|---|---|---|
| LOGO22S | 385 / 385 | 0 | 848925 / 848925 | 0 |
| TLOGOS | 422 / 422 | 0 | 620894 / 620894 | 0 |
| INTRO2 | 693 / 693 | 0 | 1071126 / 1071126 | 0 |
| FLAM1 | 979 / 979 | 0 | 1440428 / 1440428 | 0 |
| CREDITV1 | 4344 / 4344 | 0 | 6391468 / 6391468 | 0 |

The palette indices and the palettes are equal in every picture. ffmpeg
gives one picture for each opcode 11, not one for each display (INTRO2: 693
pictures, 728 displays). ffmpeg does not decode opcode 09, so the compare
leaves out the silence: the port also plays 19845 (LOGO22S) or 20598 (the
others) samples of silence at the end of each movie.
