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

// The BSP program's own binding map: the material UBO (material + stage
// params combined -- see material.glsl) follows globals (0) and locals (1);
// the fragment stage's own map is defined in bsp_fs.glsl.
#define BINDING_UNIFORMS_MATERIAL 2

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
 */
void main(void) {

  mat4 view_model = view * model;

  vec4 position = vec4(in_position, 1.0);
  vec4 normal = vec4(in_normal, 0.0);
  vec4 tangent = vec4(in_tangent, 0.0);
  vec4 bitangent = vec4(in_bitangent, 0.0);

  stage_transform(position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

  vertex.model_position = vec3(model * position);
  vertex.model_normal = normalize(vec3(model * normal));
  vertex.position = vec3(view_model * position);
  vertex.normal = normalize(vec3(view_model * normal));
  vertex.tangent = normalize(vec3(view_model * tangent));
  vertex.bitangent = normalize(vec3(view_model * bitangent));
  vertex.diffusemap = in_diffusemap;
  vertex.voxel = voxel_uvw(vec3(model * position));
  vertex.color = in_color;

  stage_vertex(in_position, vertex);

  // Cheap per-vertex ambient for the fragment shader's distant-fragment LOD
  // path (see bsp_fragment_lighting): the shared vertex_lighting()/
  // fragment_lighting() split in light.glsl also accumulates per-light
  // diffuse here, but that needs active_lights and the voxel light-data
  // sampler, both fragment-only in this port's binding scheme (see
  // BSP_UNIFORMS_LOCALS/BSP_SAMPLER_VOXEL_LIGHT_DATA) -- ambient alone still
  // delivers the LOD path's performance win of skipping full per-fragment
  // lighting for distant geometry.
  vertex.ambient = vec3(ambient) * voxel_exposure(vertex.voxel) * (1.0 - voxel_occlusion(vertex.voxel) * ambient_occlusion);
  vertex.diffuse = vec3(0.0);

  gl_Position = projection3D * view_model * position;
}
