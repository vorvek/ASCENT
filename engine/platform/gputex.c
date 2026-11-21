/*
 * ASCENT platform layer: the loader of the upscaled textures for the GPU
 * renderer (gpu.c).
 *
 * New file. It has no original. Two worker threads decode the PNG files
 * of the upscaled texture set (stb_image, public domain), put a second
 * layer over the first (the merged wall textures of texmerge.c), reduce
 * an image that is too large and fill the color of the transparent pixels
 * (see gputex_bleed). gpu.c takes the results in the main thread and
 * uploads them. No OpenGL call is made here.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "platform.h"
#include "plat_video.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO_WARNINGS
#define STBI_FAILURE_USERMSG
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wtype-limits"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif
#include "../third_party/stb/stb_image.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// ---------------------------------------------------------------------
// Image work (any thread)

// A pixel whose color fill_colors fills: alpha 0 (fx 0), or an effect
// texel (fx 1, plat_gpu_fx_mark).
static int fill_target(uint8_t a, int fx)
{
	return fx ? (a != 0 && a != 255) : a == 0;
}

// Fill the color of the target pixels from the other pixels next to them
// (an effect texel is not a source for the pixels with alpha 0 either:
// fx 0 runs after the effect texels got their colors). The pixels that
// no pass reaches get the average color of the image.
static void fill_colors(uint8_t *rgba, int w, int h, int fx)
{
	uint8_t *done;
	int x, y, pass, any = 0, left;
	int64_t sum[3] = { 0, 0, 0 }, n = 0;

	for (y = 0; y < w * h; y++)
		if (fill_target(rgba[y * 4 + 3], fx)) {
			any = 1;
			break;
		}
	if (!any)
		return;
	done = (uint8_t *) malloc((size_t) w * h);
	if (!done)
		return;
	for (y = 0; y < w * h; y++) {
		done[y] = !fill_target(rgba[y * 4 + 3], fx) && (!fx || rgba[y * 4 + 3] != 0);
		if (done[y]) {
			sum[0] += rgba[y * 4];
			sum[1] += rgba[y * 4 + 1];
			sum[2] += rgba[y * 4 + 2];
			n++;
		}
	}
	for (pass = 0; pass < 16; pass++) {
		left = 0;
		for (y = 0; y < h; y++)
			for (x = 0; x < w; x++) {
				int i = y * w + x, dx, dy, c = 0, r = 0, g = 0, b = 0;
				if (done[i])
					continue;
				for (dy = -1; dy <= 1; dy++)
					for (dx = -1; dx <= 1; dx++) {
						int xx = x + dx, yy = y + dy, j;
						if ((dx | dy) == 0 || xx < 0 || yy < 0 || xx >= w || yy >= h)
							continue;
						j = yy * w + xx;
						if (done[j] == 1) {
							r += rgba[j * 4];
							g += rgba[j * 4 + 1];
							b += rgba[j * 4 + 2];
							c++;
						}
					}
				if (c) {
					rgba[i * 4] = (uint8_t) (r / c);
					rgba[i * 4 + 1] = (uint8_t) (g / c);
					rgba[i * 4 + 2] = (uint8_t) (b / c);
					done[i] = 2;		// filled in this pass
				} else
					left++;
			}
		for (y = 0; y < w * h; y++)
			if (done[y] == 2)
				done[y] = 1;
		if (!left)
			break;
	}
	if (n)
		for (y = 0; y < w * h; y++)
			if (!done[y]) {
				rgba[y * 4] = (uint8_t) (sum[0] / n);
				rgba[y * 4 + 1] = (uint8_t) (sum[1] / n);
				rgba[y * 4 + 2] = (uint8_t) (sum[2] / n);
			}
	free(done);
}

// Fill the color of the pixels with alpha 0 from the pixels next to them,
// so that the linear filter and the mipmaps do not mix the color of the
// transparent pixels (often a bright key color) into the edges.
void gputex_bleed(uint8_t *rgba, int w, int h)
{
	fill_colors(rgba, w, h, 0);
}

int plat_gpu_fx_code(int c)
{
	switch (c) {
		case 0xCF: return 1;
		case 0xBF: return 2;
		case 0xBE: return 3;
		case 0x6F: return 4;
		case 0x6E: return 5;
		case 0x6D: return 6;
		case 0x4F: return 7;
		case 0x4E: return 8;
	}
	return 0;
}

void plat_gpu_fx_mark(uint8_t *rgba, int w, int h, const uint8_t *index, int iw, int ih)
{
	int x, y, any = 0;

	if (!rgba || !index || w <= 0 || h <= 0 || iw <= 0 || ih <= 0)
		return;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++) {
			int k = plat_gpu_fx_code(index[(y * ih / h) * iw + x * iw / w]);
			uint8_t *p = rgba + ((size_t) y * w + x) * 4;
			if (k && p[3]) {
				p[3] = (uint8_t) (255 - 8 * k);
				any = 1;
			}
		}
	if (any)
		fill_colors(rgba, w, h, 1);
}

// The upscaled interface pictures of gpu.c. stb_image memory comes from
// malloc (STBI_MALLOC default).
uint8_t *gputex_load_png(const char *path, int *w, int *h)
{
	int n = 0;

	*w = *h = 0;
	return path ? stbi_load(path, w, h, &n, 4) : NULL;
}

// A layer at scale times the original size: the PNG file if it is valid,
// else the original pixels repeated. The alpha is 0 where the original
// pixel has alpha 0. *from_file is 1 for the file.
static uint8_t *load_layer(const plat_gpu_hd_layer *l, int scale, int *from_file)
{
	int w = l->w * scale, h = l->h * scale, x, y, fw = 0, fh = 0, n = 0;
	uint8_t *img = NULL;

	*from_file = 0;
	if (l->path) {
		img = stbi_load(l->path, &fw, &fh, &n, 4);
		if (img && (fw != w || fh != h)) {
			fprintf(stderr, "GPU: %s is %dx%d, not %dx%d\n", l->path, fw, fh, w, h);
			stbi_image_free(img);
			img = NULL;
		}
		if (img) {
			// stb_image memory comes from malloc (STBI_MALLOC default)
			*from_file = 1;
			for (y = 0; y < h; y++)
				for (x = 0; x < w; x++) {
					int o = (y / scale) * l->w + x / scale;
					if (l->rgba[o * 4 + 3] == 0)
						img[(y * w + x) * 4 + 3] = 0;
					// an effect color keeps its palette color, as in the
					// original bitmap (a texture that is not merged shows
					// it; plat_gpu_fx_mark replaces it in a merged one)
					else if (l->index && plat_gpu_fx_code(l->index[o]))
						memcpy(img + (y * w + x) * 4, l->rgba + o * 4, 3);
				}
			return img;
		}
	}
	img = (uint8_t *) malloc((size_t) w * h * 4);
	if (!img)
		return NULL;
	for (y = 0; y < h; y++)
		for (x = 0; x < w; x++)
			memcpy(img + (y * w + x) * 4, l->rgba + ((y / scale) * l->w + x / scale) * 4, 4);
	return img;
}

// The top layer over the base (merge_textures_new and
// merge_textures_super_xparent of texmerge.c, at scale times the size).
static void merge_top(uint8_t *base, const uint8_t *top, const plat_gpu_hd_layer *tl, int size, int scale, int orient, int super)
{
	int x, y;

	for (y = 0; y < size; y++)
		for (x = 0; x < size; x++) {
			int tr, tc, oi;
			switch (orient & 3) {
			case 0: tr = y; tc = x; break;
			case 1: tr = x; tc = size - 1 - y; break;
			case 2: tr = size - 1 - y; tc = size - 1 - x; break;
			default: tr = size - 1 - x; tc = y; break;
			}
			oi = (tr / scale) * tl->w + tc / scale;
			if (tl->rgba[oi * 4 + 3] == 0)
				continue;			// color 255: the base shows
			if (super && tl->index && tl->index[oi] == 254) {
				base[(y * size + x) * 4 + 3] = 0;		// a hole
				continue;
			}
			memcpy(base + (y * size + x) * 4, top + (tr * size + tc) * 4, 3);
			base[(y * size + x) * 4 + 3] = 255;
		}
}

// Halve the image (a box filter). The alpha is 0 if two or more of the four
// pixels have alpha 0, and the color is the average of the others.
static uint8_t *halve(uint8_t *img, int *w, int *h)
{
	int nw = *w / 2, nh = *h / 2, x, y, k;
	uint8_t *o;

	if (nw < 1 || nh < 1)
		return img;
	o = (uint8_t *) malloc((size_t) nw * nh * 4);
	if (!o)
		return img;
	for (y = 0; y < nh; y++)
		for (x = 0; x < nw; x++) {
			const uint8_t *p[4];
			int c[3] = { 0, 0, 0 }, n = 0, op;
			p[0] = img + ((2 * y) * *w + 2 * x) * 4;
			p[1] = p[0] + 4;
			p[2] = p[0] + *w * 4;
			p[3] = p[2] + 4;
			for (k = 0; k < 4; k++)
				if (p[k][3]) {
					c[0] += p[k][0];
					c[1] += p[k][1];
					c[2] += p[k][2];
					n++;
				}
			op = n;
			if (!n)
				n = 1;
			o[(y * nw + x) * 4] = (uint8_t) (c[0] / n);
			o[(y * nw + x) * 4 + 1] = (uint8_t) (c[1] / n);
			o[(y * nw + x) * 4 + 2] = (uint8_t) (c[2] / n);
			o[(y * nw + x) * 4 + 3] = op >= 3 ? 255 : 0;
		}
	free(img);
	*w = nw;
	*h = nh;
	return o;
}

// ---------------------------------------------------------------------
// The queue

typedef struct job_item {
	int texture;
	unsigned serial;
	plat_gpu_hd_job job;
	struct job_item *next;
} job_item;

#define N_WORKERS 2

static SDL_Mutex *Lock;
static SDL_Condition *Wake;
static SDL_Thread *Workers[N_WORKERS];
static job_item *Jobs, *Jobs_tail;
static gputex_result *Done, *Done_tail;
static int Pending;
static int Quit;

static char *dup_str(const char *s)
{
	char *d;

	if (!s)
		return NULL;
	d = (char *) malloc(strlen(s) + 1);
	if (d)
		strcpy(d, s);
	return d;
}

static void *dup_mem(const void *s, size_t n)
{
	void *d;

	if (!s)
		return NULL;
	d = malloc(n);
	if (d)
		memcpy(d, s, n);
	return d;
}

static void free_layer(plat_gpu_hd_layer *l)
{
	free((void *) l->path);
	free((void *) l->rgba);
	free((void *) l->index);
	memset(l, 0, sizeof(*l));
}

static int copy_layer(plat_gpu_hd_layer *d, const plat_gpu_hd_layer *s)
{
	size_t n = (size_t) s->w * s->h;

	memset(d, 0, sizeof(*d));
	if (!s->rgba)
		return 1;
	d->w = s->w;
	d->h = s->h;
	d->path = dup_str(s->path);
	d->rgba = (const uint8_t *) dup_mem(s->rgba, n * 4);
	d->index = (const uint8_t *) dup_mem(s->index, n);
	return d->rgba != NULL;
}

static gputex_result *run_job(job_item *it)
{
	plat_gpu_hd_job *j = &it->job;
	int scale = j->scale > 0 ? j->scale : 1, from_base = 0, from_top = 0;
	int w = j->base.w * scale, h = j->base.h * scale;
	uint8_t *img = load_layer(&j->base, scale, &from_base);
	gputex_result *r;

	if (!img)
		return NULL;
	if (j->top.rgba && j->top.w == j->base.w && j->top.h == j->base.h && j->base.w == j->base.h) {
		uint8_t *top = load_layer(&j->top, scale, &from_top);
		if (top) {
			merge_top(img, top, &j->top, w, scale, j->orient, j->super);
			free(top);
		}
	}
	if (!from_base && !from_top) {
		free(img);
		return NULL;
	}
	while (j->max_size > 0 && (w > j->max_size || h > j->max_size) && w > 1 && h > 1)
		img = halve(img, &w, &h);
	plat_gpu_fx_mark(img, w, h, j->fx_index, j->fx_w, j->fx_h);
	gputex_bleed(img, w, h);

	r = (gputex_result *) calloc(1, sizeof(*r));
	if (!r) {
		free(img);
		return NULL;
	}
	r->texture = it->texture;
	r->serial = it->serial;
	r->w = w;
	r->h = h;
	r->rgba = img;
	return r;
}

static int worker(void *arg)
{
	(void) arg;
	SDL_LockMutex(Lock);
	for (;;) {
		job_item *it;
		gputex_result *r;

		while (!Jobs && !Quit)
			SDL_WaitCondition(Wake, Lock);
		if (Quit)
			break;
		it = Jobs;
		Jobs = it->next;
		if (!Jobs)
			Jobs_tail = NULL;
		SDL_UnlockMutex(Lock);

		r = run_job(it);
		free_layer(&it->job.base);
		free_layer(&it->job.top);
		free((void *) it->job.fx_index);
		free(it);

		SDL_LockMutex(Lock);
		Pending--;
		if (r) {
			if (Done_tail)
				Done_tail->next = r;
			else
				Done = r;
			Done_tail = r;
		}
	}
	SDL_UnlockMutex(Lock);
	return 0;
}

int gputex_start(void)
{
	int i;

	if (Lock)
		return 0;
	Lock = SDL_CreateMutex();
	Wake = SDL_CreateCondition();
	if (!Lock || !Wake)
		return -1;
	Quit = 0;
	for (i = 0; i < N_WORKERS; i++)
		Workers[i] = SDL_CreateThread(worker, "gputex", NULL);
	return 0;
}

void gputex_stop(void)
{
	int i;

	if (!Lock)
		return;
	SDL_LockMutex(Lock);
	Quit = 1;
	SDL_BroadcastCondition(Wake);
	SDL_UnlockMutex(Lock);
	for (i = 0; i < N_WORKERS; i++)
		if (Workers[i])
			SDL_WaitThread(Workers[i], NULL);
	while (Jobs) {
		job_item *it = Jobs;
		Jobs = it->next;
		free_layer(&it->job.base);
		free_layer(&it->job.top);
		free((void *) it->job.fx_index);
		free(it);
	}
	while (Done) {
		gputex_result *r = Done;
		Done = r->next;
		gputex_free(r);
	}
	Jobs_tail = NULL;
	Done_tail = NULL;
	Pending = 0;
	SDL_DestroyCondition(Wake);
	SDL_DestroyMutex(Lock);
	Wake = NULL;
	Lock = NULL;
}

void gputex_queue(int texture, unsigned serial, const plat_gpu_hd_job *job)
{
	job_item *it;

	if (!Lock || !job->base.rgba)
		return;
	it = (job_item *) calloc(1, sizeof(*it));
	if (!it)
		return;
	it->texture = texture;
	it->serial = serial;
	it->job = *job;
	it->job.fx_index = NULL;
	if (job->fx_index && job->fx_w > 0 && job->fx_h > 0)
		it->job.fx_index = (const uint8_t *) dup_mem(job->fx_index, (size_t) job->fx_w * job->fx_h);
	if (!copy_layer(&it->job.base, &job->base) || !copy_layer(&it->job.top, &job->top)) {
		free_layer(&it->job.base);
		free_layer(&it->job.top);
		free((void *) it->job.fx_index);
		free(it);
		return;
	}
	SDL_LockMutex(Lock);
	if (Jobs_tail)
		Jobs_tail->next = it;
	else
		Jobs = it;
	Jobs_tail = it;
	Pending++;
	SDL_SignalCondition(Wake);
	SDL_UnlockMutex(Lock);
}

gputex_result *gputex_take(void)
{
	gputex_result *r;

	if (!Lock)
		return NULL;
	SDL_LockMutex(Lock);
	r = Done;
	if (r) {
		Done = r->next;
		if (!Done)
			Done_tail = NULL;
		r->next = NULL;
	}
	SDL_UnlockMutex(Lock);
	return r;
}

int gputex_pending(void)
{
	int n;

	if (!Lock)
		return 0;
	SDL_LockMutex(Lock);
	n = Pending + (Done ? 1 : 0);
	SDL_UnlockMutex(Lock);
	return n;
}

void gputex_free(gputex_result *r)
{
	if (!r)
		return;
	free(r->rgba);
	free(r);
}
