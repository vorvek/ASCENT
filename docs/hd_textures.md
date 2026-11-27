# Upscaled textures

`tools/textures_hd.py` makes a copy of every bitmap of the EGG archives at 4 times the
width and the height. The output is derived from the game data: it lives in
`gamedata/hd` (ignored by git) and must never be committed.

## Commands

```
python -I tools/textures_hd.py --setup              # venv, PyTorch (CUDA), spandrel, Pillow, models
python -I tools/textures_hd.py                      # all 57 archives (PORTAL, HMC first)
python -I tools/textures_hd.py --only portal,bane   # some archives
python -I tools/textures_hd.py --check portal,hmc   # quality checks of finished archives
python -I tools/textures_hd.py --compare            # comparison sheets in gamedata/hd/work/texcompare
python -I tools/textures_hd.py --out DIR --only kobolds --names flame8,well1
                                                    # only these bitmaps (all frames) again, in DIR
```

`--names` makes the named bitmaps again and leaves the other files and the `.method`
file alone, for a test in a copy of a set (hard links of the archive directories).
`--unpacked DIR` and `--models DIR` give the unpacked archives and the models when
the tool runs from another checkout (a git worktree has no `gamedata`).

`--method NAME` picks another method (`combo`, the default, or one model or baseline
of the table below), `--force` redoes existing files, `--redo-effects` redoes the
files of the bitmaps with effect colors (see below). The tool runs itself again with
the Python of `gamedata/hd/venv` (Python 3.11, made with `uv` if it is installed).
A file is skipped when it exists, is newer than its archive and the archive directory
was made with the same method (file `.method`). A stopped run continues where it
stopped.

## Inputs

`PORTAL.EGG`, `HMC.EGG`, `HMT.EGG` from the install (`E:\UNDAMONTAN\DTU`), all other
archives from `gamedata/unpacked/*.EGG`. Palette: the first 768 bytes of
`PALETTE.256`. The old layout of `DEMO.EGG` and `FFM.EGG` (20-byte sound headers) is
detected by the name of the first model header. Skipped: the 149 interface bitmaps of
`PORTAL.EGG` (flag 0x40, front end art with another palette) and the damaged
`bless#6` of `MULHOR.EGG`.

## Output

`gamedata/hd/textures/<egg>/<name>.png`: `<egg>` is the archive base name in lower
case, `<name>` the EGG bitmap name in lower case with `#` replaced by `_`. RGBA 8-bit,
4 times the original width and height. `index.txt` in each directory has one line
`name w h` per file (EGG name with `#`, original size), and `name w h soft` for a
file of the fire flipbooks with a soft alpha (version 8, see The fire flipbooks). No two bitmaps of one archive
map to the same file name (checked for all 57 archives).

Alpha is 255 for a bitmap without the transparent flag (0x01). With the flag it is
the mask of color 255 upscaled with smooth edges (step 4 below): 0 outside, 255
inside, and a ramp of about one output pixel at the edge (since version 4; 0 or 255
before). The RGB of transparent pixels holds the color of the nearest opaque
pixels, so neither the bilinear filter nor the blend of a soft edge makes a dark or
bright fringe.

Equal bitmaps (same pixels, size, flags and rule) are upscaled once. They are stored in
`textures/.cache/<sha1>.png`, and the archive files are hard links to them. The level
archives share most of their bitmaps with `PORTAL.EGG`.

Effect colors (0x4E, 0x4F, 0x6D, 0x6E, 0x6F, 0xBE, 0xBF, 0xCF, see
`re/effectcolors.md`): the game draws these texels of a merged wall with a color
from the screen. Their palette color is a bright red (dark red for 0xCF). The tool
fills them with the colors around them before the upscale (step 1 of the method),
keeps them opaque, and gives these bitmaps a new cache key. The game marks the
texels from the original pixels. A wall texture without an overlay gets the same
rule with `Dtu_fix_wall_effects` (`re/effectcolors.md`); with `-origfx` its texels
keep their palette color, as in 1.3. 5044 archive files (340 distinct bitmaps,
45 wall texture names) have effect colors.

A set made before this change has a red rim around the effect texels (the red
waterfall edges, the red dots at the glass of doors). Make these files again with
`python -I tools/textures_hd.py --redo-effects` (all archives; only the bitmaps with
effect colors are upscaled again, the other files stay). The BONES archive (3279
files) took 3 minutes from an empty directory.

Not covered: color 254 of bitmaps with the super transparent flag (0x02). 816 bitmaps
of `PORTAL.EGG` use it (palette entry 254 is a dark red, 20 0 0). The upscale treats
it as a color, so the output has no exact mask for it.

## Method

For each bitmap:

1. Palette colors. Transparent pixels (flag 0x01, color 255) and the pixels of the
   effect colors get the mean color of their already filled 8 neighbors, ring after
   ring, until the picture is full.
2. Padding. Tiling bitmaps are padded by wrapping with 64 source pixels on each
   side (`TILE_PAD`): a 64 x 64 texture is upscaled as the center of a 3 x 3
   tiling of itself. Tiling bitmaps are every 64 x 64 bitmap and every bitmap that
   a table uses as a wall texture (`$TEXTURES` outside `!BITMAP_REMAP_OFF`,
   `$WCLIP`, `$ECLIP` without `obj_eclip=1`; 691 names over all tables). The
   others are padded by mirroring with 16 source pixels. Before version 3 the
   tiling bitmaps had 16 pixels of wrapping too; the receptive field of the
   models is larger than that, so the result depended on where the picture ended
   (see Seams below).
3. Upscale x4 on the GPU (fp16, batches of equal size), then crop the padding.
4. Alpha: the opaque mask, padded the same way, blurred (gaussian, sigma 1 source
   pixel), upscaled bicubic and cut at 0.5 with a ramp of one output pixel (see
   Smooth edges below). Before version 4: upscaled bicubic and cut at 0.5.
5. Mean color: the mean RGB of the opaque output pixels is moved to the mean of the
   original opaque pixels with one offset per channel (the game applies its own
   lighting, so a brightness shift by the model would be wrong).
6. Animations: the frames of an animation with a static part get the same result
   where their source pixels change little or not at all (see Animations below).

The method `combo` uses two models by a fixed rule:

- wall textures (the table list of step 2): 4x-UltraSharp,
- all other bitmaps (model skins, sprites, effects, items, icons): RealESRGAN_x4plus.

All frames of one animation have the same base name, so they always get the same
model and padding.

### Models

