# Architecture

This document describes how the source of ASCENT-DTU is organized. The facts
about the original game are in `docs/re/` and `docs/formats/`.

## Source layout

| Path | Content |
|---|---|
| `CMakeLists.txt`, `CMakePresets.json` | The top build file and the presets `debug` and `release`. |
| `cmake/` | Build scripts: `version.cmake` (the version header), `embed_text.cmake` (translations of the code into C arrays). |
| `engine/` | The Descent 1 source release of Parallax (version 1.5), with the port changes and the new game code. |
| `engine/includes/` | `types.h` and the shared assembler includes. `d_common` puts it on the include path. |
| `engine/fix`, `vecmat`, `div` | Fixed point math, vectors and matrices. |
| `engine/mem`, `misc`, `cfile`, `iff`, `pslib` | Memory, errors and arguments, file access, IFF pictures. `pslib` builds no code. |
| `engine/platform/` | The SDL3 layer: window, GPU renderer, audio, input, paths, log. Only this directory calls SDL. |
| `engine/bios/` | Keyboard, mouse, joystick and timer of the original, on top of the platform layer. |
| `engine/2d`, `3d`, `texmap` | The 8-bit 2D library, the 3D pipeline and the software texture mappers. |
| `engine/main/` | The game. The Descent 1 files and the new `dtu*.c` files. |
| `engine/demo/` | `cube`, a program that links the real libraries together. |
| `engine/lang/` | The translations of the text of the code (`<code>_port.catalog`). |
| `engine/third_party/` | stb_image, stb_vorbis and pl_mpeg, with their licenses. |
| `engine/main/editor/`, `engine/ui/` | The level editor of Descent 1 and its interface library. Not built. |
| `installer/` | `ascent-import`, `ascent-content`, the Inno Setup script and the content server worker. Windows only, but `ascent-import` also builds on Linux. |
| `tools/` | Python tools for the game files. See `tools/README.md`. |
| `docs/` | The documents. See `docs/README.md`. |

`engine/CMakeLists.txt` adds each library directory that has a
`CMakeLists.txt`. Each library is a target `d_<directory>`. The game is the
target `ascent` (`engine/main/CMakeLists.txt`). It is not in the default
build: build it by name. The library list of the game, `MAIN_SOURCES`, is
the list of `DESCENT_OBJS` of the original makefile with the new files.

Files that the build does not use, and why they stay, are listed in
[reference-sources.md](reference-sources.md).

The game program builds on Windows (MSYS2 UCRT64) and on Linux (GCC or
Clang). On Linux, `engine/includes/portable.h` supplies the functions of
the Watcom and Windows C libraries that the code calls (`stricmp`,
`strnicmp`, `strlwr`, `strupr`, `itoa`); `d_common` includes it in each
file. `ascent-import` builds on both systems; `ascent-content` and the
setup program are for Windows only.

## Game code

Each new file of `engine/main` starts with a comment that says what it
replaces and which document has the facts. The table is a summary of these
comments.

