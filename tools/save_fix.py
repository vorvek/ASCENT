#!/usr/bin/env python3
"""save_fix.py: list and correct the item lists of a save game (SGn.SG).

  python -I tools/save_fix.py SAVE.SG                      list the items and the key items
  python -I tools/save_fix.py SAVE.SG --plot-counts --out NEW.SG
  python -I tools/save_fix.py SAVE.SG --remove misc:126 --out NEW.SG
  python -I tools/save_fix.py SAVE.SG --count misc:126=1 --in-place

Actions (any number, in this order):

  --plot-counts     the count 1 for each entry of the plot items whose level
                    record has a scroll text number in its count byte: misc
                    104 (Ruldegost Document, 21 in DROW5) and misc 126
                    (Scroll of the Illithids, 18 in DROW3). 1.3 and the port
                    before Dtu_fix_plot_count gave that many copies.
  --count T:ID=N    the count N for each entry of type T and id ID.
  --remove T:ID     remove each entry of type T and id ID. The entries after
                    it move up, and the references to the item list (hot key
                    boxes, the item in the hand, the paper doll, the bow and
                    crossbow slots) follow them, as 0xB6F01 and 0x31C4B do. A
                    reference to the removed entry becomes empty.

T is weapon, armor, misc or spell (or the type number 5, 15, 16, 17). A key
item (misc 107 to 124) is in the key list; --count and --remove act on it
too. Each Violet Gem Key (misc 107 to 124) is a different key of a door:
the level design gives the player one of each, so the tool leaves them.

The output is --out FILE, or the input itself with --in-place, which first
copies it to FILE.bak (it refuses when FILE.bak exists). Only the bytes of the
lists, their counts and the references change. The level state (SAVE\\SGn.*)
is not read. The layout is docs/formats/misc.md (SG0.SG) and
docs/re/character.md (the player record at 141, the item list at P+0x103,
the key list at P+0x18D3, the references).

The tool needs the Python 3 standard library only.
"""
import argparse
import os
import shutil
import struct
import sys

P = 141                       # the player record (DTU_SG_PLAYER_OFFSET)
INV_COUNT = P + 0xF3          # int, at most 127
KEY_COUNT = P + 0xFF          # int, at most 22
INV = P + 0x103               # 127 x 16
KEYS = P + 0x18D3             # 22 x 16
INV_MAX, KEY_MAX = 127, 22
# the references (list byte, index byte) to the item list
REFS = ([P + 0x1AB9, P + 0x1ABB] + [P + 0x1AC1 + 2 * i for i in range(10)]
        + [P + 0x1AD7] + [P + 0x1AD9 + 2 * i for i in range(10)])
TYPES = {'weapon': 5, 'armor': 15, 'misc': 16, 'spell': 17}
TYPE_NAMES = {v: k for k, v in TYPES.items()}
PLOT_TEXT_COUNT = {(16, 104), (16, 126)}


def item_key(s):
    t, _, i = s.partition(':')
    t = t.strip().lower()
    t = TYPES[t] if t in TYPES else int(t)
    return t, int(i)


def entries(d, base, n):
    return [bytearray(d[base + 16 * k:base + 16 * k + 16]) for k in range(n)]


def show(d):
    n_inv = struct.unpack_from('<i', d, INV_COUNT)[0]
    n_key = struct.unpack_from('<i', d, KEY_COUNT)[0]
    print('name %r, %d items, %d key items' % (d[P:P + 23].split(b'\0')[0].decode('latin-1'), n_inv, n_key))
    for name, base, n in (('item', INV, n_inv), ('key', KEYS, n_key)):
        for k, e in enumerate(entries(d, base, n)):
            lvl, cnt = struct.unpack_from('<hh', e, 6)
            print('  %s %3d: %s %d, count %d, level %d%s' % (name, k, TYPE_NAMES.get(e[0], str(e[0])), e[1], cnt, lvl,
                                                        ', plot item' if e[10] else ''))


