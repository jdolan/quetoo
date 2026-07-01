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
 * @brief The BSP program.
 */
static struct {
  GLuint name;

  GLuint uniforms_block;
  GLuint lights_block;

  GLint active_lights;

  GLint model;

  GLint texture_material;
  GLint texture_stage;
  GLint texture_stage_next;
  GLint texture_warp;

  GLint texture_voxel_caustics;
  GLint texture_voxel_occlusion;

  GLint texture_sky;

  GLint texture_shadow_atlas;

  GLint texture_voxel_light_data;
  GLint texture_voxel_light_indices;


  struct {
    GLint surface;
    GLint alpha_test;
    GLint roughness;
    GLint hardness;
    GLint specularity;
    GLint parallax;
    GLint shadow;
  } material;

  struct {
    GLint flags;
    GLint color;
    GLint pulse;
    GLint drift;
    GLint st_origin;
    GLint stretch;
    GLint rotate;
    GLint scroll;
    GLint scale;
    GLint terrain;
    GLint dirtmap;
    GLint warp;
    GLint lighting;
    GLint emissive;
    GLint lerp;
  } stage;

  r_image_t *warp_image;
} r_bsp_program;

static float R_StageDriftHash(const void *a, const void *b) {
  uint32_t h = (uint32_t) ((uintptr_t) a >> 4) ^ (uint32_t) ((uintptr_t) b >> 4);
  h ^= h >> 16;
  h *= 0x7feb352dU;
  h ^= h >> 15;
  h *= 0x846ca68bU;
  h ^= h >> 16;
  return h / (float) UINT32_MAX;
}

/**
 * @brief Draws per-vertex normal, tangent, and bitangent lines for nearby BSP vertices when enabled.
 */
static void R_DrawBspNormals(const r_view_t *view, const r_bsp_model_t *bsp) {

  if (!r_draw_bsp_normals->value) {
    return;
  }

  const r_bsp_vertex_t *v = bsp->vertexes;
  for (int32_t i = 0; i < bsp->num_vertexes; i++, v++) {

    const vec3_t pos = v->position;
    if (Vec3_Distance(pos, view->origin) > 512.f) {
      continue;
    }

    const vec3_t normal[] = { pos, Vec3_Fmaf(pos, 8.f, v->normal) };
    const vec3_t tangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->tangent) };
    const vec3_t bitangent[] = { pos, Vec3_Fmaf(pos, 8.f, v->bitangent) };

    R_Draw3DLines(GL_LINES, normal, 2, color_red, true);

    if (r_draw_bsp_normals->integer > 1) {
      R_Draw3DLines(GL_LINES, tangent, 2, color_green, true);

      if (r_draw_bsp_normals->integer > 2) {
        R_Draw3DLines(GL_LINES, bitangent, 2, color_blue, true);
      }
    }
  }
}

/**
 * @brief Debug visualization for voxels and lights
 */
static void R_DrawBspVoxels(const r_view_t *view, const r_bsp_model_t *bsp) {

  if (!r_draw_bsp_voxels->value) {
    return;
  }

  if (!bsp->voxels.voxels) {
    return;
  }

  const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
  const cm_trace_t tr = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_SOLID);

  if (tr.fraction < 1.0f) {

    const r_bsp_voxels_t *voxels = &bsp->voxels;

    const vec3_t pos = Vec3_Subtract(tr.end, voxels->bounds.mins);
    const vec3_t xyz = Vec3_Scale(pos, 1.0f / BSP_VOXEL_SIZE);

    const int32_t x = Clampf((int32_t) floorf(xyz.x), 0, (int32_t) voxels->size.x - 1);
    const int32_t y = Clampf((int32_t) floorf(xyz.y), 0, (int32_t) voxels->size.y - 1);
    const int32_t z = Clampf((int32_t) floorf(xyz.z), 0, (int32_t) voxels->size.z - 1);

    const vec3_t voxel_pos = Vec3(x + 0.5f, y + 0.5f, z + 0.5f);
    const vec3_t voxel_world = Vec3_Fmaf(voxels->bounds.mins, BSP_VOXEL_SIZE, voxel_pos);

    const box3_t voxel_box = Box3_FromCenterRadius(voxel_world, BSP_VOXEL_SIZE * 0.5f);
    R_Draw3DBox(voxel_box, Color3f(0.f, 1.f, 1.f), false);

    const int32_t index = x + y * voxels->size.x + z * voxels->size.x * voxels->size.y;

    if (index >= 0 && index < voxels->num_voxels) {
      const r_bsp_voxel_t *voxel = &voxels->voxels[index];

      for (int32_t i = 0; i < voxel->num_lights; i++) {
        const r_bsp_light_t *light = voxel->lights[i];
        const color_t color = Color3fv(light->color);

        const vec3_t line_points[2] = { voxel_world, light->origin };
        R_Draw3DLines(GL_LINES, line_points, 2, color, true);
      }
    }
  }
}

