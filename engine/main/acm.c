/*
 * ASCENT: decoder for Interplay ACM audio streams.
 *
 * This file is new. The sounds of an EGG archive (source name NAME.RCA,
 * "raw compressed audio") are ACM streams. Descent 1 stored raw 8-bit
 * samples in the PIG file and needed no decoder.
 *
 * The format is described in docs/formats/tun.md. tools/tun.py is the
 * reference decoder. This code is written from that description.
 */

#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "mem.h"		// malloc and free of the game (the callers include it too)
#include "acm.h"

#define ACM_SIGNATURE		0x01032897
#define ACM_HEADER_SIZE		14
#define AMP_MID				0x8000

typedef struct acm_state {
	const ubyte	*data;
	int			pos, end;
	unsigned		acc;			// bit buffer, the next bit is bit 0
	int			nbits;
	int			level, rows, cols;
	int			*block;		// rows * cols values
	int			*amp;			// 0x10000 entries, AMP_MID is the value 0
	int			*hist[16];	// two lines of history for each filter pass
	int			error;
} acm_state;

static int acm_bits(acm_state *s, int n)
{
	int v;

	while ( s->nbits < n ) {
		// Bits past the end of the stream read as 0.
		if ( s->pos < s->end )
			s->acc |= (unsigned)s->data[s->pos++] << s->nbits;
		s->nbits += 8;
	}
	v = s->acc & ((1u << n) - 1);
	s->acc >>= n;
	s->nbits -= n;
	return v;
}

static void fill_zero(acm_state *s, int col)
{
	int i;
	for (i=0; i<s->rows; i++ )
		s->block[i*s->cols + col] = 0;
}

static void fill_linear(acm_state *s, int ind, int col)
{
	int i;
	int base = AMP_MID - (1 << (ind-1));

	for (i=0; i<s->rows; i++ )
		s->block[i*s->cols + col] = s->amp[base + acm_bits(s, ind)];
}

static const int map1[2] = { -1, 1 };
static const int map2n[4] = { -2, -1, 1, 2 };
static const int map2f[4] = { -3, -2, 2, 3 };
static const int map3[8] = { -4, -3, -2, -1, 1, 2, 3, 4 };

// The k13, k12, k24, k23, k45 and k44 codes. nbits is 1, 2 or 3.
static void fill_k(acm_state *s, int col, int pair_zero, const int *table, int nbits)
{
	int i = 0;

	while ( i < s->rows ) {
		if ( pair_zero && acm_bits(s, 1) == 0 ) {
			s->block[i*s->cols + col] = 0;
			i++;
			if ( i >= s->rows )
				break;
			s->block[i*s->cols + col] = 0;
			i++;
			continue;
		}
		if ( acm_bits(s, 1) == 0 ) {
			s->block[i*s->cols + col] = 0;
			i++;
			continue;
		}
		s->block[i*s->cols + col] = s->amp[AMP_MID + table[acm_bits(s, nbits)]];
		i++;
	}
}

// The k35 and k34 codes.
static void fill_k3(acm_state *s, int col, int pair_zero)
{
	int i = 0;

	while ( i < s->rows ) {
		if ( pair_zero && acm_bits(s, 1) == 0 ) {
			s->block[i*s->cols + col] = 0;
			i++;
			if ( i >= s->rows )
				break;
			s->block[i*s->cols + col] = 0;
			i++;
			continue;
		}
		if ( acm_bits(s, 1) == 0 ) {
			s->block[i*s->cols + col] = 0;
			i++;
			continue;
		}
		if ( acm_bits(s, 1) == 0 )
			s->block[i*s->cols + col] = s->amp[AMP_MID + map1[acm_bits(s, 1)]];
		else
			s->block[i*s->cols + col] = s->amp[AMP_MID + map2f[acm_bits(s, 2)]];
		i++;
	}
}

// The t15, t27 and t37 codes: one field holds "per" rows as digits.
static void fill_t(acm_state *s, int col, int nbits, int base, int per, int mid)
{
	int i = 0, j, b;

	while ( i < s->rows ) {
		b = acm_bits(s, nbits);
		for (j=0; j<per; j++ ) {
			s->block[i*s->cols + col] = s->amp[AMP_MID + (b % base) - mid];
			b /= base;
			i++;
			if ( i >= s->rows )
				break;
		}
	}
}

static void acm_load_block(acm_state *s)
{
	int pwr, val, count, k, col, ind;

	pwr = acm_bits(s, 4);
	val = acm_bits(s, 16);
	count = 1 << pwr;
	for (k=0; k<count; k++ ) {
		s->amp[AMP_MID + k] = k * val;
		s->amp[AMP_MID - 1 - k] = -(k+1) * val;
	}

	for (col=0; col<s->cols; col++ ) {
		ind = acm_bits(s, 5);
		switch (ind) {
		case 0:	fill_zero(s, col); break;
		case 17:	fill_k(s, col, 1, map1, 1); break;
		case 18:	fill_k(s, col, 0, map1, 1); break;
		case 19:	fill_t(s, col, 5, 3, 3, 1); break;
		case 20:	fill_k(s, col, 1, map2n, 2); break;
		case 21:	fill_k(s, col, 0, map2n, 2); break;
		case 22:	fill_t(s, col, 7, 5, 3, 2); break;
		case 23:	fill_k3(s, col, 1); break;
		case 24:	fill_k3(s, col, 0); break;
		case 26:	fill_k(s, col, 1, map3, 3); break;
		case 27:	fill_k(s, col, 0, map3, 3); break;
		case 29:	fill_t(s, col, 7, 11, 2, 5); break;
		default:
			if ( ind >= 3 && ind <= 16 )
				fill_linear(s, ind, col);
			else {
				s->error = 1;		// 1, 2, 25, 28, 30, 31 are not valid
				return;
			}
		}
	}
}

