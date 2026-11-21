#!/usr/bin/env python3
"""tun.py: read Descent to Undermountain .TUN music files.

A .TUN file is one Interplay ACM audio stream (signature 97 28 03 01).
The same stream format is stored inside WERDZ.DAT and WHAMMO.DAT.
See docs/formats/tun.md.

Usage:
    python -I tun.py info FILE [FILE ...]     print the header of each file
    python -I tun.py wav FILE [OUT.wav]       decode one file to 16-bit PCM WAV
    python -I tun.py check FILE [FILE ...]    decode, print consumption and statistics

Python 3 standard library only. Decoding in pure Python takes a few
seconds per megabyte of input.
"""

import array
import struct
import sys

ACM_SIGNATURE = 0x01032897
ACM_HEADER_SIZE = 14


class AcmError(Exception):
    pass


def parse_header(data, offset=0):
    """Return the 14-byte ACM header as a dict."""
    if len(data) - offset < ACM_HEADER_SIZE:
        raise AcmError("file too short for an ACM header")
    sig, total, channels, rate, packed = struct.unpack_from("<IIHHH", data, offset)
    if sig != ACM_SIGNATURE:
        raise AcmError("bad signature 0x%08x, expected 0x%08x" % (sig, ACM_SIGNATURE))
    level = packed & 15
    rows = packed >> 4
    return {
        "signature": sig,
        "total_values": total,
        "channels": channels,
        "rate": rate,
        "packed": packed,
        "level": level,
        "rows": rows,
        "cols": 1 << level,
        "block_len": rows << level,
    }


