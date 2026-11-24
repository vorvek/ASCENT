/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.  
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 * $Source: f:/miner/source/main/rcs/automap.h $
 * $Revision: 2.0 $
 * $Author: john $
 * $Date: 1995/02/27 11:29:35 $
 * 
 * Prototypes for auto-map stuff.
 * 
 * $Log: automap.h $
 * Revision 2.0  1995/02/27  11:29:35  john
 * New version 2.0, which has no anonymous unions, builds with
 * Watcom 10.0, and doesn't require parsing BITMAPS.TBL.
 * 
 * Revision 1.5  1994/12/09  00:41:21  mike
 * fix hang in automap print screen
 * 
 * Revision 1.4  1994/07/14  11:25:29  john
 * Made control centers destroy better; made automap use Tab key.
 * 
 * Revision 1.3  1994/07/12  15:45:51  john
 * Made paritial map.
 * 
 * Revision 1.2  1994/07/07  18:35:05  john
 * First version of automap
 * 
 * Revision 1.1  1994/07/07  15:12:13  john
 * Initial revision
 * 
 * 
 */



#ifndef _AUTOMAP_H
#define _AUTOMAP_H

extern void do_automap(int key_code);
extern void automap_clear_visited();
extern ubyte Automap_visited[MAX_SEGMENTS];
extern void modex_print_message(int x, int y, char *str);

// ASCENT: the automap of the target game, in the circle of the map screen
// (interface mode 4). See docs/re/automap.md.

// 1 while the map has the controls (0x13A7F8): a click in the map turns it
// on, a click outside it, Escape and a new map screen turn it off. While it
// is off the map follows the player.
extern int Dtu_automap_active;

// 1: the D key does not show the whole level (the original does, while the
// map has the controls). -origui gives the original.
extern int Dtu_fix_automap_reveal;

// Draws the map into the rectangle (x, y, w, h) of the canvas (0x1887E).
// The caller draws the frame of the circle over it.
extern void dtu_automap_frame(struct _grs_canvas *canv, int x, int y, int w, int h);

// A key of the map: KEY_MINUS, KEY_EQUAL, KEY_ESC, KEY_ALTED+KEY_F, KEY_D.
extern void dtu_automap_key(int key);

// 1 if the map takes this key of the game key loop
extern int dtu_automap_owns_key(int key);

// Called at the start of the controls of the player (dtu_read_controls of
// controls.c). While the map has the controls, it reads them for the next
// map frame, clears the controls of the player and returns 1.
extern int dtu_automap_read_controls(void);

// For tools/uitest.py (the script command "mapstate"): prints the angles
// of the map and the game time.
extern void dtu_automap_print_state(void);

#endif
