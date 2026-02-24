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

#define SKY_DISTANCE (MAX_WORLD_COORD)

#define SKY_ST_EPSILON (0.00175)

#define SKY_ST_MIN   (0.0 + SKY_ST_EPSILON)
#define SKY_ST_MAX   (1.0 - SKY_ST_EPSILON)

/**
 * @brief
 */
static struct {
  /**
   * @brief The sky cubemap.
   */
  r_image_t *image;
} r_sky;

/**
 * @brief The sky program.
 */
static struct {
  GLuint name;

  GLuint uniforms_block;

  GLint texture_sky;
  GLint texture_stage;
  GLint texture_voxel_light_data;
  GLint texture_voxel_light_indices;

  GLint block;

  struct {
    GLint flags;
    GLint color;
    GLint pulse;
    GLint st_origin;
    GLint stretch;
    GLint rotate;
    GLint scroll;
    GLint scale;
    GLint terrain;
    GLint dirtmap;
    GLint warp;
  } stage;
} r_sky_program;

/**
 * @brief
 */
static void R_DrawSkyDrawElementsMaterialStage(const r_view_t *view,
                                               const r_bsp_draw_elements_t *draw,
                                               const r_stage_t *stage) {

  glUniform1i(r_sky_program.stage.flags, stage->cm->flags);

  if (stage->cm->flags & STAGE_COLOR) {
    glUniform4fv(r_sky_program.stage.color, 1, stage->cm->color.rgba);
  }

  if (stage->cm->flags & STAGE_PULSE) {
    glUniform1f(r_sky_program.stage.pulse, stage->cm->pulse.hz);
  }

  if (stage->cm->flags & (STAGE_STRETCH | STAGE_ROTATE)) {
    glUniform2fv(r_sky_program.stage.st_origin, 1, draw->st_origin.xy);
  }

  if (stage->cm->flags & STAGE_STRETCH) {
    glUniform2f(r_sky_program.stage.stretch, stage->cm->stretch.amplitude, stage->cm->stretch.hz);
  }

  if (stage->cm->flags & STAGE_ROTATE) {
    glUniform1f(r_sky_program.stage.rotate, stage->cm->rotate.hz);
  }

  if (stage->cm->flags & (STAGE_SCROLL_S | STAGE_SCROLL_T)) {
    glUniform2f(r_sky_program.stage.scroll, stage->cm->scroll.s, stage->cm->scroll.t);
  }

  if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
    glUniform2f(r_sky_program.stage.scale, stage->cm->scale.s, stage->cm->scale.t);
  }

  if (stage->cm->flags & STAGE_TERRAIN) {
    glUniform2f(r_sky_program.stage.terrain, stage->cm->terrain.floor, stage->cm->terrain.ceil);
  }

  if (stage->cm->flags & STAGE_DIRTMAP) {
    glUniform1f(r_sky_program.stage.dirtmap, stage->cm->dirtmap.intensity);
  }

  if (stage->cm->flags & STAGE_WARP) {
    glUniform2f(r_sky_program.stage.warp, stage->cm->warp.hz, stage->cm->warp.amplitude);
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
        const int32_t frame = view->ticks / 1000.f * stage->cm->animation.fps;
        glBindTexture(GL_TEXTURE_2D, animation->frames[frame % animation->num_frames]->texnum);
      }
        break;
      default:
        break;
    }
  }

  glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
}

/**
 * @brief
 */
