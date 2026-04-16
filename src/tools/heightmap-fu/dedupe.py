#!/usr/bin/env python3
"""
Texture deduplication scanner.

Scans a texture directory recursively, finds diffuse textures that are:
  1. Exact duplicates (identical pixel data)
  2. Color variants (same structure, different hue/tint)

Outputs a summary report grouped by duplicate/variant clusters, with
information useful for consolidation (shared normalmaps via .mat files).

Usage:
    python dedupe.py /path/to/textures/
    python dedupe.py /path/to/textures/ --threshold 0.92
    python dedupe.py /path/to/textures/ --output report.json
"""

import argparse
import hashlib
import json
import sys
from collections import defaultdict
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

# ──────────────────────────────────────────────────────────────────────
# Configuration
# ──────────────────────────────────────────────────────────────────────

NORMAL_SUFFIXES = ("_norm",)
SKIP_SUFFIXES = NORMAL_SUFFIXES + (
    "_h", "_height", "_fx", "_glow", "_spec", "_d",
)

DIFFUSE_EXTENSIONS = {".tga", ".png"}

# Structural similarity threshold for "color variant" detection.
# 1.0 = identical structure, 0.0 = completely different.
DEFAULT_VARIANT_THRESHOLD = 0.90

# Resize target for perceptual hashing / comparison (keeps things fast).
COMPARE_SIZE = 64


# ──────────────────────────────────────────────────────────────────────
# Helpers
# ──────────────────────────────────────────────────────────────────────

def _is_diffuse(path: Path) -> bool:
    """Return True if this looks like a diffuse texture (not a normalmap, etc.)."""
    if path.suffix.lower() not in DIFFUSE_EXTENSIONS:
        return False
    stem = path.stem.lower()
    return not any(stem.endswith(s) for s in SKIP_SUFFIXES)


def _find_normalmap(path: Path) -> Path | None:
    """Find the normalmap counterpart for a diffuse texture."""
    stem = path.stem
    # Strip optional _d suffix
    if stem.lower().endswith("_d"):
        stem = stem[:-2]
    parent = path.parent
    for nsuf in NORMAL_SUFFIXES:
        for ext in (".tga", ".png"):
            candidate = parent / (stem + nsuf + ext)
            if candidate.exists():
                return candidate
    return None


def _find_matfile(path: Path) -> Path | None:
    """Find the .mat file for a texture."""
    mat = path.with_suffix(".mat")
    return mat if mat.exists() else None


def _parse_mat_normalmap(mat_path: Path) -> str | None:
    """Extract the normalmap reference from a .mat file."""
    try:
        text = mat_path.read_text()
        for line in text.splitlines():
            stripped = line.strip()
            if stripped.startswith("normalmap "):
                return stripped.split(None, 1)[1]
    except Exception:
        pass
    return None


def _pixel_hash(img_array: np.ndarray) -> str:
    """SHA256 hash of raw pixel data for exact duplicate detection."""
    return hashlib.sha256(img_array.tobytes()).hexdigest()


def _structural_descriptor(img_array: np.ndarray) -> np.ndarray:
    """
    Compute a grayscale structural descriptor: luminance edges at a fixed size.
    This captures the geometric structure regardless of color.
    """
    # Convert to grayscale
    if img_array.ndim == 3 and img_array.shape[2] >= 3:
        gray = cv2.cvtColor(img_array[:, :, :3], cv2.COLOR_RGB2GRAY)
    else:
        gray = img_array

    # Resize to fixed comparison size
    gray = cv2.resize(gray, (COMPARE_SIZE, COMPARE_SIZE), interpolation=cv2.INTER_AREA)

    # Compute edges (captures structure, invariant to color)
    edges = cv2.Canny(gray, 50, 150)
    return edges


