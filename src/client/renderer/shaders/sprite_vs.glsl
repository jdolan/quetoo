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
 * @brief Lights the sprite using the shared voxel lighting model from light.glsl
 * (ambient + occlusion + exposure + per-light diffuse + caustics), blended by the
 * sprite's lighting weight. Billboards have no surface normal, so a camera-facing
 * normal is synthesized for `vertex_light`'s lambert term and the sky sample.
 */
void sprite_lighting(void) {

  if (in_lighting == 0.0) {
    return;
  }

  common_vertex_t v;
  v.model_position = in_position;

  // camera world position = -R^T * t from the (orthonormal) view matrix
  vec3 camera = -transpose(mat3(view)) * view[3].xyz;
  v.model_normal = normalize(camera - in_position);

  v.voxel = voxel_uvw(in_position);
  v.ambient = vec3(0.0);
  v.diffuse = vec3(0.0);
  v.caustics = 0.0;

  vertex_lighting(v);

  vec3 lit = vertex.color * (v.ambient + v.diffuse);
  vertex.color = mix(vertex.color, lit, in_lighting);
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

  sprite_lighting();

  gl_Position = projection3D * view * position;
}
