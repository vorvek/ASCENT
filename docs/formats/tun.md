# TUN music files (Interplay ACM audio)

Status: header and bitstream verified against all 52 `.TUN` files of the
version 1.3 install and against all 376 streams inside WERDZ.DAT and
WHAMMO.DAT with `tools/tun.py` and `tools/jrg.py`. Nobody has listened to the
decoded audio. See "What is verified" below.

A `.TUN` file is one audio stream in the Interplay ACM format, the format that
Interplay also used in Fallout and other games of the time. It is not ADPCM.
It is a subband codec: each block holds quantised subband samples, and a
cascade of two-tap filters turns them back into PCM. The file has a 14-byte
header and then one bitstream to the end of the file. There is no table of
blocks and no trailer.

The same streams, with the same header, are the entries of WERDZ.DAT and
WHAMMO.DAT (see [jrg.md](jrg.md)).

DISCO.JRG lists 37 of the 52 `.TUN` files (see jrg.md).

## Header

All integers are little endian.

| Offset | Size | Meaning |
|---|---|---|
| 0 | u32 | signature 0x01032897 (bytes 97 28 03 01) |
| 4 | u32 | total number of decoded 16-bit values, counted over all channels. Frames = this value / channels |
| 8 | u16 | channel count (1 or 2) |
| 10 | u16 | sample rate in Hz (22050 in every file) |
| 12 | u16 | packed: bits 0 to 3 = `level`, bits 4 to 15 = `rows` |
| 14 | ... | bitstream |

Derived values: `cols = 1 << level`, block length = `rows * cols` values.

Combinations found:

| Files | Channels | Rate | Level | Rows | Values per block |
|---|---|---|---|---|---|
| 36 .TUN, 74 WHAMMO entries | 2 | 22050 | 7 | 16 | 2048 |
| 15 .TUN (FEBSTAFF, FECGEN, FEWDEEP, IGACT1 to 3, IGBGND1 to 3, the six TAG files) | 2 | 22050 | 10 | 16 | 16384 |
| FECREDIT.TUN, 302 WERDZ entries | 1 | 22050 | 6 | 8 | 512 |

## Bitstream

Bits are read from each byte starting at bit 0. A field of n bits is read
with its lowest bit first. Blocks follow each other with no alignment: a
block starts at the bit after the previous block.

The decoder produces blocks until it has `total` values. The last block is
decoded completely and the values past `total` are dropped. In some files the
last block needs a few bits more than the file holds. Those bits read as 0.

### Block

A block is a table of `rows` by `cols` integers. Value (row r, column c) is
at index `r * cols + c`.

| Size | Meaning |
|---|---|
| 4 bits | `pwr` |
| 16 bits | `val` |
| per column | 5 bits `ind`, then the data of that column for all rows |

`pwr` and `val` define an amplitude table: `amp[k] = k * val` for
`-(1 << pwr) <= k < (1 << pwr)`. Every coded number k below is stored in the
block as `amp[k]`. The table has 65536 slots (k from -32768 to 32767) and
keeps the entries of earlier blocks outside the range that the current block
rewrites.

`ind` selects how the column is coded:

| ind | Name | Coding of the column |
|---|---|---|
| 0 | zero | all rows are 0, no data |
| 3 to 16 | linear | each row: `ind` bits, unsigned value v, k = v - (1 << (ind - 1)) |
| 17 | k13 | see below, values -1, +1 |
| 18 | k12 | see below, values -1, +1 |
| 19 | t15 | 5 bits hold three rows: base 3 digits, lowest digit first, k = digit - 1 |
| 20 | k24 | values -2, -1, +1, +2 |
| 21 | k23 | values -2, -1, +1, +2 |
| 22 | t27 | 7 bits hold three rows: base 5 digits, k = digit - 2 |
| 23 | k35 | values -3 to +3 |
| 24 | k34 | values -3 to +3 |
| 26 | k45 | values -4 to +4 without 0 |
| 27 | k44 | values -4 to +4 without 0 |
| 29 | t37 | 7 bits hold two rows: base 11 digits, k = digit - 5 |
| 1, 2, 25, 28, 30, 31 | | invalid |

The t codes stop in the middle of a group when the last row is reached.

The k codes are prefix codes, read per row:

- k13, k24, k35, k45 start with one bit: 0 means this row and the next row
  are both 0 (only this row when it is the last one). 1 means continue.
- k12, k23, k34, k44 do not have that first bit.
- Then one bit: 0 means this row is 0. 1 means continue.
- k13, k12: one bit, 0 gives -1, 1 gives +1.
- k24, k23: two bits, values 0 to 3 give -2, -1, +1, +2.
- k35, k34: one bit. If it is 0: one more bit, 0 gives -1, 1 gives +1. If it
  is 1: two bits, values 0 to 3 give -3, -2, +2, +3.
- k45, k44: three bits, values 0 to 7 give -4, -3, -2, -1, +1, +2, +3, +4.

