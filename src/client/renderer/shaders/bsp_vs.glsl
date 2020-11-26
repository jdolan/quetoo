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

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

uniform vec3 view_origin;

uniform stage_t stage;

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;

uniform vec3 lightgrid_mins;
uniform vec3 lightgrid_maxs;

out vertex_data {
	vec3 position;
	vec3 normal;
	vec3 tangent;
	vec3 bitangent;
	vec2 diffusemap;
	vec2 lightmap;
	vec4 color;
	vec4 fog;
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

	// fun fact: position doesn't actually need a model matrix
	vertex.position = vec3(model_view * position);
	vertex.normal = vec3(model_view * normal);
	vertex.tangent = vec3(model_view * tangent);
	vertex.bitangent = vec3(model_view * bitangent);

	vertex.diffusemap = in_diffusemap;
	vertex.lightmap = in_lightmap;
	vertex.color = in_color;

	{
		// raymarch lightgrid to accumulate fog

		vec3 begin = (view_origin  - lightgrid_mins) / (lightgrid_maxs - lightgrid_mins);
		vec3 end = (position.xyz - lightgrid_mins) / (lightgrid_maxs - lightgrid_mins);

		float step_dist = 32.0f;
		int step_count = int(floor(length(position.xyz - view_origin) / step_dist));

		vertex.fog = vec4(0.0, 0.0, 0.0, 1.0);
		for (int i = 1; i < step_count; i++) {
			float t = float(i) / float(step_count);
			vec3 coordinate = mix(begin, end, t);
			vertex.fog.rgb += texture(texture_lightgrid_diffuse, coordinate).rgb;
		}
		vertex.fog.rgb /= float(step_count);
	}

	gl_Position = projection * vec4(vertex.position, 1.0);

	stage_vertex(stage, position.xyz, vertex.position, vertex.diffusemap, vertex.color);
}
