#!/usr/bin/env python3
"""
Diffuse lighting removal, albedo extraction, and normal-map-based height integration.

Pipeline:
  1. integrate_normals()   — Frankot-Chellappa FFT integration of normal map → height field
  2. correct_diffuse()     — Retinex illumination removal, optionally targeted to
                             geometrically-shadowed areas derived from the height field
"""

import numpy as np
import cv2
from PIL import Image
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))
from clean_heightmaps import deshadow_heightmap


def integrate_normals(norm_arr, flip_y=False):
    """
    Reconstruct a height field from a tangent-space normal map using
    Frankot-Chellappa frequency-domain integration.

    This is mathematically the correct heightmap for the geometry described by
    the normal map — far superior to luminance derived from a lit diffuse.

    Args:
        norm_arr:  uint8 (H, W, 4) RGBA — R=nx, G=ny, B=nz
        flip_y:    Negate the Y gradient before integration.  Use if the
                   resulting height field looks inverted vertically (DirectX
                   vs OpenGL normal map convention difference).

    Returns:
        float32 (H, W) in [0, 1], where 1 = highest point.
    """
    h, w = norm_arr.shape[:2]

    nx = (norm_arr[:, :, 0].astype(np.float32) - 128.0) / 128.0
    ny = (norm_arr[:, :, 1].astype(np.float32) - 128.0) / 128.0
    nz = np.maximum(norm_arr[:, :, 2].astype(np.float32) / 255.0, 0.01)

    if flip_y:
        ny = -ny

    # Surface gradients: n = normalize(-p, -q, 1)  →  p = -nx/nz, q = -ny/nz
    p = -nx / nz   # ∂h/∂x
    q = -ny / nz   # ∂h/∂y

    # Frankot-Chellappa least-squares integration in the Fourier domain:
    #   j·u·H = P,  j·v·H = Q  (overdetermined)
    #   H(u,v) = -(j·u·P + j·v·Q) / (u² + v²)
    P = np.fft.fft2(p)
    Q = np.fft.fft2(q)

    u = np.fft.fftfreq(w)   # ∈ [-0.5, 0.5]
    v = np.fft.fftfreq(h)
    U, V = np.meshgrid(u, v)

    denom = U * U + V * V
    denom[0, 0] = 1.0          # DC: set to 0 (height offset is arbitrary)

    Z = (-1j * U * P - 1j * V * Q) / denom
    Z[0, 0] = 0.0 + 0j

    height = np.fft.ifft2(Z).real.astype(np.float32)

    hmin, hmax = float(height.min()), float(height.max())
    if hmax > hmin:
        height = (height - hmin) / (hmax - hmin)

    return height


def height_from_diffuse(rgb):
    """
    Derive an approximate height field from a diffuse texture.

    Simply uses normalized luminance — bright = high, dark = low.
    This is a better estimate than Frankot-Chellappa integration of luminance
    Sobel gradients, which produces radial FFT ringing artifacts on
    high-contrast centre-lit textures.

    Returns float32 (H, W) in [0, 1].
    """
    f = rgb.astype(np.float32) / 255.0
    lum = (0.2126 * f[:, :, 0]
         + 0.7152 * f[:, :, 1]
         + 0.0722 * f[:, :, 2])
    lmin, lmax = float(lum.min()), float(lum.max())
    if lmax > lmin:
        lum = (lum - lmin) / (lmax - lmin)
    return lum.astype(np.float32)


def _light_dirs(shape, light_x, light_y):
    """
    Per-pixel unit direction vectors pointing FROM each pixel TOWARD the light.

    When the light is near an image edge, all vectors converge on a single
    direction (equivalent to a directional light).  When the light is inside
    the image, each pixel gets a unique vector — correctly modelling a point
    light source such as a centre-origin glow or candle.

    Returns: ldx, ldy  — both float32 (H, W), unit length.
    """
    h, w = shape
    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing='ij')
    dx = float(light_x) - xx
    dy = float(light_y) - yy
    dist = np.sqrt(dx * dx + dy * dy)
    dist = np.maximum(dist, 0.5)   # avoid divide-by-zero at the source
    return dx / dist, dy / dist


