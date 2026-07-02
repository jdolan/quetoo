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
#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffusemap;
layout (location = 5) in vec4 in_color;

/**
 * @brief Per-draw locals.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform locals_block {
  mat4 model;
};

layout (location = 0) out common_vertex_t vertex;

invariant gl_Position;

/**
 * @brief
 * @remarks TODO(#864): LOD vertex lighting is deferred; the fragment shader
 * performs full per-fragment lighting for now.
 */
void main(void) {

  mat4 view_model = view * model;

  vec4 position = vec4(in_position, 1.0);
  vec4 normal = vec4(in_normal, 0.0);
  vec4 tangent = vec4(in_tangent, 0.0);
  vec4 bitangent = vec4(in_bitangent, 0.0);

#if defined(MATERIAL_STAGES)
  stage_transform(position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);
#endif

  vertex.model_position = vec3(model * position);
  vertex.model_normal = normalize(vec3(model * normal));
  vertex.position = vec3(view_model * position);
  vertex.normal = normalize(vec3(view_model * normal));
  vertex.tangent = normalize(vec3(view_model * tangent));
  vertex.bitangent = normalize(vec3(view_model * bitangent));
  vertex.diffusemap = in_diffusemap;
  vertex.voxel = voxel_uvw(vec3(model * position));
  vertex.color = in_color;

#if defined(MATERIAL_STAGES)
  stage_vertex(in_position, vertex);
#endif

  gl_Position = projection3D * view_model * position;
}