### Synthesis ("juggle")

After a block is read, the filter cascade runs in place on it. It is skipped
when `level` is 0.

`step = 1` when `level > 9`, otherwise `step = (2048 >> level) - 2`. The rows
of the block are handled in groups of `step` rows (the last group can be
smaller). For each group, with `n` = number of rows in the group:

```
sub_len = cols / 2;  sub_count = 2 * n
pass(0, group, sub_len, sub_count)
add 1 to every value of the group whose index is a multiple of sub_len
k = 1
while sub_len > 1:
    sub_len = sub_len / 2;  sub_count = sub_count * 2
    pass(k, group, sub_len, sub_count)
    k = k + 1
```

`pass` sees the group as `sub_count` lines of `sub_len` values and filters
down each of the `sub_len` positions on its own:

```
for i in 0 .. sub_len-1:
    r0, r1 = history[k][i]            # two values per position and pass, start as 0
    for j in 0 .. sub_count/2 - 1:
        r2 = line[2j][i];    line[2j][i]   = 2*r1 + (r0 + r2)
        r3 = line[2j+1][i];  line[2j+1][i] = 2*r2 - (r1 + r3)
        r0 = r2;  r1 = r3
    history[k][i] = r0, r1
```

The history is not reset between groups or between blocks. It links each
block to the one before it.

Output: every value of the block, in index order, is shifted right by
`level` bits (arithmetic shift) and clamped to -32768..32767. For two
channels the values alternate left, right.

## What is verified

Checked with `tools/tun.py check` on all 52 `.TUN` files and with
`tools/jrg.py check --deep` on all 376 streams of WERDZ.DAT and WHAMMO.DAT:

- Header: every file and stream has the signature, a channel count of 1 or 2
  and a rate of 22050. For the stereo `.TUN` files the total is even.
- Consumption: for all 52 files and all 376 streams the decoder produces
  exactly `total` values, and the last bit it uses is in the last byte of the
  file or stream. No file has unexplained bytes after the header. No invalid
  `ind` value occurs. This is the main evidence that the bitstream layout
  above is right: one wrong field size would shift everything after it.
- End of stream: in 49 of the 52 `.TUN` files the padding part of the last
  block asks for up to 8 bits past the end of the file. The other 3 end inside
  the file.
- Amplitude table: no linear code in any `.TUN` file points outside the range
  that its own block wrote. (Not counted for the WERDZ and WHAMMO streams.)

Statistics of the decoded `.TUN` files (103 channels):

- Mean of each channel: at most 17.4 in absolute value. No DC runaway. The
  filter history runs through the whole file, so an error would grow or stay.
  It does not.
- Clipping: 48 files have no value that needed clamping. FECREDIT.TUN has 298
  of 5679351 values, IGACT1 has 5, IGACT2 has 11, IGACT3 has 1. In FECREDIT
  the clamped values sit in 20 short clusters, most of them between 84 s and
  98 s, the loudest part of the file (RMS 7890 in that 20 second window against 770 to 3500
  in the others). The signal returns to normal after each cluster. FECREDIT also
  has a constant offset of about -48 over the whole file. Guess: both come
  from the source recording, not from the decoder.
- Smoothness: RMS of the first difference divided by RMS of the signal is
  0.05 to 0.55 per channel (white noise would give 1.41). Largest: STARTPUB
  right channel 0.55, all others 0.37 or less.
- DEATHTAG.TUN and TAGHAHA.TUN in detail: the average step across block
  borders and column borders is the same as the average step elsewhere, so
  the blocks join without clicks. Both start and end with zeros. Left and
  right channel differ (correlation 0.12 and -0.12), so the two channels are
  really interleaved as described.

Nobody listened to the result.

## What is unknown or not verified

- Nobody has listened to the output. The checks above show that the
  bitstream is read correctly and that the result has the shape of audio.
  They do not prove that the synthesis filter is exact. A wrong filter
  constant could still give smooth output.
- The decoder was written from the published description of the Interplay ACM
  format (the open source libacm decoder documents it). It was not compared
  with the decoder inside DTUGAME.EXE and not compared sample by sample with
  another decoder.
- The original decoder most likely uses 32-bit integers. `tools/tun.py` uses
  unbounded integers. This only matters if a filter sum overflows 32 bits.
- How the game loops or chains the files (for example IGBGND and IGACT, or
  the two KHELBEN entries of DISCO.JRG) is unknown.
- DIGITEST.RAW (42616 bytes) is not an ACM file. It is 21308 raw signed
  16-bit little endian values (range -28026 to 31429). Its sample rate and
  channel count are not stored and were not determined.

## Tool

`tools/tun.py` (Python 3, standard library only):

```
python -I tools/tun.py info FILE [FILE ...]     print the header
python -I tools/tun.py wav FILE [OUT.wav]       decode to 16-bit PCM WAV
python -I tools/tun.py check FILE [FILE ...]    decode and print consumption and statistics
```
