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
- Gold: 102. 0x8B85D (called by MakePlayer at 0x8BF1D) writes 0x66 to the
  attribute block +0x44 (0x8B8AC), and MakePlayer copies the block to P+0x97,
  so P+0xDB = 102 (V). (An earlier version of this document said that no code
  writes it.) The player reset 0x33EBB does not touch the gold (V: it sets
  the empty hand record 0x1BD46C). No other start value exists: the other
  writes of P+0xDB are the pick up (0x280B3), the passage fee (0x59FFA), the
  rewards of Khelben (0x8450D on), the market and an event that takes all
  gold (0xAA41E).

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

Market (0x8474B, record 0x13F29E, FEBGND 14): section 6a.

No temple, training or inn screen exists in Waterdeep: the feature lists
0x141D48 and 0x141F10 have only the places above (V). Rest is an action of
the game (dtuchar.c).

## 6a. The market (femarket.c) (V unless marked)

### Screen

Screen 3 (FEBGND 14), help bar (33,408)-(442,461). The init (0x84755) sets
the rectangle of the catalog feature to (35,37)-(364,364), loads the icon
sheet (0x8578C: FEBGND 22, icons of 32x32, icons per row = width / 32), reads
the charisma (0x85764), builds the lists (0x8705D), selects "weapons" and
"buy" in the two radio groups (ferbtn.c, 0x13EC48: `lbmwpnd_` (452,81),
`lbmarmd_` (452,174), `lbmmscd_` (453,265); 0x13ECA8: `lbmbuyd_` (427,15),
`lbmseld_` (532,15); the last `_` is the language letter) and draws the
catalog and the gold.

Features (list 0x13EE9C, 57 bytes each; all act on release):

| # | Place | Keys | Help | Action |
|---|---|---|---|---|
| 0 | circle (543,434) r 36, control 0 `btnmkxit` (507,398) | Esc X | TEX[570] | 0x84BA8: leave, Function_mode 4 |
| 1 | (427,15)-(520,64) | B | TEX[572] | 0x84A6E: buy |
| 2 | (532,15)-(625,64) | S | TEX[573] | 0x84AEB: sell |
| 3 | (452,81)-(619,143) | W | TEX[574] | 0x8494B: weapons |
| 4 | (452,174)-(619,237) | A | TEX[575] | 0x849AA: armor |
| 5 | (453,265)-(618,326) | M | TEX[576] | 0x84A0C: misc |
| 6 | (35,37)-(364,364) | | TEX[577] | 0x84BE4: the catalog |
| 7 | (366,43)-(396,73), control 1 `btnmkup` | Up | "Scroll up" | 0x84B6D |
| 8 | (365,96)-(395,126), control 2 `btnmkdwn` | Down | "Scroll down" | 0x84B8A |

The catalog (0x85AF0): 9 rows at (35, 37 + 37 i), 330x32. A row (0x85E06):
the icon (32x32 at the left, the icon number is `icon=` of the item; color
255 is not drawn, 0xE1280), at x 37 the quantity (medium font, hue 2; "many"
TEX[701] for 100 or more or no limit) and the name (hue 3) at x 37 + the
width of "many" + 5, and at x 282 the price (large font, hue 7, centered).
The gold (0x85A3F): "%d" of P+0xDB, large font, hue 2, centered in
(452,352) 166x25.

A click in the catalog (0x84BE4): x - 35 must be below 330, y - 37 below
0x148; the row is found by `while (row < 9 && y >= 32) { y -= 37; row++; }`;
the row must hold a line.

The lists are circular and doubly linked. The first line shown is
0x13ED1C, the count of the list shown 0x13ED18. The scroll buttons act only
when the list has more than 9 lines; they move the first line one back or
one on, around the circle.

### Lists (0x8705D, 0x870A0)

