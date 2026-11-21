# The key configuration of DTUGAME.EXE 1.3

Addresses are those of the flat image with base 0x10000 (`gamedata/re/dtu_flat.bin`).
Text numbers are lines of `DTUE.TEX` (the first line is 0), as `Text_string` of the
port. **V**: read from the code or the data. **G**: guess, with the reason.

## 1. Tables

**V** The executable keeps the Descent 1 key configuration (`kconfig.c`) with new
tables. An item has 22 bytes, the layout of `kc_item` of Descent 1 with a text number:
`short id, x, y, w1, w2, u, d, l, r, text; ubyte type, value` (0x515AB reads type and
value at +0x14 and +0x15).

| Table | Address | Items | Type |
|---|---|---|---|
| keyboard | 0x13C190 | 62 | 0 key |
| joystick | 0x13C6E4 | 27 | 3 button, 4 axis, 5 invert |
| mouse | 0x13C978 | 29 | 1 button, 2 axis, 5 invert |
| defaults | 0x13C010 | 6 rows of 64 bytes | row 0 keyboard, 1 to 4 the joystick types, 5 the mouse |

The settings are `kconfig_settings` at 0x1E9A88 (6 x 64 bytes). 0x5413F
(`kc_set_controls`) copies row 0 into the keyboard items, row `type` (1 to 4) into the
joystick items or row 5 into the mouse items. The control type is the byte 0x13BFED
(default 5, the mouse, 0x71A07), the look sensitivity the byte 0x13BFEF (default 8).

## 2. The keyboard items

**V** Two items for each action, the second key at the next item, but Automap has the
items 38 and 49. The items 20 and 21 have the text "(unused)" and are not drawn.
Default (0x13C010) and the default of the port:

| Items | Text | Action | Executable | Port |
|---|---|---|---|---|
| 0, 1 | 378 | Auto Run (run always, toggle) | Caps Lock | Caps Lock |
| 2, 3 | 561 | Jump | Space | Space |
| 4, 5 | 380 | Turn left | Left, Pad 4 | Left, Pad 4 |
| 6, 7 | 381 | Turn right | Right, Pad 6 | Right, Pad 6 |
| 8, 9 | 382 | Slide on (toggle) | LAlt, RAlt | LAlt, RAlt |
| 10, 11 | 383 | Slide left | Z, Pad 1 | A, Pad 1 |
| 12, 13 | 384 | Slide right | X, Pad 3 | D, Pad 3 |
| 14, 15 | 385 | Float up | Ins | Ins |
| 16, 17 | 386 | Float down | Del | Del |
| 18, 19 | 390 | Attack | LCtrl, RCtrl | LCtrl, RCtrl |
| 22, 23 | 562 | Immersive Mode | V | V |
| 24, 25 | 393 | Go Forwards | Up, Pad 8 | W, Up |
| 26, 27 | 394 | Go Backwards | Down, Pad 2 | S, Down |
| 28, 29 | 395 | Cycle Weapon | C | C |
| 30, 31 | 556 | Look Up | PgUp | PgUp |
| 32, 33 | 557 | Look Down | PgDn | PgDn |
| 34, 35 | 558 | Quick Up (glance) | Home | Home |
| 36, 37 | 559 | Quick Down | End | End |
| 38, 49 | 374 | Automap | Tab | Tab |
| 39, 40 | 555 | Activate | A | E |
| 41, 42 | 560 | Run | LShift, RShift | LShift, RShift |
| 43, 44 | 563 | Inventory | I | I |
| 45, 46 | 564 | Stats | S | P |
| 47, 48 | 565 | Infravision | N | N |
| 50, 51 | 776 | Disarm | D | H |
| 52, 53 | 777 | Turn Undead | T | T |
| 54, 55 | 778 | Move silently | M | M |
| 56, 57 | 779 | Find Traps | F | F |
| 58, 59 | 780 | Open Locks | O | O |
| 60, 61 | 961 | Rest | R | R, Z |

