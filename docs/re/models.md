# Polygon models and animation in DTUGAME.EXE

Status: read from the code of DTUGAME.EXE 1.3 and checked against the data of all
archives of the disc with `tools/model.py`. Addresses are linear addresses of the LE
image with object 1 at 0x10000 (`gamedata/re/dtu_flat.bin`). `tools/dtudis.py` is the
disassembly helper that was used.

Each statement has one of these marks:

- (code): read in the disassembly at the given address.
- (data): checked with a tool against the game files.
- (guess): not proven.

Related documents: [../formats/egg.md](../formats/egg.md) (file layout),
[ai.md](ai.md) (who sets the animation state), [objects.md](objects.md).

## Short answers

- Animation is joint angles, as for the robots of Descent 1. A frame holds one angle
  triple (pitch, bank, heading, 16 bit each) for each submodel. There is no vertex data
  per frame. (code 0x72A3A, data: pictures from `tools/model.py`)
- All animations of a model are in one frame list. An animation state is a fixed
  range of frame numbers. The ranges are the same for every model (table at 0x13D9E0).
  (code 0x1114C, 0x44553)
- The frame of an active creature and of the player is computed from the game time,
  not counted per game frame. The idle animation of creatures is counted per game
  frame. See "Timing". (code)
- The interpreter data has no SUBCALL opcode. The executable walks the submodel tree
  itself, sorts the submodels by depth and calls the Descent 1 interpreter once for
  each submodel. (code 0x66E13, data: 0 SUBCALL opcodes in 2187 models)
- A weapon in a hand is not an attached object. It is part of the model. The player
  has one model for each weapon class and the game changes the model of the player
  object. (code 0x43EB2)

## The model struct in memory

`Polygon_models[]` is an array of pointers at 0x3880D0, 160 entries, one per model
number. `N_polygon_models` is at 0x13D4F8. The struct has 332 (0x14C) bytes. The model
names are at 0x3878B0, 13 bytes each (code 0x1EF1E).

"File" is the offset in the 218 byte model header of the EGG file, or the number of
the array in the model data block (egg.md, "Model data block"). All pointers are null
until the model is paged in (0x6E96D, called when bit 0 of 0xB5 is set).

| Mem | Size | File | Name | Meaning |
|---|---|---|---|---|
| 0x00 | 4 | 0x00 | `n_models` | number of submodels (code 0x70EC1) |
| 0x04 | 4 | 0x04 | `model_data_size` | |
| 0x08 | 4 | array 1 | `model_data` | pointer, interpreter data |
| 0x0C | 4 | array 2 | `submodel_ptrs` | pointer to n ints. The page-in code adds the address of `model_data` to each (0x6EC67), so they are pointers in memory and offsets in the file |
| 0x10 | 4 | array 3 | `submodel_offsets` | pointer to n vec. Offset of the joint in the space of the parent |
| 0x14 | 4 | array 4 | `submodel_norms` | pointer to n vec |
| 0x18 | 4 | array 5 | `submodel_pnts` | pointer to n vec |
| 0x1C | 4 | array 6 | `submodel_rads` | pointer to n fix |
| 0x20 | 4 | array 7 | `submodel_parents` | pointer to n bytes, 0xFF for the root |
| 0x24 | 4 | array 8 | `submodel_mins` | pointer to n vec |
| 0x28 | 4 | array 9 | `submodel_maxs` | pointer to n vec |
| 0x2C | 12 | 0x08 | `mins` | |
| 0x38 | 12 | 0x14 | `maxs` | |
| 0x44 | 4 | 0x20 | `rad` | |
| 0x48 | 4 | 0x24 | `n_textures` | |
| 0x4C | 4 | 0x28 | `textures` | pointer into `ObjBitmapPtrs` (0x18D704), see "Textures" |
| 0x50 | 4 | 0x2C | `simpler_model` | model number, -1 for none. Set by `simple_model=` of a table line. No table of the disc has the keyword (data: -1 in all models) |
| 0x54 | 4 | 0x30 | `mirror_model` | model number, -1. Set by `mirror_model=` of a `$PLAYER` line (0x204F2). Not in the tables |
| 0x58 | 4 | 0x34 | `n_frames` | |
| 0x5C | 4 | array 12 | `anim_angles` | pointer to n_frames * n_models angle triples (6 bytes each), frame after frame. Null if n_frames is 0 |
| 0x60 | 4 | array 13.1 | `frame_eye` | pointer to n_frames vec. The y value is added to the eye height of the viewer (0x76EC8). (data: 0 when standing, -5.3 on the last frame of DIE for `durnan.pof`) |
| 0x64 | 4 | array 10 | `box1_min` | pointer to n_frames + 1 vec. Bounding box of the whole model. Entry 0: rest pose or all frames. Entry f + 1: frame f |
| 0x68 | 4 | array 11 | `box1_max` | same |
| 0x6C | 4 | array 13.2 | `box2_min` | pointer to n_frames vec. Box of the weapon of attack 1. Valid if `box_flags` & 2 |
| 0x70 | 4 | array 13.3 | `box2_max` | |
| 0x74 | 4 | array 13.4 | `box4_min` | box of the second weapon. Valid if `box_flags` & 4 |
| 0x78 | 4 | array 13.5 | `box4_max` | |
| 0x7C | 4 | array 13.6 | `box8_min` | box of the torso. Valid if `box_flags` & 8 |
| 0x80 | 4 | array 13.7 | `box8_max` | |
| 0x84 | 4 | array 14.1 | `body_center` | pointer to n_frames + 1 vec, only if `box_flags` & 1. (box1_min + box1_max) / 2 |
| 0x88 | 4 | array 15.1 | `torso_center` | pointer to n_frames vec, only if & 8. Center of box 8 minus `body_center` of the frame |
| 0x8C | 4 | array 16.1 | `weapon_center` | pointer to n_frames vec, only if & 2. Center of box 2 minus `body_center` |
| 0x90 | 4 | array 17.1 | `weapon2_center` | pointer to n_frames vec, only if & 4 |
| 0x94 | 4 | array 14.2 | `body_half_height` | n_frames + 1 fix. Half of the y size of box 1 |
| 0x98 | 4 | array 14.3 | `body_radius` | n_frames + 1 fix. Half of the diagonal of box 1 in the xz plane |
| 0x9C | 4 | array 15.2 | `torso_half_height` | n_frames fix |
| 0xA0 | 4 | array 15.3 | `torso_radius` | n_frames fix, xz plane |
| 0xA4 | 4 | array 16.2 | `weapon_radius` | n_frames fix. Half of the diagonal of box 2 in space |
| 0xA8 | 4 | array 17.2 | `weapon2_radius` | n_frames fix |
| 0xAC | 4 | array 19 | `submodel_flags` | pointer to n bytes, see "Submodel flags" |
| 0xB0 | 4 | array 18 | `submodel_children` | pointer to bytes. Row s holds the child numbers of submodel s, ended by 0xFF. The row width is n_models (code 0x67020) |
| 0xB4 | 1 | 0x3C | `box_flags` | 1 whole body, 2 weapon, 4 second weapon, 8 torso (`HAVETORSO`, code 0x68059) |
| 0xB5 | 1 | | paged out flag | 1 after the header is read, 0 after page-in |
| 0xB6 | 2 | 0x4A | `gun_submodels[2]` | u8 each |
| 0xB8 | 8 | 0x66 | `fire_frames[4]` | s16 each: frame of the hit or shot in the states ATTACK, ATTACK2, 11 and 12 |
| 0xC0 | 36 | 0x6E | `speeds[9]` | fix. The EGG loader copies 8 values (0x70F57). Entry 8 comes from the table line only |
| 0xE4 | 76 | 0x8E | `state_times[19]` | fix, seconds. Index is the animation state |
| 0x130 | 4 | 0x4C | `n_guns` | s16 in the file |
| 0x134 | 24 | 0x4E | `gun_points[2]` | vec each |

