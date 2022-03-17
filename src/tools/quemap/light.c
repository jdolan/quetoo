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
#include "points.h"
#include "qlight.h"

static GArray *lights = NULL;

GPtrArray *node_lights[MAX_BSP_NODES];
GPtrArray *leaf_lights[MAX_BSP_LEAFS];
GPtrArray *unattenuated_lights;

/**
 * @brief Clamps the components of the specified vector to 1.0, scaling the vector
 * down if necessary.
 */
vec3_t ColorNormalize(const vec3_t in) {

	vec3_t out = in;

	float max = 0.0;
	for (int32_t i = 0; i < 3; i++) { // find the brightest component

		if (out.xyz[i] < 0.0) { // enforcing positive values
			out.xyz[i] = 0.0;
		}

		if (out.xyz[i] > max) {
			max = out.xyz[i];
		}
	}

	if (max > 1.0) { // clamp without changing hue
		out = Vec3_Scale(out, 1.0 / max);
	}

	return out;
}

/**
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
vec3_t ColorFilter(const vec3_t in) {
	const vec3_t luminosity = Vec3(0.2125f, 0.7154f, 0.0721f);

	vec3_t out = ColorNormalize(in);

	if (brightness != 1.f) { // apply brightness
		out = Vec3_Scale(out, brightness);
		out = ColorNormalize(out);
	}

	if (contrast != 1.f) { // apply contrast

		for (int32_t i = 0; i < 3; i++) {
			out.xyz[i] -= 0.5f; // normalize to -0.5 through 0.5
			out.xyz[i] *= contrast; // scale
			out.xyz[i] += 0.5f;
		}

		out = ColorNormalize(out);
	}

	if (saturation != 1.f) { // apply saturation
		const float d = Vec3_Dot(out, luminosity);
		const vec3_t intensity = Vec3(d, d, d);
		out = Vec3_Mix(intensity, out, saturation);
		out = ColorNormalize(out);
	}

	return out;
}

/**
 * @brief
 */
const cm_entity_t *EntityTarget(const cm_entity_t *entity) {

	const char *targetname = Cm_EntityValue(entity, "target")->nullable_string;
	if (targetname) {

		cm_entity_t **e = Cm_Bsp()->entities;
		for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
			if (!g_strcmp0(targetname, Cm_EntityValue(*e, "targetname")->nullable_string)) {
				return *e;
			}
		}
	}

	return NULL;
}

/**
 * @brief
 */
static void LightForEntity_worldspawn(const cm_entity_t *entity, light_t *light) {

	const vec3_t ambient = Cm_EntityValue(entity, "ambient")->vec3;
	if (!Vec3_Equal(ambient, Vec3_Zero())) {

		light->type = LIGHT_AMBIENT;
		light->atten = LIGHT_ATTEN_NONE;
		light->color = ambient;
		light->radius = LIGHT_RADIUS_AMBIENT;
		light->bounds = Box3_Null();

		light->color = Vec3_Scale(light->color, ambient_brightness);
	}
}

/**
 * @brief
 */
static void LightForEntity_light_sun(const cm_entity_t *entity, light_t *light) {

	light->type = LIGHT_SUN;
	light->atten = LIGHT_ATTEN_NONE;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->radius = Cm_EntityValue(entity, "light")->value ?: LIGHT_RADIUS;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->bounds = Box3_Null();

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}
	
	light->radius *= sun_brightness;

	light->normal = Vec3_Down();

	const cm_entity_t *target = EntityTarget(entity);
	if (target) {
		light->normal = Vec3_Direction(Cm_EntityValue(target, "origin")->vec3, light->origin);
	} else {
		Mon_SendSelect(MON_WARN, Cm_EntityNumber(entity), 0, "light_sun missing target");
	}

	light->size = Cm_EntityValue(entity, "_size")->value ?: LIGHT_SIZE_SUN;

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light->normal);

	for (int32_t i = 1; i <= ceilf(light->size / LIGHT_SIZE_STEP); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t a = Vec3_Scale(light->normal, LIGHT_SUN_DIST);
			const vec3_t b = Vec3_Scale(cube[j], i * LIGHT_SIZE_STEP);

			const vec3_t dir = Vec3_Normalize(Vec3_Add(a, b));

			g_array_append_val(points, dir);
		}
	}

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);
}

/**
 * @brief
 */
static void LightForEntity_light(const cm_entity_t *entity, light_t *light) {

	light->type = LIGHT_POINT;
	light->atten = LIGHT_ATTEN_LINEAR;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->radius = Cm_EntityValue(entity, "light")->value ?: LIGHT_RADIUS;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->size = Cm_EntityValue(entity, "_size")->value;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	light->radius *= light_brightness;

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	light->bounds = Box3_FromCenter(light->origin);

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light->origin);

	for (int32_t i = 1; i <= ceilf(light->size / LIGHT_SIZE_STEP); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t p = Vec3_Fmaf(light->origin, i * LIGHT_SIZE_STEP, cube[j]);
			if (Light_Trace(light->origin, p, 0, CONTENTS_SOLID).fraction < 1.f) {
				continue;
			}

			g_array_append_val(points, p);

			light->bounds = Box3_Append(light->bounds, p);
		}
	}

	light->bounds = Box3_Expand(light->bounds, light->radius);

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);
}

