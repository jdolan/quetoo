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

#include "cm_types.h"

void Cm_FreeEntity(cm_entity_t *entity);
cm_entity_t *Cm_AllocEntity(void);
void Cm_ParseEntity(cm_entity_t *pair);
cm_entity_t *Cm_SortEntity(cm_entity_t *entity);
GList *Cm_LoadEntities(const char *entity_string);
int32_t Cm_EntityNumber(const cm_entity_t *entity);
const cm_entity_t *Cm_EntityValue(const cm_entity_t *entity, const char *key);
cm_entity_t *Cm_EntitySetKeyValue(cm_entity_t *entity, const char *key, cm_entity_parsed_t field, const void *value);
GPtrArray *Cm_EntityBrushes(const cm_entity_t *entity);
char *Cm_EntityToInfoString(const cm_entity_t *entity);
cm_entity_t *Cm_EntityFromInfoString(const char *str);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
