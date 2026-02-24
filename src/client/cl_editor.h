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

#include "cl_types.h"

/**
 * @brief The editor entity type.
 */
typedef struct {
  /**
   * @brief The entity number.
   */
  int16_t number;

  /**
   * @brief The client entity.
   */
  cl_entity_t *ent;

  /**
   * @brief The entity definition.
   */
  cm_entity_t *def;

  /**
   * @brief The persistent storage for shadowmap optimization (light entities only).
   */
  bool shadow_cached;
} cl_editor_entity_t;

int32_t Cl_FindTeamMaster(const char *classname, const char *team);

#ifdef __CL_LOCAL_H__
void Cl_ParseEditorEntity(int16_t number, const char *info);
void Cl_PopulateEditorScene(const cl_frame_t *frame);
#endif /* __CL_LOCAL_H__ */