// One filter pass over a group of lines. The group has sub_count lines of
// sub_len values. hist keeps the last two input lines of the pass before.
static void acm_pass(int *hist, int *group, int sub_len, int sub_count)
{
	int i, j;

	for (i=0; i<sub_len; i++ ) {
		int r0 = hist[i];
		int r1 = hist[sub_len + i];
		int *p = group + i;

		for (j=0; j<sub_count/2; j++ ) {
			int r2 = p[0];
			int r3;
			p[0] = 2*r1 + (r0 + r2);
			r3 = p[sub_len];
			p[sub_len] = 2*r2 - (r1 + r3);
			r0 = r2;
			r1 = r3;
			p += 2*sub_len;
		}
		hist[i] = r0;
		hist[sub_len + i] = r1;
	}
}

static void acm_juggle_block(acm_state *s)
{
	int step, todo, start, n, k, sub_len, sub_count, pos;
	int *group;

	if ( s->level == 0 )
		return;

	if ( s->level > 9 )
		step = 1;
	else
		step = (2048 >> s->level) - 2;

	todo = s->rows;
	start = 0;
	while ( todo > 0 ) {
		n = (todo < step) ? todo : step;
		group = s->block + start * s->cols;

		sub_len = s->cols / 2;
		sub_count = 2 * n;
		acm_pass( s->hist[0], group, sub_len, sub_count );
		for (pos=0; pos<n*s->cols; pos+=sub_len )
			group[pos] += 1;
		k = 1;
		while ( sub_len > 1 ) {
			sub_len /= 2;
			sub_count *= 2;
			acm_pass( s->hist[k], group, sub_len, sub_count );
			k++;
		}

		todo -= n;
		start += n;
	}
}

int acm_get_info(const ubyte *data, int size, acm_info *info)
{
	unsigned sig;
	int packed;

	if ( data == NULL || size < ACM_HEADER_SIZE )
		return 0;
	sig = data[0] | (data[1]<<8) | (data[2]<<16) | ((unsigned)data[3]<<24);
	if ( sig != ACM_SIGNATURE )
		return 0;
	info->total = (int)(data[4] | (data[5]<<8) | (data[6]<<16) | ((unsigned)data[7]<<24));
	info->channels = data[8] | (data[9]<<8);
	info->rate = data[10] | (data[11]<<8);
	packed = data[12] | (data[13]<<8);
	info->level = packed & 15;
	info->rows = packed >> 4;
	if ( info->total < 0 || info->rows == 0 || info->channels < 1 || info->channels > 2 )
		return 0;
	return 1;
}

short *acm_decode(const ubyte *data, int size, acm_info *info)
{
	acm_state s;
	short *out;
	int block_len, n_out, i, k, v;

	if ( !acm_get_info(data, size, info) )
		return NULL;

	memset( &s, 0, sizeof(s) );
	s.data = data;
	s.pos = ACM_HEADER_SIZE;
	s.end = size;
	s.level = info->level;
	s.rows = info->rows;
	s.cols = 1 << info->level;
	block_len = s.rows * s.cols;

	out = (short *)malloc( (info->total + 1) * sizeof(short) );
	s.block = (int *)calloc( block_len, sizeof(int) );
	s.amp = (int *)calloc( 0x10000, sizeof(int) );
	for (k=0; k<s.level; k++ )
		s.hist[k] = (int *)calloc( (2 * (s.cols >> (k+1)) + 2), sizeof(int) );

	n_out = 0;
	if ( out != NULL && s.block != NULL && s.amp != NULL ) {
		while ( n_out < info->total && !s.error ) {
			// A stream that ends too early would give zero blocks forever.
			if ( s.pos >= s.end && s.nbits <= 0 && n_out == 0 ) {
				s.error = 1;
				break;
			}
			acm_load_block( &s );
			if ( s.error )
				break;
			acm_juggle_block( &s );
			for (i=0; i<block_len && n_out<info->total; i++ ) {
				v = s.block[i] >> s.level;
				if ( v < -32768 ) v = -32768;
				if ( v > 32767 ) v = 32767;
				out[n_out++] = (short)v;
			}
			if ( s.pos >= s.end + 64 )		// far past the end: the header was wrong
				s.error = 1;
		}
	} else
		s.error = 1;

	free( s.block );
	free( s.amp );
	for (k=0; k<16; k++ )
		if ( s.hist[k] )
			free( s.hist[k] );

	if ( s.error ) {
		if ( out ) free( out );
		return NULL;
	}
	return out;
}