| Name | File | Source | License | Use |
|---|---|---|---|---|
| ultrasharp | `4x-UltraSharp.pth` | Kim2091, huggingface.co/Kim2091/UltraSharp (SHA-256 a5812231...) | CC BY-NC-SA 4.0 (model card) | wall textures |
| x4plus | `RealESRGAN_x4plus.pth` | xinntao, github.com/xinntao/Real-ESRGAN release v0.1.0 | BSD-3-Clause | everything else |
| x4plus-anime | `RealESRGAN_x4plus_anime_6B.pth` | Real-ESRGAN v0.2.2.4 | BSD-3-Clause | evaluated only |
| animevideov3 | `realesr-animevideov3.pth` | Real-ESRGAN v0.2.5.0 | BSD-3-Clause | evaluated only |
| siax | `4x_NMKD-Siax_200k.pth` | Nmkd, mirror huggingface.co/uwg/upscaler | not checked | evaluated only |
| superscale-sp | `4x_NMKD-Superscale-SP_178000_G.pth` | Nmkd, same mirror | not checked | evaluated only |
| remacri | `4x_foolhardy_Remacri.pth` | FoolhardyVEVO, same mirror | not checked | evaluated only |

The SHA-256 of each file is in the tool and is checked by `--setup`. Loader:
spandrel 0.4.2 on PyTorch 2.14.1 + CUDA 12.6. The UltraSharp license (CC BY-NC-SA
4.0) covers the model weights: do not redistribute or sell the `.pth` file, and do
not use the model for a commercial service. It does not by itself cover the images
that the model makes; the model card discussion says that the restriction is about
profit from the model itself. The models stay in `gamedata/hd/models`, not in the
repository.

The limit on sharing the upscaled set comes from its source instead: every upscaled
texture and picture is made from the game's art, which belongs to its rights
holders. A shared set is a derived copy of that art. The repository therefore holds
only the tools, and each user makes the set from their own copy of the game.

### Evaluation

22 bitmaps (`COMPARE_SET` in the tool): stone (`grey4`), wood (`wood2`), floor
(`bflor09`), brick walls (`eroc10`, `eroc50` with carved runes), animated lava,
human and drow faces (`hum_mt04`, `dlf_ff01`), a creature skin (`min_b10`), a player
skin of `HMC.EGG` (`pa_a01`), a sign with text (`flamswrd`), white text
(`hands`), sprites with transparency (`firbal1#0`, `potion#0`, `exp19#0`,
`chain1#0`), item icons (`gencmail`, `genscrl2`), small bitmaps (`pebble#0`,
`spray2#0` 32 x 32, `cursor` 16 x 16), a switch (`swtch08a`). Candidates: nearest,
Scale4x (EPX twice; xBRZ needs a C compiler on this machine, so EPX stands in for the
pixel-art scalers), bicubic, the seven models above.

What the sheets showed:

- nearest, Scale4x: the original pixels. Scale4x only rounds staircase edges; on
  dithered 8-bit textures it gives blobs.
- bicubic: soft, no new detail.
- x4plus-anime: flattens the textures into cartoon areas (stone and floor grain is
  lost, wood becomes plastic).
- animevideov3: clean but soft, close to x4plus with less detail.
- siax, superscale-sp: the sharpest, but they keep the palette dither as a woven
  grid (faces, skins) and add white sparkles on lava.
- remacri: adds hair-like strokes that are not in the original (lava, floor).
- x4plus: clean, faithful shapes, good faces and skins, text stays sharp; softer on
  stone and floor grain.
- ultrasharp: the best wall textures (grain of stone, wood and plaster, carved
  details stay sharp, no invented patterns), but on dithered human skins it makes
  box-shaped edges (nose and cheeks of `hum_mt04`, `hum_ff01`).

So `combo` takes UltraSharp where it is best (wall textures) and x4plus where
UltraSharp fails (skins, which are model textures, never wall textures). A 50/50
blend of the two was also tried on skins and walls: it is softer than either and
shows double edges.

## Quality checks

`--check` result for all 57 archives (60282 files):

- (Version 2 and 3; version 4 has a soft alpha edge, see Smooth edges.)
- Size and mode: all 60282 files are RGBA at 4 times the size. Alpha holds only 0
  and 255 in all files.
- Seams (opaque 64 x 64 wall textures, 617 to 684 per level archive): mean
  difference between the file and the rolled upscale is 0.39 of 255 at the seam
  and 0.20 inside. The step across the seam in the file divided by the step at the
  same place in the rolled upscale: median 1.00, 5 to 95 percent 0.96 to 1.02.
  Control with mirror padding: 5.9 to 6.6 at the seam (2.6 to 2.8 inside), step
  ratio median 1.7, 95 percent 2.5.
- Color: largest mean channel difference of a texture 1.17 of 255 (`confus#7` of
  `PORTAL.EGG`, where clipping at 0 or 255 limits the offset), median 0.03. Before
  the offset the largest difference was 27.2 (`ske_g08` of `BANE.EGG`).
- Halo (2526 sprites in the largest level archive): edge brightness output minus
  original, median +0.34 of 255, range -10.0 (`bless#1`) to +24.0 (`gua_g04`); 122
  sprites differ by more than 5. A zoomed look at `bless#1` (sparkles) and `flayer#3`
  shows no fringe: thin 1-pixel rays change shape at 4x, which moves the edge
  average. `gua_g04` was not inspected. Player archives: -1.3 to +6.6.
- Animation (up to 2439 consecutive frame pairs per archive): frame difference after
  divided by before, median 1.10, 5 percent 0.94, 95 percent 1.83. Equal frames stay
  equal (0 exceptions). The largest ratio, 8.1 (`rescold#6` to `#7`), is 0.05 to
  0.43 of 255.

The seam check upscales each opaque 64 x 64 wall texture again, rolled by half its
size, so that its wrap seam lies inside the picture, and rolls the result back. With
wrapping the two results agree at the seam about as well as inside. The control does
the same with mirror padding. (The figures above are of version 2, 16 pixels of
wrapping.)

### Seams with the 3 x 3 tiling (version 3)

The lava flipbooks of `PORTAL.EGG` and `BAT.EGG` (`lava` 6 frames, `lava02` 12,
`lavaflo` 15, `magma` 6; 39 bitmaps each) were upscaled with 16 and with 64
pixels of wrapping, each once as it is and once rolled by half and rolled back.
Mean difference of the two results (of 255), in a band of 4 output pixels at the
edges and inside:

| Wrapping | At the edges | Inside |
|---|---|---|
| 16 pixels | 0.95 | 0.86 |
| 64 pixels (3 x 3) | 0.08 | 0.06 |

