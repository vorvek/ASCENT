#!/usr/bin/env python3
"""lvl.py: parser for Descent to Undermountain level files (.LVL, .LV2).

Standard library only. Layout reference: docs/formats/lvl.md

Usage:
    python -I tools/lvl.py LEVEL [LEVEL ...] [options]

Options:
    --obj OUT.obj   export segment geometry to a Wavefront OBJ file
                    (one input file only)
    --all-sides     OBJ export: also write sides that connect two segments
    --textures      list the texture names used by sides
    --objects       list every object
    --walls         list every wall
    --ranges        list every explained byte range
    --quiet         print only the verification lines
"""

import argparse
import collections
import os
import struct
import sys

FIX = 65536.0

LVL_SIG = b"LVLP"
MINE_SIG = 0x2884
GAME_SIG = 0x6705

SEGMENT_SIZE = 536
SIDE_SIZE = 80
WALL_SIZE = 24
DOOR_SIZE = 16
TRIGGER_SIZE = 54
CONTROL_SIZE = 42
MATCEN_SIZE = 16

# Vertex order of each side, from Descent 1 (segment.h, Side_to_verts).
SIDE_TO_VERTS = (
    (7, 6, 2, 3),  # 0 left
    (0, 4, 7, 3),  # 1 top
    (0, 1, 5, 4),  # 2 right
    (2, 6, 5, 1),  # 3 bottom
    (4, 5, 6, 7),  # 4 back
    (3, 2, 1, 0),  # 5 front
)
SIDE_NAMES = ("left", "top", "right", "bottom", "back", "front")
OPPOSITE = (2, 3, 0, 1, 5, 4)

# Names from Descent 1 object.h. Values 15 to 18 are new in this game.
OBJ_TYPES = {
    0: "wall", 1: "fireball", 2: "robot", 3: "hostage", 4: "player",
    5: "weapon", 6: "camera", 7: "powerup", 8: "debris", 9: "cntrlcen",
    10: "flare", 11: "clutter", 12: "ghost", 13: "light", 14: "coop",
    15: "type15", 16: "type16", 17: "type17", 18: "type18", 255: "none",
}
CONTROL_TYPES = {
    0: "none", 1: "ai", 2: "explosion", 4: "flying", 5: "slew",
    6: "flythrough", 9: "weapon", 10: "repaircen", 11: "morph",
    12: "debris", 13: "powerup", 14: "light", 15: "remote", 16: "cntrlcen",
}
MOVEMENT_TYPES = {0: "none", 1: "physics", 3: "spinning"}
RENDER_TYPES = {
    0: "none", 1: "polyobj", 2: "fireball", 3: "laser", 4: "hostage",
    5: "powerup", 6: "morph", 7: "weapon_vclip", 8: "type8",
}
WALL_TYPES = {
    0: "normal", 1: "blastable", 2: "door", 3: "illusion", 4: "open",
    5: "closed",
}


class FormatError(Exception):
    pass


class Reader:
    """Sequential little-endian reader over a bytes object."""

    def __init__(self, data, pos=0):
        self.data = data
        self.pos = pos

    def take(self, fmt):
        size = struct.calcsize(fmt)
        if self.pos + size > len(self.data):
            raise FormatError("read past end of file at %d" % self.pos)
        vals = struct.unpack_from(fmt, self.data, self.pos)
        self.pos += size
        return vals

    def u8(self):
        return self.take("<B")[0]

    def s8(self):
        return self.take("<b")[0]

    def s16(self):
        return self.take("<h")[0]

    def u16(self):
        return self.take("<H")[0]

    def s32(self):
        return self.take("<i")[0]

    def vec(self):
        return self.take("<3i")

    def raw(self, n):
        if self.pos + n > len(self.data):
            raise FormatError("read past end of file at %d" % self.pos)
        b = self.data[self.pos:self.pos + n]
        self.pos += n
        return b


def cstr(b):
    return b.split(b"\0", 1)[0].decode("latin-1")


MINE_FILEINFO_FIELDS = (
    "signature", "version", "sizeof",
    "header_offset", "header_size",
    "editor_offset", "editor_size",
    "segment_offset", "segment_howmany", "segment_sizeof",
    "newseg_verts_offset", "newseg_verts_howmany", "newseg_verts_sizeof",
    "group_offset", "group_howmany", "group_sizeof",
    "vertex_offset", "vertex_howmany", "vertex_sizeof",
    "texture_offset", "texture_howmany", "texture_sizeof",
    "walls_offset", "walls_howmany", "walls_sizeof",
    "triggers_offset", "triggers_howmany", "triggers_sizeof",
    "links_offset", "links_howmany", "links_sizeof",
    "object_offset", "object_howmany", "object_sizeof",
    "unused_offset", "unused_howmany", "unused_sizeof",
)
MINE_FILEINFO_FMT = "<HHi34i"  # 144 bytes

