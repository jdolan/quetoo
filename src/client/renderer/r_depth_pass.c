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

	GLuint names[MAX_OCCLUSION_QUERIES];

	vec3_t vertexes[MAX_OCCLUSION_QUERIES * 8];

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

	if (!r_draw_depth_pass->value) {
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
r_occlusion_query_t *R_OcclusionQuery(const vec3_t mins, const vec3_t maxs) {

	if (!r_occlusion_query->value) {
		return NULL;
	}

	if (r_view.num_occlusion_queries == MAX_OCCLUSION_QUERIES) {
		Com_Debug(DEBUG_RENDERER, "MAX_OCCLUSION_QUERIES\n");
		return NULL;
	}

	r_occlusion_query_t *q = &r_view.occlusion_queries[r_view.num_occlusion_queries];

	q->name = r_occlusion_queries.names[r_view.num_occlusion_queries];

	q->mins = mins;
	q->maxs = maxs;

	vec3_t *vertexes = &r_occlusion_queries.vertexes[r_view.num_occlusion_queries * 8];

	vertexes[0] = Vec3(q->mins.x, q->mins.y, q->mins.z);
	vertexes[1] = Vec3(q->maxs.x, q->mins.y, q->mins.z);
	vertexes[2] = Vec3(q->maxs.x, q->maxs.y, q->mins.z);
	vertexes[3] = Vec3(q->mins.x, q->maxs.y, q->mins.z);
	vertexes[4] = Vec3(q->mins.x, q->mins.y, q->maxs.z);
	vertexes[5] = Vec3(q->maxs.x, q->mins.y, q->maxs.z);
	vertexes[6] = Vec3(q->maxs.x, q->maxs.y, q->maxs.z);
	vertexes[7] = Vec3(q->mins.x, q->maxs.y, q->maxs.z);

	r_view.num_occlusion_queries++;

	return q;
}

/**
 * @brief
 */
void R_ExecuteOcclusionQueries(void) {

	if (!r_view.num_occlusion_queries) {
		return;
	}

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	//glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
	//glDepthMask(GL_FALSE);

	glUseProgram(r_depth_pass_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_occlusion_queries.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(r_occlusion_queries.vertexes), r_occlusion_queries.vertexes, GL_DYNAMIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

	glEnableVertexAttribArray(r_depth_pass_program.in_position);

	glUniformMatrix4fv(r_depth_pass_program.model, 1, GL_FALSE, (GLfloat *) matrix4x4_identity.m);

	r_occlusion_query_t *q = r_view.occlusion_queries;
	for (int32_t i = 0; i < r_view.num_occlusion_queries; i++, q++) {

		glBeginQuery(GL_ANY_SAMPLES_PASSED, q->name);

		const ptrdiff_t index = q - r_view.occlusion_queries;

		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (GLvoid *) (index * 36 * sizeof(GLuint)));

		glEndQuery(GL_ANY_SAMPLES_PASSED);
	}

	glBindVertexArray(0);

	glUseProgram(0);

	//glDepthMask(GL_TRUE);
	//glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

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
static void R_InitOcclusionQueries(void) {

	glGenQueries(lengthof(r_occlusion_queries.names), r_occlusion_queries.names);

	glGenVertexArrays(1, &r_occlusion_queries.vertex_array);
	glBindVertexArray(r_occlusion_queries.vertex_array);

	glGenBuffers(1, &r_occlusion_queries.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_occlusion_queries.vertex_buffer);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) 0);

	glGenBuffers(1, &r_occlusion_queries.elements_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, r_occlusion_queries.elements_buffer);

	GLuint elements[MAX_OCCLUSION_QUERIES * 36];
	for (GLuint i = 0, v = 0, e = 0; i < MAX_OCCLUSION_QUERIES; i++, v += 8, e += 36) {

		// bottom
		elements[e + 0] =  v + 0;
		elements[e + 1] =  v + 1;
		elements[e + 2] =  v + 2;
		elements[e + 3] =  v + 0;
		elements[e + 4] =  v + 2;
		elements[e + 5] =  v + 3;

		// top
		elements[e + 6] =  v + 7;
		elements[e + 7] =  v + 6;
		elements[e + 8] =  v + 4;
		elements[e + 9] =  v + 6;
		elements[e + 10] = v + 5;
		elements[e + 11] = v + 4;

		// front
		elements[e + 12] = v + 4;
		elements[e + 13] = v + 5;
		elements[e + 14] = v + 0;
		elements[e + 15] = v + 5;
		elements[e + 16] = v + 1;
		elements[e + 17] = v + 0;

		// back
		elements[e + 18] = v + 6;
		elements[e + 19] = v + 7;
		elements[e + 20] = v + 2;
		elements[e + 21] = v + 7;
		elements[e + 22] = v + 3;
		elements[e + 23] = v + 2;

		// left
		elements[e + 24] = v + 7;
		elements[e + 25] = v + 4;
		elements[e + 26] = v + 3;
		elements[e + 27] = v + 4;
		elements[e + 28] = v + 0;
		elements[e + 29] = v + 3;

		// right
		elements[e + 30] = v + 5;
		elements[e + 31] = v + 6;
		elements[e + 32] = v + 1;
		elements[e + 33] = v + 6;
		elements[e + 34] = v + 2;
		elements[e + 35] = v + 1;
	}

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(elements), elements, GL_STATIC_DRAW);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownOcclusionQueries(void) {

	glDeleteQueries(lengthof(r_occlusion_queries.names), r_occlusion_queries.names);

	glDeleteVertexArrays(1, &r_occlusion_queries.vertex_array);

	glDeleteBuffers(1, &r_occlusion_queries.vertex_buffer);
	glDeleteBuffers(1, &r_occlusion_queries.elements_buffer);

	memset(r_occlusion_queries.names, 0, sizeof(r_occlusion_queries.names));
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
