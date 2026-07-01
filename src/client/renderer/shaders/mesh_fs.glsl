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
 * TODO(#864): bring-up mesh program — diffuse material + clustered per-fragment
 * lighting, sharing the BSP fragment layout (see bsp_fs.glsl):
 *
 *   set 2, binding 0 : texture_material         (sampled 2D array) -> sampler 0
 *   set 2, binding 1 : texture_voxel_light_data (sampled 3D int)   -> sampler 1
 *   set 2, binding 2 : lights_block             (read-only storage)-> storage buf 0
 *   set 2, binding 3 : voxel_light_indices      (read-only storage)-> storage buf 1
 *
 * Stages, tint/color modulation, specular/normal mapping, and shadows are ported
 * in later increments.
 */

#define UNIFORMS_NO_SAMPLERS
#include "uniforms.glsl"

layout (set = 2, binding = 0) uniform sampler2DArray texture_material;
layout (set = 2, binding = 1) uniform isampler3D texture_voxel_light_data;

layout (std430, set = 2, binding = 2) readonly buffer lights_block {
  light_t lights[];
};

layout (std430, set = 2, binding = 3) readonly buffer voxel_light_indices_block {
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
 * @brief Unshadowed Lambert diffuse contribution from a single light.
 */
vec3 mesh_light(in int index, in vec3 normal) {

  const light_t light = lights[index];

  const vec3 dir = light.origin.xyz - in_model_position;
  const float dist = length(dir);
  const float radius = light.origin.w;

  const float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return vec3(0.0);
  }

  const float lambert = max(0.0, dot(normal, dir / dist));

  return light_color(light) * atten * lambert;
}

/**
 * @brief Accumulates the static voxel lights affecting this fragment.
 */
vec3 mesh_lighting(void) {

  vec3 diffuse = vec3(0.0);

  const vec3 normal = normalize(in_model_normal);

  const ivec3 voxel = voxel_xyz(in_model_position);
  const ivec2 data = texelFetch(texture_voxel_light_data, voxel, 0).xy;

  for (int i = 0; i < data.y; i++) {
    const int index = voxel_light_indices[data.x + i];
    diffuse += mesh_light(index, normal);
  }

  return diffuse;
}

/**
 * @brief
 */
void main(void) {

  const vec4 diffuse = texture(texture_material, vec3(in_diffusemap, 0.0));

  const vec3 light = vec3(ambient) + mesh_lighting();

  out_color = vec4(diffuse.rgb * light, 1.0);
}
