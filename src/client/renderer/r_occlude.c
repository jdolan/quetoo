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
	 * @brief
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
static _Bool R_OccludeBox_r(const r_view_t *view, const box3_t bounds, const r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return true;
	}

	const int32_t side = Cm_BoxOnPlaneSide(bounds, node->plane->cm);

	if (side & SIDE_FRONT) {
		if (R_OccludeBox_r(view, bounds, node->children[0]) == false) {
			return false;
		}
	}

	if (side & SIDE_BACK) {
		if (R_OccludeBox_r(view, bounds, node->children[1]) == false) {
			return false;
		}
	}

	if (node->query.name) {
		if (Box3_Intersects(node->query.bounds, bounds)) {
			if (node->query.result == 1) {
				return false;
			}
		}
	}

	return true;
}

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

	return R_OccludeBox_r(view, bounds, r_world_model->bsp->nodes);
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

	// FIXME: This indexing is ghetto, but should be safe on most GL drivers
	assert(query.name < MAX_OCCLUSION_QUERIES);
	Box3_ToPoints(bounds, r_occlusion.vertexes[query.name]);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBufferSubData(GL_ARRAY_BUFFER,
					query.name * sizeof(r_occlusion.vertexes[0]),
					sizeof(r_occlusion.vertexes[0]),
					r_occlusion.vertexes[query.name]);
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
		query->name = 0;
		R_GetError(NULL);
	}
}

/**
 * @brief
 */
static void R_UpdateOcclusionQuery(const r_view_t *view, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	int32_t side;
	const cm_bsp_plane_t *plane = node->plane->cm;
	const float dist = Cm_DistanceToPlane(view->origin, plane);
	if (dist >= 0.f) {
		side = 0;
	} else {
		side = 1;
	}

	R_UpdateOcclusionQuery(view, node->children[side]);

	r_occlusion_query_t *q = &node->query;
	if (q->name) {

		if (q->available == 0) {
			glGetQueryObjectiv(q->name, GL_QUERY_RESULT_AVAILABLE, &q->available);
			if (q->available || r_occlude->integer == 2) {
				glGetQueryObjectiv(q->name, GL_QUERY_RESULT, &q->result);
			}
		}

		if (q->available) {
			glBeginQuery(GL_ANY_SAMPLES_PASSED, q->name);
			glDrawElementsBaseVertex(GL_TRIANGLES, 36, GL_UNSIGNED_INT, NULL, q->name * 8);
			glEndQuery(GL_ANY_SAMPLES_PASSED);
			q->available = 0;
		}

		if (Box3_ContainsPoint(Box3_Expand(q->bounds, 16.f), view->origin)) {
			q->result = 1;
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

	R_UpdateOcclusionQuery(view, node->children[!side]);
}

/**
 * @brief Updates and re-draws all active occlusion queries for the current frame.
 */
void R_UpdateOcclusionQueries(r_view_t *view) {

	if (!r_occlude->integer) {
		return;
	}

	if (view->flags & VIEW_FLAG_NO_DELTA) {
		//view->num_occlusion_queries = 0;
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

	R_UpdateOcclusionQuery(view, r_world_model->bsp->nodes);

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

	glDeleteVertexArrays(1, &r_occlusion.vertex_array);

	glDeleteBuffers(1, &r_occlusion.vertex_buffer);
	glDeleteBuffers(1, &r_occlusion.elements_buffer);

	R_GetError(NULL);
}
