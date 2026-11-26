/*
 * ASCENT: the link between the 3d library and the GPU renderer of the
 * platform layer (plat_gpu_*, platform.h).
 *
 * New file. It has no original. While the 3D view is drawn on the GPU,
 * the drawers of draw.c and rod.c give the projected polygons to the
 * functions here (G3_gpu, globvars.h). The 3d library still does the
 * rotation, the projection and the 3d clipping, so the GPU gets the same
 * polygons that the texture mapper gets. This file turns the points into
 * GPU vertices, keeps the GPU textures of each bitmap and gives the light
 * and the transparency rules of the texture mapper (ntmap.c, tmapflat.c).
 *
 * The RGB form of the view (platform.h) uses RGBA textures: the bitmap
 * expanded with the palette at once, and, when the upscaled texture set
 * has the bitmap, the file of the set, loaded in the background by the
 * platform layer and put in place of the expanded bitmap when it is ready.
 */
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "globvars.h"
#include "platform.h"

//from texmap.h. The 3d library does not use the texture mapper headers.
#define NUM_LIGHTING_LEVELS 32

//The upscaled texture set: 4 times the size of the original bitmaps.
#define HD_SCALE 4
//The largest width or height of a texture of the set on the GPU.
#define HD_MAX_SIZE 1024
//The RGBA textures not used for HD_IDLE_FRAMES frames are deleted when
//they use more than HD_BUDGET bytes.
#define HD_BUDGET ((int64_t) 768 << 20)
#define HD_IDLE_FRAMES 300

static g3_gpu_options Opt;
static ubyte Opt_palette[768];
static char Opt_hd_dir[600];
static int Rgb_gen = 1;		//changes when the RGB textures must be made again
static int Hd_log;			//ASCENT_GPU_HDLOG=1: print the requests of upscaled textures
int G3_gpu_heat;			//3d.h
int G3_gpu_opaque;		//3d.h
int G3_gpu_fx_row = 25;	//3d.h
int G3_gpu_dim;			//3d.h
int G3_gpu_tint;			//3d.h

// ---------------------------------------------------------------------
// The index of the upscaled set: <hd>/textures/<egg>/index.txt, one line
// "name w h" for each file (the original size).

typedef struct hd_name {
	char name[20];
	short w,h;
} hd_name;

typedef struct hd_index {
	char egg[16];
	int loaded;			//1: index.txt read, -1: no index.txt
	hd_name *names;
	int n;
} hd_index;

#define MAX_HD_INDEX 32
static hd_index Hd_index[MAX_HD_INDEX];
static int N_hd_index;

static int cmp_name(const void *a,const void *b)
{
	return strcmp(((const hd_name *) a)->name,((const hd_name *) b)->name);
}

static void hd_index_clear(void)
{
	int i;

	for (i=0;i<N_hd_index;i++)
		free(Hd_index[i].names);
	memset(Hd_index,0,sizeof(Hd_index));
	N_hd_index = 0;
}

static hd_index *hd_index_get(const char *egg)
{
	hd_index *x;
	char path[700],line[200];
	FILE *f;
	int cap = 0;

	for (x=Hd_index;x<Hd_index+N_hd_index;x++)
		if (!strcmp(x->egg,egg))
			return x;
	if (N_hd_index >= MAX_HD_INDEX)
		return NULL;
	x = &Hd_index[N_hd_index++];
	memset(x,0,sizeof(*x));
	strncpy(x->egg,egg,sizeof(x->egg)-1);
	snprintf(path,sizeof(path),"%s/textures/%s/index.txt",Opt_hd_dir,egg);
	f = fopen(path,"r");
	if (!f) {
		x->loaded = -1;
		return x;
	}
	while (fgets(line,sizeof(line),f)) {
		hd_name hn;
		int w,h;
		memset(&hn,0,sizeof(hn));
		if (sscanf(line,"%19s %d %d",hn.name,&w,&h) != 3)
			continue;
		hn.w = (short) w;
		hn.h = (short) h;
		//the index can give the name of the archive ("flame19#0"): the
		//file name has '_' and lower case
		{
			char *c;
			for (c=hn.name;*c;c++)
				*c = *c == '#' ? '_' : (char) tolower((unsigned char) *c);
		}
		if (x->n >= cap) {
			hd_name *nn;
			cap = cap ? cap*2 : 256;
			nn = (hd_name *) realloc(x->names,(size_t) cap*sizeof(*nn));
			if (!nn)
				break;
			x->names = nn;
		}
		x->names[x->n++] = hn;
	}
	fclose(f);
	if (x->n)
		qsort(x->names,x->n,sizeof(hd_name),cmp_name);
	x->loaded = 1;
	return x;
}

//The path of the file of the set for the bitmap egg/name of size w*h, or
//0 if the set does not have it.
static int hd_path(const char *egg,const char *name,int w,int h,char *path,int size)
{
	hd_index *x;

	if (!Opt_hd_dir[0] || !egg[0] || !name[0])
		return 0;
	x = hd_index_get(egg);
	if (!x)
		return 0;
	snprintf(path,size,"%s/textures/%s/%s.png",Opt_hd_dir,egg,name);
	if (x->loaded == 1) {
		hd_name key,*hn;
		memset(&key,0,sizeof(key));
		strncpy(key.name,name,sizeof(key.name)-1);
		hn = (hd_name *) bsearch(&key,x->names,x->n,sizeof(hd_name),cmp_name);
		return hn && hn->w == w && hn->h == h;
	} else {
		//no index: the file decides
		FILE *f = fopen(path,"rb");
		if (!f)
			return 0;
		fclose(f);
		return 1;
	}
}

