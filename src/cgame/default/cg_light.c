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

#define LIGHT_INTENSITY .125f

static cg_light_t cg_lights[MAX_ENTITIES];

/**
 * @brief
 */
void Cg_AddLight(const cg_light_t *l) {
	size_t i;

	if (!cg_add_lights->value) {
		return;
	}

	for (i = 0; i < lengthof(cg_lights); i++)
		if (cg_lights[i].radius == 0.0) {
			break;
		}

	if (i == lengthof(cg_lights)) {
		Cg_Debug("MAX_ENTITIES\n");
		return;
	}

	cg_lights[i] = *l;

	if (cg_lights[i].type == LIGHT_INVALID) {
		cg_lights[i].type = LIGHT_DYNAMIC;
	}

	assert(cg_lights[i].decay >= 0.f);
	assert(cg_lights[i].intensity >= 0.f);
	
	if (cg_lights[i].intensity == 0.0) {
		cg_lights[i].intensity = LIGHT_INTENSITY;
	}

	cg_lights[i].time = cgi.client->unclamped_time;
}

/**
 * @brief
 */
void Cg_AddLights(void) {

	cg_light_t *l = cg_lights;
	for (size_t i = 0; i < lengthof(cg_lights); i++, l++) {

		const uint32_t expiration = l->time + l->decay;
		if (expiration < cgi.client->unclamped_time) {
			l->radius = 0.0;
			continue;
		}

		r_light_t out = {
			.type = l->type,
			.origin = l->origin,
			.radius = l->radius,
			.color = l->color,
			.intensity = l->intensity,
			.bounds = Box3_FromCenterRadius(l->origin, l->radius),
		};

		if (l->decay) {
			assert(out.intensity >= 0.f);
			out.intensity *= (expiration - cgi.client->unclamped_time) / (float) (l->decay);
		}

		cgi.AddLight(cgi.view, &out);
	}

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
	const r_bsp_light_t *b = bsp->lights;
	for (int32_t i = 0; i < bsp->num_lights; i++, b++) {

		if (b->type == LIGHT_PATCH && b->origin.w > 96.f) {

			const r_entity_t *e = cgi.view->entities;
			for (int32_t j = 0; j < cgi.view->num_entities; j++) {

				if (IS_MESH_MODEL(e->model) && Box3_Intersects(b->bounds, e->abs_model_bounds)) {
					cgi.AddLight(cgi.view, &(const r_light_t) {
						.type = b->type,
						.origin = Vec4_XYZ(b->origin),
						.radius = b->origin.w,
						.color = Vec4_XYZ(b->color),
						.intensity = b->color.w,
						.bounds = b->bounds,
					});
					break;
				}
			}
		}
	}

	const r_entity_t *e = cgi.view->entities;
	for (int32_t i = 0; i < cgi.view->num_entities; i++, e++) {

		if (IS_MESH_MODEL(e->model) && !e->parent) {
			cgi.AddLight(cgi.view, &(r_light_t) {
				.type = LIGHT_SPOT,
				.origin = Vec3_Fmaf(e->origin, 64.f, Vec3_Up()),
				.radius = 96.f,
				.color = Vec3_One(),
				.intensity = 1.f,
				.bounds = Box3_Expand3(e->abs_model_bounds, Vec3(0.f, 0.f, 64.f))
			});
		}
	}
}

/**
 * @brief 
 */
void Cg_InitLights(void) {

	memset(cg_lights, 0, sizeof(cg_lights));
}
