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
   * @brief Dynamic vertex buffer for decals.
   */
  r_decal_vertex_t vertexes[MAX_DECALS * 4];
  int32_t num_vertexes;

  /**
   * @brief Dynamic element buffer for decals.
   */
  GLuint elements[MAX_DECALS * 6];
  int32_t num_elements;
} r_decals;

/**
 * @brief Allocate a decal.
 */
static r_decal_t *R_AllocDecal(void) {
  return Mem_TagMalloc(sizeof(r_decal_t), MEM_TAG_RENDERER);
}

/**
 * @brief Free a decal and its face list.
 */
void R_FreeDecal(r_decal_t *decal) {

  r_decal_face_t *face = (r_decal_face_t*)decal->faces;
  while (face) {
    r_decal_face_t *next = face->next;
    Mem_Free(face);
    face = next;
  }
  
  Mem_Free(decal);
}

/**
 * @brief
 */
static void R_DecalTextureCoordinates(const r_media_t *media, vec2_t *tl, vec2_t *tr, vec2_t *br, vec2_t *bl) {

  switch (media->type) {
    case R_MEDIA_ATLAS_IMAGE: {
      const r_atlas_image_t *atlas_image = (r_atlas_image_t *) media;
      *tl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.y);
      *tr = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.y);
      *br = Vec2(atlas_image->texcoords.z, atlas_image->texcoords.w);
      *bl = Vec2(atlas_image->texcoords.x, atlas_image->texcoords.w);
    }
      break;
    case R_MEDIA_IMAGE: {
      *tl = Vec2(0.f, 0.f);
      *tr = Vec2(1.f, 0.f);
      *br = Vec2(1.f, 1.f);
      *bl = Vec2(0.f, 1.f);
    }
      break;
    default:
      assert(false);
      break;
  }
}

/**
 * @brief
 */
static void R_DecalBspFace(r_decal_t *decal, const r_bsp_face_t *face) {

  vec3_t tangent, bitangent;

  if (fabsf(decal->normal.z) > 0.9f) {
    tangent = Vec3_Cross(decal->normal, Vec3(1.f, 0.f, 0.f));
  } else {
    tangent = Vec3_Cross(decal->normal, Vec3(0.f, 0.f, 1.f));
  }

  tangent = Vec3_Normalize(tangent);
  bitangent = Vec3_Cross(decal->normal, tangent);

  const vec3_t extents[4] = {
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent, -decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent, -decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent,  decal->radius)), Vec3_Scale(bitangent,  decal->radius)),
    Vec3_Add(Vec3_Add(decal->origin, Vec3_Scale(tangent, -decal->radius)), Vec3_Scale(bitangent,  decal->radius))
  };

  vec2_t texcoords[4];
  R_DecalTextureCoordinates(decal->media, &texcoords[0], &texcoords[1], &texcoords[2], &texcoords[3]);

  // TODO: Clip decal quad to face bounds - for now just add the quad
  // A full implementation would intersect the quad with the face polygon

  const color32_t color = Color_Color32(decal->color);

  r_decal_face_t *decal_face = Mem_TagMalloc(sizeof(r_decal_face_t), MEM_TAG_RENDERER);
  
  for (int32_t i = 0; i < 4; i++) {
    decal_face->vertexes[i] = (r_decal_vertex_t) {
      .position = extents[i],
      .texcoord = texcoords[i],
      .color = color
    };
  }

  decal_face->next = decal->faces;
  decal->faces = decal_face;
  decal->num_faces++;
}

/**
 * @brief Recurses down the tree to project decal onto faces.
 */
static void R_DecalBspNode(r_decal_t *decal, const r_bsp_node_t *node) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_DecalBspNode(decal, node->children[0]);
    return;
  }

  if (dist < -decal->radius) {
    R_DecalBspNode(decal, node->children[1]);
    return;
  }

  r_decal_t d = *decal;

  d.origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  d.radius = decal->radius - fabsf(dist);

  r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & (SURF_SKY | SURF_NO_DRAW)) {
      continue;
    }

    R_DecalBspFace(&d, face);
  }

  R_DecalBspNode(decal, node->children[0]);
  R_DecalBspNode(decal, node->children[1]);
}

/**
 * @brief Adds a decal to a BSP structure (model or inline model).
 */
static void R_DecalBspInlineModel(r_bsp_inline_model_t *in, const r_decal_t *decal) {

  r_decal_t *d = R_AllocDecal();
  *d = *decal;

  R_DecalBspNode(d, in->head_node);

  if (d->num_faces > 0) {
    g_queue_push_tail(in->decals, d);
    in->decals_dirty = true;
  } else {
    R_FreeDecal(d);
  }
}

