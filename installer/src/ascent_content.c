/*
 * ascent-content: installs, removes, checks and repairs the downloadable
 * content of ASCENT (the upscaled textures, interface and movies, the
 * Spanish text, the replacement music), and
 * downloads the 1.3 patch pack for the setup program.
 *
 * This file is new. The DOS game had no downloadable content.
 *
 * Without a command it opens a window ("ASCENT-DTU Content"). The commands:
 *   ascent-content [--root DIR] [--url URL] list
 *   ascent-content [--root DIR] [--url URL] download PACK FILE
 *   ascent-content [--root DIR] [--url URL] install PACK...
 *   ascent-content [--root DIR] remove PACK...
 *   ascent-content [--root DIR] verify PACK...
 *   ascent-content [--root DIR] [--url URL] repair PACK...
 *
 * DIR is the installed game (default: the folder of this program). The
 * server address is base_url of DIR/content.ini, or URL. The server has
 * manifest.txt and the pack files (docs/install.md: the formats).
 *
 * The output lines: "PROGRESS <percent> <text>", "INFO <text>",
 * "PACK ..." (list) and "ERROR: <text>". The exit status: 0 done,
 * 1 usage, 2 download failed, 3 no such pack or no server address,
 * 4 SHA-256 mismatch, 5 write or extract error, 7 the pack is damaged.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <fcntl.h>

#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>
#include <process.h>

#include "inst.h"

enum { EXIT_USAGE = 1, EXIT_DOWNLOAD, EXIT_NO_PACK, EXIT_HASH, EXIT_WRITE, EXIT_6, EXIT_DAMAGED };

#define MAX_PACKS 32

typedef struct {
	char name[32], version[32], target[16], file[128], sha256[65], title[160];
	int64_t size, files;
	// the installed state
	char installed[32];		// version, or "" if not installed
	int state;				// 0 unknown, 1 good, 2 damaged
} pack;

static char Root[INST_PATH];
static char Base_url[1024];
static char Header_name[64], Header_value[256], User_agent[128];
static pack Packs[MAX_PACKS];
static int N_packs;
static int Have_manifest;

static char Last_error[1024];

// The progress of the current job: the window reads it; the console
// mode prints it.
static volatile LONG Progress_percent;
static char Progress_text[256];
static int Gui;
static HWND Main_wnd;
#define WM_APP_PROGRESS		(WM_APP + 1)
#define WM_APP_DONE			(WM_APP + 2)

static void out(const char *fmt, ...)
{
	va_list a;
	if (Gui)
		return;
	va_start(a, fmt);
	vprintf(fmt, a);
	va_end(a);
	putchar('\n');
	fflush(stdout);
}

static int fail(int code, const char *fmt, ...)
{
	va_list a;
	va_start(a, fmt);
	vsnprintf(Last_error, sizeof(Last_error), fmt, a);
	va_end(a);
	out("ERROR: %s", Last_error);
	return code;
}

static void progress(int percent, const char *fmt, ...)
{
	va_list a;
	char text[256];

	va_start(a, fmt);
	vsnprintf(text, sizeof(text), fmt, a);
	va_end(a);
	if (percent < 0) percent = 0;
	if (percent > 100) percent = 100;
	if (Gui) {
		int changed = percent != Progress_percent || strcmp(text, Progress_text);
		Progress_percent = percent;
		snprintf(Progress_text, sizeof(Progress_text), "%s", text);
		if (changed)
			PostMessageA(Main_wnd, WM_APP_PROGRESS, 0, 0);
	} else
		out("PROGRESS %d %s", percent, text);
}

// ---------------------------------------------------------------------
// The configuration

static void read_config(const char *url_arg)
{
	char ini[INST_PATH];

	inst_join(ini, sizeof(ini), Root, "content.ini");
	GetPrivateProfileStringA("content", "base_url", "", Base_url, sizeof(Base_url), ini);
	GetPrivateProfileStringA("content", "client_header", "X-Ascent-Client", Header_name, sizeof(Header_name), ini);
	GetPrivateProfileStringA("content", "client_token", "", Header_value, sizeof(Header_value), ini);
	GetPrivateProfileStringA("content", "user_agent", "ASCENT-Setup/1.0", User_agent, sizeof(User_agent), ini);
	if (url_arg)
		snprintf(Base_url, sizeof(Base_url), "%s", url_arg);
	if (Base_url[0] && Base_url[strlen(Base_url) - 1] != '/')
		strncat(Base_url, "/", sizeof(Base_url) - strlen(Base_url) - 1);
}

// ---------------------------------------------------------------------
// HTTP (WinHTTP)

typedef struct {
	FILE *f;					// the output file, or
	char *mem; size_t mem_len, mem_max;	// the output in memory
	inst_sha256 sha;
	int64_t total, done;
	const char *what;
} http_sink;

static void wide(const char *s, wchar_t *w, int n)
{
	MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
}

static int http_get(const char *url, http_sink *sink)
{
	wchar_t wurl[2048], host[256], path[2048], wua[128], hdr[512];
	URL_COMPONENTSW uc;
	HINTERNET ses = NULL, con = NULL, req = NULL;
	DWORD status = 0, len = sizeof(status), avail, got;
	int ok = 0;
	static char buf[1 << 16];

	wide(url, wurl, 2048);
	memset(&uc, 0, sizeof(uc));
	uc.dwStructSize = sizeof(uc);
	uc.lpszHostName = host; uc.dwHostNameLength = 256;
	uc.lpszUrlPath = path; uc.dwUrlPathLength = 2048;
	if (!WinHttpCrackUrl(wurl, 0, 0, &uc))
		return fail(EXIT_DOWNLOAD, "The address %s is not valid.", url), 0;
	wide(User_agent, wua, 128);
	ses = WinHttpOpen(wua, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ses)
		ses = WinHttpOpen(wua, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!ses)
		goto done;
	WinHttpSetTimeouts(ses, 15000, 15000, 30000, 60000);
	con = WinHttpConnect(ses, host, uc.nPort, 0);
	if (!con)
		goto done;
	req = WinHttpOpenRequest(con, L"GET", path, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
	if (!req)
		goto done;
	hdr[0] = 0;
	if (Header_name[0] && Header_value[0]) {
		char h[400];
		snprintf(h, sizeof(h), "%s: %s", Header_name, Header_value);
		wide(h, hdr, 512);
	}
	if (!WinHttpSendRequest(req, hdr[0] ? hdr : WINHTTP_NO_ADDITIONAL_HEADERS, hdr[0] ? (DWORD) -1L : 0,
			WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(req, NULL))
		goto done;
	WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
		&status, &len, WINHTTP_NO_HEADER_INDEX);
	if (status != 200) {
		fail(EXIT_DOWNLOAD, "The server answered %lu for %s.", (unsigned long) status, url);
		status = 0xFFFFFFFF;
		goto done;
	}
	{
		wchar_t cl[32];
		DWORD cll = sizeof(cl);
		if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, cl, &cll, WINHTTP_NO_HEADER_INDEX) && sink->total <= 0)
			sink->total = _wtoi64(cl);
	}
	for (;;) {
		avail = 0;
		if (!WinHttpQueryDataAvailable(req, &avail))
			goto done;
		if (avail == 0)
			break;
		if (avail > sizeof(buf))
			avail = sizeof(buf);
		if (!WinHttpReadData(req, buf, avail, &got))
			goto done;
		if (got == 0)
			break;
		inst_sha256_add(&sink->sha, buf, got);
		if (sink->f) {
			if (fwrite(buf, 1, got, sink->f) != got) {
				fail(EXIT_WRITE, "Cannot write the download of %s (disk full?).", url);
				status = 0xFFFFFFFF;
				goto done;
			}
		} else {
			if (sink->mem_len + got + 1 > sink->mem_max) {
				size_t m = (sink->mem_max ? sink->mem_max * 2 : 65536) + got;
				char *n = realloc(sink->mem, m);
				if (!n)
					goto done;
				sink->mem = n;
				sink->mem_max = m;
			}
			memcpy(sink->mem + sink->mem_len, buf, got);
			sink->mem_len += got;
			sink->mem[sink->mem_len] = 0;
		}
		sink->done += got;
		if (sink->what && sink->total > 0)
			progress((int) (sink->done * 100 / sink->total), "Downloading %s: %lld of %lld MB", sink->what,
				(long long) (sink->done >> 20), (long long) (sink->total >> 20));
	}
	ok = 1;
done:
	if (!ok && status != 0xFFFFFFFF)
		fail(EXIT_DOWNLOAD, "Cannot download %s (error %lu). Check the Internet connection.", url, (unsigned long) GetLastError());
	if (req) WinHttpCloseHandle(req);
	if (con) WinHttpCloseHandle(con);
	if (ses) WinHttpCloseHandle(ses);
	return ok;
}

// ---------------------------------------------------------------------
// The manifest and the installed state

static pack *find_pack(const char *name)
{
	int i;
	for (i = 0; i < N_packs; i++)
		if (!inst_stricmp(Packs[i].name, name))
			return &Packs[i];
	return NULL;
}

static pack *add_pack(const char *name)
{
	pack *p = find_pack(name);
	if (p || N_packs == MAX_PACKS)
		return p;
	p = &Packs[N_packs++];
	memset(p, 0, sizeof(*p));
	snprintf(p->name, sizeof(p->name), "%s", name);
	return p;
}

static void state_path(char *out, int size, const char *name)
{
	char dir[INST_PATH], file[64];
	inst_join(dir, sizeof(dir), Root, "content/installed");
	snprintf(file, sizeof(file), "%s.txt", name);
	inst_join(out, size, dir, file);
}

// The installed packs (content/installed/<name>.txt: "version=...",
// "target=...", "title=...", then the file list).
static void read_installed(void)
{
	char dir[INST_PATH];
	inst_list l;
	int i;

	for (i = 0; i < N_packs; i++)
		Packs[i].installed[0] = 0;
	inst_join(dir, sizeof(dir), Root, "content/installed");
	if (!inst_list_dir(dir, &l))
		return;
	for (i = 0; i < l.n; i++) {
		char name[64], path[INST_PATH], line[512];
		FILE *f;
		pack *p;
		if (inst_stricmp(inst_ext(l.name[i]), "txt"))
			continue;
		snprintf(name, sizeof(name), "%s", l.name[i]);
		*strrchr(name, '.') = 0;
		inst_join(path, sizeof(path), dir, l.name[i]);
		if (!(f = fopen(path, "r")) || !(p = add_pack(name))) {
			if (f) fclose(f);
			continue;
		}
		while (fgets(line, sizeof(line), f)) {
			line[strcspn(line, "\r\n")] = 0;
			if (!strncmp(line, "version=", 8)) snprintf(p->installed, sizeof(p->installed), "%s", line + 8);
			else if (!strncmp(line, "target=", 7) && !p->target[0]) snprintf(p->target, sizeof(p->target), "%s", line + 7);
			else if (!strncmp(line, "title=", 6) && !p->title[0]) snprintf(p->title, sizeof(p->title), "%s", line + 6);
			else if (line[0] != '=' && strchr(line, '=') == NULL)
				break;
		}
		fclose(f);
	}
	inst_list_free(&l);
}

// manifest.txt: comment lines with '#', then one line for each pack with
// the fields separated by tabs: name, version, target, file, size,
// sha256, files, title.
static int load_manifest(void)
{
	http_sink s;
	char url[1200], *line, *next;

	if (!Base_url[0])
		return fail(EXIT_NO_PACK, "No content server is set (base_url in content.ini).");
	memset(&s, 0, sizeof(s));
	inst_sha256_init(&s.sha);
	snprintf(url, sizeof(url), "%smanifest.txt", Base_url);
	if (!http_get(url, &s)) {
		free(s.mem);
		return EXIT_DOWNLOAD;
	}
	for (line = s.mem; line && *line; line = next) {
		char *f[8];
		int n = 0;
		pack *p;
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		line[strcspn(line, "\r")] = 0;
		if (line[0] == '#' || line[0] == 0)
			continue;
		f[n++] = line;
		while (n < 8 && strchr(f[n - 1], '\t')) {
			char *t = strchr(f[n - 1], '\t');
			*t = 0;
			f[n++] = t + 1;
		}
		if (n < 8 || strlen(f[5]) != 64 || !(p = add_pack(f[0])))
			continue;
		snprintf(p->version, sizeof(p->version), "%s", f[1]);
		snprintf(p->target, sizeof(p->target), "%s", f[2]);
		snprintf(p->file, sizeof(p->file), "%s", f[3]);
		p->size = _atoi64(f[4]);
		snprintf(p->sha256, sizeof(p->sha256), "%s", f[5]);
		p->files = _atoi64(f[6]);
		snprintf(p->title, sizeof(p->title), "%s", f[7]);
	}
	free(s.mem);
	Have_manifest = 1;
	return 0;
}

// ---------------------------------------------------------------------
// Download, install, remove, verify

static int download_pack(pack *p, const char *to)
{
	http_sink s;
	char url[1400], part[INST_PATH + 8], hex[65];
	int ok;

	if (!p->sha256[0])
		return fail(EXIT_NO_PACK, "The server has no pack %s.", p->name);
	snprintf(part, sizeof(part), "%s.part", to);
	memset(&s, 0, sizeof(s));
	inst_sha256_init(&s.sha);
	s.total = p->size;
	s.what = p->title[0] ? p->title : p->name;
	s.f = fopen(part, "wb");
	if (!s.f)
		return fail(EXIT_WRITE, "Cannot write %s.", part);
	snprintf(url, sizeof(url), "%s%s", Base_url, p->file);
	ok = http_get(url, &s);
	if (fclose(s.f))
		ok = 0;
	if (!ok) {
		remove(part);
		return Last_error[0] ? EXIT_DOWNLOAD : fail(EXIT_DOWNLOAD, "Cannot download %s.", url);
	}
	inst_sha256_hex(&s.sha, hex);
	if (s.done != p->size || strcmp(hex, p->sha256)) {
		remove(part);
		return fail(EXIT_HASH, "The download of %s is damaged: SHA-256 %s, size %lld; expected %s, size %lld.",
			p->name, hex, (long long) s.done, p->sha256, (long long) p->size);
	}
	remove(to);
	if (rename(part, to) != 0) {
		remove(part);
		return fail(EXIT_WRITE, "Cannot write %s.", to);
	}
	out("INFO downloaded %s (%lld bytes, SHA-256 %s)", p->file, (long long) p->size, hex);
	return 0;
}

static void target_dir(const pack *p, char *out, int size)
{
	inst_join(out, size, Root, !inst_stricmp(p->target, "data") ? "data" : "hd");
}

// Remove the empty folders from dir up to (not including) stop.
static void prune_dirs(char *dir, const char *stop)
{
	while (strlen(dir) > strlen(stop) && RemoveDirectoryA(dir)) {
		char *s = strrchr(dir, '/'), *b = strrchr(dir, '\\');
		if (b > s) s = b;
		if (!s) break;
		*s = 0;
	}
}

static int remove_files(const pack *p, const char *list_path)
{
	char tdir[INST_PATH], line[1024];
	FILE *f = fopen(list_path, "r");

	if (!f)
		return 0;
	target_dir(p, tdir, sizeof(tdir));
	while (fgets(line, sizeof(line), f)) {
		char *rel, path[INST_PATH];
		line[strcspn(line, "\r\n")] = 0;
		if (strlen(line) < 68 || line[64] != ' ' || !(rel = strchr(line + 65, ' ')))
			continue;
		rel++;
		inst_join(path, sizeof(path), tdir, rel);
		SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
		DeleteFileA(path);
		{
			char d[INST_PATH], *s, *b;
			snprintf(d, sizeof(d), "%s", path);
			s = strrchr(d, '/'); b = strrchr(d, '\\');
			if (b > s) s = b;
			if (s) {
				*s = 0;
				prune_dirs(d, tdir);
			}
		}
	}
	fclose(f);
	return 1;
}

static int cmd_remove(pack *p)
{
	char sp[INST_PATH], tdir[INST_PATH];

	state_path(sp, sizeof(sp), p->name);
	if (!inst_is_file(sp))
		return fail(EXIT_NO_PACK, "The pack %s is not installed.", p->name);
	progress(0, "Removing %s", p->title[0] ? p->title : p->name);
	remove_files(p, sp);
	remove(sp);
	target_dir(p, tdir, sizeof(tdir));
	RemoveDirectoryA(tdir);			// only if it is empty
	p->installed[0] = 0;
	progress(100, "Removed %s", p->name);
	out("INFO removed %s", p->name);
	return 0;
}

static void on_extract(int64_t n, void *arg)
{
	int64_t *d = arg;
	d[0] += n;
	if (d[1] > 0 && (d[0] - n) * 100 / d[1] != d[0] * 100 / d[1])
		progress((int) (d[0] * 100 / d[1]), "Installing");
}

// A hash of a file with its identity: the hard links of a pack are
// checked once.
typedef struct { DWORD vol, hi, lo; char hex[65]; } id_hash;
static id_hash *Id_cache;
static int N_id, Max_id;

static int hash_cached(const char *path, char hex[65])
{
	BY_HANDLE_FILE_INFORMATION fi;
	HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL);
	int i, ok;

	if (h == INVALID_HANDLE_VALUE)
		return 0;
	ok = GetFileInformationByHandle(h, &fi);
	CloseHandle(h);
	if (ok)
		for (i = 0; i < N_id; i++)
			if (Id_cache[i].vol == fi.dwVolumeSerialNumber && Id_cache[i].hi == fi.nFileIndexHigh && Id_cache[i].lo == fi.nFileIndexLow) {
				memcpy(hex, Id_cache[i].hex, 65);
				return 1;
			}
	if (!inst_sha256_file(path, hex))
		return 0;
	if (ok) {
		if (N_id == Max_id) {
			Max_id = Max_id ? Max_id * 2 : 4096;
			Id_cache = realloc(Id_cache, Max_id * sizeof(*Id_cache));
		}
		Id_cache[N_id].vol = fi.dwVolumeSerialNumber;
		Id_cache[N_id].hi = fi.nFileIndexHigh;
		Id_cache[N_id].lo = fi.nFileIndexLow;
		memcpy(Id_cache[N_id].hex, hex, 65);
		N_id++;
	}
	return 1;
}

// Check the files of an installed pack. Returns 0 if good, EXIT_DAMAGED.
static int cmd_verify(pack *p, int quiet)
{
	char sp[INST_PATH], tdir[INST_PATH], line[1024];
	FILE *f;
	int n = 0, bad = 0, total = 0;

	state_path(sp, sizeof(sp), p->name);
	if (!(f = fopen(sp, "r")))
		return fail(EXIT_NO_PACK, "The pack %s is not installed.", p->name);
	target_dir(p, tdir, sizeof(tdir));
	while (fgets(line, sizeof(line), f))
		if (strlen(line) > 68 && line[64] == ' ')
			total++;
	rewind(f);
	N_id = 0;
	while (fgets(line, sizeof(line), f)) {
		char *rel, path[INST_PATH], hex[65];
		int64_t size;
		line[strcspn(line, "\r\n")] = 0;
		if (strlen(line) < 68 || line[64] != ' ' || !(rel = strchr(line + 65, ' ')))
			continue;
		size = _atoi64(line + 65);
		rel++;
		line[64] = 0;
		inst_join(path, sizeof(path), tdir, rel);
		if (inst_file_size(path) != size || !hash_cached(path, hex) || strcmp(hex, line)) {
			if (bad < 5)
				out("INFO damaged or missing: %s", rel);
			bad++;
		}
		if (++n % 64 == 0 || n == total)
			progress(total ? n * 100 / total : 100, "Checking %s: %d of %d files", p->name, n, total);
	}
	fclose(f);
	p->state = bad ? 2 : 1;
	if (bad) {
		if (!quiet)
			fail(EXIT_DAMAGED, "The pack %s has %d damaged or missing files of %d.", p->name, bad, n);
		return EXIT_DAMAGED;
	}
	out("INFO %s: %d files are good", p->name, n);
	return 0;
}

static int cmd_install(pack *p)
{
	char dl_dir[INST_PATH], zip_path[INST_PATH], tdir[INST_PATH], sp[INST_PATH];
	inst_zip z;
	char *files = NULL, *links = NULL, *line, *next;
	int i, rc;
	int64_t prog[2] = { 0, 0 };
	FILE *st;

	if (!Have_manifest && (rc = load_manifest()) != 0)
		return rc;
	if (!p->sha256[0])
		return fail(EXIT_NO_PACK, "The server has no pack %s.", p->name);
	inst_join(dl_dir, sizeof(dl_dir), Root, "content/download");
	if (!inst_mkdirs(dl_dir))
		return fail(EXIT_WRITE, "Cannot make the folder %s.", dl_dir);
	inst_join(zip_path, sizeof(zip_path), dl_dir, p->file);
	if ((rc = download_pack(p, zip_path)) != 0) {
		RemoveDirectoryA(dl_dir);		// only if it is empty
		return rc;
	}
	if (!inst_zip_open(&z, zip_path)) {
		remove(zip_path);
		return fail(EXIT_WRITE, "The pack %s is not a ZIP archive.", p->file);
	}
	i = inst_zip_find(&z, "_files.txt");
	files = i >= 0 ? inst_zip_read(&z, i, NULL) : NULL;
	i = inst_zip_find(&z, "_links.txt");
	links = i >= 0 ? inst_zip_read(&z, i, NULL) : NULL;
	if (!files) {
		inst_zip_close(&z);
		remove(zip_path);
		return fail(EXIT_WRITE, "The pack %s has no file list.", p->file);
	}
	// A new version replaces the files of the old one.
	state_path(sp, sizeof(sp), p->name);
	if (inst_is_file(sp))
		remove_files(p, sp);
	target_dir(p, tdir, sizeof(tdir));
	for (i = 0; i < z.n; i++)
		prog[1] += z.e[i].usize;
	rc = 0;
	for (i = 0; i < z.n && !rc; i++) {
		char to[INST_PATH], hex[65], *d;
		const char *name = z.e[i].name;
		if (name[0] == '_' || name[strlen(name) - 1] == '/')
			continue;
		inst_join(to, sizeof(to), tdir, name);
		d = strrchr(to, '/');
		if (d) {
			*d = 0;
			inst_mkdirs(to);
			*d = '/';
		}
		// The CRC of the ZIP entry is checked here; the SHA-256 of the file
		// list is checked at the end (cmd_verify).
		if (!inst_zip_extract(&z, i, to, hex, on_extract, prog))
			rc = fail(EXIT_WRITE, "Cannot extract %s of the pack %s (disk full?).", name, p->name);
	}
	inst_zip_close(&z);
	// The duplicates: hard links (or copies) of a file of the pack.
	if (!rc && links) {
		int n = 0;
		for (line = links; line && *line; line = next) {
			char *tab, a[INST_PATH], b[INST_PATH], *d;
			next = strchr(line, '\n');
			if (next)
				*next++ = 0;
			line[strcspn(line, "\r")] = 0;
			if (!(tab = strchr(line, '\t')))
				continue;
			*tab = 0;
			inst_join(a, sizeof(a), tdir, line);		// the link
			inst_join(b, sizeof(b), tdir, tab + 1);	// the file of the pack
			d = strrchr(a, '/');
			if (d) {
				*d = 0;
				inst_mkdirs(a);
				*d = '/';
			}
			if (!inst_link_or_copy(b, a)) {
				rc = fail(EXIT_WRITE, "Cannot make %s (disk full?).", a);
				break;
			}
			if (++n % 500 == 0)
				progress(100, "Installing %s: %d links", p->name, n);
		}
	}
	free(links);
	remove(zip_path);
	RemoveDirectoryA(dl_dir);
	if (!rc) {
		char dir[INST_PATH];
		inst_join(dir, sizeof(dir), Root, "content/installed");
		inst_mkdirs(dir);
		st = fopen(sp, "w");
		if (!st || fprintf(st, "version=%s\ntarget=%s\ntitle=%s\n%s", p->version, p->target, p->title, files) < 0) {
			if (st) fclose(st);
			rc = fail(EXIT_WRITE, "Cannot write %s.", sp);
		} else if (fclose(st))
			rc = fail(EXIT_WRITE, "Cannot write %s.", sp);
	}
	free(files);
	if (rc)
		return rc;
	// The check of what was written (the file list has the SHA-256 of
	// each file).
	if ((rc = cmd_verify(p, 1)) != 0)
		return fail(EXIT_HASH, "The files of %s do not match the file list of the pack.", p->name);
	snprintf(p->installed, sizeof(p->installed), "%s", p->version);
	out("INFO installed %s version %s", p->name, p->version);
	progress(100, "Installed %s", p->title[0] ? p->title : p->name);
	return 0;
}

static int cmd_repair(pack *p)
{
	int rc = cmd_verify(p, 1);
	if (rc == 0)
		return 0;
	if (rc != EXIT_DAMAGED)
		return rc;
	out("INFO repairing %s", p->name);
	return cmd_install(p);
}

// ---------------------------------------------------------------------
// The window

enum { ID_LIST = 100, ID_INSTALL, ID_REMOVE, ID_REPAIR, ID_REFRESH, ID_CLOSE, ID_STATUS, ID_BAR };
static HWND List, Status, Bar, Buttons[5];
static volatile LONG Busy;
static int Job_cmd, Job_pack, Job_rc;
static HFONT Font;

static void fmt_size(int64_t b, char *out, int n)
{
	if (b >= (1 << 20)) snprintf(out, n, "%lld MB", (long long) ((b + (1 << 19)) >> 20));
	else if (b > 0) snprintf(out, n, "%lld KB", (long long) ((b + 1023) >> 10));
	else snprintf(out, n, "-");
}

static void fill_list(void)
{
	int i, row = 0, sel = (int) SendMessageA(List, LVM_GETNEXTITEM, (WPARAM) -1, LVNI_SELECTED);

	SendMessageA(List, LVM_DELETEALLITEMS, 0, 0);
	for (i = 0; i < N_packs; i++) {
		pack *p = &Packs[i];
		LVITEMA it;
		char size[32], status[64];
		if (!inst_stricmp(p->target, "data"))		// the patch is for the setup program
			continue;
		memset(&it, 0, sizeof(it));
		it.mask = LVIF_TEXT | LVIF_PARAM;
		it.iItem = row;
		it.pszText = p->title[0] ? p->title : p->name;
		it.lParam = i;
		SendMessageA(List, LVM_INSERTITEMA, 0, (LPARAM) &it);
		fmt_size(p->size, size, sizeof(size));
		if (!p->installed[0]) snprintf(status, sizeof(status), p->sha256[0] ? "Not installed" : "-");
		else if (p->state == 2) snprintf(status, sizeof(status), "Damaged");
		else if (p->sha256[0] && strcmp(p->installed, p->version)) snprintf(status, sizeof(status), "Update available");
		else if (p->state == 1) snprintf(status, sizeof(status), "Installed, checked");
		else snprintf(status, sizeof(status), "Installed");
		it.mask = LVIF_TEXT;
		it.iSubItem = 1; it.pszText = p->installed[0] ? p->installed : "-";
		SendMessageA(List, LVM_SETITEMA, 0, (LPARAM) &it);
		it.iSubItem = 2; it.pszText = p->version[0] ? p->version : "-";
		SendMessageA(List, LVM_SETITEMA, 0, (LPARAM) &it);
		it.iSubItem = 3; it.pszText = size;
		SendMessageA(List, LVM_SETITEMA, 0, (LPARAM) &it);
		it.iSubItem = 4; it.pszText = status;
		SendMessageA(List, LVM_SETITEMA, 0, (LPARAM) &it);
		row++;
	}
	if (sel >= 0 && sel < row) {
		LVITEMA it;
		memset(&it, 0, sizeof(it));
		it.stateMask = it.state = LVIS_SELECTED | LVIS_FOCUSED;
		SendMessageA(List, LVM_SETITEMSTATE, sel, (LPARAM) &it);
	}
}

static void set_buttons(void)
{
	int i, busy = Busy != 0, sel = (int) SendMessageA(List, LVM_GETNEXTITEM, (WPARAM) -1, LVNI_SELECTED);
	pack *p = NULL;

	if (sel >= 0) {
		LVITEMA it;
		memset(&it, 0, sizeof(it));
		it.mask = LVIF_PARAM;
		it.iItem = sel;
		if (SendMessageA(List, LVM_GETITEMA, 0, (LPARAM) &it))
			p = &Packs[it.lParam];
	}
	EnableWindow(Buttons[0], !busy && p && p->sha256[0] && (!p->installed[0] || strcmp(p->installed, p->version)));
	EnableWindow(Buttons[1], !busy && p && p->installed[0]);
	EnableWindow(Buttons[2], !busy && p && p->installed[0]);
	EnableWindow(Buttons[3], !busy);
	for (i = 0; i < 4; i++)
		InvalidateRect(Buttons[i], NULL, FALSE);
}

static unsigned __stdcall job_thread(void *arg)
{
	(void) arg;
	Last_error[0] = 0;
	if (Job_cmd == ID_REFRESH) {
		read_installed();
		Job_rc = load_manifest();
		read_installed();
	} else if (Job_cmd == ID_INSTALL)
		Job_rc = cmd_install(&Packs[Job_pack]);
	else if (Job_cmd == ID_REMOVE)
		Job_rc = cmd_remove(&Packs[Job_pack]);
	else if (Job_cmd == ID_REPAIR) {
		Job_rc = cmd_verify(&Packs[Job_pack], 1);
		if (Job_rc == EXIT_DAMAGED && Have_manifest) {
			Job_rc = cmd_install(&Packs[Job_pack]);
			if (Job_rc == 0)
				Packs[Job_pack].state = 1;
		} else if (Job_rc == EXIT_DAMAGED)
			snprintf(Last_error, sizeof(Last_error), "The pack is damaged, and the server cannot be reached to repair it.");
	}
	PostMessageA(Main_wnd, WM_APP_DONE, 0, 0);
	return 0;
}

static void start_job(int cmd)
{
	int sel = (int) SendMessageA(List, LVM_GETNEXTITEM, (WPARAM) -1, LVNI_SELECTED);
	LVITEMA it;

	if (Busy)
		return;
	if (cmd != ID_REFRESH) {
		if (sel < 0)
			return;
		memset(&it, 0, sizeof(it));
		it.mask = LVIF_PARAM;
		it.iItem = sel;
		SendMessageA(List, LVM_GETITEMA, 0, (LPARAM) &it);
		Job_pack = (int) it.lParam;
		if (cmd == ID_REMOVE) {
			char q[300];
			snprintf(q, sizeof(q), "Remove %s?", Packs[Job_pack].title);
			if (MessageBoxA(Main_wnd, q, "ASCENT-DTU Content", MB_YESNO | MB_ICONQUESTION) != IDYES)
				return;
		}
	}
	Job_cmd = cmd;
	Busy = 1;
	SendMessageA(Bar, PBM_SETPOS, 0, 0);
	SetWindowTextA(Status, cmd == ID_REFRESH ? "Asking the server for the list of packs..." : "Working...");
	set_buttons();
	CloseHandle((HANDLE) _beginthreadex(NULL, 0, job_thread, NULL, 0, NULL));
}

static HWND make(const char *cls, const char *text, DWORD style, int id)
{
	HWND w = CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, Main_wnd,
		(HMENU) (INT_PTR) id, GetModuleHandleA(NULL), NULL);
	SendMessageA(w, WM_SETFONT, (WPARAM) Font, TRUE);
	return w;
}

static void layout(void)
{
	RECT r;
	int dpi = 96, m, bh, bw, i, w, h;
	HMODULE u = GetModuleHandleA("user32.dll");
	UINT (WINAPI *get_dpi)(HWND) = u ? (UINT (WINAPI *)(HWND)) GetProcAddress(u, "GetDpiForWindow") : NULL;

	if (get_dpi)
		dpi = (int) get_dpi(Main_wnd);
	m = MulDiv(10, dpi, 96);
	bh = MulDiv(26, dpi, 96);
	bw = MulDiv(100, dpi, 96);
	GetClientRect(Main_wnd, &r);
	w = r.right; h = r.bottom;
	MoveWindow(List, m, m, w - 2 * m, h - 4 * m - 3 * bh, TRUE);
	MoveWindow(Status, m, h - 3 * m - 3 * bh + m / 2, w - 2 * m, bh, TRUE);
	MoveWindow(Bar, m, h - 2 * m - 2 * bh + m / 2, w - 2 * m, bh * 2 / 3, TRUE);
	for (i = 0; i < 5; i++)
		MoveWindow(Buttons[i], i < 4 ? m + i * (bw + m) : w - m - bw, h - m - bh, bw, bh, TRUE);
	{
		int cw[5] = { 300, 80, 80, 80, 150 }, k, total = 0;
		for (k = 0; k < 5; k++) total += cw[k];
		for (k = 0; k < 5; k++)
			SendMessageA(List, LVM_SETCOLUMNWIDTH, k, MulDiv(cw[k], w - 2 * m - MulDiv(24, dpi, 96), total));
	}
}

static LRESULT CALLBACK wnd_proc(HWND w, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg) {
	case WM_CREATE: {
		static const char *cols[5] = { "Pack", "Installed", "Available", "Download", "Status" };
		NONCLIENTMETRICSA ncm;
		int i;
		Main_wnd = w;
		memset(&ncm, 0, sizeof(ncm));
		ncm.cbSize = sizeof(ncm);
		SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
		Font = CreateFontIndirectA(&ncm.lfMessageFont);
		List = make(WC_LISTVIEWA, "", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_BORDER, ID_LIST);
		SendMessageA(List, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
		for (i = 0; i < 5; i++) {
			LVCOLUMNA c;
			memset(&c, 0, sizeof(c));
			c.mask = LVCF_TEXT | LVCF_WIDTH;
			c.pszText = (char *) cols[i];
			c.cx = 100;
			SendMessageA(List, LVM_INSERTCOLUMNA, i, (LPARAM) &c);
		}
		Status = make("STATIC", "", SS_LEFTNOWORDWRAP | SS_ENDELLIPSIS, ID_STATUS);
		Bar = make(PROGRESS_CLASSA, "", 0, ID_BAR);
		SendMessageA(Bar, PBM_SETRANGE32, 0, 100);
		Buttons[0] = make("BUTTON", "&Install", BS_PUSHBUTTON, ID_INSTALL);
		Buttons[1] = make("BUTTON", "&Remove", BS_PUSHBUTTON, ID_REMOVE);
		Buttons[2] = make("BUTTON", "Check / re&pair", BS_PUSHBUTTON, ID_REPAIR);
		Buttons[3] = make("BUTTON", "Re&fresh", BS_PUSHBUTTON, ID_REFRESH);
		Buttons[4] = make("BUTTON", "Close", BS_PUSHBUTTON, ID_CLOSE);
		layout();
		read_installed();
		fill_list();
		start_job(ID_REFRESH);
		return 0;
	}
	case WM_SIZE:
		layout();
		return 0;
	case WM_GETMINMAXINFO:
		((MINMAXINFO *) lp)->ptMinTrackSize.x = 640;
		((MINMAXINFO *) lp)->ptMinTrackSize.y = 360;
		return 0;
	case WM_NOTIFY:
		if (((NMHDR *) lp)->idFrom == ID_LIST && ((NMHDR *) lp)->code == LVN_ITEMCHANGED)
			set_buttons();
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wp)) {
		case ID_INSTALL: case ID_REMOVE: case ID_REPAIR: case ID_REFRESH:
			start_job(LOWORD(wp));
			break;
		case ID_CLOSE:
			PostMessageA(w, WM_CLOSE, 0, 0);
			break;
		}
		return 0;
	case WM_APP_PROGRESS:
		SendMessageA(Bar, PBM_SETPOS, Progress_percent, 0);
		SetWindowTextA(Status, Progress_text);
		return 0;
	case WM_APP_DONE: {
		char text[1200];
		Busy = 0;
		if (Job_cmd == ID_REFRESH && Job_rc)
			snprintf(text, sizeof(text), "The server cannot be reached: %s The installed packs can still be checked and removed.", Last_error);
		else if (Job_rc)
			snprintf(text, sizeof(text), "%s", Last_error[0] ? Last_error : "The operation failed.");
		else if (Job_cmd == ID_REPAIR)
			snprintf(text, sizeof(text), "%s: all files are good.", Packs[Job_pack].title);
		else if (Job_cmd == ID_REFRESH)
			snprintf(text, sizeof(text), "Select a pack. The game uses the packs at its next start.");
		else
			snprintf(text, sizeof(text), "Done.");
		SetWindowTextA(Status, text);
		SendMessageA(Bar, PBM_SETPOS, Job_rc ? 0 : 100, 0);
		fill_list();
		set_buttons();
		if (Job_rc && Job_cmd != ID_REFRESH)
			MessageBoxA(w, text, "ASCENT-DTU Content", MB_OK | MB_ICONWARNING);
		return 0;
	}
	case WM_CLOSE:
		if (Busy && MessageBoxA(w, "A download or an installation is running. Stop it and close?", "ASCENT-DTU Content",
				MB_YESNO | MB_ICONQUESTION) != IDYES)
			return 0;
		DestroyWindow(w);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcA(w, msg, wp, lp);
}

static int run_window(void)
{
	WNDCLASSEXA wc;
	INITCOMMONCONTROLSEX icc;
	MSG m;
	HWND w;
	int dpi = 96;
	HMODULE u = GetModuleHandleA("user32.dll");
	UINT (WINAPI *get_dpi)(void) = u ? (UINT (WINAPI *)(void)) GetProcAddress(u, "GetDpiForSystem") : NULL;

	Gui = 1;
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
	InitCommonControlsEx(&icc);
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = wnd_proc;
	wc.hInstance = GetModuleHandleA(NULL);
	wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(1));
	wc.hCursor = LoadCursorA(NULL, (LPCSTR) IDC_ARROW);
	wc.hbrBackground = (HBRUSH) (COLOR_BTNFACE + 1);
	wc.lpszClassName = "AscentContent";
	RegisterClassExA(&wc);
	if (get_dpi)
		dpi = (int) get_dpi();
	w = CreateWindowExA(0, wc.lpszClassName, "ASCENT-DTU Content", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
		MulDiv(760, dpi, 96), MulDiv(420, dpi, 96), NULL, NULL, wc.hInstance, NULL);
	ShowWindow(w, SW_SHOWDEFAULT);		// the show state of the caller (STARTUPINFO)
	while (GetMessageA(&m, NULL, 0, 0) > 0) {
		if (!IsDialogMessageA(w, &m)) {
			TranslateMessage(&m);
			DispatchMessageA(&m);
		}
	}
	return 0;
}

// ---------------------------------------------------------------------

static void usage(void)
{
	out("Usage: ascent-content [--root DIR] [--url URL] [list | download PACK FILE | install PACK... |\n"
		"       remove PACK... | verify PACK... | repair PACK...]\n"
		"Without a command the program opens its window.");
}

int main(int argc, char **argv)
{
	const char *url = NULL, *cmd = NULL;
	int i, first = 0, rc = 0;

	// A program of the Windows subsystem has no console. The output goes
	// to the pipe of the setup program, or to the console of the caller.
	if (GetFileType(GetStdHandle(STD_OUTPUT_HANDLE)) == FILE_TYPE_UNKNOWN && AttachConsole(ATTACH_PARENT_PROCESS)) {
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}

	if (!GetModuleFileNameA(NULL, Root, sizeof(Root)))
		Root[0] = 0;
	if (strrchr(Root, '\\'))
		*strrchr(Root, '\\') = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--root") && i + 1 < argc) snprintf(Root, sizeof(Root), "%s", argv[++i]);
		else if (!strcmp(argv[i], "--url") && i + 1 < argc) url = argv[++i];
		else if (argv[i][0] == '-' && argv[i][1] == '-') {
			usage();
			return EXIT_USAGE;
		} else {
			cmd = argv[i];
			first = i + 1;
			break;
		}
	}
	read_config(url);
	if (!cmd)
		return run_window();

	read_installed();
	if (!strcmp(cmd, "list")) {
		if ((rc = load_manifest()) != 0)
			out("INFO no manifest: the list has only the installed packs");
		read_installed();
		for (i = 0; i < N_packs; i++)
			out("PACK %s\tavailable=%s\tinstalled=%s\ttarget=%s\tsize=%lld\t%s", Packs[i].name,
				Packs[i].version[0] ? Packs[i].version : "-", Packs[i].installed[0] ? Packs[i].installed : "-",
				Packs[i].target, (long long) Packs[i].size, Packs[i].title);
		return rc;
	}
	if (!strcmp(cmd, "download")) {
		pack *p;
		if (first + 2 != argc) {
			usage();
			return EXIT_USAGE;
		}
		if ((rc = load_manifest()) != 0)
			return rc;
		if (!(p = find_pack(argv[first])) || !p->sha256[0])
			return fail(EXIT_NO_PACK, "The server has no pack %s.", argv[first]);
		return download_pack(p, argv[first + 1]);
	}
	if (first >= argc) {
		usage();
		return EXIT_USAGE;
	}
	if (!strcmp(cmd, "install") || !strcmp(cmd, "repair"))
		if ((rc = load_manifest()) != 0)
			return rc;
	for (i = first; i < argc && !rc; i++) {
		pack *p = find_pack(argv[i]);
		if (!p)
			return fail(EXIT_NO_PACK, "There is no pack %s.", argv[i]);
		if (!strcmp(cmd, "install")) rc = cmd_install(p);
		else if (!strcmp(cmd, "remove")) rc = cmd_remove(p);
		else if (!strcmp(cmd, "verify")) rc = cmd_verify(p, 0);
		else if (!strcmp(cmd, "repair")) rc = cmd_repair(p);
		else {
			usage();
			return EXIT_USAGE;
		}
	}
	return rc;
}
