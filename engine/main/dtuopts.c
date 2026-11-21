/*
 * ASCENT: the "Improvements" settings. See dtuopts.h.
 *
 * The configuration file is ascent.cfg in the current directory. It is a
 * text file with one "name=value" line for each setting. The port never
 * writes to the game directories.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "dtuopts.h"
#include "args.h"

int Dtu_improved_physics = 1;
int Dtu_improved_jump = 1;
int Dtu_improved_pathfinding = 1;
int Dtu_render_scale = 3;
int Dtu_improved_occlusion = 1;
int Dtu_improved_body = 1;
int Dtu_gpu_renderer = 1;

#define DTU_OPTS_FILE	"ascent.cfg"

typedef struct {
	const char *name;
	int *value;
	int min, max;
} dtu_opt;

static dtu_opt Opts[] = {
	{ "ImprovedPhysics",     &Dtu_improved_physics,     0, 1 },
	{ "ImprovedJump",        &Dtu_improved_jump,        0, 1 },
	{ "ImprovedPathfinding", &Dtu_improved_pathfinding, 0, 1 },
	{ "RenderScale",         &Dtu_render_scale,         DTU_RENDER_SCALE_MIN, DTU_RENDER_SCALE_MAX },
	{ "ImprovedOcclusion",   &Dtu_improved_occlusion,   0, 1 },
	{ "ImprovedBody",        &Dtu_improved_body,        0, 1 },
	{ "GpuRenderer",         &Dtu_gpu_renderer,         0, 1 },
};
#define N_OPTS	((int) (sizeof(Opts) / sizeof(Opts[0])))

void dtu_opts_load(void)
{
	FILE *f = fopen(DTU_OPTS_FILE, "r");
	char line[200];
	int i, t;

	if (f) {
		while (fgets(line, sizeof(line), f)) {
			char *eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq = 0;
			for (i = 0; i < N_OPTS; i++)
				if (!strcmp(line, Opts[i].name)) {
					int v = atoi(eq + 1);
					if (v < Opts[i].min) v = Opts[i].min;
					if (v > Opts[i].max) v = Opts[i].max;
					*Opts[i].value = v;
				}
		}
		fclose(f);
	}

	// The arguments win over the file.
	if (FindArg("-software"))
		Dtu_gpu_renderer = 0;
	if ((t = FindArg("-renderscale")) != 0 && t + 1 < Num_args) {
		int v = atoi(Args[t + 1]);
		if (v >= DTU_RENDER_SCALE_MIN && v <= DTU_RENDER_SCALE_MAX)
			Dtu_render_scale = v;
	}
	if (FindArg("-noimprovements")) {
		Dtu_improved_physics = Dtu_improved_jump = Dtu_improved_pathfinding = 0;
		Dtu_improved_occlusion = Dtu_improved_body = 0;
	}
}

void dtu_opts_save(void)
{
	FILE *f = fopen(DTU_OPTS_FILE, "w");
	int i;

	if (!f)
		return;
	for (i = 0; i < N_OPTS; i++)
		fprintf(f, "%s=%d\n", Opts[i].name, *Opts[i].value);
	fclose(f);
}
