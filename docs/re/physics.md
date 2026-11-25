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

**V** The object test 0x3EE70 (eax hit point, edx p0, ebx p1, ecx rad, then rad2, the
second line and its radii, the result word, the object). It asks 0x748D9 for the body.
A model with the box flag 1 (and render type 1): 0x4F7D5(p0, p1, `obj->pos`, the vector
(0,1,0) turned by the orientation of the object, rad, rad2, `body_radius[0]`, the half
height), where the half height is `torso_half_height[frame]` with the torso box (flag 8)
and `body_half_height[0]` else; the torso center of 0x748D9 is not used. The result is 2
on a hit. Other objects: the sphere test 0x3ED76 with `size + rad`. 0x4F7D5 is a capped
cylinder about the position, radius `body_radius[0] + rad`, half height `+ rad2`:

1. A start point in the cylinder (the height along the axis within the half height, the
   distance across within the radius) is a hit at distance `body_radius[0] + rad`, at
   the point of the surface toward p0.
2. Else no hit for a line shorter than 0x3F, or a line that points away from the
   position (direction . (pos - p0) < 0).
3. Else the ray test of a capped cylinder of Graphics Gems IV (Cychosz and
   Waggenspack): 0x4F4B6 the infinite cylinder (a miss if the entry or the exit is
   behind p0), 0x4F68D the two cap planes. A hit needs the entry distance 0 or more and
   at most the length of the line; it is the distance.

The object loop of fvi_sub skips the object itself, objects with the flag 2 or 0x40
(0x40 is `pass_thru`, objects.md), the ignore list and related weapons (0x55369). A
wall hit replaces a nearer object hit, except the floor (side 3) against an object hit
(0x3F8FF). The port uses this test for the swing of the player (`dtu_body_query_test`
in fvi.c, combat.md A3).

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

**V** The executable takes the sizes from arrays of the polygon model that hold one
value for each animation frame (models.md: `body_radius` +0x98 and `body_half_height`
+0x94 with n_frames + 1 entries, entry 0 the box of all frames; `torso_half_height`
+0x9C and `torso_radius` +0xA0 with n_frames entries). The tool that built the
archives computed them, the port reads them from the file. "The model of the hand" is
the model of the player object (object 0x162: race, sex, class and the letter of the
weapon in the hand, 0x43A5A), "the frame" its animation frame (object 0x172).

| Use | Address | Model | Value |
|---|---|---|---|
| move query `rad` | 0x748D9, 0x9DF34 | model 16 (0x748F4: type 4 takes model number 0x10) | `body_radius[0] / 2` |
| move query `rad2` (ceiling side 1.5 times) | 0x748D9 | model 16 | `torso_half_height[frame]` (box flag 8), else `body_half_height[0]` |
| floor search radius | 0x6C52D | the hand | `body_radius[frame] / 2` (the index is the frame, not frame + 1) |
| floor search height | 0x6C56D | the hand | `torso_half_height[frame]` |
| head test of the lift (4.3) | 0x6C7BE (0x746B3) | the hand | `body_half_height[0]` |
| kneeHigh query (4.6) | 0x6C234 | the hand | `body_half_height[0]` |
| body of the player for objects | 0x748D9 | model 16 | center `pos + torso_center[0]`, radius `torso_radius[0]`, height `torso_half_height[frame]` |
| 0x746B3 (creature reach) | 0x746B3 | the hand | radius `max(body_radius[0], 5.0)`, height `body_half_height[0]` |

The player models are the first models (the archive of the player is read before
that of the level, piggy.c 0x70303), so model 16 is the 17th model of the archive of
the player: `hmcr_p.pof` for a human male cleric, `dmfs_p.pof` for a dwarf male
fighter. **G** The use of a fixed model number looks like a rest of a test, but it is
what the code does.

**V** A player stands where the floor distance is 0: the center is
`torso_half_height[frame] + body_radius[frame] / 4` above the floor (the floor side
takes one half of the radius of the query, section 3). The saved game of the original
(`SAVE/SG1_0.LVL`) has the player object with model 1, state STAND, frame 0, and
`pos.y` = -2.4981 on a floor at -10.0 (7.5019 above it). With the archive of a human
male thief (`HMT.EGG`, the archive on the hard disk), model 1 is `hmtd_p.pof`:
5.75 + 7.0078 / 4 = 7.5020.

Values (port, `-bodylog`, the default weapon of the hand):

| | human (hmc, hand `hmch_p.pof`) | dwarf (dmf, hand `dmfh_p.pof`) |
|---|---|---|
| move `rad`, walls stop the center at `rad + 1.0` | 3.594, 4.594 | 3.504, 4.504 |
| move `rad2`, ceiling side `rad2 * 1.5` | 5.75, 8.625 | 4.55, 6.825 |
| floor search radius, frame 0 | 3.582 | 3.582 |
| center above the floor, frame 0 | 7.541 | 6.341 |
| `eye_offset` of the `$PLAYER` line, eye above the floor | 5.0, 12.541 | 4.2, 10.541 |

`torso_half_height` is the same in all frames of these models (5.75, 4.55), so `rad2`
does not change with the frame. `body_radius[frame]` does: in the attack frames (44 to
59) it is 1.6 to 2.7, so the floor search radius is 0.8 to 1.35 and the center goes
down by up to 0.5 while the player attacks. Back in frame 0 the lift of 4.3 puts it up
again. A change of the frame does not change the radius of the move (it is entry 0 of
model 16), so it cannot move the body into a wall. A higher stand height is reached
with the lift of 4.3, which moves only when the head test point is in a segment: the
port does the same.

### 4.2 Distance to the floor (0x6C30E)

**V** A query from `pos` to `pos + (0, -(8.0 + height), 0)` with the radius `rad`, no
objects. `rad` and `height` are the floor search values of 4.1. For the results 5 and 1: `d = length(hit_pnt - pos) - height`. For the result 3
(start not in the segment): `d = -abs(last d)` and the text "RLH: %d has bad gravity!".
Else `d = 8.0` (0x80000). If `abs(d) < 0x1999` (0.1) then `d = 0`.

**V** This is the path of the player and the creatures (0x6C527). For scenery (type
0x12) the query has the flag 1 (objects), and the results 2 and 4 count too (0x6C358,
0x6C401): scenery lies on other scenery, for example a chest on a crate.

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

These two clears end a fall with no damage (0x6C871, 0x6C89A). The port can give them the
damage: `Dtu_fix_fall_land_player` and `Dtu_fix_fall_land`, see 4.7.

The bit 0x80 of the mask 0x200880 is Feather Fall, so a player with that effect has no
gravity in 1.3. The port gives the gravity back to it: `Dtu_fix_feather_fall` (section 8).

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
  value of the object type. The loop goes on with the next try and the object is not
  ignored, so scenery stops the player like a wall (ai.md, "Push of scenery"). An
  object is ignored only after more than 30 hits in a row, for a try that goes away
  from it (0x9E11A).

