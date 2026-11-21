#!/usr/bin/env python3
# Integer model of engine/fix/fix.asm and engine/vecmat/vecmat.asm.
#
# This file is not part of the build. It follows the assembly step by step
# with 32-bit and 64-bit integer arithmetic (no floating point) and writes
# the expected values that test_fix.c and test_vecmat.c compare against:
#
#   python engine/fix/asm_model.py
#
# writes engine/fix/test_fix_data.h and engine/vecmat/test_vecmat_data.h.
# The tables (sine, arc sine, arc cosine, square root guess) are read from
# fix.asm, so the model does not share any data with fix.c.
#
# A case where the assembly has a divide exception, reads outside a table,
# or does not end is not part of the expected values. The index of the case
# goes into a skip list.

import os
import re

M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF

HERE = os.path.dirname(os.path.abspath(__file__))


class Fault(Exception):
	pass


def u32(v):
	return v & M32


def s32(v):
	v &= M32
	return v - 0x100000000 if v & 0x80000000 else v


def s64(v):
	v &= M64
	return v - (1 << 64) if v & (1 << 63) else v


# ---------------------------------------------------------------------------
# Tables from fix.asm

def load_tables():
	words = []
	labels = {}
	guess = []
	with open(os.path.join(HERE, "fix.asm"), encoding="latin-1") as f:
		for line in f:
			line = line.split(";")[0].rstrip()
			m = re.match(r"^(\w+)?\s+dw\s+(-?\d+)$", line)
			if m:
				if m.group(1):
					labels[m.group(1)] = len(words)
				words.append(int(m.group(2)))
				continue
			m = re.match(r"^(\w+)?\s+db\s+(\d+)\s+dup\s+\((\d+)\)$", line)
			if m:
				guess += [int(m.group(3))] * int(m.group(2))
	assert len(guess) == 256
	assert labels["sin_table"] == 0 and labels["cos_table"] == 64
	assert labels["asin_table"] == 321 and labels["acos_table"] == 321 + 258
	assert len(words) == 321 + 258 + 258
	return words, labels, guess


W, LABELS, GUESS = load_tables()
SIN = LABELS["sin_table"]
COS = LABELS["cos_table"]
ASIN = LABELS["asin_table"]
ACOS = LABELS["acos_table"]


# ---------------------------------------------------------------------------
# Processor instructions

def imul(a, b):
	# imul r32: edx:eax = eax * r32. The result is one 64-bit value.
	return (s32(a) * s32(b)) & M64


def idiv(n64, d):
	# idiv r32: eax = edx:eax / r32, truncated toward zero.
	n = s64(n64)
	d = s32(d)
	if d == 0:
		raise Fault("divide by zero")
	q = abs(n) // abs(d)
	if (n < 0) != (d < 0):
		q = -q
	if q > 0x7FFFFFFF or q < -0x80000000:
		raise Fault("divide overflow")
	return u32(q)


def div(n, d, bits):
	# div r16 or div r32: unsigned quotient and remainder.
	if d == 0:
		raise Fault("divide by zero")
	q = n // d
	if q >> bits:
		raise Fault("divide overflow")
	return q, n % d


def fixmul(a, b):
	# imul / shrd eax,edx,16
	return (imul(a, b) >> 16) & M32


def fixdiv(a, d):
	# mov edx,eax / sar edx,16 / shl eax,16 / idiv
	return idiv((s32(a) << 16) & M64, d)


def abs_eax(eax):
	# cdq / xor eax,edx / sub eax,edx. Returns eax and edx.
	edx = M32 if eax & 0x80000000 else 0
	return u32((eax ^ edx) - edx), edx


# ---------------------------------------------------------------------------
# fix.asm

def fix_fastsincos(ax):
	i = (ax >> 8) & 0xFF
	return u32(W[SIN + i] << 2), u32(W[COS + i] << 2)


def fix_sincos(ax):
	i = (ax >> 8) & 0xFF
	frac = ax & 0xFF
	out = []
	for tab in (SIN, COS):
		a = W[tab + i]
		b = W[tab + i + 1]
		out.append(u32((a + (((b - a) * frac) >> 8)) << 2))
	return out[0], out[1]


