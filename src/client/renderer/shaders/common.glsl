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
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec2 v) {
  return max(v.x, v.y);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec3 v) {
  return max(max(v.x, v.y), v.z);
}

/**
 * @brief Horizontal maximum, returns the maximum component of a vector.
 */
float hmax(vec4 v) {
  return max(max(v.x, v.y), max(v.z, v.w));
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec2 v) {
  return min(v.x, v.y);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec3 v) {
  return min(min(v.x, v.y), v.z);
}

/**
 * @brief Horizontal minimum, returns the minimum component of a vector.
 */
float hmin(vec4 v) {
  return min(min(v.x, v.y), min(v.z, v.w));
}

/**
 * @brief Inverse of v.
 */
vec3 invert(vec3 v) {
  return vec3(1.0) - v;
}

/**
 * @brief Clamps to [0.0, 1.0], like in HLSL.
 */
float saturate(float x) {
  return clamp(x, 0.0, 1.0);
}

/**
 * @brief Clamp value t to range [a,b] and map [a,b] to [0,1].
 */
float linearstep(float a, float b, float t) {
  return clamp((t - a) / (b - a), 0.0, 1.0);
}

/**
 * @brief
 */
vec3 hash33(vec3 p) {
  p = fract(p * vec3(.1031,.11369,.13787));
  p += dot(p, p.yxz + 19.19);
  return -1.0 + 2.0 * fract(vec3((p.x + p.y) * p.z, (p.x + p.z) * p.y, (p.y + p.z) * p.x));
}

/**
 * @brief https://www.shadertoy.com/view/4sc3z2
 */
float noise3d(vec3 p) {
  vec3 pi = floor(p);
  vec3 pf = p - pi;

  vec3 w = pf * pf * (3.0 - 2.0 * pf);

  return mix(
    mix(
      mix(dot(pf - vec3(0, 0, 0), hash33(pi + vec3(0, 0, 0))),
        dot(pf - vec3(1, 0, 0), hash33(pi + vec3(1, 0, 0))),
        w.x),
      mix(dot(pf - vec3(0, 0, 1), hash33(pi + vec3(0, 0, 1))),
        dot(pf - vec3(1, 0, 1), hash33(pi + vec3(1, 0, 1))),
        w.x),
      w.z),
    mix(
      mix(dot(pf - vec3(0, 1, 0), hash33(pi + vec3(0, 1, 0))),
        dot(pf - vec3(1, 1, 0), hash33(pi + vec3(1, 1, 0))),
        w.x),
      mix(dot(pf - vec3(0, 1, 1), hash33(pi + vec3(0, 1, 1))),
        dot(pf - vec3(1, 1, 1), hash33(pi + vec3(1, 1, 1))),
        w.x),
      w.z),
    w.y);
}

/**
 * @brief Calculates a lookAt matrix for the given parameters.
 * @see Mat4_LookAt
 */
mat4 lookAt(vec3 eye, vec3 pos, vec3 up) {

  vec3 Z = normalize(eye - pos);
  vec3 X = normalize(cross(up, Z));
  vec3 Y = normalize(cross(Z, X));

  return mat4(
    vec4(X.x, Y.x, Z.x, 0.0),
    vec4(X.y, Y.y, Z.y, 0.0),
    vec4(X.z, Y.z, Z.z, 0.0),
    vec4(-dot(X, eye), -dot(Y, eye), -dot(Z, eye), 1.0)
  );
}

/**
 * @brief Resolves the voxel coordinate for the specified position in world space.
 * @param position The position in world space.
 * @return The voxel coordinate for the specified position.
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
vec3 light_fog(in vec3 position) {

  vec3 diffuse = vec3(0.0);

  ivec3 voxel = voxel_xyz(position);
  ivec2 data = voxel_light_data(voxel);

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
 * @brief Calculate volumetric fog for a vertex (used for distant geometry).
 * @details Performs simplified raymarching from vertex to view with fewer samples
 * than per-fragment fog. The result is interpolated across the triangle.
 * @param world_pos The world-space position of the vertex.
 * @return The fog color (rgb) and density (a).
 */
vec4 calculate_vertex_fog(in vec3 world_pos) {
  vec4 fog = vec4(0.0);
  
  vec3 view_pos = view[0].xyz;
  float dist = distance(world_pos, view_pos);
  
  float samples = clamp(dist / BSP_VOXEL_SIZE, 1.0, fog_samples);
  
  vec3 voxel_start = voxel_uvw(world_pos);
  vec3 voxel_end = voxels.view_coordinate.xyz;
  
  for (float i = 0; i < samples; i++) {
    vec3 xyz = mix(world_pos, view_pos, i / samples);
    vec3 uvw = mix(voxel_start, voxel_end, i / samples);
    
    float fog_density_sample = voxel_fog_density(uvw);
    
    if (fog_density_sample > 0.0) {
      vec3 fog_lighting = light_fog(xyz);
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
