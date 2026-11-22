/*
 * ASCENT: new file. The in-game interface of Descent to Undermountain:
 * the play screen, the screens that open during play, the buttons and the
 * mouse cursor. It replaces the Descent 1 cockpit and HUD (gauges.c, hud.c
 * and the cockpit setup of game.c) while Dtu_display_active is 1.
 *
 * The original code is in displays.c, dispbutn.c, dispcanv.c and dtumouse.c
 * of DTUGAME.EXE. docs/re/ingame-ui.md has the addresses and says which
 * facts are verified.
 */
#ifndef _DTUDISP_H
#define _DTUDISP_H

#include "types.h"

// The interface modes (the variable at 0x13B616 of the original).
#define DTU_IM_MAIN			0		// main game screen
#define DTU_IM_INVENTORY	3
#define DTU_IM_MAP			4
#define DTU_IM_STATS			5
#define DTU_IM_HOTKEYS		6		// low resolution: hot key screen. High: inventory.
#define DTU_IM_FULL_SCREEN	7
#define DTU_IM_LETTERBOX	8

// 1: the 640x480 interface (the default of the port). 0: the 320x200
// interface (the option -lowres). Set it before dtu_display_init().
extern int Dtu_hires;

// 1 after dtu_display_init() loaded the art. While it is 0 the game draws
// the Descent 1 screen.
extern int Dtu_display_active;

extern int Dtu_interface_mode;

// 1: the mouse turns the character and no cursor is drawn. 0: the mouse
// moves the cursor (the key V changes it).
extern int Dtu_immersive;

// 0 (default): a key or a mouse button acts once for each press. 1: it acts
// again in each frame while it is down, as the original does.
extern int Dtu_ui_key_repeat;

// The number of the portrait of the player (0 to 31).
extern int Dtu_player_portrait;

// Loads the art of the interface. Returns 1 if successful. The palette and
// the fonts must be loaded.
int dtu_display_init(void);

// ASCENT: frees the art of the interface. dtu_display_init registers it
// with atexit.
void dtu_display_close(void);

// Selects an interface mode: loads the picture of the mode and sets the
// view rectangle (0x407EB of the original).
void dtu_display_select_mode(int mode);

// Sets the render canvases of the game for the current mode. init_cockpit()
// calls it.
void dtu_display_init_cockpit(void);

// Call before and after the 3d render of a frame. In high resolution each
// rendered line is shown twice, so the pixel aspect is different.
void dtu_display_begin_render(void);
void dtu_display_end_render(void);

// ASCENT: the GPU image of the view shows again in the rectangle of the
// last frame (gpu.c ends the rectangle when a screen covers it). For a
// screen that is copied back over a full screen picture (kconfig.c).
void dtu_display_view_again(void);

// Draws the interface and the rendered view on the screen canvas, and
// handles the keys and the mouse of the interface. Called once per frame
// in place of the copy of the view to the screen.
void dtu_display_frame(void);

// The inventory screen starts at entry 0 (a load or a new character).
void dtu_display_inventory_reset(void);

// Returns 1 if the interface handles this key (value of key_inkey()), so
// the game must ignore it.
int dtu_display_owns_key(int key);

// Adds a text to the message box of the main screen.
void dtu_display_add_message(char *text);

// Adds the message of a trigger: the color of the player and an empty line after it.
void dtu_display_add_trigger_message(char *text);

// Loads picture n of FEBGND.DAT and the palette of the entry (768 bytes,
// values 0 to 63). Returns NULL if the picture is not available. The
// caller releases the bitmap with gr_free_bitmap(). pal can be NULL.
// The front end uses it (dtufe.c).
struct _grs_bitmap;
struct _grs_bitmap *dtu_febgnd_load(int n, ubyte *pal);

// ASCENT: draws the cursor of the play screen at the mouse position on the
// current canvas, if the display is active and not in the immersive mode.
// The conversation window (dtudlg.c) calls it after it draws itself.
void dtu_display_draw_cursor(void);

// ASCENT: takes the last cursor of the play screen out of the frame, if
// its pixels are still there (a menu opens over the play screen).
void dtu_display_hide_cursor(void);

// ASCENT: draws the mouse pointer of the menus into dest with its center
// at (x, y): the cursor picture of the play screen in the colors of the
// current palette. It covers at most DTU_POINTER_SIZE pixels in each
// direction, from x - DTU_POINTER_SIZE / 2.
#define DTU_POINTER_SIZE	31
void dtu_display_draw_pointer(struct _grs_bitmap *dest, int x, int y);

#endif
