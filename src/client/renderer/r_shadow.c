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
 * @brief The shadow program.
 */
static struct {
  GLuint name;

  /**
   * @brief The uniform blocks.
   */
  GLuint uniforms_block;
  GLuint lights_block;

  /**
   * @brief The model matrix.
   */
  GLint model;

  /**
   * @brief The frame interpolation fraction.
   */
  GLint lerp;

  /**
   * @brief The light view matrices for 6-pass rendering without geometry shader.
   */
  GLint light_view;

  /**
   * @brief The light index.
   */
  GLint light_index;

  /**
   * @brief The face index.
   */
  GLint face_index;
} r_shadow_program;

/**
 * @brief The shadow atlas.
 */
static struct {
  /**
   * @brief The 2D depth texture atlas.
   */
  GLuint texture;

  /**
   * @brief The depth pass framebuffer.
   */
  GLuint framebuffer;

  /**
   * @brief The atlas dimensions in pixels.
   */
  GLsizei width;
  GLsizei height;

  /**
   * @brief The tile size in pixels.
   */
  GLsizei tile_size;

  /**
   * @brief The number of lights per row in the atlas grid.
   */
  int32_t lights_per_row;

  /**
   * @brief The maximum number of lights that fit in the atlas.
   */
  int32_t max_lights;

  /**
   * @brief Frame counter for temporal face amortization.
   */
  uint32_t frame_count;
} r_shadow_atlas;

/**
 * @brief Pre-culled entities for shadow rendering.
 * @details Populated once per light to avoid redundant culling tests across 6 cubemap faces.
 */
static struct {
  const r_entity_t *bsp_entities[MAX_ENTITIES];
  int32_t num_bsp_entities;

  const r_entity_t *mesh_entities[MAX_ENTITIES];
  int32_t num_mesh_entities;
} r_shadow_entities;

/**
 * @brief Computes the atlas tile origin for a given light index and face.
 */
static void R_ShadowAtlasTile(int32_t light_index, int32_t face, GLint *x, GLint *y) {

  const int32_t light_col = light_index % r_shadow_atlas.lights_per_row;
  const int32_t light_row = light_index / r_shadow_atlas.lights_per_row;
  const int32_t face_col = face % 3;
  const int32_t face_row = face / 3;
  const GLsizei ts = r_shadow_atlas.tile_size;

  *x = (light_col * 3 + face_col) * ts;
  *y = (light_row * 2 + face_row) * ts;
}

/**
 * @brief
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

  while (e) {
    if (light->source && light->source == e->id) {
      return true;
    }
    e = e->parent;
  }

  return false;
}

/**
 * @brief
 */
static void R_DrawBspEntityShadow(const r_view_t *view, const r_light_t *light, const r_entity_t *e) {

  const r_bsp_inline_model_t *in = e->model->bsp_inline;

  glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, e->matrix.array);

  if (light->bsp_light && in == r_models.world->bsp->inline_models) {
    glDrawElements(GL_TRIANGLES, light->bsp_light->num_depth_pass_elements, GL_UNSIGNED_INT, light->bsp_light->depth_pass_elements);
  } else {
    glDrawElements(GL_TRIANGLES, in->num_depth_pass_elements, GL_UNSIGNED_INT, in->depth_pass_elements);
  }
}

/**
 * @brief Culls BSP entities for shadow rendering against the specified light.
 * @details Performs all culling tests once and builds a list of visible entities.
 * This avoids redundant testing across all 6 cubemap faces.
 */
static void R_CullBspEntitiesForShadow(const r_view_t *view, const r_light_t *light) {

  r_shadow_entities.num_bsp_entities = 0;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (Box3_IsNull(e->abs_model_bounds)) {
      continue;
    }

    if (!Box3_Intersects(light->bounds, e->abs_model_bounds)) {
      continue;
    }

    vec3_t corners[8];
    Box3_ToPoints(e->abs_model_bounds, corners);

    box3_t shadow_bounds = e->abs_model_bounds;
    for (int32_t j = 0; j < 8; j++) {
      const vec3_t to_corner = Vec3_Subtract(corners[j], light->origin);
      const vec3_t dir = Vec3_Normalize(to_corner);
      shadow_bounds = Box3_Append(shadow_bounds, Vec3_Fmaf(light->origin, light->radius, dir));
    }

    shadow_bounds = Box3_Expand(shadow_bounds, 32.f);

    if (R_CulludeBox(view, shadow_bounds)) {
      continue;
    }

    r_shadow_entities.bsp_entities[r_shadow_entities.num_bsp_entities++] = e;
  }
}

/**
 * @brief Draws BSP entity shadows for the specified light.
 * @details Iterates the pre-culled list built by R_CullBspEntitiesForShadow().
 */
