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
 * @brief The bloom vertex type.
 */
typedef struct {
	vec2_t position;
	vec2_t texcoord;
} r_bloom_vertex_t;

/**
 * @brief The bloom data.
 */
static struct {
	GLuint vertex_array;
	GLuint vertex_buffer;

	GLuint bloom_attachment;
	GLuint color_attachment;
} r_bloom_data;

/**
 * @brief The bloom program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;
	GLint in_texcoord;

	GLint texture_bloom_attachment;
	GLint texture_color_attachment;
} r_bloom_program;

/**
 * @brief
 */
void R_DrawBloom(const r_view_t *view) {

	if (!r_bloom->value) {
		return;
	}

	R_CopyFramebuffer(view->framebuffer, GL_COLOR_ATTACHMENT1, &r_bloom_data.bloom_attachment);
	R_CopyFramebuffer(view->framebuffer, GL_COLOR_ATTACHMENT0, &r_bloom_data.color_attachment);

	glUseProgram(r_bloom_program.name);

	glBindVertexArray(r_bloom_data.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_bloom_data.vertex_buffer);

	glEnableVertexAttribArray(r_bloom_program.in_position);
	glEnableVertexAttribArray(r_bloom_program.in_texcoord);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_BLOOM_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_bloom_data.bloom_attachment);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_COLOR_ATTACHMENT);
	glBindTexture(GL_TEXTURE_2D, r_bloom_data.color_attachment);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glUseProgram(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitBloomProgram(void) {

	r_bloom_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "bloom_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "bloom_fs.glsl", NULL),
			NULL);

	glUseProgram(r_bloom_program.name);

	r_bloom_program.uniforms_block = glGetUniformBlockIndex(r_bloom_program.name, "uniforms_block");
	glUniformBlockBinding(r_bloom_program.name, r_bloom_program.uniforms_block, 0);

	r_bloom_program.in_position = glGetAttribLocation(r_bloom_program.name, "in_position");
	r_bloom_program.in_texcoord = glGetAttribLocation(r_bloom_program.name, "in_texcoord");

	r_bloom_program.texture_bloom_attachment = glGetUniformLocation(r_bloom_program.name, "texture_bloom_attachment");
	r_bloom_program.texture_color_attachment = glGetUniformLocation(r_bloom_program.name, "texture_color_attachment");

	glUniform1i(r_bloom_program.texture_bloom_attachment, TEXTURE_BLOOM_ATTACHMENT);
	glUniform1i(r_bloom_program.texture_color_attachment, TEXTURE_COLOR_ATTACHMENT);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitBloom(void) {

	memset(&r_bloom_program, 0, sizeof(r_bloom_program));

	glGenVertexArrays(1, &r_bloom_data.vertex_array);
	glBindVertexArray(r_bloom_data.vertex_array);

	glGenBuffers(1, &r_bloom_data.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_bloom_data.vertex_buffer);

	r_bloom_vertex_t quad[4];

	quad[0].position = Vec2(-1.f, -1.f);
	quad[1].position = Vec2( 1.f, -1.f);
	quad[2].position = Vec2( 1.f,  1.f);
	quad[3].position = Vec2(-1.f,  1.f);

	quad[0].texcoord = Vec2(0.f, 0.f);
	quad[1].texcoord = Vec2(1.f, 0.f);
	quad[2].texcoord = Vec2(1.f, 1.f);
	quad[3].texcoord = Vec2(0.f, 1.f);

	const r_bloom_vertex_t vertexes[] = { quad[0], quad[1], quad[2], quad[0], quad[2], quad[3] };

	glBufferData(GL_ARRAY_BUFFER, sizeof(r_bloom_vertex_t) * 6, vertexes, GL_STATIC_DRAW);

	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(r_bloom_vertex_t), (void *) offsetof(r_bloom_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_bloom_vertex_t), (void *) offsetof(r_bloom_vertex_t, texcoord));

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	R_GetError(NULL);

	R_InitBloomProgram();
}

/**
 * @brief
 */
void R_ShutdownBloomProgram(void) {

	glDeleteProgram(r_bloom_program.name);

	R_GetError(NULL);

	r_bloom_program.name = 0;
}

/**
 * @brief
 */
void R_ShutdownBloom(void) {

	R_ShutdownBloomProgram();

	glDeleteBuffers(1, &r_bloom_data.vertex_buffer);
	glDeleteVertexArrays(1, &r_bloom_data.vertex_array);

	if (r_bloom_data.bloom_attachment) {
		glDeleteTextures(1, &r_bloom_data.bloom_attachment);
	}
	if (r_bloom_data.color_attachment) {
		glDeleteTextures(1, &r_bloom_data.color_attachment);
	}

	R_GetError(NULL);

	memset(&r_bloom_data, 0, sizeof(r_bloom_data));
}
