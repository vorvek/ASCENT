# The in-game interface of DTUGAME.EXE (version 1.3)

Status: research notes for the play screen and the screens that open during
play. Addresses are linear addresses of the LE image with object 1 at 0x10000
(the file `gamedata/re/dtu_flat.bin`, disassembly `gamedata/re/all.asm`).
"Verified" means read from the code or the data of the executable, or checked
against the files of the install. "Guess" means not verified.

The port code for this is `engine/main/dtudisp.c`.

## Source files of the original

The assert strings name the files. Functions found so far:

| File | Address | Function (name is a guess unless the exe prints it) |
|---|---|---|
| game.c | 0x4052F | `init_game_screen` (prints "init_game_screen: Screen_mode = %d") |
| game.c | 0x40403 | load the interface picture and the button list of a mode |
| game.c | 0x404A5 | load one FEBGND entry as the interface picture |
| game.c | 0x407EB | select interface mode (prints "voidage select interface(mode)") |
| game.c | 0x40868 | leave the current screen (back to the saved mode) |
| game.c | 0x408A2 | toggle inventory (mode 3) |
| game.c | 0x408F5 | toggle map (mode 4) |
| game.c | 0x4093A | toggle statistics (mode 5) |
| game.c | 0x44F1B | compute all layout variables for low or high resolution |
| displays.c | 0x3420C | display init: icons, cursor, hrisbtn.pch |
| displays.c | 0x324DA | update all areas from the dirty flags (called each frame) |
| displays.c | 0x2FB10 | per-frame input of the displays (mouse on items, scroll bar) |
| displays.c | 0x33F55 | message box init (40 lines, visible count) |
| displays.c | 0x34682 | draw the message box of the main screen |
| displays.c | 0x34998 | add a message line (not read in detail) |
| displays.c | 0x34530 | color for a percentage (red, yellow, green) |
| displays.c | 0x36E3F | icon number to position in the icon sheet |
| displays.c | 0x3627E | load a portrait picture ("ImageNum < NumPics") |
| displays.c | 0x36815 | scroll bar create (asserts name the `SBarDef` fields) |
| displays.c | 0x3654C | scroll bar draw |
| displays.c | 0x37253 | experience to the next level |
| displays.c | 0x3601A, 0x3602E, 0x36038, 0x36042, 0x36024 | button actions: select mode 3, 4, 5, 0, 6 |
| displays.c | 0x360D5 | button action: rest |
| displays.c | 0x3614B, 0x35FD6 | button actions: use, drop (guess from the screen position) |
| displays.c | 0x37180 | button action: spells toggle (guess from the screen position) |
| dispbutn.c | 0x98E65 | button list of a mode (tables 0x14437F high, 0x1443A3 low) |
| dispbutn.c | 0x98A5D | create the button images of a list |
| dispbutn.c | 0x98E99 | button press: show the other image, call the press action |
| dispbutn.c | 0x98F38, 0x9905A | button release: call the release action |
| dispbutn.c | 0x9881C | load `hrisbtn.pch` (one 76x32 button, up at (2,36), down at (2,2)) |
| dispcanv.c | 0x992DB | create the areas of a mode |
| dispcanv.c | 0x9A545 | destroy all areas |
| dispcanv.c | 0x9ADAA | create one area |
| dispcanv.c | 0x9AC2F, 0x9AC66, 0x9ACA0, 0x9ACE1, 0x9AD2B, 0x9AD48, 0x9AD65 | area creators for the types 5, 4, 3, 6, 2, 1, 0 |
| dispcanv.c | 0x9C890 | draw a border inside an area |
| dtumouse.c | 0xB0D45 | create the cursor from a source bitmap |
| frontend | 0x800A4 | load FEBGND entry N into a bitmap ("JRG: Error loading PCX file %u") |
| frontend | 0xAEEC4 | add the language number to a FEBGND entry number |

Library functions used above (verified by their use): 0xD9334
`gr_set_current_canvas`, 0xD9268 `gr_init_sub_canvas`, 0xD9CEC
`gr_find_closest_color(r,g,b)` with 6-bit values, 0xE0820 `gr_bm_ubitblt`,
0xE1280 masked blit, 0xE7640 `gr_rect`, 0xE26F8 `gr_string`, 0xDD02D `sprintf`,
0xD7B80 the assert function (ecx line, ebx file, edx expression), 0xD7B00
`mprintf`.

## Resolution

Verified:

