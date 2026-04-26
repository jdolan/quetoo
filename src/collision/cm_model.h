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
 * @brief Loads the BSP and all sub-models for collision detection.
 * @param name The Quake path to the .bsp file, or NULL to unload.
 * @param size If non-NULL, receives the file size for compatibility checking.
 * @return The world inline model (model 0), or NULL on failure.
 */
cm_bsp_model_t *Cm_LoadBspModel(const char *name, int64_t *size);

/**
 * @brief Returns the inline BSP model with the given name (e.g. "*1").
 */
cm_bsp_model_t *Cm_Model(const char *name); // *1, *2, etc

/**
 * @brief Returns the number of inline BSP models in the loaded BSP.
 */
int32_t Cm_NumModels(void);

/**
 * @brief Returns the raw entity string from the loaded BSP.
 */
const char *Cm_EntityString(void);

/**
 * @brief Returns the worldspawn entity (first entity in the loaded BSP).
 */
const cm_entity_t *Cm_Worldspawn(void);

/**
 * @brief Returns the contents mask for the given BSP leaf number.
 */
int32_t Cm_LeafContents(const int32_t leaf_num);

/**
 * @brief Returns a const pointer to the global BSP collision model.
 */
const cm_bsp_t *Cm_Bsp(void);

#if defined(__CM_LOCAL_H__)

extern cm_bsp_t cm_bsp;

#endif /* __CM_LOCAL_H__ */
