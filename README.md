# ASCENT

ASCENT is a source port of Descent to Undermountain (Interplay, 1997).

The original game runs on DOS and uses a modified Descent engine.
Version 1.3 of the game still has many defects.
The goal of this project is a modern engine that reads the original game files and corrects those defects.

## Status

The project is in the research phase. No playable build exists.

## Goals

- Read the original game files without changes.
- Run on modern operating systems without an emulator.
- Correct the controls, the animations, and the physics.
- Make the game speed independent of the frame rate.

## Game files

This repository does not contain game files.
You must own a copy of Descent to Undermountain to use this project.

## Downloads during installation

The installer and the content manager (`ascent-content`) download the 1.3 patch and the optional upscaled textures and movies from `ascent-content.generalsimulationworks.com`.
This server uses Cloudflare.
Some internet providers block Cloudflare addresses at some times. For example, providers in Spain block them during football matches.
If a download stops with a timeout, try again later or use a VPN.
Then start "ASCENT Content" from the Start menu to download the files that are missing.

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
