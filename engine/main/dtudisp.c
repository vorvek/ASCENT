/*
 * ASCENT: new file. The in-game interface of Descent to Undermountain.
 * It replaces the Descent 1 cockpit and HUD drawing (gauges.c, hud.c and
 * the cockpit setup of game.c). See dtudisp.h.
 *
 * The layout and the behavior are from DTUGAME.EXE version 1.3. Each
 * comment with an address (0x...) names the function or the data of the
 * executable that the code below follows. docs/re/ingame-ui.md has the
 * details. A comment that says "guess" marks a part that is not verified.
 */

#include "dtulang.h"		// ASCENT: the language of the text
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "inferno.h"
#include "types.h"
#include "fix.h"
#include "gr.h"
#include "palette.h"
#include "error.h"
#include "mem.h"		// malloc and free of the game
#include "args.h"
#include "key.h"
#include "mouse.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "weapon.h"
#include "bm.h"
#include "gamefont.h"
#include "text.h"
#include "dtufile.h"
#include "dtugame.h"
#include "dtudisp.h"
#include "dtuobj.h"
#include "dtuchar.h"
#include "dtuspell.h"		// ASCENT: the spell list of the statistics screen
#include "physics.h"		// ASCENT: Dtu_player_state (infravision)
#include "kconfig.h"		// ASCENT: the key table (dtu_action_key)
#include "3d.h"			// ASCENT: g3_gpu_view_dest
#include "dtuhdui.h"		// ASCENT: the upscaled interface pictures
#include "dtudlg.h"		// ASCENT: dtu_dlg_is_open, the cursor over the conversation window
#include "segment.h"
#include "automap.h"		// ASCENT: the map of the map screen
#include "dtucombat.h"		// ASCENT: the target hint of the cursor
#include "pcx.h"				// ASCENT: HRISBTN.PCH, the second images of SPELLS
#include "timer.h"			// ASCENT: the repeat of a held scroll bar arrow
#include "platform.h"		// ASCENT: plat_mouse_wheel

int Dtu_hires = 1;					// 0x13A94B
int Dtu_display_active = 0;
int Dtu_interface_mode = DTU_IM_MAIN;		// 0x13B616
int Dtu_immersive = 0;
int Dtu_ui_key_repeat = 0;
int Dtu_player_portrait = 0;

extern void game_init_render_sub_buffers( int x, int y, int w, int h );
extern int Game_window_w, Game_window_h;

//------------------------------------------------------------------------
// FEBGND.JRG and FEBGND.DAT (docs/formats/jrg.md)

#define FEBGND_MAX	256

typedef struct febgnd_entry {
	uint	offset, w, h, size;
} febgnd_entry;

static febgnd_entry Febgnd[FEBGND_MAX];
static int N_febgnd = 0;
static char Febgnd_dat_path[600];

static uint get_u32(ubyte *p)
{
	return (uint)p[0] | ((uint)p[1] << 8) | ((uint)p[2] << 16) | ((uint)p[3] << 24);
}

static int febgnd_open(void)
{
	ubyte *jrg;
	int size, i;

	if (N_febgnd)
		return 1;
	jrg = dtu_load_file("FEBGND.JRG", &size);
	if (!jrg)
		return 0;
	if (size >= 4)
		N_febgnd = (int) get_u32(jrg);
	if (N_febgnd < 1 || N_febgnd > FEBGND_MAX || size < 4 + 16 * N_febgnd)
		N_febgnd = 0;
	for (i = 0; i < N_febgnd; i++) {
		Febgnd[i].offset = get_u32(jrg + 4 + 16 * i);
		Febgnd[i].w = get_u32(jrg + 8 + 16 * i);
		Febgnd[i].h = get_u32(jrg + 12 + 16 * i);
		Febgnd[i].size = get_u32(jrg + 16 + 16 * i);
	}
	free(jrg);
	if (N_febgnd && !dtu_find_file("FEBGND.DAT", Febgnd_dat_path, sizeof(Febgnd_dat_path)))
		N_febgnd = 0;
	return N_febgnd != 0;
}

// Loads one picture. The palette of the entry (768 bytes) is skipped: the
// interface pictures have the game palette. Returns NULL if not available.
static grs_bitmap *febgnd_load(int n)
{
	FILE *f;
	ubyte *src, *dst, ring[4096];
	grs_bitmap *bm;
	int src_size, dst_size, sp, dp, rp, flags, bit;

	if (n < 0 || n >= N_febgnd || Febgnd[n].size <= 768 || Febgnd[n].w > 1024 || Febgnd[n].h > 1024)
		return NULL;
	f = fopen(Febgnd_dat_path, "rb");
	if (!f)
		return NULL;
	src_size = Febgnd[n].size - 768;
	src = malloc(src_size);
	if (!src || fseek(f, Febgnd[n].offset + 768, SEEK_SET) != 0 || fread(src, 1, src_size, f) != (size_t) src_size) {
		fclose(f);
		free(src);
		return NULL;
	}
	fclose(f);

	bm = gr_create_bitmap(Febgnd[n].w, Febgnd[n].h);
	dst = bm->bm_data;
	dst_size = Febgnd[n].w * Febgnd[n].h;
	memset(dst, 0, dst_size);

	// LZSS of felz.c: ring of 4096 bytes filled with 0x20, start 0xFEE
	memset(ring, 0x20, sizeof(ring));
	rp = 0xFEE;
	sp = dp = 0;
	flags = 0;
	bit = 8;
	while (sp < src_size && dp < dst_size) {
		if (bit == 8) {
			flags = src[sp++];
			bit = 0;
			continue;
		}
		if (flags & (1 << bit)) {
			ubyte c = src[sp++];
			dst[dp++] = c;
			ring[rp] = c;
			rp = (rp + 1) & 4095;
		} else {
			int lo, hi, pos, len;
			if (sp + 1 >= src_size)
				break;
			lo = src[sp++];
			hi = src[sp++];
			pos = lo | ((hi & 0xF0) << 4);
			len = (hi & 0x0F) + 3;
			while (len-- && dp < dst_size) {
				ubyte c = ring[pos];
				pos = (pos + 1) & 4095;
				dst[dp++] = c;
				ring[rp] = c;
				rp = (rp + 1) & 4095;
			}
		}
		bit++;
	}
	free(src);
	dtu_hdui_loaded(bm, n);		// ASCENT: the reference of the upscaled picture
	return bm;
}

// ASCENT: for the front end (dtufe.c), which shows each picture with the
// palette of its entry. pal gets the 768 bytes of the palette (values 0 to
// 63), or stays as it is if the entry is not available.
grs_bitmap *dtu_febgnd_load(int n, ubyte *pal)
{
	FILE *f;

	if (!febgnd_open() || n < 0 || n >= N_febgnd || Febgnd[n].size <= 768)
		return NULL;
	if (pal) {
		f = fopen(Febgnd_dat_path, "rb");
		if (!f)
			return NULL;
		if (fseek(f, Febgnd[n].offset, SEEK_SET) != 0 || fread(pal, 1, 768, f) != 768) {
			fclose(f);
			return NULL;
		}
		fclose(f);
	}
	return febgnd_load(n);
}

//------------------------------------------------------------------------
// Layout. The original computes the values in 0x44F1B as scale * constant
// with scale 1 (low resolution) or 2 (high resolution). The view heights
// are not scaled: in high resolution each rendered line is shown twice.

#define SC(v)	((v) * (Dtu_hires ? 2 : 1))

typedef struct dtu_rect {
	int x, y, w, h;
} dtu_rect;

// Message box of the main screen (0x142EF8 low, 0x142F08 high)
static dtu_rect Msg_rect[2] = { { 63, 150, 244, 46 }, { 131, 306, 470, 77 } };

// A scroll bar (SBarDef, docs/re/ingame-ui.md, "Scroll bars")
typedef struct dtu_sbar {
	dtu_rect up, dn;			// the arrows on the screen
	dtu_rect slider;			// GrabSlider: the source of the slider
	dtu_rect bar;				// GrabBar: the bar, at the same place on the screen
	dtu_rect up_rest, up_pressed, dn_rest, dn_pressed;	// sources of the arrow images
} dtu_sbar;

// The scroll bar of the message box (SBar 0x1BD5B0, SBarDef 0x1431BB low,
// 0x14319A high), created by 0x340BA in mode 0. [0] low, [1] high.
static dtu_sbar Msg_sbar[2] = {
	{ { 307, 150, 7, 8 }, { 307, 187, 7, 8 }, { 300, 158, 7, 4 }, { 307, 158, 7, 29 },
	  { 307, 150, 7, 8 }, { 300, 150, 7, 8 }, { 307, 187, 7, 8 }, { 300, 187, 7, 8 } },
	{ { 605, 304, 26, 20 }, { 604, 365, 26, 20 }, { 323, 271, 25, 12 }, { 605, 324, 26, 42 },
	  { 605, 304, 26, 20 }, { 354, 263, 26, 20 }, { 604, 366, 26, 20 }, { 387, 263, 26, 20 } },
};

// ASCENT: the arrows of the message box move the text the other way: the up
// arrow shows older lines, and the slider is at the bottom for the newest
// line. The original (the reverse flag +0x26 of the SBar is 0) shows newer
// lines with the up arrow, although the newest line is at the bottom of
// the box. -origui gives the original.
int Dtu_fix_msg_scroll = 1;

// Hot key boxes of the low resolution hot key screen (0x144680, 0x1446A8)
static short Hotkey_lo_x[10] = { 107, 107, 107, 107, 107, 161, 161, 161, 161, 161 };
static short Hotkey_lo_y[10] = { 13, 48, 82, 116, 150, 13, 48, 82, 116, 150 };
static short Hotkey_lo_qty_y[10] = { 32, 66, 100, 134, 169, 32, 66, 100, 134, 169 };

//------------------------------------------------------------------------
// Buttons (dispbutn.c). The records are from the lists at 0x14437F (high)
// and 0x1443A3 (low).

#define ACT_NONE		0
#define ACT_MODE_BASE	1		// ACT_MODE_BASE + mode: select the mode
#define ACT_REST		20
#define ACT_USE		21
#define ACT_DROP		22
#define ACT_SPELLS	23		// 0x37180: the next list of the inventory screen

typedef struct dtu_button {
	short	x, y, w, h;			// hot rectangle and draw rectangle
	short	x0, y0;				// image 0 in the sheet (at rest)
	short	x1, y1;				// image 1 in the sheet (pressed)
	short	action;				// at release
} dtu_button;

#define AM(m)	(ACT_MODE_BASE + (m))

static dtu_button Buttons_hi_main[] = {		// 0x14433E
	{ 16, 305, 43, 38,  74, 2,  29, 2, AM(DTU_IM_MAP) },
	{ 67, 306, 40, 37, 161, 2, 119, 2, AM(DTU_IM_INVENTORY) },
	{ 16, 348, 42, 38, 247, 2, 203, 2, AM(DTU_IM_STATS) },
	{ 67, 348, 41, 38, 334, 2, 291, 2, ACT_REST },
	{ 0 } };
static dtu_button Buttons_hi_inv[] = {			// 0x1442EE
	{ 454, 312, 39, 33,  74, 42,  29, 42, AM(DTU_IM_MAP) },
	{ 497, 312, 39, 33, 161, 42, 119, 42, AM(DTU_IM_MAIN) },
	{ 541, 312, 39, 33, 247, 42, 203, 42, AM(DTU_IM_STATS) },
	{ 584, 312, 39, 33, 334, 42, 291, 42, ACT_REST },
	{ 454, 354, 77, 33, 264, 78, 184, 78, ACT_USE },
	{ 548, 354, 74, 33, 107, 78,  29, 78, ACT_DROP },
	{  18, 324, 76, 32, 424, 78, 344, 78, ACT_SPELLS },	// 0x14408C
	{ 0 } };
static dtu_button Buttons_hi_map[] = {			// 0x14432A
	{ 454, 312, 77, 33, 426, 115, 346, 115, AM(DTU_IM_MAIN) },
	{ 546, 312, 77, 33, 108, 115,  29, 115, AM(DTU_IM_INVENTORY) },
	{ 454, 354, 77, 33, 521,  78, 521,  41, AM(DTU_IM_STATS) },
	{ 547, 354, 76, 33, 267, 115, 188, 115, ACT_REST },
	{ 0 } };
static dtu_button Buttons_hi_stats[] = {		// 0x144316
	{ 453, 312, 79, 34, 435, 151, 353, 151, AM(DTU_IM_MAP) },
	{ 545, 312, 79, 34, 111, 151,  29, 151, AM(DTU_IM_INVENTORY) },
	{ 454, 354, 78, 34, 521, 151, 521, 115, AM(DTU_IM_MAIN) },
	{ 547, 354, 77, 34, 273, 151, 193, 151, ACT_REST },
	{ 0 } };
static dtu_button Buttons_lo_main[] = {		// 0x1442DA
	{  8, 154, 19, 17,  30, 39,   9, 39, AM(DTU_IM_MAP) },
	{ 33, 154, 19, 17,  72, 39,  51, 39, AM(DTU_IM_INVENTORY) },
	{  8, 176, 19, 16, 114, 39,  93, 39, AM(DTU_IM_STATS) },
	{ 33, 176, 19, 16, 156, 39, 135, 39, ACT_REST },
	{ 0 } };
