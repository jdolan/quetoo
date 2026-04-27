#!/usr/bin/env python3
"""Detect (and optionally fix) OpenGL-style normalmaps in a directory tree.

Quetoo's renderer expects DirectX-convention normalmaps (G channel encodes
"+Y = image-down") because the engine's per-vertex bitangent points in
the image-down direction (Quake-style V-down texture coords). Any
normalmap stored in OpenGL convention (G = image-up) will render with
inverted bump lighting -- highlights and shadows on the opposite side
of every feature.

Heuristic: a normalmap encodes a surface gradient field. If the surface
is z = h(x, y) and the normal is N = (-h_x, -h_y, 1) (OpenGL Y-up):

    h_x = -nx / nz       h_y = -ny / nz       (OpenGL: G = +Y up)
    h_x = -nx / nz       h_y = +ny / nz       (DirectX: G = +Y down)

A true gradient field has zero curl (d(h_y)/dx - d(h_x)/dy == 0). For
each candidate convention we compute the mean |curl|; the convention
with the lower curl is the one the file is actually authored in.

Usage:
  ./venv/bin/python detect_convention.py DIRECTORY [--apply] [--threshold 1.2]
                                         [--limit N] [--quiet]

  --apply       rewrite detected OpenGL files (flip G channel) in place
                so they match the engine's DirectX convention. Alpha
                (heightmap) is preserved.
  --threshold   minimum confidence ratio to act on; default 1.2 (i.e. the
                wrong convention must have at least 20% more curl).
  --limit       only process the first N files (useful for sanity checks).
  --quiet       suppress per-file output, only print the summary.
"""

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def normal_curl(rgb_u8: np.ndarray, *, directx: bool) -> float:
  """Mean |curl| of the gradient field implied by `rgb_u8` under `directx`."""
  v = rgb_u8.astype(np.float32) / 127.5 - 1.0
  nx = v[:, :, 0]
  ny = v[:, :, 1]
  nz = np.maximum(v[:, :, 2], 1e-3)  # avoid divide-by-zero on horizontal normals

  hx = -nx / nz
  hy = (+ny / nz) if directx else (-ny / nz)

  # Central differences (np.gradient handles edges)
  dhx_dy = np.gradient(hx, axis=0)
  dhy_dx = np.gradient(hy, axis=1)
  curl = dhy_dx - dhx_dy

  return float(np.mean(np.abs(curl)))


def detect_one(path: Path) -> tuple[str, float, float]:
  """Return (verdict, curl_gl, curl_dx) for a single normalmap PNG."""
  with Image.open(path) as img:
    img.load()
    rgb = np.array(img.convert("RGB"))

  curl_gl = normal_curl(rgb, directx=False)
  curl_dx = normal_curl(rgb, directx=True)

  if curl_gl < curl_dx:
    return ("OpenGL", curl_gl, curl_dx)
  return ("DirectX", curl_gl, curl_dx)


def flip_g(path: Path) -> None:
  """Rewrite path with G channel inverted; preserve alpha."""
  with Image.open(path) as img:
    img.load()
    arr = np.array(img.convert("RGBA"))
  arr[:, :, 1] = 255 - arr[:, :, 1]
  Image.fromarray(arr, mode="RGBA").save(path, optimize=True)


def main() -> int:
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("directory", type=Path)
  ap.add_argument("--apply", action="store_true",
                  help="Flip G channel of files detected as DirectX.")
  ap.add_argument("--threshold", type=float, default=1.2,
                  help="Min ratio between losing/winning curl to act (default 1.2).")
  ap.add_argument("--limit", type=int, default=0,
                  help="Process at most N files (0 = all).")
  ap.add_argument("--quiet", action="store_true")
  args = ap.parse_args()

  if not args.directory.is_dir():
    print(f"Not a directory: {args.directory}", file=sys.stderr)
    return 2

  files = sorted(args.directory.rglob("*_norm.png"))
  if args.limit > 0:
    files = files[:args.limit]

  if not files:
    print("No *_norm.png files found.")
    return 0

  print(f"Scanning {len(files)} normalmaps under {args.directory}...")
  if args.apply:
    print("  --apply: DirectX files above threshold WILL be rewritten in place.")

  counts = {"OpenGL": 0, "DirectX": 0}
  ambiguous = 0
  flipped = 0
  errors = 0

  for path in files:
    try:
      verdict, gl, dx = detect_one(path)
    except Exception as exc:
      errors += 1
      print(f"  ! {path.relative_to(args.directory)}: {exc}", file=sys.stderr)
      continue

    winner = min(gl, dx)
    loser = max(gl, dx)
    ratio = (loser / winner) if winner > 1e-9 else float("inf")
    confident = ratio >= args.threshold

    if not confident:
      ambiguous += 1
      verdict_str = "ambiguous"
    else:
      counts[verdict] += 1
      verdict_str = verdict

    if not args.quiet:
      rel = path.relative_to(args.directory)
      print(f"  {verdict_str:9s} ratio={ratio:5.2f}  "
            f"gl={gl:.4f} dx={dx:.4f}  {rel}")

    if args.apply and confident and verdict == "OpenGL":
      try:
        flip_g(path)
        flipped += 1
      except Exception as exc:
        errors += 1
        print(f"  ! flip failed for {path}: {exc}", file=sys.stderr)

  print()
  print(f"Detected OpenGL : {counts['OpenGL']}  (wrong for engine)")
  print(f"Detected DirectX: {counts['DirectX']}  (engine-correct)")
  print(f"Ambiguous       : {ambiguous}  (ratio < {args.threshold})")
  if errors:
    print(f"Errors          : {errors}")
  if args.apply:
    print(f"Flipped to DX   : {flipped}")
  else:
    print(f"(rerun with --apply to flip {counts['OpenGL']} OpenGL files to DirectX)")

  return 0


if __name__ == "__main__":
  sys.exit(main())
