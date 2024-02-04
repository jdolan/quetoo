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
 * @brief The program.
 */
static struct {
	GLuint name;

	GLuint uniforms_block;
	GLuint lights_block;

	GLint in_position;
	GLint in_normal;
	GLint in_tangent;
	GLint in_bitangent;
	GLint in_diffusemap;

	GLint in_next_position;
	GLint in_next_normal;
	GLint in_next_tangent;
	GLint in_next_bitangent;

	GLint model;

	GLint lerp;

	GLint texture_material;
	GLint texture_stage;
	GLint texture_lightgrid_ambient;
	GLint texture_lightgrid_diffuse;
	GLint texture_lightgrid_direction;
	GLint texture_lightgrid_caustics;
	GLint texture_lightgrid_fog;
	GLint texture_shadowmap;
	GLint texture_shadowmap_cube;

	GLint color;
	GLint tint_colors;

	struct {
		GLint alpha_test;
		GLint roughness;
		GLint hardness;
		GLint specularity;
		GLint bloom;
	} material;

	struct {
		GLint flags;
		GLint color;
		GLint pulse;
		GLint scroll;
		GLint scale;
		GLint shell;
	} stage;

	r_media_t *shell;
} r_mesh_program;

/**
 * @brief
 */
void R_UpdateMeshEntities(r_view_t *view) {

	r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {

		if (!IS_MESH_MODEL(e->model)) {
			continue;
		}

		e->blend_depth = INT32_MIN;

		if (e->effects & (EF_BLEND | EF_SHELL)) {
			e->blend_depth = R_BlendDepthForPoint(view, e->origin, BLEND_DEPTH_ENTITY);
		} else {
			const r_mesh_face_t *face = e->model->mesh->faces;
			for (int32_t j = 0; j < e->model->mesh->num_faces; j++, face++) {

				const r_material_t *material = e->skins[j] ?: face->material;
				if (material->cm->surface & SURF_MASK_BLEND) {
					e->blend_depth = R_BlendDepthForPoint(view, e->origin, BLEND_DEPTH_ENTITY);
					break;
				}
			}
		}
	}
}

/**
 * @brief
 */
static void R_DrawMeshEntityMaterialStage(const r_entity_t *e, const r_mesh_face_t *face, const r_mesh_model_t *mesh, const r_stage_t *stage) {

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

	if (stage->cm->flags & STAGE_SHELL) {
		const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(vec3_t);
		const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(vec3_t);

		glUniform1f(r_mesh_program.stage.shell, stage->cm->shell.radius);

		glBindBuffer(GL_ARRAY_BUFFER, mesh->shell_normals_buffer);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) old_frame_offset);
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(vec3_t), (void *) frame_offset);
	}

	glBlendFunc(stage->cm->blend.src, stage->cm->blend.dest);

	if (stage->media) {
		switch (stage->media->type) {
			case R_MEDIA_IMAGE:
			case R_MEDIA_ATLAS_IMAGE: {
				const r_image_t *image = (r_image_t *) stage->media;
				glBindTexture(GL_TEXTURE_2D, image->texnum);
			}
				break;
			default:
				break;
		}
	}

	const GLint base_vertex = (GLint) (face->vertexes - e->model->mesh->vertexes);
	glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);

	if (stage->cm->flags & STAGE_SHELL) {
		const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);
		const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

		glBindBuffer(GL_ARRAY_BUFFER, mesh->vertex_buffer);

		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, normal)));
		glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, normal)));
	}

	R_GetError(stage->media->name);
}

/**
 * @brief
 */
static void R_DrawMeshEntityShellEffect(const r_entity_t *e, const r_mesh_face_t *face, const r_mesh_model_t *mesh) {

	if (!(e->effects & EF_SHELL)) {
		return;
	}

	R_DrawMeshEntityMaterialStage(e, face, mesh, &(const r_stage_t) {
		.cm = &(const cm_stage_t) {
			.flags = STAGE_COLOR | STAGE_SHELL | STAGE_SCROLL_S | STAGE_SCROLL_T,
			.color = Color4fv(Vec3_ToVec4(e->shell, .33f)),
			.blend = { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA },
			.scroll = { 0.25f, 0.25f },
			.shell = { (e->effects & EF_WEAPON) ? .33f : 1.f }
		},
		.media = r_mesh_program.shell
	});
}

