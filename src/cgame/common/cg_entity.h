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

#include "cg_types.h"

#if defined(__CG_LOCAL_H__)

typedef struct CGameEntity CGameEntity;

typedef void (*CGameEntityInit)(CGameEntity *self);
typedef void (*CGameEntityFree)(CGameEntity *self);
typedef void (*CGameEntityThink)(CGameEntity *self);

/**
 * @brief The client game entity class type.
 */
typedef struct {

  /**
   * @brief The entity class name.
   */
  const char *classname;

  /**
   * @brief The initialization function, called once per level.
   */
  CGameEntityInit Init;

  /**
   * @brief The free function, called before re-initializing after an in-editor modification.
   * @details Implementations should release any resources (sprites, sounds, etc.) that
   *   reference the entity's @c data, as @c data is freed and reallocated immediately after.
   */
  CGameEntityFree Free;

  /**
   * @brief The think function, called once per client frame.
   */
  CGameEntityThink Think;

  /**
   * @brief The size of the opaque data.
   */
  size_t dataSize;

} CGameEntityClass;

/**
 * @brief The client game entity instance type. Client game entities are local to the client,
 * and are used for non-critical and atmospheric effects such as sparks, steam, particle
 * fields, etc.
 */
struct CGameEntity {

  /**
   * @brief The entity identifier, for persistent effects such as sounds.
   */
  int32_t id;
  
  /**
   * @brief The entity class.
   */
  const CGameEntityClass *clazz;

  /**
   * @brief The backing entity definition.
   */
  const CmEntity *def;

  /**
   * @brief The entity origin.
   */
  Vec3 origin;

  /**
   * @brief The entity bounds.
   */
  Box3 bounds;

  /**
   * @brief The entity's target, if any.
   */
  const CmEntity *target;

  /**
   * @brief The entity's teammate, if any.
   */
  const CmEntity *team;

  /**
   * @brief Timestamp for next emission.
   * @details Client game entities will Think() each frame, unless deferred.
   */
  uint32_t nextThink;

  /**
   * @brief Randomization of `nextThink`.
   */
  float hz, drift;

  /**
   * @brief Opaque, type-specific data.
   */
  void *data;
};

extern const CGameEntityClass *cgEntityClasses[];
extern const size_t cgNumEntityClasses;

extern Vector *cgEntities;

CGameEntity *Cg_EntityForDefinition(const CmEntity *e);
void Cg_LoadEntities(void);
void Cg_FreeEntities(void);

ClientEntity *Cg_Self(void);
bool Cg_IsDucking(const ClientEntity *ent);
Box3 Cg_PlayerBounds(bool ducked);
void Cg_Interpolate(const ClientFrame *frame);
void Cg_AddEntities(const ClientFrame *frame);

#endif
