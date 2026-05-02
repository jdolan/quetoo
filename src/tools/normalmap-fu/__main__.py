#!/usr/bin/env python3
"""
Normalmap-fu: comprehensive GUI tool for authoring Quetoo per-pixel material
assets (diffuse, normalmap+heightmap, specularmap).

Run with:
    python normalmap-fu/                              # current dir, no args
    python normalmap-fu/ /path/to/normalmap.png
    python normalmap-fu/ --directory /path/to/textures/

File conventions
----------------
    <base>.jpg                    diffuse map (sRGB)
    <base>_norm.png               normalmap RGB + heightmap in alpha
    <base>_spec.jpg               specularmap (greyscale stored as RGB)

The diffuse map is read-only in this tool. The normalmap+heightmap is saved
as a single RGBA PNG. The specularmap is saved as JPG.
"""

import concurrent.futures
import json
import os
import re
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageTk
import tkinter as tk
from tkinter import ttk, filedialog, messagebox


# ---------------------------------------------------------------------------
# File-naming conventions
# ---------------------------------------------------------------------------

NORMAL_SUFFIX = "_norm"
SPEC_SUFFIX = "_spec"
DIFFUSE_EXTS = (".jpg", ".jpeg", ".png", ".tga")
NORMAL_EXT = ".png"
SPEC_EXT = ".jpg"

THUMB_SIZE = 96
TILE_SIZE = 256

PRESET_FILENAME = ".normalmap-fu.json"

# Quetoo's renderer uses Quake's V-down texture coordinates, which makes the
# per-vertex bitangent point in the image-down direction. As a result the
# engine expects normalmaps where G encodes "+Y = image-down" -- i.e. the
# DirectX convention. All processed normalmaps are saved in this convention.
# (The math pipeline internally normalizes everything to OpenGL/Y-up because
# that's what Frankot-Chellappa integration assumes.)
ENGINE_NORMAL_CONVENTION = "DirectX"


def _is_normal_map(path: Path) -> bool:
  return path.stem.lower().endswith(NORMAL_SUFFIX)


def _is_spec_map(path: Path) -> bool:
  return path.stem.lower().endswith(SPEC_SUFFIX)


def _strip_suffix(stem: str, suffix: str) -> str | None:
  if stem.lower().endswith(suffix):
    return stem[: len(stem) - len(suffix)]
  return None


def _base_of(path: Path) -> str:
  """Strip any known suffix (_norm, _spec) from the filename stem."""

  stem = path.stem
  for suf in (NORMAL_SUFFIX, SPEC_SUFFIX):
    base = _strip_suffix(stem, suf)
    if base is not None:
      return base
  return stem


def _find_diffuse(parent: Path, base: str) -> Path | None:
  for ext in DIFFUSE_EXTS:
    candidate = parent / (base + ext)
    if candidate.is_file() and not _is_normal_map(candidate) \
            and not _is_spec_map(candidate):
      return candidate
  return None


def _find_normal(parent: Path, base: str) -> Path | None:
  candidate = parent / (base + NORMAL_SUFFIX + NORMAL_EXT)
  return candidate if candidate.is_file() else None


def _find_spec(parent: Path, base: str) -> Path | None:
  candidate = parent / (base + SPEC_SUFFIX + SPEC_EXT)
  return candidate if candidate.is_file() else None


# ---------------------------------------------------------------------------
# .mat file resolution
# ---------------------------------------------------------------------------

# Quetoo materials are described by `<base>.mat` files that may redirect the
# normalmap/specularmap to a different path entirely (often shared across a
# variant family). Format is a brace-delimited block of `key value` lines:
#
#   {
#     diffusemap quake/adoor03_3
#     normalmap quake/adoor03_norm
#     specularmap quake/adoor03_spec
#   }
#
# Values are paths relative to the textures root (the parent directory named
# `textures`), without an extension.

_MAT_KEYS = ("diffusemap", "normalmap", "specularmap", "heightmap", "tintmap")
_MAT_RESOLVE_EXTS = {
  "diffusemap":  DIFFUSE_EXTS,
  "normalmap":   (NORMAL_EXT, ".tga", ".jpg"),
  "specularmap": (SPEC_EXT, ".png", ".tga"),
  "heightmap":   (".png", ".tga", ".jpg"),
  "tintmap":     (".png", ".tga", ".jpg"),
}


def _textures_root(start: Path) -> Path | None:
  """Walk up from `start` looking for an ancestor directory named `textures`."""
  for d in [start, *start.parents]:
    if d.name == "textures":
      return d
  return None


def _parse_mat(mat_path: Path) -> dict[str, str]:
  """Return {key: rel_path} for the keys we know about. Tolerant of comments
  and whitespace; ignores nested stages."""
  out: dict[str, str] = {}
  try:
    text = mat_path.read_text(errors="replace")
  except Exception:
    return out
  # Strip // line and /* */ block comments
  text = re.sub(r"/\*.*?\*/", " ", text, flags=re.DOTALL)
  text = re.sub(r"//[^\n]*", " ", text)
  # Tokenize on whitespace and braces
  tokens = re.findall(r"\{|\}|[^\s{}]+", text)
  depth = 0
  i = 0
  while i < len(tokens):
    tok = tokens[i]
    if tok == "{":
      depth += 1
    elif tok == "}":
      depth -= 1
    elif depth == 1 and tok in _MAT_KEYS and i + 1 < len(tokens):
      val = tokens[i + 1]
      if val not in ("{", "}"):
        out.setdefault(tok, val)
        i += 1
    i += 1
  return out


def _resolve_mat_value(textures_root: Path, rel: str, key: str) -> Path | None:
  """Resolve a .mat path value (e.g. 'quake/adoor03_norm') to an actual file.
  Mirrors Cm_ResolveMaterialAsset: try the engine's canonical suffix (_norm
  for normalmap, _spec for specularmap) first, then the literal name."""
  exts = _MAT_RESOLVE_EXTS.get(key, (".png", ".tga", ".jpg"))
  candidates: list[str] = []
  if key == "normalmap" and not rel.endswith(NORMAL_SUFFIX):
    candidates.append(rel + NORMAL_SUFFIX)
  if key == "specularmap" and not rel.endswith(SPEC_SUFFIX):
    candidates.append(rel + SPEC_SUFFIX)
  candidates.append(rel)
  for cand in candidates:
    for ext in exts:
      p = textures_root / (cand + ext)
      if p.is_file():
        return p
  return None


def _resolve_mat_assets(parent: Path, base: str) -> dict[str, Path]:
  """If `<parent>/<base>.mat` exists, parse it and resolve normalmap/
  specularmap (and diffusemap, for completeness) to concrete paths.
  Returns an empty dict if there's no .mat or no textures root."""
  mat_path = parent / (base + ".mat")
  if not mat_path.is_file():
    return {}
  root = _textures_root(parent)
  if root is None:
    return {}
  parsed = _parse_mat(mat_path)
  resolved: dict[str, Path] = {}
  for key, rel in parsed.items():
    p = _resolve_mat_value(root, rel, key)
    if p is not None:
      resolved[key] = p
  return resolved


# ---------------------------------------------------------------------------
# Image processing
# ---------------------------------------------------------------------------

def _to_luminance(rgb: np.ndarray) -> np.ndarray:
  """Convert uint8 HxWx3 RGB to float32 HxW luminance in [0, 1]."""

  return (np.dot(rgb[:, :, :3].astype(np.float32),
                 [0.2126, 0.7152, 0.0722]) / 255.0)


def _normalize(arr: np.ndarray) -> np.ndarray:
  """Linearly rescale a float array into [0, 1]."""

  lo, hi = float(arr.min()), float(arr.max())
  if hi - lo < 1e-6:
    return np.full_like(arr, 0.5, dtype=np.float32)
  return ((arr - lo) / (hi - lo)).astype(np.float32)


def split_bands(img: np.ndarray, cutoff: float,
                low_gain: float, high_gain: float) -> np.ndarray:
  """Frequency-band split with adjustable gains.

  Splits img into a low-frequency band (below cutoff) and a high-frequency
  band (above cutoff), scales each by its gain, and recombines.

  Parameters
  ----------
  img : float32 HxW
  cutoff : 0..1, normalized frequency where 1.0 is Nyquist.
           Smaller values keep less low-band detail in the "low" portion.
  low_gain : multiplier on the low-frequency band
  high_gain : multiplier on the high-frequency band

  Returns
  -------
  float32 HxW reconstructed image (not clipped, not renormalized).
  """

  if abs(low_gain - 1.0) < 1e-3 and abs(high_gain - 1.0) < 1e-3:
    return img.astype(np.float32)

  h, w = img.shape
  F = np.fft.fft2(img.astype(np.float32))

  u = np.fft.fftfreq(w) * 2.0
  v = np.fft.fftfreq(h) * 2.0
  uu, vv = np.meshgrid(u, v)
  radius = np.sqrt(uu * uu + vv * vv)

  sigma = max(cutoff, 0.01)
  low_mask = np.exp(-(radius * radius) / (2.0 * sigma * sigma))
  high_mask = 1.0 - low_mask

  F_out = F * (low_gain * low_mask + high_gain * high_mask)
  return np.real(np.fft.ifft2(F_out)).astype(np.float32)


