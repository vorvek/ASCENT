# .TAP: level traps

One file per level (`NAME.TAP`), loaded by `traps.c` after `NAME.TIG`.
A trap is an action record. It is not a typed object: each field group is an
optional action, and a trap performs every action whose field is set.
Traps run when a trigger names them in `first_trap` (see `tig.md`).
Save games keep a copy as `SAVE\SGn_m.TAP`.

Tool: `python -I tools/sidefiles.py tap FILE`

All values are little endian. `fix` is 16.16 fixed point.

## File layout

| Offset | Size | Type | Meaning |
|---|---|---|---|
| 0 | 4 | u32 | version, always 2 |
| 4 | 2 | s16 | trap count N (loader accepts 0..1024) |
| 6 | 95 * N | | trap records |

File size is exactly `6 + 95 * N`.

## Trap record (95 bytes)

"code" means the use was read from the disassembly (run function 0x94363,
init 0x95718, loader 0x95797). "guess" marks inferred names.

| Offset | Type | Name | Default | Notes |
|---|---|---|---|---|
| 0x00 | s16 | id | -1 | Own index. Equals the record index in all files (verified). |
| 0x02 | u8 | on | 1 | Trap enabled (code). |
| 0x03 | s16 | unk03 | 0 | Always 0. Loader zeroes it for version 1 files. |
| 0x05 | s16 | charges_init | not set | Reload value for charges when the recharge flag is set (code). |
| 0x07 | s16 | charges | 1 | Remaining uses. Decremented on each run. -1 = unlimited (code). In files it equals charges_init. |
| 0x09 | s16 | unk09 | -1 | Always -1. |
| 0x0B | s16 | sound | -1 | Sound number. -1 selects a default sound (3108 or 2001, by a side flag) (code). Always -1 in files. |
| 0x0D | s16 | next_trap | -1 | Next trap in the chain run by one trigger (code). |
| 0x0F | u32 | flags | 0 | See below. |
| 0x13 | s16 | weapon_type | -1 | When not -1: creates a weapon or spell effect of this type at pos in seg (code path 0x946AE, target function near the string "Weapon_create_new"; the name is a guess). Files hold 7, 8, 11, 15. |
| 0x15 | fix[3] | pos | | World position of the trap (code: sound and effect origin). |
| 0x21 | fix[3] | vec21 | | Unit vector in all 331 level records (verified). Guess: facing or firing direction. Not seen in the traced code. |
| 0x2D | s16 | spawn_object | -1 | When not -1: creates an object from template table entry N at pos (code path 0x946FA; guess: an item). Files hold 6, 16, 32. |
| 0x2F | s16 | unk2f | -1 | Always -1. |
| 0x31 | s16 | stat_delta | 0 | When nonzero: added (as integer part of a fix) to a player value, then clamped to 0 and to the player's maximum (code). Guess: hit points, positive heals. One trap in the files uses it (value 6). |
| 0x33 | s16 | robot_type | -1 | When not -1: gates in a robot (monster) of this type in seg (code: `gate_in_robot`, error text names it). |
| 0x35 | s16 | robot_arg | -1 | If below 4 the game calls 0xAC141 instead of `gate_in_robot`. Files hold 303..351. Meaning unknown. |
| 0x37 | s16 | tex_disabled | -1 | Side texture written when a trigger disables this trap (code). Top two bits are the D1 overlay rotation bits. |
| 0x39 | s16 | tex_enabled | -1 | Side texture written when the trap runs or is enabled, if >= 0 (code). |
| 0x3B | s16 | unk3b | -1 | Always -1. |
| 0x3D | s16 | unk3d | -1 | Always -1. |
| 0x3F | s16 | light_delta | 0 | Added once to the light value of the 4 corners of the side (code). Always 0 in files. |
| 0x41 | u16 | delay_init | 0 | Copy of delay. The loader overwrites it with the converted delay. |
| 0x43 | s16 | seg | | Segment number (code). |
| 0x45 | s16 | side | | Side 0..5 (code). |
| 0x47 | u16 | interval | 0 | Minimum time between two runs (code). |
| 0x49 | u16 | delay | 0 | Delay between activation and effect (code). |
| 0x4B | s32 | timestamp | -1 | Runtime: timer value of the last run (code). |
| 0x4F | 16 bytes | | 0 | Always zero. Unknown or padding. |

### Time units

`interval` and `delay` are stored in seconds. On a fresh level load the game
multiplies both by 18.2 (function 0x9596F, constant at 0x12ED87) to get timer
ticks. It skips the conversion when the level state comes from a save, so
`SAVE\SGn_m.TAP` holds ticks.

### flags

| Bit | Meaning | Source |
|---|---|---|
| 0x0001 | Open the door on seg/side, then wake objects in the segment | code (0x7D77E, 0x9493A) |
| 0x0002 | Second door or wall action (0x7DCC3) | code, action name unknown |
| 0x0008 | Toggle wall flag 8 (the D1 "door locked" flag) | code |
| 0x0010 | Set wall type to 5 (D1 "closed") on both sides | code |
| 0x0020 | Set wall type to 4 (D1 "open"), then drop objects in the segment | code, log text "dropping objects in seg" |
| 0x0040 | Delayed: first activation arms the trap, effect follows after `delay` | code |
| 0x0080 | Runtime: armed | code |
| 0x0100 | Toggle wall type between 4 and 5 | code |
| 0x0200 | Runtime: toggle state | code |
| 0x0400 | Runtime: light_delta already applied | code |
| 0x0800 << p | Runtime: player p is inside (set by occupancy triggers) | code |
| 0x8000 | Recharge: reload charges from charges_init when they run out | code |

Bits set in the level files: 0x01, 0x08, 0x10, 0x20, 0x40, 0x100, 0x200, 0x8000.

## Verified

- All 29 files (28 levels plus `SAVE\SG1_0.TAP`) have version 2 and the exact size `6 + 95 * N`.
- `id` equals the record index in all 331 level records.
- `next_trap` is in range everywhere. Bytes 0x4F..0x5E are zero everywhere.

## Unknown

- unk03, unk09, unk2f, unk3b, unk3d, robot_arg, the 16 bytes at 0x4F.
- What exactly weapon_type and spawn_object index into.
- The loader logs "Loading the trap texture file names." and opens `NAME.hk1` as text
  only in editor mode. No such file ships.
