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

#define MAX_ACTIVE_LIGHTS               10

#define TEXTURE_DIFFUSE                  0
#define TEXTURE_NORMALMAP                1
#define TEXTURE_GLOSSMAP                 2
#define TEXTURE_LIGHTGRID                3
#define TEXTURE_LIGHTGRID_AMBIENT        3
#define TEXTURE_LIGHTGRID_DIFFUSE        4
#define TEXTURE_LIGHTGRID_RADIOSITY      5
#define TEXTURE_LIGHTGRID_DIFFUSE_DIR    6
#define TEXTURE_LIGHTGRID_RADIOSITY_DIR  7

#define TEXTURE_MASK_DIFFUSE            (1 << TEXTURE_DIFFUSE)
#define TEXTURE_MASK_NORMALMAP          (1 << TEXTURE_NORMALMAP)
#define TEXTURE_MASK_GLOSSMAP           (1 << TEXTURE_GLOSSMAP)
#define TEXTURE_MASK_LIGHTGRID          (1 << TEXTURE_LIGHTGRID)
#define TEXTURE_MASK_ALL                0xff

/**
 * @brief The program.
 */
static struct {
	GLuint name;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffuse;

	GLint in_next_position;
	GLint in_next_normal;
	GLint in_next_tangent;
	GLint in_next_bitangent;

	GLint projection;
	GLint model;
	GLint view;
	GLint normal;

	GLint lerp;

	GLint textures;

	GLint texture_diffuse;
	GLint texture_normalmap;
	GLint texture_glossmap;

	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
	GLint texture_lightgrid_radiosity;
	GLint texture_lightgrid_diffuse_dir;
	GLint texture_lightgrid_radiosity_dir;

	GLint lightgrid_mins;
	GLint lightgrid_maxs;

	GLint color;
	GLint alpha_threshold;

	GLint brightness;
	GLint contrast;
	GLint saturation;
	GLint gamma;
	GLint modulate;

	GLint bump;
	GLint parallax;
	GLint hardness;
	GLint specular;

	GLint light_positions[MAX_ACTIVE_LIGHTS];
	GLint light_colors[MAX_ACTIVE_LIGHTS];

	GLint fog_parameters;
	GLint fog_color;

	GLint caustics;
} r_mesh_program;

/**
 * @brief Applies any client-side transformations specified by the model's world or
 * view configuration structure.
 */
void R_ApplyMeshModelConfig(r_entity_t *e) {

	assert(IS_MESH_MODEL(e->model));

	const r_mesh_config_t *c;
	if (e->effects & EF_WEAPON) {
		c = &e->model->mesh->config.view;
	} else if (e->effects & EF_LINKED) {
		c = &e->model->mesh->config.link;
	} else {
		c = &e->model->mesh->config.world;
	}

	Matrix4x4_Concat(&e->matrix, &e->matrix, &c->transform);

	e->effects |= c->flags;
}

/**
 * @return True if the specified entity was frustum-culled and can be skipped.
 */
static _Bool R_CullMeshEntity(const r_entity_t *e) {
	vec3_t mins, maxs;

	if (e->effects & EF_WEAPON) { // never cull the weapon
		return false;
	}

	// calculate scaled bounding box in world space

	VectorMA(e->origin, e->scale, e->model->mins, mins);
	VectorMA(e->origin, e->scale, e->model->maxs, maxs);

	return R_CullBox(mins, maxs);
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

	glEnableVertexAttribArray(r_mesh_program.in_position);
	glEnableVertexAttribArray(r_mesh_program.in_normal);
	glEnableVertexAttribArray(r_mesh_program.in_tangent);
	glEnableVertexAttribArray(r_mesh_program.in_bitangent);
	glEnableVertexAttribArray(r_mesh_program.in_diffuse);

	glEnableVertexAttribArray(r_mesh_program.in_next_position);
	glEnableVertexAttribArray(r_mesh_program.in_next_normal);
	glEnableVertexAttribArray(r_mesh_program.in_next_tangent);
	glEnableVertexAttribArray(r_mesh_program.in_next_bitangent);

	glUniformMatrix4fv(r_mesh_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);

	matrix4x4_t normal;
	Matrix4x4_Concat(&normal, &r_locals.view, &e->matrix);
	Matrix4x4_CopyRotateOnly(&normal, &normal);
	Matrix4x4_Invert_Simple(&normal, &normal);
	Matrix4x4_Transpose(&normal, &normal);

	glUniformMatrix4fv(r_mesh_program.normal, 1, GL_FALSE, (GLfloat *) normal.m);

	glUniform1f(r_mesh_program.lerp, e->lerp);
	glUniform4fv(r_mesh_program.color, 1, e->color);

	const r_mesh_face_t *face = mesh->faces;
	for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

		const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, normal));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, tangent));
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, bitangent));
		glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, diffuse));

		const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, position));
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, normal));
		glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, tangent));
		glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, bitangent));

		const r_bsp_lightgrid_t *lg = r_model_state.world->bsp->lightgrid;
		GLint textures = lg ? TEXTURE_MASK_LIGHTGRID : 0;

		const r_material_t *material = e->skins[i] ?: face->material;
		if (material) {

			glUniform1f(r_mesh_program.bump, material->cm->bump);
			glUniform1f(r_mesh_program.parallax, material->cm->parallax);
			glUniform1f(r_mesh_program.hardness, material->cm->hardness);
			glUniform1f(r_mesh_program.specular, material->cm->specular);

			if (material->diffuse) {
				textures |= TEXTURE_MASK_DIFFUSE;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSE);
				glBindTexture(GL_TEXTURE_2D, material->diffuse->texnum);
			}

			if (material->normalmap) {
				textures |= TEXTURE_MASK_NORMALMAP;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_NORMALMAP);
				glBindTexture(GL_TEXTURE_2D, material->normalmap->texnum);
			}

			if (material->glossmap) {
				textures |= TEXTURE_MASK_GLOSSMAP;
				glActiveTexture(GL_TEXTURE0 + TEXTURE_GLOSSMAP);
				glBindTexture(GL_TEXTURE_2D, material->glossmap->texnum);
			}
		}

		glUniform1i(r_mesh_program.textures, textures);

		const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
		glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);
		
		r_view.count_mesh_triangles += face->num_elements / 3;
	}

	r_view.count_mesh_models++;
}