def table_interp(tab, eax):
	if s32(eax) > 0x10000:
		eax = 0x10000
	frac = eax & 0xFF
	i = s32(eax) >> 8
	if i < 0:
		raise Fault("index outside the table")
	a = W[tab + i]
	b = W[tab + i + 1]
	return u32(a + (((b - a) * frac) >> 8))


def fix_acos(eax):
	eax, sign = abs_eax(eax)
	eax = table_interp(ACOS, eax)
	eax = u32((eax ^ sign) - sign)
	return u32(eax + (sign & 0x8000))


def fix_asin(eax):
	eax, sign = abs_eax(eax)
	eax = table_interp(ASIN, eax)
	return u32((eax ^ sign) - sign)


def newton(n, guess, bits, unrolled):
	mask = (1 << bits) - 1
	bx = guess
	for _ in range(unrolled):
		q, r = div(n, bx, bits)
		bx = (bx + q) >> 1		# add / rcr: the carry comes back in
	while True:
		q, r = div(n, bx, bits)
		if q == bx:
			return q
		di = bx
		bx = (bx + q) >> 1
		if bx == q or bx == di:
			break
	if r:
		bx = (bx + 1) & mask
	return bx


def long_sqrt(eax):
	if s32(eax) <= 0:
		return 0
	for shift, cl in ((24, 12), (16, 8), (8, 4), (0, 0)):
		bl = (eax >> shift) & 0xFF
		if bl or shift == 0:
			break
	return newton(eax, GUESS[bl] << cl, 16, 2)


def quad_sqrt(edx, eax):
	if edx & 0x80000000:
		return 0
	if edx == 0 and not (eax & 0x80000000):
		return long_sqrt(eax)
	for shift, cl in ((24, 28), (16, 24), (8, 20), (0, 16)):
		bl = (edx >> shift) & 0xFF
		if bl or shift == 0:
			break
	return newton((edx << 32) | eax, u32(GUESS[bl] << cl), 32, 3)


def fix_sqrt(eax):
	return long_sqrt(eax) << 8


def fix_atan2(cos, sin):
	acos_, _ = abs_eax(cos)
	asin_, _ = abs_eax(sin)
	q = (imul(cos, cos) + imul(sin, sin)) & M64
	mag = quad_sqrt(q >> 32, q & M32)
	if s32(acos_) < s32(asin_):
		eax = fix_acos(fixdiv(cos, mag))
		sign = M32 if sin & 0x80000000 else 0
		return u32((eax ^ sign) - sign) & 0xFFFF
	if mag == 0:
		return sin & 0xFFFF
	eax = fix_asin(fixdiv(sin, mag))
	if cos & 0x80000000:
		eax = u32(-(eax - 0x8000))
	return eax & 0xFFFF


# ---------------------------------------------------------------------------
# vecmat.asm. A vector is a list of three 32-bit values. A matrix is a list
# of nine values in memory order: m1 m4 m7 (rvec), m2 m5 m8 (uvec),
# m3 m6 m9 (fvec).

M1, M4, M7, M2, M5, M8, M3, M6, M9 = range(9)


def vadd(a, b):
	return [u32(a[i] + b[i]) for i in range(3)]


def vsub(a, b):
	return [u32(a[i] - b[i]) for i in range(3)]


def vavg(a, b):
	return [u32(s32(a[i] + b[i]) >> 1) for i in range(3)]


def vscale(v, k):
	return [fixmul(c, k) for c in v]


def vscale_add(a, b, k):
	return [u32(fixmul(b[i], k) + a[i]) for i in range(3)]


def vscale2(v, n, d):
	if d == 0:
		raise Fault("the assembly returns with a bad stack")
	return [idiv(imul(c, n), d) for c in v]


def vmag(v):
	q = sum(imul(c, c) for c in v) & M64
	return quad_sqrt(q >> 32, q & M32)


def vdist(a, b):
	return vmag(vsub(a, b))


def neg_if_sign(v):
	v = s32(v)
	return s32(-v) if v < 0 else v


