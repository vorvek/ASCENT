#!/usr/bin/env python3
"""egg.py: list and extract Descent to Undermountain .EGG archives.

An EGG file is the Undermountain version of the Descent 1 PIG file. It holds
bitmaps, sounds and polygon models. See docs/formats/egg.md for the layout.

Usage:
  egg.py list    FILE.EGG
  egg.py check   FILE.EGG [FILE.EGG ...]
  egg.py extract FILE.EGG OUTDIR [--palette PALETTE.256] [--wav RATE]
  egg.py bin     FILE.BIN [OUT.TXT]

Only the Python 3 standard library is used. The tool never writes to the
directory of the input file unless you name it as OUTDIR.
"""

import argparse
import os
import struct
import sys
import zlib

BITMAP_HDR = struct.Struct("<8sBBBBBI")        # 17 bytes
SOUND_HDR = struct.Struct("<8sIIII")           # 24 bytes
MODEL_HDR_SIZE = 218

# Bitmap dflags (header byte 8)
DBM_FLAG_ABM = 0x40        # animation frame, frame number in bits 0..5
DBM_FLAG_LARGE = 0x80      # add 256 to the width

# Bitmap flags (header byte 11)
BM_TRANSPARENT = 0x01
BM_SUPER_TRANSPARENT = 0x02
BM_NO_LIGHTING = 0x04
BM_RLE = 0x08
BM_UNKNOWN_10 = 0x10
BM_UNKNOWN_40 = 0x40

# Model header flags (header byte 0x3C). Each bit adds per-frame arrays.
PM_FLAG_1 = 0x01
PM_FLAG_2 = 0x02
PM_FLAG_4 = 0x04
PM_FLAG_8 = 0x08


class EggError(Exception):
    pass


def cstr(raw):
    return raw.split(b"\0", 1)[0].decode("latin-1")


class Bitmap:
    __slots__ = ("index", "name", "dflags", "width", "height", "flags",
                 "avg_color", "offset", "file_offset", "size")

    @property
    def frame(self):
        return self.dflags & 0x3F if self.dflags & DBM_FLAG_ABM else None

    @property
    def full_name(self):
        if self.dflags & DBM_FLAG_ABM:
            return "%s#%d" % (self.name, self.dflags & 0x3F)
        return self.name


class Sound:
    __slots__ = ("index", "name", "length", "data_length", "offset", "flags",
                 "file_offset")


class Model:
    __slots__ = ("index", "name", "n_models", "model_data_size", "mins",
                 "maxs", "rad", "n_textures", "first_texture", "unk_2c",
                 "unk_30", "n_frames", "offset", "flags", "file_offset",
                 "sections", "size", "header")


def model_sections(n, frames, flags, model_data_size):
    """Return the ordered (name, size) list of one model data block.

    The order and sizes come from the model page-in code of DTUGAME.EXE.
    The names of the arrays after submodel_maxs are guesses.
    """
    s = [
        ("model_data", model_data_size),
        ("submodel_ptrs", 4 * n),
        ("submodel_offsets", 12 * n),
        ("submodel_norms", 12 * n),
        ("submodel_pnts", 12 * n),
        ("submodel_rads", 4 * n),
        ("submodel_parents", n),
        ("submodel_mins", 12 * n),
        ("submodel_maxs", 12 * n),
        ("frame_vec_a", 12 * (frames + 1)),
        ("frame_vec_b", 12 * (frames + 1)),
    ]
    if frames:
        s.append(("anim_angles", 6 * frames * n))
        for i in range(7):
            s.append(("frame_vec_%d" % i, 12 * frames))
    if flags & PM_FLAG_1:
        s += [("f1_vec", 12 * (frames + 1)), ("f1_int_a", 4 * (frames + 1)),
              ("f1_int_b", 4 * (frames + 1))]
    if flags & PM_FLAG_8:
        s += [("f8_vec", 12 * frames), ("f8_int_a", 4 * frames),
              ("f8_int_b", 4 * frames)]
    if flags & PM_FLAG_2:
        s += [("f2_vec", 12 * frames), ("f2_int", 4 * frames)]
    if flags & PM_FLAG_4:
        s += [("f4_vec", 12 * frames), ("f4_int", 4 * frames)]
    s.append(("submodel_children", n * (n + 1)))
    s.append(("submodel_bytes", n))
    return s


