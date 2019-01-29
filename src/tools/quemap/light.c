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

#include "light.h"
#include "qlight.h"

GList *lights = NULL;

/**
 * @brief
 */
light_t *LightForEntity(const GList *entities, const cm_entity_t *entity) {

	light_t *light = NULL;

	const char *classname = Cm_EntityValue(entity, "classname");
	if (!g_strcmp0(classname, "worldspawn")) {

		vec3_t v;
		if (Cm_EntityVector(entity, "ambient", v, 3) == 3) {

			light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);
			light->type = LIGHT_AMBIENT;
			light->radius = LIGHT_RADIUS_AMBIENT;
			light->cluster = -1;

			VectorCopy(v, light->color);
		}
	}

	if (!g_strcmp0(classname, "light") ||
		!g_strcmp0(classname, "light_spot") ||
		!g_strcmp0(classname, "light_sun")) {

		light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

		const char *targetname = Cm_EntityValue(entity, "target");

		if (!g_strcmp0(classname, "light_sun")) {
			light->type = LIGHT_SUN;
			light->atten = LIGHT_ATTEN_NONE;
		} else if (!g_strcmp0(classname, "light_spot") || targetname) {
			light->type = LIGHT_SPOT;
			light->atten = LIGHT_ATTEN_INVERSE_SQUARE;
		} else {
			light->type = LIGHT_POINT;
			light->atten = LIGHT_ATTEN_INVERSE_SQUARE;
		}

		Cm_EntityVector(entity, "origin", light->origin, 3);

		if (Cm_EntityVector(entity, "_color", light->color, 3) != 3) {
			VectorSet(light->color, 1.0, 1.0, 1.0);
		}

		if (Cm_EntityVector(entity, "light", &light->radius, 1) != 1) {
			light->radius = LIGHT_RADIUS;
		}

		if (targetname) {
			cm_entity_t *target = NULL;
			for (const GList *e = entities; e; e = e->next) {
				if (!g_strcmp0(targetname, Cm_EntityValue(e->data, "targetname"))) {
					target = e->data;
					break;
				}
			}
			if (target) {
				vec3_t target_origin;
				Cm_EntityVector(target, "origin", target_origin, 3);
				VectorSubtract(target_origin, light->origin, light->normal);
			} else {
				const int32_t i = g_list_index((GList *) entities, entity);
				Mon_SendSelect(MON_WARN, i, 0, va("Spot light at %s missing target", vtos(light->origin)));
				VectorCopy(vec3_down, light->normal);
			}
		} else {
			if (light->type == LIGHT_SPOT) {
				vec3_t angles = { 0.0, 0.0, 0.0 };
				if (Cm_EntityVector(entity, "_angle", &angles[YAW], 1) == 1) {
					if (angles[YAW] == LIGHT_ANGLE_UP) {
						VectorCopy(vec3_up, light->normal);
					} else if (angles[YAW] == LIGHT_ANGLE_DOWN) {
						VectorCopy(vec3_down, light->normal);
					} else {
						AngleVectors(angles, light->normal, NULL, NULL);
					}
				} else {
					VectorCopy(vec3_down, light->normal);
				}
			} else {
				VectorCopy(vec3_down, light->normal);
			}
		}

		if (light->type == LIGHT_SPOT) {
			if (Cm_EntityVector(entity, "_cone", &light->cone, 1) != 1) {
				light->cone = LIGHT_CONE;
			}
		}

		VectorNegate(light->normal, light->normal);
		VectorNormalize(light->normal);

		const char *atten = Cm_EntityValue(entity, "atten") ?: Cm_EntityValue(entity, "attenuation");
		if (atten) {
			light->atten = (light_atten_t) strtol(atten, NULL, 10);
		}

		if (light->atten == LIGHT_ATTEN_NONE) {
			light->cluster = -1;
		} else {
			light->cluster = Cm_LeafCluster(Cm_PointLeafnum(light->origin, 0));
		}
	}

	return light;
}

/**
 * @brief
 */
light_t *LightForPatch(const patch_t *patch) {

	light_t *light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

	light->type = LIGHT_PATCH;
	light->atten = LIGHT_ATTEN_INVERSE_SQUARE;

	Cm_WindingCenter(patch->winding, light->origin);

	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane_num];
	VectorMA(light->origin, 4.0, plane->normal, light->origin);

	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[patch->face->texinfo];

	GetTextureColor(texinfo->texture, light->color);
	const vec_t brightness = ColorNormalize(light->color, light->color);
	if (brightness < 1.0) {
		VectorScale(light->color, 1.0 / brightness, light->color);
	}

	light->radius = texinfo->value ?: DEFAULT_LIGHT;

	light->cluster = Cm_LeafCluster(Cm_PointLeafnum(light->origin, 0));

	return light;
}

