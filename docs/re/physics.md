# Player movement of DTUGAME.EXE 1.3

Source: `E:\UNDAMONTAN\DTU\DTUGAME.EXE` (Watcom C, linear executable). Addresses are
those of the flat image with base 0x10000 (`gamedata/re/dtu_flat.bin`). Fixed point
values are 16.16 (`fix`), angles are `fixang` (65536 is one turn). `FT` is 1/64 second
(0x400).

Each statement has one of these marks:

- **V**: verified. Read from the code or the data of the executable, or measured in a
  file of the game.
- **G**: guess. The reason is given.

## 1. Structure

**V** Descent 1 moves all objects with `do_physics_sim()`. The executable does not.
`object_move_one()` (movement switch at 0x69B4A) calls for `MT_PHYSICS`:

| Object type | Function | Source file (assert string) |
|---|---|---|
| 4 player | 0x9DC66 | `pcons.c` |
| 2 robot | 0x9F153 | `pmon.c` |
| 5 weapon | 0xA03A8 | `pweapon.c` |
| 0x12 and render type 1 | 0x9FE68, only for the object number at 0x13D41F | `pmon.c` |

The object structure has 479 bytes (0x1DF). `Objects` is at 0x2B0614, `Segments` at
0x2206A4 (536 bytes each), `ConsoleObject` at 0x30DF14, `FrameTime` at 0x13CD2C,
`Controls` at 0x1E9A68, `Player_num` at 0x13B924.

Object fields that the movement uses: type 0x04, id 0x05, control 0x0A, movement 0x0B,
render 0x0C, flags 0x0D, segnum 0x0E, pos 0x14, orient 0x20, copy of orient 0x44, size
0x68, flag word 0x88, velocity 0xFC, thrust 0x108, mass 0x114, drag 0x118, rotvel 0x120,
rotthrust 0x12C, physics flags 0x13E, model number 0x162, animation state 0x170,
animation frame 0x172, movement state byte 0x1AE.

## 2. Functions

| Address | Descent 1 name | What Interplay changed |
|---|---|---|
| 0x2BFAE | `read_flying_controls` (controls.c) | Section 6. |
| 0x524xx to 0x54133 | `controls_read_all` (kconfig.c) | Look pitch, run, move mode. Section 6. |
| 0x3EFD9 | `find_vector_intersection` (fvi.c) | The query has two lines and two radii for each line (44 bytes, see below). |
| 0x3F29A | `fvi_sub` | A radius for each side. New results 4 and 5. Section 3. |
| 0x6B354 | `do_physics_sim_rot` | Only the heading turns. Drag is `drag*5` for each FT (Descent 1: `drag*5/2`). |
| 0x6B55E | new | Sets rotvel from rotthrust and keeps the object upright. Not used for the player. |
| 0x6C186 | new | "kneeHigh" test. Section 4.6. |
| 0x6C30E | new | Distance of the feet to the floor. Section 4.2. |
| 0x6C662 | new | Gravity and lift out of the floor. Section 4.3. |
| 0x6C8C8 | new | Fall damage. Section 4.7. |
| 0x6CB0F | new | Start of a fall: sets the state bit 0x20 and stores `pos.y`. |
| 0x6CB68 | part of `do_physics_sim` | Thrust and drag steps. Section 4.4. |
| 0x6CCDB | new | Up and down motion in flight and levitation. |
| 0x6CD45 | new | Hit of a floor side. Section 4.5. |
| 0x9DC66 | `do_physics_sim` | The player version (pcons.c). Section 4. |
| 0x9E88D | wall part of `do_physics_sim` | Wall hit. Section 4.5. |
| 0x9E9D8 | new | One frame of a jump. Section 5. |
| 0x9EBEB | new | Search for a wall that a thief can hold ("PCONS: you fell!"). Not in the port. |
| 0x9F093 | new | "Jump stopped." |
| 0x546CC | new | The jump key. |
| 0x6A490 | new, in object.c | "RLH: pushing (%d) above floor": puts an object on the floor when it is created. Not in the port. |
| 0x746B3, 0x748D9 | new, in polyobj.c | Width and height of an object from the bounding boxes of its model. |
| 0x76E9C | `render_frame` (render.c) | Eye position and view pitch. Section 4.8. |
| 0x40E94 | `calc_frame_time` (game.c) | FrameTime is limited to 0x1B4 (1/150 s) and 0x3333 (1/5 s). No frame cap. |