static dtu_button Buttons_lo_inv[] = {			// 0x144262 and 0x14428E
	{ 227, 156, 16, 15, 300, 39, 282, 39, AM(DTU_IM_MAP) },
	{ 249, 156, 17, 15, 423, 39, 318, 39, AM(DTU_IM_MAIN) },
	{ 274, 156, 16, 15, 462, 39, 443, 39, AM(DTU_IM_STATS) },
	{ 296, 156, 15, 15, 423, 20, 423,  2, ACT_REST },
	{ 273, 178, 39, 19, 521, 39, 480, 39, ACT_DROP },
	{ 227, 178, 39, 17, 514, 21, 480, 39, ACT_USE },
	// 0x143934: the hit test 0x99141 is a circle at (22,165) with radius
	// 15; the port takes the draw rectangle
	{   6, 150, 35, 35, 477,  2, 440,  2, ACT_SPELLS },
	{ 0 } };
static dtu_button Buttons_lo_map[] = {			// 0x1442C6
	{ 227, 156, 39, 17,  50, 2,   9, 2, AM(DTU_IM_MAIN) },
	{ 273, 156, 39, 17, 132, 2,  91, 2, AM(DTU_IM_INVENTORY) },
	{ 227, 177, 39, 17, 214, 2, 173, 2, AM(DTU_IM_STATS) },
	{ 273, 177, 39, 17, 296, 2, 255, 2, ACT_REST },
	{ 0 } };
static dtu_button Buttons_lo_stats[] = {		// 0x1442B2
	{ 227, 156, 39, 16,  50, 21,   9, 21, AM(DTU_IM_MAP) },
	{ 274, 156, 38, 16, 131, 21,  91, 21, AM(DTU_IM_INVENTORY) },
	{ 227, 178, 39, 16, 212, 21, 171, 21, AM(DTU_IM_MAIN) },
	{ 274, 178, 38, 16, 293, 21, 253, 21, ACT_REST },
	{ 0 } };

static dtu_button *Button_list;		// the list of the current mode, or NULL
static int Button_pressed = -1;		// the button that is held down, or -1

//------------------------------------------------------------------------
// State

static grs_bitmap *Interface_bm;		// picture of the current mode (0x1B5654)
static int Interface_entry = -1;
static grs_bitmap *Icons_bm;			// InGameIcons (0x1BD584)
static grs_bitmap *Cursor_src;		// 0x1BD51C
static int Cursor_size;
static grs_bitmap *Button_sheet;
// 0x9881C (high resolution only, called by 0x343FC): HRISBTN.PCH, a 96x80
// PCX file. PatchBtnDn (0x142EEB) is its part at (2,2), PatchBtnUp
// (0x142EEF) its part at (2,36), each 76x32, the pixels as they are (no
// palette match). The port keeps the whole picture.
static grs_bitmap Patch_bm;
static int Patch_ok;
static grs_bitmap *Portrait_bm;
static int Portrait_loaded = -1;
static int Saved_mode = DTU_IM_MAIN;	// 0x13B61A

static dtu_rect View;					// the view on the screen (h is the rendered height)
static fix Saved_aspect;

// The message box (0x1BD47C: 40 lines, line 0 is the newest)
#define MSG_LINES		40
#define MSG_LENGTH	160
static char Msg_text[MSG_LINES][MSG_LENGTH];
static ubyte Msg_color[MSG_LINES];
static int Msg_count = 0;
static int Msg_first = 0;				// the line at the bottom of the box

static ubyte Key_was_down[256];
static int Inv_held = -1;					// see the inventory screen
static int Inv_held_spell = -1;			// the spell that the cursor holds (a reference of list 1 or 2), or -1
static int Inv_browse = 0;				// the list of the inventory screen, see there
static int next_browse(int b);
static int Mouse_was_down = 0;

//------------------------------------------------------------------------
// Drawing helpers. All coordinates are screen coordinates. The current
// canvas is the screen.