- The flag at 0x13A94B selects the interface: 0 low resolution (320x200),
  1 high resolution (640x480). All code tests this flag.
- The option `-lowres` clears the flag (0xB811A). The options menu sets it
  (0x590F4) and then calls 0x44F1B.
- `dtu.cfg` is read with the key `Graphics` (0x2B754). The writer at 0x2BC33
  writes the flag with the key of `DigiDeviceID` (format `%s=0x%x`). This is why
  the `DTU.CFG` of the install starts with `DigiDeviceID=0x1` followed by the
  real `DigiDeviceID=0xe018` line. So the saved setting is not read back.
- The static value of the flag is 0.

Guess: the manual calls 640x480 the normal mode and its pictures show it. The
port uses the high resolution by default (`Dtu_hires`) and takes `-lowres`.

In high resolution the 3D view is not rendered with 480 lines. The render
canvas has the full width and half the height, and each line is shown twice
(0x40729: the window height is doubled only for the screen). Example: main
screen, canvas 504x139, on screen 504x278.

## Interface modes

The variable at 0x13B616 (the Descent `Cockpit_mode`) has these values
(verified from the jump table at 0x4050B and from 0x40403):

| Mode | Screen | View rectangle (low) | FEBGND entry low | high |
|---|---|---|---|---|
| 0 | main game screen | (63,5) 252x139 | 57 | 63 |
| 3 | inventory | (225,6) 88x78 | 59 | 65 |
| 4 | map | (225,6) 88x78 | 58 | 64 |
| 5 | statistics | (225,6) 88x78 | 61 | 67 |
| 6 | low: hot key screen, high: inventory | (225,6) 88x78 | 60 | 65 |
| 7 | full screen | (0,0) 320x200, high 640x239 doubled | none | none |
| 8 | letterbox | width of the screen, 140 lines, centered | none | none |

Modes 1 and 2 are not valid ("JRG: Invalid game interface mode"). The entry
numbers are in the tables at 0x13B63B (high) and 0x13B65F (low). The previous
mode is saved at 0x13B61A when a screen opens, and the same key or button
returns to it. At the start of a level the mode is 7 (0x41FD0), then the game
selects mode 0 (guess, the call was not traced).

High resolution rectangles: x and width are doubled, y is doubled, the height
value is the same and the screen shows each line twice. Main view: (126,10)
504x278. Game port of the screens: (450,12) 176x156.

## Art

Verified by loading code and by comparing the palettes:

- All pictures of the interface are entries of `FEBGND.DAT` (see
  `docs/formats/jrg.md`). Each entry has its own palette of 768 bytes. The
  entries 57 to 61, 64, 66, 67, 87 to 154 have exactly the palette of
  `PALETTE.256`. The entries 23, 24, 63 and 65 differ in at most three colors
  (255, and 206, 207, 223 for entry 23). The game draws them with the game
  palette and does not load the palette of the entry.
- Icons (`InGameIcons`, bitmap at 0x1BD584): entry 23 (128x400, 16x16 icons)
  in low resolution, entry 24 (256x800, 32x32 icons) in high resolution.
  Eight icons in a row. Icon `n` is at column `n & 7`, row `n / 8`. Icon 0 is
  "no icon". Color 255 is transparent. The `icon=` value of the item tables
  is this number.
- Cursor: entry 154 (low, the top left 15x15 pixels) or entry 153 (high, the
  top left 31x31 pixels). The hot spot is the center. The picture is a star.
- Portraits: entries 87 to 118 (100x90, high) and 119 to 150 (50x45, low),
  32 portraits. The number of the portrait is a byte of the player data.
- Buttons: a sheet per language. Entry 70 + language (602x188, high) and
  entry 78 + language (570x60, low). The button record holds the number 69
  or 77, and 0xAEEC4 replaces it by the entry of the language from the
  tables at 0x145718 and 0x145734. The entries 69 and 77 are empty templates
  ("English interface buttons" with rows Main, Inv, Map, Stat). The button
  records hold the positions of both images in the sheet.
- `HRISBTN.PCH`: a PCX file (96x80) with two 76x32 images of the button SPELLS
  of the high resolution inventory screen, both with the text "INV.": (2,2) the
  pressed image (PatchBtnDn), (2,36) the image at rest (PatchBtnUp). 0x9881C loads
  them; the button shows them while a spell list is shown and the next press leads
  back to the items (character.md, section 6).
- The bitmap `cursor` (16x16) of `PORTAL.EGG` is copied to a 16x16 bitmap at
  0x13AD8B in the display init. Its use was not traced.

