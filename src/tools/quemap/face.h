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

#include "polylib.h"

/**
 * @brief The in-tree representation of BSP faces.
 */
typedef struct face_s {
	/**
	 * @brief Faces are chained on the node on which they reside.
	 * @details Faces on either side of the node may be chained together.
	 */
	struct face_s *next;

	/**
	 * @brief The portal that created this face.
	 */
	struct portal_s *portal;

	/**
	 * @brief The original brush side that gives this face its texture.
	 */
	const struct brush_side_s *brush_side;

	/**
	 * @brief The ordered, welded face winding, used to emit BSP vertexes.
	 */
	cm_winding_t *w;

	/**
	 * @brief The output face in the BSP, so that node faces may emit leaf faces.
	 */
	bsp_face_t *out;
} face_t;

extern int32_t num_welds;

face_t *AllocFace(void);
void FreeFace(face_t *f);
void ClearWeldingSpatialHash(void);
bsp_face_t *EmitFace(const face_t *face);
void PhongShading(void);
