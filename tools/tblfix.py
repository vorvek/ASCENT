#!/usr/bin/env python3
"""The table fixup of DTUGAME.EXE 1.3 (0x1A344) for the tools.

engine/main/dtutblfix.c has the list of the writes (Fixups[]) and the
names of the lines that they need (Guard[]). This module reads the two
lists from the C file and applies them to the record lines of a table
(wiki_rules.Table) in the same way as the port: each write changes the text
of a line (key=value, a flag word, a name). See engine/main/dtutblfix.h and
docs/re/objects.md, "The table fixup".

  import tblfix
  changed = tblfix.apply(table)   # None: another table (the guard fails)
"""
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
SOURCE = os.path.join(ROOT, "engine", "main", "dtutblfix.c")

SECTION = {"M": "$OBJECT", "A": "$ARMOR", "W": "$WEAPON", "S": "$SPELL"}
OP_ROW = re.compile(r'\{\s*(T_\w+),\s*([MAWS]|0),\s*(-?\d+),\s*(-?\d+),\s*"([^"]*)",\s*"([^"]*)"\s*\}')
GUARD_ROW = re.compile(r'\{\s*([MAWS]),\s*(\d+),\s*"([^"]*)"\s*\}')
CAST_TIMES = re.compile(r'Cast_times\[\d+\]\s*=\s*\{([^}]*)\}', re.S)


def read_source(path=SOURCE):
    """(ops, guard, cast_times) of dtutblfix.c."""
    with open(path, encoding="latin-1") as f:
        text = f.read()
    ops, guard = [], []
    part = text.split("static const dtu_tblfix_guard Guard[]")
    for line in part[0].splitlines():
        m = OP_ROW.search(line)
        if m:
            ops.append(dict(op=m.group(1), kind=m.group(2), id=int(m.group(3)), src=int(m.group(4)),
                            key=m.group(5), val=m.group(6)))
    for line in part[1].splitlines() if len(part) > 1 else []:
        m = GUARD_ROW.search(line)
        if m:
            guard.append((m.group(1), int(m.group(2)), m.group(3)))
    m = CAST_TIMES.search(text)
    times = [int(v) for v in re.findall(r"-?\d+", m.group(1))] if m else []
    return ops, guard, times


# ---------------------------------------------------------------------
# The text of a line (the part after the section word)

def words(text):
    """(start, end) of each word; a quoted part can have spaces."""
    out, i, n = [], 0, len(text)
    while i < n:
        while i < n and text[i] in " \t":
            i += 1
        if i >= n:
            break
        s, q = i, False
        while i < n:
            if text[i] == '"':
                q = not q
            elif text[i] in " \t" and not q:
                break
            i += 1
        out.append((s, i))
    return out


def _is_key(w, key):
    return len(w) > len(key) and w[:len(key)].lower() == key.lower() and w[len(key)] == "="


def get(text, key):
    for s, e in words(text):
        w = text[s:e]
        if _is_key(w, key):
            return w[len(key) + 1:].replace('"', "")
    return None


def has(text, word):
    return any(text[s:e].lower() == word.lower() for s, e in words(text))


def _kv(key, value, quote=False):
    if quote or not value or " " in value or "\t" in value:
        value = '"%s"' % value
    return "%s=%s" % (key, value)


def set_key(text, key, value, quote=False):
    out, last, found = [], 0, False
    for s, e in words(text):
        if _is_key(text[s:e], key):
            out.append(text[last:s])
            if value is not None and not found:
                out.append(_kv(key, value, quote))
            found = True
            last = e
    out.append(text[last:])
    t = "".join(out)
    if not found and value is not None:
        t += " " + _kv(key, value, quote)
    return t


def set_word(text, word, on):
    if on:
        return text if has(text, word) else text + " " + word
    out, last = [], 0
    for s, e in words(text):
        if text[s:e].lower() == word.lower():
            out.append(text[last:s])
            last = e
    out.append(text[last:])
    return "".join(out)


