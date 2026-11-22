#!/usr/bin/env python3
"""lang_extract.py: the text catalog of ASCENT (docs/formats/lang.md).

Commands:

  extract --datadir DIR [--datadir DIR ...] [--engine DIR] [--srt DIR] --out DIR
      Read the player-visible English text of the game files and of the
      code and write DIR/en.catalog and DIR/report.txt. The data
      directories are searched in order, as the game does (-datadir): the
      first directory that has a file supplies it. --srt DIR adds the
      subtitles DIR/<MOVIE>.en.srt.

  split --dir DIR
      Cut DIR/en.catalog into the translation parts DIR/en.part<X>.catalog
      (only the first of each group of equal English texts) and write
      DIR/parts.md.

  merge --dir DIR --lang CODE [--port-out FILE]
      Read DIR/en.catalog and the translated parts DIR/CODE.part<X>.catalog
      and write DIR/CODE.catalog (the text of the game files), FILE (the
      text of the code, default engine/lang/CODE_port.catalog) and
      DIR/<MOVIE>.CODE.srt. Then check the result (as check).

  check --dir DIR --lang CODE
      Check DIR/CODE.catalog against DIR/en.catalog: every id has a
      translation, the format placeholders (%s %d ...), the markup of the
      front end (~c ~s ~p ~e ~n ~m and the number after it), the script
      tokens ({name} ...) and the escapes are the same, and the lengths fit
      the places that show the text. Prints the problems, returns 1 on an
      error.

The catalogs hold text of the game (never commit them). The tool needs the
Python 3 standard library only.
"""
import os
import re
import sys
from collections import Counter, OrderedDict

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..'))
sys.path.insert(0, HERE)
sys.dont_write_bytecode = True
import egg  # noqa: E402
import jrg  # noqa: E402
import script  # noqa: E402
import sidefiles  # noqa: E402
import unpack  # noqa: E402

N_TEXT_STRINGS = 979          # engine/main/text.h
HUD_MESSAGE_LENGTH = 150      # engine/main/hud.c: a longer message is cut
MENU_TEXT = 48                # engine/main/dtufe.c Gfx_text, newmenu NM_MAX_TEXT_LEN 50
PART_NAMES = 'ABCDEF'
GAB_TOKENS = ['name', 'class', 'classes', 'race', 'races', 'boy', 'man', 'sir', 'gentleman']

# ---------------------------------------------------------------- escapes

def escape(b, tokens=False):
    """Game bytes to the text of a catalog value (docs/formats/lang.md)."""
    out = []
    for i, c in enumerate(b):
        if tokens and 9 <= c <= 0x11:
            out.append('{%s}' % GAB_TOKENS[c - 9])
        elif c == 0x5C:
            out.append('\\\\')
        elif c == 0x0A:
            out.append('\\n')
        elif c == 0x09:
            out.append('\\t')
        elif c < 0x20 or c >= 0x7F:
            out.append('\\x%02X' % c)
        elif c == 0x20 and (i == len(b) - 1 or i == 0):
            out.append('\\x20')       # a space at an end stays visible
        else:
            out.append(chr(c))
    return ''.join(out)


def unescape(s, tokens=False):
    """A catalog value to game bytes: escapes, tokens, UTF-8 to Latin-1."""
    out = bytearray()
    i = 0
    repl = {'\u2018': "'", '\u2019': "'", '\u201c': '"', '\u201d': '"', '\u2013': '-', '\u2014': '-',
            '\u00a0': ' ', '\u2026': '...'}
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            n = s[i + 1]
            if n == 'n':
                out.append(10); i += 2; continue
            if n == 't':
                out.append(9); i += 2; continue
            if n == 'x' and re.match(r'[0-9A-Fa-f]{2}', s[i + 2:i + 4]):
                out.append(int(s[i + 2:i + 4], 16)); i += 4; continue
            out += n.encode('latin-1', 'replace'); i += 2; continue
        if tokens and c == '{':
            m = re.match(r'\{([a-zA-Z]+)\}', s[i:])
            if m and m.group(1).lower() in GAB_TOKENS:
                out.append(9 + GAB_TOKENS.index(m.group(1).lower()))
                i += len(m.group(0))
                continue
        c = repl.get(c, c)
        out += c.encode('latin-1', 'replace')
        i += 1
    return bytes(out)


# ---------------------------------------------------------------- catalog files

FIELDS = ('id', 'src', 'ctx', 'max', 'dup', 'en', 'tr')


def read_catalog(path):
    """Returns an OrderedDict id -> dict of fields."""
    entries = OrderedDict()
    cur = None
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip('\n').rstrip('\r')
            if line.startswith('#'):
                continue
            if not line.strip():
                cur = None
                continue
            m = re.match(r'([a-z]+):(?: (.*))?$', line)
            if not m:
                raise SystemExit('%s:%d: not a "key: value" line: %r' % (path, n, line[:60]))
            k, v = m.group(1), m.group(2) or ''
            if k == 'id':
                if v in entries:
                    raise SystemExit('%s:%d: the id %s is there two times' % (path, n, v))
                cur = entries[v] = {'id': v}
            elif cur is None:
                raise SystemExit('%s:%d: a %s line before the id line' % (path, n, k))
            else:
                cur[k] = v
    return entries


