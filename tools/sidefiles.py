#!/usr/bin/env python3
"""Dump the small per-level side files of Descent to Undermountain as text.

Formats are documented in docs/formats/{brn,tig,tap,say,misc}.md.
Standard library only. Run with: python -I tools/sidefiles.py <cmd> <file>

Subcommands:
  brn FILE [--matrix]   segment visibility bit matrix
  tig FILE              triggers
  tap FILE              traps
  say FILE              level message texts
  store FILE            THESTORE.n shop inventory
  fnt FILE              PSFN font header and glyph widths
  check DIR             parse every BRN/TIG/TAP/SAY/FNT/THESTORE file in DIR
                        and report unexplained bytes or broken invariants
"""
import glob
import os
import struct
import sys

MAX_SEGMENTS = 900       # size of the per-segment table in .TIG
BRN_SEGMENTS = 800       # rows and columns of the .BRN bit matrix
BRN_ROW_BYTES = 100
TRIGGER_SIZE = 89
TRAP_SIZE = 95


class FormatError(Exception):
    pass


def fix(v):
    """16.16 fixed point to float."""
    return v / 65536.0


def vec(b, o):
    x, y, z = struct.unpack_from('<3i', b, o)
    return '(%.3f, %.3f, %.3f)' % (fix(x), fix(y), fix(z))


# --------------------------------------------------------------------- BRN

def parse_brn(b):
    if len(b) != BRN_SEGMENTS * BRN_ROW_BYTES:
        raise FormatError('BRN size %d, expected 80000' % len(b))
    return [int.from_bytes(b[r * BRN_ROW_BYTES:(r + 1) * BRN_ROW_BYTES], 'little')
            for r in range(BRN_SEGMENTS)]


