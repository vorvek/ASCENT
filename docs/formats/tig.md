# .TIG: level triggers

One file per level (`NAME.TIG`), loaded by `traps.c` together with `NAME.TAP`.
A trigger is bound to one side of one segment. When something activates that
side, the trigger checks its conditions and then runs a chain of traps
(see `tap.md`), shows a message (see `say.md`), and switches other triggers
and traps on or off. Save games keep a copy as `SAVE\SGn_m.TIG` in the same format.

Tool: `python -I tools/sidefiles.py tig FILE`

All values are little endian. `fix` is 16.16 fixed point.

## File layout

| Offset | Size | Type | Meaning |
|---|---|---|---|
| 0 | 4 | u32 | version, always 2 |
| 4 | 2 | s16 | trigger count N (loader accepts 1..254) |
| 6 | 1800 | s16[900] | per-segment table: index of the first trigger in that segment, -1 = none |
| 1806 | 89 * N | | trigger records |

File size is exactly `1806 + 89 * N`.

## Trigger record (89 bytes)

Source of each name: "code" means the field use was read from the disassembly
of DTUGAME.EXE (trigger fire function at 0x93B29, init at 0x95A50, loader at
0x95B08). "guess" means the name is inferred. Default is the value the game
writes when it initialises an empty record.

| Offset | Type | Name | Default | Notes |
|---|---|---|---|---|
| 0x00 | s16 | unk00 | -1 | Unknown. Files hold -1..-6, -8, -9, -10 and -17. Not read by the fire function. |
| 0x02 | fix[3] | pos | | World position. Used as the sound position (code). |
| 0x0E | u8 | on | 1 | Trigger enabled (code, printed as `on=%d`). |
| 0x0F | u32 | unk0f | 0 | Always 0 in files. Loader zeroes it for version 1 files. |
| 0x13 | u16 | flags | 0 | bit 0: occupancy mode, tracks players entering and leaving (code). bit 1: used with bit 0, set or clear the player bit in the trap (code). bit 2: toggle the side texture between tex_on and tex_off on each firing (code). |
| 0x15 | u16 | activation_mask | 0xFF | ANDed with the activation type of the caller; zero result means no firing (code, printed as `mask=%d`). |
| 0x17 | s16 | unk17 | -1 | Unknown. Equals need_item_id + 4 in 61 of the 63 records that need an item. |
| 0x19 | s16 | need_item_class | -1 | Player must hold item (class, id) (code: inventory search at 0xB6BB3). |
| 0x1B | s16 | need_item_id | -1 | |
| 0x1D | s16 | unk1d | -1 | Unknown. Equals lack_item_id + 4 where set. |
| 0x1F | s16 | lack_item_class | -1 | Player must not hold item (class, id) (code). |
| 0x21 | s16 | lack_item_id | -1 | |
| 0x23 | u8 | take_item | 0 | Nonzero: call 0xB6D0C with the needed item after firing (guess: remove it from the inventory). |
| 0x24 | u16 | player_mask_a | 0xFF | Tested against a per-player bit table at 0x142220 (guess: race or class restriction). |
| 0x26 | u16 | player_mask_b | 0xFF | Tested against a per-player bit table at 0x142230 (guess: class or alignment restriction). |
| 0x28 | s16 | sound | -1 | Sound number played at pos when not -1 (code). All files hold -1. |
| 0x2A | s16 | tex_on | -1 | Texture number written to the side (second texture slot) when the trigger is switched on (code, log text "Using texon"). |
| 0x2C | s16 | tex_off | -1 | Same for off (code, "Using texoff"). |
| 0x2E | s16 | fire_count | 0 | Runtime counter, saturates at 0x7FFF (code). |
| 0x30 | u32 | unk30 | 0 | Nonzero calls 0xAF3CF. Always 0 in files. |
| 0x34 | u8 | skip_count | 0 | Number of activations to ignore before firing (code). |
| 0x35 | s16 | cond35 | -1 | Condition slot. The check function is a stub that returns true. |
| 0x37 | s16 | cond37 | -1 | Same. |
| 0x39 | s16 | cond39 | -1 | Same. |
| 0x3B | s16 | first_trap | -1 | First trap to run. Further traps follow trap.next_trap (code). |
| 0x3D | s16 | enable_trigger | -1 | Trigger to switch on. The chain continues through that trigger's own enable_trigger (code). |
| 0x3F | s16 | enable_trap | -1 | Trap to switch on (code). All files hold -1. |
| 0x41 | s16 | disable_trigger | -1 | Trigger to switch off, chained through disable_trigger (code). A trigger that names itself is one-shot. |
| 0x43 | s16 | disable_trap | -1 | Trap to switch off (code). All files hold -1. |
| 0x45 | u8 | object_num | 1 | If greater than 1 and below the object count, calls 0xABBE5(object, 3) (code). Meaning of the call is unknown. |
| 0x46 | s16 | seg | | Segment number (code). |
| 0x48 | s8 | side | | Side 0..5 (code). |
| 0x49 | s16 | next_in_seg | -1 | Next trigger in the same segment (code). |
| 0x4B | s16 | say_index | -1 | Message index in `NAME.SAY` (code). |
| 0x4D | 12 bytes | | 0 | Always zero. Unknown or padding. |

## How the game uses it

`check_trigger(seg, side, type, object)` at 0x939AD looks up
`table[seg]`, walks the `next_in_seg` chain (at most 50 steps), and fires each
trigger whose `side` matches. A non-player object only fires triggers that have
no item or `cond` conditions.

The caller passes an activation type that is ANDed with `activation_mask`.
The "use" action on the side in front of the player (0x95050) passes type 1.
The other callers and types are in `docs/re/triggers.md`. Masks in the files: 0xFD (625 records),
0xFF, 0xF5, 0xF4, 0xFC, 0xF1, 0xF8, 0xF2, 0xF9, 0xFA.

## Verified

- All 29 files (28 levels plus `SAVE\SG1_0.TIG`) have version 2 and the exact size `1806 + 89 * N`.
- Bytes 0x4D..0x58 are zero in all 816 records.
- All `say_index` values are below the message count of the level's SAY file, except
  CRYPTS triggers 26 and 27 (index 16 with 16 messages). The game rejects an index
  that is out of range, so these show nothing.
- All trap and trigger references are in range.

## Known inconsistencies in the shipped files

In 7 files the segment table does not match the `seg` fields exactly: some
triggers are in no chain, and some chains hold a trigger whose `seg` differs
(BANE, DROW1, HUB1, HUB4, IBBALAR, KOBOLDS, THIEF2). `sidefiles.py check` lists them.
The game only follows the table and the chain, so unreachable triggers never fire.

## Unknown

- unk00, unk0f, unk17, unk1d, unk30, the 12 bytes at 0x4D.
- The exact meaning of the two player masks and of the activation type bits.
- The trigger loader also logs "Loading the trigger texture file names." and opens
  `NAME.hk1` as text, but then closes it without reading. No `.hk1` file ships.
