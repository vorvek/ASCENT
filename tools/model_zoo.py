#!/usr/bin/env python3
"""Character model exchange between the EGG archives and Blender.

The character models are the models with animation frames: the creatures,
the characters of the levels, the arms and the bodies of the player
(docs/re/models.md). Many names share one mesh. The tool works with one
record for each mesh.

  python -I tools/model_zoo.py extract [--out DIR] [--game DIR] [--unpacked DIR]
  python -I tools/model_zoo.py build   [--out DIR] [--hd DIR]
  python -I tools/model_zoo.py check   FILE.pom [...]

extract  writes DIR/orig/<mesh>.json (points, polygons, texture numbers, UV,
         joints, joint angles), DIR/tex/<egg>/<bitmap>.png (the textures of
         the models, with the game palette) and DIR/index.json (the names
         and the archives of each mesh). <mesh> is the first name of the
         mesh without ".pof".
build    reads DIR/new/<mesh>.json (written by tools/blender/zoo.py from the
         improved meshes) and writes HD/polymodels/<name>.<crc>.pom for each
         name of the mesh. docs/formats/pom.md has the layout.
check    parses POM files and prints their counts.

DIR is gamedata/models by default. Its files are made from the game files:
never commit them.

Python 3 standard library only.
"""
import argparse
import glob
import hashlib
import json
import math
import os
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import egg as egglib  # noqa: E402
import model as modellib  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
F1 = 65536.0
MIN_FRAMES = 100          # a character model has 108 frames or more
POM_MAGIC = b"APOM"
POM_VERSION = 1


def mesh_hash(pm):
    return hashlib.sha1(pm.sec["model_data"] + pm.sec["submodel_ptrs"]).hexdigest()[:12]


def orig_crc(pm):
    return zlib.crc32(pm.sec["model_data"] + pm.sec["submodel_ptrs"]) & 0xFFFFFFFF


# ---- tables -------------------------------------------------------------------

def table_textures(bin_path):
    """Return (egg name, {model name: [bitmap names]}) of a BIN table."""
    with open(bin_path, "rb") as f:
        egg_name, body = egglib.decode_bin(f.read())
    out = {}
    tokens = body.decode("latin1").replace("\t", " ").split()
    i = 0
    while i < len(tokens):
        t = tokens[i]
        i += 1
        if not t.lower().startswith("model=") and not t.lower().startswith("simple_model="):
            continue
        name = t.split("=", 1)[1].lower()
        tex = []
        while i < len(tokens):
            low = tokens[i].lower()
            if low.endswith(".bbm") or low.endswith(".abm"):
                tex.append(low[:-4])
            elif low.startswith("%"):
                tex.append(low)
            elif tex or "=" in low or low[0] in "$@;":
                break                    # flag words can stand before the list
            i += 1
        out.setdefault(name, tex)
    return egg_name, out


# ---- extract ------------------------------------------------------------------

def model_record(pm):
    subs = []
    for s in range(pm.n):
        points, polys = pm.polys(s)
        keys = sorted(points)
        index = {k: i for i, k in enumerate(keys)}
        plist = []
        for p in polys:
            plist.append({
                "v": [index[v] for v in p["verts"]],
                "t": p["tex"],
                "c": p["color"],
                "uv": [list(x) for x in p.get("uv", [])],
                "n": p["normal"],
            })
        par = pm.parents[s]
        subs.append({
            "parent": -1 if par == 0xFF or par == s else par,
            "offset": pm.offsets[s],
            "flags": pm.sub_flags[s],
            "points": [points[k] for k in keys],
            "polys": plist,
        })
    angles = list(struct.unpack_from("<%dh" % (3 * pm.n * pm.n_frames), pm.sec["anim_angles"], 0))
    return {"subs": subs, "n_frames": pm.n_frames, "angles": angles}