def set_count(d, base, n, key, count, what):
    done = 0
    for k in range(n):
        o = base + 16 * k
        if (d[o], d[o + 1]) == key:
            old = struct.unpack_from('<h', d, o + 8)[0]
            if old != count:
                struct.pack_into('<h', d, o + 8, count)
                print('%s %d: %s %d, count %d -> %d' % (what, k, TYPE_NAMES.get(key[0], key[0]), key[1], old, count))
                done += 1
    return done


def remove(d, key):
    done = 0
    for base, count_at, size, is_inv in ((INV, INV_COUNT, INV_MAX, True), (KEYS, KEY_COUNT, KEY_MAX, False)):
        n = struct.unpack_from('<i', d, count_at)[0]
        k = 0
        while k < n:
            o = base + 16 * k
            if (d[o], d[o + 1]) != key:
                k += 1
                continue
            cnt = struct.unpack_from('<h', d, o + 8)[0]
            empty = bytes(d[base + 16 * (size - 1):base + 16 * size])
            d[o:base + 16 * (size - 1)] = d[o + 16:base + 16 * size]
            d[base + 16 * (size - 1):base + 16 * size] = empty
            n -= 1
            struct.pack_into('<i', d, count_at, n)
            print('%s %d: %s %d (count %d) removed' % ('item' if is_inv else 'key', k,
                                                     TYPE_NAMES.get(key[0], key[0]), key[1], cnt))
            done += 1
            if is_inv:
                for r in REFS:
                    lst, idx = d[r], struct.unpack_from('<b', d, r + 1)[0]
                    if lst != 0 or idx < 0:
                        continue
                    if idx == k:
                        struct.pack_into('<b', d, r + 1, -1)
                        print('  reference at P+0x%X: item %d -> none' % (r - P, idx))
                    elif idx > k:
                        struct.pack_into('<b', d, r + 1, idx - 1)
                        print('  reference at P+0x%X: item %d -> %d' % (r - P, idx, idx - 1))
    return done


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('save')
    ap.add_argument('--plot-counts', action='store_true')
    ap.add_argument('--count', action='append', default=[], metavar='T:ID=N')
    ap.add_argument('--remove', action='append', default=[], metavar='T:ID')
    ap.add_argument('--out')
    ap.add_argument('--in-place', action='store_true')
    a = ap.parse_args(argv)

    d = bytearray(open(a.save, 'rb').read())
    if len(d) < 7374 or d[:4] != b'DGSS':
        print('%s: not a save game (DGSS, at least 7374 bytes)' % a.save)
        return 1
    n_inv = struct.unpack_from('<i', d, INV_COUNT)[0]
    n_key = struct.unpack_from('<i', d, KEY_COUNT)[0]
    if not (0 <= n_inv <= INV_MAX and 0 <= n_key <= KEY_MAX):
        print('%s: item counts %d and %d out of range' % (a.save, n_inv, n_key))
        return 1
    if not (a.plot_counts or a.count or a.remove):
        show(d)
        return 0
    if bool(a.out) == a.in_place:
        print('name --out FILE or --in-place')
        return 1

    changed = 0
    if a.plot_counts:
        for k in range(n_inv):
            o = INV + 16 * k
            old = struct.unpack_from('<h', d, o + 8)[0]
            if (d[o], d[o + 1]) in PLOT_TEXT_COUNT and old > 1:
                struct.pack_into('<h', d, o + 8, 1)
                print('item %d: %s %d, count %d -> 1' % (k, TYPE_NAMES[d[o]], d[o + 1], old))
                changed += 1
    for c in a.count:
        k, _, v = c.partition('=')
        key = item_key(k)
        changed += set_count(d, INV, n_inv, key, int(v), 'item') + set_count(d, KEYS, n_key, key, int(v), 'key')
    for r in a.remove:
        changed += remove(d, item_key(r))

    if not changed:
        print('nothing to change')
        return 0
    if a.in_place:
        bak = a.save + '.bak'
        if os.path.exists(bak):
            print('%s exists: move it away first' % bak)
            return 1
        shutil.copy2(a.save, bak)
        print('backup: %s' % bak)
        out = a.save
    else:
        out = a.out
    with open(out, 'wb') as f:
        f.write(d)
    print('wrote %s' % out)
    return 0


if __name__ == '__main__':
    sys.exit(main())
