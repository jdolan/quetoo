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

#include <stdalign.h>

#include "r_types.h"

r_material_t *R_LoadMaterial(const char *name, cm_asset_context_t context);

#if defined(__R_LOCAL_H__)

r_material_t *R_FindMaterial(const char *name, cm_asset_context_t context);
void R_SaveMaterials_f(void);

/**
 * @brief The per-draw material AND per-stage parameters, pushed to the
 * material uniform slot by the lit-geometry programs (bsp, mesh): they're
 * both material-related, unlike e.g. the active-lights bitmask, which lives
 * in its own UBO. Layout mirrors material.glsl's combined `material_block`
 * (std140: vec4, then vec2s, then scalars; `alignas` pads the struct to a
 * 16-byte multiple). `R_MaterialUniforms` fills the material fields;
 * `R_StageUniforms` fills the stage fields -- callers populate both for a
 * stage draw, or just call `R_MaterialUniforms` and leave `flags` at
 * `STAGE_NONE` (zero-initialize) for the base/blend draw.
 */
typedef struct {
  alignas(16) vec4_t color;
  vec2_t st_origin;
  vec2_t stretch;
  vec2_t scroll;
  vec2_t scale;
  vec2_t terrain;
  vec2_t warp;
  int32_t surface;
  float alpha_test;
  float roughness;
  float hardness;
  float specularity;
  float parallax;
  float shadow;
  int32_t flags;
  float pulse;
  float drift;
  float rotate;
  float dirtmap;
  float lighting;
  float emissive;
  float lerp;
  float shell;
} r_material_uniforms_t;

void R_MaterialUniforms(const r_material_t *material, int32_t surface, r_material_uniforms_t *out);

bool R_StageUniforms(const r_view_t *view, const r_entity_t *entity,
                     const r_bsp_draw_elements_t *draw, const r_stage_t *stage,
                     r_material_uniforms_t *out, SDL_GPUTexture **texture, SDL_GPUTexture **texture_next);

/**
 * @brief The mesh program's per-draw material uniforms: `r_material_uniforms_t`
 * plus per-entity tint colors for player-skin colorization, appended so they
 * remain std140-compatible as a trailing member (mirrors material.glsl's
 * MATERIAL_TINTS extension of `material_block`). bsp has no equivalent, since
 * only mesh entities are tinted.
 */
typedef struct {
  r_material_uniforms_t material;
  vec4_t tint_colors[TINT_TOTAL];
} r_mesh_material_uniforms_t;

#endif