**V** More on the results 2 and 4 (0x9E2C9 to 0x9E797). On each result that is not 0
the position is first the hit point of the query (0x9E2EC; the end point with the state
bit 2 or 0x80, 0x9E2E2). It goes back to the start of the frame only while the counter
0x1452F1 (hits of the same object in a row) is below 9 (0x9E598): from the 9th hit the
player stays at the hit point. The block does not change the velocity, the state byte
or the fall. `collide_two_objects` (0x29455) calls `collide_player_and_dressing`
(0x25FB0) for the player and scenery, and that calls 0x261DB when bit 0 of the scenery
flags is clear: `n = normalize(scenery.pos - player.pos)`; if `n.y < -0.8` then `n =
(0,-1,0)` and the byte 0x13A950 becomes 1 (0x2622D); if `n.y > -0.5` then `n.y = 0`;
then `velocity -= n * dot(n, velocity)`. So a hit from above takes away the velocity
down, and 0x13A950 marks a player on top of scenery: the jump key (section 5) permits a
jump with it. At a speed of 42.67 or more the function gives the alert level 2 and plays
the sound 70 (0x26187). A hit of an object does not end a jump (0x9E946, which calls the
jump stop, has no caller).

**V** The body test 0x4F7D5 for a start point inside of the grown cylinder gives the
hit point `pos - R * (unit vector to the axis)`: on the side of the cylinder, at the
height of the center of the scenery (0x4F88B to 0x4F8AA). A ray that starts within the
radius and is not parallel to the axis misses (0x4F4B6 needs an entry at 0 or more).

**G** (derived from the rules above, no run of the executable) The executable wants a
player to stand on scenery and to jump from it, but the result is not stable. The jump
has a speed forward, so the ray down over the top misses and ends inside of the
cylinder. In the next frame each try hits, the velocity down goes, and the player is
0 to 3 units in the top. With a velocity in x and z the tries go on, the counter
reaches 9 in the second frame, and the player goes to the hit point at the side of the
scenery. A walk on the top is not possible (each try starts inside). The port without
Improved physics has no 0x261DB and no 0x13A950: the player hangs over the scenery and
pushes it (section 13.2).

After the loop: 0x69FB8 and 0x3D2BD (the lava, 4.9).

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

The gravity (0x6C662, 4.3) clears the fall bit with no damage when the feet are less than
1.0 above the floor at the start of a frame (0x6C871, 0x6C89A); the damage comes only from
a body in the floor or a hit of the floor in the move. So whether a long fall hurts depends
on the frame rate and on where the frames fall. Port test with `-origphys` at 60 steps per
second (`-fixedfps 60`), the player dropped from the center of BONES segments, five start
frames each: segment 495 (30.0 to the floor of 494, 11 descent feet) gave no damage in 5 of
5; segments 491 (20 or 21 feet) and 343 (12 feet) took the damage. `Dtu_fix_fall_land_player`
(physics.c, 1, `-origphys` clears it, the player sibling of `Dtu_fix_fall_land` of the
creatures, which `-origobj` clears): that end of a fall goes through 0x6C8C8 too, so it takes
the damage above 10 feet. With it segment 495 took 1 to 5 in 4 of 5 (the fifth fell 10 feet,
no damage), 491 and 343 in 5 of 5. Stairs and steps give no damage with it: BONES 114 to 93
(a 2.5 step, 3 feet), DROW1 486 to 481 (steps of 2.3 to 4.9, 2 feet), THIEF1 383 to 381 and
KOBOLDS from 2, walking and running. The objlog (`-objlog`) has the line "fall: the player
fell N descent feet" for each end of a fall of 1 foot or more that goes through 0x6C8C8.

For a creature (verified in the bytes, 2026-11-24): `start_y` is the table 0x30E2D4, one
dword per object, written by add_npc (0xAC764), 0x1038B (0x103F8), the end of each call
of 0x6C8C8 and the start of a fall, 0x6CB0F (gravity, 1.0 or more above the floor, fall
bit clear). The restore of a level writes it for the player only (0xBB552). The damage
goes to `0x29A9C(creature, no attacker, damage, -1)`: no experience. Callers: the floor
hit of the move (0x9F903) and the gravity when the feet are in the floor (0x6C839).
0x6C871 clears the fall bit with no damage when the feet are less than 1.0 above the
floor at the start of a frame, so a fall ends with damage only when one frame takes the
body from 1.0 or more above the floor into it. With the rules of 1.3 (`-origobj`) the
goblin of THIEF2 (13 descent feet) took the damage in 1 of 9 port tests (15, 20 and 30
frames per second, three activation frames each), at 15. The port: `creature_fall_damage` (dtuobj_move.c). `Dtu_fix_fall_land` (1,
`-origobj` clears it): the end of a fall less than 1.0 above the floor takes the damage
too, also for a creature that the AI made inactive before its physics ended the fall. The
splash vclip of a fall on water or lava (0x6C93C) is not in the port, for the player
neither. The start of a fall by the gravity (0x6CB0F) is not in the port for a creature:
the floor probe of the port is thin (`creature_floor_dist`), so the knee lift leaves a
creature 1.0 or more above the floor under its center at a step, and the fall bit (x and
z velocity / 8) stopped it there (questtest `kobolds_walk_saveload`: the walk to segment
43 ended in segment 36). So in the port only the trap 0x9493A starts the fall of a
creature, and a creature that walks off a ledge takes no fall damage.

### 4.8 Eye

**V** `eye = pos + orient.uvec * eye_offset`, plus the y of a vector of the model for
the current animation frame (model +0x60). `eye_offset` is the value of the `$PLAYER`
line (+0x20 of the player record, default 3.0, copied to 0x30DF04). It is 5.0 for the
human male fighter. The value is negated when the player sleeps or is unconscious.

**V** When the byte 0x14530D is not 0 (1 walk, 2 run, 0 after a wall hit):
`eye.y += 0.325 * sin(phase)` (0x5333) and `phase += FrameTime * mode` turns.

**V** The view matrix is `orient * matrix(look_pitch, 0, 0)`. `look_pitch` is the word
at 0x13D9AE, limited to -0x3000 and 0x2FA0.

**V** The vector is `frame_eye[frame]` of the model of the hand (0x76EC8, frame at
object 0x172). It is 0 in the frames of the player models that were checked (hmc,
dmf), and is used by the port. Eye height in the port: 12.541 above the floor for a
human, 10.541 for a dwarf (4.1).

### 4.9 The lava (0x3D2BD)

**V** (the bytes, 2026-11-24) Called at the end of the move of the player (0x9E7B8)
and of a creature (0x9FE54). Nothing for a creature with `fire_immune`
(`Robot_info+0x98` bit 0x08, 0x3D2DA) and nothing while bit 0x20, 0x40 or 0x80 of
`+0x1AE` is set (the fall, the jump, the climb or Levitate, 0x3D323). The segment is
`children[3]` of the segment of the object, or the segment itself when that is -1
(0x3D2FD). The side is the dword 0x30F494, side 3 when it is -1. The move loops copy the
hit side of each query into it (0x9E2BA for the player, 0x9F860 for a creature), so it
is the hit side of the last query of any object. When `side.flags & 3` is 3 (lava,
0xA6F89) and the player clock (the dword at 0x1D339F of `Players[Player_num]`) is 1.0 or
more from the time at 0x13B1B4, the time is set, the fire die count of Fireball
(0x1B5EA4) becomes 1 and `0x911AA(object, object, 16, 1)` burns the object: 1d6 of fire,
a save for half. The time at 0x13B1B4 is one for all objects.

