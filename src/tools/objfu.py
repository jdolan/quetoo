#!/usr/bin/env python3
"""OBJ viewer with Quetoo usemtl path support and muzzle editing."""

import argparse
import math
import re
from pathlib import Path

import pygame
from OpenGL.GL import (
  GL_BLEND,
  GL_COLOR_BUFFER_BIT,
  GL_DEPTH_BUFFER_BIT,
  GL_DEPTH_TEST,
  GL_LINES,
  GL_LINEAR,
  GL_MODELVIEW,
  GL_ONE_MINUS_SRC_ALPHA,
  GL_PROJECTION,
  GL_QUADS,
  GL_RGBA,
  GL_SRC_ALPHA,
  GL_TEXTURE_2D,
  GL_TEXTURE_MAG_FILTER,
  GL_TEXTURE_MIN_FILTER,
  GL_TRIANGLES,
  GL_UNSIGNED_BYTE,
  glBegin,
  glBindTexture,
  glBlendFunc,
  glClear,
  glClearColor,
  glColor3f,
  glDeleteTextures,
  glDisable,
  glEnable,
  glEnd,
  glGenTextures,
  glLineWidth,
  glLoadIdentity,
  glMatrixMode,
  glOrtho,
  glPopMatrix,
  glPushMatrix,
  glTexCoord2f,
  glTexImage2D,
  glTexParameteri,
  glVertex2f,
  glVertex3f,
)
from OpenGL.GLU import gluLookAt, gluPerspective
from pygame.locals import DOUBLEBUF, KEYDOWN, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEMOTION, MOUSEWHEEL, OPENGL, QUIT


def obj_to_quake_space(v: tuple[float, float, float]) -> tuple[float, float, float]:
  # Match Quetoo R_LoadMeshModel OBJ axis transform: (x, z, y)
  return (v[0], v[2], v[1])


def load_obj(path: Path):
  verts: list[tuple[float, float, float]] = []
  texcoords: list[tuple[float, float]] = []
  faces_by_mtl: dict[str, list[list[tuple[int, int | None]]]] = {}
  current_mtl = ""

  with path.open() as f:
    for raw in f:
      line = raw.strip()
      if not line or line.startswith("#"):
        continue
      if line.startswith("v "):
        _, x, y, z = line.split()[:4]
        verts.append(obj_to_quake_space((float(x), float(y), float(z))))
      elif line.startswith("vt "):
        parts = line.split()
        texcoords.append((float(parts[1]), float(parts[2])))
      elif line.startswith("usemtl "):
        current_mtl = line[7:].strip()
      elif line.startswith("f "):
        tokens = line.split()[1:]
        poly: list[tuple[int, int | None]] = []
        for tok in tokens:
          parts = tok.split("/")
          vi = int(parts[0]) - 1
          ti = int(parts[1]) - 1 if len(parts) > 1 and parts[1] else None
          poly.append((vi, ti))
        if len(poly) >= 3:
          faces_by_mtl.setdefault(current_mtl, [])
          for i in range(1, len(poly) - 1):
            faces_by_mtl[current_mtl].append([poly[0], poly[i], poly[i + 1]])

  return verts, texcoords, faces_by_mtl


def resolve_quetoo_material(material: str, search_paths: list[Path], model_dir: Path) -> Path | None:
  if not material:
    return None

  rel = Path(material)
  image_exts = [".tga", ".png", ".jpg", ".jpeg"]

  def material_candidates(base: Path) -> list[Path]:
    out = [base]
    suffix = base.suffix.lower()
    if suffix not in image_exts:
      out.extend([Path(f"{base}{ext}") for ext in image_exts])
    return out

  local_candidates = [model_dir / rel]
  if not str(rel).startswith("models/"):
    local_candidates.append(model_dir / rel.name)
  for cand in local_candidates:
    for c in material_candidates(cand):
      if c.is_file():
        return c

  for base in search_paths:
    candidates = [
      base / "models" / rel,
      base / rel,
    ]
    if str(rel).startswith("models/"):
      candidates.insert(0, base / rel)
    for cand in candidates:
      for c in material_candidates(cand):
        if c.is_file():
          return c
  return None


