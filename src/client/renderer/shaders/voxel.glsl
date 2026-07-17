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

/**
 * @file voxel.glsl
 * @brief Provides voxel lookup helpers for lighting, caustics, and occlusion.
 * @remarks Include after uniforms.glsl and material.glsl; define BINDING_STORAGE_VOXEL_LIGHT_INDICES to declare clustered-light buffers.
 */

#if defined(BINDING_STORAGE_VOXEL_LIGHT_INDICES)
/**
 * @brief Per-voxel clustered light offsets and counts.
 */
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxel_light_data_block {
  int voxel_light_data_elements[];
};

/**
 * @brief Clustered light indices for voxel lookups.
 */
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};
#endif

/**
 * @brief Resolves normalized voxel coordinates for a world position.
 */
vec3 voxel_uvw(in vec3 position) {
  return (position - voxels.mins.xyz) / (voxels.maxs.xyz - voxels.mins.xyz);
}

/**
 * @brief Resolves the integer voxel coordinate for a world position.
 * @remarks Applies a small bias to stabilize boundary rounding.
 */
ivec3 voxel_xyz(in vec3 position) {
  vec3 pos = position - voxels.mins.xyz;
  ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE + 0.001));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

#if defined(BINDING_STORAGE_VOXEL_LIGHT_INDICES)
/**
 * @brief Returns the clustered light offset and count for a voxel.
 */
ivec2 voxel_light_data(in ivec3 voxel) {
  const int index = (voxel.z * int(voxels.size.y) + voxel.y) * int(voxels.size.x) + voxel.x;
  return ivec2(voxel_light_data_elements[index * 2 + 0], voxel_light_data_elements[index * 2 + 1]);
}

/**
 * @brief Returns a clustered light index by buffer position.
 */
int voxel_light_index(in int index) {
  return voxel_light_indices[index];
}
#endif

/**
 * @brief Samples the voxel caustics vector.
 */
vec3 voxel_caustics(in vec3 texcoord) {
  vec3 encoded = texture(texture_voxel_caustics, texcoord).rgb;
  return ((encoded * 2.0) - 1.0) * caustics;
}

/**
 * @brief Samples voxel occlusion.
 */
float voxel_occlusion(in vec3 texcoord) {
  return texture(texture_voxel_occlusion, texcoord).r;
}

/**
 * @brief Samples voxel sky exposure.
 */
float voxel_exposure(in vec3 texcoord) {
  return max(0.25, texture(texture_voxel_occlusion, texcoord).g);
}
