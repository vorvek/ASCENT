/*
 * ASCENT: new file. Internal interface of the front end of Descent to
 * Undermountain (dtufe.c, dtufe_char.c, dtufe_town.c). The public
 * interface is dtufe.h. The facts are in docs/re/frontend.md.
 */
#ifndef _DTUFE_INT_H
#define _DTUFE_INT_H

#include "types.h"
#include "fix.h"
#include "gr.h"

#define FE_W	640
#define FE_H	480

// The front end screen numbers of the executable (table at 0x13E3D7).
#define FE_SCR_WATERDEEP	0
#define FE_SCR_BLACKSTAFF	1
#define FE_SCR_MARKET		3
#define FE_SCR_CHAR1			6
#define FE_SCR_CHAR2			7
#define FE_SCR_CHAR3			8
#define FE_SCR_CHAR4			9

// Text colors ("hue", 0x80457). The value is a color of the palette of
// the screen.
#define FE_HUE_BLACK		0
#define FE_HUE_1			1
#define FE_HUE_2			2
#define FE_HUE_3			3
#define FE_HUE_4			4
#define FE_HUE_5			5		// menu item
#define FE_HUE_6			6
#define FE_HUE_7			7		// selected menu item, titles
#define FE_HUE_MAX		10

// Sound numbers of the front end (arguments of 0x2DB28 and 0x2D955).
#define FE_SND_PAGE		0xC1C
#define FE_SND_REFUSE	0xC22
#define FE_SND_CLICK		0xC23

//------------------------------------------------------------------------
// Screen records (frontend.c, feinput.c)

typedef struct fe_screen fe_screen;

// A button picture. "up" is shown while the cursor is on the feature,
// "down" while the button is pressed. A name is a bitmap of PORTAL.EGG.
typedef struct fe_control {
	int			x, y;
	char			*up;
	int			x2, y2;
	char			*down;
	grs_bitmap	*bm_up, *bm_down;		// loaded by fe_run_screen
} fe_control;

typedef struct fe_feature {
	int	x1, y1, x2, y2;				// rectangle, if r is 0
	int	cx, cy, r;						// circle
	int	control;							// index of the control, or -1
	int	cursor;							// 0 or 1
	int	key1, key2;						// scan codes, 0: none
	int	help;								// number of Text_string, or -1: help_fn
	char	*(*help_fn)(int id);
	void	(*press)(fe_screen *s, int id);
	void	(*release)(fe_screen *s, int id);
	int	id;
	int	repeat;							// 1: the press repeats while the button is held
} fe_feature;

struct fe_screen {
	fe_control	*controls;
	int			n_controls;
	int			cursor_seq[2][3];		// FESEQ entries: idle, press, release. -1: none
	fe_feature	*features;
	int			n_features;
	int			help_x1, help_y1, help_x2, help_y2;
	int			number;					// front end screen number
	int			default_help;			// number of Text_string, or -1
	void			(*init)(fe_screen *s);
	void			(*draw)(fe_screen *s);
	void			(*frame)(fe_screen *s);
	int			(*may_leave)(fe_screen *s);
	int			running;
};

// A brush animation of FESEQ.
typedef struct fe_seq {
	int			n_frames;
	int			w, h;
	int			hot_x, hot_y;			// GRAB chunk
	int			trans;					// transparent color
	grs_bitmap	**frames;
} fe_seq;

// Menu items (the Descent newmenu with the fonts of the front end).
#define FE_MI_MENU	0
#define FE_MI_SLIDER	1
#define FE_MI_CHECK	2
#define FE_MI_INPUT	3
#define FE_MI_TEXT	4
#define FE_MI_INPUT_MENU	5	// NM_TYPE_INPUT_MENU: a menu item that becomes an input field
#define FE_MI_CHOICE	6	// ASCENT: a value from min_value to max_value, shown as "x<value>" or labels[value - min_value]; left and right change it

typedef struct fe_menu_item {
	int	type;
	char	*text;
	int	value, min_value, max_value;
	char	*buf;				// FE_MI_INPUT, FE_MI_INPUT_MENU
	int	buf_len;			// most characters
	void	(*changed)(struct fe_menu_item *item);
	char	**labels;			// ASCENT: FE_MI_CHOICE: the names of the values, or NULL
	int	disabled;			// ASCENT: FE_MI_CHOICE: drawn dark, the value does not change
	// set by fe_menu
	int	x, y, w, h;
} fe_menu_item;

//------------------------------------------------------------------------
// State of one frame

extern int Fe_mx, Fe_my;				// cursor position
extern int Fe_down, Fe_press, Fe_release;		// left button: state and edges
extern int Fe_rdown, Fe_rpress;		// right button
extern int Fe_key;						// key of this frame (value of key_inkey), or 0
extern int Fe_wheel;						// ASCENT: wheel steps of this frame (positive: away from the user)
extern int Fe_quit;						// 1: the window was closed, each loop must end
extern int Fe_screen_num;				// number of the loaded screen, or -1
extern grs_font *Fe_font_l, *Fe_font_m, *Fe_font_s;
extern grs_bitmap *Fe_cursor_bm;		// picture of the cursor in this frame
extern int Fe_cursor_trans;

//------------------------------------------------------------------------
// dtufe.c

