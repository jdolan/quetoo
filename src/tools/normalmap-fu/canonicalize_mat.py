#!/usr/bin/env python3
"""Canonicalize Quetoo .mat files in a directory tree.

Two passes:

1. **Suffix canonicalization.** Quetoo's Cm_ResolveMaterialAsset accepts both
   `normalmap quake/foo` and `normalmap quake/foo_norm` (it tries `_norm`
   first as a fallback). The engine's own writer always emits the full
   suffixed form. Rewrite shorthand to the canonical form whenever the
   suffixed file exists on disk.

2. **Implicit specularmap.** When a .mat names a non-default normalmap (i.e.
   one that doesn't share the .mat's basename), it nearly always wants the
   matching non-default specularmap too. If the .mat has no specularmap
   directive AND the corresponding `<base>_spec.<ext>` exists, add it.

Usage:
    python canonicalize_mat.py /path/to/textures        # dry-run
    python canonicalize_mat.py /path/to/textures --apply
"""

from __future__ import annotations
import argparse
import re
import sys
from pathlib import Path

NORMAL_SUFFIX = "_norm"
SPEC_SUFFIX = "_spec"
NORMAL_EXTS = (".png", ".tga", ".jpg")
SPEC_EXTS = (".jpg", ".png", ".tga")


def textures_root(start: Path) -> Path | None:
  for d in [start, *start.parents]:
    if d.name == "textures":
      return d
  return None


def file_exists(root: Path, rel: str, exts: tuple[str, ...]) -> bool:
  return any((root / (rel + e)).is_file() for e in exts)


def canonical_normal(root: Path, rel: str) -> str | None:
  """Return canonical normalmap rel (with _norm) if a real file exists."""
  if rel.endswith(NORMAL_SUFFIX):
    return rel if file_exists(root, rel, NORMAL_EXTS) else None
  cand = rel + NORMAL_SUFFIX
  if file_exists(root, cand, NORMAL_EXTS):
    return cand
  return rel if file_exists(root, rel, NORMAL_EXTS) else None


def canonical_spec(root: Path, rel: str) -> str | None:
  if rel.endswith(SPEC_SUFFIX):
    return rel if file_exists(root, rel, SPEC_EXTS) else None
  cand = rel + SPEC_SUFFIX
  if file_exists(root, cand, SPEC_EXTS):
    return cand
  return rel if file_exists(root, rel, SPEC_EXTS) else None


def implied_spec(normal_rel: str, root: Path) -> str | None:
  """Given a canonical normalmap rel (ending _norm), return the matching
  spec rel if it exists on disk."""
  if not normal_rel.endswith(NORMAL_SUFFIX):
    return None
  base = normal_rel[: -len(NORMAL_SUFFIX)]
  spec = base + SPEC_SUFFIX
  return spec if file_exists(root, spec, SPEC_EXTS) else None


_DIRECTIVE_RE = re.compile(r"^(\s*)(normalmap|specularmap)(\s+)(\S+)\s*$")


def process_mat(mat_path: Path, root: Path) -> tuple[str, list[str]] | None:
  """Returns (new_text, change_log) if the file would be modified, else None."""
  original = mat_path.read_text()
  lines = original.splitlines(keepends=True)
  changes: list[str] = []

  has_normal_value: str | None = None
  has_spec_idx: int | None = None
  normal_idx: int | None = None

  out_lines = list(lines)

  for i, line in enumerate(out_lines):
    m = _DIRECTIVE_RE.match(line.rstrip("\n"))
    if not m:
      continue
    indent, key, gap, val = m.group(1), m.group(2), m.group(3), m.group(4)
    if key == "normalmap":
      canon = canonical_normal(root, val)
      if canon is None:
        changes.append(f"  ! normalmap {val}: no file found, leaving as-is")
      elif canon != val:
        out_lines[i] = f"{indent}{key}{gap}{canon}\n"
        changes.append(f"  normalmap: {val} -> {canon}")
        val = canon
      has_normal_value = val
      normal_idx = i
    elif key == "specularmap":
      canon = canonical_spec(root, val)
      if canon is None:
        changes.append(f"  ! specularmap {val}: no file found, leaving as-is")
      elif canon != val:
        out_lines[i] = f"{indent}{key}{gap}{canon}\n"
        changes.append(f"  specularmap: {val} -> {canon}")
      has_spec_idx = i

  # Implicit specularmap: non-default normalmap, no specularmap, but a
  # matching _spec file exists.
  if has_normal_value is not None and has_spec_idx is None:
    mat_base = mat_path.stem
    parent_dir = mat_path.parent.name  # e.g. "quake"
    default_normal = f"{parent_dir}/{mat_base}_norm"
    if has_normal_value != default_normal:
      spec = implied_spec(has_normal_value, root)
      if spec is not None:
        # Insert after the normalmap line, preserving its indent.
        norm_line = out_lines[normal_idx]
        m = _DIRECTIVE_RE.match(norm_line.rstrip("\n"))
        indent = m.group(1) if m else "\t"
        new_line = f"{indent}specularmap {spec}\n"
        out_lines.insert(normal_idx + 1, new_line)
        changes.append(f"  + specularmap {spec} (implied by {has_normal_value})")

  new_text = "".join(out_lines)
  if new_text == original:
    return None
  return new_text, changes


def main() -> int:
  ap = argparse.ArgumentParser(description=__doc__,
                               formatter_class=argparse.RawDescriptionHelpFormatter)
  ap.add_argument("directory", help="Directory to scan recursively for .mat files")
  ap.add_argument("--apply", action="store_true",
                  help="Write changes (default is dry-run)")
  args = ap.parse_args()

  d = Path(args.directory).expanduser()
  if not d.is_dir():
    print(f"Not a directory: {d}", file=sys.stderr)
    return 2
  root = textures_root(d)
  if root is None:
    print(f"Could not find a 'textures' ancestor of {d}", file=sys.stderr)
    return 2

  mats = sorted(d.rglob("*.mat"))
  print(f"Scanning {len(mats)} .mat files under {d}")
  print(f"Textures root: {root}")
  print(f"Mode: {'APPLY' if args.apply else 'DRY RUN'}\n")

  total_changed = 0
  for mat in mats:
    result = process_mat(mat, root)
    if result is None:
      continue
    new_text, changes = result
    rel = mat.relative_to(d)
    print(f"{rel}:")
    for c in changes:
      print(c)
    if args.apply:
      mat.write_text(new_text)
    total_changed += 1

  print(f"\n{total_changed} files {'updated' if args.apply else 'would be updated'}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
