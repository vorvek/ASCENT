# The front end of DTUGAME.EXE 1.3

The front end is everything outside the 3D game: the title picture, the main
menu, character generation, the Waterdeep screens (Khelben, the market), the
save and load menus, the options, the credits and the death screen. Source
names in the executable: frontend.c, feanim.c, fehelp.c, feinput.c, felz.c,
femarket.c, fenuchar.c, ferbtn.c, fetext.c, fetorch.c, fewdeep.c, localize.c,
dlgmenu.c, and credits.c and autosave.c of the Descent 1 base.

Addresses are linear addresses of `gamedata/re/dtu_flat.bin` (object 1 at
0x10000). TEX[n] is line n (0-based) of DTUE.TEX; the `Text_string` pointer
array starts at 0x395DAC, so a pointer at address A is TEX[(A - 0x395DAC) / 4].

Marks: **V** = read in the code or checked against the data files. **G** = a
guess, with the reason. **P** = a decision of the port (not in the
executable).

The port is in `engine/main/dtufe.c` (frame, input, art, menus, main menu,
save and load, options, credits, death), `dtufe_char.c` (character
generation), `dtufe_town.c` (Waterdeep, Blackstaff, market), `dtufe_int.h`
and `dtufe.h`.

## 1. Screen flow

The main loop of main() (0xB8711 on) dispatches on `Function_mode` (dword
0x1468CC) (V):

| Mode | Meaning | Code |
|---|---|---|
| 0 | leave the program | |
| 1 | main menu | 0x58075 |
| 2 | the game | 0x4A569, game() 0x41F4A |
| 4 | Waterdeep | 0x8C206 |
| 5 | Blackstaff tower (Khelben) | 0x82B31 |
| 6 | back from the game: load the Waterdeep picture, fade in, mode 1 | 0xB89C5 |
| 7 | market | 0x8474B |
| 8 | the Yawning Portal: start level 1 | 0xB8A09 |

```
start: movies (LOGO22S, INTRO2, TLOGOS), title picture FEBGND 193
  -> main menu (1)
       Load Game...  -> slot menu -> Waterdeep (4) or the saved level (2)
       Save Game     -> slot menu (only with a character)
       New game      -> character generation (4 screens) -> Waterdeep (4)
       Return to Game -> Waterdeep (4) (only with a character)
       Options...    -> options menu
       Credits       -> scrolling credits
       Quit          -> "Are you sure ...?" -> leave (0)
Waterdeep (4): Blackstaff (5) | Yawning Portal (8) | Market (7) | City Gates -> main menu (1)
               Alt-F2 save, Alt-F3 load. Before the first visit to Khelben only
               Blackstaff and the gates are active.
Yawning Portal (8): StartNewGame(1) = level YPINN -> game (2)
game (2): exit warp of the inn -> "Leave the Yawning Portal?" -> Waterdeep (4)
          Esc -> ESC menu (Return to dungeon, Load, Save, Abort Game) -> main menu
          death -> death picture -> Death Menu (Load Game, Abort Game) -> main menu
```

There is no 2D Yawning Portal screen and no Hall of Fame screen in 1.3 (V: no
name strings for `btnyp*` or `btnhf*`; FEBGND 26, the parchment of screen
number 2, is loaded by no code that was found).

## 2. The common parts

### Screen numbers and pictures (V)

`0x7EF92(n)` enters the front end video mode (0xEB2F8 with mode 0xE, 640x480)
and loads front end screen n. Table 0x13E3D7 (u32) gives the FEBGND entry of
screen n: 0:10 (Waterdeep), 1:0 (Blackstaff), 2:26, 3:14 (market), 4:1, 5:12,
6:2, 7:3, 8:4, 9:5 (character generation 1 to 4), 10:18, 11:19, 12:16,
13:11, 14:1, 15:16, 16:15, 17:1, 18:13, 19:6, 20:7, 21:8, 22:9, 23:16. After
the load 0xAECAF (localize.c) runs for the screen (language overlays; not
read).

### Screen records and the runner (V)