With 16 pixels the result depends on the place of the picture edge by about 1 of
255 everywhere, and the edge rows are made from a padding that ends 16 pixels
away: the step across the seam of `lava02` was up to 2 times the mean step inside
the file (the worst file of all 702 lava and magma files of the set). With 64
pixels the result is nearly independent of the place of the edge, so the edges of
a frame match each other as any two neighbor pixels inside do. A 2 x 2 tiling of
`lavaflo#7`, `magma#2` and `lava#4` shows no seam, and the lava floor of HUB1 and
the lava fall of BAT (segment 114) render with the new files.

The halo check compares the mean brightness of the opaque pixels next to transparent
pixels in the output and in the original. A dark halo would give a large negative
value. A zoomed look at `flayer#3`, `bless#1`, `potion#0` and `gencmail` on magenta
and green backgrounds shows no fringe.

The animation check compares the mean difference between consecutive frames before
(nearest 4x) and after. Large ratios occur only where the original frames differ
very little (0.05 of 255).

## Textures that stayed 8-bit in the game

The set has a file for every bitmap of every archive (checked: 60282 files,
each with its `index.txt` line, the size of the line times 4, RGBA, readable).
A texture that shows 8-bit pixels in the game is a fault of the lookup, not of
the set. `ASCENT_GPU_HDLOG=1` prints each request of an upscaled texture, and
"no source" or "not in the set" when there is none; `-testtour` makes the
player visit every segment with 4 headings.

- The source of a bitmap is found by the address of its pixels in a table of
  GameBitmaps (render.c `gpu_refs_update`). The table was made again only when
  the number of archives or of bitmaps changed. A level change that loads an
  archive with the same counts at new addresses (CLAN3 to CLAN1, both with
  CLAN.EGG; a level loaded again from a saved game) kept the old addresses: the
  bitmaps whose pixels moved had no source and stayed 8-bit among upscaled
  walls (CLAN3 tour with the warp to CLAN1: 67 to 81 requests with no source,
  for example `wood14`, `chair_03` and RLE bitmaps). Now piggy.c counts each
  change of its tables (`Piggy_generation`) and the table is made again after
  each one: 0 requests with no source in the same run.
- The edge of a wall overlay (a door or a sign over a wall, `merge_top` of
  gputex.c) was cut with the mask of the original pixels, so the outline of
  `dleft2` (KOBOLDS segment 469) and of the sign `plate21` (segment 3) was
  made of 4 x 4 blocks. The merge now uses the alpha of the upscaled overlay
  (see Smooth edges for version 4).

The red and brown double doors near the sign "Flood Control Unit 3" (HUB1
segment 150: `gdor2` on `wood12`, segments 148, 149; KOBOLDS: `glasdr1` on
`wall1`, segments 0, 71; `hdor5` in HUB1 segments 141, 143) are upscaled in a
fresh start of the level, also during the opening of the door: no rule skips
them, they are not interface bitmaps and their names match. The fault above is
the cause found for a texture that stays 8-bit; the exact game state of the
report (a level change or a load) was not reproduced.

## Smooth edges (version 4)

The outline of an overlay was still made of stair steps after the merge used the
alpha of the file: the bicubic upscale of a 1-bit mask cut at 0.5 follows the
source pixels and only rounds their corners. A torch niche (`flame8` on `wall1`,
KOBOLDS segment 115; `flame4` on `wood22`, YPINN segment 119), the round stone ring
of `sewge1` and the round opening of `well1` showed steps of one source pixel (4
output pixels) along every curve.

The tool (`smooth_alpha`): the mask is blurred with a gaussian of sigma 1 source
pixel before the bicubic upscale, so a staircase becomes a straight or a curved
line, and the cut at 0.5 has a ramp of one output pixel (the distance to the 0.5
line is estimated as (f - 0.5) / |grad f|). A source pixel that the blur would lose
(an opaque or a transparent dot or line of 1 pixel) is kept: as its square when
such a pixel of the same kind is beside it (a line stays a line), as a disk of one
source pixel across at its center when it is alone (a dot, the pixels of a
dither). A disk for every such pixel made a dotted line of the transparent top row
of `arch4` and crosses of the small holes of `resfire#17`; a square for every one
kept the dithered edges of the waterfall `wfal04` as blocks. The opaque area stays the same: 0.1973 against 0.1980
of the original for `flame4#0`, 0.5937 against 0.5945 for `sewge1`, 0.7462 against
0.7468 for `well1`. Sigma 0.7 left small waves on the arch of `flame4` and the
ring of `well1`; 1.0 does not.

The engine (gputex.c):

- `merge_top` blends the overlay over the wall with its alpha (the color under
  the soft edge is the filled color of step 1, so no fringe). A wall pixel with
  alpha 0 takes the overlay only where its alpha is 128 or more, so the merged
  alpha stays 0 or 255 (the effect codes use the values between).
- The holes of a super transparent overlay (color 254, the dark center of `well1`
  and of the round openings) come from a smooth mask (`smooth_mask`: the same
  blur of sigma 1, sampled bilinear and cut at 0.5, thin features kept as above) instead of
  the 4 x 4 blocks of the original pixels.
- The effect texels of a merged wall (`plat_gpu_fx_mark`, the see-through parts
  of water and waterfalls) come from the same smooth mask of the effect colors; a
  texel takes the code of the nearest source pixel with an effect color around
  it. A texel that had an effect color but is outside the mask gets its color
  from the texels around it (not the bright red of the palette entry). At the
  original size (the 8-bit textures of the GPU renderer) the texels are the
  source pixels, as before; the software renderer does not change.
- A layer that is not an overlay (a sprite, a grate) cuts the alpha of its file
  at 128 (0 or 255 for the alpha test), instead of using the mask of the
  original pixels. A soft file (version 8, the fire flipbooks) keeps its alpha.

A set of version 3 still works with the new engine: its alpha is 0 or 255, and the
blend of `merge_top` then gives the merge of before. The new holes and effect
masks need no new files.

Game screenshots (1920 x 1440, `-fixedfps 30 -frames 60`, old engine and set
against new engine and the files made again): the arch of `flame8` (KOBOLDS
`-testpos -110,37.5,347.5,270`) and of `flame4` (YPINN `-testpos -140,0,200,0`,
90 frames) has a smooth outline with no dark or bright fringe; the round opening
with water behind it (KOBOLDS `-testpos 0,-15,327,0`) has a smooth hole edge, and
the single effect texels of the waterfall `wfal04` at the left are round dots;
its effect and transparent pixels are mostly a dither of lines of 1 pixel, which
stay squares, so this waterfall changes little.

