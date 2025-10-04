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
layout (location = 7) in float in_bloom;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	vec4 fog;
	float lerp;
	float softness;
	float bloom;
} vertex;

/**
 * @brief
 */
vec4 sample_voxel_fog(in vec3 texcoord) {

	vec4 fog = vec4(0.0);

	float samples = clamp(length(vertex.position) / BSP_VOXEL_SIZE, 1.0, fog_samples);

	for (float i = 0; i < samples; i++) {

		vec3 xyz = mix(vertex.position, view[0].xyz, i / samples);
		vec3 uvw = mix(texcoord, voxels.view_coordinate.xyz, i / samples);

		fog += texture(texture_voxel_fog, uvw) * vec4(vec3(1.0), fog_density) * min(1.0, samples - i);
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
vec3 light_and_shadow_light(in light_t light) {

	float dist = distance(light.origin.xyz, in_position);
	float radius = light.origin.w;
	float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

	return light.color.rgb * light.color.a * atten * modulate;
}

/**
 * @brief Dynamic lighting for sprites
 */
void light_and_shadow(void) {

	if (in_lighting == 0.0) {
		return;
	}

	vec3 diffuse = vec3(0.0);

	for (int i = 0; i < MAX_LIGHTS; i++) {

		int index = active_lights[i];
		if (index == -1) {
			break;
		}

		light_t light = lights[index];

		if (box_contains(light.mins.xyz, light.maxs.xyz, in_position.xyz)) {
			diffuse += light_and_shadow_light(light);
		}
	}

	vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * diffuse, in_lighting);
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
	vertex.bloom = in_bloom;

	vec3 texcoord = voxel_uvw(in_position);

	vertex.fog = sample_voxel_fog(texcoord);

	light_and_shadow();

	gl_Position = projection3D * view * position;
}
