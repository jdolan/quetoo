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

#include "cg_entity.h"
#include "cg_types.h"

/**
 * @brief An editor entity consolidating client entity, owned definition, vtable state,
 *   and shadow cache flag into a single slot indexed by entity number.
 */
typedef struct {

  /**
   * @brief The entity number.
   */
  int16_t number;

  /**
   * @brief The client entity.
   */
  const cl_entity_t *ent;

  /**
   * @brief The owned entity definition, parsed from configstrings.
   */
  cm_entity_t *def;

  /**
   * @brief Persistent shadow cache flag for shadowmap optimization (light entities only).
   */
  bool shadow_cached;

  /**
   * @brief The client-side entity vtable state (class, origin, think, etc.).
   */
  cg_entity_t entity;

} cg_editor_entity_t;

extern cg_editor_entity_t cg_edit[MAX_ENTITIES];

int32_t Cg_FindTeamMaster(const char *classname, const char *team);
void Cg_ParseEditorEntity(int16_t number, const char *info);
void Cg_LoadEditorEntities(void);
void Cg_FreeEditorEntities(void);
void Cg_PopulateEditorScene(const cl_frame_t *frame);
void Cg_CheckEditor(void);
