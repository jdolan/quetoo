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

static cg_light_t cg_lights[MAX_LIGHTS];

/**
 * @brief
 */
void Cg_AddLight(const cg_light_t *l) {

	if (!cg_add_lights->value) {
		return;
	}

	size_t i;
	for (i = 0; i < lengthof(cg_lights); i++)
		if (cg_lights[i].radius == 0.0) {
			break;
		}

	if (i == lengthof(cg_lights)) {
		Cg_Debug("MAX_LIGHTS\n");
		return;
	}

	cg_light_t *out = cg_lights + i;

	*out = *l;

	assert(out->decay >= 0.f);
	assert(out->intensity >= 0.f);

	if (out->type == LIGHT_INVALID) {
		out->type = LIGHT_DYNAMIC;
	}

	if (out->intensity == 0.0) {
		out->intensity = LIGHT_INTENSITY;
	}

	out->time = cgi.client->unclamped_time;
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
			.atten = LIGHT_ATTEN_INVERSE_SQUARE,
			.origin = l->origin,
			.radius = l->radius,
			.size = 0.f,
			.color = l->color,
			.intensity = l->intensity,
			.normal = Vec3_Zero(),
			.theta = 0.f,
			.bounds = Box3_FromCenterRadius(l->origin, l->radius),
		};

		if (l->decay) {
			assert(out.intensity >= 0.f);
			out.intensity *= (expiration - cgi.client->unclamped_time) / (float) (l->decay);
		}

		cgi.AddLight(cgi.view, &out);
	}

	// Add world lights as shadow casters

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
	const r_bsp_light_t *b = bsp->lights;
	for (int32_t i = 0; i < bsp->num_lights; i++, b++) {

		if (b->type == LIGHT_PATCH && b->radius + b->size > 96.f) {
			cgi.AddLight(cgi.view, &(const r_light_t) {
				.type = b->type,
				.atten = b->atten,
				.origin = b->origin,
				.radius = b->radius,
				.size = b->size,
				.color = b->color,
				.intensity = b->intensity,
				.normal = b->normal,
				.theta = b->theta,
				.bounds = b->bounds,
			});
		}
	}

	// Break the world into a grid, and add an ambient shadow caster per cell

	const float size = 1024.f;

	const r_bsp_lightgrid_t *lg = bsp->lightgrid;
	for (int32_t x = lg->bounds.mins.x; x < lg->bounds.maxs.x; x += size) {
		for (int32_t y = lg->bounds.mins.y; y < lg->bounds.maxs.y; y += size) {
			for (int32_t z = lg->bounds.mins.z; z < lg->bounds.maxs.z; z += size) {

				const box3_t bounds = Box3(Vec3(x, y, z), Vec3(x + size, y + size, z + size));
				const vec3_t origin = Vec3_Fmaf(Box3_Center(bounds), Box3_Extents(bounds).z, Vec3_Up());

				cgi.AddLight(cgi.view, &(r_light_t) {
					.type = LIGHT_AMBIENT,
					.origin = Vec3(origin.x, origin.z, origin.y),
					.bounds = bounds
				});
			}
		}
	}
}

/**
 * @brief 
 */
void Cg_InitLights(void) {

	memset(cg_lights, 0, sizeof(cg_lights));
}
