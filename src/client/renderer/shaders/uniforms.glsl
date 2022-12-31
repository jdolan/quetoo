
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

#define VIEW_UNKNOWN		0
#define VIEW_MAIN			1
#define VIEW_PLAYER_MODEL	2

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
	 * @brief The lightgrid size, in luxels.
	 */
	vec4 size;
};

/**
 * @brief The uniforms block.
 */
layout (std140) uniform uniforms_block {
	/**
	 * @brief The viewport (x, y, w, h) in device pixels.
	 */
	ivec4 viewport;

	/**
	 * @brief The 2D projection matrix.
	 */
	mat4 projection2D;

	/**
	 * @brief The 3D projection matrix.
	 */
	mat4 projection3D;

	/**
	 * @brief The view matrix.
	 */
	mat4 view;

	/**
	 * @brief The lightgrid.
	 */
	lightgrid_t lightgrid;

	/**
	 * @brief The global fog color (RGB, density).
	 */
	vec4 fog_color;

	/**
	 * @brief The depth range, in world units.
	 */
	vec2 depth_range;

	/**
	 * @brief The global fog depth range, in world units.
	 */
	vec2 fog_depth_range;

	/**
	 * @brief The view type, e.g. VIEW_MAIN.
	 */
	int view_type;

	/**
	 * @brief The renderer time, in milliseconds.
	 */
	int ticks;

	/**
	 * @brief The lightmaps debugging mask.
	 */
	int lightmaps;

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
	 * @brief The volumetric fog density scalar.
	 */
	float fog_density;

	/**
	 * @brief The number of volumetric fog samples per fragment (quality).
	 */
	int fog_samples;

	/**
	 * @brief The caustics scalar.
	 */
	float caustics;

	/**
	 * @brief The bloom scalar, for non-material based objects.
	 */
	float bloom;

	/**
	 * @brief The bloom level of detail.
	 */
	int bloom_lod;

	/**
	 * @brief The developer toggle, used for shader development tweaking.
	 */
	int developer;
};

#define LIGHT_INVALID   0
#define LIGHT_AMBIENT   1
#define LIGHT_SUN       2
#define LIGHT_POINT     4
#define LIGHT_SPOT      8
#define LIGHT_PATCH    16
#define LIGHT_INDIRECT 32
#define LIGHT_DYNAMIC  64

/**
 * @brief The light struct.
 */
struct light_t {
	/**
	 * @brief The light position in model space, and radius.
	 */
	vec4 model;

	/**
	 * @brief The light mins in model space, and size.
	 */
	vec4 mins;

	/**
	 * @brief The light maxs in model space, and attenuation.
	 */
	vec4 maxs;

	/**
	 * @brief The light position in view space, and type.
	 */
	vec4 position;

	/**
	 * @brief The light color and intensity.
	 */
	vec4 color;
};

#define MAX_LIGHT_UNIFORMS 256
#define MAX_LIGHT_UNIFORMS_ACTIVE 8

/**
 * @brief The lights uniform block.
 */
layout (std140) uniform lights_block {
	/**
	 * @brief The projection matrix for directional lights.
	 */
	mat4 shadow_projection;

	/**
	 * @brief The view matrix for directional lights, centered at the origin.
	 */
	mat4 shadow_view;

	/**
	 * @brief The projection matrix for point lights.
	 */
	mat4 shadow_projection_cube;

	/**
	 * @brief The view matrices for point lights, centered at the origin.
	 */
	mat4 shadow_view_cube[6];

	/**
	 * @brief The light sources for the current frame, transformed to view space.
	 */
	light_t lights[MAX_LIGHT_UNIFORMS];

	/**
	 * @brief The number of active light sources.
	 */
	int num_lights;
};
