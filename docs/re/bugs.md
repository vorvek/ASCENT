# Crashes and faulty logic of DTUGAME.EXE 1.3, and the state of the port

This file lists the defects of the original game that can crash it or that
give wrong results, and the defects of the port that the soak test and the
sanitizers found. For each defect: the symptom, the cause with the address,
what the port does, and how it was tested.

Addresses are linear addresses of `gamedata/re/dtu_flat.bin` (object 1 at
0x10000). Marks: **V** = read in the code or checked against the files of
the install. **G** = a guess, with the reason. **P** = a decision of the
port.

## 1. How the original stops

- **Assert is a stop.** The release build keeps the asserts. The assert
  function 0xD7B80 tests the condition. A false condition runs `int 3`
  and then jumps to `Error` (0xD7BB4): the text "Error: Assertion failed:
  %s, file %s, line %d" goes to 0x148C14 and the program ends with code 1
  (0xD7C05). So a failed assert is a return to the DOS prompt with one
  line of text. **V**
- `Error` is also called directly, for example by the warp check 0x59C3C
  ("Checking warp in non-existing segment %d", 0x59CA5). **V**
- A raw `Int3()` (an `int 3` with no `Error`) does not stop the game.
  Proof: `start_player_death_sequence` (0x6940E) no longer creates the
  death camera and runs `int 3` at 0x69580 on every death of the player
  ("Can't create dead player camera."). **V**
- A divide fault (`idiv` by 0, or a quotient that does not fit in 32
  bits) and a page fault stop DOS/4GW with a register dump. A write
  through a null pointer does not fault: linear address 0 is the real
  mode interrupt table, which DOS/4GW maps. Such a write changes an
  interrupt vector, and the machine can hang or crash later, at the next
  use of that vector. **V** (code) and **G** (the effect on the machine)

The asserts that the code paths of section 2 and 3 can reach were listed
with a script that follows the call graph (for 0xADC46: fvi.c lines 685,
793, 925, 940, 1120, 1234; collide.c 3520, 3572, 3808; object.c 1469,
1703, 1727; ai.c 3303; polyobj.c 1799). None of them fails with the data of
the disc on the paths that were read. **V**

## 2. Turn undead (key T)

Symptom reported on real hardware: the game returns to the DOS prompt
after Turn Undead.

### The original (0xADC46) **V**

1. 0.5 s between two uses (`|P+0x6C - [0x145626]| >= 0.5`).
2. Level: the level of class 2 if class 2 is the cleric (`P+0x98 == 0`),
   else the level of class 1, plus 1. Armor 45 (the second Cloak of
   Elvenkind) in the cloak place (`P+0x1ADB`) allows the action and sets
   level 7. Without the cleric class (0x35A09) and without the cloak:
   TEX 931 "You cannot perform this action.".
3. `roll = rand() % 20 + 1`, `destroys = rand() % 11 + 2`, the
   `banish_sound` of the `$PLAYER` line at the player.
4. 0xADB7D sorts the NPC list (0x3D42CC, 7-byte entries) by the class in
   the low nibble of byte +3 (an exchange sort of the byte indices at +2).
5. For each entry, in that order: skip if `Robot_info+0x99` of the object
   has no bit 0x20 (undead), if 0xA66C9 says dead, if the brain bit of the
   two segments (0x9810D) is 0, or if a ray from the creature to the
   player (0x3EFD9, no objects) hits a wall. Bit 0x10 of byte +3 clear:
   "You can no longer attempt to turn the %s." (0x131B64). Else clear bit
   0x10 and read `t = table[class*14 + level]` (0xA6486, level at most
   14). The class (0 to 8) comes from the robot name: `ske_`, `zombie`,
   `ghoul`, `banegard`, `ghast`, `mummy`, `gskel`, `lich`, `sfiend`
   (0x145602, strnicmp, level init 0xAC4B4). The table, levels 1 to 14:

   | Class | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 | 13 | 14 |
   |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
   | ske_ | 10 | 7 | 4 | T | T | D | D | D* | D* | D* | D* | D* | D* | D* |
   | zombie | 13 | 10 | 7 | 4 | T | T | D | D | D* | D* | D* | D* | D* | D* |
   | ghoul | 16 | 13 | 10 | 7 | 4 | T | T | D | D | D* | D* | D* | D* | D* |
   | banegard | 20 | 19 | 16 | 13 | 10 | 7 | 4 | T | T | D | D | D | D | D* |
   | ghast | - | 20 | 19 | 16 | 13 | 10 | 7 | 4 | T | T | T | D | D | D |
   | mummy | - | - | - | 20 | 19 | 16 | 13 | 10 | 7 | 4 | 4 | T | T | T |
   | gskel | - | - | - | - | - | 20 | 19 | 16 | 13 | 10 | 10 | 7 | 7 | 4 |
   | lich | - | - | - | - | - | - | - | 20 | 19 | 16 | 16 | 13 | 13 | 10 |
   | sfiend | - | - | - | - | - | - | - | - | 20 | 19 | 19 | 16 | 16 | 13 |

   T = -1 (turned), D = -2 and D* = -3 (destroyed), - = -4 (no effect).