def cmd_extract(args):
    out = os.path.abspath(args.out)
    check_out(out)
    os.makedirs(os.path.join(out, "orig"), exist_ok=True)
    palette = egglib.load_palette(os.path.join(args.game, "PALETTE.256"))
    eggs = sorted(glob.glob(os.path.join(args.unpacked, "*.EGG")))
    for n in ("HMC.EGG", "HMT.EGG", "PORTAL.EGG"):
        eggs.append(os.path.join(args.game, n))
    # tables: egg base name -> textures of its models
    tex_of = {}
    bins = sorted(glob.glob(os.path.join(args.unpacked, "*.BIN")))
    bins += [os.path.join(args.game, n) for n in ("HMC.BIN", "HMT.BIN")]
    for b in bins:
        if not os.path.exists(b):
            continue
        try:
            egg_name, t = table_textures(b)
        except Exception:
            continue
        d = tex_of.setdefault(egg_name.lower(), {})
        for k, v in t.items():
            d.setdefault(k, v)

    meshes = {}
    for path in eggs:
        base = os.path.basename(path)
        try:
            e = egglib.Egg(path)
        except Exception as ex:
            print("skip %s: %s" % (base, ex))
            continue
        bitmaps = {}
        for b in e.bitmaps:
            bitmaps.setdefault(b.name.lower(), b)       # frame 0 of an animation
        for m in e.models:
            if m.n_frames < MIN_FRAMES or m.n_models < 2:
                continue
            try:
                pm = modellib.PolyModel(e, m)
            except Exception:
                print("skip %s (old layout)" % base)
                break
            h = mesh_hash(pm)
            name = m.name.lower()
            tex = tex_of.get(base.lower(), {}).get(name)
            if h not in meshes:
                rec = model_record(pm)
                rec.update({"mesh": name[:-4], "hash": h, "names": [], "egg": base,
                            "textures": None, "tex_dir": None, "skins": {}})
                meshes[h] = rec
            rec = meshes[h]
            if [name, base, orig_crc(pm)] not in rec["names"]:
                rec["names"].append([name, base, orig_crc(pm)])
            if tex is not None and name not in rec["skins"]:
                tdir = os.path.join(out, "tex", base[:-4].lower())
                os.makedirs(tdir, exist_ok=True)
                for t in tex:
                    b = bitmaps.get(t)
                    if b is None:
                        continue
                    f = os.path.join(tdir, t.replace("#", "_") + ".png")
                    if not os.path.exists(f):
                        try:
                            egglib.write_png(f, b.width, b.height, e.bitmap_pixels(b), palette,
                                             (255,) if b.flags & 1 else ())
                        except Exception as ex:
                            print("texture %s of %s: %s" % (t, base, ex))
                rec["skins"][name] = [base[:-4].lower(), tex]
                if rec["textures"] is None:
                    rec["textures"] = tex
                    rec["tex_dir"] = base[:-4].lower()
                    rec["egg"] = base
    index = {}
    used = set()
    for h, rec in sorted(meshes.items(), key=lambda kv: kv[1]["mesh"]):
        mesh = rec["mesh"]
        while mesh in used:
            mesh += "_"
        used.add(mesh)
        rec["mesh"] = mesh
        with open(os.path.join(out, "orig", mesh + ".json"), "w") as f:
            json.dump(rec, f)
        polys = sum(len(s["polys"]) for s in rec["subs"])
        index[mesh] = {"hash": h, "names": rec["names"], "subs": len(rec["subs"]),
                       "polys": polys, "frames": rec["n_frames"],
                       "textures": rec["textures"] is not None}
    with open(os.path.join(out, "index.json"), "w") as f:
        json.dump(index, f, indent=1)
    print("%d meshes, %d names, %d without a texture list" % (
        len(index), sum(len(v["names"]) for v in index.values()),
        sum(1 for v in index.values() if not v["textures"])))
    return 0


# ---- build ---------------------------------------------------------------------

def fx(x):
    return struct.pack("<i", int(round(x * F1)))


def fvec(v):
    return b"".join(fx(c) for c in v)


def poly_record(pts, p):
    nv = len(p["v"])
    head = struct.pack("<HH", 3 if p["t"] is not None else 2, nv)
    head += fvec(pts[p["v"][0]]) + fvec(p["n"])
    head += struct.pack("<H", p["t"] if p["t"] is not None else (p["c"] or 0))
    head += struct.pack("<%dH" % nv, *p["v"])
    if nv % 2 == 0:
        head += bytes(2)
    if p["t"] is not None:
        for u, v in p["uv"]:
            head += fx(u) + fx(v) + fx(0)
    return head


def side(pts, p, point, normal, eps=1e-4):
    """1: all points of p in front of the plane, -1: behind, 0: both sides.
    A polygon in the plane counts as in front if it faces as the plane."""
    d = [sum((pts[v][i] - point[i]) * normal[i] for i in range(3)) for v in p["v"]]
    if min(d) >= -eps and max(d) <= eps:
        return 1 if sum(a * b for a, b in zip(p["n"], normal)) >= 0 else -1
    if min(d) >= -eps:
        return 1
    if max(d) <= eps:
        return -1
    return 0


def leaf(pts, polys):
    return b"".join(poly_record(pts, p) for p in polys) + struct.pack("<H", 0)