def mag_quick(a, b, c):
	if not a > b:
		a, b = b, a
	if not b > c:
		b, c = c, b
		if not a > b:
			a, b = b, a
	b >>= 2
	c >>= 3
	b = s32(b + c)
	a = s32(a + b)
	b >>= 1
	return u32(a + b)


def vmag_quick(v):
	return mag_quick(neg_if_sign(v[0]), neg_if_sign(v[1]), neg_if_sign(v[2]))


def vdist_quick(a, b):
	d = vsub(a, b)
	return mag_quick(neg_if_sign(d[2]), neg_if_sign(d[0]), neg_if_sign(d[1]))


def copy_normalize(src, dest):
	mag = vmag(src)
	if mag == 0:
		return dest, 0
	return [fixdiv(c, mag) for c in src], mag


def copy_normalize_quick(src, dest):
	mag = vmag_quick(src)
	if mag == 0:
		return dest, 0
	return [fixdiv(c, mag) for c in src], mag


def normalized_dir(end, start):
	d = vsub(end, start)
	return copy_normalize(d, d)


def normalized_dir_quick(end, start):
	d = vsub(end, start)
	return copy_normalize_quick(d, d)


def dot(a, b):
	q = sum(imul(a[i], b[i]) for i in range(3)) & M64
	eax = (q >> 16) & M32
	ebx = u32(s32(q >> 32) >> 16)
	dx = 0xFFFF if eax & 0x80000000 else 0
	if (ebx & 0xFFFF) != dx:
		eax = 0x7FFFFFFF
		if ebx & 0x80000000:
			eax = u32(-eax)
	return eax


def cross(s0, s1):
	def term(p, q, r, s):
		return (((imul(p, q) - imul(r, s)) & M64) >> 16) & M32
	return [
		term(s1[2], s0[1], s1[1], s0[2]),
		term(s1[0], s0[2], s1[2], s0[0]),
		term(s1[1], s0[0], s1[0], s0[1]),
	]


def check_vec(v):
	ebx = 0
	for c in v:
		ebx |= abs_eax(c)[0]
	if ebx == 0:
		return v
	cl = 0
	if ebx & 0xFFFC0000:
		if ebx & 0x80000000:
			raise Fault("sar keeps the sign bit, the loop does not end")
		while ebx & 0x00F00000:
			cl += 4
			ebx >>= 4
		while ebx & 0xFFFC0000:
			cl += 2
			ebx >>= 2
		return [u32(s32(c) >> cl) for c in v]
	if not ebx & 0xFFFF8000:
		while not ebx & 0xFFFFF000:
			cl += 4
			ebx = u32(ebx << 4)
		while not ebx & 0xFFFF8000:
			cl += 2
			ebx = u32(ebx << 2)
		return [u32(c << cl) for c in v]
	return v


def perp(p0, p1, p2):
	t1 = vsub(p2, p1)
	t0 = vsub(p1, p0)
	return cross(check_vec(t0), check_vec(t1))


def normal(p0, p1, p2):
	v = perp(p0, p1, p2)
	return copy_normalize(v, v)[0]


def sincos_2_matrix(sinp, cosp, sinb, cosb, sinh, cosh):
	m = [0] * 9
	sbsh = fixmul(sinb, sinh)
	cbch = fixmul(cosb, cosh)
	m[M1] = u32(cbch + fixmul(sbsh, sinp))
	m[M8] = u32(fixmul(cbch, sinp) + sbsh)
	cbsh = fixmul(cosb, sinh)
	sbch = fixmul(sinb, cosh)
	m[M2] = u32(fixmul(cbsh, sinp) - sbch)
	m[M7] = u32(fixmul(sbch, sinp) - cbsh)
	m[M3] = fixmul(sinh, cosp)
	m[M4] = fixmul(sinb, cosp)
	m[M5] = fixmul(cosb, cosp)
	m[M6] = u32(-sinp)
	m[M9] = fixmul(cosh, cosp)
	return m


