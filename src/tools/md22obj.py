#!/usr/bin/env python3
"""Convert Quake2 MD2 models to Wavefront OBJ."""

import argparse
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

MD2_MAGIC = 0x32504449  # 'IDP2'
MD2_VERSION = 8


@dataclass
class Md2Model:
  vertices: list[tuple[float, float, float]]
  texcoords: list[tuple[float, float]]
  triangles: list[tuple[int, int, int, int, int, int]]  # v0,v1,v2,t0,t1,t2
  material: str


def _material_from_skin(skin: str) -> str:
  # Prefer extension-less local material names (e.g. "skin") so obj-fu can
  # resolve skin.tga / skin.jpg next to the model.
  base = os.path.basename(skin)
  stem, _ = os.path.splitext(base)
  return stem or "default"


def read_md2(path: Path, frame_index: int = 0) -> Md2Model:
  data = path.read_bytes()

  hdr_fmt = "<17i"
  hdr_size = struct.calcsize(hdr_fmt)
  if len(data) < hdr_size:
    raise ValueError(f"{path}: file too small for MD2 header")

  (
    ident,
    version,
    skin_width,
    skin_height,
    frame_size,
    num_skins,
    num_xyz,
    num_st,
    num_tris,
    _num_glcmds,
    num_frames,
    ofs_skins,
    ofs_st,
    ofs_tris,
    ofs_frames,
    _ofs_glcmds,
    ofs_end,
  ) = struct.unpack_from(hdr_fmt, data, 0)

  if ident != MD2_MAGIC:
    raise ValueError(f"{path}: not an MD2 file (bad magic)")
  if version != MD2_VERSION:
    raise ValueError(f"{path}: unsupported MD2 version {version}")
  if num_frames <= 0:
    raise ValueError(f"{path}: model has no frames")
  if frame_index < 0 or frame_index >= num_frames:
    raise ValueError(f"{path}: frame index {frame_index} out of range (0..{num_frames - 1})")
  if ofs_end > len(data):
    raise ValueError(f"{path}: truncated file")

  # Skins
  skins: list[str] = []
  for i in range(num_skins):
    o = ofs_skins + i * 64
    raw = struct.unpack_from("<64s", data, o)[0]
    skins.append(raw.split(b"\x00", 1)[0].decode("ascii", errors="replace"))

  # Texture coordinates (signed short)
  st: list[tuple[float, float]] = []
  for i in range(num_st):
    o = ofs_st + i * 4
    s, t = struct.unpack_from("<hh", data, o)
    u = s / float(skin_width) if skin_width else 0.0
    v = -(t / float(skin_height)) if skin_height else 0.0  # match md32obj behavior
    st.append((u, v))

  # Triangles: 3x vertex indices + 3x texcoord indices
  triangles: list[tuple[int, int, int, int, int, int]] = []
  for i in range(num_tris):
    o = ofs_tris + i * 12
    v0, v1, v2, t0, t1, t2 = struct.unpack_from("<6H", data, o)
    triangles.append((v0, v1, v2, t0, t1, t2))

  # Frame vertices
  frame_ofs = ofs_frames + frame_index * frame_size
  scale = struct.unpack_from("<3f", data, frame_ofs)
  translate = struct.unpack_from("<3f", data, frame_ofs + 12)
  vert_ofs = frame_ofs + 40  # scale[3], translate[3], name[16]

  vertices: list[tuple[float, float, float]] = []
  for i in range(num_xyz):
    o = vert_ofs + i * 4
    x, y, z, _n = struct.unpack_from("<4B", data, o)
    vx = x * scale[0] + translate[0]
    vy = y * scale[1] + translate[1]
    vz = z * scale[2] + translate[2]
    # Quake Z-up -> OBJ Y-up
    vertices.append((vx, vz, vy))

  material = _material_from_skin(skins[0]) if skins else "default"
  return Md2Model(vertices=vertices, texcoords=st, triangles=triangles, material=material)


def write_obj(model: Md2Model, out_path: Path, source_name: str):
  lines: list[str] = [f"# Converted from {source_name} by md22obj.py\n", f"usemtl {model.material}\n"]

  for x, y, z in model.vertices:
    lines.append(f"v {x:g} {y:g} {z:g}\n")

  for u, v in model.texcoords:
    lines.append(f"vt {u:g} {v:g}\n")

  lines.append("\n")
  for v0, v1, v2, t0, t1, t2 in model.triangles:
    a = v0 + 1
    b = v1 + 1
    c = v2 + 1
    ta = t0 + 1
    tb = t1 + 1
    tc = t2 + 1
    lines.append(f"f {a}/{ta} {b}/{tb} {c}/{tc}\n")

  out_path.write_text("".join(lines))


def main():
  parser = argparse.ArgumentParser(description="Convert MD2 models to OBJ")
  parser.add_argument("input", nargs="+", help="MD2 file(s) to convert")
  parser.add_argument("-o", "--output", help="Output OBJ path (single file mode only)")
  parser.add_argument("--in-place", action="store_true", help="Write .obj next to each .md2 input")
  parser.add_argument("--frame", type=int, default=0, help="Frame index to export (default: 0)")
  args = parser.parse_args()

  if args.output and len(args.input) > 1:
    print("Error: -o/--output only works with a single input file", file=sys.stderr)
    sys.exit(1)

  for md2_path_str in args.input:
    md2_path = Path(md2_path_str)
    if not md2_path.exists():
      print(f"Error: {md2_path} not found", file=sys.stderr)
      sys.exit(1)

    try:
      model = read_md2(md2_path, frame_index=args.frame)
    except ValueError as e:
      print(f"Warning: {e}", file=sys.stderr)
      continue

    if args.output:
      out_path = Path(args.output)
    elif args.in_place:
      out_path = md2_path.with_suffix(".obj")
    else:
      out_path = Path(md2_path.stem + ".obj")

    write_obj(model, out_path, md2_path.name)
    print(f"{md2_path} -> {out_path} ({len(model.vertices)} verts, {len(model.triangles)} tris)")


if __name__ == "__main__":
  main()

