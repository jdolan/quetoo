# Texture & Heightmap Processing

Work done on `quetoo-data` to improve texture quality for parallax occlusion mapping.

---

## Part 1 — JPG → TGA Bulk Conversion

All JPG textures under `target/default/textures/` were converted to TGA and the
originals removed from the repository.

```bash
# Convert
find . -name "*.jpg" | while read f; do
  magick "$f" "${f%.jpg}.tga"
done

# Stage removals and additions
find . -name "*.jpg" -print0 | xargs -0 git rm --cached --
find . -name "*.tga" -print0 | xargs -0 git add --
find . -name "*.jpg" -delete
```

**Result:** 885 JPGs converted. 883 had pre-existing identical TGAs already tracked
(the JPGs were likely derived from them). 2 were genuinely new TGAs. All JPGs deleted
from disk and removed from the git index.

---

## Part 2 — Heightmap Denoising (`clean_heightmaps.py`)

**Tool:** `quetoo/src/tools/heightmap-fu/clean_heightmaps.py`

### Problem

Normalmap textures store a heightmap in the alpha channel for parallax occlusion
mapping. Many of these heightmaps contained high-frequency noise (hairline cracks,
small surface imperfections) that cause steep parallax to make surfaces look like
they're covered in marbles rather than having smooth realistic depth.

### Pipeline

Three processing steps, all operating only on the alpha channel (RGB normalmap data
is untouched):

1. **Median blur** — removes impulse noise and hairline cracks (salt-and-pepper).
   Kernel must be odd; use 0 to disable.

2. **Bilateral filter** — smooths remaining variation while roughly preserving edges.
   The bilateral filter alone produces results that look too much like a Gaussian blur.

3. **Large-scale unsharp mask** — applied to the bilateral result. Because hairlines
   were removed in steps 1–2, they *cannot* be re-introduced here. Only features that
   survived the smoothing pass get sharpened. This restores crispness to bevels,
   grooves, and large depth features. The large sigma means only broad features are
   enhanced.

4. **Fine-scale unsharp mask** — a second, smaller-sigma unsharp mask for heightmaps
   that are already smooth/blurry (no cracks) but need more definition at the
   brick-edge / mortar-line scale. Disabled by default (`sharpen2_sigma=0`).

### Parameters & Defaults

| Parameter          | Default | Description |
|--------------------|---------|-------------|
| `--median`         | 5       | Median blur kernel (odd, 0=off) |
| `--bilateral-d`    | 9       | Bilateral filter diameter (0=off) |
| `--bilateral-sigma`| 75      | Bilateral sigma (higher = more smoothing) |
| `--sharpen-sigma`  | 5.0     | Large-scale unsharp mask Gaussian sigma |
| `--sharpen-amount` | 2.5     | Large-scale unsharp mask strength |
| `--sharpen2-sigma` | 0.0     | Fine-scale unsharp mask sigma (0=off) |
| `--sharpen2-amount`| 1.0     | Fine-scale unsharp mask strength |

### Bug Fixed — Subdirectory Recursion

The original script used `directory.glob(pattern)` which did not recurse into
subdirectories. Changed to `directory.rglob(pattern)`.

### Batch Run on `common/quake` (722 textures)

Settings used for the first full batch pass:

```bash
python clean_heightmaps.py \
  target/default/textures/common/quake \
  --sharpen2-sigma 8 --sharpen2-amount 2.0
# (all other params at defaults)
```

679 of 722 files staged in git (43 were unchanged or had no alpha channel).

### Before/After Comparisons

687 side-by-side PNG comparisons (before = git HEAD alpha, after = processed alpha,
both auto-leveled) were generated and written to:

```
quetoo-data/src/default/textures/common/quake/**/*_norm_compare.png
```

These are not committed to git — for local review only.

---

## Part 3 — Interactive Review GUI (`review_heightmaps.py`)

**Tool:** `quetoo/src/tools/heightmap-fu/review_heightmaps.py`

Because the correct filter settings are texture-dependent (cracked vs. blurry
heightmaps need different treatment), a tkinter GUI was built for interactive
per-texture review.

### Usage

```bash
cd quetoo/src/tools/heightmap-fu
source venv/bin/activate
python review_heightmaps.py /path/to/textures

# With custom pattern
python review_heightmaps.py /path/to/textures --pattern "*_norm.tga"
```

### Features

- **Live before/after preview** — both panels update within ~30 ms of any slider
  change. The "before" is always the pristine original loaded from disk; it never
  gets re-filtered.
- **Auto-leveled display** — both alpha channels are stretched to full range for
  visibility regardless of actual min/max values.
- **std readout** — shows `std before → after` so you can quantify how much the
  filter is doing.
- **Sliders** for all 7 filter parameters.
- **Save** — writes the processed TGA in-place and advances to the next file.
- **Skip** (`→`) — advances without saving.
- **Prev** (`←`) — goes back to the previous file.
- **Revert** (`r`) — resets all sliders to defaults and re-renders.

### Keyboard Shortcuts

| Key     | Action |
|---------|--------|
| `Enter` | Save & advance |
| `→`     | Skip |
| `←`     | Prev |
| `r`     | Revert to defaults |

### Status

As of this session: **122 / 722** textures reviewed interactively.
The remaining 600 have had the batch defaults applied but not individually tuned.

To resume:
```bash
cd quetoo/src/tools/heightmap-fu
source venv/bin/activate
nohup python review_heightmaps.py \
  /path/to/quetoo-data/target/default/textures/common/quake \
  > /tmp/review_heightmaps.log 2>&1 &
```

Note: the tool does not currently track which files have been reviewed — it always
starts from the first file. A future improvement would be to persist per-file state
(applied parameters, reviewed flag) to a sidecar JSON file.

---

## Files Changed / Created

### `quetoo` repo
| File | Change |
|------|--------|
| `src/tools/heightmap-fu/clean_heightmaps.py` | Added large+fine unsharp mask passes; fixed `glob` → `rglob` for recursion |
| `src/tools/heightmap-fu/review_heightmaps.py` | **New** — interactive tkinter review GUI |

### `quetoo-data` repo
| Path | Change |
|------|--------|
| `target/default/textures/**/*.jpg` | Removed (885 files converted to TGA, deleted) |
| `target/default/textures/common/quake/**/*_norm.tga` | 679 heightmaps processed |
| `src/default/textures/common/quake/**/*_compare.png` | 687 before/after PNGs (local, not committed) |
