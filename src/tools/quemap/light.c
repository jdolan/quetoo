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

static GPtrArray *lights = NULL;

GPtrArray *node_lights[MAX_BSP_NODES];
GPtrArray *leaf_lights[MAX_BSP_LEAFS];

/**
 * @brief
 */
static light_t *AllocLight(void) {
	return Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);
}

/**
 * @brief
 */
static void FreeLight(light_t *light) {

	if (light->winding) {
		Cm_FreeWinding(light->winding);
	}
	
	if (light->points) {
		g_free(light->points);
	}

	Mem_Free(light);
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
static light_t *LightForEntity_worldspawn(const cm_entity_t *entity) {

	light_t *light = NULL;

	const vec3_t ambient = Cm_EntityValue(entity, "ambient")->vec3;
	if (!Vec3_Equal(ambient, Vec3_Zero())) {

		light = AllocLight();

		light->type = LIGHT_AMBIENT;
		light->atten = LIGHT_ATTEN_NONE;
		light->color = ambient;
		light->radius = LIGHT_AMBIENT_RADIUS;
		light->intensity = LIGHT_INTENSITY;
		light->bounds = Box3_Null();
		light->visible_bounds = Box3_Null();
	}

	return light;
}

/**
 * @brief
 */
static light_t *LightForEntity_light_sun(const cm_entity_t *entity) {

	light_t *light = AllocLight();

	light->type = LIGHT_SUN;
	light->atten = LIGHT_ATTEN_NONE;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->intensity = Cm_EntityValue(entity, "_intensity")->value ?: LIGHT_INTENSITY;
	light->bounds = Box3_Null();
	light->visible_bounds = Box3_Null();

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
	}

	light->normal = Vec3_Down();

	const int32_t num = Cm_EntityNumber(entity);
	const cm_entity_t *target = EntityTarget(entity);
	if (target) {
		light->normal = Vec3_Direction(Cm_EntityValue(target, "origin")->vec3, light->origin);
	} else {
		Mon_SendSelect(MON_WARN, num, 0, "light_sun missing target");
	}

	light->size = Cm_EntityValue(entity, "_size")->value ?: LIGHT_SUN_SIZE;

	GArray *points = g_array_new(false, false, sizeof(vec3_t));

	for (int32_t i = 1; i <= ceilf(light->size / BSP_LIGHTMAP_LUXEL_SIZE); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t a = Vec3_Scale(light->normal, LIGHT_SUN_DIST);
			const vec3_t b = Vec3_Scale(cube[j], i * BSP_LIGHTMAP_LUXEL_SIZE);

			const vec3_t dir = Vec3_Normalize(Vec3_Add(a, b));

			g_array_append_val(points, dir);
		}
	}

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);
	return light;
}

/**
 * @brief
 */
static light_t *LightForEntity_light(const cm_entity_t *entity) {

	light_t *light = AllocLight();

	light->type = LIGHT_POINT;
	light->atten = LIGHT_ATTEN_LINEAR;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->radius = Cm_EntityValue(entity, "light")->value ?: LIGHT_RADIUS;
	light->intensity = Cm_EntityValue(entity, "_intensity")->value ?: LIGHT_INTENSITY;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->size = Cm_EntityValue(entity, "_size")->value ?: LIGHT_SIZE;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
	}

	light->bounds = Box3_FromCenter(light->origin);
	light->visible_bounds = Box3_Null();

	GArray *points = g_array_new(false, false, sizeof(vec3_t));

	for (int32_t i = 1; i <= ceilf(light->size / BSP_LIGHTMAP_LUXEL_SIZE); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t p = Vec3_Fmaf(light->origin, i * BSP_LIGHTMAP_LUXEL_SIZE, cube[j]);
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

	if (light->num_points == 0) {
		Mon_SendPoint(MON_WARN, light->origin, "Light in solid");
		FreeLight(light);
		return NULL;
	}

	return light;
}

/**
 * @brief
 */
