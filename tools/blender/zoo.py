"""Blender side of tools/model_zoo.py: the character model zoo.

Run inside Blender (4.2 or later):

    import sys; sys.path.insert(0, r"<repo>/tools/blender"); import zoo
    zoo.DIR = r"<repo>/gamedata/models"
    zoo.begin("durnan")        # the original and a copy to improve, side by side
    ...                        # edit the objects durnan.new.sNN
    zoo.finish("durnan")       # checks, new/durnan.json, blend/durnan.blend
    zoo.build_zoo()            # zoo.blend with a link to each model file

Axes: the game has x right, y up, z forward. Blender gets (-x, -z, y), so a
model stands up and looks along -Y (the front view shows the face). A face of the game is the mirror image
here, so the vertex order is reversed in both directions. V of the UV is
1 - v of the game.

Each submodel is one mesh object. Its parent is the object of the parent
submodel and its location is the joint offset. The submodels are rigid: only
the points, the polygons and the UV of an improved model go back to the
game. Do not move, add or delete the objects.
"""
import hashlib
import json
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "gamedata", "models"))
MAX_FACES = 1500
MAX_POLY_VERTS = 20
BG = (0.35, 0.0, 0.35)          # holes show this color in a sheet


# ---- math ---------------------------------------------------------------------

def cv(v):
    return Vector((-v[0], -v[2], v[1]))


def angles_matrix(a):
    """vm_angles_2_matrix of the game, as a Blender space rotation."""
    p, b, h = [x * 2.0 * math.pi / 65536.0 for x in a]
    sp, cp = math.sin(p), math.cos(p)
    sb, cb = math.sin(b), math.cos(b)
    sh, ch = math.sin(h), math.cos(h)
    sbsh, cbch, cbsh, sbch = sb * sh, cb * ch, cb * sh, sb * ch
    r = (cbch + sp * sbsh, sb * cp, sp * sbch - cbsh)
    u = (sp * cbsh - sbch, cb * cp, sbsh + sp * cbch)
    f = (sh * cp, -sp, ch * cp)
    m = Matrix(((r[0], u[0], f[0]), (r[1], u[1], f[1]), (r[2], u[2], f[2])))
    c = Matrix(((-1, 0, 0), (0, 0, -1), (0, 1, 0)))
    return c @ m @ c.transposed()


# ---- files --------------------------------------------------------------------

def sub_hash(sub):
    return hashlib.sha1(json.dumps([sub["points"], sub["polys"]]).encode()).hexdigest()[:16]


def clear_file():
    for o in list(bpy.data.objects):
        bpy.data.objects.remove(o, do_unlink=True)
    for c in list(bpy.data.collections):
        bpy.data.collections.remove(c)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.cameras,
                  bpy.data.libraries):
        for d in list(block):
            block.remove(d)


def material(rec, t, color=None):
    """The material of texture number t (None: a flat color of the palette)."""
    names = rec.get("textures") or []
    tex = names[t] if t is not None and t < len(names) else None
    name = "tex:%s" % tex if tex else ("tex#%s" % t)
    if t is None:
        name = "flat:%d" % (color or 0)
    if tex and rec.get("tex_dir"):
        name = "tex:%s/%s" % (rec["tex_dir"], tex)
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    mat = bpy.data.materials.new(name)
    mat.use_backface_culling = True
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    bsdf.inputs["Roughness"].default_value = 1.0
    path = None
    if tex and rec.get("tex_dir"):
        path = os.path.join(DIR, "tex", rec["tex_dir"], tex.replace("#", "_") + ".png")
    if path and os.path.exists(path):
        img = bpy.data.images.load(path, check_existing=True)
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = img
        node.interpolation = "Closest"
        nt.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])
        nt.links.new(node.outputs["Alpha"], bsdf.inputs["Alpha"])
        nt.nodes.active = node
    else:
        k = (hash(name) % 97) / 97.0
        bsdf.inputs["Base Color"].default_value = (0.3 + 0.5 * k, 0.5, 0.8 - 0.5 * k, 1)
        mat.diffuse_color = (0.3 + 0.5 * k, 0.5, 0.8 - 0.5 * k, 1)
    return mat


def make_sub(rec, name, sub, coll):
    me = bpy.data.meshes.new(name)
    bm = bmesh.new()
    verts = [bm.verts.new(cv(p)) for p in sub["points"]]
    uvl = bm.loops.layers.uv.new("UV")
    slots = []
    for p in sub["polys"]:
        idx = list(reversed(p["v"]))
        uvs = list(reversed(p["uv"])) if p["uv"] else None
        if len(set(idx)) < 3:
            continue
        try:
            f = bm.faces.new([verts[i] for i in idx])
        except ValueError:               # a second face on the same points
            dup = [bm.verts.new(verts[i].co) for i in idx]
            f = bm.faces.new(dup)
        mat = material(rec, p["t"], p.get("c"))
        if mat not in slots:
            slots.append(mat)
        f.material_index = slots.index(mat)
        if uvs:
            for loop, uv in zip(f.loops, uvs):
                loop[uvl].uv = (uv[0], 1.0 - uv[1])
    bm.to_mesh(me)
    bm.free()
    for m in slots:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def build(rec, tag, x, subs=None):
    mesh = rec["mesh"]
    coll = bpy.data.collections.new("%s.%s" % (mesh, tag))
    top = bpy.data.collections.get(mesh)
    if top is None:
        top = bpy.data.collections.new(mesh)
        bpy.context.scene.collection.children.link(top)
    top.children.link(coll)
    root = bpy.data.objects.new("%s.%s" % (mesh, tag), None)
    root.empty_display_size = 0.5
    root.location = (x, 0, 0)
    coll.objects.link(root)
    obs = []
    for s, sub in enumerate(subs or rec["subs"]):
        ob = make_sub(rec, "%s.%s.s%02d" % (mesh, tag, s), sub, coll)
        ob["sub"] = s
        ob["flags"] = sub["flags"]
        ob["done"] = sub.get("done", 0)
        obs.append(ob)
    for s, sub in enumerate(rec["subs"]):
        obs[s].parent = obs[sub["parent"]] if sub["parent"] >= 0 else root
        obs[s].location = cv(sub["offset"])
    return root, obs


def objs(mesh, tag="new"):
    rec_n = len([o for o in bpy.data.objects if o.name.startswith("%s.%s.s" % (mesh, tag))])
    return [bpy.data.objects["%s.%s.s%02d" % (mesh, tag, s)] for s in range(rec_n)]


def pose(mesh, frame=0):
    """Set the joint angles of a frame (-1: the modeling pose) on both copies."""
    rec = read_orig(mesh)
    n = len(rec["subs"])
    for tag in ("orig", "new"):
        for s, ob in enumerate(objs(mesh, tag)):
            a = (0, 0, 0) if frame < 0 else rec["angles"][3 * (frame * n + s):3 * (frame * n + s) + 3]
            m = angles_matrix(a).to_4x4()
            m.translation = cv(rec["subs"][s]["offset"])
            ob.matrix_basis = m
    bpy.context.view_layer.update()


