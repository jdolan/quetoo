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
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "r_local.h"

/*
 * TODO(#864): GPU occlusion queries are retired; Phase 7 replaces them with
 * compute Hi-Z occlusion. For now the query subsystem is stubbed and only the
 * CPU frustum cull (R_CulludeBox / R_CulludeSphere) is live.
 */

/**
 * @return True if the specified box is occluded. Always false during bring-up.
 */
bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {
  return false;
}

/**
 * @brief Tests whether the given sphere origin is occluded.
 */
bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius) {
  return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
}

/**
 * @return True if the specified box is occluded *or* culled by the view frustum.
 */
bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {
  return R_CullBox(view, bounds) || R_OccludeBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum.
 */
bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {
  return R_CullSphere(view, point, radius) || R_OccludeSphere(view, point, radius);
}

/**
 * @brief Allocates an occlusion query. Always NULL during bring-up; callers
 * store the NULL as block/light->query and null-check on use.
 */
r_occlusion_query_t *R_AllocOcclusionQuery(const box3_t bounds) {
  return NULL;
}

/**
 * @brief Frees the specified occlusion query.
 */
void R_FreeOcclusionQuery(r_occlusion_query_t *query) {
}

/**
 * @brief
 */
void R_UpdateOcclusionQueries(const r_view_t *view) {
}

/**
 * @brief
 */
void R_DrawOcclusionQueries(const r_view_t *view) {
}

/**
 * @brief
 */
void R_InitOcclusionQueries(void) {
}

/**
 * @brief
 */
void R_ShutdownOcclusionQueries(void) {
}
