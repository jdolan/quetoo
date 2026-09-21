#!/usr/bin/env python3
"""Tintmap builder for player skins.

A tintmap is an RGBA image the size of its diffuse map. Alpha masks the region
that takes a player's color, and R, G, B carry the shading of the shirt, pants
and helmet regions respectively (see mesh_fs.glsl). The shading follows the
diffuse map, so only the mask and the channel assignment need judgment. This
tool supplies both from the model:

  propose  rasterizes every UV triangle of the skin's meshes into texture space,
           segments the covered area into color regions, and writes a numbered
           overlay per texture for review.
  build    reads a recipe assigning surfaces and regions to channels, writes the
           `<texture>_tint.png` files, updates the `.mat` tint defaults so that
           an untinted player looks like the original skin, and renders preview
           strips.
  wire     draws the UV wireframe of every mesh on a transparent image the size
           of its diffuse map, one `<texture>_tris.png` per texture, to drop in
           as a layer when a skin or tintmap is edited by hand.

Recipe format (JSON):

  {
    "segmentation": {"segments": 400, "compactness": 10, "threshold": 12, "min_area": 64, "speck_area": 1000},
    "textures": {
      "default_upper": {
        "segmentation": {"segments": 800, "threshold": 6},
        "examples": {"shirt": [24, 165], "none": [70, 41]},
        "surfaces": {"u_torso": "shirt"},
        "regions": {"12": "none", "13": {"channel": "pants", "alpha": 0.8}},
        "mask": {"hue": 40, "chroma": 25, "spread": 30}
      }
    }
  }

A texture's own "segmentation" overrides the global parameters for that texture.
Examples spread a few regions per class over the whole texture by color and
local contrast, for skins whose plates and hide are too close in color for the
segmentation to separate. A class is a channel name, "none", or "auto", which
takes the channel from the surface prefix (l_ pants, u_ shirt, h_ helmet).
Surface rules apply next, region rules override both. A mask then keeps only
the pixels near a Lab hue (degrees) above a chroma, so a team skin's paint can
define the region exactly. A texture absent from the recipe gets no tintmap.
"""

import argparse
import json
import re
import sys
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont
from scipy import ndimage
from skimage import color as skcolor
from skimage import graph as skgraph
from skimage import segmentation

from md3 import Md3Surface, parse_skin, read_md3, resolve_texture

CHANNELS = {"shirt": 0, "pants": 1, "helmet": 2}
SURFACE_PREFIXES = ("l_", "u_", "h_")
PREFIX_CHANNELS = {"l_": CHANNELS["pants"], "u_": CHANNELS["shirt"], "h_": CHANNELS["helmet"]}
EFFECT_TEXTURES = re.compile(r"(magic\d*|lasersmoke\w*|miniscreen\w*|_visor|_eye|_eyes|_teeth|_i)$")
PARTS = ("lower", "upper", "head")
TARGET_SHADE = 0.5  # mean tint channel value inside a region, as in the hand-painted sets
DEFAULT_SEGMENTATION = {"segments": 400, "compactness": 10.0, "threshold": 12.0, "min_area": 64, "speck_area": 1000}


@dataclass
class TextureJob:
  rel_path: str
  image_path: Path
  surfaces: list[Md3Surface]


@dataclass
class Rasterized:
  coverage: np.ndarray  # bool
  surface_ids: np.ndarray  # int32, -1 outside coverage
  island_ids: np.ndarray  # int32, -1 outside coverage
  surface_names: list[str]


