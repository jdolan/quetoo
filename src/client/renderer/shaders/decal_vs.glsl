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
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in vec4 in_color;
layout (location = 4) in uint in_time;
layout (location = 5) in uint in_lifetime;

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

  mat4 view_model = view * model;

  vec4 position = vec4(in_position, 1.0);
  vec4 normal = vec4(in_normal, 0.0);

  vertex.model_position = vec3(model * position);
  vertex.model_normal = normalize(vec3(model * normal));
  vertex.position = vec3(view_model * position);
  vertex.normal = normalize(vec3(view_model * normal));
  vertex.tangent = vec3(1.0, 0.0, 0.0);
  vertex.bitangent = vec3(0.0, 1.0, 0.0);
  vertex.diffusemap = in_texcoord;
  vertex.voxel = voxel_uvw(vec3(model * position));
  vertex.color = in_color;

  vertex_lighting(vertex);

  decal.time = in_time;
  decal.lifetime = in_lifetime;

  gl_Position = projection3D * view * model * position;
}
