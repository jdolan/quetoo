#!/usr/bin/env python3
"""
Batch process normalmap textures to smooth noisy heightmaps in alpha channel.
Preserves major features like bevels while reducing small-scale noise.
"""

import sys
import os
from pathlib import Path
import numpy as np
from PIL import Image
import cv2

def _hairline_fraction(alpha, k, threshold=20):
    """Fraction of pixels that differ from their k×k median by more than threshold."""
    med = cv2.medianBlur(alpha, k)
    diff = np.abs(alpha.astype(np.int32) - med.astype(np.int32))
    return (diff > threshold).sum() / diff.size


def _dominant_feature_scale(alpha):
    """
    Estimate the dominant spatial feature size in pixels via the radial power
    spectrum.  Returns the approximate wavelength (pixels) of the most energetic
    non-DC spatial frequency.
    """
    psd = np.abs(np.fft.fftshift(np.fft.fft2(alpha.astype(np.float32)))) ** 2
    h, w = psd.shape
    cy, cx = h // 2, w // 2
    y, x = np.ogrid[:h, :w]
    r = np.sqrt((x - cx) ** 2 + (y - cy) ** 2).astype(int)
    max_r = min(cx, cy)
    radial = np.zeros(max_r)
    for ri in range(max_r):
        mask = r == ri
        if mask.any():
            radial[ri] = psd[mask].mean()
    # Skip DC (r=0) and the adjacent bin (r=1); find the dominant frequency ring
    peak_r = int(np.argmax(radial[2:])) + 2
    # Wavelength in pixels: image_width / (2 * ring_radius)
    return w / (2.0 * peak_r)


def suggest_params(alpha):
    """
    Analyse a raw heightmap alpha channel and return a dict of suggested
    processing parameters.

    Heuristics
    ----------
    median
        Try k=3 and k=5.  If the outlier fraction *increases* from k=3 to k=5
        the "outliers" are structural features, not noise — cap at k=3 (or 0
        when the texture is already very clean).  If the fraction genuinely
        decreases, keep searching for the smallest k that brings it below 0.5 %.

    bilateral_sigma
        Proportional to the height-range std so textures with broad tonal
        variation get enough smoothing.  Clamped to [30, 150].

    sharpen_sigma
        Derived from the dominant spatial feature scale via the power spectrum.
        sigma ≈ feature_scale / 6, clamped to [2.0, 10.0] and rounded to the
        nearest 0.5 step so it lands on a slider tick.

    sharpen_amount / fine-sharpen
        Left at the global defaults — these are style choices that are hard to
        infer analytically.

    Returns None if the heightmap appears flat (std < 1).
    """
    if np.std(alpha) < 1.0:
        return None

    std = float(np.std(alpha))

    # ── median blur ──────────────────────────────────────────────────────────
    frac3 = _hairline_fraction(alpha, 3)
    frac5 = _hairline_fraction(alpha, 5)

    if frac5 >= frac3:
        # Fraction not shrinking → structural texture, not salt-and-pepper noise.
        # k=3 gives minimal cleanup; 0 if already clean enough.
        median = 3 if frac3 > 0.001 else 0
    else:
        # Genuine noise: find smallest k that brings outliers below 0.5 %.
        median = 3
        for k in [3, 5, 7, 9, 11]:
            if _hairline_fraction(alpha, k) < 0.005:
                median = k
                break
            median = k

    # ── bilateral sigma ───────────────────────────────────────────────────────
    bilateral_sigma = int(np.clip(std * 1.2, 30, 150))

    # ── sharpen sigma (from dominant feature scale) ───────────────────────────
    feat_scale = _dominant_feature_scale(alpha)
    sharpen_sigma_raw = float(np.clip(feat_scale / 6.0, 2.0, 10.0))
    # Round to nearest 0.5 to land on a slider tick
    sharpen_sigma = round(sharpen_sigma_raw * 2.0) / 2.0

    return dict(
        median=median,
        bilateral_d=9,
        bilateral_sigma=bilateral_sigma,
        sharpen_sigma=sharpen_sigma,
        sharpen_amount=2.5,
        sharpen2_sigma=0.0,
        sharpen2_amount=1.0,
    )


def deshadow_heightmap(alpha, light_angle_deg=315, shadow_reach=10, shadow_strength=1.0):
    """
    Remove baked-in directional lighting/shadows from a heightmap.

    For each pixel, looks toward the light source by up to shadow_reach pixels
    and computes a Gaussian-weighted average of upstream brightness.  Where the
    current pixel is darker than that upstream estimate, it is brightened.
    """
    if shadow_strength <= 0 or shadow_reach < 1:
        return alpha

    h, w = alpha.shape
    arr = alpha.astype(np.float32)

    rad = np.radians(light_angle_deg)
    ldx = float(np.cos(rad))
    ldy = float(np.sin(rad))
    sigma = shadow_reach / 2.5

    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing='ij')

    illumination = np.zeros_like(arr)
    total_weight = 0.0

    for i in range(1, shadow_reach + 1):
        weight = float(np.exp(-i * i / (2.0 * sigma * sigma)))
        map_x = np.clip(xx + i * ldx, 0.0, w - 1.0)
        map_y = np.clip(yy + i * ldy, 0.0, h - 1.0)
        sampled = cv2.remap(arr, map_x, map_y, cv2.INTER_LINEAR)
        illumination += weight * sampled
        total_weight += weight

    illumination /= total_weight
    shadow_fill = np.maximum(illumination - arr, 0.0)
    corrected = arr + shadow_strength * shadow_fill
    return np.clip(corrected, 0, 255).astype(np.uint8)


