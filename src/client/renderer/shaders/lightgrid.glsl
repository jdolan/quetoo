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

#define BSP_LIGHTGRID_LUXEL_SIZE 32

/**
 * @brief Resolves the lightgrid coordinate for the specified position in world space.
 * @param lightgrid The lightgrid struct instance.
 * @param position The position in world space.
 * @return The lightgrid coordinate for the specified position.
 */
vec3 lightgrid_uvw(in vec3 position) {
	return (position - lightgrid.mins.xyz) / (lightgrid.maxs.xyz - lightgrid.mins.xyz);
}

/**
 * @brief Ray marches the point, sampling the fog texture at each iteration.
 * @param color The input and output fragment color.
 * @param lightgrid_fog_sampler The lightgrid fog texture sampler.
 * @param position The position in view space.
 * @param lightgrid_uvw The lightgrid texture coordinate.
 */
void lightgrid_fog(inout vec4 color, in sampler3D lightgrid_fog_sampler,
				   in vec3 position, in vec3 lightgrid_uvw) {

	if (fog_samples == 0) {
		return;
	}

	vec4 fog = vec4(0.0);

	int num_samples = int(clamp(length(position) / (BSP_LIGHTGRID_LUXEL_SIZE / 2.f), 1, fog_samples));

	// Add some movement to fog volumes by shifting the sample coordinates over time
	vec3 drift_size = lightgrid.luxel_size.xyz * vec3(0.25f, 0.25f, 0.125f);
	vec3 drift_seed = vec3(ticks / 2000.f, ticks / 3000.f, ticks / 5000.f);

	vec3 drift = sin(drift_seed) * drift_size;

	for (int i = 0; i < num_samples; i++) {

		vec3 uvw = mix(lightgrid_uvw + drift, lightgrid.view_coordinate.xyz, float(i) / float(num_samples));
		fog = max(fog, texture(lightgrid_fog_sampler, uvw));
	}

	float density = clamp(fog.a * fog_density, 0.0, 1.0);

	// reduce density around the view origin to look more natural and avoid banding
	density *= saturate((length(position) - 10.0) / 110.0);

	color.rgb = mix(color.rgb, fog.rgb, color.a * density);
}
