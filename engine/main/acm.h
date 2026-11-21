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

#endif
