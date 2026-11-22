/*
 * ASCENT: the "Improvements" settings.
 *
 * These settings change the game beyond the original behavior. They are
 * different from the Dtu_fix_* settings, which correct defects of the
 * original (frame rate dependence and similar) and stay on.
 *
 * Each setting is on by default. With a setting off, the game uses the
 * original behavior with the Dtu_fix_* corrections. The options -orig*
 * (for example -origphys) still give the exact original rules.
 *
 * The values are kept in the configuration file of the port (see
 * dtuopts.c). The Improvements and Graphics menus of the options menu
 * and of the Escape menu change them (key F5 opens Graphics in the game).
 */
#ifndef _DTUOPTS_H
#define _DTUOPTS_H

// Player movement without the drift of the Descent ship physics: the
// player stops when the keys are released and does not slide in turns and
// side steps. Objects settle on the floor (a dead creature: always, see
// Dtu_fix_corpse_floor in dtuobj.h). The
// corrections of the frame rate (jump, ladders, walls) are not here: they
// are always on (Dtu_fix_* in physics.c).
extern int Dtu_improved_physics;

// Jump control: steering in the air, a jump a short time after the floor
// is left, and a jump that a wall or the edge of a ledge does not stop, so
// that it lands on a ledge or the top of a ladder.
extern int Dtu_improved_jump;

// Paths for creatures of the size of a person: no stop at the openings of
// corridors, and a larger body so that creatures stay at the distance of
// their weapon (see docs/re/ai.md, "Improved pathfinding").
extern int Dtu_improved_pathfinding;

// The 3D view is rendered at this multiple of the original view size,
// from 1 to 6. The interface is not scaled by it. Default 3.
extern int Dtu_render_scale;
#define DTU_RENDER_SCALE_MIN	1
#define DTU_RENDER_SCALE_MAX	6

// Depth test for each pixel on the GPU: a part of a creature behind a
// wall is hidden. The original draws a whole object or nothing.
extern int Dtu_improved_occlusion;

// The arms and the chest of the first person body follow the pitch of the
// view, so that the weapon attacks are visible when the player looks
// forward or up.
extern int Dtu_improved_body;

// Animation interpolation: creatures, characters, the arms of the player
// and the other animated models are drawn with the joint angles between
// the current animation frame and the next one, by the time between the
// two frames. Only the picture changes: the game rules (hit and fire
// frames) use the frame numbers of the original. It also works with the
// software renderer.
extern int Dtu_anim_interp;

// Target hint: the cursor of the play screen has a warm red tint while the
// attack of the item in the hand (the swing of a melee weapon, a missile, a
// spell missile) would reach a hostile creature that it can hurt, by the
// same test as the attack (dtucombat.c). Off: the cursor of the original.
extern int Dtu_target_hint;

// The 3D view uses the GPU renderer. 0 selects the software renderer of
// the original. Option -software sets 0. The software renderer disables
// the other graphics settings.
extern int Dtu_gpu_renderer;

// Graphics settings (the Graphics menu, key F5 in the game). They need the
// GPU renderer.

// Textures and bitmaps from the upscaled texture set (4 times the original
// size, 32-bit color) when the set exists. See tools/textures_hd.py.
extern int Dtu_upscaled_textures;

// Movies from the upscaled movie set when it exists. See tools/movies_hd.py.
extern int Dtu_upscaled_movies;

// Anisotropic filtering of the textures: 0 off, 1 on (8x). The GPU renderer
// always uses trilinear mipmaps; this setting adds the anisotropy.
extern int Dtu_anisotropic;

// Infravision (the key N) shows heat: light sources (torches, lamps,
// fire) and living creatures and characters are red and visible in the
// dark, undead (the UNDEAD flag of the creature table) and the rest of the
// scene are dark and cool. Off: the effect of the original (a fade table
// with red rows). Needs the RGB form of the GPU renderer.
extern int Dtu_improved_infravision;

// The screen shader: 0 off, 1 subtle (a little bloom and a soft blend of
// the pixels), 2 VGA (a mid range computer monitor of the late 1990s).
extern int Dtu_shader;
#define DTU_SHADER_OFF		0
#define DTU_SHADER_SUBTLE	1
#define DTU_SHADER_VGA		2

// Streaming safe music: the file <hd>/music/NAME.ogg plays in place of
// the track NAME.TUN when it exists (dtumusic.c, tools/music_replace.py).
// For a track that a video platform claims. Off by default (unlike the
// other settings): the default is the original music. The options
// -safemusic and -nosafemusic change it for one run.
extern int Dtu_safe_music;

// The directory of the upscaled files (movies <hd>/NAME.m1v, textures
// <hd>/textures/...), without a separator at the end, or NULL if there is
// none. The first that exists: the argument of -hddir, "hd" in the current
// directory, "hd" in each -datadir directory in the order of the
// arguments, "hd" in the parent directories of the current directory (up
// to four levels). The result is found once and kept.
const char *dtu_hd_dir(void);

// The 3D view uses the GPU renderer and the item of the Graphics menu is
// on (the software renderer disables the other graphics settings).
#define DTU_GFX(setting)	(Dtu_gpu_renderer && (setting))

// Load the settings from the configuration file. Call it one time at start,
// after the arguments are parsed. Arguments win over the file.
void dtu_opts_load(void);

// Write the settings to the configuration file.
void dtu_opts_save(void);

// Write the settings to the session log (dtulog.h).
void dtu_opts_log(void);

#endif
