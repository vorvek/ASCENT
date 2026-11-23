# Combat and magic of the player (DTUGAME.EXE 1.3)

Status: read from the code of DTUGAME.EXE 1.3 (static reading of
`gamedata/re/dtu_flat.bin`, object 1 at 0x10000, with `tools/dtudis.py` and
`gamedata/re/all.asm`). Nothing here was tested in the running original. Spell
lines are from the decoded tables `BANE.BIN` and `YPINN.BIN` (`tools/egg.py bin`).

Marks: "verified" or "Code" means read in the disassembly at the given address.
"(guess)" marks an interpretation that is not proven.

Related documents: [models.md](models.md) (player models, animation, weapon sphere),
[ai.md](ai.md) (to-hit 0x2A7F3, damage 0x29A9C, creature side), [character.md](character.md)
(player record, item lists, combat values), [objects.md](objects.md) (the 124 byte
effect block; corrected below), [physics.md](physics.md) (controls, reference frame
time), [../formats/tbl.md](../formats/tbl.md) (`$WEAPON`, `$SPELL` keywords).

Part A is the weapon attack and missile weapons. Part B is the spell system, items
and thief skills. Part C is the port.

Corrections to other documents:

- objects.md: 0x8ED49 is not a function, it is an instruction in the Jump case of
  0x8CFFF. Bit 4 of the effect word 0x88 is Deaf, bit 7 Feather Fall, bit 16
  Improved Invisibility, bit 17 Invisibility, bit 24 regeneration (Part B, section 4).
- The `$SPELL` names of the lines 42 ("Resist Cold") and 51 ("Burning Hands") do not
  match their code: 42 is Cure Blindness or Deafness and 51 is Find Traps.
