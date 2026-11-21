/*
 * ASCENT: decoder for Interplay ACM audio streams (the sounds of an EGG
 * archive). New file. See docs/formats/tun.md and docs/formats/egg.md.
 */
#ifndef _ACM_H
#define _ACM_H

#include "types.h"

typedef struct acm_info {
	int total;			// number of 16-bit values, over all channels
	int channels;		// 1 or 2
	int rate;			// samples per second
	int level;			// log2 of the number of columns of a block
	int rows;			// rows of a block
} acm_info;

// Read the 14-byte header. Returns 1 if data is an ACM stream.
int acm_get_info(const ubyte *data, int size, acm_info *info);

// Decode a whole stream. Returns info->total signed 16-bit values
// (interleaved for two channels) in memory that the caller releases with
// free(), or NULL if the stream is not valid.
short *acm_decode(const ubyte *data, int size, acm_info *info);

// A stream that is decoded in parts, for the music files (NAME.TUN). The
// decoder holds one block (2048 or 16384 values in the shipped files) and
// 4096 bytes of the file. The functions do not allocate or release memory
// after acm_stream_open, so acm_stream_read can run on the audio thread.
typedef struct acm_stream acm_stream;

// The source of the bytes: write up to "size" bytes to "buf" and return
// the count, 0 at the end of the stream.
typedef int (*acm_read_fn)(void *user, ubyte *buf, int size);

// Reads the 14-byte header from the source. Returns NULL if the source is
// not an ACM stream. If info is not NULL it gets the values of the header.
acm_stream *acm_stream_open(acm_read_fn read, void *user, acm_info *info);

// Decode the next n_values 16-bit values (interleaved for two channels).
// Returns the count, which is less than n_values at the end of the stream
// or after an error.
int acm_stream_read(acm_stream *st, short *out, int n_values);

// Start again at the first value. The caller first puts the source back
// to the byte after the 14-byte header.
void acm_stream_rewind(acm_stream *st);

// Returns 1 if the stream has an invalid code or ends before its header says.
int acm_stream_error(acm_stream *st);

void acm_stream_close(acm_stream *st);

#endif
