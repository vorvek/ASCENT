#!/usr/bin/env python3
"""Disassembler for the dialogue scripts of Descent to Undermountain 1.3.

The scripts are the records of GABx.DAT (index GABx.RLH, see
docs/formats/jrg.md). This tool decrypts each record and parses it with the
grammar that the interpreter of DTUGAME.EXE uses (dialogue.c, statement
dispatch at 0xAAE3E). docs/re/scripts.md describes the byte code. The output
is the reference for the C interpreter in engine/main/dtuscript.c.

  python -I tools/script.py dis GABx.RLH [ID ...]     disassemble (all records or the ids)
  python -I tools/script.py disraw FILE [ID]          disassemble one decoded record (gab_ID.bin of -scriptdump)
  python -I tools/script.py check GAMEDIR             parse every record of every GABx file
  python -I tools/script.py stats GABx.RLH            counts of codes, flags, items, labels

The parse is linear: a record is a list of blocks "CB label statements". A
statement code that the interpreter does not know, an operand that runs past
the end of the record or a block that does not start with CB is an error.
"""
import glob
import os
import sys
from collections import Counter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import jrg  # noqa: E402

# Names of the statement codes (0xAAC6E jump table, 97 entries for 0x99..0xF9)
STMT = {
    0x99: "STR", 0x9A: "DEX", 0x9B: "CON", 0x9C: "INT", 0x9D: "WIS", 0x9E: "CHA",
    0x9F: "LEVEL", 0xA0: "HP",
    0xBE: "NARRATE", 0xBF: "GIVE", 0xC0: "TAKE", 0xC1: "XP", 0xC2: "ANGER",
    0xC3: "ANGER_NPCS", 0xC4: "TRIGGER", 0xC5: "SET", 0xC6: "CLEAR", 0xC7: "GOTO",
    0xC8: "END", 0xCC: "ENDBLOCK", 0xCD: "SAY", 0xD3: "SOUND", 0xD4: "SOUND_OBJ",
    0xD5: "SOUND_SEG", 0xD6: "STOP_SOUND_OBJ", 0xD7: "STOP_SOUND_SEG",
    0xDA: "TELEPORT_SELF", 0xDB: "TELEPORT_PLAYER", 0xDC: "TELEPORT_OBJ",
    0xDD: "MOVE_ITEM", 0xDE: "SUMMON", 0xDF: "RESUME", 0xE0: "VAR",
    0xE5: "WALK", 0xEA: "CAST_OBJ", 0xEB: "CAST_SEG", 0xEC: "WAIT", 0xED: "KILL",
    0xEE: "CALL", 0xF2: "IF",
}
for _c in range(0xB3, 0xBE):
    STMT[_c] = "ANIM %d" % (_c - 0xB3)
for _c in range(0xF4, 0xF9):
    STMT[_c] = "ELSE"
for _c in range(0xF9, 0xFE):
    STMT[_c] = "ENDIF"

ARITH = {0x7A: "+=", 0x7B: "-=", 0x7C: "*=", 0x7D: "/=", 0x7E: "="}
CMP = {0x7F: "<", 0x80: "<=", 0x81: "==", 0x82: "!=", 0x83: ">", 0x84: ">="}
CLASS = {0xA4: "WEAPON", 0xA5: "ARMOR", 0xA6: "MISC", 0xA7: "SPELL", 0xA8: "GOLD"}
FACING = {0xA9: "EAST", 0xAA: "WEST", 0xAB: "NORTH", 0xAC: "SOUTH"}
SIDE = {0xAD + i: "side%d" % i for i in range(6)}
TOKEN = {9: "{name}", 10: "{Class}", 11: "{classes}", 12: "{Race}", 13: "{races}",
         14: "{boy}", 15: "{man}", 16: "{sir}", 17: "{gentleman}"}
VALUE_TERM = {
    0x97: "talks", 0x98: "word153ED8+500", 0x99: "STR", 0x9A: "DEX", 0x9B: "CON",
    0x9C: "INT", 0x9D: "WIS", 0x9E: "CHA", 0x9F: "LEVEL", 0xA0: "HP", 0xA1: "mission+1",
    0xA2: "objnum", 0xA3: "mode", 0xC2: "anger", 0xE7: "segment",
}
ALIGN = ["LG", "NG", "LN", "TN", "CG", "CN"]
CLASSES = ["cleric", "mage", "fighter", "thief"]
RACES = ["human", "dwarf", "elf", "drow", "halfling", "half-elf"]


class ParseError(Exception):
    pass


