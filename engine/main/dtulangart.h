/*
 * ASCENT: new file. The pictures of the language (dtulangart.c): the
 * interface pictures with English words painted into them, with the words
 * of the language. The files are in docs/formats/lang.md, the pictures in
 * docs/re/frontend.md ("The words in the pictures").
 */
#ifndef _DTULANGART_H
#define _DTULANGART_H

#include "gr.h"

// The language number of DTUGAME.EXE for the language of the text (0
// English, 4 Spanish; the order of the letters "efgi123" at 0x145750).
int dtu_lang_art_index(void);

// The 8-bit picture of the language for picture n of FEBGND.DAT: the
// pixels of bm change where the file <lang dir>/<code>/orig/febgnd_<n>.png
// has them. Returns 1 if the file exists (nothing for English).
int dtu_lang_art_febgnd(grs_bitmap *bm, int n);

// 1 if the language has a whole picture or a patch for picture n.
int dtu_lang_art_has_febgnd(int n);

// The same for a bitmap of PORTAL.EGG (name in small letters).
int dtu_lang_art_egg(grs_bitmap *bm, const char *name);
int dtu_lang_art_has_egg(const char *name);

// The upscaled picture of the language for picture n (drawn over
// <hd>/ui/febgnd_<n>.png), or NULL.
char *dtu_lang_art_ui(char *out, int size, int n);

#endif