// ---------------------------------------------------------------------
// Textures

//One entry for each bitmap data: the GPU textures and a check sum of the
//data. The data at one address can change (the texture merge cache, the
//paging of the bitmaps, the reticle canvas), so the sum is checked the
//first time that a bitmap is used in a frame.
typedef struct tex_entry {
	const ubyte *data;
	short w,h,rowsize;
	ubyte rle;
	uint64_t sum;
	int checked_frame;
	int used_frame;
	int tex;			//8-bit form
	int tex_rgb;		//RGB form
	int rgb_gen;
	ubyte merge;		//RGB form: 1 a bitmap of the cache of texmerge.c, 2 not, 0 not known
} tex_entry;

#define TC_SIZE 4096		//power of 2
#define TC_MAX_USED (TC_SIZE*3/4)

static tex_entry Tc[TC_SIZE];
static int Tc_used;
static int Frame_num = 1;
static ubyte *Decode_buf;
static int Decode_size;
static ubyte *Rgba_buf;
static int Rgba_size;

static unsigned tc_hash(const ubyte *data,int w,int h)
{
	uint64_t k = (uint64_t) (uintptr_t) data;
	k ^= (uint64_t) w << 48 ^ (uint64_t) h << 32;
	k *= 0x9E3779B97F4A7C15ull;
	return (unsigned) (k >> 40) & (TC_SIZE-1);
}

static uint64_t sum_bytes(const ubyte *p,size_t n,uint64_t a)
{
	while (n >= 8) {
		uint64_t v;
		memcpy(&v,p,8);
		a = (a ^ v) * 0x100000001B3ull;
		a ^= a >> 29;
		p += 8;
		n -= 8;
	}
	while (n--)
		a = (a ^ *p++) * 0x100000001B3ull;
	return a;
}

//The size of the RLE data: a 32-bit size, a byte for each row, the rows.
static size_t rle_size(grs_bitmap *bm)
{
	int32_t size;

	memcpy(&size,bm->bm_data,4);
	if (size < 4 + bm->bm_h || size > 4 + bm->bm_h + bm->bm_h * (bm->bm_w * 2 + 2))
		size = 4 + bm->bm_h;		//not valid: check the row sizes only
	return (size_t) size;
}

static uint64_t bitmap_sum(grs_bitmap *bm)
{
	uint64_t a = 0xcbf29ce484222325ull;
	int y;

	if (bm->bm_flags & BM_FLAG_RLE)
		return sum_bytes(bm->bm_data,rle_size(bm),a);
	for (y=0;y<bm->bm_h;y++)
		a = sum_bytes(bm->bm_data + (size_t) y * bm->bm_rowsize,bm->bm_w,a);
	return a;
}

//Expand the bitmap to w*h bytes in Decode_buf.
static ubyte *bitmap_pixels(grs_bitmap *bm)
{
	int w = bm->bm_w, h = bm->bm_h, y;
	int need = w * h;

	if (need > Decode_size) {
		ubyte *nb = (ubyte *) realloc(Decode_buf,need);
		if (!nb)
			return NULL;
		Decode_buf = nb;
		Decode_size = need;
	}

	if (bm->bm_flags & BM_FLAG_RLE) {
		//the format of gr_rle_decode, with a limit on the row length
		const ubyte *row = bm->bm_data + 4 + h;
		const ubyte *end = bm->bm_data + rle_size(bm);

		for (y=0;y<h;y++) {
			const ubyte *s = row;
			ubyte *d = Decode_buf + (size_t) y * w;
			int x = 0;

			memset(d,255,w);
			while (s < end) {
				ubyte c = *s++;
				if ((c & 0xE0) != 0xE0) {
					if (x < w) d[x] = c;
					x++;
				} else {
					int count = c & 0x1F;
					ubyte v;
					if (count == 0 || s >= end)
						break;
					v = *s++;
					while (count--) {
						if (x < w) d[x] = v;
						x++;
					}
				}
			}
			row += bm->bm_data[4+y];
		}
	} else {
		for (y=0;y<h;y++)
			memcpy(Decode_buf + (size_t) y * w,bm->bm_data + (size_t) y * bm->bm_rowsize,w);
	}
	return Decode_buf;
}

static int expand6(int v)
{
	v &= 63;
	return (v << 2) | (v >> 4);
}

