#!/usr/bin/env python3
"""
Fix normalmap tiling seams by recomputing only the outermost 1-pixel border.

Normalmaps generated without wrap-aware Sobel have incorrect gradients at the
edges because the kernel saw a reflected or zero-padded neighbour instead of
the pixel from the opposite side of the tile.  This script corrects only those
border pixels, leaving the entire interior completely unchanged.

Convention detection: the Y-axis (green channel) direction varies across
texture packs.  We detect it automatically by correlating the generated
interior normals against the originals, then match whichever convention
produces the better fit.

Usage:
    # Fix all *_norm.png in a directory (recursive)
    python fix_tiling_borders.py /path/to/textures

    # Dry-run: report what would change without writing
    python fix_tiling_borders.py /path/to/textures --dry-run

    # Override glob pattern
    python fix_tiling_borders.py /path/to/textures --pattern "*_n.tga"

    # Adjust Sobel strength (should match the value used to generate originals)
    python fix_tiling_borders.py /path/to/textures --strength 1.0
"""

import argparse
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


def _sobel_from_padded(h: np.ndarray, strength: float) -> tuple[np.ndarray, np.ndarray]:
    """Compute wrap-padded Sobel gradients and return (dx, dy) cropped to h's shape."""
    pad = 1
    h_padded = np.pad(h, pad, mode='wrap')
    dx = cv2.Sobel(h_padded, cv2.CV_32F, 1, 0, ksize=3) * strength
    dy = cv2.Sobel(h_padded, cv2.CV_32F, 0, 1, ksize=3) * strength
    return dx[pad:-pad, pad:-pad], dy[pad:-pad, pad:-pad]


def _normals_from_gradients(dx: np.ndarray, dy: np.ndarray, flip_y: bool) -> np.ndarray:
    """Convert Sobel gradients to a uint8 RGB normalmap."""
    nx = -dx
    ny = dy if flip_y else -dy
    nz = np.ones_like(nx)
    length = np.maximum(np.sqrt(nx * nx + ny * ny + nz * nz), 1e-8)
    nx /= length
    ny /= length
    nz /= length
    r = np.clip((nx * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    g = np.clip((ny * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    b = np.clip((nz * 0.5 + 0.5) * 255, 0, 255).astype(np.uint8)
    return np.stack([r, g, b], axis=-1)


def _detect_y_flip(rgba: np.ndarray, h: np.ndarray, strength: float,
                   sample_border: int = 8) -> bool:
    """
    Detect whether the original normalmap uses a flipped Y convention.

    We compute interior normals from the heightmap (without wrap, since
    interior is identical either way) and correlate the generated green
    channel against the original.  Negative correlation → flip_y = True.
    """
    # Use a plain (non-wrap) Sobel for the interior sample — result is
    # identical to the wrap version for all non-border pixels.
    dx = cv2.Sobel(h, cv2.CV_32F, 1, 0, ksize=3) * strength
    dy = cv2.Sobel(h, cv2.CV_32F, 0, 1, ksize=3) * strength

    H, W = h.shape
    b = sample_border
    if H < b * 2 + 4 or W < b * 2 + 4:
        return False  # too small to sample reliably

    # Sample the interior, away from edges that differ between wrap/non-wrap
    dx_s = dx[b:-b, b:-b].ravel()
    dy_s = dy[b:-b, b:-b].ravel()
    orig_g = rgba[b:-b, b:-b, 1].astype(np.float32).ravel()

    # Green channel for both conventions
    ny_normal = -dy_s / np.maximum(np.sqrt(dx_s**2 + dy_s**2 + 1), 1e-8)
    ny_flip   =  dy_s / np.maximum(np.sqrt(dx_s**2 + dy_s**2 + 1), 1e-8)

    gen_g_normal = (ny_normal * 0.5 + 0.5) * 255
    gen_g_flip   = (ny_flip   * 0.5 + 0.5) * 255

    corr_normal = float(np.corrcoef(orig_g, gen_g_normal)[0, 1])
    corr_flip   = float(np.corrcoef(orig_g, gen_g_flip)[0, 1])

    return corr_flip > corr_normal


def fix_borders(path: Path, strength: float = 1.0,
                dry_run: bool = False) -> bool:
    """Fix the 1px tiling border of a single normalmap.  Returns True if changed."""
    try:
        img = Image.open(path)
        img.load()
    except Exception as exc:
        print(f"  ERROR reading {path.name}: {exc}")
        return False

    if img.mode not in ("RGBA", "LA"):
        print(f"  SKIP {path.name}: no alpha channel (mode={img.mode})")
        return False

    rgba = np.array(img.convert("RGBA"))
    alpha = rgba[:, :, 3]

    if float(np.std(alpha)) < 1.0:
        print(f"  SKIP {path.name}: flat heightmap")
        return False

    h = alpha.astype(np.float32) / 255.0
    flip_y = _detect_y_flip(rgba, h, strength)

    dx, dy = _sobel_from_padded(h, strength)
    new_normals = _normals_from_gradients(dx, dy, flip_y)

    # Apply ONLY the outermost 1px border; interior is untouched.
    result = rgba.copy()
    result[0,  :,  :3] = new_normals[0,  :]    # top row
    result[-1, :,  :3] = new_normals[-1, :]    # bottom row
    result[1:-1, 0,  :3] = new_normals[1:-1, 0]   # left col
    result[1:-1, -1, :3] = new_normals[1:-1, -1]  # right col

    changed = not np.array_equal(result[:, :, :3], rgba[:, :, :3])

    if dry_run:
        conv = "flip_y" if flip_y else "normal_y"
        print(f"  {'CHANGED' if changed else 'no-op  '} {path.name}  [{conv}]")
        return changed

    if not changed:
        print(f"  no-op   {path.name}")
        return False

    out = Image.fromarray(result, mode="RGBA")
    suffix = path.suffix.lower()
    if suffix == ".tga":
        out.save(path, compression="tga_rle")
    else:
        out.save(path)

    conv = "flip_y" if flip_y else "normal_y"
    print(f"  fixed   {path.name}  [{conv}]")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Fix normalmap tiling seams by correcting only the 1px border."
    )
    parser.add_argument("directory", type=Path,
                        help="Directory to process (recursive)")
    parser.add_argument("--pattern", default="*_norm.png",
                        help="Glob pattern (default: *_norm.png)")
    parser.add_argument("--strength", type=float, default=1.0,
                        help="Sobel strength used when originals were generated (default: 1.0)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Report changes without writing files")
    args = parser.parse_args()

    directory = args.directory.expanduser()
    if not directory.is_dir():
        print(f"Error: {directory} is not a directory", file=sys.stderr)
        sys.exit(1)

    files = sorted(directory.rglob(args.pattern))
    if not files:
        print(f"No files matching '{args.pattern}' in {directory}", file=sys.stderr)
        sys.exit(1)

    print(f"Found {len(files)} files  [strength={args.strength}"
          f"{', DRY RUN' if args.dry_run else ''}]")
    print()

    changed = sum(
        fix_borders(f, strength=args.strength, dry_run=args.dry_run)
        for f in files
    )

    verb = "Would fix" if args.dry_run else "Fixed"
    print(f"\n{verb} {changed}/{len(files)} files.")


if __name__ == "__main__":
    main()
