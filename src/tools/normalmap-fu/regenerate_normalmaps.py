#!/usr/bin/env python3
"""
Regenerate normalmap RGB channels from the alpha-channel heightmap.

Many texture packs ship normalmaps with broken RGB data (non-normalized
vectors, zero blue channel, swapped axes) but perfectly usable heightmaps
baked into the alpha channel.  This tool reads the heightmap, computes
proper tangent-space normals via Sobel gradients, and writes back a
corrected RGBA normalmap.

Usage:
    # Preview what would be processed
    python regenerate_normalmaps.py /path/to/textures --dry-run

    # Process all *_n.tga files (trak5 naming convention)
    python regenerate_normalmaps.py /path/to/textures --pattern "*_n.tga"

    # Stretch heightmap range first for more pronounced normals
    python regenerate_normalmaps.py /path/to/textures --pattern "*_n.tga" --stretch

    # Control normal intensity (higher = steeper normals)
    python regenerate_normalmaps.py /path/to/textures --pattern "*_n.tga" --strength 2.0

    # Smooth the heightmap before generating normals (reduce noise)
    python regenerate_normalmaps.py /path/to/textures --pattern "*_n.tga" --blur 3
"""

import argparse
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image


def stretch_heightmap(alpha: np.ndarray, percentile: float = 1.0) -> np.ndarray:
    """
    Stretch the heightmap to use the full [0, 255] range.

    Uses percentile clipping to avoid outlier pixels dominating the range.
    """
    lo = np.percentile(alpha, percentile)
    hi = np.percentile(alpha, 100.0 - percentile)
    if hi - lo < 1.0:
        return alpha

    stretched = (alpha.astype(np.float32) - lo) / (hi - lo) * 255.0
    return np.clip(stretched, 0, 255).astype(np.uint8)


