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

#include "quemap.h"

/**
 * @brief Brushes defined in the map file are carved via CSG before being sorted into the tree.
 */
typedef struct csg_brush_s {
	struct csg_brush_s *next;
	vec3_t mins, maxs;
	int32_t side, test_side; // side of node during construction
	struct brush_s *original;
	int32_t num_sides;
	struct brush_side_s *sides;
} csg_brush_t;

csg_brush_t *MakeBrushes(int32_t start, int32_t end, const vec3_t mins, const vec3_t maxs);
csg_brush_t *SubtractBrushes(csg_brush_t *head);
