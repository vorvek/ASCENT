/*
 * ASCENT: command line check of the Descent to Undermountain data layer
 * (dtufile.c, piggy.c, bmread.c, polyobj.c, acm.c, text.c, gamefont.c).
 * New file. It needs the game data, so it is not a registered test.
 *
 *   test_dtudata [options] DATADIR [DATADIR ...]
 *
 *   -level NAME    level table to load (default ypinn)
 *   -player NAME   player character archive (default hmc, "none" for none)
 *   -out DIR       write PNG pictures and one WAV file to DIR
 *   -sweep DIR     load every NAME.BIN of DIR (with its EGG) and print one
 *                  line for each. DIR must also be a data directory.
 *   -texcheck DIR  list the model texture references of every table of DIR
 *                  (with each player archive) that give no bitmap or a
 *                  black, empty or transparent bitmap. DIR must also be a
 *                  data directory.
 *   -texorig       with -texcheck: also list what the header slot rule of
 *                  DTUGAME.EXE gives for weapon, spell and arms models
 *   -lights DIR    list the textures of every table of DIR that have a
 *                  light value (TmapInfo[].lighting). DIR must also be a
 *                  data directory.
 *   -v             print the messages of the loaders (stderr)
 *
 * The data directories are searched in the order given.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#include "types.h"
#include "mem.h"
#include "inferno.h"
#include "gr.h"
#include "rle.h"
#include "mono.h"
#include "error.h"
#include "3d.h"
#include "bm.h"
#include "piggy.h"
#include "textures.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "wall.h"
#include "polyobj.h"
#include "robot.h"
#include "weapon.h"
#include "player.h"
#include "sounds.h"
#include "text.h"
#include "gamefont.h"
#include "dtufile.h"
#include "acm.h"

static char Out_dir[400] = "";

// ---------------------------------------------------------------------
// A PNG writer without compression (stored deflate blocks).

static unsigned crc_table[256];

static void crc_init(void)
{
	unsigned c;
	int n, k;
	for (n=0; n<256; n++) {
		c = n;
		for (k=0; k<8; k++)
			c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		crc_table[n] = c;
	}
}

static unsigned crc_update(unsigned c, const ubyte *p, int n)
{
	while (n--)
		c = crc_table[(c ^ *p++) & 255] ^ (c >> 8);
	return c;
}

static void put32(ubyte *p, unsigned v)
{
	p[0] = v>>24; p[1] = v>>16; p[2] = v>>8; p[3] = v;
}

static void png_chunk(FILE *fp, const char *tag, const ubyte *data, int len)
{
	ubyte b[8];
	unsigned c;

	put32(b, len);
	memcpy(b+4, tag, 4);
	fwrite(b, 1, 8, fp);
	if (len) fwrite(data, 1, len, fp);
	c = crc_update(0xFFFFFFFFu, b+4, 4);
	c = crc_update(c, data, len) ^ 0xFFFFFFFFu;
	put32(b, c);
	fwrite(b, 1, 4, fp);
}

// rgb: w*h*3 bytes
static void write_png(const char *name, const ubyte *rgb, int w, int h)
{
	static const ubyte sig[8] = { 137,80,78,71,13,10,26,10 };
	char path[600];
	FILE *fp;
	ubyte hdr[13];
	ubyte *raw, *z;
	int raw_len, y, pos, zlen, n;
	unsigned a = 1, b = 0;

	if (!Out_dir[0]) return;
	sprintf(path, "%s/%s", Out_dir, name);
	fp = fopen(path, "wb");
	if (!fp) { printf("cannot write %s\n", path); return; }

	raw_len = (w*3+1)*h;
	raw = malloc(raw_len);
	for (y=0; y<h; y++) {
		raw[y*(w*3+1)] = 0;
		memcpy(raw + y*(w*3+1) + 1, rgb + y*w*3, w*3);
	}
	for (n=0; n<raw_len; n++) { a = (a + raw[n]) % 65521; b = (b + a) % 65521; }

	z = malloc(raw_len + raw_len/65535*5 + 16);
	zlen = 0;
	z[zlen++] = 0x78; z[zlen++] = 0x01;
	for (pos=0; pos<raw_len; pos+=n) {
		n = raw_len - pos;
		if (n > 65535) n = 65535;
		z[zlen++] = (pos+n >= raw_len) ? 1 : 0;
		z[zlen++] = n & 255; z[zlen++] = n >> 8;
		z[zlen++] = (~n) & 255; z[zlen++] = ((~n) >> 8) & 255;
		memcpy(z+zlen, raw+pos, n);
		zlen += n;
	}
	put32(z+zlen, (b<<16)|a); zlen += 4;

	fwrite(sig, 1, 8, fp);
	put32(hdr, w); put32(hdr+4, h);
	hdr[8] = 8; hdr[9] = 2; hdr[10] = 0; hdr[11] = 0; hdr[12] = 0;
	png_chunk(fp, "IHDR", hdr, 13);
	png_chunk(fp, "IDAT", z, zlen);
	png_chunk(fp, "IEND", hdr, 0);
	fclose(fp);
	free(raw);
	free(z);
	printf("wrote %s (%d x %d)\n", path, w, h);
}

// Expand a bitmap of the game to 8-bit pixels with the 2d library.
static ubyte *bitmap_pixels(grs_bitmap *bm)
{
	ubyte *pix = malloc(bm->bm_w * bm->bm_h + 600);
	int y;

	if (bm->bm_flags & BM_FLAG_RLE) {
		ubyte *sizes = bm->bm_data + 4;
		ubyte *row = bm->bm_data + 4 + bm->bm_h;
		for (y=0; y<bm->bm_h; y++) {
			gr_rle_decode(row, pix + y*bm->bm_w);
			row += sizes[y];
		}
	} else
		memcpy(pix, bm->bm_data, bm->bm_w * bm->bm_h);
	return pix;
}

// Write a bitmap with the game palette, each pixel "scale" times larger.
// Color 255 (transparent) is drawn magenta.
static void write_bitmap_png(const char *name, grs_bitmap *bm, int scale)
{
	int w = bm->bm_w, h = bm->bm_h, x, y;
	ubyte *pix = bitmap_pixels(bm);
	ubyte *rgb = malloc(w*scale*h*scale*3);

	for (y=0; y<h*scale; y++)
		for (x=0; x<w*scale; x++) {
			int c = pix[(y/scale)*w + x/scale];
			ubyte *d = rgb + (y*w*scale + x)*3;
			if (c == 255 && (bm->bm_flags & BM_FLAG_TRANSPARENT)) {
				d[0] = 255; d[1] = 0; d[2] = 255;
			} else {
				d[0] = gr_palette[c*3]*4;
				d[1] = gr_palette[c*3+1]*4;
				d[2] = gr_palette[c*3+2]*4;
			}
		}
	write_png(name, rgb, w*scale, h*scale);
	free(rgb);
	free(pix);
}

static void write_named_bitmap(char *bitmap_name, int scale)
{
	bitmap_index bi = piggy_find_bitmap(bitmap_name);
	char file[80], *p;

	if (!bi.index) {
		printf("bitmap %s: not found\n", bitmap_name);
		return;
	}
	sprintf(file, "bm_%s.png", bitmap_name);
	for (p=file; *p; p++) if (*p == '#') *p = '_';
	printf("bitmap %s: index %d, %d x %d, flags %d, disk flags 0x%02x\n", bitmap_name, bi.index,
		GameBitmaps[bi.index].bm_w, GameBitmaps[bi.index].bm_h, GameBitmaps[bi.index].bm_flags,
		piggy_get_bitmap_disk_flags(bi.index));
	write_bitmap_png(file, &GameBitmaps[bi.index], scale);
}

static void write_wav(const char *name, const short *s, int n_values, int channels, int rate)
{
	char path[600];
	FILE *fp;
	ubyte h[44];
	int bytes = n_values*2;

	if (!Out_dir[0]) return;
	sprintf(path, "%s/%s", Out_dir, name);
	fp = fopen(path, "wb");
	if (!fp) return;
	memcpy(h, "RIFF", 4);
	h[4] = (bytes+36); h[5] = (bytes+36)>>8; h[6] = (bytes+36)>>16; h[7] = (bytes+36)>>24;
	memcpy(h+8, "WAVEfmt ", 8);
	h[16] = 16; h[17] = h[18] = h[19] = 0;
	h[20] = 1; h[21] = 0; h[22] = channels; h[23] = 0;
	h[24] = rate; h[25] = rate>>8; h[26] = rate>>16; h[27] = rate>>24;
	h[28] = (rate*channels*2); h[29] = (rate*channels*2)>>8; h[30] = (rate*channels*2)>>16; h[31] = 0;
	h[32] = channels*2; h[33] = 0; h[34] = 16; h[35] = 0;
	memcpy(h+36, "data", 4);
	h[40] = bytes; h[41] = bytes>>8; h[42] = bytes>>16; h[43] = bytes>>24;
	fwrite(h, 1, 44, fp);
	fwrite(s, 2, n_values, fp);		// little endian host
	fclose(fp);
	printf("wrote %s (%d values, %d Hz, %d channel)\n", path, n_values, rate, channels);
}

// ---------------------------------------------------------------------

// Compare the texture table with the texture names of the level file.
// Returns the number of names that differ, or -1 if there is no LVL file.
static int check_level_textures(char *level_name, int quiet)
{
	char file[40];
	ubyte *d;
	int size, mine, tex_ofs, tex_n, tex_size, i, n, bad = 0;

	sprintf(file, "%s.LVL", level_name);
	d = dtu_load_file(file, &size);
	if (!d) {
		if (!quiet) printf("level %s: no LVL file, the texture names are not compared\n", level_name);
		return -1;
	}
	if (size < 100 || memcmp(d, "LVLP", 4)) {
		printf("level %s: not a LVLP file\n", level_name);
		free(d);
		return -1;
	}
	mine = d[8] | (d[9]<<8) | (d[10]<<16) | (d[11]<<24);
	#define I32(o) (d[o] | (d[(o)+1]<<8) | (d[(o)+2]<<16) | (d[(o)+3]<<24))
	tex_ofs = I32(mine+72);
	tex_n = I32(mine+76);
	tex_size = I32(mine+80);
	#undef I32
	if (tex_ofs < 0 || tex_size != 13 || tex_n < 0 || tex_ofs + tex_n*13 > size) {
		printf("level %s: bad texture name table\n", level_name);
		free(d);
		return -1;
	}
	n = (tex_n < NumTextures) ? tex_n : NumTextures;
	for (i=0; i<n; i++)
		if (stricmp((char *)d + tex_ofs + i*13, TmapInfo[i].filename)) {
			if (bad < 5 && !quiet)
				printf("  texture %d: level '%s', table '%s'\n", i, (char *)d + tex_ofs + i*13, TmapInfo[i].filename);
			bad++;
		}
	if (!quiet)
		printf("level %s: %d texture names in the LVL file, %d in the table, %d differ in the first %d\n",
			level_name, tex_n, NumTextures, bad, n);
	if (tex_n > NumTextures)
		bad += tex_n - NumTextures;
	free(d);
	return bad;
}

static int count_used(int *n_frames_max)
{
	int i, n = 0;
	*n_frames_max = 0;
	for (i=0; i<VCLIP_MAXNUM; i++)
		if (Vclip[i].num_frames > 0) {
			n++;
			if (Vclip[i].num_frames > *n_frames_max) *n_frames_max = Vclip[i].num_frames;
		}
	return n;
}

static void print_counts(void)
{
	int i, n, fmax, n_eclips = 0, n_wclips = 0, bound = 0, sounds = 0;

	for (i=0; i<piggy_get_num_eggs(); i++) {
		int nb, ns, nm, fb, fs, fm, bad;
		bad = piggy_get_egg_counts(i, &nb, &ns, &nm, &fb, &fs, &fm);
		printf("archive %d: %d bitmaps (from %d), %d sounds (from %d), %d models (from %d), %d entries not valid\n",
			i, nb, fb, ns, fs, nm, fm, bad);
	}
	printf("bitmaps %d (with bitmap 0), sound files %d, models %d\n", Num_bitmap_files, Num_sound_files, N_polygon_models);

	n = count_used(&fmax);
	for (i=0; i<MAX_EFFECTS; i++) if (Effects[i].vc.num_frames > 0) n_eclips++;
	for (i=0; i<MAX_WALL_ANIMS; i++) if (WallAnims[i].num_frames > 0) n_wclips++;
	for (i=0; i<N_polygon_models; i++) if (Polygon_models[i].dtu_bound) bound++;
	for (i=0; i<MAX_SOUNDS; i++) if (Sounds[i] != SOUND_FILE_NONE) sounds++;

	printf("textures %d (tmaps %d), vclips %d (highest %d, most frames %d), eclips %d (highest %d), wall clips %d (highest %d)\n",
		NumTextures, Num_tmaps, n, Num_vclips-1, fmax, n_eclips, Num_effects-1, n_wclips, Num_wall_anims-1);
	printf("monsters %d, weapons %d, player types %d, dressing %d, misc items %d, armor %d, spells %d\n",
		N_robot_types, N_weapon_types, N_player_types, N_dressing_types, N_misc_types, N_armor_types, N_spell_types);
	printf("object types %d, table records %d, sound numbers %d, models named by the table %d of %d, warnings %d\n",
		Num_total_object_types, N_tbl_records, sounds, bound, N_polygon_models, Bm_tbl_warnings);
}

// Draw one model into a picture with the 3d code of the game.
static void write_model_png(int mn, const char *file)
{
	grs_canvas *canv;
	vms_angvec angles;
	ubyte *rgb;
	int x, y, w = 200, h = 200;

	if (mn < 0 || mn >= N_polygon_models || Polygon_models[mn].model_data == NULL) {
		printf("model %d: not loaded\n", mn);
		return;
	}
	canv = gr_create_canvas(w, h);
	gr_set_current_canvas(canv);
	angles.p = 0; angles.b = 0; angles.h = 0x6000;
	draw_model_picture(mn, &angles);

	rgb = malloc(w*h*3);
	for (y=0; y<h; y++)
		for (x=0; x<w; x++) {
			int c = canv->cv_bitmap.bm_data[y*canv->cv_bitmap.bm_rowsize + x];
			rgb[(y*w+x)*3] = gr_palette[c*3]*4;
			rgb[(y*w+x)*3+1] = gr_palette[c*3+1]*4;
			rgb[(y*w+x)*3+2] = gr_palette[c*3+2]*4;
		}
	write_png(file, rgb, w, h);
	free(rgb);
	gr_free_canvas(canv);
}

static void test_one(char *level, char *player)
{
	int i, mn;
	char value[200];

	if (!bm_load_level_tables(level, player)) {
		printf("FAILED to load the tables of level '%s' (player '%s')\n", level, player ? player : "");
		return;
	}
	printf("level '%s', player '%s', level archive '%s'\n", Bm_level_name, Bm_player_name, Bm_egg_name);
	print_counts();
	check_level_textures(level, 0);

	printf("textures: 0 '%s', 5 '%s', 100 '%s', last '%s'\n", TmapInfo[0].filename, TmapInfo[5].filename,
		TmapInfo[100].filename, NumTextures ? TmapInfo[NumTextures-1].filename : "");

	for (i=0; i<N_robot_types && i<4; i++) {
		mn = Robot_info[i].model_num;
		printf("monster %d '%s': model %d '%s', %d submodels, %d frames, %d textures from slot %d, hit points %d, flags 0x%x\n",
			i, Monster_info[i].name, mn, mn >= 0 ? Pof_names[mn] : "",
			mn >= 0 ? Polygon_models[mn].n_models : 0, mn >= 0 ? Polygon_models[mn].n_frames : 0,
			mn >= 0 ? Polygon_models[mn].n_textures : 0, mn >= 0 ? Polygon_models[mn].first_texture : 0,
			Monster_info[i].max_hit_points, Monster_info[i].flags);
	}
	if (N_player_types > 0) {
		player_type_info *pt = &Player_type_info[0];
		printf("player type 0: %d models, first model %d '%s', eye_offset %.2f, mass %.2f\n", pt->n_models,
			pt->model_nums[0], pt->model_nums[0] >= 0 ? Pof_names[pt->model_nums[0]] : "",
			f2fl(pt->eye_offset), f2fl(Player_ship->mass));
	}
	for (i=0; i<N_dressing_types && i<60; i++)
		if (Dressing_info[i].model_num >= 0) {
			printf("dressing %d '%s': model %d '%s'\n", i, Dressing_info[i].name, Dressing_info[i].model_num,
				Pof_names[Dressing_info[i].model_num]);
			break;
		}
	if (N_weapon_types > 1 && bm_tbl_get(Weapon_item_info[1].record, "speed", value, sizeof(value)))
		printf("weapon 1 '%s': speed '%s', model %d, vclip %d\n", Weapon_item_info[1].name, value,
			Weapon_info[1].model_num, Weapon_info[1].weapon_vclip);
	if (N_spell_types > 33)
		printf("spell 33 '%s': icon %d, model %d\n", Spell_info[33].name, Spell_info[33].icon, Spell_info[33].model_num);

	if (Out_dir[0]) {
		write_named_bitmap("stone223", 4);
		write_named_bitmap("wood16", 4);
		write_named_bitmap("ladder", 4);
		write_named_bitmap("potion#1", 4);
		write_named_bitmap("idor2#6", 4);
		write_named_bitmap("pgant3br", 4);
		write_named_bitmap("btnwdypt", 2);
		if (NumTextures > 300) {
			printf("texture 300 is '%s'\n", TmapInfo[300].filename);
			write_bitmap_png("texture_300.png", &GameBitmaps[Textures[300].index], 4);
		}
		if (Vclip[11].num_frames > 2) {
			printf("vclip 11: %d frames\n", Vclip[11].num_frames);
			write_bitmap_png("vclip11_frame2.png", &GameBitmaps[Vclip[11].frames[2].index], 4);
		}

		// One sound, for a comparison with tools/tun.py
		i = piggy_find_sound("fireball");
		if (i != SOUND_FILE_NONE) {
			int n_values, channels;
			short *s = piggy_get_sound_16(i, &n_values, &channels);
			printf("sound 'fireball': file %d, rate %d, %d values, %d bytes of 8-bit data\n", i,
				piggy_get_sound_rate(i), n_values, GameSounds[i].length);
			if (s) write_wav("fireball.wav", s, n_values, channels, piggy_get_sound_rate(i));
		}

		// Models: a piece of scenery, a monster and the player body
		g3_init();
		mn = polyobj_find_model("barrel01.pof");
		if (mn >= 0) write_model_png(mn, "model_barrel01.png");
		mn = polyobj_find_model("kettle3.pof");
		if (mn >= 0) write_model_png(mn, "model_kettle3.png");
		if (N_robot_types > 0 && Robot_info[0].model_num >= 0)
			write_model_png(Robot_info[0].model_num, "model_monster0.png");
		if (N_player_types > 0 && Player_type_info[0].model_nums[0] >= 0)
			write_model_png(Player_type_info[0].model_nums[0], "model_player.png");
	}
}

static void test_text_and_fonts(void)
{
	int i;

	load_text();
	printf("text: %d strings. 0 '%s', 2 '%s', 104 '%s', 900 '%s'\n", N_TEXT_STRINGS,
		Text_string[0], Text_string[2], Text_string[104], Text_string[900]);

	gamefont_init();
	for (i=0; i<MAX_FONTS; i++)
		if (Gamefonts[i])
			printf("font %d: %d x %d, flags %d, chars %d to %d\n", i, Gamefonts[i]->ft_w, Gamefonts[i]->ft_h,
				Gamefonts[i]->ft_flags, Gamefonts[i]->ft_minchar, Gamefonts[i]->ft_maxchar);

	if (Out_dir[0] && Gamefonts[0] && Gamefonts[4]) {
		grs_canvas *canv = gr_create_canvas(320, 60);
		ubyte *rgb = malloc(320*60*3);
		int x, y;

		gr_set_current_canvas(canv);
		gr_clear_canvas(0);
		gr_set_curfont(Gamefonts[0]);
		gr_set_fontcolor(gr_find_closest_color(63,63,63), -1);
		gr_string(4, 2, "Undermountain 123");
		gr_set_curfont(Gamefonts[1]);
		gr_string(4, 30, Text_string[0]);
		gr_set_curfont(Gamefonts[4]);
		gr_string(4, 46, "The quick brown fox jumps over the lazy dog");
		for (y=0; y<60; y++)
			for (x=0; x<320; x++) {
				int c = canv->cv_bitmap.bm_data[y*canv->cv_bitmap.bm_rowsize + x];
				rgb[(y*320+x)*3] = gr_palette[c*3]*4;
				rgb[(y*320+x)*3+1] = gr_palette[c*3+1]*4;
				rgb[(y*320+x)*3+2] = gr_palette[c*3+2]*4;
			}
		write_png("fonts.png", rgb, 320, 60);
		free(rgb);
		gr_free_canvas(canv);
	}
}

// ---------------------------------------------------------------------
// -texcheck: the texture references of the models. For each model of the
// loaded archives, each texture number that a TMAPPOLY of its data uses is
// resolved as draw_polygon_model_frame() does, and a reference is listed if
// it gives bitmap 0, an empty bitmap, a bitmap with only transparent pixels
// or a bitmap that is (almost) all black.

static void tc_mark_used(const ubyte *data, int size, int ofs, int depth, ubyte *used)
{
	#define TC_W(o) (((ofs)+(o)+2 <= size) ? (data[ofs+(o)] | (data[ofs+(o)+1]<<8)) : 0)
	int n;

	if (depth > 64)
		return;
	for (;;) {
		if (ofs < 0 || ofs+2 > size)
			return;
		switch (TC_W(0)) {
		case 0: return;
		case 1: ofs += 4 + TC_W(2)*12; break;
		case 2: ofs += 30 + ((TC_W(2)*2)|2); break;
		case 3:
			n = (short) TC_W(28);
			if (n >= 0 && n < MAX_POLYOBJ_TEXTURES) used[n] = 1;
			n = TC_W(2);
			ofs += 30 + ((n*2)|2) + n*12;
			break;
		case 4:
			tc_mark_used(data, size, ofs+TC_W(28), depth+1, used);
			tc_mark_used(data, size, ofs+TC_W(30), depth+1, used);
			ofs += 32;
			break;
		case 5: ofs += 36; break;
		case 6: tc_mark_used(data, size, ofs+TC_W(16), depth+1, used); ofs += 20; break;
		case 7: ofs += 8 + TC_W(2)*12; break;
		case 8: ofs += 4; break;
		default: return;
		}
	}
	#undef TC_W
}

// The role of a model in the table: weapon, arms, creature, ...
static void tc_model_role(int mn, char *out, int len)
{
	int i, j;

	out[0] = 0;
	#define TC_ADD(...) do { int l_ = (int)strlen(out); if (l_ < len-1) snprintf(out+l_, len-l_, __VA_ARGS__); } while (0)
	for (i=0; i<N_weapon_types; i++)
		if (Weapon_info[i].model_num == mn || Weapon_info[i].model_num_inner == mn)
			TC_ADD("weapon %d '%s' ", i, Weapon_item_info[i].name);
	for (i=0; i<N_player_types; i++)
		for (j=0; j<Player_type_info[i].n_models; j++)
			if (Player_type_info[i].model_nums[j] == mn)
				TC_ADD("arms/player ");
	for (i=0; i<N_robot_types; i++)
		if (Robot_info[i].model_num == mn)
			TC_ADD("creature '%s' ", Monster_info[i].name);
	for (i=0; i<N_dressing_types; i++)
		if (Dressing_info[i].model_num == mn)
			TC_ADD("dressing '%s' ", Dressing_info[i].name);
	for (i=0; i<N_spell_types; i++)
		if (Spell_info[i].model_num == mn)
			TC_ADD("spell '%s' ", Spell_info[i].name);
	for (i=0; i<N_misc_types; i++)
		if (Misc_info[i].model_num == mn)
			TC_ADD("object '%s' ", Misc_info[i].name);
	for (i=0; i<N_armor_types; i++)
		if (Armor_info[i].model_num == mn)
			TC_ADD("armor '%s' ", Armor_info[i].name);
	#undef TC_ADD
	if (!out[0])
		snprintf(out, len, "not named by the table ");
}

// 0 if the bitmap is good, else a reason
static const char *tc_bitmap_problem(int index, int *black_pct)
{
	grs_bitmap *bm;
	ubyte *pix;
	int i, n, n_black = 0, n_clear = 0;

	*black_pct = 0;
	if (index <= 0 || index >= Num_bitmap_files)
		return "bitmap 0 (missing)";
	bm = &GameBitmaps[index];
	if (bm->bm_w <= 0 || bm->bm_h <= 0 || bm->bm_data == NULL)
		return "empty bitmap";
	pix = bitmap_pixels(bm);
	n = bm->bm_w * bm->bm_h;
	for (i=0; i<n; i++) {
		int c = pix[i];
		if (c == 255 && (bm->bm_flags & BM_FLAG_TRANSPARENT))
			n_clear++;
		else if (gr_palette[c*3] + gr_palette[c*3+1] + gr_palette[c*3+2] <= 6)
			n_black++;
	}
	free(pix);
	if (n_clear == n)
		return "all transparent";
	*black_pct = n_black * 100 / (n - n_clear);
	if (*black_pct >= 90)
		return "black";
	return NULL;
}

static int Tc_total_refs, Tc_total_bad, Tc_weapon_bad;
static int Tc_orig = 0, Tc_orig_diff, Tc_orig_bad;		// -texorig

static void texcheck_loaded(const char *label)
{
	int mn;

	for (mn=0; mn<N_polygon_models; mn++) {
		polymodel *po = &Polygon_models[mn];
		ubyte used[MAX_POLYOBJ_TEXTURES];
		char role[300];
		int t, is_weapon;

		if (po->model_data == NULL)
			continue;
		memset(used, 0, sizeof(used));
		tc_mark_used(po->model_data, po->model_data_size, 0, 0, used);
		tc_model_role(mn, role, sizeof(role));
		is_weapon = strstr(role, "weapon") || strstr(role, "arms") || strstr(role, "creature");

		for (t=0; t<MAX_POLYOBJ_TEXTURES; t++) {
			const char *why = NULL;
			char what[120];
			int slot = -1, index = -1, pct = 0, k;

			if (!used[t])
				continue;
			Tc_total_refs++;
			what[0] = 0;
			if (t >= po->n_textures)
				why = "texture number beyond the list of the table (bitmap 0)";
			else {
				int ptr = po->first_texture + t;
				slot = ObjBitmapPtrs[ptr];
				// an eclip texture: check each frame
				for (k=0; k<MAX_EFFECTS; k++)
					if (Effects[k].changing_object_texture == slot && Effects[k].vc.num_frames > 0) {
						int f;
						for (f=0; f<Effects[k].vc.num_frames && !why; f++) {
							index = Effects[k].vc.frames[f].index;
							why = tc_bitmap_problem(index, &pct);
							if (why) snprintf(what, sizeof(what), "eclip %d frame %d ", k, f);
						}
						break;
					}
				if (k == MAX_EFFECTS) {
					index = ObjBitmaps[slot].index;
					why = tc_bitmap_problem(index, &pct);
				}
			}
			if (!po->dtu_bound && !why)
				why = "model not named by the table (slots from 0)";

			// The rule of DTUGAME.EXE for $PLAYER, $WEAPON and $SPELL models
			// (models.md, "Textures"): the first_texture of the EGG header,
			// plus 1 for a model of the level archive. Listed where it gives
			// another bitmap than the port and that bitmap is bad.
			if (Tc_orig && !why && po->dtu_bound && (strstr(role, "weapon") || strstr(role, "spell") || strstr(role, "arms"))) {
				int fm = 0, nm = 0, egg_level = piggy_get_num_eggs() - 1, oslot, oindex, opct;
				const char *owhy;
				char oname[20] = "";

				piggy_get_egg_counts(egg_level, NULL, NULL, &nm, NULL, NULL, &fm);
				k = po->dtu_first_texture_file + t + (mn >= fm ? 1 : 0);
				if (k >= 0 && k < MAX_OBJ_BITMAPS) {
					oslot = ObjBitmapPtrs[k];
					oindex = ObjBitmaps[oslot].index;
					if (oindex != index) {
						owhy = tc_bitmap_problem(oindex, &opct);
						if (oindex > 0 && oindex < Num_bitmap_files) piggy_get_bitmap_name(oindex, oname);
						Tc_orig_diff++;
						if (owhy) Tc_orig_bad++;
						printf("%-12s model %3d %-13s tex %2d: original rule slot %4d bitmap '%s' (%s, black %d%%), port bitmap %d %s\n",
							label, mn, Pof_names[mn], t, k, oname, owhy ? owhy : "good", opct, index, role);
					}
				}
			}
			if (why) {
				Tc_total_bad++;
				if (is_weapon) Tc_weapon_bad++;
				char bname[20] = "";
				if (index > 0 && index < Num_bitmap_files) piggy_get_bitmap_name(index, bname);
				printf("%-12s model %3d %-13s tex %2d of %2d (table %2d, first %4d) slot %4d bitmap %4d '%s' %s: %s%s (black %d%%)\n",
					label, mn, Pof_names[mn], t, po->dtu_textures_used, po->n_textures, po->first_texture, slot,
					index, bname, role, what, why, pct);
			}
		}
	}
}

static void texcheck_sweep(char *dir)
{
	static char *players[] = { "hmc", "hmt" };
	DIR *d = opendir(dir);
	struct dirent *e;
	char names[400][16];
	int n = 0, i, j, p, n_loaded = 0;

	if (!d) { printf("cannot read directory %s\n", dir); return; }
	while ((e = readdir(d)) != NULL && n < 400) {
		char *dot = strrchr(e->d_name, '.');
		if (dot && (!stricmp(dot, ".BIN") || !stricmp(dot, ".HEN")) && dot - e->d_name <= 8) {
			memcpy(names[n], e->d_name, dot - e->d_name);
			names[n][dot - e->d_name] = 0;
			n++;
		}
	}
	closedir(d);
	for (i=0; i<n; i++)
		for (j=i+1; j<n; j++)
			if (stricmp(names[i], names[j]) > 0) {
				char t[16];
				strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t);
			}

	for (i=0; i<n; i++) {
		if (i > 0 && !stricmp(names[i], names[i-1]))
			continue;
		if (!stricmp(names[i], "hmc") || !stricmp(names[i], "hmt"))
			continue;
		for (p=0; p<2; p++) {
			char label[40];
			if (!bm_load_level_tables(names[i], players[p]))
				break;
			n_loaded++;
			snprintf(label, sizeof(label), "%s+%s", names[i], players[p]);
			texcheck_loaded(label);
		}
	}
	printf("texcheck: %d table loads, %d model texture references, %d bad (%d of weapon, arms or creature models)\n",
		n_loaded, Tc_total_refs, Tc_total_bad, Tc_weapon_bad);
	if (Tc_orig)
		printf("texcheck: with the header rule of DTUGAME.EXE %d references of weapon, spell and arms models give another bitmap, %d of them a bad one\n",
			Tc_orig_diff, Tc_orig_bad);
}

static void sweep(char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char names[400][16];
	int n = 0, i, j, ok = 0, failed = 0, n_levels = 0, n_levels_ok = 0;

	if (!d) { printf("cannot read directory %s\n", dir); return; }
	while ((e = readdir(d)) != NULL && n < 400) {
		char *dot = strrchr(e->d_name, '.');
		if (dot && (!stricmp(dot, ".BIN") || !stricmp(dot, ".HEN")) && dot - e->d_name <= 8) {
			memcpy(names[n], e->d_name, dot - e->d_name);
			names[n][dot - e->d_name] = 0;
			n++;
		}
	}
	closedir(d);
	// sort
	for (i=0; i<n; i++)
		for (j=i+1; j<n; j++)
			if (stricmp(names[i], names[j]) > 0) {
				char t[16];
				strcpy(t, names[i]); strcpy(names[i], names[j]); strcpy(names[j], t);
			}

	for (i=0; i<n; i++) {
		int bad = 0, nb = 0, ns = 0, nm = 0, bound = 0, k;

		if (i > 0 && !stricmp(names[i], names[i-1]))
			continue;
		if (!bm_load_level_tables(names[i], NULL)) {
			char *nl = strchr(Dtu_log, 10);
			if (nl) *nl = 0;
			printf("%-9s FAILED: %s\n", names[i], Dtu_log);
			failed++;
			// A table that is missing or empty: try the archive of the same name.
			if (strstr(Dtu_log, "table file")) {
				char egg[32];
				sprintf(egg, "%s.EGG", names[i]);
				piggy_free_all();
				if (piggy_load_egg(egg) >= 0) {
					bad = piggy_get_egg_counts(0, &nb, &ns, &nm, NULL, NULL, NULL);
					printf("%-9s archive alone: bm %4d snd %3d mod %3d bad %d\n", names[i], nb, ns, nm, bad);
				}
			}
			continue;
		}
		ok++;
		bad = piggy_get_egg_counts(0, &nb, &ns, &nm, NULL, NULL, NULL);
		for (k=0; k<N_polygon_models; k++) if (Polygon_models[k].dtu_bound) bound++;
		k = check_level_textures(names[i], 1);
		if (k >= 0) { n_levels++; if (k == 0) n_levels_ok++; else printf("%-9s level texture names differ: %d\n", names[i], k); }
		printf("%-9s %-12s bm %4d snd %3d mod %3d (named %3d) bad %d | tex %4d vc %3d ec %3d wc %2d mon %2d wpn %2d dr %2d obj %3d arm %2d sp %2d pl %d | warn %d\n",
			names[i], Bm_egg_name, nb, ns, nm, bound, bad, NumTextures, Num_vclips, Num_effects, Num_wall_anims,
			N_robot_types, N_weapon_types, N_dressing_types, N_misc_types, N_armor_types, N_spell_types,
			N_player_types, Bm_tbl_warnings);
	}
	printf("sweep: %d tables loaded, %d failed. %d of %d levels have the texture names of their table\n", ok, failed, n_levels_ok, n_levels);
}

// -lights: the textures with a light value, one line for each table and
// texture: table, texture number, name, light value, eclip.
static void lights_sweep(char *dir)
{
	DIR *d = opendir(dir);
	struct dirent *e;
	char names[400][16];
	int n = 0, i, j, t;

	if (!d) { printf("cannot read directory %s\n", dir); return; }
	while ((e = readdir(d)) != NULL && n < 400) {
		char *dot = strrchr(e->d_name, '.');
		if (dot && (!stricmp(dot, ".BIN") || !stricmp(dot, ".HEN")) && dot - e->d_name <= 8) {
			memcpy(names[n], e->d_name, dot - e->d_name);
			names[n][dot - e->d_name] = 0;
			n++;
		}
	}
	closedir(d);
	for (i=0; i<n; i++)
		for (j=i+1; j<n; j++)
			if (stricmp(names[i], names[j]) > 0) {
				char tmp[16];
				strcpy(tmp, names[i]); strcpy(names[i], names[j]); strcpy(names[j], tmp);
			}
	for (i=0; i<n; i++) {
		if (i > 0 && !stricmp(names[i], names[i-1]))
			continue;
		if (!bm_load_level_tables(names[i], NULL))
			continue;
		for (t=0; t<NumTextures; t++)
			if (TmapInfo[t].lighting)
				printf("%-9s %4d %-13s %6.3f ec %d\n", names[i], t, TmapInfo[t].filename,
					f2fl(TmapInfo[t].lighting), TmapInfo[t].eclip_num);
	}
}

// The game code has its own main() (inferno.c). The test is linked with
// --wrap=main, so the start code calls this function.
int __wrap_main(int argc, char **argv)
{
	char *level = "ypinn", *player = "hmc", *sweep_dir = NULL, *texcheck_dir = NULL, *lights_dir = NULL;
	int i, n_dirs = 0, verbose = 0;

	for (i=1; i<argc; i++) {
		if (!strcmp(argv[i], "-level") && i+1 < argc) level = argv[++i];
		else if (!strcmp(argv[i], "-player") && i+1 < argc) player = argv[++i];
		else if (!strcmp(argv[i], "-out") && i+1 < argc) strncpy(Out_dir, argv[++i], sizeof(Out_dir)-1);
		else if (!strcmp(argv[i], "-sweep") && i+1 < argc) sweep_dir = argv[++i];
		else if (!strcmp(argv[i], "-texcheck") && i+1 < argc) texcheck_dir = argv[++i];
		else if (!strcmp(argv[i], "-texorig")) Tc_orig = 1;
		else if (!strcmp(argv[i], "-lights") && i+1 < argc) lights_dir = argv[++i];
		else if (!strcmp(argv[i], "-v")) verbose = 1;
		else { dtu_add_data_dir(argv[i]); n_dirs++; }
	}
	if (!n_dirs) {
		printf("usage: test_dtudata [-level NAME] [-player NAME] [-out DIR] [-sweep DIR] [-v] DATADIR ...\n");
		return 1;
	}
	if (!stricmp(player, "none")) player = NULL;

	crc_init();
	if (verbose) Dtu_log_to_stderr = 1;
	if (verbose) show_mem_info = 1;		// mem.c lists the blocks that are not freed

	if (!dtu_load_palette()) {
		printf("PALETTE.256 is not in the data directories\n");
		return 1;
	}
	printf("palette: color 1 is %d %d %d, color 254 is %d %d %d\n", gr_palette[3], gr_palette[4], gr_palette[5],
		gr_palette[254*3], gr_palette[254*3+1], gr_palette[254*3+2]);

	// The font code and the 3d code draw to the current canvas. There is
	// no screen here.
	gr_set_current_canvas(gr_create_canvas(320, 200));
	{
		static grs_screen screen;
		screen.sc_w = 320;
		screen.sc_h = 200;
		screen.sc_aspect = fixdiv(320*3, 200*4);		// the 320 x 200 mode of Descent 1
		grd_curscreen = &screen;
	}

	bm_init();

	if (texcheck_dir) {
		texcheck_sweep(texcheck_dir);
		return 0;
	}
	if (lights_dir) {
		lights_sweep(lights_dir);
		return 0;
	}
	if (sweep_dir) {
		sweep(sweep_dir);
		return 0;
	}

	test_text_and_fonts();
	test_one(level, player);

	return 0;
}