def heightmap_to_normals(
    heightmap: np.ndarray,
    strength: float = 1.0,
    blur: int = 0,
) -> np.ndarray:
    """
    Convert a greyscale heightmap to a tangent-space normalmap.

    Parameters
    ----------
    heightmap : uint8 array (H, W)
        Greyscale heightmap where brighter = higher.
    strength : float
        Normal intensity multiplier.  Higher values produce steeper normals
        (more pronounced surface detail).  1.0 is neutral.
    blur : int
        Optional Gaussian blur radius applied to the heightmap before
        computing gradients.  Reduces high-frequency noise.  0 = disabled,
        must be odd if > 0.

    Returns
    -------
    normals : uint8 array (H, W, 3)
        RGB normalmap in tangent space, encoded as [0, 255] where 128 = 0.0.
        Blue channel (Z) is always positive (pointing out of the surface).
    """
    h = heightmap.astype(np.float32) / 255.0

    if blur > 0:
        if blur % 2 == 0:
            blur += 1
        h = cv2.GaussianBlur(h, (blur, blur), 0)

    # Wrap-pad before Sobel so edge gradients are consistent with tiling.
    # ksize=3 Sobel needs only 1px, but 2px keeps things safe and is invisible.
    pad = 2
    h_padded = np.pad(h, pad, mode='wrap')
    dx = cv2.Sobel(h_padded, cv2.CV_32F, 1, 0, ksize=3) * strength
    dy = cv2.Sobel(h_padded, cv2.CV_32F, 0, 1, ksize=3) * strength
    dx = dx[pad:-pad, pad:-pad]
    dy = dy[pad:-pad, pad:-pad]

    # Construct normal vectors: N = normalize(-dx, -dy, 1)
    nx = -dx
    ny = -dy
    nz = np.ones_like(nx)

    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    length = np.maximum(length, 1e-8)

    nx /= length
    ny /= length
    nz /= length

    # Encode [-1, 1] → [0, 255]
    r = ((nx * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    g = ((ny * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)
    b = ((nz * 0.5 + 0.5) * 255.0).clip(0, 255).astype(np.uint8)

    return np.stack([r, g, b], axis=-1)


def analyze_normals(rgb: np.ndarray) -> dict:
    """Compute diagnostic stats for a normalmap's RGB channels."""
    arr = rgb.astype(np.float32) / 255.0 * 2.0 - 1.0
    nx, ny, nz = arr[:, :, 0], arr[:, :, 1], arr[:, :, 2]
    length = np.sqrt(nx * nx + ny * ny + nz * nz)
    z_positive = (nz > 0).sum() / nz.size * 100.0
    return {
        "len_mean": float(length.mean()),
        "len_std": float(length.std()),
        "b_mean": float(rgb[:, :, 2].mean()),
        "z_positive": z_positive,
    }


def process_one(
    path: Path,
    strength: float = 1.0,
    blur: int = 0,
    do_stretch: bool = False,
    stretch_pct: float = 1.0,
    dry_run: bool = False,
) -> bool:
    """Process a single normalmap file.  Returns True if processed."""
    try:
        img = Image.open(path)
        img.load()
    except Exception as exc:
        print(f"  ERROR reading {path.name}: {exc}")
        return False

    if img.mode not in ("RGBA", "LA"):
        print(f"  SKIP {path.name}: no alpha channel")
        return False

    rgba = np.array(img.convert("RGBA"))
    alpha = rgba[:, :, 3]

    # Check that the heightmap has meaningful variation
    alpha_std = float(np.std(alpha))
    if alpha_std < 1.0:
        print(f"  SKIP {path.name}: flat heightmap (std={alpha_std:.2f})")
        return False

    # Analyze existing normals
    old_stats = analyze_normals(rgba[:, :, :3])

    if dry_run:
        print(f"  {path.name}: alpha std={alpha_std:.1f}, "
              f"old normals len={old_stats['len_mean']:.3f}±{old_stats['len_std']:.3f}, "
              f"B_mean={old_stats['b_mean']:.0f}, Z+={old_stats['z_positive']:.0f}%")
        return True

    print(f"  Processing: {path.name}")
    print(f"    Heightmap: min={alpha.min()}, max={alpha.max()}, std={alpha_std:.1f}")
    print(f"    Old normals: len={old_stats['len_mean']:.3f}±{old_stats['len_std']:.3f}, "
          f"B_mean={old_stats['b_mean']:.0f}, Z+={old_stats['z_positive']:.0f}%")

    # Optionally stretch the heightmap range
    if do_stretch:
        alpha = stretch_heightmap(alpha, percentile=stretch_pct)
        print(f"    Stretched: min={alpha.min()}, max={alpha.max()}, "
              f"std={np.std(alpha):.1f}")

    # Generate new normals from heightmap
    normals = heightmap_to_normals(alpha, strength=strength, blur=blur)

    # Analyze new normals
    new_stats = analyze_normals(normals)
    print(f"    New normals: len={new_stats['len_mean']:.3f}±{new_stats['len_std']:.3f}, "
          f"B_mean={new_stats['b_mean']:.0f}, Z+={new_stats['z_positive']:.0f}%")

    # Write back: new RGB normals + original (or stretched) alpha heightmap
    rgba[:, :, :3] = normals
    if do_stretch:
        rgba[:, :, 3] = alpha

    result = Image.fromarray(rgba, mode="RGBA")
    if path.suffix.lower() == ".tga":
        result.save(path, compression="tga_rle")
    else:
        result.save(path)

    print(f"    Saved: {path.name}")
    return True


def main():
    parser = argparse.ArgumentParser(
        description="Regenerate normalmap RGB from alpha-channel heightmaps."
    )
    parser.add_argument("directory", type=Path,
                        help="Directory containing normalmap files")
    parser.add_argument("--pattern", default="*_norm.tga",
                        help="Glob pattern for normalmap files (default: *_norm.tga)")
    parser.add_argument("--strength", type=float, default=1.0,
                        help="Normal intensity multiplier (default: 1.0, higher = steeper)")
    parser.add_argument("--blur", type=int, default=0,
                        help="Gaussian blur radius for heightmap before gradient computation "
                             "(default: 0 = disabled, must be odd if > 0)")
    parser.add_argument("--stretch", action="store_true",
                        help="Stretch heightmap to full [0, 255] range before processing")
    parser.add_argument("--stretch-percentile", type=float, default=1.0,
                        help="Percentile for range stretching clipping (default: 1.0)")
    parser.add_argument("--dry-run", action="store_true",
                        help="Analyze files without modifying them")

    args = parser.parse_args()

    directory = args.directory.expanduser()
    if not directory.is_dir():
        print(f"Error: {directory} is not a directory")
        sys.exit(1)

    files = sorted(directory.rglob(args.pattern))
    if not files:
        print(f"No files matching '{args.pattern}' found in {directory}")
        sys.exit(1)

    print(f"Found {len(files)} files")
    if not args.dry_run:
        print(f"Settings: strength={args.strength}, blur={args.blur}, "
              f"stretch={args.stretch}")
    print()

    count = 0
    for f in files:
        if process_one(f, strength=args.strength, blur=args.blur,
                       do_stretch=args.stretch,
                       stretch_pct=args.stretch_percentile,
                       dry_run=args.dry_run):
            count += 1

    prefix = "Would process" if args.dry_run else "Processed"
    print(f"\n{prefix} {count}/{len(files)} files.")


if __name__ == "__main__":
    main()
