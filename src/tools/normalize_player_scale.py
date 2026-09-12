#!/usr/bin/env python3
"""Computes a player model's visual bounding box in its standing (idle) pose,
and derives the uniform scale + translate needed in world.cfg/link.cfg to
normalize a third-party player model to Quetoo's reference player model,
qforcer (see src/game/common/bg_pmove.c's PM_BOUNDS for the underlying
collision hitbox both are meant to roughly fill).

Two separate measurements are combined:
  - "legs" bounds (lower.md3 alone, LEGS_IDLE pose): used to measure the
    stance footprint and X/Y center. Legs are symmetric and centered on the
    entity origin by Quake III convention, so this is a clean signal --
    unlike the full assembled body, which is skewed left/right by whichever
    arm happens to be posed holding a weapon.
  - "full" bounds (legs + torso + head, positioned via tag_torso/tag_head,
    LEGS_IDLE + TORSO_STAND1/2 pose): used to measure the total standing
    height, feet to head-top.

world.cfg (applied to the root "legs" entity) gets both the translate and
the scale; link.cfg (applied to the "torso"/"head" child entities, in their
own local space before being attached via tag) gets only the same scale, so
the whole hierarchy scales uniformly without perturbing the tag attachments.
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import md3fu

Vec3 = md3fu.Vec3

# Legs and torso animation names used to measure the player's neutral,
# standing size. LEGS_IDLE (standing in place) is used rather than
# LEGS_WALK/RUN/BACK, whose forward/backward stride swings the legs well
# beyond the character's actual footprint -- that's normal Quake III mesh
# behavior (models routinely clip outside their own hitbox mid-animation),
# but it would badly skew a *size* measurement meant to establish scale.
LEGS_ANIMATIONS = ("LEGS_IDLE",)
TORSO_ANIMATIONS = ("TORSO_STAND1", "TORSO_STAND2")

# Some third-party models include tiny degenerate ("marker") surfaces --
# e.g. violator's u_m01..u_m10, q4hybrid's l_hologram/l_explode/l_hole, and
# gladiator's u_backfire/u_quake4ever -- used as effect/attachment anchors
# in whatever mod originally used them. Their vertices are all coincident
# (near-zero extent), so they render nothing, but they can sit far outside
# the actual body and badly skew a bounding-box measurement if not excluded.
# Real (if small) geometry like samjai's teeth also falls under this extent
# threshold, but excluding a few units of teeth from a body-sized bbox is
# harmless.
MARKER_SURFACE_EXTENT = 2.0


def _is_marker_surface(surf: "md3fu.Md3Surface") -> bool:
  if not surf.frames or not surf.frames[0]:
    return False
  verts = surf.frames[0]
  for axis in range(3):
    values = [v[axis] for v in verts]
    if max(values) - min(values) > MARKER_SURFACE_EXTENT:
      return False
  return True


def _drop_marker_surfaces(model: "md3fu.Md3Model", label: str):
  kept = []
  for surf in model.surfaces:
    if _is_marker_surface(surf):
      print(f"  (excluding degenerate marker surface {label}:{surf.name} from bounds)")
    else:
      kept.append(surf)
  model.surfaces = kept

def _bounds_of(mins: list[float], maxs: list[float], point: Vec3):
  for i in range(3):
    mins[i] = min(mins[i], point[i])
    maxs[i] = max(maxs[i], point[i])


class PlayerBounds:

  def __init__(self, model_dir: Path):
    self.model_dir = model_dir
    self.lower = md3fu.read_md3(model_dir / "lower.md3")
    self.upper = md3fu.read_md3(model_dir / "upper.md3")
    self.head = md3fu.read_md3(model_dir / "head.md3")

    _drop_marker_surfaces(self.lower, "lower")
    _drop_marker_surfaces(self.upper, "upper")
    _drop_marker_surfaces(self.head, "head")

    by_name = {a.name: a for a in md3fu.parse_animation_cfg(model_dir / "animation.cfg")}

    # Use only the *first* frame of each named animation, rather than its
    # whole frame range. Even "idle" animations can include significant sway
    # or arm/weapon motion over a long cycle (e.g. q4hybrid's LEGS_IDLE is
    # 631 frames and swings a claw-arm far out at some point in the loop) --
    # exactly the kind of animated drift we already exclude WALK/RUN stride
    # for. The first frame of IDLE/STAND is the model's true rest pose and
    # gives a single, stable, comparable measurement across all models.
    self.legs_frames = set()
    for name in LEGS_ANIMATIONS:
      if name in by_name:
        self.legs_frames.add(by_name[name].first_frame)
    if not self.legs_frames:
      self.legs_frames = {0}

    self.torso_frames = set()
    for name in TORSO_ANIMATIONS:
      if name in by_name:
        self.torso_frames.add(by_name[name].first_frame)
    if not self.torso_frames:
      self.torso_frames = {0}

    self.legs_mins, self.legs_maxs = self._legs_bounds()
    full_mins, full_maxs, head_included = self._full_bounds()
    self.full_mins = full_mins
    self.full_maxs = list(full_maxs)
    self.head_included = head_included

  def _legs_bounds(self) -> tuple[Vec3, Vec3]:
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3

    for lf in self.legs_frames:
      if lf >= self.lower.num_frames:
        continue
      for surf in self.lower.surfaces:
        if lf < len(surf.frames):
          for v in surf.frames[lf]:
            _bounds_of(mins, maxs, v)

    return tuple(mins), tuple(maxs)

  # tag_head origins beyond this are treated as sentinel/placeholder values
  # rather than real positions -- some third-party models (e.g. gladiator,
  # q4hybrid) bake a -8000/-16000 unit Z offset into tag_head for most poses,
  # seemingly hiding the head attachment outside of a few special animations.
  # That's a separate data quality issue in those assets, not something we
  # can fix by scaling, so we fall back to estimating head height instead.
  TAG_HEAD_SANITY_LIMIT = 1000.0

  def _full_bounds(self) -> tuple[Vec3, Vec3, bool]:
    mins = [float("inf")] * 3
    maxs = [float("-inf")] * 3
    torso_top = float("-inf")
    head_included = False

    for lf in self.legs_frames:
      if lf >= self.lower.num_frames:
        continue

      for surf in self.lower.surfaces:
        if lf < len(surf.frames):
          for v in surf.frames[lf]:
            _bounds_of(mins, maxs, v)

      tag_torso = self.lower.tags[lf].get("tag_torso")
      if not tag_torso:
        continue

      for tf in self.torso_frames:
        if tf >= self.upper.num_frames:
          continue

        for surf in self.upper.surfaces:
          if tf < len(surf.frames):
            for v in surf.frames[tf]:
              world_v = md3fu.apply_tag(tag_torso, v)
              _bounds_of(mins, maxs, world_v)
              torso_top = max(torso_top, world_v[2])

        tag_head = self.upper.tags[tf].get("tag_head")
        if not tag_head or abs(tag_head[0][2]) > self.TAG_HEAD_SANITY_LIMIT:
          continue

        head_included = True
        for surf in self.head.surfaces:
          if surf.frames:
            for v in surf.frames[0]:
              _bounds_of(mins, maxs, md3fu.apply_tag(tag_head, md3fu.apply_tag(tag_torso, v)))

    self.torso_top = torso_top
    return tuple(mins), tuple(maxs), head_included

  def estimate_height_if_needed(self, reference: "PlayerBounds"):
    """If this model's tag_head was garbage in every standing-pose frame
    (see TAG_HEAD_SANITY_LIMIT), estimate its true full height from its
    torso-top height using the reference model's own head/torso-top ratio,
    rather than leaving full_maxs.z wildly wrong."""
    if self.head_included:
      return

    if self.torso_top == float("-inf"):
      # No torso surfaces contributed at all (e.g. samjai bakes its entire
      # visible mesh -- body, head, sword, armor -- into lower.md3, leaving
      # upper.md3/head.md3 as empty hierarchy-only placeholders). In that
      # case full_maxs.z was already correctly captured directly from the
      # legs/lower.md3 geometry, so there's nothing to estimate.
      return

    reference_torso_height = reference.torso_top - reference.full_mins[2]
    reference_full_height = reference.full_maxs[2] - reference.full_mins[2]
    ratio = reference_full_height / reference_torso_height

    this_torso_height = self.torso_top - self.full_mins[2]
    estimated_maxs_z = self.full_mins[2] + this_torso_height * ratio

    print(f"  WARNING: {self.model_dir.name}'s tag_head is >{self.TAG_HEAD_SANITY_LIMIT:.0f} units "
          f"off in every standing/stand pose frame (torso-top z={self.torso_top:.2f}); this looks like "
          f"a data bug in upper.md3 itself (head is effectively hidden/mispositioned in-game), "
          f"separate from scale normalization.")

    if estimated_maxs_z <= self.full_maxs[2]:
      # Whatever surfaces we did capture (legs, torso body without a valid
      # head) already reach at least as high as the estimate; trust the
      # real geometry over the estimate.
      return

    print(f"    estimating full height from torso-top instead: "
          f"{self.full_maxs[2]:.2f} -> {estimated_maxs_z:.2f}")

    self.full_maxs[2] = estimated_maxs_z

  @property
  def height(self) -> float:
    return self.full_maxs[2] - self.full_mins[2]

  @property
  def legs_center(self) -> Vec3:
    return tuple((self.legs_mins[i] + self.legs_maxs[i]) / 2 for i in range(3))

  def print_report(self):
    size = tuple(self.full_maxs[i] - self.full_mins[i] for i in range(3))
    print(f"{self.model_dir.name}:")
    print(f"  full mins:   {self.full_mins[0]:8.2f} {self.full_mins[1]:8.2f} {self.full_mins[2]:8.2f}")
    print(f"  full maxs:   {self.full_maxs[0]:8.2f} {self.full_maxs[1]:8.2f} {self.full_maxs[2]:8.2f}")
    print(f"  full size:   {size[0]:8.2f} {size[1]:8.2f} {size[2]:8.2f}")
    print(f"  legs center: {self.legs_center[0]:8.2f} {self.legs_center[1]:8.2f} {self.legs_center[2]:8.2f}")


def compute_normalization(reference: PlayerBounds, model: PlayerBounds) -> tuple[float, Vec3]:
  """Returns (scale, translate) that, applied uniformly (scale around the
  origin, then translate) to `model`, aligns its standing height and X/Y
  stance center to `reference`'s (qforcer's)."""

  scale = reference.height / model.height

  translate = (
    reference.legs_center[0] - model.legs_center[0] * scale,
    reference.legs_center[1] - model.legs_center[1] * scale,
    reference.full_mins[2] - model.full_mins[2] * scale,
  )

  return scale, translate


def write_mesh_config(path: Path, translate: Vec3 | None, scale: float | None):
  lines = []
  if translate is not None:
    lines.append(f'translate "{translate[0]:.3f} {translate[1]:.3f} {translate[2]:.3f}"')
  if scale is not None:
    lines.append(f'scale "{scale:.4f}"')
  path.write_text("\n".join(lines) + "\n")


def main():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("model_dirs", nargs="+", type=Path, help="players/<model> directories")
  parser.add_argument("--reference", type=Path, help="reference model directory (default: qforcer, alongside the first model_dir's players/ root)")
  parser.add_argument("--write", action="store_true", help="write world.cfg/link.cfg into each model directory")
  parser.add_argument("--epsilon", type=float, default=0.02, help="skip writing configs if scale is within this fraction of 1.0 and translate is within 1 unit")
  args = parser.parse_args()

  reference_dir = args.reference
  if reference_dir is None:
    reference_dir = args.model_dirs[0].parent / "qforcer"
  reference = PlayerBounds(reference_dir)

  print("=== reference ===")
  reference.print_report()
  print()

  for model_dir in args.model_dirs:
    model = PlayerBounds(model_dir)
    model.estimate_height_if_needed(reference)
    model.print_report()

    scale, translate = compute_normalization(reference, model)
    print(f"  -> scale={scale:.4f} translate=({translate[0]:.2f}, {translate[1]:.2f}, {translate[2]:.2f})")

    identity = (abs(scale - 1.0) < args.epsilon and all(abs(t) < 1.0 for t in translate))
    if identity:
      print("  (close enough to reference; no config needed)")
    elif args.write:
      write_mesh_config(model_dir / "world.cfg", translate, scale)
      write_mesh_config(model_dir / "link.cfg", None, scale)
      print(f"  wrote {model_dir / 'world.cfg'} and {model_dir / 'link.cfg'}")
    print()


if __name__ == "__main__":
  main()
