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
 * @brief Applies brightness, saturation and contrast to the specified input color.
 */
vec3_t ColorFilter(const vec3_t in) {

	vec3_t out = in;

	if (brightness != 1.f) { // apply brightness
		out = Vec3_Scale(out, brightness);
	}

	if (contrast != 1.f) { // apply contrast
		for (int32_t i = 0; i < 3; i++) {
			out.xyz[i] -= 0.5f; // normalize to -0.5 through 0.5
			out.xyz[i] *= contrast; // scale
			out.xyz[i] += 0.5f;
		}
	}

	if (saturation != 1.f) { // apply saturation
		const vec3_t luminosity = Vec3(0.2125f, 0.7154f, 0.0721f);
		const float d = Vec3_Dot(out, luminosity);
		const vec3_t intensity = Vec3(d, d, d);
		out = Vec3_Mix(intensity, out, saturation);
	}

	return out;
}

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
		light->color = ColorFilter(ambient);
		light->radius = LIGHT_RADIUS_AMBIENT;
		light->intensity = LIGHT_INTENSITY * ambient_intensity;
		light->bounds = Box3_Null();
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
	light->intensity = Cm_EntityValue(entity, "_intensity")->value ?: LIGHT_INTENSITY;
	light->color = Cm_EntityValue(entity, "_color")->vec3;
	light->bounds = Box3_Null();

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	light->color = ColorFilter(light->color);

	light->intensity *= sun_intensity;

	const int32_t num = Cm_EntityNumber(entity);

	if (Cm_EntityValue(entity, "light")->value) {
		light->intensity = Cm_EntityValue(entity, "light")->value / 255.f;
		Mon_SendSelect(MON_WARN, num, 0, "light_sun key light is deprecated. Use _intensity.");
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
	}

	light->normal = Vec3_Down();

	const cm_entity_t *target = EntityTarget(entity);
	if (target) {
		light->normal = Vec3_Direction(Cm_EntityValue(target, "origin")->vec3, light->origin);
	} else {
		Mon_SendSelect(MON_WARN, num, 0, "light_sun missing target");
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
	light->size = Cm_EntityValue(entity, "_size")->value;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	light->color = ColorFilter(light->color);

	light->intensity *= light_intensity;

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
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
	light->size = Cm_EntityValue(entity, "_size")->value;

	if (Vec3_Equal(Vec3_Zero(), light->color)) {
		light->color = LIGHT_COLOR;
	}

	light->color = ColorFilter(light->color);

	light->intensity *= light_intensity;

	if (Cm_EntityValue(entity, "atten")->parsed & ENTITY_INTEGER) {
		light->atten = Cm_EntityValue(entity, "atten")->integer;
	}

	if (Cm_EntityValue(entity, "_shadow")->parsed & ENTITY_FLOAT) {
		light->shadow = Cm_EntityValue(entity, "_shadow")->value;
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
		light->cone = Cm_EntityValue(entity, "_cone")->value;
	} else {
		light->cone = LIGHT_CONE;
	}

	light->theta = cosf(Radians(light->cone));

	if (Cm_EntityValue(entity, "_falloff")->parsed & ENTITY_FLOAT) {
		light->falloff = Cm_EntityValue(entity, "_falloff")->value;
	} else {
		light->falloff = LIGHT_FALLOFF;
	}

	light->phi = cosf(Radians(light->falloff));

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
static light_t *LightForFace(const bsp_face_t *face) {

	const lightmap_t *lm = &lightmaps[face - bsp_file.faces];

	const vec3_t center = Cm_WindingCenter(lm->winding);

	light_t *light = Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);

	light->type = LIGHT_FACE;
	light->atten = lm->material->cm->light.atten;
	light->size = sqrtf(Cm_WindingArea(lm->winding));
	light->origin = Vec3_Fmaf(center, ON_EPSILON, lm->plane->normal);
	light->winding = Cm_CopyWinding(lm->winding);
	light->face = lm->face;
	light->brush_side = lm->brush_side;
	light->plane = lm->plane;
	light->normal = lm->plane->normal;
	light->cone = lm->material->cm->light.cone;
	light->theta = cosf(Radians(light->cone));
	light->falloff = lm->material->cm->light.falloff;
	light->phi = cosf(Radians(light->falloff));
	light->intensity = lm->material->cm->light.intensity;
	light->shadow = lm->material->cm->light.shadow;
	light->model = lm->model;

	light->color = GetMaterialColor(lm->brush_side->material);

	const float max = Vec3_Hmaxf(light->color);
	if (max > 0.f && max < 1.f) {
		light->color = Vec3_Scale(light->color, 1.f / max);
	}

	light->color = ColorFilter(light->color);

	light->intensity *= face_intensity;

	light->radius = lm->brush_side->value ?: lm->material->cm->light.radius;

	light->bounds = Box3_FromCenter(light->origin);

	GArray *points = g_array_new(false, false, sizeof(vec3_t));
	g_array_append_val(points, light->origin);

	for (int32_t i = 0; i < lm->winding->num_points; i++) {

		vec3_t p = lm->winding->points[i];

		p = Vec3_Fmaf(p, ON_EPSILON, Vec3_Direction(p, center));
		p = Vec3_Fmaf(p, ON_EPSILON, lm->plane->normal);

		if (Light_PointContents(p, 0) & CONTENTS_SOLID) {
			continue;
		}

		light->bounds = Box3_Append(light->bounds, p);

		g_array_append_val(points, p);
	}

	light->bounds = Box3_Expand(light->bounds, light->radius);

	light->points = (vec3_t *) points->data;
	light->num_points = points->len;

	g_array_free(points, false);
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

	lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		light_t *light = LightForEntity(*entity);
		if (light) {
			g_ptr_array_add(lights, light);
		}
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		const bsp_brush_side_t *brush_side = &bsp_file.brush_sides[face->brush_side];
		if (brush_side->surface & SURF_LIGHT) {

			light_t *light = LightForFace(face);
			if (light) {
				g_ptr_array_add(lights, light);
			}
		}
	}

	HashLights(~LIGHT_INDIRECT);

	Com_Verbose("Direct lighting for %d lights\n", lights->len);
}