//The color of each pixel with alpha 0 becomes the average of its
//neighbours with a color, one ring for each pass, so that a face drawn
//opaque (G3_gpu_opaque) shows the colors around a hole, not black. The
//alpha stays 0. A pass that fills nothing ends the work.
static void fill_holes(ubyte *p,int w,int h)
{
	static ubyte *done;
	static int done_size;
	int pass, x, y, k, changed = 1;

	if (w * h > done_size) {
		ubyte *nd = (ubyte *) realloc(done,(size_t) w * h);
		if (!nd)
			return;
		done = nd;
		done_size = w * h;
	}
	for (k=0;k<w*h;k++)
		done[k] = p[k*4+3] != 0;
	for (pass=0;pass<w+h && changed;pass++) {
		changed = 0;
		for (y=0;y<h;y++)
			for (x=0;x<w;x++) {
				int i = y * w + x, n = 0, r = 0, g = 0, b = 0, d;
				static const int dx[4] = { 1, -1, 0, 0 }, dy[4] = { 0, 0, 1, -1 };

				if (done[i])
					continue;
				for (d=0;d<4;d++) {
					int xx = (x + dx[d] + w) % w, yy = (y + dy[d] + h) % h, j = yy * w + xx;
					if (done[j] == 1) {
						r += p[j*4]; g += p[j*4+1]; b += p[j*4+2]; n++;
					}
				}
				if (n) {
					p[i*4] = (ubyte) (r / n); p[i*4+1] = (ubyte) (g / n); p[i*4+2] = (ubyte) (b / n);
					done[i] = 2;		//filled in this pass: a source from the next pass on
					changed = 1;
				}
			}
		for (k=0;k<w*h;k++)
			if (done[k] == 2)
				done[k] = 1;
	}
}

//The 8-bit pixels expanded with the palette of the view. Color 255 (the
//transparent color) has alpha 0. The result is in Rgba_buf.
static ubyte *expand_rgba(const ubyte *pix,int w,int h)
{
	int i,n = w * h;

	if (n * 4 > Rgba_size) {
		ubyte *nb = (ubyte *) realloc(Rgba_buf,(size_t) n * 4);
		if (!nb)
			return NULL;
		Rgba_buf = nb;
		Rgba_size = n * 4;
	}
	for (i=0;i<n;i++) {
		int c = pix[i];
		Rgba_buf[i*4] = (ubyte) expand6(Opt_palette[c*3]);
		Rgba_buf[i*4+1] = (ubyte) expand6(Opt_palette[c*3+1]);
		Rgba_buf[i*4+2] = (ubyte) expand6(Opt_palette[c*3+2]);
		Rgba_buf[i*4+3] = c == 255 ? 0 : 255;
	}
	fill_holes(Rgba_buf,w,h);
	return Rgba_buf;
}

static void entry_free_textures(tex_entry *e)
{
	plat_gpu_texture_delete(e->tex);
	plat_gpu_texture_delete(e->tex_rgb);
	e->tex = e->tex_rgb = 0;
	e->rgb_gen = 0;
}

//Remove the entries not used for keep_frames frames. The table is made
//again. rgb_only: delete only the RGB textures of those entries.
static void tc_sweep(int keep_frames,int rgb_only)
{
	static tex_entry old[TC_SIZE];
	int i;

	if (rgb_only) {
		for (i=0;i<TC_SIZE;i++)
			if (Tc[i].data && Tc[i].tex_rgb && Frame_num - Tc[i].used_frame > keep_frames) {
				plat_gpu_texture_delete(Tc[i].tex_rgb);
				Tc[i].tex_rgb = 0;
				Tc[i].rgb_gen = 0;
			}
		return;
	}
	memcpy(old,Tc,sizeof(Tc));
	memset(Tc,0,sizeof(Tc));
	Tc_used = 0;
	for (i=0;i<TC_SIZE;i++) {
		tex_entry *e = &old[i];
		unsigned j;

		if (!e->data)
			continue;
		if (Frame_num - e->used_frame > keep_frames) {
			entry_free_textures(e);
			continue;
		}
		j = tc_hash(e->data,e->w,e->h);
		while (Tc[j].data)
			j = (j+1) & (TC_SIZE-1);
		Tc[j] = *e;
		Tc_used++;
	}
}

//A layer of an upscaled texture job: the path of the file (if the set
//has it) and the original pixels. The buffers are allocated (free them).
static int make_layer(grs_bitmap *bm,const g3_gpu_source *src,plat_gpu_hd_layer *l,char *path,int path_size)
{
	ubyte *pix,*rgba;
	int n = bm->bm_w * bm->bm_h,has;

	memset(l,0,sizeof(*l));
	has = src->kind == 1 && hd_path(src->egg,src->name,bm->bm_w,bm->bm_h,path,path_size);
	pix = bitmap_pixels(bm);
	if (!pix)
		return 0;
	l->index = (ubyte *) malloc(n);
	if (!l->index)
		return 0;
	memcpy((ubyte *) l->index,pix,n);
	rgba = expand_rgba(pix,bm->bm_w,bm->bm_h);
	l->rgba = rgba ? (ubyte *) malloc((size_t) n*4) : NULL;
	if (!l->rgba)
		return 0;
	memcpy((ubyte *) l->rgba,rgba,(size_t) n*4);
	l->w = bm->bm_w;
	l->h = bm->bm_h;
	l->path = has ? path : NULL;
	return 1;
}

static void free_layer(plat_gpu_hd_layer *l)
{
	free((void *) l->rgba);
	free((void *) l->index);
	memset(l,0,sizeof(*l));
}