/**
 * @brief
 */
static void R_DrawMeshEntityMaterialStages(const r_entity_t *e, const r_mesh_face_t *face, const r_mesh_model_t *mesh, const r_material_t *material) {

	if (!r_draw_material_stages->value) {
		return;
	}

	if (!(material->cm->stage_flags & STAGE_DRAW) && !(e->effects & EF_SHELL)) {
		return;
	}

	glDepthMask(GL_FALSE);

	const GLboolean blend = glIsEnabled(GL_BLEND);
	if (!blend) {
		glEnable(GL_BLEND);
	}

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_STAGE);

	int32_t s = 1;
	for (const r_stage_t *stage = material->stages; stage; stage = stage->next, s++) {

		if (!(stage->cm->flags & STAGE_DRAW)) {
			continue;
		}

		R_DrawMeshEntityMaterialStage(e, face, mesh, stage);
	}

	R_DrawMeshEntityShellEffect(e, face, mesh);

	glUniform1i(r_mesh_program.stage.flags, STAGE_MATERIAL);

	glActiveTexture(GL_TEXTURE0 + TEXTURE_MATERIAL);

	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	if (!blend) {
		glDisable(GL_BLEND);
	}

	glDepthMask(GL_TRUE);

	R_GetError(NULL);
}

/**
 * @brief
 */
static void R_DrawMeshEntityFace(const r_entity_t *e,
								 const r_mesh_model_t *mesh,
								 const r_mesh_face_t *face,
								 const r_material_t *material) {

	const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, position)));
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, normal)));
	glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, tangent)));
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, bitangent)));
	glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (old_frame_offset + offsetof(r_mesh_vertex_t, diffusemap)));

	const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(r_mesh_vertex_t);

	glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));
	glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, normal)));
	glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, tangent)));
	glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, bitangent)));

	glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

	glUniform1f(r_mesh_program.material.alpha_test, material->cm->alpha_test * r_alpha_test->value);
	glUniform1f(r_mesh_program.material.roughness, material->cm->roughness * r_roughness->value);
	glUniform1f(r_mesh_program.material.hardness, material->cm->hardness * r_hardness->value);
	glUniform1f(r_mesh_program.material.specularity, material->cm->specularity * r_specularity->value);
	glUniform1f(r_mesh_program.material.bloom, material->cm->bloom * r_bloom->value);

	if (*material->cm->tintmap.path) {
		vec4_t tints[3];

		memcpy(tints, e->tints, sizeof(tints));

		for (size_t i = 0; i < lengthof(tints); i++) {
			if (!e->tints[i].w) {
				tints[i] = material->cm->tintmap_defaults[i];
			}
		}

		glUniform4fv(r_mesh_program.tint_colors, 3, tints[0].xyzw);
	}

	vec4_t color = e->color;
	switch (material->cm->surface & SURF_MASK_BLEND) {
		case SURF_BLEND_33:
			color.w *= .333f;
			break;
		case SURF_BLEND_66:
			color.w *= .666f;
			break;
		default:
			color.w *= 1.f;
			break;
	}

	glUniform4fv(r_mesh_program.color, 1, color.xyzw);

	const GLint base_vertex = (GLint) (face->vertexes - mesh->vertexes);
	glDrawElementsBaseVertex(GL_TRIANGLES, face->num_elements, GL_UNSIGNED_INT, face->elements, base_vertex);

	r_stats.mesh_triangles += face->num_elements / 3;

	R_DrawMeshEntityMaterialStages(e, face, mesh, material);
}

/**
 * @brief
 */