def _shadow_mask(height_field, light_x, light_y, shadow_reach):
    """
    Ray-march from each pixel toward the light source along the height field.
    Returns float32 [0,1]: 1 = fully occluded by taller upstream geometry.
    """
    if shadow_reach < 1:
        return np.zeros_like(height_field)

    h, w = height_field.shape
    ldx, ldy = _light_dirs((h, w), light_x, light_y)

    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing='ij')

    max_upstream = np.zeros_like(height_field)
    for i in range(1, shadow_reach + 1):
        map_x = np.clip(xx + i * ldx, 0.0, w - 1.0)
        map_y = np.clip(yy + i * ldy, 0.0, h - 1.0)
        upstream = cv2.remap(height_field, map_x, map_y, cv2.INTER_LINEAR)
        max_upstream = np.maximum(max_upstream, upstream)

    shadow = np.maximum(max_upstream - height_field, 0.0)
    smax = float(shadow.max())
    if smax > 0:
        shadow /= smax

    return shadow


def _deshadow_pos(alpha, light_x, light_y, shadow_reach, shadow_strength):
    """
    Fill shadow valleys in the height field by lifting each pixel toward the
    maximum upstream height, weighted by how dark the pixel already is.

    Uses the same max-upstream ray-march as _shadow_mask — no averaging,
    so the result has no radial blur.
    """
    if shadow_strength <= 0 or shadow_reach < 1:
        return alpha

    h, w = alpha.shape
    arr = alpha.astype(np.float32)
    ldx, ldy = _light_dirs((h, w), light_x, light_y)

    yy, xx = np.meshgrid(np.arange(h, dtype=np.float32),
                         np.arange(w, dtype=np.float32), indexing='ij')

    max_upstream = np.zeros_like(arr)
    for i in range(1, shadow_reach + 1):
        map_x = np.clip(xx + i * ldx, 0.0, w - 1.0)
        map_y = np.clip(yy + i * ldy, 0.0, h - 1.0)
        upstream = cv2.remap(arr, map_x, map_y, cv2.INTER_LINEAR)
        max_upstream = np.maximum(max_upstream, upstream)

    # Amount each pixel is below its upstream neighbours
    shadow_depth = np.maximum(max_upstream - arr, 0.0)

    # Dark-pixel bias: lift dark valleys more than already-bright pixels
    dark_weight = np.clip(1.0 - arr / 255.0, 0.0, 1.0) ** 2

    corrected = arr + shadow_strength * dark_weight * shadow_depth
    return np.clip(corrected, 0, 255).astype(np.uint8)


def _decode_normals(norm_arr):
    """Decode a uint8 (H,W,3+) normal map to unit float32 (H,W,3) XYZ in [-1,1]."""
    nx = (norm_arr[:, :, 0].astype(np.float32) - 128.0) / 128.0
    ny = (norm_arr[:, :, 1].astype(np.float32) - 128.0) / 128.0
    nz = np.maximum(norm_arr[:, :, 2].astype(np.float32) / 255.0, 0.01)
    mag = np.sqrt(nx * nx + ny * ny + nz * nz)
    return nx / mag, ny / mag, nz / mag


def _ndotl_for_dir(nx, ny, nz, lx, ly, lz):
    """Compute per-pixel NdotL for a single unit direction (lx,ly,lz)."""
    return np.clip(nx * lx + ny * ly + nz * lz, 0.0, 1.0)


def fit_light_direction(norm_arr, luma, n_azimuth=36, n_elevation=9, ambient=0.25):
    """
    Find the directional light vector that best explains the baked lighting in
    `luma` given the surface normals in `norm_arr`.

    Grid-searches over a hemisphere (n_azimuth × n_elevation candidate directions)
    and returns the unit vector (lx, ly, lz) that maximises the Pearson
    correlation between (ambient + NdotL) and luma.

    Args:
        norm_arr:    uint8 (H, W, 3+) tangent-space normal map
        luma:        float32 (H, W) luminance of the diffuse texture, in [0,1]
        n_azimuth:   number of azimuth steps (default 36 = 10° resolution)
        n_elevation: number of elevation steps, 0–90° (default 9 = 10° steps)
        ambient:     ambient floor used in the lighting model

    Returns:
        (lx, ly, lz)  unit float32 tuple — best-fit light direction
        best_corr     Pearson r for the winning direction
    """
    nx, ny, nz = _decode_normals(norm_arr)
    luma_flat = luma.ravel()
    luma_mean = luma_flat.mean()
    luma_std  = luma_flat.std() + 1e-9

    best_corr = -1.0
    best_dir  = (0.0, 0.0, 1.0)

    azimuths   = np.linspace(0, 2 * np.pi, n_azimuth, endpoint=False)
    elevations = np.linspace(np.radians(10), np.radians(80), n_elevation)

    for elev in elevations:
        lz = float(np.sin(elev))
        for az in azimuths:
            lx = float(np.cos(elev) * np.cos(az))
            ly = float(np.cos(elev) * np.sin(az))
            ndotl   = _ndotl_for_dir(nx, ny, nz, lx, ly, lz)
            lighting = (ndotl + ambient).ravel()
            # Pearson correlation between predicted lighting and observed luma
            r = float(np.dot(lighting - lighting.mean(), luma_flat - luma_mean)
                      / (lighting.std() * luma_std * len(luma_flat) + 1e-9))
            if r > best_corr:
                best_corr = r
                best_dir  = (lx, ly, lz)

    return best_dir, best_corr


