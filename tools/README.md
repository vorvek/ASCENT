# Tools

The tools are Python 3 scripts. Each one starts with a usage text at the top
of the file. Run them with `python -I` so that no
file of the current directory can replace a module. Most use only the standard
library. The tools read the game files and never write into the directory of
an input file unless you name it as the output.

## File formats

- `unpack.py`: unpack the compressed files of the disc (DOG, COW, HEN to LVL, EGG, BIN). See `docs/formats/packed.md`.
- `iso.py`: list and extract the files of an ISO 9660 disc image.
- `egg.py`: list, check and extract EGG archives (bitmaps, sounds, models) and decode BIN tables. See `docs/formats/egg.md`.
- `jrg.py`: list, check and extract the front end archives (JRG with DAT, GABx.RLH with GABx.DAT). See `docs/formats/jrg.md`.
- `tun.py`: read the TUN music files (Interplay ACM) and decode them to WAV. See `docs/formats/tun.md`.
- `lvl.py`: parse LVL and LV2 level files, list their contents, export the geometry to OBJ. See `docs/formats/lvl.md`.
- `sidefiles.py`: dump the small level files (BRN, TIG, TAP, SAY, THESTORE, FNT) as text.
- `model.py`: read a polygon model of an EGG archive, pose it with an animation frame, write OBJ or a PNG sheet. See `docs/re/models.md`.
- `script.py`: disassemble the dialogue scripts of GABx.DAT. Its listing is the reference for `engine/main/dtuscript.c`.

## Checks of the port against the data

- `run_game_checks.py`: run all the checks below that need the game files and print one line for each and a total. Run it after each merge: `python -I tools/run_game_checks.py --jobs 4`. The defaults: `build/engine/main/ascent.exe` (and `test_dtutblfix.exe` beside it), `--game E:/UNDAMONTAN/DTU`, `--gamedata` the `gamedata/` of the main checkout (also from a worktree), `--library` the reference library of the decompilation. `--list` prints the commands, `--only` picks checks.
- `check_items.py`: the item effects (potions, wands, rings, armor sets, the Sword of W'hin, misc items) with `-chartest`. See `docs/audit/gear.md`.
- `check_desc.py`: the texts of the description window and their numbers against the game. See `docs/description_window.md`.
- `questtest.py`: the quests of the campaign, with a save and a load. See `docs/re/quests.md`.
- `priestcheck.py`: the priest spells and turn undead. See `docs/audit/spells-priest.md`.
- `check_saveload.py`: a save and a load with a level change, and the spell slots.
- `check_itemfix.py`: the item corrections of the port against the level tables.
- `check_tblfix.py`: the table fixup of 1.3 (0x1A344) against the level tables and `tblfix.py`.
- `lvlcheck.py`: compare the level loader of the port (`-checklevel`) with `lvl.py`.
- `objcheck.py`: check the object records of the level files against the level tables (`docs/re/objects.md`).
- `check_creature_setup.py`: compare the creature setup by name of the port (`-setuplog`) with the reference library of the decompilation.
- `check_reach.py`: the swing and the target hint of the player reach each live creature of a level (default BONES), with the `-chartest` commands `goobjh` and `reach`. `--origcombat` gives the rules of 1.3, where no attack reaches a rat (`Dtu_fix_rat_hit`, `docs/re/combat.md`).
- `check_attack_rate.py`: measures the time between the attacks of the player (melee and arrows, with Haste and Slow) and the walk distance, with `-testmove` and `-objlog`. It checks the rules of `docs/re/combat.md` A8, also `Dtu_fix_speed_items`. `--origcombat` gives the rules of 1.3.
- `uitest.py`: interface tests. Each test starts the game hidden at a level, drives it with `-fescript` and checks the screenshots. Needs the game files.

## Reverse engineering

- `dtudis.py`: strings, cross references, callers and disassembly of DTUGAME.EXE (needs capstone in `gamedata/re/pylib`).
- `wiki_rules.py`: write the GitHub wiki pages of the game rules from the level tables and the tables of DTUGAME.EXE. The item and spell tables get an icon column, and the icons go to `OUT_DIR/icons/icon-N.png` (the high resolution icon sheet of FEBGND.DAT, with the game palette). The icons are pictures of the game: the tool refuses to write them into a path of this repository that git does not ignore. Use `gamedata/wiki` as OUT_DIR, or `--no-icons` for pages without icons.

## Content and localization

- `textures_hd.py`: make the upscaled copy of every EGG bitmap. See `docs/hd_textures.md`.
- `ui_hd.py`: make the upscaled interface pictures of FEBGND.DAT.
- `movies_hd.py`: make the upscaled MPEG-1 copies of the movies.
- `music_replace.py`: make an Ogg replacement for a music track (the setting Streaming safe music).
- `music_video.py`: make one video of all the music, for a Content ID check.
- `lang_extract.py`: extract, split, merge and check the text catalogs of the translations. See `docs/formats/lang.md`.
- `ui_lang.py`: redraw the interface pictures that have English words with the words of a language.
- `content_pack.py`: build the downloadable content packs and their manifest. See `docs/install.md`.
- `make_icon.py`: draw the program icon (`installer/res/ascent.ico`).

## Scripts in other places

- `engine/main/cc.sh`: compile files of `engine/main` one at a time with the flags of a configured build, and print their errors and warnings.
- `installer/build.sh`: the release build, the install step and the setup program. See `docs/install.md`.
- `installer/build-linux.sh`: the Linux release build and the tarball with `installer/linux/setup.sh`. See the Linux section of `docs/install.md`.
- `cmake/embed_text.cmake`: turn a translation catalog of the code into a C array.
- `cmake/version.cmake`: write `ascent_version.h` from `git describe`.

Never commit the output of a tool that is derived from the game files.
