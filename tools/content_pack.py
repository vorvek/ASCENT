"""content_pack.py: build the downloadable content packs of ASCENT.

The packs go to a folder outside the repository (default gamedata/content).
The owner copies the folder to the content server. docs/install.md has the
formats and the server setup.

Usage:
    python -I tools/content_pack.py [--hd DIR] [--patch ZIP_OR_DIR] [--out DIR]
                                    [--lang DIR] [--version V] [PACK ...]

PACK is one of patch13, textures, ui, movies, lang_es, music (default:
all but lang_es and music).

  patch13   the files of the 1.3 patch that ascent-import needs
            (installer/patch13.txt), from the patch archive dtu1_3.zip or a
            folder with the extracted patch; each file is checked
  textures  <hd>/textures without the .cache folder
  ui        <hd>/ui
  movies    <hd>/*.m1v
  lang_es   the Spanish text: es.catalog (the translation of the game files)
            and the subtitles *.es.srt of the folder --lang (default
            gamedata/lang, made by tools/lang_extract.py merge), installed
            as <hd>/lang/... (docs/formats/lang.md), and the pictures of the
            language <hd>/lang/es/ (orig/ and ui/, made by tools/ui_lang.py).
            The catalog is checked first: an error of `lang_extract.py
            check` stops the pack.
  music     <hd>/music: the replacement music NAME.ogg that the setting
            "Streaming safe music" plays in place of NAME.TUN
            (engine/main/dtumusic.c), with README.txt and the notes
            NAME.txt of tools/music_replace.py. Only when named: the
            files are not in the repository.

The version of a pack is --version, else the number of PACK_VERSION (1 for
a pack that is not there).

A pack is a ZIP archive. The PNG and M1V files are stored, the text files
are compressed. Each pack has:
  _pack.txt    name, version, target ("hd" or "data"), title
  _files.txt   one line for each installed file: SHA-256, size, path
  _links.txt   the duplicates: "path<TAB>path of the stored copy"
Many files of <hd>/textures are hard links to one image: the pack stores
each image once, and the content tool makes the duplicates again (a hard
link, else a copy).

manifest.txt has one line for each pack, with the fields separated by tabs:
name, version, target, file, size, sha256, files, title.

Python 3 standard library only. The output never goes into the repository.
"""
import hashlib
import os
import sys
import zipfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
TITLES = {
    'patch13': 'Descent to Undermountain 1.3 patch files',
    'textures': 'Upscaled textures',
    'ui': 'Upscaled interface pictures',
    'movies': 'Upscaled movies',
    'lang_es': 'Spanish text and pictures (translation, subtitles, interface pictures)',
    'music': 'Replacement music (for the setting Streaming safe music)',
}
DEFAULT_PACKS = ('patch13', 'textures', 'ui', 'movies')
# 2: lang_es has the pictures of the language
# 3: the decided terms (Aguas Profundas, Bajomontaña, trasgo, ...), the
#    gender of the creature names, the Spanish name on the Waterdeep picture
#    (FEBGND 10)
# 5: Varanegra for Blackstaff (the title of Khelben and his tower), the
#    short wand name "Varita de Proyectiles"
PACK_VERSION = {'lang_es': '5'}
STORED = ('.png', '.m1v', '.zip', '.ogg')


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def read_patch13_list():
    out = []
    with open(os.path.join(ROOT, 'installer', 'patch13.txt')) as f:
        for line in f:
            p = line.split()
            if len(p) == 3 and len(p[0]) == 64:
                out.append((p[2], int(p[1]), p[0]))
    return out


def files_of_tree(base, sub, skip_dirs=('.cache',)):
    """(relative path with '/', full path) of each file under base/sub."""
    out = []
    top = os.path.join(base, sub)
    for r, ds, fs in os.walk(top):
        ds[:] = sorted(d for d in ds if d not in skip_dirs)
        for f in sorted(fs):
            full = os.path.join(r, f)
            out.append((os.path.relpath(full, base).replace(os.sep, '/'), full))
    return out


