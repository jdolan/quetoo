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

#include "cg_types.h"

#ifdef __CG_LOCAL_H__

/**
 * @brief Light sources that optionally persistent over multiple frames.
 */
typedef struct {

	/**
	 * @brief The light origin.
	 */
	vec3_t origin;

	/**
	 * @brief The light radius.
	 */
	float radius;

	/**
	 * @brief The light color.
	 */
	vec3_t color;

	/**
	 * @brief The light intensity.
	 */
	float intensity;

	/**
	 * @brief The decay period in milliseconds.
	 */
	uint32_t decay;

	/**
	 * @brief The time when this light was added.
	 */
	uint32_t time;

} cg_light_t;

void Cg_AddLight(const cg_light_t *s);
void Cg_AddLights(void);
void Cg_InitLights(void);

#endif /* __CG_LOCAL_H__ */
