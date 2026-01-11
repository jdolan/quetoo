# Voxel-Based Light Occlusion Query Optimization

**Date:** January 2, 2026  
**Status:** Proposal  
**Impact:** Potentially significant performance improvement for light culling

---

## Current Implementation

### Light Occlusion Queries
Each `r_bsp_light_t` has an `r_occlusion_query_t` that uses the light's **conservative AABB** (`light->bounds`) as query geometry:

```c
// In r_bsp_model.c:543
light->query = R_AllocOcclusionQuery(light->bounds);
```

### Problems with Conservative AABBs

1. **Over-conservative bounds**: The light's AABB includes all voxels the light affects, creating a bounding box much larger than the actual visible geometry
2. **False visibility**: Large empty spaces inside the AABB can pass the occlusion query even when the actual lit surfaces are occluded
3. **Poor culling**: Lights behind walls often pass queries because their AABBs extend through walls into visible space

Example scenario:
```
Light in room A → AABB extends through wall into room B
Player in room B → AABB visible → Light passes query
Result: Light processed even though it's completely occluded by wall
```

---

## Proposed Solution: Voxel-Based Query Geometry

Instead of rendering a single large AABB, render **multiple smaller AABBs** - one for each voxel that the light affects.

### Key Insight

The voxel lighting system already knows exactly which voxels each light affects (stored at compile-time in `voxel->lights` hash table). Each voxel is a 32×32×32 unit cube with precise world-space bounds.

### Implementation Strategy

**Compile-time (quemap):**
- Already done: Each `voxel_t` tracks which lights affect it via `g_hash_table_add(voxel->lights, light)`
- Each light accumulates bounds via `light->bounds = Box3_Union(light->bounds, voxel->bounds)`

**Load-time (renderer):**
- For each `r_bsp_light_t`, enumerate the voxels it affects
- Store voxel AABBs instead of (or in addition to) the conservative light AABB
- Options:
  1. **Per-light voxel list**: Each light stores an array of voxel bounds
  2. **Unified voxel pool**: All lights share a pool of voxel boxes, indexed by voxel ID
  3. **Just-in-time generation**: Query voxel data at occlusion query time

**Runtime (occlusion queries):**
- Instead of `glDrawElementsBaseVertex()` with 1 box (8 vertices, 36 indices)
- Draw N boxes where N = number of voxels the light affects
- Can be done with:
  - Instanced rendering (1 draw call per light)
  - Batched geometry (1 draw call for all voxel boxes)
  - Multiple draw calls (simple but less efficient)

---

## Benefits

### 1. **Tighter Occlusion Testing**
- Query geometry closely matches actual lit surfaces
- No large empty spaces in query volume
- Better discrimination between visible/occluded lights

### 2. **Improved Culling Efficiency**
- Lights behind walls more likely to fail occlusion queries
- Fewer false positives → fewer wasted shadow map renders
- Fewer wasted fragment shader lighting calculations

### 3. **Leverages Existing Data**
- Voxel light assignment already computed at compile-time
- Voxel bounds already available in renderer
- No new data structures needed (can reuse `r_bsp_voxels_t`)

### 4. **Scalable Performance**
- Query cost scales with lit voxels, not total light count
- Typical light affects 10-50 voxels (vs 1 conservative AABB)
- GPU can early-reject occluded voxels in parallel

---

## Challenges & Considerations

### 1. **Increased Vertex Count**
- **Current**: 8 vertices per light query
- **Proposed**: 8 × N vertices where N = voxels per light
- **Mitigation**: Use instanced rendering to minimize overhead

### 2. **Data Structure**
Need efficient way to map light → voxel bounds. Options:

**Option A: Per-Light Voxel Arrays**
```c
typedef struct {
  // ... existing fields ...
  box3_t *voxel_bounds;
  int32_t num_voxel_bounds;
} r_bsp_light_t;
```
Pros: Fast access, simple
Cons: Memory duplication (voxel bounds stored per-light)

**Option B: Reference Voxel Grid**
```c
// Light stores indices into global voxel array
typedef struct {
  // ... existing fields ...
  int32_t *voxel_indices;
  int32_t num_voxels;
} r_bsp_light_t;
```
Pros: No duplication, minimal memory
Cons: Extra indirection at render time

**Option C: Reverse Lookup**
```c
// Query voxel grid: "which voxels have light N?"
// Iterate voxels.light_indices and filter by light ID
```
Pros: No new data structures
Cons: O(num_voxels × avg_lights_per_voxel) at query time - too slow

### 3. **Voxel Granularity**
- Voxels are 32×32×32 units - might be coarser than ideal for some lights
- Small lights (radius < 64) might only affect 1-2 voxels anyway
- **Optimization**: Use conservative AABB for lights affecting < N voxels (e.g., N=3)

### 4. **CPU vs GPU Cost Trade-off**
- More geometry to process on GPU
- Potentially better culling results
- Need to measure: does tighter culling outweigh increased query cost?

---

## Implementation Plan

### Phase 1: Data Structure (quemap + renderer)

