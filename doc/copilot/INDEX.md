# Documentation Index

Quick reference for all optimization and bug fix documentation.

---

## Start Here

**[README.md](README.md)** - Complete overview of the session  
**[SESSION_SUMMARY.md](SESSION_SUMMARY.md)** - Condensed summary of all work

---

## Features

### Sky Material Stages
**[SKY_MATERIAL_STAGES.md](SKY_MATERIAL_STAGES.md)** - Scrolling/animated material stages on sky surfaces
- Two-pass rendering: cubemap + blended 2D stage overlays
- Full `stage_vertex()` support (scroll, scale, rotate, stretch, warp, pulse, color)
- Controlled by existing `r_materials` cvar

---

## Performance Optimizations

### Shadow Mapping
**[SHADOWMAP_OPTIMIZATION.md](SHADOWMAP_OPTIMIZATION.md)** - 55-85% faster  
- Removed geometry shader, 6-pass rendering
- Early sphere culling
- Depth clamping and polygon offset

### Parallax Occlusion Mapping
**[PARALLAX_OPTIMIZATION_ANALYSIS.md](PARALLAX_OPTIMIZATION_ANALYSIS.md)** - Initial analysis  
**[PARALLAX_OPTIMIZATIONS_APPLIED.md](PARALLAX_OPTIMIZATIONS_APPLIED.md)** - Work log  
**[PARALLAX_FINAL_RESULTS.md](PARALLAX_FINAL_RESULTS.md)** - 45-50% faster  
- Reduced samples (96 → 24-32)
- TBN matrix to vertex shader
- Distance-based disable

### Parallax Self-Shadow
**[PARALLAX_SELF_SHADOW_OPTIMIZATION.md](PARALLAX_SELF_SHADOW_OPTIMIZATION.md)** - 99% cost reduction  
- Bounded iterations (4-16 max)
- LOD-based quality scaling
- Smart culling at call site

---

## Bug Fixes

### Frustum Culling
**[FRUSTUM_CULLING_ANALYSIS.md](FRUSTUM_CULLING_ANALYSIS.md)** - Analysis  
**[FRUSTUM_BUG_FIX.md](FRUSTUM_BUG_FIX.md)** - Critical bugs fixed  
- Using full FOV instead of half FOV
- Sin/cos components swapped

### Entity State Management
**[ENTITY_BUG_ANALYSIS.md](ENTITY_BUG_ANALYSIS.md)** - Investigation  
**[ENTITY_STATE_BUG_FIX.md](ENTITY_STATE_BUG_FIX.md)** - Fixed stale data bug  
- Entity slot reuse validation
- Spawn ID consistency check

---

## By File Modified

### C Source Files
- `src/client/renderer/r_shadow.c` → [SHADOWMAP_OPTIMIZATION.md](SHADOWMAP_OPTIMIZATION.md)
- `src/client/renderer/r_cull.c` → [FRUSTUM_BUG_FIX.md](FRUSTUM_BUG_FIX.md)
- `src/client/renderer/r_sky.c` → [SKY_MATERIAL_STAGES.md](SKY_MATERIAL_STAGES.md)
- `src/cgame/default/cg_entity.c` → [ENTITY_STATE_BUG_FIX.md](ENTITY_STATE_BUG_FIX.md)

### GLSL Shaders
- `bsp_fs.glsl` → [PARALLAX_FINAL_RESULTS.md](PARALLAX_FINAL_RESULTS.md), [PARALLAX_SELF_SHADOW_OPTIMIZATION.md](PARALLAX_SELF_SHADOW_OPTIMIZATION.md)
- `bsp_vs.glsl` → [PARALLAX_FINAL_RESULTS.md](PARALLAX_FINAL_RESULTS.md)
- `sky_vs.glsl`, `sky_fs.glsl` → [SKY_MATERIAL_STAGES.md](SKY_MATERIAL_STAGES.md)

---

## By Performance Impact

**Massive (50%+):**
- [SHADOWMAP_OPTIMIZATION.md](SHADOWMAP_OPTIMIZATION.md) - 55-85%
- [PARALLAX_SELF_SHADOW_OPTIMIZATION.md](PARALLAX_SELF_SHADOW_OPTIMIZATION.md) - 99%

**Major (30-50%):**
- [PARALLAX_FINAL_RESULTS.md](PARALLAX_FINAL_RESULTS.md) - 45-50%

**Bug Fixes (Critical):**
- [FRUSTUM_BUG_FIX.md](FRUSTUM_BUG_FIX.md)
- [ENTITY_STATE_BUG_FIX.md](ENTITY_STATE_BUG_FIX.md)

---

## Quick Stats

- **Total files:** 11
- **Total size:** ~76 KB
- **Session duration:** 2 days
- **Performance gain:** ~2x frame rate
- **Bugs fixed:** 2 critical

---

## Usage

### For Context Restoration
```
Read doc/copilot/SESSION_SUMMARY.md first
```

### For Specific Topics
```
Check doc/copilot/SHADOWMAP_OPTIMIZATION.md for shadow details
```

### For Bug Investigation
```
Review doc/copilot/ENTITY_STATE_BUG_FIX.md
```

---

**All documentation is version controlled in the Quetoo repository.**
