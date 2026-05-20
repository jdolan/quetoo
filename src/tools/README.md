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
- `objfu`
- `skyfu`

## Tool scripts

- `matfu.py`: Quetoo material authoring GUI
- `unpak.py`: Quake PAK extraction
- `mdl2obj.py`: Quake MDL v6 to OBJ
- `md22obj.py`: Quake II MD2 to OBJ
- `md32obj.py`: Quake III MD3 to OBJ
- `objfu.py`: OBJ viewer / muzzle helper
- `skyfu.py`: Skybox cubemap packer

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
