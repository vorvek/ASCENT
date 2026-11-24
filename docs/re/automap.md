# Automap

Addresses are linear addresses of the image of DTUGAME.EXE 1.3 with object 1 at
0x10000 (`gamedata/re/dtu_flat.bin`, `gamedata/re/all.asm`). **V** marks a fact read in
the code or the data. **G** marks a guess.

The map is the automap of Descent 1 (`automap.c`), changed. It is not a screen of its own
as in Descent 1: it is drawn in the circle of the map screen (interface mode 4, see
[ingame-ui.md](ingame-ui.md)) while the game goes on.

## The key and the screen

- **V** The Automap action (Tab, [controls.md](controls.md)) calls 0x408F5: in mode 4 it
  selects the saved mode (0x13B61A), else it saves the mode (not for the modes 3 to 6),
  clears the flag 0x13A7F8 (the map has the controls) and selects mode 4. The MAP button
  of the screens selects mode 4 too.
- **V** 0x34032 (mode 4 at 0x340D5) makes the canvas 0x167368, a sub-canvas of the screen
  at the rectangle of the table 0x142F18 (high: 45, 25, 345 x 350) or 0x142F28 (low: 30, 20,
  160 x 160), and the bitmap 0x13AD17 ("crystalBall") with a copy of that rectangle of the
  interface picture.
- **V** Each frame in mode 4 (0x414D9) calls 0x1887E. It draws the map (0x186BC) into the
  canvas, then 0x344ED draws the crystalBall bitmap at the rectangle with color 255
  transparent (0xE0358, the linear blit 0xDFE54 skips 0xFF). The inside of the circle of
  pictures 64 and 58 is color 255, so the map shows there and the frame covers the rest.

## The frame 0x1887E

- **V** While 0x13A7F8 is 0 the frame starts the map again: `Max_edges` = min(4 x
  `Num_vertices`, 6000), the edge list (0x19968), the view matrix and the angles (pitch
  0x2328, heading 0, bank 0) of the player, the view target at the player position,
  `set_segment_depths` (0x4D15D, the Descent 1 function) from the player segment into
  `Automap_visited` (0x185FEC), the segment limit at the largest depth. The map follows the
  player and turns with him. The keys and the controls are not read.
- **V** While 0x13A7F8 is 1 the frame reads `key_inkey` (0xDA3E4):

| Key | Action |
|---|---|
| Escape (0x01) | 0x13A7F8 = 0: the map follows the player again |
| minus (0x0C) | segment limit - 1, not below 1 |
| equal (0x0D) | segment limit + 1, not above the largest depth |
| Alt+F (0x221) | with cheats on (0x13B7A3): the edge list with the map-all cheat flag |
| D (0x20) | all segments visited, edge list, depths and limit again (the Descent 1 debug key Alt+F of `KEY_DEBUGGED`, here on the plain D key) |
| Print Screen (0xB7) | 0x41533(0) |

  Then the Descent control values (0x1E9A68): the byte 0x1E9A80 (**G**: the attack count,
  the interface clears it with 0x1E9A81 after a click in the view) resets
  the distance to 200.0 and the angles (not the target); the distance goes down by forward
  thrust x 500; pitch and heading go up by fixdiv(time, 11500), bank by fixdiv(time,
  23000); vertical and sideways thrust x 350 move the target along the up and right vectors
  of the view, back if it is more than 1000.0 from the player. Then 0x40E50 clears the 32
  bytes of the control values and the key, mouse and joystick buffers: the player does not
  move.
- **V** The turn rate. The frame of the game (0x435A6) calls `read_controls` (0x42757,
  `controls_read_all` 0x52737) before the picture, and the map frame uses those values.
  A turn key adds its `key_down_time` divided by 8 to the heading time 0x1E9A70
  (0x5332D to 0x5341E, `sar eax, 3`). The walk keeps that time from frame to frame (the
  turn ramp), but 0x40E50 clears it in each map frame, so in the map the heading time of
  a frame is at most FrameTime / 8 (0x540F0 to 0x54114 limit it to FrameTime). The heading goes
  up by fixdiv(FrameTime / 8, 11500) (0x18ACF to 0x18ADE) in each frame: 1/8 x 65536 /
  11500 x 360 = 256 degrees per second at any frame rate, when the key counts for the
  whole frame. (Descent 1 has the divisor 115000 and no division by 8: 205 degrees per
  second.)
