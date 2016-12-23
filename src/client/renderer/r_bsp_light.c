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
#include "client.h"

extern cl_client_t cl;

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
 * @brief Resets the stainmaps that we have loaded, for level changes. This is kind of a
 * slow function, so be careful calling this one.
 */
void R_ResetStainMap(void) {

	if (!r_stain_map->integer) {
		return;
	}

	GHashTable *stain_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, Mem_Free);

	for (uint32_t s = 0; s < r_model_state.world->bsp->num_surfaces; s++) {
		r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + s;

		// if this surf was never/can't be stained, don't bother
		if (!surf->stainmap || !surf->stainmap_dirty) {
			continue;
		}

		byte *lightmap = (byte *) g_hash_table_lookup(stain_table, surf->stainmap);

		// load the original lightmap data in scratch buffer
		if (!lightmap) {

			lightmap = Mem_Malloc(surf->lightmap->width * surf->lightmap->height * 3);

			R_BindDiffuseTexture(surf->lightmap->texnum);
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			// copy the lightmap over to the stainmap
			R_BindDiffuseTexture(surf->stainmap->texnum);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf->lightmap->width, surf->lightmap->height, 0, GL_RGB, GL_UNSIGNED_BYTE, lightmap);

			g_hash_table_insert(stain_table, surf->stainmap, lightmap);
		}

		// copy over the old stain buffer
		const size_t stride = surf->lightmap->width * 3;
		const size_t stain_width = surf->st_extents[0] * 3;
		const size_t lightmap_offset = (surf->lightmap_t * surf->lightmap->width + surf->lightmap_s) * 3;

		byte *light = lightmap + lightmap_offset;
		byte *stain = surf->stainmap_buffer;

		for (int16_t t = 0; t < surf->st_extents[1]; t++) {
			
			memcpy(stain, light, stain_width);

			stain += stain_width;
			light += stride;
		}

		surf->stainmap_dirty = false;
	}

	// free all the scratch
	g_hash_table_destroy(stain_table);
}

/**
 * @brief
 */
