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
layout (location = 1) in vec2 in_diffusemap;
layout (location = 2) in vec2 in_next_diffusemap;
layout (location = 3) in vec3 in_color;
layout (location = 4) in float in_lerp;
layout (location = 5) in float in_softness;
layout (location = 6) in float in_lighting;

out vertex_data {
  vec3 position;
  vec2 diffusemap;
  vec2 next_diffusemap;
  vec3 color;
  float lerp;
  float softness;
} vertex;

/**
 * @brief
 */
vec3 sprite_fragment_lighting_light(in int index) {

  light_t light = lights[index];

  float dist = distance(light.origin.xyz, in_position);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

  return light.color.rgb * light.color.a * atten * modulate;
}

/**
 * @brief Dynamic lighting for sprites
 */
void sprite_fragment_lighting(void) {

  if (in_lighting == 0.0) {
    return;
  }

  vec3 diffuse = vec3(0.0);

  ivec3 voxel = voxel_xyz(in_position);
  ivec2 data = voxel_light_data(voxel);

  for (int i = 0; i < data.y; i++) {
    int index = voxel_light_index(data.x + i);
    diffuse += sprite_fragment_lighting_light(index);
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }

    diffuse += sprite_fragment_lighting_light(index);
  }

  vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * diffuse, in_lighting);
}

/**
 * @brief
 */
void main(void) {

  vec4 position = vec4(in_position, 1.0);

  vertex.position = vec3(view * position);
  vertex.diffusemap = in_diffusemap;
  vertex.next_diffusemap = in_next_diffusemap;
  vertex.color = in_color;
  vertex.lerp = in_lerp;
  vertex.softness = in_softness;

  vec3 texcoord = voxel_uvw(in_position);

  sprite_fragment_lighting();

  gl_Position = projection3D * view * position;
}
