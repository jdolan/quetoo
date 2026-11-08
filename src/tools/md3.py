"""Quake III MD3 model reading shared by the player model tools.

Kept free of pygame and OpenGL imports so that tools which only need geometry
and texture coordinates (e.g. tintfu) can import it in a headless environment.
"""

import struct
from dataclasses import dataclass, field
from pathlib import Path

MD3_MAGIC = b"IDP3"
MD3_VERSION = 15
MD3_XYZ_SCALE = 1.0 / 64.0

Vec3 = tuple[float, float, float]
Tag = tuple[Vec3, Vec3, Vec3, Vec3]  # origin, axis0, axis1, axis2


@dataclass
class Md3Surface:
  name: str
  triangles: list[tuple[int, int, int]]
  frames: list[list[Vec3]]  # per-frame list of vertex positions
  texcoords: list[tuple[float, float]] = field(default_factory=list)  # per-vertex (s, t), shared by all frames


@dataclass
class Md3Model:
  num_frames: int
  surfaces: list[Md3Surface] = field(default_factory=list)
  tags: list[dict[str, Tag]] = field(default_factory=list)  # per-frame tag name -> tag


def parse_skin(path: Path) -> dict[str, str]:
  """Parses a .skin file: `surface_name,relative/path/no/extension` per line,
  relative to the game's data root (e.g. quetoo-data/target/default). Blank
  paths (as on tag_* lines) are skipped."""
  mapping: dict[str, str] = {}
  for raw_line in path.read_text().splitlines():
    line = raw_line.split("//")[0].strip()
    if not line or "," not in line:
      continue
    name, _, rel_path = line.partition(",")
    name, rel_path = name.strip(), rel_path.strip()
    if name and rel_path:
      mapping[name] = rel_path
  return mapping


def read_md3(path: Path) -> Md3Model:
  data = path.read_bytes()

  hdr_fmt = "<4si64siiiiiiiii"
  (magic, version, _name, _flags, num_frames, num_tags, num_surfaces, _num_skins,
   ofs_frames, ofs_tags, ofs_surfaces, _ofs_end) = struct.unpack_from(hdr_fmt, data)

  if magic != MD3_MAGIC:
    raise ValueError(f"{path}: not an MD3 file (bad magic)")
  if version != MD3_VERSION:
    raise ValueError(f"{path}: unsupported MD3 version {version}")

  tag_fmt = "<64s3f3f3f3f"
  tag_size = struct.calcsize(tag_fmt)
  tags: list[dict[str, Tag]] = []
  for f in range(num_frames):
    frame_tags: dict[str, Tag] = {}
    for t in range(num_tags):
      ofs = ofs_tags + (f * num_tags + t) * tag_size
      raw_name, ox, oy, oz, *axis = struct.unpack_from(tag_fmt, data, ofs)
      name = raw_name.split(b"\x00", 1)[0].decode("ascii", "replace")
      frame_tags[name] = ((ox, oy, oz), tuple(axis[0:3]), tuple(axis[3:6]), tuple(axis[6:9]))
    tags.append(frame_tags)

  surf_fmt = "<4s64siiiiiiiiii"
  surfaces: list[Md3Surface] = []
  surf_ofs = ofs_surfaces
  for _ in range(num_surfaces):
    (_ident, raw_sname, _sflags, surf_num_frames, _num_shaders, num_verts, num_tris,
     ofs_tris, _ofs_shaders, ofs_tc, ofs_verts, ofs_surf_end) = struct.unpack_from(surf_fmt, data, surf_ofs)
    sname = raw_sname.split(b"\x00", 1)[0].decode("ascii", "replace")

    triangles = []
    for t in range(num_tris):
      i0, i1, i2 = struct.unpack_from("<3i", data, surf_ofs + ofs_tris + t * 12)
      triangles.append((i0, i1, i2))

    # (s, t) texcoords are stored once per vertex (not per frame -- they don't
    # animate), immediately as 2 floats each.
    texcoords = []
    for v in range(num_verts):
      s, t = struct.unpack_from("<2f", data, surf_ofs + ofs_tc + v * 8)
      texcoords.append((s, t))

    frames: list[list[Vec3]] = []
    for f in range(surf_num_frames):
      base = surf_ofs + ofs_verts + f * num_verts * 8
      verts = []
      for v in range(num_verts):
        px, py, pz, _norm = struct.unpack_from("<4h", data, base + v * 8)
        verts.append((px * MD3_XYZ_SCALE, py * MD3_XYZ_SCALE, pz * MD3_XYZ_SCALE))
      frames.append(verts)

    surfaces.append(Md3Surface(sname, triangles, frames, texcoords))
    surf_ofs += ofs_surf_end

  return Md3Model(num_frames, surfaces, tags)


def resolve_texture(data_root: Path, rel_path: str) -> Path | None:
  """Resolves a .skin file's extensionless, data-root-relative path (e.g.
  `players/gork/upper_default`) to an actual image file on disk."""
  for ext in (".png", ".tga", ".jpg", ".jpeg"):
    candidate = data_root / f"{rel_path}{ext}"
    if candidate.is_file():
      return candidate
  return None
