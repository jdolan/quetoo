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

#include "bsp.h"
#include "polylib.h"

/**
 * @brief Faces are divided into patches to emit light, creating the illusion of area lights.
 */
typedef struct patch_s {
	/**
	 * @brief The model this patch belongs to.
	 */
	const bsp_model_t *model;

	/**
	 * @brief The face this patch belongs to.
	 */
	const bsp_face_t *face;

	/**
	 * @brief The brush side, for convenience.
	 */
	const bsp_brush_side_t *brush_side;

	/**
	 * @brief The patch offset, if it is part of an inline model.
	 */
	vec3_t origin;

	/**
	 * @brief The winding.
	 */
	cm_winding_t *winding;

	/**
	 * @brief The next patch in the same face.
	 */
	struct patch_s *next;
} patch_t;

extern patch_t *patches;

void BuildPatches(void);
void SubdividePatch(int32_t patch_num);
