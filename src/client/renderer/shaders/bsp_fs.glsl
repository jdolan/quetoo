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

#version 450

/*
 * TODO(#864): bring-up BSP program — diffuse material + clustered per-fragment
 * lighting with point-light shadow maps:
 *
 *   set 2, binding 0 : texture_material         (sampled 2D array)   -> sampler 0
 *   set 2, binding 1 : texture_voxel_light_data (sampled 3D int)     -> sampler 1
 *   set 2, binding 2 : texture_shadow_atlas     (sampled 2D array cmp)-> sampler 2
 *   set 2, binding 3 : lights_block             (read-only storage)  -> storage 0
 *   set 2, binding 4 : voxel_light_indices      (read-only storage)  -> storage 1
 *
 * Each light's shadow tile (light.shadow: xy base, z tile UV, w layer) selects a
 * 3x2 cube-face block in the shadow atlas; the fragment compares its radial
 * distance-from-light against the stored occluder depth. PCF softening, specular,
 * caustics and sky ambient are deferred.
 */

#define UNIFORMS_NO_SAMPLERS
#define UNIFORMS_LIGHT_CULL
#include "uniforms.glsl"

layout (set = 2, binding = 0) uniform sampler2DArray texture_material;
layout (set = 2, binding = 1) uniform isampler3D texture_voxel_light_data;
layout (set = 2, binding = 2) uniform sampler2DShadow texture_shadow_atlas;

layout (std430, set = 2, binding = 3) readonly buffer lights_block {
  light_t lights[];
};

layout (std430, set = 2, binding = 4) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};

layout (location = 0) in vec2 in_diffusemap;
layout (location = 1) in vec3 in_model_position;
layout (location = 2) in vec3 in_model_normal;

layout (location = 0) out vec4 out_color;

/**
 * @brief Resolves the integer voxel coordinate for a world-space position.
 */
ivec3 voxel_xyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Determine cubemap face index and face UV from a direction vector.
 * @details Matches the light_view matrices in r_shadow.c.
 */
void cubemap_face_uv(in vec3 dir, out int face, out vec2 face_uv) {

  const vec3 ad = abs(dir);
  float sc, tc, ma;

  if (ad.x >= ad.y && ad.x >= ad.z) {
    ma = ad.x;
    if (dir.x > 0.0) { face = 0; sc = -dir.z; tc = -dir.y; }
    else             { face = 1; sc =  dir.z; tc = -dir.y; }
  } else if (ad.y >= ad.x && ad.y >= ad.z) {
    ma = ad.y;
    if (dir.y > 0.0) { face = 2; sc = dir.x; tc =  dir.z; }
    else             { face = 3; sc = dir.x; tc = -dir.z; }
  } else {
    ma = ad.z;
    if (dir.z > 0.0) { face = 4; sc =  dir.x; tc = -dir.y; }
    else             { face = 5; sc = -dir.x; tc = -dir.y; }
  }

  face_uv = vec2(sc, tc) / (2.0 * max(ma, 0.0001)) + 0.5;
}

/**
 * @brief Resolves the shadow atlas UV for a fragment/light pair.
 */
vec2 shadow_atlas_uv(in light_t light, in vec3 light_to_frag) {

  const float tile_uv = light.shadow.z;

  int face;
  vec2 fuv;
  cubemap_face_uv(light_to_frag, face, fuv);

  // SDL_gpu renders the atlas with a top-left texel origin; flip V within the tile.
  fuv.y = 1.0 - fuv.y;

  const int face_col = face - (face / 3) * 3;
  const int face_row = face / 3;
  const vec2 tile_origin = light.shadow.xy + vec2(float(face_col) * tile_uv, float(face_row) * tile_uv);

  // Half-texel inset to prevent cross-tile bleeding.
  const vec2 half_texel = 0.5 / vec2(textureSize(texture_shadow_atlas, 0).xy);
  return clamp(tile_origin + fuv * tile_uv,
               tile_origin + half_texel,
               tile_origin + vec2(tile_uv) - half_texel);
}

/**
 * @brief Samples the shadow atlas for a light. Returns 1 (lit) .. 0 (shadowed).
 * @details Manual comparison of radial fragment distance against the stored
 * occluder depth (both normalized by depth_range.y).
 */
float sample_shadow_atlas(in light_t light) {

  if (light.shadow.z == 0.0) {
    return 1.0;
  }

  const vec3 light_to_frag = in_model_position - light.origin.xyz;
  const float current_depth = length(light_to_frag) / depth_range.y;

  const vec2 uv = shadow_atlas_uv(light, light_to_frag);

  // Hardware comparison (LEQUAL) with linear PCF: 1 = lit, 0 = shadowed.
  return texture(texture_shadow_atlas, vec3(uv, current_depth));
}

/**
 * @brief Unshadowed Lambert diffuse contribution from a single light.
 */
vec3 bsp_light(in int index, in vec3 normal) {

  const light_t light = lights[index];

  const vec3 dir = light.origin.xyz - in_model_position;
  const float dist = length(dir);
  const float radius = light.origin.w;

  const float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return vec3(0.0);
  }

  const float lambert = max(0.0, dot(normal, dir / dist));
  if (lambert <= 0.0) {
    return vec3(0.0);
  }

  return light_color(light) * atten * lambert * sample_shadow_atlas(light);
}

/**
 * @brief Accumulates the static voxel lights affecting this fragment.
 */
vec3 bsp_lighting(void) {

  vec3 diffuse = vec3(0.0);

  const vec3 normal = normalize(in_model_normal);

  const ivec3 voxel = voxel_xyz(in_model_position);
  const ivec2 data = texelFetch(texture_voxel_light_data, voxel, 0).xy;

  for (int i = 0; i < data.y; i++) {
    const int index = voxel_light_indices[data.x + i];
    diffuse += bsp_light(index, normal);
  }

  // Dynamic lights occupy the contiguous tail [num_bsp_lights, num_lights) and
  // have no voxel grid; only those flagged active for this block contribute.
  // bit j => lights[num_bsp_lights + j].
  const int num_dynamic = num_lights - num_bsp_lights;
  for (int j = 0; j < num_dynamic; j++) {
    if ((active_lights[j >> 5] & (1u << (j & 31))) != 0u) {
      diffuse += bsp_light(num_bsp_lights + j, normal);
    }
  }

  return diffuse;
}

/**
 * @brief
 */
void main(void) {

  const vec4 diffuse = texture(texture_material, vec3(in_diffusemap, 0.0));

  const vec3 light = vec3(ambient) + bsp_lighting();

  out_color = vec4(diffuse.rgb * light, 1.0);
}
