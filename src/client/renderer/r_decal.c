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
 * @brief Clips a decal to a face and adds the resulting triangles to the face's block.
 */
static void R_ClipDecalToFace(const r_view_t *view,
                              const r_bsp_face_t *face,
                              const r_decal_t *decal,
                              r_bsp_block_decals_t *decals) {

  const vec3_t n = face->plane->cm->normal;
  const vec3_t sdir = face->brush_side->axis[0].xyz;
  const vec3_t tdir = face->brush_side->axis[1].xyz;

  vec3_t t, b;
  Vec3_Tangents(n, sdir, tdir, &t, &b);

  if (decal->rotation != 0.f) {
    const float cos_rot = cosf(decal->rotation);
    const float sin_rot = sinf(decal->rotation);
    const vec3_t t_rot = Vec3_Add(Vec3_Scale(t, cos_rot), Vec3_Scale(b, sin_rot));
    const vec3_t b_rot = Vec3_Add(Vec3_Scale(b, cos_rot), Vec3_Scale(t, -sin_rot));
    t = t_rot;
    b = b_rot;
  }

  const vec3_t org = decal->origin;
  const float r = decal->radius;
  const vec3_t positions[] = {
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b, -r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t,  r)), Vec3_Scale(b,  r)),
    Vec3_Add(Vec3_Add(org, Vec3_Scale(t, -r)), Vec3_Scale(b,  r)),
  };

  cm_winding_t *dw = Cm_AllocWinding(4);
  dw->num_points = 4;
  for (int32_t i = 0; i < dw->num_points; i++) {
    dw->points[i] = Vec3_Add(positions[i], n);
  }

  cm_winding_t *fw = Cm_AllocWinding(face->num_vertexes);
  fw->num_points = face->num_vertexes;
  for (int32_t i = 0; i < face->num_vertexes; i++) {
    fw->points[i] = face->vertexes[i].position;
  }

  cm_winding_t *w = Cm_ClipWindingToWinding(dw, fw, n, -1.f - ON_EPSILON);

  Cm_FreeWinding(dw);
  Cm_FreeWinding(fw);

  if (w == NULL || w->num_points < 3) {
    if (w) {
      Cm_FreeWinding(w);
    }
    return;
  }

  const color32_t color = Color_Color32(decal->color);
  const vec2_t atlas_min = decal->image->texcoords.xy;
  const vec2_t atlas_max = decal->image->texcoords.zw;
  const vec2_t atlas_size = Vec2_Subtract(atlas_max, atlas_min);

  const int32_t num_triangles = w->num_points - 2;
  
  for (int32_t i = 0; i < num_triangles; i++) {
    r_decal_triangle_t triangle;
    
    const int32_t indices[3] = { 0, i + 1, i + 2 };

    for (int32_t j = 0; j < 3; j++) {
      const vec3_t pos = w->points[indices[j]];
      triangle.vertexes[j].position = pos;

      const vec3_t delta = Vec3_Subtract(pos, org);
      const float x = (Vec3_Dot(delta, t) / r) * 0.5f + 0.5f;
      const float y = (Vec3_Dot(delta, b) / r) * 0.5f + 0.5f;

      triangle.vertexes[j].texcoord = Vec2_Add(atlas_min, Vec2(x * atlas_size.x, y * atlas_size.y));
      triangle.vertexes[j].color = color;
      triangle.vertexes[j].time = decal->time;
      triangle.vertexes[j].lifetime = decal->lifetime;
    }
    
    decals->image = (r_image_t *) decal->image;
    decals->triangles = g_array_append_val(decals->triangles, triangle);
  }

  decals->dirty = true;

  Cm_FreeWinding(w);
}

/**
 * @brief Recurses down the tree to project the decal onto faces.
 * @details Decal geometry is accumulated on the containing `r_bsp_block_t` node.
 */
static void R_ClipDecalToNode(const r_view_t *view,
                              const r_bsp_node_t *node,
                              const r_decal_t *decal) {

  if (node->contents > CONTENTS_NODE) {
    return;
  }

  const cm_bsp_plane_t *plane = node->plane->cm;
  const float dist = Cm_DistanceToPlane(decal->origin, plane);

  if (dist > decal->radius) {
    R_ClipDecalToNode(view, node->children[0], decal);
    return;
  }

  if (dist < -decal->radius) {
    R_ClipDecalToNode(view, node->children[1], decal);
    return;
  }

  r_decal_t projected = *decal;
  
  projected.origin = Vec3_Fmaf(decal->origin, -dist, plane->normal);
  projected.radius = sqrtf(decal->radius * decal->radius - dist * dist);

  const box3_t bounds = Box3_FromCenterRadius(projected.origin, projected.radius);

  const r_bsp_face_t *face = node->faces;
  for (int32_t i = 0; i < node->num_faces; i++, face++) {

    if (!(face->brush_side->contents & CONTENTS_MASK_SOLID)) {
      continue;
    }

    if (face->brush_side->surface & SURF_SKY) {
      continue;
    }

    if (Cm_DistanceToPlane(decal->origin, face->plane->cm) < -SIDE_EPSILON) {
      continue;
    }

    if (!Box3_Intersects(face->bounds, bounds)) {
      continue;
    }

    if (projected.radius >= 16.f) {
      const vec3_t center = Box3_Center(face->bounds);
      if (Cm_BoxTrace(decal->origin, center, Box3_Zero(), 0, CONTENTS_SOLID).fraction < 1.f) {
        continue;
      }
    }

    r_bsp_block_decals_t *decals = &face->block->decals;
    R_ClipDecalToFace(view, face, &projected, decals);
  }

  R_ClipDecalToNode(view, node->children[0], decal);
  R_ClipDecalToNode(view, node->children[1], decal);
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

      R_ClipDecalToNode(view, in->head_node, &d);
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

      for (guint k = 0; k < decals->triangles->len; k++) {
        r_decal_triangle_t *v = &g_array_index(decals->triangles, r_decal_triangle_t, k);

        if (view->ticks - v->vertexes->time >= v->vertexes->lifetime) {
          decals->triangles = g_array_remove_index(decals->triangles, k);
          decals->dirty = true;
        }
      }
    }
  }
}

/**
 * @brief Draws all decals in the view using the decal shader.
 */
void R_DrawDecals(const r_view_t *view) {

  glUseProgram(r_decal_program.name);

  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

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

      if (d->triangles->len == 0) {
        continue;
      }

      if (d->dirty) {
        const GLsizei size = d->triangles->len * sizeof(r_decal_triangle_t);
        glBindBuffer(GL_ARRAY_BUFFER, d->vertex_buffer);
        glBufferData(GL_ARRAY_BUFFER, size, d->triangles->data, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        d->dirty = false;
      }

      glBindVertexArray(d->vertex_array);

      assert(d->image->texnum);
      glBindTexture(GL_TEXTURE_2D, d->image->texnum);

      glDrawElements(GL_TRIANGLES, d->triangles->len * 3, GL_UNSIGNED_INT, NULL);

      r_stats.decals += d->triangles->len;
      r_stats.decal_draw_elements++;
    }
  }

  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

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
