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

	const r_bsp_occlusion_query_t *query = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, query++) {

		if (query->ticks != view->ticks) {
			continue;
		}

		if (query->result == 0) {
			continue;
		}

		if (Box3_Intersects(query->bounds, bounds)) {
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
 * @brief Creates occlusion queries on a grid, and clips them to the BSP to reduce overdraw.
 */
void R_CreateOcclusionQueries(r_bsp_model_t *bsp) {

	GArray *queries = g_array_new(false, false, sizeof(r_bsp_occlusion_query_t));

	const float size = r_occlusion_query_size->value;
	const box3_t world_bounds = bsp->nodes->visible_bounds;
	const vec3_t grid_mins = Vec3_Scale(Vec3_Floorf(Vec3_Scale(world_bounds.mins, 1.f / size)), size);
	const vec3_t grid_maxs = Vec3_Scale(Vec3_Ceilf(Vec3_Scale(world_bounds.maxs, 1.f / size)), size);

	for (float x = grid_mins.x; x < grid_maxs.x; x+= size) {
		for (float y = grid_mins.y; y < grid_maxs.y; y+= size) {
			for (float z = grid_mins.z; z < grid_maxs.z; z += size) {

				const vec3_t mins = Vec3(x, y, z);
				const vec3_t maxs = Vec3_Add(mins, Vec3(size, size, size));
				const box3_t bounds = Box3(mins, maxs);

				r_bsp_occlusion_query_t query = {
					.bounds = Box3_Null(),
					.available = 1,
					.result = 1
				};

				int32_t leafs[256];
				const size_t num_leafs = Cm_BoxLeafnums(bounds, leafs, lengthof(leafs), NULL, 0);
				for (size_t i = 0; i < num_leafs; i++) {
					const r_bsp_leaf_t *leaf = bsp->leafs + leafs[i];

					if (!(leaf->contents & CONTENTS_SOLID)) {
						query.bounds = Box3_Union(query.bounds, leaf->bounds);
					}
				}

				if (Box3_IsNull(query.bounds)) {
					continue;
				}

				query.bounds = Box3_Intersection(query.bounds, bounds);

				int32_t top_node;
				Cm_BoxLeafnums(query.bounds, leafs, lengthof(leafs), &top_node, 0);
				query.node = bsp->nodes + top_node;

				g_array_append_val(queries, query);
			}
		}
	}

	bsp->num_occlusion_queries = queries->len;
	bsp->occlusion_queries = Mem_LinkMalloc(sizeof(r_bsp_occlusion_query_t) * bsp->num_occlusion_queries, bsp);
	memcpy(bsp->occlusion_queries, queries->data, sizeof(r_bsp_occlusion_query_t) * bsp->num_occlusion_queries);

	g_array_free(queries, true);

	vec3_t vertexes[8];

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, bsp->num_occlusion_queries * sizeof(vertexes), NULL, GL_STATIC_DRAW);

	r_bsp_occlusion_query_t *query = bsp->occlusion_queries;
	for (int32_t i = 0; i < bsp->num_occlusion_queries; i++, query++) {

		glGenQueries(1, &query->name);
		query->base_vertex = i * lengthof(vertexes);

		Box3_ToPoints(query->bounds, vertexes);
		glBufferSubData(GL_ARRAY_BUFFER, i * sizeof(vertexes), sizeof(vertexes), vertexes);

		query->next = query->node->occlusion_queries;
		query->node->occlusion_queries = query;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	R_GetError(NULL);
}

/**
 * @brief Polls the query for a result and executes it again if available.
 * @return The query result.
 */
static GLint R_UpdateOcclusionQuery(const r_view_t *view, r_bsp_occlusion_query_t *query) {

	query->ticks = view->ticks;

	if (Box3_ContainsPoint(query->bounds, view->origin)) {
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

	return query->result;
}

/**
 * @brief
 */
static void R_UpdateOcclusionQueries_r(const r_view_t *view, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (R_CullBox(view, node->visible_bounds)) {
		return;
	}

	const int32_t side = Cm_DistanceToPlane(view->origin, node->plane->cm) >= 0.f ? 0 : 1;

	R_UpdateOcclusionQueries_r(view, node->children[side]);

	bool occluded = true;

	r_bsp_occlusion_query_t *query = node->occlusion_queries;
	while (query) {

		if (R_UpdateOcclusionQuery(view, query)) {
			occluded = false;
		}

		query = query->next;
	}

	if (occluded) {
		return;
	}

	R_UpdateOcclusionQueries_r(view, node->children[!side]);
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

#if 0
	r_bsp_occlusion_query_t *query = (r_bsp_occlusion_query_t *) r_occlusion.queries->data;
	for (guint i = 0; i < r_occlusion.queries->len; i++, query++) {
		R_UpdateOcclusionQuery(view, query);
	}
#else
	R_UpdateOcclusionQueries_r(view, r_world_model->bsp->nodes);
#endif

	r_bsp_occlusion_query_t *query = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, query++) {

		if (query->ticks != view->ticks) {
			continue;
		}

		if (query->result) {
			r_stats.occlusion_queries_visible++;
		} else {
			r_stats.occlusion_queries_occluded++;
		}

		if (r_draw_occlusion_queries->value) {
			const float dist = Vec3_Distance(Box3_Center(query->bounds), view->origin);
			const float f = 1.f - Clampf(dist / MAX_WORLD_COORD, 0.f, 1.f);
			if (query->result) {
				R_Draw3DBox(query->bounds, Color3f(f, 0.f, 0.f), false);
			} else {
				R_Draw3DBox(query->bounds, Color3f(0.f, f, 0.f), false);
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
void R_DestroyOcclusionQueries(r_bsp_model_t *bsp) {

	r_bsp_occlusion_query_t *q = bsp->occlusion_queries;
	for (int32_t i = 0; i < bsp->num_occlusion_queries; i++, q++) {
		glDeleteQueries(1, &q->name);
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