| File | System | Facts |
|---|---|---|
| `dtugame.c` | Start of a level without the Descent menus: `-datadir`, `-level`, `-checklevel`, `-frames`, `-screenshot`. Weak definitions of the data layer. | |
| `dtufile.c` | Access to the game files: the game directories, the disc directory, the packed files (DOG, COW, HEN). Replaces the HOG file. | `formats/packed.md` |
| `dtuobj.c` | Objects other than the player: the settings and tables, the names and the test log, the shared helpers (models, animation, sight, sounds), the corpse on the floor, the object setup at level load, rendering, the frame entry point (`dtu_objects_frame`), the revive of a corpse. The files `dtuobj_*.c` below were split from it; `dtuobj_int.h` has their shared declarations. | `re/objects.md`, `re/ai.md`, `re/models.md` |
| `dtuobj_setup.c` | The setup of the creatures at a level start: init_ai_object, add_npc, the level fixes (0xACCDB), the creature setup by name (0xAC7B2). | `re/objects.md`, `re/ai.md` |
| `dtuobj_item.c` | The inventory of the player, a new item on the floor, the pickup of an item, the tint of Detect Magic. | `re/objects.md` |
| `dtuobj_combat.c` | The fight of the creatures: the hit test, the to-hit roll, the damage of weapons and spells (with the rules of the trolls and the rats), the death of a creature and of the player, the weapon test of a melee swing. | `re/combat.md`, `re/ai.md` |
| `dtuobj_missile.c` | The missiles and the bursts: creation, flight, hits. | `re/combat.md` |
| `dtuobj_ai.c` | The AI of a creature: attacks, the turn and the moves, the paths, blocked moves, morale, animation, `dtu_do_ai_frame`, the calls of the scripts, the AI state in a state file. | `re/ai.md` |
| `dtuobj_move.c` | The movement of a creature: the stop distance at the player, gravity, `dtu_creature_physics_sim`. | `re/ai.md`, `re/physics.md` |
| `dtuobj_scenery.c` | Scenery that breaks, the end of the game at the Flamesword, pushed scenery, the collision of two objects. | `re/objects.md`, `re/movies.md` |
| `dtusolid.c` | Improved physics: scenery as solid bodies for the player (the top holds the player, a side stops the move). New in the port. | `re/physics.md` section 13.2 |
| `dtustep.c` | The fixed step of 1/60 second and the picture between two steps. New in the port. | `re/physics.md` section 14 |
| `dtuobj_aware.c` | Alerts and the awareness events of the creatures, the wake of a creature, the check of the objects, the gate in of a creature, the drop of objects in a segment. | `re/ai.md` |
| `dtuobj_test.c` | The test aids that place the player and the objects: `-testpos`, `-testobjpos`, `-testtour`. | |
| `dtuchar.c` | The player character: attributes, equipment, item use, rest, experience, levels, choice boxes. Replaces the ship values. | `re/character.md` |
| `dtucombat.c` | The weapon attack of the player, missile and thrown weapons, the arms in the view, Turn Undead. | `re/combat.md` part A |
| `dtuspell.c` | Spell books, slots, casting, spell effects, area effects, durations, saving throws, scrolls, wands. | `re/combat.md` part B, `re/spells.md` |
| `dtuthief.c` | Thief skills: Open Locks, Find/Remove Traps, Move Silently, Hide In Shadows, Climb Walls, backstab. | `re/combat.md` B7 |
| `dtuthiefpts.c` | The thief skill point screen. | `re/combat.md` B7 |
| `dtutrig.c` | Triggers, traps, level messages, the use action and the warps of DTU.WRP. Replaces the triggers of `switch.c`. | `re/triggers.md` |
| `dtuhook.c` | The creature and world hooks of `dtutrig.c`, the use action on an object, the start of a talk. | `re/triggers.md`, `re/ai.md` |
| `dtuscript.c` | The dialogue script interpreter, the global flags, the mission flags, the mission switch of Khelben. | `re/scripts.md` |
| `dtudlg.c` | The conversation window. | `re/scripts.md` section 4 |
| `dtudisp.c` | The in-game interface: play screen, buttons, cursor, message box, the screens that open during play. Replaces the cockpit and HUD. | `re/ingame-ui.md` |
| `dtuscroll.c` | The text screen of the Scroll. | `re/ingame-ui.md` |
| `dtudesc.c` | The description window of items and spells (right click). New in the port. | `description_window.md` |
| `dtuxp.c` | The experience of the player: the kill line, the next level window, the verbose combat log, the session log lines of deaths, awards and levels. New in the port. | `debug_logs.md` |
| `dtuinfra.c` | Infravision in the 3D view. | `re/infravision.md` |
| `dtufe.c` | The front end: frame, input, art, menus, main menu, options, credits, death screen. | `re/frontend.md` |
| `dtufe_script.c` | The script of the option `-fescript` (split from `dtufe.c`). | `re/frontend.md` |
| `dtufe_char.c` | Character generation. | `re/frontend.md` |
| `dtufe_town.c` | Waterdeep, Blackstaff tower and the market. | `re/frontend.md` |
| `dtustate.c` | Level state files and their copies in the save slots. Same format as the DOS game. | `re/levelstate.md` |
| `dtumusic.c` | Streamed music (TUN files) and the music commands of DTU.WRP. Replaces the MIDI songs. | `re/music.md`, `formats/tun.md` |
| `dtufoot.c` | Footstep sounds of the player and the creatures. | `re/music.md` |
| `dtumiss.c` | The miss sound of a melee attack (the `flash_sound` of the weapon). New in the port. | `re/combat.md` |
| `dtumovie.c`, `mvedec.c` | The movie player and the MVE decoder. | `re/movies.md`, `formats/mve.md` |
| `acm.c` | The Interplay ACM audio decoder (sounds of the EGG archives). | `formats/tun.md` |
| `dtuogg.c` | The Ogg Vorbis decoder for replacement music (stb_vorbis). | |
| `dtuhdui.c` | Upscaled interface pictures on the GPU renderer. New in the port. | `hd_textures.md` |
| `dtulang.c` | The language of the text: catalogs and lookups. New in the port. | `formats/lang.md` |
| `dtulangart.c` | The interface pictures of a language. New in the port. | `formats/lang.md` |
| `dtuopts.c` | The Improvements and Graphics settings and `ascent.cfg`. New in the port. | |
| `dtulog.c` | The session log and crash reports. New in the port. | `debug_logs.md` |
| `dtusoak.c` | The soak test. New in the port. | `re/bugs.md` |

