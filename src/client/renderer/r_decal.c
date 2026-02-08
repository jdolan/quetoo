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
 * @brief Decal rendering state (shared across all models).
 */
static struct {

  /**
   * @brief The total number of decals.
   */
  int32_t num_decals;

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
   * @brief Whether decals have been added or removed (need to rebuild batches).
   */
  bool dirty;
} r_decals;

/**
 * @brief The decal shader program.
 */
static struct {
  GLuint name;
  GLuint uniforms_block;
  GLint model;
  GLint texture_diffusemap;
} r_decal_program;

/**
 * @brief
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  assert(decal);
  assert(decal->media);
  assert(decal->radius > 0.f);
  assert(decal->lifetime > 0);

  if (view->num_decals == MAX_DECALS) {
    Com_Warn("MAX_DECALS\n");
    return;
  }

  r_decal_t *out = &view->decals[view->num_decals++];
  *out = *decal;
}

/**
 * @brief Append clipped vertexes to a projected BSP decal.
 */
static void R_DecalBspFace(const r_view_t *view, const r_bsp_face_t *face, r_bsp_decal_t *decal) {

  const vec3_t org = decal->def.origin;
  const vec3_t n = decal->def.normal;
  const float r = decal->def.radius;

  vec3_t t, b;
  if (fabsf(n.z) > 0.9f) {
    t = Vec3_Cross(n, Vec3(1.f, 0.f, 0.f));
  } else {
    t = Vec3_Cross(n, Vec3(0.f, 0.f, 1.f));
  }

  t = Vec3_Normalize(t);
  b = Vec3_Cross(n, t);

  const vec3_t positions[4] = {
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r))
  };

  const color32_t color = Color_Color32(decal->def.color);

  r_bsp_decal_vertex_t *v = decal->vertexes;
  for (int32_t i = 0; i < 4; i++, v++) {

    v->position = Box3_ClampPoint(face->bounds, positions[i]);

    const vec3_t delta = Vec3_Subtract(v->position, org);
    const float x = (Vec3_Dot(delta, t) / r) * 0.5f + 0.5f; // -radius to +radius → 0 to 1
    const float y = (Vec3_Dot(delta, b) / r) * 0.5f + 0.5f;

    switch (decal->def.media->type) {
      case R_MEDIA_ATLAS_IMAGE: {
        const r_atlas_image_t *img = (r_atlas_image_t *) decal->def.media;
        const vec2_t atlas_min = Vec2(img->texcoords.x, img->texcoords.y);
        const vec2_t atlas_max = Vec2(img->texcoords.z, img->texcoords.w);
        const vec2_t atlas_size = Vec2_Subtract(atlas_max, atlas_min);
        v->diffusemap = Vec2_Add(atlas_min, Vec2(x * atlas_size.x, y * atlas_size.y));
      }
        break;
      case R_MEDIA_IMAGE:
        v[i].diffusemap = Vec2(x, y);
        break;
      default:
        break;
    }

    v->color = color;
    v->time = view->ticks;
    v->lifetime = decal->def.lifetime;
  }
}

/**
 * @brief Recurses down the tree to project the decal onto faces. The resulting `r_bsp_decal_t`s
 * are accumulated on the `r_bsp_block_t` node.
 */
static void R_DecalBspNode(const r_view_t *view, r_bsp_block_t *block, const r_bsp_node_t *node, r_decal_t *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_DecalBspNode(view, block, node->children[0], decal);
    return;
  }

  if (dist < -decal->radius) {
    R_DecalBspNode(view, block, node->children[1], decal);
    return;
  }

  const vec3_t origin = decal->origin;
  const float radius = decal->radius;

  // Project the decal onto the node's plane

  r_decal_t def = *decal;

  def.origin = Vec3_Fmaf(def.origin, -dist, plane->normal);
  def.radius = def.radius - fabsf(dist);

  const GLuint texnum = ((const r_image_t *) decal->media)->texnum;

  const r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & (SURF_SKY | SURF_NO_DRAW)) {
      continue;
    }

    if (block->num_decals == MAX_BSP_BLOCK_DECALS) {
      continue;
    }

    R_DecalBspFace(view, face, &block->decals[block->num_decals++]);

    r_decals.dirty = true;
  }

  R_DecalBspNode(view, block, node->children[0], decal);
  R_DecalBspNode(view, block, node->children[1], decal);
}

/**
 * @brief Generate `r_bsp_decal_t` and `r_draw_elements_t` for the given `r_decal_t` via BSP recursion.
 */
static void R_AddBspModelDecals(const r_view_t *view, r_bsp_inline_model_t *in, r_decal_t *decal) {

  const box3_t bounds = Box3_FromCenterRadius(decal->origin, decal->radius);

  r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (!Box3_Intersects(block->visible_bounds, bounds)) {
      continue;
    }

    R_DecalBspNode(view, block, block->node, decal);
  }
}

/**
 * @brief qsort comparator for `r_bsp_decal_t`.
 */
static int32_t R_UpdateDecals_Cmp(const void *a, const void *b) {

  const r_bsp_decal_t *m = a;
  const r_bsp_decal_t *n = b;

  if (m->def.media == NULL) {
    return  1;
  }

  if (n->def.media == NULL) {
    return -1;
  }

  return ((r_image_t *) m->def.media)->texnum - ((r_image_t *) n->def.media)->texnum;
}

/**
 * @brief
 */
static void R_AddDecals(r_view_t *view) {

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

      R_AddBspModelDecals(view, in, &d);
    }
  }
}

/**
 * @brief
 */
