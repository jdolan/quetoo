/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "r_local.h"

/*
 * Static light source loading.
 *
 * Static light sources provide directional diffuse lighting for mesh models.
 * There are several radius scale factors for the types of light sources we
 * will encounter. The goal is to normalize all light source intensities to
 * that of standard "point" lights (light entities in the map editor).
 *
 * Surface light source radius is dependent upon surface area.  For the vast
 * majority of surface lights, this means an aggressive up-scale yields the
 * best results.
 *
 * Sky surfaces are also considered light sources when sun lighting was enabled
 * during the light compilation of the level.  Given that sky surface area is
 * typically quite large, their surface area coefficient actually scales down.
 */

#define BSP_LIGHT_AMBIENT_SCALE 2.0
#define BSP_LIGHT_SURFACE_RADIUS_SCALE 2.0
#define BSP_LIGHT_SURFACE_SKY_RADIUS_SCALE 0.25
#define BSP_LIGHT_POINT_RADIUS_SCALE 1.0
#define BSP_LIGHT_COLOR_COMPONENT_MAX 0.9

/*
 * R_ResolveBspLightParameters
 *
 * Resolves ambient light, brightness, and contrast levels from Worldspawn.
 */
static void R_ResolveBspLightParameters(void) {
	const char *c;

	// resolve ambient light
	if ((c = R_WorldspawnValue("ambient_light"))) {
		sscanf(c, "%f %f %f", &r_locals.ambient_light[0],
				&r_locals.ambient_light[1], &r_locals.ambient_light[2]);

		VectorScale(r_locals.ambient_light, BSP_LIGHT_AMBIENT_SCALE, r_locals.ambient_light);

		Com_Debug("Resolved ambient_light: %1.2f %1.2f %1.2f\n",
				r_locals.ambient_light[0], r_locals.ambient_light[1],
				r_locals.ambient_light[2]);
	} else { // ensure sane default
		VectorSet(r_locals.ambient_light, 0.1, 0.1, 0.1);
	}

	// resolve sun light
	if ((c = R_WorldspawnValue("sun_light"))) {
		r_locals.sun_light = atof(c) / 255.0;

		if (r_locals.sun_light > 1.0) // should never happen
			r_locals.sun_light = 1.0;

		Com_Debug("Resolved sun_light: %1.2f\n", r_locals.sun_light);
	} else {
		r_locals.sun_light = 0.0;
	}

	// resolve sun color
	if ((c = R_WorldspawnValue("sun_color"))) {
		sscanf(c, "%f %f %f", &r_locals.sun_color[0], &r_locals.sun_color[1],
				&r_locals.sun_color[2]);

		Com_Debug("Resolved sun_color: %1.2f %1.2f %1.2f\n",
				r_locals.sun_color[0], r_locals.sun_color[1],
				r_locals.sun_color[2]);
	} else { // ensure sane default
		VectorSet(r_locals.sun_color, 1.0, 1.0, 1.0);
	}

	// resolve brightness
	if ((c = R_WorldspawnValue("brightness"))) {
		r_locals.brightness = atof(c);
	} else { // ensure sane default
		r_locals.brightness = 1.0;
	}

	// resolve saturation
	if ((c = R_WorldspawnValue("saturation"))) {
		r_locals.saturation = atof(c);
	} else { // ensure sane default
		r_locals.saturation = 1.0;
	}

	// resolve contrast
	if ((c = R_WorldspawnValue("contrast"))) {
		r_locals.contrast = atof(c);
	} else { // ensure sane default
		r_locals.contrast = 1.0;
	}

	Com_Debug("Resolved brightness: %1.2f\n", r_locals.brightness);
	Com_Debug("Resolved saturation: %1.2f\n", r_locals.saturation);
	Com_Debug("Resolved contrast: %1.2f\n", r_locals.contrast);

#if 0
	//apply brightness, saturation and contrast to the colors
	ColorFilter(r_locals.ambient_light, r_locals.ambient_light,
			r_locals.brightness, r_locals.saturation, r_locals.contrast);

	Com_Debug("Scaled ambient_light: %1.2f %1.2f %1.2f\n",
			r_locals.ambient_light[0], r_locals.ambient_light[1], r_locals.ambient_light[2]);

	ColorFilter(r_locals.sun_color, r_locals.sun_color,
			r_locals.brightness, r_locals.saturation, r_locals.contrast);

	Com_Debug("Scaled sun_color: %1.2f %1.2f %1.2f\n",
			r_locals.sun_color[0], r_locals.sun_color[1], r_locals.sun_color[2]);
#endif
}

/*
 * R_AddBspLight
 *
 * Adds the specified static light source to the world model, after first
 * ensuring that it can not be merged with any known sources.
 */
