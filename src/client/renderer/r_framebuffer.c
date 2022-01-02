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

	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, f->width, f->height, 0, format, type, NULL);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glBindTexture(GL_TEXTURE_2D, 0);

	return texture;
}

/**
 * @brief
 */
r_framebuffer_t R_CreateFramebuffer(r_pixel_t width, r_pixel_t height, int32_t attachments) {

	r_framebuffer_t framebuffer = {
		.width = width,
		.height = height,
	};

	glGenFramebuffers(1, &framebuffer.name);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.name);

	if (attachments & ATTACHMENT_COLOR) {
		framebuffer.color_attachment = R_CreateFramebufferTexture(&framebuffer, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		framebuffer.color_attachment_copy = R_CreateFramebufferTexture(&framebuffer, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color_attachment, 0);
	}

	if (attachments & ATTACHMENT_BLOOM) {
		framebuffer.bloom_attachment = R_CreateFramebufferTexture(&framebuffer, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		framebuffer.bloom_attachment_copy = R_CreateFramebufferTexture(&framebuffer, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, framebuffer.bloom_attachment, 0);
	}

	if (attachments & ATTACHMENT_DEPTH) {
		framebuffer.depth_attachment = R_CreateFramebufferTexture(&framebuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		framebuffer.depth_attachment_copy = R_CreateFramebufferTexture(&framebuffer, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, framebuffer.depth_attachment, 0);
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
void R_CopyFramebufferAttachments(const r_framebuffer_t *framebuffer, int32_t attachments) {

	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->name);

	if (attachments & ATTACHMENT_COLOR) {
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glBindTexture(GL_TEXTURE_2D, framebuffer->color_attachment_copy);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);
	}

	if (attachments & ATTACHMENT_BLOOM) {
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glBindTexture(GL_TEXTURE_2D, framebuffer->bloom_attachment_copy);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);
	}

	if (attachments & ATTACHMENT_DEPTH) {
		glReadBuffer(GL_DEPTH_STENCIL_ATTACHMENT);
		glBindTexture(GL_TEXTURE_2D, framebuffer->depth_attachment_copy);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	glReadBuffer(GL_BACK);

	R_GetError(NULL);
}

/**
 * @brief Blits the framebuffer object to the specified screen rect.
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, r_pixel_t x, r_pixel_t y, r_pixel_t w, r_pixel_t h) {

	assert(framebuffer);

	w = w ?: r_context.drawable_width;
	h = h ?: r_context.drawable_height;

	glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer->name);

	glReadBuffer(GL_COLOR_ATTACHMENT0);

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

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	glReadBuffer(GL_BACK);

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
			glDeleteTextures(1, &framebuffer->color_attachment_copy);
		}

		if (framebuffer->bloom_attachment) {
			glDeleteTextures(1, &framebuffer->bloom_attachment);
			glDeleteTextures(1, &framebuffer->bloom_attachment_copy);
		}

		if (framebuffer->depth_attachment) {
			glDeleteTextures(1, &framebuffer->depth_attachment);
			glDeleteTextures(1, &framebuffer->depth_attachment_copy);
		}

		R_GetError(NULL);
	}

	memset(framebuffer, 0, sizeof(*framebuffer));
}