Descent 1 files with large port changes: `physics.c` (player movement,
`re/physics.md`), `game.c` (the frame), `inferno.c` (start and options),
`gamesave.c` and `gamemine.c` (the level loader, `formats/lvl.md`), `bm.c`
and `piggy.c` (tables and EGG archives), `render.c`, `lighting.c`,
`automap.c`, `newmenu.c`. Each change has an `ASCENT:` comment.

Comments with an address (0x...) name the function or data of DTUGAME.EXE
1.3 that the code follows. The addresses are linear addresses of the image
with object 1 at 0x10000. The marks "(code)", "(doc)" and "(guess)" say how
the fact was found.

## Corrections of the original: Dtu_fix_*

The fidelity rule (see `CONTRIBUTING.md`) says that a defect of 1.3 is
ported as a correction that a player can turn off. A correction is a global
`int` named `Dtu_fix_<name>`:

- It is defined in the file that owns the rule, next to the other
  corrections of that file, with the value 1. Example: `int Dtu_fix_one_hit = 1;`
  in `dtucombat.c`.
- It is declared `extern` in the header of that file, with a comment that
  gives the defect and the original address.
- The code tests it where the rules differ: `if (!Dtu_fix_one_hit) { the 1.3 rule } else { the correct rule }`.
- The init function of the file clears it when an `-orig*` option is on
  the command line. The options are read once, with `FindArg`.

The corrections are not in `ascent.cfg` and have no menu item. They are on
in every normal run. The options give the exact 1.3 rules for a compare:

