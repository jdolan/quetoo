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
#include "r_gl.h"

typedef struct {
	vec2_t position;
	vec2_t texcoord;
	u8vec4_t color;
} r_stainmap_interleave_vertex_t;

static r_buffer_layout_t r_stainmap_buffer_layout[] = {
	{ .attribute = R_ATTRIB_POSITION, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_DIFFUSE, .type = R_TYPE_FLOAT, .count = 2 },
	{ .attribute = R_ATTRIB_COLOR, .type = R_TYPE_UNSIGNED_BYTE, .count = 4, .normalized = true },
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

	vec_t expire_seconds, expire_ticks;
	uint32_t expire_check;
} r_stainmap_state_t;

static cvar_t *r_stainmap_expire_time;
static r_stainmap_state_t r_stainmap_state;

/**
 * @brief Push the stain to the stain list.
 */
static _Bool R_StainSurface(const r_stain_t *stain, const r_bsp_surface_t *surf) {
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
		return false;
	}

	r_stainmap_state.surfs_stained = g_array_append_vals(r_stainmap_state.surfs_stained, &(const r_stained_surf_t) {
		.surf = surf,
		.stain = stain,
		.radius = radius_rounded,
		.point = { round(surf->lightmap_s + point_st[0]), round(surf->lightmap_t + point_st[1]) },
		.color = ColorFromRGBA((byte) (stain->color[0] * 255.0),
							   (byte) (stain->color[1] * 255.0),
							   (byte) (stain->color[2] * 255.0),
							   (byte) (stain->color[3] * 255.0))
	}, 1);

	return true;
}

/**
 * @brief
 */
static _Bool R_StainNode(const r_stain_t *stain, const r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return false;
	}

	if (node->vis_frame != r_locals.vis_frame) {
		if (!node->model) {
			return false;
		}
	}

	const vec_t dist = Cm_DistanceToPlane(stain->origin, node->plane);

	if (dist > stain->radius * 2.0) { // front only
		return R_StainNode(stain, node->children[0]);
	}

	if (dist < -stain->radius * 2.0) { // back only
		return R_StainNode(stain, node->children[1]);
	}

	r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;
	_Bool stained = false;

	for (uint32_t i = 0; i < node->num_surfaces; i++, surf++) {

		if (surf->texinfo->flags & (SURF_SKY | SURF_WARP)) {
			continue;
		}

		if (!surf->stainmap.fb) {
			continue;
		}

		if (surf->vis_frame != r_locals.vis_frame) {
			if (!node->model) {
				continue;
			}
		}

		if (R_StainSurface(stain, surf)) {
			stained = true;
		}
	}

	// recurse down both sides
	const _Bool left = R_StainNode(stain, node->children[0]);
	const _Bool right = R_StainNode(stain, node->children[1]);

	return stained || left || right;
}

/**
 * @brief Add a stain to the map.
 */
void R_AddStain(const r_stain_t *s) {

	if (!r_stainmaps->value) {
		return;
	}

	if (r_view.num_stains == MAX_STAINS) {
		Com_Debug(DEBUG_RENDERER, "MAX_STAINS reached\n");
		return;
	}

	r_view.stains[r_view.num_stains] = *s;
	r_view.stains[r_view.num_stains].radius *= r_stainmaps->value;
	r_view.num_stains++;
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
 * @brief
 */
static void R_ExpireStains(const byte alpha) {
	GSList *reset = NULL;

	const r_program_t *old_program = r_state.active_program;

	R_BindDiffuseTexture(r_image_state.null->texnum);

	const SDL_Rect old_viewport = r_state.current_viewport;

	R_PushMatrix(R_MATRIX_PROJECTION);

	R_UnbindAttributeBuffers();
	R_BindAttributeInterleaveBuffer(&r_stainmap_state.reset_buffer, R_ATTRIB_MASK_ALL);

	const r_framebuffer_t *old_framebuffer = r_framebuffer_state.current_framebuffer;

	const GLenum old_blend_enabled = r_state.blend_enabled;

	R_EnableDepthMask(false);

	R_UseProgram(program_stain);

	const _Bool old_color_enabled = r_state.color_array_enabled;
	const GLenum old_blend_src = r_state.blend_src;
	const GLenum old_blend_dest = r_state.blend_dest;

	R_EnableColorArray(true);

	R_Color(NULL);

	glBlendEquation(GL_FUNC_REVERSE_SUBTRACT);

	R_EnableBlend(true);

	R_BlendFunc(GL_ONE, GL_ONE);

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
			{ .position = { 0, 0 }, .texcoord = { 0, 0 }, .color = { alpha, alpha, alpha, alpha } },
			{ .position = { surf->stainmap.image->width, 0 }, .texcoord = { 1, 0 }, .color = { alpha, alpha, alpha, alpha } },
			{ .position = { surf->stainmap.image->width, surf->stainmap.image->height }, .texcoord = { 1, 1 }, .color = { alpha, alpha, alpha, alpha } },
			{ .position = { 0, surf->stainmap.image->height }, .texcoord = { 0, 1 }, .color = { alpha, alpha, alpha, alpha } },
		};

		R_UploadToSubBuffer(&r_stainmap_state.reset_buffer, 0, sizeof(stain_fill), stain_fill, false);

		R_BindFramebuffer(surf->stainmap.fb);

		R_SetViewport(0, 0, surf->stainmap.image->width, surf->stainmap.image->height, true);

		R_SetMatrix(R_MATRIX_PROJECTION, &surf->stainmap.projection);

		R_DrawArrays(GL_TRIANGLE_FAN, 0, 4);
	}

	R_EnableColorArray(old_color_enabled);

	R_EnableBlend(old_blend_enabled);

	R_BlendFunc(old_blend_src, old_blend_dest);

	glBlendEquation(GL_FUNC_ADD);

	R_EnableDepthMask(true);

	R_PopMatrix(R_MATRIX_PROJECTION);

	R_BindFramebuffer(old_framebuffer);

	R_SetViewport(old_viewport.x, old_viewport.y, old_viewport.w, old_viewport.h, true);

	R_UseProgram(old_program);

	r_stainmap_state.surfs_stained = g_array_set_size(r_stainmap_state.surfs_stained, 0);
}

