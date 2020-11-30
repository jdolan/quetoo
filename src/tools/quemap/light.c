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

static GArray *lights = NULL;

GPtrArray *node_lights[MAX_BSP_NODES];
GPtrArray *leaf_lights[MAX_BSP_LEAFS];

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
		*out = Vec3_Scale(*out, 1.0 / max);
	}

	return max;
}

/**
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
vec3_t ColorFilter(const vec3_t in) {
	const vec3_t luminosity = Vec3(0.2125f, 0.7154f, 0.0721f);

	vec3_t out;
	ColorNormalize(in, &out);

	if (brightness != 1.f) { // apply brightness
		out = Vec3_Scale(out, brightness);

		ColorNormalize(out, &out);
	}

	if (contrast != 1.f) { // apply contrast

		for (int32_t i = 0; i < 3; i++) {
			out.xyz[i] -= 0.5f; // normalize to -0.5 through 0.5
			out.xyz[i] *= contrast; // scale
			out.xyz[i] += 0.5f;
		}

		ColorNormalize(out, &out);
	}

	if (saturation != 1.f) { // apply saturation
		const float d = Vec3_Dot(out, luminosity);
		vec3_t intensity;

		intensity = Vec3(d, d, d);
		out = Vec3_Mix(intensity, out, saturation);

		ColorNormalize(out, &out);
	}

	return out;
}

/**
 * @brief
 */
static void LightForEntity(const cm_entity_t *entity) {

	const char *class_name = Cm_EntityValue(entity, "classname")->string;
	if (!g_strcmp0(class_name, "worldspawn")) {

		const vec3_t ambient = Cm_EntityValue(entity, "ambient")->vec3;
		if (!Vec3_Equal(ambient, Vec3_Zero())) {

			light_t light;
			light.type = LIGHT_AMBIENT;
			light.atten = LIGHT_ATTEN_NONE;
			light.color = ambient;
			light.radius = LIGHT_RADIUS_AMBIENT;

			g_array_append_val(lights, light);
		}
	} else if (!g_strcmp0(class_name, "light") ||
		!g_strcmp0(class_name, "light_spot") ||
		!g_strcmp0(class_name, "light_sun")) {

		light_t light;

		if (!g_strcmp0(class_name, "light_sun")) {
			light.type = LIGHT_SUN;
			light.atten = LIGHT_ATTEN_NONE;
		} else if (!g_strcmp0(class_name, "light_spot")) {
			light.type = LIGHT_SPOT;
			light.atten = LIGHT_ATTEN_LINEAR;
		} else {
			light.type = LIGHT_POINT;
			light.atten = LIGHT_ATTEN_LINEAR;
		}

		light.origin = Cm_EntityValue(entity, "origin")->vec3;

		if (Cm_EntityValue(entity, "_color")->parsed & ENTITY_VEC3) {
			light.color = Cm_EntityValue(entity, "_color")->vec3;
		} else {
			light.color = LIGHT_COLOR;
		}

		light.radius = Cm_EntityValue(entity, "light")->value;
		if (light.radius == 0.f) {
			light.radius = LIGHT_RADIUS;
		}

		switch (light.type) {
			case LIGHT_POINT:
			case LIGHT_SPOT:
				light.radius *= lightscale_point;
				break;
			default:
				break;
		}

		if (Cm_EntityValue(entity, "target")->parsed & ENTITY_STRING) {
			const char *targetname = Cm_EntityValue(entity, "target")->string;
			cm_entity_t *target = NULL;
			cm_entity_t **e = Cm_Bsp()->entities;
			for (size_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
				if (!g_strcmp0(targetname, Cm_EntityValue(*e, "targetname")->string)) {
					target = *e;
					break;
				}
			}
			if (target) {
				const vec3_t target_origin = Cm_EntityValue(target, "origin")->vec3;
				light.normal = Vec3_Subtract(target_origin, light.origin);
			} else {
				Mon_SendSelect(MON_WARN, Cm_EntityNumber(entity), 0,
							   va("%s at %s missing target", class_name, vtos(light.origin)));
				light.normal = Vec3_Down();
			}
		} else {
			if (light.type == LIGHT_SPOT) {
				if (Cm_EntityValue(entity, "_angle")->parsed & ENTITY_INTEGER) {
					const int32_t angle = Cm_EntityValue(entity, "_angle")->integer;
					if (angle == LIGHT_ANGLE_UP) {
						light.normal = Vec3_Up();
					} else if (angle == LIGHT_ANGLE_DOWN) {
						light.normal = Vec3_Down();
					} else {
						Vec3_Vectors(Vec3(0.f, angle, 0.f), &light.normal, NULL, NULL);
					}
				} else {
					light.normal = Vec3_Down();
				}
			} else {
				light.normal = Vec3_Down();
			}
		}

		light.normal = Vec3_Normalize(light.normal);

		if (light.type == LIGHT_SPOT) {
			if (Cm_EntityValue(entity, "_cone")->parsed & ENTITY_FLOAT) {
				light.theta = Cm_EntityValue(entity, "_cone")->value;
			} else {
				light.theta = LIGHT_CONE;
			}
			light.theta = Radians(light.theta);
		}

		light.size = Cm_EntityValue(entity, "_size")->value;
		if (light.size == 0.f) {
			if (light.type == LIGHT_SUN) {
				light.size = LIGHT_SIZE_SUN;
			} else {
				light.size = LIGHT_SIZE_STEP;
			}
		}

		if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
			light.atten = Cm_EntityValue(entity, "atten")->integer;
		}

		g_array_append_val(lights, light);
	}
}

