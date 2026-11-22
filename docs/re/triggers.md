# Triggers, traps, messages, doors and warps

How DTUGAME.EXE 1.3 runs the level mechanics that replace the Descent 1 triggers.
Source: the disassembly of the executable image (`gamedata/re/dtu_flat.bin`, base
0x10000) with `tools/dtudis.py`. The file formats are in `docs/formats/tig.md`,
`tap.md`, `say.md` and `misc.md` (DTU.WRP). The port code is `engine/main/dtutrig.c`.

Marks: **V** = read from the code of the executable. **D** = checked against the
shipped data files. **G** = guess. Nothing in this document was checked against the
running original.

## 1. Functions

| Address | Name (port name or guess) | What it does |
|---|---|---|
| 0x9550D | load | Frees the old lists, loads TIG (0x95B08) and TAP (0x95797), clears the active list (0x95459), converts the trap times (0x9596F) on a fresh level |
| 0x939AD | `dtu_trig_check(seg, side, type, obj)` | Fires all triggers on one side |
| 0x95019 | `dtu_trig_touch` | The same for an object that touches a side. A floor counts only in the segment of the object |
| 0x93B29 | `dtu_trig_fire(trigger, type, obj)` | One trigger: conditions, message, texture, switches, traps |
| 0x942ED | item conditions | need and lack, through the inventory search 0xB6BB3 |
| 0x9435D | condition stub | Returns 1. Used for the three fields at 0x35, 0x37, 0x39 |
| 0x94363 | `dtu_trap_run(trap, obj)` | One trap and the chain after it |
| 0x94B27 | trap flag actions | Doors, locks, wall types, repeat |
| 0x94897 | set wall type | Sets `wall.type` on the side and on the connected side |
| 0x94A0C | wall sound | Open or close sound of the wall animation |
| 0x9493A | drop objects | Only for side 3: the objects of the segment fall |
| 0x946AE, 0x946FA, 0x94D61 | weapon, object, monster | The trap effects that create things |
| 0x94F89 | side texture and light of a trap | |
| 0x94D8B | say | Reads the message from the SAY file and shows it |
| 0x954A0, 0x954C8, 0x9547A | active list: add, remove, run | 32 trap numbers at 0x3A7CD0, count at 0x14221C |
| 0x9565C | player number of an object | |
| 0x95050 | `dtu_player_use` | The use action |
| 0xB30FD | use ray | A line of 20 units in front of an object |
| 0x7E859 | `dtu_wall_use_door` | "I'm in check door": lock test, secret door, open |
| 0x7D77E | `wall_open_door` | As Descent 1, with the reuse of a closing door |
| 0x7DCC3 | `wall_close_door(door_num)` | As Descent 1 |
| 0x29802 | `collide_object_with_wall` | Calls 0x95019 with a type from the object type |
| 0x59C3C | warp check | Reads the warp of a segment, loads the level |
| 0x599C3 | warp file load | The lines of one level |
| 0x5A5DE | facing | The direction of the player after a warp |
| 0x5B3AC | warp command | The lines with dest_level -2 |
| 0x347A2 | add message | Word wrap into the message box (0x34998 adds one line) |

Globals: trigger list pointer 0x142214, trap list pointer 0x142218, trigger count
0x142250, trap count 0x142252, segment table 0x3A75C8 (900 shorts), SAY text buffer
0x3A65C8, warp table 0x296494 (4 bytes for each segment), last player segment
0x13D41B, time of the last use 0x14224C.

## 2. When a trigger fires

**V** `dtu_trig_check(seg, side, type, obj)` takes `table[seg]`, follows `next_in_seg`
and calls `dtu_trig_fire` for each trigger whose `side` field is equal to the side.
The trigger fires when it is on and `(activation_mask & type) != 0`.

The callers and their types (all **V**):

