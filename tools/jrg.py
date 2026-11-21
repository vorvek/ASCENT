#!/usr/bin/env python3
"""jrg.py: list and extract the Descent to Undermountain front end archives.

Handles the .JRG index files with their .DAT data files, the GABx.RLH/GABx.DAT
dialogue archives, and the small GELDOR.DAT and KCONFIG.DAT files.
See docs/formats/jrg.md for the layouts.

Usage:
    python -I jrg.py list FILE
    python -I jrg.py extract FILE OUTDIR [--raw] [--only N,N,...] [--palette N]
    python -I jrg.py check GAMEDIR [--deep]

FILE is a .JRG, .RLH or .DAT file. The partner file is found by name in the
same directory. The kind of archive is chosen from the file name:

    FEBGND            LZSS compressed 8-bit images            -> PNG
    FESEQ             IFF ANIM brushes (Deluxe Paint)         -> .anm and PNG frames
    FESTREAM          raw 8-bit animation frames              -> PNG (grey, or --palette N
                                                                 to borrow the palette of
                                                                 FEBGND entry N)
    FETXT, FETXTx     nibble-swapped text strings             -> .txt
    WERDZ, WHAMMO     Interplay ACM audio (uses tun.py)       -> WAV
    DISCO             music file name table                   -> list only
    FESOUND           index of a data file that is not shipped-> list only
    GABx              XOR-encrypted dialogue scripts          -> .gab (decrypted) and .txt
    GELDOR, KCONFIG   small record files                      -> list only

--raw writes each entry exactly as stored. check verifies that every index is
consumed completely and that the entries cover the data file without gaps.
check --deep also decodes every audio entry (slow).

Python 3 standard library only.
"""

import importlib.util
import os
import re
import struct
import sys
import zlib

GAB_KEY = b"Although curiosity killed the cat, it still had 8 lives left to live.\x00"
NIBBLE_SWAP = bytes(((i << 4) & 0xF0) | (i >> 4) for i in range(256))


class FormatError(Exception):
    pass


# ---------------------------------------------------------------- helpers

def find_partner(path, ext):
    """Find the file with the same base name and another extension, ignoring case."""
    folder = os.path.dirname(os.path.abspath(path))
    base = os.path.splitext(os.path.basename(path))[0].upper()
    want = base + "." + ext.upper()
    for name in os.listdir(folder):
        if name.upper() == want:
            return os.path.join(folder, name)
    return None


def read_file(path):
    with open(path, "rb") as f:
        return f.read()


def load_tun_module():
    here = os.path.dirname(os.path.abspath(__file__))
    sys.dont_write_bytecode = True  # keep __pycache__ out of the repository
    spec = importlib.util.spec_from_file_location("tun", os.path.join(here, "tun.py"))
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def write_png(path, width, height, pixels, palette_rgb):
    """Write an 8-bit indexed PNG. palette_rgb holds 768 bytes of 8-bit RGB."""
    def chunk(tag, body):
        return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        rows += pixels[y * width:(y + 1) * width]
    data = (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0))
            + chunk(b"PLTE", bytes(palette_rgb[:768]))
            + chunk(b"IDAT", zlib.compress(bytes(rows), 6))
            + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(data)


def vga_palette_to_rgb(pal6):
    """Convert 768 bytes of 6-bit VGA palette to 8-bit RGB."""
    return bytes(((v & 63) << 2) | ((v & 63) >> 4) for v in pal6)


GREY_PALETTE = bytes(v for i in range(256) for v in (i, i, i))


def lzss_decode(src, pos, end):
    """Decode the FELZ stream src[pos:end]. Returns (output, position after the last byte used).

    Okumura LZSS: 4096 byte ring, write position starts at 0xFEE, flag bytes are
    read low bit first, flag 1 is a literal byte, flag 0 is a two byte match
    (12-bit ring position, 4-bit length minus 3).
    """
    ring = bytearray(b" " * 4096)
    r = 0xFEE
    out = bytearray()
    while pos < end:
        flags = src[pos]
        pos += 1
        for bit in range(8):
            if pos >= end:
                break
            if (flags >> bit) & 1:
                c = src[pos]
                pos += 1
                out.append(c)
                ring[r] = c
                r = (r + 1) & 4095
            else:
                if pos + 1 >= end:
                    raise FormatError("LZSS match cut off at the end of the entry")
                lo = src[pos]
                hi = src[pos + 1]
                pos += 2
                o = lo | ((hi & 0xF0) << 4)
                for k in range((hi & 15) + 3):
                    c = ring[(o + k) & 4095]
                    out.append(c)
                    ring[r] = c
                    r = (r + 1) & 4095
    return out, pos