The runner 0x7FD23(screen) calls the pre-load callback (+0x2F), loads the
screen (0x7F5E6), calls the post-load callback (+0x33), then loops while the
byte +0x1A is not 0: 0xA4F83 (stream service), per-frame callback (+0x37),
0x81F7E (input and hot spots). When a handler clears +0x1A the "may leave"
callback (+0x23) can keep the screen; then 0x7FA5F unloads and +0x3B runs.

Screen record (0x3F bytes, packed): +0x00 control list, +0x04 cursor list,
+0x08 feature list, +0x0C help bar x1, y1, x2, y2 (s16), +0x14 / +0x15 loaded
flags, +0x16 screen number, +0x1A running, +0x1B draw, +0x1F leave, +0x23 may
leave, +0x27 / +0x2B press / release with no feature, +0x2F, +0x33, +0x37,
+0x3B as above.

Control record (0x20 bytes): x, y, bitmap, name of image 1; x2, y2, bitmap,
name of image 2. Names are bitmaps of PORTAL.EGG (0xAEDC3 replaces the last
`_` by the language letter).

Cursor record (36 bytes): six runtime dwords, then three FESEQ entries (idle
and two more; -1 = none). G: the second and third are the press and hold
animations.

Feature record (57 bytes): rectangle x1, y1, x2, y2; circle cx, cy, r (the
hit test is the distance when r is not 0, 0x7FCB4); u8 has a control; u32
control index; u32 cursor index; hot key 1 and 2 (scan codes, 0x200 = Alt);
pointer to the help text; press handler; release handler. A hot key acts as a
press and a release (0x81E64). Every screen ends with a hidden feature with
key "`" (0x80D93, not read). Events (0x825FD): the cursor enters a feature
= its help text in the help bar; press = the "down" image and the press
handler; release = restore and the release handler.

### Text, fonts, colors (V)

Fonts (0x45ADF): 0x13E388 `FNTEMFL_.FNT` (large), 0x13E380 `FNTEMFM_.FNT`
(medium: help bar, numbers, menus, credits), 0x13E384 `FNTEMFS_.FNT` (small:
race text, statistic information, copyright line).

`FeColour(hue)` 0x80457: hue 0 to 10 gives a palette index from a table for
each screen number (pointer table 0x13E505). For screens 0 to 9, hues 0..7
and 9 are 0, 255, 247, 248, 249, 254, 253, 250, 251; hue 8 and hue 10 depend
on the screen. The help bar (0x82A04, sub canvas of the record rectangle,
medium font) shows the help of the feature under the cursor in hue 7 and a
message of 0x829B1 in hue 2 for 3 seconds.

### Animations (V)

