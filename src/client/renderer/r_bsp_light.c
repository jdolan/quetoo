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

#include "parse.h"

#include "r_local.h"

/*
 * Static light source loading.
 *
 * Static light sources provide directional diffuse lighting for mesh models.
 * There are several radius scale factors for the types of light sources we
 * will encounter. The goal is to normalize all light source intensities to
 * that of standard "point" lights (light entities in the map editor).
 *
 * Surface light source radius is dependent upon surface area. For the vast
 * majority of surface lights, this means an aggressive up-scale yields the
 * best results.
 *
 * Sky surfaces are also considered light sources when sun lighting was enabled
 * during the light compilation of the level. Given that sky surface area is
 * typically quite large, their surface area coefficient actually scales down.
 */

#define BSP_LIGHT_MERGE_THRESHOLD 8.0

r_bsp_light_state_t r_bsp_light_state;

/**
 * @brief Resolves ambient light, brightness, and contrast levels from Worldspawn.
 */
static void R_ResolveBspLightParameters(void) {
	const char *c;

	// resolve ambient light
	if ((c = Cm_EntityValue(Cm_Worldspawn(), "ambient"))) {
		vec_t *f = r_bsp_light_state.ambient;
		sscanf(c, "%f %f %f", &f[0], &f[1], &f[2]);

		VectorScale(f, r_modulate->value, f);

		Com_Debug(DEBUG_RENDERER, "Resolved ambient: %s\n", vtos(f));
	} else {
		VectorSet(r_bsp_light_state.ambient, 0.15, 0.15, 0.15);
	}

	// resolve brightness
	if ((c = Cm_EntityValue(Cm_Worldspawn(), "brightness"))) {
		r_bsp_light_state.brightness = atof(c);
	} else {
		r_bsp_light_state.brightness = 1.0;
	}

	// resolve saturation
	if ((c = Cm_EntityValue(Cm_Worldspawn(), "saturation"))) {
		r_bsp_light_state.saturation = atof(c);
	} else {
		r_bsp_light_state.saturation = 1.0;
	}

	// resolve contrast
	if ((c = Cm_EntityValue(Cm_Worldspawn(), "contrast"))) {
		r_bsp_light_state.contrast = atof(c);
	} else {
		r_bsp_light_state.contrast = 1.0;
	}

	Com_Debug(DEBUG_RENDERER, "Resolved brightness: %1.2f\n", r_bsp_light_state.brightness);
	Com_Debug(DEBUG_RENDERER, "Resolved saturation: %1.2f\n", r_bsp_light_state.saturation);
	Com_Debug(DEBUG_RENDERER, "Resolved contrast: %1.2f\n", r_bsp_light_state.contrast);

	const vec_t brt = r_bsp_light_state.brightness;
	const vec_t sat = r_bsp_light_state.saturation;
	const vec_t con = r_bsp_light_state.contrast;

	// apply brightness, saturation and contrast to the colors
	ColorFilter(r_bsp_light_state.ambient, r_bsp_light_state.ambient, brt, sat, con);
	Com_Debug(DEBUG_RENDERER, "Scaled ambient: %s\n", vtos(r_bsp_light_state.ambient));
}

/**
 * @brief Adds the specified static light source after first ensuring that it
 * can not be merged with any known sources.
 */
static void R_AddBspLight(r_bsp_model_t *bsp, const r_light_t *l) {

	if (l->radius <= 0.0) {
		Com_Debug(DEBUG_RENDERER, "Bad radius: %f\n", l->radius);
		return;
	}

	r_bsp_light_t *light = Mem_LinkMalloc(sizeof(*light), bsp);
	r_bsp_light_state.lights = g_slist_prepend(r_bsp_light_state.lights, light);

	memcpy(&light->light, l, sizeof(*l));

	light->leaf = R_LeafForPoint(light->light.origin, bsp);

	const r_bsp_light_state_t *s = &r_bsp_light_state;

	ColorFilter(light->light.color, light->light.color, s->brightness, s->saturation, s->contrast);

	light->debug.type = PARTICLE_CORONA;
	light->debug.color[3] = 1.0;
	light->debug.blend = GL_ONE;
}

/**
 * @brief Parse the entity string and resolve all static light sources.
 */