**V** `controls_read_all` reads each keyboard item (0x5313D to 0x54002). The actions of
the items 28 to 61 (0x53D05 to 0x5402E): Cycle Weapon 0x544F2, Activate 0x95050,
Immersive 0x54439, Automap 0x408F5 ([automap.md](automap.md)), Inventory 0x408A2, Stats 0x4093A, Infravision
0x54632, Disarm 0x43A5A(-3), Turn Undead 0xADC46, Move silently 0xB4196, Find Traps
0xB3400, Open Locks 0xB31BD, Rest 0x360DA(1).

**V** Cycle Weapon (0x544F2) goes through the 10 hot keys from the one after the last
chosen (0x13AD13) and takes the first that holds a usable weapon (class 0x11, or class
5 with the tests of 0x1E9D44), then 0x43A5A. Infravision (0x54632, key gate 0x545F5):
without infravision (P+0xB5, or 5 with bit 1 of P+0xFA) and without the effect bit 15
of the player object TEX 819 "You don't have Infravision.", else the flag 0x14C4DA
changes with TEX 820 or 821 and bit 8 of the byte 0x1AE of the player object. The view
is in [infravision.md](infravision.md).

The hot keys 1 to 0 are not in the table. The system keys (0x13BFD0, as Descent 1:
Escape, F1 to F12, 0 to 9, minus, equal, Print Screen) cannot be chosen.

## 3. Joystick and mouse items

**V** Joystick (text): 0 Attack, 1 Immersive Mode, 2 Go Forwards, 3 Go Backwards,
4 Activate, 5 Slide on, 6 Slide left, 7 Slide right, 8 Float up, 9 Float down, 11 Run,
26 Cycle Weapon (buttons); 13/14 Look U/D, 15/16 Turn L/R, 17/18 Slide L/R, 19/20
Float U/D (axis and invert); 10, 12, 21 to 25 "(unused)". Default row 1: Attack button
1, Activate button 2, look axis Y1, turn axis X1.

**V** Mouse: 0 Attack, 1 Jump, 2 Go Forwards, 3 Go Backwards, 4 Immersive Mode, 5 to 9
slide and float, 10 Run, 26 Cycle Weapon, 27 Activate (buttons); 13 to 20 the axes as
above; 11, 12, 21 to 25, 28 "(unused)". Default row 5: Attack left button, Activate
right button, look axis F/B, turn axis L/R. 0x52DE2 to 0x52E13 read the Immersive
Mode button and the Attack button in each mode; the other mouse items (0x52E19 to
0x530C1) only in the immersive mode (0x13BAE0).

## 4. The screen (0x51170)

**V** `kconfig_sub` of Descent 1: the box background (0x632EE, the stone picture),
the title in font 3 at y 8, TEX 459 "Enter changes, ^D deletes, ^R resets defaults,
Esc exits" at y 20 (color 56,56,56), the headers (0x50DF8 keyboard: "or" (TEX 816)
over each pair of key columns at 109,40 and 264,40 with the Descent 1 lines; 0x50F34
joystick, 0x5105C mouse: Buttons, Axes, Axis, Invert? as Descent 1). Each item
(0x51778): the label in a box of `w1 - 1` pixels (color 40,40,58 current, 30,30,48
else), the value box `x + w1` to `x + w1 + w2` (42,0,48 current, 32,0,38 else) and the
key name centered (56,56,56). Keys: arrows move by u, d, l, r; Enter changes; Ctrl-D
(0x420) clears; Ctrl-R (0x413) sets the defaults of the page (row 0 or the row of the
control type). The key names (0x13BBB8) are those of Descent 1 with UP, DOWN, LEFT,
RIGHT as words.

**V** In the high resolution form (0x13A94B) each coordinate is doubled (0x50DC7 to
0x50DEB, 0x51668, 0x5171E) and the items use the font at 0x13E380 (FNTEMFM_.FNT).

