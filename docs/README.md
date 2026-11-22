# Documents

## Start here

A new contributor can read in this order:

1. [../README.md](../README.md): what the port is and what it needs.
2. [../CONTRIBUTING.md](../CONTRIBUTING.md): the rules for game files, facts, the fidelity rule, commits and licenses.
3. [architecture.md](architecture.md): the source layout, which file owns which system, the corrections, the test aids.
4. [porting.md](porting.md): the rules for a change to the engine code.
5. The `docs/re/` document of the system you work on, then its file formats in `docs/formats/`.
6. [audit/README.md](audit/README.md): the open items of the last audit.

Build with `cmake --preset debug`, `cmake --build --preset debug --target ascent`, and test with `ctest --preset debug`.

## Project

- [architecture.md](architecture.md): source layout, game files by system, `Dtu_fix_*` corrections, test aids, content and localization pipeline, the decompilation project.
- [porting.md](porting.md): rules for the engine code (build, types, assembly, DOS, edits).
- [reference-sources.md](reference-sources.md): original files that the build does not use, and why they stay.
- [maintenance.md](maintenance.md): ranked refactors for a later pass.
- [install.md](install.md): the installer, the 1.3 patch, the release build, the content packs and the content server.
- [hd_textures.md](hd_textures.md): how the upscaled textures, interface pictures and animations are made.
- [debug_logs.md](debug_logs.md): the session log and crash reports, for players and developers.
- [../tools/README.md](../tools/README.md): one line for each tool.

## File formats (verified against the game files)

- [formats/exe.md](formats/exe.md): DTUGAME.EXE, the layout of the linear executable.
- [formats/packed.md](formats/packed.md): the disc layout and the packed files (DOG, COW, HEN).
- [formats/egg.md](formats/egg.md): EGG archives (bitmaps, sounds, models) and BIN tables.
- [formats/tbl.md](formats/tbl.md): the text of the BIN tables (the BITMAPS.TBL dialect).
- [formats/jrg.md](formats/jrg.md): JRG index files, their DAT files and related small files.
- [formats/lvl.md](formats/lvl.md): LVL and LV2 level files.
- [formats/brn.md](formats/brn.md): BRN segment visibility matrix.
- [formats/tig.md](formats/tig.md): TIG level triggers.
- [formats/tap.md](formats/tap.md): TAP level traps.
- [formats/say.md](formats/say.md): SAY level messages.
- [formats/misc.md](formats/misc.md): small text and table files.
- [formats/tun.md](formats/tun.md): TUN music files (Interplay ACM).
- [formats/mve.md](formats/mve.md): MVE movies.
- [formats/lang.md](formats/lang.md): the text catalogs of the translations (a format of the port).

## Reverse engineering of DTUGAME.EXE 1.3

- [re/bugs.md](re/bugs.md): crashes and faulty logic of 1.3, and the state of the port.
- [re/objects.md](re/objects.md): object setup at level load.
- [re/ai.md](re/ai.md): creature behavior, brains and dialogue starts.
- [re/models.md](re/models.md): polygon models and animation.
- [re/physics.md](re/physics.md): player movement.
- [re/character.md](re/character.md): the player character and the inventory.
- [re/combat.md](re/combat.md): combat and magic of the player.
- [re/spells.md](re/spells.md): audit of the spells of the port against 1.3.
- [re/triggers.md](re/triggers.md): triggers, traps, messages, doors and warps.
- [re/scripts.md](re/scripts.md): scripts, conversations and mission flags.
- [re/levelstate.md](re/levelstate.md): level state files.
- [re/frontend.md](re/frontend.md): the front end.
- [re/ingame-ui.md](re/ingame-ui.md): the in-game interface.
- [re/controls.md](re/controls.md): the key configuration.
- [re/automap.md](re/automap.md): the automap.
- [re/infravision.md](re/infravision.md): infravision.
- [re/effectcolors.md](re/effectcolors.md): effect colors of the texture mappers.
- [re/music.md](re/music.md): music and sound playback.
- [re/movies.md](re/movies.md): movies.

## Audit of 2026-11-22

- [audit/README.md](audit/README.md): summary of the five audits and the test aids they added.
- [audit/frontend.md](audit/frontend.md): the front end and the town.
- [audit/ingame-ui.md](audit/ingame-ui.md): the in-game interface.
- [audit/combat.md](audit/combat.md): combat, spells, items and character rules.
- [audit/world.md](audit/world.md): the world, scripts and levels.
- [audit/presentation.md](audit/presentation.md): rendering, audio and the platform.
- [audit/decomp-gaps.md](audit/decomp-gaps.md): gaps against three decompilation notes.
