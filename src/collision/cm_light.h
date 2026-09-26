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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once

#include <Objectively/Vector.h>

#include "cm_types.h"

/**
 * @brief The distance that a material light is placed in front of its brush side.
 */
#define MATERIAL_LIGHT_OFFSET 8.f

/**
 * @brief A light emitted by a brush side whose material has a `STAGE_LIGHT` stage.
 */
typedef struct {

  /**
   * @brief The light origin in world space, in front of the brush side. For a brush entity with
   * an origin brush, this includes the entity origin.
   */
  Vec3 origin;

  /**
   * @brief The outward normal of the brush side.
   */
  Vec3 normal;

  /**
   * @brief The brush side that emits the light.
   */
  int32_t brushSide;

  /**
   * @brief The BSP material index of the brush side.
   */
  int32_t material;

  /**
   * @brief The BSP model that owns the brush side. Zero is the world.
   */
  int32_t model;
} CmMaterialLight;

/**
 * @brief Places the lights for every drawn brush side whose material has a `STAGE_LIGHT` stage.
 * @param file The BSP file, which MUST have its brush, brush side and plane lumps loaded. It MUST
 * also be the loaded collision model, because solid points are rejected with `Cm_PointContents`,
 * and inline models are resolved from its entities.
 * @param materials The materials to read the stages from, indexed by BSP material. quemap passes the
 * collision materials. The editor passes the materials it edits.
 * @param material The BSP material index to place lights for, or `-1` for all materials.
 * @param lights The Vector of `CmMaterialLight` to append to.
 * @return The number of lights appended.
 * @remarks Each brush side gets a grid of lights across its winding, spaced by the stage light
 * radius, with at least one light per brush side. The order is stable (brush, brush side, grid
 * row, grid column), so that compiled BSPs are deterministic. Brush sides that face into
 * solid get no light, because each light is rejected in solid.
 */
size_t Cm_MaterialLights(const BspFile *file, CmMaterial *const *materials, int32_t material, Vector *lights);

/**
 * @brief Resolves the default color of a stage light that does not specify `light.color`.
 * @return The average color of the pixels of the stage texture (or of the material diffusemap,
 * if the stage has no texture) that are at least half as bright as the brightest pixel,
 * normalized to length 1.
 */
Vec3 Cm_MaterialLightColor(const CmMaterial *material, const CmStage *stage);
