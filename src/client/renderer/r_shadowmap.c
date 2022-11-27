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

#define SHADOWMAP_SIZE 1024

/**
 * @brief The shadowmap program.
 */
static struct {
	GLuint name;
	GLuint uniforms_block;

	GLint in_position;
	//GLint in_next_position;
	
	GLint model;

	GLint cubemap_view;
	GLint cubemap_projection;

	GLint origin;
} r_shadowmap_program;

r_shadowmaps_t r_shadowmaps;

/**
 * @brief
 */
static void R_DrawMeshFaceShadow(const r_entity_t *e, const r_mesh_model_t *mesh, const r_mesh_face_t *face) {

	const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));
	
	const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));
	
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

	glEnableVertexAttribArray(r_shadowmap_program.in_position);
	//glEnableVertexAttribArray(r_shadowmap_program.in_next_position);
	
	glUniformMatrix4fv(r_shadowmap_program.model, 1, GL_FALSE, e->matrix.array);

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
static void R_DrawShadowmapView(const r_view_t *view) {

	if (!view->num_entities) {
		return;
	}

	glViewport(0, 0, view->framebuffer->width, view->framebuffer->height);

	glBindFramebuffer(GL_FRAMEBUFFER, view->framebuffer->name);

	glClear(GL_DEPTH_BUFFER_BIT);

	const float fov = tanf(Radians(view->fov.x / 2.f));
	const mat4_t cubemap_projection = Mat4_FromFrustum(-fov, fov, -fov, fov, NEAR_DIST, MAX_WORLD_DIST);

	glUniformMatrix4fv(r_shadowmap_program.cubemap_projection, 1, GL_FALSE, cubemap_projection.array);

	mat4_t cubemap_view[6] = {
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 1.f,  0.f,  0.f)), Vec3_Up()),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3(-1.f,  0.f,  0.f)), Vec3_Up()),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  0.f,  1.f)), Vec3(0, 1, 0)),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  0.f, -1.f)), Vec3(0, -1, 0)),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f,  1.f,  0.f)), Vec3_Up()),
		Mat4_LookAt(view->origin, Vec3_Add(view->origin, Vec3( 0.f, -1.f,  0.f)), Vec3_Up()),
	};

	glUniformMatrix4fv(r_shadowmap_program.cubemap_view, 6, GL_FALSE, (GLfloat *) cubemap_view);

	glUniform3fv(r_shadowmap_program.origin, 1, view->origin.xyz);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (IS_MESH_MODEL(e->model)) {
			R_DrawMeshEntityShadow(view, e);
		}
	}
}

/**
 * @brief 
 * @param view 
 */
void R_DrawShadowmaps(const r_view_t *view) {

	glUseProgram(r_shadowmap_program.name);

	const r_light_t *l = view->lights;
	for (int32_t i = 0; i < view->num_lights; i++, l++) {

		r_framebuffer_t fb = {
			.name = r_shadowmaps.framebuffers[i],
			.width = SHADOWMAP_SIZE,
			.height = SHADOWMAP_SIZE
		};

		r_view_t v = {
			.type = VIEW_SHADOWMAP,
			.origin = l->origin,
			.framebuffer = &fb,
			.fov = Vec2(90.f, 90.f)
		};

		const box3_t bounds = Box3_FromCenterRadius(l->origin, l->radius);
		const r_entity_t *e = view->entities;
		for (int32_t j = 0; j < view->num_entities; j++, e++) {

			if (!IS_MESH_MODEL(e->model)) {
				continue;
			}

			if (!Box3_Intersects(bounds, e->abs_model_bounds)) {
				continue;
			}

			v.entities[v.num_entities++] = *e;
		}

		R_DrawShadowmapView(&v);
	}

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowmapProgram(void) {

	memset(&r_shadowmap_program, 0, sizeof(r_shadowmap_program));

	r_shadowmap_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "shadowmap_vs.glsl", NULL),
			R_ShaderDescriptor(GL_GEOMETRY_SHADER, "shadowmap_gs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "shadowmap_fs.glsl", NULL),
			NULL);

	glUseProgram(r_shadowmap_program.name);

	r_shadowmap_program.uniforms_block = glGetUniformBlockIndex(r_shadowmap_program.name, "uniforms_block");
	glUniformBlockBinding(r_shadowmap_program.name, r_shadowmap_program.uniforms_block, 0);

	r_shadowmap_program.in_position = glGetAttribLocation(r_shadowmap_program.name, "in_position");
	//r_shadowmap_program.in_next_position = glGetAttribLocation(r_shadowmap_program.name, "in_next_position");

	r_shadowmap_program.model = glGetUniformLocation(r_shadowmap_program.name, "model");

	r_shadowmap_program.cubemap_view = glGetUniformLocation(r_shadowmap_program.name, "cubemap_view");
	r_shadowmap_program.cubemap_projection = glGetUniformLocation(r_shadowmap_program.name, "cubemap_projection");

	r_shadowmap_program.origin = glGetUniformLocation(r_shadowmap_program.name, "origin");
	
	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowmapTexture(void) {

	glGenTextures(1, &r_shadowmaps.cubemap_array);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, r_shadowmaps.cubemap_array);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, SHADOWMAP_SIZE, SHADOWMAP_SIZE, MAX_LIGHTS * 6, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowmapFramebuffers(void) {

	glGenFramebuffers(lengthof(r_shadowmaps.framebuffers), r_shadowmaps.framebuffers);

	GLuint *out = r_shadowmaps.framebuffers;
	for (size_t i = 0; i < lengthof(r_shadowmaps.framebuffers); i++, out++) {
		glBindFramebuffer(GL_FRAMEBUFFER, *out);
		glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, r_shadowmaps.cubemap_array, 0, (GLint) i);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_InitShadowmaps(void) {

	R_InitShadowmapProgram();

	R_InitShadowmapTexture();

	R_InitShadowmapFramebuffers();
}

/**
 * @brief
 */
static void R_ShutdownShadowmapProgram(void) {

	glDeleteProgram(r_shadowmap_program.name);

	r_shadowmap_program.name = 0;

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowmapTexture(void) {

	glDeleteTextures(1, &r_shadowmaps.cubemap_array);

	r_shadowmaps.cubemap_array = 0;

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_ShutdownShadowmapFramebuffers(void) {

	glDeleteFramebuffers(lengthof(r_shadowmaps.framebuffers), r_shadowmaps.framebuffers);

	memset(r_shadowmaps.framebuffers, 0, sizeof(r_shadowmaps.framebuffers));

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_ShutdownShadowmaps(void) {

	R_ShutdownShadowmapProgram();

	R_ShutdownShadowmapTexture();

	R_ShutdownShadowmapFramebuffers();
}
