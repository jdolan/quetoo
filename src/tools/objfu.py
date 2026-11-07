#!/usr/bin/env python3
"""OBJ viewer, muzzle helper and live Quetoo mesh configuration editor."""

import argparse
import fnmatch
import math
import multiprocessing
import re
import shlex
from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pygame
from PIL import Image, ImageDraw
from md3 import parse_skin, read_md3
from md3fu import parse_animation_cfg
from OpenGL.GL import (
  GL_BLEND,
  GL_COMPILE,
  GL_COLOR_BUFFER_BIT,
  GL_DEPTH_BUFFER_BIT,
  GL_DEPTH_TEST,
  GL_LINES,
  GL_LINEAR,
  GL_MODELVIEW,
  GL_MODELVIEW_MATRIX,
  GL_ONE_MINUS_SRC_ALPHA,
  GL_PROJECTION,
  GL_PROJECTION_MATRIX,
  GL_QUADS,
  GL_RGBA,
  GL_SRC_ALPHA,
  GL_TEXTURE_2D,
  GL_TEXTURE_MAG_FILTER,
  GL_TEXTURE_MIN_FILTER,
  GL_TRIANGLES,
  GL_UNSIGNED_BYTE,
  GL_VIEWPORT,
  glBegin,
  glBindTexture,
  glBlendFunc,
  glCallList,
  glClear,
  glClearColor,
  glColor3f,
  glDeleteLists,
  glDeleteTextures,
  glDisable,
  glEnable,
  glEnd,
  glEndList,
  glGenLists,
  glGenTextures,
  glGetDoublev,
  glGetIntegerv,
  glLineWidth,
  glLoadIdentity,
  glMatrixMode,
  glMultMatrixd,
  glNewList,
  glOrtho,
  glPopMatrix,
  glPushMatrix,
  glTexCoord2f,
  glTexImage2D,
  glTexParameteri,
  glViewport,
  glTranslatef,
  glVertex2f,
  glVertex3f,
)
from OpenGL.GLU import gluLookAt, gluPerspective, gluProject, gluUnProject
from pygame.locals import (DOUBLEBUF, KEYDOWN, MOUSEBUTTONDOWN, MOUSEBUTTONUP, MOUSEMOTION, MOUSEWHEEL, OPENGL, QUIT,
                           RESIZABLE, WINDOWSIZECHANGED)

# ITEM_BOUNDS in src/game/common/g_item.c. Items fall until this box rests on the floor.
ITEM_MINS = (-16, -16, -16)
ITEM_MAXS = (16, 16, 32)


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


def save_muzzle(model_dir: Path, muzzle: tuple[float, float, float]) -> list[Path]:
  path = model_dir / "model.cfg"
  text = path.read_text() if path.exists() else ""
  text = replace_model_section(text, "muzzle", f'"origin" "{" ".join(f"{v:g}" for v in muzzle)}"\n')
  path.write_text(text)
  return [path]


def load_muzzle(model_dir: Path) -> tuple[float, float, float] | None:
  path = model_dir / "model.cfg"
  if not path.is_file():
    return None
  configs = parse_model_config(path.read_text())
  return configs["muzzle"].muzzle if "muzzle" in configs else None


def draw_axes(extent: float):
  s = extent * 0.15
  glDisable(GL_TEXTURE_2D)
  glDisable(GL_DEPTH_TEST)
  glLineWidth(1.5)
  glBegin(GL_LINES)
  glColor3f(1.0, 0.3, 0.3); glVertex3f(0.0, 0.0, 0.0); glVertex3f(s, 0.0, 0.0)
  glColor3f(0.3, 1.0, 0.3); glVertex3f(0.0, 0.0, 0.0); glVertex3f(0.0, s, 0.0)
  glColor3f(0.3, 0.5, 1.0); glVertex3f(0.0, 0.0, 0.0); glVertex3f(0.0, 0.0, s)
  glEnd()
  glLineWidth(1.0)
  glEnable(GL_DEPTH_TEST)


def draw_box(mins, maxs, color=(1.0, 0.85, 0.2)):
  glDisable(GL_TEXTURE_2D)
  glColor3f(*color)
  glBegin(GL_LINES)
  corners = [(x, y, z) for x in (mins[0], maxs[0]) for y in (mins[1], maxs[1]) for z in (mins[2], maxs[2])]
  for i, a in enumerate(corners):
    for b in corners[i + 1:]:
      if sum(p != q for p, q in zip(a, b)) == 1:
        glVertex3f(*a)
        glVertex3f(*b)
  glEnd()


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


@dataclass
class MeshConfig:
  transform: np.ndarray
  muzzle: tuple[float, float, float] = (0.0, 0.0, 0.0)


def parse_mesh_config(text: str) -> MeshConfig:
  """Match R_LoadMeshConfig's ordered T/Rx/Ry/Rz/S concatenation."""
  config = MeshConfig(np.eye(4))
  # PARSER_DEFAULT recognizes // comments, but not block comments.
  text = re.sub(r'"[^"\n]*"|//[^\n]*',
                lambda m: m[0] if m[0].startswith('"') else "", text)
  for line_number, line in enumerate(text.splitlines(), 1):
    try:
      tokens = shlex.split(line, posix=False)
      while tokens:
        command = tokens.pop(0).strip('"')
        count = {"translate": 3, "rotate": 3, "scale": 1, "muzzle": 3}.get(command)
        if count is None:
          raise ValueError(f"Unknown directive: {command}")
        if tokens and tokens[0].startswith('"'):
          # Parse_Primitive discards excess values inside a quoted token.
          values = [float(v) for v in tokens.pop(0)[1:-1].split()[:count]]
        else:
          values = [float(v) for v in tokens[:count]]
          del tokens[:count]
        if len(values) != count or not all(math.isfinite(v) for v in values):
          raise ValueError(f"{command} requires {count} finite number(s)")
        if command == "muzzle":
          config.muzzle = tuple(values)
          continue
        matrix = np.eye(4)
        if command == "translate":
          matrix[:3, 3] = values
        elif command == "scale":
          matrix[:3, :3] *= values[0]
        else:
          for axis, angle in enumerate(values):
            rotation = np.eye(4)
            a, b = ((1, 2), (2, 0), (0, 1))[axis]
            c, s = math.cos(math.radians(angle)), math.sin(math.radians(angle))
            rotation[a, a] = rotation[b, b] = c
            rotation[a, b], rotation[b, a] = -s, s
            matrix = matrix @ rotation
        config.transform = config.transform @ matrix
    except ValueError as error:
      raise ValueError(f"Line {line_number}: {error}") from error
  return config


