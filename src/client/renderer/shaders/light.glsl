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
 * @file light.glsl
 * @brief Per-fragment lighting and shadow functions.
 */

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
 * @brief Sample the shadow cubemap array with PCF filtering.
 * @param light The light source.
 * @param index The light index.
 * @param v Vertex data.
 * @param f Fragment data.
 * @return Shadow term (0 = fully shadowed, 1 = fully lit).
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
 * @brief Parallax occlusion self-shadowing.
 * @details Raymarches along light direction through heightmap to compute shadows.
 * @param light_dir Light direction in view space.
 * @return Shadow term (0 = fully shadowed, 1 = fully lit).
 */
float parallax_self_shadow(in vec3 light_dir, in common_vertex_t v, in common_fragment_t f) {

  // If the light angle is nearly perpendicular, skip self-shadowing
  if (dot(light_dir, normalize(v.tbn[2])) > 0.9) {
    return 1.0;
  }

  // LOD-based step count: 16 close, 4 far (bounded for performance)
  int max_steps = int(mix(16.0, 4.0, min(f.lod * 0.33, 1.0)));

  // Adaptive step size based on LOD (larger steps at distance)
  float step_scale = mix(1.0, 2.5, min(f.lod * 0.5, 1.0));

  vec2 texel = 1.0 / textureSize(texture_material, 0).xy;
  vec3 dir = normalize(v.inverse_tbn * light_dir);
  vec3 delta = vec3(dir.xy * texel, max(dir.z * length(texel), .01)) * step_scale;
  vec3 texcoord = vec3(f.parallax, sample_material_heightmap(f.parallax, f.lod));

  float max_height = texcoord.z;
  for (int i = 0; i < max_steps && texcoord.z < 1.0; i++) {
    texcoord += delta;
    max_height = max(max_height, sample_material_heightmap(texcoord.xy, f.lod));
  }

  float shadow = 1.0 - (max_height - texcoord.z) * material.shadow;
  return clamp(shadow, 0.0, 1.0);
}

/**
 * @brief Calculate lighting and shadows for a single light source.
 * @param v Vertex data.
 * @param f Fragment data.
 * @param index The light index.
 */
void calculate_light(in common_vertex_t v, inout common_fragment_t f, in int index) {

  light_t light = lights[index];

  vec3 dir = light.origin.xyz - v.model_position;
  float dist = length(dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return;
  }

  dir = normalize(view * vec4(dir, 0.0)).xyz;

  vec3 color = light.color.rgb * light.color.a * atten * modulate;

  float shadow = sample_shadow_cubemap_array(light, index, v, f);

  // Apply parallax self-shadowing for close, high-detail fragments
  if (f.lod < 4.0 && material.shadow > 0.0) {
    shadow *= parallax_self_shadow(dir, v, f);
  }

  if (shadow <= 0.0) {
    return;
  }

  float lambert = max(0.0, dot(dir, f.normal_sample));

  f.diffuse += color * lambert * shadow;
  f.specular += blinn_phong(color * shadow, dir, f);
}

/**
 * @brief Calculate all lighting and shadows with distance-based LOD.
 * @details For distant fragments (>= lighting_distance), uses pre-calculated vertex
 * lighting (ambient + diffuse, no shadows or specular). For close fragments, performs
 * full per-fragment lighting with shadows, specular, and caustics.
 * @param v Vertex data.
 * @param f Fragment data.
 */
void calculate_lighting(in common_vertex_t v, inout common_fragment_t f) {
  
  // Sample voxel lights
  ivec3 voxel_coord = voxel_xyz(v.model_position);
  ivec2 data = voxel_light_data(voxel_coord);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    calculate_light(v, f, index);
  }

  // Sample dynamic lights
  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }
    calculate_light(v, f, index);
  }

  // Add caustics
  f.diffuse += calculate_caustics_lighting(v, f);
}
