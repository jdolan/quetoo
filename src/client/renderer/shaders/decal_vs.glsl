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

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texcoord;
layout (location = 2) in vec4 in_color;
layout (location = 3) in uint in_time;
layout (location = 4) in uint in_lifetime;

out common_vertex_t vertex;

out decal_data {
  flat uint time;
  flat uint lifetime;
} decal;

uniform mat4 model;

/**
 * @brief Decal vertex shader.
 */
void main(void) {

  vec4 position = vec4(in_position, 1.0);

  vertex.model_position = in_position;
  vertex.position = vec3(view * position);
  vertex.diffusemap = in_texcoord;
  vertex.color = in_color;
  
  // Calculate vertex lighting for decals (unshadowed diffuse)
  vec3 diffuse = vec3(0.0);
  
  ivec3 voxel_coord = voxel_xyz(in_position);
  ivec2 data = voxel_light_data(voxel_coord);
  
  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    light_t light = lights[index];
    
    float dist = distance(light.origin.xyz, in_position);
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
    
    float dist = distance(light.origin.xyz, in_position);
    float radius = light.origin.w;
    float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
    
    diffuse += light.color.rgb * light.color.a * atten * modulate;
  }
  
  vertex.lighting = diffuse;
  vertex.fog = calculate_vertex_fog(in_position);
  
  // Decal-specific data
  decal.time = in_time;
  decal.lifetime = in_lifetime;
  
  // Initialize unused fields
  vertex.model_normal = vec3(0.0, 0.0, 1.0);
  vertex.normal = vec3(0.0, 0.0, 1.0);
  vertex.smooth_normal = vec3(0.0, 0.0, 1.0);
  vertex.tangent = vec3(1.0, 0.0, 0.0);
  vertex.bitangent = vec3(0.0, 1.0, 0.0);
  vertex.tbn = mat3(1.0);
  vertex.inverse_tbn = mat3(1.0);
  vertex.voxel = voxel_uvw(in_position);
  vertex.ambient = vec3(0.0);
  vertex.caustics = 0.0;

  gl_Position = projection3D * view * model * position;
}
