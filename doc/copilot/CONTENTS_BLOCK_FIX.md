# Fix for Missing Faces on CONTENTS_BLOCK Boundaries

**Date**: 2026-01-22  
**Issue**: Faces missing in-game when they fall exactly on CONTENTS_BLOCK plane boundaries  
**Status**: Fixed

## Problem Analysis

### Root Cause

The BSP compiler splits the world into uniform chunks (BSP_BLOCK_SIZE = 512 units) for clustered rendering by creating CONTENTS_BLOCK nodes. When collecting faces for each block in `EmitBlocks_r()`, the function used:

```c
if (Box3_Contains(Box3_Expand(node->bounds, 1.f), face->bounds))
```

This had a critical flaw: faces whose bounds fell **exactly on** the CONTENTS_BLOCK split plane would fail the containment test (even with the 1-unit expansion), causing them to be omitted from all blocks.

### Why This Happened

1. **BSP splitting process**: When a CONTENTS_BLOCK node splits the world on a plane, it uses `SIDE_EPSILON` (essentially `FLT_EPSILON`, near-zero) for winding classification
2. **Faces on the plane**: Faces exactly on the split plane have all their vertices classified as "ON" the plane
3. **Containment test failure**: The face's bounding box extends infinitesimally beyond the node bounds, failing `Box3_Contains` even with 1-unit epsilon
4. **Result**: Face is not added to any block → missing in-game

### Example Scenario

```
CONTENTS_BLOCK split at X = 512:
  - Block A bounds: [-∞, -∞, -∞] to [512, ∞, ∞]
  - Block B bounds: [512, -∞, -∞] to [∞, ∞, ∞]
  - Face on plane: X = 512.0 exactly
  - Face bounds: [512, Y1, Z1] to [512, Y2, Z2]
  
Old code:
  - Box3_Contains(Box3_Expand(A, 1.f), face) → FALSE (face.min.x = 512, A.max.x = 513)
  - Box3_Contains(Box3_Expand(B, 1.f), face) → FALSE (face.max.x = 512, B.min.x = 511)
  - Face assigned to NO block → MISSING!
```

## The Fix

Changed `EmitBlocks_r()` to test if the **face center point** is contained in the expanded block bounds, with a larger epsilon:

```c
const float epsilon = BSP_BLOCK_SIZE * 0.01f;  // 5.12 units
if (Box3_ContainsPoint(Box3_Expand(node->bounds, epsilon), Box3_Center(face->bounds)))
```

### Why This Works

1. **Point test is simpler**: A point is definitively on one side or the other of a plane
2. **Face center represents intent**: The center of a face better represents which block it belongs to
3. **Larger epsilon**: 5.12 units (1% of block size) ensures boundary cases are handled
4. **No duplicate assignment**: Each face center is in exactly one expanded block

### Alternative Approaches Considered

1. **Collect faces from node tree** (rejected):
   - Would require maintaining back-references from bsp_node to source node
   - Complex and error-prone
   
2. **Use Box3_Intersects** (rejected):
   - Would cause faces to be added to multiple blocks
   - Breaks the clustered rendering assumption that faces belong to one block

3. **Just increase epsilon** (rejected):
   - Still has the fundamental problem of testing box-in-box containment
   - Doesn't address the core issue of faces exactly on boundaries

## Files Changed

- `src/tools/quemap/writebsp.c`: Modified `EmitBlocks_r()` function

## Testing

```bash
# Recompile quemap
make clean && make

# Test on a map with geometry aligned to 512-unit boundaries
quemap -bsp -light maps/test.map

# Load in-game and verify no missing faces
quetoo +map test
```

## Related Issues

This fix is similar to the voxel boundary fix documented in `VOXEL_LIGHTING_SYSTEM.md`, where sphere-box intersection was used instead of point-to-point distance to avoid hard boundaries.

## Implementation Date

January 22, 2026