A creature runs its move only while it is active (0x9F196), so an inactive creature in
the lava does not burn. The trap that drops the objects of a floor (0x9493A) sets bit
0x20 (0x94984) and wakes each creature with 0xA6505(creature, 1) (0x949CB). The 1 is the
see sound; 0xA6505 keeps the hostile bit (+0x1B0 bit 0x400000): a peaceful creature
stays peaceful, with target -1 (0xA66A4). The burn gives experience only when the id
byte of the caster (+5) is Player_num (0x915E7, then 0x4EA50 at 0x91643). The lava passes
the object itself as the caster, so the death of a goblin in the lava gives no
experience; the kill counters of the player go up all the same (0x91652, 0x91659). A
creature of type 0 that burns would give it (the port tests the object type of the
caster, `spell_deal` in dtuspell.c). The fall damage has no attacker. 0x6C662 clears
bit 0x20 when the feet are less than 1.0 above the floor
(0x6C871, 0x6C89A) and 0x6C8C8 clears it in the floor.

The port: `dtu_lava_frame` (dtuobj_move.c), from `dtu_player_physics_sim` and
`dtu_creature_physics_sim`. Until 2026-11-24 the port had no lava damage.
`Dtu_fix_lava` (1, `-origobj` clears it): each object has its own time; the object burns
when the wall or floor that its own move query hit in the frame is lava, or when its
feet are less than 1.0 above the floor below it and that floor is lava; a creature that
does not move (inactive, held, flag 0x40) burns too. The fall of a creature: 4.7.

## 5. Jump (0x546CC, 0x9E9D8)

**V** The jump key: in levitation it ends the levitation ("Levitate cancelled."). No
jump in flight, in a jump, or when the feet are more than 3.0 above the floor. Else
`jump_time = 0`, `base_y = pos.y` and the state bit 0x40 is set. A jump is also
permitted with the state bit 0x80 or with the byte 0x13A950 (the player is on top of
scenery, section 4.5; 0x5473A). The key clears 0x13A950 (0x54789).

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
the manual. The key table of the executable (62 items, its defaults and its screen) is
in `docs/re/controls.md`; it agrees with this list.

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

Defaults of the port, chosen by the owner of the project (not the original): W and S
forward and back (with the arrows), A and D side step (Z and X are not side steps), E
Activate, H Disarm (key table item 50, default D in the executable: 0x53E91 calls
0x43A5A(-3), the empty hand, "KCFG: Disarming"), P the character screen (Stats, S in
the executable), Z Rest (as R). The other keys are as in the executable. All keys come
from the key table and can be changed in the Controls menu (`docs/re/controls.md`).

**G** In the port: Home and End act as the look keys and the view returns after the
key. The mouse scales are guesses. The joystick axes follow the key table (x turns,
y looks, the defaults of the executable).

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
| `Dtu_fix_turn_ramp` | The turn keys have the whole rate after 8 frames | A function of the time the key is down, with the steps of the executable at the reference frame time (section 13) |
| `Dtu_fix_jump_exit` | The velocity after the last step of a jump is the step divided by FrameTime | The step divided by the reference frame time |
| `Dtu_fix_look_return` | Return after a glance: `(0x1194 + FrameTime) >> 2` on each frame | The value of the reference frame, as a rate |
| `Dtu_fix_fly_bob` | Phase of the flight motion: 0.0333 turns on each frame | For each reference frame |
| `Dtu_fix_jump_path` | The jump steps of the table, 28 for each second, and the velocity of a step divided by the frame time (section 5) | A throw with a constant gravity and the same height and air time (section 13) |
| `Dtu_fix_ladder` | The climb ends below the top of a ladder (section 12), the climb thrust goes into the wall and the wall throws the player back on each frame | Section 13 |
| `Dtu_fix_wall_slide` | 0.75 of the velocity into a wall comes back, on each frame while the player pushes, and the move of the frame is lost | The velocity into the wall is taken away, the velocity along it stays |
| `Dtu_fix_wall_overlap` | (port) A body that starts inside the reach of a wall (rad + 1.0) hits it with each move and cannot move any more | The body is moved out of the reach of the wall along its normal (section 13) |
| `Dtu_fix_feather_fall` | Feather Fall (flag 0x80) stops the gravity of the player (4.3): the player stays at the height where a flight ends or where the floor ends | Feather Fall keeps the gravity. The limit of the velocity down (15.0) and the fall with no damage stay (bugs.md D27) |

Not corrected: the 8 tries of the move loop for each frame.

Since the fixed step (section 14) the game runs with a frame time of 1/60 s at each
picture rate, and these settings matter only without it (`-fixedfps N`, `-nofixedstep`).

## 9. The port

Files: `engine/main/physics.c` (end of the file), `physics.h`, `controls.c`, `fvi.c`,
`fvi.h`, `object.c`, `render.c`, `inferno.c`.

Options: `-fly` (the ship of Descent 1), `-origphys`, `-fixedfps N` (each frame has
1/N second), `-testmove SCRIPT`, `-movelog FILE` (`-` is the standard output).

`-testmove forward+run:2000ms,left:30,wait:10,jump:1`: actions `forward`, `back`,
`left`, `right`, `slideleft`, `slideright`, `run`, `jump`, `lookup`, `lookdown`, `up`,
`down`, `wait`. A length is a number of frames, or a game time with `ms`.

The climb of the thief (0x9EBEB, the climb thrust of 0x2C144), the push of objects of
type 0x12 and the divide by 3 for the player bits 0x1C are in the port since the thief
work (dtuthief.c, dtuobj.c; combat.md B7, ai.md).

Not in the port: the result 4 of fvi and the second line of the query, the
"dubble wammo" case, the code at 0x9E340 (hit side in another segment), 0x6CFF8, the
splash object, the movement of robots (pmon.c) and weapons (pweapon.c).

The sizes of the body come from the models (4.1, `dtu_player_body()` at the end of
`physics.c`, `dtu_get_body_cylinder()` in `dtuobj.c`). Without the models (for
example `-fly`) the constants of `physics.h` (2.5, 6.25) are used. `dtu_get_body_size()`
(0x746B3) still has the constants for the player, so the reach of the creatures does
not change: with the rule of 0x746B3 it would grow by about 4.

Test options: `-bodylog FILE` (one line for each frame: the model of the hand, the
frame and the sizes), `-archive RSC` (the archive of the player, for example `dmf`),
`-startseg SEG.SIDE` (the test starts at the center of the segment, facing the side).

Differences: a key is down for the whole frame or not at all (the executable uses the
time that the key was down). The search for the floor tests only the floor sides
(`Fvi_dtu_body` 2), so that a wall near the player is not taken as a floor. The head
sphere of the capsule (13.1) tests only the walls (`Fvi_dtu_body` 3).

`-fixedfps N` holds from the first frame (game.c). Before, the first frame had the time
of the load, so two runs of a test did not give the same moves.

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

