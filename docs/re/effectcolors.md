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
- Not done: the tint of an object (0x14C4CD) and 0x14C4CF also turn the rule on for
  models; the port turns it on only for merged walls.
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
  shader. A texture that is not merged keeps the palette color, as the original.
- The upscaled textures: `tools/textures_hd.py` fills the effect texels with the
  colors around them before the upscale (`../hd_textures.md`). A set made before
  this has red around the effect texels; the game cannot remove that.

Tests (BONES, `-testpos 0,0,-296,180`: the glass door `glasdr1`; `-testpos
182,-6,86,90`: the waterfall `wfal04`): before, the panes were solid red and the
waterfall edges red in both renderers. After, the software renderer shows blue,
warm and green panes with the room behind and dark blue waterfall edges, the GPU
renderer (RGB and `ASCENT_GPU_8BIT=1`) the same, and with the old upscaled set red
dots stay around the waterfall edges; with the BONES archive made again by the
fixed tool they are gone.
