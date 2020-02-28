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

#version 330

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_tangent;
layout (location = 3) in vec3 in_bitangent;
layout (location = 4) in vec2 in_diffusemap;
layout (location = 5) in vec2 in_lightmap;
layout (location = 6) in vec4 in_color;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 normal;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec4 color;
	vec3 eye;
} vertex;

/**
 * @brief
 */
void main(void) {

	gl_Position = projection * view * model * vec4(in_position, 1.0);

	vertex.position = (view * model * vec4(in_position, 1.0)).xyz;
	vertex.normal = normalize(vec3(normal * vec4(in_normal, 1.0)));
	vertex.tangent = normalize(vec3(normal * vec4(in_tangent, 1.0)));
	vertex.bitangent = normalize(vec3(normal * vec4(in_bitangent, 1.0)));

	vertex.diffusemap = in_diffusemap;
	vertex.lightmap = in_lightmap;
	vertex.color = in_color;

	vertex.eye.x = -dot(vertex.position, vertex.tangent);
	vertex.eye.y = -dot(vertex.position, vertex.bitangent);
	vertex.eye.z = -dot(vertex.position, vertex.normal);
}
