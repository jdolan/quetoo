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

#include "cg_local.h"

/**
 * @brief The flare type.
 */
typedef struct cg_flare_s {
	/**
	 * @brief The flare that this flare was merged to.
	 */
	struct cg_flare_s *merged;

	/**
	 * @brief The face this flare is anchored to.
	 */
	const r_bsp_face_t *face;

	/**
	 * @brief The material stage defining this flare.
	 */
	const r_stage_t *stage;

	/**
	 * @brief The sprite input and output instances.
	 */
	r_sprite_t in, out;

	/**
	 * @brief The entity referencing the model containin this flare, if any.
	 */
	const cl_entity_t *entity;

	/**
	 * @brief The flare alpha value, ramped by occlusion traces.
	 */
	float alpha;

	/**
	 * @brief The flare exposure value, calculated every once in a while.
	 */
	float exposure;
} cg_flare_t;

static GPtrArray *cg_flares;
static uint32_t cg_flare_timestamp;

#define FLARE_ALPHA_RAMP 0.01

/**
 * @brief
 */
void Cg_AddFlares(void) {

	if (!cg_add_flares->value) {
		return;
	}

	for (guint i = 0; i < cg_flares->len; i++) {
		cg_flare_t *flare = g_ptr_array_index(cg_flares, i);

		if (flare->merged) {
			continue;;
		}

		mat4_t matrix = Mat4_Identity();
		flare->entity = NULL;

		const r_bsp_inline_model_t *in = flare->face->node->model;

		if (in != cgi.WorldModel()->bsp->inline_models) {

			const cl_entity_t *e = cgi.client->entities;
			for (int32_t j = 0; j < MAX_ENTITIES; j++, e++) {

				if (!e->current.model1) {
					continue;
				}

				if (e->current.model1 == MODEL_CLIENT) {
					continue;
				}

				const r_model_t *mod = cgi.client->models[e->current.model1];

				if (mod && mod->type == MOD_BSP_INLINE) {
					if (in == mod->bsp_inline) {
						matrix = Mat4_FromRotationTranslationScale(e->angles, e->origin, 1.f);
						flare->entity = e;
						break;
					}
				}
			}

			assert(flare->entity);
		}

		if (flare->entity) {
			flare->out.origin = Mat4_Transform(matrix, flare->in.origin);
		}

		r_sprite_t *out = cgi.AddSprite(cgi.view, &flare->out);

		// occluded, so don't bother checking this
		if (!out) {
			continue;
		}

		if ((cgi.client->unclamped_time - cg_flare_timestamp) > 50) {
			const float dist = Cm_DistanceToPlane(cgi.view->origin, flare->face->plane->cm);
			flare->exposure = 0.f;

			if (dist > 0.f) {

				const r_bsp_vertex_t *v = flare->face->vertexes;
				for (int32_t j = 0; j < flare->face->num_vertexes; j++, v++) {

					vec3_t p;
					if (flare->entity) {
						p = Mat4_Transform(matrix, v->position);
					} else {
						p = v->position;
					}

					const cm_trace_t tr = cgi.Trace(cgi.view->origin, p, Box3_Zero(), 0, CONTENTS_SOLID);
					if (tr.fraction > 0.99f) {
						flare->exposure += 1.f / flare->face->num_vertexes;
					}
				}
			}
		}

		float alpha;
		if (flare->exposure > 0.f) {
 			alpha = cgi.client->frame_msec * FLARE_ALPHA_RAMP * flare->exposure;
		} else {
			alpha = cgi.client->frame_msec * FLARE_ALPHA_RAMP * -(1.f - flare->exposure);
		}

		flare->alpha = Clampf(flare->alpha + alpha, 0.f, 1.f);
		if (flare->alpha > 0.f) {

			alpha = Clampf(flare->alpha * cg_add_flares->value, 0.f, 1.f);
			if (alpha > 0.f) {

				const color_t in_color = Color32_Color(flare->in.color);
				const color_t out_color = Color_Scale(in_color, alpha);

				out->color = Color_Color32(out_color);
			} else {
				// "undo" the sprite addition
				cgi.view->num_sprites--;
			}
		} else {
			// "undo" the sprite addition
			cgi.view->num_sprites--;
		}
	}

	if ((cgi.client->unclamped_time - cg_flare_timestamp) > 50) {
		cg_flare_timestamp = cgi.client->unclamped_time;
	}
}

