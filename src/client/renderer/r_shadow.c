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

#define SHADOWMAP_SIZE 384

/**
 * @brief The shadow program.
 */
static struct {
	GLuint name;
	GLuint uniforms_block;

	GLint in_position;
	//GLint in_next_position;

	GLint model;

	GLint cubemap_layer;
	GLint cubemap_view;
	GLint cubemap_projection;
} r_shadow_program;

/**
 * @brief The shadows.
 */
static struct {
	/**
	 * @brief Each light source targets a layer in the cubemap array.
	 */
	GLuint cubemap_array;

	/**
	 * @brief Each light source has a framebuffer to capture its depth pass.
	 */
	GLuint framebuffers[MAX_SHADOWS];
} r_shadows;

/**
 * @brief
 */
static void R_DrawMeshFaceShadow(const r_entity_t *e, const r_mesh_model_t *mesh, const r_mesh_face_t *face) {

	const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));
	
//	const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
//	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));
	
	const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
	glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);
}

/**
 * @brief
 */
static void R_DrawMeshEntityShadow(const r_view_t *view, const r_entity_t *e) {

	const r_mesh_model_t *mesh = e->model->mesh;
	assert(mesh);

	glBindVertexArray(mesh->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elements_buffer);

	glEnableVertexAttribArray(r_shadow_program.in_position);
	//glEnableVertexAttribArray(r_shadow_program.in_next_position);
	
	glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, e->matrix.array);

	//glUniform1f(r_mesh_program.lerp, e->lerp);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	{
		const r_mesh_face_t *face = mesh->faces;
		for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

			const r_material_t *material = e->skins[i] ?: face->material;
			if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
				continue;
			}

			R_DrawMeshFaceShadow(e, mesh, face);
		}
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);
}

/**
 * @brief Draws a single light source shadow from the provided sparse view.
 */
