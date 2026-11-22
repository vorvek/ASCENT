/*
 * ASCENT: new file. The text screen of the Scroll (dispscrl.c of
 * DTUGAME.EXE 1.3, 0x9DC42). Descent 1 has nothing like it. See
 * dtuscroll.c and docs/re/ingame-ui.md, section "The scroll text screen".
 */
#ifndef _DTUSCROLL_H
#define _DTUSCROLL_H

// Opens the screen with scroll text n (0x9DC42): 0 is the summary of the
// current mission, 1 to 127 an entry of the text table. Returns 0 if the
// screen opens, 1 if its picture could not be loaded.
int dtu_scroll_show(int n);

// 1 while the screen is open (0x9CC83: the frame function 0x1452DD is set).
int dtu_scroll_is_open(void);

// The part of the game frame for the screen, in place of the controls
// (0x43601 to 0x4363F, 0x9CCAE): reads the keys and the mouse buttons and
// runs the opening or the closing step.
void dtu_scroll_frame(void);

// ASCENT: draws the screen over the play screen. GameLoop calls it after
// the interface (game_render_frame_mono).
void dtu_scroll_draw(void);

// ASCENT: closes the screen at once (a new level or a loaded game).
void dtu_scroll_reset(void);

// ASCENT: the picture of the scroll (an entry of FEBGND) and its text box,
// for the low (hi 0) or the high resolution (the description window,
// dtudesc.c).
void dtu_scroll_box(int hi, int *art, int *x, int *y, int *w, int *h);

#endif