The formulas of the derived arrays (0x84 to 0xA8) are (data): they give the stored
values for `durnan.pof` to 0.01 (frames 0, 4, 8, 12, 56, 90, 95). The tool that built
the archive computed them (the code is also in the executable, 0x72E5E to 0x73954, it
runs when a model comes from a POF file). The port can read them from the file.

Checked for the multi-submodel models (data, `model.py check`, 1142 models): every row
of `submodel_children` lists exactly the submodels whose `submodel_parents` entry is the
row number. In models with one submodel the row is not initialized (1045 models).

### Gun points

`n_guns`, `gun_points[2]` and `gun_submodels[2]` are in the header (code 0x70F8E to
0x70FC3). `MAX_GUNS` is 2. There are no gun directions. Most models have `n_guns` 0 and
garbage in the other fields. Three models of `PORTAL.EGG` have one gun: `dlf_ffk.pof`
(submodel 18), `elf_p.pof` (15), `min_b.pof` (6) (data).

A creature that shoots does not need them: the animation step sets the fire point to
the position plus the forward vector (ai.md, "Animation step", step 9). The use of
`gun_points` by `calc_gun_point` (function at 0x77845, strings of robot.c) was not read.

### Submodel flags

One byte per submodel (array 19).

| Bit | Meaning |
|---|---|
| 0x01 | (code 0x66F82) the caller can add a pitch angle to this joint. The value that `draw_polygon_model` passes is always 0, so this does nothing in 1.3. (data) set on the feet of creatures |
| 0x02, 0x04, 0x08 | not read by the draw code. (data) 0x02 is on pelvis, torso, neck and head. 0x04 is on a submodel without polygons at the end of the weapon hand. (guess) marks of the model tool for the boxes |
| 0x10 | (code 0x66EF8) hidden. The submodel is drawn only if the caller asks for it. See "The missile in the hand" |
| 0x20 | (code 0x66EDE) the submodel is the part that gets the magic effect when the object has one |

Counts over all archives (data): 0x00 13367, 0x01 423, 0x02 1685, 0x04 800, 0x08 361,
0x0C 254, 0x10 388. No file has 0x20.

## Animation states and frames

