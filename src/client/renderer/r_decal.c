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
 * @brief The decal shader program.
 */
static struct {
  /**
   * @brief The program name.
   */
  GLuint name;

  /**
   * @brief The uniform block binding location.
   */
  GLuint uniforms_block;

  /**
   * @brief The model matrix.
   */
  GLint model;

  /**
   * @brief The decal texture sampler.
   */
  GLint texture_diffusemap;
} r_decal_program;

/**
 * @brief
 */
void R_AddDecal(r_view_t *view, const r_decal_t *decal) {

  assert(decal);
  assert(decal->image);
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
static void R_ClipDecalToFace(const r_view_t *view,
                              const r_bsp_face_t *face,
                              const r_decal_t *decal,
                              r_decal_vertex_t *vertexes) {

  const vec3_t org = decal->origin;
  const vec3_t n = decal->normal;
  const float r = decal->radius;

  vec3_t t, b;
  if (fabsf(n.z) > 0.9f) {
    t = Vec3_Cross(n, Vec3(1.f, 0.f, 0.f));
  } else {
    t = Vec3_Cross(n, Vec3(0.f, 0.f, 1.f));
  }

  t = Vec3_Normalize(t);
  b = Vec3_Cross(t, n);

  const vec3_t positions[4] = {
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r))
  };

  const color32_t color = Color_Color32(decal->color);

  r_decal_vertex_t *v = vertexes;
  for (int32_t i = 0; i < 4; i++, v++) {

    v->position = Box3_ClampPoint(face->bounds, positions[i]);

    const vec3_t delta = Vec3_Subtract(v->position, org);

    const float x = (Vec3_Dot(delta, t) / r) * 0.5f + 0.5f; // -radius to +radius → 0 to 1
    const float y = (Vec3_Dot(delta, b) / r) * 0.5f + 0.5f;

    const vec2_t atlas_min = decal->image->texcoords.xy;
    const vec2_t atlas_max = decal->image->texcoords.zw;
    const vec2_t atlas_size = Vec2_Subtract(atlas_max, atlas_min);

    v->texcoord = Vec2_Add(atlas_min, Vec2(x * atlas_size.x, y * atlas_size.y));

    v->color = color;
    v->time = decal->time;
    v->lifetime = decal->lifetime;
  }
}

/**
 * @brief Recurses down the tree to project the decal onto faces. The resulting `r_bsp_decal_t`s
 * are accumulated on the `r_bsp_block_t` node.
 */
static void R_AddBspBlockDecals(const r_view_t *view,
                                const r_bsp_node_t *node,
                                const r_decal_t *decal,
                                r_bsp_block_decals_t *decals) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_AddBspBlockDecals(view, node->children[0], decal, decals);
    return;
  }

  if (dist < -decal->radius) {
    R_AddBspBlockDecals(view, node->children[1], decal, decals);
    return;
  }

  const r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & (SURF_SKY | SURF_NO_DRAW)) {
      continue;
    }

    if (decals->vertexes->len == MAX_BSP_BLOCK_DECALS) {
      Com_Warn("MAX_BSP_BLOCK_DECALS\n");
      continue;
    }

    g_array_append_val(decals->instances, *decal);

    r_decal_vertex_t vertexes[4];
    R_ClipDecalToFace(view, face, decal, vertexes);

    decals->vertexes = g_array_append_val(decals->vertexes, vertexes);

    decals->dirty = true;
  }

  R_AddBspBlockDecals(view, node->children[0], decal, decals);
  R_AddBspBlockDecals(view, node->children[1], decal, decals);
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

    R_AddBspBlockDecals(view, block->node, decal, &block->decals);
  }
}

/**
 * @brief Add new decals from the view and expiring the existing ones.
 */
void R_UpdateDecals(r_view_t *view) {

  for (int32_t i = 0; i < view->num_decals; i++) {
    r_decal_t *decal = &view->decals[i];

    decal->time = view->ticks;

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

      R_AddBspModelDecals(view, in, &d);
    }
  }

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    r_bsp_inline_model_t *in = e->model->bsp_inline;

    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {
      r_bsp_block_decals_t *decals = &block->decals;

      for (guint k = 0; k < decals->instances->len; k++) {
        r_decal_t *d = &g_array_index(decals->instances, r_decal_t, k);

        if (view->ticks - d->time >= d->lifetime) {
          decals->instances = g_array_remove_index_fast(decals->instances, k);
          decals->dirty = true;
        }
      }

      for (guint k = 0; k < decals->vertexes->len; k++) {
        r_decal_vertex_t *v = &g_array_index(decals->vertexes, r_decal_vertex_t, k);

        if (view->ticks - v->time >= v->lifetime) {
          decals->vertexes = g_array_remove_index_fast(decals->vertexes, k);
          decals->dirty = true;
        }
      }
    }
  }

  R_GetError(NULL);

}

/**
 * @brief Draws all decals in the view using the decal shader.
 */
void R_DrawDecals(const r_view_t *view) {

  glUseProgram(r_decal_program.name);

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  glDepthMask(GL_FALSE);

  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(-1.f, -1.f);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (R_CullEntity(view, e)) {
      continue;
    }

    glUniformMatrix4fv(r_decal_program.model, 1, GL_FALSE, e->matrix.array);

    const r_bsp_inline_model_t *in = e->model->bsp_inline;

    r_bsp_block_t *block = in->blocks;
    for (int32_t j = 0; j < in->num_blocks; j++, block++) {

      if (block->query && block->query->result == 0) {
        continue;
      }

      r_bsp_block_decals_t *d = &block->decals;

      if (d->instances->len == 0) {
        continue;
      }

      if (d->dirty) {
        const GLsizei size = d->vertexes->len * sizeof(r_decal_vertex_t[4]);
        glBindBuffer(GL_ARRAY_BUFFER, d->vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, size, d->vertexes->data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        d->dirty = false;
      }

      glBindVertexArray(d->vertex_array);

      const r_decal_t *decal = &g_array_index(d->instances, r_decal_t, 0);
      const r_image_t *image = (r_image_t *) decal->image;
      glBindTexture(GL_TEXTURE_2D, image->texnum);

      glDrawElements(GL_TRIANGLES, d->vertexes->len * 6, GL_UNSIGNED_INT, NULL);

      r_stats.decals += d->vertexes->len;
      r_stats.decal_draw_elements++;
    }
  }


  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

  glDisable(GL_POLYGON_OFFSET_FILL);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  glDepthMask(GL_TRUE);

  glBindVertexArray(0);

  glUseProgram(0);

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

  R_InitDecalProgram();
}

/**
 * @brief
 */
static void R_ShutdownDecalProgram(void) {

  glDeleteProgram(r_decal_program.name);

  r_decal_program.name = 0;
}


/**
 * @brief Shutdown the decals subsystem.
 */
void R_ShutdownDecals(void) {

  R_ShutdownDecalProgram();
}
