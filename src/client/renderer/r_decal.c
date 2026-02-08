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
  uint32_t time;
  uint32_t lifetime;
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
   * @brief The queue of allocated decals (in-use).
   */
  GQueue *allocated_decals;

  /**
   * @brief The queue of free decals (available for reuse).
   */
  GQueue *free_decals;

  /**
   * @brief The queue of allocated decal faces (in-use).
   */
  GQueue *allocated_faces;

  /**
   * @brief The queue of free decal faces (available for reuse).
   */
  GQueue *free_faces;
} r_decals;

/**
 * @brief The decal shader program.
 */
static struct {
  GLuint name;
  GLuint uniforms_block;
  GLint texture_diffusemap;
} r_decal_program;

#define DECAL_POOL_BATCH_SIZE 64
#define FACE_POOL_BATCH_SIZE 64

/**
 * @brief Allocate a decal from the pool.
 */
static r_decal_t *R_AllocDecal(void) {

  r_decal_t *d = g_queue_pop_head(r_decals.free_decals);
  if (!d) {
    d = Mem_TagMalloc(sizeof(r_decal_t), MEM_TAG_RENDERER);
    d->faces = g_ptr_array_new();
  }
  
  g_queue_push_head(r_decals.allocated_decals, d);
  return d;
}

/**
 * @brief Return a decal to the pool, freeing all its faces.
 */
static void R_FreeDecal(r_decal_t *d) {
  // Return all faces to free pool
  if (d->faces) {
    for (guint i = 0; i < d->faces->len; i++) {
      r_decal_face_t *face = g_ptr_array_index(d->faces, i);
      g_queue_remove(r_decals.allocated_faces, face);
      g_queue_push_head(r_decals.free_faces, face);
    }
    g_ptr_array_set_size(d->faces, 0);
  }
  
  g_queue_remove(r_decals.allocated_decals, d);
  g_queue_push_head(r_decals.free_decals, d);
}

/**
 * @brief Allocate a decal face from the pool.
 */
static r_decal_face_t *R_AllocDecalFace(void) {
  r_decal_face_t *f = g_queue_pop_head(r_decals.free_faces);
  if (!f) {
    f = Mem_TagMalloc(sizeof(r_decal_face_t), MEM_TAG_RENDERER);
  }
  
  g_queue_push_head(r_decals.allocated_faces, f);
  return f;
}

/**
 * @brief Build a decal face and add it to the faces array.
 */
static void R_DecalBspFace(const r_bsp_face_t *face, const r_decal_t *decal, GPtrArray *faces) {

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

  for (int32_t i = 0; i < 4; i++) {
    f->vertexes[i].color = color;
    f->vertexes[i].time = decal->time;
    f->vertexes[i].lifetime = decal->lifetime;
  }

  g_ptr_array_add(faces, f);
}

/**
 * @brief Recurses down the tree to project decal onto faces.
 */
static void R_DecalBspNode(const r_bsp_node_t *node, r_decal_t *decal, GPtrArray *faces) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_DecalBspNode(node->children[0], decal, faces);
    return;
  }

  if (dist < -decal->radius) {
    R_DecalBspNode(node->children[1], decal, faces);
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

    R_DecalBspFace(face, decal, faces);
  }

  decal->origin = origin;
  decal->radius = radius;

  R_DecalBspNode(node->children[0], decal, faces);
  R_DecalBspNode(node->children[1], decal, faces);
}

/**
 * @brief Generate faces via BSP traversal, and if any are created, allocate a decal and add to model.
 */
static void R_DecalBspModel(const r_view_t *view, r_bsp_inline_model_t *in, r_decal_t *decal) {

  GPtrArray *faces = g_ptr_array_new();

  R_DecalBspNode(in->head_node, decal, faces);

  if (faces->len > 0) {
    r_decal_t *d = R_AllocDecal();
    d->time = view->ticks;

    d->origin = decal->origin;
    d->normal = decal->normal;
    d->radius = decal->radius;
    d->color = decal->color;
    d->media = decal->media;
    d->lifetime = decal->lifetime;

    d->faces = faces;

    g_ptr_array_add(in->decals, d);
    in->decals_dirty = true;
  } else {
    g_ptr_array_free(faces, true);
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

  r_decal_t *out = &view->decals[view->num_decals++];
  *out = *decal;
}

/**
 * @brief Comparison function for sorting decals by texture.
 */
static gint R_DecalsCmp(gconstpointer a, gconstpointer b) {
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

  // Create new decal geometry from those defined in the view

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

      d.origin = Mat4_Transform(e->inverse_matrix, decal->origin);
      d.normal = Mat4_Transform(e->inverse_matrix, Vec3_Add(decal->origin, decal->normal));
      d.normal = Vec3_Normalize(Vec3_Subtract(d.normal, d.origin));

      R_DecalBspModel(view, in, &d);
    }
  }

  // Expire existing decals and count faces

  uint32_t num_faces = 0;
  bool decals_dirty = false;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;
    
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

    decals_dirty |= in->decals_dirty;
  }

  if (!decals_dirty) {
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

    if (in->decals_dirty) {
      g_ptr_array_sort(in->decals, R_DecalsCmp);
    }

    in->decal_elements = (GLvoid *) (num_elements * sizeof(GLuint));

    for (guint j = 0; j < in->decals->len; j++) {
      r_decal_t *decal = g_ptr_array_index(in->decals, j);

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

    in->decals_dirty = false;
  }

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, num_vertexes * sizeof(r_decal_vertex_t), vertexes, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_elements * sizeof(GLuint), elements, GL_DYNAMIC_DRAW);

  free(vertexes);
  free(elements);
}

