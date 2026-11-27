/*
 * ASCENT: the "Improvements" settings.
 *
 * These settings change the game beyond the original behavior. They are
 * different from the Dtu_fix_* settings, which correct defects of the
 * original (frame rate dependence and similar) and stay on.
 *
 * Each setting is on by default. With a setting off, the game uses the
 * original behavior with the Dtu_fix_* corrections. The options -orig*
 * (for example -origphys) still give the exact original rules.
 *
 * The values are kept in the configuration file of the port (see
 * dtuopts.c). The Improvements and Graphics menus of the options menu
 * and of the Escape menu change them (key F5 opens Graphics in the game).
 */
#ifndef _DTUOPTS_H
#define _DTUOPTS_H

// Player movement without the drift of the Descent ship physics: the
// player stops when the keys are released and does not slide in turns and
// side steps. Objects settle on the floor (a dead creature: always, see
// Dtu_fix_corpse_floor in dtuobj.h). The
// corrections of the frame rate (jump, ladders, walls) are not here: they
// are always on (Dtu_fix_* in physics.c).
extern int Dtu_improved_physics;

// Jump control: steering in the air, a jump a short time after the floor
// is left, and a jump that a wall or the edge of a ledge does not stop, so
// that it lands on a ledge or the top of a ladder. In flight and in
// levitation the jump key goes up and the crouch key goes down; the crouch
// key at the floor ends Levitate (1.3: the jump key ends it).
extern int Dtu_improved_jump;

// Crouch (port, not in 1.3): while the crouch key (Left Ctrl) is down on
// the floor the upper body goes down (the view too), the walk has half the
// speed and there is no run, and the player passes under a lower ceiling.
// The player stands up when the ceiling has the room. See physics.c,
// dtu_crouch_frame.
extern int Dtu_crouch;

// The AI of the creatures beyond 1.3 (the menu line "Improved AI"; until
// 2026-11-27 "Improved pathfinding", the key ImprovedPathfinding of the
// configuration file is still read). Off: the AI of 1.3 with the Dtu_fix_*
// corrections. On (see docs/re/ai.md, "Improved AI"):
// - Paths for creatures of the size of a person: no stop at the openings
//   of corridors or at corners, paths through open doors for a creature
//   that cannot open doors, a larger body so that creatures stay at the
//   distance of their weapon, and a melee creature that keeps out of the
//   reach of a player above it.
// - Senses: a creature sees the player along a free line and hears the
//   player along an open path (no wall, no closed door). An alarm goes
//   along an open path only.
// - Doors: only a creature that can open doors (the door bit of 1.3, or a
//   person by its model) and that fights the player opens a door, and never
//   a locked one.
// - The attack: a creature in reach attacks at once and faces its target.
// - A closed door or a wall stops an attack of the player and of a
//   creature.
// - A creature that its script can make hostile joins the fight of its
//   group when it sees the player fight it.
extern int Dtu_improved_ai;

// The AI of the creatures that cast spells, beyond 1.3 (the menu line
// "Caster AI", a decision of the user of 2026-11-27; docs/re/ai.md, "Caster
// AI"). Off (the options -origcasterai, -origobj and -noimprovements): the
// spell choice of 1.3 (0x92D99) with its Dtu_fix_caster_* corrections. On:
// - Each caster type has a spell list and a role (dtucaster.h), in place of
//   the one table of 44 entries for all casters (0x142084), and the caster
//   level of AD&D 2e for its type.
// - A contingency: at the first notice of the player the caster gets its
//   protection spells at once.
// - It keeps its distance while it has spells and closes in when it has
//   none; between two casts it attacks a player in its reach.
// - It chooses its attack by the range, the free line, the allies near the
//   target and the protections of the player; one cast each round (6.0
//   seconds) plus the cast time.
// - Its spells do not act on its allies, but an area does.
// - The spells Mirror Image, Minor Globe of Invulnerability, Spiritual
//   Hammer, Flame Strike and Mind Blast (dtuspell.h) come into the levels:
//   the scrolls that replace repeated mage scrolls, the spell sets of Aleena
//   Paladinstar, and the lists of the creatures.
// - The drow priestesses use their model with a weapon.
extern int Dtu_caster_ai;