void R_LoadBspLights(r_bsp_model_t *bsp) {

	memset(&r_bsp_light_state, 0, sizeof(r_bsp_light_state));

	R_ResolveBspLightParameters();

	// iterate the world surfaces for surface lights
	const r_bsp_surface_t *surf = bsp->surfaces;

	for (uint16_t i = 0; i < bsp->num_surfaces; i++, surf++) {

		const r_bsp_texinfo_t *tex = surf->texinfo;
		if ((tex->flags & SURF_LIGHT) && tex->value) {

			r_light_t light;

			VectorMA(surf->center, 4.0, surf->normal, light.origin);
			light.radius = tex->value + sqrt(surf->area);
			VectorCopy(tex->material->diffuse->color, light.color);

			R_AddBspLight(bsp, &light);
		}
	}

	// enumerate the entities for point lights, spot lights and suns
	cm_entity_t **entity = Cm_Bsp()->entities;
	for (size_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {

		const char *classname = Cm_EntityValue(*entity, "classname");
		if (!g_strcmp0(classname, "light") ||
			!g_strcmp0(classname, "light_spot") ||
			!g_strcmp0(classname, "light_sun")) {

			r_light_t light;

			parser_t parser;

			Parse_Init(&parser, Cm_EntityValue(*entity, "origin"), PARSER_DEFAULT);
			if (Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, light.origin, 3) != 3) {
				Com_Debug(DEBUG_RENDERER, "Invalid light source\n");
				continue;
			}

			Parse_Init(&parser, Cm_EntityValue(*entity, "_color"), PARSER_DEFAULT);
			if (Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, light.color, 3) != 3) {
				VectorSet(light.color, 1.0, 1.0, 1.0);

			}

			Parse_Init(&parser, Cm_EntityValue(*entity, "light"), PARSER_DEFAULT);
			if (Parse_Primitive(&parser, PARSE_DEFAULT, PARSE_FLOAT, &light.radius, 1) != 1) {
				light.radius = DEFAULT_LIGHT;
			}

			/*
			 TODO: Sun light sources
			const char *targetname = Cm_EntityValue(*entity, "target");
			if (targetname) {
				cm_entity_t *target = NULL, *e = entities;
				for (size_t i = 0; i < num_entities; i++, e++) {
					if (!g_strcmp0(targetname, ValueForKey(e, "targetname", NULL))) {
						target = e;
						break;
					}
				}
				if (target) {
					vec3_t target_origin;
					VectorForKey(target, "origin", target_origin, NULL);
					VectorSubtract(target_origin, light->origin, light->normal);
				} else {
					Mon_SendSelect(MON_WARN, entity - entities, 0,
								   va("Spot light at %s missing target", vtos(light->origin)));
					VectorCopy(vec3_down, light->normal);
				}
			} else {
				if (light->type == LIGHT_SPOT) {
					vec3_t angles = { 0.0, FloatForKey(entity, "_angle", 0.0), 0.0};
					if (angles[YAW] == 0.0) {
						VectorCopy(vec3_down, light->normal);
					} else {
						if (angles[YAW] == LIGHT_ANGLE_UP) {
							VectorCopy(vec3_up, light->normal);
						} else if (angles[YAW] == LIGHT_ANGLE_DOWN) {
							VectorCopy(vec3_down, light->normal);
						} else {
							AngleVectors(angles, light->normal, NULL, NULL);
						}
					}
				} else {
					VectorCopy(vec3_down, light->normal);
				}
			}*/

			R_AddBspLight(bsp, &light);
		}
	}

	// allocate the lights array and copy them in
	bsp->num_bsp_lights = g_slist_length(r_bsp_light_state.lights);
	bsp->bsp_lights = Mem_LinkMalloc(sizeof(r_bsp_light_t) * bsp->num_bsp_lights, bsp);

	r_bsp_light_t *bl = bsp->bsp_lights;
	for (GSList *e = r_bsp_light_state.lights; e; e = e->next) {
		*bl++ = *((r_bsp_light_t *) e->data);
	}

	// reset state
	g_slist_free_full(r_bsp_light_state.lights, Mem_Free);
	r_bsp_light_state.lights = NULL;

	Com_Debug(DEBUG_RENDERER, "Loaded %d bsp lights\n", bsp->num_bsp_lights);
}

/**
 * @brief Developer tool for viewing static BSP light sources.
 */
void R_DrawBspLights(void) {

	if (!r_draw_bsp_lights->value) {
		return;
	}

	r_bsp_light_t *bl = r_model_state.world->bsp->bsp_lights;
	for (uint16_t i = 0; i < r_model_state.world->bsp->num_bsp_lights; i++, bl++) {

		const r_bsp_leaf_t *l = R_LeafForPoint(bl->light.origin, NULL);
		if (l->vis_frame != r_locals.vis_frame) {
			continue;
		}

		VectorCopy(bl->light.origin, bl->debug.org);
		VectorCopy(bl->light.color, bl->debug.color);
		bl->debug.scale = bl->light.radius * r_draw_bsp_lights->value;

		R_AddParticle(&bl->debug);
	}
}
