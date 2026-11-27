# Effect colors of the texture mappers

Eight palette colors of a wall texture are not drawn with their palette color: the
texture mappers of DTUGAME.EXE 1.3 give them a color from the screen. The bitmaps
use them for the glass of doors and windows (`glasdr1`, `glas02`, `miror01`), the
edges of the waterfalls (`wfal04`), water (`water2`, `effect`) and the effect walls
(`bg_e`, `rg_e`, `gg_e`, `cg_e`, `sh_e`). In the palette the colors are a bright
red (63 0 0) or a dark red (0xCF), so a renderer without the rule shows red glass
and red edges on the water.

**V**: verified in the code or the data. **G**: a guess.

## The code

- **V** The pixel shading functions 0xEC4D7 (the linear mapper 0xEC6BC) and its copy
  0x105A81 (the perspective mapper, set up by 0x105C60) get the texel color in AL
  and the light row in AH. When 0x14C4D8 (0x1512BE in the copy) is 0 they return
  `fade[row][color]` and nothing else.
- **V** 0xEA503 to 0xEA525 (draw_tmap) set 0x14C4D8 for each polygon: 1 when the
  bitmap has flag 0x20 (byte 9), when 0x14C4CD (the tint of the current object) is
  not 0, or when 0x14C4CF is not 0 (set by 0x45A60 around a call of 0x458BA).
- **V** The only code that sets flag 0x20 is the texture merge cache (0x7AB89): each
  merged bitmap (a wall with an overlay texture). No bitmap of the archives has the
  flag (`../formats/egg.md`). So a wall texture without an overlay shows the effect
  colors in their palette color, red.
- **V** 0xFF returns at once (transparent). Then the effect colors (0x105B08 to
  0x105B7E in the perspective copy), not lit, with `behind` and `below` pixels of the
  screen:

| Color | Perspective mapper (0x105A81) | Linear mapper (0xEC4D7) |
|---|---|---|
| 0xCF | `fade[r][fade[r][behind]]` | `fade[r & 15][fade[r][behind]]` |
| 0xBF | `0x50 \| (behind & 15)` | same |
| 0xBE | `0xA0 \| (behind & 15)` | `0xC0 \| s`, s = `behind & 15`, 15 becomes 14 |
| 0x6F | `0x20 \| (behind & 15)` | same |
| 0x6E | `0x50 \| (below & 15)` | same |
| 0x6D | `0xA0 \| (below & 15)` | `0xC0 \| s` as 0xBE, from below |
| 0x4F | `0x20 \| (below & 15)` | same |
| 0x4E | `below` | same |

- **V** The low 4 bits of a palette index are the brightness step in its ramp of 16
  colors (`../formats/egg.md`, PALETTE.256). An effect color shows the brightness of
  the screen in the color of a ramp: 0x20 blue, 0x50 green, 0xA0 warm, 0xC0 red.
- **V** For 0xCF the result of the first lookup goes on through the tests of the
  other effect colors and then through the general path, which looks it up again in
  row `r` (`and ah,0x1F` in the copy, `and ah,0x0F` in 0xEC4D7).
- **V** `r` is 0x14C4E1 minus 1. 0x72818 to 0x72867, in the view function 0x727A5,
  moves 0x13D500 one step each 6 views (counter 0x13D508, start 5) and writes it to
  0x14C4E1: from 0 (the start value in the data) up to 25, then back and forth
  between 24 and 31 (the direction 0x13D504 is +1 below 25 and -1 above 30).
- **V** The screen pointers. At the start of each span the copy sets 0x1512B0
  (behind) to the address of the first pixel of the span, and 0x1512AC (below) to
  the larger of the span address minus twice the length and the start of the next
  screen row (row start plus 0x280); since the first is always smaller, it is the
  start of the next row. Each effect pixel adds 1 to both, reads, and adds 1 more to
  below; below goes back to the start of the next row after 0x46C668 bytes (**G**:
  the view width; the span length is also limited to it at 0xEC835).
  So the k-th effect pixel of a span reads the k-th pixel after the start of the
  span (already drawn when k is less than its own place) and the pixel 2k - 1 of the
  next row. The other pixels do not move the pointers.
- **G** On a solid wall (a waterfall edge) the screen under the wall is the picture
  of the frame before, so the edge shows a moving pattern in the blue ramp. On a
  door with a transparent overlay (`glasdr1`, flag 0x02) the segment behind the door
  is drawn first, so the panes show the room behind in blue, warm and green.

## The port

