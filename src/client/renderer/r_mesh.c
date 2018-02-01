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
#include "r_gl.h"

r_mesh_state_t r_mesh_state;

/**
 * @brief Applies any client-side transformations specified by the model's world or
 * view configuration structure.
 */
void R_ApplyMeshModelConfig(r_entity_t *e) {

	assert(IS_MESH_MODEL(e->model));

	const r_mesh_config_t *c;
	if (e->effects & EF_WEAPON) {
		c = &e->model->mesh->view_config;
	} else if (e->effects & EF_LINKED) {
		c = &e->model->mesh->link_config;
	} else {
		c = &e->model->mesh->world_config;
	}

	Matrix4x4_ConcatTranslate(&e->matrix, c->translate[0], c->translate[1], c->translate[2]);
	Matrix4x4_ConcatRotate(&e->matrix, c->rotate[0], 1.0, 0.0, 0.0);
	Matrix4x4_ConcatRotate(&e->matrix, c->rotate[1], 0.0, 1.0, 0.0);
	Matrix4x4_ConcatRotate(&e->matrix, c->rotate[2], 0.0, 0.0, 1.0);
	Matrix4x4_ConcatScale(&e->matrix, c->scale);

	if (e->effects & EF_WEAPON) {
		vec3_t bob = { 0.2, 0.4, 0.2 };
		VectorScale(bob, r_view.bob, bob);

		Matrix4x4_ConcatTranslate(&e->matrix, bob[0], bob[1], bob[2]);
	}

	e->effects |= c->flags;
}

/**
 * @brief Returns the desired tag structure, or `NULL`.
 * @param mod The model to check for the specified tag.
 * @param frame The frame to fetch the tag on.
 * @param name The name of the tag.
 * @return The tag structure.
*/
const r_model_tag_t *R_MeshModelTag(const r_model_t *mod, const char *name, const int32_t frame) {

	if (frame > mod->mesh->num_frames) {
		Com_Warn("%s: Invalid frame: %d\n", mod->media.name, frame);
		return NULL;
	}

	const r_mesh_model_t *model = mod->mesh;
	const r_model_tag_t *tag = &model->tags[frame * model->num_tags];

	for (int32_t i = 0; i < model->num_tags; i++, tag++) {
		if (!g_strcmp0(name, tag->name)) {
			return tag;
		}
	}

	Com_Warn("%s: Tag not found: %s\n", mod->media.name, name);
	return NULL;
}

/**
 * @return True if the specified entity was frustum-culled and can be skipped.
 */