| Option | Corrections that it clears | Where |
|---|---|---|
| `-origphys` | `physics_steps`, `frame_drag`, `stop_decay`, `floor_push`, `look_return`, `fly_bob`, `turn_ramp`, `jump_exit`, `jump_path`, `ladder`, `wall_slide`, `wall_overlap` (all in `physics.c`) | `controls.c` |
| `-origobj` | `idle_anim`, `ai_ticks`, `creature_move`, `brain`, `path_follow`, `fire_once`, `attack_sound`, `cant_break`, `push_body`, `push_wall`, `push_rate`, `corpse_floor`, `magic_tint`, `guild_peace`, `drow5_shields`, `kobold_weapon`, `scenery_restore`, `bane_texture`, `partial_stack`, `revive`, `break_spread`, `noise_walls`, `alert_ray`, `alert_last`, `restore_spells`, `save_cast` | `dtuobj.c` |
| `-origrevive`, `-origdrow5`, `-origguildpeace` | one each: `revive`, `drow5_shields`, `guild_peace` | `dtuobj.c` |
| `-origcombat` | `one_hit`, `haste_attack`, `missile_rate`, `rat_hit`, `body_ray`, `turn_undead` (`dtucombat.c`); `stoneskin_word`, `rary_manual`, `melf_acid`, `spell_hours`, `cure_aid` (`dtuspell.c`); `wizardry_ring`, `potion_frame`, `luck_coin`, `speed_items`, `cursed_scroll`, `creature_missile`, `potion_class`, `chill_touch`, `slow_swing`, `kill_sound`, `backstab` (`dtuthief.c`), `creature_strength` (`dtuobj_combat.c`), `shield_ac` (`dtuchar.c`), `swing_pitch`, `attack_commit` (`dtucombat.c`, `dtuobj_combat.c`), `sword_view` (`dtucombat.c`), `spell_dice` (`dtuobj_combat.c`), `prayer_flags` (`dtuspell.c`) | `dtucombat.c`, `dtuchar.c` |
| `-origchar` | `multiclass_thac0`, `spell_save_repair`, `level_hit_points`, `endless_stack`, `level_thief` (`dtuchar.c`); `secret_door_names`, `thac0_row`, `new_thac0`, `find_traps_skill` (`dtuthief.c`), `save_seconds` | `dtuchar.c`, `dtuthief.c` |
| `-origtrig` | `trigger_table`, `trap_seconds`, `trap_close_door`, `trigger_repeat`, `disable_trap`, `warp_overlap`, `trap_pause` | `dtutrig.c` |
| `-origui` | `automap_reveal` (`automap.c`); `msg_scroll`, `view_aspect`, `button_hold` (`dtudisp.c`); `cheat_index` (`dtucheat.c`); `sensitivity_menu` (`menu.c`) | `automap.c`, `dtudisp.c`, `dtucheat.c`, `menu.c` |
| `-origscript` | `script_new_char`, `script_abilities`, `script_area_seg`, `mirt_points`, `script_run_mode` | `dtuscript.c` |
| `-origscriptwalk` | `script_walk` | `dtuscript.c` |
| `-origprison` | `prison_items` | `dtuscript.c` |
| `-origdrow5`, `-origguildpeace` | one each: `drow5_shields`, `guild_peace` | `dtuobj.c` |
| `-origsound` | `foot_pan`, `creature_steps` | `dtufoot.c` |
| `-origmusic` | `start_music`, `wrp_sound` | `dtumusic.c` |
| `-origlight` | `light_stable` | `lighting.c` |
| `-origitems` | `item_names` (`dtuitemfix.c`) | `dtuopts.c` |
| `-origfe` | `escape_load_waterdeep`, `chargen_abort`, `save_open_text` (`dtufe.c`); `start_spells`, `chargen_percentile`, `creation_luck` (`dtufe_char.c`); `market_charisma`, `market_sell_count` (`dtufe_town.c`) | `dtufe.c` |

The table was checked against the code on 2026-11-23: each `Dtu_fix_*` that
is defined with the value 1 is in one row. `chill_touch` (`dtuspell.c`) has
no option yet. `Dtu_fix_trap_seconds` is on by default as the others
(`-origtrig` clears it); the state files keep the trap timers in the unit of
the original. `Dtu_fix_lowres_aspect` is not a correction of a defect: it is
off by default (see below).

To add a correction:

1. Verify the defect in the decompilation or the executable. Write it in
   the document of the system in `docs/re/`, with the address.
2. Define `int Dtu_fix_<name> = 1;` in the owner file, with the other
   corrections of the file, and declare it in its header with a comment.
3. Keep the 1.3 rule in the code under `if (!Dtu_fix_<name>)`.
4. Clear it in the existing `-orig*` option of the file. Add a new option
   only for a new area.
5. Where 1.3 is broken or unclear, the corrected rule follows AD&D 2nd
   Edition.
6. Add the correction to the table above.

