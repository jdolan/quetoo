#!/usr/bin/env python3
"""Convert static (single-frame) MD3 models to Wavefront OBJ."""

import argparse
import math
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MD3_MAGIC = 0x33504449  # 'IDP3'
MD3_VERSION = 15
MD3_XYZ_SCALE = 1.0 / 64.0


@dataclass
class Surface:
  name: str
  shader: str
  vertices: list  # [(x, y, z), ...]
  normals: list   # [(nx, ny, nz), ...]
  texcoords: list # [(s, t), ...]
  triangles: list # [(i0, i1, i2), ...]


def decode_normal(norm: int) -> tuple:
  """Decode an MD3 compressed lat/lon normal to a unit vector."""
  lat = ((norm >> 8) & 0xFF) * math.pi / 128.0
  lon = (norm & 0xFF) * math.pi / 128.0

  x = math.cos(lat) * math.sin(lon)
  y = math.sin(lat) * math.sin(lon)
  z = math.cos(lon)

  length = math.sqrt(x * x + y * y + z * z)
  if length > 0:
    x /= length
    y /= length
    z /= length

  return (x, y, z)


def read_md3(path: Path) -> list[Surface]:
  """Parse an MD3 file and return its surfaces (frame 0 only)."""
  data = path.read_bytes()

  # Header: id, version, filename[64], flags, num_frames, num_tags,
  #          num_surfaces, num_shaders, ofs_frames, ofs_tags,
  #          ofs_surfaces, ofs_end
  hdr_fmt = "<ii64siiiiiiiii"
  hdr_size = struct.calcsize(hdr_fmt)
  (magic, version, _, _, num_frames, _, num_surfaces, _,
   _, _, ofs_surfaces, _) = struct.unpack_from(hdr_fmt, data)

  if magic != MD3_MAGIC:
    raise ValueError(f"{path}: not an MD3 file (bad magic)")
  if version != MD3_VERSION:
    raise ValueError(f"{path}: unsupported MD3 version {version}")
  if num_frames != 1:
    raise ValueError(f"{path}: animated model ({num_frames} frames), skipping")

  surfaces = []
  surf_ofs = ofs_surfaces

  for _ in range(num_surfaces):
    # Surface header
    surf_fmt = "<i64siiiiiiiiii"
    (_, surf_name, _, surf_num_frames, num_shaders, num_verts,
     num_tris, ofs_tris, ofs_shaders, ofs_tc,
     ofs_verts, ofs_end) = struct.unpack_from(surf_fmt, data, surf_ofs)

    surf_name = surf_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")

    # Shader (first one)
    shader = ""
    if num_shaders > 0:
      shader_fmt = "<64si"
      shader_name, _ = struct.unpack_from(shader_fmt, data, surf_ofs + ofs_shaders)
      shader = shader_name.split(b"\x00", 1)[0].decode("ascii", errors="replace")

    # Triangles
    triangles = []
    for t in range(num_tris):
      tri_ofs = surf_ofs + ofs_tris + t * 12
      i0, i1, i2 = struct.unpack_from("<III", data, tri_ofs)
      triangles.append((i0, i1, i2))

    # Texture coordinates
    texcoords = []
    for v in range(num_verts):
      tc_ofs = surf_ofs + ofs_tc + v * 8
      s, t = struct.unpack_from("<ff", data, tc_ofs)
      texcoords.append((s, t))

    # Vertices (frame 0 only)
    vertices = []
    normals = []
    for v in range(num_verts):
      v_ofs = surf_ofs + ofs_verts + v * 8
      px, py, pz, norm = struct.unpack_from("<hhhh", data, v_ofs)
      vertices.append((px * MD3_XYZ_SCALE, py * MD3_XYZ_SCALE, pz * MD3_XYZ_SCALE))
      normals.append(decode_normal(norm))

    surfaces.append(Surface(
      name=surf_name,
      shader=shader,
      vertices=vertices,
      normals=normals,
      texcoords=texcoords,
      triangles=triangles,
    ))

    surf_ofs += ofs_end

  return surfaces


def write_obj(surfaces: list[Surface], out_path: Path, source_name: str):
  """Write surfaces to a Wavefront OBJ file."""
  lines = [f"# Converted from {source_name} by md32obj.py\n"]

  vert_offset = 0

  for surf in surfaces:
    lines.append(f"\nusemtl {surf.shader}\n")

    # Swap Y↔Z: Quake space (Z-up) → OBJ space (Y-up)
    for x, y, z in surf.vertices:
      lines.append(f"v {x:g} {z:g} {y:g}\n")

    # Negate V: OBJ loader will negate it back
    for s, t in surf.texcoords:
      lines.append(f"vt {s:g} {-t:g}\n")

    # Swap Y↔Z on normals to match vertex transform
    for nx, ny, nz in surf.normals:
      lines.append(f"vn {nx:.6f} {nz:.6f} {ny:.6f}\n")

    # Winding is preserved: the Y↔Z swap in output is undone by the OBJ loader
    for i0, i1, i2 in surf.triangles:
      a = i0 + 1 + vert_offset
      b = i1 + 1 + vert_offset
      c = i2 + 1 + vert_offset
      lines.append(f"f {a}/{a}/{a} {b}/{b}/{b} {c}/{c}/{c}\n")

    vert_offset += len(surf.vertices)

  out_path.write_text("".join(lines))


def main():
  parser = argparse.ArgumentParser(description="Convert static MD3 models to OBJ")
  parser.add_argument("input", nargs="+", help="MD3 file(s) to convert")
  parser.add_argument("-o", "--output", help="Output OBJ path (single file mode only)")
  parser.add_argument("--in-place", action="store_true",
                      help="Write .obj next to each .md3 input")
  args = parser.parse_args()

  if args.output and len(args.input) > 1:
    print("Error: -o/--output only works with a single input file", file=sys.stderr)
    sys.exit(1)

  for md3_path_str in args.input:
    md3_path = Path(md3_path_str)
    if not md3_path.exists():
      print(f"Error: {md3_path} not found", file=sys.stderr)
      sys.exit(1)

    try:
      surfaces = read_md3(md3_path)
    except ValueError as e:
      print(f"Warning: {e}", file=sys.stderr)
      continue

    if args.output:
      out_path = Path(args.output)
    elif args.in_place:
      out_path = md3_path.with_suffix(".obj")
    else:
      out_path = Path(md3_path.stem + ".obj")

    write_obj(surfaces, out_path, md3_path.name)
    print(f"{md3_path} -> {out_path} ({sum(len(s.triangles) for s in surfaces)} tris)")


if __name__ == "__main__":
  main()
