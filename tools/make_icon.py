"""make_icon.py: draw the ASCENT program icon (installer/res/ascent.ico).

The icon is original art: a gold mountain peak with an upward chevron on a
dark square with round corners. It uses no game art.

Usage:
    python -I tools/make_icon.py [OUT.ico]

Python 3 standard library only. The ICO file holds PNG images of 16, 24,
32, 48, 64 and 256 pixels.
"""
import os
import struct
import sys
import zlib

BG = (29, 36, 51)
BG_EDGE = (58, 70, 96)
GOLD = (217, 164, 65)
GOLD_DARK = (150, 104, 32)
SNOW = (236, 232, 220)


def inside_poly(x, y, pts):
    n = len(pts)
    c = False
    j = n - 1
    for i in range(n):
        xi, yi = pts[i]
        xj, yj = pts[j]
        if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
            c = not c
        j = i
    return c


def shade(x, y):
    """The color at (x, y) in a 1 by 1 square, or None outside."""
    r = 0.18
    cx = min(max(x, r), 1 - r)
    cy = min(max(y, r), 1 - r)
    d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
    if d > r:
        return None
    col = BG_EDGE if d > r - 0.035 else BG
    # chevron above the peak
    chev_out = [(0.5, 0.10), (0.72, 0.30), (0.64, 0.30), (0.5, 0.18), (0.36, 0.30), (0.28, 0.30)]
    if inside_poly(x, y, chev_out):
        return GOLD
    # the mountain: a large peak and a small peak at the right
    big = [(0.14, 0.84), (0.50, 0.34), (0.86, 0.84)]
    small = [(0.52, 0.84), (0.72, 0.56), (0.92, 0.84)]
    snow = [(0.40, 0.48), (0.50, 0.34), (0.60, 0.48), (0.54, 0.45), (0.50, 0.50), (0.45, 0.45)]
    if inside_poly(x, y, snow):
        return SNOW
    if inside_poly(x, y, big):
        return GOLD if x < 0.5 + (y - 0.34) * 0.15 else GOLD_DARK
    if inside_poly(x, y, small):
        return GOLD_DARK
    return col


def render(size, ss=4):
    rows = []
    for py in range(size):
        row = bytearray()
        for px in range(size):
            acc = [0, 0, 0, 0]
            for sy in range(ss):
                for sx in range(ss):
                    c = shade((px + (sx + 0.5) / ss) / size, (py + (sy + 0.5) / ss) / size)
                    if c is not None:
                        acc[0] += c[0]
                        acc[1] += c[1]
                        acc[2] += c[2]
                        acc[3] += 255
            n = ss * ss
            a = acc[3] // n
            if a:
                cov = acc[3] / 255
                row += bytes((round(acc[0] / cov), round(acc[1] / cov), round(acc[2] / cov), a))
            else:
                row += b'\0\0\0\0'
        rows.append(bytes(row))
    return rows


def png(size, rows):
    raw = b''.join(b'\0' + r for r in rows)

    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)

    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def main(argv):
    out = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(__file__), '..', 'installer', 'res', 'ascent.ico')
    sizes = [16, 24, 32, 48, 64, 256]
    images = [png(s, render(s, 8 if s <= 64 else 3)) for s in sizes]
    head = struct.pack('<HHH', 0, 1, len(sizes))
    offset = 6 + 16 * len(sizes)
    dirs = b''
    for s, data in zip(sizes, images):
        dirs += struct.pack('<BBBBHHII', s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, 'wb') as f:
        f.write(head + dirs + b''.join(images))
    print('wrote', out)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
