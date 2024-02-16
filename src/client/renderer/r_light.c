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

r_lights_t r_lights;

/**
 * @brief
 */
void R_AddLight(r_view_t *view, const r_light_t *l) {

	if (view->num_lights == MAX_LIGHTS) {
		Com_Debug(DEBUG_RENDERER, "MAX_LIGHTS\n");
		return;
	}

	switch (l->type) {
		case LIGHT_AMBIENT:
		case LIGHT_SUN:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_BRUSH_SIDE:
			if (r_shadowmap->integer < 2) {
				return;
			}
			break;
		case LIGHT_DYNAMIC:
			break;
		default:
			return;
	}

	if (R_CulludeBox(view, l->bounds)) {
		return;
	}

	view->lights[view->num_lights++] = *l;
}

/**
 * @brief
 */
static void R_AddLightUniform(r_light_t *in) {

	if (r_lights.block.num_lights == MAX_LIGHT_UNIFORMS) {
		Com_Warn("MAX_LIGHT_UNIFORMS\n");
		return;
	}

	in->index = r_lights.block.num_lights++;

	r_light_uniform_t *out = &r_lights.block.lights[in->index];

	out->model = Vec3_ToVec4(in->origin, in->radius);
	out->mins = Vec3_ToVec4(in->bounds.mins, in->size);
	out->maxs = Vec3_ToVec4(in->bounds.maxs, in->atten);
	out->position = Vec3_ToVec4(Mat4_Transform(r_uniforms.block.view, in->origin), in->type);
	out->normal = Mat4_TransformPlane(r_uniforms.block.view, in->normal, 0.f);
	out->color = Vec3_ToVec4(in->color, in->intensity);
}

/**
 * @brief
 */
static bool R_IsLightSource(const r_light_t *light, const r_entity_t *e) {

	if (light->source == NULL) {
		return false;
	}

	while (e) {
		if (light->source == e->id) {
			return true;
		}
		e = e->parent;
	}

	return false;
}

/**
 * @brief Cull lights by occlusion queries, and transform them into view space.
 */
void R_UpdateLights(r_view_t *view) {

	r_light_uniform_block_t *out = &r_lights.block;

	memset(out, 0, sizeof(*out));

	out->light_projection = Mat4_FromFrustum(-1.f, 1.f, -1.f, 1.f, NEAR_DIST, MAX_WORLD_DIST);
	out->light_view = Mat4_LookAt(Vec3_Zero(), Vec3(0.f, 0.f, -1.f), Vec3(0.f, 1.f, 0.f));

	out->light_projection_cube = Mat4_FromFrustum(-1.f, 1.f, -1.f, 1.f, NEAR_DIST, MAX_WORLD_DIST);
	out->light_view_cube[0] = Mat4_LookAt(Vec3_Zero(), Vec3( 1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
	out->light_view_cube[1] = Mat4_LookAt(Vec3_Zero(), Vec3(-1.f,  0.f,  0.f), Vec3(0.f, -1.f,  0.f));
	out->light_view_cube[2] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  1.f,  0.f), Vec3(0.f,  0.f,  1.f));
	out->light_view_cube[3] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f, -1.f,  0.f), Vec3(0.f,  0.f, -1.f));
	out->light_view_cube[4] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f,  1.f), Vec3(0.f, -1.f,  0.f));
	out->light_view_cube[5] = Mat4_LookAt(Vec3_Zero(), Vec3( 0.f,  0.f, -1.f), Vec3(0.f, -1.f,  0.f));

	vec3_t pos = view->origin;
	if (r_draw_light_bounds->value) {
		const vec3_t end = Vec3_Fmaf(view->origin, MAX_WORLD_DIST, view->forward);
		pos = Cm_BoxTrace(view->origin, end, Box3_Zero(), 0, CONTENTS_MASK_VISIBLE).end;
	}

	r_light_t *l = view->lights;
	for (int32_t i = 0; i < view->num_lights; i++, l++) {

		if (r_draw_light_bounds->value && Vec3_Distance(pos, l->origin) < 32.f) {
			R_Draw3DBox(l->bounds, Color3fv(l->color), false);
		}

		l->index = -1;

		if (l->num_entities == 0) {

			const r_entity_t *e = view->entities;
			for (int32_t j = 0; j < view->num_entities; j++, e++) {

				if (!e->model) {
					continue;
				}

				if (e->effects & EF_NO_SHADOW) {
					continue;
				}

				if (R_IsLightSource(l, e)) {
					continue;
				}

				if (Box3_Intersects(e->abs_bounds, l->bounds)) {
					l->entities[l->num_entities++] = e;

					if (l->num_entities == MAX_LIGHT_ENTITIES) {
						Com_Warn("MAX_LIGHT_ENTITIES\n");
						break;
					}
				}
			}
		}

		if (l->num_entities == 0 && l->type != LIGHT_DYNAMIC) {
			continue;
		}

		R_AddLightUniform(l);
	}

	r_stats.lights = out->num_lights;

	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(*out), out);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

/**
 * @brief
 */
void R_InitLights(void) {

	memset(&r_lights, 0, sizeof(r_lights));

	glGenBuffers(1, &r_lights.buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, r_lights.buffer);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(r_lights.block), &r_lights.block, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_UNIFORM_BUFFER, 1, r_lights.buffer);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);

	R_GetError(NULL);
}

/**
 * @brief
 */
void R_ShutdownLights(void) {

	glDeleteBuffers(1, &r_lights.buffer);
}
