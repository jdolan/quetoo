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

uniform mat4 model;

uniform float lerp;

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
vec3 sample_lightgrid_ambient(in vec3 texcoord) {
	return texture(texture_lightgrid_ambient, texcoord).rgb * modulate;
}

/**
 * @brief
 */
vec3 sample_lightgrid_diffuse(in vec3 texcoord) {
	return texture(texture_lightgrid_diffuse, texcoord).rgb * modulate;
}

/**
 * @brief
 */
vec3 sample_lightgrid_direction(in vec3 texcoord) {
	vec3 direction = texture(texture_lightgrid_direction, texcoord).xyz * 2.0 - 1.0;
	return vec3(view * vec4(normalize(direction), 0.0));
}

/**
 * @brief
 */
vec3 sample_lightgrid_caustics(in vec3 texcoord) {
	return texture(texture_lightgrid_caustics, texcoord).rgb;
}

/**
 * @brief
 */
vec4 sample_lightgrid_fog(in vec3 texcoord) {

	vec4 fog = vec4(0.0);

	float samples = clamp(length(vertex.position) / BSP_LIGHTGRID_LUXEL_SIZE, 1.0, fog_samples);

	for (float i = 0; i < samples; i++) {

		vec3 xyz = mix(vertex.model, view[0].xyz, i / samples);
		vec3 uvw = mix(texcoord, lightgrid.view_coordinate.xyz, i / samples);

		fog += texture(texture_lightgrid_fog, uvw) * vec4(vec3(1.0), fog_density) * min(1.0, samples - i);
		if (fog.a >= 1.0) {
			break;
		}
	}

	if (hmax(fog.rgb) > 1.0) {
		fog.rgb /= hmax(fog.rgb);
	}

	return clamp(fog, 0.0, 1.0);
}

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
	vertex.color = vec4(1.0);

	if (view_type == VIEW_PLAYER_MODEL) {
		vertex.ambient = vec3(0.333);
		vertex.diffuse = vec3(0.666);
		vertex.direction = vec3(0.0, 0.0, 1.0);
	} else {
		vec3 texcoord = lightgrid_uvw(vec3(model * position));

		vertex.ambient = sample_lightgrid_ambient(texcoord);
		vertex.diffuse = sample_lightgrid_diffuse(texcoord);
		vertex.direction = sample_lightgrid_direction(texcoord);
		vertex.caustics = sample_lightgrid_caustics(texcoord);
		vertex.fog = sample_lightgrid_fog(texcoord);
	}

	gl_Position = projection3D * vec4(vertex.position, 1.0);

	stage_vertex(stage, position.xyz, vertex.position, vertex.diffusemap, vertex.color);
}

