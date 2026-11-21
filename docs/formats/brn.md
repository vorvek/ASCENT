# .BRN: segment visibility matrix ("brain" file)

One file per level (`NAME.BRN`), read and written by `brain.c`. It is a bit
matrix that answers "can segment A see segment B". The AI code uses it.

Tool: `python -I tools/sidefiles.py brn FILE` (add `--matrix` for a full bit picture)

## Layout

The file is always 80000 bytes: 800 rows of 100 bytes. There is no header.

    bit(A, B) = (file[A * 100 + B / 8] >> (B % 8)) & 1

Row A lists the segments that segment A sees. 800 is the segment limit of the
matrix (the trigger table in `.TIG` allows 900).

## Source

- Loader at 0x97780: allocates 80000 bytes, clears them, opens the brain file
  name (level name with extension `brn`, built at 0x496F5), reads 80000 bytes
  and sets a "have brain" flag. Log texts: "Brain file is %s", "Yes we do!".
- Lookup at 0x98110: exactly the formula above.
- Builder (log texts "Building Visibility Matrix", "Saws %d", "I just created a
  brain file, DON'T SAVE this level!!!"): for each pair of segments it casts a
  ray between the segment centers and sets the bit on success, then writes the
  80000 bytes.

## Verified

- All 28 files are 80000 bytes.
- In every file all diagonal bits 0..798 are set and row 799 is empty.
- The matrix is not symmetric (about 21000 unequal pairs of about 319000).

## Important observation: the shipped files are not per-level data

The 28 files are nearly identical. There are only 4 distinct contents, and they
differ from each other in at most 59 bytes:

- BANE, CRYPTS, MINE, MULHOR1, PRISON are identical.
- BAT, BONES, HUB1, HUB2, KOBOLDS, THIEF0, THIEF1, THIEF2, YPINN are identical (3 bytes differ from BANE).
- FLAME and IBBALAR are identical (59 bytes differ from BANE).
- The other 12 files are identical (58 bytes differ from BANE).

Every file describes 799 segments, including HUB4, whose highest trigger
segment is 71. So the shipped matrices come from one level and were copied to
all others. Which level they belong to is unknown. A port should not trust them
and should compute visibility itself.

## Unknown

- Which level the matrix was built from.
- A second lookup function at 0x9816D reads the same buffer as packed 4-bit
  values (row stride 400 bytes). That does not fit an 80000 byte buffer for 800
  segments. Its callers were not traced. It may be dead code (log text
  "Building Robot Map").
