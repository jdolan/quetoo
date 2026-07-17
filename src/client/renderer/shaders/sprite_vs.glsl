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

#include "uniforms.glsl"

/*
 * The sprite family's own dense lighting bindings, after the vertex buffer:
 * the BSP lights, dynamic lights, voxel light-data and voxel light-index
 * storage buffers at contiguous bindings 0/1/2/3. The light data is a storage
 * buffer of per-voxel (first_index, count) pairs rather than an RG32I
 * isampler3D because D3D12 cannot sample integer formats. Sprites take
 * clustered voxel BSP light plus a per-batch dynamic light tail (see
 * sprite_locals_block below), both pure distance attenuation (no normal, so
 * no Lambert term) -- computed once per vertex here rather than per fragment,
 * since sprites are flat billboards with no meaningful normal variation across
 * their surface.
 */
#define BINDING_STORAGE_BSP_LIGHTS           0
#define BINDING_STORAGE_DYNAMIC_LIGHTS       1
#define BINDING_STORAGE_VOXEL_LIGHT_DATA     2
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES  3

#include "light_types.glsl"

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxel_light_data_block {
  int voxel_light_data_elements[];
};

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};


/**
 * @brief Per-batch dynamic light cull mask (see r_sprite.c): sprite draws are
 * batched by consecutive same-texture instances, which may span an arbitrary
 * world area, so the mask is built from the union bounds of each batch rather
 * than a single entity's bounds like bsp/mesh -- a conservative superset that
 * may include a few extra lights per batch, but each is still distance-tested
 * per vertex below.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform sprite_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128]; // 128 bits (4 x uint32) per uvec4
};

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec2 in_next_diffusemap;
layout (location = 3) in vec4 in_color;         // UBYTE4_NORM; .rgb is the sprite color
layout (location = 4) in vec2 in_lerp_lighting;  // UBYTE2_NORM; .x lerp, .y lighting

layout (location = 0) out vec2 out_diffusemap;
layout (location = 1) out vec2 out_next_diffusemap;
layout (location = 2) out vec3 out_color;
layout (location = 3) out float out_lerp;
layout (location = 4) out float out_lighting;        // how much surrounding light the sprite absorbs
layout (location = 5) out vec3 out_diffuse;          // per-vertex clustered voxel + dynamic light

invariant gl_Position;

/**
 * @brief Resolves the integer voxel coordinate for a world-space position.
 */
ivec3 sprite_voxel_xyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Unshadowed, distance-only attenuated diffuse contribution from a
 * single light -- sprites are billboards with no meaningful normal, so there
 * is no Lambert term, matching the GL renderer's sprite lighting.
 */
vec3 sprite_light(in light_t light, in vec3 position) {
  const float dist = distance(light.origin.xyz, position);
  const float atten = clamp(1.0 - dist / light.origin.w, 0.0, 1.0);
  return light_color(light) * atten;
}

/**
 * @brief Accumulated clustered voxel BSP light plus the per-batch dynamic
 * light tail (see sprite_locals_block) at the sprite vertex's position.
 */
vec3 sprite_lighting(in vec3 position) {

  vec3 diffuse = vec3(0.0);

  const ivec3 voxel = sprite_voxel_xyz(position);
  const int index = (voxel.z * int(voxels.size.y) + voxel.y) * int(voxels.size.x) + voxel.x;
  const ivec2 data = ivec2(voxel_light_data_elements[index * 2 + 0], voxel_light_data_elements[index * 2 + 1]);

  for (int i = 0; i < data.y; i++) {
    diffuse += sprite_light(bsp_lights[voxel_light_indices[data.x + i]], position);
  }

  // Dynamic lights: those flagged active for this batch.
  for (int j = 0; j < num_dynamic_lights; j++) {
    if (dynamic_light_active(active_dynamic_lights, j)) {
      diffuse += sprite_light(dynamic_lights[j], position);
    }
  }

  return diffuse;
}

/**
 * @brief Billboard sprite/beam vertices are pre-oriented on the CPU. Clustered
 * voxel and dynamic lighting is evaluated once per vertex here (see
 * sprite_lighting) rather than per fragment, since sprites are flat billboards
 * that don't benefit from per-fragment lighting.
 */
void main(void) {

  out_diffusemap = in_diffusemap;
  out_next_diffusemap = in_next_diffusemap;
  out_color = in_color.rgb;
  out_lerp = in_lerp_lighting.x;
  out_lighting = in_lerp_lighting.y;
  out_diffuse = sprite_lighting(in_position);

  gl_Position = projection3D * view * vec4(in_position, 1.0);
}