/**
 * @brief Draws a single material stage pass for a BSP draw elements batch.
 */
static void R_DrawBspDrawElementsMaterialStage(const r_view_t *view,
                                               const r_entity_t *entity,
                                               const r_bsp_draw_elements_t *draw,
                                               const r_material_t *material,
                                               const r_stage_t *stage) {

  glUniform1i(r_bsp_program.stage.flags, stage->cm->flags);

  if (stage->cm->flags & STAGE_COLOR) {
    glUniform4fv(r_bsp_program.stage.color, 1, stage->cm->color.rgba);
  }

  if (stage->cm->flags & STAGE_PULSE) {
    glUniform1f(r_bsp_program.stage.pulse, stage->cm->pulse.hz);
    glUniform1f(r_bsp_program.stage.drift, stage->cm->pulse.drift * R_StageDriftHash(entity ? (const void *) entity : (const void *) draw, stage));
  }

  if (stage->cm->flags & (STAGE_STRETCH | STAGE_ROTATE)) {
    glUniform2fv(r_bsp_program.stage.st_origin, 1, draw->st_origin.xy);
  }

  if (stage->cm->flags & STAGE_STRETCH) {
    glUniform2f(r_bsp_program.stage.stretch, stage->cm->stretch.amplitude, stage->cm->stretch.hz);
  }

  if (stage->cm->flags & STAGE_ROTATE) {
    glUniform1f(r_bsp_program.stage.rotate, stage->cm->rotate.hz);
  }

  if (stage->cm->flags & (STAGE_SCROLL_S | STAGE_SCROLL_T)) {
    glUniform2f(r_bsp_program.stage.scroll, stage->cm->scroll.s, stage->cm->scroll.t);
  }

  if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
    glUniform2f(r_bsp_program.stage.scale, stage->cm->scale.s, stage->cm->scale.t);
  }

  if (stage->cm->flags & STAGE_TERRAIN) {
    glUniform2f(r_bsp_program.stage.terrain, stage->cm->terrain.floor, stage->cm->terrain.ceil);
  }

  if (stage->cm->flags & STAGE_DIRTMAP) {
    glUniform1f(r_bsp_program.stage.dirtmap, stage->cm->dirtmap.intensity);
  }

  if (stage->cm->flags & STAGE_WARP) {
    glUniform2f(r_bsp_program.stage.warp, stage->cm->warp.hz, stage->cm->warp.amplitude);
  }

  if (stage->cm->flags & STAGE_LIGHTING) {
    glUniform1f(r_bsp_program.stage.lighting, stage->cm->lighting.intensity);
  }

  if (stage->cm->flags & STAGE_EMISSIVE) {
    glUniform1f(r_bsp_program.stage.emissive, stage->cm->emissive);
  }

  glBlendFunc(stage->cm->blend.src, stage->cm->blend.dest);

  if (stage->media) {
    switch (stage->media->type) {
      case R_MEDIA_IMAGE:
      case R_MEDIA_ATLAS_IMAGE: {
        const r_image_t *image = (r_image_t *) stage->media;
        glBindTexture(GL_TEXTURE_2D, image->texnum);
      }
        break;
      case R_MEDIA_ANIMATION: {
        const r_animation_t *animation = (r_animation_t *) stage->media;
        int32_t frame;
        float lerp_frac = 0.f;
        if (stage->cm->animation.fps == 0.f && entity != NULL) {
          frame = entity->frame;
          if (stage->cm->flags & STAGE_ANIM_LERP) {
            lerp_frac = entity->lerp;
          }
        } else {
          const float drift = stage->cm->animation.drift * R_StageDriftHash(entity ? (const void *) entity : (const void *) draw, stage);
          const float frame_f = (view->ticks / 1000.f + drift) * stage->cm->animation.fps;
          frame = (int32_t) frame_f;
          if (stage->cm->flags & STAGE_ANIM_LERP) {
            lerp_frac = frame_f - floorf(frame_f);
          }
        }
        glBindTexture(GL_TEXTURE_2D, animation->frames[frame % animation->num_frames]->texnum);
        if (stage->cm->flags & STAGE_ANIM_LERP) {
          glUniform1f(r_bsp_program.stage.lerp, lerp_frac);
          glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE_NEXT);
          glBindTexture(GL_TEXTURE_2D, animation->frames[(frame + 1) % animation->num_frames]->texnum);
          glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);
        }
      }
        break;
      case R_MEDIA_MATERIAL: {
        const r_material_t *material = (r_material_t *) stage->media;
        glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);
        glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);
        glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);
      }
        break;
      default:
        break;
    }
  }

  glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);

  R_GetError(material->media.name);
}

