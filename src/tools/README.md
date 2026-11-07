# Quetoo Python tools

This directory contains developer-oriented Python tools for content workflows.

## Setup

Setup the Python virtual environment and install dependencies:

```sh
make install-tools
```

Activate the virtual environment in your shell:
```sh
source .venv/bin/activate
```

Run any of the installed commands:

- `ai-profile`
- `matfu`
- `unpak`
- `mdl2obj`
- `md22obj`
- `md32obj`
- `md3fu`
- `tintfu`
- `objfu`
- `skyfu`
- `symbolicate-dmp`
- `verify-projects`

## Tool scripts

- `ai_profile.py`: run headless bot matches and read the game's bot profiler (`g_aiProfile`)
- `matfu.py`: Quetoo material authoring GUI
- `unpak.py`: Quake PAK extraction
- `mdl2obj.py`: Quake MDL v6 to OBJ
- `md22obj.py`: Quake II MD2 to OBJ
- `md32obj.py`: Quake III MD3 to OBJ
- `md3.py`: MD3 reader shared by the player model tools
- `md3fu.py`: MD3 player model animation viewer
- `tintfu.py`: build player skin tintmaps from mesh coverage and a region recipe
- `playerfu.py`: normalize a third-party Quake III player model into Quetoo's
  `players/<name>/` layout
- `objfu.py`: OBJ viewer, muzzle helper and world/link/view configuration editor
- `skyfu.py`: Skybox cubemap packer
- `symbolicate_dmp.py`: Symbolicate a Windows crash dump
- `verify_projects.py`: Check that the three build systems agree

## md32obj

Converts a single-frame MD3 to OBJ. Multi-frame inputs require an explicit,
zero-based pose selection; animation is not retained in an OBJ:

```sh
md32obj pickup.md3 --frame 0 -o tris.obj
```

Some Q2PRO packs name their MD3 replacements `tris.md2`. Check the file magic,
not the extension, before choosing `md32obj` versus `md22obj`.

## objfu

Preview a static OBJ and edit its `model.cfg` in a companion text window:

```sh
objfu models/weapons/quake2_chaingun/tris.obj --config world
objfu models/weapons/quake2_chaingun/tris.obj --config link
objfu models/weapons/quake2_chaingun/tris.obj --config view
objfu 'weapons/quake2_*' --config muzzle
```

`model.cfg` uses flat entity-style blocks. The `type` pair comes first; transforms
are applied in their written order. Omitted placement blocks mean identity.

```text
{
  "type" "world"
  "scale" "0.8"
}
{
  "type" "link"
  "translate" "4 0 -12"
  "scale" "0.65"
}
{
  "type" "view"
  "translate" "9 -6 -32"
  "scale" "0.9"
}
{
  "type" "muzzle"
  "origin" "24 0 20"
}
```

The muzzle is one point in untransformed model space, shared by all placements.
Legacy `world.cfg`, `link.cfg` and `view.cfg` files are no longer loaded.
The text editor shows the complete file; the selected mode controls the preview.

Models may be named by OBJ path, by model directory, or by a quoted glob,
which is matched against model directories under each data root's `models/`:

```sh
objfu '*quake2*' --config world
objfu 'ammo/quake2_*' armor/quake2_body --config world
objfu 'weapons/quake2_*' --config view
```

- `world`: floor at Z=0, a 16-unit grid and a standing Enforcer for scale.
  The item's origin sits 16 units above the floor, where the game rests it, and
  its collision box (`ITEM_BOUNDS` in `g_item.c`) and axes are drawn there.
  **Rest on floor** prepends a translation that seats the transformed mesh's
  bottom within half a unit of the floor without changing its rotation or scale.
  Several models may be edited together in world mode, to compare them with
  each other. Each is placed in a grid with its own collision box, axes and
  label. Click a model, press Tab / Shift+Tab, or use the editor's selector to
  choose which configuration the editor holds. Unsaved edits are kept per file,
  and **Save all** writes every edited file.
  B cycles the boxes between the collision box, each model's transformed
  bounds, and off. The preview window is resizable in every mode.
  Generated translations and muzzle positions default to integers; hand-edited
  integer and floating-point values are both supported.
- `muzzle`: shows the untransformed model with an orbit camera and its muzzle
  marker. Edit the muzzle block's `origin`, orbiting to inspect its placement on
  the barrel from different directions. Tab / PgDn and Shift+Tab / PgUp cycle
  through a batch. Placement transforms do not affect this preview.
- `link`: attaches the mesh to the standing Enforcer's `tag_weapon`, through
  its legs/torso tag hierarchy.
