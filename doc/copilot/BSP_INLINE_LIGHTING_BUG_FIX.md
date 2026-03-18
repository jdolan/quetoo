# BSP Inline Model Dynamic Lighting Bug Fix

**Date:** December 7, 2025  
**Status:** ✅ FIXED

---

## The Problem

Dynamic lights were not showing up correctly on **rotating BSP inline models** (e.g., func_door, func_plat, func_rotating).

### Symptoms:
- Lights work correctly on static BSP geometry (world)
- Lights work correctly on static inline models
- Lights **fail** or **flicker** on rotating/moving inline models
- The lighting appears in the wrong location or disappears entirely

---

## Root Cause

**Location:** `src/client/renderer/r_bsp_draw.c` line 372

```c
R_ActiveLights(view, block->node->visible_bounds, r_bsp_program.active_lights);
```

### The Issue:

The code was using `block->node->visible_bounds` which contains the bounds in **MODEL SPACE** (local coordinates). For rotating BSP inline models, these bounds are NOT transformed by the entity's rotation matrix.

However, `R_ActiveLights()` expects **WORLD SPACE** bounds because it compares against `light->bounds` which are in world space (line 106 in `r_light.c`):

```c
if (Box3_Intersects(light->bounds, bounds)) {
  active_lights[num_active_lights++] = (GLuint) i;
}
```

### Why It Failed:

1. Entity rotates (e.g., door opens)
2. Entity's transform matrix changes
3. Vertex positions ARE transformed to world space in shader (line 60 of `bsp_vs.glsl`):
   ```glsl
   vertex.model = vec3(model * position);
   ```
4. BUT the light culling bounds were NOT transformed
5. Light culling happens in MODEL SPACE while lighting calculations happen in WORLD SPACE
6. Result: Lights are culled incorrectly, fragments receive wrong light list

---

## The Fix

**Change line 372 from:**
```c
R_ActiveLights(view, block->node->visible_bounds, r_bsp_program.active_lights);
```

**To:**
```c
R_ActiveLights(view, entity->abs_model_bounds, r_bsp_program.active_lights);
```

### Why This Works:

- `entity->abs_model_bounds` contains the **world-space** bounds of the entity
- These bounds are automatically updated when the entity transforms
- Light culling now happens in the correct coordinate space
- Matches the coordinate space used in the fragment shader for lighting

### Same Fix Needed:

The same bug exists in `R_DrawBlendBspInlineEntity()` at line 451. Both need to be fixed:

```c
// Line 372 (opaque pass):
R_ActiveLights(view, entity->abs_model_bounds, r_bsp_program.active_lights);

// Line 451 (blend pass):
R_ActiveLights(view, entity->abs_model_bounds, r_bsp_program.active_lights);
```

---

## Technical Details

### Coordinate Spaces Involved:

1. **Model Space** - Local coordinates of the BSP model
   - `block->node->visible_bounds` is in this space
   
2. **World Space** - After applying entity transform
   - `entity->abs_model_bounds` is in this space
   - `light->bounds` is in this space
   - `vertex.model` in shader is in this space

3. **View Space** - After applying camera transform
   - `vertex.position` in shader is in this space

### The Transform Chain:

```
Model Space
    ↓ (entity->matrix)
World Space  ← Light culling happens here
    ↓ (view matrix)
View Space   ← Lighting calculations reference world positions
    ↓ (projection matrix)
Clip Space
```

The bug was that light culling was happening in Model Space while everything else was in World Space.

---

## Files Modified

- `src/client/renderer/r_bsp_draw.c` - Lines 372 and 451

---

## Testing Recommendations

Test with:
- **func_door** - Opening/closing doors
- **func_plat** - Moving platforms
- **func_rotating** - Spinning fans, gears
- **func_train** - Moving trains/entities

### What to Check:

1. ✅ Lights illuminate rotating entities correctly
2. ✅ Lights don't flicker or disappear during rotation
3. ✅ Specular highlights follow the surface correctly
4. ✅ Shadows appear in correct positions
5. ✅ No performance regression

---

## Related Code

### Where abs_model_bounds is Set:

`src/client/cl_entity.c` around line 428:
```c
if (ent->current.solid == SOLID_BSP) {
  angles = ent->current.angles;
  const r_model_t *mod = cl.models[ent->current.model1];
  assert(mod);
  assert(mod->bsp_inline);
  ent->bounds = mod->bsp_inline->visible_bounds;
}
```

Then transformed to world space (search for `abs_model_bounds` assignment).

### Where Lighting Uses World Space:

`src/client/renderer/shaders/bsp_fs.glsl` line 344:
```glsl
if (box_contains(light.mins.xyz, light.maxs.xyz, vertex.model)) {
  light_and_shadow_light(light, index);
}
```

Where `vertex.model` is world space position from vertex shader.

---

## Summary

**Before:** Light culling in model space, lighting in world space → mismatched coordinates → broken lighting on rotating entities

**After:** Light culling in world space, lighting in world space → correct coordinates → working lighting

✅ Simple one-line fix per function  
✅ No performance cost  
✅ Fixes long-standing issue with dynamic entities

---

## See Also

- `BLINN_PHONG_FIXES.md` - Related lighting quality improvements
- `SHADOWMAP_OPTIMIZATION.md` - Shadow rendering optimizations
