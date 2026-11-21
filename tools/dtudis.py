#!/usr/bin/env python3
"""Reverse engineering helper for DTUGAME.EXE (Descent to Undermountain 1.3).

Works on gamedata/re/dtu_flat.bin: the linear executable image with fixups
applied, object 1 at linear address 0x10000. Needs the capstone install in
gamedata/re/pylib. This tool reads only. It is used for the documents in
docs/re/.

  python -I tools/dtudis.py strings PATTERN        strings that match a regex
  python -I tools/dtudis.py xref ADDR              places that hold the 32-bit value ADDR
  python -I tools/dtudis.py callers ADDR           call instructions that target ADDR
  python -I tools/dtudis.py func ADDR              start of the function that holds ADDR
  python -I tools/dtudis.py dis ADDR [COUNT|END]   disassembly with string and call notes
  python -I tools/dtudis.py fn ADDR                disassembly of the whole function
  python -I tools/dtudis.py dump ADDR LEN [fmt]    data dump (fmt: b, w, d, s)
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
RE_DIR = os.path.join(HERE, "..", "gamedata", "re")
sys.path.insert(0, os.path.join(RE_DIR, "pylib"))

BASE = 0x10000
_img = None


def img():
    global _img
    if _img is None:
        with open(os.path.join(RE_DIR, "dtu_flat.bin"), "rb") as f:
            _img = f.read()
    return _img


def rd(addr, n):
    o = addr - BASE
    return img()[o:o + n]


def u32(addr):
    return struct.unpack_from("<I", img(), addr - BASE)[0]


def cstr(addr, maxlen=200):
    d = img()
    o = addr - BASE
    if o < 0 or o >= len(d):
        return None
    e = d.find(b"\0", o, o + maxlen)
    if e < 0 or e == o:
        return None
    s = d[o:e]
    if len(s) < 3:
        return None
    for c in s:
        if c < 9 or (13 < c < 32) or c > 126:
            return None
    return s.decode("latin-1")


def strings(pattern):
    rx = re.compile(pattern.encode("latin-1"), re.I)
    d = img()
    out = []
    for m in re.finditer(rb"[\x09\x0a\x0d\x20-\x7e]{4,}", d):
        if rx.search(m.group()):
            out.append((m.start() + BASE, m.group().decode("latin-1")))
    return out


def xrefs(addr):
    pat = struct.pack("<I", addr)
    d = img()
    out = []
    i = d.find(pat)
    while i >= 0:
        out.append(i + BASE)
        i = d.find(pat, i + 1)
    return out


def callers(addr, lo=0x10000, hi=None):
    d = img()
    hi = hi or code_end()
    out = []
    i = d.find(b"\xe8", lo - BASE)
    while 0 <= i < hi - BASE:
        rel = struct.unpack_from("<i", d, i + 1)[0]
        if i + BASE + 5 + rel == addr:
            out.append(i + BASE)
        i = d.find(b"\xe8", i + 1)
    return out


def code_end():
    return 0x11F000


_starts = None


def func_starts():
    """Function starts: all targets of call rel32 instructions inside the code."""
    global _starts
    if _starts is None:
        d = img()
        hi = code_end() - BASE
        s = set()
        i = d.find(b"\xe8")
        while 0 <= i < hi:
            rel = struct.unpack_from("<i", d, i + 1)[0]
            t = i + 5 + rel
            if 0 <= t < hi:
                s.add(t + BASE)
            i = d.find(b"\xe8", i + 1)
        _starts = sorted(s)
    return _starts


def func_of(addr):
    import bisect
    st = func_starts()
    i = bisect.bisect_right(st, addr) - 1
    return st[i] if i >= 0 else None


def next_func(addr):
    import bisect
    st = func_starts()
    i = bisect.bisect_right(st, addr)
    return st[i] if i < len(st) else code_end()


def dis(addr, end=None, count=None, names=None):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.skipdata = True
    if end is None:
        end = addr + (count or 60) * 16
    data = rd(addr, end - addr)
    lines = []
    n = 0
    for ins in md.disasm(data, addr):
        note = ""
        for m in re.finditer(r"0x([0-9a-f]{5,8})", ins.op_str):
            v = int(m.group(1), 16)
            if names and v in names:
                note += "  ; " + names[v]
            elif 0x11F000 <= v < BASE + len(img()):
                s = cstr(v)
                if s:
                    note += "  ; \"" + s[:70].replace("\n", "\\n") + "\""
        lines.append("%06x: %-7s %s%s" % (ins.address, ins.mnemonic, ins.op_str, note))
        n += 1
        if count and n >= count:
            break
    return lines


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    cmd = argv[1]
    if cmd == "strings":
        for a, s in strings(argv[2]):
            print("%06x %s" % (a, s[:160]))
    elif cmd == "xref":
        a = int(argv[2], 16)
        for x in xrefs(a):
            print("%06x in func %06x" % (x, func_of(x) or 0))
    elif cmd == "callers":
        a = int(argv[2], 16)
        for x in callers(a):
            print("%06x in func %06x" % (x, func_of(x) or 0))
    elif cmd == "func":
        a = int(argv[2], 16)
        print("%06x .. %06x" % (func_of(a), next_func(a)))
    elif cmd == "dis":
        a = int(argv[2], 16)
        if len(argv) > 3 and argv[3].startswith("0x"):
            print("\n".join(dis(a, end=int(argv[3], 16))))
        else:
            print("\n".join(dis(a, count=int(argv[3]) if len(argv) > 3 else 60)))
    elif cmd == "fn":
        a = func_of(int(argv[2], 16))
        print("\n".join(dis(a, end=next_func(a))))
    elif cmd == "dump":
        a = int(argv[2], 16)
        n = int(argv[3], 0)
        fmt = argv[4] if len(argv) > 4 else "b"
        d = rd(a, n)
        if fmt == "s":
            print(cstr(a, n))
        elif fmt == "d":
            for i in range(0, n - 3, 4):
                v = struct.unpack_from("<i", d, i)[0]
                print("%06x: %08x %d" % (a + i, v & 0xFFFFFFFF, v))
        elif fmt == "w":
            for i in range(0, n - 1, 2):
                print("%06x: %d" % (a + i, struct.unpack_from("<h", d, i)[0]))
        else:
            for i in range(0, n, 16):
                print("%06x: %s" % (a + i, d[i:i + 16].hex(" ")))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
