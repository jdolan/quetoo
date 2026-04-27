#!/usr/bin/env python3
"""
Pack external heightmaps into normalmap alpha channels, or extract them.

Pack mode (default):
    Given normalmap files, find matching external heightmaps and merge them
    into the normalmap's alpha channel. The external heightmap is deleted
    after a successful pack.

    python pack_heightmaps.py floor1_2_nm.tga wall3_nm.tga
    python pack_heightmaps.py -d /path/to/textures/

Extract mode (-x / --extract):
    Given normalmap files with alpha-channel heightmaps, extract the alpha
    to a standalone greyscale image.

    python pack_heightmaps.py -x floor1_2_nm.tga
    python pack_heightmaps.py -x -d /path/to/textures/

Options:
    -d, --directory DIR   Process all normalmaps in DIR recursively
    -x, --extract         Extract alpha heightmaps instead of packing
    -n, --dry-run         Show what would be done without modifying files
    --no-delete           Don't delete external heightmaps after packing
"""

import argparse
import sys
from pathlib import Path

from PIL import Image
import numpy as np

NORMAL_SUFFIXES = ("_norm",)
HEIGHT_SUFFIXES = ("_h", "_height")
IMAGE_EXTENSIONS = (".tga", ".png", ".jpg", ".jpeg")


def is_normal_map(path: Path) -> bool:
    stem = path.stem.lower()
    return any(stem.endswith(s) for s in NORMAL_SUFFIXES)


def normal_base(path: Path) -> str:
    """Strip the normal map suffix from a filename stem."""
    stem = path.stem
    for suffix in NORMAL_SUFFIXES:
        if stem.lower().endswith(suffix):
            return stem[:len(stem) - len(suffix)]
    return stem


def find_heightmap(normal_path: Path) -> Path | None:
    """Find an external heightmap file matching a normalmap."""
    base = normal_base(normal_path)
    parent = normal_path.parent
    for hsuffix in HEIGHT_SUFFIXES:
        for ext in IMAGE_EXTENSIONS:
            candidate = parent / (base + hsuffix + ext)
            if candidate.exists():
                return candidate
    return None


def has_alpha_heightmap(img: Image.Image) -> bool:
    """Check if an image has a meaningful alpha channel (not flat white)."""
    if img.mode not in ("RGBA", "LA", "PA"):
        return False
    alpha = np.array(img.split()[-1])
    return alpha.min() < 250 and np.std(alpha) >= 2.0


def pack_one(normal_path: Path, dry_run: bool = False,
             delete: bool = True) -> bool:
    """Pack an external heightmap into the normalmap's alpha channel."""
    height_path = find_heightmap(normal_path)
    if height_path is None:
        return False

    try:
        normal_img = Image.open(normal_path)
        normal_img.load()
        height_img = Image.open(height_path)
        height_img.load()
    except Exception as exc:
        print(f"  ERROR reading files: {exc}")
        return False

    # Resize heightmap to match normalmap if needed
    if height_img.size != normal_img.size:
        height_img = height_img.resize(normal_img.size, Image.LANCZOS)

    height_grey = np.array(height_img.convert("L"))

    if dry_run:
        print(f"  WOULD pack {height_path.name} -> {normal_path.name} alpha")
        if delete:
            print(f"  WOULD delete {height_path.name}")
        return True

    # Merge into RGBA
    rgba = normal_img.convert("RGBA")
    arr = np.array(rgba)
    arr[:, :, 3] = height_grey
    result = Image.fromarray(arr, mode="RGBA")

    # Save (TGA with RLE compression)
    if normal_path.suffix.lower() == ".tga":
        result.save(normal_path, compression="tga_rle")
    else:
        result.save(normal_path)

    print(f"  packed {height_path.name} -> {normal_path.name}")

    if delete:
        height_path.unlink()
        print(f"  deleted {height_path.name}")

    return True


def extract_one(normal_path: Path, dry_run: bool = False) -> bool:
    """Extract the alpha channel from a normalmap to a standalone file."""
    try:
        img = Image.open(normal_path)
        img.load()
    except Exception as exc:
        print(f"  ERROR reading {normal_path.name}: {exc}")
        return False

    if not has_alpha_heightmap(img):
        return False

    base = normal_base(normal_path)
    out_path = normal_path.parent / (base + "_h.tga")

    if dry_run:
        print(f"  WOULD extract {normal_path.name} alpha -> {out_path.name}")
        return True

    alpha = img.split()[-1]
    if out_path.suffix.lower() == ".tga":
        alpha.save(out_path, compression="tga_rle")
    else:
        alpha.save(out_path)

    print(f"  extracted {normal_path.name} -> {out_path.name}")
    return True


def collect_normalmaps(paths: list[Path]) -> list[Path]:
    """Expand paths into a list of normalmap files."""
    result = []
    for p in paths:
        if p.is_dir():
            for ext in ("*.tga", "*.png"):
                result.extend(f for f in sorted(p.rglob(ext)) if is_normal_map(f))
        elif p.is_file() and is_normal_map(p):
            result.append(p)
        elif p.is_file():
            print(f"  SKIP {p.name} (not a normalmap)")
    return result


def main():
    parser = argparse.ArgumentParser(
        description="Pack/extract heightmaps from normalmap alpha channels.")
    parser.add_argument("files", nargs="*", type=Path,
                        help="Normalmap files to process")
    parser.add_argument("-d", "--directory", type=Path,
                        help="Process all normalmaps in directory recursively")
    parser.add_argument("-x", "--extract", action="store_true",
                        help="Extract alpha heightmaps instead of packing")
    parser.add_argument("-n", "--dry-run", action="store_true",
                        help="Show what would be done without modifying files")
    parser.add_argument("--no-delete", action="store_true",
                        help="Don't delete external heightmaps after packing")

    args = parser.parse_args()

    paths = list(args.files)
    if args.directory:
        paths.append(args.directory)

    if not paths:
        parser.print_help()
        sys.exit(1)

    normalmaps = collect_normalmaps(paths)
    if not normalmaps:
        print("No normalmap files found.")
        sys.exit(1)

    print(f"Found {len(normalmaps)} normalmaps")

    count = 0
    for nm in normalmaps:
        if args.extract:
            if extract_one(nm, dry_run=args.dry_run):
                count += 1
        else:
            if pack_one(nm, dry_run=args.dry_run, delete=not args.no_delete):
                count += 1

    action = "extracted" if args.extract else "packed"
    prefix = "Would have " if args.dry_run else ""
    print(f"\n{prefix}{action} {count}/{len(normalmaps)} normalmaps.")


if __name__ == "__main__":
    main()
