# POM: improved character models

A POM file holds new polygon data for one character model of an EGG archive. It is a
format of the port, not of the game. `tools/model_zoo.py build` writes the files and
`polyobj.c` (`pom_load`) reads them. The files are made from the models of the game:
they live in `gamedata/hd/polymodels` and are never committed.

Status: the tool wrote the 850 files of all model names, and `model_zoo.py check`
parsed them. The port loaded `durnan.2ecdda9f.pom` in the level YPINN (GPU renderer):
the model stands at the height of the original and has the same outline.

## What a file replaces

A model of an archive has 19 arrays (egg.md, "Model data block"). A POM file replaces
two of them:

- array 1, `model_data`: the interpreter data (points, polygons, UV),
- array 2, `submodel_ptrs`: the offset of each submodel in the interpreter data.

All other arrays and the header stay those of the archive: the joints, the joint
angles of the animation, the boxes and the flags. So a POM model has the same
submodels, the same animation and the same collision values as the original. The
texture numbers of its polygons are those of the original model, so it draws with the
textures of the table line, original or upscaled.

## File name

`<hd>/polymodels/<name>.<crc>.pom`

- `<name>`: the model name of the EGG header in lower case, without `.pof`.
- `<crc>`: 8 hex digits, the CRC-32 (the polynomial of zlib) of arrays 1 and 2 of the
  original model, in file order.

The port computes the CRC of the model that it loads and looks for the file with that
name. A model with other data (another version of the game, a changed archive) finds
no file and stays as it is. Two archives can have one model name with different data:
each gets its own file.

## Layout

All values are little endian.

| Offset | Size | Meaning |
|---|---|---|
| 0 | 4 | `APOM` |
| 4 | 4 | version, 1 |
| 8 | 4 | CRC-32 of arrays 1 and 2 of the original model |
| 12 | 4 | `n`: number of submodels. Equal to `n_models` of the original |
| 16 | 4 | `size`: size of the interpreter data |
| 20 | `size` | interpreter data |
| 20 + `size` | 4n | `submodel_ptrs`, s32 each |

The port refuses a file with another magic, version, CRC or `n`, or with a `size`
below 2 or above 4 MiB, and writes a line to the log.

## Interpreter data

The data is a Descent 1 opcode stream, as in the archives (models.md, "The sorted
draw"). The stream of a submodel is:

1. DEFPOINTS (opcode 1): u16 count, then the points (3 fix each). The points start at
   number 0 in each submodel.
2. A tree of SORTNORM nodes (opcode 4) with TMAPPOLY (3) and FLATPOLY (2) records.
3. EOF (opcode 0).

A SORTNORM record has 32 bytes: u16 opcode, u16 0, the normal (offset 4), a point of
the plane (16), the u16 offset of the front stream (28) and of the back stream (30),
both from the start of the record. An EOF follows the record. The shorter of the two
streams comes first, because the offsets have 16 bits.

The tool does not cut polygons. A polygon on both sides of a plane goes to the side
of its center. A plane is the plane of a polygon (that polygon is then the first
record of the front stream) or a plane along an axis through the middle of the set. A
set of 24 polygons or less where no polygon is in front of another is one list. The
tree has 28 levels at most (the readers of `polyobj.c` stop at 32 and 64).

A submodel without polygons is one EOF. Two zero bytes follow the last EOF of the
data: `pts_box_walk` wants 4 bytes at each opcode.

A face of the Blender file that is not flat (more than 3.5 percent of its size) or not
convex, or has more than 20 points, goes out as triangles.

The GPU renderer draws a polygon as a fan of triangles from its first point
(`gpu.c`). Blender draws a quad as the triangles 0-1-2 and 0-2-3 of its corners. If
the UV of a face are not an affine function of its points (a quad with one narrow
side and a rectangle of the texture), the two splits give different pictures. So
`zoo.export_sub` picks the first point of each polygon:

- A face that is equal to a polygon of the original (points and UV) keeps the first
  point of that polygon. It draws as in the game.
- Another face with UV that are not affine starts at the point that all triangles
  of Blender share. If no such point exists, the face goes out as triangles.

Verified in YPINN with `elf_p`: before this rule the mouth of the priestess had half
its width on the narrow chin that was made in Blender, with the original and with
the upscaled texture.

## The setting

`ImprovedModels` in `ascent.cfg` (item "Improved models" of the Graphics menu, on by
default) and the option `-origmodels`. The files are read when an archive is loaded,
so a change of the setting shows after the next level load. The software renderer
does not use the files (`DTU_GFX`).

## How the files are made

1. `python -I tools/model_zoo.py extract` writes one record for each mesh to
   `gamedata/models/orig` (127 meshes for 1006 model names in 57 archives) and the
   textures to `gamedata/models/tex`.
2. In Blender, `tools/blender/zoo.py`: `zoo.batch()` makes
   `gamedata/models/blend/<mesh>.blend` for each mesh, with the original
   (`<mesh>.orig`) and the improved copy (`<mesh>.new`) side by side, and
   `gamedata/models/new/<mesh>.json`. `zoo.build_zoo()` makes `zoo.blend` with a link
   to each file.
3. `python -I tools/model_zoo.py build` writes the POM files.
4. `python -I tools/content_pack.py models` makes the content pack `models`.

What the improved copy changes (`zoo.auto`):

- Points at one place are merged.
- Each open edge loop of a body gets a face. The face has the texture and the UV of
  the faces at the loop. The 127 originals have 13181 open edges.
- A sheet (a blade, a wing, a cape, a bow) gets a back face for each face that has
  none (1594 faces), so that it shows from both sides with back face culling.
- Pairs of triangles of one texture in one plane (12 degrees) become quads.
- No point of the original moves. The outline does not change.
- `hum_mfg` (also `durnan.pof`): the two strips of the upper arm texture are
  exchanged between the front and back faces and the outer and inner faces
  (`zoo.FIXES`). The emblem of the pauldron of `humsmf02` is then on the outer side.
- `ognew5` (also `armogre.pof`), submodels 5 and 25 (the feet): each face of the
  original is there twice, once for each side, and the back of the heel is open
  (bugs.md D30). `weld()` kept one face of each pair, the inner one for 4 of the 7, so
  the foot of the files of before 2026-11-26 showed the floor through it.
  `zoo._drop_inner_twins` (`zoo.FIXES`) deletes the inner faces and closes the heel
  with the two triangles that keep the foot convex: 9 faces, no open edge. The knees
  and the shins get their caps from `cap()`. Only the two ogre files changed: a
  build of all 850 files to a scratch folder differs from the files of before in
  `ognew5.a952e08c.pom` and `armogre.a952e08c.pom` only.

Not closed: slits without area (2 or 3 edges) in 8 meshes (`all5`, `ognew5`, the five
`ske_*`, `smr_p`).

Not used: `zoo.auto(subdivide=True)` rounds the parts with one subdivision step. The
result changed the outlines and the faces too much and was dropped.
