#!/usr/bin/env python3
"""Normalizes a raw Quake III player model archive -- as commonly distributed
by q3mdl.org and similar sites -- into a clean, Quetoo-ready `players/<name>/`
directory.

These old community releases are a wild-west of conventions that Quetoo
doesn't share: mixed-case filenames and directory names, a `models/` prefix
Quetoo's `players/`-relative paths don't use, and one `.skin` file *per mesh
part per variant* (`lower_red.skin`, `upper_red.skin`, `head_red.skin`)
instead of Quetoo's one-file-covers-every-surface convention (see e.g.
quetoo-data's players/gork/default.skin). The zip itself is often a Matryoshka
of nested .pk3 archives too -- the model proper, plus separate bot AI,
footstep sound, and weapon-effect-patch .pk3s bundled alongside it.

This tool extracts all of that (recursively, skipping the irrelevant
bot/sound/patch archives), merges each variant's per-part .skin files into a
single Quetoo-style .skin, rewrites texture paths to Quetoo's extensionless
`players/<name>/<texture>` form, and copies only the files actually
referenced -- so the output is immediately usable with md3fu.py, and a
reasonable starting point for a quetoo-data pull request.
"""

import argparse
import re
import shutil
import tempfile
import zipfile
from pathlib import Path

ARCHIVE_EXTS = {".zip", ".pk3"}

# Archives whose *names* strongly suggest they're not player model geometry
# (bot AI scripts, footstep sounds, weapon-effect patches) -- skip extracting
# these so the output isn't cluttered with irrelevant junk.
SKIP_NAME_HINTS = ("bot-", "bots-", "snd-", "sound-")

MD3_PARTS = ("lower", "upper", "head")

# vanilla Q3's per-variant skin-selection menu icon: `icon_red.tga`,
# `icon_default.tga`, etc. Quetoo looks these up itself (not via the .skin
# file) as `players/<name>/<variant>_i.<ext>` -- see cg_client.c and
# ScoreView.c.
ICON_RE = re.compile(r"^icon[-_]?(.*)\.(tga|jpg|jpeg|png|svg)$", re.IGNORECASE)

# `lower_red.skin` -> part="lower", variant="red". A bare `lower.skin` (no
# variant suffix) is treated as that part's "default" variant.
SKIN_RE = re.compile(r"^(lower|upper|head)[-_]?(.*)\.skin$", re.IGNORECASE)


def should_skip_archive(path: Path) -> bool:
  return path.name.lower().startswith(SKIP_NAME_HINTS)


def extract_recursive(archive: Path, dest: Path, depth: int = 0, seen: set = None):
  """Extracts `archive` into `dest`, then recurses into any nested
  .zip/.pk3 files it contains (to a sibling dir named after the nested
  archive), up to a sane depth to guard against accidental cycles."""
  if seen is None:
    seen = set()
  key = archive.resolve()
  if key in seen or depth > 4:
    return
  seen.add(key)

  dest.mkdir(parents=True, exist_ok=True)
  try:
    with zipfile.ZipFile(archive) as zf:
      zf.extractall(dest)
  except zipfile.BadZipFile:
    print(f"  ! not a valid zip, skipping: {archive}")
    return

  for nested in dest.rglob("*"):
    if nested.is_file() and nested.suffix.lower() in ARCHIVE_EXTS:
      if should_skip_archive(nested):
        continue
      nested_dest = nested.parent / f"_extracted_{nested.stem}"
      extract_recursive(nested, nested_dest, depth + 1, seen)


def find_model_dirs(root: Path):
  """Yields every directory under `root` named "players" (case-insensitively,
  with a parent named "models") whose children include an actual player
  model directory -- i.e. one containing at least one .md3 file -- skipping
  empty placeholder/readme-only stubs."""
  for players_dir in root.rglob("*"):
    if not players_dir.is_dir() or players_dir.name.lower() != "players":
      continue
    if players_dir.parent.name.lower() != "models":
      continue
    for model_dir in players_dir.iterdir():
      if model_dir.is_dir() and any(f.suffix.lower() == ".md3" for f in model_dir.iterdir() if f.is_file()):
        yield model_dir


def find_ci(directory: Path, filename: str) -> Path | None:
  """Case-insensitively finds a file named `filename` directly inside `directory`."""
  if not directory or not directory.is_dir():
    return None
  for f in directory.iterdir():
    if f.is_file() and f.name.lower() == filename.lower():
      return f
  return None