- `view`: fixed first-person camera looking down +X, with the model facing
  away according to its configuration. `--hand right|left|center` selects the
  engine's handed offset (default right); `--fov` matches `cg_fov`'s horizontal
  FOV at 16:9 (default 110). There is no weapon bob or recoil.
  Several models may be given to `link` and `view` as well. They are shown one
  at a time: Tab / PgDn selects the next, Shift+Tab / PgUp the previous, and the
  editor's selector also chooses. In `view`, the mouse wheel also cycles: down
  for the next, up for the previous. Unsaved edits are kept per file, and
  **Save all** writes every edited file.
  The view configuration's Y translation is retained for right hand, negated
  for left hand, and zeroed for center hand, matching the engine.
  A translucent cyan silhouette averages the screen coverage of the existing
  Quetoo weapons; brighter regions are shared by more weapons. It is rebuilt
  from their current meshes and `model.cfg` view blocks on each launch, excludes Quake
  and Quake2 models and the selected model, and follows the selected FOV
  and hand. Press **G** in the preview to hide/show it. No reference assets or
  configurations are modified.

Text edits update the preview immediately; **Save** writes the selected model's
complete `model.cfg`. **Reload** reads external edits. Invalid text is
reported, retains the last valid preview, and cannot be saved. Closing either
window prompts before discarding unsaved edits; externally changed files require
confirmation before overwriting.

Transforms follow the engine's file order, including XYZ-axis `rotate` (not
player pitch/yaw/roll), quoted or unquoted vectors, and transformed `muzzle`.
Drag to orbit, Cmd+drag to pan and scroll to zoom in world/link mode. T toggles
the text overlays in every mode; configuration errors remain visible.
`--player` selects another normalized player directory (default
`players/enforcer`, `default.skin`); `-p` supplies data roots. Previews are unlit diffuse textures, not the game's
PBR renderer.

Without `--config`, the original orbit viewer and muzzle hotkeys are unchanged:
arrows adjust X/Y, Q/E adjust Z, Shift increases the step, R resets, and S writes
the shared muzzle block to `model.cfg`, preserving placement blocks.

## md3fu

Plays back a player model's `legs -> torso -> head` tag hierarchy for a single
named `animation.cfg` sequence. No lighting (flat-shaded or unlit-textured).
Useful for checking whether an animation is baked smoothly into the `.md3`
frames themselves, independent of the game engine's interpolation and
playback logic, and for comparing raw per-frame data against interpolated
playback (which is what Quetoo actually renders).

```sh
md3fu ~/Coding/quetoo-data/target/default/players/gork --anim BOTH_DEATH1
```

- `-l` / `--list`: print the animations parsed from `animation.cfg` and exit
- `-a` / `--anim NAME`: animation to play (default `BOTH_DEATH1`)
- `-s` / `--skin NAME`: `.skin` file to texture the model with (default
  `default`)
- `--no-skin`: start untextured, with flat per-part colors
- `--no-interpolate`: start snapped to whole frames instead of blending
  toward the next one

Animations always loop; there's no "stop at the end" mode -- pause instead.

In-viewer controls:

- drag the left mouse button to orbit, scroll to zoom
- `space`: pause/resume
- `←` / `→`: step one frame back/forward (also pauses; prints the frame
  number to the console)
- `tab` / `shift+tab`: switch to the next/previous animation
- `l`: toggle frame interpolation on/off
- `t`: toggle textures on/off (if a `.skin` was found)
- `w`: toggle wireframe
- `Esc`: quit

## playerfu

Quetoo trims a lot of the mess out of Quake III's player model "spec": one
`.skin` file per variant (not one per body part), lowercase extensionless
texture paths under `players/<name>/`, no nested subfolders. Old
community-made Q3 player models rarely follow any of that. `playerfu`
normalizes a raw model archive -- typically one of the `q3mdl-*.zip` dumps
still floating around the internet -- into Quetoo's layout so it can be
loaded by `md3fu` or dropped straight into `quetoo-data`.

```sh
playerfu ~/Downloads/q3mdl-bloodseeker.zip -o /tmp/normalized
```

- `-o` / `--output DIR`: where to write `players/<name>/` (default: current
  directory)
- `--search-path DIR`: an already-extracted/normalized tree to additionally
  search when a `.skin` file references a texture from a *different* model
  (some packs genuinely borrow art across models -- and across zips). Can be
  given more than once.
- `--keep-scratch`: don't delete the temporary extraction directory

It handles, per input archive:

