#!/usr/bin/env python3
"""Write the GitHub wiki pages of the game rules of Descent to Undermountain
1.3: races, classes, weapons, armor, magic items, potions, wands, scrolls,
wizard spells, priest spells, and a page of sources.

The numbers come from two places:

  - the item and spell lines of the level table YPINN.BIN (the same rule
    values are in every table of the mission levels; --tables checks that),
  - tables of DTUGAME.EXE 1.3, read by their linear address (object 1 at
    0x10000, docs/formats/exe.md).

What exists only as code (a formula, a branch on an item number) is in the
marked tables CODE RULES below. Each entry names the address of the code in
DTUGAME.EXE 1.3. The reverse engineering notes are docs/re/combat.md
(part B), spells.md, character.md, scripts.md and frontend.md.

The player pages are written in game terms. Addresses, table keywords and
the evidence of each rule go to Sources.md only.

The pages of the content of the port (New-Gear, Encounters, Monster-Behavior
and the new spells of the spell pages) read the tables of the port code with
tools/gear_tables.py and tools/port_tables.py; their rules in words are in
the marked tables below (PORT_SPELLS, CLASS_GEAR, CASTER_KINDS and others).

The item corrections of ASCENT come from engine/main/dtuitemfix.c (read by
tools/check_itemfix.py): an item has its corrected name with the name of 1.3
in its notes, and each value that the setting "Rulebook items" changes is
marked in the notes of the item and listed on the Magic items page.

usage: python -I tools/wiki_rules.py GAME_DIR OUT_DIR [--tables DIR] [--no-icons]

GAME_DIR  the installed game: YPINN.BIN and DTUGAME.EXE of version 1.3;
          FEBGND.JRG, FEBGND.DAT and PALETTE.256 for the icons.
OUT_DIR   the pages are written here (Home.md, _Sidebar.md, ...), and the
          icons of the items and spells to OUT_DIR/icons/icon-N.png.
--tables  a directory of unpacked level tables (*.BIN, gamedata/unpacked):
          the item and spell lines of each table are compared with
          YPINN.BIN; the result goes to Sources.md and to the console.
--no-icons  write the pages without the icon column and no icon files.

This tool reads the game files only. The pages hold numbers and names of the
game, and the icons are pictures of the game; do not commit them. The tool
refuses to write the icons into a path of the ASCENT repository that git
does not ignore.
"""
import argparse
import collections
import glob
import html
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.dont_write_bytecode = True  # keep __pycache__ out of the repository
import egg  # noqa: E402
import jrg  # noqa: E402  (FEBGND.DAT, the icon sheet)
import check_itemfix  # noqa: E402  (the tables of engine/main/dtuitemfix.c)
import tblfix  # noqa: E402  (the table fixup of 1.3, engine/main/dtutblfix.c)
import gear_tables  # noqa: E402  (the new gear of ASCENT-DTU, engine/main/dtugear.c)
import port_tables  # noqa: E402  (the spells, icons, casters and dragon of the port)


# ---------------------------------------------------------------------
# Readers

class Table:
    """The record lines of a .BIN table, by section word ($WEAPON ...).

    fixup: the lines get the table fixup of 1.3 (0x1A344, tools/tblfix.py,
    engine/main/dtutblfix.c), as the game shows them. self.fixed is the set
    of changed lines, or None (another table, or fixup=False)."""

    def __init__(self, path, fixup=True):
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
        self.fixed = tblfix.apply(self) if fixup else None

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

    shown = None        # the corrected name (dtuitemfix.c), set by Pages
    fix_notes = ()      # the notes of the corrections of the item

    def name13(self):
        return (self.get("name") or self.get("weapon_name") or "?").strip()

    def name(self):
        return self.shown or self.name13()

    def note(self):
        return " ".join(self.fix_notes)


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
# Address, element type (struct format), count, what it is (for Sources.md).
# Tables of pointers are not read (their file values have no fixups); the
# rows they point to are.

EXE_TABLES = {
    # Character generation (fenuchar.c), docs/re/frontend.md section 5
    "race_combos": (0x13FECE, "i", 6, "allowed classes by race (bit masks, tested at 0x88B25)"),
    "race_mod": (0x13FCFF, "i", 36, "racial ability adjustments, 6 rows of STR DEX CON INT WIS CHA (pointers at 0x13FD8F)"),
    "race_limits": (0x13FB03, "i", 72, "racial ability limits, 6 rows of 6 minimums and 6 maximums (pointers at 0x13FCE3)"),
    # Ability tables
    "str_hit": (0x13A956, "i", 20, "strength to-hit bonus (read by 0x2A789)"),
    "str_dmg": (0x13A9A2, "i", 20, "strength damage bonus (0x2B494)"),
    "str18_hit": (0x13A9F2, "i", 5, "exceptional strength to-hit, 18/01-50 to 18/00 (0x2A7B9)"),
    "str18_dmg": (0x13AA06, "i", 5, "exceptional strength damage"),
    "dex_ac": (0x14019A, "i", 20, "dexterity armor class adjustment (0xAFB55)"),
    "con_hp": (0x1402E2, "i", 20, "constitution hit points per level, not a fighter"),
    "con_hp_f": (0x140332, "i", 20, "constitution hit points per level, fighter"),
    "int_spells": (0x14023E, "i", 20, "intelligence column of the statistics screen (no rule reads it)"),
    "wis_bonus": (0x1403D2, "i", 80, "cleric bonus slots by wisdom, 4 rows for spell levels 1 to 4 (0x3575B)"),
    "cha_sell": (0x13F2DD, "i", 20, "market: percent of the value that the player gets (0x86F75)"),
    "cha_buy": (0x13F32D, "i", 20, "market: percent of the value that the player pays"),
    # Thief skills of a new character (0x8BDE5); race index 0 human, 1 dwarf,
    # 2 elf, 3 drow, 4 halfling, 5 half-elf
    "thief_race": (0x1461C8, "i", 30, "thief skills by race, 5 rows (0x8BDE5)"),
    "thief_dex": (0x146078, "i", 84, "thief skills by dexterity, 4 rows of 21"),
    "secret_d6": (0x1466E8, "i", 6, "racial search for secret doors, d6 (0xB3ECE)"),
    # Experience, THAC0, hit points (0xAF3CF, 0xAF605, 0xAF694), class order
    # cleric, mage, fighter, thief
    "xp_c": (0x145FA0, "i", 21, "experience, cleric (0xAF43A)"),
    "xp_m": (0x145F4C, "i", 21, "experience, mage"),
    "xp_f": (0x145EF8, "i", 21, "experience, fighter"),
    "xp_t": (0x145FF4, "i", 21, "experience, thief"),
    "thac0_c": (0x146548, "B", 18, "THAC0, cleric (read at level + 1 by 0xAF605)"),
    "thac0_m": (0x14657E, "B", 18, "THAC0, mage"),
    "thac0_f": (0x14656C, "B", 18, "THAC0, fighter"),
    "thac0_t": (0x14655A, "B", 18, "THAC0, thief"),
    "hit_die": (0x146518, "i", 4, "hit die by class (0xAF694)"),
    "hd_dice_c": (0x146298, "i", 20, "hit dice by level, cleric"),
    "hd_dice_m": (0x1462E8, "i", 20, "hit dice by level, mage"),
    "hd_dice_f": (0x146338, "i", 20, "hit dice by level, fighter"),
    "hd_dice_t": (0x146388, "i", 20, "hit dice by level, thief"),
    "hd_fixed_c": (0x1463D8, "i", 20, "fixed hit points by level, cleric"),
    "hd_fixed_m": (0x146428, "i", 20, "fixed hit points by level, mage"),
    "hd_fixed_f": (0x146478, "i", 20, "fixed hit points by level, fighter"),
    "hd_fixed_t": (0x1464C8, "i", 20, "fixed hit points by level, thief"),
    # Saving throws 0xAE596: rows of 5 bytes, divisors by class
    "save_c": (0x145642, "B", 20, "saving throws, cleric (0xAE596)"),
    "save_t": (0x145656, "B", 15, "saving throws, thief"),
    "save_f": (0x145665, "B", 30, "saving throws, fighter"),
    "save_m": (0x145683, "B", 15, "saving throws, mage"),
    "save_div": (0x1456E2, "B", 4, "levels per row of the saving throw tables"),
    # Spell slots: 18 rows by class level (0x3575B)
    "slots_m": (0x145CD8, "B", 162, "mage spell slots, 18 rows of 9 (0x3575B)"),
    "slots_c": (0x145DF8, "B", 126, "cleric spell slots, 18 rows of 7"),
    # Armor class of the items by number (0xAFBDA)
    "ac_armor": (0x145C04, "i", 53, "armor class of the armor lines by number (0xAFBDA)"),
    "ac_misc": (0x145A04, "i", 128, "armor class of the misc lines by number"),
    "ac_weapon": (0x1458B8, "i", 83, "armor class of the weapon lines by number"),
    "glove_dex": (0x146240, "i", 20, "Gloves of Dexterity, by dexterity"),
    "shock": (0x2FACC, "B", 19, "Potion of Toughening: system shock by constitution 1 to 19"),
    "spider_spells": (0x2FA94, "i", 14, "spells of the Spider Piece (misc 20)"),
    "cast_time": (0x13A8DB, "h", 56, "cast time of each spell in game seconds; the table fixup 0x1A344 "
                  "writes it over cast_time= of the $SPELL lines"),
    "turn": (0xA6487, "b", 126, "turn undead: 9 rows of 14 (0xADC46)"),
}


def read_code_tables(exe):
    return {k: exe.ints(a, n, f) for k, (a, f, n, _) in EXE_TABLES.items()}


# ---------------------------------------------------------------------
# CODE RULES. Values that exist only as code. Sources: DTUGAME.EXE 1.3
# addresses (docs/re/combat.md part B unless named). L is the caster level,
# T the level of the target.
#
# Units: two game units are one foot (the fall message divides by two,
# fell >> 17, docs/re/physics.md). Game seconds: the game messages call 60
# of them a minute and 3600 an hour.
#
# Missiles of a spell (0x56FF6): speed 100 units/s (no $SPELL line has
# speed=, the default of the reader 0x242F6 is 100.0), life 30 s
# (Weapon_info+0xA8 of the template, written by 0x56FF6), Lightning Bolt
# 2 s, Melf's Acid Arrows (L/3 + 1) minutes. The spell `range` is not read.

MISSILE_FT_PER_S = 50
MISSILE_LIFE_S = 30

# 0-sided dice (Fireball, Lightning Bolt, Melf's Acid Arrows). 0x91BC3 rolls
# each die as rand() % sides + 1 with a plain idiv (0x91C25, 0x91C8E, 0x91CEC,
# 0x91D4E). A divide by 0 goes to the divide error handler of div0.asm
# (installed by div0_init(1) at 0xB8323, handler 0xF6D94, mode 1 =
# DM_SATURATE, no address lists). The saturate path decodes the
# instruction `idiv dword ptr [esp]` (F7 3C 24) wrongly: it reads the SIB
# byte as a ModRM byte and steps over the next instruction, the `inc edx`.
# The result: EDX (the remainder) = 0 and the `inc edx` is skipped, so each
# 0-sided die adds 0. Emulated with unicorn from 0xF6E18 for all four idiv
# addresses: resume at the `add` after `inc edx`, EDX 0. The int3 at
# 0xF6E17 before the saturate path is a breakpoint for a debugger and
# returns at once without one (DPMI reflects INT 3 to the real mode vector).

# Spell rules for the player pages. Fields:
#   src     addresses of the code (Sources.md)
#   dur     the duration in words
#   save    the saving throw in words
#   target  who it acts on; {r} is the radius of the line in feet
#   eff     what it does, in game terms
#   tech    detail for Sources.md (optional)
SPELLS = {
    0: dict(src="0x8D297", dur="Instant", save="See effect", target="One extraplanar creature",
            eff="Works only on an extraplanar creature. It works on a d20 roll of 11 - your level + "
                "the creature's level or more (1 to 20). Then the creature dies on d100 >= 70 + 2 x "
                "its level; otherwise it leaves the level, and you get no experience for it."),
    1: dict(src="0x8D3F7", dur="1 minute + 1 per level", save="None", target="You",
            eff="+1d8 hit points, even above your maximum, and Bless (+1 to hit) for the duration. "
                "When it ends, hit points above your maximum are lost."),
    2: dict(src="0x8D4E7, 0xB05D9", dur="Until used up", save="None", target="You",
            eff="Your armor class becomes at most 6, before the Dexterity adjustment. The spell ends "
                "when weapon hits on you add up to 8 damage + 1 per level. You still take that "
                "damage. Spell damage does not count."),
    3: dict(src="0x8D58E, 0x2AD27", dur="6 minutes", save="None", target="You", eff="+1 to hit."),
    4: dict(src="0x8D614, 0x911AA", dur="Instant", save="Half",
            target="6 missiles; each hits everyone within {r} ft of where it lands",
            eff="1d3 fire damage + 2 per level to each creature or player in the area, except you."),
    5: dict(src="0x8D62F, 0x29E3D", dur="3 minutes + 1 per level", save="Negates (each hit)",
            target="You",
            eff="Each of your melee hits forces a saving throw. If the target fails, it takes +1d4 "
                "damage; an undead target is frightened (Fear) for 1d4 minutes + 1 per level instead. "
                "The Sword of W'hin gives the same effect.",
            tech="The case acts only when the target is the caster (0x8D633). The table line is not "
                 "self-cast; the table fixup (0x1A65E) sets the cast on the caster. The cold dice of "
                 "the line are never rolled."),
    6: dict(src="0x8D6D4", dur="See effect", save="Only a target above your level, or of level 6 or more",
            target="1d6 missiles; each hits everyone within {r} ft of where it lands",
            eff="Not on blind creatures. A target of your level or lower falls asleep for 2 to 8 "
                "minutes; a player is also blind for that time. Up to 2 levels above yours: blind for "
                "1d4 minutes. Higher: stunned for 1 minute, unless it has Free Action."),
    7: dict(src="0x8DB82", dur="2 minutes + 1 per level", save="Negates",
            target="1d4 missiles + 1 per level; each affects the creature it hits",
            eff="A creature stops being hostile. A player loses control."),
    8: dict(src="0x8DC1D, 0x90CEF", dur="No end", save="Negates", target="Everyone within {r} ft of you",
            eff="A bright light appears where the missile lands and stays. Each creature or player in "
                "the area, except you and the blind, is blind with no end unless it saves. "
                "Cure Blindness or Deafness ends the blindness.",
            tech="Light 60.0 (player_light of the table fixup). A light-sensitive creature that fails "
                 "would also take the damage of the line, which is 0."),
    10: dict(src="0x8DE4B", dur="Instant", save="None", target="You",
             eff="+1d8 hit points, up to your maximum. No effect on undead or extraplanar creatures. "
                 "Extra hit points of Aid stay (the original 1.3 game takes them away)."),
    11: dict(src="0x8DF37", dur="Instant", save="None", target="You",
             eff="+3 to 17 hit points, up to your maximum. No effect on undead or extraplanar creatures. "
                 "Extra hit points of Aid stay (the original 1.3 game takes them away)."),
    12: dict(src="0x8E019", dur="2 minutes per level", save="None", target="You",
             eff="DETECT_MAGIC",
             tech="Sets effect bit 6. The renderer reads it (0x670DB, 0x75726)."),
    13: dict(src="0x8E0B5, 0x90CEF", dur="Instant", save="None", target="Everything within {r} ft of where the missile lands",
             eff="Each creature in the area loses all its spell effects, with no save. It also acts "
                 "on things: a spell missile is destroyed unless it saves (d20 >= 10), a potion on the "
                 "floor breaks on d20 >= 23 - your level, magic scenery breaks unless it saves, and a "
                 "spell light goes out unless it saves. DISPEL_PEACE",
             tech="The table fixup (0x1A8ED) writes flags 0x803: AREA, and +0x37 bit 3, with which "
                  "0x90CEF also takes objects that are not creatures. The table line had "
                  "cast_in_front (a cast on the caster); the fixup removes it."),
    14: dict(src="0x8E2D8, 0x13407, 0x903D4", dur="1 minute per level", save="Negates", target="The creature the missile hits",
             eff="Not on undead or creatures that cannot be scared. The target flees. A creature drops "
                 "its weapon on d100 <= 65 - 5 x its level. A player is held and may drop the weapon; "
                 "on a player the time is divided by half the Wisdom. A frightened player also loses "
                 "the view: it follows a creature that runs off, and when Fear ends the player stands "
                 "where that creature went."),
    15: dict(src="0x8E464, 0x9DEDD", dur="1 minute per level", save="None", target="You",
             eff="You fall slowly, at most 7.5 ft per second. No message when it ends."),
    16: dict(src="0x8E55C, 0x91BC3", dur="Instant", save="Half", target="Everything within {r} ft of where the missile lands",
             eff="1d6 fire damage per level, at most 10d6. It also burns you when you are in the area. "
                 "Scenery in the area can break.",
             tech="The table fixup (0x1A972) writes fire_damage_dice=6, #1, level 0 and flags 0x803 "
                  "(AREA, objects too); the case sets #fire_damage_dice = min(L, 10). The table text "
                  "has 0 sides and 6 per level (6L, one target)."),
    17: dict(src="0x8E5B1, 0x91BC3", dur="Instant", save="Half", target="Everything within {r} ft of where the missile lands",
             eff="1d4 + 1 per level fire damage. Scenery in the area can break.",
             tech="The table fixup (0x1AD67) makes line 17 (Flaming Sphere, mage) the cleric spell "
                  "Produce Fire of level 4: fire 1d4 + 1 per level, the flags of Fireball (AREA, "
                  "objects too), radius 24, the sounds of Fireball, the value of Abjure."),
    18: dict(src="0x8E5D1", dur="1 minute + 1 per level", save="None", target="You",
             eff="Fire damage to you is halved, as with Resist Fire."),
    19: dict(src="0x8E665", dur="10 to 60 minutes + 10 per level", save="None", target="You",
             eff="You fly."),
    20: dict(src="0x8E6F2", dur="10 minutes per level", save="None", target="You",
             eff="Ends Hold. Hold Person and the stun of Color Spray do not work on you, and nothing "
                 "slows your movement."),
    21: dict(src="0x8E79C, 0x9DE13, 0x41EC3", dur="3 minutes + 1 per level", save="None", target="You",
             eff="Ends Slow. You move twice as fast, a melee swing takes half the time (0.5 s for most "
                 "weapons), and arrows, bolts and thrown weapons wait half as long between shots. The "
                 "original 1.3 game makes the melee attacks slower (one each 1.5 swing times) and does "
                 "not change the shots; ASCENT corrects this. The Short Sword of Quickness and the Short Bow "
                 "of Speed do not make it faster again."),
    22: dict(src="0x8E85A, 0x56FF6", dur="2 minutes per level", save="Negates", target="The creature the missile hits",
             eff="Only humanoid creatures and players; not undead, not with Free Action. The target "
                 "cannot move. Each cast holds at most 1d4 targets; a target that saves does not count. "
                 "HELD_HIT On you the hold lasts 6 to 18 seconds (with Caster AI a hold that a creature "
                 "casts lasts CASTER_HOLD_MAX seconds at most, and no creature holds you again until "
                 "CASTER_HOLD_REST seconds after a hold ends, [[Monster-Behavior]]), and you "
                 "cannot move, turn, look up or down, attack, cast or use an item, except a Potion of "
                 "Freedom, which ends it. The original 1.3 game holds you for 2 minutes per level "
                 "divided by half your Wisdom and lets you attack and cast; ASCENT corrects this.",
             tech="The counter Spell_info+0x49 is 1d4 at each cast (0x56FF6); the case needs it above 0 "
                  "and takes 1 for each hold (0x8E8C5)."),
    23: dict(src="0x8EA21", dur="1d4 minutes + 1 per level", save="Only a target above level 2",
             target="The creature the missile hits",
             eff="Only undead of your level or lower. The target cannot move."),
    24: dict(src="0x8EB53", dur="4 minutes + 1 per level", save="None", target="You",
             eff="Creatures do not see you, even while you attack. Open Locks -15% while invisible."),
    25: dict(src="0x8EBE8", dur="2 hours + 1 per level, at most 4 hours 33 minutes", save="None", target="You",
             eff="Lets you turn on infravision (key N). It is of use only to a human: every other "
                 "race has infravision already. From caster level 3 on the time is 4 hours 33 minutes, "
                 "the longest spell time of ASCENT. In the original 1.3 game the spell ends at once "
                 "from caster level 8 on; ASCENT corrects this.",
             tech="(L + 2) x 3600 s in 16.16 fixed point wraps from L = 8 (Dtu_fix_spell_hours). The "
                  "case also gives the effect to a half-elf (race byte 0), who has infravision by "
                  "race."),
    26: dict(src="0x8ECB4", dur="Until you attack", save="None", target="You",
             eff="Creatures do not see you. Ends when you make a weapon attack or cast a spell missile. "
                 "No effect while you have Improved Invisibility. Open Locks -15% while invisible."),
    27: dict(src="0x8ED20, 0x9EA62", dur="1d3 minutes + 1 per level", save="None", target="You",
             eff="You jump higher.",
             tech="Jump strength 2.0 instead of 1.5 (0x9EA62)."),
    28: dict(src="0x8FFE5, 0x26C2F", dur="Instant", save="None", target="The door the missile hits",
             eff="The missile opens the door it hits, also a locked one, unless the lock is magic.",
             tech="The case of 0x8CFFF has no code; the door code is in the wall hit 0x26C2F "
                  "(0x7E859). The table line has self_cast_ok and cast_in_front (a cast on the "
                  "caster, no missile); the table fixup (0x1A73F) writes flags 2 and removes them."),
    29: dict(src="0x8EDB2, 0x9DEAE", dur="10 minutes per level", save="None", target="You",
             eff="You float and move only up and down. You cannot cast spells while you levitate."),
    30: dict(src="0x8DC1D, 0x90CEF", dur="10 minutes per level", save="Negates", target="Everyone within {r} ft of you",
             eff="A light appears where the missile lands for the duration. Each creature or player "
                 "in the area, except you and the blind, is blind for the duration unless it saves. A "
                 "light-sensitive creature that fails also takes 1d6 damage.",
             tech="Light 20.0 (player_light of the table fixup)."),
    32: dict(src="0x8EE76, 0x91BC3", dur="Instant", save="Half", target="Each creature on its line",
             eff="1d6 electric damage per level, at most 10d6, to each creature on its line, each "
                 "with its own saving throw for half. No damage to creatures immune to electricity. "
                 "Scenery that it hits can break. The bolt goes on through the creatures until its "
                 "range ends and comes back from a wall toward you, so it can hit you. A creature "
                 "that it crosses again saves again: full damage if one save fails, half if all are "
                 "made. The original 1.3 game ends the bolt at the first creature it hits; ASCENT "
                 "follows AD&D 2nd edition.",
             tech="The table fixup (0x1AABA) writes elec_damage_dice=6, #1, level 0 and flags "
                  "0x802; the case sets #elec_damage_dice = min(L, 10): 1d6 per level, at most "
                  "10d6. The missile lives 2 s in 1.3 and bounces at a wall (0x571C5, 0xA0731); "
                  "0x272DD removes it at the first body. Dtu_fix_bolt_pass (dtuobj_missile.c) "
                  "lets it go on."),
    33: dict(src="0x8EED5, 0x56FF6", dur="Instant", save="Half",
             target="1 missile, +1 at levels 2, 4, 6 and 8 (at most 5)",
             eff="1d4 + 1 damage per missile. The missiles home in on the target when a mage casts "
                 "them. No damage to a target protected by Shield.",
             tech="The table fixup (0x1AABA) writes damage_level=1; the dice are rolled at level 1. "
                  "damage_bonus=1 of the line is not added (0x91BC3 does not read it)."),
    34: dict(src="0x8EEFE", dur="Instant", save="Half", target="1d3 stones; each hits one creature",
             eff="1d4 damage per stone, 2d4 against undead."),
    35: dict(src="0x8EF49, 0xB0606", dur="5 minutes per level", save="None", target="You",
             eff="VESTMENT"),
    36: dict(src="0x8EFE0, 0x56FF6", dur="1 round + 1 per 3 levels", save="None", target="1 arrow",
             eff="One acid arrow. It hits on an attack roll with the THAC0 of a fighter of your level "
                 "against the armor class of the target (a natural 1 misses, a natural 20 hits). A hit "
                 "does 2d4 acid damage at once and 2d4 more at the start of each of the next rounds: 1 "
                 "round at levels 1-2, 2 rounds at levels 3-5, 3 at levels 6-8, and so on (a round is 1 "
                 "minute). The acid stops when the target dies. Acid stops regeneration. The original "
                 "1.3 game has no attack roll and does 4 acid damage per level in one hit, with a saving "
                 "throw for half; ASCENT corrects this.",
             tech="Dtu_fix_melf_acid (dtuspell.c melf_acid_hit, acid_frame). 1.3: the table fixup "
                  "(0x1AB90) writes acid 4 sides and level 0; the case sets #acid_damage_dice = "
                  "2 (L/3) + 2. The missile lives (L/3 + 1) minutes. The later rounds of acid "
                  "are not in the level state and end at a save and load."),
    37: dict(src="0x8F021", dur="Instant", save="Half", target="1 meteor per level (at most 5)",
             eff="1d4 fire damage per meteor. Scenery that it hits can break."),
    38: dict(src="0x8F078, 0x56D6A", dur="1 minute per level", save="None", target="You",
             eff="Bless (+1 to hit) on you only."),
    39: dict(src="0x8F116, 0x2AD36", dur="2 minutes per level", save="None", target="You",
             eff="Attacks of extraplanar creatures miss you; other creatures get -2 to hit you. Your "
                 "attack on an extraplanar creature ends the spell."),
    40: dict(src="0x8F1B2", dur="Until used up", save="None", target="You",
             eff="Absorbs 12 points of fire damage per level before you take any. A second cast "
                 "does nothing while points are left."),
    41: dict(src="0x8F21D", dur="See effect", save="None", target="You",
             eff="Choose three more level 1 slots, one level 1 and one level 2 slot, or one level 3 "
                 "slot. The next rest fills them. The original 1.3 game allows one cast only and keeps "
                 "the slots until Dispel Magic or a Potion of Anti-Magic; ASCENT corrects this to the "
                 "rule of the manual: each cast adds slots, and the rest after the next one ends them."),
    42: dict(src="0x8F2FA", dur="Instant", save="None", target="You",
             eff="Cures your blindness and deafness. It gives no cold resistance, despite its name. "
                 "Nothing happens if you are neither blind nor deaf."),
    43: dict(src="0x8F3CD", dur="1 minute per level", save="None", target="You",
             eff="Fire damage to you is halved."),
    44: dict(src="0x8F467", dur="1d4 minutes + 1 per level", save="Elves, half-elves, clerics and people only",
             target="The creature the missile hits",
             eff="Only targets of level 6 or lower; not undead, extraplanar creatures or creatures that "
                 "cannot be scared. The target flees and drops its weapon half of the time."),
    45: dict(src="0x8F6AE, 0xB0659", dur="5 minutes per level", save="None", target="You",
             eff="An invisible barrier in front of you: armor class 4 against melee weapons and 2 "
                 "against missiles (arrows, bolts, thrown weapons). No effect if your armor is "
                 "better. Magic Missile does no damage to you. The original 1.3 game gives armor "
                 "class 3 against both."),
    46: dict(src="0x8F766", dur="5 minutes per level", save="See effect", target="The creature the missile hits",
             eff="Only targets of level 3 or lower; not undead or creatures immune to sleep. Each cast "
                 "has a pool of 4 to 16 (an even number). A creature costs a quarter of its maximum hit "
                 "points, a player 2 per level; if the pool is too small, the target stays awake. An "
                 "elf resists 9 times in 10, a half-elf 3 times in 10, people 9 times in 10. A melee hit "
                 "on a sleeper always hits. Any damage wakes the sleeper (in the original 1.3 game only a "
                 "melee hit). While you sleep the view is black with \"zzz\", and you cannot move, look "
                 "around or act; in the original 1.3 game nothing shows the sleep."),
    47: dict(src="0x8FAD8", dur="3 minutes + 1 per level", save="Negates", target="The creature the missile hits",
             eff="Ends Haste. Armor class +4 (at most 10), -4 to hit, half move speed, each swing "
                 "takes twice as long, and arrows, bolts and thrown weapons wait twice as long between "
                 "shots."),
    48: dict(src="0x8FBEE, 0xB3038", dur="3 minutes + 1 per level", save="None", target="You",
             eff="Climb Walls counts as 100%."),
    49: dict(src="0x8FC7A, 0x2AE44", dur="1d4 hits + 1 per 2 levels", save="None", target="You",
             eff="Each weapon attack on you that hits does no damage and uses up one of the hits. The attack "
                 "roll comes first: a miss uses up nothing. The original 1.3 game uses up a hit for each "
                 "attack of a creature on you, also for a miss, and ends all your other spell effects "
                 "when you cast it; ASCENT corrects both."),
    50: dict(src="0x8FCFA", dur="1 hour per level, at most 4 hours 33 minutes", save="None", target="You",
             eff="Strength +1d8 for a fighter (above 18 each point adds 10 percent, up to 18/00); "
                 "+1d6 for a thief or cleric and +1d4 for a mage, at most 18. From caster level 5 on "
                 "the time is 4 hours 33 minutes, the longest spell time of ASCENT. In the original 1.3 "
                 "game the spell ends at once from caster level 10 on; ASCENT corrects this.",
             tech="The time L x 3600 s overflows the 16.16 fixed point timer from L = 10 on "
                  "(Dtu_fix_spell_hours)."),
    51: dict(src="0x8E4F6", dur="30 minutes", save="None", target="You",
             eff="Find/Remove Traps counts as 100 percent, and the search for traps runs all the time.",
             tech="The table line is a second Burning Hands; the table fixup (0x1AE9A) names it Find "
                  "Traps, a cleric spell of level 2 cast on the caster (flags 0x102). The case: "
                  "Find/Remove Traps counts as 100% and the search runs each frame. The creature "
                  "weapon Ice Shards (weapon 79) casts it."),
    53: dict(src="0x8F116", dur="3 minutes per level", save="None", target="You",
             eff="Attacks of extraplanar creatures miss you; other creatures get -2 to hit you. Your "
                 "attack on an extraplanar creature ends the spell."),
    54: dict(src="0x8E019", dur="10 minutes", save="None", target="You", eff="DETECT_MAGIC"),
    55: dict(src="0x8E0B5", dur="Instant", save="None", target="The target the missile hits",
             eff="The target loses all its spell effects, with no save. It also acts on things: a spell "
                 "missile is destroyed unless it saves (d20 >= 10), a potion on the floor breaks on "
                 "d20 >= 23 - your level, magic scenery breaks unless it saves, and a spell light goes "
                 "out unless it saves. DISPEL_PEACE"),
}
DISPEL_PEACE = ("It is no attack: a creature that is not hostile is not woken and stays at peace, and its "
                "friends are not alerted. The original 1.3 game makes it hostile; ASCENT corrects this.")