def radius(mesh, tag="orig"):
    root = bpy.data.objects["%s.%s" % (mesh, tag)]
    inv = root.matrix_world.inverted()
    r = 0.0
    lo, hi = 1e9, -1e9
    for ob in objs(mesh, tag):
        mw = inv @ ob.matrix_world
        for v in ob.data.vertices:
            p = mw @ v.co
            r = max(r, math.hypot(p.x, p.y))
            lo, hi = min(lo, p.z), max(hi, p.z)
    return r, lo, hi


SKIN = {}                 # mesh -> model name whose textures the file shows


def read_orig(mesh):
    with open(os.path.join(DIR, "orig", mesh + ".json")) as f:
        rec = json.load(f)
    skin = SKIN.get(mesh)
    if skin:
        rec["tex_dir"], rec["textures"] = rec["skins"][skin]
    return rec


def begin(mesh, frame=0, skin=None):
    """Clear the file, then make the original and the copy to improve.
    An improved submodel of the library (new_subs/) replaces its copy.
    skin: the model name (with .pof) whose textures the file shows."""
    clear_file()
    SKIN.pop(mesh, None)
    if skin:
        SKIN[mesh] = skin
    rec = read_orig(mesh)
    build(rec, "orig", 0.0)
    pose_tmp = rec["subs"]
    lib = os.path.join(DIR, "new_subs")
    subs = []
    reused = 0
    for sub in pose_tmp:
        f = os.path.join(lib, sub_hash(sub) + ".json")
        if sub["polys"] and os.path.exists(f):
            with open(f) as fh:
                d = json.load(fh)
            sub = dict(sub, points=d["points"], polys=d["polys"], done=1)
            reused += 1
        subs.append(sub)
    bpy.context.view_layer.update()
    build(rec, "new", 0.0, subs)
    pose(mesh, frame)
    r, lo, hi = radius(mesh)
    bpy.data.objects[mesh + ".new"].location.x = round(1.5 * r + 0.5, 1)
    bpy.context.view_layer.update()
    return {"subs": len(rec["subs"]), "reused": reused, "stats": stats(mesh)}


# ---- checks -------------------------------------------------------------------

def stats(mesh, tag="new", detail=False):
    faces = tris = open_edges = bad = 0
    per = []
    for ob in objs(mesh, tag):
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        f = len(bm.faces)
        t = sum(len(x.verts) - 2 for x in bm.faces)
        # an open edge with a twin at the same place is the rim of a
        # two-sided sheet, not a hole
        rim = {}
        for e in bm.edges:
            if len(e.link_faces) == 1:
                k = frozenset((round(v.co.x, 3), round(v.co.y, 3), round(v.co.z, 3)) for v in e.verts)
                rim[k] = rim.get(k, 0) + 1
        oe = sum(1 for n in rim.values() if n == 1)
        nm = sum(1 for e in bm.edges if len(e.link_faces) > 2)
        faces += f
        tris += t
        open_edges += oe
        bad += nm
        if detail and f:
            per.append((ob["sub"], f, t, oe, nm))
        bm.free()
    out = {"faces": faces, "tris": tris, "open_edges": open_edges, "edges_3_faces": bad}
    if detail:
        out["subs"] = per
    return out


# ---- operations on one object --------------------------------------------------

def _edit(ob, fn):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    r = fn(bm)
    bm.normal_update()
    bm.to_mesh(ob.data)
    bm.free()
    ob.data.update()
    return r


def weld(ob, dist=1e-4):
    """Merge the points at one place."""
    return _edit(ob, lambda bm: bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=dist))


def quads(ob, angle=12.0):
    """Join pairs of triangles of one texture into flat quads."""
    def fn(bm):
        before = len(bm.faces)
        a = math.radians(angle)
        bmesh.ops.join_triangles(bm, faces=[f for f in bm.faces if len(f.verts) == 3],
                                 cmp_uvs=True, cmp_materials=True,
                                 angle_face_threshold=a, angle_shape_threshold=math.radians(40))
        return before - len(bm.faces)
    return _edit(ob, fn)


def _boundary_loops(bm):
    edges = {e for e in bm.edges if len(e.link_faces) == 1}
    loops = []
    while edges:
        e = edges.pop()
        loop = [e]
        v0, v = e.verts
        while v != v0:
            nxt = [x for x in v.link_edges if x in edges]
            if not nxt:
                loop = None
                break
            e = nxt[0]
            edges.discard(e)
            loop.append(e)
            v = e.other_vert(v)
        if loop:
            loops.append(loop)
    return loops


def _rim_cycles(bm):
    """The open edge loops as lists of face corners, in the direction of
    their faces. The walk turns around each point through the faces, so two
    loops that meet at one point stay two loops."""
    rim = [l for f in bm.faces for l in f.loops if len(l.edge.link_faces) == 1]
    left = set(rim)
    out = []
    while left:
        l = left.pop()
        cyc = [l]
        ok = True
        cur = l
        for _ in range(100000):
            n = cur.link_loop_next
            while len(n.edge.link_faces) == 2:
                n = n.link_loop_radial_next.link_loop_next
            if len(n.edge.link_faces) != 1:
                ok = False
                break
            if n is l:
                break
            if n not in left:
                ok = False
                break
            left.discard(n)
            cyc.append(n)
            cur = n
        if not ok:
            continue
        # a loop that goes through one point twice is two loops
        work = [cyc]
        while work:
            c = work.pop()
            seen = {}
            cut = None
            for i, x in enumerate(c):
                if x.vert in seen:
                    cut = (seen[x.vert], i)
                    break
                seen[x.vert] = i
            if cut:
                work.append(c[cut[0]:cut[1]])
                work.append(c[:cut[0]] + c[cut[1]:])
            else:
                out.append(c)
    return out