### The 149 bitmaps of PORTAL.EGG with flag 0x40

These are not in-game interface art. They are front end art (Waterdeep
buttons, character generation portraits and labels, market buttons). Each one
uses the palette that is stored with the `FEBGND` background of its front end
screen. Found by a test of all 43 distinct `FEBGND` palettes (the correct
palette gives the smallest sum of color steps between neighbor pixels) and
confirmed by eye for the entries marked "seen":

| Bitmaps | FEBGND entry with the palette |
|---|---|
| `btn1*` | 2 (seen) |
| `btn2*` | 3 |
| `btn3*`, `p3*` | 4 (seen) |
| `btn4*`, `p4*`, `lb4*` | 5 (the test cannot separate it from entry 18 for `p4f*` and `lb4*`: guess) |
| `btnmk*`, `btnpu*`, `lbm*`, `lbp*` | 14 (seen) |
| `btnhf*` | 26 (seen) |
| `btnwd*` | 27 by the test for 4 of 5, not confirmed (guess: the Waterdeep screen) |
| `btnbs*`, `bsp*`, `btnyp*` | not identified |

## Layout variables

The function 0x44F1B writes 196 integers at 0x13B274 to 0x13B57C. Most are
`scale * constant` with scale 1 (low) or 2 (high). The values below were
computed by running the code of the function for both settings. Low / high.

| Address | Value | Meaning | Status |
|---|---|---|---|
| 0x13B274, 0x13B278 | 320/640, 200/480 | screen size | verified |
| 0x13B27C | 0/14 | not traced | |
| 0x13B280.. | 5/10, 79/158, 50/100, 8/16 | hit points area, main screen | verified |
| 0x13B290.. | 5/10, 67/134, 50/100, 8/16 | name area, main screen | verified |
| 0x13B2A0, 0x13B2A4 | 22/44, 92/184 | current item icon, main screen | verified |
| 0x13B2B8.. | 5/10, 111/222, 50/100, 8/16 | area 0x3BC638, created, never printed | verified |
| 0x13B2D0.. | 63/126, 5/10, 252/504, 139/139 | view of mode 0 | verified |
| 0x13B2F4.., 0x13B304.., 0x13B31C | (6,150,50,8), (6,164,50,8), (33,178) | name, hit points, icon of area mode 1 (not reachable, modes 1 and 2 are invalid) | verified |
| 0x13B324.. | 0, 0, 320/640, 144/144 | a view rectangle, not used by 0x4052F | |
| 0x13B334.. | 225/450, 6/12, 88/176, 78/78 | view of modes 3 to 6 (game port) | verified |
| 0x13B344 | 6/12 | height of the text areas of the screens | verified |
| 0x13B348, 0x13B34C | 81/162, 227/454 | width and x of the character information areas | verified |
| 0x13B350..0x13B360 | 97, 108, 119, 130, 141 (doubled in high) | y of name, hit points, armor class, experience, gold | verified |
| 0x13B54C | 16/32 | icon size | verified |
| 0x13B560.. | 5/10, 188/376, 208/416, 7/14 | item description bar of the screens (area 0x3BC524, red text) | verified |
| others | | inventory, statistics and map positions, used through pointer tables | not decoded |

Tables of positions (pairs of 16-bit x, y; verified):

- Hot key boxes, high: icons at (14 + 55 * i, 411), quantity text at
  (14 + 55 * i, 449) with width 32 and height 12, i = 0 to 9 (pointer tables
  0x1446D0 and 0x1446F8).
- Hot key boxes, low (mode 6 only): icons at (107, 13/48/82/116/150) and
  (161, same), quantity 19 or 20 lines below (0x144680, 0x1446A8).
- Inventory list slots, high: icons at (9 or 51, 9 + 58 * row), quantity 36
  below (0x144540, 0x144568). Low: (5 or 26, 2/31/61/90/119), quantity 20
  below (0x144590, 0x1445B8).
- Portrait area of the main screen: low (5,17) 50x45, high (10,34) 100x90
  (words at 0x144720).
- Message box of the main screen: low (63,150) 244x46, high (131,306) 470x77
  (0x142EF8, 0x142F08).
