# Installer and downloadable content

The setup program (`installer/ascent.iss`, Inno Setup 6.5 or later) installs ASCENT-DTU
for one user, makes the data folder from the original game files, gets the files of
the 1.3 patch, and can download the upscaled content.
`installer/build.sh` builds it. On Linux, `setup.sh` of the release tarball does the
same work (see Linux below); `installer/build-linux.sh` builds the tarball.

## Game files that the port needs

Verified against the disc image (ISO 9660, MODE1/2048), the 1.3 install in
`E:\UNDAMONTAN\DTU` and the extracted 1.3 patch.

The `DTU` folder of the disc has 519 files and the subfolder `STUFF` (121 files).
`ascent-import` copies 478 files (about 417 MB) of the 519. It leaves out:

| Left out | Reason |
|---|---|
| `STUFF/` | WAV, PCX, IFF, TGA, BMP files. The game does not read them. |
| `*.EXE`, `*.BAT`, `*.386`, `DOS4GW.EXE` | DOS programs and drivers. |
| `*.BNK`, `*.HMP` | FM sound banks and a MIDI song of the DOS sound drivers. |
| `*.ICO` (but `DTU.ICO`), `*.INI`, `*.CFG`, `*.CFF`, `KCFG.*` | Setup and configuration files of the DOS game. |
| `INSTALL.DAT`, `SETUP.DAT`, `SETUPE.TEX`, `README.TXT`, `DIGITEST.RAW` | Install program, setup program, sound test. |
| `*.LVV` | The 4 `.LVV` files are equal to the `.LV2` files of the 1.3 patch. The port reads `.LV2`. |

It also leaves out the files of the player, if a copy of the disc has them:
`SG*.SG`, `GELDOR.DAT`, `THESTORE.*`, `KCONFIG.DAT`, `*.LOG` and the `SAVE` folder.

The files that the port reads (from the code: `dtu_load_file`, `dtu_find_file`):

| Files | Use |
|---|---|
| `*.DOG` (35), or `*.LVL`, and `*.LV2` | Levels. `.LV2` has priority. `.DOG` is packed `.LVL` (`formats/packed.md`). |
| `*.COW` (57), or `*.EGG` | Bitmaps, sounds, models. |
| `*.HEN` (85), or `*.BIN` | Bitmap tables. |
| `*.TIG`, `*.TAP`, `*.SAY`, `*.BRN` | Triggers, taps, speech table, brain of each level. |
| `*.TUN` (52) | Music. |
| `*.MVE` (6) | Movies (`LOGO22S`, `INTRO2`, `TLOGOS`, `FLAM1`, `CREDITV1`, `IPLOGO`). |
| `FEBGND`, `FESEQ`, `FESTREAM`, `FETXTE`, `WERDZ` (`.DAT` and `.JRG`) | Front end art, sounds, speech, text. |
| `GABE.DAT`, `GABE.RLH` | Dialog texts (English). |
| `DTU.MSN`, `DTU.WRP`, `DISCO.JRG`, `DTU.SNG`, `DTUE.TEX`, `IPHOF.TXT`, `PALETTE.256`, `FNT*.FNT` | Mission, music tables, texts, palette, fonts. |

The import copies `DTU.ICO`, the icon of the game (`formats/misc.md`). The game window
uses it, and the setup program uses it for the shortcuts and the uninstall entry. The
setup program and the port do not contain the icon.

The import also copies the files that the port does not read today but that are game
data: the other languages (`FETXT?.*`, `GAB?.*`, `DTU?.TEX`), `*.EOI`, `*.THN`,
`WHAMMO.*`, `*.TBL`, `FESOUND.JRG`, `HRISBTN.PCH`, `BANE.TXT`, `*.RLH`.

The installed game in `E:\UNDAMONTAN\DTU` has 266 files: it has no `.COW`, `.DOG` or
`.HEN` files and reads them from the disc (`CDPath` in `DTU.CFG`). So the import uses
only the disc: a CD drive, a copy of the disc, a disc image, or a ZIP archive with one
of them (below). It refuses a folder without `.COW` files (an installed game). The 1.3 patch files come from the patch
archive (`--patch`), which the setup program downloads.

## The 1.3 patch

The 1.3 patch has 44 files. Compared with the `DTU` folder of the disc (SHA-256):