- **V** The key flush. `key_flush` (0xDA2A4, called by 0x40E50) sets each `Keyd_pressed`
  byte to 0 and the time of each key to now. The keyboard handler (0xDA850 to 0xDA86C)
  marks a key as down again only at a make code while the byte is 0: the first press or a
  typematic repeat of the keyboard. So in the map a held key counts in a frame only after
  a repeat came between the flush and the next `controls_read_all`, and only for the time
  from that repeat to the read. **G** (a model of those rules, not a measure of 1.3): with
  a repeat delay of 0.5 s and a repeat every 33 ms (the defaults of DOSBox), nothing turns
  in the first half second, then about 35 to 120 degrees per second for 20 to 30 frames
  per second, and less at a higher frame rate.
- **V** Both paths: view matrix = player orientation x the angles, the distance kept in
  100.0 to 2000.0 (0x13A804, 200.0 at the start). The load and save functions of the distance
  and the view matrix (0x19B9F, 0x19C10) have no call, jump or pointer in the
  image (a byte search), so the save game of 1.3 does not keep them.
- **V** The mouse (0x305C9, in the click code of the interface): in mode 4 a new
  press of the left button inside the rectangle of the canvas sets 0x13A7F8 = 1, a press
  outside it sets 0. **G**: the other two writes of 0 (0x2FE2E, 0x2FF8E) are other
  clicks of the same function.

## Drawing (0x186BC)

- **V** `gr_clear_canvas(255)`, `g3_start_frame`, `render_start_frame`, the viewer at the
  target minus the forward vector x the distance, zoom 0x9000, the edges (0x18CC0), the
  player (0x18567) in `player_rgb[Player_num + 1]` (single player: 27, 0, 0, a red) with
  `gr_find_closest_color` of the doubled values. No objects (hostages, keys) and no text.
  In a team or cooperative game the other players are drawn as in Descent 1.
- **V** The player (0x18567): a triangle, no sphere, no shaft, no up line. The tip is at 3
  x size forward; the two base corners are at s forward and s and -s to the right, s =
  trunc(size x 0.6667) (the doubles at 0x120A90: 2/3 and -1.0).
- **V** The edges (0x18CC0) as in Descent 1 (`EF_TOO_FAR`, frontier lines, contour lines,
  the dark lines with fade level 8, the depth sort, the cut at 1000.0) except the fade
  level of a bright line: a = depth of the first segment of the edge x 31 / largest depth /
  2; b = |center y of that segment - player y| (integer units) / 5; level = 31 - min(31,
  a + b). With the new edge flag 0x80 the level is 31 - rand() % (level + 1) in each frame
  (the line flickers). Color = `gr_fade_table[level * 256 + color]`; no-fade edges keep the
  color.

## The edge list (0x19968)

- **V** The map-all cheat is the flag 4 of the player flags (+0x2C of the player record),
  map all the flag 2. With either all segments with segnum != -1 are added, else the
  visited ones. The unknown edges of Descent 1 (`add_unknown_segment_edges`, 0x198D2) are
  not added. **G**: nothing sets the flags 2 and 4 in a single player game (the writes
  found are 0, flag 8 and a copy in 0x4E33D).
- **V** The test for lines between flat sides (dot of the normals above 0.9) clears
  `EF_DEFINING` only for edges of the normal color.
- **V** `add_one_edge` (0x191C6) is the Descent 1 function; it also sets the flag 0x80 when
  the global 0x13A80C is set.

## The sides of a segment (0x19335)

Colors are `gr_find_closest_color` with 0 to 63 values. Normal is (58, 58, 58). For each
side: `wid` and `cwid` are `WALL_IS_DOORWAY` of the side and of the connected side of the
child (0 without one), `hidden` and 0x13A80C start at 0, the color at normal.

