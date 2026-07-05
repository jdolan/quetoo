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
 * The mesh program: animated (two-frame lerp) geometry with full per-fragment
 * material lighting, sharing the BSP fragment stack (common/material/voxel/light).
 * The two animation frames are supplied as two vertex buffer bindings (the same
 * model buffer at two frame offsets): locations 0..4 are the old frame, 5..8 the
 * current frame; diffusemap (4) does not animate.
 */

#include "uniforms.glsl"

// The mesh program's own binding map: the stage UBO follows globals (0) and
// locals (1); the fragment stage's own map is defined in mesh_fs.glsl.
#define BINDING_UNIFORMS_STAGE 2

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

// Old animation frame.
layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffusemap;

// Current animation frame (diffusemap is shared with the old frame).
layout (location = 5) in vec3 in_next_position;
layout (location = 6) in vec3 in_next_normal;
layout (location = 7) in vec3 in_next_tangent;
layout (location = 8) in vec3 in_next_bitangent;

/**
 * @brief Per-entity locals.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
  float lerp;
  vec4 color;
};

layout (location = 0) out common_vertex_t vertex;

invariant gl_Position;

/**
 * @brief Lerps the two animation frames and emits the shared common_vertex_t.
 */
void main(void) {

  mat4 view_model = view * model;

  vec4 position = vec4(mix(in_position, in_next_position, lerp), 1.0);
  vec4 normal = vec4(mix(in_normal, in_next_normal, lerp), 0.0);
  vec4 tangent = vec4(mix(in_tangent, in_next_tangent, lerp), 0.0);
  vec4 bitangent = vec4(mix(in_bitangent, in_next_bitangent, lerp), 0.0);

  stage_transform(position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

  vertex.model_position = vec3(model * position);
  vertex.model_normal = normalize(vec3(model * normal));
  vertex.position = vec3(view_model * position);
  vertex.normal = normalize(vec3(view_model * normal));
  vertex.tangent = normalize(vec3(view_model * tangent));
  vertex.bitangent = normalize(vec3(view_model * bitangent));
  vertex.diffusemap = in_diffusemap;
  vertex.voxel = voxel_uvw(vec3(model * position));
  vertex.color = color;

  stage_vertex(in_position, vertex);

  gl_Position = projection3D * view_model * position;
}
