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
 * of the scene. Lists make allocating and freeing queries a constant O(1) operation.
 */
static struct {

  /**
   * @brief The allocated occlusion queries.
   */
  List *allocated;

  /**
   * @brief The free occlusion queries.
   */
  List *free;

  /**
   * @brief If true, rebuild the vertex buffer at next frame.
   */
  bool dirty;

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

static r_occlusion_query_t *R_PopOcclusionQuery(List *queries) {

  if (queries == NULL || queries->head == NULL) {
    return NULL;
  }

  ListNode *node = queries->head;
  r_occlusion_query_t *query = node->element;

  $(queries, removeNode, node);

  return query;
}

/**
 * @brief Checks BSP block occlusion queries to determine if the given box is occluded.
 * @details Note that we check the node's bounds, not the query's bounds. This is because the query
 * is tightly bound to just the BSP faces within each block, but here we want to test against each
 * block's spatial partition.
 */
bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

  return false; // TODO(#864): occlusion queries retired during bring-up (block->query is NULL); Phase 7 Hi-Z

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
      if (Box3_Contains(block->query->bounds, bounds)) {
        return true;
      }
      continue;
    }

    if (Box3_Intersects(block->query->bounds, bounds)) {
      return false;
    }
  }

  return true;
}

/**
 * @brief Tests whether the given sphere origin is occluded by any occlusion query result.
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
 * @brief Allocates an occlusion query with the specified bounds.
 */
r_occlusion_query_t *R_AllocOcclusionQuery(const box3_t bounds) {

  // TODO(#864): occlusion queries retired during SDL_gpu bring-up (the whole
  // subsystem is uninitialised while R_InitOcclusionQueries is bypassed).
  // Ported in Phase 7 (compute Hi-Z occlusion). Callers store the returned NULL
  // as block/light->query; R_FreeOcclusionQuery null-checks.
  return NULL;

  r_occlusion_query_t *query = R_PopOcclusionQuery(r_occlusion_queries.free);
  if (query == NULL) {
    query = Mem_TagMalloc(sizeof(r_occlusion_query_t), MEM_TAG_RENDERER);
    glGenQueries(1, &query->name);
  }

  query->bounds = bounds;
  query->available = 1;
  query->result = 1;

  r_occlusion_queries.dirty = true;

  $(r_occlusion_queries.allocated, prepend, query);
  return query;
}

/**
 * @brief Frees the specified occlusion query.
 */
void R_FreeOcclusionQuery(r_occlusion_query_t *query) {

  if (!query) {
    return;
  }

  ListNode *node = $(r_occlusion_queries.allocated, nodeForElement, query);
  if (node) {
    $(r_occlusion_queries.allocated, removeNode, node);
  }
  $(r_occlusion_queries.free, prepend, query);

  r_occlusion_queries.dirty = true;
}

/**
 * @brief Polls the query for a result and executes it again if available.
 * @return True if the query is occluded (no samples passed the query).
 */
static bool R_DrawOcclusionQuery(const r_view_t *view, r_occlusion_query_t *query) {

  if (r_occlude->integer == 0) {

    query->available = 1;
    query->result = 1;

  } else {

    if (Box3_Intersects(query->bounds, Box3_FromCenterRadius(view->origin, BSP_VOXEL_SIZE))) {

      query->available = 1;
      query->result = 1;

    } else {

      if (R_CullBox(view, query->bounds)) {

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
    }
  }

  return query->result == 0;
}

/**
 * @brief Updates the occlusion query vertex buffer when queries are modified.
 */
void R_UpdateOcclusionQueries(const r_view_t *view) {

  if (!r_occlusion_queries.dirty) {
    return;
  }

  const size_t num_queries = r_occlusion_queries.allocated->count;
  vec3_t *vertexes = malloc(num_queries * sizeof(vec3_t) * 8);

  GLint base_vertex = 0;
  for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
    r_occlusion_query_t *query = node->element;

    query->base_vertex = base_vertex;
    Box3_ToPoints(query->bounds, vertexes + base_vertex);
    base_vertex += 8;
  }

  glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, num_queries * sizeof(vec3_t) * 8, vertexes, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  free(vertexes);

  r_occlusion_queries.dirty = false;
}

/**
 * @brief Re-draws all occlusion queries for the current frame.
 */
void R_DrawOcclusionQueries(const r_view_t *view) {

  glUseProgram(r_depth_pass_program.name);

  glBindVertexArray(r_occlusion_queries.vertex_array);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glDepthMask(GL_FALSE);

  for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
    r_occlusion_query_t *query = node->element;

    if (R_DrawOcclusionQuery(view, query)) {
      r_stats.queries_occluded++;
    } else {
      r_stats.queries_visible++;
    }
  }

  if (r_draw_occlusion_queries->value) {

    for (const ListNode *node = r_occlusion_queries.allocated->head; node; node = node->next) {
      r_occlusion_query_t *query = node->element;
      const float dist = Vec3_Distance(Box3_Center(query->bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (query->result == 0) {
        R_Draw3DBox(query->bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(query->bounds, Color3f(f, 0.f, 0.f), false);
      }
    }
  }

  if (r_draw_bsp_blocks->value) {

    r_bsp_block_t *b = r_models.world->bsp->inline_models->blocks;
    for (int32_t i = 0; i < r_models.world->bsp->inline_models->num_blocks; i++, b++) {
      const float dist = Vec3_Distance(Box3_Center(b->visible_bounds), view->origin);
      const float f = 1.f - Clampf01(dist / MAX_WORLD_COORD);
      if (b->query->result == 0) {
        R_Draw3DBox(b->visible_bounds, Color3f(0.f, f, 0.f), false);
      } else {
        R_Draw3DBox(b->visible_bounds, Color3f(f, 0.f, 0.f), false);
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

  r_stats.queries_allocated = (int32_t) r_occlusion_queries.allocated->count;
}

/**
 * @brief GDestroyNotify for deleting occlusion queries.
 */
static void R_ShutdownOcclusionQuery(void * data) {

  r_occlusion_query_t *query = data;

  glDeleteQueries(1, &query->name);

  Mem_Free(query);
}

/**
 * @brief Initializes occlusion query state, allocating GPU buffers for bounding box rendering.
 */
void R_InitOcclusionQueries(void) {

  memset(&r_occlusion_queries, 0, sizeof(r_occlusion_queries));

  r_occlusion_queries.free = $(alloc(List), init);
  r_occlusion_queries.allocated = $(alloc(List), init);

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
 * @brief Shuts down occlusion query state, freeing all GPU resources and allocated queries.
 */
void R_ShutdownOcclusionQueries(void) {

  glDeleteBuffers(1, &r_occlusion_queries.vertex_buffer);
  glDeleteBuffers(1, &r_occlusion_queries.elements_buffer);

  glDeleteVertexArrays(1, &r_occlusion_queries.vertex_array);

  r_occlusion_queries.allocated->destroy = R_ShutdownOcclusionQuery;
  r_occlusion_queries.allocated = release(r_occlusion_queries.allocated);

  r_occlusion_queries.free->destroy = R_ShutdownOcclusionQuery;
  r_occlusion_queries.free = release(r_occlusion_queries.free);
}
