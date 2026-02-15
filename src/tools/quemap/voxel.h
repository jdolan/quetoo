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

/**
 * @brief The voxel type.
 */
typedef struct {
  vec3i_t xyz;
  vec3_t origin;
  box3_t bounds;
  float caustics;
  float exposure;
  vec4_t diffuse;
  vec4_t fog;
  GHashTable *lights;
  int32_t lights_offset;
  int32_t lights_count;
} voxel_t;

/**
 * @brief The voxel grid type.
 */
typedef struct {
  box3_t stu_bounds;
  vec3i_t size;
  size_t num_voxels;
  voxel_t *voxels;
  size_t num_light_indices;
} voxels_t;

extern voxels_t voxels;

size_t BuildVoxels(void);
void LightVoxel(int32_t voxel_num);
void FogVoxel(int32_t voxel_num);
void CausticsVoxel(int32_t voxel_num);
void ExposureVoxel(int32_t voxel_num);
void EmitVoxels(void);
void FreeVoxels(void);