static void R_DrawBspEntitiesShadow(const r_view_t *view, const r_light_t *light) {

  const r_bsp_model_t *bsp = r_models.world->bsp;
  glBindVertexArray(bsp->depth_pass.vertex_array);

  glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, Mat4_Identity().array);
  glUniform1f(r_shadow_program.lerp, 0.f);

  for (int32_t i = 0; i < r_shadow_entities.num_bsp_entities; i++) {
    R_DrawBspEntityShadow(view, light, r_shadow_entities.bsp_entities[i]);
  }

  glBindVertexArray(0);
}

/**
 * @brief
 */
static void R_DrawMeshFaceShadow(const r_entity_t *e, const r_mesh_model_t *mesh, const r_mesh_face_t *face) {

  const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
  const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) (frame_offset + offsetof(r_mesh_vertex_t, position)));

  glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->indices, face->base_vertex);
}

/**
 * @brief
 */
static void R_DrawMeshEntityShadow(const r_view_t *view, const r_light_t *light, const r_entity_t *e) {

  const r_mesh_model_t *mesh = e->model->mesh;
  assert(mesh);

  glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, e->matrix.array);
  glUniform1f(r_shadow_program.lerp, e->lerp);

  if (mesh->num_frames == 1) {
    glDrawElementsBaseVertex(GL_TRIANGLES, mesh->num_elements, GL_UNSIGNED_INT, mesh->indices, mesh->base_vertex);
  } else {

    const r_mesh_face_t *face = mesh->faces;
    for (int32_t i = 0; i < mesh->num_faces; i++, face++) {
      R_DrawMeshFaceShadow(e, mesh, face);
    }

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (GLvoid *) offsetof(r_mesh_vertex_t, position));
  }
}

/**
 * @brief Culls mesh entities for shadow rendering against the specified light.
 * @details Performs all culling tests once and builds a list of visible entities.
 * This avoids redundant testing across all 6 cubemap faces.
 */
static void R_CullMeshEntitiesForShadow(const r_view_t *view, const r_light_t *light) {

  r_shadow_entities.num_mesh_entities = 0;

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_MESH_MODEL(e->model)) {
      continue;
    }

    if (e->effects & (EF_NO_SHADOW | EF_BLEND)) {
      continue;
    }

    if (R_IsLightSource(light, e)) {
      continue;
    }

    if (!Box3_Intersects(light->bounds, e->abs_model_bounds)) {
      continue;
    }

    vec3_t corners[8];
    Box3_ToPoints(e->abs_model_bounds, corners);

    box3_t shadow_bounds = e->abs_model_bounds;
    for (int32_t j = 0; j < 8; j++) {
      const vec3_t to_corner = Vec3_Subtract(corners[j], light->origin);
      const vec3_t dir = Vec3_Normalize(to_corner);
      shadow_bounds = Box3_Append(shadow_bounds, Vec3_Fmaf(light->origin, light->radius, dir));
    }

    shadow_bounds = Box3_Expand(shadow_bounds, 32.f);

    if (R_CulludeBox(view, shadow_bounds)) {
      continue;
    }

    r_shadow_entities.mesh_entities[r_shadow_entities.num_mesh_entities++] = e;
  }
}

/**
 * @brief Draws mesh entity shadows for the specified light.
 * @details Iterates the pre-culled list built by R_CullMeshEntitiesForShadow().
 */
static void R_DrawMeshEntitiesShadow(const r_view_t *view, const r_light_t *light) {

  glBindVertexArray(r_models.mesh.depth_pass.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_models.mesh.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_models.mesh.elements_buffer);

  for (int32_t i = 0; i < r_shadow_entities.num_mesh_entities; i++) {
    R_DrawMeshEntityShadow(view, light, r_shadow_entities.mesh_entities[i]);
  }

  glBindVertexArray(0);
}

/**
 * @brief Renders the shadow atlas tiles for the specified light.
 */