def model_sections(text):
  """Flat entity-style blocks, retaining source ranges for isolated edits."""
  tokens = list(re.finditer(r'//[^\n]*|/\*.*?\*/|\#[^\n]*|"[^"]*"|[{}]|[^\s{}"]+', text, re.S))
  tokens = [t for t in tokens if not t[0].startswith(("//", "/*", "#"))]
  sections = {}
  i = 0
  while i < len(tokens):
    start = tokens[i].start()
    if tokens[i][0] != "{" or i + 3 >= len(tokens) or tokens[i + 1][0].strip('"') != "type":
      raise ValueError("Expected a block beginning with \"type\"")
    name = tokens[i + 2][0].strip('"')
    if name not in ("world", "link", "view", "muzzle") or name in sections:
      raise ValueError(f"Unknown or duplicate model config type: {name}")
    body_start = tokens[i + 2].end()
    i += 3
    while i < len(tokens) and tokens[i][0] != "}":
      if tokens[i][0] == "{":
        raise ValueError("Nested model config blocks are not supported")
      i += 1
    if i == len(tokens):
      raise ValueError(f"Unclosed {name} block")
    sections[name] = (start, tokens[i].end(), text[body_start:tokens[i].start()])
    i += 1
  return sections


def parse_model_config(text):
  configs = {}
  for name, (_, _, body) in model_sections(text).items():
    if name == "muzzle":
      tokens = shlex.split(re.sub(r'//[^\n]*|/\*.*?\*/|\#[^\n]*', "", body, flags=re.S))
      if len(tokens) != 2 or tokens[0] != "origin":
        raise ValueError("Muzzle block requires one quoted origin vector")
      config = parse_mesh_config(f'muzzle "{tokens[1]}"')
    else:
      config = parse_mesh_config(re.sub(r'//[^\n]*|/\*.*?\*/|\#[^\n]*', "", body, flags=re.S))
      if re.search(r'\bmuzzle\b', re.sub(r'//[^\n]*', "", body)):
        raise ValueError("Muzzle belongs in its own block")
    configs[name] = config
  muzzle = configs.get("muzzle", MeshConfig(np.eye(4))).muzzle
  for config in configs.values():
    config.muzzle = muzzle
  return configs


def replace_model_section(text, name, body):
  parse_model_config(text)
  sections = model_sections(text)
  block = f'{{\n"type" "{name}"\n{body.strip()}\n}}\n'
  if name in sections:
    start, end, _ = sections[name]
    return text[:start] + block.rstrip("\n") + text[end:]
  return text.rstrip() + ("\n\n" if text.strip() else "") + block


def model_config(text, mode):
  configs = parse_model_config(text)
  return configs.get(mode, MeshConfig(np.eye(4), configs.get("muzzle", MeshConfig(np.eye(4))).muzzle))


def transform_vertices(matrix, verts):
  return (np.asarray(verts) @ matrix[:3, :3].T + matrix[:3, 3]).tolist()


def tag_matrix(tag):
  origin, *axes = tag
  matrix = np.eye(4)
  matrix[:3, :3] = np.asarray(axes).T
  matrix[:3, 3] = origin
  return matrix


def ground_config(text, verts, floor=ITEM_MINS[2]):
  """Seat the mesh on the floor below a resting item's origin."""
  minimum = min(v[2] for v in transform_vertices(parse_mesh_config(text).transform, verts))
  # Prepending translates in world space even if the file rotates or scales first.
  return f"translate 0 0 {round(floor - minimum)}\n{text}"


def default_config(verts, mode):
  text = 'translate "0 0 0"\nscale "1"\n'
  if mode != "world":
    (minx, maxx), (miny, maxy), (minz, maxz) = bounds(verts)
    text += f'muzzle "{round(maxx)} {round((miny + maxy) / 2)} {round((minz + maxz) / 2)}"\n'
  return text


def view_vertical_fov(reference_fov, aspect):
  return math.degrees(2 * math.atan(math.tan(math.radians(reference_fov / 2)) / min(aspect, 16 / 9)))


def view_transform(transform, hand):
  transform = transform.copy()
  transform[1, 3] *= {"right": 1, "left": -1, "center": 0}[hand]
  return transform


def load_player(player_dir):
  """Build a static, textured legs -> torso -> head rig and its weapon anchor."""
  animations = {a.name: a.first_frame for a in parse_animation_cfg(player_dir / "animation.cfg")}
  skin = parse_skin(player_dir / "default.skin")
  parts = []
  transforms = {}
  models = {}
  frames = {"lower": animations["LEGS_IDLE"], "upper": animations["TORSO_STAND1"], "head": 0}
  for name, parent, tag in (("lower", None, None), ("upper", "lower", "tag_torso"),
                            ("head", "upper", "tag_head")):
    model = models[name] = read_md3(player_dir / f"{name}.md3")
    frame = frames[name]
    if frame < 0 or frame >= model.num_frames:
      raise ValueError(f"{player_dir}/{name}.md3: invalid standing frame {frame}")
    matrix = np.eye(4)
    if parent:
      matrix = transforms[parent] @ tag_matrix(models[parent].tags[frames[parent]][tag])
    cfg_path = player_dir / "model.cfg"
    if cfg_path.is_file():
      matrix = matrix @ model_config(cfg_path.read_text(), "link" if parent else "world").transform
    transforms[name] = matrix
    for surface in model.surfaces:
      if surface.name not in skin:
        raise ValueError(f"{player_dir}/default.skin: missing surface {surface.name}")
      verts = transform_vertices(matrix, surface.frames[frame])
      faces = [[(i, i) for i in triangle] for triangle in surface.triangles]
      # MD3 uses V-down UVs; load_texture uploads flipped images for OBJ's V-up.
      uv = [(s, 1.0 - t) for s, t in surface.texcoords]
      parts.append((verts, uv, {skin[surface.name]: faces}))
  weapon = transforms["upper"] @ tag_matrix(models["upper"].tags[frames["upper"]]["tag_weapon"])
  floor = np.eye(4)
  floor[2, 3] = -min(v[2] for verts, _, _ in parts for v in verts)
  parts = [(transform_vertices(floor, verts), uv, faces) for verts, uv, faces in parts]
  return parts, floor @ weapon