**V** The fvi query (register EAX of 0x3EFD9): +0 p0, +4 p1, +8 second p0, +0xC second
p1, +0x10 startseg, +0x14 rad, +0x18 rad2, +0x1C and +0x20 radii of the second line,
+0x24 thisobjnum (short), +0x26 ignore_obj_list, +0x2A flags. The result structure is
that of Descent 1, but `hit_wallnorm` (+0x20) is a pointer to the normal.

## 3. fvi changes

**V** `fvi_sub` tests each side of a segment with its own radius (0x3F58C):

| Side | Radius |
|---|---|
| 3 (bottom) | `rad / 2` |
| 1 (top) | `rad2 * 1.5` (double at 0x125296) |
| 0, 2, 4, 5 | `rad + 1.0` |

So the code takes side 3 as the floor and side 1 as the ceiling. **V** In the 39 level
files the first normal of side 3 has y of 0.9 or more in 18771 of 19650 segments. 257
have y near 0 (steep floor sides, used as the risers of stairs) and 57 have a negative y.

**V** New results (0x3FA08, 0x3F4DC): 5 when the result is `HIT_WALL`, the hit side is 3
and the moving object is not a weapon ("hit the floor"). 4 for a second kind of hit of a
robot or a player object (the object test 0x3EE70 returns 1, not 2).

**V** `fvi_sub` sets bit `1 << side` in 0x13D448 for each wall hit. pcons.c reverses the
velocity when the value is 10 (floor and ceiling in one query, "dubble wammo.").

**G** How the second line (+8, +0xC) is used was not read. It starts at the position
plus an offset of the model. It is probably the line of the second bounding box, for
the test against objects.

## 4. The player (pcons.c, 0x9DC66)

Order of one frame:

1. Return if the flag word 0x88 has a bit of 0x04004000.
2. Floor type: the two low bits of the byte at offset 1 of the bottom side (`side.pad`
   in Descent 1). The side is that of the segment below the player if there is one,
   else of the segment of the player (0x9DCC2). **V**
3. Step count for this frame (4.4).
4. 0x6CFF8 (moves an attacking object forward in some animation states. Not read in
   detail, not in the port), then the rotation 0x6B354.
5. `accel = thrust / mass`.
6. Not in flight or levitation: if the floor type is 1 or the flag 0x08000000 is set,
   `accel.x` and `accel.z` are halved. If the player structure has one of the bits 0x1C
   at 0x1D33BC, they are divided by 3 (meaning not known, not in the port). The flag
   0x1000 turns both off. The flag 0x2000 doubles `accel.x` and `accel.z`. **V**
7. Gravity (4.3), the flight motion, the wall hold test of the thief.
8. Thrust and drag steps (4.4), or the jump frame (section 5) when the state has 0x40.
9. `thrust = 0`.
10. Levitation (flag 0x200000): `velocity.x = velocity.z = 0` and `velocity.y >>= 2`,
    on each frame. Flag 0x80: `velocity.y` is not below -15.0 (0xFFF10000). **V**
11. The move (4.5).

### 4.1 Body size

**V** The executable takes the width and the height from bounding boxes that it keeps
for each animation frame of a model (0x73090 to 0x73500): for a box with the corners
`min` and `max` and the center `c`, the "xz radius" is `sqrt(dx*dx + dz*dz)` of the half
sizes and the height is `abs(min.y - c.y)`. A model has two such boxes for each frame
(tables at model +0x94/+0x98 and +0x9C/+0xA0).

**V** For the move of the player: `rad` = half of the xz radius of the first box of
frame 0, `rad2` = the height of the second box of the current frame (0x748D9, 0x9DF34).
For the floor search: `rad` = half of the xz radius, height = that of the second box
(0x6C52D). In 0x746B3 the width of a player is 5.0 at least.