// The 3D view is rendered at this multiple of the original view size,
// from 1 to 6. The interface is not scaled by it. Default 3. ASCENT: 0 is
// "x1/2": half the shown size in each direction (320x240 for the full
// screen view of the 640x480 interface), as the 320x200 game drew it.
// The GPU renderer only; the software renderer draws at x1.
extern int Dtu_render_scale;
#define DTU_RENDER_SCALE_MIN	0
#define DTU_RENDER_SCALE_MAX	6

// Depth test for each pixel on the GPU: a part of a creature behind a
// wall is hidden. The original draws a whole object or nothing.
extern int Dtu_improved_occlusion;

// The arms and the chest of the first person body follow the pitch of the
// view, so that the weapon attacks are visible when the player looks
// forward or up.
extern int Dtu_improved_body;

// Animation interpolation: creatures, characters, the arms of the player
// and the other animated models are drawn with the joint angles between
// the current animation frame and the next one, by the time between the
// two frames. Only the picture changes: the game rules (hit and fire
// frames) use the frame numbers of the original. It also works with the
// software renderer.
extern int Dtu_anim_interp;

// Target hint: the cursor of the play screen has a warm red tint while the
// attack of the item in the hand (the swing of a melee weapon, a missile, a
// spell missile) would reach a hostile creature that it can hurt, by the
// same test as the attack (dtucombat.c). Off: the cursor of the original.
extern int Dtu_target_hint;

// The 3D view uses the GPU renderer. 0 selects the software renderer of
// the original. Option -software sets 0. The software renderer disables
// the other graphics settings.
extern int Dtu_gpu_renderer;

// Graphics settings (the Graphics menu, key F5 in the game). They need the
// GPU renderer.

// Textures and bitmaps from the upscaled texture set (4 times the original
// size, 32-bit color) when the set exists. See tools/textures_hd.py.
extern int Dtu_upscaled_textures;

// Movies from the upscaled movie set when it exists. See tools/movies_hd.py.
extern int Dtu_upscaled_movies;

// Character and monster models from the improved model set when it exists
// (<hd>/polymodels, docs/formats/pom.md). A change takes effect when the
// next archive is loaded. -origmodels turns it off.
extern int Dtu_improved_models;

// Anisotropic filtering of the textures: 0 off, 1 on (8x). The GPU renderer
// always uses trilinear mipmaps; this setting adds the anisotropy.
extern int Dtu_anisotropic;

// Ambient occlusion: 0 off, 1 on. The corners and the narrow places of the
// level are darker (screen-space ambient occlusion, platform.h). It is not
// in the original. The arms of the player, the flames and the other
// pictures with no light do not change.
extern int Dtu_ambient_occlusion;

// Improved lighting: 0 off, 1 on. In 1.3 a light changes in steps. With the
// setting:
// - The sparkles of a warp and the fire of a rest place cast a light that
//   goes down to zero and starts again at the full value in one frame, each
//   time the game makes the picture again (fuelcen.c). The light goes up
//   again in the first quarter of each cycle (dtu_object_light in
//   lighting.c). Both renderers.
// - The light that each object casts on a vertex goes to a new value in a
//   short time, not in one frame (smooth_dynamic_light in lighting.c): a
//   spell light, an explosion, a missile. Both renderers.
// - The pulse of the effect color 0xCF moves between two rows of the fade
//   table, not one row each 6 views (render.c). The RGB form of the GPU
//   renderer only.
extern int Dtu_improved_lighting;

// Infravision (the key N) shows heat: light sources (torches, lamps,
// fire) and living creatures and characters are red and visible in the
// dark, undead (the UNDEAD flag of the creature table) and the rest of the
// scene are dark and cool. Off: the effect of the original (a fade table
// with red rows). Needs the RGB form of the GPU renderer.
extern int Dtu_improved_infravision;

// The screen shader: 0 off, 1 subtle (a little bloom and a soft blend of
// the pixels), 2 VGA (a mid range computer monitor of the late 1990s).
extern int Dtu_shader;
#define DTU_SHADER_OFF		0
#define DTU_SHADER_SUBTLE	1
#define DTU_SHADER_VGA		2

