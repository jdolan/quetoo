#!/usr/bin/env python3
"""Convert Quake MDL v6 models to Wavefront OBJ."""

import argparse
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MDL_MAGIC = b"IDPO"
MDL_VERSION = 6


@dataclass
class MdlTexCoord:
  on_seam: int
  s: int
  t: int


@dataclass
class MdlTriangle:
  faces_front: int
  verts: tuple[int, int, int]


def _read_header(data: bytes):
  if data[:4] != MDL_MAGIC:
    raise ValueError("not an MDL file (bad magic)")

  version = struct.unpack_from("<i", data, 4)[0]
  if version != MDL_VERSION:
    raise ValueError(f"unsupported MDL version {version}")

  scale = struct.unpack_from("<3f", data, 8)
  origin = struct.unpack_from("<3f", data, 20)
  num_skins, skin_w, skin_h, num_verts, num_tris, num_frames, _, _ = struct.unpack_from("<8i", data, 48)

  return scale, origin, num_skins, skin_w, skin_h, num_verts, num_tris, num_frames


def _skip_skins(data: bytes, offset: int, num_skins: int, skin_w: int, skin_h: int) -> int:
  skin_size = skin_w * skin_h
  for _ in range(num_skins):
    group = struct.unpack_from("<i", data, offset)[0]
    offset += 4
    if group == 0:
      offset += skin_size
    else:
      n = struct.unpack_from("<i", data, offset)[0]
      offset += 4
      offset += 4 * n
      offset += skin_size * n
  return offset


def read_mdl(path: Path):
  data = path.read_bytes()
  scale, origin, num_skins, skin_w, skin_h, num_verts, num_tris, num_frames = _read_header(data)
  if num_frames < 1:
    raise ValueError("model has no frames")

  offset = 84  # includes mdl_t.size
  offset = _skip_skins(data, offset, num_skins, skin_w, skin_h)

  texcoords: list[MdlTexCoord] = []
  for _ in range(num_verts):
    on_seam, s, t = struct.unpack_from("<3i", data, offset)
    texcoords.append(MdlTexCoord(on_seam=on_seam, s=s, t=t))
    offset += 12

  triangles: list[MdlTriangle] = []
  for _ in range(num_tris):
    faces_front, a, b, c = struct.unpack_from("<4i", data, offset)
    triangles.append(MdlTriangle(faces_front=faces_front, verts=(a, b, c)))
    offset += 16

  frame_type = struct.unpack_from("<i", data, offset)[0]
  if frame_type != 0:
    raise ValueError("grouped frames are not supported yet")

  # simple frame: type + bbox min/max + name[16] + trivertx[num_verts]
  offset += 4 + 4 + 4 + 16
  vertices: list[tuple[float, float, float]] = []
  for _ in range(num_verts):
    x, y, z, _normal_index = struct.unpack_from("<4B", data, offset)
    offset += 4
    vx = origin[0] + x * scale[0]
    vy = origin[1] + y * scale[1]
    vz = origin[2] + z * scale[2]
    # Quake Z-up -> OBJ Y-up
    vertices.append((vx, vz, vy))

  return vertices, texcoords, triangles, skin_w, skin_h


def _corner_uv(tc: MdlTexCoord, tri_faces_front: int, skin_w: int, skin_h: int) -> tuple[float, float]:
  s = float(tc.s)
  if (not tri_faces_front) and tc.on_seam:
    s += skin_w * 0.5
  u = s / float(skin_w)
  v = 1.0 - (float(tc.t) / float(skin_h))
  return u, v


