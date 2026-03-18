# Blinn-Phong Lighting Analysis - bsp_fs.glsl

**Date:** December 6, 2025

## Current Implementation

### The Functions

```glsl
float blinn(in vec3 light_dir) {
  return pow(max(0.0, dot(normalize(light_dir + fragment.dir), fragment.normalmap)), fragment.specularmap.w);
}

vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
  return diffuse * fragment.specularmap.rgb * blinn(light_dir);
}
```

### Usage Context

```glsl
// Ambient contribution
fragment.ambient = pow(vec3(1.0) + sky, vec3(2.0)) * ambient * max(0.0, dot(fragment.normal, fragment.normalmap));
fragment.specular = blinn_phong(fragment.ambient, fragment.normalmap);

// Per-light contribution
float lambert = dot(dir, fragment.normalmap);
diffuse *= lambert;
fragment.diffuse += diffuse;
fragment.specular += blinn_phong(diffuse, dir);

// Final output
out_color.rgb *= (fragment.ambient + fragment.diffuse);
out_color.rgb += fragment.specular;
```

---

## Issues Found

### 🐛 **CRITICAL BUG: Incorrect Ambient Specular Calculation**

**Line 335:**
```glsl
fragment.specular = blinn_phong(fragment.ambient, fragment.normalmap);
```

**Problem:** Passing `fragment.normalmap` as the `light_dir` parameter is **incorrect**. The normal is not a light direction!

**What happens:**
```glsl
// Inside blinn():
normalize(fragment.normalmap + fragment.dir)
// This creates a half-vector between the surface normal and view direction
// Instead of between a light direction and view direction
```

**Impact:** Ambient specular highlights will be positioned incorrectly and will look strange or broken.

**Fix:**
```glsl
// Option 1: Remove ambient specular (most common for skybox ambient)
fragment.specular = vec3(0.0);

// Option 2: Use a dominant light direction (e.g., sky direction)
vec3 sky_dir = normalize(vec3(0.0, 0.0, 1.0)); // Up vector as "sun"
fragment.specular = blinn_phong(fragment.ambient, sky_dir);

// Option 3: Use view direction reflection for image-based lighting
vec3 reflect_dir = reflect(-fragment.dir, fragment.normalmap);
// Sample cubemap and compute specular from that
```

---

### ⚠️ **Issue: Specular Energy Multiplied by Diffuse**

**Current code:**
```glsl
vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
  return diffuse * fragment.specularmap.rgb * blinn(light_dir);
}
```

**Problem:** Multiplying specular by `diffuse` (which includes Lambert term) is **physically incorrect**.

**Why it's wrong:**
- Diffuse and specular are independent reflection types
- Diffuse = Lambert × light_color × albedo
- Specular = Blinn-Phong × light_color × specular_color
- Specular should NOT be multiplied by Lambert!

**What this causes:**
- Specular highlights disappear on surfaces facing away from light
- Specular at grazing angles is too dim
- Not energy-conserving

**Current flow:**
```glsl
float lambert = dot(dir, fragment.normalmap);  // 0.0 to 1.0
diffuse *= lambert;  // Attenuate by Lambert

// Then specular:
fragment.specular += blinn_phong(diffuse, dir);
// This multiplies specular by lambert, which is WRONG!
```

**Fix:**
```glsl
// Option 1: Pass light color separately (BEST)
vec3 blinn_phong(in vec3 light_color, in vec3 light_dir) {
  return light_color * fragment.specularmap.rgb * blinn(light_dir);
}

// Usage:
float lambert = dot(dir, fragment.normalmap);
if (lambert > 0.0) {
  vec3 light_contrib = light.color.rgb * light.color.a * atten;
  fragment.diffuse += light_contrib * lambert;
  fragment.specular += blinn_phong(light_contrib, dir);
}

// Option 2: Pass pre-Lambert light if you want (acceptable)
vec3 light_contrib = light.color.rgb * light.color.a * atten;
fragment.diffuse += light_contrib * lambert;
fragment.specular += light_contrib * fragment.specularmap.rgb * blinn(dir);
```