HELD_HIT = ("Each attack on a held target hits with no attack roll; the hold stays. The original 1.3 game "
            "rolls; ASCENT follows AD&D 2nd edition.")

# The spells of the port (engine/main/dtuspell.c, docs/re/spells.md section
# 15). The lines and the cast times come from the code (tools/port_tables.py);
# the rules in words are here. Lines 59 and 60 are creature spells: the page
# Monster-Behavior has them.
PORT_SPELLS = {
    56: dict(src="dtuspell.c: spell_amount (1d4 + L/3, at most 8), dtu_spell_mirror_takes; dtuobj_combat.c dtu_hit_test_vs",
             dur="3 minutes per level", save="None", target="You",
             eff="1d4 images of you, +1 for each 3 levels, at most 8. Each attack that would hit you, and "
                 "each spell missile with no attack roll aimed at you alone (Magic Missile and the like), "
                 "takes an image instead, with the chance images / (images + 1): the image is gone, and the "
                 "message box says how many are left. A miss takes no image. Area spells are not fooled: "
                 "Fireball, Burning Hands, Lightning Bolt, and the Flame Strike and Mind Blast of "
                 "creatures. Hold Person is an area spell in this game, so the images do not take it."),
    57: dict(src="dtuspell.c: dtu_spell_globe, globe_stops (in apply_spell, so each path of a spell)",
             dur="1 minute per level", save="None", target="You",
             eff="No spell of level 1 to 3 acts on you: Magic Missile, Hold Person, Fireball, Lightning "
                 "Bolt and the others stop at the globe. Your own Fireball does not burn you, and a spell "
                 "that you cast on yourself works. Spells of level 4 and up pass. The Mind Blast of a mind "
                 "flayer is no spell and is not stopped. Dispel Magic ends the globe."),
    58: dict(src="dtuspell.c: hammer_bonus (L/6, at most 3), dtu_spell_port_weapon_line; dtucombat.c hand_weapon",
             dur="1 minute per level", save="None", target="You",
             eff="A blue throwing hammer of force is the weapon in your hand while the spell lasts. Your "
                 "attack throws it: 1d4 + 1 damage at the rate of the Throwing Hammer, and +1 to hit and "
                 "damage for each 6 levels (at most +3). It never runs out and is a magic weapon, so it "
                 "hurts the creatures that only magic weapons hurt. The weapon that you held stays in your "
                 "inventory and is in your hand again when the spell ends. The spell ends when you take "
                 "another item or a spell into your hand, and at a fumble that would drop the weapon. The "
                 "hammer is no item: it cannot be dropped, sold or put into a hot key box. AD&D 2nd edition "
                 "has a hammer that the priest directs at a distance for 3 rounds + 1 per level; ASCENT "
                 "follows Baldur's Gate."),
}
PORT_SPELL_END = ("These spell effects end at a rest, at a level change and by Dispel Magic, and a saved game "
                  "does not keep them.")
CASTER_AI_SETTING = "Caster AI"
IMPROVED_AI_SETTING = "Improved AI"

# The caster kinds of the lists of engine/main/dtucaster.h in game terms (no
# place names). Each list of the code needs a line here.
CASTER_KINDS = {
    "CL_DROW_MAGE": "Drow mages",
    "CL_DROW_PRIEST": "Drow priestesses and priests, priest spirits",
    "CL_DROW_HIGH_PRIEST": "Drow high priestesses (the strongest priestesses)",
    "CL_LICH": "Liches",
    "CL_ARCHMAGE": "Archmages (the great wizards of the story)",
    "CL_MAGE": "Other mages (the kobold shaman, human mages, the Mage Spirit)",
    "CL_HUMAN_CLERIC": "Human clerics (then they fight hand to hand)",
    "CL_ILLITHID": "Mind flayers (they walk at you and touch, 2d4)",
    "CL_BEHOLDER": "Beholders (their eye rays as spells)",
    "CL_MAGE_PRIEST": "A drow who is both a mage and a priestess",
    "CL_AVATAR": "An avatar of a goddess (she fights hand to hand)",
}

# The hoard of the dragon of DROW2 in the level file of 1.3 (objects 50 to
# 60, docs/re/dragon.md section 1): (class, number, count)
# The gems of the original game that the drops of the new gear give (misc 4
# and 3 of the tables)
GEM_RUBY, GEM_DIAMOND = 4, 3

DRAGON_HOARD_13 = [(16, 3, 3), (15, 10, 1), (15, 8, 1), (15, 41, 1), (15, 6, 1), (5, 37, 1), (5, 39, 1),
                   (5, 44, 1), (5, 42, 1)]

# The lich of CLAN3: Lich3 of the CLAN3 table, 88 hit points (docs/re/lich.md
# section 1)
LICH_HIT_POINTS = 88

SPELLS[9] = dict(SPELLS[8])
SPELLS[31] = dict(SPELLS[30], dur="60 minutes + 10 per level")
SPELLS[52] = dict(SPELLS[22])

# Detect Magic: the tint of 0x670D4 (objects) and 0x7569F (walls), the blue
# palette ramp at the brightness of each texel (docs/re/spells.md).
DETECT_MAGIC = ("Magic things in view are drawn in shades of blue: spell scrolls, magic weapons and "
                "armor, potions, keys and quest items, magic scenery, spell missiles in flight, and "
                "false walls that you can walk through. Creatures and your own items do not change. "
                "The original 1.3 game shows the tint mostly on near items and walls, because of a "
                "drawing defect; ASCENT corrects this.")


def vestment_text():
    # Magical Vestment (0xB0606): armor class at most max(5 - (L - 5) / 3, 1),
    # C division (toward 0)
    ac = lambda L: max(5 - int((L - 5) / 3), 1)
    parts, start = [], 1
    for L in range(2, 20):
        if L == 19 or ac(L) != ac(start):
            end = L - 1
            lv = ("level %d" % start) if start == end else ("levels %d %s %d" % (
                start, "and" if end == start + 1 else "to", end))
            parts.append("%d at %s" % (ac(start), lv))
            start = L
    return ("Your armor class becomes at most " + ", ".join(parts)
            + ", before the Dexterity adjustment.")


# Item numbers that the code knows (0xAFB55, 0x37A6A, 0x2AB9F, 0xADC46).
# Effects of worn items: the item bits of 0xAFB55 (combat.md B6a).
WORN = {
    ("armor", 16): "For a mage (also of two classes): the spells Armor, Shield and Magical Vestment on you "
                   "give an armor class 1 better than their limit (Armor 5, Shield 3 against melee "
                   "and 1 against missiles). No effect for "
                   "other classes or without these spells.",
    ("armor", 17): "Resist Fire while worn: fire damage halved.",
    ("armor", 18): "Feather Fall while worn.",
    ("armor", 19): "Fly while worn.",
    ("armor", 20): "Free Action while worn.",
    ("armor", 21): "Constitution 18 while worn, also if yours is higher.",
    ("armor", 22): "Infravision while worn (key N). It is of use only to a human: every other race, the "
                   "half-elf too, has infravision already.",
    ("armor", 23): "Jump while worn.",
    ("armor", 24): "Detect Magic while worn.",
    ("armor", 25): "Haste while worn.",
    ("armor", 26): "Strength 18/00 while worn, also if yours is higher.",
    ("armor", 27): "Doubles your mage spell slots of "
                   "levels 1 to 3. The original 1.3 game doubles them again at each change of "
                   "equipment and halves them once when you take it off; ASCENT corrects this and "
                   "doubles them once.",
    ("armor", 33): "Shield while worn: armor class 4 against melee weapons and 2 against missiles "
                   "(3 against both in the original 1.3 game), no damage from Magic Missile. Its own "
                   "armor class value does not count.",
    ("armor", 34): "Fly while worn.",
    ("armor", 35): "Feather Fall while worn.",
    ("armor", 36): "Levitate while worn. You cannot cast spells while you levitate.",
    ("armor", 37): "Haste while worn.",
    ("armor", 38): "Strength 19 while worn.",
    ("armor", 43): "Hide In Shadows 95% while worn.",
    ("armor", 44): "Improved Invisibility while worn.",
    ("armor", 45): "Worn: key T turns undead as a cleric of level 7, for any class. USE: "
                   "Continual Light around you.",
    ("armor", 46): "Invisibility while worn. It ends when you attack and comes back at the next change "
                   "of equipment.",
    ("armor", 47): "Counts with any armor, shield or bracers. Only the highest magic protection bonus "
                   "counts: of the magic of the armor, a Ring or Amulet of Protection and this cloak. "
                   "The bonus also makes the saving throws better (the best protection item).",
    ("armor", 48): "Counts with any armor, shield or bracers. Only the highest magic protection bonus "
                   "counts: of the magic of the armor, a Ring or Amulet of Protection and this cloak. "
                   "The bonus also makes the saving throws better (the best protection item).",
    ("armor", 49): "Counts with any armor, shield or bracers. Only the highest magic protection bonus "
                   "counts: of the magic of the armor, a Ring or Amulet of Protection and this cloak. "
                   "The bonus also makes the saving throws better (the best protection item).",
    ("armor", 50): "Dexterity +4 (Dexterity 6 or less), +2 (7 to 13) or +1 (14 to 19). Open Locks +10% "
                   "for a thief; for any other class Open Locks becomes 37%.",
    ("armor", 51): "Detect Magic while worn. It gives no light.",
    ("armor", 52): "Strength 18/00 while worn.",
}

# Corrections of ASCENT that the pages name, with the option that turns
# each off (engine/main dtuchar.c, dtucombat.c, dtuspell.c).
FIXES = [
    ("Dtu_fix_multiclass_thac0", "-origchar", "Two classes take the better THAC0, not the worse."),
    ("Dtu_fix_level_hit_points", "-origchar", "The hit points of a new level are added to the hit points "
     "in use; 1.3 adds them to the maximum only (the hit points in use grow by 1/65536 of them)."),
    ("Dtu_fix_secret_door_names", "-origchar", "A door texture name that no table has makes no secret "
     "door; in 1.3 it matches texture 0, so some plain doors become secret doors (one more in THIEF2)."),
    ("Dtu_fix_one_hit", "-origcombat", "One hit roll for each swing; 1.3 rolls again in each frame of "
     "the hit (about 4 rolls at 60 frames per second)."),
    ("Dtu_fix_turn_undead", "-origcombat", "Turn undead keeps its d20 after a T result and does not "
     "spoil the next undead when the count is used up."),
    ("Dtu_fix_spell_hours", "-origcombat", "Strength and Infravision do not end at once at a high caster "
     "level (10 and 8 in 1.3); their time stops at 4 hours 33 minutes."),
    ("Dtu_fix_rest_effects", "-origcombat", "A rest ends your spell and potion effects. Armor, Stone "
     "Skin, Rary's Mnemonic Enhancer and the powers of worn items stay; in 1.3 a rest ends nothing."),
    ("Dtu_fix_cure_aid", "-origcombat", "Cure Light Wounds and Cure Serious Wounds keep the extra hit "
     "points of Aid; 1.3 cuts the hit points to the maximum."),
    ("Dtu_fix_haste_attack", "-origcombat", "Haste halves the swing time; 1.3 makes the melee attacks slower."),
    ("Dtu_fix_missile_rate", "-origcombat", "Arrows, bolts and thrown weapons wait their Speed from one "
     "shot to the next (Haste halves it, Slow doubles it); in 1.3 each key press fires."),
    ("Dtu_fix_speed_items", "-origcombat", "The Short Bow of Speed in the bow slot shoots and the Short "
     "Sword of Quickness in the hand swings twice as fast, as their table lines say; in 1.3 they have no "
     "power. It is no Haste: you do not move faster, it does not add to Haste, and with Slow the rate is "
     "normal."),
    ("Dtu_fix_magic_tint", "-origobj", "The blue tint of Detect Magic is drawn on all magic things in "
                                       "view; 1.3 draws it mostly on near ones."),
    ("Dtu_fix_spell_save_repair", "-origchar", "Saves of older ASCENT versions get their spells moved "
                                               "back from the item list to the spell lists."),
    ("Dtu_fix_learn_class", "-origchar", "A character learns a spell only when its classes can cast it: "
     "a mage keeps Protection From Fire (a priest spell) as a scroll; 1.3 puts it into the mage list, "
     "where it cannot be cast."),
    ("Dtu_fix_time_wrap", "-origcombat", "The timers work after 9 hours of play, when the game clock turns "
     "negative: spells and potions last their full time, rest places, item pick up, foot steps and traps "
     "work. In 1.3 an effect that ends past that point ends at once, and after it no rest place works and "
     "no item can be picked up until one is dropped."),
    ("Dtu_fix_sleep_wake", "-origcombat", "Any damage wakes a sleeping character, as in AD&D 2nd edition; "
     "in 1.3 only a melee hit does."),
    ("Dtu_sleep_view", "-origcombat or -nosleepview", "While the character sleeps the view is black with "
     "\"zzz\", and the character cannot move, look around or act; in 1.3 nothing shows the sleep."),
    ("Dtu_fix_stoneskin_word", "-origcombat", "Stone Skin keeps your other spell effects."),
    ("Dtu_fix_rary_manual", "-origcombat", "Rary's Mnemonic Enhancer follows the manual."),
    ("Dtu_fix_melf_acid", "-origcombat", "Melf's Acid Arrows makes an attack roll and burns 2d4 a "
                                         "round for 1 round + 1 per 3 levels, as in AD&D 2nd edition; "
                                         "1.3 does 4 per level in one hit, save for half."),
    ("Dtu_fix_bolt_pass", "-origcombat", "Lightning Bolt goes on through each creature on its line, and "
                                         "comes back from a wall, until its range ends, as in AD&D 2nd "
                                         "edition; in 1.3 the first creature or the player ends it."),
    ("Dtu_fix_stoneskin_hit", "-origcombat", "Stone Skin takes a hit of a creature only after its attack "
                                             "roll hits; in 1.3 a miss uses one up too (D-197)."),
    ("Dtu_fix_hold_hit", "-origcombat", "An attack on a held target (Hold Person, Hold Undead, the stun of "
                                        "Mind Blast) hits with no roll, as in AD&D 2nd edition; 1.3 rolls "
                                        "(D-198)."),
    ("Dtu_fix_player_hold", "-origcombat", "Hold Person holds you for 6 to 18 seconds and blocks each action "
                                           "but the Potion of Freedom; 1.3 holds for 2 minutes per level "
                                           "divided by half the Wisdom and lets you attack and cast."),
    ("Dtu_fix_dispel_peace", "-origcombat", "Your Dispel Magic is no attack on a creature that is not "
                                            "hostile: it does not wake it, make it hostile or alert its "
                                            "friends, so Daland of the Temple of Myrkul can be freed (D-196)."),
    ("Dtu_fix_burning_hands", "-origcombat","Burning Hands burns each creature within 5 feet in an arc "
                                             "of 120 degrees in front of you, one roll each, as in AD&D 2nd "
                                             "edition; 1.3 fires 6 missiles that fly far and burn where they land."),
    ("Dtu_fix_haste_friends", "-origcombat", "Haste hastes only you and the creatures that are not "
                                            "hostile to you; in 1.3 it hastes every creature around you."),
    ("Dtu_fix_hold_undead_hd", "-origcombat", "Hold Undead holds undead whose levels add up to your "
                                             "level; in 1.3 each undead of your level or less."),
    ("Dtu_fix_haste_count", "-origcombat", "A creature that casts Haste hastes up to its level in "
                                          "creatures; in 1.3 it hastes no one."),
    ("Dtu_fix_fireball_blind", "-origcombat", "A made saving throw stops the blindness of Fireball near "
                                              "the point; in 1.3 the save does not."),
    ("Dtu_fix_wizardry_ring", "-origcombat", "The Ring of Wizardry doubles the slots once."),
    ("Dtu_fix_ability_saves", "-origcombat", "Dwarves and halflings get their Constitution bonus "
                                             "against Rod/Staff/Wand and Spell, and Wisdom changes the "
                                             "save against spells that attack the mind, as in AD&D 2nd "
                                             "edition; in 1.3 they do not."),
    ("Dtu_fix_prot_saves", "-origcombat", "The best Ring, Cloak or Amulet of Protection also makes your "
                                          "saving throws better by its bonus; in 1.3 no item changes a save. "
                                          "The statistics screen shows the five saves."),
    ("Dtu_fix_bracers_ac", "-origcombat", "Rings and cloaks of protection count with the Bracers of "
                                          "Defense; in 1.3 the bracers are magic armor and they do not."),
    ("Dtu_fix_armoring_cap", "-origcombat", "With the Ring of Armoring a better armor never gives a mage a "
                                            "worse armor class under Armor, Magical Vestment or Shield; in "
                                            "1.3 Bracers of Defense AC6 give AC 6 with Armor and AC8 give AC 5."),
    ("Dtu_fix_ring_magic_armor", "-origcombat", "Only the highest magic protection bonus counts, of the magic "
                                                "of the armor, a Ring, a Cloak and an Amulet of Protection; "
                                                "they count with any armor and shield. In 1.3 no ring counts "
                                                "with magic armor, the cloak only with no shield, magic or "
                                                "heavy armor, and a ring, a cloak and an amulet add."),
    ("Dtu_fix_potion_frame", "-origcombat", "The intelligence potion does not depend on the frame rate."),
    ("Dtu_fix_luck_coin", "-origcombat", "The Potion of Luck uses a coin flip, not the day clock."),
    ("Dtu_fix_cursed_scroll", "-origcombat", "The Cursed Scroll can take up to all but one of your hit "
                                             "points; 1.3 does less than half a point."),
    ("Dtu_fix_creature_missile", "-origcombat", "Creature missiles do not get the bonus of your bow and "
                                                "do not use up your Potion of Luck."),
    ("Dtu_fix_partial_stack", "-origobj", "When a stack does not fit, you take what fits and the rest "
                                          "stays on the floor; 1.3 loses the rest."),
    ("Dtu_fix_guild_peace", "-origobj or -origguildpeace", "When the Shadow Thieves are at peace with you, "
                                                         "their members stay peaceful; 1.3 makes most of "
                                                         "them hostile again at the level start."),
    ("Dtu_fix_kobold_weapon", "-origobj", "The kobold spellcaster keeps its weapon on a return visit."),
    ("Dtu_fix_drow5_shields", "-origobj or -origdrow5", "The dragon hatchling of the drow level has its 100 "
                                                       "hit points; 1.3 reads a wrong table entry."),
    ("Dtu_fix_scenery_restore", "-origobj", "Breakable scenery keeps its hit points on a return visit; "
                                            "1.3 sets it to 1."),
    ("Dtu_improved_ai (Improved AI)", "-origobj, -noimprovedai or -noimprovements", "Creatures notice you only along an open "
     "way (a wall or a closed door hides you), an alarm stops at a closed door, only humanoid creatures that "
     "fight you open doors, a blow or a missile stops at a wall or a closed door, melee creatures walk up "
     "and attack at once, and a creature that would turn hostile after a talk joins the fight of its group "
     "([[Monster-Behavior]]). In 1.3 a melee creature notices you through a closed door."),
    ("Dtu_caster_ai (Caster AI)", "-origcasterai, -origobj or -noimprovements", "Each kind of caster has "
     "its own spells, protects itself when it notices you, keeps its distance and casts each round; new "
     "spells Mirror Image, Minor Globe of Invulnerability, Spiritual Hammer, Flame Strike and Mind Blast "
     "([[Monster-Behavior]], the spell pages). In 1.3 every caster picks from one table and rarely attacks."),
    ("Dtu_new_gear (New gear)", "-nonewgear, -origitems or -noimprovements", "New content: new magic items, "
     "wands and loot, more gold and gems ([[New-Gear]])."),
    ("Dtu_class_gear (Gear for single class mages and thieves)", "-noclassgear, -origitems, -origscript or "
     "-noimprovements", "New content: offers of Aleena Paladinstar, Nalbas Oddpepper and Mirt for a "
     "character with one class, a mage or a thief ([[New-Gear]])."),
    ("Dtu_restored_dragon (The dragon Tamaramagil)", "-origobj, -origtamaramagil or -noimprovements", "New content: the red "
     "dragon of the Temple of the Drow that the files of 1.3 have but no level places ([[Encounters]])."),
    ("Dtu_lich_phylactery (The phylactery of the lich)", "-origobj, -nophylactery or -noimprovements", "New content: an altar of Velsharoon in the east "
     "crypts of the Crypts of Clangeddin is the phylactery of the lich Morik Stormhand. Break it and he "
     "fights without Stone Skin, Shield, Minor Globe of Invulnerability and Mirror Image; break it during "
     "the fight and they end at once."),
    ("Dtu_fix_bane_texture", "-origobj", "Two spinning objects in Bane's level get their own texture; 1.3 "
                                         "takes it from the level played before."),
    ("Dtu_fix_revive", "-origobj or -origrevive", "A killed troll (a regenerating creature) gets up again "
     "unless fire or acid hurt it. Fire or acid on the fallen body, or weapon damage equal to its full hit "
     "points while it is down, destroys it. It gives its experience one time, when it stays dead; in 1.3 "
     "it never gets up."),
    ("Dtu_fix_body_ray", "-origcombat", "An arrow, bolt or thrown weapon shot steeply down at a low or wide "
     "creature from close range hits it; in 1.3 it misses."),
    ("Dtu_fix_prison_items", "-origprison", "In the prison of the Shadow Thieves every item that you do "
     "not wear goes to the thieves; 1.3 loses every second item and copies the next one."),
    ("Dtu_fix_market_charisma", "none", "The market uses the Charisma of your character; 1.3 uses the "
     "Charisma of the character made last in the same run of the program."),
    ("Dtu_fix_market_sell_count", "none", "A sale adds the number sold to the stock of the market; 1.3 "
     "adds the number that you had."),
    ("Dtu_fix_market_charges", "-origfe", "A wand from the market that has no count gets the charges of its "
     "kind; 1.3 gives it charges with no limit."),
    ("Dtu_fix_market_buy_stock", "-origfe", "Items that your bag cannot take stay in the market; 1.3 removes "
     "them from the stock."),
    ("Dtu_fix_market_reward", "-origfe", "Each reward of Khelben adds its goods to the market, also before "
     "your first visit there, and a saved game gets the goods of the rewards it missed; 1.3 loses the goods "
     "of a reward before the first visit, and the port had no reward goods before 2026-11-27 (D-149)."),
    ("Dtu_fix_market_spell_line", "-origfe", "A bought scroll that goes into a spell book gets no sale line; "
     "in 1.3 that line sells for no gold (D-194)."),
    ("(port correction)", "none", "The market buys the scrolls of the item list, as 1.3 does (0x8770C); the "
     "port did not offer them before 2026-11-27."),
    ("(port correction)", "none", "Typed names (the character, a saved game) follow the keyboard layout of "
     "the system: dead keys, AltGr and the letters of the fonts work; the keys of the controls stay where "
     "they are."),
    ("Dtu_fix_item_names", "-origitems", "Items with a wrong name show the right one: the Cloak of Lathander "
                                         "(1.3: Lathandar), the Ring of Armoring and three more."),
    ("Dtu_rulebook_items (off by default)", "-rulebookitems turns it on; -norulebookitems, -origitems "
     "and -noimprovements turn it off",
     "The setting Rulebook items: some item values and class limits of the AD&D 2nd edition rulebooks."),
    ("Dtu_max_hp_level_up (off by default)", "-maxhplevelup turns it on; -nomaxhplevelup turns it off",
     "The setting Maximum HP on level up: a level that rolls its hit die gets the full die (the constitution "
     "bonus and the half for two classes stay)."),
]


