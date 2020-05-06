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

#define TEXTURE_MATERIAL                 0
#define TEXTURE_STAGE                    2
#define TEXTURE_LIGHTGRID                4
#define TEXTURE_LIGHTGRID_AMBIENT        4
#define TEXTURE_LIGHTGRID_DIFFUSE        5
#define TEXTURE_LIGHTGRID_RADIOSITY      6
#define TEXTURE_LIGHTGRID_DIRECTION      7

/**
 * @brief The program.
 */
static struct {
	GLuint name;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffusemap;

	GLint in_next_position;
	GLint in_next_normal;
	GLint in_next_tangent;
	GLint in_next_bitangent;

	GLint projection;
	GLint model;
	GLint view;

	GLint lerp;

	GLint texture_material;
	GLint texture_stage;
	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
	GLint texture_lightgrid_direction;

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

	struct {
		GLint flags;
		GLint color;
		GLint pulse;
		GLint scroll;
		GLint scale;
	} stage;

	GLuint lights_block;
	GLint lights_mask;

	GLint fog_parameters;
	GLint fog_color;

	GLint ticks;
} r_mesh_program;

/**
 * @brief
 */
void R_UpdateMeshEntities(void) {

	r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (IS_MESH_MODEL(e->model)) {
			e->blend_depth = R_BlendDepthForPoint(e->origin);
		}
	}

	glUseProgram(r_mesh_program.name);

	glUniformMatrix4fv(r_mesh_program.projection, 1, GL_FALSE, (GLfloat *) r_locals.projection3D.m);
	glUniformMatrix4fv(r_mesh_program.view, 1, GL_FALSE, (GLfloat *) r_locals.view.m);

	glUniform1f(r_mesh_program.brightness, r_brightness->value);
	glUniform1f(r_mesh_program.contrast, r_contrast->value);
	glUniform1f(r_mesh_program.saturation, r_saturation->value);
	glUniform1f(r_mesh_program.gamma, r_gamma->value);
	glUniform1f(r_mesh_program.modulate, r_modulate->value);

	glUniform3fv(r_mesh_program.lightgrid_mins, 1, r_world_model->bsp->lightgrid->mins.xyz);
	glUniform3fv(r_mesh_program.lightgrid_maxs, 1, r_world_model->bsp->lightgrid->maxs.xyz);

	glUniform3fv(r_mesh_program.fog_parameters, 1, r_locals.fog_parameters.xyz);
	glUniform3fv(r_mesh_program.fog_color, 1, r_view.fog_color.xyz);

	glUniform1i(r_mesh_program.stage.flags, STAGE_MATERIAL);
	glUniform1i(r_mesh_program.ticks, r_view.ticks);

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_DrawMeshEntityMaterialStage(const r_entity_t *e, const r_mesh_face_t *face, const r_stage_t *stage) {

	glUniform1i(r_mesh_program.stage.flags, stage->cm->flags);

	if (stage->cm->flags & STAGE_COLOR) {
		glUniform4fv(r_mesh_program.stage.color, 1, stage->cm->color.rgba);
	}

	if (stage->cm->flags & STAGE_PULSE) {
		glUniform1f(r_mesh_program.stage.pulse, stage->cm->pulse.hz);
	}

	if (stage->cm->flags & (STAGE_SCROLL_S | STAGE_SCROLL_T)) {
		glUniform2f(r_mesh_program.stage.scroll, stage->cm->scroll.s, stage->cm->scroll.t);
	}

	if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		glUniform2f(r_mesh_program.stage.scale, stage->cm->scale.s, stage->cm->scale.t);
	}

	glBlendFunc(stage->cm->blend.src, stage->cm->blend.dest);

	switch (stage->media->type) {
		case MEDIA_IMAGE:
		case MEDIA_ATLAS_IMAGE: {
			const r_image_t *image = (r_image_t *) stage->media;
			glBindTexture(GL_TEXTURE_2D, image->texnum);
		}
			break;
		default:
			break;
	}

	const GLint base_vertex = (GLint) (face->vertexes - e->model->mesh->vertexes);
	glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);

	R_GetError(stage->media->name);
}

/**
 * @brief
 */