fix fe_time(void);
int fe_frame_begin(void);				// input and background. Returns 0 if the program stops
void fe_frame_end(void);				// cursor, show the frame
void fe_input_clear(void);				// forget the edges and the key of this frame

int fe_load_screen(int number);		// background and palette, with the fades
void fe_fade_out(void);
void fe_fade_in(void);
void fe_show_picture(int febgnd_entry);	// a full screen picture with its palette, no fade

int fe_hue(int hue);
void fe_blit(grs_bitmap *bm, int x, int y, int trans);		// trans: color that is not drawn, -1: none
void fe_blit_part(grs_bitmap *bm, int sx, int sy, int w, int h, int x, int y, int trans);
void fe_restore(int x, int y, int w, int h);					// copy the background again
void fe_rect(int x, int y, int w, int h, int color);
grs_bitmap *fe_egg_bitmap(char *name, int *trans);			// bitmap of PORTAL.EGG, or NULL
grs_bitmap *fe_febgnd(int entry, ubyte *pal);				// picture of FEBGND, the caller frees it

void fe_string_size(grs_font *font, char *s, int *w, int *h);
// x 0x8000: centered between clip_x and clip_x + clip_w.
void fe_string(grs_font *font, int x, int y, int hue, int shadow_hue, char *s);
void fe_string_in(grs_font *font, int rx, int ry, int rw, int rh, int hue, int shadow_hue, char *s);

fe_seq *fe_seq_get(int n);
void fe_set_cursor(fe_seq *seq, int frame);

int fe_stream_count(void);
void fe_stream_info(int seq, int *x, int *y, int *w, int *h, int *n_frames);
grs_bitmap *fe_stream_frame(int seq, int frame);			// frame -1: the last. bm_x, bm_y: position
// Plays a sequence over the screen. "draw" draws the state of the screen
// under it. Returns the last frame (the caller frees it).
grs_bitmap *fe_stream_play(int seq, fe_screen *s);

char *fe_fetxt(int n);					// entry of FETXTx, or ""
char *fe_text(int n);					// Text_string[n], or ""

void fe_sound(int number);
void fe_refuse(void);
int fe_speech_queue(int werdz_entry);		// plays the line after the lines before it
int fe_speech_playing(void);
void fe_speech_stop(void);

void fe_help_message(char *s);		// shows s in the help bar for 3 seconds
void fe_run_screen(fe_screen *s);

int fe_menu(char *title, char *subtitle, fe_menu_item *items, int n, int cur, int box);
int fe_message_box(char *title, char *text, int n_choices, char *c0, char *c1);
int fe_input_box(char *title, char *buf, int max_len);

int fe_rand(void);

// Save games (0xBA99F, 0xBB780). fe_load_game returns 0 if nothing was
// loaded, else the next FE_GO_ mode: FE_GO_WATERDEEP, or FE_GO_LEVEL with
// the level in Fe_load_level. in_game: 1 from the death menu.
int fe_load_game(int in_game);
void fe_save_game(void);
extern int Fe_load_level;

//------------------------------------------------------------------------
// dtufe_char.c

int fe_new_character(void);			// the four screens. 1: a character was made

// dtufe_town.c
extern int Fe_khelben_mission, Fe_khelben_blurb;
// ASCENT: the full list of Waterdeep is in use (0x142035 == 0x141D48,
// 0x8C4B6): set by 0x8C431, cleared by 0x8C426
extern int Fe_full_list;
#define FE_GO_MENU			1
#define FE_GO_LEVEL			2		// Function_mode 2: the game
#define FE_GO_WATERDEEP		4
#define FE_GO_BLACKSTAFF	5
#define FE_GO_MARKET			7
#define FE_GO_PORTAL			8
int fe_waterdeep(void);					// returns FE_GO_
void fe_blackstaff(void);
void fe_market(void);
void fe_town_reset(void);				// a new character: Khelben starts again
// The stock of the market in the save games: 0x87C05(S, write). write 1:
// geldor.dat -> TheStore.<S>, 0: TheStore.<S> -> geldor.dat (after the
// running geldor.dat is deleted, 0x87BFB).
void fe_market_store(int s, int write);
// A new character (0x87EC3): geldor.dat is deleted, and the market keeps
// the charisma of this character (0x13E38C).
void fe_market_new_character(void);
// 0x879A7: reward n (0 to 8) of Khelben adds its goods to geldor.dat.
int fe_market_reward(int n);
// Settings of the market (dtufe_town.c, docs/re/frontend.md): D-081 and
// D-082, D-185, D-186. -origfe clears them (dtufe.c).
extern int Dtu_fix_market_charisma;
extern int Dtu_fix_market_sell_count;
extern int Dtu_fix_market_charges;
extern int Dtu_fix_market_buy_stock;
// ASCENT: the Potions of Freedom of the first stock and of each reward
// (dtufe_town.c). 0 with -origfe.
extern int Dtu_market_freedom;

//------------------------------------------------------------------------
// dtufe.c

extern int Fe_frame_count;

//------------------------------------------------------------------------
// dtufe_script.c

extern int Script_n, Script_pos, Script_wait;
extern int Script_btn, Script_rbtn;
extern char *Script_shot;
extern int Script_in_game;
int script_active(void);
void script_load(void);
void script_frame(void);

#endif
