"""unpack.py: unpack the compressed files of the Descent to Undermountain disc.

The disc stores three file types in compressed form. The installer unpacks
them and changes the extension:

    NAME.DOG -> NAME.LVL   level
    NAME.COW -> NAME.EGG   bitmap, sound and model archive
    NAME.HEN -> NAME.BIN   encoded BITMAPS.TBL text

See docs/formats/packed.md for the layout.

Usage:
    python -I unpack.py FILE [FILE ...] --out OUTDIR
    python -I unpack.py --all DISC_DTU_DIR --out OUTDIR

Python 3 standard library only.
"""
import os
import sys

EXT = {'.DOG': '.LVL', '.COW': '.EGG', '.HEN': '.BIN'}


class FormatError(Exception):
    pass


def unpack(src):
    """Return the unpacked bytes of one DOG, COW or HEN file."""
    out = bytearray()
    pos = 2
    end = len(src)
    while pos < end:
        flags = src[pos]
        pos += 1
        for bit in range(8):
            if pos >= end:
                break
            if (flags >> bit) & 1:
                out.append(src[pos])
                pos += 1
                continue
            if pos + 1 >= end:
                raise FormatError('match cut off at the end of the file')
            word = src[pos] | (src[pos + 1] << 8)
            pos += 2
            dist = word >> 4
            length = (word & 15) + 3
            if dist == 0 or dist > len(out):
                raise FormatError('bad match distance %d at input offset %d' % (dist, pos - 2))
            if dist >= length:
                out += out[-dist:len(out) - dist + length]
            else:
                for _ in range(length):
                    out.append(out[-dist])
    size16 = src[0] | (src[1] << 8)
    if size16 != len(out) & 0xFFFF:
        raise FormatError('header %04x does not match unpacked size %d' % (size16, len(out)))
    return bytes(out)


def main(argv):
    args = argv[1:]
    if '--out' not in args:
        print(__doc__)
        return 1
    i = args.index('--out')
    outdir = args[i + 1]
    del args[i:i + 2]
    if args and args[0] == '--all':
        base = args[1]
        files = [os.path.join(base, f) for f in sorted(os.listdir(base))
                 if os.path.splitext(f)[1].upper() in EXT]
    else:
        files = args
    if not files:
        print(__doc__)
        return 1
    os.makedirs(outdir, exist_ok=True)
    failed = 0
    for path in files:
        stem, ext = os.path.splitext(os.path.basename(path))
        new_ext = EXT.get(ext.upper())
        if new_ext is None:
            print('%s: unknown extension' % path)
            failed += 1
            continue
        with open(path, 'rb') as f:
            src = f.read()
        try:
            data = unpack(src)
        except FormatError as e:
            print('%s: %s' % (path, e))
            failed += 1
            continue
        with open(os.path.join(outdir, stem.upper() + new_ext), 'wb') as f:
            f.write(data)
    print('unpacked %d files to %s, %d failed' % (len(files) - failed, outdir, failed))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
