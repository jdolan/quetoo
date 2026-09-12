#!/usr/bin/env python3
"""MD3 animation viewer.

Plays back a Quetoo/Quake III player model's legs -> torso -> head hierarchy
for a single named `animation.cfg` sequence, with no textures and no lighting.
Useful for checking whether an animation is baked smoothly into the .md3 frames
themselves, independent of the game engine's interpolation and playback logic.
"""

import argparse
import math
import struct
from dataclasses import dataclass, field
from pathlib import Path

import pygame
from OpenGL.GL import (
  GL_BLEND,
  GL_COLOR_BUFFER_BIT,
  GL_CULL_FACE,
  GL_DEPTH_BUFFER_BIT,
  GL_DEPTH_TEST,
  GL_FILL,
  GL_FRONT_AND_BACK,
  GL_LINE,
  GL_LINEAR,
  GL_MODELVIEW,
  GL_ONE_MINUS_SRC_ALPHA,
  GL_PROJECTION,
  GL_REPEAT,
  GL_RGBA,
  GL_SRC_ALPHA,
  GL_TEXTURE_2D,
  GL_TEXTURE_MAG_FILTER,
  GL_TEXTURE_MIN_FILTER,
  GL_TEXTURE_WRAP_S,
  GL_TEXTURE_WRAP_T,
  GL_TRIANGLES,
  GL_UNSIGNED_BYTE,
  glBegin,
  glBindTexture,
  glBlendFunc,
  glClear,
  glClearColor,
  glColor3f,
  glDisable,
  glDrawPixels,
  glEnable,
  glEnd,
  glGenTextures,
  glLoadIdentity,
  glMatrixMode,
  glPolygonMode,
  glTexCoord2f,
  glTexImage2D,
  glTexParameteri,
  glVertex3f,
  glWindowPos2i,
)
from OpenGL.GLU import gluLookAt, gluPerspective
from PIL import Image
from pygame.locals import DOUBLEBUF, KEYDOWN, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEMOTION, MOUSEWHEEL, OPENGL, QUIT

MD3_MAGIC = b"IDP3"
MD3_VERSION = 15
MD3_XYZ_SCALE = 1.0 / 64.0

Vec3 = tuple[float, float, float]
Tag = tuple[Vec3, Vec3, Vec3, Vec3]  # origin, axis0, axis1, axis2

# Animation slots, in the exact order animation.cfg entries are parsed. This
# mirrors entity_animation_t in src/shared/shared.h.
ANIMATION_NAMES = [
  "BOTH_DEATH1", "BOTH_DEAD1", "BOTH_DEATH2", "BOTH_DEAD2", "BOTH_DEATH3", "BOTH_DEAD3",
  "TORSO_GESTURE",
  "TORSO_ATTACK1", "TORSO_ATTACK2",
  "TORSO_DROP", "TORSO_RAISE",
  "TORSO_STAND1", "TORSO_STAND2",
  "LEGS_WALKCR", "LEGS_WALK", "LEGS_RUN", "LEGS_BACK", "LEGS_SWIM",
  "LEGS_JUMP1", "LEGS_LAND1", "LEGS_JUMP2", "LEGS_LAND2",
  "LEGS_IDLE", "LEGS_IDLECR",
  "LEGS_TURN",
]
ANIM_TORSO_GESTURE = ANIMATION_NAMES.index("TORSO_GESTURE")
ANIM_LEGS_WALKCR = ANIMATION_NAMES.index("LEGS_WALKCR")


@dataclass
class Animation:
  name: str
  first_frame: int
  num_frames: int
  looping_frames: int
  hz: int


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


def parse_animation_cfg(path: Path) -> list[Animation]:
  """Parse animation.cfg exactly as R_LoadMd3Animations does (see
  src/client/renderer/r_mesh_model_md3.c), including the trick where LEGS_*
  frame numbers are stored relative to TORSO_GESTURE and must be rebased to
  the legs model's own frame numbering."""
  animations: list[Animation] = []
  skip = 0

  for raw_line in path.read_text().splitlines():
    line = raw_line.split("//")[0].strip()
    if not line:
      continue

    tokens = line.split()
    if len(tokens) < 4 or not tokens[0].lstrip("-").isdigit():
      continue

    first_frame, num_frames, looping_frames, hz = (int(t) for t in tokens[:4])
    index = len(animations)

    if index == ANIM_LEGS_WALKCR:
      skip = first_frame - animations[ANIM_TORSO_GESTURE].first_frame
    if index >= ANIM_LEGS_WALKCR:
      first_frame -= skip

    name = ANIMATION_NAMES[index] if index < len(ANIMATION_NAMES) else f"ANIM_{index}"
    animations.append(Animation(name, first_frame, num_frames, looping_frames, hz))

  return animations


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


