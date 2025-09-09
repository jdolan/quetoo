
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
	 * @brief The projection matrix for shadow projection.
	 */
	mat4 light_projection;

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

#define LIGHT_ATTEN_NONE           0
#define LIGHT_ATTEN_LINEAR         1
#define LIGHT_ATTEN_INVERSE_SQUARE 2

/**
 * @brief The light struct.
 */
struct light_t {
	/**
	 * @brief The light position in view space.
	 */
	vec4 position;
	
	/**
	 * @brief The light position in model space, and radius.
	 */
	vec4 model;

	/**
	 * @brief The light mins in model space.
	 */
	vec4 mins;

	/**
	 * @brief The light maxs in model space.
	 */
	vec4 maxs;

	/**
	 * @brief The light color and intensity.
	 */
	vec4 color;
};

#define MAX_LIGHTS 768
#define MAX_SHADOW_CUBEMAP_LAYERS 256
#define MAX_SHADOW_CUBEMAP_ARRAYS (MAX_LIGHTS / MAX_SHADOW_CUBEMAP_LAYERS)

/**
 * @brief The lights uniform block.
 */
layout (std140) uniform lights_block {
	/**
	 * @brief The light sources for the current frame, transformed to view space.
	 */
	light_t lights[MAX_LIGHTS];

	/**
	 * @brief The count of light sources.
	 */
	int num_lights;
};

/**
 * @brief The bit vector of active light indexes for the current render operation.
 */
uniform uint active_lights[MAX_LIGHTS / 32];

/**
 * @brief The diffusemap texture, for non-material passes such as sprites.
 */
uniform sampler2D texture_diffusemap;
uniform sampler2D texture_next_diffusemap;

/**
 * @brief The material primary texture.
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
 * @brief The lightgrid textures.
 */
uniform isampler3D texture_lightgrid_lights0;
uniform isampler3D texture_lightgrid_lights1;
uniform isampler3D texture_lightgrid_lights2;
uniform isampler3D texture_lightgrid_lights3;
uniform isampler3D texture_lightgrid_lights4;
uniform isampler3D texture_lightgrid_lights5;
uniform sampler3D texture_lightgrid_diffuse;
uniform sampler3D texture_lightgrid_fog;
uniform sampler3D texture_lightgrid_stains;

/**
 * @brief The sky cubemap texture.
 */
uniform samplerCube texture_sky;

/**
 * @brief The shadow array and cubemap array texture.
 */
uniform samplerCubeArrayShadow texture_shadow_cubemap_array0;
uniform samplerCubeArrayShadow texture_shadow_cubemap_array1;
uniform samplerCubeArrayShadow texture_shadow_cubemap_array2;
uniform samplerCubeArrayShadow texture_shadow_cubemap_array3;

/**
 * @brief The framebuffer attachment textures.
 */
uniform sampler2D texture_color_attachment;
uniform sampler2D texture_bloom_attachment;
uniform sampler2D texture_depth_attachment_copy;