def cap(ob, dome=0.0, max_edges=64):
    """Close each open edge loop with a face (dome 0) or with a ring and a
    face (dome > 0: the ring goes out by dome times the loop radius and has
    0.6 of its size). The new faces get the texture and the UV of the faces
    at the loop, so no texture changes."""
    def fn(bm):
        uvl = bm.loops.layers.uv.verify()
        made = 0
        for cyc in _rim_cycles(bm):
            if len(cyc) < 3 or len(cyc) > max_edges:
                continue
            # against the direction of the faces at the rim: the cap then
            # faces the same way as they do
            cyc = list(reversed(cyc))
            verts = [l.vert for l in cyc]
            if len(set(verts)) != len(verts):
                continue
            uvs = [l[uvl].uv.copy() for l in cyc]
            mats = [l.face.material_index for l in cyc]
            mat = max(set(mats), key=mats.count)
            center = sum((x.co for x in verts), Vector()) / len(verts)
            if dome > 0:
                n = Vector()
                for i in range(len(verts)):
                    n += (verts[i].co - center).cross(verts[(i + 1) % len(verts)].co - center)
                rad = sum((x.co - center).length for x in verts) / len(verts)
                if n.length > 1e-9:
                    n.normalize()
                    ring = [bm.verts.new(center + (x.co - center) * 0.6 + n * dome * rad) for x in verts]
                    uvc = sum(uvs, Vector((0, 0))) / len(uvs)
                    ruv = [uvc + (u - uvc) * 0.6 for u in uvs]
                    for i in range(len(verts)):
                        j = (i + 1) % len(verts)
                        try:
                            f2 = bm.faces.new((verts[i], verts[j], ring[j], ring[i]))
                        except ValueError:
                            continue
                        f2.material_index = mat
                        for lp, uv in zip(f2.loops, (uvs[i], uvs[j], ruv[j], ruv[i])):
                            lp[uvl].uv = uv
                        made += 1
                    verts, uvs = ring, ruv
            try:
                f2 = bm.faces.new(verts)
            except ValueError:
                continue
            f2.material_index = mat
            for lp, uv in zip(f2.loops, uvs):
                lp[uvl].uv = uv
            made += 1
        return made
    return _edit(ob, fn)


def fix_normals(ob):
    """Make the faces of a closed submodel face outward."""
    return _edit(ob, lambda bm: bmesh.ops.recalc_face_normals(bm, faces=bm.faces))


def _rims(me):
    """(center, low corner, high corner) of the box of each open edge loop."""
    bm = bmesh.new()
    bm.from_mesh(me)
    out = []
    for loop in _boundary_loops(bm):
        c = np.array([[v.co.x, v.co.y, v.co.z] for e in loop for v in e.verts])
        out.append(((c.min(0) + c.max(0)) / 2, c.min(0), c.max(0)))
    bm.free()
    return out


def round_sub(ob, amount=1.0, keep_size=True, crease_open=True):
    """One step of subdivision: each quad gives 4 quads, each triangle 3.
    amount 1 is the Catmull-Clark shape, 0 keeps the flat faces. keep_size
    scales the result back to the box of the original. The UV stays linear
    in each old face, so the texture does not move."""
    me = ob.data
    if not me.polygons:
        return 0
    lo0 = np.array([min(v.co[i] for v in me.vertices) for i in range(3)])
    hi0 = np.array([max(v.co[i] for v in me.vertices) for i in range(3)])
    rims0 = _rims(me)

    def subdivided(kind):
        mod = ob.modifiers.new("zoo", "SUBSURF")
        mod.levels = 1
        mod.subdivision_type = kind
        mod.uv_smooth = "NONE"
        mod.boundary_smooth = "PRESERVE_CORNERS"
        dg = bpy.context.evaluated_depsgraph_get()
        new = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), depsgraph=dg)
        ob.modifiers.remove(mod)
        return new

    smooth = subdivided("CATMULL_CLARK")
    if amount < 1.0:
        flat = subdivided("SIMPLE")
        if len(flat.vertices) == len(smooth.vertices):
            for a, b in zip(smooth.vertices, flat.vertices):
                a.co = b.co.lerp(a.co, amount)
        bpy.data.meshes.remove(flat)
    if keep_size:
        co = np.array([v.co[:] for v in smooth.vertices])
        lo1, hi1 = co.min(0), co.max(0)
        size1 = np.where(hi1 - lo1 > 1e-6, hi1 - lo1, 1.0)
        scale = np.where(hi1 - lo1 > 1e-6, (hi0 - lo0) / size1, 1.0)
        co = (co - (lo1 + hi1) / 2) * scale + (lo0 + hi0) / 2
        for v, c in zip(smooth.vertices, co):
            v.co = c
        # An open rim joins the next submodel: it keeps the box of its old
        # rim (the box of the whole part does not hold a tapered tube).
        if rims0:
            bm = bmesh.new()
            bm.from_mesh(smooth)
            for loop in _boundary_loops(bm):
                vs = {v for e in loop for v in e.verts}
                c1 = np.array([[v.co.x, v.co.y, v.co.z] for v in vs])
                lo, hi = c1.min(0), c1.max(0)
                mid = (lo + hi) / 2
                old = min(rims0, key=lambda r: float(np.sum((r[0] - mid) ** 2)))
                size = np.where(hi - lo > 1e-6, hi - lo, 1.0)
                k = np.where(hi - lo > 1e-6, (old[2] - old[1]) / size, 1.0)
                for v in vs:
                    q = (np.array(v.co) - mid) * k + old[0]
                    v.co = q
            bm.to_mesh(smooth)
            bm.free()
    name = me.name
    for p in smooth.polygons:
        p.use_smooth = False
    ob.data = smooth
    bpy.data.meshes.remove(me)
    smooth.name = name
    return len(smooth.polygons)


def _components(bm):
    seen = set()
    out = []
    for f in bm.faces:
        if f in seen:
            continue
        comp = []
        stack = [f]
        seen.add(f)
        while stack:
            x = stack.pop()
            comp.append(x)
            for e in x.edges:
                for y in e.link_faces:
                    if y not in seen:
                        seen.add(y)
                        stack.append(y)
        out.append(comp)
    return out


