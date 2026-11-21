#!/usr/bin/env python3
"""Polygon model tool for Descent to Undermountain EGG archives.

Reads one model of an EGG archive, applies the joint angles of one animation
frame as DTUGAME.EXE does (see docs/re/models.md) and writes a Wavefront OBJ
file or a PNG picture. The picture is a small software render, used to check
that the animation data is understood.

  python -I tools/model.py info  EGG MODEL
  python -I tools/model.py obj   EGG MODEL OUT.obj [--frame N]
  python -I tools/model.py sheet EGG MODEL OUT.png [--state walk | --frames 4,8,12]
                                 [--view side|front|top] [--cell 160]
  python -I tools/model.py check EGG [EGG ...]

MODEL is a model number or a POF name (for example durnan.pof).
Frame -1 is the rest pose (no joint angles).
"""
import argparse
import math
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import egg as egglib  # noqa: E402

F1 = 65536.0

# Animation states of DTUGAME.EXE. The names are the debug strings at
# 0x120004. FIRST_FRAME is the table of 16-bit values at 0x13D9E0: state s
# uses the frames FIRST_FRAME[s] to FIRST_FRAME[s + 1] - 1.
STATE_NAMES = ["rest", "stand", "walk", "sidle_r", "sidle_l", "run", "attack",
               "attack2", "flinch", "die", "fidget", "s11", "s12", "s13",
               "s14", "s15", "s16", "s17", "s18"]
FIRST_FRAME = [0, 0, 4, 20, 24, 28, 44, 60, 76, 84, 96, 108, 124, 140, 152,
               164, 168, 176, 180, 192]

# Bits of the per-submodel flag byte (last array of the data block).
SUB_FLAG_PITCH = 0x01      # extra pitch can be added (not used by the 1.3 code path)
SUB_FLAG_HIDDEN = 0x10     # drawn only on request (the weapon or missile in the hand)
SUB_FLAG_GLOW = 0x20       # drawn with the magic effect


def vec(d, o):
    return [x / F1 for x in struct.unpack_from("<3i", d, o)]


