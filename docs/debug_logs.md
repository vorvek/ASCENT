# Debug logs

Each run of the game writes a session log. When something goes wrong, the log (and,
after a crash, the crash dump) is what a developer needs to look into it.

## Where the logs are

- Installed game: `%APPDATA%\ASCENT-DTU\logs` (paste this into the address bar of the
  Explorer).
- Development run: the `logs` folder of the `-userdir` folder, or of the current folder
  when there is no `-userdir`.

The name is the start date and time, for example `ascent-2026-11-22_14-05-33.log`. The
game keeps the last 20 logs and deletes the older ones with their crash dumps. The log
is never written into the installation folder. `-nolog` turns the log off.

## What to send

After a problem, send the newest `ascent-*.log`. After a crash, send also the `.dmp`
file with the same name: a message box names the two files (the release build, not in a
test run). If the game stopped with no message, the next start writes a note in its log
that the previous session did not end cleanly; send the log of the previous session
too.

## What the log has

- The version (`ASCENT_VERSION` of the build and `git describe`), the build type, the
  compiler, the command line.
- The system: Windows version, CPU, memory, SDL version.
- The folders: installation, user, data folders (the one with `DTU.MSN` is marked), `hd`.
- The settings of `ascent.cfg` (Improvements and Graphics), after the arguments.
- The video driver, each display, the window, the OpenGL vendor, renderer and version.
  If the OpenGL 3.3 window cannot be made, the reason (the SDL renderer is used then).
- The events of the game: game start, each level load (name, time, segments, objects),
  the character at each level start (name, race, classes and levels, hit points,
  experience, gold), level finished, death.
- Each creature death: `Creature death: obj N NAME, cause ..., attacker ..., experience X,
  segment S` and the numbers of the last roll, if any. The cause is the weapon, a spell, a
  missile with no source (a trap), a creature missile, a script kill (0xA6ACB), the end of a
  script run (0xAC033), Turn Undead or Abjure. A troll that falls and can get up:
  `Creature falls: ...`.
- Each creature object that goes away with no death of the damage function:
  `Creature removed: obj N NAME, cause ..., hit points H, segment S`. The cause is a
  script (the object leaves the level), Abjure, a level fix, a squashed rat with no free
  segment, a gated creature with no AI, the end of the Fear view or the end of its
  lifeleft. `not named` is a removal by code that does not name its cause. A rat that the
  player squashes with boots and that jumps to another segment: `Creature squashed as a
  rat: ...`. A Descent 1 rule that the port stops for a creature (the explosion, the
  damage of `apply_damage_to_robot`): `Creature: stopped ...` (at most 100 lines).
- Each award of experience: `Experience: +TOTAL, +PART for each class, now X / Y`, and each
  level gained: `Level gained: class levels A / B -> C / D, N hit points`.
- Each voice line of Khelben: `Front end: speech N starts at T ms, S samples at R Hz`, `Front
  end: speech N waits at T ms for speech M` (the line is queued while line M plays) and
  `Front end: speech N ends at T ms, after D ms`.
- Each talk of a thief with Mirt in the inn: `Thief points: Mirt, thief level L, levels paid
  before P, N points to spend`, and after the skill point screen `Thief points: spent, hide
  H, move M, open locks O, traps T, climb C, N points left` (the values without items).
- Each saving throw of the player: `Save of the player: CAUSE (SOURCE), CATEGORY: d20 R,
  needs N (table T, +B (protection), +C (CON), +W (WIS)): made` (or `failed`). The cause
  is a spell (the source is its caster), Chill Touch (the attacker), or the blindness or
  deafness of the fire (Fireball, the fire trap). N is the number of the class table T
  minus each bonus; a bonus of 0 is left out. B: the best Ring, Cloak or Amulet of
  Protection (`Dtu_fix_prot_saves`). C: the Constitution bonus of a dwarf or halfling, W:
  the Wisdom adjustment against a spell that attacks the mind (`Dtu_fix_ability_saves`);
  CON and WIS are the abbreviations of the statistics screen in the language (SAB in
  Spanish). With the verbose combat log on, the message area also shows
  `Save d20 R vs N: made.`.
- An object that the restore of a level state kills: after the 1.3 line `STATE: killing
  object N -- illegal segment`, the line `State: object N is an effect (type 1 fireball,
  vclip V, ...)` or `State: object N type T id I NAME has the illegal segment S`.