Push of scenery (YPINN, `-testpos -80,-2.46,33,0`, `-testmove
wait:300ms,forward:600ms,wait:3000ms`: a walk of 0.6 s into the chair obj 5 at z 48
(`pushable` 1), the table obj 3 at z 59 (`pushable` 16) behind it, strength 17). End
z of the chair, the table and the player:

| Frames per second | Setup 1.0 | Now, Improved physics on | Now, off |
|---|---|---|---|
| 30 | 63.2, 72.1, 54.0 | 56.4, 62.2, 45.4 | 55.3, 62.2, 47.0 |
| 60 | 67.0, 71.8, 53.6 | 56.3, 63.3, 47.1 | 56.0, 63.3, 48.2 |
| 144 | 60.9, 72.4, 53.5 | 59.3, 65.2, 50.0 | 59.3, 65.8, 50.6 |

Setup 1.0 let the player walk through the chair (player end 54 > chair start 48)
and pushed the table 13 units. Now the chair stops the player and moves 8 to 11
units ahead of it. How far the executable moves the chair in this test was not
measured (no run of DTUGAME.EXE).

These numbers are from before the corrections of the push in objects.md, "Pushed
scenery" (the measurements of the push are there now).

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
   Then the lava 0x3D2BD (4.9, 0x9FE54).

The port has the steps 1, 2, 3 (weapon test only), 5, 6, 7 and 8 without the
push of scenery, the floor types and the sounds. The fall damage (4.7) is in the port
since 2026-11-24.
`Dtu_fix_creature_move` (on by default) makes three parts independent of the
frame rate: the factor 0.5 of step 2 acts per 3/64 second (v goes to the
speed of the state with the same time constant as at that frame time), the
steps of thrust, drag and gravity are all done (the executable does two at
most per frame), and the lift out of the floor is 2.0 per 3/64 second.

## 12. Ladders (0x951DE, 0x2C144, 0x9EBEB)

**V** A ladder is a side whose texture (tmap_num2, or tmap_num when tmap_num2 is 0) is
in the range of the texture `ladder` and the three textures after it (0x13A86B to
0x13A867, combat.md B7). No segment or side flag marks a ladder. In the level files
the texture table has the name `ladder` at index 5 to 8 and 119. Solid sides with
the textures 5 to 8: 2 in BANE, 7 in DROW1, 4 in HUB1, 6 in FLAME, 3 in PRISON, none in YPINN (for example FLAME
segment 432 side 0 and 470 side 4, the wall z = 265 from y 39.9 to 64.9, with the
floor of segment 462 at 64.9 behind its top).

**V** The player takes hold with Activate (0x951DE): the side faces the player
(dot of the normal and the forward vector at most 0.25) and the plane is nearer than
10.0. The state bit 0x80 is set, with no roll.

**V** The climb thrust (0x2C144): forward gives `normalize(h + up)`, back gives
`normalize(h - up)`, `h` the forward vector in x and z: up or down and into the wall.
The size is the key time `>> 1` for a ladder (`>> 3` for the thief skill). With the
thrust and drag steps of 4.4 and no gravity (bit 0x80) the speed up is 17.4 for each
second (measured, 20 to 300 frames per second).

**V** In each frame 0x9EBEB(player, 10.0) tests the sides 0, 2, 4, 5 of the segment of
the player, in this order, with the same face and distance tests. A side in reach that
is nearer than 4.0 pushes the player out to 4.0. A side that is no ladder is skipped
when the player has no climb skill or spell (0x9EDE6 to 0x9EE24 jump to the next side,
0x9EF8B). A ladder side clears the fall bit 0x20 and ends the test. When no side
passes: if the feet are more than 3.0 above the floor, `velocity += (uvec + fvec) *
35.0` ("PCONS: push!"), then bit 0x80 is cleared ("PCONS: you fell!").

The port had taken the first side in reach and ended the climb when it was no ladder.
So the side wall of a narrow ladder shaft ended the climb (FLAME, segment 470). This is
corrected in `dtuthief.c` for all settings.

**V** (measured in the port with the rules of the executable, `-origphys`, and with the
`Dtu_fix_*` rules) Why the top of a ladder fails: the test uses the segment of the
center of the player. The center enters the segment above the ladder while the feet are
still a body height (7.5) under the floor at the top. There the side in front is the
opening to that floor, no ladder is in reach, and the climb ends. The push of 35.0 is
taken away by the gravity of 4.3 (24.0 for each step of 1/64 s with no floor in 8.0)
within two steps: the center rises less than 1.0 and the player falls down the ladder.
FLAME: the center reaches 65.2 to 65.6 (feet 57.7 to 58.1, top floor 64.9) at 20, 60,
144 and 300 frames per second, then falls to 47.4. The failure does not depend on the
frame rate.

**G** An attack at the top gets the player up in the original (report of a player).
0x6CFF8 moves the position forward by 1.414 only for the hit test and puts it back on
all paths (verified), so it does not move the player. The idea that a larger box of an
attack frame finds the floor at the top is not true: **V** the port now takes the
sizes of the frames (4.1), and in the attack frames the floor search radius is smaller
(0.8 to 1.35, not 1.8), so the player is lower. Measured with `-origphys` at 60 frames
per second, FLAME segment 432: the center reaches 65.43 and the player falls back to
47.4, with or without an attack at 1.3 to 1.8 s of the climb. The report is not
explained.

## 13. Corrections and Improvements of the movement (port)

The corrections are on and not in a menu. `-origphys` turns them off with the other
`Dtu_fix_*` settings (section 8). The Improvements are the menu settings of
`engine/main/dtuopts.h`. `-noimprovements` turns them off. For tests:
`-improved PJ` sets them, for example `-improved 01`.

`Dtu_fix_jump_path`: the jump is a throw with a constant gravity. Height 7.5 (the top
of the table), air time 15/28 s: `v0 = 4 * 7.5 * 28 / 15 = 56.0`, `g = 8 * 7.5 * (28 /
15)^2 = 209.07`. The position of a frame uses the mean velocity of the frame, so the
path is exact at each frame rate. Forward: 14.0 for each second (the 0.5 of a step, 28
steps for each second, `w` = 0), 1.5 times in a run, 2 times with the Jump spell. The
jump ends when the feet reach a floor, at a ceiling, at a wall on the way down (as
0x9F093), or becomes a fall of the game below its start height. A jump from a ladder
goes toward the wall, as the original (its direction is that of the climb thrust).

`Dtu_fix_ladder`: no velocity into the wall while the player climbs. The player holds
the ladder of the last frame while no ladder of the segment is in reach and the feet
are under its top edge (plane nearer than 10.0, out to 4.0 as 0x9EBEB). When the feet
are at the top edge and the player climbs up: a step onto the floor behind the ladder,
24.0 for each second away from the ladder side, no gravity, until the floor is 1.0 or
less under the feet (0.5 s at most). A player who starts to fall (less than 10.0) with
a ladder in reach in the segment or the segment below, facing it, takes hold of it:
a step back over the edge at the top starts the climb down.

