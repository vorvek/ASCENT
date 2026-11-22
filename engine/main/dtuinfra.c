/*
 * ASCENT: new file. Infravision in the 3D view. See dtuinfra.h and
 * docs/re/infravision.md. Addresses are linear addresses of the image of
 * DTUGAME.EXE 1.3 with object 1 at 0x10000.
 *
 * The original: at the start of each frame (0x76CC8) the flag 0x14C4DA is
 * 1 when the viewer object has the bit 8 of its byte 0x1AE. The pixel
 * shading functions of the texture mapper (0xEC4D7 and its copy 0x105A81)
 * then change the color of each lit texel: a lit color with a brightness
 * step (the low 4 bits of the color, 0 is the brightest of a row of 16) of
 * 14 or more becomes the texel in the red row 0xC0 at the step of the
 * unlit texel; a step of 8 or less becomes the row 0xB0 at the lit step.
 * The port puts this rule into a copy of the fade table, so the software
 * texture mapper and the GPU renderer (both read the fade table) show it.
 *
 * The improved mode (Dtu_improved_infravision, GPU renderer, RGB form)
 * gives each polygon a heat: light sources and living creatures are red
 * and visible in the dark, undead and the rest of the scene are dark and
 * cool.
 */
#include <string.h>

#include "inferno.h"
#include "gr.h"
#include "3d.h"
#include "platform.h"
#include "object.h"
#include "bm.h"
#include "textures.h"		// NumTextures
#include "robot.h"
#include "physics.h"		// Dtu_player_state, DTU_PS_INFRAVISION
#include "lighting.h"		// dtu_object_light_value
#include "args.h"
#include "dtuopts.h"
#include "dtuinfra.h"

int Dtu_infra_heat;

static ubyte Normal_fade[256*GR_FADE_LEVELS];	// the fade table while the infravision table is in place
static int Swapped;

// The colors that 0xEC4D7 handles before the infravision test (they are
// animated there): the infravision rule does not change them.
static int special_color(int c)
{
	switch (c) {
		case 0xCF: case 0xBF: case 0xBE: case 0x6F: case 0x6E: case 0x6D: case 0x4F: case 0x4E: case 0xFF:
			return 1;
	}
	return 0;
}

static void set_table(int on)
{
	int l, c;

	if (on && !Swapped) {
		memcpy(Normal_fade, gr_fade_table, sizeof(Normal_fade));
		for (l=0; l<GR_FADE_LEVELS; l++)
			for (c=0; c<256; c++) {
				int f = Normal_fade[l*256 + c], step = f & 0x0F;

				if (special_color(c) || f == 0xFF)
					continue;
				// 0xEC5F5: the threshold 0x14C4DB is the constant 14
				if (step >= 14)
					gr_fade_table[l*256 + c] = 0xC0 | (c & 0x0F);
				else if (step <= 8)
					gr_fade_table[l*256 + c] = 0xB0 | step;
			}
		Swapped = 1;
	} else if (!on && Swapped) {
		memcpy(gr_fade_table, Normal_fade, sizeof(Normal_fade));
		Swapped = 0;
	}
}

int dtu_infra_begin_view(int gpu_rgb)
{
	int mode = DTU_INFRA_OFF;
	static int arg = -1;

	// test option: -infravision turns infravision on at the first frame
	if (arg < 0) {
		arg = FindArg("-infravision") != 0;
		if (arg)
			Dtu_player_state |= DTU_PS_INFRAVISION;
	}

	// 0x76CC8: only the view of the object with the bit (the player)
	if (Viewer == ConsoleObject && (Dtu_player_state & DTU_PS_INFRAVISION))
		mode = (gpu_rgb && DTU_GFX(Dtu_improved_infravision)) ? DTU_INFRA_IMPROVED : DTU_INFRA_ORIGINAL;

	plat_gpu_set_infravision(mode == DTU_INFRA_IMPROVED);
	Dtu_infra_heat = (mode == DTU_INFRA_IMPROVED);
	G3_gpu_heat = PLAT_GPU_HEAT_COLD;
	if (mode == DTU_INFRA_ORIGINAL)
		set_table(1);
	return mode;
}

void dtu_infra_fallback(void)
{
	Dtu_infra_heat = 0;
	set_table(1);
}

void dtu_infra_end_view(int gpu_view)
{
	if (Swapped) {
		set_table(0);
		if (gpu_view)
			plat_gpu_set_fade_table(gr_fade_table, GR_FADE_LEVELS);
	}
	Dtu_infra_heat = 0;
	G3_gpu_heat = PLAT_GPU_HEAT_COLD;
}

int dtu_infra_object_heat(object *obj)
{
	switch (obj->type) {
		case OBJ_ROBOT:
			// a creature of the table flag UNDEAD, or a dead creature, is cold
			if (obj->id < N_robot_types && (Monster_info[obj->id].flags & MONSTER_F_UNDEAD))
				return PLAT_GPU_HEAT_COLD;
			if (obj->shields < F1_0 || (obj->flags & (OF_EXPLODING|OF_SHOULD_BE_DEAD|OF_DESTROYED)))
				return PLAT_GPU_HEAT_COLD;
			return PLAT_GPU_HEAT_WARM;
		case OBJ_PLAYER:
		case OBJ_HOSTAGE:
		case OBJ_COOP:
			return PLAT_GPU_HEAT_WARM;
		case OBJ_DEBRIS:
			return PLAT_GPU_HEAT_COLD;
	}
	// fire, missiles, light objects and scenery that casts light
	return dtu_object_light_value(obj) > 0 ? PLAT_GPU_HEAT_HOT : PLAT_GPU_HEAT_COLD;
}

// The textures with a light value in the 45 level tables (see
// docs/re/infravision.md): the effect clip textures of the $EFFECTS section
// (fire, torches, lava, lamps, portals, glowing water), each with an eclip,
// and "empty.bbm" with light values 0.2 to 1.0 in $TEXTURES, without an
// eclip. The empty bitmap is transparent: a light that the level editor
// puts on a side to light the room, not a visible light. The heat of a face
// goes to the texture that shows, so the light value alone does not make a
// face hot.

// A texture that shows light: a light value and an effect clip.
static int light_texture(int t)
{
	return t >= 0 && t < NumTextures && t < MAX_TEXTURES && TmapInfo[t].lighting > 0
		&& TmapInfo[t].eclip_num >= 0;
}

// A light that does not show (empty.bbm with a light value).
static int hidden_light(int t)
{
	return t > 0 && t < NumTextures && t < MAX_TEXTURES && TmapInfo[t].lighting > 0
		&& TmapInfo[t].eclip_num < 0;
}

static int texture_heat(int t)
{
	if (!light_texture(t))
		return PLAT_GPU_HEAT_COLD;
	// lava, lava flow and magma (volatile): the whole surface is hot
	if (TmapInfo[t].flags & TMI_VOLATILE)
		return PLAT_GPU_HEAT_GLOW;
	return PLAT_GPU_HEAT_HOT;
}

// The merged texture of a face is one bitmap, so the heat is that of the
// face. With an overlay the overlay decides: a torch on a wall is hot, a
// door or a grate over a light base is cold (the levels put flame and empty
// light textures as the base of door sides, to light the doorway). An
// overlay that is a hidden light leaves the base visible: the base decides.
int dtu_infra_face_heat(int tmap1, int tmap2)
{
	tmap2 &= 0x3FFF;
	if (tmap2 > 0 && !hidden_light(tmap2))
		return texture_heat(tmap2);
	return texture_heat(tmap1);
}