- New: `BANE.LV2`, `FLAME.LV2`, `IBBALAR.LV2`, `THIEF2.LV2` (equal to the `.LVV` files of the disc), `SAVEFIX.EXE`.
- Changed: `DISCO.JRG`, `DTU.WRP`, `DTUE.TEX`, `GABE.DAT`, `GABE.RLH`, `DTUGAME.EXE`, `README.TXT`, `UNINST.BAT`.
- Equal to the disc: the 31 other files (fonts, `FETXTE`, level files, `SETUP.EXE`).

The port needs 9 of them: the 4 `.LV2` files and the 5 changed data files.
`installer/patch13.txt` lists them with the SHA-256 and the size. It is the one place
with this list: CMake makes a C table of it for `ascent-import`, and
`tools/content_pack.py` reads it. A source "has the 1.3 patch" when its files have
these 9 hashes (for example the 1.3 install: `DTUGAME.EXE` 1,760,375 bytes).

Where the patch is today:

- Interplay's support page for the game (Wayback Machine, 2000-10-18,
  `http://www.interplay.com/support/dtu/index.html`) links
  `ftp://ftp1.interplay.com/pub/patches/dtu1-3.exe`. The FTP file is not in the Wayback
  Machine. The two archive.org mirrors of `ftp.interplay.com` do not have it.
- The README of the patch names the file `DTU1-3.EXE` (a self-extracting archive).
- The Patches Scrolls (`https://www.patches-scrolls.de/patch/1215/7/22838`, "patch 1.3")
  has `dtu1_3.zip`. The download needs a session cookie: the page
  `download.php?g=Descent%2Bto%2BUndermountain&f=dtu1_3.zip` sets it, then `get.php`
  sends the file. It is not one fixed URL.
- FilePlanet lists only a v1.1 patch and has a browser check.

The downloaded candidate (in `gamedata/patch/ps`, ignored by git):

| | |
|---|---|
| File | `dtu1_3.zip` (ZIP, deflate, 44 entries) |
| Size | 1,891,697 bytes |
| SHA-256 | `43c14d99e323d906301e16e0e6852ac4b8a9fee7f9aab7dd3c0e2e41a17b0182` |
| Check | All 44 files are byte for byte equal to the owner's extracted 1.3 patch folder. |

Decision of the owner: the setup program does not download from these sites. The owner
hosts the 9 files as the pack `patch13` on the content server (below).
`ascent-import --patch` also takes `dtu1_3.zip`, `DTU1-3.EXE` (a ZIP after a program
is found) or an extracted patch folder, and checks each file.

## Runtime of an installed game

`dtu_init_dirs()` (dtufile.c) is the first call of `main`:

- With `-datadir` (development): the data folders are the arguments, and the files of
  the player stay in the current folder, as before.
- Without `-datadir` (installed game): the data folder is `data` next to the executable
  (`SDL_GetBasePath`), and the files of the player go to the folder of the user
  (`SDL_GetPrefPath`, `%APPDATA%\ASCENT-DTU`): `ascent.cfg`, `SG<n>.SG`, the `SAVE`
  folder of the level states, `GELDOR.DAT` and `THESTORE.<n>`. The names of these
  files are found with any case of the letters (`dtu_user_find`, dtustate.c). If the data folder has
  no `DTU.MSN`, a message box says so. `Error()` shows a message box too.
- The settings: first `default.cfg` next to the executable (the defaults of the
  installation, same `name=value` format as `ascent.cfg`, written only by the setup
  program), then `ascent.cfg` of the user, then the arguments (`-language`, ...). A
  setting in `ascent.cfg` wins over `default.cfg`, so the default never changes a
  choice of the player. A `-datadir` run does not read `default.cfg` (dtuopts.c).
- `-userdir DIR` changes the folder of the user.
- The session logs go to `logs` in the folder of the user (docs/debug_logs.md).
- `dtu_user_path()` gives the path of a file of the player. dtuopts.c, dtufe.c (save
  and load), dtustate.c and dtufe_town.c (market) use it.
- The upscaled files: an installed game uses only `hd` next to the executable (or
  `-hddir`), not the `hd` folders of the current folder and its parents.
- `-debug` (and every build without `ASCENT_RELEASE`) prints the developer messages:
  `MEM_LEAKAGE` (`mem_report_leaks`), "Loading picture", the MUSIC summary.