- The cause of each level change: `Level change: DTU.WRP line ...` (the warp line, its
  source and destination segments), `Level change: script command teleport PC ...`, or
  the exit of the inn to Waterdeep. A direct start (`-level NAME`) logs its start
  segment (`Level start: segment ...`, see `docs/audit/world.md`, W20).
- The input lines (for a report of keys or a mouse that do nothing):
  - At the start, after the video lines: `Input: SDL compiled X, linked Y`, `Input:
    environment NAME=VALUE` (XDG_SESSION_TYPE, XDG_CURRENT_DESKTOP, DISPLAY,
    WAYLAND_DISPLAY and others), `Input: hint NAME=VALUE` (the SDL hints of the mouse and
    the keyboard), on Linux `Input: libXi.so.6 loads`, `Input: X server 'VENDOR', release
    N` and `Input: X extension XInputExtension present`, `Input: display N bounds ...`,
    `Input: window flags 0x..., position, size, display scale, pixel density`, and each
    keyboard and mouse that SDL reports (`Input: keyboard ID 'NAME'`, `Input: mouse ID
    'NAME'`). A device added or removed later: `Input: keyboard ID added ('NAME')`.
  - `Input: gamepad 'NAME' (vendor V product P), axes LX LY RX RY LT RT` and `Input:
    joystick 'NAME' ...` when one is opened. A gamepad axis away from 0 at the open counts
    only after `Input: gamepad axis N at rest`.
  - Each change of the mouse mode: `Input: relative mouse mode on: ok (SDL reports 1)` or
    the error of SDL, `Input: mouse grab on: ...`, `Input: the relative mode was off with
    the focus: set again`, and the first 20 warps of the pointer to the window center
    (`Input: warp of the pointer from X,Y to the center ...`).
  - With `-inputlog` only: the window events: `Input window: focus gained` (focus lost, mouse enter, mouse leave,
    enter full screen, leave full screen, resized, moved), with the relative mode and the
    grab, at most 300 lines.
  - With `-inputlog` only: the first 200 key events (`Input key down: scancode N (NAME), key 0x..., mod 0x...,
    repeat R, keyboard ID, game code 0x..`), the first 200 mouse motions (`Input motion:
    mouse ID, x, y, xrel, yrel, relative R`) and the first 200 mouse buttons. Then one line
    for each second with input: `Input second: keys N down (R repeats) N up (keyboard ID),
    mouse N motions (Z with no relative motion) sum xrel X yrel Y (mouse ID), N buttons, N
    wheel, N warps, relative R, grab G, focus F`.
  - The control settings at the first frame of the controls and after a change of the
    control type or the sensitivity: `Controls: control type T, sensitivity S, immersive
    I, ...`, the two keys of look up, look down, quick up, quick down, turn left, turn
    right, immersive, slide on and the map, the mouse axes (`Controls: mouse look axis A
    invert I, turn axis ...`, 0 x, 1 y, 255 none) and the rows `KeyConfig5=` (mouse) and
    `KeyConfig0=` (keys) as in `ascent.cfg`.
  - With `-inputlog` only: one line for each 60 frames of the controls (one second with
    the fixed step) with look or turn input: `Controls second: N frames with input (M automap), keys look up U
    down D turn T frames, look keys K axes A pad P return R, heading keys K axes A pad P,
    mouse raw look L turn T, source S, mouse look M, immersive I, pitch A to B (last write:
    look, glance return or none), heading H1 to H2`. The look parts are pitch units (fixang,
    0x4000 is 90 degrees), the heading parts are the heading time of the frames (fix), the
    heading is in degrees. Source 1 is the mouse (control type 5 in the immersive mode), 2
    a joystick, 0 none. Keys down with a look part of 0: the sensitivity is 0 or another
    input cancels it. Mouse motion in the `Input second` lines with raw look and turn 0:
    the game did not read the motion.
- The messages of the data loaders (`dtu_log()`: tables, archives, music, movies, front
  end loads and saves of save games, level states). At most 3000 in one session.
- `Error()`, `Warning()` and `Int3()` (at most 500 warnings), the messages of SDL, the
  OpenGL errors (`glGetError` every 5 seconds, at most 50), the shader errors.
- With the test option `-setuplog`: one line for each creature setup by name at a level
  start (`creature setup (0xAC7B2), level L: obj N NAME type T script S: ...; before ...;
  after ...`, the values of the type before and after) and one for each `add_npc`
  (`hostile` or `peaceful`, awareness, target). `tools/check_creature_setup.py` reads them.
- The last line `Session end: normal exit` or `Session end: exit after an error`. A log
  without it is the log of a session that crashed or was stopped.