GAME_FILEINFO_FIELDS = (
    "signature", "version", "sizeof", "mine_filename", "level",
    "player_offset", "player_sizeof",
    "object_offset", "object_howmany", "object_sizeof",
    "walls_offset", "walls_howmany", "walls_sizeof",
    "doors_offset", "doors_howmany", "doors_sizeof",
    "triggers_offset", "triggers_howmany", "triggers_sizeof",
    "links_offset", "links_howmany", "links_sizeof",
    "control_offset", "control_howmany", "control_sizeof",
    "matcen_offset", "matcen_howmany", "matcen_sizeof",
)
GAME_FILEINFO_FMT = "<HHi15si23i"  # 119 bytes


class Level:
    def __init__(self, data, name="?"):
        self.data = data
        self.name = name
        self.ranges = []   # (start, end, label, kind)
        self.notes = []
        self.parse()

    # range accounting -----------------------------------------------------

    def mark(self, start, end, label, kind="parsed"):
        if end > start:
            self.ranges.append((start, end, label, kind))

    def unexplained(self):
        """Return (gaps, overlaps) over the whole file."""
        gaps = []
        overlaps = []
        pos = 0
        for start, end, label, kind in sorted(self.ranges):
            if start > pos:
                gaps.append((pos, start))
            elif start < pos:
                overlaps.append((start, min(pos, end), label))
            pos = max(pos, end)
        if pos < len(self.data):
            gaps.append((pos, len(self.data)))
        return gaps, overlaps

    # parsing --------------------------------------------------------------

    def parse(self):
        d = self.data
        if len(d) < 20 or d[:4] != LVL_SIG:
            raise FormatError("no LVLP signature")
        (self.version, self.mine_offset, self.game_offset,
         self.trailer_offset) = struct.unpack_from("<4i", d, 4)
        self.mark(0, 20, "level header")
        self.parse_mine()
        self.parse_game()
        if self.trailer_offset < len(d):
            self.trailer = d[self.trailer_offset:]
            self.notes.append(
                "%d bytes after the trailer offset are not understood: %s"
                % (len(self.trailer), self.trailer.hex(" ")))
        else:
            self.trailer = b""

    def parse_mine(self):
        d = self.data
        o = self.mine_offset
        vals = struct.unpack_from(MINE_FILEINFO_FMT, d, o)
        mfi = self.mine_fileinfo = dict(zip(MINE_FILEINFO_FIELDS, vals))
        if mfi["signature"] != MINE_SIG:
            raise FormatError("bad mine signature 0x%04x" % mfi["signature"])
        if mfi["version"] != 18:
            raise FormatError("mine version %d is not supported (only 18)"
                              % mfi["version"])
        if mfi["sizeof"] != 144:
            raise FormatError("mine fileinfo size %d" % mfi["sizeof"])
        self.mark(o, o + 144, "mine fileinfo")

        # mine header
        o = mfi["header_offset"]
        self.num_vertices, self.num_segments = struct.unpack_from("<2i", d, o)
        self.mark(o, o + mfi["header_size"], "mine header")

        # mine editor block
        o = mfi["editor_offset"]
        e = struct.unpack_from("<28i", d, o)
        self.mine_editor = {
            "current_seg": e[0], "newsegment_offset": e[1],
            "newsegment_size": e[2], "curside": e[3], "markedsegp": e[4],
            "markedside": e[5], "groupsegp": e[6:16], "groupside": e[16:26],
            "num_groups": e[26], "current_group": e[27],
        }
        self.mark(o, o + mfi["editor_size"], "mine editor block")

        # texture names
        o = mfi["texture_offset"]
        n, sz = mfi["texture_howmany"], mfi["texture_sizeof"]
        self.textures = [cstr(d[o + i * sz:o + (i + 1) * sz])
                         for i in range(n)]
        self.mark(o, o + n * sz, "texture names")

        # vertices
        o = mfi["vertex_offset"]
        n, sz = mfi["vertex_howmany"], mfi["vertex_sizeof"]
        if sz != 12:
            raise FormatError("vertex size %d" % sz)
        self.vertices = [struct.unpack_from("<3i", d, o + i * 12)
                         for i in range(n)]
        self.mark(o, o + n * 12, "vertices")

        # segments
        o = mfi["segment_offset"]
        n, sz = mfi["segment_howmany"], mfi["segment_sizeof"]
        if sz != SEGMENT_SIZE:
            raise FormatError("segment size %d" % sz)
        self.segments = [self.parse_segment(d, o + i * sz) for i in range(n)]
        self.mark(o, o + n * sz, "segments")

        # the editor "new segment" template and its 8 vertices
        o, sz = (self.mine_editor["newsegment_offset"],
                 self.mine_editor["newsegment_size"])
        if o > 0 and sz == SEGMENT_SIZE:
            self.new_segment = self.parse_segment(d, o)
            self.mark(o, o + sz, "editor new segment")
        else:
            self.new_segment = None
        o = mfi["newseg_verts_offset"]
        n, sz = mfi["newseg_verts_howmany"], mfi["newseg_verts_sizeof"]
        self.newseg_verts = [struct.unpack_from("<3i", d, o + i * sz)
                             for i in range(n)] if sz == 12 else []
        self.mark(o, o + n * sz, "editor new segment vertices")

    @staticmethod
    def parse_segment(d, o):
        seg = {}
        seg["segnum"], seg["group"] = struct.unpack_from("<hh", d, o)
        sides = []
        for s in range(6):
            so = o + 4 + s * SIDE_SIZE
            typ, flag, wall_num, tmap, tmap2 = struct.unpack_from(
                "<BBhHH", d, so)
            uvl = [struct.unpack_from("<3i", d, so + 8 + 12 * i)
                   for i in range(4)]
            normals = [struct.unpack_from("<3i", d, so + 56 + 12 * i)
                       for i in range(2)]
            sides.append({
                "type": typ, "byte1": flag, "wall_num": wall_num,
                "tmap_num": tmap, "tmap_num2": tmap2,
                "uvls": uvl, "normals": normals,
            })
        seg["sides"] = sides
        seg["children"] = struct.unpack_from("<6h", d, o + 484)
        seg["verts"] = struct.unpack_from("<8h", d, o + 496)
        (seg["static_light"], seg["unk516"], seg["unk520"], seg["unk524"],
         seg["objects"], seg["value"], seg["unk532"], seg["unk533"],
         seg["special"], seg["matcen_num"]) = struct.unpack_from(
            "<4ihhBBBb", d, o + 512)
        return seg

    def parse_game(self):
        d = self.data
        o = self.game_offset
        sig, ver, size = struct.unpack_from("<HHi", d, o)
        if sig != GAME_SIG:
            raise FormatError("bad game signature 0x%04x" % sig)
        if size != 119:
            raise FormatError("game fileinfo size %d" % size)
        vals = struct.unpack_from(GAME_FILEINFO_FMT, d, o)
        gfi = self.game_fileinfo = dict(zip(GAME_FILEINFO_FIELDS, vals))
        if ver != 41:
            raise FormatError("game version %d is not supported (only 41)"
                              % ver)
        self.mark(o, o + 119, "game fileinfo")
        o += 119

        # level name: NUL terminated
        end = d.index(b"\0", o)
        self.level_name = d[o:end].decode("latin-1")
        self.mark(o, end + 1, "level name")
        o = end + 1

        # POF name list. The game reads pof_count names. The file holds
        # more: the list runs up to player_offset.
        self.pof_count = struct.unpack_from("<H", d, o)[0]
        self.mark(o, o + 2, "pof name count")
        o += 2
        total = (gfi["player_offset"] - o) // 13
        self.pof_names = [cstr(d[o + i * 13:o + (i + 1) * 13])
                          for i in range(total)]
        n_read = min(self.pof_count, total)
        self.mark(o, o + n_read * 13, "pof names read by the game")
        self.mark(o + n_read * 13, o + total * 13,
                  "pof names past the count")
        o += total * 13
        if o != gfi["player_offset"]:
            self.notes.append("pof name list does not end at player_offset")

        # player block: written by the editor, never read by the game
        o = gfi["player_offset"]
        self.player_block = d[o:o + gfi["player_sizeof"]]
        self.mark(o, o + gfi["player_sizeof"], "player block", "opaque")

        # objects: variable size records
        r = Reader(d, gfi["object_offset"])
        self.objects = []
        for i in range(gfi["object_howmany"]):
            start = r.pos
            ob = self.read_object(r, ver)
            ob["offset"] = start
            ob["length"] = r.pos - start
            self.objects.append(ob)
        self.objects_end = r.pos
        self.mark(gfi["object_offset"], r.pos, "objects")

        # walls
        o, n = gfi["walls_offset"], gfi["walls_howmany"]
        if n and gfi["walls_sizeof"] != WALL_SIZE:
            raise FormatError("wall size %d" % gfi["walls_sizeof"])
        self.walls = []
        for i in range(n):
            v = struct.unpack_from("<4i4BbBh", d, o + i * WALL_SIZE)
            self.walls.append(dict(zip(
                ("segnum", "sidenum", "hps", "linked_wall", "type", "flags",
                 "state", "trigger", "clip_num", "keys", "pad"), v)))
        self.mark(o, o + n * WALL_SIZE, "walls")

        # active doors
        o, n = gfi["doors_offset"], gfi["doors_howmany"]
        self.doors = []
        for i in range(n):
            v = struct.unpack_from("<i4hi", d, o + i * DOOR_SIZE)
            self.doors.append({"n_parts": v[0], "front_wallnum": v[1:3],
                               "back_wallnum": v[3:5], "time": v[5]})
        self.mark(o, o + n * DOOR_SIZE, "active doors")

        # triggers
        o, n = gfi["triggers_offset"], gfi["triggers_howmany"]
        self.triggers = []
        for i in range(n):
            v = struct.unpack_from("<bhiibh10h10h", d, o + i * TRIGGER_SIZE)
            self.triggers.append({
                "type": v[0], "flags": v[1], "value": v[2], "time": v[3],
                "link_num": v[4], "num_links": v[5],
                "seg": v[6:16], "side": v[16:26]})
        self.mark(o, o + n * TRIGGER_SIZE, "triggers")

        # control center triggers
        o, n = gfi["control_offset"], gfi["control_howmany"]
        self.control = []
        for i in range(n if o > -1 else 0):
            v = struct.unpack_from("<h10h10h", d, o + i * CONTROL_SIZE)
            self.control.append({"num_links": v[0], "seg": v[1:11],
                                 "side": v[11:21]})
        if o > -1:
            self.mark(o, o + n * CONTROL_SIZE, "control center triggers")

        # robot makers
        o, n = gfi["matcen_offset"], gfi["matcen_howmany"]
        self.matcens = []
        for i in range(n if o > -1 else 0):
            v = struct.unpack_from("<3i2h", d, o + i * MATCEN_SIZE)
            self.matcens.append(dict(zip(
                ("robot_flags", "hit_points", "interval", "segnum",
                 "fuelcen_num"), v)))
        if o > -1:
            self.mark(o, o + n * MATCEN_SIZE, "matcens")

    @staticmethod
    def read_object(r, ver):
        """Mirror of the read_object routine in DTUGAME.EXE (1.3)."""
        ob = {}
        (ob["type"], ob["id"], ob["control_type"], ob["movement_type"],
         ob["render_type"], ob["flags"]) = r.take("<6B")
        ob["segnum"] = r.s16()
        ob["attached_obj"] = r.s16() if ver > 32 else -1
        ob["pos"] = r.vec()
        ob["orient"] = r.take("<9i")
        ob["size"] = r.s32()
        ob["shields"] = r.s32()
        ob["last_pos"] = r.vec()
        ob["contains_type"], ob["contains_id"], ob["contains_count"] = \
            r.take("<3b")

        # Block that Descent 1 does not have. Keys are the offsets of the
        # fields in the in-memory object struct of the 1.3 executable.
        ext = collections.OrderedDict()
        if ver > 31:
            ext[0x80] = r.s16()
            ext[0x82] = r.s16()
            ext[0x88] = r.s32()
            for off in range(0x8c, 0xc0, 4):
                ext[off] = r.s32()
            if ver > 35:
                ext[0xc0] = r.s32()
                ext[0xc4] = r.s32()
            for off in range(0xc8, 0xe0, 4):
                ext[off] = r.s32()
            ext[0xe0] = r.s32()
            ext[0xe4] = r.s32()
            if ver > 36:
                for off in range(0xe8, 0xfc, 4):
                    ext[off] = r.s32()
            if ver > 40:
                ext[0x1b0] = r.s32()
        ob["ext"] = ext

        mt = ob["movement_type"]
        if mt == 1:
            ob["phys"] = {
                "velocity": r.vec(), "thrust": r.vec(),
                "mass": r.s32(), "drag": r.s32(), "brakes": r.s32(),
                "rotvel": r.vec(), "rotthrust": r.vec(),
                "turnroll": r.s16(), "flags": r.u16(),
            }
        elif mt == 3:
            ob["spin_rate"] = r.vec()
        elif mt != 0:
            raise FormatError("object movement type %d at %d" % (mt, r.pos))

        ct = ob["control_type"]
        if ct == 1:
            ai = {"behavior": r.u8(), "flags": r.take("<11b")}
            (ai["hide_segment"], ai["hide_index"], ai["path_length"],
             ai["cur_path_index"], ai["follow_path_start_seg"],
             ai["follow_path_end_seg"]) = r.take("<6h")
            ob["ai"] = ai
        elif ct == 2:
            ob["explosion"] = r.take("<iih")
        elif ct == 9:
            ob["weapon"] = r.take("<hhi")
        elif ct == 13:
            if ver >= 25:
                ob["powerup_count"] = r.s32()
        elif ct == 14:
            ob["light_intensity"] = r.s32()
            ob["light_short"] = r.s16() if ver > 38 else 0
        elif ct not in (0, 4, 5, 12, 16):
            raise FormatError("object control type %d at %d" % (ct, r.pos))

        rt = ob["render_type"]
        if rt in (1, 6):
            po = {"model_num": r.s32()}
            # versions below 30 store animation angles here: not supported
            if ver < 30:
                raise FormatError("game version below 30")
            if ver > 34:
                po["m16e"] = r.s16()
                po["m170"] = r.s16()
                po["m172"] = r.s32()
            elif ver > 27:
                po["m170"] = r.s32()
                po["m172"] = r.s32()
            if ver > 30:
                po["m176"] = r.s32()
            po["subobj_flags"] = r.s32()
            po["tmap_override"] = r.s32()
            ob["polyobj"] = po
        elif rt in (2, 7):
            ob["vclip"] = r.take("<iib")
        elif rt == 5:
            ob["vclip"] = r.take("<iib")   # read and discarded by the game
        elif rt == 8:
            ob["render8"] = r.s32()
        elif rt not in (0, 3):
            raise FormatError("object render type %d at %d" % (rt, r.pos))
        return ob

    # derived data ---------------------------------------------------------

    def bbox(self):
        vs = self.vertices
        if not vs:
            return None
        lo = [min(v[k] for v in vs) / FIX for k in range(3)]
        hi = [max(v[k] for v in vs) / FIX for k in range(3)]
        return lo, hi

    def side_is_rendered(self, seg, s):
        return seg["children"][s] == -1 or seg["sides"][s]["wall_num"] != -1

    # verification ---------------------------------------------------------

    def verify(self):
        """Return an ordered dict: check name -> (passed, total)."""
        res = collections.OrderedDict()
        nseg = len(self.segments)
        nvert = len(self.vertices)
        ntex = len(self.textures)

        ok = tot = 0
        for seg in self.segments:
            for c in seg["children"]:
                tot += 1
                ok += (-2 <= c < nseg)
        res["children index in range"] = (ok, tot)

        ok = tot = 0
        for i, seg in enumerate(self.segments):
            for s, c in enumerate(seg["children"]):
                if 0 <= c < nseg:
                    tot += 1
                    ok += (i in self.segments[c]["children"])
        res["children symmetric"] = (ok, tot)

        ok = tot = 0
        for seg in self.segments:
            for v in seg["verts"]:
                tot += 1
                ok += (0 <= v < nvert)
        res["vertex index in range"] = (ok, tot)

        # connected sides share the same four vertices
        ok = tot = 0
        for i, seg in enumerate(self.segments):
            for s, c in enumerate(seg["children"]):
                if not 0 <= c < nseg:
                    continue
                other = self.segments[c]
                if i not in other["children"]:
                    continue
                tot += 1
                mine = set(seg["verts"][k] for k in SIDE_TO_VERTS[s])
                os_ = other["children"].index(i)
                theirs = set(other["verts"][k] for k in SIDE_TO_VERTS[os_])
                ok += (mine == theirs)
        res["connected sides share 4 vertices"] = (ok, tot)

        ok = tot = 0
        for seg in self.segments:
            for side in seg["sides"]:
                tot += 2
                ok += (side["tmap_num"] < ntex)
                ok += ((side["tmap_num2"] & 0x3fff) < ntex)
        res["texture index in range"] = (ok, tot)

        ok = tot = 0
        for seg in self.segments:
            for side in seg["sides"]:
                tot += 1
                ok += (side["type"] in (1, 2, 3))
        res["side type is 1, 2 or 3"] = (ok, tot)

        # stored normal 0 against the normal computed from the vertices
        ok = tot = 0
        for seg in self.segments:
            if not all(0 <= v < nvert for v in seg["verts"]):
                continue
            for s, side in enumerate(seg["sides"]):
                vi = [seg["verts"][k] for k in SIDE_TO_VERTS[s]]
                p = [self.vertices[v] for v in vi]
                if side["type"] == 3:
                    tri = (p[0], p[1], p[3])
                else:
                    tri = (p[0], p[1], p[2])
                n = _normal(tri)
                if n is None:
                    continue
                tot += 1
                sn = [c / FIX for c in side["normals"][0]]
                ok += (sum(a * b for a, b in zip(n, sn)) > 0.99)
        res["side normal matches geometry"] = (ok, tot)

        ok = tot = 0
        for seg in self.segments:
            for side in seg["sides"]:
                tot += 1
                ok += (-1 <= side["wall_num"] < len(self.walls))
        res["side wall_num in range"] = (ok, tot)

        ok = tot = 0
        for w, wall in enumerate(self.walls):
            tot += 1
            sn, sd = wall["segnum"], wall["sidenum"]
            if 0 <= sn < nseg and 0 <= sd < 6:
                ok += (self.segments[sn]["sides"][sd]["wall_num"] == w)
        res["wall matches side wall_num"] = (ok, tot)

        ok = tot = 0
        for i, seg in enumerate(self.segments):
            for s, side in enumerate(seg["sides"]):
                w = side["wall_num"]
                if w == -1:
                    continue
                tot += 1
                if 0 <= w < len(self.walls):
                    ok += (self.walls[w]["segnum"] == i
                           and self.walls[w]["sidenum"] == s)
        res["side wall_num matches wall"] = (ok, tot)

        ok = tot = 0
        for wall in self.walls:
            tot += 1
            ok += (-1 <= wall["linked_wall"] < len(self.walls)
                   and (wall["trigger"] == 255
                        or wall["trigger"] < max(1, len(self.triggers))))
        res["wall links in range"] = (ok, tot)

        ok = tot = 0
        for ob in self.objects:
            tot += 1
            ok += (0 <= ob["segnum"] < nseg)
        res["object segnum in range"] = (ok, tot)

        ok = ok2 = tot = 0
        for ob in self.objects:
            if not 0 <= ob["segnum"] < nseg:
                continue
            seg = self.segments[ob["segnum"]]
            if not all(0 <= v < nvert for v in seg["verts"]):
                continue
            tot += 1
            pts = [self.vertices[v] for v in seg["verts"]]
            inside = all(
                min(p[k] for p in pts) <= ob["pos"][k]
                <= max(p[k] for p in pts) for k in range(3))
            ok += inside
            ok2 += self.point_in_segment(ob["pos"], seg)
        res["object position inside segment bounding box"] = (ok, tot)
        res["object position behind all 6 side planes"] = (ok2, tot)

        ok = tot = 0
        for ob in self.objects:
            tot += 1
            m = [c / FIX for c in ob["orient"]]
            rows = (m[0:3], m[3:6], m[6:9])
            good = all(abs(sum(c * c for c in row) - 1.0) < 0.02
                       for row in rows)
            ok += good
        res["object orientation rows are unit vectors"] = (ok, tot)

        gfi = self.game_fileinfo
        res["object records end at walls_offset"] = (
            int(self.objects_end == gfi["walls_offset"]), 1)
        res["header counts match"] = (
            int(self.num_vertices == len(self.vertices))
            + int(self.num_segments == len(self.segments)), 2)
        return res

    def point_in_segment(self, pos, seg):
        for s, side in enumerate(seg["sides"]):
            v0 = self.vertices[seg["verts"][SIDE_TO_VERTS[s][0]]]
            n = side["normals"][0]
            dist = sum((pos[k] - v0[k]) / FIX * (n[k] / FIX)
                       for k in range(3))
            # Descent normals point into the segment
            if dist < -0.05:
                return False
        return True

    # export ---------------------------------------------------------------

    def export_obj(self, path, all_sides=False):
        lines = ["# %s: %s" % (self.name, self.level_name),
                 "# exported by ASCENT tools/lvl.py"]
        # Descent is left handed (x right, y up, z forward): negate z.
        for v in self.vertices:
            lines.append("v %.5f %.5f %.5f"
                         % (v[0] / FIX, v[1] / FIX, -v[2] / FIX))
        faces = collections.defaultdict(list)
        vt = 0
        nvert = len(self.vertices)
        count = 0
        for seg in self.segments:
            if not all(0 <= v < nvert for v in seg["verts"]):
                continue
            for s, side in enumerate(seg["sides"]):
                if not all_sides and not self.side_is_rendered(seg, s):
                    continue
                vi = [seg["verts"][k] + 1 for k in SIDE_TO_VERTS[s]]
                ti = []
                for u, v, _l in side["uvls"]:
                    lines.append("vt %.5f %.5f" % (u / FIX, 1.0 - v / FIX))
                    vt += 1
                    ti.append(vt)
                idx = side["tmap_num"]
                name = (self.textures[idx] if idx < len(self.textures)
                        else "tmap%d" % idx)
                c = list(zip(vi, ti))
                # z is negated, so the winding is reversed
                if side["type"] == 3:
                    tris = ((c[3], c[1], c[0]), (c[3], c[2], c[1]))
                else:
                    tris = ((c[2], c[1], c[0]), (c[3], c[2], c[0]))
                for t in tris:
                    faces[name].append(
                        "f " + " ".join("%d/%d" % p for p in t))
                count += 1
        for name in sorted(faces):
            lines.append("g %s" % name)
            lines.append("usemtl %s" % name)
            lines.extend(faces[name])
        with open(path, "w", newline="\n") as f:
            f.write("\n".join(lines) + "\n")
        return count


