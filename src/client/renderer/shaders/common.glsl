/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec2 v) {
  return max(v.x, v.y);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec3 v) {
  return max(max(v.x, v.y), v.z);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec4 v) {
  return max(max(v.x, v.y), max(v.z, v.w));
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec2 v) {
  return min(v.x, v.y);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec3 v) {
  return min(min(v.x, v.y), v.z);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec4 v) {
  return min(min(v.x, v.y), min(v.z, v.w));
}

/**
 * @brief Inverse of v.
 */
vec3 invert(vec3 v) {
  return vec3(1.0) - v;
}

/**
 * @brief Clamps to [0.0, 1.0], like in HLSL.
 */
float saturate(float x) {
  return clamp(x, 0.0, 1.0);
}

/**
 * @brief Common vertex data structure.
 * @details Used by both BSP and mesh shaders. Not all fields are used by all shader types.
 */
struct common_vertex_t {
  vec3 model_position;   // World-space position
  vec3 model_normal;     // World-space normal
  vec3 position;         // View-space position
  vec3 normal;           // View-space normal
  vec3 smooth_normal;    // View-space smooth normal (mesh only)
  vec3 tangent;          // View-space tangent
  vec3 bitangent;        // View-space bitangent
  mat3 tbn;              // Tangent-to-view matrix
  mat3 inverse_tbn;      // View-to-tangent matrix (BSP only)
  vec2 diffusemap;       // Diffuse texture coordinates
  vec3 voxel;            // Voxel texture coordinates
  vec4 color;            // Vertex color
  vec3 ambient;          // Ambient lighting
  float caustics;        // Caustics intensity
  vec4 fog;              // Volumetric fog (rgb = color, a = density)
  vec3 lighting;         // Pre-calculated vertex lighting
};

/**
 * @brief Common fragment data structure.
 * @details Used by both BSP and mesh shaders. Not all fields are used by all shader types.
 */
struct common_fragment_t {
  vec3 view_dir;         // View direction (normalized, pointing toward camera)
  float view_dist;       // Distance from camera
  float lod;             // Texture LOD level (BSP only)
  vec3 normal;           // View-space normal
  vec3 tangent;          // View-space tangent
  vec3 bitangent;        // View-space bitangent
  mat3 tbn;              // Tangent-to-view matrix
  vec2 parallax;         // Parallax-offset texture coordinates (BSP only)
  vec4 diffuse_sample;   // Diffuse texture sample
  vec3 normal_sample;    // Normal map sample (world/view space)
  vec4 specular_sample;  // Specular texture sample (rgb = color, a = gloss)
  vec3 ambient;          // Ambient lighting contribution
  vec3 diffuse;          // Diffuse lighting contribution
  float caustics;        // Caustics contribution (BSP only)
  vec3 specular;         // Specular lighting contribution
  vec4 fog;              // Fog contribution (BSP only)
};

/**
 * @brief Clamp value t to range [a,b] and map [a,b] to [0,1].
 */
float linearstep(float a, float b, float t) {
  return clamp((t - a) / (b - a), 0.0, 1.0);
}

/**
 * @brief
 */
