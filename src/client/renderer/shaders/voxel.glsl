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
 */

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
  ivec3 voxel = ivec3(floor(pos / BSP_VOXEL_SIZE));
  return clamp(voxel, ivec3(0), ivec3(voxels.size.xyz) - ivec3(1));
}

/**
 * @brief Fetches the light data (index offset, index count) for the given voxel.
 * @param voxel The voxel coordinate.
 * @return The offset into the voxel light index TBO, and the count of index elements (texels).
 */
ivec2 voxel_light_data(in ivec3 voxel) {
  return texelFetch(texture_voxel_light_data, voxel, 0).xy;
}

/**
 * @brief Fetches the light index element at the specified position in the light index TBO.
 * @param index The position in the light index TBO.
 * @return The index of the light referenced by the index element.
 */
int voxel_light_index(in int index) {
  return texelFetch(texture_voxel_light_indices, index).x;
}

/**
 * @brief Samples the caustics intensity at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The caustics intensity (R channel, 0-1).
 */
float voxel_caustics(in vec3 texcoord) {
  return texture(texture_voxel_data, texcoord).r * caustics;
}

/**
 * @brief Samples the exposure at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The exposure (G channel, 0-1).
 */
float voxel_exposure(in vec3 texcoord) {
  return max(0.25, texture(texture_voxel_data, texcoord).g);
}

/**
 * @brief Calculate vertex lighting contribution from a single light (unshadowed diffuse only).
 * @param v Vertex data.
 * @param index The light index.
 * @return The diffuse lighting contribution.
 */
vec3 vertex_light(in common_vertex_t v, in int index) {
  light_t light = lights[index];

  vec3 light_dir = light.origin.xyz - v.model_position;
  float dist = length(light_dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

  if (atten <= 0.0) {
    return vec3(0.0);
  }

  light_dir = normalize(light_dir);
  float lambert = dot(v.model_normal, light_dir);
  lambert = bool(material.surface & (SURF_MASK_BLEND | SURF_LIQUID)) ? abs(lambert) : max(0.0, lambert);
  return light.color.rgb * light.color.a * atten * lambert * modulate;
}

/**
 * @brief Calculate vertex lighting from voxel lights (unshadowed diffuse only).
 * @details Used for distant geometry where per-fragment lighting is too expensive.
 * @param v Vertex data (lighting field is written).
 */
void vertex_lighting(inout common_vertex_t v) {
  vec3 diffuse = vec3(0.0);

  ivec3 voxel_coord = voxel_xyz(v.model_position);
  ivec2 data = voxel_light_data(voxel_coord);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    diffuse += vertex_light(v, index);
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }
    diffuse += vertex_light(v, index);
  }

  v.lighting = diffuse;
}

/**
 * @brief Calculate caustics lighting contribution.
 */
void vertex_caustics(inout common_vertex_t v) {
  v.caustics = voxel_caustics(v.voxel);
}

/**
 * @brief Calculate caustics lighting contribution.
 * @details Applies animated noise-based caustics to the diffuse lighting.
 * Uses pre-calculated vertex caustics if available, otherwise samples from voxel data.
 * @param v Vertex data.
 * @param f Fragment data.
 * @return The caustics contribution to add to diffuse.
 */
void fragment_caustics(in common_vertex_t v, inout common_fragment_t f) {

  f.caustics = v.caustics;

  if (f.caustics == 0.0) {
    return;
  }

  float noise = noise3d(v.model_position * .05 + (ticks / 1000.0) * 0.5);

  // make the inner edges stronger, clamp to 0-1
  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = f.ambient + f.diffuse;
  f.diffuse += max(vec3(0.0), light * f.caustics * noise);
}