class PolyModel:
    def __init__(self, egg, m):
        self.egg = egg
        self.m = m
        self.name = m.name
        self.n = m.n_models
        self.n_frames = m.n_frames
        self.flags = m.flags
        raw = egg.model_data(m)
        self.sec = {}
        pos = 0
        for name, size in m.sections:
            self.sec[name] = raw[pos:pos + size]
            pos += size
        n = self.n
        s = self.sec
        self.data = s["model_data"]
        self.ptrs = list(struct.unpack_from("<%di" % n, s["submodel_ptrs"], 0))
        self.offsets = [vec(s["submodel_offsets"], 12 * i) for i in range(n)]
        self.norms = [vec(s["submodel_norms"], 12 * i) for i in range(n)]
        self.pnts = [vec(s["submodel_pnts"], 12 * i) for i in range(n)]
        self.rads = [x / F1 for x in struct.unpack_from("<%di" % n, s["submodel_rads"], 0)]
        self.parents = list(s["submodel_parents"])
        self.mins = [vec(s["submodel_mins"], 12 * i) for i in range(n)]
        self.maxs = [vec(s["submodel_maxs"], 12 * i) for i in range(n)]
        self.sub_flags = list(s["submodel_bytes"])
        h = m.header
        self.gun_submodels = [h[0x4A], h[0x4B]]
        self.n_guns = struct.unpack_from("<h", h, 0x4C)[0]
        self.gun_points = [vec(h, 0x4E), vec(h, 0x5A)]
        self.fire_frames = struct.unpack_from("<4h", h, 0x66)
        self.speeds = [x / F1 for x in struct.unpack_from("<8i", h, 0x6E)]
        self.state_times = [x / F1 for x in struct.unpack_from("<19i", h, 0x8E)]

    # ---- animation data ------------------------------------------------

    def angles(self, frame, sub):
        """Joint angles (pitch, bank, heading) of a submodel, 65536 per turn."""
        if frame < 0 or not self.n_frames:
            return (0, 0, 0)
        return struct.unpack_from("<3h", self.sec["anim_angles"],
                                  6 * (frame * self.n + sub))

    def frame_offset(self, frame):
        """Offset added to the object position for a frame (flag 1 array)."""
        if not self.flags & 1:
            return [0.0, 0.0, 0.0]
        return vec(self.sec["f1_vec"], 12 * (frame + 1))

    def children(self, sub):
        """Child list of a submodel from the data block (row of n bytes)."""
        row = self.sec["submodel_children"][sub * self.n:(sub + 1) * self.n]
        out = []
        for b in row:
            if b == 0xFF:
                break
            out.append(b)
        return out

    # ---- interpreter data ----------------------------------------------

    def polys(self, sub):
        """Parse the opcode stream of one submodel.

        Returns (points, polys). A polygon is a dict with the keys verts
        (point numbers), normal, tex (texture number or None), color.
        """
        d = self.data
        points = {}
        polys = []
        subcalls = []

        def walk(p):
            while True:
                op = struct.unpack_from("<H", d, p)[0]
                if op == 0:
                    return
                if op == 1:
                    nv = struct.unpack_from("<H", d, p + 2)[0]
                    for i in range(nv):
                        points[i] = vec(d, p + 4 + 12 * i)
                    p += 4 + 12 * nv
                elif op == 7:
                    nv, start = struct.unpack_from("<2H", d, p + 2)
                    for i in range(nv):
                        points[start + i] = vec(d, p + 8 + 12 * i)
                    p += 8 + 12 * nv
                elif op == 2:
                    nv = struct.unpack_from("<H", d, p + 2)[0]
                    color = struct.unpack_from("<H", d, p + 28)[0]
                    verts = struct.unpack_from("<%dH" % nv, d, p + 30)
                    polys.append({"verts": verts, "normal": vec(d, p + 16),
                                  "tex": None, "color": color})
                    p += 30 + ((nv & ~1) + 1) * 2
                elif op == 3:
                    nv = struct.unpack_from("<H", d, p + 2)[0]
                    tex = struct.unpack_from("<H", d, p + 28)[0]
                    verts = struct.unpack_from("<%dH" % nv, d, p + 30)
                    uv = p + 30 + ((nv & ~1) + 1) * 2
                    uvs = [struct.unpack_from("<2i", d, uv + 12 * i) for i in range(nv)]
                    polys.append({"verts": verts, "normal": vec(d, p + 16),
                                  "tex": tex, "color": None,
                                  "uv": [(u / F1, v / F1) for u, v in uvs]})
                    p += 30 + ((nv & ~1) + 1) * 2 + 12 * nv
                elif op == 4:
                    a, b = struct.unpack_from("<2H", d, p + 28)
                    walk(p + a)
                    walk(p + b)
                    p += 32
                elif op == 5:
                    p += 36
                elif op == 6:
                    subcalls.append(struct.unpack_from("<H", d, p + 2)[0])
                    p += 20
                elif op == 8:
                    p += 4
                else:
                    raise ValueError("%s: opcode %d at %d" % (self.name, op, p))

        walk(self.ptrs[sub])
        self.last_subcalls = subcalls
        return points, polys

    # ---- pose -------------------------------------------------------------

    def pose(self, frame, hidden=True):
        """Return (vertices, faces) of the whole model in object space.

        A face is (vertex numbers, normal, tex, color, submodel).
        """
        mats = {}

        def world(sub):
            if sub in mats:
                return mats[sub]
            local = (angles_matrix(self.angles(frame, sub)), self.offsets[sub])
            par = self.parents[sub]
            if par == 0xFF or par == sub:
                mats[sub] = local
            else:
                pm, po = world(par)
                lm, lo = local
                mats[sub] = (mat_mul(pm, lm), vadd(po, mat_vec(pm, lo)))
            return mats[sub]

        verts = []
        faces = []
        for sub in range(self.n):
            if not hidden and self.sub_flags[sub] & SUB_FLAG_HIDDEN:
                continue
            m, o = world(sub)
            points, polys = self.polys(sub)
            index = {}
            for k in sorted(points):
                index[k] = len(verts)
                verts.append(vadd(o, mat_vec(m, points[k])))
            for p in polys:
                faces.append(([index[v] for v in p["verts"]],
                              mat_vec(m, p["normal"]), p["tex"], p["color"], sub))
        return verts, faces