vec3 hash33(vec3 p) {
  p = fract(p * vec3(.1031,.11369,.13787));
  p += dot(p, p.yxz + 19.19);
  return -1.0 + 2.0 * fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

/**
 * @brief https://www.shadertoy.com/view/4sc3z2
 */
float noise3d(vec3 p) {
  vec3 pi = floor(p);
  vec3 pf = p - pi;

  vec3 w = pf * pf * (3.0 - 2.0 * pf);

  return mix(
    mix(
      mix(dot(pf - vec3(0, 0, 0), hash33(pi + vec3(0, 0, 0))),
        dot(pf - vec3(1, 0, 0), hash33(pi + vec3(1, 0, 0))),
        w.x),
      mix(dot(pf - vec3(0, 0, 1), hash33(pi + vec3(0, 0, 1))),
        dot(pf - vec3(1, 0, 1), hash33(pi + vec3(1, 0, 1))),
        w.x),
      w.z),
    mix(
      mix(dot(pf - vec3(0, 1, 0), hash33(pi + vec3(0, 1, 0))),
        dot(pf - vec3(1, 1, 0), hash33(pi + vec3(1, 1, 0))),
        w.x),
      mix(dot(pf - vec3(0, 1, 1), hash33(pi + vec3(0, 1, 1))),
        dot(pf - vec3(1, 1, 1), hash33(pi + vec3(1, 1, 1))),
        w.x),
      w.z),
    w.y);
}

/**
 * @brief Calculates a lookAt matrix for the given parameters.
 * @see Mat4_LookAt
 */
mat4 lookAt(vec3 eye, vec3 pos, vec3 up) {

  vec3 Z = normalize(eye - pos);
  vec3 X = normalize(cross(up, Z));
  vec3 Y = normalize(cross(Z, X));

  return mat4(
    vec4(X.x, Y.x, Z.x, 0.0),
    vec4(X.y, Y.y, Z.y, 0.0),
    vec4(X.z, Y.z, Z.z, 0.0),
    vec4(-dot(X, eye), -dot(Y, eye), -dot(Z, eye), 1.0)
  );
}

/**
 * @brief Poisson disk samples for PCF soft shadows.
 */
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

/**
 * @brief Simple pseudo-random function for per-pixel rotation.
 */
float random_angle(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.283185;
}

/**
 * @brief Rotate a 3D vector around a normalized axis with precomputed sin/cos.
 */
vec3 rotate_around_axis(vec3 v, vec3 axis, float s, float c) {
  float oc = 1.0 - c;

  return vec3(
    (oc * axis.x * axis.x + c) * v.x + (oc * axis.x * axis.y - axis.z * s) * v.y + (oc * axis.z * axis.x + axis.y * s) * v.z,
    (oc * axis.x * axis.y + axis.z * s) * v.x + (oc * axis.y * axis.y + c) * v.y + (oc * axis.y * axis.z - axis.x * s) * v.z,
    (oc * axis.z * axis.x - axis.y * s) * v.x + (oc * axis.y * axis.z + axis.x * s) * v.y + (oc * axis.z * axis.z + c) * v.z
  );
}

/**
 * @brief Soft shadow sampling with PCF and Poisson disk.
 * @param light The light to sample shadows for.
 * @param index The light index in the shadow cubemap array.
 * @param v Vertex data.
 * @param f Fragment data.
 * @return Shadow term (0.0 = fully shadowed, 1.0 = fully lit).
 */
float sample_shadow_cubemap_array(in light_t light, in int index, in common_vertex_t v, in common_fragment_t f) {

  vec3 light_to_frag = v.model_position - light.origin.xyz;
  float current_depth = length(light_to_frag) / depth_range.y;

  // Estimate penumbra size based on light radius (treat as light size)
  float light_size = light.origin.w * 3.0; // 3x radius term used for penumbra heuristic
  float dist_to_light = length(light_to_frag);
  float penumbra = light_size * (dist_to_light / light.origin.w);
  float filter_radius = penumbra * 0.005;  // Scale to texel units

  // Distance-based sample count (close = more samples, far = fewer)
  int num_samples = f.view_dist < 512.0 ? 16 : (f.view_dist < 1024.0 ? 8 : 4);

  // Per-pixel rotation to eliminate banding
  vec3 rotation_axis = normalize(light_to_frag);
  float angle = random_angle(v.model_position);
  float s = sin(angle);
  float c = cos(angle);

  float shadow = 0.0;

  for (int i = 0; i < num_samples; i++) {
    // Rotate Poisson sample to eliminate banding patterns
    vec3 rotated_offset = rotate_around_axis(poisson_disk[i], rotation_axis, s, c);
    vec3 sample_dir = light_to_frag + rotated_offset * filter_radius;
    vec4 shadowmap = vec4(sample_dir, index);

    shadow += texture(texture_shadow_cubemap_array, shadowmap, current_depth);
  }

  return shadow / float(num_samples);
}

/**
 * @brief Blinn specular term.
 * @param light_dir Light direction in view space.
 * @param f Fragment data.
 * @return Specular intensity.
 */
float blinn(in vec3 light_dir, in common_fragment_t f) {
  return pow(max(0.0, dot(normalize(light_dir + f.view_dir), f.normal_sample)), f.specular_sample.w);
}

/**
 * @brief Blinn-Phong specular lighting.
 * @param light_color Light color and intensity.
 * @param light_dir Light direction in view space.
 * @param f Fragment data.
 * @return Specular contribution.
 */
vec3 blinn_phong(in vec3 light_color, in vec3 light_dir, in common_fragment_t f) {
  return light_color * f.specular_sample.rgb * blinn(light_dir, f);
}

/**
 * @brief Toksvig specular anti-aliasing.
 * @details Adjusts gloss based on normal map variance to reduce specular aliasing.
 * @param normal Normal map sample (unnormalized).
 * @param power Specular power.
 * @return Adjusted gloss multiplier.
 */
float toksvig_gloss(in vec3 normal, in float power) {
  float len_rcp = 1.0 / saturate(length(normal));
  return 1.0 / (1.0 + power * (len_rcp - 1.0));
}