- The player attack is always state 6 (ATTACK); the states 7, 11 and 12 are never
  chosen by the input of 1.3 (Part A, section 1). The comment of dtuobj.h ("11 and 12
  are attacks of the player") holds for the model data only.

# Part A. The weapon attack and missile weapons

## A0. Shorthand and data used below

- `P` = player record of the current player: the 0x1D3xxx / 0x1D4xxx
  addresses below are indexed with `[0x13B924] * 0x1B37`.
- `Objects` base 0x2B0614, stride 0x1DF. Player object pointer `[0x30DF14]`,
  `Viewer` `[0x13D628]`. Player object number: `P+0x1D3333`.
- Object fields used: +0x04 type, +0x05 id, +0x0C render type (byte), +0x0D
  flags, +0x14 position, +0x20 orientation, +0x44 transposed orientation,
  +0x6C hit points, +0xFC velocity, +0x108 thrust, +0x114 mass, +0x118 drag,
  +0x11C gravity, +0x13E physics flags, +0x140/+0x142 parent type/number,
  +0x162 model, +0x170 animation state (s16), +0x172 frame, +0x176 goal
  state, +0x1AF per-swing byte.
- `Weapon_info`: base 0x1E9D08, stride 0xD9 (217). Offsets verified from the
  `$WEAPON` parser 0x209F3 (table below).
- `Robot_info`: base 0x38E344, stride 0xF2 (ai.md table).
- `Polygon_models[]` pointers at 0x3880D0. Model header: `fire_frames[4]` s16
  at +0xB8, `state_times[19]` fix at +0xE4 (so `state_times[6]` = +0xFC).
- First frame of a state: s16 at `0x13D9E0 + 2*state` (verified 0x4455D).
  States 6, 7, 11, 12 start at frames 44, 60, 108, 124 and have 16 frames.
- GameTime `[0x13CD30]`, FrameTime `[0x13CD2C]`, Difficulty `[0x13CDCA]`.
- `Controls` (control_info) at 0x1E9A68, 0x20 bytes, cleared each frame at
  0x43934. +0x19 (0x1E9A81) is the attack state, +0x18 (0x1E9A80) the key
  down time.

### Weapon_info offsets (verified, parser 0x209F3)

| Off | Address | Key | Type |
|---|---|---|---|
| +0x00 | 0x1E9D08 | render type (-1 none, 1 blob, 2 `weapon_pof`, 3 vclip) | byte |
| +0x01 | 0x1E9D09 | `flash_vclip` | byte |
| +0x02 | 0x1E9D0A | `monster_hit_vclip` | byte |
| +0x03 | 0x1E9D0B | `monster_miss_vclip` | byte |
| +0x04 | 0x1E9D0C | `wall_hit_vclip` | byte |
| +0x07 | 0x1E9D0F | `ammo_usage` | byte |
| +0x0A | 0x1E9D12 | `bounce=1` -> 1, `stick=1` -> 2, `thrown=1` -> 3 (value must be non-zero, 0xDD9BB) | byte |
| +0x0B | 0x1E9D13 | `homing` | byte |
| +0x0D, +0x0E | 0x1E9D15/16 | `damage_dice`, `#damage_dice` | byte |
| +0x12 | 0x1E9D1A | `attack_type`: 1 melee, 0 projectile | byte |
| +0x13 | 0x1E9D1B | `weapon_char` | byte |
| +0x1C | 0x1E9D24 | `ammo_initial` | s16 |
| +0x20 | 0x1E9D28 | `flash_sound` | s16 |
| +0x22 | 0x1E9D2A | `POV_clip` | s16 |
| +0x24 | 0x1E9D2C | `monster_hit_sound` | s16 |
| +0x26 | 0x1E9D2E | `monster_miss_sound` | s16 |
| +0x28 | 0x1E9D30 | `monster_kill_sound` | s16 |
| +0x2A | 0x1E9D32 | `wall_hit_sound` | s16 |
| +0x2C | 0x1E9D34 | `armor_hit_sound` | s16 |
| +0x36 | 0x1E9D3E | `to_hit_bonus` | s16 |
| +0x38 | 0x1E9D40 | `damage_bonus` | s16 |
| +0x3A | 0x1E9D42 | `PC_spell` | s16 |
| +0x3C | 0x1E9D44 | `enemy_spell` | s16 |
| +0x3E | 0x1E9D46 | flags: 0x01 `2_handed_weapon`, 0x02 `stackable`, 0x04 `arrow`, 0x08 `bolt`, 0x10 `magic`, 0x20 `bonus_exempt`, 0x40 `auto_hit`, 0x80 `non_hotkey_item` | byte |
| +0x60 | 0x1E9D68 | `reach` | fix |
| +0x64 | 0x1E9D6C | `fire_wait` (values below 0xCCD = 0.05 become 0.05, 0x21660) | fix |
| +0x68 | 0x1E9D70 | `fire_delay` | fix |
| +0x6C, +0x70 | 0x1E9D74/78 | `blob_size`, `flash_size` | fix |
| +0x74 | 0x1E9D7C | `impact_size` | fix |
| +0x78 | 0x1E9D80 | `speed[5]` (one per difficulty) | fix |
| +0x8C..+0x98 | 0x1E9D94.. | `mass`, `drag`, `thrust`, `gravity` | fix |
| +0xA8 | 0x1E9DB0 | `lifetime` | fix |
| +0xBC | 0x1E9DC4 | `damage_percent` | fix |
| +0xCF | 0x1E9DD7 | `combustion` | s16 |

`reach` (+0x60) is written by the parser and read nowhere by direct address
(grep of all.asm). So `reach=` has no effect in 1.3 (verified for direct
addressing).

## A1. Input and the start of the attack

### Key, button and the read (verified)

- Key config items are 22 bytes (0x16) with the type at +0x14 and the value
  at +0x15 (0x515AB). Arrays: keyboard 0x13C190 (62 items), joystick 0x13C6E4
  (27), mouse 0x13C978 (29) (0x5225D). Defaults: 6 rows of 64 bytes at
  0x13C010, copied to `kconfig_settings` 0x1E9A88 (0x71A0E); `kconfig.dat`
  saves 0x180 bytes of it (0x7190F).
- Attack action: text number 0x186 (390). Keyboard items 18 and 19, default
  scan codes 0x1D (Left Ctrl) and 0x9D (Right Ctrl) (bytes 18 and 19 of
  0x13C010). Joystick item 0, default button 0 (row 1, 0x13C050). Mouse item
  0, default button 0, the left button (row 5, 0x13C150; (guess) row 5 is the
  mouse row as in Descent 1).
- Read in `controls_read_all` (around 0x52740): keyboard 0x538CC, while
  `keyd_pressed[key]` (0x467B40) of item 18 or 19 is set: Controls+0x18 =
  `key_down_time` (0xDA650), Controls+0x19 = 1. Joystick 0x529C3 (button
  state 0xDF360), mouse 0x52DFF (`mouse_button_state` 0xE8ADC, ORed into
  +0x19). So Controls+0x19 is 1 on every frame the input is held.
- Edge: the game control function 0x2FB10 (called at 0x4378B from the game
  frame 0x435A6), code 0x2FCBE: while Controls+0x19 is set, a request is made
  only if it was not held on the last frame (`[0x13AD7E]`, set while held,
  cleared on release). A misc item (type 0x10) in the hand is used instead
  (0x2FAF0, 0x37A6A). Else the request `[0x13B69F] = 2` (0x2FD41). So the
  attack starts on key down. Holding the key does not repeat it.

### Order in a game frame (verified, 0x435A6)

1. 0x2FB10 controls (0x4378B), sets the request.
2. 0x4450C player animation (0x43835).
3. 0x69C5C moves all objects (0x4389A): the player 0x9DC66 calls the hit test
   0x6CFF8 once (0x9DD69, no loop around it); weapon objects move with
   0xA03A8.
4. 0x44300 starts the attack (0x438B1).

### 0x44300 (start attack) and 0x41A4D (gate), verified

0x44300 does nothing unless 0x41A4D returns non-zero. 0x41A4D:

1. Clear the request if the hand slot index (byte `P+0x1D4DF4`) is negative
   and the word 0x13AD15 is not negative. Return 0 if there is no request or
   if `[0x13D3F7]` is not 0 (request kept).
2. Spell (item type 0x11): casting checks and texts (other agent).
3. Weapon with flag `arrow` (4) and no bow in `P+0x1D4DD6` (signed byte < 0),
   or flag `bolt` (8) and no crossbow in `P+0x1D4DD8`: message (text pointer
   0x396A10 or 0x396A14), clear the request, return 0.
4. Melee weapon (`attack_type` 1, not a spell) (0x41E36): with `st` =
   `state_times[6]` of the current player model (model +0xFC),
   `|GameTime - last|` must be greater than `st` (`last` = `[0x13B74B]`), and
   Controls+0x19 must still be 1. Else clear the request, return 0. On
   success `last = GameTime`; if player object byte 0x89 bit 0x20 is set
   (flag word 0x88 bit 0x2000), `last += st / 2`, so the next attack is
   allowed only 1.5 `st` later. Return 1.
5. Other items (missile weapons, empty hand of a projectile type): return 1
   with no time gate.

0x44300:

1. `esi = [0x13B69F] - 1` (= 1, the hand). If the state is not 6, 11, 7 or
   12: byte 0x1AF = 0, and the goal state becomes 6 if `esi == [0x13B62A]`
   (debug text " right"), else 11 (" left"). With `[0x13B6A3]` not 0 it would
   be 7 or 12. If the state is already an attack state the goal is not
   changed.
2. `[0x13B62A]` is 1 in the image and has no write in the code. `[0x13B6A3]`
   is only ever written with 0 (0x2A9AA, 0x4447B, 0x56C5D, ...). The only
   request value is 2 (0x2FD41). So the player attack of 1.3 is always state 6
   (ATTACK). States 7, 11 and 12 are never chosen by the input (verified for
   direct writes; no indirect write was looked for).
3. Item type 5: `P+0x1D33BC &= 0xEC`; if player byte 0x8A bit 2 is set,
   object +0xC8 = 0 ((guess) attacking ends an invisibility).
4. Spell, or weapon with `attack_type` 0: call 0x56A0D (fire, section 5).
   Then if the stack count (+8) of the item is 0 and 0x13AD15 >= 0: goal =
   state = 1 and 0x544F2. The request is kept until 0x56A0D fires.
5. Melee: the request is cleared here (0x44479). So a press that fails the
   gate of 0x41A4D or that is accepted is consumed in the same frame. No
   buffer.

### Attack rate (verified)

- The weapon change 0x43A5A (0x441E9 to 0x44258): for a melee weapon,
  `t = max(fire_wait, 1.0)`; if player object byte 0x8B bit 0x08 is set (flag
  word bit 0x08000000, (guess) a slow effect) `t = 2 * t`. `t` is written to
  `state_times[6]`, `[7]`, `[11]`, `[12]` of the current player model
  (+0xFC, +0x100, +0x110, +0x114).
- So the swing lasts `t` seconds (0x4450C uses `state_times[s]`) and the gate
  of 0x41A4D allows a new swing after more than `t` seconds. Every melee
  `fire_wait` of the tables is 0.5 to 1.5: every weapon below 1.0 swings at
  1.0 s. Long Sword 1.0, Bastard Sword 1.3, Warhammer 1.5 (BANE.BIN).
- Missile weapons have no gate in 0x41A4D. Their rate: section 5.
- All timing rules with Haste, Slow and the items of speed: section A8.

### What else stops an attack

- 0x43A5A returns without a model change while the state is 6 or 11.
- The to-hit roll fumble can take the weapon away (0xAE837, character.md).
- In an attack state 0x44300 does not restart the attack.

## A2. The first person view

### Call path (verified)

`render_frame` 0x76C7C -> `render_mine` 0x77845 -> `render_segment` 0x75F10
(walls of the segment, then each object of the segment) -> `do_render_object`
0x75D25 (no viewer test, only "already rendered") -> `render_object` 0x67C5A
-> `draw_polygon_object` 0x6746A -> `draw_polygon_model` 0x727A5 with argument
7 = (object == Viewer and Viewer type != 5) (0x679B6).

So the arms are drawn as an ordinary object of the segment that holds the
player, after the walls of that segment, in the painter order of
`render_mine`. There is no separate pass, no depth clear and no special
clipping (Descent 1 has `if (obj == Viewer) return;` in `render_object`; the
1.3 code has no such test).

### render_object 0x67C5A, only for the player object (verified)

Before the draw, the position of the player object is changed and it is
restored after the draw (0x67EBD):

1. Floor type 1 under the player (segment byte +0xF5 `& 3 == 1`, after the
   segment redirect at +0x1E8): `y -= 0x1388` (0.0763).
2. If `[0x13B250]` is not 0: `y += 0x13880 - 16 * look_pitch`
   (`look_pitch` = s16 at 0x13D9AE, fixang). 0x13880 = 1.2207. At the limits
   of `look_pitch` (-0x3000, 0x2FA0) the change is +4.22 and -1.76.
3. If also `look_pitch > 0`: `k = 2 * look_pitch` (as fix, at most 0x5F40 =
   0.372), with s, c = sin, cos of (heading - 0x2000): `x -= fixmul(s,k) +
   fixmul(c,k)`, `z += fixmul(s,k) - fixmul(c,k)`. That moves the model back
   along the heading by `k * sqrt(2)` (at most 0.526).
4. `[0x13B250]` is set to 1 at the start of every model change 0x43A5A
   (0x43A8D) and to 0 when the weapon in the hand has the flag `arrow` or
   `bolt` (0x43BDA to 0x43BEA). So the pitch offset is off for missile
   weapons with a launcher.

The orientation is the object orientation (+0x20) without the look pitch, so
the arms do not tilt with the view; they only move up, down and back. (guess)
positive `look_pitch` looks down.

### Inside draw_polygon_model 0x727A5 (verified)

Argument 7 is read at one place only (0x72889): the choice of the simpler
model. With argument 7 = 0 a model with a simpler model always uses it; with
1 it uses it by depth (Descent 1 rule). No model of the disc has a simpler
model, so argument 7 changes nothing. The sorted draw 0x66E13 does not get
argument 7 (its stack arguments are glow, argument 3, 0, argument 6, argument
8; 0x72A66). The position rule (step 7 of models.md, body center) applies to
the player as to any object. All submodels are drawn but the hidden ones
(flag 0x10) while "show hidden" is 0: the missile in the hand vanishes from
the fire frame on (models.md, "The missile in the hand").

### Camera (verified, 0x76C7C)

- `eye = pos + uvec * eye_offset` (`[0x30DF04]`, 5.0 for the human male
  fighter of HMC.BIN) `+ frame_eye[frame].y` (model +0x60, 12 bytes a frame).
- View bob when byte 0x14530D (move mode 1 walk, 2 run) is not 0:
  `eye.y += 0.325 * sin(phase)` (0x5333), angle = `phase * 65536.0` (float at
  0x12AF07), `phase += FrameTime * 2^-16 * mode` (double at 0x12AF0B). So one
  bob period per second walking, two running. Time based.
- View matrix = `angles_2_matrix(look_pitch, 0, 0) * orient` for the player
  (0x76FC4), the head tracker angles at 0x38E338 otherwise.
- The model is not bobbed: the bob changes only the eye.

## A3. The swing and the hit test

### Hit window (verified, 0x6CFF8, called each game frame from 0x9DD69)

1. The object position is moved forward by `sqrt(2)` (1.414) along the
   heading for the test and restored at the end (0x6D017 to 0x6D066, 0x6D5C6).
2. Not in state 6, 7, 11 or 12: byte 0x1AF = 0, return.
3. `rel = frame - first[state]`, `n = first[state+1] - first[state]` (16).
   The player tests when `rel >= 6` and `rel <= n - rel`: frames 6, 7, 8 of
   the state, that is from 6/16 to 9/16 of `t`.
4. Lolth special case (level byte `P+0x1D338D == 0x20` and object number
   0x24): damage to the player, text "Lolth struck you with a mighty blow!".
5. Weapon sphere 0x7473F(object, frame, which 0): center = position +
   rotate(`weapon_center[frame]`), radius `weapon_radius[frame]`; returns the
   weapon type (the item in the hand for the player). Radius times 8.0 for
   the weapon types 0x12, 0x13, 0x22, 0x23, 0x2B, 0x2C, 0x2D, 0x45 (BANE.BIN:
   Dagger, Short Sword, Short Sword +1, +2, Dagger +1, +2, Dagger of
   Impaling, Short Sword of Quickness).
6. `find_vector_intersection` 0x3EFD9 from the center to `center + r *
   normalize(center - position)`, flags 1 (objects), this object = the
   attacker. The query has the radius 0 at +0x14 and r at +0x18 (0x6D33E,
   0x6D348; physics.md section 3): the walls are tested with 1.0, the floor
   with 0 and the ceiling with 1.5 r (0x3F58C). The object test (0x3EE70,
   0x4F7D5, physics.md section 3) adds 0 to the radius of a body and r to its
   half height: r is a vertical reach only. The swing of a dagger (r about
   17 with the factor 8) reaches far up and down, not to the sides.
   fvi_sub keeps the nearest contact; a wall that is nearer than the object
   wins, except the floor (side 3) against an object (0x3F8FF).
7. Player only (0x6D3D0): if the query hit an object, or a wall whose side
   number (dword at hit data +0x14) is not 1 and not 3 ((guess) not the
   ceiling, not the floor): byte 0x1AF = 0, `[0x13D430]` = hit object, and the
   animation start `P+0x1D3373 += (2*rel - n) * fixdiv(n, t)` (as fix
   seconds, 0x6D430 to 0x6D47A). For `t` = 1.0: rel 6 gives -64 s, rel 7
   -32 s, rel 8 0. A negative shift makes the next 0x4450C jump past the end
   of the state: the swing ends at once. At rel 8 nothing changes. A wall hit
   then calls the trigger touch 0x95019(segment, side, 4, object)
   (triggers.md).
8. Object hit ("I hit something."):
   - scenery (type 18): if bit 4 of the byte at `0x19B340 + 32 * id`
     (`Dressing_info`, 32-byte entries) is set: no effect. Else hit points limited to 20.0, damage function
     0x29A9C(scenery, no attacker, 100.0, weapon = attacker id). Without an
     attacker the damage function subtracts the 100.0 directly (0x29BCA), so
     the scenery always drops below 1.0 and breaks: 0xA6ACB(scenery, 0,
     10.0). The push branch 0x25D4F is reached only if the hit points stay at
     1.0 or more.
   - creature (type 2), attacker not a creature: 0x278B5(creature, player,
     weapon type, hit point 0x30EF58), then byte 0x1AF = 0.
   - player (type 4): creatures only (ai.md).

### The swing and the look pitch (verified, 2026-11-23)

- The look pitch is the word at 0x13D9AE. It changes only the view
  (`render_frame` 0x76FC4), the place of the arms (0x67D04), the missile
  direction (0x562E8, for the player object only) and the arms offset in
  `ai_animation_step` (0x11640, 0x11657). The player object orientation (+0x20)
  has the heading only.
- The hit window copies the orientation (+0x20) transposed to +0x44 (0x6D129 to
  0x6D12F). 0x7473F rotates `weapon_center[frame]` with +0x44 (0x74821, 0x7483F)
  and adds the position (+0x14, 0x74849). No instruction of 0x6CFF8 or 0x7473F
  reads 0x13D9AE. So the swing goes where a level look goes, at every pitch.
- Start and direction: the position moved `sqrt(2)` forward along the heading,
  plus the rotated weapon center; the ray goes from the center away from that
  point by the sphere radius r. The vertical reach is r (query +0x18). For the
  Broad Sword in HMC the hit frames 6 to 8 put the center 2.8 to 7.0 below the
  eye and 3.8 to 4.8 in front of it, r 2.6 to 2.8: the swing covers the low
  front, not the center of the view.
- Result: a GiantRat (body radius 5.27, half height 1.45, its top 11.45 below
  the eye of the human fighter) is hit by the level Broad Sword at few
  distances (none from 2 to 9 in front in CLAN3), and looking down at it does
  not help. The Short Sword and the Dagger (r times 8, about 17) reach it.
- Missiles: the start is the position plus 2.0 up (0x562C1), the direction
  `rotate((0,0,1), angles_2_matrix(look_pitch,0,0) * orient)` (0x562E8 to
  0x56323). Thrown weapons, arrows, bolts and spell missiles all go through
  0x561DA, so they all follow the look pitch. The start is 3.0 below the eye
  (eye_offset 5.0): a missile goes a little under the center of the screen.

### One hit per swing? (verified, a defect of 1.3)

0x278B5 has a guard: if byte 0x1AF of the attacker is not 0 it prints
"Register only one collision per smack" and returns; else it sets the byte
to 8. But step 7 above sets the byte to 0 just before every call. For a first
contact at rel 6 or 7 the swing ends at once (one hit). For a first contact
at rel 8 the shift is 0 and the test repeats on every game frame while the
frame stays 8: one hit roll per game frame for `t / 16` seconds (1/16 s at
`t` = 1.0: about 4 rolls at 60 frames per second, 1 at 16). 0x4450C counts
the byte down by 1 per game frame while above 1 (8 to 1); the branch that
would force the end frame when it reaches 0 cannot run, since a decrement
from above 1 never gives 0 (0x445B5 to 0x445D6).

### 0x278B5 (player hits creature), verified

1. Player goal state = 10 (0xA).
2. Weapon type 13 with the creature flag `Robot_info+0xD9` bit 0x08 clear:
   plot code (quest bits 0x82 to 0x8D through 0xADB3B, text 0x1FD).
3. Creature byte 0x1CC = 2 (active). Return if the creature flags 0x0D have
   bit 1 or 2.
4. Guard byte 0x1AF (above). Return with "Player aint using a weapon???" if
   the player state is not 6 or 11. Byte 0x1AF = 8.
5. Weapon item: `monster_hit_sound` linked to the creature, volume 0x8000
   (0.5) (0x2E600). Then the creature `hit_sound` (+0x9A), volume 0.5.
6. Alert 0x160AB(player, 3); 0x12AD9(creature, 5).
7. Damage function 0x29A9C(creature, player, 0, weapon type).
8. `enemy_spell` (+0x3C) of the weapon >= 0: cast it on the creature
   (0x90CEF or 0x8CFFF by spell flag bit 1).
9. Hit points went down: blood 0x25C76 (`Robot_info+0xDA`); sound: creature
   `hit_sound` at 0.5 if hit points >= 1.0, else `die_sound` (+0x9C) at 1.0;
   push: `v = creature.pos - player.pos`, `v.y = 0`, normalized, times 20.0
   (0x140000), 0x6B70D (adds v / mass to the thrust, ai.md);
   `monster_hit_vclip` (+0x02) explosion at the hit point, size
   `impact_size`; `monster_hit_sound` (alive) or `monster_kill_sound` (dead)
   at the hit point, volume 0.5 (0x2E900).
10. No damage (miss): `monster_miss_vclip` (+0x03) explosion, size
    `impact_size`; `monster_miss_sound` at the hit point, volume 0.5.

### Damage function 0x29A9C for a creature target (verified)

- Return if the target has less than 1.0 hit points or flags 0x0D & 7.
- Backstab flag `[0x13A955]` = 0xB337E(target, attacker, weapon).
- To-hit 0x2A7F3 (ai.md, character.md section 11). Result <= 0: no damage.
- 0x9305D(target, 1); target +0x15E |= 0x80.
- Creature with `weapon_resistant` (+0x99 bit 0x40): no damage unless the
  weapon has the flag `magic` (0x10).
- Damage = 0x2AF5E(target, attacker, weapon, 5). Critical hit (roll result
  2): a second 0x2AF5E roll is added (two rolls, not double).
- Target flag 0x8B bit 0x20: decrement +0x80, text, no damage ((guess) a
  protection spell).
- Attacker flag 0x88 bit 0x08 with melee or natural weapon, failed save
  0xAE596(target, 4): `undead` target is held for `(rand % 4 + 1 + level) *
  15` s (bit 0x89 |= 1), else damage += `rand % 4 + 1` ((guess) a touch
  spell).
- Hit points -= damage. Damage > 0: the FLIN rule of ai.md, damage level
  `+0x16E = clamp(10 - hp * 10 / max_hp, 0, 10)`, text with the damage when
  the attacker id equals the player number.
- Below 1.0: kill text "%s %s %s.", experience (below), kill counters
  `P+0x1D33AB` and `P+0x1D33AD` + 1, `die_sound` at 1.0, DIE state, egg
  (ai.md).

Player values (to-hit and damage), from 0x2A7F3 and 0x2AF5E (character.md
section 11, verified again here for the weapon type tests):

- Melee (`attack_type` 1) or thrown (+0x0A == 3): +4 to the roll on a
  backstab; strength to-hit bonus 0x2A789(`P+0x1D33D3`, `P+0x1D33D5`)
  (0x2AB4F to 0x2AB91); strength damage bonus table 0x13A9A2 (0x2B475).
- Projectile not thrown: the to-hit bonus of the launcher: the bow
  (`P+0x1D4DD6`) for `arrow`, else the crossbow (`P+0x1D4DD8`); its misc
  item bonus at 0x19BAC6 + 0x44 * id (0x2AB0A to 0x2AB4D).
- Weapon `to_hit_bonus` (+0x36) is added (0x2AAE7).

### Experience (verified)

On a kill, if the attacker id byte equals the player number `[0x13B924]`
(0x2A0C5) and the creature flag 0x8B bit 0x01 is clear: 0x4EA50(`score_value`
= `Robot_info+0x0C`) -> 0xAF3CF (character.md section 10). (Note: the test
compares only the id byte. A missile passes its parent as attacker, so a
player missile kill also gives experience.)

## A4. Sounds (verified)

| When | Sound | Volume | Where |
|---|---|---|---|
| Swing | none found | | no sound call in 0x2FB10, 0x44300, 0x4450C, 0x9DC66, 0x6CFF8 |
| Melee contact with a creature (before the roll) | weapon `monster_hit_sound` | 0.5 | linked to the creature, 0x27AF5 |
| same | creature `hit_sound` (+0x9A) | 0.5 | linked, 0x27B38 |
| Melee hit, alive | creature `hit_sound`; weapon `monster_hit_sound` | 0.5; 0.5 | creature; hit point |
| Melee kill | creature `die_sound` (+0x9C) 1.0 (0x27C4B) and again 1.0 in 0x29A9C; weapon `monster_kill_sound` 0.5 | | |
| Melee miss | weapon `monster_miss_sound` | 0.5 | hit point |
| Missile fired | weapon `flash_sound` | 1.0 | 0x2D955 when the parent is the viewer, else at the missile |
| Missile hits creature, damage, alive above 2.0 | creature `hit_sound` 1.0, or weapon `monster_hit_sound` 0.25 if the creature has none | | 0x275DF, 0x27641 |
| Missile, creature at 2.0 or less | creature `die_sound` 1.0, only below 1.0; if the creature has no `die_sound` (0): weapon `monster_kill_sound` 0.25 and creature goal state 9, both only if that sound is set | | 0x27650 to 0x27709 |
| Missile miss | weapon `monster_miss_sound` 0.25 | | 0x277F9 |
| Missile hits wall | `wall_hit_sound` 0.5 at the hit point and 0.5 at the object (special sounds 0x8AC.. and 0x8B0.. on side types 3 and 1); alert level 2 | | 0x26C2F |

`armor_hit_sound` is read only at 0x28573 and 0x86D27 (creature hits the
player, not part of this task). The tables of BANE.BIN set no
`monster_miss_sound`, `monster_kill_sound`, `monster_miss_vclip`, `POV_clip`
or `reach` on any weapon line (decoded table, grep), so those sounds are
silent in practice (the parser default was not checked). Of the 85 decoded
tables only DEMO.BIN and MOIN1.BIN set `monster_kill_sound` (12). **V**

The impact sound of the killing blow (verified, 0x27CCF to 0x27D08): the sound
at the hit point is `monster_hit_sound` (W+0x24) if the hit points are 1.0 or
more after the damage, else `monster_kill_sound` (W+0x28); a value of -1 or less
plays nothing. So a melee kill has no impact sound at the hit point: only the
contact sound before the roll (linked to the creature, 0.5) and the two
`die_sound` calls (0.5 after the limit of 0x2E631 and 0x2E957). On a normal
hit the impact sound plays two times (contact and hit point). A missile kill
(creature at less than 1.0) plays no weapon sound at all. In DTUGAME.EXE
0x2E631 keeps a one-shot sound in a sound object linked to the object (Descent
1 played it at once); the sync 0x2EC66 stops a sound of a dead object only if
it loops, and 0x2E816 (kill the sounds of an object) runs at 0x6A3CC, the end
of the DIE animation, not in the frame of the kill. So no code stops the
contact sound; the defect is the choice of the sound. **V**

Port (always on, no setting): the melee hit point plays `monster_hit_sound` on
each hit that does damage, and `monster_kill_sound` as well on a kill if a table
sets it (dtucombat.c `player_hits_creature`); a missile that kills plays the
weapon `monster_hit_sound` at 0.25 at the hit point (dtuobj.c). The die sound is
not changed. See bugs.md, D10.

### Damage function 0x29A9C for the player target: touch spell and alerts (verified, 2026-11-22)

- 0x2A637 to 0x2A67B: when the damage of the attack is not 0 (after the
  hit text and the screen flash), a creature type with `Robot_info+0x7C` >=
  0 casts that spell on the player: `spell_apply_effect` 0x8CFFF(spell,
  player, creature). For spell 22 and 52 the code first sets the byte
  `Spell_info+0x49` (0x1B56BF + 127 * spell) to 1. Then the death test of
  the player (0x2A680). Only the creature setup sets +0x7C (22, Hold Person,
  for the lich and the CLAN3 type 10), so only the lich has a touch. The
  port: `damage_player_w` (dtuobj.c); the save and the duration are those of
  the spell (`dtu_spell_cast_on`).
- The melee hit of the player (0x278B5) calls the alert function
  0x160AB(player, 3) after the contact sounds (0x27B3F), and a missile of
  the player calls 0x160AB(creature, 3) (0x274B7). See ai.md, "Alert
  function". The port: `dtu_creature_alert`.

### Feedback of a miss and of a swing at nothing

From the facts above (A3, A4, 0x278B5, 0x29A9C):

- A swing that touches nothing: no sound, no text, no vclip. The swing plays to
  its end.
- A swing that touches a creature: the swing ends at once on a contact at rel 6
  or 7 (A3 step 7). Before the to-hit roll the weapon `monster_hit_sound` and the
  creature `hit_sound` play (linked to the creature, 0.5).
- A missed roll: then only `monster_miss_vclip` and `monster_miss_sound`, which no
  weapon line of the shipped tables sets, so a miss adds nothing to the contact.
  0x29A9C prints its text only for damage (hit points went down) or a kill.
- A swing into a wall (not the floor or the ceiling): the swing ends at once as for
  a creature, with no sound, and the trigger touch 0x95019 runs.

So a miss and a swing at the air differ only by the contact sounds (which some
creature and weapon pairs do not have) and the shorter swing. The port has all of
these (`player_hits_creature`, `swing_hit_test` of dtucombat.c). The target hint
(C1) is an addition, not a restored behavior.

### Miss sound (port improvement)

1.3 has no sound for a melee attack that hits nothing (above). The tables do
have a sound for each melee weapon in `flash_sound` (W+0x20): 2400 (`cmbt01a`) for
the blades and axes, 2420 (`cmbt06a`) for the maces and hammers, 2435 (no `$SOUND`
line in BANE.BIN) for the clubs. The code reads `flash_sound` only for a missile
(0x55511, 0x934A8, 0x93294), the spell missiles (0x56FF6, 0x946FA) and the paging
of the weapon in the hand (0x6AB61), so a swing never plays it; a thrown dagger or
axe plays 2400 when it flies. **V** The three sounds of that group (2400, 2420 and
2436 `cmbt11b`) are 0.67 to 0.83 s long with a slow start (60 to 100 ms to the
peak, low pitch); the hit sounds start at once. Their use as a swing sound is a
guess (**G**).

The port setting "Miss sound" (`Dtu_miss_sound`, `dtumiss.c`, on by default)
plays that swing sound, the `flash_sound` of the weapon, through the sample path
of the game (`digi_play_sample_handle` of `digi.c`). The clubs have 2435, which
has no sound: the port plays 2436 (`cmbt11b`, the next sound of that group) in
its place. An empty hand, a weapon with no `flash_sound` and a creature with no
weapon line play 2400.

- A swing of the player that touches nothing: one sound that starts on the first
  frame of the hit test with no contact (relative frame 6 of n = 16, reached at
  5.5 / 16 of the swing time: 0.34 s into a swing of 1.0 s, 0.17 s with Haste),
  or when the frames of the hit test are over or at the end of the swing if
  those frames were skipped. If the swing then touches a creature, scenery or a
  wall, the sound stops at once (`digi_stop_handle`, a short fade of the mixer);
  a swing that touches something on its first test frame has none. The timing
  depends only on the frames of the swing, not on the sound.
- A missed attack roll of the player: the miss sound plays in place of the two
  contact sounds (weapon `monster_hit_sound`, creature `hit_sound`), which then
  play only for a hit. In 1.3 a missed roll sounds as a hit.
- A missed melee roll of a creature on the player (0x2A24A, `damage_player_w`):
  the swing sound of the weapon of the creature at its place, at volume 0.25,
  distance 200. A new one does not start while the last one of a creature plays.
- Not for a missile or a thrown weapon, and not with a hit. The player sound is
  at volume 0.5, as the contact sounds. A new sound of the player stops the last
  one (the sounds are longer than a swing of Haste).

`-origsound`, `-nomisssound` and the setting off give the 1.3 sounds.
`tools/check_attack_rate.py` checks the rule, the sound number and the start time.

## A5. Missile and thrown weapons

### When the player fires (verified, 0x56A0D, called by 0x44300 each frame while the request is pending)

1. Return 0 if `[0x13D3F7]` is not 0.
2. Item = hand item `P+0x1D4DF3`. A weapon with `PC_spell` != -1 is handled as
   a spell (wand).
3. Not a spell: the state must be 6, 7, 11 or 12 and the frame at or after
   the fire frame of that state (`fire_frames[0..3]` at model +0xB8..+0xBE).
   Else return 0 and keep the request. HMC.BIN `fire_frames=45 61 109 125`:
   frame 45 is `rel` 1 of state 6, so the missile leaves at 1/16 of the
   animation.
4. Projectile weapon: if `ammo_usage` (+0x07) < `ammo_initial` (+0x1C, signed)
   the text "Oops.  Out of arrows!" (flag 4), "...bolts!" (flag 8) or
   "...ammo!" and no shot. With the table values (`ammo_usage` 1,
   `ammo_initial` -1 or 1) this only stops lines with `ammo_initial=8`
   (verified compare; meaning (guess)).
5. Fire timer per hand `h` (D1 style): if `last[h] + 2 * FrameTime < GameTime`
   then `next[h] = GameTime`; `last[h] = GameTime` (0x215D74, 0x215D7C).
   While `next[h] <= GameTime`: `next[h] += fire_wait`, fire one missile
   0x56EB0(player, weapon, 0, count 1, h) -> 0x56516 -> 0x564F7 -> 0x561DA.
6. If at least one was fired: use one item 0xB7084(hand item), clear the
   request, player goal state = 10. Return 1.

Rate: `next` is reset to now whenever 0x56A0D was not called during the last
two frames, which is the case for every new key press (the request is cleared
after a shot). So `fire_wait` limits only shots of one held request. Between
presses the limit is only "state 6 at or after the fire frame". A second
press while the same state 6 is still after its fire frame fires again at
once (verified code reading; not tested).

### Missile creation 0x561DA -> Laser_create_new 0x55511 (verified)

- Start point: player position + (0, 2.0, 0) (0x562BB). Not a gun point and
  not the eye (the eye is 3.0 higher with `eye_offset` 5.0).
- Direction: `rotate((0, 0, d), angles_2_matrix(look_pitch, 0, 0) * orient)`
  with d = 1.0; d = 3.0 if `enemy_spell` is 0x20, 2.0 if it is 0x10 or 0x11.
  So the aim follows the look pitch.
- 0x55511(dir, pos, segment, parent, weapon, make sound): object type 5;
  render type from +0x00 (2 `weapon_pof` -> polygon object 1, radius from the
  model / `lw_ratio`; 1 blob and 3 vclip -> radius `blob_size`); mass, drag,
  gravity copied to +0x114, +0x118, +0x11C; physics flag 4 for `bounce`, 0x10
  for `stick` and for weapon id 1, 0x80 for `thrown`; hit points = those of
  the parent; lifetime +0xA8 (infinite lifetime of a polygon object that is
  not thrown becomes 30.0); `flash_vclip` explosion of `flash_size`;
  `flash_sound`; velocity = dir * `speed[Difficulty]`, halved and copied to
  the thrust if `thrust` != 0; weapon id 1 or physics flag 0x10 (all `stick`
  weapons, the arrows and bolts): lifetime += `rand * 4 - 1.0` (plus or minus
  1 s).
- `enemy_spell` 6: random velocity change of `(rand % 5 - 2) * 16.0` on each
  axis of the thrust. `homing`: tracking target (0x55D91).
- BANE.BIN: Arrow speed 80 (all difficulties), Bolt 80, Throwing Axe, Dart,
  Throwing Dagger, Throwing Hammer `speed=30 40 50 60 80`, `gravity` 0 on all.

### Flight: pweapon.c 0xA03A8 (verified)

- Moves with `velocity * FrameTime` in one `find_vector_intersection` (flags
  5, ignore list 0x30F10C), up to 3 tries. It does not read +0x11C (gravity)
  or apply drag: weapon objects fly straight in 1.3.
- Result wall (1 or 5): 0xA0731; then trigger touch 0x95019(segment, side, 4)
  unless flag 0x40 or byte 0x1AE bit 2. Object (2): 0x6CDC5. 4: 0x29455
  (`collide_two_objects`).

### On hit

- Wall: 0xA0731 restores the position, calls `collide_object_with_wall`
  0x29802 -> 0x26C2F, which plays the wall sounds and queues the weapon for
  deletion (0x6A3CC) unless `enemy_spell` is 0x20. Then bounce reverses the
  velocity (all three axes negated, no reflection), object flag 0x10 makes it
  stick (`add_stuck_object` 0x7EBDD), else it is removed. No instruction sets
  object flag 0x10 on an object by direct address (checked `or [reg+0xd],
  0x10` and `[reg+0x2B0621]`).
- Creature: `collide_robot_and_weapon` 0x272DD (table at 0x29355, key 0x401):
  return if the weapon has zero velocity, flag 0x40, or byte 0x1AE bit 2;
  creature active; `enemy_spell` cast; `combustion` explosion and delete if
  byte 0x1AE bit 0; stick, persistent or thrown (0xB0) hit a creature only
  once (+0x14C); alert or 0xBA3B1; unless flag 0x20: 0x29A9C(creature,
  parent, 0, weapon id) and push 0x25DE6. Then the sounds of section 4.
  Stick: stop at the hit point, gravity 1.0, flags 0x44. Thrown: velocity
  reversed, y = 0, normalized (speed 1.0), gravity 1.0, flag 0x40. Else hit
  vclip and 0x2709A. Finally a polygon object (render type 1) is always
  queued for deletion (0x27883).

### Can a thrown weapon be picked up? (verified for the paths read)

No. A polygon-model weapon object is deleted when it hits a wall (0x26C2F)
and when it hits a creature (0x27883). No path turns it back into an item.
The "Skipping" branch of 0xA03A8 (flag 0x80 and position unchanged since the
last frame) only clears the parent fields.

### Ammunition (verified)

- The missile item is the item in the hand: the "Arrow", "Bolt", "Dart" ...
  stack (`stack_limit` 100 or 30 to 50). The bow or crossbow is equipped in
  `P+0x1D4DD6` / `P+0x1D4DD8` and only gives the to-hit bonus and the check of
  0x41A4D.
- 0xB7084 ("Decrement list %d n %d uses %d"): the stack count (+8) goes down
  by 1 per shot (not by `ammo_usage`); -1 is endless; at 0 or less the entry
  is removed (0xB6F01).
- End of the attack state (0x4450C, 0x44653 to 0x446DF): weapon in the hand
  with +0x0A == 3 (thrown) and count <= 0: 0x43A5A(-3) (empty hand model,
  letter `_`), then 0x4448D "Autoarming anything": the first non-empty hot
  key slot (`P+0x1D4DDB`, 10 slots) is armed. The -4 branch needs
  `[0x13B62A] == 0`, which never happens.

## A6. pweapon.c

The string 0x130626 is used only in 0xA03A8. The function before it
(0x9FE68) uses "pmon.c", the one after it (0xA0802) "dtuio.c". pweapon.c holds:

| Address | Role |
|---|---|
| 0xA03A8 | Move a weapon object (type 5, from `object_move_one` 0x697F7): asserts `!(PF_USES_THRUST) || drag != 0`, velocity * FrameTime, up to 3 query tries, dispatch of the result. No gravity, no drag. |
| 0xA0731 | Weapon hits a wall: restore position, `collide_object_with_wall`, then bounce (negate velocity) or stick (object flag 0x10) or delete. |

Nothing in pweapon.c is about the player's melee weapon.

## A7. Rules that count game frames

1. The melee test 0x6CFF8 runs once per game frame. At `rel` = n/2 (frame 8)
   it rolls once per game frame (section 3). Number of hit rolls depends on
   the frame rate.
2. The anim shift `(2*rel - n) * n / t` mixes frames with a rate (frames per
   second), not a time; at rel 8 it is 0.
3. Byte 0x1AF counts down 1 per game frame (0x4450C), 8 to 1.
4. The forward offset of the melee test (`sqrt(2)`) and of the arms (look
   pitch) are per frame positions, not speeds: no frame rate effect.
5. Missile fire timer reset: "not called during the last 2 * FrameTime"
   (0x56BB0): depends on the frame time.
6. The attack edge detection (0x13AD7E) is per frame: a press and release
   inside one frame is lost (input sampled per frame).
7. View wobble when player byte 0x1AE bit 0x10 is set ((guess) drunk or
   confused): counters 0x13D9C4 (-4..4) and 0x13D9CC (-3..2) step by 1 per
   rendered frame (the 0x13D9D4 time step is GameTime + 0x14, about every
   frame) and are added to two columns of the view matrix (0x76D4C to
   0x76E2A).
8. The texture pulse of `draw_polygon_model` counts draw calls (models.md).
9. Not frame based: the swing length and the melee gate (time), the view bob
   (time), the missile `fire_wait` accumulator (time).

## A8. Attack timing (verified)

All inputs of the time between two attacks of the player. The port has these
rules in `engine/main/dtuatk.c`; `test_dtuatk` (ctest) checks them.

### What the executable reads

`fire_wait` (Weapon_info+0x64, 0x1E9D6C) has three readers in the whole
image: the `$WEAPON` reader (0x21648; default 0.25 from 0x207E7; 0x21660 to
0x21682 raise a value below 0xCCD, 0.05, to 0.05), the weapon change (0x4421B)
and the fire timer (0x56C01). The time of the last melee attack (0x13B74B)
has three references, all in the gate (0x41E8D, 0x41EB9, 0x41ED7). No code
reads the level or the class of the character, the weapon specialization or a
number of attacks for the attack rate: a fighter of level 12 attacks as fast
as one of level 1. There is no second weapon and no off-hand attack: the
states 7, 11 and 12 are never chosen (A1).

### Melee weapon and empty hand

1. Weapon change 0x43A5A (0x441E9 to 0x44258), only for item type 5 with
   `attack_type` 1: `t = fire_wait`; `t = 1.0` if `t < 0x10001` (0x44221); `t
   = 2 t` with Slow (effect bit 27). `t` goes to `state_times[6]`, `[7]`,
   `[11]`, `[12]` of the player model of the weapon letter.
2. The swing (state 6) lasts `t` (0x4450C). The gate 0x41E36 accepts an
   attack when `|GameTime - last| > t` and the key is still down, then `last =
   GameTime`.
3. Haste (effect bit 13, 0x41EC3 to 0x41ED7): `last += t / 2` (`sar eax,1`,
   `add [0x13B74B],eax`). The next attack is accepted 1.5 `t` after this one:
   Haste makes the attacks slower. The swing time does not change. A defect.
4. The model keeps `t`: Slow that ends does not shorten it. It changes with
   the next weapon change, and 0x43A5A returns early during the states 6 and
   11.
5. The Slow spell on the player (0x8FB6A to 0x8FBA3) doubles `state_times[6]`
   of the current model at the cast, whatever the item in the hand is. Nothing
   halves it at the end (0x9002C has no access to +0xFC).

`fire_wait` of the melee lines of BANE.BIN: 0.5 (Dagger +2, Dagger of
Impaling, Short Sword of Quickness) to 1.5 (Warhammer, Dwarven Sword, Dwarven
Scimitar). With step 1 every value below 1.0 is 1.0: Dagger 0.6, Short Sword
0.8, Scimitar 0.9, Long Sword +1 0.9 and +2 0.8 swing as fast as the Long
Sword 1.0. Only the slower weapons differ: Broad Sword 1.1, Mace 1.2, Bastard
Sword and Hand Axe 1.3, Warhammer 1.5. The lower limit is needed by the
animation code: 0x44589 computes the frame of a state only when the integer
part of its time is not 0.

### Missile and thrown weapons

1. 0x43A5A does not write the state times (`attack_type` 0): the attack state
   lasts `state_times[6]` of the model, 1.0 in the `$PLAYER` line
   (`state_times=.5 2 .5 .5 1 1 ...`; the defaults at 0x13DA08 are the same).
   The missile leaves at the fire frame (`fire_frames=56 72 116 132`: frame 12
   of 16 of state 6, 0.72 s after its start).
2. The fire timer (A5, 0x56BB0 to 0x56C07): `next[h] += fire_wait` for each
   shot, but `next[h] = GameTime` when 0x56A0D was not called in the last two
   frames. The request ends with the shot and a request needs a new key
   press, so the timer starts again for each press. `fire_wait` (Arrow 2.0,
   Arrows +1 1.5, Arrows +2 1.0, Bolt 3.0, Bolt +1 2.0, Bolt +2 5.0, Dart 0.5,
   Throwing Dagger 0.8, Throwing Hammer 1.2, Throwing Axe 1.5) never limits
   the shots. A defect: each press in state 6 after the fire frame fires. In
   the port with `-origcombat` two arrows leave 0.05 s apart.
3. Haste and Slow: no code for the missile rate. The bow or the crossbow of
   the slot is not read for the rate (references to the slots P+0x1D4DD6 and
   P+0x1D4DD8: the inventory screen 0x30FD0 to 0x35FC2, the gate 0x41DA4 and
   0x41DF9, the model 0x43C16 and 0x43C49, 0x5459A and 0x545C9).

### Items and spells that name speed

| Item or spell | Table line | Effect in 1.3 |
|---|---|---|
| Haste (spell 21), Potion of Speed (`$OBJECT` 87) | | effect bit 13: move speed times 2 (0x9DE13), melee attacks 1.5 times slower (0x41EC3) |
| Ring of Quickness (`$ARMOR` 25), Boots of Speed (37) | `pc_spell=21` | item bit 9 (0xAFB55, by item number): Haste while worn |
| Sword of W'hin (`$WEAPON` 54) | | item bit 9 in the hand |
| Assembled Spider | | Haste by USE (B6) |
| Slow (spell 47) | | effect bit 27: swing time times 2 (0x44240, 0x8FBA3), move speed / 2 |
| Short Bow of Speed (`$OBJECT` 100) | `pc_spell1=21`, `to_hit_bonus=2` | none. The image has no keyword `pc_spell1` (the keywords are `PC_spell` and `PC_spell_2`, 0x1121AD and 0x112742) and 0xAFB55 does not read the bow slot. The bow is a Short Bow +2 |
| Short Sword of Quickness (`$WEAPON` 69) | `pc_spell=21`, `fire_wait=0.5` | none. `PC_spell` (+0x3A) is read only in 0x56A0D, and 0x44300 calls it only for a spell or a weapon with `attack_type` 0 (0x44423). `fire_wait` 0.5 becomes 1.0. The sword is a Short Sword +2 |

These are all the lines with `pc_spell=21` or `pc_spell1=` in the 46 level
tables (checked with a script over `gamedata/unpacked/*.BIN`).

### Creatures

No attack timer (ai.md, section 1 point 6): the attack animation paces the
attacks, `state_times=` of the `$MONSTER` line. Slow and Haste on a creature
set the effect bit only: 0x8FB6A doubles the state time only for the player
object.

### The port

| Rule | 1.3 (`-origcombat`) | Port (default) | Setting |
|---|---|---|---|
| Melee swing and gate | `max(fire_wait, 1.0)`, Slow times 2 | the same | |
| Haste, melee | gate 1.5 `t`, swing `t` | gate and swing `t / 2`, the swing is animated below 1.0 | `Dtu_fix_haste_attack` |
| Missile, thrown: attack state | time of the model (1.0), Slow times 2 | the same | |
| Missile, thrown: rate | none | `fire_wait` from shot to shot, Slow times 2, Haste / 2. A press is accepted when its fire frame comes at or after the end of the wait, else it is consumed | `Dtu_fix_missile_rate` |
| Short Bow of Speed in the bow slot | none | its arrows twice as fast: the wait / 2 (1.0 s for an Arrow instead of 2.0). No Haste effect | `Dtu_fix_speed_items` |
| Short Sword of Quickness in the hand | none | its swings twice as fast: swing and gate 0.5 s, animated. No Haste effect | `Dtu_fix_speed_items` |
| Speed item with Haste | | twice as fast, not four times | `Dtu_fix_speed_items` |
| Speed item with Slow | | the normal rate: they cancel, as Haste and Slow do | `Dtu_fix_speed_items` |
| Level and class | not read | not read | |

Kept from 1.3 as intended code: the lower limit 1.0 of the swing time, so a
fast weapon is not faster than a Long Sword (a Dagger swings in 1.0 s, as its
description says it is fast).

The speed items (`Dtu_fix_speed_items`, decision of 2026-11-23). The power is
an attack rate bonus of the item only: the bow counts while it is in the bow
slot and the arrow is in the hand, the sword while it is in the hand. It sets
no effect bit: no move speed times 2, no Haste in the effect list, no change
to other weapons, spells or casting. For the rate the item counts as one more
source of Haste (dtuatk.c `dtu_atk_rate_effects`): the Haste bit is one bit,
so Haste from a spell, a potion, the Ring of Quickness or the Boots of Speed
adds nothing to it. Slow and the item cancel, as Slow and Haste do in the
rate rules (two times, then half). The item halves the time also when
`Dtu_fix_haste_attack` is off, unless Haste is on (then the 1.3 rule of the
spell stays). AD&D 2nd edition gives a fighter
3/2 attacks at level 7 and 2 at level 13, and a short bow 2 shots in a round;
1.3 has no rounds for the player and the port adds none.

Not ported: the state time that stays doubled after Slow ends (step 4 and 5
above). The port computes the time from the effect bits in each frame outside
of an attack state.

Corrected errors of the port (found with this section):

- With Haste the halved swing time (0.5 for most weapons) was below the 1.0
  that the animation needs: the swing stayed on its first frame and never
  ended, so a player with Haste, the Ring of Quickness or the Boots of Speed
  could not hit at all.
- The Short Sword of Quickness was taken for a wand (a weapon with
  `pc_spell`): the first attack cast Haste at level 1 and used the sword up.
- The attack state of a missile weapon lasted `max(fire_wait, 1.0)` (2.0 for
  an arrow, 3.0 for a bolt) instead of the time of the model.

Measured with tools/check_attack_rate.py (YPINN, 30 frames a second, a key
press every 2 or 3 frames; time between two accepted melee attacks or two
arrows, the steps of the presses included): Long Sword 1.07, Dagger 1.07,
Short Sword of Quickness 0.53 (swing 0.50), with Haste 0.53, with Slow 1.07,
Long Sword with Haste 0.53, Dagger with Haste 0.53, Long Sword with Slow 2.10;
Short Bow 2.10, Short Bow of Speed 1.10, with Haste 1.10, Short Bow with Haste
1.10, Short Bow of Speed with Slow 2.10. The walk of 30 frames is the same
with the empty hand, the sword in the hand and the bow in the bow slot, two
times as far with the Ring of Quickness. With `-origcombat`: Short Sword of
Quickness 1.07, with Haste 1.60.

Measured before the speed item rule of 2026-11-23, in BANE (`-noattack`, a key press every 0.2 s, `-objlog`; time
between two swing starts or two shots): Long Sword 1.2, Dagger 1.2, Warhammer
1.6, Long Sword with Haste 0.6, with Slow 2.2, with Haste and `-origcombat`
1.6, fighter with 900000 experience 1.2; Short Bow with Arrow 2.2, Short Bow
of Speed 1.2, Short Bow with Haste 1.2, with Slow 4.2, Crossbow with Bolt 3.2,
Dart 1.0 (the attack state), Short Bow of Speed with `-origcombat` two arrows
0.05 apart in each attack state.

# Part B. Spells, usable items, saving throws, thief skills

## B0. Spell_info (127 bytes, base 0x1B5676, index * 127), reader 0x242F6

Keyword to offset, Code (stores in 0x242F6 to 0x25550). Offsets are relative to
0x1B5676 as in objects.md. `damage_bonus` is stored at -2 (0x1B5674 + n*127), so the
real start of the record is probably 0x1B5674 (guess).

| Off | Type | Keyword | Default |
|---|---|---|---|
| -0x02 | s16 | `damage_bonus` | 0 |
| 0x00 | u8 | class: `cleric_use` 1, `mage_use` 2 | 0 |
| 0x02 | s16 | `icon` | 0 |
| 0x04 | s16 | `cast_time`; replaced after the read by the words of 0x13A8DB (fixup 0x1A344, spells.md section 1) | 0 |
| 0x06 | s16 | `range` | 0 |
| 0x08 | s16 | `radius` | 0 |
| 0x0A | s16 | `level_range` | 0 |
| 0x0C | s16 | `level_radius` | 0 |
| 0x0E | s16 | `cast_sound` | 10 |
| 0x10..0x18 | s16 | `monster_hit_sound`, `monster_miss_sound`, `monster_kill_sound`, `wall_hit_sound`, `armor_hit_sound` | -1 |
| 0x1A..0x22 | s16 | `cast_vclip`, `monster_hit_vclip`, `monster_miss_vclip`, `wall_hit_vclip`, `armor_hit_vclip` | -1 |
| 0x24 | s16 | bitmap of `picture` | -1 |
| 0x26 | u8 | `plot_item` | 0 |
| 0x27 | u8 | `sub_use_count` | 1 |
| 0x28 | s16 | `uses` | 1 |
| 0x2A | s16 | `uses_per_level` | 0 |
| 0x2C | s16 | `levels_per_use` | 0 |
| 0x2E | s16 | `to_hit_bonus` | 0 |
| 0x30 | s16 | weapon model (pof) number; `weapon_vclip` sets -1 | -1 |
| 0x32 | s16 | `weapon_vclip` | -1 |
| 0x34 | s16 | ? | -1 |
| 0x36 | u8 | flags: 1 `area` (`type=AREA`; `target` clears it), 2 `stackable`, bits 2..4 cast with: 0 `cast_with_hands`, 4 `cast_with_wand`, 8 `cast_with_scroll`, 0x10 `cast_with_arrow`; 0x20 `potion`, 0x40 `always`, 0x80 `homing` | 0 |
| 0x37 | u8 | flags: 1 `self_cast_ok`, 2 `non_hotkey_item`, `cast_in_front` sets 5 (bits 0 and 2) | 0 |
| 0x38 | u8 | missile render type: 2 `weapon_pof`, 3 `weapon_vclip` | 0xFF |
| 0x39 | u8 | `level` (spell level) | 1 |
| 0x3A, 0x3B, 0x3C | u8 | `damage_dice`, `#damage_dice` (die count), `damage_level` | 0 |
| 0x3D, 0x3E, 0x3F | u8 | `fire_damage_dice`, `#fire_damage_dice`, `fire_damage_level` | 0 |
| 0x40, 0x41, 0x42 | u8 | cold: same three | 0 |
| 0x43, 0x44, 0x45 | u8 | acid: same three | 0 |
| 0x46, 0x47, 0x48 | u8 | elec: same three | 0 |
| 0x4E | fix | `duration` (negative becomes 0x3FFFFFFF) | 30.0 |
| 0x52 | fix | `level_duration` | 0 |
| 0x56 | fix | `size` (integer << 16) | 3.0 |
| 0x5A | fix | `player_light` | 0 |
| 0x5E | fix | `modifier` | 1.0 |
| 0x62 | fix | `cast_size` | 0 |
| 0x66 | fix | `weapon_vclip_size` | 1.0 |
| 0x6A | fix | `damage_percent` (value * a constant at 0x122B06 * 65536) | 0x1999 (0.1) |
| 0x6E | s16 | `combustion` | -1 |
| 0x70 | s16 | `value` | 10000 |
| 0x72 | s16 | `inv_icon` | 0 |
| 0x74 | s16 | `vclip_trail` | -1 |
| 0x76 | u8 | flags: 2 set by `cast_on_top` | 0xFF |
| 0x77 | fix | `speed` (integer << 16) | 100.0 |
| 0x7B | s16 | `cast_on_top` (vclip) | -1 |

Notes: `range`, `radius`, `cast_time` are integers. In the text, `#damage_dice=1`
means the die count, `damage_dice=4` the die size: Magic Missile is 1d4+1 (guess
from the AD&D rule; the code check is in section 3).

Note: code that walks Spell_info with a pointer uses the base 0x1B5674 (0x56D43,
0x570BE, 0x920FD, 0x923BC, 0x92ED0, 0x93199). So the record starts at 0x1B5674 and
the offsets of section 0 are 2 less than the record offsets. This document keeps
the objects.md convention (base 0x1B5676) unless it says "record offset".

## B1. Casting by the player

### Spell lists, slots and rest (Code)

- The two spell lists (P+0x8F3 mage, P+0x10E3 cleric, counts P+0xF7, P+0xFB; P =
  0x1D331C) are the spell book: picking up a spell object (type 17) calls 0xB69AD,
  which adds the spell to the mage list if the spell has the mage bit and the
  character is a mage (0x35A2B), else to the cleric list if it has the cleric bit
  and the character is a cleric (0x35A09). A spell already in the list is refused
  (result 2). The 16 byte entry: byte 0 = 17, byte 1 = spell number, byte 3 =
  class bits, byte 4 = Spell_info flags (+0x36), byte 5 letter: `K` (hands), `Q`
  (wand, flag 4), `N` (scroll, flag 8), `B` (arrow, flag 0x10), word 6 = spell
  level (read by 0x35573), byte 0x0B = Spell_info `sub_use_count` (+0x27, default
  1). A spell that no list takes stays in the item list (character.md insert rule)
  and is cast as a "scroll" (see below).
- Memorization is the AD&D slot count per spell level, not a list of memorized
  copies:
  - P+0x23 .. P+0x2B: mage slots per spell level 1..9 (maximum); P+0x2C .. P+0x34:
    mage slots left.
  - P+0x35 .. P+0x3B: cleric slots per level 1..7 (maximum); P+0x3C .. P+0x42:
    cleric slots left.
  - 0x3575B(P, class level, ...) (displays.c, called at level up 0x8BFAC):
    mage maximum = table 0x145CD8[mage_level * 9 + i] (18 rows of 9 bytes, the AD&D
    wizard table: 1 / 2 / 2,1 / 3,2 / 4,2,1 / 4,2,2 / 4,3,2,1 / 4,3,3,2 /
    4,3,3,2,1 / 4,4,3,2,2 / 4,4,4,3,3 / 4,4,4,4,4,1 / 5,5,5,4,4,2 / 5,5,5,4,4,2,1 /
    5,5,5,5,5,2,1 / 5,5,5,5,5,3,2,1 / 5,5,5,5,5,3,3,2 / 5,5,5,5,5,3,3,2,1). No
    intelligence bonus. Cleric maximum = table 0x145DF8[cleric_level * 7 + i] (the
    AD&D priest table: 1 / 2 / 2,1 / 3,2 / 3,3,1 / 3,3,2 / 3,3,2,1 / 3,3,3,2 /
    4,4,3,2,1 / 4,4,3,3,2 / 5,4,4,3,2,1 / 6,5,5,3,2,2 / 6,6,6,4,2,2 /
    6,6,6,5,3,2,1 / 6,6,6,6,4,2,1 / 7,7,7,6,4,3,1 / 7,7,7,7,5,3,2 /
    8,8,8,8,6,4,2) plus, for spell levels 1 to 4 when the base is > 0, the wisdom
    bonus: int tables at 0x1403D2, 0x140422, 0x140472, 0x1404C2 (pointer table
    0x140512) indexed by wisdom 0..19:
    level 1: 0 for 0..12, 1 (13), 2 (14..18), 3 (19); level 2: 1 (15), 2 (16..19);
    level 3: 1 (17, 18), 2 (19); level 4: 1 (18, 19). Asserts "PA.wisdom > 0",
    "PA.wisdom <= 19". Slots left are cut to the new maximum.
  - 0x3551B restores all slots left = maximum. Callers: the rest action 0x360D5
    (via 0x36049), the game start after Waterdeep 0x42284 (only with the flag
    0x141C38 and level 1 or below), the new character 0x8BEBC (0x8BFFE). Not a
    level change, a load or a new class level: spells.md section 13.
- Rest: button R (main screen) action 0x360D5: 0x3E137(segment of the player)
  tests whether the segment is a rest stop (the Descent fuel center code,
  fuelcen.c). No: "You can not rest here". Yes: "You feel refreshed" (HUD, or a
  message box 0x655FE when 0x13B616 is set), slots restored (0x3551B), then
  0x3E1F6 (fuelcen.c "Rest Stop Reenergization": calls 0x160AB(player, 5) and
  0x92FF2 ("randomizing spell table", recharges creature spells)).
- Rary's Mnemonic Enhancer (spell 41) on the player: a box "Which additional spell
  levels would you like to cast?" with the strings "One 3rd", "One 1st and one
  2nd", "Three 1st" (pushed in this order; 0x655FE returns the index; which index
  belongs to which string was not checked): 0: P+0x23 += 3; 1: P+0x23 += 1 and
  P+0x24 += 1; other: P+0x25 += 1. So 0 is "Three 1st" and 2 "One 3rd" (guess from
  the amounts). Choice saved at P+0x1A54. Only the maximum is raised (the next
  rest fills it). Effect bit 22, no timer set in the cast; 0x9002C (0x90890)
  takes the same amounts back.

