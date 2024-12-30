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

#define MAX_OCCLUSION_QUERIES 1024

/**
 * @brief Hardware occlusion queries.
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

	const r_occlusion_query_t *query = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, query++) {
		if (Box3_Intersects(query->bounds, bounds) && query->result > 0) {
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
 * @return True if the specified box is occluded *or* culled by the view frustum,
 * false otherwise.
 */
bool R_CulludeBox(const r_view_t *view, const box3_t bounds) {
	return R_CullBox(view, bounds) || R_OccludeBox(view, bounds);
}

/**
 * @return True if the specified sphere is occluded *or* culled by the view frustum,
 * false otherwise.
 */
bool R_CulludeSphere(const r_view_t *view, const vec3_t point, const float radius) {
	return R_CullSphere(view, point, radius) || R_OccludeSphere(view, point, radius);
}

/**
 * @brief
 */
r_occlusion_query_t R_CreateOcclusionQuery(const box3_t bounds) {

	GLuint name;
	glGenQueries(1, &name);

	assert(name < MAX_OCCLUSION_QUERIES);

	r_occlusion_query_t query = {
		.name = name,
		.bounds = bounds,
		.available = 1,
		.result = 1,
	};

	vec3_t vertexes[8];
	Box3_ToPoints(bounds, vertexes);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBufferSubData(GL_ARRAY_BUFFER, (name - 1) * sizeof(vertexes), sizeof(vertexes), vertexes);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	R_GetError(NULL);
	return query;
}

/**
 * @brief
 */
void R_DestroyOcclusionQuery(r_occlusion_query_t *query) {

	if (query->name) {
		glDeleteQueries(1, &query->name);
	}

	memset(query, 0, sizeof(*query));

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_UpdateOcclusionQuery(const r_view_t *view, r_occlusion_query_t *query) {

	if (Box3_ContainsPoint(Box3_Expand(query->bounds, 16.f), view->origin)) {
		query->available = 2;
		query->result = 1;
	} else if (R_CullBox(view, query->bounds)) {
		query->available = 3;
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
			glDrawElementsBaseVertex(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL, (query->name - 1) * 8);
			glEndQuery(GL_ANY_SAMPLES_PASSED);
			query->available = 0;
		}
	}
}

/**
 * @brief Updates and re-draws all active occlusion queries for the current frame.
 */
void R_UpdateOcclusionQueries(r_view_t *view) {

	if (!r_occlude->integer) {
		return;
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

	r_occlusion_query_t *query = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, query++) {

		R_UpdateOcclusionQuery(view, query);

		if (query->result) {
			r_stats.occlusion_queries_visible++;
			if (r_draw_occlusion_queries->value) {
				if (query->available == 2) {
					R_Draw3DBox(query->bounds, color_yellow, false);
				} else {
					R_Draw3DBox(query->bounds, color_red, false);
				}
			}
		} else {
			r_stats.occlusion_queries_occluded++;
			if (r_draw_occlusion_queries->value) {
				R_Draw3DBox(query->bounds, color_green, false);
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
	glBufferData(GL_ARRAY_BUFFER, MAX_OCCLUSION_QUERIES * sizeof(vec3_t) * 8, NULL, GL_STATIC_DRAW);

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