class Egg:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if len(d) < 12:
            raise EggError("file too short")
        nb, ns, nm = struct.unpack_from("<3I", d, 0)
        self.header_size = 12 + 17 * nb + 24 * ns + MODEL_HDR_SIZE * nm
        if self.header_size > len(d):
            raise EggError("header tables exceed file size")
        base = self.header_size       # all data offsets are relative to this
        self.data_start = base

        self.bitmaps = []
        pos = 12
        for i in range(nb):
            name, dflags, w, h, flags, avg, off = BITMAP_HDR.unpack_from(d, pos)
            b = Bitmap()
            b.index = i
            b.name = cstr(name)
            b.dflags = dflags
            b.width = w + (256 if dflags & DBM_FLAG_LARGE else 0)
            b.height = h
            b.flags = flags
            b.avg_color = avg
            b.offset = off
            b.file_offset = base + off
            if flags & BM_RLE:
                if b.file_offset + 4 > len(d):
                    raise EggError("bitmap %d outside file" % i)
                b.size = struct.unpack_from("<I", d, b.file_offset)[0]
            else:
                b.size = b.width * b.height
            self.bitmaps.append(b)
            pos += 17

        self.sounds = []
        for i in range(ns):
            name, length, dlen, off, flags = SOUND_HDR.unpack_from(d, pos)
            s = Sound()
            s.index = i
            s.name = cstr(name)
            s.length = length
            s.data_length = dlen
            s.offset = off
            s.flags = flags
            s.file_offset = base + off
            self.sounds.append(s)
            pos += 24

        self.models = []
        for i in range(nm):
            h = d[pos:pos + MODEL_HDR_SIZE]
            m = Model()
            m.index = i
            m.header = h
            (m.n_models, m.model_data_size) = struct.unpack_from("<2i", h, 0)
            m.mins = struct.unpack_from("<3i", h, 0x08)
            m.maxs = struct.unpack_from("<3i", h, 0x14)
            (m.rad, m.n_textures, m.first_texture, m.unk_2c, m.unk_30,
             m.n_frames, m.offset) = struct.unpack_from("<7i", h, 0x20)
            m.flags = h[0x3C]
            m.name = cstr(h[0x3D:0x4A])
            m.file_offset = base + m.offset
            m.sections = model_sections(m.n_models, m.n_frames, m.flags,
                                        m.model_data_size)
            m.size = sum(sz for _, sz in m.sections)
            self.models.append(m)
            pos += MODEL_HDR_SIZE

    # ---- data access -------------------------------------------------

    def bitmap_pixels(self, b):
        """Return width*height palette indices, top row first."""
        d = self.data
        if not b.flags & BM_RLE:
            raw = d[b.file_offset:b.file_offset + b.size]
            if len(raw) != b.width * b.height:
                raise EggError("bitmap %s: short data" % b.full_name)
            return raw
        return rle_decode(d, b.file_offset, b.width, b.height, b.full_name)

    def sound_data(self, s):
        return self.data[s.file_offset:s.file_offset + s.data_length]

    def model_data(self, m):
        return self.data[m.file_offset:m.file_offset + m.size]

    # ---- coverage ----------------------------------------------------

    def ranges(self):
        """Return (start, end, label) for every explained file range."""
        r = [(0, 12, "counts"),
             (12, 12 + 17 * len(self.bitmaps), "bitmap headers")]
        p = r[-1][1]
        r.append((p, p + 24 * len(self.sounds), "sound headers"))
        p = r[-1][1]
        r.append((p, p + MODEL_HDR_SIZE * len(self.models), "model headers"))
        for b in self.bitmaps:
            r.append((b.file_offset, b.file_offset + b.size,
                      "bitmap %d %s" % (b.index, b.full_name)))
        for s in self.sounds:
            r.append((s.file_offset, s.file_offset + s.data_length,
                      "sound %d %s" % (s.index, s.name)))
        for m in self.models:
            r.append((m.file_offset, m.file_offset + m.size,
                      "model %d %s" % (m.index, m.name)))
        return r

    def coverage(self):
        """Return (gaps, overlaps, past_end). Zero length entries are skipped."""
        size = len(self.data)
        rs = sorted((a, b, l) for a, b, l in self.ranges() if b > a)
        gaps, overlaps, past = [], [], []
        pos = 0
        for a, b, label in rs:
            if b > size:
                past.append((a, b, label))
            if a > pos:
                gaps.append((pos, a))
            elif a < pos:
                overlaps.append((a, min(b, pos), label))
            pos = max(pos, b)
        if pos < size:
            gaps.append((pos, size))
        return gaps, overlaps, past