### Selecting and casting (Code)

- The spell to cast is the "current item" reference P+0x1AD7 (2 bytes: list 0
  items / 1 mage / 2 cleric, index). Spells are shown as a view of the inventory
  list (ingame-ui.md: "BrowseItemType <= JRG_VIEWING_PRIEST" 0x35ABD) and can sit
  in the hot keys P+0x1AC1. The S button of the main screen opens the statistics
  screen (mode 5, ingame-ui.md), which has the spell tables. A spell goes into
  P+0x1AD7 through a hot key box (0x43A5A): the button SPELLS of the inventory
  screen shows a spell list, the cursor takes a spell, and a click on a box or on
  the figure of the paper doll puts it into a box (character.md, section 6).
- Fire key / button: game.c 0x44300 calls 0x41A4D (may fire?) and then 0x56A0D
  (fire) for a spell (type 17) or a weapon.
- 0x41A4D for a spell from a spell list (game.c):
  1. Cast timer 0x153EBC is 0x3FFFFFFF (no cast running): 0x356E0(spell) checks the
     class and slots (0x35683: mage bit: 0x35A2B and P+0x2C+level-1 > 0; cleric
     bit: 0x35A09 and P+0x3C+level-1 > 0). On failure: sound 3000 (0xBB8) and one
     of "You are not a spell caster." (0x359F4), "You are not a Mage.",
     "You are not a Cleric.", "Spell is of too high a level." (spell level - 1 >
     class level, 0x3598C / 0x359C0), "Out of spell points for this spell level.".
  2. Mage spell with armor worn whose Armor_info+0x10 flag 2 (`MAGIC_OK`) is clear:
     "You cannot cast a spell while wearing %s.". Player object +0x1AE bit 0x80
     (set by Levitate, guess: also by climbing): "You cannot cast a spell while
     climbing.".
  3. Start: 0x153EBC = GameTime + cast time (+0x04, integer seconds, the value of 0x13A8DB, not of the line). Message
     "Casting time: %d minutes ..." (> 60), "Casting time: %d seconds ..." (> 1) or
     "Casting time: 1 second ...". Returns 0.
  4. Later frames: while 0x153EBC > GameTime returns 0. Then 0x153EBC =
     0x3FFFFFFF and the spell fires.
  A spell from the item list (a scroll) skips the cast time and the slot check.
  A cast is ended by 0x9305D(obj, dl) ("Your spell was interrupted!", "Your
  spell was aborted."). Callers (**Code**): 0x29D80 (dl 1, a creature hit by a
  melee weapon), 0x2A45F (dl 1, the player hit by a weapon: any hit with a
  to-hit result above 0, before the damage, also with 0 damage; Invulnerability
  and Stoneskin return before it), 0x913A7 and 0x917D5 (dl 1, spell damage on a
  creature or the player when the save of category 4 fails), 0x54704 (dl 0, the
  jump key 0x546CC when the player does not levitate: the jump does not happen
  on that press if a cast stopped). Player branch: only while 0x153EBC is below
  0x3FFFFFFF and above GameTime; clears 0x13B6A3 and 0x13B69F, animation 8,
  0x153EBC = 0x3FFFFFFF; dl 1: 0xB7084 uses the slot of the spell, "Your spell
  was interrupted!"; dl 0: "Your spell was aborted.", no slot. Creature branch:
  only while +0x1C0 is above GameTime; dl 1 takes the spell level from the mage
  points +0x19E or the cleric points +0x19F; both below 1: both 0 and +0x1A8 =
  GameTime + (9 - Robot_info+0xE2 - +0xE3) minutes; ailp+0x2C = -2 or -1; +0x1C0
  = GameTime.
- 0x44300 sets the player's requested animation +0x176 = 6 or 7 (fire button 1) or
  11 or 12 (fire button 2), by the flag 0x13B6A3, unless the animation +0x16E is
  already 6, 7, 11 or 12. A weapon attack clears bits of P+0xA0 (mask 0xEC) and
  ends Invisibility (bit 17: timer 0xC8 = 0).
- 0x56A0D(hand) (laser.c, fire): spell path (also a weapon with `PC_spell`, a
  wand):
  - Missile template: byte 4 of the entry: flag 4 -> weapon 3, 8 -> 4, 0x10 -> 5,
    else weapon 2 ("LASER: id = %d"). A spell from the item list sets Spell_info
    flag 8 (`cast_with_scroll`, "LASER: scroll spell"), from a spell list clears it.
  - `self_cast_ok` (+0x37 bit 0) spells are always cast on the player himself:
    Haste stores the caster level in the counter +0x49 (record +0x4B), Prayer
    sets the flag word +0x36 to 0x102 (area + self). Then 0x90CEF (area flag) or
    0x8CFFF(spell, player, player), 0x8CAD3(spell, player) (cast light/vclip),
    player +0x176 = 10, P+0x57 = GameTime, 0xB7084 (use up).
  - Other spells: a fire rate loop with the per hand times 0x215D74[hand],
    0x215D7C[hand] and Weapon_info[template].+0x64 (fire wait of weapon 2..5), each
    shot 0x56FF6(player, spell, template, hand); then 0xB7084, +0x176 = 10.
- 0xB7084(ref) (use up one): spell entry: byte 0x0B -= 1; at 0 it is set back to
  `sub_use_count` and 0x35573 takes one slot of the spell level (mage P+0x2C+lvl-1
  or cleric P+0x3C+lvl-1, not below 0). P+0x1AB7 keeps the last spell reference.
  Item list entry: word +8 (charges, -1 none) -= 1, at 0 the entry is removed
  (0xB6F01).

### Missile: 0x56FF6(caster, spell, template weapon 2..5, hand) (Code)

Copies the spell into Weapon_info[template] (record offsets of Spell_info, base
0x1B5674): W+0x3C = spell number (the impact code casts it), sounds +0x10..+0x1A
to W+0x20..0x2C, vclips +0x1C..+0x24 to W+1..5, render type +0x3A to W+0, model
+0x32 to W+0x18, `weapon_vclip` +0x34 to W+0x14, `to_hit_bonus` +0x30 to W+0x36,
`speed` +0x79 to the 5 speeds W+0x78, `weapon_vclip_size` +0x68 to W+0x6C,
`vclip_trail` to W+0xD5, `combustion` to W+0xCF, `player_light` +0x5C to W+0xA0,
dice to W+0x0D..0x10, homing (flag 0x80) to W+0x0B, lifetime W+0xA8 = 30.0, flags
W+0x3E = 0x10. No reader of `range` (+0x06) was found besides debug prints.
Per spell (missile count n, default 1; L = caster level):

| Spell | Rule |
|---|---|
| 4 Burning Hands | n = 6 |
| 6 Color Spray | vclip 175 + rand % 6; n = 1d6 |
| 7 Confusion | n = 1d4 + L |
| 22, 52 Hold Person | counter +0x49 = 1d4 |
| 23 Hold Undead | counter = 1d3, HD pool +0x76 = L |
| 32 Lightning Bolt | lifetime 2.0, W+0x0A = 1 |
| 33 Magic Missile | n = min(L / 2 + 1, 5); homing: finds a target (0x55D91) and fires n homing missiles at it (0x56F3A); for the player homing only if 0x35A2B() (is a mage) |
| 34 Magical Stone | n = 1d3 |
| 36 Melf's Acid Arrow | lifetime = (L / 3 + 1) * 60.0 |
| 37 Minute Meteors | n = min(L, 5) |
| 46 Sleep | HD pool +0x49 = 2 * (rand % 7) + 4 (4..16) |
| 47 Slow | counter +0x49 = L |

Then 0x56EB0(caster, spell, 0, n, hand) creates the n missiles. On impact the
collision code (collide.c, ai.md: 0x27D82 and others) calls 0x8CFFF (target
spell) or 0x90CEF (area spell) with W+0x3C.

## B2. The spell effect function 0x8CFFF (spells.c)

Correction of objects.md: 0x8ED49 is not a function. It is an instruction inside
the Jump case of 0x8CFFF. All effects are set by 0x8CFFF (0x8CFFF to 0x9002B).

### Arguments and globals (Code 0x8CFFF to 0x8D28F)

`apply_spell(int spell /*eax*/, object *target /*edx*/, object *caster /*ebx*/)`.

- Returns at once if spell < 0 or >= `N_spell_types` (0x13A853), if target or
  caster is NULL, or if either is dead (`flags & 7`, object +0x0D).