Changes that go beyond the original are not corrections. They are the
Improvements settings of `dtuopts.h` (`Dtu_improved_physics`,
`Dtu_improved_jump`, `Dtu_improved_pathfinding`, the GPU and upscale
settings, and others). They are in `ascent.cfg`, the menus change them, and
`-noimprovements` turns them all off. To add one, add the variable to
`dtuopts.c` and `dtuopts.h` and a line to the table `Opts[]` of
`dtuopts.c`.

The experience and combat log improvements (`dtuxp.h`):

| Setting | Default | `ascent.cfg` | One run | Menu |
|---|---|---|---|---|
| `Dtu_kill_xp_message`: the kill line shows the experience ("You killed Ghoul: 175 Exp.", the total also for two classes), and a death by another cause that the player sees says "no experience". Off: the 1.3 line "You killed X." | on | `KillXpMessage` | `-killxpmessage`, `-nokillxpmessage` | none |
| `Dtu_max_hp_level_up`: a level that rolls its hit die (0xAF694) gets the full die. The constitution bonus, the fixed hit points of the high levels and the half for two classes (0xAF86F) stay. A new character and statement 9F (0x8BA3A) already get the full die in 1.3. | off | `MaxHpOnLevelUp` | `-maxhplevelup`, `-nomaxhplevelup` | Maximum HP on level up |
| `Dtu_verbose_combat`: after the hit or miss message of an attack by or on the player, one line with the numbers: "Roll d20 14 +1 bonus = 15 vs 13: hit. Damage 1d8 5 +2 strength = 7." The number after "vs" is THAC0 minus the armor class in use. | off | `VerboseCombatLog` | `-verbosecombat`, `-noverbosecombat` | Verbose combat log |

A right click on the experience field of the interface screens or on a
class line of the statistics screen opens the next level window
(description window): the experience of each class and of its next level
(table 0x146048). The test aids: `swingat=N` (a swing of the player that
touches creature N), `killby=N.A` (creature A kills creature N), `descxp`,
`class2=N`. `tools/questtest.py` (kill_*, max_hp_*, verbose_combat_*) and
`tools/uitest.py` (test_next_level_window) test them.

`Dtu_miss_sound` (`dtuopts.c`, played by `dtumiss.c`) is "Miss sound" in the
Improvements menu and `MissSound` in `ascent.cfg`, on by default: a short
whoosh when a melee attack hits nothing (`re/combat.md`, "Miss sound").
`-origsound` and `-nomisssound` turn it off for one run without a change of
the setting. The miss sound is the `flash_sound` of the weapon from the game
files (`re/combat.md`).

`Dtu_fix_lowres_aspect` (`dtuopts.c`) is the shape of the 320x200 frame
of the low resolution in the window. It is off by default: square pixels
(16:10), so the interface pictures have the shape of the high resolution.
On, the window shows the frame at 4:3, as the DOS monitor did
(`plat_bind_vga_aspect`, docs/re/ingame-ui.md, "Aspect of the 3D view").
It is the item "4:3 screen" of the Graphics menu and `Lowres43` in
`ascent.cfg`; `-squarepixels` and `-screen43` set it for one run. The 3D
view follows the shown pixel (`Dtu_fix_view_aspect` in `dtudisp.c`: 1.0
with square pixels, 1.2 at 4:3, 2.0 in high resolution; `-origui` gives
the 1.2 and 2.4 of 1.3).

`Dtu_render_scale` (`dtuopts.c`) is "Internal resolution" in the Graphics
menu and `RenderScale` in `ascent.cfg`: the GPU renders the 3D view at x1
to x6 of the view canvas (default x3). The value 0 is the step "x1/2": half
the shown size in each direction, so the full screen view of the 640x480
interface is 320x239 (`g3_gpu_begin_view`). It gives the pixel size of the
320x200 game while the interface keeps its resolution; the low resolution
interface (`-lowres`) stays a command-line option. `-renderscale half`,
`0.5` or `1/2` selects it for one run. The view is scaled up with the scale
filter of the frame (gpu.c `compose`; `ASCENT_SCALE_FILTER=nearest` gives
plain blocks, `test_render_scale_half`). The software renderer always
draws at x1, and the menu says that it ignores this setting. The values 1
to 6 keep their meaning, so an older `ascent.cfg` stays valid.