/**
 * @brief Adds new stains from the view each frame.
 */
void R_AddStains(void) {

	if (!r_model_state.world) {
		return;
	}

	if (r_stainmap_expire_time->integer) {

		if (r_stainmap_expire_time->modified) {
			r_stainmap_expire_time->modified = false;
			r_stainmap_state.expire_seconds = (255.0 / r_stainmap_expire_time->value);
			r_stainmap_state.expire_check = r_view.ticks;
		} else {
			uint32_t diff = r_view.ticks - r_stainmap_state.expire_check;
			r_stainmap_state.expire_ticks += r_stainmap_state.expire_seconds * diff;

			if (r_stainmap_state.expire_ticks > 1) {
				const uint8_t val = (uint8_t) r_stainmap_state.expire_ticks;

				R_ExpireStains(val);

				r_stainmap_state.expire_ticks -= val;
			}

			r_stainmap_state.expire_check = r_view.ticks;
		}
	}

	const uint16_t num_stains = r_view.num_stains;

	if (!num_stains) {
		return;
	}

	const r_stain_t *s = r_view.stains;

	for (int32_t i = 0; i < num_stains; i++, s++) {

		R_StainNode(s, r_model_state.world->bsp->nodes);

		for (uint16_t e = 0; e < cl.frame.num_entities && r_view.num_stains < MAX_STAINS; e++) {

			const uint32_t snum = (cl.frame.entity_state + e) & ENTITY_STATE_MASK;
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

			// add a "new" stain for the transformed position
			r_view.stains[r_view.num_stains] = *s;
			Matrix4x4_Transform(&ent->inverse_matrix, s->origin, r_view.stains[r_view.num_stains].origin);

			if (R_StainNode(&r_view.stains[r_view.num_stains], node)) {
				r_view.num_stains++;

				if (r_view.num_stains == MAX_STAINS) {
					Com_Debug(DEBUG_RENDERER, "MAX_STAINS reached\n");
					break;
				}
			}
		}
	}

	if (!r_stainmap_state.surfs_stained->len) {
		return;
	}

	// sort stains for optimal binding
	g_array_sort(r_stainmap_state.surfs_stained, R_AddStains_Sort);

	const SDL_Rect old_viewport = r_state.current_viewport;
	const r_framebuffer_t *old_framebuffer = r_framebuffer_state.current_framebuffer;
	const r_program_t *old_program = r_state.active_program;

	const GLenum old_blend_src = r_state.blend_src;
	const GLenum old_blend_dest = r_state.blend_dest;
	const GLenum old_blend_enabled = r_state.blend_enabled;

	const _Bool color_array_enabled = r_state.color_array_enabled;

	R_UseProgram(program_stain);
	R_EnableBlend(true);
	R_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	R_EnableColorArray(true);
	R_Color(NULL);
	R_EnableDepthMask(false);

	R_PushMatrix(R_MATRIX_PROJECTION);

	R_BindAttributeInterleaveBuffer(&r_stainmap_state.vertex_buffer, R_ATTRIB_MASK_ALL);
	R_BindAttributeBuffer(R_ATTRIB_ELEMENTS, &r_stainmap_state.index_buffer);

	GLint vert_offset = 0, elem_offset = 0;

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

	const float angle = Randomf() * 360; // for random rotations

	// adjust vertices
	for (size_t i = 0; i < r_stainmap_state.surfs_stained->len; i++) {
		const r_stained_surf_t *stain = &g_array_index(r_stainmap_state.surfs_stained, r_stained_surf_t, i);
		const r_image_t *image = stain->stain->image;

		vec4_t texcoords;
		R_Stain_ResolveTexcoords(image, texcoords);

		R_BindFramebuffer(stain->surf->stainmap.fb);

		R_SetViewport(0, 0, stain->surf->stainmap.image->width, stain->surf->stainmap.image->height, true);

		R_SetMatrix(R_MATRIX_PROJECTION, &stain->surf->stainmap.projection);

		R_BindDiffuseTexture(image->texnum);

		vec4_t position = { stain->point[0], stain->point[1], stain->point[0] + stain->radius, stain->point[1] + stain->radius };

		// flip Y, because apparently we need this :)
		position[1] = stain->surf->stainmap.image->height - position[1];
		position[3] = stain->surf->stainmap.image->height - position[3];

		matrix4x4_t m = matrix4x4_identity;
		vec2_t center = {
			(position[0] + position[2]) * 0.5,
			(position[1] + position[3]) * 0.5
		};
		Matrix4x4_ConcatTranslate(&m, center[0], center[1], 0.0);
		Matrix4x4_ConcatRotate(&m, angle, 0.0, 0.0, 1.0);
		Matrix4x4_ConcatTranslate(&m, -center[0], -center[1], 0.0);

		vec2_t vertexes[4] = {
			{ position[0], position[3] },
			{ position[2], position[3] },
			{ position[2], position[1] },
			{ position[0], position[1] },
		};

		for (int32_t p = 0; p < 4; p++) {
			Matrix4x4_Transform2(&m, vertexes[p], vertexes[p]);
		}

		r_stainmap_state.vertex_scratch = g_array_append_vals(r_stainmap_state.vertex_scratch, (const r_stainmap_interleave_vertex_t[4]) {
			{ .position = { vertexes[0][0], vertexes[0][1] }, .texcoord = { texcoords[0], texcoords[3] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { vertexes[1][0], vertexes[1][1] }, .texcoord = { texcoords[2], texcoords[3] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { vertexes[2][0], vertexes[2][1] }, .texcoord = { texcoords[2], texcoords[1] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } },
			{ .position = { vertexes[3][0], vertexes[3][1] }, .texcoord = { texcoords[0], texcoords[1] }, .color = { stain->color.r, stain->color.g, stain->color.b, stain->color.a } }
		}, 4);

		R_EnableScissor(&(const SDL_Rect) {
			stain->surf->lightmap_s,
			stain->surf->lightmap_t,
			stain->surf->lightmap_size[0],
			stain->surf->lightmap_size[1]
		});

		R_UploadToSubBuffer(&r_stainmap_state.vertex_buffer, sizeof(r_stainmap_interleave_vertex_t) * vert_offset, sizeof(r_stainmap_interleave_vertex_t) * 4, r_stainmap_state.vertex_scratch->data, true);

		R_DrawArrays(GL_TRIANGLES, elem_offset, 6);

		elem_offset += 6;
		vert_offset += 4;
	}

	// reset scratch for next batch
	g_array_set_size(r_stainmap_state.vertex_scratch, 0);

	R_EnableBlend(old_blend_enabled);

	R_BlendFunc(old_blend_src, old_blend_dest);

	R_EnableColorArray(color_array_enabled);

	R_PopMatrix(R_MATRIX_PROJECTION);

	R_EnableDepthMask(true);

	R_UseProgram(old_program);

	R_BindFramebuffer(old_framebuffer);

	R_SetViewport(old_viewport.x, old_viewport.y, old_viewport.w, old_viewport.h, true);

	R_EnableScissor(NULL);

	R_UnbindAttributeBuffers();

	r_stainmap_state.surfs_stained = g_array_set_size(r_stainmap_state.surfs_stained, 0);
}

/**
 * @brief Resets the stainmaps that we have loaded.
 */
void R_ResetStainmap(void) {

	R_ExpireStains(255);
}

/**
 * @brief Initialize the stainmap subsystem.
 */
void R_InitStainmaps(void) {

	r_stainmap_state.surfs_stained = g_array_sized_new(false, false, sizeof(r_stained_surf_t), MAX_STAINS);
	r_stainmap_state.vertex_scratch = g_array_sized_new(false, false, sizeof(r_stainmap_interleave_vertex_t), MAX_STAINS * 4);
	r_stainmap_state.index_scratch = g_array_sized_new(false, false, sizeof(uint16_t), MAX_STAINS * 6);

	R_CreateInterleaveBuffer(&r_stainmap_state.reset_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_stainmap_interleave_vertex_t),
		.layout = r_stainmap_buffer_layout,
		.hint = GL_STATIC_DRAW,
		.size = sizeof(r_stainmap_interleave_vertex_t) * 4
	});

	R_CreateInterleaveBuffer(&r_stainmap_state.vertex_buffer, &(const r_create_interleave_t) {
		.struct_size = sizeof(r_stainmap_interleave_vertex_t),
		.layout = r_stainmap_buffer_layout,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(r_stainmap_interleave_vertex_t) * MAX_STAINS * 4
	});

	R_CreateElementBuffer(&r_stainmap_state.index_buffer, &(const r_create_element_t) {
		.type = R_TYPE_UNSIGNED_SHORT,
		.hint = GL_DYNAMIC_DRAW,
		.size = sizeof(uint16_t) * MAX_STAINS * 6
	});

	r_stainmap_expire_time = Cvar_Add("r_stainmap_expire_time", "8000", CVAR_ARCHIVE, "The amount of time, in milliseconds, stains should take to fully disappear.");
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
