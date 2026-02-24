# Sky Material Stages

## Overview

The sky shader (`r_sky.c`, `sky_vs.glsl`, `sky_fs.glsl`) was extended to support
material stage rendering on top of the skybox cubemap. This allows `.mat` files to
define scrolling, warping, and otherwise animated texture layers on sky surfaces,
using the same `stage { ... }` syntax already supported on BSP and mesh materials.

---

## Motivation

Sky surfaces in Quetoo are rendered as ordinary BSP geometry with the `SURF_SKY`
flag, but the shader previously ignored any material stages attached to the sky
material. Common use cases include:

- Scrolling cloud layers blended over the sky cubemap
- Animated effects (rotation, stretch, warp) layered on the sky
- Color-tinted or pulsing overlays

---

## Implementation

### Files Modified

| File | Change |
|------|--------|
| `src/client/renderer/shaders/sky_vs.glsl` | Read `in_diffusemap`, call `stage_vertex()` |
| `src/client/renderer/shaders/sky_fs.glsl` | Branch on `stage.flags` for cubemap vs. stage pass |
| `src/client/renderer/r_sky.c` | Stage uniform locations, two-pass draw loop |

---

### Vertex Shader (`sky_vs.glsl`)

**Before:** `vertex.diffusemap` was always `vec2(0.0)` — UV coordinates were
unused because the sky only ever sampled a cubemap.

**After:**

```glsl
layout (location = 4) in vec2 in_diffusemap;
...
vertex.diffusemap = in_diffusemap;
...
stage_vertex(stage, in_position, vertex.position, vertex.diffusemap, vertex.color);
```

- `in_diffusemap` reads the BSP vertex array's UV channel (location 4, same as
  `bsp_vs.glsl`).
- `stage_vertex()` applies scroll, scale, rotate, and stretch transforms in the
  vertex shader before interpolation — identical to the BSP path.
- `material.glsl` is now also included in the **vertex** shader descriptor so
  `stage_vertex()` and `stage_transform()` are available.

---

### Fragment Shader (`sky_fs.glsl`)

The fragment shader now branches on `stage.flags`:

```glsl
if ((stage.flags & STAGE_MATERIAL) == STAGE_MATERIAL) {

    // Default pass: render cubemap
    out_color = texture(texture_sky, normalize(cubemap_coord));
    out_color.rgb = mix(out_color.rgb, vertex.fog.rgb, vertex.fog.a);

} else {

    // Stage pass: render 2D overlay texture
    vec2 st = vertex.diffusemap;

    if ((stage.flags & STAGE_WARP) == STAGE_WARP) {
        st += texture(texture_warp, st + vec2(ticks * stage.warp.x * 0.000125)).xy * stage.warp.y;
    }

    out_color = sample_material_stage(st) * vertex.color;
}
```

- `STAGE_MATERIAL` (the default) renders the cubemap exactly as before.
- Any other `stage.flags` value renders a 2D stage texture from `texture_stage`,
  with optional warp distortion applied to the UV coordinates.

---

### C Renderer (`r_sky.c`)

**Program struct** gained a `texture_stage` uniform location and a `stage` sub-struct
mirroring `r_bsp_program.stage`:

```c
struct {
    GLint flags;
    GLint color;
    GLint pulse;
    GLint st_origin;
    GLint stretch;
    GLint rotate;
    GLint scroll;
    GLint scale;
    GLint terrain;
    GLint dirtmap;
    GLint warp;
} stage;
```

**`R_InitSkyProgram`** was updated to:
1. Include `material.glsl` in the vertex shader descriptor.
2. Look up all stage uniform locations.
3. Bind `texture_stage` to `TEXTURE_STAGE`.
4. Initialize `stage.flags = STAGE_MATERIAL` (so the default cubemap pass runs
   with no additional setup).

**`R_DrawSky`** now performs two passes:

1. **Cubemap pass** — sets `stage.flags = STAGE_MATERIAL` and draws all `SURF_SKY`
   elements as before.
2. **Stage pass** — when `r_materials` is enabled, iterates sky draw elements a
   second time, enabling `GL_BLEND` and calling the new helper
   `R_DrawSkyDrawElementsMaterialStage()` for each `STAGE_DRAW` stage found on the
   surface's material. Resets `stage.flags` and active texture unit when done.

`R_DrawSkyDrawElementsMaterialStage()` is a direct analogue of
`R_DrawBspDrawElementsMaterialStage()` — it binds all stage uniforms conditionally
by flag and issues the draw call.

---

## Supported Stage Features on Sky

All features handled by `stage_vertex()` and the stage fragment path work on sky
surfaces:

| Feature | Flag(s) | Notes |
|---------|---------|-------|
| Scrolling | `STAGE_SCROLL_S`, `STAGE_SCROLL_T` | Texture units per second |
| Scaling | `STAGE_SCALE_S`, `STAGE_SCALE_T` | UV multiplier |
| Rotation | `STAGE_ROTATE` | Hz, around `st_origin` |
| Stretch | `STAGE_STRETCH` | Sinusoidal amplitude modulation |
| Color tint | `STAGE_COLOR` | RGBA multiply |
| Alpha pulse | `STAGE_PULSE` | Sinusoidal alpha |
| Warp | `STAGE_WARP` | UV distortion via warp texture |
| Animation | `STAGE_ANIM` | Frame from animation array |
| Terrain | `STAGE_TERRAIN` | Z-based alpha blend |
| Dirtmap | `STAGE_DIRTMAP` | Position-based alpha variation |

`STAGE_ENVMAP` and `STAGE_SHELL` have no meaningful mapping to sky surfaces and
are silently ignored (their flags will never appear on a sky material in practice).

---

## Example Material

```
textures/sky/cloudy
{
    stage
    {
        map textures/effects/clouds
        scroll 0.02 0.005
        blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
    }
}
```

This scrolls a cloud overlay horizontally at 0.02 tex-units/sec and vertically at
0.005 tex-units/sec over the cubemap sky.

---

## Design Notes

- **Two-pass, not one:** The stage pass is a separate draw over the sky geometry
  with blending enabled, exactly like BSP material stages. This avoids complicating
  the cubemap path.
- **No fog on stage pass:** Sky stage textures are not fogged. The cubemap pass
  applies fog as before. Adding per-stage fog to sky stages would require a full
  `fragment_t` setup; it is not worth the complexity.
- **`r_materials 0` skips stages:** Controlled by the existing `r_materials` cvar,
  consistent with BSP and mesh behaviour.
- **Vertex shader includes `material.glsl`:** Required for `stage_vertex()` and
  `stage_transform()` access, matching the BSP program descriptor pattern.
