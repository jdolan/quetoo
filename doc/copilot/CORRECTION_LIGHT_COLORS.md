# Correction: Light Color Space

**Date:** November 30, 2024

## Important Correction

In the previous documentation, I incorrectly stated that lights use HSV color space. This is **incorrect**.

## The Truth

**Lights use RGB, NOT HSV:**

```c
// Light creation pattern
cg_light_t light = {
  .color = ColorHSV(hue, sat, val).vec3,  // ← HSV→RGB conversion happens HERE
  ...
};
```

**What happens:**
1. `ColorHSV(h, s, v)` converts HSV to RGB, returns `color_t` (RGBA float)
2. `.vec3` extracts the RGB part as `vec3_t`
3. Light stores **RGB values** in `vec3_t color` field

**Sprites use HSV:**

```c
// Sprite creation pattern
cg_sprite_t sprite = {
  .color = Vec3(hue, sat, val),  // ← HSV stored directly
  ...
};
```

**What happens:**
1. `Vec3(h, s, v)` creates a `vec3_t` with HSV values
2. Sprite stores **HSV values** in `vec3_t color` field
3. Conversion happens at render time: `ColorHSV(c.x, c.y, c.z)`

## Why Both Use vec3_t

Both lights and sprites use `vec3_t` for their color fields, but they store **different color spaces**:

- **Lights:** `vec3_t` contains **RGB** (red, green, blue)
- **Sprites:** `vec3_t` contains **HSV** (hue, saturation, value)

This is why the code patterns are different:
- Lights: `ColorHSV(...).vec3` (convert at creation)
- Sprites: `Vec3(...)` (store HSV, convert at render)

## Why This Design Makes Sense

### Lights (RGB storage):
- Created once, rarely change
- No per-frame conversion overhead
- RGB ready for GPU immediately
- Static or simple animations

### Sprites (HSV storage):
- Animated with color transitions
- Interpolation in HSV gives better visual results
- Color fades (hue rotation, saturation, brightness)
- Single conversion per sprite per frame is acceptable

## Example Comparison

```c
// Light: Blue-ish light
cg_light_t light = {
  .color = ColorHSV(240.f, 1.f, 1.f).vec3,  // → RGB(0, 0, 1) stored
};

// Sprite: Blue-ish sprite  
cg_sprite_t sprite = {
  .color = Vec3(240.f, 1.f, 1.f),  // → HSV(240, 1, 1) stored
};
```

At render time:
- **Light:** RGB already available, sent to GPU directly
- **Sprite:** HSV converted to RGB: `ColorHSV(240.f, 1.f, 1.f)` → RGB(0, 0, 1)

## Apology

I apologize for the confusion in the earlier documentation. The distinction between light colors (RGB) and sprite colors (HSV) is now clearly documented in:
- `LIGHT_VS_SPRITE_COLORS.md` - Detailed explanation
- This correction document

## Summary

✅ **Lights:** `vec3_t` stores **RGB** (converted from HSV at creation)
✅ **Sprites:** `vec3_t` stores **HSV** (converted to RGB at render)
✅ **Both:** Use `vec3_t` type, but different color spaces
✅ **Refactor:** Only changed sprites from vec4_t→vec3_t (dropped alpha from HSV)

The refactor is still correct - we only changed sprite colors from HSVA to HSV. Light colors were always RGB and remain unchanged.