/**
 * @brief
 */
static void LightForPatch(const patch_t *patch) {

	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane_num];

	light_t light = {};

	light.type = LIGHT_PATCH;
	light.atten = LIGHT_ATTEN_INVERSE_SQUARE;
	light.size = sqrtf(Cm_WindingArea(patch->winding));
	light.origin = Cm_WindingCenter(patch->winding);
	light.origin = Vec3_Add(light.origin, Vec3_Scale(plane->normal, 4.0));

	if (Light_PointContents(light.origin, 0) & CONTENTS_SOLID) {
		return;
	}

	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[patch->face->texinfo];

	light.color = GetTextureColor(texinfo->texture);
	const float brightness = ColorNormalize(light.color, &light.color);
	if (brightness < 1.0) {
		light.color = Vec3_Scale(light.color, 1.0 / brightness);
	}

	light.radius = (texinfo->value ?: DEFAULT_LIGHT) * lightscale_patch;

	g_array_append_val(lights, light);
}

/**
 * @brief
 */
void FreeLights(void) {

	if (!lights) {
		return;
	}

	g_array_free(lights, true);
	lights = NULL;

	for (int32_t i = 0; i < bsp_file.num_nodes; i++) {
		if (node_lights[i]) {
			g_ptr_array_free(node_lights[i], true);
			node_lights[i] = NULL;
		}
	}

	for (int32_t i = 0; i < bsp_file.num_leafs; i++) {
		if (leaf_lights[i]) {
			g_ptr_array_free(leaf_lights[i], true);
			leaf_lights[i] = NULL;
		}
	}
}

/**
 * @return A GPtrArray of all light sources that intersect the specified bounds.
 */
static GPtrArray *BoxLights(const vec3_t box_mins, const vec3_t box_maxs) {

	GPtrArray *box_lights = g_ptr_array_new();

	const light_t *light = (light_t *) lights->data;
	for (guint i = 0; i < lights->len; i++, light++) {

		if (light->atten != LIGHT_ATTEN_NONE) {
			const vec3_t radius = Vec3(light->radius + light->size * .5f,
									   light->radius + light->size * .5f,
									   light->radius + light->size * .5f);

			const vec3_t mins = Vec3_Add(light->origin, Vec3_Scale(radius, -1.f));
			const vec3_t maxs = Vec3_Add(light->origin, Vec3_Scale(radius,  1.f));

			if (!Vec3_BoxIntersect(box_mins, box_maxs, mins, maxs)) {
				continue;
			}
		}

		g_ptr_array_add(box_lights, (gpointer) light);
	}

	return box_lights;
}

/**
 * @brief Hashes all light sources into bins by node and leaf.
 */