/**
 * @brief Creates a flare from the specified face and stage.
 * @details The flare is
 */
cg_flare_t *Cg_LoadFlare(const r_bsp_face_t *face, const r_stage_t *stage) {

	cg_flare_t *flare = cgi.Malloc(sizeof(*flare), MEM_TAG_CGAME_LEVEL);

	flare->face = face;
	flare->stage = stage;

	box3_t bounds = Box3_Null();

	for (int32_t i = 0; i < face->num_vertexes; i++) {
		bounds = Box3_Append(bounds, face->vertexes[i].position);
	}

	flare->in.origin = Box3_Center(bounds);
	flare->in.origin = Vec3_Fmaf(flare->in.origin, 2.f, face->plane->cm->normal);

	flare->in.size = Box3_Distance(bounds);

	if (stage->cm->flags & STAGE_COLOR) {
		flare->in.color = Color_Color32(stage->cm->color);
	} else {
		flare->in.color = Color_Color32(color_white);
	}

	if (stage->cm->flags & (STAGE_SCALE_S | STAGE_SCALE_T)) {
		flare->in.size *= (stage->cm->scale.s ? stage->cm->scale.s : stage->cm->scale.t);
	}

	flare->in.media = stage->media;
	flare->in.softness = 0.25f;
	flare->in.lighting = 1.f;

	flare->out = flare->in;

	return flare;
}

/**
 * @brief
 */
static int32_t Cg_MergeFlaresVerts(const cg_flare_t *a, const cg_flare_t *b) {

	int32_t count = 0;

	const r_bsp_vertex_t *av = a->face->vertexes;
	const r_bsp_vertex_t *bv = b->face->vertexes;

	for (int32_t i = 0; i < a->face->num_vertexes; i++) {
		for (int32_t j = 0; j < b->face->num_vertexes; j++) {
			if (Vec3_Distance(av[i].position, bv[j].position) < ON_EPSILON) {
				count++;
			}
		}
	}

	if (b->merged) {
		count += Cg_MergeFlaresVerts(a, b->merged);
	}

	return count;
}

/**
 * @brief
 */
static void Cg_MergeFlares(void) {

	for (guint i = 0; i < cg_flares->len; i++) {
		cg_flare_t *a = g_ptr_array_index(cg_flares, i);

		for (guint j = i + 1; j < cg_flares->len; j++) {
			cg_flare_t *b = g_ptr_array_index(cg_flares, j);

			if (b->face->texinfo == a->face->texinfo &&
				b->face->plane == a->face->plane &&
				b->face->plane_side == a->face->plane_side) {

				if (Cg_MergeFlaresVerts(a, b) > 1) {
					Cg_Debug("Merging %s to %s\n", vtos(b->in.origin), vtos(a->in.origin));

					a->merged = b;

					b->in.origin = Box3_Center(Box3_Union(a->face->bounds, b->face->bounds));
					b->in.size += a->in.size;
				}
			}
		}
	}
}

/**
 * @brief
 */
void Cg_LoadFlares(void) {

	cg_flares = g_ptr_array_new();

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;

	const r_bsp_face_t *face = bsp->faces;
	for (int32_t i= 0; i < bsp->num_faces; i++, face++) {

		const r_material_t *material = face->texinfo->material;
		if (material->cm->flags & STAGE_FLARE) {

			const r_stage_t *stage = material->stages;
			while (stage) {
				if (stage->cm->flags & STAGE_FLARE) {
					break;
				}
				stage = stage->next;
			}
			assert(stage);

			if (stage->media == NULL) {
				continue;
			}

			cg_flare_t *flare = Cg_LoadFlare(face, stage);
			g_ptr_array_add(cg_flares, flare);
		}
	}

	Cg_MergeFlares();

	Cg_Debug("Loaded %u flares\n", cg_flares->len);
}

/**
 * @brief
 */
void Cg_FreeFlares(void) {

	if (cg_flares) {
		g_ptr_array_free(cg_flares, 1);
		cg_flares = NULL;
	}
}