def rle_decode(d, start, width, height, name="?"):
    """Decode a Descent 1 RLE bitmap.

    Layout: u32 total size (includes these 4 bytes), `height` bytes with the
    encoded size of each row, then the rows. In a row, a byte with the top
    three bits set is a code: low five bits zero ends the row, otherwise the
    low five bits are a run length and the next byte is the color. Every
    other byte is one literal pixel.
    """
    total = struct.unpack_from("<I", d, start)[0]
    end = start + total
    if end > len(d):
        raise EggError("bitmap %s: RLE data outside file" % name)
    sizes = d[start + 4:start + 4 + height]
    pos = start + 4 + height
    out = bytearray(width * height)
    for y in range(height):
        row_end = pos + sizes[y]
        if row_end > end:
            raise EggError("bitmap %s: row %d outside RLE data" % (name, y))
        x = 0
        o = y * width
        p = pos
        while p < row_end:
            c = d[p]
            p += 1
            if c & 0xE0 == 0xE0:
                n = c & 0x1F
                if n == 0:
                    break
                if p >= row_end:
                    raise EggError("bitmap %s: row %d cut" % (name, y))
                v = d[p]
                p += 1
                if x + n > width:
                    raise EggError("bitmap %s: row %d too wide" % (name, y))
                out[o + x:o + x + n] = bytes([v]) * n
                x += n
            else:
                if x >= width:
                    raise EggError("bitmap %s: row %d too wide" % (name, y))
                out[o + x] = c
                x += 1
        if x != width:
            raise EggError("bitmap %s: row %d has %d of %d pixels"
                           % (name, y, x, width))
        pos = row_end
    if pos != end:
        raise EggError("bitmap %s: RLE size mismatch (%d bytes left)"
                       % (name, end - pos))
    return bytes(out)


# ---- palette and PNG -------------------------------------------------

def load_palette(path):
    """Read the first 768 bytes of PALETTE.256 (6 bit VGA) as 8 bit RGB."""
    with open(path, "rb") as f:
        raw = f.read(768)
    if len(raw) != 768:
        raise EggError("palette file too short")
    if max(raw) > 63:
        raise EggError("palette is not 6 bit VGA")
    return bytes((v << 2) | (v >> 4) for v in raw)


def gray_palette():
    return bytes(v for i in range(256) for v in (i, i, i))


def png_chunk(tag, payload):
    crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)


def write_png(path, width, height, pixels, palette, transparent=()):
    """Write an 8 bit paletted PNG. `transparent` lists fully clear indices."""
    rows = bytearray()
    for y in range(height):
        rows.append(0)
        rows += pixels[y * width:(y + 1) * width]
    out = [b"\x89PNG\r\n\x1a\n",
           png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 3, 0, 0, 0)),
           png_chunk(b"PLTE", palette)]
    if transparent:
        alpha = bytearray(b"\xff" * (max(transparent) + 1))
        for i in transparent:
            alpha[i] = 0
        out.append(png_chunk(b"tRNS", bytes(alpha)))
    out.append(png_chunk(b"IDAT", zlib.compress(bytes(rows), 9)))
    out.append(png_chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(b"".join(out))


def write_wav(path, samples, rate):
    """Wrap unsigned 8 bit mono samples in a WAV header."""
    hdr = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(samples), b"WAVE",
                      b"fmt ", 16, 1, 1, rate, rate, 1, 8, b"data", len(samples))
    with open(path, "wb") as f:
        f.write(hdr + samples)


# ---- .BIN table files ------------------------------------------------

def _rol(c):
    return ((c << 1) & 0xFF) | (c >> 7)