`Dtu_fix_wall_slide`: a wall takes away the velocity into it (factor 1.0, not 1.75) and
keeps the velocity along it. When the velocity is along the side (the body touches an
edge of the side), the player gets 8.0 for each second away from the side, so the
tries of the move loop do not stay at the same place.

`Dtu_fix_wall_overlap`: the move query has another radius for each side (section 3:
the floor `rad/2`, the walls `rad + 1.0`). A segment under the player is entered
through its top side with the small radius, so a wall of that segment can be nearer
than `rad + 1.0` to the center when it first comes into the query. This happens when
the player walks off a ledge: the floor search (only floor sides, section 9) finds no
floor as soon as the center is past the edge, the player falls, and the body is
inside the reach of the wall under the edge. From such a start every move hits the
wall (the hit point is behind the start of the query, also for a move away from the
wall), so the player is held in the air and cannot move. The saved game of a user
("stuck", HUB1): the center at (-67.92, -308.67, 223.83) in segment 179, 0.46 from
the wall of segment 70 side 0 (the side under the edge of the floor of segment 180),
falling, the position the same in each frame (`-movelog`, 120 frames). The
correction: when a wall hit is behind the start of the query and the start is in
front of the side and nearer than its radius (`rad + 1.0`, the ceiling `rad2 * 1.5`),
the center moves along the normal of the side to 1/32 beyond that radius, when the
way there is free and the point is in a segment, and the move starts again from
there (physics.c, `dtu_push_out_of_wall`, the line `WALLOVERLAP` of `-movelog`).
With it the saved game moves the player 4.08 away from the wall, the player falls
to the floor of segment 71 (y -334.98) in 10 frames and walks. A walk over the same
edge with short key presses (YPINN start position replaced by `-testpos
-66.5,-304,223.8,270`, 2 frames forward every 15 frames) gives the stuck player at
30 and 60 frames per second without the correction, and one `WALLOVERLAP` and a fall
to the floor below with it. The same with Improved physics off. Whether the
executable has the same defect was not tested (its floor search uses the radii of
the move, so it holds the player on the edge longer).

`Dtu_improved_physics` (menu):

- On the floor, the velocity in x and z that is not in the direction of the keys goes
  with the time constant 1/64 s (not near a wall that was touched in the last 0.1 s, so
  the slide along a wall stays). With no key it goes with the same time constant and is
  0 after 0.1 s. The speed in the direction of the keys follows the rules of 4.4:
  acceleration and top speed (49.33, 74.0) do not change.
- The turn has no inertia: the turn rate is `rotthrust / mass * (1 - 5 drag) / (5 drag)`,
  the rate at the end of the ramp of the original.
- The body is a capsule (13.1). It replaces the step rule of the earlier version of
  the Improvement (a wall side whose top was 5.5 or less above the feet lifted the
  player onto that top).

`Dtu_improved_jump` (menu): the keys steer the jump (60.0 for each second squared, the
speed in x and z at most the start speed of the jump, 14.0 at least). A jump is possible
0.15 s after the floor is left. A wall does not end the jump (only the velocity into it
goes), a ceiling turns it down, and the body that touches the edge of a higher floor on
the way up does not end it.

`Dtu_turn_scale` (controls.c, a default of the port, not a correction and not in the
menu): the turn keys give 0.75 of the turn time of the executable. The turn rate at the
end of the ramp is 0.94 turns for each second (measured 0.937, 0.937, 0.935 at 20, 60,
144 frames per second), was 1.25. The ramp has the same shape. The mouse in immersive
mode and the joystick are not scaled. `-origphys` gives the rate of the executable
(measured 1.31, 1.29, 1.67 at 20, 60, 144 frames per second: the rest step of 6.2
makes it grow with the frame rate).

`Dtu_fix_turn_ramp` (corrected rule, on; `-origphys` clears it). The ramp of 0x533DC
adds 1/8 of the key time to the turn time of each frame, so the rate is k/8 in frame k
and whole from frame 8 on: a function of frames. The port makes it a function of the
time the key is down, with the shape of the executable at the reference frame time
T = 3/64 s: the rate is k/8 from (k-1)T to kT. The turn time of a frame is the
integral of that function over the frame (controls.c, `dtu_turn_ramp_integral`), and
the part of the angle below one unit is kept for the next frame. With the turn inertia
(`Dtu_improved_physics` off) the rotation steps of 0x6B354 run on a clock of reference
frames (3 steps each, from the key down): the angle grows by the rate of the reference
frame over its time, in the frames that it spans (physics.c, `dtu_physics_sim_rot`).
The mouse, the joystick and the gamepad stick turn with no ramp.

Turn angle in turns after the key is down for 0.1, 0.375 and 1.0 s (YPINN, `-testmove
wait:10,right:1200ms`, the angle interpolated at the time). Reference: the rule of the
executable at the frame time T (computed): 0.0154, 0.1841, 0.7695 with the inertia;
0.0187, 0.1977, 0.7834 without it.

| Frames per second | Before, Improved physics on | After, on | Before, off (inertia) | After, off |
|---|---|---|---|---|
| 20 | 0.0183 0.1969 0.7828 | 0.0186 0.1977 0.7836 | 0.0150 0.1838 0.7686 | 0.0154 0.1845 0.7695 |
| 60 | 0.0182 0.1964 0.7818 | 0.0186 0.1977 0.7836 | 0.0121 0.1729 0.7570 | 0.0154 0.1841 0.7693 |
| 144 | 0.0179 0.1955 0.7800 | 0.0186 0.1976 0.7825 | 0.0106 0.1677 0.7509 | 0.0153 0.1838 0.7686 |
| 300 | 0.0174 0.1938 0.7744 | 0.0185 0.1973 0.7835 | 0.0098 0.1645 0.7441 | 0.0152 0.1833 0.7678 |

The value "0.25 at 20 against 0.18 at 144 frames" of an earlier note came from the
nearest logged frame, not from the angle at the time: at 20 frames per second a frame
is 0.05 s.

### 13.1 The capsule (`Dtu_improved_physics`)

The body of the original (4.1, 4.2) has one radius for the walls and a much smaller one
for the floor: the walls stop the center at `rad + 1.0` (4.59 human, 4.50 dwarf, 4.97
halfling), the floor search has the radius `floor_rad / 2` (1.79). So the body stands on
a floor 3.6 wide and is 9.2 wide for the walls. Two defects come from this:

- A step of 3.5 that is a wall side (BANE, segment 17 side 0 at z 70) stops a dwarf or a
  halfling (center 6.34 above the floor, 2.84 above the step) always, and a human or an
  elf (7.54, 4.04 above it) at some frame rates: the sphere of the center touches the edge
  of the step before the floor search finds the step. The executable has no step test
  for walls (4.6).
- At the edge of a ledge the floor search loses the floor while the center is still in
  the reach of the wall under the edge. The player falls inside that reach and the move
  query holds the player (the stuck save, `Dtu_fix_wall_overlap`).

**V** (fvi.c, `check_sphere_to_face`) The sphere test of fvi is a slab, not round: a
sphere hits a side when its center comes within the radius of the plane of the side and
the point under the center on that plane is within the radius of the side. A sphere whose
center is less than its radius above the top edge of a riser stops at the full radius
from the riser, not at the round distance. So a sphere does not roll up an edge.