6. -4: hostile, 0xA6505, morale 0x10ADF, TEX 927 "You are currently
   unable to affect the %s.", and bit 0x10 is set again. -3 and -2:
   0xA6505, then 0xA6ACB(creature, no attacker, its hit points): the
   creature dies, TEX 928 "You destroyed the %s.", `destroys - 1`. -1 or
   `roll >= t`: target -1, fleeing, not hostile, 0xA6505, morale, TEX 929
   "You successfully turned the %s.". Else hostile, 0xA6505, morale, TEX
   930 "You failed to turn the %s.".

### Defects found in the original

| # | Address | Defect | Effect |
|---|---|---|---|
| T1 | 0xA6ACB -> 0x29A9C | The kill of a destroyed creature goes through the damage function with no attacker and weapon -1. That function reads the attacker without a test at 0x29E3D (`[attacker+0x88]`), 0x2A054 and 0x2A0C7 (`[attacker+5]`). With no attacker these are reads of linear 0x88 and 0x5 (the interrupt table). | The kill message and the experience depend on the byte at linear address 5 (compared with the player number), and a hold effect on the creature depends on bit 3 of the byte at 0x88. No fault on DOS. **V** (code), **G** (the values on a real machine) |
| T2 | 0xADF09, 0xADF57 | Bit 0x10 ("can be tried") of the next undead is cleared before the test that the destroys are used up. | That creature can never be turned again, and the player gets "You can no longer attempt to turn the ..." for a creature that was never tried. **V** |
| T3 | 0xADF65 | The local at `esp+0x2B8` is always -1, so the code that counts the creatures of the class (0xADF77 to 0xAE097) never runs: the first test returns. | Dead code; the destroy limit only counts destroys, a turn has no limit: every visible undead is turned. **V** |
| T4 | 0xAC431, 0xADC46 | The NPC list is filled at the level start and by a gate; an entry is never taken out. After a death the object number can belong to a new object (a missile, an explosion, gold, a new creature), and the class nibble and bit 0x10 stay. 0xADDDD reads `Robot_info[obj->id]` of that object without a type test. | A wrong object can be treated as undead with the class of the dead creature. A read past `Robot_info` for an id above the robot count. **V** (code), the effect was not observed |
| T5 | 0x9810D | The brain bit decides which undead see the player. The BRN files of the disc are copies of one matrix that fits no level (docs/formats/brn.md). | Undead behind walls are tested, undead in view may be skipped. **V** |

No path of 0xADC46 was found that faults or fails an assert with the data of
the disc. The reported return to DOS could not be explained from the code
that was read. Candidates that were not ruled out: T4 with an object
number that was reused by a creature whose model has fewer frames (the
body cylinder assert `xzframe <= po->n_frames`, polyobj.c 1799, is on the
death path), and the null writes of section 4 (an earlier write into the
interrupt table makes the next `int 3` or DOS call crash).

### The port **P**

Turn undead was not in the port. It is now in `dtucombat.c`
(`dtu_combat_turn_undead`), with the table, the classes, the order, the
messages and the destroy limit of the executable. The key T is read in
`dtu_combat_frame()` (controls.c does not read it). The Spider Piece 0x12
calls it with level 2 (`dtuchar.c`). Differences:

- T1: the kill is `dtu_damage_creature_direct(creature, NULL, ...)`: no
  experience and no read of a missing attacker.
- T2: `Dtu_fix_turn_undead` (on; `-origcombat` clears it) keeps bit 0x10.
- T4: the state is kept per object number and signature, so a new object
  starts with a new state and only robots are tested.