- Unless Spell_info+0x37 bit 3 is set (no reader sets it), the target must be the
  player (type 4) or a creature (type 2); a creature with control type
  (+0x0A) not 1 prints "SPELLS: not a critter!!!!!" and returns.
- Text buffers for the messages: target name at 0x3A6584 ("You" / "a trap" /
  creature name 0xA7F03 / object name 0xB8C0F), verb 0x3A6514 ("were"/"was");
  caster name 0x3A6548, verbs 0x3A6528 ("are"), 0x3A6500 ("have"/"has").
- Target level 0x3A6544: the player: 0x91EC1(spell); a creature: 0xAE3BB(target).
  For a creature target that is not the caster: 0xA6505(target,1) if its AI mode
  +0x1CC < 2, then 0x12AD9(target,5) (wake it and make it react, guess).
- Caster level 0x3A6540: the player: 0x91EC1(spell); a trap (type 13):
  2 * (max(class1_level, class2_level of the player) + 1) (P+0xAD, P+0xAF are
  read as fix, Code 0x8D23F); a creature: 0xAE3BB(caster).
- 0x3A65C4 = 0x8CEAE(target, caster): 1 if target or caster is the player; else
  1 if the caster segment is in the visible list (0x38C060) and not 0x16636(caster),
  and d20 <= intelligence - 2 (the word P+0xBD, read at 0x8CEF7; correction: not the
  wisdom). 0x16636(obj): 1 when obj+0x147 (set by the two Invisibility spells) is
  set, the player is not blind, and the intelligence is below 13 or the higher class
  level + 1 below 10. When set, the message goes to the HUD (0x347A2). The texts of
  each case are in [spells.md](spells.md).
- `switch (spell)` jump table at 0x8CF1F, 56 entries (0 to 0x37). Default:
  "Invalid spell id = %d".
- Common end 0x8FFE5: if Spell_info `cast_on_top` (+0x7B) >= 0, 0x8CCED(spell,
  target, duration 0x3A653C) attaches that vclip to the target for the duration.
  Then the player: 0xAFB55 (recompute armor class and attributes); a creature:
  0x10ADF. The early return 0x90024 (effect already active, save made, etc.)
  skips this.

### 0x91EC1: caster level of the player for a spell (Code)

Class bytes: P+0x97 class 1, P+0x98 class 2 (-1 none). Level words P+0xAD and
P+0xAF (0 based, read as fix >> 16). If class1 == 0 (cleric) the cleric level is
class1_level, else the mage level is class1_level; the same for class 2 (P+0x98).
Spell class bits (Spell_info +0x00): both cleric and mage: max(cleric, mage) + 1;
cleric only: cleric level + 1; else mage level + 1. So a level 1 character casts
at level 1. A thief/mage casts mage spells with the mage level (class 2).

### Durations

All timers are absolute values of `GameTime` (fix, 0x13CD30): timer = GameTime +
duration. 60.0 is one AD&D round: the messages divide by 60 ("minutes") or 3600
("hours"). L is the caster level 0x3A6540. Durations multiply by 15 << 18 = 60.0 per
unit (Code: the pattern `x*16 - x` then `<< 0x12`).
For the player as the target of Fear, Scare, Hold Person and the stun of Color
Spray the duration is divided by (wisdom / 2): `fixdiv(d, wisdom << 15)` with
wisdom P+0xBB (Code 0x8DAA2, 0x8E338, 0x8E8FA, 0x8F57F).

### Damage: 0x911AA(target, caster, spell, level) and 0x91BC3 (Code)

0x91BC3(caster, target, spell, level) rolls (each die `rand() % sides + 1`):

| Part | count | sides | per level |
|---|---|---|---|
| normal | +0x3B `#damage_dice` | +0x3A `damage_dice` | +0x3C `damage_level` * level |
| fire | +0x3E | +0x3D | +0x3F |
| acid | +0x44 | +0x43 | +0x45 |
| electric | +0x47 | +0x46 | +0x48 |

Cold (+0x40..0x42) is never rolled. `damage_bonus` is not added here. A part is
rolled if its sides or count byte is not 0. 0 sides with count > 0 divides by
zero (`rand() % 0`, plain `idiv dword ptr [esp]` at 0x91C25, 0x91C8E, 0x91CEC,
0x91D4E). The table text of Fireball and Lightning Bolt has `fire_damage_dice=0` /
`elec_damage_dice=0` and Melf's Acid Arrow has no `acid_damage_dice` in all 92
tables, but the table fixup 0x1A344 gives them 6, 6 and 4 sides and 0 per level
(0x1A9C7 `mov byte ptr [0x1b5ea3], 6`, 0x1AAE9 `mov byte ptr [0x1b669c], 6`), so
in 1.3 these three spells never divide by zero: Fireball and Lightning Bolt roll
min(L, 10) d6 (D-101, DTU-Decomp `docs/resolved-2026-11-23.md`). The case of 0 sides
needs a line with a count and no sides that the fixup does not write. The divide error goes to the
handler of div0.asm (div0_init(1) at 0xB8323, handler 0xF6D94, saturate mode). Its
int3 at 0xF6E17 returns, it decodes the SIB byte 0x24 as a ModRM byte and resumes
after the `inc edx` with EDX 0: each such die adds 0, and the part for each level
is added after the loop. **V** in DOSBox-X: a copy of DTUGAME.EXE
that runs the bytes of 0x91C25 with 0 sides right after div0_init and exits with
0x2A + EDX gave 42 (EDX 0; EBX, ECX, ESI, EDI, EBP kept), and 43 with 1 side. The
port adds 0 for each die (`spell_dice`, dtuobj_combat.c). A fire part sets target
+0x15E bit 0.
Creature target: Robot_info+0x98 bit 1 (electricity_immune): electric 0; bit 3
(fire_immune): fire 0; else bit 4: fire / 2. Effect bit 10 (Resist Fire / Flame
Walk): fire / 2. Effect bit 9 (Protection From Fire): fire is taken from the pool
+0x82 (s16); only the rest goes through. A fire or acid part > 0 clears bit 24
(regeneration). Debug print "Damage: %d ( Normal %d Fire %d Acid %d Electric %d )".

0x911AA:
- Creature: plays Spell_info `monster_hit_sound` (+0x10, read as the high word of the
  dword at +0x0E, Code 0x91354), rolls,
  then saving throw 0xAE596(target, 4): made gives damage / 2, else 0x9305D(target,
  1) (interrupts its spell, guess). shields -= damage. If Armor (bit 0) is active,
  the Armor pool +0x90 is reduced too (to 0 at < 1.0). Message to the player
  "You cast %s on %s for %d damage.". Death: `monster_kill_sound` (+0x14), the creature
  becomes dying (+0x170 = 9), its effect word +0x88 and +0x15E cleared, it drops
  its item (0x3C4BD) when +0x7E > 0.
- Player: no damage while Invulnerability (bit 18). Same roll and save (half).
  Messages "You took %d damage." (self or no caster), "%s cast Cause Serious
  Wounds on you for %d damage." (spell 0 from a creature), "A trap hit you with %s
  for %d damage.", "%s cast %s on you for %d damage.". Hurt sound P+0x1B0B when
  damage > max_hp / 16, else P+0x1B0F (Code 0x918B4). Screen flash. At hp < 0 the
  player dies (P+0x73 = killer object).
- Misc object (type 11): 0x3CA7E. Dressing (18): shields -= Dressing_info+0x0C
  * `damage_percent`, breaks below 1.0.

### Area: 0x90CEF(spell, source object, caster) (Code)

Caster level as above. Radius = `radius` (+0x08) + level * `level_radius` (+0x0C),
integer units. For each object 0 to Highest_object_index (0x13D320): alive, its
segment visible from the source segment by the brain bit table 0x9810D (ai.md),
player or creature (or any type if +0x37 bit 3), distance (0xD705C) <= radius:
0x8CFFF(spell, obj, caster). The caster is included (the cases exclude self where
they need to).
Fireball (16) extra, for the player and creatures within 6.0: if not blind: the
save 4; a failed save gives the message (player: "You lost your eyesight!", a
white flash of +254 on each color, 0x13D50C, 0x1F3B00, 0x1F3AF8, 0x46C688 and
0x46C68C set to 0; the sound P+0x1B0B or Robot_info+0x9A at volume 0x5555). Saved
or not (0x90E22 jumps to 0x90F73), blind for 1..1800 s (rand % 1800 + 1, timer
0x98, bit 2). Then if not deaf and the save 2 (petrification/polymorph) fails: deaf
(bit 4, no timer, the player's digital volume becomes rand % 0x1800 via 0x2DF7E,
"You lost your hearing!", the same sound). Only the fire trap reaches this code
(every other caller tests the area flag, which Fireball does not have).

The fire trap (0xA6D5A to 0xA6F66 of 0xA6ACB): after scenery object 0x29 of level
0 (YPINN) breaks: a light object (0x68AC8 type 13, control 14, life 3.0, light
300.0, flags +0x0D |= 0xC0, +0x1AE |= 2; if none, object 1 is the source), sound
2000 at the object (side 3, volume 1.0), "You set off a fire trap!!!", a flash of
+254 on each color, the burst of weapon 25 (0x93294), the player velocity =
(uvec - fvec) * (rand() % 501 + 500), then velocity.y = rand() % 30 (a fix), then
0x90CEF(16, object, light) two times. 0x8CFFF takes the level of a caster of type
13 as 2 * max(class 1 level, class 2 level) of the player (0x8D239).
A source of type weapon (missile) is deleted (0x6A3CC).
Light spells 8, 9, 30, 31: create a light object (0x68AC8 type 13) at the source
point with flags +0x0D |= 0xC0, +0x1AE |= 2, +0x144 |= 1 (dispellable); lifetime
+0x84: 30: 10 L minutes, 31: 10 (L+6) minutes, 8 and 9: 0x3FFFFFFF; light +0x140:
20.0 (30, 31), 60.0 (8, 9).

## B3. Each spell (Code: the cases of 0x8CFFF, jump table 0x8CF1F)

Names, class and level from `YPINN.BIN` (the 92 level tables have the same
`$SPELL` lines for the spells checked). L = caster level (0x3A6540), T = target
level (0x3A6544; a creature: Robot_info max hp / 8, 1..18, 0xAE3BB). "min" =
duration in units of 60.0 GameTime seconds (one AD&D round). Save = 0xAE596(target,
category), section 5. Every timed effect returns at once if its bit is already set.
"Area" = `type=AREA` (flag +0x36 bit 0): the impact calls 0x90CEF, which calls the
case for every player or creature in the radius.

