/*
 * ASCENT: new file. See dtugame.h.
 *
 * It has:
 *  - the options -datadir, -checklevel, -frames and -screenshot
 *  - the weak definitions of the data layer functions of dtugame.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "palette.h"
#include "error.h"
#include "args.h"
#include "segment.h"
#include "object.h"
#include "wall.h"
#include "bm.h"
#include "textures.h"
#include "piggy.h"
#include "gamefont.h"
#include "gameseq.h"
#include "gamemine.h"
#include "gamesave.h"
#include <ctype.h>
#include "mission.h"
#include "dtufile.h"
#include "dtugame.h"

#define DTU_WEAK __attribute__((weak))

int Dtu_objects_enabled = 1;
int Dtu_direct_start = 0;

//------------------------------------------------------------------------
// Weak definitions of the data layer functions

// A palette for the time before the data layer loads the palette of the
// game: 16 hues (hue 0 is gray) with 16 levels each. The color number is
// hue*16+level. The fade table scales the level.
DTU_WEAK int dtu_load_palette(void)
{
	static const ubyte hues[16][3] = {
		{63,63,63}, {63,20,20}, {63,40,16}, {63,56,20}, {44,60,20}, {20,60,24},
		{20,56,48}, {20,48,63}, {24,32,63}, {40,28,63}, {56,28,60}, {63,28,44},
		{52,40,30}, {40,48,40}, {40,44,56}, {56,52,44}
	};
	int hue, level, l;

	for (hue=0; hue<16; hue++)
		for (level=0; level<16; level++) {
			int c = hue*16 + level;
			gr_palette[c*3+0] = hues[hue][0] * (level+1) / 16;
			gr_palette[c*3+1] = hues[hue][1] * (level+1) / 16;
			gr_palette[c*3+2] = hues[hue][2] * (level+1) / 16;
		}
	gr_palette[0] = gr_palette[1] = gr_palette[2] = 0;

	for (l=0; l<GR_FADE_LEVELS; l++) {
		for (hue=0; hue<16; hue++)
			for (level=0; level<16; level++)
				gr_fade_table[l*256 + hue*16 + level] = hue*16 + (level * l) / (GR_FADE_LEVELS-1);
		gr_fade_table[l*256+255] = 255;		// the transparent color
	}

	gr_palette_load(gr_palette);

	return 0;
}

// ASCENT: the data layer exists now. These functions call it.
int dtu_load_fonts(void)
{
	gamefont_init();
	return 1;
}

int dtu_load_tables(void)
{
	bm_init();
	return 1;
}

// The archive of the player character. The front end of the game selects
// it from the race, the sex and the class. The default is the human male
// fighter until the port has a character selection.
char Dtu_player_archive[16] = "hmc";

int dtu_load_level_data(char *level_name)
{
	char name[16];
	int i;

	for (i=0; i<15 && level_name[i] && level_name[i] != '.'; i++)
		name[i] = tolower(level_name[i]);
	name[i] = 0;
	return bm_load_level_tables(name, Dtu_player_archive);
}

DTU_WEAK int dtu_texture_ready(int tmap_num)
{
	if (tmap_num < 0 || tmap_num >= NumTextures)
		return 0;
	return GameBitmaps[Textures[tmap_num].index].bm_data != NULL;
}

//------------------------------------------------------------------------

void dtu_read_datadir_args(void)
{
	int i;

	for (i=1; i<Num_args-1; i++)
		if (!stricmp(Args[i], "-datadir"))
			dtu_add_data_dir(Args[i+1]);
}

int dtu_check_level(char *level_name)
{
	if (load_level(level_name)) {
		printf("Cannot load the level %s\n", level_name);
		return 1;
	}

	level_print_summary(stdout, level_name);

	// Facts about the load that tools/lvl.py does not print. A line that
	// starts with "#" is not a part of the summary.
	printf("# file %s\n", Gamesave_current_filename);
	printf("# validate_segment: %d sides with another type, %d sides with another normal than in the file\n",
		Level_validate_type_changes, Level_validate_normal_changes);
	printf("# fix_object_segs: %d objects moved to another segment, %d objects moved to the center of their segment\n",
		Level_objects_moved, Level_objects_centered);
	printf("# Highest_segment_index %d, Highest_vertex_index %d, Highest_object_index %d\n",
		Highest_segment_index, Highest_vertex_index, Highest_object_index);

	return 0;
}

// Compares two level file names without the extension. Ignores case.
static int level_name_match(char *a, char *b)
{
	while (*a && *a != '.' && *b && *b != '.') {
		char ca = *a++, cb = *b++;
		if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
		if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
		if (ca != cb)
			return 0;
	}
	return (*a == 0 || *a == '.') && (*b == 0 || *b == '.');
}

int dtu_find_level_num(char *level_name)
{
	int i;

	for (i=0; i<Last_level; i++)
		if (level_name_match(Level_names[i], level_name))
			return i+1;

	return 0;
}

//------------------------------------------------------------------------
// -frames and -screenshot

int dtu_write_screenshot(char *filename)
{
	grs_bitmap *bm = &grd_curscreen->sc_canvas.cv_bitmap;
	FILE *f;
	int x, y;

	if (!bm->bm_data || bm->bm_type != BM_LINEAR)
		return 0;

	f = fopen(filename, "wb");
	if (!f)
		return 0;

	fprintf(f, "P6\n%d %d\n255\n", bm->bm_w, bm->bm_h);
	for (y=0; y<bm->bm_h; y++)
		for (x=0; x<bm->bm_w; x++) {
			int c = bm->bm_data[y*bm->bm_rowsize + x];
			int k;
			for (k=0; k<3; k++) {
				int v = gr_palette[c*3+k] & 63;
				fputc((v << 2) | (v >> 4), f);
			}
		}
	fclose(f);

	return 1;
}

int dtu_frame_done(void)
{
	static int frame_count = 0;
	static int frame_limit = -1;
	int t;

	if (frame_limit == -1) {
		frame_limit = 0;
		if ((t = FindArg("-frames")) != 0 && t+1 < Num_args)
			frame_limit = atoi(Args[t+1]);
	}

	frame_count++;

	if (frame_limit > 0 && frame_count >= frame_limit) {
		if ((t = FindArg("-screenshot")) != 0 && t+1 < Num_args) {
			if (dtu_write_screenshot(Args[t+1]))
				printf("Wrote %s after %d frames\n", Args[t+1], frame_count);
			else
				printf("Cannot write %s\n", Args[t+1]);
		}
		return 1;
	}

	return 0;
}

//------------------------------------------------------------------------

int dtu_flat_color(int tmap_level, fix light)
{
	static ubyte bright[256];
	static int n_bright = -1;
	int row, base;

	// The colors of the palette that are bright enough to show the light.
	// The list is made one time, so load the palette before the first frame.
	if (n_bright < 0) {
		int c;

		n_bright = 0;
		for (c=1; c<255; c++) {
			int r = gr_palette[c*3], g = gr_palette[c*3+1], b = gr_palette[c*3+2];
			int m = r > g ? r : g;
			if (b > m) m = b;
			if (m >= 40)
				bright[n_bright++] = c;
		}
		if (n_bright == 0)
			bright[n_bright++] = 254;
	}

	if (tmap_level < 0)
		tmap_level = 0;
	base = bright[(tmap_level * 7) % n_bright];

	if (light < 0)
		light = 0;
	row = fixmul(light, (GR_FADE_LEVELS-1) * F1_0) >> 16;
	if (row < 6)
		row = 6;
	if (row > GR_FADE_LEVELS-1)
		row = GR_FADE_LEVELS-1;

	return gr_fade_table[row*256 + base];
}
