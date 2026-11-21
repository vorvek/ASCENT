/*
 * ASCENT: new file. The movie player. It replaces the calls of the MVE
 * library of Interplay in DTUGAME.EXE: the play function 0x96439 ("MPLAY32")
 * and its three callers. docs/re/movies.md has the addresses.
 *
 * The player decodes the MVE file of the game (mvedec.c). If an upscaled
 * copy of the movie exists (NAME.m1v, an MPEG-1 video stream made by
 * tools/movies_hd.py), the player shows its pictures in place of the MVE
 * pictures. The sound and the timing always come from the MVE file.
 *
 * Options:
 *   -nomovies          play no movie
 *   -notitles          (original) no movies at the start
 *   -hddir DIR         the directory of the upscaled movies. Without it the
 *                      player looks for hd/NAME.m1v in the data directories
 *                      and in the current directory.
 *   -nohdmovies        do not use the upscaled movies
 *   -movienative       show the movie as the original: 1:1 in the middle of
 *                      a 640x480 screen. Without it the movie fills the
 *                      window with its own aspect (432:320, square pixels).
 *   -playmovie NAME    test: play the movie NAME at the start and leave the
 *                      program. -movieframes N stops after N frames and
 *                      -movieshot FILE.ppm writes the last frame shown.
 */
#ifndef _DTUMOVIE_H
#define _DTUMOVIE_H

#define DTUMOVIE_DONE		0		// the movie played to its end
#define DTUMOVIE_SKIPPED	1		// the player pressed a key or a button
#define DTUMOVIE_MISSING	-1		// no file, a bad file, or -nomovies

// Play one movie. name is the file name, for example "INTRO2.MVE". The
// file comes from the data directories.
int dtumovie_play(const char *name);

// The movies at the start (0xB84F3): LOGO22S.MVE, INTRO2.MVE, TLOGOS.MVE,
// unless -notitles. A key in one movie skips the rest. The front end calls
// it before the title picture. It also runs the test option -playmovie.
void dtumovie_title(void);

// The end sequence of the dialog statement D3 with sound 0xFFFF (0xA9A5B):
// the movie FLAM1.MVE, with the game time stopped. The game goes on after it.
void dtumovie_flame(void);

// The end of the game (0xA6786): the Flamesword (object 3 of level 0x20,
// FLAME) breaks. The movie CREDITV1.MVE plays and the game ends to the
// front end. Returns 1 if the game ends.
int dtumovie_end_credits(void);

#endif
