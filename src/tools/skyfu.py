#!/usr/bin/env python3
"""Compose Quetoo sky cubemap cross images from six face images."""

import argparse
from pathlib import Path

from PIL import Image


# Quetoo's cubemap cross layout in a 4x3 grid.
CELL_MAP = {
  "lf": (0, 1),
  "rt": (2, 1),
  "ft": (3, 1),
  "bk": (1, 1),
  "up": (1, 0),
  "dn": (1, 2),
}


def compose_sky(template: Path, output: Path, faces: dict[str, Path], quality: int) -> None:
  base = Image.open(template).convert("RGB")

  face_size = base.width // 4
  if base.height != face_size * 3:
    raise ValueError(f"Template must be 4x3 cubemap cross, got {base.width}x{base.height}")

  for key, src in faces.items():
    image = Image.open(src).convert("RGB")
    if image.size != (face_size, face_size):
      image = image.resize((face_size, face_size), Image.Resampling.LANCZOS)
    cx, cy = CELL_MAP[key]
    base.paste(image, (cx * face_size, cy * face_size))

  output.parent.mkdir(parents=True, exist_ok=True)
  save_kwargs = {"optimize": True}
  if output.suffix.lower() in {".jpg", ".jpeg"}:
    save_kwargs["quality"] = quality
  base.save(output, **save_kwargs)


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--template", required=True, help="Template cross image path (e.g. sky/template.jpg)")
  parser.add_argument("--output", required=True, help="Output sky image path (e.g. sky/sky4.jpg)")
  parser.add_argument("--bk", required=True, help="Back face image")
  parser.add_argument("--up", required=True, help="Up face image")
  parser.add_argument("--dn", required=True, help="Down face image")
  parser.add_argument("--ft", required=True, help="Front face image")
  parser.add_argument("--lf", required=True, help="Left face image")
  parser.add_argument("--rt", required=True, help="Right face image")
  parser.add_argument("--quality", type=int, default=95, help="JPEG quality for .jpg output (default: 95)")
  return parser.parse_args()


def main() -> int:
  args = parse_args()

  faces = {
    "bk": Path(args.bk).expanduser(),
    "up": Path(args.up).expanduser(),
    "dn": Path(args.dn).expanduser(),
    "ft": Path(args.ft).expanduser(),
    "lf": Path(args.lf).expanduser(),
    "rt": Path(args.rt).expanduser(),
  }

  for key, path in faces.items():
    if not path.is_file():
      raise FileNotFoundError(f"--{key} not found: {path}")

  template = Path(args.template).expanduser()
  if not template.is_file():
    raise FileNotFoundError(f"--template not found: {template}")

  output = Path(args.output).expanduser()
  compose_sky(template, output, faces, quality=max(1, min(100, args.quality)))

  print(f"Wrote {output}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())

