/*
 * ascent-import: makes the "data" directory of an installed ASCENT from
 * the game disc.
 *
 * This file is new. It replaces the DOS installer of the game
 * (INSTALL.EXE), which copied a part of the disc to the hard disk.
 *
 *   ascent-import --source PATH --dest DIR [--patch PATH] [--check]
 *
 * PATH is a folder (a CD drive, a copy of the disc, or the DTU folder of
 * the disc), a disc image (.cue, .bin, .iso), or a ZIP archive with one
 * of them inside (at any depth; an image first). An installed game is not
 * a source: it has only a part of the files. A disc image in a ZIP
 * archive is read as a stream: it is not extracted.
 *
 * The packed files of the disc (.DOG, .COW, .HEN) are copied as they are:
 * the game reads them. The files of the 1.3 patch (installer/patch13.txt)
 * come from the source if it has them, else from --patch (a ZIP archive,
 * also a self-extracting one, or a folder). docs/install.md has the list.
 *
 * The output lines: "PROGRESS <percent> <text>", "INFO <text>",
 * "SOURCE ...", "PATCHED yes|no" and "ERROR: <text>".
 * The exit status: 0 done, 1 usage, 2 no game data, 3 the patch is
 * needed, 4 the patch is not valid, 5 files are missing, 6 write error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/statvfs.h>
#endif

#include "inst.h"
#include "patch13_list.h"		// generated from installer/patch13.txt

enum { EXIT_USAGE = 1, EXIT_NO_DATA, EXIT_PATCH_NEEDED, EXIT_PATCH_BAD, EXIT_MISSING, EXIT_WRITE };

// A source of game files: a folder, the DTU folder of a disc image, or a
// folder or a disc image in a ZIP archive.
typedef struct {
	char path[INST_PATH];		// the folder, the image file or the ZIP archive
	char dir[INST_PATH];		// the folder with the files (not for an image)
	char inner[INST_PATH];		// ZIP: the image or the folder in the archive
	inst_iso iso;
	inst_zip zip;
	int is_image, is_zip;
	int n_cow;					// .COW files: a disc has 57
	inst_list files;			// folder: the names; image: built from iso
	int *iso_index;				// image: the index in iso.files
	int *zip_index;				// folder in a ZIP archive: the index in zip.e
} source;

typedef struct {
	char name[64];
	source *src;
	int index;					// in src->files
	int64_t size;
} plan_entry;

static plan_entry *Plan;
static int N_plan, Max_plan;
static int64_t Total_bytes, Done_bytes;
static int Last_percent = -1;

static void out(const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vprintf(fmt, a);
	va_end(a);
	putchar('\n');
	fflush(stdout);
}

static int fail(int code, const char *fmt, ...)
{
	char msg[2048];
	va_list a;
	va_start(a, fmt);
	vsnprintf(msg, sizeof(msg), fmt, a);
	va_end(a);
	out("ERROR: %s", msg);
	return code;
}

static void progress_text(const char *text)
{
	int p = Total_bytes > 0 ? (int) (Done_bytes * 100 / Total_bytes) : 0;
	if (p > 100)
		p = 100;
	out("PROGRESS %d %s", p, text);
	Last_percent = p;
}

static void on_bytes(int64_t n, void *arg)
{
	Done_bytes += n;
	if (Total_bytes > 0 && Done_bytes * 100 / Total_bytes != Last_percent)
		progress_text((const char *) arg);
}

// ---------------------------------------------------------------------
// The files that the import leaves out: the DOS programs and drivers, the
// setup and install data, the configuration and the files of the player.

static int excluded(const char *name)
{
	static const char *ext[] = { "EXE", "COM", "BAT", "PIF", "386", "BNK", "HMP", "ICO", "INI",
		"CFG", "CFF", "LOG", "SG", "LVV", "TMP", NULL };
	static const char *names[] = { "INSTALL.DAT", "SETUP.DAT", "README.TXT", "DIGITEST.RAW",
		"GELDOR.DAT", "KCONFIG.DAT", "SETUPE.TEX", NULL };
	const char *e = inst_ext(name);
	int i;

	// The icon of the game: the window, the shortcuts and the uninstall
	// entry use it.
	if (!inst_stricmp(name, "DTU.ICO"))
		return 0;
	for (i = 0; ext[i]; i++)
		if (!inst_stricmp(e, ext[i]))
			return 1;
	for (i = 0; names[i]; i++)
		if (!inst_stricmp(name, names[i]))
			return 1;
	{
		char u[16];
		for (i = 0; i < 15 && name[i]; i++)
			u[i] = (name[i] >= 'a' && name[i] <= 'z') ? name[i] - 32 : name[i];
		u[i] = 0;
		if (!strncmp(u, "THESTORE.", 9) || !strncmp(u, "KCFG.", 5))
			return 1;
	}
	if (!inst_stricmp(e, ""))
		return 1;
	return 0;
}

// ---------------------------------------------------------------------
// Sources

static int count_ext(inst_list *l, const char *ext)
{
	int i, n = 0;
	for (i = 0; i < l->n; i++)
		if (!inst_stricmp(inst_ext(l->name[i]), ext))
			n++;
	return n;
}

static int has_name(inst_list *l, const char *name)
{
	int i;
	for (i = 0; i < l->n; i++)
		if (!inst_stricmp(l->name[i], name))
			return i;
	return -1;
}

// Open a folder: the folder itself or its DTU folder. 1 if it has DTU.MSN.
static int open_folder(source *s, const char *path)
{
	char sub[INST_PATH];

	memset(s, 0, sizeof(*s));
	snprintf(s->path, sizeof(s->path), "%s", path);
	inst_join(sub, sizeof(sub), path, "DTU");
	if (inst_is_dir(sub)) {
		char msn[INST_PATH];
		inst_join(msn, sizeof(msn), sub, "DTU.MSN");
		if (inst_is_file(msn))
			snprintf(s->dir, sizeof(s->dir), "%s", sub);
	}
	if (!s->dir[0])
		snprintf(s->dir, sizeof(s->dir), "%s", path);
	if (!inst_list_dir(s->dir, &s->files) || has_name(&s->files, "DTU.MSN") < 0) {
		inst_list_free(&s->files);
		return 0;
	}
	s->n_cow = count_ext(&s->files, "COW");
	return 1;
}

// The files of the DTU folder of the opened image. 1 if it has DTU.MSN.
static int image_files(source *s, const char *name, char *err, int err_size)
{
	int i;

	s->is_image = 1;
	s->iso_index = malloc(sizeof(int) * (s->iso.n + 1));
	for (i = 0; i < s->iso.n; i++) {
		const char *p = s->iso.files[i].path;
		if (!inst_stricmp(p, "DTU/") || strncmp(p, "DTU/", 4) || strchr(p + 4, '/'))
			continue;
		s->iso_index[s->files.n] = i;
		{
			inst_list *l = &s->files;
			if (l->n == l->max) {
				l->max = l->max ? l->max * 2 : 1024;
				l->name = realloc(l->name, l->max * sizeof(char *));
				l->size = realloc(l->size, l->max * sizeof(int64_t));
			}
			l->name[l->n] = strdup(p + 4);
			l->size[l->n] = s->iso.files[i].size;
			l->n++;
		}
	}
	if (has_name(&s->files, "DTU.MSN") < 0) {
		snprintf(err, err_size, "The image %s has no Descent to Undermountain data (no DTU\\DTU.MSN).", name);
		return 0;
	}
	s->n_cow = count_ext(&s->files, "COW");
	return 1;
}

static int open_image(source *s, const char *path, char *err, int err_size)
{
	memset(s, 0, sizeof(*s));
	snprintf(s->path, sizeof(s->path), "%s", path);
	if (!inst_iso_open(&s->iso, path, err, err_size))
		return 0;
	return image_files(s, path, err, err_size);
}

// Undo an image or a folder of a ZIP archive (the archive stays open).
static void zip_source_reset(source *s)
{
	if (s->is_image)
		inst_iso_close(&s->iso);
	inst_list_free(&s->files);
	free(s->iso_index);
	free(s->zip_index);
	s->iso_index = s->zip_index = NULL;
	s->is_image = 0;
	s->n_cow = 0;
	s->inner[0] = 0;
}

// The folder part of a name in a ZIP archive, with the last '/'.
static void zip_dir(const char *name, char *dir, int size)
{
	const char *p = strrchr(name, '/');
	int n = p ? (int) (p - name + 1) : 0;
	snprintf(dir, size, "%.*s", n, name);
}

static const char *zip_base(const char *name)
{
	const char *p = strrchr(name, '/');
	return p ? p + 1 : name;
}

// Entry i of the archive as a disc image. A .cue names its .bin. 1 if it
// is the game disc (DTU.MSN and .COW files).
static int zip_try_image(source *s, int i, char *err, int err_size)
{
	inst_zip *z = &s->zip;
	const char *name = z->e[i].name;
	int img = i, sector_size = 0, data_offset = 0;

	if (!inst_stricmp(inst_ext(name), "cue")) {
		char bin[INST_PATH], want[INST_PATH * 2], dir[INST_PATH];
		char *text = inst_zip_read(z, i, NULL), *q;
		int rc, k;
		if (!text) {
			snprintf(err, err_size, "Cannot read %s in the ZIP archive.", name);
			return 0;
		}
		rc = inst_cue_parse(text, bin, sizeof(bin), &sector_size, &data_offset);
		free(text);
		if (rc <= 0) {
			if (rc < 0)
				snprintf(err, err_size, "The first track of %s is not a data track.", name);
			else
				snprintf(err, err_size, "%s is not a cue sheet with a data track.", name);
			return 0;
		}
		for (q = bin; *q; q++)
			if (*q == '\\')
				*q = '/';
		// The name is relative to the folder of the cue sheet; else any
		// entry with the same file name
		zip_dir(name, dir, sizeof(dir));
		snprintf(want, sizeof(want), "%s%s", dir, bin);
		img = inst_zip_find(z, want);
		for (k = 0; k < z->n && img < 0; k++)
			if (!inst_stricmp(zip_base(z->e[k].name), zip_base(bin)))
				img = k;
		if (img < 0) {
			snprintf(err, err_size, "The cue sheet %.400s names %.400s, which is not in the ZIP archive.", name, bin);
			return 0;
		}
	}
	snprintf(s->inner, sizeof(s->inner), "%s", z->e[img].name);
	if (!inst_iso_open_zip(&s->iso, z, img, sector_size, data_offset, z->e[img].name, err, err_size))
		return 0;
	s->is_image = 1;
	if (!image_files(s, z->e[img].name, err, err_size))
		return 0;
	if (s->n_cow == 0) {
		snprintf(err, err_size, "The image %s in the ZIP archive has no .COW files: it is not the game disc.",
			z->e[img].name);
		return 0;
	}
	return 1;
}

// The files of folder dir of the archive (not of its subfolders).
static void zip_folder_files(source *s, const char *dir)
{
	inst_zip *z = &s->zip;
	inst_list *l = &s->files;
	int i, n = (int) strlen(dir);

	s->zip_index = malloc(sizeof(int) * (z->n + 1));
	for (i = 0; i < z->n; i++) {
		const char *name = z->e[i].name;
		if (strncmp(name, dir, n) || !name[n] || strchr(name + n, '/'))
			continue;
		if (l->n == l->max) {
			l->max = l->max ? l->max * 2 : 1024;
			l->name = realloc(l->name, l->max * sizeof(char *));
			l->size = realloc(l->size, l->max * sizeof(int64_t));
		}
		s->zip_index[l->n] = i;
		l->name[l->n] = strdup(name + n);
		l->size[l->n] = (int64_t) z->e[i].usize;
		l->n++;
	}
	s->n_cow = count_ext(l, "COW");
}

// A ZIP archive: a disc image in it (.iso, .cue with its .bin, or a .bin
// of 16 MB or more), else a copy of the disc in it (a folder with DTU.MSN
// and .COW files). An installed game in it is not a source.
static int open_zip(source *s, const char *path, char *err, int err_size)
{
	static const char *image_ext[] = { "iso", "img", "cue", "bin", NULL };
	inst_zip *z = &s->zip;
	char first_err[INST_PATH + 200] = "", installed[INST_PATH] = "";
	int i, k, best = -1, best_cow = 0;

	memset(s, 0, sizeof(*s));
	snprintf(s->path, sizeof(s->path), "%s", path);
	s->is_zip = 1;
	if (!inst_zip_open(z, path)) {
		snprintf(err, err_size, "%s is not a ZIP archive, or it is damaged.", path);
		return 0;
	}
	for (k = 0; image_ext[k]; k++)
		for (i = 0; i < z->n; i++) {
			const char *name = z->e[i].name;
			if (inst_stricmp(inst_ext(name), image_ext[k]))
				continue;
			// A small .BIN is a file of the game (a bitmap table), not an image.
			if (!strcmp(image_ext[k], "bin") && z->e[i].usize < (16u << 20))
				continue;
			if (zip_try_image(s, i, err, err_size))
				return 1;
			if (!first_err[0])
				snprintf(first_err, sizeof(first_err), "%s", err);
			zip_source_reset(s);
		}
	// A copy of the disc: the folder with DTU.MSN and the most .COW files
	for (i = 0; i < z->n; i++) {
		char dir[INST_PATH];
		if (inst_stricmp(zip_base(z->e[i].name), "DTU.MSN"))
			continue;
		zip_dir(z->e[i].name, dir, sizeof(dir));
		zip_folder_files(s, dir);
		if (s->n_cow > best_cow) {
			best = i;
			best_cow = s->n_cow;
		} else if (s->n_cow == 0 && !installed[0])
			snprintf(installed, sizeof(installed), "%s", dir[0] ? dir : "/");
		zip_source_reset(s);
	}
	if (best >= 0) {
		zip_dir(z->e[best].name, s->inner, sizeof(s->inner));
		zip_folder_files(s, s->inner);
		return 1;
	}
	if (first_err[0])
		snprintf(err, err_size, "%s", first_err);
	else if (installed[0])
		snprintf(err, err_size, "The ZIP archive %s is not the game disc: it has an installed game (%s) "
			"but no disc image and no copy of the disc. Choose the game disc, a disc image, "
			"or a ZIP archive with one of them, not an installed game.", path, installed);
	else
		snprintf(err, err_size, "The ZIP archive %s has no Descent to Undermountain disc "
			"(no disc image and no DTU.MSN).", path);
	inst_zip_close(z);
	return 0;
}

static int open_source(source *s, const char *path, char *err, int err_size)
{
	const char *e = inst_ext(path);

	err[0] = 0;
	if (inst_is_dir(path)) {
		if (open_folder(s, path))
			return 1;
		snprintf(err, err_size, "The folder %s has no Descent to Undermountain data (no DTU.MSN).", path);
		return 0;
	}
	if (!inst_is_file(path)) {
		snprintf(err, err_size, "%s does not exist.", path);
		return 0;
	}
	if (!inst_stricmp(e, "cue") || !inst_stricmp(e, "iso") || !inst_stricmp(e, "bin") || !inst_stricmp(e, "img"))
		return open_image(s, path, err, err_size);
	if (!inst_stricmp(e, "zip"))
		return open_zip(s, path, err, err_size);
	snprintf(err, err_size, "%s is not a folder, a disc image (.cue, .bin, .iso) or a ZIP archive.", path);
	return 0;
}

static int copy_from(source *s, int i, const char *to, const char *text)
{
	if (s->is_image)
		return inst_iso_extract(&s->iso, &s->iso.files[s->iso_index[i]], to, on_bytes, (void *) text);
	if (s->is_zip) {
		char hex[65];
		return inst_zip_extract(&s->zip, s->zip_index[i], to, hex, on_bytes, (void *) text);
	}
	{
		char from[INST_PATH];
		int ok;
		inst_join(from, sizeof(from), s->dir, s->files.name[i]);
		ok = inst_copy_file(from, to);
		on_bytes(s->files.size[i], (void *) text);
		return ok;
	}
}

static int hash_of(source *s, int i, char hex[65])
{
	if (s->is_image) {
		static unsigned char buf[1 << 16];
		inst_iso_file *f = &s->iso.files[s->iso_index[i]];
		inst_sha256 c;
		uint32_t done = 0;
		inst_sha256_init(&c);
		while (done < f->size) {
			uint32_t k = f->size - done > sizeof(buf) ? sizeof(buf) : f->size - done;
			if (!inst_iso_read(&s->iso, f, done, buf, k))
				return 0;
			inst_sha256_add(&c, buf, k);
			done += k;
		}
		inst_sha256_hex(&c, hex);
		return 1;
	}
	if (s->is_zip)
		return inst_zip_sha256(&s->zip, s->zip_index[i], hex);
	{
		char p[INST_PATH];
		inst_join(p, sizeof(p), s->dir, s->files.name[i]);
		return inst_sha256_file(p, hex);
	}
}

// ---------------------------------------------------------------------
// The plan: one source for each output file

static void plan_set(source *s, int i)
{
	const char *name = s->files.name[i];
	char upper[64];
	int k;

	if (strlen(name) >= sizeof(upper) || excluded(name))
		return;
	for (k = 0; name[k]; k++)
		upper[k] = (name[k] >= 'a' && name[k] <= 'z') ? name[k] - 32 : name[k];
	upper[k] = 0;
	for (k = 0; k < N_plan; k++)
		if (!strcmp(Plan[k].name, upper))
			break;
	if (k == N_plan) {
		if (N_plan == Max_plan) {
			Max_plan = Max_plan ? Max_plan * 2 : 1024;
			Plan = realloc(Plan, Max_plan * sizeof(*Plan));
		}
		N_plan++;
	}
	snprintf(Plan[k].name, sizeof(Plan[k].name), "%s", upper);
	Plan[k].src = s;
	Plan[k].index = i;
	Plan[k].size = s->files.size[i];
}

static plan_entry *plan_find(const char *name)
{
	int k;
	for (k = 0; k < N_plan; k++)
		if (!inst_stricmp(Plan[k].name, name))
			return &Plan[k];
	return NULL;
}

// 1 if the planned files have all the files of the 1.3 patch.
static int plan_patched(void)
{
	int i;

	// The names and the sizes first: a disc has no .LV2 files, and the
	// hash of a file of an image in a ZIP archive is slow.
	for (i = 0; i < N_PATCH13; i++) {
		plan_entry *p = plan_find(Patch13[i].name);
		if (!p || p->size != Patch13[i].size)
			return 0;
	}
	for (i = 0; i < N_PATCH13; i++) {
		plan_entry *p = plan_find(Patch13[i].name);
		char hex[65];
		if (!p || p->size != Patch13[i].size || !hash_of(p->src, p->index, hex) || strcmp(hex, Patch13[i].sha256))
			return 0;
	}
	return 1;
}

// The order of the copy for an image in a ZIP archive: the order of the
// files in the image, so that the archive is read once from the start.
static int plan_by_lba(const void *a, const void *b)
{
	const plan_entry *x = a, *y = b;
	uint32_t lx = x->src->iso.files[x->src->iso_index[x->index]].lba;
	uint32_t ly = y->src->iso.files[y->src->iso_index[y->index]].lba;
	return lx < ly ? -1 : lx > ly;
}

// The free bytes of the disk of a folder, or -1 if unknown.
static int64_t free_bytes(const char *dir)
{
#ifdef _WIN32
	ULARGE_INTEGER avail;
	if (GetDiskFreeSpaceExA(dir, &avail, NULL, NULL))
		return (int64_t) avail.QuadPart;
#else
	struct statvfs v;
	if (statvfs(dir, &v) == 0)
		return (int64_t) v.f_bavail * (int64_t) v.f_frsize;
#endif
	return -1;
}

// ---------------------------------------------------------------------
// The patch: a ZIP archive or a folder

static int apply_patch(const char *patch, const char *dest)
{
	int i;

	if (inst_is_dir(patch)) {
		for (i = 0; i < N_PATCH13; i++) {
			char from[INST_PATH], to[INST_PATH], hex[65];
			if (!inst_find_name(patch, Patch13[i].name, from, sizeof(from)))
				return fail(EXIT_PATCH_BAD, "The patch folder %s has no %s.", patch, Patch13[i].name);
			if (!inst_sha256_file(from, hex) || strcmp(hex, Patch13[i].sha256))
				return fail(EXIT_PATCH_BAD, "%s of the patch folder is not the file of the 1.3 patch.", Patch13[i].name);
			inst_join(to, sizeof(to), dest, Patch13[i].name);
			if (!inst_copy_file(from, to))
				return fail(EXIT_WRITE, "Cannot write %s.", to);
			out("INFO patch file %s", Patch13[i].name);
		}
		return 0;
	} else {
		inst_zip z;
		if (!inst_zip_open(&z, patch))
			return fail(EXIT_PATCH_BAD, "%s is not a ZIP archive.", patch);
		for (i = 0; i < N_PATCH13; i++) {
			char to[INST_PATH], hex[65];
			int k, e = -1;
			// The name can be in a folder of the archive.
			for (k = 0; k < z.n && e < 0; k++) {
				const char *b = strrchr(z.e[k].name, '/');
				b = b ? b + 1 : z.e[k].name;
				if (!inst_stricmp(b, Patch13[i].name))
					e = k;
			}
			if (e < 0) {
				inst_zip_close(&z);
				return fail(EXIT_PATCH_BAD, "The patch archive has no %s.", Patch13[i].name);
			}
			inst_join(to, sizeof(to), dest, Patch13[i].name);
			if (!inst_zip_extract(&z, e, to, hex, NULL, NULL)) {
				inst_zip_close(&z);
				remove(to);
				return fail(EXIT_PATCH_BAD, "Cannot extract %s from the patch archive.", Patch13[i].name);
			}
			if (strcmp(hex, Patch13[i].sha256)) {
				inst_zip_close(&z);
				remove(to);
				return fail(EXIT_PATCH_BAD, "%s of the patch archive is not the file of the 1.3 patch (SHA-256 %s).",
					Patch13[i].name, hex);
			}
			out("INFO patch file %s", Patch13[i].name);
		}
		inst_zip_close(&z);
		return 0;
	}
}

// ---------------------------------------------------------------------

static void usage(void)
{
	printf("Usage: ascent-import --source PATH --dest DIR [--patch PATH] [--check]\n\n"
		"  --source  the game disc (a drive or a folder), a disc image (.cue, .bin, .iso),\n"
		"            or a ZIP archive with the disc or a disc image in it\n"
		"  --patch   the 1.3 patch: a ZIP archive or a folder\n"
		"  --dest    the data folder to make\n"
		"  --check   only examine the sources\n");
}

int main(int argc, char **argv)
{
	const char *src_path = NULL, *patch = NULL, *dest = NULL;
	static source src;
	source *full = &src;
	char err[INST_PATH + 200];
	int i, check = 0, patched, rc;
	static const char *core[] = { "DTU.MSN", "PALETTE.256", "FEBGND.DAT", "FEBGND.JRG", "FESTREAM.DAT",
		"FETXTE.DAT", "WERDZ.DAT", "IPHOF.TXT", "HMC.COW", "HMC.HEN", "YPINN.DOG", "YPINN.TIG", NULL };

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--source") && i + 1 < argc) src_path = argv[++i];
		else if (!strcmp(argv[i], "--patch") && i + 1 < argc) patch = argv[++i];
		else if (!strcmp(argv[i], "--dest") && i + 1 < argc) dest = argv[++i];
		else if (!strcmp(argv[i], "--check")) check = 1;
		else {
			usage();
			return EXIT_USAGE;
		}
	}
	if (!src_path || (!dest && !check)) {
		usage();
		return EXIT_USAGE;
	}

	if (!open_source(&src, src_path, err, sizeof(err)))
		return fail(EXIT_NO_DATA, "%s", err);
	// An installed game has no .COW files: the disc has them.
	if (src.n_cow == 0)
		return fail(EXIT_NO_DATA, "%s is not the game disc: it has no .COW files. "
			"Choose the game disc, a disc image, or a ZIP archive with one of them, not an installed game.",
			src.is_image ? src.path : src.dir);
	if (src.is_zip)
		out("SOURCE disc %s (%s)", src.path, src.inner[0] ? src.inner : "/");
	else
		out("SOURCE disc %s", src.is_image ? src.path : src.dir);
	if (full->n_cow < 57)
		out("INFO the disc has %d .COW files (57 expected)", full->n_cow);

	for (i = 0; i < full->files.n; i++)
		plan_set(full, i);
	for (i = 0; core[i]; i++)
		if (!plan_find(core[i]) && !(!strcmp(core[i], "YPINN.DOG") && plan_find("YPINN.LVL")))
			return fail(EXIT_MISSING, "The source has no %s. It is not a complete Descent to Undermountain.", core[i]);

	patched = plan_patched();
	out("PATCHED %s", patched ? "yes" : "no");
	Total_bytes = 0;
	for (i = 0; i < N_plan; i++)
		Total_bytes += Plan[i].size;
	out("FILES %d %lld", N_plan, (long long) Total_bytes);
	if (check)
		return 0;
	if (!patched && !patch)
		return fail(EXIT_PATCH_NEEDED, "The source does not have the 1.3 patch, and no patch archive was given.");

	if (!inst_mkdirs(dest))
		return fail(EXIT_WRITE, "Cannot make the folder %s.", dest);
	{
		int64_t free = free_bytes(dest), need = Total_bytes + (1 << 20);
		if (free >= 0 && free < need)
			return fail(EXIT_WRITE, "Not enough free space in %s: %lld MB needed, %lld MB free.", dest,
				(long long) (need >> 20) + 1, (long long) (free >> 20));
	}
	if (src.is_image && src.iso.zr)
		qsort(Plan, N_plan, sizeof(*Plan), plan_by_lba);
	for (i = 0; i < N_plan; i++) {
		char to[INST_PATH], text[200];
		snprintf(text, sizeof(text), "Copying %s", Plan[i].name);
		inst_join(to, sizeof(to), dest, Plan[i].name);
		if (!copy_from(Plan[i].src, Plan[i].index, to, text)) {
			remove(to);
			return fail(EXIT_WRITE, "Cannot copy %s to %s. Check the source and the free space.", Plan[i].name, dest);
		}
	}
	// The rest of the image in the ZIP archive: the CRC of the whole entry
	if (src.is_image && src.iso.zr) {
		progress_text("Checking the disc image in the ZIP archive");
		if (!inst_zip_reader_finish(src.iso.zr))
			return fail(EXIT_NO_DATA, "The disc image %s in the ZIP archive %s is damaged (CRC error).",
				src.inner, src.path);
	}
	progress_text("Copying the files of the 1.3 patch");
	if (!patched && (rc = apply_patch(patch, dest)) != 0)
		return rc;

	// The check of the result
	for (i = 0; i < N_PATCH13; i++) {
		char p[INST_PATH], hex[65];
		inst_join(p, sizeof(p), dest, Patch13[i].name);
		if (!inst_sha256_file(p, hex) || strcmp(hex, Patch13[i].sha256))
			return fail(EXIT_MISSING, "%s in %s is not the file of the 1.3 patch.", Patch13[i].name, dest);
	}
	for (i = 0; core[i]; i++) {
		char p[INST_PATH];
		if (!inst_find_name(dest, core[i], p, sizeof(p)) &&
			!(!strcmp(core[i], "YPINN.DOG") && inst_find_name(dest, "YPINN.LVL", p, sizeof(p))))
			return fail(EXIT_MISSING, "%s is missing in %s.", core[i], dest);
	}
	Done_bytes = Total_bytes;
	progress_text("Done");
	out("INFO %d files in %s", N_plan + (patched ? 0 : 0), dest);
	return 0;
}