def resolve_obj_path(obj_arg: str, search_paths: list[Path]) -> Path:
  candidate = Path(obj_arg).expanduser()
  if candidate.is_absolute():
    return candidate.resolve() if candidate.is_file() else candidate

  cwd_candidate = Path.cwd() / candidate
  if cwd_candidate.is_file():
    return cwd_candidate.resolve()

  for base in search_paths:
    p = base / candidate
    if p.is_file():
      return p.resolve()
    if not str(candidate).startswith("models/"):
      p2 = base / "models" / candidate
      if p2.is_file():
        return p2.resolve()

  if search_paths:
    return search_paths[0] / candidate
  return cwd_candidate


def load_texture(path: Path) -> int:
  surf = pygame.image.load(str(path)).convert_alpha()
  data = pygame.image.tostring(surf, "RGBA", True)
  w, h = surf.get_size()
  tex = glGenTextures(1)
  glBindTexture(GL_TEXTURE_2D, tex)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data)
  return tex


def bounds(verts: list[tuple[float, float, float]]):
  xs = [v[0] for v in verts]
  ys = [v[1] for v in verts]
  zs = [v[2] for v in verts]
  return (min(xs), max(xs)), (min(ys), max(ys)), (min(zs), max(zs))


def read_cfg_without_muzzle(path: Path) -> list[str]:
  if not path.is_file():
    return []
  return [line for line in path.read_text().splitlines(keepends=True) if not line.strip().startswith("muzzle")]


def save_muzzle(model_dir: Path, muzzle: tuple[float, float, float]) -> list[Path]:
  line = f'muzzle "{muzzle[0]:.0f} {muzzle[1]:.0f} {muzzle[2]:.0f}"\n'
  written: list[Path] = []
  for cfg_name in ("link.cfg", "view.cfg"):
    cfg_path = model_dir / cfg_name
    lines = read_cfg_without_muzzle(cfg_path)
    lines.append(line)
    cfg_path.write_text("".join(lines))
    written.append(cfg_path)
  return written


def load_muzzle(model_dir: Path) -> tuple[float, float, float] | None:
  link_cfg = model_dir / "link.cfg"
  if not link_cfg.is_file():
    return None
  pat = re.compile(r'\s*muzzle\s+"([^"]+)"')
  for line in link_cfg.read_text().splitlines():
    m = pat.match(line)
    if not m:
      continue
    vals = m.group(1).split()
    if len(vals) != 3:
      continue
    try:
      return (float(vals[0]), float(vals[1]), float(vals[2]))
    except ValueError:
      continue
  return None


def draw_axes(extent: float):
  s = extent * 0.15
  glDisable(GL_TEXTURE_2D)
  glLineWidth(1.5)
  glBegin(GL_LINES)
  glColor3f(1.0, 0.3, 0.3); glVertex3f(0.0, 0.0, 0.0); glVertex3f(s, 0.0, 0.0)
  glColor3f(0.3, 1.0, 0.3); glVertex3f(0.0, 0.0, 0.0); glVertex3f(0.0, s, 0.0)
  glColor3f(0.3, 0.5, 1.0); glVertex3f(0.0, 0.0, 0.0); glVertex3f(0.0, 0.0, s)
  glEnd()
  glLineWidth(1.0)


def draw_crosshair(pos: tuple[float, float, float], size: float):
  x, y, z = pos
  glDisable(GL_TEXTURE_2D)
  glLineWidth(2.5)
  glBegin(GL_LINES)
  glColor3f(1.0, 0.15, 0.0); glVertex3f(x - size, y, z); glVertex3f(x + size, y, z)
  glColor3f(0.1, 1.0, 0.1); glVertex3f(x, y - size, z); glVertex3f(x, y + size, z)
  glColor3f(0.2, 0.5, 1.0); glVertex3f(x, y, z - size); glVertex3f(x, y, z + size)
  glEnd()
  glLineWidth(1.0)