/**
 * @brief Draws all decals in the view using the decal shader.
 */
void R_DrawDecals(const r_view_t *view) {
  
  r_stats.decals = 0;
  
  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.f, -1.f);
  
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(r_decal_program.name);
  
  glBindVertexArray(r_decals.vertex_array);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {
    
    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    const r_bsp_inline_model_t *in = e->model->bsp_inline;

    if (in->decals->len == 0) {
      continue;
    }

    GLvoid *elements = in->decal_elements;

    for (guint j = 0; j < in->decals->len; ) {
      r_decal_t *decal = g_ptr_array_index(in->decals, j);
      const GLuint texnum = ((const r_image_t *) decal->media)->texnum;

      glBindTexture(GL_TEXTURE_2D, texnum);

      int32_t batch_num_faces = 0;
      guint k = j;
      while (k < in->decals->len) {
        r_decal_t *batch = g_ptr_array_index(in->decals, k);
        const GLuint batch_texnum = ((const r_image_t *) batch->media)->texnum;
        if (batch_texnum == texnum) {
          batch_num_faces += batch->faces->len;
          r_stats.decals++;
          k++;
        } else {
          break;
        }
      }

      glDrawElements(GL_TRIANGLES, batch_num_faces * 6, GL_UNSIGNED_INT, elements);
      r_stats.decal_draw_elements++;

      elements = (GLvoid *) ((intptr_t) elements + batch_num_faces * 6 * sizeof(GLuint));

      j = k;
    }
  }
  
  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);
  
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);

  R_GetError(NULL);
}

/**
 * @brief GDestroyNotify for deleting decals.
 */
static void R_ShutdownDecal(gpointer data) {
  r_decal_t *decal = data;
  if (decal->faces) {
    g_ptr_array_free(decal->faces, true);
  }
  Mem_Free(decal);
}

/**
 * @brief GDestroyNotify for deleting decal faces.
 */
static void R_ShutdownDecalFace(gpointer data) {
  Mem_Free(data);
}

/**
 * @brief Initialize the decal shader program.
 */
static void R_InitDecalProgram(void) {

  memset(&r_decal_program, 0, sizeof(r_decal_program));

  r_decal_program.name = R_LoadProgram(
      R_ShaderDescriptor(GL_VERTEX_SHADER, "decal_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "decal_fs.glsl", NULL),
      NULL);

  glUseProgram(r_decal_program.name);

  r_decal_program.uniforms_block = glGetUniformBlockIndex(r_decal_program.name, "uniforms_block");
  glUniformBlockBinding(r_decal_program.name, r_decal_program.uniforms_block, 0);

  r_decal_program.texture_diffusemap = glGetUniformLocation(r_decal_program.name, "texture_diffusemap");
  glUniform1i(r_decal_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  r_decals.free_decals = g_queue_new();
  r_decals.allocated_decals = g_queue_new();
  r_decals.free_faces = g_queue_new();
  r_decals.allocated_faces = g_queue_new();

  glGenBuffers(1, &r_decals.vertex_buffer);

  glGenBuffers(1, &r_decals.elements_buffer);

  glGenVertexArrays(1, &r_decals.vertex_array);
  glBindVertexArray(r_decals.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, texcoord));
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, color));
  glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, time));
  glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(r_decal_vertex_t), (void *) offsetof(r_decal_vertex_t, lifetime));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glEnableVertexAttribArray(3);
  glEnableVertexAttribArray(4);

  glBindVertexArray(0);
  
  R_InitDecalProgram();

  R_GetError("R_InitDecals");
}

/**
 * @brief Reclaims all allocated decals to the free pool. Called between level loads.
 */
void R_FreeDecals(void) {
  r_decal_t *d;
  while ((d = g_queue_pop_head(r_decals.allocated_decals))) {
    if (d->faces) {
      g_ptr_array_set_size(d->faces, 0);
    }
    g_queue_push_head(r_decals.free_decals, d);
  }
  
  r_decal_face_t *f;
  while ((f = g_queue_pop_head(r_decals.allocated_faces))) {
    g_queue_push_head(r_decals.free_faces, f);
  }
}

/**
 * @brief Shutdown the decals subsystem.
 */
void R_ShutdownDecals(void) {

  glDeleteVertexArrays(1, &r_decals.vertex_array);
  glDeleteBuffers(1, &r_decals.vertex_buffer);
  glDeleteBuffers(1, &r_decals.elements_buffer);

  g_queue_free_full(r_decals.allocated_decals, R_ShutdownDecal);
  g_queue_free_full(r_decals.free_decals, R_ShutdownDecal);
  g_queue_free_full(r_decals.allocated_faces, R_ShutdownDecalFace);
  g_queue_free_full(r_decals.free_faces, R_ShutdownDecalFace);

  memset(&r_decals, 0, sizeof(r_decals));
}