def brn_can_see(b, seg_a, seg_b):
    return (b[seg_a * BRN_ROW_BYTES + seg_b // 8] >> (seg_b % 8)) & 1


def dump_brn(b, matrix=False):
    rows = parse_brn(b)
    used = [r for r in range(BRN_SEGMENTS) if rows[r]]
    n = (max(used) + 1) if used else 0
    ones = sum(bin(r).count('1') for r in rows)
    print('rows with any bit set: %d (highest %d)' % (len(used), n - 1))
    print('highest column bit set: %d' % (max(r.bit_length() for r in rows) - 1))
    print('bits set: %d' % ones)
    print('diagonal bits set: %d' % sum((rows[r] >> r) & 1 for r in range(BRN_SEGMENTS)))
    print('asymmetric pairs: %d' % sum(
        1 for a in range(n) for c in range(a) if ((rows[a] >> c) & 1) != ((rows[c] >> a) & 1)))
    for r in range(BRN_SEGMENTS):
        vis = [c for c in range(BRN_SEGMENTS) if (rows[r] >> c) & 1]
        if matrix:
            print(''.join('#' if (rows[r] >> c) & 1 else '.' for c in range(BRN_SEGMENTS)))
        else:
            print('seg %3d sees %3d: %s' % (r, len(vis), ranges(vis)))


def ranges(vals):
    out = []
    i = 0
    while i < len(vals):
        j = i
        while j + 1 < len(vals) and vals[j + 1] == vals[j] + 1:
            j += 1
        out.append('%d' % vals[i] if i == j else '%d-%d' % (vals[i], vals[j]))
        i = j + 1
    return ' '.join(out)


# --------------------------------------------------------------------- TIG

# (offset, struct code, name). Names ending in '?' are guesses, 'unk' unknown.
TRIGGER_FIELDS = [
    (0x00, 'h', 'unk00'),
    (0x02, 'V', 'pos'),
    (0x0e, 'B', 'on'),
    (0x0f, 'I', 'unk0f'),
    (0x13, 'H', 'flags'),
    (0x15, 'H', 'activation_mask'),
    (0x17, 'h', 'unk17'),
    (0x19, 'h', 'need_item_class'),
    (0x1b, 'h', 'need_item_id'),
    (0x1d, 'h', 'unk1d'),
    (0x1f, 'h', 'lack_item_class'),
    (0x21, 'h', 'lack_item_id'),
    (0x23, 'B', 'take_item'),
    (0x24, 'H', 'player_mask_a?'),
    (0x26, 'H', 'player_mask_b?'),
    (0x28, 'h', 'sound'),
    (0x2a, 'h', 'tex_on'),
    (0x2c, 'h', 'tex_off'),
    (0x2e, 'h', 'fire_count'),
    (0x30, 'I', 'unk30'),
    (0x34, 'B', 'skip_count'),
    (0x35, 'h', 'cond35_unused'),
    (0x37, 'h', 'cond37_unused'),
    (0x39, 'h', 'cond39_unused'),
    (0x3b, 'h', 'first_trap'),
    (0x3d, 'h', 'enable_trigger'),
    (0x3f, 'h', 'enable_trap'),
    (0x41, 'h', 'disable_trigger'),
    (0x43, 'h', 'disable_trap'),
    (0x45, 'B', 'object_num?'),
    (0x46, 'h', 'seg'),
    (0x48, 'b', 'side'),
    (0x49, 'h', 'next_in_seg'),
    (0x4b, 'h', 'say_index'),
    (0x4d, '12s', 'zero4d'),
]

TRAP_FIELDS = [
    (0x00, 'h', 'id'),
    (0x02, 'B', 'on'),
    (0x03, 'h', 'unk03'),
    (0x05, 'h', 'charges_init'),
    (0x07, 'h', 'charges'),
    (0x09, 'h', 'unk09'),
    (0x0b, 'h', 'sound'),
    (0x0d, 'h', 'next_trap'),
    (0x0f, 'I', 'flags'),
    (0x13, 'h', 'weapon_type?'),
    (0x15, 'V', 'pos'),
    (0x21, 'V', 'vec21'),
    (0x2d, 'h', 'spawn_object?'),
    (0x2f, 'h', 'unk2f'),
    (0x31, 'h', 'stat_delta'),
    (0x33, 'h', 'robot_type'),
    (0x35, 'h', 'robot_arg'),
    (0x37, 'h', 'tex_disabled'),
    (0x39, 'h', 'tex_enabled'),
    (0x3b, 'h', 'unk3b'),
    (0x3d, 'h', 'unk3d'),
    (0x3f, 'h', 'light_delta'),
    (0x41, 'H', 'delay_init'),
    (0x43, 'h', 'seg'),
    (0x45, 'h', 'side'),
    (0x47, 'H', 'interval'),
    (0x49, 'H', 'delay'),
    (0x4b, 'i', 'timestamp'),
    (0x4f, '16s', 'zero4f'),
]

TRAP_FLAG_NAMES = {
    0x0001: 'open_door', 0x0002: 'door_action2?', 0x0008: 'toggle_door_lock',
    0x0010: 'wall_closed', 0x0020: 'wall_open_drop', 0x0040: 'delayed',
    0x0080: 'armed(runtime)', 0x0100: 'toggle_wall', 0x0200: 'toggle_state(runtime)',
    0x0400: 'light_done(runtime)', 0x8000: 'recharge',
}


def field_size(code):
    return 12 if code == 'V' else struct.calcsize('<' + code)


def check_layout(fields, size):
    pos = 0
    for off, code, _name in fields:
        assert off == pos, 'layout gap at 0x%x' % pos
        pos += field_size(code)
    assert pos == size, 'layout size %d != %d' % (pos, size)


check_layout(TRIGGER_FIELDS, TRIGGER_SIZE)
check_layout(TRAP_FIELDS, TRAP_SIZE)


def unpack_record(fields, b, base):
    rec = {}
    for off, code, name in fields:
        if code == 'V':
            rec[name] = struct.unpack_from('<3i', b, base + off)
        else:
            rec[name] = struct.unpack_from('<' + code, b, base + off)[0]
    return rec


def format_record(fields, rec, defaults):
    parts = []
    for _off, code, name in fields:
        v = rec[name]
        if name in defaults and v == defaults[name]:
            continue
        if code == 'V':
            parts.append('%s=(%.3f, %.3f, %.3f)' % ((name,) + tuple(fix(x) for x in v)))
        elif isinstance(v, bytes):
            parts.append('%s=%s' % (name, v.hex()))
        elif name in ('flags', 'activation_mask') or name.startswith('player_mask'):
            parts.append('%s=0x%x' % (name, v))
        else:
            parts.append('%s=%d' % (name, v))
    return ' '.join(parts)


TRIGGER_DEFAULTS = {
    'unk0f': 0, 'flags': 0, 'unk17': -1, 'need_item_class': -1, 'need_item_id': -1,
    'unk1d': -1, 'lack_item_class': -1, 'lack_item_id': -1, 'take_item': 0,
    'player_mask_a?': 0xff, 'player_mask_b?': 0xff, 'sound': -1, 'tex_on': -1,
    'tex_off': -1, 'fire_count': 0, 'unk30': 0, 'skip_count': 0,
    'cond35_unused': -1, 'cond37_unused': -1, 'cond39_unused': -1,
    'first_trap': -1, 'enable_trigger': -1, 'enable_trap': -1,
    'disable_trigger': -1, 'disable_trap': -1, 'object_num?': 1,
    'next_in_seg': -1, 'say_index': -1, 'zero4d': bytes(12),
}

TRAP_DEFAULTS = {
    'unk03': 0, 'charges_init': -1, 'charges': -1, 'unk09': -1, 'sound': -1,
    'next_trap': -1, 'flags': 0, 'weapon_type?': -1, 'spawn_object?': -1,
    'unk2f': -1, 'stat_delta': 0, 'robot_type': -1, 'robot_arg': -1,
    'tex_disabled': -1, 'tex_enabled': -1, 'unk3b': -1, 'unk3d': -1,
    'light_delta': 0, 'delay_init': 0, 'interval': 0, 'delay': 0,
    'timestamp': -1, 'zero4f': bytes(16),
}


def parse_tig(b):
    if len(b) < 6:
        raise FormatError('TIG too short')
    version, count = struct.unpack_from('<Ih', b, 0)
    if version != 2:
        raise FormatError('TIG version %d, expected 2' % version)
    if not 0 < count < 255:
        raise FormatError('TIG count %d out of range 1..254' % count)
    need = 6 + 2 * MAX_SEGMENTS + count * TRIGGER_SIZE
    if len(b) != need:
        raise FormatError('TIG size %d, expected %d' % (len(b), need))
    seg_table = struct.unpack_from('<%dh' % MAX_SEGMENTS, b, 6)
    base = 6 + 2 * MAX_SEGMENTS
    trigs = [unpack_record(TRIGGER_FIELDS, b, base + i * TRIGGER_SIZE) for i in range(count)]
    return version, seg_table, trigs


def tig_problems(seg_table, trigs):
    """Check the per-segment chains against the seg fields."""
    probs = []
    reached = set()
    for seg, head in enumerate(seg_table):
        t = head
        steps = 0
        while t != -1:
            if not 0 <= t < len(trigs):
                probs.append('seg %d chain reaches bad trigger %d' % (seg, t))
                break
            if trigs[t]['seg'] != seg:
                probs.append('trigger %d in chain of seg %d has seg %d' % (t, seg, trigs[t]['seg']))
            reached.add(t)
            t = trigs[t]['next_in_seg']
            steps += 1
            if steps > 255:
                probs.append('seg %d chain loops' % seg)
                break
    for i in range(len(trigs)):
        if i not in reached:
            probs.append('trigger %d is in no segment chain' % i)
    return probs


def dump_tig(b):
    version, seg_table, trigs = parse_tig(b)
    print('version %d, %d triggers' % (version, len(trigs)))
    print('segment table (segment: first trigger):')
    print('  ' + ' '.join('%d:%d' % (s, t) for s, t in enumerate(seg_table) if t != -1))
    for i, t in enumerate(trigs):
        print('trigger %d: %s' % (i, format_record(TRIGGER_FIELDS, t, TRIGGER_DEFAULTS)))
    for p in tig_problems(seg_table, trigs):
        print('NOTE: ' + p)


# --------------------------------------------------------------------- TAP

def parse_tap(b):
    if len(b) < 6:
        raise FormatError('TAP too short')
    version, count = struct.unpack_from('<Ih', b, 0)
    if version != 2:
        raise FormatError('TAP version %d, expected 2' % version)
    if not 0 <= count <= 1024:
        raise FormatError('TAP count %d out of range 0..1024' % count)
    need = 6 + count * TRAP_SIZE
    if len(b) != need:
        raise FormatError('TAP size %d, expected %d' % (len(b), need))
    return version, [unpack_record(TRAP_FIELDS, b, 6 + i * TRAP_SIZE) for i in range(count)]


def dump_tap(b):
    version, traps = parse_tap(b)
    print('version %d, %d traps' % (version, len(traps)))
    for i, t in enumerate(traps):
        names = [n for bit, n in sorted(TRAP_FLAG_NAMES.items()) if t['flags'] & bit]
        rest = t['flags'] & ~sum(TRAP_FLAG_NAMES)
        if rest:
            names.append('0x%x' % rest)
        line = format_record(TRAP_FIELDS, t, TRAP_DEFAULTS)
        if names:
            line += ' [' + ','.join(names) + ']'
        print('trap %d: %s' % (i, line))


# --------------------------------------------------------------------- SAY

def nibble_swap(data):
    return bytes(((c << 4) | (c >> 4)) & 0xff for c in data)


def parse_say(b):
    if len(b) < 12:
        raise FormatError('SAY too short')
    encoded, count = struct.unpack_from('<II', b, 0)
    if encoded not in (0, 1):
        raise FormatError('SAY encoding flag %d, expected 0 or 1' % encoded)
    if 8 + 4 * (count + 1) > len(b):
        raise FormatError('SAY offset table runs past end of file')
    offs = struct.unpack_from('<%dI' % (count + 1), b, 8)
    if offs[0] != 8 + 4 * (count + 1):
        raise FormatError('SAY first text does not follow the offset table')
    if offs[-1] != len(b):
        raise FormatError('SAY last offset %d != file size %d' % (offs[-1], len(b)))
    texts = []
    for i in range(count):
        if offs[i + 1] < offs[i]:
            raise FormatError('SAY offsets not ascending at %d' % i)
        t = b[offs[i]:offs[i + 1]]
        if encoded:
            t = nibble_swap(t.split(b'\0')[0])
        texts.append(t)
    return encoded, texts


def dump_say(b):
    encoded, texts = parse_say(b)
    print('encoded %d, %d messages' % (encoded, len(texts)))
    for i, t in enumerate(texts):
        print('%d: %s' % (i, t.decode('cp437')))


# ------------------------------------------------------------------- store

def parse_store(b):
    if len(b) % 10:
        raise FormatError('THESTORE size %d is not a multiple of 10' % len(b))
    items = []
    for o in range(0, len(b), 10):
        tail, item_id, item_class, quantity = struct.unpack_from('<IBBi', b, o)
        # The game swaps bytes 0..3 with 6..9 and byte 4 with byte 5 to get its
        # in-memory record: s32 quantity, u8 class, u8 id, u32 tail.
        items.append((item_class, item_id, quantity, tail))
    return items


def dump_store(b):
    for i, (c, n, q, tail) in enumerate(parse_store(b)):
        print('%2d: class %d id %d quantity %s tail %d' % (i, c, n, 'unlimited' if q == -1 else q, tail))


# --------------------------------------------------------------------- FNT

FT_COLOR, FT_PROPORTIONAL, FT_KERNED = 1, 2, 4


def parse_fnt(b):
    if b[:4] != b'PSFN':
        raise FormatError('FNT magic %r' % b[:4])
    datasize = struct.unpack_from('<I', b, 4)[0]
    (w, h, flags, baseline, minchar, maxchar, bytewidth,
     p_data, p_chars, p_widths, p_kern) = struct.unpack_from('<hhhhBBhIIII', b, 8)
    n = maxchar - minchar + 1
    spans = [(0, 8, 'file header'), (8, 36, 'font header')]
    if flags & FT_PROPORTIONAL:
        widths = list(struct.unpack_from('<%dh' % n, b, 8 + p_widths))
        spans.append((8 + p_widths, 8 + p_widths + 2 * n, 'widths'))
    else:
        widths = [w] * n
    if flags & FT_COLOR:
        size = sum(widths) * h
    else:
        size = sum((x + 7) // 8 for x in widths) * h
    spans.append((8 + p_data, 8 + p_data + size, 'glyph data'))
    kern = []
    if flags & FT_KERNED:
        k = 8 + p_kern
        while b[k] != 0xff:
            kern.append(tuple(b[k:k + 3]))
            k += 3
        spans.append((8 + p_kern, k + 1, 'kerning'))
    if flags & FT_COLOR:
        spans.append((8 + datasize, 8 + datasize + 768, 'palette'))
    spans.sort()
    gaps = []
    pos = 0
    for a, e, _name in spans:
        if a != pos:
            gaps.append((pos, a))
        pos = max(pos, e)
    if pos != len(b):
        gaps.append((pos, len(b)))
    info = dict(datasize=datasize, w=w, h=h, flags=flags, baseline=baseline,
                minchar=minchar, maxchar=maxchar, bytewidth=bytewidth,
                widths=widths, kern=kern, spans=spans)
    return info, gaps


def dump_fnt(b):
    info, gaps = parse_fnt(b)
    f = info['flags']
    kinds = [n for bit, n in ((1, 'color'), (2, 'proportional'), (4, 'kerned')) if f & bit]
    print('datasize %d, cell %dx%d, flags %d (%s), baseline %d, chars %d..%d, bytewidth %d' % (
        info['datasize'], info['w'], info['h'], f, ','.join(kinds) or 'mono fixed',
        info['baseline'], info['minchar'], info['maxchar'], info['bytewidth']))
    for a, e, name in info['spans']:
        print('  %6d..%6d %s' % (a, e, name))
    print('widths: ' + ' '.join(str(x) for x in info['widths']))
    print('kerning triples: %d' % len(info['kern']))
    print('unexplained ranges: %s' % (gaps or 'none'))


# ------------------------------------------------------------------- check

def check_dir(d):
    bad = 0
    counts = {}

    def files(pattern):
        seen = {}
        for pat in (pattern.upper(), pattern.lower()):
            for f in glob.glob(os.path.join(d, pat)) + glob.glob(os.path.join(d, 'SAVE', pat)):
                seen[os.path.normcase(f)] = f
        return sorted(seen.values())

    def run(kind, pattern, fn):
        nonlocal bad
        for f in files(pattern):
            counts[kind] = counts.get(kind, 0) + 1
            try:
                notes = fn(open(f, 'rb').read(), f) or []
            except (FormatError, struct.error, IndexError) as e:
                notes = ['FAILED: %s' % e]
                bad += 1
            for n in notes:
                print('%s: %s' % (os.path.basename(f), n))

    def c_brn(b, f):
        parse_brn(b)

    def c_tig(b, f):
        _v, seg_table, trigs = parse_tig(b)
        notes = tig_problems(seg_table, trigs)
        say = os.path.splitext(f)[0] + '.SAY'
        if os.path.exists(say):
            n = len(parse_say(open(say, 'rb').read())[1])
            for i, t in enumerate(trigs):
                if t['say_index'] >= n:
                    notes.append('trigger %d say_index %d >= %d messages' % (i, t['say_index'], n))
        tap = os.path.splitext(f)[0] + '.TAP'
        if os.path.exists(tap):
            n = len(parse_tap(open(tap, 'rb').read())[1])
            for i, t in enumerate(trigs):
                for k in ('first_trap', 'enable_trap', 'disable_trap'):
                    if not -1 <= t[k] < n:
                        notes.append('trigger %d %s %d out of range' % (i, k, t[k]))
        for i, t in enumerate(trigs):
            for k in ('enable_trigger', 'disable_trigger'):
                if not -1 <= t[k] < len(trigs):
                    notes.append('trigger %d %s %d out of range' % (i, k, t[k]))
            if t['zero4d'] != bytes(12):
                notes.append('trigger %d has nonzero bytes at 0x4d' % i)
        return notes

    def c_tap(b, f):
        _v, traps = parse_tap(b)
        notes = []
        for i, t in enumerate(traps):
            if t['id'] != i:
                notes.append('trap %d has id %d' % (i, t['id']))
            if not -1 <= t['next_trap'] < len(traps):
                notes.append('trap %d next_trap %d out of range' % (i, t['next_trap']))
            if t['zero4f'] != bytes(16):
                notes.append('trap %d has nonzero bytes at 0x4f' % i)
        return notes

    def c_say(b, f):
        parse_say(b)

    def c_fnt(b, f):
        _info, gaps = parse_fnt(b)
        return ['unexplained ranges %s' % gaps] if gaps else []

    def c_store(b, f):
        parse_store(b)

    run('BRN', '*.BRN', c_brn)
    run('TIG', '*.TIG', c_tig)
    run('TAP', '*.TAP', c_tap)
    run('SAY', '*.SAY', c_say)
    run('FNT', '*.FNT', c_fnt)
    run('THESTORE', 'THESTORE.*', c_store)
    print('parsed: ' + ', '.join('%s %d' % kv for kv in sorted(counts.items())))
    print('files that failed to parse: %d' % bad)
    return 1 if bad else 0


def main(argv):
    if len(argv) < 3 or argv[1] not in ('brn', 'tig', 'tap', 'say', 'store', 'fnt', 'check'):
        sys.stderr.write(__doc__)
        return 2
    cmd, path = argv[1], argv[2]
    if cmd == 'check':
        return check_dir(path)
    b = open(path, 'rb').read()
    try:
        if cmd == 'brn':
            dump_brn(b, '--matrix' in argv[3:])
        elif cmd == 'tig':
            dump_tig(b)
        elif cmd == 'tap':
            dump_tap(b)
        elif cmd == 'say':
            dump_say(b)
        elif cmd == 'store':
            dump_store(b)
        elif cmd == 'fnt':
            dump_fnt(b)
    except FormatError as e:
        sys.stderr.write('error: %s\n' % e)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