The capsule (physics.c, `dtu_capsule_*`): a vertical body with the radius
`R = rad + 1.0`, from the feet (the center minus the stand height, `floor_height +
floor_rad / 2`) to the head (`body_half_height[0]` above the center).

- Walls: the query of the original (the sphere at the center with its radii for the
  walls, the floor and the ceiling, the objects, the list of the segments for the
  triggers) and a sphere at the head, centered `head - R` above the center, for the walls
  only (`Fvi_dtu_body` 3: walls `rad + 1.0`, floor and ceiling 0). The earliest hit counts.
  A hit of the head sphere counts only when the move goes into the side. The head sphere
  is used when its center is 0.25 or more above the center: human 1.16, dwarf 0.05 (not
  used), halfling below the center (not used).
- Floor: a round bottom, a half ellipsoid `R` wide and `stand - R - 0.25` deep (human
  2.65, dwarf 1.59, halfling 1.12). It stands on each solid side that faces up (normal
  into the segment with y above 0.1) and is under the axis (the height of its plane at
  the axis), and on each edge and corner of a solid side within `R` of the axis in x and
  z, of the segments that are open to the segment of the player and in reach. Not only
  side 3: a ramp or the floor of a turned segment can be another side, and the floor
  search of the original (a line, fvi mode 2 tests the other sides with the radius 0)
  finds those too. A first version with side 3 only let the player fall into such a
  floor (FLAME, segment 174). The highest support counts, an edge up to 5.5 above the feet (the
  kneeHigh limit) and a face up to the center. With that depth the sphere of the center
  is always more than `R` above an edge that holds the body, so the slab of the side
  under the edge does not catch it. A round bottom with the depth `R` would put the
  center into that reach again.
- An edge where the floor goes on (a face that faces up is 0.05 past the edge toward the
  axis, at the height of the edge within 0.16) is a fold of the floor, not the edge of a
  step, and does not hold the body: the face under the axis does. Examples: the top of
  a slope after a flat floor, the line between the two halves of a side. Before this
  rule the round bottom rode on the top edge of a slope until the axis was about 4.0
  past it, 2.7 above the face under the axis. On the stairs of BONES (segments 404, 406,
  408, 410 and 412: slopes of 45 degrees, 2.5 long, with a ceiling 20 above that slopes too)
  the sphere of the center then came within the ceiling reach of the original
  (`rad2 * 1.5`, 8.62 for a human) of the ceiling of the slope, and each move hit it:
  the player went down from 403 into 404 and stopped there (z 350.3). On the face under
  the axis the center is 8.81 from that ceiling. The test of the fold is
  `cap_floor_goes_on` (physics.c).
- A walk on the floor down a face that slopes (the face under the axis holds the body,
  no edge, no scenery): the velocity gets the downward part of the slope in the
  direction of the walk (`dtu_capsule_follow_slope`), so the move goes along the face.
  A straight move on the BONES stairs takes the center 0.58 nearer to the sloped ceiling
  for each frame at 60 frames per second and the walk speed, more than the 0.19 of room,
  and the wall rule of the hit takes half of the speed: the walk down took 1.7 times as
  long as the walk up. Now one ceiling hit is left at the top of each slope. Up a slope
  the floor hit lifts the body as before.
- The capsule that stood on the floor in the last frame (no jump, climb, fall, flight,
  levitation or ladder step) is put onto its support at once: up by 5.5 at most (the
  head test point of 4.3 must be in a segment), down by 4.0 at most. So it does not
  bounce down stairs and slopes and does not rise in steps of 2.0. Else the rules of 4.3.
- The first contact with a step: the sphere of the center hits the side under the edge
  before the bottom stands on the edge. On that wall hit, on the floor, the support at
  the end of the query (the same search) is taken: if it is more than 0.1 and at most 5.5
  above the feet, the player is lifted onto it and the move is tried again (the line
  `CAPSTEP` of `-movelog`).
- A player who stands on an edge with no floor under the axis (none, or one more than 1.0
  lower), where the bottom is steeper than 0.25, and who does not walk toward the edge,
  slides off it with 8.0 for each second. So the player does not stand in the air beside
  a ledge.
- No sphere at the feet for the walls: in the slab test it would stop at each riser
  within `R` of its center (each step), and it would lower the reach of a jump onto a
  ledge by `stand - R` (2.95 for a human) against the original.
- Objects (creatures, scenery): the move query of the player takes the height of the
  capsule (feet to head, not 6.25 up and down from the center) in the test of fvi.c. The
  radius stays `rad`: with `R` the player stops 1.0 to 1.2 farther from a creature, and
  the fight test of combat.md C5 lost its hits at 60 frames per second (0 contacts, 2 with
  `rad`). With the capsule height a human collides with scenery whose top is up to 1.29
  above the feet (a dwarf already did in the original: its center is 6.34 above the feet),
  and a dwarf passes under objects 1.7 lower than before.
- The creatures, the weapons and the reach of the creatures (0x746B3) do not change.

With Improved physics off the body of the original is used, with no change. Section
13.2 replaces the item "Objects" above for scenery.

Measured (port, `-fixedfps`, the same runs give the same moves; "before" is the body of
the original with the step rule, "after" the capsule; Improved physics on unless noted):

| Test | 20 | 60 | 200 |
|---|---|---|---|
| BANE walk 2.7 s (run of 4 s), end z, human, before / after | 143.1 / 146.6 | 141.5 / 144.3 | 142.5 / 143.5 |
| the same, dwarf (`-archive dmf`) | 143.1 / 146.6 | 142.1 / 144.3 | 20.5 / 143.5 |
| the same, halfling (`-archive fmf`) | 143.1 / 146.6 | 141.5 / 144.3 | 140.8 / 143.5 |
| the same, Improved physics off, human / dwarf / halfling, before = after | 64.9 / 65.4 / 64.9 | 134.4 / 65.5 / 65.0 | 143.2 / 65.5 / 65.0 |
| FLAME ladder (segment 432, Activate, 4 s forward): end y, highest y, before / after | 72.46, 72.93 / 72.40, 73.04 | 72.39, 72.62 / 72.40, 72.67 | 72.43, 72.49 / 72.39, 72.78 |

- BANE: the capsule climbs the step of 3.5 with one or two `CAPSTEP` lifts (the first
  contact) and then on its round bottom, for each race and frame rate, and walks down
  the ledge of 3.5 with the floor snap (no fall). The dwarf at 200 frames per second
  "before" stayed in the floor at the start (center 2.9 too low, z 20.5).
- FLAME: the climb, the step onto the floor at the top (64.9, center 72.4) and the walk
  on (end z 244.6 to 245.1) as before.
- Ledge walk-off (HUB1, `-testpos -66.5,-304,223.8,270`, 2 frames forward every 15):
  before, one `WALLOVERLAP` at 20 and 60 frames per second and a fall to the floor of
  segment 71 (y -334.99). After, no `WALLOVERLAP`: the player sinks with the bottom
  over the edge, slides off and falls to the same floor. At 200 frames per second the
  presses are too short to reach the edge (both).
