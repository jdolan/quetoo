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

#include "csg.h"
#include "polylib.h"

/**
 * @brief The map file representation of a plane.
 */
typedef struct plane_s {
	vec3_t normal;
	dvec_t dist;
	int32_t type;
	struct plane_s *hash_chain;
} plane_t;

/**
 * @brief The map file representation of texture attributes.
 */
typedef struct {
	vec_t shift[2];
	vec_t rotate;
	vec_t scale[2];
	char name[32];
	int32_t flags;
	int32_t value;
} brush_texture_t;

#define	TEXINFO_NODE -1

typedef struct brush_side_s {
	int32_t plane_num;
	int16_t texinfo;
	cm_winding_t *winding;
	struct brush_side_s *original;
	int32_t contents;
	int32_t surf;
	_Bool visible; // choose visible planes first
	_Bool tested; // this side already checked as a split
	_Bool bevel; // don't use for bsp splitting
} brush_side_t;

typedef struct brush_s {
	int32_t entity_num;
	int32_t brush_num;

	int32_t contents;

	vec3_t mins, maxs;

	int32_t num_sides;
	brush_side_t *original_sides;
} brush_t;

csg_brush_t *AllocBrush(int32_t num_sides);
void FreeBrush(csg_brush_t *brush);
void FreeBrushes(csg_brush_t *brushes);
size_t CountBrushes(csg_brush_t *brushes);
csg_brush_t *CopyBrush(const csg_brush_t *brush);
vec_t BrushVolume(csg_brush_t *brush);
csg_brush_t *BrushFromBounds(const vec3_t mins, const vec3_t maxs);
int32_t TestBrushToPlane(csg_brush_t *brush, int32_t plane_num, int32_t *num_splits, _Bool *hint_split, int32_t *epsilon_brush);
void SplitBrush(csg_brush_t *brush, int32_t plane_num, csg_brush_t **front, csg_brush_t **back);