- Scroll bar of the message box (`SBarDef`, 5 rectangles of 4 integers; low at
  0x142FC8, high at 0x142F58): bar, up arrow, source of the pressed up arrow
  in the interface picture, down arrow, source of the pressed down arrow.
  High: bar (605,324) 26x42, up (605,304) 26x20 pressed image at (354,263),
  down (604,366) 26x20 pressed image at (387,263). Low: bar (307,158) 7x29,
  up (307,150) 7x8 pressed at (300,150), down (307,187) 7x8 pressed at
  (300,187). High slider source (323,271) 25x12 (0x142F48).

## Areas (dispcanv.c)

An area is a record of 0x5C bytes (verified from 0x9ADAA):

| Offset | Content |
|---|---|
| 0x00 | Xpos, Ypos, Width, Height |
| 0x10 | `Print`: a `grs_canvas`, sub canvas of the screen. Its `cv_color` (0x24) is the clear color, `cv_font` (0x28) the font, `cv_font_fg_color` (0x2C) the text color, background color -1 |
| 0x30 | `Bgnd`: sub bitmap of the interface picture at the same rectangle |
| 0x44 | 1 if the area exists |
| 0x48 | restore function. High resolution: copy `Bgnd` (0x9C38F). Low resolution: fill with `cv_color` (0x9C1F2) |
| 0x50 | print function of the type |
| 0x58 | border function 0x9C890 (color, thickness): a frame inside the area |

Types and print functions (table at 0x144D48):

| Type | Print | Arguments | Draws |
|---|---|---|---|
| 0 | 0x9BD8A | icon number | restore, then the icon at (0,0), masked. 0 draws nothing |
| 1 | 0x9B95E | string | restore, then the string centered, y = (height - font height) / 2 |
| 2 | 0x9B761 | x, y, string | restore, then the string. y 0x8000 means centered as type 1 |
| 3 | 0x9B241 | hit points, maximum | restore, then "DEAD" if below 1, else "%d / %d", centered. Color by percentage: 25 or less red (63,0,0), 51 or more green (0,63,0), else yellow (63,63,0). If the word at 0x13AD5E is not 0, a bar of that color with width * percent / 100 instead of the text |
| 4 | 0x9AF8E | attribute | not read |
| 5 | 0x9BB7F | bitmap | the bitmap centered in the area (portrait) |
| 6 | 0x9B503 | value | "%d Exp" yellow, or a bar if the word at 0x13AD60 is not 0 |

Areas of the main screen (mode 0, created at 0x996D8): portrait 0x3BC410
(type 5), current item icon 0x3BC918 (type 0), hit points 0x3BC5DC (type 3),
name 0x3BC6F0 (type 1 high, type 2 low, text color (48,48,48)), 0x3BC638
(type 1, blue, never printed).

Areas of modes 3 to 6 (created at 0x99323 and 0x99449): name 0x3BC6F0 gray
(48,48,48), hit points 0x3BC5DC, 0x3BC18C yellow (63,63,0) printed "%s %d"
with the value of 0xB0A92, experience 0x3BC3B4 (type 6), 0x3BC130 orange
(63,48,32) printed "%s %d" with the player field at +0xDB. By the order in
the manual picture the third line is the armor class and the last line the
gold (guess for which label string is which). The item description bar
0x3BC524 has red text.

Hot key areas: 10 icon areas at 0x3BB04C and 10 quantity areas at 0x3BA91C
(text color (32,0,0)). Created in high resolution for the modes 0, 3, 4, 5,
and in low resolution only for mode 6 (0x995BE). Update code at 0x329CB: a
box with no item is cleared. A box with an item or a spell shows the icon (byte
2 of the entry) and the text of 0xB7487: the count, or "Lvl N" ("L-N" in low
resolution) for a spell of a spell list (character.md, section 6). The box of
the current item gets a border of thickness 1 in the color (63,63,0).

The font of all areas is `GAME_FONT` (font 4: `FNTEMLS_.FNT` low,
`FNTEMHS_.FNT` high).

## Player data used by the main screen

The player record has 0x1B37 bytes, the array starts at 0x1D331C, the index
is at 0x13B924. Fields read by 0x324DA (verified offsets, names are guesses):
+0x00 name, +0x53 hit points (fix), +0x9F maximum hit points (fix), +0x9B
portrait number (byte), +0xDB gold or armor class, +0x1AC1 the 10 hot keys
(2 bytes each, the second byte is negative if empty), +0x1AD7 the current
item (2 bytes).

## Message box (main screen only)

Verified from 0x33F55 and 0x34682:

- 40 line pointers at 0x1BD47C. The first byte of a line is its color, the
  text follows.
