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

/**
 * @brief Frees the entity and all subsequent pairs in its linked list.
 */
void Cm_FreeEntity(cm_entity_t *entity);

/**
 * @brief Allocates and returns a new zeroed entity key-value pair.
 */
cm_entity_t *Cm_AllocEntity(void);

/**
 * @brief Returns a deep copy of the entity linked list.
 */
cm_entity_t *Cm_CopyEntity(const cm_entity_t *entity);

/**
 * @brief Parses the string field of an entity pair into its typed fields.
 */
void Cm_ParseEntity(cm_entity_t *pair);

/**
 * @brief Sorts the entity key-value pairs, placing classname first.
 */
cm_entity_t *Cm_SortEntity(cm_entity_t *entity);

/**
 * @brief Parses the BSP entity string into a GList of entity linked lists.
 * @return A GList of cm_entity_t* head pointers (one per entity).
 */
GList *Cm_LoadEntities(const char *entity_string);

/**
 * @brief Returns the index of the entity in the BSP entities array, or -1 if not found.
 */
int32_t Cm_EntityNumber(const cm_entity_t *entity);

/**
 * @brief Returns the entity pair matching key, or a null entity if not found.
 */
const cm_entity_t *Cm_EntityValue(const cm_entity_t *entity, const char *key);

/**
 * @brief Sets or adds the key-value pair on the entity.
 * @return The updated or newly created entity pair.
 */
cm_entity_t *Cm_EntitySetKeyValue(cm_entity_t *entity, const char *key, cm_entity_parsed_t field, const void *value);

/**
 * @brief Returns a GPtrArray of brushes belonging to the given entity.
 */
GPtrArray *Cm_EntityBrushes(const cm_entity_t *entity);

/**
 * @brief Serializes the entity linked list to a Quake info string.
 */
char *Cm_EntityToInfoString(const cm_entity_t *entity);

/**
 * @brief Parses a Quake info string into an entity linked list.
 */
cm_entity_t *Cm_EntityFromInfoString(const char *str);

/**
 * @brief Parses brushes from .map text and attaches them to the corresponding entities.
 */
void Cm_ParseMapBrushes(const char *map_text, cm_entity_t **entities, int32_t num_entities);

#if defined(__CM_LOCAL_H__)
#endif /* __CM_LOCAL_H__ */
