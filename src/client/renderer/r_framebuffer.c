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
										 GLenum attachment,
										 GLenum internal_format,
										 GLenum format,
										 GLenum type) {
	GLuint name;

	if (f->multisample) {
		glGenRenderbuffers(1, &name);
		glBindRenderbuffer(GL_RENDERBUFFER, name);

		const GLsizei samples = r_context.multisample_samples;
		if (samples > 1) {
			glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, internal_format, f->width, f->height);
		} else {
			glRenderbufferStorage(GL_RENDERBUFFER, internal_format, f->width, f->height);
		}

		glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, name);
	} else {
		glGenTextures(1, &name);
		glBindTexture(GL_TEXTURE_2D, name);

		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, f->width, f->height, 0, format, type, NULL);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, name, 0);
	}

	return name;
}

/**
 * @brief
 */
static GLuint R_CreateColorAttachment(const r_framebuffer_t *f, GLenum attachment) {
	return R_CreateFramebufferTexture(f, attachment, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE);
}

/**
 * @brief
 */
static GLuint R_CreateDepthAttachment(const r_framebuffer_t *f, GLenum attachment) {
	return R_CreateFramebufferTexture(f, attachment, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
}

/**
 * @brief
 */
r_framebuffer_t R_CreateFramebuffer(r_pixel_t width, r_pixel_t height, _Bool multisample) {

	r_framebuffer_t framebuffer = {
		.width = width,
		.height = height,
		.multisample = multisample
	};

	glGenFramebuffers(1, &framebuffer.name);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.name);

	framebuffer.bloom_attachment = R_CreateColorAttachment(&framebuffer, GL_COLOR_ATTACHMENT1);
	framebuffer.color_attachment = R_CreateColorAttachment(&framebuffer, GL_COLOR_ATTACHMENT0);
	framebuffer.depth_attachment = R_CreateDepthAttachment(&framebuffer, GL_DEPTH_STENCIL_ATTACHMENT);

	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		Com_Error(ERROR_FATAL, "Failed to create framebuffer: %d\n", status);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	if (multisample) {
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	} else {
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	R_GetError(NULL);

	return framebuffer;
}

/**
 * @brief
 */
void R_DestroyFramebuffer(r_framebuffer_t *framebuffer) {

	if (framebuffer) {

		if (framebuffer->name) {
			glDeleteFramebuffers(1, &framebuffer->name);
		}

		if (framebuffer->multisample) {
			if (framebuffer->bloom_attachment) {
				glDeleteRenderbuffers(1, &framebuffer->bloom_attachment);
			}
			if (framebuffer->color_attachment) {
				glDeleteRenderbuffers(1, &framebuffer->color_attachment);
			}

			if (framebuffer->depth_attachment) {
				glDeleteRenderbuffers(1, &framebuffer->depth_attachment);
			}
		} else {
			if (framebuffer->bloom_attachment) {
				glDeleteTextures(1, &framebuffer->bloom_attachment);
			}
			if (framebuffer->color_attachment) {
				glDeleteTextures(1, &framebuffer->color_attachment);
			}
			if (framebuffer->depth_attachment) {
				glDeleteTextures(1, &framebuffer->depth_attachment);
			}
		}

		memset(framebuffer, 0, sizeof(*framebuffer));
	}
}

/**
 * @brief Blits the framebuffer object to the specified screen rect.
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h) {

	if (framebuffer) {
		w = w ?: r_context.drawable_width;
		h = h ?: r_context.drawable_height;
		
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->name);
		glBlitFramebuffer(0,
						  0,
						  framebuffer->width,
						  framebuffer->height,
						  x,
						  y,
						  x + w,
						  y + h,
						  GL_COLOR_BUFFER_BIT,
						  GL_NEAREST);
		glReadBuffer(GL_COLOR_ATTACHMENT0);
	} else {
		Com_Warn("NULL framebuffer\n");
	}
}
