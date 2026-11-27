/*
 * ASCENT: new file. The spell lists of the creatures that cast, for the
 * setting Dtu_caster_ai (dtuopts.h). dtuspell.c includes it. It has no
 * part in DTUGAME.EXE: 1.3 has one table of 44 entries for all casters
 * (0x142084) and no list for a creature type.
 *
 * AD&D 2nd edition is the guide: the spells of the levels 1 to 4 that a
 * mage or a priest of the level of the type would have, with the level at
 * which a caster gets the spell (a spell of level s from caster level
 * 2 s - 1, the rule of both classes up to level 4). Flame Strike (priest
 * 5) and Mind Blast are for creatures only. docs/re/ai.md, "Caster AI",
 * has the lists as tables.
 */

// The flags of an entry
#define CS_BUFF		0x01		// a protection: cast by the contingency at the first notice of the player, at once and with no points
#define CS_CLOSE		0x02		// only on a player in the reach of the spell at the caster (Burning Hands)
#define CS_HEAL		0x04		// on the caster itself, below half of its hit points
#define CS_FREE		0x08		// no spell points: a power of the creature with a recharge (Mind Blast)
#define CS_SELF		0x10		// on the caster itself, when it does not have it (Spiritual Hammer: then it throws the hammer)

typedef struct {
	unsigned char	spell;			// the spell line
	unsigned char	min_level;		// the lowest caster level that has it; 0: 2 * spell level - 1
	unsigned char	weight;			// the weight of the choice
	unsigned char	flags;			// CS_
} caster_spell;

#define CASTER_LIST_MAX	12

// The lists. Each ends with a spell 255.
enum {
	CL_DROW_MAGE, CL_DROW_PRIEST, CL_DROW_HIGH_PRIEST, CL_LICH, CL_ARCHMAGE, CL_MAGE, CL_HUMAN_CLERIC, CL_ILLITHID,
	CL_BEHOLDER, CL_MAGE_PRIEST, CL_AVATAR, CL_COUNT
};