static void R_DrawMeshEntityMaterialStages(const r_entity_t *e, const r_mesh_face_t *face, const r_material_t *material) {

	if (!r_materials->value) {
		return;
	}

	if (!(material->cm->flags & STAGE_DRAW)) {
		return;
	}

	glDepthMask(GL_FALSE);

	glEnable(GL_BLEND);
	glEnable(GL_POLYGON_OFFSET_FILL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

	int32_t s = 1;
	for (const r_stage_t *stage = material->stages; stage; stage = stage->next, s++) {

		if (!(stage->cm->flags & STAGE_DRAW)) {
			continue;
		}

		glPolygonOffset(-1.f, -s);

		R_DrawMeshEntityMaterialStage(e, face, stage);
	}

	glUniform1i(r_mesh_program.stage.flags, STAGE_MATERIAL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glPolygonOffset(0.f, 0.f);
	glDisable(GL_POLYGON_OFFSET_FILL);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDepthMask(GL_TRUE);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_DrawMeshEntity(const r_entity_t *e) {

	const r_mesh_model_t *mesh = e->model->mesh;
	assert(mesh);

	if (e->effects & EF_WEAPON) {
		glDepthRange(0.f, 0.1f);
	}

	if (e->effects & EF_BLEND) {
		glUniform1f(r_mesh_program.alpha_threshold, 0.f);
		glEnable(GL_BLEND);
	} else {
		glUniform1f(r_mesh_program.alpha_threshold, .125f);
		glEnable(GL_CULL_FACE);
	}

	glBindVertexArray(mesh->vertex_array);

	glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->elements_buffer);

	glEnableVertexAttribArray(r_mesh_program.in_position);
	glEnableVertexAttribArray(r_mesh_program.in_normal);
	glEnableVertexAttribArray(r_mesh_program.in_tangent);
	glEnableVertexAttribArray(r_mesh_program.in_bitangent);
	glEnableVertexAttribArray(r_mesh_program.in_diffusemap);

	glEnableVertexAttribArray(r_mesh_program.in_next_position);
	glEnableVertexAttribArray(r_mesh_program.in_next_normal);
	glEnableVertexAttribArray(r_mesh_program.in_next_tangent);
	glEnableVertexAttribArray(r_mesh_program.in_next_bitangent);

	glUniformMatrix4fv(r_mesh_program.model, 1, GL_FALSE, (GLfloat *) e->matrix.m);

	glUniform1f(r_mesh_program.lerp, e->lerp);
	glUniform4fv(r_mesh_program.color, 1, e->color.xyzw);
	glUniform1i(r_mesh_program.lights_mask, e->lights);

	const r_mesh_face_t *face = mesh->faces;
	for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

		const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, position));
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, normal));
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, tangent));
		glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, bitangent));
		glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) old_frame_offset + offsetof(r_mesh_vertex_t, diffusemap));

		const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

		glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, position));
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, normal));
		glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, tangent));
		glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) frame_offset + offsetof(r_mesh_vertex_t, bitangent));

		const r_material_t *material = e->skins[i] ?: face->material;
		if (material) {

			glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

			glUniform1f(r_mesh_program.bump, material->cm->bump * r_bumpmap->value);
			glUniform1f(r_mesh_program.parallax, material->cm->parallax * r_parallax->value);
			glUniform1f(r_mesh_program.hardness, material->cm->hardness * r_hardness->value);
			glUniform1f(r_mesh_program.specular, material->cm->specular * r_specular->value);
		}

		const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
		glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);
		
		r_view.count_mesh_triangles += face->num_elements / 3;

		R_DrawMeshEntityMaterialStages(e, face, material);
	}

	glBindVertexArray(0);

	if (e->effects & EF_WEAPON) {
		glDepthRange(0.f, 1.f);
	}

	if (e->effects & EF_BLEND) {
		glDisable(GL_BLEND);
	} else {
		glDisable(GL_CULL_FACE);
	}

	r_view.count_mesh_models++;
}