//Ask for the upscaled form of the bitmap (if the set has it).
static void request_hd(grs_bitmap *bm,int tex)
{
	g3_gpu_source src,sb,st;
	plat_gpu_hd_job job;
	char path_b[700],path_t[700];

	if (!Opt.upscaled || !Opt_hd_dir[0] || !Opt.source)
		return;
	memset(&src,0,sizeof(src));
	if (!Opt.source(bm,&src)) {
		if (Hd_log)
			printf("GPU hd: no source for a bitmap of %dx%d\n",bm->bm_w,bm->bm_h);
		return;
	}
	if (Hd_log && src.kind == 1 && !hd_path(src.egg,src.name,bm->bm_w,bm->bm_h,path_b,sizeof(path_b)))
		printf("GPU hd: not in the set: %s/%s %dx%d\n",src.egg,src.name,bm->bm_w,bm->bm_h);
	memset(&job,0,sizeof(job));
	job.scale = HD_SCALE;
	job.max_size = HD_MAX_SIZE;
	if (src.kind == 1) {
		if (!make_layer(bm,&src,&job.base,path_b,sizeof(path_b)) || !job.base.path) {
			free_layer(&job.base);
			return;
		}
		//a wall texture without an overlay with effect colors
		//(Dtu_fix_wall_effects, main/texmerge.c)
		if (bm->bm_flags & BM_FLAG_DTU_EFFECTS) {
			job.fx_index = job.base.index;
			job.fx_w = bm->bm_w;
			job.fx_h = bm->bm_h;
		}
	} else if (src.kind == 2 && src.bottom && src.top) {
		memset(&sb,0,sizeof(sb));
		memset(&st,0,sizeof(st));
		if (!Opt.source(src.bottom,&sb) || !Opt.source(src.top,&st))
			return;
		if (!make_layer(src.bottom,&sb,&job.base,path_b,sizeof(path_b))
				|| !make_layer(src.top,&st,&job.top,path_t,sizeof(path_t))
				|| (!job.base.path && !job.top.path)) {
			free_layer(&job.base);
			free_layer(&job.top);
			return;
		}
		job.orient = src.orient & 3;
		job.super = (src.top->bm_flags & BM_FLAG_SUPER_TRANSPARENT) != 0;
		//the effect colors of the merged bitmap (texmap/tmapfx.c); the
		//loader copies the pixels
		if (bm->bm_flags & BM_FLAG_DTU_EFFECTS) {
			job.fx_index = bitmap_pixels(bm);
			job.fx_w = bm->bm_w;
			job.fx_h = bm->bm_h;
		}
	} else
		return;
	if (Hd_log)
		printf("GPU hd: %s %s/%s%s%s\n",src.kind == 2 ? "merge" : "bitmap",src.kind == 2 ? sb.egg : src.egg,
			src.kind == 2 ? sb.name : src.name,src.kind == 2 ? " + " : "",src.kind == 2 ? st.name : "");
	plat_gpu_texture_load_hd(tex,&job);
	free_layer(&job.base);
	free_layer(&job.top);
}

//The texture entry of a bitmap, with a checked sum, or NULL.
static tex_entry *bitmap_entry(grs_bitmap *bm)
{
	unsigned j;
	tex_entry *e;
	uint64_t sum;
	ubyte rle = (bm->bm_flags & BM_FLAG_RLE) != 0;

	if (!bm->bm_data || bm->bm_w <= 0 || bm->bm_h <= 0 || (bm->bm_flags & BM_FLAG_PAGED_OUT))
		return NULL;

	j = tc_hash(bm->bm_data,bm->bm_w,bm->bm_h);
	for (;;) {
		e = &Tc[j];
		if (!e->data)
			break;
		if (e->data == bm->bm_data && e->w == bm->bm_w && e->h == bm->bm_h
				&& e->rowsize == bm->bm_rowsize && e->rle == rle)
			break;
		j = (j+1) & (TC_SIZE-1);
	}

	if (e->data && e->checked_frame == Frame_num) {
		e->used_frame = Frame_num;
		return e;
	}

	sum = bitmap_sum(bm);
	if (e->data && e->sum == sum) {
		e->checked_frame = e->used_frame = Frame_num;
		return e;
	}

	if (e->data)
		entry_free_textures(e);		//new data at the same address
	else {
		if (Tc_used >= TC_MAX_USED)
			return NULL;		//full until the next sweep
		memset(e,0,sizeof(*e));
		e->data = bm->bm_data;
		e->w = bm->bm_w;
		e->h = bm->bm_h;
		e->rowsize = bm->bm_rowsize;
		e->rle = rle;
		Tc_used++;
	}
	e->sum = sum;
	e->checked_frame = e->used_frame = Frame_num;
	return e;
}

//RGB form: the merged wall textures (texmerge.c) have one texture for each
//pair of bitmaps and rotation. The cache of texmerge.c puts other merges
//into its bitmaps (an animated overlay changes them in each frame of the
//animation), so a texture for the bitmap of the cache would be made again
//each time, and its upscaled form would come too late.
typedef struct merge_entry {
	grs_bitmap *bottom,*top;
	int orient;
	int tex;
	int rgb_gen;
	int used_frame;
} merge_entry;

