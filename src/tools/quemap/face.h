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
 * @brief
 */
typedef struct face_s {
	struct face_s *next; // on node

	// the chain of faces off of a node can be merged,
	// but each face_t along the way will remain in the chain
	// until the entire tree is freed
	struct face_s *merged; // if set, this face isn't valid anymore

	struct portal_s *portal;
	int32_t texinfo;
	int32_t plane_num;
	int32_t contents; // faces in different contents can't merge
	int32_t face_num; // for leaf faces

	cm_winding_t *w;

} face_t;

extern int32_t c_merge;

face_t *AllocFace(void);
void FreeFace(face_t *f);
face_t *MergeFaces(face_t *f1, face_t *f2, const vec3_t normal);
int32_t EmitFace(const face_t *face);
void PhongVertexes(int32_t vertex_num);
