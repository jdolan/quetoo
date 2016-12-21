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
#define BSP_LIGHT_SURFACE_RADIUS_SCALE 0.4
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
	GList *e = r_bsp_light_state.lights;
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
		r_bsp_light_state.lights = g_list_prepend(r_bsp_light_state.lights, bl);

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

	char class_name[MAX_QPATH];
	_Bool entity = false, light = false;

	while (true) {

		const char *c = ParseToken(&ents);

		if (!strlen(c)) {
			break;
		}

		if (*c == '{') {
			entity = true;
		}

		if (!entity) { // skip any whitespace between ents
			continue;
		}

		if (*c == '}') {
			entity = false;

			if (light) { // add it
				R_AddBspLight(bsp, origin, color, radius * BSP_LIGHT_POINT_RADIUS_SCALE);

				radius = BSP_LIGHT_POINT_DEFAULT_RADIUS;
				VectorSet(color, 1.0, 1.0, 1.0);

				light = false;
			}
		}

		if (!g_strcmp0(c, "classname")) {

			c = ParseToken(&ents);
			g_strlcpy(class_name, c, sizeof(class_name));

			if (!strncmp(c, "light", 5)) { // light, light_spot, etc..
				light = true;
			}

			continue;
		}

		if (!g_strcmp0(c, "origin")) {
			sscanf(ParseToken(&ents), "%f %f %f", &origin[0], &origin[1], &origin[2]);
			continue;
		}

		if (!g_strcmp0(c, "light")) {
			radius = atof(ParseToken(&ents)) * s->brightness;
			continue;
		}

		if (!g_strcmp0(c, "_color")) {
			sscanf(ParseToken(&ents), "%f %f %f", &color[0], &color[1], &color[2]);
			ColorFilter(color, color, s->brightness, s->saturation, s->contrast);
			continue;
		}
	}

	// allocate the lights array and copy them in
	bsp->num_bsp_lights = g_list_length(r_bsp_light_state.lights);
	bsp->bsp_lights = Mem_LinkMalloc(sizeof(r_bsp_light_t) * bsp->num_bsp_lights, bsp);

	GList *e = r_bsp_light_state.lights;
	r_bsp_light_t *bl = bsp->bsp_lights;
	while (e) {
		*bl++ = *((r_bsp_light_t *) e->data);
		e = e->next;
	}

	// reset state
	g_list_free_full(r_bsp_light_state.lights, Mem_Free);
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


/**
 * @brief
 */
static void R_StainNode(const vec3_t org, const vec4_t color, const vec_t size, const r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	vec_t dist;

	if (node->plane->type < SIDE_BOTH) {
		dist = org[node->plane->type] - node->plane->dist;
	} else {
		dist = DotProduct(org, node->plane->normal) - node->plane->dist;
	}

	if (dist > size) {
		R_StainNode (org, color, size, node->children[0]);
		return;
	} else if (dist < -size) {
		R_StainNode (org, color, size, node->children[1]);
		return;
	}

	const r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;
	const vec_t color_frac = color[3] * r_stain_map->value;

	for (uint32_t c = node->num_surfaces; c; c--, surf++) {

		if (!surf->lightmap || !surf->stainmap) {
			return;
		}

		const r_bsp_texinfo_t	*tex = surf->texinfo;

		if ((tex->flags & (SURF_SKY | SURF_BLEND_33 | SURF_BLEND_66 | SURF_WARP))) {
			continue;
		}

		vec_t fdist = DotProduct(org, surf->plane->normal) - surf->plane->dist;

		if (surf->flags & R_SURF_PLANE_BACK) {
			fdist *= -1;
		}

		const vec_t frad = size - fabs(fdist);
		vec_t fminlight = 0.0;

		if (frad < fminlight) {
			continue;
		}

		fminlight = frad - fminlight;

		vec3_t impact;
		VectorMA(org, -fdist, surf->plane->normal, impact);

		const vec2_t local = {
			DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
			DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
		};

		const r_pixel_t smax = surf->st_extents[0];
		const r_pixel_t tmax = surf->st_extents[1];

		vec_t ftacc = 0.0;

		byte *pfBL = surf->stainmap_buffer;
		_Bool changes = false;

		for (uint16_t t = 0; t < tmax; t++, ftacc += 16.0) {
			const uint32_t td = (uint32_t) fabs(local[1] - ftacc);

			uint16_t s = 0;
			vec_t fsacc = 0.0;

			for (; s < smax; s++, fsacc += 16.0, pfBL += 3) {
				const uint32_t sd = (uint32_t) fabs(local[0] - fsacc);

				if (sd > td) {
					fdist = sd + (td / 2);
				} else {
					fdist = td + (sd / 2);
				}

				if (fdist >= fminlight) {
					continue;
				}

				for (uint32_t i = 0; i < 3; i++) {
					const uint8_t new_color = (uint8_t) (Clamp(Lerp(pfBL[i] / 255.0, color[i], color_frac), 0.0, 1.0) * 255.0);

					if (pfBL[i] != new_color) {
						pfBL[i] = new_color;
						changes = true;
					}
				}
			}
		}

		if (!changes) {
			continue;
		}

		R_BindDiffuseTexture(surf->stainmap->texnum);
		glTexSubImage2D(GL_TEXTURE_2D, 0, surf->lightmap_s, surf->lightmap_t, smax, tmax, GL_RGB, GL_UNSIGNED_BYTE,
		                surf->stainmap_buffer);
		R_GetError("stain");
	}

	R_StainNode(org, color, size, node->children[0]);
	R_StainNode(org, color, size, node->children[1]);
}

/**
 * @brief Add a stain to the map.
 */
void R_AddStain(const vec3_t org, const vec4_t color, const vec_t size) {

	if (!r_stain_map->integer) {
		return;
	}

	R_StainNode(org, color, size, r_model_state.world->bsp->nodes);
}
