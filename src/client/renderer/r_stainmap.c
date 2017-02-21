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

typedef struct {
	vec2_t position;
	vec2_t texcoord;
	u8vec4_t color;
} r_stainmap_interleave_vertex_t;

static r_buffer_layout_t r_stainmap_buffer_layout[] = {
	{ .attribute = R_ARRAY_POSITION, .type = R_ATTRIB_FLOAT, .count = 2, .size = sizeof(vec2_t) },
	{ .attribute = R_ARRAY_DIFFUSE, .type = R_ATTRIB_FLOAT, .count = 2, .size = sizeof(vec2_t), .offset = 8 },
	{ .attribute = R_ARRAY_COLOR, .type = R_ATTRIB_UNSIGNED_BYTE, .count = 4, .size = sizeof(u8vec4_t), .offset = 16, .normalized = true },
	{ .attribute = -1 }
};

extern cl_client_t cl;

typedef struct {
	const r_bsp_surface_t *surf;
	const r_stain_t *stain;
	vec_t radius;
	vec2_t point;
	color_t color;
} r_stained_surf_t;

typedef struct {
	GArray *surfs_stained;

	GArray *vertex_scratch;
	GArray *index_scratch;
	
	r_buffer_t vertex_buffer;
	r_buffer_t index_buffer;

	r_buffer_t reset_buffer;
} r_stainmap_state_t;

static r_stainmap_state_t r_stainmap_state;

/**
 * @brief Push the stain to the stain list.
 */
