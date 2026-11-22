#!/usr/bin/env python3
"""Write the GitHub wiki pages of the game rules of Descent to Undermountain
1.3: races, classes, weapons, armor, magic items, potions, wands, scrolls,
wizard spells and priest spells.

The numbers come from two places:

  - the item and spell lines of the level table YPINN.BIN (the same rule
    values are in every table of the mission levels; --tables checks that),
  - tables of DTUGAME.EXE 1.3, read by their linear address (object 1 at
    0x10000, docs/formats/exe.md).

What exists only as code (a formula, a branch on an item number) is in the
marked tables CODE RULES below. Each entry names the address of the code in
DTUGAME.EXE 1.3 and, where the port has it, the function of engine/main.
The reverse engineering notes are docs/re/combat.md (part B), spells.md,
character.md and frontend.md.

usage: python -I tools/wiki_rules.py GAME_DIR OUT_DIR [--tables DIR]

GAME_DIR  the installed game: YPINN.BIN and DTUGAME.EXE of version 1.3.
OUT_DIR   the pages are written here (Home.md, _Sidebar.md, ...).
--tables  a directory of unpacked level tables (*.BIN, gamedata/unpacked):
          the item and spell lines of each table are compared with
          YPINN.BIN and the differences are printed.

This tool reads the game files only. The pages hold numbers and names of the
game; do not commit them.
"""
import argparse
import collections
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import egg  # noqa: E402


# ---------------------------------------------------------------------
# Readers

class Table:
    """The record lines of a .BIN table, by section word ($WEAPON ...)."""

    def __init__(self, path):
        with open(path, "rb") as f:
            raw = f.read()
        _, text = egg.decode_bin(raw)
        self.sections = collections.defaultdict(list)
        for line in text.decode("latin-1").split("\n"):
            line = line.split(";")[0].strip()
            if line.startswith("@"):
                line = line[1:]
            m = re.match(r"(\$[A-Z_]+)\b", line)
            if m:
                self.sections[m.group(1)].append(Record(line))

    def __getitem__(self, sec):
        return self.sections[sec]


class Record:
    def __init__(self, line):
        self.line = line
        bare = re.sub(r'"[^"]*"', " ", line)
        self.flags = set(w.lower() for w in bare.split()[1:] if "=" not in w)

    def get(self, key, default=None):
        # The first value of key=; "#key" is another key (tbl.md). Keys are
        # not case sensitive (the tables have Fighter_use, #Damage_dice).
        m = re.search(r"(?<![\w#])%s\s*=\s*(\"[^\"]*\"|\S+)" % re.escape(key),
                      self.line, re.I)
        return m.group(1).strip('"') if m else default

    def num(self, key, default=0):
        v = self.get(key)
        if v is None:
            return default
        try:
            return int(v)
        except ValueError:
            try:
                return float(v)
            except ValueError:
                return default

    def has(self, flag):
        return flag.lower() in self.flags

    def name(self):
        return (self.get("name") or self.get("weapon_name") or "?").strip()


class Exe:
    """DTUGAME.EXE (DOS/4GW linear executable): bytes by linear address."""

    def __init__(self, path):
        with open(path, "rb") as f:
            self.d = f.read()
        d = self.d
        le = struct.unpack_from("<I", d, 0x3C)[0]
        if d[le:le + 2] != b"LE":
            raise SystemExit("%s: no LE header" % path)
        u32 = lambda o: struct.unpack_from("<I", d, le + o)[0]
        self.page_size = u32(0x28)
        objtab, nobj, pagemap, self.data_pages = u32(0x40), u32(0x44), u32(0x48), u32(0x80)
        self.objects = []
        for i in range(nobj):
            vsize, base, flags, first, count, _ = struct.unpack_from("<6I", d, le + objtab + 24 * i)
            pages = []
            for p in range(first, first + count):
                e = d[le + pagemap + 4 * (p - 1):le + pagemap + 4 * p]
                pages.append((e[0] << 16) | (e[1] << 8) | e[2])
            self.objects.append((base, vsize, pages))
        # The keyword string of the $SPELL reader at its 1.3 address
        if self.read(0x120FDB, 13) != b"cast_in_front":
            raise SystemExit("%s: not the executable of version 1.3" % path)

    def read(self, addr, n):
        for base, vsize, pages in self.objects:
            if base <= addr < base + vsize:
                out = b""
                while n > 0:
                    off = addr - base
                    page, inpage = divmod(off, self.page_size)
                    k = min(n, self.page_size - inpage)
                    if page < len(pages):
                        fo = self.data_pages + (pages[page] - 1) * self.page_size + inpage
                        out += self.d[fo:fo + k].ljust(k, b"\0")
                    else:
                        out += b"\0" * k     # not in the file: zero
                    addr += k
                    n -= k
                return out
        raise ValueError("address 0x%X is not in the executable" % addr)

    def ints(self, addr, n, fmt="i"):
        size = struct.calcsize("<" + fmt)
        return list(struct.unpack_from("<%d%s" % (n, fmt), self.read(addr, n * size)))


# ---------------------------------------------------------------------
# CODE TABLES: data tables of DTUGAME.EXE 1.3, read from the executable.
# Address, element type (struct format), count. Tables of pointers are not
# read (their file values have no fixups); the rows they point to are.

EXE_TABLES = {
    # Character generation (fenuchar.c), docs/re/frontend.md section 5
    "race_combos": (0x13FECE, "i", 6),      # class masks allowed, by race of the screen (bit mask-1, 0x13FEE6)
    "race_mod": (0x13FCFF, "i", 36),        # 6 rows of 6: STR DEX CON INT WIS CHA (pointers at 0x13FD8F)
    "race_limits": (0x13FB03, "i", 72),     # 6 rows of 12: minimum x6, maximum x6 (pointers at 0x13FCE3)
    # Ability tables
    "str_hit": (0x13A956, "i", 20),         # to hit by strength 0..19 (attack 0x2A789)
    "str_dmg": (0x13A9A2, "i", 20),         # damage by strength 0..19 (0x2B494)
    "str18_hit": (0x13A9F2, "i", 5),        # fighter 18/xx: 01-50, 51-75, 76-90, 91-99, 00
    "str18_dmg": (0x13AA06, "i", 5),
    "dex_ac": (0x14019A, "i", 20),          # armor class adjustment (0xAFB55)
    "con_hp": (0x1402E2, "i", 20),          # hit points per level, not a fighter
    "con_hp_f": (0x140332, "i", 20),        # fighter
    "int_spells": (0x14023E, "i", 20),      # shown on the statistics screen only
    "wis_bonus": (0x1403D2, "i", 80),       # 4 rows of 20: priest spell levels 1..4
    "cha_sell": (0x13F2DD, "i", 20),        # market: percent of the value, the player sells
    "cha_buy": (0x13F32D, "i", 20),         # the player buys
    # Thief skills of a new character (0x8BDE5); race index 0 human, 1 dwarf,
    # 2 elf, 3 drow, 4 halfling, 5 half-elf
    "thief_race": (0x1461C8, "i", 30),      # 5 rows: hide, move, locks, traps, climb
    "thief_dex": (0x146078, "i", 84),       # 4 rows of 21: hide, move, locks, traps
    "secret_d6": (0x1466E8, "i", 6),        # racial search for secret doors (0xB3ECE)
    # Experience, THAC0, hit points (0xAF3CF, 0xAF605, 0xAF694), class order
    # cleric, mage, fighter, thief
    "xp_c": (0x145FA0, "i", 21), "xp_m": (0x145F4C, "i", 21),
    "xp_f": (0x145EF8, "i", 21), "xp_t": (0x145FF4, "i", 21),
    "thac0_c": (0x146548, "B", 18), "thac0_m": (0x14657E, "B", 18),
    "thac0_f": (0x14656C, "B", 18), "thac0_t": (0x14655A, "B", 18),
    "hit_die": (0x146518, "i", 4),
    "hd_dice_c": (0x146298, "i", 20), "hd_dice_m": (0x1462E8, "i", 20),
    "hd_dice_f": (0x146338, "i", 20), "hd_dice_t": (0x146388, "i", 20),
    "hd_fixed_c": (0x1463D8, "i", 20), "hd_fixed_m": (0x146428, "i", 20),
    "hd_fixed_f": (0x146478, "i", 20), "hd_fixed_t": (0x1464C8, "i", 20),
    # Saving throws 0xAE596: rows of 5 bytes, divisors by class
    "save_c": (0x145642, "B", 20), "save_t": (0x145656, "B", 15),
    "save_f": (0x145665, "B", 30), "save_m": (0x145683, "B", 15),
    "save_div": (0x1456E2, "B", 4),
    # Spell slots: 18 rows by class level (0x3575B)
    "slots_m": (0x145CD8, "B", 162), "slots_c": (0x145DF8, "B", 126),
    # Armor class of the items by number (0xAFBDA)
    "ac_armor": (0x145C04, "i", 53), "ac_misc": (0x145A04, "i", 128),
    "ac_weapon": (0x1458B8, "i", 83),
    "glove_dex": (0x146240, "i", 20),       # Gloves of Dexterity, by dexterity
    "shock": (0x2FACC, "B", 19),            # Potion of Toughening, constitution 1..19
    "spider_spells": (0x2FA94, "i", 14),    # Spider Piece 0x14
    "turn": (0xA6487, "b", 126),            # turn undead: 9 rows of 14 (0xA6487)
}


def read_code_tables(exe):
    return {k: exe.ints(a, n, f) for k, (a, f, n) in EXE_TABLES.items()}


# ---------------------------------------------------------------------
# CODE RULES. Values that exist only as code. Sources: DTUGAME.EXE 1.3
# addresses (docs/re/combat.md part B unless named), port functions in
# engine/main. L is the caster level, T the level of the target.

MIN = 60          # the game messages call 60 game seconds a "minute"