- Software renderer: `engine/texmap/tmapfx.c`. texmerge.c sets
  `BM_FLAG_DTU_EFFECTS` (0x20) on a merged bitmap that has an effect color; draw_tmap
  (ntmap.c) sets `Tmap_fx`. The linear mapper (tmap_ll.c) does the rule while it
  draws, with the pointers of the original. The perspective mapper (tmap_per.c)
  draws the span, then does the effect pixels again in order from a copy of the
  screen before the span and the unlit texels of the span. render.c moves the pulse
  `Tmap_fx_row` as 0x72818 does (per view, not per time).
- The tint of an object (0x14C4CD) also turns the rule on (0xEA513, verified in
  the bytes): the software renderer sets `Tmap_fx` for a tinted object too
  (ntmap.c), so a model under Detect Magic shows its effect colors from the
  screen. Not done: the GPU renderer (it marks the effect texels only in merged
  bitmaps), and 0x14C4CF. 0x45A60 sets 0x14C4CF around the fade 0x458BA(1) at
  0x5A054 and 0x5A3B3 (the level change) and 0xA9A77, 0xA9B71 (the movie of
  script statement D3), so the rule is on for every polygon of those fade frames.
- The same functions change the view behind the conversation window (the byte
  0x14C4CE, see [scripts.md](scripts.md), section 4). In the DOS game it changes
  every lit polygon; the port applies it to every lit polygon.
- GPU renderer (`engine/platform/gpu.c`, `engine/3d/gpu3d.c`, `gputex.c`): a face
  with `BM_FLAG_DTU_EFFECTS` copies the image under it (`Dest_tex`, as a darkening
  does) and the shader replaces the effect texels. In the RGB form an effect texel
  has the alpha `255 - 8k` (k = 1 to 8 in the order of the table) and the color of
  the texels next to it, so that the filters and the mipmaps do not spread the red
  (`plat_gpu_fx_mark`); the brightness step of a screen color is the low 4 bits of
  its nearest palette color. Behind is the pixel under the effect texel and below
  the pixel one canvas row lower, so on the GPU the glass is a tinted view of the
  room and a waterfall edge shows the wall under it in the blue ramp. The pointer
  walk of the original (the pixel 2k - 1 of the next row) has no equivalent in a
  shader. A texture that is not merged keeps the palette color, as the original,
  unless `Dtu_fix_wall_effects` marks it (below).
- `Dtu_fix_wall_effects` (texmerge.c `texmerge_mark_wall_effects`, called by
  bmread.c after the tables; `-origfx` clears it). 1.3 draws a face without an
  overlay from the bitmap itself (0x75367, 0x7558C: the merge cache only for an
  overlay), so the effect texels of such a texture go through the light like any
  texel: `fade[row][0x4F]` of PALETTE.256 is 0xFF (0 0 0) at row 0, dark reds (8 0 0,
  17 1 1, then 0xC0 ramp entries) up to row 28, and the bright red 0x4E or the
  color itself (63 0 0) only at rows 31 to 33 (verified against the file). In a
  normal light 1.3 shows these texels black or dark red. The level files use such
  textures alone on a side (counted with `lvl.py` and the level tables, rendered
  sides with no overlay; "see" is a side with a wall and a child, "solid" one
  without a child):

  | Bitmap | Effect colors | Sides alone (see / solid) |
  |---|---|---|
  | `wfal04` (eclip) | 0x4F, 927 texels | 134 / 1 (illusion walls, THIEF0 segments 142 and 145) |
  | `wfal02` (eclip) | 0x4E | 2 / 2 |
  | `water2` (eclip) | 0x6F | 11 / 37 |
  | `web1` | 0x4E | 20 / 0 |
  | `rg_e` | 0xBE | 2 / 0 |
  | `miror01` | 0x6D | 0 / 2 |
  | `port04` (eclip) | 0x4E, 0xCF | 5 / 2 |
  | `lava`, `lavaflo`, `magma`, `port03` (eclips) | 0xCF only, 1 to 4 texels | many |

  The correction sets `BM_FLAG_DTU_EFFECTS` on each texture bitmap and each frame
  of an eclip that changes a wall texture when it has an effect color other than
  0xCF (palette 7 0 0, near the result of the rule; this keeps the copies of the
  GPU renderer off the lava walls). The software renderer, the GPU renderer and
  the upscaled textures (`plat_gpu_fx_mark` on the file of an unmerged bitmap)
  then draw them as a merged bitmap. A merged bitmap no longer inherits the flag
  of its base; texmerge.c sets it from its own pixels.
  Test: THIEF0, the save in front of the waterfall of segment 142: before, red
  rims around the drops in every renderer and texture set; after, the rims show
  the room behind in the blue ramp; `-origfx` gives the rule of 1.3 again (the
  palette color through the light, dark in a dim light and red in a bright one).