SPELL_BITS = [("stackable", 0x002), ("cast_with_wand", 0x004), ("cast_with_scroll", 0x008),
              ("cast_with_arrow", 0x010), ("potion", 0x020), ("always", 0x040), ("homing", 0x080),
              ("self_cast_ok", 0x100), ("non_hotkey_item", 0x200), ("hit_objects", 0x800)]


def spell_flags(text):
    f = 0
    for s, e in words(text):
        w = text[s:e].lower()
        if w == "non_hotkey_item":
            f |= 0x200
        elif w == "cast_with_hands":
            f &= ~0x1C
        elif w == "cast_in_front":
            f |= 0x500
        elif w == "cast_with_wand":
            f = (f & ~0x1C) | 0x04
        elif w == "cast_with_scroll":
            f = (f & ~0x14) | 0x08
        elif w == "cast_with_arrow":
            f = (f & ~0x0C) | 0x10
        elif w in ("stackable", "potion", "always", "homing", "self_cast_ok", "hit_objects"):
            f |= dict(SPELL_BITS)[w]
        elif w == "type=target":
            f &= ~1
        elif w == "type=area":
            f |= 1
    return f


def set_spell_flags(text, flags):
    have = spell_flags(text)
    t = text
    if (have & 1) != (flags & 1):
        t = set_key(t, "type", "AREA" if flags & 1 else "TARGET")
    if (have & 0x500) != (flags & 0x500):
        t = set_word(t, "cast_in_front", (flags & 0x500) == 0x500)
        t = set_word(t, "self_cast_ok", (flags & 0x500) == 0x100)
    if (have & 0x1C) != (flags & 0x1C):
        t = set_word(t, "cast_with_hands", False)
    for word, bit in SPELL_BITS:
        if bit == 0x100:
            continue
        if (have & bit) != (flags & bit) or bit & 0x1C:
            t = set_word(t, word, bool(flags & bit))
    return t


CLASS_BITS = [("cleric_use", 0x01), ("mage_use", 0x02), ("fighter_use", 0x04), ("thief_use", 0x20)]
WEAR_WORDS = ["helmet", "cloak", "necklace", "armor", "gauntlet", "shield", "ring", "belt", "boot",
              "bow", "crossbow", "lockpick"]


def class_mask(text):
    m = 0xFFFF if has(text, "all_use") else 0
    for w, b in CLASS_BITS:
        if has(text, w):
            m |= b
    if has(text, "lockpick"):
        m |= 0x20
    return m


def set_class(text, mask):
    all_use = (mask & 0xFFFF) == 0xFFFF
    t = text if all_use else set_word(text, "all_use", False)
    for w, b in CLASS_BITS:
        t = set_word(t, w, bool(mask & b) and not all_use)
    if all_use:
        t = set_word(t, "all_use", True)
    return t


NAME_KEYS = ("name", "weapon_name", "spell_name")


def name_of(text):
    """The name that the reader keeps: the last name key of the line."""
    last = None
    for s, e in words(text):
        w = text[s:e]
        for k in NAME_KEYS:
            if _is_key(w, k):
                last = w[len(k) + 1:].replace('"', "")
    return last


def set_name(text, name):
    found = False
    for k in NAME_KEYS:
        if get(text, k) is not None:
            text = set_key(text, k, name, True)
            found = True
    return text if found else set_key(text, "name", name, True)


# ---------------------------------------------------------------------
# Apply

class Lines:
    """The lines of a wiki_rules.Table by table letter, as texts."""

    def __init__(self, table):
        self.table = table
        self.texts = {}
        for k, sec in SECTION.items():
            self.texts[k] = []
            for rec in table[sec]:
                parts = rec.line.split(None, 1)
                self.texts[k].append(parts[1] if len(parts) > 1 else "")

    def store(self):
        import wiki_rules
        for k, sec in SECTION.items():
            recs = self.table[sec]
            for i, t in enumerate(self.texts[k]):
                line = sec + " " + t
                if recs[i].line != line:
                    recs[i] = wiki_rules.Record(line)


