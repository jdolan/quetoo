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
		if (cg_lights[i].radius == 0.f) {
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

	if (out->intensity == 0.f) {
		out->intensity = 1.f;
	}

	out->time = cgi.client->unclamped_time;
}

/**
 * @brief
 */
static void Cg_AddBspLights(void) {

	const r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
	for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

		switch (l->type) {
			case LIGHT_INVALID:
			case LIGHT_AMBIENT:
			case LIGHT_SUN:
			case LIGHT_PATCH:
				continue;
			default:
				break;
		}

		if (l->shadow == 0.f || Box3_Radius(l->bounds) < 64.f) {
			continue;
		}

		cgi.AddLight(cgi.view, &(const r_light_t) {
			.type = l->type,
			.atten = l->atten,
			.origin = l->origin,
			.color = l->color,
			.normal = l->normal,
			.radius = l->radius,
			.size = l->size,
			.intensity = l->intensity,
			.shadow = l->shadow,
			.cone = l->cone,
			.falloff = l->falloff,
			.bounds = l->bounds,
		});
	}
}

/**
 * @brief
 */
void Cg_AddLights(void) {

	if (!cg_add_lights->value) {
		return;
	}

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
			.color = l->color,
			.normal = Vec3_Zero(),
			.radius = l->radius,
			.size = 0.f,
			.intensity = l->intensity,
			.shadow = MATERIAL_LIGHT_SHADOW,
			.bounds = Box3_FromCenterRadius(l->origin, l->radius),
			.source = l->source,
		};

		if (l->decay) {
			assert(out.intensity >= 0.f);
			out.intensity *= (expiration - cgi.client->unclamped_time) / (float) (l->decay);
		}

		cgi.AddLight(cgi.view, &out);
	}

	Cg_AddBspLights();
}

/**
 * @brief 
 */
void Cg_InitLights(void) {

	memset(cg_lights, 0, sizeof(cg_lights));
}
