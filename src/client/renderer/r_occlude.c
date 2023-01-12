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
 * @brief Hardware occlusion queries.
 */
static struct {
	/**
	 * @brief The backing vertex array.
	 */
	vec3_t vertexes[MAX_OCCLUSION_QUERIES][8];

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
_Bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

	if (!r_occlude->integer) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	for (int32_t i = 0; i < view->num_occlusion_queries; i++) {
		const r_occlusion_query_t *q = view->occlusion_queries[i];
		if (Box3_Contains(q->bounds, bounds) && q->result == 0) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
_Bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius) {
	return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
}

/**
 * @return True if the specified box is occluded *or* culled by the view frustum,
 * false otherwise.
 */
_Bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {
	return R_OccludeBox(view, bounds) || R_CullBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum,
 * false otherwise.
 */
_Bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {
	return R_OccludeSphere(view, point, radius) || R_CullSphere(view, point, radius);
}

/**
 * @brief
 */
r_occlusion_query_t R_CreateOcclusionQuery(const box3_t bounds) {

	r_occlusion_query_t query = {
		.bounds = bounds,
		.available = 1,
		.result = 1,
	};

	glGenQueries(1, &query.name);
	R_GetError(NULL);

	return query;
}

/**
 * @brief
 */
void R_DestroyOcclusionQuery(r_occlusion_query_t *query) {

	if (query->name) {
		glDeleteQueries(1, &query->name);
		R_GetError(NULL);
	}
}

/**
 * @brief
 */
void R_AddOcclusionQuery(r_view_t *view, r_occlusion_query_t *q) {

	if (!r_occlude->integer) {
		return;
	}

	if (R_CullBox(view, q->bounds)) {
		return;
	}

	if (Box3_ContainsPoint(Box3_Expand(q->bounds, 16.f), view->origin)) {
		return;
	}

	if (view->num_occlusion_queries == MAX_OCCLUSION_QUERIES) {
		Com_Warn("MAX_OCCLUSION_QUERIES\n");
		return;
	}

	Box3_ToPoints(q->bounds, r_occlusion.vertexes[view->num_occlusion_queries]);

	view->occlusion_queries[view->num_occlusion_queries++] = q;
}

/**
 * @brief Updates and re-draws all active occlusion queries for the current frame.
 */
void R_UpdateOcclusionQueries(r_view_t *view) {

	if (!r_occlude->integer) {
		return;
	}

	if (view->flags & VIEW_FLAG_NO_DELTA) {
		view->num_occlusion_queries = 0;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	glDepthMask(GL_FALSE);

	glUseProgram(r_depth_pass_program.name);

	glBindVertexArray(r_occlusion.vertex_array);
	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion.elements_buffer);

	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(r_occlusion.vertexes[0]) * view->num_occlusion_queries, r_occlusion.vertexes);

	for (int32_t i = 0; i < view->num_occlusion_queries; i++) {
		r_occlusion_query_t *q = view->occlusion_queries[i];

		if (!q->available) {
			glGetQueryObjectiv(q->name, GL_QUERY_RESULT_AVAILABLE, &q->available);
			if (q->available || r_occlude->integer == 2) {
				glGetQueryObjectiv(q->name, GL_QUERY_RESULT, &q->result);

				if (q->result == 0) {
					
					for (int32_t j = 0; j < view->num_occlusion_queries; j++) {
						r_occlusion_query_t *p = view->occlusion_queries[j];

						if (Box3_Contains(q->bounds, p->bounds)) {
							p->result = 0;
						}
					}
				}
			}
		}

		if (q->available) {
			glBeginQuery(GL_ANY_SAMPLES_PASSED, q->name);
			glDrawElementsBaseVertex(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL, i * 8);
			glEndQuery(GL_ANY_SAMPLES_PASSED);
			q->available = 0;
		}

		if (q->result) {
			r_stats.occlusion_queries_visible++;
			if (r_draw_occlusion_queries->value) {
				R_Draw3DBox(q->bounds, color_red, false);
			}
		} else {
			r_stats.occlusion_queries_occluded++;
			if (r_draw_occlusion_queries->value) {
				R_Draw3DBox(q->bounds, color_green, false);
			}
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

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
	glBufferData(GL_ARRAY_BUFFER, sizeof(r_occlusion.vertexes), NULL, GL_DYNAMIC_DRAW);

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

//	r_occlusion_query_t *q = r_occlusion.queries;
//	for (int32_t i = 0; i < MAX_OCCLUSION_QUERIES; i++, q++) {
//		glDeleteQueries(1, &q->name);
//	}

	glDeleteVertexArrays(1, &r_occlusion.vertex_array);

	glDeleteBuffers(1, &r_occlusion.vertex_buffer);
	glDeleteBuffers(1, &r_occlusion.elements_buffer);

	R_GetError(NULL);
}