/**
 * @brief Draws all active material stages for a BSP draw elements batch.
 */
static void R_DrawBspDrawElementsMaterialStages(const r_view_t *view,
                                                const r_entity_t *entity,
                                                const r_bsp_draw_elements_t *draw,
                                                const r_material_t *material) {

  if (!r_draw_material_stages->value) {
    return;
  }

  if (!(material->cm->stage_flags & STAGE_DRAW)) {
    return;
  }

  if (draw->surface & SURF_MASK_BLEND) {
    glBlendFunc(GL_ONE, GL_ZERO);
  } else {
    glEnable(GL_BLEND);
  }

  glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

  for (r_stage_t *stage = material->stages; stage; stage = stage->next) {

    if (!(stage->cm->flags & STAGE_DRAW)) {
      continue;
    }

    R_DrawBspDrawElementsMaterialStage(view, entity, draw, material, stage);
  }

  glUniform1i(r_bsp_program.stage.flags, STAGE_NONE);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

  if (draw->surface & SURF_MASK_BLEND) {
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  } else {
    glDisable(GL_BLEND);
  }

  R_GetError(NULL);
}

/**
 * @brief The last bound material, to avoid redundant state changes.
 */
static const r_material_t *r_bsp_bound_material;

/**
 * @brief Draws the specified draw elements for the given entity.
 * @param entity The entity, or `NULL` for the world model.
 * @param draw The draw elements command.
 */
static inline void R_DrawBspDrawElements(const r_view_t *view,
                     const r_entity_t *entity,
                     const r_bsp_draw_elements_t *draw) {

  if (draw->material != r_bsp_bound_material) {
    r_bsp_bound_material = draw->material;

    glBindTexture(GL_TEXTURE_2D_ARRAY, draw->material->texture->texnum);

    glUniform1f(r_bsp_program.material.alpha_test, draw->material->cm->alpha_test * r_alpha_test->value);
    glUniform1f(r_bsp_program.material.roughness, draw->material->cm->roughness * r_roughness->value);
    glUniform1f(r_bsp_program.material.hardness, draw->material->cm->hardness * r_hardness->value);
    glUniform1f(r_bsp_program.material.specularity, draw->material->cm->specularity * r_specularity->value);
    glUniform1f(r_bsp_program.material.parallax, draw->material->cm->parallax * r_parallax->value);
    glUniform1f(r_bsp_program.material.shadow, draw->material->cm->shadow * r_parallax_shadow->value);
  }

  glUniform1i(r_bsp_program.material.surface, draw->surface);

  if (!(draw->surface & SURF_MATERIAL)) {

    glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
    r_stats.bsp_triangles += draw->num_elements / 3;

    R_GetError(draw->material->media.name);
  }

  R_DrawBspDrawElementsMaterialStages(view, entity, draw, draw->material);

  r_stats.bsp_draw_elements++;
}

/**
 * @brief Draws all opaque surfaces for a BSP inline model entity.
 */
static void R_DrawOpaqueBspEntity(const r_view_t *view, const r_entity_t *entity) {

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, r_bsp_program.active_lights);
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        r_stats.blocks_occluded++;
        continue;
      }

      r_stats.blocks_visible++;

      R_ActiveLights(view, block->node->visible_bounds, r_bsp_program.active_lights);
    }

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
        continue;
      }

      R_DrawBspDrawElements(view, entity, draw);
    }
  }

  r_stats.bsp_inline_models++;
}

/**
 * @brief Draws all opaque BSP inline model entities for the current view, including the world.
 */