def gather_jobs(data_root: Path, model: str, skin: str) -> list[TextureJob]:
  """Collects, per diffuse texture of the skin, the MD3 surfaces mapped onto it.
  Effect textures (visors, eyes, teeth, magic) are skipped."""
  model_dir = data_root / "players" / model
  mapping = parse_skin(model_dir / f"{skin}.skin")

  surfaces: dict[str, Md3Surface] = {}
  for part in PARTS:
    path = model_dir / f"{part}.md3"
    if path.is_file():
      for surface in read_md3(path).surfaces:
        surfaces[surface.name] = surface

  jobs: dict[str, TextureJob] = {}
  for name, rel_path in mapping.items():
    if not name.startswith(SURFACE_PREFIXES):
      continue
    if EFFECT_TEXTURES.search(rel_path):
      print(f"{rel_path}: effect texture, skipped")
      continue
    if name not in surfaces:
      raise SystemExit(f"{skin}.skin names surface {name}, which no {PARTS} .md3 defines")
    if rel_path not in jobs:
      image_path = resolve_texture(data_root, rel_path)
      if image_path is None:
        raise SystemExit(f"{rel_path}: no image found under {data_root}")
      jobs[rel_path] = TextureJob(rel_path, image_path, [])
    jobs[rel_path].surfaces.append(surfaces[name])

  return list(jobs.values())


def uv_islands(surface: Md3Surface) -> list[int]:
  """Labels each triangle with the id of its connected group in UV space."""
  parent = list(range(len(surface.texcoords)))

  def find(i):
    while parent[i] != i:
      parent[i] = parent[parent[i]]
      i = parent[i]
    return i

  by_uv: dict[tuple[int, int], int] = {}
  for i, (s, t) in enumerate(surface.texcoords):
    key = (round(s * 4096), round(t * 4096))
    if key in by_uv:
      parent[find(i)] = find(by_uv[key])
    else:
      by_uv[key] = i

  for a, b, c in surface.triangles:
    parent[find(a)] = find(b)
    parent[find(b)] = find(c)

  roots: dict[int, int] = {}
  labels = []
  for a, _, _ in surface.triangles:
    root = find(a)
    labels.append(roots.setdefault(root, len(roots)))
  return labels


def rasterize(job: TextureJob, width: int, height: int) -> Rasterized:
  """Draws every UV triangle into a surface-id image and an island-id image."""
  surface_img = Image.new("I", (width, height), -1)
  island_img = Image.new("I", (width, height), -1)
  surface_draw = ImageDraw.Draw(surface_img)
  island_draw = ImageDraw.Draw(island_img)

  names = []
  island_base = 0
  for surface_index, surface in enumerate(job.surfaces):
    names.append(surface.name)
    islands = uv_islands(surface)
    for tri, island in zip(surface.triangles, islands):
      points = [((surface.texcoords[i][0] % 1.0) * width, (surface.texcoords[i][1] % 1.0) * height) for i in tri]
      surface_draw.polygon(points, fill=surface_index)
      island_draw.polygon(points, fill=island_base + island)
    island_base += max(islands, default=-1) + 1

  surface_ids = np.asarray(surface_img, dtype=np.int32)
  island_ids = np.asarray(island_img, dtype=np.int32)
  return Rasterized(surface_ids >= 0, surface_ids, island_ids, names)


def segment(rgb: np.ndarray, raster: Rasterized, params: dict) -> np.ndarray:
  """Splits the covered area into color regions. Returns an int32 label image
  with 0 outside coverage and region ids from 1, each region lying within one
  UV island."""
  lab = skcolor.rgb2lab(rgb)
  mask = raster.coverage

  sp = segmentation.slic(lab, n_segments=params["segments"], compactness=params["compactness"], mask=mask,
                         start_label=1, channel_axis=-1, convert2lab=False)
  sp[~mask] = 0

  # SLIC leaves masked pixels at 0 where no seed lands; each such component becomes its own superpixel
  unlabeled, _ = ndimage.label(mask & (sp == 0))
  sp = np.where(unlabeled > 0, unlabeled + sp.max(), sp)
  if (mask & (sp == 0)).any():
    raise RuntimeError("superpixel labeling left covered pixels unlabeled")

  # a superpixel may straddle two islands; split it so no region ever crosses one
  islands = raster.island_ids - raster.island_ids[mask].min() + 1
  sp = np.where(mask, sp * (islands.max() + 1) + islands, 0)
  sp = segmentation.relabel_sequential(sp)[0]

  sp_island = np.zeros(sp.max() + 1, dtype=np.int32)
  sp_island[sp[mask]] = raster.island_ids[mask]
  rag = skgraph.rag_mean_color(lab, sp)
  for u, v, data in rag.edges(data=True):
    if u == 0 or v == 0 or sp_island[u] != sp_island[v]:
      data["weight"] = np.inf
  # cut_threshold numbers components from 0, and 0 is our background
  merged = skgraph.cut_threshold(sp, rag, params["threshold"], in_place=False) + 1
  merged[~mask] = 0

  labels = absorb_small_regions(merged, mask, raster.island_ids, params["min_area"])
  return segmentation.relabel_sequential(labels)[0].astype(np.int32)