def write_catalog(path, entries, header, fields=FIELDS):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        for h in header:
            f.write('# %s\n' % h if h else '#\n')
        f.write('\n')
        for e in entries:
            for k in fields:
                if k in e and (e[k] != '' or k in ('en', 'tr')):
                    f.write('%s: %s\n' % (k, e[k]) if e[k] != '' else '%s:\n' % k)
            f.write('\n')


# ---------------------------------------------------------------- game files

class Data:
    def __init__(self, dirs):
        self.dirs = dirs
        self.lists = [{n.upper(): os.path.join(d, n) for n in os.listdir(d)} for d in dirs]

    def find(self, name):
        for l in self.lists:
            if name.upper() in l:
                return l[name.upper()]
        return None

    def load(self, name):
        """dtu_load_file: the plain file, else NAME.HEN for a .BIN, in each directory."""
        base, ext = os.path.splitext(name.upper())
        packed = {'.BIN': '.HEN', '.LVL': '.DOG', '.EGG': '.COW'}.get(ext)
        for l in self.lists:
            if name.upper() in l:
                with open(l[name.upper()], 'rb') as f:
                    return f.read()
            if packed and base + packed in l:
                with open(l[base + packed], 'rb') as f:
                    return unpack.unpack(f.read())
        return None

    def names(self, ext):
        out = set()
        for l in self.lists:
            out.update(os.path.splitext(n)[0] for n in l if n.endswith(ext))
        return sorted(out)


def mission_levels(data):
    """The level names of DTU.MSN in order (index = level number)."""
    raw = data.load('DTU.MSN')
    levels = []
    if not raw:
        return levels
    lines = raw.decode('latin-1').splitlines()
    n = None
    for line in lines:
        t = line.split(';')[0].strip()
        if not t:
            continue
        m = re.match(r'num_levels\s*=\s*(\d+)', t)
        if m:
            n = int(m.group(1))
            continue
        if n is not None and len(levels) < n and '=' not in t:
            levels.append(os.path.splitext(t)[0].upper())
    return levels


# ---------------------------------------------------------------- engine sources

def c_literals(text, pos):
    """The C string literals that start at pos (adjacent ones are joined). Returns (bytes, end) or None."""
    out = bytearray()
    i = pos
    found = False
    while True:
        while i < len(text) and text[i] in ' \t\r\n':
            i += 1
        if i >= len(text) or text[i] != '"':
            break
        found = True
        i += 1
        while i < len(text) and text[i] != '"':
            c = text[i]
            if c == '\\':
                n = text[i + 1]
                if n == 'n': out.append(10); i += 2
                elif n == 't': out.append(9); i += 2
                elif n == 'x':
                    m = re.match(r'[0-9A-Fa-f]{1,2}', text[i + 2:])
                    out.append(int(m.group(0), 16)); i += 2 + len(m.group(0))
                elif n in '01234567':
                    m = re.match(r'[0-7]{1,3}', text[i + 1:])
                    out.append(int(m.group(0), 8) & 255); i += 1 + len(m.group(0))
                else:
                    out += n.encode('latin-1'); i += 2
            else:
                out += c.encode('latin-1')
                i += 1
        i += 1
    return (bytes(out), i) if found else None


def visible(b):
    return re.search(rb'[A-Za-z]{2}', b) is not None


# The calls whose string arguments the program shows (and translates at
# that place: HUD_init_message, newmenu, nm_messagebox, fe_menu, the
# message box of the game, the choice box), and the markers of dtulang.h.
CALL_RULES = [
    ('LT', 'all'), ('LN', 'all'),
    ('HUD_init_message', 'first'),
    ('say', 'first'), ('say_effect', 'all'),
    ('dtu_char_choice', 'all'),
    ('nm_messagebox', 'all'), ('nm_messagebox1', 'all'),
    ('fe_help_message', 'first'),
    ('dtu_display_add_message', 'first'),
]


def call_args(text, pos):
    """The text of the arguments of a call whose '(' is at pos, and the end."""
    depth = 0
    i = pos
    in_str = False
    while i < len(text):
        c = text[i]
        if in_str:
            if c == '\\':
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[pos + 1:i], i
        i += 1
    return text[pos + 1:], len(text)


