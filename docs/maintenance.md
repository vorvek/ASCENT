# Maintenance: refactors for a later pass

This is a ranked list of refactors. Items 1 and 10 are done; the others are not. Each one must keep the
behavior: build, run ctest, and run the soak test and `tools/uitest.py` on
the levels the change touches before and after it. Do one file at a time and
move code without changing it in the same commit.

The sizes were measured on 2026-11-22 (lines of the file; a function is the
code between a `{` and a `}` in column 0).

## Size of the game files

| File | Lines | Functions | Largest functions (lines) |
|---|---|---|---|
| `dtuobj.c` before the split (2026-11-23) | 9559 | 260 | `dtu_do_ai_frame` 486, `dtu_creature_physics_sim` 362, `dtu_objects_level_setup` 261, `level_fixes_objects` 180 |
| `dtuspell.c` | 3154 | 130 | `apply_spell` 491 |
| `dtufe.c` | 3121 | 113 | `fe_menu` 249, `fe_run_screen` 160 |
| `dtuchar.c` | 3019 | 119 | `use_misc` 254, `dtu_char_update` 196, `run_command` 124 |
| `dtudisp.c` | 2396 | 32 | `do_input` 262 |
| `dtuscript.c` | 2281 | 86 | `sc_exec` 319 |
| `dtustate.c` | 1930 | 69 | `read_lvl_dgss` 167, `write_lvl` 163, `obj_from_image` 163, `obj_to_image` 135 |
| `dtutrig.c` | 1863 | 73 | `dtu_trig_fire` 186 |
| `dtucombat.c` | 1788 | 60 | `dtu_combat_turn_undead` 115 |
| `dtufe_town.c` | 1377 | 60 | |
| `dtufe_char.c` | 1259 | 49 | `make_player` 127 |

## Ranked list

### 1. Split dtuobj.c (done)

Done on 2026-11-23, on the text of cc4c21a. The functions moved without a
change: only `static` went away where another file now calls a function or
reads a variable, and `dtuobj_int.h` declares these, as `dtufe_int.h` does
for the front end. No name changed. A function whose name a Descent 1 file
also has (`create_path` of `aipath.c`) stays static, so its callers stay in
its file. One commit for each file:

| File | Lines | Functions | Largest functions (lines) |
|---|---|---|---|
| `dtuobj.c` | 1779 | 50 | `dtu_objects_level_setup` 261, `dtu_objects_frame` 76, `corpse_regen_frame` 73 |
| `dtuobj_ai.c` | 2436 | 57 | `dtu_do_ai_frame` 486, `blocked_move` 120, `follow_path` 116 |
| `dtuobj_combat.c` | 1644 | 58 | `weapon_sphere_test` 149, `damage_player_w` 100, `dtu_hit_test_vs` 79 |
| `dtuobj_scenery.c` | 998 | 20 | `dressing_move_frame` 177, `end_sequence_frame` 64 |
| `dtuobj_setup.c` | 934 | 25 | `level_fixes_objects` 180, `creature_setup_by_name` 112 |
| `dtuobj_move.c` | 619 | 6 | `dtu_creature_physics_sim` 362 |
| `dtuobj_missile.c` | 582 | 13 | `missiles_frame` 91, `missile_hit_object` 85 |
| `dtuobj_aware.c` | 465 | 10 | `dtu_gate_in_creature` 54 |
| `dtuobj_item.c` | 449 | 18 | `pick_up_item` 65 |
| `dtuobj_test.c` | 180 | 3 | `dtu_test_place_player` 40 |

The proof: the game checks of `tools/run_game_checks.py` and ctest gave
the same results before and after, the release build links, and hidden
runs with fixed random numbers (`seed=N`, `-fixedfps 30`, BONES, MINE and
CLAN3, melee, spells and the AI alone) wrote the same `-objlog` and
`-charlog` files, byte for byte.

Left for a later pass:

- Split `dtu_do_ai_frame` (486 lines) into steps, in a separate change.
- Many helpers of `dtuobj.c` are now shared through `dtuobj_int.h`
  (`obj_model`, `is_hostile`, `sound_at`, `tbl_int` and others). A later
  pass can give them `dtu_` names and move them to a header of their own.
- The table of the creature names (`Creature_names`) and the display names
  could go to a file of their own.

### 2. Make apply_spell a table of spell handlers

Problem: `apply_spell` in `dtuspell.c` is 491 lines, one branch per spell.
A fix to one spell needs a read of the whole function.

Proposal: one static function per spell or per spell group, and a table
indexed by the spell number, with the original address in each handler.
The shared start and end (saving throw, resistances, messages) stay in
`apply_spell`. Optionally move scrolls and wands to `dtuspell_items.c`.

Risk: medium. The order of the checks before and after the branch matters.
`-testspell` and `-chartest castby=` cover each spell.

### 3. Make sc_exec a table of statement handlers

Problem: `sc_exec` in `dtuscript.c` is a 319-line dispatch over the
statement codes of the script byte code.

