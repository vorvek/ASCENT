/*
 * ASCENT: new file. The start of a Descent to Undermountain level without
 * the title screens and menus of Descent 1 (the options -level and
 * -checklevel), and the functions that the game calls to load the data of
 * the target game.
 *
 * The functions under "data layer" are the interface to the code that
 * reads the archives, tables, textures, models and fonts of the target
 * game. dtugame.c has a weak definition of each one, which loads nothing,
 * so that the program links and runs before the data layer has them. A
 * normal definition in another file replaces the weak one.
 */
#ifndef _DTUGAME_H
#define _DTUGAME_H

#include "types.h"

//------------------------------------------------------------------------
// Data layer. Each function returns 1 if the data is loaded and 0 if the
// data is not available. A function must not stop the program if a file is
// absent.

// Loads the palette of the game into gr_palette (256 colors, 6 bits for
// each component) and the fade table into gr_fade_table (GR_FADE_LEVELS
// rows of 256 color numbers, row GR_FADE_LEVELS-1 is full light), and
// calls gr_palette_load(). Color 255 is transparent in each row of the
// fade table. The weak definition makes a palette of 16 hues with 16
// levels each and returns 0.
int dtu_load_palette(void);

// Loads the fonts (Gamefonts[], see gamefont.h). While GAME_FONT is NULL
// the game draws no text.
int dtu_load_fonts(void);

// Loads the tables that are the same for all levels: the texture table
// (NumTextures, TmapInfo[].filename, Textures[], GameBitmaps[]), the wall
// animations (Num_wall_anims, WallAnims[]), the effects, the models
// (N_polygon_models, Pof_names[], Polygon_models[]), the monster, item and
// player tables, and the sounds.
int dtu_load_tables(void);

// Loads the data of one level that is not in the level file (the archive
// and the table of the level, for example YPINN.EGG and YPINN.BIN, if the
// level has them). "level_name" is the file name of the level with or
// without the extension. The game calls it after load_level() and before
// level_translate_textures() and level_resolve_models(), so the function
// can add textures and models to the tables.
int dtu_load_level_data(char *level_name);

// Returns 1 if the texture with this number of the texture table can be
// drawn (the bitmap is loaded or can be paged in). The renderer draws a
// side with a flat color if this is 0. The weak definition returns 1 if
// the number is below NumTextures and the bitmap of the texture has data.
int dtu_texture_ready(int tmap_num);

//------------------------------------------------------------------------
// The game.

// 1: all objects are simulated and drawn. 0 (the default for now): only
// the object of the player is simulated, no object is drawn, the AI does
// not run, and the weapons of the player do not fire. The monsters and
// items of the target game use tables that are not loaded yet.
extern int Dtu_objects_enabled;

// 1 while the game runs from the option -level (no menus, Escape leaves
// the program).
extern int Dtu_direct_start;

// Reads the options -datadir DIR (one or more) and adds each directory to
// the search list of dtu_load_file().
void dtu_read_datadir_args(void);

// The option -checklevel NAME: loads the level, prints the summary to
// stdout. Returns the exit status of the program (0 if the level loads).
int dtu_check_level(char *level_name);

// Returns the number of the level in the loaded mission (1 for the first
// level), or 0 if the mission does not have a level with this file name.
int dtu_find_level_num(char *level_name);

// The game calls this function after each frame. It handles the options
// -frames N (leave the game after N frames) and -screenshot FILE (write
// the last frame to a PPM file). Returns 1 if the game must stop.
int dtu_frame_done(void);

// Writes the current screen to a PPM file (P6). Returns 1 if successful.
int dtu_write_screenshot(char *filename);

// The color number for a side with no texture. "tmap_level" is the texture
// number of the level file and "light" is the light of the side (fix, 0 to
// 1). The color depends on the texture number, so that sides with
// different textures look different.
int dtu_flat_color(int tmap_level, fix light);

#endif