def draw_text(font, text: str, x: int, y: int, w: int, h: int, color=(255, 215, 50)):
  surf = font.render(text, True, color)
  tw, th = surf.get_size()
  data = pygame.image.tostring(pygame.transform.flip(surf, False, True), "RGBA", False)

  tex = glGenTextures(1)
  glBindTexture(GL_TEXTURE_2D, tex)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, data)

  glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity()
  glOrtho(0, w, 0, h, -1, 1)
  glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity()

  glDisable(GL_DEPTH_TEST)
  glEnable(GL_TEXTURE_2D)
  glEnable(GL_BLEND)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  glColor3f(1.0, 1.0, 1.0)

  yy = h - y - th
  glBegin(GL_QUADS)
  glTexCoord2f(0.0, 0.0); glVertex2f(x, yy)
  glTexCoord2f(1.0, 0.0); glVertex2f(x + tw, yy)
  glTexCoord2f(1.0, 1.0); glVertex2f(x + tw, yy + th)
  glTexCoord2f(0.0, 1.0); glVertex2f(x, yy + th)
  glEnd()

  glBindTexture(GL_TEXTURE_2D, 0)
  glDeleteTextures([tex])
  glDisable(GL_BLEND)
  glEnable(GL_DEPTH_TEST)
  glMatrixMode(GL_PROJECTION); glPopMatrix()
  glMatrixMode(GL_MODELVIEW); glPopMatrix()


