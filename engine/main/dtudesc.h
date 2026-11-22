/*
 * ASCENT: new file. The description window: a right click on an item of
 * the inventory screen, a spell of a spell list or a line of the market
 * shows what the item or the spell does, on the picture of the scroll text
 * screen. DTUGAME.EXE 1.3 has nothing like it. The layout rules are in
 * docs/description_window.md.
 */
#ifndef _DTUDESC_H
#define _DTUDESC_H

// The text of an item: lines separated by '\n', the first line is the
// name. count: the count or the charges to show, -1 none. price: the
// price to show, -1 none; sell 1: the price is the sale price of the
// player. Returns the number of lines.
int dtu_desc_item_text(int type, int id, int count, int price, int sell, char *out, int size);
// The text of a spell at caster level L (-1: the level of the player).
int dtu_desc_spell_text(int spell, int L, char *out, int size);

// Open the window in the game (over the play screen). The window stops the
// game time, as the scroll text screen does, and a key or a mouse button
// closes it. Each returns 1 if the window opened.
int dtu_desc_open_entry(int index);			// an entry of Dtu_inventory[]
int dtu_desc_open_spell(int spell);
int dtu_desc_open_item(int type, int id, int count, int price, int sell);
// Open the window in the front end (the market, 640x480). The caller
// closes it and gives it no other input while it is open.
int dtu_desc_open_fe(int type, int id, int count, int price, int sell);

int dtu_desc_is_open(void);
void dtu_desc_close(void);
// No window at a new level and in a loaded game.
void dtu_desc_reset(void);
// The game frame while the window is open: the keys and the mouse buttons.
void dtu_desc_frame(void);
// Draws the window over the play screen (after the interface) or over the
// front end screen.
void dtu_desc_draw(void);
void dtu_desc_draw_fe(void);

// The test aid (-chartest desc): writes the text of each item number of
// the three item tables and of each spell to the -charlog file and to
// stdout ("desc: " lines), with a check that each text fits the scroll
// box of both interfaces. Returns the number of failures.
// L: the caster level of the spell texts, 0 for the level of the player.
int dtu_desc_log_all(int L);

#endif