The state names are the debug strings at 0x120004 (8 bytes each). The first frame of a
state is in the table of 16 bit values at 0x13D9E0. State s uses the frames `first[s]`
to `first[s + 1] - 1` (code 0x1114C to 0x11160).

| State | Name | First frame | Frames | Default time (0x13DA08) |
|---|---|---|---|---|
| 0 | REST | 0 | 0 | 0 |
| 1 | STAND | 0 | 4 | 0.5 |
| 2 | WALK | 4 | 16 | 2.0 |
| 3 | SIDLE_R | 20 | 4 | 0.5 |
| 4 | SIDLE_L | 24 | 4 | 0.5 |
| 5 | RUN | 28 | 16 | 1.0 |
| 6 | ATTACK | 44 | 16 | 1.0 |
| 7 | ATTACK2 | 60 | 16 | 1.0 |
| 8 | FLINCH | 76 | 8 | 0.5 |
| 9 | DIE | 84 | 12 | 0.5 |
| 10 | FIDGET | 96 | 12 | 0.75 |
| 11 | no name | 108 | 16 | 1.0 |
| 12 | no name | 124 | 16 | 1.0 |
| 13 | no name | 140 | 12 | 1.5 |
| 14 | no name | 152 | 12 | 0.75 |
| 15 | no name | 164 | 4 | 1.0 |
| 16 | no name | 168 | 8 | 0.5 |
| 17 | no name | 176 | 4 | 0.5 |
| 18 | no name | 180 | 12 | 2.0 |
| end | | 192 | | |

- A creature model needs 108 frames (states 0 to 10). A player model needs 192. Frame
  counts in the archives (data): 108 (808 models), 192 (796), 109 (106), 181 (16), 180
  (6), 126 (1), and 1, 4 or 5 for scenery (83). The `$PLAYER` reader stops with
  "Player model %s does not have enough animation frames" if `n_frames` is below
  `first[10] - 1` (code 0x205B3).
- States 11 and 12 are attack states of the player. They have the fire frames 3 and 4
  and the game treats them like 6 and 7 (code 0x44640, 0x678F2). (guess) attacks with
  the other hand or spell casting.
- States 13 to 18 have a movement speed (below). Their meaning is not known.
- `state_times=` of a table line gives the times of the states 1, 2, 3 and so on. A
  `$MONSTER` line has 10 numbers, a `$PLAYER` line 18.
- `fire_frames=` gives absolute frame numbers. Defaults: 56, 72, 116, 132 (code
  0x1F502). `HMC.BIN` has 45, 61, 109, 125.

### Speed of a state

Function 0x74AA7 (object, state or -1 for the state of the object) returns the
movement speed of an animation state (code):

| State | Value |
|---|---|
| 2, 3, 4 | `speeds[0]` |
| 5 | `speeds[2]` |
| 13, 14, 15, 16, 17, 18 | `speeds[3]` to `speeds[8]` |
| other | 0 |

The result is doubled if bit 0x20 of object byte 0x89 is set. It is halved if bit 0x08
of byte 0x8B is set and bit 0x10 of 0x89 is not (spell effects, see objects.md).
`speeds[1]` is not read by this function.

## The object fields

| Object | Type | Meaning |
|---|---|---|
| 0x162 | int | model number |
| 0x166 | int | `subobj_flags` (Descent 1) |
| 0x16A | int | `tmap_override`, -1 for none |
| 0x16E | s16 | not identified. Decremented when a dead creature is kept as a corpse (0x696E9) |
| 0x170 | s16 | animation state |
| 0x172 | int | animation frame, absolute frame number |
| 0x176 | int | goal state: the state that follows when the current one ends |

The level file stores 0x16E to 0x176 in the polyobj part of an object record (lvl.md).
(data) They are 0, 1, 0, 1 for creatures and players (STAND, frame 0, goal STAND) and
0, 0, -1, -1 for scenery. Frame -1 means "no joint angles".

## Drawing a model

### draw_polygon_model (0x727A5)

Register arguments: eax = position, edx = orientation matrix, ebx = model number, ecx =
`subobj_flags`. Stack arguments in order:

| # | Meaning |
|---|---|
| 1 | light (fix) |
| 2 | pointer to the glow values |
| 3 | 1 in all callers |
| 4 | segment number. Not used by the function |
| 5 | frame, -1 for none |
| 6 | show the hidden submodels (flag 0x10) |
| 7 | 1 if the object is the viewer |
| 8 | effect mode: 0 normal. Not 0: submodels with flag 0x20 get the magic effect. 3: draw only those |

Steps (code):

1. Return if the switch at 0x13A89F is 0. Print "Trying to draw bogus poly" and return
   if the model number is out of range.
2. Pulse counter: the counter at 0x13D508 goes down by one for each call. Every sixth
   call adds the step at 0x13D504 (+1 or -1) to the value at 0x13D500, which moves
   between 25 and 30. The byte at 0x14C4E1 gets the value. So this pulse depends on the
   number of models drawn, not on time.
3. `po = Polygon_models[model_num]`. If `po->simpler_model` is not -1 and
   `subobj_flags` is 0: if argument 7 is 0, use the simpler model. Else use it while the
   depth of the position is above `Simple_model_threshhold_scale` (0x13D50C) * step *
   `rad`, as in Descent 1. No model of the disc has a simpler model.
