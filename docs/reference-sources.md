# Source files that the build does not use

The Descent 1 source release has files that the port does not build. The
files below stay in the tree because they are original Parallax source:
they show how Descent 1 did a thing that DTUGAME.EXE inherited, or they are
the reference for a C replacement. No `CMakeLists.txt` compiles them and no
built file includes them. Do not add them to the build without a port
review (`docs/porting.md`).

The port removed the files that were provably dead: old copies of the
headers (`engine/lib/`), stale copies and old revisions of built files,
empty stubs, generated model data, the DOS build scripts, the editor data
files, the DOS art tools and the DOS test programs. `git log --diff-filter=D`
lists them.

## Assembly

Each `.asm` file is the reference for the C file of the same name that
replaces it (`docs/porting.md`, "Assembly"). The C code must give the same
results bit for bit.

| Directory | Files |
|---|---|
| `engine/texmap` | 28 |
| `engine/3d` | 11 |
| `engine/bios` | 10 |
| `engine/2d` | 6 |
| `engine/main` | 5 (`cockpit.asm`, `int3hand.asm`, `ipx_esr.asm`, `soscodec.asm`, `test.asm`) |
| `engine/div` | 2 |
| `engine/fix`, `engine/vecmat` | 1 each |

`engine/includes/*.inc` are the assembler includes of these files.

## Original makefiles

`engine/*/makefile` and `engine/main/editor/makefile` are the Watcom
makefiles of the release. The `CMakeLists.txt` files take their source
lists from them (for example `MAIN_SOURCES` is `DESCENT_OBJS`). They are not
run.

## engine/main

| File | Content |
|---|---|
| `multi.c`, `multibot.c`, `network.c`, `netmisc.c`, `netware.c` | Multiplayer and IPX network play. DTU 1.3 has no multiplayer. |
| `modem.c`, `serial.c`, `nocomlib.c`, `ourcom.c` | Modem and serial play. |
| `vfx.c`, `victor.c`, `iglasses.c` | Virtual reality helmets. |
| `demo.c`, `demorecs.c` | The old demo recorder. `newdemo.c` is built. |
| `cdrom.c` | CD-ROM drive access. `dtufile.c` replaces it. |
| `digicomp.c` | Sound sample tools. |
| `dumpmine.c` | Text dump of a mine. |
| `flyby.c`, `objfly.c` | Fly-through of a mine and of objects. |
| `photos.c`, `objvecs.c` | Multiple-view pictures of an object. |
| `mcockpit.c` | Generator of `cockpit.inc`. |
| `nfile.c` | Reads from a big file. |
| `objphys.c`, `ship.c`, `radar.c`, `garage.c` | Early object physics, ship values, radar and garage code. |

The `arcade.h` and `coindev.h` headers of the arcade version stay, because
built files include them. Their `.c` files are removed.

## engine/main/editor and engine/ui

The Descent 1 level editor (`med`) and its interface library. DTU levels
have a different format (`docs/formats/lvl.md`), so the port does not build
the editor. The code documents the Descent 1 segment, wall, trigger and
object data that the DTU level format extends.

## Other libraries

The `CMakeLists.txt` file of each library names the unbuilt files of its
directory in a comment (DOS test programs, old versions, tools). The table
lists the others.

| File | Content |
|---|---|
| `engine/2d/cursor.c`, `fdraw.c`, `tpoly.c`, `vesalib.c`, `tmapbits.c` | Cursor, polygon and VESA code that the original library did not use, and a sample bitmap. `engine/2d/CMakeLists.txt` lists the other unbuilt files of the directory. |
| `engine/bios/speaker.c` | PC speaker. |
| `engine/texmap/tmap.c`, `tmapv.c`, `tmapz.c`, `tmapsky.c`, `scanline.c`, `quad.c` | Older and experimental texture mappers. `engine/texmap/CMakeLists.txt` lists the built ones. |
| `engine/pslib/*.c` | The old library file system with LZW compression. `engine/pslib/CMakeLists.txt` explains why it is not built. |
| `engine/cfile/pscomp.c` | A DOS test program for the old compressed files. |
