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
layout (location = 4) in vec2 in_diffusemap;

out common_vertex_t vertex;

// Sky-specific cubemap coordinate
out vec3 cubemap_coord;

invariant gl_Position;

/**
 * @brief
 */
void main(void) {

  vec4 position = vec4(in_position, 1.0);

  vertex.model_position = in_position;
  vertex.position = vec3(view * position);
  cubemap_coord = vec3(sky_projection * position);
  vertex.voxel = voxel_uvw(in_position);
  vertex.fog = vertex_fog(vertex);
  
  // Initialize unused fields
  vertex.model_normal = normalize(in_position);
  vertex.normal = normalize(vec3(view * vec4(in_position, 0.0)));
  vertex.smooth_normal = vertex.normal;
  vertex.tangent = vec3(1.0, 0.0, 0.0);
  vertex.bitangent = vec3(0.0, 1.0, 0.0);
  vertex.tbn = mat3(1.0);
  vertex.inverse_tbn = mat3(1.0);
  vertex.diffusemap = in_diffusemap;
  vertex.color = vec4(1.0);
  vertex.ambient = vec3(0.0);
  vertex.caustics = 0.0;
  vertex.lighting = vec3(1.0); // Sky is self-lit

  stage_vertex(stage, in_position, vertex.position, vertex.diffusemap, vertex.color);

  gl_Position = projection3D * view * position;
}
