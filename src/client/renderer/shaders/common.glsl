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

uniform float brightness;
uniform float contrast;
uniform float saturation;
uniform float gamma;

/**
 * @brief
 */
vec4 ColorFilter(in vec4 color) {

	vec3 luminance = vec3(0.2125, 0.7154, 0.0721);
	vec3 bias = vec3(0.5);

	vec3 scaled = mix(vec3(1.0), color.rgb, gamma) * brightness;

	color.rgb = mix(bias, mix(vec3(dot(luminance, scaled)), scaled, saturation), contrast);

	return color;
}

/**
 * @brief Phong BRDF for specular highlights.
 */
vec3 brdf_phong(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	vec3 reflection = reflect(light_dir, normal);
	float r_dot_v = max(-dot(view_dir, reflection), 0.0);
	return light_color * specular_intensity * pow(r_dot_v, 16.0 * specular_exponent);
}

/**
 * @brief Blinn-Phong BRDF for specular highlights.
 */
vec3 brdf_blinn(vec3 view_dir, vec3 light_dir, vec3 normal,
	vec3 light_color, float specular_intensity, float specular_exponent) {

	const float exponent = 16.0 * 4.0; // roughly matches phong this way
	vec3 half_angle = normalize(light_dir + view_dir);
	float n_dot_h = max(dot(normal, half_angle), 0.0);
	return light_color * specular_intensity * pow(n_dot_h, exponent * specular_exponent);
}

/**
 * @brief Lambert BRDF for diffuse lighting.
 */
vec3 brdf_lambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * max(dot(light_dir, normal), 0.0);
}

/**
 * @brief Half-Lambert BRDF for diffuse lighting.
 */
vec3 brdf_halflambert(vec3 light_dir, vec3 normal, vec3 light_color) {
	return light_color * (1.0 - (dot(normal, light_dir) * 0.5 + 0.5));
}