def _normal(tri):
    a, b, c = tri
    u = [(b[k] - a[k]) / FIX for k in range(3)]
    v = [(c[k] - b[k]) / FIX for k in range(3)]
    n = [u[1] * v[2] - u[2] * v[1],
         u[2] * v[0] - u[0] * v[2],
         u[0] * v[1] - u[1] * v[0]]
    mag = sum(x * x for x in n) ** 0.5
    if mag < 1e-6:
        return None
    return [x / mag for x in n]


def fx(v):
    return "%.3f" % (v / FIX)


def print_summary(lv, args):
    mfi, gfi = lv.mine_fileinfo, lv.game_fileinfo
    print("level name     : %s" % lv.level_name)
    print("container      : LVLP version %d, mine at %d, game at %d, "
          "trailer offset %d"
          % (lv.version, lv.mine_offset, lv.game_offset, lv.trailer_offset))
    print("mine data      : signature 0x%04x version %d"
          % (mfi["signature"], mfi["version"]))
    print("game data      : signature 0x%04x version %d"
          % (gfi["signature"], gfi["version"]))
    print("vertices       : %d" % len(lv.vertices))
    print("segments       : %d" % len(lv.segments))
    bb = lv.bbox()
    if bb:
        print("bounding box   : min (%.2f, %.2f, %.2f) max (%.2f, %.2f, %.2f)"
              % (tuple(bb[0]) + tuple(bb[1])))
    print("texture names  : %d in table (%d unique)"
          % (len(lv.textures), len(set(lv.textures))))
    used = collections.Counter()
    used2 = collections.Counter()
    for seg in lv.segments:
        for s, side in enumerate(seg["sides"]):
            if not lv.side_is_rendered(seg, s):
                continue
            used[side["tmap_num"]] += 1
            if side["tmap_num2"] & 0x3fff:
                used2[side["tmap_num2"] & 0x3fff] += 1
    print("textures used  : %d base, %d overlay (rendered sides only)"
          % (len(used), len(used2)))
    print("pof names      : count field %d, names stored %d"
          % (lv.pof_count, len(lv.pof_names)))
    print("objects        : %d (%d bytes, header says sizeof %d)"
          % (len(lv.objects), lv.objects_end - gfi["object_offset"],
             gfi["object_sizeof"]))
    print("walls          : %d" % len(lv.walls))
    print("active doors   : %d" % len(lv.doors))
    print("triggers       : %d" % len(lv.triggers))
    print("control center : %d" % len(lv.control))
    print("matcens        : %d" % len(lv.matcens))
    print("player block   : %d bytes, %d nonzero (not read by the game)"
          % (len(lv.player_block), sum(1 for c in lv.player_block if c)))

    hist = collections.Counter(ob["type"] for ob in lv.objects)
    print("object types   : " + ", ".join(
        "%s=%d" % (OBJ_TYPES.get(t, "type%d" % t), n)
        for t, n in sorted(hist.items())))
    hist = collections.Counter(ob["length"] for ob in lv.objects)
    print("object lengths : " + ", ".join(
        "%d bytes x%d" % kv for kv in sorted(hist.items())))
    hist = collections.Counter(w["type"] for w in lv.walls)
    print("wall types     : " + ", ".join(
        "%s=%d" % (WALL_TYPES.get(t, "type%d" % t), n)
        for t, n in sorted(hist.items())))
    hist = collections.Counter(seg["special"] for seg in lv.segments)
    print("segment special: " + ", ".join(
        "%d x%d" % kv for kv in sorted(hist.items())))

    if args.textures:
        print("textures used by rendered sides (index name count):")
        for idx, n in sorted(used.items()):
            nm = lv.textures[idx] if idx < len(lv.textures) else "?"
            print("  base    %4d %-12s %d" % (idx, nm, n))
        for idx, n in sorted(used2.items()):
            nm = lv.textures[idx] if idx < len(lv.textures) else "?"
            print("  overlay %4d %-12s %d" % (idx, nm, n))
    if args.objects:
        print("objects (index type id control movement render seg pos "
              "size shields model pof):")
        for i, ob in enumerate(lv.objects):
            model = ob.get("polyobj", {}).get("model_num", -1)
            pof = (lv.pof_names[model]
                   if 0 <= model < len(lv.pof_names) else "")
            print("  %3d %-8s id=%-3d %-9s %-8s %-8s seg=%-4d "
                  "(%s, %s, %s) size=%s shields=%s model=%d %s"
                  % (i, OBJ_TYPES.get(ob["type"], ob["type"]), ob["id"],
                     CONTROL_TYPES.get(ob["control_type"],
                                       ob["control_type"]),
                     MOVEMENT_TYPES.get(ob["movement_type"],
                                        ob["movement_type"]),
                     RENDER_TYPES.get(ob["render_type"], ob["render_type"]),
                     ob["segnum"], fx(ob["pos"][0]), fx(ob["pos"][1]),
                     fx(ob["pos"][2]), fx(ob["size"]), fx(ob["shields"]),
                     model, pof))
    if args.walls:
        print("walls (index seg side type flags state trigger clip keys "
              "linked hps):")
        for i, w in enumerate(lv.walls):
            print("  %3d seg=%-4d side=%d %-9s flags=0x%02x state=%d "
                  "trigger=%d clip=%d keys=0x%02x linked=%d hps=%s"
                  % (i, w["segnum"], w["sidenum"],
                     WALL_TYPES.get(w["type"], w["type"]), w["flags"],
                     w["state"], w["trigger"], w["clip_num"], w["keys"],
                     w["linked_wall"], fx(w["hps"])))
        for i, t in enumerate(lv.triggers):
            print("  trigger %d: type=%d flags=0x%04x value=%s time=%s "
                  "links=%s"
                  % (i, t["type"], t["flags"], fx(t["value"]),
                     fx(t["time"]),
                     list(zip(t["seg"], t["side"]))[:t["num_links"]]))
    if args.ranges:
        print("byte ranges:")
        for start, end, label, kind in sorted(lv.ranges):
            print("  %8d %8d %8d  %s%s"
                  % (start, end, end - start, label,
                     "" if kind == "parsed" else " [%s]" % kind))