def write_obj(
  out_path: Path,
  source_name: str,
  vertices: list[tuple[float, float, float]],
  texcoords: list[MdlTexCoord],
  triangles: list[MdlTriangle],
  skin_w: int,
  skin_h: int,
  material: str | None = None,
):
  lines = [f"# Converted from {source_name} by mdl2obj.py\n"]
  if material:
    lines.append(f"usemtl {material}\n")

  # Compute per-vertex normals from triangle geometry.
  accum = [[0.0, 0.0, 0.0] for _ in vertices]
  for tri in triangles:
    a, b, c = tri.verts
    ax, ay, az = vertices[a]
    bx, by, bz = vertices[b]
    cx, cy, cz = vertices[c]
    ex, ey, ez = bx - ax, by - ay, bz - az
    fx, fy, fz = cx - ax, cy - ay, cz - az
    nx = ey * fz - ez * fy
    ny = ez * fx - ex * fz
    nz = ex * fy - ey * fx
    accum[a][0] += nx; accum[a][1] += ny; accum[a][2] += nz
    accum[b][0] += nx; accum[b][1] += ny; accum[b][2] += nz
    accum[c][0] += nx; accum[c][1] += ny; accum[c][2] += nz
  normals: list[tuple[float, float, float]] = []
  for nx, ny, nz in accum:
    ln = math.sqrt(nx * nx + ny * ny + nz * nz)
    if ln > 0.0:
      normals.append((nx / ln, ny / ln, nz / ln))
    else:
      normals.append((0.0, 0.0, 1.0))

  for x, y, z in vertices:
    lines.append(f"v {x:g} {y:g} {z:g}\n")

  vt_lines: list[str] = []
  faces: list[tuple[int, int, int]] = []
  vt_index = 1

  for tri in triangles:
    tri_vt: list[int] = []
    for vi in tri.verts:
      u, v = _corner_uv(texcoords[vi], tri.faces_front, skin_w, skin_h)
      vt_lines.append(f"vt {u:.6f} {v:.6f}\n")
      tri_vt.append(vt_index)
      vt_index += 1
    faces.append(tuple(tri_vt))

  lines.append("\n")
  lines.extend(vt_lines)
  lines.append("\n")
  for nx, ny, nz in normals:
    lines.append(f"vn {nx:.6f} {ny:.6f} {nz:.6f}\n")
  lines.append("\n")

  for tri, (ta, tb, tc) in zip(triangles, faces):
    a, b, c = (tri.verts[0] + 1, tri.verts[1] + 1, tri.verts[2] + 1)
    lines.append(f"f {a}/{ta}/{a} {b}/{tb}/{b} {c}/{tc}/{c}\n")

  out_path.write_text("".join(lines))


def main():
  parser = argparse.ArgumentParser(description="Convert Quake MDL v6 models to OBJ")
  parser.add_argument("input", nargs="+", help="MDL file(s) to convert")
  parser.add_argument("-o", "--output", help="Output OBJ path (single-file mode only)")
  parser.add_argument("--in-place", action="store_true", help="Write .obj next to each .mdl")
  parser.add_argument("--material", help="Write `usemtl` with this Quetoo quake path")
  args = parser.parse_args()

  if args.output and len(args.input) > 1:
    print("Error: -o/--output only works with a single input file", file=sys.stderr)
    sys.exit(1)

  for mdl_path_str in args.input:
    mdl_path = Path(mdl_path_str)
    if not mdl_path.exists():
      print(f"Error: {mdl_path} not found", file=sys.stderr)
      sys.exit(1)

    try:
      vertices, texcoords, triangles, skin_w, skin_h = read_mdl(mdl_path)
    except ValueError as e:
      print(f"Warning: {mdl_path}: {e}", file=sys.stderr)
      continue

    if args.output:
      out_path = Path(args.output)
    elif args.in_place:
      out_path = mdl_path.with_suffix(".obj")
    else:
      out_path = Path(mdl_path.stem + ".obj")

    write_obj(out_path, mdl_path.name, vertices, texcoords, triangles, skin_w, skin_h, args.material)
    print(f"{mdl_path} -> {out_path} ({len(vertices)} verts, {len(triangles)} tris)")


if __name__ == "__main__":
  main()
