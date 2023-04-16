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
 * @brief The blur vertex type.
 */
typedef struct {
	vec2_t position;
	vec2_t texcoord;
} r_blur_vertex_t;

/**
 * @brief The blur data.
 */
static struct {
	r_framebuffer_t framebuffers[2];

	GLuint vertex_array;
	GLuint vertex_buffer;
} r_blur_data;

/**
 * @brief The blur program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_texcoord;

	GLint texture_diffusemap;

	GLint horizontal;
} r_blur_program;

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
static GLuint R_CreateFramebufferAttachment(const r_framebuffer_t *f, r_attachment_t attachment) {

	switch (attachment) {
		case ATTACHMENT_COLOR:
		case ATTACHMENT_BLOOM:
			return R_CreateFramebufferTexture(f, GL_RGBA32F, GL_RGBA, GL_FLOAT);
		case ATTACHMENT_DEPTH:
			return R_CreateFramebufferTexture(f, GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8);
		default:
			assert(0);
	}
}

/**
 * @brief
 */
r_framebuffer_t R_CreateFramebuffer(GLint width, GLint height, int32_t attachments) {

	r_framebuffer_t framebuffer = {
		.width = width,
		.height = height,
	};

	glGenFramebuffers(1, &framebuffer.name);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.name);

	if (attachments & ATTACHMENT_COLOR) {
		framebuffer.color_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_COLOR);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, framebuffer.color_attachment, 0);
	}

	if (attachments & ATTACHMENT_BLOOM) {
		framebuffer.bloom_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_BLOOM);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, framebuffer.bloom_attachment, 0);
	}

	if (attachments & ATTACHMENT_DEPTH) {
		framebuffer.depth_attachment = R_CreateFramebufferAttachment(&framebuffer, ATTACHMENT_DEPTH);
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
void R_CopyFramebufferAttachment(r_framebuffer_t *framebuffer, r_attachment_t attachment, GLuint *texture) {

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
		case ATTACHMENT_BLOOM:
			glReadBuffer(GL_COLOR_ATTACHMENT1);
			break;
		default:
			break;
	}

	glBindTexture(GL_TEXTURE_2D, *texture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief Blits the framebuffer object to the specified screen rect.
 */
void R_BlitFramebuffer(const r_framebuffer_t *framebuffer, GLint x, GLint y, GLint w, GLint h) {

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
void R_ReadFramebufferAttachment(const r_framebuffer_t *framebuffer,
								 r_attachment_t attachment,
								 SDL_Surface **surface) {

	assert(framebuffer);
	assert(surface);

	if (*surface == NULL) {
		*surface = SDL_CreateRGBSurfaceWithFormat(0,
												  framebuffer->width,
												  framebuffer->height,
												  24,
												  SDL_PIXELFORMAT_RGB24);
	}

	glBindTexture(GL_TEXTURE_2D, framebuffer->color_attachment);
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

		if (framebuffer->bloom_attachment) {
			glDeleteTextures(1, &framebuffer->bloom_attachment);
		}

		if (framebuffer->depth_attachment) {
			glDeleteTextures(1, &framebuffer->depth_attachment);
		}

		R_GetError(NULL);
	}

	memset(framebuffer, 0, sizeof(*framebuffer));
}

/**
 * @brief
 */
void R_BlurFramebufferAttachment(const r_framebuffer_t *framebuffer, r_attachment_t attachment, int32_t kernel) {

	assert(framebuffer);

	GLuint texture = 0;
	switch (attachment) {
		case ATTACHMENT_COLOR:
			texture = framebuffer->color_attachment;
			break;
		case ATTACHMENT_BLOOM:
			texture = framebuffer->bloom_attachment;
			break;
		default:
			break;
	}

	assert(texture);

	glUseProgram(r_blur_program.name);

	glBindVertexArray(r_blur_data.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_blur_data.vertex_buffer);

	glEnableVertexAttribArray(r_blur_program.in_position);
	glEnableVertexAttribArray(r_blur_program.in_texcoord);

	for (int32_t i = 0; i < kernel; i++) {

		glBindFramebuffer(GL_FRAMEBUFFER, r_blur_data.framebuffers[i & 1].name);
		glUniform1i(r_blur_program.horizontal, i & 1);

		if (i == 0) {
			glBindTexture(GL_TEXTURE_2D, texture);
		} else {
			glBindTexture(GL_TEXTURE_2D, r_blur_data.framebuffers[(i + 1) & 1].color_attachment);
		}

		glDrawArrays(GL_TRIANGLES, 0, 6);
	}

	glBindTexture(GL_TEXTURE_2D, texture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, framebuffer->width, framebuffer->height);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->name);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitBlurProgram(void) {

	r_blur_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "blur_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "blur_fs.glsl", NULL),
			NULL);

	glUseProgram(r_blur_program.name);

	r_blur_program.uniforms_block = glGetUniformBlockIndex(r_blur_program.name, "uniforms_block");
	glUniformBlockBinding(r_blur_program.name, r_blur_program.uniforms_block, 0);

	r_blur_program.in_position = glGetAttribLocation(r_blur_program.name, "in_position");
	r_blur_program.in_texcoord = glGetAttribLocation(r_blur_program.name, "in_texcoord");

	r_blur_program.texture_diffusemap = glGetUniformLocation(r_blur_program.name, "texture_diffusemap");
	glUniform1i(r_blur_program.texture_diffusemap, TEXTURE_DIFFUSEMAP);

	r_blur_program.horizontal = glGetUniformLocation(r_blur_program.name, "horizontal");
	glUniform1i(r_blur_program.horizontal, 1);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitFramebuffer(void) {

	memset(&r_blur_data, 0, sizeof(r_blur_data));

	// FIXME: These should be associated with an actual framebuffer, and use the same sizes
	r_blur_data.framebuffers[0] = R_CreateFramebuffer(r_context.width, r_context.height, ATTACHMENT_COLOR);
	r_blur_data.framebuffers[1] = R_CreateFramebuffer(r_context.width, r_context.height, ATTACHMENT_COLOR);

	glGenVertexArrays(1, &r_blur_data.vertex_array);
	glBindVertexArray(r_blur_data.vertex_array);

	glGenBuffers(1, &r_blur_data.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_blur_data.vertex_buffer);

	r_blur_vertex_t quad[4];

	quad[0].position = Vec2(-1.f, -1.f);
	quad[1].position = Vec2( 1.f, -1.f);
	quad[2].position = Vec2( 1.f,  1.f);
	quad[3].position = Vec2(-1.f,  1.f);

	quad[0].texcoord = Vec2(0.f, 0.f);
	quad[1].texcoord = Vec2(1.f, 0.f);
	quad[2].texcoord = Vec2(1.f, 1.f);
	quad[3].texcoord = Vec2(0.f, 1.f);

	const r_blur_vertex_t vertexes[] = { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3] };

	glBufferData(GL_ARRAY_BUFFER, sizeof(r_blur_vertex_t) * 6, vertexes, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(r_blur_vertex_t), (void *) offsetof(r_blur_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_blur_vertex_t), (void *) offsetof(r_blur_vertex_t, texcoord));

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitBlurProgram();
}

/**
 * @brief
 */
void R_ShutdownBlurProgram(void) {

	glDeleteProgram(r_blur_program.name);

	R_GetError(NULL);

	r_blur_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownFramebuffer(void) {

	R_ShutdownBlurProgram();

	R_DestroyFramebuffer(&r_blur_data.framebuffers[0]);
	R_DestroyFramebuffer(&r_blur_data.framebuffers[1]);

	glDeleteBuffers(1, &r_blur_data.vertex_buffer);
	glDeleteVertexArrays(1, &r_blur_data.vertex_array);

	R_GetError(NULL);

	memset(&r_blur_data, 0, sizeof(r_blur_data));
}
