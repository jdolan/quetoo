
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
 * @brief The lightgrid struct.
 */
struct lightgrid_t {
	/**
	 * @brief The lightgrid mins, in world space.
	 */
	vec4 mins;

	/**
	 * @brief The lightgrid maxs, in world space.
	 */
	vec4 maxs;

	/**
	 * @brief The view origin, in lightgrid space.
	 */
	vec4 view_coordinate;

	/**
	 * @brief The lightgrid texel resolution per dimension.
	 */
	vec3 resolution;
};

/**
 * @brief The light struct.
 */
struct light_t {
	/**
	 * @brief The light origin and radius.
	 */
	vec4 origin;

	/**
	 * @brief The light color and intensity.
	 */
	vec4 color;
};

#define MAX_LIGHTS 0x20

/**
 * @brief The uniforms block.
 */
layout (std140) uniform uniforms {
	/**
	 * @brief The 3D projection matrix.
	 */
	mat4 projection3D;

	/**
	 * @brief The 2D projection matrix.
	 */
	mat4 projection2D;

	/**
	 * @brief The 2D projection matrix for the framebuffer object.
	 */
	mat4 projection2D_FBO;

	/**
	 * @brief The view matrix.
	 */
	mat4 view;

	/**
	 * @brief The lightgrid.
	 */
	lightgrid_t lightgrid;

	/**
	 * @brief The light sources for the current frame, transformed to view space.
	 */
	light_t lights[MAX_LIGHTS];

	/**
	 * @brief The renderer time, in milliseconds.
	 */
	int ticks;

	/**
	 * @brief The brightness scalar.
	 */
	float brightness;

	/**
	 * @brief The contrast scalar.
	 */
	float contrast;

	/**
	 * @brief The saturation scalar.
	 */
	float saturation;

	/**
	 * @brief The gamma scalar.
	 */
	float gamma;

	/**
	 * @brief The modulate scalar.
	 */
	float modulate;

	/**
	 * @brief The global fog color.
	 */
	// vec3 fog_global_color; // FIXME

	/**
	 * @brief The global fog density scalar.
	 */
	// float fog_global_density; // FIXME

	/**
	 * @brief The fog density scalar.
	 */
	float fog_density;

	/**
	 * @brief The number of fog samples per fragment (quality).
	 */
	int fog_samples;

	/**
	 * @brief The pixel dimensions of the framebuffer.
	 */
	vec2 resolution;
};