`--check kobolds` of the set of version 3 and of KOBOLDS made again (the halo and
the color of the check count a pixel from alpha 128 on): soft alpha in 2470 files
(0 before); color difference median 0.07 before and 0.09 after; halo median 0.69
and 0.69, sprites with a halo above 5 of 255: 247 before, 250 after; the static
pixels of the 762 frame pairs of the animations of `anim_merge`: median 2.66
before, 0.40 after. The seam check compares a file with a new upscale of the single frame, so
it shows the frames of `anim_merge` as seams (`up7#0` 5.84); the step across the
wrap edge of the files themselves is the same as before (`water2` max 1.10
against 1.14, `magma` 0.66, `flame8` 0.97, `up7` 8.45 against 8.63: `up7` does not
tile in the original either).

## Animations (version 4)

Each frame of a flipbook was upscaled alone. The model reacts to the whole
picture, so the parts that do not move (the niche and the holder of a torch, the
wall behind it) came out a little different in each frame and shimmered when the
animation played. In the torch niches the original frames also change most of
the niche by a few shades (the light of the flame, drawn as a dither that changes
from frame to frame): of the 871 changing opaque pixels of `flame8`, 723 change by
less than 24 of 255; the model turns these small changes into changes of the
shapes of the stones.

`anim_merge` takes the frames of one base name (`name#0`, `name#1`, ...; the
frame number is in the EGG header, the tables play them as a `$WCLIP` or
`$ECLIP`). An animation qualifies when its frames have one size, are not all
equal, and at least 30 percent of the opaque pixels are equal in all frames
(`ANIM_MIN_STATIC`). The reference is the frame closest to the per-pixel median.
For each other frame:

- a pixel whose color differs from the reference by more than 40 of 255
  (`ANIM_LIGHT`, the largest channel) or whose transparency differs is a changing
  pixel (the flame);
- elsewhere the result is the result of the reference plus the bicubic upscale of
  the color difference to the reference, blurred with sigma 1 source pixel (the
  dither of the light would show as a grain): the same detail in every frame, the
  colors of the frame;
- the weight of that result ramps from 0 at 0.5 source pixels from a changing
  pixel to 1 at 2.5 (`ANIM_RAMP0`, `ANIM_RAMP`), so no seam shows; the changing
  pixels keep the frame's own result. The distance wraps for tiling bitmaps.

Pixels that no frame changes get the result of the reference in every frame, and
equal frames stay equal. The cache key of a frame has the keys of all frames of
its animation and `ANIM_VERSION`.

Over all 57 archives 81 base names qualify: the torches and candles (`flm1` to
`flm7`, `flame2` to `flame19`, `candle1`, `candle2`), the door and lift clips
(`gdor*`, `hdor*`, `idor1`, `dup*`, `down*`, `up*`, `dwide3`, `dwide4`,
`glasdr1`, `shelfup1`, `wald1`, `wald2`), `light1` to `light3`, `port01` to
`port04`, `magma`, `water2`, `chain1`, `blob03`, `arrow_a`, and the skins
`beh_be01`, `beh_sp01`, `pq_a01`. Lava (`lava`, `lava02`, `lavaflo`) and the
waterfalls change everywhere and keep the result of each frame.

Measured on the files (consecutive frame pairs, pixels opaque in both, mean of the
largest channel difference of 255; "detail" is the difference after a high-pass
of sigma 4 output pixels, the change of shapes):

| Animation | Static pixels, before | after | Light pixels (change 1 to 40), detail, before | after |
|---|---|---|---|---|
| `flame8` (14 frames) | 2.25 | 0.23 | 6.16 | 2.73 |
| `flame4` (13) | 3.17 | 0.45 | 9.27 | 5.71 |
| `flame3` (14) | 3.26 | 1.26 | | |
| `gdor2` (13) | 2.76 | 0.15 | | |
| `light1` (5) | 2.93 | 0.59 | | |
| `magma` (6) | 4.25 | 2.18 | | |

The changing pixels (the flame) keep their difference: 99.9 before, 99.7 after
for `flame8`. In the game (KOBOLDS `flame8`, 6 screenshots 2 frames apart at
`-fixedfps 30`), the mean difference of consecutive screenshots in the niche
without the flame fell from 2.96 to 1.18 of 255, the high-pass part (the shimmer
of the shapes) from 1.47 to 0.28; a wall area beside the niche has 0.00 in both,
so the view did not move.

### Doors (version 5)

The report: a door of the Shadow Thieves (THIEF1, `up3` on `stone61`, wall 22
of segment 24) left a part of its texture in place while it opened. The port
has no pass that uses the previous frame; the fault was in the files. A door
clip moves its picture: the planks and bands slide up over the place where
other planks were. A plank pixel that slid over a plank pixel of a similar
color differs from the reference frame by less than `ANIM_LIGHT`, so it got the
result of the reference: the bands and the grain of the reference frame stayed
in place, faint, in every frame (`up3#7` to `#9` showed the bands of `up3#5`;
the closed `up3#0` had a smeared lower band).

The wall clips (`$WCLIP`, all doors: 40 of the 81 animations, 8244 archive
files, 471 distinct frames) now keep the result of the reference only where
the pixels are equal (`ANIM_LIGHT_WCLIP` = 0): the frame of the door stays
steady, the moving part keeps the result of its own frame. The effects
(`$ECLIP`: the torches, lights, portals, magma) keep `ANIM_LIGHT`. Their cache
keys do not change.

Test: the 11 door clips of THIEF1 made again with `--names` in a copy of the
set; a sheet of the 11 frames of `up3` (original, before, after) shows no band
of the reference in the frames after. In the game (THIEF1 `-testpos
15.6,-4,410,180 -testmove wait:20,use:1,wait:80`, screenshots at frames 20 to
60) the closed door has its own lower band and grain; the reference frame is
the same in both sets.

## Corrections of single bitmaps (version 6)

Two reports of the stream of quests 6 to 8. Both were faults of the upscale,
not of the original art or of the engine.

- The flat water (`water`, `$ECLIP` 37, 6 frames of 64 x 64, the same pixels
  in every level archive) showed one soft frame with no ripples. The original
  frames are alike: dithered blue blobs. x4plus (the base of the method
  `detail`) turned the dither of frames 0, 1, 2, 4 and 5 into streaks of foam
  and smoothed frame 3 into a soft picture. High frequency of the file
  divided by that of the original (mean absolute step between neighbor
  pixels): 1.00, 0.78, 0.88, 0.43, 0.91, 0.73. The model is not stable on this
  texture: a mirror of the input changes the result of a frame from foam to
  soft (frame 1: 10.5 as it is, 5.3 mirrored top to bottom). A mean of the 8
  mirrors and turns was soft in all frames and still uneven (3.5 to 5.2). A
  scan of all wall animations of the level archives for a frame below 0.65 of
  the median of its animation found only `water` (and frames that are empty
  or transparent). `NAME_METHOD` gives `water` the model UltraSharp: the
  dither stays a fine grain, the same in each frame (0.63, 0.59, 0.60, 0.59,
  0.60, 0.59). The foam of the other frames is gone: it was not in the
  original.