def angles_2_matrix(p, b, h):
	sinp, cosp = fix_sincos(p)
	sinb, cosb = fix_sincos(b)
	sinh, cosh = fix_sincos(h)
	return sincos_2_matrix(sinp, cosp, sinb, cosb, sinh, cosh)


def vec_ang_2_matrix(v, ax):
	sinb, cosb = fix_sincos(ax)
	sinp = u32(-v[1])
	cosp = fix_sqrt(u32(0x10000 - fixmul(sinp, sinp)))
	sinh = fixdiv(v[0], cosp)
	cosh = fixdiv(v[2], cosp)
	return sincos_2_matrix(sinp, cosp, sinb, cosb, sinh, cosh)


def normalize(v):
	return copy_normalize(v, v)


def vector_2_matrix(m, f, u, r):
	# m is the matrix before the call. The assembly leaves it alone when
	# the forward vector has no length.
	def just_forward(src):
		z, mag = normalize(src)
		if mag == 0:
			return m
		if (z[0] | z[2]) == 0:
			m8 = 0x10000 if z[1] & 0x80000000 else u32(-0x10000)
			return [0x10000, 0, 0, 0, 0, m8] + z
		x, _ = normalize([z[2], 0, u32(-z[0])])
		y = cross(z, x)
		return x + y + z

	if u is not None:
		z, mag = normalize(f)
		if mag == 0:
			return just_forward(z)
		y, mag = normalize(u)
		if mag == 0:
			return just_forward(z)
		x, mag = normalize(cross(y, z))
		if mag == 0:
			return just_forward(z)
		y = cross(z, x)
		return x + y + z
	if r is not None:
		z, mag = normalize(f)
		if mag == 0:
			return just_forward(z)
		x, mag = normalize(r)
		if mag == 0:
			return just_forward(z)
		y, mag = normalize(cross(z, x))
		if mag == 0:
			return just_forward(z)
		x = cross(y, z)
		return x + y + z
	return just_forward(f)


def rotate(s, m):
	def row(a, b, c):
		q = (imul(s[0], m[a]) + imul(s[1], m[b]) + imul(s[2], m[c])) & M64
		return (q >> 16) & M32
	return [row(M1, M4, M7), row(M2, M5, M8), row(M3, M6, M9)]


def matrix_x_matrix(s0, s1):
	rows = ((M1, M2, M3), (M4, M5, M6), (M7, M8, M9))
	cols = ((M1, M4, M7), (M2, M5, M8), (M3, M6, M9))
	names = ((M1, M2, M3), (M4, M5, M6), (M7, M8, M9))
	d = [0] * 9
	for i in range(3):
		for j in range(3):
			q = sum(imul(s0[rows[i][k]], s1[cols[j][k]]) for k in range(3)) & M64
			d[names[i][j]] = (q >> 16) & M32
	return d


def transpose(m):
	d = list(m)
	d[M2], d[M4] = m[M4], m[M2]
	d[M3], d[M7] = m[M7], m[M3]
	d[M6], d[M8] = m[M8], m[M6]
	return d


def delta_ang_norm(v0, v1, fvec):
	eax = fix_acos(dot(v0, v1))
	if fvec is not None:
		d = dot(cross(v0, v1), fvec)
		sign = M32 if d & 0x80000000 else 0
		eax = u32((eax ^ sign) - sign)
	return eax & 0xFFFF


def delta_ang(v0, v1, fvec):
	v0, _ = normalize(v0)
	v1, _ = normalize(v1)
	return delta_ang_norm(v0, v1, fvec), v0, v1


def dist_to_plane(checkp, norm, planep):
	return dot(vsub(checkp, planep), norm)


def extract_angles_matrix(m):
	eax = m[M9]
	ebx = m[M3]
	head = fix_atan2(eax, ebx) if (eax | ebx) else 0
	sinh, cosh = fix_sincos(head)
	if s32(abs_eax(cosh)[0]) > s32(abs_eax(sinh)[0]):
		cp = fixdiv(m[M9], cosh)
	else:
		cp = fixdiv(m[M3], sinh)
	ebx = u32(-m[M6])
	pitch = fix_atan2(cp, ebx) if (cp | ebx) else 0
	if cp == 0:
		bank = 0
	else:
		sb = fixdiv(m[M4], cp)
		cb = fixdiv(m[M5], cp)
		bank = fix_atan2(cb, sb) if (cb | sb) else 0
	return [pitch, bank, head]


