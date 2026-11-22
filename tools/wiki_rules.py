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

usage: python -I tools/wiki_rules.py GAME_DIR OUT_DIR [--tables DIR]

GAME_DIR  the installed game: YPINN.BIN and DTUGAME.EXE of version 1.3.
OUT_DIR   the pages are written here (Home.md, _Sidebar.md, ...).
--tables  a directory of unpacked level tables (*.BIN, gamedata/unpacked):
          the item and spell lines of each table are compared with
          YPINN.BIN; the result goes to Sources.md and to the console.

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
                "The Sword of W'hin gives the same effect. The original 1.3 game fires the spell as a "
                "missile at others, so casting it does nothing; ASCENT corrects this.",
            tech="The case acts only when the target is the caster (0x8D633); the line is not "
                 "self-cast, so the missile never hits the caster. The cold dice of the line are "
                 "never rolled."),
    6: dict(src="0x8D6D4", dur="See effect", save="Only a target above your level, or of level 6 or more",
            target="1d6 missiles; each hits everyone within {r} ft of where it lands",
            eff="Not on blind creatures. A target of your level or lower falls asleep for 2 to 8 "
                "minutes; a player is also blind for that time. Up to 2 levels above yours: blind for "
                "1d4 minutes. Higher: stunned for 1 minute, unless it has Free Action."),
    7: dict(src="0x8DB82", dur="2 minutes + 1 per level", save="Negates",
            target="1d4 missiles + 1 per level; each affects the creature it hits",
            eff="A creature stops being hostile. A player loses control."),
    8: dict(src="0x8DC1D, 0x90CEF", dur="No end", save="Negates", target="Everyone within {r} ft of you",
            eff="A light (brightness 60) appears where you stand and stays. Each creature or player in "
                "the area, except you and the blind, is blinded with no end unless it saves. "
                "Light-sensitive creatures would also take the damage of the spell, which is 0."),
    10: dict(src="0x8DE4B", dur="Instant", save="None", target="You",
             eff="+1d8 hit points, up to your maximum. No effect on undead or extraplanar creatures."),
    11: dict(src="0x8DF37", dur="Instant", save="None", target="You",
             eff="+3 to 17 hit points, up to your maximum. No effect on undead or extraplanar creatures."),
    12: dict(src="0x8E019", dur="2 minutes per level", save="None", target="You",
             eff="DETECT_MAGIC",
             tech="Sets effect bit 6. The renderer reads it (0x670DB, 0x75726)."),
    13: dict(src="0x8E0B5, 0x56D36", dur="Instant", save="None", target="You",
             eff="In 1.3 the mage version is cast on yourself: all your spell effects and potion "
                 "effects end.",
             tech="The line has cast_in_front, which sets the self-cast bit (+0x37 bit 0, 0x253C7), "
                  "so 0x56A0D casts it on the player."),
    14: dict(src="0x8E2D8, 0x13407, 0x903D4", dur="1 minute per level", save="Negates", target="The creature the missile hits",
             eff="Not on undead or creatures that cannot be scared. The target flees. A creature drops "
                 "its weapon on d100 <= 65 - 5 x its level. A player is held and may drop the weapon; "
                 "on a player the time is divided by half the Wisdom. A frightened player also loses "
                 "the view: it follows a creature that runs off, and when Fear ends the player stands "
                 "where that creature went."),
    15: dict(src="0x8E464, 0x9DEDD", dur="1 minute per level", save="None", target="You",
             eff="You fall slowly, at most 7.5 ft per second. No message when it ends."),
    16: dict(src="0x8E55C, 0x91BC3", dur="Instant", save="Half", target="The creature the missile hits",
             eff="6 fire damage per level. Scenery that it hits can break.",
             tech="The case sets #fire_damage_dice = min(L, 10); the sides are 0 (fire_damage_dice=0 "
                  "in every table). Each 0-sided die adds 0 (see the divide error handler), so the "
                  "damage is fire_damage_level x L = 6L. Type TARGET: only the creature hit."),
    17: dict(src="0x8E5B1", dur="Instant", save="Half", target="The creature the missile hits",
             eff="No damage: the spell has no damage dice. Scenery that it hits can break."),
    18: dict(src="0x8E5D1", dur="1 minute + 1 per level", save="None", target="You",
             eff="Fire damage to you is halved, as with Resist Fire."),
    19: dict(src="0x8E665", dur="10 to 60 minutes + 10 per level", save="None", target="You",
             eff="You fly."),
    20: dict(src="0x8E6F2", dur="10 minutes per level", save="None", target="You",
             eff="Ends Hold. Hold Person and the stun of Color Spray do not work on you, and nothing "
                 "slows your movement."),
    21: dict(src="0x8E79C, 0x9DE13, 0x41EC3", dur="3 minutes + 1 per level", save="None", target="You",
             eff="Ends Slow. You move twice as fast. The original 1.3 game makes each melee swing half "
                 "again as long; ASCENT corrects this and halves the swing time."),
    22: dict(src="0x8E85A", dur="2 minutes per level", save="Negates", target="The creature the missile hits",
             eff="Only humanoid creatures and players; not undead, not with Free Action. The target "
                 "cannot move. On a player the time is divided by half the Wisdom."),
    23: dict(src="0x8EA21", dur="1d4 minutes + 1 per level", save="Only a target above level 2",
             target="The creature the missile hits",
             eff="Only undead of your level or lower. The target cannot move."),
    24: dict(src="0x8EB53", dur="4 minutes + 1 per level", save="None", target="You",
             eff="Creatures do not see you, even while you attack. Open Locks -15% while invisible."),
    25: dict(src="0x8EBE8", dur="2 hours + 1 per level", save="None", target="You",
             eff="Lets you turn on infravision (key N). Works only for a human or half-elf; the other "
                 "races have infravision already."),
    26: dict(src="0x8ECB4", dur="Until you attack", save="None", target="You",
             eff="Creatures do not see you. Ends when you make a weapon attack or cast a spell missile. "
                 "No effect while you have Improved Invisibility. Open Locks -15% while invisible."),
    27: dict(src="0x8ED20, 0x9EA62", dur="1d3 minutes + 1 per level", save="None", target="You",
             eff="You jump higher: jump strength 2.0 instead of 1.5."),
    28: dict(src="0x8FFE5, 0x26C2F", dur="Instant", save="None", target="You",
             eff="Has no effect: the spell is cast on yourself, so no missile flies. A Knock missile "
                 "would open the door it hits, also a locked one, unless the lock is magic.",
             tech="The case of 0x8CFFF has no code; the door code is in the wall hit 0x26C2F "
                  "(0x7E859), which a self-cast spell never reaches."),
    29: dict(src="0x8EDB2, 0x9DEAE", dur="10 minutes per level", save="None", target="You",
             eff="You float and move only up and down. You cannot cast spells while you levitate."),
    30: dict(src="0x8DC1D, 0x90CEF", dur="10 minutes per level", save="Negates", target="Everyone within {r} ft of you",
             eff="A light (brightness 20) appears where you stand for the duration. Each creature or "
                 "player in the area, except you and the blind, is blind for the duration unless it "
                 "saves. Light-sensitive creatures take 1d6 damage."),
    32: dict(src="0x8EE76, 0x91BC3", dur="Instant", save="Half", target="The creature the missile hits",
             eff="6 electric damage per level. No damage to creatures immune to electricity. Scenery "
                 "that it hits can break.",
             tech="As Fireball: #elec_damage_dice = min(L, 10) dice of 0 sides add 0; "
                  "elec_damage_level 6. The missile lives 2 s."),
    33: dict(src="0x8EED5, 0x56FF6", dur="Instant", save="Half",
             target="1 missile, +1 at levels 2, 4, 6 and 8 (at most 5)",
             eff="1d4 damage per missile. The missiles home in on the target when a mage casts them. "
                 "No damage to a target protected by Shield.",
             tech="damage_bonus=1 of the line is not added (0x91BC3 does not read it)."),
    34: dict(src="0x8EEFE", dur="Instant", save="Half", target="1d3 stones; each hits one creature",
             eff="1d4 damage per stone, 2d4 against undead."),
    35: dict(src="0x8EF49, 0xB0606", dur="5 minutes per level", save="None", target="You",
             eff="VESTMENT"),
    36: dict(src="0x8EFE0, 0x56FF6", dur="Instant", save="Half", target="The creature the missile hits",
             eff="4 acid damage per level. Acid stops regeneration.",
             tech="#acid_damage_dice = 2 (L/3) + 2 dice of 0 sides (no acid_damage_dice key in any "
                  "table) add 0; acid_damage_level 4. The missile lives (L/3 + 1) minutes."),
    37: dict(src="0x8F021", dur="Instant", save="Half", target="1 meteor per level (at most 5)",
             eff="1d4 damage per meteor. Scenery that it hits can break."),
    38: dict(src="0x8F078, 0x56D6A", dur="1 minute per level", save="None", target="You",
             eff="Bless (+1 to hit) on you only."),
    39: dict(src="0x8F116, 0x2AD36", dur="2 minutes per level", save="None", target="You",
             eff="Attacks of extraplanar creatures miss you; other creatures get -2 to hit you. Your "
                 "attack on an extraplanar creature ends the spell."),
    40: dict(src="0x8F1B2", dur="Until used up", save="None", target="You",
             eff="Absorbs 12 points of fire damage per level before you take any. Cast it again to "
                 "add more."),
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
             eff="Your armor class becomes at most 3, before the Dexterity adjustment. Magic Missile "
                 "does no damage to you."),
    46: dict(src="0x8F766", dur="5 minutes per level", save="See effect", target="The creature the missile hits",
             eff="Only targets of level 3 or lower; not undead or creatures immune to sleep. Each cast "
                 "has a pool of 4 to 16 (an even number). A creature costs a quarter of its maximum hit "
                 "points, a player 2 per level; if the pool is too small, the target stays awake. An "
                 "elf resists 9 times in 10, a half-elf 3 times in 10, people 9 times in 10. A sleeping "
                 "player is also blind. A melee hit wakes the sleeper and always hits."),
    47: dict(src="0x8FAD8", dur="3 minutes + 1 per level", save="Negates", target="The creature the missile hits",
             eff="Ends Haste. Armor class +4 (at most 10), -4 to hit, half move speed, and each swing "
                 "takes twice as long."),
    48: dict(src="0x8FBEE, 0xB3038", dur="3 minutes + 1 per level", save="None", target="You",
             eff="Climb Walls counts as 100%."),
    49: dict(src="0x8FC7A, 0x2AE44", dur="1d4 hits + 1 per 2 levels", save="None", target="You",
             eff="Each melee hit on you does no damage until the hits are used up. The original 1.3 "
                 "game also ends all your other spell effects when you cast it; ASCENT corrects this."),
    50: dict(src="0x8FCFA", dur="1 hour per level", save="None", target="You",
             eff="Strength +1d8 for a fighter (above 18 each point adds 10 percent, up to 18/00); "
                 "+1d6 for a thief or cleric and +1d4 for a mage, at most 18. From caster level 10 on "
                 "the spell ends at once (1.3 and ASCENT).",
             tech="The time L x 3600 s overflows the 16.16 fixed point timer from L = 10 on."),
    51: dict(src="0x8E4F6", dur="30 minutes", save="None", target="", eff="",
             tech="The line is a second Burning Hands (cold, ice.bbm), but the case is Find Traps: "
                  "Find/Remove Traps counts as 100% and the search runs each frame. The line is type "
                  "AREA and not self-cast: it fires one missile and gives the effect to everyone in "
                  "the area at the hit. The creature weapon Ice Shards (weapon 79) casts it."),
    53: dict(src="0x8F116", dur="3 minutes per level", save="None", target="You",
             eff="Attacks of extraplanar creatures miss you; other creatures get -2 to hit you. Your "
                 "attack on an extraplanar creature ends the spell."),
    54: dict(src="0x8E019", dur="10 minutes", save="None", target="You", eff="DETECT_MAGIC"),
    55: dict(src="0x8E0B5", dur="Instant", save="None", target="The target the missile hits",
             eff="The target loses all its spell effects, with no save. It also acts on things: a spell "
                 "missile is destroyed unless it saves (d20 >= 10), a potion on the floor breaks on "
                 "d20 >= 23 - your level, magic scenery breaks unless it saves, and a spell light goes "
                 "out unless it saves."),
}
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
    ("armor", 16): "Does nothing.",
    ("armor", 17): "Resist Fire while worn: fire damage halved.",
    ("armor", 18): "Feather Fall while worn.",
    ("armor", 19): "Fly while worn.",
    ("armor", 20): "Free Action while worn.",
    ("armor", 21): "Constitution 18 while worn, also if yours is higher.",
    ("armor", 22): "Infravision while worn, only for a human or half-elf.",
    ("armor", 23): "Jump while worn.",
    ("armor", 24): "Detect Magic while worn.",
    ("armor", 25): "Haste while worn.",
    ("armor", 26): "Strength 18/00 while worn, also if yours is higher.",
    ("armor", 27): "This is the Ring of Wizardry of the 1.3 readme. Doubles your mage spell slots of "
                   "levels 1 to 3. The original 1.3 game doubles them again at each change of "
                   "equipment and halves them once when you take it off; ASCENT corrects this and "
                   "doubles them once.",
    ("armor", 33): "Shield while worn: armor class at most 3, no damage from Magic Missile. Its own "
                   "armor class value does not count.",
    ("armor", 34): "Fly while worn.",
    ("armor", 35): "Feather Fall while worn.",
    ("armor", 36): "Levitate while worn. You cannot cast spells while you levitate.",
    ("armor", 37): "Haste while worn.",
    ("armor", 38): "Strength 19 while worn.",
    ("armor", 43): "Hide In Shadows 95% while worn.",
    ("armor", 44): "Improved Invisibility while worn.",
    ("armor", 45): "This is the Cloak of Lathandar of the 1.3 readme (it repeats the name Cloak of "
                   "Elvenkind). Worn: key T turns undead as a cleric of level 7, for any class. USE: "
                   "Continual Light around you.",
    ("armor", 46): "Invisibility while worn. It ends when you attack and comes back at the next change "
                   "of equipment.",
    ("armor", 47): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 48): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 49): "Counts only with no shield, no magic armor or bracers, and no chain or plate mail.",
    ("armor", 50): "Dexterity +4 (Dexterity 6 or less), +2 (7 to 13) or +1 (14 to 19). Open Locks +10% "
                   "for a thief; for any other class Open Locks becomes 37%.",
    ("armor", 51): "Detect Magic while worn. It gives no light.",
    ("armor", 52): "Strength 18/00 while worn.",
}