static light_t *LightForEntity_light_spot(const cm_entity_t *entity) {

	light_t *light = AllocLight();

	light->type = LIGHT_SPOT;
	light->atten = LIGHT_ATTEN_LINEAR;
	light->origin = Cm_EntityValue(entity, "origin")->vec3;
	light->radius = Cm_EntityValue(entity, "light")->value ?: LIGHT_RADIUS;
	light->intensity = Cm_EntityValue(entity, "_intensity")->value ?: LIGHT_INTENSITY;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->size = Cm_EntityValue(entity, "_size")->value ?: LIGHT_SIZE;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
	}

	light->bounds = Box3_FromCenter(light->origin);
	light->visible_bounds = Box3_Null();

	light->normal = Vec3_Down();

	const cm_entity_t *target = EntityTarget(entity);
	if (target) {
		light->normal = Vec3_Direction(Cm_EntityValue(target, "origin")->vec3, light->origin);
	} else if (Cm_EntityValue(entity, "angle")->parsed & ENTITY_INTEGER) {
		const int32_t angle = Cm_EntityValue(entity, "angle")->integer;
		if (angle == LIGHT_SPOT_ANGLE_UP) {
			light->normal = Vec3_Up();
		} else if (angle == LIGHT_SPOT_ANGLE_DOWN) {
			light->normal = Vec3_Down();
		} else {
			Vec3_Vectors(Vec3(0.f, angle, 0.f), &light->normal, NULL, NULL);
		}
	} else {
		Mon_SendSelect(MON_WARN, Cm_EntityNumber(entity), 0,
					   va("light_spot at %s missing target and angle", vtos(light->origin)));
	}

	if (Cm_EntityValue(entity, "_cone")->parsed & ENTITY_FLOAT) {
		light->cone = Cm_EntityValue(entity, "_cone")->value;
	} else {
		light->cone = LIGHT_SPOT_CONE;
	}

	light->theta = cosf(Radians(light->cone));

	if (Cm_EntityValue(entity, "_falloff")->parsed & ENTITY_FLOAT) {
		light->falloff = Cm_EntityValue(entity, "_falloff")->value;
	} else {
		light->falloff = LIGHT_SPOT_FALLOFF;
	}

	light->phi = cosf(Radians(light->cone + light->falloff));

	GArray *points = g_array_new(false, false, sizeof(vec3_t));

	for (int32_t i = 1; i <= ceilf(light->size / BSP_LIGHTMAP_LUXEL_SIZE); i++) {

		const vec3_t cube[] = CUBE_8;
		for (size_t j = 0; j < lengthof(cube); j++) {

			const vec3_t p = Vec3_Fmaf(light->origin, i * BSP_LIGHTMAP_LUXEL_SIZE, cube[j]);
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

	if (light->num_points == 0) {
		Mon_SendPoint(MON_WARN, light->origin, "Light in solid");
		FreeLight(light);
		return NULL;
	}

	g_array_free(points, false);
	return light;
}

/**
 * @brief
 */
static light_t *LightForEntity(const cm_entity_t *entity) {

	const char *class_name = Cm_EntityValue(entity, "classname")->string;
	if (!g_strcmp0(class_name, "worldspawn")) {
		return LightForEntity_worldspawn(entity);
	} else if (!g_strcmp0(class_name, "light_sun")) {
		return LightForEntity_light_sun(entity);
	} else if (!g_strcmp0(class_name, "light")) {
		return LightForEntity_light(entity);
	} else if (!g_strcmp0(class_name, "light_spot")) {
		return LightForEntity_light_spot(entity);
	} else {
		return NULL;
	}
}

/**
 * @brief
 */
static light_t *LightForBrushSide(const bsp_brush_side_t *brush_side, int32_t side) {

	light_t *light = Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);

	light->type = LIGHT_BRUSH_SIDE;
	light->brush_side = brush_side;

	const bsp_plane_t *plane = &bsp_file.planes[brush_side->plane + side];
	const material_t *material = &materials[brush_side->material];

	light->atten = material->cm->light.atten;
	light->radius = brush_side->value ?: material->cm->light.radius;
	light->winding = Cm_WindingForBrushSide(&bsp_file, brush_side);
	light->plane = plane;
	light->normal = plane->normal;
	light->cone = material->cm->light.cone;
	light->theta = cosf(Radians(light->cone));
	light->falloff = material->cm->light.falloff;
	light->phi = cosf(Radians(light->cone + light->falloff));
	light->color = material->diffuse;
	light->intensity = material->cm->light.intensity;
	light->shadow = material->cm->light.shadow;

	assert(light->phi <= light->theta);

	light->bounds = Box3_Null();
	light->visible_bounds = Box3_Null();

	GArray *points = g_array_new(false, false, sizeof(vec3_t));

	const lightmap_t *lm = lightmaps;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, lm++) {

		if (lm->brush_side == light->brush_side && lm->plane == light->plane) {

			light->model = lm->model;

			for (size_t j = 0; j < lm->num_luxels; j++) {

				if (Light_PointContents(lm->luxels[j].origin, 0) & CONTENTS_SOLID) {
					continue;
				}

				light->bounds = Box3_Append(light->bounds, lm->luxels[j].origin);
				g_array_append_val(points, lm->luxels[j].origin);
			}
		}
	}

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);

	if (light->num_points == 0) {
		FreeLight(light);
		return NULL;
	}

	light->origin = Box3_Center(light->bounds);
	light->size = Box3_Distance(light->bounds);

	light->bounds = Box3_Expand(light->bounds, light->radius);
	return light;
}