def absorb_small_regions(labels: np.ndarray, mask: np.ndarray, island_ids: np.ndarray, min_area: int) -> np.ndarray:
  """Merges regions under `min_area` pixels into their largest neighbor on the
  same UV island, so the overlay stays readable and the recipe stays short."""
  labels = labels.copy()
  while True:
    ids, counts = np.unique(labels[mask], return_counts=True)
    small = ids[counts < min_area]
    if len(small) == 0 or len(ids) == 1:
      return labels
    changed = False
    for small_id in small:
      region = labels == small_id
      island = island_ids == island_ids[region][0]
      ring = ndimage.binary_dilation(region) & island & ~region
      neighbors = labels[ring]
      if neighbors.size == 0:
        continue
      values, votes = np.unique(neighbors, return_counts=True)
      labels[region] = values[np.argmax(votes)]
      changed = True
    if not changed:
      return labels


def load_rgb(path: Path) -> np.ndarray:
  return np.asarray(Image.open(path).convert("RGB")).astype(np.float32) / 255.0


def region_table(rgb: np.ndarray, raster: Rasterized, labels: np.ndarray) -> dict[int, dict]:
  table = {}
  for region in np.unique(labels[labels > 0]):
    mask = labels == region
    ys, xs = np.nonzero(mask)
    table[int(region)] = {
      "surface": raster.surface_names[int(raster.surface_ids[ys[0], xs[0]])],
      "island": int(raster.island_ids[ys[0], xs[0]]),
      "area": int(mask.sum()),
      "mean_rgb": [int(round(c * 255)) for c in rgb[mask].mean(axis=0)],
      "centroid": [int(xs.mean()), int(ys.mean())],
    }
  return table


