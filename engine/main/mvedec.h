/*
 * ASCENT: new file. A decoder for the Interplay MVE movies of Descent to
 * Undermountain (docs/formats/mve.md). It replaces the MVE library of
 * Interplay (MVE_RunMovie and its decoders) that DTUGAME.EXE links.
 *
 * The decoder reads a whole file from memory. It has no dependency on the
 * engine or on SDL, so that the tools and the tests can use it.
 */
#ifndef _MVEDEC_H
#define _MVEDEC_H

#include <stddef.h>
#include <stdint.h>

// The decoder gives the sound as signed 16-bit stereo at audio_rate.
// It calls the function in the order of the file, before it returns the
// frame of the same chunk.
typedef void (*mve_audio_func)(void *user, const int16_t *stereo, int frames);

typedef struct mve_decoder {
	const uint8_t *data;
	size_t size, pos;
	int ended;
	char error[128];

	// Opcode 0x02: one frame lasts timer_rate * timer_sub microseconds.
	uint32_t timer_rate;
	uint32_t timer_sub;
	uint32_t frame_us;

	// Opcode 0x03 and 0x04.
	int audio_present;
	int audio_rate;
	int audio_stereo;
	int audio_16bit;
	int audio_compressed;
	int audio_started;			// opcode 0x04 was seen
	mve_audio_func audio_func;
	void *audio_user;
	int16_t *audio_tmp;
	int audio_tmp_frames;
	int64_t audio_frames;		// stereo frames given to audio_func
	int64_t audio_silence_frames;	// the part of audio_frames from opcode 0x09
	int audio_is_silence;		// 1 while audio_func gets the silence of opcode 0x09

	// Opcode 0x05, 0x0C, 0x0F, 0x11 and 0x07.
	int width, height;			// in pixels
	uint8_t *buf[2];			// the two pictures (8 bits per pixel)
	int cur;					// buf[cur] is the newest picture
	uint8_t pal[768];			// 6 bits for each component
	const uint8_t *map;
	int map_size;
	int video_frames;			// number of pictures decoded (opcode 0x11)
	int shown;					// number of displays (opcode 0x07)
	int new_picture;			// 1 if a picture was decoded since the last display
	int video_inits;			// number of opcode 0x05
	int block_counts[16];		// statistics: the block codes used
	int bad_blocks;				// blocks with a copy outside the picture
} mve_decoder;

// Start the decoding of a file in memory. The data must stay valid until
// mve_close. Returns 0 if successful, -1 if the data is not an MVE file.
int mve_open(mve_decoder *m, const uint8_t *data, size_t size);
void mve_close(mve_decoder *m);
void mve_set_audio_func(mve_decoder *m, mve_audio_func func, void *user);

// Process the chunks up to the next display (opcode 0x07). Returns 1 when
// a frame is ready (mve_frame and m->pal), 0 at the end of the movie and
// -1 after an error (m->error).
int mve_next_frame(mve_decoder *m);

// The newest picture: width * height bytes, one row after the other.
const uint8_t *mve_frame(const mve_decoder *m);

#endif
