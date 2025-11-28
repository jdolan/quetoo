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
 * @brief The occlusion query accounting structure.
 * @details Occlusion queries are maintained in a dynamic pool that will grow to meet the demands
 * of the scene. Queues make allocating and freeing queries a constant O(1) operation.
 */
static struct {
  /**
   * @brief The queue of allocated occlusion queries.
   */
  GQueue *allocated;

  /**
   * @brief The queue of free occlusion queries.
   */
  GQueue *free;

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
} r_occlusion_queries;

/**
 * @brief Conservatively checks allocated occlusion queries to determine if the given box is occluded.
 */
bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

  if (!r_occlude->integer) {
    return false;
  }

  if (view->type == VIEW_PLAYER_MODEL) {
    return false;
  }

  const r_bsp_inline_model_t *in = r_models.world->bsp->inline_models;

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (block->query->result == 0) {
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
 * @brief Allocates an occlusion query for the specified bounds.
 */
r_occlusion_query_t *R_AllocOcclusionQuery(const box3_t bounds) {

  r_occlusion_query_t *query = g_queue_pop_head(r_occlusion_queries.free);
  if (query == NULL) {
    query = Mem_TagMalloc(sizeof(r_occlusion_query_t), MEM_TAG_RENDERER);
  }

  query->bounds = bounds;
  query->available = 1;
  query->result = 1;

  g_queue_push_head(r_occlusion_queries.allocated, query);
  return query;
}

/**
 * @brief Frees the specified occlusion query.
 */
void R_FreeOcclusionQuery(r_occlusion_query_t *query) {

  g_queue_remove(r_occlusion_queries.allocated, query);
  g_queue_push_head(r_occlusion_queries.free, query);
}

/**
 * @brief Polls the query for a result and executes it again if available.
 * @return True if the query is occluded (no samples passed the query).
 */
static bool R_DrawOcclusionQuery(const r_view_t *view, r_occlusion_query_t *query) {

  if (query->name == 0) {
    glGenQueries(1, &query->name);
  }

  if (r_occlude->integer == 0) {

    query->available = 1;
    query->result = 1;

  } else if (Box3_ContainsPoint(Box3_Expand(query->bounds, 16.f), view->origin)) {

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

  query->ticks = view->ticks;

  return query->result == 0;
}

/**
 * @brief GCompareDataFunc for occlusion query sorting.
 */
static gint R_OcclusionQuery_Cmp(gconstpointer a, gconstpointer b, gpointer data) {

  const r_occlusion_query_t *p = a;
  const r_occlusion_query_t *q = b;

  return SignOf(Box3_Radius(p->bounds) - Box3_Radius(q->bounds));
}

/**
 * @brief Re-draws all occlusion queries for the current frame.
 */
void R_DrawOcclusionQueries(const r_view_t *view) {

  g_queue_sort(r_occlusion_queries.allocated, R_OcclusionQuery_Cmp, NULL);

  vec3_t *vertexes = malloc(r_occlusion_queries.allocated->length * sizeof(vec3_t) * 8);

  for (guint i = 0; i < r_occlusion_queries.allocated->length; i++) {
    r_occlusion_query_t *query = g_queue_peek_nth(r_occlusion_queries.allocated, i);

    query->base_vertex = i * 8;

    Box3_ToPoints(query->bounds, vertexes + query->base_vertex);
  }

  glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, r_occlusion_queries.allocated->length * sizeof(vec3_t) * 8, vertexes, GL_DYNAMIC_DRAW);

  free(vertexes);

  glUseProgram(r_depth_pass_program.name);

  glBindVertexArray(r_occlusion_queries.vertex_array);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDepthMask(GL_FALSE);

  for (guint i = 0; i < r_occlusion_queries.allocated->length; i++) {
    r_occlusion_query_t *query = g_queue_peek_nth(r_occlusion_queries.allocated, i);

    if (query->ticks == view->ticks) {
      continue;
    }

    if (R_DrawOcclusionQuery(view, query)) {
      r_stats.queries_occluded++;

      // Queries are sorted by size, so if one is occluded, iterate the remaining ones and check
      // if any subsequent queries are contained within it and can be skipped this frame

      for (guint j = i + 1; j < r_occlusion_queries.allocated->length; j++) {
        r_occlusion_query_t *q = g_queue_peek_nth(r_occlusion_queries.allocated, j);

        if (Box3_Contains(query->bounds, q->bounds)) {

          q->available = 1;
          q->result = 0;
          q->ticks = view->ticks;

          r_stats.queries_occluded++;
        }
      }
    } else {
      r_stats.queries_visible++;
    }
  }

  if (r_draw_occlusion_queries->value) {

    for (guint i = 0; i < r_occlusion_queries.allocated->length; i++) {
      r_occlusion_query_t *query = g_queue_peek_nth(r_occlusion_queries.allocated, i);

      const float dist = Vec3_Distance(Box3_Center(query->bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (query->result == 0) {
        R_Draw3DBox(query->bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(query->bounds, Color3f(f, 0.f, 0.f), false);
      }
    }
  }

  glDepthMask(GL_TRUE);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  glBindVertexArray(0);

  glUseProgram(0);

  R_GetError(NULL);

  r_stats.queries_allocated = r_occlusion_queries.allocated->length;
}

/**
 * @brief
 */
void R_InitOcclusionQueries(void) {

  memset(&r_occlusion_queries, 0, sizeof(r_occlusion_queries));

  r_occlusion_queries.free = g_queue_new();
  r_occlusion_queries.allocated = g_queue_new();

  glGenVertexArrays(1, &r_occlusion_queries.vertex_array);
  glBindVertexArray(r_occlusion_queries.vertex_array);

  glGenBuffers(1, &r_occlusion_queries.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);

  glGenBuffers(1, &r_occlusion_queries.elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

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

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) 0);

  glEnableVertexAttribArray(0);

  glBindVertexArray(0);
}

/**
 * @brief GDestroyNotify for deleting occlusion queries.
 */
static void R_ShutdownOcclusionQuery(gpointer data) {

  r_occlusion_query_t *query = data;

  glDeleteQueries(1, &query->name);

  Mem_Free(query);
}

/**
 * @brief
 */
void R_ShutdownOcclusionQueries(void) {

  glDeleteBuffers(1, &r_occlusion_queries.vertex_buffer);
  glDeleteBuffers(1, &r_occlusion_queries.elements_buffer);

  glDeleteVertexArrays(1, &r_occlusion_queries.vertex_array);

  g_queue_free_full(r_occlusion_queries.allocated, R_ShutdownOcclusionQuery);
  g_queue_free_full(r_occlusion_queries.free, R_ShutdownOcclusionQuery);
}
