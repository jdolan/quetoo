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
  CmEntity *def;

  /**
   * @brief The client entity.
   */
  const ClientEntity *ent;

  /**
   * @brief The model, or `NULL`.
   */
  const RenderModel *model;

  /**
   * @brief The brushes pointer array for BSP model entities, or `NULL`.
   */
  Vector *brushes;

  /**
   * @brief The client-side entity state for `misc_*` (class, origin, think, etc.).
   */
  CGameEntity misc;

} CGameEditorEntity;

/**
 * @brief Encapsulates all mutable editor state.
 */
typedef struct {

  /**
   * @brief Editor entity array, indexed by entity number.
   */
  CGameEditorEntity entities[MAX_ENTITIES];

  /**
   * @brief When false, `func_group` entities are excluded from editor traces and scene drawing.
   * @details Toggled via the 'G' key in the EntityViewController.
   */
  bool showFuncGroups;

  /**
   * @brief The entity number of the currently selected entity, or 0 if none.
   */
  int16_t selected;

  /**
   * @brief The materials of the map, indexed by BSP material, as the renderer loaded and the
   * editor edits them.
   */
  CmMaterial **materials;

  /**
   * @brief The `CmMaterialLight` previews of the brush sides with a `STAGE_LIGHT` material.
   * @details BSP lights are not drawn in editor mode, so these are placed as quemap places them
   * and added as dynamic lights, so that edits to the light stages show without a recompile.
   */
  Vector *materialLights;

  /**
   * @brief The resolved default light colors, indexed by BSP material, and zero until resolved.
   */
  Vec3 *materialLightColors;

} CGameEditor;

extern CGameEditor cgEditor;

/**
 * @brief The result of a combined editor trace against all BSP models and `CONTENTS_EDITOR` entities.
 */
typedef struct {

  /**
   * @brief Pointer into `cgEditor.entities[]` for the resolved entity. Always valid; defaults to
   *   worldspawn (`&cgEditor.entities[0]`) when no more-specific entity was hit.
   */
  CGameEditorEntity *ent;

  /**
   * @brief The raw BSP trace result. Check `.fraction < 1.f` for a hit; `.material`, `.brush`,
   *   and `.plane` are set on a BSP brush hit.
   */
  CmTrace trace;

} CGameEditorTrace;

/**
 * @brief The maximum number of entities collected along the entity selection ray.
 */
#define CG_EDITOR_MAX_CANDIDATES 32

int32_t Cg_FindTeamMaster(const char *classname, const char *team);
void Cg_ParseEditorEntity(int16_t number, const char *info);
void Cg_LoadEditorEntities(void);
void Cg_FreeEditorEntities(void);
void Cg_PopulateEditorScene(const ClientFrame *frame);
void Cg_UpdateEditorMaterialLights(const CmMaterial *material);
size_t Cg_EntitySelectionCandidates(const Vec3 start, const Vec3 end, int16_t out[CG_EDITOR_MAX_CANDIDATES]);
CGameEditorTrace Cg_MaterialSelectionTrace(const Vec3 start, const Vec3 end);
void Cg_CycleEditorSelection(int32_t dir);
void Cg_CheckEditor(void);