static void R_AddBspLight(vec3_t org, float radius, vec3_t color) {
	r_bsp_light_t *l;
	vec3_t delta;
	int i;

	if (radius <= 0.0) {
		Com_Debug("R_AddBspLight: Bad radius: %f\n", radius);
		return;
	}

	l = r_load_model->bsp_lights;
	for (i = 0; i < r_load_model->num_bsp_lights; i++, l++) {

		VectorSubtract(org, l->origin, delta);

		if (VectorLength(delta) <= 32.0) // merge them
			break;
	}

	if (i == r_load_model->num_bsp_lights) { // or allocate a new one

		l = (r_bsp_light_t *) R_HunkAlloc(sizeof(*l));

		if (!r_load_model->bsp_lights) // first source
			r_load_model->bsp_lights = l;

		VectorCopy(org, l->origin);
		l->leaf = R_LeafForPoint(l->origin, r_load_model);

		r_load_model->num_bsp_lights++;
	}

	l->count++;

	l->radius = ((l->radius * (l->count - 1)) + radius) / l->count;

	VectorMix(l->color, color, 1.0 / l->count, l->color);
}

/*
 * R_LoadBspLights
 *
 * Parse the entity string and resolve all static light sources.  Sources which
 * are very close to each other are merged.  Their colors are blended according
 * to their light value (intensity).
 */
void R_LoadBspLights(void) {
	const char *ents;
	char class_name[128];
	vec3_t org, tmp, color;
	float radius;
	boolean_t entity, light;
	r_bsp_surface_t *surf;
	r_bsp_light_t *l;
	int i;

	R_ResolveBspLightParameters();

	// iterate the world surfaces for surface lights
	surf = r_load_model->surfaces;

	for (i = 0; i < r_load_model->num_surfaces; i++, surf++) {
		vec3_t color = { 0.0, 0.0, 0.0 };
		float scale = 1.0;

		// light-emitting surfaces are of course lights
		if ((surf->texinfo->flags & SURF_LIGHT) && surf->texinfo->value) {
			VectorCopy(surf->color, color);
			scale = BSP_LIGHT_SURFACE_RADIUS_SCALE;
		}

		// and so are sky surfaces, if sun light was specified
		else if ((surf->texinfo->flags & SURF_SKY) && r_locals.sun_light) {
			VectorCopy(r_locals.sun_color, color);
			scale = BSP_LIGHT_SURFACE_SKY_RADIUS_SCALE;
		}

		if (!VectorCompare(color, vec3_origin)) {
			VectorMA(surf->center, 1.0, surf->normal, org);

			VectorSubtract(surf->maxs, surf->mins, tmp);
			radius = VectorLength(tmp) * scale;

			R_AddBspLight(org, radius, color);
		}
	}

	// parse the entity string for point lights
	ents = Cm_EntityString();

	VectorClear(org);

	radius = 300.0;
	VectorSet(color, 1.0, 1.0, 1.0);

	memset(class_name, 0, sizeof(class_name));
	entity = light = false;

	while (true) {

		const char *c = ParseToken(&ents);

		if (!strlen(c))
			break;

		if (*c == '{')
			entity = true;

		if (!entity) // skip any whitespace between ents
			continue;

		if (*c == '}') {
			entity = false;

			if (light) { // add it

				radius *= BSP_LIGHT_POINT_RADIUS_SCALE;

				R_AddBspLight(org, radius, color);

				radius = 300.0;
				VectorSet(color, 1.0, 1.0, 1.0);

				light = false;
			}
		}

		if (!strcmp(c, "classname")) {

			c = ParseToken(&ents);
			strncpy(class_name, c, sizeof(class_name) - 1);

			if (!strncmp(c, "light", 5)) // light, light_spot, etc..
				light = true;

			continue;
		}

		if (!strcmp(c, "origin")) {
			sscanf(ParseToken(&ents), "%f %f %f", &org[0], &org[1], &org[2]);
			continue;
		}

		if (!strcmp(c, "light")) {
			radius = atof(ParseToken(&ents));
			continue;
		}

		if (!strcmp(c, "_color")) {
			sscanf(ParseToken(&ents), "%f %f %f", &color[0], &color[1],
					&color[2]);
			continue;
		}
	}

	l = r_load_model->bsp_lights;
	for (i = 0; i < r_load_model->num_bsp_lights; i++, l++) {
		float max = 0.0;
		int j;

		for (j = 0; j < 3; j++) {
			if (l->color[j] > max)
				max = l->color[j];
		}

		if (max > BSP_LIGHT_COLOR_COMPONENT_MAX) {
			VectorScale(l->color, BSP_LIGHT_COLOR_COMPONENT_MAX / max, l->color);
		}
	}

	Com_Debug("Loaded %d bsp lights\n", r_load_model->num_bsp_lights);
}
