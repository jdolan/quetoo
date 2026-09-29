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

#include <Objectively/List.h>
#include <Objectively/Vector.h>

#include "cm_types.h"

/**
 * @brief Frees the entity and all subsequent pairs in its linked list.
 */
void Cm_FreeEntity(Entity *entity);

/**
 * @brief Allocates and returns a new zeroed entity key-value pair.
 */
Entity *Cm_AllocEntity(void);

/**
 * @brief Returns a deep copy of the entity linked list.
 */
Entity *Cm_CopyEntity(const Entity *entity);

/**
 * @brief Returns a new entity list with keys from src assigned into a copy of dst.
 * @details Keys already present in dst take priority; keys only in src are appended.
 *   Analogous to JavaScript's `Object.assign(dst, src)`.
 * @return A newly allocated entity list; the caller must free with `Cm_FreeEntity`.
 */
Entity *Cm_EntityAssign(const Entity *dst, const Entity *src);

/**
 * @brief Parses the string field of an entity pair into its typed fields.
 */
void Cm_ParseEntity(Entity *pair);

/**
 * @brief Sorts the entity key-value pairs, placing classname first.
 */
Entity *Cm_SortEntity(Entity *entity);

/**
 * @brief Parses an entity string into a List of entity linked lists.
 * @return A List of `Entity`* head pointers (one per entity).
 */
List *Cm_LoadEntities(const char *entityString);

/**
 * @brief Returns the index of the entity in the BSP entities array, or -1 if not found.
 */
int32_t Cm_EntityNumber(const Entity *entity);

/**
 * @brief Returns the entity pair matching key, or a null entity if not found.
 */
const Entity *Cm_EntityValue(const Entity *entity, const char *key);

/**
 * @brief Sets or adds the key-value pair on the entity.
 * @return The updated or newly created entity pair.
 */
Entity *Cm_EntitySetKeyValue(Entity *entity, const char *key, EntityParsed field, const void *value);

/**
 * @brief Returns a Vector of brushes belonging to the given entity.
 */
Vector *Cm_EntityBrushes(const Entity *entity);

/**
 * @brief Serializes the entity linked list to a Quake info string.
 */
char *Cm_EntityToInfoString(const Entity *entity);

/**
 * @brief Parses a Quake info string into an entity linked list.
 */
Entity *Cm_EntityFromInfoString(const char *str);

/**
 * @brief Parses brushes from .map text and attaches them to the corresponding entities.
 */
void Cm_ParseMapBrushes(const char *mapText, Entity **entities, int32_t numEntities);

#if defined(__CM_LOCAL_H__)
#endif
