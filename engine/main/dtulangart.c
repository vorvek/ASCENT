/*
 * ASCENT: new file. The pictures of the language, with no part in
 * DTUGAME.EXE.
 *
 * DTUGAME.EXE has the button sheets of the play screen and some front end
 * bitmaps for each language (FEBGND 70 + language, 78 + language; the
 * PORTAL.EGG bitmaps whose last letter is the language letter), but the
 * Spanish ones on the disc are empty templates. The other interface
 * pictures have the English words painted into them. tools/ui_lang.py
 * makes the pictures of a language from the English art:
 *
 *   <lang dir>/<code>/orig/febgnd_<n>.png  the 8-bit picture n (a patch)
 *   <lang dir>/<code>/orig/egg_<name>.png  a bitmap of PORTAL.EGG (a patch)
 *   <lang dir>/<code>/ui/febgnd_<n>.png    the upscaled picture (RGBA, over
 *                                          <hd>/ui/febgnd_<n>.png)
 *
 * A patch is a PNG of the size of the picture, gray and alpha: the gray
 * value is the palette index, a pixel with alpha 0 keeps the pixel of the
 * game file. The lang dir is that of dtu_lang_path (the hd directory
 * for the content pack). English has no files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gr.h"
#include "dtulang.h"
#include "dtulangart.h"

// engine/platform/gputex.c (stb_image): RGBA, malloc memory
extern unsigned char *gputex_load_png(const char *path, int *w, int *h);

int dtu_lang_art_index(void)
{
	return Dtu_language == DTU_LANG_ES ? 4 : 0;
}

static char *art_path(char *out, int size, const char *kind, const char *file)
{
	char rel[128];

	if (Dtu_language == DTU_LANG_EN)
		return NULL;
	snprintf(rel, sizeof(rel), "%s/%s/%s", dtu_lang_code(Dtu_language), kind, file);
	return dtu_lang_path(out, size, rel);
}

static int patch(grs_bitmap *bm, const char *path)
{
	unsigned char *rgba;
	int w = 0, h = 0, x, y;

	if (!bm || !bm->bm_data || !path)
		return 0;
	rgba = gputex_load_png(path, &w, &h);
	if (!rgba)
		return 0;
	if (w != bm->bm_w || h != bm->bm_h) {
		fprintf(stderr, "%s is %dx%d, not %dx%d: not used\n", path, w, h, bm->bm_w, bm->bm_h);
		free(rgba);
		return 0;
	}
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			const unsigned char *p = rgba + (y * w + x) * 4;
			if (p[3] >= 128)
				bm->bm_data[y * bm->bm_rowsize + x] = p[0];
		}
	free(rgba);
	return 1;
}

int dtu_lang_art_has_febgnd(int n)
{
	char file[32], path[700];

	snprintf(file, sizeof(file), "febgnd_%d.png", n);
	return art_path(path, sizeof(path), "orig", file) != NULL;
}

int dtu_lang_art_febgnd(grs_bitmap *bm, int n)
{
	char file[32], path[700];

	snprintf(file, sizeof(file), "febgnd_%d.png", n);
	return patch(bm, art_path(path, sizeof(path), "orig", file));
}

// The front end asks for some bitmaps in each frame: the answers are kept
// (for the language and the name).
#define HAS_MEMO	64
static struct { char name[13]; int lang, has; } Has_memo[HAS_MEMO];
static int N_has_memo;

int dtu_lang_art_has_egg(const char *name)
{
	char file[32], path[700];
	int i, has;

	for (i = 0; i < N_has_memo; i++)
		if (Has_memo[i].lang == Dtu_language && !strcmp(Has_memo[i].name, name))
			return Has_memo[i].has;
	snprintf(file, sizeof(file), "egg_%.12s.png", name);
	has = art_path(path, sizeof(path), "orig", file) != NULL;
	if (strlen(name) <= 12) {
		i = N_has_memo < HAS_MEMO ? N_has_memo++ : 0;
		strcpy(Has_memo[i].name, name);
		Has_memo[i].lang = Dtu_language;
		Has_memo[i].has = has;
	}
	return has;
}

int dtu_lang_art_egg(grs_bitmap *bm, const char *name)
{
	char file[32], path[700];

	snprintf(file, sizeof(file), "egg_%.12s.png", name);
	return patch(bm, art_path(path, sizeof(path), "orig", file));
}

char *dtu_lang_art_ui(char *out, int size, int n)
{
	char file[32];

	snprintf(file, sizeof(file), "febgnd_%d.png", n);
	return art_path(out, size, "ui", file);
}