# Corrections of ASCENT that the pages name, with the option that turns
# each off (engine/main dtuchar.c, dtucombat.c, dtuspell.c).
FIXES = [
    ("Dtu_fix_multiclass_thac0", "-origchar", "Two classes take the better THAC0, not the worse."),
    ("Dtu_fix_haste_attack", "-origcombat", "Haste halves the swing time; 1.3 makes it longer."),
    ("Dtu_fix_chill_touch", "-origcombat", "Chill Touch acts on the caster; 1.3 fires it as a missile "
                                           "and it does nothing."),
    ("Dtu_fix_magic_tint", "-origobj", "The blue tint of Detect Magic is drawn on all magic things in "
                                       "view; 1.3 draws it mostly on near ones."),
    ("Dtu_fix_spell_save_repair", "-origchar", "Saves of older ASCENT versions get their spells moved "
                                               "back from the item list to the spell lists."),
    ("Dtu_fix_stoneskin_word", "-origcombat", "Stone Skin keeps your other spell effects."),
    ("Dtu_fix_rary_manual", "-origcombat", "Rary's Mnemonic Enhancer follows the manual."),
    ("Dtu_fix_wizardry_ring", "-origcombat", "The Ring of Wizardry doubles the slots once."),
    ("Dtu_fix_potion_frame", "-origcombat", "The intelligence potion does not depend on the frame rate."),
    ("Dtu_fix_luck_coin", "-origcombat", "The Potion of Luck uses a coin flip, not the day clock."),
]


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


