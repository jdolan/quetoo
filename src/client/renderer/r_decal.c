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
static bool R_ClipDecalToFace(const r_view_t *view,
                              const r_bsp_face_t *face,
                              const r_decal_t *decal,
                              r_decal_vertexes_t *out) {

  const vec3_t org = decal->origin;
  const vec3_t n = face->plane->cm->normal;
  const float r = decal->radius;

  vec3_t t, b;
  if (fabsf(n.z) > 0.9f) {
    t = Vec3_Cross(n, Vec3(1.f, 0.f, 0.f));
  } else {
    t = Vec3_Cross(n, Vec3(0.f, 0.f, 1.f));
  }

  t = Vec3_Normalize(t);
  b = Vec3_Cross(t, n);

  // Apply rotation around face normal
  if (decal->rotation != 0.f) {
    const float cos_rot = cosf(decal->rotation);
    const float sin_rot = sinf(decal->rotation);
    const vec3_t t_rot = Vec3_Add(Vec3_Scale(t, cos_rot), Vec3_Scale(b, sin_rot));
    const vec3_t b_rot = Vec3_Add(Vec3_Scale(b, cos_rot), Vec3_Scale(t, -sin_rot));
    t = t_rot;
    b = b_rot;
  }

  cm_winding_t *dw = Cm_AllocWinding(4);
  dw->num_points = 4;
  dw->points[0] = Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r));
  dw->points[1] = Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r));
  dw->points[2] = Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r));
  dw->points[3] = Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r));

  cm_winding_t *fw = Cm_AllocWinding(face->num_vertexes);
  fw->num_points = face->num_vertexes;
  for (int32_t i = 0; i < face->num_vertexes; i++) {
    fw->points[i] = face->vertexes[i].position;
  }

  cm_winding_t *w = Cm_ClipWindingToWinding(dw, fw, face->plane->cm->normal, SIDE_EPSILON);

  Cm_FreeWinding(dw);
  Cm_FreeWinding(fw);

  if (w == NULL || w->num_points < 3) {
    if (w) {
      Cm_FreeWinding(w);
    }
    return false;
  }

  const color32_t color = Color_Color32(decal->color);
  const vec2_t atlas_min = decal->image->texcoords.xy;
  const vec2_t atlas_max = decal->image->texcoords.zw;
  const vec2_t atlas_size = Vec2_Subtract(atlas_max, atlas_min);

  for (int32_t i = 0; i < 4; i++) {
    const vec3_t pos = (i < w->num_points) ? w->points[i] : w->points[w->num_points - 1];
    out->vertexes[i].position = pos;

    const vec3_t delta = Vec3_Subtract(pos, org);
    const float x = (Vec3_Dot(delta, t) / r) * 0.5f + 0.5f;
    const float y = (Vec3_Dot(delta, b) / r) * 0.5f + 0.5f;

    out->vertexes[i].texcoord = Vec2_Add(atlas_min, Vec2(x * atlas_size.x, y * atlas_size.y));
    out->vertexes[i].color = color;
    out->vertexes[i].time = decal->time;
    out->vertexes[i].lifetime = decal->lifetime;
  }

  Cm_FreeWinding(w);
  return true;
}

/**
 * @brief Recurses down the tree to project the decal onto faces. Decal geometry is accumulated on
 * the containing `r_bsp_block_t` node.
 */
static void R_ClipDecalToNode(const r_view_t *view,
                              const r_bsp_node_t *node,
                              const r_decal_t *decal,
                              r_bsp_block_decals_t *decals) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_ClipDecalToNode(view, node->children[0], decal, decals);
    return;
  }

  if (dist < -decal->radius) {
    R_ClipDecalToNode(view, node->children[1], decal, decals);
    return;
  }

  // Project the decal onto this node's plane
  r_decal_t projected = *decal;
  
  // Project origin onto plane
  projected.origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  
  // Reduce radius using Pythagorean theorem (circle projection)
  projected.radius = sqrtf(decal->radius * decal->radius - dist * dist);

  const r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & SURF_SKY) {
      continue;
    }

    r_decal_vertexes_t vertexes;
    if (R_ClipDecalToFace(view, face, &projected, &vertexes)) {

      decals->image = (r_image_t *) decal->image;
      decals->vertexes = g_array_append_val(decals->vertexes, vertexes);

      decals->dirty = true;
    }
  }

  R_ClipDecalToNode(view, node->children[0], decal, decals);
  R_ClipDecalToNode(view, node->children[1], decal, decals);
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

    R_ClipDecalToNode(view, in->head_node, decal, &block->decals);
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

      // Iterate backwards to safely remove expired decal quads
      for (guint k = 0; k < decals->vertexes->len; k++) {
        r_decal_vertexes_t *v = &g_array_index(decals->vertexes, r_decal_vertexes_t, k);

        if (view->ticks - v->vertexes->time >= v->vertexes->lifetime) {
          decals->vertexes = g_array_remove_index(decals->vertexes, k);
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

      if (d->vertexes->len == 0) {
        continue;
      }

      if (d->dirty) {
        const GLsizei size = d->vertexes->len * sizeof(r_decal_vertexes_t);
        glBindBuffer(GL_ARRAY_BUFFER, d->vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, size, d->vertexes->data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        d->dirty = false;
      }

      glBindVertexArray(d->vertex_array);

      assert(d->image->texnum);
      glBindTexture(GL_TEXTURE_2D, d->image->texnum);

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