#define MT_SIZE 1024		//power of 2
static merge_entry Mt[MT_SIZE];
static int Mt_used;

//The first place of a merge in Mt. tc_hash() gives a place in Tc (TC_SIZE
//places): without the mask of MT_SIZE three of four merges were written
//after the end of Mt, into the entries of Tc that follow it in memory, so a
//merged wall could show the texture of another merge, and a bitmap entry
//could get the texture number of a merge.
static unsigned mt_hash(const grs_bitmap *bottom,const grs_bitmap *top,int orient)
{
	return tc_hash((const ubyte *) bottom,orient,(int) (((uintptr_t) top >> 4) & 0xFFFF)) & (MT_SIZE-1);
}

static void mt_sweep(int keep_frames)
{
	static merge_entry old[MT_SIZE];
	int i;

	int before = Mt_used;

	memcpy(old,Mt,sizeof(Mt));
	memset(Mt,0,sizeof(Mt));
	Mt_used = 0;
	for (i=0;i<MT_SIZE;i++) {
		merge_entry *m = &old[i];
		unsigned j;
		if (!m->bottom)
			continue;
		if (Frame_num - m->used_frame > keep_frames) {
			plat_gpu_texture_delete(m->tex);
			continue;
		}
		j = mt_hash(m->bottom,m->top,m->orient);
		while (Mt[j].bottom)
			j = (j+1) & (MT_SIZE-1);
		Mt[j] = *m;
		Mt_used++;
	}
	if (Hd_log)
		printf("GPU hd: merge sweep (%d frames): %d of %d kept\n",keep_frames,Mt_used,before);
}

static int merge_texture(grs_bitmap *bm,const g3_gpu_source *src)
{
	unsigned j = mt_hash(src->bottom,src->top,src->orient);
	merge_entry *m;
	ubyte *pix,*rgba;

	for (;;) {
		m = &Mt[j];
		if (!m->bottom || (m->bottom == src->bottom && m->top == src->top && m->orient == src->orient))
			break;
		j = (j+1) & (MT_SIZE-1);
	}
	if (m->bottom && m->tex && m->rgb_gen == Rgb_gen) {
		m->used_frame = Frame_num;
		return m->tex;
	}
	if (!m->bottom) {
		if (Mt_used >= MT_SIZE*3/4)
			return 0;
		m->bottom = src->bottom;
		m->top = src->top;
		m->orient = src->orient;
		Mt_used++;
	}
	plat_gpu_texture_delete(m->tex);
	m->tex = 0;
	pix = bitmap_pixels(bm);
	rgba = pix ? expand_rgba(pix,bm->bm_w,bm->bm_h) : NULL;
	//the effect colors of a merged bitmap (texmap/tmapfx.c)
	if (rgba && (bm->bm_flags & BM_FLAG_DTU_EFFECTS))
		plat_gpu_fx_mark(rgba,bm->bm_w,bm->bm_h,pix,bm->bm_w,bm->bm_h);
	if (rgba)
		m->tex = plat_gpu_texture_create_rgba(bm->bm_w,bm->bm_h,rgba,0);
	m->rgb_gen = Rgb_gen;
	m->used_frame = Frame_num;
	if (m->tex)
		request_hd(bm,m->tex);
	return m->tex;
}

//The GPU texture of a bitmap for the form of the view, or 0.
static int bitmap_texture(grs_bitmap *bm)
{
	tex_entry *e = bitmap_entry(bm);
	ubyte *pix,*rgba;

	if (!e)
		return 0;
	if (!Opt.rgb) {
		if (!e->tex && (pix = bitmap_pixels(bm)) != NULL)
			e->tex = plat_gpu_texture_create(bm->bm_w,bm->bm_h,pix);
		return e->tex;
	}
	if (e->merge != 2 && Opt.source) {
		g3_gpu_source src;
		memset(&src,0,sizeof(src));
		if (Opt.source(bm,&src) && src.kind == 2 && src.bottom && src.top) {
			e->merge = 1;
			return merge_texture(bm,&src);
		}
		if (!e->merge)
			e->merge = 2;
	}
	if (e->tex_rgb && e->rgb_gen == Rgb_gen)
		return e->tex_rgb;
	plat_gpu_texture_delete(e->tex_rgb);
	e->tex_rgb = 0;
	pix = bitmap_pixels(bm);
	rgba = pix ? expand_rgba(pix,bm->bm_w,bm->bm_h) : NULL;
	if (!rgba)
		return 0;
	//the effect colors of a wall texture without an overlay
	//(Dtu_fix_wall_effects, main/texmerge.c)
	if (bm->bm_flags & BM_FLAG_DTU_EFFECTS)
		plat_gpu_fx_mark(rgba,bm->bm_w,bm->bm_h,pix,bm->bm_w,bm->bm_h);
	e->tex_rgb = plat_gpu_texture_create_rgba(bm->bm_w,bm->bm_h,rgba,0);
	e->rgb_gen = Rgb_gen;
	if (e->tex_rgb)
		request_hd(bm,e->tex_rgb);
	return e->tex_rgb;
}