def decode_bin(raw):
    """Decode a .BIN table. Return (egg_name, text_bytes).

    Bytes 0..12 hold the EGG file name, NUL padded. The rest is the table
    text. Byte 0x0A is a line end and stays. Every other byte b decodes to
    rol8(rol8(b) ^ 0xD3), the same scheme as Descent 1 BITMAPS.BIN.
    """
    name = cstr(raw[:13])
    body = bytes(c if c == 0x0A else _rol(_rol(c) ^ 0xD3) for c in raw[13:])
    return name, body


# ---- commands --------------------------------------------------------

def flag_text(flags):
    names = [(BM_TRANSPARENT, "TRANS"), (BM_SUPER_TRANSPARENT, "SUPERTRANS"),
             (BM_NO_LIGHTING, "NOLIGHT"), (BM_RLE, "RLE"),
             (BM_UNKNOWN_10, "x10"), (0x20, "x20"), (BM_UNKNOWN_40, "x40"),
             (0x80, "x80")]
    return "|".join(n for bit, n in names if flags & bit) or "-"


def report_coverage(egg, out=sys.stdout):
    gaps, overlaps, past = egg.coverage()
    total = sum(b - a for a, b in gaps)
    out.write("coverage: file size %d, unexplained bytes %d in %d range(s), "
              "%d overlap(s), %d entry(ies) past end of file\n"
              % (len(egg.data), total, len(gaps), len(overlaps), len(past)))
    for a, b in gaps:
        out.write("  unexplained 0x%X..0x%X (%d bytes)\n" % (a, b, b - a))
    for a, b, label in overlaps:
        out.write("  overlap 0x%X..0x%X at %s\n" % (a, b, label))
    for a, b, label in past:
        out.write("  past end 0x%X..0x%X %s\n" % (a, b, label))
    return not (gaps or overlaps or past)


def cmd_list(args):
    egg = Egg(args.egg)
    w = sys.stdout.write
    w("%s: %d bitmaps, %d sounds, %d models, data starts at 0x%X\n"
      % (os.path.basename(args.egg), len(egg.bitmaps), len(egg.sounds),
         len(egg.models), egg.data_start))
    w("\n[bitmaps] index name frame width height flags avg_color offset size\n")
    for b in egg.bitmaps:
        w("%5d %-8s %3s %4d %4d %-22s %3d 0x%08X %6d\n"
          % (b.index, b.name, "-" if b.frame is None else b.frame, b.width,
             b.height, flag_text(b.flags), b.avg_color, b.file_offset, b.size))
    w("\n[sounds] index name length data_length flags offset\n")
    for s in egg.sounds:
        w("%5d %-8s %7d %7d %d 0x%08X\n"
          % (s.index, s.name, s.length, s.data_length, s.flags, s.file_offset))
    w("\n[models] index name submodels frames flags textures first_texture "
      "model_data_size offset size\n")
    for m in egg.models:
        w("%5d %-12s %3d %4d 0x%02X %3d %4d %7d 0x%08X %7d\n"
          % (m.index, m.name, m.n_models, m.n_frames, m.flags, m.n_textures,
             m.first_texture, m.model_data_size, m.file_offset, m.size))
    w("\n")
    report_coverage(egg)
    return 0


def cmd_check(args):
    ok = True
    for path in args.egg:
        egg = Egg(path)
        rle = raw = 0
        for b in egg.bitmaps:
            egg.bitmap_pixels(b)           # raises on bad data
            if b.flags & BM_RLE:
                rle += 1
            else:
                raw += 1
        print("%s: %d bitmaps decoded (%d RLE, %d raw), %d sounds, %d models"
              % (os.path.basename(path), len(egg.bitmaps), rle, raw,
                 len(egg.sounds), len(egg.models)))
        ok = report_coverage(egg) and ok
    return 0 if ok else 1


def safe_name(name):
    return "".join(c if c.isalnum() or c in "._-#" else "_" for c in name) or "_"