void R_DrawOpaqueBspEntities(const r_view_t *view) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  R_DrawSky(view, bsp);

  glUseProgram(r_bsp_program.name);

  glBindVertexArray(bsp->vertex_array);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

  r_bsp_bound_material = NULL;

  glUniform1i(r_bsp_program.stage.flags, STAGE_NONE);

  glEnable(GL_CULL_FACE);

  if (r_draw_wireframe->integer != 2) {
    glEnable(GL_DEPTH_TEST);
  }

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {

    if (IS_BSP_INLINE_MODEL(e->model)) {

      if (R_CullEntity(view, e)) {
        continue;
      }

      glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, e->matrix.array);

      R_DrawOpaqueBspEntity(view, e);
    }
  }

  glDisable(GL_CULL_FACE);

  if (r_draw_wireframe->integer != 2) {
    glDisable(GL_DEPTH_TEST);
  }

  glBindVertexArray(0);

  glUseProgram(0);

  R_GetError(NULL);

  R_DrawBspNormals(view, bsp);

  R_DrawBspVoxels(view, bsp);
}

/**
 * @brief Draws all blended (translucent) surfaces for a BSP inline model entity.
 */
static void R_DrawBlendBspEntity(const r_view_t *view, const r_entity_t *entity) {

  const r_bsp_inline_model_t *in = entity->model->bsp_inline;

  if (!IS_WORLDSPAWN(entity->model)) {
    R_ActiveLights(view, entity->abs_model_bounds, r_bsp_program.active_lights);
  }

  const r_bsp_block_t *block = in->blocks;
  for (int32_t i = 0; i < in->num_blocks; i++, block++) {

    if (!(block->surface & SURF_MASK_BLEND)) {
      continue;
    }

    if (IS_WORLDSPAWN(entity->model)) {

      if (block->query->result == 0) {
        continue;
      }

      R_ActiveLights(view, block->node->visible_bounds, r_bsp_program.active_lights);
    }

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (!(draw->surface & SURF_MASK_BLEND)) {
        continue;
      }

      R_DrawBspDrawElements(view, entity, draw);
    }
  }
}

/**
 * @brief Draws all BSP inline model entities for the current view, including the world.
 */
void R_DrawBlendBspEntities(const r_view_t *view) {
  const r_bsp_model_t *bsp = r_models.world->bsp;

  glUseProgram(r_bsp_program.name);

  glBindVertexArray(bsp->vertex_array);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

  r_bsp_bound_material = NULL;

  glUniform1i(r_bsp_program.stage.flags, STAGE_NONE);

  glEnable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);

  glDepthMask(GL_FALSE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  const r_entity_t *e = view->entities;
  for (int32_t i = 0; i < view->num_entities; i++, e++) {
    if (IS_BSP_INLINE_MODEL(e->model)) {

      if (R_CullEntity(view, e)) {
        continue;
      }

      glUniformMatrix4fv(r_bsp_program.model, 1, GL_FALSE, e->matrix.array);

      R_DrawBlendBspEntity(view, e);
    }
  }

  glBlendFunc(GL_ONE, GL_ZERO);
  glDisable(GL_BLEND);

  glDepthMask(GL_TRUE);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  glBindVertexArray(0);

  glUseProgram(0);

  R_GetError(NULL);
}

#define WARP_IMAGE_SIZE 16

/**
 * @brief Compiles and links the BSP GLSL program, binding all uniforms and buffer blocks.
 */
