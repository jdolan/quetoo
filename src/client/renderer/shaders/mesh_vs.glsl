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

layout (location = 5) in vec3 in_next_position;
layout (location = 6) in vec3 in_next_normal;
layout (location = 7) in vec3 in_next_tangent;
layout (location = 8) in vec3 in_next_bitangent;

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;
uniform sampler3D texture_lightgrid_caustics;
uniform sampler3D texture_lightgrid_fog;

uniform mat4 model;

uniform float lerp;

uniform vec4 color;

uniform stage_t stage;

out vertex_data {
	vec3 model;
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec4 color;
	vec3 ambient;
	vec3 diffuse;
	vec3 direction;
	vec3 caustics;
	vec4 fog;
} vertex;

invariant gl_Position;

/**
 * @brief
 */
void main(void) {

	mat4 model_view = view * model;

	vec4 position = vec4(mix(in_position, in_next_position, lerp), 1.0);
	vec4 normal = vec4(mix(in_normal, in_next_normal, lerp), 0.0);
	vec4 tangent = vec4(mix(in_tangent, in_next_tangent, lerp), 0.0);
	vec4 bitangent = vec4(mix(in_bitangent, in_next_bitangent, lerp), 0.0);

	stage_transform(stage, position.xyz, normal.xyz, tangent.xyz, bitangent.xyz);

	vertex.model = vec3(model * position);
	vertex.position = vec3(model_view * position);
	vertex.normal = vec3(model_view * normal);
	vertex.tangent = vec3(model_view * tangent);
	vertex.bitangent = vec3(model_view * bitangent);

	vertex.diffusemap = in_diffusemap;
	vertex.color = color;

	if (view_type == VIEW_PLAYER_MODEL) {
		vertex.ambient = vec3(1.0);
		vertex.diffuse = vec3(1.0);
		vertex.direction = vec3(0.0, 0.0, 1.0);
		vertex.caustics = vec3(0.0);
		vertex.fog = vec4(0.0, 0.0, 0.0, 1.0);
	} else {

		vec3 lightgrid_uvw = lightgrid_uvw(vec3(model * position));

		vertex.ambient = texture(texture_lightgrid_ambient, lightgrid_uvw).rgb;
		vertex.diffuse = texture(texture_lightgrid_diffuse, lightgrid_uvw).rgb;
		vec3 direction = texture(texture_lightgrid_direction, lightgrid_uvw).xyz;
		vertex.direction = (view * vec4(normalize(direction), 0.0)).xyz;
		vertex.caustics = texture(texture_lightgrid_caustics, lightgrid_uvw).rgb;
		vertex.fog = vec4(0.0, 0.0, 0.0, 1.0);

		lightgrid_fog(vertex.fog, texture_lightgrid_fog, vertex.position, lightgrid_uvw);
		global_fog(vertex.fog, vertex.position);
	}

	gl_Position = projection3D * vec4(vertex.position, 1.0);

	stage_vertex(stage, position.xyz, vertex.position, vertex.diffusemap, vertex.color);
}