def _normal_delight(f, norm_arr, light_dir=None, ambient=0.25):
    """
    Estimate and divide out the baked directional lighting using the normal map.

    For each pixel, computes NdotL (how much light hit the surface), then
    recovers the underlying albedo by dividing: albedo ≈ diffuse / lighting.

    Args:
        f:          float32 (H, W, 3) diffuse in [0, 1]
        norm_arr:   uint8  (H, W, 3+) tangent-space normal map
        light_dir:  (lx, ly, lz) unit vector, or None to auto-fit from the image
        ambient:    ambient floor (prevents divide-by-zero and over-brightening)

    Returns float32 (H, W, 3) albedo in [0, 1].
    """
    nx, ny, nz = _decode_normals(norm_arr)

    if light_dir is None:
        luma = (0.2126 * f[:, :, 0] + 0.7152 * f[:, :, 1] + 0.0722 * f[:, :, 2])
        light_dir, _ = fit_light_direction(norm_arr, luma, ambient=ambient)

    lx, ly, lz = light_dir
    ndotl   = _ndotl_for_dir(nx, ny, nz, lx, ly, lz)
    lighting = np.clip(ndotl + ambient, ambient, 1.0)

    delit = f / lighting[:, :, np.newaxis]
    return np.clip(delit, 0.0, 1.0)


def color_transfer_lab(source, reference, strength=0.3):
    """
    Soft LAB color transfer: nudge `source` mean/std per-channel toward
    `reference`, blended at `strength` (0=no change, 1=full match).

    Both inputs are float32 (H, W, 3) BGR in [0, 1].
    Both source and reference are assumed to still carry some baked lighting,
    so a low default strength (0.3) avoids over-correcting.
    """
    s = (source * 255).astype(np.uint8)
    r = (reference * 255).astype(np.uint8)

    s_lab = cv2.cvtColor(s, cv2.COLOR_BGR2LAB).astype(np.float32)
    r_lab = cv2.cvtColor(r, cv2.COLOR_BGR2LAB).astype(np.float32)

    result = np.empty_like(s_lab)
    for c in range(3):
        s_mean, s_std = s_lab[:, :, c].mean(), s_lab[:, :, c].std() + 1e-6
        r_mean, r_std = r_lab[:, :, c].mean(), r_lab[:, :, c].std() + 1e-6
        corrected = (s_lab[:, :, c] - s_mean) * (r_std / s_std) + r_mean
        result[:, :, c] = s_lab[:, :, c] + strength * (corrected - s_lab[:, :, c])

    result = np.clip(result, 0, 255).astype(np.uint8)
    out_bgr = cv2.cvtColor(result, cv2.COLOR_LAB2BGR).astype(np.float32) / 255.0
    return np.clip(out_bgr, 0.0, 1.0)


