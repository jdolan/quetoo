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
	GLuint uniforms_block;

	GLint in_position;

	GLint model;
} r_depth_pass_program;

/**
 * @brief The occlusion queries.
 */
static struct {
	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint elements_buffer;
} r_occlusion_queries;

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
static void R_DrawOcclusionQueries(const r_view_t *view) {

	if (!r_occlude->integer) {
		return;
	}

	r_stats.count_bsp_occlusion_queries = r_world_model->bsp->num_occlusion_queries;

	glDepthMask(GL_FALSE);

	glBindVertexArray(r_occlusion_queries.vertex_array);
	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

	r_bsp_occlusion_query_t *q = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, q++) {

		if (Box3_ContainsPoint(Box3_Expand(q->bounds, 1.f), view->origin)) {
			q->pending = false;
			q->result = 1;
			continue;
		}

		if (R_CullBox(view, q->bounds)) {
			q->pending = false;
			q->result = 0;
			continue;
		}

		if (q->pending) {

			GLint available;
			glGetQueryObjectiv(q->name, GL_QUERY_RESULT_AVAILABLE, &available);

			if (available == GL_TRUE) {
				glGetQueryObjectiv(q->name, GL_QUERY_RESULT, &q->result);
			} else {
				continue;
			}
		}

		q->pending = true;

		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(q->vertexes), q->vertexes);

		glBeginQuery(GL_ANY_SAMPLES_PASSED, q->name);

		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLvoid *) 0);

		glEndQuery(GL_ANY_SAMPLES_PASSED);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glDepthMask(GL_TRUE);

	q = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, q++) {
		if (q->result) {
			r_stats.count_bsp_occlusion_queries_passed++;
		}
	}
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

	R_DrawOcclusionQueries(view);

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
_Bool R_OccludeBox(const r_view_t *view, const box3_t bounds) {

	if (!r_depth_pass->value) {
		return false;
	}

	if (!r_occlude->integer) {
		return false;
	}

	if (view->type == VIEW_PLAYER_MODEL) {
		return false;
	}

	const r_bsp_occlusion_query_t *q = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, q++) {
		
		if (!Box3_Contains(q->bounds, bounds)) {
			continue;
		}

		return q->result == 0;
	}

	return false;
}

/**
 * @brief
 */
_Bool R_OccludeSphere(const r_view_t *view, const vec3_t origin, float radius) {

	return R_OccludeBox(view, Box3_FromCenterRadius(origin, radius));
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
static void R_InitOcclusionQueries(void) {

	glGenVertexArrays(1, &r_occlusion_queries.vertex_array);
	glBindVertexArray(r_occlusion_queries.vertex_array);

	glGenBuffers(1, &r_occlusion_queries.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vec3_t[8]), NULL, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) 0);

	glGenBuffers(1, &r_occlusion_queries.elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

	const GLuint elements[] = {
		// bottom
		0, 1, 3, 0, 3, 2,
		// top
		6, 7, 4, 7, 5, 4,
		// front
		4, 5, 0, 5, 1, 0,
		// back
		7, 6, 3, 6, 2, 3,
		// left
		6, 4, 2, 4, 0, 2,
		// right
		5, 7, 1, 7, 3, 1,
	};

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownOcclusionQueries(void) {

	glDeleteVertexArrays(1, &r_occlusion_queries.vertex_array);

	glDeleteBuffers(1, &r_occlusion_queries.vertex_buffer);
	glDeleteBuffers(1, &r_occlusion_queries.elements_buffer);
}

/**
 * @brief
 */
void R_InitDepthPass(void) {

	R_InitDepthPassProgram();

	R_InitOcclusionQueries();
}

/**
 * @brief
 */
void R_ShutdownDepthPass(void) {

	R_ShutdownDepthPassProgram();

	R_ShutdownOcclusionQueries();
}
