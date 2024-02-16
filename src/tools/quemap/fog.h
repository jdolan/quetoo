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

#include "bsp.h"

#define FOG_COLOR Vec3(1.f, 1.f, 1.f)

/**
 * @brief This is the fog alpha value. At 0.03125 (1/32), it will take 32 samples, or 1024 world
 * units, to completely obscure an object within the fog.
 */
#define FOG_DENSITY 0.03125f

/**
 * @brief Diffuse light is multiplied and scaled by the fog color to produce the final fog sample.
 */
#define FOG_ABSORPTION 1.f

/**
 * @brief Fog types.
 */
typedef enum {
	FOG_INVALID = -1,

	/**
	 * @brief Global fog defined in worldspawn.
	 */
	FOG_GLOBAL,

	/**
	 * @brief Fog volumes come from brush entities that are merged into worldspawn.
	 */
	FOG_VOLUME
} fog_type_t;

/**
 * @details Fog is baked into the lightgrid as an additional RGBA 3D texture.
 */
typedef struct {

	/**
	 * @brief The fog type.
	 */
	fog_type_t type;

	/**
	 * @brief The entity definition.
	 */
	const cm_entity_t *entity;

	/**
	 * @brief The brushes originally belonging to this fog entity.
	 */
	GPtrArray *brushes;

	/**
	 * @brief The fog color.
	 */
	vec3_t color;

	/**
	 * @brief The fog density.
	 */
	float density;

	/**
	 * @brief The fog absorption.
	 */
	float absorption;

	/**
	 * @brief The bounds of all brushes in this fog entity.
	 */
	box3_t bounds;
} fog_t;

extern GArray *fogs;

_Bool PointInsideFog(const vec3_t point, const fog_t *fog);
void FreeFog(void);
void BuildFog(void);