def parts(ob):
    """The connected parts of an object: (faces, closed, volume, size)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    out = []
    for comp in _components(bm):
        closed = all(len(e.link_faces) == 2 for f in comp for e in f.edges)
        vol = 0.0
        if closed:
            for f in comp:
                vs = [v.co for v in f.verts]
                for i in range(1, len(vs) - 1):
                    vol += vs[0].dot(vs[i].cross(vs[i + 1])) / 6.0
        co = [v.co for f in comp for v in f.verts]
        size = [max(c[i] for c in co) - min(c[i] for c in co) for i in range(3)]
        out.append((len(comp), closed, vol, size))
    bm.free()
    return out


def _compactness(me):
    """36 pi V^2 / A^3 of a mesh with its holes filled: 1 for a ball, 0.5 for
    a cube, near 0 for a sheet."""
    bm = bmesh.new()
    bm.from_mesh(me)
    edges = [e for e in bm.edges if len(e.link_faces) == 1]
    if edges:
        bmesh.ops.holes_fill(bm, edges=edges, sides=0)
    if any(len(e.link_faces) != 2 for e in bm.edges):
        bm.free()
        return 0.0
    bmesh.ops.triangulate(bm, faces=bm.faces)
    vol = sum(f.verts[0].co.dot(f.verts[1].co.cross(f.verts[2].co)) for f in bm.faces) / 6.0
    area = sum(f.calc_area() for f in bm.faces)
    bm.free()
    return 36 * math.pi * vol * vol / (area ** 3) if area > 1e-9 else 0.0


def _double_side(me):
    """Give each face of a sheet a second face that looks the other way,
    if it has none: a sheet then shows from both sides with culling."""
    bm = bmesh.new()
    bm.from_mesh(me)
    uvl = bm.loops.layers.uv.verify()

    def key(f):
        return frozenset((round(v.co.x, 3), round(v.co.y, 3), round(v.co.z, 3)) for v in f.verts)
    have = {}
    for f in bm.faces:
        have.setdefault(key(f), []).append(f)
    made = 0
    for f in list(bm.faces):
        twins = have[key(f)]
        if any(t is not f and t.normal.dot(f.normal) < 0 for t in twins):
            continue
        loops = list(reversed(f.loops))
        f2 = bm.faces.new([bm.verts.new(l.vert.co) for l in loops])
        f2.material_index = f.material_index
        for l2, l in zip(f2.loops, loops):
            l2[uvl].uv = l[uvl].uv
        made += 1
    bm.normal_update()
    bm.to_mesh(me)
    bm.free()
    return made


SOLID = 0.05              # compactness above which a part is a body, not a sheet


def round_parts(ob, amount=1.0, dome=0.0, do_round=True):
    """Work on each connected part of an object. A body (a part that holds a
    volume when its holes are closed) gets round_sub() and then cap(). A
    sheet (a blade, a wing, a bow) keeps its shape and gets back faces."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    comps = _components(bm)
    pieces = []
    log = {"bodies": 0, "rounded": 0, "sheets": 0, "back_faces": 0}
    for comp in comps:
        tmp_me = bpy.data.meshes.new("zoo_tmp")
        b3 = bm.copy()
        b3.faces.ensure_lookup_table()
        keep = {f.index for f in comp}
        bmesh.ops.delete(b3, geom=[f for f in b3.faces if f.index not in keep], context="FACES")
        b3.to_mesh(tmp_me)
        manifold = all(len(e.link_faces) <= 2 for e in b3.edges)
        b3.free()
        for m in ob.data.materials:
            tmp_me.materials.append(m)
        if manifold and _compactness(tmp_me) > SOLID:
            tmp = bpy.data.objects.new("zoo_tmp", tmp_me)
            bpy.context.scene.collection.objects.link(tmp)
            if do_round and len(comp) >= 4:
                round_sub(tmp, amount)
                log["rounded"] += 1
            cap(tmp, dome=dome)
            tmp_me = tmp.data
            bpy.data.objects.remove(tmp, do_unlink=True)
            log["bodies"] += 1
        else:
            log["sheets"] += 1
            log["back_faces"] += _double_side(tmp_me)
        pieces.append(tmp_me)
    bm.free()
    out = bmesh.new()
    for me in pieces:
        out.from_mesh(me)
        bpy.data.meshes.remove(me)
    out.to_mesh(ob.data)
    out.free()
    ob.data.update()
    return log


def auto(mesh, amount=1.0, dome=0.0, skip=(), no_round=(), limit=MAX_FACES, subdivide=False):
    """The standard steps for each submodel of the improved copy: weld the
    points, join triangles into quads, then round_parts(), which closes the
    holes of the bodies and gives back faces to the sheets. The points of
    the original stay: the silhouette does not change.
    subdivide (off: the owner of the project did not want it) adds one
    subdivision step, the largest submodels first, while the face limit
    holds. skip: submodels to leave alone. no_round: submodels that get no
    subdivision."""
    obs = [ob for ob in objs(mesh) if ob.data.polygons and ob["sub"] not in skip and not ob["done"]]
    for ob in obs:
        weld(ob)
        quads(ob)
    total = stats(mesh)["faces"]
    order = []
    for ob in obs:
        size = [max(v.co[i] for v in ob.data.vertices) - min(v.co[i] for v in ob.data.vertices) for i in range(3)]
        order.append((size[0] * size[1] * size[2], ob))
    order.sort(key=lambda t: -t[0])
    # every submodel will get at least its caps: keep room for them
    reserve = {ob.name: 2 for v, ob in order}
    room = sum(reserve.values())
    rounded = []
    log = {"bodies": 0, "rounded": 0, "sheets": 0, "back_faces": 0}
    for vol, ob in order:
        before = len(ob.data.polygons)
        room -= reserve[ob.name]
        saved = ob.data.copy()
        tries = [(True, dome), (True, 0.0), (False, dome), (False, 0.0)]
        if ob["sub"] in no_round or not subdivide:
            tries = tries[2:]
        for do_round, d in tries:
            r = round_parts(ob, amount, d, do_round)
            after = len(ob.data.polygons)
            if total - before + after + room <= limit:
                break
            old = ob.data
            ob.data = saved.copy()
            bpy.data.meshes.remove(old)
        bpy.data.meshes.remove(saved)
        total += after - before
        for k in log:
            log[k] += r[k]
        if do_round and r["rounded"]:
            rounded.append(ob["sub"])
    for ob in obs:
        for p in ob.data.polygons:
            p.use_smooth = False
    log.update({"rounded_subs": rounded, "not_rounded": [ob["sub"] for ob in obs if ob["sub"] not in rounded],
                "stats": stats(mesh)})
    return log


def swap_strips(ob, a, b, pick):
    """Exchange two rectangles of a texture between two groups of faces.
    a and b are (u0, u1, v0, v1). pick(face normal) says "a", "b" or None:
    a face of group "a" gets its UV moved from rectangle a to rectangle b,
    and the other way for "b"."""
    def fn(bm):
        uvl = bm.loops.layers.uv.verify()
        n = 0
        for f in bm.faces:
            g = pick(f.normal)
            if g is None:
                continue
            src, dst = (a, b) if g == "a" else (b, a)
            us = [l[uvl].uv for l in f.loops]
            if min(u.y for u in us) < src[2] - 0.03 or max(u.y for u in us) > src[3] + 0.03:
                continue
            for l in f.loops:
                u = l[uvl].uv
                l[uvl].uv = (dst[0] + (u.x - src[0]) / (src[1] - src[0]) * (dst[1] - dst[0]),
                             dst[2] + (u.y - src[2]) / (src[3] - src[2]) * (dst[3] - dst[2]))
            n += 1
        return n
    return _edit(ob, fn)


def loop_cut(ob, axis, cuts=1, smooth=0.0):
    """Cut the edges that run along an axis (0, 1, 2 of the object) with new
    loops. smooth > 0 moves the new points out to a round shape."""
    def fn(bm):
        ax = Vector((0, 0, 0))
        ax[axis] = 1.0
        edges = [e for e in bm.edges
                 if abs((e.verts[1].co - e.verts[0].co).normalized().dot(ax)) > 0.75]
        r = bmesh.ops.subdivide_edges(bm, edges=edges, cuts=cuts, smooth=smooth,
                                      use_grid_fill=True, use_only_quads=False)
        return len(edges)
    return _edit(ob, fn)


# ---- pictures -----------------------------------------------------------------

def _camera():
    cam = bpy.data.objects.get("zoo_cam")
    if cam is None:
        cam = bpy.data.objects.new("zoo_cam", bpy.data.cameras.new("zoo_cam"))
        bpy.context.scene.collection.objects.link(cam)
    cam.data.type = "ORTHO"
    bpy.context.scene.camera = cam
    return cam


