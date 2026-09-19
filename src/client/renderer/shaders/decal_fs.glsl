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
 * Decals are their own dense descriptor family: the decal atlas at sampler 0,
 * the BSP lights, dynamic lights, voxel light-data and voxel light-index
 * storage buffers at the contiguous bindings 1/2/3/4. The light data is a
 * storage buffer of per-voxel (first_index, count) pairs rather than an
 * RG32I isampler3D because D3D12 cannot sample integer formats. No shadows
 * or normal maps -- decals take clustered voxel diffuse plus a per-draw
 * dynamic light tail, both unshadowed Lambert.
 *
 * The lifetime fade is applied to the vertex colour in decal_vs, which is where
 * the decal's instance is read.
 */
#define BINDING_STORAGE_BSP_LIGHTS           1
#define BINDING_STORAGE_DYNAMIC_LIGHTS       2
#define BINDING_STORAGE_VOXEL_LIGHT_DATA     3
#define BINDING_STORAGE_VOXEL_LIGHT_INDICES  4

#include "light_types.glsl"

layout (set = SAMPLER_SET, binding = 0) uniform sampler2D textureDiffusemap;

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxelLightDataBlock {
  int voxelLightDataElements[];
};

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxelLightIndicesBlock {
  int voxelLightIndices[];
};

/**
 * @brief Per-draw dynamic light mask.
 */
layout (std140, set = UNIFORM_SET, binding = BINDING_LOCALS) uniform decalLocalsBlock {
  uvec4 activeDynamicLights[MAX_DYNAMIC_LIGHTS / 128];
};

layout (location = 0) in vec3 inModelPosition;
layout (location = 1) in vec3 inModelNormal;
layout (location = 2) in vec2 inTexcoord;
layout (location = 3) in vec4 inColor;

layout (location = 0) out vec4 outColor;

/**
 * @brief Resolves the integer voxel coordinate for a world-space position.
 */
ivec3 decalVoxelXyz(in vec3 position) {
  const vec3 pos = position - voxels.mins.xyz;
  const ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Unshadowed Lambert diffuse contribution from a single light.
 */
vec3 decalLight(in Light light, in vec3 normal) {

  const vec3 dir = light.origin.xyz - inModelPosition;
  const float dist = length(dir);
  const float radius = light.origin.w;

  const float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return vec3(0.0);
  }

  const float lambert = max(0.0, dot(normal, dir / dist));

  return lightColor(light) * atten * lambert;
}

/**
 * @brief Shades decal fragments with voxel and dynamic lighting.
 */
void main(void) {

  const vec4 diffuse = texture(textureDiffusemap, inTexcoord);

  const vec3 normal = normalize(inModelNormal);

  vec3 light = ambient;

  const ivec3 voxel = decalVoxelXyz(inModelPosition);
  const int voxelIndex = (voxel.z * int(voxels.size.y) + voxel.y) * int(voxels.size.x) + voxel.x;
  const ivec2 data = ivec2(voxelLightDataElements[voxelIndex * 2 + 0], voxelLightDataElements[voxelIndex * 2 + 1]);

  for (int i = 0; i < data.y; i++) {
    const int index = voxelLightIndices[data.x + i];
    light += decalLight(bspLights[index], normal);
  }

  for (int j = 0; j < numDynamicLights; j++) {
    if (dynamicLightActive(activeDynamicLights, j)) {
      light += decalLight(dynamicLights[j], normal);
    }
  }

  outColor = diffuse * inColor;
  outColor.rgb *= light;
}
