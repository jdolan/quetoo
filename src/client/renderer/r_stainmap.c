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

static GHashTable *r_surfs_stained;

static byte *r_stain_circle;
static size_t r_stain_circle_size;

#define STAIN_BYTE_FROM_XY(x, y) \
	stain[((y) * stain_size) + (x)]

static vec_t R_BilinearFilterStain(const byte *stain, const size_t stain_size, vec_t u, vec_t v) {
	u = u * stain_size - 0.5;
	v = v * stain_size - 0.5;

	int32_t x = floor(u);
	int32_t y = floor(v);

	vec_t u_ratio = u - x;
	vec_t v_ratio = v - y;
	vec_t u_opposite = 1.0 - u_ratio;
	vec_t v_opposite = 1.0 - v_ratio;

	return (STAIN_BYTE_FROM_XY(x, y)   * u_opposite  + STAIN_BYTE_FROM_XY(x+1, y)   * u_ratio) * v_opposite + 
		   (STAIN_BYTE_FROM_XY(x, y+1) * u_opposite  + STAIN_BYTE_FROM_XY(x+1, y+1) * u_ratio) * v_ratio;
}

/**
 * @brief Attempt to stain the surface. Returns true if any pixels were modified.
 */
static _Bool R_StainSurface(const r_stain_t *stain, r_bsp_surface_t *surf) {

	_Bool surf_touched = false;

	// determine if the surface is within range
	const vec_t dist = R_DistanceToSurface(stain->origin, surf);

	if (fabs(dist) > stain->radius) {
		return false;
	}

	// project the stain onto the plane, in world space
	vec3_t point;
	VectorMA(stain->origin, -dist, surf->plane->normal, point);

	// determine if the stain is on the front of the surface
	vec3_t dir;
	VectorSubtract(point, stain->origin, dir);

	if (DotProduct(dir, surf->plane->normal) > 0.0) {
		return false;
	}

	const r_bsp_texinfo_t *tex = surf->texinfo;

	// transform the impact point into texture space
	vec2_t point_st = {
		DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
		DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
	};

	// and convert to lightmap space
	point_st[0] *= r_model_state.world->bsp->lightmap_scale;
	point_st[1] *= r_model_state.world->bsp->lightmap_scale;

	// resolve the radius of the stain where it impacts the surface
	const vec_t radius = sqrt(stain->radius * stain->radius - dist * dist);

	// transform the radius into lightmap space, accounting for unevenly scaled textures
	const int32_t radius_st = (int32_t) ceil((radius / tex->scale[0]) * r_model_state.world->bsp->lightmap_scale);
	
	point_st[0] -= radius_st / 2.0;
	point_st[1] -= radius_st / 2.0;
	
	const int32_t point_st_round[2] = {
		round(point_st[0]),
		round(point_st[1])
	};

	byte *buffer = surf->stainmap_buffer;

	// iterate the luxels and stain the ones that are within reach
	for (uint16_t t = 0; t < surf->lightmap_size[1]; t++) {

		for (uint16_t s = 0; s < surf->lightmap_size[0]; s++, buffer += 3) {

			if (s < (point_st_round[0]) || t < (point_st_round[1]) || 
				s >= (point_st_round[0] + radius_st) || t >= (point_st_round[1] + radius_st)) {
				continue;
			}
			
			const vec_t delta_s = (s - point_st[0]) / radius_st;
			const vec_t delta_t = (t - point_st[1]) / radius_st;

			const vec_t atten = R_BilinearFilterStain(r_stain_circle, r_stain_circle_size, delta_s, delta_t) / 255.0;

			if (atten <= 0.0) {
				continue;
			}

			const vec_t intensity = stain->color[3] * atten * r_stainmap->value;

			const vec_t src_alpha = Clamp(intensity, 0.0, 1.0);
			const vec_t dst_alpha = 1.0 - src_alpha;

			for (uint32_t j = 0; j < 3; j++) {

				const vec_t src = stain->color[j] * src_alpha;
				const vec_t dst = (buffer[j] / 255.0) * dst_alpha;

				buffer[j] = (uint8_t) (Clamp(src + dst, 0.0, 1.0) * 255.0);
			}

			surf_touched = true;
		}
	}

	return surf_touched;
}

/**
 * @brief
 */