/**
 * @brief
 */
static void LightForEntity_light_spot(const cm_entity_t *entity, light_t *light) {

	light->type = LIGHT_SPOT;
	light->atten = LIGHT_ATTEN_LINEAR;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->radius = Cm_EntityValue(entity, "light")->value ?: LIGHT_RADIUS;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->size = Cm_EntityValue(entity, "_size")->value;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	light->radius *= light_brightness;

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	light->bounds = Box3_FromCenter(light->origin);

	light->normal = Vec3_Down();

	const cm_entity_t *target = EntityTarget(entity);
	if (target) {
		light->normal = Vec3_Direction(Cm_EntityValue(target, "origin")->vec3, light->origin);
	} else if (Cm_EntityValue(entity, "angle")->parsed & ENTITY_INTEGER) {
		const int32_t angle = Cm_EntityValue(entity, "angle")->integer;
		if (angle == LIGHT_ANGLE_UP) {
			light->normal = Vec3_Up();
		} else if (angle == LIGHT_ANGLE_DOWN) {
			light->normal = Vec3_Down();
		} else {
			Vec3_Vectors(Vec3(0.f, angle, 0.f), &light->normal, NULL, NULL);
		}
	} else {
		Mon_SendSelect(MON_WARN, Cm_EntityNumber(entity), 0,
					   va("light_spot at %s missing target and angle", vtos(light->origin)));
	}

	if (Cm_EntityValue(entity, "_cone")->parsed & ENTITY_FLOAT) {
		light->theta = Cm_EntityValue(entity, "_cone")->value;
	} else {
		light->theta = LIGHT_CONE;
	}

	light->theta = Radians(light->theta);

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light->origin);

	for (int32_t i = 1; i <= ceilf(light->size / LIGHT_SIZE_STEP); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t p = Vec3_Fmaf(light->origin, i * LIGHT_SIZE_STEP, cube[j]);
			if (Light_Trace(light->origin, p, 0, CONTENTS_SOLID).fraction < 1.f) {
				continue;
			}

			light->bounds = Box3_Append(light->bounds, p);

			g_array_append_val(points, p);
		}
	}

	light->bounds = Box3_Expand(light->bounds, light->radius);

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);
}

/**
 * @brief
 */
static void LightForEntity(const cm_entity_t *entity) {

	light_t light = { .type = LIGHT_INVALID };

	const char *class_name = Cm_EntityValue(entity, "classname")->string;
	if (!g_strcmp0(class_name, "worldspawn")) {
		LightForEntity_worldspawn(entity, &light);
	} else if (!g_strcmp0(class_name, "light_sun")) {
		LightForEntity_light_sun(entity, &light);
	} else if (!g_strcmp0(class_name, "light")) {
		LightForEntity_light(entity, &light);
	} else if (!g_strcmp0(class_name, "light_spot")) {
		LightForEntity_light_spot(entity, &light);
	}

	if (light.type != LIGHT_INVALID) {
		g_array_append_val(lights, light);
	}
}

/**
 * @brief
 */
static void LightForPatch(const patch_t *patch) {

	const bsp_brush_side_t *brush_side = &bsp_file.brush_sides[patch->face->brush_side];
	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane];

	light_t light = {
		.type = LIGHT_PATCH,
		.atten = LIGHT_ATTEN_INVERSE_SQUARE,
		.size = sqrtf(Cm_WindingArea(patch->winding)),
		.origin = Vec3_Fmaf(Cm_WindingCenter(patch->winding), 4.f, plane->normal),
		.face = patch->face,
		.plane = plane,
		.normal = plane->normal,
		.theta = LIGHT_CONE,
		.model = patch->model,
	};

	if (Light_PointContents(light.origin, 0) & CONTENTS_SOLID) {
		return;
	}

	light.color = GetMaterialColor(brush_side->material);
	light.color = ColorNormalize(light.color);

	const float max = Maxf(Maxf(light.color.x, light.color.y), light.color.z);
	if (max < 1.0) {
		light.color = Vec3_Scale(light.color, 1.0 / max);
	}

	light.radius = (brush_side->value ?: DEFAULT_LIGHT) * patch_brightness;

	light.bounds = Box3_FromCenter(light.origin);

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light.origin);

	for (int32_t i = 0; i < patch->winding->num_points; i++) {

		const vec3_t p = Vec3_Fmaf(patch->winding->points[i], 4.f, plane->normal);
		if (Light_PointContents(p, 0) & CONTENTS_SOLID) {
			continue;
		}

		light.bounds = Box3_Append(light.bounds, p);

		g_array_append_val(points, p);
	}

	light.bounds = Box3_Expand(light.bounds, light.radius);

	light.points = (vec3_t *) points->data;
	light.num_points = points->len;

	g_array_free(points, false);

	g_array_append_val(lights, light);
}

