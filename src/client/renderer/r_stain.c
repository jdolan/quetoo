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
 * @brief Attempt to stain the surface.
 */
static void R_StainFace(const r_stain_t *stain, r_bsp_face_t *face) {

	vec3_t point;
	Matrix4x4_Transform(&face->lightmap.matrix, stain->origin.xyz, point.xyz);

	vec2_t st = Vec2_Subtract(Vec3_XY(point), face->lightmap.st_mins);

	const vec2_t size = Vec2_Subtract(face->lightmap.st_maxs, face->lightmap.st_mins);
	const vec2_t padding = Vec2_Subtract(Vec2(face->lightmap.w, face->lightmap.h), size);

	st = Vec2_Add(st, Vec2_Scale(padding, .5f));

	// convert the radius to luxels
	const float radius = stain->radius / r_world_model->bsp->luxel_size;

	// square it to avoid a sqrt per luxe;
	const float radius_squared = radius * radius;

	// now iterate the luxels within the radius and stain them
	for (int32_t i = -radius; i <= radius; i++) {

		const ptrdiff_t t = st.y + i;
		if (t < 0 || t >= face->lightmap.h) {
			continue;
		}

		for (int32_t j = -radius; j <= radius; j++) {

			const ptrdiff_t s = st.x + j;
			if (s < 0 || s >= face->lightmap.w) {
				continue;
			}

			// this luxel is stained, so attenuate and blend it
			byte *stainmap = face->lightmap.stainmap + (face->lightmap.w * t + s) * BSP_LIGHTMAP_BPP;

			const float dist_squared = Vec2_LengthSquared(Vec2(i, j));
			const float atten = (radius_squared - dist_squared) / radius_squared;

			const float intensity = stain->color.a * atten * r_stains->value;

			const float src_alpha = Clampf(intensity, 0.0, 1.0);
			const float dst_alpha = 1.0 - src_alpha;

			const color_t src = Color_Scale(stain->color, src_alpha);
			const color_t dst = Color_Scale(Color3b(stainmap[0], stainmap[1], stainmap[2]), dst_alpha);

			const color32_t out = Color_Color32(Color_Add(src, dst));

			stainmap[0] = out.r;
			stainmap[1] = out.g;
			stainmap[2] = out.b;

			face->stain_frame = r_locals.stain_frame;
		}
	}
}

/**
 * @brief
 */
static void R_StainNode(const r_stain_t *stain, const r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		return;
	}

	const float dist = Cm_DistanceToPlane(stain->origin, node->plane->cm);

	if (dist > stain->radius) { // front only
		R_StainNode(stain, node->children[0]);
		return;
	}

	if (dist < -stain->radius) { // back only
		R_StainNode(stain, node->children[1]);
		return;
	}

	// project the stain onto the node's plane

	const r_stain_t local = {
		.origin = Vec3_Add(stain->origin, Vec3_Scale(node->plane->cm->normal, -dist)),
		.radius = stain->radius - fabsf(dist),
		.color = stain->color
	};

	const int32_t side = dist > 0.f ? 0 : 1;

	r_bsp_face_t *face = node->faces;
	for (int32_t i = 0; i < node->num_faces; i++, face++) {

		if (face->plane_side != side) {
			continue;
		}

		if (face->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		R_StainFace(&local, face);
	}

	// recurse down both sides
	R_StainNode(stain, node->children[0]);
	R_StainNode(stain, node->children[1]);
}

/**
 * @brief
 */
void R_AddStain(const r_stain_t *stain) {

	if (r_view.num_stains == MAX_STAINS) {
		Com_Warn("MAX_STAINS\n");
		return;
	}

	r_view.stains[r_view.num_stains++] = *stain;
}

/**
 * @brief
 */
void R_UpdateStains(void) {

	if (!r_view.num_stains) {
		return;
	}

	if (r_world_model->bsp->lightmap == NULL) {
		return;
	}

	r_locals.stain_frame++;

	const r_stain_t *stain = r_view.stains;
	for (int32_t i = 0; i < r_view.num_stains; i++, stain++) {

		R_StainNode(stain, r_world_model->bsp->nodes);

		const r_entity_t *e = r_view.entities;
		for (int32_t j = 0; j < r_view.num_entities; j++, e++) {
			if (e->model && e->model->type == MOD_BSP_INLINE) {

				r_stain_t s = *stain;
				Matrix4x4_Transform(&e->inverse_matrix, s.origin.xyz, s.origin.xyz);

				R_StainNode(&s, e->model->bsp_inline->head_node);
			}
		}
	}

	const r_bsp_model_t *bsp = r_world_model->bsp;

	glBindTexture(GL_TEXTURE_2D_ARRAY, bsp->lightmap->atlas->texnum);

	const r_bsp_face_t *face = bsp->faces;
	for (int32_t i = 0; i < bsp->num_faces; i++, face++) {

		if (face->stain_frame == r_locals.stain_frame) {
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
					0,
					face->lightmap.s,
					face->lightmap.t,
					BSP_LIGHTMAP_LAYERS,
					face->lightmap.w,
					face->lightmap.h,
					1,
					GL_RGB,
					GL_UNSIGNED_BYTE,
					face->lightmap.stainmap);
		}
	}

	R_GetError(NULL);
}
