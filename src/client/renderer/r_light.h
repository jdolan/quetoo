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

void R_AddLight(r_view_t *view, const r_light_t *l);

#ifdef __R_LOCAL_H__

/**
 * @brief The light uniform type.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
	/**
	 * @brief The light origin in model space, and radius.
	 */
	vec4_t model;

	/**
	 * @brief The light mins in model space, and size.
	 */
	vec4_t mins;

	/**
	 * @brief The light maxs in model space, and attenuation.
	 */
	vec4_t maxs;

	/**
	 * @brief The light origin in view space, and type.
	 */
	vec4_t position;

	/**
	 * @brief The light color and intensity.
	 */
	vec4_t color;
} r_light_uniform_t;

#define MAX_LIGHT_UNIFORMS 128

/**
 * @brief The lights uniform block struct.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
	/**
	 * @brief The projection matrix for directional lights.
	 */
	mat4_t shadow_projection;

	/**
	 * @brief The view matrix for directional lights, centered at the origin.
	 */
	mat4_t shadow_view;

	/**
	 * @brief The projection matrix for point lights.
	 */
	mat4_t shadow_projection_cube;

	/**
	 * @brief The view matrices for point lights, centered at the origin.
	 */
	mat4_t shadow_view_cube[6];

	/**
	 * @brief The visible light sources for the current frame.
	 */
	r_light_uniform_t lights[MAX_LIGHT_UNIFORMS];

	/**
	 * @brief The number of visible light sources.
	 */
	int32_t num_lights;
} r_light_uniform_block_t;

/**
 * @brief The lights uniform block type.
 */
typedef struct {
	/**
	 * @brief The uniform buffer name.
	 */
	GLuint buffer;

	/**
	 * @brief The uniform buffer interface block.
	 */
	r_light_uniform_block_t block;
} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(r_view_t *view);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif /* __R_LOCAL_H__ */
