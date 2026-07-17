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
 * @brief Voxel grid functions for lighting and caustics.
 * @remarks Include after uniforms.glsl and material.glsl (which declares
 * texture_voxel_caustics/texture_voxel_occlusion, part of the canonical
 * lit-material sampler family). The light-data storage buffers are opt-in,
 * declared only when a caller pre-defines BINDING_STORAGE_VOXEL_LIGHT_INDICES
 * (not every consumer of this file does clustered per-voxel light lookups).
 */

// A program (vertex or fragment) opts into the clustered light buffers by
// defining their binding slots before including this file.
#if defined(BINDING_STORAGE_VOXEL_LIGHT_INDICES)
// The per-voxel (first_index, count) pairs, indexed by linear voxel index. A
// storage buffer rather than an RG32I isampler3D because D3D12 cannot sample
// integer texture formats.
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxel_light_data_block {
  int voxel_light_data_elements[];
};

layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};
#endif

/**
 * @brief Resolves the voxel texture coordinate for the specified position in world space.
 * @param position The position in world space.
 * @return The normalized voxel texture coordinates (0-1 range).
 */
vec3 voxel_uvw(in vec3 position) {
  return (position - voxels.mins.xyz) / (voxels.maxs.xyz - voxels.mins.xyz);
}

/**
 * @brief Resolves the voxel coordinate for the specified position in world space.
 * @param position The position in world space.
 * @return The integer voxel coordinates (x, y, z).
 */
ivec3 voxel_xyz(in vec3 position) {
  vec3 pos = position - voxels.mins.xyz;
  // The epsilon bias makes boundary rounding deterministic across GPU/driver
  // generations: geometry built exactly on a voxel-grid plane divides out to
  // (near-)exact integers, and without a bias, ULP-level differences in how
  // different compilers/hardware round that division can flip the floor()
  // result to the neighboring voxel -- producing hardware-dependent seams
  // where a light is in one voxel's list but not its neighbor's.
  ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE + 0.001));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

#if defined(BINDING_STORAGE_VOXEL_LIGHT_INDICES)
/**
 * @brief Fetches the light data (index offset, index count) for the given voxel.
 * @param voxel The voxel coordinate.
 * @return The offset into the voxel light index TBO, and the count of index elements (texels).
 */
ivec2 voxel_light_data(in ivec3 voxel) {
  const int index = (voxel.z * int(voxels.size.y) + voxel.y) * int(voxels.size.x) + voxel.x;
  return ivec2(voxel_light_data_elements[index * 2 + 0], voxel_light_data_elements[index * 2 + 1]);
}

/**
 * @brief Fetches the light index element at the specified position in the light index TBO.
 * @param index The position in the light index TBO.
 * @return The index of the light referenced by the index element.
 */
int voxel_light_index(in int index) {
  return voxel_light_indices[index];
}
#endif

/**
 * @brief Samples the encoded caustics vector at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return Signed direction vector with length as intensity (scaled by uniform caustics).
 */
vec3 voxel_caustics(in vec3 texcoord) {
  vec3 encoded = texture(texture_voxel_caustics, texcoord).rgb;
  return ((encoded * 2.0) - 1.0) * caustics;
}

/**
 * @brief Samples the spatial occlusion at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The occlusion (R channel, 0=open, 1=fully enclosed).
 */
float voxel_occlusion(in vec3 texcoord) {
  return texture(texture_voxel_occlusion, texcoord).r;
}

/**
 * @brief Samples the exposure at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The sky exposure (G channel, 0-1), floored at 0.25.
 */
float voxel_exposure(in vec3 texcoord) {
  return max(0.25, texture(texture_voxel_occlusion, texcoord).g);
}
