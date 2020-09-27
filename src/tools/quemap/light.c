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

GArray *lights = NULL;

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
	const vec3_t luminosity = Vec3(0.2125, 0.7154, 0.0721);

	vec3_t out;
	ColorNormalize(in, &out);

	if (brightness != 1.0) { // apply brightness
		out = Vec3_Scale(out, brightness);

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
static void LightForEntity(const GList *entities, const cm_entity_t *entity) {

	const char *classname = Cm_EntityValue(entity, "classname");
	if (!g_strcmp0(classname, "worldspawn")) {

		vec3_t v;
		if (Cm_EntityVector(entity, "ambient", v.xyz, 3) == 3) {

			light_t light;
			light.type = LIGHT_AMBIENT;
			light.atten = LIGHT_ATTEN_NONE;
			light.radius = LIGHT_RADIUS_AMBIENT;
			light.cluster = -1;

			light.color = v;
			g_array_append_val(lights, light);
		}
	} else if (!g_strcmp0(classname, "light") ||
		!g_strcmp0(classname, "light_spot") ||
		!g_strcmp0(classname, "light_sun")) {

		light_t light;

		const char *targetname = Cm_EntityValue(entity, "target");

		if (!g_strcmp0(classname, "light_sun")) {
			light.type = LIGHT_SUN;
			light.atten = LIGHT_ATTEN_NONE;
		} else if (!g_strcmp0(classname, "light_spot") || targetname) {
			light.type = LIGHT_SPOT;
			light.atten = LIGHT_ATTEN_INVERSE_SQUARE;
		} else {
			light.type = LIGHT_POINT;
			light.atten = LIGHT_ATTEN_INVERSE_SQUARE;
		}

		Cm_EntityVector(entity, "origin", light.origin.xyz, 3);

		if (Cm_EntityVector(entity, "_color", light.color.xyz, 3) != 3) {
			light.color = Vec3(1.0, 1.0, 1.0);
		}

		if (Cm_EntityVector(entity, "light", &light.radius, 1) != 1) {
			light.radius = LIGHT_RADIUS;
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
				light.normal = Vec3_Subtract(target_origin, light.origin);
			} else {
				const int32_t i = g_list_index((GList *) entities, entity);
				Mon_SendSelect(MON_WARN, i, 0, va("%s at %s missing target", classname, vtos(light.origin)));
				light.normal = Vec3_Down();
			}
		} else {
			if (light.type == LIGHT_SPOT) {
				vec3_t angles = Vec3_Zero();
				if (Cm_EntityVector(entity, "_angle", &angles.y, 1) == 1) {
					if (angles.y == LIGHT_ANGLE_UP) {
						light.normal = Vec3_Up();
					} else if (angles.y == LIGHT_ANGLE_DOWN) {
						light.normal = Vec3_Down();
					} else {
						Vec3_Vectors(angles, &light.normal, NULL, NULL);
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
			if (Cm_EntityVector(entity, "_cone", &light.theta, 1) == 1) {
				light.theta = Maxf(1.0, light.theta);
			} else {
				light.theta = LIGHT_CONE;
			}
			light.theta = Radians(light.theta);
		}

		if (Cm_EntityVector(entity, "_size", &light.size, 1) == 1) {
			if (light.size) {
				light.size = Maxf(LIGHT_SIZE_STEP, light.size);
			}
		} else {
			if (light.type == LIGHT_SUN) {
				light.size = LIGHT_SIZE_SUN;
			}
		}

		if (light.type != LIGHT_SUN) {
			light.radius += light.size;
		}

		const char *atten = Cm_EntityValue(entity, "atten") ?: Cm_EntityValue(entity, "attenuation");
		if (atten) {
			light.atten = (light_atten_t) strtol(atten, NULL, 10);
		}

		if (light.atten == LIGHT_ATTEN_NONE) {
			light.cluster = -1;
		} else {
			light.cluster = Cm_LeafCluster(Cm_PointLeafnum(light.origin, 0));
		}

		g_array_append_val(lights, light);
	}
}

/**
 * @brief
 */
static void LightForPatch(const patch_t *patch) {

	light_t light;

	light.type = LIGHT_PATCH;
	light.atten = LIGHT_ATTEN_INVERSE_SQUARE;

	light.origin = Cm_WindingCenter(patch->winding);

	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane_num];
	light.origin = Vec3_Add(light.origin, Vec3_Scale(plane->normal, 4.0));

	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[patch->face->texinfo];

	light.color = GetTextureColor(texinfo->texture);
	const float brightness = ColorNormalize(light.color, &light.color);
	if (brightness < 1.0) {
		light.color = Vec3_Scale(light.color, 1.0 / brightness);
	}

	light.radius = texinfo->value ?: DEFAULT_LIGHT;

	light.cluster = Cm_LeafCluster(Cm_PointLeafnum(light.origin, 0));

	g_array_append_val(lights, light);
}

/**
 * @brief
 */
static void FreeLights(void) {

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
 * @brief
 */
static GPtrArray *BoxLights(const vec3_t box_mins, const vec3_t box_maxs) {

	byte pvs[MAX_BSP_LEAFS >> 3];
	Cm_BoxPVS(box_mins, box_maxs, pvs);

	GPtrArray *box_lights = g_ptr_array_new();

	const light_t *light = (light_t *) lights->data;
	for (guint i = 0; i < lights->len; i++, light++) {

		if (light->cluster != -1) {
			if (!(pvs[light->cluster >> 3] & (1 << (light->cluster & 7)))) {
				continue;
			}
		}

		if (light->atten != LIGHT_ATTEN_NONE) {
			const vec3_t radius = Vec3(light->radius, light->radius, light->radius);
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

		const vec3_t node_mins = Vec3s_CastVec3(node->mins);
		const vec3_t node_maxs = Vec3s_CastVec3(node->maxs);

		node_lights[i] = BoxLights(node_mins, node_maxs);
	}

	const bsp_leaf_t *leaf = bsp_file.leafs;
	for (int32_t i = 0; i < bsp_file.num_leafs; i++, leaf++) {

		const vec3_t leaf_mins = Vec3s_CastVec3(leaf->mins);
		const vec3_t leaf_maxs = Vec3s_CastVec3(leaf->maxs);

		leaf_lights[i] = BoxLights(leaf_mins, leaf_maxs);
	}
}

/**
 * @brief
 */
void BuildDirectLights(const GList *entities) {

	FreeLights();

	lights = g_array_new(false, false, sizeof(light_t));

	for (const GList *e = entities; e; e = e->next) {
		LightForEntity(entities, e->data);
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

	light_t light;

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

	light.type = LIGHT_INDIRECT;
	light.atten = LIGHT_ATTEN_INVERSE_SQUARE;

	light.origin = Cm_WindingCenter(patch->winding);
	light.origin = Vec3_Add(light.origin, Vec3_Scale(lm->plane->normal, 4.0));

	lightmap = Vec3_Scale(lightmap, 1.0 / (w * h));
	light.radius = ColorNormalize(lightmap, &lightmap);

	const vec3_t diffuse = GetTextureColor(lm->texinfo->texture);
	light.color = Vec3_Multiply(lightmap, diffuse);

	light.cluster = Cm_LeafCluster(Cm_PointLeafnum(light.origin, 0));

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