class _Decoder:
    def __init__(self, data, offset, end, info):
        self.d = data
        self.p = offset + ACM_HEADER_SIZE
        self.end = end
        self.acc = 0
        self.n = 0
        self.overrun_bits = 0
        self.level = info["level"]
        self.rows = info["rows"]
        self.cols = info["cols"]
        self.block = [0] * info["block_len"]
        # Amplitude table. Index 0x8000 is the middle (value 0).
        self.amp = [0] * 0x10000
        self.wraps = None
        self.stale_reads = 0
        self.count = 0

    def bits(self, n):
        while self.n < n:
            if self.p < self.end:
                self.acc |= self.d[self.p] << self.n
                self.p += 1
            else:
                self.overrun_bits += 8
            self.n += 8
        v = self.acc & ((1 << n) - 1)
        self.acc >>= n
        self.n -= n
        return v

    def bit_position(self):
        """Number of bits consumed after the header."""
        return self.p * 8 - self.n

    # Column fillers. Each one writes one value per row into column col.

    def f_zero(self, ind, col):
        blk = self.block
        for pos in range(col, len(blk), self.cols):
            blk[pos] = 0

    def f_bad(self, ind, col):
        raise AcmError("invalid filler index %d" % ind)

    def f_linear(self, ind, col):
        blk = self.block
        amp = self.amp
        bits = self.bits
        base = 0x8000 - (1 << (ind - 1))
        lim = 1 << (ind - 1)
        if lim > self.count:
            # Possible index outside the part of the table written for this block.
            cnt = self.count
            for pos in range(col, len(blk), self.cols):
                v = bits(ind)
                m = v - lim
                if m >= cnt or m < -cnt:
                    self.stale_reads += 1
                blk[pos] = amp[base + v]
        else:
            for pos in range(col, len(blk), self.cols):
                blk[pos] = amp[base + bits(ind)]

    def _k(self, col, pair_zero, table):
        # Shared body of the k13/k12/k24/k23/k45/k44 fillers.
        blk = self.block
        amp = self.amp
        bits = self.bits
        cols = self.cols
        rows = self.rows
        nbits = 1 if len(table) == 2 else (2 if len(table) == 4 else 3)
        i = 0
        while i < rows:
            if pair_zero:
                if bits(1) == 0:
                    blk[i * cols + col] = 0
                    i += 1
                    if i >= rows:
                        break
                    blk[i * cols + col] = 0
                    i += 1
                    continue
            if bits(1) == 0:
                blk[i * cols + col] = 0
                i += 1
                continue
            blk[i * cols + col] = amp[0x8000 + table[bits(nbits)]]
            i += 1

    _MAP1 = (-1, 1)
    _MAP2N = (-2, -1, 1, 2)
    _MAP2F = (-3, -2, 2, 3)
    _MAP3 = (-4, -3, -2, -1, 1, 2, 3, 4)

    def f_k13(self, ind, col):
        self._k(col, True, self._MAP1)

    def f_k12(self, ind, col):
        self._k(col, False, self._MAP1)

    def f_k24(self, ind, col):
        self._k(col, True, self._MAP2N)

    def f_k23(self, ind, col):
        self._k(col, False, self._MAP2N)

    def f_k45(self, ind, col):
        self._k(col, True, self._MAP3)

    def f_k44(self, ind, col):
        self._k(col, False, self._MAP3)

    def _k3(self, col, pair_zero):
        blk = self.block
        amp = self.amp
        bits = self.bits
        cols = self.cols
        rows = self.rows
        i = 0
        while i < rows:
            if pair_zero:
                if bits(1) == 0:
                    blk[i * cols + col] = 0
                    i += 1
                    if i >= rows:
                        break
                    blk[i * cols + col] = 0
                    i += 1
                    continue
            if bits(1) == 0:
                blk[i * cols + col] = 0
                i += 1
                continue
            if bits(1) == 0:
                blk[i * cols + col] = amp[0x8000 + self._MAP1[bits(1)]]
                i += 1
                continue
            blk[i * cols + col] = amp[0x8000 + self._MAP2F[bits(2)]]
            i += 1

    def f_k35(self, ind, col):
        self._k3(col, True)

    def f_k34(self, ind, col):
        self._k3(col, False)

    def _t(self, col, nbits, base, per, mid):
        blk = self.block
        amp = self.amp
        bits = self.bits
        cols = self.cols
        rows = self.rows
        i = 0
        while i < rows:
            b = bits(nbits)
            for _ in range(per):
                blk[i * cols + col] = amp[0x8000 + (b % base) - mid]
                b //= base
                i += 1
                if i >= rows:
                    break

    def f_t15(self, ind, col):
        self._t(col, 5, 3, 3, 1)

    def f_t27(self, ind, col):
        self._t(col, 7, 5, 3, 2)

    def f_t37(self, ind, col):
        self._t(col, 7, 11, 2, 5)

    def load_block(self):
        bits = self.bits
        pwr = bits(4)
        val = bits(16)
        count = 1 << pwr
        self.count = count
        amp = self.amp
        if val:
            amp[0x8000:0x8000 + count] = range(0, count * val, val)
            amp[0x8000 - count:0x8000] = range(-count * val, 0, val)
        else:
            amp[0x8000 - count:0x8000 + count] = [0] * (2 * count)
        fillers = self._FILLERS
        for col in range(self.cols):
            ind = bits(5)
            fillers[ind](self, ind, col)

    def juggle_block(self):
        level = self.level
        if level == 0:
            return
        cols = self.cols
        rows = self.rows
        if level > 9:
            step = 1
        else:
            step = (2048 >> level) - 2
        if self.wraps is None:
            # One pair of history rows per pass. Pass k works on rows of length cols >> (k + 1).
            self.wraps = [[0] * (2 * (cols >> (k + 1))) for k in range(level)]
        blk = self.block
        todo = rows
        start = 0
        while True:
            cnt = min(step, todo)
            seg_len = cnt * cols
            seg = blk[start:start + seg_len]
            L = cols
            for k in range(level):
                L >>= 1
                E = self.wraps[k] + seg
                self.wraps[k] = seg[-2 * L:]
                m = E[L:-L]
                u = [x + z for x, z in zip(E, E[2 * L:])]
                two_l = 2 * L
                if L == 1:
                    seg[0::2] = [2 * y + s for y, s in zip(m[0::2], u[0::2])]
                    seg[1::2] = [2 * y - s for y, s in zip(m[1::2], u[1::2])]
                else:
                    for i in range(L):
                        seg[i::two_l] = [2 * y + s for y, s in zip(m[i::two_l], u[i::two_l])]
                        j = i + L
                        seg[j::two_l] = [2 * y - s for y, s in zip(m[j::two_l], u[j::two_l])]
                if k == 0:
                    for pos in range(0, seg_len, L):
                        seg[pos] += 1
            blk[start:start + seg_len] = seg
            if todo <= step:
                break
            todo -= step
            start += step * cols


_Decoder._FILLERS = (
    _Decoder.f_zero, _Decoder.f_bad, _Decoder.f_bad, _Decoder.f_linear,
    _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear,
    _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear,
    _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear, _Decoder.f_linear,
    _Decoder.f_linear, _Decoder.f_k13, _Decoder.f_k12, _Decoder.f_t15,
    _Decoder.f_k24, _Decoder.f_k23, _Decoder.f_t27, _Decoder.f_k35,
    _Decoder.f_k34, _Decoder.f_bad, _Decoder.f_k45, _Decoder.f_k44,
    _Decoder.f_bad, _Decoder.f_t37, _Decoder.f_bad, _Decoder.f_bad,
)


