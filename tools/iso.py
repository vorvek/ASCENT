"""iso.py: list and extract files from an ISO 9660 image with 2048-byte sectors.

The Descent to Undermountain disc image is a single MODE1/2048 track, so the
.bin file of a bin/cue pair is a plain ISO 9660 image.

Usage:
    python -I iso.py list IMAGE
    python -I iso.py extract IMAGE OUTDIR [GLOB ...]

Python 3 standard library only. File names lose the ";1" version suffix.
"""
import fnmatch
import os
import struct
import sys

SECTOR = 2048


def read_dir(f, lba, size, prefix, out):
    f.seek(lba * SECTOR)
    data = f.read(size)
    pos = 0
    while pos < len(data):
        rec_len = data[pos]
        if rec_len == 0:
            # Records do not cross sectors. Skip to the next sector.
            pos = (pos // SECTOR + 1) * SECTOR
            continue
        ext_lba, ext_size = struct.unpack_from('<I4xI', data, pos + 2)
        flags = data[pos + 25]
        name_len = data[pos + 32]
        name = data[pos + 33:pos + 33 + name_len]
        pos += rec_len
        if name in (b'\x00', b'\x01'):
            continue
        name = name.decode('latin-1').split(';')[0].rstrip('.')
        path = prefix + name
        if flags & 2:
            read_dir(f, ext_lba, ext_size, path + '/', out)
        else:
            out.append((path, ext_lba, ext_size))


def read_image(f):
    f.seek(16 * SECTOR)
    pvd = f.read(SECTOR)
    if pvd[1:6] != b'CD001':
        raise SystemExit('not an ISO 9660 image with 2048-byte sectors')
    root_lba, root_size = struct.unpack_from('<I4xI', pvd, 156 + 2)
    out = []
    read_dir(f, root_lba, root_size, '', out)
    return pvd[40:72].decode('latin-1').strip(), out


def main(argv):
    if len(argv) < 3 or argv[1] not in ('list', 'extract'):
        print(__doc__)
        return 1
    with open(argv[2], 'rb') as f:
        label, files = read_image(f)
        if argv[1] == 'list':
            print('volume label: %s' % label)
            for path, lba, size in sorted(files):
                print('%10d  %s' % (size, path))
            print('%d files, %d bytes' % (len(files), sum(x[2] for x in files)))
            return 0
        if len(argv) < 4:
            print(__doc__)
            return 1
        outdir = argv[3]
        globs = [g.upper() for g in argv[4:]]
        count = 0
        for path, lba, size in files:
            if globs and not any(fnmatch.fnmatch(path.upper(), g) for g in globs):
                continue
            dest = os.path.join(outdir, *path.split('/'))
            os.makedirs(os.path.dirname(dest) or '.', exist_ok=True)
            f.seek(lba * SECTOR)
            with open(dest, 'wb') as o:
                left = size
                while left:
                    chunk = f.read(min(left, 1 << 20))
                    if not chunk:
                        raise SystemExit('image ends inside %s' % path)
                    o.write(chunk)
                    left -= len(chunk)
            count += 1
        print('extracted %d files to %s' % (count, outdir))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
