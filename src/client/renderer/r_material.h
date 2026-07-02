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

/**
 * @brief The per-stage parameters, pushed to the stage uniform slot by the
 * MATERIAL_STAGES bsp variant. Field order mirrors material.glsl's `stage_block`
 * (std140: vec4, then vec2s, then scalars, padded to a 16-byte multiple).
 */
typedef struct {
  vec4_t color;
  vec2_t st_origin;
  vec2_t stretch;
  vec2_t scroll;
  vec2_t scale;
  vec2_t terrain;
  vec2_t warp;
  int32_t flags;
  float pulse;
  float drift;
  float rotate;
  float dirtmap;
  float lighting;
  float emissive;
  float lerp;
  float shell;
  float _pad[3];
} r_stage_uniforms_t;

bool R_StageUniforms(const r_view_t *view, const r_entity_t *entity,
                     const r_bsp_draw_elements_t *draw, const r_stage_t *stage,
                     r_stage_uniforms_t *out, SDL_GPUTexture **texture, SDL_GPUTexture **texture_next);
#endif