static void R_StainNode(const vec3_t org, const vec4_t color, vec_t size, r_bsp_node_t *node) {

	if (node->contents != CONTENTS_NODE) {
		return;
	}

	const vec_t dist = DotProduct(org, node->plane->normal) - node->plane->dist;

	if (dist > size) { // front only
		R_StainNode(org, color, size, node->children[0]);
		return;
	}

	if (dist < -size) { // back only
		R_StainNode(org, color, size, node->children[1]);
		return;
	}

	const vec_t src_stain_alpha = color[3] * r_stain_map->value;
	const vec_t dst_stain_alpha = 1.0 - src_stain_alpha;

	r_bsp_surface_t *surf = r_model_state.world->bsp->surfaces + node->first_surface;

	for (uint32_t i = 0; i < node->num_surfaces; i++, surf++) {

		if (!surf->lightmap || !surf->stainmap) {
			continue;
		}

		const r_bsp_texinfo_t *tex = surf->texinfo;

		if ((tex->flags & (SURF_SKY | SURF_BLEND_33 | SURF_BLEND_66 | SURF_WARP))) {
			continue;
		}

		vec_t face_dist = DotProduct(org, surf->plane->normal) - surf->plane->dist;

		if (surf->flags & R_SURF_PLANE_BACK) {
			face_dist *= -1.0;
		}

		const vec_t stain = size - fabs(face_dist);
		if (stain < 0.0) {
			continue;
		}

		vec3_t point;
		VectorMA(org, -face_dist, surf->plane->normal, point);

		const vec2_t point_st = {
			DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->st_mins[0],
			DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->st_mins[1]
		};

		const r_pixel_t smax = surf->st_extents[0];
		const r_pixel_t tmax = surf->st_extents[1];

		byte *buffer = surf->stainmap_buffer;

		vec_t ftacc = 0.0;

		for (uint16_t t = 0; t < tmax; t++, ftacc += 16.0) {
			const uint32_t td = (uint32_t) fabs(point_st[1] - ftacc);

			vec_t fsacc = 0.0; // FIXME: Increment by lightmap_scale?

			for (uint16_t s = 0; s < smax; s++, fsacc += 16.0, buffer += 3) {
				const uint32_t sd = (uint32_t) fabs(point_st[0] - fsacc);

				vec_t sample_dist;
				if (sd > td) {
					sample_dist = sd + (td / 2);
				} else {
					sample_dist = td + (sd / 2);
				}

				if (sample_dist >= stain) {
					continue;
				}

				for (uint32_t c = 0; c < 3; c++) {

					const vec_t blend = ((color[c]) * src_stain_alpha) + ((buffer[c] / 255.0) * dst_stain_alpha);
					const uint8_t new_color = (uint8_t) (Clamp(blend, 0.0, 1.0) * 255.0);

					if (buffer[c] != new_color) {
						buffer[c] = new_color;
						surf->stainmap_dirty = true;
					}
				}
			}
		}

		if (!surf->stainmap_dirty) {
			continue;
		}

		R_BindDiffuseTexture(surf->stainmap->texnum); // FIXME: Diffuse?
		glTexSubImage2D(GL_TEXTURE_2D, 0, surf->lightmap_s, surf->lightmap_t, smax, tmax, GL_RGB, GL_UNSIGNED_BYTE, surf->stainmap_buffer);

		R_GetError(tex->name);
	}

	// recurse down both sides

	R_StainNode(org, color, size, node->children[0]);
	R_StainNode(org, color, size, node->children[1]);
}


		/*
static void R_RotateLightsForBspInlineModel(const r_entity_t *e) {
	static vec3_t light_origins[MAX_LIGHTS];
	static int16_t frame;

	// for each frame, backup the light origins
	if (frame != r_locals.frame) {
		for (uint16_t i = 0; i < r_view.num_lights; i++) {
			VectorCopy(r_view.lights[i].origin, light_origins[i]);
		}
		frame = r_locals.frame;
	}

	// for malformed inline models, simply return
	if (e && e->model->bsp_inline->head_node == -1) {
		return;
	}

	// for well-formed models, iterate the lights, transforming them into model
	// space and marking surfaces, or restoring them if the model is NULL

	const r_bsp_node_t *nodes = r_model_state.world->bsp->nodes;
	r_light_t *l = r_view.lights;

	for (uint16_t i = 0; i < r_view.num_lights; i++, l++) {
		if (e) {
			Matrix4x4_Transform(&e->inverse_matrix, light_origins[i], l->origin);
			R_MarkLight(l, nodes + e->model->bsp_inline->head_node);
		} else {
			VectorCopy(light_origins[i], l->origin);
		}
	}
}
		*/

/**
 * @brief Add a stain to the map.
 */
void R_AddStain(const vec3_t org, const vec4_t color, const vec_t size) {

	if (!r_stain_map->integer) {
		return;
	}

	R_StainNode(org, color, size, r_model_state.world->bsp->nodes);

	for (uint16_t i = 0; i < cl.frame.num_entities; i++) {

		const uint32_t snum = (cl.frame.entity_state + i) & ENTITY_STATE_MASK;
		const entity_state_t *s = &cl.entity_states[snum];

		if (s->solid != SOLID_BSP) {
			continue;
		}

		const cl_entity_t *ent = &cl.entities[s->number];
		const cm_bsp_model_t *mod = cl.cm_models[s->model1];

		if (mod == NULL || mod->head_node == -1) {
			continue;
		}

		vec3_t origin;
		Matrix4x4_Transform(&ent->inverse_matrix, org, origin);

		r_bsp_node_t *node = &r_model_state.world->bsp->nodes[mod->head_node];

		R_StainNode(origin, color, size, node);
	}
}
