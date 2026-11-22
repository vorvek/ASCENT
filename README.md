# ASCENT-DTU

ASCENT-DTU is a source port of Descent to Undermountain (Interplay, 1997).

The original game runs on DOS and uses a modified Descent engine. It had a troubled development, not helped by using an engine meant for spaceships flying mining tunnels instead of
dungeon crawling humanoids. Version 1.3 of the game still has many defects.

ASCENT-DTU aims to make the game playable in modern Windows, alongside bugfixes and optional improvements.

## New features

* Gamepad support
* A new 32-bit color GPU based renderer allowing for higher resolutions, alongside a depth buffer (no more enemies clipping through walls!).
* Upscaled textures and movies.
* Content-id-safe streaming and gameplay recordings. Originally, the Yawning Portal uses "Return From The Hunt" as a theme. It's been replaced with the "Seedy Tavern" theme by Jeremy Soule, from Neverwinter Nights.
* Improved physics, untied to the framerate. No more floating corpses or cursed ladders. Jumping no longer pulls you back down in a microsecond.
* Animation interpolation.
* Target hint: the cursor turns a little red when your attack would reach an enemy.
* Full screen shaders.

## Game files

This repository does not contain game files.
You must own a copy of Descent to Undermountain to use this project.

## Downloads during installation

The installer and the content manager (`ascent-content`) can download the 1.3 patch and the optional upscaled textures and movies.
The server they download this content from uses Cloudflare.

Some internet providers block Cloudflare addresses at some times. For example, providers in Spain block them during football matches.
If a download stops with a timeout, try again later or use a VPN.
Then start "ASCENT-DTU Content" from the Start menu to download the files that are missing.

The upscaled files are not necessary to play. The 1.3 patch is also available at [The Patches Scrolls](https://www.patches-scrolls.de/patch/1215/7/41518/download).

## Troubleshooting

Each run writes a log in `%APPDATA%\ASCENT-DTU\logs`. If the game fails or crashes, send the newest `ascent-*.log` (and the `.dmp` file with the same name after a crash).
See [docs/debug_logs.md](docs/debug_logs.md).

## Source layout

- `engine/`: the Descent 1 source release (version 1.5) from Parallax Software. It is the base of the port.
- `tools/`: Python tools that read the original game files.
- `docs/formats/`: the file format documents.

## License

The code in `engine/` is the property of Parallax Software Corporation.
The Parallax license permits use and derivative works only for non-commercial purposes.
The full text is in `engine/readme.txt`. This project is non-commercial.

## Legal

Descent to Undermountain is a trademark of its owners.
This project has no connection with Interplay or with Wizards of the Coast.
