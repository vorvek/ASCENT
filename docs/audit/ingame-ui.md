# Audit: the in-game interface

Scope: the interface modes, the paper doll and the inventory list, the hot
keys, the spell lists, the statistics screen, the map screen (Tab), the
message box, the conversation window, the Escape menu and the F2 options,
the key configuration, the gamepad, immersive mode, the cursor, the help
screen (F1) and the F keys. Files: `engine/main/dtudisp.c`, `dtudlg.c`,
`kconfig.c`, `automap.c`, `controls.c`, `dtuhdui.c`, `newmenu.c`, and the
key switch in `game.c` and the options menu in `menu.c`.

Method: the code of DTUGAME.EXE 1.3 (`gamedata/re/all.asm`, addresses of
the flat image with base 0x10000) against the port, and test runs of the
port (hidden, muted, `-level YPINN -fescript`, screenshots). The tests are
in `tools/uitest.py` (they need the game files, so they are not ctest
tests):

    python tools/uitest.py --exe build/engine/main/ascent.exe \
        --datadir E:/UNDAMONTAN/DTU --datadir D:/dev/ASCENT/gamedata/cd/DTU \
        --hddir D:/dev/ASCENT/gamedata/hd --char COPY_OF_SG1.SG

With the build of main (6e8dea2) 8 of the 12 tests fail; on this branch all
12 pass.

## Findings

Most important first. Status: the commit of the fix, or open with an
estimate of the size (S: under a day, M: a few days, L: more).