def main():
  parser = argparse.ArgumentParser(description="OBJ viewer with Quetoo usemtl path support and muzzle editing")
  parser.add_argument("-p", "--path", action="append", default=[], help="Quetoo data root search path")
  parser.add_argument("obj", help="OBJ path")
  args = parser.parse_args()

  search_paths = [Path(p).expanduser().resolve() for p in args.path]
  if not search_paths:
    search_paths.append((Path(__file__).resolve().parents[3] / ".." / "quetoo-data" / "target" / "default").resolve())

  obj_path = resolve_obj_path(args.obj, search_paths)
  if not obj_path.is_file():
    raise SystemExit(f"OBJ not found: {obj_path}")

  verts, texcoords, faces_by_mtl = load_obj(obj_path)
  if not verts:
    raise SystemExit("OBJ has no vertices")

  (minx, maxx), (miny, maxy), (minz, maxz) = bounds(verts)
  cx, cy, cz = (minx + maxx) * 0.5, (miny + maxy) * 0.5, (minz + maxz) * 0.5
  extent = max(maxx - minx, maxy - miny, maxz - minz) or 1.0

  muzzle = load_muzzle(obj_path.parent)
  if muzzle is None:
    muzzle = (round(cx), round(cy), round(cz))
  else:
    print(f"Loaded muzzle from link.cfg: {muzzle}")

  pygame.init()
  pygame.key.set_repeat(400, 30)
  w, h = 1280, 800
  pygame.display.set_mode((w, h), DOUBLEBUF | OPENGL)
  pygame.display.set_caption(f"obj-fu · {obj_path.name}")
  font_big = pygame.font.SysFont("menlo", 20, bold=True)
  font_small = pygame.font.SysFont("menlo", 16)

  glEnable(GL_DEPTH_TEST)
  glEnable(GL_TEXTURE_2D)
  glClearColor(0.10, 0.11, 0.13, 1.0)

  glMatrixMode(GL_PROJECTION)
  glLoadIdentity()
  gluPerspective(50.0, w / h, max(0.01, extent * 0.01), extent * 100.0)
  glMatrixMode(GL_MODELVIEW)

  tex_by_mtl: dict[str, int | None] = {}
  for mtl in faces_by_mtl.keys():
    p = resolve_quetoo_material(mtl, search_paths, obj_path.parent)
    tex_by_mtl[mtl] = load_texture(p) if p else None
    if p:
      print(f"usemtl {mtl} -> {p}")
    elif mtl:
      print(f"usemtl {mtl} -> (not found)")

  yaw = 35.0
  pitch = 20.0
  dist = extent * 2.3
  orbiting = False
  last = (0, 0)
  status_msg = ""
  clock = pygame.time.Clock()

  running = True
  while running:
    for event in pygame.event.get():
      if event.type == QUIT:
        running = False
      elif event.type == KEYDOWN:
        step = 5.0 if (pygame.key.get_mods() & pygame.KMOD_SHIFT) else 1.0
        mx, my, mz = muzzle
        if event.key == pygame.K_ESCAPE:
          running = False
        elif event.key == pygame.K_LEFT:
          muzzle = (mx + step, my, mz)
        elif event.key == pygame.K_RIGHT:
          muzzle = (mx - step, my, mz)
        elif event.key == pygame.K_UP:
          muzzle = (mx, my + step, mz)
        elif event.key == pygame.K_DOWN:
          muzzle = (mx, my - step, mz)
        elif event.key in (pygame.K_q, pygame.K_PAGEDOWN):
          muzzle = (mx, my, mz + step)
        elif event.key in (pygame.K_e, pygame.K_PAGEUP):
          muzzle = (mx, my, mz - step)
        elif event.key == pygame.K_r:
          muzzle = (round(cx), round(cy), round(cz))
          status_msg = "Muzzle reset to centroid"
        elif event.key == pygame.K_s:
          written = save_muzzle(obj_path.parent, muzzle)
          status_msg = f'Saved muzzle "{muzzle[0]:.0f} {muzzle[1]:.0f} {muzzle[2]:.0f}"'
          print(status_msg)
          print("  -> " + ", ".join(str(p) for p in written))
      elif event.type == MOUSEBUTTONDOWN and event.button == 1:
        orbiting = True
        last = event.pos
      elif event.type == MOUSEBUTTONUP and event.button == 1:
        orbiting = False
      elif event.type == MOUSEMOTION and orbiting:
        dx = event.pos[0] - last[0]
        dy = event.pos[1] - last[1]
        yaw += dx * 0.35
        pitch += dy * 0.35
        last = event.pos
      elif event.type == MOUSEWHEEL:
        dist = max(extent * 0.15, dist - event.y * extent * 0.08)

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()

    rt = math.radians(yaw)
    rp = math.radians(pitch)
    ex = cx + dist * math.cos(rp) * math.sin(rt)
    ey = cy + dist * math.cos(rp) * math.cos(rt)
    ez = cz + dist * math.sin(rp)
    upz = -1.0 if 90.0 < (pitch % 360.0) < 270.0 else 1.0
    gluLookAt(ex, ey, ez, cx, cy, cz, 0.0, 0.0, upz)

    for mtl, tris in faces_by_mtl.items():
      tex = tex_by_mtl.get(mtl)
      if tex:
        glEnable(GL_TEXTURE_2D)
        glBindTexture(GL_TEXTURE_2D, tex)
      else:
        glDisable(GL_TEXTURE_2D)
      glColor3f(1.0, 1.0, 1.0)

      glBegin(GL_TRIANGLES)
      for tri in tris:
        for vi, ti in tri:
          if ti is not None and 0 <= ti < len(texcoords):
            u, v = texcoords[ti]
            glTexCoord2f(u, v)
          x, y, z = verts[vi]
          glVertex3f(x, y, z)
      glEnd()

    draw_axes(extent)
    draw_crosshair(muzzle, size=extent * 0.035)

    draw_text(font_big, f'muzzle  x:{muzzle[0]:+.0f}  y:{muzzle[1]:+.0f}  z:{muzzle[2]:+.0f}  (Quake/Quetoo space)', 12, 10, w, h)
    help_lines = [
      "← → : X      ↑ ↓ : Y      Q/E : Z      Shift : x5",
      "Drag : orbit      Scroll : dolly      S : save      R : reset",
    ]
    for i, txt in enumerate(help_lines):
      draw_text(font_small, txt, 12, h - 14 - (i + 1) * 22, w, h, color=(170, 170, 170))
    if status_msg:
      draw_text(font_small, status_msg, 12, 42, w, h, color=(100, 255, 130))

    pygame.display.flip()
    clock.tick(60)

  pygame.quit()


if __name__ == "__main__":
  main()
