/*
 * ASCENT test for the misc library (error.c and parsarg.c).
 * New file. It replaces no original file.
 *
 * Usage: test_misc <directory for temporary files>
 *
 * It tests: the warning function, Assert(), Int3(), parse_args() with an
 * argument file and with wildcards, and Error() with the error function.
 * Error() ends the program, so that test is the last one. The error
 * function ends the program with the code 0 when the message is correct.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "parsarg.h"

static int n_failed = 0;

#define CHECK(expr) do { \
		if (!(expr)) { \
			printf("FAILED, line %d: %s\n", __LINE__, #expr); \
			n_failed++; \
		} \
	} while (0)

static char last_warning[256];

static void keep_warning(char *s)
{
	strncpy(last_warning, s, sizeof(last_warning) - 1);
}

#define MAX_SEEN 16
static char seen[MAX_SEEN][300];
static int n_seen;

static void keep_arg(char *arg)
{
	if (n_seen < MAX_SEEN)
		strncpy(seen[n_seen], arg, sizeof(seen[0]) - 1);
	n_seen++;
}

static int was_seen(const char *arg)
{
	int i;

	for (i = 0; i < n_seen && i < MAX_SEEN; i++)
		if (!strcmp(seen[i], arg))
			return 1;
	return 0;
}

static void write_file(const char *name, const char *text)
{
	FILE *f = fopen(name, "wb");

	if (!f) {
		printf("cannot create %s\n", name);
		exit(1);
	}
	fputs(text, f);
	fclose(f);
}

static void error_hook(char *msg)
{
	if (strcmp(msg, "Error: test message 42") != 0) {
		printf("FAILED: Error() gave the message '%s'\n", msg);
		exit(1);
	}
	if (n_failed) {
		printf("test_misc: %d checks failed\n", n_failed);
		exit(1);
	}
	printf("test_misc: ok\n");
	exit(0);
}

int main(int argc, char **argv)
{
	char path[300], spec[300], argfile[300];
	char *args[4];
	int side_effect = 0;

	if (argc < 2) {
		printf("Usage: test_misc <directory for temporary files>\n");
		return 1;
	}

	/* Warning() */
	set_warn_func(keep_warning);
	Warning("number %d", 7);
	CHECK(!strcmp(last_warning, "Warning: number 7"));
	clear_warn_func(keep_warning);

	/* Assert() with a true expression returns. It evaluates the
	   expression with and without NDEBUG. */
	Assert(++side_effect == 1);
	CHECK(side_effect == 1);

	/* Int3() returns when Int3_break_enabled is 0. */
	CHECK(Int3_break_enabled == 0);
	printf("An Int3 message can follow. It is part of the test.\n");
	fflush(stdout);
	Int3();

	/* parse_args(): plain arguments */
	n_seen = 0;
	args[0] = "-a";
	args[1] = "name";
	parse_args(2, args, keep_arg, 0);
	CHECK(n_seen == 2);
	CHECK(was_seen("-a") && was_seen("name"));

	/* parse_args(): argument file with a comment */
	snprintf(argfile, sizeof(argfile), "@%s/args.txt", argv[1]);
	write_file(argfile + 1, "-x one ; a comment\r\ntwo\t-y\r\n");
	n_seen = 0;
	args[0] = argfile;
	parse_args(1, args, keep_arg, 0);
	CHECK(n_seen == 4);
	CHECK(was_seen("-x") && was_seen("one") && was_seen("two") && was_seen("-y"));

	/* parse_args(): wildcards */
	snprintf(path, sizeof(path), "%s/wild1.tst", argv[1]);
	write_file(path, "1");
	snprintf(path, sizeof(path), "%s/WILD2.TST", argv[1]);
	write_file(path, "2");
	snprintf(spec, sizeof(spec), "%s/wild?.tst", argv[1]);
	n_seen = 0;
	args[0] = spec;
	args[1] = "-wild*";
	parse_args(2, args, keep_arg, PA_EXPAND);
#ifndef _MSC_VER
	CHECK(n_seen == 3);
	snprintf(path, sizeof(path), "%s/wild1.tst", argv[1]);
	CHECK(was_seen(path));
	snprintf(path, sizeof(path), "%s/WILD2.TST", argv[1]);
	CHECK(was_seen(path));
#endif
	CHECK(was_seen("-wild*"));

	/* A spec that matches nothing goes to the handler as it is. */
	snprintf(spec, sizeof(spec), "%s/nothing*.xyz", argv[1]);
	n_seen = 0;
	args[0] = spec;
	parse_args(1, args, keep_arg, PA_EXPAND);
	CHECK(n_seen == 1 && was_seen(spec));

	/* Error() calls the error function and never returns. */
	set_error_func(error_hook);
	Error("test message %d", 42);

	printf("FAILED: Error() returned\n");
	return 1;
}