**V** A player in a saved game of the original (`SAVE/SG1_0.LVL`, object 0, level
YPINN) stands with `pos.y` = -2.4981 on a floor at -10.0: the center is 7.5019 above
the floor. With the rules above this value is `height + rad/2`.

**G** The port has no bounding boxes for the animation frames. It uses `rad` = 2.5
(half of the least width 5.0) and height = 6.25, so that `height + rad/2` is 7.5. Only
the sum is verified. The rest pose of the player model in the port has y from -6.154 to
5.346 and an xz radius of 3.47, which would give `rad` 1.73.

Result of the guess: walls stop the player at 3.5 from the center, the head needs
9.375 above the center (`rad2 * 1.5`). A corridor of height 20 leaves 3.1 for a jump.
With the true values of the model this number can be different.

### 4.2 Distance to the floor (0x6C30E)

**V** A query from `pos` to `pos + (0, -(8.0 + height), 0)` with the radius `rad`, no
objects. For the results 5 and 1: `d = length(hit_pnt - pos) - height`. For the result 3
(start not in the segment): `d = -abs(last d)` and the text "RLH: %d has bad gravity!".
Else `d = 8.0` (0x80000). If `abs(d) < 0x1999` (0.1) then `d = 0`.

### 4.3 Gravity (0x6C662)

**V** `no_gravity` is true for a robot with the flag 0x20 of its type (flying) that is
not in animation state 9, for an object with a bit of 0x200880 in the flag word, and
for the state bit 0x80.

- `d < 0` (feet in the floor): `pos.y += min(-d, 2.0)`, one time on each frame, when the
  point `pos + (0, height of box 1, 0)` is in a segment. The state bit 0x80 is cleared.
  Without `no_gravity` the fall damage function is called.
- `d > 0` and gravity: `accel.y -= 3.0 * d` (0x30000). The byte 0x13D470 is cleared.
  If `d < 1.0` the fall bit 0x20 is cleared, else a fall starts (0x6CB0F) when the bit
  is not set.
- `d == 0`: the fall bit is cleared.

So gravity is not a constant. It grows with the distance to the floor, up to
`3.0 * 8.0 = 24.0` for each FT step (1536 for each second squared) when no floor is
within 8.0.

**V** The value `gravity` of a `$PLAYER` line is stored at +0x14 of the player record
(0x187B78, default 1.0). No use of it was found in these functions.

### 4.4 Thrust and drag (0x6CB68)

**V** As in Descent 1, for each FT step: `velocity += accel`, then
`velocity *= 1 - drag`. Then for the rest `k` of the frame (0 to 1):
`velocity += accel * k` and `velocity *= 1 - k*drag`. Changes:

- The step count is `FrameTime / FT`. If it is more than 2, it becomes 2 and `k` becomes
  0 (0x9DD4F). A frame that is longer than 3 FT loses the rest of its time.
- When the byte 0x13D470 is 0 (gravity acted in this frame), the rest step uses
  `1 - drag`, not `1 - k*drag`, and the byte becomes 1.
- `velocity.y` is not below -350.0 (0xFEA20000).

**V** Values of the player: mass 4.0, drag 0.075 (0x1333), in all level files and in the
saved game. `max_thrust` 16.0 and `max_rotthrust` 1.0 are the values that the port
reads from the `$PLAYER` line of the human male fighter (`hmc`).

Derived: `accel` is 4.0 for each step with a full forward key. The speed limit is
`accel * (1 - drag) / drag` = 49.33 units for each second (walk) and 74.0 (run).

### 4.5 The move

**V** `frame_vec = velocity * FrameTime`. The query goes from `pos` to
`pos + frame_vec` with x and z of `frame_vec` multiplied by 1.2 (0x13333). The flags are
9 (objects, segment list). The loop runs 8 times at most ("Stupid human. "). It ends
when the length of the velocity is 0x72 or less.

- Result 0: `pos += frame_vec` (not the end point of the query).
- Result 5 (floor side, 0x6CD45): `pos` = position at the start of the frame. The state
  bits 0x40 and 0x80 are cleared. If the kneeHigh test (4.6) is false, the part of the
  velocity along the normal is removed. The fall damage function is called. The loop
  goes on.
