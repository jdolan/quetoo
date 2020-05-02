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

// settings and preferences
extern cvar_t *r_allow_high_dpi;
extern cvar_t *r_anisotropy;
extern cvar_t *r_brightness;
extern cvar_t *r_bumpmap;
extern cvar_t *r_caustics;
extern cvar_t *r_contrast;
extern cvar_t *r_display;
extern cvar_t *r_draw_buffer;
extern cvar_t *r_flares;
extern cvar_t *r_fog;
extern cvar_t *r_fullscreen;
extern cvar_t *r_gamma;
extern cvar_t *r_get_error;
extern cvar_t *r_hardness;
extern cvar_t *r_height;
extern cvar_t *r_materials;
extern cvar_t *r_modulate;
extern cvar_t *r_multisample;
extern cvar_t *r_parallax;
extern cvar_t *r_saturation;
extern cvar_t *r_screenshot_format;
extern cvar_t *r_shadows;
extern cvar_t *r_shell;
extern cvar_t *r_specular;
extern cvar_t *r_stains;
extern cvar_t *r_supersample;
extern cvar_t *r_texture_mode;
extern cvar_t *r_swap_interval;
extern cvar_t *r_warp;
extern cvar_t *r_width;

extern r_view_t r_view;

void R_GetError_(const char *function, const char *msg);
#define R_GetError(msg) R_GetError_(__func__, msg)

_Bool R_CullBox(const vec3_t mins, const vec3_t maxs);
_Bool R_CullSphere(const vec3_t point, const float radius);
void R_Init(void);
void R_Shutdown(void);
void R_LoadMedia(void);
void R_BeginFrame(void);
void R_DrawView(r_view_t *view);
void R_EndFrame(void);

#ifdef __R_LOCAL_H__

// private hardware configuration information
typedef struct {
	const char *renderer;
	const char *vendor;
	const char *version;

	int32_t max_texunits;
	int32_t max_texture_size;
} r_config_t;

extern r_config_t r_config;

// private renderer structure
typedef struct {

	/**
	 * @brief The 3D projection matrix.
	 */
	mat4_t projection3D;

	/**
	 * @brief The 2D projection matrix.
	 */
	mat4_t projection2D;

	/**
	 * @brief The view matrix.
	 */
	mat4_t view;

	/**
	 * @brief The view frustum, for box and sphere culling.
	 */
	cm_bsp_plane_t frustum[4];

	/**
	 * @brief The leaf in which the view origin resides.
	 */
	const r_bsp_leaf_t *leaf; // the leaf at the view origin

	/**
	 * @brief The PVS and PHS data at the view origin.
	 */
	byte vis_data_pvs[MAX_BSP_LEAFS >> 3];
	byte vis_data_phs[MAX_BSP_LEAFS >> 3];

	/**
	 * @brief The PVS frame counter. World elements must reference the current
	 * frame in order to be drawn.
	 */
	int32_t vis_frame;

	/**
	 * @brief The stain frame counter.
	 */
	int32_t stain_frame;

	/**
	 * @brief The fog parameters, scaled by the renderer.
	 */
	vec3_t fog_parameters;

} r_locals_t;

extern r_locals_t r_locals;

// development tools
extern cvar_t *r_blend;
extern cvar_t *r_clear;
extern cvar_t *r_cull;
extern cvar_t *r_lock_vis;
extern cvar_t *r_no_vis;
extern cvar_t *r_draw_bsp_lightgrid;
extern cvar_t *r_draw_bsp_lightmaps;
extern cvar_t *r_draw_bsp_normals;
extern cvar_t *r_draw_entity_bounds;
extern cvar_t *r_draw_wireframe;

#endif /* __R_LOCAL_H__ */
