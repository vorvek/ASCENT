/*
 * ASCENT test for the HOG file reader (cfile.c).
 * New file. It replaces no original file.
 *
 * Usage: test_cfile <directory for temporary files>
 *
 * The test makes a small hog file and some loose files in the directory.
 * It tests: open, length, read, getc, gets, seek and tell on a hog entry,
 * the match of names without case, loose files, the alternate hog file
 * and the alternate directory.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <direct.h>
#define chdir _chdir
#define make_dir(name) _mkdir(name)
#elif defined(_WIN32)
#include <unistd.h>
#include <direct.h>
#define make_dir(name) _mkdir(name)
#else
#include <unistd.h>
#include <sys/stat.h>
#define make_dir(name) mkdir(name, 0777)
#endif

#include "cfile.h"

#ifndef _MSC_VER
/* in cfile.c, not in cfile.h */
extern int cfile_find_nocase(const char *path, char *out, int out_size);
#endif

static int n_failed = 0;

#define CHECK(expr) do { \
		if (!(expr)) { \
			printf("FAILED, line %d: %s\n", __LINE__, #expr); \
			n_failed++; \
		} \
	} while (0)

static void write_file(const char *name, const void *data, int length)
{
	FILE *f = fopen(name, "wb");

	if (!f || fwrite(data, 1, length, f) != (size_t) length) {
		printf("cannot write %s\n", name);
		exit(1);
	}
	fclose(f);
}

/* Adds one entry to a hog file: 13 bytes of name, 4 bytes of length (low
   byte first), then the data. */
static void hog_add(FILE *f, const char *name, const void *data, int length)
{
	char name_field[13];
	unsigned char len_field[4];

	memset(name_field, 0, sizeof(name_field));
	strncpy(name_field, name, 12);
	len_field[0] = length & 0xff;
	len_field[1] = (length >> 8) & 0xff;
	len_field[2] = (length >> 16) & 0xff;
	len_field[3] = (length >> 24) & 0xff;
	fwrite(name_field, 1, 13, f);
	fwrite(len_field, 1, 4, f);
	fwrite(data, 1, length, f);
}

static FILE *hog_create(const char *name)
{
	FILE *f = fopen(name, "wb");

	if (!f) {
		printf("cannot create %s\n", name);
		exit(1);
	}
	fwrite("DHF", 1, 3, f);
	return f;
}

static const char first_text[] = "first file";
static const char lines_text[] = "line one\r\nline two\r\nlast";
static const char loose_text[] = "loose file data";
static const char alt_text[] = "from the alternate hog";

int main(int argc, char **argv)
{
	unsigned char bin[300], buf[300];
	char line[64];
	CFILE *cf;
	FILE *f;
	int i, ok;

	if (argc < 2) {
		printf("Usage: test_cfile <directory for temporary files>\n");
		return 1;
	}
	if (chdir(argv[1]) != 0) {
		printf("cannot change to the directory %s\n", argv[1]);
		return 1;
	}

	/* Remove the files of an earlier run. */
	remove("descent.hog");
	remove("alt.hog");
	remove("FIRST.TXT");
	remove("Sub/Loose.Dat");

	for (i = 0; i < 300; i++)
		bin[i] = (i * 7 + 3) & 0xff;

	/* The main hog file. Its name on the disk is in lower case. The
	   library opens "DESCENT.HOG". */
	f = hog_create("descent.hog");
	hog_add(f, "FIRST.TXT", first_text, (int) strlen(first_text));
	hog_add(f, "Second.Bin", bin, 300);
	hog_add(f, "EMPTY.DAT", "", 0);
	hog_add(f, "LINES.TXT", lines_text, (int) strlen(lines_text));
	hog_add(f, "TWELVECH.ARS", "12", 2);	/* name with 12 characters, the longest */
	fclose(f);

	/* Not in the hog file and not on the disk. */
	CHECK(cfopen("NOFILE.XXX", "rb") == NULL);
	CHECK(cfexist("NOFILE.XXX") == 0);

	/* Open an entry. The case of the name is different from the hog file. */
	CHECK(cfexist("second.BIN") == 2);
	cf = cfopen("SECOND.BIN", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == 300);
		CHECK(cftell(cf) == 0);

		/* read */
		memset(buf, 0, sizeof(buf));
		CHECK(cfread(buf, 1, 16, cf) == 16);
		CHECK(memcmp(buf, bin, 16) == 0);
		CHECK(cftell(cf) == 16);
		CHECK(cfread(buf, 4, 5, cf) == 5);
		CHECK(memcmp(buf, bin + 16, 20) == 0);
		CHECK(cftell(cf) == 36);

		/* seek from the start */
		CHECK(cfseek(cf, 100, SEEK_SET) == 0);
		CHECK(cftell(cf) == 100);
		CHECK(cfgetc(cf) == bin[100]);
		CHECK(cftell(cf) == 101);

		/* seek from the current position */
		CHECK(cfseek(cf, 49, SEEK_CUR) == 0);
		CHECK(cftell(cf) == 150);
		CHECK(cfgetc(cf) == bin[150]);
		CHECK(cfseek(cf, -51, SEEK_CUR) == 0);
		CHECK(cftell(cf) == 100);
		CHECK(cfgetc(cf) == bin[100]);

		/* seek from the end, and the end of the entry */
		CHECK(cfseek(cf, -1, SEEK_END) == 0);
		CHECK(cftell(cf) == 299);
		CHECK(cfgetc(cf) == bin[299]);
		CHECK(cftell(cf) == 300);
		CHECK(cfgetc(cf) == EOF);	/* the next entry follows, but this one ends here */
		CHECK(cftell(cf) == 300);

		/* the full entry */
		CHECK(cfseek(cf, 0, SEEK_SET) == 0);
		CHECK(cfread(buf, 1, 300, cf) == 300);
		CHECK(memcmp(buf, bin, 300) == 0);

		/* a read past the end fails and does not move the position */
		CHECK(cfseek(cf, 290, SEEK_SET) == 0);
		CHECK(cfread(buf, 1, 11, cf) == (size_t) EOF);
		CHECK(cftell(cf) == 290);

		cfclose(cf);
	}

	/* The first entry, the empty entry and the entry with the longest name. */
	cf = cfopen("first.txt", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == (int) strlen(first_text));
		CHECK(cfread(buf, 1, strlen(first_text), cf) == strlen(first_text));
		CHECK(memcmp(buf, first_text, strlen(first_text)) == 0);
		cfclose(cf);
	}
	cf = cfopen("empty.dat", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == 0);
		CHECK(cfgetc(cf) == EOF);
		cfclose(cf);
	}
	cf = cfopen("twelvech.ars", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == 2);
		CHECK(cfgetc(cf) == '1');
		CHECK(cfgetc(cf) == '2');
		CHECK(cfgetc(cf) == EOF);
		cfclose(cf);
	}

	/* cfgets() removes the CR characters. */
	cf = cfopen("Lines.txt", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfgets(line, sizeof(line), cf) == line);
		CHECK(!strcmp(line, "line one\n"));
		CHECK(cfgets(line, sizeof(line), cf) == line);
		CHECK(!strcmp(line, "line two\n"));
		CHECK(cfgets(line, sizeof(line), cf) == NULL);	/* the last line has no line end */
		cfclose(cf);
	}

	/* A loose file in a directory. The case of the path is different. */
	make_dir("Sub");
	write_file("Sub/Loose.Dat", loose_text, (int) strlen(loose_text));
	CHECK(cfexist("sub/LOOSE.DAT") == 1);
	cf = cfopen("SUB\\loose.dat", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == (int) strlen(loose_text));
		CHECK(cfread(buf, 1, strlen(loose_text), cf) == strlen(loose_text));
		CHECK(memcmp(buf, loose_text, strlen(loose_text)) == 0);
		CHECK(cfseek(cf, 6, SEEK_SET) == 0);
		CHECK(cfgetc(cf) == 'f');
		CHECK(cftell(cf) == 7);
		cfclose(cf);
	}