def write_pack(out_dir, name, version, target, entries, data=None):
    """entries: list of (relpath, full path). data: relpath -> bytes (instead of a file).
    Duplicates (same content) are stored once."""
    data = data or {}
    by_inode = {}
    infos = []          # (relpath, full, sha, size)
    for rel, full in entries:
        if rel in data:
            b = data[rel]
            infos.append((rel, None, hashlib.sha256(b).hexdigest(), len(b)))
            continue
        st = os.stat(full)
        key = (st.st_dev, st.st_ino) if st.st_ino else full
        if key not in by_inode:
            by_inode[key] = sha256_file(full)
        infos.append((rel, full, by_inode[key], st.st_size))

    stored_by_sha = {}
    links = []
    files_txt = []
    for rel, full, sha, size in infos:
        files_txt.append('%s %d %s' % (sha, size, rel))
        if sha in stored_by_sha:
            links.append('%s\t%s' % (rel, stored_by_sha[sha]))
        else:
            stored_by_sha[sha] = rel

    path = os.path.join(out_dir, name + '.zip')
    tmp = path + '.tmp'
    with zipfile.ZipFile(tmp, 'w', allowZip64=False) as z:
        def add_text(arc, text):
            z.writestr(zipfile.ZipInfo(arc, (2026, 1, 1, 0, 0, 0)), text.encode('utf-8'), zipfile.ZIP_DEFLATED)
        add_text('_pack.txt', 'name=%s\nversion=%s\ntarget=%s\ntitle=%s\n' % (name, version, target, TITLES[name]))
        add_text('_files.txt', '\n'.join(files_txt) + '\n')
        add_text('_links.txt', '\n'.join(links) + ('\n' if links else ''))
        stored = set(stored_by_sha.values())
        for rel, full, sha, size in infos:
            if rel not in stored:
                continue
            method = zipfile.ZIP_STORED if rel.lower().endswith(STORED) else zipfile.ZIP_DEFLATED
            if full is None:
                z.writestr(zipfile.ZipInfo(rel, (2026, 1, 1, 0, 0, 0)), data[rel], method)
            else:
                z.write(full, rel, method)
    os.replace(tmp, path)
    unique = sum(size for rel, full, sha, size in infos if rel in stored)
    print('%-8s %6d files, %6d stored, %6d links, %8.1f MB unique, pack %8.1f MB' % (
        name, len(infos), len(stored), len(links), unique / 1e6, os.path.getsize(path) / 1e6))
    return {'name': name, 'version': version, 'target': target, 'file': name + '.zip',
            'size': os.path.getsize(path), 'sha256': sha256_file(path), 'files': len(infos),
            'title': TITLES[name]}


def lang_files(lang_dir, code, hd):
    """The files of a language pack: <code>.catalog and <MOVIE>.<code>.srt of
    lang_dir, installed as lang/<name> in the hd folder, and the pictures of
    the language, <hd>/lang/<code>/. The catalog must pass the check of
    tools/lang_extract.py."""
    import subprocess
    cat = os.path.join(lang_dir, '%s.catalog' % code)
    if not os.path.exists(cat):
        raise SystemExit('no %s: run tools/lang_extract.py merge first' % cat)
    r = subprocess.run([sys.executable, '-I', os.path.join(ROOT, 'tools', 'lang_extract.py'), 'check',
                        '--dir', lang_dir, '--lang', code], capture_output=True, text=True)
    print(r.stdout.splitlines()[0] if r.stdout else '')
    if r.returncode != 0:
        raise SystemExit('the check of %s found errors:\n%s' % (cat, r.stdout))
    out = [('lang/%s.catalog' % code, cat)]
    for f in sorted(os.listdir(lang_dir)):
        if f.lower().endswith('.%s.srt' % code):
            out.append(('lang/' + f, os.path.join(lang_dir, f)))
    if not os.path.isdir(os.path.join(hd, 'lang', code)):
        raise SystemExit('no %s: run tools/ui_lang.py build first' % os.path.join(hd, 'lang', code))
    return out + files_of_tree(hd, 'lang/' + code)