## Save games of the DOS game 1.3

The save games of the port have the format of DTUGAME.EXE 1.3 (docs/formats/misc.md,
docs/re/levelstate.md), so they go both ways. To go on with a game of the DOS version:

1. Copy from the DOS game folder (for example `C:\DTU`) into `%APPDATA%\ASCENT-DTU`:
   the files `SG0.SG` to `SG9.SG` and `THESTORE.1` to `THESTORE.10`.
2. Copy the folder `SAVE` (the files `SGn.LS`, `SGn_m.LVL`, `SGn_m.TIG`,
   `SGn_m.TAP`) to `%APPDATA%\ASCENT-DTU\SAVE`. Do not copy `CSUF.LS` and the
   `TEMP*` files: they belong to a game that runs.
3. Start the port and choose "Load Game...".

Slot n of the menu is `SGn.SG` with the level files `SAVE\SG<n+1>*.*` and the market
stock `THESTORE.<n+1>`: `SG0.SG` goes with `SAVE\SG1.LS` and `SAVE\SG1_0.LVL`, and so
on. Copy the files of a slot together. Upper and lower case names both work. A save
of the port copies back to the DOS game the same way. The saves of earlier builds of
the port still load.

Screenshots of `save_screen_shot` (game.c) still go to the current folder. The
shortcuts start the game in the installation folder, which is writable for a per-user
install.

## Release build

```sh
cmake --preset release            # build-rel, Release, ASCENT_RELEASE=ON
cmake --build --preset release    # ascent, ascent-import, ascent-content
cmake --install build-rel --prefix build-rel/stage
```

- `ASCENT_RELEASE`: no developer messages without `-debug`, Windows subsystem (no
  console). The session log and the crash report work as in the development build
  (docs/debug_logs.md); the crash shows a message box with the files to send.
- The game is compiled with `-g`. `build-rel/ascent-symbols.exe` and
  `build-rel/ascent.map` keep the symbols for the crash addresses; the debug
  information is then removed from `ascent.exe` (docs/debug_logs.md). Keep the two
  files of each release. `installer/build.sh` passes its version as `ASCENT_VERSION`
  (the version line of the log). The asserts stay (`-DNDEBUG` is removed): the original code has parts that
  exist only without `NDEBUG` (`fixed_frametime`).
- The executables have the icon `installer/res/ascent.ico` (original art, made by
  `tools/make_icon.py`) and a manifest with the UTF-8 code page, so that paths with
  non-ASCII letters work with `fopen`.
- The install rule copies `SDL3.dll` and `libwinpthread-1.dll` (found with
  `RUNTIME_DEPENDENCIES`), `content.ini` and the license texts. The tools link the
  MinGW runtime statically.

## ascent-import

```
ascent-import --source PATH --dest DIR [--patch PATH] [--check]
```

- `PATH`: a CD drive, a copy of the disc, its `DTU` folder, an image (`.cue`,
  `.bin`, `.iso`; 2048 or 2352-byte sectors), or a `.zip` archive with the disc in it
  (below). A folder without `.COW` files (an installed game) is an error. The tool
  does not search the drives.
- Before the copy it checks the free space of `DIR` (the size of the files and 1 MB):
  "Not enough free space in DIR: N MB needed, M MB free.", status 6.
- The disc files first, then the patch files. At the end it checks the 9 patch hashes
  and core files.
- Output: `PROGRESS <percent> <text>`, `SOURCE`, `PATCHED yes|no`, `FILES`,
  `INFO`, `ERROR: <text>`. Exit status: 0 done, 1 usage, 2 no game data, 3 patch
  needed, 4 patch not valid, 5 files missing, 6 write error.

### A ZIP archive as the source

Many players have the game as the ZIP archive of archive.org. Verified against this
archive:

| | |
|---|---|
| File | `Descent_to_Undermountain_1998.zip`, 583,422,066 bytes, 263 entries (688,334,872 bytes unpacked), all deflate (method 8), no ZIP64, comment `TORRENTZIPPED-EE24C92E` |
| SHA-256 | `1ce0a6240f164b184dde27e63e542eac0afde4b907158216919b4cf87d731ca9` |
| `desund/` | 258 files: an installed game (no `.COW`, `.DOG` or `.HEN` files), and `desund/SAVE/CSUF.LS` |
| `desund/cd/dtu.iso` | the disc image: ISO 9660, 2048-byte sectors, 478,353,408 bytes (399,826,303 compressed), SHA-256 `b4456bf69976171ad5c85b1f410cd97e7b4b03aec94ad928a4807ca4a06a28e9` |
| `dosbox.conf` | the DOSBox settings |

