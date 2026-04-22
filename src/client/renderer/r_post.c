/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006-2011 Quetoo.
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
 * @brief Vertex type for fullscreen post-processing quads.
 */
typedef struct {
  vec2_t position;
  vec2_t texcoord;
} r_post_vertex_t;

/**
 * @brief Shared fullscreen quad geometry.
 */
static struct {
  GLuint vertex_array;
  GLuint vertex_buffer;
} r_post_data;

/**
 * @brief Half-resolution bloom ping-pong framebuffers.
 *
 * bloom_fbo[0] receives the bloom extract pass and every second blur result.
 * bloom_fbo[1] receives every first blur result.  They ping-pong across
 * the horizontal and vertical Gaussian passes.
 *
 * Stored dimensions let R_DrawPost detect viewport resizes and recreate.
 */
static struct {
  GLuint name;
  GLuint color_attachment;
} r_bloom_fbo[2];

static GLint r_bloom_width, r_bloom_height;

/**
 * @brief The Gaussian blur program (blur_vs.glsl + blur_fs.glsl).
 */
static struct {
  GLuint name;
  GLint texture_bloom_attachment;
  GLint axis;
} r_blur_program;

/**
 * @brief The post-processing program (post_vs.glsl + post_fs.glsl).
 *
 * Handles both bloom extraction (mode 0) and bloom composite (mode 1).
 */
static struct {
  GLuint name;
  GLint texture_color_attachment;
  GLint texture_bloom_attachment;
  GLint mode;
  GLint bloom;
  GLint bloom_threshold;
} r_post_program;

/**
 * @brief Create or recreate the half-resolution bloom ping-pong FBOs.
 */
static void R_CreateBloomFbos(GLint width, GLint height) {

  r_bloom_width  = width  / 2;
  r_bloom_height = height / 2;

  if (r_bloom_width  < 1) { r_bloom_width  = 1; }
  if (r_bloom_height < 1) { r_bloom_height = 1; }

  for (int32_t i = 0; i < 2; i++) {

    if (r_bloom_fbo[i].name) {
      glDeleteFramebuffers(1, &r_bloom_fbo[i].name);
      glDeleteTextures(1, &r_bloom_fbo[i].color_attachment);
    }

    glGenFramebuffers(1, &r_bloom_fbo[i].name);
    glBindFramebuffer(GL_FRAMEBUFFER, r_bloom_fbo[i].name);

    glGenTextures(1, &r_bloom_fbo[i].color_attachment);
    glBindTexture(GL_TEXTURE_2D, r_bloom_fbo[i].color_attachment);

    glTexImage2D(GL_TEXTURE_2D,
           0,
           GL_R11F_G11F_B10F,
           r_bloom_width,
           r_bloom_height,
           0,
           GL_RGB,
           GL_FLOAT,
           NULL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_bloom_fbo[i].color_attachment, 0);

    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      Com_Error(ERROR_FATAL, "Failed to create bloom framebuffer %d: %d\n", i, status);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }

  R_GetError(NULL);
}

/**
 * @brief Destroy the half-resolution bloom ping-pong FBOs.
 */
static void R_DestroyBloomFbos(void) {

  for (int32_t i = 0; i < 2; i++) {
    if (r_bloom_fbo[i].name) {
      glDeleteFramebuffers(1, &r_bloom_fbo[i].name);
      glDeleteTextures(1, &r_bloom_fbo[i].color_attachment);
      r_bloom_fbo[i].name = 0;
      r_bloom_fbo[i].color_attachment = 0;
    }
  }

  r_bloom_width  = 0;
  r_bloom_height = 0;

  R_GetError(NULL);
}