/**
 * @brief
 */
void FreeLights(void) {

	if (!lights) {
		return;
	}

	for (guint i = 0; i < lights->len; i++) {
		light_t *l = &g_array_index(lights, light_t, i);
		if (l->num_points) {
			g_free(l->points);
		}
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

	g_ptr_array_free(unattenuated_lights, true);
	unattenuated_lights = NULL;
}

/**
 * @return A GPtrArray of all light sources that intersect the specified bounds.
 */
static GPtrArray *BoxLights(const box3_t bounds) {

	GPtrArray *box_lights = g_ptr_array_new();

	const light_t *light = (light_t *) lights->data;
	for (guint i = 0; i < lights->len; i++, light++) {

		if (light->atten != LIGHT_ATTEN_NONE) {
			if (!Box3_Intersects(bounds, light->bounds)) {
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

		node_lights[i] = BoxLights(node->bounds);
	}

	const bsp_leaf_t *leaf = bsp_file.leafs;
	for (int32_t i = 0; i < bsp_file.num_leafs; i++, leaf++) {

		if (leaf->cluster == 0) {
			continue;
		}

		leaf_lights[i] = BoxLights(leaf->bounds);
	}

	unattenuated_lights = g_ptr_array_new();

	for (guint i = 0; i < lights->len; i++) {
		light_t *light = &g_array_index(lights, light_t, i);

		if (light->atten == LIGHT_ATTEN_NONE) {
			g_ptr_array_add(unattenuated_lights, light);
		}
	}
}

/**
 * @brief
 */
void BuildDirectLights(void) {

	FreeLights();

	lights = g_array_new(false, false, sizeof(light_t));

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		LightForEntity(*entity);
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		const bsp_brush_side_t *brush_side = &bsp_file.brush_sides[face->brush_side];
		if (brush_side->surface & SURF_LIGHT) {

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

	const bsp_plane_t *plane = &bsp_file.planes[patch->face->plane];

	light_t light = {
		.type = LIGHT_INDIRECT,
		.atten = LIGHT_ATTEN_INVERSE_SQUARE,
		.size = sqrtf(Cm_WindingArea(patch->winding)),
		.origin = Vec3_Fmaf(Cm_WindingCenter(patch->winding), 4.f, lm->plane->normal),
		.face = patch->face,
		.plane = plane,
		.normal = plane->normal,
		.theta = LIGHT_CONE,
		.model = patch->model,
	};

	if (Light_PointContents(light.origin, 0) & CONTENTS_SOLID) {
		return;
	}

	vec2_t patch_mins = Vec2_Mins();
	vec2_t patch_maxs = Vec2_Maxs();

	for (int32_t i = 0; i < patch->winding->num_points; i++) {
		const vec3_t point = Vec3_Subtract(patch->winding->points[i], patch->origin);
		const vec3_t st = Mat4_Transform(lm->matrix, point);

		patch_mins = Vec2_Minf(patch_mins, Vec3_XY(st));
		patch_maxs = Vec2_Maxf(patch_maxs, Vec3_XY(st));
	}

	assert(patch_mins.x >= lm->st_mins.x - 0.001f);
	assert(patch_mins.y >= lm->st_mins.y - 0.001f);
	assert(patch_maxs.x <= lm->st_maxs.x + 0.001f);
	assert(patch_maxs.y <= lm->st_maxs.y + 0.001f);

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

			lightmap = Vec3_Add(lightmap, indirect_bounce ? l->indirect[indirect_bounce - 1] : l->diffuse);
		}
	}

	if (Vec3_Equal(lightmap, Vec3_Zero())) {
		return;
	}

	lightmap = Vec3_Scale(lightmap, 1.f / (w * h));
	light.radius = Vec3_Length(lightmap) * indirect_brightness;

	lightmap = ColorNormalize(lightmap);
	light.color = Vec3_Multiply(lightmap, GetMaterialColor(lm->brush_side->material));

	light.bounds = Box3_FromCenter(light.origin);

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light.origin);

	for (int32_t i = 0; i < patch->winding->num_points; i++) {

		const vec3_t p = Vec3_Fmaf(patch->winding->points[i], 4.f, lm->plane->normal);
		if (Light_PointContents(p, 0) & CONTENTS_SOLID) {
			continue;
		}

		light.bounds = Box3_Append(light.bounds, p);

		g_array_append_val(points, p);
	}

	light.bounds = Box3_Expand(light.bounds, light.radius);

	light.points = (vec3_t *) points->data;
	light.num_points = points->len;

	g_array_free(points, false);

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

		if (lm->brush_side->surface & (SURF_LIGHT | SURF_MATERIAL | SURF_MASK_NO_LIGHTMAP)) {
			continue;
		}

		for (const patch_t *patch = &patches[i]; patch; patch = patch->next) {
			LightForLightmappedPatch(lm, patch);
		}
	}

	HashLights();

	Com_Verbose("Indirect lighting for %d patches\n", lights->len);
}
