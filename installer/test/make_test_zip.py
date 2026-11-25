#!/usr/bin/env python3
"""Make installer/test/test_zip_data.h, the ZIP archive of test_inst.c.

    python3 installer/test/make_test_zip.py

The archive has the entries that the reader of inst.c must decode:
  text.txt    deflate with dynamic codes, more than the 32 KB window
  short.txt   deflate with the fixed codes (a short text)
  noise.bin   deflate with stored blocks (level 0)
  plain.bin   stored (method 0)
  sub/x.cue   a cue sheet
The ZIP64 records are written for all of them (ZIP64_LIMIT is made small),
so the ZIP64 end record and the ZIP64 extra fields are tested too.
test_inst.c makes the same contents and compares them.
"""
import io
import os
import zipfile


def text():
    # The same as make_text() of test_inst.c
    words = ['fire', 'water', 'stone', 'the', 'of', 'dungeon', 'dark']
    return ''.join(words[i * 7919 % 10007 % 7] + ' ' for i in range(15000)).encode()


def noise(n):
    # The same as make_noise() of test_inst.c (an LCG)
    x = 12345
    out = bytearray()
    for _ in range(n):
        x = (x * 1103515245 + 12345) & 0xffffffff
        out.append((x >> 16) & 255)
    return bytes(out)


CUE = b'FILE "..\\DISC.BIN" BINARY\r\n  TRACK 01 MODE1/2352\r\n    INDEX 01 00:00:00\r\n'

buf = io.BytesIO()
old = zipfile.ZIP64_LIMIT
zipfile.ZIP64_LIMIT = 16
with zipfile.ZipFile(buf, 'w') as z:
    z.writestr(zipfile.ZipInfo('text.txt'), text(), zipfile.ZIP_DEFLATED, 9)
    z.writestr(zipfile.ZipInfo('short.txt'), b'hello hello hello hello', zipfile.ZIP_DEFLATED, 6)
    z.writestr(zipfile.ZipInfo('noise.bin'), noise(5000), zipfile.ZIP_DEFLATED, 0)
    z.writestr(zipfile.ZipInfo('plain.bin'), noise(1000), zipfile.ZIP_STORED)
    z.writestr(zipfile.ZipInfo('sub/x.cue'), CUE, zipfile.ZIP_DEFLATED, 6)
zipfile.ZIP64_LIMIT = old
data = buf.getvalue()
assert b'PK\x06\x06' in data[-200:]

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'test_zip_data.h')
with open(out, 'w', newline='\n') as f:
    f.write('// Made by make_test_zip.py. Do not edit.\n')
    f.write('static const unsigned char Test_zip[%d] = {\n' % len(data))
    for i in range(0, len(data), 16):
        f.write('\t' + ''.join('%d,' % b for b in data[i:i + 16]) + '\n')
    f.write('};\n')
print(out, len(data))
