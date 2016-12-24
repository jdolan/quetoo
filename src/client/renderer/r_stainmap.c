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

// a list of stained surfaces, for uploading
static GHashTable *r_surfs_stained;

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

		// THIS IS NOT RIGHT, BUT SOMETHIN LIKE THIS 
		const vec_t step_s = 16.0;//tex->scale[0] * r_model_state.world->bsp->lightmaps->scale;
		const vec_t step_t = 16.0;//tex->scale[1] * r_model_state.world->bsp->lightmaps->scale;

		// check if the plane is even touched by the stain to begin with.
		// make the origin parallel to the plane normal (stain * 0,0,1 for up faces for instance)
		const vec_t projected = DotProduct(stain->origin, surf->plane->normal);

		// subtract this projected distance by the surf's distance into the world, which
		// gives us how far (in world units) the stain point is from the plane on the planes'
		// axis. For most stains, this will be a very tiny value for planes that end up passing
		// this check (like 0.03 or so). 
		vec_t face_dist = projected - surf->plane->dist;

		// subtract the radius from the distance to see if we're close enough to potentially
		// stain this plane.
		const vec_t intensity = stain->radius - face_dist;

		if (intensity < 0.0) {
			continue;
		}

		// square the intensity, so we avoid a sqrt in distance check
		const vec_t intensity_squared = intensity * intensity;

		// AT THIS POINT, plane is within distance (on the plane's axis) to be stained.
		// it still might be pretty far away though, so we gotta get technical.

		// if we're not a back plane, we have to project "backwards" since our
		// backwards is technically forwards. Since back planes are already pointing
		// backwards we don't have to invert for those.
		if (!(surf->flags & R_SURF_PLANE_BACK)) {
			face_dist = -face_dist;
		}

		// project stain->origin onto the plane by multiplying it by face_dist * normal - this
		// gives us "point", which is coplanar to the plane (lays exactly onto the plane)
		vec3_t point;
		VectorMA(stain->origin, face_dist, surf->plane->normal, point);

		// convert the world space coordinate of "point" into texture ST coordinates.
		// I wish I could say I understood how this worked exactly... here goes an attempt!
		//
		// The dotproduct of the point and the s/t directions (vecs[0] and vecs[1]) + dist (vecs[n][3])
		// will give us the texture coordinate, -in texels-, of where this point is on the
		// texture itself. If we manage to stain at the exact 0,0 point on a texture (like
		// top-left of a crate) then, theoretically, this would give us the exact same value as
		// st_mins[n]. st_mins[n] holds the lowest value (what one would think of as the "top-left" texcoord)
		// of the texture coordinates. By subtracting the output of our projection by this, we get the offset to
		// our texture-space stain position from the "0,0 point" of the texcoords. 
		//
		// Note that, again, this is in diffuse texel space! The crates on edge will give us
		// values between -256 and 256 here (depending on which side it is). If we fire at the center of
		// the crate, we'll get values like 128,128.
		const vec2_t point_st = {
			DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
			DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
		};

		// store a copy of the LIGHTMAP extents. The aspect ratio of these match the ones in tex->vecs from what I can tell.
		// (one of the squished boxes in the mega area on edge had approx (3.2, 0, 0) (0, 0, 4.0)
		// for the vecs[0] and [1] respectively, and st_extents of 18,23 - (4.0 - 3.2) = 0.8, (23 * 0.8) = (18.4), which 
		// seems to match up.
		const r_pixel_t smax = surf->st_extents[0];
		const r_pixel_t tmax = surf->st_extents[1];

		byte *buffer = surf->stainmap_buffer;

		// this is the texel we're checking to see should be stained.
		// this is in diffuse texels, not lightmap texels.
		vec2_t texel_check = { 0.0, 0.0 };
		
		const vec_t slen = 4.0 / VectorLengthSquared(tex->vecs[0]);
		const vec_t tlen = 4.0 / VectorLengthSquared(tex->vecs[1]);

		// the magical loop. So, an interesting thing to note here is that this loop
		// operates on both lightmap and diffuse texture coordinates. It loops through
		// every pixel of the lightmap coordinates, but only for number purposes - the
		// texcoord pixel s/t is never used. The only thing that -is- used is dt and ds, which
		// are texel offsets from the diffuse texture. The silly thing is that this loop
		// assumes that each diffuse texel is 1/16th of a lightmap texel on both axis (which may not always
		// be true). Even sillier is that "1" texel is -not- 1 world-space unit, which makes the distance check
		// weird because the intensity is in world space, not texel space.
		for (uint16_t t = 0; t < tmax; t++, texel_check[1] += step_t) {

			texel_check[0] = 0.0;

			for (uint16_t s = 0; s < smax; s++, texel_check[0] += step_s, buffer += 3) {

				// subtract our st stain point from the texel we're currently checking.
				// this gives us a vector to the stain position.
				const vec2_t sample_diff = { point_st[0] - texel_check[0], point_st[1] - texel_check[1] };

				// grab the distance of said vector above. No sqrt, since the intensity is squared.
				const vec_t sample_dist = (sample_diff[0] * sample_diff[0]) * slen + (sample_diff[1] * sample_diff[1]) * tlen;

				// we're outside of the stain circle
				if (sample_dist >= intensity_squared) {
					continue;
				}

				// we stained! color bits are straightforward.
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

		// mark it to be uploaded at the end of the frame
		g_hash_table_add(r_surfs_stained, surf);
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

	const r_bsp_surface_t *surf = (r_bsp_surface_t *) value;

	// bind as diffuse, since it's the only texunit that stays bound so that the
	// GL_TEXTURE_2D resolves properly
	R_BindDiffuseTexture(surf->stainmap->texnum);
	glTexSubImage2D(GL_TEXTURE_2D, 0, surf->lightmap_s, surf->lightmap_t, surf->st_extents[0], surf->st_extents[1], GL_RGB,
					GL_UNSIGNED_BYTE, surf->stainmap_buffer);

	R_GetError("stain upload");
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

	// upload the stain maps that were modified
	g_hash_table_foreach(r_surfs_stained, R_AddStains_UploadSurfaces, NULL);

	// clear for next iteration
	g_hash_table_remove_all(r_surfs_stained);
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

/**
 * @brief Initialize the stainmap subsystem.
 */
void R_InitStainmaps(void) {

	r_surfs_stained = g_hash_table_new(g_direct_hash, g_direct_equal);
}

/**
 * @brief Shutdown the stainmap subsystem.
 */
void R_ShutdownStainmaps(void) {

	g_hash_table_destroy(r_surfs_stained);
}
