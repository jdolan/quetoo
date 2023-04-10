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
void R_BlurFramebuffer(const r_framebuffer_t *framebuffer, r_attachment_t attachment, int32_t kernel) {

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
void R_InitBlur(void) {

	memset(&r_blur_data, 0, sizeof(r_blur_data));

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
void R_ShutdownBlur(void) {

	R_ShutdownBlurProgram();

	R_DestroyFramebuffer(&r_blur_data.framebuffers[0]);
	R_DestroyFramebuffer(&r_blur_data.framebuffers[1]);

	glDeleteBuffers(1, &r_blur_data.vertex_buffer);
	glDeleteVertexArrays(1, &r_blur_data.vertex_array);

	R_GetError(NULL);

	memset(&r_blur_data, 0, sizeof(r_blur_data));
}
