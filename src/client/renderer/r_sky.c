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
  GLint texture_voxel_light_data;
  GLint texture_voxel_light_indices;

  GLint block;
} r_sky_program;

/**
 * @brief
 */
void R_DrawSky(const r_view_t *view, const r_bsp_model_t *bsp) {

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  glUseProgram(r_sky_program.name);

  glBindVertexArray(bsp->vertex_array);

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
      R_ShaderDescriptor(GL_VERTEX_SHADER, "sky_vs.glsl", NULL),
      R_ShaderDescriptor(GL_FRAGMENT_SHADER, "material.glsl", "sky_fs.glsl", NULL),
      NULL);

  glUseProgram(r_sky_program.name);

  r_sky_program.uniforms_block = glGetUniformBlockIndex(r_sky_program.name, "uniforms_block");
  glUniformBlockBinding(r_sky_program.name, r_sky_program.uniforms_block, 0);

  const GLuint lights_block = glGetUniformBlockIndex(r_sky_program.name, "lights_block");
  glUniformBlockBinding(r_sky_program.name, lights_block, 1);

  r_sky_program.texture_sky = glGetUniformLocation(r_sky_program.name, "texture_sky");
  r_sky_program.texture_voxel_light_data = glGetUniformLocation(r_sky_program.name, "texture_voxel_light_data");
  r_sky_program.texture_voxel_light_indices = glGetUniformLocation(r_sky_program.name, "texture_voxel_light_indices");

  glUniform1i(r_sky_program.texture_sky, TEXTURE_SKY);
  glUniform1i(r_sky_program.texture_voxel_light_data, TEXTURE_VOXEL_LIGHT_DATA);
  glUniform1i(r_sky_program.texture_voxel_light_indices, TEXTURE_VOXEL_LIGHT_INDICES);

  r_sky_program.block = glGetUniformLocation(r_sky_program.name, "block");

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
