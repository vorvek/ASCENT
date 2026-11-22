# Level state files of DTUGAME.EXE 1.3

The game keeps the state of each level that the player has left: the
objects (dead creatures stay dead, taken items stay taken), the walls and
doors, the textures of the sides, the triggers and the traps. The state
goes into files of the directory `SAVE` under the current directory. A
save game copies the state files of all changed levels into files of the
save slot, and a load copies them back.

Addresses are linear addresses of `gamedata/re/dtu_flat.bin` (object 1 at
0x10000). Marks: **V** = read in the code or checked against the files of
the install. **G** = a guess, with the reason. **P** = a decision of the
port.

## 1. Files

All files are in `.\save` (string 0x128873, 0x133C47). Each function that
uses them saves the current directory (0xECACE), changes to `.\save`
(0xEE092) and changes back at the end (V).

| File | Content | Written by |
|---|---|---|
| `CSUF.ls` | the level table of the running game (section 2) | 0x5ABC6, 0x5A756, 0xBC1EC, 0xBC51D |
| `temp<L>.lvl` | state of level L: objects, walls, doors, sides, AI, automap (section 3) | 0x5A756 via 0xBAD08 |
| `temp<L>.tig` | the trigger records of level L, TIG format | 0x5A756 via 0x95E0A |
| `temp<L>.tap` | the trap records of level L, TAP format | 0x5A756 via 0x95FB8 |
| `sg<S>.ls` | the level table of save slot S | 0xBC1EC |
| `sg<S>_<L>.lvl`, `.tig`, `.tap` | the state of level L in save slot S | 0xBBF30 (move), 0xBC03A (copy) |

L is the level index of DTU.MSN (0 = YPINN, `Current_level_num - 1`). S is
the slot number of the menu plus 1 (SG0.SG belongs to `sg1.ls`) (V). The
install has `SAVE\CSUF.LS`, `SG1.LS` and `SG1_0.LVL`, `.TAP`, `.TIG` (V).
`SG1_0.TIG` and `SG1_0.TAP` are byte for byte equal to `YPINN.TIG` and
`YPINN.TAP` (V): the triggers of that game were in their first state.

## 2. The level table (`*.ls`)

600 bytes: 100 records of 6 bytes, one for each level index (V, read with
`fread(buf, 6, 100)` at 0x5A7BB, 0x5AAE0, 0xBC2A7).

| Offset | Size | Meaning |
|---|---|---|
| 0 | u8 | 1: `temp<L>.*` holds the state of the level (changed since the last save) |
| 1 | u8 | 1: the slot files `sg<slot>_<L>.*` hold the state |
| 2 | s32 | the slot S of those files, -1: none |

A second table of 100 dwords at 0x296304 has 1 for each level that has a
state (set by 0x5A756, cleared by 0x5ABC6, filled from byte 1 by 0xBC51D).
The readers of that table were not traced (G: the automap or Khelben).

## 3. The state file (`temp<L>.lvl`) (V, 0xBAD08 writes, 0xBB1A7 reads)

The writer opens the file with mode "wb" (0xA08C2) and writes:

1. `DGSS` (4 bytes, the first bytes of the autosave.c version string at
   0x146938) and the u32 version 12.