static void R_DrawShadowView(const r_view_t *view) {

	glViewport(view->viewport.x, view->viewport.y, view->viewport.z, view->viewport.w);

	glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

	for (int32_t i = 0; i < 6; i++) {
		glFramebufferTextureLayer(GL_FRAMEBUFFER,
								  GL_DEPTH_ATTACHMENT,
								  r_shadows.cubemap_array,
								  0,
								  view->tag * 6 + i);

		glClear(GL_DEPTH_BUFFER_BIT);
	}

	glFramebufferTexture(GL_FRAMEBUFFER,
						 GL_DEPTH_ATTACHMENT,
						 r_shadows.cubemap_array,
						 0);

	glUniform1i(r_shadow_program.cubemap_layer, view->tag);

	const float fov = tanf(Radians(view->fov.x / 2.f));
	const float near = view->depth_range.x, far = view->depth_range.y;

	const mat4_t cubemap_projection = Mat4_FromFrustum(-fov, fov, -fov, fov, near, far);

	glUniformMatrix4fv(r_shadow_program.cubemap_projection, 1, GL_FALSE, cubemap_projection.array);

	const mat4_t cubemap_view[6] = {
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 1.f,  0.f,  0.f)), Vec3(0.f, -1.f,  0.f)),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3(-1.f,  0.f,  0.f)), Vec3(0.f, -1.f,  0.f)),

		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  1.f,  0.f)), Vec3(0.f,  0.f,  1.f)),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f, -1.f,  0.f)), Vec3(0.f,  0.f, -1.f)),

		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  0.f,  1.f)), Vec3(0.f, -1.f,  0.f)),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  0.f, -1.f)), Vec3(0.f, -1.f,  0.f)),
	};

	glUniformMatrix4fv(r_shadow_program.cubemap_view, 6, GL_FALSE, (GLfloat *) cubemap_view);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (IS_MESH_MODEL(e->model)) {
			R_DrawMeshEntityShadow(view, e);
		}
	}

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_DrawShadows(const r_view_t *view) {

	if (!r_shadowmap->value) {
		return;
	}

	glUseProgram(r_shadow_program.name);

	const r_light_uniform_t *l = r_lights.block.lights;
	for (int32_t i = 0; i < r_lights.block.num_lights; i++, l++) {

		r_view_t v = {
			.framebuffer = &(r_framebuffer_t) {
				.name = r_shadows.framebuffers[i],
				.width = SHADOWMAP_SIZE,
				.height = SHADOWMAP_SIZE,
			},
			.viewport = Vec4i(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE),
			.fov = Vec2(90.f, 90.f),
			.depth_range = Vec2(NEAR_DIST, MAX_WORLD_DIST),
			.origin = Vec4_XYZ(l->model),
			.ticks = view->ticks,
			.tag = i,
		};

		const box3_t bounds = Box3_FromCenterRadius(Vec4_XYZ(l->model), l->model.w);

		if (R_CulludeBox(view, bounds)) {
			continue;
		}

		const r_entity_t *e = view->entities;
		for (int32_t j = 0; j < view->num_entities; j++, e++) {

			if (!e->model) {
				continue;
			}

			if (!Box3_Intersects(bounds, e->abs_model_bounds)) {
				continue;
			}

			v.entities[v.num_entities++] = *e;
		}

		R_DrawShadowView(&v);
	}

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowProgram(void) {

	memset(&r_shadow_program, 0, sizeof(r_shadow_program));

	r_shadow_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "shadow_vs.glsl", NULL),
			R_ShaderDescriptor(GL_GEOMETRY_SHADER, "shadow_gs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "shadow_fs.glsl", NULL),
			NULL);

	glUseProgram(r_shadow_program.name);

	r_shadow_program.uniforms_block = glGetUniformBlockIndex(r_shadow_program.name, "uniforms_block");
	glUniformBlockBinding(r_shadow_program.name, r_shadow_program.uniforms_block, 0);

	r_shadow_program.in_position = glGetAttribLocation(r_shadow_program.name, "in_position");
	//r_shadow_program.in_next_position = glGetAttribLocation(r_shadow_program.name, "in_next_position");

	r_shadow_program.model = glGetUniformLocation(r_shadow_program.name, "model");

	r_shadow_program.cubemap_layer = glGetUniformLocation(r_shadow_program.name, "cubemap_layer");
	r_shadow_program.cubemap_view = glGetUniformLocation(r_shadow_program.name, "cubemap_view");
	r_shadow_program.cubemap_projection = glGetUniformLocation(r_shadow_program.name, "cubemap_projection");

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowTexture(void) {

	glGenTextures(1, &r_shadows.cubemap_array);
	
	glActiveTexture(GL_TEXTURE0 + TEXTURE_SHADOWMAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, r_shadows.cubemap_array);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, MAX_SHADOWS * 6, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowFramebuffers(void) {

	glGenFramebuffers(MAX_SHADOWS, r_shadows.framebuffers);

	for (int32_t i = 0; i < MAX_SHADOWS; i++) {

		glBindFramebuffer(GL_FRAMEBUFFER, r_shadows.framebuffers[i]);

		for (int32_t j = 0; j < 6; j++) {
			glFramebufferTextureLayer(
									  GL_FRAMEBUFFER,
									  GL_DEPTH_ATTACHMENT,
									  r_shadows.cubemap_array,
									  0,
									  i * 6 + j);
			glClear(GL_DEPTH_BUFFER_BIT);
		}

		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);

		const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE) {
			Com_Error(ERROR_FATAL, "Failed to create shadow framebuffer: %d\n", status);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_InitShadows(void) {

	R_InitShadowProgram();

	R_InitShadowTexture();

	R_InitShadowFramebuffers();
}

/**
 * @brief
 */
static void R_ShutdownShadowProgram(void) {

	glDeleteProgram(r_shadow_program.name);

	r_shadow_program.name = 0;

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowTexture(void) {

	glDeleteTextures(1, &r_shadows.cubemap_array);

	r_shadows.cubemap_array = 0;

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowFramebuffers(void) {

	glDeleteFramebuffers(MAX_SHADOWS, r_shadows.framebuffers);

	memset(r_shadows.framebuffers, 0, sizeof(r_shadows.framebuffers));

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_ShutdownShadows(void) {

	R_ShutdownShadowProgram();

	R_ShutdownShadowTexture();

	R_ShutdownShadowFramebuffers();
}