def frankot_chellappa(p: np.ndarray, q: np.ndarray) -> np.ndarray:
  """Integrate gradients (p, q) into a height field via Frankot-Chellappa."""

  h, w = p.shape
  P = np.fft.fft2(p)
  Q = np.fft.fft2(q)

  u = np.fft.fftfreq(w) * 2.0 * np.pi
  v = np.fft.fftfreq(h) * 2.0 * np.pi
  uu, vv = np.meshgrid(u, v)

  denom = uu * uu + vv * vv
  denom[0, 0] = 1.0

  H = (1j * uu * P + 1j * vv * Q) / denom
  H[0, 0] = 0.0

  return np.real(np.fft.ifft2(H)).astype(np.float32)


def attenuate_normals(normals_u8: np.ndarray, strength: float) -> np.ndarray:
  """Scale the XY of a tangent-space normalmap and renormalize Z."""

  if abs(strength - 1.0) < 1e-3:
    return normals_u8.copy()

  n = normals_u8.astype(np.float64) / 127.5 - 1.0
  nx = n[:, :, 0] * strength
  ny = n[:, :, 1] * strength

  xy_sq = nx * nx + ny * ny
  too_large = xy_sq > 0.999
  scale = np.where(too_large, np.sqrt(0.999 / np.maximum(xy_sq, 1e-10)), 1.0)
  nx *= scale
  ny *= scale

  nz = np.sqrt(np.maximum(1.0 - nx * nx - ny * ny, 0.0))
  result = np.stack([nx, ny, nz], axis=2)
  return np.clip((result + 1.0) * 127.5, 0, 255).astype(np.uint8)


def heightmap_from_normals(normals_u8: np.ndarray,
                           cutoff: float, low_gain: float, high_gain: float,
                           strength: float, y_flip: bool, invert: bool
                           ) -> np.ndarray:
  """Generate a heightmap from an RGB normalmap via Frankot-Chellappa,
     then apply frequency-band reweighting + contrast strength."""

  n = normals_u8.astype(np.float32) / 127.5 - 1.0
  nx, ny, nz = n[:, :, 0], n[:, :, 1], np.clip(n[:, :, 2], 0.01, None)

  p = -nx / nz
  q = -ny / nz
  if y_flip:
    q = -q

  height = frankot_chellappa(p, q)
  height = _normalize(height)

  height = split_bands(height, cutoff, low_gain, high_gain)
  height = np.clip(height, 0.0, 1.0)

  height = 0.5 + (height - 0.5) * strength
  height = np.clip(height, 0.0, 1.0)

  if invert:
    height = 1.0 - height

  return (height * 255.0).astype(np.uint8)


def normals_from_diffuse(diffuse_u8: np.ndarray,
                         strength: float,
                         cutoff: float, low_gain: float, high_gain: float
                         ) -> tuple[np.ndarray, np.ndarray]:
  """Generate a normalmap (and matching heightmap) from a diffuse texture.

  Builds a luminance height field, frequency-band reweights it, then derives
  tangent-space normals from Sobel gradients. Returns (normals_u8_HxWx3,
  height_u8_HxW).
  """

  height = _to_luminance(diffuse_u8)
  height = split_bands(height, cutoff, low_gain, high_gain)
  height_norm = _normalize(height)
  height_u8 = (height_norm * 255.0).astype(np.uint8)

  pad = 2
  h_padded = np.pad(height_norm, pad, mode='wrap')
  dx = cv2.Sobel(h_padded, cv2.CV_32F, 1, 0, ksize=3) * strength
  dy = cv2.Sobel(h_padded, cv2.CV_32F, 0, 1, ksize=3) * strength
  dx = dx[pad:-pad, pad:-pad]
  dy = dy[pad:-pad, pad:-pad]

  nz = np.ones_like(dx)
  length = np.sqrt(dx * dx + dy * dy + nz * nz)
  nx = -dx / length
  ny = -dy / length
  nz = nz / length

  normals = np.stack([nx, ny, nz], axis=2)
  normals_u8 = np.clip((normals + 1.0) * 127.5, 0, 255).astype(np.uint8)
  return normals_u8, height_u8


def normals_from_height(height_u8: np.ndarray, strength: float) -> np.ndarray:
  """Derive a tangent-space normalmap from a heightmap via Sobel gradients.

  Larger 'strength' steepens the slope (taller bumps).  Output is unit-length.
  """

  h = height_u8.astype(np.float32) / 255.0
  pad = 2
  h_padded = np.pad(h, pad, mode='wrap')
  dx = cv2.Sobel(h_padded, cv2.CV_32F, 1, 0, ksize=3) * strength
  dy = cv2.Sobel(h_padded, cv2.CV_32F, 0, 1, ksize=3) * strength
  dx = dx[pad:-pad, pad:-pad]
  dy = dy[pad:-pad, pad:-pad]
  nz = np.ones_like(dx)
  length = np.sqrt(dx * dx + dy * dy + nz * nz)
  nx = -dx / length
  ny = -dy / length
  nz = nz / length
  normals = np.stack([nx, ny, nz], axis=2)
  return np.clip((normals + 1.0) * 127.5, 0, 255).astype(np.uint8)