void R_InitBspProgram(void) {

  memset(&r_bsp_program, 0, sizeof(r_bsp_program));

  r_bsp_program.name = R_LoadProgram(
      R_ShaderDescriptor(GL_VERTEX_SHADER, "material.glsl", "voxel.glsl", "light.glsl", "bsp_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "material.glsl", "voxel.glsl", "light.glsl", "bsp_fs.glsl", NULL),
      NULL);

  glUseProgram(r_bsp_program.name);

  r_bsp_program.uniforms_block = glGetUniformBlockIndex(r_bsp_program.name, "uniforms_block");
  glUniformBlockBinding(r_bsp_program.name, r_bsp_program.uniforms_block, 0);

  r_bsp_program.lights_block = glGetUniformBlockIndex(r_bsp_program.name, "lights_block");
  glUniformBlockBinding(r_bsp_program.name, r_bsp_program.lights_block, 1);

  r_bsp_program.active_lights = glGetUniformLocation(r_bsp_program.name, "active_lights");

  r_bsp_program.model = glGetUniformLocation(r_bsp_program.name, "model");

  r_bsp_program.texture_material = glGetUniformLocation(r_bsp_program.name, "texture_material");
  r_bsp_program.texture_stage = glGetUniformLocation(r_bsp_program.name, "texture_stage");
  r_bsp_program.texture_stage_next = glGetUniformLocation(r_bsp_program.name, "texture_stage_next");
  r_bsp_program.texture_warp = glGetUniformLocation(r_bsp_program.name, "texture_warp");

  r_bsp_program.texture_voxel_caustics = glGetUniformLocation(r_bsp_program.name, "texture_voxel_caustics");
  r_bsp_program.texture_voxel_occlusion = glGetUniformLocation(r_bsp_program.name, "texture_voxel_occlusion");

  r_bsp_program.texture_sky = glGetUniformLocation(r_bsp_program.name, "texture_sky");

  r_bsp_program.texture_shadow_atlas = glGetUniformLocation(r_bsp_program.name, "texture_shadow_atlas");

  r_bsp_program.texture_voxel_light_data = glGetUniformLocation(r_bsp_program.name, "texture_voxel_light_data");
  r_bsp_program.texture_voxel_light_indices = glGetUniformLocation(r_bsp_program.name, "texture_voxel_light_indices");

  r_bsp_program.material.surface = glGetUniformLocation(r_bsp_program.name, "material.surface");
  r_bsp_program.material.alpha_test = glGetUniformLocation(r_bsp_program.name, "material.alpha_test");
  r_bsp_program.material.roughness = glGetUniformLocation(r_bsp_program.name, "material.roughness");
  r_bsp_program.material.hardness = glGetUniformLocation(r_bsp_program.name, "material.hardness");
  r_bsp_program.material.specularity = glGetUniformLocation(r_bsp_program.name, "material.specularity");
  r_bsp_program.material.parallax = glGetUniformLocation(r_bsp_program.name, "material.parallax");
  r_bsp_program.material.shadow = glGetUniformLocation(r_bsp_program.name, "material.shadow");

  r_bsp_program.stage.flags = glGetUniformLocation(r_bsp_program.name, "stage.flags");
  r_bsp_program.stage.color = glGetUniformLocation(r_bsp_program.name, "stage.color");
  r_bsp_program.stage.pulse = glGetUniformLocation(r_bsp_program.name, "stage.pulse");
  r_bsp_program.stage.drift = glGetUniformLocation(r_bsp_program.name, "stage.drift");
  r_bsp_program.stage.st_origin = glGetUniformLocation(r_bsp_program.name, "stage.st_origin");
  r_bsp_program.stage.stretch = glGetUniformLocation(r_bsp_program.name, "stage.stretch");
  r_bsp_program.stage.rotate = glGetUniformLocation(r_bsp_program.name, "stage.rotate");
  r_bsp_program.stage.scroll = glGetUniformLocation(r_bsp_program.name, "stage.scroll");
  r_bsp_program.stage.scale = glGetUniformLocation(r_bsp_program.name, "stage.scale");
  r_bsp_program.stage.terrain = glGetUniformLocation(r_bsp_program.name, "stage.terrain");
  r_bsp_program.stage.dirtmap = glGetUniformLocation(r_bsp_program.name, "stage.dirtmap");
  r_bsp_program.stage.warp = glGetUniformLocation(r_bsp_program.name, "stage.warp");
  r_bsp_program.stage.lighting = glGetUniformLocation(r_bsp_program.name, "stage.lighting");
  r_bsp_program.stage.emissive = glGetUniformLocation(r_bsp_program.name, "stage.emissive");
  r_bsp_program.stage.lerp = glGetUniformLocation(r_bsp_program.name, "stage.lerp");

  glUniform1i(r_bsp_program.texture_material, TEXTURE_MATERIAL);
  glUniform1i(r_bsp_program.texture_stage, TEXTURE_STAGE);
  glUniform1i(r_bsp_program.texture_stage_next, TEXTURE_STAGE_NEXT);
  glUniform1i(r_bsp_program.texture_warp, TEXTURE_WARP);

  glUniform1i(r_bsp_program.texture_voxel_caustics, TEXTURE_VOXEL_CAUSTICS);
  glUniform1i(r_bsp_program.texture_voxel_occlusion, TEXTURE_VOXEL_OCCLUSION);

  glUniform1i(r_bsp_program.texture_sky, TEXTURE_SKY);

  glUniform1i(r_bsp_program.texture_shadow_atlas, TEXTURE_SHADOW_ATLAS);

  glUniform1i(r_bsp_program.texture_voxel_light_data, TEXTURE_VOXEL_LIGHT_DATA);
  glUniform1i(r_bsp_program.texture_voxel_light_indices, TEXTURE_VOXEL_LIGHT_INDICES);

  r_bsp_program.warp_image = (r_image_t *) R_AllocMedia("r_warp_image", sizeof(r_image_t), R_MEDIA_IMAGE);
  r_bsp_program.warp_image->media.Retain = R_RetainImage;
  r_bsp_program.warp_image->media.Free = R_FreeImage;

  r_bsp_program.warp_image->width = r_bsp_program.warp_image->height = WARP_IMAGE_SIZE;
  r_bsp_program.warp_image->type = IMG_PROGRAM;
  r_bsp_program.warp_image->target = GL_TEXTURE_2D;
  r_bsp_program.warp_image->internal_format = GL_RGBA8;
  r_bsp_program.warp_image->format = GL_RGBA;
  r_bsp_program.warp_image->pixel_type = GL_UNSIGNED_BYTE;
  r_bsp_program.warp_image->minify = GL_LINEAR_MIPMAP_LINEAR;
  r_bsp_program.warp_image->magnify = GL_LINEAR;

  byte data[WARP_IMAGE_SIZE][WARP_IMAGE_SIZE][4];

  for (int32_t i = 0; i < WARP_IMAGE_SIZE; i++) {
    for (int32_t j = 0; j < WARP_IMAGE_SIZE; j++) {
      data[i][j][0] = RandomRangeu(0, 48);
      data[i][j][1] = RandomRangeu(0, 48);
      data[i][j][2] = 0;
      data[i][j][3] = 255;
    }
  }

  glActiveTexture(GL_TEXTURE0 + TEXTURE_WARP);

  R_UploadImage(r_bsp_program.warp_image, (byte *) data);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  R_GetError(NULL);
}

/**
 * @brief Deletes the BSP GLSL program object.
 */
void R_ShutdownBspProgram(void) {

  glDeleteProgram(r_bsp_program.name);

  r_bsp_program.name = 0;
}

/**
 * @brief The bring-up BSP world program. Renders world geometry with the
 * material diffuse texture (world_vs/world_fs). This is the incremental first
 * materials step toward the full bsp_vs/bsp_fs (lighting, stages, voxels,
 * shadows).
 * @remarks TODO(#864): grow world_vs/world_fs toward the full bsp program.
 */
static GraphicsPipeline *r_world_pipeline;

/**
 * @brief The material sampler (trilinear, repeat).
 */
static Sampler *r_world_sampler;

/**
 * @brief Builds the bring-up world pipeline from the world_vs/world_fs shaders.
 */
void R_InitWorldPipeline(void) {

  Shader *vertexShader = $(r_device.device, loadShader, "shaders/world_vs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_VERTEX,
    .num_uniform_buffers = 2, // globals (binding 0) + locals/model (binding 1)
  });

  Shader *fragmentShader = $(r_device.device, loadShader, "shaders/world_fs", &(SDL_GPUShaderCreateInfo) {
    .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
    .num_samplers = 1, // texture_material (2D array)
  });

  const Framebuffer *framebuffer = r_device.device->framebuffer;

  SDL_GPUGraphicsPipelineCreateInfo info = GPU_GraphicsPipeline3D;
  info.vertex_shader = vertexShader->shader;
  info.fragment_shader = fragmentShader->shader;

  // Bring-up: draw all faces regardless of winding so geometry is guaranteed visible.
  info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;

  info.vertex_input_state = (SDL_GPUVertexInputState) {
    .vertex_buffer_descriptions = &(SDL_GPUVertexBufferDescription) {
      .slot = 0,
      .pitch = sizeof(r_bsp_vertex_t),
      .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
    },
    .num_vertex_buffers = 1,
    .vertex_attributes = (SDL_GPUVertexAttribute[]) {
      {
        .location = 0,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
        .offset = offsetof(r_bsp_vertex_t, position),
      },
      {
        .location = 4,
        .buffer_slot = 0,
        .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
        .offset = offsetof(r_bsp_vertex_t, diffusemap),
      },
    },
    .num_vertex_attributes = 2,
  };

  info.target_info = (SDL_GPUGraphicsPipelineTargetInfo) {
    .color_target_descriptions = &(SDL_GPUColorTargetDescription) {
      .format = framebuffer->colorFormats[0],
      .blend_state = GPU_BlendStateOpaque,
    },
    .num_color_targets = 1,
    .depth_stencil_format = framebuffer->depthFormat,
    .has_depth_stencil_target = true,
  };

  r_world_pipeline = $(r_device.device, createGraphicsPipeline, &info);

  release(vertexShader);
  release(fragmentShader);

  r_world_sampler = $(r_device.device, createSampler, &(SDL_GPUSamplerCreateInfo) {
    .min_filter = SDL_GPU_FILTER_LINEAR,
    .mag_filter = SDL_GPU_FILTER_LINEAR,
    .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
    .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
    .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
  });
}