def port_strings(engine_dir):
    """The strings of the code: [(english bytes, file:line)]"""
    out = []
    main = os.path.join(engine_dir, 'main')
    names = sorted(f for f in os.listdir(main) if f.endswith('.c'))
    # only the files of the program (MAIN_SOURCES of engine/main/CMakeLists.txt)
    cm = os.path.join(main, 'CMakeLists.txt')
    if os.path.exists(cm):
        m = re.search(r'set\(MAIN_SOURCES(.*?)\)', open(cm, encoding='latin-1').read(), re.S)
        if m:
            built = set(m.group(1).split())
            names = [n for n in names if n in built]
    call_re = re.compile(r'\b(%s)\s*\(' % '|'.join(re.escape(n) for n, _ in CALL_RULES))
    rules = dict(CALL_RULES)
    for name in names:
        if name.startswith('test_') or name in ('dtusoak.c',):
            continue
        text = open(os.path.join(main, name), encoding='latin-1').read()
        # no comments
        text = re.sub(r'/\*.*?\*/', lambda m: re.sub(r'[^\n]', ' ', m.group(0)), text, flags=re.S)
        text = re.sub(r'//[^\n]*', lambda m: ' ' * len(m.group(0)), text)
        for m in call_re.finditer(text):
            fn = m.group(1)
            # a definition or declaration of the function itself
            before = text[max(0, m.start() - 40):m.start()]
            if re.search(r'(void|int|char|static)\s*\**\s*$', before):
                continue
            args, _ = call_args(text, m.end() - 1)
            line = text.count('\n', 0, m.start()) + 1
            i = 0
            first = True
            while i < len(args):
                if args[i] == '"':
                    r = c_literals(args, i)
                    if r:
                        b, i = r
                        if visible(b):
                            out.append((b, '%s:%d' % (name, line), fn))
                        if rules[fn] == 'first':
                            break
                        first = False
                        continue
                if args[i] == ',' and rules[fn] == 'first':
                    break
                i += 1
    return out


def tex_usage(engine_dir):
    """String number -> the files of the code that use it, and the TXT_ names of text.h."""
    use = {}
    main = os.path.join(engine_dir, 'main')
    pats = [re.compile(p) for p in (r'Text_string\[\s*(\d+)\s*\]', r'\bfe_text\(\s*(\d+)', r'\bgtext\(\s*(\d+)',
                                     r'\bgame_text\(\s*(\d+)', r'\btext\(\s*(\d+)\s*,', r'\bmessage\(\s*(\d+)\s*,',
                                     r'\bturn_message\(\s*(\d+)', r'\{\s*"[^"]*",\s*(\d+),')]
    for name in sorted(os.listdir(main)):
        if not name.endswith('.c') or name.startswith('test_'):
            continue
        text = open(os.path.join(main, name), encoding='latin-1').read()
        for p in pats:
            for m in p.finditer(text):
                use.setdefault(int(m.group(1)), set()).add(name)
    names = {}
    th = os.path.join(main, 'text.h')
    if os.path.exists(th):
        for m in re.finditer(r'#define\s+(TXT_\w+)\s+Text_string\[\s*(\d+)\s*\]', open(th, encoding='latin-1').read()):
            names.setdefault(int(m.group(2)), m.group(1))
    return use, names


# ---------------------------------------------------------------- extraction

def tex_strings(raw):
    """load_text of text.c: CR dropped, one string per line, escapes."""
    text = raw.replace(b'\r', b'')
    lines = text.split(b'\n')
    out = []
    for i in range(N_TEXT_STRINGS):
        s = lines[i] if i < len(lines) else b''
        r = bytearray()
        j = 0
        while j < len(s):
            if s[j] == 0x5C and j + 1 < len(s):
                n = s[j + 1]
                r.append({ord('n'): 10, ord('t'): 9, 0x5C: 0x5C}.get(n, n))
                j += 2
            else:
                r.append(s[j])
                j += 1
        out.append(bytes(r))
    return out


def fetxt_strings(data):
    j = data.load('FETXTE.JRG')
    d = data.load('FETXTE.DAT')
    out = []
    for off, size in jrg.parse_fetxt(j):
        b = bytes(((c << 4) | (c >> 4)) & 0xFF for c in d[off:off + size])
        out.append(b[:size - 1] if size else b'')
    return out


BLURBS = [(311, 10), (321, 8), (329, 2), (331, 10), (341, 10), (351, 10), (361, 10), (371, 2), (373, 2), (375, 5),
          (380, 2), (382, 2), (384, 5), (389, 5), (394, 3), (397, 5), (402, 2), (404, 2), (406, 5), (411, 5),
          (416, 5), (421, 2), (423, 5), (428, 5), (433, 2), (435, 7), (442, 7), (449, 6), (455, 6), (461, 6),
          (467, 6), (473, 6), (479, 6), (485, 2)]   # engine/main/dtufe_town.c Blurb_table (0x142840)
RACES = ['human', 'half-elf', 'elf', 'drow', 'dwarf', 'halfling']


def fetxt_context(n):
    if n <= 5:
        return 'race text of character generation (%s), small font, prewrapped in a box' % RACES[n], 'lines'
    for m, (first, cnt) in enumerate(BLURBS):
        if first <= n < first + cnt:
            return ('Khelben in Blackstaff tower, mission %d, blurb %d; the port wraps the words; ~c color, '
                    '~s speech, ~p ~e picture, ~n line break, ~m ? (keep every ~ code and its number)' % (m, n - first)), None
    if 6 <= n <= 12:
        return 'front end text (not shown by the port yet)', None
    if 13 <= n <= 39:
        return 'scroll or book text (the scroll text screen is not in the port yet); ~ codes as in the Khelben text', None
    if 272 <= n <= 310:
        return 'quest text (not shown by the port yet)', None
    return 'front end text (use not known; many are placeholders)', None


