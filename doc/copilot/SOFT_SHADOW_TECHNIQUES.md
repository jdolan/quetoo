# Soft Shadow Techniques for Cubemap Shadows

**Date:** December 7, 2025  
**Target:** Reduce jaggies and artifacts from low-resolution shadow cubemaps

---

## Current Implementation Analysis

### What You Have:
- **Shadow cubemap arrays** with configurable resolution (`r_shadow_cubemap_array_size`)
- **Hardware PCF** enabled via `GL_TEXTURE_COMPARE_MODE`
- **Point sampling** with `GL_LINEAR` filter (bilinear PCF - 2x2 samples)
- **Depth bias** computed as `length(shadowmap.xyz) / depth_range.y`
- **Front-face culling** during shadow rendering (reduces peter-panning)
- **Polygon offset** (2.0, 4.0) for shadow acne reduction

### Current Issues:
- Low shadow map resolution causes **hard edges** and **aliasing**
- **Single bilinear PCF sample** not enough for soft shadows
- **Visible pixelation** on curved surfaces
- **Jagged shadow edges** especially at grazing angles

---

## Recommended Techniques (Ordered by Impact/Cost Ratio)

### ⭐ **1. Percentage-Closer Filtering (PCF) with Multiple Samples**

**Impact:** High | **Cost:** Medium | **Difficulty:** Easy

Replace single hardware PCF with manual PCF using multiple samples.

#### Implementation:

```glsl
// Current (1 sample with hardware PCF):
float sample_shadow_cubemap_array(in light_t light, in int index) {
  vec4 shadowmap = vec4(vertex.model - light.origin.xyz, layer);
  float bias = length(shadowmap.xyz) / depth_range.y;
  return texture(texture_shadow_cubemap_array0, shadowmap, bias);
}

// New (4-16 samples with Poisson disk):
const vec3 poisson_disk[16] = vec3[](
  vec3( 0.2770745,  0.6951455,  0.6638531),
  vec3(-0.5932785, -0.1203284,  0.7954771),
  vec3( 0.4494750,  0.2469098, -0.8414080),
  vec3(-0.1460639, -0.5679667, -0.8098297),
  vec3( 0.6400498, -0.4071948,  0.6527679),
  vec3(-0.3631914,  0.7935778, -0.4889667),
  vec3( 0.1248857, -0.8975238,  0.4223446),
  vec3(-0.7720318,  0.4438458,  0.4568679),
  vec3( 0.8851806,  0.1653373, -0.4349109),
  vec3(-0.5238012, -0.7260296,  0.4437121),
  vec3( 0.3642682,  0.5968054,  0.7145538),
  vec3(-0.8331701, -0.3328346, -0.4437121),
  vec3( 0.5527260, -0.6985809, -0.4568679),
  vec3(-0.2407123,  0.3153157,  0.9181205),
  vec3( 0.7269405, -0.1430640,  0.6718765),
  vec3(-0.6444675,  0.6444675, -0.4076138)
);

float sample_shadow_cubemap_array_pcf(in light_t light, in int index, in float radius) {
  int array = index / MAX_SHADOW_CUBEMAP_LAYERS;
  int layer = index % MAX_SHADOW_CUBEMAP_LAYERS;
  
  vec3 light_to_frag = vertex.model - light.origin.xyz;
  float current_depth = length(light_to_frag) / depth_range.y;
  
  float shadow = 0.0;
  int samples = 16;  // Can be made dynamic based on quality settings
  
  for (int i = 0; i < samples; i++) {
    vec3 sample_dir = light_to_frag + poisson_disk[i] * radius;
    vec4 shadowmap = vec4(sample_dir, layer);
    
    switch (array) {
      case 0:
        shadow += texture(texture_shadow_cubemap_array0, shadowmap, current_depth);
        break;
      // ... other cases
    }
  }
  
  return shadow / float(samples);
}
```

**Benefits:**
- Soft shadow edges (penumbra effect)
- Reduces jaggies significantly
- Configurable quality (4, 9, 16, or 25 samples)

**Cost:** 4-16x more texture fetches (but can use LOD or distance-based sample count)

---

### ⭐⭐ **2. Vogel Disk or Rotated Poisson Disk (Prevents Banding)**