def apply_tag(tag: Tag, point: Vec3) -> Vec3:
  """Transforms `point` from the tag's child space into its parent space."""
  origin, axis0, axis1, axis2 = tag
  x, y, z = point
  return (
    origin[0] + x * axis0[0] + y * axis1[0] + z * axis2[0],
    origin[1] + x * axis0[1] + y * axis1[1] + z * axis2[1],
    origin[2] + x * axis0[2] + y * axis1[2] + z * axis2[2],
  )


def lerp_tag(tag_a: Tag, tag_b: Tag, frac: float) -> Tag:
  """Linearly blends two tags' origins and axes toward `tag_b` by `frac`,
  mirroring R_ApplyMeshTag's `Mat4_Mix(t2->matrix, t1->matrix, back_lerp)`
  (see src/client/renderer/r_mesh.c). Without this, only the vertices *within*
  each part get interpolated -- the rigid legs->torso->head attachment itself
  would still snap frame-to-frame, which is most of a player model's visible
  motion, so skipping it makes interpolation look like it barely does anything."""
  if frac <= 0.0:
    return tag_a

  def lerp3(a, b):
    return (a[0] + (b[0] - a[0]) * frac, a[1] + (b[1] - a[1]) * frac, a[2] + (b[2] - a[2]) * frac)

  return tuple(lerp3(a, b) for a, b in zip(tag_a, tag_b))


def resolve_texture(data_root: Path, rel_path: str) -> Path | None:
  """Resolves a .skin file's extensionless, data-root-relative path (e.g.
  `players/gork/upper_default`) to an actual image file on disk."""
  for ext in (".png", ".tga", ".jpg", ".jpeg"):
    candidate = data_root / f"{rel_path}{ext}"
    if candidate.is_file():
      return candidate
  return None


def load_texture(path: Path) -> int:
  """Loads an image with Pillow (so we're not at the mercy of SDL_image's
  format support) and uploads it as an OpenGL 2D texture."""
  image = Image.open(path).convert("RGBA")
  data = image.tobytes()

  tex_id = int(glGenTextures(1))
  glBindTexture(GL_TEXTURE_2D, tex_id)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT)
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width, image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data)
  return tex_id