4. Page the model in if needed.
5. If the frame is above -1 and `po->anim_angles` is null: frame = -1.
6. Texture list: for i below `n_textures`, `texture_list[i]` (0x37EB48) =
   `*po->textures[i]`. Page in each bitmap that has the flag 0x10.
7. If `box_flags` & 1: `position -= transpose(orient) * body_center[frame + 1]` (0x729C1
   to 0x72A0A: copy transposed matrix 0xD778A, rotate 0xD76E7, subtract 0xD6EF1). So the
   position of an object is the center of the body box of its current frame, and the
   origin of the model moves against it from frame to frame.
8. `g3_start_instance_matrix(position, orient)` (0xEA833). `g3_set_interp_points`
   (0xF07A4, buffer at 0x37EC10).
9. `angles = frame == -1 ? NULL : po->anim_angles + frame * n_models * 6`.
10. If `subobj_flags` is 0:
    - `n_models` > 1: call the sorted draw (0x66E13, below).
    - else: `g3_draw_polygon_model(po->model_data, texture_list, angles, light, glow)`
      (0xF07AA: esi = data, edi = textures, eax = angles, edx = light, ebx = glow).
    - If the switch at 0x13B683 is set, call 0x73ACD (guess: draws the boxes).
11. Else, for each bit i of `subobj_flags` (Descent 1 code for debris): offset = -(
    `submodel_mins[i]` + `submodel_maxs[i]`) / 2, `g3_start_instance_matrix(offset,
    NULL)`, draw `submodel_ptrs[i]`, `g3_done_instance`.
12. `g3_done_instance` (0xEA8D7).

### The sorted draw (0x66E13, newmodel.c)

Arguments: eax = po, edx = texture list, ebx = angles, ecx = light, stack: glow pointer,
argument 3, 0, argument 6, argument 8 of `draw_polygon_model`. It asserts
`po->n_models > 1`.

Pass 1, recursive function 0x66F68 (po, submodel s, angles), starts with submodel 0:

1. `g3_start_instance_angles(&submodel_offsets[s], angles ? &angles[s] : NULL)`
   (0xEA820). With null angles the joint is not turned.
2. Save the current instance state (0xEA919) in `saved[s]` (0x2ADF50, 48 bytes each).
3. Rotate the zero vector into view space (0xD8B54) and store the result in `origin[s]`
   (0x2AEC50, 12 bytes each). This is the joint position as the camera sees it.
4. For each child c of row s of `submodel_children` (until 0xFF): recurse with c.
5. `g3_done_instance`.

Pass 2:

1. Fill `order[]` (0x2AEB50) with 0 to n - 1 and sort it with `qsort` (0xD8566) by
   `origin[].z`, largest z first (compare function 0x66DE6). Far submodels are drawn
   first.
2. Save the instance state. For each submodel s of `order[]`:
   - Restore `saved[s]` (0xEA93A).
   - If the effect mode is not 0 and `submodel_flags[s]` & 0x20: set the bytes at
     0x14C4CD to 0x40 and 0x14C4D0 to 15 for this submodel (the magic effect of the
     texture mapper), and restore them after it.
   - Skip the submodel if `submodel_flags[s]` & 0x10 and argument 6 is 0.
   - Skip it if the effect mode is 3 and the byte at 0x14C4CD is not 0x40.
   - `g3_draw_polygon_model(submodel_ptrs[s], texture_list, angles, light, glow)`.
3. Restore the instance state.

The transform of a submodel is that of Descent 1: a point p of submodel s is
`offset[s] + M(angles[s]) * p` in the space of the parent, where M is
`vm_angles_2_matrix` (angle order pitch, bank, heading). (data) `tools/model.py` uses
this rule and gives correct poses, see "Proof".

The interpreter data of a submodel is a Descent 1 opcode stream that starts at
`submodel_ptrs[s]`. It holds DEFPOINTS or DEFP_START, SORTNORM, FLATPOLY, TMAPPOLY and
ends with EOF. (data) All submodel streams of all 2187 models parse with the Descent 1
record sizes and none holds a SUBCALL.

### The callers in object.c

`draw_polygon_object` is the function that holds 0x676ED to 0x67A1C. It handles these
cases (code):

1. `tmap_override` is not -1: all textures of the model are replaced by that bitmap for
   the call (0x67682), as in Descent 1.
2. The object is a player with bit 1 or 2 of byte 0x8A set, or a creature with byte
   0x147 not 0: call 0x673AD. It sets a special texture mapper function (0xD8D88 with
   0xF1578), draws the model, clears it and sets the value at 0x14C560 to 0x22. If
   0x670D4(object) is 2 it draws the model again with effect mode 3. (guess) the
   invisible or shadow look.
3. The object is a weapon (type 5) with flag bit 4 of byte 0x0D and its weapon type has a
   model at `Weapon_info` + 0x1A: draw that model with frame -1.
4. Normal case: compute "show hidden" (below) and call `draw_polygon_model` with frame =
   object 0x172, argument 7 = (object is the viewer and the viewer is not a weapon),
   argument 8 = (0x670D4(object) == 2). If 0x670D4 returns 1 the whole model gets the
   magic effect.

