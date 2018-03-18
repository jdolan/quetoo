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

#define LIGHT_DEFAULT_COLOR (vec3_t) { 1.0, 1.0, 1.0 }

#define LIGHT_SPOT_DEFAULT_CONE 20.0
#define LIGHT_SPOT_ANGLE_UP -1.0
#define LIGHT_SPOT_ANGLE_DOWN -2.0
#define LIGHT_SPOT_DEFAULT_ANGLE LIGHT_SPOT_ANGLE_DOWN

#define LIGHT_SUN_DEFAULT_LIGHT 1.0
#define LIGHT_SUN_DEFAULT_COLOR (vec3_t) { 1.0, 1.0, 1.0 }

light_t *lights[MAX_BSP_LEAFS];
static size_t num_lights;

sun_t suns[MAX_BSP_ENTITIES];
size_t num_suns;

/**
 * @brief
 */
static entity_t *FindTargetEntity(const char *target) {

	for (int32_t i = 0; i < num_entities; i++) {
		const char *name = ValueForKey(&entities[i], "targetname", NULL);
		if (name && !g_ascii_strcasecmp(name, target)) {
			return &entities[i];
		}
	}

	return NULL;
}

/**
 * @brief
 */
static light_t *BuildLight(const vec3_t origin, light_type_t type) {

	light_t *light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);
	num_lights++;

	VectorCopy(origin, light->origin);
	light->type = type;

	const bsp_leaf_t *leaf = &bsp_file.leafs[Light_PointLeafnum(light->origin)];
	const int16_t cluster = leaf->cluster;

	light->next = lights[cluster];
	lights[cluster] = light;

	return light;
}

/**
 * @brief
 */
void BuildDirectLights(void) {

	num_lights = 0;
	memset(lights, 0, sizeof(lights));

	num_suns = 0;
	memset(suns, 0, sizeof(suns));

	// surface lights
	for (size_t i = 0; i < lengthof(face_patches); i++) {

		const patch_t *p = face_patches[i];
		while (p) {

			vec3_t origin;
			WindingCenter(p->winding, origin);

			const bsp_plane_t *plane = &bsp_file.planes[p->face->plane_num];
			if (p->face->side) {
				VectorMA(origin, -4.0, plane->normal, origin);
			} else {
				VectorMA(origin,  4.0, plane->normal, origin);
			}

			light_t *light = BuildLight(origin, LIGHT_SURFACE);

			const bsp_texinfo_t *tex = &bsp_file.texinfo[p->face->texinfo];

			light->radius = tex->value;
			GetTextureColor(tex->texture, light->color);

			p = p->next;
		}
	}

	// entity lights
	for (size_t i = 1; i < num_entities; i++) {

		const entity_t *e = &entities[i];

		const char *classname = ValueForKey(e, "classname", NULL);
		const char *targetname = ValueForKey(e, "target", NULL);

		vec3_t origin;
		VectorForKey(e, "origin", origin, NULL);

		if (!g_strcmp0(classname, "light") || !g_strcmp0(classname, "light_spot")) {

			light_t *light;
			if (!g_strcmp0(classname, "light_spot") || targetname) {
				light = BuildLight(origin, LIGHT_SPOT);
			} else {
				light = BuildLight(origin, LIGHT_POINT);
			}

			light->radius = FloatForKey(e, "light", DEFAULT_LIGHT);

			VectorForKey(e, "_color", light->color, LIGHT_DEFAULT_COLOR);
			ColorNormalize(light->color, light->color);

			if (light->type == LIGHT_SPOT) {
				light->cone = cos(Radians(FloatForKey(e, "_cone", LIGHT_SPOT_DEFAULT_CONE)));

				if (targetname) {
					const entity_t *target = FindTargetEntity(targetname);
					if (target) {
						vec3_t target_origin;
						VectorForKey(target, "origin", target_origin, NULL);
						VectorSubtract(target_origin, light->origin, light->normal);
						VectorNormalize(light->normal);
					} else {
						Mon_SendSelect(MON_WARN, i, 0, va("Spot light at %s missing target", vtos(light->origin)));
						VectorCopy(vec3_down, light->normal);
					}
				} else {
					const vec_t angle = FloatForKey(e, "_angle", LIGHT_SPOT_DEFAULT_ANGLE);
					if (angle == LIGHT_SPOT_ANGLE_UP) {
						VectorCopy(vec3_up, light->normal);
					} else if (angle == LIGHT_SPOT_ANGLE_DOWN) {
						VectorCopy(vec3_down, light->normal);
					} else {
						AngleVectors((const vec3_t) { 0.0, angle, 0.0 }, light->normal, NULL, NULL);
					}
				}

				VectorNegate(light->normal, light->normal);
			}
		}

		if (!g_strcmp0(classname, "light_sun")) {

			sun_t *sun = &suns[num_suns++];

			VectorCopy(origin, sun->origin);

			sun->light = FloatForKey(e, "light", LIGHT_SUN_DEFAULT_LIGHT);

			VectorForKey(e, "_color", sun->color, LIGHT_SUN_DEFAULT_COLOR);
			ColorNormalize(sun->color, sun->color);

			if (targetname) {
				const entity_t *target = FindTargetEntity(targetname);
				if (target) {
					vec3_t target_origin;
					VectorForKey(target, "origin", target_origin, NULL);
					VectorSubtract(target_origin, sun->origin, sun->normal);
					VectorNormalize(sun->normal);
				} else {
					Mon_SendSelect(MON_WARN, i, 0, va("Sun light at %s missing target", vtos(origin)));
					VectorCopy(vec3_down, sun->normal);
				}
			} else {
				Mon_SendSelect(MON_WARN, i, 0, va("Sun light at %s missing target", vtos(origin)));
				VectorCopy(vec3_down, sun->normal);
			}

			VectorNegate(sun->normal, sun->normal);
		}
	}

	Com_Verbose("Lighting %zd lights, %zd suns\n", num_lights, num_suns);
}

/**
 * @brief
 */
void BuildIndirectLights(void) {

	num_lights = 0;
	memset(lights, 0, sizeof(lights));

	for (int32_t face_num = 0; face_num < bsp_file.num_faces; face_num++) {

		const bsp_face_t *face = &bsp_file.faces[face_num];
		const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];

		if (texinfo->flags & (SURF_SKY | SURF_WARP | SURF_LIGHT | SURF_HINT | SURF_NO_DRAW)) {
			continue; // we have no light to reflect
		}

		const face_lighting_t *fl = &face_lighting[face_num];

		for (size_t i = 0; i < fl->num_luxels; i++) {

			const vec_t *direct = fl->direct + i * 3;
			const vec_t *indirect = fl->indirect + i * 3;

			vec3_t color;
			VectorAdd(direct, indirect, color);

			if (VectorCompare(color, vec3_origin)) {
				continue;
			}

			vec3_t origin;
			vec3_t normal;

			VectorCopy(fl->origins + i * 3, origin);
			VectorCopy(fl->normals + i * 3, normal);

			light_t *light = BuildLight(origin, LIGHT_SPOT);

			VectorMA(light->origin, 4.0, normal, light->origin);
			VectorCopy(normal, light->normal);

			VectorScale(color, 1.0 / 255.0 / 256.0 / (indirect_bounce + 1.0), light->color);
			light->radius = 256.0;

			light->cone = 1.0;
		}
	}
}

