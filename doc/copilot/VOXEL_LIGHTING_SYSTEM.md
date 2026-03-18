# Voxel Lighting System

**Date**: 2026-01-01  
**Status**: Complete with minor bleeding artifacts through thin walls

## Overview

The voxel lighting system divides the BSP world into a 3D grid of voxels (32×32×32 unit cubes) aligned to world coordinates (0, ±32, ±64, etc.). Each voxel stores:
- **Contents**: 4 tetrahedral samples (RGBA32I) for liquid detection and caustics
- **Fog**: RGB color + density for volumetric fog with light absorption
- **Light metadata**: Offset and count into the light indices buffer
- **Light indices**: List of light IDs that affect this voxel

## Architecture

### Quemap (Compile-Time)

**BuildVoxelExtents()**: Creates world-aligned grid
```c
voxels.stu_bounds.mins = floor(world_bounds.mins / 32) * 32
voxels.stu_bounds.maxs = ceil(world_bounds.maxs / 32) * 32
voxels.size = (maxs - mins) / 32
```

**LightVoxel()**: Assigns lights to voxels
- Distance test: `dist < light->radius + BSP_VOXEL_SIZE * 0.5`
- Traces from **light to voxel corners/center** (9 points)
- Success if trace reaches within `0.5 * BSP_VOXEL_SIZE` of target
- Key insight: Tracing from light outward avoids starting inside geometry

**EmitVoxels()**: Writes to BSP file in u/t/s order
1. Contents (4 × int32 per voxel) - RGBA32I
2. Fog (1 × color32 per voxel) - RGBA8
3. Light metadata (2 × int32 per voxel) - offset, count
4. Light indices (flat int32 array)

### Runtime (Renderer)

**R_LoadBspVoxels()**: Loads voxel data into 3D textures
- `texture_voxel_contents` (GL_RGBA32I) - 4 contents samples per voxel
- `texture_voxel_fog` (GL_RGBA8 with mipmaps) - fog color and density
- `texture_voxel_light_data` (GL_RG32I) - offset and count per voxel
- `texture_voxel_light_indices` (GL_TEXTURE_BUFFER, GL_R32I) - light IDs

**Shader**: Looks up lights for fragment
```glsl
// Nudge position along surface normal to avoid sampling voxels inside walls
vec3 nudged_pos = vertex.model + vertex.model_normal * BSP_VOXEL_SIZE;
ivec3 voxel = voxel_xyz(nudged_pos);
ivec2 data = voxel_light_data(voxel);  // offset, count

for (int i = 0; i < data.y; i++) {
  int light_id = voxel_light_index(data.x + i);
  light_and_shadow_light(light_id);
}
```

## Key Design Decisions

### World-Aligned Grid
Voxels are placed at fixed world positions (0, 32, 64, ...) rather than being centered on the map. This:
- ✅ Eliminates coordinate mismatch bugs between quemap and runtime
- ✅ Makes voxel positions predictable and debuggable
- ✅ Simplifies the math (no padding, no transforms)
- ❌ Can cause Z-fighting if geometry aligns exactly with grid

### Light Assignment Strategy
After extensive experimentation, settled on:
1. **Conservative distance check**: radius + 16 units ensures boundary voxels get lights
2. **Trace from light to voxel corners**: Avoids starting traces inside geometry
3. **Tolerance check**: Success if trace reaches within 16 units of target point
4. **No trace from voxel to light**: Starting from embedded corners always failed

### Shader-Side Nudge
Fragments nudge their lookup position one voxel (32 units) along the surface normal:
```glsl
vec3 nudged_pos = vertex.model + vertex.model_normal * BSP_VOXEL_SIZE;
```
This ensures surfaces at voxel boundaries sample the voxel in open space rather than the one inside geometry.

### Contents Texture (RGBA32I)
4 tetrahedral samples per voxel at ±0.25 offsets provide sub-voxel gradient information for:
- **Caustics attenuation**: Count liquid samples (0-4) for smooth 0.0-1.0 gradient
- **Fog detection**: Check for water/lava/slime at sample points

## Critical Bugs Fixed

### 1. Light Indices Texture Buffer Setup Bug
**Problem**: Code was setting properties on `light_data` instead of `light_indices`
```c
// WRONG:
out->light_data->target = GL_TEXTURE_BUFFER;
out->light_data->internal_format = GL_R32I;
out->light_data->buffer = out->light_indices_buffer;

// CORRECT:
out->light_indices->target = GL_TEXTURE_BUFFER;
out->light_indices->internal_format = GL_R32I;
out->light_indices->buffer = out->light_indices_buffer;
```
**Impact**: Light index lookups were completely broken, sampling garbage data

