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

	GLuint uniforms;

	GLint in_position;

	GLint in_next_position;

	GLint model;

	GLint lerp;

	GLint texture_lightgrid_fog;

	GLint z;
	GLint min_z;
	GLint max_z;

	GLint depth_range;
	GLint inv_viewport_size;
	GLint transition_size;
	GLint depth_stencil_attachment;
	
	GLint dist;
	GLint normal;

	GLuint framebuffer;

	r_image_t *color_attachment;
	r_image_t *color_attachment1;
} r_mesh_shadow_program;

/**
 * @brief Blur program.
 */
static struct {
	GLuint name;

	GLuint uniforms;

	GLint in_position;
	GLint in_diffusemap;
	
	GLint texture_diffusemap;
	GLint resolution;
	GLint direction;

	GLint depth_range;
	GLint inv_viewport_size;
	GLint transition_size;
	GLint depth_stencil_attachment;

	GLuint vertex_array;
	GLuint vertex_buffer;
} r_mesh_shadow_blur_program;

/**
 * @brief Vertex struct.
 */
typedef struct {
	vec2s_t position;
	vec2_t diffusemap;
} r_mesh_2d_blur_vertex_t;

/**
 * @brief
 */
void R_UpdateMeshEntitiesShadows(void) {

	if (!r_shadows->value) {
		return;
	}

	R_GetError(NULL);
}


/**
 * @brief
 */
static void R_DrawMeshShadowEntity(const r_entity_t *e) {

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

	const cm_trace_t zero_tr = Cl_Trace(root->origin, Vec3_Add(root->origin, Vec3(0, 0, -MAX_WORLD_DIST)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_SOLID);

	for (uint64_t i = 0; i < ((r_shadows->value > 1) ? lengthof(offsets) : 1); i++) {
		const cm_trace_t tr = (i == 0) ? zero_tr : Cl_Trace(Vec3_Add(root->origin, offsets[i]), Vec3_Add(Vec3_Add(root->origin, offsets[i]), Vec3(0, 0, -MAX_WORLD_DIST)), Vec3_Zero(), Vec3_Zero(), 0, CONTENTS_SOLID);

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
		
			r_view.count_mesh_triangles += face->num_elements / 3;
		}
	}

	glBindVertexArray(0);

	r_view.count_mesh_models++;
}

/**
 * @brief Draws the actual 3d piece projected onto the ground to the main color
 * attachment of the shadow framebuffer
 */
static _Bool R_DrawMeshEntitiesShadowsProjected(int32_t blend_depth) {

	_Bool any_rendered = false;

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (IS_MESH_MODEL(e->model)) {

			if (e->effects & EF_NO_SHADOW) {
				continue;
			}

			if (e->blend_depth != blend_depth) {
				continue;
			}

			if (!any_rendered) {
	
				glBindFramebuffer(GL_FRAMEBUFFER, r_mesh_shadow_program.framebuffer);

				glDrawBuffers(2, (const GLenum[]) { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

				glClear(GL_COLOR_BUFFER_BIT);
	
				glDrawBuffers(1, (const GLenum[]) { GL_COLOR_ATTACHMENT0 });

				glEnable(GL_DEPTH_TEST);

				glColorMask(false, false, false, true);

				glDepthMask(false);

				glEnable(GL_BLEND);

				glBlendEquation(GL_MAX);

				glUseProgram(r_mesh_shadow_program.name);

				glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID_FOG);
				glBindTexture(GL_TEXTURE_3D, r_world_model->bsp->lightgrid->textures[3]->texnum);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);

				any_rendered = true;
			}
			
			R_DrawMeshShadowEntity(e);
		}
	}

	if (any_rendered) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		glDepthMask(true);

		glColorMask(true, true, true, true);

		glDisable(GL_BLEND);

		glBlendEquation(GL_FUNC_ADD);

		glDisable(GL_DEPTH_TEST);
	
		R_GetError(NULL);
	}

	return any_rendered;
}

/**
 * @brief 
*/
static void R_DrawMeshShadowBlit(GLuint in_texture_id, GLenum out_attachment_id, vec2_t direction) {

	glDrawBuffers(1, &out_attachment_id);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, in_texture_id);
	glActiveTexture(GL_TEXTURE0 + 1);
	glBindTexture(GL_TEXTURE_2D, r_context.depth_stencil_attachment);
	
	glUniform2f(r_mesh_shadow_blur_program.direction, direction.x, direction.y);

	glDepthMask(false);

	glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

	glDepthMask(true);
	
	R_GetError(NULL);
}

