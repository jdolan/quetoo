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

#define MAX_LIGHTS 32

struct light {
	vec4 origin;
	vec4 color;
};

layout (std140) uniform lights_block {
	light lights[MAX_LIGHTS];
};

uniform int lights_mask;

/**
 * @brief
 */
void dynamic_light(in vec3 position, in vec3 normal, in float specular_exponent,
				   inout vec3 diff_light, inout vec3 spec_light,
				   inout vec3 debug) {

	for (int i = 0; i < MAX_LIGHTS; i++) {

		if ((lights_mask & (1 << i)) == 0) {
			continue;
		}

		float radius = lights[i].origin.w;
		if (radius == 0.0) {
			continue;
		}

		float intensity = lights[i].color.w;
		if (intensity == 0.0) {
			continue;
		}

		float dist = distance(lights[i].origin.xyz, position);
		if (dist < radius) {

			vec3 light_dir = normalize(lights[i].origin.xyz - position);
			// debug.rgb = light_dir; // debuggery
			float angle_atten = dot(light_dir, normal);
			// debug.rgb = vec3(angle_atten); // debuggery
			if (angle_atten > 0.0) {
				
				float dist_atten;
				dist_atten = 1.0 - dist / radius;
				dist_atten *= dist_atten; // for looks, not for correctness
				
				float attenuation = dist_atten * angle_atten;
				
				vec3 view_dir = normal;//normalize(-position);
				// debug.rgb = view_dir; // debuggery
				vec3 half_dir = normalize(light_dir + view_dir);
				// debug.rgb = half_dir; // debuggery
				float specular_base = dot(half_dir, normal);
				// debug.rgb = vec3(specular_base); // debuggery
				float specular = pow(specular_base, specular_exponent);
				// debug.rgb = vec3(specular); // debuggery
				
				vec3 color = lights[i].color.rgb * intensity;
				// debug.rgb = color; // debuggery
				
				diff_light += attenuation * radius * color;
				// debug.rgb = diff_light; // debuggery
				spec_light += attenuation * attenuation * radius * specular * color;
				// debug.rgb = spec_light; // debuggery
			}
		}
	}
}