def fix_tiling_borders_rgba(rgba: np.ndarray, strength: float = 1.0) -> np.ndarray:
  """Fix the outermost 1px border of an RGBA normalmap for seamless tiling.

  Recomputes only the border pixels from the alpha-channel heightmap using
  wrap-aware Sobel, leaving the entire interior completely unchanged.
  Y-axis convention is auto-detected by correlating interior normals.
  """

  alpha = rgba[:, :, 3]
  if float(np.std(alpha)) < 1.0:
    return rgba  # flat heightmap, nothing to do

  h = alpha.astype(np.float32) / 255.0
  H, W = h.shape

  # Compute interior normals (non-wrap; identical to wrap for non-edge pixels)
  # to detect Y convention by correlating against the original green channel.
  b = 8
  flip_y = False
  if H > b * 2 + 4 and W > b * 2 + 4:
    dx0 = cv2.Sobel(h, cv2.CV_32F, 1, 0, ksize=3) * strength
    dy0 = cv2.Sobel(h, cv2.CV_32F, 0, 1, ksize=3) * strength
    dx_s = dx0[b:-b, b:-b].ravel()
    dy_s = dy0[b:-b, b:-b].ravel()
    orig_g = rgba[b:-b, b:-b, 1].astype(np.float32).ravel()
    mag = np.maximum(np.sqrt(dx_s ** 2 + dy_s ** 2 + 1), 1e-8)
    corr_normal = float(np.corrcoef(orig_g, (-dy_s / mag * 0.5 + 0.5) * 255)[0, 1])
    corr_flip   = float(np.corrcoef(orig_g, ( dy_s / mag * 0.5 + 0.5) * 255)[0, 1])
    flip_y = corr_flip > corr_normal

  # Recompute with wrap padding
  pad = 1
  h_padded = np.pad(h, pad, mode='wrap')
  dx = cv2.Sobel(h_padded, cv2.CV_32F, 1, 0, ksize=3) * strength
  dy = cv2.Sobel(h_padded, cv2.CV_32F, 0, 1, ksize=3) * strength
  dx = dx[pad:-pad, pad:-pad]
  dy = dy[pad:-pad, pad:-pad]

  nx = -dx
  ny = dy if flip_y else -dy
  nz = np.ones_like(nx)
  length = np.maximum(np.sqrt(nx * nx + ny * ny + nz * nz), 1e-8)
  nx /= length; ny /= length; nz /= length

  r = np.clip((nx * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
  g = np.clip((ny * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
  b_ch = np.clip((nz * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
  new_border = np.stack([r, g, b_ch], axis=-1)

  result = rgba.copy()
  result[0,    :,   :3] = new_border[0,    :]
  result[-1,   :,   :3] = new_border[-1,   :]
  result[1:-1, 0,   :3] = new_border[1:-1, 0]
  result[1:-1, -1,  :3] = new_border[1:-1, -1]
  return result


def specular_from_diffuse(diffuse_u8: np.ndarray, height_u8: np.ndarray | None,
                          input_black: float, input_white: float,
                          output_black: float, output_white: float,
                          cavity_strength: float) -> np.ndarray:
  """Generate a specularmap from diffuse luminance, darkened by heightmap
     cavity. Levels are applied Photoshop-style (input range remapped to
     output range)."""

  lum = _to_luminance(diffuse_u8)

  if height_u8 is not None and cavity_strength > 1e-3:
    h, w = lum.shape
    if height_u8.shape != lum.shape:
      height_resized = cv2.resize(height_u8, (w, h), interpolation=cv2.INTER_LINEAR)
    else:
      height_resized = height_u8
    cavity = height_resized.astype(np.float32) / 255.0
    lum = lum * (1.0 - cavity_strength * (1.0 - cavity))

  ib = max(0.0, min(input_black, input_white - 1e-3))
  iw = max(input_white, ib + 1e-3)
  spec = np.clip((lum - ib) / (iw - ib), 0.0, 1.0)
  spec = output_black + spec * (output_white - output_black)
  return np.clip(spec * 255.0, 0, 255).astype(np.uint8)


# ---------------------------------------------------------------------------
# Pipeline (pure functions, shared by GUI and CLI)
# ---------------------------------------------------------------------------

def to_engine_normal(opengl_u8: np.ndarray) -> np.ndarray:
  """Convert an internal (OpenGL) normalmap to the engine's expected convention."""
  if ENGINE_NORMAL_CONVENTION == "OpenGL":
    return opengl_u8
  out = opengl_u8.copy()
  out[:, :, 1] = 255 - out[:, :, 1]
  return out


def to_opengl_normal(raw_u8: np.ndarray, *, source_is_directx: bool) -> np.ndarray:
  """Return the normalmap in OpenGL convention (Y up), flipping G if needed."""
  if not source_is_directx:
    return raw_u8
  out = raw_u8.copy()
  out[:, :, 1] = 255 - out[:, :, 1]
  return out


def process_normal(orig_normal_u8: np.ndarray, params: dict) -> np.ndarray:
  """Apply strength + denoise + slope-gate + freq reweight to a normalmap.

  The input is assumed to already be in OpenGL convention (Y up); callers
  are responsible for converting via `to_opengl_normal` first if needed.
  """
  n = attenuate_normals(orig_normal_u8, float(params.get("normal_strength", 1.0)))

  v = n.astype(np.float32) / 127.5 - 1.0
  x = v[:, :, 0].copy()
  y = v[:, :, 1].copy()

  denoise = float(params.get("n_denoise", 0.0))
  if denoise > 0.05:
    sigma_color = denoise / 100.0
    sigma_space = max(2.0, denoise * 0.4)
    x = cv2.bilateralFilter(x, d=0, sigmaColor=sigma_color, sigmaSpace=sigma_space)
    y = cv2.bilateralFilter(y, d=0, sigmaColor=sigma_color, sigmaSpace=sigma_space)

  gate = float(params.get("n_slope_gate", 0.0))
  if gate > 1e-4:
    mag = np.sqrt(x * x + y * y)
    knee = max(gate * 0.5, 1e-4)
    attenuation = np.clip((mag - gate + knee) / (knee * 2.0), 0.0, 1.0)
    x *= attenuation
    y *= attenuation

  cutoff = float(params.get("n_freq_cutoff", 0.5))
  low_g = float(params.get("n_freq_low_gain", 1.0))
  high_g = float(params.get("n_freq_high_gain", 1.0))
  if abs(low_g - 1.0) > 1e-3 or abs(high_g - 1.0) > 1e-3:
    x = split_bands(x, cutoff, low_g, high_g)
    y = split_bands(y, cutoff, low_g, high_g)

  mag2 = x * x + y * y
  scale = np.where(mag2 > 0.999, np.sqrt(0.999 / np.maximum(mag2, 1e-10)),
                   1.0).astype(np.float32)
  x *= scale
  y *= scale
  z = np.sqrt(np.maximum(1.0 - x * x - y * y, 0.0))
  v = np.stack([x, y, z], axis=2)
  n = np.clip((v + 1.0) * 127.5, 0, 255).astype(np.uint8)

  return n


def process_height(processed_normal_u8: np.ndarray, params: dict,
                   *, as_heightmap: bool) -> np.ndarray:
  """Derive heightmap (white=high) or depthmap (white=low) from a normalmap."""
  return heightmap_from_normals(
    processed_normal_u8,
    cutoff=float(params.get("h_freq_cutoff", 0.3)),
    low_gain=float(params.get("h_freq_low_gain", 1.0)),
    high_gain=float(params.get("h_freq_high_gain", 1.0)),
    strength=float(params.get("h_strength", 1.0)),
    y_flip=False,
    invert=as_heightmap,
  )


def apply_height_filters(height_u8: np.ndarray, params: dict,
                         *, as_heightmap: bool) -> np.ndarray:
  """Apply the same frequency / strength / invert pipeline used by
  heightmap_from_normals, but to an existing 8-bit heightmap. This lets
  the height sliders edit a loaded heightmap in place without re-deriving
  it from the normalmap on every change."""
  cutoff = float(params.get("h_freq_cutoff", 0.3))
  low_gain = float(params.get("h_freq_low_gain", 1.0))
  high_gain = float(params.get("h_freq_high_gain", 1.0))
  strength = float(params.get("h_strength", 1.0))

  height = height_u8.astype(np.float32) / 255.0
  height = split_bands(height, cutoff, low_gain, high_gain)
  height = np.clip(height, 0.0, 1.0)
  height = 0.5 + (height - 0.5) * strength
  height = np.clip(height, 0.0, 1.0)
  if as_heightmap:
    # 'Heightmap' mode shows white=high; if the loaded data is stored
    # as a depthmap (white=low) the user toggles this to flip back.
    height = 1.0 - height
  return (height * 255.0).astype(np.uint8)


def process_spec(diffuse_u8: np.ndarray, height_u8: np.ndarray | None,
                 params: dict) -> np.ndarray:
  return specular_from_diffuse(
    diffuse_u8, height_u8,
    input_black=float(params.get("s_input_black", 0.0)),
    input_white=float(params.get("s_input_white", 1.0)),
    output_black=float(params.get("s_output_black", 0.0)),
    output_white=float(params.get("s_output_white", 1.0)),
    cavity_strength=float(params.get("s_cavity_strength", 0.5)),
  )


# ---------------------------------------------------------------------------
# Asset model
# ---------------------------------------------------------------------------

class Asset:
  """An on-disk asset and its in-memory modified version, with toggle state."""

  def __init__(self, path: Path | None = None):
    self.path: Path | None = path
    self.original: np.ndarray | None = None
    self.modified: np.ndarray | None = None
    self.show_modified: tk.BooleanVar | None = None  # bound to UI toggle

  def display(self) -> np.ndarray | None:
    if self.show_modified is not None and self.show_modified.get() \
            and self.modified is not None:
      return self.modified
    return self.original


# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------

PARAM_GROUPS = {
  "normal": [
    ("normal_strength",   "Strength",         0.1,  3.0,    1.0,  False),
    ("n_freq_cutoff",     "Freq cutoff",      0.01, 2.0,    0.5,  False),
    ("n_freq_low_gain",   "Low-freq gain",    0.0,  3.0,    1.0,  False),
    ("n_freq_high_gain",  "High-freq gain",   0.0,  3.0,    1.0,  False),
    ("n_denoise",         "Denoise (edge-preserving)", 0.0, 50.0, 0.0, False),
    ("n_slope_gate",      "Slope gate",       0.0,  0.3,    0.0,  False),
  ],
  "height": [
    ("h_strength",        "Strength",         0.1,  4.0,    1.0,  False),
    ("h_freq_cutoff",     "Freq cutoff",      0.01, 2.0,    0.3,  False),
    ("h_freq_low_gain",   "Low-freq gain",    0.0,  3.0,    1.0,  False),
    ("h_freq_high_gain",  "High-freq gain",   0.0,  3.0,    1.0,  False),
  ],
  "spec": [
    ("s_input_black",     "Input black",      0.0,  1.0,    0.0,  False),
    ("s_input_white",     "Input white",      0.0,  1.0,    1.0,  False),
    ("s_output_black",    "Output black",     0.0,  1.0,    0.0,  False),
    ("s_output_white",    "Output white",     0.0,  1.0,    1.0,  False),
    ("s_cavity_strength", "Cavity strength",  0.0,  1.0,    0.5,  False),
  ],
}


class NormalmapFuApp:
  def __init__(self, root: tk.Tk):
    self.root = root
    self.root.title("Normalmap-fu")
    self.root.configure(bg="#2b2b2b")
    self.root.geometry("1700x1000")
    self.root.minsize(1100, 750)

    self.diffuse = Asset()
    self.normal = Asset()
    self.height = Asset()
    self.spec = Asset()

    self.base_path: Path | None = None  # parent / base name of current set
    self.base_name: str | None = None

    self.slider_vars: dict[str, tk.Variable] = {}

    self.normal_convention_var = tk.StringVar(master=self.root, value="DirectX")
    self.height_convention_var = tk.StringVar(master=self.root, value="Heightmap")

    self._build_ui()
    self._poll_directory()

  # --------------------------------------------------------------------
  # UI construction
  # --------------------------------------------------------------------

  def _build_ui(self):
    paned = tk.PanedWindow(self.root, orient=tk.VERTICAL, bg="#2b2b2b",
                           sashwidth=6, sashrelief=tk.RAISED)
    paned.pack(fill=tk.BOTH, expand=True)

    top_pane = tk.Frame(paned, bg="#2b2b2b")
    paned.add(top_pane, stretch="always", minsize=600)

    self._build_header(top_pane)
    self._build_action_bar(top_pane)
    self._build_controls(top_pane)
    self._build_tiles(top_pane)

    browser_pane = tk.Frame(paned, bg="#2b2b2b")
    paned.add(browser_pane, stretch="always", minsize=120)
    self._build_browser(browser_pane)

  def _build_header(self, parent):
    hdr = tk.Frame(parent, bg="#2b2b2b")
    hdr.pack(side=tk.TOP, fill=tk.X, padx=8, pady=(8, 0))

    self.file_var = tk.StringVar(master=self.root,
                                 value="Select a texture to begin")
    tk.Label(hdr, textvariable=self.file_var, anchor="w", bg="#2b2b2b",
             fg="#cccccc", font=("Helvetica", 11, "bold"))\
      .pack(side=tk.LEFT)

    self.stats_var = tk.StringVar(master=self.root)
    tk.Label(hdr, textvariable=self.stats_var, anchor="e", bg="#2b2b2b",
             fg="#888888", font=("Helvetica", 10))\
      .pack(side=tk.RIGHT)

  def _build_action_bar(self, parent):
    bar = tk.Frame(parent, bg="#2b2b2b")
    bar.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=(4, 4))

    tk.Button(bar, text="Set directory...", command=self._browse_directory)\
      .pack(side=tk.LEFT, padx=4)

    tk.Button(bar, text="Load preset...", command=self._load_preset_dialog)\
      .pack(side=tk.LEFT, padx=4)
    tk.Button(bar, text="Save preset...", command=self._save_preset_dialog)\
      .pack(side=tk.LEFT, padx=4)

    self.dir_var = tk.StringVar(master=self.root, value="(no directory)")
    tk.Label(bar, textvariable=self.dir_var, anchor="w", bg="#2b2b2b",
             fg="#999999", font=("Helvetica", 10))\
      .pack(side=tk.LEFT, padx=4)

    tk.Label(bar, text="Filter:", bg="#2b2b2b", fg="#cccccc",
             font=("Helvetica", 10)).pack(side=tk.LEFT, padx=(12, 2))
    self.filter_var = tk.StringVar(master=self.root, value="")
    tk.Entry(bar, textvariable=self.filter_var, width=20,
             font=("Helvetica", 10)).pack(side=tk.LEFT, padx=2)
    self._filter_after_id: str | None = None
    self.filter_var.trace_add("write", lambda *_: self._schedule_filter())

    self.browser_count_var = tk.StringVar(master=self.root)
    tk.Label(bar, textvariable=self.browser_count_var, anchor="e",
             bg="#2b2b2b", fg="#888888", font=("Helvetica", 10))\
      .pack(side=tk.RIGHT, padx=4)

  def _build_controls(self, parent):
    """Build the tabbed parameter notebook above the tiles."""

    notebook = ttk.Notebook(parent)
    notebook.pack(side=tk.BOTTOM, fill=tk.X, padx=8, pady=4)

    # --- Normal tab ---
    n_tab = tk.Frame(notebook, bg="#2b2b2b")
    notebook.add(n_tab, text="Normalmap")
    self._build_param_grid(n_tab, "normal")
    n_btns = tk.Frame(n_tab, bg="#2b2b2b")
    n_btns.grid(row=99, column=0, columnspan=3, padx=8, pady=6, sticky="w")
    ttk.Button(n_btns, text="Generate from diffuse",
               command=self._generate_normal_from_diffuse)\
      .pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(n_btns, text="Generate from heightmap",
               command=self._generate_normal_from_height)\
      .pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(n_btns, text="Sanitize",
               command=self._sanitize_normal)\
      .pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(n_btns, text="Save", width=10,
               command=self._save_normal).pack(side=tk.LEFT, padx=(12, 0))

    # --- Heightmap tab ---
    h_tab = tk.Frame(notebook, bg="#2b2b2b")
    notebook.add(h_tab, text="Heightmap")
    self._build_param_grid(h_tab, "height")
    h_btns = tk.Frame(h_tab, bg="#2b2b2b")
    h_btns.grid(row=99, column=0, columnspan=3, padx=8, pady=6, sticky="w")
    ttk.Button(h_btns, text="Generate from normalmap",
               command=self._generate_height_from_normal)\
      .pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(h_btns, text="Save", width=10,
               command=self._save_height).pack(side=tk.LEFT, padx=(12, 0))

    # --- Specular tab ---
    s_tab = tk.Frame(notebook, bg="#2b2b2b")
    notebook.add(s_tab, text="Specularmap")
    self._build_param_grid(s_tab, "spec")
    s_btns = tk.Frame(s_tab, bg="#2b2b2b")
    s_btns.grid(row=99, column=0, columnspan=3, padx=8, pady=6, sticky="w")
    ttk.Button(s_btns, text="Generate from diffuse + height",
               command=self._reprocess_spec)\
      .pack(side=tk.LEFT, padx=(0, 6))
    ttk.Button(s_btns, text="Save", width=10,
               command=self._save_spec).pack(side=tk.LEFT, padx=(12, 0))

  def _build_param_grid(self, parent: tk.Frame, group: str):
    for row, (key, label, lo, hi, default, is_int) in \
            enumerate(PARAM_GROUPS[group]):
      var = tk.IntVar(master=self.root, value=default) if is_int \
        else tk.DoubleVar(master=self.root, value=default)
      self.slider_vars[key] = var

      tk.Label(parent, text=label, width=16, anchor="w", bg="#2b2b2b",
               fg="#cccccc").grid(row=row, column=0, padx=(8, 4), pady=2,
                                  sticky="w")

      val_label = tk.Label(parent, width=6, anchor="e", bg="#2b2b2b",
                           fg="#999999")
      val_label.grid(row=row, column=2, padx=(4, 8))

      def _update(v, vl=val_label, vr=var, is_i=is_int, g=group):
        vl.config(text=str(int(vr.get())) if is_i else f"{vr.get():.2f}")
        if g == "normal":
          self._reprocess_normal()
        elif g == "height":
          self._reprocess_height()
        elif g == "spec":
          self._reprocess_spec()

      ttk.Scale(parent, from_=lo, to=hi, variable=var, orient=tk.HORIZONTAL,
                command=_update, length=420)\
        .grid(row=row, column=1, padx=4, pady=2, sticky="ew")

      val_label.config(text=str(int(default)) if is_int else f"{default:.2f}")

    parent.columnconfigure(1, weight=1)

  def _build_tiles(self, parent):
    """Build the four 256×256 preview tiles with original/modified toggles."""

    frame = tk.Frame(parent, bg="#1e1e1e")
    frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=8, pady=8)

    self._tile_widgets: dict[str, tk.Label] = {}
    self._tile_photos: dict[str, ImageTk.PhotoImage] = {}

    specs = [
      ("diffuse", "DIFFUSE",  "#cccc50", self.diffuse, False),
      ("normal",  "NORMAL",   "#5090dc", self.normal,  True),
      ("height",  "HEIGHT",   "#dc9050", self.height,  True),
      ("spec",    "SPECULAR", "#50dc50", self.spec,    True),
    ]

    for col, (key, title, color, asset, has_toggle) in enumerate(specs):
      cell = tk.Frame(frame, bg="#1e1e1e")
      cell.grid(row=0, column=col, padx=4, sticky="nsew")

      tk.Label(cell, text=title, bg="#1e1e1e", fg=color,
               font=("Helvetica", 10, "bold"))\
        .pack(side=tk.TOP, anchor="w")

      tile = tk.Label(cell, bg="#1e1e1e", text="(empty)", fg="#555555",
                      font=("Helvetica", 12),
                      width=TILE_SIZE, height=TILE_SIZE,
                      relief="sunken", borderwidth=2)
      tile.pack(side=tk.TOP, padx=0, pady=2)
      tile.config(width=0, height=0)  # unset char-based size after pack
      self._tile_widgets[key] = tile

      if has_toggle:
        toggle_frame = tk.Frame(cell, bg="#1e1e1e")
        toggle_frame.pack(side=tk.TOP, fill=tk.X)
        var = tk.BooleanVar(master=self.root, value=True)
        asset.show_modified = var
        ttk.Radiobutton(toggle_frame, text="Original", variable=var,
                        value=False,
                        command=lambda k=key: self._update_tile(k))\
          .pack(side=tk.LEFT, padx=4)
        ttk.Radiobutton(toggle_frame, text="Modified", variable=var,
                        value=True,
                        command=lambda k=key: self._update_tile(k))\
          .pack(side=tk.LEFT, padx=4)

      if key == "normal":
        conv = tk.Frame(cell, bg="#2b2b2b")
        conv.pack(side=tk.TOP, anchor="w", padx=4, pady=(2, 0))
        tk.Label(conv, text="Source:", bg="#2b2b2b", fg="#cccccc",
                 font=("Helvetica", 10)).pack(side=tk.LEFT)
        ttk.Radiobutton(conv, text="OpenGL", value="OpenGL",
                        variable=self.normal_convention_var,
                        command=self._on_normal_convention)\
          .pack(side=tk.LEFT, padx=2)
        ttk.Radiobutton(conv, text="DirectX", value="DirectX",
                        variable=self.normal_convention_var,
                        command=self._on_normal_convention)\
          .pack(side=tk.LEFT, padx=2)
      elif key == "height":
        conv = tk.Frame(cell, bg="#2b2b2b")
        conv.pack(side=tk.TOP, anchor="w", padx=4, pady=(2, 0))
        tk.Label(conv, text="Output:", bg="#2b2b2b", fg="#cccccc",
                 font=("Helvetica", 10)).pack(side=tk.LEFT)
        ttk.Radiobutton(conv, text="Heightmap", value="Heightmap",
                        variable=self.height_convention_var,
                        command=self._reprocess_height)\
          .pack(side=tk.LEFT, padx=2)
        ttk.Radiobutton(conv, text="Depthmap", value="Depthmap",
                        variable=self.height_convention_var,
                        command=self._reprocess_height)\
          .pack(side=tk.LEFT, padx=2)

      frame.columnconfigure(col, weight=1)

    frame.rowconfigure(0, weight=1)

    # Diffuse tile click loads file
    self._tile_widgets["diffuse"].bind("<Button-1>",
                                       lambda e: self._open_file_dialog())

  # --------------------------------------------------------------------
  # Browser
  # --------------------------------------------------------------------

  def _build_browser(self, parent):
    browser_frame = tk.Frame(parent, bg="#1e1e1e")
    browser_frame.pack(side=tk.TOP, fill=tk.BOTH, expand=True, padx=4, pady=4)

    self.browser_canvas = tk.Canvas(browser_frame, bg="#1e1e1e",
                                    highlightthickness=0)
    self.browser_scrollbar = ttk.Scrollbar(browser_frame, orient=tk.VERTICAL,
                                           command=self.browser_canvas.yview)
    self.browser_canvas.configure(yscrollcommand=self.browser_scrollbar.set)

    self.browser_scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
    self.browser_canvas.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    self.thumb_frame = tk.Frame(self.browser_canvas, bg="#1e1e1e")
    self._thumb_window = self.browser_canvas.create_window(
      (0, 0), window=self.thumb_frame, anchor="nw")

    self.thumb_frame.bind("<Configure>",
                          lambda e: self.browser_canvas.configure(
                            scrollregion=self.browser_canvas.bbox("all")))
    self.browser_canvas.bind("<Configure>", self._on_browser_resize)

    def _on_mousewheel(event):
      self.browser_canvas.yview_scroll(-1 * event.delta, "units")
    self.browser_canvas.bind("<MouseWheel>", _on_mousewheel)
    self.thumb_frame.bind("<MouseWheel>", _on_mousewheel)

    self.browser_dir: Path | None = None
    self._all_browser_files: list[Path] = []
    self.browser_files: list[Path] = []
    self._browser_set: set[Path] = set()
    self._thumb_path_map: dict[Path, tk.Label] = {}
    self._thumb_photos: dict[Path, ImageTk.PhotoImage] = {}
    self._decode_queued: set[Path] = set()
    self._selected_thumb: tk.Label | None = None
    self._browser_cols = 1
    self._thumb_executor = concurrent.futures.ThreadPoolExecutor(
      max_workers=4, thread_name_prefix="thumb")

  def _on_browser_resize(self, event):
    self.browser_canvas.itemconfigure(self._thumb_window, width=event.width)
    pad = 2 * 2 + 4
    cols = max(1, event.width // (THUMB_SIZE + pad))
    if cols != self._browser_cols:
      self._browser_cols = cols
      self._reflow_thumbs()

  def _filter_files(self, files: list[Path]) -> list[Path]:
    needle = self.filter_var.get().strip().lower()
    if not needle:
      return list(files)
    return [f for f in files if needle in f.name.lower()]

  def _schedule_filter(self):
    """Coalesce rapid keystrokes into a single filter pass."""
    if self._filter_after_id is not None:
      try:
        self.root.after_cancel(self._filter_after_id)
      except Exception:
        pass
    self._filter_after_id = self.root.after(200, self._apply_filter)

  def _apply_filter(self):
    if self.browser_dir is None:
      return
    self._incremental_update()

  def _browse_directory(self):
    path = filedialog.askdirectory(title="Select texture directory")
    if not path:
      return
    self._set_directory(Path(path))

  def _set_directory(self, directory: Path):
    self.browser_dir = directory
    self.dir_var.set(str(directory))

    # Auto-load directory-local preset if present
    preset_path = directory / PRESET_FILENAME
    if preset_path.is_file():
      try:
        self._apply_preset(json.loads(preset_path.read_text()))
        print(f"Preset: loaded {preset_path}")
      except Exception as exc:
        print(f"Preset: failed to load {preset_path}: {exc}")

    files = []
    for ext in ("*.tga", "*.png", "*.jpg", "*.jpeg"):
      files.extend(directory.rglob(ext))
    self._all_browser_files = sorted(files)
    self.browser_files = self._filter_files(self._all_browser_files)

    self.browser_count_var.set(f"{len(self.browser_files)} textures")
    print(f"Browser: {len(self.browser_files)} textures in {directory}")

    # New directory: nuke any previously-loaded thumbnails.
    for lbl in self._thumb_path_map.values():
      lbl.destroy()
    self._thumb_path_map.clear()
    self._thumb_photos.clear()
    self._decode_queued.clear()
    self._selected_thumb = None

    self._sync_browser_view()

  @staticmethod
  def _decode_thumbnail(path: Path) -> tuple | None:
    """Background worker: open + thumbnail an image. Returns (img, has_alpha).
       Pure CPU/IO; does NOT touch any Tk objects."""
    try:
      img = Image.open(path)
      img.load()
      has_alpha_height = False
      if img.mode in ("RGBA", "LA", "PA"):
        alpha = np.array(img.split()[-1])
        has_alpha_height = alpha.min() < 250 and np.std(alpha) >= 2.0
      img = img.convert("RGB")
      img.thumbnail((THUMB_SIZE, THUMB_SIZE), Image.LANCZOS)
      return (img, has_alpha_height)
    except Exception:
      return None

  def _queue_decode(self, path: Path):
    """Submit a thumbnail decode to the worker pool."""
    if path in self._thumb_path_map or path in self._decode_queued:
      return
    self._decode_queued.add(path)
    future = self._thumb_executor.submit(self._decode_thumbnail, path)
    future.add_done_callback(
      lambda fut, p=path: self.root.after(
        0, lambda: self._on_decode_done(p, fut.result())))

  def _on_decode_done(self, path: Path, result):
    """Main-thread callback: build the Label + PhotoImage from decoded data."""
    self._decode_queued.discard(path)
    if result is None:
      return
    if path in self._thumb_path_map:
      return  # raced; already built
    img, has_alpha_height = result
    photo = ImageTk.PhotoImage(img, master=self.root)
    self._thumb_photos[path] = photo

    needs_height = _is_normal_map(path) and not has_alpha_height
    border_color = "#cc3333" if needs_height else "#1e1e1e"
    pad = 3 if needs_height else 0
    lbl = tk.Label(self.thumb_frame, image=photo, bg=border_color,
                   relief="flat", borderwidth=0, cursor="hand2",
                   padx=pad, pady=pad,
                   highlightthickness=3 if needs_height else 0,
                   highlightbackground="#cc3333" if needs_height
                                                  else "#1e1e1e")
    lbl._default_highlight = "#cc3333" if needs_height else "#1e1e1e"
    lbl._needs_height = needs_height

    lbl.bind("<Button-1>",
             lambda e, p=path, l=lbl: self._on_thumb_click(p, l))
    name = path.stem
    lbl.bind("<Enter>",
             lambda e, n=name: self.browser_count_var.set(n))
    lbl.bind("<Leave>",
             lambda e: self.browser_count_var.set(
               f"{len(self.browser_files)} textures"))
    lbl.bind("<MouseWheel>",
             lambda e: self.browser_canvas.yview_scroll(-1 * e.delta, "units"))

    self._thumb_path_map[path] = lbl
    # Place into grid only if this path is in the current filtered view.
    if path in self._browser_set:
      try:
        idx = self.browser_files.index(path)
        row, col = divmod(idx, self._browser_cols)
        lbl.grid(row=row, column=col, padx=2, pady=2)
      except ValueError:
        pass

  def _sync_browser_view(self):
    """Show labels in current filter, hide others, queue any missing decodes."""
    self._browser_set = set(self.browser_files)

    # Hide labels that are no longer in the filter.
    for path, lbl in self._thumb_path_map.items():
      if path not in self._browser_set:
        lbl.grid_remove()

    # Place / queue thumbs for filtered set, in order.
    cols = self._browser_cols
    for idx, path in enumerate(self.browser_files):
      lbl = self._thumb_path_map.get(path)
      if lbl is not None:
        row, col = divmod(idx, cols)
        lbl.grid(row=row, column=col, padx=2, pady=2)
      else:
        self._queue_decode(path)

  def _reflow_thumbs(self):
    if not hasattr(self, "_browser_set"):
      self._browser_set = set(self.browser_files)
    cols = self._browser_cols
    for idx, path in enumerate(self.browser_files):
      lbl = self._thumb_path_map.get(path)
      if lbl is not None:
        row, col = divmod(idx, cols)
        lbl.grid(row=row, column=col, padx=2, pady=2)

  def _on_thumb_click(self, path: Path, label: tk.Label):
    if self._selected_thumb is not None:
      prev = self._selected_thumb
      prev.config(highlightbackground=prev._default_highlight,
                  highlightthickness=3 if prev._needs_height else 0)
    label.config(highlightbackground="#5090dc", highlightthickness=3)
    self._selected_thumb = label
    self._load_set(path)

  # --------------------------------------------------------------------
  # File I/O
  # --------------------------------------------------------------------

  def _open_file_dialog(self):
    path = filedialog.askopenfilename(
      title="Open texture",
      filetypes=[
        ("Image files", "*.tga *.png *.jpg *.jpeg"),
        ("All files",   "*"),
      ],
    )
    if path:
      self._load_set(Path(path))

  def _load_set(self, path: Path):
    """Resolve diffuse/normal/spec for the asset set sharing a base name.
    A sibling <base>.mat file, if present, can redirect normal/spec to a
    different shared file (typical for variant families)."""

    parent = path.parent
    base = _base_of(path)
    self.base_path = parent
    self.base_name = base

    diffuse_path = _find_diffuse(parent, base)
    normal_path = _find_normal(parent, base)
    spec_path = _find_spec(parent, base)

    # .mat overrides take precedence: a material may share its normalmap
    # or specularmap with sibling textures (e.g. all the variant frames of
    # an animated door point at one shared normalmap).
    mat = _resolve_mat_assets(parent, base)
    if "diffusemap" in mat:
      diffuse_path = mat["diffusemap"]
    if "normalmap" in mat:
      normal_path = mat["normalmap"]
    if "specularmap" in mat:
      spec_path = mat["specularmap"]
    if mat:
      print(f".mat: {base}  ->  "
            + ", ".join(f"{k}={v.name}" for k, v in mat.items()))

    self.diffuse = Asset(diffuse_path)
    self.diffuse.original = self._read_rgb(diffuse_path)
    self._tile_widgets["diffuse"].config(text="(no diffuse)" if
                                         self.diffuse.original is None
                                         else "")

    # Normal + height are coupled: read the normalmap PNG once.
    n_var = self.normal.show_modified
    h_var = self.height.show_modified
    self.normal = Asset(normal_path)
    self.height = Asset(normal_path)
    self.normal.show_modified = n_var
    self.height.show_modified = h_var

    if normal_path is not None:
      try:
        img = Image.open(normal_path)
        img.load()
        self.normal.original = np.array(img.convert("RGB"))
        if img.mode in ("RGBA", "LA", "PA"):
          alpha = np.array(img.split()[-1])
          if alpha.min() < 250:
            self.height.original = alpha
      except Exception as exc:
        messagebox.showerror("Error", f"Failed to read normalmap:\n{exc}")

    s_var = self.spec.show_modified
    self.spec = Asset(spec_path)
    self.spec.show_modified = s_var
    self.spec.original = self._read_rgb(spec_path)
    if self.spec.original is not None and self.spec.original.ndim == 3:
      # Specular stored as RGB; collapse to single luminance channel display
      self.spec.original = self.spec.original

    # Header
    parts = [base]
    if normal_path:
      h, w = self.normal.original.shape[:2]
      parts.append(f"({w}×{h})")
    self.file_var.set("  ".join(parts))

    # Initial preview generation; apply current slider settings so that
    # tuning carries across selections (useful for batching variant families).
    self._reprocess_normal()
    self._reprocess_spec()
    self._update_all_tiles()

  @staticmethod
  def _read_rgb(path: Path | None) -> np.ndarray | None:
    if path is None or not path.is_file():
      return None
    try:
      img = Image.open(path)
      img.load()
      return np.array(img.convert("RGB"))
    except Exception:
      return None

  # --------------------------------------------------------------------
  # Processing
  # --------------------------------------------------------------------

  def _fv(self, k: str) -> float:
    return float(self.slider_vars[k].get())

  def _params_dict(self) -> dict:
    return {key: var.get() for key, var in self.slider_vars.items()}

  def _reprocess_normal(self, cascade: bool = True):
    """Apply normal-strength attenuation + frequency reweighting."""

    if self.normal.original is None:
      self.normal.modified = None
      self._update_tile("normal")
      return

    self.normal.modified = process_normal(
      to_opengl_normal(self.normal.original,
                       source_is_directx=self.normal_convention_var.get() == "DirectX"),
      self._params_dict())
    self._update_tile("normal")

    # Heightmap depends on normalmap; re-derive unless the caller asked us
    # not to (e.g. when the normal was just *generated* from the heightmap,
    # to avoid a feedback loop).
    if cascade:
      self._reprocess_height()

  def _on_normal_convention(self):
    """Switching DirectX/OpenGL rebuilds the normalmap and the heightmap."""
    self._reprocess_normal()

  def _reprocess_height(self):
    """Apply height-tab sliders. If a heightmap is already loaded
    (self.height.original), filter it in place; otherwise derive a fresh
    heightmap from the (modified or original) normalmap."""

    as_heightmap = self.height_convention_var.get() == "Heightmap"

    if self.height.original is not None:
      self.height.modified = apply_height_filters(
        self.height.original, self._params_dict(), as_heightmap=as_heightmap)
    else:
      src = self.normal.modified if self.normal.modified is not None \
        else self.normal.original
      if src is None:
        self.height.modified = None
        self._update_tile("height")
        return
      self.height.modified = process_height(
        src, self._params_dict(), as_heightmap=as_heightmap)

    std = float(np.std(self.height.modified))
    self.stats_var.set(f"height std: {std:.1f}")
    self._update_tile("height")
    self._reprocess_spec()

  def _generate_height_from_normal(self):
    """Replace the loaded heightmap with one freshly integrated from the
    current normalmap, then run the slider filters on top."""

    src = self.normal.modified if self.normal.modified is not None \
      else self.normal.original
    if src is None:
      messagebox.showinfo("Info", "No normalmap loaded.")
      return

    as_heightmap = self.height_convention_var.get() == "Heightmap"
    # Integrate at neutral filter settings so the integration result is
    # the new "original"; sliders then act on top via _reprocess_height.
    neutral = {"h_freq_cutoff": 0.3, "h_freq_low_gain": 1.0,
               "h_freq_high_gain": 1.0, "h_strength": 1.0}
    self.height.original = process_height(src, neutral,
                                          as_heightmap=as_heightmap)
    self._reprocess_height()

  def _reprocess_spec(self):
    """Generate specularmap from diffuse + height."""

    if self.diffuse.original is None:
      self.spec.modified = None
      self._update_tile("spec")
      return

    height_src = self.height.modified if self.height.modified is not None \
      else self.height.original
    self.spec.modified = process_spec(
      self.diffuse.original, height_src, self._params_dict())
    self._update_tile("spec")

  def _generate_normal_from_diffuse(self):
    if self.diffuse.original is None:
      messagebox.showinfo("Info", "No diffuse texture loaded.")
      return

    normals, height = normals_from_diffuse(
      self.diffuse.original,
      strength=self._fv("normal_strength"),
      cutoff=self._fv("n_freq_cutoff"),
      low_gain=self._fv("n_freq_low_gain"),
      high_gain=self._fv("n_freq_high_gain"),
    )
    # Treat the generated normals as the new "original" so that subsequent
    # re-processing stays consistent.
    self.normal.original = normals
    self.normal.modified = normals
    self.height.original = height
    self.height.modified = height
    if self.normal.path is None and self.base_path is not None \
        and self.base_name is not None:
      self.normal.path = self.base_path / (self.base_name + NORMAL_SUFFIX
                                           + NORMAL_EXT)
    if self.height.path is None:
      self.height.path = self.normal.path
    self._update_all_tiles()
    self._reprocess_height()
    self._reprocess_spec()

  def _generate_normal_from_height(self):
    src = self.height.display()
    if src is None:
      messagebox.showinfo("Info", "No heightmap loaded.")
      return

    normals = normals_from_height(src, strength=self._fv("normal_strength"))
    self.normal.original = normals
    self.normal.modified = None
    if self.normal.path is None and self.base_path is not None \
        and self.base_name is not None:
      self.normal.path = self.base_path / (self.base_name + NORMAL_SUFFIX
                                           + NORMAL_EXT)
    self._reprocess_normal(cascade=False)
    self._reprocess_spec()

  def _sanitize_normal(self):
    """Replace 'dead' RGB pixels (those baked under low-alpha heightmap
    regions) with flat normal (128, 128, 255). The source baker often
    leaves junk normals where the heightmap is deeply concave; image
    viewers hide them via alpha compositing, but the engine sees them.
    Lerps RGB toward flat by (1 - alpha/threshold) where alpha < threshold."""

    if self.normal.original is None:
      messagebox.showinfo("Info", "No normalmap loaded.")
      return
    if self.height.original is None:
      messagebox.showinfo("Info",
                          "No heightmap (alpha) to drive sanitization.")
      return

    rgb = self.normal.original.astype(np.float32)
    h, w = rgb.shape[:2]
    if self.height.original.shape[:2] != (h, w):
      messagebox.showinfo("Info", "Heightmap size mismatch; cannot sanitize.")
      return

    THRESHOLD = 64.0  # alpha below this is considered "dead"
    alpha = self.height.original.astype(np.float32)
    weight = np.clip(1.0 - alpha / THRESHOLD, 0.0, 1.0)[..., None]

    flat = np.array([128.0, 128.0, 255.0], dtype=np.float32)
    sanitized = (1.0 - weight) * rgb + weight * flat
    sanitized = np.clip(sanitized, 0.0, 255.0).astype(np.uint8)

    affected = int((weight > 0).sum())
    print(f"Sanitize: blended {affected} pixels (alpha < {int(THRESHOLD)})"
          f" toward flat normal")

    self.normal.original = sanitized
    self._reprocess_normal()

  # --------------------------------------------------------------------
  # Display
  # --------------------------------------------------------------------

  def _update_all_tiles(self):
    for k in ("diffuse", "normal", "height", "spec"):
      self._update_tile(k)

  def _update_tile(self, key: str):
    asset: Asset = getattr(self, key)
    arr = asset.display()
    tile = self._tile_widgets[key]

    if arr is None:
      tile.config(image="", text=f"(no {key})")
      self._tile_photos.pop(key, None)
      return

    if arr.ndim == 2:
      img = Image.fromarray(arr, mode="L").convert("RGB")
    else:
      img = Image.fromarray(arr, mode="RGB")

    w, h = img.size
    scale = TILE_SIZE / max(w, h)
    if scale != 1.0:
      img = img.resize((int(w * scale), int(h * scale)), Image.LANCZOS)

    photo = ImageTk.PhotoImage(img, master=self.root)
    self._tile_photos[key] = photo
    tile.config(image=photo, text="")

  # --------------------------------------------------------------------
  # Saving
  # --------------------------------------------------------------------

  # --------------------------------------------------------------------
  # Presets
  # --------------------------------------------------------------------

  def _collect_preset(self) -> dict:
    """Snapshot all slider values + conventions into a dict."""
    preset = {
      "normal_convention": self.normal_convention_var.get(),
      "height_convention": self.height_convention_var.get(),
      "params": {key: var.get() for key, var in self.slider_vars.items()},
    }
    return preset

  def _apply_preset(self, preset: dict):
    """Apply a preset dict to the UI variables. Unknown keys are ignored."""
    nc = preset.get("normal_convention")
    if nc in ("OpenGL", "DirectX"):
      self.normal_convention_var.set(nc)
    hc = preset.get("height_convention")
    if hc in ("Heightmap", "Depthmap"):
      self.height_convention_var.set(hc)
    for key, val in (preset.get("params") or {}).items():
      var = self.slider_vars.get(key)
      if var is not None:
        try:
          var.set(val)
        except Exception:
          pass
    # Re-derive everything that may have changed
    self._reprocess_normal()
    self._reprocess_spec()

  def _save_preset_dialog(self):
    initial = str(self.browser_dir / PRESET_FILENAME) if self.browser_dir \
      else PRESET_FILENAME
    path = filedialog.asksaveasfilename(
      title="Save preset", defaultextension=".json",
      initialfile=Path(initial).name,
      initialdir=str(Path(initial).parent),
      filetypes=[("JSON preset", "*.json")])
    if not path:
      return
    Path(path).write_text(json.dumps(self._collect_preset(), indent=2) + "\n")
    print(f"Preset: saved {path}")

  def _load_preset_dialog(self):
    initial = str(self.browser_dir) if self.browser_dir else "."
    path = filedialog.askopenfilename(
      title="Load preset",
      initialdir=initial,
      filetypes=[("JSON preset", "*.json")])
    if not path:
      return
    try:
      self._apply_preset(json.loads(Path(path).read_text()))
      print(f"Preset: loaded {path}")
    except Exception as exc:
      messagebox.showerror("Error", f"Failed to load preset:\n{exc}")

  def _save_normal(self):
    """Save the modified normalmap (RGB), preserving existing height (A)."""

    normals = self.normal.modified if self.normal.modified is not None \
      else self.normal.original
    if normals is None:
      messagebox.showinfo("Info", "No normalmap to save.")
      return
    if self.base_path is None or self.base_name is None:
      messagebox.showinfo("Info", "No output location.")
      return

    out_path = self.normal.path or (
      self.base_path / (self.base_name + NORMAL_SUFFIX + NORMAL_EXT))
    h, w = normals.shape[:2]
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    # WYSIWYG: save exactly what's shown in the Modified tile.
    rgba[:, :, :3] = normals
    # Re-merge the current heightmap into the alpha channel so the .mat-
    # referenced file ends up with both the edited normal and the edited
    # height. Fall back to whatever's on disk, then a flat 255.
    height = self.height.modified if self.height.modified is not None \
      else self.height.original
    if height is not None and height.shape[:2] == (h, w):
      rgba[:, :, 3] = height
    else:
      rgba[:, :, 3] = self._existing_alpha(out_path, w, h)

    rgba = fix_tiling_borders_rgba(rgba)
    Image.fromarray(rgba, mode="RGBA").save(out_path, optimize=True)
    print(f"Saved: {out_path.name}")

    self.normal.path = out_path
    self.normal.original = normals
    self._update_tile("normal")
    self._refresh_thumb_for(out_path)

  def _save_height(self):
    """Save the modified heightmap (A), preserving existing normal (RGB)."""

    height = self.height.modified if self.height.modified is not None \
      else self.height.original
    if height is None:
      messagebox.showinfo("Info", "No heightmap to save.")
      return
    if self.base_path is None or self.base_name is None:
      messagebox.showinfo("Info", "No output location.")
      return

    out_path = self.height.path or self.normal.path or (
      self.base_path / (self.base_name + NORMAL_SUFFIX + NORMAL_EXT))
    h, w = height.shape[:2]
    rgba = np.zeros((h, w, 4), dtype=np.uint8)
    rgba[:, :, :3] = self._existing_rgb(out_path, w, h)
    rgba[:, :, 3] = height

    rgba = fix_tiling_borders_rgba(rgba)
    Image.fromarray(rgba, mode="RGBA").save(out_path, optimize=True)
    print(f"Saved: {out_path.name}")

    self.height.path = out_path
    self.height.original = height
    self._update_tile("height")
    self._refresh_thumb_for(out_path)

  def _existing_alpha(self, path: Path, w: int, h: int) -> np.ndarray:
    """Return alpha channel of existing PNG at (w,h), or 255 if missing."""
    if path.exists():
      try:
        img = np.array(Image.open(path).convert("RGBA"))
        a = img[:, :, 3]
        if a.shape != (h, w):
          a = cv2.resize(a, (w, h), interpolation=cv2.INTER_LINEAR)
        return a
      except Exception:
        pass
    return np.full((h, w), 255, dtype=np.uint8)

  def _existing_rgb(self, path: Path, w: int, h: int) -> np.ndarray:
    """Return RGB channels of existing PNG at (w,h), or flat normal if missing."""
    if path.exists():
      try:
        img = np.array(Image.open(path).convert("RGBA"))
        rgb = img[:, :, :3]
        if rgb.shape[:2] != (h, w):
          rgb = cv2.resize(rgb, (w, h), interpolation=cv2.INTER_LINEAR)
        return rgb
      except Exception:
        pass
    flat = np.zeros((h, w, 3), dtype=np.uint8)
    flat[:, :, 0] = 128
    flat[:, :, 1] = 128
    flat[:, :, 2] = 255
    return flat

  def _save_spec(self):
    """Save the modified specularmap as a JPG."""

    spec = self.spec.modified if self.spec.modified is not None \
      else self.spec.original
    if spec is None:
      messagebox.showinfo("Info", "No specularmap to save.")
      return
    if self.base_path is None or self.base_name is None:
      messagebox.showinfo("Info", "No output location.")
      return

    if spec.ndim == 2:
      img = Image.fromarray(spec, mode="L").convert("RGB")
    else:
      img = Image.fromarray(spec, mode="RGB")

    out_path = self.spec.path or (
      self.base_path / (self.base_name + SPEC_SUFFIX + SPEC_EXT))
    img.save(out_path, quality=92, optimize=True)
    print(f"Saved: {out_path.name}")

    self.spec.path = out_path
    self.spec.original = np.array(img)
    self._update_tile("spec")
    self._refresh_thumb_for(out_path)

  # --------------------------------------------------------------------
  # Browser auto-refresh
  # --------------------------------------------------------------------

  def _refresh_thumb_for(self, path: Path):
    lbl = self._thumb_path_map.get(path)
    if lbl is None:
      # File is new in the directory; trigger a poll
      self.root.after(50, self._poll_directory_now)
      return

    # Re-decode the image so the thumbnail reflects what's now on disk.
    result = self._decode_thumbnail(path)
    if result is None:
      return
    img, has_alpha_height = result
    photo = ImageTk.PhotoImage(img, master=self.root)
    self._thumb_photos[path] = photo
    lbl.config(image=photo)

    needs_height = _is_normal_map(path) and not has_alpha_height
    lbl._needs_height = needs_height
    lbl._default_highlight = "#cc3333" if needs_height else "#1e1e1e"
    border_color = "#cc3333" if needs_height else "#1e1e1e"
    lbl.config(bg=border_color, padx=3 if needs_height else 0,
               pady=3 if needs_height else 0)
    if lbl is self._selected_thumb:
      lbl.config(highlightbackground="#5090dc", highlightthickness=3)
    else:
      lbl.config(highlightbackground=lbl._default_highlight,
                 highlightthickness=3 if needs_height else 0)

  def _poll_directory(self):
    self._poll_directory_now()
    self.root.after(1000, self._poll_directory)

  def _poll_directory_now(self):
    if self.browser_dir is None or not self.browser_dir.is_dir():
      return
    files = []
    for ext in ("*.tga", "*.png", "*.jpg", "*.jpeg"):
      files.extend(self.browser_dir.rglob(ext))
    files = sorted(files)
    if files != self._all_browser_files:
      self._all_browser_files = files
      self._incremental_update(self._filter_files(files))

  def _incremental_update(self, new_files: list[Path] | None = None):
    if new_files is None:
      new_files = self._filter_files(self._all_browser_files)
    self.browser_files = new_files
    self.browser_count_var.set(f"{len(self.browser_files)} textures")
    self._sync_browser_view()


# ---------------------------------------------------------------------------
# Batch CLI
# ---------------------------------------------------------------------------

def _load_image_rgb(path: Path) -> np.ndarray | None:
  if not path.is_file():
    return None
  try:
    img = Image.open(path)
    img.load()
    return np.array(img.convert("RGB"))
  except Exception as exc:
    print(f"  ! load failed {path.name}: {exc}", file=sys.stderr)
    return None


def _load_normal_rgba(path: Path) -> tuple[np.ndarray, np.ndarray] | None:
  """Returns (rgb, alpha) from a normalmap PNG, or (rgb, 255-filled) if no alpha."""
  if not path.is_file():
    return None
  try:
    img = Image.open(path)
    img.load()
    rgb = np.array(img.convert("RGB"))
    if img.mode in ("RGBA", "LA", "PA"):
      alpha = np.array(img.split()[-1])
    else:
      alpha = np.full(rgb.shape[:2], 255, dtype=np.uint8)
    return rgb, alpha
  except Exception as exc:
    print(f"  ! load failed {path.name}: {exc}", file=sys.stderr)
    return None


def _save_normal_rgba(out_path: Path, normal_rgb: np.ndarray,
                      height_a: np.ndarray):
  h, w = normal_rgb.shape[:2]
  if height_a.shape != (h, w):
    height_a = cv2.resize(height_a, (w, h), interpolation=cv2.INTER_LINEAR)
  rgba = np.zeros((h, w, 4), dtype=np.uint8)
  # WYSIWYG: pipeline already produces the bytes we want on disk.
  rgba[:, :, :3] = normal_rgb
  rgba[:, :, 3] = height_a
  rgba = fix_tiling_borders_rgba(rgba)
  Image.fromarray(rgba, mode="RGBA").save(out_path, optimize=True)


def _save_spec_jpg(out_path: Path, spec: np.ndarray):
  if spec.ndim == 2:
    img = Image.fromarray(spec, mode="L").convert("RGB")
  else:
    img = Image.fromarray(spec, mode="RGB")
  img.save(out_path, quality=92, optimize=True)


def _enumerate_bases(directory: Path) -> list[tuple[str, Path]]:
  """Discover (base, parent) pairs by scanning for diffuse files."""
  bases: dict[tuple[Path, str], None] = {}
  for ext in DIFFUSE_EXTS:
    for path in directory.rglob(f"*{ext}"):
      if _is_normal_map(path) or _is_spec_map(path):
        continue
      bases[(path.parent, path.stem)] = None
  return [(stem, parent) for (parent, stem) in bases]


def cmd_batch(args) -> int:
  preset = json.loads(Path(args.preset).read_text())
  params = preset.get("params") or {}
  dx_to_gl = preset.get("normal_convention", "OpenGL") == "DirectX"
  as_heightmap = preset.get("height_convention", "Heightmap") == "Heightmap"
  save_set = set(s.strip() for s in args.save.split(",") if s.strip())
  unknown = save_set - {"normal", "height", "spec"}
  if unknown:
    print(f"Unknown --save targets: {', '.join(unknown)}", file=sys.stderr)
    return 2

  if args.directory:
    bases = _enumerate_bases(Path(args.directory).expanduser())
  elif args.files:
    bases = []
    for f in args.files:
      p = Path(f).expanduser()
      bases.append((_base_of(p), p.parent))
  else:
    print("--directory or --files required", file=sys.stderr)
    return 2

  if args.filter:
    needle = args.filter.lower()
    bases = [b for b in bases if needle in b[0].lower()]

  print(f"Batch: {len(bases)} candidates  "
        f"(normal={preset.get('normal_convention')}, "
        f"height={preset.get('height_convention')}, "
        f"save={','.join(sorted(save_set))})")

  ok = 0
  skipped = 0
  for base, parent in sorted(bases):
    diffuse_path = _find_diffuse(parent, base)
    normal_path = parent / (base + NORMAL_SUFFIX + NORMAL_EXT)
    spec_path = parent / (base + SPEC_SUFFIX + SPEC_EXT)

    # Need a normal source for normal/height save targets
    needs_normal_src = bool({"normal", "height"} & save_set)
    needs_diffuse = "spec" in save_set

    norm_rgb = None
    norm_alpha = None
    if needs_normal_src:
      loaded = _load_normal_rgba(normal_path)
      if loaded is None:
        print(f"  - {base}: no normalmap, skipping")
        skipped += 1
        continue
      norm_rgb, norm_alpha = loaded

    diffuse = None
    if needs_diffuse:
      if diffuse_path is None:
        print(f"  - {base}: no diffuse, skipping spec")
      else:
        diffuse = _load_image_rgb(diffuse_path)

    # Process pipeline
    proc_normal = None
    proc_height = None
    if norm_rgb is not None:
      proc_normal = process_normal(
        to_opengl_normal(norm_rgb, source_is_directx=dx_to_gl), params)
      proc_height = process_height(proc_normal, params, as_heightmap=as_heightmap)

    proc_spec = None
    if diffuse is not None:
      height_for_spec = proc_height if proc_height is not None else norm_alpha
      proc_spec = process_spec(diffuse, height_for_spec, params)

    # Write
    actions = []
    if "normal" in save_set or "height" in save_set:
      out_normal = proc_normal if "normal" in save_set else norm_rgb
      out_height = proc_height if "height" in save_set else norm_alpha
      actions.append(f"-> {normal_path.name}")
      if not args.dry_run:
        _save_normal_rgba(normal_path, out_normal, out_height)

    if "spec" in save_set and proc_spec is not None:
      actions.append(f"-> {spec_path.name}")
      if not args.dry_run:
        _save_spec_jpg(spec_path, proc_spec)

    if actions:
      print(f"  + {base}  {' '.join(actions)}")
      ok += 1
    else:
      skipped += 1

  print(f"Done: {ok} written, {skipped} skipped"
        + (" (dry run)" if args.dry_run else ""))
  return 0


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def cmd_gui(args) -> int:
  root = tk.Tk()
  app = NormalmapFuApp(root)

  directory = Path(args.directory).expanduser() if args.directory else None
  files = [Path(f).expanduser() for f in (args.files or [])]

  if directory and directory.is_dir():
    root.after(100, lambda: app._set_directory(directory))
  elif files and files[0].is_file():
    root.after(100, lambda: app._set_directory(files[0].parent))

  if files and files[0].is_file():
    root.after(200, lambda: app._load_set(files[0]))

  root.mainloop()
  return 0


def main() -> int:
  import argparse
  parser = argparse.ArgumentParser(
    prog="normalmap-fu",
    description="Author Quetoo per-pixel material assets (normal/height/spec).")
  sub = parser.add_subparsers(dest="cmd")

  gui = sub.add_parser("gui", help="Launch the GUI (default).")
  gui.add_argument("-d", "--directory")
  gui.add_argument("files", nargs="*")
  gui.set_defaults(func=cmd_gui)

  batch = sub.add_parser("batch", help="Reprocess assets headlessly.")
  batch.add_argument("--preset", required=True,
                     help="Path to preset .json (saved from the GUI).")
  group = batch.add_mutually_exclusive_group()
  group.add_argument("-d", "--directory",
                     help="Recursively process all bases under this directory.")
  group.add_argument("--files", nargs="+",
                     help="Explicit list of files (any of diffuse/norm/spec).")
  batch.add_argument("--save", default="normal,height,spec",
                     help="Comma-separated list of {normal,height,spec}.")
  batch.add_argument("--filter",
                     help="Only process bases whose name contains this substring.")
  batch.add_argument("--dry-run", action="store_true",
                     help="Report what would be written without writing.")
  batch.set_defaults(func=cmd_batch)

  # Default to GUI when no subcommand given.  Preserve backward-compatible
  # `--directory PATH` and bare-file forms by falling through to gui parsing.
  argv = sys.argv[1:]
  if not argv or argv[0] not in ("gui", "batch", "-h", "--help"):
    argv = ["gui", *argv]
  args = parser.parse_args(argv)
  return args.func(args) or 0


if __name__ == "__main__":
  sys.exit(main())