Six lists: the stock for weapons, armor, misc (heads 0x13EB54, 0x13EB50,
0x13EB58, counts 0x13EB60, 0x13EB5C, 0x13EB64) and the goods of the player
(0x13EB38, 0x13EB3C, 0x13EB40, counts 0x13EB44 to 0x13EB4C). A line is a
0x37-byte node: +8 class, +9 number, +0x0A stackable, +0x0B count of one
item (word), +0x0D quantity (-1 no limit), +0x11 value, +0x15 sell price,
+0x19 buy price, +0x21 name, +0x25 (written, never read), +0x26 equipped,
+0x27 the 16-byte item record. A new line is put into its list sorted by
class, number, count (higher first) and equipped; a line equal in these four
gets the quantity added (-1 if one of them is -1) and is not added.

The stock (0x872C1): the 10-byte records of `geldor.dat` (s32 quantity, u8
class, u8 number, u32 count), read to the end of the file; with no file, the
21 records at 0x13EB6C (the same goods as the GELDOR.DAT of the install, in
another order). 0x873EE builds the item record from the item tables; for a
stackable type the count is 1, else the count of the record. Classes: 5
weapon, 15 armor, 16 misc, 17 spell (no spell is listed: the stock has none
and the spells of the player are in other lists).

The goods of the player: each entry of the item list (P+0x103, P+0xF3
entries) that is not a plot item (`plot_item=` of the table, record +0x0A).
A stackable entry gives one line with the stack as the quantity and count
1, another entry a line of quantity 1 with its count (charges).

### Price (0x86C5A) (V)

```
value = value= of the table line (weapon +0x34, armor, misc, spell)
max   = uses= (weapon +0x2E, no start value; misc and spell start at 1);
        armor: -1
fixed = the flag "stackable" (weapon 0x02, armor 0x10, misc 0x01, spell 0x02)
if value < 1: value = 1
if not fixed:
    max = max < 1 ? 0xFFFF : min(max, 0xFFFF)
    cond = the count of the line; cond < 0 or > 0xFFFF: 0xFFFF
    value = value * (min(cond, max) * 100 / max) / 100
price = value * percent[charisma] / 100, at least 1
if game flag 107 of the player (bit array P+0x1A33, 0xADAFE): price * 2
```

`percent` (0x86F75, charisma clamped to 0 .. 19): the player sells, table
0x13F2DD: 20 25 27 33 35 40 45 50 55 60 65 70 75 80 85 88 93 97 99 100; the
player buys, table 0x13F32D: 400 350 300 275 250 225 200 190 180 165 150 135
125 120 115 112 108 107 105 101. There is no other reaction adjustment.

The charisma (0x85764): if 0x13E38C is not 0, the word +0x5D of the record
it points to, else P+0xBF. Character generation sets 0x13E38C to its record
0x3A63F8 (0x87E82) and nothing clears it, so after a new character the market
uses the charisma of that record for the rest of the run, even for a loaded
character.

All 21 goods of the first stock are stackable, so their price is value times
the percent. Example with charisma 12: Potion of Healing (value 300) costs
405 and sells for 210.

### Buy (0x84D41) and sell (0x84CA6)

Buy: the quantity text is "many" or "%d"; room = 0xB7303 (stackable: the
limit `stack_limit=` minus the items of the type; else 127 minus the entries
of the type); no room: message TEX[700] (the bag is full). Can = gold /
price (or 1000 for no limit at price 0); 0: message TEX[702]. The most is
min(quantity as unsigned, can, room). The pop-up text is TEX[704] with the
quantity text, the price and the most.

Sell: the price is the sell price, the most is the quantity, the text
TEX[705] (quantity, price). If the (class, number, count) of the line is in
a hot key, in the hand, in a slot, or in the bow or crossbow place
(0x8622B builds the table 0x3A4FC8), the help bar shows TEX[703] for 3
seconds.

### The quantity pop-up (0x84EEA) (V)