- T5: the brain bit is 1 (`Dtu_fix_brain`); the ray decides.
- `dtuobj.c` has `dtu_creature_turn()` for the flags, 0xA6505 and the
  morale check.

Test: the soak test calls turn undead with levels 1 to 14 on all levels.
BONES with the player next to the zombies 139 and 140 (`-testpos
180,0,150,0`, seeds 7, 11 to 16): failed turns (zombie hostile), turns
(morale number 0, the zombie flees), and a destroy at level 12 ("takes 15
from nothing"), no fault.

## 3. Death by a zombie in a secret area of the second level

Symptom reported on real hardware: the game returns to the DOS prompt when
a zombie kills the player in one of the secret areas of the second level,
the tomb with the Ghoulmaster.

### The place **V**

- DTU.MSN: hub1 (1), kobolds (2), bones (3). The tomb is `BONES.LVL`
  ("Undead Raiders"). BONES.SAY: "This crypt belongs to my master!".
- The Ghoulmaster is object 106, robot type 3 `Ghoul` (`ghu_y.pof`),
  segment 294, script 32 (GABE): "So, you have found me at last. I have
  watched you deal with my minions...". Zombies (type 2, `zom_n.pof`): 4,
  11, 23, 24 (script 30), 28, 29, 139 and 140 (segments 329 and 327, x =
  180).
- Script 32 starts with `ANGER_NPCS any range 50 = 5` (all creatures
  within 50.0 become hostile) and its reply 132 ("Yes, I will join you")
  kills the player inside the script (`HP -= 9999`), then goes on to label
  300 (`NARRATE`).

### The original

The death path was read: the damage function 0x29A9C for the player
(type 4): the die sound, the killer at P+0x5C, hit points 0, flags |= 2,
0x6A3CC (into the delete list), the damage level `10 - hp * 10 / max_hp`
(0x2A744, divides by the maximum hit points, not 0). 0x695E5 calls
`start_player_death_sequence` (0x6940E): the asserts pass, the `int 3` at
0x69580 runs on every death. `dead_player_frame` (0x6934A) changes the
player object to type 12 and writes a word into the segment of the player
(0x6939C: the word at `Segments[seg] + 0xFA` = `rand() % 5 + 9`, with the
segment of the player object, which is valid there). No fault or failed assert
was found on this path. **V**

The defects of section 4 that can hit at the time of a death in that
place: D2 (gold of a humanoid creature, not a zombie), D3 (the delete list,
needs 800 deletions in one frame). The real cause was not found. **G**:
a push of the player body out of its segment by a creature, see P1: the
port crashed in that state, and the original copies the same uninitialized
segment list (its trigger loop 0x69C18 tests the segment numbers against
`Num_segments` 0x13CD3C, so a value equal to `Num_segments` still passes).

### The port

Test: `-level bones -testpos 180,0,150,0 -frames 4000 -fixedfps 30`. Zombie
139 kills the player at frame 251, the death screen starts at frame 342,
no fault, no assert. The script death of label 132 was not run.

## 4. Other defects of the original

| # | Address | Defect | Effect | Port |
|---|---|---|---|---|
| D1 | 0x2703D, 0x29CE0, 0x434E1, 0x4D9CB, 0x5694D, 0x569F3, 0x6C958, 0x9F3DC, 0x9F957, 0xA6973, 0xA6C7E | The result of `object_create_explosion` (0x3BD4C) is used without a test. It is null when the object list is full (800), when the segment is not valid, or when the vclip number is not 0 to 249 (0x3BC87). | Writes to linear 0x1AE, 0xD, 0x14, 0x68 (the interrupt table). 0x29CEC writes the vector of `int 3`. **V** | The port tests the vclip, the size and the segment before it creates an explosion (`vclip_at` in dtuobj.c, dtucombat.c, dtuspell.c) and tests each `obj_create` result. |
| D2 | 0x6A6B6 to 0x6A6F1 | "make gold" (a humanoid dies, 4 of 10 times) changes the explosion object into a gold item without a test of the pointer. | A null result writes linear 0x4, 0x5, 0xC, 0x6C, 0x84 to 0x87 and 0x162: the vectors of `int 1`, `int 3`, `int 1Bh` and `int 21h` (DOS). The next DOS call crashes. **V** | dtuobj.c 0x6A639 creates an item with `dtu_create_item` and tests the result. |
| D3 | 0x6A682 to 0x6A68F | Gold amount `rand() % (moola >> 2)`: `moola` 1 to 3 passes the test `moola >= 1` and divides by 0. | Divide fault. No `$MONSTER` line of the disc has `moola` 1 to 3, so the data never triggers it. **V** | dtuobj.c tests `(moola >> 2) > 0`. |
| D4 | 0x6A545, 0x6A5DB | The delete list (0x2AEF54, 800 entries) is written before the test of the count. With 800 entries the write goes to 0x2AFBD4, the first entries of the free object list. | The next `obj_allocate` (0x6874B) returns a wrong object number. Needs 800 deletions in one frame. **V** | The port uses the Descent 1 object code (`OF_SHOULD_BE_DEAD`), no such list. |
| D5 | 0x6965E | The delete loop accepts object number 800 (`jg`, not `jge`). | `Objects[800]` is past the array. **V** | as D4 |
| D6 | 0x11183 to 0x111A4 | The animation step of a creature divides `elapsed * (frames - 1) << 16` by the state time with no test. | State time 0 (REST, or a state with no time in the table): divide fault. A quotient above 2^31 (a creature that stays in a state with time 0.5 for about 18 minutes of game time, for example while held or inactive): divide fault. **V** (code), **G** (when it happens in play) | dtuobj.c `ai_animation_step` tests the time and uses 64-bit math. |
| D7 | 0x123EF | The stuck counter of the flee movement (0x13A79C) and the last position (0x13A790) are one global for all creatures. | A fleeing creature uses the position of another creature: it turns or reverses at random. **V** | not ported in that form (dtuobj.c has its own flee code). |
| D8 | 0x59C3C | The warp check calls `Error` (return to DOS) when the segment of the player is not valid. | A player outside the mine at a move ends the game. **V** | dtutrig.c returns. |
| D10 | 0x27CCF to 0x27D08, 0x27650 | Impact sound of the killing blow: the hit point plays `monster_kill_sound` in place of `monster_hit_sound` when the creature dies, and no shipped table sets `monster_kill_sound`. A missile kill plays no weapon sound. | The killing blow has no weapon impact at the hit point (only the contact sound and the die sound). **V** | Fixed, always on: combat.md, "The impact sound of the killing blow". |
| D11 | 0x433E0, 0x9FCF1 | Footsteps: the step of the player is 0x200 to the side in turn, which the pan code turns into a hard left or right pan; the step of a creature plays on each game frame that shows the first frame of its walk cycle (3.4 steps in 50 ms at 60 fps). | Steps that sound off to the side, and creature steps repeated quickly at a distance. The rate of the player (0.625 s, 0.3125 s running) is a normal cadence. **V** | `Dtu_fix_foot_pan`, `Dtu_fix_creature_steps` (dtufoot.c), off with `-origsound`. music.md, "Footsteps". |
| D9 | 0x1F5ED to 0x1F634 | The sounds of the `$PLAYER` line default to -1; `banish_sound` is played by turn undead without a test. | No effect: `digi_xlat_sound` (0x2D7F5) rejects -1. **V** | as the original |
| D12 | 0x675D4 to 0x675EC, 0x13D3E3 | `draw_polygon_object` sets `Interpolation_method` (0x14BDB0) to 1, the linear texture mapper, for each object but the viewer, because `Linear_tmap_polygon_objects` (0x13D3E3) is 1 in the data and no code writes it (Descent 1 default). | The textures of large model polygons near the eye (bed, table, crate) bend along the triangle seams. **V** | Fixed, always on: `draw_polygon_model_frame` (polyobj.c) draws all models with the perspective mapper (2). The GPU path interpolates u and v with perspective anyway (gpu.c scene shader). Checked with `-level YPINN -testpos -20,-8,45,313 -testmove wait:5,lookdown:3`: the planks of the table top are straight in software, the GPU picture does not change (2 pixels). |
| D13 | 0x434AB to 0x4351B, vclip 10 | Each step of the player on floor type 1 (water) makes a splash: vclip 10 (`generic.abm`, sparkles) with the size of the player, 0x100 times the side step (0x200, sign changes each step) added to world x and z. Vclip 10 has `vlighting=10.0` in all 46 tables, the light of the big explosions; the splash lives half of its 2.0 s and starts at 5.0. | White flashes in water: at each step the floor, walls and arms near the player go to full light for about a second, and for half of the headings every second splash lies 2.8 units in front, so the player walks into the sparkle sprite (radius 4.7) and it fills the bottom of the view. **V** (code and table), **G** (that this is the reported flash) | The splash was not in the port. Now `dtu_water_splash` (dtuobj.c, called from dtufoot.c) makes it, with no light (`dtu_is_water_splash`, lighting.c) and the offset on the right vector of the player (same length). BEHOLDER, `-testpos -40,-90,135,45 -testmove lookdown:4,forward:200`, frame 26: mean view brightness 54.4 with no splash, 63.0 (software) and 68.1 (GPU) with the original splash, 54.4 and 55.2 with the fixed one; the sparkles move from the center of the view to its edge. |
| D14 | 0x70EED, 0x2252F, 0x25591 | `$WEAPON` and `$SPELL` models of a level archive keep the `first_texture` of the EGG header plus 1 (models.md, "Textures"). The slots of the running table parse differ (the `$PLAYER` line of a player table takes more slots than its place holder). | Weapon and spell models with the texture of another slot. In MINES1, PRISON1 and TRAIN1 the header slot of `arrow.pof`, `flare.pof`, `cbbolt.pof`, `ltning.pof`, `ice.pof`, `acid.pof` and the `m_a`, `e_a`, `y_a` weapons is empty: bitmap 0, drawn black. **G** (simulation with the port parser, `test_dtudata -texcheck DIR -texorig`) | The port binds each model to the slots of its own table line (polyobj.c `load_polygon_model`). `test_dtudata -texcheck` over the 78 tables with HMC and HMT (156 loads): no weapon, arms or creature texture resolves to bitmap 0, an empty, a transparent (other than the `*empty` bitmaps) or a black bitmap. |

## 5. Defects of the port found by this work

| # | Where | Defect | Fix | Test |
|---|---|---|---|---|
| P1 | fvi.c `find_vector_intersection`, object.c `object_move_one` (line 2077), physics.c (2281, 768) | The early return for `HIT_BAD_P0` (the start point is not in the start segment) does not set `hit_data->n_segs` and `seglist`. The physics code copies the uninitialized list into `phys_seglist` (gdb: `n_phys_segs` 99, values like -927650856), and the trigger loop of `object_move_one` calls `find_connect_side(&Segments[garbage])`. | Not fixed here (files of the physics work). Proposed: in fvi.c set `hit_data->n_segs = 0` in the `HIT_BAD_P0` branch; in object.c test `phys_seglist[i]` and `[i+1]` against `0 .. Highest_segment_index` as the original does (0x69C27). | Soak: SIGSEGV in 9 of 50 runs of the first round (bones, hub3, hub4, mulhor1, thief0), all at gameseg.c:275 from object.c:2077, mostly after a warp. |
| P2 | dtuobj.c `follow_path` | The way back of a patrol with an empty path set the path index to -1 and `path_point` read `path[-1]` (the field before the array) as a segment number. | Return when the path is empty. | Soak check of the path index; no new report after the fix. |
| P3 | dtuobj.c (BRN buffer), dtuscript.c (level file test) | `dtu_load_file` allocates with the debug allocator (`mem_malloc`, dtufile.c includes mem.h). dtuobj.c and dtuscript.c freed with the C library `free`. The record of the block stayed in the table of mem.c; when a later block got the same address, mem.c checked the check bytes of the old size inside the new data: "MEM_OVERWRITE ... dtufile.c, line 134", 80001 bytes (also seen by the `-statetest` warp test, about 1 run in 5). | Both files include mem.h. | gdb watchpoint on the check bytes: the only writes were the heap code of `free` (dtuobj.c:919) and the next `malloc`. No MEM_OVERWRITE after the fix. |
| P4 | dtucombat.c | Turn undead was missing. | Section 2. | Section 2. |
| P5 | gameseg.c `trace_segs` | The port removed the `stackavail()` test of Watcom C. Two segments can send the trace back and forth without end (no list of visited segments), so the recursion overflowed the stack (SIGSEGV in `create_abs_vertex_lists`, FLAME.LVL, seed 5, frame 90). The original (0x4BCDF) has the test at 0x4BD40 (`stackavail() < 0x400`: return the old segment) and a DTU test at 0x4BCE9 (the trace does not go back into the start segment of `find_point_seg`, 0x4BE45). | A depth limit of 256: the whole trace returns -1 and `find_point_seg` searches all segments. | FLAME seeds 5, 6, 7 under the sanitizer build: no fault (the player dies in the level at frames 426 to 2623). |
| P6 | object.c 2251 `fix_object_segs` | `Int3` "cannot find segment" at the level start of BEHOLDR2 and MYRKUL: objects of the level files are outside the mine. | Not a crash (the Int3 prints). Not changed. | 18 lines in round 1 and 2. |

The original only reads the BRN matrix (0x9810D: `buf[A*100 + B/8]`). The
largest levels have 800 segments (BEHOLDER, DROW2, IBBALAR), so A and B
stay below 800 and the reads stay in the 80000 bytes. The builder 0x9786A
that writes the matrix is never called. **V**

## 6. The soak test

`engine/main/dtusoak.c`, option `-soak SEED` with `-level NAME -frames N
-mute` (`-fixedfps 30` for a fixed frame time). Each frame:

- Checks: the object list of each segment (range, no loop, no free
  object, `segnum`, `prev`, no object in two lists), each object in a
  valid segment and in its list, `num_objects`, robot and weapon ids, the
  player and the creatures inside the mine (every 16 frames), the
  inventory and the gold, and `dtu_objects_check()` of dtuobj.c (the
  animation state and frame, the target, the path, the mode of each
  creature). A problem prints "SOAK FAIL". A signal prints "SOAK CRASH"
  with the last action. `-soakabort` stops at the first problem,
  `-soaklog FILE` writes each action.
- Actions, one every 3 to 32 frames: walk, turn, side step, look, run
  (held keys), jump, attack, use (E), the keys O, F, M and T, turn undead
  at a random level, a spell of the player on a creature, a spell of a
  creature on the player or on a creature, learn, select and cast a spell,
  use, equip, drop and take items, direct damage, wake a creature, and a
  jump to a random segment or to a creature. The dialogue window gets the
  keys space, down and enter. The player is healed each frame
  (`-soakdeath` keeps the damage).
- `gamedata/run/bug/soak.sh` runs the levels and seeds under gdb and
  writes a summary line for each run.

### Results

Build `build-bug` (RelWithDebInfo, asserts and the debug allocator on),
`-fixedfps 30 -mute`, under gdb, 4000 or 5000 frames per run.

| Round | Runs | Frame limit reached | Player died | Crash | SOAK FAIL | MEM errors |
|---|---|---|---|---|---|---|
| 1, seeds 1 and 2, 29 levels, before P2, P3, P5 (the first BONES run also showed the P2 path reports and the P3 MEM_OVERWRITE) | 54 | 7 | most of the others | 9: P1 (bones, hub3 x2, hub4 x2, mulhor1, thief0) | path reports before P2, 0 after | P3 before the fix |
| 2, seeds 5 and 6, 30 levels, after P2, P3, P5 in part | 60 | 33 | 20 | 4: P1 (hub2, ibbalar, ibbalar2), P5 (flame, before the fix) | 0 | 0 |

"Player died": a single hit or spell above the hit points ends the run
through the death screen even with the heal of each frame. PLAYGRND.LVL
and MADLANDS.LVL are not in the game data (the level does not load).

### Sanitizers

- GCC 15 (MSYS2 UCRT64) has no UBSan runtime for MinGW, so the build uses
  `-fsanitize=undefined -fsanitize-trap=undefined -fno-sanitize=alignment
  -fstack-protector-strong` (`build-ubsan2`). AddressSanitizer is not
  available for MinGW GCC. clang 22 in `C:\Program Files\LLVM` has ASan
  only for the MSVC target; that needs an MSVC build of the port and SDL3
  and was not tried.
- With the GPU renderer every level stops on the first frame with a trap in
  `engine/3d/gpu3d.c` (`bitmap_texture`, `merge_texture`, lines 452 to
  594; the checks share one `ud2`, so the failed check is not known;
  turning off `pointer-overflow`, `object-size`, `null` and
  `nonnull-attribute` only moves the trap). Not diagnosed further.
- With `-software`: 16 runs (bones, bane, crypts, drow4, drow5, clan2,
  ypinn, hub1, seeds 3 and 4): 7 reached 4000 frames, 6 ended with the
  death of the player, 3 stopped on the bounds check in
  `find_connect_side` (P1). No other trap. FLAME seeds 5 to 7 after P5:
  no trap.
