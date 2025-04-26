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
 * @brief The shadow program.
 */
static struct {
	GLuint name;

	/**
	 * @brief The uniform blocks.
	 */
	GLuint uniforms_block;
	GLuint lights_block;

	/**
	 * @brief The vertex attributes.
	 */
	GLint in_position;
	GLint in_next_position;

	/**
	 * @brief The model matrix.
	 */
	GLint model;

	/**
	 * @brief The frame interpolation fraction.
	 */
	GLint lerp;

	/**
	 * @brief The light index and shadow layer.
	 */
	GLint light_index;
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
	 * @brief The depth pass framebuffer.
	 */
	GLuint framebuffer;
} r_shadows;

/**
 * @brief
 */
static void R_DrawBspInlineEntityShadow(const r_view_t *view, const r_entity_t *e) {

	const r_bsp_inline_model_t *in = IS_BSP_MODEL(e->model)
		? e->model->bsp->inline_models
		: e->model->bsp_inline;



	glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, e->matrix.array);
	glUniform1f(r_shadow_program.lerp, 0.f);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, in->depth_pass_elements_buffer);
	glDrawElements(GL_TRIANGLES, in->num_depth_pass_elements, GL_UNSIGNED_INT, NULL);
}

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

	glEnableVertexAttribArray(r_shadow_program.in_position);
	glEnableVertexAttribArray(r_shadow_program.in_next_position);
	
	glUniformMatrix4fv(r_shadow_program.model, 1, GL_FALSE, e->matrix.array);
	glUniform1f(r_shadow_program.lerp, e->lerp);

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

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);
}

/**
 * @brief Clears the shadow texture for the specified light.
 */
static void R_ClearShadow(const r_view_t *view, const r_light_t *light) {

	for (int32_t i = 0; i < 6; i++) {
		glFramebufferTextureLayer(GL_FRAMEBUFFER,
								  GL_DEPTH_ATTACHMENT,
								  r_shadows.cubemap_array,
								  0,
								  light->index * 6 + i);

		glClear(GL_DEPTH_BUFFER_BIT);
	}

	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, r_shadows.cubemap_array, 0);
}

/**
 * @brief
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

	if (light->source == NULL) {
		return false;
	}

	while (e) {
		if (light->source == e->id) {
			return true;
		}
		e = e->parent;
	}

	return false;
}

/**
 * @brief Renders the shadow cubemap for the specified light.
 */
static void R_DrawShadow(const r_view_t *view, const r_light_t *light) {

	R_ClearShadow(view, light);

	glUniform1i(r_shadow_program.light_index, light->index);

	const r_bsp_model_t *bsp = r_world_model->bsp;
	glBindVertexArray(bsp->vertex_array);

	glEnableVertexAttribArray(r_shadow_program.in_position);
	glDisableVertexAttribArray(r_shadow_program.in_next_position);

	glBindBuffer(GL_ARRAY_BUFFER, bsp->vertex_buffer);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (!IS_BSP_MODEL(e->model) && !IS_BSP_INLINE_MODEL(e->model)) {
			continue;
		}

		if (!Box3_Intersects(light->bounds, e->abs_model_bounds)) {
			continue;
		}

		R_DrawBspInlineEntityShadow(view, e);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (!IS_MESH_MODEL(e->model)) {
			continue;
		}

		if (e->effects & EF_NO_SHADOW) {
			continue;
		}

		if (R_IsLightSource(light, e)) {
			continue;
		}

		if (!Box3_Intersects(light->bounds, e->abs_model_bounds)) {
			continue;
		}

		R_DrawMeshEntityShadow(view, e);
	}

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_DrawShadows(const r_view_t *view) {

	if (!r_shadowmap->integer) {
		return;
	}

	glBindFramebuffer(GL_FRAMEBUFFER, r_shadows.framebuffer);

	glUseProgram(r_shadow_program.name);

	const GLsizei size = r_shadowmap_size->integer;
	glViewport(0, 0, size, size);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	const r_light_t *l = view->lights;
	for (int32_t i = 0; i < view->num_lights; i++, l++) {

		if (l->index == -1) {
			continue;
		}

		R_DrawShadow(view, l);
	}

	glCullFace(GL_BACK);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glViewport(0, 0, r_context.drawable_width, r_context.drawable_height);

	glUseProgram(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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

	r_shadow_program.lights_block = glGetUniformBlockIndex(r_shadow_program.name, "lights_block");
	glUniformBlockBinding(r_shadow_program.name, r_shadow_program.lights_block, 1);

	r_shadow_program.in_position = glGetAttribLocation(r_shadow_program.name, "in_position");
	r_shadow_program.in_next_position = glGetAttribLocation(r_shadow_program.name, "in_next_position");

	r_shadow_program.model = glGetUniformLocation(r_shadow_program.name, "model");
	r_shadow_program.lerp = glGetUniformLocation(r_shadow_program.name, "lerp");

	r_shadow_program.light_index = glGetUniformLocation(r_shadow_program.name, "light_index");

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowTextures(void) {

	const GLsizei size = r_shadowmap_size->integer;

	glGenTextures(1, &r_shadows.cubemap_array);
	
	glActiveTexture(GL_TEXTURE0 + TEXTURE_SHADOWMAP);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARRAY, r_shadows.cubemap_array);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

	glTexImage3D(GL_TEXTURE_CUBE_MAP_ARRAY, 0, GL_DEPTH_COMPONENT, size, size, MAX_LIGHT_UNIFORMS * 6, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_InitShadowFramebuffer(void) {

	glGenFramebuffers(1, &r_shadows.framebuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, r_shadows.framebuffer);

	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_InitShadows(void) {

	R_InitShadowProgram();

	R_InitShadowTextures();

	R_InitShadowFramebuffer();
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
static void R_ShutdownShadowFramebuffer(void) {

	glDeleteFramebuffers(1, &r_shadows.framebuffer);

	r_shadows.framebuffer = 0;

	R_GetError(NULL);
}

/**
 * @brief 
 */
void R_ShutdownShadows(void) {

	R_ShutdownShadowProgram();

	R_ShutdownShadowTexture();

	R_ShutdownShadowFramebuffer();
}
