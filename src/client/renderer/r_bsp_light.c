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

#define BSP_LIGHT_SUN_SCALE 1.0
#define BSP_LIGHT_MERGE_THRESHOLD 24.0
#define BSP_LIGHT_SURFACE_RADIUS_SCALE 1.0
#define BSP_LIGHT_POINT_RADIUS_SCALE 1.0
#define BSP_LIGHT_POINT_DEFAULT_RADIUS 300.0

r_bsp_light_state_t r_bsp_light_state;

/**
 * @brief Resolves ambient light, brightness, and contrast levels from Worldspawn.
 */
static void R_ResolveBspLightParameters(void) {
	const char *c;
	vec3_t tmp;

	// resolve ambient light
	if ((c = Cm_WorldspawnValue("ambient"))) {
		vec_t *f = r_bsp_light_state.ambient;
		sscanf(c, "%f %f %f", &f[0], &f[1], &f[2]);

		VectorScale(f, r_modulate->value, f);

		Com_Debug(DEBUG_RENDERER, "Resolved ambient: %s\n", vtos(f));
	} else {
		VectorSet(r_bsp_light_state.ambient, 0.15, 0.15, 0.15);
	}

	// resolve sun light
	if ((c = Cm_WorldspawnValue("sun_light"))) {
		r_bsp_light_state.sun.diffuse = atof(c) * BSP_LIGHT_SUN_SCALE;

		Com_Debug(DEBUG_RENDERER, "Resolved sun_light: %1.2f\n", r_bsp_light_state.sun.diffuse);
	} else {
		r_bsp_light_state.sun.diffuse = 0.0;
	}

	// resolve sun angles and direction
	if ((c = Cm_WorldspawnValue("sun_angles"))) {
		VectorClear(tmp);
		sscanf(c, "%f %f", &tmp[0], &tmp[1]);

		Com_Debug(DEBUG_RENDERER, "Resolved sun_angles: %s\n", vtos(tmp));
		AngleVectors(tmp, r_bsp_light_state.sun.dir, NULL, NULL);
	} else {
		VectorCopy(vec3_down, r_bsp_light_state.sun.dir);
	}

	// resolve sun color
	if ((c = Cm_WorldspawnValue("sun_color"))) {
		vec_t *f = r_bsp_light_state.sun.color;
		sscanf(c, "%f %f %f", &f[0], &f[1], &f[2]);

		Com_Debug(DEBUG_RENDERER, "Resolved sun_color: %s\n", vtos(f));
	} else {
		VectorSet(r_bsp_light_state.sun.color, 1.0, 1.0, 1.0);
	}

	// resolve brightness
	if ((c = Cm_WorldspawnValue("brightness"))) {
		r_bsp_light_state.brightness = atof(c);
	} else {
		r_bsp_light_state.brightness = 1.0;
	}

	// resolve saturation
	if ((c = Cm_WorldspawnValue("saturation"))) {
		r_bsp_light_state.saturation = atof(c);
	} else {
		r_bsp_light_state.saturation = 1.0;
	}

	// resolve contrast
	if ((c = Cm_WorldspawnValue("contrast"))) {
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

	ColorFilter(r_bsp_light_state.sun.color, r_bsp_light_state.sun.color, brt, sat, con);
	Com_Debug(DEBUG_RENDERER, "Scaled sun color: %s\n", vtos(r_bsp_light_state.sun.color));
}

/**
 * @brief Adds the specified static light source after first ensuring that it
 * can not be merged with any known sources.
 */
static void R_AddBspLight(r_bsp_model_t *bsp, vec3_t origin, vec3_t color, vec_t radius) {

	if (radius <= 0.0) {
		Com_Debug(DEBUG_RENDERER, "Bad radius: %f\n", radius);
		return;
	}

	if (r_lighting->value) { // scale by r_lighting->value, if enabled
		radius *= r_lighting->value;
	}

	r_bsp_light_t *bl = NULL;
	GSList *e = r_bsp_light_state.lights;
	while (e) {
		vec3_t delta;

		bl = (r_bsp_light_t *) e->data;
		VectorSubtract(origin, bl->light.origin, delta);

		if (VectorLength(delta) <= BSP_LIGHT_MERGE_THRESHOLD) { // merge them
			break;
		}

		bl = NULL;
		e = e->next;
	}

	if (!bl) { // or allocate a new one
		bl = Mem_LinkMalloc(sizeof(*bl), bsp);
		r_bsp_light_state.lights = g_slist_prepend(r_bsp_light_state.lights, bl);

		VectorCopy(origin, bl->light.origin);
		bl->leaf = R_LeafForPoint(bl->light.origin, bsp);
	}

	bl->count++;
	bl->light.radius = ((bl->light.radius * (bl->count - 1)) + radius) / bl->count;

	VectorMix(bl->light.color, color, 1.0 / bl->count, bl->light.color);

	bl->debug.type = PARTICLE_CORONA;
	bl->debug.color[3] = 1.0;
	bl->debug.blend = GL_ONE;
}

/**
 * @brief Parse the entity string and resolve all static light sources. Sources which
 * are very close to each other are merged. Their colors are blended according
 * to their light value (intensity).
 */
void R_LoadBspLights(r_bsp_model_t *bsp) {
	vec3_t origin, color;
	vec_t radius;

	memset(&r_bsp_light_state, 0, sizeof(r_bsp_light_state));

	const r_bsp_light_state_t *s = &r_bsp_light_state;

	R_ResolveBspLightParameters();

	// iterate the world surfaces for surface lights
	const r_bsp_surface_t *surf = bsp->surfaces;

	for (uint16_t i = 0; i < bsp->num_surfaces; i++, surf++) {

		// light-emitting surfaces are of course lights
		if ((surf->texinfo->flags & SURF_LIGHT) && surf->texinfo->value) {
			VectorMA(surf->center, 1.0, surf->normal, origin);

			radius = sqrt(surf->texinfo->light * sqrt(surf->area) * BSP_LIGHT_SURFACE_RADIUS_SCALE);

			R_AddBspLight(bsp, origin, surf->texinfo->emissive, radius * s->brightness);
		}
	}

	// parse the entity string for point lights
	const char *ents = Cm_EntityString();

	VectorClear(origin);

	radius = BSP_LIGHT_POINT_DEFAULT_RADIUS * s->brightness;
	VectorSet(color, 1.0, 1.0, 1.0);

	_Bool entity = false, light = false;
	char token[MAX_BSP_ENTITY_VALUE];
	parser_t parser;

	Parse_Init(&parser, ents, PARSER_NO_COMMENTS);

	while (true) {

		if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
			break;
		}

		if (*token == '{') {
			entity = true;
			continue;
		}

		if (!entity) { // skip any whitespace between ents
			continue;
		}

		if (*token == '}') {
			entity = false;

			if (light) { // add it
				R_AddBspLight(bsp, origin, color, radius * BSP_LIGHT_POINT_RADIUS_SCALE);

				radius = BSP_LIGHT_POINT_DEFAULT_RADIUS;
				VectorSet(color, 1.0, 1.0, 1.0);

				light = false;
			}
		}

		if (!g_strcmp0(token, "classname")) {

			if (!Parse_Token(&parser, PARSE_DEFAULT, token, sizeof(token))) {
				break;
			}

			if (!strncmp(token, "light", 5)) { // light, light_spot, etc..
				light = true;
			}

			continue;
		}

		if (!g_strcmp0(token, "origin")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP | PARSE_WITHIN_QUOTES, PARSE_FLOAT, origin, 3) != 3) {
				break;
			}

			continue;
		}

		if (!g_strcmp0(token, "light")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP | PARSE_WITHIN_QUOTES, PARSE_FLOAT, &radius, 1) != 1) {
				break;
			}

			radius *= s->brightness;
			continue;
		}

		if (!g_strcmp0(token, "_color")) {

			if (Parse_Primitive(&parser, PARSE_DEFAULT | PARSE_NO_WRAP | PARSE_WITHIN_QUOTES, PARSE_FLOAT, color, 3) != 3) {
				break;
			}

			ColorFilter(color, color, s->brightness, s->saturation, s->contrast);
			continue;
		}
	}

	// allocate the lights array and copy them in
	bsp->num_bsp_lights = g_slist_length(r_bsp_light_state.lights);
	bsp->bsp_lights = Mem_LinkMalloc(sizeof(r_bsp_light_t) * bsp->num_bsp_lights, bsp);

	GSList *e = r_bsp_light_state.lights;
	r_bsp_light_t *bl = bsp->bsp_lights;
	while (e) {
		*bl++ = *((r_bsp_light_t *) e->data);
		e = e->next;
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