| # | Name | Class, lvl | Kind | What the case does | Case address |
|---|---|---|---|---|---|
| 0 | Abjure | C4 | target | Creature caster (not self): damage roll 0x911AA with level 1 (jump to 0x8EEF7; the "Cause Serious Wounds" message). Else only an `extraplanar` creature (Robot_info+0x98 bit 2, 0x142080 is the entry of the target): d20 >= clamp(11 - L + T, 1, 20) (T = 0x3A6544 = 0xAE3BB of the target, max hp / 8, 1..18), or "%s was not affected by abjure." and return without the common end. Made: the creature stops (velocity along the ray from the player scaled to 10.0 with y 0, then movement type 3 with the spin (0, 0.05, 0)); d100 >= 2T + 70: it dies (0x29A9C(obj, no attacker, its shields, weapon -1)), else its `hit_sound` (Robot_info+0x9A) and 0xA9F58: control, movement and render type 0, +0x1AE |= 4, flags |= 0xC0: the creature leaves the level, no death and no experience (verified) | 0x8D297 |
| 1 | Aid | C2 | target | +1d8 hit points (fix, P+0x53 for the player); sets Bless (bit 1) with timer 0x94 = (L+1) min; message "%d aid hit points ... for %d minutes". The extra hit points go when Bless ends (hp cut to max) | 0x8D3F7 |
| 2 | Armor | M1 | self | bit 0, pool +0x90 = (8 + L) hit points (fix). No timer: ends when the pool reaches 0 (each hit lowers it). Effect: AC at most 6 (0xB05D9 player, 0x2ADF9 creature). See "Armor" below the table | 0x8D4E7 |
| 3 | Bless | C1 | target | bit 1, timer 0x94 = 360 s fixed (0x1680000, 6 min). Effect +1 to hit (0x2AD27) | 0x8D58E |
| 4 | Burning Hands | M1 | 6 missiles (area line) | not self: 0x911AA(target, caster, 4, L): fire 1d3 + 2 per level (`fire_damage_dice=3`, count 1, `fire_damage_level=2`) | 0x8D614 |
| 5 | Chill Touch | M1 | self | caster only (target == caster). bit 3, timer 0x9C = (L+3) min; object +0x16A = 0x13A873. Effect in melee (0x29E3D): the target saves (4) or: undead: Fear (bit 8) for (1d4 + L) min; else +1d4 damage. Line has `cold_damage_dice=4` (not used, cold is never rolled) | 0x8D62F |
| 6 | Color Spray | M1 | 1d6 missiles, vclip 175..180 | not self, not `blind` creatures, not already blinded. If T > L or T >= 6: save 4 or nothing. T <= L: unconscious = Sleep bit 26, timer 0xE8 = (2 + rand % 7) min, for a creature AI mode 7 (+0x142), for the player also blind (bit 2) with the same timer, screen shake, controls cleared. T <= L + 2: blind (bit 2) 1d4 min. Else: stunned = Hold bit 14 for 60 s (player: / (wisdom/2), and Confusion bit 5 with +0x1B0 timer); not with Free Action or Hold | 0x8D6D4 |
| 7 | Confusion | M4 | 1d4+L missiles | save 4. bit 5, timer 0xA0 = (L+2) min; player: +0x1B0 = 0x3FFFFFFF and +0x1AE bit 0x10 | 0x8DB82 |
| 8 | Continual Light (Cleric) | C3 | area | creature or player in radius, not self, not blind, not `blind` type: save 4 or blind (bit 2) forever (0x3FFFFFFF). Adds `player_light` to +0x1A6, 0x8CAD3. `light_sensitive_2` creature: also damage 0x911AA. Area code makes a light object, light 60.0, no end | 0x8DC1D |
| 9 | Continual Light (Mage) | M2 | area | same as 8 | 0x8DC1D |
| 10 | Cure Light Wounds | C1 | self/target | not undead or extraplanar creatures (mask 0x2004): +1d8 hp, at most max hp (player P+0xA1, creature Robot_info+0x78) | 0x8DE4B |
| 11 | Cure Serious Wounds | C4 | self/target | as 10 with +(rand % 15 + 3) (3..17) | 0x8DF37 |
| 12 | Detect Magic (mage) | M1 | self | bit 6, timer 0xA4 = 2L min. Read by the renderer (0x670DB, 0x75726; guess: magic items shown) | 0x8E019 |
| 13 | Dispel Magic (mage) | M3 | target | by object type: player: 0x9002C(obj, 1) (all effects end) and 0x396B2(1); creature: 0x9002C(obj, 1); a missile (type 5) of another spell whose Weapon_info has `PC_spell` and flag `magic`: save 4 or destroyed; misc item named "*otion" not plot: d20 >= clamp(23 - L, 2, 20) destroys it (sound 2702); dressing with `magic`: save 5 or 0xA6ACB(obj, caster, 10.0) (break); other object with +0x144 bit 0 (spell lights): save 4 or destroyed. No save for creatures and the player Verified details: the missile needs W+0x3C >= 0, not the dispel spell, and W+0x3E bit 0x10; save 4 of a missile or a light uses the default number 10 (fails on d20 <= 9); missile: sound word 0x1B6860, then 0x6A3CC; potion: word 0x19BAE2 + id 68 == 0 (not plot) and "otion" in the name 0x1B14EC + id 32, sound 2702; a light with +0x144 bit 0: sound, then 0xA6ACB (removed); the player also gets 0x396B2(1) (all potion timers end). | 0x8E0B5 |
| 14 | Fear | M4 | target | not self, not `no_scare`/`undead`, save 4. bit 8, timer 0xAC = L min (player / (wisdom/2)). d100 <= 65 - 5T: drops the weapon (player 0xAE837, creature 0x3C4BD). Player: also Hold (bit 14) for the same time and the view is moved to another object (0x13407, 0x13D628) Verified: the player drop 0xAE837 acts only for an item list reference: weapon 13 is a script case (flags 0x8A, 0x90, 0xAC055(0, 0x1FD)); else 0x3C93B(player, 1, type, id) makes the item at the center of the segment of the player (0x13899), 0xB6D0C takes it out, "You dropped your weapon!"; no object: animation 10, "You were disarmed!". The creature drop 0x3C4BD is object_create_egg (what it contains). The view: 0x13407 gates a creature of Robot_info[0] at the player (fails with "Cannot gate in an NPC until you kill one." above 2 difficulty + 3 creatures), Viewer = that creature with the Fear timer, render types 0; at the end of Fear (0x903D4) the creature is deleted and the player is moved to its segment center with its orientation. | 0x8E2D8 |
| 15 | Feather Fall | M1 | self/target | bit 7, timer 0xA8 = L min. Effect: no gravity flag in physics (0x6C77A), fall speed limited to -15.0 (0x9DEDD) | 0x8E464 |
| 16 | Fireball | M3 | missile, impact area radius 40 (the fixup; the table text has 10) | sets count `#fire_damage_dice` = min(L, 10), then 0x911AA: fire count d6 (the fixup 0x1A9C7 gives 6 sides and 0 per level; the table text has 0 sides and `fire_damage_level=6`); save 4 for half. Area extra: blind and deaf within 6.0 (section 2) | 0x8E55C |
| 17 | Flaming Sphere | M2 | missile | player/creature: 0x911AA(target, caster, 17, L) with the dice of the line (no change of the count). Scenery: the path of 37 (0x8F03A): bit 0x04 clear and save 5 failed: 0xA6ACB(obj, caster, 10.0). Fireball (16) takes the same scenery path | 0x8E5B1 |
| 18 | Flame Walk | C3 | self/target | bit 10, timer 0xB0 = (L+1) min. Effect: fire damage / 2 (0x91DD0 and collide.c 0x2609D, 0x26399) | 0x8E5D1 |
| 19 | Fly | M3 | self | bit 11, timer 0xB4 = 10 (1d6 + L) min. Effect: flying physics (0x6C77A), fly movement (0x9DDA4) | 0x8E665 |
| 20 | Free Action | C4 | self only | bit 12, ends Hold (timer 0xC0 = 0), timer 0xB8 = 10 L min. Effect: no slow down (0x9DDB4, 0x9F39B) | 0x8E6F2 |
| 21 | Haste | M3 | self | needs counter +0x49 > 0 (set to L at the self cast), -1. Ends Slow (timer 0xEC = 0). bit 13, timer 0xBC = (L+3) min. Effect: move speed x2 (0x9DE13), attack delay / 2 (0x41EC3) | 0x8E79C |
| 22 | Hold Person | M3 | missile | not self, not held, no Free Action, not undead, save 4, only `humanoid` creatures or the player, counter +0x49 > 0 (1d4 at launch), -1. bit 14, timer 0xC0 = 2L min (player / (wisdom/2)), +0x1AE bit 0x20, 0x6C662 (stop). Message in seconds below 60 | 0x8E85A |
| 23 | Hold Undead | M3 | missile | not self, not held; T > 2: save 4; only `undead` creatures with T <= L; counter > 0, -1; HD pool +0x76 -= T. bit 14, timer 0xC0 = (1d4 + L) min | 0x8EA21 |
| 24 | Improved Invisibility | M4 | self | bit 16, timer 0xC8 = (L+4) min; creature +0x147 = 1. Not ended by attacks | 0x8EB53 |
| 25 | Infravision | M3 | self | not for a player of a race other than human (P+0x9A != 0) or an `infravision` creature. bit 15, timer 0xC4 = (6L + 12) * 10 min (2 h + 1 h per level); message in hours | 0x8EBE8 |
| 26 | Invisibility | M2 | self | not with bit 16 or 17. bit 17, timer 0xC8 = 0x3FFFFFFF (no end); creature +0x147 = 1. Ended by a weapon attack (0x4440E) and by casting a spell missile (0x5705B) | 0x8ECB4 |
| 27 | Jump | M1 | self | bit 19, timer 0xD0 = (1d3 + L) min. Effect: jump speed 2.0 instead of 1.5 (0x9EA62) | 0x8ED20 |
| 28 | Knock | M2 | target | the case does nothing (jump table entry = 0x8FFE5, the common end). Guess: doors are opened by the door code that the missile hits | 0x8FFE5 |
| 29 | Levitate | M2 | self/target | bit 21, timer 0xD8 = 10 L min; +0x1AE bit 0x80 set, bit 0x20 cleared. Effect: no gravity, vertical speed 0 and / 4 (0x9DEAE); "cannot cast while climbing" while +0x1AE bit 0x80 | 0x8EDB2 |
| 30 | Light (Mage) | M1 | area | as 8 but blind for 10 L min; area light object 20.0 for 10 L min | 0x8DC1D |
| 31 | Light (Cleric) | C1 | area | as 8 but blind for 10 (L + 6) min; light object 10 (L+6) min | 0x8DC1D |
| 32 | Lightning Bolt | M3 | missile, lifetime 2.0 | count `#elec_damage_dice` = min(L, 10), 0x911AA: electric count d6 (the fixup gives 6 sides and 0 per level; the table text has 0 sides and 6 L); `electricity_immune` creatures 0. A dressing whose Dressing_info flags (+0x04) bit 2 (0x04) is clear: save 6 or 0xA6ACB(obj, caster, 10.0) | 0x8EE76 |
| 33 | Magic Missile | M1 | min(L/2+1, 5) homing missiles | not self, not Shield (bit 25): 0x911AA(target, caster, 33, 1): 1d4 + 1 (normal dice 4, count 1, `damage_level` 0? The line has `damage_bonus=1`, which 0x91BC3 does not add; guess: 1d4) | 0x8EED5 |
| 34 | Magical Stone | C1 | 1d3 missiles | not self: count = 2 against `undead`, else 1; 0x911AA: count d4 | 0x8EEFE |
| 35 | Magical Vestment | C3 | self | bit 31, timer 0xF8 = 5 L min. Effect: AC at most max(5 - (L-5)/3, 1) (0xB0606, 0x2ADAF) | 0x8EF49 |
| 36 | Melf's Acid Arrow | M2 | missile, lifetime (L/3+1) min | not self: `#acid_damage_dice` = 2 (L/3) + 2, then 0x911AA with level L: acid count d4 (the fixup gives 4 sides and 0 per level; the table text has no sides and `acid_damage_level=4`) | 0x8EFE0 |
| 37 | Melf's Minute Meteors | M3 | min(L,5) missiles | player/creature: 0x911AA: 1d4 (+0 per level); a dressing whose Dressing_info flags bit 2 (0x04) is clear: save 5 or 0xA6ACB(obj, caster, 10.0) | 0x8F021 |
| 38 | Prayer | C3 | self area (flag set to 0x102 at the cast) | the caster, and others whose +0x1B0 bit 0x200 (`not hostile`, objects.md) is clear: Bless bit 1, timer 0x94 = L min | 0x8F078 |
| 39 | Protection From Evil (mage) | M1 | self | bit 23, timer 0xE0 = 2L min. Effect: attacks of `extraplanar` creatures always miss, other creatures -2 to hit (0x2AD36); attacking an extraplanar creature ends it (0x2AD70) | 0x8F116 |
| 40 | Protection From Fire | C3 | self | bit 9, pool +0x82 += 12 L hit points. No timer: ends when the pool < 1 (0x90489) | 0x8F1B2 |
| 41 | Rary's Mnemonic Enhancer | M4 | self | Returns at once if bit 22 is set (0x8F226): a second cast does nothing. Sets bit 22, message "%s %s Rary's Mnemonic Enhancer for 1 day." For the player the box 0x655FE(0, 3, "Three 1st", "One 1st and one 2nd", "One 3rd", text): 0: P+0x23 += 3, 1: P+0x23 and P+0x24 += 1, other (also Escape, -1): P+0x25 += 1; the choice is kept at P+0x1A54. Only the maximum is raised; the next rest fills it (0x3551B). No timer: bit 22 ends only with force (Dispel Magic 0x8E0F2, Potion of Anti-Magic 0x38AFD); the end (0x90886) takes the same amounts back. The rest (0x36049) and the level start do not clear it. The manual says something else (each cast three more levels, good for one rest period): see C3 `Dtu_fix_rary_manual` | 0x8F21D |
| 42 | Resist Cold (name in the table) | C2 | self | The code is Cure Blindness or Deafness: needs bit 2 or 4; ends blindness (timer 0x98 = 0, the timer code clears bit 2) and deafness (bit 4 cleared, the player's volume set back from 0x13BFEB). Messages "%s %s no longer blind.", "... no longer deaf.", "Cure Blindness or Deafness cast on %s by %s." | 0x8F2FA |
| 43 | Resist Fire | C2 | self | bit 10, timer 0xB0 = L min (same bit as 18) | 0x8F3CD |
| 44 | Scare | M2 | missile | not self, T <= 6, not Fear, a creature not of the caster type (creature vs creature) and not `no_scare`/`undead`/`extraplanar`. Save 4 only for: player elf (race 2) or half-elf (5) or cleric (0x35A09); creature with Robot_info+0xD9 bit 3 or a cleric level (+0xE3). Fear bit 8, timer 0xAC = (1d4 + L) min (player / (wisdom/2)); 50% drops the weapon; player as 14 Drop: rand() % 2 != 0. The view change as 14. | 0x8F467 |
| 45 | Shield | M1 | self | bit 25, timer 0xE4 = 5 L min. Effect: AC at most 3 (0xB0659, 0x2ADE9), blocks Magic Missile | 0x8F6AE |
| 46 | Sleep | M1 | missile, HD pool 4..16 | not self, not asleep, not `no_sleep`/`undead`, T <= 3. Player elf: d10 <= 9 immune; half-elf: d10 <= 3 immune; creature with +0xD9 bit 3: d10 <= 9 immune. Cost: creature max hp / 4 (Robot_info+0x78 >> 2), player 2 T; pool +0x49 must be >= cost, pool -= cost. bit 26, timer 0xE8 = 5 L min; +0x1AE bit 0x20, +0xF8 = 0; creature AI mode 7; player: blind too, +0x170 = 9, controls cleared. A melee hit on a sleeper wakes it (0x2A839) | 0x8F766 |
| 47 | Slow | M3 | missile, area radius 80 | not self, not slowed, counter > 0 (L at launch), save 4. Ends Haste (timer 0xBC = 0). bit 27, timer 0xEC = (L+3) min, counter -1. Player: the model animation time (Polygon_model+0xFC) doubled. Effect: AC +4 (max 10), -4 to hit, move speed / 2 | 0x8FAD8 |
| 48 | Spider Climb | M1 | self | bit 28, timer 0xF0 = (L+3) min. End: P+0xA0 bit 0x20 cleared, +0x1AE bit 0x80 moved to 0x20 | 0x8FBEE |
| 49 | Stone Skin | M4 | self | Writes +0x88 = 0x20000000: all other effect bits are cleared (bug or design). Count +0x80 = 1d4 + L/2 attacks. Each melee hit on the target takes one and does no damage (0x29DF9, 0x2AE44). Ends at count 0 | 0x8FC7A |
| 50 | Strength | M2 | self | player only for the gain: warrior (0x35A6F) 1d8, at 18 the rest goes to the percent in steps of 10 (at most 100, then 0); not when 18/00 already; thief (0x35A4D) 1d6, cleric (0x35A09) 1d6, else (mage) 1d4, at most 18. Gains kept at P+0x1A55, P+0x1A56 for the end. bit 30, timer 0xF4 = L hours (3600 L s). Effect +1 to hit (0x2ACBA) Verified (0x8FCFA): returns if bit 30 is set; warrior: returns at 18/00 (18 and percent 0), else 1d8, above 18 each point is 10 percent, above 100 the percent becomes 0 and the rest is lost; thief or cleric 1d6, mage 1d4, nothing at 18, at most 18. Gains at P+0x1A55, P+0x1A56, 0x1BD5A6 |= 8. End 0x90BE4: strength - gain; percent 0 counts as 100 before the percent gain is taken back; 0xAFB55. | 0x8FCFA |
| 51 | "Burning Hands" (cold, `ice.bbm`, in the table) | M1 | self | The code is Find Traps: bit 20, timer 0xD4 = 1800 s (30 min). Effect: Find/Remove Traps = 100 (0xB032F) and 0xB4040 each frame (game.c 0x43901, thiefpts.c) | 0x8E4F6 |
| 52 | Hold Person (cleric) | C2 | missile | as 22 (the line has `cast_on_top=130`) | 0x8E85A |
| 53 | Protection From Evil (cleric) | C1 | self | as 39 with timer 3L min | 0x8F116 |
| 54 | Detect Magic (cleric) | C1 | self | as 12 with timer 600 s (10 min) | 0x8E019 |
| 55 | Dispel Magic (cleric) | C3 | target | as 13 | 0x8E0B5 |

Missile data (`weapon_vclip`, `speed`, size) comes from the line through the copy
in 0x56FF6 (section 1). `speed` is not in any `$SPELL` line of YPINN.BIN, so the
default 100.0 (0x640000) is used for all. `range` and `level_range` are stored but
no reader was found. `level_radius` is read by 0x90CEF; no line sets it.

Spell number 0 from a creature caster is reported as "Cause Serious Wounds"
(0x91987). Creature casters (0x92D99, ai.md) use the same 0x8CFFF / 0x90CEF.

## B4. Duration effects: 0x8CFFF sets, 0x9002C ends (Code)

Units: every timer holds an absolute `GameTime` (0x13CD30, fix seconds, the
Descent game clock). Set: timer = GameTime + duration. 0x3FFFFFFF means "no end".
So the timers count time, not frames.

0x9002C(object, force): returns at once if +0x88 == 0 and +0x1AE bit 0x10 is
clear. For each bit: when force != 0, or the timer is not 0x3FFFFFFF and
timer < GameTime: clear the bit, set +0x15F bit 3, run the end action, and for the
player print "NAME expired." on the HUD. Callers: every frame for the player
(game.c 0x438BD in the frame function 0x435A6, and 0x43A4A), every frame for each
creature (ai.md, 0x14386), with force = 1 from Dispel Magic (0x8E0F2, 0x8E10D) and
the item use code (0x38AFD).

Corrected bit list (replaces objects.md where it differs):

| Bit | Effect | Timer | End rule and action (0x9002C) | Read by (effect while set) |
|---|---|---|---|---|
| 0 | Armor | none (+0x90 is a pool) | pool +0x90 <= 0; player 0xAFB55 | AC <= 6 (0xB05D9, 0x2ADF9); damage lowers the pool (0x29F3E, 0x913AF) |
| 1 | Bless (also Aid, Prayer) | 0x94 | hp cut to max (removes Aid hp) | +1 to hit (0x2AD27) |
| 2 | Blinded | 0x98 | light +0x1A6 -= Light (Cleric) `player_light`; player screen values restored | 0x10DA1, 0x12C1E, 0x13D0B ... (AI sight, guess) |
| 3 | Chill Touch | 0x9C | +0x16A = -1 | melee extra 1d4 or Fear on undead (0x29E3D) |
| 4 | Deaf (Fireball) | none | only by spell 42 | 0x13D9B, 0x161E2, 0x58B82 (hearing, guess) |
| 5 | Confusion | 0xA0 | player: +0x1B0 = 0, +0x1AE bit 0x10 off; creature: hostile again (+0x1B0) and 0x10ADF | 0x10BC4 (AI) |
| 6 | Detect Magic | 0xA4 | | 0x670DB, 0x75726 (render) |
| 7 | Feather Fall | 0xA8 | no message | physics (0x6C77A), fall speed >= -15.0 (0x9DEDD) |
| 8 | Fear | 0xAC | player: view object restored (0x13D628); creature hostile again | 0x10D32, 0x158C6, 0x59DB3 (AI, talk) |
| 9 | Protection From Fire | none (+0x82 pool) | pool < 1 | fire damage taken from the pool (0x91DE6, 0x2605D, 0x26359, 0x2B3FE) |
| 10 | Resist Fire / Flame Walk | 0xB0 | | fire / 2 (0x91DD0, 0x2609D, 0x26399, 0x26629, 0x2B3E0) |
| 11 | Fly | 0xB4 | | flying physics and movement |
| 12 | Free Action | 0xB8 | | no slow down (0x9DDB4, 0x9F39B) |
| 13 | Haste | 0xBC | | speed x2 (0x9DE13), attack delay / 2 (0x41EC3), AI 0x119AD, 0x14173 |
| 14 | Hold (Hold Person/Undead, Color Spray stun, Fear on the player) | 0xC0 | creature +0x1DB = GameTime, 0x10ADF | no move (0x9DC84 mask 0x4004000), AI stops (ai.md) |
| 15 | Infravision | 0xC4 | | the key N can turn the view on (0x54668), see [infravision.md](infravision.md) |
| 16 | Improved Invisibility | 0xC8 (shared) | bits 16 and 17 end together; creature +0x147 = 0; player sound 161 | AC/skills 0xB0310: P+0xD7 -= 15 |
| 17 | Invisibility | 0xC8 | as 16 | ended by attacking (0x4440E, 0x5705B) |
| 18 | Invulnerability | 0xCC | sound 163 | no spell damage (0x9178B), AC - 2 (0xB0707), 0x2A270 |
| 19 | Jump | 0xD0 | | jump speed 2.0 (0x9EA62) |
| 20 | Find Traps (spell 51) | 0xD4 | | F/R Traps 100 (0xB032F), trap search each frame 0xB4040 (0x43901) |
| 21 | Levitate | 0xD8 | | physics, vertical speed (0x9DEAE); cleared by 0x546D0 |
| 22 | Rary's Mnemonic Enhancer | none | only by force; extra slots taken back | |
| 23 | Protection From Evil | 0xE0 | 0xAFB55 | 0x2AD36, 0x2AD70 |
| 24 | Regeneration (creatures) | none | not handled; cleared by fire or acid damage (0x91E3D) | ai.md revive code |
| 25 | Shield | 0xE4 | 0xAFB55 | AC <= 3, no Magic Missile |
| 26 | Sleep / unconscious | 0xE8 | also clears bit 2; player +0x170 = 1, controls back; creature: AI wake (0xA6505) | auto hit and wake (0x2A839), no move |
| 27 | Slow | 0xEC | 0xAFB55 | AC + 4, -4 to hit, speed / 2 |
| 28 | Spider Climb | 0xF0 | P+0xA0 bit 0x20 off; +0x1AE 0x80 -> 0x20 | climbing (guess, P+0xA0) |
| 29 | Stoneskin | none (+0x80 count) | count <= 0 | melee hit absorbed (0x29DF9, 0x2AE44) |
| 30 | Strength | 0xF4 | strength and percent gains taken back | +1 to hit (0x2ACBA) |
| 31 | Magical Vestment | 0xF8 | 0xAFB55 | AC limit (0xB0606, 0x2ADAF) |

Also: the timer +0x1B0 of the player (Confusion) is checked at the end of
0x9002C (clears +0x1AE bit 0x10).

Items that give effects (worn): 0xAFB55 keeps the bits that the worn items give
in 0x1458AC and, when an item is taken off, ends the matching effect with
0xB0A98(timer address, bit): item bit 0 -> Chill Touch (bit 3), 1 -> Resist
Fire (10), 3 -> Feather Fall (7), 4 -> Fly (11), 5 -> Free Action (12), 6 ->
Infravision (15, not for non-human races), 7 -> Jump (19), 8 -> Detect Magic (6),
9 -> Haste (13), 11 -> Levitate (21), 12 -> Improved Invisibility (16), 13 ->
Invisibility (17), 10 -> Shield (25), 14 -> Protection From Evil (23).

Armor class (0xAFB55, player): AC = 10 + items; Armor: min(AC, 6); Magical
Vestment: min(AC, max(5 - (L - 5) / 3, 1)) with L = 0x91EC1(35); Shield:
min(AC, 3); Slow: AC + 4; then the dexterity table 0x14019A; Invulnerability:
AC - 2; limit -10 to 10.

To hit (collide.c 0x2A849, d20 roll in 0x1B7AA4, attacker ebp, target ecx):
attacker Strength +1, Bless +1, attacker Slow -4; target with Protection From
Evil: an `extraplanar` attacker rolls 0, other creatures -2; creature target AC
limits by Vestment, Shield (3), Armor (6) and +4 for Slow (max 10); a sleeping
target is hit at once and wakes (0x2A839).

## B5. Saving throws: 0xAE596(object, category) (Code)

Returns 1 (saved) when d20 (`rand() % 20 + 1`) >= the save number. A debug line
prints the result. Categories (the AD&D order of the table rows): 0
paralyzation/poison/death, 1 rod/staff/wand, 2 petrification/polymorph, 3 breath
weapon, 4 spell. The spell code uses 4 for all spell saves, 2 for the Fireball
deafness, 5 and 6 for dressings.

- Player: level = max(class1_level + 1, class2_level + 1) (P+0xAD, P+0xAF), at
  most 12. Row = (level - 1) / divisor[class]; divisors (bytes 0x1456E2): cleric
  3, mage 5, fighter 2, thief 4. Row pointer tables 0x1456D2[class] (cleric
  0x145692, mage 0x1456C6, fighter 0x1456AE, thief 0x1456A2), each row 5 bytes.
  Two classes: the lower (better) of the two numbers, both with the same level.
- Creature: level = 0xAE3BB (max hp / 8, 1..18), at most 12. Cleric table if
  Robot_info+0xE3 (cleric level) > 0, else the fighter table. `magic_resistant`
  (Robot_info+0x98 bit 0x400): save number - level / 3.
- Dressing (type 18): table by its `explode_sound`: 2702 -> 0x1456E6, 2218 ->
  0x1456E8, other -> 0x1456EA, byte [category - 5] (bytes 3,2 / 3,14 / 7,9);
  `magic` dressing: number / 2.
- No modifiers for race, constitution, wisdom or magic items were found in this
  function.

Tables (rows by level band, columns 0..4):

| Class | Rows |
|---|---|
| cleric (levels 1-3, 4-6, 7-9, 10-12) | 10 14 13 16 15 / 9 13 12 15 14 / 7 11 10 13 12 / 6 10 9 12 11 |
| mage (1-5, 6-10, 11-12) | 14 11 13 15 12 / 13 9 11 13 10 / 11 7 9 11 8 |
| fighter (1-2, 3-4, ..., 11-12) | 14 16 15 17 17 / 13 15 14 16 16 / 11 13 12 13 14 / 10 12 11 12 13 / 8 10 9 9 11 / 7 9 8 8 10 |
| thief (1-4, 5-8, 9-12) | 13 14 12 16 15 / 12 12 11 15 13 / 11 10 10 14 11 |

These are the AD&D 2nd edition saving throw tables.

## B6. Usable items: 0x37A6A(index) (displays.c, Code)

Called from the inventory input (0x2FCC0), the inventory browser (0x35ABD) and the
USE button (0x360DA). Argument: index in the item list (asserts "Index >= 0",
"Index < P.inventory_count"). The behavior is hard coded by item type and table
line number; the `pc_spell`, `pc_spell2`, `duration`, `set_*` keywords of the
`$OBJECT` lines are not read here (the numbers in the code differ from them, see
"Potion of Agility" 0x53). L in this section is not used: potions do not depend on
a level. "vclip of spell n" = 0x8CAD3(n, player), the cast light/vclip only.
All timers are GameTime based; a new potion keeps the later of the old and the
new end time.

Weapon (type 5): only weapon line 13 when it is the current item and the
player has none of the effect bits 0xC004124: script events (0xADB3B 0x86, 0x91;
0xAC055 0x1FD) and if alignment P+0xE3 == 0 (Lawful Good): area spell 8
(Continual Light), spell 20 (Free Action) and spell 53 (Protection From Evil) on
the player. Other weapons: nothing here (a weapon with `PC_spell`, a wand, fires
its spell through 0x56A0D, section 1).

Armor (type 15): only armor line 45: area spell 8 on the player.

Misc (type 16), by line number (`YPINN.BIN` names; the same in all tables):

| Line | Name | Action |
|---|---|---|
| 0x02 | Gem of Curing | uses counter P+0x1A57 must be <= 0x8445A() ("You cannot use the Gem Of Curing at this time."); if blind: blindness cured "...And now you can see!"; else if hp < max: spell 11 (Cure Serious Wounds); counter + 1. Not used up |
| 0x12 | Spider Piece | charges (+8) > 0: class 1 becomes a cleric of level (class1_level + 1) * 2 for the call of turn undead (0xADC46, 0x38196), so the turn is at that level + 1 (a class 2 cleric or armor 45 in the cloak place give the level of 0xADC91, 0xADCB3); charges - 1. Else "Nothing happens..." |
| 0x13 | Spider Piece | charges: whole map known (0x185FEC[] = 1); charges - 1 |
| 0x14 | Spider Piece | charges (0x385D3): 0x40974 and 0x34B26 (the hands of the interface); for each object 1 to Highest_object_index (0x3864E): if it is a creature (type 2) without flag 2, with Robot_info+0xD9 bit 3 (names rf, rm, av, dro, 0x1F448) and 0x9810D(its segment, player segment): the player faces it (0xD7110, 0xD74C3, 0x6B088), then rand() % 3: 0 save 0 or killed (0x29A9C(obj, 0, its hit points, -1)); 1 a spell of 0x2FA94 (4, 6, 7, 9, 13, 16, 22, 32, 33, 34, 36, 37, 44, 47) with its flags +0x38 |= 0xC0 for the shot, 0x56FF6(player, spell, 2, 1); 2 save 2 or movement type 3 (spin), spin_rate (0, 0.5, 0), lifeleft = 0xAE3BB level seconds. After each object index one game frame 0x435A6(1, 0). At the end 0xD74C3 with a NULL vector (0x3884D: the int 3 of the assert, then a read of address 0), charges - 1, 0x8CAD3(16, player). The port runs one object index for each game frame (dtucombat.c) and keeps the last orientation |
| 0x16 | Spider Piece | charges: area spell 14 (Fear) and spells 1, 35, 19, 40, 43, 18, 50 on the player |
| 0x17 | Spider Piece | charges (0x38268): 0x8CAD3(12, player); 0x40974, 0x34B26; player render type 0; for each creature 1 to Highest_object_index (type 2, flag 2 clear): Viewer = it until GameTime > start + 1.0 + rand() % 2.0, game frames 0x435A6(1, 0) meanwhile; then Viewer = player, render type 1, charges - 1. The port steps the same loop at each game frame |
| 0x18 | Spider Piece | charges (0x38451): 0xAC055(0, 0x1FC) runs global script 508 on a temporary object (one point more: STR for a fighter, DEX for a thief, WIS for a cleric, INT for a mage, each below 18; CON and CHA on rand(1..2) == 1 or global flag 77 or 69); charges - 1; 0x8CAD3(41, player), the cast vclip of spell 41. The piece stays in the inventory with 0 charges. Else "Nothing happens..." |
| 0x19 | Assembled Spider | charges: spells 48, 20, 21, 25 on the player, then their timers (Spider Climb, Free Action, Haste, Infravision) = 0x3FFFFFFF (permanent) |
| 0x1A | Spider Piece | charges: hp = max, spells 1, 45, 15, 24, 35, 38, 39, 43, 49 on the player; then spell 42 (cure blind/deaf) if blind or deaf |
| 0x48 | Potion of Agility | dexterity < 18: dexterity = 18 until GameTime + 600 s (P+0x1A9F); vclip of 21 |
| 0x49 | Potion of Anti-Magic | if any effect or potion timer is set: 0x9002C(player, 1) (all effects end) and 0x396B2(1); vclip of 13 |
| 0x4A | Potion of Dragon's Blood | strength < 18: strength 18 for (rand % (class1_level + 1) + 1) min (P+0x1A9B); a warrior also gets a percent rand % 100; vclip of 50 |
| 0x4B | Potion of Extra-Healing | hp < max: + (rand % 22 + 6) (6..27), "You feel much better."; vclip of 11 |
| 0x4C | Potion of Fire Resistance | bit 10 for 600 s; vclip of 43 |
| 0x4D | Potion of Flying | spell 19 (Fly) on the player |
| 0x4E | Potion of Freedom | bit 12 (Free Action) for (rand % 4 + 5) * 600 s, "You are immune to paralysis for awhile." |
| 0x4F | Potion of Healing | hp < max: + (rand % 7 + 4) (4..10), "You feel better."; vclip of 10 |
| 0x50 | Potion of Hill Giant Strength | strength < 19: strength 19, percent 0, for (rand % 4 + 5) * 600 s (P+0x1A97), "You have Hill Giant strength for awhile." |
| 0x51 | Poshun uv Hil Gient Inteljuns | intelligence = rand % 3 + 5 for (rand % 4 + 5) * 600 s (P+0x1A93); then a random vclip and screen flash for each point of intelligence |
| 0x52 | Potion of Infravision | bit 15 for (rand % 8 + 1) * 600 s; vclip of 25 |
| 0x53 | "Potion of Agility" (second line) | Invisibility: bit 17 for (rand % 4 + 3) * 600 s; vclip of 26 |
| 0x54 | Potion of Invulnerability | bit 18 for (rand % 16 + 5) * 60 s; screen flash; vclip of 28 |
| 0x55 | Potion of Jumping | spell 27 (Jump) on the player |
| 0x56 | Potion of Luck | P+0x1A58 = GameTime + 3600 s; vclips 1, 3, 38; script flag 0x4D; "You feel lucky today." (P+0x1A58 is read by the hit roll 0x2A87B: then some rolls become a natural 20, guess) |
| 0x57 | Potion of Speed | bit 13 (Haste) for (rand % 16 + 5) * 60 s; vclip of 21 |
| 0x58 | Potion of Toughening | if max hp < current hp (already toughened): "You don't feel so good." and a system shock roll: d100 > table 0x2FACC[constitution - 1] (25 30 35 40 45 50 55 60 65 70 75 80 85 88 90 95 97 99 99) kills the player. Else Bless bit on object 0 with timer GameTime + (rand % 6 + 7) * 600 s and shields += rand % 17 + 2, "You feel tougher." (the extra hp go when Bless ends) |
| 0x65 | Orb of Insight | script event 0x1FE |
| 0x66 | Scroll | 0x9DC42(word +8 of the entry): shows scroll text n (ingame-ui.md, "The scroll text screen") |
| 0x6A | Cursed Scroll | screen shake; each with 50%: spells 7, 9, 44, 22, 16 on the player, map forgotten, and strength, dexterity, constitution, intelligence, wisdom, charisma each set to rand % value + 1; then damage rand() % (shields as a fix) (0x38159: less than half a point; `Dtu_fix_cursed_scroll`: whole points); "You no longer think that reading the scroll was such a good idea..."; not used up (0x3817F jumps past 0x39694) |
| 0x7F | thief scroll | 0x9DC42(1) |

The end of the function (0x39694) calls 0xB7084 (use up one, section 1) for the
cases that jump there (potions, Gem not).

Scrolls of spells: a spell object picked up by a character that cannot learn it
stays in the item list; selecting it and firing casts it with flag
`cast_with_scroll` and missile template 4, without cast time and slot, and uses
one count (+8) per cast (sections 1). Wands: weapons with `PC_spell` (Weapon_info
+0x3A): the fire code casts that spell with template 3 (`cast_with_wand`). Their
charges are the weapon entry count +8 (0xB7084, guess for wands: the same path).
Rings, cloaks and other worn items: their effects are bits of the item tables
(0x1458B8 weapons, 0x145A04 misc, armor table) applied by 0xAFB55 (section 4).

### B6a. Worn items, potion timers, Luck (Code)

The effects of worn items: 0xAFB55 collects item bits in edx (the slot code
at 0xAFBC8 to 0xB03C2), compares them with 0x1458AC, ends the effect of each
bit that went away with 0xB0A98(timer, mask, 1) (timer 0, bit cleared, no
message), stores edx at 0x1458AC, sets the counter 0x1458B4 to 0 and calls
0xB08A8. 0xB08A8 (also called by the inventory screen 0x312D1, which works
only when 0x1458B4 & 0x1F is 0) sets each effect of 0x1458AC with
0xB0A98(timer, mask, 0): timer 0x3FFFFFFF, bit set.

| Item bit | Effect (timer, bit) | Items (slot, number) |
|---|---|---|
| 0 | Chill Touch (0x9C, 3) | weapon 54 in the hand |
| 1 | Resist Fire (0xB0, 10) | ring 17 (Ring of Coolness), weapon 54 |
| 3 | Feather Fall (0xA8, 7) | boots 35, ring 18, weapon 54 |
| 4 | Fly (0xB4, 11) | boots 34 (Winged Boots), ring 19 |
| 5 | Free Action (0xB8, 12) | ring 20, weapon 54 |
| 6 | Infravision (0xC4, 15), only race byte <= 0 | ring 22, weapon 54 |
| 7 | Jump (0xD0, 19) | ring 23, weapon 54 |
| 8 | Detect Magic (0xA4, 6) | gauntlet 51 (Glim-Gauntlet), ring 24, weapon 54 |
| 9 | Haste (0xBC, 13) | boots 37 (Boots of Speed), ring 25, weapon 54 |
| 10 | Shield (0xE4, 25) | necklace 33 (Amulet of Shield; the test is on the number only) |
| 11 | Levitate (0xD8, 21) | boots 36 |
| 12 | Improved Invisibility (0xC8, 16) | cloak 44 (Chameleon Cloak), weapon 54 |
| 13 | Invisibility (0xC8, 17) | cloak 46 (Elven Cloak) |
| 14 | Protection From Evil (0xE0, 23) | no item sets it |

Other values in 0xAFB55: weapon 54 in the hand sets Find/Remove Traps and Climb Walls 250
(95 after the limit); cloak 43 Hide In Shadows 95; effect bits 16 or 17
(invisible) take 15 from Open Locks (0xB030B); the Find Traps effect (bit
20) sets Find/Remove Traps 100 (0xB032A). Ring 16 ("Armoring", `pc_spell=2`) in a ring place sets the byte [esp+0x2C] (0xB0014, 0xB01B6), which 0xAFAD1 reads after each cap of Armor, Magical Vestment and Shield (0xB05FC, 0xB064F, 0xB067C): a mage (0x35A2B) gets 1 better armor class. Ring 27 (the
tables call it "Ring of Warmth", `pc_spell=42`) doubles the mage slot maxima
P+0x23 to P+0x25 each time 0xAFB55 runs while it is worn (0xB00A5, 0xB0247;
flag 0x1465A4), and when it is taken off they are halved one time (0xB075F):
the doubling repeats with each change of the equipment (a defect). The
`pc_spell=`, `set_strength=` and similar keywords of the lines are not read
by this code; the numbers above are.

0x396B2(force), the potion timers: called each frame with 0 (0x438C4 in the
frame 0x435A6, 0x43A51), with 1 by Dispel Magic and the Potion of Anti-Magic
(0x38B07). A timer ends when GameTime is above it or with force:

| Timer | Potion | While it runs | End |
|---|---|---|---|
| P+0x1A93 | Poshun uv Hil Gient Inteljuns | each call, 1 in 63: Automap_visited 0x185FEC[i] = 0 for i from rand % 49 to the last segment, step 16 + rand % 17 (frame rate dependent) | intelligence = word 0x187BB6, "...Intelligence expired." |
| P+0x1A97 | Hill Giant Strength | strength at least 19, percent 0 | strength and percent from 0x187BB0, 0x187BB2 |
| P+0x1A9B | Dragon's Blood | strength at least 18 | as above |
| P+0x1A9F | Agility | dexterity at least 18 | dexterity from 0x187BBC |
| P+0x1A58 | Luck | none | 0xADB3B(0x4D, 0), "Your luck just ran out." |

Each change calls 0xAFB55 and sets 0x1BD5A4 |= 0x80010. The restore copies
absolute values, so a Strength spell gain made during the potion is lost.

Luck in the hit roll 0x2A87B: the attacker is the player, P+0x1A58 is not 0
and the day clock P+0x83 (raw fix) is odd: 0xAE564 plays sound 0x8A8, sets
P+0x1A58 = 0 and clears script flag 77, and the d20 becomes 20 (a critical
hit). So Luck gives one critical hit. A natural 1 is a fumble; then
`rand() % 20 + 1 > dexterity` drops the weapon (0xAE837).

The scrolls: misc 0x66 (Scroll) calls 0x9DC42(word +8 of the entry), misc
0x7F (thief scroll) 0x9DC42(1); both jump to 0x396A0, after the use up
0x39694: a scroll is not used up. 0x9DC42 is the scroll text screen of
dispscrl.c (0x9CB62 colors, 0x9D43B the scroll picture, 0x9CEA3 the text).
0x9CEA3(n): n = 0 takes the Khelben blurb of the current mission, FETXT entry
0x1451E5[mission] (271 is "Quest One: Corner the Kobolds"); n = 1 to 0x7F takes
FETXT entry 0x144FE5[n] (0x144DE5[n] when 0x13A94B is set: entries 13 to 138, longer texts
for the high resolution).
The Scroll of a new character has count 0, so it shows the current mission. The
Scrolls in the levels have counts 2 to 27 (objects.md): 0x144FE5[2] = 141 "A
Brief History of Undermountain", [3] = 142, a note of Colvin Rowander. The thief
scroll shows entry 270, the picture markup `~pc3` (FEBGND 191 low, 192 high, a map
of Waterdeep). The screen is in [ingame-ui.md](ingame-ui.md), "The scroll text
screen", and in the port in `dtuscroll.c`.

The Gem of Curing compares its use counter, byte P+0x1A57 (0x1D4D73 absolute,
read at 0x3891B, + 1 at 0x389E0, no other writer), with 0x8445A(), the mission
number 0x13E588 (0 at 0x82BC7, the new mission at 0x83E5E, scripts.md section 8).
So it can be used once at the start and once more for each new mission.

Spell missiles (0x56FF6, Laser_create_new 0x55511 at 0x557AF): the life
+0x84 = Weapon_info[template]+0xA8, which 0x56FF6 writes: 30.0, Lightning
Bolt 2.0, Melf's Acid Arrow (L/3 + 1) * 60.0. The spell `duration` and
`range` are not used for a missile. The wall hit 0x26C2F (from 0x29802), with
spell = W+0x3C: Knock (28) calls the door function 0x7E859(seg, side, 1,
parent); an area spell calls 0x90CEF(spell, missile, parent) at 0x26D63,
which also deletes the missile. A missile whose life ends has no area.

## B7. Thief skills (Code)

thiefpts.c (0xB4419 to 0xB611B, main 0xB5BB9) is only the screen where a thief
spends skill points at level up (strings "SkillIndex < NUM_SKILLS",
"Difference >= 0", "%d%%"). The points come from Mirt (scripts.md: 0xABB53
gives 20 per thief level since the last visit, 20 more at the first, into
P+0x1AB3). 0xB4ECF sets the limit 0x146840 = points / 2 (at least 1); 0xB44B4
adds 1 to a skill per point while the skill is below 95 (0x5F) and the skill has
grown by less than the limit in this visit. The port has the screen in
`dtuthiefpts.c`. Verified in the instructions:

- 0xB5B49: nothing unless the word P+0x1AB3 is at least 1 and one of the base
  skills (the copy without items, `[0x13A883]` + 0x5C to 0x6C, the attributes
  at 0x187B90) is below 95. Then the box 0x655FE with the exe string "You have
  some thief skill
points available.
Spend them Now?" and TEX 32 "Yes", TEX
  33 "No"; Yes (0) calls 0xB5BB9 and returns its result, else 0.
- The skills in the order of the screen (0xB4DDE for the base copy, 0xB4E74
  for the values in use at P+0xC7): +0x0 Hide In Shadows, +0x4 Move Silently,
  +0x10 Open Locks, +0x8 Find/Remove Traps, +0xC Climb Walls; the names are
  exe strings (table 0x146844). The screen works on a copy (0x3D4E20).
- 0xB4419: a skill is full when it is 95 or more or has taken the limit.
  0xB4499: all five full. 0xB5BB9 returns 0 at once if all are full or a
  picture does not load (0xB4F32).
- Pictures: FEBGND 0x146744[hi] (152 low, 151 high, the scroll of the scroll
  text screen) drawn with mask in the middle of the screen over the play
  screen (0xB58FE); FEBGND 190 (0xB5675, 160 x 70) has the buttons minus
  up, plus up, minus down, plus down at x 1, 41, 81, 121, low at y 41
  (26 x 26), high at y 1 (37 x 37) (0xB5619).
- Layout (0xB503D): the content area is the picture without the margins
  left, right, top, bottom 0x146750 to 0x146768: 47, 44, 43, 33 low and 100,
  100, 130, 100 high. Fonts (0xB598F, 0xB59E0): the medium font of the front
  end (0x13E380, FNTEMFM_.FNT) in the high resolution game, else the game
  font. The bottom row of the area, centered: minus, 16, the points (as wide
  as "999"), 16, plus, its height the largest of the three. Five rows from
  the top, each as high as "555%%" (measured as written) + 6, 5 apart: the
  value in a box 3 in from the left (as wide as "555%%"), the name in a box
  from 5 after it to 3 from the right. A message line as high as the font
  under the area (5 lower in the high resolution).
- Colors (0xB4C45, closest palette colors): names white; values `%d%%` red
  when the skill took points, else yellow; the points `%d` red; all text
  centered in its box. The frame of the selected row (0xB4AB5, the function
  of both modes of 0x146860): blue lines one pixel in, then magenta lines
  at the edge (right and bottom one pixel in).
- Loop (0xB5D49 to 0xB60E4), each pass: the message goes after 3 s; one key
  (a key clears the mouse button of the pass); the mouse button and
  position (the click point is the middle of the cursor, 0xB5DBC). With the
  button down: on plus or minus, a point on the press only (the button was
  up in the last pass); on a row, that row is selected. Keys: minus or left
  a point back (0xB457D), equal or right a point (0xB44B4), up and down move
  the selection around, ` a screenshot, Escape: with points left and a skill
  that is not full "You must spend all points" in red on the message line,
  sound 3000 (0x2DAD3, volume at most 0x8000), and the screen stays; else
  it closes. The plus or minus button under the held mouse button shows its
  down picture. The mouse stays in the area (0xB43B9). The music frame
  0xA4F83 runs.
- Close (0xB60EA): each skill in use gets the gain of its base value, the
  base value takes the value of the screen (0xB4D2F), the displays are
  marked for a redraw (0x1BD5A6 |= 8), the game time starts, and 0xB5BB9
  returns 1. Points that are left stay in P+0x1AB3 until Mirt sets the next
  number. The skill use code is in the file before it (0xB3038 to 0xB43B9; no
source name string) and in the movement code.

Text numbers are lines of DTUE.TEX (a pointer A is TEX[(A - 0x395DAC) / 4],
checked with "No response..." at 0x396C24 and in the port, where text 935 is
"You have successfully picked this lock!"). Messages go through 0x347A2(-1,
text), sounds through 0x2E900(sound, seg, side, pos, 0, F1_0).

Shared:

- 0xB3038(skill, &roll): roll = `rand() % 100 + 1`, success if skill >= roll.
  Skill (jump table 0xB3024): 0 Hide In Shadows P+0xC7, 1 Move Silently P+0xCB,
  2 Open Locks P+0xD7, 3 Find/Remove Traps P+0xCF (100 with the Find Traps
  effect, bit 20), 4 Climb Walls P+0xD3 (100 with Levitate bit 21 or Spider
  Climb bit 28). The values are those of 0xAFB55 (items, armor, limit 0 to 95).
- 0xB30FD(obj, fvi flags, dist): a ray from the object position along its
  forward vector, length dist << 16, through find_vector_intersection
  0x3EFD9; the hit data at 0x3D4A00 (hit_seg +0x10, hit_side +0x14).
- The thief level from 1: 0xB31BD, 0xB365F and 0xB396C take P+0xB1 if class 2
  (P+0x98) is the thief, else P+0xAF, plus 1.
- The 0.5 second limits: |P+0x83 - last| >= 0x8000, the last times at
  0x1466E0 (locks), 0x1466E4 (search), 0x146700 (sneak), 0x14224C (Activate).

Keys (kconfig items of 22 bytes at 0x13C190, defaults at 0x13C010): 39/40
Activate A, 50 Disarm D, 52 Turn Undead T, 54 Move silently M, 56 Find Traps
F, 58 Open Locks O, 60 Rest R. Hide In Shadows and Climb Walls have no key.

### Open Locks 0xB31BD (key O, 0x53FE5)

1. The ray 0xB30FD(obj, 1, 20), then the 0.5 s limit.
2. P+0xD7 == 0: TEX 931 "You cannot perform this action.". No other path for
   a character without the skill.
3. hit_side < 0: return. The wall Walls[seg.sides[side].wall_num] (0x39FEC8,
   0x18 bytes, the Descent 1 layout). Type (+0x10) not 2 (door): return.
4. Flag 8 (locked, +0x11) clear, or state (+0x12) 1 (opening): TEX 936 "This
   door is not locked.".
5. Sound 2500 at the side.
6. Keys (+0x15) bit 0 (KEY_NONE) clear: TEX 932 "Strong magic prevents you
   from picking this lock.".
7. Byte +0x17 (the pad of the Descent 1 record) == level + 1: TEX 933 "You are
   not yet ready to pick this lock.".
8. Roll skill 2. Fail: +0x17 = level + 1, TEX 934. Success: flag 8 cleared,
   +0x17 = 0, wall_open_door 0x7D77E, TEX 935.

The lockpick slot is not read here (it only adds to Open Locks in 0xAFB55).
"Strong magic prevents you from unlocking this door." is the door use function
0x7E859 (keys & 6), not Open Locks.

### Find/Remove Traps

- 0xB3400 (key F, 0x53F9C): no skill test, 0.5 s limit. Every trigger with
  byte +0x0F == 0xFE and every trap with byte +0x03 == 0xFE goes back to 0xFF
  (on and off). P+0xA0 ^= 0x10. Off: TEX 938 "No longer searching.", on: TEX
  939 "Searching...".
- Each frame (game.c 0x438F3) while P+0xA0 bit 0x10 or the Find Traps effect
  (object +0x8A bit 0x10) is on: 0xB4040(player). Movement is divided by 3
  while P+0xA0 & 0x1C (0x9DDE3).
- 0xB4040: ray 0xB30FD(player, 1, 40); for the sides 0 to 5 of hit_seg with
  dot(side normal, forward vector) <= 0.25: if P+0xCF != 0 or the spell is on:
  0xB365F (triggers) then 0xB396C (traps), with a flag 1 for a dwarf that is
  not a thief and has no spell; without the spell also 0xB3ECE, the racial
  search for secret walls (TEX 948, 949; a d6 against 0x1466E8[race]: human 3,
  dwarf 6, elf 5, dark elf 5, halfling 3, half-elf 5). P+0xDF bit 0 counts as
  race 5.
- 0xB3ECE(seg, side, player, dwarf), dwarf = race 1 of any class: the wall of
  the side; if its byte +0x16 is set, return; else set it (so one roll for each
  wall; 0xAD5BF clears +0x16 for all walls at a new level). 0xB3D1E: a dwarf on
  a side with pad & 3 stops. Then rand() % 6 + 1 <= 0x1466E8[race]. Wall type 5
  (closed): each trap that is on with seg and side of the side (+0x43, +0x45)
  and its position (+0x15) within 40.0 of the player: 0xB3E57 (side pad |= 0x10
  on both sides, sound 0xC20 = 3104, TEX 949 "You found a secret door!"), then
  0xB3D4E: each trigger that is on with first_trap (+0x3B) = trap id (+0x00);
  a trigger on another side needs its own racial d6; found: its side pad |= 4,
  0x93B29(trigger, 0xFF, player). TEX 948 if none. Wall type 2 (door) with flag
  0x40: the side center (0x4AD3E) within 40.0: 0xB3E57. A side without a wall
  (wall number -1) reads Walls[-1] (the bytes before 0x39FEC8).
- 0xB34E0, a harmful trap: on, and (dwarf flag: side pad & 3 == 0), and flags
  & 0x140 or weapon_type >= 0 or spawn >= 0 or stat_delta < 0 or robot_type >=
  0. 0xB353A: a trigger that is on and leads through first_trap, enable_trigger
  or enable_trap to a harmful trap.
- 0xB365F, triggers (records of 0x59 bytes at [0x142214]), the segment chain
  (0x3A75C8[seg], next_in_seg +0x49, at most 50 steps). A candidate has the
  side (+0x48), is harmful and is within 40.0 (0x280000) of the player. Byte
  +0x0F: level + 1 a failed find, 0xFF found, 0xFE found and refused; byte
  +0x10: level + 1 a failed removal. +0x0F == level + 1: skip. +0x0F >= 0:
  roll skill 3, a fail sets +0x0F = level + 1. +0x0F == 0xFE or +0x10 == level
  + 1: skip. Found: side pad |= 4, +0x0F = 0xFF. With the spell: 0xB3605 makes
  a marker object for 5.0 s, sound 3001, "You found a trigger!", +0x0F = 0xFE,
  +0x10 = level + 1, return (no removal). Without the spell: the box
  0x655FE(0, 2, TEX 32 "Yes", TEX 33 "No", TEX 940). No: +0x0F = 0xFE. Yes:
  roll skill 3; fail: +0x10 = level + 1, a roll below 96 TEX 941, 96 or more
  TEX 942 "Ooops!!!" and 0x93B29(trigger, 0xFF, player) (the trigger fires).
  Success: if the first trap has flag 1, wall_open_door of its side; side pad
  &= ~4, trigger on = 0, tex_off (+0x2C, if not -1) into tmap_num2 of the side,
  TEX 943.
- 0xB396C, traps (records of 0x5F bytes at [0x142218]): all traps with the
  seg (+0x43) and side (+0x45), harmful, within 40.0 of the trap position
  (+0x15). State byte +0x03, removal byte +0x04 (the same codes). Magical
  (spawn_object +0x2D >= 0 or robot_type +0x33 >= 0): P+0xCF >>= 1 before the
  rolls and P+0xCF += P+0xCF after them; the spell path returns before the
  restore (a defect: the next 0xAFB55 restores it). Found: side pad |= 8; the
  spell path: marker, sound 3001 and 2130 if magical, "You found a magical
  trap!" or "You found a trap!"; the box TEX 944 (magical) or TEX 945; fail
  TEX 946, or at a roll of 96 or more TEX 942 and 0x93B29(t, 0xFF, player) for
  every trigger whose first_trap is this trap; success: door open if flag 1,
  side pad &= ~8, on = 0, tex_disabled (+0x37) into tmap_num2, 0x954C8 (out of
  the list of active traps), TEX 947.
- The automap draws the sides with the pad bits 4 (blue), 8 (green) and 0x10
  (magenta, a found secret door) in flickering colors (0x1968A, see
  [automap.md](automap.md)). Nothing else was found that reads the bits 4 and 8.

### Move Silently 0xB4196 (key M, 0x53F53) and Hide In Shadows 0xB333F

- Move Silently: racial for elf, dark elf, halfling (race 2, 3, 4); 0.5 s
  limit. Not a thief, P+0xCB == 0 and not racial: TEX 931, bits 0x0C cleared.
  Bits 0x0C set: cleared, TEX 950. A thief or a skill: the armor check
  0xB0AC2 (armor lines 3, 4, 7 to 11: "You cannot perform this action while
  wearing %s."), then roll skill 1. Racial without the skill: those armor
  lines give TEX 951 "You cannot move silently in metal armor.", else success.
  Success and 0xAC2F5() == 0: P+0xA0 |= 4, else |= 8. TEX 952 in both cases.
- Hide In Shadows: no key. 0x9DFDE calls 0xB333F when the speed (|+0xFC|) is
  at most 0x72, P+0xC7 != 0 and the flag 0x14534A is set (set while the speed
  is above 0x72: one try at each stop). Roll skill 0; success and 0xAC2F5() ==
  0: P+0xA0 |= 1, else |= 2. No message. Moving (0x1452F0) or object +0x1AE &
  0xE0 clears 0x03 (0x427DF).
- 0xAC2F5: 1 if an entry of the NPC list (0x3D42CC, count 0x145520) passes
  0xAC297: alive (0xA66C9), not peaceful (+0x1B0 bit 0x200), active (+0x1CC
  == 2), and the player is not hidden from it (0x165C4). No distance and no
  line of sight.
- The AI: 0x165C4 with 0x165F9: the player is hidden when invisible (+0x8A &
  3) or P+0xA0 bit 1; a creature with `blind` (+0x98 bit 1), with the blind
  effect (+0x88 bit 4), or with iq (+0xD4) >= 130 and level (0xAE3BB) >= 10
  still sees the player. 0x13D8E (creature target) and 0x142BF (the player):
  when dot(forward, direction to the target) <= 0, P+0xA0 bit 4 (silent) or a
  deaf creature (+0x88 bit 0x10) means "not noticed", else a roll against iq.
- The footsteps 0x433E0 (sound 2220 + rand % 4 each 0.625 s) are skipped
  while P+0xA0 & 0x0C: a failed sneak is quiet too.
- An attack clears P+0xA0 bits 0x01, 0x02 and 0x10 (mask 0xEC at 0x44402 and
  0x5706E): hiding and searching end, sneaking stays.

### Climb Walls 0x951DE (from Activate 0x95050)

- Activate (key A): 0.5 s limit (0x14224C), ray 0xB30FD(player, 0, 20), the
  trigger check (type 1), the door use; the climb when the door result is not
  3. There is no climb key.
- 0x951DE(seg, side, player): already climbing (+0x1AE & 0x80): return 1. The
  side must be solid (wall_is_doorway 2) or a wall of type 5, with dot(normal,
  forward) <= 0.25 and |distance to the plane| < 10.0. A ladder texture (tmap2,
  or tmap1 when tmap2 is 0, in [lo, lo + 3], lo = the texture "ladder",
  0x13A86B / 0x13A867 set at 0x1C839 and 0xAD616): +0x1AE |= 0x80 with no roll
  and no P+0xA0 bit. Else a wall of type 5: return 0. Needs a thief, P+0xD3 !=
  0, or Levitate or Spider Climb (+0x8A & 0x1020), else TEX 931. Metal armor
  (0xB0AC2): the armor message. Roll skill 4; success, side pad bit 0 clear and
  the item in the hand (P+0x1AD7) not a weapon (byte 0 != 5): +0x1AE |= 0x80,
  P+0xA0 |= 0x20. Else TEX 823 "You cannot get a good grip.".
- The input 0x2C144: forward thrust t (0x1E9A7C): dir = normalize(normalize(
  cross(rvec, (0, 1, 0))) + up) for t > 0; a negative t is applied to the
  vector with - up: forward goes up and into the wall, back goes down and into
  the wall. t >>= 3 with P+0xA0 bit 0x20, else >>= 1 (a ladder). Sideways
  (0x1E9A74) the same along 0x2C48E (not read).
- Each frame 0x9EBEB(player, 10.0) from pcons 0x9DE74: the sides 0, 2, 4, 5 of
  the segment with the same tests and plane distance below 10.0. Nearer than
  4.0: pos += normal * (4.0 - d). Jumping (0x40): 0x9F028 clears 0xC0 and P bit
  0x20; higher than 3.0 above the floor (0x6C30E): accel += normal * 150.0 /
  mass. A ladder: clears 0x20 (falling), the hold stays. Else it needs the
  skill or the spell, pad bit 0 clear and no weapon in the hand; thrust x and
  z * 0.4 (0x6666). When the player is more than 20.0 from the point
  0x145353, the point is updated; higher than 3.0: roll skill 4; fail: TEX 953
  "You just slipped!", +0xF0 = 0, +0x1AE: 0x20 set, 0x80 cleared, P+0xA0 &=
  ~0x20, accel += normal * 50.0 / mass, sound P+0x1B17. No wall: higher than
  3.0: velocity += (uvec + fvec) * 35.0 ("PCONS: push!"); then 0x80 and P bit
  0x20 cleared, "PCONS: you fell!". No damage here.
- The fall damage 0x6C8C8: after a fall of more than 10 feet it sets 0x80 and
  calls 0x9EBEB(player, 20.0); a wall in reach releases the hold and the fall
  damage is skipped.

### Backstab (0xB337E)

- The flag 0x13A955 is cleared at 0x29BDC and set at 0x29D5B by
  0xB337E(target, attacker, weapon), called when the target has 1.0 hit points
  or more. Conditions: weapon >= 0, the attacker is the player, thief_ability
  P+0xC5 != 0, P+0xA0 bit 4 (a made sneak), Weapon_info attack type (+0x12) 1
  (melee), the target `humanoid` (+0x98 & 0x40), and 0xAC2F5() == 0. No angle
  or facing test. 0xAC2F5 is true for any creature of the NPC list that
  lives, is not peaceful, is active and is not fooled by the hiding, with no
  distance and no ray (verified in the bytes 2026-11-23, D-099).
- `Dtu_fix_backstab` (default, -origcombat clears it, AD&D 2nd Edition: the
  thief attacks from behind a victim that is not aware): the player must be
  in the half space behind the face of the target, and only a watcher that
  sees the player (a ray) stops the backstab.
- Effects: hit roll + 4 (0x2AB66); damage x (thief level from 0 + 1 + 7) >> 2;
  weapon 45 with a hit roll of 19 or more: + rand % 4 + 3, "Your backstab was
  very successful!", else "Your backstab was successful!". TEX 937 is unused.
- Turn undead (key T, 0xADC46) and the Spider Piece 0x12 use the cleric level.

## B8. Frame rate rules in these paths

Verified time based (GameTime 0x13CD30 or FrameTime 0x13CD2C), no change needed:
all effect timers (0x8CFFF, 0x9002C), the cast time 0x153EBC (0x41A4D), the fire
rate times 0x215D74 / 0x215D7C (0x56A0D), potion timers (0x37A6A, 0x396B2), the
0.5 s limits of Open Locks and Move Silently (day clock P+0x83 += FrameTime,
0x4D83C), the view loops of Fear and Spider Piece 0x17 (they draw frames until a
GameTime limit), the light object lifetimes (+0x84).

Frame dependent (Code):

1. 0xB3ECE, called through 0xB4040 every frame while search mode (P+0xA0 bit
   0x10) is on and the Find Traps spell is off. Not frame dependent: the wall
   byte +0x16 is set before the d6, so each wall gets one roll for each level.
2. 0xB4040 itself runs every frame, but its trap rolls are stored per level, so
   the result does not depend on the frame rate (except that toggling search
   mode resets the state bytes).
3. 0x9002C runs once per frame per object; it only compares times.
4. Not a frame count but frame time dependent: the Luck test reads the lowest
   bit of the day clock P+0x83 (`P+0x83 % 2`, 0x2A889, 0x2B11F) as a coin flip.

Not found in these paths: a counter of frames for spell effects. The 0x8CEAE
message test is one d20 per cast.

## B9. Open points

- The spell tables of the statistics screen (whether a click there takes a spell)
  were not traced. The hot key path is in character.md, section 6.
- 0xAC2F5 (NPC table loop with 0xAC297) is read as "an NPC watches" (guess).
- Knock (28) has no case code; the wall hit 0x26C2F calls the door function
  0x7E859(seg, side, 1, parent) for it (B6a).

## B10. The spells of the creatures (Code)

- The AI step 0x1490B, each AI frame before the time slicing: the creature
  is active (+0x1CC == 2), has a mage level (Robot_info+0xE2) or a cleric
  level (+0xE3) above 0, is not dead, and its condition word +0x15E is not
  0. If ailp+0x2C (the chosen entry) < 0 and +0x1A8 <= GameTime: 0x92D99(obj,
  distance) chooses. Else if +0x1C0 <= GameTime: 0x91FDC(obj) casts. The
  levels and the points come from the NPC table (ai.md, 0xA504A) or the
  `$MONSTER` keywords `spell_level`, `spell_points`, `cleric_points`.
- The condition bits of +0x15E (writers): 0x1 fire hit (0x26333, 0x2B234,
  0x2B399, guess), 0x4, 0x8, 0x10 the alert levels 2 to 4 (0x16262), 0x20 and
  0x40 targeted by a spell missile with or without `self_cast_ok` (0x5594A,
  0x55953, guess), 0x80 melee hit (0x29D85), 0x100 spell damage (0x91332),
  0x200 wake (0x13F09), 0x400 hit or flinch (0x29FB1, 0x91450), 0x800 an effect
  ended (0x9002C), 0x1000 physics (0x6CB24), 0x2000 slowed in the move
  (0x9F394), 0x4000 failed morale (0x11007), 0x8000 spells recharged
  (0x92E7F).
- The spell table 0x142084: 44 entries of 9 bytes (u8 spell, u16 condition
  mask, u32 effect mask, u16 Robot_info+0x98 mask), in file order the spells
  1 2 3 4 5 6 7 9 8 10 11 13 55 14 15 16 17 18 19 20 21 22 52 24 26 29 30 31
  32 33 34 35 36 37 38 40 42 43 44 45 46 47 49 50. 0x92FF2 ("randomizing spell
  table", from the rest 0x3E1F6) swaps entry i with entry rand() % 44 for i =
  0 to 43.
- 0x92D99: both point pools below 1: refill from Robot_info+0xE4, +0xE5 and
  +0x15F |= 0x80. Scan up to 44 entries from the global index 0x142210 + 1
  (mod 44); skip when (condition mask & +0x15E) == 0, a mage spell (Spell_info
  class bit 2) with mage level or mage points below the spell level, a cleric
  spell (bit 1) with the same test, the creature has an effect of the effect
  mask, or Robot_info+0x98 & the flag mask. Else 0x9231F(obj, distance,
  index): success clears the condition bits of the entry and returns 1. At
  the end of the scan the bits of the last entry are cleared and ailp+0x2C =
  (activity < 2) - 2.
- 0x9231F (jump table 0x92243): a condition per spell and a weight, clamped
  to 0 to 100; the spell is chosen when `rand() % 100 + 1 <= weight`. Then
  ailp+0x2C = index, ailp+0x2D = 1, +0x1C0 = GameTime + cast time (+0x04, integer
  seconds, the value of 0x13A8DB). `clear(r)` = 0x10815: an fvi ray with radius r from the eye (y + 5
  unless flying) to the player; `safe` = 0x91F66: no other hostile creature
  in the segment of the player. D = distance >> 16, L = 0xAE3BB.

| Spell | Condition | Weight |
|---|---|---|
| 1 Aid | 1 | 40 + r % 41 |
| 2 Armor | AC (Robot_info+0x76) > 6 | 32 AC - 192 |
| 3 Bless, 38 Prayer | 1 | r % ((32 - L) 4) (100 for Bless if spell# is 14) |
| 4 Burning Hands | clear(30.0) | 50 + r % 50 (term 0 if the player has 0x40600) |
| 5 Chill Touch | distance < 200.0 | 100 - D |
| 6 Color Spray | clear(30.0) | 25 + r % 75 (term 0 if the player is blind) |
| 7 Confusion | safe, the player not confused | 25 + r % 26 |
| 8, 9, 30, 31 Light | the player not blind | 25 + r % 51 |
| 10, 11 Cure | hp (+0x6C) < max - 4 / max - 10 | 100 - 100 hp / max |
| 13, 55 Dispel | the player has effects & 0xF3FFBECB or the creature & 0x0C004124 | no roll: `self_cast_ok` is set, 0x91FDC at once, the bit cleared, return 0 (the scan goes on) |
| 14 Fear | the player has no Fear | 25 + r % 26 |
| 44 Scare | the player has no Fear | r % (50 - 16 L) + 50 (player not elf, half-elf or cleric) |
| 15 Feather Fall | 1 | 0x6C30E(obj) >> 16 (guess: the height above the floor) |
| 16, 17 | safe and (own fire protection, fire immune or resistant, or D > `radius`) | 25 + r % 26 |
| 18, 40, 43 | +0x15E bit 0, or spell# has fire dice | 20 + r % (40 b + 40 t) |
| 19 Fly, 29 Levitate | the player flies or levitates or is 20.0 or more above (19 also: retry count) | Fly 20 + r % (40 fleeing + 40 retry); Levitate 25 + r % 51 |
| 20 Free Action | slowed, stuck (0x2000), fleeing, or spell# in 6, 22, 52, 23 | 50 + r % 51 |
| 21 Haste | slowed, stuck, fleeing or the player hasted (sets +0x49 = L) | 40 + r % 41, 100 if slowed |
| 22, 52 Hold Person | the player not held and no Free Action | 50 + r % (D % 50) |
| 24, 26 Invisibility | 1 | 100 - 100 hp / max + r % (50 (segment not visible)) |
| 32 Lightning Bolt | 0xB30FD(obj, 1, 160): no wall within 160.0 | 50 |
| 33 Magic Missile | clear(5.0), the player has no Shield | 50 |
| 34, 37 | clear(30.0) | 50, 30 |
| 35 Vestment | AC > v, v = max(5 - (L - 5) / 3, 1) | 8 (AC - v) |
| 36 Acid Arrow | 1 | 50 |
| 45 Shield | spell# 33 or AC > 3 | 16 AC - 48 + r % (50 [spell# 33]) |
| 46 Sleep | safe, the player not asleep | 100 - 12 L |
| 47 Slow | the player not slowed | 50 + r % (50 if the player is hasted) |
| 49 Stoneskin | 1 | 25 + r % (25 (hp < max) + 25 (player no caster) + 25 (late)) |
| 50 Strength | D > 3 | D |

  "spell#" is the byte 0x153EDC (ailp+0x2C of object 0); no writer was found
  (guess: it stays -1). Not cast ("bad spell"): 12, 23, 25, 27, 28, 39, 41, 48,
  51, 53, 54. Some weights take `rand() % 0` or a negative divisor; the
  executable installs the Descent divide handler in saturate mode (0xF72AB,
  called with 1 from 0xB8328), which returns 0.
- 0x91FDC, the cast: `self_cast_ok`: 0x90CEF(spell, obj, obj) for an area
  spell, else 0x8CFFF(spell, obj, obj), then 0x8CAD3. Else 0x56FF6(obj, spell,
  template 3 (the wand weapon), hand 1) (guess: the missile goes along the
  facing of the creature). The spell level is taken from +0x19E (mage) or
  +0x19F (cleric). ailp+0x2C = -2 if Robot_info+0x62 == 1 and the creature is
  active, else -1. Both pools below 1: both 0 and +0x1A8 = GameTime + (9 -
  mage level - cleric level) * 60.0; a creature with a mage level also gets
  ailp+5 |= 0x32, mode 4 and +0x143 = 1 (guess: it runs from the player).
- The caster level of a creature is 0xAE3BB (max hp / 8, 1 to 18) in 0x8CFFF
  and in 0x56FF6 (0x57079), not the mage level. All strings of 0x92D99,
  0x9231F and 0x91FDC go to the debug log 0xD7B00.

# Part C. The port

Files: `engine/main/dtucombat.c`, `dtucombat.h` (Part A), `engine/main/dtuspell.c`,
`dtuspell.h` (Part B). The placeholder attack of `dtuobj.c` (`dtu_player_attack`) is no
longer called: `controls.c` calls `dtu_combat_attack_key()` and `dtu_combat_frame()`,
`game.c` calls `dtu_combat_player_attack()` for the fire button of the key table.

## C1. The weapon attack

- Input: Left Ctrl, Right Ctrl and the `-testmove` action `attack`, on the key down
  (0x2FCBE). The mouse button goes through the fire button of `game.c`. A misc item in
  the hand is used, not swung (0x2FAF0).
- Order of a frame as 0x435A6: the animation of the arms (0x4450C), the hit test of the
  swing (0x6CFF8), the start of an attack (0x44300 with the gate 0x41A4D).
- The swing time is `max(fire_wait, 1.0)`, two times with Slow (0x441E9). The gate needs
  more than one swing time since the last start and the key still down. The attack is
  always state 6. The animation frame is computed from the time as in 0x4450C, a state
  time below 1.0 does not animate (but the halved swing of Haste does). The timing
  rules, the rate of the missile weapons and the items of speed: section A8,
  `dtuatk.c`.
- The hit test runs on frames 6 to 8 of the state: the weapon sphere of the frame
  (`weapon_center`, `weapon_radius`, 8 times for the dagger and short sword numbers),
  from the body position moved forward by the square root of 2, against the walls and
  the bodies of creatures and scenery. A hit on frame 6 or 7 ends the swing. A wall that
  is not side 1 or 3 ends the swing and touches the trigger of the side
  (`dtu_trig_touch`, type 4).
- The body test of the swing is the object test of the query of the executable
  (`dtu_body_query_test` in fvi.c, A3 step 6): a capped cylinder about the position of
  the object with the radius `body_radius[0]` and the half height `torso_half_height`
  of the frame (`body_half_height[0]` without the torso box) plus r. The walls take the
  radii of the swing query (`Fvi_dtu_body` 4 in fvi.c). The nearest contact wins, as in
  fvi_sub. Before this the port used its general body test (`dtu_object_hit_test`): the
  torso cylinder with r added to its radius, and any start point in it a hit at
  distance 1, so the object with the lowest number won. With the dagger (r about 17)
  the swing hit scenery and creatures beside and behind the player, and a creature in
  front (the giant rats of BONES) lost to them, so the target hint never showed it.
  The creature swing (dtuobj.c) still uses the general test.
- The hit (0x278B5, 0x29A9C): the sounds of the contact, the to-hit roll of the
  character (`dtu_char_attack_roll`) against the armor class of the creature with its
  spell effects and the bonus of the effects of the player (Bless, Strength, Slow), a
  sleeping creature is hit and wakes, `weapon_resistant`, Stoneskin, the damage roll of
  the character (a second roll on a critical hit), Chill Touch, then
  `dtu_damage_creature_direct` of `dtuobj_combat.c` (message, flinch, death, experience). A push
  of 20.0 divided by the mass is added to the thrust of the creature. Hit and miss
  vclips and sounds of the weapon.
- Missile and thrown weapons: the attack plays state 6 and the missile leaves at the fire
  frame of the model (0x56A0D) from the position plus 2.0 up, along the view with the
  look pitch, with `dtu_create_missile` of `dtuobj_missile.c` (flight, hit, damage). One item of
  the stack in the hand is used (`dtu_char_use_up`, 0xB7084). An arrow needs a bow, a
  bolt a crossbow. A thrown stack that is used up gives the empty hand and arms the
  first hot key (0x4448D).
- The arms: the model `<archive><letter>_p.pof` with the fallbacks of 0x43A5A, drawn by
  `dtu_combat_draw_view()` from `render.c` after `render_mine`, at the player position
  plus `1.2207 - 16 * look_pitch` up and moved back when looking down (0x67C5A), with
  the orientation of the player object (no pitch), in the frame of the arm animation. A
  thrown weapon is hidden from the fire frame on. The executable draws the player object
  in the painter order of its segment; the segment of the viewer is drawn last, so the
  picture is the same.
- The setting `Dtu_improved_body` (Improvements, on by default, not in the executable).
  The arms model is the upper body only (root 0, chest 1 with flag 0x02, right arm 2 to
  5 with the weapon in the hand part, left arm 7 to 10, an end marker with flag 0x04 at
  the end of the weapon). With the setting on, the model is placed as at look pitch 0
  and turned with the view pitch about the eye (without the bob), with the view matrix
  as its orientation: the arms stay at the same place on the screen at every pitch and
  no point of the arms is further from the eye than at pitch 0. For a model whose weapon
  (the middle of the hand joint and the end marker) is in the view in no frame of the
  attack state, the arms rise along the view up vector during the swing, so that the
  weapon of the frames 6 to 8 is at 0.6 of the half height under the center of the view
  (rise over 3/16 of the swing, fall over the last 4/16). Computed values: short sword
  1.42 and dagger 1.32 for the human, elf and K models, 2.62 and 2.52 for the D and F
  models (also warhammer 1.05 and empty hand 0.73 there); all other weapons 0. The hit
  test, the swing time and the missile start do not change. Off: the rule of 0x67C5A.
- The setting `Dtu_target_hint` ("Target hint" in Improvements, `TargetHint` in
  `ascent.cfg`, on by default, not in the executable; `-notargethint` turns it off
  for one run). In each frame `dtu_combat_frame` tests the attack of the item in the
  hand with the code of the attack itself and with no effect:
  - melee weapon or empty hand: `swing_test` (the sphere test of 0x6CFF8 that
    `swing_hit_test` uses) for the hit frames rel 6 to n/2 of state 6, in order. The
    first frame with a contact (an object, or a wall that is not side 1 or 3)
    decides, because the swing ends there.
  - missile or thrown weapon: the start of the shot (`missile_start`, the position
    plus 2.0 up, the view with the look pitch), then `dtu_missile_path_target` of
    dtuobj_missile.c: the wall query and the body test of `missiles_frame` (shared
    `missile_path_object`) along the line for speed times life. No hint without
    the bow or the crossbow that the ammunition needs.
  - spell, scroll, wand: `dtu_spell_hint_target` of dtuspell.c: no hint for a spell
    on the caster or a spell of a book that the checks of 0x356E0 refuse; the homing
    target of `cast_now` if there is one; else the line of the missile (template
    weapon, size of the spell model, spell speed times the missile life).

  The hint is on when the first object is a creature that the attack can hurt:
  hostile (`dtu_creature_is_hostile`), 1.0 hit points or more, not dying, and not
  `weapon_resistant` against a weapon without `magic` (the rule of 0x29D9D; not
  applied to spells). Scenery, a corpse, a creature that is not hostile or a wall in
  front turns it off. The cursor of the play screen (dtudisp.c `draw_cursor_main`)
  then has its colors moved to red (green times 0.62, blue times 0.48), matched to
  the nearest color of the palette, so the frame stays 8-bit and the tint shows with
  both renderers. The cursor that holds an item and the cursor over the
  conversation window are not tinted. The port draws no cursor in the immersive
  mode; there the tinted cursor shows at the center of the view only while the
  hint is on (checked in BANE, `-testpos 25,-1,235,0`, V in the script: the mark
  shows in reach and not out of reach, both renderers). Spread missiles (Burning Hands without a target)
  and movement after the test are not predicted. A large body (the beetle, radius
  11.8) reaches through a thin wall: the swing touches it and the hint shows it.
  Checked in BANE (`-noattack`, `-objlog` line `combat: target hint`): long sword
  at `-testpos 25,-1,235,0` hint 1 and the swing touches the skeleton (roll missed),
  at `25,-1,231,0` hint 0 and no contact (233 is in reach with the body test of the
  executable, see C5); dart at `25,-1,215,0` hint 1 and the dart
  hits the skeleton; dart at `-160.7,14.6,162.7,0` hint 0 and the dart hits the
  wall; Magic Missile (SG0.SG) at `25,-1,215,0` hint 1, the missile homes on the
  skeleton; Burning Hands at the wall hint 0, the missiles hit the wall.

### The swing at the look pitch and a committed creature attack (2026-11-23)

- `Dtu_fix_swing_pitch` (dtucombat.c `swing_test`, on by default). Each hit
  frame keeps the sphere test of 1.3 and adds the view ray: a ray from the eye
  (the center of the body plus eye_offset) along the view with the look pitch,
  as long as the distance from the eye to the end of the swing ray of that
  frame at pitch 0, with the vertical radius r of the sphere, as the swing
  query. A wall, the floor or the ceiling before the body stops it. The ray
  enters a body through its top (the rule of `Dtu_fix_body_ray`). The sphere
  wins when it touches an object. Looking up, the sphere and its ray turn with
  the pitch about the eye; a wall between the eye and the turned center moves
  the center to 0.25 before it. Looking down the sphere stays: turned, it would
  go under the body. At pitch 0 the sphere is the one of 1.3, so each creature
  that the level swing of 1.3 hits is still hit. The target hint uses the same
  `swing_test`, so it shows exactly what the swing hits at each pitch.
- The negative case: a low body (its top more than 4.0 below the eye) in front
  of the feet is not hit by a swing at the ceiling with a Broad Sword or a Mace;
  1.3 hits it when the level swing reaches it. The Dagger and the Short Sword
  keep their vertical reach of about 17 at every pitch, so they still hit it.
- `Dtu_fix_attack_commit` (dtuobj_combat.c `weapon_test`, on by default). 1.3 and the
  port before: a creature starts a melee attack when the player is within both
  radii plus 2.0 (0x1599F); the hit and the roll against the armor class
  (0x27D82, 0x29A9C, 0x2A7F3) come only when the weapon sphere of a model frame
  touches the player, from frame 8 of the attack state to its end (0x6CFF8 from
  `monster_physics` 0x9F2B4). A player who steps out of the sphere before that
  takes no roll: a step back avoids each melee attack. Corrected: in each frame
  before frame 8 the attack is tested; with the player in reach, in front of the
  creature (facing dot 0.75, the test of 0x1396E) and no wall between the
  centers, it is committed. When the sphere misses, the roll is made at the
  first hit frame with the player out of reach, or at the last two frames of the
  state. It is lost if the creature dies, is held, or loses the player
  (invisible, hidden in shadows), or if the player is farther than twice the
  reach or behind a wall or a closed door at that moment. The walk speed limit
  is 49.3 and the hit comes about half a swing (0.5 s) after the start, so a
  player who backs off at once covers some 10 to 20: twice the reach (about 28
  for a Zombie) keeps the hit for a step back and loses it for a run away. The
  touch spell of a lich (0x2A637) follows each melee hit that does damage, so a
  committed hit casts it too. A missile is an object: a player who leaves its
  path is not hit. The touches of `ai_fire_laser_at_player` (0x11B06, a special
  attack or a spell caster in reach) are made at once at a fire frame, with a
  reach test at that moment: no change.
- The swing of the player keeps the rule of 1.3: the sphere test of the frames
  6 to 8 (6/16 to 9/16 of the swing). A creature that steps out of the sphere
  before that is not hit. The window is short, the hint shows the target before
  the swing, and a creature chases the player to its stop distance, which is in
  the reach, so a commit for the player would change little.
- Test aids: `-chartest reach=N.M` (the body turns with the heading only; M 0
  looks at the center of creature N from the eye, 1 level, 2 fully up, 3 fully
  down; the log gives the swing of each frame, the contact, the contact of 1.3,
  the hint), `pitch=P` (the look pitch, fixang, + is down), `stepback=N.D` (when
  the next attack of creature N becomes valid, the player moves D away).
  `tools/check_reach.py` has a pitch pass, a level pass and a negative pass,
  `--weapon` and `--commit`.

### The long sword in the middle of the view (2026-11-23)

Verified in the code and the models:

- 0x43A5A builds the name of the arms model from the letter of the item in
  the hand (byte 5 of the inventory record, `weapon_char`, 0x43EB2 to
  0x43F96). Every long sword of the tables (weapons 6, 13, 14, 32, 33, 71)
  has the letter `g`, so the plain Long Sword and the Long Sword +1 use the
  same model (`hmcg_p.pof`, `hftg_p.pof`). The Broad Sword is `h` in all 46
  tables.
- 0x67C5A draws the player object 0x13880 above its position (less 16 for
  each unit of look pitch), with no sideways offset. 0x4450C puts the arms
  into state 1 (STAND) when the velocity is 0.
- 0x729C1 takes the center of the body box of the frame away from the
  position. The body box of the `g` models has another center (the long
  blade is likely in the box): in the stand frames (1.27, 1.20, 1.58),
  against (0.43, 0.28, 1.06) for `hmch_p.pof`. So the whole arms model is drawn 0.85 further left and
  0.5 nearer. The middle of the blade (between the hand joint and the end
  marker, submodel flag 0x04) is 0.07 left of the eye and 0.3 in front of
  it: the blade stands upright in the middle of the view.

The middle of the weapon right of the eye in the stand and walk frames
(`hmc` models): long sword -0.07, bastard sword 0.39, mace 0.60, hand axe
0.58, broad sword and short sword 0.69, club 0.70, scimitar 0.85, dagger
0.92.

`Dtu_fix_sword_view` (1, `-origcombat` clears it): for a melee weapon in
the hand, outside the attack states, the arms model moves right along the
right vector of the view by the largest `0.69 - x` of the stand and walk
frames (the long sword 0.76, the bastard sword 0.31, the hand axe 0.12, the
mace 0.10). The measure is the offset, not x/z, because the middle of the
long sword is nearly at the eye. The shift goes out over the first 3/16 of
the swing and comes back over 0.25 s after it; from 3/16 to the end of the
swing the picture is that of 1.3. The hit test and the swing time do not
use the drawn place. `tools/check_sword_view.py` reads the measure in the
log.

## C2. Spells

- Books: the two spell lists of a save game of the original (`-char FILE`). New spells
  go in with `dtu_spell_learn` (0xB69AD). Slots: the maximum from the tables 0x145CD8 and
  0x145DF8 with the wisdom bonus, full at each level start and at a rest
  (`dtu_char_rest` calls `dtu_spell_restore_slots`).
- Selection: as the executable, the hot key boxes hold spells (the key, a click on the
  box), and the button SPELLS of the inventory screen shows the spell lists; the cursor
  takes a spell into a box or onto the figure of the paper doll (character.md, section
  6). In a spell list comma and period choose a spell and Enter arms it (port keys).
  The statistics screen also shows the books and the slots in the empty panel at the
  lower left (a port addition; the original layout of the spell tables was not
  decoded): comma and period choose, Enter takes the spell in the hand. Taking an item
  in the hand puts the spell away. The model of the arms of a spell of a book is the
  one of letter `K` (byte 5 of the entry, 0x43EF8).
- Cast: the attack key with a spell in the hand: the checks of 0x356E0 and 0x41A4D with
  the messages, the cast time (integer seconds of game time, 0x41C9F, from the table
  0x13A8DB that the fixup 0x1A344 writes over `cast_time=`; Armor waits 60 s), then the
  cast and one slot.
  A scroll (a spell item in the hand) and a wand (a weapon with `pc_spell`) cast at once
  and use one item or charge.
- `self_cast_ok` spells are cast on the player (Prayer and area spells as an area around
  him). The others fire missiles with `dtu_create_missile` (templates 2 hands, 3 wand, 4
  scroll, 5 arrow), the speed of the line (default 100.0), the number of 0x56FF6.
  Homing missiles turn to the target (the rule of the turn is a guess).
- Effects: `dtu_spell_hit` (the strong definition of the weak function of `dtuobj_combat.c`)
  runs the case of 0x8CFFF, or the area 0x90CEF for an area spell. An area spell whose
  missile hits a wall or ends has its area at the last point. The area test uses a ray
  in place of the brain table.
- Duration effects are bits of `dtu.m_88` with absolute game time timers in the 124 byte
  block (0x8C to 0xF8); `dtu_spell_frame` ends them for the player and the creatures
  (0x9002C) with "NAME expired.". The movement reads the bits through `physics.h`
  (`DTU_OF_`), the armor class through `dtu_char_update` (`dtu_spell_player_ac`).
- Saving throws: 0xAE596 with the tables of B5. The key of the cleric level of a
  creature (`cleric_level`) is a guess.

### Armor (spell 2), Code

- Cast (0x8D4E7): nothing if bit 0 of +0x88 is set already. Else bit 0 and the pool
  +0x90 = (8 + L) << 16, L the caster level (0x91EC1 for the player: the mage level
  for Armor). For the player 0x1BD5A4 |= 0x10 (a dirty flag of the display areas).
  No timer is set.
- Message (0x8D524 to 0x8D584, with 0x347A2 into the message box, only when 0x8CEAE
  gives 1: the player is the target or the caster, else a perception roll): the
  caster on himself `"%s %s %d armor hit points."` (0x12D928) with the name of the
  target ("You" for the player) and "have" (player) or "has"; else `"%d armor hit
  points cast on %s by %s."` (0x12D943) with the target and the caster ("You" for
  the player). The player casting on himself reads "You have 9 armor hit points."
  at level 1. There is no "for N seconds" text.
