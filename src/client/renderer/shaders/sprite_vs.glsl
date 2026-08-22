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

/**
 * @file sprite_vs.glsl
 * @brief Computes per-vertex lighting for billboard sprites and beams.
 */
#define BINDING_STORAGE_BSP_LIGHTS           0
#define BINDING_STORAGE_DYNAMIC_LIGHTS       1
#define BINDING_STORAGE_VOXEL_LIGHT_DATA     2
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES  3
#define BINDING_STORAGE_SPRITE_INSTANCES     4

#include "light_types.glsl"

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxel_light_data_block {
  int voxel_light_data_elements[];
};

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};


/**
 * @brief Per-batch dynamic light mask for sprite draws.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform sprite_locals_block {
  uvec4 active_dynamic_lights[MAX_DYNAMIC_LIGHTS / 128];
};

/*
 * Sprites and beams reduce to the same quad, a center and two half axes, so both
 * arrive as one instance type. There is no vertex buffer: the four corners are
 * `center + (±a) + (±b)`, expanded here from gl_VertexIndex against the static
 * quad index buffer.
 */

/**
 * @brief One sprite or beam quad. Must match `r_sprite_instance_t`.
 */
struct sprite_instance_t {
  vec4 center;
  vec4 a;
  vec4 b;
  vec4 texcoords;
  vec4 next_texcoords;
  vec4 color;
};

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_SPRITE_INSTANCES) readonly buffer sprite_instances_block {
  sprite_instance_t sprite_instances[];
};

/**
 * @brief The half axis signs and texture coordinate corners, by corner index.
 */
const vec2 SPRITE_SIGNS[4] = { vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0), vec2(-1.0, -1.0) };
const vec2 SPRITE_CORNERS[4] = { vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0) };

layout (location = 0) out vec2 out_diffusemap;
layout (location = 1) out vec2 out_next_diffusemap;
layout (location = 2) out vec3 out_color;
layout (location = 3) out float out_lerp;
layout (location = 4) out float out_lighting;
layout (location = 5) out vec3 out_diffuse;

invariant gl_Position;

/**
 * @brief Resolves the integer voxel coordinate for a sprite vertex.
 */
ivec3 sprite_voxel_xyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Computes distance-attenuated sprite lighting from one light.
 */
vec3 sprite_light(in light_t light, in vec3 position) {
  const float dist = distance(light.origin.xyz, position);
  const float atten = clamp(1.0 - dist / light.origin.w, 0.0, 1.0);
  return light_color(light) * atten;
}

/**
 * @brief Accumulates voxel and dynamic sprite lighting at a position.
 */
vec3 sprite_lighting(in vec3 position) {

  vec3 diffuse = vec3(0.0);

  const ivec3 voxel = sprite_voxel_xyz(position);
  const int index = (voxel.z * int(voxels.size.y) + voxel.y) * int(voxels.size.x) + voxel.x;
  const ivec2 data = ivec2(voxel_light_data_elements[index * 2 + 0], voxel_light_data_elements[index * 2 + 1]);

  for (int i = 0; i < data.y; i++) {
    diffuse += sprite_light(bsp_lights[voxel_light_indices[data.x + i]], position);
  }

  for (int j = 0; j < num_dynamic_lights; j++) {
    if (dynamic_light_active(active_dynamic_lights, j)) {
      diffuse += sprite_light(dynamic_lights[j], position);
    }
  }

  return diffuse;
}

/**
 * @brief Passes sprite attributes through and evaluates per-vertex lighting.
 */
void main(void) {

  const uint corner = uint(gl_VertexIndex) & 3u;

  const sprite_instance_t instance = sprite_instances[uint(gl_VertexIndex) >> 2];

  const vec2 signs = SPRITE_SIGNS[corner];
  const vec3 position = instance.center.xyz + instance.a.xyz * signs.x + instance.b.xyz * signs.y;

  out_diffusemap = mix(instance.texcoords.xy, instance.texcoords.zw, SPRITE_CORNERS[corner]);
  out_next_diffusemap = mix(instance.next_texcoords.xy, instance.next_texcoords.zw, SPRITE_CORNERS[corner]);
  out_color = instance.color.rgb;
  out_lerp = instance.center.w;
  out_lighting = instance.a.w;
  out_diffuse = sprite_lighting(position);

  gl_Position = projection3D * view * vec4(position, 1.0);
}