FEBGND 25 (0x860D9), centered on the screen ((640 - w) / 2, (480 - h) / 2).
Sub canvases: title (35,32) 339 wide (medium font, hue 3, centered: the
name of the item), the text (35, 33 + font height) (hue 7, centered), the
quantity (175,144) 62x37 (hue 7, centered). The features of the screen
become the list 0x13F10F, the help bar (px+41, py+200)-(px+374, py+254), the
mouse is held in the pop-up and put at (320,280); a warning goes to the help
bar; the quantity starts at 1.

| # | Place (px, py = corner) | Control | Keys | Help | Action |
|---|---|---|---|---|---|
| 11 | (88,127)-(154,194) | `btnpulft` at (88,127) | Left Down | TEX[578] | 0x85740: quantity - 1, at least 1 |
| 12 | (260,127)-(327,197) | `btnpurit` at (260,127) | Right Up | TEX[579] | 0x85719: quantity + 1, at most the most |
| 13 | (54,258)-(199,314) | `lbpyesd_` at (54,258) | Right Down | TEX[580] | 0x85458: accept |
| 14 | (217,259)-(360,315) | `lbprejd_` at (217,259) | Right Down | TEX[581] | 0x8566D: reject |
| 15 | (175,144)-(237,181) | | Right Down | TEX[582] | 0x86F9C: menu "Enter Qty", 5 characters |

The keys of 13 to 15 are the keys of 11 and 12, which come first: the
keyboard can only change the quantity.

Accept (0x85458), on a copy of the line:
- Buy: the quantity of the line goes down by the number (0x86B80, a line at
  0 goes away); for each item: if the player has fewer than 127 entries and
  0xB6315 puts the item record into the item list, the line goes to the
  player list (0x876ED) and the price is taken from the gold. At the end a
  negative gold becomes 0.
- Sell: the line of the player goes down; for each item 0xB6E01(class,
  number, count) takes one item and the price goes to the gold; 0xAFB55
  (the character values); then 0x873EE adds to the stock the quantity of
  the copy, that is the items that the player had in the line, not the
  number sold.

Reject (0x8566D) closes the pop-up. The window gives the screen its
features and help bar back and draws the catalog and the gold again.

### Leaving and the stock file

At the exit (0x847DE) the stock is written to `geldor.dat` (0x86846: armor,
weapons, misc, each line as s32 quantity, u8 class, u8 number, u32 count),
and the lists are freed.

- New character: `geldor.dat` is deleted (0x87BFB at 0x87EC3).
- Save (0x87C05(S, 1)): if `geldor.dat` exists it is copied to `TheStore.<S>`
  (S = slot + 1, at most 999) with each record coded by 0x87BB7 (bytes 0..3
  and 6..9 change places, and bytes 4 and 5). Without `geldor.dat` nothing is
  written.
- Load: `geldor.dat` is deleted, then `TheStore.<S>` is decoded into it
  (0x87C05(S, 0)).
- Rewards of Khelben (0x879A7(n), n 0 to 8, table 0x13F6A0): the records of
  the reward are added at the end of `geldor.dat`; nothing if the file does
  not exist.

`THESTORE.1` of the install decodes to `GELDOR.DAT` of the install (V).

## 6b. The ESC menu of the game (game() 0x4225A) (V)

The Escape key of the game sets Function_mode 1 (Descent 1). After the game
loop, if Function_mode is not 2 (and no demo, no editor), game() keeps the
mode, sets 2 and:

- If byte 0x13B254 is set (command 0 of DTU.WRP ran in this game: the exit
  of the inn): message box (no title) "Yes" "No", text TEX[795] "Leave the
  Yawning Portal?". The byte is not cleared by "No".
- Else `nm_messagebox(NULL, 4, "Return to dungeon", "Load Game", "Save
  Game", "Abort Game", "ESC Menu\n")` (strings 0x125723, 0x125719,
  0x12570F, 0x125704, 0x1256FA):
  - Return to dungeon, or Esc: the game goes on.
  - Load Game: 0xBB780(1) (the slot menu and TEX[801] "Restore Game?").
  - Save Game: 0xBA99F(0): the save in a level.
  - Abort Game: TEX[103] "Abort Game?" Yes / No.
