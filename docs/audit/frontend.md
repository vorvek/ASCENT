# Audit of the front end and the town

Scope: title and movies, main menu, character generation, Waterdeep,
Khelben (Blackstaff), the market, the save and load menus, the death menu,
the options of the front end, the credits, quitting. Port files:
`engine/main/dtufe.c`, `dtufe_char.c`, `dtufe_town.c`, `dtufe_int.h`.
Reference: DTUGAME.EXE 1.3 (`gamedata/re/dtu_flat.bin`, addresses are linear
addresses with object 1 at 0x10000) and the notes in `docs/re/frontend.md`.

Test runs: `ascent.exe -datadir E:/UNDAMONTAN/DTU -datadir
D:/dev/ASCENT/gamedata/cd/DTU [-hddir D:/dev/ASCENT/gamedata/hd] -nomovies
-mute -userdir <scratch> -fescript <script>`, with copies of the saves of the
user in a scratch directory. A run without `-hddir` gives 640x480 frames that
can be compared with the coordinates of the executable.

## Findings

Most important first.

| Id | Finding | Evidence | Status |
|---|---|---|---|
| FE-1 | The credits scrolled over the Waterdeep picture. The executable loads FEBGND 160 (a street at dusk) with a fade out and a fade in before the text, and FEBGND 10 again at the end. | 0x2C67D: `0x2C5E6(0xA0)`; 0x2C6BB: `0x2C5E6(0xA)`; 0x2C5E6 calls 0x80202 (FEBGND n into the picture of the screen) and 0x7F4CF (fade out, blit, fade in). Test: main menu, Credits, shot after 2 s shows FEBGND 160; the main menu after Esc is Waterdeep again. | Fixed, 63c8702 |
| FE-2 | The six torches of the Blackstaff tower did not burn. | fetorch.c: table 0x13E594 (FESEQ 23 to 28 at fixed places), 0x96198 load, 0x96263 one frame each 0x1000, called first by the frame callback 0x830D4. Test: new quick character, Blackstaff, two shots 2 frames apart differ only in the torch rectangles; the flames sit on the torch holders of FEBGND 0. | Fixed, 8b10baa |
| FE-3 | The market never doubled its prices: the price code asked a weak hook that always returned 0, but the global flags exist now (dtuscript.c). | 0x86F5A: `0xADAFE(0x6B)`, price * 2. Test: SG1.SG (copy, between levels 1) with and without global flag 107: every price of the weapons list doubles (Long Sword 18 / 36, Short Sword 12 / 24). | Fixed, d84922c |
| FE-4 | The load slot menus had no spacer and no fixed width: the executable puts a text item of four empty lines first, gives newmenu the item width 190 and makes the first slot the current item. The port started the slots at the top of the box and with the width of the longest name. Both the menu of the front end and the menu of the game (ESC menu, death menu) are changed. | 0xBA665 (`"\n\n\n\n"`, type 4), 0xBA7FF (`newmenu_do3(NULL, TEX[798], 11, m, NULL, 1, NULL, 190, -1)`), the result minus 1 is the slot (0xBA826). Test: main menu Load Game, and `-level YPINN` Esc, Load Game: the slots start four lines lower, the first valid slot is selected, Down then Enter loads slot 1 (SG1.SG). | Fixed, a42197f |
| FE-5 | The Waterdeep music went on in the main menu after the City Gates. The executable stops all streams after the Waterdeep screen, whatever comes next, and the main menu starts no music. | 0x8C25A: `call 0xA48D0` after the screen runner, no condition; the main loop (0xB883C to 0xB88B9) and 0x58075 have no music call. | Fixed, 42bce23 |
| FE-6 | The hidden animation of Khelben's face is not ported. Armed when the cursor leaves (286,173)-(350,237); when the hot spot is then exactly at (318,205), FESEQ 29 plays at (290,190) (0x1100 per frame, last frame 3 s), then FESEQ 30 (0x2100 per frame). A trial port showed the frames with the color 247 (pure red) around the beard, drawn by the unmasked blit 0xE0820. Whether the DOS game shows that red too was not settled, so the code was not committed. | 0x83A93, flag 0x13E91D, called from 0x830EF. | Not reachable in 1.3: 0x83A93 runs only when the phase pointer 0x13E922 is 0 (the frame callback 0x830D4). The screen entry 0x82B31 sets it to 0x8313C before the screen runner and all six writes are not 0 (recovered/town.md). Not ported. |
| FE-7 | The notes said the eyes of Khelben (FESEQ 31) are drawn. The pre-load callback 0x82BDB loads FESEQ 31 and sets an 8 s timer, but the blink code 0x82C28 (six patterns at 0x13E711, drawn at (294,158)) has no call, jump or pointer anywhere in the image: 1.3 never shows the eyes. The port correctly shows none. | Scan of the image for rel32 calls and jumps and for the absolute value 0x82C28: none. | Documented, 526a39e |
| FE-8 | The speed of the credits. The executable moves the text one pixel when 0x100 (1/256 s) has passed and polls without a wait, so the speed is the speed of the machine (two 416x470 blits and the text per step). The port uses 60 pixels a second. | 0x2C776 (`timer + 0x100`), 0x2C841. | Decision of the port: 1.3 has no fixed speed (at most 256 pixels a second, less when the two blits of a step take longer); the port keeps 60 pixels a second (comment in `fe_credits`) |
| FE-9 | The credits end on a key only in the executable (0xDA3E4 is the key check); the port also ends them on a mouse click. | 0x2C68B to 0x2C6B1. | Fixed: only a key ends the credits; `test_fe_credits_key_only` |
| FE-10 | Hires Dungeon Graphics (check box, 0x13A94B) of the options menu of the front end is left out; the port adds Controls..., Improvements... and Graphics... instead. | 0x58C18; docs/re/frontend.md section 10. | Open, decision of the port (P) |
| FE-11 | "Hear Khelben In English" (0x13E91E) is shown only when the language is not English. The port has no item. | 0x58C18. | Open, small; belongs with the localization work |
| FE-12 | Quit: the executable fades the palette out (0x7F54E(0x20)) before Function_mode 0; the port leaves at once. | 0x581E9. | Fixed: 32 steps before Fe_quit (no fade in a script run) |
| FE-13 | The notes listed the title movies as not done; dtumovie.c plays LOGO22S, INTRO2 and TLOGOS with the skip rule of 0x142254. | 0xB84F3 to 0xB858E. | Documented, 28686e5 |

## Checked and found to match

- Main menu items, actions and conditions (0x57F28, 0x58075), the Esc and
  Quit question, the last choice as the first current item.
- Title picture and version text (0xB85B8 to 0xB86EB: colors (12,0,28) and
  (63,0,0), offsets 10 and 11).
- Movie order and skip rule (0xB84F3).
- Options labels TEX[350], TEX[349], TEX[351] (0x58C18).
- Save slot menu: ten input items, 19 characters, `newmenu_do1` with the
  first slot current (0xBA5D4).
- Waterdeep music entry 0 (0x8C208: the loop on 0xA3BBA leaves EAX at 0).
- Character generation dice: three dice, a 1 is rolled again (0xB0C65).

## Not covered

The character generation screens were read but not compared line by line
with fenuchar.c beyond the points above; the market buy and sell code was
tested only through prices. The death menu was not run in this audit.