static int in_rect(int x, int y, int rx, int ry, int rw, int rh)
{
	return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static int rgb6(int r, int g, int b)
{
	return gr_find_closest_color(r, g, b);
}

static void blit(grs_bitmap *src, int sx, int sy, int w, int h, int dx, int dy, int masked)
{
	grs_bitmap *dest = &grd_curcanv->cv_bitmap;

	if (!src) return;
	if (sx < 0 || sy < 0 || sx + w > src->bm_w || sy + h > src->bm_h) return;
	if (dx < 0) { sx -= dx; w += dx; dx = 0; }
	if (dy < 0) { sy -= dy; h += dy; dy = 0; }
	if (dx + w > dest->bm_w) w = dest->bm_w - dx;
	if (dy + h > dest->bm_h) h = dest->bm_h - dy;
	if (w <= 0 || h <= 0) return;
	if (masked)
		gr_bm_ubitbltm(w, h, dx, dy, sx, sy, src, dest);
	else {
		gr_bm_ubitblt(w, h, dx, dy, sx, sy, src, dest);
		dtu_hdui_blit(src, sx, sy, w, h, dest, dx, dy, w, h);	// ASCENT: the upscaled picture
	}
}

static void fill(int x, int y, int w, int h, int color)
{
	gr_setcolor(color);
	gr_rect(x, y, x + w - 1, y + h - 1);
}

// The restore function of an area: high resolution copies the interface
// picture (0x9C38F), which the frame already drew. Low resolution fills
// the area with the clear color, black for all areas (0x9C1F2).
static void area_restore(int x, int y, int w, int h)
{
	if (!Dtu_hires)
		fill(x, y, w, h, rgb6(0, 0, 0));
}

// Area type 1 (0x9B95E): the string centered in the area
static void area_text(int x, int y, int w, int h, char *s, int color)
{
	int sw, sh, aw;

	area_restore(x, y, w, h);
	if (!s || !*s || !GAME_FONT) return;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(color, -1);
	gr_get_string_size(s, &sw, &sh, &aw);
	gr_string(x + (w - sw) / 2, y + (h - GAME_FONT->ft_h) / 2, s);
}

// Area type 0 (0x9BD8A): an icon of the sheet. Icon 0 is "no icon".
// 0x36E3F: 8 icons in a row.
static void area_icon(int x, int y, int icon)
{
	int size = SC(16);

	area_restore(x, y, size, size);
	if (icon <= 0 || icon >= 256 || !Icons_bm) return;
	blit(Icons_bm, (icon & 7) * size, (icon / 8) * size, size, size, x, y, 1);
}

// The border function of an area (0x9C890)
static void area_border(int x, int y, int w, int h, int color, int t)
{
	fill(x, y, w, t, color);
	fill(x, y + h - t, w, t, color);
	fill(x, y + t, t, h - 2 * t, color);
	fill(x + w - t, y + t, t, h - 2 * t, color);
}

// Area type 3 (0x9B241): the hit points. The color is from 0x34530.
static void area_hit_points(int x, int y, int w, int h, int hp, int max_hp)
{
	char text[40];
	int percent, color;

	if (max_hp < 1) max_hp = 1;
	percent = hp * 100 / max_hp;
	if (percent <= 25)
		color = rgb6(63, 0, 0);
	else if (percent >= 51)
		color = rgb6(0, 63, 0);
	else
		color = rgb6(63, 63, 0);
	if (hp < 1)
		strcpy(text, "DEAD");
	else
		sprintf(text, "%d / %d", hp, max_hp);
	area_text(x, y, w, h, text, color);
}

//------------------------------------------------------------------------
// The values that the screen shows

static char *player_name(void)
{
	if (Dtu_char.created && Dtu_char.name[0])
		return Dtu_char.name;
	return Players[Player_num].callsign;
}

// Player record +0x53 and +0x9F of the original
static int player_hit_points(void)
{
	return dtu_char_hit_points();
}

static int player_max_hit_points(void)
{
	return dtu_char_max_hit_points();
}

// The icon of an entry of the inventory (byte 2 of the item record of the
// original), 0: none.
static int entry_icon(int index)
{
	if (index < 0 || index >= Dtu_n_inventory || Dtu_inventory[index].icon < 0)
		return 0;
	return Dtu_inventory[index].icon;
}

// The item in the hand (player record +0x1AD7 of the original). A spell in
// the hand is the reference of the hand in the executable.
static int current_item_icon(void)
{
	if (Dtu_spell_current >= 0)
		return dtu_spell_icon(Dtu_spell_current);
	return entry_icon(Dtu_char.current.n);
}

// 0xB7487: the text under the icon of an entry. A spell of a spell list:
// "Lvl %d" (high resolution) or "L-%d" (low resolution) with the spell
// level. An item (a weapon, armor, a misc item, a scroll): nothing for 0
// or 1, "Many" or "+++" for -1 (no limit), else the count. Misc 102 (the
// Scroll) has no text, another type "???".
static void item_count_text(int index, char *text)
{
	dtu_inv_item *e;

	text[0] = 0;
	if (index < 0 || index >= Dtu_n_inventory)
		return;
	e = &Dtu_inventory[index];
	if (e->type == OBJ_MISC && e->id == 102)
		return;
	if (e->type != OBJ_WEAPON && e->type != OBJ_ARMOR && e->type != OBJ_MISC && e->type != OBJ_SPELL) {
		strcpy(text, "???");
		return;
	}
	if (e->count == 0 || e->count == 1)
		return;
	if (e->count < 0)
		strcpy(text, Dtu_hires ? "Many" : "+++");
	else
		sprintf(text, "%d", e->count);
}

static void spell_level_text(int spell, char *text)
{
	sprintf(text, Dtu_hires ? LT("Lvl %d") : LT("L-%d"), dtu_spell_level(spell));
}

// A text of the string table of the game
static char *gtext(int n, char *def)
{
	if (n >= 0 && n < N_TEXT_STRINGS && Text_string[n] && Text_string[n][0])
		return Text_string[n];
	return def;
}

// Area type 2 (0x9B761) with x 0 and y 0x8000: the string at the left edge
static void area_text_left(int x, int y, int w, int h, char *s, int color)
{
	area_restore(x, y, w, h);
	if (!s || !*s || !GAME_FONT) return;
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(color, -1);
	gr_string(x, y + (h - GAME_FONT->ft_h) / 2, s);
}

//------------------------------------------------------------------------
// Scroll bars (displays.c, docs/re/ingame-ui.md, "Scroll bars")

static int Sbar_active = 0;			// the scroll bar took the mouse button in this frame
static int Sbar_grab = 0;			// ASCENT: the press began on the scroll bar
static fix Sbar_repeat_time = 0;	// ASCENT: the next repeat of a held arrow

// SBar +0x2A: x and y of the up arrow, the width of the bar, the height
// from the top of the up arrow to the bottom of the down arrow minus 1
static void sbar_whole(dtu_sbar *s, dtu_rect *r)
{
	r->x = s->up.x;
	r->y = s->up.y;
	r->w = s->bar.w;
	r->h = s->dn.y + s->dn.h - 1 - s->up.y;
}

// The part at a point of the whole rectangle (0x363D4 tests them in this
// order): 0 the up arrow, 1 the down arrow, 2 the bar, -1 none.
static int sbar_part(dtu_sbar *s, int mx, int my)
{
	if (in_rect(mx, my, s->up.x, s->up.y, s->up.w, s->up.h)) return 0;
	if (in_rect(mx, my, s->dn.x, s->dn.y, s->dn.w, s->dn.h)) return 1;
	if (in_rect(mx, my, s->bar.x, s->bar.y, s->bar.w, s->bar.h)) return 2;
	return -1;
}

// 0x363D4 for one part, with the reverse flag 0 (both creations): the up
// arrow takes the step off the first entry (not below 0), the down arrow
// adds it (not above the maximum), a place in the bar sets the first entry
// in proportion, in 16.16 numbers as the original (it can be odd).
static void sbar_click(dtu_sbar *s, int part, int my, int step, int *first, int max)
{
	if (part == 0) {
		*first -= step;
		if (*first < 0) *first = 0;
	} else if (part == 1) {
		*first += step;
		if (*first > max) *first = max;
	} else if (part == 2) {
		int range = s->bar.h - s->slider.h;
		if (range > 0 && max > 0) {
			long long q = ((long long) range << 16) / max;
			if (q == 0) q = 1;
			*first = (int) ((((long long) (my - s->bar.y)) << 16) / q);
		} else
			*first = 0;
		if (*first < 0) *first = 0;
		if (*first > max) *first = max;
	}
}

// ASCENT: an arrow acts at the press, then repeats after 0.4 s ten times a
// second while the button is down. The original acts in each frame while
// the button is down (Dtu_ui_key_repeat 1).
static int sbar_arrow_acts(int pressed)
{
	fix now;

	if (Dtu_ui_key_repeat)
		return 1;
	now = timer_get_fixed_seconds();
	if (pressed)
		Sbar_repeat_time = now + (2 * F1_0) / 5;
	else if (now >= Sbar_repeat_time)
		Sbar_repeat_time = now + F1_0 / 10;
	else
		return 0;
	return 1;
}

// A masked copy out of the interface picture (color 255 is transparent),
// also in the upscaled picture
static void sbar_blit_masked(dtu_rect *src, int w, int h, int dx, int dy)
{
	blit(Interface_bm, src->x, src->y, w, h, dx, dy, 1);
	if (Interface_bm)
		dtu_hdui_blit_key(Interface_bm, src->x, src->y, w, h, &grd_curcanv->cv_bitmap, dx, dy, w, h, 255);
}

// 0x3654C (SBar, position p, count c): the bar, the slider in it (c > 0),
// and the arrows: an arrow that the mouse holds shows its pressed image.
static void sbar_draw_bar(dtu_sbar *s, int p, int c, int active, int mx, int my)
{
	blit(Interface_bm, s->bar.x, s->bar.y, s->bar.w, s->bar.h, s->bar.x, s->bar.y, 0);
	if (c > 0) {
		int range = s->bar.h - s->slider.h, y;
		if (p > c) p = c;
		if (p <= 0)
			y = 0;
		else if (p >= c - 1)
			y = range;
		else {
			long long q = ((long long) range << 16) / c;
			y = (int) (((long long) p * q) >> 16);
		}
		sbar_blit_masked(&s->slider, s->slider.w, s->slider.h, s->bar.x, s->bar.y + y);
	}
	// 0x36693: the pressed images while the scroll bar has the button (the
	// original draws them at its update after the click), else 0x36704:
	// both arrows at rest, masked
	if (active && in_rect(mx, my, s->up.x, s->up.y, s->up.w, s->up.h)) {
		blit(Interface_bm, s->up_pressed.x, s->up_pressed.y, s->up.w, s->up.h, s->up.x, s->up.y, 0);
		blit(Interface_bm, s->dn_rest.x, s->dn_rest.y, s->dn.w, s->dn.h, s->dn.x, s->dn.y, 0);
	} else if (active && in_rect(mx, my, s->dn.x, s->dn.y, s->dn.w, s->dn.h)) {
		blit(Interface_bm, s->up_rest.x, s->up_rest.y, s->up.w, s->up.h, s->up.x, s->up.y, 0);
		blit(Interface_bm, s->dn_pressed.x, s->dn_pressed.y, s->dn.w, s->dn.h, s->dn.x, s->dn.y, 0);
	} else {
		sbar_blit_masked(&s->up_rest, s->up.w, s->up.h, s->up.x, s->up.y);
		sbar_blit_masked(&s->dn_rest, s->dn.w, s->dn.h, s->dn.x, s->dn.y);
	}
}

//------------------------------------------------------------------------
// Message box (0x33F55 init, 0x34682 draw)

static int msg_visible_lines(void)
{
	return (Msg_rect[Dtu_hires].h - 6) / 10;
}

static void msg_insert(char *line, int color)
{
	int i;

	for (i = MSG_LINES - 1; i > 0; i--) {
		strcpy(Msg_text[i], Msg_text[i - 1]);
		Msg_color[i] = Msg_color[i - 1];
	}
	strncpy(Msg_text[0], line, MSG_LENGTH - 1);
	Msg_text[0][MSG_LENGTH - 1] = 0;
	Msg_color[0] = color;
	if (Msg_count < MSG_LINES) Msg_count++;
}

// ASCENT: the color of the next message, or -1. See
// dtu_display_add_trigger_message().
static int Msg_next_color = -1;

void dtu_display_add_message(char *text)
{
	char line[MSG_LENGTH];
	int max_w, sw, sh, aw, len, last_space, color;
	char *p = text;

	if (!Dtu_display_active || !GAME_FONT || !text) return;
	p = text = dtu_tr(text);		// ASCENT: the text of the code in the language (dtulang.h)
	if (strstr(text, "{sex:")) {
		// ASCENT: the tokens {sex:} (dtulang.h) of a text that a caller made
		static char resolved[1024];
		snprintf(resolved, sizeof(resolved), "%s", text);
		dtu_lang_sex_text(resolved);
		p = text = resolved;
	}
	// The color of a message is the first byte of the line in the original.
	// The callers were not traced: gray is a guess.
	color = rgb6(48, 48, 48);
	if (Msg_next_color >= 0)
		color = Msg_next_color;
	max_w = Msg_rect[Dtu_hires].w - 4;
	gr_set_curfont(GAME_FONT);
	while (*p) {
		len = 0;
		last_space = -1;
		while (p[len] && p[len] != '\n' && len < MSG_LENGTH - 1) {
			line[len] = p[len];
			line[len + 1] = 0;
			gr_get_string_size(line, &sw, &sh, &aw);
			if (sw > max_w && len > 0) break;
			if (p[len] == ' ') last_space = len;
			len++;
		}
		if (p[len] && p[len] != '\n' && last_space > 0)
			len = last_space;
		line[len] = 0;
		msg_insert(line, color);
		p += len;
		while (*p == ' ' || *p == '\n') p++;
	}
	Msg_first = 0;
}

// The message of a trigger (0x94F5B): 0x347A2 with the number of the
// player, which gives the color of the player (table at 0x13B970, player
// 0 is 15,15,23, each value times 2), then 0x34983, which adds a line
// with one space.
void dtu_display_add_trigger_message(char *text)
{
	if (!Dtu_display_active || !GAME_FONT || !text) return;
	Msg_next_color = rgb6(30, 30, 46);
	dtu_display_add_message(text);
	msg_insert(" ", Msg_next_color);
	Msg_next_color = -1;
}

static void msg_draw(int mx, int my)
{
	dtu_rect *r = &Msg_rect[Dtu_hires];
	dtu_sbar *sb = &Msg_sbar[Dtu_hires];
	int n = msg_visible_lines(), i, line;

	// 0x346CA: black over the box and the width of the scroll bar
	fill(r->x, r->y, r->w + sb->bar.w, r->h, rgb6(0, 0, 0));
	if (!GAME_FONT) return;
	gr_set_curfont(GAME_FONT);
	for (i = 0; i < n; i++) {
		line = Msg_first + n - 1 - i;
		if (line < 0 || line >= Msg_count) continue;
		gr_set_fontcolor(Msg_color[line], -1);
		gr_string(r->x + 2, r->y + 3 + 10 * i, Msg_text[line]);
	}
	// 0x3475B: with more lines than the box shows, 0x3654C draws the scroll
	// bar with the first line (0 if negative) and the count minus 3.
	if (Msg_count > n) {
		int max = Msg_count - n;
		if (Dtu_fix_msg_scroll)
			sbar_draw_bar(sb, max - Msg_first, max + 1, Sbar_active, mx, my);
		else
			sbar_draw_bar(sb, Msg_first < 0 ? 0 : Msg_first, Msg_count - 3, Sbar_active, mx, my);
	}
}

// 0x2FF58: the mouse button is down in this frame, in mode 0 (the flag
// 0x13AD43). In the whole rectangle of the scroll bar: with more lines than
// the box shows, 0x363D4 with the step 1 and the maximum count minus the
// visible lines on the first line (0x13AE24). In each frame while the
// button is down, as the scroll bar of the inventory list. Returns 1 if the
// scroll bar takes the click.
static int msg_sbar_input(int mx, int my, int pressed)
{
	dtu_sbar *s = &Msg_sbar[Dtu_hires];
	dtu_rect r;
	int n = msg_visible_lines(), max = Msg_count - n, part, first;

	Sbar_active = 0;
	sbar_whole(s, &r);
	if (!in_rect(mx, my, r.x, r.y, r.w, r.h))
		return 0;
	// ASCENT: a press that began at another place does not scroll
	if (pressed)
		Sbar_grab = 1;
	if (!Sbar_grab && !Dtu_ui_key_repeat)
		return 0;
	if (Msg_count <= n)
		return 1;
	Sbar_active = 1;
	part = sbar_part(s, mx, my);
	if (part < 0 || (part < 2 && !sbar_arrow_acts(pressed)))
		return 1;
	first = Dtu_fix_msg_scroll ? max - Msg_first : Msg_first;
	sbar_click(s, part, my, 1, &first, max);
	Msg_first = Dtu_fix_msg_scroll ? max - first : first;
	return 1;
}

// ASCENT: the mouse wheel over the message box or its scroll bar: older
// lines for a step away from the user. The original (DOS) has no wheel.
static void msg_wheel(int mx, int my, int steps)
{
	dtu_rect *r = &Msg_rect[Dtu_hires];
	int n = msg_visible_lines();

	if (!steps || !in_rect(mx, my, r->x, r->y, r->w + Msg_sbar[Dtu_hires].bar.w, r->h))
		return;
	Msg_first += steps;
	if (Msg_first > Msg_count - n) Msg_first = Msg_count - n;
	if (Msg_first < 0) Msg_first = 0;
}

//------------------------------------------------------------------------
// Modes

static int mode_is_screen(int mode)
{
	return mode >= DTU_IM_INVENTORY && mode <= DTU_IM_HOTKEYS;
}

static int mode_has_interface(int mode)
{
	return mode == DTU_IM_MAIN || mode_is_screen(mode);
}

// The tables at 0x13B63B (high) and 0x13B65F (low)
static int mode_picture(int mode)
{
	static int hi[9] = { 63, 63, 63, 65, 64, 67, 65, 63, 63 };
	static int lo[9] = { 57, 57, 57, 59, 58, 61, 60, 57, 57 };
	return Dtu_hires ? hi[mode] : lo[mode];
}

static dtu_button *mode_buttons(int mode)
{
	switch (mode) {
	case DTU_IM_MAIN:			return Dtu_hires ? Buttons_hi_main : Buttons_lo_main;
	case DTU_IM_INVENTORY:	return Dtu_hires ? Buttons_hi_inv : Buttons_lo_inv;
	case DTU_IM_MAP:			return Dtu_hires ? Buttons_hi_map : Buttons_lo_map;
	case DTU_IM_STATS:		return Dtu_hires ? Buttons_hi_stats : Buttons_lo_stats;
	case DTU_IM_HOTKEYS:		return Dtu_hires ? Buttons_hi_inv : Buttons_lo_inv;
	}
	return NULL;
}

// 0x142F18 (high) and 0x142F28 (low): the rectangle of the map in the
// circle of the map screen (0x340D5). The map is drawn there, then the part
// of the picture that is not color 255 (the frame around the circle) is
// drawn over it (0x344ED).
static void map_rect(dtu_rect *r)
{
	if (Dtu_hires) {
		r->x = 45; r->y = 25; r->w = 345; r->h = 350;
	} else {
		r->x = 30; r->y = 20; r->w = 160; r->h = 160;
	}
}

static void draw_map(void)
{
	dtu_rect r;

	map_rect(&r);
	dtu_automap_frame(grd_curcanv, r.x, r.y, r.w, r.h);
	if (Interface_bm)
		blit(Interface_bm, r.x, r.y, r.w, r.h, r.x, r.y, 1);
}

// 0x4052F (init_game_screen): the view rectangle of a mode
static void mode_view(int mode, dtu_rect *v)
{
	int sw = Dtu_hires ? 640 : 320;

	if (mode == DTU_IM_MAIN) {
		v->x = SC(63); v->y = SC(5); v->w = SC(252); v->h = 139;
	} else if (mode_is_screen(mode)) {
		v->x = SC(225); v->y = SC(6); v->w = SC(88); v->h = 78;
	} else if (mode == DTU_IM_LETTERBOX) {
		// 0x40686: 140 lines in the middle of the screen
		v->x = 0; v->w = sw; v->h = 140;
		v->y = ((Dtu_hires ? 480 : 200) - (Dtu_hires ? 280 : 140)) / 2;
	} else {
		// 0x40649: 320x200, or 640x239 with each line shown twice
		v->x = 0; v->y = 0; v->w = sw; v->h = Dtu_hires ? 239 : 200;
	}
}

void dtu_display_init_cockpit(void)
{
	mode_view(Dtu_interface_mode, &View);
	game_init_render_sub_buffers(0, 0, View.w, View.h);
	Game_window_w = View.w;
	Game_window_h = View.h;
	// 0x40772: the call 0xE89A8 with the center of the view. The function
	// was not read: it puts the mouse there (guess).
	mouse_set_limits(0, 0, Dtu_hires ? 639 : 319, Dtu_hires ? 479 : 199);
	mouse_set_pos(View.x + View.w / 2, View.y + View.h * (Dtu_hires ? 2 : 1) / 2);
}

// 0x407EB and 0x40403
void dtu_display_select_mode(int mode)
{
	dtu_button *b;
	int entry;

	if (mode < 0 || mode > DTU_IM_LETTERBOX || mode == 1 || mode == 2)
		return;
	Dtu_interface_mode = mode;
	Dtu_automap_active = 0;		// 0x40922: a new map screen follows the player
	Button_pressed = -1;
	Button_list = NULL;
	Inv_held = -1;				// 0x361BF: the cursor lets go of the item
	Inv_held_spell = -1;

	if (mode_has_interface(mode)) {
		entry = mode_picture(mode);
		if (entry != Interface_entry || !Interface_bm) {
			if (Interface_bm) gr_free_bitmap(Interface_bm);
			Interface_bm = febgnd_load(entry);
			Interface_entry = entry;
			// 0x98D21: each button draws its image 0 into the picture
			if (Interface_bm && Button_sheet)
				for (b = mode_buttons(mode); b && b->w; b++)
					if (b->x0 + b->w <= Button_sheet->bm_w && b->y0 + b->h <= Button_sheet->bm_h
							&& b->x + b->w <= Interface_bm->bm_w && b->y + b->h <= Interface_bm->bm_h)
						gr_bm_ubitblt(b->w, b->h, b->x, b->y, b->x0, b->y0, Button_sheet, Interface_bm);
		}
		Button_list = mode_buttons(mode);
	}
	dtu_display_init_cockpit();
}

// 0x408A2, 0x408F5, 0x4093A: the same key or button leaves the screen
static void toggle_screen(int mode)
{
	if (Dtu_interface_mode == mode || (mode == DTU_IM_INVENTORY && Dtu_interface_mode == DTU_IM_HOTKEYS)) {
		dtu_display_select_mode(Saved_mode);
		return;
	}
	if (!mode_is_screen(Dtu_interface_mode))
		Saved_mode = Dtu_interface_mode;
	dtu_display_select_mode(mode);
}

static void do_action(int action)
{
	if (action >= ACT_MODE_BASE && action <= ACT_MODE_BASE + DTU_IM_LETTERBOX) {
		int mode = action - ACT_MODE_BASE;
		if (mode == DTU_IM_MAIN) {
			// 0x36042: the button of the open screen selects mode 0
			dtu_display_select_mode(DTU_IM_MAIN);
		} else {
			if (!mode_is_screen(Dtu_interface_mode))
				Saved_mode = Dtu_interface_mode;
			dtu_display_select_mode(mode);
		}
		return;
	}
	switch (action) {
	case ACT_REST:			// 0x360D5
		dtu_char_rest();
		break;
	case ACT_USE:			// 0x3614B: the held item, or the item in the hand
		if (Inv_held_spell >= 0)
			break;				// 0x36189: a held spell is not used
		if (Inv_held >= 0) {
			dtu_char_use(Inv_held);
			Inv_held = -1;
		} else
			dtu_char_use_current();
		break;
	case ACT_DROP:			// 0x35FD6: the held item (not a spell: 0x13ADB0 is 0)
		if (Inv_held >= 0) {
			dtu_char_drop(Inv_held);
			Inv_held = -1;
		}
		break;
	case ACT_SPELLS:		// 0x37180: the next list (items, mage spells, cleric spells)
		Inv_browse = next_browse(Inv_browse);
		break;
	}
}

//------------------------------------------------------------------------
// Init

int dtu_display_init(void)
{
	Dtu_display_active = 0;
	if (!febgnd_open())
		return 0;
	// 0x3420C: the icons are entry 23 (low) or 24 (high), the cursor is the
	// top left of entry 154 (15x15) or 153 (31x31)
	Icons_bm = febgnd_load(Dtu_hires ? 24 : 23);
	Cursor_src = febgnd_load(Dtu_hires ? 153 : 154);
	Cursor_size = Dtu_hires ? 31 : 15;
	// 0x98B78 and 0xAEEC4: the button sheet of the language. The tables at
	// 0x145718 (high) and 0x145734 (low) give entry 70 + language and
	// 78 + language. ASCENT: the language is English (0).
	Button_sheet = febgnd_load(Dtu_hires ? 70 : 78);
	if (Dtu_hires && !Patch_ok) {
		char path[512];
		memset(&Patch_bm, 0, sizeof(Patch_bm));
		Patch_ok = dtu_find_file("HRISBTN.PCH", path, sizeof(path))
			&& pcx_read_bitmap(path, &Patch_bm, BM_LINEAR, NULL) == PCX_ERROR_NONE
			&& Patch_bm.bm_w >= 78 && Patch_bm.bm_h >= 68;
		if (!Patch_ok)
			dtu_log("HRISBTN.PCH could not be read: the button SPELLS keeps its images");
	}
	if (!Icons_bm || !Cursor_src || !Button_sheet)
		return 0;
	if (Icons_bm) Icons_bm->bm_flags |= BM_FLAG_TRANSPARENT;
	Interface_entry = -1;
	if (FindArg("-origui"))
		Dtu_fix_msg_scroll = 0;
	Msg_count = 0;
	Msg_first = 0;
	memset(Key_was_down, 0, sizeof(Key_was_down));
	Dtu_display_active = 1;
	dtu_display_select_mode(DTU_IM_MAIN);
	if (!Interface_bm) {
		Dtu_display_active = 0;
		return 0;
	}
	mouse_set_relative(Dtu_immersive);
	return 1;
}

//------------------------------------------------------------------------
// Render

void dtu_display_begin_render(void)
{
	Saved_aspect = grd_curscreen->sc_aspect;
	// A rendered line covers two lines of the 640x480 screen.
	if (Dtu_display_active && Dtu_hires)
		grd_curscreen->sc_aspect = Saved_aspect * 2;
}

void dtu_display_end_render(void)
{
	grd_curscreen->sc_aspect = Saved_aspect;
}

// ASCENT: the rectangle of the last draw_view, for dtu_display_view_again.
static int View_dest[4], View_dest_set;

void dtu_display_view_again(void)
{
	if (View_dest_set)
		g3_gpu_view_dest(View_dest[0], View_dest[1], View_dest[2], View_dest[3]);
}

// Copies the rendered view to the screen. High resolution shows each line
// twice (0x40729).
static void draw_view(void)
{
	grs_bitmap *src = &VR_render_sub_buffer[0].cv_bitmap;
	grs_bitmap *dest = &grd_curcanv->cv_bitmap;
	int y, w = View.w, h = View.h;

	if (w > src->bm_w) w = src->bm_w;
	if (h > src->bm_h) h = src->bm_h;
	if (View.x + w > dest->bm_w) w = dest->bm_w - View.x;
	// ASCENT: the GPU image of the view (gpu3d.c) shows in this rectangle
	g3_gpu_view_dest(View.x, View.y, w, Dtu_hires ? 2 * h : h);
	View_dest[0] = View.x;
	View_dest[1] = View.y;
	View_dest[2] = w;
	View_dest[3] = Dtu_hires ? 2 * h : h;
	View_dest_set = 1;
	for (y = 0; y < h; y++) {
		ubyte *s = src->bm_data + y * src->bm_rowsize;
		if (Dtu_hires) {
			if (View.y + 2 * y + 1 >= dest->bm_h) break;
			memcpy(dest->bm_data + (View.y + 2 * y) * dest->bm_rowsize + View.x, s, w);
			memcpy(dest->bm_data + (View.y + 2 * y + 1) * dest->bm_rowsize + View.x, s, w);
		} else {
			if (View.y + y >= dest->bm_h) break;
			memcpy(dest->bm_data + (View.y + y) * dest->bm_rowsize + View.x, s, w);
		}
	}
}

static void load_portrait(void)
{
	int n = Dtu_player_portrait;

	if (n < 0 || n > 31) n = 0;
	if (Portrait_loaded == n + (Dtu_hires ? 100 : 0)) return;
	if (Portrait_bm) gr_free_bitmap(Portrait_bm);
	// 0x3627E: entries 87.. (100x90, high) and 119.. (50x45, low)
	Portrait_bm = febgnd_load((Dtu_hires ? 87 : 119) + n);
	Portrait_loaded = n + (Dtu_hires ? 100 : 0);
}

// The areas of the main screen (0x996D8) and their update (0x324DA)
static void draw_main_areas(void)
{
	int px = SC(5), py = SC(17), pw = SC(50), ph = SC(45);

	// portrait, area type 5 (0x9BB7F): the picture centered in the area
	load_portrait();
	if (Portrait_bm)
		blit(Portrait_bm, 0, 0, Portrait_bm->bm_w, Portrait_bm->bm_h,
			px + (pw - Portrait_bm->bm_w) / 2, py + (ph - Portrait_bm->bm_h) / 2, 0);

	// name: text color (48,48,48). Low resolution prints it at x 1
	// (area type 2), high resolution centers it.
	if (Dtu_hires)
		area_text(SC(5), SC(67), SC(50), SC(8), player_name(), rgb6(48, 48, 48));
	else {
		area_restore(5, 67, 50, 8);
		if (GAME_FONT) {
			gr_set_curfont(GAME_FONT);
			gr_set_fontcolor(rgb6(48, 48, 48), -1);
			gr_string(5 + 1, 67 + (8 - GAME_FONT->ft_h) / 2, player_name());
		}
	}

	area_hit_points(SC(5), SC(79), SC(50), SC(8), player_hit_points(), player_max_hit_points());
	area_icon(SC(22), SC(92), current_item_icon());
}

// The hot key boxes (0x995BE create, 0x329CB update). High resolution has
// them on the main screen and on the screens, low resolution on the hot
// key screen only.
static int hot_keys_visible(void)
{
	if (!mode_has_interface(Dtu_interface_mode))
		return 0;
	return Dtu_hires ? (Dtu_interface_mode != DTU_IM_HOTKEYS) : (Dtu_interface_mode == DTU_IM_HOTKEYS);
}

static void hot_key_pos(int i, int *x, int *y, int *qy)
{
	if (Dtu_hires) {
		*x = 14 + 55 * i; *y = 411; *qy = 449;		// 0x1446D0, 0x1446F8
	} else {
		*x = Hotkey_lo_x[i]; *y = Hotkey_lo_y[i]; *qy = Hotkey_lo_qty_y[i];
	}
}

static void draw_hot_keys(void)
{
	int i, size = SC(16);
	char text[16];

	if (!hot_keys_visible())
		return;
	for (i = 0; i < DTU_N_HOT_KEYS; i++) {
		int x, y, qy, n = dtu_char_hot_key_item(i), spell = dtu_char_hot_key_spell(i);

		hot_key_pos(i, &x, &y, &qy);
		// 0x329CB: a box with no entry is cleared. A box with an entry
		// (0x35863: list 0 items, 1 mage spells, 2 cleric spells) shows
		// byte 2 of the entry as the icon and the text of 0xB7487. The box
		// of the reference in the hand gets a border of thickness 1 in the
		// color (63,63,0).
		if (spell >= 0) {
			area_icon(x, y, dtu_spell_icon(spell));
			spell_level_text(spell, text);
		} else if (n >= 0) {
			area_icon(x, y, entry_icon(n));
			item_count_text(n, text);
		} else {
			area_icon(x, y, 0);
			area_text(x, qy, size, SC(6), "", rgb6(32, 0, 0));
			continue;
		}
		area_text(x, qy, size, SC(6), text, rgb6(32, 0, 0));
		if (dtu_char_hot_key_in_hand(i))
			area_border(x, y, size, size, rgb6(63, 63, 0), 1);
	}
}

//------------------------------------------------------------------------
// The inventory screen (modes 3 and 6)

// The list shows ten entries in two columns (0x144540 high, 0x144590 low).
static int Inv_first = 0;				// the first entry that is shown (0x13AD9F)
static int Inv_sel = 0;					// ASCENT: the entry that the keys act on
// Inv_held (above): the entry that the cursor holds (0x13ADAF, 0x1BD694), or -1
static int Inv_known = 0;
static char Desc_text[80];				// the text of the description bar (area 0x3BC524)

static short Inv_lo_y[5] = { 2, 31, 61, 90, 119 };

static void inv_cell_pos(int cell, int *x, int *y, int *qy)
{
	int col = cell & 1, row = cell >> 1;

	if (Dtu_hires) {
		*x = col ? 51 : 9; *y = 9 + 58 * row; *qy = *y + 36;
	} else {
		*x = col ? 26 : 5; *y = Inv_lo_y[row]; *qy = *y + 20;
	}
}

// The boxes of the paper doll (0x1443E0 x, 0x144440 y), in the order of
// DTU_SLOT_. [0] low resolution, [1] high resolution.
static short Slot_pos[2][DTU_N_SLOTS][2] = {
	{ { 67, 92 }, { 193, 51 }, { 67, 52 }, { 191, 72 }, { 191, 92 }, { 67, 72 }, { 73, 123 }, { 187, 123 },
	  { 72, 162 }, { 186, 162 }, { 164, 11 }, { 193, 11 } },
	{ { 133, 184 }, { 383, 103 }, { 133, 103 }, { 383, 143 }, { 383, 183 }, { 133, 143 }, { 141, 244 }, { 374, 246 },
	  { 132, 326 }, { 374, 317 }, { 328, 22 }, { 384, 22 } },
};
// "Put ... here" of the string table (0x3147D to 0x3167C)
static short Slot_text[DTU_N_SLOTS] = { 787, 665, 666, 667, 668, 669, 670, 671, 672, 673, 785, 786 };

static int inventory_mode(void)
{
	return Dtu_interface_mode == DTU_IM_INVENTORY || Dtu_interface_mode == DTU_IM_HOTKEYS;
}

// The list of the inventory screen ("BrowseItemType", 0x13ADB1): 0 the
// items, 1 the mage spells, 2 the cleric spells (0x36F2A). Each list keeps
// its first entry (0x13AD9F, 0x13ADA5, 0x13ADA9). Inv_first (above) is the
// one of the items. Inv_browse (above) is 0x13ADB1.
static int Spell_first[2];
static int Spell_sel[2];					// ASCENT: the spell that the keys act on

static int has_class(int c)
{
	return Dtu_char.created && (Dtu_char.pa.class1 == c || Dtu_char.pa.class2 == c);
}

// 0x3713C: the list after the current one. Mage spells for a mage, cleric
// spells for a cleric (0x35A2B, 0x35A09), then the items.
static int next_browse(int b)
{
	if (b == 1)
		return has_class(DTU_CLASS_CLERIC) ? 2 : 0;
	if (b == 2)
		return 0;
	if (has_class(DTU_CLASS_MAGE)) return 1;
	if (has_class(DTU_CLASS_CLERIC)) return 2;
	return 0;
}

static int browse_spell(int n)
{
	if (Inv_browse < 1 || Inv_browse > 2 || n < 0 || n >= Dtu_spell_book_n[Inv_browse - 1])
		return -1;
	return Dtu_spell_book[Inv_browse - 1][n];
}

static void inv_show_entry(int index)
{
	if (index < 0) return;
	Inv_first = (index / 10) * 10;
}

// 0xBB867 (load) and 0x8BC69 (new character): the list starts at entry 0.
// ASCENT: the port finds a new entry by the count (inv_check), so the count
// of a loaded list is known here and does not move the list to its last
// page (that hid the first ten entries after a load).
void dtu_display_inventory_reset(void)
{
	Inv_first = 0;
	Inv_sel = 0;
	Inv_held = -1;
	Inv_known = Dtu_n_inventory;
	// ASCENT: the item list and the first spells (guess: 0xBB867 was not
	// seen to set 0x13ADB1)
	Inv_browse = 0;
	Inv_held_spell = -1;
	Spell_first[0] = Spell_first[1] = 0;
	Spell_sel[0] = Spell_sel[1] = 0;
}

static void inv_check(void)
{
	// 0xB6926: a new entry shows its page
	if (Dtu_n_inventory > Inv_known)
		inv_show_entry(Dtu_n_inventory - 1);
	Inv_known = Dtu_n_inventory;
	if (Inv_held >= Dtu_n_inventory) Inv_held = -1;
	if (Inv_sel >= Dtu_n_inventory) Inv_sel = Dtu_n_inventory - 1;
	if (Inv_sel < 0) Inv_sel = 0;
	if (Inv_first >= Dtu_n_inventory) Inv_first = Dtu_n_inventory > 0 ? ((Dtu_n_inventory - 1) / 10) * 10 : 0;
	{
		int b;
		for (b = 0; b < 2; b++) {
			int n = Dtu_spell_book_n[b];
			if (Spell_first[b] >= n) Spell_first[b] = n > 0 ? ((n - 1) / 10) * 10 : 0;
			if (Spell_sel[b] >= n) Spell_sel[b] = n - 1;
			if (Spell_sel[b] < 0) Spell_sel[b] = 0;
		}
	}
	if (Inv_browse && !next_browse(0))
		Inv_browse = 0;
}

static void draw_inventory(void)
{
	int cell, slot, size = SC(16), x, y, qy;
	char text[16];

	if ((Dtu_hires || Dtu_interface_mode == DTU_IM_INVENTORY) && Inv_browse)
		for (cell = 0; cell < 10; cell++) {
			// A spell list: the icon and "Lvl %d" of each spell (0x327D7
			// with 0xB7487 and the list number)
			int n = Spell_first[Inv_browse - 1] + cell, spell = browse_spell(n);

			inv_cell_pos(cell, &x, &y, &qy);
			if (spell < 0) {
				area_icon(x, y, 0);
				area_text(x, qy, size, SC(6), "", rgb6(32, 0, 0));
				continue;
			}
			area_icon(x, y, spell == Inv_held_spell ? 0 : dtu_spell_icon(spell));
			spell_level_text(spell, text);
			area_text(x, qy, size, SC(6), text, rgb6(32, 0, 0));
			if (n == Spell_sel[Inv_browse - 1])
				area_border(x - 1, y - 1, size + 2, size + 2, rgb6(48, 48, 48), 1);
		}
	else if (Dtu_hires || Dtu_interface_mode == DTU_IM_INVENTORY)
		for (cell = 0; cell < 10; cell++) {
			int n = Inv_first + cell;

			inv_cell_pos(cell, &x, &y, &qy);
			if (n >= Dtu_n_inventory) {
				area_icon(x, y, 0);
				area_text(x, qy, size, SC(6), "", rgb6(32, 0, 0));
				continue;
			}
			// The entry that the cursor holds is drawn at the cursor.
			area_icon(x, y, n == Inv_held ? 0 : entry_icon(n));
			item_count_text(n, text);
			area_text(x, qy, size, SC(6), text, rgb6(32, 0, 0));
			// ASCENT: the mark of the entry that the keys act on. The original
			// has the mouse only.
			if (n == Inv_sel)
				area_border(x - 1, y - 1, size + 2, size + 2, rgb6(48, 48, 48), 1);
		}
	if (Dtu_hires ? 1 : (Dtu_interface_mode == DTU_IM_INVENTORY))
		for (slot = 0; slot < DTU_N_SLOTS; slot++) {
			int n = Dtu_char.slot[slot].n;

			x = Slot_pos[Dtu_hires][slot][0];
			y = Slot_pos[Dtu_hires][slot][1];
			area_icon(x, y, (n >= 0 && n != Inv_held) ? entry_icon(n) : 0);
		}
}

// Sets the text of the description bar for the place under the cursor.
static void inv_describe(int mx, int my)
{
	int cell, slot, size = SC(16), x, y, qy, i;

	Desc_text[0] = 0;
	if (hot_keys_visible())
		for (i = 0; i < DTU_N_HOT_KEYS; i++) {
			hot_key_pos(i, &x, &y, &qy);
			if (mx >= x && my >= y && mx < x + size && my < y + size) {
				if (dtu_char_hot_key_spell(i) >= 0)
					strncpy(Desc_text, dtu_spell_name(dtu_char_hot_key_spell(i)), sizeof(Desc_text) - 1);
				else if (dtu_char_hot_key_item(i) >= 0)
					strncpy(Desc_text, Dtu_inventory[dtu_char_hot_key_item(i)].name, sizeof(Desc_text) - 1);
			}
		}
	if (!inventory_mode())
		return;
	for (cell = 0; cell < 10; cell++) {
		inv_cell_pos(cell, &x, &y, &qy);
		if (!(mx >= x && my >= y && mx < x + size && my < y + size))
			continue;
		if (Inv_browse) {
			int spell = browse_spell(Spell_first[Inv_browse - 1] + cell);
			if (spell >= 0)
				strncpy(Desc_text, dtu_spell_name(spell), sizeof(Desc_text) - 1);
		} else if (Inv_first + cell < Dtu_n_inventory)
			strncpy(Desc_text, Dtu_inventory[Inv_first + cell].name, sizeof(Desc_text) - 1);
	}
	for (slot = 0; slot < DTU_N_SLOTS; slot++) {
		x = Slot_pos[Dtu_hires][slot][0];
		y = Slot_pos[Dtu_hires][slot][1];
		if (mx >= x && my >= y && mx < x + size && my < y + size) {
			int n = Dtu_char.slot[slot].n;
			if (n >= 0 && n < Dtu_n_inventory)
				strncpy(Desc_text, Dtu_inventory[n].name, sizeof(Desc_text) - 1);
			else
				strncpy(Desc_text, gtext(Slot_text[slot], ""), sizeof(Desc_text) - 1);
		}
	}
	// ASCENT: with the cursor on no item the bar shows the entry of the keys
	if (!Desc_text[0] && Inv_held >= 0 && Inv_held < Dtu_n_inventory)
		strncpy(Desc_text, Dtu_inventory[Inv_held].name, sizeof(Desc_text) - 1);
	if (!Desc_text[0] && Inv_held_spell >= 0)
		strncpy(Desc_text, dtu_spell_name(Inv_held_spell), sizeof(Desc_text) - 1);
	if (!Desc_text[0] && Inv_browse && browse_spell(Spell_sel[Inv_browse - 1]) >= 0)
		strncpy(Desc_text, dtu_spell_name(browse_spell(Spell_sel[Inv_browse - 1])), sizeof(Desc_text) - 1);
	if (!Desc_text[0] && !Inv_browse && Inv_sel < Dtu_n_inventory)
		strncpy(Desc_text, Dtu_inventory[Inv_sel].name, sizeof(Desc_text) - 1);
	Desc_text[sizeof(Desc_text) - 1] = 0;
	{
		// ASCENT: the name of an item or a spell in the language (dtulang.h)
		char *t = dtu_tr_name(Desc_text);
		if (t != Desc_text)
			snprintf(Desc_text, sizeof(Desc_text), "%s", t);
	}
}

// A click of the mouse on the inventory screen (0x2FB10, 0x30650 and the
// next blocks). Returns 1 if the click was on an item place.
static int inv_click(int mx, int my)
{
	int cell, slot, size = SC(16), x, y, qy, i;

	if (hot_keys_visible())
		for (i = 0; i < DTU_N_HOT_KEYS; i++) {
			hot_key_pos(i, &x, &y, &qy);
			if (mx >= x && my >= y && mx < x + size && my < y + size) {
				// 0x3058E: the held item goes into the box. With no held item
				// a box that is not empty works like its key (0x3056E: 0x43A5A
				// with its tests).
				if (Inv_held >= 0) {
					dtu_char_set_hot_key(i, Inv_held);
					Inv_held = -1;
				} else if (Inv_held_spell >= 0) {
					// 0x304D6: the box takes the held spell if the character
					// can use it and no box holds it. Else the box works like
					// its key and the cursor keeps the spell.
					if (dtu_char_set_hot_key_spell(i, Inv_held_spell))
						Inv_held_spell = -1;
					else if (Dtu_char.hot_key[i].n >= 0)
						dtu_char_press_hot_key(i);
				} else if (Dtu_char.hot_key[i].n >= 0)
					dtu_char_press_hot_key(i);
				return 1;
			}
		}
	if (!inventory_mode())
		return 0;
	for (slot = 0; slot < DTU_N_SLOTS; slot++) {
		x = Slot_pos[Dtu_hires][slot][0];
		y = Slot_pos[Dtu_hires][slot][1];
		if (mx >= x && my >= y && mx < x + size && my < y + size) {
			// 0x3067C: a spell has no wear bits, no slot takes it
			if (Inv_held_spell >= 0)
				return 1;
			if (Inv_held >= 0) {
				// 0x3067C: the slot takes the held item if the wear bits fit
				if (dtu_char_equip(Inv_held, slot))
					Inv_held = -1;
			} else if (Dtu_char.slot[slot].n >= 0) {
				// 0x306C4: the cursor takes the item out of the slot
				Inv_held = Dtu_char.slot[slot].n;
				dtu_char_unequip(slot);
			}
			return 1;
		}
	}
	// The figure of the paper doll (0x1440EA and 0x144148 high, 0x1441A6 and
	// 0x144204 low, the press action 0x35ADF). ASCENT: the port takes a held
	// spell there (0x35CE5); a held item goes back to the list, as before.
	if (Inv_held_spell >= 0) {
		static short doll[2][2][4] = { { { 120, 12, 31, 29 }, { 101, 41, 67, 140 } },
			{ { 244, 24, 55, 57 }, { 210, 81, 122, 278 } } };
		for (i = 0; i < 2; i++) {
			short *r = doll[Dtu_hires][i];
			if (mx >= r[0] && my >= r[1] && mx < r[0] + r[2] && my < r[1] + r[3]) {
				// a spell that a box holds stays at the cursor (0x35D17)
				if (dtu_char_arm_spell(Inv_held_spell) || !dtu_spell_usable(Inv_held_spell))
					Inv_held_spell = -1;
				return 1;
			}
		}
	}
	for (cell = 0; cell < 10; cell++) {
		inv_cell_pos(cell, &x, &y, &qy);
		if (mx >= x && my >= y && mx < x + size && my < y + size) {
			int n = Inv_first + cell;
			if (Inv_browse) {
				// 0x3029E with a spell list: the cursor takes the spell (a
				// reference of list 1 or 2). A spell cannot be dropped
				// (0x13ADB0 is 0).
				int spell = browse_spell(Spell_first[Inv_browse - 1] + cell);
				if (Inv_held >= 0 || Inv_held_spell >= 0) {
					Inv_held = -1;
					Inv_held_spell = -1;
				} else if (spell >= 0) {
					Inv_held_spell = spell;
					Spell_sel[Inv_browse - 1] = Spell_first[Inv_browse - 1] + cell;
				}
				return 1;
			}
			if (Inv_held_spell >= 0)
				Inv_held_spell = -1;		// the spell goes back to its list
			else if (Inv_held >= 0)
				Inv_held = -1;				// the item goes back to the list
			else if (n < Dtu_n_inventory) {
				Inv_held = n;
				Inv_sel = n;
			}
			return 1;
		}
	}
	return 0;
}

//------------------------------------------------------------------------
// The scroll bar of the inventory list (SBar 0x1BD622). 0x3416C creates it
// for the modes 3 and 6 with the SBarDef at 0x143158 (high) and 0x143179
// (low); 0x36815 copies the images out of the interface picture.
// [0] low resolution, [1] high resolution.

static dtu_sbar Inv_sbar[2] = {
	{ { 46, 3, 7, 7 }, { 46, 139, 7, 7 }, { 9, 65, 7, 7 }, { 46, 10, 7, 129 },
	  { 30, 96, 7, 7 }, { 9, 96, 7, 7 }, { 30, 122, 7, 7 }, { 9, 122, 7, 7 } },
	{ { 90, 8, 19, 16 }, { 89, 277, 19, 16 }, { 451, 51, 17, 12 }, { 89, 24, 19, 255 },
	  { 89, 8, 19, 16 }, { 449, 13, 19, 16 }, { 89, 279, 19, 16 }, { 449, 32, 19, 16 } },
};

// 0x36F2A: the first entry and the count of the current list
static int *inv_list(int *count)
{
	if (Inv_browse >= 1 && Inv_browse <= 2) {
		*count = Dtu_spell_book_n[Inv_browse - 1];
		return &Spell_first[Inv_browse - 1];
	}
	*count = Dtu_n_inventory;
	return &Inv_first;
}

// The whole scroll bar of the inventory list
static void sbar_rect(dtu_rect *r)
{
	sbar_whole(&Inv_sbar[Dtu_hires], r);
}

// 0x3013D: after a change the first entry is at most the even count above
// the count, minus ten.
static void inv_list_clamp(int *first, int n)
{
	int max = ((n + 1) & ~1) - 10;

	if (*first > max) *first = max;
	if (*first < 0) *first = 0;
}

// The mouse button is down in this frame (0x3010C). Returns 1 if the
// scroll bar takes it; then the click does not reach the list or the paper
// doll. The original acts in each frame while the button is down: an arrow
// moves the list by one row (two entries) in each frame, a place in the bar
// sets the list there (so a drag in the bar moves the list). 0x363D4 with
// the step 2 and the maximum ((count - 1) / 10) * 10, then the clamp of
// 0x3013D.
static int sbar_input(int mx, int my, int pressed)
{
	dtu_sbar *s = &Inv_sbar[Dtu_hires];
	dtu_rect r;
	int n, part, *first;

	Sbar_active = 0;
	// 0x30121: not with an item or a spell at the cursor, and only while
	// the scroll bar exists (0x13AD44)
	if (!inventory_mode() || Inv_held >= 0 || Inv_held_spell >= 0)
		return 0;
	sbar_rect(&r);
	if (!in_rect(mx, my, r.x, r.y, r.w, r.h))
		return 0;
	// ASCENT: a press that began at another place does not scroll
	if (pressed)
		Sbar_grab = 1;
	if (!Sbar_grab && !Dtu_ui_key_repeat)
		return 0;
	first = inv_list(&n);
	if (n <= 10)
		return 1;
	Sbar_active = 1;
	part = sbar_part(s, mx, my);
	if (part < 0 || (part < 2 && !sbar_arrow_acts(pressed)))
		return 1;
	sbar_click(s, part, my, 2, first, ((n - 1) / 10) * 10);
	inv_list_clamp(first, n);
	return 1;
}

// ASCENT: the mouse wheel over the list or the scroll bar moves the list by
// one row (two entries) for each step. The original (DOS) has no wheel.
static void sbar_wheel(int mx, int my, int steps)
{
	dtu_rect r;
	int n, *first;

	if (!steps || !inventory_mode())
		return;
	sbar_rect(&r);
	if (!in_rect(mx, my, 0, 0, r.x + r.w, r.y + r.h))
		return;
	first = inv_list(&n);
	if (n <= 10)
		return;
	*first -= 2 * steps;
	inv_list_clamp(first, n);
}

// 0x3654C, called by 0x324DA with the first entry and the count minus 8
// (0 for ten entries or less: no slider)
static void sbar_draw(int mx, int my)
{
	int n, first = *inv_list(&n);

	sbar_draw_bar(&Inv_sbar[Dtu_hires], first, n > 10 ? n - 8 : 0, Sbar_active, mx, my);
}


//------------------------------------------------------------------------
// The statistics screen (mode 5). The areas are created at 0x99B11 with the
// rectangles of the tables at 0x144938 to 0x144B40, and printed at 0x32BE0.

static void draw_stats(void)
{
	static short label_lo[6][2] = { { 11, 101 }, { 11, 109 }, { 11, 117 }, { 43, 101 }, { 43, 109 }, { 43, 117 } };
	static short label_hi[6][2] = { { 20, 200 }, { 20, 216 }, { 20, 232 }, { 87, 200 }, { 87, 216 }, { 87, 232 } };
	static char *label_def[6] = { "STR:", "DEX:", "CON:", "INT:", "WIS:", "CHA:" };
	static char *class_def[4] = { "Cleric", "Mage", "Fighter", "Thief" };
	static short class_text[4] = { 642, 643, 644, 647 };
	static char *skill_def[5] = { "Hide In Shadows", "Move Silently", "Open Locks", "Find/remove traps", "Climb Walls" };
	dtu_player_attr *pa = &Dtu_char.pa;
	int gray = rgb6(50, 50, 50), white = rgb6(63, 63, 63);
	int value[6], skill[5], i, x, y, w, h;
	char text[80];

	if (!Dtu_char.created)
		return;

	// The portrait (0x99ADD, area type 5): low (15,10) 50x45, high (32,22)
	// 100x90 (the words at 0x144728).
	load_portrait();
	if (Portrait_bm)
		blit(Portrait_bm, 0, 0, Portrait_bm->bm_w, Portrait_bm->bm_h,
			(Dtu_hires ? 32 : 15) + (SC(50) - Portrait_bm->bm_w) / 2, (Dtu_hires ? 22 : 10) + (SC(45) - Portrait_bm->bm_h) / 2, 0);

	// alignment, race, class and level (0x32BE0 to 0x32E3B)
	x = Dtu_hires ? 20 : 11;
	w = Dtu_hires ? 108 : 60;
	h = Dtu_hires ? 13 : 7;
	i = (pa->alignment < 0 || pa->alignment > 5) ? 3 : pa->alignment;
	area_text_left(x, Dtu_hires ? 131 : 63, w, h, gtext(636 + i, ""), gray);
	if (pa->race == 0 && (pa->flags & DTU_PF_HALF_ELF)) {
		strncpy(text, gtext(917, "Half-elf"), sizeof(text) - 1);
		text[sizeof(text) - 1] = 0;
		if (text[0] >= 'a' && text[0] <= 'z') text[0] -= 'a' - 'A';
	} else
		strcpy(text, gtext(649 + ((pa->race < 0 || pa->race > 4) ? 5 : pa->race), "Human"));
	area_text_left(x, Dtu_hires ? 147 : 72, w, h, text, gray);
	if (pa->class1 >= 0 && pa->class1 <= 3) {
		sprintf(text, Dtu_hires ? LT("Lvl %d %s") : " %d %s", pa->class1_level + 1, gtext(class_text[pa->class1], class_def[pa->class1]));
		area_text_left(x, Dtu_hires ? 163 : 81, w, h, text, gray);
	}
	if (pa->class2 >= 0 && pa->class2 <= 3) {
		sprintf(text, Dtu_hires ? LT("Lvl %d %s") : " %d %s", pa->class2_level + 1, gtext(class_text[pa->class2], class_def[pa->class2]));
		area_text_left(x, Dtu_hires ? 179 : 90, w, h, text, gray);
	}

	// the six abilities (0x32E3B to 0x330C3)
	value[0] = pa->strength; value[1] = pa->dexterity; value[2] = pa->constitution;
	value[3] = pa->intelligence; value[4] = pa->wisdom; value[5] = pa->charisma;
	for (i = 0; i < 6; i++) {
		x = Dtu_hires ? label_hi[i][0] : label_lo[i][0];
		y = Dtu_hires ? label_hi[i][1] : label_lo[i][1];
		area_text_left(x, y, Dtu_hires ? 26 : 19, h, gtext(655 + i, label_def[i]), gray);
		sprintf(text, "%d", value[i]);
		area_text_left(x + (Dtu_hires ? 27 : 19), y, Dtu_hires ? 14 : 9, h, text, white);
	}

	// the thief skills (0x330C3 with 0x32172). The colors are a guess.
	skill[0] = pa->hide_in_shadows; skill[1] = pa->move_silently; skill[2] = pa->open_locks;
	skill[3] = pa->find_traps; skill[4] = pa->climb_walls;
	h = Dtu_hires ? 20 : 11;
	for (i = 0; i < 5; i++) {
		int thief = (pa->class1 == DTU_CLASS_THIEF || pa->class2 == DTU_CLASS_THIEF);

		y = Dtu_hires ? 62 + 20 * i : 30 + 11 * i;
		area_text_left(Dtu_hires ? 218 : 108, y, Dtu_hires ? 120 : 75, h,
			(i == 3 && !Dtu_hires) ? LT("Find Traps") : gtext(678 + i, skill_def[i]), thief ? white : gray);
		sprintf(text, "%d%c", skill[i], '%');
		area_text_left(Dtu_hires ? 338 : 183, y, Dtu_hires ? 50 : 24, h, text, thief ? white : gray);
	}
	// The spell tables of the screen (areas 0x3BA134 to 0x3BA584) are not in
	// the port.
}

// The character information of the screens (0x99449): name, hit points,
// armor class, experience, gold.
static void draw_info_areas(void)
{
	int x = SC(227), w = SC(81), h = SC(6);
	char text[80];

	area_text(x, SC(97), w, h, player_name(), rgb6(48, 48, 48));
	area_hit_points(x, SC(108), w, h, player_hit_points(), player_max_hit_points());
	// 0x3BC18C: "%s %d" with the armor class (0xB0A92). That the label is
	// string 675 ("AC:") is a guess from the order of the strings.
	sprintf(text, "%s %d", gtext(675, "AC:"), dtu_char_armor_class());
	area_text(x, SC(119), w, h, text, rgb6(63, 63, 0));
	// 0x3BC3B4, area type 6: the experience (the result of 0x37253)
	sprintf(text, LT("%d Exp"), dtu_char_next_level(NULL, NULL));
	area_text(x, SC(130), w, h, text, rgb6(63, 63, 0));
	// 0x32657: "%s %d" with string 674 ("Gold:") and the gold
	sprintf(text, "%s %d", gtext(674, "Gold:"), Dtu_gold);
	area_text(x, SC(141), w, h, text, rgb6(63, 48, 32));
	// The item description bar (area 0x3BC524, red text)
	area_text(SC(5), SC(188), SC(208), SC(7), Desc_text, rgb6(63, 0, 0));
}

static void draw_buttons(void)
{
	dtu_button *b;

	if (!Button_list) return;
	// 0x98F38, the release handler of SPELLS (high resolution): after the
	// action 0x37192 sets 0x13B0BB when a spell list is shown and the next
	// press leads back to the items. Then the images of the button (+0x56 at
	// rest, +0x5A pressed) are PatchBtnUp and PatchBtnDn; else the images of
	// the sheet come back (0x9902F).
	if (Dtu_hires && Patch_ok && Inv_browse && !next_browse(Inv_browse))
		for (b = Button_list; b->w; b++)
			if (b->action == ACT_SPELLS) {
				int pressed = Button_pressed >= 0 && &Button_list[Button_pressed] == b;
				blit(&Patch_bm, 2, pressed ? 2 : 36, b->w, b->h, b->x, b->y, 0);
				if (pressed)
					return;
			}
	if (Button_pressed < 0) return;
	b = &Button_list[Button_pressed];
	blit(Button_sheet, b->x1, b->y1, b->w, b->h, b->x, b->y, 0);
}

// ASCENT: the pixels under the last cursor and of the cursor itself, so
// that a menu can take the cursor out of the frame (dtu_display_hide_cursor)
#define CUR_SAVE	33
static ubyte Cur_under[CUR_SAVE * CUR_SAVE], Cur_over[CUR_SAVE * CUR_SAVE];
static ubyte *Cur_data;			// the first pixel of the saved square, NULL: none
static int Cur_rowsize, Cur_w, Cur_h;

static void cursor_copy(ubyte *buf, int to_screen)
{
	int y;

	for (y = 0; y < Cur_h; y++) {
		if (to_screen)
			memcpy(Cur_data + y * Cur_rowsize, buf + y * CUR_SAVE, Cur_w);
		else
			memcpy(buf + y * CUR_SAVE, Cur_data + y * Cur_rowsize, Cur_w);
	}
}

static void draw_cursor_main(int mx, int my);

// The cursor (dtumouse.c): the hot spot is the center of the picture.
static void draw_cursor(int mx, int my)
{
	grs_bitmap *dest = &grd_curcanv->cv_bitmap;
	int x0 = mx - CUR_SAVE / 2, y0 = my - CUR_SAVE / 2, x1 = x0 + CUR_SAVE, y1 = y0 + CUR_SAVE;

	// ASCENT: save the square under the cursor
	if (x0 < 0) x0 = 0;
	if (y0 < 0) y0 = 0;
	if (x1 > dest->bm_w) x1 = dest->bm_w;
	if (y1 > dest->bm_h) y1 = dest->bm_h;
	Cur_data = NULL;
	if (x1 > x0 && y1 > y0) {
		Cur_data = dest->bm_data + y0 * dest->bm_rowsize + x0;
		Cur_rowsize = dest->bm_rowsize;
		Cur_w = x1 - x0;
		Cur_h = y1 - y0;
		cursor_copy(Cur_under, 0);
	}
	draw_cursor_main(mx, my);
	if (Cur_data)
		cursor_copy(Cur_over, 0);
}

// ASCENT: a menu opens over the play screen: the cursor of the play screen
// goes out of the frame (the menu shows its own pointer). Nothing happens
// if the pixels of the cursor are not in the frame any more.
void dtu_display_hide_cursor(void)
{
	int y;

	if (!Cur_data)
		return;
	for (y = 0; y < Cur_h; y++)
		if (memcmp(Cur_data + y * Cur_rowsize, Cur_over + y * CUR_SAVE, Cur_w))
			break;
	if (y == Cur_h)
		cursor_copy(Cur_under, 1);
	Cur_data = NULL;
}

// ASCENT: the target hint (Dtu_target_hint, dtucombat.c): the colors of the
// cursor with a warm red tint. Each color is moved towards red (red stays,
// green times 0.62, blue times 0.48) and matched to the nearest color of
// the palette, so the frame stays 8-bit and the tint shows in both
// renderers. Color 255 (transparent) is not used.
static ubyte *hint_tint_map(void)
{
	static ubyte map[256], map_pal[768];
	static int map_ok;
	int c, j;

	if (map_ok && !memcmp(map_pal, gr_palette, sizeof(map_pal)))
		return map;
	memcpy(map_pal, gr_palette, sizeof(map_pal));
	for (c = 0; c < 256; c++) {
		int r = map_pal[c*3], g = map_pal[c*3+1] * 62 / 100, b = map_pal[c*3+2] * 48 / 100;
		int best = c, best_d = 0x7FFFFFFF;

		for (j = 0; j < 255; j++) {
			int dr = r - map_pal[j*3], dg = g - map_pal[j*3+1], db = b - map_pal[j*3+2];
			int d = dr*dr + dg*dg + db*db;
			if (d < best_d) {
				best_d = d;
				best = j;
			}
		}
		map[c] = (ubyte) best;
	}
	map_ok = 1;
	return map;
}

static void draw_cursor_main(int mx, int my)
{
	grs_bitmap *dest = &grd_curcanv->cv_bitmap;
	int x, y, half = Cursor_size / 2;
	ubyte *tint = NULL;

	// 0x3074A: the cursor that holds an item or a spell is its icon
	if (((Inv_held >= 0 && Inv_held < Dtu_n_inventory && entry_icon(Inv_held) > 0) ||
			(Inv_held_spell >= 0 && dtu_spell_icon(Inv_held_spell) > 0)) && Icons_bm) {
		int icon = Inv_held >= 0 ? entry_icon(Inv_held) : dtu_spell_icon(Inv_held_spell), size = SC(16);
		blit(Icons_bm, (icon & 7) * size, (icon / 8) * size, size, size, mx - size / 2, my - size / 2, 1);
		return;
	}
	if (!Cursor_src) return;
	if (dtu_combat_target_hint() && !dtu_dlg_is_open())
		tint = hint_tint_map();
	for (y = 0; y < Cursor_size; y++)
		for (x = 0; x < Cursor_size; x++) {
			int c = Cursor_src->bm_data[y * Cursor_src->bm_rowsize + x];
			int dx = mx - half + x, dy = my - half + y;
			// The draw code of the cursor was not read. The background of
			// the picture is color 255, the transparent color.
			if (c == 255) continue;
			if (dx < 0 || dy < 0 || dx >= dest->bm_w || dy >= dest->bm_h) continue;
			dest->bm_data[dy * dest->bm_rowsize + dx] = tint ? tint[c] : c;
		}
}

// ASCENT: the cursor of the play screen at the mouse position, for the
// conversation window (dtudlg.c), which draws over the interface.
void dtu_display_draw_cursor(void)
{
	int mx = 0, my = 0;

	if (!Dtu_display_active || Dtu_immersive)
		return;
	mouse_get_pos(&mx, &my);
	draw_cursor(mx, my);
}

// ASCENT: the mouse pointer of the menus (newmenu.c). The menus of the
// original have no pointer. It is the cursor picture of the play screen
// (FEBGND 153 for a screen of 640 pixels or more, 154 for 320x200), loaded
// with the palette of its entry. Each color is matched to the nearest
// color of the current palette, so the pointer shows in the palette of the
// game and of the front end. Color 255 is not used: it is transparent and
// shows the image of the GPU renderer.
void dtu_display_draw_pointer(grs_bitmap *dest, int mx, int my)
{
	static grs_bitmap *src[2];
	static ubyte src_pal[2][768], tried[2];
	static ubyte map_pal[768], map[2][256], map_ok[2];
	int hi = dest->bm_w >= 640 ? 1 : 0;
	int size = hi ? 31 : 15, half = size / 2, x, y;

	if (!tried[hi]) {
		tried[hi] = 1;
		src[hi] = dtu_febgnd_load(hi ? 153 : 154, src_pal[hi]);
	}
	if (!src[hi])
		return;
	if (memcmp(map_pal, gr_palette, sizeof(map_pal))) {
		memcpy(map_pal, gr_palette, sizeof(map_pal));
		map_ok[0] = map_ok[1] = 0;
	}
	if (!map_ok[hi]) {
		int c, j;
		for (c = 0; c < 256; c++) {
			int r = src_pal[hi][c*3], g = src_pal[hi][c*3+1], b = src_pal[hi][c*3+2];
			int best = 0, best_d = 0x7FFFFFFF;
			for (j = 0; j < 255; j++) {
				int dr = r - map_pal[j*3], dg = g - map_pal[j*3+1], db = b - map_pal[j*3+2];
				int d = dr*dr + dg*dg + db*db;
				if (d < best_d) {
					best_d = d;
					best = j;
				}
			}
			map[hi][c] = (ubyte) best;
		}
		map_ok[hi] = 1;
	}
	if (size > src[hi]->bm_w) size = src[hi]->bm_w;
	for (y = 0; y < size && y < src[hi]->bm_h; y++)
		for (x = 0; x < size; x++) {
			int c = src[hi]->bm_data[y * src[hi]->bm_rowsize + x];
			int dx = mx - half + x, dy = my - half + y;
			if (c == 255) continue;
			if (dx < 0 || dy < 0 || dx >= dest->bm_w || dy >= dest->bm_h) continue;
			dest->bm_data[dy * dest->bm_rowsize + dx] = map[hi][c];
		}
}

//------------------------------------------------------------------------
// Input

// Returns 1 if the key acts in this frame: at the press, or in each frame
// while it is down if Dtu_ui_key_repeat is set.
static int key_acts(int scancode)
{
	int down = keyd_pressed[scancode] != 0;
	int was = Key_was_down[scancode];
	// ASCENT: the presses since the last frame (repeats are not counted),
	// so that a press and a release within one frame also act once.
	int presses = key_down_count(scancode);

	Key_was_down[scancode] = down;
	if (Dtu_ui_key_repeat) return down || presses;
	return presses > 0 || (down && !was);
}

// ASCENT: the actions of the key table (kconfig.c) that the play screen
// handles
static const int Screen_actions[] = { DTU_ACT_INVENTORY, DTU_ACT_AUTOMAP, DTU_ACT_IMMERSIVE,
	DTU_ACT_REST, DTU_ACT_STATS, DTU_ACT_INFRAVISION };

// ASCENT: 1 if a key of the action acts in this frame (key_acts), or a
// mouse or joystick button of it went down.
static int action_acts(int act)
{
	static ubyte was[DTU_N_ACTIONS];
	int k0 = dtu_action_key(act, 0), k1 = dtu_action_key(act, 1), r = 0, d;

	if (k0 != 255) r |= key_acts(k0);
	if (k1 != 255) r |= key_acts(k1);
	d = dtu_action_down(act);
	if (d && (!was[act] || Dtu_ui_key_repeat)) r = 1;
	was[act] = d;
	return r;
}

int dtu_display_owns_key(int key)
{
	int i;

	if (!Dtu_display_active) return 0;
	// The map reads its keys (minus, equal, Escape) while it has the
	// controls (0x1893F)
	if (dtu_automap_owns_key(key)) return 0;
	// ASCENT: the keys of the play screen actions and the hot keys
	for (i = 0; i < (int) (sizeof(Screen_actions) / sizeof(Screen_actions[0])); i++)
		if (dtu_action_has_key(Screen_actions[i], key))
			return 1;
	for (i = 0; i < DTU_N_HOT_ACTIONS; i++)
		if (dtu_action_has_key(DTU_ACT_HOT_KEY_1 + i, key))
			return 1;
	switch (key) {
	case KEY_F3:
	case KEY_MINUS:					// 0x43193, also with Shift
	case KEY_SHIFTED + KEY_MINUS:
	case KEY_EQUAL:					// 0x432A8, also with Shift
	case KEY_SHIFTED + KEY_EQUAL:
		return 1;
	// The key switch of the executable (0x42F9C) has no Shift+F1 to Shift+F8
	// and no Shift+Enter (the stereo and head tracking keys of Descent 1),
	// and the number keys are the hot keys (0x430D1): they never select a
	// weapon of Descent 1, also when the hot key page gives the hot keys
	// other keys.
	case KEY_SHIFTED + KEY_F1: case KEY_SHIFTED + KEY_F2: case KEY_SHIFTED + KEY_F3:
	case KEY_SHIFTED + KEY_F4: case KEY_SHIFTED + KEY_F5: case KEY_SHIFTED + KEY_F6:
	case KEY_SHIFTED + KEY_F7: case KEY_SHIFTED + KEY_F8: case KEY_SHIFTED + KEY_ENTER:
	case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5:
	case KEY_6: case KEY_7: case KEY_8: case KEY_9: case KEY_0:
		return 1;
	case KEY_COMMA:
	case KEY_PERIOD:
	case KEY_ENTER:
	case KEY_U:
	case KEY_BACKSP:
		return inventory_mode() || Dtu_interface_mode == DTU_IM_STATS;
	}
	return 0;
}

// The keys 1 to 0 (0x430C9 to 0x4310A, the scan codes 2 to 11). With a held
// entry (0x31A46) the key puts it into the box with the tests of a click
// (0x31A55); only if the box is the one that was chosen last, 0x13AD15
// becomes -1 and the box is armed. With no held entry: 0x43A5A(box).
static void hot_key_key(int i)
{
	int ok, last = dtu_char_last_hot_key() == i;

	if (Inv_held >= 0) {
		dtu_char_set_hot_key(i, Inv_held);
		ok = dtu_char_hot_key_item(i) == Inv_held;
		if (ok) Inv_held = -1;
	} else if (Inv_held_spell >= 0) {
		ok = dtu_char_set_hot_key_spell(i, Inv_held_spell);
		if (ok) Inv_held_spell = -1;
	} else {
		dtu_char_press_hot_key(i);
		return;
	}
	if (ok && last) {
		dtu_char_forget_hot_key();
		dtu_char_press_hot_key(i);
	}
}

static void do_input(int mx, int my)
{
	int down, pressed, released, i;

	// While the conversation window is open the executable runs the reduced
	// frame 0x4391D: it draws the displays (0x324DA) but calls neither the
	// input of the displays (0x2FB10) nor the key table (controls_read_all),
	// so no key and no click of the interface acts. The port keeps the key
	// and button states current, so that nothing acts when it closes.
	if (dtu_dlg_is_open()) {
		static const int keys[] = { KEY_F3, KEY_EQUAL, KEY_MINUS, KEY_COMMA, KEY_PERIOD, KEY_ENTER, KEY_U, KEY_BACKSP };
		for (i = 0; i < (int) (sizeof(Screen_actions) / sizeof(Screen_actions[0])); i++)
			action_acts(Screen_actions[i]);
		for (i = 0; i < DTU_N_HOT_ACTIONS; i++)
			action_acts(DTU_ACT_HOT_KEY_1 + i);
		action_acts(DTU_ACT_PREV_HOT_KEY);
		action_acts(DTU_ACT_NEXT_HOT_KEY);
		for (i = 0; i < (int) (sizeof(keys) / sizeof(keys[0])); i++)
			key_acts(keys[i]);
		Mouse_was_down = !Dtu_immersive && (mouse_get_btns() & 1);
		Button_pressed = -1;
		Sbar_active = Sbar_grab = 0;
		return;
	}

	// ASCENT: the actions of the key table (kconfig.c): Inventory (0x408A2,
	// default I), Automap (0x408F5, Tab), Rest (0x360DA, R; the port adds
	// Z), Stats (0x4093A, the executable S, the port P), Immersive Mode
	// (0x54439, V), Infravision (0x54632, N).
	if (action_acts(DTU_ACT_INVENTORY)) toggle_screen(DTU_IM_INVENTORY);
	if (action_acts(DTU_ACT_AUTOMAP)) toggle_screen(DTU_IM_MAP);
	if (action_acts(DTU_ACT_REST)) do_action(ACT_REST);
	if (action_acts(DTU_ACT_STATS)) toggle_screen(DTU_IM_STATS);
	if (action_acts(DTU_ACT_INFRAVISION)) {
		// 0x54632: the race value (0x5464B to 0x54661: 5 with the half-elf
		// flag, else the race byte; the same byte and flag as the race
		// name of the statistics screen, 0x32C94 and 0x32CBB). A race
		// other than human, or the half-elf, has infravision. Without it
		// and without the Infravision effect (bit 15 of the effects of the
		// player object: the spell, the potion, the ring) TEX 819, else
		// the flag 0x14C4DA changes with TEX 820 or 821 and the bit 8 of
		// the byte 0x1AE of the player object (DTU_PS_INFRAVISION). The 3D
		// view is in dtuinfra.c.
		dtu_player_attr *pa = &Dtu_char.pa;
		int race = (pa->flags & DTU_PF_HALF_ELF) ? 5 : pa->race;
		char *text;
		if (race == 0 && !(ConsoleObject && (ConsoleObject->dtu.m_88 & DTU_FX_INFRAVISION)))
			text = Text_string[819];
		else {
			Dtu_player_state ^= DTU_PS_INFRAVISION;
			text = Text_string[(Dtu_player_state & DTU_PS_INFRAVISION) ? 820 : 821];
		}
		Msg_next_color = rgb6(30, 30, 46);		// 0x347A2, the color of the player
		dtu_display_add_message(text ? text : "");
		Msg_next_color = -1;
	}
	if (action_acts(DTU_ACT_IMMERSIVE)) {
		// ASCENT: 0x54439 toggles the flag at 0x13BAE0 and adds TEX 817
		// "Immersive mode is on." or TEX 818 "Immersive mode is off." with
		// 0x347A2 (the color of the player, no blank line). Its key gate
		// 0x545F5 lets the key act at most once in 36.4 clock ticks
		// (0.364 s): one short press gives one toggle. This is the only
		// handler of V: controls.c follows Dtu_immersive.
		char *text;
		Dtu_immersive = !Dtu_immersive;
		mouse_set_relative(Dtu_immersive);
		// 0x54470 to 0x544D7: off, the mouse goes to the middle of the view
		// (0xE89A8), as after a mode change (dtu_display_init_cockpit)
		if (!Dtu_immersive)
			mouse_set_pos(View.x + View.w / 2, View.y + View.h * (Dtu_hires ? 2 : 1) / 2);
		text = Text_string[Dtu_immersive ? 817 : 818];
		Msg_next_color = rgb6(30, 30, 46);
		dtu_display_add_message(text ? text : (Dtu_immersive ? LN("Immersive mode is on.") : LN("Immersive mode is off.")));
		Msg_next_color = -1;
	}
	// The key switch of the game (0x42F9C): F3 (0x43043) calls 0x40C3A in
	// mode 7, else 0x40974. The key "=" (also with Shift, 0x432A8) calls
	// 0x40974, the key "-" (also with Shift, 0x43193) calls 0x40C3A.
	// 0x40974: mode 0 goes to mode 7, a screen (modes 3 to 6) goes to
	// mode 0. 0x40C3A: mode 7 goes to mode 0. Both set the saved mode
	// (0x13B61A) to 0. Their HUD message (0x4F31A, TEX 738 "Press F3 to
	// return to Window mode") is not shown: the HUD draw function of the
	// executable has no caller.
	// The switch tests the key code with its modifiers: Alt+F3 is the load
	// (0x432C2), Ctrl and Alt with these keys do not change the screen.
	{
		int f3 = key_acts(KEY_F3), larger = key_acts(KEY_EQUAL), smaller = key_acts(KEY_MINUS);
		if (keyd_pressed[KEY_LALT] || keyd_pressed[KEY_RALT] || keyd_pressed[KEY_LCTRL] || keyd_pressed[KEY_RCTRL])
			f3 = larger = smaller = 0;
		if (keyd_pressed[KEY_LSHIFT] || keyd_pressed[KEY_RSHIFT])
			f3 = 0;						// Shift+F3 (0x13D) is not in the switch
		// the map takes the plain keys minus and equal while it has the
		// controls (0x18944)
		if (Dtu_automap_active && !keyd_pressed[KEY_LSHIFT] && !keyd_pressed[KEY_RSHIFT])
			larger = smaller = 0;
		if (f3 && Dtu_interface_mode == DTU_IM_FULL_SCREEN)
			smaller = 1, f3 = 0;
		if (f3 || larger) {
			if (Dtu_interface_mode == DTU_IM_MAIN) {
				dtu_display_select_mode(DTU_IM_FULL_SCREEN);
				Saved_mode = DTU_IM_MAIN;
			} else if (mode_is_screen(Dtu_interface_mode)) {
				dtu_display_select_mode(DTU_IM_MAIN);
				Saved_mode = DTU_IM_MAIN;
			}
		} else if (smaller && Dtu_interface_mode == DTU_IM_FULL_SCREEN) {
			dtu_display_select_mode(DTU_IM_MAIN);
			Saved_mode = DTU_IM_MAIN;
		}
	}

	// The keys 1 to 0 arm the item of a hot key box (help text 760 of the
	// game). The key code of the original was not read. ASCENT: the keys
	// of the hot key page of the key table (kconfig.c), 1 to 0 by default.
	// ASCENT: not while the conversation window waits for a choice: the
	// keys 1 to 9 choose a reply then (dtudlg.c)
	for (i = 0; i < DTU_N_HOT_KEYS && i < DTU_N_HOT_ACTIONS; i++)
		if (action_acts(DTU_ACT_HOT_KEY_1 + i) && !dtu_dlg_is_waiting())
			hot_key_key(i);
	// ASCENT: the previous and the next hot key that holds an item (the
	// shoulder buttons of the gamepad by default)
	{
		int prev = action_acts(DTU_ACT_PREV_HOT_KEY), next = action_acts(DTU_ACT_NEXT_HOT_KEY);
		if (prev != next) {
			int h = -1, k, step = next ? 1 : DTU_N_HOT_KEYS - 1;
			for (k = 0; k < DTU_N_HOT_KEYS; k++)
				if (dtu_char_hot_key_in_hand(k))
					h = k;
			if (h < 0 && !next)
				h = 0;
			for (k = 1; k <= DTU_N_HOT_KEYS; k++) {
				int j = (h + k * step + 10 * DTU_N_HOT_KEYS) % DTU_N_HOT_KEYS;
				if (Dtu_char.hot_key[j].n >= 0 && j != h) {
					dtu_char_press_hot_key(j);
					break;
				}
			}
		}
	}
	// ASCENT: keys for the inventory screen. The original has the mouse
	// only. Comma and period choose an entry, Enter wears it or takes it
	// off or takes it in the hand, U uses it, Backspace drops it.
	if (inventory_mode() && Inv_browse) {
		// ASCENT: a spell list: comma and period choose a spell, Enter arms
		// it as the figure of the paper doll does (0x35CE5)
		int b = Inv_browse - 1;
		if (key_acts(KEY_COMMA) && Spell_sel[b] > 0) Spell_first[b] = (--Spell_sel[b] / 10) * 10;
		if (key_acts(KEY_PERIOD) && Spell_sel[b] < Dtu_spell_book_n[b] - 1) Spell_first[b] = (++Spell_sel[b] / 10) * 10;
		if (key_acts(KEY_ENTER) && browse_spell(Spell_sel[b]) >= 0) dtu_char_arm_spell(browse_spell(Spell_sel[b]));
		key_acts(KEY_U); key_acts(KEY_BACKSP);
	} else if (inventory_mode()) {
		if (key_acts(KEY_COMMA) && Inv_sel > 0) inv_show_entry(--Inv_sel);
		if (key_acts(KEY_PERIOD) && Inv_sel < Dtu_n_inventory - 1) inv_show_entry(++Inv_sel);
		if (key_acts(KEY_ENTER) && Inv_sel < Dtu_n_inventory) {
			int slot = dtu_char_slot_of(Inv_sel);
			if (slot >= 0) dtu_char_unequip(slot);
			else dtu_char_equip_auto(Inv_sel);
		}
		if (key_acts(KEY_U) && Inv_sel < Dtu_n_inventory) dtu_char_use(Inv_sel);
		if (key_acts(KEY_BACKSP) && Inv_sel < Dtu_n_inventory) dtu_char_drop(Inv_sel);
	} else if (Dtu_interface_mode == DTU_IM_STATS) {
		// ASCENT: the spell list of the statistics screen: comma and period
		// choose a spell, Enter takes it in the hand (dtuspell.c)
		int prev = key_acts(KEY_COMMA), next = key_acts(KEY_PERIOD), take = key_acts(KEY_ENTER);
		dtu_spell_screen_keys(prev, next, take);
		key_acts(KEY_U); key_acts(KEY_BACKSP);
	} else {
		// keep the key state current, so that a held key does not act when
		// the screen opens
		key_acts(KEY_COMMA); key_acts(KEY_PERIOD); key_acts(KEY_ENTER); key_acts(KEY_U); key_acts(KEY_BACKSP);
	}
	inv_check();
	inv_describe(mx, my);
	// ASCENT: the mouse wheel (the conversation window reads it while it
	// is open)
	if (!dtu_dlg_is_open()) {
		int wheel = plat_mouse_wheel();
		if (!Dtu_immersive) {
			sbar_wheel(mx, my, wheel);
			if (Dtu_interface_mode == DTU_IM_MAIN)
				msg_wheel(mx, my, wheel);
		}
	}

	down = !Dtu_immersive && (mouse_get_btns() & 1);
	pressed = down && (!Mouse_was_down || Dtu_ui_key_repeat);
	released = !down && Mouse_was_down;
	Mouse_was_down = down;
	if (Dtu_immersive || !mode_has_interface(Dtu_interface_mode)) {
		Button_pressed = -1;
		Sbar_active = Sbar_grab = 0;
		return;
	}

	// 0x3010C: the scroll bar of the inventory list, in each frame while
	// the button is down. A click that it takes goes no further.
	if (!down)
		Sbar_active = Sbar_grab = 0;
	else if (Button_pressed < 0 && sbar_input(mx, my, pressed))
		return;
	else if (Button_pressed < 0 && Dtu_interface_mode == DTU_IM_MAIN && msg_sbar_input(mx, my, pressed))
		return;

	if (pressed) {
		// 0x98E99: the pressed button shows its other image
		for (i = 0; Button_list && Button_list[i].w; i++)
			if (in_rect(mx, my, Button_list[i].x, Button_list[i].y, Button_list[i].w, Button_list[i].h)) {
				Button_pressed = i;
				if (Dtu_ui_key_repeat)
					do_action(Button_list[i].action);
				return;
			}
		// 0x305C9: in the map screen a click in the map gives it the
		// controls, a click outside it takes them back
		if (Dtu_interface_mode == DTU_IM_MAP) {
			dtu_rect r;
			map_rect(&r);
			Dtu_automap_active = in_rect(mx, my, r.x, r.y, r.w, r.h);		}
		if (inv_click(mx, my))
			return;
		if (Inv_held >= 0)
			Inv_held = -1;					// a click on no place: the item goes back to the list
		Inv_held_spell = -1;
	}
	if (released && Button_pressed >= 0 && Button_list) {
		// 0x98F38: the action runs at the release
		dtu_button *b = &Button_list[Button_pressed];
		int action = in_rect(mx, my, b->x, b->y, b->w, b->h) ? b->action : ACT_NONE;
		Button_pressed = -1;
		if (!Dtu_ui_key_repeat)
			do_action(action);
	}
}

//------------------------------------------------------------------------
// The frame

void dtu_display_frame(void)
{
	int mx = 0, my = 0;

	dtu_char_frame();
	dtu_lang_sync();		// ASCENT: the text follows the sex of the player (dtulang.h)
	if (!Dtu_display_active) return;

	mouse_get_pos(&mx, &my);
	do_input(mx, my);

	gr_set_current_canvas(&VR_screen_pages[0]);

	if (mode_has_interface(Dtu_interface_mode) && Interface_bm)
		blit(Interface_bm, 0, 0, Interface_bm->bm_w, Interface_bm->bm_h, 0, 0, 0);
	else
		gr_clear_canvas(rgb6(0, 0, 0));

	draw_view();

	if (mode_has_interface(Dtu_interface_mode)) {
		if (Dtu_interface_mode == DTU_IM_MAIN) {
			draw_main_areas();
			msg_draw(mx, my);
		} else {
			draw_info_areas();
			if (inventory_mode()) {
				draw_inventory();
				sbar_draw(mx, my);
			}
			else if (Dtu_interface_mode == DTU_IM_STATS) {
				draw_stats();
				dtu_spell_screen_draw(Dtu_hires);	// ASCENT: the spell books (dtuspell.c)
			} else if (Dtu_interface_mode == DTU_IM_MAP)
				draw_map();				// 0x414FD
		}
		draw_hot_keys();
		draw_buttons();
	}
	// ASCENT: with the conversation window open, dtu_dlg_draw() draws the
	// cursor after the window (dtu_display_draw_cursor)
	if (!Dtu_immersive && !dtu_dlg_is_open())
		draw_cursor(mx, my);
	// ASCENT: immersive mode has no cursor. With the target hint, the tinted
	// cursor shows at the center of the view while an attack can reach a
	// hostile creature, and nothing shows at other times.
	else if (Dtu_immersive && !dtu_dlg_is_open() && View_dest_set && dtu_combat_target_hint())
		draw_cursor(View_dest[0] + View_dest[2] / 2, View_dest[1] + View_dest[3] / 2);
}