def frame_for(anim: Animation, elapsed_ms: float, loop: bool = True) -> int:
  """Resolves an absolute model frame index for the given animation and elapsed
  time, mirroring Cg_AnimateClientEntity_ (see src/cgame/common/cg_client.c).

  If `loop` is set, the whole animation restarts from frame 0 once it ends,
  regardless of `looping_frames` -- handy for eyeballing a non-looping
  animation (e.g. a death) back-to-back instead of freezing on its last frame."""
  frame_duration = 1000.0 / anim.hz
  anim_duration = anim.num_frames * frame_duration

  if loop:
    elapsed_ms %= anim_duration

  if elapsed_ms >= anim_duration:
    if anim.looping_frames:
      t = elapsed_ms - anim_duration
      loop_frame = int(t // frame_duration) % anim.looping_frames
      frame = anim.num_frames - anim.looping_frames + loop_frame
    else:
      frame = anim.num_frames - 1  # hold the final frame, as a BOTH_DEAD* pose would
  else:
    frame = int(elapsed_ms // frame_duration)

  return anim.first_frame + frame


def lerp_frame_for(anim: Animation, elapsed_ms: float, loop: bool = True) -> tuple[int, int, float]:
  """Like `frame_for`, but also resolves the *next* frame and a [0, 1) blend
  fraction toward it, mirroring the old-frame/frame interpolation Quetoo's
  renderer does every draw call (see r_mesh_draw.c) instead of snapping
  frame-to-frame at animation.cfg's hz."""
  frame_duration = 1000.0 / anim.hz
  anim_duration = anim.num_frames * frame_duration

  if loop:
    elapsed_ms %= anim_duration

  if elapsed_ms >= anim_duration:
    if anim.looping_frames:
      idx = (elapsed_ms - anim_duration) / frame_duration
      loop_frame = int(idx) % anim.looping_frames
      next_loop_frame = (loop_frame + 1) % anim.looping_frames
      frac = idx - int(idx)
      frame = anim.num_frames - anim.looping_frames + loop_frame
      next_frame = anim.num_frames - anim.looping_frames + next_loop_frame
    else:
      frame = next_frame = anim.num_frames - 1  # hold the final frame
      frac = 0.0
  else:
    idx = elapsed_ms / frame_duration
    frame = int(idx)
    frac = idx - frame
    next_frame = min(frame + 1, anim.num_frames - 1)

  return anim.first_frame + frame, anim.first_frame + next_frame, frac


def draw_surfaces(model: Md3Model, frame: int, transform, color: tuple[float, float, float],
                   next_frame: int = None, frac: float = 0.0, textures: dict[str, int] = None):
  """Draws every surface of `model` at `frame` (optionally lerped toward
  `next_frame` by `frac`). If `textures` maps a surface's name to a bound GL
  texture id, that surface is drawn textured (modulated white) instead of
  flat-colored."""
  for surface in model.surfaces:
    tex_id = textures.get(surface.name) if textures else None
    if tex_id is not None:
      glEnable(GL_TEXTURE_2D)
      glBindTexture(GL_TEXTURE_2D, tex_id)
      glColor3f(1.0, 1.0, 1.0)
    else:
      glDisable(GL_TEXTURE_2D)
      glColor3f(*color)

    verts_a = surface.frames[frame]
    verts_b = surface.frames[next_frame] if next_frame is not None and frac > 0.0 else None

    glBegin(GL_TRIANGLES)
    for i0, i1, i2 in surface.triangles:
      for i in (i0, i1, i2):
        if tex_id is not None:
          s, t = surface.texcoords[i]
          glTexCoord2f(s, t)
        if verts_b is not None:
          ax, ay, az = verts_a[i]
          bx, by, bz = verts_b[i]
          v = (ax + (bx - ax) * frac, ay + (by - ay) * frac, az + (bz - az) * frac)
        else:
          v = verts_a[i]
        x, y, z = transform(v)
        glVertex3f(x, y, z)
    glEnd()

  glDisable(GL_TEXTURE_2D)


def draw_overlay(font, window_height: int, lines: list[str]):
  """Draws a translucent block of `lines` of text in the window's top-left
  corner, as an unlit 2D pixel blit (raster position in window coordinates,
  so it ignores the 3D camera/projection entirely)."""
  line_height = font.get_linesize()
  pad = 6
  surface = pygame.Surface((420, line_height * len(lines) + pad * 2), pygame.SRCALPHA)
  surface.fill((0, 0, 0, 140))
  for i, line in enumerate(lines):
    surface.blit(font.render(line, True, (235, 235, 235)), (pad, pad + i * line_height))

  data = pygame.image.tostring(surface, "RGBA", True)

  glDisable(GL_DEPTH_TEST)
  glEnable(GL_BLEND)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  glWindowPos2i(10, window_height - surface.get_height() - 10)
  glDrawPixels(surface.get_width(), surface.get_height(), GL_RGBA, GL_UNSIGNED_BYTE, data)
  glDisable(GL_BLEND)
  glEnable(GL_DEPTH_TEST)


def read_frame_bounds(path: Path):
  """Yields (mins, maxs, origin, radius) for every frame in an MD3, used only
  to pick a sane camera distance."""
  data = path.read_bytes()
  hdr_fmt = "<4si64siiiiiiiii"
  (_magic, _version, _name, _flags, num_frames, _num_tags, _num_surfaces, _num_skins,
   ofs_frames, _ofs_tags, _ofs_surfaces, _ofs_end) = struct.unpack_from(hdr_fmt, data)

  frame_fmt = "<3f3f3ff16s"
  frame_size = struct.calcsize(frame_fmt)
  for f in range(num_frames):
    mins = struct.unpack_from("<3f", data, ofs_frames + f * frame_size)
    maxs = struct.unpack_from("<3f", data, ofs_frames + f * frame_size + 12)
    origin = struct.unpack_from("<3f", data, ofs_frames + f * frame_size + 24)
    radius = struct.unpack_from("<f", data, ofs_frames + f * frame_size + 36)[0]
    yield (mins, maxs, origin, radius)


def main():
  parser = argparse.ArgumentParser(description="MD3 player model animation viewer")
  parser.add_argument("model_dir", help="Directory containing lower.md3, upper.md3, head.md3 and animation.cfg")
  parser.add_argument("-a", "--anim", default="BOTH_DEATH1", help="Animation name to play (default: BOTH_DEATH1)")
  parser.add_argument("-l", "--list", action="store_true", help="List available animations and exit")
  parser.add_argument("-s", "--skin", default="default",
                       help="Name of the .skin file (without extension) to texture the model with "
                            "(default: 'default'); also toggleable in-viewer with 't'")
  parser.add_argument("--no-skin", action="store_true", help="Start untextured, with flat per-part colors")
  parser.add_argument("--no-interpolate", action="store_true",
                       help="Start snapped to whole frames instead of blending toward the next one "
                            "(also toggleable in-viewer with 'l')")
  args = parser.parse_args()

  model_dir = Path(args.model_dir).expanduser().resolve()

  animations = parse_animation_cfg(model_dir / "animation.cfg")
  by_name = {a.name: a for a in animations}

  if args.list:
    for a in animations:
      print(f"{a.name:14s} first_frame={a.first_frame:4d}  num_frames={a.num_frames:3d}  "
            f"looping_frames={a.looping_frames:3d}  hz={a.hz}")
    return

  if args.anim not in by_name:
    raise SystemExit(f"Unknown animation {args.anim!r}. Use --list to see available animations.")
  anim = by_name[args.anim]

  legs = read_md3(model_dir / "lower.md3")
  torso = read_md3(model_dir / "upper.md3")
  head_path = model_dir / "head.md3"
  head = read_md3(head_path) if head_path.is_file() else None

  # BOTH_* and TORSO_* frame numbers apply identically to legs.md3 and
  # upper.md3 up through TORSO_STAND2; LEGS_* animations only drive the legs,
  # with the torso held at its first standing frame.
  torso_static = ANIMATION_NAMES.index(anim.name) >= ANIM_LEGS_WALKCR if anim.name in ANIMATION_NAMES else False
  torso_stand = by_name.get("TORSO_STAND1")

  extent = max((f[3] for f in read_frame_bounds(model_dir / "lower.md3")), default=64.0)

  pygame.init()
  w, h = 1000, 800
  pygame.display.set_mode((w, h), DOUBLEBUF | OPENGL)
  font = pygame.font.SysFont(None, 20)

  glEnable(GL_DEPTH_TEST)
  glClearColor(0.12, 0.12, 0.14, 1.0)

  glMatrixMode(GL_PROJECTION)
  glLoadIdentity()
  gluPerspective(50.0, w / h, extent * 0.02, extent * 50.0)
  glMatrixMode(GL_MODELVIEW)

  # .skin files map an MD3 surface name straight to a data-root-relative
  # texture path (no extension); one file covers all three model parts.
  textures: dict[str, int] = {}
  skin_path = model_dir / f"{args.skin}.skin"
  if skin_path.is_file():
    data_root = model_dir.parent.parent
    cache: dict[str, int | None] = {}
    for surf_name, rel_path in parse_skin(skin_path).items():
      if rel_path not in cache:
        image_path = resolve_texture(data_root, rel_path)
        cache[rel_path] = load_texture(image_path) if image_path else None
        if image_path is None:
          print(f"warning: no texture file found for '{surf_name}': {rel_path}")
      if cache[rel_path] is not None:
        textures[surf_name] = cache[rel_path]
  elif not args.no_skin:
    print(f"warning: no such skin file: {skin_path}")

  cx, cy, cz = 0.0, 0.0, extent * 0.5
  yaw, pitch, dist = -35.0, 15.0, extent * 2.5
  orbiting = False
  last = (0, 0)
  wireframe = False
  paused = False
  interpolate = not args.no_interpolate
  textured = bool(textures) and not args.no_skin
  elapsed = 0.0
  anim_index = animations.index(anim)

  clock = pygame.time.Clock()
  running = True

  def set_caption():
    status = "paused" if paused else "playing"
    pygame.display.set_caption(f"md3-fu · {model_dir.name} · {anim.name} · {status}")

  def switch_anim(index):
    nonlocal anim, anim_index, torso_static, elapsed
    anim_index = index % len(animations)
    anim = animations[anim_index]
    torso_static = ANIMATION_NAMES.index(anim.name) >= ANIM_LEGS_WALKCR if anim.name in ANIMATION_NAMES else False
    elapsed = 0.0
    set_caption()

  def anim_duration_ms(a):
    return a.num_frames * (1000.0 / a.hz)

  set_caption()

  while running:
    dt = clock.tick(60)

    for event in pygame.event.get():
      if event.type == QUIT:
        running = False
      elif event.type == KEYDOWN:
        if event.key == pygame.K_ESCAPE:
          running = False
        elif event.key == pygame.K_w:
          wireframe = not wireframe
        elif event.key == pygame.K_l:
          interpolate = not interpolate
        elif event.key == pygame.K_t:
          if textures:
            textured = not textured
        elif event.key == pygame.K_SPACE:
          paused = not paused
          set_caption()
        elif event.key == pygame.K_TAB:
          if event.mod & pygame.KMOD_SHIFT:
            switch_anim(anim_index - 1)
          else:
            switch_anim(anim_index + 1)
        elif event.key in (pygame.K_RIGHT, pygame.K_LEFT):
          paused = True
          set_caption()
          step = anim_duration_ms(anim) / anim.num_frames
          elapsed += step if event.key == pygame.K_RIGHT else -step
          duration = anim_duration_ms(anim)
          elapsed = elapsed % duration  # animations always loop; space just pauses/resumes
          frame = frame_for(anim, elapsed)
          print(f"{anim.name}: frame {frame} ({frame - anim.first_frame + 1}/{anim.num_frames})")
      elif event.type == MOUSEBUTTONDOWN and event.button == 1:
        orbiting = True
        last = event.pos
      elif event.type == MOUSEBUTTONUP and event.button == 1:
        orbiting = False
      elif event.type == MOUSEMOTION and orbiting:
        dx, dy = event.pos[0] - last[0], event.pos[1] - last[1]
        yaw += dx * 0.35
        pitch += dy * 0.35
        last = event.pos
      elif event.type == MOUSEWHEEL:
        dist = max(extent * 0.2, dist - event.y * extent * 0.1)

    if not paused:
      elapsed += dt

    legs_frame, legs_next, legs_frac = lerp_frame_for(anim, elapsed)
    legs_frame, legs_next = legs_frame % legs.num_frames, legs_next % legs.num_frames
    if not interpolate:
      legs_frac = 0.0

    if torso_static and torso_stand:
      torso_frame = torso_next = torso_stand.first_frame % torso.num_frames
      torso_frac = 0.0
    else:
      torso_frame, torso_next, torso_frac = lerp_frame_for(anim, elapsed)
      torso_frame, torso_next = torso_frame % torso.num_frames, torso_next % torso.num_frames
      if not interpolate:
        torso_frac = 0.0

    tag_torso_a = legs.tags[legs_frame].get("tag_torso")
    tag_torso_b = legs.tags[legs_next].get("tag_torso")
    tag_torso = lerp_tag(tag_torso_a, tag_torso_b, legs_frac) if tag_torso_a and tag_torso_b else tag_torso_a

    tag_head = None
    if head:
      tag_head_a = torso.tags[torso_frame].get("tag_head")
      tag_head_b = torso.tags[torso_next].get("tag_head")
      tag_head = lerp_tag(tag_head_a, tag_head_b, torso_frac) if tag_head_a and tag_head_b else tag_head_a

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()

    rt, rp = math.radians(yaw), math.radians(pitch)
    ex = cx + dist * math.cos(rp) * math.sin(rt)
    ey = cy + dist * math.cos(rp) * math.cos(rt)
    ez = cz + dist * math.sin(rp)
    gluLookAt(ex, ey, ez, cx, cy, cz, 0.0, 0.0, 1.0)

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE if wireframe else GL_FILL)
    glDisable(GL_CULL_FACE)

    active_textures = textures if textured else None

    draw_surfaces(legs, legs_frame, lambda v: v, (0.45, 0.55, 0.75), legs_next, legs_frac, active_textures)

    if tag_torso:
      draw_surfaces(torso, torso_frame, lambda v: apply_tag(tag_torso, v), (0.8, 0.55, 0.25),
                    torso_next, torso_frac, active_textures)

      if head and tag_head:
        head_transform = lambda v: apply_tag(tag_torso, apply_tag(tag_head, v))
        draw_surfaces(head, 0, head_transform, (0.85, 0.85, 0.8), textures=active_textures)

    frame_in_anim = frame_for(anim, elapsed) - anim.first_frame + 1
    overlay_lines = [
      f"{anim.name}   frame {frame_in_anim}/{anim.num_frames}   {'paused' if paused else 'playing'}",
      f"interpolate [l]: {'on' if interpolate else 'off'}   "
      f"texture [t]: {'on' if textured else 'off' if textures else 'n/a'}   wireframe [w]: {'on' if wireframe else 'off'}",
      "space: pause/play    ←/→: step frame    tab / shift+tab: next / prev anim",
      "drag: orbit    scroll: zoom    esc: quit",
    ]
    draw_overlay(font, h, overlay_lines)

    pygame.display.flip()

  pygame.quit()


if __name__ == "__main__":
  main()