def extract_angles_vector_normalized(v):
	pitch = fix_asin(u32(-v[1])) & 0xFFFF
	head = fix_atan2(v[2], v[0]) if (v[0] | v[2]) else 0
	return [pitch, 0, head]


def extract_angles_vector(v, a):
	t, mag = copy_normalize(v, None)
	if mag == 0:
		return a
	return extract_angles_vector_normalized(t)


# ---------------------------------------------------------------------------
# Test streams. test_fix.c and test_vecmat.c draw the same values with the
# same generator and fold the results into the same checksum.

class Rng:
	def __init__(self, seed):
		self.state = seed

	def next(self):
		self.state = (self.state * 1664525 + 1013904223) & M32
		return self.state

	def fix(self):
		# A signed value with a random magnitude.
		r = self.next()
		sh = self.next() >> 27
		return u32(s32(r) >> sh)

	def vec(self):
		return [self.fix(), self.fix(), self.fix()]

	def ang(self):
		return self.next() >> 16


class Stream:
	def __init__(self, name, seed, count):
		self.name = name
		self.rng = Rng(seed)
		self.count = count
		self.sum = 2166136261
		self.skip = []

	def add(self, words):
		for w in words:
			self.sum = ((self.sum ^ (w & M32)) * 16777619) & M32

	def run(self, draw, calc):
		for i in range(self.count):
			args = draw(self.rng)
			try:
				words = calc(*args)
			except Fault:
				self.skip.append(i)
				continue
			self.add(words)
		return self

	def emit(self, out):
		out.append("#define N_%s %d" % (self.name, self.count))
		out.append("#define SUM_%s 0x%08Xu" % (self.name, self.sum))
		out.append("static const int SKIP_%s[] = {%s-1};" % (
			self.name, "".join("%d," % i for i in self.skip)))


SENT_VEC = [0x11111111] * 3
SENT_MAT = [0x22222222] * 9
SENT_ANG = [0x3333] * 3


def hexs(v):
	return "0x%08Xu" % (v & M32)


def table(out, decl, rows):
	out.append("static const %s[] = {" % decl)
	for row in rows:
		out.append("\t{" + ",".join(
			"{" + ",".join(hexs(w) for w in v) + "}" if isinstance(v, list)
			else hexs(v) for v in row) + "},")
	out.append("};")


HEADER = [
	"// Expected values for the tests. Written by engine/fix/asm_model.py,",
	"// an integer model of the assembly. Do not edit this file.",
	"",
]


