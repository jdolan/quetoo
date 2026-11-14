import re
import tempfile
import unittest
from fractions import Fraction
from itertools import combinations
from pathlib import Path

import brushfu
from brushfu import BrushError, Hallway, build_curve, build_straight

FACE = re.compile(r"\(\s*(-?\d+)\s+(-?\d+)\s+(-?\d+)\s*\)\s*\(\s*(-?\d+)\s+(-?\d+)\s+(-?\d+)\s*\)\s*"
                  r"\(\s*(-?\d+)\s+(-?\d+)\s+(-?\d+)\s*\)")


def sub(a, b):
  return tuple(x - y for x, y in zip(a, b))


def cross(a, b):
  return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def dot(a, b):
  return sum(x * y for x, y in zip(a, b))


def parse_planes(lines):
  planes = []
  for line in lines:
    v = [int(c) for c in FACE.match(line).groups()]
    a, b, c = tuple(v[0:3]), tuple(v[3:6]), tuple(v[6:9])
    n = cross(sub(c, a), sub(b, a))
    planes.append((n, dot(n, a)))
  return planes


def brush_vertices(planes):
  """Intersects every plane triple exactly, and keeps the corners inside every plane."""
  found = set()
  for (n1, d1), (n2, d2), (n3, d3) in combinations(planes, 3):
    det = dot(n1, cross(n2, n3))
    if not det:
      continue
    # Cramer's rule
    rows, ds, p = [n1, n2, n3], [d1, d2, d3], []
    for k in range(3):
      m = [list(r) for r in rows]
      for r in range(3):
        m[r][k] = ds[r]
      p.append(Fraction(dot(m[0], cross(m[1], m[2])), det))
    p = tuple(p)
    if all(dot(n, p) <= d for n, d in planes):
      found.add(p)
  return found


def points(group):
  return {p for brush in group for p in brush.points}


TEXTURE = "quake/city2_6"