# Missiles of a spell (0x56FF6). Speed 100 units/s (no $SPELL line has
# speed=, the default of the reader 0x242F6 is 100.0), life 30 s
# (Weapon_info+0xA8 of the template, written by 0x56FF6).
MISSILES = {
    4: lambda L: "6",
    6: lambda L: "1d6",
    7: lambda L: "1d4+%d" % L,
    33: lambda L: str(min(L // 2 + 1, 5)),
    34: lambda L: "1d3",
    37: lambda L: str(min(L, 5)),
}
MISSILE_COUNT_TEXT = {4: "6", 6: "1d6", 7: "1d4 + L", 33: "min(L/2 + 1, 5)", 34: "1d3", 37: "min(L, 5)"}
MISSILE_LIFE = {32: lambda L: 2, 36: lambda L: (L // 3 + 1) * 60}

# Spell effects: the cases of 0x8CFFF (jump table 0x8CF1F), the port
# dtuspell.c apply_spell(). Fields:
#   dur    seconds as a function of L, or a text
#   save   the saving throw (category 4, spell, unless named)
#   eff    what it does
#   lv     columns of the "by caster level" table: (title, function of L)
#   name   the name the code gives when it differs from the table line
#   note   extra notes (differences, defects, open points)
SPELLS = {
    0: dict(src="0x8D297", dur="instant",
            save="own roll: d20 >= 11 - L + T (1 to 20)",
            eff="Works only on an extraplanar creature. If the roll is made, the creature dies "
                "on d100 >= 2T + 70; else it leaves the level (no death, no experience).",
            lv=[("d20 needed vs T 5", lambda L: str(min(max(11 - L + 5, 1), 20)))]),
    1: dict(src="0x8D3F7", dur=lambda L: (L + 1) * MIN, save="none",
            eff="+1d8 hit points, also above the maximum, and Bless (+1 to hit) for the duration. "
                "When Bless ends, hit points above the maximum go.",
            lv=[("duration", lambda L: dur_text((L + 1) * MIN))]),
    2: dict(src="0x8D4E7, 0xB05D9", dur="until the pool is used up", save="none",
            eff="Armor class at most 6 (before the Dexterity adjustment). A pool of 8 + L hit points: "
                "each weapon hit lowers the pool by its full damage, and you still take the damage. "
                "The spell ends at 0. Spell damage on you does not lower the pool.",
            lv=[("pool", lambda L: str(8 + L))]),
    3: dict(src="0x8D58E, 0x2AD27", dur=360, save="none", eff="+1 to hit."),
    4: dict(src="0x8D614, 0x911AA", dur="instant", save="half",
            eff="Each missile that hits makes an area (radius of the line) at the point of the hit. "
                "Each creature or player in it, except the caster, takes fire 1d3 + 2L.",
            lv=[("fire per target", lambda L: "1d3+%d" % (2 * L))]),
    5: dict(src="0x8D62F, 0x29E3D", dur=lambda L: (L + 3) * MIN, save="see text",
            eff="The case works only when the target is the caster (0x8D633), but the spell is not "
                "self-cast: it fires a missile, and a missile hits others. As read, the spell does "
                "nothing when you cast it. The effect itself (also given by the Sword of W'hin): "
                "each melee hit, the target saves or takes +1d4 damage; an undead target gets "
                "Fear for 1d4 + L minutes instead. The cold dice of the line are never rolled.",
            lv=[("duration", lambda L: dur_text((L + 3) * MIN))]),
    6: dict(src="0x8D6D4", dur="see text", save="only if T > L or T >= 6",
            eff="Not on blind creatures. T <= L: unconscious (sleep) for 2 to 8 minutes (2 + rand % 7); "
                "a player is also blind for that time. T <= L + 2: blind for 1d4 minutes. "
                "Higher T: stunned (held) for 60 s, not with Free Action. Each missile has an area "
                "(radius of the line) at the hit."),
    7: dict(src="0x8DB82", dur=lambda L: (L + 2) * MIN, save="negates",
            eff="Confused: a creature stops being hostile for the time; a player loses control.",
            lv=[("duration", lambda L: dur_text((L + 2) * MIN))]),
    8: dict(src="0x8DC1D, 0x90CEF", dur="no end", save="negates",
            eff="An area around you (the spell is cast on yourself, see the cast column). Each creature "
                "or player in it, except you and blind creatures, saves or is blind with no end. "
                "A light object (light 60) appears at your place, with no end. Light-sensitive "
                "creatures also take the damage of the line (this line has no dice: 0)."),
    10: dict(src="0x8DE4B", dur="instant", save="none",
             eff="+1d8 hit points, up to the maximum. Not on undead or extraplanar creatures."),
    11: dict(src="0x8DF37", dur="instant", save="none",
             eff="+3 to 17 hit points (3 + rand % 15), up to the maximum. Not on undead or "
                 "extraplanar creatures."),
    12: dict(src="0x8E019", dur=lambda L: 2 * L * MIN, save="none",
             eff="Sets the effect Detect Magic. The renderer reads it (0x670DB, 0x75726); what it "
                 "shows was not decoded. ASCENT shows nothing.",
             lv=[("duration", lambda L: dur_text(2 * L * MIN))]),
    13: dict(src="0x8E0B5, 0x56D36", dur="instant", save="none on you",
             eff="This line has cast_in_front, so it is cast on yourself (as read): all your spell "
                 "effects and potion effects end. On another target (cast by a creature, or the "
                 "priest version): all effects of a creature or player end, no save; a spell missile "
                 "saves (d20 >= 10) or is destroyed; a potion item on the floor is destroyed on "
                 "d20 >= 23 - L (2 to 20); magic scenery saves or breaks; a spell light saves or goes."),
    14: dict(src="0x8E2D8", dur=lambda L: L * MIN, save="negates",
             eff="Not on undead or no_scare creatures. The target flees. A creature drops its weapon "
                 "on d100 <= 65 - 5T. A player is held for the time, may drop the weapon, and sees "
                 "through a gated creature (not in ASCENT). On a player the time is divided by "
                 "(Wisdom / 2).",
             lv=[("duration", lambda L: dur_text(L * MIN))]),
    15: dict(src="0x8E464, 0x9DEDD", dur=lambda L: L * MIN, save="none",
             eff="No gravity flag; the fall speed is at most 15 units/s. No end message.",
             lv=[("duration", lambda L: dur_text(L * MIN))]),
    16: dict(src="0x8E55C, 0x91BC3", dur="instant", save="half",
             eff="Fire min(L, 10) dice of 0 sides + 6L. The dice have 0 sides in every table: the "
                 "1.3 code divides by 0 there (rand() % 0, as read; not tested in the DOS game). "
                 "ASCENT (Dtu_fix_zero_dice) rolls no dice: fire 6L. Only the creature that the "
                 "missile hits (type TARGET). Scenery saves (category 5) or breaks.",
             lv=[("fire (ASCENT)", lambda L: str(6 * L))]),
    17: dict(src="0x8E5B1", dur="instant", save="half",
             eff="Damage with the dice of the line, and the line has no dice: no damage (as read). "
                 "Scenery saves (category 5) or breaks."),
    18: dict(src="0x8E5D1", dur=lambda L: (L + 1) * MIN, save="none",
             eff="Fire damage is halved (the same effect as Resist Fire).",
             lv=[("duration", lambda L: dur_text((L + 1) * MIN))]),
    19: dict(src="0x8E665", dur="10 x (1d6 + L) minutes", save="none",
             eff="You fly.",
             lv=[("duration", lambda L: "%d to %d min" % (10 * (1 + L), 10 * (6 + L)))]),
    20: dict(src="0x8E6F2", dur=lambda L: 10 * L * MIN, save="none",
             eff="Ends Hold. Hold Person and the stun of Color Spray do not work on you; no slow down "
                 "of the movement.",
             lv=[("duration", lambda L: dur_text(10 * L * MIN))]),
    21: dict(src="0x8E79C, 0x9DE13, 0x41EC3", dur=lambda L: (L + 3) * MIN, save="none",
             eff="Ends Slow. Move speed x2. The melee attack: in 1.3 the next swing waits half a "
                 "swing longer (Haste is slower); ASCENT (Dtu_fix_haste_attack) halves the swing time.",
             lv=[("duration", lambda L: dur_text((L + 3) * MIN))]),
    22: dict(src="0x8E85A", dur=lambda L: 2 * L * MIN, save="negates",
             eff="Only humanoid creatures or a player; not undead, not with Free Action. The target "
                 "cannot move. On a player the time is divided by (Wisdom / 2).",
             lv=[("duration", lambda L: dur_text(2 * L * MIN))]),
    23: dict(src="0x8EA21", dur="(1d4 + L) minutes", save="only if T > 2",
             eff="Only undead creatures of level T <= L. The target cannot move.",
             lv=[("duration", lambda L: "%d to %d min" % (1 + L, 4 + L)),
                 ("highest T", lambda L: str(L))]),
    24: dict(src="0x8EB53", dur=lambda L: (L + 4) * MIN, save="none",
             eff="Creatures do not see you (0x165C4), also while you attack. Open Locks -15 while "
                 "invisible.",
             lv=[("duration", lambda L: dur_text((L + 4) * MIN))]),
    25: dict(src="0x8EBE8", dur=lambda L: (6 * L + 12) * 10 * MIN, save="none",
             eff="Lets you turn on the infravision view (key N). Only a human or half-elf can take "
                 "it (the other races have infravision).",
             lv=[("duration", lambda L: dur_hours((6 * L + 12) * 10 * MIN))]),
    26: dict(src="0x8ECB4", dur="until you attack", save="none",
             eff="Creatures do not see you. Ends when you make a weapon attack or cast a spell "
                 "missile. Not with Improved Invisibility. Open Locks -15 while invisible."),
    27: dict(src="0x8ED20, 0x9EA62", dur="(1d3 + L) minutes", save="none",
             eff="Jump speed 2.0 in place of 1.5.",
             lv=[("duration", lambda L: "%d to %d min" % (1 + L, 3 + L))]),
    28: dict(src="0x8FFE5, 0x26C2F", dur="instant", save="none",
             eff="The case has no code. The line is self-cast, so you get no missile and nothing "
                 "happens (as read). A Knock missile would open the door it hits, also a locked "
                 "door, unless the lock is magic (wall keys & 6)."),
    29: dict(src="0x8EDB2, 0x9DEAE", dur=lambda L: 10 * L * MIN, save="none",
             eff="No gravity; you move up and down only. You cannot cast spells while you levitate "
                 "(\"You cannot cast a spell while climbing.\").",
             lv=[("duration", lambda L: dur_text(10 * L * MIN))]),
    30: dict(src="0x8DC1D, 0x90CEF", dur=lambda L: 10 * L * MIN, save="negates",
             eff="An area around you (cast on yourself). Each creature or player in it, except you "
                 "and blind creatures, saves or is blind for the time. A light object (light 20) at "
                 "your place for the time. Light-sensitive creatures take 1d6.",
             lv=[("duration", lambda L: dur_text(10 * L * MIN))]),
    32: dict(src="0x8EE76, 0x91BC3", dur="instant", save="half",
             eff="Electric min(L, 10) dice of 0 sides + 6L (the same 0 sides problem as Fireball; "
                 "ASCENT: 6L). No damage to electricity_immune creatures. The missile lives 2 s. "
                 "Scenery saves (category 6) or breaks.",
             lv=[("electric (ASCENT)", lambda L: str(6 * L))]),
    33: dict(src="0x8EED5, 0x56FF6", dur="instant", save="half",
             eff="1d4 for each missile (damage_bonus=1 of the line is not added). Homing for a mage. "
                 "No damage to a target with Shield.",
             lv=[("missiles", lambda L: str(min(L // 2 + 1, 5)))]),
    34: dict(src="0x8EEFE", dur="instant", save="half",
             eff="1d4 for each stone, 2d4 against undead."),
    35: dict(src="0x8EF49, 0xB0606", dur=lambda L: 5 * L * MIN, save="none",
             eff="Armor class at most max(5 - (L - 5) / 3, 1) (before the Dexterity adjustment).",
             lv=[("duration", lambda L: dur_text(5 * L * MIN)),
                 ("AC limit", lambda L: str(max(5 - int((L - 5) / 3), 1)))]),
    36: dict(src="0x8EFE0, 0x56FF6", dur="instant", save="half",
             eff="Acid (2(L/3) + 2) dice of 0 sides + 4L (0 sides as Fireball; ASCENT: 4L). Acid "
                 "ends regeneration. The missile lives (L/3 + 1) minutes.",
             lv=[("acid (ASCENT)", lambda L: str(4 * L))]),
    37: dict(src="0x8F021", dur="instant", save="half",
             eff="1d4 for each meteor. Scenery saves (category 5) or breaks.",
             lv=[("meteors", lambda L: str(min(L, 5)))]),
    38: dict(src="0x8F078, 0x56D6A", dur=lambda L: L * MIN, save="none",
             eff="Bless (+1 to hit) on you. The cast writes the flag word of the line to 0x102 "
                 "(0x56D6A), which clears the area bit, so 1.3 casts it on you alone (as read). "
                 "ASCENT casts it as an area around you, and the case then blesses others whose "
                 "flag 0x200 of +0x1B0 (peaceful) is clear, that is hostile creatures: review.",
             lv=[("duration", lambda L: dur_text(L * MIN))]),
    39: dict(src="0x8F116, 0x2AD36", dur=lambda L: 2 * L * MIN, save="none",
             eff="Attacks of extraplanar creatures miss; other creatures -2 to hit you. Your attack "
                 "on an extraplanar creature ends the spell.",
             lv=[("duration", lambda L: dur_text(2 * L * MIN))]),
    40: dict(src="0x8F1B2", dur="until the pool is used up", save="none",
             eff="A pool of 12L hit points that takes fire damage first. Cast again, the pool grows.",
             lv=[("pool", lambda L: str(12 * L))]),
    41: dict(src="0x8F21D", dur="see text", save="none",
             eff="You choose: three more level 1 slots, one level 1 and one level 2, or one level 3. "
                 "The maximum grows; the next rest fills the slots. 1.3: one cast only, the bonus "
                 "stays until Dispel Magic or the Potion of Anti-Magic. ASCENT "
                 "(Dtu_fix_rary_manual): the rule of the manual, each cast adds, the rest after the "
                 "next one ends it."),
    42: dict(src="0x8F2FA", dur="instant", save="none", name="Cure Blindness or Deafness",
             eff="The line is called Resist Cold, but the code cures blindness and deafness of the "
                 "caster (self-cast). Nothing happens if you are neither."),
    43: dict(src="0x8F3CD", dur=lambda L: L * MIN, save="none",
             eff="Fire damage is halved.",
             lv=[("duration", lambda L: dur_text(L * MIN))]),
    44: dict(src="0x8F467", dur="(1d4 + L) minutes", save="see text",
             eff="Targets of level T <= 6 only; not undead, no_scare or extraplanar creatures. Only an "
                 "elf, half-elf or cleric (or a creature with a cleric level) gets a save. The "
                 "target flees and drops its weapon half of the time. One target (type TARGET).",
             lv=[("duration", lambda L: "%d to %d min" % (1 + L, 4 + L))]),
    45: dict(src="0x8F6AE, 0xB0659", dur=lambda L: 5 * L * MIN, save="none",
             eff="Armor class at most 3 (before the Dexterity adjustment). Magic Missile does no "
                 "damage to you.",
             lv=[("duration", lambda L: dur_text(5 * L * MIN))]),
    46: dict(src="0x8F766", dur=lambda L: 5 * L * MIN, save="see text",
             eff="Targets of level T <= 3 only; not undead or no_sleep. An elf resists on d10 <= 9, a "
                 "half-elf on d10 <= 3, a drow not at all. The target sleeps (a player is also blind); a melee hit "
                 "wakes it and always hits. 1.3 also has a pool of 4 to 16 per cast that a creature "
                 "of max hit points H uses up by H / 4 (not in ASCENT).",
             lv=[("duration", lambda L: dur_text(5 * L * MIN))]),
    47: dict(src="0x8FAD8", dur=lambda L: (L + 3) * MIN, save="negates",
             eff="Ends Haste. Armor class +4 (at most 10), -4 to hit, move speed / 2, swing time x2. "
                 "One target (type TARGET; the radius of the line is not used).",
             lv=[("duration", lambda L: dur_text((L + 3) * MIN))]),
    48: dict(src="0x8FBEE, 0xB3038", dur=lambda L: (L + 3) * MIN, save="none",
             eff="Climb Walls counts as 100%.",
             lv=[("duration", lambda L: dur_text((L + 3) * MIN))]),
    49: dict(src="0x8FC7A, 0x2AE44", dur="1d4 + L/2 hits", save="none",
             eff="Each melee hit on you is taken by the skin (no damage) until the count is 0. In "
                 "1.3 the cast clears all your other spell effects (it writes the whole effect word); "
                 "ASCENT (Dtu_fix_stoneskin_word) keeps them.",
             lv=[("hits", lambda L: "%d to %d" % (1 + L // 2, 4 + L // 2))]),
    50: dict(src="0x8FCFA", dur=lambda L: L * 3600, save="none",
             eff="Strength + 1d8 for a fighter (above 18 each point is +10 percent, up to 18/00), "
                 "+1d6 for a thief or cleric, +1d4 for a mage; at most 18 for these. The time is "
                 "L x 3600 s in 32 bits: from caster level 10 on it wraps and the spell ends at once "
                 "(1.3, kept in ASCENT).",
             lv=[("duration", lambda L: dur_hours(L * 3600) if L < 10 else "ends at once")]),
    51: dict(src="0x8E4F6", dur=1800, save="none", name="Find Traps",
             eff="The line is a second Burning Hands (ice), but the code is Find Traps: Find/Remove "
                 "Traps counts as 100% and the search runs each frame. The line is type AREA and not "
                 "self-cast: it fires one missile and gives the effect to all in the area at the hit "
                 "(as read)."),
    52: dict(src="0x8E85A", same=22),
    53: dict(src="0x8F116", dur=lambda L: 3 * L * MIN, save="none",
             eff="As the mage spell: extraplanar attacks miss, other creatures -2 to hit you.",
             lv=[("duration", lambda L: dur_text(3 * L * MIN))]),
    54: dict(src="0x8E019", dur=600, save="none", eff="As the mage spell, for a fixed 10 minutes."),
    55: dict(src="0x8E0B5", dur="instant", save="none",
             eff="Fires a missile. The target (creature or player) loses all spell effects, no save. "
                 "A spell missile saves (d20 >= 10) or is destroyed; a potion item on the floor is "
                 "destroyed on d20 >= 23 - L (2 to 20); magic scenery saves or breaks; a spell light "
                 "saves or goes."),
}
SPELLS[9] = dict(SPELLS[8])
SPELLS[31] = dict(SPELLS[30], dur=lambda L: 10 * (L + 6) * MIN,
                  lv=[("duration", lambda L: dur_text(10 * (L + 6) * MIN))])
SPELLS[52] = dict(SPELLS[22], src="0x8E85A")

# Item numbers that the code knows (0xAFB55, 0x37A6A, 0x2AB9F, 0xADC46).
# Effects of worn items: the item bits of 0xAFB55 (combat.md B6a), the
# port dtuchar.c worn_item_bits().
WORN = {
    ("armor", 0): "Protection: see the ring rules.",
    ("armor", 14): "Protection: see the ring rules.",
    ("armor", 15): "Protection: see the ring rules.",
    ("armor", 16): "Nothing. The code sets a flag that no code reads (pc_spell=2, Armor, is not used).",
    ("armor", 17): "Resist Fire while worn: fire damage halved.",
    ("armor", 18): "Feather Fall while worn.",
    ("armor", 19): "Fly while worn.",
    ("armor", 20): "Free Action while worn.",
    ("armor", 21): "Constitution 18 while worn (also when it was higher).",
    ("armor", 22): "Infravision while worn, only for a human or half-elf (the race byte 0).",
    ("armor", 23): "Jump while worn.",
    ("armor", 24): "Detect Magic while worn.",
    ("armor", 25): "Haste while worn.",
    ("armor", 26): "Strength 18/00 while worn (also when it was higher).",
    ("armor", 27): "The Ring of Wizardry of the 1.3 readme (the line says Ring of Warmth, pc_spell=42). "
                   "Doubles the mage slots of spell levels 1 to 3. 1.3 doubles them again at each change "
                   "of the equipment and halves them once when it is taken off (0xB00A5, 0xB075F); "
                   "ASCENT (Dtu_fix_wizardry_ring) doubles them once.",
    ("armor", 33): "Shield while worn (AC at most 3, no Magic Missile damage). The necklace code skips its "
                   "armor class value.",
    ("armor", 34): "Fly while worn.",
    ("armor", 35): "Feather Fall while worn.",
    ("armor", 36): "Levitate while worn (you cannot cast spells while levitating).",
    ("armor", 37): "Haste while worn.",
    ("armor", 38): "Strength 19 while worn.",
    ("armor", 43): "Hide In Shadows 95% while worn.",
    ("armor", 44): "Improved Invisibility while worn.",
    ("armor", 45): "Probably the Cloak of Lathandar of the 1.3 readme (the line repeats Cloak of Elvenkind). "
                   "Worn: the key T turns undead as a cleric of level 7, for any class (0xADCB3). Used "
                   "(USE button): Continual Light (spell 8) as an area around you (0x37A6A).",
    ("armor", 46): "Invisibility while worn (ends when you attack; the next equipment change sets it again).",
    ("armor", 47): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 48): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 49): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 50): "Dexterity +4 (DEX 0 to 6), +2 (7 to 13) or +1 (14 to 19). Open Locks +10% for a thief; "
                   "for any other class Open Locks becomes 37%.",
    ("armor", 51): "Detect Magic while worn. The line has player_light=1 (no reader found).",
    ("armor", 52): "Strength 18/00 while worn.",
}


def dur_text(s):
    if isinstance(s, str):
        return s
    if s % 60 == 0:
        return "%d min" % (s // 60)
    return "%d s" % s


def dur_hours(s):
    return "%d h" % (s // 3600) if s % 3600 == 0 else dur_text(s)


# ---------------------------------------------------------------------
# Markdown

def md_table(head, rows, align=None):
    out = ["| " + " | ".join(head) + " |"]
    out.append("|" + "|".join((":-:" if a == "c" else "--:" if a == "r" else "---") for a in (align or "l" * len(head))) + "|")
    for r in rows:
        out.append("| " + " | ".join(str(c).replace("|", "/") for c in r) + " |")
    return "\n".join(out)


def feet(units):
    u = float(units)
    f = u / 2
    return ("%d" % f) if f == int(f) else ("%.1f" % f)


def classes_of(rec):
    if rec.has("all_use"):
        return "F T M C"
    out = []
    for flag, letter in (("fighter_use", "F"), ("thief_use", "T"), ("mage_use", "M"), ("cleric_use", "C")):
        out.append(letter if rec.has(flag) else "-")
    return " ".join(out)


def value_of(rec):
    v = rec.get("value")
    return v if v is not None else "-"


def signed(v):
    return ("+%d" % v) if v > 0 else str(v)


UNITS_NOTE = ("Distances are in game units. Two units are one foot: the fall message of the game "
              "divides by two (`fell >> 17` at the fall code, docs/re/physics.md). Times are game "
              "seconds (the game clock `GameTime`); the game messages call 60 game seconds a minute "
              "and 3600 an hour, and these pages do the same.")

SOURCE_NOTE = ("Numbers come from the item and spell lines of the table `YPINN.BIN` and from "
               "`DTUGAME.EXE` 1.3. The page is written by `tools/wiki_rules.py`.")


# ---------------------------------------------------------------------
# Pages

class Pages:
    def __init__(self, tab, ct):
        self.t = tab
        self.c = ct
        self.W = tab["$WEAPON"]
        self.A = tab["$ARMOR"]
        self.O = tab["$OBJECT"]
        self.S = tab["$SPELL"]
        self.counts = {}

    # -- helpers --------------------------------------------------------
    def slots(self, cls):
        return [self.c["slots_m"][r * 9:r * 9 + 9] for r in range(18)] if cls == "m" else \
            [self.c["slots_c"][r * 7:r * 7 + 7] for r in range(18)]

    def first_level(self, cls, spell_level):
        for r, row in enumerate(self.slots(cls)):
            if row[spell_level - 1] > 0:
                return r + 1
        return None

    def spell_self(self, s):
        # 0x56D36: +0x37 bit 0, set by self_cast_ok and by cast_in_front (0x253C7)
        r = self.S[s]
        return r.has("self_cast_ok") or r.has("cast_in_front")

    def spell_area(self, s):
        return (self.S[s].get("type") or "").upper() == "AREA"

    def count(self, page, n):
        self.counts[page] = self.counts.get(page, 0) + n

    # -- Home -----------------------------------------------------------
    def home(self):
        return "\n".join([
            "# Descent to Undermountain: game rules",
            "",
            "These pages list the rules of Descent to Undermountain 1.3 as the game data and the game "
            "code use them. The manual is not always right about them, and the game shows no "
            "description of a magic item. The numbers here come from the item and spell tables of "
            "the game files (`YPINN.BIN`, the same in every level of the mission) and from the tables "
            "and the code of `DTUGAME.EXE` 1.3, read for the ASCENT source port. Where the code ignores "
            "a value of the data, or a rule is not known, the page says so.",
            "",
            "Where ASCENT corrects a defect of the original game, the page names the correction "
            "(`Dtu_fix_...`; the option `-origcombat` and its kin turn the corrections off).",
            "",
            "## Pages",
            "",
            "- [[Races]]: ability limits and adjustments, allowed classes, special abilities",
            "- [[Classes]]: experience, hit points, THAC0, saving throws, spell slots, thief skills",
            "- [[Weapons]]: damage, speed, classes",
            "- [[Armor]]: armor class and classes",
            "- [[Magic Items|Magic-Items]]: rings, amulets, cloaks, boots, gloves, bracers, girdles and more",
            "- [[Potions]]",
            "- [[Wands]]",
            "- [[Scrolls]]",
            "- [[Wizard Spells|Wizard-Spells]]",
            "- [[Priest Spells|Priest-Spells]]",
            "",
            "## Units",
            "",
            UNITS_NOTE,
            "",
            "## Words",
            "",
            "- L: the caster level. For a spell of a spell book it is the level of the class of the spell. "
            "For a wand or a scroll the game takes the level of a class too (see [[Wands]]).",
            "- T: the level of the target. For a creature it is its maximum hit points / 8, from 1 to 18.",
            "- Save: a saving throw. The target rolls d20 and is safe at the number of its table or more "
            "([[Classes]]).",
            "- F, T, M, C: fighter, thief, mage, cleric. The game has these four classes only. The manual "
            "says priest for cleric.",
            "",
            SOURCE_NOTE,
            "",
        ])

    def sidebar(self):
        return "\n".join([
            "**[[Home]]**",
            "",
            "- [[Races]]",
            "- [[Classes]]",
            "- [[Weapons]]",
            "- [[Armor]]",
            "- [[Magic Items|Magic-Items]]",
            "- [[Potions]]",
            "- [[Wands]]",
            "- [[Scrolls]]",
            "- [[Wizard Spells|Wizard-Spells]]",
            "- [[Priest Spells|Priest-Spells]]",
            "",
        ])

    # -- Races ----------------------------------------------------------
    def races(self):
        c = self.c
        names = ["Human", "Half-elf", "Elf", "Drow", "Dwarf", "Halfling"]
        stats = ["STR", "DEX", "CON", "INT", "WIS", "CHA"]
        combo_names = {1: "F", 2: "T", 3: "F/T", 4: "M", 5: "F/M", 6: "M/T", 7: "F/M/T", 8: "C",
                       9: "F/C", 10: "C/T", 11: "F/C/T", 12: "M/C", 13: "F/M/C", 14: "M/C/T", 15: "F/M/C/T"}
        rows = []
        for i, n in enumerate(names):
            mods = c["race_mod"][i * 6:i * 6 + 6]
            lim = c["race_limits"][i * 12:i * 12 + 12]
            rows.append([n, " ".join(signed(m) if m else "0" for m in mods),
                         " ".join(str(v) for v in lim[:6]), " ".join(str(v) for v in lim[6:])])
        crow = []
        for i, n in enumerate(names):
            m = c["race_combos"][i]
            crow.append([n, ", ".join(combo_names[k] for k in range(1, 16) if m & (1 << (k - 1)))])
        trow = []
        tnames = ["Hide In Shadows", "Move Silently", "Open Locks", "Find/Remove Traps", "Climb Walls"]
        # race index of the thief tables: human, dwarf, elf, drow, halfling, half-elf
        tidx = [0, 5, 2, 3, 1, 4]
        for k, sk in enumerate(tnames):
            row = c["thief_race"][k * 6:k * 6 + 6]
            trow.append([sk] + [signed(row[tidx[i]]) if row[tidx[i]] else "0" for i in range(6)])
        d6 = c["secret_d6"]
        sec = [names[i] + " %d in 6" % d6[tidx[i]] for i in range(6)]
        self.count("Races", len(rows) + len(crow) + len(trow))
        return "\n".join([
            "# Races",
            "",
            "Six races. The race screen of the character generation sets them; the rules below are "
            "the tables of `DTUGAME.EXE` and the code that reads the race.",
            "",
            "## Ability scores",
            "",
            "Each score is rolled as 3d6 with ones rolled again (6 to 18), plus 7 bonus points to "
            "spend (the manual says 5). The racial adjustment is added; the result must be within "
            "the limits. A fighter that is not a halfling can put points past 18 into exceptional "
            "strength, 10 percent for each point. A halfling gets the percentile 01 in place of a roll.",
            "",
            md_table(["Race", "Adjustment " + " ".join(stats), "Minimum", "Maximum"], rows),
            "",
            "Class minimums: fighter STR 9, thief DEX 9, mage INT 9, cleric WIS 9.",
            "",
            "Source: adjustments 0x13FCFF, limits 0x13FB03 (fenuchar.c), bonus points 0x89B95.",
            "",
            "## Classes",
            "",
            "One class or two. No race can have three classes, and no race can be a cleric/thief.",
            "",
            md_table(["Race", "Allowed classes"], crow),
            "",
            "Source: the masks at 0x13FECE, tested at 0x88B25.",
            "",
            "## Level limits",
            "",
            "There is no level limit by race. Every class of every race stops at level 18 (0xAF43A "
            "raises a level while the level value + 1 < 18). The manual says humans can advance "
            "unlimited levels; the code has the same cap for all.",
            "",
            "## Special abilities",
            "",
            md_table(["Rule", "Races", "Source"], [
                ["Infravision (the key N works)", "all but human: half-elf, elf, drow, dwarf, halfling", "0x54632"],
                ["Resistance to Sleep", "elf: d10 <= 9 resists; half-elf: d10 <= 3; a drow has none", "0x8F766"],
                ["A save against Scare (others get none)", "elf, half-elf (also any cleric)", "0x8F467"],
                ["Move Silently with no roll, without the thief skill (not in chain or plate mail)", "elf, drow, halfling", "0xB4196"],
                ["Find/Remove Traps 50% without the thief class", "dwarf", "0x8BDE5"],
                ["The spell and the Ring of Infravision work", "human and half-elf only (race byte 0)", "0x8EBE8, 0xB0968"],
                ["Racial search for secret doors (d6) while searching (key F)", ", ".join(sec), "0xB3ECE (not in ASCENT)"],
            ]),
            "",
            "Saving throws have no racial bonus: the save function 0xAE596 reads the class and the "
            "level only. The manual gives dwarves and halflings a bonus against spells; 1.3 has none.",
            "",
            "A half-elf is stored as a human with a flag. The rules that test for a human (the spell "
            "and the ring of Infravision) treat a half-elf as a human.",
            "",
            "## Thief skills by race",
            "",
            "Added to the start values of a thief ([[Classes]]).",
            "",
            md_table(["Skill"] + names, trow),
            "",
            "Source: 0x1461C8 to 0x146228.",
            "",
            "## Differences from the manual",
            "",
            "- The manual lets only humans, elves and half-elves be mages. The game also allows drow.",
            "- The manual does not list the mage/cleric for half-elves. The game allows it.",
            "- 7 bonus points, not 5.",
            "- The manual says elves and drow are nearly immune to Sleep. In the code only the elf is.",
            "- No racial saving throw bonus, no level limits by race.",
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Classes --------------------------------------------------------
    def classes(self):
        c = self.c
        cl = [("Fighter", "f", 2), ("Thief", "t", 3), ("Mage", "m", 1), ("Cleric", "c", 0)]
        die = c["hit_die"]          # cleric, mage, fighter, thief
        xp_rows = []
        for lvl in range(1, 19):
            row = [str(lvl)]
            for n, k, idx in cl:
                row.append("{:,}".format(c["xp_" + k][lvl]) if lvl >= 2 else "0")
            xp_rows.append(row)
        th_rows = []
        for lvl in range(1, 19):
            th_rows.append([str(lvl)] + [str(c["thac0_" + k][min(lvl, 17)]) for n, k, idx in cl])
        hp_rows = []
        for n, k, idx in cl:
            d = die[idx]
            dice = c["hd_dice_" + k]
            fixed = c["hd_fixed_" + k]
            last = max(i for i in range(20) if dice[i]) + 1
            fx = fixed[last]
            hp_rows.append([n, "d%d (%d)" % (d, d), "2 to %d: %d to %d" % (last, d // 2, d),
                            "%d and on: +%d" % (last + 1, fx)])
        sv = {}
        for n, k, idx in cl:
            v = c["save_" + k]
            sv[k] = [v[i:i + 5] for i in range(0, len(v), 5)]
        div = c["save_div"]         # cleric, mage, fighter, thief
        save_rows = []
        for n, k, idx in cl:
            for r, row in enumerate(sv[k]):
                lo = r * div[idx] + 1
                hi = min((r + 1) * div[idx], 12)
                save_rows.append([n, "%d to %d" % (lo, hi)] + [str(x) for x in row])
        con = c["con_hp"]
        conf = c["con_hp_f"]
        abil = []
        for v in range(3, 20):
            str_note = ""
            abil.append([str(v), signed(c["str_hit"][v]) if c["str_hit"][v] else "0",
                         signed(c["str_dmg"][v]) if c["str_dmg"][v] else "0",
                         signed(c["dex_ac"][v]) if c["dex_ac"][v] else "0",
                         signed(con[v]) if con[v] else "0", signed(conf[v]) if conf[v] else "0",
                         "%d%% / %d%%" % (c["cha_buy"][v], c["cha_sell"][v])])
        str18 = md_table(["18/01-50", "18/51-75", "18/76-90", "18/91-99", "18/00"],
                         [["%s hit, %s dmg" % (signed(c["str_hit"][18] + c["str18_hit"][i]),
                                               signed(c["str_dmg"][18] + c["str18_dmg"][i])) for i in range(5)]])
        ms = self.slots("m")
        cs = self.slots("c")
        slot_rows = []
        for r in range(18):
            slot_rows.append([str(r + 1), " ".join(str(x) for x in ms[r] if True).rstrip(" 0") or "0",
                              " ".join(str(x) for x in cs[r]).rstrip(" 0") or "0"])
        wis = c["wis_bonus"]
        wis_rows = []
        for v in range(9, 20):
            wis_rows.append([str(v)] + [str(wis[k * 20 + v]) for k in range(4)])
        tdex = c["thief_dex"]
        tdex_rows = []
        for v in range(9, 20):
            tdex_rows.append([str(v)] + [signed(tdex[k * 21 + v]) if tdex[k * 21 + v] else "0" for k in range(4)])
        turn = c["turn"]
        tnames = ["Skeleton", "Zombie", "Ghoul", "Baneguard", "Ghast", "Mummy", "Guardian skeleton", "Lich", "Shadow fiend"]
        tcode = {-1: "T", -2: "D", -3: "D", -4: "-"}
        turn_rows = []
        for i, n in enumerate(tnames):
            turn_rows.append([n] + [tcode.get(x, str(x)) for x in turn[i * 14:i * 14 + 14]])
        self.count("Classes", len(xp_rows) + len(th_rows) + len(hp_rows) + len(save_rows) + len(abil)
                   + len(slot_rows) + len(wis_rows) + len(tdex_rows) + len(turn_rows))
        shock = c["shock"]
        return "\n".join([
            "# Classes",
            "",
            "Four classes: fighter, thief, mage, cleric (the manual says priest). There is no paladin "
            "and no ranger. A character has one class or two ([[Races]]).",
            "",
            "## Class rules",
            "",
            md_table(["Class", "Hit die", "Weapons and armor", "Special"], [
                ["Fighter", "d%d" % die[2], "the items with the fighter flag",
                 "exceptional strength 18/xx (not a halfling); the best THAC0; more hit points from CON"],
                ["Thief", "d%d" % die[3], "the items with the thief flag; chain and plate mail stop three skills",
                 "thief skills, backstab"],
                ["Mage", "d%d" % die[1], "the items with the mage flag; no mage spell in armor without the flag MAGIC_OK",
                 "mage spells"],
                ["Cleric", "d%d" % die[0], "the items with the cleric flag", "cleric spells, turn undead (key T)"],
            ]),
            "",
            "Each item line has class flags (`fighter_use`, `thief_use`, `mage_use`, `cleric_use`, "
            "`all_use`); the item pages show them. The game has the texts \"You cannot use this "
            "object.\" (825) and \"You cannot use this armor.\" (828), but the place where 1.3 tests "
            "the flags was not found. ASCENT refuses an item that has none of the flags of your "
            "classes.",
            "",
            "Mage spells in armor: 0x41A4D refuses a mage spell (\"You cannot cast a spell while "
            "wearing %s.\") when a worn armor item lacks the flag `MAGIC_OK`. Of the armor lines only "
            "the rings, amulets, cloaks, boots, bracers, girdle and gloves have it ([[Armor]]).",
            "",
            "## A new character",
            "",
            "- Hit points: the full hit die plus the Constitution adjustment (the fighter column for a "
            "fighter). Two classes: the mean of the two, rounded down (0x8BA3A).",
            "- Gold: 102. Each character gets a Scroll (misc 102) first.",
            "- Fighter: Broad Sword, Leather Armor, Gem of Curing, Potion of Healing.",
            "- Thief: Short Sword, Leather Armor, Apprentice Lockpicks, Potion of Healing.",
            "- Mage: Dagger, Potion of Healing, and six spells: min(INT, 18) / 3 of Armor, Burning "
            "Hands, Chill Touch, Color Spray, Detect Magic, Feather Fall, Jump, Light, Magic Missile, "
            "Protection From Evil, Shield, Sleep, Spider Climb at random, the rest Sleep.",
            "- Cleric: Mace, Leather Armor, Potion of Healing, and the spell Abjure. No code that "
            "gives a cleric the other spells at a new level was found: cleric spells, as mage "
            "spells, come from spell items that you pick up (0xB69AD). The manual says a priest "
            "gets every spell of a new spell level.",
            "- The empty hand does 1d2 plus the strength bonuses.",
            "",
            "Source: 0x8BB91, 0x8BEBC, the start items at 0x13F6CD.",
            "",
            "## Experience",
            "",
            "Experience for each level. With two classes each gained point is halved (at least 1) and "
            "goes to both classes. Level 18 is the last level.",
            "",
            md_table(["Level", "Fighter", "Thief", "Mage", "Cleric"], xp_rows, "rrrrr"),
            "",
            "Source: 0x145EF8, 0x145FF4, 0x145F4C, 0x145FA0 (0xAF43A).",
            "",
            "## Hit points",
            "",
            "At each new level: one hit die, rolled as half the die to the full die (`rand() % (die - "
            "die/2 + 1) + die/2`), then a fixed number. The Constitution adjustment is added at every "
            "level, also to the fixed number. With two classes the sum of the new levels is halved.",
            "",
            md_table(["Class", "Hit die (level 1)", "Levels with a die: hit points", "Fixed"], hp_rows),
            "",
            "Source: 0xAF694, tables 0x146518, 0x146298 to 0x146388, 0x1463D8 to 0x1464C8. The manual "
            "shows the fixed numbers one level earlier (fighter and cleric from level 10).",
            "",
            "## THAC0",
            "",
            "To hit: the attack hits when d20 + bonuses >= THAC0 - armor class of the target. A 20 is "
            "a critical hit (the damage is rolled twice), a 1 a fumble (a second d20 above your "
            "Dexterity: you drop the weapon).",
            "",
            md_table(["Level", "Fighter", "Thief", "Mage", "Cleric"], th_rows, "rrrrr"),
            "",
            "The table is read at level + 1 (0xAF605), so the values are one level ahead of the AD&D "
            "rules: a first level fighter has 19. The index stops at 17 (0xAF605), so level 18 has the "
            "value of level 17. Two classes: 1.3 takes the worse value. ASCENT "
            "(Dtu_fix_multiclass_thac0) takes the better one.",
            "",
            "Source: 0x14656C, 0x14655A, 0x14657E, 0x146548.",
            "",
            "## Ability scores",
            "",
            md_table(["Score", "STR to hit", "STR damage", "DEX armor class", "CON hit points",
                      "CON (fighter)", "CHA market buy / sell"], abil, "rrrrrrr"),
            "",
            "Strength 18 of a fighter (0x2A7B9):",
            "",
            str18,
            "",
            "Charisma sets the market prices: the percent of the value that you pay or get "
            "(0x86F75). Intelligence decides the number of first spells of a mage. Wisdom gives "
            "clerics bonus spells (below) and shortens Fear, Scare, Hold Person and the stun of "
            "Color Spray on you: the time is divided by (Wisdom / 2). Dexterity is also the number "
            "the second d20 of a fumble must not exceed.",
            "",
            "Source: 0x13A956, 0x13A9A2, 0x13A9F2, 0x13AA06, 0x14019A, 0x1402E2, 0x140332, 0x13F2DD, "
            "0x13F32D. The statistics screen also shows a column for Intelligence (\"spells\": %s for "
            "INT 9 to 19, table 0x14023E); no rule that reads it was found."
            % ", ".join(str(x) for x in c["int_spells"][9:20]),
            "",
            "## Saving throws",
            "",
            "d20 >= the number. The level is the higher class level, at most 12. Two classes: the "
            "better number of the two tables at that level. No adjustment for race, Constitution, "
            "Wisdom or magic items was found (0xAE596). Spells use the column Spell.",
            "",
            md_table(["Class", "Levels", "Paralyzation, poison, death", "Rod, staff, wand",
                      "Petrification, polymorph", "Breath", "Spell"], save_rows),
            "",
            "Source: rows from 0x145642, divisors 0x1456E2.",
            "",
            "## Spell slots",
            "",
            "Slots for each spell level, by class level (the AD&D tables). A rest (key R at a rest "
            "place) and the start of each dungeon level fill them (0x3551B). A spell above the class level + 1 is refused. The "
            "game has spells of levels 1 to 4 only.",
            "",
            md_table(["Level", "Mage: spell levels 1 to 9", "Cleric: spell levels 1 to 7"], slot_rows),
            "",
            "Cleric bonus slots for Wisdom (only for a spell level that has a slot):",
            "",
            md_table(["WIS", "Level 1", "Level 2", "Level 3", "Level 4"], wis_rows, "rrrrr"),
            "",
            "No Intelligence bonus for mages. Source: 0x145CD8, 0x145DF8, 0x1403D2 to 0x1404C2 "
            "(0x3575B).",
            "",
            "## Thief skills",
            "",
            "A new thief starts with Hide In Shadows 5%, Move Silently 10%, Open Locks 10%, "
            "Find/Remove Traps 5%, Climb Walls 60%, plus the race ([[Races]]) and the Dexterity "
            "adjustment (Climb Walls has none). Each skill is limited to 0 to 95%.",
            "",
            md_table(["DEX", "Hide In Shadows", "Move Silently", "Open Locks", "Find/Remove Traps"],
                     tdex_rows, "rrrrr"),
            "",
            "A skill roll succeeds when d100 <= the skill (0xB3038). After a level the thief spends "
            "skill points on the screen of `thiefpts.c`; the number of points was not read, and the "
            "screen is not in ASCENT.",
            "",
            md_table(["Item or effect", "Change"], [
                ["Chain mail or plate mail (armor 3, 4, 7 to 11)",
                 "-250 to Hide In Shadows, Move Silently, Climb Walls (so 0); Move Silently and Climb "
                 "Walls refuse with the armor message"],
                ["Gauntlets (misc 9)", "-250 to Find/Remove Traps and Open Locks"],
                ["Lockpicks in the lockpick place (thief only)",
                 "Lockpicks +5, Apprentice +10, Journeyman +15, Master +20 to Open Locks"],
                ["Gloves of Dexterity", "Open Locks +10 (thief); 37 for others"],
                ["Cloak of Elvenkind (armor 43)", "Hide In Shadows 95"],
                ["Invisible (spell, potion, cloak)", "Open Locks -15"],
                ["Spider Climb, Levitate", "Climb Walls 100"],
                ["Find Traps (spell 51)", "Find/Remove Traps 100"],
                ["Sword of W'hin in the hand", "Find/Remove Traps 250 (95 after the limit)"],
            ]),
            "",
            "- Open Locks (key O): one try for each lock and thief level. A lock with the key flag "
            "\"magic\" cannot be picked (\"Strong magic prevents you from picking this lock.\").",
            "- Find/Remove Traps (key F): search mode, movement / 3. One find roll for each trap and "
            "level; then you choose to remove it; a failed removal at d100 >= 96 sets the trap off.",
            "- Move Silently (key M) and Hide In Shadows (a roll each time you stop): creatures do not "
            "notice you. An attack ends hiding.",
            "- Climb Walls: the Activate key at a wall; no weapon in the hand. A slip on a failed roll.",
            "- Backstab: a melee attack while Move Silently is on, on a humanoid creature, while no "
            "creature watches you: +4 to hit, damage x (thief level + 7) / 4 (x2 at levels 1 to 4, "
            "x3 at 5 to 8, x4 at 9 to 12, x5 at 13 to 16, x6 at 17 and 18). The Dagger of Impaling "
            "adds 3 to 6 on a roll of 19 or 20.",
            "",
            "Source: combat.md B7 (0xB31BD, 0xB3400, 0xB4196, 0xB333F, 0x951DE, 0xB337E), 0xAFB55.",
            "",
            "## Turn undead",
            "",
            "Key T, a cleric (the cleric level), or anyone who wears armor 45 (as a cleric of level 7). "
            "One d20 for all undead creatures in sight; at most 2 to 12 are destroyed. A number: the "
            "d20 must reach it. T: turned. D: destroyed. -: no effect (you can try again later).",
            "",
            md_table(["Undead"] + [str(i) for i in range(1, 15)], turn_rows),
            "",
            "Levels above 14 count as 14. Source: 0xADC46, table 0xA6487.",
            "",
            "## Rest",
            "",
            "Key R at a rest place (the turning ankh): hit points to the maximum and all spell slots "
            "full. The place works again after 300 s (0x3E1F6). Potion of Toughening: the system shock roll "
            "for Constitution 1 to 19 is %s percent." % ", ".join(str(x) for x in shock),
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Weapons --------------------------------------------------------
    def weapon_row(self, i, r):
        n = r.num("#damage_dice", 1)
        s = r.num("damage_dice", 0)
        b = r.num("damage_bonus", 0)
        h = r.num("to_hit_bonus", 0)
        dice = ("%dd%d" % (n, s)) if s else "no dice"
        if b:
            dice += signed(b) if s else ", %s" % signed(b)
        fs = r.num("fire_damage_dice", 0)
        fn = r.num("#fire_damage_dice", 0)
        dtype = "normal"
        if fs and fn:
            dtype = "normal + fire %dd%d" % (fn, fs)
        fw = r.num("fire_wait", 0)
        melee = (r.get("attack_type") or "").upper() != "PROJECTILE"
        if melee:
            speed = "%.1f s" % max(float(fw or 1.0), 1.0)
        else:
            speed = ("%.1f s" % float(fw)) if fw else "-"
        notes = []
        if not s:
            notes.append("the line has no damage dice (the damage is not known)")
        if r.get("weapon_name") and r.get("name") and r.get("weapon_name") != r.get("name"):
            notes.append("line %d, weapon_name %s" % (i, r.get("weapon_name")))
        ac = self.c["ac_weapon"][i] if i < 83 else 0
        if ac:
            notes.append("armor class %s in the hand" % signed(ac))
        if i in WEAPON_NOTES:
            notes.append(WEAPON_NOTES[i])
        return [r.name(), dice, signed(h) if h else "0", dtype, speed,
                "yes" if r.has("magic") else "no", classes_of(r), value_of(r), "; ".join(notes)]

    def weapons(self):
        melee, thrown, ammo, other = [], [], [], []
        for i, r in enumerate(self.W):
            if i in WEAPON_INTERNAL or r.get("pc_spell"):
                if not r.get("pc_spell"):
                    other.append([str(i), r.name(), WEAPON_INTERNAL.get(i, "")])
                continue
            proj = (r.get("attack_type") or "").upper() == "PROJECTILE"
            row = self.weapon_row(i, r)
            if not proj:
                melee.append(row)
            elif r.get("thrown") == "1":
                thrown.append(row)
            else:
                kind = "arrow" if r.has("arrow") else "bolt" if r.has("bolt") else "other"
                ammo.append([row[0], kind] + row[1:])
        launch = []
        for i, r in enumerate(self.O):
            if r.has("bow") or r.has("crossbow"):
                note = ""
                if r.get("pc_spell1"):
                    note = "pc_spell1=%s in the line (Haste); no code reads it" % r.get("pc_spell1")
                launch.append([r.name(), "bow" if r.has("bow") else "crossbow", signed(r.num("to_hit_bonus", 0)),
                               classes_of(r), r.num("value", 0), note])
        head = ["Weapon", "Damage", "To hit", "Damage type", "Speed", "Magic", "F T M C", "Value", "Notes"]
        melee.sort(key=lambda x: x[0])
        thrown.sort(key=lambda x: x[0])
        ammo.sort(key=lambda x: (x[1], x[0]))
        self.count("Weapons", len(melee) + len(thrown) + len(ammo) + len(launch) + len(other))
        return "\n".join([
            "# Weapons",
            "",
            "Damage: the dice of the line (`#damage_dice` dice of `damage_dice` sides) plus "
            "`damage_bonus`. Melee weapons add the Strength damage, missiles add the to-hit bonus "
            "of the bow or crossbow to the attack and the damage. A critical hit (20) rolls the "
            "damage twice. A backstab multiplies it ([[Classes]]).",
            "",
            "Speed: a melee swing lasts max(`fire_wait`, 1.0) s (0x441E9), so every weapon faster "
            "than 1.0 s swings at 1.0 s; Slow doubles it. A thrown or missile weapon fires once each "
            "`fire_wait` s while the key is held (a new key press can fire sooner).",
            "",
            "Damage type: the game has no slashing, piercing or bludgeoning types and no weapon "
            "sizes. It has fire dice (`fire_damage_dice`, which fire resistance halves) and the flag "
            "`magic`: a creature with `weapon_resistant` takes no damage from a weapon that is not "
            "magic (0x29D9D).",
            "",
            "Classes: the flags of the line (F fighter, T thief, M mage, C cleric). Where 1.3 tests "
            "them was not found; ASCENT refuses a weapon of another class ([[Classes]]). There is no "
            "paladin or ranger.",
            "",
            "## Melee weapons",
            "",
            md_table(head, melee),
            "",
            "## Thrown weapons",
            "",
            "Thrown in the direction of the view; the stack is used up one by one. A thrown weapon is a "
            "missile: no Strength bonus, and as any missile without the flag `arrow` it gets the to-hit "
            "bonus of a worn crossbow (0x2AB0A).",
            "",
            md_table(head, thrown),
            "",
            "## Arrows and bolts",
            "",
            "An arrow needs a bow, any other missile the crossbow (0x2AB0A tests the flag `arrow`).",
            "",
            md_table(["Weapon", "Kind"] + head[1:], ammo),
            "",
            "## Bows and crossbows",
            "",
            "Misc items in the bow or crossbow place of the paper doll. The to-hit bonus is added to "
            "the attack and the damage of the missile.",
            "",
            md_table(["Launcher", "Kind", "To hit and damage", "F T M C", "Value", "Notes"], launch),
            "",
            "## Wands",
            "",
            "Weapon lines with `pc_spell` cast a spell: see [[Wands]].",
            "",
            "## Other weapon lines",
            "",
            "Lines for spells, traps and creatures, and lines that the tables mark as unused.",
            "",
            md_table(["#", "Line", "Use"], other),
            "",
            "## Differences from the manual",
            "",
            "- The manual gives thieves daggers, short swords, long swords, scimitars, clubs, darts, "
            "throwing daggers and short bows. The lines also give thieves the Broad Sword, the Long "
            "Bow is fighter only as the manual says, and arrows have the thief flag.",
            "- The manual gives mages daggers and darts. The Throwing Dagger also has the mage flag.",
            "- The manual says the dagger is fast. In 1.3 every melee weapon below 1.0 s swings at 1.0 s.",
            "- The Warhammer does 3d10, more than any sword; the Warhammer +1 does 1d6+1.",
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Armor ----------------------------------------------------------
    def armor(self):
        rows = []
        for i, r in enumerate(self.A):
            if not (r.has("armor") or r.has("shield")) or 39 <= i <= 42:
                continue
            ac = self.c["ac_armor"][i]
            dec = r.num("armor_class_decrement", None)
            note = []
            if i in (3, 4, 7, 8, 9, 10, 11):
                note.append("stops Hide In Shadows, Move Silently, Climb Walls")
            if 5 <= i <= 11:
                note.append("magic armor: no ring counts; no Cloak of Protection")
            if r.has("shield"):
                note.append("no Cloak of Protection")
            if dec is not None and -dec != ac:
                note.append("the line says %d" % dec)
            rows.append([r.name(), "shield" if r.has("shield") else "body", signed(-ac) if ac else "0",
                         str(10 + ac), "yes" if r.has("magic_ok") else "no", classes_of(r), r.num("value", 0),
                         "; ".join(note)])
        for i in (8, 9):
            r = self.O[i]
            note = "boot place" if i == 8 else "gauntlet place; -250 to Find/Remove Traps and Open Locks"
            ac = self.c["ac_misc"][i]
            rows.append([r.name(), "misc item", signed(-ac), str(10 + ac), "not tested (misc)", classes_of(r),
                         r.num("value", 0), note])
        self.count("Armor", len(rows))
        return "\n".join([
            "# Armor",
            "",
            "Armor class starts at 10. Each worn item lowers it by its value. The value is not read "
            "from the line: the executable has its own table by item number (0x145C04), which "
            "agrees with `armor_class_decrement` of the lines where they have it. Then the spell "
            "limits (Armor 6, Shield 3, Magical Vestment), Slow +4, the Dexterity adjustment, "
            "Invulnerability -2, and the limit -10 to 10 (0xAFB55).",
            "",
            "Mage spells: a mage cannot cast while wearing an item without the flag `MAGIC_OK`. No "
            "body armor and no shield has it. The flag `thief_ok` is in the lines, but the thief "
            "rules test the item numbers, not the flag.",
            "",
            "AC bonus +2 lowers your armor class by 2; AC alone is the armor class with only this item "
            "and Dexterity 7 to 14.",
            "",
            "Rings, amulets, cloaks, boots, bracers, gloves and the girdle are on [[Magic Items|Magic-Items]].",
            "",
            md_table(["Armor", "Place", "AC bonus", "AC alone", "Mage spells", "F T M C", "Value", "Notes"], rows),
            "",
            "The paper doll has no helmet place, although the string \"Put Helmet here\" exists.",
            "",
            "## Differences from the manual",
            "",
            "- The manual says only fighters and priests can use shields. The Shield lines also have "
            "the thief flag.",
            "- The manual lists helms, leather gauntlets and metal gauntlets. The game has one misc "
            "item \"Gauntlets\" (fighter and cleric, AC 1) and no helm.",
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Magic items ----------------------------------------------------
    def magic_items(self):
        groups = collections.OrderedDict([
            ("Rings", ("ring",)), ("Amulets", ("necklace",)), ("Cloaks", ("cloak",)),
            ("Boots", ("boot",)), ("Gauntlets and gloves", ("gauntlet",)),
            ("Bracers", ()), ("Girdles", ("belt",)),
        ])
        out = ["# Magic items", "",
               "The game shows no description of an item. This page says what each one does, from the "
               "code of `DTUGAME.EXE` 1.3. The effects are code for single item numbers (0xAFB55); "
               "the keywords `pc_spell`, `set_strength` and similar of the lines are not read. An "
               "effect of a worn item has no end while you wear it and ends when you take it off.",
               "", "AC bonus: the value of the executable table (0x145C04); +2 lowers your armor class by 2.", ""]
        total = 0
        for title, flags in groups.items():
            rows = []
            for i, r in enumerate(self.A):
                if title == "Bracers":
                    if not 39 <= i <= 42:
                        continue
                elif not any(r.has(f) for f in flags) or 39 <= i <= 42:
                    continue
                if r.has("armor") and not 39 <= i <= 42:
                    continue
                ac = self.c["ac_armor"][i]
                eff = WORN.get(("armor", i), "")
                if title == "Bracers":
                    eff = "Worn in the armor place, so not with body armor. Magic armor for the rules: no "\
                          "ring and no Cloak of Protection counts with it."
                if title == "Rings" and i in (0, 14, 15):
                    eff = "Counts only without magic armor or bracers. Of two rings of protection only the better counts."
                dec = r.num("armor_class_decrement", None)
                if i == 33:
                    ac = 0
                if ac and dec is None:
                    eff = (eff + " " if eff else "") + "The AC bonus is in the executable table only; the line has none."
                rows.append([r.name(), signed(-ac) if ac else "0", classes_of(r), r.num("value", 0), eff or "AC only"])
            if title == "Amulets":
                rows.append([self.O[5].name(), "0", classes_of(self.O[5]), "-", "Quest item (misc 5). No effect found."])
            if title == "Cloaks":
                rows.append([self.O[6].name(), "0", classes_of(self.O[6]), self.O[6].num("value", 0), "Plain cloak (misc 6). No effect."])
            if title == "Girdles":
                rows.append([self.O[7].name(), "0", classes_of(self.O[7]), self.O[7].num("value", 0), "Plain belt (misc 7). No effect."])
            total += len(rows)
            out += ["## " + title, "", md_table(["Item", "AC bonus", "F T M C", "Value", "Effect"], rows), ""]
            if title == "Rings":
                out += ["Two ring places.", ""]
        wrows = []
        for i in (54, 69, 70, 71, 45):
            r = self.W[i]
            wrows.append([r.name(), WEAPON_NOTES.get(i, "")])
        r = self.O[100]
        wrows.append([r.name(), "A bow with +2 to hit and damage. The line has pc_spell1=21 (Haste); no code reads it."])
        out += ["## Weapons with powers", "", "Damage and classes: [[Weapons]].", "",
                md_table(["Weapon", "Power"], wrows), ""]
        orows = [
            [self.O[2].name(), "USE: if you are blind, it cures the blindness; else if you are hurt, it casts Cure "
                               "Serious Wounds (3 to 17). Not used up. A counter limits the uses (0x8445A, not "
                               "decoded); ASCENT allows one use for each rest."],
            [self.O[101].name(), "USE: a script event (0x1FE) of the quest."],
            [self.O[0].name(), "The line has player_light=3. No code that reads it was found."],
            [self.O[10].name() + ", " + self.O[13].name() + ", " + self.O[11].name() + ", " + self.O[12].name(),
             "In the lockpick place, for a thief: Open Locks +5, +10, +15, +20."],
            ["Spider Piece (misc 18)", "USE with charges: turns undead as a cleric of level 2."],
            ["Spider Piece (misc 19)", "USE with charges: the whole map is known."],
            ["Spider Piece (misc 20)", "USE with charges: each creature in sight that has a flag (Robot_info+0xD9 bit 3) "
                                       "saves or is killed, gets one of the spells %s, or is frozen. Not in ASCENT."
             % ", ".join(str(s) for s in self.c["spider_spells"])],
            ["Spider Piece (misc 22)", "USE with charges: Fear around you, and Aid, Magical Vestment, Fly, Protection "
                                       "From Fire, Resist Fire, Flame Walk, Strength on you."],
            ["Spider Piece (misc 23)", "USE with charges: the view visits each creature for 1 to 3 s. Not in ASCENT."],
            ["Spider Piece (misc 24)", "USE with charges: a script event (0x1FC). Not in ASCENT."],
            ["Assembled Spider (misc 25)", "USE with charges: Spider Climb, Free Action, Haste and Infravision with no end."],
            ["Spider Piece (misc 26)", "USE with charges: all hit points, and Aid, Shield, Feather Fall, Improved "
                                       "Invisibility, Magical Vestment, Prayer, Protection From Evil, Resist Fire, "
                                       "Stone Skin; then Cure Blindness or Deafness if needed."],
            ["Spider Piece (misc 21)", "No use found."],
        ]
        total += len(wrows) + len(orows)
        out += ["## Other items with a use", "",
                "Source: 0x37A6A (combat.md B6).", "",
                md_table(["Item", "Effect"], orows), "",
                "Potions, wands and scrolls have their own pages: [[Potions]], [[Wands]], [[Scrolls]].", "",
                "## Notes", "",
                "- The 1.3 readme lists new items: Potion of Invisibility, Ring of Wizardry, Cloak of "
                "Lathandar, Drowslayer and +3 Longsword. In the tables the Potion of Invisibility is the "
                "second \"Potion of Agility\" (misc 83), the Ring of Wizardry is \"Ring of Warmth\" "
                "(armor 27), and the Cloak of Lathandar is probably the second \"Cloak of Elvenkind\" "
                "(armor 45). No line is named Drowslayer or +3 Longsword. Weapon 13 has code for +2 to "
                "hit and double damage against creatures with a flag (Robot_info+0xD9 bit 8) and a "
                "use that casts Continual Light, Free Action and Protection From Evil for a Lawful "
                "Good character; in the 1.3 tables line 13 is a missile line named Wand of Lightning "
                "Bolts, so that code is never reached.",
                "- The game has no identification of items.", "",
                SOURCE_NOTE, ""]
        self.count("Magic-Items", total)
        return "\n".join(out)

    # -- Potions --------------------------------------------------------
    def potions(self):
        rows = []
        for i in range(72, 89):
            r = self.O[i]
            eff, src = POTIONS[i]
            rows.append([r.name(), eff, r.num("value", 0), src])
        self.count("Potions", len(rows))
        return "\n".join([
            "# Potions",
            "",
            "Use a potion with the USE button or from a hot key. Each use takes one potion. The "
            "effect is code for the item number (0x37A6A); the keywords `pc_spell2`, `duration` and "
            "`set_*` of the lines are not read. A new potion of a kind that runs keeps the later end "
            "time. The Potion of Anti-Magic and Dispel Magic end all potion effects.",
            "",
            md_table(["Potion", "Effect", "Value", "Source"], rows),
            "",
            "\"rounds\" in the game texts are 60 s. A potion that casts a spell (Flying, Jumping) uses "
            "the spell with your caster level ([[Wizard Spells|Wizard-Spells]]).",
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Wands ----------------------------------------------------------
    def wands(self):
        rows = []
        for i, r in enumerate(self.W):
            sp = r.get("pc_spell")
            if not sp:
                continue
            s = int(sp)
            sname = SPELLS.get(s, {}).get("name") or self.S[s].name()
            note = ""
            if i == 69:
                note = "A short sword (1d6+2, +2 to hit); see below."
            if i == 79:
                note = "A creature weapon (spell 51, Find Traps by its code)."
            if s == 12:
                note = "Detect Magic is self-cast: the wand casts it on you."
            if s in (16, 32):
                note = "1.3 divides by 0 dice sides here (see the spell); ASCENT: the damage for each level."
            rows.append([r.name(), "%s (%d)" % (sname, s), "?" if i in (69, 79) else r.num("uses", 1), classes_of(r),
                         value_of(r), note])
        self.count("Wands", len(rows))
        return "\n".join([
            "# Wands",
            "",
            "A wand is a weapon line with `pc_spell`: the attack with it in the hand casts that spell "
            "(0x56A0D), with no cast time and no spell slot. Each cast takes one charge; the "
            "charges are `uses` of the line. At 0 the wand is gone (0xB7084).",
            "",
            "Caster level: 0x91EC1 gives the level of the class of the spell. For a mage spell it "
            "takes the level of your first class that is not a cleric (or the second): a fighter of "
            "level 8 casts a wand of Magic Missiles at level 8. A cleric spell takes the cleric "
            "level. A character with no class of the right kind casts at level 1. ASCENT gives a "
            "character with no mage class level 1 for a mage spell: this differs from 1.3.",
            "",
            md_table(["Wand", "Spell", "Charges", "F T M C", "Value", "Notes"], rows),
            "",
            "The spell rules: [[Wizard Spells|Wizard-Spells]].",
            "",
            "The Short Sword of Quickness has `pc_spell=21` (Haste). By the rule above the attack "
            "with it casts Haste on you like a wand; that this replaces its melee hit and how many "
            "charges it has was not checked.",
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Scrolls --------------------------------------------------------
    def scrolls(self):
        rows = []
        for i in (102, 106, 126, 127):
            rows.append([self.O[i].name() + " (misc %d)" % i, SCROLLS[i]])
        self.count("Scrolls", len(rows))
        return "\n".join([
            "# Scrolls",
            "",
            "## Spell scrolls",
            "",
            "A spell in the world is an item of the spell table. When you pick it up, a mage learns "
            "a mage spell and a cleric learns a cleric spell (into the spell book, 0xB69AD). A spell "
            "that no book of yours takes stays in the item list. With it in the hand, the attack "
            "casts it as a scroll: no cast time, no spell slot, one item used (0x56A0D). The caster "
            "level is the one of [[Wands]].",
            "",
            "To put a spell item in a hot key box the character must be a mage or a cleric and the "
            "class flags of the spell must fit, except for Protection From Evil (39 and 53) and "
            "Protection From Fire (40), which anyone can use (0x3533C).",
            "",
            "## Other scrolls",
            "",
            md_table(["Scroll", "Effect"], rows),
            "",
            SOURCE_NOTE,
            "",
        ])

    # -- Spells ---------------------------------------------------------
    def spell_page(self, cls):
        flag = "mage_use" if cls == "m" else "cleric_use"
        title = "Wizard spells" if cls == "m" else "Priest spells"
        nums = [i for i, r in enumerate(self.S) if r.has(flag)]
        by_level = collections.defaultdict(list)
        for i in nums:
            by_level[self.S[i].num("level", 1)].append(i)
        out = ["# " + title, "",
               ("Mage spells. The manual says wizard." if cls == "m" else "Cleric spells. The manual says priest."),
               "",
               "Each row is one line of the spell table, with the rule of the code (the cases of "
               "0x8CFFF). L is the caster level: the %s level + 1 counted from 1. T is the level of "
               "the target (a creature: maximum hit points / 8, 1 to 18). " % ("mage" if cls == "m" else "cleric")
               + UNITS_NOTE, "",
               "- Cast: the time of the line, in game seconds (the cast starts and the spell fires that "
               "much later; a hit on you stops it and the slot is gone). A scroll or wand casts at once.",
               "- Way: \"self\" spells act on the caster (the line has `self_cast_ok` or `cast_in_front`, "
               "0x56D36); an area of a self spell is around the caster. The others fire missiles at "
               "100 units/s that live 30 s unless named; the effect is on what the missile hits.",
               "- Range: the `range` of the line. No code reads it (only debug prints); a missile flies "
               "until it hits or its life ends.",
               "- Area: the radius of an AREA line, read by 0x90CEF. Lines of type TARGET also have "
               "a radius, but it is not used.",
               "- Save: the saving throw of the target (category spell). \"half\": half damage.",
               "- Damage of all spells is reduced for creatures by their immunities (fire_immune, "
               "electricity_immune, fire_resistant), by Resist Fire, Flame Walk and Protection From Fire.",
               ""]
        rows_total = 0
        unused = []
        for lvl in sorted(by_level):
            first = self.first_level(cls, lvl)
            out += ["## Level %d" % lvl, "",
                    "First slot of this level at class level %d." % first if first else "", ""]
            rows = []
            lvrows = collections.OrderedDict()
            spells = sorted(by_level[lvl], key=lambda s: (self.spell_name(s), s))
            for s in spells:
                r = self.S[s]
                rule = SPELLS[s]
                way = "self" if self.spell_self(s) else "missile"
                if way == "missile" and s in MISSILE_COUNT_TEXT:
                    way = "missiles: " + MISSILE_COUNT_TEXT[s]
                if s in MISSILE_LIFE:
                    way += ", life %s" % ("2 s" if s == 32 else "(L/3 + 1) min")
                rng = r.num("range", 0)
                rng_t = "%s (%s ft)" % (rng, feet(rng)) if rng else "0"
                if r.num("level_range", 0):
                    rng_t += " + %s/L" % r.num("level_range")
                area = "-"
                if self.spell_area(s):
                    rad = r.num("radius", 0)
                    area = "radius %s (%s ft)" % (rad, feet(rad))
                dur = rule["dur"]
                dtxt = dur_text(dur) if not callable(dur) else self.dur_formula(s)
                name = self.spell_name(s)
                rows.append([name, str(s), "%d s" % r.num("cast_time", 0), way, rng_t, area, dtxt,
                             rule.get("save", "-"), rule.get("eff", "")])
                for col, fn in rule.get("lv", []):
                    lvrows["%s: %s" % (name, col)] = fn
                if r.get("duration") is not None or r.get("level_duration") is not None:
                    unused.append([name, str(s), r.get("duration") or "-", r.get("level_duration") or "-"])
            rows_total += len(rows)
            out += [md_table(["Spell", "#", "Cast", "Way", "Range (line, not used)", "Area", "Duration", "Save",
                              "Effect"], rows), ""]
            if lvrows:
                cols = list(lvrows)
                lrows = []
                start = 1
                for L in range(start, 19):
                    lrows.append([str(L)] + [lvrows[k](L) for k in cols])
                rows_total += len(lrows)
                out += ["By caster level:", "", md_table(["L"] + cols, lrows), ""]
        out += ["## Notes", ""]
        out += self.spell_manual_notes(cls)
        unused.sort()
        rows_total += len(unused)
        out += ["", "## Durations of the lines", "",
                "The lines have `duration` and `level_duration`. The spell code does not use them; each "
                "case has its own time (the Duration column). The only reader is 0x43E3A, when a spell "
                "is taken in the hand and its `uses` is -1 (no line has that): it stores GameTime + "
                "`duration` + (L + 1) x `level_duration` at P+0x47, which no code reads. The values, "
                "for comparison with the manual:", "",
                md_table(["Spell", "#", "duration", "level_duration"], unused)]
        out += ["", SOURCE_NOTE, ""]
        self.count("Wizard-Spells" if cls == "m" else "Priest-Spells", rows_total)
        return "\n".join(out)

    def spell_name(self, s):
        n = self.S[s].name()
        code = SPELLS[s].get("name")
        if s == 51:
            return "Burning Hands (ice): Find Traps"
        if code:
            return "%s (works as %s)" % (n, code)
        return n

    def dur_formula(self, s):
        rule = SPELLS[s]
        f = rule["dur"]
        # describe by sampling: a + b*L in minutes or hours
        v1, v2 = f(1), f(2)
        b, a = v2 - v1, v1 - (v2 - v1)
        unit, name = (3600, "h") if b and b % 3600 == 0 and a % 3600 == 0 else (60, "min")
        if b == 0:
            return dur_text(v1)
        ka, kb = a // unit, b // unit
        if ka == 0:
            return "%s L %s" % ("" if kb == 1 else str(kb), name) if kb != 1 else "L %s" % name
        if kb == 1:
            return "%d + L %s" % (ka, name) if ka > 0 else "L - %d %s" % (-ka, name)
        return "%d + %dL %s" % (ka, kb, name)

    def spell_manual_notes(self, cls):
        if cls == "m":
            return [
                "- Chill Touch, Knock: as read, no effect when the player casts them (see the rows). "
                "Test in the DOS game.",
                "- Fireball, Lightning Bolt, Melf's Acid Arrows: the 1.3 dice have 0 sides; the code "
                "divides by 0 (as read, not tested). ASCENT counts only the damage for each level.",
                "- Fireball: the 1.3 readme says a Fireball that bursts close may blind or deafen. "
                "That code (blind 1 to 1800 s, deaf) is in the area function 0x90CEF, which a TARGET "
                "line like Fireball does not reach, as read. Not in ASCENT.",
                "- Dispel Magic (mage) has cast_in_front and so acts on the caster (as read). ASCENT "
                "fires it as a missile.",
                "- Light and Continual Light have cast_in_front: an area around the caster (as read). "
                "ASCENT fires them as missiles.",
                "- The manual lists Flame Arrow (mage level 3). There is no such spell; weapon line 8 "
                "is a Flame Arrow missile (1d6 + 4d6 fire, mage flag) ([[Weapons]]).",
                "- Manual: Magic Missile gives 2 missiles at level 3; the game at level 2. Minute "
                "Meteors: the manual says one per level; the game at most 5. Sleep: the manual says "
                "level 5 or more is safe; the game spares T >= 4. Scare: the manual says level 6 or "
                "more is safe and it is an area spell; the game affects T <= 6, one target. Stone "
                "Skin: the manual says 1 to 4 attacks; the game 1d4 + L/2. Haste: the manual says "
                "faster attacks; 1.3 makes them slower (ASCENT corrects it). Fly: the manual says it "
                "lingers a random time; the game has a random duration and no extra time. Rary's "
                "Mnemonic Enhancer: the manual rule is in ASCENT as a correction.",
            ]
        return [
            "- Prayer: as read, only the caster is blessed (see the row). The manual adds penalties "
            "to enemies and bonuses to damage and saves; the game has only +1 to hit.",
            "- Resist Cold is Cure Blindness or Deafness in the code. The 1.3 readme lists Cure "
            "Blindness or Deafness as a new spell.",
            "- Flame Walk halves fire damage; the manual says it negates normal fire.",
            "- Magical Stone: the manual says three stones; the game 1d3.",
            "- Magical Vestment: the manual says the AC of chain mail (5); the game gives 5 up to level 7 and "
            "better at higher levels.",
            "- The manual lists Silence, 15' Radius (level 2). There is no such spell.",
            "- The manual says a priest gets every spell of a new spell level. No such code was found: "
            "a new cleric has Abjure, and spell items add spells.",
            "- The 1.3 readme names a new spell Produce Fire. No line or code with that name was found.",
        ]


WEAPON_INTERNAL = {
    0: "the empty hand (1d2 plus strength, from the character record, not the line)",
    1: "Flare", 2: "template of a spell cast with the hands", 3: "template of a wand cast",
    4: "template of a scroll cast", 5: "template of a spell arrow", 10: "named DO NOT USE",
    13: "Wand of Lightning Bolts: a missile line without pc_spell, 5d6 (see Magic Items, notes)",
    14: "Black Lightning (creature)", 15: "Bomb (trap)", 16: "Lava Flames (trap)", 17: "Mini Flames (trap)",
    25: "Slow Bomb (trap)", 26: "Fast Bomb (trap)", 63: "Wand of Genericness", 64: "Wand of Genericness",
    65: "Wand of Genericness", 66: "Rod of Genericness", 67: "Rod of Genericness", 68: "Rod of Genericness",
    76: "Temp", 77: "Wand (creature)", 78: "Acid Spittle (creature)", 80: "Myconid Spores (creature)",
    81: "Beholder Burn (creature)", 82: "Spectator Burn (creature)", 83: "Spectator Burn (creature)",
    84: "Spectator Burn (creature)", 85: "Spectator Burn (creature)", 86: "Spectator Burn (creature)",
    87: "Spectator Burn (creature)", 88: "Spectator Burn (creature)",
}

# 0x2AB9F, 0x2B1F2, 0xB337E, 0xAFB55, 0xB038F (combat.md B6a, audit/combat.md C5)
WEAPON_NOTES = {
    8: "Flame Arrow: homing, mage flag; levels_per_use=5 (no reader found)",
    45: "backstab with a hit roll of 19 or 20: +3 to 6 damage",
    49: "uses=10 in the line",
    54: "in the hand: Chill Touch, Resist Fire, Feather Fall, Free Action, Jump, Detect Magic, Haste, "
        "Improved Invisibility and (human or half-elf) Infravision with no end, Find/Remove Traps 95; "
        "ac_adjustment, adjust_strength, adjust_dexterity of the line are not read",
    69: "pc_spell=21 (Haste): the attack casts Haste, see Wands",
    70: "against undead: double damage when d100 <= 5 x (T - 4), else the undead is destroyed",
    71: "+2 to hit and +2 fire against a regenerating target, +4 to hit and +4 fire against undead",
    75: "no magic flag",
}

# 0x37A6A, combat.md B6; port dtuchar.c use_misc()
POTIONS = {
    72: ("Dexterity 18 for 600 s, if it is below 18.", "0x37A6A case 0x48"),
    73: ("If any effect or potion runs: all spell effects and potion effects end.", "case 0x49"),
    74: ("Strength 18 for 1 to (first class level) minutes, if below 18; a fighter also gets a random "
         "percentile 0 to 99.", "case 0x4A"),
    75: ("+6 to 27 hit points (6 + rand % 22), up to the maximum.", "case 0x4B"),
    76: ("Resist Fire for 600 s: fire damage halved.", "case 0x4C"),
    77: ("The spell Fly on you (10 x (1d6 + L) minutes).", "case 0x4D"),
    78: ("Free Action for 50 to 80 minutes ((rand % 4 + 5) x 600 s).", "case 0x4E"),
    79: ("+4 to 10 hit points (4 + rand % 7), up to the maximum.", "case 0x4F"),
    80: ("Strength 19 for 50 to 80 minutes, if below 19.", "case 0x50"),
    81: ("Intelligence 5 to 7 for 50 to 80 minutes. While it runs, parts of the map are forgotten "
         "now and then (1 in 63 for each frame in 1.3; ASCENT: Dtu_fix_potion_frame).", "case 0x51"),
    82: ("Infravision for 10 to 80 minutes ((rand % 8 + 1) x 600 s).", "case 0x52"),
    83: ("Invisibility for 30 to 60 minutes. The line repeats the name Potion of Agility; this is the "
         "Potion of Invisibility of the 1.3 readme.", "case 0x53"),
    84: ("Invulnerability for 5 to 20 minutes: no spell damage, armor class -2, and creatures of level "
         "below 4 or without a magic weapon cannot hurt you.", "case 0x54, 0x2A24A"),
    85: ("The spell Jump on you ((1d3 + L) minutes).", "case 0x55"),
    86: ("For one hour: the next attack roll on an odd value of the day clock is a natural 20, and "
         "the damage roll can be the highest; then the luck ends (one critical hit). ASCENT "
         "(Dtu_fix_luck_coin) uses a coin flip in place of the clock bit.", "case 0x56, 0x2A87B"),
    87: ("Haste for 5 to 20 minutes.", "case 0x57"),
    88: ("Hit points + 2 to 18 and Bless for 70 to 120 minutes; the extra hit points go when Bless "
         "ends. If you already have more hit points than the maximum: \"You don't feel so good.\" and "
         "a system shock roll: d100 above the table value for your Constitution kills you.",
         "case 0x58, table 0x2FACC"),
}

# 0x37A6A cases 0x66, 0x6A, 0x7F
SCROLLS = {
    102: "USE: shows a text screen (0x9DC42, the number of the item selects the text; not decoded). "
         "Not used up. Every new character gets one. Not in ASCENT (the use is logged only).",
    106: "USE: the screen shakes; each of these with a 50% chance: Confusion, Continual Light, Scare, "
         "Hold Person, Fireball on you, the map forgotten, and each ability score set to a random value "
         "from 1 to its value; then damage of rand % (your hit points). Quest item.",
    126: "Quest item. No use code found for this number.",
    127: "USE: shows text screen 1. Quest item.",
}


# ---------------------------------------------------------------------
# Table check

RULE_KEYS = {
    "$WEAPON": ["name", "weapon_name", "damage_dice", "#damage_dice", "damage_bonus", "to_hit_bonus",
                "fire_damage_dice", "#fire_damage_dice", "pc_spell", "uses", "fire_wait", "attack_type", "value"],
    "$ARMOR": ["name", "armor_class_decrement", "value"],
    "$OBJECT": ["name", "value", "to_hit_bonus"],
    "$SPELL": ["name", "level", "cast_time", "range", "radius", "duration", "level_duration", "type",
               "damage_dice", "#damage_dice", "fire_damage_dice", "#fire_damage_dice", "fire_damage_level",
               "acid_damage_level", "elec_damage_level"],
}
RULE_FLAGS = ["all_use", "fighter_use", "thief_use", "mage_use", "cleric_use", "magic", "magic_ok",
              "arrow", "bolt", "self_cast_ok", "cast_in_front", "homing", "bow", "crossbow"]


def check_tables(ref, directory):
    for path in sorted(glob.glob(os.path.join(directory, "*.BIN"))):
        try:
            t = Table(path)
        except Exception as e:     # a file that is not a table
            print("%s: %s" % (os.path.basename(path), e))
            continue
        diffs = []
        for sec, keys in RULE_KEYS.items():
            if not t[sec]:
                continue
            if len(t[sec]) != len(ref[sec]):
                diffs.append("%s: %d lines, YPINN.BIN %d" % (sec, len(t[sec]), len(ref[sec])))
                continue
            for i, (a, b) in enumerate(zip(t[sec], ref[sec])):
                for k in keys:
                    if a.get(k) != b.get(k):
                        diffs.append("%s %d: %s=%s, YPINN.BIN %s" % (sec, i, k, a.get(k), b.get(k)))
                for f in RULE_FLAGS:
                    if a.has(f) != b.has(f):
                        diffs.append("%s %d: flag %s" % (sec, i, f))
        n = sum(len(t[s]) for s in RULE_KEYS)
        if n == 0:
            continue        # a table of a player character
        print("%-13s %4d item and spell lines, %d differences%s" % (
            os.path.basename(path), n, len(diffs), "" if not diffs else ": " + "; ".join(diffs[:4])))


# ---------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="Write the wiki pages of the game rules")
    ap.add_argument("game_dir", help="the installed game (YPINN.BIN, DTUGAME.EXE 1.3)")
    ap.add_argument("out_dir", help="where the pages go")
    ap.add_argument("--tables", help="directory of unpacked level tables to compare with YPINN.BIN")
    a = ap.parse_args(argv)

    tab = Table(os.path.join(a.game_dir, "YPINN.BIN"))
    exe = Exe(os.path.join(a.game_dir, "DTUGAME.EXE"))
    ct = read_code_tables(exe)
    for sec, n in (("$WEAPON", 89), ("$ARMOR", 53), ("$OBJECT", 128), ("$SPELL", 56)):
        if len(tab[sec]) != n:
            raise SystemExit("YPINN.BIN: %d %s lines, expected %d" % (len(tab[sec]), sec, n))
    # The armor class table agrees with armor_class_decrement where the line has it
    for i, r in enumerate(tab["$ARMOR"]):
        dec = r.num("armor_class_decrement", None)
        if dec is not None and -dec != ct["ac_armor"][i]:
            print("note: armor %d %s: line %s, executable %d" % (i, r.name(), dec, ct["ac_armor"][i]))

    p = Pages(tab, ct)
    pages = {
        "Home.md": p.home(), "_Sidebar.md": p.sidebar(), "Races.md": p.races(), "Classes.md": p.classes(),
        "Weapons.md": p.weapons(), "Armor.md": p.armor(), "Magic-Items.md": p.magic_items(),
        "Potions.md": p.potions(), "Wands.md": p.wands(), "Scrolls.md": p.scrolls(),
        "Wizard-Spells.md": p.spell_page("m"), "Priest-Spells.md": p.spell_page("c"),
    }
    os.makedirs(a.out_dir, exist_ok=True)
    for name, text in pages.items():
        if "—" in text:
            raise SystemExit("%s: em dash in the text" % name)
        with open(os.path.join(a.out_dir, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("%-18s %3d table rows" % (name, p.counts.get(name[:-3], 0)))
    if a.tables:
        check_tables(tab, a.tables)
    return 0


if __name__ == "__main__":
    sys.exit(main())