/**
 * @brief
 */
void FreeLights(void) {

	if (!lights) {
		return;
	}

	g_ptr_array_free(lights, true);
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
 * @return A GPtrArray of all light sources of the specified types intersecting the given bounds.
 */
static GPtrArray *BoxLights(light_type_t type, const box3_t bounds) {

	GPtrArray *box_lights = g_ptr_array_new();

	for (guint i = 0; i < lights->len; i++) {
		light_t *light = g_ptr_array_index(lights, i);

		if (!(light->type & type)) {
			continue;
		}

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
 * @brief Hashes light sources of the specified type(s) into bins by node and leaf.
 */
static void HashLights(light_type_t type) {

	assert(lights);

	const bsp_node_t *node = bsp_file.nodes;
	for (int32_t i = 0; i < bsp_file.num_nodes; i++, node++) {

		if (node->num_faces == 0) {
			continue;
		}

		if (node_lights[i]) {
			g_ptr_array_free(node_lights[i], true);
		}

		node_lights[i] = BoxLights(type, Box3_Expand(node->visible_bounds, BSP_LIGHTMAP_LUXEL_SIZE));
	}

	const bsp_leaf_t *leaf = bsp_file.leafs;
	for (int32_t i = 0; i < bsp_file.num_leafs; i++, leaf++) {

		if (leaf->contents & CONTENTS_SOLID) {
			continue;
		}

		if (leaf_lights[i]) {
			g_ptr_array_free(leaf_lights[i], true);
		}

		leaf_lights[i] = BoxLights(type, Box3_Expand(leaf->visible_bounds, BSP_LIGHTMAP_LUXEL_SIZE));
	}
}

/**
 * @brief
 */
void BuildDirectLights(void) {

	const uint32_t start = SDL_GetTicks();

	Progress("Building direct lights", 0);

	lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

	const int32_t count = Cm_Bsp()->num_entities + Cm_Bsp()->num_brush_sides;

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		light_t *light = LightForEntity(*entity);
		if (light) {
			g_ptr_array_add(lights, light);
		}
		Progress("Building direct lights", i * 100.f / count);
	}

	const bsp_brush_side_t *side = bsp_file.brush_sides;
	for (int32_t i = 0; i < bsp_file.num_brush_sides; i++, side++) {

		if (side->surface & SURF_LIGHT) {

			for (int32_t j = 0; j < 2; j++) {
				light_t *light = LightForBrushSide(side, j);
				if (light) {
					g_ptr_array_add(lights, light);
				}
			}

			Progress("Building direct lights", i * 100.f / count);
		}
	}

	HashLights(~LIGHT_PATCH);

	Com_Print("\r%-24s [100%%] %d ms\n", "Building direct lights", SDL_GetTicks() - start);

	Com_Verbose("Direct lighting for %d lights\n", lights->len);
}

/**
 * @brief Add a patch light source from a directly lit lightmap.
 */
static light_t *LightForPatch(const lightmap_t *lm, int32_t s, int32_t t) {

	const int32_t w = Mini(s + LIGHT_PATCH_SIZE, lm->w) - s;
	const int32_t h = Mini(t + LIGHT_PATCH_SIZE, lm->h) - t;

	assert(w);
	assert(h);

	box3_t bounds = Box3_Null();
	vec3_t diffuse = Vec3_Zero();

	for (int32_t i = s; i < s + w; i++) {
		for (int32_t j = t; j < t + h; j++) {

			const luxel_t *luxel = &lm->luxels[j * lm->w + i];

			if (Light_PointContents(luxel->origin, lm->model->head_node) & CONTENTS_SOLID) {
				continue;
			}

			bounds = Box3_Append(bounds, luxel->origin);
			diffuse = Vec3_Add(diffuse, luxel->diffuse);
		}
	}

	diffuse = Vec3_Scale(diffuse, 1.f / (w * h * BSP_LIGHTMAP_LUXEL_SIZE * BSP_LIGHTMAP_LUXEL_SIZE));
	diffuse = Vec3_Multiply(diffuse, lm->material->ambient);

	if (Vec3_Hmaxf(diffuse) == 0.f) {
		return NULL;
	}

	light_t *light = Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);

	light->type = LIGHT_PATCH;
	light->atten = LIGHT_ATTEN_LINEAR;
	light->origin = Box3_Center(bounds);
	light->radius = w * h;
	light->size = Maxi(w, h);
	light->bounds = Box3_Expand(bounds, light->radius);
	light->visible_bounds = Box3_Null();
	light->color = diffuse;
	light->model = lm->model;
	light->brush_side = lm->brush_side;
	light->plane = lm->plane;
	light->intensity = LIGHT_INTENSITY;

	vec3_t points[8];
	Box3_ToPoints(bounds, points);

	light->points = g_malloc_n(8, sizeof(vec3_t));

	for (size_t i = 0; i < lengthof(points); i++) {

		bool unique = true;

		for (int32_t j = 0; j < light->num_points; j++) {
			if (Vec3_Equal(light->points[j], points[i])) {
				unique = false;
			}
		}

		if (unique) {
			light->points[light->num_points++] = points[i];
		}
	}

	return light;
}

/**
 * @brief
 */
void BuildIndirectLights(void) {

	const uint32_t start = SDL_GetTicks();

	Progress("Building indirect lights", 0);

	lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {

		const lightmap_t *lm = &lightmaps[i];

		if (lm->brush_side->surface & (SURF_LIGHT | SURF_MATERIAL | SURF_MASK_NO_LIGHTMAP)) {
			continue;
		}

		for (int32_t s = 0; s < lm->w; s += LIGHT_PATCH_SIZE) {
			for (int32_t t = 0; t < lm->h; t += LIGHT_PATCH_SIZE) {

				light_t *light = LightForPatch(lm, s, t);
				if (light) {
					Com_Debug(DEBUG_ALL, "Patch radius %g, size: %g, color: %s\n",
						   light->radius,
						   light->size,
						   vtos(light->color));

					g_ptr_array_add(lights, light);
				}
			}
		}

		Progress("Building indirect lights", i * 100.f / bsp_file.num_faces);
	}

	HashLights(LIGHT_PATCH);

	Com_Print("\r%-24s [100%%] %d ms\n", "Building indirect lights", SDL_GetTicks() - start);

	Com_Verbose("Indirect lighting for %d lit patches\n", lights->len);
}

/**
 * @brief
 */
void EmitLights(void) {

	const uint32_t start = SDL_GetTicks();

	bsp_file.num_lights = 0;

	for (guint i = 0; i < lights->len; i++) {
		const light_t *light = g_ptr_array_index(lights, i);
		switch (light->type) {
			case LIGHT_PATCH:
				break;
			default:
				bsp_file.num_lights++;
				break;
		}
	}

	if (bsp_file.num_lights >= MAX_BSP_LIGHTS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTS\n");
	}

	Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTS, bsp_file.num_lights);

	bsp_light_t *out = bsp_file.lights;
	for (guint i = 0; i < lights->len; i++) {

		light_t *light = g_ptr_array_index(lights, i);

		switch (light->type) {
			case LIGHT_PATCH:
				break;

			default:
				out->type = light->type;
				out->atten = light->atten;
				out->origin = light->origin;
				out->color = light->color;
				out->normal = light->normal;
				out->radius = light->radius;
				out->size = light->size;
				out->intensity = light->intensity;
				out->shadow = light->shadow;
				out->cone = light->cone;
				out->falloff = light->falloff;
				out->bounds = light->visible_bounds;
				out->bounds = Box3_Expand(out->bounds, 1.f);
				out++;
				break;
		}

		Progress("Emitting lights", 100.f * i / lights->len);
	}

	Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting lights", SDL_GetTicks() - start);
}