---

### ⚠️ **Issue: Double Normalization in blinn()**

**Current code:**
```glsl
float blinn(in vec3 light_dir) {
  return pow(max(0.0, dot(normalize(light_dir + fragment.dir), fragment.normalmap)), ...);
}
```

**Problem:** You're normalizing `light_dir + fragment.dir`, but:
- `fragment.dir` is already normalized (line 380: `fragment.dir = normalize(-vertex.position)`)
- `light_dir` is already normalized (line 290: `dir = normalize(view * vec4(dir, 0.0)).xyz`)

**Impact:** 
- **WAIT, this is actually CORRECT!**
- The sum of two unit vectors is NOT a unit vector
- You MUST normalize the sum to get the proper half-vector
- **This is fine, no issue here!**

---

### 💡 **Quality Improvement: Missing N·L Clamping in Specular**

**Current code:**
```glsl
float lambert = dot(dir, fragment.normalmap);
if (lambert <= 0.0) {
  return;  // Early exit
}
// ... later ...
fragment.specular += blinn_phong(diffuse, dir);
```

**Good:** The early exit prevents negative Lambert values from contributing.

**However:** The Blinn-Phong calculation doesn't check if N·L > 0 at the mathematical level. While you early-exit, it's cleaner to explicitly ensure this.

**Better approach:**
```glsl
float blinn(in vec3 light_dir, in float n_dot_l) {
  if (n_dot_l <= 0.0) return 0.0;  // No specular if light is behind surface
  
  vec3 half_vec = normalize(light_dir + fragment.dir);
  float n_dot_h = max(0.0, dot(half_vec, fragment.normalmap));
  return pow(n_dot_h, fragment.specularmap.w);
}
```

**Current code is acceptable** since you early-exit, but explicit checking is cleaner.

---

### 💡 **Quality Improvement: Specular Fresnel Effect Missing**

**Current approach:** Raw Blinn-Phong without Fresnel

**Issue:** Physically, specular reflections increase at grazing angles (Fresnel effect). Your current implementation has uniform specular regardless of view angle.

**Improvement:** Add Schlick's Fresnel approximation:

```glsl
vec3 fresnel_schlick(float cos_theta, vec3 F0) {
  return F0 + (1.0 - F0) * pow(1.0 - cos_theta, 5.0);
}

float blinn(in vec3 light_dir) {
  vec3 half_vec = normalize(light_dir + fragment.dir);
  float n_dot_h = max(0.0, dot(half_vec, fragment.normalmap));
  return pow(n_dot_h, fragment.specularmap.w);
}

vec3 blinn_phong_fresnel(in vec3 light_color, in vec3 light_dir) {
  vec3 half_vec = normalize(light_dir + fragment.dir);
  float v_dot_h = max(0.0, dot(fragment.dir, half_vec));
  
  // F0 = specular color (typically 0.04 for dielectrics, higher for metals)
  vec3 F0 = mix(vec3(0.04), fragment.diffusemap.rgb, fragment.specularmap.r);
  vec3 F = fresnel_schlick(v_dot_h, F0);
  
  return light_color * F * blinn(light_dir);
}
```

**Impact:** More realistic specular highlights with proper grazing angle brightening.

---

### 💡 **Quality Improvement: Energy Conservation**

**Current approach:** Diffuse and specular are added together without energy conservation.

**Issue:** A surface can't reflect more light than it receives. Current code can result in overbright surfaces.

**Improvement:** Make diffuse and specular energy-conserving:

```glsl
// Standard PBR approach:
vec3 kS = fresnel_schlick(...);  // Specular contribution
vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);  // Diffuse contribution

fragment.diffuse += kD * light_color * lambert;
fragment.specular += kS * light_color * blinn(light_dir);
```

**Trade-off:** Requires metallic parameter and changes the look. May not be desired for Quake-style aesthetic.

---

## Recommendations

### Priority 1: FIX CRITICAL BUG ⚠️

**Fix the ambient specular calculation:**