def _render_setup(w, h):
    sc = bpy.context.scene
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light = "STUDIO"
    sh.color_type = "TEXTURE"
    sh.show_backface_culling = True
    sh.show_shadows = False
    sh.show_cavity = False
    sh.show_specular_highlight = False
    sc.display.render_aa = "5"
    sc.render.resolution_x = w
    sc.render.resolution_y = h
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = False
    sc.render.image_settings.file_format = "PNG"
    if sc.world is None:
        sc.world = bpy.data.worlds.new("zoo")
    sc.world.use_nodes = False
    sc.world.color = BG
    sc.view_settings.view_transform = "Standard"
    sc.view_settings.exposure = 1.2


def _shot(path):
    bpy.context.scene.render.filepath = path
    bpy.ops.render.render(write_still=True)
    img = bpy.data.images.load(path)
    w, h = img.size
    a = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(img)
    return a


def _save(a, path):
    h, w = a.shape[:2]
    img = bpy.data.images.new("zoo_sheet", w, h, alpha=True)
    img.pixels = a.ravel()
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    bpy.data.images.remove(img)


def sheet(mesh, path=None, frame=None, angles=(0, 90, 180, 270), cell=(640, 520), wire=False):
    """A picture of the original (left of each cell) and the improved copy
    (right), seen from the angles, with back faces culled."""
    if frame is not None:
        pose(mesh, frame)
    path = path or os.path.join(DIR, "sheets", mesh + ".png")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    ro = bpy.data.objects[mesh + ".orig"]
    rn = bpy.data.objects[mesh + ".new"]
    r, lo, hi = radius(mesh)
    cam = _camera()
    _render_setup(*cell)
    span = rn.location.x - ro.location.x
    width = span + 1.7 * r
    height = (hi - lo) * 1.04
    cam.data.ortho_scale = max(width, height * cell[0] / cell[1])
    cam.data.clip_end = 100000
    cx = (ro.location.x + rn.location.x) / 2
    cam.location = (cx, -(20 * r + 50), (lo + hi) / 2)
    cam.rotation_euler = (math.radians(90), 0, 0)
    shots = []
    tmp = os.path.join(DIR, "sheets", "_tmp.png")
    for a in angles:
        ro.rotation_euler.z = rn.rotation_euler.z = math.radians(a)
        bpy.context.view_layer.update()
        shots.append(_shot(tmp))
    ro.rotation_euler.z = rn.rotation_euler.z = 0
    cols = 2 if len(shots) > 1 else 1
    rows = []
    for i in range(0, len(shots), cols):
        row = shots[i:i + cols]
        while len(row) < cols:
            row.append(np.zeros_like(shots[0]))
        rows.append(np.concatenate(row, axis=1))
    _save(np.concatenate(list(reversed(rows)), axis=0), path)
    return path