2. Before the objects: each object with render type 6 (morph) gets its
   morph data back (0x5EDEB) or, if there is none, it is killed (flag 2) and
   becomes render type 1 with control and movement type 0 ("STATE: killing
   morphing").
3. u32 `Highest_object_index + 1` (0x13D324), then that many object records
   of 479 bytes (0x1DF, the memory image of the array 0x2B0614).
4. u32 `Num_walls` (0x13E2D4), then the walls (24 bytes each, 0x39FEC8).
5. u32 `Num_open_doors` (0x3A3EA0), then the moving doors (16 bytes each,
   0x3A1AE8).
6. For each segment 0 to `Highest_segment_index` (0x13CD44) and each of
   its 6 sides: the s16 values at side +2, +4, +6 (wall number, texture 1,
   texture 2). A segment is 536 bytes (0x2206A4), a side 80 bytes at
   segment +4.
7. 0x4AB04: u32 `Highest_segment_index`, then for each segment the 6 bytes
   at side +1 (the side flags of DTU; bit 0x20 is set for a trap side by
   0x5A855).
8. 0x16413 (the AI state of Descent 1 `ai_save_state`): u32 0x13A6C0, u32
   0x167174, 37600 bytes at 0x153EB0 (the local AI records), 40000 bytes
   at 0x15D190 (2500 path points of 16 bytes), 128 bytes at 0x153A30, u32
   0x13A6C8, u32 0x13A6CC.
9. 900 bytes at 0x185FEC (G: `Automap_visited`, one byte per segment).

The size of `SAVE\SG1_0.LVL` is 113449 bytes (V). It starts with `DGSS`,
version 12 and the object count 49 (V).

The reader (0xBB1A7) checks `DGSS` and version 12, clears the object list
of each of the 900 segments, resets the objects (0x69E03(1)), reads the
objects and links each to its segment (0xBAFE0). An object with type 0xFF
or type 1, or with no valid segment, is killed ("STATE: killing object
%d -- illegal segment"). 0xBAFE0 sets the object up again from the tables
(0xAE3F2), finds its segment, and deletes a creature that has the flag 2
("STATE: found dead robot"). The fire of a rest place is a fireball, so it is
killed here; the station frame makes it again (character.md, section 9). Then the reader reads the walls, the doors,
the sides, the side flags (0x4AC07), the AI state (0x16408, 0x164C7) and
the automap bytes, and sets four timers (0x215D74 to 0x215D80) to the game
time.

The reader copies object 0 to 0x3D51CC before it reads the objects; that
copy is never read (V: one reference in the executable). So the player
object is the one of the file.

## 4. Save the state of a level: 0x5A756(L) (V)

1. Read `CSUF.ls` ("r+b"; if it does not open, return 0).
2. Set byte 0 of record L to 1 and 0x296304[L] to 1, write the table back.
3. Write `temp<L>.lvl` (0xBAD08), `temp<L>.tig` (0x95E0A: u32 2, s16
   trigger count, the 900 shorts of the segment table, the 89-byte trigger
   records; the editor also writes a text file of texture names) and
   `temp<L>.tap` (0x95FB8: before it writes, each trap gets its field
   +0x41 back from +0x49, then the TAP format).

Callers: the warp to another level (0x5A14C), command 0 of DTU.WRP (YPInn
to Waterdeep, 0x5B3BE, before the question "Leave the Yawning Portal?"),
and the save game in a level (0xBA9FE).

## 5. Restore the state of a level: 0x5AA75(L) (V)

1. Read `CSUF.ls`; if it does not open, return 0.
2. If byte 0 of record L is 1: log "Restoring temp<L>.lvl.", read
   `temp<L>.lvl` (0xBB1A7), set 0x13CE31 = 1, read `temp<L>.tig` and
   `.tap` (0x9550D, the level trigger loader), 0x13CE31 = 0, then
   0x5A855(0).
3. Else if byte 1 is 1 and the slot is 0 or more: the same with
   `sg<slot>_<L>.lvl`.
4. Else return 0: the level stays as the level file made it.

0x5A855(0) applies the trigger changes of single levels (levels with the
byte at player +0x71 equal to 2, 6, 7, 21 or 32). With the argument 1
(called by the level loader 0x494EB) it first resets the runtime fields
of all triggers and traps.

**V** The rules (0x5A901 to 0x5AA5D, trigger records of 89 bytes at
0x142214):

| Level | Rule |
|---|---|
| 2 kobolds | trigger 1 off (the one-time message of segment 167) |
| 6 thief1 | byte 6 of trigger 34 = 0. That byte is part of the y coordinate of the position of the trigger (the sound position), not the `on` byte at 0x0E: a defect with no seen effect |
| 7 thief2 | triggers 43 and 46 on; trigger 46 gets the flags, the masks, the sound, the textures, the count, the skip count, the trap and trigger numbers, `object_num` and the message of trigger 43 (so it runs trap 17 in place of 18) |
| 21 drow2 | `object_num` of trigger 20 = 42 (the script that 0xACCDB gives to object 42, objects.md) |
| 32 flame | triggers 26, 32 and 33 on again (the one-time triggers of Lolth, object 36) |

The reset of argument 1 clears `fire_count` and the bytes 0x0F, 0x10 of each
trigger and the bytes 3, 4 of each trap, and sets side flag 0x20 of the side
of each trap that is on and has a `spawn_object` or a `robot_type`. No reader
of side flag 0x20 was found (all readers of the side flag byte 0x2206A9 test
the bits 0 to 4).

The port: `dtu_trig_state_rules` (dtutrig.c) after the read of the TIG and TAP
state files in `restore_files` (dtustate.c), all rules but the byte of
thief1. The reset of argument 1 is the load of the TIG and TAP files of the
level. Test: `-level FLAME -statetest -1,0,-1,1,62 -triglog` warps to HUB4
and back and prints "level rule of the restore (0x5A855), level 32" after
"Restoring TEMP32.LVL."

Callers: `StartNewGame` 0x4DD5A (the Yawning Portal from Waterdeep, L 0),
the warp 0x5A1B0 (after the new level and its warp lines are loaded), the
load of a save game in a level 0xBBDE5.

## 6. The warp (0x59C3C) around the state (V)

```
0x5A0F6: for each moving door: 0x7DCC3(0)   ; wall_close_door of door 0
         clear the flag 2 (WALL_DOOR_OPENED) of every wall
         player +0x1D4E1B = the segment of the player
         0x5B305 (music commands of the segment)
         0x5A756(Current_level_num - 1)       ; save the level
         load the new level (0x494EB), read its warp lines (0x599C3)
         0x5AA75(destination level)           ; restore it
         end the loading picture (0x7C303, 0x7C2BB), PALETTE.256
         put the player into the destination segment (0x5A268 on)
```

0x7DCC3 is the Descent 1 `wall_close_door`: the door gets the state 0
(closed), the first frame of its animation, and leaves the list. In DTU
`do_door_open` (0x7DE76) never takes a door out of the list when it is
open (the list is left only at 0x7DE54, in the close code), so the list
holds every door that was opened in this visit. The warp therefore closes
all doors of the level before it saves it: a door that the player opened
is closed when the player comes back. A save game in a level does not do
this, so a door stays open in a save.

## 7. Save games (V)

The save (0xBA99F, argument 1 in Waterdeep, 0 in a level):

1. In a level: 0x5A756(Current_level_num - 1).
2. 0xBC1EC(S): the slot table. It reads `CSUF.ls` (cs) and `sg<S>.ls` (ss;
   if there is no file every record is (0, 0, S)). For each level L:
   - cs byte 0 is 1: if ss byte 1 is 1, delete `sg<S>_<L>.*` (0xBBECA);
     move `temp<L>.*` to `sg<S>_<L>.*` (0xBBF30); cs = (0, 1, S).
   - else cs byte 1 is 1 and the cs slot is not S: copy
     `sg<cs slot>_<L>.*` to `sg<S>_<L>.*` (0xBC03A); cs = (0, 1, S).
   - else cs byte 1 is 0: delete `sg<S>_<L>.*`; if ss byte 1 is 1 and the
     cs slot is not S, delete `sg<cs slot>_<L>.*`.
   - the cs slot becomes S.
   Then cs is written to `sg<S>.ls` and to `CSUF.ls`.
3. The market: `TheStore.<S>` (0x87C05(S, 1), docs/re/frontend.md).
4. The `SG<S-1>.SG` file (docs/formats/misc.md).

The load (0xBB780): 0xBC51D(S): read `sg<S>.ls`, delete the running state
(0x5AC6F("CSUF.ls")), write `CSUF.ls` with byte 0 = 0 and bytes 1 to 5 of
`sg<S>.ls`, and set 0x296304[L] = byte 1. Then the market (delete
`geldor.dat`, 0x87C05(S, 0)), the SG file, and for "between levels" 0:
`StartNewLevelSub(level, 0)` (0x4E33D), the three lists of the SG file
(54, 16 and 40 bytes per record) and 0x5AA75(level - 1). The player
object, with its position, comes from the state file.

## 8. New game and end (V)

- A new character (0x87EB9, 0x87EBE): 0x5AC6F("CSUF.ls") and 0x5ABC6.
- 0x5AC6F(name): open `name`, delete `temp<i>`, `temp<i>.tig` and
  `temp<i>.tap` for i = 0 to 99 (the name without extension is deleted,
  so the `temp<i>.lvl` files stay; their byte 0 in the new table is 0),
  then delete `name`.
- 0x5ABC6: clear 0x296304, write `CSUF.ls` with 100 records (0, 0, -1).
- The close of the program (0x426FA) calls 0x5AC6F("CSUF.ls").

## 9. Entering the Yawning Portal again

`StartNewGame` (0x4DD07) starts level 1 and then restores level 0. The
state of the inn was saved by command 0 when the player stood in the exit
segment, so the player stands there again. The warp check of the trigger
code runs when a move takes the player into another segment (0x69BC8), so
the exit does not ask again until the player leaves the segment and comes
back (V for the code, G for the effect: not run).

## 10. The port

`engine/main/dtustate.c` follows sections 2 to 8 with these differences:

- P: the files are in `SAVE` in the directory of the user. The port never
  writes into the game directory. Names are found with any case of the
  letters; new files get the upper case names of the DOS directory
  (`TEMP3.LVL`, `SG1_3.LVL`, `CSUF.LS`).
- The `.LVL`, `.TIG` and `.TAP` files have the format of the executable
  (section 3, tig.md, tap.md), so the level states of a 1.3 save load in the
  port and those of the port load in 1.3. The port also reads the files of
  its earlier builds (header `ASLS`, `ASLT`, `ASLP`), but no longer writes
  them.
- The object image (479 bytes). Offsets verified with the read order of
  `read_object` (0x47D90): 0x00 signature, 0x04 type, 0x05 id, 0x06 next,
  0x08 prev, 0x0A control, 0x0B movement, 0x0C render, 0x0D flags, 0x0E
  segnum, 0x10 the s16 of the level record, 0x12 attached object (-1), 0x14
  pos, 0x20 orient, 0x44 copy of orient, 0x68 size, 0x6C shields, 0x70
  last_pos, 0x7C contains type, id, count, 0x7F matcen_creator, 0x80 to 0xF8
  the block of the level record (0x84 lifeleft), 0xFC physics (velocity,
  thrust 0x108, mass 0x114, drag 0x118, brakes 0x11C, rotvel 0x120,
  rotthrust 0x12C, turnroll s16 0x138, flags u16 0x13E), 0x140 control part
  (ai_static: behavior, flags[11] 0x141, hide_segment 0x14C, hide_index
  0x14E, path_length 0x150, cur_path_index 0x152, follow path 0x154 and
  0x156, danger laser 0x158 and 0x15C; explosion: spawn 0x140, delete 0x144,
  objnum 0x148; light: intensity 0x140, s16 0x144), 0x162 render part
  (polygon: model 0x162, subobj_flags 0x166, tmap_override 0x16A, 0x16E,
  0x170, 0x172, 0x176; vclip: number 0x162, frametime 0x166, frame 0x16A;
  item: s16 bitmap 0x162), 0x1B0 script word, 0x1D5 count of an item. The
  other bytes (docs/re/objects.md, ai.md) are kept from the file that the
  level came from.
- Model numbers. Verified in `SAVE\SG1_0.LVL` (YPINN), `SG1_2.LVL`
  (KOBOLDS) and `SG1_3.LVL` (BONES): the executable numbers the models of
  the level archive from 0 (`ske_gg.pof` is 37 in BONES), the port has the
  21 models of the player archive first (58). The port converts with
  `Bm_level_first_model` (bmread.c). The player model of the executable
  (3 in BONES, the dagger in the hand) is a number outside this rule; the
  writer keeps the number of the loaded file, or 0.
- Texture numbers. The sides and `tmap_override` of the file hold the
  numbers of the texture table of the level file (verified: all 1362 sides
  of `SG1_0.LVL` and 3078 of `SG1_3.LVL` equal those of YPINN.LVL and
  BONES.LVL). The port translates them as `level_translate_textures()`.
- Scenery: the executable gives all scenery the movement type 1 (all 55 of
  BONES). The port reads it as movement none (it moves pushed scenery
  itself) and writes 1.
- A dead creature with the flags 0x40 and 2 (0xC6) stays in the list of the
  executable and does nothing (0xBAFE0, 0x6A3CC). The port deletes it at the
  read; the file of the port has a free entry there. A corpse (shields below 1.0,
  object flag 4 in the port) is put on the floor at the setup of the level
  (`Dtu_fix_corpse_floor`, docs/re/ai.md, "The corpse"); the file keeps the
  position that the port writes.
- The AI block (section 3, step 8). The ai_local record of a creature
  (47 bytes, docs/re/ai.md) is read into the AI state of the port after
  `dtu_objects_level_setup()` and written from it: awareness, retry count,
  mode (only still, wander, chase and run from: the port keeps the path of
  a creature in its own list, not in `Point_segs`), flags, goal segment, the
  times and the morale; from the object image the activity, the target,
  the regeneration and spell times, the script word, the AI state and the
  animation state. The rest of the block (`Point_segs`, the cloak records,
  the values before and after) is written back as it was read. A level that
  was not read from such a file gets 0x13A6C0 = 1, the last value 6.0, no
  path points, and path_length 0 for each creature.
- The save game (SG file) sets `GameTime` from its offset 49 as 0xBBA2F,
  so the times of the level files count from the same clock.
- P: the port takes a door out of the list of moving doors when it is
  open (Descent 1 `do_door_open`). To give the result of the executable,
  the warp closes every door wall that is not closed (state, flag 2 and
  the first frame of the animation), then clears flag 2 of all walls.
- The level rules of 0x5A855 are in `dtu_trig_state_rules` (section 5).
- P: after a restore that keeps the position of the player (the Yawning
  Portal from Waterdeep, the load of a save game), the port tells the
  trigger code that the player stands in that segment
  (`dtu_trig_player_placed`), because the port checks the segment of the
  player in the first frame of a level and the executable only after a
  move into another segment (section 9).
- P: the direct start (`-level NAME`) begins with a new level table, as a
  new character does. The close of the program deletes the running state
  (`CSUF.ls`, `temp*`) as 0x426FA does; the slot files stay.
- Calls: `dtustate_warp_leave` / `dtustate_warp_enter` around the load in
  the warp of dtutrig.c; `dtustate_game_started` in `StartNewGame`
  (gameseq.c); the save in a level and the slot tables in the save and load
  of dtufe.c; command 0 of DTU.WRP saves the inn before the question.

Settings: none. The rules of the executable are kept.

### Test (`-statetest`)

`-level BANE -statetest 0,5,42,121,178 -frames 300 -fixedfps 30`: frame 30
opens the door of segment 0 side 5 and kills creature 42 (a blackbat), frame
150 puts the player into segment 121 (warp to HUB2, segment 231), and 60
frames later into HUB2 segment 178 (warp back to BANE, segment 1). Result:

```
before the warp:  door wall 48 state 2 flags 0x12; object 42 shields 0 flags 0x4 (destroyed)
Warping to hub2.lvl, Segment 231.   State: saved the state of level 9
Warping to bane.lvl, Segment 1.     State: saved the state of level 8
Restoring temp9.lvl.
level start:      door wall 48 state 0 flags 0x10 (closed by 0x5A0F6); object 42 shields 0 flags 0x4
```

The save in a level keeps the door open: with the script of `-fescript`
(new character, Khelben, the Yawning Portal, the ESC menu "Save Game" in
slot 9, a move to segment 18, the ESC menu "Load Game") and `-statetest
12,5,2,18,-1` in YPINN, the load gives "Restoring sg10_0.lvl." and the door
of segment 12 in state 2 (open) with flags 0x12, creature 2 dead, and the
player at the position of the save (segment 2, not 18).

Saves of the DOS game (two saves of a played 1.3 game, level BONES, with the
level files of YPINN, HUB1, KOBOLDS and BONES; `-fescript`, `-statelog`):

- Both slots load: the character, 20 and 21 items, 7 mage spells, the key
  item, the hot keys and the 7 fuel centers of the save; "Restoring
  SG1_3.LVL" with 150 objects, all models but the player's as in the level.
- A save in the port right after the load: the TIG and TAP files are byte for
  byte those of the original, the walls, sides, side flags, automap, path
  points and cloak records are equal; the objects differ in the dead
  creatures (freed) and the values that the frames changed.
- Load, warp to HUB1 (`SG2_1.LVL` restored) and back (`TEMP3.LVL` of the
  port restored), save in the port, load that save: "Restoring SG4_3.LVL".
- `-level BONES -statetest 0,5,42,381,170`: a level with no file of the
  executable is written with the AI values of a new level and read back.
- The saves of an earlier build of the port (`ASLS` files) load.

Option `-statelog`: prints the model numbers that differ from the level and
the objects that lie outside their segment after a restore.

A map change that a trigger makes lives only in this state. Example: the new
opening of HUB1 that Khelben speaks of at missions 3 and 4 is trap 3 of
HUB1.TAP (walls 36 and 37 to type 4), fired by the floor of segment 61 when
the player carries MISC 107. The save of a played 1.3 game at mission 4 has
the open walls in `SG1_1.LVL` and the used trap in `SG1_1.TAP`; the port
restores them. Details in [scripts.md](scripts.md), section 8.
