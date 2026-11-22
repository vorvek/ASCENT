# Installer and downloadable content

The setup program (`installer/ascent.iss`, Inno Setup 6.5 or later) installs ASCENT-DTU
for one user, makes the data folder from the original game files, gets the files of
the 1.3 patch, and can download the upscaled content.
`installer/build.sh` builds it.

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
only the disc: a CD drive, a copy of the disc, or a disc image. It refuses a folder
without `.COW` files (an installed game). The 1.3 patch files come from the patch
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

- `PATH`: a CD drive, a copy of the disc, its `DTU` folder, or an image (`.cue`,
  `.bin`, `.iso`; 2048 or 2352-byte sectors). A folder without `.COW` files (an
  installed game) is an error. The tool does not search the drives.
- The disc files first, then the patch files. At the end it checks the 9 patch hashes
  and core files.
- Output: `PROGRESS <percent> <text>`, `SOURCE`, `PATCHED yes|no`, `FILES`,
  `INFO`, `ERROR: <text>`. Exit status: 0 done, 1 usage, 2 no game data, 3 patch
  needed, 4 patch not valid, 5 files missing, 6 write error.

## Content packs

`tools/content_pack.py` builds the packs from `gamedata/hd` and the patch into
`gamedata/content` (outside git):

| Pack | Content | Installed files | Stored | Pack size |
|---|---|---|---|---|
| `patch13` | the 9 files of `patch13.txt` | 9 | 9 | 796,417 bytes |
| `textures` | `hd/textures` without `.cache` | 60,396 | 3,896 | 396,695,525 bytes |
| `ui` | `hd/ui` | 39 | 39 | 119,884,683 bytes |
| `movies` | `hd/*.m1v` | 5 | 5 | 485,625,672 bytes |
| `lang_es` | `gamedata/lang/es.catalog` and `*.es.srt` as `hd/lang/...` (only when named: `content_pack.py lang_es`; the catalog must pass `lang_extract.py check`) | 3 | 3 | 401,931 bytes |

The language packs hold the Spanish text of the game files and the subtitles
(`docs/formats/lang.md`); they are made from the translations that
`tools/lang_extract.py merge` writes. The translation of the text of the program is
part of the program.

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
  manifest (`lang_es`: "Spanish text (translation and subtitles)"); the pack `patch13`
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
movies, KB for `lang_es`).

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
the location of the game disc", a CD drive, a copy of the disc, or an `.iso` file or a
`.bin` and `.cue` pair; Setup does not search the drives; Next runs `ascent-import --check` and shows its error), tasks (Start menu entry,
checked; desktop shortcut; and, when the setup program has a server address, the
downloadable content: "Upscaled textures and interface (for the GPU renderer; download,
about 493 MB)", checked; "Upscaled movies (for the GPU renderer; download, about 463
MB)", unchecked because of the size; "Spanish translation of the game text (download,
about 393 KB)", task `langes`, pack `lang_es`, checked only in the Spanish setup
program), ready, the progress page of the patch download and the import (a failure
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
`desktopicon`, `hdtextures`, `hdmovies`, `langes`), `/VERYSILENT`,
`/SUPPRESSMSGBOXES`, `/LOG=`.

The uninstaller (Windows "Installed apps") removes the program, `data`, `hd` and
`content`. It asks before it removes `%APPDATA%\ASCENT-DTU` (saves and settings); the
default answer, also in a silent uninstall, keeps them.
