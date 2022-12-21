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

	// Add world lights as shadow casters

	const r_bsp_model_t *bsp = cgi.WorldModel()->bsp;
	const r_bsp_light_t *b = bsp->lights;
	for (int32_t i = 0; i < bsp->num_lights; i++, b++) {

		if (b->type == LIGHT_PATCH && b->origin.w > 96.f) {
			cgi.AddLight(cgi.view, &(const r_light_t) {
				.type = b->type,
				.origin = Vec4_XYZ(b->origin),
				.radius = b->origin.w,
				.color = Vec4_XYZ(b->color),
				.intensity = b->color.w,
				.normal = Vec4_XYZ(b->normal),
				.dist = b->normal.w,
				.bounds = b->bounds,
			});
		}
	}

	// Break the world into a grid, and add an ambient shadow caster per cell

	const float size = 512.f;

	const r_bsp_lightgrid_t *lg = bsp->lightgrid;
	for (int32_t x = lg->bounds.mins.x; x < lg->bounds.maxs.x; x += size) {
		for (int32_t y = lg->bounds.mins.y; y < lg->bounds.maxs.y; y += size) {
			for (int32_t z = lg->bounds.mins.z; z < lg->bounds.maxs.z; z += size) {

				const box3_t bounds = Box3(Vec3(x, y, z), Vec3(x + size, y + size, z + size));
				const vec3_t origin = Vec3_Fmaf(Box3_Center(bounds), Box3_Size(bounds).z * .5f, Vec3_Up());

				cgi.AddLight(cgi.view, &(r_light_t) {
					.type = LIGHT_AMBIENT,
					.origin = Vec3(origin.x, origin.z, origin.y),
					.radius = Vec3_Distance(origin, bounds.mins),
					.color = Vec3_One(),
					.intensity = 1.f,
					.normal = Vec3_Down(),
					.dist = -origin.z,
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
