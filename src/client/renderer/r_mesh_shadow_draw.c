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
#include "cl_local.h"

/**
 * @brief The program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;

	GLint in_position;

	GLint in_next_position;

	GLint model;

	GLint lerp;

	GLint texture_lightgrid_fog;

	GLint z;
	GLint min_z;
	GLint max_z;

	GLint dist;
	GLint normal;
} r_mesh_shadow_program;

/**
 * @brief
 */
static void R_DrawMeshEntityShadow(const r_entity_t *e) {

	const r_mesh_model_t *mesh = e->model->mesh;
	assert(mesh);

	glBindVertexArray(mesh->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elements_buffer);

	glEnableVertexAttribArray(r_mesh_shadow_program.in_position);

	glEnableVertexAttribArray(r_mesh_shadow_program.in_next_position);
	
	glUniform1f(r_mesh_shadow_program.lerp, e->lerp);

	const r_entity_t *root;

	for (root = e; root->parent; root = root->parent) ;

	const vec3_t offsets[] = {
		{ .x = 0, .y = 0, .z = 0 },
		{ .x = 32, .y = 0, .z = 0 },
		{ .x = 0, .y = 32, .z = 0 },
		{ .x = -32, .y = 0, .z = 0 },
		{ .x = 0, .y = -32, .z = 0 },
		{ .x = 32, .y = 32, .z = 0 },
		{ .x = -32, .y = 32, .z = 0 },
		{ .x = -32, .y = -32, .z = 0 },
		{ .x = 32, .y = -32, .z = 0 }
	};

	glUniform1f(r_mesh_shadow_program.min_z, e->model->mins.z);

	mat4_t model;

	const cm_trace_t zero_tr = Cl_Trace(root->origin, Vec3_Add(root->origin, Vec3(0, 0, -MAX_WORLD_DIST)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_VISIBLE);

	for (uint64_t i = 0; i < ((r_shadows->value > 1) ? lengthof(offsets) : 1); i++) {
		const cm_trace_t tr = (i == 0) ? zero_tr : Cl_Trace(Vec3_Add(root->origin, offsets[i]), Vec3_Add(Vec3_Add(root->origin, offsets[i]), Vec3(0, 0, -MAX_WORLD_DIST)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_MASK_VISIBLE);

		if (i != 0 && fabsf(tr.end.z - zero_tr.end.z) < TRACE_EPSILON) {
			continue;
		}

		Matrix4x4_CreateIdentity(&model);

		Matrix4x4_ConcatTranslate(&model, tr.end.x, tr.end.y, tr.end.z);

		Matrix4x4_ConcatScale3(&model, 1.f, 1.f, 0.f);
		
		Matrix4x4_ConcatTranslate(&model, -tr.end.x, -tr.end.y, -tr.end.z);

		Matrix4x4_Concat(&model, &model, &e->matrix);

		glUniformMatrix4fv(r_mesh_shadow_program.model, 1, GL_FALSE, (GLfloat *) model.m);
		
		glUniform1f(r_mesh_shadow_program.dist, Vec3_Distance(e->origin, tr.end));
	
		const r_mesh_face_t *face = mesh->faces;
		for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

			const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));

			const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

			glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));

			const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
			glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);
		
			r_stats.count_mesh_triangles += face->num_elements / 3;
		}
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	r_stats.count_mesh_models++;
}

/**
 * @brief Draws the actual 3d piece projected onto the ground to the main color
 * attachment of the shadow framebuffer
 */
static void R_DrawMeshEntitiesShadowsProjected(const r_view_t *view, int32_t blend_depth) {
	_Bool any_rendered = false;

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_MESH_MODEL(e->model)) {

			if (e->effects & EF_NO_SHADOW) {
				continue;
			}

			if (e->blend_depth != blend_depth) {
				continue;
			}

			if (!any_rendered) {

				glEnable(GL_DEPTH_TEST);

				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glDepthMask(false);

				glUseProgram(r_mesh_shadow_program.name);

				glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
				glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

				any_rendered = true;
			}
			
			R_DrawMeshEntityShadow(e);
		}
	}

	if (any_rendered) {
		glDisable(GL_DEPTH_TEST);

		glDisable(GL_BLEND);
		
		glDepthMask(true);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

		R_GetError(NULL);
	}
}

/**
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshEntitiesShadows(const r_view_t *view, int32_t blend_depth) {

	if (!r_shadows->value) {
		return;
	}

	R_DrawMeshEntitiesShadowsProjected(view, blend_depth);
}

/**
 * @brief
 */
void R_InitMeshShadowProgram(void) {
	
	memset(&r_mesh_shadow_program, 0, sizeof(r_mesh_shadow_program));

	r_mesh_shadow_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "mesh_shadow_vs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "soften_fs.glsl", "mesh_shadow_fs.glsl", NULL),
			NULL);

	glUseProgram(r_mesh_shadow_program.name);

	r_mesh_shadow_program.uniforms_block = glGetUniformBlockIndex(r_mesh_shadow_program.name, "uniforms_block");
	glUniformBlockBinding(r_mesh_shadow_program.name, r_mesh_shadow_program.uniforms_block, 0);

	r_mesh_shadow_program.in_position = glGetAttribLocation(r_mesh_shadow_program.name, "in_position");
	r_mesh_shadow_program.in_next_position = glGetAttribLocation(r_mesh_shadow_program.name, "in_next_position");

	r_mesh_shadow_program.model = glGetUniformLocation(r_mesh_shadow_program.name, "model");
	
	r_mesh_shadow_program.lerp = glGetUniformLocation(r_mesh_shadow_program.name, "lerp");

	r_mesh_shadow_program.texture_lightgrid_fog = glGetUniformLocation(r_mesh_shadow_program.name, "texture_lightgrid_fog");

	r_mesh_shadow_program.z = glGetUniformLocation(r_mesh_shadow_program.name, "z");
	r_mesh_shadow_program.min_z = glGetUniformLocation(r_mesh_shadow_program.name, "min_z");
	r_mesh_shadow_program.max_z = glGetUniformLocation(r_mesh_shadow_program.name, "max_z");

	r_mesh_shadow_program.dist = glGetUniformLocation(r_mesh_shadow_program.name, "dist");
	r_mesh_shadow_program.normal = glGetUniformLocation(r_mesh_shadow_program.name, "normal");

	glUniform1f(r_mesh_shadow_program.max_z, 1.f / 512.f);

	glUniform1i(r_mesh_shadow_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);

	glUseProgram(0);

	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glBindVertexArray(0);
	
	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownMeshShadowProgram(void) {

	glDeleteProgram(r_mesh_shadow_program.name);

	r_mesh_shadow_program.name = 0;
}