/**
 * @brief
 */
void BuildDirectLights(const GList *entities) {

	g_list_free_full(lights, Mem_Free);
	lights = NULL;

	for (const GList *e = entities; e; e = e->next) {
		light_t *light = LightForEntity(entities, e->data);
		if (light) {
			lights = g_list_prepend(lights, light);
		}
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];

		if (texinfo->flags & SURF_LIGHT) {

			for (const patch_t *patch = patches[i]; patch; patch = patch->next) {
				light_t *light = LightForPatch(patch);
				if (light) {
					lights = g_list_prepend(lights, light);
				}
			}
		}
	}

	Com_Verbose("Direct lighting for %d lights\n", g_list_length(lights));
}

/**
 * @return A light source for indirect light from a directly lit patch.
 */
light_t *LightForLightmappedPatch(const lightmap_t *lm, const patch_t *patch) {

	light_t *light = NULL;

	vec2_t patch_mins, patch_maxs;

	patch_mins[0] = patch_mins[1] = FLT_MAX;
	patch_maxs[0] = patch_maxs[1] = -FLT_MAX;

	const vec_t *offset = patch_offsets[lm - lightmaps];

	for (int32_t i = 0; i < patch->winding->num_points; i++) {

		vec3_t point, st;
		VectorSubtract(patch->winding->points[i], offset, point);

		Matrix4x4_Transform(&lm->matrix, point, st);

		for (int32_t j = 0; j < 2; j++) {

			if (st[j] < patch_mins[j]) {
				patch_mins[j] = st[j];
			}
			if (st[j] > patch_maxs[j]) {
				patch_maxs[j] = st[j];
			}
		}
	}

	assert(patch_mins[0] >= lm->st_mins[0] - SIDE_EPSILON);
	assert(patch_mins[1] >= lm->st_mins[1] - SIDE_EPSILON);
	assert(patch_maxs[0] <= lm->st_maxs[0] + SIDE_EPSILON);
	assert(patch_maxs[1] <= lm->st_maxs[1] + SIDE_EPSILON);

	const int16_t w = patch_maxs[0] - patch_mins[0];
	const int16_t h = patch_maxs[1] - patch_mins[1];

	vec3_t lightmap;
	VectorClear(lightmap);

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++) {

			const int32_t ds = patch_mins[0] - lm->st_mins[0] + s;
			const int32_t dt = patch_mins[1] - lm->st_mins[1] + t;

			const luxel_t *l = &lm->luxels[dt * lm->w + ds];

			assert(l->s == ds);
			assert(l->t == dt);

			VectorAdd(lightmap, l->direct, lightmap);
			VectorAdd(lightmap, l->indirect, lightmap);
		}
	}

	if (!VectorCompare(lightmap, vec3_origin)) {
		vec3_t diffuse;

		light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

		light->type = LIGHT_PATCH;
		light->atten = LIGHT_ATTEN_INVERSE_SQUARE;

		Cm_WindingCenter(patch->winding, light->origin);
		VectorMA(light->origin, 4.0, lm->plane->normal, light->origin);

		VectorScale(lightmap, 1.0 / (w * h), lightmap);
		light->radius = ColorNormalize(lightmap, lightmap);

		GetTextureColor(lm->texinfo->texture, diffuse);
		VectorMultiply(lightmap, diffuse, light->color);

		light->cluster = Cm_LeafCluster(Cm_PointLeafnum(light->origin, 0));
	}

	return light;
}

/**
 * @brief
 */
void BuildIndirectLights(void) {

	g_list_free_full(lights, Mem_Free);
	lights = NULL;

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {

		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & (SURF_LIGHT | SURF_SKY)) {
			continue;
		}

		for (const patch_t *patch = patches[i]; patch; patch = patch->next) {

			light_t *light = LightForLightmappedPatch(lm, patch);
			if (light) {
				lights = g_list_prepend(lights, light);
			}
		}
	}

	Com_Verbose("Indirect lighting for %d patches\n", g_list_length(lights));
}