- Result 1 (wall, 0x9E47C and 0x9E88D): a jump stops. For the top side: the state bit
  0x40 is cleared and `velocity.y = 0`. `collide_object_with_wall()` gets the absolute
  dot product of the moved vector and the normal. Then
  `velocity -= normal * dot(velocity, normal) * 1.75` (0x1C000) and `pos` = position at
  the start of the frame. The loop goes on. So 0.75 of the velocity into the wall comes
  back.
- Results 2 and 4 (object): `pos` = start position, `collide_two_objects()`. An object
  of type 0x12 is pushed when a value of the player structure (0x1D33D3) is above a
  value of the object type.

After the loop: 0x69FB8 and 0x3D2BD (not read. Probably the segment of the object).

### 4.6 "kneeHigh" (0x6C186)

**V** Called for the result 5 with the hit side. `c1` = center of the hit side. A query
with the radius 0x1000 goes down from `pos`. `c2` = center of the side that it hits. If
`0 < c1.y - c2.y < 5.5` (0x58000): `velocity.y = c1.y - pos.y + 3.0`, the text
"obj %d hit a kneeHigh Segment." and the result is true.

Step height: there is no step test for the walls. A floor that is higher than the feet
lifts the player by 2.0 on each frame (4.3). The floor side stops the player only when
the test sphere of the radius `rad/2` at the center of the body touches it, which is
`height` above the feet.

### 4.7 Fall damage (0x6C8C8)

**V** Only with the fall bit 0x20 and no bit of 0x200880.
`fell = (start_y - pos.y) >> 17` ("descent feet": two units are one foot). If
`fell > 10`: `damage = fell/10 + rand() % (fell/10*5 + 1)`, half of it on floor type 1.
The texts are "RLH: Object %d fell %d descent feet." and "You fell %d feet!". A thief
can take hold of a wall here (0x9EBEB with 20.0). At the end `start_y = pos.y` and the
fall bit is cleared.

### 4.8 Eye

**V** `eye = pos + orient.uvec * eye_offset`, plus the y of a vector of the model for
the current animation frame (model +0x60). `eye_offset` is the value of the `$PLAYER`
line (+0x20 of the player record, default 3.0, copied to 0x30DF04). It is 5.0 for the
human male fighter. The value is negated when the player sleeps or is unconscious.

**V** When the byte 0x14530D is not 0 (1 walk, 2 run, 0 after a wall hit):
`eye.y += 0.325 * sin(phase)` (0x5333) and `phase += FrameTime * mode` turns.

**V** The view matrix is `orient * matrix(look_pitch, 0, 0)`. `look_pitch` is the word
at 0x13D9AE, limited to -0x3000 and 0x2FA0.

**G** The port does not have the vector of the animation frame. Eye height in the port:
7.5 + 5.0 = 12.5 above the floor.

## 5. Jump (0x546CC, 0x9E9D8)

**V** The jump key: in levitation it ends the levitation ("Levitate cancelled."). No
jump in flight, in a jump, or when the feet are more than 3.0 above the floor. Else
`jump_time = 0`, `base_y = pos.y` and the state bit 0x40 is set.

**V** A jump frame replaces the thrust and drag steps:

- First frame: `jump_vec = normalize(thrust)`, `jump_vec.y = 0`,
  `jump_vec *= (w << 13) + 0x8000`. `w` is the word at 0x1D33DF of the player
  structure. Times 1.5 in a run, times 2.0 with the flag 0x080000.
- `jump_time += FrameTime`. `step = (jump_time * 28.0) >> 16` (0x1C0000).
- Same step as in the last frame: `velocity = 0`.
- Step below 15: `velocity = jump_vec * (step - last_step)`,
  `velocity.y = table[step] - (pos.y - base_y)`.
- Step 15 or more: the end. `velocity = jump_vec * (15 - last_step)`,
  `velocity.y = base_y - pos.y`, and 1.5 less when `abs(floor distance + velocity.y)` is
  above 3.0. The state bits 0x40 and 0x20 are cleared.
- `velocity /= FrameTime`.