0x670D4 returns the magic look of an object: 0 if the viewer does not have bit 0x40 of
byte 0x88 (guess: detect magic), else 1 for a magic item (flag bits in the item tables)
and 2 for a creature whose weapon is magic.

### The missile in the hand

"Show hidden" is argument 6. The code at 0x677EB to 0x67982 sets it to 1 in these cases:

1. `Robot_info[obj->id].weapon_type` (s16 at entry + 0x6A) is 0 or more. The test uses
   the id of any object as a creature type.
2. Else, for a creature whose weapon type has the value 3 in `Weapon_info` + 0x0A, and
   for a player whose current weapon is such a weapon: the state is not an attack state
   (6, 7, and for the player 11, 12).
3. In an attack state: the frame is below the fire frame of the state (`fire_frames[0]`
   for 6, `[1]` for 7, `[2]` for 11, `[3]` for 12).

So a submodel with flag 0x10 is the missile that the figure holds (an arrow, a dagger
to throw). It is drawn until the fire frame of the attack and is missing from then to
the end of the attack, while the missile object flies. (data) 388 submodels have the
flag, for example submodel 6 of `dfcb_p.pof`.

## The weapon in the hand and the player models

The models of a `$PLAYER` line are the same body with different things in the hands.
The body is only the pelvis and two arms: `hmc__p.pof` has 11 submodels (root, pelvis,
two arms of four parts and one empty end marker). `hmca_p.pof` has 13: the hand part
has 48 points in place of 14 and two more parts follow it (data, `model.py info`). The
viewer sees his own arms because the player object is drawn like any other object, with
argument 7 set.

The function at 0x43A5A changes the model when the player takes a weapon in the hand
(log text "CHANGE PLAYER MODEL"). It builds the name (code 0x43EB2 to 0x43F96):

`<race letter><sex letter><class letter><weapon letter>_P.POF`

- The first three letters are those of the player table (tbl.md), with the same five
  replacements.
- The weapon letter is byte 5 of the inventory record of the weapon. If it is 0 the
  letter is `G`. (data) The letters of the model names are the values of `weapon_char=`
  of the `$WEAPON` lines: `hmca_p.pof` to `hmcy_p.pof` and `a` to `y`. `_` is the empty
  hand.
- The model is searched by name (0x6DB99). If it is missing the function tries
  `...<letter>_M.POF`, then `..._<letter>M.POF`, then `..._<letter>P.POF`, then the name
  of the first model of the player (player struct + 0x1AED).
- With a negative argument the function takes the weapon away: letter `_` (0x43CDA).
- It sets object 0x162 and the model number at `*0x13A883`, pages the model in and
  then needs `box_flags` & 8 ("Player model must have a torso box").
- It returns without a change while the state is 6 or 11.

Creatures have their weapon in the mesh: submodel 24 of `durnan.pof` is the hand with
the sword (80 points).

### Where the weapon is (melee test)

Function 0x7473F (object, out vector, out radius, which, out weapon type) gives the
weapon sphere of the current frame (code):

- which 0 and `box_flags` & 2: center = object position + rotate(`weapon_center[frame]`,
  matrix at object 0x44), radius = `weapon_radius[frame]`.
- which 1 and `box_flags` & 4: the same with `weapon2_center` and `weapon2_radius`.
- It returns 0 if `box_flags` & 1 is not set or the figure has no weapon. The weapon
  type is `Robot_info[id].weapon_type` for a creature and the current item for a player.

The matrix at object 0x44 is the second matrix of the object (lvl.md: the loader copies
the orientation to it). Correction (code 0x9F28B, 0x6D129): the movement code and the
weapon test write the transposed orientation to it on each frame (0xD778A is the copy
with transpose). So `rotate(v, matrix at 0x44)` turns a vector of the model into the
world. The port uses the transposed orientation for the weapon sphere, the torso center
and the body center.

### Body size for physics

- 0x746B3 (object, out radius, out half height): for a polygon object with `box_flags` &
  1, radius = `body_radius[0]` and half height = `body_half_height[0]`. A weapon object
  gets the radius 0x800. A player gets at least 5.0. Other objects get `obj->size` for
  both.
- 0x748D9 gives the cylinder for collisions. It reads the model of the object, but for
  a player always model 16 (code 0x748F4). With `box_flags` & 8 and frames: center =
  position + rotate(`torso_center[0]`), radius = `torso_radius[0]`, half height =
  `torso_half_height[frame]`. On the last frame of DIE the half height is 0x1999 (0.1).

The walk physics uses these. They are listed here because the values are model data.

## Textures

- `ObjBitmaps[]` is at 0x18F19C (pointers to bitmaps). `ObjBitmapPtrs[]` is at 0x18D704,
  1700 entries. An entry is a pointer to an `ObjBitmaps` entry. The count
  `n_obj_bitmap_ptrs` is at 0x13A897, the count of `ObjBitmaps` is the word at 0x13A8A9
  (code 0x1DCA9, the Descent 1 function that adds one model texture; `%N` gives the
  entry of eclip N).