/**
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshEntities(void) {

	glEnable(GL_DEPTH_TEST);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	glUseProgram(r_mesh_program.name);

	glUniformMatrix4fv(r_mesh_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_mesh_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);

	glUniform1f(r_mesh_program.alpha_threshold, 0.f);

	glUniform1f(r_mesh_program.brightness, r_brightness->value);
	glUniform1f(r_mesh_program.contrast, r_contrast->value);
	glUniform1f(r_mesh_program.saturation, r_saturation->value);
	glUniform1f(r_mesh_program.gamma, r_gamma->value);
	glUniform1f(r_mesh_program.modulate, r_modulate->value);

	const r_bsp_lightgrid_t *lg = r_model_state.world->bsp->lightgrid;
	if (lg) {

		for (int32_t i = 0; i < BSP_LIGHTGRID_TEXTURES; i++) {
			glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID + i);
			glBindTexture(GL_TEXTURE_3D, lg->textures[i]->texnum);
		}

		glUniform3fv(r_mesh_program.lightgrid_mins, 1, lg->mins);
		glUniform3fv(r_mesh_program.lightgrid_maxs, 1, lg->maxs);
	}

	{
		glEnable(GL_CULL_FACE);

		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (e->model && e->model->type == MOD_MESH) {

				if (e->effects & EF_NO_DRAW) {
					continue;
				}

				if (e->effects & EF_BLEND) {
					continue;
				}

				if (R_CullMeshEntity(e)) {
					continue;
				}

				R_DrawMeshEntity(e);
			}
		}

		glDisable(GL_CULL_FACE);
	}

	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		const r_entity_t *e = r_view.entities;
		for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
			if (e->model && e->model->type == MOD_MESH) {

				if (e->effects & EF_NO_DRAW) {
					continue;
				}

				if (!(e->effects & EF_BLEND)) {
					continue;
				}

				if (R_CullMeshEntity(e)) {
					continue;
				}

				R_DrawMeshEntity(e);
			}
		}

		glBlendFunc(GL_ONE, GL_ZERO);
		glDisable(GL_BLEND);
	}

	glActiveTexture(GL_TEXTURE0);

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitMeshProgram(void) {

	memset(&r_mesh_program, 0, sizeof(r_mesh_program));

	r_mesh_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "mesh_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "color_filter.glsl", "mesh_fs.glsl"),
			NULL);

	glUseProgram(r_mesh_program.name);

	r_mesh_program.in_position = glGetAttribLocation(r_mesh_program.name, "in_position");
	r_mesh_program.in_normal = glGetAttribLocation(r_mesh_program.name, "in_normal");
	r_mesh_program.in_tangent = glGetAttribLocation(r_mesh_program.name, "in_tangent");
	r_mesh_program.in_bitangent = glGetAttribLocation(r_mesh_program.name, "in_bitangent");
	r_mesh_program.in_diffuse = glGetAttribLocation(r_mesh_program.name, "in_diffuse");

	r_mesh_program.in_next_position = glGetAttribLocation(r_mesh_program.name, "in_next_position");
	r_mesh_program.in_next_normal = glGetAttribLocation(r_mesh_program.name, "in_next_normal");
	r_mesh_program.in_next_tangent = glGetAttribLocation(r_mesh_program.name, "in_next_tangent");
	r_mesh_program.in_next_bitangent = glGetAttribLocation(r_mesh_program.name, "in_next_bitangent");

	r_mesh_program.projection = glGetUniformLocation(r_mesh_program.name, "projection");
	r_mesh_program.view = glGetUniformLocation(r_mesh_program.name, "view");
	r_mesh_program.model = glGetUniformLocation(r_mesh_program.name, "model");
	r_mesh_program.normal = glGetUniformLocation(r_mesh_program.name, "normal");

	r_mesh_program.lerp = glGetUniformLocation(r_mesh_program.name, "lerp");

	r_mesh_program.textures = glGetUniformLocation(r_mesh_program.name, "textures");
	r_mesh_program.texture_diffuse = glGetUniformLocation(r_mesh_program.name, "texture_diffuse");
	r_mesh_program.texture_normalmap = glGetUniformLocation(r_mesh_program.name, "texture_normalmap");
	r_mesh_program.texture_glossmap = glGetUniformLocation(r_mesh_program.name, "texture_glossmap");

	r_mesh_program.texture_lightgrid_ambient = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_ambient");
	r_mesh_program.texture_lightgrid_diffuse = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_diffuse");
	r_mesh_program.texture_lightgrid_radiosity = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_radiosity");
	r_mesh_program.texture_lightgrid_diffuse_dir = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_diffuse_dir");
	r_mesh_program.texture_lightgrid_radiosity_dir = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_radiosity_dir");

	r_mesh_program.lightgrid_mins = glGetUniformLocation(r_mesh_program.name, "lightgrid_mins");
	r_mesh_program.lightgrid_maxs = glGetUniformLocation(r_mesh_program.name, "lightgrid_maxs");

	r_mesh_program.color = glGetUniformLocation(r_mesh_program.name, "color");
	r_mesh_program.alpha_threshold = glGetUniformLocation(r_mesh_program.name, "alpha_threshold");

	r_mesh_program.brightness = glGetUniformLocation(r_mesh_program.name, "brightness");
	r_mesh_program.contrast = glGetUniformLocation(r_mesh_program.name, "contrast");
	r_mesh_program.saturation = glGetUniformLocation(r_mesh_program.name, "saturation");
	r_mesh_program.gamma = glGetUniformLocation(r_mesh_program.name, "gamma");
	r_mesh_program.modulate = glGetUniformLocation(r_mesh_program.name, "modulate");

	r_mesh_program.bump = glGetUniformLocation(r_mesh_program.name, "bump");
	r_mesh_program.parallax = glGetUniformLocation(r_mesh_program.name, "parallax");
	r_mesh_program.hardness = glGetUniformLocation(r_mesh_program.name, "hardness");
	r_mesh_program.specular = glGetUniformLocation(r_mesh_program.name, "specular");

	for (size_t i = 0; i < lengthof(r_mesh_program.light_positions); i++) {
		r_mesh_program.light_positions[i] = glGetUniformLocation(r_mesh_program.name, va("light_positions[%zd]", i));
		r_mesh_program.light_colors[i] = glGetUniformLocation(r_mesh_program.name, va("light_colors[%zd]", i));
	}

	r_mesh_program.fog_parameters = glGetUniformLocation(r_mesh_program.name, "fog_parameters");
	r_mesh_program.fog_color = glGetUniformLocation(r_mesh_program.name, "fog_color");

	r_mesh_program.caustics = glGetUniformLocation(r_mesh_program.name, "caustics");

	glUniform1i(r_mesh_program.texture_diffuse, TEXTURE_DIFFUSE);
	glUniform1i(r_mesh_program.texture_normalmap, TEXTURE_NORMALMAP);
	glUniform1i(r_mesh_program.texture_glossmap, TEXTURE_GLOSSMAP);

	glUniform1i(r_mesh_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_mesh_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
	glUniform1i(r_mesh_program.texture_lightgrid_radiosity, TEXTURE_LIGHTGRID_RADIOSITY);
	glUniform1i(r_mesh_program.texture_lightgrid_diffuse_dir, TEXTURE_LIGHTGRID_DIFFUSE_DIR);
	glUniform1i(r_mesh_program.texture_lightgrid_radiosity_dir, TEXTURE_LIGHTGRID_RADIOSITY_DIR);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownMeshProgram(void) {

	glDeleteProgram(r_mesh_program.name);

	r_mesh_program.name = 0;
}