int g3_gpu_preload(grs_bitmap *bm)
{
	if (!bm || !plat_gpu_available() || !Opt.rgb || !Opt.upscaled)
		return 0;
	return bitmap_texture(bm) != 0;
}

int g3_gpu_texture_generation(void)
{
	return Rgb_gen;
}

// ---------------------------------------------------------------------
// Polygons

//The light of a point as the software mapper sees it. draw_tmap of 1.3
//(0xEA4C4) and of ntmap.c shift the fix light left by 5 (times
//NUM_LIGHTING_LEVELS) in 32 bits, and the scanline selector (0xE906C)
//clamps the result to 0 .. 0x1F8000. A light of 1024 or more wraps: side 4
//of segment 204 of THIEF1 has 0x832FD5E8 (-31952) in the level file, which
//the shift makes 0x65FABD00, the brightest row. The GPU clamps the light
//itself, so only the wrap is done here.
static float gpu_light(fix l)
{
	return (float) (int32_t) ((uint32_t) l << 5) / (65536.0f * NUM_LIGHTING_LEVELS);
}

//A projected point (a clipped polygon of the 3d library): the position
//times the depth (plat_gpu_vertex).
static void make_vertex(plat_gpu_vertex *o,g3s_point *p)
{
	float z = p->z / 65536.0f;

	memset(o,0,sizeof(*o));
	o->x = p->p3_sx / 65536.0f * z;
	o->y = p->p3_sy / 65536.0f * z;
	o->z = z;
	o->u = p->p3_u / 65536.0f;
	o->v = p->p3_v / 65536.0f;
	o->l = gpu_light(p->p3_l);
	o->lq[0] = -1.0f;
}

//A rotated point that need not be projected or in front of the eye. The
//projection of g3_project_point (sx = x * w2 / z + w2, sy = h2 - y * h2 / z)
//times z, which is linear in x, y, z.
static void make_vertex_3d(plat_gpu_vertex *o,g3s_point *p)
{
	float x = p->x / 65536.0f, y = p->y / 65536.0f, z = p->z / 65536.0f;
	float w2 = Canv_w2 / 65536.0f, h2 = Canv_h2 / 65536.0f;

	memset(o,0,sizeof(*o));
	o->x = x * w2 + w2 * z;
	o->y = h2 * z - y * h2;
	o->z = z;
	o->u = p->p3_u / 65536.0f;
	o->v = p->p3_v / 65536.0f;
	o->l = gpu_light(p->p3_l);
	o->lq[0] = -1.0f;
}

static void draw_points(int nv,g3s_point **pointlist,plat_gpu_material *m)
{
	plat_gpu_vertex v[MAX_POINTS_IN_POLY];
	int i;

	if (nv < 3 || nv > MAX_POINTS_IN_POLY)
		return;
	for (i=0;i<nv;i++)
		make_vertex(&v[i],pointlist[i]);
	plat_gpu_draw(v,nv,m);
}

//A face of g3_draw_tmap, not clipped (gpu_tmap). A lit quad gets the
//bilinear light of its corners.
static void draw_face(int nv,g3s_point **pointlist,plat_gpu_material *m)
{
	static const float st[4][2] = { {0,0}, {1,0}, {1,1}, {0,1} };
	plat_gpu_vertex v[MAX_POINTS_IN_POLY];
	int i,k;

	if (nv < 3 || nv > MAX_POINTS_IN_POLY)
		return;
	for (i=0;i<nv;i++)
		make_vertex_3d(&v[i],pointlist[i]);
	if (nv == 4 && m->mode == PLAT_GPU_TEXTURED && m->lit)
		for (i=0;i<4;i++) {
			v[i].s = st[i][0];
			v[i].t = st[i][1];
			for (k=0;k<4;k++)
				v[i].lq[k] = fmaxf(gpu_light(pointlist[k]->p3_l), 0.0f);
		}
	plat_gpu_draw(v,nv,m);
}

//A flat face: the current color, or a darkening of the image under it
//(tmap_scanline_flat in tmapflat.c).
static void flat_material(plat_gpu_material *m,int color)
{
	memset(m,0,sizeof(*m));
	if (Gr_scanline_darkening_level < GR_FADE_LEVELS) {
		m->mode = PLAT_GPU_DARKEN;
		m->color = Gr_scanline_darkening_level < 0 ? 0 : Gr_scanline_darkening_level;
	} else {
		m->mode = PLAT_GPU_FLAT;
		m->color = color & 255;
	}
}

//The color of draw_tmap_flat (tmapflat.c): the average color of the
//bitmap with the average light.
static int average_light_color(int nv,g3s_point **pointlist,grs_bitmap *bm)
{
	fix average_light = 0;
	int i;

	for (i=0;i<nv;i++)
		average_light += pointlist[i]->p3_l;
	average_light = f2i(average_light * NUM_LIGHTING_LEVELS / nv);
	if (average_light < 0)
		average_light = 0;
	else if (average_light > NUM_LIGHTING_LEVELS-1)
		average_light = NUM_LIGHTING_LEVELS-1;
	return gr_fade_table[average_light*256 + bm->avg_color];
}

