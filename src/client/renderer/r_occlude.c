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

#include "r_local.h"

/**
 * @brief
 */
bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

	if (!r_occlude->integer) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	const r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;

	const r_bsp_block_t *block = in->blocks;
	for (int32_t i = 0; i < in->num_blocks; i++, block++) {

		if (block->occluded) {
			if (Box3_Contains(block->node->bounds, bounds)) {
				return true;
			}
			continue;
		}

		if (Box3_Intersects(block->node->bounds, bounds)) {
			return false;
		}
	}

	return true;
}

/**
 * @brief
 */
bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius) {
	return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
}

/**
 * @return True if the specified box is occluded *or* culled by the view frustum, false otherwise.
 */
bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {
	return R_CullBox(view, bounds) || R_OccludeBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum, false otherwise.
 */
bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {
	return R_CullSphere(view, point, radius) || R_OccludeSphere(view, point, radius);
}

/**
 * @brief Polls the query for a result and executes it again if available.
 * @return True if the query is occluded (no samples passed the query).
 */
static bool R_UpdateOcclusionQuery(const r_view_t *view, r_occlusion_query_t *query) {

	if (Box3_ContainsPoint(Box3_Expand(query->bounds, 16.f), view->origin)) {
		query->available = 1;
		query->result = 1;
	} else if (R_CullBox(view, query->bounds)) {
		query->available = 1;
		query->result = 0;
	} else {
		if (query->available == 0) {
			glGetQueryObjectiv(query->name, GL_QUERY_RESULT_AVAILABLE, &query->available);
			if (query->available || r_occlude->integer == 2) {
				glGetQueryObjectiv(query->name, GL_QUERY_RESULT, &query->result);
				query->available = 1;
			}
		}

		if (query->available) {
			glBeginQuery(GL_ANY_SAMPLES_PASSED, query->name);
			glDrawElementsBaseVertex(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL, query->base_vertex);
			glEndQuery(GL_ANY_SAMPLES_PASSED);
			query->available = 0;
		}
	}

	return query->result == 0;
}

/**
 * @brief Updates and re-draws all active occlusion queries for the current frame.
 */
void R_UpdateOcclusionQueries(const r_view_t *view) {

	if (!r_occlude->integer) {
		return;
	}

	r_bsp_model_t *bsp = r_world_model->bsp;

	glUseProgram(r_depth_pass_program.name);

	glBindVertexArray(bsp->occlusion.vertex_array);
	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->occlusion.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bsp->occlusion.elements_buffer);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	r_bsp_block_t *block = bsp->inline_models->blocks;
	for (int32_t i = 0; i < bsp->inline_models->num_blocks; i++, block++) {

		block->occluded = R_UpdateOcclusionQuery(view, &block->query);

		if (block->occluded) {
			r_stats.occlusion_queries_occluded++;
		} else {
			r_stats.occlusion_queries_visible++;
		}

		if (r_draw_bsp_blocks->value) {
			const float dist = Vec3_Distance(Box3_Center(block->node->bounds), view->origin);
			const float f = 1.f - Clampf(dist / MAX_WORLD_COORD, 0.f, 1.f);
			if (block->occluded) {
				R_Draw3DBox(block->query.bounds, Color3f(0.f, f, 0.f), false);
			} else {
				R_Draw3DBox(block->query.bounds, Color3f(f, 0.f, 0.f), false);
			}
		}
	}

	r_bsp_light_t *light = bsp->lights;
	for (int32_t i = 0; i < bsp->num_lights; i++, light++) {

		if (light->query.name) {
			light->occluded = R_UpdateOcclusionQuery(view, &light->query);
			if (light->occluded) {
				r_stats.occlusion_queries_occluded++;
			} else {
				r_stats.occlusion_queries_visible++;
			}
		}
	}

	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	R_GetError(NULL);
}