def draw_overlay(rgb: np.ndarray, raster: Rasterized, labels: np.ndarray, table: dict[int, dict]) -> Image.Image:
  """The review image: the skin dimmed outside coverage, region borders in
  white, surface borders in cyan, and each region's id at its centroid."""
  height, width = labels.shape
  base = rgb * np.where(raster.coverage[..., None], 1.0, 0.35)
  borders = segmentation.find_boundaries(labels, mode="inner")
  surface_borders = segmentation.find_boundaries(raster.surface_ids, mode="inner") & raster.coverage
  base[borders] = (1.0, 1.0, 1.0)
  base[surface_borders] = (0.0, 1.0, 1.0)
  image = Image.fromarray((base * 255).astype(np.uint8))

  font = ImageFont.load_default(size=max(12, width // 80))
  draw = ImageDraw.Draw(image)
  for region, info in table.items():
    x, y = info["centroid"]
    text = str(region)
    box = draw.textbbox((x, y), text, font=font, anchor="mm")
    draw.rectangle(box, fill=(0, 0, 0))
    draw.text((x, y), text, fill=(255, 230, 0), font=font, anchor="mm")
  return image


def draw_surfaces(rgb: np.ndarray, raster: Rasterized) -> Image.Image:
  """A second review image coloring each surface, to see what the mesh covers."""
  palette = np.array([[220, 40, 40], [40, 200, 40], [40, 90, 255], [240, 220, 0],
                      [240, 0, 240], [0, 220, 220], [255, 140, 0], [140, 80, 0]], dtype=np.float32) / 255.0
  tint = palette[raster.surface_ids % len(palette)]
  image = np.where(raster.coverage[..., None], rgb * 0.5 + tint * 0.5, rgb * 0.35)
  return Image.fromarray((image * 255).astype(np.uint8))


def draw_wireframe(size: tuple[int, int], job: TextureJob) -> Image.Image:
  """Every UV triangle outlined in white on transparency, surface borders in cyan."""
  width, height = size
  image = Image.new("RGBA", size, (0, 0, 0, 0))
  draw = ImageDraw.Draw(image)
  for surface in job.surfaces:
    for tri in surface.triangles:
      points = [((surface.texcoords[i][0] % 1.0) * width, (surface.texcoords[i][1] % 1.0) * height) for i in tri]
      draw.polygon(points, outline=(255, 255, 255, 255))
  raster = rasterize(job, width, height)
  borders = segmentation.find_boundaries(raster.surface_ids, mode="thick") & raster.coverage
  pixels = np.asarray(image).copy()
  pixels[borders] = (0, 255, 255, 255)
  return Image.fromarray(pixels, "RGBA")


def wire(args):
  data_root = Path(args.data).expanduser()
  out_dir = Path(args.out).expanduser()
  out_dir.mkdir(parents=True, exist_ok=True)
  for job in gather_jobs(data_root, args.model, args.skin):
    size = Image.open(job.image_path).size
    path = out_dir / f"{texture_name(job)}_tris.png"
    draw_wireframe(size, job).save(path)
    print(f"{job.rel_path}: {sum(len(s.triangles) for s in job.surfaces)} triangles -> {path}")


def texture_name(job: TextureJob) -> str:
  return job.rel_path.rsplit("/", 1)[-1]


def segmentation_params(recipe: dict | None, texture: str | None = None) -> dict:
  """The global segmentation parameters, with a texture's own overrides applied."""
  params = dict(DEFAULT_SEGMENTATION)
  if recipe:
    params.update(recipe.get("segmentation", {}))
    if texture:
      params.update(recipe.get("textures", {}).get(texture, {}).get("segmentation", {}))
  return params


def propose(args):
  data_root = Path(args.data).expanduser()
  jobs = gather_jobs(data_root, args.model, args.skin)
  out_dir = Path(args.out).expanduser() / args.model
  out_dir.mkdir(parents=True, exist_ok=True)
  recipe = json.loads(Path(args.recipe).read_text()) if args.recipe else None

  for job in jobs:
    name = texture_name(job)
    params = segmentation_params(recipe, name)
    rgb = load_rgb(job.image_path)
    height, width = rgb.shape[:2]
    raster = rasterize(job, width, height)
    labels = segment(rgb, raster, params)
    table = region_table(rgb, raster, labels)

    Image.fromarray(labels.astype(np.uint16)).save(out_dir / f"{name}_labels.png")
    draw_overlay(rgb, raster, labels, table).save(out_dir / f"{name}_regions.png")
    draw_surfaces(rgb, raster).save(out_dir / f"{name}_surfaces.png")
    (out_dir / f"{name}_regions.json").write_text(json.dumps({
      "texture": job.rel_path,
      "size": [width, height],
      "surfaces": raster.surface_names,
      "segmentation": params,
      "regions": table,
    }, indent=2) + "\n")

    print(f"{job.rel_path}: {len(job.surfaces)} surfaces {raster.surface_names}, "
          f"{raster.coverage.mean():.0%} covered, {len(table)} regions -> {out_dir / name}_regions.png")


def resolve_rule(rule) -> tuple[str | None, float]:
  if isinstance(rule, str):
    name, alpha = rule, 1.0
  else:
    name, alpha = rule.get("channel", "none"), float(rule.get("alpha", 1.0))
  if name != "none" and name not in CHANNELS:
    raise SystemExit(f"unknown channel {name!r}; use {list(CHANNELS)} or none")
  return (None if name == "none" else name), alpha


def pixel_features(rgb: np.ndarray) -> np.ndarray:
  """Smoothed Lab plus local L contrast, so painted plates (flat) separate from
  hide or cloth (textured) even when their colors are close."""
  lab = skcolor.rgb2lab(rgb)
  smooth = np.stack([ndimage.gaussian_filter(lab[..., i], 2.0) for i in range(3)], axis=-1)
  mean = ndimage.uniform_filter(lab[..., 0], 9)
  mean_sq = ndimage.uniform_filter(lab[..., 0] ** 2, 9)
  contrast = np.sqrt(np.maximum(mean_sq - mean ** 2, 0.0))
  return np.concatenate([smooth, contrast[..., None]], axis=-1)


def classify_by_example(rgb: np.ndarray, raster: Rasterized, labels: np.ndarray, examples: dict,
                        speck_area: int) -> np.ndarray:
  """Spreads a few example regions per channel over the whole coverage: a
  Gaussian model per class on `pixel_features`, then a majority filter and
  speck removal. Returns per-pixel channel index, -1 for none."""
  features = pixel_features(rgb)
  classes = list(examples)
  models = []
  for name in classes:
    mask = np.isin(labels, [int(r) for r in examples[name]])
    if not mask.any():
      raise SystemExit(f"examples for {name} name no existing region")
    x = features[mask]
    if len(x) < 4 * x.shape[1]:
      raise SystemExit(f"examples for {name} cover only {len(x)} pixels; add regions")
    mean = x.mean(axis=0)
    cov = np.cov(x, rowvar=False) + np.eye(x.shape[1])
    models.append((mean, np.linalg.pinv(cov), np.linalg.slogdet(cov)[1], np.log(len(x))))

  x = features[raster.coverage]
  scores = np.empty((len(x), len(classes)), dtype=np.float32)
  for i, (mean, inv, log_det, log_prior) in enumerate(models):
    d = x - mean
    scores[:, i] = -0.5 * np.einsum("ij,jk,ik->i", d, inv, d) - 0.5 * log_det + log_prior
  best = np.full(labels.shape, -1, dtype=np.int32)
  best[raster.coverage] = scores.argmax(axis=1)

  # the majority filter must not see off-coverage pixels, or thin UV strips vanish
  _, (iy, ix) = ndimage.distance_transform_edt(~raster.coverage, return_indices=True)
  best = ndimage.median_filter(best[iy, ix], size=9)
  best[~raster.coverage] = -1
  for i in range(len(classes)):
    components, count = ndimage.label(best == i)
    areas = ndimage.sum(np.ones_like(best), components, index=np.arange(1, count + 1))
    for small in np.nonzero(areas < speck_area)[0] + 1:
      best[components == small] = -2
  holes = best == -2
  if holes.any():
    _, (iy, ix) = ndimage.distance_transform_edt(holes | ~raster.coverage, return_indices=True)
    best = best[iy, ix]
  best[~raster.coverage] = -1

  channel = np.full(labels.shape, -1, dtype=np.int32)
  for name in classes:
    if name not in CHANNELS and name not in ("none", "auto"):
      raise SystemExit(f"unknown example class {name!r}; use {list(CHANNELS)}, none or auto")
  for i, name in enumerate(classes):
    if name == "auto":
      for index, surface_name in enumerate(raster.surface_names):
        channel[(best == i) & (raster.surface_ids == index)] = PREFIX_CHANNELS[surface_name[:2]]
    elif name != "none":
      channel[best == i] = CHANNELS[name]
  return channel


def hue_mask(rgb: np.ndarray, rule: dict) -> np.ndarray:
  """Pixels whose hue lies within `spread` degrees of `hue` and whose chroma
  exceeds `chroma`, in Lab. Team skins paint their color on top of the shared
  metal, so this finds exactly what their artist meant to be team colored."""
  lab = skcolor.rgb2lab(rgb)
  chroma = np.hypot(lab[..., 1], lab[..., 2])
  hue = np.degrees(np.arctan2(lab[..., 2], lab[..., 1])) % 360.0
  delta = np.abs((hue - float(rule["hue"]) + 180.0) % 360.0 - 180.0)
  return (chroma > float(rule.get("chroma", 25.0))) & (delta < float(rule.get("spread", 30.0)))


def assign(rgb: np.ndarray, raster: Rasterized, labels: np.ndarray, texture_recipe: dict,
           speck_area: int) -> tuple[np.ndarray, np.ndarray]:
  """Applies the recipe: examples first, then surface rules, then region rules,
  then an optional hue mask that clears every pixel outside it. Returns
  per-pixel channel index (-1 for none) and alpha."""
  channel = np.full(labels.shape, -1, dtype=np.int32)
  alpha = np.zeros(labels.shape, dtype=np.float32)

  if "examples" in texture_recipe:
    channel = classify_by_example(rgb, raster, labels, texture_recipe["examples"], speck_area)
    alpha = np.where(channel >= 0, 1.0, 0.0).astype(np.float32)

  for surface_name, rule in texture_recipe.get("surfaces", {}).items():
    if surface_name not in raster.surface_names:
      raise SystemExit(f"recipe names surface {surface_name}, not in {raster.surface_names}")
    name, a = resolve_rule(rule)
    mask = raster.surface_ids == raster.surface_names.index(surface_name)
    channel[mask] = CHANNELS[name] if name else -1
    alpha[mask] = a if name else 0.0

  for region_id, rule in texture_recipe.get("regions", {}).items():
    mask = labels == int(region_id)
    if not mask.any():
      raise SystemExit(f"recipe names region {region_id}, which the segmentation did not produce")
    name, a = resolve_rule(rule)
    channel[mask] = CHANNELS[name] if name else -1
    alpha[mask] = a if name else 0.0

  if "mask" in texture_recipe:
    keep = hue_mask(rgb, texture_recipe["mask"])
    keep = ndimage.binary_closing(keep, iterations=2)
    channel[~keep] = -1
    alpha[~keep] = 0.0

  return channel, alpha


def build_tintmap(rgb: np.ndarray, raster: Rasterized, channel: np.ndarray, alpha: np.ndarray,
                  feather: float, pad: int) -> np.ndarray:
  """Composes the RGBA tintmap: shading in the assigned channel, alpha from the
  recipe, feathered at region borders and padded past UV edges so bilinear
  sampling and mipmaps do not pull in untinted texels at seams."""
  # the pixel's value, not its luma: value >= every channel, so the default color
  # that reproduces the region (mean rgb / mean shade) fits in [0, 1] before scaling
  value = rgb.max(axis=2)

  tint = np.zeros(rgb.shape[:2] + (4,), dtype=np.float32)
  for index in range(3):
    region = channel == index
    if not region.any():
      continue
    # lift dark skins so that team colors read as bright as on the hand-painted sets,
    # but keep the brightest texels below white so the region keeps its shading
    scale = max(1.0, TARGET_SHADE / max(value[region].mean(), 1e-3))
    scale = min(scale, 1.0 / max(np.percentile(value[region], 95), 1e-3))
    tint[..., index] = np.where(region, np.clip(value * scale, 0.0, 1.0), 0.0)
  tint[..., 3] = np.where(channel >= 0, alpha, 0.0)

  # copy shading past the mask border before the alpha blur, or the feathered
  # texels would subtract diffuse without adding any tint back and read as a dark seam
  reach = pad + int(np.ceil(3 * feather))
  if reach > 0:
    _, (iy, ix) = ndimage.distance_transform_edt(channel < 0, return_indices=True)
    band = ndimage.binary_dilation(channel >= 0, iterations=reach) & (channel < 0)
    tint[band, :3] = tint[iy, ix][band, :3]

  if feather > 0:
    tint[..., 3] = ndimage.gaussian_filter(tint[..., 3], feather)
    tint[~raster.coverage & ~ndimage.binary_dilation(raster.coverage, iterations=pad), 3] = 0.0

  return tint


def tint_defaults(rgb: np.ndarray, tint: np.ndarray) -> list[list[float] | None]:
  """Per channel, the least-squares color that reproduces the diffuse map where
  the channel is set, so an untinted player wears the original skin."""
  defaults = []
  for index in range(3):
    weight = tint[..., index] * tint[..., 3]
    if not weight.any():
      defaults.append(None)
      continue
    color = (rgb * weight[..., None]).sum(axis=(0, 1)) / (weight * tint[..., index]).sum()
    defaults.append([float(c) for c in np.clip(color, 0.0, 1.0)])
  return defaults


def write_material(data_root: Path, job: TextureJob, defaults: list[list[float] | None]):
  """Creates or updates `<texture>.mat` with the computed tint defaults. Other
  content of an existing material is preserved."""
  path = data_root / f"{job.rel_path}.mat"
  lines = []
  if path.is_file():
    lines = [l for l in path.read_text().splitlines() if "tintmap." not in l]
  else:
    lines = ["{", f"\tdiffusemap {job.rel_path}"]
    for suffix, key in (("_norm", "normalmap"), ("_spec", "specularmap")):
      if resolve_texture(data_root, job.rel_path + suffix):
        lines.append(f"\t{key} {job.rel_path}{suffix}")
    lines.append("}")

  insert_at = next((i + 1 for i, l in enumerate(lines) if l.strip().startswith("diffusemap")), 1)
  additions = [f"\ttintmap.tint_{name}_default {c[0]:.3f} {c[1]:.3f} {c[2]:.3f}"
               for name, c in zip("rgb", defaults) if c]
  lines[insert_at:insert_at] = additions
  path.write_text("\n".join(lines) + "\n")
  return path


def apply_tint(rgb: np.ndarray, tint: np.ndarray, colors: list[tuple[float, float, float]]) -> np.ndarray:
  out = rgb * (1.0 - tint[..., 3:4])
  for index, color in enumerate(colors):
    out += np.asarray(color, dtype=np.float32) * (tint[..., index] * tint[..., 3])[..., None]
  return np.clip(out, 0.0, 1.0)


def draw_preview(rgb: np.ndarray, tint: np.ndarray, defaults: list, size: int) -> Image.Image:
  """original | .mat defaults | white (today's server default) | red | blue | mixed"""
  white = (1.0, 1.0, 1.0)
  fallback = [tuple(c) if c else (0.0, 0.0, 0.0) for c in defaults]
  panels = [
    rgb,
    apply_tint(rgb, tint, fallback),
    apply_tint(rgb, tint, [white] * 3),
    apply_tint(rgb, tint, [(1.0, 0.0, 0.0)] * 3),
    apply_tint(rgb, tint, [(0.0, 0.0, 1.0)] * 3),
    apply_tint(rgb, tint, [(0.9, 0.7, 0.1), (0.2, 0.5, 0.9), (0.1, 0.8, 0.3)]),
  ]
  height, width = rgb.shape[:2]
  scale = size / max(width, height)
  thumbs = [Image.fromarray((p * 255).astype(np.uint8)).resize((int(width * scale), int(height * scale)),
                                                                Image.LANCZOS) for p in panels]
  strip = Image.new("RGB", (sum(t.width for t in thumbs), thumbs[0].height))
  x = 0
  for thumb in thumbs:
    strip.paste(thumb, (x, 0))
    x += thumb.width
  return strip


def build(args):
  data_root = Path(args.data).expanduser()
  recipe = json.loads(Path(args.recipe).read_text())
  jobs = gather_jobs(data_root, args.model, args.skin)
  preview_dir = Path(args.preview).expanduser() / args.model if args.preview else None
  if preview_dir:
    preview_dir.mkdir(parents=True, exist_ok=True)

  for job in jobs:
    name = texture_name(job)
    texture_recipe = recipe.get("textures", {}).get(name)
    if texture_recipe is None:
      print(f"{job.rel_path}: not in recipe, no tintmap")
      continue

    params = segmentation_params(recipe, name)
    rgb = load_rgb(job.image_path)
    height, width = rgb.shape[:2]
    raster = rasterize(job, width, height)
    labels_path = Path(args.proposals).expanduser() / args.model / f"{name}_labels.png" if args.proposals else None
    if labels_path and labels_path.is_file():
      proposed = json.loads(labels_path.with_name(f"{name}_regions.json").read_text())["segmentation"]
      if {k: float(v) for k, v in proposed.items()} != {k: float(v) for k, v in params.items()}:
        raise SystemExit(f"{name}: proposals were made with {proposed}, recipe has {params}; run propose again")
      labels = np.asarray(Image.open(labels_path)).astype(np.int32)
    else:
      print(f"{job.rel_path}: no proposals, segmenting now; region ids may differ from an earlier overlay")
      labels = segment(rgb, raster, params)
    channel, alpha = assign(rgb, raster, labels, texture_recipe, params["speck_area"])
    tint = build_tintmap(rgb, raster, channel, alpha, args.feather, args.pad)
    defaults = tint_defaults(rgb, tint)

    masked = (tint[..., 3] > 0.5).mean()
    print(f"{job.rel_path}: {masked:.0%} of the texture masked, defaults "
          + ", ".join(f"{n}={'-' if c is None else ' '.join(f'{v:.2f}' for v in c)}" for n, c in zip(CHANNELS, defaults)))

    if preview_dir:
      draw_preview(rgb, tint, defaults, args.preview_size).save(preview_dir / f"{name}_preview.png")

    if args.dry_run:
      continue

    tint_path = data_root / f"{job.rel_path}_tint.png"
    Image.fromarray((tint * 255).round().astype(np.uint8), "RGBA").save(tint_path)
    mat_path = write_material(data_root, job, defaults)
    print(f"  wrote {tint_path.name}, {mat_path.name}")


def main():
  parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
  parser.add_argument("--data", default="/usr/local/share/quetoo/default",
                      help="game data root containing players/ (default: %(default)s)")
  parser.add_argument("--model", required=True, help="player model directory name, e.g. bloodseeker")
  parser.add_argument("--skin", default="default", help="skin name (default: %(default)s)")
  sub = parser.add_subparsers(dest="command", required=True)

  p = sub.add_parser("propose", help="write numbered region overlays for review")
  p.add_argument("--out", required=True, help="directory for overlays and region tables")
  p.add_argument("--recipe", help="take segmentation parameters from this recipe")
  p.set_defaults(func=propose)

  b = sub.add_parser("build", help="build tintmaps and .mat defaults from a recipe")
  b.add_argument("--recipe", required=True, help="recipe JSON")
  b.add_argument("--proposals", help="propose --out directory; its label images are reused when present")
  b.add_argument("--preview", help="directory for preview strips")
  b.add_argument("--preview-size", type=int, default=384, help="preview panel size (default: %(default)s)")
  b.add_argument("--feather", type=float, default=1.0, help="alpha blur sigma in pixels (default: %(default)s)")
  b.add_argument("--pad", type=int, default=4, help="pixels to pad past UV edges (default: %(default)s)")
  b.add_argument("--dry-run", action="store_true", help="render previews only, write nothing to the data root")
  b.set_defaults(func=build)

  w = sub.add_parser("wire", help="draw the UV wireframe of each texture on a transparent image")
  w.add_argument("--out", required=True, help="directory for the <texture>_tris.png files")
  w.set_defaults(func=wire)

  args = parser.parse_args()
  args.func(args)


if __name__ == "__main__":
  sys.exit(main())