`ascent-import --source FILE.zip` (and so both setup programs) finds the disc in the
archive, at any depth:

1. A disc image: an `.iso` or `.img` entry, a `.cue` entry with the `.bin` that it
   names (relative to the folder of the `.cue`, else any entry with that file name),
   or a `.bin` entry of 16 MB or more (a smaller `.BIN` is a bitmap table of the
   game). The first image with `DTU\DTU.MSN` and `.COW` files is the source.
2. Else a copy of the disc: the folder with `DTU.MSN` and the most `.COW` files.
3. Else an error. A folder with `DTU.MSN` and no `.COW` files is an installed game:
   "The ZIP archive ... is not the game disc: it has an installed game (...) but no
   disc image and no copy of the disc." Without `DTU.MSN`: "The ZIP archive ... has
   no Descent to Undermountain disc (no disc image and no DTU.MSN)." A file that is
   not a ZIP archive: "... is not a ZIP archive, or it is damaged."

So the archive of archive.org gives the image `desund/cd/dtu.iso`, not the installed
game next to it. The image is not extracted: the tool reads it from the archive as a
stream, so the install needs no temporary file and no extra free space. The
decoder (`inst.c`, inflate after RFC 1951, written for ASCENT) keeps the last 32 KB
and decodes forward; a read before the current position starts again at the start
of the entry. The tool copies the files of an image in a ZIP archive in the order of
the image (their sectors), so that it decodes the entry once; at the end it decodes
the rest of the entry and checks its CRC-32 ("The disc image ... is damaged (CRC
error).", status 2). `--check` reads only the start of the image (the directories),
so it takes a fraction of a second. The import of the archive.org archive took about 3
seconds on the development computer (Windows, release build).

The ZIP reader takes the methods 0 (stored) and 8 (deflate), ZIP64 (the end record
and the extra field of the entries, also when an archive has the ZIP64 records but
does not need them) and a self-extracting program before the archive. It refuses an
encrypted entry. `ascent-content` (the content packs) and `--patch` use the same
reader. The test `test_inst` (ctest) checks it with the archive of
`installer/test/make_test_zip.py`.

Verified: the import of the archive.org archive and of the image extracted from it
(Python `zipfile`) give the same 483 files with the same SHA-256. So do a ZIP64
archive with a copy of the disc in a folder (`Some Game/DTU/...`) and a ZIP64
archive with a `.cue` and a MODE1/2352 `.bin` made from the image, both made with
Python. A changed byte in the compressed image is found (CRC error). The Linux build of
`ascent-import` (WSL, Debian 13) gives the same files from the archive.org archive.
`ascent-content` with the new reader installed and verified the packs `lang_es`,
`music` and `textures` (60,396 files) from a local test server. Not tested: the two
setup programs themselves with a ZIP archive (only `ascent-import`, which they run).

## Linux

### Install the release

The Linux release is `ascent-dtu-<version>-linux-x86_64.tar.gz`. It needs an x86_64
system with glibc 2.34 or later (for example Ubuntu 22.04, Debian 12, Fedora 35) and
X11 or Wayland. For an older system, build the game from the source (see below).

The Linux build is new. The build, the tests of `ctest` and the release script were
also run in containers of Ubuntu 20.04 and Ubuntu 22.04. The other tests were in WSL
(Debian 13) only: the build of the
tarball, `setup.sh` with a copy of the disc (user and root installs, the real content
server and a local test server, stopped and damaged downloads, the removal), and 200
frames of a level with `-hidden` and the SDL dummy video and audio drivers, in English
and Spanish. Not tested: a real desktop (the menu entry, a window, the GPU renderer
with OpenGL), audio output, keyboard, mouse and gamepad input, a real CD drive, and a
disc image as the source.

Unpack it and run the setup in its folder:

```sh
tar xzf ascent-dtu-1.0-linux-x86_64.tar.gz
cd ascent-dtu-1.0-linux-x86_64
./setup.sh
```

The setup needs a POSIX shell, coreutils, curl or wget, and unzip. It asks, in this
order:

1. The installation folder. The default is `~/.local/share/ASCENT-DTU/game`. Run as
   root, the default is `/opt/ascent-dtu` (an install for all users). The folder must
   be new, empty, or an earlier installation (then the setup updates it).
2. The game disc: a mounted CD, a copy of the disc in a folder, an image (`.iso`,
   or `.cue` with `.bin`), or a `.zip` archive with one of them (read without
   unpacking). `ascent-import --check` examines it. An installed game is refused,
   also in a ZIP archive, as in the Windows setup program.
3. The language of the game: `en` or `es`.
4. The downloads, with the sizes of `manifest.txt`: the upscaled textures and
   interface (packs `textures` and `ui`, default yes), the upscaled movies (default
   no), the Spanish text (`lang_es`; always with the language `es`), the replacement
   music (`music`, default yes).

Then it shows a summary and the free space, and asks once more. The steps:

- It copies the program (`ascent`, `ascent-import`, `content.ini` with the server
  address that it used, the license texts, `setup.sh` for the removal).
- It downloads `manifest.txt` and the pack `patch13`, checks the size and the SHA-256,
  and runs `ascent-import --source ... --dest PREFIX/data --patch ...`. A failure stops
  the setup and removes `data`.
- It installs the chosen packs: download to `content/download` (a `.part` file; a
  stopped download goes on with a Range request at the next run), check of the size
  and the SHA-256, unpack into `content/staging-<pack>`, check of each file against
  `_files.txt`, the links of `_links.txt` (hard links, else copies), then the move
  into `hd` and the state file `content/installed/<pack>.txt` (the format of
  `ascent-content`). A damaged pack is deleted. A pack that fails is a warning: the
  game works without it.
- With the language `es` and `lang_es` installed it writes `default.cfg` with
  `Language=es`, as the Spanish Windows setup does. The player's `ascent.cfg` is not
  changed, so a language chosen in the game stays.
- The icon: `ascent-dtu.png` from `data/DTU.ICO` of the disc, made with `icotool`
  (package `icoutils`), `magick` or `convert` (ImageMagick). Without one of them the
  setup says so and makes no icon.
- The launcher `PREFIX/ascent-dtu` starts the game in `~/.local/share/ASCENT-DTU`
  (the folder of the player, where the screenshots then go). The setup links it as
  `~/.local/bin/ascent-dtu` (root: `/usr/local/bin`) and says when that folder is not
  in `PATH`. The menu entry is `ascent-dtu.desktop` in
  `~/.local/share/applications` (root: `/usr/local/share/applications`).
- `install-record.txt` lists each file and folder that the setup made, and
  `setup.log` keeps the output of the tools.

Each question is also an option, for an install without questions:

| Option | Answer |
|---|---|
| `--prefix DIR` | the installation folder |
| `--source PATH` | the game disc |
| `--lang en\|es` | the language |
| `--textures yes\|no`, `--movies yes\|no`, `--spanish yes\|no`, `--music yes\|no` | the downloads |
| `--yes` | the default of each answer that is not given |
| `--url URL` | another content server (as `/CONTENTURL=`; also `ASCENT_CONTENT_URL`; `file://` works with curl) |
| `--patch PATH` | the 1.3 patch archive or folder, instead of the download |

```sh
./setup.sh --yes --source /media/cdrom --lang en --textures no
```

`PREFIX/setup.sh --uninstall` removes the entries of `install-record.txt` (the
program, `data`, `hd`, `content`, the launcher, the menu entry, and the folders that
the setup made if they are empty). It asks first; `--yes` does not ask. The saves and
settings in `~/.local/share/ASCENT-DTU` stay; `--purge` removes them too.

There is no `ascent-content` for Linux: it uses WinHTTP and a Windows window. The
setup does its work with curl or wget, unzip and sha256sum, so it needs no second
program. To add a pack later, run the setup again with the same folder.

### Build the release

```sh
installer/build-linux.sh
```

It configures `build-rel-linux` (Release, `ASCENT_RELEASE=ON`), builds `ascent` and
`ascent-import`, stages the files and packs
`build-rel-linux/ascent-dtu-<version>-linux-x86_64.tar.gz`. The variables:
`ASCENT_VERSION` (default 1.0), `ASCENT_BUILD_DIR`, and `SDL3_SOURCE_DIR` (an SDL
checkout of the tag in `CMakeLists.txt`, to build without the download).

