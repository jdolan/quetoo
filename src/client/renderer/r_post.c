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
 * @brief The post vertex type.
 */
typedef struct {
	vec2_t position;
	vec2_t texcoord;
} r_post_vertex_t;

/**
 * @brief The post data.
 */
static struct {
	GLuint vertex_array;
	GLuint vertex_buffer;
} r_post_data;

/**
 * @brief The post program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_texcoord;

	GLint texture_color_attachment;
	GLint texture_bloom_attachment;
} r_post_program;

/**
 * @brief Draw post-processing stages like bloom and HDR.
 */
void R_DrawPost(const r_view_t *view) {

	if (!r_post->integer) {
		return;
	}

	assert(view->framebuffer);

	if (r_bloom->value) {
		R_BlurFramebufferAttachment(view->framebuffer, ATTACHMENT_BLOOM, r_bloom_iterations->integer);
	}

	glUseProgram(r_post_program.name);

	glBindVertexArray(r_post_data.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_post_data.vertex_buffer);

	glEnableVertexAttribArray(r_post_program.in_position);
	glEnableVertexAttribArray(r_post_program.in_texcoord);

	glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT4 });

	glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, view->framebuffer->color_attachment);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_BLOOM_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, view->framebuffer->bloom_attachment);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	glDrawBuffers(1, (const GLenum []) { GL_COLOR_ATTACHMENT0 });

	glUseProgram(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitPostProgram(void) {

	r_post_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "post_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "post_fs.glsl", NULL),
			NULL);

	glUseProgram(r_post_program.name);

	r_post_program.uniforms_block = glGetUniformBlockIndex(r_post_program.name, "uniforms_block");
	glUniformBlockBinding(r_post_program.name, r_post_program.uniforms_block, 0);

	r_post_program.in_position = glGetAttribLocation(r_post_program.name, "in_position");
	r_post_program.in_texcoord = glGetAttribLocation(r_post_program.name, "in_texcoord");

	r_post_program.texture_color_attachment = glGetUniformLocation(r_post_program.name, "texture_color_attachment");
	r_post_program.texture_bloom_attachment = glGetUniformLocation(r_post_program.name, "texture_bloom_attachment");

	glUniform1i(r_post_program.texture_color_attachment, TEXTURE_COLOR_ATTACHMENT);
	glUniform1i(r_post_program.texture_bloom_attachment, TEXTURE_BLOOM_ATTACHMENT);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitPost(void) {

	memset(&r_post_program, 0, sizeof(r_post_program));

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

	glBufferData(GL_ARRAY_BUFFER, sizeof(r_post_vertex_t) * 6, vertexes, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(r_post_vertex_t), (void *) offsetof(r_post_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_post_vertex_t), (void *) offsetof(r_post_vertex_t, texcoord));

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitPostProgram();
}

/**
 * @brief
 */
void R_ShutdownPostProgram(void) {

	glDeleteProgram(r_post_program.name);

	R_GetError(NULL);

	r_post_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownPost(void) {

	R_ShutdownPostProgram();

	glDeleteBuffers(1, &r_post_data.vertex_buffer);
	glDeleteVertexArrays(1, &r_post_data.vertex_array);

	R_GetError(NULL);

	memset(&r_post_data, 0, sizeof(r_post_data));
}
