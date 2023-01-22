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

#define LIGHT_COLOR Vec3(1.f, 1.f, 1.f)
#define LIGHT_RADIUS DEFAULT_LIGHT_RADIUS
#define LIGHT_RADIUS_AMBIENT 256.f
#define LIGHT_INTENSITY DEFAULT_LIGHT_INTENSITY
#define LIGHT_SHADOW DEFAULT_LIGHT_SHADOW
#define LIGHT_ANGLE_UP -1.f
#define LIGHT_ANGLE_DOWN -2.f
#define LIGHT_CONE 22.5f
#define LIGHT_FALLOFF DEFAULT_LIGHT_FALLOFF
#define LIGHT_SUN_DIST 1024.f
#define LIGHT_SIZE_SUN 32.f
#define LIGHT_SIZE_STEP 16.f

/**
 * @brief BSP light sources may come from entities or emissive surfaces.
 */
typedef struct light_s {
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
	 * @brief The light intensity.
	 */
	float intensity;

	/**
	 * @brief The light cone for angular attenuation in degrees.
	 */
	float cone;

	/**
	 * @brief The angular attenuation interval in degrees.
	 */
	float falloff;

	/**
	 * @brief The cosine of the light cone.
	 */
	float theta;

	/**
	 * @brief The cosine of the light falloff.
	 */
	float phi;

	/**
	 * @brief The light shadow scalar.
	 */
	float shadow;

	/**
	 * @brief The size of the light, in world units, to simulate area lights.
	 */
	float size;

	/**
	 * @brief The bounds of the light source.
	 */
	box3_t bounds;

	/**
	 * @brief The sample points (origins) to be traced to for this light.
	 * @remarks For directional lights, these are directional vectors, not points.
	 */
	vec3_t *points;

	/**
	 * @brief The number of sample points.
	 */
	int32_t num_points;

	/**
	 * @brief The light source winding for patch and indirect lights.
	 */
	cm_winding_t *winding;

	/**
	 * @brief The light source face for patch and indirect lights.
	 */
	const bsp_face_t *face;

	/**
	 * @brief The light source brush side for patch and indirect lights.
	 */
	const bsp_brush_side_t *brush_side;

	/**
	 * @brief The light source plane for patch and indirect lights.
	 */
	const bsp_plane_t *plane;

	/**
	 * @brief The light source model for patch and indirect lights.
	 */
	const bsp_model_t *model;

	/**
	 * @brief The output light source.
	 */
	const bsp_light_t *out;
} light_t;

extern GPtrArray *node_lights[MAX_BSP_NODES];
extern GPtrArray *leaf_lights[MAX_BSP_LEAFS];

vec3_t ColorNormalize(const vec3_t in);
vec3_t ColorFilter(const vec3_t in);

void FreeLights(void);
void BuildDirectLights(void);
void BuildIndirectLights(void);
void EmitLights(void);
