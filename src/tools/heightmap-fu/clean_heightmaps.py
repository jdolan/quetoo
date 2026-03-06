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
