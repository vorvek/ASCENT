# Contributing to ASCENT-DTU

ASCENT-DTU is a source port of Descent to Undermountain 1.3 (Interplay, 1997, DOS).
It reads the original game files. It does not include them.

Start with [docs/README.md](docs/README.md) and [docs/architecture.md](docs/architecture.md).

## Game files

- Never commit original game files: executables, levels, archives, art, sound, music, movies, tables or text.
- Never commit output that a tool derives from the game files (extracted bitmaps, upscaled textures, catalogs of the game text, logs with game text).
- `.gitignore` excludes `gamedata/` and the common game file extensions. Check `git status` before each commit.
- Do not write to your game installation. Copy a file to a scratch directory before you change it.

## Facts about the original

- State a fact about a file format or about the behavior of DTUGAME.EXE only after you verify it against the real files or the executable.
- Record each verified file format in `docs/formats/`. Record behavior in `docs/re/`.
- In code, give the address of the original function in a comment (linear address, object 1 at 0x10000). Mark a part that is not verified with "guess".

## The fidelity rule

Version 1.3 has defects. For each rule of the original, decide:

1. Intended behavior: port it as it is.
2. A defect: port the correct behavior as a `Dtu_fix_*` setting that is on by default, and let an `-orig*` option turn it off. See "Corrections of the original" in `docs/architecture.md`.
3. A quirk with no effect for players: do not port it.

Where 1.3 is broken or unclear, follow the rules of AD&D 2nd Edition, so that a tabletop player can predict the result.
Changes that go beyond the original are "Improvements" settings (`engine/main/dtuopts.h`), not `Dtu_fix_*` settings.

## Code

- Read [docs/porting.md](docs/porting.md) before you change engine code.
- C99 with GNU extensions. CMake and Ninja. GCC or Clang (MSYS2 UCRT64 on Windows).
- Mark each changed block of an original file with a comment that starts with `ASCENT:`.
- Keep the line ends (CRLF) and the tabs of the file you edit. Do not reformat code you do not change.
- Only `engine/platform/` calls SDL.
- A new test program is `test_<name>.c` and is registered with `add_test`. A test that needs the game files must skip its data checks when the files are absent, or must stay out of ctest.

Build and test:

```sh
cmake --preset debug
cmake --build --preset debug
cmake --build --preset debug --target ascent
ctest --preset debug
```

## Commits

- One change per commit. Keep commits small.
- Write a short, factual subject in the imperative mood: "Fix the trap timer after a restore", not "Fixed" or "Fixes".
- Add a body when the reason is not obvious. Name the original address or the document that is the evidence.
- No emoji. In documents, do not use em dashes: use a comma, a colon, or two sentences.
- Documents use short, plain sentences in the active voice.

## Licenses

- `engine/` is the Descent 1 source release of Parallax Software. The Parallax license permits use and derivative works only for non-commercial purposes. The text is in `engine/readme.txt`. This project is non-commercial.
- The Descent 2 source release has the same Parallax license.
- DXX-Rebirth is under GPLv3 plus the Parallax license.
- Check the license of a reference before you copy code from it. `docs/porting.md` asks you to write C code from the assembly in this tree, not to copy it from other ports.
- `engine/third_party/` has its own licenses (stb, pl_mpeg). Keep their license files.

## Legal

Descent to Undermountain is a trademark of its owners. This project has no connection with Interplay or with Wizards of the Coast.
