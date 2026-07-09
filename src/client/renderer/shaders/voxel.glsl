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
 * @remarks Include after uniforms.glsl. The light-data texture and light-index
 * storage buffer are declared here (fragment set 2). The caustics and occlusion
 * volumes are declared when VOXEL_CAUSTICS_OCCLUSION is defined (the lit fragment
 * shaders enable it); otherwise the accessors return neutral fallbacks.
 */

// A program (vertex or fragment) opts into the clustered light buffers by
// defining their binding slots before including this file.
#if defined(BINDING_STORAGE_VOXEL_LIGHT_INDICES)
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_INDICES) readonly buffer voxel_light_indices_block {
  int voxel_light_indices[];
};

// The per-voxel (first_index, count) pairs, indexed by linear voxel index. A
// storage buffer rather than an RG32I isampler3D because D3D12 cannot sample
// integer texture formats.
layout (std430, set = SAMPLER_SET, binding = BINDING_STORAGE_VOXEL_LIGHT_DATA) readonly buffer voxel_light_data_block {
  int voxel_light_data_elements[];
};
#endif

#if defined(VOXEL_CAUSTICS_OCCLUSION)
/**
 * @brief The per-voxel caustics direction (RGB) and spatial occlusion + sky
 * exposure (RG) volumes, sampled with normalized texture coordinates.
 */
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_VOXEL_CAUSTICS)  uniform sampler3D texture_voxel_caustics;
layout (set = SAMPLER_SET, binding = BINDING_SAMPLER_VOXEL_OCCLUSION) uniform sampler3D texture_voxel_occlusion;
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
 * @remarks Returns none unless VOXEL_CAUSTICS_OCCLUSION is defined.
 */
vec3 voxel_caustics(in vec3 texcoord) {
#if defined(VOXEL_CAUSTICS_OCCLUSION)
  vec3 encoded = texture(texture_voxel_caustics, texcoord).rgb;
  return ((encoded * 2.0) - 1.0) * caustics;
#else
  return vec3(0.0);
#endif
}

/**
 * @brief Samples the spatial occlusion at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The occlusion (R channel, 0=open, 1=fully enclosed).
 * @remarks Returns open unless VOXEL_CAUSTICS_OCCLUSION is defined.
 */
float voxel_occlusion(in vec3 texcoord) {
#if defined(VOXEL_CAUSTICS_OCCLUSION)
  return texture(texture_voxel_occlusion, texcoord).r;
#else
  return 0.0;
#endif
}

/**
 * @brief Samples the exposure at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The sky exposure (G channel, 0-1), floored at 0.25.
 * @remarks Returns full unless VOXEL_CAUSTICS_OCCLUSION is defined.
 */
float voxel_exposure(in vec3 texcoord) {
#if defined(VOXEL_CAUSTICS_OCCLUSION)
  return max(0.25, texture(texture_voxel_occlusion, texcoord).g);
#else
  return 1.0;
#endif
}