GAB_KIND = {'SAY': 'NPC line (conversation window)', 'REPLY': 'reply of the player (a choice in the conversation window)',
            'NARRATE': 'narration (conversation window, or a message in the message box)',
            'LINES': 'status line of an NPC (message box, at most %d bytes)' % HUD_MESSAGE_LENGTH}


class GabScript(script.Script):
    """Records each string of a text operand: (offset, bytes, kind, label)."""

    def __init__(self, data, sid):
        super().__init__(data, sid)
        self.texts = []
        self.label = None
        self.kind = None

    def text(self):
        import inspect
        caller = inspect.stack()[1].function
        kind = {'say': None, 'statement': 'NARRATE', 'parse': 'LINES'}.get(caller, 'TEXT')
        if caller == 'say':
            kind = 'SAY' if self.d[self.p - 1] == 0xCD else 'REPLY'
        p = self.p
        while p < len(self.d) and self.d[p] < 0x7A:
            e = self.d.index(0, p)
            self.texts.append((p, self.d[p:e], kind, self.cur_label))
            p = e + 1
        return super().text()

    def emit(self, at, text):
        if text.startswith('LABEL '):
            self.cur_label = text[6:]
        super().emit(at, text)

    cur_label = None


def gab_strings(data):
    rlh = data.find('GABE.RLH')
    out = []
    for sid, rec in script.load(rlh):
        s = GabScript(rec, sid)
        s.parse()
        prev_say = None
        for off, b, kind, label in s.texts:
            if kind == 'SAY':
                prev_say = (sid, off)
            out.append((sid, off, b, kind, label, prev_say if kind == 'REPLY' else None))
    return out


def table_names(data):
    """name= of $OBJECT, $ARMOR, $SPELL, $WEAPON (and weapon_name=), $DRESSING: english -> (class, tables)"""
    names = OrderedDict()
    cls = {'$OBJECT': 'misc', '$ARMOR': 'armor', '$SPELL': 'spell', '$WEAPON': 'weapon', '$DRESSING': 'dressing'}
    for base in data.names('.BIN') + data.names('.HEN'):
        raw = data.load(base + '.BIN')
        if not raw or len(raw) < 14:
            continue
        _, text = egg.decode_bin(raw)
        for line in text.decode('latin-1').split('\n'):
            line = line.split(';')[0]
            m = re.match(r'\s*@?(\$[A-Z_]+)', line)
            if not m or m.group(1) not in cls:
                continue
            vals = re.findall(r'\b(name|weapon_name)\s*=\s*("[^"]*"|\S+)', line)
            # 0x20AD7: name= and weapon_name= write the same name, the last one wins
            if not vals:
                continue
            v = vals[-1][1].strip('"')
            if not visible(v.encode('latin-1')):
                continue
            e = names.setdefault(v, [cls[m.group(1)], set()])
            e[1].add(base)
    return names


def slug(s):
    t = re.sub(r'[^a-z0-9]+', '_', s.lower()).strip('_')
    return t[:40] or 'x'


def read_srt(path):
    cues = []
    with open(path, encoding='utf-8-sig') as f:
        blocks = re.split(r'\n\s*\n', f.read().replace('\r', '').strip())
    for b in blocks:
        lines = b.split('\n')
        if len(lines) >= 2 and '-->' in lines[1]:
            cues.append((lines[0].strip(), lines[1].strip(), '\n'.join(lines[2:])))
    return cues


