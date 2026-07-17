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
 * @brief Shared material sampler binding slots.
 */
typedef enum {
  R_SAMPLER_MATERIAL,
  R_SAMPLER_SHADOW_ATLAS_0,
  R_SAMPLER_SHADOW_ATLAS_1,
  R_SAMPLER_SHADOW_ATLAS_2,
  R_SAMPLER_SHADOW_ATLAS_3,
  R_SAMPLER_SHADOW_ATLAS_4,
  R_SAMPLER_SHADOW_ATLAS_5,
  R_SAMPLER_VOXEL_CAUSTICS,
  R_SAMPLER_VOXEL_OCCLUSION,
  R_SAMPLER_SKY,
  R_SAMPLER_STAGE,
  R_SAMPLER_STAGE_NEXT,
  R_SAMPLER_MATERIAL_TOTAL,
} r_material_sampler_t;

/**
 * @brief Shared material storage buffer binding slots.
 */
typedef enum {
  R_STORAGE_BSP_LIGHTS,
  R_STORAGE_DYNAMIC_LIGHTS,
  R_STORAGE_VOXEL_LIGHT_DATA,
  R_STORAGE_VOXEL_LIGHT_INDICES,
  R_STORAGE_MATERIAL_TOTAL,
} r_material_storage_t;

/**
 * @brief Per-draw material and stage uniforms.
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
 * @brief Mesh material uniforms, including tint colors.
 */
typedef struct {
  r_material_uniforms_t material;
  vec4_t tint_colors[TINT_TOTAL];
} r_mesh_material_uniforms_t;

#endif
