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

#include "polylib.h"

/**
 * @brief Map brushes are carved via CSG before being sorted into the tree.
 */
typedef struct CsgBrush {
  const struct Brush *original;
  struct BrushSide *brushSides;
  int32_t numBrushSides;
  Box3 bounds;
  struct CsgBrush *next;
} CsgBrush;

CsgBrush *AllocBrush(int32_t numSides);
void FreeBrush(CsgBrush *brush);
void FreeBrushes(CsgBrush *brushes);
size_t CountBrushes(const CsgBrush *brushes);
CsgBrush *CopyBrush(const CsgBrush *brush);
float BrushVolume(CsgBrush *brush);
CsgBrush *BrushFromBounds(const Box3 bounds);
int32_t BrushOnPlaneSide(const CsgBrush *brush, int32_t plane);
int32_t BrushOnPlaneSideSplits(const CsgBrush *brush, int32_t plane, int32_t *numSplits);
void SplitBrush(const CsgBrush *brush, int32_t plane, CsgBrush **front, CsgBrush **back);
