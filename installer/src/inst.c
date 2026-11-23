/*
 * ASCENT installer tools: the shared code. See inst.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#include "stb_image.h"		// the inflate code (stbi_zlib_decode_noheader_buffer)

#include "inst.h"

// Offsets above 2 GB (long has 32 bits on Windows).
#ifdef _WIN32
#define inst_fseek(f, o, w)	_fseeki64(f, o, w)
#define inst_ftell(f)		_ftelli64(f)
#else
#define inst_fseek(f, o, w)	fseeko(f, o, w)
#define inst_ftell(f)		ftello(f)
#endif

// ---------------------------------------------------------------------
// Files

int inst_stricmp(const char *a, const char *b)
{
	while (*a && *b) {
		int d = tolower((unsigned char) *a) - tolower((unsigned char) *b);
		if (d)
			return d;
		a++, b++;
	}
	return tolower((unsigned char) *a) - tolower((unsigned char) *b);
}

const char *inst_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	const char *sep = strrchr(name, '/');

	if (sep == NULL || strrchr(name, '\\') > sep)
		sep = strrchr(name, '\\');
	if (dot == NULL || (sep && dot < sep))
		return "";
	return dot + 1;
}

int inst_is_dir(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

int inst_is_file(const char *path)
{
	struct stat st;
	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFREG;
}

int64_t inst_file_size(const char *path)
{
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA a;
	if (!GetFileAttributesExA(path, GetFileExInfoStandard, &a) || (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return -1;
	return ((int64_t) a.nFileSizeHigh << 32) | a.nFileSizeLow;
#else
	struct stat st;
	if (stat(path, &st) != 0 || (st.st_mode & S_IFMT) != S_IFREG)
		return -1;
	return st.st_size;
#endif
}

void inst_join(char *out, int size, const char *a, const char *b)
{
	int n = (int) strlen(a);

	if (n == 0)
		snprintf(out, size, "%s", b);
	else if (a[n - 1] == '/' || a[n - 1] == '\\')
		snprintf(out, size, "%s%s", a, b);
	else
		snprintf(out, size, "%s/%s", a, b);
}

int inst_mkdirs(const char *path)
{
	char p[INST_PATH];
	int i, n;

	snprintf(p, sizeof(p), "%s", path);
	n = (int) strlen(p);
	while (n > 0 && (p[n - 1] == '/' || p[n - 1] == '\\'))
		p[--n] = 0;
	for (i = 1; i <= n; i++) {
		if (p[i] == '/' || p[i] == '\\' || p[i] == 0) {
			char c = p[i];
			p[i] = 0;
			if (i > 0 && p[i - 1] != ':' && !inst_is_dir(p)) {
#ifdef _WIN32
				CreateDirectoryA(p, NULL);
#else
				mkdir(p, 0777);
#endif
			}
			p[i] = c;
		}
	}
	return inst_is_dir(path);
}

int inst_copy_file(const char *from, const char *to)
{
	FILE *in = fopen(from, "rb"), *out;
	static char buf[1 << 16];
	size_t n;
	int ok = 1;

	if (!in)
		return 0;
	out = fopen(to, "wb");
	if (!out) {
		fclose(in);
		return 0;
	}
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
		if (fwrite(buf, 1, n, out) != n) {
			ok = 0;
			break;
		}
	if (ferror(in))
		ok = 0;
	fclose(in);
	if (fclose(out))
		ok = 0;
	return ok;
}

int inst_link_or_copy(const char *target, const char *dest)
{
	remove(dest);
#ifdef _WIN32
	if (CreateHardLinkA(dest, target, NULL))
		return 1;
#else
	if (link(target, dest) == 0)
		return 1;
#endif
	return inst_copy_file(target, dest);
}

static int list_add(inst_list *l, const char *name, int64_t size)
{
	if (l->n == l->max) {
		int max = l->max ? l->max * 2 : 256;
		char **nn = realloc(l->name, max * sizeof(*nn));
		int64_t *ns;
		if (!nn)
			return 0;
		l->name = nn;
		ns = realloc(l->size, max * sizeof(*ns));
		if (!ns)
			return 0;
		l->size = ns;
		l->max = max;
	}
	l->name[l->n] = strdup(name);
	l->size[l->n] = size;
	if (!l->name[l->n])
		return 0;
	l->n++;
	return 1;
}

int inst_list_dir(const char *dir, inst_list *l)
{
	memset(l, 0, sizeof(*l));
#ifdef _WIN32
	{
		char pat[INST_PATH];
		WIN32_FIND_DATAA fd;
		HANDLE h;

		inst_join(pat, sizeof(pat), dir, "*");
		h = FindFirstFileA(pat, &fd);
		if (h == INVALID_HANDLE_VALUE)
			return 0;
		do {
			if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				list_add(l, fd.cFileName, ((int64_t) fd.nFileSizeHigh << 32) | fd.nFileSizeLow);
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
#else
	{
		DIR *d = opendir(dir);
		struct dirent *e;
		if (!d)
			return 0;
		while ((e = readdir(d)) != NULL) {
			char p[INST_PATH];
			inst_join(p, sizeof(p), dir, e->d_name);
			if (inst_is_file(p))
				list_add(l, e->d_name, inst_file_size(p));
		}
		closedir(d);
	}
#endif
	return 1;
}

void inst_list_free(inst_list *l)
{
	int i;
	for (i = 0; i < l->n; i++)
		free(l->name[i]);
	free(l->name);
	free(l->size);
	memset(l, 0, sizeof(*l));
}

int inst_find_name(const char *dir, const char *name, char *out, int size)
{
	inst_list l;
	int i, found = 0;
	char p[INST_PATH];

	inst_join(p, sizeof(p), dir, name);
	if (inst_is_file(p)) {
		snprintf(out, size, "%s", p);
		return 1;
	}
	if (!inst_list_dir(dir, &l))
		return 0;
	for (i = 0; i < l.n && !found; i++)
		if (!inst_stricmp(l.name[i], name)) {
			inst_join(out, size, dir, l.name[i]);
			found = 1;
		}
	inst_list_free(&l);
	return found;
}

int inst_remove_tree(const char *path)
{
#ifdef _WIN32
	char pat[INST_PATH];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	if (!inst_is_dir(path))
		return remove(path) == 0 || !inst_is_file(path);
	inst_join(pat, sizeof(pat), path, "*");
	h = FindFirstFileA(pat, &fd);
	if (h != INVALID_HANDLE_VALUE) {
		do {
			char p[INST_PATH];
			if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
				continue;
			inst_join(p, sizeof(p), path, fd.cFileName);
			if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				inst_remove_tree(p);
			else {
				SetFileAttributesA(p, FILE_ATTRIBUTE_NORMAL);
				DeleteFileA(p);
			}
		} while (FindNextFileA(h, &fd));
		FindClose(h);
	}
	RemoveDirectoryA(path);
	return !inst_is_dir(path);
#else
	(void) path;
	return 0;
#endif
}

// ---------------------------------------------------------------------
// SHA-256 (FIPS 180-4)

static const uint32_t K256[64] = {
	0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
	0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
	0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
	0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
	0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
	0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
	0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
	0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void sha_block(inst_sha256 *c, const uint8_t *p)
{
	uint32_t w[64], a, b, d, e, f, g, h, cc, t1, t2;
	int i;

	for (i = 0; i < 16; i++)
		w[i] = ((uint32_t) p[4*i] << 24) | ((uint32_t) p[4*i+1] << 16) | ((uint32_t) p[4*i+2] << 8) | p[4*i+3];
	for (i = 16; i < 64; i++) {
		uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
		uint32_t s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
		w[i] = w[i-16] + s0 + w[i-7] + s1;
	}
	a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3];
	e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
	for (i = 0; i < 64; i++) {
		t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
		t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
		h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
	}
	c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
	c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}

void inst_sha256_init(inst_sha256 *c)
{
	static const uint32_t h0[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
	memcpy(c->h, h0, sizeof(h0));
	c->len = 0;
	c->n = 0;
}

void inst_sha256_add(inst_sha256 *c, const void *data, size_t size)
{
	const uint8_t *p = data;

	c->len += size;
	while (size > 0) {
		if (c->n == 0 && size >= 64) {
			sha_block(c, p);
			p += 64;
			size -= 64;
		} else {
			size_t k = 64 - c->n;
			if (k > size)
				k = size;
			memcpy(c->buf + c->n, p, k);
			c->n += (int) k;
			p += k;
			size -= k;
			if (c->n == 64) {
				sha_block(c, c->buf);
				c->n = 0;
			}
		}
	}
}

void inst_sha256_hex(inst_sha256 *c, char hex[65])
{
	uint64_t bits = c->len * 8;
	uint8_t pad = 0x80, z = 0, l[8];
	int i;

	inst_sha256_add(c, &pad, 1);
	while (c->n != 56)
		inst_sha256_add(c, &z, 1);
	for (i = 0; i < 8; i++)
		l[i] = (uint8_t) (bits >> (56 - 8 * i));
	inst_sha256_add(c, l, 8);
	for (i = 0; i < 8; i++)
		sprintf(hex + 8 * i, "%08x", c->h[i]);
	hex[64] = 0;
}

int inst_sha256_file(const char *path, char hex[65])
{
	static uint8_t buf[1 << 16];
	inst_sha256 c;
	FILE *f = fopen(path, "rb");
	size_t n;

	hex[0] = 0;
	if (!f)
		return 0;
	inst_sha256_init(&c);
	while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
		inst_sha256_add(&c, buf, n);
	if (ferror(f)) {
		fclose(f);
		return 0;
	}
	fclose(f);
	inst_sha256_hex(&c, hex);
	return 1;
}

// ---------------------------------------------------------------------
// CRC-32 of the ZIP format

static uint32_t crc_table[256];

static uint32_t crc32_add(uint32_t crc, const uint8_t *p, size_t n)
{
	if (!crc_table[1]) {
		uint32_t i, j, c;
		for (i = 0; i < 256; i++) {
			c = i;
			for (j = 0; j < 8; j++)
				c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
			crc_table[i] = c;
		}
	}
	crc = ~crc;
	while (n--)
		crc = crc_table[(crc ^ *p++) & 255] ^ (crc >> 8);
	return ~crc;
}

// ---------------------------------------------------------------------
// ZIP

static uint32_t u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }

int inst_zip_open(inst_zip *z, const char *path)
{
	uint8_t *tail = NULL, *cd = NULL;
	int64_t size, tail_size, i, eocd = -1;
	uint32_t cd_size, cd_offset, n, pos;

	memset(z, 0, sizeof(*z));
	z->f = fopen(path, "rb");
	if (!z->f)
		return 0;
	inst_fseek(z->f, 0, SEEK_END);
	size = inst_ftell(z->f);
	tail_size = size < 65557 ? size : 65557;
	tail = malloc(tail_size);
	if (!tail || size < 22)
		goto fail;
	inst_fseek(z->f, size - tail_size, SEEK_SET);
	if (fread(tail, 1, tail_size, z->f) != (size_t) tail_size)
		goto fail;
	for (i = tail_size - 22; i >= 0; i--)
		if (u32(tail + i) == 0x06054b50) {
			eocd = i;
			break;
		}
	if (eocd < 0)
		goto fail;
	n = u16(tail + eocd + 10);
	cd_size = u32(tail + eocd + 12);
	cd_offset = u32(tail + eocd + 16);
	// A self-extracting program before the archive moves all the offsets.
	z->delta = (int64_t) (size - tail_size + eocd) - ((int64_t) cd_offset + cd_size);
	if (z->delta < 0)
		goto fail;
	cd = malloc(cd_size ? cd_size : 1);
	z->e = calloc(n ? n : 1, sizeof(inst_zip_entry));
	if (!cd || !z->e)
		goto fail;
	inst_fseek(z->f, (int64_t) (cd_offset + z->delta), SEEK_SET);
	if (fread(cd, 1, cd_size, z->f) != cd_size)
		goto fail;
	for (pos = 0; z->n < (int) n && pos + 46 <= cd_size; z->n++) {
		inst_zip_entry *e = &z->e[z->n];
		uint32_t nl, xl, cl;
		if (u32(cd + pos) != 0x02014b50)
			goto fail;
		e->method = u16(cd + pos + 10);
		e->crc = u32(cd + pos + 16);
		e->csize = u32(cd + pos + 20);
		e->usize = u32(cd + pos + 24);
		nl = u16(cd + pos + 28);
		xl = u16(cd + pos + 30);
		cl = u16(cd + pos + 32);
		e->local_offset = u32(cd + pos + 42);
		if (pos + 46 + nl > cd_size || nl >= sizeof(e->name))
			goto fail;
		memcpy(e->name, cd + pos + 46, nl);
		e->name[nl] = 0;
		pos += 46 + nl + xl + cl;
	}
	free(tail);
	free(cd);
	return 1;
fail:
	free(tail);
	free(cd);
	inst_zip_close(z);
	return 0;
}

void inst_zip_close(inst_zip *z)
{
	if (z->f)
		fclose(z->f);
	free(z->e);
	memset(z, 0, sizeof(*z));
}

int inst_zip_find(inst_zip *z, const char *name)
{
	int i;
	for (i = 0; i < z->n; i++)
		if (!inst_stricmp(z->e[i].name, name))
			return i;
	return -1;
}

static int zip_seek_data(inst_zip *z, inst_zip_entry *e)
{
	uint8_t h[30];

	if (inst_fseek(z->f, (int64_t) (e->local_offset + z->delta), SEEK_SET) != 0 || fread(h, 1, 30, z->f) != 30 || u32(h) != 0x04034b50)
		return 0;
	return inst_fseek(z->f, (int64_t) (e->local_offset + z->delta + 30 + u16(h + 26) + u16(h + 28)), SEEK_SET) == 0;
}

char *inst_zip_read(inst_zip *z, int i, uint32_t *size)
{
	inst_zip_entry *e = &z->e[i];
	uint8_t *c = NULL;
	char *u = NULL;

	if (i < 0 || i >= z->n || !zip_seek_data(z, e))
		return NULL;
	c = malloc(e->csize + 1);
	u = malloc(e->usize + 1);
	if (!c || !u || fread(c, 1, e->csize, z->f) != e->csize)
		goto fail;
	if (e->method == 0 && e->csize == e->usize)
		memcpy(u, c, e->usize);
	else if (e->method == 8) {
		if (stbi_zlib_decode_noheader_buffer(u, (int) e->usize, (const char *) c, (int) e->csize) != (int) e->usize)
			goto fail;
	} else
		goto fail;
	if (crc32_add(0, (uint8_t *) u, e->usize) != e->crc)
		goto fail;
	u[e->usize] = 0;
	free(c);
	if (size)
		*size = e->usize;
	return u;
fail:
	free(c);
	free(u);
	return NULL;
}

int inst_zip_extract(inst_zip *z, int i, const char *to, char hex[65],
	void (*progress)(int64_t done, void *arg), void *arg)
{
	inst_zip_entry *e = &z->e[i];
	static uint8_t buf[1 << 16];
	inst_sha256 sha;
	uint32_t crc = 0, left;
	FILE *out;
	int ok = 1;

	hex[0] = 0;
	if (i < 0 || i >= z->n)
		return 0;
	if (e->method == 8) {
		// The deflate entries are the small files: decode in memory.
		uint32_t size;
		char *data = inst_zip_read(z, i, &size);
		if (!data)
			return 0;
		out = fopen(to, "wb");
		ok = out && fwrite(data, 1, size, out) == size;
		if (out && fclose(out))
			ok = 0;
		inst_sha256_init(&sha);
		inst_sha256_add(&sha, data, size);
		inst_sha256_hex(&sha, hex);
		free(data);
		if (progress)
			progress(size, arg);
		return ok;
	}
	if (e->method != 0 || e->csize != e->usize || !zip_seek_data(z, e))
		return 0;
	out = fopen(to, "wb");
	if (!out)
		return 0;
	inst_sha256_init(&sha);
	for (left = e->usize; left > 0 && ok; ) {
		size_t k = left < sizeof(buf) ? left : sizeof(buf);
		if (fread(buf, 1, k, z->f) != k || fwrite(buf, 1, k, out) != k)
			ok = 0;
		else {
			crc = crc32_add(crc, buf, k);
			inst_sha256_add(&sha, buf, k);
			left -= (uint32_t) k;
			if (progress)
				progress(k, arg);
		}
	}
	if (fclose(out))
		ok = 0;
	if (ok && crc != e->crc)
		ok = 0;
	inst_sha256_hex(&sha, hex);
	return ok;
}

// ---------------------------------------------------------------------
// ISO 9660

static int iso_read(inst_iso *iso, uint32_t lba, uint32_t offset, uint8_t *out, uint32_t size)
{
	while (size > 0) {
		uint32_t s = lba + offset / 2048, o = offset % 2048;
		uint32_t k = 2048 - o;
		if (k > size)
			k = size;
		if (inst_fseek(iso->f, (int64_t) ((int64_t) s * iso->sector_size + iso->data_offset + o), SEEK_SET) != 0 ||
			fread(out, 1, k, iso->f) != k)
			return 0;
		out += k;
		offset += k;
		size -= k;
	}
	return 1;
}

static int iso_add(inst_iso *iso, const char *path, uint32_t lba, uint32_t size)
{
	if (iso->n == iso->max) {
		int max = iso->max ? iso->max * 2 : 512;
		inst_iso_file *nf = realloc(iso->files, max * sizeof(*nf));
		if (!nf)
			return 0;
		iso->files = nf;
		iso->max = max;
	}
	snprintf(iso->files[iso->n].path, sizeof(iso->files[0].path), "%s", path);
	iso->files[iso->n].lba = lba;
	iso->files[iso->n].size = size;
	iso->n++;
	return 1;
}

static int iso_dir(inst_iso *iso, uint32_t lba, uint32_t size, const char *prefix, int depth)
{
	uint8_t *d;
	uint32_t pos = 0;

	if (depth > 8 || size > (1u << 24))
		return 0;
	d = malloc(size);
	if (!d || !iso_read(iso, lba, 0, d, size)) {
		free(d);
		return 0;
	}
	while (pos < size) {
		uint32_t len = d[pos], ext_lba, ext_size, nl;
		char name[256], path[260];
		int flags;
		if (len == 0) {
			pos = (pos / 2048 + 1) * 2048;		// the records do not cross sectors
			continue;
		}
		if (pos + 33 > size || pos + len > size)
			break;
		ext_lba = u32(d + pos + 2);
		ext_size = u32(d + pos + 10);
		flags = d[pos + 25];
		nl = d[pos + 32];
		if (nl == 1 && (d[pos + 33] == 0 || d[pos + 33] == 1)) {
			pos += len;
			continue;
		}
		memcpy(name, d + pos + 33, nl);
		name[nl] = 0;
		if (strchr(name, ';'))
			*strchr(name, ';') = 0;
		if (nl && name[strlen(name) - 1] == '.')
			name[strlen(name) - 1] = 0;
		snprintf(path, sizeof(path), "%s%s", prefix, name);
		pos += len;
		if (flags & 2) {
			char sub[262];
			snprintf(sub, sizeof(sub), "%s/", path);
			iso_dir(iso, ext_lba, ext_size, sub, depth + 1);
		} else
			iso_add(iso, path, ext_lba, ext_size);
	}
	free(d);
	return 1;
}

// The .bin file named in a .cue sheet, and the sector format of track 1.
static int parse_cue(const char *cue, char *bin, int bin_size, int *sector_size, int *data_offset, char *err, int err_size)
{
	FILE *f = fopen(cue, "r");
	char line[1024], dir[INST_PATH];
	int have_file = 0, have_track = 0;
	char *p;

	if (!f) {
		snprintf(err, err_size, "Cannot open %s.", cue);
		return 0;
	}
	snprintf(dir, sizeof(dir), "%s", cue);
	p = strrchr(dir, '/');
	if (!p || strrchr(dir, '\\') > p)
		p = strrchr(dir, '\\');
	if (p)
		p[1] = 0;
	else
		dir[0] = 0;
	while (fgets(line, sizeof(line), f)) {
		char *s = line;
		while (*s == ' ' || *s == '\t')
			s++;
		if (!have_file && !strncmp(s, "FILE", 4)) {
			char *q1 = strchr(s, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
			if (q1 && q2) {
				*q2 = 0;
				if (q1[1] && (q1[2] == ':' || q1[1] == '\\' || q1[1] == '/'))
					snprintf(bin, bin_size, "%s", q1 + 1);
				else
					snprintf(bin, bin_size, "%s%s", dir, q1 + 1);
				have_file = 1;
			}
		} else if (have_file && !have_track && !strncmp(s, "TRACK", 5)) {
			have_track = 1;
			if (strstr(s, "MODE1/2048")) { *sector_size = 2048; *data_offset = 0; }
			else if (strstr(s, "MODE1/2352")) { *sector_size = 2352; *data_offset = 16; }
			else if (strstr(s, "MODE2/2352")) { *sector_size = 2352; *data_offset = 24; }
			else {
				snprintf(err, err_size, "The first track of %s is not a data track.", cue);
				fclose(f);
				return 0;
			}
		}
	}
	fclose(f);
	if (!have_file || !have_track) {
		snprintf(err, err_size, "%s is not a cue sheet with a data track.", cue);
		return 0;
	}
	return 1;
}

int inst_iso_open(inst_iso *iso, const char *path, char *err, int err_size)
{
	char bin[INST_PATH];
	uint8_t pvd[2048];
	int sector_size = 0, data_offset = 0;
	static const int formats[3][2] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 } };
	int i;

	memset(iso, 0, sizeof(*iso));
	snprintf(bin, sizeof(bin), "%s", path);
	if (!inst_stricmp(inst_ext(path), "cue") &&
		!parse_cue(path, bin, sizeof(bin), &sector_size, &data_offset, err, err_size))
		return 0;
	iso->f = fopen(bin, "rb");
	if (!iso->f) {
		snprintf(err, err_size, "Cannot open %s.", bin);
		return 0;
	}
	for (i = 0; i < 3; i++) {
		if (sector_size && (formats[i][0] != sector_size || formats[i][1] != data_offset))
			continue;
		iso->sector_size = formats[i][0];
		iso->data_offset = formats[i][1];
		if (iso_read(iso, 16, 0, pvd, 2048) && pvd[0] == 1 && !memcmp(pvd + 1, "CD001", 5))
			break;
	}
	if (i == 3) {
		snprintf(err, err_size, "%s is not an ISO 9660 disc image.", bin);
		inst_iso_close(iso);
		return 0;
	}
	if (!iso_dir(iso, u32(pvd + 156 + 2), u32(pvd + 156 + 10), "", 0)) {
		snprintf(err, err_size, "Cannot read the directories of %s.", bin);
		inst_iso_close(iso);
		return 0;
	}
	return 1;
}

void inst_iso_close(inst_iso *iso)
{
	if (iso->f)
		fclose(iso->f);
	free(iso->files);
	memset(iso, 0, sizeof(*iso));
}

int inst_iso_extract(inst_iso *iso, const inst_iso_file *file, const char *to,
	void (*progress)(int64_t done, void *arg), void *arg)
{
	static uint8_t buf[2048 * 32];
	uint32_t done = 0;
	FILE *out = fopen(to, "wb");
	int ok = 1;

	if (!out)
		return 0;
	while (done < file->size && ok) {
		uint32_t k = file->size - done;
		if (k > sizeof(buf))
			k = sizeof(buf);
		if (iso->sector_size == 2048) {
			if (inst_fseek(iso->f, (int64_t) ((int64_t) file->lba * 2048 + done), SEEK_SET) != 0 || fread(buf, 1, k, iso->f) != k)
				ok = 0;
		} else if (!iso_read(iso, file->lba, done, buf, k))
			ok = 0;
		if (ok && fwrite(buf, 1, k, out) != k)
			ok = 0;
		done += k;
		if (progress)
			progress(k, arg);
	}
	if (fclose(out))
		ok = 0;
	return ok;
}