- The face of the dwarf (`dwf_mf01` of `PRISON.EGG`, the skin of
  `dwf_mfw.pof`, the only dwarf creature model) had eyes that did not match.
  Each eye of the original is three thin creases (palette 161 to 168 on skin
  176 to 184, rows 20 to 29). x4plus kept the creases of the right eye and made
  the left one curved fold with a bag below it. UltraSharp keeps the three
  creases of each eye, with the grain of the dither on the skin.
  `REGION_FIX` takes the UltraSharp result in the two eye boxes (source pixels
  13..25 and 39..52 across, 17..31 down), blurred with sigma 1 output pixel,
  with the mean color of the x4plus result there, blended with a soft edge
  (sigma 6 output pixels). The rest of the face (nose, beard, helmet) is the
  x4plus result as before.

The method and the region are in the cache key of these bitmaps; version 6
marks every archive directory as stale, so a run links every file again and
upscales only these 7 bitmaps.

## The water flipbooks (version 7)

The report of the stream of 2026-11-26: the running water had a pixelated
texture, while the flat water of version 6 looked clean. All water and liquid
flipbooks of the archives (each in 19 archives: the 18 level archives and
`PORTAL.EGG`, 64 x 64, all wall textures of the tables except `lava02`):

| Flipbook | Frames | Use | Before | After |
|---|---|---|---|---|
| `water` | 6 | flat water, floors | UltraSharp (version 6) | the same |
| `wfal01` | 8 | running water: the overlay of the streams (HUB3, 7 sides), the base of sides in 11 levels | detail, foam streaks, 0.99 to 1.14 | UltraSharp, 0.84 to 0.88 |
| `wfal02` | 8 | waterfall (BEHOLDR2, IBBALAR1) | detail, 0.34 to 0.40 | UltraSharp, 0.29 to 0.31 |
| `wfal04` | 8 | waterfall with see-through edges (0x4F, transparent) | detail, 0.17 to 0.20 | UltraSharp, 0.17 to 0.21 |
| `water2` | 6 | see-through water (0x6F, BEHOLDR2) | detail, 0.09 to 0.13 | UltraSharp, 0.10 to 0.14 |
| `water1` | 8 | brown water (no rendered side in the levels) | detail, 0.51 to 0.63 | UltraSharp, 0.54 to 0.64 |
| `lava`, `lavaflo`, `magma` | 6, 15, 6 | lava | detail | the same |
| `lava02` | 12 | lava (IBBALAR2) | x4plus | the same |

The numbers are the high frequency of each frame: the mean absolute step
between neighbor pixels of the file reduced to the original size (mean of
4 x 4 blocks), divided by that of the original frame (RGB of the palette).
Above 1 the file has detail that the original does not have. `water2` and
`wfal04` are low because most of their texels are effect colors or
transparent (filled before the upscale).

`NAME_METHOD` gives the five other water flipbooks the model of the flat
water. The foam of `wfal01` is gone and its frames agree (spread 0.04
against 0.15), as the frames of `water` did in version 6. The lava keeps its
method: UltraSharp made a grain on `lava` and `magma` and stair steps along
the smooth bands of `lava02` (the step across the edges of the 4 x 4 source
cells against the steps inside them: 1.10 to 1.31 against 1.04 to 1.16).
The cache key has the method, so a run upscales only the 38 frames of these
five flipbooks again (the test run of DROW with `water` and the lava: 83 files in 10 s).

Test sheets (frame 0, the original at 4 times next to the file, before and
after) were made in a scratch directory with `--out DIR --only drow --names
...`; the set in `gamedata/hd` was made again with one run.

The place of the report (a screenshot of the user with the set of the content
pack, version 6): HUB3, the room of the waterfall `wfal04` (segments 212
and 213, side 5) and the sign of the temple of Clangeddin. The floor had two
styles. The rapids, the sloped and flat floor faces where the water runs
(segments 213, 214, 215, 217, 227, 228, 230, side 3: base `water`,
overlay `wfal01`, an opaque eclip that covers the base), had sharp bright
streaks: the merged texture takes the upscaled file of `wfal01`
(`ASCENT_GPU_HDLOG=1` lists `merge portal/water_N + wfal01_N`), made with
the method `detail` in version 6. The rest of the floor (base `water` with
no overlay or with the transparent overlay `empty`) is the file of `water`,
UltraSharp since version 6: soft dark blotches. The small green specks are
in both originals: palette 55 to 59, a dark green ramp (RGB 1 13 9 to 7 25
19 of 63), 78 texels in `water#0` and 89 in `wfal01#0`. With version 7 both
are UltraSharp: the rapids keep their streaks along the flow, with the same
grain and specks as the water around them. Test: HUB3 `-testpos
-62.5,-30,5,180,15`, frame 200 (after the upscaled loads), GPU renderer with
`-hddir` at the set of the content pack and at the new set; the mean
difference in the floor area is 2.9 of 255. The content pack `textures` still
has version 6 until it is made again and uploaded.

## The fire flipbooks (version 8)

The report: the breath of the dragon Tamaramagil (DROW2, `dtudragon.c`) is a
stream of small flames; in the GPU renderer, from the front and near the
camera, they had hard edges and blocks. The set always had files for the
sprites of the explosions (every bitmap of every archive), and the loader
finds them by name like any other bitmap (render.c `gpu_bitmap_source`: the
address of the pixels in `GameBitmaps`, then the archive and the name;
gpu3d.c `hd_path` in `index.txt`). Three things made the edges hard:

- A 64 x 64 bitmap counts as a tiling texture: the flipbooks were upscaled
  as the middle of a 3 x 3 tiling of themselves, not as single pictures.
- The alpha of the file is the mask with a ramp of one output pixel
  (`smooth_alpha`), and a single pixel (a spark) is kept as a disk or a
  square of one source pixel.
- The engine cut that alpha at 128 (gputex.c `load_layer`) and the shader
  draws a sprite with an alpha test at 0.5 (`gpu_bitmap` in gpu3d.c: a
  quad with `transparent`, `clamp`, no light, no blend).

`SOFT_SPRITES` gives these flipbooks a model, no tiling (mirror padding of
16 pixels) and a soft alpha (`fire_alpha`):