// ASCENT: the shape of the low resolution frame. The DOS game showed its
// 320x200 frame on a 4:3 monitor, so each pixel was 1.2 times taller than
// wide; the 3D view (aspect 1.2, 0x41193) is made for that. The interface
// pictures are those of 640x480 at half size, so DOS showed them 1.2 times
// taller than in high resolution. 0 (default): square pixels (16:10), so
// the pictures have the shape of the high resolution; the 3D view then
// uses the aspect 1.0 (Dtu_fix_view_aspect, dtudisp.c). 1: the window shows
// the 320x200 frame at 4:3, as DOS did (plat_bind_vga_aspect), in both
// renderers, with the aspect 1.2. "4:3 screen" in the Graphics menu,
// Lowres43 in ascent.cfg; the options -squarepixels and -screen43 set 0
// and 1 for one run.
extern int Dtu_fix_lowres_aspect;

// Streaming safe music: the file <hd>/music/NAME.ogg plays in place of
// the track NAME.TUN when it exists (dtumusic.c, tools/music_replace.py).
// For a track that a video platform claims. Off by default (unlike the
// other settings): the default is the original music. The options
// -safemusic and -nosafemusic change it for one run.
extern int Dtu_safe_music;

// Miss sound: the swing sound of the weapon (its flash_sound, a game
// sound) when a melee attack hits nothing, for the player (a swing at the
// air, a missed roll) and for a creature that misses the player
// (dtumiss.c). 1.3 has no sound there. On by default. -origsound and
// -nomisssound turn it off for one run.
extern int Dtu_miss_sound;

// The group "Restored and new content" of the Improvements menu: content
// that 1.3 does not have, or has in its data but not in its levels. Each
// line is on by default; with a line off the level is as 1.3 ships it. A
// change takes effect at the next load of the level.
// "The dragon Tamaramagil": the encounter with the adult red dragon of
// DROW2 (dtudragon.h, docs/re/dragon.md).
extern int Dtu_restored_dragon;
// "The phylactery of the lich": the altar of Velsharoon in the east crypts
// of CLAN3; when it breaks, the lich Morik Stormhand fights without his
// contingency (dtulich.h, docs/re/lich.md). LichPhylactery of ascent.cfg;
// -origobj and -nophylactery clear it for one run. The variable is in
// dtulich.c.
extern int Dtu_lich_phylactery;
// "Gear for single class mages and thieves": Aleena Paladinstar sells
// wands, scroll bundles, the Pearls of Power and bracers to a single class
// mage, Nalbas and Mirt sell a poisoned bow, potions and old guild things
// to a single class thief (dtugear.h, the script patches of
// dtuclassgear.c). ClassGear of ascent.cfg; -noclassgear, -noimprovements,
// -origitems and -origscript clear it for one run. The variable
// Dtu_class_gear is declared in dtugear.h. The new item lines need the new
// gear too; a change takes effect at the next load of a level.

// Rulebook items: the values and the class limits of some item lines take
// the value of the AD&D 2nd edition rulebooks (the table of dtuitemfix.c,
// docs/audit/items.md). Off by default (unlike the other settings): it
// changes the balance of the game. The options -rulebookitems and
// -norulebookitems change it for one run. The variable Dtu_rulebook_items
// is declared in dtuitemfix.h.

// New gear: new armor and gems in the item tables, wands 63 to 67, more
// gold and gems from the creatures, new market goods (dtugear.h). On by
// default. NewGear of ascent.cfg; -newgear and -nonewgear change it for one
// run, -noimprovements and -origitems clear it. It takes effect when the
// tables of the next level are loaded. The variable Dtu_new_gear is
// declared in dtugear.h.

// The directory of the upscaled files (movies <hd>/NAME.m1v, textures
// <hd>/textures/...), without a separator at the end, or NULL if there is
// none. The first that exists: the argument of -hddir, "hd" in the current
// directory, "hd" in each -datadir directory in the order of the
// arguments, "hd" in the parent directories of the current directory (up
// to four levels). The result is found once and kept.
const char *dtu_hd_dir(void);

// The 3D view uses the GPU renderer and the item of the Graphics menu is
// on (the software renderer disables the other graphics settings).
#define DTU_GFX(setting)	(Dtu_gpu_renderer && (setting))

// Load the settings from the configuration file. Call it one time at start,
// after the arguments are parsed. Arguments win over the file.
void dtu_opts_load(void);

// Write the settings to the configuration file.
void dtu_opts_save(void);

// Write the settings to the session log (dtulog.h).
void dtu_opts_log(void);

#endif