/**
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshEntitiesShadows(int32_t blend_depth) {

	if (!r_shadows->value) {
		return;
	}

	if (!R_DrawMeshEntitiesShadowsProjected(blend_depth)) {
		return;
	}
	
	glUseProgram(r_mesh_shadow_blur_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_uniforms.buffer);

	glBindVertexArray(r_mesh_shadow_blur_program.vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, r_mesh_shadow_blur_program.vertex_buffer);

	glEnableVertexAttribArray(r_mesh_shadow_blur_program.in_position);
	
	glEnableVertexAttribArray(r_mesh_shadow_blur_program.in_diffusemap);

	glEnable(GL_BLEND);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

	// blit to special blur texture
	R_DrawMeshShadowBlit(r_mesh_shadow_program.color_attachment->texnum, GL_COLOR_ATTACHMENT1, Vec2(1, 0));

	// blit to screen framebuffer
	R_DrawMeshShadowBlit(r_mesh_shadow_program.color_attachment1->texnum, GL_COLOR_ATTACHMENT2, Vec2(0, 1));

	glBlendFunc(GL_ONE, GL_ZERO);

	glDisable(GL_BLEND);

	glActiveTexture(GL_TEXTURE0);
	
	glUseProgram(0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, r_context.framebuffer);
	
	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitMeshShadowProgram(void) {
	
	memset(&r_mesh_shadow_program, 0, sizeof(r_mesh_shadow_program));

	r_mesh_shadow_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "common_vs.glsl", "mesh_shadow_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "common_fs.glsl", "soften_fs.glsl", "mesh_shadow_fs.glsl"),
			NULL);

	glUseProgram(r_mesh_shadow_program.name);

	r_mesh_shadow_program.uniforms = glGetUniformBlockIndex(r_mesh_shadow_program.name, "uniforms");
	glUniformBlockBinding(r_mesh_shadow_program.name, r_mesh_shadow_program.uniforms, 0);

	r_mesh_shadow_program.in_position = glGetAttribLocation(r_mesh_shadow_program.name, "in_position");
	r_mesh_shadow_program.in_next_position = glGetAttribLocation(r_mesh_shadow_program.name, "in_next_position");

	r_mesh_shadow_program.model = glGetUniformLocation(r_mesh_shadow_program.name, "model");
	
	r_mesh_shadow_program.lerp = glGetUniformLocation(r_mesh_shadow_program.name, "lerp");

	r_mesh_shadow_program.texture_lightgrid_fog = glGetUniformLocation(r_mesh_shadow_program.name, "texture_lightgrid_fog");

	r_mesh_shadow_program.z = glGetUniformLocation(r_mesh_shadow_program.name, "z");
	r_mesh_shadow_program.min_z = glGetUniformLocation(r_mesh_shadow_program.name, "min_z");
	r_mesh_shadow_program.max_z = glGetUniformLocation(r_mesh_shadow_program.name, "max_z");
	r_mesh_shadow_program.depth_stencil_attachment = glGetUniformLocation(r_mesh_shadow_program.name, "depth_stencil_attachment");
	r_mesh_shadow_program.depth_range = glGetUniformLocation(r_mesh_shadow_program.name, "depth_range");
	r_mesh_shadow_program.inv_viewport_size = glGetUniformLocation(r_mesh_shadow_program.name, "inv_viewport_size");
	r_mesh_shadow_program.transition_size = glGetUniformLocation(r_mesh_shadow_program.name, "transition_size");
	
	r_mesh_shadow_program.dist = glGetUniformLocation(r_mesh_shadow_program.name, "dist");
	r_mesh_shadow_program.normal = glGetUniformLocation(r_mesh_shadow_program.name, "normal");

	glUniform1f(r_mesh_shadow_program.max_z, 1.f / 512.f);

	glUniform2f(r_mesh_shadow_program.depth_range, 1.0, MAX_WORLD_DIST);
	glUniform2f(r_mesh_shadow_program.inv_viewport_size, 1.0 / r_context.drawable_width, 1.0 / r_context.drawable_height);
	glUniform1f(r_mesh_shadow_program.transition_size, 0.0016f);

	glUniform1i(r_mesh_shadow_program.depth_stencil_attachment, 0);

	glUniform1i(r_mesh_shadow_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);

	glUseProgram(0);

	glGenFramebuffers(1, &r_mesh_shadow_program.framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, r_mesh_shadow_program.framebuffer);

	R_GetError("Make framebuffer");

	R_CreateImage(&r_mesh_shadow_program.color_attachment, "__shadow_attachment", r_context.drawable_width, r_context.drawable_height, IT_PROGRAM);
	R_UploadImage(r_mesh_shadow_program.color_attachment, GL_RGBA, NULL);

	glBindTexture(GL_TEXTURE_2D, r_mesh_shadow_program.color_attachment->texnum);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r_mesh_shadow_program.color_attachment->texnum, 0);
	
	R_CreateImage(&r_mesh_shadow_program.color_attachment1, "__shadow_blur_attachment", r_context.drawable_width, r_context.drawable_height, IT_PROGRAM);
	R_UploadImage(r_mesh_shadow_program.color_attachment1, GL_RGBA, NULL);

	glBindTexture(GL_TEXTURE_2D, r_mesh_shadow_program.color_attachment1->texnum);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, r_mesh_shadow_program.color_attachment1->texnum, 0);

	glBindTexture(GL_TEXTURE_2D, r_context.color_attachment);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, r_context.color_attachment, 0);
	
	R_GetError("Color attachment");

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, r_context.depth_stencil_attachment, 0);

	R_GetError("Depth stencil attachment");
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glBindTexture(GL_TEXTURE_2D, 0);

	memset(&r_mesh_shadow_blur_program, 0, sizeof(r_mesh_shadow_blur_program));

	r_mesh_shadow_blur_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "common_vs.glsl", "draw_2d_blur_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "common_fs.glsl", "soften_fs.glsl", "draw_2d_blur_fs.glsl"),
			NULL);

	glUseProgram(r_mesh_shadow_blur_program.name);

	r_mesh_shadow_blur_program.uniforms = glGetUniformBlockIndex(r_mesh_shadow_blur_program.name, "uniforms");
	glUniformBlockBinding(r_mesh_shadow_blur_program.name, r_mesh_shadow_blur_program.uniforms, 0);
	
	r_mesh_shadow_blur_program.in_position = glGetAttribLocation(r_mesh_shadow_blur_program.name, "in_position");
	r_mesh_shadow_blur_program.in_diffusemap = glGetAttribLocation(r_mesh_shadow_blur_program.name, "in_diffusemap");
	
	r_mesh_shadow_blur_program.texture_diffusemap = glGetUniformLocation(r_mesh_shadow_blur_program.name, "texture_diffusemap");
	r_mesh_shadow_blur_program.resolution = glGetUniformLocation(r_mesh_shadow_blur_program.name, "resolution");
	r_mesh_shadow_blur_program.direction = glGetUniformLocation(r_mesh_shadow_blur_program.name, "direction");

	r_mesh_shadow_blur_program.depth_stencil_attachment = glGetUniformLocation(r_mesh_shadow_blur_program.name, "depth_stencil_attachment");
	r_mesh_shadow_blur_program.depth_range = glGetUniformLocation(r_mesh_shadow_blur_program.name, "depth_range");
	r_mesh_shadow_blur_program.inv_viewport_size = glGetUniformLocation(r_mesh_shadow_blur_program.name, "inv_viewport_size");
	r_mesh_shadow_blur_program.transition_size = glGetUniformLocation(r_mesh_shadow_blur_program.name, "transition_size");

	glUniform2f(r_mesh_shadow_blur_program.depth_range, 1.0, MAX_WORLD_DIST);
	glUniform2f(r_mesh_shadow_blur_program.inv_viewport_size, 1.0 / r_context.drawable_width, 1.0 / r_context.drawable_height);
	glUniform1f(r_mesh_shadow_blur_program.transition_size, 0.0016f);

	glGenVertexArrays(1, &r_mesh_shadow_blur_program.vertex_array);
	glBindVertexArray(r_mesh_shadow_blur_program.vertex_array);

	glGenBuffers(1, &r_mesh_shadow_blur_program.vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, r_mesh_shadow_blur_program.vertex_buffer);

	glVertexAttribPointer(0, 2, GL_SHORT, GL_FALSE, sizeof(r_mesh_2d_blur_vertex_t), (void *) offsetof(r_mesh_2d_blur_vertex_t, position));
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_2d_blur_vertex_t), (void *) offsetof(r_mesh_2d_blur_vertex_t, diffusemap));

	r_mesh_2d_blur_vertex_t quad[4];
	
	quad[0].position = Vec2s(0, 0);
	quad[1].position = Vec2s(r_context.drawable_width, 0);
	quad[2].position = Vec2s(r_context.drawable_width, r_context.drawable_height);
	quad[3].position = Vec2s(0, r_context.drawable_height);
	
	quad[0].diffusemap = Vec2(0, 0);
	quad[1].diffusemap = Vec2(1, 0);
	quad[2].diffusemap = Vec2(1, 1);
	quad[3].diffusemap = Vec2(0, 1);

	glBufferData(GL_ARRAY_BUFFER, lengthof(quad) * sizeof(r_mesh_2d_blur_vertex_t), quad, GL_STATIC_DRAW);

	glUniform2f(r_mesh_shadow_blur_program.resolution, r_context.drawable_width, r_context.drawable_height);
	glUniform1i(r_mesh_shadow_blur_program.texture_diffusemap, 0);
	glUniform1i(r_mesh_shadow_blur_program.depth_stencil_attachment, 1);

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