Table at 0x14530E (15 values): 0x18000, 0x30000, 0x48000, 0x60000, 0x6C000, 0x72000,
0x78000, 0x78000, 0x72000, 0x6C000, 0x60000, 0x48000, 0x30000, 0x18000, 0. The top is
7.5, the jump takes 15/28 second.

**G** The source of `w` is not known. The port uses 0: 0.5 for each step, 7.5 forward.

## 6. Controls

### 6.1 read_flying_controls (0x2BFAE)

**V**

- Returns when the player is dead or the flag 0x4000 is set.
- `forward_thrust_time == 0`: thrust x, z and velocity x, z are divided by 3, on each
  frame.
- `rotthrust.x = pitch_time * 3.0`, `rotthrust.y = heading_time * 3.0`.
- Forward: `thrust += dir * forward_thrust_time`. `dir` is the forward vector of the
  view matrix in flight (flag 0x800), the forward vector with y = 0, normalized, in
  levitation, and `fvec + (0,1,0) * fvec.y` else (the orientation of the player has no
  pitch, so this is `fvec`).
- Side: `thrust += (rvec + (0,1,0) * rvec.y) * sideways_thrust_time`, and
  `rotthrust.z = sideways_thrust_time`.
- `thrust.y += vertical_thrust_time`.
- Then, as in Descent 1, `thrust *= max_thrust / FrameTime` and
  `rotthrust *= max_rotthrust / FrameTime`.

### 6.2 controls_read_all

**V**

- Heading from the keys (0x533DC), as in Descent 1: the key time divided by 8 is added
  to the heading time of the last frame. No key: 0. The value is limited to FrameTime
  (0x540F0). The turn has its whole rate after 8 frames.
- Run (0x54033): when the shift state (0x13CC23 bit 8) or the byte 0x13BAED (run
  always) is set, forward and side time are multiplied by 1.5. Move mode (0x1452F0): 0,
  1 walk, 2 run. No walk or run with a state bit of 0xE0.
- Look keys (0x52BF9, 0x539DF): `look_pitch` changes by `key time * s / 8` as a fixang,
  `s` is the byte at 0x13BFEF (8 in the image).
- After a glance (0x53C9F): `look_pitch` goes to 0 by `(0x1194 + FrameTime) >> 2` on
  each frame.
- The head tracker code of Descent 1 writes the view angles at 0x38E338.

Derived: the turn rate with a full key is `3.0 * max_rotthrust / mass` = 0.75 for each
step, limit `0.75 * (1 - 0.375) / 0.375` = 1.25 turns for each second. **G** This is
fast. The value of `s` or of `max_rotthrust` in a real game may be lower.

### 6.3 Keys

From the manual on the disc (`MANUAL/DTU.PDF`, pages 34, 35, 48, 52). **V** as text of
the manual, the code of the key table was not read.

| Key | Action |
|---|---|
| 8 or Up | forward |
| 2 or Down | backward |
| 4 or Left, 6 or Right | turn |
| Z, X | side step left, right |
| Alt | side step mode on and off: the turn keys side step |
| Left Shift | run |
| Caps Lock | run always, on and off |
| Space | jump |
| Page Up, Page Down | look up, look down |
| Home, End | glance up, glance down |
| Insert, Delete | up, down in flight and levitation |
| V | immersive mode on and off: the mouse turns and looks |
| A | activate. A thief at a wall starts to climb |

The manual: levitation moves only up and down, flight moves in all directions. No
crouch and no swimming are in the manual or in the code that was read. Floor type 1
only makes the player slow (and has a splash object in the fall function).

**G** In the port: Home and End act as the look keys and the view returns after the
key. The mouse scale and the joystick (x turns, y walks) are guesses.

## 7. State and flag bits

**V** State byte 0x1AE: 0x20 falling, 0x40 jumping, 0x80 holds a wall (climb), 0x02
(the position is not reset after a hit), 0x01 and 0x04 (used in object.c for dead
objects).

Flag word 0x88: **V** 0x200000 levitation (cleared with "Levitate cancelled."),
0x800 thrust in the view direction and no gravity, 0x80 no gravity and slow fall,
0x04004000 no movement. **G** 0x800 is the Fly spell, 0x80 is Feather Fall, 0x2000 is
Haste, 0x080000 is the Jump spell, 0x08000000 is a slow effect.