//The face is not clipped or projected (g3_draw_tmap): the GPU clips it.
//When the 3d library clipped a face at the edges of the view, the new
//corners got the light of the edge, and the fan of triangles of the
//clipped polygon interpolated the light in screen space over other
//triangles than those of the face. A large floor or ceiling face with one
//bright corner (a torch) then showed light and dark areas that moved when
//the view turned. Here the triangles are those of the face, the light is
//interpolated with perspective, and a quad gets the bilinear light of its
//corners, so the light of a point of a face does not depend on the view.
static void gpu_tmap(int nv,g3s_point **pointlist,grs_bitmap *bm,int special)
{
	plat_gpu_material m;
	int tex;

	//special: the only special tmap drawer of the game is draw_tmap_flat
	//(the cloaked objects, object.c)
	tex = special ? 0 : bitmap_texture(bm);
	if (!tex) {
		flat_material(&m,average_light_color(nv,pointlist,bm));
		m.heat = G3_gpu_heat;
		draw_face(nv,pointlist,&m);
		return;
	}

	memset(&m,0,sizeof(m));
	m.mode = PLAT_GPU_TEXTURED;
	m.texture = tex;
	m.lit = !(bm->bm_flags & BM_FLAG_NO_LIGHTING);
	m.transparent = (bm->bm_flags & BM_FLAG_TRANSPARENT) != 0 && !G3_gpu_opaque;
	m.heat = G3_gpu_heat;
	//the effect colors of a merged bitmap take the colors of the image
	//under the face (texmap/tmapfx.c); the lit mappers of the original do it
	m.effects = m.lit && (bm->bm_flags & BM_FLAG_DTU_EFFECTS) != 0;
	m.fx_row = G3_gpu_fx_row;
	//the dark view of the conversation window: every lit texture, the
	//walls and the models (texmap/tmapfx.c, Tmap_dim)
	m.dim = m.lit ? G3_gpu_dim : 0;
	//the tint of Detect Magic (texmap/tmapfx.c, Tmap_tint): lit textures
	m.tint = m.lit ? G3_gpu_tint : 0;
	//ASCENT: a texture with no light gives light itself (a flame, a lamp)
	m.no_ao = !m.lit;
	draw_face(nv,pointlist,&m);
}

static void gpu_poly(int nv,g3s_point **pointlist)
{
	plat_gpu_material m;

	flat_material(&m,grd_curcanv->cv_color);
	m.heat = G3_gpu_heat;
	draw_points(nv,pointlist,&m);
}

//g3_draw_bitmap: a rectangle at the depth of the point. scale_bitmap draws
//the bitmap with no light, and color 255 is transparent.
static void gpu_bitmap(g3s_point *pnt,fix w,fix h,grs_bitmap *bm)
{
	plat_gpu_material m;
	plat_gpu_vertex v[4];
	float x0 = (pnt->p3_sx - w) / 65536.0f, x1 = (pnt->p3_sx + w) / 65536.0f;
	float y0 = (pnt->p3_sy - h) / 65536.0f, y1 = (pnt->p3_sy + h) / 65536.0f;
	float z = pnt->z / 65536.0f;
	int i;

	memset(&m,0,sizeof(m));
	m.mode = PLAT_GPU_TEXTURED;
	m.texture = bitmap_texture(bm);
	m.transparent = 1;
	m.clamp = 1;
	m.heat = G3_gpu_heat;
	m.no_ao = 1;		//ASCENT: a flat picture at one depth
	if (!m.texture)
		return;
	memset(v,0,sizeof(v));
	for (i=0;i<4;i++) {
		v[i].z = z;
		v[i].l = 1.0f;
		v[i].lq[0] = -1.0f;
	}
	v[0].x = x0; v[0].y = y0; v[0].u = 0; v[0].v = 0;
	v[1].x = x1; v[1].y = y0; v[1].u = 1; v[1].v = 0;
	v[2].x = x1; v[2].y = y1; v[2].u = 1; v[2].v = 1;
	v[3].x = x0; v[3].y = y1; v[3].u = 0; v[3].v = 1;
	for (i=0;i<4;i++) {		// homogeneous (plat_gpu_vertex)
		v[i].x *= z;
		v[i].y *= z;
	}
	plat_gpu_draw(v,4,&m);
}

//g3_draw_sphere: gr_disk with the current color, at the depth of the point
static void gpu_sphere(g3s_point *pnt,fix r)
{
	plat_gpu_material m;
	plat_gpu_vertex v[24];
	float cx = pnt->p3_sx / 65536.0f, cy = pnt->p3_sy / 65536.0f, rad = r / 65536.0f;
	int i;

	if (rad <= 0)
		return;
	memset(&m,0,sizeof(m));
	m.mode = PLAT_GPU_FLAT;
	m.color = grd_curcanv->cv_color & 255;
	m.heat = G3_gpu_heat;
	m.no_ao = 1;		//ASCENT: a flat disk at one depth
	for (i=0;i<24;i++) {
		float a = i * (6.2831853f / 24), z = pnt->z / 65536.0f;
		memset(&v[i],0,sizeof(v[i]));
		v[i].x = (cx + rad * cosf(a)) * z;		// homogeneous (plat_gpu_vertex)
		v[i].y = (cy + rad * sinf(a)) * z;
		v[i].z = z;
		v[i].l = 1.0f;
		v[i].lq[0] = -1.0f;
	}
	plat_gpu_draw(v,24,&m);
}