class ConfigEditor:
  """Native text editor for one or more configurations, hosted separately from SDL's event loop."""

  def __init__(self, entries, mode):
    import tkinter as tk
    from tkinter import messagebox, scrolledtext, ttk

    self.tk = tk
    self.messagebox = messagebox
    self.paths = [path for path, _ in entries]
    self.models = [verts for _, verts in entries]
    self.baselines = [path.read_text() if path.exists() else None for path in self.paths]
    self.mode = mode
    self.buffers = [baseline if baseline is not None else replace_model_section(
                      "", mode, '"origin" "0 0 0"' if mode == "muzzle" else 'translate "0 0 0"\nscale "1"')
                    for baseline, verts in zip(self.baselines, self.models)]
    self.configs = []
    for text in self.buffers:
      try:
        self.configs.append(model_config(text, mode))
      except ValueError:
        self.configs.append(None)
    self.index = 0
    self.error = ""
    self.running = True
    self.root = tk.Tk()
    self.root.geometry("520x480")
    self.root.protocol("WM_DELETE_WINDOW", self.close)
    buttons = ttk.Frame(self.root)
    buttons.pack(fill="x")
    ttk.Button(buttons, text="Save", command=self.save).pack(side="left")
    if len(self.paths) > 1:
      ttk.Button(buttons, text="Save all", command=self.save_all).pack(side="left")
    ttk.Button(buttons, text="Reload", command=self.reload).pack(side="left")
    if mode == "world":
      ttk.Button(buttons, text="Rest on floor", command=self.ground).pack(side="left")
    self.selector = None
    if len(self.paths) > 1:
      self.selector = ttk.Combobox(self.root, state="readonly")
      self.selector.bind("<<ComboboxSelected>>", lambda event: self.select(self.selector.current()))
      self.selector.pack(fill="x")
    self.text = scrolledtext.ScrolledText(self.root, undo=True, wrap="none", font=("Menlo", 14))
    self.text.pack(fill="both", expand=True)
    self.status = tk.StringVar()
    ttk.Label(self.root, textvariable=self.status, wraplength=500).pack(fill="x")
    self.text.insert("1.0", self.buffers[0])
    self.text.edit_modified(False)
    self.text.bind("<<Modified>>", self.changed)
    self.changed()

  @property
  def path(self):
    return self.paths[self.index]

  @property
  def verts(self):
    return self.models[self.index]

  @property
  def config(self):
    return self.configs[self.index]

  @property
  def baseline(self):
    return self.baselines[self.index]

  def dirty(self, index):
    return self.buffers[index] != (self.baselines[index] or "")

  def contents(self):
    return self.text.get("1.0", "end-1c")

  def changed(self, event=None):
    self.text.edit_modified(False)
    self.buffers[self.index] = self.contents()
    self.root.title(f"objfu - {self.path} ({self.mode})")
    if self.selector is not None:
      self.selector["values"] = [f"{'* ' if self.dirty(i) else ''}{model_name(path)}"
                                 for i, path in enumerate(self.paths)]
      self.selector.current(self.index)
    try:
      self.configs[self.index] = model_config(self.contents(), self.mode)
      self.error = ""
      state = "unsaved" if self.dirty(self.index) else "saved"
      if self.baseline is None:
        state = "new file; Save to create"
      others = sum(self.dirty(i) for i in range(len(self.paths)) if i != self.index)
      extra = f" {others} other file(s) unsaved." if others else ""
      self.status.set(f"{self.path.name}: {state}. Preview updates live.{extra}")
    except ValueError as error:
      self.error = str(error)
      self.status.set(f"{error}. Preview retains last valid configuration; cannot save.")

  def replace(self, text):
    self.text.delete("1.0", "end")
    self.text.insert("1.0", text)
    self.changed()

  def select(self, index):
    if index == self.index or not 0 <= index < len(self.paths):
      return
    self.buffers[self.index] = self.contents()
    self.index = index
    self.replace(self.buffers[index])
    self.text.edit_reset()

  def write(self, index):
    try:
      parse_model_config(self.buffers[index])
    except ValueError as error:
      self.messagebox.showerror("Invalid configuration", f"{self.paths[index]}: {error}", parent=self.root)
      return False
    path = self.paths[index]
    try:
      disk = path.read_text() if path.exists() else None
      if disk != self.baselines[index] and not self.messagebox.askyesno(
          "File changed externally", f"Overwrite external changes to {path}?", parent=self.root):
        return False
      path.write_text(self.buffers[index])
      self.baselines[index] = self.buffers[index]
    except OSError as error:
      self.messagebox.showerror("Save failed", str(error), parent=self.root)
      return False
    return True

  def save(self):
    self.changed()
    saved = self.write(self.index)
    self.changed()
    return saved

  def save_all(self):
    self.changed()
    saved = all(self.write(i) for i in range(len(self.paths)) if self.dirty(i))
    self.changed()
    return saved

  def reload(self):
    if self.dirty(self.index) and not self.messagebox.askyesno(
        "Discard edits", "Discard unsaved edits and reload?", parent=self.root):
      return
    try:
      self.baselines[self.index] = self.path.read_text() if self.path.exists() else None
    except OSError as error:
      self.messagebox.showerror("Reload failed", str(error), parent=self.root)
      return
    self.replace(self.baseline or "")
    self.text.edit_reset()

  def ground(self):
    try:
      text = self.contents()
      section = model_sections(text).get("world", (0, 0, ""))[2]
      self.replace(replace_model_section(text, "world", ground_config(section, self.verts)))
    except ValueError as error:
      self.messagebox.showerror("Invalid configuration", str(error), parent=self.root)

  def close(self):
    self.changed()
    if any(self.dirty(i) for i in range(len(self.paths))):
      save = self.messagebox.askyesnocancel("Unsaved edits", "Save all edited files before closing?",
                                            parent=self.root)
      if save is None or (save and not self.save_all()):
        return
    self.running = False