def guard_ok(lines, guard):
    for k, i, name in guard:
        t = lines.texts[k]
        if i >= len(t) or (name_of(t[i]) or "").rstrip(" ") != name:
            return False
    return True


def apply_lines(lines, ops=None, guard=None, times=None):
    """Apply the writes to lines (Lines). Returns the set of changed lines
    (letter, number), or None when the guard fails."""
    if ops is None:
        ops, guard, times = read_source()
    if not guard_ok(lines, guard):
        return None
    T = lines.texts
    changed = set()

    def put(k, i, new):
        if new != T[k][i]:
            T[k][i] = new
            changed.add((k, i))

    for o in ops:
        op, k, i, src, key, val = o["op"], o["kind"], o["id"], o["src"], o["key"], o["val"]
        if k != "0" and i >= len(T[k]):
            continue
        t = T[k][i] if k != "0" else None
        s = None
        if src >= 0:
            sk = val if op == "T_COPY" and val else k
            if src >= len(T[sk]):
                continue
            s = T[sk][src]
        if op == "T_SET":
            put(k, i, set_key(t, key, val if val else None))
        elif op == "T_COPY":
            put(k, i, set_key(t, key, get(s, key)))
        elif op == "T_NAME":
            put(k, i, set_name(t, val))
        elif op == "T_CLASS":
            put(k, i, set_class(t, int(val, 0)))
        elif op == "T_CLASS_OR":
            put(k, i, set_class(t, class_mask(t) | int(val, 0)))
        elif op == "T_WEAR_OR":
            n = t
            for j, w in enumerate(WEAR_WORDS):
                if int(val, 0) & (1 << j):
                    n = set_word(n, w, True)
            put(k, i, n)
        elif op == "T_FLAGS":
            put(k, i, set_spell_flags(t, int(val, 0)))
        elif op == "T_FLAGS_OR":
            put(k, i, set_spell_flags(t, spell_flags(t) | int(val, 0)))
        elif op == "T_FLAGS_AND":
            put(k, i, set_spell_flags(t, spell_flags(t) & int(val, 0)))
        elif op == "T_FLAGS_COPY":
            put(k, i, set_spell_flags(t, spell_flags(s)))
        elif op == "T_RENDER":
            n = set_key(t, "weapon_pof", get(s, "weapon_pof"))
            if val == "1":
                n = set_key(n, "weapon_vclip", get(s, "weapon_vclip"))
            put(k, i, n)
        elif op == "T_COPYREC":
            name = name_of(t) or ""
            n = s
            if k == "S":
                ws, wd = s.split(None, 1), t.split(None, 1)
                if ws and wd and ws[0].isdigit() and wd[0].isdigit():
                    n = s[:len(s) - len(s.lstrip())] + wd[0] + s.lstrip()[len(ws[0]):]
            put(k, i, set_name(n, name))
        elif op == "T_MULVAL":
            v = get(s, key)
            put(k, i, set_key(t, key, str((int(v) if v else 0) * int(val, 0))))
        elif op == "T_CAST_TIMES":
            for j in range(min(len(T["S"]), len(times))):
                put("S", j, set_key(T["S"][j], "cast_time", str(times[j])))
        elif op in ("T_MAGIC_WEAPONS", "T_MAGIC_ARMOR"):
            kk = "W" if op == "T_MAGIC_WEAPONS" else "A"
            for j, wt in enumerate(T[kk]):
                plot = get(wt, "plot_item")
                try:
                    plot = int(plot or 0) != 0
                except ValueError:
                    plot = False
                if plot or (kk == "W" and "+" in (name_of(wt) or "")):
                    put(kk, j, set_word(wt, "magic", True))
    return changed


def apply(table):
    """Apply the fixup to a wiki_rules.Table in place. Returns the set of
    changed lines, or None when the table has other lines (no change)."""
    lines = Lines(table)
    changed = apply_lines(lines)
    if changed is not None:
        lines.store()
    return changed