- Then Function_mode = the kept mode, and 2 again if the choice was not 0
  (for the exit question "Yes" is 0; for the menu only "Abort Game" + "Yes"
  gives 0). So a load from this menu always goes on in Function_mode 2,
  also for a save of Waterdeep (0xBB780 set mode 4).

The end of the game after "Abort Game" (0x42639): byte 0x13B254 is 0, so
0x141BBB = 0 (no character) and 0x8C426 (the first visit list of
Waterdeep); main menu.

## 7a. Loading pictures (V)

`0x7C1E5(n)`: the picture of the loader. It keeps the video mode, and shows
FEBGND n if 0x13E1E5 is 0, else FEBGND 12 for n < 0 and FEBGND
table[n % 6] for n >= 0 (table 0x13E1C9: 156, 157, 158, 159, 160, 155), then
sets 0x13E1E1 = 1. 0x13E1E5 starts as 1 and only 0x7C288 clears it (the
death picture 161 at 0x4247D). `0x7C2BB` ends the picture: fade out
(0xD9EA4) and the old video mode. `0x7C2EE` clears the flags with no fade.

- Before a level (load_level 0x494EB): when the table of the level (0x1D2728)
  is not the loaded table (0x1B79A2) and the archive differs, or the loaded
  table is FRONTEND.TBL, or the archive is SHORT.EGG: video mode 640x480 and
  `0x7C1E5(n)` with n = -1 if 0x13E324 is 1 (set by Function_mode 8, the
  Yawning Portal of the front end) or the new table is FRONTEND.TBL, else
  `rand() & 0xFF`. Then the tables load (0x4A569) and 0x13E324 = 0.
- After the level: the warp (0x5A1B5) and the load of a save game (0xBBDEA)
  end the picture (0x7C303, 0x7C2BB), set the game mode, load PALETTE.256
  and clear the screen.
- Back to the front end (0xB88FF, modes 4, 6, 1): fade, mode 640x480; if the
  loaded archive is not "portal" (0x80F09), `0x7C1E5(-1)` (FEBGND 12) and
  0x7C2EE; then the Waterdeep screen.

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
| Market | done: lists, prices, buy, sell, quantity window, stock file, `TheStore.<S>` in the saves (section 6a) |
| Save, load | done, with the level state files (docs/re/levelstate.md) and the market stock; saves in a level |
| Options | Music, FX, Reverse Stereo; Hires Dungeon Graphics is left out (P). The port adds "Improvements..." (the settings of `dtuopts.h`); the ESC menu and the F2 options menu of the game also have it |
| Credits | done |
| Exit of the inn | done: "Leave the Yawning Portal?" then Waterdeep |
| Death | done: picture, music, Death Menu with Load Game and Abort Game |
| ESC menu in the game | done (section 6b) |
| Loading pictures | done (section 7a); the picture is scaled to the screen of the game (P) |
| Movies (MVE) | not done |

Decisions of the port (P):

- Input: `Dtu_fe_original_input` (option `-feoriginput`) gives the behavior
  of section 12. It is 0 by default: a press or a key acts one time, and
  only the controls where a person expects it (the portrait arrows, the
  sliders) repeat with a delay (0.4 s, then 8 or 10 times a second).
- The script of the option `-fescript FILE` drives the front end for tests
  (comment in dtufe.c). While it runs the clock is the frame count (30
  frames are one second).