def run_config_editor(entries, mode, connection):
  editor = ConfigEditor(entries, mode)
  last_state = None

  def update():
    nonlocal last_state
    while connection.poll():
      message = connection.recv()
      if message == "close":
        editor.close()
      elif message[0] == "select":
        editor.select(message[1])
    state = (editor.running, editor.index, editor.contents(), editor.error)
    if state != last_state:
      connection.send((editor.running, editor.index, editor.configs, editor.error))
      last_state = state
    if editor.running:
      editor.root.after(50, update)
    else:
      editor.root.destroy()

  try:
    update()
    editor.root.mainloop()
  finally:
    connection.close()


class ConfigPreview:
  def __init__(self, entries, mode):
    # Tk and SDL cannot share NSApplication on macOS. Spawn also keeps native
    # text editing and modal file dialogs independent of the rendering loop.
    context = multiprocessing.get_context("spawn")
    self.connection, child = context.Pipe()
    self.process = context.Process(target=run_config_editor, args=(entries, mode, child))
    self.process.start()
    child.close()
    self.running = True
    self.index = 0
    self.configs = [None] * len(entries)
    self.error = ""
    self.selected = True

  @property
  def config(self):
    return self.configs[self.index]

  def update(self):
    while self.connection.poll():
      try:
        self.running, self.index, self.configs, self.error = self.connection.recv()
      except EOFError:
        if self.running:
          raise RuntimeError("Configuration editor exited unexpectedly") from None
        break
    if self.running and not self.process.is_alive():
      raise RuntimeError(f"Configuration editor exited with status {self.process.exitcode}")

  def select(self, index):
    self.selected = True
    self.connection.send(("select", index))

  def deselect(self):
    self.selected = False

  def close(self):
    self.connection.send("close")

  def finish(self):
    if self.running and self.process.is_alive():
      self.close()
    self.process.join()
    self.connection.close()


def draw_mesh(verts, texcoords, faces_by_mtl, textures):
  for material, triangles in faces_by_mtl.items():
    texture = textures.get(material)
    if texture:
      glEnable(GL_TEXTURE_2D)
      glBindTexture(GL_TEXTURE_2D, texture)
    else:
      glDisable(GL_TEXTURE_2D)
    glColor3f(1.0, 1.0, 1.0)
    glBegin(GL_TRIANGLES)
    for triangle in triangles:
      for vi, ti in triangle:
        if ti is not None:
          glTexCoord2f(*texcoords[ti])
        glVertex3f(*verts[vi])
    glEnd()


def draw_floor(size):
  glDisable(GL_TEXTURE_2D)
  glColor3f(0.22, 0.24, 0.27)
  glBegin(GL_QUADS)
  for x, y in ((-size, -size), (size, -size), (size, size), (-size, size)):
    glVertex3f(x, y, -0.05)
  glEnd()
  glColor3f(0.38, 0.40, 0.43)
  glBegin(GL_LINES)
  for position in range(-int(size), int(size) + 1, 16):
    glVertex3f(position, -size, 0)
    glVertex3f(position, size, 0)
    glVertex3f(-size, position, 0)
    glVertex3f(size, position, 0)
  glEnd()


def view_reference_models(search_paths):
  """Find the Quetoo weapon meshes that the view guide averages, first root winning."""
  references = {}
  for root in search_paths:
    directory = root / "models" / "weapons"
    for weapon in sorted(directory.glob("*")):
      if not weapon.is_dir() or weapon.name.startswith("quake"):
        continue
      if weapon.name in references:
        continue
      model = next((weapon / f"tris.{extension}" for extension in ("obj", "md3")
                    if (weapon / f"tris.{extension}").is_file()), None)
      if model is not None:
        references[weapon.name] = model.resolve()
  return list(references.values())


def view_reference(model, hand="right"):
  """Load a reference weapon mesh in first-person space, through its model.cfg view block."""
  config_path = model.parent / "model.cfg"
  try:
    config = model_config(config_path.read_text() if config_path.is_file() else "", "view")
  except ValueError as error:
    raise ValueError(f"{config_path}: {error}") from error
  if model.suffix == ".obj":
    verts, _, faces = load_obj(model)
    triangles = [[vi for vi, _ in triangle] for group in faces.values() for triangle in group]
  else:
    verts, triangles = [], []
    for surface in read_md3(model).surfaces:
      offset = len(verts)
      verts.extend(surface.frames[0])
      triangles.extend([[offset + i for i in triangle] for triangle in surface.triangles])
  if not verts or not triangles:
    raise ValueError(f"Silhouette reference has no triangles: {model}")
  print(f"View silhouette reference: {model.parent.name} ({config_path})")
  return transform_vertices(view_transform(config.transform, hand), verts), triangles


def view_references(search_paths, current_model, hand="right"):
  meshes = [view_reference(model, hand) for model in view_reference_models(search_paths)
            if model != current_model.resolve()]
  if not meshes:
    raise ValueError("No Quetoo weapon meshes found for the view silhouette in the data search paths")
  return meshes


def view_coverage(mesh, width, height, fov, hand):
  """Binary screen coverage of one reference weapon, from 0 to 1."""
  focal = height / (2 * math.tan(math.radians(view_vertical_fov(fov, width / height) / 2)))
  offset = {"right": -5, "left": 5, "center": 0}[hand]
  verts, triangles = mesh
  mask = Image.new("L", (width, height))
  draw = ImageDraw.Draw(mask)
  for triangle in triangles:
    polygon = [verts[i] for i in triangle]
    clipped = []
    for previous, point in zip(polygon[-1:] + polygon[:-1], polygon):
      if (previous[0] >= 0.1) != (point[0] >= 0.1):
        fraction = (0.1 - previous[0]) / (point[0] - previous[0])
        clipped.append([a + fraction * (b - a) for a, b in zip(previous, point)])
      if point[0] >= 0.1:
        clipped.append(point)
    if len(clipped) >= 3:
      projected = [(width / 2 - focal * (y + offset) / x, height / 2 - focal * z / x)
                   for x, y, z in clipped]
      draw.polygon(projected, fill=255)
  return np.asarray(mask, dtype=np.float32) / 255


