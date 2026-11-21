/*
 * ASCENT test for the IFF bitmap reader and writer (iff.c).
 * New file. It replaces no original file.
 *
 * Usage: test_iff <directory for temporary files>
 *
 * The test writes bitmaps as PBM files and reads them again. It uses a
 * wide bitmap (the writer compresses it), a narrow one (not compressed)
 * and one with an odd width (each row has a pad byte). It also reads an
 * ILBM file (bit planes) that the test builds byte by byte, and checks the
 * error codes for a file that does not exist and for a file that is not IFF.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mem.h"		/* the bitmaps come from the allocator of mem.h */
#include "iff.h"

static int n_failed = 0;

#define CHECK(expr) do { \
		if (!(expr)) { \
			printf("FAILED, line %d: %s\n", __LINE__, #expr); \
			n_failed++; \
		} \
	} while (0)

static char tmp_dir[300];

static char *tmp_name(const char *name)
{
	static char path[400];

	snprintf(path, sizeof(path), "%s/%s", tmp_dir, name);
	return path;
}

/* Writes a bitmap, reads it again and compares. */
static void round_trip(const char *name, int w, int h)
{
	grs_bitmap bm, bm2;
	ubyte pal[768], pal2[768];
	ubyte *data;
	int x, y, i, ret, same;

	data = malloc(w * h);
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			/* runs of equal bytes and bytes that are all different */
			if (y % 3 == 0)
				data[y * w + x] = (ubyte) (y + x / 20);
			else if (y % 3 == 1)
				data[y * w + x] = (ubyte) (x * 5 + y);
			else
				data[y * w + x] = (ubyte) ((x % 7 < 3) ? 200 : x);
		}
	for (i = 0; i < 768; i++)
		pal[i] = (ubyte) ((i * 3 + i / 3) & 63);	/* the palette has 6 bits for each color */

	memset(&bm, 0, sizeof(bm));
	bm.bm_w = w;
	bm.bm_h = h;
	bm.bm_type = BM_LINEAR;
	bm.bm_rowsize = w;
	bm.bm_data = data;

	iff_has_transparency = 0;
	iff_transparent_color = 0;

	ret = iff_write_bitmap(tmp_name(name), &bm, pal);
	CHECK(ret == IFF_NO_ERROR);

	memset(&bm2, 0, sizeof(bm2));
	memset(pal2, 0xff, sizeof(pal2));
	ret = iff_read_bitmap(tmp_name(name), &bm2, BM_LINEAR, pal2);
	CHECK(ret == IFF_NO_ERROR);
	if (ret != IFF_NO_ERROR)
		printf("%s: %s\n", name, iff_errormsg(ret));
	if (ret == IFF_NO_ERROR) {
		CHECK(bm2.bm_w == w);
		CHECK(bm2.bm_h == h);
		CHECK(bm2.bm_rowsize == w);
		CHECK(bm2.bm_type == BM_LINEAR);
		CHECK(bm2.bm_data != NULL);
		same = bm2.bm_data != NULL && bm2.bm_w == w && bm2.bm_h == h && memcmp(bm2.bm_data, data, w * h) == 0;
		CHECK(same);
		CHECK(memcmp(pal, pal2, 768) == 0);
		CHECK(iff_has_transparency == 0);

		/* Read the file again into the bitmap that exists. */
		memset(bm2.bm_data, 0, w * h);
		ret = iff_read_into_bitmap(tmp_name(name), &bm2, NULL);
		CHECK(ret == IFF_NO_ERROR);
		CHECK(memcmp(bm2.bm_data, data, w * h) == 0);
		free(bm2.bm_data);
	}
	free(data);
}

static void put32(FILE *f, unsigned n)
{
	fputc((n >> 24) & 0xff, f);
	fputc((n >> 16) & 0xff, f);
	fputc((n >> 8) & 0xff, f);
	fputc(n & 0xff, f);
}

static void put16(FILE *f, unsigned n)
{
	fputc((n >> 8) & 0xff, f);
	fputc(n & 0xff, f);
}

/* An ILBM file: 16 x 2 pixels, 4 bit planes, not compressed. The pixel
   (x,y) has the color (x + y) & 15. The transparent color is 5. */