## Test aids

ctest runs 16 programs (`test_*.c` in the library directories). None needs
the game files: `test_2d` skips its data checks when `ASCENT_DATA_DIR` has
no font files. CI runs them on Windows and on Linux with GCC and Clang
(`.github/workflows/build.yml`), and builds the game on both systems.

Tests that need the game files. `tools/run_game_checks.py` runs the game
checks of the tools in one command and prints one line for each check and a
total. Run it after each merge:

    ninja -C build engine/main/ascent.exe engine/main/test_dtutblfix.exe
    python -I tools/run_game_checks.py --jobs 4

It runs `check_items.py`, `check_desc.py`, `questtest.py`, `priestcheck.py`,
`check_saveload.py`, `check_itemfix.py`, `check_tblfix.py`,
`check_creature_setup.py` (the setup by name, with quest flag 67, and
`--abilities`) and `uitest.py`. The defaults are `build/engine/main/ascent.exe`,
`E:/UNDAMONTAN/DTU` (`--game`), `gamedata/` of the main checkout
(`--gamedata`) and the reference library of the decompilation (`--library`).
A check whose files are missing is skipped.

| Aid | Use |
|---|---|
| `test_dtudata` | Checks the data layer against the game files. Build it by name. Not in ctest. |
| `tools/uitest.py` | Interface tests: starts the game hidden at a level, drives it with `-fescript`, compares the screenshots. |
| `tools/lvlcheck.py` | Compares the level loader (`-checklevel`) with `tools/lvl.py`. |
| `tools/check_creature_setup.py` | Compares the creature setup by name (`-setuplog`) with the reference library of the decompilation. |
| `tools/objcheck.py` | Checks the object records of the levels against the level tables. |
| `tools/check_desc.py` | Checks the texts of the description window (`-chartest desc`) in both interfaces and both languages, and compares their numbers with the game (each armor, potion and wand, all 56 spells). |

Command line options for tests. An automated run (with `-hidden`, `-frames`,
`-fescript` or `-playmovie`) has a hidden window, no sound and no message
boxes.

| Option | Use |
|---|---|
| `-datadir DIR` | A game data directory. Repeat it for the disc directory. A run with it uses the current directory for the settings and the logs. |
| `-level NAME` | Start a level without the front end. |
| `-checklevel NAME` | Load a level, print a summary, exit. |
| `-frames N` | Leave the game after N frames. |
| `-screenshot FILE` | Write the last frame to a PPM file. |
| `-fixedfps N` | A fixed frame time from the first frame, so that two runs give the same moves. One step of the game for each picture (no fixed step). |
| `-renderfps N` | Each picture has 1/N second, with the fixed step of 1/60 second (`dtustep.c`). |
| `-nofixedstep` | One step of the game for each picture with the time of the picture, as the executable. |
| `-fescript FILE` | Drive the front end and, after `gamekeys`, the game with a script: keys, clicks, gamepad, waits, screenshots (`shot`). The command list is at the top of `dtufe_script.c`. |
| `-testmove ACTIONS` | Press movement keys for a time: `forward+run:2000ms,left:30,jump:1`. |
| `-testpos X,Y,Z[,H]` | Put the player at a position at the first frame. |
| `-testobjpos N,X,Y,Z[,H];...` | Put objects at positions at the first frame (pushed scenery tests). |
| `-testtour [FRAMES]` | Visit the center of each segment with 4 headings (texture coverage). |
| `-testkill N[,W]` | Every N frames the nearest active creature dies. |
| `-testdeath N` | The player dies at frame N. |
| `-testcrash N [KIND]` | Make a crash at frame N to test the crash report. |
| `-statetest DOOR,SIDE,OBJ[,WARP,BACK[,DAMAGE]]` | Open a door, damage a creature, warp away and back, and print the level state. |
| `-chartest LIST` | Character commands at frames: `str=`, `attackedby=`, `equip=`, `use=`, `xp=`, `castby=`, the look of a direct start (`charname=`, `female=`, `portrait=`, `race=`, `class=`) and more (see `run_command` in `dtuchar.c`). |
| `-testmarket` | The front end starts in the market. |
| `-testblackstaff` | The front end starts at Khelben in Blackstaff tower (the first visit). |
| `-autochoice N` | Choice boxes answer N. |
| `-dlgauto LIST` | Conversations take their replies from the list. |
| `-testspell N`, `-testslots N` | Put spell N in the book and hand; give N slots for each spell level. |
| `-soak SEED` | The soak test: random player actions with a check of all objects after each frame. Prints `SOAK FAIL` and `SOAK CRASH` lines. With `-soakdeath`, `-soakabort`, `-soaklog`. |
| `-loadshots`, `-movieshot` | Screenshots of the loading pictures and of movie frames. |
| `-setuplog`, `-objlog`, `-triglog`, `-scriptlog`, `-charlog`, `-musiclog`, `-soundlog`, `-movelog`, `-statelog` | Logs of one system. |