| Flipbook | Frames | Vclip, use | Before | After |
|---|---|---|---|---|
| `flame-hi` | 6 | 11: the fire burst of a fire hit (Burning Hands on a creature), the flames of the breath | x4plus, tiling, alpha cut | x4plus, glow |
| `firbal1` | 10 | 12 and 14: the Fireball missile, the column of Flame Strike, one flame in 8 of the breath | x4plus, tiling, alpha cut | x4plus, glow |
| `fbxpld` | 12 | 111: the hit of Fireball and of Flame Strike | x4plus, tiling, alpha cut | animevideov3, cover |
| `smoke1` | 10 | 135: the smoke trail of Fireball and of Melf's Minute Meteors | x4plus, tiling, alpha cut | animevideov3, cover |

Each is in the 16 to 18 archives of the levels and `PORTAL.EGG`, 64 x 64,
transparent (flag 0x01), with no effect color except two texels 0xCF of
`smoke1`. None is a wall texture or an animation of `anim_merge`.

- glow: the alpha of `smooth_alpha`, dilated by 2 output pixels
  (`GLOW_DILATE`) and blurred with sigma 2 output pixels (`GLOW_SIGMA`): an
  edge ramp of about 1.5 source pixels; a spark stays a bright dot with a
  soft rim. A blur of the mask alone (no dilation) made the sparks of
  `flame-hi` frames 2 to 5 faint and olive on a dark wall; a coverage alpha
  removed them.
- cover: the mask blurred with sigma 1 source pixel (`COVER_SIGMA`),
  upscaled bicubic, smoothstep from 0 to 0.8 (`COVER_FULL`): the dithered
  smoke of the last frames (opaque and transparent pixels mixed) becomes a
  translucent smoke instead of a pattern of dots.

The models, chosen on sheets of the frames (original at 4x, each model, each
alpha, on a dark and a light background): on `flame-hi` x4plus gives clean
flame tongues; UltraSharp keeps the palette dither as a grain;
animevideov3 is a little softer than x4plus. On `firbal1` (a ball of rock
with glowing cracks) x4plus is clean and animevideov3 puts blue patches in
the cracks. On `fbxpld` and `smoke1` (photographic clouds) x4plus makes
glassy facets that are not in the original, UltraSharp a grain of the
dither, and animevideov3 smooth clouds.

`index.txt` has a 4th field `soft` on the lines of these files. The engine:

- gpu3d.c reads the field (`hd_name.soft`); a job of a single bitmap with a
  soft file (`plat_gpu_hd_job.soft`) keeps the alpha of the file (gputex.c
  `load_layer`), not cut at 128. A bitmap with effect colors in the alpha
  (`BM_FLAG_DTU_EFFECTS`) and a merged texture never are soft.
- `gpu_bitmap` (the sprites of `g3_draw_bitmap`) sets
  `plat_gpu_material.blend` for such a texture. gpu.c draws it with the
  blend `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` for the color and `ZERO,
  ONE_MINUS_SRC_ALPHA` for the alpha of the image (the mask of the ambient
  occlusion goes to 0 under the fire), without writing the depth, and
  discards texels with alpha 0. render.c draws the segments from far to
  near and sorts the objects of a segment by distance (`sort_func`), so the
  flames blend in that order; a later polygon of the level still hides a
  flame by the depth test.
- A polygon of a model with such a texture keeps the alpha test at 0.5.
  The 8-bit form, the software renderer and a set without `soft` (version 7
  and before) draw as before.

Test sheets: `--out DIR --only drow --names flame-hi,firbal1,fbxpld` in a
scratch directory (28 files, 4 s). In the game (GPU renderer, 1920 x 1440,
DROW2, the player at 60 and at 34 units in front of the dragon and to her
side, `dragon=3`, the screenshot 15 frames into the stream; BONES, a level 9
Fireball at a zombie) the flames have soft edges and no blocks, the sparks
are soft dots, and the trail and the hit of Fireball are clouds with no
hard outline. The set of version 7 was kept in a scratch copy for the
pictures before.

The run that made version 8 from a set of version 7: 2 min 37 s for all 57
archives (all files linked again, 38 bitmaps upscaled: the 28 frames of
the first three flipbooks and the 10 of `smoke1`). Changed files in
`gamedata/hd/textures`: 705 of 60396 (the 630 archive files of the four
flipbooks, 70.5 MB counted as separate files, 38 distinct PNG files; 18
`index.txt` files; 57 `.method` files).

## Making a set of version 5

One run: `python -I tools/textures_hd.py`. Version 5 marks every archive
directory as stale, so every file is linked again from the cache; only the 471
frames of the door clips are upscaled again.

## Making a set of version 4

One run: `python -I tools/textures_hd.py` (all archives, no `--force`). Version 4
marks every archive directory as stale, so every file is linked again from the
cache. New cache keys, so these are upscaled again: every bitmap with the
transparent flag and color 255 that is not in these animations (32970 archive
files), and every frame of the 81
animations above (15156 files); about 2800 distinct bitmaps (1574 wall bitmaps
with the detail method, 1031 tiling and 201 other bitmaps with x4plus). The other
bitmaps come from the cache. Measured: KOBOLDS alone, with the cache of version
3, upscaled 2434 bitmaps (most of them shared with PORTAL) and linked 700 in 5
minutes 29 s (about 7.5 per second). The other archives share most of these, so
the full run should take about 10 to 15 minutes (estimate; the full run was not
made).

## The first opening of a door, the first use of a switch

The report: the first time a door opens or a switch is used, some frames show
a texture that is wrong but similar; the next uses look right. Two faults of
the GPU renderer caused it. The software renderer and the GPU renderer without
the upscaled set show the 8-bit texture at each frame, so neither has it.

- The GPU texture of a bitmap is made the first time a view draws it, and its
  upscaled form is loaded in the background (gputex.c, 2 threads). A door
  animation shows a new merged texture (`texmerge.c`: the wall under the door
  frame) about every 2 frames, and a switch shows a new one when it is used.
  So the first opening showed each frame of the animation in the 8-bit form
  for one frame, then the upscaled form: a blurred door that went sharp
  frame after frame. The next door of the same kind found all its textures.
  Descent 1 pages in the frames of all wall animations at the level start
  (`paging_touch_walls`, paging.c). The port now does the same for the GPU
  textures (render.c `gpu_preload_level`): after the first view of a level,
  and again when the RGB textures are made again, it makes the textures of
  the frames of the door animations of the walls (as `wall_set_tmap_num` sets
  them), of the textures that the triggers and the traps put on their sides
  (`tex_on`, `tex_off`, `tex_enabled`, `tex_disabled`, converted as
  `set_side_tmap2` does), and of each frame of the animated textures (the
  effects) of the sides, each merged with the other texture of its side, and
  asks for their upscaled forms. It runs after the first view so that the
  upscaled loads of that view come first. `ASCENT_GPU_NOPRELOAD=1` turns it
  off (a test aid).