Proposal: one function per statement and a table indexed by the statement
code, in the order of the dispatch at 0xAAE3E. `tools/script.py` uses the
same grammar and can check that every code has a handler.

Risk: medium. The interpreter keeps its state between frames, so each
handler must return the same "wait" result as the branch.

### 4. Split the input and the drawing of dtudisp.c

Problem: `dtudisp.c` has only 32 functions for 2396 lines. `do_input` is
262 lines and mixes the buttons, the keys and the screens that open during
play.

Proposal: `dtudisp_input.c` with `do_input` cut by screen (play screen,
inventory, spell book, automap keys); `dtudisp.c` keeps the drawing.

Risk: medium. `tools/uitest.py` covers the interface and must pass before
and after.

### 5. Use field tables for the level state files

Problem: `dtustate.c` reads and writes the same records in pairs of long
functions (`obj_to_image` and `obj_from_image`, `write_lvl` and
`read_lvl_dgss`). A field added to one side and not the other breaks the
save compatibility with the DOS game.

Proposal: one table per record (offset, size, field) that both directions
use, and a round trip test that needs no game files (build a level state in
memory, write it, read it back, compare).

Risk: medium to high. The format must stay byte-exact with the DOS game
(`docs/re/levelstate.md`). The decompilation project has a byte-exact
writer that can serve as the check.

### 6. Split dtufe.c

Problem: `dtufe.c` holds the frame, input, the `-fescript` interpreter, the
art loaders, the menu, the main menu, the options, the credits and the
death screen.

Proposal: `dtufe_script.c` (the `-fescript` interpreter) and `dtufe_menu.c`
(`fe_menu` and the options). `dtufe_int.h` already exists.

Risk: low to medium. The script interpreter also drives the game menus, so
keep its entry points unchanged.

Done in part on 2026-11-23: `dtufe_script.c` has the interpreter (376
lines, 7 functions), moved without a change; `dtufe_int.h` declares what
`dtufe.c` and `dtufe_script.c` share. Open: `dtufe_menu.c`. The menu code
reads `Fe_pal` of `dtufe.c`, and `dtudesc.c` includes `dtufe_int.h` and has
a static `Fe_pal` of its own, so a declaration of `Fe_pal` in `dtufe_int.h`
does not build. Rename the variable of `dtudesc.c` first, in its own commit.

### 7. Move the test aids out of the game files

Problem: test code is mixed with game code: `run_command` (`-chartest`) in
`dtuchar.c`, `-testkill` in `dtuobj.c`, `-statetest` in `dtustate.c`,
`-testmove` in `controls.c`. `-testpos`, `-testobjpos` and `-testtour` are
already in `dtuobj_test.c` (item 1).

Proposal: one file `dtutest.c` with the test options, which calls the game
through the public headers. The soak test (`dtusoak.c`) already works this
way.

Risk: low. Some aids read file statics; give them small accessor functions.

### 8. Split use_misc and dtu_char_update in dtuchar.c

Problem: `use_misc` (254 lines) is one branch per item kind;
`dtu_char_update` (196 lines) runs all the timers of the character.

Proposal: a handler per item group; one function per timer group (potions,
regeneration, effects) called from `dtu_char_update` in the same order.

Risk: medium. The order of the timers in one frame matters.

### 9. A registry of the Dtu_fix_* corrections

Problem: each file reads its own `-orig*` option, and five corrections have
no option (`docs/architecture.md`). The list of corrections exists only in
the document.

Proposal: one table (name, pointer, option, document) in one file. The
options clear the corrections of their group from the table, the session log
prints the table, and a new correction is one line.

Risk: low. The defaults must not change.

### 10. Build the game on Linux (done)

Done on 2026-11-23. `engine/includes/portable.h` gives `stricmp`,
`strnicmp`, `strcmpi`, `strlwr`, `strupr` and `itoa` on the systems other
than Windows. `d_common` includes it in each file with `-include`, so the
original files need no edit. The other fixes:

- `args.c`: a `/` at the start of an argument stays on Linux (an absolute
  path); on Windows it still becomes `-`.
- `text.c`: the escape scan used `strcpy` on overlapping strings. The
  `strcpy` of glibc gave `Unsupported key sequence` for `DTUE.TEX`.
- `dtusoak.c`: `unistd.h` for `_exit`.
- `installer/src/inst.c`: a parameter named `link` hid `link()`.
  `ascent-import` builds on Linux; `ascent-content` stays Windows only.
- `CMakeLists.txt`: on Linux the SDL3 search skips the directories of
  `PATH` (in WSL they have the SDL3 of MSYS2).

`dtufile.c` already finds a file when the case of its name differs
(`dtu_find_in_dir`). The CI job builds the game with GCC and Clang and
runs ctest.

## Other large functions outside the dtu files

`ai.c` `do_ai_frame` (832 lines) is the Descent 1 robot AI with port
changes. `physics.c` `dtu_player_physics_sim` (420 lines)
and `do_physics_sim` (565 lines) hold the player movement rules of
`docs/re/physics.md`. Leave them until the dtu files are split.
