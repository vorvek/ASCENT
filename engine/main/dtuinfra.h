/*
 * ASCENT: new file. Infravision: the key N of DTUGAME.EXE 1.3 (0x54632)
 * and how the 3D view shows it. The facts are in docs/re/infravision.md.
 */
#ifndef _DTUINFRA_H
#define _DTUINFRA_H

#include "object.h"

// The modes of the 3D view of a frame.
#define DTU_INFRA_OFF		0
#define DTU_INFRA_ORIGINAL	1		// the fade table of the original (both renderers)
#define DTU_INFRA_IMPROVED	2		// the heat colors of the GPU renderer

// 1 while the improved mode draws a frame: render.c and object.c set
// G3_gpu_heat for the faces and the objects.
extern int Dtu_infra_heat;

// The state is the bit DTU_PS_INFRAVISION of Dtu_player_state (the byte
// 0x1AE of the player object). The key N changes it (dtudisp.c).

// Call before the 3D view of a frame is started. gpu_rgb: the GPU renderer
// will draw the view in the RGB form. Returns DTU_INFRA_*. With
// DTU_INFRA_ORIGINAL the fade table (gr_fade_table) has the infravision
// colors until dtu_infra_end_view().
int dtu_infra_begin_view(int gpu_rgb);

// Call when the improved mode was chosen but the GPU does not draw the
// view: the software renderer gets the original fade table.
void dtu_infra_fallback(void);

// Call after the 3D view of the frame: the normal fade table comes back.
// gpu_view: the GPU drew the view (it gets the normal table again).
void dtu_infra_end_view(int gpu_view);

// The heat (PLAT_GPU_HEAT_*) of an object and of a face with the textures
// tmap1 and tmap2.
int dtu_infra_object_heat(object *obj);
int dtu_infra_face_heat(int tmap1, int tmap2);

#endif
