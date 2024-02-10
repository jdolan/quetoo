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
layout (location = 3) in vec4 in_color;
layout (location = 4) in float in_lerp;
layout (location = 5) in float in_softness;
layout (location = 6) in float in_lighting;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	vec4 fog;

	float lerp;
	float softness;
} vertex;

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
vec4 sample_lightgrid_fog(in vec3 texcoord) {

	vec4 fog = vec4(0.0);

	float samples = clamp(length(vertex.position) / BSP_LIGHTGRID_LUXEL_SIZE, 1.0, fog_samples);

	for (float i = 0; i < samples; i++) {

		vec3 xyz = mix(vertex.position, view[0].xyz, i / samples);
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
 * @brief Dynamic lighting for sprites
 */
void light_and_shadow(in vec3 texcoord) {

	if (in_lighting == 0.0) {
		return;
	}

	vec3 ambient = sample_lightgrid_ambient(texcoord);
	vec3 diffuse = sample_lightgrid_diffuse(texcoord);

	for (int i = 0; i < num_lights; i++) {

		int type = int(lights[i].position.w);
		if (type != LIGHT_DYNAMIC) {
			continue;
		}

		float radius = lights[i].model.w;
		if (radius <= 0.0) {
			continue;
		}

		float intensity = lights[i].color.w;
		if (intensity <= 0.0) {
			continue;
		}

		vec3 light_pos = lights[i].position.xyz;
		float atten = 1.0 - distance(light_pos, vertex.position) / radius;
		if (atten <= 0.0) {
			continue;
		}

		diffuse += radius * lights[i].color.rgb * intensity * atten * atten;
	}

	vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * (ambient + diffuse), in_lighting);
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
	vertex.softness = in_softness;

	vec3 texcoord = lightgrid_uvw(in_position);

	vertex.fog = sample_lightgrid_fog(texcoord);

	light_and_shadow(texcoord);

	gl_Position = projection3D * view * position;
}