def patch13_data(patch):
    """The bytes of the 1.3 files from a ZIP archive or a folder, checked."""
    need = read_patch13_list()
    data = {}
    if os.path.isdir(patch):
        names = {n.upper(): n for n in os.listdir(patch)}
        for name, size, sha in need:
            if name.upper() not in names:
                raise SystemExit('the patch folder has no %s' % name)
            with open(os.path.join(patch, names[name.upper()]), 'rb') as f:
                data[name] = f.read()
    else:
        with zipfile.ZipFile(patch) as z:
            names = {os.path.basename(n).upper(): n for n in z.namelist()}
            for name, size, sha in need:
                if name.upper() not in names:
                    raise SystemExit('the patch archive has no %s' % name)
                data[name] = z.read(names[name.upper()])
    for name, size, sha in need:
        if len(data[name]) != size or hashlib.sha256(data[name]).hexdigest() != sha:
            raise SystemExit('%s of the patch is not the file of installer/patch13.txt' % name)
    return data


def main(argv):
    hd = os.path.join(ROOT, 'gamedata', 'hd')
    patch = os.path.join(ROOT, 'gamedata', 'patch', 'ps', 'dtu1_3.zip')
    out = os.path.join(ROOT, 'gamedata', 'content')
    lang = os.path.join(ROOT, 'gamedata', 'lang')
    version = None
    packs = []
    i = 1
    while i < len(argv):
        a = argv[i]
        if a in ('--hd', '--patch', '--out', '--version', '--lang') and i + 1 < len(argv):
            v = argv[i + 1]
            if a == '--hd': hd = v
            elif a == '--patch': patch = v
            elif a == '--out': out = v
            elif a == '--lang': lang = v
            else: version = v
            i += 2
        elif a in TITLES:
            packs.append(a)
            i += 1
        else:
            print(__doc__)
            return 1
    packs = packs or list(DEFAULT_PACKS)
    out = os.path.abspath(out)
    if out == ROOT or out.startswith(os.path.join(ROOT, '')) and not out.startswith(os.path.join(ROOT, 'gamedata', '')):
        raise SystemExit('the output must be outside the repository or in gamedata/ (ignored by git)')
    os.makedirs(out, exist_ok=True)

    # The manifest keeps the packs that are not built again.
    manifest = {}
    mpath = os.path.join(out, 'manifest.txt')
    if os.path.exists(mpath):
        with open(mpath, encoding='utf-8') as f:
            for line in f:
                p = line.rstrip('\n').split('\t')
                if len(p) == 8 and not line.startswith('#'):
                    manifest[p[0]] = dict(zip(('name', 'version', 'target', 'file', 'size', 'sha256', 'files', 'title'), p))

    for name in packs:
        version_of = version or PACK_VERSION.get(name, '1')
        if name == 'patch13':
            data = patch13_data(patch)
            m = write_pack(out, name, version_of, 'data', [(n, None) for n in data], data)
        elif name == 'textures':
            m = write_pack(out, name, version_of, 'hd', files_of_tree(hd, 'textures'))
        elif name == 'ui':
            m = write_pack(out, name, version_of, 'hd', files_of_tree(hd, 'ui'))
        elif name == 'music':
            m = write_pack(out, name, version_of, 'hd', files_of_tree(hd, 'music'))
        elif name.startswith('lang_'):
            m = write_pack(out, name, version_of, 'hd', lang_files(lang, name[5:], hd))
        else:
            m = write_pack(out, name, version_of, 'hd',
                           [(f, os.path.join(hd, f)) for f in sorted(os.listdir(hd)) if f.lower().endswith('.m1v')])
        manifest[name] = m

    with open(mpath, 'w', encoding='utf-8', newline='\n') as f:
        f.write('# ASCENT content manifest, format 1\n')
        f.write('# name\tversion\ttarget\tfile\tsize\tsha256\tfiles\ttitle\n')
        for name in TITLES:
            if name in manifest:
                m = manifest[name]
                f.write('\t'.join(str(m[k]) for k in ('name', 'version', 'target', 'file', 'size', 'sha256', 'files', 'title')) + '\n')
    print('wrote', mpath)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