1. **Add reverse mapping in quemap** (`light.c`, `voxel.c`):
   ```c
   typedef struct light_s {
     // ... existing fields ...
     GArray *voxel_indices;  // List of voxel indices this light affects
   } light_t;
   ```

2. **Emit per-light voxel lists** in `EmitLights()`:
   ```c
   // After emitting light data, write voxel indices per light
   for (each light) {
     write int32_t num_voxels
     write int32_t voxel_indices[num_voxels]
   }
   ```

3. **Load voxel indices in renderer** (`r_bsp_model.c`):
   ```c
   typedef struct {
     // ... existing r_bsp_light_t fields ...
     int32_t *voxel_indices;
     int32_t num_voxels;
   } r_bsp_light_t;
   ```

### Phase 2: Occlusion Query Rendering

1. **Modify `R_AllocOcclusionQuery()`** to support multiple boxes:
   ```c
   r_occlusion_query_t *R_AllocOcclusionQueryVoxels(const box3_t *bounds, int32_t num_bounds);
   ```

2. **Update occlusion query structure**:
   ```c
   typedef struct {
     // ... existing fields ...
     int32_t num_boxes;       // Number of boxes to render
     int32_t base_vertex;     // Base vertex for first box
   } r_occlusion_query_t;
   ```

3. **Batch render voxel boxes** in `R_DrawOcclusionQueries()`:
   ```c
   // Build vertex buffer with all voxel boxes for all lights
   for (each query with voxels) {
     for (each voxel index in light->voxel_indices) {
       box3_t voxel_bounds = GetVoxelBounds(voxel_index);
       Box3_ToPoints(voxel_bounds, vertexes + offset);
     }
   }
   
   // Render with instanced draw or multiple calls
   glDrawElementsBaseVertex(..., num_boxes * 36, ..., query->base_vertex);
   ```

### Phase 3: Optimization & Testing

1. **Implement hybrid approach**:
   ```c
   if (light->num_voxels < 3) {
     // Use conservative AABB for small lights
     query = R_AllocOcclusionQuery(light->bounds);
   } else {
     // Use voxel-based query for larger lights
     query = R_AllocOcclusionQueryVoxels(light);
   }
   ```

2. **Performance testing**:
   - Measure occlusion query time (GPU time)
   - Measure culling efficiency (`r_stats.lights_occluded` vs `r_stats.lights_visible`)
   - Test on maps with complex geometry and many lights

3. **Tuning**:
   - Adjust voxel count threshold for hybrid approach
   - Consider LOD: use fewer voxels for distant lights
   - Profile CPU cost of building voxel query geometry

---

## Expected Performance Impact

### Best Case Scenarios
- **Dense geometry with occluded lights**: 30-50% reduction in lights processed
- **Lights behind walls**: Near 100% occlusion rate (vs current false positives)
- **Complex indoor scenes**: Significant shadow map render reduction

### Worst Case Scenarios  
- **Open outdoor scenes**: Minimal benefit (most lights visible anyway)
- **Small lights**: Neutral (1-2 voxels ≈ conservative AABB)
- **High voxel count per light**: Increased query overhead might negate benefits

### Realistic Expectations
- 10-20% improvement in scenes with moderate occlusion
- Most impact on shadow rendering and lighting passes
- Bigger gains in larger maps with many static lights

---

## Alternative Approaches

### 1. **Hierarchical Occlusion Queries**
Use multiple LODs: coarse AABB first, then voxel-level if ambiguous
- Pros: Adaptive detail
- Cons: More complex, multiple query passes

### 2. **Shader-Based Culling**
Use compute shader to read occlusion results and cull lights on GPU
- Pros: No CPU involvement
- Cons: Requires compute shaders (not in OpenGL 4.1 core)

### 3. **Temporal Coherence**
Cache occlusion results across frames, update subset each frame
- Pros: Amortizes cost
- Cons: Lag in response to camera movement

---

## Next Steps

1. **Prototype** Option B (reference voxel grid) as it's memory-efficient
2. **Measure baseline** occlusion query performance with current system
3. **Implement voxel-based queries** for BSP lights only (not dynamic lights)
4. **Compare** culling efficiency and frame times
5. **Iterate** on threshold values and rendering approach

---

## Questions for Discussion

1. **Is the memory cost of per-light voxel lists acceptable?**
   - Typical light: 20 voxels × 4 bytes = 80 bytes per light
   - 100 lights: ~8KB total - probably fine

2. **Should dynamic lights use voxel queries too?**
   - Dynamic lights don't have compile-time voxel assignments
   - Could use runtime voxel lookups but adds CPU cost
   - Probably start with static lights only

3. **Alternative: voxel visibility bitmap?**
   - Each light stores a bitfield of visible voxels (size.x × size.y × size.z bits)
   - Compact but requires bit manipulation at query time
   - Possibly too complex for marginal benefit

---

## Related Documents

- `VOXEL_LIGHTING_SYSTEM.md` - Current voxel lighting implementation
- `CLUSTERED_LIGHTING_IMPLEMENTATION.md` - Voxel-based light assignment
- `r_occlude.c` - Current occlusion query system

---

**Status:** Ready for review and discussion
**Recommended:** Prototype Phase 1 to validate feasibility before full implementation