- FESEQ: brushes of the cursors (entries 1 to 16 are 31x34 cursors) and
  other small animations (portraits 21 and 22, Khelben's eyes 31).
- FESTREAM (feanim.c): 0x8C883 open(seq), 0x8C93E frame(&bitmap, advance,
  last), 0x8C925 close. 0x8B18D play(seq): blit each frame at its position
  (not masked), wait 0x199A ticks (0.1 s); a click or a key jumps to the
  last frame. The caller keeps the last frame and draws it again.

### Sounds (V for the calls, G for the source)

Page turn `0x2D955(0xC1C, 0x10000)`; click of a control (0x7FEE2); refusal
0x80EF7 = `0x2DB28(0xC22, 0x10000)`. 0xC1C and 0xC22 are sound numbers that
0x2D7F5 maps (table not read; G: sounds of PORTAL.EGG).

### Menus (V)

The menus of the front end are the Descent 1 newmenu (0x63F41, 0x63F74,
0x63FB2; message box 0x655FE) drawn with the front end fonts: the title with
the large font, the subtitle and the items with the medium font, the current
item in hue 7, the others in hue 5 (0x63C7A), the box cut from the stone
picture of the screen (0x632E8). Item types are the Descent ones: 0 menu, 2
check box, 4 text, 6 input menu, 7 slider.

## 3. Start-up (V for the order, G where marked)

main() (0xB7E60) plays the movies LOGO22S.MVE, INTRO2.MVE and TLOGOS.MVE
(movie player 0x96439; the names are used at 0xB8514, 0xB8537 and 0xB8563,
in this order; the conditions around the calls were not read), then 0xB85B8
shows the
title picture FEBGND 193 with the version text at the bottom right while
the tables load, then loads the front end tables of the Yawning Portal
(PORTAL.EGG, named by YPINN.BIN) and enters the main menu.

## 4. Main menu (0x58075) (V)

Items (built by 0x57F28; action numbers for `do_option` 0x582CC, jump table
0x58260):

| Item | Text | Action | Condition |
|---|---|---|---|
| Load Game... | TEX[323] | 5: 0xBB780(0) | always |
| Save Game | TEX[436] | 6: 0xBA99F(1) | full Waterdeep list (0x8C4B6: 0x142035 == 0x141D48) or a character exists (0x141BBB) |
| New game | TEX[0] | 0: character generation | always |
| Return to Game | TEX[782] | 26: Function_mode = 4 | as Save Game |
| Options... | TEX[325] | 13: 0x58C18 | always |
| Credits | TEX[328] | 23: 0x2C61B | always |
| Quit | TEX[2] | 4 | always |

- Title "" and subtitle TEX[963] "Descent to Undermountain\nMain Menu" at the
  middle of the Waterdeep picture (screen 0), no box. The executable passes
  the picture name "scroll2.pcx", which is not in its picture table
  (0x631D2). The copyright line TEX[964] is drawn with the small font, hue 2,
  centered, 15 pixels above the bottom (0x58158).
- The first current item is the last choice (0x13CCE9, 0 at the start, -1
  after a cancelled quit).
- Esc or Quit: message box TEX[195] "Are you sure you want\nto leave the
  game?" Yes / No; Yes sets Function_mode 0.
- The loop runs while Function_mode is 1.
- New game (0x582EB): bytes 0x13E393 = 1, 0x13E394 = 0x13E395 = 0, clears
  0x13E39A (tower closed), Khelben visit counter 0x13E396 = 0, 0x8C426 (first
  visit list of Waterdeep), then `NuChar()` 0x87D73; on 1 Function_mode = 4.

## 5. Character generation (fenuchar.c, 0x87D73) (V unless marked)

Entry: 0x8C5D0 loads the FESTREAM descriptors. `Continue` (0x3A64AD) = 1,
screen index (0x3A63EC) = 0, the new character record 0x3A63F8 (0xB5 bytes)
is cleared, and the state: race 0x3A645D, sex 0x3A6461, race chosen
0x13FE20, class mask 0x3A6465 (1 fighter, 2 thief, 4 mage, 8 cleric),
statistics valid 0x13F6C9, alignment 0x3A6469, portrait 0x13FE0C, name flag
0x140141. Loop: run screen table 0x141BA7 (four records, NULL) with 0x7FD23,
`index += delta` (0x3A63D4). The shields: Esc 0x8B27E (if the race was
chosen, message box TEX[630] "Abort New Character" Yes / No), next 0x8B2A9,
back 0x8B2C9. No music call in fenuchar.c: the music of the menu goes on (G).

### Screen 1: race and sex (record 0x1408A1, screen 6, FEBGND 2)

Help bar (117,425)-(539,470), default TEX[706]. Features: "?" shield circle
(38,40) r 38 key R = quick character (0x8B402); Esc shield (603,51) r 38;
lower left shield (38,451) key PgDn = abort; lower right (597,438) key PgUp =
next; left page corner (84,307)-(133,354) key Left = page back; left figure
(72,122)-(222,374) key 1 = male; right page corner (348,315)-(395,362) key
Right = page forward; right figure (241,123)-(403,373) key 2 = female; race
text (461,108)-(628,378).

The book (FESTREAM 0) opens after 0.5 s. Page turns: table 0x13FE26 (human
-1/1, half-elf 10/2, elf 9/3, drow 8/4, dwarf 7/5, halfling 6/-1), 348x265 at
(66,116). A figure rises (13 frames) or recedes (12 frames): table 0x13FE56.
A page turn recedes the figure, clears the classes and plays the page sound.
The race text is FETXT entry `race` in the small font, black, in the sub
canvas (455,131) 175x277 at (5,5). Races: 0 human, 1 half-elf, 2 elf, 3 drow,
4 dwarf, 5 halfling. Leaving forward needs a race and a sex (TEX[712]).

### Screen 2: classes (record 0x140C26, screen 7, FEBGND 3)

Help bar (112,409)-(516,455), default TEX[707]. Scrolls: fighter
(88,34)-(290,193) key F, cleric (331,37)-(529,193) key C, mage
(86,227)-(288,384) key M, thief (330,227)-(531,385) key T. Select / deselect
sequences: fighter 36/37, thief 42/43, mage 40/41, cleric 38/39.

Allowed masks (`ComboBit[mask] & RaceCombos[race]`, 0x88B25, tables 0x13FEE6
and 0x13FECE):

| Race | Value | Classes |
|---|---|---|
| human | 139 | F, T, M, C (one class only) |
| half-elf | 2495 | F, T, F/T, M, F/M, M/T, C, F/C, M/C |
| elf, drow | 191 | F, T, F/T, M, F/M, M/T, C |
| dwarf | 391 | F, T, F/T, C, F/C |
| halfling | 135 | F, T, F/T, C |

No three-class combination and no cleric/thief. A refused toggle shows
TEX[717] or TEX[716] and plays the refusal sound. Leaving forward needs a
class (TEX[713]).

### Quick character (0x8B402)

Random race (`rand() % 6`) and sex if none was chosen; random allowed class
mask `((rand() % 14) + 1) & 15` if none; the statistics are not rolled: each
class has a fixed set (table 0x13FFFA, STR DEX CON INT WIS CHA: fighter 16 12
16 10 10 12, thief 15 17 13 11 10 10, mage 10 15 10 16 13 12, cleric 16 10 12
11 16 11), a multi-class character gets the integer mean; percentile
`rand() % 100`. The record is valid, the screen ends with index 1 and delta
1, so the next screen is the statistics screen (index 2) with the numbers in
the rows.

### Screen 3: statistics (record 0x1415C5, screen 8, FEBGND 4)

Help bar (105,421)-(528,466), default TEX[708]. Generate button (29,51)-
(111,172) key Enter (0x89A77): each of six source slots gets `Dice(3, 6)` with
ones rolled again (0xB0CDB, 0xB0C65): every number is 6 to 18; the rows are
cleared; percentile 0x13F868 = 1 for a halfling, else `rand() % 100`; **7
bonus points** (0x89B95; the manual says 5); FESTREAM 44 (the planks).

Source slots (168, 72+39i) 96x38; rows (304, 121+31i) 60x31; bonus box
(452,352) 80x48. Click rules (0x8A578): pick up and drop numbers between
slots and rows; the right button picks up only the bonus points of a row; a
bonus point is placed only on a row that has a number; the room of a row is
the points until value + racial modifier reaches 18 + max(modifier, 0); the
STR row of a fighter who is not a halfling takes points past 18 as +10
percentile each (18/00 at the end). Messages TEX[722] to TEX[725].

Racial modifiers (0x13FD8F): elf and drow DEX +1 CON -1; dwarf CON +1 CHA -1;
halfling STR -1 DEX +1. Limits (0x8B71C), final value with modifier:

| Race | Minimum STR DEX CON INT WIS CHA | Maximum |
|---|---|---|
| human | 3 3 3 3 3 3 | 18 18 18 18 18 18 |
| half-elf | 3 6 6 4 3 8 | 18 18 18 18 18 18 |
| elf, drow | 3 6 7 8 3 8 | 18 19 18 18 18 18 |
| dwarf | 8 3 11 3 3 3 | 18 17 19 18 18 17 |
| halfling | 7 7 10 6 3 3 | 18 19 18 18 17 18 |

Class minimums: fighter STR 9, thief DEX 9, mage INT 9, cleric WIS 9.

The information column (0x88E3B, small font) shows the adjustments of the
tables 0x1401EE (STR to hit), 0x140382 (STR damage), 0x14019A (DEX armor
class), 0x1402E2 / 0x140332 (CON hit points, not fighter / fighter),
0x14023E (INT spells), 0x1403D2.. (WIS bonus spells), 0x14014A (CHA
reaction), or TEX[620] / TEX[621] for a value outside the limits. The
pictures `p3<m|f><race>.bbm` (table 0x14000E). Leaving forward needs six
rows (TEX[718]) inside the limits (TEX[719]); leaving stores the record
(0x8AAA7). A cheat key sequence (table 0x141BD3, not decoded) sets all six to
18 and gives 999 hit points with STR 18, else 1.

### Screen 4: name, portrait, alignment (record 0x141B68, screen 9, FEBGND 5)

Help bar (109,414)-(529,461), default TEX[709]. Name box (46,112)-(192,158)
key N: Descent input box, title TEX[422], 15 characters of "a-z A-Z 0-9 - _
space" (0x13CD1B); Esc or empty keeps the old name; drawn centered with the
medium font, hue 7. Portrait (226,124)-(372,281): FESEQ 21 (male, 18 frames)
or 22 (female, 14 frames), 108x120 at (245,145); arrows (229,300)-(288,363)
key Left and (308,300)-(371,363) key Right; the new picture slides in by
width / 8 each 0x700 ticks. Alignment marks key 1..6: 0 Lawful Good, 1
Neutral Good, 2 Lawful Neutral, 3 True Neutral, 4 Chaotic Good, 5 Chaotic
Neutral (radio group of ferbtn.c, labels `lb4lg_d_` ...); a thief cannot be
Lawful Good (start value 4). Finish (552,413)-(625,474) needs a name
(TEX[774]). Pictures `p4<m|f><race>.bbm` (table 0x14009E).

### The character that New Game makes (0x87E77, MakePlayer 0x8BEBC)

After the last screen: 0x141BBB = 1, `MakePlayer(1, 1)`, 0x82BC4 (Khelben
mission, blurb and next mission = 0), delete the running level state
(`CSUF.ls` and `temp*`, 0x5AC6F) and write a fresh one (0x5ABC6), delete
`geldor.dat` (0x87BFB), load `ypinn.TBL`. Return 1: Function_mode 4. **No
level is entered here and no character file is written.**

The 80-byte attribute block (player +0x97, also 0x187B90; names in
`docs/re/character.md`): class 1 and class 2 codes (0 cleric, 1 mage, 2
fighter, 3 thief; taken in the order fighter, thief, mage, cleric), -1, race
code (human 0, half-elf 0 with flag bit 0 at +0x48, elf 2, drow 3, dwarf 1,
halfling 4), sex, spell class bits, 0x15, hit points, maximum, 2, 1, STR to
hit and damage, 10, levels 0, DEX armor class, the six statistics and the
percentile, thief flag, five thief skills (0x8BDE5), 0x66, flags, alignment,
-1.

- Hit points (0x8BA3A): full hit die (cleric 8, mage 4, fighter 10, thief 6)
  plus the CON adjustment (the fighter table only for a fighter); two
  classes: the mean, rounded toward zero. No roll.
- Experience: 0 (tables 0x146048).
- Thief skills: base 5, 10, 10, 5, 60 plus the race tables 0x1461C8..
  0x146228 and the DEX tables 0x146078..0x146174; a dwarf who is not a thief
  gets 50 at find traps.
- Items (0x8BB91): fighter (5,20) (15,1) (16,2) (16,79); thief (5,19) (15,1)
  (16,13) (16,79); mage (5,18) (16,79) and spells; cleric (5,27) (15,1)
  (16,79); an item is not added twice. Mage: the 13 spells 2 4 5 6 12 15 27
  30 33 39 45 46 48 shuffled, the first min(INT, 18) / 3, and 46. Cleric:
  spell 0.
- Portrait: player +0x1B03 = the frame number (no sex offset).
- Gold: no code of character generation writes it (G: the player reset
  0x33EBB, not read).

## 6. Waterdeep, Blackstaff, market (fewdeep.c, femarket.c) (V)

Waterdeep (0x8C206, record 0x14202D, screen 0, help bar (136,375)-(501,400)):
music stream 0 (overview.tun); hot spots Blackstaff (275,120)-(334,189) keys
B K, Yawning Portal (10,180)-(90,260) keys Y P, Market (215,190)-(304,264)
keys M 4, City Gates (492,271)-(635,345) keys X Esc, Alt-F2 save
(0xBA99F(1)), Alt-F3 load (0xBB780(0)). Before the first visit to Khelben
(mission and blurb 0) the list 0x141F10 has only Blackstaff (250,90)-(375,230)
and the gates.

Blackstaff (0x82B31, record 0x13EAF3, FEBGND 0): Khelben's face FEBGND 195 (7
frames of 82x117 at (290,124)), eyes FESEQ 31, torches, the scrolling text of
the mission (FETXT, table 0x142840) with markup `~c ~s ~p ~e ~n ~m`, music
stream 2 at volume 25. Exit circle (551,422) r 32 keys Esc X.