- Armor class: 0xB05D9 (player, before the dexterity adjustment) and 0x2ADF9
  (creature) cut the armor class to 6.
- Damage: the pool is lowered by the full damage (the hit points take it in full as
  well; Armor does not absorb) at 0x29F3E (a weapon hits a creature), 0x2A4FB (a
  weapon hits the player) and 0x913AF (spell damage on a creature, after the save).
  A pool below 1.0 becomes 0. The branch of spell damage on the player (0x917xx) has
  no such step.
- End (0x9002C, each frame): bit 0 set and the pool 0 or less (or Dispel Magic):
  bit 0 cleared, 0x1BD5A4 |= 0x10, the creature condition bit +0x15F |= 8; the
  player gets "Armor expired." (0x12E385, 0x347A2) and 0xAFB55 (the armor class), a
  creature 0x10ADF.
- Port (dtuspell.c case 2, `dtu_spell_armor_hit`, the weapon hits of dtuobj.c and
  dtucombat.c): as read. The message is printed when the player is the target or the
  caster, and for two creatures by the rule of 0x8CEAE (spells.md).
  Whether 0x2A4FB also takes damage with no attacker and no weapon (traps) was not
  read: the port lowers the pool for weapon hits only. Checked in BANE (`-level
  bane.lvl -char SG0.SG`, the Elflama save at mage level 9 by `-chartest xp=300000`,
  `-testpos 25,-1,235,0`, key 5 and the attack key): "You have 17 armor hit points.",
  armor class 5 to 2, three Ghoul hits of 6, 9 and 6 take the pool 17 to 11, 2 and 0,
  "Armor expired." in the next frame, armor class 2 to 5.