def _structure_similarity(desc_a: np.ndarray, desc_b: np.ndarray) -> float:
    """
    Compare two structural descriptors. Returns 0–1 similarity.
    Uses normalized cross-correlation of edge maps.
    """
    a = desc_a.astype(np.float64).ravel()
    b = desc_b.astype(np.float64).ravel()

    norm_a = np.linalg.norm(a)
    norm_b = np.linalg.norm(b)

    if norm_a < 1e-6 or norm_b < 1e-6:
        # Both nearly blank → similar; one blank → dissimilar
        return 1.0 if (norm_a < 1e-6 and norm_b < 1e-6) else 0.0

    return float(np.dot(a, b) / (norm_a * norm_b))


def _luminance_similarity(img_a: np.ndarray, img_b: np.ndarray) -> float:
    """
    Compare two images by resized grayscale SSIM-like correlation.
    Complements edge-based comparison for textures with subtle differences.
    """
    def _to_gray_resized(img):
        if img.ndim == 3 and img.shape[2] >= 3:
            gray = cv2.cvtColor(img[:, :, :3], cv2.COLOR_RGB2GRAY)
        else:
            gray = img
        return cv2.resize(gray, (COMPARE_SIZE, COMPARE_SIZE),
                          interpolation=cv2.INTER_AREA).astype(np.float64)

    a = _to_gray_resized(img_a).ravel()
    b = _to_gray_resized(img_b).ravel()

    norm_a = np.linalg.norm(a - a.mean())
    norm_b = np.linalg.norm(b - b.mean())

    if norm_a < 1e-6 or norm_b < 1e-6:
        return 1.0 if (norm_a < 1e-6 and norm_b < 1e-6) else 0.0

    return float(np.dot(a - a.mean(), b - b.mean()) / (norm_a * norm_b))


def _color_distance(img_a: np.ndarray, img_b: np.ndarray) -> float:
    """
    Mean color distance in LAB space. Small distance = color variant.
    Returns distance in [0, ~100+] range.
    """
    def _mean_lab(img):
        rgb = cv2.resize(img[:, :, :3], (32, 32), interpolation=cv2.INTER_AREA)
        lab = cv2.cvtColor(rgb, cv2.COLOR_RGB2LAB).astype(np.float64)
        return lab.mean(axis=(0, 1))

    lab_a = _mean_lab(img_a)
    lab_b = _mean_lab(img_b)
    return float(np.linalg.norm(lab_a - lab_b))


# ──────────────────────────────────────────────────────────────────────
# Scanning
# ──────────────────────────────────────────────────────────────────────

class TextureInfo:
    __slots__ = ("path", "rel_path", "size_wh", "pixel_hash",
                 "struct_desc", "img_array", "normalmap", "mat_normalmap")

    def __init__(self, path: Path, base_dir: Path):
        self.path = path
        self.rel_path = path.relative_to(base_dir)
        self.normalmap: Path | None = None
        self.mat_normalmap: str | None = None

        img = Image.open(path)
        img.load()
        rgb = np.array(img.convert("RGB"))
        self.img_array = rgb
        self.size_wh = (rgb.shape[1], rgb.shape[0])
        self.pixel_hash = _pixel_hash(rgb)
        self.struct_desc = _structural_descriptor(rgb)

        # Find associated normalmap
        nm = _find_normalmap(path)
        self.normalmap = nm

        # Check .mat file for normalmap override
        mat = _find_matfile(path)
        if mat:
            self.mat_normalmap = _parse_mat_normalmap(mat)


def scan_textures(base_dir: Path, verbose: bool = False) -> list[TextureInfo]:
    """Scan directory for diffuse textures and load metadata."""
    files = sorted(f for f in base_dir.rglob("*")
                   if f.is_file() and _is_diffuse(f))

    if verbose:
        print(f"Found {len(files)} diffuse textures")

    textures = []
    for i, f in enumerate(files):
        if verbose and (i + 1) % 100 == 0:
            print(f"  Loading {i + 1}/{len(files)}...")
        try:
            textures.append(TextureInfo(f, base_dir))
        except Exception as exc:
            print(f"  WARNING: Failed to load {f}: {exc}", file=sys.stderr)

    return textures


def find_duplicates(textures: list[TextureInfo]) -> list[list[TextureInfo]]:
    """Group textures by identical pixel data."""
    by_hash: dict[str, list[TextureInfo]] = defaultdict(list)
    for t in textures:
        by_hash[t.pixel_hash].append(t)

    return [group for group in by_hash.values() if len(group) > 1]


