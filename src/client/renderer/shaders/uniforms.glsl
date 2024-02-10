
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

#define BSP_LIGHTGRID_LUXEL_SIZE 32.0

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

	/**
	 * @brief The lightrgrid luxel size, in texture space.
	 */
	vec4 luxel_size;
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
	 * @brief The depth range, in world units.
	 */
	vec2 depth_range;

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
	 * @brief The shadow debugging mask.
	 */
	int shadows;

	/**
	 * @brief The tonemapping algorithm.
	 */
	int tonemap;

	/**
	 * @brief The gamma scalar.
	 */
	float gamma;

	/**
	 * @brief The modulate scalar.
	 */
	float modulate;

	/**
	 * @brief The caustics scalar.
	 */
	float caustics;

	/**
	 * @brief The stains scalar.
	 */
	float stains;

	/**
	 * @brief The bloom scalar, for non-material based objects.
	 */
	float bloom;

	/**
	 * @brief The HDR scalar.
	 */
	float hdr;

	/**
	 * @brief The volumetric fog density scalar.
	 */
	float fog_density;

	/**
	 * @brief The number of volumetric fog samples per fragment (quality).
	 */
	int fog_samples;

	/**
	 * @brief The developer toggle, used for shader development tweaking.
	 */
	int developer;
};

#define LIGHT_INVALID     0
#define LIGHT_AMBIENT     1
#define LIGHT_SUN         2
#define LIGHT_POINT       4
#define LIGHT_SPOT        8
#define LIGHT_BRUSH_SIDE 16
#define LIGHT_PATCH      32
#define LIGHT_DYNAMIC    64

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
	 * @brief The normal and plane distance in view space.
	 */
	vec4 normal;

	/**
	 * @brief The light color and intensity.
	 */
	vec4 color;
};

#define MAX_LIGHT_UNIFORMS 256
#define MAX_LIGHT_UNIFORMS_ACTIVE 16

/**
 * @brief The lights uniform block.
 */
layout (std140) uniform lights_block {
	/**
	 * @brief The projection matrix for directional lights.
	 */
	mat4 light_projection;

	/**
	 * @brief The view matrix for directional lights, centered at the origin.
	 */
	mat4 light_view;

	/**
	 * @brief The projection matrix for point lights.
	 */
	mat4 light_projection_cube;

	/**
	 * @brief The view matrices for point lights, centered at the origin.
	 */
	mat4 light_view_cube[6];

	/**
	 * @brief The light sources for the current frame, transformed to view space.
	 */
	light_t lights[MAX_LIGHT_UNIFORMS];

	/**
	 * @brief The number of active light sources.
	 */
	int num_lights;
};

/**
 * @brief The diffusemap textures, for non-material passes such as sprites.
 */
uniform sampler2D texture_diffusemap;
uniform sampler2D texture_next_diffusemap;

/**
 * @brief The material texture.
 */
uniform sampler2DArray texture_material;

/**
 * @brief The material secondary texture.
 */
uniform sampler2D texture_stage;

/**
 * @brief The warp texture, for liquids.
 */
uniform sampler2D texture_warp;

/**
 * @brief The lightmap textures.
 */
uniform sampler2D texture_lightmap_ambient;
uniform sampler2D texture_lightmap_diffuse;
uniform sampler2D texture_lightmap_direction;
uniform sampler2D texture_lightmap_caustics;
uniform sampler2D texture_lightmap_stains;

/**
 * @brief The lightgrid textures.
 */
uniform sampler3D texture_lightgrid_ambient;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_direction;
uniform sampler3D texture_lightgrid_caustics;
uniform sampler3D texture_lightgrid_fog;

/**
 * @brief The sky cubemap texture.
 */
uniform samplerCube texture_sky;

/**
 * @brief The shadowmap textures.
 */
uniform sampler2DArrayShadow texture_shadowmap;
uniform samplerCubeArrayShadow texture_shadowmap_cube;

/**
 * @brief The framebuffer attachment textures.
 */
uniform sampler2D texture_color_attachment;
uniform sampler2D texture_bloom_attachment;
uniform sampler2D texture_depth_attachment_copy;