def pages_note():
    return "Where these numbers come from: [[Sources]]."


# ---------------------------------------------------------------------
# Pages

class Pages:
    def __init__(self, tab, ct, check=None, mission=()):
        self.t = tab
        self.c = ct
        self.W = tab["$WEAPON"]
        self.A = tab["$ARMOR"]
        self.O = tab["$OBJECT"]
        self.S = tab["$SPELL"]
        self.check = check
        self.mission = set(mission)
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
            "These pages describe the rules of Descent to Undermountain 1.3 as the game really plays. "
            "The manual is often wrong about them, and the game shows no description of a magic "
            "item. The numbers come from the game data and from the code of the game, read for the "
            "ASCENT source port.",
            "",
            "ASCENT plays by these rules. Where the original 1.3 game has a defect that ASCENT "
            "corrects, the page says so in one line. Start ASCENT with `-origcombat` (and "
            "`-origchar` for THAC0) to play with the original behavior.",
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
            "- [[Sources]]: where each number comes from",
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
            ["The spell and the Ring of Infravision work", "human and half-elf only"],
            ["Finds secret doors while searching (key F)", ", ".join(sec)],
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
            "A half-elf counts as a human for the rules that test for a human (the spell and the "
            "Ring of Infravision).",
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
            ["Cloak of Elvenkind (the first one)", "Hide In Shadows 95"],
            ["Invisible (spell, potion, cloak)", "Open Locks -15"],
            ["Spider Climb, Levitate", "Climb Walls 100"],
            ["Sword of W'hin in the hand", "Find/Remove Traps 95"],
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
            "Each item is marked for the classes that may use it (the F T M C columns of the item "
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
            "number. With two classes the sum of the new levels is halved.",
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
            "at most 12. With two classes the better number of the two tables counts. Race, "
            "Constitution, Wisdom and magic items do not change saves. Spells use the Spell column.",
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
            "Skill points: Mirt, at the Yawning Portal, gives a thief 20 skill points for each thief "
            "level gained since the last visit, and 20 more at the first visit. Each point raises one "
            "skill by 1%, up to 95%. One skill can take at most half of the points of a visit. "
            "ASCENT does not have the skill point screen yet.",
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
            "- Climb Walls: the Activate key at a wall, with no weapon in the hand. A failed roll is a "
            "slip.",
            "- Backstab: a melee attack while Move Silently is on, on a humanoid creature, while no "
            "creature watches you: +4 to hit, damage x2 at levels 1 to 4, x3 at 5 to 8, x4 at 9 to "
            "12, x5 at 13 to 16, x6 at 17 and 18. The Dagger of Impaling adds 3 to 6 on a roll of 19 "
            "or 20.",
            "",
            "## Turn undead",
            "",
            "Key T. A cleric turns at the cleric level; anyone who wears the Cloak of Lathandar turns "
            "as a cleric of level 7. One d20 counts for all undead in sight, and at most 2 to 12 are "
            "destroyed. A number: the d20 must reach it. T: turned. D: destroyed. -: no effect (you "
            "can try again later).",
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
            "Potion of Toughening, system shock roll by Constitution 1 to 19: %s percent."
            % ", ".join(str(x) for x in shock),
            "",
            "## Differs from the manual",
            "",
            "- The manual says a priest gets every spell of a new spell level. In the game clerics "
            "find their spells as items, like mages.",
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
        dice = "%dd%d" % (n, s)
        if b:
            dice += signed(b)
        fs = r.num("fire_damage_dice", 0)
        fn = r.num("#fire_damage_dice", 0)
        dtype = "normal"
        if fs and fn:
            dtype = "normal + %dd%d fire" % (fn, fs)
        # fire_wait default 0.25 s (0x207E7 writes 0x4000 to +0x64); a melee
        # swing lasts max(fire_wait, 1.0) s (0x441E9)
        fw = float(r.num("fire_wait", 0.25))
        melee = (r.get("attack_type") or "").upper() != "PROJECTILE"
        speed = "%g s" % (max(fw, 1.0) if melee else fw)
        if "." not in speed:
            speed = speed.replace(" s", ".0 s")
        notes = []
        ac = self.c["ac_weapon"][i] if i < 83 else 0
        if ac:
            notes.append("Armor class %s while in the hand." % signed(ac))
        if r.get("weapon_name") and r.get("name") and r.get("weapon_name") != r.get("name"):
            notes.append("Also called %s." % r.get("weapon_name"))
        if i in WEAPON_NOTES:
            notes.append(WEAPON_NOTES[i])
        return [r.name(), dice, signed(h) if h else "0", dtype,
                "yes" if r.has("magic") else "no", speed, classes_of(r), value_of(r), " ".join(notes)]

    def weapons(self):
        melee, thrown, ammo = [], [], []
        for i, r in enumerate(self.W):
            if i in WEAPON_INTERNAL or (r.get("pc_spell") and i != 69):
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
                    note = "Its name promises speed, but it has no other power."
                launch.append([r.name(), "bow" if r.has("bow") else "crossbow", signed(r.num("to_hit_bonus", 0)),
                               classes_of(r), r.num("value", 0), note])
        head = ["Weapon", "Damage", "To hit", "Damage type", "Magic", "Speed", "F T M C", "Value", "Notes"]
        melee.sort(key=lambda x: x[0])
        thrown.sort(key=lambda x: x[0])
        ammo.sort(key=lambda x: (x[1], x[0]))
        self.count("Weapons", len(melee) + len(thrown) + len(ammo) + len(launch))
        return "\n".join([
            "# Weapons",
            "",
            "- Damage: the dice plus the bonus. Melee and thrown weapons add the Strength damage. "
            "Arrows and bolts add the bonus of the bow or crossbow to the attack and the damage. A "
            "critical hit (20) rolls the damage twice. A backstab multiplies it ([[Classes]]).",
            "- Speed: the time of a melee swing. Every swing lasts at least 1.0 s, so a faster weapon "
            "swings at 1.0 s; Slow doubles the time. A thrown or missile weapon fires once per that "
            "time while you hold the key; a new key press can fire sooner.",
            "- Damage type: the game has no slashing, piercing or bludgeoning damage and no weapon "
            "sizes. It has fire dice, which fire resistance halves.",
            "- Magic: a creature that resists weapons takes no damage from a weapon that is not magic.",
            "- F T M C: the classes that may use it ([[Classes]]).",
            "- Value: the base price in gold.",
            "",
            "## Melee weapons",
            "",
            md_table(head, melee),
            "",
            "## Thrown weapons",
            "",
            "Thrown where you look; the stack is used up one by one. A thrown weapon also gets the "
            "to-hit bonus of a worn crossbow.",
            "",
            md_table(head, thrown),
            "",
            "## Arrows and bolts",
            "",
            "An arrow needs a bow and a bolt needs a crossbow. The Flame Arrow needs neither.",
            "",
            md_table(["Weapon", "Kind"] + head[1:], ammo),
            "",
            "## Bows and crossbows",
            "",
            "Worn in the bow or crossbow place of the paper doll. The bonus is added to the attack "
            "and the damage of the missile.",
            "",
            md_table(["Launcher", "Kind", "To hit and damage", "F T M C", "Value", "Notes"], launch),
            "",
            "Wands are on [[Wands]].",
            "",
            "## Differs from the manual",
            "",
            "- The manual gives thieves daggers, short swords, long swords, scimitars, clubs, darts, "
            "throwing daggers and short bows. The game also gives thieves the Broad Sword and arrows.",
            "- The manual gives mages daggers and darts. The game also allows the Throwing Dagger.",
            "- The manual says the dagger is fast. In the game no melee weapon swings faster than "
            "1.0 s.",
            "- The Warhammer does 3d10, more than any sword; the Warhammer +1 does 1d6+1.",
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
            rows.append([r.name(), "shield" if r.has("shield") else "body", signed(-ac) if ac else "0",
                         str(10 + ac), classes_of(r), r.num("value", 0), " ".join(note)])
        for i in (8, 9):
            r = self.O[i]
            note = "Boot place." if i == 8 else "Gauntlet place. Find/Remove Traps and Open Locks become 0."
            ac = self.c["ac_misc"][i]
            rows.append([r.name(), "boots" if i == 8 else "gauntlets", signed(-ac), str(10 + ac), classes_of(r),
                         r.num("value", 0), note])
        self.count("Armor", len(rows))
        return "\n".join([
            "# Armor",
            "",
            "- Armor class starts at 10. Each worn item lowers it by its AC bonus. Then come the "
            "spell limits (Armor 6, Shield 3, Magical Vestment), Slow +4, the Dexterity adjustment "
            "and Invulnerability -2. The result stays within -10 to 10.",
            "- AC alone: your armor class with only this item and Dexterity 7 to 14.",
            "- Mage spells: a mage cannot cast a mage spell while wearing any item on this page except "
            "boots and gauntlets.",
            "- F T M C: the classes that may wear it ([[Classes]]).",
            "",
            "Rings, amulets, cloaks, boots, bracers, gloves and girdles are on [[Magic Items|Magic-Items]].",
            "",
            md_table(["Armor", "Place", "AC bonus", "AC alone", "F T M C", "Value", "Notes"], rows),
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
               "- F T M C: the classes that may wear it ([[Classes]]).",
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
                    eff = "Worn in the armor place, so not with body armor. They count as magic armor."
                if title == "Rings" and i in (0, 14, 15):
                    eff = "Counts only without magic armor or bracers. Of two of these rings only the better counts."
                if i == 33:
                    ac = 0
                rows.append([r.name(), signed(-ac) if ac else "0", classes_of(r), r.num("value", 0), eff or "AC only."])
            if title == "Amulets":
                rows.append([self.O[5].name(), "0", classes_of(self.O[5]), "-", "Quest item. No other effect."])
            if title == "Cloaks":
                rows.append([self.O[6].name(), "0", classes_of(self.O[6]), self.O[6].num("value", 0), "A plain cloak. No effect."])
            if title == "Girdles":
                rows.append([self.O[7].name(), "0", classes_of(self.O[7]), self.O[7].num("value", 0), "A plain belt. No effect."])
            total += len(rows)
            out += ["## " + title, "", md_table(["Item", "AC bonus", "F T M C", "Value", "Effect"], rows), ""]
            if title == "Rings":
                out += ["You can wear two rings.", ""]
        wrows = []
        for i in (54, 69, 70, 71, 45):
            r = self.W[i]
            wrows.append([r.name(), WEAPON_NOTES.get(i, "")])
        r = self.O[100]
        wrows.append([r.name(), "A bow with +2 to hit and damage. Its name promises speed, but it has no other power."])
        out += ["## Weapons with powers", "", "Damage and classes: [[Weapons]].", "",
                md_table(["Weapon", "Power"], wrows), ""]
        sp = ", ".join(self.S[s].name() for s in self.c["spider_spells"])
        orows = [
            [self.O[2].name(), "USE: cures your blindness; if you are not blind but hurt, Cure Serious "
                               "Wounds (3 to 17 hit points). Not used up. You get one use at the start "
                               "and one more for each new mission from Khelben. ASCENT allows one use "
                               "for each rest instead."],
            [self.O[101].name(), "USE: it asks you questions and gives answers, a script of the quest."],
            [self.O[0].name(), "Does nothing: it gives no light and has no use."],
            [self.O[10].name() + ", " + self.O[13].name() + ", " + self.O[11].name() + ", " + self.O[12].name(),
             "In the lockpick place, for a thief: Open Locks +5, +10, +15, +20."],
            ["Spider Piece", "USE with charges: turns undead as a cleric of level 2."],
            ["Spider Piece", "USE with charges: you know the whole map."],
            ["Spider Piece", "USE with charges: each person in sight gets one of three, at random: "
                             "killed unless it saves, hit by a random spell (%s), or frozen for its "
                             "level in seconds unless it saves. Not in ASCENT yet." % sp],
            ["Spider Piece", "Using it does nothing."],
            ["Spider Piece", "USE with charges: Fear around you, and Aid, Magical Vestment, Fly, "
                             "Protection From Fire, Resist Fire, Flame Walk and Strength on you."],
            ["Spider Piece", "USE with charges: the view visits each creature in turn for 1 to 3 "
                             "seconds. Not in ASCENT yet."],
            ["Spider Piece", "USE with charges: a quest event."],
            ["Assembled Spider", "USE with charges: Spider Climb, Free Action, Haste and Infravision "
                                 "with no end."],
            ["Spider Piece", "USE with charges: all your hit points, and Aid, Shield, Feather Fall, "
                             "Improved Invisibility, Magical Vestment, Prayer, Protection From Evil, "
                             "Resist Fire and Stone Skin; then Cure Blindness or Deafness if needed."],
        ]
        total += len(wrows) + len(orows)
        out += ["## Other items with a use", "",
                "The Spider Pieces are listed in the order of the game's item list.", "",
                md_table(["Item", "Effect"], orows), "",
                "Potions, wands and scrolls have their own pages: [[Potions]], [[Wands]], [[Scrolls]].", "",
                "## Notes", "",
                "- The 1.3 readme lists new items: Potion of Invisibility, Ring of Wizardry, Cloak of "
                "Lathandar, Drowslayer and +3 Longsword. In the game the Potion of Invisibility is the "
                "second \"Potion of Agility\", the Ring of Wizardry is the \"Ring of Warmth\", and the "
                "Cloak of Lathandar is the second \"Cloak of Elvenkind\". There is no Drowslayer and no "
                "+3 Longsword.",
                "- The game does not identify items: every item shows its real name.", "",
                pages_note(), ""]
        self.count("Magic-Items", total)
        return "\n".join(out)

    # -- Potions --------------------------------------------------------
    def potions(self):
        rows = []
        for i in range(72, 89):
            r = self.O[i]
            eff, _ = POTIONS[i]
            rows.append([r.name(), eff, r.num("value", 0)])
        self.count("Potions", len(rows))
        return "\n".join([
            "# Potions",
            "",
            "Drink a potion with the USE button or a hot key. Each use takes one potion. A new potion "
            "of a kind that is running keeps the later end. The Potion of Anti-Magic and Dispel Magic "
            "end all potion effects. A potion that casts a spell (Flying, Jumping) casts it at your "
            "caster level ([[Wizard Spells|Wizard-Spells]]).",
            "",
            md_table(["Potion", "Effect", "Value"], rows),
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
            if s in (16, 32):
                note = "6 damage per caster level."
            rows.append([r.name(), self.S[s].name(), r.num("uses", 1), classes_of(r), value_of(r), note])
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
            md_table(["Wand", "Spell", "Charges", "F T M C", "Value", "Notes"], rows),
            "",
            "The spell rules: [[Wizard Spells|Wizard-Spells]].",
            "",
            "The Short Sword of Quickness is not a wand: it is a plain magic sword ([[Weapons]]).",
            "",
            pages_note(),
            "",
        ])

    # -- Scrolls --------------------------------------------------------
    def scrolls(self):
        rows = []
        for i in (102, 106, 126, 127):
            rows.append([self.O[i].name().strip(), SCROLLS[i]])
        self.count("Scrolls", len(rows))
        return "\n".join([
            "# Scrolls",
            "",
            "## Spell scrolls",
            "",
            "A spell in the world is an item. When you pick it up, a mage learns a mage spell and a "
            "cleric learns a cleric spell; it goes into the spell book. A spell that none of your "
            "books takes stays in the item list. With it in the hand, an attack casts it as a scroll: "
            "no cast time, no spell slot, and the scroll is used up. The caster level is that of a "
            "wand ([[Wands]]).",
            "",
            "Only a mage or a cleric can put a spell item in a hot key box, and its class must fit. "
            "Protection From Evil and Protection From Fire are the exceptions: anyone can use them.",
            "",
            "## Other scrolls",
            "",
            md_table(["Scroll", "Effect"], rows),
            "",
            pages_note(),
            "",
        ])

    # -- Spells ---------------------------------------------------------
    def spell_range(self, s):
        if self.spell_self(s) or s == 5:		# Chill Touch: self under Dtu_fix_chill_touch
            return "You"
        if s == 32:
            return "%d ft" % (MISSILE_FT_PER_S * 2)
        if s == 36:
            return "%d ft + %d ft per 3 levels" % (MISSILE_FT_PER_S * 60, MISSILE_FT_PER_S * 60)
        return "%d ft" % (MISSILE_FT_PER_S * MISSILE_LIFE_S)

    def spell_effect(self, s):
        e = SPELLS[s]["eff"]
        if e == "DETECT_MAGIC":
            return DETECT_MAGIC
        if e == "VESTMENT":
            return vestment_text()
        return e

    def spell_page(self, cls):
        flag = "mage_use" if cls == "m" else "cleric_use"
        title = "Wizard spells" if cls == "m" else "Priest spells"
        nums = [i for i, r in enumerate(self.S) if r.has(flag) and i not in OTHER_SPELL_LINES]
        by_level = collections.defaultdict(list)
        for i in nums:
            by_level[self.S[i].num("level", 1)].append(i)
        out = ["# " + title, "",
               ("Mage spells (the manual says wizard)." if cls == "m" else "Cleric spells (the manual says priest).")
               + " Each row has all the rules of one spell. \"Level\" in a row means your %s level."
               % ("mage" if cls == "m" else "cleric"), "",
               "- Cast: the cast time in game seconds. A hit on you while you cast stops the spell, and "
               "the slot is lost. A scroll or a wand casts at once.",
               "- Range: \"You\" means the spell acts on you. The other spells fire missiles that fly "
               "50 ft per second until they hit something, for 30 seconds (1500 ft) unless the row "
               "says less. The spell acts on what the missile hits.",
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
                r = self.S[s]
                rule = SPELLS[s]
                ct = r.num("cast_time", 0)
                target = rule["target"].replace("{r}", feet(r.num("radius", 0)))
                rows.append([self.spell_name(s), ("%d s" % ct) if ct else "Instant", self.spell_range(s),
                             target, rule["dur"], rule["save"], self.spell_effect(s)])
            rows_total += len(rows)
            out += [md_table(["Spell", "Cast", "Range", "Target", "Duration", "Save", "Effect"], rows), ""]
        out += ["## Differs from the manual", ""]
        out += self.spell_manual_notes(cls)
        out += ["", pages_note(), ""]
        self.count("Wizard-Spells" if cls == "m" else "Priest-Spells", rows_total)
        return "\n".join(out)

    def spell_name(self, s):
        return self.S[s].name()

    def spell_manual_notes(self, cls):
        if cls == "m":
            return [
                "- Knock has no effect when you cast it. Chill Touch also has none in 1.3; ASCENT "
                "corrects this.",
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
                "- Scare: the manual says level 6 or more is safe and it is an area spell; the game "
                "affects level 6 and lower, one target.",
                "- Stone Skin: the manual says 1 to 4 attacks; the game 1d4 + 1 per 2 levels.",
                "- Haste: the manual says faster attacks. The original 1.3 game makes them slower; "
                "ASCENT corrects this.",
                "- Fly: the manual says it lingers a random time; the game has a random duration and no "
                "extra time.",
            ]
        return [
            "- Prayer: the manual adds penalties to enemies and bonuses to damage and saves. The game "
            "gives only Bless (+1 to hit) to you.",
            "- Resist Cold cures blindness and deafness instead. The 1.3 readme lists Cure Blindness or "
            "Deafness as a new spell.",
            "- Flame Walk halves fire damage; the manual says it stops normal fire.",
            "- Magical Stone: the manual says three stones; the game 1d3.",
            "- Magical Vestment: the manual gives the armor class of chain mail (5); the game gives 6 "
            "at levels 1 and 2, 5 up to level 7, and better at higher levels.",
            "- The manual lists Silence, 15' Radius (level 2). There is no such spell.",
            "- The manual says a priest gets every spell of a new spell level. Clerics find their "
            "spells as items.",
            "- The 1.3 readme names a new spell Produce Fire. There is no such spell.",
        ]

    # -- Sources --------------------------------------------------------
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
                    "spell levels, cast times, radii",
                    "the item and spell lines (`$WEAPON`, `$ARMOR`, `$OBJECT`, `$SPELL`) of the table "
                    "`YPINN.BIN`"],
                   ["Race, class, ability, experience, THAC0, hit point, saving throw, slot, thief and "
                    "turn undead tables; armor class of the items",
                    "data tables of `DTUGAME.EXE` (below)"],
                   ["Spell effects, durations, saves, item effects, potion effects",
                    "the code of `DTUGAME.EXE`: the spell cases of 0x8CFFF (jump table 0x8CF1F), the item "
                    "use 0x37A6A, the worn item bits 0xAFB55"],
               ]), "",
               "## Units", "",
               "- Two game units are one foot: the fall message of the game divides the fall distance "
               "by two (`fell >> 17`).",
               "- Game seconds: the game messages call 60 of them a minute and 3600 an hour.",
               "- Spell missiles fly 100 units (50 ft) per second: no `$SPELL` line has `speed=`, and "
               "the default of the reader 0x242F6 is 100.0. They live 30 s, Lightning Bolt 2 s and "
               "Melf's Acid Arrows (L/3 + 1) minutes (0x56FF6 writes the life of the missile "
               "template).", "",
               "## The tables of all levels", ""]
        if self.check is None:
            out += ["This build did not compare the level tables (no `--tables`).", ""]
        else:
            mission = [x for x in self.check if x[0][:-4].lower() in self.mission]
            other = [x for x in self.check if x[0][:-4].lower() not in self.mission]
            bad = [x for x in mission if x[2]]
            have = set(x[0][:-4].lower() for x in self.check)
            missing = [m for m in self.mission if m not in have]
            out += ["The item and spell lines of the level tables were compared with `YPINN.BIN`: "
                    "names, dice, bonuses, values, classes, spell levels, cast times, ranges, radii, "
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
                "`rand() % sides + 1`, then the per level part), the area function 0x90CEF, the save "
                "function 0xAE596 (category 4 for spells).", "",
                md_table(["Spell", "Line", "Code", "Detail"], srows), ""]
        out += ["## Item code", "",
                md_table(["Item", "Code"], [[self.O[i].name(), POTIONS[i][1]] for i in range(72, 89)]
                         + [["Gem of Curing", "case 2 of 0x37A6A (0x3890F)"],
                            ["Scroll, thief scroll", "cases 0x66 and 0x7F of 0x37A6A, text screen 0x9DC42"],
                            ["Cursed Scroll", "case 0x6A of 0x37A6A"],
                            ["Spider Pieces", "cases 0x12 to 0x1A of 0x37A6A"],
                            ["Worn items", "the item bits of 0xAFB55"],
                            ["Weapon powers", "0x2AB9F, 0x2B1F2, 0xB337E, 0xAFB55, 0xB038F"]]), ""]
        out += ["## Other rules", "",
                md_table(["Rule", "Code"], RULE_SOURCES), ""]
        out += ["## Rules found in the code", "",
                "These rules were open questions of the reverse engineering. Each was decided from "
                "the 1.3 code.", "",
                md_table(["Rule", "Evidence"], SOURCES_FINDINGS), ""]
        out += ["## Corrections of ASCENT", "",
                "ASCENT corrects these defects of 1.3. The option turns the correction off.", "",
                md_table(["Setting", "Option", "Correction"], FIXES), ""]
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
        out += ["## Item line values the game does not use", "",
                "\n".join("- " + x for x in UNUSED_ITEM_VALUES), ""]
        self.count("Sources", len(EXE_TABLES) + len(srows) + len(SOURCES_FINDINGS) + len(FIXES)
                   + len(unused) + len(olines) + len(wlines) + len(RULE_SOURCES) + 22)
        return "\n".join(out)


# Spell lines that are not real player spells (Sources.md).
OTHER_SPELL_LINES = {
    51: "A data artifact: a second Burning Hands line with the code of Find Traps.",
}

WEAPON_INTERNAL = {
    0: "the empty hand (1d2 plus strength, from the character record, not the line)",
    1: "Flare", 2: "template of a spell cast with the hands", 3: "template of a wand cast",
    4: "template of a scroll cast", 5: "template of a spell arrow", 10: "named DO NOT USE",
    13: "a missile line without pc_spell, 5d6. 0x2B1F2 has code for weapon 13 (+2 to hit and double "
        "damage against creatures with Robot_info+0xD9 bit 8, and a use that casts Continual Light, "
        "Free Action and Protection From Evil for a Lawful Good character), probably the Drowslayer of "
        "the readme; with this line the code is never reached",
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
    8: "Homes in on the target.",
    45: "A backstab with a hit roll of 19 or 20 does +3 to 6 damage.",
    54: "In the hand: Chill Touch, Resist Fire, Feather Fall, Free Action, Jump, Detect Magic, Haste, "
        "Improved Invisibility and (human or half-elf) Infravision with no end, and Find/Remove Traps 95%.",
    69: "Its name promises speed, but it has no other power.",
    70: "Against undead: double damage on d100 <= 5 x (level of the undead - 4); otherwise the undead "
        "is destroyed.",
    71: "+2 to hit and +2 fire damage against a regenerating target, +4 and +4 against undead.",
    75: "Not magic, despite the +2.",
}

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
    83: ("Invisibility for 30, 40, 50 or 60 minutes. This is the Potion of Invisibility of the 1.3 "
         "readme; it repeats the name Potion of Agility.", "case 0x53"),
    84: ("Invulnerability for 5 to 20 minutes: no spell damage, armor class -2, and creatures below "
         "level 4 or without a magic weapon cannot hurt you.", "case 0x54, 0x2A24A"),
    85: ("The spell Jump on you: 1d3 minutes + 1 per level.", "case 0x55"),
    86: ("For one hour your next attack can become a critical hit, and then the luck ends. The "
         "original 1.3 game ties the chance to the day clock; ASCENT corrects this with a coin flip.",
         "case 0x56, 0x2A87B (P+0x1A58, day clock bit P+0x83)"),
    87: ("Haste for 5 to 20 minutes.", "case 0x57"),
    88: ("+2 to 18 hit points and Bless for 70 to 120 minutes; the extra hit points go when Bless "
         "ends. If you already have more hit points than your maximum: \"You don't feel so good.\" and "
         "a system shock roll ([[Classes]]): d100 above the value for your Constitution kills you.",
         "case 0x58, table 0x2FACC"),
}

