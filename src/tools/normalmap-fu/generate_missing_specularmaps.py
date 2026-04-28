#!/usr/bin/env python3
"""Generate specularmaps for textures that don't have one.

Honors .mat references: if a base's .mat names a specularmap (possibly
shared across a variant family), and that file resolves on disk, the
base is considered to already have a specularmap and is left alone.
Otherwise a fresh specularmap is generated from diffuse + heightmap
(loaded from the normalmap's alpha channel when available) and written
to <base>_spec.jpg next to the diffuse.

Usage:
  python generate_missing_specularmaps.py <directory> [--apply]
"""

from __future__ import annotations

import argparse
import importlib.util
import sys
from pathlib import Path

HERE = Path(__file__).parent
MAIN = HERE / "__main__.py"

spec = importlib.util.spec_from_file_location("nmfu_main", MAIN)
nmfu = importlib.util.module_from_spec(spec)
sys.modules["nmfu_main"] = nmfu
spec.loader.exec_module(nmfu)


def main() -> int:
  ap = argparse.ArgumentParser(description=__doc__)
  ap.add_argument("directory", type=Path)
  ap.add_argument("--apply", action="store_true",
                  help="Actually write files (default is dry-run).")
  args = ap.parse_args()

  directory = args.directory.expanduser().resolve()
  if not directory.is_dir():
    print(f"Not a directory: {directory}", file=sys.stderr)
    return 2

  bases = nmfu._enumerate_bases(directory)
  print(f"Scanning {len(bases)} bases under {directory}")

  default_params = {
    "s_input_black":    0.0,
    "s_input_white":    1.0,
    "s_output_black":   0.0,
    "s_output_white":   1.0,
    "s_cavity_strength": 0.5,
  }

  generated = 0
  skipped_have_mat = 0
  skipped_have_sibling = 0
  skipped_no_diffuse = 0
  skipped_no_spec_resolution = 0
  skipped_luma = 0

  for base, parent in sorted(bases):
    # Glowmaps (additive emissives) never need a specularmap.
    if base.endswith("_luma") or base.endswith("_glow"):
      skipped_luma += 1
      continue

    mat_assets = nmfu._resolve_mat_assets(parent, base)

    # Where will the specularmap live?
    if "specularmap" in mat_assets:
      # .mat names one and it resolves on disk -> nothing to do.
      skipped_have_mat += 1
      continue

    # No mat-resolved spec. Default location is <base>_spec.jpg next to base.
    spec_path = parent / (base + nmfu.SPEC_SUFFIX + nmfu.SPEC_EXT)
    # Also accept .png/.tga siblings as "already have one".
    have_sibling = (
      spec_path.is_file()
      or (parent / (base + nmfu.SPEC_SUFFIX + ".png")).is_file()
      or (parent / (base + nmfu.SPEC_SUFFIX + ".tga")).is_file()
    )
    if have_sibling:
      skipped_have_sibling += 1
      continue

    # If the .mat parsed but its specularmap value didn't resolve, that's
    # a separate problem (broken .mat); flag and continue without writing
    # a sibling that would conflict with the .mat directive.
    mat_path = parent / (base + ".mat")
    if mat_path.is_file():
      parsed = nmfu._parse_mat(mat_path)
      if "specularmap" in parsed:
        print(f"  ! {base}: .mat names specularmap '{parsed['specularmap']}' "
              "but it does not resolve; not generating a sibling")
        skipped_no_spec_resolution += 1
        continue

    diffuse_path = nmfu._find_diffuse(parent, base)
    if diffuse_path is None:
      skipped_no_diffuse += 1
      continue

    diffuse = nmfu._load_image_rgb(diffuse_path)
    if diffuse is None:
      skipped_no_diffuse += 1
      continue

    # Heightmap source: prefer the normalmap's alpha channel.
    height = None
    normal_path = mat_assets.get("normalmap") or \
                  (parent / (base + nmfu.NORMAL_SUFFIX + nmfu.NORMAL_EXT))
    if normal_path.is_file():
      loaded = nmfu._load_normal_rgba(normal_path)
      if loaded is not None:
        _, height = loaded

    spec_img = nmfu.process_spec(diffuse, height, default_params)

    rel = spec_path.relative_to(directory)
    if args.apply:
      nmfu._save_spec_jpg(spec_path, spec_img)
      print(f"  + wrote {rel}")
    else:
      print(f"  + would write {rel}")
    generated += 1

  print()
  print(f"Generated:                {generated}")
  print(f"Skipped (mat references): {skipped_have_mat}")
  print(f"Skipped (sibling exists): {skipped_have_sibling}")
  print(f"Skipped (luma/glow):      {skipped_luma}")
  print(f"Skipped (no diffuse):     {skipped_no_diffuse}")
  print(f"Skipped (mat unresolved): {skipped_no_spec_resolution}")
  if not args.apply:
    print("(dry run; pass --apply to write)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
