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
#include "soften.glsl"

layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_DIFFUSE)      uniform sampler2D texture_diffusemap;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_NEXT_DIFFUSE) uniform sampler2D texture_next_diffusemap;

/*
 * The sprite family's own dense lighting bindings, after diffuse (0), next-diffuse
 * (1) and the soft-particle depth attachment (2): the voxel light data at sampler
 * 3, then the lights and voxel light-index storage buffers at the contiguous
 * bindings 4/5 (SLOT_STORAGE_LIGHTS / _VOXEL_LIGHT_INDICES on the C side). Sprites
 * take clustered voxel diffuse only (no normal, so pure distance attenuation).
 */
layout (set = SAMPLER_SET, binding = 3) uniform isampler3D texture_voxel_light_data;

layout (std430, set = SAMPLER_SET, binding = 4) readonly buffer lights_block {
  int num_lights;
  int num_bsp_lights;
  light_t lights[];
};

layout (std430, set = SAMPLER_SET, binding = 5) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec2 in_next_diffusemap;
layout (location = 3) in vec3 in_color;
layout (location = 4) in float in_lerp;
layout (location = 5) in float in_lighting;

layout (location = 0) out vec4 out_color;

/**
 * @brief Resolves the integer voxel coordinate for a world-space position.
 */
ivec3 sprite_voxel_xyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Accumulated clustered voxel light at the sprite's position. Sprites are
 * billboards with no meaningful normal, so lighting is pure distance attenuation
 * (no Lambert term), matching the GL renderer's sprite lighting.
 */
vec3 sprite_lighting(void) {

  vec3 diffuse = vec3(0.0);

  const ivec3 voxel = sprite_voxel_xyz(in_position);
  const ivec2 data = texelFetch(texture_voxel_light_data, voxel, 0).xy;

  for (int i = 0; i < data.y; i++) {
    const light_t light = lights[voxel_light_indices[data.x + i]];

    const float dist = distance(light.origin.xyz, in_position);
    const float atten = clamp(1.0 - dist / light.origin.w, 0.0, 1.0);

    diffuse += light_color(light) * atten;
  }

  return diffuse;
}

/**
 * @brief Additive sprite/particle shading with a soft-particle edge: the color
 * is faded (toward black, so additive blending fades to nothing) as the fragment
 * nears or passes the opaque scene depth behind it. Absorptive sprites (smoke,
 * @c in_lighting > 0) are modulated by the surrounding clustered voxel light;
 * emissive sprites (@c in_lighting == 0, e.g. blaster particles) stay fullbright.
 */
void main(void) {

  const vec3 texture_color = mix(
      texture(texture_diffusemap, in_diffusemap).rgb,
      texture(texture_next_diffusemap, in_next_diffusemap).rgb,
      in_lerp);

  vec3 color = in_color;
  if (in_lighting > 0.0) {
    color = mix(color, color * sprite_lighting(), in_lighting);
  }

  out_color = vec4(texture_color * color * soften(), 1.0);
}