_Bool R_CullMeshModel(const r_entity_t *e) {
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
 * @brief Updates static lighting information for the specified mesh entity.
 */
void R_UpdateMeshModelLighting(const r_entity_t *e) {

	if (e->effects & EF_NO_LIGHTING) {
		return;
	}

	if (e->lighting->state != LIGHTING_READY) {

		// update the origin and bounds based on the entity
		if (e->effects & EF_WEAPON) {
			VectorCopy(r_view.origin, e->lighting->origin);
		} else {
			VectorCopy(e->origin, e->lighting->origin);
		}

		e->lighting->radius = e->scale * e->model->radius;

		// calculate scaled bounding box in world space
		VectorMA(e->lighting->origin, e->scale, e->model->mins, e->lighting->mins);
		VectorMA(e->lighting->origin, e->scale, e->model->maxs, e->lighting->maxs);
	}

	R_UpdateLighting(e->lighting);
}

/**
 * @brief Sets the shade color for the mesh by modulating any preset color
 * with static lighting.
 */
static void R_SetMeshColor_default(const r_entity_t *e) {
	vec4_t color;

	if ((e->effects & EF_NO_LIGHTING) == 0 && r_state.max_active_lights) {
		VectorCopy(r_bsp_light_state.ambient, color);

		if (!r_lighting->value) {
			const r_illumination_t *il = e->lighting->illuminations;

			for (uint16_t i = 0; i < r_state.max_active_lights; i++, il++) {

				if (il->diffuse == 0.0) {
					break;
				}

				VectorMA(color, il->diffuse / il->light.radius, il->light.color, color);
			}

			ColorNormalize(color, color);
		}
	} else {
		VectorSet(color, 1.0, 1.0, 1.0);
	}

	for (int32_t i = 0; i < 3; i++) {
		color[i] *= e->color[i];
	}

	if (e->effects & EF_BLEND) {
		color[3] = Clamp(e->color[3], 0.0, 1.0);
	} else {
		color[3] = 1.0;
	}

	R_Color(color);

	Vector4Copy(color, r_mesh_state.color);
}

/**
 * @brief Populates hardware light sources with illumination information.
 */
static void R_ApplyMeshModelLighting_default(const r_entity_t *e) {
	uint16_t i;

	for (i = 0; i < r_state.max_active_lights; i++) {
		const r_illumination_t *il = &e->lighting->illuminations[i];

		if (il->diffuse == 0.0) {
			break;
		}

		r_state.active_program->UseLight(i, &r_mesh_state.world_view, &il->light);
	}

	if (i < r_state.max_active_lights) { // disable the next light as a stop
		r_state.active_program->UseLight(i, &r_mesh_state.world_view, NULL);
	}
}

/**
 * @brief Sets up the texture for tinting. Do this after UseMaterial.
 */
static void R_UpdateMeshTints(void) {

	if (r_mesh_state.material && r_mesh_state.material->tintmap) {
		R_EnableTexture(texunit_tint, true);
		R_UseTints();
	} else {
		R_EnableTexture(texunit_tint, false);
	}
}

/**
 * @brief Sets renderer state for the specified entity.
 */
static void R_SetModelState_default(const r_entity_t *e) {

	// setup VBO states
	R_SetArrayState(e->model);
	
	if (e->effects & EF_WEAPON) {
		R_DepthRange(0.0, 0.3);
	}

	R_RotateForEntity(e);

	// setup lerp for animating models
	if (e->old_frame != e->frame) {
		R_UseInterpolation(e->lerp);
	}
}

/**
 * @brief Sets renderer state for the specified mesh.
 */
static void R_SetMeshState_default(const r_entity_t *e, const uint16_t mesh_index, const r_model_mesh_t *mesh) {

	r_material_t *material = (r_draw_wireframe->value) ? NULL : ((mesh_index < 32 && e->skins[mesh_index]) ? e->skins[mesh_index] : mesh->material);

	r_mesh_state.material = material;

	if (!r_draw_wireframe->value) {

		R_BindDiffuseTexture(material->diffuse->texnum);

		R_SetMeshColor_default(e);

		if (e->effects & EF_ALPHATEST) {
			R_EnableAlphaTest(ALPHA_TEST_ENABLED_THRESHOLD);
		}

		if (e->effects & EF_BLEND) {
			R_EnableBlend(true);
			R_EnableDepthMask(false);
		}

		if ((e->effects & EF_NO_LIGHTING) == 0 && r_state.lighting_enabled) {

			R_ApplyMeshModelLighting_default(e);
		}
	} else {
		R_Color(NULL);
	}

	R_UseMaterial(r_mesh_state.material);

	R_UpdateMeshTints();
}

/**
 * @brief Restores renderer state for the given entity.
 */
static void R_ResetModelState_default(const r_entity_t *e) {

	R_RotateForEntity(NULL);

	if (e->effects & EF_WEAPON) {
		R_DepthRange(0.0, 1.0);
	}

	if (e->effects & EF_BLEND) {
		R_EnableBlend(false);
		R_EnableDepthMask(true);
	}

	if (e->effects & EF_ALPHATEST) {
		R_EnableAlphaTest(ALPHA_TEST_DISABLED_THRESHOLD);
	}

	R_ResetArrayState();

	R_UseInterpolation(0.0);

	R_EnableTexture(texunit_tint, false);
}

/**
 * @brief Returns whether or not the main diffuse stage of a mesh should be rendered.
 */
static _Bool R_DrawMeshDiffuse_default(void) {
	return r_draw_wireframe->value || !r_materials->value || !r_mesh_state.material->cm->only_stages;
}

/**
 * @brief Draw the diffuse pass of each mesh segment for the specified model.
 */
static void R_DrawMeshParts_default(const r_entity_t *e, const r_mesh_model_t *model) {
	uint32_t offset = 0;
	const r_model_mesh_t *mesh = model->meshes;

	for (uint16_t i = 0; i < model->num_meshes; i++, mesh++) {

		R_SetMeshState_default(e, i, mesh);

		if (!R_DrawMeshDiffuse_default()) {
			offset += mesh->num_elements;
			continue;
		}

		R_DrawArrays(GL_TRIANGLES, offset, mesh->num_elements);

		offset += mesh->num_elements;
	}
}

/**
 * @brief Draw the material passes of each mesh segment for the specified model.
 */
static void R_DrawMeshPartsMaterials_default(const r_entity_t *e, const r_mesh_model_t *model) {
	uint32_t offset = 0;
	const r_model_mesh_t *mesh = model->meshes;

	for (uint16_t i = 0; i < model->num_meshes; i++, mesh++) {

		R_SetMeshState_default(e, i, mesh);

		R_DrawMeshMaterial(r_mesh_state.material, offset, mesh->num_elements);

		offset += mesh->num_elements;
	}
}

/**
 * @brief Draws the mesh model for the given entity. This only draws the base model.
 */
void R_DrawMeshModel_default(const r_entity_t *e) {

	r_view.current_entity = e;

	R_SetModelState_default(e);

	R_DrawMeshParts_default(e, e->model->mesh);

	r_view.num_mesh_tris += e->model->num_tris;
	r_view.num_mesh_models++;

	R_ResetModelState_default(e); // reset state
}

/**
 * @brief Draws the mesh materials for the given entity.
 */
void R_DrawMeshModelMaterials_default(const r_entity_t *e) {

	if (r_draw_wireframe->value || !r_materials->value) {
		return;
	}

	r_view.current_entity = e;

	R_SetModelState_default(e);
	
	R_DrawMeshPartsMaterials_default(e, e->model->mesh);

	R_ResetModelState_default(e); // reset state
}

/**
 * @brief Draws all mesh models for the current frame.
 */
void R_DrawMeshModels_default(const r_entities_t *ents) {

	R_EnableLighting(program_default, true);

	R_GetMatrix(R_MATRIX_MODELVIEW, &r_mesh_state.world_view);

	if (r_draw_wireframe->value) {
		R_BindDiffuseTexture(r_image_state.null->texnum);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	}

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW) {
			continue;
		}

		R_DrawMeshModel_default(e);
	}

	for (size_t i = 0; i < ents->count; i++) {
		const r_entity_t *e = ents->entities[i];

		if (e->effects & EF_NO_DRAW) {
			continue;
		}

		r_view.current_entity = e;

		R_DrawMeshModelMaterials_default(e);
	}

	r_view.current_entity = NULL;

	if (r_draw_wireframe->value) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}

	R_EnableLighting(NULL, false);

	R_EnableBlend(true);

	R_EnableDepthMask(false);

	R_DrawMeshShadows_default(ents);

	R_DrawMeshShells_default(ents);

	R_EnableBlend(false);

	R_EnableDepthMask(true);
}