void R_DrawSky(const r_view_t *view, const r_bsp_model_t *bsp) {

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glUseProgram(r_sky_program.name);

  glBindVertexArray(bsp->vertex_array);

  glUniform1i(r_sky_program.stage.flags, STAGE_MATERIAL);

  const r_bsp_block_t *block = bsp->inline_models->blocks;
  for (int32_t i = 0; i < bsp->inline_models->num_blocks; i++, block++) {

    if (block->query->result == 0) {
      continue;
    }

    glUniform1i(r_sky_program.block, block->flags);

    const r_bsp_draw_elements_t *draw = block->draw_elements;
    for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

      if (!(draw->surface & SURF_SKY)) {
        continue;
      }

      glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
    }
  }

  if (r_materials->value) {

    glEnable(GL_BLEND);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

    block = bsp->inline_models->blocks;
    for (int32_t i = 0; i < bsp->inline_models->num_blocks; i++, block++) {

      if (block->query->result == 0) {
        continue;
      }

      glUniform1i(r_sky_program.block, block->flags);

      const r_bsp_draw_elements_t *draw = block->draw_elements;
      for (int32_t j = 0; j < block->num_draw_elements; j++, draw++) {

        if (!(draw->surface & SURF_SKY)) {
          continue;
        }

        if (!draw->material || !(draw->material->cm->stage_flags & STAGE_DRAW)) {
          continue;
        }

        for (const r_stage_t *stage = draw->material->stages; stage; stage = stage->next) {

          if (!(stage->cm->flags & STAGE_DRAW)) {
            continue;
          }

          R_DrawSkyDrawElementsMaterialStage(view, draw, stage);
        }
      }
    }

    glUniform1i(r_sky_program.stage.flags, STAGE_MATERIAL);
    glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
    glDisable(GL_BLEND);
  }

  glBindVertexArray(0);

  glUseProgram(0);

  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitSkyProgram(void) {

  memset(&r_sky_program, 0, sizeof(r_sky_program));

  r_sky_program.name = R_LoadProgram(
      R_ShaderDescriptor(GL_VERTEX_SHADER, "material.glsl", "sky_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "material.glsl", "sky_fs.glsl", NULL),
      NULL);

  glUseProgram(r_sky_program.name);

  r_sky_program.uniforms_block = glGetUniformBlockIndex(r_sky_program.name, "uniforms_block");
  glUniformBlockBinding(r_sky_program.name, r_sky_program.uniforms_block, 0);

  const GLuint lights_block = glGetUniformBlockIndex(r_sky_program.name, "lights_block");
  glUniformBlockBinding(r_sky_program.name, lights_block, 1);

  r_sky_program.texture_sky = glGetUniformLocation(r_sky_program.name, "texture_sky");
  r_sky_program.texture_stage = glGetUniformLocation(r_sky_program.name, "texture_stage");
  r_sky_program.texture_voxel_light_data = glGetUniformLocation(r_sky_program.name, "texture_voxel_light_data");
  r_sky_program.texture_voxel_light_indices = glGetUniformLocation(r_sky_program.name, "texture_voxel_light_indices");

  glUniform1i(r_sky_program.texture_sky, TEXTURE_SKY);
  glUniform1i(r_sky_program.texture_stage, TEXTURE_STAGE);
  glUniform1i(r_sky_program.texture_voxel_light_data, TEXTURE_VOXEL_LIGHT_DATA);
  glUniform1i(r_sky_program.texture_voxel_light_indices, TEXTURE_VOXEL_LIGHT_INDICES);

  r_sky_program.block = glGetUniformLocation(r_sky_program.name, "block");

  r_sky_program.stage.flags = glGetUniformLocation(r_sky_program.name, "stage.flags");
  r_sky_program.stage.color = glGetUniformLocation(r_sky_program.name, "stage.color");
  r_sky_program.stage.pulse = glGetUniformLocation(r_sky_program.name, "stage.pulse");
  r_sky_program.stage.st_origin = glGetUniformLocation(r_sky_program.name, "stage.st_origin");
  r_sky_program.stage.stretch = glGetUniformLocation(r_sky_program.name, "stage.stretch");
  r_sky_program.stage.rotate = glGetUniformLocation(r_sky_program.name, "stage.rotate");
  r_sky_program.stage.scroll = glGetUniformLocation(r_sky_program.name, "stage.scroll");
  r_sky_program.stage.scale = glGetUniformLocation(r_sky_program.name, "stage.scale");
  r_sky_program.stage.terrain = glGetUniformLocation(r_sky_program.name, "stage.terrain");
  r_sky_program.stage.dirtmap = glGetUniformLocation(r_sky_program.name, "stage.dirtmap");
  r_sky_program.stage.warp = glGetUniformLocation(r_sky_program.name, "stage.warp");

  glUniform1i(r_sky_program.stage.flags, STAGE_MATERIAL);

  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitSky(void) {

  memset(&r_sky, 0, sizeof(r_sky));

  R_InitSkyProgram();
}

/**
 * @brief
 */
static void R_ShutdownSkyProgram(void) {

  glDeleteProgram(r_sky_program.name);

  r_sky_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownSky(void) {

  R_ShutdownSkyProgram();
}

/**
 * @brief Loads the sky cubemap specified in worldspawn.
 */
void R_LoadSky(void) {

  glActiveTexture(GL_TEXTURE0 + TEXTURE_SKY);

  const char *name = Cm_EntityValue(Cm_Worldspawn(), "sky")->nullable_string;
  if (name) {
    r_sky.image = R_LoadImage(va("sky/%s", name), IMG_CUBEMAP);
  } else {
    r_sky.image = NULL;
  }

  if (r_sky.image == NULL) {
    Com_Warn("Failed to load sky sky/%s\n", name);

    r_sky.image = R_LoadImage("sky/template", IMG_CUBEMAP);
    if (r_sky.image == NULL) {
      Com_Error(ERROR_DROP, "Failed to load default sky\n");
    }
  }

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);
}