def process_heightmap(alpha_channel, blur_size=5, bilateral_d=9, bilateral_sigma=75,
                      sharpen_sigma=5.0, sharpen_amount=2.5,
                      sharpen2_sigma=0.0, sharpen2_amount=1.0):
    """
    Process heightmap to reduce noise while preserving edges.
    
    Args:
        alpha_channel: numpy array of the alpha channel (heightmap)
        blur_size: size for median blur (removes noise speckles)
        bilateral_d: diameter for bilateral filter
        bilateral_sigma: sigma value for bilateral filter (higher = more smoothing)
        sharpen_sigma: Gaussian sigma for large-scale unsharp mask (0 to disable).
                       Controls the minimum feature size that gets sharpened back —
                       larger values only sharpen broader features.
        sharpen_amount: Strength of the large-scale unsharp mask (higher = crisper large edges)
        sharpen2_sigma: Gaussian sigma for fine-scale unsharp mask (0 to disable).
                        Applied after denoising, so hairlines cannot be re-introduced.
                        Use small values (1–3) to crisp up brick edges, mortar lines, etc.
        sharpen2_amount: Strength of the fine-scale unsharp mask
    
    Returns:
        Processed alpha channel
    """
    # Convert to uint8 if needed
    if alpha_channel.dtype != np.uint8:
        alpha_channel = alpha_channel.astype(np.uint8)
    
    # Step 1: Median filter to remove salt-and-pepper noise (hairline cracks, speckles)
    if blur_size > 0:
        smoothed = cv2.medianBlur(alpha_channel, blur_size)
    else:
        smoothed = alpha_channel
    
    # Step 2: Bilateral filter - smooths remaining variation while somewhat preserving edges
    if bilateral_d > 0:
        smoothed = cv2.bilateralFilter(smoothed, bilateral_d, bilateral_sigma, bilateral_sigma)
    
    # Step 3: Fine-scale unsharp mask to crisp up medium features (brick edges, mortar lines).
    # Applied after denoising, so hairlines removed in steps 1-2 cannot be re-introduced.
    # Use a small sigma (1–3) to target features at the scale of brick edges.
    if sharpen2_amount > 0 and sharpen2_sigma > 0:
        blurred2 = cv2.GaussianBlur(smoothed, (0, 0), sharpen2_sigma)
        sharpened2 = cv2.addWeighted(smoothed, 1.0 + sharpen2_amount, blurred2, -sharpen2_amount, 0)
        smoothed = np.clip(sharpened2, 0, 255).astype(np.uint8)

    # Step 4: Large-radius unsharp mask to restore crisp large features (bevels, grooves).
    # Because hairlines were already removed in steps 1-2, they cannot be re-introduced
    # here — the unsharp mask only amplifies features that survived the smoothing pass.
    # sharpen_sigma controls the scale cutoff: features narrower than ~3*sigma pixels
    # are not enhanced.
    if sharpen_amount > 0 and sharpen_sigma > 0:
        blurred = cv2.GaussianBlur(smoothed, (0, 0), sharpen_sigma)
        sharpened = cv2.addWeighted(smoothed, 1.0 + sharpen_amount, blurred, -sharpen_amount, 0)
        smoothed = np.clip(sharpened, 0, 255).astype(np.uint8)
    
    return smoothed

def process_normalmap(input_path, output_path=None, median_size=5, bilateral_d=9,
                      bilateral_sigma=75, sharpen_sigma=5.0, sharpen_amount=2.5,
                      sharpen2_sigma=0.0, sharpen2_amount=1.0):
    """
    Process a normalmap TGA file to smooth its heightmap alpha channel.
    
    Args:
        input_path: Path to input normalmap file
        output_path: Path to output file (if None, overwrites input)
        median_size: Size of median blur kernel (odd number, 0 to disable)
        bilateral_d: Bilateral filter diameter (0 to disable)
        bilateral_sigma: Bilateral filter sigma value
    """
    if output_path is None:
        output_path = input_path
    
    print(f"Processing: {os.path.basename(input_path)}")
    
    # Load image
    img = Image.open(input_path)
    
    # Check if image has alpha channel
    if img.mode not in ('RGBA', 'LA'):
        print(f"  Warning: {os.path.basename(input_path)} has no alpha channel, skipping")
        return
    
    # Convert to numpy array
    img_array = np.array(img)
    
    # Extract alpha channel (heightmap)
    alpha = img_array[:, :, 3] if img.mode == 'RGBA' else img_array[:, :, 1]
    
    # Check if heightmap has variation (not flat)
    alpha_std = np.std(alpha)
    if alpha_std < 1.0:
        print(f"  Skipping: heightmap appears flat (std={alpha_std:.2f})")
        return
    
    print(f"  Original heightmap - min: {alpha.min()}, max: {alpha.max()}, std: {alpha_std:.2f}")
    
    # Process heightmap
    processed_alpha = process_heightmap(alpha, median_size, bilateral_d, bilateral_sigma,
                                        sharpen_sigma, sharpen_amount,
                                        sharpen2_sigma, sharpen2_amount)
    
    processed_std = np.std(processed_alpha)
    print(f"  Processed heightmap - min: {processed_alpha.min()}, max: {processed_alpha.max()}, std: {processed_std:.2f}")
    
    # Replace alpha channel
    if img.mode == 'RGBA':
        img_array[:, :, 3] = processed_alpha
    else:
        img_array[:, :, 1] = processed_alpha
    
    # Convert back to image and save
    result = Image.fromarray(img_array, mode=img.mode)
    result.save(output_path, compression='tga_rle')
    print(f"  Saved to: {os.path.basename(output_path)}")