static const caster_spell Caster_lists[CL_COUNT][CASTER_LIST_MAX] = {
	// CL_DROW_MAGE: the drow mages. Minor Globe, Lightning Bolt and Fireball
	// only for the types of level 9 (a decision of the user).
	{ { 45, 0, 0, CS_BUFF }, { 56, 0, 0, CS_BUFF }, { 57, 9, 0, CS_BUFF },
	  { 33, 0, 40, 0 }, { 4, 0, 90, CS_CLOSE }, { 36, 0, 30, 0 }, { 22, 0, 25, 0 }, { 32, 9, 30, 0 }, { 16, 9, 30, 0 }, { 255 } },
	// CL_DROW_PRIEST: the drow priestesses and priests
	{ { 53, 0, 0, CS_BUFF }, { 38, 0, 0, CS_BUFF },
	  { 58, 0, 80, CS_SELF }, { 52, 0, 25, 0 }, { 34, 0, 30, 0 }, { 255 } },
	// CL_DROW_HIGH_PRIEST: the priestesses of about 1000 experience points
	// and more (rffk8b 975, rffk9 1400, and the two of the inn): also Flame
	// Strike
	{ { 53, 0, 0, CS_BUFF }, { 38, 0, 0, CS_BUFF },
	  { 58, 0, 80, CS_SELF }, { 52, 0, 25, 0 }, { 34, 0, 20, 0 }, { 59, 8, 35, 0 }, { 255 } },
	// CL_LICH
	{ { 49, 0, 0, CS_BUFF }, { 45, 0, 0, CS_BUFF }, { 57, 0, 0, CS_BUFF }, { 56, 0, 0, CS_BUFF },
	  { 16, 0, 30, 0 }, { 32, 0, 30, 0 }, { 22, 0, 25, 0 }, { 33, 0, 30, 0 }, { 47, 0, 15, 0 }, { 255 } },
	// CL_ARCHMAGE: Halaster, Ibbalar, Aleena Paladinstar, the mage of the inn
	{ { 49, 0, 0, CS_BUFF }, { 45, 0, 0, CS_BUFF }, { 57, 0, 0, CS_BUFF }, { 56, 0, 0, CS_BUFF },
	  { 16, 0, 30, 0 }, { 32, 0, 30, 0 }, { 22, 0, 25, 0 }, { 33, 0, 30, 0 }, { 36, 0, 20, 0 }, { 47, 0, 15, 0 }, { 255 } },
	// CL_MAGE: the kobold shaman, the human mages, the Mage Spirit
	{ { 45, 0, 0, CS_BUFF }, { 56, 0, 0, CS_BUFF },
	  { 33, 0, 40, 0 }, { 4, 0, 90, CS_CLOSE }, { 36, 0, 30, 0 }, { 22, 0, 25, 0 }, { 32, 0, 25, 0 }, { 255 } },
	// CL_HUMAN_CLERIC: Bless once, Hold Person, a cure on itself when hurt,
	// then melee
	{ { 3, 0, 0, CS_BUFF },
	  { 52, 0, 30, 0 }, { 10, 0, 60, CS_HEAL }, { 11, 0, 60, CS_HEAL }, { 255 } },
	// CL_ILLITHID: the Mind Blast (a power, no points), then Hold Person
	// (the weapon spell of the type in 1.3, 0xAC7B2). It walks to the player
	// and touches (2d4, the dice of the creature setup).
	{ { 60, 1, 100, CS_FREE }, { 22, 1, 30, 0 }, { 255 } },
	// CL_BEHOLDER: the eyes of a beholder as the spells that the tables
	// have (AD&D 2e: slow, fear, charm or sleep as Hold Person, cause
	// serious wounds as Magic Missile, disintegrate as Lightning Bolt). It
	// keeps the move of 1.3 (it flies). Its weapon of the tables (Beholder
	// Burn) has the lifetime -1 and is not fired, as in 1.3.
	{ { 47, 1, 25, 0 }, { 14, 1, 20, 0 }, { 22, 1, 25, 0 }, { 33, 1, 35, 0 }, { 32, 1, 20, 0 }, { 255 } },
	// CL_MAGE_PRIEST: Thashasa (a mage and a priestess)
	{ { 45, 0, 0, CS_BUFF }, { 56, 0, 0, CS_BUFF }, { 53, 0, 0, CS_BUFF },
	  { 33, 0, 35, 0 }, { 36, 0, 25, 0 }, { 22, 0, 25, 0 }, { 58, 0, 60, CS_SELF }, { 32, 0, 25, 0 }, { 10, 0, 60, CS_HEAL }, { 255 } },
	// CL_AVATAR: the avatar of Lolth. It fights in melee.
	{ { 49, 0, 0, CS_BUFF }, { 57, 0, 0, CS_BUFF }, { 53, 0, 0, CS_BUFF },
	  { 52, 0, 25, 0 }, { 59, 0, 35, 0 }, { 32, 0, 30, 0 }, { 47, 0, 15, 0 }, { 255 } },
};

// The flags of a type
#define CF_POWER		0x02		// not a caster of 1.3 (no mage or cleric level): its list is a power of the creature
#define CF_MELEE		0x04		// it walks to the player also with spells: the blast and the hold on its way, then its touch
#define CF_MOVE13	0x08		// a beholder: it keeps the move of 1.3 (it flies) and does not touch between its casts