## 8. Frame rate

**V** `calc_frame_time` has no frame cap. FrameTime is between 1/150 and 1/5 second.

Reference of the port: a frame time of 3 FT = 0xC00 (21.33 frames for each second),
`DTU_REF_FRAMETIME`. Evidence:

- The step limit of 4.4: 3 FT is the longest frame in which the executable does all
  steps. A slower machine has less thrust and gravity for each second.
- The function at 0x40F96 sets a quality value from FrameTime that is best at 0x1000
  (16 frames) or less time, so 16 frames and more was the normal case.
- The jump has 28 steps for each second: a step for each frame at about that rate.
- A 1997 computer drew this game with 15 to 25 frames for each second (not measured).

Places that depend on the frame rate, and the setting of the port for each. Each
setting is 1 (corrected) by default. The option `-origphys` sets all to 0.

| Setting | Rule of the executable | Corrected rule |
|---|---|---|
| `Dtu_fix_physics_steps` | 2 thrust and drag steps at most for each frame, then a linear rest step. The rotation has no limit but a linear rest step: the turn rate is 1.25 at 21 frames and 1.76 at 200 frames | All whole FT steps of the passed time, the rest goes to the next frame |
| `Dtu_fix_frame_drag` | While gravity acts: one more full drag on each frame | One more drag for each reference frame |
| `Dtu_fix_stop_decay` | No forward input: velocity x, z divided by 3 on each frame. Levitation: `velocity.y >>= 2` on each frame | One time at once, then one time for each reference frame |
| `Dtu_fix_floor_push` | Lift out of the floor: 2.0 at most on each frame | 2.0 for each reference frame, as a rate |
| `Dtu_fix_turn_ramp` | The turn keys have the whole rate after 8 frames | After 8 reference frames (0.375 s) |
| `Dtu_fix_jump_exit` | The velocity after the last step of a jump is the step divided by FrameTime | The step divided by the reference frame time |
| `Dtu_fix_look_return` | Return after a glance: `(0x1194 + FrameTime) >> 2` on each frame | The value of the reference frame, as a rate |
| `Dtu_fix_fly_bob` | Phase of the flight motion: 0.0333 turns on each frame | For each reference frame |

Not corrected: the 8 tries of the move loop for each frame, the bounce at a wall (one
for each frame while the player pushes), the jump steps (28 for each second against the
frame times).

## 9. The port

Files: `engine/main/physics.c` (end of the file), `physics.h`, `controls.c`, `fvi.c`,
`fvi.h`, `object.c`, `render.c`, `inferno.c`.

Options: `-fly` (the ship of Descent 1), `-origphys`, `-fixedfps N` (each frame has
1/N second), `-testmove SCRIPT`, `-movelog FILE` (`-` is the standard output).

`-testmove forward+run:2000ms,left:30,wait:10,jump:1`: actions `forward`, `back`,
`left`, `right`, `slideleft`, `slideright`, `run`, `jump`, `lookup`, `lookdown`, `up`,
`down`, `wait`. A length is a number of frames, or a game time with `ms`.

Not in the port: the climb of the thief (0x9EBEB, climb thrust in 0x2C144), the push of
objects of type 0x12, the result 4 of fvi and the second line of the query, the
"dubble wammo" case, the code at 0x9E340 (hit side in another segment), 0x6CFF8, the
divide by 3 for the player bits 0x1C, the splash object, the bounding boxes of the
animation frames, the movement of robots (pmon.c) and weapons (pweapon.c).

Differences: a key is down for the whole frame or not at all (the executable uses the
time that the key was down). The search for the floor tests only the floor sides
(`Fvi_dtu_body` 2), so that a wall near the player is not taken as a floor.

## 10. Tests (port, corrected rules)

Fixed frame times, 20, 60 and 200 frames for each second.