- Save games: the port reads a slot in the directory of the user first (any
  case of the letters), then in the directories of the game data, and writes it
  in the directory of the user. The file has the format of the executable
  (docs/formats/misc.md): the bytes of the loaded file stay, the port writes the
  header, the Khelben state, the mission flags (dtuscript.c), the player record
  (`dtu_char_pack`), the attribute block, the RLPD block and, for a save in a
  level, the three lists at 7374. A load sets `GameTime` from offset 49 and
  applies the three lists at the level start (three counts of 0, as in the saves
  of earlier builds of the port, keep the lists of the level). The level state
  goes to `SAVE\` (docs/re/levelstate.md), the stock of the market to
  `THESTORE.<S>`.
- The loads of the ESC menu and of the death menu end the game; the front
  end then starts the level of the save (the executable starts it inside
  game()).
- The market: `GELDOR.DAT` and `THESTORE.<S>` are in the directory of the user.
  The inventory of the port has the limits of the executable (127 items and 22
  key items, `DTU_MAX_INVENTORY`), and a buy stops there. Game flag 107 (the
  double price) is the weak hook `dtu_hook_game_flag` (0 until the flags
  exist). The mouse is not held in the quantity window.
- The loading picture: the executable sets 640x480 and shows the picture;
  the port draws it scaled into the screen of the game, keeps it while the
  level loads and fades it out (0x7C2BB) after the level state is restored.
  The first level after the front end always shows a picture, because the
  front end of the executable has its own table (FRONTEND.TBL) and the port
  keeps the tables of YPINN.
- The Escape key of the game: game.c calls `dtufe_escape_key()`; the menu is
  shown after the frame, where game() of the executable shows it.
- Abort Game of the ESC menu: the port clears the character (0x141BBB = 0);
  the first visit list of Waterdeep (0x8C426) is not used, the lists follow
  the Khelben state.
- Test options: `-statetest DOORSEG,DOORSIDE,OBJ[,WARPSEG,BACKSEG]`
  (dtustate.c) opens a door, kills a creature and moves the player into
  warp segments, with a log line "STATETEST" at each level start;
  `-loadshots` writes each loading picture to `loading<n>.ppm`; the script
  command `gamekeys` lets the script of `-fescript` run in the frames of the
  game (its keys go to the game). With the direct start (`-level NAME`) the
  script runs in the game from its first line, as after `gamekeys`. A run with `-frames` and no script ends
  the program when the game ends (the death screen would wait for a key).
- The Descent message boxes of the game in the death menu are drawn as front
  end menus over the death picture; the text colors are the closest colors of
  the picture to the colors of the Waterdeep screen.
- The exit of the inn is asked one time each time the player enters the exit
  segment (the trigger code calls the hook in each frame; whether the
  executable asks again in each frame after "No" was not checked).
- The hooks `dtu_hook_warp_command` (dtutrig.h) and
  `dtu_hook_player_death_screen` (dtuobj.h) are defined in dtufe.c. game.c
  calls `dtufe_game_frame()` after each frame.

Settings (on by default, each corrects a defect of the executable):

| Setting | File | The executable | The port with the setting on |
|---|---|---|---|
| `Dtu_fix_market_charisma` | dtufe_town.c | after a new character the market uses the charisma of that character for the rest of the run, also for a loaded character (0x13E38C) | the charisma of the player |
| `Dtu_fix_market_sell_count` | dtufe_town.c | a sale adds to the stock the items the player had in the line, not the items sold (0x8564F) | the items sold |
| `Dtu_fix_escape_load_waterdeep` | dtufe.c | a save of Waterdeep loaded from the ESC menu gives the character but the game goes on in the level (0x423E1) | the player is in Waterdeep |

## Open questions

1. The market: the meaning of node +0x25 (0x3533C, never read) and the order
   in which 0xB6E01 takes an item when the player has two equal entries
   (the port takes the first).
2. The mission switch 0x83EF5 and the rewards 0x844ED: 35 cases not written
   down; the mapping from the DTU.WRP commands 1 to 19 to the quest flags.
3. The player reset 0x33EBB and 0x3575B: armor class, THAC0 (the gold is
   102, section 5).
4. The sound numbers 0xC1C and 0xC22 and their source.
5. The hidden "`" feature (0x80D93) and the cheat sequence 0x141BD3.
6. Game flag 107 (P+0x1A33 bit array), which doubles the market prices: who
   sets it.
7. The meaning of the unknown fields of the save file (see misc.md).
