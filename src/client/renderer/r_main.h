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

#pragma once

#include "r_types.h"

extern cvar_t *r_allow_high_dpi;
extern cvar_t *r_anisotropy;
extern cvar_t *r_brightness;
extern cvar_t *r_bloom;
extern cvar_t *r_bloom_lod;
extern cvar_t *r_caustics;
extern cvar_t *r_contrast;
extern cvar_t *r_display;
extern cvar_t *r_finish;
extern cvar_t *r_fog_density;
extern cvar_t *r_fog_samples;
extern cvar_t *r_fullscreen;
extern cvar_t *r_gamma;
extern cvar_t *r_hardness;
extern cvar_t *r_height;
extern cvar_t *r_modulate;
extern cvar_t *r_multisample;
extern cvar_t *r_roughness;
extern cvar_t *r_saturation;
extern cvar_t *r_screenshot_format;
extern cvar_t *r_shadowmap;
extern cvar_t *r_shadowmap_size;
extern cvar_t *r_specularity;
extern cvar_t *r_sprite_downsample;
extern cvar_t *r_stains;
extern cvar_t *r_texture_downsample;
extern cvar_t *r_texture_mode;
extern cvar_t *r_texture_storage;
extern cvar_t *r_swap_interval;
extern cvar_t *r_width;

extern r_stats_t r_stats;

void R_Init(void);
void R_Shutdown(void);
void R_BeginFrame(void);
void R_InitView(r_view_t *view);
void R_DrawViewDepth(r_view_t *view);
void R_DrawMainView(r_view_t *view);
void R_DrawPlayerModelView(r_view_t *view);
void R_EndFrame(void);

#ifdef __R_LOCAL_H__

/**
 * @brief OpenGL driver information.
 */
typedef struct {
	const char *renderer;
	const char *vendor;
	const char *version;

	int32_t max_texunits;
	int32_t max_texture_size;
} r_config_t;

extern r_config_t r_config;

/**
 * @brief The lightgrid uniform struct.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
	/**
	 * @brief The lightgrid mins, in world space.
	 */
	vec4_t mins;

	/**
	 * @brief The lightgrid maxs, in world space.
	 */
	vec4_t maxs;

	/**
	 * @brief The view origin, in lightgrid space.
	 */
	vec4_t view_coordinate;

	/**
	 * @brief The lightgrid size, in luxels.
	 */
	vec4_t size;
} r_lightgrid_t;

#define FOG_DENSITY    1.f
#define FOG_DEPTH_NEAR 128.f
#define FOG_DEPTH_FAR  2048.f

/**
 * @brief The uniforms block type.
 */
typedef struct {
	/**
	 * @brief The name of the uniform buffer.
	 */
	GLuint buffer;

	/**
	 * @brief The uniform block struct.
	 * @remarks This struct is vec4 aligned.
	 */
	struct r_uniform_block_t {
		/**
		 * @brief The viewport (x, y, w, h) in device pixels.
		 */
		vec4i_t viewport;

		/**
		 * @brief The 2D projection matrix.
		 */
		mat4_t projection2D;

		/**
		 * @brief The 3D projection matrix.
		 */
		mat4_t projection3D;

		/**
		 * @brief The view matrix.
		 */
		mat4_t view;

		/**
		 * @brief The lightgrid uniforms.
		 */
		r_lightgrid_t lightgrid;

		/**
		 * @brief The global fog color (RGB, density).
		 */
		vec4_t fog_color;

		/**
		 * @brief The depth range, in world units.
		 */
		vec2_t depth_range;

		/**
		 * @brief The global fog depth range, in world units.
		 */
		vec2_t fog_depth_range;

		/**
		 * @brief The view type, e.g. VIEW_MAIN.
		 */
		int32_t view_type;

		/**
		 * @brief The renderer time, in milliseconds.
		 */
		int32_t ticks;

		/**
		 * @brief The lightmaps debugging mask.
		 */
		int32_t lightmaps;

		/**
		 * @brief The shadows debugging mask.
		 */
		int32_t shadows;

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
		int32_t fog_samples;

		/**
		 * @brief The caustics scalar.
		 */
		float caustics;

		/**
		 * @brief The bloom scalar.
		 */
		float bloom;

		/**
		 * @brief The bloom level of detail.
		 */
		int32_t bloom_lod;

		/**
		 * @brief The developer flags.
		 */
		int32_t developer;
	} block;

} r_uniforms_t;

/**
 * @brief The uniform variables block, updated once per frame and common to all programs.
 */
extern r_uniforms_t r_uniforms;

// developer tools
extern cvar_t *r_alpha_test;
extern cvar_t *r_blend_depth_sorting;
extern cvar_t *r_cull;
extern cvar_t *r_depth_pass;
extern cvar_t *r_developer;
extern cvar_t *r_draw_bsp_lightgrid;
extern cvar_t *r_draw_bsp_lightmap;
extern cvar_t *r_draw_bsp_normals;
extern cvar_t *r_draw_entity_bounds;
extern cvar_t *r_draw_light_bounds;
extern cvar_t *r_draw_material_stages;
extern cvar_t *r_draw_occlusion_queries;
extern cvar_t *r_draw_wireframe;
extern cvar_t *r_get_error;
extern cvar_t *r_error_level;
extern cvar_t *r_max_errors;
extern cvar_t *r_occlude;

/**
 * @brief Keeps track of how many errors we've run into, so we can
 * break out of expensive error handlers if too many have happened.
 */
extern int32_t r_error_count;

void R_GetError_(const char *function, const char *msg);

#define R_GetError(msg) { \
	if (r_get_error->integer) { \
		R_GetError_(__func__, msg); \
	} \
}

_Bool R_CullBox(const r_view_t *view, const box3_t bounds);
_Bool R_CullSphere(const r_view_t *view, const vec3_t point, const float radius);

_Bool R_CulludeBox(const r_view_t *view, const box3_t bounds);
_Bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius);
#endif /* __R_LOCAL_H__ */