```glsl
// BEFORE (line 335):
fragment.specular = blinn_phong(fragment.ambient, fragment.normalmap);  // WRONG!

// AFTER (simplest fix - remove ambient specular):
fragment.specular = vec3(0.0);

// OR (better - use sky direction):
vec3 sky_dir = normalize(vec3(0.0, 0.0, 1.0));  // Or extract from skybox
fragment.specular = blinn_phong(fragment.ambient, sky_dir);
```

### Priority 2: FIX SPECULAR ENERGY

**Decouple specular from Lambert term:**

```glsl
// In light_and_shadow_light():
vec3 light_contrib = light.color.rgb * light.color.a * atten;

float lambert = dot(dir, fragment.normalmap);
if (lambert <= 0.0) {
  return;
}

fragment.diffuse += light_contrib * lambert;
fragment.specular += blinn_phong(light_contrib, dir);  // Pass light_contrib, not diffuse!
```

**Update blinn_phong signature:**

```glsl
vec3 blinn_phong(in vec3 light_color, in vec3 light_dir) {
  return light_color * fragment.specularmap.rgb * blinn(light_dir);
}
```

### Priority 3: OPTIONAL QUALITY IMPROVEMENTS

**Add Fresnel effect** (medium impact on visual quality):
- More realistic specular highlights
- Grazing angle brightening
- ~10-15 extra instructions per light

**Add energy conservation** (large impact on realism):
- Prevents overbright surfaces
- More physically accurate
- May change artistic look significantly

---

## Summary

| Issue | Severity | Impact | Fix Complexity |
|-------|----------|--------|----------------|
| Ambient specular uses normal as light dir | **CRITICAL** | Broken ambient specular | Easy |
| Specular multiplied by Lambert (diffuse) | **HIGH** | Dim/incorrect specular | Medium |
| Missing Fresnel effect | Low | Less realistic highlights | Medium |
| No energy conservation | Low | Can overbright | Medium |

**Recommended action:** Fix the critical bug and the specular/Lambert issue. The quality improvements are optional depending on your desired visual style.

---

## Code Diff for Critical Fixes

```diff
@@ -239,7 +239,7 @@
  */
-vec3 blinn_phong(in vec3 diffuse, in vec3 light_dir) {
-  return diffuse * fragment.specularmap.rgb * blinn(light_dir);
+vec3 blinn_phong(in vec3 light_color, in vec3 light_dir) {
+  return light_color * fragment.specularmap.rgb * blinn(light_dir);
 }

@@ -287,13 +287,15 @@
   float atten = clamp(1.0 - length(dir) / radius, 0.0, 1.0);
   if (atten <= 0.0) {
     return;
   }
 
-  vec3 diffuse = light.color.rgb * light.color.a * atten * modulate;
+  vec3 light_contrib = light.color.rgb * light.color.a * atten * modulate;
 
   dir = normalize(view * vec4(dir, 0.0)).xyz;
 
   float lambert = dot(dir, fragment.normalmap);
   if (lambert <= 0.0) {
     return;
   }
 
-  diffuse *= lambert;
-
   float shadow = sample_shadow_cubemap_array(light, index);
 
   if (fragment.lod < 4.0 && material.shadow > 0.0) {
     shadow *= parallax_self_shadow(dir);
   }
 
-  diffuse *= shadow;
+  light_contrib *= shadow;
 
-  fragment.diffuse += diffuse;
-  fragment.specular += blinn_phong(diffuse, dir);
+  fragment.diffuse += light_contrib * lambert;
+  fragment.specular += blinn_phong(light_contrib, dir);
 }

@@ -335,7 +337,8 @@
   vec3 sky = textureLod(texture_sky, normalize(vertex.cubemap), 6).rgb;
   fragment.ambient = pow(vec3(1.0) + sky, vec3(2.0)) * ambient * max(0.0, dot(fragment.normal, fragment.normalmap));
-  fragment.specular = blinn_phong(fragment.ambient, fragment.normalmap);
+  
+  fragment.specular = vec3(0.0);  // No ambient specular (or use sky_dir if preferred)
 
   fragment.diffuse = vec3(0);
```