static void R_StainNode(const r_stain_t *stain, const r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		if (!node->model) {
			return;
		}
	}

	const vec_t dist = Cm_DistanceToPlane(stain->origin, node->plane);

	if (dist > stain->radius * 2.0) { // front only
		R_StainNode(stain, node->children[0]);
		return;
	}

	if (dist < -stain->radius * 2.0) { // back only
		R_StainNode(stain, node->children[1]);
		return;
	}

	r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;

	for (uint32_t i = 0; i < node->num_surfaces; i++, surf++) {

		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP)) {
			continue;
		}

		if (!surf->lightmap || !surf->stainmap) {
			continue;
		}

		if (surf->vis_frame != r_locals.vis_frame) {
			if (!node->model) {
				continue;
			}
		}

		if (R_StainSurface(stain, surf)) {
			g_hash_table_add(r_surfs_stained, surf);
		}
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
 * @brief
 */
static void R_AddStains_UploadSurfaces(gpointer key, gpointer value, gpointer userdata) {

	r_bsp_surface_t *surf = (r_bsp_surface_t *) value;

	R_BindDiffuseTexture(surf->stainmap->texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0,
	                surf->lightmap_s, surf->lightmap_t,
	                surf->lightmap_size[0], surf->lightmap_size[1],
	                GL_RGB,
	                GL_UNSIGNED_BYTE, surf->stainmap_buffer);

	R_GetError(surf->texinfo->name);

	// mark the surface as having been modified, so reset knows it's resettable
	surf->stainmap_dirty = true;
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

	g_hash_table_foreach(r_surfs_stained, R_AddStains_UploadSurfaces, NULL);
	g_hash_table_remove_all(r_surfs_stained);
}

/**
 * @brief Resets the stainmaps that we have loaded, for level changes. This is kind of a
 * slow function, so be careful calling this one.
 */
void R_ResetStainmap(void) {

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	GHashTable *hash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, Mem_Free);

	for (uint32_t s = 0; s < r_model_state.world->bsp->num_surfaces; s++) {
		r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + s;

		// skip if we don't have a stainmap or we weren't stained
		if (!surf->stainmap || !surf->stainmap_dirty) {
			continue;
		}

		byte *lightmap = (byte *) g_hash_table_lookup(hash, surf->stainmap);
		if (!lightmap) {

			lightmap = Mem_Malloc(surf->lightmap->width * surf->lightmap->height * 3);

			R_BindDiffuseTexture(surf->lightmap->texnum);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			R_BindDiffuseTexture(surf->stainmap->texnum);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->lightmap->width, surf->lightmap->height,
			             0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			g_hash_table_insert(hash, surf->stainmap, lightmap);
		}

		const size_t offset = (surf->lightmap_t *surf->lightmap->width + surf->lightmap_s) * 3;
		const size_t stride = surf->lightmap->width * 3;

		byte *lm = lightmap + offset;
		byte *sm = surf->stainmap_buffer;

		for (int16_t t = 0; t < surf->lightmap_size[1]; t++) {

			memcpy(sm, lm, surf->lightmap_size[0] * 3);

			sm += surf->lightmap_size[0] * 3;
			lm += stride;
		}

		surf->stainmap_dirty = false;
	}

	g_hash_table_destroy(hash);
}

/**
 * @brief Initialize the stainmap subsystem.
 */
void R_InitStainmaps(void) {

	r_surfs_stained = g_hash_table_new(g_direct_hash, g_direct_equal);

	SDL_Surface *surf = NULL;

	if (Img_LoadImage("particles/stain_burn", &surf)) {
		r_stain_circle_size = surf->w;
		r_stain_circle = Mem_TagMalloc(sizeof(byte) * surf->w * surf->h, MEM_TAG_RENDERER);

		SDL_LockSurface(surf);

		size_t p = 0;
		byte *input = (byte *) surf->pixels;
		byte *output = r_stain_circle;

		for (int32_t i = 0; i < surf->w * surf->h; i++, input += 4, output++) {
			*output = *(input + 3);
		}

		SDL_UnlockSurface(surf);
		SDL_FreeSurface(surf);
	}
}

/**
 * @brief Shutdown the stainmap subsystem.
 */
void R_ShutdownStainmaps(void) {

	g_hash_table_destroy(r_surfs_stained);
}