- The stuck save (HUB1, segment 179, a copy of the user folder, loaded with the front
  end script, 30 and 60 frames per second): before, `WALLOVERLAP` moves the player 4.08
  from the wall and the player falls to y -334.98. After, the round bottom stands on the
  edge of the floor of segment 180 at once (axis 0.46 past the edge, center y -305.36),
  no `WALLOVERLAP`. From there: 1.5 s forward walks along the edge to segment 174 (y
  -309.90); 1.5 s back falls to the floor of segment 71 (y -335.01); a turn and 1.5 s
  forward falls and walks on in segment 68.
- The same before and after: 4 s with no input in YPINN, BANE, DROW1 (center 7.5 above
  the floor, no change after the first second), the YPINN and DROW1 walks (same end
  positions), the PRISON jumps (end z 375.46, 378.13, 378.42; end y within 0.06), the
  push of scenery of section 10 (chair, table, player end z 54.4, 62.5, 46.6 at 30; 56.1,
  63.6, 46.9 at 60; 59.3, 65.2, 50.0 at 144), the fight test of combat.md C5 (no contact
  at 20, 2 contacts and the kill at 3.43 s at 60, 2 contacts and the kill at 2.19 s at
  200 frames per second).
- With Improved physics off all 38 runs of these tests give the same numbers before and
  after.
- Levitation and the Fly spell (BANE, `-chartest 20:fx=21.60` and `fx=11.60`, up, forward,
  down): the same path before and after (in flight the end height differs by 1.6 over
  the edge of a step, no snap in flight).
- Walks from the center of 210 random segments toward a random side (YPINN, BANE, HUB1,
  DROW1, FLAME, PRISON, BONES, 30 per level, 1.5 s walk or 1.2 s run, 30 frames per
  second), before and after: 196 the same within 3.0. In the 14 others the capsule goes
  as far or farther (up to 33 more) or ends on a higher floor: up steps and ramps where
  the body of the original stopped (FLAME segment 173 and 148, BANE 74), out of a hold
  in the air with `WALLOVERLAP` (FLAME 45), out of a corner (DROW1 287). In none did the
  capsule stop where the original passed (doors and openings: the walls are tested
  with the same sphere and radius).
- The fold rule and the walk down a slope (the two items above, 2026-11-24):
  - BONES, from the center of 403 toward 413 (`-testpos`, 1 s wait, then forward):
    before, the player stopped in 404 at z 350.3 at 20, 30, 60 and 144 frames per
    second, walk and run (with `-origphys` it went through). After, it reaches 413 and
    walks on to 440; from 404 to 413 in 90 frames at 60 frames per second (before the
    slope rule 128, the walk up 80). Up from 413 to 403: the same as before.
  - DROW1 486 to 481 and back, THIEF1 383 to 381 and back, walk and run, 20 to 144
    frames per second: all reach the other end, as before.
  - Walks over the ramps of all levels (segments whose floor is not flat, from the
    segment before to the segment after and back, 5 for each level, 2.5 s, 60 frames
    per second walk and 30 run), before and after: no walk stops where it passed
    before. Now passing: BONES 404, DROW2 124 and 249, FLAME 644 (642 to 643). The
    other differences are paths that differ by a few units (creatures that move, BAT).
  - The tests of section 10 and 13.1 (BANE step for the three sizes, YPINN and DROW1
    walks, YPINN idle, PRISON jump, 20, 60 and 200 frames per second): the same
    numbers. HUB1 ledge walk-off: the same path, the end height is 0.04 to 0.1 lower
    (within the 0.1 of `DTU_FLOOR_EPS`; the floor of segment 71 slopes a little).
- Soak test (`-soak 3` and `-soak 4`, 2700 frames at 30 frames per second, the seven
  levels above, `-hidden`): no `SOAK FAIL` (the player and the objects stay inside the
  mine and in their segments) before or after. YPINN with the seed 3 stops at frame 302
  in both builds (a hang of the soak test, not of the movement). The frames in which the
  player has a velocity but does not move are holds of the game (a cast, scenery that
  stops the player, the jumps of the test to a point in the air) that come in both
  builds.

### 13.2 Scenery as solid bodies (`Dtu_improved_physics`)

New code, not in the executable: `engine/main/dtusolid.c`, the move of the player in
`physics.c`, the push, the fall and the sound in `dtuobj_scenery.c`.

The problem (saved game of a user, level THIEF0, segment 85): the way to an opening
goes from a bench (obj 4) to a barrel (obj 3), a crate (obj 2), and a chest (obj 5)
that lies on a second crate (obj 1). With the rules of the port before this change the
player who jumped at the bench came down on its cylinder, each try of each frame hit
it and pushed it (8 push lines in each frame, 1211 in 2.4 s), the player stayed in the
air 3.3 above the floor with a velocity down that grew to 198, and no jump was
possible (the floor search does not see objects). The intent of the executable is a
stand on scenery (section 4.5), but its rules do not give a stable one.

The rules with Improved physics on:

- Shape. Upright scenery (the up vector has y of 0.9 or more) is the box of the
  drawn points of the rest pose of its model, turned with the object
  (`model_points_box`, relative to the position: the model is drawn with
  `body_center` at the position of the object, models.md). Not `box1_min[0]` to
  `box1_max[0]`: that box of `chair.pof` is 2.95 above the drawn chair
  (formats/egg.md), so a pushed chair fell until that box stood on the floor and
  stood 2.95 deep in it, with the legs out of sight. Other scenery is the cylinder of
  `dtu_get_body_cylinder()`.
  Scenery with `pass_thru` is no body.
- The body of the player for scenery is an upright cylinder: the radius of the move
  query (`rad`, 3.59 for a human), from the feet to the head of the capsule (13.1).
- Top. The top of scenery under the axis, or within `rad` of it, is a support of the
  capsule as a floor is, when it is not more than 1.5 (`DTU_SOLID_STEP`) above the
  feet. So the floor distance, the gravity, the jump key and the end of a jump see it.
  An axis that is more than 1.0 outside of the top is on an edge: the slide of 13.1
  takes the player off it.
- Side. The move of the player is tested against the box grown by `rad` (round
  corners). A side stops the velocity into it and keeps the velocity along it. The
  player moves up to the side (1/32 before it), and the rest of the frame goes on.
  A top that is 1.5 or less above the feet is no side: the player steps onto it.
- A move down onto a top ends a jump or a fall as a floor does (fall damage as 0x6CD45).
  A move up against a bottom stops the velocity up.
- The move query of fvi does not test scenery for the player in this mode. All
  scenery near the player is tested, not only that of the segments of the move.
- A body that starts inside of scenery moves out of it with 16.0 for each second.
- Push. The player who stands on a floor and holds the keys against a side (the key
  direction has 0.5 or more into the side, the side is 0.125 or less ahead) for 0.3 s
  moves the scenery, when the strength is above `pushable` (the test of 0x9E5BE). The
  speed grows in 0.25 s to `24.0 * (1 - 0.6 * pushable / strength)`: 24.0 is one half
  of the walk speed limit, the mean speed of the push cycle of the executable at its
  reference frame rate (objects.md). The scenery moves directly by the speed times the
  frame time, as far as the walls and the other bodies let it (the tests of
  `Dtu_fix_push_body` and `Dtu_fix_push_wall`), and the player gets its speed. No
  velocity stays in the scenery: it stops when the player stops. There is no push in a
  jump, in a fall, or of the scenery that holds the player. Other scenery in the way
  moves too when the strength is above its `pushable`. Scenery that lies on the pushed
  scenery moves with it.
