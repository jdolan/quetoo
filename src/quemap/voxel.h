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

#include <Objectively/HashTable.h>

/**
 * @brief The voxel type.
 */
typedef struct {
  Vec3i xyz;
  Vec3 origin;
  Box3 bounds;
  Vec3 caustics;
  float exposure;
  float occlusion;
  HashTable *lights;
  int32_t lights_offset;
  int32_t lights_count;
} Voxel;

/**
 * @brief The voxel grid type.
 */
typedef struct {
  Box3 stu_bounds;
  Vec3i size;
  size_t num_voxels;
  Voxel *voxels;
  size_t num_light_indices;
} Voxels;

extern Voxels voxels;

size_t BuildVoxels(void);
void LightVoxel(int32_t voxel_num);
void FloodLights(void);
void AssignLightVoxels(void);
void AssignBlockVoxels(void);
void CausticsVoxel(int32_t voxel_num);
void ExposureVoxel(int32_t voxel_num);
void OccludeVoxel(int32_t voxel_num);
void SmoothVoxels(void);
void EmitVoxels(void);
void FreeVoxels(void);
