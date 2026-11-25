/*
 * ASCENT installer tools: the shared code. See inst.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <unistd.h>
#endif

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
// Inflate (RFC 1951), as a stream: the decoder keeps the last 32 KB of
// output and stops at any symbol, so that the caller can read the data in
// parts. Written for ASCENT after the description of the format in RFC
// 1951 (no code of another library). The codes of 10 bits or less are
// found with one table lookup; a longer code is decoded bit by bit with
// the counts of the canonical code (the usual method, also used by
// puff.c of zlib).

#define INF_FAST 10			// the bits of the fast table
#define INF_WSIZE 32768

typedef struct {
	int16_t count[16];		// the number of codes of each length
	int16_t symbol[320];	// the symbols, ordered by code
	uint16_t fast[1 << INF_FAST];	// symbol << 4 | length, or 0 (a longer code)
} inf_huff;

typedef struct {
	// The input: the compressed bytes of the entry
	FILE *f;
	int64_t in_pos;			// the file offset of the next byte to read
	uint64_t in_left;		// the compressed bytes not yet read
	uint8_t in[1 << 16];
	int in_n, in_i;
	uint32_t bits;
	int nbits;
	int overrun;			// bytes after the end of the input (zero bits)
	// The state
	int last, type;			// type: -1 a block header is next, 0 stored, 1 codes
	uint32_t stored_left;
	uint32_t match_len, match_dist;
	inf_huff len, dist;
	uint8_t window[INF_WSIZE];
	uint64_t total;			// the bytes of output so far
	int error, done;
} inf_state;

static int inf_byte(inf_state *s)
{
	if (s->in_i == s->in_n) {
		size_t k = s->in_left < sizeof(s->in) ? (size_t) s->in_left : sizeof(s->in);
		if (k == 0) {
			s->overrun++;
			return 0;
		}
		if (inst_fseek(s->f, s->in_pos, SEEK_SET) != 0 || fread(s->in, 1, k, s->f) != k) {
			s->error = 1;
			s->overrun++;
			return 0;
		}
		s->in_pos += k;
		s->in_left -= k;
		s->in_n = (int) k;
		s->in_i = 0;
	}
	return s->in[s->in_i++];
}

static void inf_need(inf_state *s, int n)
{
	while (s->nbits < n) {
		s->bits |= (uint32_t) inf_byte(s) << s->nbits;
		s->nbits += 8;
	}
}

static uint32_t inf_bits(inf_state *s, int n)
{
	uint32_t v;
	if (n == 0)
		return 0;
	inf_need(s, n);
	v = s->bits & ((1u << n) - 1);
	s->bits >>= n;
	s->nbits -= n;
	return v;
}

// Build the tables from the code lengths. 0 if the lengths are not valid.
static int inf_build(inf_huff *h, const uint8_t *lengths, int n)
{
	int16_t offs[16];
	int len, sym, left;

	memset(h->count, 0, sizeof(h->count));
	memset(h->fast, 0, sizeof(h->fast));
	for (sym = 0; sym < n; sym++)
		h->count[lengths[sym]]++;
	if (h->count[0] == n)
		return 1;		// no codes (a distance table can be empty)
	left = 1;
	for (len = 1; len < 16; len++) {
		left <<= 1;
		left -= h->count[len];
		if (left < 0)
			return 0;	// more codes than possible
	}
	offs[1] = 0;
	for (len = 1; len < 15; len++)
		offs[len + 1] = offs[len] + h->count[len];
	for (sym = 0; sym < n; sym++)
		if (lengths[sym])
			h->symbol[offs[lengths[sym]]++] = (int16_t) sym;
	// The fast table: the codes of INF_FAST bits or less, bit reversed
	{
		uint32_t next[16], c = 0;
		for (len = 1; len < 16; len++) {
			c = (c + (len > 1 ? (uint32_t) h->count[len - 1] : 0)) << 1;
			next[len] = c;
		}
		for (sym = 0; sym < n; sym++) {
			int l = lengths[sym], b;
			uint32_t r = 0, code;
			if (l == 0 || l > INF_FAST)
				continue;
			code = next[l]++;
			for (b = 0; b < l; b++)
				r |= ((code >> b) & 1) << (l - 1 - b);
			for (; r < (1u << INF_FAST); r += 1u << l)
				h->fast[r] = (uint16_t) (sym << 4 | l);
		}
	}
	return 1;
}

static int inf_decode(inf_state *s, const inf_huff *h)
{
	int len, code, first, index, count;
	uint16_t e;

	inf_need(s, 16);
	e = h->fast[s->bits & ((1u << INF_FAST) - 1)];
	if (e) {
		s->bits >>= e & 15;
		s->nbits -= e & 15;
		return e >> 4;
	}
	// A longer code: one bit after the other (RFC 1951, 3.2.2)
	code = first = index = 0;
	for (len = 1; len < 16; len++) {
		code |= (int) (s->bits & 1);
		s->bits >>= 1;
		s->nbits--;
		count = h->count[len];
		if (code - count < first)
			return h->symbol[index + (code - first)];
		index += count;
		first += count;
		first <<= 1;
		code <<= 1;
	}
	return -1;
}

static const uint16_t inf_len_base[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
	35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const uint8_t inf_len_extra[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
	3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const uint16_t inf_dist_base[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
	257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const uint8_t inf_dist_extra[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
	7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static int inf_header(inf_state *s)
{
	s->last = (int) inf_bits(s, 1);
	switch (inf_bits(s, 2)) {
	case 0: {
		uint32_t len, nlen;
		// The stored data starts at a byte
		s->bits >>= s->nbits & 7;
		s->nbits -= s->nbits & 7;
		len = inf_bits(s, 16);
		nlen = inf_bits(s, 16);
		if ((len ^ 0xffff) != nlen)
			return 0;
		s->stored_left = len;
		s->type = 0;
		return 1;
	}
	case 1: {
		uint8_t l[320];
		int i;
		for (i = 0; i < 144; i++) l[i] = 8;
		for (; i < 256; i++) l[i] = 9;
		for (; i < 280; i++) l[i] = 7;
		for (; i < 288; i++) l[i] = 8;
		inf_build(&s->len, l, 288);
		for (i = 0; i < 30; i++) l[i] = 5;
		inf_build(&s->dist, l, 30);
		s->type = 1;
		return 1;
	}
	case 2: {
		static const uint8_t order[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
		uint8_t l[320];
		int nlen = (int) inf_bits(s, 5) + 257, ndist = (int) inf_bits(s, 5) + 1, ncode = (int) inf_bits(s, 4) + 4;
		int i;
		if (nlen > 286 || ndist > 30)
			return 0;
		memset(l, 0, sizeof(l));
		for (i = 0; i < ncode; i++)
			l[order[i]] = (uint8_t) inf_bits(s, 3);
		if (!inf_build(&s->len, l, 19))
			return 0;
		for (i = 0; i < nlen + ndist; ) {
			int sym = inf_decode(s, &s->len), rep, v = 0;
			if (sym < 0)
				return 0;
			if (sym < 16) {
				l[i++] = (uint8_t) sym;
				continue;
			}
			if (sym == 16) {
				if (i == 0)
					return 0;
				v = l[i - 1];
				rep = 3 + (int) inf_bits(s, 2);
			} else if (sym == 17)
				rep = 3 + (int) inf_bits(s, 3);
			else
				rep = 11 + (int) inf_bits(s, 7);
			if (i + rep > nlen + ndist)
				return 0;
			while (rep--)
				l[i++] = (uint8_t) v;
		}
		if (l[256] == 0)
			return 0;	// no end-of-block code
		{
			uint8_t d[30];
			memcpy(d, l + nlen, ndist);
			if (!inf_build(&s->len, l, nlen) || !inf_build(&s->dist, d, ndist))
				return 0;
		}
		s->type = 1;
		return 1;
	}
	default:
		return 0;
	}
}

// Decode up to size bytes into out. Returns the bytes made; less than size
// at the end of the data or after an error (s->error).
static size_t inf_read(inf_state *s, uint8_t *out, size_t size)
{
	size_t got = 0;

	while (got < size && !s->error && !s->done) {
		if (s->match_len) {
			while (s->match_len && got < size) {
				uint8_t b = s->window[(s->total - s->match_dist) & (INF_WSIZE - 1)];
				s->window[s->total & (INF_WSIZE - 1)] = b;
				out[got++] = b;
				s->total++;
				s->match_len--;
			}
		} else if (s->type == 0 && s->stored_left) {
			uint8_t b;
			if (s->nbits >= 8) {
				b = (uint8_t) (s->bits & 255);
				s->bits >>= 8;
				s->nbits -= 8;
			} else
				b = (uint8_t) inf_byte(s);
			s->window[s->total & (INF_WSIZE - 1)] = b;
			out[got++] = b;
			s->total++;
			s->stored_left--;
		} else if (s->type == 1) {
			int sym = inf_decode(s, &s->len);
			if (sym < 0 || sym > 285) {
				s->error = 1;
			} else if (sym < 256) {
				s->window[s->total & (INF_WSIZE - 1)] = (uint8_t) sym;
				out[got++] = (uint8_t) sym;
				s->total++;
			} else if (sym == 256) {
				s->type = -1;
			} else {
				int d;
				sym -= 257;
				s->match_len = inf_len_base[sym] + inf_bits(s, inf_len_extra[sym]);
				d = inf_decode(s, &s->dist);
				if (d < 0 || d > 29) {
					s->error = 1;
					break;
				}
				s->match_dist = inf_dist_base[d] + inf_bits(s, inf_dist_extra[d]);
				if (s->match_dist > s->total)
					s->error = 1;
			}
		} else {
			// The end of a block, or the start of the data
			if (s->last) {
				s->done = 1;
				break;
			}
			if (!inf_header(s))
				s->error = 1;
		}
		if (s->overrun > 4)
			s->error = 1;	// the data ends in the middle of a block
	}
	return got;
}

// ---------------------------------------------------------------------
// ZIP

static uint32_t u16(const uint8_t *p) { return p[0] | (p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
static uint64_t u64(const uint8_t *p) { return u32(p) | ((uint64_t) u32(p + 4) << 32); }

static int read_at(FILE *f, int64_t pos, void *buf, size_t size)
{
	return pos >= 0 && inst_fseek(f, pos, SEEK_SET) == 0 && fread(buf, 1, size, f) == size;
}

int inst_zip_open(inst_zip *z, const char *path)
{
	uint8_t *tail = NULL, *cd = NULL;
	int64_t size, tail_size, i, eocd = -1, end_pos;
	uint64_t n, cd_size, cd_offset, pos;

	memset(z, 0, sizeof(*z));
	z->f = fopen(path, "rb");
	if (!z->f)
		return 0;
	inst_fseek(z->f, 0, SEEK_END);
	size = inst_ftell(z->f);
	tail_size = size < 65557 ? size : 65557;
	if (size < 22)
		goto fail;
	tail = malloc(tail_size);
	if (!tail || !read_at(z->f, size - tail_size, tail, tail_size))
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
	end_pos = size - tail_size + eocd;		// the central directory ends here
	{
		// ZIP64: the locator is just before the end record. It gives the
		// offset of the ZIP64 end record, without the self-extracting
		// program. Some programs write the ZIP64 records also when the
		// values fit in the end record: the central directory then ends
		// before them.
		uint8_t loc[20], rec[56];
		int64_t rec_pos;
		int zip64 = n == 0xffff || cd_size == 0xffffffff || cd_offset == 0xffffffff;
		if (!read_at(z->f, end_pos - 20, loc, 20) || u32(loc) != 0x07064b50) {
			if (zip64)
				goto fail;
			goto entries;
		}
		rec_pos = (int64_t) u64(loc + 8);
		if (!read_at(z->f, rec_pos, rec, 56) || u32(rec) != 0x06064b50) {
			// A program before the archive: the record is just before the locator.
			rec_pos = end_pos - 20 - 56;
			if (!read_at(z->f, rec_pos, rec, 56) || u32(rec) != 0x06064b50)
				goto fail;
		}
		n = u64(rec + 32);
		cd_size = u64(rec + 40);
		cd_offset = u64(rec + 48);
		end_pos = rec_pos;
	}
entries:
	// A self-extracting program before the archive moves all the offsets.
	z->delta = end_pos - (int64_t) (cd_offset + cd_size);
	if (z->delta < 0 || cd_size > (1u << 30) || n > 1000000)
		goto fail;
	cd = malloc(cd_size ? (size_t) cd_size : 1);
	z->e = calloc(n ? (size_t) n : 1, sizeof(inst_zip_entry));
	if (!cd || !z->e || !read_at(z->f, (int64_t) cd_offset + z->delta, cd, (size_t) cd_size))
		goto fail;
	for (pos = 0; (uint64_t) z->n < n && pos + 46 <= cd_size; z->n++) {
		inst_zip_entry *e = &z->e[z->n];
		uint32_t nl, xl, cl, x;
		if (u32(cd + pos) != 0x02014b50)
			goto fail;
		e->flags = u16(cd + pos + 8);
		e->method = u16(cd + pos + 10);
		e->crc = u32(cd + pos + 16);
		e->csize = u32(cd + pos + 20);
		e->usize = u32(cd + pos + 24);
		nl = u16(cd + pos + 28);
		xl = u16(cd + pos + 30);
		cl = u16(cd + pos + 32);
		e->local_offset = u32(cd + pos + 42);
		if (pos + 46 + nl + xl > cd_size || nl >= sizeof(e->name))
			goto fail;
		memcpy(e->name, cd + pos + 46, nl);
		e->name[nl] = 0;
		// The ZIP64 extra field: the 64-bit values of the fields that are 0xffffffff
		for (x = 0; x + 4 <= xl; ) {
			const uint8_t *p = cd + pos + 46 + nl + x;
			uint32_t id = u16(p), len = u16(p + 2), k = 4;
			if (x + 4 + len > xl)
				break;
			if (id == 0x0001) {
				if (e->usize == 0xffffffff && k + 8 <= 4 + len) { e->usize = u64(p + k); k += 8; }
				if (e->csize == 0xffffffff && k + 8 <= 4 + len) { e->csize = u64(p + k); k += 8; }
				if (e->local_offset == 0xffffffff && k + 8 <= 4 + len) { e->local_offset = u64(p + k); k += 8; }
			}
			x += 4 + len;
		}
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

// The file offset of the data of an entry, or -1.
static int64_t zip_data_pos(inst_zip *z, inst_zip_entry *e)
{
	uint8_t h[30];
	int64_t p = (int64_t) e->local_offset + z->delta;

	if (!read_at(z->f, p, h, 30) || u32(h) != 0x04034b50)
		return -1;
	return p + 30 + u16(h + 26) + u16(h + 28);
}

struct inst_zip_reader {
	inst_zip *z;
	inst_zip_entry *e;
	int64_t data_pos;
	uint64_t pos;			// the offset in the entry of the next byte
	uint32_t crc;			// of the bytes 0 .. pos
	inf_state *inf;			// method 8
};

static int reader_start(inst_zip_reader *r)
{
	r->pos = 0;
	r->crc = 0;
	if (r->inf) {
		memset(r->inf, 0, offsetof(inf_state, len));
		r->inf->f = r->z->f;
		r->inf->in_pos = r->data_pos;
		r->inf->in_left = r->e->csize;
		r->inf->type = -1;
		r->inf->total = 0;
		r->inf->error = r->inf->done = 0;
	}
	return 1;
}

inst_zip_reader *inst_zip_reader_open(inst_zip *z, int i)
{
	inst_zip_reader *r;

	if (i < 0 || i >= z->n)
		return NULL;
	if ((z->e[i].flags & 1) || (z->e[i].method != 0 && z->e[i].method != 8) ||
		(z->e[i].method == 0 && z->e[i].csize != z->e[i].usize))
		return NULL;		// encrypted, or another method
	r = calloc(1, sizeof(*r));
	if (!r)
		return NULL;
	r->z = z;
	r->e = &z->e[i];
	r->data_pos = zip_data_pos(z, r->e);
	if (r->data_pos < 0) {
		free(r);
		return NULL;
	}
	if (r->e->method == 8) {
		r->inf = malloc(sizeof(inf_state));
		if (!r->inf) {
			free(r);
			return NULL;
		}
	}
	reader_start(r);
	return r;
}

void inst_zip_reader_close(inst_zip_reader *r)
{
	if (r) {
		free(r->inf);
		free(r);
	}
}

// Read the next size bytes. 1 if all of them were read.
static int reader_next(inst_zip_reader *r, uint8_t *buf, size_t size)
{
	if (size > r->e->usize - r->pos)
		return 0;
	if (r->inf) {
		if (inf_read(r->inf, buf, size) != size)
			return 0;
	} else if (!read_at(r->z->f, r->data_pos + (int64_t) r->pos, buf, size))
		return 0;
	r->crc = crc32_add(r->crc, buf, size);
	r->pos += size;
	return 1;
}

int inst_zip_reader_read_at(inst_zip_reader *r, uint64_t offset, void *buf, size_t size)
{
	static uint8_t skip[1 << 16];

	if (offset > r->e->usize || size > r->e->usize - offset)
		return 0;
	if (offset < r->pos)
		reader_start(r);
	while (r->pos < offset) {
		uint64_t k = offset - r->pos;
		if (k > sizeof(skip))
			k = sizeof(skip);
		if (!reader_next(r, skip, (size_t) k))
			return 0;
	}
	return reader_next(r, buf, size);
}

int inst_zip_reader_finish(inst_zip_reader *r)
{
	static uint8_t buf[1 << 16];

	while (r->pos < r->e->usize) {
		uint64_t k = r->e->usize - r->pos;
		if (k > sizeof(buf))
			k = sizeof(buf);
		if (!reader_next(r, buf, (size_t) k))
			return 0;
	}
	return r->crc == r->e->crc;
}

// Read an entry from the start to the end: write to out (if not NULL),
// hash and count. 1 if successful and the CRC matches.
static int zip_stream(inst_zip *z, int i, FILE *out, char hex[65],
	void (*progress)(int64_t done, void *arg), void *arg)
{
	static uint8_t buf[1 << 16];
	inst_zip_reader *r = inst_zip_reader_open(z, i);
	inst_sha256 sha;
	int ok = 1;

	hex[0] = 0;
	if (!r)
		return 0;
	inst_sha256_init(&sha);
	while (ok && r->pos < r->e->usize) {
		uint64_t k = r->e->usize - r->pos;
		if (k > sizeof(buf))
			k = sizeof(buf);
		if (!reader_next(r, buf, (size_t) k) || (out && fwrite(buf, 1, (size_t) k, out) != k))
			ok = 0;
		else {
			inst_sha256_add(&sha, buf, (size_t) k);
			if (progress)
				progress((int64_t) k, arg);
		}
	}
	if (ok && r->crc != r->e->crc)
		ok = 0;
	inst_zip_reader_close(r);
	inst_sha256_hex(&sha, hex);
	return ok;
}

int inst_zip_extract(inst_zip *z, int i, const char *to, char hex[65],
	void (*progress)(int64_t done, void *arg), void *arg)
{
	FILE *out;
	int ok;

	hex[0] = 0;
	if (i < 0 || i >= z->n)
		return 0;
	out = fopen(to, "wb");
	if (!out)
		return 0;
	ok = zip_stream(z, i, out, hex, progress, arg);
	if (fclose(out))
		ok = 0;
	return ok;
}

int inst_zip_sha256(inst_zip *z, int i, char hex[65])
{
	hex[0] = 0;
	if (i < 0 || i >= z->n)
		return 0;
	return zip_stream(z, i, NULL, hex, NULL, NULL);
}

char *inst_zip_read(inst_zip *z, int i, uint32_t *size)
{
	inst_zip_reader *r;
	char *u;

	if (i < 0 || i >= z->n || z->e[i].usize >= (1u << 30))
		return NULL;
	r = inst_zip_reader_open(z, i);
	u = malloc((size_t) z->e[i].usize + 1);
	if (!r || !u || !inst_zip_reader_read_at(r, 0, u, (size_t) z->e[i].usize) || r->crc != z->e[i].crc) {
		inst_zip_reader_close(r);
		free(u);
		return NULL;
	}
	inst_zip_reader_close(r);
	u[z->e[i].usize] = 0;
	if (size)
		*size = (uint32_t) z->e[i].usize;
	return u;
}

// ---------------------------------------------------------------------
// ISO 9660

// Read bytes of the image file (or of the ZIP entry).
static int iso_raw(inst_iso *iso, int64_t pos, uint8_t *out, uint32_t size)
{
	if (iso->zr)
		return inst_zip_reader_read_at(iso->zr, (uint64_t) pos, out, size);
	return read_at(iso->f, pos, out, size);
}

static int iso_read(inst_iso *iso, uint32_t lba, uint32_t offset, uint8_t *out, uint32_t size)
{
	if (iso->sector_size == 2048)
		return iso_raw(iso, (int64_t) lba * 2048 + offset, out, size);
	while (size > 0) {
		uint32_t s = lba + offset / 2048, o = offset % 2048;
		uint32_t k = 2048 - o;
		if (k > size)
			k = size;
		if (!iso_raw(iso, (int64_t) s * iso->sector_size + iso->data_offset + o, out, k))
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

// The cue sheet: the name of the FILE (as written) and the sector format
// of track 1. 1 if found; 0 without a file or a track; -1 if track 1 is
// not a data track.
int inst_cue_parse(const char *text, char *bin, int bin_size, int *sector_size, int *data_offset)
{
	int have_file = 0, have_track = 0;
	const char *line = text;

	while (*line) {
		char s[1024];
		const char *end = line + strcspn(line, "\r\n"), *t;
		int n = (int) (end - line);
		if (n >= (int) sizeof(s))
			n = sizeof(s) - 1;
		memcpy(s, line, n);
		s[n] = 0;
		line = *end ? end + 1 : end;
		for (t = s; *t == ' ' || *t == '\t'; t++)
			;
		if (!have_file && !strncmp(t, "FILE", 4)) {
			const char *q1 = strchr(t, '"'), *q2 = q1 ? strchr(q1 + 1, '"') : NULL;
			if (q1 && q2) {
				snprintf(bin, bin_size, "%.*s", (int) (q2 - q1 - 1), q1 + 1);
				have_file = 1;
			}
		} else if (have_file && !have_track && !strncmp(t, "TRACK", 5)) {
			have_track = 1;
			if (strstr(t, "MODE1/2048")) { *sector_size = 2048; *data_offset = 0; }
			else if (strstr(t, "MODE1/2352")) { *sector_size = 2352; *data_offset = 16; }
			else if (strstr(t, "MODE2/2352")) { *sector_size = 2352; *data_offset = 24; }
			else
				return -1;
		}
	}
	return have_file && have_track;
}

// The .bin file named in a .cue sheet file, and the sector format of track 1.
static int parse_cue(const char *cue, char *bin, int bin_size, int *sector_size, int *data_offset, char *err, int err_size)
{
	FILE *f = fopen(cue, "rb");
	char text[16384], name[INST_PATH], dir[INST_PATH];
	size_t n;
	char *p;
	int rc;

	if (!f) {
		snprintf(err, err_size, "Cannot open %s.", cue);
		return 0;
	}
	n = fread(text, 1, sizeof(text) - 1, f);
	text[n] = 0;
	fclose(f);
	rc = inst_cue_parse(text, name, sizeof(name), sector_size, data_offset);
	if (rc < 0) {
		snprintf(err, err_size, "The first track of %s is not a data track.", cue);
		return 0;
	}
	if (rc == 0) {
		snprintf(err, err_size, "%s is not a cue sheet with a data track.", cue);
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
	if (name[0] && (name[1] == ':' || name[0] == '\\' || name[0] == '/'))
		snprintf(bin, bin_size, "%s", name);
	else
		snprintf(bin, bin_size, "%s%s", dir, name);
	return 1;
}

// Find the sector format (if not given) and read the directories.
static int iso_start(inst_iso *iso, int sector_size, int data_offset, const char *name, char *err, int err_size)
{
	static const int formats[3][2] = { { 2048, 0 }, { 2352, 16 }, { 2352, 24 } };
	uint8_t pvd[2048];
	int i;

	for (i = 0; i < 3; i++) {
		if (sector_size && (formats[i][0] != sector_size || formats[i][1] != data_offset))
			continue;
		iso->sector_size = formats[i][0];
		iso->data_offset = formats[i][1];
		if (iso_read(iso, 16, 0, pvd, 2048) && pvd[0] == 1 && !memcmp(pvd + 1, "CD001", 5))
			break;
	}
	if (i == 3) {
		snprintf(err, err_size, "%s is not an ISO 9660 disc image.", name);
		inst_iso_close(iso);
		return 0;
	}
	if (!iso_dir(iso, u32(pvd + 156 + 2), u32(pvd + 156 + 10), "", 0)) {
		snprintf(err, err_size, "Cannot read the directories of %s.", name);
		inst_iso_close(iso);
		return 0;
	}
	return 1;
}

int inst_iso_open(inst_iso *iso, const char *path, char *err, int err_size)
{
	char bin[INST_PATH];
	int sector_size = 0, data_offset = 0;

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
	return iso_start(iso, sector_size, data_offset, bin, err, err_size);
}

int inst_iso_open_zip(inst_iso *iso, inst_zip *z, int i, int sector_size, int data_offset,
	const char *name, char *err, int err_size)
{
	memset(iso, 0, sizeof(*iso));
	iso->zr = inst_zip_reader_open(z, i);
	if (!iso->zr) {
		snprintf(err, err_size, "Cannot read %s in the ZIP archive (not stored or deflate, or encrypted).", name);
		return 0;
	}
	return iso_start(iso, sector_size, data_offset, name, err, err_size);
}

void inst_iso_close(inst_iso *iso)
{
	if (iso->f)
		fclose(iso->f);
	inst_zip_reader_close(iso->zr);
	free(iso->files);
	memset(iso, 0, sizeof(*iso));
}

int inst_iso_read(inst_iso *iso, const inst_iso_file *file, uint32_t offset, void *buf, uint32_t size)
{
	if (offset > file->size || size > file->size - offset)
		return 0;
	return iso_read(iso, file->lba, offset, buf, size);
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
		if (!iso_read(iso, file->lba, done, buf, k) || fwrite(buf, 1, k, out) != k)
			ok = 0;
		done += k;
		if (progress)
			progress(k, arg);
	}
	if (fclose(out))
		ok = 0;
	return ok;
}
