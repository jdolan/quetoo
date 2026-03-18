# R_CullBox Frustum Culling Analysis

**Date:** November 29, 2025  
**File:** `src/client/renderer/r_cull.c`

---

## Analysis Result: ✅ **Code is CORRECT** (with minor note)

After careful analysis, your `R_CullBox` implementation is **mathematically correct**.

---

## Understanding the Plane Convention

### **"Outward-Facing" Frustum Planes:**

Your comment states: *"The frustum planes are outward facing."*

This means:
- Plane normals point **away from** the visible frustum volume
- Plane normals point **toward** the culled region

### **Distance Interpretation:**

For outward-facing planes:
- **Negative distance** = Point is **behind** the plane (outside frustum) → **CULL**
- **Positive distance** = Point is **in front of** the plane (inside frustum) → **VISIBLE**
- **Zero distance** = Point is exactly on the plane

---

## Verification with R_CullSphere

The sphere test confirms this interpretation:

```cpp
const float dist = Cm_DistanceToPlane(point, plane);
if (dist < -radius) {  // Center is MORE than radius distance behind plane
  return true;         // Entire sphere is outside frustum → CULL
}
```

**Logic:**
- `dist = -5`, `radius = 2` → `-5 < -2` → **cull** ✓ (sphere is 5 units behind, fully outside)
- `dist = -1`, `radius = 2` → `-1 < -2` → **false** (sphere crosses plane, partially visible)
- `dist = +3`, `radius = 2` → `+3 < -2` → **false** (sphere is in front, visible)

This confirms: **negative distance = behind plane = outside frustum**

---

## R_CullBox Logic Analysis

### **Current Implementation:**

```cpp
for (size_t i = 0; i < lengthof(view->frustum); i++, plane++) {
  size_t j;
  for (j = 0; j < lengthof(points); j++) {
    const float dist = Cm_DistanceToPlane(points[j], plane);
    if (dist >= 0.f) {  // If ANY point is in front of or on plane
      break;            // This plane doesn't cull the box
    }
  }
  
  if (j == lengthof(points)) {  // If ALL 8 points are behind plane (dist < 0)
    return true;                 // Box is fully outside frustum → CULL
  }
}
```

### **Test Cases:**

#### **Case 1: Box fully outside (behind one plane)**
- All 8 points: `dist < 0` (all behind plane)
- Inner loop runs through all 8 points (never breaks)
- `j == 8` → **CULL** ✓

#### **Case 2: Box partially visible (some points behind, some in front)**
- Some points: `dist < 0` (behind)
- At least one point: `dist >= 0` (in front or on plane)
- Inner loop breaks when it hits the first point with `dist >= 0`
- `j < 8` → **DON'T CULL** ✓

#### **Case 3: Box fully inside frustum**
- All 8 points: `dist > 0` (all in front of all planes)
- Inner loop breaks immediately on first point for every plane
- `j == 0 or 1` for all planes → **DON'T CULL** ✓

**Conclusion:** Logic is **CORRECT** ✅

---

## Minor Consideration: The `dist >= 0.f` Check

### **Current Code:**
```cpp
if (dist >= 0.f)  // Includes dist == 0.0 (exactly on plane)
```

### **Options:**

#### **Option A: Keep as-is (current)**
```cpp
if (dist >= 0.f)
```
- **Pro:** Conservative - treats "on plane" as visible
- **Pro:** Avoids Z-fighting artifacts at boundaries
- **Con:** Might render objects that are technically outside (but edge case)

#### **Option B: Use strict inequality**
```cpp
if (dist > 0.f)
```
- **Pro:** Only treats "truly in front" as visible
- **Con:** Objects exactly on plane boundary might flicker
- **Con:** Floating-point precision issues at boundaries

#### **Option C: Use epsilon tolerance**
```cpp
if (dist > -EPSILON)  // e.g., EPSILON = 0.001f
```
- **Pro:** Handles floating-point precision issues
- **Pro:** Small objects near boundaries are more stable
- **Con:** Slightly more conservative culling

---

## Recommendation

**Keep the current code as-is: `dist >= 0.f`**

### **Rationale:**

1. ✅ **Correct:** The logic is mathematically sound
2. ✅ **Conservative:** Errs on the side of rendering (avoids popping)
3. ✅ **Stable:** Avoids flickering at plane boundaries
4. ✅ **Consistent:** Matches common frustum culling implementations
5. ✅ **Performance:** Already very efficient

### **When to use strict `> 0.f`:**
- If you're seeing performance issues from over-rendering
- If you have a second, more precise culling pass
- If you're okay with occasional boundary artifacts

### **When to use epsilon tolerance:**
- If you're seeing flickering at frustum edges
- If objects pop in/out near the screen edge
- If you have very small objects that disappear incorrectly

---

## Summary

**Status:** ✅ No bugs found - implementation is correct

**Logic:** 
- Tests all 8 corners of the bounding box
- For each frustum plane, checks if ALL points are behind (outside)
- If any point is in front of (or on) a plane, that plane doesn't cull the box
- Only culls if ALL points are behind at least one plane

**Performance:**
- Worst case: 4 planes × 8 points = 32 distance tests
- Early exit when any point is in front of a plane
- Very efficient for common cases

**Correctness:**
- Matches the outward-facing plane convention
- Consistent with `R_CullSphere` implementation  
- Follows standard frustum culling algorithm

---

## No Changes Needed

Your frustum culling implementation is solid! The only potential "improvement" would be adding epsilon tolerance if you see boundary artifacts, but the current implementation is correct and efficient as-is.

**Well done!** 🎉