/**
 * @brief Draw the bloom pipeline: extract → blur → composite.
 *
 * The pipeline runs entirely on the GPU using three fullscreen quad passes
 * per blur iteration:
 *
 *   1. Bloom extract (post program, mode 0):
 *      Sample the full-resolution scene color attachment, apply a
 *      soft-knee quadratic threshold, and write the bright regions to
 *      the first half-resolution bloom FBO.
 *
 *   2. For each blur iteration:
 *      a. Horizontal Gaussian blur (blur program, axis 0):
 *         Read bloom_fbo[0], write bloom_fbo[1].
 *      b. Vertical Gaussian blur (blur program, axis 1):
 *         Read bloom_fbo[1], write bloom_fbo[0].
 *
 *   3. Bloom composite (post program, mode 1):
 *      Add the blurred bloom (bloom_fbo[0]) onto the scene color and
 *      write the result to the post attachment (GL_COLOR_ATTACHMENT1).
 *      R_BlitFramebuffer will then blit the post attachment to the screen.
 */
void R_DrawPost(const r_view_t *view) {

  assert(view->framebuffer);
  assert(view->framebuffer->post_attachment);

  if (view->framebuffer->width  != r_bloom_width  * 2 ||
      view->framebuffer->height != r_bloom_height * 2) {
    R_CreateBloomFbos(view->framebuffer->width, view->framebuffer->height);
  }

  glBindVertexArray(r_post_data.vertex_array);

  // --- Pass 1: bloom extract → bloom_fbo[0] ---

  if (r_bloom->value > 0.f) {

    glUseProgram(r_post_program.name);
    glUniform1i(r_post_program.mode, 0);
    glUniform1f(r_post_program.bloom_threshold, r_bloom_threshold->value);

    glBindFramebuffer(GL_FRAMEBUFFER, r_bloom_fbo[0].name);
    glViewport(0, 0, r_bloom_width, r_bloom_height);

    glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_ATTACHMENT);
    glBindTexture(GL_TEXTURE_2D, view->framebuffer->color_attachment);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // --- Pass 2: iterative separable Gaussian blur ---

    glUseProgram(r_blur_program.name);

    const int32_t iterations = Clampf(r_bloom_iterations->integer, 1, 8);
    for (int32_t i = 0; i < iterations; i++) {

      // Horizontal pass: bloom_fbo[0] → bloom_fbo[1]
      glBindFramebuffer(GL_FRAMEBUFFER, r_bloom_fbo[1].name);
      glUniform1i(r_blur_program.axis, 0);
      glActiveTexture(GL_TEXTURE0 + TEXTURE_BLOOM_ATTACHMENT);
      glBindTexture(GL_TEXTURE_2D, r_bloom_fbo[0].color_attachment);
      glDrawArrays(GL_TRIANGLES, 0, 6);

      // Vertical pass: bloom_fbo[1] → bloom_fbo[0]
      glBindFramebuffer(GL_FRAMEBUFFER, r_bloom_fbo[0].name);
      glUniform1i(r_blur_program.axis, 1);
      glActiveTexture(GL_TEXTURE0 + TEXTURE_BLOOM_ATTACHMENT);
      glBindTexture(GL_TEXTURE_2D, r_bloom_fbo[1].color_attachment);
      glDrawArrays(GL_TRIANGLES, 0, 6);
    }
  }

  // --- Pass 3: composite bloom onto scene → post_attachment ---

  glUseProgram(r_post_program.name);
  glUniform1i(r_post_program.mode, 1);
  glUniform1f(r_post_program.bloom, r_bloom->value);

  glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);
  glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT1 });
  glViewport(0, 0, view->framebuffer->width, view->framebuffer->height);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_ATTACHMENT);
  glBindTexture(GL_TEXTURE_2D, view->framebuffer->color_attachment);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_BLOOM_ATTACHMENT);
  glBindTexture(GL_TEXTURE_2D, r_bloom->value > 0.f ? r_bloom_fbo[0].color_attachment : 0);

  glDrawArrays(GL_TRIANGLES, 0, 6);

  glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT0 });
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

  glBindVertexArray(0);
  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitBlurProgram(void) {

  r_blur_program.name = R_LoadProgram(
    R_ShaderDescriptor(GL_VERTEX_SHADER,   "blur_vs.glsl", NULL),
    R_ShaderDescriptor(GL_FRAGMENT_SHADER, "blur_fs.glsl", NULL),
    NULL);

  glUseProgram(r_blur_program.name);

  r_blur_program.texture_bloom_attachment = glGetUniformLocation(r_blur_program.name, "texture_bloom_attachment");
  r_blur_program.axis                     = glGetUniformLocation(r_blur_program.name, "axis");

  glUniform1i(r_blur_program.texture_bloom_attachment, TEXTURE_BLOOM_ATTACHMENT);

  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitPostProgram(void) {

  r_post_program.name = R_LoadProgram(
    R_ShaderDescriptor(GL_VERTEX_SHADER,   "post_vs.glsl", NULL),
    R_ShaderDescriptor(GL_FRAGMENT_SHADER, "post_fs.glsl", NULL),
    NULL);

  glUseProgram(r_post_program.name);

  r_post_program.texture_color_attachment = glGetUniformLocation(r_post_program.name, "texture_color_attachment");
  r_post_program.texture_bloom_attachment = glGetUniformLocation(r_post_program.name, "texture_bloom_attachment");
  r_post_program.mode                     = glGetUniformLocation(r_post_program.name, "mode");
  r_post_program.bloom                    = glGetUniformLocation(r_post_program.name, "bloom");
  r_post_program.bloom_threshold          = glGetUniformLocation(r_post_program.name, "bloom_threshold");

  glUniform1i(r_post_program.texture_color_attachment, TEXTURE_COLOR_ATTACHMENT);
  glUniform1i(r_post_program.texture_bloom_attachment, TEXTURE_BLOOM_ATTACHMENT);

  glUseProgram(0);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitPost(void) {

  memset(&r_post_data,    0, sizeof(r_post_data));
  memset(&r_blur_program, 0, sizeof(r_blur_program));
  memset(&r_post_program, 0, sizeof(r_post_program));
  memset(r_bloom_fbo,     0, sizeof(r_bloom_fbo));

  glGenVertexArrays(1, &r_post_data.vertex_array);
  glBindVertexArray(r_post_data.vertex_array);

  glGenBuffers(1, &r_post_data.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, r_post_data.vertex_buffer);

  r_post_vertex_t quad[4];

  quad[0].position = Vec2(-1.f, -1.f);
  quad[1].position = Vec2( 1.f, -1.f);
  quad[2].position = Vec2( 1.f,  1.f);
  quad[3].position = Vec2(-1.f,  1.f);

  quad[0].texcoord = Vec2(0.f, 0.f);
  quad[1].texcoord = Vec2(1.f, 0.f);
  quad[2].texcoord = Vec2(1.f, 1.f);
  quad[3].texcoord = Vec2(0.f, 1.f);

  const r_post_vertex_t vertexes[] = { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3] };

  glBufferData(GL_ARRAY_BUFFER, sizeof(vertexes), vertexes, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(r_post_vertex_t), (void *) offsetof(r_post_vertex_t, position));
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_post_vertex_t), (void *) offsetof(r_post_vertex_t, texcoord));

  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);

  glBindVertexArray(0);

  R_GetError(NULL);

  R_InitBlurProgram();
  R_InitPostProgram();
}

/**
 * @brief
 */
void R_ShutdownPost(void) {

  R_DestroyBloomFbos();

  glDeleteProgram(r_blur_program.name);
  glDeleteProgram(r_post_program.name);

  glDeleteBuffers(1, &r_post_data.vertex_buffer);
  glDeleteVertexArrays(1, &r_post_data.vertex_array);

  R_GetError(NULL);

  memset(&r_post_data,    0, sizeof(r_post_data));
  memset(&r_blur_program, 0, sizeof(r_blur_program));
  memset(&r_post_program, 0, sizeof(r_post_program));
}
