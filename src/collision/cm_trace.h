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

cm_trace_t Cm_BoxTrace(const vec3_t start, const vec3_t end, const box3_t bounds, int32_t head_node,
					   int32_t contents, const mat4_t matrix, const mat4_t inverse_matrix);

box3_t Cm_EntityBounds(const solid_t solid, const mat4_t matrix,
	                   const box3_t bounds);

box3_t Cm_TraceBounds(const vec3_t start, const vec3_t end, const box3_t bounds);

#ifdef __CM_LOCAL_H__
#endif /* __CM_LOCAL_H__ */
