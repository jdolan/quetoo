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
	 * @brief The light position in model space, and radius.
	 */
	vec4_t model;

	/**
	 * @brief The light position in view space, and type.
	 */
	vec4_t position;

	/**
	 * @brief The light normal and distance for directional lights.
	 */
	vec4_t normal;

	/**
	 * @brief The light color and intensity.
	 */
	vec4_t color;

	/**
	 * @brief The light mins in view space.
	 */
	vec4_t mins;

	/**
	 * @brief The light maxs in view space.
	 */
	vec4_t maxs;

	/**
	 * @brief The light projection matrix.
	 */
	mat4_t projection;

	/**
	 * @brief The light view matrices.
	 */
	mat4_t view[6];
} r_light_uniform_t;

#define MAX_LIGHT_UNIFORMS 96

/**
 * @brief The lights uniform block struct.
 * @remarks This struct is vec4 aligned.
 */
typedef struct {
	/**
	 * @brief The visible light sources for the current frame, transformed to view space.
	 */
	r_light_uniform_t lights[MAX_LIGHT_UNIFORMS];

	/**
	 * @brief The number of active light sources.
	 */
	int32_t num_lights;
} r_lights_block_t;

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
	r_lights_block_t block;
} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(r_view_t *view);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif /* __R_LOCAL_H__ */