| Type | Caller | Event |
|---|---|---|
| 1 | 0x95131 in the use action | The player uses the side in front (section 6) |
| 2 | 0x2998F, object type 2 | A monster touches a side |
| 2 | 0xA97A7 | A command of the monster script interpreter names a side. The object is the script object at 0x145537 |
| 4 | 0x2998F, object type 5 | A weapon touches a side |
| 4 | 0x6D4A1, 0xA06DC | Two other movement functions on a wall hit (**G**: missiles and thrown objects) |
| 8 | 0x2998F, object type 4 | The player touches a side. Called from the wall part of the player movement (0x9E901) |
| 8 | 0x69C44, side 3 | The player is in another segment than in the last frame: the floor triggers of the new segment |
| 8 | 0x59EC9, side 3 | The same, from the warp check, for a segment with a warp line (see below) |
| 0x10 | 0x2998F, object type 8 | **G**: a thrown or broken object |
| 0xFF | 0xB3870, 0xB3C16, 0xB3E1C | The thief skills (find and remove traps) fire a trigger when the removal fails. They also set side flag 4 ("found") and the trigger bytes 0x0F and 0x10. Text "You found a trigger!" at 0x1321BC |

0x29802 does not call the triggers for an object with flag 0x40 in byte 0x0D or flag 2
in byte 0x1AE.

**V** The floor rule of 0x95019: for side 3 the call is dropped unless `seg` is the
segment of the object. So "the player enters a segment" is a trigger on side 3 with
type 8. **D** 570 of the 879 triggers are on side 3.

**V** The segment change (0x69BC8, in the object move function 0x697F7): only for the
player object, when its segment is not the value at 0x13D41B. The code stores the
segment, calls the warp check 0x59C3C, and if that returns 0 it calls 0x95019 with
side 3 and type 8 for each segment of the physics segment list.

**V** Repeat in segments with a warp line: when the warp check returns a value that
is not 0 (a warp, or 2 for a command line), the caller writes -1 to 0x13D41B. In the
next frame the segment "changes" again. For a command line the warp check calls the
floor triggers itself (0x59EC9) before the command. Result: while the player stands
in a segment with a command line (music, ambient sound), the command and the floor
triggers of the segment run in each frame. The command function is made for this (it
compares with the current music first). A message trigger that does not switch
itself off adds its message and plays its sound in each frame. **D** BANE has this
case: segment 27 has the line `9,27 -2,223 67` and trigger 23 (message 21, no
`disable_trigger`). The add message function has no test for a repeated text.
Not checked in the running original. The port setting `Dtu_fix_trigger_repeat`
(section 9) fires the floor triggers one time for each entry of the segment.

**V** Fixed level rules at the end of 0x939AD. Level 7 (thief2): after trigger 46 the
code clears the lock of the wall of segment 608 side 4 and opens the door of segment
607 side 5. After trigger 43 the same for segment 610 side 4 and segment 609 side 5.
Level 32 (flame): after trigger 33, 32 or 26 it calls 0xAC055(object 36, 0), that is
`run_script(object 36, 0)` (the player talks to Lolth), when the 4-bit variable of the
script of object 36 (0x2B4B20, bits 24..27, statement E0) is below 1, 2 or 3. Script
320 sets the variable to 1, 2 and 3 itself, so this runs only when the run of mode 3
of the trigger did not start (a conversation window was open, for example). The port
has both rules (`trig_level_rules` in dtutrig.c).

**V** `wall_open_door` (0x7DBF5) has a rule for level 32: when the door of segment 478
side 4 opens and the variable of object 36 is below 3, the automatic close (wall flag
0x10) of both sides is cleared, the code runs game frames for 5 seconds (0x4391D) and
then calls 0xAC055(object 36, 0). The port: `dtu_trig_door_opened` (dtutrig.c), called
at the end of `wall_open_door` (wall.c); it counts the 5 seconds in `dtu_trig_frame`.

## 3. The steps of one trigger (0x93B29)

All **V**. The order is the order of the code.

1. Log line "TRAPS: trigger %d (on=%d type=%d mask=%d)". Return if `on` is 0 or the
   mask test fails.
2. Occupancy, only if the object type is 4 or 14 and flag 1 is set: `first_trap`
   must be valid. With flag 2 the code sets bit `0x800 << player` in the trap flags,
   without it the code clears the bit. Then it counts the four player bits. Without
   flag 2: if a player is still inside, return. If no player is inside, each trap of
   the chain loses the active flag 0x80, gets the stamp -1 and leaves the active
   list, then return. With flag 2 the trigger goes on. **D** One trigger has flags 1
   and one has flags 3 in all files.
3. `fire_count` goes up to 0x7FFF at most.
4. `skip_count`: if it is above 0 it goes down by 1, and the trigger returns while
   it is still above 0.
