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

- `lvlcheck.py`: compare the level loader of the port (`-checklevel`) with `lvl.py`.
- `objcheck.py`: check the object records of the level files against the level tables (`docs/re/objects.md`).
- `check_creature_setup.py`: compare the creature setup by name of the port (`-setuplog`) with the reference library of the decompilation.
- `uitest.py`: interface tests. Each test starts the game hidden at a level, drives it with `-fescript` and checks the screenshots. Needs the game files.

## Reverse engineering

- `dtudis.py`: strings, cross references, callers and disassembly of DTUGAME.EXE (needs capstone in `gamedata/re/pylib`).
- `wiki_rules.py`: write the GitHub wiki pages of the game rules from the level tables and the tables of DTUGAME.EXE.

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
- `cmake/embed_text.cmake`: turn a translation catalog of the code into a C array.
- `cmake/version.cmake`: write `ascent_version.h` from `git describe`.

Never commit the output of a tool that is derived from the game files.