- The merged wall textures have a table of 1024 entries in gpu3d.c (`Mt`).
  Its first place came from `tc_hash`, which gives a place in the table of
  the bitmaps (`Tc`, 4096 entries): three of four merges were written after
  the end of `Mt`, into the entries of `Tc` that follow it in memory. A merged
  wall could then show the texture of another merge, and a bitmap could get
  the texture number of a merge (found with a debugger: `Tc[1408]` held the
  pointers of merge 3138, and a texture number made of a pointer made the
  game hang in `tex_info_get`). The place is now masked to the size of `Mt`.

The time of the preload: BANE 89 pairs of textures (71 of the doors, 0 of the
triggers) and 437 bitmaps; CRYPTS 185 pairs (160 of the doors, 6 of the
triggers) and 679 bitmaps; 130 to 230 ms at the first frame of the level, and
up to 0.7 s in other runs while the machine was busy. The upscaled loads were done 5
to 8 frames later. Most of the time was the name of each texture: one
glGenTextures call cost about 0.45 ms, so gpu.c now takes the names 256 at a
time. A texture made between the draws of a view cost several times more
than in one batch, so the preload makes all the textures at one time.
`ASCENT_GPU_HDLOG=1` prints the counts and the time ("preload of level"), the
frame when the upscaled loads are done, and each sweep of the merge table.

Tests (`-hidden -fixedfps 30`, `ASCENT_SHOT_PRESENT` for each frame):

- BANE `-testpos -160.4,19.1,100,0 -testmove
  wait:20,use:1,wait:60,left:12,wait:20,use:1,wait:60`: the door of wall 66
  (`dup5` on `wood91`, clip 25), then the door of wall 65, of the same kind.
  Before, the first door showed the 8-bit form at presents 97, 100, 102, 105,
  107, 110 and 112, each replaced one present later (mean difference of the
  view to the upscaled form 3 to 6 of 255); the second door had none. After,
  each frame of the first door is the upscaled form when it first shows (mean
  difference 0.0 to 1.1 to the upscaled frame of the run before, from the
  torches), as the second door.
- CRYPTS `-testpos -21,-12.2,423.5,269 -testmove wait:20,use:1,wait:40`: the
  switch of trigger 2 (segment 544 side 4, `swtch03b` to `swtch03a` on
  `eroc39`). With `ASCENT_GPU_NOPRELOAD=1` the first present after the use
  shows the 8-bit form; with the preload it shows the upscaled form.
- Static walls: a wall seen for the first time after a turn showed its 8-bit
  form for a frame. The preload now also makes the textures of all the other
  sides that a view can draw (`WID_RENDER_FLAG`), after the doors, the
  triggers and the effects. It adds 30 to 100 pairs to a level and 10 to 30 MB
  of upscaled textures: BANE 142 pairs (89 before), 490 bitmaps; CRYPTS 247
  (185), 741 bitmaps, 254 MB in all (236); HUB1 104 (71); YPINN 116 (78);
  MYRKUL 342 (245); DROW1 201 (162). No sweep in any of these runs. Test: BANE
  `-fixedfps 30 -testmove wait:60,left:150,forward+run:100,right:80` with
  `ASCENT_GPU_HDLOG=1`: after "the upscaled loads are done" the wall textures
  `eroc42 + empty` and `stone223` were still asked for (shown 8-bit first);
  with the preload of the static sides only the sprite frames `cswound_*`
  are. `ASCENT_GPU_NOPRELOAD=2` leaves out the static sides (a test aid).
- Not covered: the bitmaps of objects (sprites, models) are made when a view
  first draws them; the merge table keeps 1024 entries and its sweep (60
  frames, at 3/4 full) or the budget sweep (768 MB) can delete a preloaded
  texture before its door is used in a large level.

## Making a set of version 2 again

One run does it: `python -I tools/textures_hd.py` (all archives, no `--force`).
Version 3 marks every archive directory as stale, so every file is linked again
from the cache. A bitmap whose cache key changed is upscaled again:

- every tiling bitmap (the 3 x 3 padding is in its key): 56916 files of the 57
  archives, 3653 distinct bitmaps (2083 64 x 64 wall textures with the detail
  method, 1534 64 x 64 bitmaps with x4plus, 36 wall textures of other sizes),
- every bitmap with an effect color (the fill of those colors is in its key; the
  same files as `--redo-effects`).

All other bitmaps come from the cache. Measured at the 3 x 3 padding: 7.7 detail
and 15 x4plus bitmaps of 64 x 64 per second, so the tiling bitmaps take about 6
to 7 minutes; with the effect bitmaps and the links of 60282 files, 10 to 15
minutes. Then the interface: `python -I tools/ui_hd.py --force --only
10,195,27,...,56` (32 pictures, about 30 s).

## Time and size

RTX 4080 SUPER, 16 GB. PORTAL: 3293 distinct bitmaps upscaled in 97 s (34 per
second). With the cache, each level archive takes 8 to 15 s (4 to 128 new bitmaps)
and each player archive under 1 s. All 57 archives: about 5 minutes.

Disk: 3855 distinct PNG files, 397 MB. The 60282 archive files are hard links to
them; counted as separate files they would be 6.5 GB (about 350 MB per level
archive, 2.5 MB per player archive). Copy the directory with a tool that keeps hard
links, or copy only the archives you need.

## Interface pictures

`tools/ui_hd.py` upscales the interface pictures of FEBGND.DAT into
`<hd>/ui/febgnd_<n>.png` (and `<hd>/ui/index.txt`). It uses the environment and
the models of `textures_hd.py`. The game shows them with the GPU renderer and the
setting Upscaled textures (`engine/main/dtuhdui.c`, the compose step of
`engine/platform/gpu.c`); option `-nohdui` turns them off.

