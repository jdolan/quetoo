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

void R_AddLight(const r_light_t *l);

#ifdef __R_LOCAL_H__

/**
 * @brief The lights uniform block type.
 */
typedef struct {
	/**
	 * @brief The uniform buffer name.
	 */
	GLuint buffer;

	/**
	 * @brief The lights block struct.
	 * @remarks This struct is vec4 aligned.
	 */
	struct {
		/**
		 * @brief The light sources for the current frame, transformed to view space.
		 */
		r_light_t lights[MAX_LIGHTS];

		/**
		 * @brief The number of active light sources.
		 */
		int32_t num_lights;
	} block;

} r_lights_t;

/**
 * @brief The lights uniform block, updated once per frame.
 */
extern r_lights_t r_lights;

void R_UpdateLights(void);
void R_InitLights(void);
void R_ShutdownLights(void);
#endif /* __R_LOCAL_H__ */