def view_guide(coverages):
  """Average binary coverage per weapon, not triangle count or mesh density."""
  height, width = coverages[0].shape
  coverage = np.sum(coverages, axis=0)
  pixels = np.empty((height, width, 4), dtype=np.uint8)
  pixels[:, :, :3] = (90, 190, 220)
  pixels[:, :, 3] = np.rint(coverage / len(coverages) * 150).astype(np.uint8)
  return pixels


def view_silhouette(meshes, width, height, fov, hand):
  return view_guide([view_coverage(mesh, width, height, fov, hand) for mesh in meshes])


def draw_view_guide(texture, width, height):
  glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity()
  glOrtho(0, width, 0, height, -1, 1)
  glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity()
  glDisable(GL_DEPTH_TEST)
  glEnable(GL_TEXTURE_2D)
  glBindTexture(GL_TEXTURE_2D, texture)
  glEnable(GL_BLEND)
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
  glColor3f(1, 1, 1)
  glBegin(GL_QUADS)
  for u, v in ((0, 0), (1, 0), (1, 1), (0, 1)):
    glTexCoord2f(u, v)
    glVertex2f(u * width, v * height)
  glEnd()
  glDisable(GL_BLEND)
  glEnable(GL_DEPTH_TEST)
  glMatrixMode(GL_PROJECTION); glPopMatrix()
  glMatrixMode(GL_MODELVIEW); glPopMatrix()


def model_name(obj_path):
  """Name a model by its category and directory, e.g. ammo/quake2_bullets."""
  return f"{obj_path.parent.parent.name}/{obj_path.parent.name}"


def find_models(pattern, search_paths):
  """Match a glob against model directories under each root's models/, first root winning."""
  found = {}
  for root in search_paths:
    models = root / "models"
    if not models.is_dir():
      continue
    for obj in models.rglob("tris.obj"):
      name = obj.parent.relative_to(models).as_posix()
      if name not in found and any(fnmatch.fnmatchcase(candidate, pattern) for candidate in (
          name, f"models/{name}", f"{name}/tris.obj", f"models/{name}/tris.obj")):
        found[name] = obj.resolve()
  return [found[name] for name in sorted(found)]


def resolve_obj_args(patterns, search_paths):
  paths = []
  for pattern in patterns:
    if any(c in pattern for c in "*?["):
      matches = find_models(pattern, search_paths)
      if not matches:
        raise SystemExit(f"No models match: {pattern}")
      paths.extend(matches)
      continue
    path = resolve_obj_path(pattern, search_paths)
    if not path.is_file():
      path = resolve_obj_path(f"{pattern}/tris.obj", search_paths)
    if not path.is_file():
      raise SystemExit(f"OBJ not found: {pattern}")
    paths.append(path)
  return list(dict.fromkeys(paths))


