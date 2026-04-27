# normalmap-fu

Tool for authoring per-pixel material assets used by Quetoo:
diffuse + normalmap (with packed heightmap in the alpha channel) + specular.

The main entry point is a Tkinter GUI that lets you preview and tweak each
processing stage interactively. The same pipeline is also exposed as a
headless CLI driven by a preset file saved from the GUI.

## Setup

All Python tools in `src/tools/` share a single virtual environment at the
repo root:

```bash
python3 -m venv venv
source venv/bin/activate
pip install numpy pillow opencv-python scikit-image scipy
```

## Asset naming

Assets live next to each other and share a base name:

```
diffuse.jpg          diffuse texture (sRGB)
diffuse_norm.png     RGB = tangent-space normal, A = heightmap
diffuse_spec.jpg     specular intensity
```

JPG/PNG/TGA are all accepted on input.

## Normalmap convention

Quetoo's renderer expects **DirectX-convention** normalmaps (G channel
points image-down). This is determined by how `Cm_Tangents`
(`src/collision/cm_polylib.c`) builds the bitangent (`∂P/∂T`) combined
with Quake's V-down texture coordinates.

The GUI's `Source convention` radio (DirectX / OpenGL) tells the tool how
to interpret the file you loaded. Internally everything is processed in
OpenGL convention (Y-up, required by the Frankot-Chellappa heightmap
integration), and the saved output is always converted back to DirectX
before writing.

## GUI

```bash
python -m src.tools.normalmap-fu       # from repo root
# or:
python src/tools/normalmap-fu/__main__.py gui
```

Browse to a texture directory; thumbnails of every diffuse base appear in
a grid. Clicking one opens the editor with sliders for normal smoothing,
heightmap integration, specular generation, etc. Save writes
`*_norm.png` and `*_spec.jpg` next to the diffuse.

Presets can be saved to a JSON file and reused in batch mode or shared
between texture sets.

## Batch / CLI

```bash
python src/tools/normalmap-fu/__main__.py batch \
    --preset path/to/preset.json \
    --directory ~/Coding/quetoo-data/target/default/textures/quake \
    --save normal,height,spec

# Or process specific files:
python src/tools/normalmap-fu/__main__.py batch \
    --preset preset.json \
    --files texture1_d.jpg texture2_norm.png

# Preview without writing:
python src/tools/normalmap-fu/__main__.py batch \
    --preset preset.json -d /textures --dry-run
```

`--save` controls which outputs to write (any combination of
`normal`, `height`, `spec`). `--filter SUBSTR` restricts processing to
bases whose name contains `SUBSTR`.

## One-off scripts

The other `*.py` files in this directory are standalone utilities used
during the Quake-remake texture import:

| Script | Purpose |
|---|---|
| `detect_convention.py` | Scan a directory and flag (or `--apply`-fix) normalmaps stored in OpenGL convention so they match the engine's DirectX expectation. Uses a curl-of-implied-gradient heuristic. |
| `clean_heightmaps.py` | Smooth noisy alpha-channel heightmaps with bilateral + median filtering. Predates the GUI; kept for batch CLI use. |
| `regenerate_normalmaps.py` | Recompute normal RGB from the alpha heightmap when the existing RGB data is corrupt. |
| `pack_heightmaps.py` | Move standalone `*_h.*` heightmaps into the alpha channel of the matching normalmap. |
| `correct_diffuse.py` | Retinex-style diffuse lighting removal, optionally guided by a height-derived shadow mask. |
| `extract_quake_textures.py` | Extract wall textures from Quake 1 `.pak`/`.bsp` files for use as color reference. |
| `dedupe.py` / `prune_duplicates.py` | Find and remove identical or near-identical textures across a tree. |
| `variant_review.py` | GUI for reviewing color-variant consolidations produced by `dedupe.py`. |

Each script has its own `--help`.