- Fall. Scenery that was pushed, or that lay on scenery that moved or broke, falls
  with the gravity of the jump (209.07) when its support (the floor under its
  position, or the top of other scenery under it) is 0.5 or more below it. At the
  start of a level and after a load no scenery falls (a level can have scenery on a
  wall), but scenery that is less than 3.0 deep in the top of other scenery is put
  onto that top: the gravity of the port before this change let a chest sink into its
  crate at each push (the saved game has the chest 1.44 in the crate).
- A creature pushes scenery with the rules of objects.md, as before.

The sound of scenery that moves (both settings): the executable links the sound 2706
to the object one time while its speed is above 0x70 and ends it below that (0xA0323
to 0xA038A). The port played it at the place of the object each 8 frames while the
scenery had a velocity: more copies at the same time at a high frame rate (18 for each
second at 144 frames), also for scenery that a wall held in its place. Now one sound
is linked to the object, as a loop, from 0.1 s after the scenery started to move until
0.125 s after it moved the last time. **G** The loop: the executable passes 0 for the
loop flag, and the length of its sound was not checked. The sound was not heard in
a test (the tests run with `-mute`).

Measured (`-fixedfps`, THIEF0, the character of the saved game, creatures removed with
`-testkill 1`, `-noattack`):

| Test | 20 | 60 | 144 |
|---|---|---|---|
| Bench to barrel: on the bench (`onobj=4`), face the barrel, walk 0.1 s, jump, walk 0.7 s. End height of the center (barrel top + stand = 12.05) | 12.05 | 12.05 | 12.05 |
| Barrel to crate (14.12) | 14.12 | 14.12 | 14.12 |
| Crate to chest on the next crate (21.12) | 21.12 | 21.12 | 21.15 |
| Push lines in these three tests | 0 | 0 | 0 |
| Walk of 1.0 s into the side of the crate obj 2 from 9.9 away: the crate moves (a wall stops it after 2.8) | 2.76 | 2.77 | 2.81 |
| The same, walk of 0.8 s, with a push delay of 0.15 s (240 frames per second: 2.46) | 2.42 | 2.42 | 2.45 |

- The saved game, 60 frames per second: a jump at the bench lands on it (0 push
  lines; before 1211), and the chest is put back onto its crate at the load.
- Three jumps against the side of the crate from the floor with 0.5 s of walk
  between them: the crate moves 0.84 (the player leans on it between the jumps).
- With `-noimprovements` and with `-origphys -origobj -noimprovements` the move log
  of a walk, a jump and a walk at the crate is the same file before and after the
  change (300 frames).
- A first version took the push speed from the velocity of the player at the hit and
  the lean time from the hits of the move. At 144 and 240 frames per second most
  frames have no hit (the velocity into the side is 0 after a hit and grows only with
  a thrust step of 1/64 s), so the push started late: 0.03 and 0.00 against 2.04 at 60
  frames per second for a walk of 0.8 s.

Not done: with Improved physics off the rules of 0x261DB and 0x13A950 (section 4.5)
are not in the port. A player who comes down on a creature still hangs over it (the
move stops at a creature). The player does not move with scenery that a creature
pushes under the player.

## 14. The fixed step (`Dtu_fixed_step`, port)

New code, not in the executable: `engine/main/dtustep.c` and `GameLoop()` in `game.c`.

The executable does one step of the game for each picture, with the time of the
picture as `FrameTime` (section 8). The port has a correction for each known rule that
counts frames, but a step of 1/144 s and a step of 1/20 s never give the same result,
and each new rule must be made independent of the frame rate again (the first version
of the push of 13.2 was not).

With `Dtu_fixed_step` (on by default) the game runs in steps of 1/60 s:

- `GameLoop()` has three parts: `game_frame_start()` (the values of the player),
  `game_frame_render()` (the picture) and `game_frame_sim()` (the controls, the game
  time, the scripts, the walls and triggers, the objects, the AI). Without the fixed
  step their order is that of the executable: start, picture, `calc_frame_time()`, sim.
- With the fixed step the time of each picture goes to an account. For each 1/60 s in
  the account the game does `game_frame_start()` and `game_frame_sim()` with `FrameTime`
  1/60 s: no step in a picture at a high frame rate, more than one at a low frame
  rate, 8 at most (more time is dropped, for example after a long load). `FrameCount`
  counts the steps, not the pictures.
- The picture shows the state between the last two steps. The part of a step that
  stays in the account is `Dtu_step_alpha` (0 to 1). Before a step the positions and
  orientations of all objects, the look pitch and the phase of the view motion are
  stored. For the picture each object goes to `stored + (now - stored) * alpha` (the
  orientation from the forward and up vectors), and after the picture the state of
  the game comes back. An object that moved more than 25.0 in one step (a warp, a
  load) is not put between. So the picture is up to one step (1/60 s) behind the
  game.
- The code of the picture has the time of the picture as `FrameTime` (the texts of
  the HUD count with it).
- The keys and the mouse are read in the steps. Key events stay in their queue until a
  step reads them.
- The map of the map screen is drawn with the picture: its control values add up over
  the steps until the next map frame takes them (`dtu_automap_read_controls`,
  `docs/re/automap.md`).
- A new level, the end of the game or a load ends the steps of that picture.

Not with the fixed step: the test option `-fixedfps N` (one step of 1/N s for each
picture, as before: the tests of the frame rate and all measurements of this document
use it), a demo, a network game. `-nofixedstep` and `-origphys` turn it off. The new
test option `-renderfps N` gives each picture 1/N s with the fixed step.

The `Dtu_fix_*` corrections of section 8 stay: they are needed without the fixed step,
and at 1/60 s they give what `-fixedfps 60` gave before.

Measured:

- THIEF0, a walk, a jump and a walk at a crate (the test of 13.2), 5 s: the move log
  is the same file with `-fixedfps 60` and with `-renderfps` 20, 60, 144 and 240 (301
  steps each). With `-renderfps 144 -nofixedstep` it has 719 steps and another end
  height of the center (4.08 against 4.15).
- With `-fixedfps 60` the move logs are the same files as before this change, with
  the Improvements on and off.
- The picture (line `VIEW` of `-movelog`: the place of the player in each picture),
  YPINN, a walk with a turn, 0.5 s of constant motion: the turn between two pictures
  has a standard deviation of 0.1 to 0.2 percent of its mean at 45, 144 and 240
  pictures per second (0.07 percent at 60, where each picture is one step).

Not done: the view turns with the steps, so the mouse look has up to 1/60 s more
delay than before at a high frame rate. The animation frames of the models, the
doors, the vclips and the lights change in steps of 1/60 s. The menus and the front
end are not part of the game loop and do not change.
