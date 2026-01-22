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
 * @brief The shadows.
 */
static struct {
  /**
   * @brief Each light source targets a layer in the cubemap array texture.
   */
  GLuint cubemap_array;

  /**
   * @brief The depth pass framebuffer.
   */
  GLuint framebuffer;
} r_shadow_textures;

/**
 * @brief
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

  while (e) {
    if (light->source == e->id) {
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
 * @brief
 */
static void R_DrawBspEntitiesShadow(const r_view_t *view, const r_light_t *light) {

  const r_bsp_model_t *bsp = r_models.world->bsp;
  glBindVertexArray(bsp->depth_pass.vertex_array);

  glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, Mat4_Identity().array);
  glUniform1f(r_shadow_program.lerp, 0.f);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (!IS_BSP_INLINE_MODEL(e->model)) {
      continue;
    }

    if (!Box3_Intersects(light->bounds, e->abs_model_bounds)) {
      continue;
    }

    const vec3_t to_mins = Vec3_Subtract(e->abs_model_bounds.mins, light->origin);
    const vec3_t to_maxs = Vec3_Subtract(e->abs_model_bounds.maxs, light->origin);
    
    const vec3_t shadow_mins_dir = Vec3_Normalize(to_mins);
    const vec3_t shadow_maxs_dir = Vec3_Normalize(to_maxs);
    
    const vec3_t shadow_mins_end = Vec3_Fmaf(light->origin, light->radius, shadow_mins_dir);
    const vec3_t shadow_maxs_end = Vec3_Fmaf(light->origin, light->radius, shadow_maxs_dir);
    
    box3_t shadow_bounds = e->abs_model_bounds;
    shadow_bounds = Box3_Append(shadow_bounds, shadow_mins_end);
    shadow_bounds = Box3_Append(shadow_bounds, shadow_maxs_end);

    if (R_CulludeBox(view, shadow_bounds)) {
      continue;
    }

    R_DrawBspEntityShadow(view, light, e);
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
 * @brief Draws mesh entity shadows for the specified light.
 * @return The number of mesh entities that were rendered.
 */
static int32_t R_DrawMeshEntitiesShadow(const r_view_t *view, const r_light_t *light) {

  glBindVertexArray(r_models.mesh.depth_pass.vertex_array);

  glBindBuffer(GL_ARRAY_BUFFER, r_models.mesh.vertex_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_models.mesh.elements_buffer);

  int32_t count = 0;

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

    const vec3_t to_mins = Vec3_Subtract(e->abs_model_bounds.mins, light->origin);
    const vec3_t to_maxs = Vec3_Subtract(e->abs_model_bounds.maxs, light->origin);
    
    const vec3_t shadow_mins_dir = Vec3_Normalize(to_mins);
    const vec3_t shadow_maxs_dir = Vec3_Normalize(to_maxs);
    
    const vec3_t shadow_mins_end = Vec3_Fmaf(light->origin, light->radius, shadow_mins_dir);
    const vec3_t shadow_maxs_end = Vec3_Fmaf(light->origin, light->radius, shadow_maxs_dir);
    
    box3_t shadow_bounds = e->abs_model_bounds;
    shadow_bounds = Box3_Append(shadow_bounds, shadow_mins_end);
    shadow_bounds = Box3_Append(shadow_bounds, shadow_maxs_end);

    if (R_CulludeBox(view, shadow_bounds)) {
      continue;
    }

    R_DrawMeshEntityShadow(view, light, e);
    count++;
  }

  glBindVertexArray(0);

  return count;
}

/**
 * @brief Renders the shadow cubemap for the specified light.
 */
static void R_DrawShadow(const r_view_t *view, const r_light_t *light) {

  if (light->shadow_cached && *light->shadow_cached) {
    return;
  }

  const GLint index = (GLint) (light - view->lights);

  glUniform1i(r_shadow_program.light_index, index);

  const vec3_t closest_point = Box3_ClampPoint(light->bounds, view->origin);
  const float dist = Vec3_Distance(closest_point, view->origin);

  int32_t mesh_entity_count = 0;

  for (GLint face = 0; face < 6; face++) {

    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                              GL_DEPTH_ATTACHMENT,
                              r_shadow_textures.cubemap_array,
                              0,
                              index * 6 + face);

    glClear(GL_DEPTH_BUFFER_BIT);

    glUniform1i(r_shadow_program.face_index, face);

    R_DrawBspEntitiesShadow(view, light);

    if (r_shadows->value && dist <= r_shadow_distance->value) {
      mesh_entity_count += R_DrawMeshEntitiesShadow(view, light);
    }
  }

  if (light->shadow_cached) {
    *light->shadow_cached = (mesh_entity_count == 0);
  }

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawShadows(const r_view_t *view) {

  glBindFramebuffer(GL_FRAMEBUFFER, r_shadow_textures.framebuffer);

  glUseProgram(r_shadow_program.name);

  const GLsizei size = r_shadow_cubemap_array_size->integer;
  glViewport(0, 0, size, size);

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

  glViewport(0, 0, r_context.pw, r_context.ph);

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
 * @brief
 */
static void R_InitShadowTextures(void) {

  const GLsizei size = r_shadow_cubemap_array_size->integer;

  glGenTextures(1, &r_shadow_textures.cubemap_array);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_SHADOW_CUBEMAP_ARRAY);
  glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, r_shadow_textures.cubemap_array);

  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
  glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

  glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, size, size, MAX_LIGHTS * 6, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowFramebuffer(void) {

  glGenFramebuffers(1, &r_shadow_textures.framebuffer);

  glBindFramebuffer(GL_FRAMEBUFFER, r_shadow_textures.framebuffer);

  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);

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

  glDeleteTextures(1, &r_shadow_textures.cubemap_array);

  r_shadow_textures.cubemap_array = 0;

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowFramebuffer(void) {

  glDeleteFramebuffers(1, &r_shadow_textures.framebuffer);

  r_shadow_textures.framebuffer = 0;

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
