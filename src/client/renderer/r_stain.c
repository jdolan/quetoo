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
static void R_UpdateStain(const r_view_t *view, const r_stain_t *stain) {

	const r_bsp_voxels_t *lg = r_models.world->bsp->voxels;

	const vec3_t translate = Vec3_Subtract(stain->origin, lg->bounds.mins);
	const vec3i_t origin = Vec3_CastVec3i(Vec3_Divide(translate, lg->voxel_size));

	const int32_t radius = stain->radius / BSP_VOXEL_SIZE;

	for (int32_t z = -radius; z < radius; z++) {
		for (int32_t y = -radius; y < radius; y++) {
			for (int32_t x = -radius; x < radius; x++) {

				// this voxel is stained, so attenuate and blend it

				const vec3i_t pos = Vec3i_Add(origin, Vec3i(x, y, z));

				const int32_t voxel = lg->size.x * lg->size.y * pos.z + lg->size.x * pos.y + pos.x;

				color32_t *out = lg->stain_buffer + voxel;

				const float atten = 1.f;

				const float intensity = stain->color.a * atten;

				const float src_alpha = Clampf01(intensity);
				const float dst_alpha = 1.f - src_alpha;

				const color_t src = Color_Scale(stain->color, src_alpha);
				const color_t dst = Color_Scale(Color32_Color(*out), dst_alpha);

				*out = Color_Color32(Color_Add(src, dst));
			}
		}
	}
}

/**
 * @brief
 */
void R_AddStain(r_view_t *view, const r_stain_t *stain) {

	if (!r_stains->value) {
		return;
	}

	if (view->num_stains == MAX_STAINS) {
		Com_Warn("MAX_STAINS\n");
		return;
	}

	if (R_OccludeSphere(view, stain->origin, stain->radius)) {
		return;
	}

	view->stains[view->num_stains++] = *stain;
}

/**
 * @brief
 */
void R_UpdateStains(const r_view_t *view) {

	const r_stain_t *stain = view->stains;
	for (int32_t i = 0; i < view->num_stains; i++, stain++) {
		R_UpdateStain(view, stain);
	}

	const r_image_t *s = r_models.world->bsp->voxels->stains;
	const color32_t *c = r_models.world->bsp->voxels->stain_buffer;

	glActiveTexture(GL_TEXTURE0 + TEXTURE_VOXEL_STAINS);
	glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, 0, s->width, s->height, s->depth, s->format, s->pixel_type, c);
	glGenerateMipmap(GL_TEXTURE_3D);
	glActiveTexture(GL_TEXTURE0 + TEXTURE_DIFFUSEMAP);

	R_GetError(NULL);
}
