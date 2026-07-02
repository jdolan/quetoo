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

r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context);

#if defined(__R_LOCAL_H__)

/**
 * @brief The per-draw material parameters, pushed to fragment uniform slot 2 by
 * the lit-geometry programs. Layout mirrors material.glsl's `material_block`
 * (std140: seven scalars padded to a 16-byte multiple).
 */
typedef struct {
  int32_t surface;
  float alpha_test;
  float roughness;
  float hardness;
  float specularity;
  float parallax;
  float shadow;
  float _pad;
} r_material_uniforms_t;

r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context);
void R_SaveMaterials_f(void);
void R_MaterialUniforms(const r_material_t *material, int32_t surface, r_material_uniforms_t *out);
#endif
