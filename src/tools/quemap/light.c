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
 * @brief Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
float ColorNormalize(const vec3_t in, vec3_t *out) {
	float max = 0.0;

	*out = in;

	for (int32_t i = 0; i < 3; i++) { // find the brightest component

		if (out->xyz[i] < 0.0) { // enforcing positive values
			out->xyz[i] = 0.0;
		}

		if (out->xyz[i] > max) {
			max = out->xyz[i];
		}
	}

	if (max > 1.0) { // clamp without changing hue
		*out = vec3_scale(*out, 1.0 / max);
	}

	return max;
}

/**
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
vec3_t ColorFilter(const vec3_t in) {
	const vec3_t luminosity = { 0.2125, 0.7154, 0.0721 };

	vec3_t out;
	ColorNormalize(in, &out);

	if (brightness != 1.0) { // apply brightness
		out = vec3_scale(out, brightness);

		ColorNormalize(out, &out);
	}

	if (contrast != 1.0) { // apply contrast

		for (int32_t i = 0; i < 3; i++) {
			out.xyz[i] -= 0.5; // normalize to -0.5 through 0.5
			out.xyz[i] *= contrast; // scale
			out.xyz[i] += 0.5;
		}

		ColorNormalize(out, &out);
	}

	if (saturation != 1.0) { // apply saturation
		const float d = vec3_dot(out, luminosity);
		vec3_t intensity;

		intensity = vec3(d, d, d);
		out = vec3_mix(intensity, out, saturation);

		ColorNormalize(out, &out);
	}

	return out;
}

/**
 * @brief
 */
static light_t *LightForEntity(const GList *entities, const cm_entity_t *entity) {

	light_t *light = NULL;

	const char *classname = Cm_EntityValue(entity, "classname");
	if (!g_strcmp0(classname, "worldspawn")) {

		vec3_t v;
		if (Cm_EntityVector(entity, "ambient", v.xyz, 3) == 3) {

			light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);
			light->type = LIGHT_AMBIENT;
			light->atten = LIGHT_ATTEN_NONE;
			light->radius = LIGHT_RADIUS_AMBIENT;
			light->cluster = -1;

			light->color = v;
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

		Cm_EntityVector(entity, "origin", light->origin.xyz, 3);

		if (Cm_EntityVector(entity, "_color", light->color.xyz, 3) != 3) {
			light->color = vec3(1.0, 1.0, 1.0);
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
				Cm_EntityVector(target, "origin", target_origin.xyz, 3);
				light->normal = vec3_subtract(target_origin, light->origin);
			} else {
				const int32_t i = g_list_index((GList *) entities, entity);
				Mon_SendSelect(MON_WARN, i, 0, va("%s at %s missing target", classname, vtos(light->origin)));
				light->normal = vec3_down();
			}
		} else {
			if (light->type == LIGHT_SPOT) {
				vec3_t angles = { 0.0, 0.0, 0.0 };
				if (Cm_EntityVector(entity, "_angle", &angles.y, 1) == 1) {
					if (angles.y == LIGHT_ANGLE_UP) {
						light->normal = vec3_up();
					} else if (angles.y == LIGHT_ANGLE_DOWN) {
						light->normal = vec3_down();
					} else {
						vec3_vectors(angles, &light->normal, NULL, NULL);
					}
				} else {
					light->normal = vec3_down();
				}
			} else {
				light->normal = vec3_down();
			}
		}

		light->normal = vec3_normalize(light->normal);

		if (light->type == LIGHT_SPOT) {
			if (Cm_EntityVector(entity, "_cone", &light->theta, 1) == 1) {
				light->theta = maxf(1.0, light->theta);
			} else {
				light->theta = LIGHT_CONE;
			}
			light->theta = radians(light->theta);
		}

		if (Cm_EntityVector(entity, "_size", &light->size, 1) == 1) {
			if (light->size) {
				light->size = maxf(LIGHT_SIZE_STEP, light->size);
			}
		} else {
			if (light->type == LIGHT_SUN) {
				light->size = LIGHT_SIZE_SUN;
			}
		}

		if (light->type != LIGHT_SUN) {
			light->radius += light->size;
		}

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
static light_t *LightForPatch(const patch_t *patch) {

	light_t *light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

	light->type = LIGHT_PATCH;
	light->atten = LIGHT_ATTEN_INVERSE_SQUARE;

	Cm_WindingCenter(patch->winding, light->origin);

	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane_num];
	light->origin = vec3_add(light->origin, vec3_scale(plane->normal, 4.0));

	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[patch->face->texinfo];

	light->color = GetTextureColor(texinfo->texture);
	const float brightness = ColorNormalize(light->color, &light->color);
	if (brightness < 1.0) {
		light->color = vec3_scale(light->color, 1.0 / brightness);
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

			for (const patch_t *patch = &patches[i]; patch; patch = patch->next) {
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
 * @return An indirect light source from a directly lit patch.
 */
static light_t *LightForLightmappedPatch(const lightmap_t *lm, const patch_t *patch) {

	light_t *light = NULL;

	vec2_t patch_mins = Vec2_Mins();
	vec2_t patch_maxs = Vec2_Maxs();

	for (int32_t i = 0; i < patch->winding->num_points; i++) {

		vec3_t point = vec3_subtract(patch->winding->points[i], patch->origin);

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, point.xyz, st.xyz);

		patch_mins = Vec2_Minf(patch_mins, vec3_xy(st));
		patch_maxs = Vec2_Maxf(patch_maxs, vec3_xy(st));
	}

	assert(patch_mins.x >= lm->st_mins.x - SIDE_EPSILON);
	assert(patch_mins.y >= lm->st_mins.y - SIDE_EPSILON);
	assert(patch_maxs.x <= lm->st_maxs.x + SIDE_EPSILON);
	assert(patch_maxs.y <= lm->st_maxs.y + SIDE_EPSILON);

	const int16_t w = patch_maxs.x - patch_mins.x;
	const int16_t h = patch_maxs.y - patch_mins.y;

	vec3_t lightmap;
	lightmap = vec3_zero();

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++) {

			const int32_t ds = patch_mins.x - lm->st_mins.x + s;
			const int32_t dt = patch_mins.y - lm->st_mins.y + t;

			const luxel_t *l = &lm->luxels[dt * lm->w + ds];

			assert(l->s == ds);
			assert(l->t == dt);

			lightmap = vec3_add(lightmap, l->diffuse);
			lightmap = vec3_add(lightmap, l->radiosity);
		}
	}

	if (!vec3_equal(lightmap, vec3_zero())) {

		light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

		light->type = LIGHT_INDIRECT;
		light->atten = LIGHT_ATTEN_INVERSE_SQUARE;

		Cm_WindingCenter(patch->winding, light->origin);
		light->origin = vec3_add(light->origin, vec3_scale(lm->plane->normal, 4.0));

		lightmap = vec3_scale(lightmap, 1.0 / (w * h));
		light->radius = ColorNormalize(lightmap, &lightmap);

		const vec3_t diffuse = GetTextureColor(lm->texinfo->texture);
		light->color = vec3_multiply(lightmap, diffuse);

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

		for (const patch_t *patch = &patches[i]; patch; patch = patch->next) {
			light_t *light = LightForLightmappedPatch(lm, patch);
			if (light) {
				lights = g_list_prepend(lights, light);
			}
		}
	}

	Com_Verbose("Indirect lighting for %d patches\n", g_list_length(lights));
}
