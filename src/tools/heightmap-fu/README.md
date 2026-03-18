# Heightmap Fu

Batch processing tool for cleaning noisy heightmaps in normalmap textures.

## Problem

High-resolution diffuse textures often have normalmaps with heightmaps encoded in the alpha channel. When these heightmaps contain too much fine detail (hairline cracks, small surface imperfections), steep parallax mapping effects can make surfaces look like they're covered in marbles instead of having smooth, realistic depth.

## Solution

This tool uses advanced image processing (bilateral filtering + median blur) to smooth out high-frequency noise in heightmaps while preserving the major depth features like bevels, grooves, and intentional surface variation.

**Key technique:** Bilateral filtering smooths similar adjacent pixels while preserving sharp edges, making it perfect for removing texture detail noise while keeping the important geometric features.

## Requirements

```bash
python3 -m venv venv
source venv/bin/activate
pip install numpy pillow opencv-python
```

## Usage

### Basic Usage

```bash
# Activate virtual environment
source /path/to/quetoo/venv/bin/activate

# Process all *_norm.tga files in a directory
python clean_heightmaps.py /path/to/textures
```

### Advanced Usage

```bash
# Custom file pattern
python clean_heightmaps.py /path/to/textures --pattern "*_normal.tga"

# Adjust smoothing strength (higher = more smoothing)
python clean_heightmaps.py /path/to/textures --bilateral-sigma 300

# Adjust median blur for noise removal (higher = more aggressive)
python clean_heightmaps.py /path/to/textures --median 15

# Adjust bilateral filter diameter (higher = larger smoothing area)
python clean_heightmaps.py /path/to/textures --bilateral-d 21

# Aggressive denoising (recommended for very noisy heightmaps)
python clean_heightmaps.py /path/to/textures --bilateral-sigma 300 --median 15 --bilateral-d 21

# Dry run to see what files will be processed
python clean_heightmaps.py /path/to/textures --dry-run
```

## Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `--pattern` | `*_norm.tga` | Glob pattern for normalmap files |
| `--median` | 5 | Median blur kernel size (odd number, 0 to disable) |
| `--bilateral-d` | 9 | Bilateral filter diameter (0 to disable) |
| `--bilateral-sigma` | 75 | Bilateral filter sigma (higher = more smoothing) |
| `--dry-run` | False | List files without processing |

## How It Works

1. **Median Blur**: Removes salt-and-pepper noise and small speckles (like hairline cracks)
2. **Bilateral Filter**: Smooths gradual variations while preserving sharp edges (like bevels)
3. **Preserves**: RGB normalmap data is untouched, only alpha channel (heightmap) is processed

## Tips

- Start with default settings and gradually increase if needed
- Multiple passes can help achieve smoother results
- Higher `--median` values remove finer details (good for hairline cracks)
- Higher `--bilateral-sigma` values create smoother transitions
- Larger `--bilateral-d` affects a wider area around each pixel
- Always keep backups before processing!

## Example Workflow

```bash
# Initial test on one texture
python clean_heightmaps.py /path/to/textures --pattern "metal1_1_norm.tga"

# If still too noisy, increase settings
python clean_heightmaps.py /path/to/textures --bilateral-sigma 200 --median 9 --bilateral-d 15

# Run multiple passes for extreme noise
python clean_heightmaps.py /path/to/textures --bilateral-sigma 300 --median 15 --bilateral-d 21
python clean_heightmaps.py /path/to/textures --bilateral-sigma 300 --median 15 --bilateral-d 21

# Process all textures once satisfied
python clean_heightmaps.py /path/to/textures --bilateral-sigma 300 --median 15 --bilateral-d 21
```

## Output

The script shows statistics for each processed file:
```
Processing: metal1_1_norm.tga
  Original heightmap - min: 129, max: 227, std: 9.19
  Processed heightmap - min: 173, max: 222, std: 5.69
  Saved to: metal1_1_norm.tga
```

Lower standard deviation (std) indicates smoother heightmaps with less noise.

## Technical Details

- Uses OpenCV's `bilateralFilter` for edge-preserving smoothing
- Uses `medianBlur` for impulse noise removal
- Processes only the alpha channel; RGB normalmap data remains intact
- Overwrites original files (make backups first!)
- Skips files without alpha channels or flat heightmaps

## Author

Created for Quetoo texture processing to improve parallax mapping quality.