**Impact:** High | **Cost:** Low additional | **Difficulty:** Easy

Add per-pixel rotation to Poisson samples to eliminate banding patterns.

```glsl
// Add noise function:
float random(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453);
}

// In PCF loop:
float angle = random(vertex.model) * 6.283185;  // 2*PI
mat3 rotation = mat3(
  cos(angle), -sin(angle), 0,
  sin(angle),  cos(angle), 0,
  0,           0,          1
);

vec3 rotated_sample = rotation * poisson_disk[i];
vec3 sample_dir = light_to_frag + rotated_sample * radius;
```

**Benefits:**
- Eliminates repeating patterns
- More natural-looking shadows
- Minimal cost

**Alternative:** Use `interleavedGradientNoise()` for temporal stability.

---

### ⭐⭐⭐ **3. Distance-Based Filter Radius (Physically-Based Soft Shadows)**

**Impact:** Very High | **Cost:** None | **Difficulty:** Easy

Vary PCF sample radius based on distance from light and occluder.

```glsl
// Penumbra size estimation
float estimate_penumbra_size(in light_t light, in vec3 light_to_frag) {
  float light_size = light.origin.w * 0.01;  // Treat radius as light size (1% of radius)
  float blocker_dist = length(light_to_frag);
  float receiver_dist = blocker_dist;  // Simplified; ideally sample blocker distance
  
  // Similar triangles: penumbra = light_size * (receiver - blocker) / blocker
  return light_size * (receiver_dist / blocker_dist);
}

// In sampling function:
float penumbra = estimate_penumbra_size(light, light_to_frag);
float filter_radius = penumbra * 0.05;  // Scale factor for texel units
```

**Benefits:**
- Realistic soft shadows (large penumbra far from light, sharp close to occluder)
- Free once you have PCF implemented
- Physically motivated

---

### ⭐ **4. PCSS (Percentage-Closer Soft Shadows) - Full Version**

**Impact:** Very High | **Cost:** High | **Difficulty:** Medium

Two-pass approach: blocker search + adaptive PCF.

```glsl
// Pass 1: Find average blocker distance
float find_blocker_distance(vec3 light_to_frag, int layer, int array, float search_radius) {
  float blocker_sum = 0.0;
  int blocker_count = 0;
  float current_depth = length(light_to_frag);
  
  for (int i = 0; i < 16; i++) {
    vec3 sample_dir = light_to_frag + poisson_disk[i] * search_radius;
    vec4 shadowmap = vec4(sample_dir, layer);
    
    float depth = textureGather(texture_shadow_cubemap_array0, shadowmap).r * depth_range.y;
    if (depth < current_depth) {
      blocker_sum += depth;
      blocker_count++;
    }
  }
  
  return blocker_count > 0 ? blocker_sum / float(blocker_count) : -1.0;
}

// Pass 2: PCF with adaptive radius
float sample_shadow_pcss(in light_t light, in int index) {
  vec3 light_to_frag = vertex.model - light.origin.xyz;
  
  float blocker_dist = find_blocker_distance(light_to_frag, layer, array, 0.05);
  if (blocker_dist < 0.0) return 1.0;  // No blockers
  
  float receiver_dist = length(light_to_frag);
  float light_size = light.origin.w * 0.02;
  float penumbra = light_size * (receiver_dist - blocker_dist) / blocker_dist;
  float filter_radius = penumbra * 0.01;
  
  // Now do PCF with adaptive radius
  return sample_shadow_cubemap_array_pcf(light, index, filter_radius);
}
```

**Benefits:**
- Contact-hardening shadows (sharp near occluder, soft far away)
- Most realistic approach
- Industry standard for AAA games

**Cost:** ~32 texture fetches (16 blocker + 16 PCF)

---

### ⭐ **5. ESM (Exponential Shadow Maps)**

**Impact:** High | **Cost:** Medium | **Difficulty:** Medium

Store `exp(c * depth)` instead of depth, enables gaussian blur.

**Pros:**
- Very soft shadows via gaussian blur (separable, fast)
- No sampling required in shader

**Cons:**
- Light bleeding artifacts
- Requires additional blur pass
- Precision issues with large `c` values

**Skip this unless:** You want extremely soft shadows and can tolerate artifacts.

---

