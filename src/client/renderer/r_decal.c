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
 * @brief Decal vertex structure.
 */
typedef struct {
  vec3_t position;
  vec2_t texcoord;
  color32_t color;
} r_decal_vertex_t;

/**
 * @brief A decal face represents a single quad of decal geometry.
 */
typedef struct r_decal_face_s {
  /**
   * @brief The quad vertices for this face.
   */
  r_decal_vertex_t vertexes[4];

  /**
   * @brief Next face in the linked list.
   */
  struct r_decal_face_s *next;
} r_decal_face_t;

/**
 * @brief Decal rendering state (shared across all models).
 */
static struct {
  /**
   * @brief The vertex buffer object (shared).
   */
  GLuint vertex_buffer;

  /**
   * @brief The elements buffer object (shared).
   */
  GLuint elements_buffer;

  /**
   * @brief The vertex array object (shared).
   */
  GLuint vertex_array;

  /**
   * @brief Object pool for r_decal_t instances.
   */
  GQueue *decal_pool;

  /**
   * @brief Object pool for r_decal_face_t instances.
   */
  GQueue *face_pool;
} r_decals;

#define DECAL_POOL_BATCH_SIZE 64
#define FACE_POOL_BATCH_SIZE 64

/**
 * @brief Allocate a decal from the pool.
 */
static r_decal_t *R_AllocDecal(void) {

  r_decal_t *d = g_queue_pop_head(r_decals.decal_pool);
  if (!d) {
    r_decal_t *decals = calloc(DECAL_POOL_BATCH_SIZE, sizeof(r_decal_t));
    r_decal_t *decal = decals;
    for (int32_t i = 0; i < DECAL_POOL_BATCH_SIZE; i++, decal++) {
      decal->faces = g_ptr_array_new();
      g_queue_push_tail(r_decals.decal_pool, decal);
    }
    d = g_queue_pop_head(r_decals.decal_pool);
  }
  
  return d;
}

/**
 * @brief Return a decal to the pool, freeing all its faces.
 */
static void R_FreeDecal(r_decal_t *d) {
  if (d->faces) {
    for (guint i = 0; i < d->faces->len; i++) {
      g_queue_push_tail(r_decals.face_pool, g_ptr_array_index(d->faces, i));
    }
    g_ptr_array_set_size(d->faces, 0);
  }
  g_queue_push_tail(r_decals.decal_pool, d);
}

/**
 * @brief Allocate a decal face from the pool.
 */
static r_decal_face_t *R_AllocDecalFace(void) {
  r_decal_face_t *f = g_queue_pop_head(r_decals.face_pool);
  
  if (!f) {
    r_decal_face_t *faces = calloc(FACE_POOL_BATCH_SIZE, sizeof(r_decal_face_t));
    r_decal_face_t *face = faces;
    for (int32_t i = 0; i < FACE_POOL_BATCH_SIZE; i++, face++) {
      g_queue_push_tail(r_decals.face_pool, face);
    }
    f = g_queue_pop_head(r_decals.face_pool);
  }
  
  return f;
}

/**
 * @brief
 */
static void R_DecalBspFace(const r_bsp_face_t *face, r_decal_t *decal) {

  r_decal_face_t *f = R_AllocDecalFace();

  vec3_t tangent, bitangent;
  if (fabsf(decal->normal.z) > 0.9f) {
    tangent = Vec3_Cross(decal->normal, Vec3(1.f, 0.f, 0.f));
  } else {
    tangent = Vec3_Cross(decal->normal, Vec3(0.f, 0.f, 1.f));
  }

  tangent = Vec3_Normalize(tangent);
  bitangent = Vec3_Cross(decal->normal, tangent);

  f->vertexes[0].position = Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent, -decal->radius));
  f->vertexes[1].position = Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent, -decal->radius));
  f->vertexes[2].position = Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent,  decal->radius));
  f->vertexes[3].position = Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent,  decal->radius));

  // TODO: Clip decal quad to face bounds; might be able to just Box3_Clamp()?

  switch (decal->media->type) {
    case R_MEDIA_ATLAS_IMAGE: {
      const r_atlas_image_t *atlas_image = (r_atlas_image_t *) decal->media;
      f->vertexes[0].texcoord = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
      f->vertexes[1].texcoord = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
      f->vertexes[2].texcoord = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
      f->vertexes[3].texcoord = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
    }
      break;
    case R_MEDIA_IMAGE: {
      f->vertexes[0].texcoord = Vec2(0.f, 0.f);
      f->vertexes[1].texcoord = Vec2(1.f, 0.f);
      f->vertexes[2].texcoord = Vec2(1.f, 1.f);
      f->vertexes[3].texcoord = Vec2(0.f, 1.f);
    }
      break;
    default:
      break;
  }

  const color32_t color = Color_Color32(decal->color);

  f->vertexes[0].color = color;
  f->vertexes[1].color = color;
  f->vertexes[2].color = color;
  f->vertexes[3].color = color;

  g_ptr_array_add(decal->faces, f);
}

