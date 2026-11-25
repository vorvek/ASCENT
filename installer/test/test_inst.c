/*
 * ASCENT: the test of the ZIP reader of the installer tools (inst.c): the
 * ZIP64 records, the stored entries and the deflate decoder (dynamic and
 * fixed codes, stored blocks), the reader with a seek back, the CRC check
 * and the cue sheet parser.
 *
 *   test_inst DIR     (DIR: a folder for the temporary files)
 *
 * test_zip_data.h is made by make_test_zip.py.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inst.h"
#include "test_zip_data.h"

static int Failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); Failures++; } } while (0)

static unsigned char *make_text(size_t *n)
{
	static const char *words[7] = { "fire", "water", "stone", "the", "of", "dungeon", "dark" };
	unsigned char *t = malloc(15000 * 9), *p = t;
	int i;

	for (i = 0; i < 15000; i++) {
		size_t k = strlen(words[(long) i * 7919 % 10007 % 7]);
		memcpy(p, words[(long) i * 7919 % 10007 % 7], k);
		p += k;
		*p++ = ' ';
	}
	*n = (size_t) (p - t);
	return t;
}

static unsigned char *make_noise(size_t n)
{
	unsigned char *t = malloc(n);
	unsigned int x = 12345;
	size_t i;

	for (i = 0; i < n; i++) {
		x = x * 1103515245u + 12345u;
		t[i] = (unsigned char) (x >> 16);
	}
	return t;
}

static void write_file(const char *path, const unsigned char *data, size_t n)
{
	FILE *f = fopen(path, "wb");
	CHECK(f != NULL);
	if (f) {
		CHECK(fwrite(data, 1, n, f) == n);
		fclose(f);
	}
}

static void sha_of(const unsigned char *data, size_t n, char hex[65])
{
	inst_sha256 c;
	inst_sha256_init(&c);
	inst_sha256_add(&c, data, n);
	inst_sha256_hex(&c, hex);
}

// The offset of the data of an entry in Test_zip (after the local header)
static size_t data_offset(const char *name)
{
	size_t i, n = strlen(name);
	for (i = 0; i + 30 + n <= sizeof(Test_zip); i++)
		if (Test_zip[i] == 'P' && Test_zip[i + 1] == 'K' && Test_zip[i + 2] == 3 && Test_zip[i + 3] == 4 &&
			Test_zip[i + 26] == n && !memcmp(Test_zip + i + 30, name, n))
			return i + 30 + n + (Test_zip[i + 28] | Test_zip[i + 29] << 8);
	return 0;
}

int main(int argc, char **argv)
{
	char path[INST_PATH], out[INST_PATH], hex[65], want[65];
	unsigned char *text, *noise5, *noise1, buf[200];
	size_t text_n;
	inst_zip z;
	int i;

	if (argc < 2) {
		printf("usage: test_inst DIR\n");
		return 2;
	}
	text = make_text(&text_n);
	noise5 = make_noise(5000);
	noise1 = make_noise(1000);
	inst_join(path, sizeof(path), argv[1], "test.zip");
	write_file(path, Test_zip, sizeof(Test_zip));

	CHECK(inst_zip_open(&z, path));
	CHECK(z.n == 5);
	if (z.n == 5) {
		static const char *names[5] = { "text.txt", "short.txt", "noise.bin", "plain.bin", "sub/x.cue" };
		uint32_t size;
		char *d;
		for (i = 0; i < 5; i++)
			CHECK(inst_zip_find(&z, names[i]) == i);
		CHECK(z.e[0].method == 8 && z.e[3].method == 0);
		CHECK(z.e[0].usize == text_n);

		d = inst_zip_read(&z, 0, &size);
		CHECK(d && size == text_n && !memcmp(d, text, text_n));
		free(d);
		d = inst_zip_read(&z, 1, &size);
		CHECK(d && size == 23 && !strcmp(d, "hello hello hello hello"));
		free(d);
		d = inst_zip_read(&z, 2, &size);
		CHECK(d && size == 5000 && !memcmp(d, noise5, 5000));
		free(d);
		d = inst_zip_read(&z, 3, &size);
		CHECK(d && size == 1000 && !memcmp(d, noise1, 1000));
		free(d);

		// Extract and hash
		inst_join(out, sizeof(out), argv[1], "text.txt");
		CHECK(inst_zip_extract(&z, 0, out, hex, NULL, NULL));
		sha_of(text, text_n, want);
		CHECK(!strcmp(hex, want));
		CHECK(inst_sha256_file(out, hex) && !strcmp(hex, want));
		CHECK(inst_zip_sha256(&z, 0, hex) && !strcmp(hex, want));
		remove(out);

		// The reader: forward, back (a new start), forward, then the CRC
		{
			inst_zip_reader *r = inst_zip_reader_open(&z, 0);
			CHECK(r != NULL);
			if (r) {
				CHECK(inst_zip_reader_read_at(r, 40000, buf, 150) && !memcmp(buf, text + 40000, 150));
				CHECK(inst_zip_reader_read_at(r, 10, buf, 100) && !memcmp(buf, text + 10, 100));
				CHECK(inst_zip_reader_read_at(r, text_n - 100, buf, 100) && !memcmp(buf, text + text_n - 100, 100));
				CHECK(!inst_zip_reader_read_at(r, text_n - 10, buf, 11));	// past the end
				CHECK(inst_zip_reader_read_at(r, 0, buf, 1));
				CHECK(inst_zip_reader_finish(r));
				inst_zip_reader_close(r);
			}
		}

		// The cue sheet
		d = inst_zip_read(&z, 4, &size);
		CHECK(d != NULL);
		if (d) {
			char bin[INST_PATH];
			int ss = 0, off = 0;
			CHECK(inst_cue_parse(d, bin, sizeof(bin), &ss, &off) == 1);
			CHECK(!strcmp(bin, "..\\DISC.BIN") && ss == 2352 && off == 16);
			CHECK(inst_cue_parse("FILE \"a.bin\" BINARY\n  TRACK 01 AUDIO\n", bin, sizeof(bin), &ss, &off) == -1);
			CHECK(inst_cue_parse("REM nothing\n", bin, sizeof(bin), &ss, &off) == 0);
			free(d);
		}
	}
	inst_zip_close(&z);

	// A damaged entry: the CRC (or the decoder) finds it
	{
		unsigned char *bad = malloc(sizeof(Test_zip));
		size_t o = data_offset("text.txt");
		CHECK(o > 0);
		memcpy(bad, Test_zip, sizeof(Test_zip));
		bad[o + 200] ^= 0x10;
		write_file(path, bad, sizeof(Test_zip));
		CHECK(inst_zip_open(&z, path));
		CHECK(inst_zip_read(&z, 0, NULL) == NULL);
		CHECK(!inst_zip_sha256(&z, 0, hex));
		inst_zip_close(&z);
		// A cut archive: no end record
		write_file(path, Test_zip, sizeof(Test_zip) / 2);
		CHECK(!inst_zip_open(&z, path));
		free(bad);
	}
	remove(path);
	free(text);
	free(noise5);
	free(noise1);
	if (Failures)
		printf("%d failures\n", Failures);
	else
		printf("test_inst: all passed\n");
	return Failures != 0;
}