Market (0x8474B, record 0x13F29E, FEBGND 14): buy / sell, weapons / armor /
misc, a nine row catalog, the quantity pop-up FEBGND 25. Stock: `geldor.dat`
(10-byte records), saved as `TheStore.<n+1>`. The prices and the first stock
(0x8705D, 0x872C1) were not traced.

## 7. Entering a level (V)

Mode 8 (0xB8A09): 0x5895E, the Descent 1 `do_new_game_menu`: one mission, so
no list; the difficulty menu is a stub that sets 2 ("E3 hack"); fade out,
leave the front end, `StartNewGame(1)` 0x4DD07: Function_mode 2, the player
object from the current player record (the character is not reset),
StartNewLevel(1) = YPINN. The one gold fee of the inn was not found in this
path.

Back from the game: DTU.WRP command 0 ("YPInn to Waterdeep", 0x5B3AC) marks
the level in CSUF.ls, sets Function_mode 4 and byte 0x13B254; game() then
asks TEX[795] "Leave the\nYawning Portal?". After game(), modes 4, 6 and 1
set the front end video mode, show a loading picture (0x7C1E5: FEBGND 12, or
155 to 160) and load the Waterdeep picture.

## 8. Death (game() 0x423F5) (V)

Wait 3 s or a key; 0x82BC4 resets the Khelben state; 0x8C426 (first visit
list); the streams stop; fade out; FEBGND 161 (the dead adventurer) with its
palette; Function_mode 6; music stream 7 (deathtag.tun) once; wait for a key;
fade; load PALETTE.256; clear the screen (G: 0xD9CEC finds black, 0xEAD68
clears); message box (no title, text "Death Menu\n", choices "Load Game",
"Abort Game", 0x42593). Load Game: 0xBB780(1) (asks TEX[801] "Restore
Game?"); Abort Game: TEX[103] "Abort Game?" Yes / No; Yes leaves to the main
menu, anything else shows the menu again.

## 9. Save and load (autosave.c) (V)

- Load slot menu 0xBA647: ten slots `SG%d.sg`; a slot is valid if it starts
  with `DGSS` and version 12; the item is its 20-byte description, else
  TEX[67] "-empty-" as a text item. A text item of four empty lines comes
  first. No valid slot: TEX[797] "No saved games were found" with TEX[34]
  "Ok". Subtitle TEX[798] "Select Game to Restore".
- Save slot menu 0xBA486: ten input menu items (type 6, 19 characters),
  subtitle TEX[796] "Save Game". An error gives TEX[800].
- Load 0xBB780: 0x141BBB = 1; the level files and the market are restored;
  the player record and the attribute block are read; "between levels" 1 =
  Function_mode 4 (Waterdeep), 0 = the level is started (0x4E33D) and the
  game goes on.
- The file layout is in `docs/formats/misc.md` (SG0.SG and the .LS level
  state table).

## 10. Options (0x58C18) (V)

While the front end is active (byte 0x13E338, 0x7F13D) the menu (title
TEX[208] "Options:") has: Music Volume (slider 0..8, 0x13BFEC), a blank text
item, FX Volume (slider 0..8, 0x13BFEB), a blank item, Reverse Stereo (check
box, 0x13BFEE), two blank items, Hires Dungeon Graphics (check box,
0x13A94B), and "Hear Khelben In English" (0x13E91E) when the language is not
English. Brightness, Detail levels, Controls..., the joystick and mouse
sensitivity and Calibrate Joystick are items of the same function only in the
game.

## 11. Credits (credits.c, 0x2C61B) (V)

`iphof.txt`, music stream 2 (khelben.tun) looped at volume 100. The lines
scroll up in a column at x 112, 416 wide, 470 high from y 5 (blit
0xE0820), over the picture of the screen that was loaded before (Waterdeep
from the menu). Defaults (0x2CEA7): medium font, row height = the height of
the medium font, hue 7, transparent background, centered. A line that starts
with `;` is a comment and `~` starts a command (0x2CBD1): `~Sl` / `~Ss` large
or small font, `~Jc` `~Jl` `~Jr` justification, `~cN` hue N. IPHOF.TXT has
no command and no comment. First delay 0x1000 ticks, then one pixel each 0x100
ticks; 10 s after the last line, or any key, ends it. FECREDIT.TUN is not
used.

## 12. Input

The executable calls the action of a pressed feature again in each frame
while the mouse button is down (0x81F7E), and its menus act on the state of
the button, not on a new press (V for the feature loop; G for the effect on
each control).

## 13. The port

| Part | State |
|---|---|
| Title picture | done; the movies are not played (P) |
| Main menu | done, items and conditions as section 4 |
| Character generation | done, all four screens and the quick character; the character goes to `Dtu_char` (dtuchar.h) and the game starts in YPINN through Waterdeep |
| Waterdeep, Blackstaff | done (Khelben text, face, speech, torches) |
| Market | the screen and its buttons only; no trade |
| Save, load | done for the parts of section 9 that the port has (P below) |
| Options | Music, FX, Reverse Stereo; Hires Dungeon Graphics is left out (P) |
| Credits | done |
| Exit of the inn | done: "Leave the Yawning Portal?" then Waterdeep |
| Death | done: picture, music, Death Menu with Load Game and Abort Game |
| ESC menu in the game | not done: Escape in a game that the front end started ends the game and goes to the main menu |
| Movies (MVE) | not done |

Decisions of the port (P):

- Input: `Dtu_fe_original_input` (option `-feoriginput`) gives the behavior
  of section 12. It is 0 by default: a press or a key acts one time, and
  only the controls where a person expects it (the portrait arrows, the
  sliders) repeat with a delay (0.4 s, then 8 or 10 times a second).
- The script of the option `-fescript FILE` drives the front end for tests
  (comment in dtufe.c). While it runs the clock is the frame count (30
  frames are one second).
- Save games: the port reads a slot in the current directory first, then in
  the directories of the game data, and writes it in the current directory.
  It writes only "between levels" files (the saves of the main menu and of
  Waterdeep) with the header, the Khelben state, the player record
  (`dtu_char_pack`), the attribute block and the RLPD block; the 19 mission
  flags are 0. It does not write or read the level files of `SAVE\` and the
  market stock. A save of a level starts that level from its start.
- The Descent message boxes of the game in the death menu are drawn as front
  end menus over the death picture; the text colors are the closest colors of
  the picture to the colors of the Waterdeep screen.
- The exit of the inn is asked one time each time the player enters the exit
  segment (the trigger code calls the hook in each frame; whether the
  executable asks again in each frame after "No" was not checked).
- The hooks `dtu_hook_warp_command` (dtutrig.h) and
  `dtu_hook_player_death_screen` (dtuobj.h) are defined in dtufe.c. game.c
  calls `dtufe_game_frame()` after each frame.

## Open questions

1. The market: the prices, the first stock (0x8705D, 0x872C1), the pop-up
   position and the icon sheet.
2. The mission switch 0x83EF5 and the rewards 0x844ED: 35 cases not written
   down; the mapping from the DTU.WRP commands 1 to 19 to the quest flags.
3. The player reset 0x33EBB and 0x3575B: starting gold, armor class, THAC0.
4. The sound numbers 0xC1C and 0xC22 and their source.
5. The hidden "`" feature (0x80D93) and the cheat sequence 0x141BD3.
6. The ESC menu of the game and the loading pictures after the game.
7. The meaning of the unknown fields of the save file (see misc.md).
