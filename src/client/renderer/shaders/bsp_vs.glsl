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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#version 450

#include "uniforms.glsl"

#define BINDING_UNIFORMS_MATERIAL           2
#define BINDING_STORAGE_NUM_ACTIVE_SAMPLERS 3

#include "common.glsl"
#include "material.glsl"
#include "voxel.glsl"

layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform bspLocalsBlock {
  mat4 model;
  uvec4 activeDynamicLights[MAX_DYNAMIC_LIGHTS / 128];

  /**
   * @brief The layer of texturePortal this draw's faces sample, or -1 for none. Unused here,
   * but both stages take the same block at the same slot.
   */
  int portalLayer;
};

#include "light.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inTangent;
layout (location = 3) in vec3 inBitangent;
layout (location = 4) in vec2 inDiffusemap;
layout (location = 5) in vec4 inColor;

layout (location = 0) out CommonVertex vertex;

invariant gl_Position;

/**
 * @brief Transforms BSP vertices and prepares shared interpolants.
 */
void main(void) {

  mat4 viewModel = view * model;

  vec4 position = vec4(inPosition, 1.0);
  vec4 normal = vec4(inNormal, 0.0);
  vec4 tangent = vec4(inTangent, 0.0);
  vec4 bitangent = vec4(inBitangent, 0.0);

  stageTransform(position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

  vertex.modelPosition = vec3(model * position);
  vertex.modelNormal = normalize(vec3(model * normal));
  vertex.position = vec3(viewModel * position);
  vertex.normal = normalize(vec3(viewModel * normal));
  vertex.tangent = normalize(vec3(viewModel * tangent));
  vertex.bitangent = normalize(vec3(viewModel * bitangent));
  vertex.diffusemap = inDiffusemap;
  vertex.voxel = voxelUvw(vec3(model * position));
  vertex.color = inColor;

  stageVertex(inPosition, vertex);

  vertexLighting(vertex);

  gl_Position = projection3D * viewModel * position;
}