### Spells in the port

Complete as read: 1 Aid (the hit points are capped at the maximum), 2 Armor, 3 Bless, 4
Burning Hands, 5 Chill Touch, 6 Color Spray, 7 Confusion, 10 Cure Light Wounds, 11 Cure
Serious Wounds, 12 and 54 Detect Magic (the effect bit only, the renderer does not read
it), 15 Feather Fall, 16 Fireball, 18
Flame Walk, 19 Fly, 20 Free Action, 21 Haste, 22 and 52 Hold Person (no counter of
targets), 23 Hold Undead (no pool), 24 Improved Invisibility, 25 Infravision, 26
Invisibility, 27 Jump, 28 Knock (no effect, as in the executable), 29 Levitate, 30 and 31
Light, 8 and 9 Continual Light (blindness and a light object), 32 Lightning Bolt
(not the scenery part), 33 Magic Missile, 34 Magical Stone, 35 Magical Vestment, 36
Melf's Acid Arrows, 37 Melf's Minute Meteors (not the scenery part), 38 Prayer, 39 and 53
Protection From Evil (the bit only: the to-hit rules are in the creature attack), 40
Protection From Fire, 42 Cure Blindness or Deafness, 43 Resist Fire, 45 Shield, 46 Sleep
(no hit dice pool), 47 Slow, 48 Spider Climb, 49 Stone Skin, 51 Find Traps (the bit
only).