def find_variants(textures: list[TextureInfo],
                  threshold: float = DEFAULT_VARIANT_THRESHOLD,
                  verbose: bool = False) -> list[dict]:
    """
    Find groups of textures that are structural variants (same geometry,
    different color). Excludes exact duplicates (handled separately).
    """
    # Group by size first (variants must be same resolution)
    by_size: dict[tuple[int, int], list[TextureInfo]] = defaultdict(list)
    for t in textures:
        by_size[t.size_wh].append(t)

    # Track which textures have been assigned to a cluster
    assigned: set[str] = set()
    # Track exact duplicate hashes so we skip those pairs
    dup_hashes: set[str] = set()
    for t in textures:
        if sum(1 for t2 in textures if t2.pixel_hash == t.pixel_hash) > 1:
            dup_hashes.add(t.pixel_hash)

    clusters: list[dict] = []

    for size, group in sorted(by_size.items()):
        if len(group) < 2:
            continue

        if verbose:
            print(f"  Comparing {len(group)} textures at {size[0]}×{size[1]}...")

        # Pairwise structural comparison within each size group
        n = len(group)
        for i in range(n):
            if str(group[i].path) in assigned:
                continue

            cluster_members = [group[i]]

            for j in range(i + 1, n):
                if str(group[j].path) in assigned:
                    continue

                # Skip if they're exact duplicates (different cluster type)
                if group[i].pixel_hash == group[j].pixel_hash:
                    continue

                struct_sim = _structure_similarity(
                    group[i].struct_desc, group[j].struct_desc)
                lum_sim = _luminance_similarity(
                    group[i].img_array, group[j].img_array)

                # Combined similarity: weight structure more heavily
                combined = 0.6 * struct_sim + 0.4 * lum_sim

                if combined >= threshold:
                    cluster_members.append(group[j])

            if len(cluster_members) > 1:
                # Compute pairwise color distances for the report
                color_dists = []
                for a_idx in range(len(cluster_members)):
                    for b_idx in range(a_idx + 1, len(cluster_members)):
                        cd = _color_distance(cluster_members[a_idx].img_array,
                                             cluster_members[b_idx].img_array)
                        color_dists.append(cd)

                cluster = {
                    "members": cluster_members,
                    "mean_color_distance": float(np.mean(color_dists)),
                    "size": size,
                }
                clusters.append(cluster)

                for m in cluster_members:
                    assigned.add(str(m.path))

    return clusters


# ──────────────────────────────────────────────────────────────────────
# Reporting
# ──────────────────────────────────────────────────────────────────────

def _normalmap_info(t: TextureInfo, base_dir: Path) -> str:
    """Describe the normalmap situation for a texture."""
    if t.mat_normalmap:
        return f"mat:{t.mat_normalmap}"
    if t.normalmap:
        return str(t.normalmap.relative_to(base_dir))
    return "none"