# ---------------------------------------------------------------------
# Icons

# The icon of an item or a spell is icon= of its line (0xB69AD for a spell,
# byte 2 of the item record). The game draws it from the icon sheet of
# FEBGND.DAT (0x3420C): entry 23 (16x16 cells) in low resolution, entry 24
# (32x32 cells) in high resolution. Eight icons in a row: icon n is at
# column n & 7, row n / 8 (0x36E3F). Icon 0 is "no icon". The game uses the
# palette of PALETTE.256, not the one of the entry, and color 255 is
# transparent (docs/re/ingame-ui.md). The wiki uses the high resolution
# sheet at its own size: the cells are drawn for 32x32 and are not a
# doubled copy of the low resolution cells.
ICON_ENTRY = 24
ICON_CELL = 32
ICON_SCALE = 1
ICON_TRANSPARENT = 255


class IconSheet:
    def __init__(self, game_dir):
        arc = jrg.Archive(os.path.join(game_dir, "FEBGND.JRG"))
        if len(arc.entries) <= ICON_ENTRY:
            raise SystemExit("FEBGND.JRG: no entry %d" % ICON_ENTRY)
        e = arc.entries[ICON_ENTRY]
        if e["w"] != 8 * ICON_CELL or e["h"] % ICON_CELL:
            raise SystemExit("FEBGND entry %d: %dx%d is not a sheet of %d pixel icons"
                             % (ICON_ENTRY, e["w"], e["h"], ICON_CELL))
        self.pix, _ = jrg.lzss_decode(arc.data, e["off"] + 768, e["off"] + e["size"])
        if len(self.pix) != e["w"] * e["h"]:
            raise SystemExit("FEBGND entry %d: the picture does not decode" % ICON_ENTRY)
        self.w = e["w"]
        self.count = 8 * (e["h"] // ICON_CELL)
        # The icons of the port (200 and up: the new spells, the Pearls of
        # Power), new art of tools/spell_icons.py in engine/main/dtuspellicons.h.
        # The game adds them to the loaded sheet (port_icons_add, dtudisp.c).
        self.port = port_tables.icons()
        self.palette = egg.load_palette(os.path.join(game_dir, "PALETTE.256"))
        # The tinted icons of the bows and arrows of the new gear (204 and up):
        # copies of icons of the sheet in the colors of a tint, the rule of
        # tint_icons of dtudisp.c with the palette of the entry
        pal = arc.data[e["off"]:e["off"] + 768]
        g = gear_tables.read()
        first = g["d"]["DTU_GEAR_ICON_ARROWS3"]
        for k, (src, r, gr, b) in enumerate(g["icon_tints"]):
            cmap = []
            for c in range(256):
                lum = (pal[c * 3] * 30 + pal[c * 3 + 1] * 59 + pal[c * 3 + 2] * 11) // 100
                lv = min(63, lum * 3 // 2)
                want = (r * lv // 63, gr * lv // 63, b * lv // 63)
                best = min(range(254), key=lambda j: sum((want[x] - pal[j * 3 + x]) ** 2 for x in range(3)))
                cmap.append(best)
            cmap[255] = 255
            x0, y0 = (src & 7) * ICON_CELL, (src // 8) * ICON_CELL
            cell = []
            for y in range(ICON_CELL):
                p = (y0 + y) * self.w + x0
                cell += [cmap[v] for v in self.pix[p:p + ICON_CELL]]
            self.port[first + k] = bytes(cell)
        self.count = max([self.count] + [n + 1 for n in self.port])

    def write(self, n, path):
        c, s = ICON_CELL, ICON_SCALE
        x0, y0 = (n & 7) * c, (n // 8) * c
        out = bytearray()
        for y in range(c * s):
            if n in self.port:
                row = self.port[n][(y // s) * c:(y // s) * c + c]
            else:
                p = (y0 + y // s) * self.w + x0
                row = self.pix[p:p + c]
            out += bytes(v for v in row for _ in range(s))
        egg.write_png(path, c * s, c * s, out, self.palette, transparent=(ICON_TRANSPARENT,))


def check_icon_dir(icon_dir):
    """Refuse to write the icons into the ASCENT repository unless git ignores
    the place: the icons are pictures of the game. Another repository (a
    clone of the GitHub wiki) is fine."""
    def git(cwd, *args):
        try:
            r = subprocess.run(["git", "-C", cwd] + list(args), capture_output=True, text=True)
        except OSError:
            return None, ""
        return r.returncode, r.stdout.strip()

    here = os.path.dirname(os.path.abspath(__file__))
    rc, code_common = git(here, "rev-parse", "--path-format=absolute", "--git-common-dir")
    if rc != 0:
        return  # the tool is not in a git checkout
    probe = os.path.abspath(icon_dir)
    while not os.path.isdir(probe):
        probe = os.path.dirname(probe)
    rc, out_common = git(probe, "rev-parse", "--path-format=absolute", "--git-common-dir")
    if rc != 0 or os.path.normcase(os.path.normpath(out_common)) != os.path.normcase(os.path.normpath(code_common)):
        return  # not in the ASCENT repository
    sample = os.path.join(os.path.abspath(icon_dir), "icon-1.png")
    rc, _ = git(probe, "check-ignore", "-q", sample)
    if rc != 0:
        raise SystemExit("%s: git does not ignore this place of the ASCENT repository; the icons are "
                         "pictures of the game. Use an ignored OUT_DIR (gamedata/wiki) or --no-icons."
                         % icon_dir)
    rc, tracked = git(probe, "ls-files", "--", os.path.abspath(icon_dir))
    if rc != 0 or tracked:
        raise SystemExit("%s: the ASCENT repository tracks files here" % icon_dir)


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


def cast_text(sec):
    # The cast time of 0x13A8DB in game seconds; 600 is a turn
    return ("%d min" % (sec // 60)) if sec > 60 else ("%d s" % sec)


def classes_of(rec):
    if rec.has("all_use"):
        return "F/T/M/C"
    out = []
    for flag, letter in (("fighter_use", "F"), ("thief_use", "T"), ("mage_use", "M"), ("cleric_use", "C")):
        out.append(letter if rec.has(flag) else "-")
    return "/".join(out)


def value_of(rec):
    v = rec.get("value")
    return v if v is not None else "-"


def signed(v):
    return ("+%d" % v) if v > 0 else str(v)


def pages_note():
    return "Where these numbers come from: [[Sources]]."


# ---------------------------------------------------------------------
# Pages

class Pages:
    def __init__(self, tab, ct, check=None, mission=(), icon_count=0, gear=None):
        self.t = tab
        self.c = ct
        self.W = tab["$WEAPON"]
        self.A = tab["$ARMOR"]
        self.O = tab["$OBJECT"]
        self.S = tab["$SPELL"]
        self.check = check
        self.mission = set(mission)
        self.counts = {}
        # icon_count: the number of cells of the icon sheet; 0 for no icons
        self.icon_count = icon_count
        self.icons = icon_count > 0
        self.used_icons = set()
        self.gear = gear        # GearData: the new gear of ASCENT-DTU
        # the spell lines of the port (56 and up) and their cast times
        self.port_spells, self.port_cast = {}, {}
        for n, (line, ct) in port_tables.spells().items():
            self.port_spells[n] = Record(line)
            self.port_cast[n] = ct
        # the scrolls of Caster AI: {spell: [(level, the spell of 1.3)]}
        self.port_scrolls = {n: [(lvl, self.S[old].name()) for lvl, _, old in rows]
                             for n, rows in port_tables.port_scrolls().items()}
        self.casters = port_tables.casters()
        self.dragon = port_tables.dragon()
        self.ai = port_tables.improved_ai()
        self.item_fixes()

    def spell_rec(self, s):
        """The line of spell s: of the table, or of the port (56 and up)."""
        return self.S[s] if s < len(self.S) else self.port_spells[s]

    # -- icons ----------------------------------------------------------
    def icon_cell(self, *recs):
        """The icon of each line as an <img> tag (no "|", so it fits a table
        cell; GitHub wikis resolve a relative src from the wiki root)."""
        tags = []
        for rec in recs:
            try:
                n = int(rec.get("icon", "0"))
            except ValueError:
                n = 0
            if 0 < n < self.icon_count:
                self.used_icons.add(n)
                tags.append('<img src="icons/icon-%d.png" alt="%s">' % (n, html.escape(rec.name().strip())))
        return " ".join(tags)

    def icon_head(self, head):
        return [" "] + head if self.icons else head

    def icon_row(self, row, *recs):
        return [self.icon_cell(*recs)] + row if self.icons else row

    # -- the corrections of ASCENT (engine/main/dtuitemfix.c) ------------
    def item_fixes(self):
        """Sets the corrected name and the notes of each item line that
        dtuitemfix.c names, and keeps the rows for the pages."""
        self.name_fixes, self.rules = check_itemfix.read_tables()
        secs = {"W": self.W, "A": self.A, "M": self.O}
        self.rule_rows = []
        for n in self.name_fixes:
            r = secs[n["kind"]][n["id"]]
            if r.name13() != n["name13"].strip():
                raise SystemExit("dtuitemfix.c: %s %d is %r, not %r" % (n["kind"], n["id"], r.name13(), n["name13"]))
            r.shown = n["name"]
            r.fix_notes += ("The original 1.3 game calls it \"%s\"; ASCENT corrects the name." % r.name13(),)
        by_item = collections.OrderedDict()
        for x in self.rules:
            by_item.setdefault((x["kind"], x["id"]), []).append(x)
        for (kind, i), rows in by_item.items():
            r = secs[kind][i]
            new = {x["key"].lower(): x["v"] for x in rows}
            parts13, parts = [], []
            if any(k in new for k in ("damage_dice", "#damage_dice", "damage_bonus", "to_hit_bonus")):
                def dice(get):
                    b, h = int(get("damage_bonus", 0)), int(get("to_hit_bonus", 0))
                    return "%dd%d%s, to hit %s" % (int(get("#damage_dice", 1)), int(get("damage_dice", 6)),
                                                    signed(b) if b else "", signed(h) if h else "0")
                parts13.append(dice(lambda k, d: r.num(k, d)))
                parts.append(dice(lambda k, d: new.get(k, r.num(k, d))))
            if new.get("thief_use") == "0":
                parts13.append("a thief can use it")
                parts.append("not for a thief")
            if new.get("all_use") == "0" and new.get("mage_use") == "1":
                parts13.append("every class can use it")
                parts.append("only a mage can use it")
            if new.get("magic") == "1":
                parts13.append("not magic")
                parts.append("magic")
            source = "; ".join(collections.OrderedDict((x["source"].split(":")[0], 1) for x in rows))
            r.fix_notes += ("With Rulebook items: %s (%s)." % ("; ".join(parts), source),)
            self.rule_rows.append([r.name(), "; ".join(parts13), "; ".join(parts), source])

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
        r = self.spell_rec(s)
        return r.has("self_cast_ok") or r.has("cast_in_front")

    def spell_area(self, s):
        return (self.spell_rec(s).get("type") or "").upper() == "AREA"

    def count(self, page, n):
        self.counts[page] = self.counts.get(page, 0) + n

    # -- Home -----------------------------------------------------------
    def home(self):
        return "\n".join([
            "# Descent to Undermountain: game rules",
            "",
            "These pages describe the rules of Descent to Undermountain 1.3 as the game really plays. "
            "The manual is often wrong about them, and the game shows no description of a magic "
            "item. The numbers come from the game data and from the code of the game, read for the "
            "ASCENT source port.",
            "",
            "ASCENT plays by these rules. Where the original 1.3 game has a defect that ASCENT "
            "corrects, the page says so in one line. Start ASCENT with the options of the "
            "[[Sources]] page (`-origcombat`, `-origchar`, `-origobj`, `-origitems`) to play with the "
            "original behavior.",
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
            "- [[New gear and loot|New-Gear]]: the new items, wands and loot of ASCENT-DTU (spoilers)",
            "- [[Encounters]]: the dragon Tamaramagil and the lich Morik Stormhand (spoilers)",
            "- [[Monster behavior|Monster-Behavior]]: how creatures notice you, open doors, join fights "
            "and cast spells",
            "- [[Sources]]: where each number comes from",
            "",
            "## Settings of ASCENT",
            "",
            "The Improvements menu (in the options of the main menu and of the game menu) has these "
            "settings that change the rules. Each is on by default unless the line says otherwise.",
            "",
            "- Rulebook items (off by default): some item values and class limits of the AD&D 2nd edition "
            "rulebooks ([[Magic Items|Magic-Items]]).",
            "- Maximum HP on level up (off by default): a new level gives the full hit die.",
            "- Improved AI: creatures notice you only along an open way, open doors only in a fight, stop "
            "their blows at walls and doors, attack at once in reach, and join the fight of their group "
            "([[Monster-Behavior]]).",
            "- Caster AI: each kind of spellcaster has its own spells, protects itself the moment it "
            "notices you, keeps its distance and casts each round; it brings the new spells Mirror Image, "
            "Minor Globe of Invulnerability and Spiritual Hammer ([[Monster-Behavior]], "
            "[[Wizard Spells|Wizard-Spells]], [[Priest Spells|Priest-Spells]]).",
            "- New gear: new magic items, wands, the Rod of Kelemvor, more gold and gems, and magic items "
            "of the original game in the market ([[New-Gear]]).",
            "- Gear for single class mages and thieves: offers for a character with one class, a mage or "
            "a thief ([[New-Gear]]).",
            "- The dragon Tamaramagil: the red dragon of the drow temple and her hatchlings "
            "([[Encounters]]).",
            "- The phylactery of the lich: the wards of the lich Morik Stormhand are bound to an altar that "
            "you can break ([[Encounters]]).",
            "",
            "Some corrections of ASCENT that a player notices (all of them: [[Sources]]):",
            "",
            "- The game clock no longer breaks after about 9 hours of play: in the original 1.3 game spell "
            "and potion effects then end at once, the rest places stop working and nothing can be picked "
            "up.",
            "- The market grows with Khelben's rewards, also before your first visit, and a saved game "
            "gets the goods of the rewards it missed.",
            "- The market buys scrolls: spells that you cannot learn and spare copies ([[Scrolls]]).",
            "- Typed names follow the keyboard layout of your system (dead keys, AltGr, accented letters).",
            "- Stone Skin, Hold Person, Sleep, Dispel Magic and Lightning Bolt follow AD&D 2nd edition "
            "more closely (the spell pages).",
            "",
            "## Units",
            "",
            "Distances are in feet. Times are in game time: the game calls 60 game seconds a minute "
            "and 60 minutes an hour.",
            "",
            "## Words",
            "",
            "- Level, or caster level: your level in the class of the spell. A wand or a scroll also "
            "uses a class level ([[Wands]]).",
            "- Level of a creature: its maximum hit points / 8, from 1 to 18.",
            "- Save: a saving throw. The target rolls d20 and is safe at the number of its table or "
            "more ([[Classes]]).",
            "- F, T, M, C: fighter, thief, mage, cleric. The game has only these four classes. The "
            "manual says priest for cleric and wizard for mage.",
            "- People: human-like creatures of the dungeon, such as humans and drow. Some spells treat "
            "them apart.",
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
            "- [[New gear and loot|New-Gear]]",
            "- [[Encounters]]",
            "- [[Monster behavior|Monster-Behavior]]",
            "- [[Sources]]",
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
        sec = ["%s %d in 6" % (names[i].lower(), d6[tidx[i]]) for i in range(6)]
        special = [
            ["Infravision (key N)", "all but human: half-elf, elf, drow, dwarf, halfling"],
            ["Resists Sleep", "elf 9 times in 10, half-elf 3 times in 10; drow do not"],
            ["A save against Scare (others get none)", "elf, half-elf (also any cleric)"],
            ["Move Silently with no roll and without the thief class (not in chain or plate mail)",
             "elf, drow, halfling"],
            ["Find/Remove Traps 50% without the thief class", "dwarf"],
            ["The spell, the potion and the Ring of Infravision are of use", "human only"],
            ["Finds secret doors while searching (key F): one d6 roll for each secret door or wall "
             "within 20 ft in front of you", ", ".join(sec)],
        ]
        self.count("Races", len(rows) + len(crow) + len(trow) + len(special))
        return "\n".join([
            "# Races",
            "",
            "Six races, chosen on the race screen when you make a character.",
            "",
            "## Ability scores",
            "",
            "Each score is rolled as 3d6 with ones rolled again (6 to 18). You get 7 bonus points to "
            "spend. The racial adjustment is added, and the result must stay within the limits. A "
            "fighter that is not a halfling can put points past 18 into exceptional strength, 10 "
            "percent for each point. A halfling gets the percentile 01 instead of a roll.",
            "",
            md_table(["Race", "Adjustment " + " ".join(stats), "Minimum", "Maximum"], rows),
            "",
            "Class minimums: fighter STR 9, thief DEX 9, mage INT 9, cleric WIS 9.",
            "",
            "## Classes",
            "",
            "One class or two. No race can have three classes, and no race can be a cleric/thief.",
            "",
            md_table(["Race", "Allowed classes"], crow),
            "",
            "## Level limits",
            "",
            "There is no level limit by race. Every class of every race stops at level 18.",
            "",
            "## Special abilities",
            "",
            md_table(["Ability", "Races"], special),
            "",
            "Saving throws have no racial bonus: only the class and the level count.",
            "",
            "The search for secret doors works for every race and class, also without the Find/Remove "
            "Traps skill, but not while the Find Traps spell is on. Each secret door or wall gets one "
            "roll only: a failed roll does not come again, also when you come back to the level.",
            "",
            "A half-elf has infravision, as in the AD&D rules. The spell and the Ring of Infravision "
            "also give their effect to a half-elf (the game tests for the human kind, and a half-elf "
            "is of it), but a half-elf gains nothing from them.",
            "",
            "## Thief skills by race",
            "",
            "Added to the start values of a thief ([[Classes]]).",
            "",
            md_table(["Skill"] + names, trow),
            "",
            "## Differs from the manual",
            "",
            "- The manual lets only humans, elves and half-elves be mages. The game also allows drow.",
            "- The manual does not list the mage/cleric for half-elves. The game allows it.",
            "- 7 bonus points, not 5.",
            "- The manual says elves and drow are nearly immune to Sleep. In the game only the elf is.",
            "- No racial saving throw bonus and no level limits by race. The manual gives dwarves and "
            "halflings a bonus against spells and lets humans advance without limit.",
            "",
            pages_note(),
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
            slot_rows.append([str(r + 1), " ".join(str(x) for x in ms[r]).rstrip(" 0") or "0",
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
        thief_items = [
            ["Chain mail or plate mail (any)",
             "Hide In Shadows, Move Silently and Climb Walls 0; Move Silently and Climb Walls refuse"],
            ["Gauntlets", "Find/Remove Traps and Open Locks 0"],
            ["Lockpicks in the lockpick place (thief only)",
             "Open Locks: Lockpicks +5, Apprentice +10, Journeyman +15, Master +20"],
            ["Gloves of Dexterity", "Open Locks +10 (thief); 37 for others"],
            ["Cloak of Elvenkind", "Hide In Shadows 95"],
            ["Invisible (spell, potion, cloak)", "Open Locks -15"],
            ["Spider Climb, Levitate", "Climb Walls 100"],
            ["Sword of W'hin in the hand", "Find/Remove Traps 95, Climb Walls 95"],
            ["Find Traps (spell)", "Find/Remove Traps 100 for the rolls"],
            ["A magic trap (one that makes a creature or a thing)", "Find/Remove Traps counts half"],
        ]
        self.count("Classes", len(xp_rows) + len(th_rows) + len(hp_rows) + len(save_rows) + len(abil)
                   + 1 + len(slot_rows) + len(wis_rows) + len(tdex_rows) + len(thief_items) + len(turn_rows) + 4)
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
                ["Fighter", "d%d" % die[2], "items marked F",
                 "exceptional strength 18/xx (not a halfling); the best THAC0; more hit points from CON"],
                ["Thief", "d%d" % die[3], "items marked T; chain and plate mail stop three skills",
                 "thief skills, backstab"],
                ["Mage", "d%d" % die[1], "items marked M; no mage spell in body armor or with a shield",
                 "mage spells"],
                ["Cleric", "d%d" % die[0], "items marked C", "cleric spells, turn undead (key T)"],
            ]),
            "",
            "Each item is marked for the classes that may use it (the F/T/M/C columns of the item "
            "pages). You can pick up, carry and sell any item. In the inventory screen an item that "
            "none of your classes may use does not go onto the paper doll, into your hand or into a "
            "hot key box. There is no message.",
            "",
            "Mage spells in armor: a mage cannot cast a mage spell while wearing body armor or a "
            "shield (\"You cannot cast a spell while wearing ...\"). Rings, amulets, cloaks, boots, "
            "bracers, gloves and the girdle allow spells.",
            "",
            "## A new character",
            "",
            "- Hit points: the full hit die plus the Constitution adjustment (the fighter column for a "
            "fighter). Two classes: the mean of the two, rounded down.",
            "- Gold: 102. Each character first gets a Scroll ([[Scrolls]]).",
            "- Fighter: Broad Sword, Leather Armor, Gem of Curing, Potion of Healing.",
            "- Thief: Short Sword, Leather Armor, Apprentice Lockpicks, Potion of Healing.",
            "- Mage: Dagger, Potion of Healing, and six spells: min(INT, 18) / 3 of Armor, Burning "
            "Hands, Chill Touch, Color Spray, Detect Magic, Feather Fall, Jump, Light, Magic Missile, "
            "Protection From Evil, Shield, Sleep and Spider Climb at random, the rest Sleep.",
            "- Cleric: Mace, Leather Armor, Potion of Healing, and every cleric spell of level 1. A "
            "cleric knows all the cleric spells of each spell level that it can cast: level 1 from "
            "the start, level 2 at cleric level 3, level 3 at level 5 and level 4 at level 7. The "
            "new spells come at each new level. The spell slots for each level do not change.",
            "- The empty hand does 1d2 plus the Strength bonuses.",
            "",
            "## Experience",
            "",
            "Experience for each level. With two classes each point you gain is halved (at least 1) "
            "and goes to both classes. Level 18 is the last level.",
            "",
            md_table(["Level", "Fighter", "Thief", "Mage", "Cleric"], xp_rows, "rrrrr"),
            "",
            "## Hit points",
            "",
            "At each new level you roll one hit die, from half the die to the full die, or you get a "
            "fixed number. The Constitution adjustment is added at every level, also to the fixed "
            "number. With two classes the sum of the new levels is halved. The new hit points raise "
            "your maximum and your current hit points. The original 1.3 game raises the maximum "
            "only, so you start the new level with almost the same hit points; ASCENT corrects this.",
            "",
            md_table(["Class", "Hit die (level 1)", "Levels with a die: hit points", "Fixed"], hp_rows),
            "",
            "## THAC0",
            "",
            "An attack hits when d20 + bonuses >= THAC0 - the armor class of the target. A 20 is a "
            "critical hit: the damage is rolled twice. A 1 is a fumble: on a second d20 above your "
            "Dexterity you drop the weapon.",
            "",
            md_table(["Level", "Fighter", "Thief", "Mage", "Cleric"], th_rows, "rrrrr"),
            "",
            "The values are one level ahead of the AD&D table: a first level fighter has 19. Level 18 "
            "has the value of level 17. With two classes the original 1.3 game takes the worse "
            "value; ASCENT corrects this and takes the better one.",
            "",
            "## Ability scores",
            "",
            md_table(["Score", "STR to hit", "STR damage", "DEX armor class", "CON hit points",
                      "CON (fighter)", "CHA market buy / sell"], abil, "rrrrrrr"),
            "",
            "Strength 18 of a fighter:",
            "",
            str18,
            "",
            "- Charisma sets the market prices: the percent of the value that you pay or get.",
            "- Intelligence decides the number of first spells of a mage. The statistics screen also "
            "shows a spells column for Intelligence (%s for INT 9 to 19); it changes nothing."
            % ", ".join(str(x) for x in c["int_spells"][9:20]),
            "- Wisdom gives clerics bonus slots (below). It also shortens Fear, Scare, Hold Person "
            "and the stun of Color Spray on you: the time is divided by half your Wisdom.",
            "- Dexterity is also the number that the second d20 of a fumble must not exceed.",
            "",
            "## Saving throws",
            "",
            "Roll d20: the save succeeds at the number or more. The level is your higher class level, "
            "at most 12. With two classes the better number of the two tables counts. The best "
            "worn Ring, Cloak or Amulet of Protection lowers all five numbers by its bonus. A "
            "dwarf or halfling lowers Rod/Staff/Wand and Spell by its Constitution bonus (Con 4-6 "
            "1, 7-10 2, 11-13 3, 14-17 4, 18 or more 5). Wisdom changes the save against Confusion, "
            "Fear, Hold Person and Scare (Wis 3 -3, 4 -2, 5-7 -1, 15 +1, 16 +2, 17 +3, 18 +4). "
            "In 1.3 nothing changes a save. Spells use the Spell column. The statistics screen "
            "shows your five numbers.",
            "",
            md_table(["Class", "Levels", "Paralyzation, poison, death", "Rod, staff, wand",
                      "Petrification, polymorph", "Breath", "Spell"], save_rows),
            "",
            "## Spell slots",
            "",
            "Slots for each spell level, by class level. A rest and the start of each dungeon level "
            "fill them. You cannot cast a spell whose level is above your class level + 1. The game "
            "has spells of levels 1 to 4 only.",
            "",
            md_table(["Level", "Mage: spell levels 1 to 9", "Cleric: spell levels 1 to 7"], slot_rows),
            "",
            "Cleric bonus slots for Wisdom (only for a spell level that has a slot already):",
            "",
            md_table(["WIS", "Level 1", "Level 2", "Level 3", "Level 4"], wis_rows, "rrrrr"),
            "",
            "Mages get no bonus slots for Intelligence.",
            "",
            "## Thief skills",
            "",
            "A new thief starts with Hide In Shadows 5%, Move Silently 10%, Open Locks 10%, "
            "Find/Remove Traps 5% and Climb Walls 60%, plus the race ([[Races]]) and the Dexterity "
            "adjustment (Climb Walls has none). Each skill stays within 0 to 95%.",
            "",
            md_table(["DEX", "Hide In Shadows", "Move Silently", "Open Locks", "Find/Remove Traps"],
                     tdex_rows, "rrrrr"),
            "",
            "A skill roll succeeds when d100 <= the skill.",
            "",
            "Skill points: a new thief has 60 skill points (Player's Handbook). Mirt, at the "
            "Yawning Portal, lets you spend them, and adds 30 for each thief level gained since "
            "the last visit. In the original 1.3 game Mirt gives 20 for each level, and 20 more at "
            "the first visit, and replaces the points you have (the first visit at level 1 "
            "gives 40). Each point raises one "
            "skill by 1%, up to 95%. One skill can take at most half of the points of a visit. "
            "The screen closes with Escape only when the points are spent or no skill can take more; "
            "points that no skill can take are lost. If you answer No, Mirt offers the same points "
            "at the next talk.",
            "",
            md_table(["Item or effect", "Change"], thief_items),
            "",
            "- Open Locks (key O): one try for each lock and thief level. A lock with strong magic "
            "cannot be picked (\"Strong magic prevents you from picking this lock.\").",
            "- Find/Remove Traps (key F): search mode, movement at a third of the speed. One find roll "
            "for each trap and level; then you choose to remove it. A failed removal on d100 >= 96 "
            "sets the trap off.",
            "- Move Silently (key M) and Hide In Shadows (a roll each time you stop): creatures do not "
            "notice you. An attack ends hiding.",
            "- Climb Walls: the Climb Wall key (Q, R3 on the gamepad) at a wall, with no weapon in the hand. A failed "
            "roll is a slip. Activate takes hold of a ladder but does not climb a wall.",
            "- Backstab: a melee attack from behind a humanoid creature while Move Silently is on, "
            "while no creature that can see you watches you: +4 to hit, damage x2 at levels 1 to 4, x3 at 5 to 8, x4 at 9 to "
            "12, x5 at 13 to 16, x6 at 17 and 18. The Dagger of Impaling adds 3 to 6 on a roll of 19 "
            "or 20.",
            "",
            "## Turn undead",
            "",
            "Key T. A cleric turns at the cleric level; anyone who wears the Cloak of Lathander turns "
            "as a cleric of level 7. One d20 counts for all undead in sight. Each use turns or "
            "destroys at most 2 to 12 of them (each number from 2 to 12 is equally likely), the "
            "weakest kinds first. Half a second must pass between two uses. A number: the d20 must "
            "reach it. T: "
            "turned. D: destroyed. -: no effect (you can try again later). In the original 1.3 game "
            "a T makes every later undead of the same use turn, and the first undead after the 2 to "
            "12 can never be tried again; ASCENT corrects both.",
            "",
            md_table(["Undead"] + [str(i) for i in range(1, 15)], turn_rows),
            "",
            "Levels above 14 count as 14.",
            "",
            "## Rest",
            "",
            "Key R at a rest place (the turning ankh): hit points to the maximum and all spell slots "
            "full. The place works again after 5 minutes.",
            "",
            "## System shock",
            "",
            "Only the Potion of Toughening asks for a system shock roll ([[Potions]]). The chance to "
            "live, by Constitution 1 to 19: %s percent." % ", ".join(str(x) for x in shock),
            "",
            "## Difficulty",
            "",
            "The game has one difficulty. The difficulty menu of 1.3 has no choices and sets the "
            "middle difficulty of the engine, which sets the morale of the creatures, the speed of "
            "their missiles and the distance at which they notice you.",
            "",
            "## Differs from the manual",
            "",
            "- The manual shows the fixed hit points one level earlier (fighter and cleric from level "
            "10).",
            "- The manual gives racial save bonuses. The game has none.",
            "",
            pages_note(),
            "",
        ])

    # -- Weapons --------------------------------------------------------
    def weapon_row(self, i, r):
        # The $WEAPON reader 0x209F3 starts each line with the defaults of
        # 0x207E7: damage_dice=6 (sides), #damage_dice=1, so a line with no
        # dice does 1d6. 0x2AF5E adds damage_bonus (0x2B1E7).
        n = r.num("#damage_dice", 1)
        s = r.num("damage_dice", 6)
        b = r.num("damage_bonus", 0)
        h = r.num("to_hit_bonus", 0)
        # 0 dice (Flame Tongue after the table fixup) roll nothing
        if n and s:
            dice = "%dd%d" % (n, s) + (signed(b) if b else "")
        else:
            dice = str(b)
        fs = r.num("fire_damage_dice", 0)
        fn = r.num("#fire_damage_dice", 0)
        if fs and fn:
            dice += " + %dd%d fire" % (fn, fs)
        # fire_wait default 0.25 s (0x207E7 writes 0x4000 to +0x64); a melee
        # swing lasts max(fire_wait, 1.0) s (0x441E9)
        fw = float(r.num("fire_wait", 0.25))
        melee = (r.get("attack_type") or "").upper() != "PROJECTILE"
        speed = "%g s" % (max(fw, 1.0) if melee else fw)
        if melee and r.get("pc_spell") == "21":
            # Dtu_fix_speed_items: the swing of the Short Sword of Quickness
            # is twice as fast (engine/main/dtuatk.c)
            speed = "%g s" % (max(fw, 1.0) / 2)
        if "." not in speed:
            speed = speed.replace(" s", ".0 s")
        notes = []
        ac = self.c["ac_weapon"][i] if i < 83 else 0
        if ac:
            notes.append("Armor class %s while in the hand." % signed(ac))
        if r.get("weapon_name") and r.get("name") and r.get("weapon_name") != r.get("name"):
            notes.append("Also called %s." % r.get("weapon_name"))
        if i == 13:
            # a table cell: no "|" in the link
            notes.append("A talking sword with powers: [[Magic-Items]].")
        elif i in WEAPON_NOTES:
            notes.append(WEAPON_NOTES[i])
        if r.note():
            notes.append(r.note())
        return [r.name(), dice, signed(h) if h else "0",
                "yes" if r.has("magic") else "no", speed, classes_of(r), value_of(r), " ".join(notes)]

    def weapons(self):
        melee, thrown, ammo = [], [], []
        for i, r in enumerate(self.W):
            if i in WEAPON_INTERNAL or (r.get("pc_spell") and i != 69):
                continue
            proj = (r.get("attack_type") or "").upper() == "PROJECTILE"
            row = self.weapon_row(i, r)
            if not proj:
                melee.append((row, r))
            elif r.get("thrown") == "1":
                thrown.append((row, r))
            else:
                kind = "arrow" if r.has("arrow") else "bolt" if r.has("bolt") else "other"
                ammo.append(([row[0], kind] + row[1:], r))
        launch = []
        for i, r in enumerate(self.O):
            if r.has("bow") or r.has("crossbow"):
                note = ""
                if r.get("pc_spell1"):
                    note = SPEED_BOW_NOTE
                launch.append(self.icon_row([r.name(), "bow" if r.has("bow") else "crossbow",
                                             signed(r.num("to_hit_bonus", 0)), classes_of(r),
                                             r.num("value", 0), note], r))
        head = ["Weapon", "Damage", "To hit", "Magic", "Speed", "F/T/M/C", "Value", "Notes"]
        melee.sort(key=lambda x: x[0][0])
        thrown.sort(key=lambda x: x[0][0])
        ammo.sort(key=lambda x: (x[0][1], x[0][0]))
        melee, thrown, ammo = ([self.icon_row(row, r) for row, r in rows] for rows in (melee, thrown, ammo))
        self.count("Weapons", len(melee) + len(thrown) + len(ammo) + len(launch))
        return "\n".join([
            "# Weapons",
            "",
            "- Damage: the dice plus the bonus. Melee and thrown weapons add the Strength damage. "
            "Arrows and bolts add the bonus of the bow or crossbow to the attack and the damage. "
            "ASCENT-DTU, with the setting \"%s\": the bonuses to hit of the bow and the arrow add, and "
            "for the damage only the larger of the two counts ([[New gear|New-Gear#bows-and-arrows]]). A "
            "critical hit (20) rolls the damage twice. A backstab multiplies it ([[Classes]]). The "
            "game has no slashing, piercing or bludgeoning damage and no weapon sizes. Some weapons "
            "add fire dice, which fire resistance halves." % NEW_GEAR_SETTING,
            "- Speed: the time of a melee swing. Every swing lasts at least 1.0 s, so a faster weapon "
            "swings at 1.0 s. For a thrown or missile weapon it is the wait from one shot to the "
            "next (the original 1.3 game has no wait: each key press fires; ASCENT corrects this). "
            "Haste halves the time, also below 1.0 s, and Slow doubles it. The Short Sword of "
            "Quickness and the Short Bow of Speed halve it too, but not again with Haste. The level "
            "and the class do not change it: no character gets more attacks.",
            "- Magic: a creature that resists weapons takes no damage from a weapon that is not magic.",
            "- F/T/M/C: the classes that may use it ([[Classes]]).",
            "- Value: the base price in gold. -: the item has no price.",
            "",
            "## Melee weapons",
            "",
            md_table(self.icon_head(head), melee),
            "",
            "## Thrown weapons",
            "",
            "Thrown where you look; the stack is used up one by one. A thrown weapon also gets the "
            "to-hit bonus of a worn crossbow.",
            "",
            md_table(self.icon_head(head), thrown),
            "",
            "## Arrows and bolts",
            "",
            "An arrow needs a bow and a bolt needs a crossbow. The Flame Arrow needs neither.",
            "",
            md_table(self.icon_head(["Weapon", "Kind"] + head[1:]), ammo),
            "",
            "## Bows and crossbows",
            "",
            "Worn in the bow or crossbow place of the paper doll. The bonus is added to the attack "
            "and the damage of the missile. ASCENT-DTU, with the setting \"%s\": the bonus adds to the "
            "attack; for the damage the larger of the bonus of the bow and of the arrow counts. The "
            "Arrows +1 and +2 then have +1 and +2 to hit too, and the market sells magic arrows "
            "([[New gear|New-Gear#bows-and-arrows]])." % NEW_GEAR_SETTING,
            "",
            md_table(self.icon_head(["Launcher", "Kind", "To hit and damage", "F/T/M/C", "Value", "Notes"]), launch),
            "",
            "## New in ASCENT-DTU",
            "",
            "With the setting \"%s\": the %s is the gift of Daland at the altar of Kelemvor, a %s lies in "
            "the drow city, the market sells the magic weapons that the original game never gives, "
            "and five new wands cast spells ([[Wands]]). See [[New gear and loot|New-Gear]]."
            % (NEW_GEAR_SETTING, self.gear.item(5, self.gear.g["daland"][2]).name(), self.gear.item(5, self.gear.g["flame"][1]).name()),
            "",
            "## Differs from the manual",
            "",
            "- The manual gives thieves daggers, short swords, long swords, scimitars, clubs, darts, "
            "throwing daggers and short bows. The game also gives thieves the Broad Sword.",
            "- The manual gives mages daggers and darts. The game also allows the Throwing Dagger.",
            "- The manual says the dagger is fast. In the game no melee weapon swings faster than "
            "1.0 s without Haste.",
            "- Drowslayer and Longsword +3 are new items of the 1.3 readme. Both fight as a Long "
            "Sword with their bonus. The powers of the Drowslayer are on [[Magic Items|Magic-Items]].",
            "",
            pages_note(),
            "",
        ])

    # -- Armor ----------------------------------------------------------
    def armor(self):
        rows = []
        for i, r in enumerate(self.A):
            if not (r.has("armor") or r.has("shield")) or 39 <= i <= 42:
                continue
            ac = self.c["ac_armor"][i]
            note = []
            if i in (3, 4, 7, 8, 9, 10, 11):
                note.append("Stops Hide In Shadows, Move Silently, Climb Walls.")
            if 5 <= i <= 11:
                note.append("Magic armor: rings of protection and the Cloak of Protection do not count.")
            elif r.has("shield"):
                note.append("The Cloak of Protection does not count.")
            if r.note():
                note.append(r.note())
            rows.append(self.icon_row([r.name(), "shield" if r.has("shield") else "body", signed(-ac) if ac else "0",
                                       str(10 + ac), classes_of(r), r.num("value", 0), " ".join(note)], r))
        for i in (8, 9):
            r = self.O[i]
            note = "Boot place." if i == 8 else "Gauntlet place. Find/Remove Traps and Open Locks become 0."
            # 0xAFBC8: the boot place counts only an armor line (the magic
            # boots, armor class 0), so the -1 of the misc table for the
            # Boots never counts (docs/audit/gear.md)
            ac = 0 if i == 8 else self.c["ac_misc"][i]
            if i == 8:
                note = "Boot place. No armor class: the boot place counts only magic boots."
            rows.append(self.icon_row([r.name(), "boots" if i == 8 else "gauntlets", signed(-ac) if ac else "0",
                                       str(10 + ac), classes_of(r), r.num("value", 0), note], r))
        rows += self.gear_rows_armor()      # ASCENT-DTU: the new gear
        self.count("Armor", len(rows))
        return "\n".join([
            "# Armor",
            "",
            "- Armor class starts at 10. Each worn item lowers it by its AC bonus. Then come the "
            "spell limits (Armor 6, Shield 4 against melee and 2 against missiles, Magical "
            "Vestment), Slow +4, the Dexterity adjustment "
            "and Invulnerability -2. The result stays within -10 to 10.",
            "- AC alone: your armor class with only this item and Dexterity 7 to 14.",
            "- Mage spells: a mage cannot cast a mage spell while wearing any item on this page except "
            "boots and gauntlets.",
            "- F/T/M/C: the classes that may wear it ([[Classes]]).",
            "",
            "Rings, amulets, cloaks, boots, bracers, gloves and girdles are on [[Magic Items|Magic-Items]].",
            "",
            md_table(self.icon_head(["Armor", "Place", "AC bonus", "AC alone", "F/T/M/C", "Value", "Notes"]), rows),
            "",
            "There is no helmet: the paper doll has no place for one.",
            "",
            "## Differs from the manual",
            "",
            "- The manual says only fighters and priests can use shields. Thieves can use them too.",
            "- The manual lists helms, leather gauntlets and metal gauntlets. The game has one item "
            "\"Gauntlets\" (fighter and cleric, AC bonus 1) and no helm.",
            "",
            pages_note(),
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
               "The game shows no description of an item. This page says what each one does. A worn "
               "item works while you wear it and stops when you take it off.", "",
               "- AC bonus: how much the item lowers your armor class.",
               "- F/T/M/C: the classes that may wear it ([[Classes]]).",
               "- Value: the base price in gold.", ""]
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
                    eff = ("Worn in the armor place, so not with body armor. A base armor class like body "
                           "armor: rings and cloaks of protection count with them.")
                if title == "Rings" and i in (0, 14, 15):
                    eff = ("Only the highest magic protection bonus counts: of the magic of the armor, the "
                           "best of these rings and a Cloak or Amulet of Protection. Of two of these rings only the "
                           "better counts. The bonus also makes the saving throws better (the best "
                           "protection item).")
                if title == "Amulets" and 28 <= i <= 32:
                    eff = ("Only the highest magic protection bonus counts: of the magic of the armor, "
                           "this amulet and a Ring or Cloak of Protection. The bonus also makes the "
                           "saving throws better (the best protection item).")
                if i == 33:
                    ac = 0
                eff = " ".join(x for x in (eff or "AC only.", r.note()) if x)
                rows.append(self.icon_row([r.name(), signed(-ac) if ac else "0", classes_of(r), r.num("value", 0), eff], r))
            if title != "Bracers":
                rows += self.gear_rows_magic(flags)      # ASCENT-DTU: the new gear
            if title == "Amulets":
                rows.append(self.icon_row([self.O[5].name(), "0", classes_of(self.O[5]), "-",
                                           "Quest item. No other effect."], self.O[5]))
            if title == "Cloaks":
                rows.append(self.icon_row([self.O[6].name(), "0", classes_of(self.O[6]), self.O[6].num("value", 0),
                                           "A plain cloak. No effect."], self.O[6]))
            if title == "Girdles":
                rows.append(self.icon_row([self.O[7].name(), "0", classes_of(self.O[7]), self.O[7].num("value", 0),
                                           "A plain belt. No effect."], self.O[7]))
            total += len(rows)
            out += ["## " + title, "", md_table(self.icon_head(["Item", "AC bonus", "F/T/M/C", "Value", "Effect"]), rows), ""]
            if title == "Rings":
                out += ["You can wear two rings.", ""]
        wrows = []
        for i in (13, 54, 69, 70, 71, 45):
            r = self.W[i]
            wrows.append(self.icon_row([r.name(), WEAPON_NOTES.get(i, "")], r))
        r = self.O[100]
        wrows.append(self.icon_row([r.name(), "A bow with +2 to hit and damage. " + SPEED_BOW_NOTE], r))
        out += ["## Weapons with powers", "", "Damage and classes: [[Weapons]].", "",
                md_table(self.icon_head(["Weapon", "Power"]), wrows), ""]
        orows = [
            self.icon_row([self.O[2].name(), "USE: cures your blindness; if you are not blind but hurt, Cure Serious "
                                             "Wounds (3 to 17 hit points). Not used up. You get one use at the start "
                                             "and one more for each new mission from Khelben; at other times: \"You "
                                             "cannot use the Gem Of Curing at this time.\""], self.O[2]),
            self.icon_row([self.O[101].name(), "USE: it asks you questions and gives answers, a script of the quest."],
                          self.O[101]),
            self.icon_row([self.O[0].name(), "Does nothing: it gives no light and has no use."], self.O[0]),
            self.icon_row([self.O[10].name() + ", " + self.O[13].name() + ", " + self.O[11].name() + ", " + self.O[12].name(),
                           "In the lockpick place, for a thief: Open Locks +5, +10, +15, +20."],
                          self.O[10], self.O[13], self.O[11], self.O[12]),
        ]
        sp = ", ".join(self.S[s].name() for s in self.c["spider_spells"])
        # The inventory icon of each piece ($OBJECT icon= of the lines 18 to
        # 26, FEBGND entries 23 and 24), described in words; USE from the
        # cases 0x12 to 0x1A of 0x37A6A (docs/audit/gear.md); the statue of
        # FLAME takes 18, 19, 24, 26, 22, 23, 20 (docs/re/quests.md).
        statue = "Yes, the statue of the last level takes it."
        spider = [
            (18, "Two black legs, bent at the lower right, reaching up to the left.",
             "Turn undead as a cleric of level 2 x your first class level + 1.", statue),
            (19, "Two black legs, bent at the lower left, reaching up to the right. The mirror "
                 "image of the piece above.",
             "You know the whole map of the level.", statue),
            (20, "A red-brown hourglass: the waist of the spider.",
             "Each person in sight (human, drow and the like) gets one of three at random: death "
             "unless it saves, a random spell (%s), or it spins in place unless it saves (how the "
             "spin ends is not known)." % sp, statue),
            (21, "An empty square of sand, with no drawing on it.",
             "Nothing happens: this piece has no use.",
             "No. It lies in the last level, and the statue does not take it."),
            (22, "Two black legs that start at the upper right and hang down to the lower left.",
             "Fear around you, and Aid, Magical Vestment, Fly, Protection From Fire, Resist Fire, "
             "Flame Walk and Strength on you.", statue),
            (23, "Two black legs that start at the upper left and curve down to the lower right, "
                 "like a sickle. Almost the mirror image of the piece above.",
             "The view visits each creature of the level in turn, for 1 to 3 seconds each.", statue),
            (24, "The left half of the black body, with a pale hourglass mark. The cut edge is at "
                 "the right.",
             "A quest script runs.", statue),
            (26, "The right half of the black body, with a pale hourglass mark. The cut edge is at "
                 "the left: the mirror image of the piece above.",
             "All your hit points, and Aid, Shield, Feather Fall, Improved Invisibility, Magical "
             "Vestment, Prayer, Protection From Evil, Resist Fire and Stone Skin on you; then Cure "
             "Blindness or Deafness if you need it.", statue),
            (25, "The whole black spider with eight legs.",
             "Spider Climb, Free Action, Haste and Infravision with no end.",
             "No. It lies behind the wall that the statue opens, and later triggers of the level "
             "need it."),
        ]
        # With icons the picture is in the icon column; without, its number
        # stays in the text
        srows = [self.icon_row([self.O[i].name(), pic if self.icons else "%s (icon %s)" % (pic, self.O[i].get("icon", "?")),
                                use, st], self.O[i])
                 for i, pic, use, st in spider]
        total += len(wrows) + len(orows) + len(srows)
        out += ["## Other items with a use", "",
                md_table(self.icon_head(["Item", "Effect"]), orows), "",
                "## Spider Pieces", "",
                "The pieces of Lolth's spider are quest items for every class. Eight of them have the "
                "name Spider Piece, but each has its own picture in the item list, and its own picture "
                "on the floor too. The pictures are the parts of a black spider on sand. Three pairs "
                "are mirror images: look at which side the legs or the cut edge of the body are on. "
                "Each use takes one charge; with no charge left, \"Nothing happens...\".", "",
                md_table(self.icon_head(["Item", "Picture", "USE", "Taken by the statue"]), srows), "",
                "The statue of the last level takes seven pieces and then opens a wall; the "
                "Assembled Spider lies behind it.", ""]
        out += ["Potions, wands and scrolls have their own pages: [[Potions]], [[Wands]], [[Scrolls]].", "",
                "## Notes", "",
                "- Quest items cannot be dropped (\"You can't drop that.\"): the keys, the Spider Pieces, "
                "the Spider Amulet, the Cursed Scroll and the other items of the quests.",
                "- A stack that does not fit in your inventory: you take what fits, and the rest stays "
                "on the floor. The original 1.3 game loses the rest; ASCENT corrects this.",
                "- The 1.3 readme lists new items: Potion of Invisibility, Ring of Wizardry, Cloak of "
                "Lathandar, Drowslayer and +3 Longsword. ASCENT spells the name of the god Lathander.",
                "- ASCENT corrects these item names (the option -origitems gives the names of 1.3): "
                + "; ".join("\"%s\" is %s" % (n["name13"], n["name"]) for n in self.name_fixes) + ".",
                "- The game does not identify items: every item shows its real name.",
                "- Some items share a picture in the item list, and only the name tells them apart: "
                "the %s and the %s; the %s, the %s and the %s; the %s and the %s."
                % (self.O[101].name(), self.O[16].name(), self.O[2].name(), self.O[103].name(),
                   self.O[105].name(), self.O[125].name(), self.O[107].name()), "",
                "## Rulebook items", "",
                "The setting \"Rulebook items\" of the Improvements menu gives these items the values of "
                "the AD&D 2nd edition rulebooks. It is off by default, because it changes the balance of "
                "the game. The tables of the pages show the values of 1.3. PHB: Player's Handbook. DMG: "
                "Dungeon Master Guide.", "",
                md_table(["Item", "1.3", "Rulebook items", "Source"], self.rule_rows), "",
                pages_note(), ""]
        self.count("Magic-Items", total + len(self.rule_rows))
        return "\n".join(out)

    # -- Potions --------------------------------------------------------
    def potions(self):
        rows = []
        for i in range(72, 89):
            r = self.O[i]
            eff, _ = POTIONS[i]
            rows.append(self.icon_row([r.name(), " ".join(x for x in (eff, r.note()) if x), r.num("value", 0)], r))
        self.count("Potions", len(rows))
        return "\n".join([
            "# Potions",
            "",
            "Drink a potion with the USE button or a hot key. Each use takes one potion. A new potion "
            "of a kind that is running keeps the later end. The Potion of Anti-Magic and Dispel Magic "
            "end all potion effects. A potion that casts a spell (Flying, Jumping) casts it at your "
            "caster level ([[Wizard Spells|Wizard-Spells]]).",
            "",
            md_table(self.icon_head(["Potion", "Effect", "Value"]), rows),
            "",
            pages_note(),
            "",
        ])

    # -- Wands ----------------------------------------------------------
    def wands(self):
        rows = []
        for i, r in enumerate(self.W):
            sp = r.get("pc_spell")
            if not sp or i in (69, 79):
                continue
            s = int(sp)
            note = ""
            if s == 12:
                note = "Cast on you."
            note = " ".join(x for x in (note, r.note()) if x)
            rows.append(self.icon_row([r.name(), self.S[s].name(), r.num("uses", 1), classes_of(r), value_of(r), note], r))
        # ASCENT-DTU: the new gear gives five lines of the game files a spell
        for i, (r, s) in sorted(self.gear.wands.items()):
            rows.append(self.icon_row([r.name(), self.S[s].name(), r.num("uses", 1), classes_of(r), value_of(r),
                                       NEW_GEAR_LINK], r))
        self.count("Wands", len(rows))
        return "\n".join([
            "# Wands",
            "",
            "An attack with a wand in the hand casts its spell at once, with no cast time and no "
            "spell slot. Each cast takes one charge, and at 0 the wand is gone. The Charges column is "
            "the charge count of a new wand; a wand placed in a dungeon level has its own count "
            "(the levels give 10 to 56).",
            "",
            "Caster level: for a mage spell the wand uses your level in your first class that is not "
            "a cleric; a fighter of level 8 casts Magic Missile at level 8. For a cleric spell it uses "
            "your cleric level. If you have no such class, it casts at level 1.",
            "",
            md_table(self.icon_head(["Wand", "Spell", "Charges", "F/T/M/C", "Value", "Notes"]), rows),
            "",
            "The spell rules, also the damage: [[Wizard Spells|Wizard-Spells]].",
            "",
            pages_note(),
            "",
        ])

    # -- Scrolls --------------------------------------------------------
    def scrolls(self):
        rows = []
        for i in (102, 106, 126, 127):
            rows.append(self.icon_row([self.O[i].name().strip(), " ".join(x for x in (SCROLLS[i], self.O[i].note()) if x)],
                                      self.O[i]))
        self.count("Scrolls", len(rows))
        return "\n".join([
            "# Scrolls",
            "",
            "## Spell scrolls",
            "",
            "A spell in the world is an item. When you pick it up, a mage learns a mage spell and a "
            "cleric learns a cleric spell; it goes into the spell book. A spell goes only into the book "
            "of the class that can cast it: Protection From Fire, a cleric spell that also has the mark "
            "of a mage, goes into the book of a cleric, and a mage keeps it as a scroll. (The original "
            "1.3 game puts it into the mage book, where it cannot be cast; ASCENT corrects this.) A "
            "spell that none of your books takes stays in the item list. With it in the hand, an attack "
            "casts it as a scroll: no cast time, no spell slot, and the scroll is used up. The caster "
            "level is that of a wand ([[Wands]]).",
            "",
            "The market buys the scrolls of the item list: the spells that you cannot learn and the "
            "spare copies of spells that you know. A scroll that you buy and that goes into a book "
            "leaves no line in your sale list.",
            "",
            "Only a mage or a cleric can put a spell item in a hot key box, and its class must fit. "
            "Protection From Evil and Protection From Fire are the exceptions: anyone can use them.",
            "",
            "## Other scrolls",
            "",
            md_table(self.icon_head(["Scroll", "Effect"]), rows),
            "",
            "A quest item cannot be dropped.",
            "",
            pages_note(),
            "",
        ])

    # -- Spells ---------------------------------------------------------
    def spell_range(self, s):
        """The flight of the missile when it is shorter than the usual
        1500 ft, or None."""
        if self.spell_self(s):
            return None
        if s == 32:
            return "%d ft" % (MISSILE_FT_PER_S * 2)
        if s == 36:
            return "%d ft + %d ft per 3 levels" % (MISSILE_FT_PER_S * 60, MISSILE_FT_PER_S * 60)
        return None

    def spell_radius(self, s):
        # 0x90CEF: radius + L x level_radius, in units (2 units are a foot).
        # The dwords read at Spell_info+6 and +0xA, shifted by 16, are the
        # words radius (+8, default 20 of the reader 0x242F6) and
        # level_radius (+0xC).
        r = self.spell_rec(s)
        text = "%s ft" % feet(r.num("radius", 20))
        lr = r.num("level_radius", 0)
        if lr:
            text += " + %s ft per level" % feet(lr)
        return text

    def spell_effect(self, s):
        if s in PORT_SPELLS:
            e = "**New in ASCENT-DTU.** " + PORT_SPELLS[s]["eff"] + " " + PORT_SPELL_END
            return e
        e = SPELLS[s]["eff"]
        if e == "DETECT_MAGIC":
            return DETECT_MAGIC
        if e == "VESTMENT":
            return vestment_text()
        d = self.casters["d"]
        return (e.replace("DISPEL_PEACE", DISPEL_PEACE).replace("HELD_HIT", HELD_HIT)
                .replace("CASTER_HOLD_MAX", "%g" % d["CASTER_HOLD_MAX"])
                .replace("CASTER_HOLD_REST", "%g" % d["CASTER_HOLD_REST"]))

    def spell_page(self, cls):
        flag = "mage_use" if cls == "m" else "cleric_use"
        title = "Wizard spells" if cls == "m" else "Priest spells"
        # 1.3 casts a spell with both class bits as a cleric spell (0x35683)
        nums = [i for i, r in enumerate(self.S) if r.has(flag) and i not in OTHER_SPELL_LINES
                and not (cls == "m" and r.has("cleric_use"))]
        # the spells of the port with the class word (Mirror Image, Minor
        # Globe of Invulnerability, Spiritual Hammer)
        new = [i for i, r in sorted(self.port_spells.items()) if r.has(flag)]
        for i in new:
            if i not in PORT_SPELLS:
                raise SystemExit("dtuspell.c: spell line %d %r has no rule text (PORT_SPELLS)"
                                 % (i, self.port_spells[i].name()))
        nums += new
        by_level = collections.defaultdict(list)
        for i in nums:
            by_level[self.spell_rec(i).num("level", 1)].append(i)
        out = ["# " + title, "",
               ("Mage spells (the manual says wizard)." if cls == "m" else "Cleric spells (the manual says priest).")
               + " Each row has all the rules of one spell. \"Level\" in a row means your %s level."
               % ("mage" if cls == "m" else "cleric"), "",
               "- Cast: the cast time in game seconds (the AD&D casting time: one second for each "
               "segment, 60 for a round, 600 for a turn). The game sets it for each spell; the "
               "cast times of the spell table files are not used. A hit on you while you cast stops "
               "the spell, and the slot is lost. A scroll or a wand casts at once.",
               "- Target: \"You\" means the spell acts on you. The other spells fire a missile that "
               "flies 50 ft per second until it hits something, for at most 30 seconds (1500 ft) "
               "unless the row says less. The spell acts on what the missile hits.",
               "- Area: an area spell acts on each creature and player within the distance of its "
               "center (you, or the place where the missile lands) that the center can see. "
               "\"Everything\" in a row: also items on the floor, scenery and spell missiles.",
               "- Save: the target's saving throw against spells ([[Classes]]). Half: half damage. "
               "Negates: no effect.",
               "- Damage to creatures is reduced by their immunities, and damage to you by Resist "
               "Fire, Flame Walk and Protection From Fire.",
               ""]
        rows_total = 0
        for lvl in sorted(by_level):
            first = self.first_level(cls, lvl)
            out += ["## Level %d" % lvl, ""]
            if first:
                out += ["First slot at class level %d." % first, ""]
            rows = []
            spells = sorted(by_level[lvl], key=lambda s: (self.spell_name(s), s))
            for s in spells:
                r = self.spell_rec(s)
                rule = PORT_SPELLS[s] if s in PORT_SPELLS else SPELLS[s]
                ct = self.port_cast[s] if s in self.port_cast else self.c["cast_time"][s]
                target = rule["target"]
                # The area of the flags after the table fixup (dtutblfix.c)
                if self.spell_area(s) and (target in ("You", "The creature the missile hits",
                                                      "The target the missile hits")
                                           or target.startswith(("Everyone within", "Everything within"))):
                    target = ("You and everyone within {r} ft of you" if self.spell_self(s)
                              else "Everyone within {r} ft of where the missile lands")
                    if r.has("hit_objects"):
                        target = target.replace("everyone", "everything").replace("Everyone", "Everything")
                target = target.replace("{r} ft", self.spell_radius(s))
                rng = self.spell_range(s)
                if rng:
                    target += " (the missile flies %s)" % rng
                rows.append(self.icon_row([self.spell_name(s), cast_text(ct), target, rule["dur"], rule["save"],
                                           self.spell_effect(s)], r))
            rows_total += len(rows)
            out += [md_table(self.icon_head(["Spell", "Cast", "Target", "Duration", "Save", "Effect"]), rows), ""]
        out += self.new_spells_where(cls, new)
        out += ["## Differs from the manual", ""]
        out += self.spell_manual_notes(cls)
        out += ["", pages_note(), ""]
        self.count("Wizard-Spells" if cls == "m" else "Priest-Spells", rows_total)
        return "\n".join(out)

    def spell_name(self, s):
        return self.spell_rec(s).name()

    def new_spells_where(self, cls, new):
        """Where the player gets the spells of the port of the class."""
        if not new:
            return []
        names = " and ".join(self.spell_name(s) for s in new)
        out = ["## New spells of ASCENT-DTU", "",
               "%s %s new in ASCENT-DTU: the original game, version 1.3, does not have %s." % (
                   names, "are" if len(new) > 1 else "is", "them" if len(new) > 1 else "it"), ""]
        if cls == "m":
            lv = dict((s, self.spell_rec(s).num("level", 1)) for s in new)
            out += ["With the setting \"%s\" of the Improvements menu (on by default):" % CASTER_AI_SETTING, "",
                    "- Aleena Paladinstar in the Yawning Portal sells each with her scroll sets: %s." % "; ".join(
                        "%s with the spells of level %d (%s gold)" % (self.spell_name(s), lv[s], ALEENA_PRICE[s])
                        for s in new),
                    "- Scrolls of them lie in the dungeon, each in place of a scroll that the original game "
                    "has more than once (spoilers): %s. A level that your saved game has visited "
                    "already keeps its old scroll." % "; ".join(
                        "%s in %s" % (self.spell_name(s), " and ".join(
                            "%s (for %s)" % (LEVEL_NAMES[lvl], old) for lvl, old in self.port_scrolls.get(s, ())))
                        for s in new),
                    "- The creature casters of Caster AI cast them too ([[Monster-Behavior]]).", ""]
        else:
            out += ["A cleric gets %s with the other cleric spells of its level, at the level up that "
                    "gives the first slot of that level; a saved game of a cleric who has that level gets it "
                    "at the next level start. No scroll of it lies in the dungeon. The drow priestesses of "
                    "Caster AI cast it too ([[Monster-Behavior]])." % names, ""]
        return out

    def spell_manual_notes(self, cls):
        if cls == "m":
            return [
                "- Chill Touch acts on you (the manual: a touch attack).",
                "- The manual lists Flame Arrow (level 3). There is no such spell. The Flame Arrow is a "
                "magic missile weapon (1d6 + 4d6 fire, mage only, [[Weapons]]).",
                "- Fireball: the 1.3 readme says that a Fireball that bursts close may blind or deafen. "
                "In the game only the fire trap of the Yawning Portal does that: within 3 ft, it blinds "
                "you for 1 second to 30 minutes even if you make your save (the save only stops the "
                "message and the flash), and a failed save against petrification deafens you until "
                "cured.",
                "- Magic Missile: the manual gives 2 missiles at level 3; the game at level 2.",
                "- Melf's Minute Meteors: the manual says one per level; the game gives at most 5.",
                "- Sleep: the manual says level 5 or more is safe; the game spares level 4 and up.",
                "- Scare: the manual says level 6 or more is safe; the game affects level 6 and lower.",
                "- Stone Skin: the manual says 1 to 4 attacks; the game 1d4 + 1 per 2 levels.",
                "- Haste: the manual says faster attacks. The original 1.3 game makes them slower; "
                "ASCENT corrects this.",
                "- Fly: the manual says it lingers a random time; the game has a random duration and no "
                "extra time.",
            ]
        return [
            "- Prayer: the manual adds penalties to enemies and bonuses to damage and saves. The game "
            "gives only Bless (+1 to hit) to you.",
            "- Resist Cold is not in the game: its line is Cure Blindness or Deafness, a new spell of "
            "the 1.3 readme.",
            "- Flame Walk halves fire damage; the manual says it stops normal fire.",
            "- Magical Stone: the manual says three stones; the game 1d3.",
            "- Magical Vestment: the manual gives the armor class of chain mail (5); the game gives 6 "
            "at levels 1 and 2, 5 up to level 7, and better at higher levels.",
            "- The manual lists Silence, 15' Radius (level 2). There is no such spell.",
            "- Produce Fire (a new spell of the 1.3 readme) is a level 4 spell that bursts like a small "
            "Fireball; the manual gives a fire in the hand.",
            "- Protection From Fire also has the mark of a mage. In the original 1.3 game a mage learns "
            "it from a scroll but cannot cast it; in ASCENT a mage keeps it as a scroll ([[Scrolls]]).",
        ]

    # -- Monster behavior (Improved AI, Caster AI) -----------------------
    def caster_rows(self):
        """The rows of the caster kinds, from the lists of dtucaster.h."""
        ca = self.casters
        f = ca["flags"]
        levels = collections.defaultdict(list)
        for name, lst, lvl, fl in ca["types"]:
            levels[lst].append(lvl)
        rows = []
        for lst, entries in ca["lists"].items():
            if lst not in CASTER_KINDS:
                raise SystemExit("dtucaster.h: the list %s has no text (CASTER_KINDS)" % lst)
            buff, other = [], []
            for spell, min_level, weight, fl in entries:
                text = self.spell_name(spell)
                notes = []
                if min_level > 1:
                    notes.append("from caster level %d" % min_level)
                if fl & f["CS_CLOSE"]:
                    notes.append("when you are in its reach")
                if fl & f["CS_HEAL"]:
                    notes.append("on itself below half of its hit points")
                if fl & f["CS_FREE"]:
                    notes.append("a power: no spell points, every %g seconds" % ca["d"]["CAI_POWER_WAIT"])
                if fl & f["CS_SELF"]:
                    notes.append("on itself, then it throws the hammer")
                if notes:
                    text += " (%s)" % ", ".join(notes)
                (buff if fl & f["CS_BUFF"] else other).append(text)
            lv = levels.get(lst) or [0]
            lvt = ("%d" % lv[0]) if min(lv) == max(lv) else ("%d to %d" % (min(lv), max(lv)))
            rows.append([CASTER_KINDS[lst], lvt, ", ".join(buff) or "none", ", ".join(other)])
        return rows

    def monster_behavior(self):
        ai, ca = self.ai, self.casters
        d = ca["d"]
        rows = self.caster_rows()
        out = ["# Monster behavior (ASCENT-DTU)", "",
               "Two settings of the Improvements menu change how the creatures act: \"%s\" and \"%s\". "
               "Both are on by default. With both off the creatures act as in the original game, version "
               "1.3. This page names no places." % (IMPROVED_AI_SETTING, CASTER_AI_SETTING), "",
               "## %s" % IMPROVED_AI_SETTING, "",
               "In the original game a creature that fights hand to hand notices you through a closed door "
               "or a wall when you are near, every creature that can open doors walks to the doors, also a "
               "peaceful one, and a sword or an arrow reaches through a closed door. %s changes this:"
               % IMPROVED_AI_SETTING, "",
               "- **Sight.** A creature sees you along a straight line from its eyes with no wall and no "
               "closed door in the way. An illusory wall does not stop the sight. A creature that faces away "
               "from you notices you only now and then.",
               "- **Hearing.** A creature that fights hand to hand and casts no spells also hears you along a "
               "way that a body can pass (open doors, no walls) of at most %s ft." % feet(ai["DTU_HEAR_DIST"]),
               "- **In a fight.** A creature that noticed you (by sight, hearing, a hit, an alarm or a "
               "script) in the last %g seconds fights. It follows you through a door that closes behind you, "
               "and gives up %g seconds after it noticed you last." % (ai["DTU_COMBAT_TIME"], ai["DTU_COMBAT_TIME"]),
               "- **Alarms.** A call for help wakes a creature only along an open way of at most %s ft, and "
               "only when that creature sees or hears you itself. A closed door stops an alarm."
               % feet(ai["DTU_ALARM_DIST"]),
               "- **Doors.** Only a creature that can open doors (the humanoids: orcs, goblins, kobolds, "
               "ogres, people, liches) opens one, and only while it fights you or flees. No creature opens a "
               "door with a lock or a key, so a prisoner stays in its cell. A creature on a walk of the story "
               "still opens the doors on its way.",
               "- **Walls stop blows.** A blow, an arrow, a bolt or a thrown weapon does not pass a wall or a "
               "closed door: yours do not, and theirs do not.",
               "- **Melee.** A creature that fights hand to hand walks straight at you, stops when you are in "
               "the reach of its weapon, turns to you and attacks at once. In the original game it sidles "
               "around you and may swing with its back to you.",
               "- **Groups.** A creature that a talk would make hostile (a guard, a member of a band) joins "
               "the fight when it sees you and sees a creature of its group fight you (within %s ft, checked "
               "each %g seconds). Its group is every hostile creature when its talk starts with the attack, "
               "else the creatures of its own kind that also talk. A shop keeper, a quest giver or an ally "
               "never joins." % (feet(ai["DTU_JOIN_DIST"]), ai["DTU_JOIN_PERIOD"]),
               "- **Spiders.** Giant and large spiders run faster, at the web speeds of AD&D 2nd edition.",
               "- **Paths.** Creatures go around corners, through doorways and up ladders better, slide "
               "along walls and walk around furniture.", "",
               "## %s" % CASTER_AI_SETTING, "",
               "In the original game every spellcaster picks from one list that favors light spells and "
               "spells on itself, stands while it casts, has few spell points, and rarely attacks. With %s:"
               % CASTER_AI_SETTING, "",
               "- **Its own spells.** Each kind of caster has a list of AD&D 2nd edition spells and a caster "
               "level (the table below). A caster has a spell of spell level s from caster level 2 s - 1 on, "
               "unless the table says another level.",
               "- **Protections at once.** When the fight starts (the first time it notices you) the caster "
               "gets the protections of its list at once, with no cast time and no spell points: a "
               "contingency. Dispel Magic takes them away.",
               "- **Spell points.** At least %d for each caster level. They do not come back during a "
               "fight; a fight ends when the caster has not been in a fight for %g seconds, and a new fight "
               "has all its points again." % (d["CAI_POOL_LEVEL"], d["CAI_FIGHT_END"]),
               "- **One cast each round.** A choice each %g seconds, then the cast time of the spell (at most "
               "%d seconds). A hit that hurts the caster while it casts ruins the spell; it tries again %g "
               "seconds later. A caster needs a free line to you: behind a closed door it casts nothing and "
               "gets no protections." % (d["CAI_ROUND"], d["CAI_CAST_MAX"], d["CAI_INTERRUPT"]),
               "- **A sensible choice.** It does not cast a spell that cannot reach you, a missile that a "
               "wall or a creature would stop, Burning Hands out of reach, a Fireball with itself or an ally "
               "in the blast, Magic Missile against Shield, Hold Person against Free Action, Protection From "
               "Evil or a hold, the same Hold Person within %g seconds of its last one, Slow or Fear on you "
               "twice, or a spell that your Minor Globe of Invulnerability stops. Fire spells are less "
               "likely against Resist Fire and Protection From Fire. Damage is more likely while you are "
               "held, and spells that your mirror images do not fool are more likely while you have them. "
               "If your globe stops all its spells, it fights hand to hand." % d["CAI_HOLD_WAIT"],
               "- **Allies.** Its harmful spells do not act on the creatures of its side, and its missiles "
               "fly past them. Fireball and Burning Hands still burn them.",
               "- **Distance.** A caster with spells backs away when you come nearer than %s ft, walks to "
               "you when you are farther than %s ft, and holds its place between. With no points left it "
               "walks up and fights hand to hand. Between two casts it strikes when you are in its reach. "
               "Mind flayers always walk at you; beholders float as before." % (
                   feet(d["DTU_CASTER_KEEP_NEAR"]), feet(d["DTU_CASTER_KEEP_FAR"])),
               "- **Holds.** A hold that a creature casts on you lasts %g seconds at most, and no creature "
               "holds you again until %g seconds after a hold ends. Each attack on a held target hits with no "
               "roll." % (d["CASTER_HOLD_MAX"], d["CASTER_HOLD_REST"]),
               "- **Protection From Evil** on a creature gives -2 to your attack rolls against it and +2 to "
               "its saves against your spells.",
               "- **Spiritual Hammer.** A priestess casts it once on herself, then throws the hammer at you "
               "between her casts, at most one throw each %g seconds." % d["CAI_THROW"],
               "- **Mirror Image** on a creature shows as copies of it at its sides (four at most).",
               "- The dragon hatchlings keep the spells of the original game, and the dragon Tamaramagil "
               "fights in her own way ([[Encounters]], spoilers).", "",
               md_table(["Caster", "Caster level", "Protections at the start of a fight", "Spells"], rows), "",
               "### Spells that only creatures cast", ""]
        fs, mb = self.port_spells.get(59), self.port_spells.get(60)
        if fs is None or mb is None:
            raise SystemExit("dtuspell.c: no spell lines 59 and 60 (Flame Strike, Mind Blast)")
        out += ["- **%s** (priest spell of level %d, cast time %d seconds; a creature casts it in %d): a "
                "column of fire on you, %dd%d fire damage, a save for half. Resist Fire halves it, Protection "
                "From Fire takes it from its pool, and the fire protection of items works as against any "
                "fire." % (fs.name(), fs.num("level", 5), self.port_cast[59], min(self.port_cast[59], d["CAI_CAST_MAX"]),
                           fs.num("#fire_damage_dice", 6), fs.num("fire_damage_dice", 8)),
                "- **%s** (the mind flayers): a cone %s ft long and about 21 degrees to each side. Each "
                "creature of the other side in it saves against wands at -4 or is stunned for 3d4 seconds, as "
                "held. Free Action does not prevent it, a Potion of Freedom ends it, and Protection From Evil "
                "gives +2 to the save. It is a power, not a spell: no points, once each %g seconds, and the "
                "Minor Globe of Invulnerability does not stop it." % (mb.name(), feet(mb.num("range", 120)),
                                                                       d["CAI_POWER_WAIT"]), "",
                pages_note(), ""]
        self.count("Monster-Behavior", len(rows))
        return "\n".join(out)

    # -- Encounters (the dragon, the lich) ------------------------------
    def item_names(self, items):
        """Names with counts: [(class 5/15/16, number, count)] -> "a, b and 3 Diamonds"."""
        parts = []
        for c, i, n in items:
            if c == 16 and i == 1:
                parts.append("%d gold" % n)
                continue
            name = self.gear.item(c, i).name() if self.gear else self.t[{5: "$WEAPON", 15: "$ARMOR", 16: "$OBJECT"}[c]][i].name()
            parts.append(name if n == 1 else "%d x %s" % (n, name))
        return ", ".join(parts[:-1]) + " and " + parts[-1] if len(parts) > 1 else "".join(parts)

    def encounters(self):
        dd = self.dragon["d"]
        gd = self.gear
        tr = list(self.dragon["treasure"])
        gold = sum(n for c, i, n in tr if c == 16 and i == 1)
        tr = [x for x in tr if not (x[0] == 16 and x[1] == 1)]
        diamonds = sum(n for c, i, n in tr if (c, i) == (16, 3)) + 3
        tr13 = [x for x in DRAGON_HOARD_13 if (x[0], x[1]) != (16, 3)]
        tr = [x for x in tr if (x[0], x[1]) != (16, 3)]
        lich_list = self.casters["lists"]["CL_LICH"]
        lich_level = dict((t, lv) for t, _, lv, _ in self.casters["types"]).get("lich3", 12)
        lim = [(s, lv) for t, s, lv in self.casters["limits"] if t == "lich3"]
        dmg_level = lim[0][1] if lim else lich_level
        missiles = 1 + sum(1 for x in (2, 4, 6, 8) if dmg_level >= x)
        wards = [self.spell_name(s) for s, _, _, fl in lich_list if fl & self.casters["flags"]["CS_BUFF"]]
        attacks = [self.spell_name(s) for s, _, _, fl in lich_list if not fl & self.casters["flags"]["CS_BUFF"]]
        rname, rspell, rlevel, rvalue, rlvl = gd.g["rod"]
        script, potion, mace = gd.g["daland"]
        breath_avg = dd["DR_BREATH_DICE"] * (dd["DR_BREATH_SIDES"] + 1) / 2 + dd["DR_BREATH_BONUS"]
        out = ["# Encounters (ASCENT-DTU)", "",
               "> **Spoiler warning.** This page tells how two fights of the middle of the game go. Each "
               "section starts with a short summary; the details are hidden under a second warning.", "",
               "Both encounters are new in ASCENT-DTU. Each is a setting of the group \"Restored and new "
               "content\" of the Improvements menu, on by default, and takes effect at the next load of its "
               "level.", "",
               "## The dragon Tamaramagil", "",
               "Setting: \"The dragon Tamaramagil\". The files of the original game have a red dragon for "
               "%s, the second drow level, with her dialogue, her hatchlings and her hoard, but no level "
               "places her. ASCENT puts her in her hall." % LEVEL_NAMES[21], "",
               "In short: she talks before she fights, and she asks a favor. The fight is optional and hard: "
               "her fire breath can kill a character of the middle of the game in one go, but it comes with a "
               "warning and can be dodged. Her hoard holds some of the best items of the game.", "",
               "<details><summary>Show the details of the encounter (heavier spoilers)</summary>", "",
               "### How it goes", "",
               "- She rests in the middle of her hall and asks you to pull a lever in the corridor that you "
               "came from, to free her children, for the key of her hoard.",
               "- The lever is behind a locked door in an alcove at the end of that corridor. No thief skill "
               "and no spell opens that door: its key, the Jailer's Key, lies in the cells behind her hall.",
               "- The lever was no cage: it opens her hatchery. She laughs at you, turns hostile and keeps "
               "her key. Her hatchlings fly into the hall.",
               "- The hatchlings come %d at a time while she lives; the next one comes when one dies, and all "
               "of them when she is dead. A waiting hatchling joins at once when it is hurt or when you come "
               "within %s ft of it, so the one who runs into the hatchery fights them all. A hatchling has "
               "about 50 hit points, armor class 0 and THAC0 7, gives 7000 experience, flies, and casts mage "
               "spells: Fireball (6d6), Lightning Bolt, Magic Missile, Melf's Acid Arrows, Hold Person, Fear "
               "and others." % (dd["DR_HATCH_AT_ONCE"], feet(dd["DR_HATCH_NEAR"])),
               "- An attack on her starts the fight too, and so does a hatchling that is hurt in the "
               "hatchery: the hatchery opens as by the lever.",
               "- Her hoard is behind the locked door of the west side of her hall. The Dragon Key drops "
               "in front of her when she dies, and a thief can pick the lock. If the door opens while she "
               "is at peace and sees it, she calls you a thief and attacks.",
               "",
               "### Her values", "",
               "- A red dragon of AD&D 2nd edition: %d hit points, armor class %d, THAC0 %d, %d experience."
               % (dd["DR_HIT_POINTS"], dd["DR_ARMOR_CLASS"], dd["DR_THAC0"], dd["DR_EXPERIENCE"]),
               "- Immune to fire. Magic resistance %d percent: each spell cast on her fails on a d100 roll of "
               "%d or less (\"Tamaramagil resists ...\")." % (dd["DR_MAGIC_RESIST"], dd["DR_MAGIC_RESIST"]),
               "- In reach: two claws, 1d%d each, or a bite of %dd%d (%d percent of the time when both "
               "reach). One attack each %g seconds, each with its own attack roll. The claws reach a player "
               "who touches her front half; the bite reaches a bit farther in front of her."
               % (dd["DR_CLAW_SIDES"], dd["DR_BITE_DICE"], dd["DR_BITE_SIDES"], dd["DR_BITE_CHANCE"],
                  dd["DR_MELEE_TIME"] + dd["DR_RECOVER_TIME"]),
               "- She cannot leave the middle of her hall, and she turns fast.",
               "",
               "### The breath, and how to live through it", "",
               "- %dd%d + %d fire damage (%d on average) in a cone %s ft long from her jaws, about 20 degrees "
               "to each side. A save against breath halves it, then your fire protections work: Resist Fire "
               "(the spell, the Ring of Coolness, the Potion of Fire Resistance, the Cloak of Fire "
               "Resistance) halves it again, Protection From Fire takes it from its pool."
               % (dd["DR_BREATH_DICE"], dd["DR_BREATH_SIDES"], dd["DR_BREATH_BONUS"], breath_avg,
                  feet(dd["DR_BREATH_RANGE"])),
               "- The first breath comes %g seconds after the fight starts, the next ones %g seconds after "
               "the end of the last." % (dd["DR_BREATH_FIRST"], dd["DR_BREATH_PAUSE"]),
               "- The warning: she roars, rears up, flames show at her jaws, and the message box says "
               "\"Tamaramagil rears and draws a deep breath.\" For the first %g second she turns with you; "
               "then her aim is fixed, and you have %g second to step out of the cone or out of her sight. "
               "The stream lasts %g second and hurts once." % (dd["DR_AIM_TIME"], dd["DR_WIND_TIME"] - dd["DR_AIM_TIME"],
                                                              dd["DR_FIRE_TIME"]),
               "- A wall stops it: the frames of the four doorways of her hall are cover. Beyond %s ft of her "
               "jaws, in the corridor, the breath does not reach, and she cannot follow." % feet(dd["DR_BREATH_RANGE"]),
               "",
               "### The reward", "",
               "- %d experience for her, 7000 for each hatchling." % dd["DR_EXPERIENCE"],
               "- The hoard of the original game: %s." % self.item_names([(16, 3, 3)] + tr13),
               "- ASCENT adds: %s, %d more diamonds and %d gold." % (self.item_names(tr), diamonds - 3, gold),
               "- With the setting \"New gear\": %s ([[New-Gear]])." % self.item_names(gd.g["dragon"]),
               "- In the original game the lever sets the hatchlings free: they leave, the red dragons burn "
               "the Castle Ward of Waterdeep, and the market doubles its prices. With the setting the "
               "hatchlings stay, and the market keeps its prices.",
               "",
               "</details>", "",
               "With the setting off the level is as the original game ships it: no dragon, and the lever "
               "opens a hatch in the ceiling of her hall.", "",
               "## The lich Morik Stormhand", "",
               "Setting: \"The phylactery of the lich\" (it needs \"%s\" for his protections). In %s, the "
               "lich Morik Stormhand waits in his room behind two grills." % (CASTER_AI_SETTING, LEVEL_NAMES[19]), "",
               "In short: with %s he protects himself with %s the moment he sees you. Dispel Magic takes "
               "them away, and a phylactery hidden in the crypts can break them for good. Barander of %s "
               "tells you about it." % (CASTER_AI_SETTING, self.and_list(wards), LEVEL_NAMES[17]), "",
               "<details><summary>Show the details of the encounter (heavier spoilers)</summary>", "",
               "### His protections and spells", "",
               "- Caster level %d. His protections: %s. Stone Skin takes 1d4 + %d hits, Mirror Image gives "
               "1d4 + %d images, the globe stops every spell of level 1 to 3, and Shield stops Magic Missile."
               % (lich_level, self.and_list(wards), lich_level // 2, lich_level // 3),
               "- His spells: %s. His damage spells have the dice of caster level %d: Fireball and Lightning "
               "Bolt %dd6, Magic Missile %d missiles. He has %d hit points." % (
                   self.and_list(attacks), dmg_level, min(dmg_level, 10), missiles, LICH_HIT_POINTS),
               "- Two Banegards guard the alcoves behind the grills. A lever behind an illusory wall of the "
               "corridor before his room opens the grills. He does not come out for you.",
               "",
               "### How to break his protections", "",
               "- **Dispel Magic** on him ends all his protections, with no save: the spell, a scroll, a Rod "
               "of Dispel Magic, or the %s (below). A scroll of it lies in a crypt cell of the west wing of the "
               "upper level." % rname,
               "- **The altar of Velsharoon.** Morik was a priest of Velsharoon, and his phylactery is an "
               "altar of his god in an empty crypt cell of the east wing of the upper level. A thief picks "
               "the lock of the wing; the others open it when they bring the %s of the level to the shield "
               "at its door. One blow of a weapon breaks the altar, and so do Fireball, Lightning Bolt and Dispel "
               "Magic. Broken before the fight, he fights without his protections; broken during the fight, "
               "they end at once. Another scroll of Dispel Magic lies in that cell." % self.W[72].name(),
               "- **How much it matters.** In the test fights of the port (a character who walks up and "
               "fights, with a few healing potions) a fighter of level 6 won 3 of 10 fights without help and "
               "10 of 10 with Dispel Magic or the broken altar; a cleric of level 6 won 3, 5 and 9 of 10; a "
               "thief/mage of level 5/5 won none without help and 9 or 10 of 10 with it.",
               "- The hints: Barander, a priest of Lathander in %s, speaks of the phylactery when you ask how "
               "the lich can be stopped, and at your arrival in the crypts you see tracks of bare bone that "
               "lead again and again to the east crypts." % LEVEL_NAMES[17],
               "- The altar is there once in a game, and not if the lich is dead already.",
               "",
               "</details>", "",
               "## The %s and Daland" % rname, "",
               "Setting: \"New gear\" for the rod. In %s, Daland stands in hidden "
               "crypts, held by a curse that also keeps him from talking." % LEVEL_NAMES[16], "",
               "<details><summary>Show how to free him (spoilers)</summary>", "",
               "- The %s lies at his feet. Every class can use it: each use casts %s at caster level %d. It "
               "has one charge, and a rest at a rest place charges it again; it is never used up. The market "
               "does not sell it." % (rname, self.spell_name(rspell), rlevel),
               "- Use it on him (or any Dispel Magic: the spell, a scroll, a wand or rod of it). He stays "
               "your friend, because your Dispel Magic is no attack, and he can talk. With the rod in your "
               "pack, the talk to the held Daland hints at it.",
               "- Hear his tale and take his flask of Dragon's Blood, pour it on the altar of Kelemvor in the "
               "living quarters of the temple, then talk to him again. He gives the %s and a %s." % (
                   gd.item(5, mace).name(), self.O[potion].name()),
               "- Later, the rest place of %s or the one of %s charges the rod again before the lich, and "
               "its Dispel Magic ends his protections." % (LEVEL_NAMES[15], LEVEL_NAMES[19]),
               "",
               "</details>", "",
               pages_note(), ""]
        self.count("Encounters", 0)
        return "\n".join(out)

    @staticmethod
    def and_list(xs):
        xs = list(xs)
        return ", ".join(xs[:-1]) + " and " + xs[-1] if len(xs) > 1 else "".join(xs)

    # -- Sources --------------------------------------------------------
    # -- New gear (ASCENT-DTU) -------------------------------------------
    def new_gear(self):
        gd = self.gear
        d = gd.d
        out = ["# New gear and loot (ASCENT-DTU)", "",
               "> **Spoiler warning.** This page gives the places of items and the content of treasures. "
               "Read it only if you want to know them before you find them.", "",
               "Everything on this page belongs to the setting \"%s\" of the Improvements menu (on by "
               "default). It is not in the original game, version 1.3. With the setting off the game has "
               "the items, the market and the loot of 1.3. The setting takes effect at the next level." % NEW_GEAR_SETTING, ""]
        # the new items
        rows = []
        for i, r in sorted(gd.armor.items()):
            ac = gd.g["armor_ac"][i - d["DTU_GEAR_FIRST_ARMOR"]]
            rows.append(self.icon_row([r.name(), gear_effect(i, d), signed(-ac) if ac else "0", classes_of(r),
                                       r.num("value", 0), gd.where(15, i)], r))
        fr = gd.frost
        rows.append(self.icon_row([fr.name(), frost_effect(d), "0", classes_of(fr), fr.num("value", 0), gd.where(5, gd.frost_id)], fr))
        for did, dr in sorted(gd.daggers.items(), key=lambda x: x[0] != gd.venom_id):
            rows.append(self.icon_row([dr.name(), dagger_effect(d, did == gd.venom_id), "0", classes_of(dr), dr.num("value", 0),
                                       gd.where(5, did)], dr))
        out += ["## New magic items", "",
                "- AC bonus: how much the item lowers your armor class.",
                "- F/T/M/C: the classes that may wear it ([[Classes]]).",
                "- Value: the base price in gold. The market sells at about 1.35 times the value (Charisma 12) "
                "and buys at 75 percent of it.", "",
                md_table(self.icon_head(["Item", "Effect", "AC bonus", "F/T/M/C", "Value", "Where"]), rows), ""]
        # the wands
        wrows = []
        for i, (r, spell) in sorted(gd.wands.items()):
            wrows.append(self.icon_row([r.name(), self.S[spell].name(), r.num("uses", 1), classes_of(r),
                                        r.num("value", 0), gd.where(5, i)], r))
        out += ["## New wands", "",
                "Five wands of the game files that had no spell now cast one. They work as the other "
                "[[Wands]].", "",
                md_table(self.icon_head(["Wand", "Spell", "Charges", "F/T/M/C", "Value", "Where"]), wrows), ""]
        # the Rod of Kelemvor
        rname, rspell, rlevel, rvalue, rlvl = gd.g["rod"]
        out += ["## The %s" % rname, "",
                "A rod that every class can use. Each use casts %s at caster level %d. It has one charge: a rest "
                "at a rest place charges it again, and it is never used up. Value %d; the market does not sell "
                "it. It lies at the feet of Daland in the hidden crypts of %s: use it on him to free him. Its "
                "Dispel Magic also ends the protections of a spellcaster. Not the same item as the Rod of Dispel "
                "Magic above (a mage only, its charges are used up)."
                % (self.S[rspell].name(), rlevel, rvalue, GEAR_LEVEL_TEXT.get(rlvl, "the temple")), ""]
        # the items of 1.3
        script, potion, mace = gd.g["daland"]
        lvl, ft, _ = gd.g["flame"]
        mrow = gd.item(5, mace)
        frow = gd.item(5, ft)
        removed = ["the %s %s" % (gd.item(c, i).name(), GEAR_STAGES[s]) for s, c, i in gd.g["removed"]]
        out += ["## Items of the original game", "",
                "- %s: Daland of %s gives it with the %s when the altar of Kelemvor is consecrated. "
                "Value %d with the setting. How: Daland stands in hidden crypts, held by a curse that also "
                "keeps him from talking. Cast Dispel Magic on him (the spell, a scroll, or a wand or rod "
                "of it; the Rod of Kelemvor at his feet works for every class): he stays your friend and "
                "can talk. The tombs of Mulhorand have a scroll of the "
                "spell for a mage. Hear his tale and take his flask of Dragon's Blood, pour it on the altar "
                "of Kelemvor in the living quarters of the temple, then talk to him again."
                % (mrow.name(), GEAR_LEVEL_TEXT.get(16, "the temple"), self.O[potion].name(), gd.value(5, mace)),
                "- %s: one lies in %s, in a prison cell of the troll wing, at the far end of the "
                "corridors from their entry. Its fire stops the regeneration of a troll. Value %d with the "
                "setting. The market sells another one %s." % (frow.name(), GEAR_LEVEL_TEXT.get(lvl, "a drow level"),
                                                              gd.value(5, ft), " and ".join(GEAR_STAGES[s] for s in gd.stages_of(5, ft))),
                "- The market of the original game sells " + "; ".join(removed) + ". With the setting it "
                "does not: the market sells it later (above). A market that already has it keeps it.", ""]
        old = [(row, gd.item(row[2], row[3])) for row in gd.g["market"]
               if not (row[2] == 15 and row[3] in gd.armor) and not (row[2] == 5 and row[3] in gd.wands)
               and not (row[2] == 5 and row[3] == ft) and not (row[2] == 5 and row[3] in gd.daggers)
               and not (row[2] == 5 and row[3] in gd.arrow_lines)]
        orows = [self.icon_row([r.name(), GEAR_STAGES[row[0]], "many" if row[1] < 0 else str(row[1]),
                                gd.value(row[2], row[3])], r) for row, r in old]
        out += ["The original game has rules for these magic items but never gives them to you. The market "
                "sells them, weaker ones first. Their rules are on the other pages.", "",
                md_table(self.icon_head(["Item", "In the market", "Quantity", "Value"]), orows), "",
                "A scroll is read by a character of its class: the Scroll of Dispel Magic by a mage. It frees "
                "Daland and ends the protections of a spellcaster.", ""]
        # the market by stage
        srows = []
        for s in sorted(set(row[0] for row in gd.g["market"] + gd.g["class_market"])):
            goods = [gd.item(row[2], row[3]).name() + (" (now many: no limit)" if row[1] < 0 else "")
                     for row in gd.g["market"] if row[0] == s]
            goods += ["%s (with the class gear, below)" % gd.item(row[2], row[3]).name()
                      for row in gd.g["class_market"] if row[0] == s]
            srows.append([GEAR_STAGES[s], ", ".join(goods)])
        out += ["## The market of Waterdeep", "",
                "Each reward of Khelben adds goods to the market. The setting adds these to the goods of "
                "the original game. \"Many\": the market has no limit of that good from then on.", "",
                md_table(["When", "New goods"], srows), ""]
        out += self.bows_and_arrows()
        # gold and gems
        gems = [(r.name(), r.num("value", 0)) for i, r in sorted(gd.misc.items())]
        grows = [self.icon_row([r.name(), r.num("value", 0)], r) for i, r in sorted(gd.misc.items())]
        # the Ruby and the Diamond of the original game
        for i in (GEM_RUBY, GEM_DIAMOND):
            grows.append(self.icon_row(["%s (also in the original game)" % self.O[i].name(), self.O[i].num("value", 0)],
                                       self.O[i]))
        grows.sort(key=lambda row: int(row[-1]))
        p4, r3, s1 = d["DTU_GEAR_PEARL_IN_4"], d["DTU_GEAR_RUBY_IN_3"], d["DTU_GEAR_SAPPHIRE_ONE_IN"]
        out += ["## Gold and gems", "",
                "- Gold: a slain creature that carries money leaves gold %d times in 10: its money plus 1 to "
                "%d times its money. The people you talk to (Durnan, Mirt and the others) leave none. In the "
                "original game only a humanoid leaves gold, 4 times in 10, and much less." % (d["DTU_GEAR_GOLD_IN_10"],
                                                                                             d["DTU_GEAR_GOLD_DICE"]),
                "- Gems: an undead or intelligent creature leaves a gem 1 time in %d. The gem follows the "
                "experience the creature gives: below %d a %s; below %d a %s %d times in 4, else a Ruby; below "
                "%d a Ruby %d times in 3, else a Diamond; from %d a Diamond, and 1 time in %d a %s."
                % (d["DTU_GEAR_GEM_ONE_IN"], d["DTU_GEAR_GEM_XP_PEARL"], gems[0][0], d["DTU_GEAR_GEM_XP_RUBY"], gems[1][0],
                   p4, d["DTU_GEAR_GEM_XP_DIAMOND"], r3, d["DTU_GEAR_GEM_XP_DIAMOND"], s1, gems[2][0]),
                "- Undead: skeletons, zombies, ghouls, ghasts, mummies, Banegards, the lich and the spirits "
                "of the undead.",
                "- Intelligent: the people (humans, drow, dwarves, halflings, kobolds, goblins, orcs, ogres, "
                "trolls, ghouls, mind flayers), gargoyles, and every creature that casts spells (mephits, "
                "beholders, spectators, dragons).",
                "- No gems: animals and vermin (rats, bats, beetles, scarabs, spiders) and the Mage Spirit.",
                "- The market buys a gem at 75 percent of its value (Charisma 12).", "",
                md_table(self.icon_head(["Gem", "Value"]), grows), ""]
        # the place for the spells of the caster branch
        out += self.new_gear_spells()
        # the treasury
        trows = [self.icon_row([gd.item(c, i).name(), gear_effect(i, d) if c == 15 else frost_effect(d) if i == gd.frost_id else "",
                                count], gd.item(c, i))
                 for c, i, count in gd.g["dragon"]]
        out += ["## The treasury of the red dragon", "",
                "> **Heavier spoiler.** This section names the treasure of the dragon of the drow city.", "",
                "<details><summary>Show the treasure</summary>", "",
                md_table(self.icon_head(["Item", "Effect", "Count"]), trows), "",
                "</details>", ""]
        # the place for the dragon encounter of the dragon branch
        out += self.new_gear_dragon()
        out += self.class_gear_section()
        out += ["## Saved games", "",
                "A game saved with new items loads again with the setting. With the setting off, the new "
                "items of the save are gone and their effects end; the five new wands are the plain wands "
                "of the original game again. Do not load such a save in the original game.", "",
                pages_note(), ""]
        self.count("New-Gear", len(rows) + len(wrows) + len(orows) + len(srows) + len(grows) + len(trows))
        return "\n".join(out)

    def new_gear_spells(self):
        """The new spells and their scrolls on [[New-Gear]] (the setting
        Caster AI brings them, not New gear)."""
        rows = []
        for s, rec in sorted(self.port_spells.items()):
            if s not in PORT_SPELLS:
                continue
            where = []
            if s in ALEENA_PRICE:
                where.append("Aleena Paladinstar (the Yawning Portal) sells it with her spells of level %d for %d "
                             "gold" % (rec.num("level", 1), ALEENA_PRICE[s]))
            for lvl, old in self.port_scrolls.get(s, ()):
                where.append("a scroll in %s, in place of a scroll of %s" % (LEVEL_NAMES[lvl], old))
            if rec.has("cleric_use"):
                where.append("a cleric gets it with the cleric spells of level %d" % rec.num("level", 1))
            rows.append(self.icon_row([rec.name(), "%s %d" % ("cleric" if rec.has("cleric_use") else "mage",
                                                               rec.num("level", 1)), "; ".join(where)], rec))
        return ["## New spells and their scrolls", "",
                "The setting \"%s\" (Improvements menu, on by default) brings three new spells for the "
                "player. Their rules are on the pages [[Wizard Spells|Wizard-Spells]] and [[Priest "
                "Spells|Priest-Spells]]. The scrolls are placed when a level is loaded with no saved state, "
                "so a level that your game has visited keeps its old scroll." % CASTER_AI_SETTING, "",
                md_table(self.icon_head(["Spell", "Class, level", "Where"]), rows), ""]

    def bows_and_arrows(self):
        """The rule of the bows and arrows of the new gear and the arrows of
        the market by stage (dtugear.h, dtugear.c Gear_arrows, Gear_market)."""
        gd = self.gear
        d = gd.d
        rows = []
        effect = {d["DTU_GEAR_ARROWS3"]: "+%d to hit and damage" % 3,
                  d["DTU_GEAR_ARROW_FIRE"]: "1d6 and 1d6 fire. Fire resistance halves the fire, and the fire stops "
                                            "the regeneration of a troll",
                  d["DTU_GEAR_ARROW_ACID"]: "1d6 and 1d%d acid. The acid stops the regeneration of a troll"
                                            % d["DTU_GEAR_ACID_DICE"],
                  d["DTU_GEAR_ARROW_DISPEL"]: "1d6. A hit casts Dispel Magic on the creature: all its spell effects "
                                              "end, with no save. The arrow is an attack: the creature turns hostile"}
        for i in sorted(gd.arrow_lines):
            r = gd.arrow_lines[i]
            if i == gd.venom_arrows_id:
                continue
            if i in effect:
                what = effect[i]
            else:
                what = "+%d to hit and damage" % r.num("damage_bonus", 0)
            stages = []
            for row in gd.g["market"]:
                if row[2] == 5 and row[3] == i:
                    stages.append(("many" if row[1] < 0 else "%d" % row[1]) + " " + GEAR_STAGES[row[0]])
            rows.append(self.icon_row([r.name(), what, "+%d" % gd.arrow_plus[i], gd.value(5, i), "; ".join(stages)], r))
        venom = gd.arrow_lines[gd.venom_arrows_id]
        bow = gd.item(16, d["DTU_GEAR_BOW_LIGHTNING"])
        return ["## Bows and arrows", "",
                "With the setting the bows and arrows have the rule of ASCENT-DTU:", "",
                "- To hit: the bonus of the bow and the bonus of the arrow add. The Arrows +1 and +2 of the "
                "original game get their +1 and +2 to hit (the original game gives them none; the setting "
                "\"Rulebook items\" gives the same values, and they never add twice).",
                "- Damage: the larger of the damage bonus of the bow and the damage bonus of the arrow counts; "
                "they do not add. A Short Bow of Speed (+2) with plain arrows: +2 to hit, +2 damage. With "
                "Arrows +3: +5 to hit, +3 damage.",
                "- Crossbows and bolts the same way. A thrown weapon keeps the rule of the original game.",
                "- Magic: every arrow but the plain Arrow is magic and hurts a creature that only magic weapons "
                "hurt. The game knows only magic or not; the plus in the table is what the arrow counts as.",
                "- The original game adds the bonus of the bow to the attack and the damage, and the damage "
                "bonus of the arrow too (a Short Bow +2 with Arrows +2: +2 to hit and +4 damage).", "",
                "In the market a tier comes in a number; when the next tier comes, the one before has no limit "
                "(\"many\"). The Dispelling Arrows never do.", "",
                md_table(self.icon_head(["Arrows", "Damage and effect", "Counts as", "Value", "In the market"]), rows), "",
                "- The %s (single class thieves, [[New gear|New-Gear#gear-for-single-class-mages-and-thieves]]): "
                "a plain arrow whose hit poisons; magic, counts as +1, no bonus." % venom.name(),
                "- The %s (single class thieves, Mirt): +%d to hit and no damage bonus of its own, so an arrow "
                "does its own damage bonus; each arrow shot from it deals %dd%d electric damage more on a hit (none "
                "to a creature immune to electricity)." % (bow.name(), bow.num("to_hit_bonus", 0),
                                                           d["DTU_GEAR_LIGHTNING_N"], d["DTU_GEAR_LIGHTNING_SIDES"]), ""]

    def new_gear_dragon(self):
        """The encounter of the dragon: a link to [[Encounters]]."""
        return ["The hoard of the dragon has more than these %d items, and the encounter has its own "
                "page: [[Encounters]]." % len(self.gear.g["dragon"]), ""]

    def check_class_gear(self):
        """CLASS_GEAR against the blocks of dtuclassgear.c: each price and
        each mission of the table is in the blocks and the other way round,
        and each sale of the blocks has its row."""
        g = self.gear.g
        prices = set(p for row in CLASS_GEAR for p in row[3])
        missions = set(m for row in CLASS_GEAR for m in (row[4] or ()))
        if prices != set(g["class_gold"]):
            raise SystemExit("CLASS_GEAR: the prices %s, dtuclassgear.c %s" % (sorted(prices), g["class_gold"]))
        if missions != set(g["class_missions"]):
            raise SystemExit("CLASS_GEAR: the missions %s, dtuclassgear.c %s" % (sorted(missions), g["class_missions"]))
        for price, cls, i, n in g["class_sales"]:
            name = self.spell_name(i) if cls == 17 else self.gear.item(cls, i).name()
            name = re.sub(r" I+$", "", name)      # the Pearls of Power I to III share a row
            if not any(price in row[3] and name in row[2] for row in CLASS_GEAR):
                raise SystemExit("CLASS_GEAR: no row for the sale of %s for %d gold (dtuclassgear.c)" % (name, price))
        for m in missions:
            if m not in MISSION_STAGE:
                raise SystemExit("CLASS_GEAR: mission %d is no reward stage (MISSION_STAGE)" % m)

    def class_gear_section(self):
        """The class offers on the page [[New-Gear]] (under its spoiler warning)."""
        self.check_class_gear()
        gd = self.gear
        d = gd.d
        rows = []
        for cls, who, item, prices, missions, when in CLASS_GEAR:
            if missions:
                when = "; ".join(("%s: " % roman if len(missions) > 1 else "") + GEAR_STAGES[MISSION_STAGE[m]]
                                 for roman, m in zip(("I", "II", "III"), missions))
            rows.append([who, item, ", ".join(str(p) for p in prices), when])
        arrow = gd.g["class_arrow"]
        market = []
        for stage, qty, c, i, count in gd.g["class_market"]:
            market.append("a %s %s" % (gd.item(c, i).name(), GEAR_STAGES[stage]))
        boots = gd.item(15, d["DTU_GEAR_BOOTS_ELVEN"]).name()
        bracers = gd.item(15, d["DTU_GEAR_BRACERS_ARCHERY"]).name()
        return ["## Gear for single class mages and thieves", "",
                "The setting \"%s\" (Improvements menu, on by default) gives a character with one class, a "
                "mage or a thief, offers that the other characters do not get. No new class power: only gear, "
                "and it costs gold. A character with two classes, a fighter or a cleric sees none of them. "
                "Each item is sold once; the arrows and the scrolls can be bought again, the potions three times. "
                "The new items need the setting \"%s\" too. The gold is the price; the market buys these "
                "items back for less." % (CLASS_GEAR_SETTING, NEW_GEAR_SETTING), "",
                "- A mage: talk to Aleena Paladinstar in the Yawning Portal and ask for her scrolls. Her "
                "first menu has the new reply lines.",
                "- A thief: Nalbas Oddpepper makes his offer at the first talk, and has his little stock at "
                "each later talk. Mirt the Merchant opens his old guild things at a talk after the first.", "",
                md_table(["Seller", "Item", "Gold", "From"], rows), "",
                "- %s: an %s (1d6) whose hit poisons a creature that fails a save against poison: 1 hit point "
                "each second for %d seconds. Undead and extraplanar creatures are immune. Magic: it hurts what "
                "a +1 weapon hurts, with no bonus. Nalbas and Mirt sell them." % (
                    arrow[2], arrow[1], d["DTU_GEAR_POISON_TIME"]),
                "- %s: +%d to hit and no damage bonus of its own; each arrow shot from it deals %dd%d electric "
                "damage more on a hit ([[New gear|New-Gear#bows-and-arrows]])." % (
                    gd.item(16, d["DTU_GEAR_BOW_LIGHTNING"]).name(), gd.item(16, d["DTU_GEAR_BOW_LIGHTNING"]).num("to_hit_bonus", 0),
                    d["DTU_GEAR_LIGHTNING_N"], d["DTU_GEAR_LIGHTNING_SIDES"]),
                "- Only a single class thief gets the %s, the %s and the %s. The magic arrows of the market "
                "are for every character." % (gd.item(16, d["DTU_GEAR_BOW_LIGHTNING"]).name(), arrow[2], bracers),
                "- Pearl of Power I to III: use it (the item list or a hot key) to get back one spent mage spell "
                "slot of spell level 1, 2 or 3. Once in each dungeon level; it is ready again at the next "
                "level and after a rest. A saved game keeps its state.",
                "- %s: Move Silently %d while worn." % (boots, d["DTU_GEAR_ELVEN_MOVE"]),
                "- %s: +%d to hit with arrows and bolts. They take the gauntlet place, so they never add to the "
                "Gloves of the Archer." % (bracers, d["DTU_GEAR_BRACERS_HIT"]),
                "- The market adds, for any mage: %s." % "; ".join(market), ""]

    def port_sources(self):
        """The evidence of the spells of the port, [[Monster-Behavior]] and
        [[Encounters]] (sections of Sources.md). None of it is in DTUGAME.EXE."""
        srows = [[self.port_spells[s].name(), str(s), "%d s" % self.port_cast[s],
                  PORT_SPELLS[s]["src"] if s in PORT_SPELLS else "creature spell ([[Monster-Behavior]])"]
                 for s in sorted(self.port_spells)]
        return ["## Spells of the port (ASCENT-DTU)", "",
                "The spell lines 56 and up are added by the port after the table fixup of each table "
                "(`dtu_spell_port_line`, `engine/main/dtuspell.c`); their cast times are `port_cast_time`. "
                "The tool reads both with `tools/port_tables.py`. The icons 200 and up are new art of "
                "`tools/spell_icons.py`, stored as color numbers in `engine/main/dtuspellicons.h` and read "
                "from there. The scrolls of Caster AI are the table `Port_scrolls` of `dtuobj_setup.c`; the "
                "prices of Aleena Paladinstar are in `patch_aleena` of `dtuscript.c`. docs/re/spells.md, "
                "section 15.", "",
                md_table(["Spell", "Line", "Cast", "Code"], srows), "",
                "Corrected rules of 1.3 spells on the spell pages: Lightning Bolt (D-195, `Dtu_fix_bolt_pass`), "
                "Dispel Magic (D-196, `Dtu_fix_dispel_peace`), Stone Skin (D-197, `Dtu_fix_stoneskin_hit`), "
                "Hold Person and held targets (D-198, `Dtu_fix_hold_hit`; `Dtu_fix_player_hold`, "
                "`engine/main/dtuhold.h`), Sleep (D-191, `Dtu_fix_sleep_wake`; `Dtu_sleep_view`), the "
                "spell books of scrolls (D-189, `Dtu_fix_learn_class`). The D numbers are the rows of "
                "docs/audit/defects-1.3.md.", "",
                "## Monster behavior and encounters (ASCENT-DTU)", "",
                "- [[Monster-Behavior]], Improved AI: `Dtu_improved_ai`, `engine/main/dtuobj_aware.c` "
                "(`creature_senses_player`, `creature_in_combat`, `creature_join_fight`, "
                "`attack_wall_between`; the distances `DTU_HEAR_DIST`, `DTU_ALARM_DIST`, `DTU_JOIN_DIST` "
                "and the time `DTU_COMBAT_TIME`, read by the tool) and `dtuobj_ai.c` (`opens_doors`, "
                "`melee_fighter`); "
                "docs/re/ai.md, \"Improved AI\".",
                "- [[Monster-Behavior]], Caster AI: `Dtu_caster_ai`; the lists `Caster_lists`, the types "
                "`Caster_types` and `Caster_limits` of `engine/main/dtucaster.h` (the table of the page is "
                "made from them); the numbers `CAI_` and `CASTER_HOLD_` of `dtuspell.c` and "
                "`DTU_CASTER_KEEP_` of `dtuspell.h`; docs/re/ai.md, \"Caster AI\".",
                "- [[Encounters]], the dragon: `Dtu_restored_dragon` and `Dtu_fix_tamaramagil`, "
                "`engine/main/dtudragon.c` (the `DR_` numbers and the table `Dr_treasure`, read by the tool); "
                "the hoard of 1.3 is objects 50 to 60 of DROW2; docs/re/dragon.md.",
                "- [[Encounters]], the lich: `Dtu_lich_phylactery`, `engine/main/dtulich.c`; his list and "
                "`Caster_limits` (`lich3`) of `dtucaster.h`; the test fights of `tools/caster_balance.py lich` "
                "(docs/re/ai.md, \"The lich and the holds, measured\"); docs/re/lich.md.",
                "- [[Encounters]], the Rod of Kelemvor: `dtu_gear_rod_*` of `engine/main/dtugear.c`, "
                "`dtu_gear_place_rod` of `dtuobj_setup.c`; docs/re/objects.md, \"The Rod of Kelemvor\".",
                "- The class gear on [[New-Gear]]: the table of the tool is checked against the sales, the "
                "prices and the missions of the blocks of `engine/main/dtuclassgear.c` and the lines "
                "`Class_lines`, `Class_arrow`, `Class_market` of `dtugear.c` (`tools/gear_tables.py`); "
                "docs/re/scripts.md, section 10.", ""]

    def gear_rows_armor(self):
        """The new body armor and shield lines for Armor.md."""
        gd = self.gear
        rows = []
        for i, r in sorted(gd.armor.items()):
            if not (r.has("armor") or r.has("shield")):
                continue
            ac = gd.g["armor_ac"][i - gd.d["DTU_GEAR_FIRST_ARMOR"]]
            note = " ".join(x for x in (gear_effect(i, gd.d), NEW_GEAR_LINK) if x)
            rows.append(self.icon_row([r.name(), "shield" if r.has("shield") else "body", signed(-ac) if ac else "0",
                                       str(10 + ac), classes_of(r), r.num("value", 0), note], r))
        return rows

    def gear_rows_magic(self, flags):
        """The new worn items with one of the flags, for Magic-Items.md."""
        gd = self.gear
        rows = []
        for i, r in sorted(gd.armor.items()):
            if r.has("armor") or r.has("shield") or not any(r.has(f) for f in flags):
                continue
            ac = gd.g["armor_ac"][i - gd.d["DTU_GEAR_FIRST_ARMOR"]]
            rows.append(self.icon_row([r.name(), signed(-ac) if ac else "0", classes_of(r), r.num("value", 0),
                                       gear_effect(i, gd.d) + " " + NEW_GEAR_LINK], r))
        return rows

    def gear_sources(self):
        """The evidence of [[New-Gear]] (the section of Sources.md)."""
        gd = self.gear
        lvl, ft, pos = gd.g["flame"]
        script, potion, mace = gd.g["daland"]
        return ["## New gear (ASCENT-DTU)", "",
                "The page [[New-Gear]] is the setting \"%s\" of ASCENT (`Dtu_new_gear`, `engine/main/dtugear.c` "
                "and `dtugear.h`; docs/re/objects.md, \"New gear\"). The tool reads the tables of the code "
                "with `tools/gear_tables.py`:" % NEW_GEAR_SETTING, "",
                "- The new item lines: `Gear_lines` (read after the table fixup by the table reader, armor %d "
                "and up, misc %d and up), their armor class `Gear_armor_ac`; the effects by number in "
                "`dtuchar.c` (`item_bits`: boot bit 5, cloak and armor bit 1, belt bit 6, ring bit 15; "
                "`archer_bonus`, `regen_frame`, the constitution of the belt) and the Mirror Shield in "
                "`dtu_gear_mirror` (called from `missile_hit_object`, `dtuobj_missile.c`)."
                % (gd.d["DTU_GEAR_FIRST_ARMOR"], gd.d["DTU_GEAR_FIRST_MISC"]),
                "- The wands: `Gear_wands` (the lines 63 to 67 of 1.3 have `enemy_spell=-1` and no `PC_spell`); "
                "the values of the Mace of Disruption and the Flame Tongue: `Gear_values`.",
                "- The market: `Gear_market` (stage, quantity, class, number, count), `Gear_market_removed`; "
                "the stages are the rewards 0x879A7 of the missions (docs/re/scripts.md, section 8).",
                "- The Mace of Disruption: script %d gives misc %d (`dtu_gear_script_give`, from the GIVE "
                "statement 0xA8F77 of `dtuscript.c`). Daland is held by the level fix 0xAD129 and cannot "
                "talk while held (0xABBE5)." % (script, potion),
                "- The Flame Tongue: `dtu_gear_place_flame_tongue` (`dtuobj_setup.c`), level %d of DTU.MSN at "
                "%.1f %.1f %.1f, once in a game (global flag 0x%X)." % (lvl, pos[0], pos[1], pos[2],
                                                                         gd.d["DTU_GEAR_FLAG_FLAME"]),
                "- The drops: `gear_drops` (`dtuobj_combat.c`) with `dtu_gear_gold`, `dtu_gear_gem` and "
                "`dtu_gear_gem_class` (the door bit, a person model of Improved AI, a spell level, a spell as "
                "the weapon, the weapons 81 to 88); the rule of 1.3 is 0x6A639. The named people: `Gear_no_gold`.",
                "- The treasury of the dragon: `Gear_dragon` (`dtu_gear_dragon_treasure`).",
                "- The checks: `tools/check_items.py` (gear_*), `tools/questtest.py` (myrkul_daland_*), "
                "`tools/check_saveload.py` (the fourth and fifth run), `tools/gear_income.py` (the gold and gems "
                "of the levels).", ""]

    def sources(self):
        c = self.c
        out = ["# Sources", "",
               "Where the numbers of these pages come from. The pages are written by the script "
               "`tools/wiki_rules.py` of ASCENT from the files of an installed Descent to Undermountain "
               "1.3. Addresses are linear addresses in `DTUGAME.EXE` 1.3 (object 1 at 0x10000). The "
               "reverse engineering notes are in `docs/re/` of the ASCENT repository.", "",
               "## Kinds of numbers", "",
               md_table(["Kind", "Source"], [
                   ["Item and spell names, damage dice, to-hit and damage bonuses, values, class flags, "
                    "spell levels, radii",
                    "the item and spell lines (`$WEAPON`, `$ARMOR`, `$OBJECT`, `$SPELL`) of the table "
                    "`YPINN.BIN`, after the table fixup of `DTUGAME.EXE` (below)"],
                   ["Race, class, ability, experience, THAC0, hit point, saving throw, slot, thief and "
                    "turn undead tables; armor class of the items",
                    "data tables of `DTUGAME.EXE` (below)"],
                   ["Spell effects, durations, saves, item effects, potion effects",
                    "the code of `DTUGAME.EXE`: the spell cases of 0x8CFFF (jump table 0x8CF1F), the item "
                    "use 0x37A6A, the worn item bits 0xAFB55"],
               ] + ([
                   ["Icons of the items and spells",
                    "`icon=` of the item or spell line after the table fixup; the picture is that cell of "
                    "the high resolution icon sheet, entry %d of `FEBGND.DAT` (%dx%d cells, eight in a "
                    "row: icon n at column n & 7, row n / 8; 0x3420C, 0x36E3F), in the colors of "
                    "`PALETTE.256`, with color %d transparent" % (ICON_ENTRY, ICON_CELL, ICON_CELL, ICON_TRANSPARENT)],
               ] if self.icons else [])), "",
               ] + ([
               "The icons of the item and spell tables come from the game files of Descent to "
               "Undermountain. They are the property of their owner and are shown here only to "
               "identify the items and spells.", ""] if self.icons else []) + [
               "## Units", "",
               "- Two game units are one foot: the fall message of the game divides the fall distance "
               "by two (`fell >> 17`).",
               "- Game seconds: the game messages call 60 of them a minute and 3600 an hour.",
               "- Spell missiles fly 100 units (50 ft) per second: no `$SPELL` line has `speed=`, and "
               "the default of the reader 0x242F6 is 100.0. They live 30 s, Lightning Bolt 2 s and "
               "Melf's Acid Arrows (L/3 + 1) minutes (0x56FF6 writes the life of the missile "
               "template).", "",
               "## The table fixup", "",
               "After it reads the tables, `DTUGAME.EXE` writes fixed values into fixed lines of the "
               "item, weapon, armor and spell tables (0x1A344, called at 0x1C8F9). The game never "
               "shows the table text of these fields, so the pages show the values after the fixup. "
               "It renames ten items and spells (Produce Fire, Find Traps, Cure Blindness or "
               "Deafness, Drowslayer, Longsword +3, Dwarven Scimitar, Ring of Wizardry, Gauntlets of "
               "Dexterity, Cloak of Lathandar, Potion of Invisibility), copies four spell lines to "
               "make the cleric versions 52 to 55, and changes ranges, radii, the area flag, damage "
               "dice, classes and prices. The full list is in `engine/main/dtutblfix.c` of ASCENT; "
               "`tools/check_tblfix.py` prints each change (%d lines of `YPINN.BIN`)." % len(self.t.fixed or ()), "",
               "## The tables of all levels", ""]
        if self.check is None:
            out += ["This build did not compare the level tables (no `--tables`).", ""]
        else:
            mission = [x for x in self.check if x[0][:-4].lower() in self.mission]
            other = [x for x in self.check if x[0][:-4].lower() not in self.mission]
            bad = [x for x in mission if x[2]]
            have = set(x[0][:-4].lower() for x in self.check)
            missing = sorted(m for m in self.mission if m not in have)
            out += ["The item and spell lines of the level tables were compared with `YPINN.BIN`: "
                    "names, dice, bonuses, values, classes, spell levels, ranges, radii, "
                    "durations, types and the rule flags. %d tables are levels of the mission list "
                    "`DTU.MSN`; %s." % (len(mission), "all of them agree" if not bad else
                                        "%d differ" % len(bad))
                    + (" The mission levels %s have no table of their own in the unpacked files."
                       % ", ".join("`%s`" % m for m in missing) if missing else ""), ""]
            if bad:
                out += ["- %s: %s" % (name, "; ".join(d[:4])) for name, _, d in bad] + [""]
            if other:
                odiff = [x[0] for x in other if x[2]]
                out += ["The tables %s are not levels of the mission. Of these, %s differ from "
                        "`YPINN.BIN` (older dialects of the table, docs/formats/tbl.md); the pages do "
                        "not use them." % (", ".join("`%s`" % x[0] for x in other),
                                           ", ".join("`%s`" % x for x in odiff) or "none")]
            out += [""]
        out += ["## Data tables of the executable", "",
                md_table(["Address", "Count", "Contents"],
                         [["0x%X" % a, str(n), d] for k, (a, f, n, d) in EXE_TABLES.items()]), ""]
        srows = []
        for s in sorted(SPELLS):
            if s >= len(self.S):
                continue
            srows.append([self.S[s].name(), str(s), SPELLS[s]["src"], SPELLS[s].get("tech", "")])
        out += ["## Spell code", "",
                "L is the caster level: 0x91EC1 returns the level of the class of the spell. For a mage "
                "spell it takes the first class that is not a cleric, so a fighter or a thief casts a "
                "mage wand at the own class level. The damage function is 0x91BC3 (each die "
                "`rand() % sides + 1`, then the per level part), the save function 0xAE596 (category 4 "
                "for spells). The area function 0x90CEF takes each object within radius + L x "
                "level_radius units (0x90D20 to 0x90D37: the high words of the dwords at "
                "Spell_info+6 and +0xA are the words `radius`, +8, and `level_radius`, +0xC, of the "
                "reader 0x242F6), with the quick distance. After the table fixup every area spell has "
                "level_radius 0. ASCENT uses the same sum (dtu_spell_area) with the exact distance.", "",
                md_table(["Spell", "Line", "Code", "Detail"], srows), ""]
        out += ["## Item code", "",
                md_table(["Item", "Code"], [[self.O[i].name(), POTIONS[i][1]] for i in range(72, 89)]
                         + [["Gem of Curing", "case 2 of 0x37A6A (0x3890F)"],
                            ["Scroll, thief scroll", "cases 0x66 and 0x7F of 0x37A6A, text screen 0x9DC42"],
                            ["Cursed Scroll", "case 0x6A of 0x37A6A"],
                            ["Spider Pieces", "cases 0x12 to 0x1A of 0x37A6A: misc 18 turn undead at "
                             "level (class 1 level + 1) x 2 (0x38196), 19 the map, 20 the people in "
                             "sight (0x385D3), 21 no case, 22 Fear and seven spells, 23 the view of "
                             "each creature (0x38268), 24 script 0x1FC, 25 the Assembled Spider, 26 "
                             "all hit points and nine spells. The statue of FLAME takes 18, 19, 24, "
                             "26, 22, 23 and 20"],
                            ["Ring of Armoring", "0xB0014, 0xB01B6, 0xAFAD1 (after the caps 0xB05FC, "
                             "0xB064F, 0xB067C)"],
                            ["Boots (misc 8)", "the boot place counts only an armor line (0xAFBC8)"],
                            ["Drowslayer (weapon 13)", "use 0x37B58, drop 0xB7197, warp 0x59D2D, "
                             "script 0x1FD"],
                            ["Plot items cannot be dropped", "0xB611C"],
                            ["Partial stacks", "0xB65BB (1.3 cuts the count to stack_limit)"],
                            ["Worn items", "the item bits of 0xAFB55"],
                            ["Weapon powers", "0x2AB9F, 0x2B1F2, 0xB337E, 0xAFB55, 0xB038F"]]), ""]
        out += ["## Other rules", "",
                md_table(["Rule", "Code"], RULE_SOURCES), ""]
        out += ["## Rules found in the code", "",
                "These rules were open questions of the reverse engineering. Each was decided from "
                "the 1.3 code.", "",
                md_table(["Rule", "Evidence"], SOURCES_FINDINGS), ""]
        out += ["## Corrections of ASCENT", "",
                "ASCENT corrects these defects of 1.3 that change the rules of these pages. Each "
                "setting is on by default unless the row says otherwise; the option turns it off for "
                "one run (\"none\": no option). The corrections of the physics, the creature "
                "movement, the sound and the triggers are in the reverse engineering notes.", "",
                md_table(["Setting", "Option", "Correction"], FIXES), ""]
        if NOT_IN_ASCENT:
            out += ["## Not in ASCENT yet", "",
                    "\n".join("- " + x for x in NOT_IN_ASCENT), ""]
        # Values of the lines that no code reads
        unused = []
        for s, r in enumerate(self.S):
            rng = r.num("range", 0)
            lr = r.num("level_range", 0)
            unused.append([r.name(), str(s), ("%s ft" % feet(rng)) + (" + %s ft per level" % feet(lr) if lr else ""),
                           r.get("duration") or "-", r.get("level_duration") or "-"])
        out += ["## Spell line values the game does not use", "",
                "The spell lines have `range`, `level_range`, `duration` and `level_duration`. The "
                "spell code does not read them: a missile flies until it hits or its life ends, and "
                "each spell case has its own time. The only reader of `duration` is 0x43E3A, for a "
                "spell with `uses` -1 (no line has that); the value it stores is never read. The "
                "values, for comparison with the manual:", "",
                md_table(["Spell", "Line", "Range", "duration", "level_duration"], unused), ""]
        olines = []
        for s, why in OTHER_SPELL_LINES.items():
            olines.append([self.S[s].name(), str(s), why + " " + SPELLS[s].get("tech", "")])
        out += ["## Other spell table lines", "",
                "Lines of the spell table that are not on the spell pages.", "",
                md_table(["Line name", "Line", "Why"], olines), ""]
        wlines = [[str(i), self.W[i].name(), WEAPON_INTERNAL[i]] for i in sorted(WEAPON_INTERNAL)]
        wlines += [["79", self.W[79].name(), "creature weapon; casts spell line 51 (Find Traps)"]]
        out += ["## Other weapon lines", "",
                "Lines for spells, traps and creatures, and lines that the tables mark as unused.", "",
                md_table(["Line", "Name", "Use"], wlines), ""]
        out += self.gear_sources()
        out += self.port_sources()
        out += ["## Item line values the game does not use", "",
                "\n".join("- " + x for x in UNUSED_ITEM_VALUES), ""]
        self.count("Sources", len(EXE_TABLES) + len(srows) + len(SOURCES_FINDINGS) + len(FIXES)
                   + len(unused) + len(olines) + len(wlines) + len(RULE_SOURCES) + 22)
        return "\n".join(out)


# ---------------------------------------------------------------------
# The new gear of ASCENT-DTU (the setting "New gear", engine/main/dtugear.c).
# The items, the values, the classes, the market goods, the places, the
# treasury and the numbers of the drop rule come from the code tables
# (tools/gear_tables.py). The words for the effects are here, with the
# numbers of dtugear.h in them.

NEW_GEAR_LINK = "New in ASCENT-DTU: [[New-Gear]]."
NEW_GEAR_SETTING = "New gear"

# The class gear (the setting "Gear for single class mages and thieves",
# engine/main/dtuclassgear.c and dtugear.h): who sells what, the prices and
# the first mission (the mission number that the blocks test, None: no
# test). Pages.check_class_gear() compares the prices and the missions with
# the blocks of dtuclassgear.c and the goods with its sales.
CLASS_GEAR_SETTING = "Gear for single class mages and thieves"
CLASS_GEAR = [
    ("mage", "Aleena Paladinstar", "Wand of Magic Missiles", [750], None,
     "the first visit; one more at each reward of Khelben that adds goods to the market"),
    ("mage", "Aleena Paladinstar", "Wand of Paralyzation", [1000], [7], None),
    ("mage", "Aleena Paladinstar", "Wand of Fireballs", [1500], [10], None),
    ("mage", "Aleena Paladinstar", "5 scrolls of Magic Missile (if you know the spell)", [250], None, "any time"),
    ("mage", "Aleena Paladinstar", "3 scrolls of Lightning Bolt (if you know the spell)", [1500], [10], None),
    ("mage", "Aleena Paladinstar", "Pearl of Power I, II, III", [2500, 6000, 12000], [3, 10, 16], None),
    ("mage", "Aleena Paladinstar", "Bracers of Defense AC2", [5000], [10], None),
    ("thief", "Nalbas Oddpepper", "Short Bow and 16 Venom Arrows", [300], None, "the first talk"),
    ("thief", "Nalbas Oddpepper", "8 more Venom Arrows", [160], None, "after the bow"),
    ("thief", "Nalbas Oddpepper", "Potion of Invisibility (three in all, each)", [400], [7], None),
    ("thief", "Mirt the Merchant", "Leather Armor +2", [2000], [3], None),
    ("thief", "Mirt the Merchant", "Master Lockpicks", [600], [3], None),
    ("thief", "Mirt the Merchant", "Boots of Elvenkind", [5000], [7], None),
    ("thief", "Mirt the Merchant", "Bracers of Archery", [7500], [10], None),
    ("thief", "Mirt the Merchant", "Short Sword of Quickness", [4500], [14], None),
    ("thief", "Mirt the Merchant", "Girdle of Hill Giant Strength", [6000], [14], None),
    ("thief", "Mirt the Merchant", "20 Venom Arrows (as often as you like)", [400], [3], None),
    ("thief", "Mirt the Merchant", "Short Bow of Lightning +3", [15000], [20], None),
]

# The mission of a reward of Khelben and its stage of the market
# (dtugear.c: stage 1 missions 3 and 4, 2 missions 7 and 8, 3 missions 9 and
# 10, 4 mission 11, 5 mission 14, 6 mission 16, 7 missions 20 and 21, 8
# mission 29; docs/re/scripts.md section 8)
MISSION_STAGE = {3: 1, 7: 2, 10: 3, 11: 4, 14: 5, 16: 6, 20: 7, 29: 8}

# The market stages: the reward of Khelben that adds the goods (the mission
# that the player gets, docs/re/scripts.md, section 8), in game terms
GEAR_STAGES = {
    0: "when Khelben sends you to the crypts of the Ghoulmaster (after the kobold chief)",
    1: "when Khelben sends you after the Shadow Thieves (after the Spider Piece of the crypts)",
    2: "when Khelben sends you to the tombs of Mulhorand (after the Shadow Thieves)",
    3: "when Khelben sends you to the mine (after the Scarab Beetle of the tombs)",
    4: "when Khelben sends you to the temple of Myrkul (after the Spider Piece of the mine)",
    5: "when Khelben sends you to the drow city (after the temple of Clangeddin)",
    6: "when Khelben sends you after the Spider Piece of the drow (after the first drow mission)",
    7: "when Khelben sends you to the beholder caves (after the illithids)",
    8: "when Khelben sends you to Ibbalar (after the Crystal Rose and the Madlands)",
}

# The names of the levels (the line of DTU.MSN), as the level files name
# them (the level name of tools/lvl.py on the unpacked .LVL files, checked
# 2026-11-27)
LEVEL_NAMES = {
    11: "the Dwarven Mine",
    13: "the Tomb of Ankh-Kephra",
    14: "the Pyramid",
    15: "the Third Tier Hub",
    16: "the Temple of Myrkul",
    17: "the Temple of Clangeddin",
    19: "the Crypts of Clangeddin",
    21: "the Temple of the Drow",
    27: "the Garden of The Drow",
}

# The places in game terms (the levels of the code tables)
GEAR_LEVEL_TEXT = {
    16: LEVEL_NAMES[16],
    21: LEVEL_NAMES[21] + " (the second drow level)",
    27: LEVEL_NAMES[27],
}

# The prices of Aleena Paladinstar for the scroll sets with the spells of the
# port (patch_aleena of engine/main/dtuscript.c, docs/re/spells.md 15.4)
ALEENA_PRICE = {56: 1000, 57: 5000}



def gear_effect(name, d):
    """The effect of a new armor line in words, with the numbers of
    dtugear.h (the defines named for the line number)."""
    ids = dict((v, k) for k, v in d.items() if k in (
        "DTU_GEAR_MIRROR_SHIELD", "DTU_GEAR_BOOTS_FREE", "DTU_GEAR_ARCHER", "DTU_GEAR_CLOAK_FIRE",
        "DTU_GEAR_BELT_DWARF", "DTU_GEAR_RING_REGEN", "DTU_GEAR_PLATE_FIRE", "DTU_GEAR_CLOAK_DISPLACE",
        "DTU_GEAR_SASH_MAGI"))
    return {
        "DTU_GEAR_MIRROR_SHIELD": "A shield +1. A spell missile of a creature that hits you from the front (up to "
                                  "60 degrees from where you look) flies back to the creature with the power of its "
                                  "caster, and a kill is yours. The eye rays of a beholder are spell missiles. Area "
                                  "spells such as Fireball burst at the shield; arrows, bolts, thrown weapons and "
                                  "traps hit you as before. A Lightning Bolt that comes back from a wall to your "
                                  "front, your own or one the shield turned, is turned again, as long as it flies.",
        "DTU_GEAR_BOOTS_FREE": "Free Action while worn: Hold Person, Hold Undead and the stun of Color Spray "
                               "do not work on you, and nothing slows your movement.",
        "DTU_GEAR_ARCHER": "Arrows and bolts get +%d to hit and +%d damage." % (d["DTU_GEAR_ARCHER_HIT"],
                                                                            d["DTU_GEAR_ARCHER_DAMAGE"]),
        "DTU_GEAR_CLOAK_FIRE": "Resist Fire while worn: fire damage to you is halved.",
        "DTU_GEAR_BELT_DWARF": "Your Constitution becomes %d, and a human gets infravision (the other races have "
                               "it)." % d["DTU_GEAR_DWARF_CON"],
        "DTU_GEAR_RING_REGEN": "Heals 1 hit point each %d seconds while worn." % d["DTU_GEAR_REGEN_TIME"],
        "DTU_GEAR_PLATE_FIRE": "Plate mail +2 (magic heavy armor) with Resist Fire: fire damage to you is halved.",
        "DTU_GEAR_CLOAK_DISPLACE": "You seem to stand beside your true place: the first weapon attack that would hit "
                                   "you misses (arrows and bolts too, not spells). The cloak is ready again %d seconds "
                                   "later and shimmers to tell you." % d["DTU_GEAR_DISPLACE_TIME"],
        "DTU_GEAR_SASH_MAGI": "For a mage: %d more spell slot at each spell level that you can cast (after the "
                              "doubling of a Ring of Wizardry), and saving throws against spells %d better."
                              % (d["DTU_GEAR_SASH_SLOTS"], d["DTU_GEAR_SASH_SAVE"]),
    }.get(ids.get(name), "")


class ScrollRecord(Record):
    """A spell line as a scroll of the market (the name "Scroll of ...")."""

    def __init__(self, rec):
        Record.__init__(self, rec.line)
        self.shown = "Scroll of " + rec.name()


class GearData:
    """The new gear as the pages need it."""

    def __init__(self, g, tab):
        self.g = g
        self.d = g["d"]
        self.armor, self.misc = {}, {}
        na, nm = self.d["DTU_GEAR_FIRST_ARMOR"], self.d["DTU_GEAR_FIRST_MISC"]
        for line in g["lines"]:
            if line.startswith("$ARMOR"):
                self.armor[na] = Record(line)
                na += 1
            else:
                self.misc[nm] = Record(line)
                nm += 1
        self.tab = tab
        # the wand lines with the new text (a copy of the line with the keys of the code)
        self.wands = {}
        for i, name13, name, spell in g["wands"]:
            r = tab["$WEAPON"][i]
            if r.name13() != name13:
                raise SystemExit("dtugear.c: weapon %d is %r, not %r" % (i, r.name13(), name13))
            line = re.sub(r'weapon_name="[^"]*"', 'weapon_name="%s"' % name, r.line)
            line = re.sub(r"\ball_use\b", "mage_use", line)
            line = re.sub(r"\bvalue=\d+", "value=3000", line)
            self.wands[i] = (Record(line + " pc_spell=%d" % spell), spell)
        self.values = {i: v for i, _, v in g["values"]}
        # the Frost Brand: the line it copies, with its name and value
        fid, _, src, src13, fname, fvalue = g["frost"]
        r = tab["$WEAPON"][src]
        if r.name13() != src13:
            raise SystemExit("dtugear.c: weapon %d is %r, not %r" % (src, r.name13(), src13))
        line = re.sub(r'weapon_name="[^"]*"', 'weapon_name="%s"' % fname, r.line)
        line = re.sub(r"\bvalue=\d+", "value=%d" % fvalue, line)
        self.frost_id = fid
        self.frost = Record(line)
        # the daggers: copies of the Dagger +2 with their name, value and bonus
        self.daggers = {}
        for did, _, src, src13, dname, dvalue, bonus, thief in g["daggers"]:
            r = tab["$WEAPON"][src]
            if r.name13() != src13:
                raise SystemExit("dtugear.c: weapon %d is %r, not %r" % (src, r.name13(), src13))
            line = re.sub(r'weapon_name="[^"]*"', 'weapon_name="%s"' % dname, r.line)
            line = re.sub(r"\bvalue=\d+", "value=%d" % dvalue, line)
            line = re.sub(r"\bto_hit_bonus=\d+", "to_hit_bonus=%d" % bonus, line)
            line = re.sub(r"\bdamage_bonus=\d+", "damage_bonus=%d" % bonus, line)
            if thief:
                line = re.sub(r"\b(Fighter_use|mage_use)\b", "", line, flags=re.I)
            self.daggers[did] = Record(line)
        self.venom_id = g["d"]["DTU_GEAR_VENOM"]
        # the bows and arrows: the Arrows +1 and +2 with their to-hit bonus
        # and value, and the new arrow lines (copies of them)
        self.arrow_plus = {}
        self.arrow_lines = {}
        hits = {i: (n13, h) for i, n13, h in g["arrow_hits"]}

        def arrow_line(src, name, value, hit, damage, icon=None, fire=0):
            line = tab["$WEAPON"][src].line
            line = re.sub(r'weapon_name="[^"]*"', 'weapon_name="%s"' % name, line)
            line = re.sub(r'(?<![\w#])name="[^"]*"', 'name="%s"' % name, line)
            line = re.sub(r"\s(value|to_hit_bonus|damage_bonus)=\S+", " ", line)
            line += " value=%d to_hit_bonus=%d damage_bonus=%d" % (value, hit, damage)
            if icon is not None:
                line = re.sub(r"\sicon=\S+", " ", line) + " icon=%d" % icon
            if fire:
                line += " fire_damage_dice=%d #fire_damage_dice=1" % fire
            if " magic" not in line:
                line += " magic"
            return Record(line)
        for i, (n13, h) in hits.items():
            r = tab["$WEAPON"][i]
            if r.name13() != n13:
                raise SystemExit("dtugear.c: weapon %d is %r, not %r" % (i, r.name13(), n13))
            self.arrow_lines[i] = arrow_line(i, n13, self.values.get(i, 0), h, r.num("damage_bonus", 0))
            self.arrow_plus[i] = h
        for aid, name, src, src13, value, hit, damage, icon, fire, plus in g["arrows"]:
            if tab["$WEAPON"][src].name13() != src13:
                raise SystemExit("dtugear.c: weapon %d is %r, not %r" % (src, tab["$WEAPON"][src].name13(), src13))
            self.arrow_lines[aid] = arrow_line(src, name, value, hit, damage, icon, fire)
            self.arrow_plus[aid] = plus
        # the Venom Arrows of the class gear (a copy of the Arrow, magic)
        afrom, a13, aname, avalue = g["class_arrow"]
        self.venom_arrows_id = self.d["DTU_GEAR_VENOM_ARROWS"]
        self.arrow_lines[self.venom_arrows_id] = arrow_line(afrom, aname, avalue, 0, 0, self.d["DTU_GEAR_ICON_VENOM"])
        self.arrow_plus[self.venom_arrows_id] = 1
        # the lines of the class gear after the new gear (armor 62 and up,
        # misc 131 and up, the reader of the tables in the same order)
        na, nm = self.d["DTU_GEAR_FIRST_ARMOR"] + len(self.armor), self.d["DTU_GEAR_FIRST_MISC"] + len(self.misc)
        self.armor_class_gear, self.misc_class_gear = {}, {}
        for line in g["class_lines"]:
            if line.startswith("$ARMOR"):
                self.armor_class_gear[na] = Record(line)
                na += 1
            else:
                self.misc_class_gear[nm] = Record(line)
                nm += 1
        if self.d["DTU_GEAR_BOOTS_ELVEN"] not in self.armor_class_gear or \
                self.d["DTU_GEAR_PEARL_POWER"] not in self.misc_class_gear:
            raise SystemExit("dtugear.c: the class gear lines are not at the numbers of dtugear.h")

    def item(self, cls, i):
        """The record of class 5, 15 or 16 number i, new or of 1.3."""
        if cls == 15 and i in self.armor:
            return self.armor[i]
        if cls == 16 and i in self.misc:
            return self.misc[i]
        if cls == 15 and i in self.armor_class_gear:
            return self.armor_class_gear[i]
        if cls == 16 and i in self.misc_class_gear:
            return self.misc_class_gear[i]
        if cls == 5 and i == self.frost_id:
            return self.frost
        if cls == 5 and i in self.daggers:
            return self.daggers[i]
        if cls == 5 and i in self.wands:
            return self.wands[i][0]
        if cls == 5 and i in self.arrow_lines:
            return self.arrow_lines[i]
        if cls == 17:       # a spell scroll of the market
            return ScrollRecord(self.tab["$SPELL"][i])
        return self.tab[{5: "$WEAPON", 15: "$ARMOR", 16: "$OBJECT"}[cls]][i]

    def value(self, cls, i):
        if cls == 5 and i in self.values:
            return self.values[i]
        return self.item(cls, i).num("value", 0)

    def stages_of(self, cls, i):
        return sorted(set(row[0] for row in self.g["market"] if row[2] == cls and row[3] == i))

    def where(self, cls, i):
        """Where the setting gives item (cls, i): market stages, the Mace,
        the Flame Tongue, the treasury, the drops."""
        out = []
        script, potion, mace = self.g["daland"]
        lvl, ft, _ = self.g["flame"]
        if cls == 5 and i == mace:
            out.append("Daland gives it in %s when the altar of Kelemvor is consecrated" % GEAR_LEVEL_TEXT.get(16, "the temple"))
        if cls == 5 and i == ft:
            out.append("found in %s, in a prison cell of the troll wing" % GEAR_LEVEL_TEXT.get(lvl, "a drow level"))
        if cls == 5 and i == self.venom_id:
            out.append("found in %s, in a locked closet: a thief picks the lock, a Knock spell or the Gloves of "
                       "Dexterity open it for the other classes" % GEAR_LEVEL_TEXT.get(self.g["venom"][0], "a drow level"))
        if any(row[0] == cls and row[1] == i for row in self.g["dragon"]):
            out.append("a treasure of the red dragon (below)")
        for s in self.stages_of(cls, i):
            out.append("the market of Waterdeep " + GEAR_STAGES[s])
        if cls == 16 and i in self.misc:
            out.append("left by slain creatures (below)")
        return "; ".join(out) if out else "-"


def frost_effect(d):
    return ("A long sword +3. Against fire creatures (the red dragon, her hatchlings, fire mephits) +%d to hit "
            "and damage. While in your hand fire damage to you is halved." % (3 + d["DTU_GEAR_FROST_BONUS"]))


def dagger_effect(d, venom):
    if venom:
        return ("A dagger +2. A hit poisons the creature unless it saves against poison: 1 hit point each second for "
                "%d seconds (a new hit starts the time again). Undead and extraplanar creatures are immune; the poison "
                "does not stop the regeneration of a troll." % d["DTU_GEAR_POISON_TIME"])
    return ("A dagger +3 for thieves. A backstab with it multiplies the damage by %d more (a thief of level 1: times 3 "
            "instead of 2)." % d["DTU_GEAR_ASSASSIN_STEP"])


# Spell lines that are not real player spells (Sources.md).
OTHER_SPELL_LINES = {
}

WEAPON_INTERNAL = {
    0: "the empty hand (1d2 plus strength, from the character record, not the line)",
    1: "Flare", 2: "template of a spell cast with the hands", 3: "template of a wand cast",
    4: "template of a scroll cast", 5: "template of a spell arrow", 10: "named DO NOT USE",
    15: "Bomb (trap)", 16: "Lava Flames (trap)", 17: "Mini Flames (trap)",
    25: "Slow Bomb (trap)", 26: "Fast Bomb (trap)", 63: "Wand of Genericness", 64: "Wand of Genericness",
    65: "Wand of Genericness", 66: "Rod of Genericness", 67: "Rod of Genericness", 68: "Rod of Genericness",
    76: "Temp", 77: "Wand (creature)", 78: "Acid Spittle (creature)", 80: "Myconid Spores (creature)",
    81: "Beholder Burn (creature)", 82: "Spectator Burn (creature)", 83: "Spectator Burn (creature)",
    84: "Spectator Burn (creature)", 85: "Spectator Burn (creature)", 86: "Spectator Burn (creature)",
    87: "Spectator Burn (creature)", 88: "Spectator Burn (creature)",
}

# 0x2AB9F, 0x2B1F2, 0xB337E, 0xAFB55, 0xB038F (combat.md B6a, audit/combat.md C5)
WEAPON_NOTES = {
    8: "Homes in on the target.",
    45: "A backstab with a hit roll of 19 or 20 does +3 to 6 damage.",
    13: "In the hand, a quest script runs when you use it, attack with it, try to drop it or go to "
        "another level. It can refuse to be dropped: on d18 above your Intelligence or d7 above your "
        "first class level. USE, for a Lawful Good character: Continual Light around you, Free Action "
        "and Protection From Evil, unless the quest has stopped it.",
    54: "In the hand: Chill Touch, Resist Fire, Feather Fall, Free Action, Jump, Detect Magic, Haste, "
        "Improved Invisibility and Infravision with no end, and Find/Remove "
        "Traps and Climb Walls 95%.",
    69: "Attacks twice as fast while it is in the hand: a swing of 0.5 s. It is no Haste: you do not "
        "move faster, and with Haste the swing stays 0.5 s. The original 1.3 game gives it no power; "
        "ASCENT corrects this.",
    70: "Against undead: double damage on d100 <= 5 x (level of the undead - 4); otherwise the undead "
        "is destroyed.",
    71: "+2 to hit and +2 fire damage against a regenerating target, +4 and +4 against undead.",
    75: "No bonus to hit, despite the +2.",
    74: "No bonus to hit, despite the +1.",
}

SPEED_BOW_NOTE = ("Shoots twice as fast while it is in the bow slot: half the wait between arrows. It is no "
                  "Haste: you do not move faster, and with Haste the wait is not halved again. The original "
                  "1.3 game gives it no power; ASCENT corrects this.")

# 0x37A6A, combat.md B6
POTIONS = {
    72: ("Dexterity 18 for 10 minutes, if yours is lower.", "case 0x48 of 0x37A6A"),
    73: ("If any spell or potion effect runs: all of them end.", "case 0x49"),
    74: ("Strength 18 for 1 minute to (your first class level) minutes, if yours is lower; a fighter "
         "also gets a random percentile from 0 to 99.", "case 0x4A"),
    75: ("+6 to 27 hit points, up to your maximum.", "case 0x4B"),
    76: ("Resist Fire for 10 minutes: fire damage halved.", "case 0x4C"),
    77: ("The spell Fly on you: 10 to 60 minutes + 10 per level.", "case 0x4D"),
    78: ("Free Action for 50, 60, 70 or 80 minutes.", "case 0x4E"),
    79: ("+4 to 10 hit points, up to your maximum.", "case 0x4F"),
    80: ("Strength 19 for 50, 60, 70 or 80 minutes, if yours is lower.", "case 0x50"),
    81: ("Intelligence 5 to 7 for 50, 60, 70 or 80 minutes. While it runs, parts of the map are "
         "forgotten now and then. The original 1.3 game rolls this each frame, so it depends on the "
         "frame rate; ASCENT corrects this.", "case 0x51"),
    82: ("Infravision for 10 to 80 minutes, in steps of 10.", "case 0x52"),
    83: ("Invisibility for 30, 40, 50 or 60 minutes.", "case 0x53"),
    84: ("Invulnerability for 5 to 20 minutes: no spell damage, armor class -2, and creatures below "
         "level 4 or without a magic weapon cannot hurt you.", "case 0x54, 0x2A24A"),
    85: ("The spell Jump on you: 1d3 minutes + 1 per level.", "case 0x55"),
    86: ("For one hour, each attack roll and each damage roll of your weapon flips a coin. On a win "
         "the roll is the best one (a natural 20, a critical hit; or the highest damage), and the "
         "luck ends. The original 1.3 game takes the coin from the day clock; ASCENT corrects this "
         "with a random coin.",
         "case 0x56, 0x2A87B (P+0x1A58, day clock bit P+0x83)"),
    87: ("Haste for 5 to 20 minutes.", "case 0x57"),
    88: ("+2 to 18 hit points and Bless for 70 to 120 minutes; the extra hit points go when Bless "
         "ends. If you already have more hit points than your maximum: \"You don't feel so good.\" and "
         "a system shock roll ([[Classes]]): d100 above the value for your Constitution kills you.",
         "case 0x58, table 0x2FACC"),
}

# 0x37A6A cases 0x66, 0x6A, 0x7F; misc 126 has no case
SCROLLS = {
    102: "USE: read its contents.",
    106: "USE: the screen shakes, then each of these with a 50% chance: Confusion, Continual Light, "
         "Scare, Hold Person and Fireball on you, the map forgotten, and each ability score set to a "
         "random value from 1 to its value. Then damage from 0 to your hit points - 1. Not used up. "
         "Quest item. The original 1.3 game does less than half a point of damage; ASCENT corrects "
         "this.",
    126: "Quest item. Using it does nothing.",
    127: "USE: shows a clue. Quest item.",
}

SOURCES_FINDINGS = [
    ["The table text of Fireball, Lightning Bolt and Melf's Acid Arrows has 0-sided dice, which add "
     "0. The table fixup gives them 6, 6 and 4 sides, so 1.3 never rolls the 0-sided dice; only "
     "-rawtables of ASCENT shows the rule.",
     "The count of the 0-sided dice is set by the cases 0x8E592, 0x8EEAB, 0x8EFE0. 0x91BC3 divides by "
     "the sides with `idiv dword ptr [esp]` (0x91C25, 0x91C8E, 0x91CEC, 0x91D4E). The divide error "
     "handler of div0.asm (div0_init(1) at 0xB8323, handler 0xF6D94, saturate mode, no lists) "
     "returns remainder 0 and, because it decodes the SIB form wrongly, resumes after the next "
     "instruction `inc edx`. So each 0-sided die adds 0. Emulated from 0xF6E18 for all four "
     "addresses."],
    ["Arrows +1 do 1d6+1, Arrows +2 1d6+2, Magic Arrow 1d6.",
     "The lines have no `damage_dice`. The `$WEAPON` reader 0x209F3 calls 0x207E7 first, which sets "
     "sides 6 and count 1 (0x208D7, 0x208DB). 0x2AF5E rolls them (0x2B077) and adds `damage_bonus` "
     "(0x2B1E7)."],
    ["The Short Sword of Quickness never casts Haste and has no Haste in 1.3.",
     "`PC_spell` (Weapon_info+0x3A) is read only in 0x56A0D (0x56B1C, 0x56CA4). Its one caller 0x44300 "
     "calls it only for a spell (type 0x11) or a weapon with `attack_type` 0 (0x44423 to 0x44445). The "
     "sword is a melee weapon (default `attack_type` 1). ASCENT makes its swings twice as fast, with no "
     "Haste effect (docs/re/combat.md, section A8)."],
    ["The Short Bow of Speed has no Haste in 1.3.",
     "The line has `pc_spell1=21`. The image has no keyword `pc_spell1` (`PC_spell` 0x1121AD, `PC_spell_2` "
     "0x112742), and the code of the worn items 0xAFB55 does not read the bow slot. ASCENT makes its "
     "arrows twice as fast, with no Haste effect (docs/re/combat.md, section A8)."],
    ["Arrows, bolts and thrown weapons have no wait between shots in 1.3.",
     "The fire timer of 0x56A0D starts again when the function was not called in the last two frames "
     "(0x56BB0), and each shot ends the request, which needs a new key press. `fire_wait` is read "
     "only at 0x4421B (melee swing) and 0x56C01 (this timer)."],
    ["No class or level gives more attacks.",
     "The melee gate 0x41E36 reads the game time, the time of the last attack 0x13B74B, the swing "
     "time of the model and the Haste bit (0x41EC3). 0x13B74B has no other reference."],
    ["Gem of Curing: one use at the start, one more for each mission.",
     "0x3891B: the use counter P+0x1D4D73 must be <= 0x8445A(), the mission number 0x13E588 (0 at a "
     "new game, 0x82BC7; the next mission at Khelben, 0x83E5E). 0x389E0 adds 1. No other code writes "
     "the counter."],
    ["The Scroll shows Khelben's mission summary; found Scrolls show their own text.",
     "Case 0x66 calls 0x9DC42 with the count of the item. 0x9CEA3: count 0 (the Scroll of a new "
     "character) takes the mission blurb table 0x1451E5[mission]; another count n takes the text "
     "table 0x144FE5[n] (0x144DE5[n], longer texts, in the high resolution). Both hold entries "
     "of `FETXT` (for example 271 \"Quest One: Corner the Kobolds\", 141 \"A Brief History of "
     "Undermountain\"). The levels give Scrolls counts 2 to 27."],
    ["The thief scroll shows a picture clue.",
     "Case 0x7F calls 0x9DC42(1): text 0x144FE5[1] = `FETXT` 270, which is the picture markup `~pc3`."],
    ["The Lantern gives no light.",
     "Its `player_light` (Misc_info at 0x19BAF0 + 0x44 x item) is written only by the reader 0x23780 "
     "(0x23B5D); `lightcast` is stored only. 0x37A6A has no case for item 0 (0x37DAE)."],
    ["Spider Piece 21 does nothing.",
     "0x37A6A has no case for item 0x15 (0x37DA1 jumps to the end 0x396A0). The mission switch "
     "0x83EF5 does not ask for it."],
    ["Thief skill points: 20 per thief level, 20 more at the first visit.",
     "Talking to Mirt (TEX 905) in level 0: 0xABB53 gives (thief level - P+0x1A53) x 20 points, + 20 "
     "if P+0x1A53 is 0, into P+0x1AB3, and opens the screen 0xB5B49. 0xB44B4 adds 1 per point, not "
     "above 95; 0xB4ECF limits one skill to half of the points (at least 1)."],
    ["Where 1.3 enforces the class flags.",
     "0x3533C tests the class bits of an item entry (byte 3) against the classes (0x352F7). It is "
     "called when the cursor takes an item (0x30344, 0x312C3: no wear bits, so no slot takes it), for "
     "the hot key boxes (0x304B7, 0x31A67), the paper doll figure (0x35CFB), the hand (0x5453A) and the "
     "market (0x87649, 0x877BD). The texts 825 \"You cannot use this object.\" and 828 \"You cannot use "
     "this armor.\" are pickup refusals (0x28220, 0x284D7) when the item cannot be added."],
    ["The Fireball blindness of the readme comes only from a fire trap.",
     "The blind and deaf code for Fireball is in the area function 0x90CEF. Every caller tests the area "
     "flag of the spell (Spell_info+0x36 bit 0), which Fireball (type TARGET) does not have, except "
     "0xA6F51 and 0xA6F61: the fire trap object 0x29 of level 0 (\"You set off a fire trap!!!\")."],
    ["Fear moves the view of a frightened player.",
     "0x8E2D8 calls 0x13407 for the player: it gates a creature at the player and makes it the viewer; "
     "at the end of Fear 0x903D4 deletes it and moves the player to its segment."],
    ["Sleep resistance of people.",
     "0x8F840: a creature with Robot_info+0xD9 bit 3 (names starting with rf, rm, av or dro, set at "
     "0x1F448) resists on d10 <= 9."],
]

RULE_SOURCES = [
    ["Character: 7 bonus points", "0x89B95"],
    ["Character: start hit points, two classes", "0x8BA3A"],
    ["Character: start items and spells", "0x8BB91, 0x8BEBC, lists at 0x13F6CD"],
    ["Level cap 18 (a level is raised while level + 1 < 18)", "0xAF43A"],
    ["Hit points of a new level: rand() % (die - die/2 + 1) + die/2", "0xAF694"],
    ["THAC0 at level + 1, index at most 17", "0xAF605"],
    ["Saving throws: class and level only", "0xAE596"],
    ["Infravision by race", "0x54632"],
    ["Sleep resistance of elves and half-elves", "0x8F766"],
    ["Scare save of elves, half-elves and clerics", "0x8F467"],
    ["Move Silently of elves, drow and halflings", "0xB4196"],
    ["Find/Remove Traps 50% of dwarves", "0x8BDE5"],
    ["Infravision spell and ring: race byte 0 (human, half-elf)", "0x8EBE8, 0xB0968"],
    ["Racial search for secret doors", "0xB3ECE"],
    ["Mage spell refused in armor without MAGIC_OK", "0x41A4D"],
    ["Spell slots filled by a rest, a new character and the way from Waterdeep into the dungeon; not by a level change or a load",
     "0x3551B (0x3608B, 0x8BFFE, 0x42284), 0x3575B"],
    ["Wisdom divides Fear, Scare, Hold Person and the Color Spray stun on the player",
     "0x8DAA2, 0x8E338, 0x8E8FA, 0x8F57F"],
    ["Charisma market prices", "0x86F75"],
    ["Thief skills", "0xB3038, 0xB31BD (Open Locks), 0xB3400, 0xB4196 (Move Silently), 0xB333F (Hide In "
     "Shadows), 0x951DE (Climb Walls), 0xB337E (backstab), worn items 0xAFB55"],
    ["Turn undead; the Cloak of Lathandar as a cleric of level 7", "0xADC46, table 0xA6487; 0xADCB3"],
    ["Rest place: again after 300 s", "0x3E1F6"],
    ["Armor class order and limits", "0xAFB55, armor table 0x145C04"],
    ["Melee swing max(fire_wait, 1.0) s, Slow doubles it", "0x441E9"],
    ["Missile fire rate fire_wait (default 0.25 s, 0x207E7): the timer of 1.3 never limits it", "0x56A0D, 0x56BB0"],
    ["Weapon resistant creatures and the magic flag", "0x29D9D"],
    ["Thrown and other missiles take the crossbow bonus; arrows the bow bonus", "0x2AB0A"],
    ["Critical hit rolls twice; fumble drop", "0x29A9C, 0xAE837"],
    ["Wand and scroll cast, charges", "0x56A0D, 0xB7084"],
    ["Caster level of a spell, wand or scroll", "0x91EC1"],
    ["Spell items learned on pick up", "0xB69AD"],
    ["Spell items in hot key boxes (39, 40, 53 for all)", "0x3533C"],
]

NOT_IN_ASCENT = [
]

UNUSED_ITEM_VALUES = [
    "Sword of W'hin: `ac_adjustment`, `adjust_strength`, `adjust_dexterity`. Its powers are code for "
    "the item number.",
    "Flame Arrow: `levels_per_use=5`.",
    "Ring of Armoring: `pc_spell=2` (Armor). Its power is code for the item number (0xB0014, "
    "0xB01B6 set a flag that 0xAFAD1 reads for a mage).",
    "Ring of Wizardry: `pc_spell=42`. Its power is code for the item number.",
    "Glim-Gauntlet: `player_light=1`. Lantern: `player_light=3`, `lightcast=3`.",
    "Gloves, Glim-Gauntlet, Gauntlets of Ogre Power, Elven Cloak: the AC bonus is in the executable "
    "table 0x145C04 only; the lines have no `armor_class_decrement`. Where a line has one, it agrees "
    "with the table.",
    "Potions: `pc_spell2`, `duration` and the `set_` keywords. Each potion is code for its number.",
    "Worn items: `pc_spell`, `set_strength` and similar. Each effect is code for the item number.",
    "Magic Missile: `damage_bonus=1`.",
]


# ---------------------------------------------------------------------
# Table check

RULE_KEYS = {
    "$WEAPON": ["name", "weapon_name", "damage_dice", "#damage_dice", "damage_bonus", "to_hit_bonus",
                "fire_damage_dice", "#fire_damage_dice", "pc_spell", "uses", "fire_wait", "attack_type", "value"],
    "$ARMOR": ["name", "armor_class_decrement", "value"],
    "$OBJECT": ["name", "value", "to_hit_bonus"],
    "$SPELL": ["name", "level", "range", "radius", "duration", "level_duration", "type",
               "damage_dice", "#damage_dice", "fire_damage_dice", "#fire_damage_dice", "fire_damage_level",
               "acid_damage_level", "elec_damage_level"],
}
RULE_FLAGS = ["all_use", "fighter_use", "thief_use", "mage_use", "cleric_use", "magic", "magic_ok",
              "arrow", "bolt", "self_cast_ok", "cast_in_front", "homing", "bow", "crossbow"]


def check_tables(ref, directory):
    """Compare the rule values of each level table with YPINN.BIN. Returns
    a list of (file name, line count, differences)."""
    out = []
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
        out.append((os.path.basename(path), n, diffs))
    return out


# ---------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="Write the wiki pages of the game rules")
    ap.add_argument("game_dir", help="the installed game (YPINN.BIN, DTUGAME.EXE 1.3)")
    ap.add_argument("out_dir", help="where the pages go")
    ap.add_argument("--tables", help="directory of unpacked level tables to compare with YPINN.BIN")
    ap.add_argument("--no-icons", action="store_true", help="no icon column and no icon files")
    a = ap.parse_args(argv)

    sheet = None
    icon_dir = os.path.join(a.out_dir, "icons")
    if not a.no_icons:
        check_icon_dir(icon_dir)
        sheet = IconSheet(a.game_dir)

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

    check = check_tables(tab, a.tables) if a.tables else None
    # The level names of the mission list (lines "name.lvl ;n")
    mission = []
    msn = os.path.join(a.game_dir, "DTU.MSN")
    if os.path.exists(msn):
        with open(msn, "rb") as f:
            mission = re.findall(r"(\w+)\.lvl", f.read().decode("latin-1").lower())
    gear = GearData(gear_tables.read(), tab)
    p = Pages(tab, ct, check, mission, sheet.count if sheet else 0, gear)
    pages = {
        "Home.md": p.home(), "_Sidebar.md": p.sidebar(), "Races.md": p.races(), "Classes.md": p.classes(),
        "Weapons.md": p.weapons(), "Armor.md": p.armor(), "Magic-Items.md": p.magic_items(),
        "Potions.md": p.potions(), "Wands.md": p.wands(), "Scrolls.md": p.scrolls(),
        "Wizard-Spells.md": p.spell_page("m"), "Priest-Spells.md": p.spell_page("c"),
        "New-Gear.md": p.new_gear(),
        "Encounters.md": p.encounters(), "Monster-Behavior.md": p.monster_behavior(),
        "Sources.md": p.sources(),
    }
    # Player pages carry no code detail: no addresses and no table keywords
    code_words = re.compile(r"0x[0-9A-Fa-f]+|self_cast_ok|cast_in_front|fire_wait|damage_bonus|GameTime"
                            r"|Dtu_fix|pc_spell|YPINN|DTUGAME|\.c\b|_use\b")
    os.makedirs(a.out_dir, exist_ok=True)
    for name, text in pages.items():
        if "\u2014" in text:
            raise SystemExit("%s: em dash in the text" % name)
        if name != "Sources.md":
            m = code_words.search(text)
            if m:
                raise SystemExit("%s: code detail on a player page: %r" % (name, m.group(0)))
            if re.search(r"\bunknown\b", text, re.I):
                raise SystemExit("%s: an unknown value on a player page" % name)
        with open(os.path.join(a.out_dir, name), "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("%-18s %3d table rows" % (name, p.counts.get(name[:-3], 0)))
    if sheet:
        os.makedirs(icon_dir, exist_ok=True)
        keep = set("icon-%d.png" % n for n in p.used_icons)
        for old in glob.glob(os.path.join(icon_dir, "icon-*.png")):
            if os.path.basename(old) not in keep:
                os.remove(old)
        for n in sorted(p.used_icons):
            sheet.write(n, os.path.join(icon_dir, "icon-%d.png" % n))
        print("%-18s %3d icons" % ("icons/", len(p.used_icons)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
