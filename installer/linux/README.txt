ASCENT-DTU @VERSION@ for Linux (x86_64)
=======================================

ASCENT-DTU is a source port of Descent to Undermountain (Interplay, 1997,
version 1.3). It needs the game disc: this release has no game files.

Install
-------

    ./setup.sh

The setup asks for:

- the installation folder (default ~/.local/share/ASCENT-DTU/game; as root
  /opt/ascent-dtu, for all users),
- the game disc: a mounted CD, a copy of the disc in a folder, or a disc
  image (.iso, or .cue with .bin). An installed game is not enough,
- the language of the game (English or Spanish) and the optional
  downloads (upscaled textures and interface, upscaled movies, the
  Spanish text, the miss sounds of a melee attack).

It downloads the files of the 1.3 patch, makes the data folder, and makes
the command ascent-dtu (~/.local/bin) and a menu entry.

Without questions, for example:

    ./setup.sh --yes --source /media/cdrom --lang en --textures no

./setup.sh --help lists the options.

The setup needs curl or wget, unzip and sha256sum. For the icon it uses
icotool (package icoutils) or ImageMagick if one is installed.

Play
----

    ascent-dtu

Saves and settings: ~/.local/share/ASCENT-DTU. The game reads save games
of the DOS version 1.3.

Remove
------

    ~/.local/share/ASCENT-DTU/game/setup.sh --uninstall

It asks first. The saves and settings stay; --purge removes them too.

Licenses
--------

LICENSE-PARALLAX.txt: the Descent source code (Parallax Software license,
non-commercial use). LICENSE-SDL3.txt: SDL 3 (zlib license), linked into
the program.
