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
#define WORLD_TRACE_DISTANCE 1024.0
#define MAX_WORLD_DIST 11586.0

/**
 * @brief Clamps softly to 1.0 for values up to 1.5, to prevent an unsightly hard cutoff.
 */
float soft_clip_fog(float x) {
	x = clamp(x, 0.0, 1.5);
	float x3 = x * x * x;
	return 0.5 * (2.0 * x - x3 / 3.375);
}

/**
 * @brief Draws the boundaries of the lightgrid voxels;
 */
vec4 lightgrid_raster(vec3 uvw, float distance) {
	float alpha = 1.0 - clamp(distance / WORLD_TRACE_DISTANCE, 0.0, 1.0);
	vec4 c = vec4(1.0);
	c.rgb = fract(uvw * lightgrid.size.xyz);
	c.rgb = abs(c.rgb * 2.0 - 1.0);
	float t = 0.7 + (0.25 * alpha);
	float m = max(max(c.r, c.g), c.b);
	return vec4(
		smoothstep(t, 1.0, c.r),
		smoothstep(t, 1.0, c.g),
		smoothstep(t, 1.0, c.b),
		smoothstep(t, 1.0, m) * alpha);
}

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

	if (fog_density == 0.0) {
		return;
	}

	vec4 fog = vec4(0.0);

	int num_samples = int(clamp(length(position) / 16.0, 1, fog_samples));
	float num_samples_rcp = 1.0 / float(num_samples);

	for (int i = 0; i < num_samples; i++) {
		vec3 uvw = mix(lightgrid_uvw, lightgrid.view_coordinate.xyz, float(i) * num_samples_rcp);
		fog += texture(lightgrid_fog_sampler, uvw);
	}

	fog *= num_samples_rcp;

	float density = clamp(fog.a * fog_density, 0.0, 1.0);

	// reduce density around the view origin to look more natural and avoid banding
	density *= saturate((length(position) - 10.0) / 110.0);

	color.rgb = mix(color.rgb, fog.rgb, color.a * density);
}

/**
 * @brief
 * @param color The input and output color.
 * @param lightgrid_caustics_sampler The lightgrid caustics texture sampler.
 * @param position The position in world space.
 * @param lightgrid_uvw The lightgrid texture coordinate.
 */
void lightgrid_caustics(inout vec3 color, in sampler3D lightgrid_caustics_sampler,
						in vec3 lightgrid_uvw) {

	float hz = 0.5;
	float thickness = 0.05;
	float glow = 3.0;
	float intensity = texture(lightgrid_caustics_sampler, lightgrid_uvw).r;

	if (intensity == 0.0) {
		return;
	}

	// scale the uvw based on the lightgrid size, so that caustics look the same on all maps
	float scale = 120 * distance(lightgrid.maxs, lightgrid.mins) / MAX_WORLD_DIST;

	// grab raw 3d noise
	float factor = noise3d(lightgrid_uvw * scale + (ticks / 1000.0) * hz);

	// scale to make very close to -1.0 to 1.0 based on observational data
	factor = factor * (0.3515 * 2.0);

	// make the inner edges stronger, clamp to 0-1
	factor = clamp(pow((1.0 - abs(factor)) + thickness, glow), 0.0, 1.0);

	// add it up
	color = clamp(color + factor * intensity, 0.0, 1.0);
}