static void R_ExpireDecals(r_view_t *view) {

  r_decals.num_decals = 0;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; i < in->num_blocks; j++, block++) {

      r_bsp_decal_t *decal = block->decals;
      for (int32_t k = 0; k < block->num_decals; k++, decal++) {

        if (view->ticks - decal->time >= decal->def.lifetime) {
          memmove(decal, decal + 1, block->num_decals - j * sizeof(r_bsp_decal_t));
          block->num_decals--;
          k--;
          r_decals.dirty = true;
        }
      }

      r_decals.num_decals += block->num_decals;
    }
  }
}

/**
 * @brief Update all decals in the view.
 */
void R_UpdateDecals(r_view_t *view) {

  R_AddDecals(view);

  R_ExpireDecals(view);

  if (r_decals.num_decals == 0 || r_decals.dirty == false) {
    return;
  }

  // Allocate VBO space

  r_bsp_decal_vertex_t *vertexes = malloc(r_decals.num_decals * 4 * sizeof(r_bsp_decal_vertex_t));
  GLuint *elements = malloc(r_decals.num_decals * 6 * sizeof(GLuint));

  GLuint num_vertexes = 0;
  GLuint num_elements = 0;

  // Upload geometry in (model, texture) batch order

  e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    if (g_hash_table_size(in->decals) == 0) {
      continue;
    }

    GHashTableIter iter;
    g_hash_table_iter_init(&iter, in->decals);

    gpointer key, value;
    while (g_hash_table_iter_next(&iter, &key, &value)) {

      r_bsp_decal_draw_elements_t *batch = value;

      batch->elements = (GLvoid *) (num_elements * sizeof(GLuint));
      batch->num_elements = 0;

      for (guint j = 0; j < batch->decals->len; j++) {
        r_decal_t *decal = g_ptr_array_index(batch->decals, j);

        for (guint k = 0; k < decal->faces->len; k++) {
          r_bsp_decal_face_t *face = g_ptr_array_index(decal->faces, k);
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

          batch->num_elements += 6;
        }
      }
    }
  }

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, num_vertexes * sizeof(r_bsp_decal_vertex_t), vertexes, GL_DYNAMIC_DRAW);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, num_elements * sizeof(GLuint), elements, GL_DYNAMIC_DRAW);

  free(vertexes);
  free(elements);

  r_decals.dirty = false;
}

/**
 * @brief Draws all decals in the view using the decal shader.
 */
void R_DrawDecals(const r_view_t *view) {

  // Count decals from hash tables
  size_t num_decals = 0;
  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {
    if (IS_BSP_INLINE_MODEL(e->model)) {
      const r_bsp_inline_model_t *in = e->model->bsp_inline;
      GHashTableIter iter;
      g_hash_table_iter_init(&iter, in->decals);
      gpointer key, value;
      while (g_hash_table_iter_next(&iter, &key, &value)) {
        const r_bsp_decal_draw_elements_t *batch = value;
        num_decals += batch->decals->len;
      }
    }
  }

  r_stats.decals = num_decals;

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.f, -1.f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUseProgram(r_decal_program.name);

  glBindVertexArray(r_decals.vertex_array);

  e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    const r_bsp_inline_model_t *in = e->model->bsp_inline;

    if (g_hash_table_size(in->decals) == 0) {
      continue;
    }

    glUniformMatrix4fv(r_decal_program.model, 1, GL_FALSE, e->matrix.array);

    GHashTableIter it;
    gpointer key, value;
    g_hash_table_iter_init(&it, in->decals);

    while (g_hash_table_iter_next(&it, &key, &value)) {
      const r_bsp_decal_draw_elements_t *d = value;

      glBindTexture(GL_TEXTURE_2D, d->texnum);
      glDrawElements(GL_TRIANGLES, d->num_elements, GL_UNSIGNED_INT, d->elements);

      r_stats.decal_draw_elements++;
    }
  }

  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

  glDisable(GL_POLYGON_OFFSET_FILL);

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);

  R_GetError(NULL);
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

  r_decal_program.model = glGetUniformLocation(r_decal_program.name, "model");

  r_decal_program.texture_diffusemap = glGetUniformLocation(r_decal_program.name, "texture_diffusemap");
  glUniform1i(r_decal_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief Initialize the decals subsystem.
 */
void R_InitDecals(void) {

  memset(&r_decals, 0, sizeof(r_decals));

  glGenBuffers(1, &r_decals.vertex_buffer);

  glGenBuffers(1, &r_decals.elements_buffer);

  glGenVertexArrays(1, &r_decals.vertex_array);
  glBindVertexArray(r_decals.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_decals.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_decals.elements_buffer);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_bsp_decal_vertex_t), (void *) offsetof(r_bsp_decal_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_bsp_decal_vertex_t), (void *) offsetof(r_bsp_decal_vertex_t, diffusemap));
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(r_bsp_decal_vertex_t), (void *) offsetof(r_bsp_decal_vertex_t, color));
  glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(r_bsp_decal_vertex_t), (void *) offsetof(r_bsp_decal_vertex_t, time));
  glVertexAttribIPointer(4, 1, GL_UNSIGNED_INT, sizeof(r_bsp_decal_vertex_t), (void *) offsetof(r_bsp_decal_vertex_t, lifetime));

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
 * @brief Shutdown the decals subsystem.
 */
void R_ShutdownDecals(void) {

  glDeleteVertexArrays(1, &r_decals.vertex_array);
  glDeleteBuffers(1, &r_decals.vertex_buffer);
  glDeleteBuffers(1, &r_decals.elements_buffer);

  memset(&r_decals, 0, sizeof(r_decals));
}
