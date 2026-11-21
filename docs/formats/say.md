# .SAY: level messages

One file per level (`NAME.SAY`). It holds the English text lines that triggers
show on screen (signs, speaking skulls, hints). A trigger selects a line with
its `say_index` field (see `tig.md`).

The file extension depends on the language: `say` (English), `ftm`, `gtm`,
`itm`, `stm`, `ntm`, `mtm` (table at 0x131C58 in DTUGAME.EXE, `localize.c`).
Only `.SAY` files ship in the English install.

Tool: `python -I tools/sidefiles.py say FILE`

## Layout

All values are little endian.

| Offset | Size | Type | Meaning |
|---|---|---|---|
| 0 | 4 | u32 | encoding flag: 0 = plain text, 1 = nibble swapped text |
| 4 | 4 | u32 | message count N |
| 8 | 4 * (N + 1) | u32[N + 1] | absolute file offsets. Message i is the bytes from offset[i] to offset[i + 1] |
| 8 + 4 * (N + 1) | | | text bytes |

- offset[0] equals `8 + 4 * (N + 1)`. offset[N] equals the file size.
- Messages are not NUL terminated. The game adds the terminator after reading.
- Message 0 is a title line ("These are the text messages for the Bane level ...").
  Triggers do use index 0 in some levels.

## Reader in the game

Function 0x94D8B (`traps.c`), called when a trigger with `say_index != -1` fires:

1. Plays sound 20 at the trigger position.
2. Opens the file and reads the flag and the count. A flag other than 0 or 1 aborts.
3. Rejects `say_index < 0` and `say_index >= N`.
4. Seeks to `8 + 4 * say_index`, reads two u32 (start, end). Rejects a length
   below 0 or of 4095 and more.
5. Reads the text. If the flag is nonzero, swaps the two nibbles of every byte
   up to the first zero byte.
6. Shows the text as a HUD message and plays sound 3001.

## Verified

- All 28 files parse with no unexplained bytes: the offset table is ascending,
  starts right after itself and ends at the file size.
- All 28 files have flag 0 and hold plain ASCII with no control characters.
- The nibble swap path is read from the code only. No shipped file uses it.

## Unknown

- Nothing in the file layout.