Added with the item and thief work: 0 Abjure (as read, the spin of the creature is
a stop in the port; the creature that is sent away is removed), 41 Rary's Mnemonic
Enhancer (the choice box is `nm_messagebox`, `-autochoice N` answers it), 13 and 55
Dispel Magic on missiles, potion objects, scenery and light objects, 14 and 44 the
weapon drop (0xAE837, 0x3C4BD) and the save rule of Scare, 16, 17, 32, 37 on scenery
(0xA6ACB with the saves 5 and 6), the damage of 4, 33, 34, 36 on scenery
(`damage_percent` * size, 0x3CA7E without debris), 50 Strength with the gain, the
interruption of a cast (0x9305D: a weapon hit on the player, spell damage without a
save, the jump key; a melee hit or spell damage on a casting creature), the creature
spells (B10, `dtu_spell_creature_frame`), the caster level of 0x8CFFF (the player
0x91EC1, a creature 0xAE3BB) for every hit, the missile life of 0x56FF6 and the area
of an area spell at the wall hit (0x26C2F; the area at the end of the life is gone).

Fear (14) and Scare (44) on the player move the view into a gated creature of type 0
(`dtu_fear_view_start`, `dtu_fear_view_end` in dtucombat.c; the port finds the free place
at the center with the size test of `dtu_gate_in_creature`, not the model radii of
0x13384, and leaves the player in place if the creature is gone at the end).

Still partial: the
counters of Hold Person, Hold Undead and Sleep, the cast vclips of `cast_on_top`
(0x8CCED), the debris of 0x3CA7E. The audit of all spells against the executable
(cast time, texts, end texts) is [spells.md](spells.md).

Items (B6, `dtuchar.c`): all potions 0x48 to 0x58, the Gem of Curing (the counter
0x13E588 is not decoded: the port allows one use for each rest, guess), the Spider
Pieces 0x13, 0x16, 0x19, 0x1A (0x12, 0x14, 0x17, 0x18 are not in the port), the Cursed
Scroll, weapon 13, armor 45, the Orb of Insight and weapon 13 through the weak hook
`dtu_hook_script_event()` (dtuscript.c may give it), the potion timers 0x396B2, Luck,
the worn item effects of B6a, the Ring of Wizardry. The text screen of the Scroll
(0x9DC42, `dtuscroll.c`). Scrolls of spells and wands
cast as in C2.

Thief skills (B7, `dtuthief.c`): Open Locks (key O), Find/Remove Traps (key F, the box
with `dtu_char_choice`), Move Silently (key M), Hide In Shadows (one roll at each stop),
Climb Walls (Activate on a solid side, the thrust of 0x2C144 and the frame 0x9EBEB in
`physics.c`), the backstab, the slow move while sneaking or searching, Hide and Sneak in
the sight of the creatures (0x165C4, 0x13D8E). Not in the port: the racial search for
secret walls (0xB3ECE), the marker object of the Find Traps spell, the side pad bits
of found traps, the fall damage skip of 0x6C8C8. The thief point screen (thiefpts.c) is
in `dtuthiefpts.c`.

Scenery (ai.md): the break by a melee hit (0x6D4EC), by a blocked creature (0x14CA9)
and by spells, the push by the player and by creatures (0x9FA41, 0x9E5BE) and the move
of pushed scenery (0x9FE68, simplified: gravity to the floor, a stop at a wall).

The arms of the player ask for the state WALK while the player walks and STAND when he
stands (guess for the rule: the controls of the executable request 2 each frame at
0x2BFF2, 17 in flight, 10 with the missile flag 8; the stand request was not found).
The dead code `dtu_player_attack` and `player_animation` of dtuobj.c is removed.

The audit of the combat rules ([../audit/combat.md](../audit/combat.md)) added the
effects of 0x2A7F3 and 0x2AF5E for every attacker (`dtu_hit_test`,
`dtu_weapon_extra_damage` in `dtuobj_combat.c`), the protections of the player in
0x2A24A, the fumble texts, Flame Tongue, Mace of Disruption, the fire dice of
weapons, the strength 18 columns and the invisibility in 0x165C4. Its table lists
what is still open.

## C3. Settings

All are global variables, 1 by default; the option `-origcombat` clears them.

| Setting | File | Rule of the executable (0) | Corrected rule (1) |
|---|---|---|---|
| `Dtu_fix_one_hit` | dtucombat.c | a first contact on frame 8 rolls once in each game frame while frame 8 lasts (frame rate) | one roll for each swing |
| `Dtu_fix_haste_attack` | dtucombat.c | Haste adds half a swing time to the wait (Haste is slower) | Haste halves the swing time (guess of the intent) and, with `Dtu_fix_missile_rate`, the wait of a missile weapon |
| `Dtu_fix_missile_rate` | dtucombat.c | no wait between the shots of a missile or thrown weapon: each key press fires (0x56BB0) | `fire_wait` from shot to shot, two times with Slow (A8) |
| `Dtu_fix_rat_hit` | dtucombat.c, fvi.c, dtuobj.c | no swing, missile or spell missile of the player reaches a rat: the "Rat" of each table has `pass_thru`, 0x6A102 sets its object flag 0x40 at each load, and fvi_sub skips that flag (0x3F38F). A rat dies only when it runs into a player with boots (0x26FE0, see objects.md "Rats") | the swing, the missiles and the target hint of the player reach a rat; the player still walks through rats |
| `Dtu_fix_speed_items` | dtuchar.c, dtucombat.c | the Short Bow of Speed and the Short Sword of Quickness have no power (`pc_spell1` is no keyword, `PC_spell` of a melee weapon is not read) | the arrows of the bow in the bow slot and the swings of the sword in the hand are twice as fast; no Haste effect, not more with Haste, normal with Slow (A8) |
| `Dtu_fix_body_ray` | dtucombat.c, fvi.c, dtuobj.c | the body test of a missile (query 0xA05AD, 0x3EE70, `body_query` 0x4F7D5) fails in the infinite cylinder test 0x4F4B6 when the entry distance is below 0, before the caps clip the ray: a missile that starts above a body and within its radius misses it (31 creatures of MINE in `check_reach.py --origcombat`) | for a missile the caps decide: the ray enters through the top of the body (`check_reach.py`: no miss in MINE, BONES, CLAN3) |
| `Dtu_fix_swing_pitch` | dtucombat.c | the swing ignores the look pitch (0x6CFF8, 0x7473F use the orientation +0x20): a low creature close to the player and at the center of the screen is not hit when the player looks down at it | the view ray adds the body at the center of the view within the reach of the frame; looking up the sphere turns with the view (C1) |
| `Dtu_fix_attack_commit` | dtuobj.c | the roll of a creature melee attack is made only when its weapon sphere touches the player: a step back during the animation avoids it | an attack that starts with the player in reach, in front and in the open is rolled even if the player steps back, unless the player is farther than twice the reach, behind a wall, hidden, or the creature dies or is held (C1) |
| `Dtu_fix_speed_items` | dtuchar.c | the Short Bow of Speed and the Short Sword of Quickness have no power (`pc_spell1` is no keyword, `PC_spell` of a melee weapon is not read) | Haste while the bow is in the bow slot or the sword in the hand (A8) |
| `Dtu_fix_chill_touch` | dtuspell.c | not a correction: the table fixup 0x1A344 casts Chill Touch on the caster in 1.3 (0x1A65E, 0x56D36); `-origcombat` does not clear it | the cast is on the caster also with `-rawtables`: Chill Touch for L + 3 minutes |
| `Dtu_fix_stoneskin_word` | dtuspell.c | Stone Skin clears all other effect bits | only its bit is set |
| `Dtu_fix_rary_manual` | dtuspell.c | one cast (bit 22), three levels, no second cast, the bonus stays until Dispel Magic | the manual: each cast adds three levels, the first rest after the cast fills them, the next rest ends the bonus (the end at the entry of Waterdeep is not in the port) |
| `Dtu_fix_melf_acid` | dtuspell.c | Melf's Acid Arrows (0x8EFE0): no attack roll, 4 L acid in one hit, save for half | the AD&D 2nd edition rule: an attack roll with the THAC0 of a fighter of the caster level, 2d4 acid on a hit and 2d4 at the start of each of the next L/3 rounds of 60.0, no save (spells.md, section 10) |
| `Dtu_fix_wizardry_ring` | dtuchar.c | the Ring of Wizardry doubles the slots of the levels 1 to 3 each time the equipment changes, halves them once at the end | one doubling while it is worn |
| `Dtu_fix_potion_frame` | dtuchar.c | the intelligence potion rolls 1 in 63 in each frame | one roll for each reference frame (3/64 s) |
| `Dtu_fix_luck_coin` | dtuchar.c | the Potion of Luck takes the lowest bit of the day clock (frame times) | rand() & 1 |
| `Dtu_fix_cure_aid` | dtuspell.c | Cure Light and Cure Serious Wounds cut the hit points to the maximum after the addition (0x8DEE2), so they take away the hit points of Aid | a cure never lowers the hit points |
| `Dtu_fix_turn_undead` | dtucombat.c | turn undead clears bit 0x10 of the next undead before the count test (0xADF09), and a T writes 0xFF over the d20 (0xAE1B8) | the bit and the d20 stay (bugs.md T2, T6) |
| `Dtu_fix_cursed_scroll` | dtuchar.c | the Cursed Scroll does rand() % (hit points as a fix) damage, below half a point (0x38159) | rand() % hit points, whole points |
| `Dtu_fix_creature_missile` | dtuchar.c, dtuobj.c | a creature missile adds the bow or crossbow bonus of the player and can use up the Potion of Luck of the player (0x2B0AE, 0x2B107) | only the missiles of the player |
| `Dtu_fix_sword_view` | dtucombat.c | the arms of a long sword stand in the middle of the view (C1, "The long sword in the middle of the view") | at rest the arms move right until the middle of the weapon is as far right of the eye as that of the broad sword; out in the first 3/16 of a swing |

With the option `-origobj` (dtuobj.c, 1 by default):

| Setting | Rule of the executable (0) | Corrected rule (1) |
|---|---|---|
| `Dtu_fix_magic_tint` | the tint of Detect Magic stays on after a magic bitmap or rod object for the next polygons, and the linear texture mapper tints only the first scanline after a change, near black (spells.md, section 8) | the tint is for the object only, both mappers draw the blue ramp |
| `Dtu_fix_cant_break` | the keyword `cant_break` is ignored: every scenery object breaks (portals, coffins) | `cant_break` keeps the object whole |
| `Dtu_fix_push_body` | the move of pushed scenery stops when its center is inside of the body of the player (twice the radius of the move query of the player plus half of the radius of the scenery, 0x3EE70 and 0x4F7D5), and the player stops closer than that to a chair unless its last step was long: a chair moves only below about 30 frames per second, and then only now and then | a body that the scenery starts in does not stop a move away from it; the player is as large as its move query; other scenery is the box of its model; the push of other scenery needs the strength of the first push (objects.md, "Pushed scenery") |
| `Dtu_fix_push_rate` | the scenery ends a push about 1.84 steps of the mover ahead of where the mover stopped, and a step is the velocity times the frame time, so the lead is 4 units at 20 frames per second and 0.4 at 200 | at the end of a push the scenery slides the difference to the lead of the reference frame (DTU_PUSH_LEAD 1.3 steps of the reference frame, measured) |
| `Dtu_fix_push_wall` | pushed scenery tests the walls with its center and half of `body_radius[0]` (walls `rad + 1.0`): the end of a table goes 3.4 into a wall; the port before this used `size / 2`, and in the test of objects.md the center of a pushed table stopped at the wall | the corners and the mid points of the edges of the box of the model stay 0.25 out of the walls |

## C4. Test options

`-autochoice N` answers the choice boxes (Rary's Mnemonic Enhancer, the removal of a
trap) with N. `-chartest` commands added: `give=TYPE.ID.COUNT`, `skill=N.VALUE` (a base
thief skill, 0 Hide to 4 Climb), `fx=BIT.SECONDS`, `cast=SPELL` (on the player),
`restreset` (the rest places work again). `-testmove` actions added: `picklock`,
`search`, `sneak`. `-charlog` has the lines `thief:`, `climb:`, `use:`, `potion:`,
`choice box`; `-objlog` has `cspell:`, `push:`, `npc table:` and the breaks.
`-testspell N` puts spell N into the book of the character and into the hand.
`-testslots N` gives N slots for each spell level. `-objlog FILE` has the lines
`combat:` (model, swing start and end, test frames with the sphere, hits with the
result, armor class, damage and hit points) and `spell:`/`save:`. `-charlog FILE` has the
d20 of each attack roll.

## C5. Verification

- Screenshots (BANE, `-testpos 25,-1,205,0`): at rest the long sword stands at the right
  edge of the view, close to the eye (frame 0 of `hmch_p.pof`); in the middle of the
  swing (frame 6) the blade crosses the lower middle of the view towards the skeleton.
  With SG0.SG the dagger model `hmcd_p.pof` and the blue Magic Missile (`miss2.pof`) in
  flight to the skeleton.
- Fight, `-level BANE -noattack -testpos 25,-1,233,0` with a step forward and an attack
  each 1.2 s, the default fighter with a long sword (THAC0 19, +1): the skeleton (8 hit
  points, AC 7) dies at 5.62 s (20 frames per second, 5 contacts), 1.93 s (60, 2
  contacts) and 5.49 s (200, 5 contacts), 35 experience points each time. The first
  contact is at 0.82, 0.73 and 0.69 s. The difference of the kill time is the dice.
- Swing time, 32 swings in 40 s at each rate: 1.100 s at 20 frames per second, 1.066 at
  60, 1.068 at 200 (the frame of the end is reached at 15.5/16 of the time, rounded to
  the frame time); the swings start each 1.200 s (the key script).
- SG0.SG, Magic Missile: the cast at 1.05 s, the homing missile hits the skeleton at
  1.38 s: save failed (d20 4 against 17), 3 damage. Burning Hands: 6 missiles, each hit
  is an area of radius 10 (three hits, one save made), the skeleton dies. Shield: the
  cast time of 10 s, the effect at 11.05 s for 300 s, armor class 6 to -1 (Shield gives
  3 before the dexterity adjustment).
- Item and thief work (30 frames per second, `-char SG0.SG`, the thief and mage of the
  first level):
  - Wand of Magic Missiles (weapon 55, 15 charges), BANE: two casts with template 3, the
    homing missile hits the skeleton (saved: 1 damage; failed: 1 damage).
  - Scroll of Cure Light Wounds (spell 10 as an item, the character cannot learn it):
    hit points 3 to 7, one of two scrolls used.
  - Potions of Speed (Haste 480 s), Fire Resistance (600 s), Toughening (6 hit points,
    13 of 7, Bless 4800 s), Agility, Anti-Magic (all effects and the toughening hit
    points end), Hill Giant Strength (strength 19/0), Luck (the third attack roll is a
    natural 20 and a critical hit). Winged Boots and Ring of Quickness: Fly and Haste
    with no end, Fly taken away with the boots.
  - Open Locks, BANE wall 84 (seg 488 side 4, flags 0x18, keys 1), Apprentice Lockpicks
    in the slot: skill 35, d100 23, "You have successfully picked this lock!", the door
    opens.
  - Find/Remove Traps, THIEF2 trigger 5 (seg 344 side 4): skill 10, d100 20: not found;
    with `skill=3.90`: found (d100 20), the box answered Yes, removed (d100 73), text 943.
  - Climb Walls, BANE seg 196 side 0 (empty hand): two failed grips (96, 94), the third
    roll 54 holds, the player climbs from y -44.0 to -22.5 at 4.36 per second, the roll
    after 20.0 (60) holds.
  - Bench, BANE obj 29: one swing breaks it (0xA6ACB); the screenshots from the same
    place show the bench before and the floor after.
  - Creature spells, DROW2 hatchlings (mage 4/4, caster level 6): Fear (save made, d20
    13 against 12), Chill Touch, Hold Person on the player (save failed, d20 11 against
    12: held for 130.9 s); the hatchling without points recharges after 300 s.
  - Abjure (scroll, caster level 1), MYRKUL: a skeleton (not extraplanar) is not
    affected; the fire mephit (level 3, extraplanar): d20 against 13 made, d100 37
    against 76: sent back (0xA9F58, the object leaves the level).
  - Rary's Mnemonic Enhancer (scroll, choice 0 twice): manual rule: maximum of level 1
    from 1 to 4 to 7, the first rest fills 7, the second rest ends it (1); the rule of
    the executable (`-origcombat`): the second cast has no effect, the maximum stays 4
    through three rests.
- The body test of the swing (30 frames per second, `-noattack`, `-objlog` lines
  `combat: contact obj`). Before is the port with its general body test, after the test
  of the executable.
  - BONES, the character of the user (half-elf thief with a dagger, r 16.97 to 17.78),
    giant rat obj 8 (`-testpos -49.86,-3,Z,0`, the rat 6 to 16 ahead, it closes to
    9.19): before, the swings hit the zombie obj 4 (not in front) in 2 or 3 of 3
    swings at every distance and the hint changed on and off; after, all contacts are
    the rat, from 6.0 to 12.0 across and 5.86 under the body, and the hint is 1 until
    the rat dies. Long sword: the rat is touched before and after. Looking down
    (`lookdown:20`) and `-noimprovements` give the same contacts (the test uses the
    orientation of the body, not the view).
  - BONES, crypts (tombstones) in rows of three 12.5 apart (obj 35, 37, 36 at z 62.5,
    obj 38, 40, 39 at z 37.5), the player at x 122.5 facing +z: before, the dagger hit
    the side crypt obj 35 first at z 50 to 57, and obj 35 or 36 from z 42 to 46 (crypts
    behind and beside); after, only the middle crypt obj 37, from z 55 (dagger and long
    sword), and nothing from z 42 to 53.
  - BANE skeleton obj 10, long sword, `-testpos 25,-1,Z,0`: before, contact from z 235;
    after, from z 233 (10.64 across: `body_radius[0]` of the skeleton is larger than
    the torso radius that the port used). The fight of C5 above (a step and an attack
    each 1.2 s from z 233): the same hits and kill times before and after (20 frames
    per second: hit, kill at 1.90 s; 60: hit, miss, miss, kill at 4.18 s).