### 2. Light Metadata Emission Order Mismatch
**Problem**: Contents/fog written in u/t/s nested loops, but metadata written in linear loop
```c
// Contents/fog: (u * size.y + t) * size.x + s  ✓
// Metadata: linear i++  ✗
```
**Fix**: All data now written in identical u/t/s order
**Impact**: Shader was fetching metadata for wrong voxels

### 3. Trace Direction
**Problem**: Tracing from voxel corners to light failed when corners were inside geometry
**Solution**: Trace from light to voxel corners, check if trace reaches close enough
**Impact**: Boundary voxels finally get lights assigned correctly

### 4. CONTENTS_SOLID Filtering
**Problem**: Early code skipped lighting voxels that had ANY solid samples
```c
if (ProjectVoxel(v, soffs, toffs, uoffs) == CONTENTS_SOLID) {
  continue;  // Skip this voxel entirely!
}
```
**Impact**: Voxels at boundaries (where visible surfaces exist) had zero lights
**Fix**: Removed this check - boundary voxels need lights!

## Performance Characteristics

### Memory Usage
- ~4 bytes per voxel for contents (RGBA32I)
- ~4 bytes per voxel for fog (RGBA8)
- ~8 bytes per voxel for metadata (2×int32)
- Variable size for light indices (depends on light density)

Typical map: ~167k voxels = ~2.7MB base + indices

### Runtime Performance
- Average ~0.9-2.0 lights per voxel with current settings
- Single voxel lookup per fragment (integer texture fetch)
- Light iteration loop typically processes 0-3 lights per fragment
- No noticeable performance impact compared to previous lighting system

### Compile Time
- Lighting voxels is parallelized via Work() system
- Trace cost is primary bottleneck
- Typical compile time: Reasonable for development iteration

## Remaining Known Issues

### Minor Light Bleeding
Lights can bleed through thin walls (< 1 voxel thick) because:
- Conservative radius includes nearby voxels
- Traces from light may reach corners on opposite side of thin geometry
- Acceptable trade-off for eliminating boundary artifacts

**Potential fix**: Could check if trace passes through any geometry (not just occlusion at endpoints)

### Caustics Boundaries
While much improved with 4-sample sub-voxel gradients, caustics still show slight stepping at voxel boundaries. This is inherent to discrete voxel sampling.

**Potential fix**: Trilinear interpolation of caustics (samples 8 neighboring voxels) - but expensive

## Debug Visualization

### Voxel Grid Overlay
```glsl
if (developer > 0.5) {
  ivec3 voxel = voxel_xyz(vertex.model);
  vec3 voxel_color = vec3(
    float(voxel.x & 1),
    float(voxel.y & 1),
    float(voxel.z & 1)
  );
  out_color.rgb = mix(out_color.rgb, voxel_color, 0.3);
}
```
Shows 3D checkerboard pattern aligned with voxel boundaries. Essential for verifying coordinate system.

### Light Count Visualization
```glsl
if (developer > 0.5) {
  fragment.diffuse = vec3(float(data.y) / 10.0);
}
```
Displays number of lights per voxel (scaled by 10). Brighter = more lights.

## Future Improvements

1. **Voxel-based fog volumes**: Store fog density in voxels, sample along view ray
2. **Global illumination**: Store indirect lighting in voxels via light bounces
3. **Dynamic voxel updates**: Update voxel lighting for dynamic lights without recompiling
4. **Hierarchical voxels**: Use octree for more efficient storage and lookup
5. **Better thin wall handling**: Additional geometry tests during light assignment

## Lessons Learned

1. **Coordinate system alignment is critical**: Spent significant time debugging mismatches between quemap emission order and shader lookup
2. **Trace direction matters**: Tracing from light outward is more robust than from embedded corners
3. **Boundary voxels are most important**: These are where visible surfaces exist
4. **Simple is better**: Conservative radius + tolerance check > complex multisampling
5. **Debug visualization is essential**: Voxel checkerboard pattern immediately showed coordinate bugs
6. **Shader-side hacks can be good**: Normal-based nudge elegantly solves boundary sampling issues

## Related Documents
- `CLUSTERED_LIGHTING_IMPLEMENTATION.md` - Previous clustered forward lighting attempt
- `SESSION_CONTEXT.md` - Current session state