- one or more levels of nested `.pk3`/`.zip` (skipping obvious `bot-`/`snd-`
  prefixed companion archives that aren't the model itself)
- more than one model per archive (each gets its own `players/<name>/`)
- mixed-case file and directory names, normalized to lowercase on output
- merging separate `lower_*.skin` / `upper_*.skin` / `head_*.skin` files into
  a single `<variant>.skin`, falling back to that part's `default` skin when
  a variant doesn't override it
- resolving each texture reference case-insensitively, honoring an explicit
  `models/players/<other-model>/...` reference (in the same archive, or via
  `--search-path`) rather than guessing from a same-named file that happens
  to live anywhere else
- disambiguating two *different* source textures that would otherwise
  flatten to the same output filename (renaming the second by prefixing its
  source model's name)
- copying each variant's menu-selection icon (vanilla Q3's `icon_<variant>.*`)
  to Quetoo's `<variant>_i.<ext>` convention, falling back to `icon_default.*`
  the same way a variant falls back to the default skin

References it can't resolve -- a typo'd path, a texture the original archive
never actually shipped, an animated-shader name that isn't a real file, or a
vanilla Quake III engine builtin like `textures/common/nodraw` -- are printed
as warnings rather than silently dropped or guessed at; the resulting
`.skin` simply omits that surface.

## verify-projects

Each game and client game module is described three times - in its `Makefile.am`,
in the MSVS project and property sheet, and in the Xcode project - and a local
build on macOS or Linux only exercises the first. This compares all three:

```sh
python3 src/tools/verify_projects.py --verbose
```

It reports a source present in one system and missing from another, a module
building a sibling's manifest, duplicate entries, feature defines that disagree
between a module's three descriptions, and a file in `src/{game,cgame}/common`
that no project references. None of those fail a local build, and the middle two
present at runtime as a network fault or as the wrong game modes in a menu.

It needs no dependencies beyond the standard library, so it runs without the
virtual environment the other tools want.

## matfu

Tool for authoring per-pixel material assets used by Quetoo:
diffuse + normalmap (with packed heightmap in the alpha channel) + specular.

The main entry point is a Tkinter GUI that lets you preview and tweak each
processing stage interactively. The same pipeline is also exposed as a
headless CLI driven by a preset file saved from the GUI.

### Setup

Use the shared setup section at the top of this document.

### Asset naming

Assets live next to each other and share a base name:

```
diffuse.jpg          diffuse texture (sRGB)
diffuse_norm.png     RGB = tangent-space normal, A = heightmap
diffuse_spec.jpg     specular intensity
```

JPG/PNG/TGA are all accepted on input.

### Normalmap convention

Quetoo's renderer expects **DirectX-convention** normalmaps (G channel
points image-down). This is determined by how `Winding_Tangents`
(`src/common/winding.c`) builds the bitangent (`∂P/∂T`) combined
with Quake's V-down texture coordinates.

The GUI's `Source convention` radio (DirectX / OpenGL) tells the tool how
to interpret the file you loaded. Internally everything is processed in
OpenGL convention (Y-up, required by the Frankot-Chellappa heightmap
integration), and the saved output is always converted back to DirectX
before writing.

### GUI

```bash
matfu
# or (direct module path):
python3 src/tools/matfu.py
```

Browse to a texture directory; thumbnails of every diffuse base appear in
a grid. Clicking one opens the editor with sliders for normal smoothing,
heightmap integration, specular generation, etc. Save writes
`*_norm.png` and `*_spec.jpg` next to the diffuse.

Presets can be saved to a JSON file and reused in batch mode or shared
between texture sets.

### Batch / CLI

```bash
matfu batch \
    --preset path/to/preset.json \
    --directory ~/Coding/quetoo-data/target/default/textures/quake \
    --save normal,height,spec

# Or process specific files:
matfu batch \
    --preset preset.json \
    --files texture1_d.jpg texture2_norm.png

# Preview without writing:
matfu batch \
    --preset preset.json -d /textures --dry-run
```

`--save` controls which outputs to write (any combination of
`normal`, `height`, `spec`). `--filter SUBSTR` restricts processing to
bases whose name contains `SUBSTR`.

By default batch mode reprocesses each existing `*_norm.png`. With
`--from-diffuse` it instead generates the normalmap from the diffuse with
the preset's normal strength and frequency settings, exactly as the GUI's
Generate from diffuse followed by Save, so a preset can regenerate a set
of normalmaps without compounding:

```bash
matfu batch --preset preset.json --from-diffuse --save normal \
    --files models/weapons/quake2_*/*_norm.png
```
