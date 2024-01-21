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

	glUniform1i(r_model_program.stage.flags, stage->cm->flags);

	if (stage->cm->flags & STAGE_COLOR) {
		glUniform4fv(r_model_program.stage.color, 1, stage->cm->color.rgba);
	}

	if (stage->cm->flags & STAGE_PULSE) {
		glUniform1f(r_model_program.stage.pulse, stage->cm->pulse.hz);
	}

	if (stage->cm->flags & (STAGE_SCROLL_S | STAGE_SCROLL_T)) {
		glUniform2f(r_model_program.stage.scroll, stage->cm->scroll.s, stage->cm->scroll.t);
	}

	if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		glUniform2f(r_model_program.stage.scale, stage->cm->scale.s, stage->cm->scale.t);
	}

	if (stage->cm->flags & STAGE_SHELL) {
		const ptrdiff_t old_frame_offset = e->old_frame * face->num_vertexes * sizeof(vec3_t);
		const ptrdiff_t frame_offset = e->frame * face->num_vertexes * sizeof(vec3_t);

		glUniform1f(r_model_program.stage.shell, stage->cm->shell.radius);
		
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
		.media = (r_media_t *) r_model_program.shell
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

	glUniform1i(r_model_program.stage.flags, STAGE_MATERIAL);

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

	glVertexAttribPointer(7, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, position)));
	glVertexAttribPointer(8, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, normal)));
	glVertexAttribPointer(9, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, tangent)));
	glVertexAttribPointer(10, 3, GL_FLOAT, GL_FALSE, sizeof(r_mesh_vertex_t), (void *) (frame_offset + offsetof(r_mesh_vertex_t, bitangent)));

	glBindTexture(GL_TEXTURE_2D_ARRAY, material->texture->texnum);

	glUniform1f(r_model_program.material.alpha_test, material->cm->alpha_test * r_alpha_test->value);
	glUniform1f(r_model_program.material.roughness, material->cm->roughness * r_roughness->value);
	glUniform1f(r_model_program.material.hardness, material->cm->hardness * r_hardness->value);
	glUniform1f(r_model_program.material.specularity, material->cm->specularity * r_specularity->value);
	glUniform1f(r_model_program.material.bloom, material->cm->bloom * r_bloom->value);

	if (*material->cm->tintmap.path) {
		vec4_t tints[3];

		memcpy(tints, e->tints, sizeof(tints));

		for (size_t i = 0; i < lengthof(tints); i++) {
			if (!e->tints[i].w) {
				tints[i] = material->cm->tintmap_defaults[i];
			}
		}

		//glUniform4fv(r_model_program.tint_colors, 3, tints[0].xyzw);
	}

	float alpha = 1.f;
	switch (material->cm->surface & SURF_MASK_BLEND) {
		case SURF_BLEND_33:
			alpha = .333f;
			break;
		case SURF_BLEND_66:
			alpha = .666f;
			break;
		default:
			break;
	}

	glVertexAttrib4f(r_model_program.in_color, 1.f, 1.f, 1.f, alpha);

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

	R_UseModelProgram(e, e->model);

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

	if (e->effects & EF_WEAPON) {
		glDepthRange(0.f, 1.f);
	}

	r_stats.mesh_models++;
}

/**
 * @brief Draws mesh entities at the specified blend depth.
 */
void R_DrawMeshEntities(const r_view_t *view, int32_t blend_depth) {

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

	R_GetError(NULL);
}
