#!/usr/bin/env python3
"""Generate arched hallways as integer-exact Valve 220 brushwork.

The hallway cross section is a rectangle with an elliptical arch on top and, optionally, another below,
and the two may differ. `--curve` sweeps it around a vertical pivot at the origin, and `--straight`
extrudes it along X, centered on the origin. Move,
rotate and texture the result in the editor; every brush face is `common/caulk` except the interior
surface, when `--texture` is given.

Every vertex is an integer, and every face is exactly planar, so the surface is contiguous between
brushes and between segments without vertex snapping or face splitting. A curve is built from
tetrahedra: its vertices are the ideal circle points rounded to the grid, which are evenly spaced at
any segment count, and each cell is cut into tetrahedra so that neighbours agree on every shared
face. A straight hallway needs no such cutting; it is one prism per wall and per arch facet. The
two share their cross section exactly, so they abut vertex for vertex.
"""

import argparse
import math
import os
import sys
import tempfile
from dataclasses import dataclass, replace
from itertools import combinations
from math import atan2, cos, gcd, hypot, radians, sin


class BrushError(ValueError):
  pass


@dataclass
class Hallway:
  """Overall dimensions, as the finished brushwork measures: `width` and `height` include the walls."""
  width: int = 320
  wall: int = 32
  height: int = 240
  arch_top: int = 96  # the interior's rise above the top spring line
  arch_bottom: int = 0  # the interior's fall below the bottom spring line; zero leaves the floor open
  facets_top: int = 16
  facets_bottom: int = 0  # zero uses facets_top
  radius: int = 0

  @property
  def interior(self) -> int:
    return self.width - 2 * self.wall

  @property
  def bottom_facets(self) -> int:
    return self.facets_bottom or self.facets_top

  @property
  def spring_top(self) -> int:
    return self.height - self.arch_top - self.wall

  @property
  def spring_bottom(self) -> int:
    return self.wall + self.arch_bottom if self.arch_bottom else 0

  def validate(self) -> None:
    for name in ("width", "wall", "height", "arch_top", "facets_top"):
      if getattr(self, name) <= 0:
        raise BrushError(f"{name.replace('_', '-')} must be positive")
    for name in ("arch_bottom", "facets_bottom", "radius"):
      if getattr(self, name) < 0:
        raise BrushError(f"{name.replace('_', '-')} must not be negative")
    if self.interior <= 0:
      raise BrushError("width must exceed twice the wall")
    # an open floor needs side walls to stand on; a closed ring may pinch its walls down to nothing
    if self.spring_top < self.spring_bottom or (self.spring_top == self.spring_bottom and not self.arch_bottom):
      raise BrushError("height must exceed the arches and the walls above and below them")
    if self.facets_top % 2 or self.bottom_facets % 2:
      raise BrushError("arch facets must be even, so that each arch has an apex")


@dataclass
class Piece:
  """One brush's profile: its (rho, z) corners, and the interior surface edge, if any."""
  points: list
  surface: tuple = None


@dataclass
class Section:
  pieces: list
  rhos: list
  visible: list
  chain: list  # the interior surface's (rho, z) vertices, in order; a closed ring starts and ends at the bottom crown