static void HashLights(void) {

	assert(lights);

	const bsp_node_t *node = bsp_file.nodes;
	for (int32_t i = 0; i < bsp_file.num_nodes; i++, node++) {

		if (node->num_faces == 0) {
			continue;
		}

		node_lights[i] = BoxLights(node->mins, node->maxs);
	}

	const bsp_leaf_t *leaf = bsp_file.leafs;
	for (int32_t i = 0; i < bsp_file.num_leafs; i++, leaf++) {

		if (leaf->cluster == 0) {
			continue;
		}

		leaf_lights[i] = BoxLights(leaf->mins, leaf->maxs);
	}
}

/**
 * @brief
 */
void BuildDirectLights(void) {

	FreeLights();

	lights = g_array_new(false, false, sizeof(light_t));

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (size_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		LightForEntity(*entity);
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];

		if (texinfo->flags & SURF_LIGHT) {

			for (const patch_t *patch = &patches[i]; patch; patch = patch->next) {
				LightForPatch(patch);
			}
		}
	}

	HashLights();

	Com_Verbose("Direct lighting for %d lights\n", lights->len);
}

/**
 * @brief Add an indirect light source from a directly lit patch.
 */
static void LightForLightmappedPatch(const lightmap_t *lm, const patch_t *patch) {

	light_t light = {};

	light.type = LIGHT_INDIRECT;
	light.atten = LIGHT_ATTEN_INVERSE_SQUARE;
	light.size = sqrtf(Cm_WindingArea(patch->winding));
	light.origin = Cm_WindingCenter(patch->winding);
	light.origin = Vec3_Add(light.origin, Vec3_Scale(lm->plane->normal, 4.0));

	if (Light_PointContents(light.origin, 0) & CONTENTS_SOLID) {
		return;
	}

	vec2_t patch_mins = Vec2_Mins();
	vec2_t patch_maxs = Vec2_Maxs();

	for (int32_t i = 0; i < patch->winding->num_points; i++) {

		vec3_t point = Vec3_Subtract(patch->winding->points[i], patch->origin);

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, point.xyz, st.xyz);

		patch_mins = Vec2_Minf(patch_mins, Vec3_XY(st));
		patch_maxs = Vec2_Maxf(patch_maxs, Vec3_XY(st));
	}

	assert(patch_mins.x >= lm->st_mins.x - 0.001);
	assert(patch_mins.y >= lm->st_mins.y - 0.001);
	assert(patch_maxs.x <= lm->st_maxs.x + 0.001);
	assert(patch_maxs.y <= lm->st_maxs.y + 0.001);

	const int16_t w = patch_maxs.x - patch_mins.x;
	const int16_t h = patch_maxs.y - patch_mins.y;

	vec3_t lightmap = Vec3_Zero();

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++) {

			const int32_t ds = patch_mins.x - lm->st_mins.x + s;
			const int32_t dt = patch_mins.y - lm->st_mins.y + t;

			const luxel_t *l = &lm->luxels[dt * lm->w + ds];

			assert(l->s == ds);
			assert(l->t == dt);

			lightmap = Vec3_Add(lightmap, bounce ? l->radiosity[bounce - 1] : l->diffuse);
		}
	}

	if (Vec3_Equal(lightmap, Vec3_Zero())) {
		return;
	}

	lightmap = Vec3_Scale(lightmap, 1.0 / (w * h));
	light.radius = ColorNormalize(lightmap, &lightmap) * radiosity;

	const vec3_t diffuse = GetTextureColor(lm->texinfo->texture);
	light.color = Vec3_Multiply(lightmap, diffuse);

	g_array_append_val(lights, light);
}

/**
 * @brief
 */
void BuildIndirectLights(void) {

	FreeLights();

	lights = g_array_new(false, false, sizeof(light_t));

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {

		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & (SURF_LIGHT | SURF_MASK_NO_LIGHTMAP)) {
			continue;
		}

		for (const patch_t *patch = &patches[i]; patch; patch = patch->next) {
			LightForLightmappedPatch(lm, patch);
		}
	}

	HashLights();

	Com_Verbose("Indirect lighting for %d patches\n", lights->len);
}