- SDL3 is built from the source and linked statically
  (`CMAKE_DISABLE_FIND_PACKAGE_SDL3=ON`, so the FetchContent path of `CMakeLists.txt`
  is used even when the system has SDL3). Ubuntu 24.04 has no SDL3 package, and the
  static SDL3 makes the program independent of the SDL3 of the system. It loads X11, Wayland, ALSA, PulseAudio and PipeWire with
  `dlopen` when it starts, so the program needs only the libraries of glibc (`libc`
  and `libm`; before glibc 2.34 also `libdl` and `libpthread`). The script
  stops if `ascent` links a shared SDL3.
- The glibc of the build system is the oldest glibc that the release runs on. The CI
  job builds on Ubuntu 22.04 (glibc 2.35), and the tarball then needs glibc 2.34. A
  tarball that is built on a newer system needs a newer glibc: a build on Debian 13
  needs glibc 2.38. Do not use a system older than Ubuntu 22.04 for the release: it
  has no libdecor or no PipeWire 0.3 package new enough for SDL3, and SDL3 is then
  built without them.
- The build needs the packages of the Linux job of `.github/workflows/build.yml`.
- The tarball has `ascent`, `ascent-import`, `setup.sh`, `content.ini`, `README.txt`
  (`installer/linux/README.txt`), `LICENSE-PARALLAX.txt` and `LICENSE-SDL3.txt`. It has
  no game file and no original art: the icon is made from the disc at install time.
- The CI job `linux` (GCC) runs the script and keeps the tarball as the build
  artifact `ascent-dtu-linux-x86_64`. It also runs `shellcheck` on `setup.sh`.

### Build from the source

```sh
cmake --preset debug               # downloads and builds SDL3 if it is not installed
cmake --build --preset debug --target ascent ascent-import
```

The build needs CMake 3.16 or later, a C compiler (GCC 9 or later is tested) and
Ninja or Make. The oldest system that the CI builds on is Ubuntu 20.04 (CMake 3.16,
GCC 9, glibc 2.31; the job `linux-old`). The presets need CMake 3.20. With an older
CMake, give the options:

```sh
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --target ascent ascent-import
```

The SDL3 build needs the development packages that `.github/workflows/build.yml`
installs (Ubuntu names). SDL3 is built without each part that has no package or a
package that is too old: on Ubuntu 20.04, without libdecor (the window frame on
Wayland desktops that do not draw one) and without PipeWire (the sound then goes
through PulseAudio or ALSA). Make the data folder with `ascent-import` from the disc and the
1.3 patch, then start the game with the folder:

```sh
build-debug/installer/ascent-import --source /path/to/disc --patch /path/to/patch.zip --dest ~/dtu-data
build-debug/engine/main/ascent -datadir ~/dtu-data
```

Without `-datadir` the data folder is `data` next to the executable, and the files of
the player go to `~/.local/share/ASCENT-DTU` (`$XDG_DATA_HOME/ASCENT-DTU`). The game
files keep their upper case names: the game finds them with any case.

## Content packs

`tools/content_pack.py` builds the packs from `gamedata/hd` and the patch into
`gamedata/content` (outside git):

| Pack | Content | Installed files | Stored | Pack size |
|---|---|---|---|---|
| `patch13` | the 9 files of `patch13.txt` | 9 | 9 | 796,417 bytes |
| `textures` | `hd/textures` without `.cache` | 60,396 | 3,896 | 396,695,525 bytes |
| `ui` | `hd/ui` | 39 | 39 | 119,884,683 bytes |
| `movies` | `hd/*.m1v` | 5 | 5 | 485,625,672 bytes |
| `lang_es` (version 4) | `gamedata/lang/es.catalog` and `*.es.srt` as `hd/lang/...`, and the pictures of the language `hd/lang/es/` (`tools/ui_lang.py`) (only when named: `content_pack.py lang_es`; the catalog must pass `lang_extract.py check`) | 46 | 45 | 3,502,119 bytes |
| `music` | `hd/music`: the replacement music `STARTPUB.ogg` (`engine/main/dtumusic.c`), `README.txt` and the notes `STARTPUB.txt` of `tools/music_replace.py` (only when named: `content_pack.py music`; see below) | 3 | 3 | 2,332,194 bytes |
| `models` | `hd/polymodels`: the improved character and monster models `*.pom` (`tools/model_zoo.py build`, [formats/pom.md](formats/pom.md)) (only when named: `content_pack.py models`) | 851 | 103 | about 0.8 MB |

