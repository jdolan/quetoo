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
#define LIGHT_RADIUS 300.f
#define LIGHT_INTENSITY 1.f

/**
 * @brief BSP light sources may come from entities or emissive surfaces.
 */
typedef struct light_s {
  /**
   * @brief The entity number.
   */
  int32_t entity;

  /**
   * @brief The origin.
   */
  vec3_t origin;

  /**
   * @brief The color.
   */
  vec3_t color;

  /**
   * @brief The light radius in units.
   */
  float radius;

  /**
   * @brief The light intensity.
   */
  float intensity;

  /**
   * @brief The bounds of the light source.
   */
  box3_t bounds; // FIXME: Remove this?

  /**
   * @brief The output light in the BSP, so that voxels may reference them.
   */
  bsp_light_t *out;
} light_t;

extern GPtrArray *lights;

void FreeLights(void);
void BuildLights(void);
void EmitLights(void);
void EmitLightGrid(void);