- `Dtu_fix_solid_effects` (texmerge.c, render.c, tmapfx.c `tmap_fx_solid_bitmap`,
  the shader of gpu.c with `u_fx` 2; `-origfx` clears it). The report of the
  stream of 2026-11-26: round red mirrors with a strange reflection in the
  software renderer and nothing in the GPU renderer. They are the glass `glas02`
  (64 x 64, flags 0x09, 845 texels of 0x6D in a disk of about 30 texels, in a
  brass ring), the overlay of closed walls (wall type 2, no door clip): DROW1
  segment 679 side 2 and 683 side 0, DROW2 205 side 2, DROW4 94 side 2 and 208
  side 0, CLAN1 717 side 4 (no segment behind). `miror01` and `miror02` (the
  same 0x6D, 317 and 663 texels) are not on a rendered side of the levels of
  the stream. 0x6D takes the warm ramp at the brightness step of the pixel
  `below`. Behind a closed wall the frame draws nothing before the face, so
  that pixel (and `behind`) holds the picture of an earlier frame: with the
  rule of 1.3 (the software renderer) the disk shows a warm, moving smear of
  the view (the pointer walk reads the next row from the left edge of the
  view, stretched 2 times). **G**: the DOS game shows the same; the port was
  not compared with it. The GPU renderer has no image under the face (the
  view of the frame starts empty) and showed the darkest step of the ramp, a
  dark red disk. The correction is the
  intent of the rule, a glass that shows what is around it, with a source
  that exists: on a face whose side the view does not see past (no
  `WID_RENDPAST_FLAG`), each effect texel shows the color filled into it
  from the texels around it (the mean of the filled 8 neighbors, ring after
  ring, as `fill_colors` of gputex.c and step 1 of `textures_hd.py`), lit as
  the other texels. The software renderer draws a copy of the bitmap with
  the nearest palette color (not 255, not an effect color) in these texels
  and without the rule; the GPU renderer draws the face without the rule, so
  it shows the filled colors of its texture: the same fill in the 8-bit
  expansion, the smooth fill of the upscaled file. The 8-bit form of the GPU
  renderer (`ASCENT_GPU_8BIT=1`) draws the copy of the software renderer.
  The glass doors, waterfalls and effect walls that the view sees past do
  not change (BONES `-testpos 0,0,-296,180`, the door `glasdr1`: the same
  screenshot with and without `-origfx` in both renderers). Tests
  (`-testpos` at frame 60, both renderers, before with `-origfx`): DROW1
  `380,-3.5,1097.1,90`, CLAN1 `-137.8,0,700,0`.

  The water slides of HUB3 (the hub of the drow levels: DROW1 is entered
  from it in the logs of the stream). The sloped faces where the water runs
  down are the overlay `wfal04` (the eclip of the waterfall, 8 frames, 881
  texels of 0x4F in a dither with transparent texels) on a stone base:
  segment 79 side 3 (base `113`), 108 side 5 (`175`), 149 side 3 (`175`),
  159 side 3 (`168`), all with no segment behind. The flat water is `water`
  with the overlay `wfal01` (segments 213 to 230, side 3), which has no
  effect color. A slide is a merged bitmap with effect colors on a face
  that the view does not see past, so with the rule of 1.3 its 0x4F texels
  read the screen of earlier frames: the software renderer showed dark and
  moving dots in the water, the GPU renderer (gpu.c `fxrgb`, one texel of
  the 64 x 64 bitmap for each effect texel) dark blue squares of the darkest
  step of the ramp over the whole slide. The upscaled files of `wfal04` and
  the texture filter were not the cause: the texels between the squares
  show the upscaled water. With the solid form the effect texels show the
  water filled into them, and the GPU renderer shows the smooth upscaled
  water up to the edge of the stone. Test: HUB3 `-testpos 85,5,80,90,30`
  (the fifth value is the look pitch, new for this test), both renderers.
