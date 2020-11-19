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

#define LIGHT_COLOR (vec3_t) { 1.f, 1.f, 1.f }
#define LIGHT_RADIUS DEFAULT_LIGHT
#define LIGHT_RADIUS_AMBIENT 255.f
#define LIGHT_ANGLE_UP -1.f
#define LIGHT_ANGLE_DOWN -2.f
#define LIGHT_CONE 22.5f
#define LIGHT_SIZE_SUN 256.f
#define LIGHT_SIZE_STEP 16.f

typedef enum {
	LIGHT_INVALID = -1,
	LIGHT_AMBIENT,
	LIGHT_SUN,
	LIGHT_POINT,
	LIGHT_SPOT,
	LIGHT_PATCH,
	LIGHT_INDIRECT
} light_type_t;

typedef enum {
	LIGHT_ATTEN_NONE,
	LIGHT_ATTEN_LINEAR,
	LIGHT_ATTEN_INVERSE_SQUARE,
} light_atten_t;


/**
 * @brief BSP light sources may come from entities or emissive surfaces.
 */
typedef struct {

	/**
	 * @brief The cluster containing this light, or -1 for directional lights.
	 */
	int32_t cluster;

	/**
	 * @brief The type.
	 */
	light_type_t type;

	/**
	 * @brief The attenuation.
	 */
	light_atten_t atten;

	/**
	 * @brief The origin.
	 */
	vec3_t origin;

	/**
	 * @brief The color.
	 */
	vec3_t color;

	/**
	 * @brief The normal vector for spotlights and sunlights.
	 */
	vec3_t normal;

	/**
	 * @brief The light radius in units.
	 */
	float radius;

	/**
	 * @brief The angle, in radians, from the normal where spotlight attenuation occurs.
	 */
	float theta;

	/**
	 * @brief The size of the light, in world units, to simulate area lights.
	 */
	float size;

} light_t;

extern GArray *lights;

extern GPtrArray *node_lights[MAX_BSP_NODES];
extern GPtrArray *leaf_lights[MAX_BSP_LEAFS];

float ColorNormalize(const vec3_t in, vec3_t *out);
vec3_t ColorFilter(const vec3_t in);

void BuildDirectLights(const GList *entities);
void BuildIndirectLights(void);