# 0x37A6A cases 0x66, 0x6A, 0x7F; misc 126 has no case
SCROLLS = {
    102: "USE: shows Khelben's summary of your current mission. A Scroll found in a dungeon level "
         "shows its own text instead: notes and histories of Undermountain. Not used up. Every new "
         "character gets one. ASCENT does not show the text yet.",
    106: "USE: the screen shakes, then each of these with a 50% chance: Confusion, Continual Light, "
         "Scare, Hold Person and Fireball on you, the map forgotten, and each ability score set to a "
         "random value from 1 to its value. Then damage from 0 to your hit points - 1. Quest item.",
    126: "Quest item. Using it does nothing.",
    127: "USE: shows a picture clue. Not used up. Quest item.",
}

SOURCES_FINDINGS = [
    ["Fireball, Lightning Bolt and Melf's Acid Arrows do only the damage per level (6, 6, 4 per "
     "level).",
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
    ["The Short Sword of Quickness never casts Haste.",
     "`PC_spell` (Weapon_info+0x3A) is read only in 0x56A0D (0x56B1C, 0x56CA4). Its one caller 0x44300 "
     "calls it only for a spell (type 0x11) or a weapon with `attack_type` 0 (0x44423 to 0x44445). The "
     "sword is a melee weapon (default `attack_type` 1)."],
    ["The Short Bow of Speed has no Haste.",
     "`pc_spell1` of an `$OBJECT` line is stored only (docs/formats/tbl.md)."],
    ["Gem of Curing: one use at the start, one more for each mission.",
     "0x3891B: the use counter P+0x1D4D73 must be <= 0x8445A(), the mission number 0x13E588 (0 at a "
     "new game, 0x82BC7; the next mission at Khelben, 0x83E5E). 0x389E0 adds 1. No other code writes "
     "the counter."],
    ["The Scroll shows Khelben's mission summary; found Scrolls show their own text.",
     "Case 0x66 calls 0x9DC42 with the count of the item. 0x9CEA3: count 0 (the Scroll of a new "
     "character) takes the mission blurb table 0x1451E5[mission]; another count n takes the text "
     "table 0x144FE5[n]. Both hold entries of `FETXT` (for example 271 \"Quest One: Corner the "
     "Kobolds\", 141 \"A Brief History of Undermountain\"). The levels give Scrolls counts 2 to 27."],
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
    ["Spell slots filled by a rest and a new level", "0x3551B, 0x3575B"],
    ["Wisdom divides Fear, Scare, Hold Person and the Color Spray stun on the player",
     "0x8DAA2, 0x8E338, 0x8E8FA, 0x8F57F"],
    ["Charisma market prices", "0x86F75"],
    ["Thief skills", "0xB3038, 0xB31BD (Open Locks), 0xB3400, 0xB4196 (Move Silently), 0xB333F (Hide In "
     "Shadows), 0x951DE (Climb Walls), 0xB337E (backstab), worn items 0xAFB55"],
    ["Turn undead; the Cloak of Lathandar as a cleric of level 7", "0xADC46, table 0xA6487; 0xADCB3"],
    ["Rest place: again after 300 s", "0x3E1F6"],
    ["Armor class order and limits", "0xAFB55, armor table 0x145C04"],
    ["Melee swing max(fire_wait, 1.0) s, Slow doubles it", "0x441E9"],
    ["Missile fire rate fire_wait (default 0.25 s, 0x207E7)", "0x56A0D"],
    ["Weapon resistant creatures and the magic flag", "0x29D9D"],
    ["Thrown and other missiles take the crossbow bonus; arrows the bow bonus", "0x2AB0A"],
    ["Critical hit rolls twice; fumble drop", "0x29A9C, 0xAE837"],
    ["Wand and scroll cast, charges", "0x56A0D, 0xB7084"],
    ["Caster level of a spell, wand or scroll", "0x91EC1"],
    ["Spell items learned on pick up", "0xB69AD"],
    ["Spell items in hot key boxes (39, 40, 53 for all)", "0x3533C"],
]

NOT_IN_ASCENT = [
    "The text screen of the Scroll and the thief scroll (0x9DC42).",
    "The thief skill point screen (thiefpts.c, 0xB5B49).",
    "The Spider Pieces 20, 23 and 24.",
]

UNUSED_ITEM_VALUES = [
    "Sword of W'hin: `ac_adjustment`, `adjust_strength`, `adjust_dexterity`. Its powers are code for "
    "the item number.",
    "Flame Arrow: `levels_per_use=5`.",
    "Armoring: `pc_spell=2` (Armor). The code sets an item bit that nothing reads.",
    "Ring of Warmth: `pc_spell=42`. Its power is code for the item number.",
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
    "$SPELL": ["name", "level", "cast_time", "range", "radius", "duration", "level_duration", "type",
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

    check = check_tables(tab, a.tables) if a.tables else None
    # The level names of the mission list (lines "name.lvl ;n")
    mission = []
    msn = os.path.join(a.game_dir, "DTU.MSN")
    if os.path.exists(msn):
        with open(msn, "rb") as f:
            mission = re.findall(r"(\w+)\.lvl", f.read().decode("latin-1").lower())
    p = Pages(tab, ct, check, mission)
    pages = {
        "Home.md": p.home(), "_Sidebar.md": p.sidebar(), "Races.md": p.races(), "Classes.md": p.classes(),
        "Weapons.md": p.weapons(), "Armor.md": p.armor(), "Magic-Items.md": p.magic_items(),
        "Potions.md": p.potions(), "Wands.md": p.wands(), "Scrolls.md": p.scrolls(),
        "Wizard-Spells.md": p.spell_page("m"), "Priest-Spells.md": p.spell_page("c"),
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
    return 0


if __name__ == "__main__":
    sys.exit(main())
