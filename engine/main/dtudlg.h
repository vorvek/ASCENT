/*
 * ASCENT: new file. The conversation window of Descent to Undermountain
 * (dlgmenu.c of DTUGAME.EXE 1.3): the speech and the narration of a
 * script, the list of replies and the choice of the player. Descent 1 has
 * nothing like it. See docs/re/scripts.md, section 4.
 */
#ifndef _DTUDLG_H
#define _DTUDLG_H

// The open window (0xA74AA, 0x145534). Opening selects interface mode 0 if
// the current mode is not 0 or 7 (0xB13C4).
void dtu_dlg_open(void);
void dtu_dlg_close(void);			// 0xA74D0
int dtu_dlg_is_open(void);

// Adds a text to the text area: speech (green, 0xB1F5F) or narration
// (yellow, 0xB1F52). The texts stay until a choice is made.
void dtu_dlg_add_text(const char *text, int narration);

// Adds a reply (0xB1D99). At most 10.
void dtu_dlg_add_reply(const char *text);

// The reply list is complete: the wait for the choice starts (0xB2280).
void dtu_dlg_start_choice(void);

// Reads the keys. Returns -1 while there is no choice, the number of the
// reply (from 0), or -2 for Escape. With no reply Enter returns 0. A choice
// clears the texts and the replies (0xB23C6).
int dtu_dlg_poll(void);

// Draws the window over the screen. GameLoop calls it after the interface
// (game_render_frame_mono).
void dtu_dlg_draw(void);

#endif