def closeup(mesh, subs, path=None, angles=(0, 90, 180, 270), cell=(640, 520), pad=1.25):
    """As sheet(), but the view holds only the named submodels of both copies."""
    path = path or os.path.join(DIR, "sheets", mesh + "_close.png")
    ro = bpy.data.objects[mesh + ".orig"]
    rn = bpy.data.objects[mesh + ".new"]
    cam = _camera()
    _render_setup(cell[0] // 2, cell[1])
    shots = []
    tmp = os.path.join(DIR, "sheets", "_tmp.png")
    for a in angles:
        row = []
        for tag, root in (("orig", ro), ("new", rn)):
            root.rotation_euler.z = math.radians(a)
            bpy.context.view_layer.update()
            pts = []
            for s in subs:
                ob = bpy.data.objects["%s.%s.s%02d" % (mesh, tag, s)]
                pts += [ob.matrix_world @ v.co for v in ob.data.vertices]
            lo = Vector((min(p[i] for p in pts) for i in range(3)))
            hi = Vector((max(p[i] for p in pts) for i in range(3)))
            c = (lo + hi) / 2
            cam.data.ortho_scale = max(hi.x - lo.x, (hi.z - lo.z) * cell[0] / 2 / cell[1], 0.1) * pad
            cam.location = (c.x, lo.y - 500, c.z)
            cam.rotation_euler = (math.radians(90), 0, 0)
            row.append(_shot(tmp))
            root.rotation_euler.z = 0
        shots.append(np.concatenate(row, axis=1))
    rows = [np.concatenate(shots[i:i + 2], axis=1) for i in range(0, len(shots), 2)]
    _save(np.concatenate(list(reversed(rows)), axis=0), path)
    return path


# ---- output -------------------------------------------------------------------

def _planar_convex(pts, tol=0.035):
    n = Vector()
    c = sum(pts, Vector()) / len(pts)
    for i in range(len(pts)):
        n += (pts[i] - c).cross(pts[(i + 1) % len(pts)] - c)
    if n.length < 1e-12:
        return False
    n.normalize()
    size = max((p - c).length for p in pts)
    if max(abs((p - c).dot(n)) for p in pts) > tol * size:
        return False
    for i in range(len(pts)):
        a, b, d = pts[i], pts[(i + 1) % len(pts)], pts[(i + 2) % len(pts)]
        if (b - a).cross(d - b).dot(n) < -1e-7:
            return False
    return True


def _uv_affine(f, uvl, tol=0.004):
    """True if the UV of a face are an affine function of its points (0.004
    is a quarter of a texel of a 64 pixel texture)."""
    ls = list(f.loops)
    o = ls[0].vert.co
    n = f.normal
    ex = None
    for l in ls[1:]:
        d = l.vert.co - o
        d -= n * d.dot(n)
        if d.length > 1e-9:
            if ex is None:
                ex = d.normalized()
            elif abs(ex.cross(d).dot(n)) > 1e-9 * max(1.0, d.length):
                break
    if ex is None:
        return True
    ey = n.cross(ex)
    p = [((l.vert.co - o).dot(ex), (l.vert.co - o).dot(ey)) for l in ls]
    u = [l[uvl].uv for l in ls]
    # the map of the first three points that are not on one line
    for i in range(1, len(ls) - 1):
        (ax, ay), (bx, by) = p[i], p[i + 1]
        det = ax * by - ay * bx
        if abs(det) > 1e-12:
            break
    else:
        return True
    du, dv = u[i] - u[0], u[i + 1] - u[0]
    for (x, y), w in zip(p, u):
        s = (x * by - y * bx) / det
        t = (ax * y - ay * x) / det
        if (u[0] + du * s + dv * t - w).length > tol:
            return False
    return True


def _fan_starts(bm, uvl):
    """For each face with more than 3 points: the loop where the fan of the
    game must start to give the triangles of Blender, None if each start is
    good (affine UV), False if no start gives them."""
    tris = {}
    for t in bm.calc_loop_triangles():
        tris.setdefault(t[0].face, []).append(t)
    out = {}
    for f in bm.faces:
        if len(f.verts) < 4:
            continue
        if _uv_affine(f, uvl):
            out[f] = None
            continue
        common = None
        for t in tris.get(f, []):
            s = set(t)
            common = s if common is None else common & s
        # a quad has two such loops: take the first one of the face, so that
        # each export gives the same file (a set has no fixed order)
        ls = list(f.loops)
        out[f] = min(common, key=ls.index) if common else False
    return out


def _corner_key(p, uv):
    return (round(p[0], 3), round(p[1], 3), round(p[2], 3), round(uv[0], 3), round(uv[1], 3))


def _orig_starts(sub):
    """The polygons of an original submodel with more than 3 points: the set
    of its corners (points and UV, in game form) -> its first corner."""
    out = {}
    for p in (sub or {}).get("polys", []):
        if len(p["v"]) > 3 and p.get("uv"):
            keys = [_corner_key(sub["points"][v], uv) for v, uv in zip(p["v"], p["uv"])]
            out[frozenset(keys)] = keys[0]
    return out


def export_sub(ob, rec, orig=None):
    """The points and the polygons of an object, in the form of the game.
    A face that is not flat or not convex goes out as triangles."""
    names = rec.get("textures") or []
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    uvl = bm.loops.layers.uv.verify()
    bad = [f for f in bm.faces if len(f.verts) > 3 and
           (len(f.verts) > MAX_POLY_VERTS or not _planar_convex([v.co.copy() for v in f.verts]))]
    if bad:
        bmesh.ops.triangulate(bm, faces=bad, quad_method='FIXED')
    # The game draws a polygon as a fan of triangles from its first point.
    # Blender draws the triangles of calc_loop_triangles. The two pictures
    # are equal only if the UV are affine on the face, or if the fan starts
    # at the point that all the triangles of Blender share.
    # A face that is equal to a polygon of the original (orig: the record
    # of the submodel) keeps the first point of that polygon instead, so it
    # draws as in the game.
    starts = _orig_starts(orig)

    def game_corner(l):
        c, u = l.vert.co, l[uvl].uv
        return _corner_key((-c.x, c.z, -c.y), (u.x, 1.0 - u.y))

    def kept(f):
        if not starts or len(f.verts) < 4:
            return None
        keys = [game_corner(l) for l in f.loops]
        k = starts.get(frozenset(keys))
        return f.loops[keys.index(k)] if k is not None else None

    first = _fan_starts(bm, uvl)
    split = [f for f in bm.faces if first.get(f) is False and kept(f) is None]
    if split:
        bmesh.ops.triangulate(bm, faces=split, quad_method='FIXED')
        first = _fan_starts(bm, uvl)
    for f in bm.faces:
        k = kept(f)
        if k is not None:
            first[f] = k
    bm.verts.index_update()
    bm.normal_update()
    points = [[-v.co.x, v.co.z, -v.co.y] for v in bm.verts]
    polys = []
    for f in bm.faces:
        if f.calc_area() < 1e-10:
            continue
        mat = ob.data.materials[f.material_index].name
        if len(mat) > 4 and mat[-4] == "." and mat[-3:].isdigit():
            mat = mat[:-4]                  # a copy that an append made
        if mat.startswith("tex:"):
            t = names.index(mat[4:].split("/")[-1])
        elif mat.startswith("tex#"):
            t = int(mat[4:])
        else:
            t = None
        loops = list(reversed(f.loops))
        if first.get(f) is not None:
            k = loops.index(first[f])
            loops = loops[k:] + loops[:k]
        n = f.normal
        polys.append({"v": [l.vert.index for l in loops], "t": t, "c": None if t is not None else int(mat[5:]),
                      "uv": [[l[uvl].uv.x, 1.0 - l[uvl].uv.y] for l in loops],
                      "n": [-n.x, n.z, -n.y]})
    bm.free()
    return {"points": points, "polys": polys}


def _save_blend(path, copy):
    """Save without the save handlers of other add-ons (they save again)."""
    h = bpy.app.handlers
    kept = (list(h.save_pre), list(h.save_post))
    h.save_pre.clear()
    h.save_post.clear()
    try:
        bpy.ops.wm.save_as_mainfile(filepath=path, copy=copy)
    finally:
        h.save_pre.extend(kept[0])
        h.save_post.extend(kept[1])


def finish(mesh, save=True, library=True):
    """Check the improved copy, write new/<mesh>.json and blend/<mesh>.blend."""
    rec = read_orig(mesh)
    st = stats(mesh, "new", detail=True)
    problems = []
    if st["faces"] > MAX_FACES:
        problems.append("%d faces" % st["faces"])
    new = objs(mesh, "new")
    if len(new) != len(rec["subs"]):
        problems.append("the number of submodels changed")
    out = {"mesh": mesh, "hash": rec["hash"], "subs": []}
    lib = os.path.join(DIR, "new_subs")
    os.makedirs(lib, exist_ok=True)
    os.makedirs(os.path.join(DIR, "new"), exist_ok=True)
    os.makedirs(os.path.join(DIR, "blend"), exist_ok=True)
    for s, ob in enumerate(new):
        d = export_sub(ob, rec, rec["subs"][s])
        if d["polys"] and not rec["subs"][s]["polys"]:
            problems.append("submodel %d: polygons in a part that was empty" % s)
        if not d["polys"] and rec["subs"][s]["polys"] and not rigid_child(mesh, s):
            problems.append("submodel %d: the polygons are gone" % s)
        if len(d["points"]) > 900:
            problems.append("submodel %d: %d points" % (s, len(d["points"])))
        out["subs"].append(d)
        if library and d["polys"]:
            with open(os.path.join(lib, sub_hash(rec["subs"][s]) + ".json"), "w") as f:
                json.dump(d, f)
    with open(os.path.join(DIR, "new", mesh + ".json"), "w") as f:
        json.dump(out, f)
    if save:
        pose(mesh, 0)
        _save_blend(os.path.join(DIR, "blend", mesh + ".blend"), copy=True)
    st.pop("subs", None)
    return {"stats": st, "orig": stats(mesh, "orig"), "problems": problems}


def build_zoo(columns=12):
    """zoo.blend: one linked collection for each model file, in a grid."""
    clear_file()
    files = sorted(f for f in os.listdir(os.path.join(DIR, "blend")) if f.endswith(".blend"))
    x = y = 0.0
    row_depth = 0.0
    for i, f in enumerate(files):
        mesh = f[:-6]
        path = os.path.join(DIR, "blend", f)
        with bpy.data.libraries.load(path, link=True, relative=True) as (src, dst):
            dst.collections = [c for c in src.collections if c == mesh]
        if not dst.collections or dst.collections[0] is None:
            continue
        coll = dst.collections[0]
        inst = bpy.data.objects.new(mesh, None)
        inst.instance_type = "COLLECTION"
        inst.instance_collection = coll
        bpy.context.scene.collection.objects.link(inst)
        size = max((max(o.location.x for o in coll.all_objects if o.parent is None) + 10), 20) * 1.6
        if i % columns == 0 and i:
            x = 0.0
            y -= row_depth + 20
            row_depth = 0.0
        inst.location = (x, y, 0)
        x += size
        row_depth = max(row_depth, 30.0)
    path = os.path.join(DIR, "zoo.blend")
    _save_blend(path, copy=False)
    return {"models": len(files), "file": path}


# ---- the list of the models ----------------------------------------------------

def _arm_strips(mesh, subs, a, b):
    for s in subs:
        ob = bpy.data.objects["%s.new.s%02d" % (mesh, s)]
        if not ob["done"]:
            swap_strips(ob, a, b, lambda n: "a" if abs(n.z) > 0.7 else ("b" if abs(n.y) > 0.7 else None))


def _drop_inner_twins(mesh, subs):
    """A part whose faces are all two-sided: delete the face of each pair
    that looks into the part, then close each hole of 4 edges with the two
    triangles that keep the part convex. Without this, weld() keeps one face
    of each pair, inner or outer by chance, and the hole stays open (cap()
    alone would fold it inward). Returns the number of new faces."""
    made = 0
    for s in subs:
        ob = bpy.data.objects["%s.new.s%02d" % (mesh, s)]
        if ob["done"]:
            continue

        def fn(bm):
            bm.normal_update()
            c = sum((v.co for v in bm.verts), Vector()) / len(bm.verts)
            pairs = {}
            for f in bm.faces:
                k = frozenset(tuple(round(x, 3) for x in v.co) for v in f.verts)
                pairs.setdefault(k, []).append(f)
            drop = [f for g in pairs.values() if len(g) == 2 for f in g
                    if f.normal.dot(f.calc_center_median() - c) < 0]
            bmesh.ops.delete(bm, geom=drop, context="FACES")
            bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-4)
            uvl = bm.loops.layers.uv.verify()
            # a hole goes against the corners of its faces: nxt[b] = a
            nxt, side = {}, {}
            for e in bm.edges:
                if len(e.link_faces) == 1:
                    f = e.link_faces[0]
                    for lp in f.loops:
                        if lp.edge is e:
                            nxt[lp.link_loop_next.vert] = lp.vert
                            side[lp.vert] = side[lp.link_loop_next.vert] = f
            n = 0
            while nxt:
                ring = [next(iter(nxt))]
                while nxt.get(ring[-1]) not in (None, ring[0]):
                    ring.append(nxt[ring[-1]])
                for v in ring:
                    nxt.pop(v, None)
                if len(ring) != 4:
                    continue
                for d in (0, 1):
                    q = ring[d:] + ring[:d]
                    tris = ((q[0], q[1], q[2]), (q[0], q[2], q[3]))
                    ok = True
                    for t in tris:
                        nv = (t[1].co - t[0].co).cross(t[2].co - t[1].co).normalized()
                        if any(nv.dot(v.co - t[0].co) > 1e-4 for v in bm.verts):
                            ok = False
                    if ok:
                        break
                if not ok:
                    continue
                for t in tris:
                    f = bm.faces.new(t)
                    f.material_index = side[t[0]].material_index
                    for lp in f.loops:
                        lp[uvl].uv = next(x[uvl].uv for x in lp.vert.link_loops if x.face is not f)
                    n += 1
            return n
        made += _edit(ob, fn)
    return made