/**
 * @brief Add an indirect light source from a directly lit face.
 */
static light_t *LightForLightmappedFace(const lightmap_t *lm, int32_t s, int32_t t, int32_t size) {

	struct {
		int32_t s, t;
		int32_t w, h;

		box3_t bounds;
		vec3_t diffuse;
	} patch;

	patch.s = s;
	patch.t = t;
	patch.w = Mini(s + size, lm->w) - s;
	patch.h = Mini(t + size, lm->h) - t;

	patch.bounds = Box3_Null();
	patch.diffuse = Vec3_Zero();

	for (int32_t s = patch.s; s < patch.s + patch.w; s++) {
		for (int32_t t = patch.t; t < patch.t + patch.h; t++) {

			const luxel_t *luxel = &lm->luxels[t * lm->w + s];
			patch.bounds = Box3_Append(patch.bounds, luxel->origin);

			const lumen_t *lumen = luxel->diffuse;
			for (int32_t i = 0; i < BSP_LIGHTMAP_CHANNELS; i++, lumen++) {
				patch.diffuse = Vec3_Add(patch.diffuse, lumen->color);
			}
		}
	}

	//patch.diffuse = Vec3_Scale(patch.diffuse, 1.f / (patch.w * patch.h));
	patch.diffuse = Vec3_Multiply(patch.diffuse, GetMaterialColor(lm->brush_side->material));

	if (Vec3_Length(patch.diffuse) < .1f) {
		return NULL;
	}

	light_t *light = Mem_TagMalloc(sizeof(light_t), MEM_TAG_LIGHT);

	light->type = LIGHT_INDIRECT;
	light->atten = DEFAULT_LIGHT_ATTEN;
	light->model = lm->model;
	light->plane = lm->plane;
	light->size = Box3_Distance(patch.bounds);
	light->origin = Box3_Center(patch.bounds);
	light->color = patch.diffuse;
	light->intensity = DEFAULT_LIGHT_INTENSITY;
	light->intensity *= indirect_intensity;

	light->radius = Vec3_Length(light->color) * light->size;
	light->bounds = patch.bounds;

	light->points = g_malloc_n(9, sizeof(vec3_t));
	light->points[0] = light->origin;
	Box3_ToPoints(patch.bounds, &light->points[1]);

	light->bounds = Box3_Expand(patch.bounds, light->radius);

	return light;
}

