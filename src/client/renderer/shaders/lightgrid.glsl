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

/**
 * @brief The lightgrid type.
 */
struct lightgrid_t {
	/**
	 * @brief The lightgrid mins, in world space.
	 */
	vec3 mins;

	/**
	 * @brief The ligthgrid maxs, in world space.
	 */
	vec3 maxs;

	/**
	 * @brief The view origin, in lightgrid space.
	 */
	vec3 view_coordinate;
};

/**
 * @brief Global fog scalar.
 */
uniform float fog;

/**
 * @param lightgrid The lightgrid struct instance.
 * @param position The vertex position in world space.
 * @return The lightgrid sample coordinate for the specified vertex.
 */
vec3 lightgrid_vertex(in lightgrid_t lightgrid, in vec3 position) {
	return (position - lightgrid.mins) / (lightgrid.maxs - lightgrid.mins);
}

/**
 * @brief Ray marches the fragment, sampling the fog texture at each iteration, accumulating the result.
 * @param lightgrid The lightgrid instance.
 * @param fog_texture The lightgrid fog texture sampler.
 * @param frag_position The fragment position in view space.
 * @param frag_coordinate The fragment lightgrid texture coordinate.
 * @param frag_color The intermediate fragment color.
 * @return The fog color.
 */
vec3 lightgrid_fog(in lightgrid_t lightgrid, in sampler3D fog_texture, in vec3 frag_position, in vec3 frag_coordinate, in vec4 frag_color) {

	vec3 fog_color = vec3(0.0);

	int iterations = max(1, int(length(frag_position) / 64.0));

	for (int i = 0; i < iterations; i++) {
		vec3 coordinate = mix(lightgrid.view_coordinate, frag_coordinate, float(i) / float(iterations));
		vec4 fog_sample = texture(fog_texture, coordinate);
		fog_color += fog_sample.rgb * fog_sample.a;
	}

	fog_color = frag_color.a * fog * fog_color / float(iterations);
	return frag_color.rgb + fog_color;
}