def convex_set(pts, polys):
    """No polygon has a part in front of the plane of another one: with back
    face culling, any draw order is correct."""
    for a in polys:
        point, normal = pts[a["v"][0]], a["n"]
        for b in polys:
            if b is not a and side(pts, b, point, normal, 1e-3) >= 0:
                d = max(sum((pts[v][i] - point[i]) * normal[i] for i in range(3)) for v in b["v"])
                if d > 1e-3:
                    return False
    return True


def bsp(pts, polys, depth=0):
    """The opcodes that draw the polygons from back to front: SORTNORM
    nodes (docs/formats/pom.md). No polygon is cut: a polygon on both sides
    of a plane goes to the side of its center. The planes are polygon planes
    or planes along an axis through the middle of the set."""
    if len(polys) <= 1 or depth >= 28 or (len(polys) <= 24 and convex_set(pts, polys)):
        return leaf(pts, polys)
    cens = [[sum(pts[v][i] for v in p["v"]) / len(p["v"]) for i in range(3)] for p in polys]
    cands = []
    step = max(1, len(polys) // 16)
    for cand in polys[::step]:
        cands.append((cand, pts[cand["v"][0]], cand["n"]))
    for ax in range(3):
        mid = sorted(c[ax] for c in cens)[len(cens) // 2]
        point = [0.0, 0.0, 0.0]
        normal = [0.0, 0.0, 0.0]
        point[ax] = mid
        normal[ax] = 1.0
        cands.append((None, point, normal))
    best = None
    for cand, point, normal in cands:
        f = b = c = 0
        for p in polys:
            if p is cand:
                continue
            k = side(pts, p, point, normal)
            if k > 0:
                f += 1
            elif k < 0:
                b += 1
            else:
                c += 1
        if cand is None and (f + c == 0 or b + c == 0):
            continue
        score = c * 3 + abs(f - b)
        if best is None or score < best[0]:
            best = (score, cand, point, normal)
    score, cand, point, normal = best
    front, back = [], []
    for p, cen in zip(polys, cens):
        if p is cand:
            continue
        k = side(pts, p, point, normal)
        if k == 0:
            k = 1 if sum((cen[i] - point[i]) * normal[i] for i in range(3)) >= 0 else -1
        (front if k > 0 else back).append(p)
    if cand is None and (not front or not back):
        return leaf(pts, polys)
    # A polygon in the plane is drawn after the back list and before the
    # front list: it goes first in the front stream.
    fd = (poly_record(pts, cand) if cand is not None else b"") + bsp(pts, front, depth + 1)
    bd = bsp(pts, back, depth + 1)
    # the offsets have 16 bits without a sign: the shorter stream goes first
    if len(fd) <= len(bd):
        off_f, off_b, body = 34, 34 + len(fd), fd + bd
    else:
        off_f, off_b, body = 34 + len(bd), 34, bd + fd
    if max(off_f, off_b) > 65535:
        raise ValueError("a submodel is too large for the 16 bit offsets")
    return (struct.pack("<HH", 4, 0) + fvec(normal) + fvec(point) + struct.pack("<HH", off_f, off_b)
            + struct.pack("<H", 0) + body)


def sub_stream(sub):
    pts = sub["points"]
    if not sub["polys"]:
        return struct.pack("<H", 0)
    if len(pts) > 1000:
        raise ValueError("more than 1000 points in a submodel")
    d = struct.pack("<HH", 1, len(pts)) + b"".join(fvec(p) for p in pts)
    return d + bsp(pts, sub["polys"])


def pom_body(new):
    """The interpreter data and the submodel pointers of an improved mesh."""
    data = b""
    ptrs = []
    for sub in new["subs"]:
        ptrs.append(len(data))
        data += sub_stream(sub)
    # The readers of the engine want 4 bytes at each opcode: two bytes
    # follow the last end mark, as in the archives.
    data += bytes(2)
    return data, struct.pack("<%di" % len(ptrs), *ptrs)


def pom_bytes(body, crc):
    data, ptrs = body
    head = POM_MAGIC + struct.pack("<IIII", POM_VERSION, crc, len(ptrs) // 4, len(data))
    return head + data + ptrs


def pom_name(name, crc):
    return "%s.%08x.pom" % (name.lower().replace(".pof", ""), crc)


def poly_set(sub):
    """The polygons of a submodel as a sorted list that does not depend on
    the point numbers, the polygon order or the first point of a polygon."""
    out = []
    for p in sub["polys"]:
        pts = [tuple(round(c, 3) for c in sub["points"][v]) for v in p["v"]]
        uvs = [tuple(round(c, 3) for c in uv) for uv in p["uv"]] or [()] * len(pts)
        ring = list(zip(pts, uvs))
        k = ring.index(min(ring))
        out.append((p["t"], p["c"], tuple(ring[k:] + ring[:k])))
    return sorted(out, key=repr)


def same_mesh(orig, new):
    return all(poly_set(a) == poly_set(b) for a, b in zip(orig["subs"], new["subs"]))


def cmd_build(args):
    out = os.path.abspath(args.out)
    dst = os.path.join(os.path.abspath(args.hd), "polymodels")
    check_out(dst)
    os.makedirs(dst, exist_ok=True)
    n = 0
    size = 0
    same = 0
    for old in glob.glob(os.path.join(dst, "*.pom")):
        os.remove(old)
    for f in sorted(glob.glob(os.path.join(out, "new", "*.json"))):
        with open(f) as fh:
            new = json.load(fh)
        with open(os.path.join(out, "orig", new["mesh"] + ".json")) as fh:
            orig = json.load(fh)
        if new["hash"] != orig["hash"] or len(new["subs"]) != len(orig["subs"]):
            print("%s: not made from this original, skipped" % new["mesh"])
            continue
        if args.skip_no_table and not orig["skins"]:
            continue
        if same_mesh(orig, new):
            same += 1                    # no change: the game keeps its model
            continue
        done = set()
        body = pom_body(new)
        for name, egg, crc in orig["names"]:
            if (name, crc) in done:
                continue
            done.add((name, crc))
            b = pom_bytes(body, crc)
            with open(os.path.join(dst, pom_name(name, crc)), "wb") as fh:
                fh.write(b)
            n += 1
            size += len(b)
    with open(os.path.join(dst, "README.txt"), "w", newline="\n") as fh:
        fh.write("Improved character and monster models of ASCENT (docs/formats/pom.md).\n"
                 "Made with tools/model_zoo.py from the models of the game.\n")
    print("%d files, %.1f MB in %s (%d meshes without a change left out)" % (n, size / 1e6, dst, same))
    return 0


def walk_pom(data, p, stats, depth=0):
    while True:
        op = struct.unpack_from("<H", data, p)[0]
        if op == 0:
            return
        if op == 1:
            nv = struct.unpack_from("<H", data, p + 2)[0]
            stats["points"] += nv
            p += 4 + 12 * nv
        elif op in (2, 3):
            nv = struct.unpack_from("<H", data, p + 2)[0]
            stats["polys"] += 1
            p += 30 + ((nv & ~1) + 1) * 2 + (12 * nv if op == 3 else 0)
        elif op == 4:
            a, b = struct.unpack_from("<2H", data, p + 28)
            walk_pom(data, p + a, stats, depth + 1)
            walk_pom(data, p + b, stats, depth + 1)
            stats["depth"] = max(stats["depth"], depth + 1)
            p += 32
        else:
            raise ValueError("opcode %d at %d" % (op, p))


def cmd_check(args):
    for path in args.files:
        with open(path, "rb") as f:
            d = f.read()
        magic, ver, crc, n, size = struct.unpack_from("<4sIIII", d, 0)
        if magic != POM_MAGIC or ver != POM_VERSION or len(d) != 20 + size + 4 * n:
            print("%s: bad header" % path)
            continue
        data = d[20:20 + size]
        ptrs = struct.unpack_from("<%di" % n, d, 20 + size)
        stats = {"points": 0, "polys": 0, "depth": 0}
        for p in ptrs:
            walk_pom(data, p, stats)
        print("%s: crc %08x, %d submodels, %d bytes, %d points, %d polygons, tree depth %d" % (
            os.path.basename(path), crc, n, size, stats["points"], stats["polys"], stats["depth"]))
    return 0


def check_out(out):
    inside = out == ROOT or out.startswith(os.path.join(ROOT, ""))
    if inside and not out.startswith(os.path.join(ROOT, "gamedata", "")):
        raise SystemExit("the output must be outside the repository or in gamedata/ (ignored by git)")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("extract")
    p.add_argument("--out", default=os.path.join(ROOT, "gamedata", "models"))
    p.add_argument("--game", default="E:/UNDAMONTAN/DTU")
    p.add_argument("--unpacked", default=os.path.join(ROOT, "gamedata", "unpacked"))
    p.set_defaults(fn=cmd_extract)
    p = sub.add_parser("build")
    p.add_argument("--out", default=os.path.join(ROOT, "gamedata", "models"))
    p.add_argument("--hd", default=os.path.join(ROOT, "gamedata", "hd"))
    p.add_argument("--all", dest="skip_no_table", action="store_false",
                   help="also the models that no table names")
    p.set_defaults(fn=cmd_build)
    p = sub.add_parser("check")
    p.add_argument("files", nargs="+")
    p.set_defaults(fn=cmd_check)
    args = ap.parse_args(argv)
    return args.fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())
