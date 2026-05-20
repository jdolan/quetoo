#!/usr/bin/env python3
"""unpak: unpack Quake/Quake2 .pak archives with unzip-like syntax."""

import argparse
import fnmatch
import os
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


PAK_MAGIC = b"PACK"
DIR_ENTRY_SIZE = 64


@dataclass
class PakEntry:
  name: str
  offset: int
  size: int


def read_pak(path: Path) -> tuple[bytes, list[PakEntry]]:
  data = path.read_bytes()
  if len(data) < 12:
    raise ValueError("file too small for PAK header")

  magic, dir_offset, dir_length = struct.unpack_from("<4sii", data, 0)
  if magic != PAK_MAGIC:
    raise ValueError("not a Quake/Quake2 PAK (bad magic)")
  if dir_length < 0 or dir_offset < 0:
    raise ValueError("invalid directory metadata")
  if dir_offset + dir_length > len(data):
    raise ValueError("directory extends beyond end of file")
  if dir_length % DIR_ENTRY_SIZE != 0:
    raise ValueError("directory length is not a multiple of 64")

  entries: list[PakEntry] = []
  count = dir_length // DIR_ENTRY_SIZE
  for i in range(count):
    off = dir_offset + i * DIR_ENTRY_SIZE
    raw_name, file_offset, file_size = struct.unpack_from("<56sii", data, off)
    name = raw_name.split(b"\x00", 1)[0].decode("utf-8", errors="replace")
    if not name:
      continue
    if file_offset < 0 or file_size < 0 or file_offset + file_size > len(data):
      raise ValueError(f"invalid entry bounds: {name}")
    entries.append(PakEntry(name=name, offset=file_offset, size=file_size))

  return data, entries


def select_entries(entries: list[PakEntry], includes: list[str], excludes: list[str]) -> list[PakEntry]:
  if includes:
    selected = [e for e in entries if any(fnmatch.fnmatch(e.name, pat) for pat in includes)]
  else:
    selected = list(entries)

  if excludes:
    selected = [e for e in selected if not any(fnmatch.fnmatch(e.name, pat) for pat in excludes)]

  return selected


def safe_join(root: Path, rel: str) -> Path:
  candidate = (root / rel).resolve()
  root_resolved = root.resolve()
  try:
    candidate.relative_to(root_resolved)
  except ValueError:
    raise ValueError(f"unsafe entry path: {rel}")
  return candidate


def list_entries(entries: list[PakEntry], pak_path: Path):
  total = sum(e.size for e in entries)
  print(f"Archive:  {pak_path}")
  print("  Length      Name")
  print("---------  ------------------------------")
  for e in entries:
    print(f"{e.size:9d}  {e.name}")
  print("---------  ------------------------------")
  print(f"{total:9d}  {len(entries)} file(s)")


def test_entries(entries: list[PakEntry], quiet: bool = False):
  if not quiet:
    print("testing archive entries...")
  for e in entries:
    if not quiet:
      print(f"OK  {e.name}")
  if not quiet:
    print(f"tested: {len(entries)} file(s)")


def extract_entries(
  data: bytes,
  entries: list[PakEntry],
  output_dir: Path,
  overwrite: bool,
  never_overwrite: bool,
  quiet: bool,
) -> int:
  extracted = 0
  for e in entries:
    out_path = safe_join(output_dir, e.name)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    if out_path.exists():
      if never_overwrite:
        if not quiet:
          print(f"skipping: {e.name} (exists)")
        continue
      if not overwrite:
        if not quiet:
          print(f"skipping: {e.name} (exists; use -o to overwrite)")
        continue

    out_path.write_bytes(data[e.offset:e.offset + e.size])
    extracted += 1
    if not quiet:
      print(f"extracting: {e.name}")
  return extracted


def build_parser() -> argparse.ArgumentParser:
  parser = argparse.ArgumentParser(
    prog="unpak",
    description="Extract Quake/Quake2 PAK archives (unzip-style usage).",
  )
  parser.add_argument("-l", "--list", action="store_true", help="list archive contents")
  parser.add_argument("-t", "--test", action="store_true", help="test archive integrity")
  parser.add_argument("-d", "--dir", default=".", help="extract into directory")
  parser.add_argument("-x", "--exclude", nargs="+", default=[], help="exclude patterns")
  parser.add_argument("-o", "--overwrite", action="store_true", help="overwrite existing files")
  parser.add_argument("-n", "--never-overwrite", action="store_true", help="never overwrite files")
  parser.add_argument("-q", "--quiet", action="store_true", help="quiet mode")
  parser.add_argument("pakfile", help="PAK archive")
  parser.add_argument("members", nargs="*", help="member patterns to extract/list")
  return parser


def main() -> int:
  parser = build_parser()
  args = parser.parse_args()

  if args.overwrite and args.never_overwrite:
    print("error: -o and -n are mutually exclusive", file=sys.stderr)
    return 2

  pak_path = Path(args.pakfile).expanduser().resolve()
  if not pak_path.is_file():
    print(f"error: not found: {pak_path}", file=sys.stderr)
    return 2

  try:
    data, entries = read_pak(pak_path)
  except Exception as e:
    print(f"error: {e}", file=sys.stderr)
    return 1

  selected = select_entries(entries, args.members, args.exclude)
  if not selected:
    if not args.quiet:
      print("warning: no matching entries")
    return 0

  if args.list:
    list_entries(selected, pak_path)
    return 0

  if args.test:
    test_entries(selected, quiet=args.quiet)
    return 0

  out_dir = Path(args.dir).expanduser()
  out_dir.mkdir(parents=True, exist_ok=True)
  try:
    count = extract_entries(
      data,
      selected,
      out_dir,
      overwrite=args.overwrite,
      never_overwrite=args.never_overwrite,
      quiet=args.quiet,
    )
  except Exception as e:
    print(f"error: {e}", file=sys.stderr)
    return 1

  if not args.quiet:
    print(f"extracted: {count} file(s)")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())