### ⭐ **6. Increase Shadow Map Resolution**

**Impact:** High | **Cost:** Memory/Performance | **Difficulty:** Trivial

Simply increase `r_shadow_cubemap_array_size`.

```c
// If currently 512:
r_shadow_cubemap_array_size = 1024;  // 4x memory, ~2x render cost
```

**Benefits:**
- Reduces aliasing directly
- Works with all other techniques

**Cost:**
- 512 → 1024: 4x memory per light
- 512 → 2048: 16x memory per light

**Recommendation:** Combine with other techniques rather than using this alone.

---

## Recommended Implementation Plan

### **Phase 1: Quick Win (1-2 hours)**

Implement **basic PCF with Poisson disk**:

1. Add Poisson disk constant array
2. Replace single `texture()` call with 9-16 sample loop
3. Add distance-based sample count (close = 16, far = 4)
4. Add per-pixel rotation for banding reduction

**Expected result:** 60-80% softer shadows, noticeable quality improvement

---

### **Phase 2: Physical Accuracy (2-3 hours)**

Add **distance-based filter radius**:

1. Estimate penumbra size from light radius and fragment distance
2. Scale PCF sample radius accordingly
3. Tune light radius multiplier for desired softness

**Expected result:** Realistic soft shadow falloff

---

### **Phase 3: High Quality (4-6 hours)**

Implement **PCSS (optional)**:

1. Add blocker search pass
2. Compute adaptive filter radius
3. Add quality settings (low/med/high sample counts)

**Expected result:** Contact-hardening shadows like AAA games

---

## Additional Optimizations

### **A. Distance-Based Quality Scaling**

```glsl
float dist = length(vertex.position);
int samples = dist < 500.0 ? 16 : (dist < 1000.0 ? 9 : 4);
```

### **B. Light Size Parameter**

Add to `r_light_t` structure:
```c
struct r_light_t {
  // ...existing...
  float size;  // Physical size for soft shadow penumbra
};
```

### **C. Temporal Anti-Aliasing (TAA)**

Use lower sample counts (4-9) and rely on TAA to smooth temporal noise.

### **D. Reduce Polygon Offset Aggressiveness**

```c
// Current:
glPolygonOffset(2.0f, 4.0f);

// Try:
glPolygonOffset(1.5f, 2.0f);  // Less shadow acne offset = sharper contact
```

---

## Code Structure Changes Needed

### New Shader Code:

**Files to modify:**
- `src/client/renderer/shaders/bsp_fs.glsl`
- `src/client/renderer/shaders/mesh_fs.glsl`

**New functions:**
- `sample_shadow_cubemap_array_pcf()`
- `estimate_penumbra_size()`
- Optional: `find_blocker_distance()` for PCSS

### New CVars:

```c
cvar_t *r_shadow_pcf_samples;       // 4, 9, 16, 25
cvar_t *r_shadow_filter_radius;     // Base filter radius multiplier
cvar_t *r_shadow_light_size;        // Light size multiplier for penumbra
cvar_t *r_shadow_pcss;              // Enable PCSS (0/1)
```

---

## Expected Performance Impact

| Technique | GPU Cost | Visual Improvement | Memory | Recommendation |
|-----------|----------|-------------------|--------|----------------|
| 4-sample PCF | +30% | Good | 0 | ✅ Always |
| 9-sample PCF | +80% | Very Good | 0 | ✅ Default |
| 16-sample PCF | +150% | Excellent | 0 | Medium/High settings |
| 25-sample PCF | +250% | Excellent+ | 0 | Ultra settings |
| Rotation | +5% | Good (no banding) | 0 | ✅ Always |
| Distance radius | 0% | Very Good | 0 | ✅ Always |
| PCSS | +200-300% | Excellent | 0 | Optional/Ultra |
| 1024 resolution | +100% render | Very Good | 4x | If memory allows |

---

## Summary

**Minimum viable improvement:**
1. ✅ 9-sample PCF with Poisson disk
2. ✅ Per-pixel rotation
3. ✅ Distance-based filter radius

**This gives you:**
- Soft shadow edges
- No banding artifacts  
- Realistic penumbra
- ~80% GPU cost increase (acceptable for quality gain)

**Result:** Professional-looking soft shadows that hide low shadow map resolution effectively.