class Script:
    def __init__(self, data, sid, stats=None):
        self.d = data
        self.p = 0
        self.sid = sid
        self.depth = 0          # indentation of the listing
        self.ifd = 0            # IF depth of the interpreter (0x145525)
        self.out = []
        self.st = stats if stats is not None else Counter()

    # --- low level -------------------------------------------------------
    def byte(self):
        if self.p >= len(self.d):
            raise ParseError("operand past the end at 0x%X" % self.p)
        b = self.d[self.p]
        self.p += 1
        return b

    def peek(self):
        if self.p >= len(self.d):
            raise ParseError("operand past the end at 0x%X" % self.p)
        return self.d[self.p]

    def cstr(self):
        try:
            e = self.d.index(0, self.p)
        except ValueError:
            raise ParseError("string without NUL at 0x%X" % self.p)
        s = self.d[self.p:e]
        self.p = e + 1
        return s

    def hexstr(self):
        """0xA76B9: sscanf("%hx") of a string; the pointer moves past the NUL."""
        start = self.p
        s = self.cstr()
        t = s.decode("latin1")
        if not t or any(c not in "0123456789abcdefABCDEF" for c in t):
            raise ParseError("not a hex number %r at 0x%X" % (t, start))
        return int(t, 16) & 0xFFFF

    def num(self):
        """0xA7DF0: a number operand."""
        b = self.byte()
        if b == 0xD2:
            a = self.hexstr()
            c = self.hexstr()
            return "rand(%d..%d)" % ((1, a) if c == 0 else (a, c))
        if b == 0xE0:
            if self.byte() == 0x15:
                return "var(obj %s)" % self.num()
            self.p -= 1
            return "var"
        self.p -= 1
        return "%d" % self.hexstr()

    def objref(self):
        """An optional '0x15 number' object prefix."""
        if self.peek() == 0x15:
            self.p += 1
            return " obj %s" % self.num()
        return ""

    def text(self):
        """0xA77BE: one or more strings, up to the first byte of 0x7A or above."""
        alts = []
        while self.p < len(self.d) and self.d[self.p] < 0x7A:
            s = self.cstr()
            alts.append("".join(TOKEN.get(c, chr(c)) if c < 0x20 and c != 10 else
                                ("\\n" if c == 10 else chr(c)) for c in s))
        if len(alts) > 20:
            raise ParseError("more than 20 text alternatives (the executable loops forever)")
        self.st["text_alternatives_%d" % len(alts)] += 1
        return " | ".join('"%s"' % a for a in alts) if alts else '""'

    def arith(self):
        op = self.byte()
        if op not in ARITH:
            raise ParseError("arithmetic byte 0x%02X at 0x%X" % (op, self.p - 1))
        return "%s %s" % (ARITH[op], self.num())

    def cmp(self):
        op = self.byte()
        if op not in CMP:
            raise ParseError("compare byte 0x%02X at 0x%X" % (op, self.p - 1))
        if self.peek() == 0xEF:
            self.p += 1
            return "%s MAXHP" % CMP[op]
        return "%s %s" % (CMP[op], self.num())

    def side(self):
        b = self.byte()
        if b not in SIDE:
            raise ParseError("side byte 0x%02X at 0x%X" % (b, self.p - 1))
        return SIDE[b]

    def facing(self):
        b = self.byte()
        if b not in FACING:
            raise ParseError("facing byte 0x%02X at 0x%X" % (b, self.p - 1))
        return FACING[b]

    def item(self):
        c = self.byte()
        if c not in CLASS:
            raise ParseError("item class byte 0x%02X at 0x%X" % (c, self.p - 1))
        n = self.num()
        self.st["item %s %s" % (CLASS[c], n)] += 1
        return "%s %s" % (CLASS[c], n)

    def who(self):
        b = self.byte()
        return "any" if b == 0xCA else "critter#%d" % b

    def rng(self):
        if self.peek() == 0xCA:
            self.p += 1
            return "anywhere"
        return "range %s" % self.num()

    # --- conditions ------------------------------------------------------
    def term(self):
        """0xA8516"""
        neg = ""
        if self.peek() == 0xE9:
            self.p += 1
            neg = "NOT "
        b = self.byte()
        if b <= 7:
            r = "flag%d%s" % (b, self.objref())
        elif 0x85 <= b <= 0x8A:
            r = "align==%s" % ALIGN[b - 0x85]
        elif 0x8B <= b <= 0x8E:
            r = "class==%s" % CLASSES[b - 0x8B]
        elif 0x8F <= b <= 0x94:
            r = "race==%s" % RACES[b - 0x8F]
        elif b in (0x95, 0x96):
            r = "sex==%s" % ("male", "female")[b - 0x95]
        elif b in (0x97, 0xDF, 0xE0):
            name = {0x97: "talks", 0xDF: "resume", 0xE0: "var"}[b]
            r = "%s%s %s" % (name, self.objref(), self.cmp())
        elif b in VALUE_TERM:
            r = "%s %s" % (VALUE_TERM[b], self.cmp())
        elif b == 0xC3:
            w = self.who()
            g = self.rng()
            r = "anger(%s, %s) %s" % (w, g, self.cmp())
        elif b == 0xC4:
            r = "trigger_on(seg %s %s)" % (self.num(), self.side())
        elif b == 0xC9:
            r = "dead(obj %s)" % self.num()
        elif b == 0xD0:
            r = "npc_has(%s)" % self.item()
        elif b == 0xD1:
            r = "player_has(%s)" % self.item()
        elif b == 0xE8:
            r = "fire_count(seg %s %s) %s" % (self.num(), self.side(), self.cmp())
        elif b == 0xF0:
            n = self.num()
            self.st["visited %s" % n] += 1
            r = "visited(seg %s)" % n
        elif b == 0xF1:
            n = self.num()
            self.st["test gflag %s" % n] += 1
            r = "gflag %s" % n
        elif b == 0xFE:
            # a term of the port (engine/main/dtuscript.c sc_term): the
            # blocks of the class gear (dtuclassgear.c); no record of the game
            # has it
            r = "knows(SPELL %s)" % self.num()
        else:
            self.p -= 1
            r = "%s %s" % (self.num(), self.cmp())
        return neg + r

    def cond(self):
        """0xA8BDB"""
        parts = [self.term()]
        while True:
            b = self.byte()
            if b == 0xE1:
                parts.append("AND")
                parts.append(self.term())
            elif b == 0xE2:
                parts.append("OR")
                parts.append(self.term())
            elif b == 0xF3:
                return " ".join(parts)
            else:
                raise ParseError("condition ends with 0x%02X at 0x%X" % (b, self.p - 1))

    # --- statements ------------------------------------------------------
    def emit(self, at, s):
        self.out.append("%05X  %s%s" % (at, "  " * max(self.depth, 0), s))

    def if_line(self, at):
        c = self.cond()
        self.emit(at, "IF %s" % c)
        self.depth += 1
        self.ifd += 1
        if self.ifd > 5:
            raise ParseError("IF depth above 5 at 0x%X" % at)

    def else_endif(self, at, code):
        if 0xF4 <= code <= 0xF8:
            if code - 0xF4 + 1 != self.ifd:
                raise ParseError("ELSE of depth %d at depth %d (0x%X)" % (code - 0xF3, self.ifd, at))
            self.depth -= 1
            self.emit(at, "ELSE")
            self.depth += 1
            return True
        if 0xF9 <= code <= 0xFD:
            if code - 0xF9 + 1 != self.ifd:
                raise ParseError("ENDIF of depth %d at depth %d (0x%X)" % (code - 0xF8, self.ifd, at))
            self.depth -= 1
            self.ifd -= 1
            self.emit(at, "ENDIF")
            return True
        return False

    def say(self, at):
        """0xA8CBC"""
        self.emit(at, "SAY %s" % self.text())
        self.depth += 1
        base = self.ifd
        while True:
            at = self.p
            b = self.byte()
            if b == 0xCE:
                self.depth -= 1
                if self.ifd != base:
                    raise ParseError("reply list ends inside an IF at 0x%X" % at)
                self.emit(at, "WAIT_REPLY")
                return
            if b == 0xCF:
                lab = self.num()
                self.emit(at, "REPLY -> %s %s" % (lab, self.text()))
                self.st["replies"] += 1
            elif b == 0xF2:
                self.if_line(at)
            elif self.else_endif(at, b):
                pass
            else:
                raise ParseError("say code 0x%02X at 0x%X" % (b, at))

    def statement(self):
        at = self.p
        b = self.byte()
        self.st["code %02X %s" % (b, STMT.get(b, "?"))] += 1
        if 0x99 <= b <= 0xA0:
            self.emit(at, "%s %s" % (STMT[b], self.arith()))
        elif 0xB3 <= b <= 0xBD:
            self.emit(at, STMT[b])
        elif b == 0xBE:
            self.emit(at, "NARRATE %s" % self.text())
        elif b in (0xBF, 0xC0):
            it = self.item()
            extra = ""
            if self.peek() in (0xE4, 0xE3):
                k = self.byte()
                extra = " %s %s" % ("from seg" if k == 0xE4 else "to seg", self.num())
            self.emit(at, "%s %s%s" % (STMT[b], it, extra))
        elif b == 0xC1:
            op = self.byte()
            self.emit(at, "XP %s %s" % (ARITH.get(op, "op%02X" % op), self.num()))
        elif b == 0xC2:
            self.emit(at, "ANGER %s" % self.arith())
        elif b == 0xC3:
            w = self.who()
            g = self.rng()
            op = self.byte()
            self.emit(at, "ANGER_NPCS %s %s %s %s" % (w, g, ARITH.get(op, "op%02X" % op), self.num()))
        elif b == 0xC4:
            self.emit(at, "TRIGGER seg %s %s" % (self.num(), self.side()))
        elif b in (0xC5, 0xC6):
            k = self.byte()
            if k <= 7:
                self.emit(at, "%s flag%d%s" % (STMT[b], k, self.objref()))
            else:
                n = self.num()
                self.st["%s gflag %s" % (STMT[b], n)] += 1
                self.emit(at, "%s gflag %s%s" % (STMT[b], n, "" if k == 0xF1 else " (prefix 0x%02X)" % k))
        elif b == 0xC7:
            self.emit(at, "GOTO %s" % self.num())
        elif b in (0xC8, 0xCC):
            self.emit(at, STMT[b])
        elif b == 0xCD:
            self.say(at)
        elif b == 0xD3:
            s = self.num()
            v = self.num()
            loop = ""
            if self.peek() == 0xD8:
                self.p += 1
                loop = " D8"
            self.emit(at, "SOUND %s vol %s%s" % (s, v, loop))
        elif b == 0xD4:
            s, v, o, n = self.num(), self.num(), self.num(), self.num()
            loop = ""
            if self.peek() == 0xD9:
                self.p += 1
                loop = " LOOP"
            self.emit(at, "SOUND_OBJ %s vol %s obj %s %s%s" % (s, v, o, n, loop))
        elif b == 0xD5:
            s, v, g = self.num(), self.num(), self.num()
            sd = self.side()
            n = self.num()
            loop = ""
            if self.peek() == 0xD9:
                self.p += 1
                loop = " LOOP"
            self.emit(at, "SOUND_SEG %s vol %s seg %s %s %s%s" % (s, v, g, sd, n, loop))
        elif b == 0xD6:
            self.emit(at, "STOP_SOUND_OBJ %s" % self.num())
        elif b == 0xD7:
            s = self.num()
            self.emit(at, "STOP_SOUND_SEG %s seg %s %s" % (s, self.num(), self.side()))
        elif b == 0xDA:
            g = self.num()
            self.emit(at, "TELEPORT_SELF seg %s %s" % (g, self.facing()))
        elif b == 0xDB:
            lv, g = self.num(), self.num()
            self.emit(at, "TELEPORT_PLAYER level %s seg %s %s" % (lv, g, self.facing()))
        elif b == 0xDC:
            o, g = self.num(), self.num()
            self.emit(at, "TELEPORT_OBJ obj %s seg %s %s" % (o, g, self.facing()))
        elif b == 0xDD:
            it = self.item()
            g1, g2 = self.num(), self.num()
            self.emit(at, "MOVE_ITEM %s seg %s -> seg %s" % (it, g1, g2))
        elif b == 0xDE:
            w = self.who()
            g = self.num()
            self.emit(at, "SUMMON %s seg %s %s" % (w, g, self.facing()))
        elif b == 0xDF:
            k = self.peek()
            if k == 0x15:
                self.p += 1
                o = self.num()
                self.emit(at, "RESUME obj %s %s" % (o, self.arith()))
            elif 0x7A <= k <= 0x7E:
                self.emit(at, "RESUME %s" % self.arith())
            else:
                self.emit(at, "RESUME here")
        elif b == 0xE0:
            o = self.objref()
            self.emit(at, "VAR%s %s" % (o, self.arith()))
        elif b == 0xE5:
            segs = []
            while self.peek() != 0xE6:
                segs.append(str(self.hexstr()))
            self.p += 1
            self.emit(at, "WALK %s" % " ".join(segs))
        elif b in (0xEA, 0xEB):
            n = self.num()
            self.emit(at, "%s spell %s %s %s" % (STMT[b], n, "obj" if b == 0xEA else "seg", self.num()))
        elif b == 0xEC:
            self.emit(at, "WAIT %s" % self.num())
        elif b == 0xED:
            if self.peek() == 0xC4:
                self.p += 1
                g = self.num()
                self.emit(at, "TRIGGER_OFF seg %s %s" % (g, self.side()))
            else:
                self.emit(at, "KILL obj %s" % self.num())
        elif b == 0xEE:
            self.emit(at, "CALL obj %s" % self.num())
        elif b == 0xF2:
            self.if_line(at)
        elif self.else_endif(at, b):
            pass
        else:
            raise ParseError("unknown statement code 0x%02X at 0x%X" % (b, at))

    def parse(self):
        labels = []
        while self.p < len(self.d):
            at = self.p
            if self.byte() != 0xCB:
                raise ParseError("block does not start with CB at 0x%X" % at)
            if self.peek() == 0xE7:
                self.p += 1
                lab = "E7 %d" % self.hexstr()
                labels.append(lab)
            else:
                v = self.hexstr()
                labels.append(v)
                lab = "%d (0x%X)" % (v, v)
            if self.ifd:
                raise ParseError("block starts inside an IF at 0x%X" % at)
            self.out.append("")
            self.emit(at, "LABEL %s" % lab)
            self.depth = 1
            # Labels 0x3E9 to 0x3EC hold text lines that 0xA8D72 reads after a goto
            if self.p < len(self.d) and self.d[self.p] < 0x7A:
                at = self.p
                self.emit(at, "LINES %s" % self.text())
                self.st["line blocks"] += 1
            while self.p < len(self.d) and self.d[self.p] != 0xCB:
                self.statement()
            self.depth -= 1
            if self.ifd != 0:
                raise ParseError("block ends inside an IF (depth %d) at 0x%X" % (self.ifd, self.p))
        return labels