5. Player object (type 4) only:
   - if `first_trap >= 0` and (`tex_off >= 0` or flag 4): the default sound at the
     trigger position, 3108 when `(side flags & 3) == 2`, else 2001, volume
     `0x8000 + rand() % 0x8001`
   - `unk30` not 0: call 0xAF3CF (always 0 in the files)
   - player masks: `mask_b` against table 0x142230 (1, 16, 4, 8, 32, 2), index 5
     when player byte 0x1D33FB has bit 0, else player byte 0x1D33B6. `mask_a` against
     table 0x142220 (4, 2, 1, 8) with player byte 0x1D33B3 or 0x1D33B4. A failed
     test returns. **G**: class (two bytes for a character with two classes) and
     race or alignment. **D** 9 triggers have a `mask_a` and 3 have a `mask_b` that is not 0xFF
   - item conditions: `need_item` must be in the inventory and `lack_item` must not
     be. **D** 63 triggers need an item, most of class 16
   - the three condition fields (stub, always true)
   - `say_index != -1`: the message (section 5)
   - `object_num > 1` and below the object count: 0xABBE5(object, 3), then return if
     that switched the trigger off. **G**: wakes a monster
6. Other objects: return if a `need_item`, `lack_item` or condition field is set.
7. Texture, only if `tex_off >= 0`. With flag 4 (toggle): return if `first_trap` is
   set and `clock - trap.timestamp < trap.interval`. Then the second texture of the
   side becomes `tex_off` if it is `tex_on`, else `tex_on` (log "Using texoff",
   "Using texon"). Without flag 4 the second texture becomes `tex_off`.
8. `sound != -1`: that sound at the trigger position.
9. `take_item`: 0xB6D0C(need class, need id).
10. `enable_trigger` chain: each trigger gets `on = 1` and, if it has a `tex_on`,
    that texture on its side. The chain goes on through the `enable_trigger` of that
    trigger.
11. `enable_trap`: `on = 1` and the `tex_enabled` texture. No chain.
12. `disable_trigger` chain: `on = 0`, side flag 4 cleared, `tex_off` texture. A
    trigger that names itself fires one time.
13. `disable_trap`: `on = 0`, `tex_disabled` texture, removed from the active list.
    The code clears the side flags 0x28 of the side of the trap whose number is the
    number of the firing trigger (a defect: it should be the trap that is switched
    off). **D** No file uses `disable_trap`.
14. `first_trap != -1`: run the trap chain.

The chain loops count a step only when a trigger names itself, and stop at 50. A
chain of two triggers that name each other would not end. **D** not in the files.

Texture numbers (**D**): `tex_on` is equal to the second texture number of the side
in the level file for 94 of the 95 triggers with a texture. So the numbers are
numbers of the texture table of the level file, like the side numbers.

## 4. The steps of one trap (0x94363)

All **V**. A trap that is off is skipped (the chain goes on).

1. Delay: if `delay != 0`, the trap is not active (flag 0x80) and `charges != 0`:
   - first call: set flag 0x10000, add the trap to the active list, stamp = clock,
     go to the next trap
   - later calls: while `clock - stamp <= delay` set the chain stop mark and leave.
     After that: remove from the active list, `delay = delay_init`, clear 0x10000,
     set the chain stop mark, stamp = clock, and run the effect
2. No delay: if `clock - stamp < interval`, go to the next trap.
3. stamp = clock.
4. Charges, if `charges != -1`: the value goes down by 1. If the old value was 0 or
   less: charges = 0, an active trap leaves the active list, a trap with flag 0x8000
   gets `charges = charges_init`, and the effect does not run. So a recharge trap
   with one charge runs on every second activation.
5. Sound: the `sound` number, or the default sound as in a trigger.
6. `weapon_type != -1`: 0x946AE (0x93294 if the weapon table entry at 0x1E9DCE is
   above 0, else 0x934A8). **D** 38 traps.
7. `spawn_object != -1`: 0x946FA creates an object of type 13 and copies fields of
   entry N of the table at 0x1B5684. **D** 9 traps.
