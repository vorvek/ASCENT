/*
 * ASCENT: access to the Descent to Undermountain data files.
 *
 * This file is new. It replaces the HOG file and the "alternate hog
 * directory" of the Descent 1 cfile library for the game data: the data
 * of Descent to Undermountain is a set of plain files in the game
 * directory and a set of packed files on the disc.
 *
 * The packed format (NAME.DOG, NAME.COW, NAME.HEN) is described in
 * docs/formats/packed.md. tools/unpack.py is the reference decoder.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

#ifndef _WIN32
#include <dirent.h>
#endif

#include "types.h"
#include "mem.h"		// malloc and free of the game (the callers include it too)
#include "dtufile.h"
#include "args.h"		// ASCENT: -datadir, -userdir, -debug
#include "error.h"		// set_error_func
#include "platform.h"	// the directories of an installed game
#include <sys/stat.h>

#define DTU_MAX_DIRS		8
#define DTU_MAX_PATH		512

static char Dtu_dirs[DTU_MAX_DIRS][DTU_MAX_PATH];
static int Dtu_num_dirs = 0;

void dtu_add_data_dir(const char *path)
{
	int len;

	if ( path == NULL || path[0] == 0 )
		return;
	if ( Dtu_num_dirs >= DTU_MAX_DIRS )
		return;

	len = (int)strlen(path);
	if ( len > DTU_MAX_PATH-2 )
		return;
	memcpy( Dtu_dirs[Dtu_num_dirs], path, len+1 );
	// Remove the separator at the end. dtu_find_in_dir() adds one.
	while ( len > 1 && (Dtu_dirs[Dtu_num_dirs][len-1] == '/' || Dtu_dirs[Dtu_num_dirs][len-1] == '\\') )
		Dtu_dirs[Dtu_num_dirs][--len] = 0;
	Dtu_num_dirs++;
}

// ----------------------------------------------------------------------
// ASCENT: the directories of a development run and of an installed game.
// See dtufile.h.

#ifdef ASCENT_RELEASE
int Dtu_debug = 0;
#else
int Dtu_debug = 1;
#endif

static char Dtu_user_dir[DTU_MAX_PATH];		// empty: the current directory
static char Dtu_install_dir[DTU_MAX_PATH];	// empty: not an installed game

static int dtu_dir_exists(const char *path)
{
	struct stat st;

	return stat(path, &st) == 0 && (st.st_mode & S_IFMT) == S_IFDIR;
}

static void dtu_error_box(char *msg)
{
	plat_message_box("ASCENT", msg);
}

void dtu_init_dirs(void)
{
	char path[DTU_MAX_PATH];
	int i, n_datadir = 0;

	if (FindArg("-debug"))
		Dtu_debug = 1;
	if (!Dtu_debug)
		mem_report_leaks = 0;		// MEM_LEAKAGE (mem.c)

	for (i=1; i<Num_args-1; i++)
		if (!stricmp(Args[i], "-datadir")) {
			dtu_add_data_dir(Args[i+1]);
			n_datadir++;
		}

	// A development run gives the data directories. It keeps its files
	// in the current directory.
	if (n_datadir == 0) {
		// An installed game: the data is in "data" next to the executable,
		// and the files of the player go to the directory of the user.
		if (plat_base_dir(Dtu_install_dir, sizeof(Dtu_install_dir)) && (int) strlen(Dtu_install_dir) < DTU_MAX_PATH-8) {
			snprintf(path, sizeof(path), "%s/data", Dtu_install_dir);
			dtu_add_data_dir(path);
		} else
			Dtu_install_dir[0] = 0;
		if (!plat_pref_dir("ASCENT", Dtu_user_dir, sizeof(Dtu_user_dir)))
			Dtu_user_dir[0] = 0;
		set_error_func(dtu_error_box);		// the release build has no console
		if (!dtu_find_file("DTU.MSN", path, sizeof(path))) {
			char msg[DTU_MAX_PATH+200];
			snprintf(msg, sizeof(msg), "The game data is not in %s/data.\n\n"
				"Run the setup program again, or start the game with -datadir.", Dtu_install_dir);
			plat_message_box("ASCENT", msg);
			exit(1);
		}
	}

	// -userdir DIR: the files of the player go to DIR (for tests).
	if ((i = FindArg("-userdir")) != 0 && i+1 < Num_args && dtu_dir_exists(Args[i+1]))
		snprintf(Dtu_user_dir, sizeof(Dtu_user_dir), "%s", Args[i+1]);
}

const char *dtu_install_dir(void)
{
	return Dtu_install_dir[0] ? Dtu_install_dir : NULL;
}

const char *dtu_user_dir(void)
{
	return Dtu_user_dir[0] ? Dtu_user_dir : NULL;
}

char *dtu_user_path(char *out, int size, const char *name)
{
	if (Dtu_user_dir[0])
		snprintf(out, size, "%s/%s", Dtu_user_dir, name);
	else
		snprintf(out, size, "%s", name);
	return out;
}

static int dtu_names_equal(const char *a, const char *b)
{
	while ( *a && *b ) {
		if ( tolower((unsigned char)*a) != tolower((unsigned char)*b) )
			return 0;
		a++; b++;
	}
	return *a == *b;
}

static int dtu_file_exists(const char *path)
{
	FILE *fp = fopen( path, "rb" );
	if ( fp == NULL )
		return 0;
	fclose( fp );
	return 1;
}

// Look for a file in one directory. The name match ignores case.
static int dtu_find_in_dir(const char *dir, const char *name, char *path_out, int path_size)
{
	if ( (int)(strlen(dir) + strlen(name) + 2) > path_size )
		return 0;

	sprintf( path_out, "%s/%s", dir, name );
	if ( dtu_file_exists(path_out) )
		return 1;

#ifndef _WIN32
	// The file systems of Windows ignore case. On the other systems,
	// compare the name with each entry of the directory.
	{
		DIR *d = opendir( dir );
		struct dirent *e;

		if ( d != NULL ) {
			while ( (e = readdir(d)) != NULL ) {
				if ( dtu_names_equal(e->d_name, name) ) {
					sprintf( path_out, "%s/%s", dir, e->d_name );
					closedir( d );
					return dtu_file_exists(path_out);
				}
			}
			closedir( d );
		}
	}
#endif

	path_out[0] = 0;
	return 0;
}

int dtu_find_file(const char *name, char *path_out, int path_size)
{
	int i;

	for (i=0; i<Dtu_num_dirs; i++ )
		if ( dtu_find_in_dir(Dtu_dirs[i], name, path_out, path_size) )
			return 1;

	if ( path_size > 0 )
		path_out[0] = 0;
	return 0;
}

static ubyte *dtu_read_whole_file(const char *path, int *size_out)
{
	FILE *fp;
	long len;
	ubyte *buf;

	fp = fopen( path, "rb" );
	if ( fp == NULL )
		return NULL;
	fseek( fp, 0, SEEK_END );
	len = ftell( fp );
	fseek( fp, 0, SEEK_SET );
	if ( len < 0 ) {
		fclose( fp );
		return NULL;
	}
	buf = (ubyte *)malloc( (size_t)len + 1 );
	if ( buf == NULL ) {
		fclose( fp );
		return NULL;
	}
	if ( len > 0 && fread( buf, 1, (size_t)len, fp ) != (size_t)len ) {
		fclose( fp );
		free( buf );
		return NULL;
	}
	fclose( fp );
	buf[len] = 0;		// so that a text file is a C string
	*size_out = (int)len;
	return buf;
}

// The LZSS variant of the DOG, COW and HEN files. See docs/formats/packed.md.
// The first two bytes are the low 16 bits of the unpacked size. The stream
// has no end marker and the full size is not stored.
ubyte *dtu_unpack(const ubyte *src, int src_size, int *size_out)
{
	ubyte *out;
	int out_size, out_alloc;
	int pos, bit;
	unsigned flags;

	if ( src == NULL || src_size < 2 )
		return NULL;

	// A match gives at most 18 bytes for 2 bytes. 9 times the input is the
	// largest possible output.
	out_alloc = src_size * 4 + 1024;
	out = (ubyte *)malloc( out_alloc + 1 );
	if ( out == NULL )
		return NULL;
	out_size = 0;

	pos = 2;
	while ( pos < src_size ) {
		flags = src[pos++];
		for (bit=0; bit<8; bit++ ) {
			if ( pos >= src_size )
				break;
			if ( out_size + 18 > out_alloc ) {
				// mem.h has no realloc
				ubyte *bigger;
				out_alloc *= 2;
				bigger = (ubyte *)malloc( out_alloc + 1 );
				if ( bigger == NULL ) {
					free( out );
					return NULL;
				}
				memcpy( bigger, out, out_size );
				free( out );
				out = bigger;
			}
			if ( (flags >> bit) & 1 ) {
				out[out_size++] = src[pos++];
			} else {
				int word, dist, length, i;

				if ( pos+1 >= src_size ) {		// match cut off at the end of the file
					free( out );
					return NULL;
				}
				word = src[pos] | (src[pos+1] << 8);
				pos += 2;
				dist = word >> 4;
				length = (word & 15) + 3;
				if ( dist == 0 || dist > out_size ) {
					free( out );
					return NULL;
				}
				// A match can overlap the bytes that it writes.
				for (i=0; i<length; i++ ) {
					out[out_size] = out[out_size - dist];
					out_size++;
				}
			}
		}
	}

	if ( (out_size & 0xFFFF) != (src[0] | (src[1] << 8)) ) {
		free( out );
		return NULL;
	}

	out[out_size] = 0;
	*size_out = out_size;
	return out;
}

// Replace the extension of name (3 characters after the last dot).
static void dtu_set_ext(char *dest, const char *name, const char *ext)
{
	char *dot;

	strcpy( dest, name );
	dot = strrchr( dest, '.' );
	if ( dot == NULL )
		dot = dest + strlen(dest);
	*dot++ = '.';
	strcpy( dot, ext );
}

ubyte *dtu_load_file(const char *name, int *size_out)
{
	char path[DTU_MAX_PATH+16];
	char other[64];
	const char *dot;
	const char *packed_ext = NULL;
	int is_lvl = 0;
	int i, size;
	ubyte *data;

	if ( size_out )
		*size_out = 0;
	if ( name == NULL || strlen(name) > 48 )
		return NULL;

	dot = strrchr( name, '.' );
	if ( dot != NULL ) {
		if ( dtu_names_equal(dot, ".lvl") ) {
			packed_ext = "DOG";
			is_lvl = 1;
		} else if ( dtu_names_equal(dot, ".egg") )
			packed_ext = "COW";
		else if ( dtu_names_equal(dot, ".bin") )
			packed_ext = "HEN";
	}

	for (i=0; i<Dtu_num_dirs; i++ ) {
		data = NULL;
		size = 0;

		if ( is_lvl ) {
			dtu_set_ext( other, name, "LV2" );
			if ( dtu_find_in_dir(Dtu_dirs[i], other, path, sizeof(path)) )
				data = dtu_read_whole_file( path, &size );
		}

		if ( data == NULL && dtu_find_in_dir(Dtu_dirs[i], name, path, sizeof(path)) )
			data = dtu_read_whole_file( path, &size );

		if ( data == NULL && packed_ext != NULL ) {
			dtu_set_ext( other, name, packed_ext );
			if ( dtu_find_in_dir(Dtu_dirs[i], other, path, sizeof(path)) ) {
				int packed_size;
				ubyte *packed = dtu_read_whole_file( path, &packed_size );
				if ( packed != NULL ) {
					data = dtu_unpack( packed, packed_size, &size );
					free( packed );
				}
			}
		}

		if ( data != NULL ) {
			if ( size_out )
				*size_out = size;
			return data;
		}
	}

	return NULL;
}

// ----------------------------------------------------------------------
// Messages of the data loaders. See dtufile.h.

char Dtu_log[DTU_LOG_SIZE];
int Dtu_log_count = 0;
int Dtu_log_to_stderr = 0;
static int Dtu_log_len = 0;

void dtu_log(const char *format, ...)
{
	char buf[400];
	va_list args;
	int len;

	va_start( args, format );
	vsnprintf( buf, sizeof(buf)-2, format, args );
	va_end( args );

	len = (int)strlen(buf);
	if ( len == 0 || buf[len-1] != 10 ) {		// 10 is the line end
		buf[len++] = 10;
		buf[len] = 0;
	}

	Dtu_log_count++;
	if ( Dtu_log_len + len < DTU_LOG_SIZE ) {
		memcpy( Dtu_log + Dtu_log_len, buf, len+1 );
		Dtu_log_len += len;
	}
	if ( Dtu_log_to_stderr )
		fputs( buf, stderr );
}

void dtu_log_clear(void)
{
	Dtu_log[0] = 0;
	Dtu_log_len = 0;
	Dtu_log_count = 0;
}