static void R_DrawMeshEntity(const r_view_t *view, const r_entity_t *e) {

	const r_mesh_model_t *mesh = e->model->mesh;
	assert(mesh);

	if (e->effects & EF_WEAPON) {
		glDepthRange(.0f, 0.1f);
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

	glUniformMatrix4fv(r_mesh_program.model, 1, GL_FALSE, e->matrix.array);

	glUniform1f(r_mesh_program.lerp, e->lerp);
	glUniform4fv(r_mesh_program.color, 1, e->color.xyzw);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	{
		const r_mesh_face_t *face = mesh->faces;
		for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

			const r_material_t *material = e->skins[i] ?: face->material;
			if ((material->cm->surface & SURF_MASK_BLEND) || (e->effects & EF_BLEND)) {
				continue;
			}

			R_DrawMeshEntityFace(e, mesh, face, material);
		}
	}

	glDisable(GL_CULL_FACE);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	{
		const r_mesh_face_t *face = mesh->faces;
		for (int32_t i = 0; i < mesh->num_faces; i++, face++) {

			const r_material_t *material = e->skins[i] ?: face->material;
			if (!(material->cm->surface & SURF_MASK_BLEND) && !(e->effects & EF_BLEND)) {
				continue;
			}

			R_DrawMeshEntityFace(e, mesh, face, material);
		}
	}

	glBlendFunc(GL_ONE, GL_ZERO);
	glDisable(GL_BLEND);

	glDisable(GL_DEPTH_TEST);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	glBindVertexArray(0);

	if (e->effects & EF_WEAPON) {
		glDepthRange(0.f, 1.f);
	}

	r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities at the specified blend depth.
 */
void R_DrawMeshEntities(const r_view_t *view, int32_t blend_depth) {

	glUseProgram(r_mesh_program.name);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_MESH_MODEL(e->model)) {

			if (e->effects & EF_NO_DRAW) {
				continue;
			}

			if (e->blend_depth != blend_depth) {
				continue;
			}

			R_DrawMeshEntity(view, e);
		}
	}

	glUseProgram(0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_InitMeshProgram(void) {

	memset(&r_mesh_program, 0, sizeof(r_mesh_program));

	r_mesh_program.name = R_LoadProgram(
			R_ShaderDescriptor(GL_VERTEX_SHADER, "lightgrid.glsl", "material.glsl", "mesh_vs.glsl", NULL),
			R_ShaderDescriptor(GL_GEOMETRY_SHADER, "polylib.glsl", "mesh_gs.glsl", NULL),
			R_ShaderDescriptor(GL_FRAGMENT_SHADER, "lightgrid.glsl", "material.glsl", "mesh_fs.glsl", NULL),
			NULL);

	glUseProgram(r_mesh_program.name);

	r_mesh_program.uniforms_block = glGetUniformBlockIndex(r_mesh_program.name, "uniforms_block");
	glUniformBlockBinding(r_mesh_program.name, r_mesh_program.uniforms_block, 0);

	r_mesh_program.lights_block = glGetUniformBlockIndex(r_mesh_program.name, "lights_block");
	glUniformBlockBinding(r_mesh_program.name, r_mesh_program.lights_block, 1);

	r_mesh_program.in_position = glGetAttribLocation(r_mesh_program.name, "in_position");
	r_mesh_program.in_normal = glGetAttribLocation(r_mesh_program.name, "in_normal");
	r_mesh_program.in_tangent = glGetAttribLocation(r_mesh_program.name, "in_tangent");
	r_mesh_program.in_bitangent = glGetAttribLocation(r_mesh_program.name, "in_bitangent");
	r_mesh_program.in_diffusemap = glGetAttribLocation(r_mesh_program.name, "in_diffusemap");

	r_mesh_program.in_next_position = glGetAttribLocation(r_mesh_program.name, "in_next_position");
	r_mesh_program.in_next_normal = glGetAttribLocation(r_mesh_program.name, "in_next_normal");
	r_mesh_program.in_next_tangent = glGetAttribLocation(r_mesh_program.name, "in_next_tangent");
	r_mesh_program.in_next_bitangent = glGetAttribLocation(r_mesh_program.name, "in_next_bitangent");

	r_mesh_program.model = glGetUniformLocation(r_mesh_program.name, "model");

	r_mesh_program.lerp = glGetUniformLocation(r_mesh_program.name, "lerp");

	r_mesh_program.texture_material = glGetUniformLocation(r_mesh_program.name, "texture_material");
	r_mesh_program.texture_stage = glGetUniformLocation(r_mesh_program.name, "texture_stage");
	r_mesh_program.texture_lightgrid_ambient = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_ambient");
	r_mesh_program.texture_lightgrid_diffuse = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_diffuse");
	r_mesh_program.texture_lightgrid_direction = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_direction");
	r_mesh_program.texture_lightgrid_caustics = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_caustics");
	r_mesh_program.texture_lightgrid_fog = glGetUniformLocation(r_mesh_program.name, "texture_lightgrid_fog");
	r_mesh_program.texture_shadowmap = glGetUniformLocation(r_mesh_program.name, "texture_shadowmap");
	r_mesh_program.texture_shadowmap_cube = glGetUniformLocation(r_mesh_program.name, "texture_shadowmap_cube");

	r_mesh_program.color = glGetUniformLocation(r_mesh_program.name, "color");

	r_mesh_program.material.alpha_test = glGetUniformLocation(r_mesh_program.name, "material.alpha_test");
	r_mesh_program.material.roughness = glGetUniformLocation(r_mesh_program.name, "material.roughness");
	r_mesh_program.material.hardness = glGetUniformLocation(r_mesh_program.name, "material.hardness");
	r_mesh_program.material.specularity = glGetUniformLocation(r_mesh_program.name, "material.specularity");
	r_mesh_program.material.bloom = glGetUniformLocation(r_mesh_program.name, "material.bloom");

	r_mesh_program.stage.flags = glGetUniformLocation(r_mesh_program.name, "stage.flags");
	r_mesh_program.stage.color = glGetUniformLocation(r_mesh_program.name, "stage.color");
	r_mesh_program.stage.pulse = glGetUniformLocation(r_mesh_program.name, "stage.pulse");
	r_mesh_program.stage.scroll = glGetUniformLocation(r_mesh_program.name, "stage.scroll");
	r_mesh_program.stage.scale = glGetUniformLocation(r_mesh_program.name, "stage.scale");
	r_mesh_program.stage.shell = glGetUniformLocation(r_mesh_program.name, "stage.shell");

	r_mesh_program.tint_colors = glGetUniformLocation(r_mesh_program.name, "tint_colors");

	glUniform1i(r_mesh_program.texture_material, TEXTURE_MATERIAL);
	glUniform1i(r_mesh_program.texture_stage, TEXTURE_STAGE);
	glUniform1i(r_mesh_program.texture_lightgrid_ambient, TEXTURE_LIGHTGRID_AMBIENT);
	glUniform1i(r_mesh_program.texture_lightgrid_diffuse, TEXTURE_LIGHTGRID_DIFFUSE);
	glUniform1i(r_mesh_program.texture_lightgrid_direction, TEXTURE_LIGHTGRID_DIRECTION);
	glUniform1i(r_mesh_program.texture_lightgrid_caustics, TEXTURE_LIGHTGRID_CAUSTICS);
	glUniform1i(r_mesh_program.texture_lightgrid_fog, TEXTURE_LIGHTGRID_FOG);
	glUniform1i(r_mesh_program.texture_shadowmap, TEXTURE_SHADOWMAP);
	glUniform1i(r_mesh_program.texture_shadowmap_cube, TEXTURE_SHADOWMAP_CUBE);

	glUniform1i(r_mesh_program.stage.flags, STAGE_MATERIAL);

	glUseProgram(0);

	R_GetError(NULL);

	r_mesh_program.shell = (r_media_t *) R_LoadImage("textures/envmaps/envmap_3", IMG_PROGRAM);
	assert(r_mesh_program.shell);
}

/**
 * @brief
 */
void R_ShutdownMeshProgram(void) {

	glDeleteProgram(r_mesh_program.name);

	r_mesh_program.name = 0;
}