- Texture number t of a TMAPPOLY is `*po->textures[t]`.
- The archive loader sets `po->textures` from the header (code):
  - model of a player archive: `&ObjBitmapPtrs[first_texture]` (0x70B89),
  - model of the level archive: `&ObjBitmapPtrs[first_texture + 1]` (0x70EED).
- `load_polygon_model` (0x72DC5: eax = name, edx = texture count, ebx = pointer to the
  first slot, ecx = 1 for a main model, stack: pointers to the fire frames, speeds and
  state times) looks the name up first (0x6DB99). If the archives have the model it
  returns the number and changes nothing (0x72DD9). So the three lists of a table line
  do not reach a model of an archive: the values of the header count.
- After the call, two readers store the slot pointer of the running table parse in
  `po->textures`:
  - `$MONSTER`: `&ObjBitmapPtrs[start]` for the model and for the simple model
    (0x1EEFB, 0x1EF7B),
  - `$DRESSING`: `&ObjBitmapPtrs[start - 1]` (0x22B53).
- The readers of `$PLAYER`, `$WEAPON` (0x2252F) and `$SPELL` (0x25591) do not. For these
  the pointer of the header stays.

So for creatures and scenery the header value is not used, and the rule of the port (a
model draws with the texture list of its table line) is what the executable does.

Not solved: for weapon and spell models of a level archive the header value plus 1
must be the slot that the running parse gives. A count with `YPINN.BIN` and `HMC.BIN`
does not confirm this: the `$PLAYER` line of the player table takes 174 slots in place
of the 30 of the place holder line, so all later slots move by 144. Either a step of
the parser was missed or these models draw with wrong slots in the original. (guess)
The 1 is the stray bitmap line of the player table (`HMC.BIN` line 24). The port should
keep its rule for all models.

Check (`test_dtudata -texcheck DIR [-texorig] DATADIR ...`, test_dtudata.c): for each
table of DIR, loaded with `HMC` and with `HMT`, each texture number that a TMAPPOLY of
a model uses is resolved as `draw_polygon_model_frame` does, and a reference that gives
bitmap 0, an empty bitmap, an all transparent bitmap or a bitmap that is 90% black is
listed. Result over the 78 tables of the disc (156 loads, 60972 references): no
reference of a weapon, arms or creature model is bad. The listed references are the
`*empty` bitmaps (transparent by design: `pempty` of the arms, `empty` of `rat_s.pof`,
`b01empty` ... of the place holder models) and models that no table line names (the
place holders `a01temp1.pof` ... and spell models of tables without the spell).
With `-texorig` the check also applies the header rule above to the `$WEAPON`,
`$SPELL` and arms models: 1460 references give another bitmap than the port, and 60
give bitmap 0 (MINES1, PRISON1, TRAIN1: `arrow.pof`, `flare.pof`, `cbbolt.pof`,
`ltning.pof`, `ice.pof`, `acid.pof`, `m_a.pof`, `e_a.pof`, `y_a.pof`). This is the
likely cause of the black weapons of the original (bugs.md D14). (guess)

## Timing

### The frame time (0x40E91, calc_frame_time)

Code:

1. `raw = timer_get_fixed_seconds() - last` (timer function 0xDFAE0, last value at
   0x13B270).
2. If raw is not above 0 or is above 1.0: message "Bad FrameTime", the old value stays.
3. If the switch at 0x13B697 is set: double it.
4. `RealFrameTime` (0x1C7FD0) = the value.
5. Clamp: `FrameTime` is at least 0x1B4 (436 / 65536 s, that is 1/150 s) and at most
   0x3333 (0.2 s).
6. If the value at 0x13B687 is not 0 it replaces the frame time.
7. Store `FrameTime` (0x13CD2C).

`GameTime` (0x13CD30) is the sum of the frame times. So above 150 frames per second the
game time runs faster than the clock: at 300 frames per second everything that uses
`GameTime` or `FrameTime` runs at double speed. A frame limit in the main loop was not
searched.

### Active creatures: by time (0x11018)

`frame = first[s] + ((elapsed * ((first[s + 1] - first[s] - 1) << 16)) / state_times[s]
+ 0x8000) >> 16` with `elapsed = GameTime - ailp[0x24]` (64 bit product, code 0x11139
to 0x111B0). With the backward flag of the AI: `first[s + 1] - 1` minus the same
term. The state ends when the frame reaches `first[s + 1]`. Then state = goal state,
frame = `first[state]`, start time = `GameTime`. The rest is in ai.md.

### The player: by time (0x4450C)

Called once per game frame from 0x435A6 and 0x4391D. Code:

1. s = object 0x170, goal = object 0x176, `elapsed = GameTime - anim_start` (player
   struct, 0x1D3373 for player 0).
2. If `state_times[s]` is 1.0 or more (the test is on the integer part, 0x44589) and
   the state has frames: `frame = first[s] + ((elapsed * ((first[s + 1] - first[s]) <<
   16)) / state_times[s] + 0x8000) >> 16`. With a state time below 1.0 the frame is not
   computed and stays.