def gen_fix():
	out = list(HEADER)

	# Each angle.
	s = Stream("SINCOS", 0, 65536)
	a = iter(range(65536))
	s.run(lambda r: (next(a),),
		lambda a: list(fix_sincos(a)) + list(fix_fastsincos(a)))
	s.emit(out)

	# Each input from -1.0 to 1.0 and a short distance past the two ends.
	s = Stream("ASINCOS", 0, 2 * 0x10400 + 1)
	v = iter(range(-0x10400, 0x10401))
	s.run(lambda r: (u32(next(v)),),
		lambda v: [fix_asin(v) & 0xFFFF, fix_acos(v) & 0xFFFF])
	s.emit(out)

	s = Stream("ASINCOS_RND", 0x1001, 20000)
	s.run(lambda r: (r.fix(),),
		lambda v: [fix_asin(v) & 0xFFFF, fix_acos(v) & 0xFFFF])
	s.emit(out)

	s = Stream("SQRT_LOW", 0, 0x30000)
	v = iter(range(0x30000))
	s.run(lambda r: (next(v),), lambda v: [long_sqrt(v), fix_sqrt(v)])
	s.emit(out)

	s = Stream("SQRT_RND", 0x1002, 50000)
	s.run(lambda r: (r.fix(),), lambda v: [long_sqrt(v), fix_sqrt(v)])
	s.emit(out)

	def draw_quad(r):
		lo = r.next()
		hi = r.next() >> 1
		return lo, hi >> (r.next() >> 27)
	s = Stream("QUAD_RND", 0x1003, 50000)
	s.run(draw_quad, lambda lo, hi: [quad_sqrt(hi, lo)])
	s.emit(out)

	s = Stream("ATAN2_RND", 0x1004, 50000)
	s.run(lambda r: (r.fix(), r.fix()), lambda c, sn: [fix_atan2(c, sn)])
	s.emit(out)

	# Small tables with the values in the open, for edge cases.
	angs = [0, 1, 0x7F, 0x80, 0xFF, 0x100, 0x1234, 0x2000, 0x3FFF, 0x4000,
		0x4001, 0x40FF, 0x6000, 0x7FFF, 0x8000, 0x8001, 0xA5A5, 0xBFFF,
		0xC000, 0xC001, 0xFF00, 0xFFFF]
	table(out, "struct { uint32_t a,s,c,fs,fc; } sincos_cases",
		[[a] + list(fix_sincos(a)) + list(fix_fastsincos(a)) for a in angs])

	vals = [0, 1, -1, 0xFF, 0x100, 0x101, 0x8000, -0x8000, 0xB505, -0xB505,
		0xFF00, 0xFFFF, -0xFFFF, 0x10000, -0x10000, 0x10001, -0x10001,
		0x7FFFFFFF, -0x7FFFFFFF]
	table(out, "struct { uint32_t v,asin,acos; } asincos_cases",
		[[v, fix_asin(u32(v)) & 0xFFFF, fix_acos(u32(v)) & 0xFFFF] for v in vals])

	vals = [0, -1, -0x80000000, 1, 2, 3, 4, 5, 8, 9, 15, 16, 17, 24, 25, 99,
		100, 255, 256, 257, 0xFFFF, 0x10000, 0x10001, 0x40000, 0xFFFFFF,
		0x1000000, 0x1000001, 0x3FFFFFFF, 0x40000000, 0x7FFE0001,
		0x7FFE0002, 0x7FFFFFFF]
	table(out, "struct { uint32_t v,lsqrt,fsqrt; } sqrt_cases",
		[[v, long_sqrt(u32(v)), fix_sqrt(u32(v))] for v in vals])

	quads = [(0, 0), (1, 0), (0x7FFFFFFF, 0), (0x80000000, 0),
		(0xFFFFFFFF, 0), (0, 1), (1, 1), (0, 0x100), (0xFFFFFFFF, 0xFF),
		(0, 0x10000), (0, 0x1000000), (0, 0x3FFFFFFF), (1, 0x3FFFFFFF),
		(0, 0x40000000), (0xFFFFFFFF, 0x7FFFFFFF), (0, 0x80000000),
		(0xFFFFFFFF, 0xFFFFFFFF), (0x00000001, 0xFFFFFFFE >> 1),
		(0xFFFE0001, 0), (0xFFFFFFFE, 0x0FFFFFFF)]
	table(out, "struct { uint32_t lo,hi,r; } quad_cases",
		[[lo, hi, quad_sqrt(hi, lo)] for lo, hi in quads])

	f1 = 0x10000
	big = 0x7FFFFFFF
	mn = -0x80000000
	pairs = [(f1, 0), (0, f1), (-f1, 0), (0, -f1), (f1, f1), (-f1, f1),
		(-f1, -f1), (f1, -f1), (1, 0), (0, 1), (-1, 0), (0, -1), (1, 1),
		(3, 4), (-4, 3), (0, 0), (big, big), (big, -big), (-big, 1),
		(big, 0), (0, big), (mn, 0), (0, mn), (mn, mn), (mn, big),
		(0x12345, -0x54321), (-0x7000000, 0x123), (0x123, -0x7000000),
		(46340, 46341), (46341, 46340)]
	rows = []
	for c, sn in pairs:
		try:
			rows.append([c, sn, fix_atan2(u32(c), u32(sn))])
		except Fault:
			pass
	table(out, "struct { uint32_t c,s,r; } atan2_cases", rows)

	return out


