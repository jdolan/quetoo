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

layout (location = 7) in vec3 in_next_position;
layout (location = 8) in vec3 in_next_normal;
layout (location = 9) in vec3 in_next_tangent;
layout (location = 10) in vec3 in_next_bitangent;

uniform int model_type;
uniform mat4 model;
uniform float lerp;

uniform stage_t stage;

out vertex_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec3 lightgrid;
	vec4 color;
} vertex;

invariant gl_Position;

/**
 * @brief
 */
void main(void) {

	mat4 view_model = view * model;

	vec4 position = vec4(mix(in_position, in_next_position, lerp), 1.0);
	vec4 normal = vec4(mix(in_normal, in_next_normal, lerp), 0.0);
	vec4 tangent = vec4(mix(in_tangent, in_next_tangent, lerp), 0.0);
	vec4 bitangent = vec4(mix(in_bitangent, in_next_bitangent, lerp), 0.0);

	stage_transform(stage, position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

	vertex.model = vec3(model * position);
	vertex.position = vec3(view_model * position);
	vertex.normal = normalize(vec3(view_model * normal));
	vertex.tangent = normalize(vec3(view_model * tangent));
	vertex.bitangent = normalize(vec3(view_model * bitangent));

	vertex.diffusemap = in_diffusemap;
	vertex.lightmap = in_lightmap;
	vertex.lightgrid = lightgrid_uvw(vec3(model * position));
	vertex.color = in_color;

	gl_Position = projection3D * view_model * vec4(in_position, 1.0);

	stage_vertex(stage, position.xyz, vertex.position, vertex.diffusemap, vertex.color);
}