static void R_DrawShadow(const r_view_t *view, const r_light_t *light) {

  const vec3_t closest_point = Box3_ClampPoint(light->bounds, view->origin);
  const float dist = Vec3_Distance(closest_point, view->origin);

  R_CullBspEntitiesForShadow(view, light);

  if (r_shadows->value && dist <= r_shadow_distance->value) {
    R_CullMeshEntitiesForShadow(view, light);
  } else {
    r_shadow_entities.num_mesh_entities = 0;
  }

  const bool is_shadow_cacheable = r_shadow_entities.num_bsp_entities <= 1
                                && r_shadow_entities.num_mesh_entities == 0;

  if (is_shadow_cacheable) {
    if (light->shadow_cached && *light->shadow_cached) {
      return;
    }
  }

  const GLint index = (GLint) (light - view->lights);

  if (index >= r_shadow_atlas.max_lights) {
    return;
  }

  glUniform1i(r_shadow_program.light_index, index);

  glEnable(GL_SCISSOR_TEST);

  // Cacheable lights render all 6 faces; uncacheable lights amortize
  // across 2 frames, rendering one row of the 3×2 tile block per frame
  GLint face_start, face_end;

  if (is_shadow_cacheable) {
    face_start = 0;
    face_end = 6;

    GLint block_x, block_y;
    R_ShadowAtlasTile(index, 0, &block_x, &block_y);
    glScissor(block_x, block_y, r_shadow_atlas.tile_size * 3, r_shadow_atlas.tile_size * 2);
    glClear(GL_DEPTH_BUFFER_BIT);
  } else {
    const GLint face_row = r_shadow_atlas.frame_count & 1;
    face_start = face_row * 3;
    face_end = face_start + 3;

    GLint row_x, row_y;
    R_ShadowAtlasTile(index, face_start, &row_x, &row_y);
    glScissor(row_x, row_y, r_shadow_atlas.tile_size * 3, r_shadow_atlas.tile_size);
    glClear(GL_DEPTH_BUFFER_BIT);
  }

  for (GLint face = face_start; face < face_end; face++) {

    GLint tile_x, tile_y;
    R_ShadowAtlasTile(index, face, &tile_x, &tile_y);

    glViewport(tile_x, tile_y, r_shadow_atlas.tile_size, r_shadow_atlas.tile_size);
    glScissor(tile_x, tile_y, r_shadow_atlas.tile_size, r_shadow_atlas.tile_size);

    glUniform1i(r_shadow_program.face_index, face);

    if (r_shadow_entities.num_bsp_entities > 0) {
      R_DrawBspEntitiesShadow(view, light);
    }

    if (r_shadow_entities.num_mesh_entities > 0) {
      R_DrawMeshEntitiesShadow(view, light);
    }
  }

  glDisable(GL_SCISSOR_TEST);

  if (light->shadow_cached) {
    *light->shadow_cached = is_shadow_cacheable;
  }

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawShadows(const r_view_t *view) {

  r_shadow_atlas.frame_count++;

  glBindFramebuffer(GL_FRAMEBUFFER, r_shadow_atlas.framebuffer);

  glUseProgram(r_shadow_program.name);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_DEPTH_CLAMP);

  glEnable(GL_CULL_FACE);
  glCullFace(GL_FRONT);

  const r_light_t *l = view->lights;
  for (int32_t i = 0; i < view->num_lights; i++, l++) {

    if (l->occluded) {
      continue;
    }

    R_DrawShadow(view, l);
  }

  glCullFace(GL_BACK);
  glDisable(GL_CULL_FACE);

  glDisable(GL_DEPTH_CLAMP);
  glDisable(GL_DEPTH_TEST);

  const SDL_Rect viewport = r_context.viewport;
  glViewport(viewport.x, viewport.y, viewport.w, viewport.h);

  glUseProgram(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  R_GetError(NULL);
}

/**
 * @brief Initializes the shadow program with optimized shaders (no geometry shader).
 */
static void R_InitShadowProgram(void) {

  memset(&r_shadow_program, 0, sizeof(r_shadow_program));

  r_shadow_program.name = R_LoadProgram(
        R_ShaderDescriptor(GL_VERTEX_SHADER, "shadow_vs.glsl", NULL),
        R_ShaderDescriptor(GL_FRAGMENT_SHADER, "shadow_fs.glsl", NULL),
        NULL);

  glUseProgram(r_shadow_program.name);

  r_shadow_program.uniforms_block = glGetUniformBlockIndex(r_shadow_program.name, "uniforms_block");
  glUniformBlockBinding(r_shadow_program.name, r_shadow_program.uniforms_block, 0);

  r_shadow_program.lights_block = glGetUniformBlockIndex(r_shadow_program.name, "lights_block");
  glUniformBlockBinding(r_shadow_program.name, r_shadow_program.lights_block, 1);

  r_shadow_program.model = glGetUniformLocation(r_shadow_program.name, "model");
  r_shadow_program.lerp = glGetUniformLocation(r_shadow_program.name, "lerp");
  r_shadow_program.light_view = glGetUniformLocation(r_shadow_program.name, "light_view");
  r_shadow_program.light_index = glGetUniformLocation(r_shadow_program.name, "light_index");
  r_shadow_program.face_index = glGetUniformLocation(r_shadow_program.name, "face_index");

  const mat4_t light_view[6] = {
    Mat4_LookAt(Vec3_Zero(), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f)),
    Mat4_LookAt(Vec3_Zero(), Vec3(-1.0,  0.0,  0.0), Vec3(0.0, -1.0,  0.0)),
    Mat4_LookAt(Vec3_Zero(), Vec3( 0.0,  1.0,  0.0), Vec3(0.0,  0.0,  1.0)),
    Mat4_LookAt(Vec3_Zero(), Vec3( 0.0, -1.0,  0.0), Vec3(0.0,  0.0, -1.0)),
    Mat4_LookAt(Vec3_Zero(), Vec3( 0.0,  0.0,  1.0), Vec3(0.0, -1.0,  0.0)),
    Mat4_LookAt(Vec3_Zero(), Vec3( 0.0,  0.0, -1.0), Vec3(0.0, -1.0,  0.0))
  };

  glUniformMatrix4fv(r_shadow_program.light_view, 6, false, (float *) light_view);

  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief Initializes the shadow atlas texture.
 * @details Computes atlas dimensions from tile_size and GL_MAX_TEXTURE_SIZE to fit MAX_LIGHTS.
 */
static void R_InitShadowTextures(void) {

  GLint max_texture_size;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture_size);

  r_shadow_atlas.tile_size = r_shadow_tile_size->integer;

  r_shadow_atlas.lights_per_row = max_texture_size / (3 * r_shadow_atlas.tile_size);
  if (r_shadow_atlas.lights_per_row < 1) {
    r_shadow_atlas.lights_per_row = 1;
  }

  const int32_t needed_rows = (MAX_LIGHTS + r_shadow_atlas.lights_per_row - 1) / r_shadow_atlas.lights_per_row;
  const int32_t max_rows = max_texture_size / (2 * r_shadow_atlas.tile_size);
  const int32_t lights_per_col = needed_rows < max_rows ? needed_rows : max_rows;

  r_shadow_atlas.max_lights = r_shadow_atlas.lights_per_row * lights_per_col;
  if (r_shadow_atlas.max_lights > MAX_LIGHTS) {
    r_shadow_atlas.max_lights = MAX_LIGHTS;
  }

  r_shadow_atlas.width = r_shadow_atlas.lights_per_row * 3 * r_shadow_atlas.tile_size;
  r_shadow_atlas.height = lights_per_col * 2 * r_shadow_atlas.tile_size;

  if (r_shadow_atlas.max_lights < MAX_LIGHTS) {
    Com_Warn("Shadow atlas supports %d of %d lights at tile size %d\n",
             r_shadow_atlas.max_lights, MAX_LIGHTS, r_shadow_atlas.tile_size);
  }

  Com_Print("  Shadow atlas: %dx%d (%d tiles of %d, %d lights max)\n",
            r_shadow_atlas.width, r_shadow_atlas.height,
            r_shadow_atlas.max_lights * 6, r_shadow_atlas.tile_size,
            r_shadow_atlas.max_lights);

  glGenTextures(1, &r_shadow_atlas.texture);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_SHADOW_ATLAS);
  glBindTexture(GL_TEXTURE_2D, r_shadow_atlas.texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, r_shadow_atlas.width, r_shadow_atlas.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowFramebuffer(void) {

  glGenFramebuffers(1, &r_shadow_atlas.framebuffer);

  glBindFramebuffer(GL_FRAMEBUFFER, r_shadow_atlas.framebuffer);

  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, r_shadow_atlas.texture, 0);

  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    Com_Error(ERROR_FATAL, "Shadow atlas framebuffer incomplete: %d\n", status);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitShadows(void) {

  R_InitShadowProgram();

  R_InitShadowTextures();

  R_InitShadowFramebuffer();
}

/**
 * @brief
 */
static void R_ShutdownShadowProgram(void) {

  glDeleteProgram(r_shadow_program.name);

  r_shadow_program.name = 0;

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowTexture(void) {

  glDeleteTextures(1, &r_shadow_atlas.texture);

  r_shadow_atlas.texture = 0;

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowFramebuffer(void) {

  glDeleteFramebuffers(1, &r_shadow_atlas.framebuffer);

  r_shadow_atlas.framebuffer = 0;

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownShadows(void) {

  R_ShutdownShadowProgram();

  R_ShutdownShadowTexture();

  R_ShutdownShadowFramebuffer();
}

/**
 * @brief Returns the shadow atlas layout metadata for use by R_UpdateLights.
 */
void R_ShadowAtlasInfo(int32_t *lights_per_row, int32_t *tile_size, int32_t *atlas_width, int32_t *atlas_height, int32_t *max_lights) {
  *lights_per_row = r_shadow_atlas.lights_per_row;
  *tile_size = r_shadow_atlas.tile_size;
  *atlas_width = r_shadow_atlas.width;
  *atlas_height = r_shadow_atlas.height;
  *max_lights = r_shadow_atlas.max_lights;
}