8. `flags != 0`: the flag actions, in this order:
   - 0x100 toggle wall: with 0x200 set the wall type becomes 4 (open), wall sound,
     drop objects. Else type 5 (closed), wall sound
   - 0x001: `wall_open_door`, drop objects
   - 0x002: `wall_close_door(wall number)`. A defect: the function takes a number of
     the list of moving doors. **D** not in the files
   - 0x010: wall type 5, wall sound
   - 0x020: wall type 4, wall sound, drop objects
   - 0x040 and not 0x080: set 0x080 and add the trap to the active list. The trap
     then runs in each frame (with its interval and charges) until its charges end
     or the last player leaves (section 3, step 2)
   - 0x008 lock: with 0x200 set the lock flag of the wall is inverted, else it is
     cleared. So the first run takes the lock off, the second puts it on
   - at the end 0x200 is inverted, for each trap with a flag
9. `robot_type != -1`: `gate_in_robot(type, seg, 1)` at 0x1374D, or 0xAC141 if
   `robot_arg < 4`. **D** 49 traps.
10. `tex_enabled >= 0`: second texture of the side. One time (flag 0x400): add
    `light_delta` to the light of the four corners.
11. `stat_delta != 0`: add it (whole points) to the player value at 0x1D336F, clamp to
    the maximum at 0x1D33BD and to 0. **G**: hit points. **D** one trap.
12. Next trap: `next_trap`, unless the chain stop mark is set.

Drop objects (0x9493A) does nothing unless the side is 3 (the floor).

The wall sound (0x94A0C) takes the animation of the wall. If the wall has no valid
animation number, it takes `texture number % Num_wall_anims` (second texture, or the
first if the second is 0), and `(seg + side + wall) % Num_wall_anims` if that is
negative. Type 4 plays the open sound, other types the close sound.

### Active traps

**V** 0x9547A runs each trap of the active list with `dtu_trap_run(trap, NULL)` one
time in each frame. It is called from the game loop at 0x438E4. Because it calls the
normal run function, the chain after an active trap runs too.

### Time

**V** The trap times in the TAP file are seconds. 0x9596F multiplies `interval` and
`delay` by 18.2 (double at 0x12ED87) and cuts the fraction (not for a level state
from a save).

**V** The clock that the traps compare with is 0xEDC4C, a jump to 0x10A0F3. That
function calls 0x10A08D, which calls DOS function 2Ch (get time) and returns
`((hour * 60 + minute) * 60 + second) * 100 + hundredths`, then subtracts the start
value at 0x474908 (with a correction of 8640000 for midnight). This is the `clock()`
of the C library: real time in 1/100 second.

Result: the trap timers count real time, not frames. They do not depend on the frame
rate. But the units do not agree: a time of N seconds in the file is `N * 18.2`
hundredths, so 0.182 * N seconds. **D** The files have intervals of 5, 10, 15 and 50
and delays of 1 to 25: 10 seconds in the file is 1.82 seconds in the game. The DOS
clock steps in units of about 5.5 hundredths, so the times have that grain.
The stamp of a trap that never ran is -1, and the tests are unsigned, so the first
run passes.

## 5. Messages (0x94D8B)

All **V**.

1. Sound 20 at the trigger position (volume argument 5).
2. Opens the SAY file of the level, reads the flag and the count. On an open error
   it shows a text with the file name.
3. Rejects an index outside the count, and a length below 0 or of 0xFFF and more.
4. Reads the text. If the flag is not 0 it swaps the two halves of each byte.
5. 0x347A2(player number, text) adds the text to the message box, then 0x34983 adds
   a line with one space, then sound 3001 at full volume.

0x347A2 breaks the text at spaces, tabs and line ends into lines of the box width
(100 characters at most), with two spaces after a word that ends with `.`, `!` or
`?`. The color: for player -1 the nearest color to (0, 60, 0), else the color of the
player from the table at 0x13B970 (player 0: 15, 15, 23, each times 2). 0x34998
moves the 40 lines up by one and stores the new line with the color in its first
byte. No test for a repeated text.

## 6. The use action and doors

**V** 0x95050 is called from the key code (0x53D6F keyboard, and 0x52A91 and 0x53094, **G**: joystick button and
mouse button) while the control is down. The keyboard entries are number
39 and 40 of the key table at 0x13C190 (22 bytes each), text 555 "Activate". The
default table at 0x13C010 has scan code 0x1E (the key A) for entry 39 and none for
entry 40. The other defaults in that table agree with the keys of the manual (Space
jump, Z and X side step, V immersive mode, Tab map, I inventory). The mouse and
joystick defaults were not read.