def build_arch(h: Hallway, sign: int, rise: int, facets: int, zs: int):
  """Builds one arch's brushes in (rho, z): sign is +1 above the spring line `zs`, -1 below it.

  Returns the interior's vertices, left (small rho) to right, and one piece per facet.
  """
  rho0 = h.radius
  rhoIn = rho0 + h.wall
  rhoOut = rhoIn + h.interior
  rhoEnd = rhoOut + h.wall
  rc = rhoIn + h.interior / 2.0
  a, b = h.interior / 2.0, float(rise)
  hw, ht = a + h.wall, b + h.wall
  zo = zs + sign * (rise + h.wall)

  # equal angle points on the half ellipse, left to right: a vertex aligned circle, as TrenchBroom's
  # shape tools sample, scaled to the arch's bounds
  directions, inner = [], []
  for k in range(facets + 1):
    phi = math.pi * (1.0 - k / facets)
    u, v = a * cos(phi), b * sin(phi)
    if k == 0:
      u, v = -a, 0.0
    elif k == facets:
      u, v = a, 0.0
    elif k * 2 == facets:
      u, v = 0.0, b
    directions.append((u, v))
    inner.append((int(round(rc + u)), zs + sign * int(round(v))))

  inner[0] = (rhoIn, zs)
  inner[-1] = (rhoOut, zs)
  if len(set(inner)) != len(inner):
    raise BrushError("too many arch facets for this size; reduce the arch facets")

  # radial cuts from the ellipse center out to the rectangular outside of the wall
  outer, angles = [], []
  for u, v in directions:
    t = hw / abs(u) if u else math.inf
    if v > 0:
      t = min(t, ht / v)
    x, y = rc + t * u, zs + sign * t * v
    if abs(abs(u) * ht - v * hw) < 1e-9 or abs(u) * t >= hw - 1e-9:
      outer.append((rho0 if u < 0 else rhoEnd, int(round(y))))
    else:
      outer.append((int(round(x)), zo))
    angles.append(atan2(v, u))

  corners = [(rho0, zo, atan2(ht, -hw)), (rhoEnd, zo, atan2(ht, hw))]

  pieces = []
  for k in range(facets):
    points = [inner[k], inner[k + 1], outer[k + 1]]
    for cx, cz, ca in reversed(corners):
      if angles[k + 1] + 1e-9 < ca < angles[k] - 1e-9:
        points.append((cx, cz))
    points.append(outer[k])
    unique = []
    for p in points:
      if p not in unique:
        unique.append(p)
    pieces.append(Piece(unique, (inner[k], inner[k + 1])))
  return inner, pieces


def build_section(h: Hallway) -> Section:
  """Builds the cross section in (rho, z), where rho is the distance from the pivot axis.

  The floor of the outside is z = 0. Side walls stand between the two spring lines, with the top arch
  above and, if there is one, the bottom arch below. Without a bottom arch the floor is open.
  """
  h.validate()

  rho0 = h.radius
  rhoIn = rho0 + h.wall
  rhoOut = rhoIn + h.interior
  rhoEnd = rhoOut + h.wall
  zt, zb = h.spring_top, h.spring_bottom

  top, pieces = build_arch(h, 1, h.arch_top, h.facets_top, zt)
  if zt > zb:
    pieces += [
      Piece([(rho0, zb), (rho0, zt), (rhoIn, zt), (rhoIn, zb)], ((rhoIn, zt), (rhoIn, zb))),
      Piece([(rhoOut, zb), (rhoOut, zt), (rhoEnd, zt), (rhoEnd, zb)], ((rhoOut, zt), (rhoOut, zb))),
    ]

  if h.arch_bottom:
    bottom, below = build_arch(h, -1, h.arch_bottom, h.bottom_facets, zb)
    pieces += below
    crown = h.bottom_facets // 2
    chain = bottom[crown::-1] + top + bottom[:crown - 1:-1]
  else:
    chain = [(rhoIn, zb)] + top + [(rhoOut, zb)]
  chain = [p for k, p in enumerate(chain) if k == 0 or p != chain[k - 1]]

  rhos = sorted({p[0] for piece in pieces for p in piece.points})
  visible = sorted({p[0] for piece in pieces for p in piece.surface})
  return Section(pieces, rhos, visible, chain)


def cross3(a, b):
  return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def sub3(a, b):
  return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def dot3(a, b):
  return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


@dataclass
class Face:
  normal: tuple
  dist: int
  points: list
  triple: tuple


