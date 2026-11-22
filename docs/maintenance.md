# Maintenance: refactors for a later pass

This is a ranked list of refactors. None is done yet. Each one must keep the
behavior: build, run ctest, and run the soak test and `tools/uitest.py` on
the levels the change touches before and after it. Do one file at a time and
move code without changing it in the same commit.

The sizes were measured on 2026-11-22 (lines of the file; a function is the
code between a `{` and a `}` in column 0).

## Size of the game files

| File | Lines | Functions | Largest functions (lines) |
|---|---|---|---|
| `dtuobj.c` | 8070 | 213 | `dtu_do_ai_frame` 443, `dtu_creature_physics_sim` 335, `dtu_objects_level_setup` 239, `dressing_move_frame` 176, `level_fixes_objects` 163, `weapon_test` 148 |
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

### 1. Split dtuobj.c

Problem: one file of 8070 lines holds the object setup, rendering, damage
and death, missiles, creature movement, the creature AI, paths, collisions
of pushed scenery and the test aids of the player. It has many file
statics, so every change touches the same file, and parallel work on it
conflicts.

Proposed split, along the section comments that exist:

- `dtuobj_setup.c`: "Setup" (creature setup by name, level fixes, the
  setup log).
- `dtuobj_damage.c`: "Damage", death, corpses, drops.
- `dtumissile.c`: missiles and bursts (`dtu_create_missile` to
  `missiles_frame`).
- `dtuobj_move.c`: creature gravity, integration, `dtu_creature_physics_sim`,
  "Collisions" and pushed scenery.
- `dtuobj_ai.c`: "The AI" and paths (`dtu_do_ai_frame`, `path_advance_body`).
- `dtuobj.c`: tables, helpers, inventory, rendering, the frame entry points.
- A private header `dtuobj_int.h`, as `dtufe_int.h` does for the front end.

Risk: high. The statics become shared, and the order of static
initialization and of the calls in one frame must not change. The AI files
are under active work: do this when no branch changes `dtuobj.c`. Split
`dtu_do_ai_frame` only after the move, in a separate change.

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

### 7. Move the test aids out of the game files

Problem: test code is mixed with game code: `run_command` (`-chartest`) in
`dtuchar.c`, `-testpos`, `-testobjpos`, `-testtour` and `-testkill` in
`dtuobj.c`, `-statetest` in `dtustate.c`, `-testmove` in `controls.c`.

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

### 10. Build the game on Linux

Problem: the libraries and tests build on Linux, but the game code calls
`stricmp`, `strnicmp` and `strlwr`.

Proposal: a small compatibility header in `engine/includes` that maps them
to `strcasecmp`, `strncasecmp` and a local lower-case function, and then the
`ascent` target in the Linux CI job.

Risk: low for the build; the run on Linux needs a separate test of the file
names (the game files are upper case).

## Other large functions outside the dtu files

`ai.c` `do_ai_frame` (832 lines) is the Descent 1 robot AI with port
changes. `physics.c` `dtu_player_physics_sim` (420 lines)
and `do_physics_sim` (565 lines) hold the player movement rules of
`docs/re/physics.md`. Leave them until the dtu files are split.
