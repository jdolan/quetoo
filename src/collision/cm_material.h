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

cm_material_t *Cm_FindMaterial(const char *diffuse);

_Bool Cm_UnrefMaterial(cm_material_t *mat);
void Cm_RefMaterial(cm_material_t *mat);

cm_material_t *Cm_LoadMaterial(const char *diffuse);
GArray *Cm_LoadMaterials(const char *path);
void Cm_UnloadMaterials(GArray *materials);

void Cm_WriteMaterials(void);

typedef void (*Cm_EnumerateMaterialsFunc) (cm_material_t *material);
void Cm_EnumerateMaterials(Cm_EnumerateMaterialsFunc enumerator);

#ifdef __CM_LOCAL_H__
void Cm_ShutdownMaterials(void);
#endif /* __CM_LOCAL_H__ */