def hull_faces(points):
  """Exact convex hull of integer points: every supporting plane with at least three of them."""
  points = sorted(set(points))
  planes = {}
  for a, b, c in combinations(points, 3):
    n = cross3(sub3(c, a), sub3(b, a))
    if n == (0, 0, 0):
      continue
    sides = [dot3(n, sub3(p, a)) for p in points]
    if all(s <= 0 for s in sides):
      pass
    elif all(s >= 0 for s in sides):
      n = (-n[0], -n[1], -n[2])
    else:
      continue
    d = dot3(n, a)
    g = gcd(gcd(abs(n[0]), abs(n[1])), gcd(abs(n[2]), abs(d)))
    key = (n[0] // g, n[1] // g, n[2] // g, d // g)
    planes[key] = None

  faces = []
  for nx, ny, nz, d in planes:
    n = (nx, ny, nz)
    on = [p for p in points if dot3(n, p) == d]
    best, area = None, 0
    for a, b, c in combinations(on, 3):
      m = cross3(sub3(c, a), sub3(b, a))
      size = abs(m[0]) + abs(m[1]) + abs(m[2])
      if size > area:
        # Quake plane points run (a, b, c) with the outward normal along (c - a) x (b - a)
        best, area = ((a, b, c) if dot3(m, n) > 0 else (a, c, b)), size
    faces.append(Face(n, d, on, best))
  return faces


def check_convex(points, faces):
  """Every point must be a corner of the hull, or a brush would silently lose a surface vertex."""
  for p in set(points):
    ns = [f.normal for f in faces if p in f.points]
    for a, b, c in combinations(ns, 3):
      if dot3(a, cross3(b, c)):
        break
    else:
      raise BrushError(f"vertex {p} is not a corner of its brush; the profile is not convex there")


@dataclass
class Brush:
  lines: list
  points: list
  faces: list


@dataclass
class Hall:
  """The brushes of a hallway, grouped as they are written: one group per slice of a curve."""
  groups: list
  stats: dict


CAULK = "common/caulk"
SCALE = 0.25


def axial_axes(n):
  ax, ay, az = abs(n[0]), abs(n[1]), abs(n[2])
  if az >= ax and az >= ay:
    return "1 0 0", "0 -1 0"
  if ax >= ay:
    return "0 1 0", "0 0 -1"
  return "1 0 0", "0 0 -1"


def face_line(face) -> str:
  pts = " ".join(f"( {p[0]} {p[1]} {p[2]} )" for p in face.triple)
  u, v = axial_axes(face.normal)
  return f"{pts} {CAULK} [ {u} 0 ] [ {v} 0 ] 0 {SCALE} {SCALE}"


def ring_paths(rhos, count, angle):
  """Every ring at evenly spaced angles, rounded to the nearest integer point."""
  step = angle / count
  paths, radial = {}, 0.0
  for rho in rhos:
    path = [(round(rho * cos(i * step)), round(rho * sin(i * step))) for i in range(count + 1)]
    paths[rho] = path
    radial = max(radial, max(abs(hypot(*p) - rho) for p in path))
  return paths, radial


def cycle_triangles(cycle):
  """Fans a face's corners from its smallest corner, so that two cells sharing a face agree."""
  ring = [p for k, p in enumerate(cycle) if p != cycle[k - 1]]
  if len(ring) < 3:
    return []
  start = ring.index(min(ring))
  ring = ring[start:] + ring[:start]
  return [(ring[0], ring[k], ring[k + 1]) for k in range(1, len(ring) - 1)]


def tetrahedra(caps, sides):
  """Splits a cell into tetrahedra: a cone from its smallest corner to every other triangle.

  Triangulating every face by `cycle_triangles` makes neighbouring cells split their shared faces the
  same way, so the tetrahedra meet face to face and every face is exactly planar.
  """
  corners = sorted({p for cycle in caps + sides for p in cycle})
  apex = corners[0]
  centroid = tuple(sum(p[k] for p in corners) / len(corners) for k in range(3))
  tets, inverted = [], 0
  for cycle in caps + sides:
    for a, b, c in cycle_triangles(cycle):
      if apex in (a, b, c):
        continue
      n = cross3(sub3(c, a), sub3(b, a))
      volume = dot3(n, sub3(apex, a))
      if not volume:
        continue
      # with the face seen from outside, the apex lies on the far side of a sound cone
      outward = sum(n[k] * (a[k] - centroid[k]) for k in range(3))
      if (volume > 0) == (outward > 0):
        inverted += 1
      tets.append((apex, a, b, c))
  return tets, inverted


def surface_v(section: Section, scale: float):
  """The v texture coordinate of every interior surface vertex (rho, z), in texels.

  v is the distance up from the nearest end of the chain along the cross section, so the walls and
  the arch halves all stand upright, and the halves meet at the crown. An open chain starts on the
  floor; a closed ring starts and ends at the bottom crown, and meets itself at the top crown.
  """
  chain = section.chain
  along = [0.0]
  for a, b in zip(chain, chain[1:]):
    along.append(along[-1] + hypot(b[0] - a[0], b[1] - a[1]))
  total = along[-1]
  return {point: -min(d, total - d) / scale for point, d in zip(chain, along)}


def vertex_textures(section: Section, paths: dict, segments: int, scale: float):
  """Assigns every interior surface vertex (joint, rho, z) a texture coordinate, in texels.

  u is the distance along the vertex's own ring, so texels stay square along the bend; it is zero at
  the middle of the bend on every ring, which splits the unavoidable shear between the two ends. Each
  triangle's axes then interpolate its three vertices exactly, and every shared edge agrees on both
  ends: the texture is continuous across the whole surface.
  """
  v = surface_v(section, scale)
  ring = {}
  for rho in section.visible:
    steps = [0.0]
    for a, b in zip(paths[rho], paths[rho][1:]):
      steps.append(steps[-1] + hypot(b[0] - a[0], b[1] - a[1]))
    ring[rho] = steps

  uv = {}
  for (rho, z), height in v.items():
    for i in range(segments + 1):
      uv[(i, rho, z)] = ((ring[rho][i] - ring[rho][-1] / 2) / scale, height)
  return uv


def textured_line(face, uvs, texture: str, scale: float) -> str:
  """A face whose texture axes interpolate the texture coordinates of its three corners."""
  a, b, c = (tuple(float(x) for x in p) for p in face.triple)
  e1, e2 = sub3(b, a), sub3(c, a)
  n = cross3(e1, e2)
  det = dot3(e1, cross3(e2, n))
  axes = []
  for k in (0, 1):
    d1, d2 = uvs[1][k] - uvs[0][k], uvs[2][k] - uvs[0][k]
    q, r = cross3(e2, n), cross3(n, e1)
    g = tuple((d1 * q[m] + d2 * r[m]) / det for m in range(3))
    axes.append((g, uvs[0][k] - dot3(g, a)))
  pts = " ".join(f"( {p[0]} {p[1]} {p[2]} )" for p in face.triple)
  (gu, uo), (gv, vo) = axes
  fmt = lambda x: format(x, ".10g")
  u = " ".join(fmt(x * scale) for x in gu)
  v = " ".join(fmt(x * scale) for x in gv)
  return f"{pts} {texture} [ {u} {fmt(uo)} ] [ {v} {fmt(vo)} ] 0 {scale:g} {scale:g}"


def brush_lines(faces, surface, texture: str, scale: float):
  """Plane lines for a brush; a face whose corners all have a texture coordinate in `surface` gets it."""
  return [textured_line(f, [surface[p] for p in f.triple], texture, scale)
          if surface and all(p in surface for p in f.points) else face_line(f) for f in faces]


def check_texture_scale(scale: float) -> None:
  if scale <= 0:
    raise BrushError("texture scale must be positive")


def build_curve(h: Hallway, degrees: float = 90.0, segments: int = 8, texture: str = None,
                scale: float = SCALE) -> Hall:
  """Builds the curve as one group of tetrahedra per slice.

  The pivot is the origin. The curve enters across the plane y = 0, with the inside of the bend toward
  -X, heading +Y, and turns toward -X.
  """
  if segments < 1:
    raise BrushError("segments must be at least 1")
  if not 0 < degrees <= 180:
    raise BrushError("a curve must turn more than 0 and at most 180 degrees")
  check_texture_scale(scale)

  section = build_section(h)
  paths, radial = ring_paths(section.rhos, segments, radians(degrees))
  textures = vertex_textures(section, paths, segments, scale) if texture else {}

  groups, inverted = [], 0
  for i in range(segments):
    brushes = []
    for piece in section.pieces:
      joints = [[(*paths[rho][i + j], z) for rho, z in piece.points] for j in (0, 1)]
      sides = [[joints[0][k], joints[0][k - 1], joints[1][k - 1], joints[1][k]] for k in range(len(piece.points))]
      tets, bad = tetrahedra(joints, sides)
      inverted += bad

      surface = {}
      if texture and piece.surface:
        for j in (0, 1):
          for rho, z in piece.surface:
            surface[(*paths[rho][i + j], z)] = textures[(i + j, rho, z)]

      for tet in tets:
        faces = hull_faces(list(tet))
        brushes.append(Brush(brush_lines(faces, surface, texture, scale), list(tet), faces))
    groups.append(brushes)
  return Hall(groups, {"radial": radial, "inverted": inverted})


def build_straight(h: Hallway, length: int, texture: str = None, scale: float = SCALE) -> Hall:
  """Builds a straight hallway as one group of prisms, one per wall and per arch facet.

  It runs along X from -length / 2 to length / 2, and is centered on Y, with the floor at z = 0. Its
  cross section is the curve's, so a curve and a straight hallway abut vertex for vertex once one is
  turned a quarter and moved. Both the length and the width are even to keep it on the grid.
  """
  if length < 1 or length % 2:
    raise BrushError("length must be a positive even number")
  check_texture_scale(scale)

  section = build_section(replace(h, radius=0))
  half = section.rhos[-1] // 2
  if section.rhos[-1] % 2:
    raise BrushError("the width must be even")
  reach = length // 2

  # a quarter turn: the cross section's rho runs toward -Y, and the hallway's length along +X
  place = lambda rho, along, z: (along, half - rho, z)
  v = surface_v(section, scale) if texture else {}

  brushes = []
  for piece in section.pieces:
    joints = [[place(rho, along, z) for rho, z in piece.points] for along in (-reach, reach)]
    surface = {}
    if texture and piece.surface:
      for along in (-reach, reach):
        for rho, z in piece.surface:
          surface[place(rho, along, z)] = (along / scale, v[(rho, z)])

    points = sorted(set(joints[0] + joints[1]))
    faces = hull_faces(points)
    check_convex(points, faces)
    brushes.append(Brush(brush_lines(faces, surface, texture, scale), points, faces))
  return Hall([brushes], {"radial": 0.0, "inverted": 0})


def render_entities(hall: Hall) -> str:
  out = ["// entity 0\n{\n\"classname\" \"func_group\"\n"]
  brushes = [brush for group in hall.groups for brush in group]
  for n, brush in enumerate(brushes):
    out.append(f"// brush {n}\n{{\n" + "\n".join(brush.lines) + "\n}\n")
  out.append("}\n")
  return "".join(out)


HEADER = "// Game: Quetoo\n// Format: Quake3 (Valve)\n"
WORLDSPAWN = "// entity 0\n{\n\"mapversion\" \"220\"\n\"classname\" \"worldspawn\"\n}\n"


def renumber(text: str) -> str:
  count = iter(range(1 << 30))
  return "".join(f"// entity {next(count)}\n" if line == "// entity 0\n" else line for line in text.splitlines(True))


def write_atomic(path: str, text: str) -> None:
  directory = os.path.dirname(os.path.abspath(path))
  fd, temp = tempfile.mkstemp(dir=directory, suffix=".tmp")
  try:
    with os.fdopen(fd, "w") as f:
      f.write(text)
    os.replace(temp, path)
  except BaseException:
    if os.path.exists(temp):
      os.unlink(temp)
    raise


def parse_args(argv=None):
  p = argparse.ArgumentParser(
    description="Generate an arched hallway as integer-exact Valve 220 brushwork, centered on the origin.",
    epilog="A curve enters across y = 0 heading +Y, with its pivot at the origin, and turns toward -X. "
           "A straight hallway runs along X. Move and rotate the result in the editor.")
  shape = p.add_mutually_exclusive_group(required=True)
  shape.add_argument("--curve", nargs="?", type=float, const=90.0, metavar="DEGREES",
                     help="a curved hallway turning this many degrees (default 90)")
  shape.add_argument("--straight", type=int, metavar="LENGTH", help="a straight hallway of this length")
  p.add_argument("--width", type=int, default=320, help="overall width, outside to outside (default 320)")
  p.add_argument("--wall", type=int, default=32, help="wall thickness (default 32)")
  p.add_argument("--height", type=int, default=240,
                 help="overall height, from the bottom of the outside to the top of the arch's outside (default 240)")
  p.add_argument("--arch-top", type=int, default=96, help="interior rise of the top arch above its spring line (default 96)")
  p.add_argument("--arch-bottom", type=int, default=0,
                 help="interior fall of the bottom arch below its spring line; 0 leaves the floor open (default 0)")
  p.add_argument("--facets-top", type=int, default=16, help="top arch brushes per slice, even (default 16)")
  p.add_argument("--facets-bottom", type=int, default=0, help="bottom arch brushes per slice, even (default: --facets-top)")
  p.add_argument("--segments", type=int, default=8, help="slices in a curve (default 8)")
  p.add_argument("--radius", type=int, default=0, help="curve: pivot to the outside of the inner wall (default 0)")
  p.add_argument("--texture", help="texture for the interior surface, continuous across every seam (default: caulk)")
  p.add_argument("--texture-scale", type=float, default=SCALE, help="texture scale of the interior surface (default 0.25)")
  p.add_argument("-o", "--output", help="output .map file; standard output if omitted")
  return p.parse_args(argv)


def main(argv=None) -> int:
  args = parse_args(argv)
  hallway = Hallway(args.width, args.wall, args.height, arch_top=args.arch_top, arch_bottom=args.arch_bottom,
                    facets_top=args.facets_top, facets_bottom=args.facets_bottom, radius=args.radius)
  try:
    if args.straight is not None:
      hall = build_straight(hallway, args.straight, args.texture, args.texture_scale)
    else:
      hall = build_curve(hallway, args.curve, args.segments, args.texture, args.texture_scale)
  except BrushError as e:
    print(f"brushfu: {e}", file=sys.stderr)
    return 1

  text = renumber(HEADER + WORLDSPAWN + render_entities(hall))
  if args.output:
    write_atomic(args.output, text)
  else:
    sys.stdout.write(text)

  brushes = sum(len(g) for g in hall.groups)
  if args.straight is not None:
    print(f"brushfu: straight hallway, {brushes} brushes, all vertices integer", file=sys.stderr)
  else:
    print(f"brushfu: {len(hall.groups)} segments, {brushes} brushes, all vertices integer, worst radial "
          f"error {hall.stats['radial']:.2f}", file=sys.stderr)
  if hall.stats["inverted"]:
    print(f"brushfu: WARNING {hall.stats['inverted']} inverted tetrahedra", file=sys.stderr)
  return 0


if __name__ == "__main__":
  sys.exit(main())