3. If object byte 0x1AF is above 1: decrement it. When it reaches 0: frame =
   `first[s + 1]`. This is a count of game frames.
4. The state ends if frame >= `first[s + 1]` (or frame < `first[s]` with flag bit 1 of
   byte 0x0D). If it has not ended: return if state == goal. Else end it at once if the
   goal is 6, 7, 11, 12 or 9, or if flag bit 8 of byte 0x0D is set. Else return.
5. At the end: clear the flag bits 1 and 8. After an attack state with a weapon whose
   `Weapon_info` + 0x0A is 3 and whose count is 0, change the player model (0x43A5A with
   -3 or -4, the weapon is used up).
6. State 9 (DIE): frame = `first[10] - 1` and stay. Else: `anim_start = GameTime`, state
   = goal, frame = `first[state]`.

### Idle creatures: per game frame (0x14013)

This code runs for a creature that the AI does not process fully (ai.md, "do_ai_frame"
steps 5 and 22: not active, or without AI states, as a peaceful character that stands
around). Code 0x14013 to 0x140F8:

1. Add 1 to the frame if the state is WALK, or above SIDLE_L, or the object has flag
   bit 2 of byte 0x0D, or `(clock + objnum) % 10 == 0`.
2. If the frame is `first[s + 1]`: the next state is FIDGET if `(FrameTime + objnum) %
   (iq >> (awareness + 1)) == 0`, else STAND. Frame = `first[state]`.

`clock` is the value at player struct 0x1D339F. It is not a frame counter. It is a time
in 16.16 seconds: 0x4D81A adds `FrameTime` to it each frame and wraps it at 3600.0
(then the byte at 0x1D3391 counts up). The remainder of such a value by 10 is 0 in
about one game frame of ten. So:

- STAND, REST and the SIDLE states of an idle creature advance by one animation frame
  each ten game frames on average, at any frame rate.
- FIDGET, FLINCH, DIE, RUN and ATTACK of an idle creature advance by one animation
  frame each game frame.

The other tests of the AI on this value ("every 10th tick", "every 20th tick" in
ai.md) are of the same kind: they are true in one game frame of N.

### Answer to "animations run wrong at high frame rates"

Verified causes in the code:

1. The idle animation is counted in game frames (above). At 20 frames per second the 12
   frames of FIDGET take 0.6 s, close to the state time of 0.75 s. At 70 frames per
   second they take 0.17 s. STAND runs at one tenth of the frame rate.
2. The frame time has a floor of 1/150 s. Above 150 frames per second all time based
   animation and movement runs too fast.
3. The texture pulse of `draw_polygon_model` and byte 0x1AF of the player count calls
   and frames.

Recommendation for the port: compute the idle animation from time. Use `state_times[s]`
for the states that advance each frame. For STAND use one animation frame each 0.5 s
(ten game frames at the 20 frames per second that the state times suggest as the design
frame rate, (guess)). Do not copy the floor of the frame time, or keep the game logic
at a fixed tick.

Correction (code 0x14A54, found with the port of the creatures): the idle animation is
seen much less than the text above says. Step 20 of `do_ai_frame` puts a creature that
is not active back to STAND, frame 0, each time the creature is processed. The time
slicing of step 19 processes a creature on each frame if it is nearer than 100.0, each
0.5 s if it is nearer than 150.0, and each 1.0 s beyond that. So a creature that is near
and not active stands on frame 0 at any frame rate. Only a creature farther away plays
the idle steps between two visits, and there the number of steps depends on the frame
rate.

What the port does (`engine/main/dtuobj.c`):

- `Dtu_fix_idle_anim` (on by default): one idle step (0x14013) for each 3/64 second, the
  reference frame time of physics.md, and the tenth step is the one that moves STAND,
  REST and the SIDLE states. So STAND moves by one frame each 0.469 s and FIDGET takes
  0.5625 s. Off: one step for each game frame, as the executable.
- The reset of step 20 is kept in both cases. `Dtu_idle_anim_near` (off by default, option
  `-idleanim`) leaves it out, so that the idle animation runs at all distances. This is
  not the behavior of the executable.
- The frame time floor of 1/150 s is in `calc_frame_time` of the port as in the executable.
  The frame limit of the port (`Game_max_fps`, 60) keeps the frame time above it.

Measured in the port (level BANE, 27 creatures, 40 seconds of game time, options
`-idleanim -noattack -objlog FILE -objlogframes`, fixed frame rates):

| Frames per second | Corrected: STAND, seconds per frame | Corrected: FIDGET, seconds | Rule of the executable (`-origobj`): STAND, seconds per frame |
|---|---|---|---|
| 20 | 0.464 | 0.60 | 0.249 |
| 60 | 0.465 | 0.56 | 0.083 |
| 200 | 0.465 | 0.56 | 0.050 |

The values of the executable rule are for a constant frame time: the clock then has
a short cycle of remainders by 10 (5 frames at 20 per second).

## Proof with the tool

`tools/model.py` reads a model from an EGG file, applies the rules above (frame major
angle array, parent array, Descent 1 angle matrix) and writes an OBJ file or a picture.

