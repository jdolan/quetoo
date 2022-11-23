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
 *
 */
static struct {
	GLuint name;
	GLuint uniforms_block;

	GLint in_position;
	GLint in_next_position;
	
	GLint model;

	GLint cubemap_projections;
} r_shadowmap_program;

typedef struct {
	const r_light_t *light;
	GLuint cubemap;
	GLuint fb;
	mat4_t matrix[6];
} r_shadowmap_t;

static r_shadowmap_t r_shadowmaps[MAX_LIGHTS];

/**
 * @brief
 */
static void R_DrawMeshEntityFace(const r_entity_t *e,
								 const r_mesh_model_t *mesh,
								 const r_mesh_face_t *face) {

	const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));
	
	const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
	//glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));
	
	const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
	glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);

	//r_stats.count_mesh_triangles += face->num_elements / 3;
}

/**
 * @brief
 */
static void R_DrawMeshEntity(const r_entity_t *e) {

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

			R_DrawMeshEntityFace(e, mesh, face);
		}
	}

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	
	glBindVertexArray(0);

	//r_stats.count_mesh_models++;
}

/**
 * @brief 
 * @param view 
 */
void R_DrawShadowmaps(const r_view_t *view) {

	// https://stackoverflow.com/questions/65876124/how-do-i-use-a-cubemap-array-in-opengl-to-render-multiple-point-lights-with-shad
	glUseProgram(r_shadowmap_program.name);

	const r_light_t *l = view->lights;
	r_shadowmap_t *sh = r_shadowmaps;

	for (int32_t i = 0; i < view->num_lights; i++, l++, sh++) {

		glViewport(0, 0, SHADOWMAP_SIZE, SHADOWMAP_SIZE);
		glBindFramebuffer(GL_FRAMEBUFFER, sh->fb);
		glClear(GL_DEPTH_BUFFER_BIT);

		const box3_t light_bounds = Box3_FromCenterRadius(l->origin, l->radius);
		
		const float fov = tanf(Radians(90.f));
		const mat4_t projection = Mat4_FromFrustum(-fov, fov, -fov, fov, 1.f, l->radius);

		const vec3_t angles[] = {
			Vec3(0.f,    0.f, 0.f),
			Vec3(0.f,   90.f, 0.f),
			Vec3(0.f,  180.f, 0.f),
			Vec3(0.f,  270.f, 0.f),
			Vec3(270.f,  0.f, 0.f),
			Vec3(90.f,   0.f, 0.f)
		};

		for (size_t j = 0; j < lengthof(sh->matrix); j++) {
			sh->matrix[j] = Mat4_FromRotation(-90.f, Vec3(1.f, 0.f, 0.f)); // put Z going up
			sh->matrix[j] = Mat4_ConcatRotation(sh->matrix[j], 90.f, Vec3(0.f, 0.f, 1.f)); // put Z going up
			sh->matrix[j] = Mat4_ConcatRotation3(sh->matrix[j], angles[j]);
			sh->matrix[j] = Mat4_ConcatTranslation(sh->matrix[j], Vec3_Negate(l->origin));
			sh->matrix[j] = Mat4_Concat(sh->matrix[j], projection);
		}

		glUniformMatrix4fv(r_shadowmap_program.name, 6, GL_FALSE, sh->matrix);
		
		// bind light's cubemap render target
		// glViewport

		const r_entity_t *e = view->entities;
		for (int32_t j = 0; j < view->num_entities; j++, e++) {
	
			if (!IS_MESH_MODEL(e->model)) {
				continue;
			}
			
			if (!Box3_Intersects(light_bounds, e->abs_model_bounds)) {
			//	continue;
			}

			// draw the mesh
			const r_mesh_model_t *mesh = e->model->mesh;

			R_DrawMeshEntity(e);
		}
	
	}

	glUseProgram(0);

	R_GetError(NULL);
}

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
	r_shadowmap_program.in_next_position = glGetAttribLocation(r_shadowmap_program.name, "in_next_position");

	r_shadowmap_program.model = glGetUniformLocation(r_shadowmap_program.name, "model");

	r_shadowmap_program.cubemap_projections = glGetUniformLocation(r_shadowmap_program.name, "cubemap_projections");
	
	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief 
 * @param  
 */
void R_InitShadowmaps(void) {

	R_InitShadowmapProgram();

	r_shadowmap_t *sh = r_shadowmaps;
	for (size_t i = 0; i < lengthof(r_shadowmaps); i++, sh++) {
	
		glGenTextures(1, &sh->cubemap);
		glBindTexture(GL_TEXTURE_CUBE_MAP, sh->cubemap);

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

		for (int32_t j = 0; j < 6; j++) {
			glTexImage2D(
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + j,
				0,
				GL_DEPTH_COMPONENT,
				SHADOWMAP_SIZE,
				SHADOWMAP_SIZE,
				0,
				GL_DEPTH_COMPONENT,
				GL_FLOAT,
				NULL);
		}

		glGenFramebuffers(1, &sh->fb);
		glBindFramebuffer(GL_FRAMEBUFFER, sh->fb);
		glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, sh->cubemap, 0);
		glDrawBuffer(GL_NONE);
		glReadBuffer(GL_NONE);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);  
	}

}

/**
 * @brief 
 * @param  
 */
void R_ShutdownShadowmaps(void) {

	r_shadowmap_t *sh = r_shadowmaps;
	for (size_t i = 0; i < lengthof(r_shadowmaps); i++, sh++) {
	
		glDeleteTextures(1, &sh->cubemap);
	}
}