# ---- math: the Descent 1 conventions (vecmat) --------------------------------

def angles_matrix(a):
    """vm_angles_2_matrix. Returns the columns rvec, uvec, fvec as a 3x3 list
    m[row][col] so that mat_vec(m, p) = p.x*rvec + p.y*uvec + p.z*fvec."""
    p, b, h = [x * 2.0 * math.pi / 65536.0 for x in a]
    sp, cp = math.sin(p), math.cos(p)
    sb, cb = math.sin(b), math.cos(b)
    sh, ch = math.sin(h), math.cos(h)
    sbsh, cbch, cbsh, sbch = sb * sh, cb * ch, cb * sh, sb * ch
    rvec = (cbch + sp * sbsh, sb * cp, sp * sbch - cbsh)
    uvec = (sp * cbsh - sbch, cb * cp, sbsh + sp * cbch)
    fvec = (sh * cp, -sp, ch * cp)
    return [[rvec[i], uvec[i], fvec[i]] for i in range(3)]


def mat_vec(m, v):
    return [m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2] for i in range(3)]


def mat_mul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)]
            for i in range(3)]


def vadd(a, b):
    return [a[0] + b[0], a[1] + b[1], a[2] + b[2]]


# ---- output -------------------------------------------------------------------

def write_obj(path, model, frame):
    verts, faces = model.pose(frame)
    with open(path, "w") as f:
        f.write("# %s frame %d\n" % (model.name, frame))
        for v in verts:
            f.write("v %.5f %.5f %.5f\n" % (v[0], v[1], -v[2]))
        cur = None
        for idx, _n, tex, color, sub in faces:
            g = "sub%d" % sub
            if g != cur:
                f.write("g %s\n" % g)
                cur = g
            # z is negated above, so the order is reversed to keep the facing
            f.write("f " + " ".join(str(i + 1) for i in reversed(idx)) + "\n")


VIEWS = {
    # name: (screen x axis, screen y axis, depth axis) as (index, sign)
    "side": ((2, 1), (1, 1), (0, 1)),
    "front": ((0, 1), (1, 1), (2, -1)),
    "top": ((0, 1), (2, 1), (1, 1)),
}


def render(model, frames, view, cell, hidden=True):
    """Draw the frames side by side. Returns (width, height, pixels)."""
    ax, ay, az = VIEWS[view]
    poses = [model.pose(f, hidden) for f in frames]
    lo = [1e9, 1e9]
    hi = [-1e9, -1e9]
    for verts, _ in poses:
        for v in verts:
            x = v[ax[0]] * ax[1]
            y = v[ay[0]] * ay[1]
            lo[0] = min(lo[0], x)
            hi[0] = max(hi[0], x)
            lo[1] = min(lo[1], y)
            hi[1] = max(hi[1], y)
    span = max(hi[0] - lo[0], hi[1] - lo[1], 1e-6)
    scale = (cell - 8) / span
    w = cell * len(frames)
    h = cell
    pix = bytearray(w * h)
    light = (0.4, 0.7, -0.6)
    ln = math.sqrt(sum(c * c for c in light))
    light = [c / ln for c in light]
    for fi, (verts, faces) in enumerate(poses):
        x0 = fi * cell + 4 - lo[0] * scale + (cell - 8 - (hi[0] - lo[0]) * scale) / 2
        y0 = h - 4 + lo[1] * scale
        order = []
        for idx, n, tex, color, sub in faces:
            depth = sum(verts[i][az[0]] * az[1] for i in idx) / len(idx)
            order.append((depth, idx, n, tex, sub))
        order.sort(key=lambda t: -t[0])
        for depth, idx, n, tex, sub in order:
            nl = math.sqrt(sum(c * c for c in n)) or 1.0
            facing = n[az[0]] * az[1] / nl
            if facing > 0.05:
                continue          # back face (the viewer is at -depth)
            lum = max(0.0, sum(n[i] * light[i] for i in range(3)) / nl)
            base = 70 + 12 * ((tex if tex is not None else 5) % 6)
            shade = int(min(255, base * (0.45 + 0.9 * lum)))
            pts = [(x0 + verts[i][ax[0]] * ax[1] * scale,
                    y0 - verts[i][ay[0]] * ay[1] * scale) for i in idx]
            fill_poly(pix, w, h, pts, max(1, shade))
        for x in range(fi * cell, fi * cell + cell):
            pix[(h - 1) * w + x] = 255
        for y in range(h):
            pix[y * w + fi * cell] = 40
    return w, h, bytes(pix)


