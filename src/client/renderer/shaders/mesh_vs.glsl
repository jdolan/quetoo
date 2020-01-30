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
layout (location = 4) in vec2 in_diffuse;

layout (location = 5) in vec3 in_next_position;
layout (location = 6) in vec3 in_next_normal;
layout (location = 7) in vec3 in_next_tangent;
layout (location = 8) in vec3 in_next_bitangent;

uniform mat4 projection;
uniform mat4 model_view;
uniform mat4 model;
uniform mat4 normal;

uniform float lerp;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffuse;
	vec3 world_position;
	vec3 eye;
} vertex;

/**
 * @brief
 */
void main(void) {

	vec4 lerp_position = vec4(mix(in_position, in_next_position, lerp), 1.0);
	vec4 lerp_normal = vec4(mix(in_normal, in_next_normal, lerp), 1.0);
	vec4 lerp_tangent = vec4(mix(in_tangent, in_next_tangent, lerp), 1.0);
	vec4 lerp_bitangent = vec4(mix(in_bitangent, in_next_bitangent, lerp), 1.0);

	gl_Position = projection * model_view * lerp_position;

	vertex.position = vec3(model_view * lerp_position);
	vertex.normal = normalize(vec3(normal * lerp_normal));
	vertex.tangent = normalize(vec3(normal * lerp_tangent));
	vertex.bitangent = normalize(vec3(normal * lerp_bitangent));

	vertex.diffuse = in_diffuse;

	vertex.eye.x = -dot(vertex.position, vertex.tangent);
	vertex.eye.y = -dot(vertex.position, vertex.bitangent);
	vertex.eye.z = -dot(vertex.position, vertex.normal);

	vertex.world_position = vec3(model * lerp_position);
}

