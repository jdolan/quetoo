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

#ifdef __R_LOCAL_H__
r_occlusion_query_t *R_CreateOcclusionQuery(const box3_t bounds);
void R_DestroyOcclusionQuery(r_occlusion_query_t *query);

r_occlusion_query_t *R_OcclusionQueryForBounds(const box3_t bounds);

#define R_CONDITIONAL_RENDER(bounds, commands) { \
	const r_occlusion_query_t *query = R_OcclusionQueryForBounds(bounds); \
	if (query) { \
		glBeginConditionalRender(query->name, GL_QUERY_WAIT); \
		commands; \
		glEndConditionalRender(); \
	} else { \
		commands; \
	} \
}

_Bool R_OccludeBox(const r_view_t *view, const box3_t bounds);
_Bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius);

void R_DrawOcclusionQueries(const r_view_t *view);
void R_UpdateOcclusionQueries(r_view_t *view);;
void R_InitOcclusionQueries(void);
void R_ShutdownOcclusionQueries(void);
#endif /* __R_LOCAL_H__ */