- Visible lines = (box height - 6) / 10: 4 in low, 7 in high resolution.
- Draw: fill the box (and the scroll bar width) with black, font `GAME_FONT`,
  line i (from the top, 0 based) at x = box x + 2, y = box y + 3 + 10 * i.
  The line step is 10 in both resolutions.
- The top line is line `first + visible - 1`, the bottom line is line
  `first`, so line 0 is the newest (guess: 0x34998 inserts at index 0).
- The scroll bar is drawn if there are more lines than visible lines.
- The black rectangle covers the scroll bar column too (its width is added at
  0x346E2). So the arrows of the interface picture are hidden until there
  are more lines than visible lines. The manual picture shows the arrows
  with an empty box. This difference is not explained (guess: an older
  build).
- The message box is drawn only if the byte at 0x13AD43 is not 0. It is set
  to 1 at 0x340C9 (display setup of a mode, not read in detail).

## Buttons (dispbutn.c)

A button record has 94 bytes (verified from 0x98A5D, 0x98E99, 0x98F38):

| Offset | Content |
|---|---|
| 0x00 | hot rectangle x, y, w, h |
| 0x10 | draw rectangle x, y, w, h |
| 0x20 | position of image 0 (shown at rest) in the sheet |
| 0x28 | position of image 1 (shown while pressed) in the sheet |
| 0x30 | pressed flag, 0x35 disabled flag, 0x36 current image number |
| 0x3A | hit test function (0x991C7) |
| 0x3E | action at press, 0x42 action at release |
| 0x46, 0x4A | press and release handlers |
| 0x4E | sheet number for 0xAEEC4 (69 high, 77 low), or -1: the interface picture |
| 0x52 | -1, 0 or 1 (not traced) |

Main screen, high resolution (list at 0x14433E), hot rectangle, image 0,
image 1, action at release:

| Button | Rectangle | Image 0 | Image 1 | Action |
|---|---|---|---|---|
| M | (16,305) 43x38 | (74,2) | (29,2) | mode 4 |
| I | (67,306) 40x37 | (161,2) | (119,2) | mode 3 |
| S | (16,348) 42x38 | (247,2) | (203,2) | mode 5 |
| R | (67,348) 41x38 | (334,2) | (291,2) | rest |

Main screen, low resolution (0x1442DA): M (8,154) 19x17 images (30,39) and
(9,39); I (33,154) 19x17 images (72,39), (51,39); S (8,176) 19x16 images
(114,39), (93,39); R (33,176) 19x16 images (156,39), (135,39).

The other lists (same record layout, read them from the image): high
inventory 0x1442EE (9 buttons: M, I, S, R, use, drop, SPELLS (0x14408C, the
action 0x37180 changes the list between items, mage spells and cleric spells),
and two regions without image for the figure of the paper doll, press action
0x35ADF; see character.md, section 6), high map 0x14432A, high
statistics 0x144316, low inventory 0x144262 (10), low map 0x1442C6, low
statistics 0x1442B2, low hot keys 0x14428E (8). In each list the button of
the current screen has the action "mode 0".

The main screen of version 1.3 has no USE button. The manual picture shows
one. The interface picture has no USE label at that place.

## Cursor and mouse mode

Verified: the cursor record is at 0x1BD51C. It keeps the image, a work copy
and the saved background. Not traced: the code that moves it.

Verified: the key V (immersive mode) is 0x54439, called from 0x529D5,
0x52DFA and 0x53DA9 when the key of the control at 0x13C70F is down. It
calls the key gate 0x545F5 first: the gate returns 1 only when 36.4 clock
ticks (0.364 s, `clock()` at 0x10A0F3) have passed since the last time it
returned 1 (time at 0x13BAE1, shared with 0x54632). So a held key toggles
again each 0.364 s, and a short press toggles once. The flag at 0x13BAE0 is
inverted. On: TEX 817 "Immersive mode is on." Off: TEX 818 "Immersive mode
is off.", then the mouse goes to the center of the view (0xE89A8). The text
goes to the message box with 0x347A2 and the player number (the color of the
player). The manual says: in non-immersive mode the mouse
moves the cursor, in immersive mode the mouse turns the character and the
buttons are not reachable.

Port addition (not in the executable): the menus of `newmenu.c` (the
Escape menu, F2, the message boxes, Improvements, Graphics, the death
menu, the list boxes) show a pointer: the cursor picture (entry 153 or 154)
with its colors matched to the current palette. While a menu is open the
mouse is not in the relative mode; the mode of before comes back when the
last menu closes. The item under the pointer is selected when the pointer
moves, the left button chooses it, the right button is Escape, and the
wheel moves the selection, changes a slider or a number, or scrolls a list.
The cursor of the play screen is taken out of the frame under a menu. The
setting `Dtu_fe_original_input` turns all of this off.