#ifndef _MSC_VER
	/* The search that finds the real name. Windows does not need it, but
	   the function is the same on all systems. */
	{
		char real[64];

		CHECK(cfile_find_nocase("SUB\\loose.dat", real, sizeof(real)) == 1);
		CHECK(!strcmp(real, "Sub/Loose.Dat"));
		CHECK(cfile_find_nocase("./sub/../DESCENT.HOG", real, sizeof(real)) == 1);
		CHECK(!strcmp(real, "./Sub/../descent.hog"));
		CHECK(cfile_find_nocase("Sub/Loose.Dat", real, sizeof(real)) == 1);
		CHECK(!strcmp(real, "Sub/Loose.Dat"));
		CHECK(cfile_find_nocase("sub/nofile.dat", real, sizeof(real)) == 0);
		CHECK(cfile_find_nocase("nodir/loose.dat", real, sizeof(real)) == 0);
		CHECK(cfile_find_nocase("SUB\\loose.dat", real, 5) == 0);	/* 'real' is too small */
	}
#endif

	/* The alternate directory. */
	CHECK(cfopen("loose.dat", "rb") == NULL);
	cfile_use_alternate_hogdir("sub/");
	CHECK(cfexist("LOOSE.DAT") == 1);
	cf = cfopen("loose.dat", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == (int) strlen(loose_text));
		cfclose(cf);
	}
	cfile_use_alternate_hogdir(NULL);
	CHECK(cfexist("LOOSE.DAT") == 0);

	/* The alternate hog file. Its entries win over the main hog file. */
	f = hog_create("alt.hog");
	hog_add(f, "ALT.TXT", alt_text, (int) strlen(alt_text));
	hog_add(f, "first.txt", alt_text, 4);
	fclose(f);
	CHECK(cfexist("alt.txt") == 0);
	cfile_use_alternate_hogfile("ALT.HOG");
	CHECK(cfexist("alt.txt") == 2);
	cf = cfopen("alt.txt", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == (int) strlen(alt_text));
		CHECK(cfread(buf, 1, strlen(alt_text), cf) == strlen(alt_text));
		CHECK(memcmp(buf, alt_text, strlen(alt_text)) == 0);
		cfclose(cf);
	}
	cf = cfopen("FIRST.TXT", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == 4);
		cfclose(cf);
	}
	cfile_use_alternate_hogfile(NULL);
	CHECK(cfexist("alt.txt") == 0);

	/* A loose file wins over an entry of the hog file. */
	write_file("FIRST.TXT", loose_text, 5);
	CHECK(cfexist("first.txt") == 1);
	cf = cfopen("first.txt", "rb");
	CHECK(cf != NULL);
	if (cf) {
		CHECK(cfilelength(cf) == 5);
		cfclose(cf);
	}
	remove("FIRST.TXT");
	CHECK(cfexist("first.txt") == 2);

	ok = !n_failed;
	if (!ok) {
		printf("test_cfile: %d checks failed\n", n_failed);
		return 1;
	}
	printf("test_cfile: ok\n");
	return 0;
}
