# Infravision

Addresses are linear addresses of the image of DTUGAME.EXE 1.3 with object 1 at
0x10000 (`gamedata/re/dtu_flat.bin`, `gamedata/re/all.asm`). **V** marks a fact read in
the code or the data. **G** marks a guess.

## How the player gets it

- **V** The spell Infravision (25), the Potion of Infravision (item code 0x52), the
  Assembled Spider and the worn items with the effect bit 6 (ring 22, weapon 54) set the
  effect bit 15 (0x8000) of the int at 0x88 of the player object, with a timer at 0xC4.
  See [combat.md](combat.md).
- **V** The effect bit does not change the view. Only the key N (action 47/48, see
  [controls.md](controls.md)) turns the view on or off.

## The key N (0x54632)

- **V** The key gate 0x545F5 lets the key act at most once in 36.4 clock ticks.
- **V** The player can use it when the race value is not 0, or when the byte 0x2B069D
  has bit 0x80. The race value (0x5464B to 0x54664) is 5 when bit 0 of the byte at
  0x1D33FB (+ player * 0x1B37) is set, else the byte at 0x1D33B6. These are the race
  byte and the half-elf flag that the statistics screen uses for the race name
  (0x32C94, 0x32CBB; the port: `pa.race`, `DTU_PF_HALF_ELF`). So every race but
  human has infravision: dwarf, elf, dark elf, halfling and half-elf. (An earlier
  version of this note named the byte `P+0xB5`, the `infravision` field, which is
  wrong; the port then gave infravision to no race.) 0x2B069D is the byte 0x89 of the player object at 0x2B0614, so the test is the
  effect bit 15 of the int at 0x88 (the spell, the potion, the ring).
- **V** Without it: TEX 819 "You don't have Infravision." (0x396A78).
- **V** Else the byte 0x14C4DA changes (0 to 1 or 1 to 0). On: TEX 820 and bit 8 of the
  byte 0x2B07C2 (byte 0x1AE of the player object) is set (0x546A8). Off: TEX 821 and the
  bit is cleared (0x546C2).
- **V** No other code clears that bit (only 0x546C2, and the writes of the whole byte at
  0x424D7, 0x5A326 and 0x68135). The view stays on when the effect time ends.

## The frame (0x76C7C)

- **V** At the start of the render of a frame (0x76CC8): 0x14C4DA = 1 when the viewer
  object (0x13D628) has bit 8 of its byte 0x1AE and the dword 0x1468D0 is not 1, else 0.
- **G** 0x1468D0 is a screen state that 0x40C6D sets. Its value 1 was not identified.
- **V** 0x59C3C copies the player object and saves 0x14C4DA and the byte 0x2B07C2 into
  0x13CE20 and 0x13CE24 (a second view, not decoded).

## The pixel rule (0xEC4D7, copy 0x105A81)

0xEC4D7 and 0x105A81 are the two copies of the light function of the texture mappers.
0x105C60 copies the globals 0x14C4CE to 0x14C4E1 into 0x1512BD to 0x1512D5 for the
second copy (0x14C4DA goes to 0x1512D5, 0x14C4DB to 0x1512CA). Input: AL is the texel
color, AH the light row. The table 0x463CB0 is the fade table (row * 256 + color).

- **V** When 0x14C4D8 is 0 the function returns `fade[row][color]` and nothing else.
  0xEA503 to 0xEA525 set 0x14C4D8 for each polygon: 1 when the bitmap has flag 0x20
  (byte 9 of the bitmap), or 0x14C4CD (the tint of the current object) is not 0, or
  0x14C4CF is not 0.
- **V** The texture merge cache (0x7AB89) sets flag 0x20 on each merged bitmap (a wall
  with an overlay texture).
- **G** Which other bitmaps have flag 0x20 was not found. The port applies the rule to
  all lit textures.
- **V** The colors 0xCF, 0xBF, 0xBE, 0x6F, 0x6E, 0x6D, 0x4F, 0x4E and 0xFF are handled
  before the infravision test (the effect colors, see effectcolors.md, and
  transparency).
- **V** The rule (0xEC5E5 to 0xEC62D), with `lit = fade[row][color]` and `step = lit &
  0x0F`:
  - 0x14C4DA = 0: the color is `lit`.
  - `step >= 14` (0x14C4DB, the constant 14 in the data): the color is
    `0xC0 | (color & 0x0F)`, the unlit texel in the row 0xC0.
  - `step <= 8`: the color is `0xB0 | step`.
  - Else (9 to 13): `lit`.
- **V** PALETTE.256 has 16 rows of 16 colors. In each ramp row index 0 is the brightest
  and 15 the darkest. Row 0xC0 is a red ramp (57,29,29 to 7,0,0, 6-bit values). Row
  0xB0 is a pink orange ramp (63,44,41 down). So dark texels show at the brightness of
  the unlit texture in red, bright texels become pink orange, and the middle steps keep
  their color. The whole view turns red.
- **V** No creature flag, no object type and no light source is tested: the rule is the
  same for all pixels that use the function.

## The port

- `engine/main/dtuinfra.c`. The state is `DTU_PS_INFRAVISION` (0x08) of
  `Dtu_player_state` (the byte 0x1AE). The key N (dtudisp.c) also accepts the effect
  bit `DTU_FX_INFRAVISION`.
- Original effect: a copy of the fade table with the rule above replaces
  `gr_fade_table` while the 3D view of the player is drawn. The software texture mapper
  and the GPU renderer (8-bit and RGB forms, both light through the fade table) show it.
- Improved infravision (`Dtu_improved_infravision`, `ImprovedInfravision` in
  `ascent.cfg`, item "Improved infravision" of the Graphics menu, on by default). It
  needs the GPU renderer in the RGB form; else the original effect is used. Each
  polygon gets a heat (`G3_gpu_heat`, `PLAT_GPU_HEAT_*`):
  - hot: an object that casts light (fire, missiles, light objects, scenery with
    `lightcast`) and a face whose texture has light (`TmapInfo[].lighting`). Only the
    bright texels glow, from orange to yellow white.
  - warm: living creatures, the player and the characters. A red ramp of the texel
    brightness, visible in the dark. Grey texels (metal) get less of it.
  - cold: creatures with the table flag UNDEAD (17 of the 112 creature names of the tables, for
    example the skeletons, Zombie, Ghoul, Ghast, Mummy, Lich3), dead creatures, the
    first person model (the arms and the weapon), and the rest of the scene: a dark
    blue of the lit brightness.
  - No table has a flag for constructs (golems). No creature is cold for that reason.
- Test options: `-infravision` turns the view on at the first frame, `-noimprovedinfra`
  turns the improved mode off for one run.