def parse_skin_file(path: Path) -> dict[str, str]:
  """Parses a .skin file: `surface_name,relative/path/no/extension` per line
  (comments via `//`; blank paths, as on tag_* lines, are skipped)."""
  mapping: dict[str, str] = {}
  for raw_line in path.read_text(errors="replace").splitlines():
    line = raw_line.split("//")[0].strip()
    if not line or "," not in line:
      continue
    name, _, rel_path = line.partition(",")
    name, rel_path = name.strip(), rel_path.strip()
    # some packs (e.g. Gaunt/Gammy) quote the path: l_legs,"models/.../x.tga"
    if len(rel_path) >= 2 and rel_path[0] == '"' and rel_path[-1] == '"':
      rel_path = rel_path[1:-1].strip()
    if name and rel_path:
      mapping[name] = rel_path
  return mapping


def resolve_texture(rel_path: str, model_dir: Path, scratch_root: Path,
                     search_paths: list[Path]) -> Path | None:
  """Case-insensitively locates the texture file a .skin line points at,
  handling old-Q3 quirks: mixed case, a `models/` prefix Quetoo doesn't use,
  and genuine cross-model references -- some packs share art between
  releases (e.g. one model's skin borrowing another bundled model's head
  texture), or between entirely separate zips (via `search_paths`)."""
  posix = rel_path.replace("\\", "/")
  parts = posix.split("/")
  basename = parts[-1]
  ref_model = parts[-2] if len(parts) >= 2 else None

  # If the reference doesn't explicitly name a *different* model (either no
  # path at all, or it points back at this same model's own directory), look
  # there first -- overwhelmingly the common case. Crucially, we do NOT do
  # this when ref_model names another model: two unrelated models in the same
  # pack can each happen to ship a same-named file (e.g. samjai's own
  # h_blue.tga is a completely different texture from the
  # models/players/magdalena/h_blue.tga its head_blue.skin actually asks
  # for), and silently preferring the local coincidence over honoring the
  # author's explicit path would produce a wrong-but-present texture, which
  # is worse than an honest "unresolved" warning.
  same_model = ref_model is None or ref_model.lower() == model_dir.name.lower()
  if same_model:
    hit = find_ci(model_dir, basename)
    if hit:
      return hit

  # The reference explicitly names another model -- look for a directory with
  # that exact name anywhere in this zip's extraction (e.g. Neptune's skin
  # referencing flayer/, bundled alongside it in the same zip).
  if ref_model and not same_model:
    for candidate in scratch_root.rglob("*"):
      if candidate.is_dir() and candidate.name.lower() == ref_model.lower():
        hit = find_ci(candidate, basename)
        if hit:
          return hit

  # extra search paths the caller explicitly opted into (e.g. another
  # already-normalized pack, for references crossing *zip* boundaries),
  # honoring the referenced model name the same way. We do NOT fall back to a
  # bare basename search across a whole search path either: it's likely to
  # contain other, unrelated models (potentially even this very one) that
  # coincidentally share a filename, reintroducing the same silent-wrong-match
  # risk step 1 exists to avoid. If `ref_model` names a directory we can't
  # find (in this zip or in any search path), or it's a bare filename that
  # simply isn't present locally, that's a genuinely unresolvable/broken
  # reference in the original release -- report it, don't guess.
  if ref_model:
    for search_path in search_paths:
      for candidate in search_path.rglob("*"):
        if candidate.is_dir() and candidate.name.lower() == ref_model.lower():
          hit = find_ci(candidate, basename)
          if hit:
            return hit

  return None


