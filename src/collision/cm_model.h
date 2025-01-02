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

cm_bsp_model_t *Cm_LoadBspModel(const char *name, int64_t *size);
cm_bsp_model_t *Cm_Model(const char *name); // *1, *2, etc

int32_t Cm_NumModels(void);

const char *Cm_EntityString(void);
cm_entity_t *Cm_Worldspawn(void);

int32_t Cm_LeafContents(const int32_t leaf_num);

const cm_bsp_t *Cm_Bsp(void);

#ifdef __CM_LOCAL_H__

extern cm_bsp_t cm_bsp;

#endif /* __CM_LOCAL_H__ */