def gen_vecmat():
	out = list(HEADER)

	def basic(a, b, k):
		return (vadd(a, b) + vsub(a, b) + vavg(a, b) + vscale(a, k) +
			vscale_add(a, b, k) + [vmag(a), vdist(a, b), vmag_quick(a),
			vdist_quick(a, b), dot(a, b)] + cross(a, b))
	s = Stream("BASIC", 0x2001, 20000)
	s.run(lambda r: (r.vec(), r.vec(), r.fix()), basic)
	s.emit(out)

	# Vectors near unit length, where the dot product does not saturate.
	def draw_small(r):
		a = [u32(s32(r.next()) >> 14) for _ in range(3)]
		b = [u32(s32(r.next()) >> 12) for _ in range(3)]
		return a, b, u32(s32(r.next()) >> 13)
	s = Stream("BASIC_SMALL", 0x2002, 20000)
	s.run(draw_small, basic)
	s.emit(out)

	def norm(a, b):
		w = []
		for v, mag in (copy_normalize(a, SENT_VEC),
				copy_normalize_quick(a, SENT_VEC),
				normalized_dir(a, b), normalized_dir_quick(a, b)):
			w += v + [mag]
		return w
	s = Stream("NORM", 0x2003, 20000)
	s.run(lambda r: (r.vec(), r.vec()), norm)
	s.emit(out)

	s = Stream("PERP", 0x2004, 20000)
	s.run(lambda r: (r.vec(), r.vec(), r.vec()),
		lambda p0, p1, p2: perp(p0, p1, p2) + normal(p0, p1, p2))
	s.emit(out)

	def mat(a1, a2, v):
		m1 = angles_2_matrix(*a1)
		m2 = angles_2_matrix(*a2)
		mm = matrix_x_matrix(m1, m2)
		return (m1 + rotate(v, m1) + mm + transpose(mm) +
			extract_angles_matrix(m1) + extract_angles_matrix(mm))
	s = Stream("MATRIX", 0x2005, 10000)
	s.run(lambda r: ([r.ang(), r.ang(), r.ang()],
		[r.ang(), r.ang(), r.ang()], r.vec()), mat)
	s.emit(out)

	def draw_v2m(r):
		f = r.vec()
		u = r.vec()
		rv = r.vec()
		mode = r.next() >> 30
		sel = r.next() >> 28
		if sel == 0:
			f[0] = f[2] = 0
		elif sel == 1:
			u = [0, 0, 0]
		elif sel == 2:
			rv = [0, 0, 0]
		elif sel == 3:
			u = list(f)
		elif sel == 4:
			rv = list(f)
		elif sel == 5:
			f = [0, 0, 0]
		return f, (u if mode & 1 else None), (rv if mode & 2 else None)
	s = Stream("V2M", 0x2006, 20000)
	s.run(draw_v2m, lambda f, u, r: vector_2_matrix(SENT_MAT, f, u, r))
	s.emit(out)

	def dang(v0, v1, f, usef):
		a, n0, n1 = delta_ang(v0, v1, f if usef else None)
		return [a] + n0 + n1 + [delta_ang_norm(n0, n1, f if usef else None)]
	s = Stream("DELTA_ANG", 0x2007, 20000)
	s.run(lambda r: (r.vec(), r.vec(), r.vec(), r.next() >> 31), dang)
	s.emit(out)

	def draw_a2m(r):
		x = u32(s32(r.next()) >> 15)
		y = u32(s32(r.next()) >> 16)
		z = u32(s32(r.next()) >> 15)
		return [x, y, z], r.ang()
	s = Stream("ANG_2_MATRIX", 0x2008, 10000)
	s.run(draw_a2m, vec_ang_2_matrix)
	s.emit(out)

	s = Stream("EXTRACT_VEC", 0x2009, 20000)
	s.run(lambda r: (r.vec(),), lambda v: extract_angles_vector(v, SENT_ANG))
	s.emit(out)

	s = Stream("PLANE", 0x200A, 20000)
	s.run(lambda r: (r.vec(), r.vec(), r.vec()),
		lambda c, n, p: [dist_to_plane(c, n, p)])
	s.emit(out)

	def draw_scale2(r):
		v = r.vec()
		n = u32(s32(r.next()) >> 16)
		return v, n, r.next() | 0x10000
	s = Stream("SCALE2", 0x200B, 20000)
	s.run(draw_scale2, vscale2)
	s.emit(out)

	# Small tables with the values in the open, for edge cases.
	f1 = 0x10000
	big = 0x7FFFFFFF
	mn = 0x80000000
	vecs = [[0, 0, 0], [f1, 0, 0], [0, u32(-f1), 0], [3 * f1, 4 * f1, 0],
		[1, 0, 0], [1, 1, 1], [u32(-1), 2, u32(-2)],
		[0x7FFF, 0x7FFF, 0x7FFF], [0x8000, 0x8000, 0],
		[0xB505, 0xB505, 0xB505], [0x10000000, 0x20000000, 0x30000000],
		[big, 0, 0], [big, big, big], [u32(-big), big, u32(-big)],
		[mn, 0, 0], [mn, mn, mn], [0x40000000, 0x40000000, 0],
		[0x123456, u32(-0x654321), 0xABCDE]]
	rows = []
	for v in vecs:
		n, mag = copy_normalize(v, SENT_VEC)
		nq, magq = copy_normalize_quick(v, SENT_VEC)
		rows.append([v, mag, n, magq, nq])
	table(out, "struct { uint32_t v[3],mag,n[3],magq,nq[3]; } norm_cases", rows)

	pairs = [([f1, 0, 0], [0, f1, 0]), ([f1, 2 * f1, 3 * f1], [4 * f1, 5 * f1, 6 * f1]),
		([u32(-f1), 2 * f1, u32(-3 * f1)], [4 * f1, u32(-5 * f1), 6 * f1]),
		([1, 1, 1], [1, 1, 1]), ([u32(-1), 0, 0], [1, 0, 0]),
		([0x7FFF, 0, 0], [1, 0, 0]), ([u32(-0x7FFF), 0, 0], [3, 0, 0]),
		([big, big, big], [big, big, big]),
		([big, big, big], [u32(-big), u32(-big), u32(-big)]),
		([mn, 0, 0], [mn, 0, 0]), ([mn, mn, mn], [big, big, big]),
		([0x00B50000, 0, 0], [0x00B50000, 0, 0]),
		([0x00B60000, 0, 0], [0x00B60000, 0, 0]),
		([0x00B60000, 0, 0], [u32(-0x00B60000), 0, 0]),
		([0x01000000, 0x01000000, 0], [0x00400000, 0x003FFFFF, 0]),
		([0x01000000, 0x01000000, 0], [0x00400000, 0x00400000, 0]),
		([0x01000000, 0x01000000, 0], [u32(-0x00400000), u32(-0x00400000), u32(-1)]),
		([0x123456, u32(-0x654321), 0xABCDE], [u32(-0x1111), 0x22222, 0x333333])]
	table(out, "struct { uint32_t a[3],b[3],dot,cross[3]; } dot_cases",
		[[a, b, dot(a, b), cross(a, b)] for a, b in pairs])

	angs = [[0, 0, 0], [0x4000, 0, 0], [0, 0x4000, 0], [0, 0, 0x4000],
		[0x8000, 0, 0], [0, 0, 0x8000], [0xC000, 0x4000, 0x8000],
		[0x1234, 0x5678, 0x9ABC], [0xFFFF, 0x0001, 0x7FFF],
		[0x2000, 0xE000, 0x6000], [0x3FFF, 0, 0x1000], [0x4000, 0x1000, 0x2000]]
	table(out, "struct { uint32_t a[3],m[9],e[3]; } angle_cases",
		[[a, angles_2_matrix(*a), extract_angles_matrix(angles_2_matrix(*a))]
			for a in angs])

	return out


def write(path, lines):
	with open(path, "w", newline="\n") as f:
		f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
	write(os.path.join(HERE, "test_fix_data.h"), gen_fix())
	write(os.path.join(HERE, "..", "vecmat", "test_vecmat_data.h"), gen_vecmat())