def normalize_model(model_dir: Path, scratch_root: Path, output_root: Path,
                     search_paths: list[Path]):
  """Normalizes one old-style model directory into `output_root/players/<name>/`,
  Quetoo-ready. Returns (out_dir, variants_written, warnings)."""
  name = model_dir.name.lower()
  out_dir = output_root / "players" / name
  out_dir.mkdir(parents=True, exist_ok=True)

  warnings = []

  cfg = find_ci(model_dir, "animation.cfg")
  if cfg:
    shutil.copy2(cfg, out_dir / "animation.cfg")
  else:
    warnings.append("no animation.cfg found")

  for part in MD3_PARTS:
    f = find_ci(model_dir, f"{part}.md3")
    if f:
      shutil.copy2(f, out_dir / f"{part}.md3")
    elif part != "head":  # a head is common but optional, even in vanilla Q3
      warnings.append(f"no {part}.md3 found")

  # Group this model's .skin files by variant, tracking which mesh part each
  # came from so we can merge them.
  skins_by_variant: dict[str, dict[str, Path]] = {}
  for f in model_dir.iterdir():
    if not f.is_file():
      continue
    m = SKIN_RE.match(f.name)
    if not m:
      continue
    part = m.group(1).lower()
    variant = (m.group(2) or "default").strip("-_").lower() or "default"
    skins_by_variant.setdefault(variant, {})[part] = f

  default_part_files = skins_by_variant.get("default", {})

  # vanilla Q3's per-variant menu icon (`icon_red.tga`, `icon_default.tga`,
  # ...), grouped the same way as skins_by_variant.
  icons_by_variant: dict[str, Path] = {}
  for f in model_dir.iterdir():
    if not f.is_file():
      continue
    m = ICON_RE.match(f.name)
    if m:
      variant = (m.group(1) or "default").strip("-_").lower() or "default"
      icons_by_variant[variant] = f

  # Tracks dest basename (lowercase, no dir) -> resolved source path, across
  # every variant for this model. Two source files that happen to share a
  # basename but aren't actually the same file (e.g. samjai's own
  # h_blue.tga vs. the *different* h_blue.tga its head_blue.skin borrows
  # from magdalena) are a real thing in these packs -- Quetoo's flat
  # per-model directory has no subfolders to keep them apart, so we
  # disambiguate the second one instead of silently letting it clobber/alias
  # the first.
  copied: dict[str, Path] = {}

  def copy_texture(tex: Path, dest_basename: str | None = None) -> str:
    resolved = tex.resolve()
    dest_name = (dest_basename or tex.name).lower()
    existing = copied.get(dest_name)
    if existing == resolved:
      return Path(dest_name).stem
    if existing is not None:
      disambiguated, n = f"{tex.parent.name.lower()}_{dest_name}", 1
      while copied.get(disambiguated) not in (None, resolved):
        n += 1
        disambiguated = f"{tex.parent.name.lower()}{n}_{dest_name}"
      warnings.append(f"texture name collision, '{tex.parent.name}/{tex.name}' renamed to '{disambiguated}'")
      dest_name = disambiguated
      existing = copied.get(dest_name)
      if existing == resolved:
        return Path(dest_name).stem
    shutil.copy2(tex, out_dir / dest_name)
    copied[dest_name] = resolved
    return Path(dest_name).stem

  written_variants = []
  for variant, part_files in sorted(skins_by_variant.items()):
    merged: dict[str, str] = {}
    for part in MD3_PARTS:
      # Fall back to that part's "default" skin if this variant doesn't
      # override it (common: a "red" skin only re-textures the torso).
      f = part_files.get(part) or default_part_files.get(part)
      if not f:
        continue
      for surf_name, rel_path in parse_skin_file(f).items():
        tex = resolve_texture(rel_path, model_dir, scratch_root, search_paths)
        if not tex:
          warnings.append(f"{variant}.skin: unresolved texture for '{surf_name}': {rel_path}")
          continue
        merged[surf_name] = f"players/{name}/{copy_texture(tex)}"

    if not merged:
      continue

    # Copy this variant's menu icon too, if there is one (falling back to
    # the "default" icon the same way we fall back to the default skin).
    icon = icons_by_variant.get(variant) or icons_by_variant.get("default")
    if icon:
      copy_texture(icon, dest_basename=f"{variant}_i{icon.suffix.lower()}")
    else:
      warnings.append(f"{variant}.skin: no menu icon (icon_{variant}.* or icon_default.*) found")

    with (out_dir / f"{variant}.skin").open("w") as fh:
      for surf_name, tex_path in merged.items():
        fh.write(f"{surf_name},{tex_path}\n")
    written_variants.append(variant)

  if not written_variants:
    warnings.append("no usable .skin variants found (model will be untextured)")

  return out_dir, written_variants, warnings


def main():
  parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
  parser.add_argument("archive", type=Path, help="The raw q3mdl-*.zip (or .pk3) to normalize")
  parser.add_argument("-o", "--output", type=Path, default=Path("."),
                       help="Directory under which to write players/<name>/ (default: cwd)")
  parser.add_argument("--search-path", action="append", type=Path, default=[], dest="search_paths",
                       help="Extra directory to search for textures referenced across zips "
                            "(e.g. another already-normalized pack); repeatable")
  parser.add_argument("--keep-scratch", action="store_true",
                       help="Don't delete the temporary extraction directory on exit")
  args = parser.parse_args()

  scratch_ctx = tempfile.TemporaryDirectory(prefix="playerfu-")
  scratch_root = Path(scratch_ctx.name)

  print(f"extracting {args.archive} ...")
  extract_recursive(args.archive, scratch_root)

  model_dirs = list(find_model_dirs(scratch_root))
  if not model_dirs:
    raise SystemExit(f"No models/players/*/*.md3 found in {args.archive}")

  for model_dir in model_dirs:
    out_dir, variants, warnings = normalize_model(
      model_dir, scratch_root, args.output, args.search_paths)
    variant_list = ", ".join(variants) if variants else "none"
    print(f"{model_dir.name} -> {out_dir}  ({len(variants)} skin(s): {variant_list})")
    for w in warnings:
      print(f"  ! {w}")

  if args.keep_scratch:
    print(f"scratch extraction kept at {scratch_root}")
  else:
    scratch_ctx.cleanup()


if __name__ == "__main__":
  main()