Steps (**V**):

1. Return if the player object has flag 0x40 in byte 0x89.
2. Return if less than 0x8000 (half a second) passed since the last use.
3. 0xB30FD: `find_vector_intersection` from the player position, 20 units along the
   forward vector, radius 0.
4. If a side was hit: log "TRAPS: seg=%d side=%d", `dtu_trig_check(seg, side, 1,
   player)`, then the door function 0x7E859. If that did not open a door: 0x951DE
   (climbing: "You cannot perform this action while wearing %s").
5. If no side was hit: 0x7EDFA (**G**: use of an object in front).

The door function 0x7E859(seg, side, object, unlock) (**V**):

- not a wall of type 2 (door): return 0
- locked (wall flag 8): sound 2501. Without `unlock`: text 132 "This door is
  locked.", return 1. With `unlock` and `wall.keys & 6`: "Strong magic prevents you
  from unlocking this door.", return 1
- state "opening": return 3
- wall flag 0x40 (secret door) and side flag 0x10 not set: set side flag 0x10 on both
  sides, sound 3104, text 949 "You found a secret door!"
- `wall_open_door`, return 3

**V** The door function has two callers: the use action, and the weapon and wall
collision for weapon type 0x1C with `unlock = 1` (**G**: the spell Knock). So a door
does not open when the player walks into it, and no door needs a key from the
inventory. A locked door opens through a trap with flag 8 (and that trap is run by a
trigger that can need an item), through the thief skill, or through the spell.
**D** Of 1644 door walls, 142 are locked at the start, 35 have `keys = 4`.

`wall_open_door` has more callers (**V**): the trap flag 1, the level rule of thief2,
`wall_toggle` (0x7E9F7), the monster and wall collision (0x25D30), the thief skills
(0xB38C4, 0xB3C53) and 0xB3318.

**V** `wall_open_door` (0x7D77E) is the Descent 1 function: the states opening (1),
waiting (2) and closing (3), a closing door opens again with
`time = play_time - time`, linked walls, the open sound of the animation. It reads
the animation number of the wall with no test. It has a rule for level 32 (segment
478 side 4, section 2). Not read: the frame functions that move the door (0x7E199 calls
`wall_close_door`). The port uses the Descent 1 functions (animation frames by time,
5 seconds waiting for a door with the automatic flag). **D** Each door wall in the
files has an animation number from 1 to 81, and 1617 of 1644 have the automatic flag.

## 7. Warps (DTU.WRP)

**V** The file name is the mission file name with the extension `wrp`. 0x599C3 reads
the lines of one level into a table with 4 bytes for each segment: dest_level
(signed byte, -1 = no line), dest_segment (short), value (byte).

**V** The warp check 0x59C3C(seg, object) runs when the segment of the player
changes (section 2):