# Changes of single models, made before auto(). mesh -> function(mesh).
FIXES = {
    # The feet of the ogre (submodels 5 and 25) have each face twice, one
    # for each side, and the back of the heel is open (bugs.md D30). The
    # inner faces go and two outward triangles close the heel.
    "ognew5": lambda m: _drop_inner_twins(m, (5, 25)),
    # The upper arm texture has two strips. The strip with the emblem of the
    # pauldron was on the front and the back of the arm. It goes to the outer
    # side, and the strip with the edge of the pauldron to the front and back.
    "hum_mfg": lambda m: _arm_strips(m, (17, 21), (0.04, 0.94, 1.58, 1.83), (0.0, 1.0, 1.08, 1.47)),
}
AUTO_ARGS = {}            # mesh -> keyword arguments of auto()


def _troll(mesh):
    """The pelvis of the troll is two half shells: submodel 1 (front) and
    submodel 18 (back, never turns against 1). Their top is open and has
    the shape of the open bottom of the chest (submodel 2), so each turn of
    the chest shows the inside. The two halves become one closed part in
    submodel 1, with a mound that goes up into the chest. The chest gets a
    small mound down."""
    pelvis = bpy.data.objects[mesh + ".new.s01"]
    chest = bpy.data.objects[mesh + ".new.s02"]
    old = chest.data
    name = old.name
    chest.data = bpy.data.objects[mesh + ".orig.s02"].data.copy()
    bpy.data.meshes.remove(old)
    chest.data.name = name
    weld(chest)
    merge_rigid(mesh, 18)
    weld(pelvis, 0.02)
    mound(pelvis, steps=((0.9, 0.35), (0.6, 0.7), (0.3, 0.85)))
    cap(chest, dome=0.3)
    quads(pelvis)
    quads(chest)


# Changes of single models, made after auto(). mesh -> function(mesh).
POST = {"troll": _troll}


def improve(mesh, picture=True):
    """All steps for one model: begin, the fix of the model, auto, finish."""
    info = begin(mesh)
    if mesh in FIXES:
        FIXES[mesh](mesh)
    a = auto(mesh, **AUTO_ARGS.get(mesh, {}))
    if mesh in POST:
        POST[mesh](mesh)
    r = finish(mesh)
    if picture:
        sheet(mesh, angles=(20, 200), cell=(520, 520))
    return {"mesh": mesh, "reused": info["reused"], "orig": r["orig"], "new": r["stats"],
            "not_rounded": a["not_rounded"], "sheets": a["sheets"], "back_faces": a["back_faces"],
            "problems": r["problems"]}


def meshes():
    with open(os.path.join(DIR, "index.json")) as f:
        return sorted(json.load(f))


def batch(names=None, fresh=True):
    """improve() for the named models (default: all). fresh empties the
    library of improved submodels first. Writes batch.json. Returns the
    models with a problem or with open edges."""
    import contextlib
    import io
    import shutil
    import traceback
    lib = os.path.join(DIR, "new_subs")
    if fresh and os.path.isdir(lib):
        shutil.rmtree(lib)
    out = {}
    order = list(names or meshes())
    for m in list(FIXES):                 # a model with a fix gives its submodels to the others
        if m in order:
            order.remove(m)
            order.insert(0, m)
    with contextlib.redirect_stdout(io.StringIO()):
        for m in order:
            try:
                out[m] = improve(m)
            except Exception:
                out[m] = {"error": traceback.format_exc()[-400:]}
    with open(os.path.join(DIR, "batch.json"), "w") as f:
        json.dump(out, f, indent=0)
    bad = {}
    for m, r in out.items():
        if "error" in r:
            bad[m] = r["error"]
        elif r["problems"] or r["new"]["open_edges"] or r["new"]["faces"] > MAX_FACES:
            bad[m] = [r["new"]["faces"], r["new"]["open_edges"], r["problems"]]
    return {"models": len(out), "bad": bad,
            "max_faces": max((r["new"]["faces"] for r in out.values() if "new" in r), default=0),
            "back_faces": sum(r.get("back_faces", 0) for r in out.values())}