def decode_acm(data, offset=0, length=None, progress=None):
    """Decode one ACM stream.

    Returns (info, samples, report). samples is an array('h') of interleaved
    signed 16-bit values with exactly info['total_values'] items. report has
    'bytes_used' (header plus bitstream, rounded up to whole bytes),
    'bytes_total', 'clipped' (values that needed saturation), 'stale_reads'
    and 'overrun_bits' (bits read past the end, 0 for a complete stream).
    """
    end = len(data) if length is None else offset + length
    info = parse_header(data, offset)
    if info["block_len"] == 0:
        raise AcmError("block length is zero")
    dec = _Decoder(data, offset, end, info)
    total = info["total_values"]
    level = info["level"]
    out = array.array("h")
    clipped = 0
    blocks = 0
    while len(out) < total:
        dec.load_block()
        dec.juggle_block()
        vals = [v >> level for v in dec.block]
        if min(vals) < -32768 or max(vals) > 32767:
            clipped += sum(1 for v in vals if v < -32768 or v > 32767)
            vals = [-32768 if v < -32768 else (32767 if v > 32767 else v) for v in vals]
        out.extend(vals)
        blocks += 1
        if progress and blocks % 64 == 0:
            progress(len(out), total)
    del out[total:]
    bits_used = dec.bit_position() - offset * 8
    report = {
        "blocks": blocks,
        "bytes_used": (bits_used + 7) // 8,
        "bytes_total": end - offset,
        "clipped": clipped,
        "stale_reads": dec.stale_reads,
        "overrun_bits": dec.overrun_bits,
    }
    return info, out, report


def wav_bytes(samples, channels, rate):
    """Build a RIFF WAVE file image from an array('h')."""
    if sys.byteorder != "little":
        samples = array.array("h", samples)
        samples.byteswap()
    pcm = samples.tobytes()
    hdr = struct.pack("<4sI4s4sIHHIIHH4sI", b"RIFF", 36 + len(pcm), b"WAVE", b"fmt ", 16,
                      1, channels, rate, rate * channels * 2, channels * 2, 16, b"data", len(pcm))
    return hdr + pcm


def stats(samples, channels):
    """Per channel statistics used to sanity check a decode."""
    res = []
    for c in range(channels):
        ch = samples[c::channels]
        n = len(ch)
        if n == 0:
            res.append({"n": 0})
            continue
        mean = sum(ch) / n
        rms = (sum(v * v for v in ch) / n) ** 0.5
        full = sum(1 for v in ch if v == 32767 or v == -32768)
        maxstep = max((abs(b - a) for a, b in zip(ch, ch[1:])), default=0)
        # Largest absolute mean over windows of 4096 samples: detects DC runaway.
        worst_dc = 0
        for i in range(0, n, 4096):
            w = ch[i:i + 4096]
            worst_dc = max(worst_dc, abs(sum(w) / len(w)))
        # RMS of the first difference over RMS. White noise gives about 1.41,
        # band-limited audio gives much less.
        drms = (sum((b - a) * (b - a) for a, b in zip(ch, ch[1:])) / max(1, n - 1)) ** 0.5
        res.append({"n": n, "min": min(ch), "max": max(ch), "mean": mean, "rms": rms,
                    "full_scale": full, "max_step": maxstep, "worst_window_dc": worst_dc,
                    "diff_ratio": drms / rms if rms else 0.0})
    return res


def print_header(path, data):
    info = parse_header(data)
    frames = info["total_values"] / max(1, info["channels"])
    print(path)
    print("  size          %d bytes" % len(data))
    print("  signature     0x%08x" % info["signature"])
    print("  total values  %d (%.0f frames, %.2f s)" % (info["total_values"], frames, frames / max(1, info["rate"])))
    print("  channels      %d" % info["channels"])
    print("  rate          %d Hz" % info["rate"])
    print("  packed        0x%04x: level %d (%d columns), %d rows, %d values per block"
          % (info["packed"], info["level"], info["cols"], info["rows"], info["block_len"]))


def main(argv):
    if len(argv) < 3 or argv[1] not in ("info", "wav", "check"):
        print(__doc__)
        return 2
    cmd = argv[1]
    if cmd == "info":
        for path in argv[2:]:
            with open(path, "rb") as f:
                print_header(path, f.read())
        return 0
    if cmd == "wav":
        path = argv[2]
        out = argv[3] if len(argv) > 3 else path.rsplit(".", 1)[0] + ".wav"
        with open(path, "rb") as f:
            data = f.read()
        print_header(path, data)
        info, samples, rep = decode_acm(data)
        with open(out, "wb") as f:
            f.write(wav_bytes(samples, info["channels"], info["rate"]))
        print("  decoded       %d values in %d blocks, used %d of %d bytes, %d clipped"
              % (len(samples), rep["blocks"], rep["bytes_used"], rep["bytes_total"], rep["clipped"]))
        print("  wrote         %s" % out)
        return 0
    for path in argv[2:]:
        with open(path, "rb") as f:
            data = f.read()
        info, samples, rep = decode_acm(data)
        print("%s: values %d/%d, bytes used %d/%d, overrun bits %d, clipped %d, stale table reads %d"
              % (path, len(samples), info["total_values"], rep["bytes_used"], rep["bytes_total"],
                 rep["overrun_bits"], rep["clipped"], rep["stale_reads"]))
        for c, s in enumerate(stats(samples, info["channels"])):
            if s["n"]:
                print("  ch%d: min %d max %d mean %.1f rms %.1f full-scale %d max-step %d "
                      "worst-window-dc %.1f diff-ratio %.3f"
                      % (c, s["min"], s["max"], s["mean"], s["rms"], s["full_scale"], s["max_step"],
                         s["worst_window_dc"], s["diff_ratio"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