```
python -I tools/model.py info  gamedata/unpacked/PORTAL.EGG durnan.pof
python -I tools/model.py obj   gamedata/unpacked/PORTAL.EGG durnan.pof out.obj --frame 8
python -I tools/model.py sheet gamedata/unpacked/PORTAL.EGG durnan.pof walk.png --state walk --step 2
python -I tools/model.py sheet gamedata/unpacked/PORTAL.EGG durnan.pof mix.png --frames=-1,0,44,50,56,84,90,95
python -I tools/model.py check gamedata/unpacked/*.EGG
```

Results that were looked at:

- `durnan.pof`, state WALK, frames 4 to 18 in steps of 2, side view: a man with a sword
  in the right hand. The legs swing in turn and the feet stay near the floor line. The
  cycle closes (frame 18 is close to frame 4).
- Frame -1: straight legs, both arms forward (the modeling pose). Frame 0 (STAND):
  upright, sword held up. Frame 50 (ATTACK): body bent forward, sword swung down. Frame
  56 (the fire frame): sword level, forward. Frames 84, 90, 95 (DIE): upright, falling
  back, flat on the floor.
- `check` over all archives: 2187 models, 0 submodel streams that do not parse, 0
  SUBCALL opcodes, child lists equal to the parent array for all 1142 models with more
  than one submodel.

The pictures are not in the repository because they show game art. The commands above
make them again.

## Not covered

- The names and the use of the states 11 to 18.
- `speeds[1]`.
- How an animated scenery object plays its frames (`animates`, `first_frame`,
  `last_frame` of `$DRESSING`, models with 4 or 5 frames such as `chest5.pof`).
- `calc_gun_point` and the use of `gun_points`.
- The interpreter itself (0xF07AA) was not compared with Descent 1 line by line. It
  draws the data correctly in the port, see egg.md.
- The texture slot rule for weapon and spell models (see "Textures").
- Object field 0x16E.
- The special draw 0x673AD and the magic effect bytes 0x14C4CD and 0x14C4D0.
- Morph objects and debris with these models.

## The port

`engine/main/polyobj.c` and `polyobj.h`:

- `polyobj_load_dtu_model` reads the header and all 19 arrays into the fields of
  `polymodel` with the names of this document (`anim_angles`, `frame_eye`, `box1_min` to
  `box8_max`, `body_center`, `body_half_height`, `body_radius`, `torso_*`, `weapon_*`,
  `weapon2_*`, `submodel_flags`, `submodel_children`, `box_flags`, `gun_submodels`,
  `fire_frames`, `speeds`, `state_times`, `n_guns`, `gun_points`). The arrays of the
  submodels (2 to 9) have 64 entries (`MAX_POLYMODEL_SUBMODELS`). `MAX_SUBMODELS` (10)
  still sizes the joint angles in the object of Descent 1, which the models of the target
  game do not use.
- `draw_polygon_model_frame` is 0x727A5 without the simpler model, the pulse counter and
  the effect modes: texture list, position rule, `g3_start_instance_matrix`, then the
  sorted draw for a model with more than one submodel.
- The sorted draw is 0x66E13: pass 1 walks `submodel_children` with
  `g3_start_instance_angles` and keeps the instance state and the depth of each joint,
  pass 2 sorts by depth (largest first) and draws each submodel with its saved state.
  `g3_get_instance_state` and `g3_set_instance_state` in `engine/3d/instance.c` are the
  functions 0xEA919 and 0xEA93A. The instance stack of the 3d library has 72 levels in
  place of 5.
- All models draw with the perspective texture mapper. The original draws each object
  but the viewer with the linear mapper (bugs.md D12).
- Animation interpolation (`Dtu_anim_interp`, on by default, `AnimInterpolation` in
  `ascent.cfg`, item of the Graphics menu, option `-noaniminterp`):
  `draw_polygon_model_frame_lerp` draws the joint angles (each the short way around,
  as a signed 16-bit difference) and the body center between the frame of the game and
  the next frame of the state. `polyobj_anim_track_update` gives the next frame and
  the fraction: it learns the time of one frame from the frame changes that the draw
  sees (the mean since the first change of the state), or takes it from the state time
  where the game counts the frames by time (active creatures 0x11018: `state_time / (n
  - 1)`, the arms 0x4450C: `state_time / n`). It does not interpolate past the last
  frame of the state, nor over a change of the state, nor over a jump of more than 2
  frames or against the direction of the state: the new frame is drawn at once. The
  game rules keep the frame numbers (`DTU_ANIM_FRAME`, `View_frame`). The picture
  leads the frame of the game by up to one frame (it reaches the pose of frame F+1
  when the game frame becomes F+1). Callers: `dtu_draw_polygon_object` (creatures,
  players) and `dtu_combat_draw_view` (the arms).
- Not in the port: the magic effect (flag 0x20, effect modes), the special draw 0x673AD,
  the texture pulse, the draw of the viewer (the arms of the player), the model change of
  the player.

Checked by eye: `durnan.pof` in the level YPINN stands in the pose of frame 0 of the
sheet of `tools/model.py` (upright, sword held up), and the skeletons, ghouls and orcs of
BANE and DROW1 draw with whole bodies and weapons in the hand.
