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
} r_decals;

/**
 * @brief
 */
static void R_DecalBspFace(const r_bsp_face_t *face, r_decal_t *decal) {

  r_decal_face_t *f = calloc(1, sizeof(r_decal_face_t));

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

  f->next = decal->faces;
  decal->faces = f;
  decal->num_faces++;
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

  r_decal_t d = *decal;

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

  decal->origin = d.origin;
  decal->radius = d.radius;

  R_DecalBspNode(node->children[0], decal);
  R_DecalBspNode(node->children[1], decal);
}

/**
 * @brief Allocate a decal, generate faces via BSP traversal, and insert sorted into model's list.
 */
static void R_DecalBspModel(r_bsp_inline_model_t *in, const r_decal_t *decal) {

  r_decal_t *d = calloc(1, sizeof(r_decal_t));

  *d = *decal;

  R_DecalBspNode(in->head_node, d);

  if (d->faces) {

    const GLuint texnum = ((const r_image_t *) d->media)->texnum;

    r_decal_t **insert = &in->decals;
    r_decal_t *prev = NULL;

    while (*insert) {
      const GLuint insert_texnum = ((const r_image_t *) (*insert)->media)->texnum;
      if (texnum < insert_texnum) {
        break;
      }
      prev = *insert;
      insert = &(*insert)->next;
    }

    d->next = *insert;
    d->prev = prev;
    if (*insert) {
      (*insert)->prev = d;
    }
    *insert = d;
  } else {
    free(d);
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

  assert(decal);
  assert(decal->media);
  assert(decal->radius > 0.f);

  view->decals[view->num_decals] = *decal;

  view->num_decals++;
}

/**
 * @brief Update all decals in the view.
 */
void R_UpdateDecals(r_view_t *view) {

  bool dirty = false;

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

      d.origin = Mat4_Transform(e->inverse_matrix, decal->origin);
      d.normal = Mat4_Transform(e->inverse_matrix, Vec3_Add(decal->origin, decal->normal));
      d.normal = Vec3_Normalize(Vec3_Subtract(d.normal, d.origin));

      R_DecalBspModel(in, &d);

      dirty = true;
    }
  }

  // expire existing decals

  uint32_t num_decals = 0;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;
    for (r_decal_t *decal = in->decals; decal;) {

      r_decal_t *next = decal->next;

//      if (decal->lifetime > 0) {
//        const uint32_t age = view->ticks - decal->time;
//        if (age >= decal->lifetime) {
//          if (decal->prev) {
//            decal->prev->next = decal->next;
//          } else {
//            in->decals = decal->next;
//          }
//          if (decal->next) {
//            decal->next->prev = decal->prev;
//          }
//          free(decal);
//          dirty = true;
//        }
//      }

      decal = next;
      num_decals++;
    }
  }

  if (!dirty) {
    return;
  }

  // and finally, upload the geometry if it has changed

  r_decal_vertex_t *vertexes = malloc(num_decals * 4 * sizeof(r_decal_vertex_t));
  GLuint *elements = malloc(num_decals * 6 * sizeof(GLuint));

  GLuint num_vertexes = 0;
  GLuint num_elements = 0;

  e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    in->decal_elements = (GLvoid *) (num_elements * sizeof(GLuint));

    for (r_decal_t *decal = in->decals; decal; decal = decal->next) {

      r_stats.decals++;

      for (r_decal_face_t *face = decal->faces; face; face = face->next) {
        const GLuint base_vertex = num_vertexes;

        for (int32_t j = 0; j < 4; j++) {
          vertexes[num_vertexes++] = face->vertexes[j];
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
  glBufferSubData(GL_ARRAY_BUFFER, 0, num_vertexes * sizeof(r_decal_vertex_t), vertexes);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, num_elements * sizeof(GLuint), elements);

  free(vertexes);
  free(elements);
}

/**
 * @brief Draws decals for a BSP entity using the bound shader.
 */
void R_DrawBspEntityDecals(const r_view_t *view, const r_entity_t *e) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  const r_bsp_inline_model_t *in = e->model->bsp_inline;

  if (!in->decals) {
    return;
  }

  glDepthMask(GL_FALSE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.f, -1.f);

  /*glBindVertexArray(r_decals.vertex_array);

  GLvoid *elements = in->decal_elements;

  const r_decal_t *decal = in->decals;
  while (decal) {

    const GLuint texnum = ((const r_image_t *) decal->media)->texnum;

    glBindTexture(GL_TEXTURE_2D, texnum);

    int32_t batch_num_faces = 0;

    const r_decal_t *batch = decal;
    while (batch) {
      const GLuint batch_texnum = ((const r_image_t *) batch->media)->texnum;
      if (batch_texnum == texnum) {
        batch_num_faces += batch->num_faces;
        batch = batch->next;
      } else {
        break;
      }
    }

    glDrawElements(GL_TRIANGLES, batch_num_faces * 6, GL_UNSIGNED_INT, elements);
    r_stats.decal_draw_elements++;

    elements = (GLvoid *) ((intptr_t) elements + batch_num_faces * 6 * sizeof(GLuint));

    decal = batch;
  }*/

  glDisable(GL_POLYGON_OFFSET_FILL);
  glDepthMask(GL_TRUE);

  glBindVertexArray(bsp->vertex_array);

  R_GetError(NULL);
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  glGenBuffers(1, &r_decals.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &r_decals.elements_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, GL_DYNAMIC_DRAW);

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
