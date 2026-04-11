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
 * @brief 2D Poisson disk samples for PCF soft shadows.
 */
const vec2 poisson_disk[16] = vec2[](
  vec2( 0.2770745,  0.6951455),
  vec2(-0.5932785, -0.1203284),
  vec2( 0.4494750,  0.2469098),
  vec2(-0.1460639, -0.5679667),
  vec2( 0.6400498, -0.4071948),
  vec2(-0.3631914,  0.7935778),
  vec2( 0.1248857, -0.8975238),
  vec2(-0.7720318,  0.4438458),
  vec2( 0.8851806,  0.1653373),
  vec2(-0.5238012, -0.7260296),
  vec2( 0.3642682,  0.5968054),
  vec2(-0.8331701, -0.3328346),
  vec2( 0.5527260, -0.6985809),
  vec2(-0.2407123,  0.3153157),
  vec2( 0.7269405, -0.1430640),
  vec2(-0.6444675,  0.6444675)
);

/**
 * @brief Simple pseudo-random function for per-pixel rotation.
 */
float random_angle(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.283185;
}

/**
 * @brief Determine cubemap face index and compute face UV from direction vector.
 * @details Face UV mapping derived from the light_view matrices in r_shadow.c:
 *   Face 0 (+X): sc = -dz, tc = -dy, ma = |dx|
 *   Face 1 (-X): sc =  dz, tc = -dy, ma = |dx|
 *   Face 2 (+Y): sc =  dx, tc =  dz, ma = |dy|
 *   Face 3 (-Y): sc =  dx, tc = -dz, ma = |dy|
 *   Face 4 (+Z): sc =  dx, tc = -dy, ma = |dz|
 *   Face 5 (-Z): sc = -dx, tc = -dy, ma = |dz|
 * @param dir Direction vector from light to fragment (world space).
 * @param face Output face index (0-5).
 * @param face_uv Output UV coordinates within the face tile [0, 1].
 * @param ma Output dominant axis magnitude (for filter radius computation).
 */
void cubemap_face_uv(in vec3 dir, out int face, out vec2 face_uv, out float ma) {

  vec3 ad = abs(dir);
  float sc, tc;

  if (ad.x >= ad.y && ad.x >= ad.z) {
    ma = ad.x;
    if (dir.x > 0.0) {
      face = 0; sc = -dir.z; tc = -dir.y;
    } else {
      face = 1; sc = dir.z; tc = -dir.y;
    }
  } else if (ad.y >= ad.x && ad.y >= ad.z) {
    ma = ad.y;
    if (dir.y > 0.0) {
      face = 2; sc = dir.x; tc = dir.z;
    } else {
      face = 3; sc = dir.x; tc = -dir.z;
    }
  } else {
    ma = ad.z;
    if (dir.z > 0.0) {
      face = 4; sc = dir.x; tc = -dir.y;
    } else {
      face = 5; sc = -dir.x; tc = -dir.y;
    }
  }

  face_uv = vec2(sc, tc) / (2.0 * ma) + 0.5;
}

/**
 * @brief Sample the shadow atlas with PCF filtering.
 * @param light The light source.
 * @param index The light index.
 * @param v Vertex data.
 * @param f Fragment data.
 * @return Shadow term (0 = fully shadowed, 1 = fully lit).
 */
float sample_shadow_atlas(in light_t light, in int index, in common_vertex_t v, in common_fragment_t f) {

  if (light.shadow.z == 0.0) {
    return 1.0;
  }

  vec3 light_to_frag = v.model_position - light.origin.xyz;
  float current_depth = length(light_to_frag) / depth_range.y;

  int face;
  vec2 fuv;
  float ma;
  cubemap_face_uv(light_to_frag, face, fuv, ma);

  // Atlas tile origin for this face within the light's 3×2 block
  int face_col = face - (face / 3) * 3;
  int face_row = face / 3;
  vec2 tile_origin = light.shadow.xy + vec2(float(face_col) * light.shadow.z,
                                             float(face_row) * light.shadow.w);

  // Half-texel inset to prevent cross-tile bleeding
  ivec2 atlas_size = textureSize(texture_shadow_atlas, 0);
  vec2 half_texel = 0.5 / vec2(atlas_size);
  vec2 tile_min = tile_origin + half_texel;
  vec2 tile_max = tile_origin + vec2(light.shadow.z, light.shadow.w) - half_texel;

  // Filter radius in face UV space [0..1]
  float light_size = light.origin.w * 3.0;
  float dist_to_light = length(light_to_frag);
  float filter_radius = light_size * (dist_to_light / light.origin.w) * 0.005;
  float filter_uv = filter_radius / (2.0 * max(ma, 0.001));

  // Distance-based sample count (close = more samples, far = fewer)
  int num_samples = f.view_dist < 512.0 ? 16 : (f.view_dist < 1024.0 ? 8 : 4);

  // Per-pixel rotation to eliminate banding
  float angle = random_angle(v.model_position);
  float s = sin(angle);
  float c = cos(angle);

  float shadow = 0.0;

  for (int i = 0; i < num_samples; i++) {
    vec2 rotated = vec2(c * poisson_disk[i].x - s * poisson_disk[i].y,
                        s * poisson_disk[i].x + c * poisson_disk[i].y);

    vec2 sample_fuv = fuv + rotated * filter_uv;

    vec2 atlas_uv = tile_origin + sample_fuv * vec2(light.shadow.z, light.shadow.w);

    atlas_uv = clamp(atlas_uv, tile_min, tile_max);

    shadow += texture(texture_shadow_atlas, vec3(atlas_uv, current_depth));
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
void fragment_light(in common_vertex_t v, inout common_fragment_t f, in int index) {

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

  float shadow = sample_shadow_atlas(light, index, v, f);

  // Apply parallax self-shadowing for close, high-detail fragments
  if (stage.flags == STAGE_NONE) {
    if (f.lod < 4.0 && material.shadow > 0.0) {
      shadow *= parallax_self_shadow(dir, v, f);
    }
  }

  if (shadow <= 0.0) {
    return;
  }

  float lambert = dot(dir, f.normal_sample);
  lambert = material.alpha_blend ? abs(lambert) : max(0.0, lambert);

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
void fragment_lighting(in common_vertex_t v, inout common_fragment_t f) {

  // Sample static voxel lights
  ivec3 voxel_coord = voxel_xyz(v.model_position);
  ivec2 data = voxel_light_data(voxel_coord);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    fragment_light(v, f, index);
  }

  // Sample dynamic lights
  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }
    fragment_light(v, f, index);
  }

  // Add caustics
  fragment_caustics(v, f);
}