- `Dtu_fix_glass_windows` (texmerge.c, render.c `render_wid`; `-origfx` clears
  it). The glass `glas02` is a one way window. Wall data (**V**, `lvl.py`):

  | Level | Glass side (seg/side, base + overlay, wall) | Other side (seg/side, base + overlay, wall) |
  |---|---|---|
  | DROW1 | 679/2, `misc1` + `glas02`, wall 61 closed | 680/5, `misc1` + `well2`, wall 60 closed |
  | DROW1 | 683/0, `misc1` + `glas02`, wall 63 closed | 682/5, `misc1` + `well2`, wall 62 closed |
  | DROW2 | 205/2, `zroc39` + `glas02`, wall 117 closed | 294/5, `stone223` + `well2`, wall 116 closed |
  | DROW4 | 94/2, `wall1` + `glas02`, wall 5 closed | 100/5, `misc1` + `well1`, wall 4 closed |
  | DROW4 | 208/0, `wall1` + `glas02`, wall 7 closed | 209/5, `misc1` + `well1`, wall 6 closed |
  | CLAN1 | 717/4, `eroc43` + `glas02`, no wall | no segment behind |

  All walls have flags 0, no trigger, no clip, `keys` 1. `well1` and `well2`
  (flags 0x0B) are a stone ring with a hole of color 254 (845 and 818
  texels; the glass of `glas02` is 845 texels of 0x6D). The render traversal
  of 1.3 (0x7CFEE, `wall_is_doorway`; 0x7CFA3, `check_transparency`, **V** in
  the pseudocode) passes a closed wall with an overlay only when the overlay
  has the super transparent flag 0x02, as Descent 1 does; the port has the
  same rule (wall.c `check_transparency`, render.c). So from the room of
  `well2` the wall is see-through (the hole shows the room behind; the glass
  face is a back face and is not drawn), and from the other side `glas02`
  (flags 0x09, no 0x02) is a closed wall: the room behind is not drawn and
  the 0x6D texels show earlier frames. **G** (inference): the design is a
  window that looks out of the room of the drow wizard (DROW1) and shows the
  room tinted from outside, the use of 0x6D on the glass of doors
  (`glasdr1`); the traversal rule of Descent 1 does not know the effect
  colors. With the correction the render traversal passes a closed wall
  whose overlay has an effect color other than 0xCF when the side has a
  segment behind it, so the room behind is drawn first and the glass shows
  it in the warm ramp in both renderers. Movement, sight, sound, missiles
  and the use ray keep `WALL_IS_DOORWAY`. The segments seen through the
  glass are rendered segments, so the automap marks them as seen.
  `Dtu_fix_solid_effects` stays for the faces with no room behind (CLAN1
  717/4, the slides of HUB3). The software renderer keeps the pointer walk
  of 1.3 for `below` (the next row from the left edge of the view, 2 pixels
  for each effect pixel), so its tinted view is a stretched copy of the left
  part of the view; the GPU renderer reads the pixel one row lower, so its
  view through the glass is the room behind. Tests: DROW1 `-testpos
  380,-3.5,1097.1,90` (outside) and `412,-3.5,1098.3,270` (the room of
  `well2`), frame 60, both renderers, before with `-origfx`: outside, the
  GPU renderer showed a dark red disk and the software renderer a smear;
  after, both show the room behind and the creature in it in the warm ramp (the
  software one stretched); inside, no change.
- The upscaled textures: `tools/textures_hd.py` fills the effect texels with the
  colors around them before the upscale (`../hd_textures.md`). A set made before
  this has red around the effect texels; the game cannot remove that.
- A texture of effect texels and transparent pixels only: `web1` (the cobwebs of
  THIEF1, 336 pixels of 0x4E, the rest 255; also `web2`, `web4`, `effect#0` to
  `#9`, the effect walls `bg_e`, `rg_e`, `gg_e`, `cg_e`, `sh_e`). The tool has no
  other color to fill them from, so the file keeps the red of 0x4E in every
  pixel. With the upscaled set the GPU renderer showed red dots along the threads
  of the cobwebs: the texels inside the alpha of the file but outside the smooth
  mask of the effect texels kept that red and were drawn opaque (118 of the
  65536 texels of `web1`). `plat_gpu_fx_mark` now makes every texel that is not
  an effect texel transparent when the texture has no opaque texel of another
  color. Test: THIEF1 `-testpos -271,-22,295,90` (the webs of segments 94 to 97),
  frame 150: before, red dots on the webs; after, none, as with the 8-bit
  textures of the GPU renderer and the software renderer. A texture with other
  colors (a waterfall, a merged wall) does not change.

Tests (BONES, `-testpos 0,0,-296,180`: the glass door `glasdr1`; `-testpos
182,-6,86,90`: the waterfall `wfal04`): before, the panes were solid red and the
waterfall edges red in both renderers. After, the software renderer shows blue,
warm and green panes with the room behind and dark blue waterfall edges, the GPU
renderer (RGB and `ASCENT_GPU_8BIT=1`) the same, and with the old upscaled set red
dots stay around the waterfall edges; with the BONES archive made again by the
fixed tool they are gone.
