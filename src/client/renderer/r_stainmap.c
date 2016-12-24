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
#include "client.h"

extern cl_client_t cl;

/**
 * @brief
 */
static void R_StainNode(const r_stain_t *stain, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		if (!node->model) { // and not a bsp submodel
			return;
		}
	}

	const vec_t dist = DotProduct(stain->origin, node->plane->normal) - node->plane->dist;

	if (dist > stain->radius) { // front only
		R_StainNode(stain, node->children[0]);
		return;
	}

	if (dist < -stain->radius) { // back only
		R_StainNode(stain, node->children[1]);
		return;
	}

	const vec_t src_stain_alpha = stain->color[3] * r_stainmap->value;
	const vec_t dst_stain_alpha = 1.0 - src_stain_alpha;

	const vec_t step = r_model_state.world->bsp->lightmaps->scale;

	r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;

	for (uint32_t i = 0; i < node->num_surfaces; i++, surf++) {

		if (!surf->lightmap || !surf->stainmap) {
			continue;
		}

		if (surf->vis_frame != r_locals.vis_frame) {
			if (!node->model) { // and not a bsp submodel
				continue;
			}
		}

		const r_bsp_texinfo_t *tex = surf->texinfo;

		if ((tex->flags & (SURF_SKY | SURF_BLEND_33 | SURF_BLEND_66 | SURF_WARP))) {
			continue;
		}

		vec_t face_dist = DotProduct(stain->origin, surf->plane->normal) - surf->plane->dist;

		if (surf->flags & R_SURF_PLANE_BACK) {
			face_dist *= -1.0;
		}

		const vec_t intensity = stain->radius - fabs(face_dist);
		if (intensity < 0.0) {
			continue;
		}

		vec3_t point;
		VectorMA(stain->origin, -face_dist, surf->plane->normal, point);

		const vec2_t point_st = {
			DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
			DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
		};

		const r_pixel_t smax = surf->st_extents[0];
		const r_pixel_t tmax = surf->st_extents[1];

		byte *buffer = surf->stainmap_buffer;

		vec_t tstep = 0.0;

		for (uint16_t t = 0; t < tmax; t++, tstep += step) {
			const uint32_t td = (uint32_t) fabs(point_st[1] - tstep);

			vec_t sstep = 0.0;

			for (uint16_t s = 0; s < smax; s++, sstep += step, buffer += 3) {
				const uint32_t sd = (uint32_t) fabs(point_st[0] - sstep);

				vec_t sample_dist;
				if (sd > td) {
					sample_dist = sd + (td / 2);
				} else {
					sample_dist = td + (sd / 2);
				}

				if (sample_dist >= intensity) {
					continue;
				}

				for (uint32_t j = 0; j < 3; j++) {

					const vec_t src = stain->color[j] * src_stain_alpha;
					const vec_t dst = buffer[j] * dst_stain_alpha / 255.0;

					const uint8_t c = (uint8_t) (Clamp(src + dst, 0.0, 1.0) * 255.0);

					if (buffer[j] != c) {
						buffer[j] = c;
						surf->stainmap_dirty = true;
					}
				}
			}
		}

		if (!surf->stainmap_dirty) {
			continue;
		}

		R_BindDiffuseTexture(surf->stainmap->texnum);
		glTexSubImage2D(GL_TEXTURE_2D, 0, surf->lightmap_s, surf->lightmap_t, smax, tmax, GL_RGB,
						GL_UNSIGNED_BYTE, surf->stainmap_buffer);

		R_GetError(tex->name);
	}

	// recurse down both sides

	R_StainNode(stain, node->children[0]);
	R_StainNode(stain, node->children[1]);
}

/**
 * @brief Add a stain to the map.
 */
void R_AddStain(const r_stain_t *s) {

	if (!r_stainmap->value) {
		return;
	}

	if (r_view.num_stains == MAX_STAINS) {
		Com_Debug(DEBUG_RENDERER, "MAX_STAINS reached\n");
		return;
	}

	r_view.stains[r_view.num_stains++] = *s;
}

/**
 * @brief Adds new stains from the view each frame.
 */
void R_AddStains(void) {

	const r_stain_t *s = r_view.stains;
	for (int32_t i = 0; i < r_view.num_stains; i++, s++) {

		R_StainNode(s, r_model_state.world->bsp->nodes);

		for (uint16_t i = 0; i < cl.frame.num_entities; i++) {

			const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
			const entity_state_t *st = &cl.entity_states[snum];

			if (st->solid != SOLID_BSP) {
				continue;
			}

			const cl_entity_t *ent = &cl.entities[st->number];
			const cm_bsp_model_t *mod = cl.cm_models[st->model1];

			if (mod == NULL || mod->head_node == -1) {
				continue;
			}

			r_bsp_node_t *node = &r_model_state.world->bsp->nodes[mod->head_node];

			r_stain_t stain = *s;
			Matrix4x4_Transform(&ent->inverse_matrix, s->origin, stain.origin);

			R_StainNode(&stain, node);
		}
	}
}

/**
 * @brief Resets the stainmaps that we have loaded, for level changes. This is kind of a
 * slow function, so be careful calling this one.
 */
void R_ResetStainmap(void) {

	GHashTable *hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, Mem_Free);

	for (uint32_t s = 0; s < r_model_state.world->bsp->num_surfaces; s++) {
		r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + s;

		// if this surf was never/can't be stained, don't bother
		if (!surf->stainmap || !surf->stainmap_dirty) {
			continue;
		}

		byte *lightmap = (byte *) g_hash_table_lookup(hash, surf->stainmap);

		// load the original lightmap data in scratch buffer
		if (!lightmap) {

			lightmap = Mem_Malloc(surf->lightmap->width * surf->lightmap->height * 3);

			R_BindDiffuseTexture(surf->lightmap->texnum);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			// copy the lightmap over to the stainmap
			R_BindDiffuseTexture(surf->stainmap->texnum);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->lightmap->width, surf->lightmap->height, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			g_hash_table_insert(hash, surf->stainmap, lightmap);
		}

		// copy over the old stain buffer
		const size_t stride = surf->lightmap->width * 3;
		const size_t stain_width = surf->st_extents[0] * 3;
		const size_t lightmap_offset = (surf->lightmap_t * surf->lightmap->width + surf->lightmap_s) * 3;

		byte *light = lightmap + lightmap_offset;
		byte *stain = surf->stainmap_buffer;

		for (int16_t t = 0; t < surf->st_extents[1]; t++) {

			memcpy(stain, light, stain_width);

			stain += stain_width;
			light += stride;
		}

		surf->stainmap_dirty = false;
	}

	g_hash_table_destroy(hash);
}
