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

extern bsp_file_t bsp_file;

typedef struct epair_s {
	struct epair_s *next;
	char *key;
	char *value;
} epair_t;

typedef struct {
	vec3_t origin;
	int32_t first_brush;
	int32_t num_brushes;
	epair_t *epairs;

	// only valid for func_areaportals
	int32_t area_portal_num;
	int32_t portal_areas[2];
} entity_t;

extern uint16_t num_entities;
extern entity_t entities[MAX_BSP_ENTITIES];

void ParseEntities(void);
void UnparseEntities(void);

void SetKeyValue(entity_t *ent, const char *key, const char *value);
const char *ValueForKey(const entity_t *ent, const char *key);
// will return "" if not present

vec_t FloatForKey(const entity_t *ent, const char *key);
void VectorForKey(const entity_t *ent, const char *key, vec3_t vec);

epair_t *ParseEpair(void);

int32_t LoadBSPFile(const char *filename, const bsp_lump_id_t lumps);
void WriteBSPFile(const char *filename, const int32_t version);
