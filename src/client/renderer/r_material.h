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

#include "common/asset.h"
#include "r_types.h"

RenderMaterial *R_LoadMaterial(const char *name, AssetContext context);

#if defined(__R_LOCAL_H__)

RenderMaterial *R_FindMaterial(const char *name, AssetContext context);
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
} RenderMaterialSampler;

/**
 * @brief Shared material storage buffer binding slots.
 */
typedef enum {
  R_STORAGE_BSP_LIGHTS,
  R_STORAGE_DYNAMIC_LIGHTS,
  R_STORAGE_VOXEL_LIGHT_DATA,
  R_STORAGE_VOXEL_LIGHT_INDICES,
  R_STORAGE_MATERIAL_TOTAL,
} RenderMaterialStorage;

/**
 * @brief Per-draw material and stage uniforms.
 */
typedef struct {
  alignas(16) Vec4 color;
  Vec2 stOrigin;
  Vec2 stretch;
  Vec2 scroll;
  Vec2 scale;
  Vec2 terrain;
  Vec2 warp;
  int32_t surface;
  float alphaTest;
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
} RenderMaterialUniforms;

void R_MaterialUniforms(const RenderMaterial *material, int32_t surface, RenderMaterialUniforms *out);

bool R_StageUniforms(const RenderView *view, const RenderEntity *entity,
                     const RenderBspDrawElements *draw, const RenderStage *stage,
                     RenderMaterialUniforms *out, SDL_GPUTexture **texture, SDL_GPUTexture **textureNext);

/**
 * @brief Mesh material uniforms, including tint colors.
 */
typedef struct {
  RenderMaterialUniforms material;
  Vec4 tintColors[TINT_TOTAL];
} RenderMeshMaterialUniforms;

#endif