def print_verification(lv):
    total = len(lv.data)
    gaps, overlaps = lv.unexplained()
    opaque = sum(e - s for s, e, _l, k in lv.ranges if k == "opaque")
    gap_bytes = sum(e - s for s, e in gaps)
    print("coverage       : %d of %d bytes parsed, %d opaque, "
          "%d unexplained"
          % (total - gap_bytes - opaque, total, opaque, gap_bytes))
    for s, e in gaps:
        print("  unexplained range %d..%d (%d bytes)" % (s, e, e - s))
    for s, e, label in overlaps:
        print("  overlap %d..%d in %s" % (s, e, label))
    for note in lv.notes:
        print("  note: %s" % note)
    all_ok = True
    for name, (ok, tot) in lv.verify().items():
        pct = 100.0 * ok / tot if tot else 100.0
        if ok != tot:
            all_ok = False
        print("check          : %-45s %6d / %-6d %6.2f%%"
              % (name, ok, tot, pct))
    return all_ok and not gaps and not overlaps


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Parse Descent to Undermountain level files.")
    ap.add_argument("files", nargs="+")
    ap.add_argument("--obj", metavar="OUT.obj")
    ap.add_argument("--all-sides", action="store_true")
    ap.add_argument("--textures", action="store_true")
    ap.add_argument("--objects", action="store_true")
    ap.add_argument("--walls", action="store_true")
    ap.add_argument("--ranges", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args(argv)
    if args.obj and len(args.files) != 1:
        ap.error("--obj needs exactly one input file")

    status = 0
    for path in args.files:
        print("== %s" % path)
        try:
            with open(path, "rb") as f:
                data = f.read()
            lv = Level(data, os.path.basename(path))
        except (OSError, FormatError, struct.error, ValueError) as e:
            print("error: %s" % e)
            status = 1
            continue
        print("file size      : %d" % len(data))
        if not args.quiet:
            print_summary(lv, args)
        if not print_verification(lv):
            status = max(status, 2)
        if args.obj:
            n = lv.export_obj(args.obj, args.all_sides)
            print("wrote %s: %d vertices, %d sides"
                  % (args.obj, len(lv.vertices), n))
    return status


if __name__ == "__main__":
    sys.exit(main())