/**
 * @brief
 */
void BuildIndirectLights(void) {

	lights = lights ?: g_ptr_array_new_with_free_func((GDestroyNotify) FreeLight);

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {

		const lightmap_t *lm = &lightmaps[i];

		if (lm->brush_side->surface & (SURF_LIGHT | SURF_MATERIAL | SURF_MASK_NO_LIGHTMAP)) {
			continue;
		}

		for (int32_t s = 0; s < lm->w; s += /*lm->material->cm->patch_size*/ 8) {
			for (int32_t t = 0; t < lm->h; t += /*lm->material->cm->patch_size*/ 8) {

				light_t *light = LightForLightmappedFace(lm, s, t, 8);
				if (light) {
					//if (light->radius > light->size) {
						printf("radius %g, size: %g, color: %s\n",
							   light->radius,
							   light->size,
							   vtos(light->color));
					//}

					g_ptr_array_add(lights, light);
				}
			}
		}
	}

	HashLights(LIGHT_INDIRECT);

	Com_Verbose("Indirect lighting for %d lit faces\n", lights->len);
}

/**
 * @brief Gather feedback from the lightmaps to determine the _actual_ light bounds. This is
 * used for culling and shadow casting in the renderer, and must be pixel-perfect.
 */
static box3_t LightBounds(const light_t *light) {

	switch (light->type) {
		case LIGHT_AMBIENT:
		case LIGHT_SUN:
			return light->bounds;
		default:
			break;
	}

	box3_t bounds = Box3_FromCenter(light->origin);

	const lightmap_t *lightmap = lightmaps;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, lightmap++) {

		if (!Box3_Intersects(light->bounds, lightmap->node->bounds)) {
			continue;
		}

		const luxel_t *luxel = lightmap->luxels;
		for (size_t j = 0; j < lightmap->num_luxels; j++, luxel++) {

			for (size_t k = 0; k < lengthof(luxel->diffuse); k++) {

				if (luxel->diffuse[k].light == light) {
					bounds = Box3_Append(bounds, luxel->origin);
				}
			}
		}
	}

	const luxel_t *luxel = lg.luxels;
	for (size_t i = 0; i < lg.num_luxels; i++, luxel++) {

		for (size_t k = 0; k < lengthof(luxel->diffuse); k++) {

			if (luxel->diffuse[k].light == light) {
				bounds = Box3_Append(bounds, luxel->origin);
			}
		}
	}

	return Box3_Expand(bounds, luxel_size);
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
			case LIGHT_INDIRECT:
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

		if (light->out) {
			continue;
		}

		switch (light->type) {
			case LIGHT_INDIRECT:
				break;

			case LIGHT_FACE:
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
				out->bounds = LightBounds(light);

				/*for (guint j = i + 1; j < lights->len; j++) {
					light_t *l = g_ptr_array_index(lights, j);
					if (l->type == LIGHT_FACE &&
						l->face->brush_side == light->face->brush_side) {

						out->bounds = Box3_Union(out->bounds, l->bounds);
						out->origin = Box3_Center(out->bounds);
						out->radius = Box3_Radius(out->bounds);
						out->size = Box3_Distance(light->face->bounds);

						l->out = out;
					}
				}*/
				light->out = out++;
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
				out->bounds = LightBounds(light);
				light->out++;
				break;
		}

		Progress("Emitting lights", 100.f * i / lights->len);
	}

	Com_Print("\r%-24s [100%%] %d ms\n\n", "Emitting lights", SDL_GetTicks() - start);
}