static void R_StainSurface(const r_stain_t *stain, const r_bsp_surface_t *surf) {
	// determine if the surface is within range
	const vec_t dist = R_DistanceToSurface(stain->origin, surf);

	if (fabs(dist) > stain->radius) {
		return;
	}

	// project the stain onto the plane, in world space
	vec3_t point;
	VectorMA(stain->origin, -dist, surf->plane->normal, point);

	// determine if the stain is on the front of the surface
	vec3_t dir;
	VectorSubtract(point, stain->origin, dir);

	if (DotProduct(dir, surf->plane->normal) > 0.0) {
		return;
	}

	const r_bsp_texinfo_t *tex = surf->texinfo;

	// resolve the radius of the stain where it impacts the surface
	const vec_t radius = sqrt(stain->radius * stain->radius - dist * dist);

	// transform the radius into lightmap space, accounting for unevenly scaled textures
	const vec_t radius_st = (radius / tex->scale[0]) * r_model_state.world->bsp->lightmap_scale;
		
	// transform the impact point into texture space
	vec2_t point_st = {
		DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
		DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
	};

	// and convert to lightmap space
	point_st[0] *= r_model_state.world->bsp->lightmap_scale;
	point_st[1] *= r_model_state.world->bsp->lightmap_scale;

	point_st[0] -= radius_st / 2.0;
	point_st[1] -= radius_st / 2.0;
	
	const vec_t radius_rounded = ceil(radius_st);

	if ((point_st[0] < 0 && (point_st[0] + radius_rounded) < 0) ||
		(point_st[1] < 0 && (point_st[1] + radius_rounded) < 0) ||
		point_st[0] >= surf->lightmap_size[0] ||
		point_st[1] >= surf->lightmap_size[1]) {
		return;
	}

	r_stainmap_state.surfs_stained = g_array_append_vals(r_stainmap_state.surfs_stained, &(const r_stained_surf_t) {
		.surf = surf,
		.stain = stain,
		.radius = radius_rounded,
		.point = { round(surf->lightmap_s + point_st[0]), round(surf->lightmap_t + point_st[1]) },
		.color = ColorFromRGBA((byte) (stain->color[0] * 255.0), (byte) (stain->color[1] * 255.0), (byte) (stain->color[2] * 255.0), (byte) (stain->color[3] * 255.0))
	}, 1);

	/*byte *buffer = surf->stainmap_buffer;

	// iterate the luxels and stain the ones that are within reach
	for (uint16_t t = 0; t < surf->lightmap_size[1]; t++) {

		for (uint16_t s = 0; s < surf->lightmap_size[0]; s++, buffer += 3) {

			if (s < (point_st_round[0]) || t < (point_st_round[1]) || 
				s >= (point_st_round[0] + radius_st_round) || t >= (point_st_round[1] + radius_st_round)) {
				continue;
			}
			
			const vec_t delta_s = fabs((s - point_st[0]) / radius_st);
			const vec_t delta_t = fabs((t - point_st[1]) / radius_st);

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
	}*/
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

		if (!surf->lightmap) {
			continue;
		}

		if (surf->vis_frame != r_locals.vis_frame) {
			if (!node->model) {
				continue;
			}
		}

		R_StainSurface(stain, surf);
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
 * @brief Sort stains to maintain optimal bindings
 */
static gint R_AddStains_Sort(gconstpointer a, gconstpointer b) {
	const r_stained_surf_t *sa = (const r_stained_surf_t *) a;
	const r_stained_surf_t *sb = (const r_stained_surf_t *) b;

	if (sa->surf->stainmap.fb != sb->surf->stainmap.fb) {
		return Sign(sa->surf->stainmap.fb - sb->surf->stainmap.fb);
	}

	if (sa->stain->image != sb->stain->image) {
		return Sign(sa->stain->image - sb->stain->image);
	}

	return 0;
}

/**
 * @brief
 */
static void R_Stain_ResolveTexcoords(const r_image_t *image, vec4_t out) {

	if (image->type == IT_ATLAS_IMAGE) {
		const r_atlas_image_t *atlas_image = (const r_atlas_image_t *) image;

		Vector4Copy(atlas_image->texcoords, out);
		return;
	}
	
	Vector4Set(out, 0, 0, 1, 1);
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

	if (!r_stainmap_state.surfs_stained->len) {
		return;
	}

	// sort stains for optimal binding
	g_array_sort(r_stainmap_state.surfs_stained, R_AddStains_Sort);

	const SDL_Rect old_viewport = r_view.viewport;
	const r_framebuffer_t *old_framebuffer = r_framebuffer_state.current_framebuffer;
	const r_program_t *old_program = r_state.active_program;

	const GLenum old_blend_src = r_state.blend_src;
	const GLenum old_blend_dest = r_state.blend_dest;
	const GLenum old_blend_enabled = r_state.blend_enabled;

	R_UseProgram(program_stain);
	R_EnableBlend(true);
	R_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	R_EnableColorArray(true);
	R_Color(NULL);
	R_EnableDepthMask(false);

	R_PushMatrix(R_MATRIX_PROJECTION);

	R_BindAttributeInterleaveBuffer(&r_stainmap_state.vertex_buffer, R_ARRAY_MASK_ALL);
	R_BindAttributeBuffer(R_ARRAY_ELEMENTS, &r_stainmap_state.index_buffer);

	GLint vert_offset = 0, elem_offset = 0;
	GLsizei vert_count = 0, elem_count = 0;

	// adjust elements - we can do this all at once since they're constants
	const uint16_t num_elements = (r_stainmap_state.surfs_stained->len * 6);

	if (r_stainmap_state.index_scratch->len < num_elements) {
		const uint16_t start = (r_stainmap_state.index_scratch->len == 0) ? 0 : (g_array_index(r_stainmap_state.index_scratch, uint16_t, r_stainmap_state.index_scratch->len - 1)) + 1;

		for (uint16_t i = (uint16_t) r_stainmap_state.index_scratch->len, s = start; i < num_elements; i += 6, s += 4) {
			r_stainmap_state.index_scratch = g_array_append_vals(r_stainmap_state.index_scratch, (const uint16_t[6]) {
				s + 0, s + 1, s + 2,
				s + 0, s + 2, s + 3
			}, 6);
		}

		// update size of GPU buffers
		R_UploadToBuffer(&r_stainmap_state.index_buffer, sizeof(uint16_t) * num_elements, r_stainmap_state.index_scratch->data);
		R_UploadToBuffer(&r_stainmap_state.vertex_buffer, sizeof(r_stainmap_interleave_vertex_t) * (r_stainmap_state.surfs_stained->len * 4), NULL);
	}

	// adjust vertices
	for (size_t i = 0; i < r_stainmap_state.surfs_stained->len + 1; i++) {
		const r_stained_surf_t *stain = NULL;
		
		if (i < r_stainmap_state.surfs_stained->len) {
			stain = &g_array_index(r_stainmap_state.surfs_stained, r_stained_surf_t, i);
		}

		if (elem_count && (i == r_stainmap_state.surfs_stained->len || r_framebuffer_state.current_framebuffer != stain->surf->stainmap.fb)) {

			R_UploadToSubBuffer(&r_stainmap_state.vertex_buffer, sizeof(r_stainmap_interleave_vertex_t) * vert_offset, sizeof(r_stainmap_interleave_vertex_t) * vert_count, r_stainmap_state.vertex_scratch->data, false);

			R_DrawArrays(GL_TRIANGLES, elem_offset, elem_count);

			elem_offset += elem_count;
			vert_offset += vert_count;
			elem_count = vert_count = 0;

			// reset scratch for next batch
			g_array_set_size(r_stainmap_state.vertex_scratch, 0);
		}

		if (i == r_stainmap_state.surfs_stained->len) {
			break;
		}

		r_image_t *image = r_image_state.null;//stain->stain->image;

		vec4_t texcoords;
		R_Stain_ResolveTexcoords(image, texcoords);

		R_BindFramebuffer(stain->surf->stainmap.fb);

		R_SetViewport(0, 0, stain->surf->stainmap.image->width, stain->surf->stainmap.image->height, true);

		R_SetMatrix(R_MATRIX_PROJECTION, &stain->surf->stainmap.projection);

		R_BindDiffuseTexture(image->texnum);

		r_stainmap_state.vertex_scratch = g_array_append_vals(r_stainmap_state.vertex_scratch, (const r_stainmap_interleave_vertex_t[4]) {
			{ .position = { stain->point[0], (stain->surf->stainmap.image->height - (stain->point[1] + stain->radius)) }, .texcoord = { texcoords[0], texcoords[3] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { stain->point[0] + stain->radius, (stain->surf->stainmap.image->height - (stain->point[1] + stain->radius)) }, .texcoord = { texcoords[2], texcoords[3] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { stain->point[0] + stain->radius, (stain->surf->stainmap.image->height - stain->point[1]) }, .texcoord = { texcoords[2], texcoords[1] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { stain->point[0], (stain->surf->stainmap.image->height - stain->point[1]) }, .texcoord = { texcoords[0], texcoords[1] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } }
		}, 4);

		vert_count += 4;
		elem_count += 6;
	}

	R_EnableBlend(old_blend_enabled);

	R_BlendFunc(old_blend_src, old_blend_dest);

	R_PopMatrix(R_MATRIX_PROJECTION);
	
	R_EnableDepthMask(true);

	R_UseProgram(old_program);

	R_BindFramebuffer(old_framebuffer);

	R_SetViewport(old_viewport.x, old_viewport.y, old_viewport.w, old_viewport.h, true);

	R_UnbindAttributeBuffers();

	r_stainmap_state.surfs_stained = g_array_set_size(r_stainmap_state.surfs_stained, 0);
}

/**
 * @brief Resets the stainmaps that we have loaded.
 */
void R_ResetStainmap(void) {

	GSList *reset = NULL;

	const r_program_t *old_program = r_state.active_program;

	R_BindDiffuseTexture(r_image_state.null->texnum);

	const SDL_Rect old_viewport = r_view.viewport;
	
	R_PushMatrix(R_MATRIX_PROJECTION);

	R_UnbindAttributeBuffers();
	R_BindAttributeInterleaveBuffer(&r_stainmap_state.reset_buffer, R_ARRAY_MASK_ALL);

	const r_framebuffer_t *old_framebuffer = r_framebuffer_state.current_framebuffer;
	
	const GLenum old_blend_enabled = r_state.blend_enabled;

	R_EnableDepthMask(false);

	R_EnableBlend(false);

	R_UseProgram(program_stain);

	const _Bool old_color_enabled = r_state.color_array_enabled;

	R_EnableColorArray(true);

	R_Color(NULL);

	for (uint32_t s = 0; s < r_model_state.world->bsp->num_surfaces; s++) {
		r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + s;

		// skip if we don't have a stainmap or we weren't stained
		if (!surf->stainmap.fb) {
			continue;
		}

		if (g_slist_find(reset, surf->stainmap.fb)) {
			continue;
		}

		reset = g_slist_prepend(reset, surf->stainmap.fb);

		const r_stainmap_interleave_vertex_t stain_fill[4] = {
			{ .position = { 0, 0 }, .texcoord = { 0, 0 }, .color = { 0, 0, 0, 0 } },
			{ .position = { surf->stainmap.image->width, 0 }, .texcoord = { 1, 0 }, .color = { 0, 0, 0, 0 } },
			{ .position = { surf->stainmap.image->width, surf->stainmap.image->height }, .texcoord = { 1, 1 }, .color = { 0, 0, 0, 0 } },
			{ .position = { 0, surf->stainmap.image->height }, .texcoord = { 0, 1 }, .color = { 0, 0, 0, 0 } },
		};

		R_UploadToSubBuffer(&r_stainmap_state.reset_buffer, 0, sizeof(stain_fill), stain_fill, false);

		R_BindFramebuffer(surf->stainmap.fb);

		R_SetViewport(0, 0, surf->stainmap.image->width, surf->stainmap.image->height, true);

		R_SetMatrix(R_MATRIX_PROJECTION, &surf->stainmap.projection);

		R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	R_EnableColorArray(old_color_enabled);

	R_EnableBlend(old_blend_enabled);
	
	R_EnableDepthMask(true);

	R_PopMatrix(R_MATRIX_PROJECTION);

	R_BindFramebuffer(old_framebuffer);

	R_SetViewport(old_viewport.x, old_viewport.y, old_viewport.w, old_viewport.h, true);

	R_UseProgram(old_program);

	r_stainmap_state.surfs_stained = g_array_set_size(r_stainmap_state.surfs_stained, 0);
}

/**
 * @brief Initialize the stainmap subsystem.
 */
void R_InitStainmaps(void) {
	
	r_stainmap_state.surfs_stained = g_array_sized_new(false, false, sizeof(r_stained_surf_t), MAX_STAINS);
	r_stainmap_state.vertex_scratch = g_array_sized_new(false, false, sizeof(r_stainmap_interleave_vertex_t), MAX_STAINS * 4);
	r_stainmap_state.index_scratch = g_array_sized_new(false, false, sizeof(uint16_t), MAX_STAINS * 6);

	R_CreateInterleaveBuffer(&r_stainmap_state.reset_buffer, sizeof(r_stainmap_interleave_vertex_t), r_stainmap_buffer_layout, GL_STATIC_DRAW, sizeof(r_stainmap_interleave_vertex_t) * 4, NULL);

	R_CreateInterleaveBuffer(&r_stainmap_state.vertex_buffer, sizeof(r_stainmap_interleave_vertex_t), r_stainmap_buffer_layout, GL_DYNAMIC_DRAW, sizeof(r_stainmap_interleave_vertex_t) * MAX_STAINS * 4, NULL);
	R_CreateElementBuffer(&r_stainmap_state.index_buffer, R_ATTRIB_UNSIGNED_SHORT, GL_DYNAMIC_DRAW, sizeof(uint16_t) * MAX_STAINS * 6, NULL);
}

/**
 * @brief Shutdown the stainmap subsystem.
 */
void R_ShutdownStainmaps(void) {

	g_array_free(r_stainmap_state.surfs_stained, true);
	g_array_free(r_stainmap_state.vertex_scratch, true);
	g_array_free(r_stainmap_state.index_scratch, true);

	R_DestroyBuffer(&r_stainmap_state.reset_buffer);
	R_DestroyBuffer(&r_stainmap_state.vertex_buffer);
	R_DestroyBuffer(&r_stainmap_state.index_buffer);
}