def fill_poly(pix, w, h, pts, color):
    ys = [p[1] for p in pts]
    y_lo = max(0, int(math.ceil(min(ys))))
    y_hi = min(h - 1, int(math.floor(max(ys))))
    n = len(pts)
    for y in range(y_lo, y_hi + 1):
        xs = []
        for i in range(n):
            x1, y1 = pts[i]
            x2, y2 = pts[(i + 1) % n]
            if y1 == y2:
                continue
            if (y1 <= y < y2) or (y2 <= y < y1):
                xs.append(x1 + (y - y1) * (x2 - x1) / (y2 - y1))
        xs.sort()
        for k in range(0, len(xs) - 1, 2):
            xa = max(0, int(math.ceil(xs[k])))
            xb = min(w - 1, int(math.floor(xs[k + 1])))
            if xb >= xa:
                pix[y * w + xa:y * w + xb + 1] = bytes([color]) * (xb - xa + 1)


# ---- commands -----------------------------------------------------------------

def find_model(egg, key):
    if key.isdigit():
        return egg.models[int(key)]
    for m in egg.models:
        if m.name.lower() == key.lower():
            return m
    raise SystemExit("no model %s in %s" % (key, egg.path))


def cmd_info(args):
    egg = egglib.Egg(args.egg)
    pm = PolyModel(egg, find_model(egg, args.model))
    m = pm.m
    print("%s: %d submodels, %d frames, %d textures, first_texture %d, flags 0x%02x"
          % (pm.name, pm.n, pm.n_frames, m.n_textures, m.first_texture, pm.flags))
    print("rad %.3f mins %s maxs %s" % (m.rad / F1, [round(x / F1, 3) for x in m.mins],
                                        [round(x / F1, 3) for x in m.maxs]))
    print("n_guns %d gun_submodels %s gun_points %s" % (pm.n_guns, pm.gun_submodels,
          [[round(c, 3) for c in g] for g in pm.gun_points[:max(pm.n_guns, 0)]]))
    print("fire_frames %s" % (pm.fire_frames,))
    print("speeds %s" % pm.speeds)
    print("state_times %s" % pm.state_times)
    for s in range(pm.n):
        pts, polys = pm.polys(s)
        print("sub %2d parent %3d flags 0x%02x offset %s points %3d polys %3d children %s subcalls %s"
              % (s, pm.parents[s], pm.sub_flags[s], [round(c, 3) for c in pm.offsets[s]],
                 len(pts), len(polys), pm.children(s), pm.last_subcalls))
    if pm.flags & 1:
        d = pm.sec
        f = pm.n_frames
        print("box 1 (torso): offset[0] %s height[0] %.3f radius[0] %.3f"
              % ([round(c, 3) for c in vec(d["f1_vec"], 0)],
                 struct.unpack_from("<i", d["f1_int_a"], 0)[0] / F1,
                 struct.unpack_from("<i", d["f1_int_b"], 0)[0] / F1))
        for flag, key, label in ((8, "f8", "box 8"), (2, "f2", "point 2"), (4, "f4", "point 4")):
            if pm.flags & flag and f:
                k = min(f - 1, 50)
                ints = [struct.unpack_from("<i", d[x], 4 * k)[0] / F1
                        for x in sorted(d) if x.startswith(key + "_int")]
                print("%s: frame %d vec %s values %s" % (label, k,
                      [round(c, 3) for c in vec(d[key + "_vec"], 12 * k)],
                      [round(c, 3) for c in ints]))