// The types by the name of the $MONSTER line. level: the caster level (AD&D
// 2e, the number of the type name for the drow and the humans); 0: the
// level of 1.3, max hit points / 8 (0xAE3BB). The dragon ("Dragon") and
// its hatchlings ("hatchlng") are not here: they keep the casts of 1.3.
static const struct { const char *name; unsigned char list, level, flags; } Caster_types[] = {
	// the drow mages (rmm*)
	{ "rmme1", CL_DROW_MAGE, 1, 0 }, { "rmmf1", CL_DROW_MAGE, 1, 0 }, { "rmmq1", CL_DROW_MAGE, 1, 0 }, { "rmmk1", CL_DROW_MAGE, 1, 0 },
	{ "rmmf3", CL_DROW_MAGE, 3, 0 }, { "rmmk3", CL_DROW_MAGE, 3, 0 }, { "rmmk5", CL_DROW_MAGE, 5, 0 }, { "rmmk7", CL_DROW_MAGE, 7, 0 },
	{ "rmmk9", CL_DROW_MAGE, 9, 0 }, { "rmmk9b", CL_DROW_MAGE, 9, 0 },
	// the drow priestesses (rff*) and priests (rmf*)
	{ "rffk1", CL_DROW_PRIEST, 1, 0 }, { "rffk5", CL_DROW_PRIEST, 5, 0 }, { "rffk8", CL_DROW_HIGH_PRIEST, 8, 0 },
	{ "rffk8b", CL_DROW_HIGH_PRIEST, 8, 0 }, { "rffk9", CL_DROW_HIGH_PRIEST, 9, 0 },
	{ "rfft1", CL_DROW_PRIEST, 1, 0 }, { "rfft4", CL_DROW_PRIEST, 4, 0 }, { "rfft6", CL_DROW_PRIEST, 6, 0 },
	{ "rfft9", CL_DROW_HIGH_PRIEST, 9, 0 },
	{ "rmft1", CL_DROW_PRIEST, 1, 0 }, { "rmfk1", CL_DROW_PRIEST, 1, 0 }, { "rmfk4", CL_DROW_PRIEST, 4, 0 },
	{ "rmfk5", CL_DROW_PRIEST, 5, 0 }, { "rmfk9", CL_DROW_PRIEST, 9, 0 },
	// the two priestesses of the inn (YPINN)
	{ "drowfem", CL_DROW_HIGH_PRIEST, 9, 0 }, { "priestes", CL_DROW_HIGH_PRIEST, 9, 0 },
	// the liches
	{ "lich1", CL_LICH, 12, 0 }, { "lich2", CL_LICH, 12, 0 }, { "lich3", CL_LICH, 12, 0 },
	// the human clerics
	{ "hffk1", CL_HUMAN_CLERIC, 1, 0 }, { "hfft1", CL_HUMAN_CLERIC, 1, 0 }, { "hfft4", CL_HUMAN_CLERIC, 4, 0 },
	{ "hfft9", CL_HUMAN_CLERIC, 9, 0 }, { "hmct1", CL_HUMAN_CLERIC, 1, 0 }, { "hmfk1", CL_HUMAN_CLERIC, 1, 0 },
	{ "hmft4", CL_HUMAN_CLERIC, 4, 0 }, { "hmft7", CL_HUMAN_CLERIC, 7, 0 },
	// the other mages
	{ "kob_spel", CL_MAGE, 2, 0 }, { "hmmk1", CL_MAGE, 1, 0 }, { "hfmk1", CL_MAGE, 1, 0 }, { "Daspel", CL_MAGE, 5, 0 },
	{ "Mage Spirit", CL_MAGE, 6, 0 },
	{ "halaster", CL_ARCHMAGE, 18, 0 }, { "ibbalar", CL_ARCHMAGE, 12, 0 }, { "Aleena", CL_ARCHMAGE, 9, 0 },
	{ "hmmnpc", CL_ARCHMAGE, 12, 0 },
	// the others
	{ "prstsprt", CL_DROW_PRIEST, 6, 0 },
	{ "thashasa", CL_MAGE_PRIEST, 7, 0 },
	{ "avatar", CL_AVATAR, 16, 0 },
	{ "beholder", CL_BEHOLDER, 8, CF_MOVE13 }, { "xabash", CL_BEHOLDER, 12, CF_MOVE13 },
	{ "flayer", CL_ILLITHID, 8, CF_POWER|CF_MELEE },
};

// The spells that a type casts at a lower level than its caster level. The
// lich of CLAN3 (Lich3, missions 12 and 13) is a boss of the middle of the
// game (a decision of the user of 2026-11-27): a character of level 6 has
// about 60 hit points, and Lightning Bolt at level 12 (10d6, 35) kills it
// with two casts. His damage spells have the dice of level 5 (Fireball and
// Lightning Bolt 5d6, Magic Missile 3 missiles). The other spells keep
// level 12: the protections of his contingency (Stone Skin 1d4 + 6 hits,
// Mirror Image 1d4 + 4 images, the Minor Globe), which Dispel Magic or the
// phylactery takes away, the holds and Slow. lich1 and lich2 (no level
// places them before the end of the game) keep level 12 for all.
// tools/caster_balance.py lich measures the fight.
static const struct { const char *name; unsigned char spell, level; } Caster_limits[] = {
	{ "lich3", 16, 5 }, { "lich3", 32, 5 }, { "lich3", 33, 5 },
};