| Test | 20 | 60 | 200 |
|---|---|---|---|
| YPINN, BANE, DROW1: 4 s with no input, height of the center above the floor | 7.5000 | 7.5000 | 7.4698 |
| the same, change of the height after the first second | 0 | 0 | 0 |
| YPINN: walk 1.5 s, turn 0.2 s, walk 0.3 s. End x, z | 3.938, 122.349 | 3.767, 121.802 | 3.566, 121.790 |
| BANE: walk 2.7 s over a step up of 3.5 and a ledge down of 3.5. End z | 143.351 | 143.146 | 142.975 |
| DROW1: walk 1.2 s, turn 0.15 s, walk 0.3 s. End x, z | 8.643, -22.402 | 7.998, -22.488 | 7.929, -22.521 |
| PRISON: jump, then walk and jump (path of 21). End z | 388.763 | 390.519 | 390.206 |

With `-origphys` the YPINN test ends at (3.191, 116.156), (5.585, 118.337) and
(5.971, 114.492), and the BANE test at 138.417, 141.045 and 141.099.

Other results: the walk speed limit is 49.33 and the run speed limit 74.0. In DROW1 the
player stops at a wall after 80 units and moves 0.3 forward and back while the key is
down (the bounce of 4.5). In BANE a wall at z 187 stops the player and lets the player pass after about 0.5 s (probably a door that opens at the hit). In YPINN
(height 20) a jump ends at 3.0 at the ceiling. In PRISON (height 30) it goes to 7.5.

## 11. Creatures (pmon.c, 0x9F153)

Read with the port of the creatures (`dtu_creature_physics_sim` in
`engine/main/dtuobj.c`). The function has the form of the player movement and
uses the same helpers (0x6C662 gravity, 0x6CB68 thrust and drag, 0x6CD45 floor
hit, 0x6C8C8 fall damage, 0x9E88D wall hit).

1. Return if the object is held (status 0x04004000), has object flag 0x80 or
   one of the flags 7, or has control type 11. Return if the activity byte
   0x1CC is 0 (but for the rat type): a creature that is not active does not
   move and has no gravity.
2. The velocity is multiplied by 0.5 (0x9F286), see ai.md "How a creature
   moves". The transposed orientation is copied to object 0x44.
3. 0x6CFF8, the weapon test (ai.md, "Melee attack"), then 0x6B354 (not read).
4. Acceleration = thrust / mass. On floor type 1 a creature that does not fly
   gets half of it in x and z and bit 0x20 at 0x15F (and a splash object when
   the switch at 0x145360 is set).
5. Gravity 0x6C662. A creature that does not fly and is in CURRENT_STATE DEAD
   gets bit 0x20 at 0x1AE: from then on the velocity in x and z is divided by
   8 on each frame.
6. 0x6CCDB and 0x6CB68. The thrust of a creature that does not fly is cleared.
7. The body cylinder 0x748D9. The radius of the move query is one fourth of
   its radius (0x9F605). The query tests objects unless the animation state is
   DIE.
8. The move loop, 5 tries at most. A velocity of 10 (fix units) or less ends
   it with `retry_count` 0. Results of the query:
   - nothing hit: move, `retry_count` 0, blocker fields cleared.
   - floor: 0x6CD45 and 0x6C8C8.
   - wall: 0x9E88D, `retry_count` 1, end.
   - object: back to the start position, `retry_count` = clock parity, the
     blocker (0x138) and its frame count (0x13A) are set. Scenery whose
     `pushable` value is below `str` of the creature gets 8 times the velocity
     and thrust of the creature and movement type 1. A creature that is hit
     goes to the ignore list. Then `collide_two_objects`.
   - bad start point: the velocity is negated, `retry_count` 1.
9. After the loop: `update_object_seg`, the step sound on the first frame of
   WALK and RUN, the attack sound in ATTACK. An object that is in no segment
   gets object flag 0x40 and render, control and movement type 0.

The port has the steps 1, 2, 3 (weapon test only), 5, 6, 7 and 8 without the
push of scenery, the floor types, the sounds and the fall damage.
`Dtu_fix_creature_move` (on by default) makes three parts independent of the
frame rate: the factor 0.5 of step 2 acts per 3/64 second (v goes to the
speed of the state with the same time constant as at that frame time), the
steps of thrust, drag and gravity are all done (the executable does two at
most per frame), and the lift out of the floor is 2.0 per 3/64 second.
