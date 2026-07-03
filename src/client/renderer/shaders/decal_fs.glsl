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

#define UNIFORMS_NO_SAMPLERS
#include "uniforms.glsl"

/*
 * Decals are their own dense descriptor family: the decal atlas and voxel light
 * data at samplers 0/1, the lights and voxel light-index storage buffers at the
 * contiguous bindings 2/3 (SLOT_STORAGE_LIGHTS / _VOXEL_LIGHT_INDICES on the C
 * side). No shadows or normal maps -- decals take clustered voxel diffuse only.
 */
layout (set = SAMPLER_SET, binding = 0) uniform sampler2D texture_diffusemap;
layout (set = SAMPLER_SET, binding = 1) uniform isampler3D texture_voxel_light_data;

layout (std430, set = SAMPLER_SET, binding = 2) readonly buffer lights_block {
  int num_lights;
  int num_bsp_lights;
  light_t lights[];
};

layout (std430, set = SAMPLER_SET, binding = 3) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};

layout (location = 0) in vec3 in_model_position;
layout (location = 1) in vec3 in_model_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in vec4 in_color;
layout (location = 4) flat in uint in_time;
layout (location = 5) flat in uint in_lifetime;

layout (location = 0) out vec4 out_color;

/**
 * @brief Resolves the integer voxel coordinate for a world-space position.
 */
ivec3 decal_voxel_xyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Unshadowed Lambert diffuse contribution from a single light.
 */
vec3 decal_light(in int index, in vec3 normal) {

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
 * @brief Decal fragment shader: the decal atlas lit by clustered voxel lights
 * (matching the surface beneath it), alpha-blended and faded over its lifetime.
 */
void main(void) {

  const vec4 diffuse = texture(texture_diffusemap, in_texcoord);

  const vec3 normal = normalize(in_model_normal);

  vec3 light = vec3(ambient);

  const ivec3 voxel = decal_voxel_xyz(in_model_position);
  const ivec2 data = texelFetch(texture_voxel_light_data, voxel, 0).xy;

  for (int i = 0; i < data.y; i++) {
    const int index = voxel_light_indices[data.x + i];
    light += decal_light(index, normal);
  }

  out_color = diffuse * in_color;
  out_color.rgb *= light;

  if (in_lifetime > 0u) {
    const float age = float(uint(ticks) - in_time);
    const float fade = 1.0 - clamp(age / float(in_lifetime), 0.0, 1.0);
    out_color.a *= fade;
  }
}