static void ilbm_test(void)
{
	grs_bitmap bm;
	ubyte pal[768];
	FILE *f;
	int x, y, p, i, ret, ok;

	f = fopen(tmp_name("planes.lbm"), "wb");
	if (!f) {
		printf("cannot create %s\n", tmp_name("planes.lbm"));
		exit(1);
	}
	fwrite("FORM", 1, 4, f);
	put32(f, 4 + (8 + 20) + (8 + 48) + (8 + 16));
	fwrite("ILBM", 1, 4, f);

	fwrite("BMHD", 1, 4, f);
	put32(f, 20);
	put16(f, 16);		/* w */
	put16(f, 2);		/* h */
	put16(f, 0);		/* x */
	put16(f, 0);		/* y */
	fputc(4, f);		/* planes */
	fputc(2, f);		/* masking: has a transparent color */
	fputc(0, f);		/* compression: none */
	fputc(0, f);		/* pad */
	put16(f, 5);		/* transparent color */
	fputc(1, f);		/* x aspect */
	fputc(1, f);		/* y aspect */
	put16(f, 16);		/* page width */
	put16(f, 2);		/* page height */

	fwrite("CMAP", 1, 4, f);
	put32(f, 48);
	for (i = 0; i < 48; i++)
		fputc(i * 4, f);

	fwrite("BODY", 1, 4, f);
	put32(f, 16);
	for (y = 0; y < 2; y++)
		for (p = 0; p < 4; p++) {
			unsigned bits = 0;

			for (x = 0; x < 16; x++)
				if (((x + y) & 15) & (1 << p))
					bits |= 0x8000 >> x;
			put16(f, bits);
		}
	fclose(f);

	memset(&bm, 0, sizeof(bm));
	memset(pal, 0, sizeof(pal));
	ret = iff_read_bitmap(tmp_name("planes.lbm"), &bm, BM_LINEAR, pal);
	CHECK(ret == IFF_NO_ERROR);
	if (ret == IFF_NO_ERROR) {
		CHECK(bm.bm_w == 16);
		CHECK(bm.bm_h == 2);
		CHECK(bm.bm_type == BM_LINEAR);
		ok = 1;
		for (y = 0; y < 2; y++)
			for (x = 0; x < 16; x++)
				if (bm.bm_data[y * 16 + x] != ((x + y) & 15))
					ok = 0;
		CHECK(ok);
		ok = 1;
		for (i = 0; i < 48; i++)
			if (pal[i] != i)		/* the reader makes 6 bits from 8 */
				ok = 0;
		CHECK(ok);
		CHECK(iff_has_transparency == 1);
		CHECK(iff_transparent_color == 5);
		free(bm.bm_data);
	}
	iff_has_transparency = 0;
}

int main(int argc, char **argv)
{
	grs_bitmap bm;
	FILE *f;

	if (argc < 2) {
		printf("Usage: test_iff <directory for temporary files>\n");
		return 1;
	}
	snprintf(tmp_dir, sizeof(tmp_dir), "%s", argv[1]);

	round_trip("wide.bbm", 320, 20);		/* compressed */
	round_trip("narrow.bbm", 32, 9);		/* not compressed */
	round_trip("odd.bbm", 131, 7);		/* compressed, odd width */
	round_trip("oddsmall.bbm", 33, 5);	/* not compressed, odd width */
	round_trip("w128.bbm", 128, 5);		/* a row is one run of 128 different bytes */
	round_trip("w129.bbm", 129, 5);
	round_trip("w127.bbm", 127, 5);
	ilbm_test();

	/* error codes */
	memset(&bm, 0, sizeof(bm));
	CHECK(iff_read_bitmap(tmp_name("nofile.bbm"), &bm, BM_LINEAR, NULL) == IFF_NO_FILE);

	f = fopen(tmp_name("notiff.bbm"), "wb");
	if (f) {
		fwrite("This is not an IFF file.", 1, 24, f);
		fclose(f);
	}
	CHECK(iff_read_bitmap(tmp_name("notiff.bbm"), &bm, BM_LINEAR, NULL) == IFF_NOT_IFF);

	CHECK(!strcmp(iff_errormsg(IFF_NO_ERROR), "No error."));
	CHECK(!strcmp(iff_errormsg(IFF_NO_FILE), "Cannot open file."));
	CHECK(!strcmp(iff_errormsg(IFF_READ_ERROR), "Error reading file."));

	if (n_failed) {
		printf("test_iff: %d checks failed\n", n_failed);
		return 1;
	}
	printf("test_iff: ok\n");
	return 0;
}