`engine/main/cc.sh` compiles single files of `engine/main` with the flags
of a build directory.

## The description window

`dtudesc.c` shows what an item or a spell does, on the picture of the scroll
text screen. A right click on the inventory screen, the spell lists or the
market opens it; `/` opens it for the entry of the keys. The hooks are small:
`dtudisp.c` (the right click and the key, no input while it is open),
`game.c` (its frame and its draw after the scroll text screen) and
`dtufe_town.c` (the market). The text reads the rule values of `dtuchar.c`
(`dtu_char_misc_use`, `dtu_char_item_powers` and the other `dtu_char_item_*`
functions) and of `dtuspell.c` (`dtu_spell_get_facts`). The item and spell
code reads the same values, so a change of a rule changes the text too. The
layout rules are in [description_window.md](description_window.md).

## Content packs and localization

The port runs with the original files only. Optional content is made from
the game files on the computer of the owner and downloaded by the
installer or by `ascent-content`. No content is in the repository.

1. `tools/textures_hd.py`, `tools/ui_hd.py` and `tools/movies_hd.py` make
   the upscaled textures, interface pictures and movies in `gamedata/hd`
   (`docs/hd_textures.md`). `tools/music_replace.py` makes replacement music.
2. `tools/content_pack.py` builds ZIP packs (`patch13`, `textures`, `ui`,
   `movies`, `lang_<code>`, `music`) and `manifest.txt` in `gamedata/content`
   (`docs/install.md`, "Content packs").
3. The owner copies the packs to the content server
   (`installer/server/worker.js`, `installer/content.ini`).
4. The setup program (`installer/build.sh`, `installer/ascent.iss`) and
   `ascent-content` download and verify the packs.

Localization (`docs/formats/lang.md`):

1. `tools/lang_extract.py extract` reads the English text of the game files
   and of the code and writes `en.catalog`. The strings of the code are the
   arguments of `LT`, `LN` and a few message functions in `MAIN_SOURCES`.
2. `split` cuts the catalog into parts for translators. `merge` joins the
   translations into `<code>.catalog`. `check` validates it.
3. The translation of the text of the code is in the repository
   (`engine/lang/<code>_port.catalog`). `cmake/embed_text.cmake` puts it into
   the program as `Lang_port_<code>`.
4. The translation of the game text is derived from the game files, so it
   is not in the repository. The pack `lang_<code>` installs it with the
   subtitles and the pictures of `tools/ui_lang.py`.
5. `dtulang.c` loads the catalog of the language of the setting
   `Language=` and looks up each text by id or by its English text.

## The decompilation project

DTU-Decomp is a separate repository: a Ghidra project of DTUGAME.EXE 1.3,
the decompiled C of each function, labels, and checked C reconstructions of
some systems (`recovered/`), built as a reference library with differential
tests against the original x86 code. It also has a cross reference table
from the port functions to the original functions.

ASCENT uses it as evidence: a rule in the port names the address of the
original function, and the function can be read in the decompilation.
`tools/check_creature_setup.py` runs the reference library to compare the
port with the original routine. The port does not include code or data
from the decompilation project.
