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

#define MAX_LIGHTS 64

/**
 * @brief Dynamic light source struct.
 */
struct light {
	vec4 origin;
	vec4 color;
};

/**
 * @brief Dynamic light sources, in view space.
 */
layout (std140) uniform lights_block {
	light lights[MAX_LIGHTS];
};

/**
 * @brief The number of active dynamic light sources.
 */
uniform int lights_active;

/**
 * @brief
 */
vec3 directional_light(in vec3 position,
		   in vec3 normal,
		   in vec3 light_dir,
		   in vec3 light_color,
		   in float specularity) {

	vec3 diffuse = light_color * max(0.0, dot(normal, light_dir));

	vec3 eye = normalize(position);
	vec3 specular = light_color * pow(max(dot(reflect(-light_dir, normal), eye), 0.0), specularity * 64);

	return diffuse + specular;
}

/**
 * @brief
 */
vec3 dynamic_light(in vec3 position,
				   in vec3 normal,
				   in float specularity) {

	vec3 dynamic = vec3(0.0);

	for (int i = 0; i < lights_active; i++) {

		float radius = lights[i].origin.w;
		float dist = distance(lights[i].origin.xyz, position);
		if (dist < radius) {

			vec3 light_dir = normalize(lights[i].origin.xyz - position);
			vec3 light_color = lights[i].color.rgb * lights[i].color.a * 4; // FIXME: lol 4

			float attenuation = smoothstep(1.0, 0.0, dist / radius);
			dynamic += attenuation * directional_light(position, normal, light_dir, light_color, specularity);
		}
	}

	return dynamic;
}
