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

- `matfu`
- `unpak`
- `mdl2obj`
- `md22obj`
- `md32obj`
- `md3fu`
- `objfu`
- `skyfu`
- `symbolicate-dmp`
- `verify-projects`

## Tool scripts

- `matfu.py`: Quetoo material authoring GUI
- `unpak.py`: Quake PAK extraction
- `mdl2obj.py`: Quake MDL v6 to OBJ
- `md22obj.py`: Quake II MD2 to OBJ
- `md32obj.py`: Quake III MD3 to OBJ
- `md3fu.py`: MD3 player model animation viewer
- `objfu.py`: OBJ viewer / muzzle helper
- `skyfu.py`: Skybox cubemap packer
- `symbolicate_dmp.py`: Symbolicate a Windows crash dump
- `verify_projects.py`: Check that the three build systems agree

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
points image-down). This is determined by how `Cm_Tangents`
(`src/collision/cm_polylib.c`) builds the bitangent (`∂P/∂T`) combined
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