**V** The Controls menu (0x50B4F, called from the options menu 0x58C18 in the game
only): radio items Joystick, CH Flightstick Pro, Thrustmaster FCS, Gravis Gamepad,
Mouse (TEX 143 to 147, the control types 1 to 5; no "Keyboard only"), "Customize
Above", a blank line, "Customize Keyboard".

## 5. Files

**V** `kconfig.dat` (0x7190F writes, 0x719E3 reads): the 384 bytes of 0x1E9A88, then
0x13BFED and 0x13BFEF (`docs/formats/jrg.md`). With no file the defaults are copied.
The file of the install differs from the defaults in row 0 only: items 10 A, 12 D,
24 W, 26 S, 39 E, 45 P, 50 H, 60 Z.

**V** `KCFG.FRE`, `KCFG.GER`, `KCFG.ITA`, `KCFG.SPA`, `KCFG.NON`, `KCFG.MAR`: key names
for the other languages (table 0x1457AB by the language 0x1456F8, read by 0xAEEE8).
English has no file (entry 0): the names of the code are used. `KCFG.ENG` on the disc
is a sample (`docs/formats/misc.md`).

## 6. The port

`engine/main/kconfig.c` (end of the file): the tables above, the settings, the screen
(`dtu_kconfig`) and the Controls menu (`dtu_kconfig_menu`) in the form of the
executable. Changes:

- The port defaults of the keyboard (table above). W and S take the place of keypad 8
  and 2.
- A page "Hot Keys" (port): 10 hot keys, two keys each, 1 to 0 by default. The digits
  are no system keys in the port. A key leaves each other item of both keyboard pages.
- Items "Customize Hot Keys", "Customize Gamepad" and "Reset to defaults" in the
  Controls menu. The menu opens from the F2 options and, in the port, from the options
  of the front end (with the palette of the game).
- The settings are in `ascent.cfg` of the current directory (`ControlType`,
  `KeyConfig0` to `KeyConfig5` with the 64 bytes of each row in hex, `KeyConfigHot`,
  `PadConfig`, `PadDeadZone`, `PadSpeed`, `PadInvertLook`), not in `kconfig.dat`.
- The mouse buttons act only in the immersive mode (the Attack button too: the left
  button clicks the interface in the other mode). **G** whether the executable lets a
  click on the interface attack.
- Cycle Weapon takes the next hot key that holds an item (the weapon tests are not
  ported). Infravision: see [infravision.md](infravision.md).
- `-keyconfig "forward=Q;back=Y,DOWN"` sets keys for a test.

### Gamepad (port)

SDL_Gamepad through `engine/platform/input.c` (`plat_pad_*`). Each action has two
gamepad bindings: a button, or a direction of a stick or a trigger. The sticks give an
amount (after the dead zone, 0 to 32% in steps of 4%, default 24%); the turn and the
look are scaled by the speed setting (1 to 16, 8 is the rate of the turn keys), with
no ramp. In the menus the d-pad and the left stick give the arrow keys, A Enter and B
Escape; Start is Escape in the game and in the menus (not a binding).

Defaults: left stick walk and side step, right stick turn and look, A Jump, X and RT
Attack (a spell in the hand is cast with Attack: the game has no separate cast
action), B Activate, Y Cycle Weapon, LT Run, left stick click Auto Run, right stick
click Quick Down, Back Automap, d-pad up Inventory, down Rest, left Find Traps, right
Stats, LB and RB the previous and the next hot key. No default: Slide on, Float up and
down, Immersive Mode, Quick Up, Infravision, Disarm, Turn Undead, Move silently, Open
Locks, the hot keys 1 to 10.

Tests: `engine/platform/test_pad.c` (ctest `platform_pad`), and the `-fescript`
commands `padbutton NAME 0|1` and `padaxis NAME VALUE`, which call the functions that
the event loop calls.
