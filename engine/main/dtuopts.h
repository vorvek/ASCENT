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
 * dtuopts.c). The Improvements menu of the options menu and of the
 * Escape menu changes them.
 */
#ifndef _DTUOPTS_H
#define _DTUOPTS_H

// Player movement without the drift of the Descent ship physics, ladders
// and climbing that do not depend on the frame rate, and objects (also
// dead creatures) that settle on the floor.
extern int Dtu_improved_physics;

// Jump control that reaches the top of a ladder or a ledge without an
// attack, and control of the direction in the air.
extern int Dtu_improved_jump;

// Paths for creatures of the size of a person: no stop at the openings of
// corridors, and a larger body so that creatures stay at the distance of
// their weapon.
extern int Dtu_improved_pathfinding;

// The 3D view is rendered at this multiple of the original view size,
// from 2 to 6. The interface is not scaled by it. Default 3.
extern int Dtu_render_scale;
#define DTU_RENDER_SCALE_MIN	2
#define DTU_RENDER_SCALE_MAX	6

// Depth test for each pixel on the GPU: a part of a creature behind a
// wall is hidden. The original draws a whole object or nothing.
extern int Dtu_improved_occlusion;

// The arms and the chest of the first person body follow the pitch of the
// view, so that the weapon attacks are visible when the player looks
// forward or up.
extern int Dtu_improved_body;

// The 3D view uses the GPU renderer. 0 selects the software renderer of
// the original. Option -software sets 0.
extern int Dtu_gpu_renderer;

// Load the settings from the configuration file. Call it one time at start,
// after the arguments are parsed. Arguments win over the file.
void dtu_opts_load(void);

// Write the settings to the configuration file.
void dtu_opts_save(void);

#endif