/**
 * @brief Recurses down the tree to project decal onto faces.
 */
static void R_DecalBspNode(const r_bsp_node_t *node, r_decal_t *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_DecalBspNode(node->children[0], decal);
    return;
  }

  if (dist < -decal->radius) {
    R_DecalBspNode(node->children[1], decal);
    return;
  }

  const vec3_t origin = decal->origin;
  const float radius = decal->radius;

  decal->origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  decal->radius = decal->radius - fabsf(dist);

  r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & (SURF_SKY | SURF_NO_DRAW)) {
      continue;
    }

    R_DecalBspFace(face, decal);
  }

  decal->origin = origin;
  decal->radius = radius;

  R_DecalBspNode(node->children[0], decal);
  R_DecalBspNode(node->children[1], decal);
}

/**
 * @brief Allocate a decal, generate faces via BSP traversal, and add to model's list.
 */
static void R_DecalBspModel(r_bsp_inline_model_t *in, const r_decal_t *decal) {

  r_decal_t *d = R_AllocDecal();

  d->origin = decal->origin;
  d->radius = decal->radius;
  d->normal = decal->normal;
  d->color = decal->color;
  d->media = decal->media;
  d->lifetime = decal->lifetime;

  R_DecalBspNode(in->head_node, d);

  if (d->faces->len > 0) {
    g_ptr_array_add(in->decals, d);
    in->decals_dirty = true;
  } else {
    R_FreeDecal(d);
  }
}

/**
 * @brief
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  assert(decal);
  assert(decal->media);
  assert(decal->radius > 0.f);

  if (view->num_decals == MAX_DECALS) {
    Com_Warn("MAX_DECALS\n");
    return;
  }

  view->decals[view->num_decals++] = *decal;
}

/**
 * @brief Comparison function for sorting decals by texture.
 */
static gint R_CompareDecalsByTexture(gconstpointer a, gconstpointer b) {
  const r_decal_t *da = *(const r_decal_t **) a;
  const r_decal_t *db = *(const r_decal_t **) b;
  
  const GLuint tex_a = ((const r_image_t *) da->media)->texnum;
  const GLuint tex_b = ((const r_image_t *) db->media)->texnum;
  
  return (gint)tex_a - (gint)tex_b;
}

/**
 * @brief Update all decals in the view.
 */
void R_UpdateDecals(r_view_t *view) {

  // add new decals from the view

  for (int32_t i = 0; i < view->num_decals; i++) {
    const r_decal_t *decal = &view->decals[i];

    const r_entity_t *e = view->entities;
    for (int32_t j = 0; j < view->num_entities; j++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      const box3_t bounds = Box3_FromCenterRadius(decal->origin, decal->radius);

      if (!Box3_Intersects(e->abs_model_bounds, bounds)) {
        continue;
      }

      r_bsp_inline_model_t *in = e->model->bsp_inline;

      r_decal_t d = *decal;

      d.time = view->ticks;
      d.faces = NULL;  // Will be set by R_AllocDecal

      d.origin = Mat4_Transform(e->inverse_matrix, decal->origin);
      d.normal = Mat4_Transform(e->inverse_matrix, Vec3_Add(decal->origin, decal->normal));
      d.normal = Vec3_Normalize(Vec3_Subtract(d.normal, d.origin));

      R_DecalBspModel(in, &d);
    }
  }

  // expire existing decals and count faces

  uint32_t num_faces = 0;
  bool needs_upload = false;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;
    
    // Expire old decals
    for (guint j = 0; j < in->decals->len; ) {
      r_decal_t *decal = g_ptr_array_index(in->decals, j);
      
      if (decal->lifetime > 0) {
        const uint32_t age = view->ticks - decal->time;
        if (age >= decal->lifetime) {
          R_FreeDecal(decal);
          g_ptr_array_remove_index_fast(in->decals, j);
          in->decals_dirty = true;
          continue;
        }
      }
      
      num_faces += decal->faces->len;
      j++;
    }
    
    // Sort if dirty
    if (in->decals_dirty) {
      g_ptr_array_sort(in->decals, R_CompareDecalsByTexture);
      in->decals_dirty = false;
      needs_upload = true;
    }
  }

  if (!needs_upload) {
    return;
  }

  // Upload geometry

  r_decal_vertex_t *vertexes = malloc(num_faces * 4 * sizeof(r_decal_vertex_t));
  GLuint *elements = malloc(num_faces * 6 * sizeof(GLuint));

  GLuint num_vertexes = 0;
  GLuint num_elements = 0;

  e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    in->decal_elements = (GLvoid *) (num_elements * sizeof(GLuint));

    for (guint j = 0; j < in->decals->len; j++) {
      r_decal_t *decal = g_ptr_array_index(in->decals, j);

      r_stats.decals++;

      for (guint k = 0; k < decal->faces->len; k++) {
        r_decal_face_t *face = g_ptr_array_index(decal->faces, k);
        const GLuint base_vertex = num_vertexes;

        for (int32_t l = 0; l < 4; l++) {
          vertexes[num_vertexes++] = face->vertexes[l];
        }

        elements[num_elements++] = base_vertex + 0;
        elements[num_elements++] = base_vertex + 1;
        elements[num_elements++] = base_vertex + 2;
        elements[num_elements++] = base_vertex + 0;
        elements[num_elements++] = base_vertex + 2;
        elements[num_elements++] = base_vertex + 3;
      }
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, num_vertexes * sizeof(r_decal_vertex_t), vertexes, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_elements * sizeof(GLuint), elements, GL_DYNAMIC_DRAW);

  free(vertexes);
  free(elements);
}