/**
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshEntities(int32_t blend_depth) {

	glEnable(GL_DEPTH_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glUseProgram(r_mesh_program.name);

	glBindBufferBase(GL_UNIFORM_BUFFER, 0, r_lights.uniform_buffer);

	const r_bsp_model_t *bsp = r_world_model->bsp;
	for (int32_t i = 0; i < BSP_LIGHTGRID_TEXTURES; i++) {
		glActiveTexture(GL_TEXTURE0 + TEXTURE_LIGHTGRID + i);
		glBindTexture(GL_TEXTURE_3D, bsp->lightgrid->textures[i]->texnum);
	}

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	const r_entity_t *e = r_view.entities;
	for (int32_t i = 0; i < r_view.num_entities; i++, e++) {
		if (IS_MESH_MODEL(e->model)) {

			if (e->effects & EF_NO_DRAW) {
				continue;
			}

			if (e->blend_depth != blend_depth) {
				continue;
			}

			R_DrawMeshEntity(e);
		}
	}

	glUseProgram(0);

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_DEPTH_TEST);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitMeshProgram(void) {

	memset(&r_mesh_program, 0, sizeof(r_mesh_program));

	r_mesh_program.name = R_LoadProgram(
			&MakeShaderDescriptor(GL_VERTEX_SHADER, "materials.glsl", "mesh_vs.glsl"),
			&MakeShaderDescriptor(GL_FRAGMENT_SHADER, "materials.glsl", "common_fs.glsl", "lights_fs.glsl", "mesh_fs.glsl"),
			NULL);

	glUseProgram(r_mesh_program.name);

	r_mesh_program.in_position = glGetAttribLocation(r_mesh_program.name, "in_position");
	r_mesh_program.in_normal = glGetAttribLocation(r_mesh_program.name, "in_normal");
	r_mesh_program.in_tangent = glGetAttribLocation(r_mesh_program.name, "in_tangent");
	r_mesh_program.in_bitangent = glGetAttribLocation(r_mesh_program.name, "in_bitangent");
	r_mesh_program.in_diffusemap = glGetAttribLocation(r_mesh_program.name, "in_diffusemap");

	r_mesh_program.in_next_position = glGetAttribLocation(r_mesh_program.name, "in_next_position");
	r_mesh_program.in_next_normal = glGetAttribLocation(r_mesh_program.name, "in_next_normal");
	r_mesh_program.in_next_tangent = glGetAttribLocation(r_mesh_program.name, "in_next_tangent");
	r_mesh_program.in_next_bitangent = glGetAttribLocation(r_mesh_program.name, "in_next_bitangent");

	r_mesh_program.projection = glGetUniformLocation(r_mesh_program.name, "projection");
	r_mesh_program.view = glGetUniformLocation(r_mesh_program.name, "view");
	r_mesh_program.model = glGetUniformLocation(r_mesh_program.name, "model");

	r_mesh_program.lerp = glGetUniformLocation(r_mesh_program.name, "lerp");

	r_mesh_program.texture_material = glGetUniformLocation(r_mesh_program.name, "texture_material");
	r_mesh_program.texture_stage = glGetUniformLocation(r_mesh_program.name, "texture_stage");
	r_mesh_program.texture_lightgrid_ambient = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_ambient");
	r_mesh_program.texture_lightgrid_diffuse = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_diffuse");
	r_mesh_program.texture_lightgrid_direction = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_direction");

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

	r_mesh_program.stage.flags = glGetUniformLocation(r_mesh_program.name, "stage.flags");
	r_mesh_program.stage.color = glGetUniformLocation(r_mesh_program.name, "stage.color");
	r_mesh_program.stage.pulse = glGetUniformLocation(r_mesh_program.name, "stage.pulse");
	r_mesh_program.stage.scroll = glGetUniformLocation(r_mesh_program.name, "stage.scroll");
	r_mesh_program.stage.scale = glGetUniformLocation(r_mesh_program.name, "stage.scale");

	r_mesh_program.lights_block = glGetUniformBlockIndex(r_mesh_program.name, "lights_block");
	glUniformBlockBinding(r_mesh_program.name, r_mesh_program.lights_block, 0);
	r_mesh_program.lights_mask = glGetUniformLocation(r_mesh_program.name, "lights_mask");

	r_mesh_program.fog_parameters = glGetUniformLocation(r_mesh_program.name, "fog_parameters");
	r_mesh_program.fog_color = glGetUniformLocation(r_mesh_program.name, "fog_color");

	r_mesh_program.ticks = glGetUniformLocation(r_mesh_program.name, "ticks");

	glUniform1i(r_mesh_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_mesh_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_mesh_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_mesh_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
	glUniform1i(r_mesh_program.texture_lightgrid_direction, TEXTURE_LIGHTGRID_DIRECTION);

	glUseProgram(0);
	
	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownMeshProgram(void) {

	glDeleteProgram(r_mesh_program.name);

	r_mesh_program.name = 0;
}

