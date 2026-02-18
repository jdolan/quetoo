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
 * @brief Voxel grid functions for lighting, fog, and caustics.
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
  return texture(texture_voxel_data, texcoord).r;
}

/**
 * @brief Sample voxel caustics at a world-space position (convenience function).
 * @param position The world-space position.
 * @return The caustics intensity.
 */
float sample_voxel_caustics(in vec3 position) {
  return voxel_caustics(voxel_uvw(position)) * caustics;
}

/**
 * @brief Samples the exposure at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The exposure (G channel, 0-1).
 */
float voxel_exposure(in vec3 texcoord) {
  return texture(texture_voxel_data, texcoord).g;
}

/**
 * @brief Samples the fog density at the given voxel texture coordinate.
 * @param texcoord The voxel texture coordinate (0-1 range).
 * @return The fog density (B channel, 0-1).
 */
float voxel_fog_density(in vec3 texcoord) {
  return texture(texture_voxel_data, texcoord).b;
}

/**
 * @brief Calculate lighting at a specific world position for fog rendering.
 * @details Computes unshadowed lighting for performance in volumetric fog raymarching.
 * @param position The world position to calculate lighting at.
 * @return The combined diffuse lighting from voxel lights and dynamic lights.
 */
vec3 voxel_fog_lighting(in vec3 position) {

  vec3 diffuse = vec3(0.0);

  ivec3 voxel_coord = voxel_xyz(position);
  ivec2 data = voxel_light_data(voxel_coord);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    light_t light = lights[index];

    float dist = distance(light.origin.xyz, position);
    float radius = light.origin.w;
    float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

    diffuse += light.color.rgb * light.color.a * atten * modulate;
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }

    light_t light = lights[index];

    float dist = distance(light.origin.xyz, position);
    float radius = light.origin.w;
    float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

    diffuse += light.color.rgb * light.color.a * atten * modulate;
  }

  return diffuse;
}

/**
 * @brief Sample voxel fog at a world-space position (convenience function).
 * @param position The world-space position.
 * @return The fog color (rgb) and density (a).
 */
vec4 sample_voxel_fog(in vec3 position) {
  float density = voxel_fog_density(voxel_uvw(position));
  if (density > 0.0) {
    vec3 lighting = voxel_fog_lighting(position);
    return vec4(lighting, density * fog_density);
  }
  return vec4(0.0);
}

/**
 * @brief Calculate volumetric fog with raymarching.
 * @details Raymarches from world_pos to the camera, accumulating fog density and lighting.
 * Supports both vertex (pre-calculated) and fragment (dynamic) fog based on distance.
 * @param world_pos The world-space position to calculate fog from.
 * @param view_pos The camera position in world space.
 * @param max_samples Maximum number of raymarching samples.
 * @return The fog color (rgb) and density (a).
 */
vec4 calculate_fog(in vec3 world_pos, in vec3 view_pos, in float max_samples) {
  vec4 fog = vec4(0.0);
  
  float dist = distance(world_pos, view_pos);
  float samples = clamp(dist / BSP_VOXEL_SIZE, 1.0, max_samples);
  
  vec3 voxel_start = voxel_uvw(world_pos);
  vec3 voxel_end = voxels.view_coordinate.xyz;
  
  for (float i = 0; i < samples; i++) {
    vec3 xyz = mix(world_pos, view_pos, i / samples);
    vec3 uvw = mix(voxel_start, voxel_end, i / samples);
    
    float fog_density_sample = voxel_fog_density(uvw);
    
    if (fog_density_sample > 0.0) {
      vec3 fog_lighting = voxel_fog_lighting(xyz);
      fog += vec4(fog_lighting, fog_density_sample * fog_density) * min(1.0, samples - i);
    }
    
    if (fog.a >= 0.95) {
      fog.a = 1.0;
      break;
    }
  }
  
  if (hmax(fog.rgb) > 1.0) {
    fog.rgb /= hmax(fog.rgb);
  }
  
  return clamp(fog, 0.0, 1.0);
}

/**
 * @brief Calculate vertex fog (pre-calculated in vertex shader).
 * @param world_pos The world-space position of the vertex.
 * @return The fog color (rgb) and density (a).
 */
vec4 calculate_vertex_fog(in vec3 world_pos) {
  return calculate_fog(world_pos, view[0].xyz, fog_samples);
}

/**
 * @brief Calculate fragment fog with distance-based LOD.
 * @details For distant fragments (>= fog_distance), uses pre-calculated vertex fog.
 * For close fragments, performs full per-fragment raymarching for high-quality
 * dynamic lighting effects (e.g., rocket lighting fog).
 * @param world_pos The world-space position.
 * @param voxel_uvw_coord The voxel texture coordinate.
 * @param view_dist The distance from camera.
 * @param vertex_fog The pre-calculated vertex fog.
 * @return The fog color (rgb) and density (a).
 */
vec4 calculate_fragment_fog(in vec3 world_pos, in vec3 voxel_uvw_coord, in float view_dist, in vec4 vertex_fog) {
  // For distant fragments, use interpolated vertex fog (much cheaper)
  if (view_dist >= fog_distance) {
    return vertex_fog;
  }

  // For close fragments, do full per-fragment raymarching
  return calculate_fog(world_pos, view[0].xyz, fog_samples);
}

/**
 * @brief Calculate vertex lighting from voxel lights (unshadowed diffuse only).
 * @details Used for distant geometry where per-fragment lighting is too expensive.
 * @param world_pos The world-space position.
 * @param world_normal The world-space normal.
 * @return The accumulated diffuse lighting.
 */
vec3 calculate_vertex_lighting(in vec3 world_pos, in vec3 world_normal) {
  vec3 diffuse = vec3(0.0);

  ivec3 voxel_coord = voxel_xyz(world_pos);
  ivec2 data = voxel_light_data(voxel_coord);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    light_t light = lights[index];

    vec3 light_dir = light.origin.xyz - world_pos;
    float dist = length(light_dir);
    float radius = light.origin.w;
    float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

    if (atten > 0.0) {
      light_dir = normalize(light_dir);
      float lambert = max(0.0, dot(world_normal, light_dir));
      diffuse += light.color.rgb * light.color.a * atten * lambert * modulate;
    }
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }

    light_t light = lights[index];

    vec3 light_dir = light.origin.xyz - world_pos;
    float dist = length(light_dir);
    float radius = light.origin.w;
    float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

    if (atten > 0.0) {
      light_dir = normalize(light_dir);
      float lambert = max(0.0, dot(world_normal, light_dir));
      diffuse += light.color.rgb * light.color.a * atten * lambert * modulate;
    }
  }

  return diffuse;
}