static g3_gpu_hooks Hooks = { NULL, gpu_tmap, gpu_poly, gpu_bitmap, gpu_sphere };

// ---------------------------------------------------------------------
// The view

static int Last_view_gpu;

void g3_gpu_set_options(const g3_gpu_options *o)
{
	const char *dir = (o->hd_dir && o->upscaled) ? o->hd_dir : "";
	int upscaled = o->upscaled && dir[0];

	//The RGB textures depend on the palette, the upscaled setting and the
	//directory of the set.
	if (upscaled != (Opt.upscaled && Opt_hd_dir[0]) || strcmp(dir,Opt_hd_dir)
			|| (o->palette && memcmp(o->palette,Opt_palette,768))) {
		Rgb_gen++;
		if (strcmp(dir,Opt_hd_dir))
			hd_index_clear();
	}
	Opt = *o;
	Opt.upscaled = upscaled;
	{
		static int done;
		if (!done) {
			const char *e = getenv("ASCENT_GPU_HDLOG");
			Hd_log = (e && e[0] == '1');
			done = 1;
		}
	}
	snprintf(Opt_hd_dir,sizeof(Opt_hd_dir),"%s",dir);
	if (o->palette)
		memcpy(Opt_palette,o->palette,768);
}

int g3_gpu_begin_view(int enable,int scale,int line_mul,int depth_test,int clear_color)
{
	grs_bitmap *cb = &grd_curcanv->cv_bitmap;
	int y,flags;

	G3_gpu = NULL;
	Last_view_gpu = 0;
	if (!enable || !plat_gpu_available() || !cb->bm_data || cb->bm_type != BM_LINEAR
			|| cb->bm_w <= 0 || cb->bm_h <= 0)
		return 0;

	Frame_num++;
	if (Tc_used >= TC_MAX_USED - 256)
		tc_sweep(Tc_used >= TC_MAX_USED - 16 ? 0 : 60,0);
	if (Mt_used >= MT_SIZE*3/4 - 32)
		mt_sweep(60);
	if (plat_gpu_texture_bytes() > HD_BUDGET) {
		tc_sweep(HD_IDLE_FRAMES,1);
		mt_sweep(HD_IDLE_FRAMES);
	}

	plat_gpu_set_fade_table(gr_fade_table,GR_FADE_LEVELS);
	plat_gpu_set_view_palette(Opt_palette);

	//the canvas shows the GPU image where it has the transparent color
	for (y=0;y<cb->bm_h;y++)
		memset(cb->bm_data + (size_t) y * cb->bm_rowsize,255,cb->bm_w);

	if (scale < 0) scale = 1;
	if (line_mul < 1) line_mul = 1;
	flags = (depth_test ? PLAT_GPU_VIEW_DEPTH : 0) | (Opt.rgb ? PLAT_GPU_VIEW_RGB : 0)
		| (Opt.rgb && Opt.anisotropic ? PLAT_GPU_VIEW_ANISO : 0)
		| (Opt.rgb && Opt.ssao ? PLAT_GPU_VIEW_SSAO : 0);
	//ASCENT: the ambient occlusion finds the points of the level from the
	//pixels: the scale of the view matrix (g3_set_view_matrix)
	plat_gpu_set_view_scale(Matrix_scale.x / 65536.0f,Matrix_scale.y / 65536.0f,Matrix_scale.z / 65536.0f);
	//ASCENT: scale 0 is half the shown size in each direction (the view of
	//the 640x480 interface at the pixel size of the 320x200 one)
	if (scale == 0) {
		int w = (cb->bm_w + 1) / 2, h = (cb->bm_h * line_mul + 1) / 2;
		plat_gpu_begin_view(cb->bm_w,cb->bm_h,w > 0 ? w : 1,h > 0 ? h : 1,flags,clear_color);
	} else {
		//scale 1 is the original resolution: the size of the canvas
		if (scale == 1)
			line_mul = 1;
		plat_gpu_begin_view(cb->bm_w,cb->bm_h,cb->bm_w*scale,cb->bm_h*line_mul*scale,flags,clear_color);
	}

	Hooks.canvas = cb->bm_data;
	G3_gpu = &Hooks;
	Last_view_gpu = 1;
	return 1;
}

void g3_gpu_clear_depth(void)
{
	if (G3_gpu)
		plat_gpu_clear_depth();
}

void g3_gpu_end_view(void)
{
	if (G3_gpu) {
		plat_gpu_end_view();
		G3_gpu = NULL;
	}
}

void g3_gpu_view_dest(int x,int y,int w,int h)
{
	if (Last_view_gpu)
		plat_gpu_set_view_dest(x,y,w,h);
	else
		plat_gpu_set_view_dest(0,0,0,0);
}

void g3_gpu_view_darken(int x0,int y0,int x1,int y1,int level)
{
	if (Last_view_gpu)
		plat_gpu_view_darken(x0,y0,x1,y1,level);
}
