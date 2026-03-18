# Light vs Sprite Color Spaces - Important Distinction

**Date:** November 30, 2024

## Key Difference

### Lights use RGB (vec3_t)
```c
cg_light_t light = {
  .color = ColorHSV(hue, sat, val).vec3,  // HSV→RGB conversion, stores RGB
  ...
};
```

The `ColorHSV()` function converts HSV to RGB and returns `color_t` (RGBA).
The `.vec3` extracts just the RGB components as `vec3_t`.
**Result: Light colors are stored as RGB values.**

### Sprites use HSV (vec3_t)
```c
cg_sprite_t sprite = {
  .color = Vec3(hue, sat, val),  // Stores HSV directly
  ...
};
```

Sprite colors are stored as HSV and converted to RGB at render time:
```c
const vec3_t c = Vec3_Mix(s->color, s->end_color, life);  // Interpolate in HSV
const color_t color = ColorHSV(c.x, c.y, c.z);            // Convert to RGB
```

**Result: Sprite colors are stored as HSV values.**

## Why This Matters

### Lights (RGB):
- Created once and stored
- No per-frame conversion needed
- RGB is ready for GPU
- Simpler at runtime

### Sprites (HSV):
- Interpolated every frame (color → end_color)
- Interpolation in HSV space gives better visual results
- Single HSV→RGB conversion per sprite per frame
- More perceptually correct color transitions

## Summary

| Type | Storage | Conversion | Usage |
|------|---------|------------|-------|
| **Lights** | RGB (vec3_t) | HSV→RGB at creation | Static or slowly changing |
| **Sprites** | HSV (vec3_t) | HSV→RGB at render | Animated, interpolated |

Both use `vec3_t` but in **different color spaces**!