Each line starts with the seconds since the start. The file is UTF-8: the Latin-1 text of
the game (Spanish names and rolls) is converted. Each line is written to the disk at
once, so a crash keeps the lines before it.

## Crash report

An unhandled exception (on Windows: `SetUnhandledExceptionFilter`), `abort()` and an
invalid parameter of the C runtime write a report to the log:

```
[    2.660] CRASH: unhandled exception in thread 3620
[    2.660] Exception 0xC0000005 (access violation)
[    2.660] Address 0x00007FF7201E609E  ascent.exe+0x14609E  (addr2line 0x14014609E)
[    2.660] The code wrote the address 0x0000000000000000
[    2.660] Registers:
[    2.660]   RAX 0000000000000000  RBX 0000000000000032  ...
[    2.660] Stack (9 frames):
[    2.660]   #0   0x00007FF7201E609E  ascent.exe+0x14609E  (addr2line 0x14014609E)
[    2.660]   #1   0x00007FF7201608C5  ascent.exe+0xC08C5  (addr2line 0x1400C08C5)
...
[    2.776] Minidump: ...\logs\ascent-2026-11-22_02-23-44.dmp
[    2.776] CRASH report end
```

Each address has its module and the offset in the module. For `ascent.exe` the value
after `addr2line` is the address in the file (the image base of the file plus the
offset: Windows loads the program at another address each time). The stack comes from
the x64 unwind tables (`RtlVirtualUnwind`). The minidump (`MiniDumpWriteDump`, written
from a second thread so that a stack overflow can still write it) has the threads, their
stacks and the modules.

## Turning a crash address into a function

The release build (`cmake --preset release`, `cmake --build --preset release`) compiles
the game with `-g`, keeps a copy of the executable with its debug information and a map
file in `build-rel`, and then removes the debug information from `ascent.exe`:

- `build-rel/ascent-symbols.exe`: the same code as the installed `ascent.exe`, with
  the debug information. It is not installed.
- `build-rel/ascent.map`: the linker map. Its addresses are offsets in the module (the
  `ascent.exe+0x...` values of the report).

Keep the two files of each release that players get (for example with the setup
program). With the values after `addr2line` in the report:

```sh
export PATH=/c/msys64/ucrt64/bin:$PATH
addr2line -f -p -e build-rel/ascent-symbols.exe 0x14014609E 0x1400C08C5 0x140009A41
```

```
dtulog_frame at engine/main/dtulog.c:229
dtu_frame_done at engine/main/dtugame.c:245
game at engine/main/game.c:2897
```

A development build (`build`, RelWithDebInfo) has the debug information in
`build/engine/main/ascent.exe`: use that file with `addr2line`.

The test option `-testcrash N [KIND]` makes a crash at game frame N to check the report.
KIND is `null` (default, a write to address 0), `abort`, `error` (`Error()`),
`overflow` (stack overflow) or `divide` (integer divide by zero):

```sh
build/engine/main/ascent.exe -datadir E:/UNDAMONTAN/DTU -datadir gamedata/cd/DTU \
  -hidden -mute -nomovies -userdir <scratch> -level BANE -frames 200 -testcrash 50
```

## Memory report

The option `-memreport` lists on stderr, at the exit, each block of the mem library
(`engine/mem/mem.c`, the `malloc` and `free` of the game code) that is not freed: the
variable, the file and line of the `malloc`, and the return addresses of 6 callers as
offsets in the module. Add the image base 0x140000000 for `addr2line`. Then it prints
the memory in use and the largest use of the session. A block whose check bytes are
overwritten gives `MEM_OVERWRITE` (at its `free`, or at the exit).

```sh
build/engine/main/ascent.exe -datadir E:/UNDAMONTAN/DTU -datadir gamedata/cd/DTU \
  -hidden -mute -nomovies -userdir <scratch> -level KOBOLDS -frames 300 -memreport 2> mem.txt
addr2line -f -s -e build/engine/main/ascent.exe 0x1400ABCDE
```

MinGW has no address sanitizer. The UBSan build of docs/audit/presentation.md traps
array bounds errors that the compiler can see.

## Code

- `engine/platform/plat_log.c`: the log file, the rotation, the system and video lines,
  the SDL messages, the OpenGL error check, the crash handler.
- `engine/main/dtulog.c`: the start lines, the settings, the game events, the hook of
  `Error()`, `Warning()` and `Int3()` (`set_log_func` in `engine/misc/error.c`), the
  forward of `dtu_log()`, `-testcrash`.
- `cmake/version.cmake`: `ascent_version.h` with the version, made at each build.