def cmd_extract(args):
    egg = Egg(args.egg)
    src_dir = os.path.dirname(os.path.abspath(args.egg))
    pal_path = args.palette
    if pal_path is None:
        for cand in ("PALETTE.256", "palette.256"):
            p = os.path.join(src_dir, cand)
            if os.path.isfile(p):
                pal_path = p
                break
    if pal_path:
        palette = load_palette(pal_path)
    else:
        sys.stderr.write("warning: no PALETTE.256 found, using a gray ramp\n")
        palette = gray_palette()

    out = args.outdir
    dirs = {k: os.path.join(out, k) for k in ("bitmaps", "sounds", "models")}
    for p in dirs.values():
        os.makedirs(p, exist_ok=True)

    index = []
    for b in egg.bitmaps:
        pix = egg.bitmap_pixels(b)
        trans = []
        if b.flags & BM_TRANSPARENT:
            trans.append(255)
        if b.flags & BM_SUPER_TRANSPARENT:
            trans.append(254)
        fn = "%04d_%s.png" % (b.index, safe_name(b.full_name))
        write_png(os.path.join(dirs["bitmaps"], fn), b.width, b.height, pix,
                  palette, trans)
        index.append("bitmap\t%d\t%s\t%dx%d\t%s\tbitmaps/%s"
                     % (b.index, b.full_name, b.width, b.height,
                        flag_text(b.flags), fn))
    for s in egg.sounds:
        data = egg.sound_data(s)
        base = "%04d_%s" % (s.index, safe_name(s.name))
        with open(os.path.join(dirs["sounds"], base + ".raw"), "wb") as f:
            f.write(data)
        if args.wav:
            write_wav(os.path.join(dirs["sounds"], base + ".wav"), data, args.wav)
        index.append("sound\t%d\t%s\t%d\tflags=%d\tsounds/%s.raw"
                     % (s.index, s.name, s.data_length, s.flags, base))
    for m in egg.models:
        base = "%04d_%s" % (m.index, safe_name(m.name))
        with open(os.path.join(dirs["models"], base + ".hdr"), "wb") as f:
            f.write(m.header)
        with open(os.path.join(dirs["models"], base + ".dat"), "wb") as f:
            f.write(egg.model_data(m))
        pos = 0
        parts = []
        for name, size in m.sections:
            parts.append("%s@%d+%d" % (name, pos, size))
            pos += size
        index.append("model\t%d\t%s\tsubmodels=%d frames=%d flags=0x%02X "
                     "textures=%d first_texture=%d\tmodels/%s.dat\t%s"
                     % (m.index, m.name, m.n_models, m.n_frames, m.flags,
                        m.n_textures, m.first_texture, base, " ".join(parts)))
    with open(os.path.join(out, "index.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(index) + "\n")
    print("%s: wrote %d bitmaps, %d sounds, %d models to %s"
          % (os.path.basename(args.egg), len(egg.bitmaps), len(egg.sounds),
             len(egg.models), out))
    report_coverage(egg)
    return 0


def cmd_bin(args):
    with open(args.bin, "rb") as f:
        raw = f.read()
    name, text = decode_bin(raw)
    if args.out:
        with open(args.out, "wb") as f:
            f.write(text)
        print("%s: EGG name '%s', %d bytes of table text written to %s"
              % (os.path.basename(args.bin), name, len(text), args.out))
    else:
        print("; EGG name: %s" % name)
        sys.stdout.write(text.decode("latin-1"))
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Descent to Undermountain EGG tool")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("list", help="list every entry")
    p.add_argument("egg")
    p.set_defaults(func=cmd_list)

    p = sub.add_parser("check", help="decode all bitmaps and report unexplained bytes")
    p.add_argument("egg", nargs="+")
    p.set_defaults(func=cmd_check)

    p = sub.add_parser("extract", help="extract bitmaps to PNG, the rest to raw files")
    p.add_argument("egg")
    p.add_argument("outdir")
    p.add_argument("--palette", help="PALETTE.256 (default: next to the EGG file)")
    p.add_argument("--wav", type=int, metavar="RATE",
                   help="also write WAV files, unsigned 8 bit mono at RATE Hz")
    p.set_defaults(func=cmd_extract)

    p = sub.add_parser("bin", help="decode a .BIN table file to text")
    p.add_argument("bin")
    p.add_argument("out", nargs="?")
    p.set_defaults(func=cmd_bin)

    args = ap.parse_args(argv)
    try:
        return args.func(args)
    except EggError as e:
        sys.stderr.write("error: %s\n" % e)
        return 2


if __name__ == "__main__":
    sys.exit(main())
