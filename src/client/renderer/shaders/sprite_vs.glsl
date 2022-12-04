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

uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_fog;

out vertex_data {
	vec3 position;
	vec2 diffusemap;
	vec2 next_diffusemap;
	vec4 color;
	float lerp;
	float softness;
	vec4 fog;
} vertex;

/**
 * @brief Dynamic lighting for sprites
 */
void sprite_lighting(vec3 position, vec3 normal) {

	if (in_lighting == 0.0) {
		return;
	}

	vec3 light = vec3(0.0);

	vec3 grid_coord = lightgrid_uvw(in_position);
	light += texture(texture_lightgrid_ambient, grid_coord).rgb;
	light += texture(texture_lightgrid_diffuse, grid_coord).rgb;

	for (int i = 0; i < num_lights; i++) {

		float radius = lights[i].origin.w;
		if (radius <= 0.0) {
			continue;
		}

		float intensity = lights[i].color.w;
		if (intensity <= 0.0) {
			continue;
		}

		vec3 light_pos = (view * vec4(lights[i].origin.xyz, 1.0)).xyz;
		float atten = 1.0 - distance(light_pos, position) / radius;
		if (atten <= 0.0) {
			continue;
		}

		light += radius * lights[i].color.rgb * intensity * atten * atten;
	}

	vertex.color.rgb = mix(vertex.color.rgb, vertex.color.rgb * light * modulate, in_lighting);
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

	vec3 lightgrid_uvw = lightgrid_uvw(in_position);

	vertex.fog = vec4(0.0, 0.0, 0.0, 1.0);
	lightgrid_fog(vertex.fog, texture_lightgrid_fog, vertex.position, lightgrid_uvw);
	global_fog(vertex.fog, vertex.position);

	sprite_lighting(vertex.position, vec3(0.0, 0.0, 1.0)); // TODO: actual normals

	gl_Position = projection3D * view * position;
}
