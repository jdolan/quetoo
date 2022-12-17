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
 * @brief The depth pass program.
 */
r_depth_pass_program_t r_depth_pass_program;

/**
 * @brief
 */
static void R_DrawBspDepthPass(const r_view_t *view) {

	glUniformMatrix4fv(r_depth_pass_program.model, 1, GL_FALSE, Mat4_Identity().array);

	glBindVertexArray(r_world_model->bsp->vertex_array);
	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glBindBuffer(GL_ARRAY_BUFFER, r_world_model->bsp->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_world_model->bsp->depth_pass_elements_buffer);

	glDrawElements(GL_TRIANGLES, r_world_model->bsp->num_depth_pass_elements, GL_UNSIGNED_INT, NULL);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
}

/**
 * @brief
 */
void R_DrawDepthPass(const r_view_t *view) {

	if (!r_depth_pass->value) {
		return;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

	glUseProgram(r_depth_pass_program.name);

	R_DrawBspDepthPass(view);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glUseProgram(0);

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

	r_depth_pass_program.uniforms_block = glGetUniformBlockIndex(r_depth_pass_program.name, "uniforms_block");
	glUniformBlockBinding(r_depth_pass_program.name, r_depth_pass_program.uniforms_block, 0);

	r_depth_pass_program.in_position = glGetAttribLocation(r_depth_pass_program.name, "in_position");
	
	r_depth_pass_program.model = glGetUniformLocation(r_depth_pass_program.name, "model");

	glUseProgram(0);

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
