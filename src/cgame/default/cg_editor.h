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
   * @brief The owned entity definition, parsed from configstrings.
   */
  cm_entity_t *def;

  /**
   * @brief The client entity.
   */
  const cl_entity_t *ent;

  /**
   * @brief The model, or `NULL`.
   */
  const r_model_t *model;

  /**
   * @brief The brushes pointer array for BSP model entities, or `NULL`.
   */
  GPtrArray *brushes;

  /**
   * @brief Persistent shadow cache flag for shadowmap optimization (light entities only).
   */
  bool shadow_cached;

  /**
   * @brief The client-side entity state for `misc_*` (class, origin, think, etc.).
   */
  cg_entity_t misc;

} cg_editor_entity_t;

/**
 * @brief Encapsulates all mutable editor state.
 */
typedef struct {

  /**
   * @brief Editor entity array, indexed by entity number.
   */
  cg_editor_entity_t entities[MAX_ENTITIES];

  /**
   * @brief When false, `func_group` entities are excluded from editor traces and scene drawing.
   * @details Toggled via the 'G' key in the EntityViewController.
   */
  bool show_func_groups;

  /**
   * @brief The entity number of the currently selected entity, or 0 if none.
   */
  int16_t selected;

} cg_editor_t;

extern cg_editor_t cg_editor;

/**
 * @brief The result of a combined editor trace against all BSP models and `CONTENTS_EDITOR` entities.
 */
typedef struct {

  /**
   * @brief Pointer into `cg_editor.entities[]` for the resolved entity. Always valid; defaults to
   *   worldspawn (`&cg_editor.entities[0]`) when no more-specific entity was hit.
   */
  cg_editor_entity_t *ent;

  /**
   * @brief The raw BSP trace result. Check `.fraction < 1.f` for a hit; `.material`, `.brush`,
   *   and `.plane` are set on a BSP brush hit.
   */
  cm_trace_t trace;

} cg_editor_trace_t;

int32_t Cg_FindTeamMaster(const char *classname, const char *team);
void Cg_ParseEditorEntity(int16_t number, const char *info);
void Cg_LoadEditorEntities(void);
void Cg_FreeEditorEntities(void);
void Cg_PopulateEditorScene(const cl_frame_t *frame);
cg_editor_trace_t Cg_EntitySelectionTrace(const vec3_t start, const vec3_t end);
cg_editor_trace_t Cg_MaterialSelectionTrace(const vec3_t start, const vec3_t end);
void Cg_CheckEditor(void);
