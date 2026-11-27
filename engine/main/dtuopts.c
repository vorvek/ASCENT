/*
 * ASCENT: the "Improvements" settings. See dtuopts.h.
 *
 * The configuration file is ascent.cfg in the current directory (a run with
 * -datadir) or in the directory of the user (dtu_user_path). It is a
 * text file with one "name=value" line for each setting. The port never
 * writes to the game directories. An installed game first reads default.cfg
 * in the installation folder (same format, written by the setup program).
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
#include "dtulang.h"		// ASCENT: the language (Language=en)
#include "dtuitemfix.h"	// ASCENT: Dtu_rulebook_items, Dtu_fix_item_names
#include "dtuxp.h"		// ASCENT: Dtu_kill_xp_message, Dtu_max_hp_level_up
#include "dtugear.h"		// ASCENT: Dtu_new_gear

int Dtu_improved_physics = 1;
int Dtu_improved_jump = 1;
int Dtu_crouch = 1;
int Dtu_improved_ai = 1;
int Dtu_render_scale = 3;
int Dtu_improved_occlusion = 1;
int Dtu_improved_body = 1;
int Dtu_gpu_renderer = 1;
int Dtu_upscaled_textures = 1;
int Dtu_upscaled_movies = 1;
int Dtu_improved_models = 1;
int Dtu_anisotropic = 1;
int Dtu_ambient_occlusion = 0;
int Dtu_improved_lighting = 1;
int Dtu_shader = DTU_SHADER_SUBTLE;
int Dtu_safe_music = 0;
int Dtu_anim_interp = 1;
int Dtu_improved_infravision = 1;
int Dtu_target_hint = 1;
int Dtu_miss_sound = 1;			// ASCENT: dtumiss.c
int Dtu_fix_lowres_aspect = 0;		// ASCENT: 1: the 320x200 frame at 4:3 (dtuopts.h)

#define DTU_OPTS_FILE	"ascent.cfg"
#define DTU_DEFAULTS_FILE	"default.cfg"	// ASCENT: in the installation folder

// The values from the file of the settings that an argument changed, and
// the values of the arguments. The save writes the value of the file while
// the setting still has the value of the argument, so that an argument is
// not kept for the next start. A change in a menu is kept.
static int File_gpu_renderer = -1, File_render_scale = -1;
static int Arg_gpu_renderer = -1, Arg_render_scale = -1;
static int File_language = -1, Arg_language = -1;
static int File_rulebook = -1, Arg_rulebook = -1;
static int File_new_gear = -1, Arg_new_gear = -1;
static int File_lowres_aspect = -1, Arg_lowres_aspect = -1;
static int File_kill_xp = -1, Arg_kill_xp = -1;
static int File_max_hp = -1, Arg_max_hp = -1;
static int File_verbose = -1, Arg_verbose = -1;

typedef struct {
	const char *name;
	int *value;
	int min, max;
} dtu_opt;

static dtu_opt Opts[] = {
	{ "ImprovedPhysics",     &Dtu_improved_physics,     0, 1 },
	{ "ImprovedJump",        &Dtu_improved_jump,        0, 1 },
	{ "Crouch",              &Dtu_crouch,               0, 1 },
	{ "ImprovedAI",          &Dtu_improved_ai,          0, 1 },
	{ "RenderScale",         &Dtu_render_scale,         DTU_RENDER_SCALE_MIN, DTU_RENDER_SCALE_MAX },
	{ "ImprovedOcclusion",   &Dtu_improved_occlusion,   0, 1 },
	{ "ImprovedBody",        &Dtu_improved_body,        0, 1 },
	{ "GpuRenderer",         &Dtu_gpu_renderer,         0, 1 },
	{ "UpscaledTextures",    &Dtu_upscaled_textures,    0, 1 },
	{ "UpscaledMovies",      &Dtu_upscaled_movies,      0, 1 },
	{ "ImprovedModels",      &Dtu_improved_models,      0, 1 },
	{ "Anisotropic",         &Dtu_anisotropic,          0, 1 },
	{ "AmbientOcclusion",    &Dtu_ambient_occlusion,    0, 1 },
	{ "ImprovedLighting",    &Dtu_improved_lighting,    0, 1 },
	{ "Shader",              &Dtu_shader,               DTU_SHADER_OFF, DTU_SHADER_VGA },
	{ "SafeMusic",           &Dtu_safe_music,           0, 1 },
	{ "AnimInterpolation",   &Dtu_anim_interp,          0, 1 },
	{ "ImprovedInfravision", &Dtu_improved_infravision, 0, 1 },
	{ "TargetHint",          &Dtu_target_hint,          0, 1 },
	{ "MissSound",           &Dtu_miss_sound,           0, 1 },
	{ "RulebookItems",       &Dtu_rulebook_items,       0, 1 },		// dtuitemfix.h
	{ "NewGear",             &Dtu_new_gear,             0, 1 },		// dtugear.h
	// ASCENT: a new key: LowresAspect=1 (the default of one day) is ignored,
	// so a file of that time gets square pixels
	{ "Lowres43",            &Dtu_fix_lowres_aspect,    0, 1 },
	{ "KillXpMessage",       &Dtu_kill_xp_message,      0, 1 },		// dtuxp.h
	{ "MaxHpOnLevelUp",      &Dtu_max_hp_level_up,      0, 1 },		// dtuxp.h
	{ "VerboseCombatLog",    &Dtu_verbose_combat,       0, 1 },		// dtuxp.h
};
#define N_OPTS	((int) (sizeof(Opts) / sizeof(Opts[0])))

// Reads one settings file: "name=value" lines.
static void read_opts_file(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[200];
	int i;

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
				// ASCENT: the language of the text (dtulang.h), a code
				if (!strcmp(line, "Language")) {
					int l = dtu_lang_from_code(v);
					if (l >= 0)
						Dtu_language = l;
					continue;
				}
			}
			// ASCENT: "Improved pathfinding" is "Improved AI" now: a file with
			// the old key keeps its value
			if (!strcmp(line, "ImprovedPathfinding"))
				Dtu_improved_ai = atoi(eq + 1) != 0;
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
}

void dtu_opts_load(void)
{
	char path[600];
	int t;

	// ASCENT: the defaults of the installation (default.cfg next to the
	// executable, written by the setup program, for example Language=es),
	// then the file of the user, which wins.
	if (dtu_install_dir()) {
		snprintf(path, sizeof(path), "%s/%s", dtu_install_dir(), DTU_DEFAULTS_FILE);
		read_opts_file(path);
	}
	read_opts_file(dtu_user_path(path, sizeof(path), DTU_OPTS_FILE));	// ASCENT: the directory of the user

	// The arguments win over the file.
	if (FindArg("-software")) {
		File_gpu_renderer = Dtu_gpu_renderer;
		Dtu_gpu_renderer = Arg_gpu_renderer = 0;
	}
	if ((t = FindArg("-renderscale")) != 0 && t + 1 < Num_args) {
		const char *a = Args[t + 1];
		// ASCENT: 0.5, 1/2 or half: the step x1/2 (0)
		int v = !strcmp(a, "0.5") || !strcmp(a, "1/2") || !strcmp(a, "half") ? 0
			: strspn(a, "0123456789") == strlen(a) && a[0] ? atoi(a) : -1;
		if (v >= DTU_RENDER_SCALE_MIN && v <= DTU_RENDER_SCALE_MAX) {
			File_render_scale = Dtu_render_scale;
			Dtu_render_scale = Arg_render_scale = v;
		}
	}
	dtu_kconfig_args();		// -keyconfig, for the tests
	if ((t = FindArg("-language")) != 0 && t + 1 < Num_args && dtu_lang_from_code(Args[t + 1]) >= 0) {
		File_language = Dtu_language;
		Dtu_language = Arg_language = dtu_lang_from_code(Args[t + 1]);
	}
	if (FindArg("-noimprovements")) {
		Dtu_improved_physics = Dtu_improved_jump = Dtu_improved_ai = Dtu_crouch = 0;
		Dtu_improved_occlusion = Dtu_improved_body = 0;
		Dtu_improved_infravision = 0;
		Dtu_target_hint = 0;
		Dtu_miss_sound = 0;
		Dtu_rulebook_items = 0;
		Dtu_new_gear = 0;		// ASCENT: dtugear.h
		Dtu_kill_xp_message = 0;		// ASCENT: dtuxp.h
	}
	// ASCENT: dtuxp.h. -killxpmessage and -nokillxpmessage set the kill line
	// with the experience for one run, -maxhplevelup and -nomaxhplevelup
	// "Maximum HP on level up".
	if (FindArg("-killxpmessage") || FindArg("-nokillxpmessage")) {
		File_kill_xp = Dtu_kill_xp_message;
		Dtu_kill_xp_message = Arg_kill_xp = FindArg("-killxpmessage") && !FindArg("-nokillxpmessage");
	}
	if (FindArg("-maxhplevelup") || FindArg("-nomaxhplevelup")) {
		File_max_hp = Dtu_max_hp_level_up;
		Dtu_max_hp_level_up = Arg_max_hp = FindArg("-maxhplevelup") && !FindArg("-nomaxhplevelup");
	}
	// -verbosecombat and -noverbosecombat: "Verbose combat log" for one run
	if (FindArg("-verbosecombat") || FindArg("-noverbosecombat")) {
		File_verbose = Dtu_verbose_combat;
		Dtu_verbose_combat = Arg_verbose = FindArg("-verbosecombat") && !FindArg("-noverbosecombat");
	}
	// ASCENT: the item tables (dtuitemfix.h). -rulebookitems and
	// -norulebookitems set "Rulebook items" for one run. -origitems gives
	// the item names and the item values of 1.3.
	if (FindArg("-rulebookitems") || FindArg("-norulebookitems") || FindArg("-origitems")) {
		File_rulebook = Dtu_rulebook_items;
		Dtu_rulebook_items = Arg_rulebook = FindArg("-rulebookitems") && !FindArg("-norulebookitems") && !FindArg("-origitems");
	}
	// ASCENT: the new gear (dtugear.h). -newgear and -nonewgear set "New
	// gear" for one run; -origitems clears it too.
	if (FindArg("-newgear") || FindArg("-nonewgear") || FindArg("-origitems")) {
		File_new_gear = Dtu_new_gear;
		Dtu_new_gear = Arg_new_gear = FindArg("-newgear") && !FindArg("-nonewgear") && !FindArg("-origitems");
	}
	if (FindArg("-origitems"))
		Dtu_fix_item_names = 0;
	// ASCENT: -squarepixels shows the 320x200 frame with square pixels and
	// -screen43 at 4:3 for one run (Dtu_fix_lowres_aspect)
	if (FindArg("-squarepixels") || FindArg("-screen43")) {
		File_lowres_aspect = Dtu_fix_lowres_aspect;
		Dtu_fix_lowres_aspect = Arg_lowres_aspect = !FindArg("-squarepixels");
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
		if (Opts[i].value == &Dtu_rulebook_items && File_rulebook >= 0 && v == Arg_rulebook)
			v = File_rulebook;
		if (Opts[i].value == &Dtu_new_gear && File_new_gear >= 0 && v == Arg_new_gear)
			v = File_new_gear;
		if (Opts[i].value == &Dtu_fix_lowres_aspect && File_lowres_aspect >= 0 && v == Arg_lowres_aspect)
			v = File_lowres_aspect;
		if (Opts[i].value == &Dtu_kill_xp_message && File_kill_xp >= 0 && v == Arg_kill_xp)
			v = File_kill_xp;
		if (Opts[i].value == &Dtu_max_hp_level_up && File_max_hp >= 0 && v == Arg_max_hp)
			v = File_max_hp;
		if (Opts[i].value == &Dtu_verbose_combat && File_verbose >= 0 && v == Arg_verbose)
			v = File_verbose;
		fprintf(f, "%s=%d\n", Opts[i].name, v);
	}
	{
		int l = Dtu_language;
		if (File_language >= 0 && l == Arg_language)
			l = File_language;
		fprintf(f, "Language=%s\n", dtu_lang_code(l));
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
	if (n < (int) sizeof(line) - 2)
		snprintf(line + n, sizeof(line) - n, " Language=%s", dtu_lang_code(Dtu_language));
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