| Picture | Size | Use | Method |
|---|---|---|---|
| 63, 64, 65, 67 | 640x480 | play screen: main, map, inventory (and hot keys), statistics | detail |
| 57, 58, 59, 60, 61 | 320x200 | the same at 320x200 (`-lowres`; 60 is the hot key screen) | detail |
| 86, 85 | 640x400, 320x200 | stone of the newmenu boxes (Escape menu, message boxes) | detail |
| 193 | 640x480 | title | x4plus |
| 18, 19 | 640x480 | front end screens 10 and 11 (Interplay logo, demo title) | x4plus |
| 0 | 640x480 | screen 1, Blackstaff | x4plus |
| 10, 11 | 640x480 | screen 0, Waterdeep (the main menu), and its menu box stone | ultrasharp, detail |
| 1 | 640x480 | screen 4 and the menu box stone of screens 1 and 4 | detail |
| 2 to 5, 6 to 9 | 640x480 | character creation 1 to 4 and their menu box stones | detail |
| 14, 15, 25 | 640x480, 414x349 | screen 3, the market, its menu box stone, the quantity window | detail |
| 26 | 640x480 | screen 2 | x4plus |
| 12, 155 to 160 | 640x480 | loading pictures | x4plus |
| 161 | 640x480 | death picture | x4plus |
| 195 | 620x122 | Khelben, the talking head of the Blackstaff screen: 7 mouth pictures of 82x117 | face, RGBA |
| 27 to 56 | 160x160 | Blackstaff screen, the pictures that the speech shows (`~pN`) | x4plus |

The files keep the pixel grid of the original picture (57 is 1920x1200, 6
times 320x200). gpu.c draws each file in the rectangle of the original
picture in the frame, so the file takes the shape of the frame on the screen:
with the 4:3 screen of the low resolution (`Dtu_fix_lowres_aspect`) a 320x200
picture is 1.2 times taller than its file, as the original was on the DOS
monitor. Do not make the 320x200 files at 4:3.

Picture 10 (Waterdeep) was made with x4plus first: the model made the grass a flat
green. The grass of the original is a fine grain of green, brown and red pixels.
4x-UltraSharp keeps the grain and the detail of the rocks and roofs (8x8 block
difference max 3.7, mean 0.74, against 12.8 and 2.49 for the detail method). Make
it again with `python -I tools/ui_hd.py --only 10 --force`.

A language with pictures of its own (docs/formats/lang.md, "Pictures of the language") has an RGBA
overlay `<hd>/lang/<code>/ui/febgnd_<n>.png` for a picture with English words; gpu.c draws it over
the upscaled file when it makes the texture.

Not upscaled: the portraits (87 to 150), icons (22 to 24), buttons (69 to 84) and
cursors: they are drawn as the changing part of the interface. Picture 13 (the menu box stone of screen 5)
has a palette that differs from the palette of its screen (picture 12), and
picture 16 is a flat blue screen; both are left out.

Palette: the in-game pictures have the colors of PALETTE.256 in their entries
(checked for 57 to 67, 85 and 86: every used color is equal). A front end
picture that is drawn over a screen is shown with the palette of the screen: the
menu box stones 1, 6 to 9, 11 and 15 have the same colors as their screens for
every used index, and 25 has the colors of 14. The tool uses the palette of the
entry, and the palette of 14 for picture 25. The Blackstaff screen shows 195 and
27 to 56 with the palette of picture 0: 195 has the colors of 0 for every used
index, 27 to 56 differ in one used index; the tool uses the palette of 0.

### The talking head of Khelben

The Blackstaff screen (picture 0) has the face of Khelben in its mouth picture 2.
While the speech plays, `bs_frame` (dtufe_town.c) draws one of the 7 mouth
pictures of 195 over it each 0.1 to 0.15 seconds with `fe_blit_part`, with color
255 not drawn. Before the fix only the draws with no transparent color went to the
upscaled layer, and 195 had no file: the compose showed the upscaled picture 0
where a frame pixel still had the color of picture 0 and the 8-bit frame
elsewhere, so mouth picture 2 looked upscaled and the others were a mix of
upscaled and 8-bit pixels.

Now `fe_blit_part` passes its transparent color (`dtu_hdui_blit_key`,
`plat_gpu_ui_draw_key`). The map of the frame keeps the pixels below where the
picture has that color, and the layer draws the file of 195 with alpha
blending. ui_hd.py makes 195 as RGBA: color 255 gets the colors of its opaque
neighbors before the upscale (as the transparent pixels of the textures), the
alpha is the mask upscaled bicubic and cut at 0.5, and each 82 pixel wide cell is
upscaled alone, so no mouth picture takes colors from the next one. A draw with
a transparent color needs such an RGBA file; 195 is the only picture of the list
that the game draws so.

Method `face`: x4plus plus 0.15 times the fine detail of UltraSharp (the method
`detail` with 0.6 for the stone and wood art). Compared on the 7 mouth pictures
at the 3x output: nearest shows the palette dither; x4plus alone is clean but
flat (forehead wrinkles and the beard become smooth areas); detail 0.6 and
UltraSharp bring the dither back as a grain and a grid on the skin;
animevideov3 makes blotches; 0.3 and 0.15 keep wrinkles and the beard, 0.15 with
no grain. The 7 frames are made with the same method in one file, so they match
each other.

Other faces: the portraits of the character creation (FESEQ, drawn at 245, 145)
and the portraits 87 to 150 have no upscaled file and are drawn over no
upscaled picture of the same face, so all their frames stay 8-bit alike. No
other screen animates a face over an upscaled picture.

Size: 3 times for the pictures of the 640x480 screen (1920x1440 at 1:1, a 1440p
monitor; at 1080p the image is reduced to 2.25 times, at 4K enlarged 1.5 times
with the linear filter), 6 times for the 320x200 screen. The 4x result of the
models is resized with bicubic filtering (with antialias for the reduction).
The mean color is moved back to the mean of the original; the mean color of 8x8
blocks of the original differs by 1 to 5 (of 255) on average.

Time and disk: 38 pictures in 67 s, 115 MB; the 31 pictures of the Blackstaff screen (195, 27 to 56) add 27 s and 9 MB.

The presentation: the engine calls `dtu_hdui_loaded` when a picture is loaded
(the reference pixels, before the buttons are drawn into the play screen) and
`dtu_hdui_blit` when the pixels of a picture are copied to the screen (dtudisp.c
`blit`, newmenu.c `nm_draw_background` and `nm_restore_background`, dtufe.c
`fe_frame_begin`, `fe_blit_part` and the scaled loading picture). The shadow of
the newmenu box is passed as darkenings (`dtu_hdui_menu_shadow`). gpu.c keeps a
list of the draws, makes a map of the expected index of each frame pixel, and
shows the upscaled layer where the frame pixel has the expected index; a
darkened pixel gets the luminance ratio of its two indexes. A frame pixel with
5 or more different neighbors stays 8-bit. A draw is dropped when less than 40%
of its pixels still match. The colors go through the curve from the palette of
the last palette load (`gr_current_pal`) to the shown palette, so the fades and
flashes apply.