def load(rlh_path):
    rlh = jrg.read_file(rlh_path)
    dat = jrg.read_file(jrg.find_partner(rlh_path, "DAT"))
    recs = []
    for rid, size, off in jrg.parse_rlh(rlh):
        plain = jrg.gab_decrypt(dat[off:off + size])
        recs.append((rid & 0x1FF, plain))
    return recs


def cmd_dis(path, ids):
    for sid, data in load(path):
        if ids and sid not in ids:
            continue
        s = Script(data, sid)
        print("; script %d, %d bytes" % (sid, len(data)))
        try:
            s.parse()
            err = None
        except ParseError as e:
            err = str(e)
        print("\n".join(s.out))
        if err:
            print("; ERROR: %s" % err)
        print()


def cmd_check(gamedir):
    files = sorted([f for f in glob.glob(os.path.join(gamedir, "*")) if os.path.basename(f).upper()[:3] == "GAB" and f.upper().endswith(".RLH")])
    bad = 0
    for f in sorted(set(files)):
        recs = load(f)
        nbytes = 0
        errs = []
        for sid, data in recs:
            s = Script(data, sid)
            try:
                s.parse()
                nbytes += len(data)
            except ParseError as e:
                errs.append("id %d: %s" % (sid, e))
        if errs:
            bad += 1
            print("%s: %d records, %d errors" % (os.path.basename(f), len(recs), len(errs)))
            for e in errs[:20]:
                print("  " + e)
        else:
            print("%s: OK, %d records, %d bytes parsed to the end, no unknown code" % (os.path.basename(f), len(recs), nbytes))
    return 1 if bad else 0


def cmd_stats(path):
    st = Counter()
    for sid, data in load(path):
        try:
            Script(data, sid, st).parse()
        except ParseError as e:
            print("id %d: %s" % (sid, e))
    for k in sorted(st):
        print("%6d  %s" % (st[k], k))


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    if argv[1] == "dis":
        return cmd_dis(argv[2], set(int(x, 0) for x in argv[3:])) or 0
    if argv[1] == "disraw":
        # a decoded record in a file (gab_ID.bin of the option -scriptdump
        # of the game: the record with the patches of the port)
        with open(argv[2], "rb") as f:
            data = f.read()
        s = Script(data, int(argv[3], 0) if len(argv) > 3 else 0)
        print("; %s, %d bytes" % (argv[2], len(data)))
        try:
            s.parse()
            err = None
        except ParseError as e:
            err = str(e)
        print("\n".join(s.out))
        if err:
            print("; ERROR: %s" % err)
            return 1
        return 0
    if argv[1] == "check":
        return cmd_check(argv[2])
    if argv[1] == "stats":
        return cmd_stats(argv[2]) or 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