def cmd_obj(args):
    egg = egglib.Egg(args.egg)
    pm = PolyModel(egg, find_model(egg, args.model))
    write_obj(args.out, pm, args.frame)
    print("wrote %s (frame %d of %d)" % (args.out, args.frame, pm.n_frames))


def cmd_sheet(args):
    egg = egglib.Egg(args.egg)
    pm = PolyModel(egg, find_model(egg, args.model))
    if args.frames:
        frames = [int(x) for x in args.frames.split(",")]
    else:
        s = STATE_NAMES.index(args.state)
        frames = list(range(FIRST_FRAME[s], FIRST_FRAME[s + 1]))
        if args.step > 1:
            frames = frames[::args.step]
    frames = [f for f in frames if f < pm.n_frames]
    w, h, pix = render(pm, frames, args.view, args.cell, not args.no_hidden)
    egglib.write_png(args.out, w, h, pix, egglib.gray_palette())
    print("wrote %s: frames %s" % (args.out, frames))


def cmd_check(args):
    """Data checks for the claims of docs/re/models.md."""
    tot = bad_child = bad_ptr = subcall = with_frames = 0
    flag_count = {}
    frames = {}
    for path in args.egg:
        try:
            egg = egglib.Egg(path)
        except Exception as e:           # old layout archives
            print("%s: %s" % (os.path.basename(path), e))
            continue
        for m in egg.models:
            try:
                pm = PolyModel(egg, m)
            except Exception as e:
                print("%s %s: %s" % (os.path.basename(path), m.name, e))
                continue
            tot += 1
            if pm.n_frames:
                with_frames += 1
            frames[pm.n_frames] = frames.get(pm.n_frames, 0) + 1
            # child lists against the parent array
            for s in range(pm.n):
                want = [c for c in range(pm.n) if pm.parents[c] == s]
                if sorted(pm.children(s)) != want:
                    bad_child += 1
            for s in range(pm.n):
                try:
                    pm.polys(s)
                    subcall += len(pm.last_subcalls)
                except Exception:
                    bad_ptr += 1
            for b in pm.sub_flags:
                flag_count[b] = flag_count.get(b, 0) + 1
    print("models %d, with frames %d" % (tot, with_frames))
    print("frame counts: %s" % sorted(frames.items()))
    print("submodels whose child list differs from the parent array: %d" % bad_child)
    print("submodels whose opcode stream does not parse: %d" % bad_ptr)
    print("SUBCALL opcodes: %d" % subcall)
    print("submodel flag bytes: %s" % sorted(flag_count.items()))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("info")
    p.add_argument("egg")
    p.add_argument("model")
    p.set_defaults(fn=cmd_info)
    p = sub.add_parser("obj")
    p.add_argument("egg")
    p.add_argument("model")
    p.add_argument("out")
    p.add_argument("--frame", type=int, default=-1)
    p.set_defaults(fn=cmd_obj)
    p = sub.add_parser("sheet")
    p.add_argument("egg")
    p.add_argument("model")
    p.add_argument("out")
    p.add_argument("--state", default="walk", choices=STATE_NAMES)
    p.add_argument("--frames")
    p.add_argument("--step", type=int, default=1)
    p.add_argument("--view", default="side", choices=sorted(VIEWS))
    p.add_argument("--cell", type=int, default=160)
    p.add_argument("--no-hidden", action="store_true",
                   help="leave out the submodels with flag 0x10")
    p.set_defaults(fn=cmd_sheet)
    p = sub.add_parser("check")
    p.add_argument("egg", nargs="+")
    p.set_defaults(fn=cmd_check)
    args = ap.parse_args(argv)
    return args.fn(args) or 0


if __name__ == "__main__":
    sys.exit(main())
