# Vertex Shader Voxel Hint Optimization

**Date:** January 3, 2026  
**Status:** Implemented, awaiting performance testing

## Overview

This optimization moves voxel light list lookups from the fragment shader to the vertex shader, passing hints through the pipeline. Fragments within the same voxel as their vertices can skip the expensive 3D texture fetch.

## Motivation

The existing clustered lighting system performs per-fragment voxel lookups:

```glsl
// Fragment shader (before)
ivec3 voxel = voxel_xyz(vertex.model);        // Calculate voxel cell
ivec2 data = voxel_light_data(voxel);          // 3D texture fetch
```

For a 1920×1080 scene with 2x overdraw, this means ~4 million `voxel_light_data()` texture fetches per frame.

**Key insight:** Adjacent fragments within the same triangle often share the same voxel cell, especially for small/medium triangles relative to the 32-unit voxel size.

## Implementation

### Vertex Shader Changes

**`bsp_vs.glsl` and `mesh_vs.glsl`:**

Added two new outputs with `flat` interpolation (no interpolation across triangle):

```glsl
out vertex_data {
  // ... existing fields ...
  flat ivec3 voxel_hint;       // Voxel cell coordinates
  flat ivec2 voxel_data_hint;  // (offset, count) into light index array
} vertex;
```

Calculate hints once per vertex:

```glsl
vertex.voxel_hint = voxel_xyz(vertex.model);
vertex.voxel_data_hint = voxel_light_data(vertex.voxel_hint);
```

### Fragment Shader Changes

**`bsp_fs.glsl` and `mesh_fs.glsl`:**

Added early-out check using vertex hint:

```glsl
ivec3 voxel = voxel_xyz(vertex.model);
ivec2 data;

if (voxel == vertex.voxel_hint) {
  data = vertex.voxel_data_hint;  // Reuse from vertex shader
} else {
  data = voxel_light_data(voxel);  // Fragment in different voxel, fetch
}
```

## How It Works

### Cache Hit (Common Case)

For fragments whose world position falls within the same voxel as their vertex:

1. Fragment calculates its voxel: `voxel_xyz(vertex.model)` (cheap math)
2. Compares to hint: `voxel == vertex.voxel_hint` (ivec3 comparison)
3. **Hit!** Uses `vertex.voxel_data_hint` directly - no texture fetch

### Cache Miss (Triangle Spans Voxels)

When a triangle crosses voxel boundaries:

1. Fragment calculates its voxel
2. Comparison fails: `voxel != vertex.voxel_hint`
3. Falls back to normal path: `voxel_light_data(voxel)`

### Large Face Behavior

For faces spanning multiple voxels (the concern you raised):

- **Each vertex** gets its own voxel's light list
- **Fragments near vertices** use the vertex's hint (cache hit)
- **Fragments far from vertices** (center of large face) calculate their own voxel and fetch correctly (cache miss)
- **No false negatives** - fragments always get the correct lights for their position

## Performance Characteristics

### Expected Hit Rate

Triangle size distribution matters:

- **Small triangles** (< 32 units): ~95%+ hit rate - entire triangle in one voxel
- **Medium triangles** (32-128 units): ~50-80% hit rate - some edge fragments miss
- **Large triangles** (> 128 units): ~20-40% hit rate - only fragments near vertices hit

**Typical Quake-style BSP geometry** has many small/medium faces, so expect **60-80% overall hit rate**.

### Cost Analysis

**Per vertex (added):**
- `voxel_xyz()`: ~10 ALU ops (subtract, divide, floor, clamp)
- `voxel_light_data()`: 1 texture fetch (3D, RG32I)
- Passing hints: 5 int32 registers (3 + 2)

**Per fragment (saved on hit):**
- `voxel_light_data()`: 1 texture fetch ← **THIS IS THE WIN**

**Net benefit:**
- Fragment shader runs ~10-100x more than vertex shader (triangle size dependent)
- Even at 60% hit rate: eliminate 60% of 3D texture fetches
- Vertex cost is negligible compared to fragment savings

### Memory/Bandwidth

**Vertex shader output increase:**
- 5 int32 per vertex = 20 bytes/vertex
- Typical scene: 50k vertices = 1MB additional VS→FS data per frame
- Modern GPUs: ~500 GB/s bandwidth, 1MB is negligible

## Limitations & Edge Cases

### 1. Large Faces

Fragments in the center of large faces that cross many voxels will miss the cache and fall back to normal lookups. This is **correct behavior** - each fragment gets its actual voxel's lights.

**No false negatives** - the fallback ensures correctness.

### 2. Parallax Offset

If parallax mapping significantly displaces `vertex.model` position, the voxel calculation might differ between VS and FS. This would increase cache miss rate but remain correct.

### 3. Stage Transforms

The BSP stage transforms (scrolling, rotating textures) can modify vertex positions. The hints are calculated **after** stage transforms, so this is handled correctly.

### 4. Player Models

Mesh shader handles `VIEW_PLAYER_MODEL` case by setting hints to zero (these models don't use voxel lighting).

## Future Enhancements

If hit rate is lower than expected:

### Option A: Conservative Per-Triangle Hints

Use geometry shader to compute per-triangle bounding voxels and emit conservative light list union. More complex but eliminates all misses.

### Option B: Selective Face Subdivision (Option 3)

Subdivide BSP faces larger than N voxels during compilation:

```c
// In quemap
void SubdivideLargeFaces(face_t *face) {
  ivec3 voxel_span = VoxelSpan(face->bounds);
  if (voxel_span.x > 4 || voxel_span.y > 4 || voxel_span.z > 4) {
    // Subdivide face into smaller chunks
  }
}
```

This would increase hit rate to near 100% but adds geometry.

## Testing

### Performance Metrics

Measure before/after with:

```
r_speeds 1
```

Look for:
- Frame time improvement (~5-15% expected in fragment-heavy scenes)
- Reduced texture bandwidth

### Visual Verification

No visual changes expected. Lighting should be identical to before.

### Edge Case Testing

- Stand at voxel boundaries and look for lighting pops
- Large open areas with big BSP faces
- Complex geometry with many small faces

## Implementation Files

### Modified Files
- `src/client/renderer/shaders/bsp_vs.glsl`
- `src/client/renderer/shaders/bsp_fs.glsl`
- `src/client/renderer/shaders/mesh_vs.glsl`
- `src/client/renderer/shaders/mesh_fs.glsl`

### Lines of Code
- Added: ~20 lines
- Changed: ~10 lines
- Total impact: Minimal, surgical change

## Related Work

This pattern is similar to:
- **Deferred shading light tile hints** - compute shader builds per-tile light lists
- **Sprite vertex lighting** - already uses voxel lookups in vertex shader
- **Forward+ rendering** - depth prepass builds screen-space light lists

## Authors

Implemented by GitHub Copilot CLI with Jay Dolan, January 2026.