The language packs hold the Spanish text of the game files, the subtitles and the
interface pictures with Spanish words (`docs/formats/lang.md`); they are made from the translations that
`tools/lang_extract.py merge` writes. The translation of the text of the program is
part of the program.

The pack `models` holds the cleaned character and monster models. The Windows setup program (task `models`) and `setup.sh` (`--models`) offer it, checked by default because it is small. The game uses the files with the GPU renderer and the setting "Improved models".

The pack `music` holds the replacement music that the setting "Streaming safe music"
(`SafeMusic` in `ascent.cfg`, off by default) plays in place of the original tracks.
The files are not in the repository, the setup program or the Linux tarball: they
are only on the content server. The Windows setup program (task `music`) and
`setup.sh` (`--music`) offer the pack, checked by default because it is small; the
setting stays off. Build it into a folder of its own, so that a sync of
`gamedata/content` does not upload a changed pack by mistake: `content_pack.py --out
gamedata/content-pending music` (copy `manifest.txt` there first, so that the new
manifest keeps the other packs). Without the pack the setting has no effect.

A pack is a ZIP file (PNG and M1V stored, text deflated). It has:

- `_pack.txt`: `name=`, `version=`, `target=` (`hd` or `data`), `title=`.
- `_files.txt`: one line for each installed file: `<sha256> <size> <path>`.
- `_links.txt`: `<path>\t<path of the stored copy>`. Many texture files have the same
  content (hard links in `gamedata/hd`): the pack stores each one once, and
  `ascent-content` makes the others as hard links (else copies).

`manifest.txt`: lines starting with `#` are comments; each other line has 8 fields
separated by tabs: name, version, target, file, size, sha256, files, title.

## The content server

`installer/content.ini` is the one place with the server address (`base_url`,
`https://ascent-content.generalsimulationworks.com/`, a Cloudflare Worker in front of
an R2 bucket, `installer/server/worker.js`), the header (`X-Ascent-Client: ascent-setup-1`) and the User-Agent
(`ASCENT-Setup/1.0`) that the client sends. The setup program reads it at compile time
and writes the address it used into the installed `content.ini`. A test gives another
address: `/CONTENTURL=` for the setup program, `--url` for `ascent-content`.

The header and the User-Agent are a deterrent, not a protection: anyone can read them
from the program or the traffic and send them. The server can only make casual use
harder and limit the traffic.

Proposed nginx configuration (not deployed):

```nginx
# /etc/nginx/conf.d/ascent.conf
limit_req_zone  $binary_remote_addr zone=ascent_req:10m  rate=20r/m;
limit_conn_zone $binary_remote_addr zone=ascent_conn:10m;

map $http_x_ascent_client $ascent_client_ok {
    default            0;
    "ascent-setup-1"   1;
}
map $http_user_agent $ascent_agent_ok {
    default            0;
    "~^ASCENT-Setup/"  1;
}

server {
    listen 443 ssl;
    http2 on;
    server_name content.example.org;            # the name of the server
    ssl_certificate     /etc/letsencrypt/live/content.example.org/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/content.example.org/privkey.pem;

    location /ascent/ {
        if ($ascent_client_ok = 0) { return 404; }
        if ($ascent_agent_ok = 0)  { return 404; }
        limit_req  zone=ascent_req burst=10 nodelay;
        limit_conn ascent_conn 2;               # two downloads at a time from one address
        alias /srv/ascent/content/;             # manifest.txt and the *.zip packs
        autoindex off;
        types { text/plain txt; application/zip zip; }
        location = /ascent/manifest.txt {
            if ($ascent_client_ok = 0) { return 404; }
            if ($ascent_agent_ok = 0)  { return 404; }
            alias /srv/ascent/content/manifest.txt;
            add_header Cache-Control "no-cache";
        }
    }
    location / { return 404; }
}
```

`base_url` is then `https://content.example.org/ascent/`. Copy the folder with
`rsync -av gamedata/content/ server:/srv/ascent/content/`. nginx has no daily byte
limit per address; a provider budget alert on the egress of the VM covers the cost.

## ascent-content

