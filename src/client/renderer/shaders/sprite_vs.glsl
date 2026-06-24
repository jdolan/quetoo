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
layout (location = 5) in float in_lighting;

out vertex_data {
  vec3 position;
  vec2 diffusemap;
  vec2 next_diffusemap;
  vec3 color;
  float lerp;
} vertex;

/**
 * @brief
 */
vec3 sprite_lighting_light(in int index) {

  light_t light = lights[index];

  float dist = distance(light.origin.xyz, in_position);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

  return light_color(light) * atten;
}

/**
 * @brief Per-channel darkening from a "dark" light (negative intensity), matching dark_light()
 * in light.glsl. Lets a dark light absorb sprites -- including emissive ones such as projectiles
 * and weapon glows -- so they fade through the light's color to black inside a void.
 */
vec3 sprite_dark_light(in int index) {

  light_t light = lights[index];

  float dist = distance(light.origin.xyz, in_position);
  float atten = clamp(1.0 - dist / light.origin.w, 0.0, 1.0);

  float k = clamp(1.0 - atten * (-light.color.a), 0.0, 1.0);
  return k * (k + (1.0 - k) * light.color.rgb);
}

/**
 * @brief Dynamic lighting for sprites.
 * @details Positive lights only modulate lit sprites (in_lighting > 0), but dark lights absorb
 * every sprite, emissive ones included, so projectiles and glows fade inside a void.
 */
void sprite_lighting(void) {

  // Common path: no dark lights. Emissive sprites (in_lighting == 0) skip lighting entirely,
  // exactly as before -- no per-sprite light loop.
  if (num_dark_lights == 0) {

    if (in_lighting == 0.0) {
      return;
    }

    vec3 diffuse = vec3(0.0);

    if (editor == 0) {
      ivec2 data = voxel_light_data(voxel_xyz(in_position));
      for (int i = 0; i < data.y; i++) {
        diffuse += sprite_lighting_light(voxel_light_index(data.x + i));
      }
    }

    for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
      int index = active_lights[i];
      if (index == -1) {
        break;
      }
      diffuse += sprite_lighting_light(index);
    }

    vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * diffuse, in_lighting);
    return;
  }

  // Dark lights present: positive lights still only modulate lit sprites, but dark lights absorb
  // every sprite -- emissive ones (projectiles, glows) included -- so they fade inside a void.
  vec3 diffuse = vec3(0.0);
  vec3 darkness = vec3(1.0);

  if (editor == 0) {
    ivec2 data = voxel_light_data(voxel_xyz(in_position));
    for (int i = 0; i < data.y; i++) {
      int index = voxel_light_index(data.x + i);
      if (lights[index].color.a < 0.0) {
        darkness *= sprite_dark_light(index);
      } else {
        diffuse += sprite_lighting_light(index);
      }
    }
  }

  for (int i = 0; i < MAX_DYNAMIC_LIGHTS; i++) {
    int index = active_lights[i];
    if (index == -1) {
      break;
    }
    if (lights[index].color.a < 0.0) {
      darkness *= sprite_dark_light(index);
    } else {
      diffuse += sprite_lighting_light(index);
    }
  }

  if (in_lighting != 0.0) {
    vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * diffuse, in_lighting);
  }

  vertex.color.rgb *= darkness;
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

  vec3 texcoord = voxel_uvw(in_position);

  sprite_lighting();

  gl_Position = projection3D * view * position;
}