def item_layout(models):
  """Place items on a centered grid of resting origins, spaced to clear the widest model."""
  radius = max([-ITEM_MINS[0], ITEM_MAXS[0]] + [max(abs(v[0]), abs(v[1])) for verts in models for v in verts])
  spacing = math.ceil((radius * 2 + 24) / 16) * 16
  columns = math.ceil(math.sqrt(len(models)))
  rows = math.ceil(len(models) / columns)
  origins = [((i % columns - (columns - 1) / 2) * spacing, ((rows - 1) / 2 - i // columns) * spacing,
              -ITEM_MINS[2]) for i in range(len(models))]
  return origins, spacing, columns, rows


def ray_box(origin, direction, mins, maxs):
  """Slab intersection; returns the entry distance along the ray, or None."""
  near, far = -math.inf, math.inf
  for o, d, lo, hi in zip(origin, direction, mins, maxs):
    if abs(d) < 1e-12:
      if not lo <= o <= hi:
        return None
      continue
    t0, t1 = sorted(((lo - o) / d, (hi - o) / d))
    near, far = max(near, t0), min(far, t1)
  if near > far or far < 0:
    return None
  return max(near, 0.0)


def pick_box(item_bounds, origin, direction):
  hits = [(t, i) for i, (mins, maxs) in enumerate(item_bounds)
          if (t := ray_box(origin, direction, mins, maxs)) is not None]
  return min(hits)[1] if hits else None


def orbit_offset(yaw, pitch, distance):
  rt, rp = math.radians(yaw), math.radians(pitch)
  return np.array((math.cos(rp) * math.sin(rt), math.cos(rp) * math.cos(rt), math.sin(rp))) * distance


def pan_delta(yaw, pitch, distance, fov, height, dx, dy):
  """Move the orbit target so the point beneath the cursor follows the drag."""
  forward = -orbit_offset(yaw, pitch, 1)
  right = np.cross(forward, (0, 0, 1))
  right /= np.linalg.norm(right)
  up = np.cross(right, forward)
  units = 2 * distance * math.tan(math.radians(fov / 2)) / height
  return (-dx * right + dy * up) * units


BOX_MODES = ("collision", "model", "off")
CYCLE_KEYS = (pygame.K_TAB, pygame.K_PAGEUP, pygame.K_PAGEDOWN)

class FlyCamera:
  def __init__(self, target, yaw, pitch, distance):
    self.position = np.asarray(target, dtype=float) + orbit_offset(yaw, pitch, distance)
    self.yaw, self.pitch = yaw, pitch
    self.speed = 240.0
    self.looking = False

  def release(self):
    self.looking = False
    pygame.event.set_grab(False)
    pygame.mouse.set_visible(True)

  def event(self, event):
    if event.type == pygame.WINDOWFOCUSLOST:
      self.release()
    elif event.type == MOUSEBUTTONDOWN and event.button == 3:
      self.looking = True
      pygame.event.set_grab(True)
      pygame.mouse.set_visible(False)
      pygame.mouse.get_rel()
    elif event.type == MOUSEBUTTONUP and event.button == 3:
      self.release()
    elif event.type == MOUSEMOTION and self.looking:
      self.yaw += event.rel[0] * 0.35
      self.pitch = max(-85, min(85, self.pitch + event.rel[1] * 0.35))
    elif event.type == MOUSEWHEEL:
      self.speed = max(1.0, min(4096.0, self.speed * 1.2 ** event.y))

  def move(self, forward, right, seconds, fast=False):
    direction = -orbit_offset(self.yaw, self.pitch, 1)
    lateral = np.cross(direction, (0, 0, 1))
    lateral /= np.linalg.norm(lateral)
    delta = direction * forward + lateral * right
    magnitude = np.linalg.norm(delta)
    if magnitude:
      self.position += delta / magnitude * self.speed * seconds * (4 if fast else 1)

  def update(self, seconds):
    if pygame.key.get_focused() and not pygame.key.get_mods() & (pygame.KMOD_CTRL | pygame.KMOD_META):
      keys = pygame.key.get_pressed()
      self.move(int(keys[pygame.K_w]) - int(keys[pygame.K_s]),
                int(keys[pygame.K_d]) - int(keys[pygame.K_a]), seconds,
                bool(pygame.key.get_mods() & pygame.KMOD_SHIFT))

  def apply(self):
    gluLookAt(*self.position, *(self.position - orbit_offset(self.yaw, self.pitch, 1)), 0, 0, 1)


def model_bounds(transform, verts):
  points = np.asarray(transform_vertices(transform, verts))
  return points.min(axis=0), points.max(axis=0)


def compile_mesh(verts, texcoords, faces_by_mtl, textures):
  display_list = glGenLists(1)
  glNewList(display_list, GL_COMPILE)
  draw_mesh(verts, texcoords, faces_by_mtl, textures)
  glEndList()
  return display_list


def edit_config(args, obj_paths, search_paths):
  meshes = []
  for obj_path in obj_paths:
    mesh = load_obj(obj_path)
    if not mesh[0]:
      raise SystemExit(f"OBJ has no vertices: {obj_path}")
    meshes.append(mesh)
  models = [verts for verts, _, _ in meshes]
  world = args.config == "world"
  player_parts = []
  player_offset = (0, 0, 0)
  anchor = np.eye(4)
  origins = [(0, 0, 0)] * len(meshes)
  # Several world models are laid out side by side; link and view show the selected one alone.
  grid = world and len(meshes) > 1
  if args.config in ("world", "link"):
    player_dir = resolve_obj_path(f"{args.player}/lower.md3", search_paths).parent
    player_parts, weapon = load_player(player_dir)
    if args.config == "link":
      anchor = weapon
    elif not grid:
      origins = [(0, -48, -ITEM_MINS[2])]
    else:
      origins, spacing, columns, rows = item_layout(models)
      player_offset = (-(columns + 1) / 2 * spacing, 0, 0)
  elif args.config == "view":
    anchor[1, 3] = {"right": -5, "left": 5, "center": 0}[args.hand]
  extent = max(256, max(max(abs(o[0]), abs(o[1])) for o in origins) + 64, abs(player_offset[0]) + 64)

  pygame.init()
  w, h = 1280, 800
  pygame.display.set_mode((w, h), DOUBLEBUF | OPENGL | RESIZABLE)
  caption = None
  font = pygame.font.SysFont("menlo", 16)
  small = pygame.font.SysFont("menlo", 12)
  textures = {}
  display_lists = []
  editor = None
  camera = None
  guide_texture = None
  guide_visible = True
  try:
    if args.config == "view":
      references = [(model, view_reference(model, args.hand)) for model in view_reference_models(search_paths)]
      coverages = {}
      guide_texture = glGenTextures(1)
      glBindTexture(GL_TEXTURE_2D, guide_texture)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
    guide_key = None
    guide_count = 0

    def update_guide():
      # The silhouette depends on aspect via the FOV, so it is rasterized at the window size.
      # Each reference is rasterized once per size; the selected model is left out of the average.
      nonlocal guide_key, guide_count
      key = (w, h, editor.index)
      if guide_texture is None or guide_key == key:
        return guide_count > 0
      guide_key = key
      if coverages.get("size") != (w, h):
        coverages.clear()
        coverages["size"] = (w, h)
      current = obj_paths[editor.index].resolve()
      selected = []
      for model, mesh in references:
        if model != current:
          if model not in coverages:
            coverages[model] = view_coverage(mesh, w, h, args.fov, args.hand)
          selected.append(coverages[model])
      guide_count = len(selected)
      if selected:
        glBindTexture(GL_TEXTURE_2D, guide_texture)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     np.flipud(view_guide(selected)).copy().tobytes())
      return guide_count > 0

    def mesh_textures(materials, model_dir):
      # Materials resolve relative to their own model, so cache by resolved file, not name.
      resolved = {}
      for material in materials:
        path = resolve_quetoo_material(material, search_paths, model_dir)
        if path is None:
          raise ValueError(f"Texture not found: {material} ({model_dir})")
        if path not in textures:
          textures[path] = load_texture(path)
        resolved[material] = textures[path]
      return resolved

    for obj_path, (verts, uv, faces) in zip(obj_paths, meshes):
      display_lists.append(compile_mesh(verts, uv, faces, mesh_textures(faces, obj_path.parent)))
    for verts, uv, faces in player_parts:
      display_lists.append(compile_mesh(verts, uv, faces, mesh_textures(faces, obj_paths[0].parent)))
    player_lists = display_lists[len(meshes):]

    entries = [(path.parent / "model.cfg", verts) for path, verts in zip(obj_paths, models)]
    editor = ConfigPreview(entries, args.config)
    glEnable(GL_DEPTH_TEST)
    glClearColor(0.10, 0.11, 0.13, 1.0)
    yaw, pitch = 135.0, 15.0
    distance = max(160.0, columns * spacing * 1.5) if grid else 160.0
    target = np.array((0.0, 0.0, 32.0))
    camera = FlyCamera(target, yaw, pitch, distance)
    overlays = True
    box_mode = 0
    bounds_cache = {}

    def cached_bounds(index, transform):
      key = (index, transform.tobytes())
      if key not in bounds_cache:
        bounds_cache[key] = model_bounds(transform, models[index])
      return bounds_cache[key]
    pressed = (0, 0)
    clock = pygame.time.Clock()

    def transforms():
      out = []
      for config in editor.configs:
        transform = config.transform if config is not None else np.eye(4)
        out.append(view_transform(transform, args.hand) if args.config == "view" else transform)
      return out

    def item_matrix(index, transform):
      matrix = anchor.copy()
      matrix[:3, 3] += origins[index]
      return matrix @ transform

    def pick(position):
      modelview = glGetDoublev(GL_MODELVIEW_MATRIX)
      projection = glGetDoublev(GL_PROJECTION_MATRIX)
      viewport = glGetIntegerv(GL_VIEWPORT)
      x, y = position[0], viewport[3] - position[1]
      near = np.array(gluUnProject(x, y, 0.0, modelview, projection, viewport))
      far = np.array(gluUnProject(x, y, 1.0, modelview, projection, viewport))
      item_bounds = []
      for i, transform in enumerate(transforms()):
        points = np.vstack([transform_vertices(item_matrix(i, transform), models[i]),
                            np.add(origins[i], ITEM_MINS), np.add(origins[i], ITEM_MAXS)])
        item_bounds.append((points.min(axis=0), points.max(axis=0)))
      return pick_box(item_bounds, near, far - near)

    while editor.running:
      seconds = min(clock.tick(60) / 1000.0, 0.1)
      editor.update()
      if not editor.running:
        break
      title = model_name(obj_paths[editor.index]) if not grid else f"{len(obj_paths)} models"
      if caption != title:
        caption = title
        pygame.display.set_caption(f"objfu - {title} - model.cfg ({args.config})")
      for event in pygame.event.get():
        if event.type == QUIT:
          editor.close()
        elif event.type == KEYDOWN and event.key == pygame.K_ESCAPE:
          editor.deselect()
          camera.release()
        elif event.type == WINDOWSIZECHANGED:
          w, h = max(1, event.x), max(1, event.y)
          glViewport(0, 0, w, h)
        elif args.config == "view" and event.type == KEYDOWN and event.key == pygame.K_g:
          guide_visible = not guide_visible
        elif args.config == "view" and event.type == MOUSEBUTTONDOWN and event.button == 1:
          editor.select(editor.index)
        elif event.type == KEYDOWN and event.key == pygame.K_t:
          overlays = not overlays
        elif world and event.type == KEYDOWN and event.key == pygame.K_b:
          box_mode = (box_mode + 1) % len(BOX_MODES)
        elif len(meshes) > 1 and event.type == KEYDOWN and event.key in CYCLE_KEYS:
          backward = event.key == pygame.K_PAGEUP or (
            event.key == pygame.K_TAB and pygame.key.get_mods() & pygame.KMOD_SHIFT)
          editor.select((editor.index + (-1 if backward else 1)) % len(meshes))
        elif args.config == "view" and len(meshes) > 1 and event.type == MOUSEWHEEL and event.y:
          # Scrolling down moves down the list, as Tab does.
          editor.select((editor.index + (1 if event.y < 0 else -1)) % len(meshes))
        elif args.config != "view":
          camera.event(event)
          if event.type == MOUSEBUTTONDOWN and event.button == 1:
            pressed = event.pos
          elif event.type == MOUSEBUTTONUP and event.button == 1:
            if not camera.looking and math.dist(pressed, event.pos) < 5:
              selected = pick(event.pos) if grid else editor.index
              if selected is not None:
                editor.select(selected)
      if not editor.running:
        break
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
      glMatrixMode(GL_PROJECTION)
      glLoadIdentity()
      if args.config == "view":
        gluPerspective(view_vertical_fov(args.fov, w / h), w / h, 0.1, 4096)
      else:
        gluPerspective(50, w / h, 0.1, 8192)
      glMatrixMode(GL_MODELVIEW)
      glLoadIdentity()
      if args.config == "view":
        gluLookAt(0, 0, 0, 1, 0, 0, 0, 0, 1)
        if guide_visible and update_guide():
          draw_view_guide(guide_texture, w, h)
      else:
        camera.update(seconds)
        camera.apply()
        if args.config != "muzzle":
          draw_floor(math.ceil(extent / 16) * 16)
        glPushMatrix()
        glTranslatef(*player_offset)
        for display_list in player_lists:
          glCallList(display_list)
        glPopMatrix()
      current = transforms()
      for i, transform in enumerate(current):
        if not grid and i != editor.index:
          continue
        matrix = item_matrix(i, transform)
        glPushMatrix()
        glMultMatrixd(np.ascontiguousarray(matrix.T))
        glCallList(display_lists[i])
        glPopMatrix()
        if world:
          glPushMatrix()
          glTranslatef(*origins[i])
          selected = editor.selected and i == editor.index
          highlight = selected
          if BOX_MODES[box_mode] == "collision":
            draw_box(ITEM_MINS, ITEM_MAXS, (1.0, 0.85, 0.2) if highlight else (0.45, 0.45, 0.5))
          elif BOX_MODES[box_mode] == "model":
            draw_box(*cached_bounds(i, transform), (0.2, 0.9, 1.0) if highlight else (0.3, 0.45, 0.5))
          draw_axes(64 if highlight else 40)
          glPopMatrix()
        elif editor.selected and editor.config is not None:
          draw_crosshair(transform_vertices(matrix, [editor.config.muzzle])[0], 1)
      if args.config in ("link", "muzzle"):
        draw_axes(64)
      if overlays and grid:
        modelview = glGetDoublev(GL_MODELVIEW_MATRIX)
        projection = glGetDoublev(GL_PROJECTION_MATRIX)
        viewport = glGetIntegerv(GL_VIEWPORT)
        for i, origin in enumerate(origins):
          x, y, z = gluProject(origin[0], origin[1] + ITEM_MINS[1], 0, modelview, projection, viewport)
          if 0 < z < 1:
            color = (255, 215, 50) if editor.selected and i == editor.index else (170, 170, 170)
            draw_text(small, obj_paths[i].parent.name, int(x) - 40, int(h - y) + 4, w, h, color)
      cycle = ""
      if len(meshes) > 1 and not grid:
        cycle = f"  {'Wheel/' if args.config == 'view' else ''}Tab/PgDn: next  Shift+Tab/PgUp: previous"
      if args.config == "view":
        label = f"Fixed first-person camera  T: text{cycle}"
      elif grid:
        label = "WASD: fly  Right mouse: look  Shift: fast  Scroll: speed  Click/Tab: select  T: text"
      else:
        label = f"WASD: fly  Right mouse: look  Shift: fast  Scroll: speed  T: text{cycle}"
      if args.config == "muzzle":
        label = f"Model-space muzzle  WASD: fly  Right mouse: look  Shift: fast  Scroll: speed{cycle}"
      # Configuration errors stay visible with the overlays hidden.
      if overlays or editor.error:
        draw_text(font, editor.error or label, 12, 12, w, h)
      line = 36
      if overlays and len(meshes) > 1 and not grid:
        draw_text(font, f"{model_name(obj_paths[editor.index])}  ({editor.index + 1}/{len(meshes)})",
                  12, line, w, h)
        line += 24
      if overlays and args.config == "view":
        guide = f"Average of {guide_count} Quetoo weapons" if guide_count else "No Quetoo weapons to average"
        draw_text(font, f"G: guide {'on' if guide_visible else 'off'}  {guide}", 12, line, w, h)
      elif overlays and world and editor.selected:
        bottom = min(v[2] for v in transform_vertices(current[editor.index], models[editor.index]))
        box = {"collision": "game ITEM_BOUNDS", "model": "model bounds", "off": "off"}[BOX_MODES[box_mode]]
        draw_text(font, f"{model_name(obj_paths[editor.index])}  Model bottom: {bottom - ITEM_MINS[2]:.3f}  "
                  f"Floor: 0  Item origin: {-ITEM_MINS[2]}  B: box {box}", 12, 36, w, h)
      pygame.display.flip()
  finally:
    if camera is not None:
      camera.release()
    if editor is not None:
      editor.finish()
    for display_list in display_lists:
      glDeleteLists(display_list, 1)
    glDeleteTextures(list(textures.values()))
    if guide_texture is not None:
      glDeleteTextures([guide_texture])
    pygame.quit()


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("-p", "--path", action="append", default=[], help="Quetoo data root search path")
  parser.add_argument("-c", "--config", choices=("world", "link", "view", "muzzle"),
                      help="Live text editing of the selected mesh configuration")
  parser.add_argument("--player", default="players/enforcer", help="Reference player directory (world/link)")
  parser.add_argument("--hand", choices=("right", "left", "center"), default="right",
                      help="First-person handedness (default: right)")
  parser.add_argument("--fov", type=float, default=110, help="First-person FOV at 16:9 (default: 110)")
  parser.add_argument("obj", nargs="+",
                      help="OBJ path, model directory or quoted glob of model directories, e.g. '*quake2*' "
                           "(several models require --config; world shows them together, "
                           "link and view cycle through them)")
  args = parser.parse_args()
  if not math.isfinite(args.fov) or not 1 < args.fov < 179:
    parser.error("--fov must be between 1 and 179 degrees")

  search_paths = [Path(p).expanduser().resolve() for p in args.path]
  if not search_paths:
    search_paths.append((Path(__file__).resolve().parents[3] / "quetoo-data" / "target" / "default").resolve())

  obj_paths = resolve_obj_args(args.obj, search_paths)
  if len(obj_paths) > 1 and not args.config:
    parser.error(f"{len(obj_paths)} models matched; several models require --config")
  obj_path = obj_paths[0]

  if args.config:
    edit_config(args, obj_paths, search_paths)
    return

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
    print(f"Loaded muzzle from model.cfg: {muzzle}")

  pygame.init()
  pygame.key.set_repeat(400, 30)
  w, h = 1280, 800
  pygame.display.set_mode((w, h), DOUBLEBUF | OPENGL | RESIZABLE)
  pygame.display.set_caption(f"obj-fu · {obj_path.name}")
  font_big = pygame.font.SysFont("menlo", 20, bold=True)
  font_small = pygame.font.SysFont("menlo", 16)

  glEnable(GL_DEPTH_TEST)
  glEnable(GL_TEXTURE_2D)
  glClearColor(0.10, 0.11, 0.13, 1.0)

  def set_projection():
    glViewport(0, 0, w, h)
    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(50.0, w / h, max(0.01, extent * 0.01), extent * 100.0)
    glMatrixMode(GL_MODELVIEW)

  set_projection()

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
  camera = FlyCamera((cx, cy, cz), yaw, pitch, dist)
  selected = True
  status_msg = ""
  clock = pygame.time.Clock()

  running = True
  while running:
    seconds = min(clock.tick(60) / 1000.0, 0.1)
    for event in pygame.event.get():
      camera.event(event)
      if event.type == QUIT:
        running = False
      elif event.type == WINDOWSIZECHANGED:
        w, h = max(1, event.x), max(1, event.y)
        set_projection()
      elif event.type == KEYDOWN:
        step = 5.0 if (pygame.key.get_mods() & pygame.KMOD_SHIFT) else 1.0
        mx, my, mz = muzzle
        if event.key == pygame.K_ESCAPE:
          selected = False
          camera.release()
        elif not selected:
          continue
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
        elif event.key == pygame.K_s and pygame.key.get_mods() & (pygame.KMOD_CTRL | pygame.KMOD_META):
          written = save_muzzle(obj_path.parent, muzzle)
          status_msg = f'Saved muzzle "{muzzle[0]:.0f} {muzzle[1]:.0f} {muzzle[2]:.0f}"'
          print(status_msg)
          print("  -> " + ", ".join(str(p) for p in written))
      elif event.type == MOUSEBUTTONDOWN and event.button == 1:
        selected = True

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
    glLoadIdentity()

    camera.update(seconds)
    camera.apply()

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
    if selected:
      draw_crosshair(muzzle, size=extent * 0.035)

    draw_text(font_big, f'muzzle  x:{muzzle[0]:+.0f}  y:{muzzle[1]:+.0f}  z:{muzzle[2]:+.0f}  (Quake/Quetoo space)', 12, 10, w, h)
    help_lines = [
      "← → : X      ↑ ↓ : Y      Q/E : Z      Shift : x5",
      "WASD: fly  Right mouse: look  Shift: fast  Scroll: speed  Ctrl+S: save  R: reset",
    ]
    for i, txt in enumerate(help_lines):
      draw_text(font_small, txt, 12, h - 14 - (i + 1) * 22, w, h, color=(170, 170, 170))
    if status_msg:
      draw_text(font_small, status_msg, 12, 42, w, h, color=(100, 255, 130))

    pygame.display.flip()
  camera.release()

  pygame.quit()


if __name__ == "__main__":
  main()
