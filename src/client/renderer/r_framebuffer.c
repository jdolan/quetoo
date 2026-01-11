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
 * @brief
 */
static GLuint R_CreateFramebufferTexture(const r_framebuffer_t *f,
                     GLenum internal_format,
                     GLenum format,
                     GLenum type) {
  GLuint texture;

  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);

  glTexImage2D(GL_TEXTURE_2D, 
         0,
         internal_format,
         f->width,
         f->height,
         0,
         format,
         type,
         NULL);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture(GL_TEXTURE_2D, 0);

  return texture;
}

/**
 * @brief
 */
static GLuint R_CreateFramebufferAttachment(const r_framebuffer_t *f, r_attachment_t attachment) {

  switch (attachment) {
    case ATTACHMENT_COLOR:
      return R_CreateFramebufferTexture(f, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
    case ATTACHMENT_DEPTH:
    case ATTACHMENT_DEPTH_COPY:
      return R_CreateFramebufferTexture(f, GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT);
    default:
      assert(0);
  }
}

/**
 * @brief
 */
r_framebuffer_t R_CreateFramebuffer(GLint width, GLint height, int32_t attachments) {

  const float scale = Clampf(r_supersample->value, 0.25f, 2.f);

  r_framebuffer_t framebuffer = {
    .width = width * scale,
    .height = height * scale,
    .attachments = attachments,
  };

  glGenFramebuffers(1, &framebuffer.name);
  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.name);

  if (attachments & ATTACHMENT_COLOR) {
    framebuffer.color_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_COLOR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color_attachment, 0);
  }

  if (attachments & ATTACHMENT_DEPTH) {
    framebuffer.depth_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_DEPTH);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depth_attachment, 0);
  }

  if (attachments & ATTACHMENT_DEPTH_COPY) {
    framebuffer.depth_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_DEPTH_COPY);
  }

  const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    Com_Error(ERROR_FATAL, "Failed to create framebuffer: %d\n", status);
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  R_GetError(NULL);

  return framebuffer;
}

/**
 * @brief
 */
void R_ClearFramebuffer(r_framebuffer_t *framebuffer) {

  glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->name);

  glDrawBuffers(4, (const GLenum []) {
    GL_COLOR_ATTACHMENT0,
    GL_COLOR_ATTACHMENT1,
    GL_COLOR_ATTACHMENT2,
    GL_COLOR_ATTACHMENT3,
    GL_COLOR_ATTACHMENT4
  });

  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glDrawBuffers(1, (const GLenum []) {
    GL_COLOR_ATTACHMENT0,
  });

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

/**
 * @brief
 */
void R_CopyFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, GLuint *texture) {

  assert(framebuffer);
  assert(texture);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->name);

  if (*texture == 0) {
    *texture = R_CreateFramebufferAttachment(framebuffer, attachment);
  }

  switch (attachment) {
    case ATTACHMENT_COLOR:
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      break;
    default:
      break;
  }

  glBindTexture(GL_TEXTURE_2D, *texture);
  glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  glReadBuffer(GL_BACK);

  R_GetError(NULL);
}

/**
 * @brief Blits the framebuffer object to the specified screen rect.
 */
void R_BlitFramebufferAttachment(const r_framebuffer_t *framebuffer,
                 r_attachment_t attachment,
                 GLint x, GLint y, GLint w, GLint h) {

  assert(framebuffer);

  w = w ?: r_context.pw;
  h = h ?: r_context.ph;

  glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->name);

  switch (attachment) {
    case ATTACHMENT_COLOR:
      glReadBuffer(GL_COLOR_ATTACHMENT0);
      break;
    default:
      Com_Error(ERROR_DROP, "Can't blit attachment %d\n", attachment);
  }

  glBlitFramebuffer(0,
            0,
            framebuffer->width,
            framebuffer->height,
            x,
            y,
            x + w,
            y + h,
            GL_COLOR_BUFFER_BIT,
            GL_LINEAR);

  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

  glReadBuffer(GL_BACK);

  R_GetError(NULL);
}

/**
 * @brief Blits the framebuffer object to the specified screen rect.
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, GLint x, GLint y, GLint w, GLint h) {
  R_BlitFramebufferAttachment(framebuffer, ATTACHMENT_COLOR, x, y, w, h);
}

/**
 * @brief
 */
void R_ReadFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, SDL_Surface **surface) {

  assert(framebuffer);
  assert(surface);

  if (*surface == NULL) {
    *surface = SDL_CreateSurface(framebuffer->width, framebuffer->height, SDL_PIXELFORMAT_RGB24);
  }

  GLuint in = 0;
  switch (attachment) {
    case ATTACHMENT_COLOR:
      in = framebuffer->color_attachment;
      break;
    default:
      break;
  }

  assert(in);

  glBindTexture(GL_TEXTURE_2D, in);
  glGetTexImage(GL_TEXTURE_2D, 0, GL_BGR, GL_UNSIGNED_BYTE, (*surface)->pixels);

  R_GetError(NULL);
}

/**
 * @brief
 */
void R_DestroyFramebuffer(r_framebuffer_t *framebuffer) {

  assert(framebuffer);

  if (framebuffer->name) {
    glDeleteFramebuffers(1, &framebuffer->name);

    if (framebuffer->color_attachment) {
      glDeleteTextures(1, &framebuffer->color_attachment);
    }

    if (framebuffer->depth_attachment) {
      glDeleteTextures(1, &framebuffer->depth_attachment);
    }

    if (framebuffer->depth_attachment_copy) {
      glDeleteTextures(1, &framebuffer->depth_attachment_copy);
    }

    R_GetError(NULL);
  }

  memset(framebuffer, 0, sizeof(*framebuffer));
}