def print_report(duplicates: list[list[TextureInfo]],
                 variants: list[dict],
                 base_dir: Path):
    """Print a human-readable report."""

    print("=" * 72)
    print("TEXTURE DEDUPLICATION REPORT")
    print("=" * 72)

    # ── Exact duplicates ──
    print(f"\n{'─' * 72}")
    print(f"EXACT DUPLICATES: {len(duplicates)} groups")
    print(f"{'─' * 72}")

    if not duplicates:
        print("  (none found)")

    for i, group in enumerate(duplicates, 1):
        size = group[0].size_wh
        print(f"\n  Group {i} ({size[0]}×{size[1]}, {len(group)} copies):")
        for t in group:
            nm_info = _normalmap_info(t, base_dir)
            print(f"    {t.rel_path}  [nm: {nm_info}]")

    # ── Color variants ──
    print(f"\n{'─' * 72}")
    print(f"COLOR VARIANTS: {len(variants)} groups")
    print(f"{'─' * 72}")

    if not variants:
        print("  (none found)")

    for i, cluster in enumerate(variants, 1):
        members = cluster["members"]
        size = cluster["size"]
        cd = cluster["mean_color_distance"]
        print(f"\n  Group {i} ({size[0]}×{size[1]}, {len(members)} variants, "
              f"mean color distance: {cd:.1f}):")
        for t in members:
            nm_info = _normalmap_info(t, base_dir)
            print(f"    {t.rel_path}  [nm: {nm_info}]")

        # Identify normalmaps in use
        nms = set()
        for t in members:
            if t.mat_normalmap:
                nms.add(f"mat:{t.mat_normalmap}")
            elif t.normalmap:
                nms.add(str(t.normalmap.relative_to(base_dir)))
        if len(nms) > 1:
            print(f"    ⚠ Multiple normalmaps — candidates for consolidation:")
            for nm in sorted(nms):
                print(f"      • {nm}")
        elif len(nms) == 1:
            print(f"    ✓ Already sharing normalmap: {nms.pop()}")
        else:
            print(f"    — No normalmaps found")

    # ── Summary ──
    total_dup_files = sum(len(g) - 1 for g in duplicates)
    total_variant_groups = len(variants)
    total_variant_files = sum(len(c["members"]) for c in variants)

    print(f"\n{'─' * 72}")
    print("SUMMARY")
    print(f"{'─' * 72}")
    print(f"  Exact duplicate groups:  {len(duplicates)} "
          f"({total_dup_files} files removable)")
    print(f"  Color variant groups:    {total_variant_groups} "
          f"({total_variant_files} files, normalmaps consolidatable)")
    print()


def save_json_report(duplicates: list[list[TextureInfo]],
                     variants: list[dict],
                     base_dir: Path,
                     output_path: Path):
    """Save a machine-readable JSON report."""
    report = {
        "base_dir": str(base_dir),
        "exact_duplicates": [],
        "color_variants": [],
    }

    for group in duplicates:
        report["exact_duplicates"].append({
            "pixel_hash": group[0].pixel_hash,
            "size": list(group[0].size_wh),
            "files": [
                {
                    "path": str(t.rel_path),
                    "normalmap": str(t.normalmap.relative_to(base_dir)) if t.normalmap else None,
                    "mat_normalmap": t.mat_normalmap,
                }
                for t in group
            ],
        })

    for cluster in variants:
        report["color_variants"].append({
            "size": list(cluster["size"]),
            "mean_color_distance": cluster["mean_color_distance"],
            "files": [
                {
                    "path": str(t.rel_path),
                    "normalmap": str(t.normalmap.relative_to(base_dir)) if t.normalmap else None,
                    "mat_normalmap": t.mat_normalmap,
                }
                for t in cluster["members"]
            ],
        })

    output_path.write_text(json.dumps(report, indent=2))
    print(f"JSON report saved: {output_path}")


# ──────────────────────────────────────────────────────────────────────
# Entry point
# ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Scan textures for duplicates and color variants.")
    parser.add_argument("directory", type=Path,
                        help="Root texture directory to scan")
    parser.add_argument("--threshold", type=float,
                        default=DEFAULT_VARIANT_THRESHOLD,
                        help=f"Structural similarity threshold for variant "
                             f"detection (default: {DEFAULT_VARIANT_THRESHOLD})")
    parser.add_argument("--output", "-o", type=Path, default=None,
                        help="Save JSON report to this file")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Show progress during scan")

    args = parser.parse_args()

    if not args.directory.is_dir():
        print(f"Error: {args.directory} is not a directory", file=sys.stderr)
        sys.exit(1)

    base_dir = args.directory.resolve()

    print(f"Scanning {base_dir} ...")
    textures = scan_textures(base_dir, verbose=args.verbose)
    print(f"Loaded {len(textures)} diffuse textures.\n")

    print("Finding exact duplicates...")
    duplicates = find_duplicates(textures)

    print("Finding color variants...")
    variants = find_variants(textures, threshold=args.threshold,
                             verbose=args.verbose)

    print_report(duplicates, variants, base_dir)

    if args.output:
        save_json_report(duplicates, variants, base_dir, args.output)


if __name__ == "__main__":
    main()