Port addition (not in the executable): with the Improvements setting "Target
hint" (`Dtu_target_hint`, `TargetHint` in `ascent.cfg`, on by default) the cursor
of the play screen has a warm red tint while the attack of the item in the hand
would reach a hostile creature that it can hurt. The test is the attack code
itself, see combat.md, C1. The tint is a remap of the cursor colors to the nearest
palette colors, so it shows in the 8-bit frame of both renderers and over the
upscaled interface pictures. The cursor that holds an item and the cursor over the
conversation window have no hint. The immersive mode has no cursor: with the hint,
the tinted cursor shows at the center of the view only while the attack would
reach a creature (dtudisp.c, after `draw_cursor`).

## Input defect

Not traced in the code. The port task states that the original repeats a
display action on every frame while a key is down. The port acts once per key
press and once per mouse click. `Dtu_ui_key_repeat = 1` gives the original
behavior.

## Screens that are documented only

- Inventory (mode 3): picture 65 or 59. List slots, paper doll regions and
  the 12 equipment boxes are in tables that are not decoded (pointer tables
  near 0x144540 and the layout variables 0x13B364 to 0x13B548). Input code is
  in 0x2FB10.
- Map (mode 4): picture 64 or 58. The automap in the circle (0x34032,
  "crystalBall != NULL", and 0x1887E) is in [automap.md](automap.md).
- Statistics (mode 5): picture 67 or 61. 0x992DB creates about 40 text areas
  at 0x99B11 to 0x9A52B (areas 0x3BB77C to 0x3BC244, gray, white and orange
  text). The positions come from tables that are not decoded.
- Low resolution hot key screen (mode 6): picture 60.
- There is no journal or spell book screen in the mode table. Spells are a
  view of the inventory list ("BrowseItemType <= JRG_VIEWING_PRIEST", the
  asserts at 0x35D2C; the variable 0x13ADB1, the button SPELLS). 0x35ABD is the
  test of a point in a rectangle.

## State of the port (engine/main/dtudisp.c)

Implemented from the verified facts: the interface picture of each mode, the
view rectangles and the doubled lines of the high resolution, the areas of
the main screen (portrait, name, hit points, current item icon), the message
box, the character information areas of the screens, the hot key boxes
(empty), the buttons M, I, S, R and their screen changes with both images,
the cursor, the modes 0, 3, 4, 5, 6, 7, 8.

Connected to the character of `dtuchar.c` (see `character.md`, section 13, for
the verified positions): name, hit points and maximum, the icon of the item in
the hand, the ten hot key boxes with icon, count and the border of the item in
the hand, armor class, experience and gold of the screens, the item
description bar, the inventory list (ten entries, two columns), the twelve
places of the paper doll, the cursor that holds an item, the buttons USE, DROP
and R (rest), the statistics screen (portrait, alignment, race, classes and
levels, the six abilities, the five thief skills), the portrait number.

The map screen draws the automap (automap.md). The SPELLS button and the spell
lists of the inventory screen, and spells in the hot key boxes, are in the port
(character.md, sections 6 and 14). Frame only: the spell tables of the
statistics screen, the scroll bar of the inventory list. `HRISBTN.PCH` gives the
second images of the button SPELLS.

Guesses in the port: the keys (I, Tab, V, R, F3 from the manual, the key
code of the original was not read), F3 changes between mode 0 and mode 7,
the start in non-immersive mode, the color of the message text, the place of
the slider of the scroll bar, the mouse position at the view center after a
mode change, the colors of the texts of the statistics screen, the keys of the
inventory screen (comma, period, Enter, U, Backspace: an addition, the original
has the mouse), the keys 1 to 0 for the hot key boxes (from the help text, the
key code was not read).

Settings (global variables in dtudisp.c): `Dtu_hires` (option `-lowres`
clears it), `Dtu_immersive`, `Dtu_ui_key_repeat` (1 restores the repeat of
the original), `Dtu_player_portrait`. Options for tests: `-fullview` starts
in mode 7, `-uimode N` starts in mode N.

The port draws all displays in each frame. The original draws an area only
when its dirty flag is set (flags at 0x1BD5A4). The picture on the screen is
the same.
