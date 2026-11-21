/*
 * ASCENT installer tools: the shared code of ascent-import and
 * ascent-content (files, SHA-256, ZIP archives, ISO 9660 images).
 *
 * This file is new. The DOS game had an installer (INSTALL.EXE) that
 * copied a part of the disc. The port has its own tools.
 *
 * The file names are UTF-8. The executables have a manifest with the UTF-8
 * code page, so that fopen() and the A functions of Windows take UTF-8.
 */
#ifndef INST_H
#define INST_H

#include <stdio.h>
#include <stdint.h>

#define INST_PATH 1024

// ---------------------------------------------------------------------
// Files

int inst_is_dir(const char *path);
int inst_is_file(const char *path);
int64_t inst_file_size(const char *path);		// -1 if no file
int inst_mkdirs(const char *path);				// makes the parents too; 1 if it exists after
int inst_remove_tree(const char *path);			// 1 if successful
// Path join with '/' (Windows takes both separators).
void inst_join(char *out, int size, const char *a, const char *b);
// Find a name in a directory, case ignored. Writes the real name to out.
int inst_find_name(const char *dir, const char *name, char *out, int size);
// Hard link, else a copy. 1 if successful.
int inst_link_or_copy(const char *target, const char *link);
int inst_copy_file(const char *from, const char *to);

// The entries of a directory (files only, no subdirectories).
typedef struct {
	int n, max;
	char **name;
	int64_t *size;
} inst_list;
int inst_list_dir(const char *dir, inst_list *l);
void inst_list_free(inst_list *l);

int inst_stricmp(const char *a, const char *b);
const char *inst_ext(const char *name);			// the text after the last dot, or ""

// ---------------------------------------------------------------------
// SHA-256

typedef struct {
	uint32_t h[8];
	uint64_t len;
	uint8_t buf[64];
	int n;
} inst_sha256;
void inst_sha256_init(inst_sha256 *c);
void inst_sha256_add(inst_sha256 *c, const void *data, size_t size);
void inst_sha256_hex(inst_sha256 *c, char hex[65]);
int inst_sha256_file(const char *path, char hex[65]);	// 1 if the file was read

// ---------------------------------------------------------------------
// ZIP archives (also a ZIP after a self-extracting program). The methods
// are 0 (stored) and 8 (deflate). No ZIP64.

typedef struct {
	char name[260];
	uint32_t method, crc, csize, usize, local_offset;
} inst_zip_entry;

typedef struct {
	FILE *f;
	int n;
	inst_zip_entry *e;
	int64_t delta;		// the offset of the archive in the file
} inst_zip;

int inst_zip_open(inst_zip *z, const char *path);	// 1 if successful
void inst_zip_close(inst_zip *z);
int inst_zip_find(inst_zip *z, const char *name);	// case ignored; -1 if not found
// Extract an entry to a file. hex gets the SHA-256 of the data. Returns 1
// if successful (the CRC matches). progress is called with the bytes done.
int inst_zip_extract(inst_zip *z, int i, const char *to, char hex[65],
	void (*progress)(int64_t done, void *arg), void *arg);
// Read an entry into memory (for the small text entries). free() it.
char *inst_zip_read(inst_zip *z, int i, uint32_t *size);

// ---------------------------------------------------------------------
// ISO 9660 images (.iso, .bin with 2048 or 2352-byte sectors, .cue)

typedef struct {
	char path[260];		// "DTU/YPINN.DOG"
	uint32_t lba, size;
} inst_iso_file;

typedef struct {
	FILE *f;
	int sector_size, data_offset;
	int n, max;
	inst_iso_file *files;
} inst_iso;

int inst_iso_open(inst_iso *iso, const char *path, char *err, int err_size);
void inst_iso_close(inst_iso *iso);
int inst_iso_extract(inst_iso *iso, const inst_iso_file *file, const char *to,
	void (*progress)(int64_t done, void *arg), void *arg);

#endif