| Id | Description | Evidence | Status |
|---|---|---|---|
| U1 | While the conversation window was open, the interface still took its keys and clicks: I, Tab, P, R (rest), V, N, F3, the hot keys and the buttons. The screens opened under the window, and the window was then drawn over the inventory or the map picture. The executable runs the reduced frame 0x4391D while the window waits: it draws the displays (0x324DA) but calls neither the display input 0x2FB10 nor `controls_read_all`. | Calls of 0x4391D; YPINN, Durnan (`-testpos -60,-8.7,9,270 -testmove wait:10,use:1`), keys I and Tab: main build opens the inventory and the map; `test_dlg_blocks_interface` | fixed, 5e00a92 |
| U2 | F3 in a screen (inventory, map, statistics) did nothing, and the keys "-" and "=" did nothing. The key switch 0x42F9C: F3 calls 0x40C3A in mode 7, else 0x40974; "=" (also with Shift) calls 0x40974, "-" (also with Shift) 0x40C3A. 0x40974: mode 0 to full screen (7), a screen to mode 0. 0x40C3A: mode 7 to mode 0. The F1 help text of the game says "-/+ Change Screen Size". | 0x43043, 0x43193, 0x432A8, 0x40974, 0x40C3A; screenshot of the inventory before and after F3; `test_f3_screen`, `test_equal_minus` | fixed, 6d4945d |
| U3 | Alt+F2 opened the save picker of Descent 1 (`state_save_all`, its save function does nothing in the port, and it stops the time without a start). Alt+F3 opened the Descent 1 restore picker ("No saved games were found!"). The executable calls its own save 0xBA99F (slot menu 0xBA486) and load 0xBB780(1), the same as Save Game and Load Game of the Escape menu. | 0x432B8, 0x432C2; screenshots of both pickers in the main build; `test_alt_f2_f3` | fixed, cfe7e98 |
| U4 | The scroll bar of the message box: only a click on an arrow at the press acted. A held arrow did not repeat, a click or a drag in the bar did nothing, the low resolution slider was missing, the high resolution slider was centered and its place was a guess. The executable (0x2FF58) handles it as the inventory scroll bar (0x363D4, step 1, in each frame while the button is down, maximum the line count minus the visible lines), and draws it with 0x3654C(first line, line count minus 3) with the slider at the left edge. | 0x2FF58 to 0x3005D, 0x3475B to 0x34795; `test_msg_bar_and_hold` | fixed, 0274314 (the arrows repeat after 0.4 s as the inventory arrows of the port; `Dtu_ui_key_repeat` 1 gives each frame) |
| U5 | Defect of the original: the up arrow of the message box takes 1 off the first line (the reverse flag of the SBar is 0), so it shows newer lines although the newest line is at the bottom of the box, and the slider is at the top for the newest line. | 0x363D4 (+0x26 tested at 0x3642A), 0x34785; `test_msg_arrows`, `test_msg_arrows_orig` | fixed, 0274314: `Dtu_fix_msg_scroll` (always on, `-origui` gives the original) |
| U6 | The conversation window had no Tab, and Space with no reply went on. In the executable Space and Tab move the focus between the replies and the text in all states (0xB2A47, 0xB2BB5 with no reply, 0xB2CEB back); with no reply only Enter and keypad Enter go on. The key ` (screenshot) was missing in the window. docs/re/scripts.md said that Space goes on; corrected. | 0xB2B6F to 0xB2BE0, 0xB28F0, 0xB2BED; `test_dlg_tab_focus` | fixed, a95b3cd |
| U7 | Alt+F3, Shift+F3, Ctrl+F3 also changed the screen size (the port read the state of F3 without its modifiers). The switch tests the key code with its modifiers. | 0x42E62 to 0x42F15; screenshot of the load picker over a full screen view in the main build; `test_alt_f2_f3`, `test_shift_fkeys_and_grave` | fixed, a753bd9 and ae5e7fc |
| U8 | Shift+F1 to Shift+F4 and Shift+F8 added Descent 1 stereo messages to the message box ("Stereoscopic balance = 8.6", "Stereo separation = 6.4", "Head tracking sensitivy = 2"). The number keys went to the Descent 1 weapon select when the hot key page gave the hot keys other keys. The switch of the executable has no Shift F keys, and the number keys are always the hot keys (0x430D1). | Screenshot of the message box after Shift+F1, F3, F8; `test_shift_fkeys_and_grave` | fixed, ae5e7fc |
| U9 | The F2 options menu was the Descent 1 menu: FX, Music, Reverse Stereo, Brightness, Controls, Detail levels, Calibrate Joystick (always), Sensitivity 0 to 8, "Ship auto-leveling". The executable in the game: Brightness, Music Volume, FX Volume, Reverse Stereo (each followed by a blank), Detail levels..., Controls..., Joystick/Mouse Sensitivity 0 to 7 (a value above 7 is lowered to 7), Calibrate Joystick only for the control types 1 to 4, a blank. | 0x58C18 to 0x59014 (TEX 352, 350, 349, 351, 354, 353, 356, 355); screenshots before and after | fixed, 5f59f10 ("Improvements..." and "Graphics..." of the port take the places of two blanks) |
| U10 | My first fix of U2 gave minus and equal to the screen also while the map had the controls; the map reads one key per frame first (0x1893F: Escape, minus, equal, Alt+F, D, Print Screen). | 0x1893F; `test_map_minus_equal` | fixed, 81c6d48 |
| U11 | Immersive mode off did not move the mouse. 0x54439 puts it in the middle of the view (0x54470 to 0x544D7, 0xE89A8). | 0x54439; `test_immersive_off_centers_mouse` | fixed, 98a6736 |
| U12 | The key ` (grave) did nothing in the game. The executable takes a screenshot with it (0x42FEC, 0x80DA1, as Print Screen). | 0x42F69 to 0x42FF5; `test_shift_fkeys_and_grave` | fixed, 2c61777 |
| U13 | The Joystick/Mouse Sensitivity slider of F2 has no effect on the walk controls of the port: `dtu_read_controls` (controls.c) uses a fixed 8/8. The executable scales the look and turn values by the byte 0x13BFEF / 8 at 13 places (0x52BCE to 0x53A8C) and keeps the byte in kconfig.dat. | `grep Config_joystick_sensitivity engine/main/controls.c` (none); 13 reads of 0x13BFEF | open, S to M: read the 13 places, store the byte in `ascent.cfg`, scale the mouse, joystick and key look values |
| U14 | The HUD messages of Descent 1 code (`HUD_init_message`) go to the message box in the port. In the executable the HUD message ring 0x4F31A is filled, but its draw function has no caller, so these messages are never shown (for example TEX 738 "Press F3 to return to Window mode" at each screen size change). The port does not call it for F3. Other callers of `HUD_init_message` in the port may show texts that the original never shows. | No call of the draw function after 0x4F0A5; 0x4098C, 0x409C6, 0x40C4C | open, S: list the callers of `HUD_init_message` and keep only those that the executable routes to 0x347A2 |
| U15 | The Descent 1 cheat codes of `game.c` (`john_cheat_func`, the wowie, the full map) still run in the game. The executable has its own cheat tables (0x13B74F, 0x13B753, 0x13B782, tested at 0x4324E and 0x428B9). | 0x428B1 to 0x4324E | open, M (shared with the combat and items audit) |
| U16 | The immersive key V: the executable lets it act at most once in 0.364 s (key gate 0x545F5), so a held key toggles again and again; the port toggles once for each press. | controls.md, ingame-ui.md | open, S (a port change that helps, not a defect; list only) |
| U17 | The original message box scroll bar draws the arrows only with more lines than visible lines; the black fill covers them otherwise (as before). The manual picture shows them with an empty box. | 0x346CA | as the original, no change |
| U18 | The color of the message lines: the first byte of a line is its color in the executable; the callers of 0x34998 were not traced, the port uses grey (48,48,48), and the color of the player (30,30,46) for triggers and the V and N messages. | ingame-ui.md | open, S (trace the callers of 0x34998 and 0x347A2) |
| U19 | The interface of the low resolution (`-lowres`): main screen, inventory, map, statistics checked by screenshot, no defect seen. The "7" of the game font looks like a "?" at a small size, it is the glyph of the font. | screenshots with SG1.SG | no defect |
| U20 | The Controls menu (0x50B4F) and the key screen: the items and the radio buttons are as controls.md. Whether the radio button of the current control type is drawn filled could not be read from the screenshot at this size. | screenshots | open, S: check the radio glyphs of the font |

## Notes for the other work

- Text: the new options menu (U9) keeps the `TXT_` names of `menu.c` and two
  English strings of the port ("Improvements...", "Graphics...") that were
  there before. No string table was changed.
- `dtufe.c`: two small functions (`dtufe_save_key`, `dtufe_load_key`) and
  their handling in `dtufe_game_frame`, next to the Escape menu. The argument
  of 0xBA99F at Alt+F2 is not set by the switch (eax holds a value of the
  cheat tests before it); the port saves as the Escape menu, 0xBA99F(0).
- `save_screen_shot` writes to the current directory, not to the user
  directory (platform audit).
