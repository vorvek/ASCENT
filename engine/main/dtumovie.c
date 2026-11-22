/*
 * ASCENT: new file. The movie player (see dtumovie.h). It replaces the
 * play function 0x96439 of DTUGAME.EXE and the MVE library of Interplay
 * that it calls (MVE_RunMovie 0xF5FA0). docs/re/movies.md describes the
 * original; mvedec.c decodes the files.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include "args.h"
#include "key.h"
#include "mouse.h"
#include "game.h"
#include "kconfig.h"
#include "digi.h"
#include "dtufile.h"
#include "dtumusic.h"
#include "dtuobj.h"
#include "dtumovie.h"
#include "dtuopts.h"
#include "mvedec.h"
#include "platform.h"
#include "gr.h"
#include "dtulang.h"		// ASCENT: the subtitles

// pl_mpeg (MIT license) decodes the upscaled movies (engine/third_party/pl_mpeg).
#define PL_MPEG_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include "../third_party/pl_mpeg/pl_mpeg.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// The original shows the movies in VESA mode 0x101 (640x480) with the
// picture in the middle (0xF4490: x = (640 - width) / 2 rounded down to a
// multiple of 4, y = (480 - height) / 2).
#define NATIVE_W	640
#define NATIVE_H	480

//------------------------------------------------------------------------
// Sound: a ring buffer from the game thread to the mixer thread.

#define RING_FRAMES	(1 << 18)		// 11.9 s at 22050 Hz
static int16_t Ring[RING_FRAMES * 2];
static uint32_t Ring_read, Ring_write;	// frame counters; the mixer moves Ring_read
static int Ring_dropped;
static int Ring_on;						// 1 while the mixer reads the ring

static int ring_callback(void *user, int16_t *buffer, int frames)
{
	uint32_t r = __atomic_load_n(&Ring_read, __ATOMIC_RELAXED);
	uint32_t w = __atomic_load_n(&Ring_write, __ATOMIC_ACQUIRE);
	uint32_t n = w - r, i;

	(void) user;
	if (n > (uint32_t) frames)
		n = frames;
	for (i = 0; i < n; i++) {
		uint32_t k = (r + i) & (RING_FRAMES - 1);
		buffer[2 * i] = Ring[2 * k];
		buffer[2 * i + 1] = Ring[2 * k + 1];
	}
	__atomic_store_n(&Ring_read, r + n, __ATOMIC_RELEASE);
	return n;
}

static void ring_push(void *user, const int16_t *s, int frames)
{
	uint32_t r = __atomic_load_n(&Ring_read, __ATOMIC_ACQUIRE);
	uint32_t w = __atomic_load_n(&Ring_write, __ATOMIC_RELAXED);
	int i;

	(void) user;
	if ((int) (RING_FRAMES - (w - r)) < frames) {
		Ring_dropped += frames;
		return;
	}
	for (i = 0; i < frames; i++) {
		uint32_t k = (w + i) & (RING_FRAMES - 1);
		Ring[2 * k] = s[2 * i];
		Ring[2 * k + 1] = s[2 * i + 1];
	}
	__atomic_store_n(&Ring_write, w + frames, __ATOMIC_RELEASE);
}

//------------------------------------------------------------------------
// The upscaled movie

typedef struct {
	plm_video_t *video;
	uint8_t *bgra;				// the newest picture, 4 bytes for each pixel
	int w, h;
	int frames;
	int ok;
} hd_movie;

// ASCENT: the upscaled movie is NAME.m1v in the hd directory (dtu_hd_dir,
// dtuopts.c: -hddir, or "hd" in the current directory or in a data
// directory). The setting "Upscaled movies" of the Graphics menu and the
// GPU renderer select it; -nohdmovies and -movienative turn it off.
static int hd_open(hd_movie *hd, const char *mve_name)
{
	char base[64], name[80], path[700];
	plm_buffer_t *buf;
	const char *dot, *dir;
	FILE *f;
	int t;

	memset(hd, 0, sizeof(*hd));
	if (!DTU_GFX(Dtu_upscaled_movies) || FindArg("-nohdmovies") || FindArg("-movienative"))
		return 0;
	if ((dir = dtu_hd_dir()) == NULL)
		return 0;
	dot = strrchr(mve_name, '.');
	t = dot ? (int) (dot - mve_name) : (int) strlen(mve_name);
	if (t >= (int) sizeof(base))
		return 0;
	memcpy(base, mve_name, t);
	base[t] = 0;
	snprintf(name, sizeof(name), "%s.m1v", base);
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	if ((f = fopen(path, "rb")) == NULL)
		return 0;
	fclose(f);

	buf = plm_buffer_create_with_filename(path);
	if (!buf)
		return 0;
	hd->video = plm_video_create_with_buffer(buf, 1);
	if (!hd->video || !plm_video_has_header(hd->video)) {
		dtu_log("MOVIE: %s is not an MPEG-1 video stream\n", path);
		if (hd->video)
			plm_video_destroy(hd->video);
		hd->video = NULL;
		return 0;
	}
	hd->w = plm_video_get_width(hd->video);
	hd->h = plm_video_get_height(hd->video);
	hd->bgra = malloc((size_t) hd->w * hd->h * 4);
	if (!hd->bgra) {
		plm_video_destroy(hd->video);
		hd->video = NULL;
		return 0;
	}
	dtu_log("MOVIE: upscaled movie %s, %dx%d\n", path, hd->w, hd->h);
	return 1;
}

// Decode the next picture. Returns 0 when the stream has no more pictures.
static int hd_next(hd_movie *hd)
{
	plm_frame_t *f;

	if (!hd->video)
		return 0;
	f = plm_video_decode(hd->video);
	if (!f) {
		hd->ok = 0;
		return 0;
	}
	plm_frame_to_bgra(f, hd->bgra, hd->w * 4);
	hd->frames++;
	hd->ok = 1;
	return 1;
}

static void hd_close(hd_movie *hd)
{
	if (hd->video)
		plm_video_destroy(hd->video);
	free(hd->bgra);
	memset(hd, 0, sizeof(*hd));
}

//------------------------------------------------------------------------
// Test output

static int write_ppm(const char *file, const uint8_t *pix, int w, int h, const uint8_t *pal6, const uint8_t *bgra)
{
	FILE *f = fopen(file, "wb");
	int i;

	if (!f)
		return 0;
	fprintf(f, "P6\n%d %d\n255\n", w, h);
	for (i = 0; i < w * h; i++) {
		uint8_t rgb[3];
		if (bgra) {
			rgb[0] = bgra[i * 4 + 2];
			rgb[1] = bgra[i * 4 + 1];
			rgb[2] = bgra[i * 4 + 0];
		} else {
			int c;
			for (c = 0; c < 3; c++) {
				int v = pal6[pix[i] * 3 + c] & 63;
				rgb[c] = (v << 2) | (v >> 4);
			}
		}
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	return 1;
}

//------------------------------------------------------------------------
// ASCENT: subtitles (not in the executable). With a language other than
// English the speech of a movie gets the cues of <lang dir>/<MOVIE>.<code>.srt
// (dtulang.h). The text is drawn with the medium font of the front end
// (FNTEMFM_), white with a black outline, centered near the bottom of the
// 640x480 picture; an upscaled movie gets the same text scaled by its size.
// The time of a cue is the time of the picture (the frame number times the
// frame time), so a movie with -moviefast shows the same cues.

static grs_font *Sub_font;
static int Sub_font_tried;
static const dtu_sub_cue *Sub_cues;
static int Sub_n;

static grs_font *sub_font(void)
{
	char path[600];

	if (!Sub_font_tried) {
		Sub_font_tried = 1;
		if (dtu_find_file("FNTEMFM_.FNT", path, sizeof(path))) {
			grs_font *save = grd_curcanv ? grd_curcanv->cv_font : NULL;
			Sub_font = gr_init_font(path);
			if (grd_curcanv)
				grd_curcanv->cv_font = save;
		}
	}
	return Sub_font;
}

// The width of a line in pixels of the font
static int sub_width(grs_font *f, const char *s, int n)
{
	int w = 0, i;

	for (i = 0; i < n; i++) {
		int c = (ubyte) s[i] - f->ft_minchar;
		if (c < 0 || c > f->ft_maxchar - f->ft_minchar)
			w += f->ft_w / 2;
		else
			w += (f->ft_flags & FT_PROPORTIONAL) ? f->ft_widths[c] : f->ft_w;
	}
	return w;
}

// 1 if the pixel (x, y) of the glyph of character c is set
static int sub_pixel(grs_font *f, int c, int x, int y)
{
	int letter = c - f->ft_minchar, w;
	ubyte *p;

	if (letter < 0 || letter > f->ft_maxchar - f->ft_minchar)
		return 0;
	w = (f->ft_flags & FT_PROPORTIONAL) ? f->ft_widths[letter] : f->ft_w;
	p = (f->ft_flags & FT_PROPORTIONAL) ? f->ft_chars[letter] :
		f->ft_data + letter * ((f->ft_flags & FT_COLOR) ? f->ft_w : (f->ft_w + 7) / 8) * f->ft_h;
	if (!p || x >= w)
		return 0;
	if (f->ft_flags & FT_COLOR)
		return p[y * w + x] != 255;
	return (p[y * ((w + 7) / 8) + (x >> 3)] >> (7 - (x & 7))) & 1;
}

// The mask of the text of a cue in the picture of the movie (mw x mh, at
// most 640x480): 0 nothing, 1 outline, 2 text. The lines are centered, the
// last one 8 pixels above the bottom of the picture. Returns 0 if no cue
// is shown.
static ubyte Sub_mask[NATIVE_W * NATIVE_H];
static int Sub_w, Sub_h, Sub_y0, Sub_y1;	// the size of the mask, the rows with text

static int sub_build(int ms, int mw, int mh)
{
	grs_font *f;
	const char *text = NULL, *line;
	int i, lines = 1, y, lh;

	for (i = 0; i < Sub_n; i++)
		if (ms >= Sub_cues[i].start_ms && ms < Sub_cues[i].end_ms) {
			text = Sub_cues[i].text;
			break;
		}
	if (!text || !text[0] || mw > NATIVE_W || mh > NATIVE_H || (f = sub_font()) == NULL)
		return 0;
	for (line = text; *line; line++)
		if (*line == '\n')
			lines++;
	lh = f->ft_h + 2;
	Sub_w = mw;
	Sub_h = mh;
	memset(Sub_mask, 0, sizeof(Sub_mask));
	y = mh - 8 - lines * lh;
	if (y < 1)
		y = 1;
	Sub_y0 = y - 1;
	Sub_y1 = y + lines * lh + 1;
	for (line = text; *line; ) {
		int n = (int) strcspn(line, "\n"), x = (mw - sub_width(f, line, n)) / 2, k;
		for (k = 0; k < n; k++) {
			int c = (ubyte) line[k], letter = c - f->ft_minchar, w, gx, gy;
			w = (letter < 0 || letter > f->ft_maxchar - f->ft_minchar) ? f->ft_w / 2 :
				(f->ft_flags & FT_PROPORTIONAL) ? f->ft_widths[letter] : f->ft_w;
			for (gy = 0; gy < f->ft_h; gy++)
				for (gx = 0; gx < w; gx++)
					if (sub_pixel(f, c, gx, gy)) {
						int px = x + gx, py = y + gy, dx, dy;
						if (px < 1 || px >= mw - 1 || py < 1 || py >= mh - 1)
							continue;
						for (dy = -1; dy <= 1; dy++)
							for (dx = -1; dx <= 1; dx++)
								if (!Sub_mask[(py + dy) * mw + px + dx])
									Sub_mask[(py + dy) * mw + px + dx] = 1;
						Sub_mask[py * mw + px] = 2;
					}
			x += w;
		}
		line += n;
		if (*line == '\n')
			line++;
		y += lh;
	}
	if (Sub_y0 < 0) Sub_y0 = 0;
	if (Sub_y1 > mh) Sub_y1 = mh;
	return 1;
}

// The palette index nearest to a color (6-bit values)
static int sub_color(const uint8_t *pal6, int r, int g, int b)
{
	int i, best = 0, best_d = 0x7FFFFFFF;

	for (i = 0; i < 256; i++) {
		int dr = (pal6[i * 3] & 63) - r, dg = (pal6[i * 3 + 1] & 63) - g, db = (pal6[i * 3 + 2] & 63) - b;
		int d = dr * dr + dg * dg + db * db;
		if (d < best_d) {
			best_d = d;
			best = i;
		}
	}
	return best;
}

// Draws the mask into an 8-bit picture with a row length of pitch; the
// movie picture starts at (ox, oy) in it.
static void sub_draw8(uint8_t *pix, int pitch, int ox, int oy, const uint8_t *pal6)
{
	int white = sub_color(pal6, 63, 63, 63), black = sub_color(pal6, 0, 0, 0), x, y;

	for (y = Sub_y0; y < Sub_y1; y++)
		for (x = 0; x < Sub_w; x++) {
			int m = Sub_mask[y * Sub_w + x];
			if (m)
				pix[(oy + y) * pitch + ox + x] = m == 2 ? white : black;
		}
}

// Draws the mask into the 32-bit picture of an upscaled movie (w x h shows
// the whole movie picture).
static void sub_draw32(uint32_t *pix, int w, int h)
{
	int x, y;

	for (y = 0; y < h; y++) {
		int sy = (int) ((long long) y * Sub_h / h), row;
		if (sy < Sub_y0 || sy >= Sub_y1)
			continue;
		row = sy * Sub_w;
		for (x = 0; x < w; x++) {
			int m = Sub_mask[row + (int) ((long long) x * Sub_w / w)];
			if (m)
				pix[y * w + x] = m == 2 ? 0xFFFFFFFFu : 0xFF000000u;
		}
	}
}

//------------------------------------------------------------------------
// The player

static int Movie_frame_limit;		// -movieframes
static const char *Movie_shot;		// -movieshot
static int Movie_fast;				// -moviefast: no wait (test)

// The skip test of the control callback 0x9640C: key_inkey() returns a key.
// ASCENT: the Alt keys do not skip (Alt+Tab, Alt+Enter), and a mouse
// button skips too.
static int skip_requested(void)
{
	int k, skip = 0;

	while ((k = key_inkey()) != 0) {
		int scan = k & 0xFF;
		if (scan != KEY_LALT && scan != KEY_RALT)
			skip = 1;
	}
	if (mouse_button_down_count(MB_LEFT) || mouse_button_down_count(MB_RIGHT))
		skip = 1;
	return skip;
}

// ASCENT: the last picture that was shown, for -movieshot (with the
// subtitles)
static const uint8_t *Shown8;
static const uint32_t *Shown32;
static int Shown_w, Shown_h;

static void present(mve_decoder *m, hd_movie *hd, int native, int ms)
{
	static uint8_t screen[NATIVE_W * NATIVE_H];
	static uint8_t frame8[NATIVE_W * NATIVE_H];
	static uint32_t *frame32;
	static size_t frame32_size;
	int sub = Sub_n > 0 && sub_build(ms, m->width, m->height);

	if (hd->ok) {
		const uint32_t *pix = (const uint32_t *) hd->bgra;
		size_t need = (size_t) hd->w * hd->h;
		if (sub) {
			// the subtitles go into a copy: the decoder keeps its picture
			if (frame32_size < need) {
				free(frame32);
				frame32 = malloc(need * 4);
				frame32_size = frame32 ? need : 0;
			}
			if (frame32) {
				memcpy(frame32, hd->bgra, need * 4);
				sub_draw32(frame32, hd->w, hd->h);
				pix = frame32;
			}
		}
		plat_present_xrgb(pix, hd->w, hd->h, hd->w * 4);
		Shown8 = NULL;
		Shown32 = pix;
		Shown_w = hd->w;
		Shown_h = hd->h;
	} else if (native && m->width <= NATIVE_W && m->height <= NATIVE_H) {
		int x = ((NATIVE_W - m->width) / 2) & ~3, y = (NATIVE_H - m->height) / 2, j;
		memset(screen, 0, sizeof(screen));
		for (j = 0; j < m->height; j++)
			memcpy(screen + (y + j) * NATIVE_W + x, mve_frame(m) + j * m->width, m->width);
		if (sub)
			sub_draw8(screen, NATIVE_W, x, y, m->pal);
		plat_present(screen, NATIVE_W, NATIVE_H, NATIVE_W, m->pal);
		Shown8 = screen;
		Shown32 = NULL;
		Shown_w = NATIVE_W;
		Shown_h = NATIVE_H;
	} else {
		const uint8_t *pix = mve_frame(m);
		if (sub) {
			memcpy(frame8, pix, (size_t) m->width * m->height);
			sub_draw8(frame8, m->width, 0, 0, m->pal);
			pix = frame8;
		}
		plat_present(pix, m->width, m->height, m->width, m->pal);
		Shown8 = pix;
		Shown32 = NULL;
		Shown_w = m->width;
		Shown_h = m->height;
	}
}

int dtumovie_play(const char *name)
{
	char path[600];
	FILE *f;
	uint8_t *data;
	long size;
	mve_decoder m;
	hd_movie hd;
	int r, result = DTUMOVIE_DONE, started = 0, dropped = 0, native;
	uint64_t t0 = 0;

	if (FindArg("-nomovies"))
		return DTUMOVIE_MISSING;
	if (!dtu_find_file(name, path, sizeof(path))) {
		dtu_log("MOVIE: %s not found\n", name);
		return DTUMOVIE_MISSING;
	}
	f = fopen(path, "rb");
	if (!f)
		return DTUMOVIE_MISSING;
	fseek(f, 0, SEEK_END);
	size = ftell(f);
	fseek(f, 0, SEEK_SET);
	data = malloc(size > 0 ? size : 1);
	if (!data || fread(data, 1, size, f) != (size_t) size) {
		fclose(f);
		free(data);
		return DTUMOVIE_MISSING;
	}
	fclose(f);
	if (mve_open(&m, data, size) != 0) {
		dtu_log("MOVIE: %s is not an MVE file\n", path);
		free(data);
		return DTUMOVIE_MISSING;
	}

	native = FindArg("-movienative") != 0;
	hd_open(&hd, name);
	Sub_n = dtu_lang_subtitles(name, &Sub_cues);		// ASCENT: dtulang.h
	Shown8 = NULL;
	Shown32 = NULL;

	// 0xA48D0 in the callers: the music and the sounds stop.
	dtumusic_stop();
	digi_stop_all();
	plat_audio_music_stop();
	Ring_read = Ring_write = 0;
	Ring_dropped = 0;
	Ring_on = 0;
	mve_set_audio_func(&m, ring_push, NULL);
	key_flush();
	mouse_flush();

	while ((r = mve_next_frame(&m)) == 1) {
		uint64_t due, now;
		int k = m.shown - 1;

		if (!started) {
			// Opcode 0x04 of the first video chunk starts the sound. The
			// audio chunks before it fill the buffer, so the sound of a
			// chunk plays about one second after the picture of the same
			// chunk is decoded: the encoder put the sound ahead.
			if (m.audio_present && !Movie_fast) {
				plat_audio_music_set_volume((PLAT_AUDIO_VOLUME_MAX * Config_digi_volume) / 8);
				plat_audio_music_start(ring_callback, NULL, m.audio_rate);
				Ring_on = 1;
			}
			t0 = plat_time_us();
			started = 1;
		}
		if (m.new_picture && hd.video) {
			if (!hd_next(&hd))
				dtu_log("MOVIE: the upscaled movie ends at picture %d of %s\n", hd.frames, name);
		}
		m.new_picture = 0;

		due = t0 + (uint64_t) k * m.frame_us;
		now = plat_time_us();
		if (!Movie_fast && now > due + m.frame_us) {
			dropped++;			// too late: the next frame is due
		} else {
			while (!Movie_fast && (now = plat_time_us()) < due) {
				int ms = (int) ((due - now) / 1000);
				if (!plat_pump() || skip_requested()) {
					result = DTUMOVIE_SKIPPED;
					break;
				}
				plat_sleep_ms(ms > 2 ? 2 : (ms > 0 ? ms : 0));
			}
			if (result == DTUMOVIE_SKIPPED)
				break;
			present(&m, &hd, native, (int) (((uint64_t) k * m.frame_us) / 1000));
		}
		if (!plat_pump() || skip_requested()) {
			result = DTUMOVIE_SKIPPED;
			break;
		}
		if (Movie_frame_limit && m.shown >= Movie_frame_limit)
			break;
	}
	if (r < 0)
		dtu_log("MOVIE: %s: %s\n", name, m.error);
	if (r == 0 && result == DTUMOVIE_DONE && !Movie_fast) {
		// Show the last frame for its full time.
		uint64_t end = t0 + (uint64_t) m.shown * m.frame_us;
		while (plat_time_us() < end && plat_pump())
			plat_sleep_ms(2);
	}

	if (Movie_shot) {
		// ASCENT: the picture that was shown last, with the subtitles
		if (Shown8 || Shown32 ? write_ppm(Movie_shot, Shown8, Shown_w, Shown_h, m.pal, (const uint8_t *) Shown32) :
				write_ppm(Movie_shot, mve_frame(&m), hd.ok ? hd.w : m.width, hd.ok ? hd.h : m.height,
				m.pal, hd.ok ? hd.bgra : NULL))
			printf("Wrote %s (movie %s frame %d%s)\n", Movie_shot, name, m.shown, hd.ok ? ", upscaled" : "");
		Movie_shot = NULL;
	}

	if (Ring_on)
		plat_audio_music_stop();
	Ring_on = 0;
	dtu_log("MOVIE: %s %s: %d frames shown of %d pictures, %d late, %u us per frame, sound %lld frames%s%s\n",
		name, result == DTUMOVIE_SKIPPED ? "skipped" : "done", m.shown, m.video_frames, dropped, m.frame_us,
		(long long) m.audio_frames, Ring_dropped ? " (buffer overflow)" : "", hd.frames ? ", upscaled" : "");
	printf("MOVIE: %s %s: %d frames, %d pictures, %d late, %lld sound frames decoded, %u played, %d upscaled pictures\n",
		name, result == DTUMOVIE_SKIPPED ? "skipped" : "done", m.shown, m.video_frames, dropped,
		(long long) m.audio_frames, (unsigned) __atomic_load_n(&Ring_read, __ATOMIC_ACQUIRE), hd.frames);
	hd_close(&hd);
	mve_close(&m);
	free(data);
	key_flush();
	mouse_flush();
	return result;
}

void dtumovie_title(void)
{
	int t;

	if ((t = FindArg("-playmovie")) != 0 && t + 1 < Num_args) {
		int n = FindArg("-movieframes");
		int s = FindArg("-movieshot");
		if (n && n + 1 < Num_args)
			Movie_frame_limit = atoi(Args[n + 1]);
		if (s && s + 1 < Num_args)
			Movie_shot = Args[s + 1];
		Movie_fast = FindArg("-moviefast") != 0;
		dtumovie_play(Args[t + 1]);
		exit(0);
	}

	// 0xB84F3: unless -notitles. The flag 0x142254 is cleared by a key in
	// a movie (0x9640C), and then the next movies do not play.
	if (FindArg("-notitles"))
		return;
	if (dtumovie_play("LOGO22S.MVE") == DTUMOVIE_SKIPPED)
		return;
	if (dtumovie_play("INTRO2.MVE") == DTUMOVIE_SKIPPED)
		return;
	dtumovie_play("TLOGOS.MVE");
}

void dtumovie_flame(void)
{
	// 0xA9A5B: the sequence runs only with digital sound (0x2EF31). The
	// port plays the movie in each case.
	stop_time();
	dtumovie_play("FLAM1.MVE");
	start_time();
}

int dtumovie_end_credits(void)
{
	// 0xA6A2F: the credits movie, then Function_mode 6 (back to the front
	// end). The front end hook ends the game after this frame.
	stop_time();
	dtumovie_play("CREDITV1.MVE");
	start_time();
	return dtu_hook_player_death_screen();
}
