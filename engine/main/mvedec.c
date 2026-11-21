/*
 * ASCENT: new file. The Interplay MVE decoder (see mvedec.h). It replaces
 * the MVE library of Interplay in DTUGAME.EXE (MVE_RunMovie 0xF5FA0).
 *
 * Written from the format description in docs/formats/mve.md. The order of
 * the bits in the pattern blocks was checked against the decoded frames of
 * ffmpeg for every movie of the game (see docs/formats/mve.md).
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mvedec.h"

static const char Mve_signature[20] = "Interplay MVE File\x1A";

static unsigned get16(const uint8_t *p)
{
	return p[0] | (p[1] << 8);
}

static uint32_t get32(const uint8_t *p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24);
}

static uint64_t get64(const uint8_t *p)
{
	return get32(p) | ((uint64_t) get32(p + 4) << 32);
}

static int fail(mve_decoder *m, const char *text)
{
	snprintf(m->error, sizeof(m->error), "%s (file offset %u)", text, (unsigned) m->pos);
	m->ended = 1;
	return -1;
}

int mve_open(mve_decoder *m, const uint8_t *data, size_t size)
{
	memset(m, 0, sizeof(*m));
	if (size < 26 || memcmp(data, Mve_signature, 20) != 0)
		return -1;
	// The 3 words after the signature are 0x001A, 0x0100, 0x1133 in each file.
	if (get16(data + 20) != 0x001A || get16(data + 22) != 0x0100 || get16(data + 24) != 0x1133)
		return -1;
	m->data = data;
	m->size = size;
	m->pos = 26;
	return 0;
}

void mve_close(mve_decoder *m)
{
	free(m->buf[0]);
	free(m->buf[1]);
	free(m->audio_tmp);
	m->buf[0] = m->buf[1] = NULL;
	m->audio_tmp = NULL;
}

void mve_set_audio_func(mve_decoder *m, mve_audio_func func, void *user)
{
	m->audio_func = func;
	m->audio_user = user;
}

const uint8_t *mve_frame(const mve_decoder *m)
{
	return m->buf[m->cur];
}

//------------------------------------------------------------------------
// Sound

// The delta table of the compressed sound (Interplay DPCM). The values
// near the index 128 are not monotonic: that is the table of the format.
static const int16_t Dpcm_delta[256] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 47, 51, 56, 61,
	66, 72, 79, 86, 94, 102, 112, 122, 133, 145, 158, 173, 189, 206, 225, 245,
	267, 292, 318, 348, 379, 414, 452, 493, 538, 587, 640, 699, 763, 832, 908, 991,
	1081, 1180, 1288, 1405, 1534, 1673, 1826, 1993, 2175, 2373, 2590, 2826, 3084, 3365, 3672, 4008,
	4373, 4772, 5208, 5683, 6202, 6767, 7385, 8059, 8794, 9597, 10472, 11428, 12471, 13609, 14851, 16206,
	17685, 19298, 21060, 22981, 25078, 27367, 29864, 32589, -29973, -26728, -23186, -19322, -15105, -10503, -5481, -1,
	1, 1, 5481, 10503, 15105, 19322, 23186, 26728, 29973, -32589, -29864, -27367, -25078, -22981, -21060, -19298,
	-17685, -16206, -14851, -13609, -12471, -11428, -10472, -9597, -8794, -8059, -7385, -6767, -6202, -5683, -5208, -4772,
	-4373, -4008, -3672, -3365, -3084, -2826, -2590, -2373, -2175, -1993, -1826, -1673, -1534, -1405, -1288, -1180,
	-1081, -991, -908, -832, -763, -699, -640, -587, -538, -493, -452, -414, -379, -348, -318, -292,
	-267, -245, -225, -206, -189, -173, -158, -145, -133, -122, -112, -102, -94, -86, -79, -72,
	-66, -61, -56, -51, -47, -43, -42, -41, -40, -39, -38, -37, -36, -35, -34, -33,
	-32, -31, -30, -29, -28, -27, -26, -25, -24, -23, -22, -21, -20, -19, -18, -17,
	-16, -15, -14, -13, -12, -11, -10, -9, -8, -7, -6, -5, -4, -3, -2, -1
};

static int16_t *audio_space(mve_decoder *m, int frames)
{
	if (frames > m->audio_tmp_frames) {
		int16_t *p = realloc(m->audio_tmp, (size_t) frames * 2 * sizeof(int16_t));
		if (!p)
			return NULL;
		m->audio_tmp = p;
		m->audio_tmp_frames = frames;
	}
	return m->audio_tmp;
}

static void audio_out(mve_decoder *m, const int16_t *s, int frames, int silence)
{
	if (frames <= 0)
		return;
	m->audio_frames += frames;
	if (silence)
		m->audio_silence_frames += frames;
	m->audio_is_silence = silence;
	if (m->audio_func)
		m->audio_func(m->audio_user, s, frames);
}

// Opcode 0x08 (data) and 0x09 (silence). len is the size of the decoded
// sound in bytes. Only stream 0 (bit 0 of the mask) is played: the files
// of the game have one stream.
static void audio_frame(mve_decoder *m, const uint8_t *p, int size, int silence)
{
	unsigned mask, len;
	int ch, bytes, frames, i;
	int16_t *out;

	if (!m->audio_present || size < 6)
		return;
	mask = get16(p + 2);
	len = get16(p + 4);
	if (!(mask & 1))
		return;
	ch = m->audio_stereo ? 2 : 1;
	bytes = m->audio_16bit ? 2 : 1;
	frames = len / (ch * bytes);
	out = audio_space(m, frames);
	if (!out)
		return;
	p += 6;
	size -= 6;

	if (silence) {
		memset(out, 0, (size_t) frames * 4);
		audio_out(m, out, frames, 1);
		return;
	}

	if (m->audio_compressed) {
		// The first word of each channel is the start value and the first
		// sample. Each byte after it is an index into the delta table.
		int pred[2], n = frames * ch, k;
		if (size < 2 * ch)
			return;
		for (i = 0; i < ch; i++)
			pred[i] = (int16_t) get16(p + 2 * i);
		k = 2 * ch;
		for (i = 0; i < n; i++) {
			int c = i % ch;
			if (i >= ch) {
				if (k >= size)
					break;
				pred[c] += Dpcm_delta[p[k++]];
				if (pred[c] > 32767) pred[c] = 32767;
				if (pred[c] < -32768) pred[c] = -32768;
			}
			if (ch == 2)
				out[i] = (int16_t) pred[c];
			else
				out[2 * i] = out[2 * i + 1] = (int16_t) pred[c];
		}
		frames = i / ch;
	} else {
		int n = frames * ch;
		if (n * bytes > size)
			n = size / bytes;
		for (i = 0; i < n; i++) {
			int v = bytes == 2 ? (int16_t) get16(p + 2 * i) : (p[i] - 128) * 256;
			if (ch == 2)
				out[i] = (int16_t) v;
			else
				out[2 * i] = out[2 * i + 1] = (int16_t) v;
		}
		frames = n / ch;
	}
	audio_out(m, out, frames, 0);
}

//------------------------------------------------------------------------
// Video (8 bits per pixel)

typedef struct {
	const uint8_t *p, *end;
	int bad;
} reader;

static const uint8_t *take(reader *r, int n)
{
	static const uint8_t zero[64];
	const uint8_t *p = r->p;
	if (r->end - r->p < n) {
		r->bad = 1;
		r->p = r->end;
		return zero;
	}
	r->p += n;
	return p;
}

static int byte(reader *r)
{
	return *take(r, 1);
}

// Copy an 8x8 block from src at the block position plus (dx, dy).
static void copy_block(mve_decoder *m, uint8_t *dst, const uint8_t *src, int x, int y, int dx, int dy)
{
	int w = m->width, sx = x + dx, sy = y + dy, j;

	if (sx < 0 || sy < 0 || sx + 8 > w || sy + 8 > m->height) {
		m->bad_blocks++;
		return;
	}
	for (j = 0; j < 8; j++)
		memcpy(dst + (size_t) (y + j) * w + x, src + (size_t) (sy + j) * w + sx, 8);
}

// Fill the 8x8 block from a list of 64 colors.
static void put_block(mve_decoder *m, uint8_t *dst, int x, int y, const uint8_t *pix)
{
	int j;
	for (j = 0; j < 8; j++)
		memcpy(dst + (size_t) (y + j) * m->width + x, pix + j * 8, 8);
}

static void decode_block(mve_decoder *m, int code, reader *r, int x, int y)
{
	uint8_t *dst = m->buf[m->cur];
	const uint8_t *prev = m->buf[m->cur ^ 1];
	uint8_t pix[64], P[16];
	const uint8_t *s;
	int i, j, b, q;
	uint64_t bits;

	switch (code) {
	case 0x0:	// the block of the previous picture
		copy_block(m, dst, prev, x, y, 0, 0);
		return;
	case 0x1:	// no change: the buffer holds the picture before the previous one
		return;
	case 0x2:	// copy from the new picture, below or to the right
	case 0x3:	// the same, above or to the left
		b = byte(r);
		if (b < 56) {
			i = 8 + b % 7;
			j = b / 7;
		} else {
			i = -14 + (b - 56) % 29;
			j = 8 + (b - 56) / 29;
		}
		if (code == 3) {
			i = -i;
			j = -j;
		}
		copy_block(m, dst, dst, x, y, i, j);
		return;
	case 0x4:	// copy from the previous picture, -8 to +7
		b = byte(r);
		copy_block(m, dst, prev, x, y, (b & 15) - 8, (b >> 4) - 8);
		return;
	case 0x5:	// copy from the previous picture, signed bytes
		s = take(r, 2);
		copy_block(m, dst, prev, x, y, (int8_t) s[0], (int8_t) s[1]);
		return;
	case 0x6:	// not used by the files of the game
		return;
	case 0x7:	// 2 colors
		s = take(r, 2);
		P[0] = s[0];
		P[1] = s[1];
		if (P[0] <= P[1]) {
			s = take(r, 8);
			for (j = 0; j < 8; j++)
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(s[j] >> i) & 1];
		} else {
			bits = get16(take(r, 2));
			for (j = 0; j < 8; j++)
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(bits >> ((j >> 1) * 4 + (i >> 1))) & 1];
		}
		put_block(m, dst, x, y, pix);
		return;
	case 0x8:	// 2 colors for each quadrant or for each half
		s = take(r, 2);
		P[0] = s[0];
		P[1] = s[1];
		if (P[0] <= P[1]) {
			// The quadrants in the order top left, bottom left, top right,
			// bottom right. Each has 2 colors and 16 bits.
			for (q = 0; q < 4; q++) {
				int qx = (q >> 1) * 4, qy = (q & 1) * 4;
				if (q) {
					s = take(r, 2);
					P[0] = s[0];
					P[1] = s[1];
				}
				bits = get16(take(r, 2));
				for (j = 0; j < 4; j++)
					for (i = 0; i < 4; i++)
						pix[(qy + j) * 8 + qx + i] = P[(bits >> (j * 4 + i)) & 1];
			}
		} else {
			uint32_t f0 = get32(take(r, 4)), f1;
			s = take(r, 2);
			P[2] = s[0];
			P[3] = s[1];
			f1 = get32(take(r, 4));
			if (P[2] <= P[3]) {
				// left half and right half, 4x8 each
				for (j = 0; j < 8; j++)
					for (i = 0; i < 4; i++) {
						pix[j * 8 + i] = P[(f0 >> (j * 4 + i)) & 1];
						pix[j * 8 + 4 + i] = P[2 + ((f1 >> (j * 4 + i)) & 1)];
					}
			} else {
				// top half and bottom half, 8x4 each
				for (j = 0; j < 4; j++)
					for (i = 0; i < 8; i++) {
						pix[j * 8 + i] = P[(f0 >> (j * 8 + i)) & 1];
						pix[(j + 4) * 8 + i] = P[2 + ((f1 >> (j * 8 + i)) & 1)];
					}
			}
		}
		put_block(m, dst, x, y, pix);
		return;
	case 0x9:	// 4 colors, 2 bits each
		s = take(r, 4);
		memcpy(P, s, 4);
		if (P[0] <= P[1] && P[2] <= P[3]) {
			// 1x1: 2 bytes for each row
			s = take(r, 16);
			for (j = 0; j < 8; j++) {
				unsigned row = get16(s + 2 * j);
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(row >> (2 * i)) & 3];
			}
		} else if (P[0] <= P[1]) {
			// 2x2 cells: 32 bits
			bits = get32(take(r, 4));
			for (j = 0; j < 8; j++)
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(bits >> (2 * ((j >> 1) * 4 + (i >> 1)))) & 3];
		} else if (P[2] <= P[3]) {
			// 2x1 cells (2 wide, 1 high): 64 bits
			bits = get64(take(r, 8));
			for (j = 0; j < 8; j++)
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(bits >> (2 * (j * 4 + (i >> 1)))) & 3];
		} else {
			// 1x2 cells (1 wide, 2 high): 64 bits
			bits = get64(take(r, 8));
			for (j = 0; j < 8; j++)
				for (i = 0; i < 8; i++)
					pix[j * 8 + i] = P[(bits >> (2 * ((j >> 1) * 8 + i))) & 3];
		}
		put_block(m, dst, x, y, pix);
		return;
	case 0xA:	// 4 colors for each quadrant or for each half
		s = take(r, 4);
		memcpy(P, s, 4);
		if (P[0] <= P[1]) {
			for (q = 0; q < 4; q++) {
				int qx = (q >> 1) * 4, qy = (q & 1) * 4;
				if (q)
					memcpy(P, take(r, 4), 4);
				bits = get32(take(r, 4));
				for (j = 0; j < 4; j++)
					for (i = 0; i < 4; i++)
						pix[(qy + j) * 8 + qx + i] = P[(bits >> (2 * (j * 4 + i))) & 3];
			}
		} else {
			uint64_t f0 = get64(take(r, 8)), f1;
			memcpy(P + 4, take(r, 4), 4);
			f1 = get64(take(r, 8));
			if (P[4] <= P[5]) {
				for (j = 0; j < 8; j++)
					for (i = 0; i < 4; i++) {
						pix[j * 8 + i] = P[(f0 >> (2 * (j * 4 + i))) & 3];
						pix[j * 8 + 4 + i] = P[4 + ((f1 >> (2 * (j * 4 + i))) & 3)];
					}
			} else {
				for (j = 0; j < 4; j++)
					for (i = 0; i < 8; i++) {
						pix[j * 8 + i] = P[(f0 >> (2 * (j * 8 + i))) & 3];
						pix[(j + 4) * 8 + i] = P[4 + ((f1 >> (2 * (j * 8 + i))) & 3)];
					}
			}
		}
		put_block(m, dst, x, y, pix);
		return;
	case 0xB:	// 64 pixels
		put_block(m, dst, x, y, take(r, 64));
		return;
	case 0xC:	// 16 colors, one for each 2x2 cell
		s = take(r, 16);
		for (j = 0; j < 8; j++)
			for (i = 0; i < 8; i++)
				pix[j * 8 + i] = s[(j >> 1) * 4 + (i >> 1)];
		put_block(m, dst, x, y, pix);
		return;
	case 0xD:	// 4 colors, one for each 4x4 quadrant
		s = take(r, 4);
		for (j = 0; j < 8; j++)
			for (i = 0; i < 8; i++)
				pix[j * 8 + i] = s[(j >> 2) * 2 + (i >> 2)];
		put_block(m, dst, x, y, pix);
		return;
	case 0xE:	// one color
		memset(pix, byte(r), 64);
		put_block(m, dst, x, y, pix);
		return;
	case 0xF:	// 2 colors in a checkerboard
		s = take(r, 2);
		for (j = 0; j < 8; j++)
			for (i = 0; i < 8; i++)
				pix[j * 8 + i] = s[(i + j) & 1];
		put_block(m, dst, x, y, pix);
		return;
	}
}

// Opcode 0x05: the size of the picture in 8x8 blocks.
static int video_init(mve_decoder *m, const uint8_t *p, int size, int version)
{
	int w, h;

	if (size < 4)
		return fail(m, "short opcode 0x05");
	w = get16(p) * 8;
	h = get16(p + 2) * 8;
	if (version >= 2 && size >= 8 && get16(p + 6))
		return fail(m, "16-bit video is not supported");
	if (w <= 0 || h <= 0 || w > 4096 || h > 4096)
		return fail(m, "bad video size");
	if (w != m->width || h != m->height || !m->buf[0]) {
		free(m->buf[0]);
		free(m->buf[1]);
		m->buf[0] = calloc((size_t) w * h, 1);
		m->buf[1] = calloc((size_t) w * h, 1);
		if (!m->buf[0] || !m->buf[1])
			return fail(m, "out of memory");
		m->width = w;
		m->height = h;
	} else {
		memset(m->buf[0], 0, (size_t) w * h);
		memset(m->buf[1], 0, (size_t) w * h);
	}
	m->cur = 0;
	m->video_inits++;
	return 0;
}

// Opcode 0x11: 14 bytes of header, then the data of the blocks.
static int video_data(mve_decoder *m, const uint8_t *p, int size)
{
	reader r;
	int bx, by, n = 0, nb;

	if (!m->buf[0])
		return fail(m, "video data before opcode 0x05");
	if (size < 14)
		return fail(m, "short opcode 0x11");
	nb = (m->width / 8) * (m->height / 8);
	if (!m->map || m->map_size * 2 < nb)
		return fail(m, "no decoding map for opcode 0x11");
	// Bit 0 of the flags word: the two pictures change places, so the new
	// picture is drawn over the picture before the previous one.
	if (get16(p + 12) & 1)
		m->cur ^= 1;
	r.p = p + 14;
	r.end = p + size;
	r.bad = 0;
	for (by = 0; by < m->height; by += 8)
		for (bx = 0; bx < m->width; bx += 8, n++) {
			int code = (m->map[n >> 1] >> ((n & 1) * 4)) & 15;
			m->block_counts[code]++;
			decode_block(m, code, &r, bx, by);
		}
	if (r.bad)
		return fail(m, "the block data of opcode 0x11 ends too soon");
	m->video_frames++;
	m->new_picture = 1;
	return 0;
}

//------------------------------------------------------------------------

int mve_next_frame(mve_decoder *m)
{
	while (!m->ended) {
		size_t clen, end;
		unsigned ctype;

		if (m->pos + 4 > m->size) {
			m->ended = 1;
			break;
		}
		clen = get16(m->data + m->pos);
		ctype = get16(m->data + m->pos + 2);
		(void) ctype;
		m->pos += 4;
		end = m->pos + clen;
		if (end > m->size)
			return fail(m, "chunk past the end of the file");

		{
			int show = 0;
			while (m->pos + 4 <= end) {
				const uint8_t *p = m->data + m->pos + 4;
				int len = get16(m->data + m->pos);
				int op = m->data[m->pos + 2], ver = m->data[m->pos + 3];

				if (m->pos + 4 + len > end)
					return fail(m, "opcode past the end of the chunk");
				m->pos += 4 + len;

				switch (op) {
				case 0x00:		// end of the stream
					m->ended = 1;
					break;
				case 0x01:		// end of the chunk
					break;
				case 0x02:		// timer
					if (len < 6)
						return fail(m, "short opcode 0x02");
					m->timer_rate = get32(p);
					m->timer_sub = get16(p + 4);
					m->frame_us = m->timer_rate * m->timer_sub;
					break;
				case 0x03:		// sound format
					if (len < 8)
						return fail(m, "short opcode 0x03");
					m->audio_present = 1;
					m->audio_stereo = get16(p + 2) & 1;
					m->audio_16bit = (get16(p + 2) >> 1) & 1;
					m->audio_compressed = ver >= 1 ? (get16(p + 2) >> 2) & 1 : 0;
					m->audio_rate = get16(p + 4);
					if (m->audio_compressed)
						m->audio_16bit = 1;
					break;
				case 0x04:		// start the sound
					m->audio_started = 1;
					break;
				case 0x05:
					if (video_init(m, p, len, ver) < 0)
						return -1;
					break;
				case 0x07:		// show the picture
					show = 1;
					break;
				case 0x08:
					audio_frame(m, p, len, 0);
					break;
				case 0x09:
					audio_frame(m, p, len, 1);
					break;
				case 0x0A:		// video mode: 640x480 in the files of the game
					break;
				case 0x0C:		// palette: first, count, 3 bytes each
					if (len >= 4) {
						unsigned first = get16(p), count = get16(p + 2);
						if (first + count > 256 || 4 + count * 3 > (unsigned) len)
							return fail(m, "bad palette");
						memcpy(m->pal + first * 3, p + 4, count * 3);
					}
					break;
				case 0x0F:		// decoding map
					m->map = p;
					m->map_size = len;
					break;
				case 0x11:
					if (video_data(m, p, len) < 0)
						return -1;
					break;
				case 0x13:		// not used by the player (132 bytes)
				case 0x15:		// not used by the player (4 bytes)
					break;
				default:
					return fail(m, "unknown opcode");
				}
				if (m->ended)
					break;
			}
			if (!m->ended)
				m->pos = end;
			if (show && m->buf[0]) {
				m->shown++;
				return 1;
			}
		}
	}
	return 0;
}