| Wall type | Color |
|---|---|
| door, state not closed | none |
| door without child | normal |
| door with flag 0x40 (secret, see wall.c) | hidden = 1; normal unless both segments are visited; then (62, 0, 62) for keys 1 (none), else (0, 30, 30) |
| door, `wid` = `cwid` = 7 | none |
| door, other `tmap_num2` on the two sides | normal |
| door, else | (20, 62, 62) for keys 1, else (0, 30, 30) |
| illusion with `WALL_ILLUSION_OFF` | none |
| illusion | hidden = 1; normal if this segment is not visited; (62, 0, 62) without child; normal if the child is not visited; none if `wid` = `cwid` = 7; (62, 0, 62) if one of them is 7 or 3; else normal |
| open | none |
| closed | normal without child; none if `wid` or `cwid` is 7 or 3; else normal |

Then, in this order:

1. **V** Segment special 2 or 6: (58, 54, 26); special 3: (62, 32, 0).
2. **V** The side flags (the `pad` byte) of the side or of the connected side: bit 4 (a found
   trigger, see [combat.md](combat.md)) (0, 0, 62); else bit 8 (a found trap) (0, 62, 0);
   else bit 0x10 (a found secret door) (62, 0, 62).
3. **V** Colors (0, 0, 62), (0, 62, 0), (62, 0, 62) and (0, 30, 30) set 0x13A80C (flicker).
4. **V** The segment of the player: (62, 20, 20), hidden = 1, flicker.
5. **V** Normal color only for a side without child (-1) or with hidden = 1; no edges for
   "none" (255).
6. **V** Map all and the segment not visited: (0, 0, 50).
7. **V** The four edges of the side with `hidden` as the secret flag; no grate diagonals,
   no no-fade edges, no start segment color.

## The port

`engine/main/automap.c` (`dtu_automap_frame`, `dtu_automap_key`,
`dtu_automap_read_controls`, the changed `add_segment_edges`, `draw_all_edges`,
`automap_build_edge_list`, `add_one_edge`) and `engine/main/dtudisp.c` (`draw_map`, the
click). The map draws into the 8-bit frame, so it is the same with the software and the
GPU renderer. Port decisions:

- The game key loop (game.c) gives Escape, minus, equal and Alt+F to the map while it has
  the controls. The flight values come from the walking actions of the key table
  (`dtu_read_controls` of controls.c, which returns at once): forward and back zoom, turn
  turns the heading, slide and float move the target, look up and down pitch (**G**: the
  key table of the executable has no pitch item), attack resets. The player gets no
  controls, as after 0x40E50.
- The turn and the pitch keys add FrameTime / 8 in each step, the rule of 0x5332D: 256
  degrees per second at any frame rate. The whole stick of the gamepad turns at that
  rate times the gamepad speed setting (8 is 1). The port does not flush the keys in each
  map frame, so a held key turns at once and at a steady rate; the turn of 1.3, which
  depends on the key repeat of the keyboard, is not reproduced. The mouse does not turn
  the map. Before this rule the port added the whole FrameTime with no division by 8:
  2052 degrees per second, more than 5 turns per second.
- The values add up over the steps of `Dtu_fixed_step` until the map frame takes them,
  so a picture rate below 60 does not lose steps. Measured (`-renderfps`, the right
  turn key held for 2 s at YPINN, the script command `mapstate`): 255 degrees per second
  at 30 and 144 pictures per second, with and without `-nofixedstep`, and the same with
  the right stick of the gamepad; before, 2051 at 144 and 1025 at 30 pictures per second
  with the fixed step (one step of two was lost), 2051 without it.
- `Dtu_fix_automap_reveal` (1, `-origui` gives 0): the D key does not show the whole level.
  D is a movement key of the port.
- A new mode selection clears the flag 0x13A7F8 (the executable clears it in 0x408F5).
- Not done: Print Screen in the map, the other players. (The distance is not in the
  save game of 1.3 either.)