def correct_diffuse(rgb, illum_sigma=60.0, strength=0.0, gamma=1.0,
                    height_field=None, norm_arr=None,
                    light_x=None, light_y=None,
                    shadow_reach=0, shadow_strength=0.8,
                    ref_rgb=None, ref_strength=0.3,
                    ambient=0.25):
    """
    Remove baked illumination from a diffuse RGB texture.

    Args:
        rgb:             uint8 (H, W, 3) diffuse texture (RGB)
        illum_sigma:     Gaussian sigma for illumination estimate (pixels)
        strength:        Global Retinex correction blend (0=none, 1=full)
        gamma:           Output brightness curve (>1.0 = brighter midtones)
        height_field:    float32 (H, W) [0,1] from integrate_normals, or None
        light_x, light_y: Manual light position override (image coords).
                         When None, the light direction is auto-fitted from the
                         normal map + diffuse correlation (recommended).
        norm_arr:        uint8 (H, W, 3+) tangent-space normal map, or None.
                         When provided, uses NdotL delighting with auto-fitted
                         light direction (physically correct for on-surface
                         shadows).  Falls back to height-field ray-march when None.
        shadow_reach:    Pixels to ray-march for height-field shadow occlusion
                         (only used when norm_arr is None, 0=disabled)
        shadow_strength: NdotL / shadow correction blend strength (0=off, 1=full)
        ref_rgb:         uint8 (H, W, 3) reference texture (e.g. original Quake
                         texture) used as a soft color anchor.  May itself have
                         baked lighting — only used for gentle tone calibration.
        ref_strength:    How strongly to pull toward reference colors (0–1,
                         default 0.3).  0 disables the reference entirely.
        ambient:         Lambertian ambient floor for NdotL correction.

    Returns:
        corrected_rgb:  uint8 (H, W, 3)
        heightmap:      uint8 (H, W)
        light_dir:      (lx, ly, lz) unit tuple — fitted or derived light direction
    """
    h_rgb, w_rgb = rgb.shape[:2]

    # ── Input as float BGR (OpenCV convention for LAB) ────────────────────────
    f_rgb = rgb.astype(np.float32) / 255.0
    f_bgr = f_rgb[:, :, ::-1].copy()

    # ── NdotL delight (auto-fit light direction) ──────────────────────────────
    light_dir = None
    if norm_arr is not None:
        na = norm_arr
        if na.shape[:2] != (h_rgb, w_rgb):
            na = cv2.resize(na, (w_rgb, h_rgb), interpolation=cv2.INTER_LINEAR)

        if light_x is not None and light_y is not None:
            # Manual override: convert image-space position to unit direction
            dx = float(light_x) - w_rgb / 2.0
            dy = float(light_y) - h_rgb / 2.0
            dz = max(w_rgb, h_rgb) * 0.5   # fixed elevation ≈ 45°
            mag = np.sqrt(dx*dx + dy*dy + dz*dz) + 1e-9
            light_dir = (dx / mag, dy / mag, dz / mag)
        else:
            luma = (0.2126 * f_rgb[:, :, 0]
                  + 0.7152 * f_rgb[:, :, 1]
                  + 0.0722 * f_rgb[:, :, 2])
            light_dir, corr = fit_light_direction(na, luma, ambient=ambient)

        # NdotL strength is clamped to [0,1] — this is a proper lerp to the
        # physically-correct albedo.  Values > 1 would over-brighten.
        ndotl_blend = min(float(shadow_strength), 1.0)
        delit_bgr = _normal_delight(f_bgr, na, light_dir=light_dir, ambient=ambient)
        f_bgr = np.clip(f_bgr + ndotl_blend * (delit_bgr - f_bgr), 0.0, 1.0)

    # ── Height-field ray-march: cast shadow fill ──────────────────────────────
    # Run whenever shadow_reach > 0 AND we have a height field, regardless of
    # whether we also have a normal map.  NdotL (above) removes Lambertian
    # shading from surface orientation; this pass removes cast shadows (one
    # surface occluding light that would otherwise hit another surface).
    if height_field is not None and shadow_reach > 0:
        # Derive the 2D shadow-ray direction from the fitted 3D light direction
        # or from the manual override.  Tangent +Y = image -Y, so flip ly.
        if light_dir is not None:
            dlx, dly, dlz = light_dir
            far = max(w_rgb, h_rgb) * 10.0
            lx_2d = w_rgb / 2.0 + dlx * far
            ly_2d = h_rgb / 2.0 + dly * far   # no Y flip: tangent ly sign matches image
        elif light_x is not None and light_y is not None:
            lx_2d, ly_2d = float(light_x), float(light_y)
        else:
            lx_2d, ly_2d = w_rgb * 1.2, -h_rgb * 0.2

        hf = height_field
        if hf.shape != (h_rgb, w_rgb):
            hf = cv2.resize(hf, (w_rgb, h_rgb), interpolation=cv2.INTER_LINEAR)

        # Retinex of the current (already NdotL-corrected) image as fill target
        lum_f = (0.2126 * f_bgr[:, :, 2]
               + 0.7152 * f_bgr[:, :, 1]
               + 0.0722 * f_bgr[:, :, 0])
        ksize = int(60 * 6) | 1
        illum = cv2.GaussianBlur(lum_f, (ksize, ksize), 60).astype(np.float32)
        illum = illum / max(float(illum.mean()), 1e-6) + 1e-6
        retinex = np.clip(
            np.stack([f_bgr[:, :, c] / illum for c in range(3)], axis=2),
            0.0, 1.0)

        mask = _shadow_mask(hf, lx_2d, ly_2d, shadow_reach)
        # shadow_strength > 1 allows extra boost in shadow regions specifically
        f_bgr = np.clip(
            f_bgr + mask[:, :, None] * shadow_strength * (retinex - f_bgr),
            0.0, 1.0)

    elif norm_arr is None and shadow_reach > 0:
        # No height field either — nothing to do
        pass

    # ── Retinex global flattening ─────────────────────────────────────────────
    if strength > 0:
        lum = (0.2126 * f_bgr[:, :, 2]
             + 0.7152 * f_bgr[:, :, 1]
             + 0.0722 * f_bgr[:, :, 0])
        ksize = int(illum_sigma * 6) | 1
        illum = cv2.GaussianBlur(lum, (ksize, ksize), illum_sigma).astype(np.float32)
        illum = illum / max(float(illum.mean()), 1e-6) + 1e-6
        retinex = np.stack([f_bgr[:, :, c] / illum for c in range(3)], axis=2)
        lo = float(np.percentile(retinex, 1))
        hi = float(np.percentile(retinex, 99))
        if hi > lo:
            retinex = (retinex - lo) / (hi - lo)
        retinex = np.clip(retinex, 0.0, 1.0)
        f_bgr = np.clip(f_bgr + strength * (retinex - f_bgr), 0.0, 1.0)

    # ── Soft color transfer from Quake reference ──────────────────────────────
    if ref_rgb is not None and ref_strength > 0:
        ref_bgr = ref_rgb.astype(np.float32) / 255.0
        ref_bgr = ref_bgr[:, :, ::-1].copy()
        if ref_bgr.shape[:2] != (h_rgb, w_rgb):
            ref_bgr = cv2.resize(ref_bgr, (w_rgb, h_rgb), interpolation=cv2.INTER_LANCZOS4)
        f_bgr = color_transfer_lab(f_bgr, ref_bgr, strength=ref_strength)

    # ── Gamma ─────────────────────────────────────────────────────────────────
    if abs(gamma - 1.0) > 0.01:
        f_bgr = np.power(np.clip(f_bgr, 0.0, 1.0), 1.0 / gamma)
    f_bgr = np.clip(f_bgr, 0.0, 1.0)
    corrected_rgb = (f_bgr[:, :, ::-1] * 255).round().astype(np.uint8)

    # ── Heightmap ─────────────────────────────────────────────────────────────
    if height_field is not None:
        hf = height_field
        if hf.shape != (h_rgb, w_rgb):
            hf = cv2.resize(hf, (w_rgb, h_rgb), interpolation=cv2.INTER_LINEAR)
        hf_uint8 = (hf * 255).round().astype(np.uint8)
        if shadow_reach > 0:
            hm_reach = min(shadow_reach, 3)
            # Use the same 2D light position as the albedo cast-shadow pass
            if light_dir is not None:
                dlx, dly, dlz = light_dir
                far = max(w_rgb, h_rgb) * 10.0
                lx_hm = w_rgb / 2.0 + dlx * far
                ly_hm = h_rgb / 2.0 + dly * far   # no Y flip
            elif light_x is not None and light_y is not None:
                lx_hm, ly_hm = float(light_x), float(light_y)
            else:
                lx_hm, ly_hm = w_rgb * 1.2, -h_rgb * 0.2
            hf_uint8 = _deshadow_pos(hf_uint8, lx_hm, ly_hm,
                                     hm_reach, shadow_strength)
        heightmap = hf_uint8
    else:
        f_out = f_bgr[:, :, ::-1]
        lum_out = (0.2126 * f_out[:, :, 0]
                 + 0.7152 * f_out[:, :, 1]
                 + 0.0722 * f_out[:, :, 2])
        lmin, lmax = float(lum_out.min()), float(lum_out.max())
        if lmax > lmin:
            lum_out = (lum_out - lmin) / (lmax - lmin)
        heightmap = (lum_out * 255).round().astype(np.uint8)

    return corrected_rgb, heightmap, light_dir