The content tool is installed with the game ("ASCENT-DTU Content" in the Start menu).

- Without a command it opens a window: the list of the packs of the server and of the
  installed packs (version installed, version available, size, status), and the buttons
  Install, Remove, Check / repair, Refresh. It works without the server for the check
  and the removal of installed packs. The names in the list are the titles of the
  manifest (`lang_es`: "Spanish text and pictures (translation, subtitles, interface pictures)"); the pack `patch13`
  (target `data`) is not listed. The window is in English only.
- Commands: `list`, `download PACK FILE`, `install PACK...`, `remove PACK...`,
  `verify PACK...`, `repair PACK...`, with `--root DIR` and `--url URL`.
- A download is checked against the size and the SHA-256 of the manifest. After an
  install, each file is checked against `_files.txt`. The state of an installed pack
  is `content/installed/<name>.txt` (version and file list).
- Exit status: 0 done, 1 usage, 2 download failed, 3 no such pack or no server
  address, 4 SHA-256 mismatch, 5 write error, 7 the pack is damaged.

## The setup program

`installer/build.sh [CONTENT_DIR]` builds `build-rel/setup/ascent-dtu-setup-<version>.exe`
(the download sizes come from `CONTENT_DIR/manifest.txt`: MB for the textures and the
movies, KB for `lang_es` and `music`).

The setup program is in English, Spanish, German, French and Italian. It starts with
the language dialog (`ShowLanguageDialog=yes`; the language of Windows is preselected).
The standard texts come from the `.isl` files of Inno Setup (`Default.isl`,
`Languages\Spanish.isl`, `German.isl`, `French.isl`, `Italian.isl`); the texts of the
script are in `[CustomMessages]` of `ascent.iss`, one block for each language with the
same names. A new language needs a `[Languages]` line and a full block. The language
changes only the setup program and the uninstaller; the "ASCENT-DTU Content" shortcut
has a translated name.

The pages: language, installation folder (default `%LOCALAPPDATA%\Programs\ASCENT`, no
administrator rights), game disc (one field with Folder and Image buttons: "Select
the location of the game disc", a CD drive, a copy of the disc, an `.iso` file or a
`.bin` and `.cue` pair, or a `.zip` archive with the disc or a disc image in it; the
Image button shows `*.cue;*.bin;*.iso;*.zip`; Setup does not search the drives; Next runs `ascent-import --check` and shows its error), tasks (Start menu entry,
checked; desktop shortcut; and, when the setup program has a server address, the
downloadable content: "Upscaled textures and interface (for the GPU renderer; download,
about 493 MB)", checked; "Upscaled movies (for the GPU renderer; download, about 463
MB)", unchecked because of the size; "Spanish translation of the game text (download,
about 2775 KB)", task `langes`, pack `lang_es`, checked only in the Spanish setup
program; "Replacement music for the setting 'Streaming safe music' (download, about
2278 KB)" (the text has double quotes), task `music`, pack `music`, checked in every language), ready, the progress page of the patch download and the import (a failure
stops Setup with the message of the tool), the content downloads (a failure is a
warning: the game works without them), and the last page with "Play ASCENT-DTU".

The default language of the game: when the setup program is in Spanish and `lang_es`
is installed, Setup writes `default.cfg` with the line `Language=es` into the
installation folder. The game reads it before `ascent.cfg` (Runtime, above): a new
player starts in Spanish, and a player whose `ascent.cfg` has a `Language=` line keeps
that language. When the player changes a setting, the game writes `ascent.cfg` with all
the settings, `Language=` included; from then on the language is the player's own. In the other languages Setup does not
write `default.cfg` (the language can be chosen in the options menu of the game). The
uninstaller removes `default.cfg`.

Command line: `/SOURCE=`, `/PATCH=`, `/CONTENTURL=`, and `/LANG=` (`english`,
`spanish`, `german`, `french`, `italian`), `/DIR=`, `/TASKS=` (`startmenu`,
`desktopicon`, `hdtextures`, `hdmovies`, `langes`, `music`), `/VERYSILENT`,
`/SUPPRESSMSGBOXES`, `/LOG=`.

The uninstaller (Windows "Installed apps") removes the program, `data`, `hd` and
`content`. It asks before it removes `%APPDATA%\ASCENT-DTU` (saves and settings); the
default answer, also in a silent uninstall, keeps them.