/**
 * @brief Releases the bring-up world pipeline and sampler.
 */
void R_ShutdownWorldPipeline(void) {
  r_world_pipeline = release(r_world_pipeline);
  r_world_sampler = release(r_world_sampler);
}

/**
 * @brief Renders all opaque world BSP draw elements with their material diffuse
 * texture into the present framebuffer. No culling yet; the world model matrix
 * is identity.
 */
void R_DrawWorld(const r_view_t *view) {

  if (!r_models.world || !r_world_pipeline) {
    return;
  }

  CommandBuffer *commands = r_device.device->commandBuffer;
  if (!commands) {
    return;
  }

  const r_bsp_model_t *bsp = r_models.world->bsp;
  Framebuffer *framebuffer = r_device.device->framebuffer;

  const SDL_GPUColorTargetInfo color =
      $(framebuffer, colorTargetInfo, 0, SDL_GPU_LOADOP_LOAD, SDL_GPU_STOREOP_STORE, NULL);
  const SDL_GPUDepthStencilTargetInfo depth =
      $(framebuffer, depthTargetInfo, SDL_GPU_LOADOP_CLEAR, SDL_GPU_STOREOP_STORE, 1.f);

  RenderPass *pass = $(commands, beginRenderPass, &color, 1, &depth);

  $(pass, setViewport, &(SDL_GPUViewport) {
    .x = 0.f, .y = 0.f,
    .w = (float) framebuffer->size.w, .h = (float) framebuffer->size.h,
    .min_depth = 0.f, .max_depth = 1.f,
  });

  // Vertex uniform slot 0 = per-frame globals; slot 1 = per-draw locals (model).
  const mat4_t model = Mat4_Identity();
  $(commands, pushVertexUniformData, 0, &r_uniforms.block, sizeof(r_uniforms.block));
  $(commands, pushVertexUniformData, 1, model.array, sizeof(model));

  $(pass, bindPipeline, r_world_pipeline);
  $(pass, bindVertexBuffers, 0, &(SDL_GPUBufferBinding) { .buffer = bsp->vertex_buffer->buffer }, 1);
  $(pass, bindIndexBuffer, &(SDL_GPUBufferBinding) { .buffer = bsp->elements_buffer->buffer }, SDL_GPU_INDEXELEMENTSIZE_32BIT);

  const r_bsp_draw_elements_t *draw = bsp->draw_elements;
  for (int32_t i = 0; i < bsp->num_draw_elements; i++, draw++) {

    // TODO(#864): sky (cubemap) and blended surfaces need dedicated passes; skip for now.
    if (draw->surface & (SURF_SKY | SURF_MASK_BLEND)) {
      continue;
    }

    if (!draw->material || !draw->material->texture || !draw->material->texture->texture) {
      continue;
    }

    $(pass, bindFragmentSamplers, 0, &(SDL_GPUTextureSamplerBinding) {
      .texture = draw->material->texture->texture->texture,
      .sampler = r_world_sampler->sampler,
    }, 1);

    const Uint32 firstIndex = (Uint32) ((uintptr_t) draw->elements / sizeof(GLuint));

    $(pass, drawIndexedPrimitives, draw->num_elements, 1, firstIndex, 0, 0);

    r_stats.bsp_triangles += draw->num_elements / 3;
    r_stats.bsp_draw_elements++;
  }

  pass = release(pass);
}