/**
 * @brief Draws decals for a BSP entity using the bound shader.
 */
void R_DrawBspEntityDecals(const r_view_t *view, const r_entity_t *e) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  const r_bsp_inline_model_t *in = e->model->bsp_inline;

  if (!in->decals || in->decals->len == 0) {
    return;
  }

  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.f, -1.f);

  glBindVertexArray(r_decals.vertex_array);

  GLvoid *elements = in->decal_elements;

  for (guint i = 0; i < in->decals->len; ) {
    r_decal_t *decal = g_ptr_array_index(in->decals, i);
    const GLuint texnum = ((const r_image_t *) decal->media)->texnum;

    glBindTexture(GL_TEXTURE_2D, texnum);

    // Batch decals with the same texture
    int32_t batch_num_faces = 0;
    guint j = i;
    while (j < in->decals->len) {
      r_decal_t *batch = g_ptr_array_index(in->decals, j);
      const GLuint batch_texnum = ((const r_image_t *) batch->media)->texnum;
      if (batch_texnum == texnum) {
        batch_num_faces += batch->faces->len;
        j++;
      } else {
        break;
      }
    }

    glDrawElements(GL_TRIANGLES, batch_num_faces * 6, GL_UNSIGNED_INT, elements);
    r_stats.decal_draw_elements++;

    elements = (GLvoid *) ((intptr_t) elements + batch_num_faces * 6 * sizeof(GLuint));

    i = j;
  }

  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);

  glBindVertexArray(bsp->vertex_array);

  R_GetError(NULL);
}

/**
 * @brief Free all decals associated with an inline model.
 */
void R_FreeInlineModelDecals(r_bsp_inline_model_t *in) {
  if (in->decals) {
    for (guint i = 0; i < in->decals->len; i++) {
      R_FreeDecal(g_ptr_array_index(in->decals, i));
    }
    g_ptr_array_free(in->decals, TRUE);
    in->decals = NULL;
  }
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  r_decals.decal_pool = g_queue_new();
  r_decals.face_pool = g_queue_new();

  glGenBuffers(1, &r_decals.vertex_buffer);

  glGenBuffers(1, &r_decals.elements_buffer);

  glGenVertexArrays(1, &r_decals.vertex_array);
  glBindVertexArray(r_decals.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, texcoord));
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, color));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);

  glBindVertexArray(0);

  R_GetError("R_InitDecals");
}

/**
 * @brief Shutdown the decals subsystem.
 */
void R_ShutdownDecals(void) {

  // Free all pool contents
  if (r_decals.decal_pool) {
    r_decal_t *d;
    while ((d = g_queue_pop_head(r_decals.decal_pool))) {
      if (d->faces) {
        g_ptr_array_free(d->faces, TRUE);
      }
      free(d);
    }
    g_queue_free(r_decals.decal_pool);
  }

  if (r_decals.face_pool) {
    r_decal_face_t *f;
    while ((f = g_queue_pop_head(r_decals.face_pool))) {
      free(f);
    }
    g_queue_free(r_decals.face_pool);
  }

  if (r_decals.vertex_array) {
    glDeleteVertexArrays(1, &r_decals.vertex_array);
  }

  if (r_decals.vertex_buffer) {
    glDeleteBuffers(1, &r_decals.vertex_buffer);
  }

  if (r_decals.elements_buffer) {
    glDeleteBuffers(1, &r_decals.elements_buffer);
  }

  memset(&r_decals, 0, sizeof(r_decals));
}
