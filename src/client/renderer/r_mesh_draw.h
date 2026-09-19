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

#include "r_types.h"

#if defined(__R_LOCAL_H__)
void R_DrawMeshEntities(const RenderView *view, RenderPass *pass);
void R_InitMeshPipeline(void);
void R_ShutdownMeshPipeline(void);
void R_UpdateMeshPipeline(void);

/**
 * @brief Resolves the material to draw for the given face of a mesh entity.
 * @return The material to draw, or `NULL` if the entity has an explicit
 * per-face skins array (`has_skins` is `true`) and this face has no skin
 * assigned, meaning it should not be drawn at all.
 */
static inline const RenderMaterial *R_MeshEntityFaceMaterial(const RenderEntity *e,
                                                            const RenderMeshFace *face,
                                                            int32_t i) {
  if (e->hasSkins) {
    return e->skins[i];
  }
  return face->material;
}
#endif