class BrushfuTest(unittest.TestCase):
  @classmethod
  def setUpClass(cls):
    cls.hallway = Hallway()
    cls.curve = build_curve(cls.hallway, 90, 8)
    cls.straight = build_straight(cls.hallway, 128)

  def assertRecoversPoints(self, hall):
    for group in hall.groups:
      for brush in group:
        corners = brush_vertices(parse_planes(brush.lines))
        self.assertTrue(all(c.denominator == 1 for p in corners for c in p), brush.lines)
        self.assertEqual({tuple(int(c) for c in p) for p in corners}, set(brush.points))

  def test_curve_brushes_are_integer_tetrahedra_without_inversions(self):
    self.assertEqual(self.curve.stats["inverted"], 0)
    self.assertLess(self.curve.stats["radial"], 1.0)
    faces = {}
    for group in self.curve.groups:
      for brush in group:
        self.assertEqual(len(brush.points), 4)
        for tri in combinations(sorted(brush.points), 3):
          faces[tri] = faces.get(tri, 0) + 1
    self.assertLessEqual(max(faces.values()), 2)
    self.assertRecoversPoints(self.curve)

  def test_curve_enters_and_leaves_square(self):
    self.assertEqual(len(self.curve.groups), 8)
    first, last = points(self.curve.groups[0]), points(self.curve.groups[-1])
    self.assertEqual(min(p[1] for p in first), 0)
    self.assertEqual(max(p[0] for p in first), 320)
    self.assertEqual(min(p[0] for p in last), 0)
    self.assertEqual(max(p[1] for p in last), 320)

  def test_curve_turns_the_requested_angle(self):
    for degrees, segments in ((45, 4), (180, 12)):
      curve = build_curve(self.hallway, degrees, segments)
      self.assertEqual(curve.stats["inverted"], 0)
      self.assertLess(curve.stats["radial"], 1.0)
      end = points(curve.groups[-1])
      if degrees == 180:
        self.assertEqual(min(p[0] for p in end), -320)
        self.assertEqual({p[1] for p in end if p[0] == -320}, {0})

  def test_other_parameters_stay_integer(self):
    for kwargs, segments in (({"radius": 64}, 4), ({"width": 240, "wall": 24, "arch_top": 64}, 5),
                           ({"arch_top": 64, "arch_bottom": 32, "facets_bottom": 8}, 4)):
      curve = build_curve(Hallway(**kwargs), 90, segments)
      self.assertEqual(curve.stats["inverted"], 0)
      self.assertRecoversPoints(curve)

  def test_straight_is_centered_prisms(self):
    self.assertEqual(len(self.straight.groups), 1)
    brushes = self.straight.groups[0]
    self.assertEqual(len(brushes), 2 + self.hallway.facets_top)
    pts = points(brushes)
    self.assertEqual((min(p[0] for p in pts), max(p[0] for p in pts)), (-64, 64))
    self.assertEqual((min(p[1] for p in pts), max(p[1] for p in pts)), (-160, 160))
    self.assertEqual((min(p[2] for p in pts), max(p[2] for p in pts)), (0, self.hallway.height))
    self.assertRecoversPoints(self.straight)

  def test_straight_shares_the_curves_cross_section(self):
    # the straight's rho runs toward -Y from +160, so a quarter turn and a move map it onto the curve
    straight = points(self.straight.groups[0])
    section = {(160 - p[1], p[2]) for p in straight if p[0] == -64}
    self.assertEqual(section, {(160 - p[1], p[2]) for p in straight if p[0] == 64})
    entry = {(p[0], p[2]) for p in points(self.curve.groups[0]) if p[1] == 0}
    exit_ = {(p[1], p[2]) for p in points(self.curve.groups[-1]) if p[0] == 0}
    self.assertEqual(section, entry)
    self.assertEqual(section, exit_)

  def texels(self, hall, scale=0.25):
    """Every textured corner's texture coordinates, as every textured face computes them."""
    texel = {}
    for group in hall.groups:
      for brush in group:
        for line in brush.lines:
          fields = line.split()
          if fields[15] != TEXTURE:
            continue
          self.assertEqual(fields[-2:], [f"{scale:g}"] * 2)
          axes, offsets = [], []
          for text in re.findall(r"\[ (.*?) \]", line):
            nums = [float(x) for x in text.split()]
            axes.append(nums[:3])
            offsets.append(nums[3])
          for corner in range(3):
            p = tuple(int(c) for c in fields[5 * corner + 1:5 * corner + 4])
            uv = tuple(sum(a * c for a, c in zip(ax, p)) / scale + off for ax, off in zip(axes, offsets))
            texel.setdefault(p, []).append(uv)
    self.assertTrue(texel)
    return texel

  def assertContinuous(self, texel):
    for p, uvs in texel.items():
      for uv in uvs:
        self.assertAlmostEqual(uv[0], uvs[0][0], places=3, msg=p)
        self.assertAlmostEqual(uv[1], uvs[0][1], places=3, msg=p)

  def test_curve_texture_is_continuous_across_every_shared_edge(self):
    for scale in (0.25, 0.5):
      self.assertContinuous(self.texels(build_curve(self.hallway, 90, 6, TEXTURE, scale), scale))

  def test_straight_texture_is_continuous_and_matches_the_curves_v(self):
    straight = self.texels(build_straight(self.hallway, 64, TEXTURE))
    self.assertContinuous(straight)
    curve = self.texels(build_curve(self.hallway, 90, 6, TEXTURE))
    seam = {(p[0], p[2]): round(uvs[0][1], 3) for p, uvs in curve.items() if p[1] == 0}
    ends = {(160 - p[1], p[2]): round(uvs[0][1], 3) for p, uvs in straight.items() if p[0] == -32}
    self.assertEqual(seam, ends)

  def test_bottom_arch_closes_the_floor_asymmetrically(self):
    h = Hallway(arch_top=96, arch_bottom=32, facets_bottom=8)
    straight = build_straight(h, 64, TEXTURE)
    brushes = straight.groups[0]
    self.assertEqual(len(brushes), 2 + 16 + 8)
    pts = points(brushes)
    self.assertEqual((min(p[2] for p in pts), max(p[2] for p in pts)), (0, h.height))
    # the interior crowns are the arches' rises from the spring lines, a wall inside the outside
    inside = {p[2] for p in pts if p[1] == 0}
    self.assertEqual((min(inside), max(inside)), (0, h.height))
    crowns = sorted(p[2] for p in pts if p[0] == -32 and p[1] == 0 and p[2] in (32, h.height - 32))
    self.assertEqual(crowns, [32, h.height - 32])
    self.assertRecoversPoints(straight)
    self.assertContinuous(self.texels(straight))

    curve = build_curve(h, 90, 6, TEXTURE)
    self.assertEqual(curve.stats["inverted"], 0)
    self.assertRecoversPoints(curve)
    self.assertContinuous(self.texels(curve))
    faces = {}
    for group in curve.groups:
      for brush in group:
        for tri in combinations(sorted(brush.points), 3):
          faces[tri] = faces.get(tri, 0) + 1
    self.assertLessEqual(max(faces.values()), 2)

    section = {(160 - p[1], p[2]) for p in points(straight.groups[0]) if p[0] == -32}
    entry = {(p[0], p[2]) for p in points(curve.groups[0]) if p[1] == 0}
    self.assertEqual(section, entry)

  def test_arches_may_meet_without_walls(self):
    h = Hallway(arch_top=104, arch_bottom=104, height=272)
    self.assertEqual(h.spring_top, h.spring_bottom)
    straight = build_straight(h, 64, TEXTURE)
    self.assertEqual(len(straight.groups[0]), 32)
    self.assertRecoversPoints(straight)
    self.assertContinuous(self.texels(straight))

  def test_rejects_bad_parameters(self):
    with self.assertRaises(BrushError):
      build_curve(Hallway(facets_top=15), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(Hallway(arch_bottom=32, facets_bottom=7), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(Hallway(arch_bottom=-1), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(Hallway(arch_bottom=100, height=240), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(Hallway(width=0), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(self.hallway, 90, 0)
    with self.assertRaises(BrushError):
      build_curve(self.hallway, 0, 8)
    with self.assertRaises(BrushError):
      build_curve(self.hallway, 181, 8)
    with self.assertRaises(BrushError):
      build_curve(self.hallway, 90, 4, TEXTURE, 0)
    with self.assertRaises(BrushError):
      build_curve(Hallway(width=64), 90, 8)
    with self.assertRaises(BrushError):
      build_curve(Hallway(height=128), 90, 8)
    with self.assertRaises(BrushError):
      build_straight(self.hallway, 0)
    with self.assertRaises(BrushError):
      build_straight(self.hallway, 63)
    with self.assertRaises(BrushError):
      build_straight(Hallway(width=255), 64)

  def test_main_writes_each_shape(self):
    with tempfile.TemporaryDirectory() as d:
      path = str(Path(d) / "hall.map")
      for args in (["--curve", "--segments", "4"], ["--curve", "45", "--segments", "3"],
                   ["--straight", "64", "--texture", TEXTURE]):
        self.assertEqual(brushfu.main(args + ["-o", path]), 0)
        text = Path(path).read_text()
        self.assertIn('"mapversion" "220"', text)
        self.assertEqual(text.count('"classname" "func_group"'), 1)
        self.assertEqual(re.findall(r"// entity (\d+)", text), ["0", "1"])
        brushes = re.findall(r"// brush (\d+)", text)
        self.assertEqual(brushes, [str(i) for i in range(len(brushes))])
    with self.assertRaises(SystemExit):
      brushfu.parse_args([])


if __name__ == "__main__":
  unittest.main()
