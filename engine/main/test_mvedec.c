/*
 * ASCENT: new file. The test of the MVE decoder (mvedec.c).
 *
 *   test_mvedec                    the self test (a registered test)
 *   test_mvedec [-nosilence] FILE.MVE [FRAMES.raw [AUDIO.s16]]
 *
 * The self test builds a small movie in memory and checks the chunks, the
 * timer, the palette, the swap of the two pictures, some block codes and
 * the compressed sound.
 *
 * With a file name it decodes the whole movie and prints the counts. FRAMES.raw gets, for
 * each decoded picture, width * height palette indices and then the palette
 * as 256 entries of 4 bytes B, G, R, 255 with the 8-bit value (v << 2) |
 * (v >> 4): the layout of "ffmpeg -f rawvideo -pix_fmt pal8". AUDIO.s16 gets
 * the sound as signed 16-bit stereo, without the silence of opcode 0x09
 * (ffmpeg does not decode that opcode) if the option -nosilence is given.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mvedec.h"

static FILE *Audio_file;
static int Skip_silence;
static int64_t Audio_written;

static void audio(void *user, const int16_t *s, int frames)
{
	mve_decoder *m = user;
	if (Audio_file) {
		if (Skip_silence && m->audio_is_silence)
			return;
		fwrite(s, 4, frames, Audio_file);
		Audio_written += frames;
	}
}

//------------------------------------------------------------------------
// The self test

static uint8_t T_data[2048];
static int T_size, T_chunk;
static int16_t T_audio[64];
static int T_audio_n, T_silence_n;

static void t_put(const void *p, int n) { memcpy(T_data + T_size, p, n); T_size += n; }
static void t_w16(int v) { uint8_t b[2] = { v & 255, (v >> 8) & 255 }; t_put(b, 2); }
static void t_chunk(int type) { T_chunk = T_size; t_w16(0); t_w16(type); }
static void t_chunk_end(void)
{
	int len = T_size - T_chunk - 4;
	T_data[T_chunk] = len & 255;
	T_data[T_chunk + 1] = len >> 8;
}
static void t_op(int op, int ver, const void *p, int n)
{
	uint8_t h[4] = { n & 255, n >> 8, op, ver };
	t_put(h, 4);
	if (n)
		t_put(p, n);
}

static void t_audio(void *user, const int16_t *s, int frames)
{
	mve_decoder *m = user;
	int i;
	for (i = 0; i < frames * 2 && T_audio_n < 64; i++)
		T_audio[T_audio_n++] = s[i];
	if (m->audio_is_silence)
		T_silence_n += frames;
}

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); return 1; } } while (0)

static int self_test(void)
{
	mve_decoder m;
	uint8_t b[128];
	const uint8_t *f;
	int i;

	t_put("Interplay MVE File\x1A\0", 20);
	t_w16(0x1A); t_w16(0x100); t_w16(0x1133);

	// sound: stereo, 16-bit, compressed (version 1)
	t_chunk(0);
	{ uint8_t a[10] = { 0, 0, 7, 0, 0x22, 0x56, 0, 0, 0, 0 }; t_op(3, 1, a, 10); }
	t_op(1, 0, NULL, 0);
	t_chunk_end();

	// video: 2x1 blocks (16x8 pixels), palette entries 1 and 2
	t_chunk(2);
	{ uint8_t v[8] = { 2, 0, 1, 0, 1, 0, 0, 0 }; t_op(5, 2, v, 8); }
	{ uint8_t p[10] = { 1, 0, 2, 0, 10, 20, 30, 40, 50, 60 }; t_op(0xC, 0, p, 10); }
	t_op(1, 0, NULL, 0);
	t_chunk_end();

	// frame 1: block 0 code E (color 5), block 1 code B (64 pixels 0..63)
	t_chunk(3);
	{ uint8_t t[6] = { 0xE8, 0x03, 0, 0, 8, 0 }; t_op(2, 0, t, 6); }
	b[0] = 0xBE; t_op(0xF, 0, b, 1);
	{ uint8_t a[12] = { 0, 0, 1, 0, 8, 0, 100, 0, 0x9C, 0xFF, 1, 255 }; t_op(8, 0, a, 12); }
	memset(b, 0, 14); b[0] = 1; b[8] = 2; b[10] = 1;
	b[14] = 5;
	for (i = 0; i < 64; i++) b[15 + i] = i;
	t_op(0x11, 3, b, 79);
	t_op(4, 0, NULL, 0);
	{ uint8_t d[6] = { 1, 0, 2, 0, 0, 0 }; t_op(7, 1, d, 6); }
	t_op(1, 0, NULL, 0);
	t_chunk_end();

	// frame 2 (swap): block 0 code 0 (the previous picture), block 1 code 7
	// with P0 = 1, P1 = 2: bit 0 of the first row byte is the left pixel.
	t_chunk(3);
	b[0] = 0x70; t_op(0xF, 0, b, 1);
	{ uint8_t a[6] = { 0, 0, 0xFF, 0xFF, 8, 0 }; t_op(9, 0, a, 6); }
	memset(b, 0, 14); b[0] = 2; b[2] = 1; b[8] = 2; b[10] = 1; b[12] = 1;
	b[14] = 1; b[15] = 2; b[16] = 0x01; memset(b + 17, 0x80, 7);
	t_op(0x11, 3, b, 24);
	{ uint8_t d[6] = { 0 }; t_op(7, 1, d, 6); }
	t_op(1, 0, NULL, 0);
	t_chunk_end();

	// frame 3 (swap): code 1 in both blocks: the picture of frame 1 stays
	t_chunk(3);
	b[0] = 0x11; t_op(0xF, 0, b, 1);
	memset(b, 0, 14); b[0] = 3; b[2] = 2; b[8] = 2; b[10] = 1; b[12] = 1;
	t_op(0x11, 3, b, 14);
	{ uint8_t d[6] = { 0 }; t_op(7, 1, d, 6); }
	t_op(1, 0, NULL, 0);
	t_chunk_end();

	t_chunk(4);
	t_op(0, 0, NULL, 0);
	t_chunk_end();
	t_chunk(5);
	t_chunk_end();

	CHECK(mve_open(&m, T_data, T_size) == 0);
	mve_set_audio_func(&m, t_audio, &m);

	CHECK(mve_next_frame(&m) == 1);
	CHECK(m.width == 16 && m.height == 8);
	CHECK(m.frame_us == 8000);
	CHECK(m.audio_rate == 22050 && m.audio_stereo && m.audio_compressed);
	CHECK(m.pal[3] == 10 && m.pal[8] == 60);
	f = mve_frame(&m);
	CHECK(f[0] == 5 && f[7 * 16 + 7] == 5);
	CHECK(f[8] == 0 && f[9] == 1 && f[16 + 8] == 8 && f[7 * 16 + 15] == 63);
	// the start values are the first sample; then +1 (index 1) and -1 (index 255)
	CHECK(T_audio_n == 4);
	CHECK(T_audio[0] == 100 && T_audio[1] == -100 && T_audio[2] == 101 && T_audio[3] == -101);

	CHECK(mve_next_frame(&m) == 1);
	f = mve_frame(&m);
	CHECK(f[0] == 5 && f[7 * 16 + 7] == 5);
	CHECK(f[8] == 2 && f[9] == 1 && f[15] == 1);
	CHECK(f[16 + 8] == 1 && f[16 + 15] == 2);
	CHECK(T_silence_n == 2 && m.audio_silence_frames == 2);

	CHECK(mve_next_frame(&m) == 1);
	f = mve_frame(&m);
	CHECK(f[0] == 5 && f[8] == 0 && f[9] == 1 && f[7 * 16 + 15] == 63);

	CHECK(mve_next_frame(&m) == 0);
	CHECK(m.shown == 3 && m.video_frames == 3 && m.bad_blocks == 0);
	mve_close(&m);
	printf("test_mvedec: self test passed\n");
	return 0;
}

int main(int argc, char **argv)
{
	FILE *f, *ff = NULL;
	uint8_t *data;
	long size;
	mve_decoder m;
	int r, i, a = 1;

	if (argc > 1 && !strcmp(argv[1], "-nosilence")) {
		Skip_silence = 1;
		a++;
	}
	if (argc == 1)
		return self_test();
	if (argc <= a) {
		fprintf(stderr, "usage: test_mvedec [-nosilence] FILE.MVE [FRAMES.raw [AUDIO.s16]]\n");
		return 2;
	}
	f = fopen(argv[a], "rb");
	if (!f) {
		fprintf(stderr, "cannot open %s\n", argv[a]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc(size);
	if (!data || fread(data, 1, size, f) != (size_t) size) {
		fprintf(stderr, "cannot read %s\n", argv[a]);
		return 1;
	}
	fclose(f);
	if (argc > a + 1)
		ff = fopen(argv[a + 1], "wb");
	if (argc > a + 2)
		Audio_file = fopen(argv[a + 2], "wb");

	if (mve_open(&m, data, size) != 0) {
		fprintf(stderr, "%s is not an MVE file\n", argv[a]);
		return 1;
	}
	mve_set_audio_func(&m, audio, &m);
	while ((r = mve_next_frame(&m)) == 1) {
		if (ff && m.new_picture) {
			uint8_t pal[1024];
			for (i = 0; i < 256; i++) {
				uint8_t rr = m.pal[i * 3] & 63, g = m.pal[i * 3 + 1] & 63, b = m.pal[i * 3 + 2] & 63;
				pal[i * 4 + 0] = (b << 2) | (b >> 4);
				pal[i * 4 + 1] = (g << 2) | (g >> 4);
				pal[i * 4 + 2] = (rr << 2) | (rr >> 4);
				pal[i * 4 + 3] = 255;
			}
			fwrite(mve_frame(&m), 1, (size_t) m.width * m.height, ff);
			fwrite(pal, 1, 1024, ff);
		}
		m.new_picture = 0;
	}
	if (r < 0)
		printf("error: %s\n", m.error);
	printf("%s: %dx%d, frame %u us (rate %u, sub %u), %d pictures, %d displays, %d video inits\n",
		argv[a], m.width, m.height, m.frame_us, m.timer_rate, m.timer_sub, m.video_frames, m.shown, m.video_inits);
	printf("sound: %d Hz, %s, %s, %s; %lld frames, of which %lld silence (opcode 0x09)\n",
		m.audio_rate, m.audio_stereo ? "stereo" : "mono", m.audio_16bit ? "16-bit" : "8-bit",
		m.audio_compressed ? "DPCM" : "PCM", (long long) m.audio_frames, (long long) m.audio_silence_frames);
	printf("block codes:");
	for (i = 0; i < 16; i++)
		printf(" %X:%d", i, m.block_counts[i]);
	printf("\nblocks with a copy outside the picture: %d\n", m.bad_blocks);
	mve_close(&m);
	if (ff)
		fclose(ff);
	if (Audio_file)
		fclose(Audio_file);
	free(data);
	return r < 0;
}
