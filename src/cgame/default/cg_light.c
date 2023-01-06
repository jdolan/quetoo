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

#define LIGHT_INTENSITY 1.f

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
static void Cg_AddBspLights(void) {

	const r_bsp_light_t *l = cgi.WorldModel()->bsp->lights;
	for (int32_t i = 0; i < cgi.WorldModel()->bsp->num_lights; i++, l++) {

		switch (l->type) {
			case LIGHT_INVALID:
			case LIGHT_AMBIENT:
			case LIGHT_SUN:
			case LIGHT_INDIRECT:
				continue;
			default:
				break;
		}

		const float dist = Vec3_Distance(cgi.view->origin, l->origin) - Box3_Radius(l->bounds);

		if (dist < 1024.f && Box3_Radius(l->bounds) > 64.f) {
			cgi.AddLight(cgi.view, &(const r_light_t) {
				.type = l->type,
				.atten = l->atten,
				.origin = l->origin,
				.radius = l->radius,
				.size = l->size,
				.color = l->color,
				.intensity = l->intensity,
				.normal = l->normal,
				.theta = l->theta,
				.bounds = l->bounds,
			});
		}
	}
}

/**
 * @brief
 */
static void Cg_AddAmbientLights(void) {

	const r_entity_t *e = cgi.view->entities;
	for (int32_t i = 0; i < cgi.view->num_entities; i++, e++) {

		if (!e->model) {
			continue;
		}

		if (e->parent) {
			continue;
		}

		if (e->effects & EF_NO_SHADOW) {
			continue;
		}

		int32_t j;

		// FIXME: A better way to do this would be to sample the lightgrid at the
		// FIXME: entity origin and determine if there is enough diffuse brightness
		// FIXME: to warrant skipping the ambient light
		
		const r_light_t *l = cgi.view->lights;
		for (j = 0; j < cgi.view->num_lights; j++, l++) {

			if (l->type == LIGHT_AMBIENT) {
				continue;
			}

			if (Box3_Contains(l->bounds, e->abs_bounds)) {
				break;
			}
		}

		if (j < cgi.view->num_lights) {
			continue;
		}

		r_light_t light = {
			.type = LIGHT_AMBIENT,
			.bounds = e->abs_bounds,
			.normal = Vec3_Down(),
			.entities[0] = e,
			.num_entities = 1,
		};

		const r_entity_t *child = e + 1;
		for (int32_t j = i + 1; j < cgi.view->num_entities; j++, child++) {
			const r_entity_t *c = child;
			while (c) {
				if (c->parent == e) {
					light.entities[light.num_entities++] = child;
					light.bounds = Box3_Union(light.bounds, child->abs_bounds);
					break;
				}
				c = c->parent;
			}
		}

		light.origin = Box3_Center(light.bounds);

		if (IS_BSP_INLINE_MODEL(e->model)) {
			const r_bsp_inline_model_t *in = e->model->bsp_inline;

			const cm_entity_t *cm = cgi.EntityValue(in->def, "classname");
			if (!g_strcmp0(cm->string, "func_rotating") ||
				!g_strcmp0(cm->string, "func_door_rotating")) {
				light.origin = e->origin;
			}
		}

		light.bounds = Box3_Expand3(light.bounds, Vec3_Scale(Box3_Size(light.bounds), .125f));
		light.bounds.mins.z -= Box3_Size(light.bounds).z;

		light.origin = Vec3_Fmaf(light.origin, Box3_Size(light.bounds).z * 2.f, Vec3_Up());
		light.radius = Vec3_Distance(light.origin, light.bounds.mins);

		cgi.AddLight(cgi.view, &light);
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
			.radius = l->radius,
			.size = 0.f,
			.color = l->color,
			.intensity = l->intensity,
			.normal = Vec3_Zero(),
			.theta = 0.f,
			.bounds = Box3_FromCenterRadius(l->origin, l->radius),
		};

		cgi.BoxLeafnums(out.bounds, NULL, 0, &out.node, 0);

		if (l->decay) {
			assert(out.intensity >= 0.f);
			out.intensity *= (expiration - cgi.client->unclamped_time) / (float) (l->decay);
		}

		cgi.AddLight(cgi.view, &out);
	}

	Cg_AddBspLights();

	Cg_AddAmbientLights();
}

/**
 * @brief 
 */
void Cg_InitLights(void) {

	memset(cg_lights, 0, sizeof(cg_lights));
}