def coverage(entries, data_size):
    """entries: list of (offset, size). Return a list of problem strings."""
    problems = []
    pos = 0
    for i, (off, size) in enumerate(entries):
        if off != pos:
            problems.append("entry %d starts at %d, expected %d" % (i, off, pos))
        pos = off + size
    if data_size is not None and pos != data_size:
        problems.append("entries end at %d, data file has %d bytes" % (pos, data_size))
    return problems


# ---------------------------------------------------------------- index parsers

def parse_counted_pairs(jrg):
    """u32 count, then count times (u32 offset, u32 size)."""
    if len(jrg) < 4:
        raise FormatError("index too short")
    n = struct.unpack_from("<I", jrg, 0)[0]
    if 4 + 8 * n != len(jrg):
        raise FormatError("index size %d does not match count %d" % (len(jrg), n))
    return [struct.unpack_from("<II", jrg, 4 + 8 * i) for i in range(n)]


def parse_febgnd(jrg):
    n = struct.unpack_from("<I", jrg, 0)[0]
    if 4 + 16 * n != len(jrg):
        raise FormatError("index size %d does not match count %d" % (len(jrg), n))
    return [struct.unpack_from("<IIII", jrg, 4 + 16 * i) for i in range(n)]  # offset, w, h, size


def parse_fetxt(jrg):
    if len(jrg) % 8:
        raise FormatError("index size %d is not a multiple of 8" % len(jrg))
    return [struct.unpack_from("<II", jrg, 8 * i) for i in range(len(jrg) // 8)]


def parse_festream(jrg):
    """Return (unknown0, [sequence dict])."""
    unknown0, n = struct.unpack_from("<II", jrg, 0)
    pos = 8
    seqs = []
    for _ in range(n):
        if pos + 24 > len(jrg):
            raise FormatError("sequence header runs past the end of the index")
        x, y, w, h, unknown, count = struct.unpack_from("<6I", jrg, pos)
        pos += 24
        if pos + 8 * count > len(jrg):
            raise FormatError("frame table runs past the end of the index")
        frames = [struct.unpack_from("<II", jrg, pos + 8 * i) for i in range(count)]
        pos += 8 * count
        seqs.append({"x": x, "y": y, "w": w, "h": h, "unknown": unknown, "frames": frames})
    if pos != len(jrg):
        raise FormatError("%d unexplained bytes at the end of the index" % (len(jrg) - pos))
    return unknown0, seqs


def parse_fesound(jrg):
    n, rate = struct.unpack_from("<II", jrg, 0)
    if 8 + 8 * n != len(jrg):
        raise FormatError("index size %d does not match count %d" % (len(jrg), n))
    return rate, [struct.unpack_from("<II", jrg, 8 + 8 * i) for i in range(n)]


def parse_disco(jrg):
    n = struct.unpack_from("<I", jrg, 0)[0]
    if 4 + 20 * n != len(jrg):
        raise FormatError("index size %d does not match count %d" % (len(jrg), n))
    out = []
    for i in range(n):
        name, value = struct.unpack_from("<16sI", jrg, 4 + 20 * i)
        out.append((name.split(b"\0")[0].decode("latin-1"), value))
    return out


def parse_rlh(rlh):
    if len(rlh) % 8:
        raise FormatError("RLH size %d is not a multiple of 8" % len(rlh))
    return [struct.unpack_from("<HHI", rlh, 8 * i) for i in range(len(rlh) // 8)]  # id, size, offset


def gab_decrypt(blob):
    """Each record is XORed with key[i % 70] ^ key[(0xFFFFFFFF - i) % 70], i counted per record."""
    k = GAB_KEY
    return bytes(b ^ k[i % 70] ^ k[(0xFFFFFFFF - i) % 70] for i, b in enumerate(blob))


# ---------------------------------------------------------------- IFF ANIM (FESEQ)

def iff_chunks(buf, pos, end):
    while pos + 8 <= end:
        tag = buf[pos:pos + 4]
        size = struct.unpack_from(">I", buf, pos + 4)[0]
        yield tag, pos + 8, size
        pos += 8 + size + (size & 1)


def pbm_body(buf, pos, size, w, h, compression):
    """Decode a PBM BODY chunk to w*h bytes. Rows are stored padded to an even width."""
    row = w + (w & 1)
    end = pos + size
    if compression == 0:
        raw = buf[pos:end]
    elif compression == 1:
        raw = bytearray()
        while pos < end and len(raw) < row * h:
            c = buf[pos]
            pos += 1
            if c < 128:
                raw += buf[pos:pos + c + 1]
                pos += c + 1
            elif c > 128:
                raw += bytes([buf[pos]]) * (257 - c)
                pos += 1
    else:
        raise FormatError("unknown BODY compression %d" % compression)
    if len(raw) != row * h:
        raise FormatError("BODY decodes to %d bytes, expected %d" % (len(raw), row * h))
    return bytearray(b"".join(bytes(raw[y * row:y * row + w]) for y in range(h)))


def anim_delta(buf, pos, size, w, h, prev):
    """Apply one DLTA chunk (ANHD operation 0x4B) to a copy of the previous frame."""
    end = pos + size
    pix = bytearray(prev)
    pos += 4  # u32 big endian, value 4 in every chunk of FESEQ.DAT, meaning unknown
    p = 0
    for y in range(h):
        items = buf[pos]
        pos += 1
        left = w
        for _ in range(items):
            code = buf[pos]
            pos += 1
            if code == 0:  # run
                rep = buf[pos]
                val = buf[pos + 1]
                pos += 2
                left -= rep
                if left == -1:
                    rep -= 1
                pix[p:p + rep] = bytes([val]) * rep
                p += rep
            elif code > 0x80:  # skip
                left -= code - 0x80
                p += code - 0x80
                if left == -1:
                    p -= 1
            else:  # literal
                left -= code
                keep = code - 1 if left == -1 else code
                pix[p:p + keep] = buf[pos:pos + keep]
                p += keep
                pos += code
        if left != 0 and not (left == -1 and (w & 1)):
            raise FormatError("DLTA row %d does not add up (%d left)" % (y, left))
    if end - pos not in (0, 1):
        raise FormatError("DLTA has %d unexplained bytes" % (end - pos))
    return pix


def decode_anim(buf, off, size):
    """Decode one FORM ANIM. Returns dict with w, h, palette (8-bit RGB), frames, chunk names."""
    if buf[off:off + 4] != b"FORM" or buf[off + 8:off + 12] != b"ANIM":
        raise FormatError("entry is not FORM ANIM")
    if struct.unpack_from(">I", buf, off + 4)[0] + 8 != size:
        raise FormatError("FORM size does not match the index entry size")
    info = {"frames": [], "chunks": [], "palette": GREY_PALETTE}
    pix = None
    for tag, p, l in iff_chunks(buf, off + 12, off + size):
        if tag != b"FORM" or buf[p:p + 4] != b"PBM ":
            raise FormatError("unexpected chunk %r inside ANIM" % tag)
        for t2, p2, l2 in iff_chunks(buf, p + 4, p + l):
            info["chunks"].append(t2.decode("latin-1"))
            if t2 == b"BMHD":
                (info["w"], info["h"], info["x"], info["y"], planes, info["masking"], info["compression"],
                 _pad, info["transparent"], _ax, _ay, info["page_w"], info["page_h"]) = \
                    struct.unpack_from(">HHhhBBBBHBBhh", buf, p2)
                if planes != 8:
                    raise FormatError("BMHD has %d planes, expected 8" % planes)
            elif t2 == b"CMAP":
                info["palette"] = bytes(buf[p2:p2 + l2]).ljust(768, b"\0")
            elif t2 == b"BODY":
                pix = pbm_body(buf, p2, l2, info["w"], info["h"], info["compression"])
                info["frames"].append(bytes(pix))
            elif t2 == b"ANHD":
                if buf[p2] != 0x4B:
                    raise FormatError("ANHD operation 0x%02x, expected 0x4b" % buf[p2])
            elif t2 == b"DLTA":
                if pix is None:
                    raise FormatError("DLTA before BODY")
                pix = anim_delta(buf, p2, l2, info["w"], info["h"], pix)
                info["frames"].append(bytes(pix))
    return info


# ---------------------------------------------------------------- archive kinds

def kind_of(path):
    base = os.path.splitext(os.path.basename(path))[0].upper()
    if base == "FEBGND":
        return "bgnd"
    if base == "FESEQ":
        return "seq"
    if base == "FESTREAM":
        return "stream"
    if base.startswith("FETXT"):
        return "txt"
    if base in ("WERDZ", "WHAMMO"):
        return "acm"
    if base == "DISCO":
        return "disco"
    if base == "FESOUND":
        return "sound"
    if re.fullmatch(r"GAB[A-Z]", base):
        return "gab"
    if base == "GELDOR":
        return "geldor"
    if base == "KCONFIG":
        return "kconfig"
    raise FormatError("do not know the archive kind of %s" % path)


class Archive:
    """One opened archive. entries is a list of dicts with at least 'desc'."""

    def __init__(self, path):
        self.kind = kind_of(path)
        self.problems = []
        self.entries = []
        self.header = ""
        self.data = None
        k = self.kind
        if k in ("geldor", "kconfig"):
            self.data = read_file(find_partner(path, "DAT") or path)
            getattr(self, "_open_" + k)()
            return
        if k == "gab":
            idx_path = find_partner(path, "RLH")
        else:
            idx_path = find_partner(path, "JRG")
        if idx_path is None:
            raise FormatError("index file for %s not found" % path)
        self.index = read_file(idx_path)
        dat_path = find_partner(path, "DAT")
        if dat_path is not None:
            self.data = read_file(dat_path)
        elif k not in ("disco", "sound"):
            raise FormatError("data file for %s not found" % path)
        getattr(self, "_open_" + k)()

    def _cover(self, pairs):
        self.problems += coverage(pairs, None if self.data is None else len(self.data))

    def _open_bgnd(self):
        ents = parse_febgnd(self.index)
        self.header = "%d images" % len(ents)
        for off, w, h, size in ents:
            self.entries.append({"off": off, "size": size, "w": w, "h": h,
                                 "desc": "image %dx%d, offset %d, size %d" % (w, h, off, size)})
        self._cover([(e["off"], e["size"]) for e in self.entries])

    def _open_seq(self):
        ents = parse_counted_pairs(self.index)
        self.header = "%d animations" % len(ents)
        for off, size in ents:
            self.entries.append({"off": off, "size": size, "desc": "IFF ANIM, offset %d, size %d" % (off, size)})
        self._cover(ents)

    def _open_stream(self):
        unknown0, seqs = parse_festream(self.index)
        self.header = "first u32 = %d (meaning unknown), %d sequences" % (unknown0, len(seqs))
        pairs = []
        for s in seqs:
            pairs += s["frames"]
            bad = [i for i, (o, z) in enumerate(s["frames"]) if z != s["w"] * s["h"]]
            if bad:
                self.problems.append("sequence has frames whose size is not w*h: %r" % bad)
            first = s["frames"][0][0] if s["frames"] else 0
            s["desc"] = ("sequence at screen (%d,%d), %dx%d, %d frames, first offset %d, unknown field %d"
                         % (s["x"], s["y"], s["w"], s["h"], len(s["frames"]), first, s["unknown"]))
            self.entries.append(s)
        self._cover(pairs)

    def _open_txt(self):
        ents = parse_fetxt(self.index)
        self.header = "%d strings" % len(ents)
        text = self.data.translate(NIBBLE_SWAP)
        for off, size in ents:
            s = text[off:off + size]
            if size == 0 or s[-1] != 0 or 0 in s[:-1]:
                self.problems.append("string at %d is not a single NUL-terminated string" % off)
            shown = s.rstrip(b"\0").decode("cp437", "replace").replace("\n", "\\n")
            self.entries.append({"off": off, "size": size, "text": s,
                                 "desc": "offset %d, size %d: %s" % (off, size, shown[:60])})
        self._cover(ents)

    def _open_acm(self):
        ents = parse_counted_pairs(self.index)
        self.header = "%d ACM audio streams" % len(ents)
        for off, size in ents:
            d = "offset %d, size %d" % (off, size)
            if self.data[off:off + 4] == b"\x97\x28\x03\x01" and size >= 14:
                total, ch, rate, packed = struct.unpack_from("<IHHH", self.data, off + 4)
                d += ", %d values, %d ch, %d Hz, level %d, rows %d, %.2f s" % (
                    total, ch, rate, packed & 15, packed >> 4, total / max(1, ch) / max(1, rate))
            else:
                self.problems.append("entry at %d has no ACM signature" % off)
            self.entries.append({"off": off, "size": size, "desc": d})
        self._cover(ents)

    def _open_disco(self):
        ents = parse_disco(self.index)
        self.header = "%d music slots" % len(ents)
        for name, value in ents:
            self.entries.append({"desc": "%-14s value %d" % (name or "(empty)", value)})

    def _open_sound(self):
        rate, ents = parse_fesound(self.index)
        self.header = "%d entries, second header field %d (sample rate, a guess)" % (len(ents), rate)
        for off, size in ents:
            self.entries.append({"off": off, "size": size, "desc": "offset %d, size %d" % (off, size)})
        if ents and ents[0][0] != 8:
            self.problems.append("first entry starts at %d, expected 8" % ents[0][0])
        pos = ents[0][0] if ents else 0
        for i, (off, size) in enumerate(ents):
            if off != pos:
                self.problems.append("entry %d starts at %d, expected %d" % (i, off, pos))
            pos = off + size
        self.header += ", data file would be %d bytes" % pos
        if self.data is None:
            self.header += " (FESOUND.DAT not present)"

    def _open_gab(self):
        ents = parse_rlh(self.index)
        self.header = "%d dialogue records" % len(ents)
        for ident, size, off in ents:
            self.entries.append({"off": off, "size": size, "id": ident,
                                 "desc": "id %d, offset %d, size %d" % (ident, off, size)})
        self._cover([(e["off"], e["size"]) for e in self.entries])

    def _open_geldor(self):
        d = self.data
        if len(d) % 10:
            self.problems.append("size %d is not a multiple of 10" % len(d))
        self.header = "%d records of 10 bytes" % (len(d) // 10)
        for i in range(len(d) // 10):
            a, b, c, e = struct.unpack_from("<iBBi", d, 10 * i)
            self.entries.append({"desc": "i32 %d, u8 %d, u8 %d, i32 %d" % (a, b, c, e)})

    def _open_kconfig(self):
        d = self.data
        if len(d) != 386:
            self.problems.append("size %d, expected 386" % len(d))
        self.header = "384 byte table plus 2 single bytes"
        for i in range(0, min(len(d), 384), 16):
            self.entries.append({"desc": "table +%3d: %s" % (i, d[i:i + 16].hex(" "))})
        if len(d) >= 386:
            self.entries.append({"desc": "byte 384: %d, byte 385: %d" % (d[384], d[385])})

    # ---- extraction

    def extract(self, outdir, raw=False, only=None, palette=None, deep_check=False):
        """Extract entries. With outdir None nothing is written (used by check)."""
        k = self.kind
        if k in ("disco", "geldor", "kconfig") or (k == "sound" and self.data is None):
            return 0
        if outdir is not None:
            os.makedirs(outdir, exist_ok=True)
        tun = None
        done = 0
        for i, e in enumerate(self.entries):
            if only is not None and i not in only:
                continue
            name = "%03d" % i

            def put(suffix, blob):
                if outdir is not None:
                    with open(os.path.join(outdir, name + suffix), "wb") as f:
                        f.write(blob)

            if k == "stream":
                for fi, (off, size) in enumerate(e["frames"]):
                    frame = self.data[off:off + size]
                    if raw:
                        put("_%02d.bin" % fi, frame)
                    elif outdir is not None:
                        write_png(os.path.join(outdir, "%s_%02d.png" % (name, fi)), e["w"], e["h"], frame,
                                  palette or GREY_PALETTE)
                done += 1
                continue
            blob = self.data[e["off"]:e["off"] + e["size"]]
            if k == "bgnd":
                pix, used = lzss_decode(self.data, e["off"] + 768, e["off"] + e["size"])
                if len(pix) != e["w"] * e["h"] or used != e["off"] + e["size"]:
                    raise FormatError("entry %d: LZSS gives %d bytes, expected %d"
                                      % (i, len(pix), e["w"] * e["h"]))
                if raw:
                    put(".bin", blob)
                elif outdir is not None:
                    write_png(os.path.join(outdir, name + ".png"), e["w"], e["h"], pix,
                              vga_palette_to_rgb(blob[:768]))
            elif k == "seq":
                anim = decode_anim(self.data, e["off"], e["size"])
                put(".anm", blob)
                if not raw and outdir is not None:
                    for fi, frame in enumerate(anim["frames"]):
                        write_png(os.path.join(outdir, "%s_%02d.png" % (name, fi)), anim["w"], anim["h"],
                                  frame, anim["palette"])
            elif k == "txt":
                put(".bin" if raw else ".txt", blob if raw else e["text"].rstrip(b"\0"))
            elif k == "acm":
                if raw:
                    put(".acm", blob)
                elif outdir is not None or deep_check:
                    if tun is None:
                        tun = load_tun_module()
                    info, samples, rep = tun.decode_acm(self.data, e["off"], e["size"])
                    if rep["bytes_used"] != rep["bytes_total"] or len(samples) != info["total_values"]:
                        self.problems.append("entry %d: used %d of %d bytes, %d of %d values"
                                             % (i, rep["bytes_used"], rep["bytes_total"], len(samples),
                                                info["total_values"]))
                    e["report"] = rep
                    put(".wav", tun.wav_bytes(samples, info["channels"], info["rate"]))
            elif k == "gab":
                name = "%03d_id%03d" % (i, e["id"])
                if raw:
                    put(".bin", blob)
                else:
                    plain = gab_decrypt(blob)
                    put(".gab", plain)
                    # Text view: every run of printable bytes that ends in NUL. This is a
                    # heuristic view of the script, not a parse of it.
                    lines = [m.group(1) for m in re.finditer(rb"([\x20-\x7e\r\n]{2,})\x00", plain)]
                    put(".txt", b"\n".join(lines) + b"\n")
            elif k == "sound":
                put(".bin", blob)
            done += 1
        return done


# ---------------------------------------------------------------- commands

def cmd_list(path):
    arc = Archive(path)
    print("%s: %s, %s" % (os.path.basename(path), arc.kind, arc.header))
    for i, e in enumerate(arc.entries):
        print("%4d  %s" % (i, e["desc"]))
    for p in arc.problems:
        print("PROBLEM: " + p)
    return 1 if arc.problems else 0


def cmd_extract(path, outdir, raw, only, palette_index):
    arc = Archive(path)
    palette = None
    if palette_index is not None:
        bg_idx = find_partner(os.path.join(os.path.dirname(os.path.abspath(path)), "FEBGND.JRG"), "JRG")
        if bg_idx is None:
            raise FormatError("--palette needs FEBGND.JRG and FEBGND.DAT next to the input file")
        bg = Archive(bg_idx)
        e = bg.entries[palette_index]
        palette = vga_palette_to_rgb(bg.data[e["off"]:e["off"] + 768])
    n = arc.extract(outdir, raw=raw, only=only, palette=palette)
    print("%s: extracted %d of %d entries to %s" % (os.path.basename(path), n, len(arc.entries), outdir))
    for p in arc.problems:
        print("PROBLEM: " + p)
    return 1 if arc.problems else 0


def cmd_check(gamedir, deep):
    status = 0
    seen = set()
    for name in sorted(os.listdir(gamedir)):
        base, ext = os.path.splitext(name.upper())
        if ext not in (".JRG", ".RLH") and name.upper() not in ("GELDOR.DAT", "KCONFIG.DAT"):
            continue
        if base in seen:
            continue
        seen.add(base)
        path = os.path.join(gamedir, name)
        try:
            arc = Archive(path)
            if arc.kind != "acm" or deep:
                arc.extract(None, deep_check=deep)
            note = ""
            if arc.kind == "acm" and not deep:
                note = " (headers only, use --deep to decode)"
            if arc.kind == "gab":
                bad = 0
                for e in arc.entries:
                    plain = gab_decrypt(arc.data[e["off"]:e["off"] + e["size"]])
                    if plain[:1] != b"\xcb" or plain[-1:] != b"\xcc":
                        bad += 1
                if bad:
                    arc.problems.append("%d records do not start with CB and end with CC after decryption" % bad)
            problems = arc.problems
            print("%-13s %-8s %s: %s%s" % (name, arc.kind, arc.header,
                                           "OK" if not problems else "%d PROBLEMS" % len(problems), note))
        except (FormatError, struct.error, IndexError) as ex:
            problems = ["%s: %s" % (type(ex).__name__, ex)]
            print("%-13s FAILED" % name)
        for p in problems[:10]:
            print("    PROBLEM: " + p)
        if problems:
            status = 1
    return status


def main(argv):
    raw = "--raw" in argv
    deep = "--deep" in argv
    only = None
    palette = None
    positional = []
    it = iter(argv[1:])
    for a in it:
        if a == "--only":
            only = set(int(v) for v in next(it).split(","))
        elif a == "--palette":
            palette = int(next(it))
        elif not a.startswith("--"):
            positional.append(a)
    args = positional
    try:
        if len(args) == 2 and args[0] == "list":
            return cmd_list(args[1])
        if len(args) == 3 and args[0] == "extract":
            return cmd_extract(args[1], args[2], raw, only, palette)
        if len(args) == 2 and args[0] == "check":
            return cmd_check(args[1], deep)
    except FormatError as ex:
        print("error: %s" % ex)
        return 1
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
