/*
 * ASCENT: the "Improvements" settings. See dtuopts.h.
 *
 * The configuration file is ascent.cfg in the current directory (a run with
 * -datadir) or in the directory of the user (dtu_user_path). It is a
 * text file with one "name=value" line for each setting. The port never
 * writes to the game directories.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "dtuopts.h"
#include "args.h"
#include "types.h"
#include "fix.h"
#include "kconfig.h"		// the key table, written to the same file
#include "dtufile.h"		// ASCENT: dtu_user_path, dtu_install_dir
#include "dtulog.h"		// ASCENT: the session log

int Dtu_improved_physics = 1;
int Dtu_improved_jump = 1;
int Dtu_improved_pathfinding = 1;
int Dtu_render_scale = 3;
int Dtu_improved_occlusion = 1;
int Dtu_improved_body = 1;
int Dtu_gpu_renderer = 1;
int Dtu_upscaled_textures = 1;
int Dtu_upscaled_movies = 1;
int Dtu_anisotropic = 1;
int Dtu_shader = DTU_SHADER_SUBTLE;
int Dtu_safe_music = 0;
int Dtu_anim_interp = 1;
int Dtu_improved_infravision = 1;
int Dtu_target_hint = 1;

#define DTU_OPTS_FILE	"ascent.cfg"

// The values from the file of the settings that an argument changed, and
// the values of the arguments. The save writes the value of the file while
// the setting still has the value of the argument, so that an argument is
// not kept for the next start. A change in a menu is kept.
static int File_gpu_renderer = -1, File_render_scale = -1;
static int Arg_gpu_renderer = -1, Arg_render_scale = -1;

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
	{ "UpscaledTextures",    &Dtu_upscaled_textures,    0, 1 },
	{ "UpscaledMovies",      &Dtu_upscaled_movies,      0, 1 },
	{ "Anisotropic",         &Dtu_anisotropic,          0, 1 },
	{ "Shader",              &Dtu_shader,               DTU_SHADER_OFF, DTU_SHADER_VGA },
	{ "SafeMusic",           &Dtu_safe_music,           0, 1 },
	{ "AnimInterpolation",   &Dtu_anim_interp,          0, 1 },
	{ "ImprovedInfravision", &Dtu_improved_infravision, 0, 1 },
	{ "TargetHint",          &Dtu_target_hint,          0, 1 },
};
#define N_OPTS	((int) (sizeof(Opts) / sizeof(Opts[0])))

void dtu_opts_load(void)
{
	char path[600];
	FILE *f = fopen(dtu_user_path(path, sizeof(path), DTU_OPTS_FILE), "r");	// ASCENT: the directory of the user
	char line[200];
	int i, t;

	if (f) {
		while (fgets(line, sizeof(line), f)) {
			char *eq = strchr(line, '=');
			if (!eq)
				continue;
			*eq = 0;
			{
				// the key table (kconfig.c): the value without the line end
				char *v = eq + 1, *e = v + strlen(v);
				while (e > v && (e[-1] == '\n' || e[-1] == '\r'))
					*--e = 0;
				if (dtu_kconfig_cfg_line(line, v))
					continue;
			}
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
	if (FindArg("-software")) {
		File_gpu_renderer = Dtu_gpu_renderer;
		Dtu_gpu_renderer = Arg_gpu_renderer = 0;
	}
	if ((t = FindArg("-renderscale")) != 0 && t + 1 < Num_args) {
		int v = atoi(Args[t + 1]);
		if (v >= DTU_RENDER_SCALE_MIN && v <= DTU_RENDER_SCALE_MAX) {
			File_render_scale = Dtu_render_scale;
			Dtu_render_scale = Arg_render_scale = v;
		}
	}
	dtu_kconfig_args();		// -keyconfig, for the tests
	if (FindArg("-noimprovements")) {
		Dtu_improved_physics = Dtu_improved_jump = Dtu_improved_pathfinding = 0;
		Dtu_improved_occlusion = Dtu_improved_body = 0;
		Dtu_improved_infravision = 0;
		Dtu_target_hint = 0;
	}
	if (FindArg("-notargethint"))		// a comparison with the original cursor
		Dtu_target_hint = 0;
	if (FindArg("-noimprovedinfra"))		// a comparison with the original infravision
		Dtu_improved_infravision = 0;
	if (FindArg("-noaniminterp"))		// a comparison with the original animation
		Dtu_anim_interp = 0;
}

void dtu_opts_save(void)
{
	char path[600];
	FILE *f = fopen(dtu_user_path(path, sizeof(path), DTU_OPTS_FILE), "w");	// ASCENT: the directory of the user
	int i;

	if (!f)
		return;
	for (i = 0; i < N_OPTS; i++) {
		int v = *Opts[i].value;
		if (Opts[i].value == &Dtu_gpu_renderer && File_gpu_renderer >= 0 && v == Arg_gpu_renderer)
			v = File_gpu_renderer;
		if (Opts[i].value == &Dtu_render_scale && File_render_scale >= 0 && v == Arg_render_scale)
			v = File_render_scale;
		fprintf(f, "%s=%d\n", Opts[i].name, v);
	}
	dtu_kconfig_cfg_write(f);		// ASCENT: the key table (kconfig.c)
	fclose(f);
}

// ASCENT: the settings in the session log (dtulog.h)
void dtu_opts_log(void)
{
	char line[1024];
	int i, n;

	n = snprintf(line, sizeof(line), "Settings (ascent.cfg and arguments):");
	for (i = 0; i < N_OPTS && n < (int) sizeof(line) - 2; i++)
		n += snprintf(line + n, sizeof(line) - n, " %s=%d", Opts[i].name, *Opts[i].value);
	dtulog_event("%s", line);
}

static int is_dir(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

const char *dtu_hd_dir(void)
{
	static char dir[600];
	static int done;
	int i, n;

	if (done)
		return dir[0] ? dir : NULL;
	done = 1;
	dir[0] = 0;
	if ((i = FindArg("-hddir")) != 0 && i + 1 < Num_args && is_dir(Args[i + 1]))
		snprintf(dir, sizeof(dir), "%s", Args[i + 1]);
	else if (!dtu_install_dir() && is_dir("hd"))	// ASCENT: an installed game: only next to the executable
		snprintf(dir, sizeof(dir), "hd");
	else
		for (i = 1; i < Num_args - 1; i++)
			if (!stricmp(Args[i], "-datadir")) {
				char path[sizeof(dir)];
				snprintf(path, sizeof(path), "%s/hd", Args[i + 1]);
				if (is_dir(path)) {
					strcpy(dir, path);
					break;
				}
			}
	// ASCENT: "hd" next to the executable of an installed game
	if (!dir[0] && dtu_install_dir()) {
		char path[sizeof(dir)];
		snprintf(path, sizeof(path), "%s/hd", dtu_install_dir());
		if (is_dir(path))
			strcpy(dir, path);
	}
	// Then "hd" in the parent directories of the current directory, up to
	// four levels (for example gamedata/hd from gamedata/run). Not for an
	// installed game.
	if (!dir[0] && !dtu_install_dir()) {
		char path[sizeof(dir)] = "..";
		for (i = 0; i < 4 && !dir[0]; i++) {
			char test[sizeof(dir) + 4];
			snprintf(test, sizeof(test), "%s/hd", path);
			if (is_dir(test))
				snprintf(dir, sizeof(dir), "%s", test);
			else
				strncat(path, "/..", sizeof(path) - strlen(path) - 1);
		}
	}
	// no separator at the end
	n = (int) strlen(dir);
	while (n > 1 && (dir[n - 1] == '/' || dir[n - 1] == '\\'))
		dir[--n] = 0;
	return dir[0] ? dir : NULL;
}
