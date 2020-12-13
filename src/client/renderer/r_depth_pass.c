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

/**
 * @brief The depth program.
 */
static struct {
	GLuint name;
	GLuint uniforms;

	GLint in_position;

	GLint model;
} r_depth_pass_program;

/**
 * @brief
 */
static void R_DrawBspInlineModelDepthPass(const r_entity_t *e, const r_bsp_inline_model_t *in) {

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {
		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawDepthPass(void) {

	if (!r_draw_depth_pass->value) {
		return;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(8.f, 1.f);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	glUseProgram(r_depth_pass_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_world_model->bsp->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->elements_buffer);

	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glUniformMatrix4fv(r_depth_pass_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);
	R_DrawBspInlineModelDepthPass(NULL, r_world_model->bsp->inline_models);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {

			glUniformMatrix4fv(r_depth_pass_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);
			R_DrawBspInlineModelDepthPass(e, e->model->bsp_inline);
		}
	}

	glBindVertexArray(0);

	glUseProgram(0);

	glPolygonOffset(0.f, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);
}


/**
 * @brief
 */
static void R_InitDepthPassProgram(void) {

	memset(&r_depth_pass_program, 0, sizeof(r_depth_pass_program));

	r_depth_pass_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "depth_pass_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "depth_pass_fs.glsl", NULL),
			NULL);

	glUseProgram(r_depth_pass_program.name);

	r_depth_pass_program.uniforms = glGetUniformBlockIndex(r_depth_pass_program.name, "uniforms");
	glUniformBlockBinding(r_depth_pass_program.name, r_depth_pass_program.uniforms, 0);

	r_depth_pass_program.in_position = glGetAttribLocation(r_depth_pass_program.name, "in_position");

	r_depth_pass_program.model = glGetUniformLocation(r_depth_pass_program.name, "model");

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownDepthPassProgram(void) {

	glDeleteProgram(r_depth_pass_program.name);

	r_depth_pass_program.name = 0;
}

/**
 * @brief
 */
void R_InitDepthPass(void) {

	R_InitDepthPassProgram();
}

/**
 * @brief
 */
void R_ShutdownDepthPass(void) {

	R_ShutdownDepthPassProgram();
}