def batch_process(directory, pattern='*_norm.tga', median_size=5, bilateral_d=9,
                  bilateral_sigma=75, sharpen_sigma=5.0, sharpen_amount=2.5,
                  sharpen2_sigma=0.0, sharpen2_amount=1.0, dry_run=False):
    """
    Batch process all normalmap files in a directory.
    
    Args:
        directory: Directory containing normalmap files
        pattern: Glob pattern for normalmap files
        median_size: Median blur kernel size
        bilateral_d: Bilateral filter diameter
        bilateral_sigma: Bilateral filter sigma
        dry_run: If True, only list files without processing
    """
    directory = Path(directory).expanduser()
    files = sorted(directory.rglob(pattern))
    
    if not files:
        print(f"No files matching '{pattern}' found in {directory}")
        return
    
    print(f"Found {len(files)} normalmap files")
    
    if dry_run:
        print("\nFiles to process:")
        for f in files:
            print(f"  {f.name}")
        print("\nDry run - no files processed")
        return
    
    print(f"\nProcessing with settings:")
    print(f"  Median blur: {median_size}")
    print(f"  Bilateral filter: d={bilateral_d}, sigma={bilateral_sigma}")
    print(f"  Unsharp mask (large):  sigma={sharpen_sigma}, amount={sharpen_amount}")
    if sharpen2_sigma > 0 and sharpen2_amount > 0:
        print(f"  Unsharp mask (fine):   sigma={sharpen2_sigma}, amount={sharpen2_amount}")
    print()
    
    success = 0
    skipped = 0
    
    for file_path in files:
        try:
            process_normalmap(file_path, median_size=median_size,
                            bilateral_d=bilateral_d, bilateral_sigma=bilateral_sigma,
                            sharpen_sigma=sharpen_sigma, sharpen_amount=sharpen_amount,
                            sharpen2_sigma=sharpen2_sigma, sharpen2_amount=sharpen2_amount)
            success += 1
        except Exception as e:
            print(f"  Error processing {file_path.name}: {e}")
            skipped += 1
        print()
    
    print(f"Complete! Processed: {success}, Skipped: {skipped}")

if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='Clean noisy heightmaps in normalmap textures')
    parser.add_argument('directory', help='Directory containing normalmap files')
    parser.add_argument('--pattern', default='*_norm.tga', help='File pattern (default: *_norm.tga)')
    parser.add_argument('--median', type=int, default=5, help='Median blur size (default: 5, use 0 to disable)')
    parser.add_argument('--bilateral-d', type=int, default=9, help='Bilateral filter diameter (default: 9, use 0 to disable)')
    parser.add_argument('--bilateral-sigma', type=int, default=75, help='Bilateral filter sigma (default: 75, higher = more smoothing)')
    parser.add_argument('--sharpen-sigma', type=float, default=5.0,
                        help='Gaussian sigma for large-scale unsharp mask (default: 5.0, 0 to disable). '
                             'Controls the minimum feature size sharpened back — larger values only '
                             'sharpen broader features, leaving small details smooth.')
    parser.add_argument('--sharpen-amount', type=float, default=2.5,
                        help='Large-scale unsharp mask strength (default: 2.5, higher = crisper large edges)')
    parser.add_argument('--sharpen2-sigma', type=float, default=0.0,
                        help='Gaussian sigma for fine-scale unsharp mask (default: 0 = disabled). '
                             'Use 1–3 to sharpen brick edges and mortar lines on blurry heightmaps. '
                             'Safe to use — applied after denoising so hairlines cannot return.')
    parser.add_argument('--sharpen2-amount', type=float, default=1.0,
                        help='Fine-scale unsharp mask strength (default: 1.0)')
    parser.add_argument('--dry-run', action='store_true', help='List files without processing')
    
    args = parser.parse_args()
    
    batch_process(args.directory, args.pattern, args.median, args.bilateral_d,
                  args.bilateral_sigma, args.sharpen_sigma, args.sharpen_amount,
                  args.sharpen2_sigma, args.sharpen2_amount, args.dry_run)
