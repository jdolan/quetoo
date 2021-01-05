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
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffusemap;
layout (location = 5) in vec2 in_lightmap;
layout (location = 6) in vec4 in_color;

uniform mat4 model;

uniform stage_t stage;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;
} vertex;

/**
 * @brief
 */
void main(void) {

	mat4 model_view = view * model;

	vec4 position = vec4(in_position, 1.0);
	vec4 normal = vec4(in_normal, 0.0);
	vec4 tangent = vec4(in_tangent, 0.0);
	vec4 bitangent = vec4(in_bitangent, 0.0);

	stage_transform(stage, position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

	vertex.position = vec3(model_view * position);
	vertex.normal = vec3(model_view * normal);
	vertex.tangent = vec3(model_view * tangent);
	vertex.bitangent = vec3(model_view * bitangent);

	vertex.diffusemap = in_diffusemap;
	vertex.lightmap = in_lightmap;
	vertex.lightgrid = lightgrid_uvw(vec3(model * position));
	vertex.color = in_color;

	gl_Position = projection3D * vec4(vertex.position, 1.0);

	stage_vertex(stage, position.xyz, vertex.position, vertex.diffusemap, vertex.color);
}