def cmd_extract(args):
    dirs, engine, srtdir, out = args['datadir'], args.get('engine', [os.path.join(ROOT, 'engine')])[0], args.get('srt', [None])[0], args['out'][0]
    data = Data(dirs)
    entries = []
    report = Counter()
    first_en = {}

    def add(e, group, max_note=None):
        key = (group, e['en'])
        if key in first_en:
            e['dup'] = first_en[key]
        else:
            first_en[key] = e['id']
        if max_note:
            e['max'] = max_note
        entries.append(e)
        report[e['src'].split(' ')[0]] += 1

    # DTUE.TEX
    use, txt_names = tex_usage(engine)
    tex = tex_strings(data.load('DTUE.TEX'))
    for i, b in enumerate(tex):
        if not visible(b):
            continue
        ctx = []
        if i in txt_names:
            ctx.append('Descent 1 name %s' % txt_names[i])
        if 845 <= i <= 905:
            ctx.append('creature name in messages')
        if i in use:
            ctx.append('used in ' + ', '.join(sorted(use[i])))
        else:
            ctx.append('not used by the port code (may be a leftover of Descent 1)')
        lines = b.split(b'\n')
        note = None
        if len(lines) > 1:
            note = '%d lines, longest %d characters' % (len(lines), max(len(l) for l in lines))
        add({'id': 'tex.%d' % i, 'src': 'DTUE.TEX line %d' % (i + 1), 'ctx': '; '.join(ctx), 'en': escape(b)}, 'tex', note)

    # FETXTE
    for i, b in enumerate(fetxt_strings(data)):
        if not visible(b):
            continue
        ctx, kind = fetxt_context(i)
        note = None
        if kind == 'lines':
            lines = b.split(b'\n')
            note = '%d lines, longest %d characters' % (len(lines), max(len(l) for l in lines))
        add({'id': 'fe.%d' % i, 'src': 'FETXTE entry %d' % i, 'ctx': ctx, 'en': escape(b)}, 'fe', note)

    # GABE
    levels = mission_levels(data)
    for sid, off, b, kind, label, prompt in gab_strings(data):
        if not visible(b):
            continue
        level = levels[0] if sid < 10 and levels else (levels[sid // 10] if sid // 10 < len(levels) else 'global')
        if sid >= 508:
            level = 'global'
        ctx = '%s; level %s, script %d, label %s' % (GAB_KIND.get(kind, kind), level, sid, label)
        if prompt:
            ctx += '; answers gab.%d.%d' % prompt
        note = '%d bytes (message box)' % HUD_MESSAGE_LENGTH if kind == 'LINES' else None
        add({'id': 'gab.%d.%d' % (sid, off), 'src': 'GABE script %d offset %d' % (sid, off), 'ctx': ctx,
             'en': escape(b, tokens=True)}, 'gab', note)

    # NAME.SAY
    for base in data.names('.SAY'):
        raw = data.load(base + '.SAY')
        try:
            _, texts = sidefiles.parse_say(raw)
        except sidefiles.FormatError:
            continue
        for i, b in enumerate(texts):
            if not visible(b):
                continue
            ctx = 'level message of %s (a trigger shows it in the message box, which wraps the words)' % base
            if i == 0:
                ctx += '; message 0 is the title line of the file, some triggers show it'
            add({'id': 'say.%s.%d' % (base, i), 'src': '%s.SAY message %d' % (base, i), 'ctx': ctx,
                 'en': escape(b)}, 'say')

    # tables
    seen = set()
    for name, (c, tables) in table_names(data).items():
        i = 'tbl.%s.%s' % (c, slug(name))
        k = 2
        while i in seen:
            i = 'tbl.%s.%s_%d' % (c, slug(name), k)
            k += 1
        seen.add(i)
        add({'id': i, 'src': 'tables (%s)' % ', '.join(sorted(tables)[:4]) + (' and %d more' % (len(tables) - 4) if len(tables) > 4 else ''),
             'ctx': '%s name (inventory, market, messages); found by its English text' % c, 'en': escape(name.encode('latin-1'))},
            'tbl', '%d characters' % 39)

    # code
    seen_port = {}
    for b, where, fn in port_strings(engine):
        en = escape(b)
        if en in seen_port:
            seen_port[en]['ctx'] += ', ' + where
            continue
        i = 'port.' + slug(b.decode('latin-1'))
        k = 2
        while i in seen:
            i = 'port.%s_%d' % (slug(b.decode('latin-1')), k)
            k += 1
        seen.add(i)
        note = None
        if fn in ('HUD_init_message', 'say', 'say_effect', 'dtu_display_add_message'):
            note = '%d bytes with the values (message box)' % HUD_MESSAGE_LENGTH
        e = {'id': i, 'src': 'engine (%s)' % fn, 'ctx': 'text of the program, found by its English text; ' + where, 'en': en}
        seen_port[en] = e
        add(e, 'port', note)
    # the in-game menu box: newmenu items are short
    for e in seen_port.values():
        if e['src'].startswith('engine (LN') and 'max' not in e:
            e['max'] = '%d characters (menu item)' % MENU_TEXT

    # subtitles
    if srtdir:
        for f in sorted(os.listdir(srtdir)):
            m = re.match(r'(.+)\.en\.srt$', f, re.I)
            if not m:
                continue
            movie = m.group(1).upper()
            for num, times, text in read_srt(os.path.join(srtdir, f)):
                add({'id': 'sub.%s.%s' % (movie, num), 'src': '%s.MVE speech (transcript)' % movie,
                     'ctx': 'subtitle %s, %s; at most 2 lines of about 40 characters' % (num, times),
                     'en': escape(text.encode('latin-1', 'replace'))}, 'sub', '2 lines, 42 characters')

    os.makedirs(out, exist_ok=True)
    write_catalog(os.path.join(out, 'en.catalog'), entries,
                  ['ASCENT text catalog: en (the English text of the game files and of the program)',
                   'Made by tools/lang_extract.py extract. Holds text of the game: never commit it.',
                   'Format: docs/formats/lang.md'])
    lines = ['Text catalog: %d entries, %d unique texts' % (len(entries), sum(1 for e in entries if 'dup' not in e)), '']
    by = Counter(e['id'].split('.')[0] for e in entries)
    uniq = Counter(e['id'].split('.')[0] for e in entries if 'dup' not in e)
    words = Counter()
    for e in entries:
        if 'dup' not in e:
            words[e['id'].split('.')[0]] += len(re.findall(r"[A-Za-z']+", e['en']))
    for k in ('tex', 'fe', 'gab', 'say', 'tbl', 'port', 'sub'):
        lines.append('%-5s %6d entries %6d unique %7d words' % (k, by[k], uniq[k], words[k]))
    with open(os.path.join(out, 'report.txt'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print('\n'.join(lines))
    print('wrote', os.path.join(out, 'en.catalog'))
    return 0


# ---------------------------------------------------------------- parts

def part_of(e):
    """The part of an entry: A DTUE.TEX and the program, B front end, C-E scripts, F tables, levels, subtitles."""
    kind = e['id'].split('.')[0]
    if kind in ('tex', 'port'):
        return 'A'
    if kind == 'fe':
        return 'B'
    if kind == 'gab':
        return None   # C, D or E by size
    return 'F'


def cmd_split(args):
    d = args['dir'][0]
    entries = list(read_catalog(os.path.join(d, 'en.catalog')).values())
    parts = {p: [] for p in PART_NAMES}
    gab = [e for e in entries if e['id'].startswith('gab.') and 'dup' not in e]
    total = sum(len(e['en']) for e in gab)
    acc = 0
    cur = 'C'
    last_sid = None
    for e in gab:
        sid = e['id'].split('.')[1]
        # a script stays in one part
        if sid != last_sid and acc >= total * {'C': 1, 'D': 2}.get(cur, 9) / 3:
            cur = chr(ord(cur) + 1)
        last_sid = sid
        acc += len(e['en'])
        parts[cur].append(e)
    for e in entries:
        if 'dup' in e or e['id'].startswith('gab.'):
            continue
        parts[part_of(e)].append(e)
    rows = []
    for p in PART_NAMES:
        name = 'en.part%s.catalog' % p
        write_catalog(os.path.join(d, name), parts[p],
                      ['ASCENT translation part %s: copy this file to <lang>.part%s.catalog and add a "tr:" line' % (p, p),
                       'after each "en:" line. Format and rules: docs/formats/lang.md, gamedata/lang/parts.md.',
                       'Holds text of the game: never commit it.'])
        kinds = Counter(e['id'].split('.')[0] for e in parts[p])
        words = sum(len(re.findall(r"[A-Za-z']+", e['en'])) for e in parts[p])
        chars = sum(len(e['en']) for e in parts[p])
        rows.append((p, name, len(parts[p]), words, chars, ', '.join('%s %d' % kv for kv in sorted(kinds.items()))))
    write_parts_md(d, rows)
    for r in rows:
        print('%s %-20s %5d entries %6d words %7d characters  %s' % r)
    return 0


def write_parts_md(d, rows):
    desc = {'A': 'DTUE.TEX (menus, messages, help texts of the front end, creature names) and the text of the program',
            'B': 'FETXTE: race texts, Khelben in Blackstaff tower, scrolls and quest texts',
            'C': 'GABE scripts, first third (conversations, replies, narration)',
            'D': 'GABE scripts, second third',
            'E': 'GABE scripts, last third',
            'F': 'item, spell and scenery names of the tables, level messages (NAME.SAY), movie subtitles'}
    with open(os.path.join(d, 'parts.md'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('# Translation parts\n\n')
        f.write('Made by `tools/lang_extract.py split`. Each part is a file of entries in the format of\n'
                '`docs/formats/lang.md`. Only the first of a group of equal English texts is in a part;\n'
                'the merge copies its translation to the others (the `dup:` lines of en.catalog).\n\n')
        f.write('| Part | File | Entries | Words | Characters | Content |\n|---|---|---|---|---|---|\n')
        for p, name, n, w, c, kinds in rows:
            f.write('| %s | `%s` | %d | %d | %d | %s (%s) |\n' % (p, name, n, w, c, desc[p], kinds))
        f.write('\n## How to translate a part\n\n')
        f.write(TRANSLATOR_RULES)


TRANSLATOR_RULES = '''1. Take one part, for example `en.partC.catalog`. Write `es.partC.catalog` in the same
   folder: the same entries in the same order, each with one new line `tr: <translation>`
   right after its `en:` line. Keep the `id:` and `en:` lines unchanged (the merge checks
   the English text). You may drop the `src:`, `ctx:`, `max:` lines. UTF-8, LF line ends.
2. One line per value. Write a line break of the text as `\\n`, a tab as `\\t`, a
   backslash as `\\\\`. `\\xNN` is a byte of the game font (keep it as it is). A space at the
   start or the end of a value is `\\x20`.
3. Keep every placeholder in the same order: `%s`, `%d`, `%c`, `%%`, `%.100s` and the
   other printf forms. The program fills them in that order.
4. Keep the markup of the front end text exactly (FETXT, Khelben): `~c` with its number
   (color), `~s` with its number (Khelben speaks a sound), `~p`, `~e`, `~n` (line break),
   `~m`. Move a `~s` code only to the sentence that goes with it.
5. Keep the script tokens of the conversations: `{name}` (the name of the player),
   `{class}`, `{classes}`, `{race}`, `{races}`, `{boy}`, `{man}`, `{sir}`, `{gentleman}`.
   The program fills them with words of DTUE.TEX (in Spanish, from part A); write the
   sentence so that the word fits (gender: `{boy}` is chico/chica, `{man}` hombre/mujer,
   `{sir}` señor/señora, `{gentleman}` caballero/dama, chosen by the sex of the player).
6. Lengths: `max:` says how much room the text has. A message of the message box is cut
   at 150 bytes after the values are filled in. A menu item has about 48 characters. A text
   with several lines (`N lines, longest L characters`) is drawn as it is: keep the line
   breaks and keep each line no longer than L characters (the line count may change by one).
   Khelben's text and the conversations are wrapped by the program: their line breaks do not matter.
7. Names of people and places stay as in English (Waterdeep, Khelben Arunsun, Durnan,
   Undermountain, Halaster). Use the terms of `glossary.md`. Use "tú" for the player (never "vos").
8. Write an entry with a `dup:` line? There are none in the parts: the merge copies the
   translation to the equal texts.
9. Do not translate the names of keys or files, nor text that is clearly a placeholder of
   the developers ("This is hint scroll 27", "Foobar"): write `tr:` with the English text.
10. Check your file: `python -I tools/lang_extract.py merge --dir gamedata/lang --lang es`
    (it works with some parts missing and lists what is missing).
'''


# ---------------------------------------------------------------- merge and check

PH_RE = re.compile(r'%[-+ #0]*(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|h|ll|l|L|z|j|t)?[diouxXeEfFgGaAcspn%]')
MARK_RE = re.compile(r'~[a-z]\d*')
TOK_RE = re.compile(r'\{(?:%s)\}' % '|'.join(GAB_TOKENS), re.I)
ESC_RE = re.compile(r'\\x[0-9A-Fa-f]{2}')


def check_entry(e, tr, problems):
    en = e['en']
    kind = e['id'].split('.')[0]
    if kind in ('port', 'tex', 'say', 'gab', 'tbl'):
        a, b = PH_RE.findall(en), PH_RE.findall(tr)
        if a != b:
            problems.append(('error', e['id'], 'placeholders %s, translation has %s' % (' '.join(a) or '-', ' '.join(b) or '-')))
    if kind == 'fe':
        a, b = sorted(MARK_RE.findall(en)), sorted(MARK_RE.findall(tr))
        if a != b:
            problems.append(('error', e['id'], 'markup %s, translation has %s' % (' '.join(a), ' '.join(b))))
    if kind == 'gab':
        a, b = sorted(t.lower() for t in TOK_RE.findall(en)), sorted(t.lower() for t in TOK_RE.findall(tr))
        if a != b:
            problems.append(('error', e['id'], 'tokens %s, translation has %s' % (' '.join(a) or '-', ' '.join(b) or '-')))
    a, b = sorted(ESC_RE.findall(en)), sorted(ESC_RE.findall(tr))
    if a != b and kind != 'sub':
        problems.append(('error', e['id'], 'byte escapes %s, translation has %s' % (' '.join(a), ' '.join(b))))
    raw = unescape(tr, tokens=(kind == 'gab'))
    bad = [c for c in tr if ord(c) > 255 and c not in '\u2018\u2019\u201c\u201d\u2013\u2014\u2026']
    if bad:
        problems.append(('error', e['id'], 'characters that the fonts cannot show: %s' % ''.join(sorted(set(bad)))))
    m = e.get('max', '')
    if 'bytes' in m:
        limit = int(m.split()[0])
        extra = sum({'d': 4, 's': 20}.get(p[-1], 2) for p in PH_RE.findall(en))
        if len(raw) + extra > limit:
            problems.append(('long', e['id'], '%d bytes (about %d with the values), the room is %d' % (len(raw), len(raw) + extra, limit)))
    elif 'lines' in m:
        # "N lines, longest L characters" (a text drawn as it is: one more
        # line is allowed) or "N lines, L characters" (a subtitle)
        n_lines = int(m.split()[0])
        longest = int(re.search(r'(\d+) characters', m).group(1))
        sub = 'longest' not in m
        lines = raw.split(b'\n')
        if len(lines) > n_lines + (0 if sub else 1):
            problems.append(('long', e['id'], '%d lines, the English text has %d' % (len(lines), n_lines)))
        over = [len(l) for l in lines if len(l) > longest]
        if over:
            problems.append(('long', e['id'], 'lines of %s characters, the longest English line has %d' % (', '.join(map(str, over)), longest)))
    elif 'characters' in m:
        limit = int(m.split()[0])
        for l in raw.split(b'\n'):
            if len(l) > limit:
                problems.append(('long', e['id'], '%d characters, the room is about %d' % (len(l), limit)))
    elif kind == 'tex' and '\\n' not in en and len(en) >= 6 and len(tr) > len(en) * 1.6 + 4:
        problems.append(('long', e['id'], '%d characters, the English text has %d (check the place)' % (len(tr), len(en))))


def cmd_merge(args, write=True):
    d = args['dir'][0]
    lang = args['lang'][0]
    port_out = args.get('port-out', [os.path.join(ROOT, 'engine', 'lang', '%s_port.catalog' % lang)])[0]
    en = read_catalog(os.path.join(d, 'en.catalog'))
    problems = []
    tr = {}
    if write:
        for p in PART_NAMES:
            path = os.path.join(d, '%s.part%s.catalog' % (lang, p))
            if not os.path.exists(path):
                problems.append(('missing', 'part %s' % p, 'no file %s' % os.path.basename(path)))
                continue
            for i, e in read_catalog(path).items():
                if i not in en:
                    problems.append(('error', i, '%s: the id is not in en.catalog' % os.path.basename(path)))
                    continue
                if 'en' in e and e['en'] != en[i]['en']:
                    problems.append(('error', i, '%s: the English text differs from en.catalog' % os.path.basename(path)))
                    continue
                if 'tr' in e and (e['tr'] != '' or en[i]['en'] == ''):
                    tr[i] = e['tr']
    else:
        path = os.path.join(d, '%s.catalog' % lang)
        for i, e in read_catalog(path).items():
            if 'tr' in e:
                tr[i] = e['tr']
        if os.path.exists(port_out):
            for i, e in read_catalog(port_out).items():
                if 'tr' in e:
                    tr[i] = e['tr']
    # the equal texts
    for i, e in en.items():
        if i not in tr and 'dup' in e and e['dup'] in tr:
            tr[i] = tr[e['dup']]
    missing = [i for i in en if i not in tr]
    for i in en:
        if i in tr:
            check_entry(en[i], tr[i], problems)
    if write:
        game = [dict(id=i, en=e['en'], tr=tr[i]) for i, e in en.items() if i in tr and not i.startswith(('port.', 'sub.'))]
        port = [dict(id=i, en=e['en'], tr=tr[i]) for i, e in en.items() if i in tr and i.startswith('port.')]
        write_catalog(os.path.join(d, '%s.catalog' % lang), game,
                      ['ASCENT text catalog: %s (the text of the game files)' % lang,
                       'Made by tools/lang_extract.py merge. Holds text of the game: never commit it.',
                       'Format: docs/formats/lang.md'], fields=('id', 'en', 'tr'))
        write_catalog(port_out, port,
                      ['ASCENT text catalog: %s, the text of the program (port strings)' % lang,
                       'Format: docs/formats/lang.md. tools/lang_extract.py merge writes this file.'], fields=('id', 'en', 'tr'))
        # subtitles: the times of <MOVIE>.en.srt, the text of the catalog
        movies = sorted(set(i.split('.')[1] for i in en if i.startswith('sub.')))
        for movie in movies:
            src = None
            for f in os.listdir(d):
                if f.upper() == '%s.EN.SRT' % movie:
                    src = os.path.join(d, f)
            if not src or not any(i.startswith('sub.%s.' % movie) for i in tr):
                continue        # no translated cue: no file
            with open(os.path.join(d, '%s.%s.srt' % (movie, lang)), 'w', encoding='utf-8', newline='\n') as f:
                for num, times, text in read_srt(src):
                    t = tr.get('sub.%s.%s' % (movie, num))
                    t = t.replace('\\n', '\n') if t is not None else text
                    f.write('%s\n%s\n%s\n\n' % (num, times, t))
        print('wrote %s (%d entries), %s (%d entries)' % (os.path.join(d, '%s.catalog' % lang), len(game), port_out, len(port)))
    errors = [p for p in problems if p[0] == 'error']
    longs = [p for p in problems if p[0] == 'long']
    print('%d of %d entries translated, %d missing, %d errors, %d too long' % (len(tr), len(en), len(missing), len(errors), len(longs)))
    by = Counter(i.split('.')[0] for i in missing)
    if missing:
        print('missing by source: ' + ', '.join('%s %d' % kv for kv in sorted(by.items())))
    for p in problems:
        print('%-7s %s: %s' % p)
    return 1 if errors else 0


def cmd_check(args):
    return cmd_merge(args, write=False)


def parse_args(argv):
    out = {}
    i = 0
    while i < len(argv):
        a = argv[i]
        if a.startswith('--') and i + 1 < len(argv):
            out.setdefault(a[2:], []).append(argv[i + 1])
            i += 2
        else:
            raise SystemExit(__doc__)
    return out


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 1
    args = parse_args(argv[2:])
    cmd = argv[1]
    need = {'extract': ('datadir', 'out'), 'split': ('dir',), 'merge': ('dir', 'lang'), 'check': ('dir', 'lang')}
    if cmd not in need or any(k not in args for k in need[cmd]):
        print(__doc__)
        return 1
    return {'extract': cmd_extract, 'split': cmd_split, 'merge': cmd_merge, 'check': cmd_check}[cmd](args)


if __name__ == '__main__':
    sys.exit(main(sys.argv))
