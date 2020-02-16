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
_Bool R_CullMeshEntity(const r_entity_t *e) {
	vec3_t mins, maxs;

	if (e->effects & EF_WEAPON) { // never cull the weapon
		return false;
	}

	// calculate scaled bounding box in world space

	mins = Vec3_Add(e->origin, Vec3_Scale(e->model->mins, e->scale));
	maxs = Vec3_Add(e->origin, Vec3_Scale(e->model->maxs, e->scale));

	return R_CullBox(mins, maxs);
}

/**
 * @brief Returns the desired tag structure, or `NULL`.
 * @param mod The model to check for the specified tag.
 * @param frame The frame to fetch the tag on.
 * @param name The name of the tag.
 * @return The tag structure.
*/
const r_mesh_tag_t *R_MeshTag(const r_model_t *mod, const char *name, const int32_t frame) {

	if (frame > mod->mesh->num_frames) {
		Com_Warn("%s: Invalid frame: %d\n", mod->media.name, frame);
		return NULL;
	}

	const r_mesh_model_t *model = mod->mesh;
	const r_mesh_tag_t *tag = &model->tags[frame * model->num_tags];

	for (int32_t i = 0; i < model->num_tags; i++, tag++) {
		if (!g_strcmp0(name, tag->name)) {
			return tag;
		}
	}

	Com_Warn("%s: Tag not found: %s\n", mod->media.name, name);
	return NULL;
}
