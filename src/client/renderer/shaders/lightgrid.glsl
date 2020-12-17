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
void lightgrid_fog(inout vec4 color, in sampler3D lightgrid_fog_sampler, in vec3 position, in vec3 lightgrid_uvw) {

	if (fog_density == 0.0) {
		return;
	}

	vec4 fog = vec4(0.0);

	float len = length(position);

	int num_samples = int(clamp(len / 16.0, 1, fog_samples));
	float num_samples_rcp = 1.0 / float(num_samples);
	
	for (int i = 0; i < num_samples; i++) {
		vec3 uvw = mix(lightgrid_uvw, lightgrid.view_coordinate.xyz, float(i) * num_samples_rcp);
		fog += texture(lightgrid_fog_sampler, uvw);
	}

	fog *= num_samples_rcp;

	float strength;
	strength  = clamp(fog.a * fog_density, 0.0, 1.0);
	strength *= clamp((len - 8.0) / (64.0 - 8.0), 0.0, 1.0);

	color.rgb = mix(color.rgb, fog.rgb, color.a * strength);
}