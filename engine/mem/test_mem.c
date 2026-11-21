/*
 * ASCENT test for the debug memory allocator (mem.c).
 * New file. It replaces no original file.
 *
 * It tests: allocate, zero fill, free, the leak check, the overwrite
 * check, the free of an unknown pointer, and many blocks at one time.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

/* The functions of mem.c. The test does not include mem.h, so that malloc()
   and free() in this file are the functions of the C library. */
extern void *mem_malloc(unsigned int size, char *var, char *file, int line, int fill_zero);
extern void mem_free(void *buffer);
extern void mem_display_blocks(void);
extern int mem_validate_heap(void);
extern int mem_num_blocks(void);
extern unsigned int mem_bytes_malloced(void);

static int n_failed = 0;
static int n_warnings = 0;

#define CHECK(expr) do { \
		if (!(expr)) { \
			printf("FAILED, line %d: %s\n", __LINE__, #expr); \
			n_failed++; \
		} \
	} while (0)

#define MEM_ALLOC(size, zero) mem_malloc((size), "test", __FILE__, __LINE__, (zero))

static void count_warning(char *s)
{
	(void) s;
	n_warnings++;
}

#define N_MANY 5000

int main(void)
{
	static unsigned char *many[N_MANY];
	unsigned char *p, *q;
	void *other;
	int i, ok;

	set_warn_func(count_warning);

	/* Allocate and free. */
	CHECK(mem_num_blocks() == 0);
	p = MEM_ALLOC(100, 0);
	CHECK(p != NULL);
	CHECK(mem_num_blocks() == 1);
	CHECK(mem_bytes_malloced() == 100);
	memset(p, 0xAA, 100);	/* the full block can be written */
	CHECK(mem_validate_heap() == 0);

	/* Zero fill. */
	q = MEM_ALLOC(64, 1);
	CHECK(q != NULL);
	ok = 1;
	for (i = 0; i < 64; i++)
		if (q[i] != 0)
			ok = 0;
	CHECK(ok);
	CHECK(mem_num_blocks() == 2);
	CHECK(mem_bytes_malloced() == 164);

	mem_free(q);
	CHECK(mem_num_blocks() == 1);
	CHECK(mem_bytes_malloced() == 100);

	/* Overwrite check: one byte after the end of the block. */
	p[100] = 0;
	CHECK(mem_validate_heap() == 1);
	p[100] = 0xFC;
	CHECK(mem_validate_heap() == 0);

	/* Leak check: the block p is still allocated. */
	printf("The MEM_LEAKAGE message that follows is part of the test.\n");
	fflush(stdout);
	mem_display_blocks();
	CHECK(mem_num_blocks() == 1);
	mem_free(p);
	CHECK(mem_num_blocks() == 0);
	CHECK(mem_bytes_malloced() == 0);

	/* A pointer that mem_malloc() did not return, and the null pointer.
	   mem_free() gives a warning and does not free the pointer. */
	other = malloc(32);
	CHECK(n_warnings == 0);
	mem_free(other);
	CHECK(n_warnings == 1);
	mem_free(NULL);
	CHECK(n_warnings == 2);
	free(other);

	/* Many blocks, so that the block table grows. Free them in an order
	   that is not the order of allocation. */
	for (i = 0; i < N_MANY; i++) {
		many[i] = MEM_ALLOC(1 + i % 50, 0);
		memset(many[i], i & 0xff, 1 + i % 50);
	}
	CHECK(mem_num_blocks() == N_MANY);
	CHECK(mem_validate_heap() == 0);
	for (i = 0; i < N_MANY; i += 2)
		mem_free(many[i]);
	CHECK(mem_num_blocks() == N_MANY / 2);
	ok = 1;
	for (i = 1; i < N_MANY; i += 2)
		if (many[i][0] != (i & 0xff))
			ok = 0;
	CHECK(ok);
	CHECK(mem_validate_heap() == 0);
	for (i = N_MANY - 1; i > 0; i -= 2)
		mem_free(many[i]);
	CHECK(mem_num_blocks() == 0);
	CHECK(mem_bytes_malloced() == 0);
	CHECK(n_warnings == 2);

	if (n_failed) {
		printf("test_mem: %d checks failed\n", n_failed);
		return 1;
	}
	printf("test_mem: ok\n");
	return 0;
}
