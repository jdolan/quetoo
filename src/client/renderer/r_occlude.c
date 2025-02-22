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
 * @brief OpenGL occlusion queries.
 */
static struct {
	/**
	 * @brief The vertex array object.
	 */
	GLuint vertex_array;

	/**
	 * @brief The vertex buffer object.
	 */
	GLuint vertex_buffer;

	/**
	 * @brief The elements buffer object.
	 */
	GLuint elements_buffer;
} r_occlusion;

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
 * @brief Creates an occlusion query for each block node in the world model.
 */
void R_CreateOcclusionQueries(const r_bsp_model_t *bsp) {
	vec3_t vertexes[8];

	const r_bsp_inline_model_t *in = bsp->inline_models;

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, in->num_blocks * sizeof(vertexes), NULL, GL_STATIC_DRAW);

	r_bsp_block_t *block = in->blocks;
	for (int32_t i = 0; i < in->num_blocks; i++, block++) {

		glGenQueries(1, &block->query.name);
		block->query.base_vertex = i * lengthof(vertexes);

		block->query.available = 1;
		block->query.result = 1;

		Box3_ToPoints(block->visible_bounds, vertexes);
		glBufferSubData(GL_ARRAY_BUFFER, i * sizeof(vertexes), sizeof(vertexes), vertexes);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	R_GetError(NULL);
}

/**
 * @brief Polls the block's query for a result and executes it again if available.
 * @return True if the block is occluded (no samples passed the query).
 */
static bool R_UpdateOcclusionQuery(const r_view_t *view, r_bsp_block_t *block) {

	r_occlusion_query_t *query = &block->query;

	if (Box3_ContainsPoint(block->node->bounds, view->origin)) {
		query->available = 1;
		query->result = 1;
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

	glUseProgram(r_depth_pass_program.name);

	glBindVertexArray(r_occlusion.vertex_array);
	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion.elements_buffer);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	r_bsp_inline_model_t *in = r_world_model->bsp->inline_models;
	r_bsp_block_t *block = in->blocks;
	for (int32_t i = 0; i < in->num_blocks; i++, block++) {

		if (R_CullBox(view, block->node->bounds)) {
			block->occluded = true;
		} else {
			block->occluded = R_UpdateOcclusionQuery(view, block);
		}

		if (block->occluded) {
			r_stats.occlusion_queries_occluded++;
		} else {
			r_stats.occlusion_queries_visible++;
		}

		if (r_draw_bsp_blocks->value) {
			const float dist = Vec3_Distance(Box3_Center(block->node->bounds), view->origin);
			const float f = 1.f - Clampf(dist / MAX_WORLD_COORD, 0.f, 1.f);
			if (block->occluded) {
				R_Draw3DBox(block->node->bounds, Color3f(0.f, f, 0.f), false);
			} else {
				R_Draw3DBox(block->node->bounds, Color3f(f, 0.f, 0.f), false);
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

/**
 * @brief
 */
void R_DestroyOcclusionQueries(const r_bsp_model_t *bsp) {

	r_bsp_inline_model_t *in = bsp->inline_models;
	r_bsp_block_t *block = in->blocks;

	for (int32_t i = 0; i < in->num_blocks; i++, block++) {
		glDeleteQueries(1, &block->query.name);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitOcclusionQueries(void) {

	memset(&r_occlusion, 0, sizeof(r_occlusion));

	glGenVertexArrays(1, &r_occlusion.vertex_array);
	glBindVertexArray(r_occlusion.vertex_array);

	glGenBuffers(1, &r_occlusion.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) 0);

	glGenBuffers(1, &r_occlusion.elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion.elements_buffer);

	const GLuint elements[] = {
		// bottom
		0, 1, 3, 0, 3, 2,
		// top
		6, 7, 4, 7, 5, 4,
		// front
		4, 5, 0, 5, 1, 0,
		// back
		7, 6, 3, 6, 2, 3,
		// left
		6, 4, 2, 4, 0, 2,
		// right
		5, 7, 1, 7, 3, 1,
	};

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownOcclusionQueries(void) {

	glDeleteVertexArrays(1, &r_occlusion.vertex_array);

	glDeleteBuffers(1, &r_occlusion.vertex_buffer);
	glDeleteBuffers(1, &r_occlusion.elements_buffer);

	R_GetError(NULL);
}