def build_edit_zoo(columns=12, cell=(60.0, 60.0)):
    """zoo_edit.blend: all models in one file as local data, so that the
    meshes can be edited there. The models stand in a grid (rows along -Y).
    After an edit, run export_all() in that file, then
    tools/model_zoo.py build."""
    clear_file()
    files = sorted(f for f in os.listdir(os.path.join(DIR, "blend")) if f.endswith(".blend"))
    for i, f in enumerate(files):
        mesh = f[:-6]
        with bpy.data.libraries.load(os.path.join(DIR, "blend", f), link=False) as (src, dst):
            dst.collections = [c for c in src.collections if c == mesh]
        if not dst.collections or dst.collections[0] is None:
            continue
        bpy.context.scene.collection.children.link(dst.collections[0])
        x, y = (i % columns) * cell[0], -(i // columns) * cell[1]
        for tag in ("orig", "new"):
            root = bpy.data.objects[mesh + "." + tag]
            root.location.x += x
            root.location.y += y
    # an append makes a copy of a material that the file has: use the first
    for block in (bpy.data.materials, bpy.data.images):
        for d in list(block):
            n = d.name
            if len(n) > 4 and n[-4] == "." and n[-3:].isdigit() and n[:-4] in block:
                d.user_remap(block[n[:-4]])
                block.remove(d)
    path = os.path.join(DIR, "zoo_edit.blend")
    _save_blend(path, copy=False)
    return {"models": len(files), "file": path}


def export_all(library=False):
    """In a file with many models (zoo_edit.blend): write new/<mesh>.json for
    each model from its objects <mesh>.new.sNN. Returns the problems."""
    out = {}
    names = sorted(o.name[:-4] for o in bpy.data.objects if o.name.endswith(".new") and o.type == "EMPTY")
    for mesh in names:
        r = finish(mesh, save=False, library=library)
        if r["problems"] or r["stats"]["faces"] > MAX_FACES:
            out[mesh] = r["problems"] or [r["stats"]["faces"]]
    return {"models": len(names), "problems": out}


# ---- changes of single models after auto() --------------------------------------

def rigid_child(mesh, child):
    """True if a submodel never turns against its parent (all joint angles
    of all frames are 0): its polygons can go into the parent."""
    rec = read_orig(mesh)
    n = len(rec["subs"])
    a = rec["angles"]
    return all(a[3 * (f * n + child) + k] == 0 for f in range(rec["n_frames"]) for k in range(3))


def merge_rigid(mesh, child, from_orig=True):
    """Move the polygons of a submodel into its parent and leave the
    submodel empty. Only for a submodel that never turns against the parent.
    from_orig: start again from the original polygons of both (without the
    caps and the back faces of auto())."""
    rec = read_orig(mesh)
    if not rigid_child(mesh, child):
        raise ValueError("submodel %d turns against its parent" % child)
    parent = rec["subs"][child]["parent"]
    pc = bpy.data.objects["%s.new.s%02d" % (mesh, child)]
    pp = bpy.data.objects["%s.new.s%02d" % (mesh, parent)]
    if from_orig:
        for s, ob in ((child, pc), (parent, pp)):
            src = bpy.data.objects["%s.orig.s%02d" % (mesh, s)].data
            old = ob.data
            name = old.name
            ob.data = src.copy()
            bpy.data.meshes.remove(old)
            ob.data.name = name
    off = cv(rec["subs"][child]["offset"])
    bm = bmesh.new()
    bm.from_mesh(pp.data)
    n0 = len(bm.verts)
    # the material slots of both must be one list
    slots = list(pp.data.materials)
    remap = []
    for mt in pc.data.materials:
        if mt not in slots:
            slots.append(mt)
            pp.data.materials.append(mt)
        remap.append(slots.index(mt))
    tmp = pc.data.copy()
    for p in tmp.polygons:
        p.material_index = remap[p.material_index]
    for v in tmp.vertices:
        v.co = v.co + off
    bm.from_mesh(tmp)
    bpy.data.meshes.remove(tmp)
    bm.to_mesh(pp.data)
    bm.free()
    pc.data.clear_geometry()
    pp.data.update()
    return {"parent": parent, "faces": len(pp.data.polygons)}


def rims(ob):
    """The open edge loops of an object: (edges, center, size)."""
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    out = []
    for cyc in _rim_cycles(bm):
        co = [l.vert.co for l in cyc]
        lo = [min(c[i] for c in co) for i in range(3)]
        hi = [max(c[i] for c in co) for i in range(3)]
        out.append((len(cyc), [round((a + b) / 2, 2) for a, b in zip(lo, hi)],
                    [round(b - a, 2) for a, b in zip(lo, hi)]))
    bm.free()
    return out


def mound(ob, steps=((0.85, 0.45), (0.5, 0.8)), max_edges=64):
    """Close each open edge loop with a round mound: for each step a ring
    with (size, height) of the loop radius, then a face. A joint then stays
    closed when the next submodel turns away. The new faces get the texture
    of the faces at the loop."""
    def fn(bm):
        uvl = bm.loops.layers.uv.verify()
        made = 0
        for cyc in _rim_cycles(bm):
            if len(cyc) < 3 or len(cyc) > max_edges:
                continue
            cyc = list(reversed(cyc))
            verts = [l.vert for l in cyc]
            if len(set(verts)) != len(verts):
                continue
            uvs = [l[uvl].uv.copy() for l in cyc]
            mats = [l.face.material_index for l in cyc]
            mat = max(set(mats), key=mats.count)
            base = [v.co.copy() for v in verts]
            center = sum(base, Vector()) / len(base)
            n = Vector()
            for i in range(len(base)):
                n += (base[i] - center).cross(base[(i + 1) % len(base)] - center)
            if n.length < 1e-9:
                continue
            n.normalize()
            rad = sum((b - center).length for b in base) / len(base)
            uvc = sum(uvs, Vector((0, 0))) / len(uvs)
            for size, height in steps:
                ring = [bm.verts.new(center + (b - center) * size + n * height * rad) for b in base]
                ruv = [uvc + (u - uvc) * size for u in [l[uvl].uv for l in cyc]]
                for i in range(len(verts)):
                    j = (i + 1) % len(verts)
                    f2 = bm.faces.new((verts[i], verts[j], ring[j], ring[i]))
                    f2.material_index = mat
                    for lp, uv in zip(f2.loops, (uvs[i], uvs[j], ruv[j], ruv[i])):
                        lp[uvl].uv = uv
                    made += 1
                verts, uvs = ring, ruv
            f2 = bm.faces.new(verts)
            f2.material_index = mat
            for lp, uv in zip(f2.loops, uvs):
                lp[uvl].uv = uv
            made += 1
        return made
    return _edit(ob, fn)