1. No line: return 0.
2. `dest_level == -2` (command): fire the floor triggers, then
   0x5B3AC(0xFE, code, value), return 2. Code 0 leaves the dungeon (inn to
   Waterdeep). Codes 1 to 19 go through a jump table (**G**: the mission checks of
   the file comments). Codes 100 to 198 are music and 200 to 298 are ambient sound:
   `code - 100` or `code - 200` is a bit set (1 start, 2 stop, 4, 8, 0x10 "do nothing
   if the value plays already", 0x20).
3. A warp to another level first goes through a check at 0x59D2D (an item of the
   player of class 5, id 13, and the monsters of the level; messages 0x8B to 0x98).
   Not understood.
4. Not in a network game: gold. From level 0 (the inn) the player needs 1 gold
   ("Passage in and out of Undermountain costs one gold. ..."), and with less than 2
   the game asks ("If you enter the well now you will not have enough gold for
   passage ..."). To level 0 the player needs 1 gold ("It costs 1 gold piece to
   ascend back to Waterdeep. ..."). The passage takes 1 gold.
5. The warp: waits for the sounds, logs "Dest level %d". For the same level: log
   "solo-warp!". Else log "Warping to %s, Segment %d.", close the moving doors, clear
   the door flag 2 of all walls, save the state of the level (0x5A756), load the
   level and its warp lines.
6. The player object is put back, moved to the center of the destination segment
   with velocity 0, and turned by `value` (0x5A5DE): 0 east (forward +x), 1 west
   (-x), 2 north (+z), 3 south (-z).
7. Return 1.

**D** 75 of the 628 lines are warps, 12 of them inside one level. The value of each
warp line is 0 to 3.

**V** The sparkles of a warp. 0x599C3 gives each segment with a warp line a station of
type 3 (the table 0x1C5E28, at most 200) with the center of the segment. The station
frame 0x3DCAF (each game frame) makes, for a station of type 3 whose segment is in the
visible list and has no object, a vclip 10 (`generic.abm`, the sparkles, `vlighting`
10.0) at that center with the size of the player (0x3DFD8 to 0x3E032). The vclip ends
after its play time and the next frame makes a new one, so a visible warp sparkles and
lights its place all the time. The port does this in `warp_sparkle_proc` (fuelcen.c)
for the segments of the last render list. The same frame has code for the station types
4 (a pulse of the side light, explosions and the sounds 0x8AC to 0x8AF) and 5 (the sound
2707 at a random volume). No level file has a segment special 4 or 5. Five of the six
callers of 0x3D5D3 (`fuelcen_activate`) pass the special of a segment; the sixth
(0xBCC31) passes a value of its caller that was not traced. A level change clears the
stations of type 4 and 5 (0x5A42C). **G**: the code of the types 4 and 5 does not run.

## 8. What the port does

`engine/main/dtutrig.c` follows sections 2 to 7. Differences:

- The clock is the game time in 1/100 second, not the real time. A run with
  `-fixedfps` gives the same result each time. The game time stops in a pause, the
  real clock of the executable does not.
- The chain loops count each step (the executable counts only a trigger that names
  itself).
- `disable_trap` clears the side flags of the trap that is switched off.
- The warp does not save the state of the level that the player leaves. Each level
  loads new. The gold rule and the check of step 3 are a hook that allows the warp.
- A warp to a level that is not in the game data is dropped with a log line.
- Before the load of the other level the port stops all sound channels and waits
  100 ms, because a stopped channel of the mixer reads its sample data until its
  fade ends.
- The warp command codes 100 to 198 (music) are not run here. `dtumusic.c` reads the
  warp file for them itself. The other codes go to the hook `dtu_hook_warp_command`.
- The thief skills, the monster script command, the climbing and the use of objects
  are not in the port.
- The messages do not use the two spaces after the end of a sentence.
- Systems that do not exist yet are hook functions (see `dtutrig.h`).

## 9. Settings

| Setting | Default | The executable | The port with the setting on |
|---|---|---|---|
| `Dtu_fix_trigger_table` | 1 | Follows the segment table and the `next_in_seg` chains of the TIG file. In 7 files some triggers are in no chain and cannot fire | Makes the table and the chains again from the `seg` field of each record |
| `Dtu_fix_trigger_repeat` | 1 | The floor triggers of a segment with a warp command line fire in each frame while the player is in the segment | They fire one time for each entry of the segment |
| `Dtu_fix_trap_close_door` | 1 | Trap flag 2 calls `wall_close_door` with a wall number | The door of the trap starts to close. Off: no action |
| `Dtu_fix_trap_seconds` | 1 | A trap time of N seconds is 0.182 * N seconds | The time is N seconds |
| `Dtu_trig_log` | 0 | | Prints each trigger and trap event |

Options: `-triglog` sets `Dtu_trig_log`. `-origtrig` clears the four fix settings
that are on. `-trapseconds` sets `Dtu_fix_trap_seconds`.

`Dtu_fix_trap_seconds` is on: the loader of the executable scales seconds by 18.2
but its clock counts 1/100 second, so a trap time of N seconds lasts 0.182 * N
seconds. This is a unit defect. The shipped levels were
tested with the short times, so `-origtrig` restores them.

## 10. Not known

- The meaning of the two player masks, of `unk30`, of the call 0xABBE5(object, 3)
  and of 0xAC155 (called after a use that opened no door).
- The check at 0x59D2D before a warp to another level.
- The mission checks (warp command codes 1 to 19).
- The frame functions of the doors in the executable (animation time, waiting time).
- The default mouse and joystick controls of the use action.
- The trigger fields `unk00`, `unk17`, `unk1d`.
