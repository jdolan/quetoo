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
 * @brief Allocates an initializes the flare for the specified surface, if one
 * should be applied. The flare is linked to the provided BSP model and will
 * be freed automatically.
 */
void R_LoadFlare(r_bsp_model_t *bsp, r_bsp_face_t *face) {

	face->flare = Mem_LinkMalloc(sizeof(*face->flare), bsp);

	vec3_t mins = Vec3_Mins();
	vec3_t maxs = Vec3_Maxs();

	face->flare->origin = Vec3_Zero();
	for (int32_t i = 0; i < face->num_vertexes; i++) {
		mins = Vec3_Minf(mins, face->vertexes[i].position);
		maxs = Vec3_Maxf(maxs, face->vertexes[i].position);
	}

	face->flare->origin = Vec3_Scale(Vec3_Add(maxs, mins), .5f);
	face->flare->origin = Vec3_Add(face->flare->origin, Vec3_Scale(face->plane->cm->normal, 2.f));

	face->flare->size = Vec3_Distance(maxs, mins);

	const r_stage_t *s = face->texinfo->material->stages;
	while (s) {
		if (s->cm->flags & STAGE_FLARE) {
			break;
		}
		s = s->next;
	}
	assert(s);

	if (s->cm->flags & STAGE_COLOR) {
		face->flare->color = Color_Color32(s->cm->color);
	} else {
		face->flare->color = Color_Color32(color_white);
	}

	if (s->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		face->flare->size *= (s->cm->scale.s ? s->cm->scale.s : s->cm->scale.t);
	}

	face->flare->media = s->media;
	face->flare->softness = 1.f;
}

/**
 * @brief
 */
static void R_UpdateBspInlineModelFlares(r_view_t *view, const r_entity_t *e, const r_bsp_inline_model_t *in) {

	for (guint i = 0; i < in->flare_faces->len; i++) {

		const r_bsp_face_t *face = g_ptr_array_index(in->flare_faces, i);

		r_sprite_t flare = *face->flare;

		if (e) {
			Matrix4x4_Transform(&e->matrix, flare.origin.xyz, flare.origin.xyz);
		}

		flare.color.a = (byte) Clampf(flare.color.a * r_flares->value, 0.f, 255.f);

		R_AddSprite(view, &flare);
	}
}

/**
 * @brief
 */
void R_UpdateFlares(r_view_t *view) {

	if (!r_flares->value || r_draw_wireframe->value) {
		return;
	}

	R_UpdateBspInlineModelFlares(view, NULL, r_world_model->bsp->inline_models);

	const r_entity_t *e = view->entities;
	for (int32_t i = 0; i < view->num_entities; i++, e++) {
		if (IS_BSP_INLINE_MODEL(e->model)) {
			R_UpdateBspInlineModelFlares(view, e, e->model->bsp_inline);
		}
	}
}