/**
 * @brief
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  if (view->num_decals == MAX_DECALS) {
    Com_Warn("MAX_DECALS\n");
    return;
  }

  view->decals[view->num_decals++] = *decal;
}

/**
 * @brief
 */
static bool R_UpdateBspEntityDecals(const r_view_t *view, const r_entity_t *e, r_bsp_inline_model_t *in) {

  for (GList *link = in->decals->head; link;) {

    r_decal_t *decal = link->data;
    GList *next = link->next;

    if (decal->lifetime > 0) {
      const uint32_t age = view->ticks - decal->time;
      if (age >= decal->lifetime) {
        g_queue_remove(in->decals, decal);
        R_FreeDecal(decal);
        in->decals_dirty = true;
        link = next;
        continue;
      }
    }

    link = next;
  }

  return in->decals_dirty;
}

/**
 * @brief Update all decals in the view.
 */
void R_UpdateDecals(r_view_t *view) {

  for (int32_t i = 0; i < view->num_decals; i++) {
    const r_decal_t *decal = &view->decals[i];

    const r_entity_t *e = view->entities;
    for (int32_t j = 0; j < view->num_entities; j++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      r_bsp_inline_model_t *in = e->model->bsp_inline;
      r_decal_t d = *decal;

      d.origin = Mat4_Transform(e->inverse_matrix, decal->origin);
      d.normal = Mat4_Transform(e->inverse_matrix, Vec3_Add(decal->origin, decal->normal));
      d.normal = Vec3_Normalize(Vec3_Subtract(d.normal, d.origin));

      R_DecalBspInlineModel(in, &d);
    }
  }

  bool dirty = false;
  
  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    dirty |= R_UpdateBspEntityDecals(view, e, in);
  }

  if (dirty) { // most frames, decals are unmodified, and do not require uploading

    r_decals.num_vertexes = 0;
    r_decals.num_elements = 0;

    const r_entity_t *e = view->entities;
    for (int32_t i = 0; i < view->num_entities; i++, e++) {

      if (!IS_BSP_INLINE_MODEL(e->model)) {
        continue;
      }

      r_bsp_inline_model_t *in = e->model->bsp_inline;

      for (GList *link = in->decals->head; link; link = link->next) {
        r_decal_t *decal = link->data;

        for (r_decal_face_t *face = (r_decal_face_t*) decal->faces; face; face = face->next) {

          const int32_t base_vertex = r_decals.num_vertexes;

          for (int32_t i = 0; i < 4; i++) {
            r_decals.vertexes[r_decals.num_vertexes++] = face->vertexes[i];
          }

          r_decals.elements[r_decals.num_elements++] = base_vertex + 0;
          r_decals.elements[r_decals.num_elements++] = base_vertex + 1;
          r_decals.elements[r_decals.num_elements++] = base_vertex + 2;
          r_decals.elements[r_decals.num_elements++] = base_vertex + 0;
          r_decals.elements[r_decals.num_elements++] = base_vertex + 2;
          r_decals.elements[r_decals.num_elements++] = base_vertex + 3;
        }
      }
    }

    glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, r_decals.num_vertexes * sizeof(r_decal_vertex_t), r_decals.vertexes);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, r_decals.num_elements * sizeof(GLuint), r_decals.elements);
  }
}

/**
 * @brief Draw decals for a BSP entity using the BSP shader (must already be bound).
 */
void R_DrawBspEntityDecals(const r_view_t *view, const r_entity_t *e) {

  const r_bsp_inline_model_t *in = e->model->bsp_inline;

  if (g_queue_is_empty(in->decals)) {
    return;
  }

  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.0, -1.0);

  glBindVertexArray(r_decals.vertex_array);

  // TODO: Batch by texture for efficiency

  r_decal_t *first = g_queue_peek_head(in->decals);
  const r_image_t *image = (const r_image_t *) first->media;

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
  glBindTexture(GL_TEXTURE_2D, image->texnum);

  glDrawElements(GL_TRIANGLES, r_decals.num_elements, GL_UNSIGNED_INT, NULL);
  r_stats.decal_draw_elements++;

  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);

  R_GetError(NULL);
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  glGenBuffers(1, &r_decals.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(r_decals.vertexes), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &r_decals.elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(r_decals.elements), NULL, GL_DYNAMIC_DRAW);

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
