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
 * @brief The occlusion queries.
 */
static struct {
	GLuint vertex_array;
	GLuint vertex_buffer;
	GLuint elements_buffer;
} r_occlusion_queries;

/**
 * @brief Draws all opaque world geometry, writing to the depth buffer.
 */
static void R_DrawBspInlineModelDepthPass(const r_entity_t *e, const r_bsp_inline_model_t *in) {

	const r_bsp_draw_elements_t *draw = in->draw_elements;
	for (int32_t i = 0; i < in->num_draw_elements; i++, draw++) {

		if (draw->texinfo->flags & SURF_MASK_TRANSLUCENT) {
			continue;
		}
		
		glDrawElements(GL_TRIANGLES, draw->num_elements, GL_UNSIGNED_INT, draw->elements);
	}

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_DrawDepthPass(void) {

	if (!r_depth_pass->value) {
		return;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glEnable(GL_POLYGON_OFFSET_FILL);
	glPolygonOffset(1.f, 1.f);

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

	glPolygonOffset(0.f, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	if (r_occlude->value) {
		glDepthMask(GL_FALSE);

		glBindVertexArray(r_occlusion_queries.vertex_array);

		glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

		glEnableVertexAttribArray(r_depth_pass_program.in_position);

		glUniformMatrix4fv(r_depth_pass_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);

		r_bsp_occlusion_query_t *q = r_world_model->bsp->occlusion_queries;
		for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, q++) {

			q->result = -1;

			if (Vec3_BoxIntersect(r_view.origin, r_view.origin, q->mins, q->maxs)) {
				continue;
			}

			if (R_CullBox(q->mins, q->maxs)) {
				continue;
			}


			glBufferData(GL_ARRAY_BUFFER, sizeof(q->vertexes), q->vertexes, GL_DYNAMIC_DRAW);

			glBeginQuery(GL_ANY_SAMPLES_PASSED, q->name);

			glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLvoid *) 0);

			glEndQuery(GL_ANY_SAMPLES_PASSED);

			r_view.count_bsp_occlusion_queries++;

			glGetQueryObjectiv(q->name, GL_QUERY_RESULT, &q->result);

			if (q->result) {
				r_view.count_bsp_occlusion_queries_passed++;
			}
		}

		glDepthMask(GL_TRUE);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	glUseProgram(0);

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);
}

/**
 * @brief
 */
_Bool R_OccludeBox(const vec3_t mins, const vec3_t maxs) {

	if (!r_occlude->value) {
		return false;
	}

	const r_bsp_occlusion_query_t *q = r_world_model->bsp->occlusion_queries;
	for (int32_t i = 0; i < r_world_model->bsp->num_occlusion_queries; i++, q++) {

		if (q->result == -1) {
			continue;
		}

		int32_t j;
		for (j = 0; j < 3; j++) {
			if (mins.xyz[j] < q->mins.xyz[j] || maxs.xyz[j] > q->maxs.xyz[j]) {
				break;
			}
		}

		if (j < 3) {
			continue;
		}

		if (q->result == 0) {
			return true;
		}
	}

	return false;
}

/**
 * @brief
 */
_Bool R_OccludeSphere(const vec3_t origin, float radius) {

	const vec3_t size = Vec3(radius, radius, radius);

	return R_OccludeBox(Vec3_Subtract(origin, size), Vec3_Add(origin, size));
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
static void R_InitOcclusionQueries(void) {

	glGenVertexArrays(1, &r_occlusion_queries.vertex_array);
	glBindVertexArray(r_occlusion_queries.vertex_array);

	glGenBuffers(1, &r_occlusion_queries.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) 0);

	glGenBuffers(1, &r_occlusion_queries.elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

	const GLuint elements[36] = {
		// bottom
		0, 1, 2, 0, 2, 3,
		// top
		7, 6, 4, 6, 5, 4,
		// front
		4, 5, 0, 5, 1, 0,
		// back
		6, 7, 2, 7, 3, 2,
		// left
		7, 4, 3, 4, 0, 3,
		// right
		5, 6, 1, 6, 2, 1,
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
