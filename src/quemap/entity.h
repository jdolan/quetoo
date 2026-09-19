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
 * @brief Entity pairs.
 */
typedef struct EntityKeyValue {
  struct EntityKeyValue *next;
  char key[MAX_BSP_ENTITY_KEY];
  char value[MAX_BSP_ENTITY_VALUE];
} EntityKeyValue;

/**
 * @brief The map file representation of an entity.
 */
typedef struct {

  EntityKeyValue *values;

  int32_t first_brush;
  int32_t num_brushes;

  int32_t first_brush_side;
  int32_t num_brush_sides;

  int32_t first_patch;
  int32_t num_patches;

  Box3 bounds;

} Entity;

void SetValueForKey(Entity *ent, const char *key, const char *value);
const char *ValueForKey(const Entity *ent, const char *key, const char *def);
Vec3 VectorForKey(const Entity *ent, const char *key, const Vec3 def);
