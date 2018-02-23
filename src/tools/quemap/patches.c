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

#include "qlight.h"
#include "image.h"

static GHashTable *texture_colors;

/**
 * @brief
 */
void BuildTextureColors(void) {

	texture_colors = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, Mem_Free);

	for (int32_t i = 0; i < bsp_file.num_texinfo; i++) {

		const bsp_texinfo_t *tex = &bsp_file.texinfo[i];

		if (g_hash_table_contains(texture_colors, tex->texture)) {
			continue;
		}

		const cm_material_t *material = LoadMaterial(tex->texture, ASSET_CONTEXT_TEXTURES);

		vec_t *color = Mem_Malloc(sizeof(vec3_t));
		VectorSet(color, 1.0, 1.0, 1.0);

		SDL_Surface *surf;
		if (Img_LoadImage(material->diffuse.path, &surf)) {
			Com_Debug(DEBUG_ALL, "Loaded %s (%dx%d)\n", tex->texture, surf->w, surf->h);

			const int32_t texels = surf->w * surf->h;
			uint32_t c[3] = { 0, 0, 0 };

			for (int32_t j = 0; j < texels; j++) {

				const byte *pos = (byte *) surf->pixels + j * 4;

				c[0] += *pos++; // r
				c[1] += *pos++; // g
				c[2] += *pos++; // b
			}

			SDL_FreeSurface(surf);

			for (int32_t j = 0; j < 3; j++) {
				color[j] = (c[j] / texels) / 255.0;
			}
		} else {
			Com_Warn("Couldn't load %s\n", material->diffuse.path);
		}

		g_hash_table_insert(texture_colors, (gpointer) tex->texture, color);
	}
}

/**
 * @brief
 */
void GetTextureColor(const char *name, vec3_t color) {

	const vec_t *data = g_hash_table_lookup(texture_colors, name);
	if (data) {
		VectorCopy(data, color);
	} else {
		VectorSet(color, 1.0, 1.0, 1.0);
	}
}

/**
 * @brief Free the color hash table
 */
void FreeTextureColors(void) {
	g_hash_table_destroy(texture_colors);
}

/**
 * @brief
 */
static _Bool HasLight(const bsp_face_t *f) {
	const bsp_texinfo_t *tex;

	tex = &bsp_file.texinfo[f->texinfo];
	return (tex->flags & SURF_LIGHT) && tex->value;
}

/**
 * @brief
 */
static _Bool IsSky(const bsp_face_t *f) {
	const bsp_texinfo_t *tex;

	tex = &bsp_file.texinfo[f->texinfo];
	return tex->flags & SURF_SKY;
}

/**
 * @brief
 */
static void EmissiveLight(patch_t *patch) {

	if (HasLight(patch->face)) {
		vec3_t color;

		const bsp_texinfo_t *tex = &bsp_file.texinfo[patch->face->texinfo];
		GetTextureColor(tex->texture, color);

		VectorScale(color, tex->value, patch->light);
	}
}

/**
 * @brief
 */
static void BuildPatch(int32_t fn, winding_t *w) {
	patch_t *patch;
	bsp_plane_t *plane;

	patch = (patch_t *) Mem_TagMalloc(sizeof(*patch), MEM_TAG_PATCH);

	face_patches[fn] = patch;

	patch->face = &bsp_file.faces[fn];
	patch->winding = w;

	// resolve the normal
	plane = &bsp_file.planes[patch->face->plane_num];

	if (patch->face->side) {
		VectorNegate(plane->normal, patch->normal);
	} else {
		VectorCopy(plane->normal, patch->normal);
	}

	WindingCenter(w, patch->origin);

	// nudge the origin out along the normal
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	patch->area = WindingArea(w);

	if (patch->area < 1.0) { // clamp area
		patch->area = 1.0;
	}

	EmissiveLight(patch); // surface light
}

/**
 * @brief
 */
static entity_t *EntityForModel(int32_t num) {
	int32_t i;
	char name[16];

	g_snprintf(name, sizeof(name), "*%i", num);

	// search the entities for one using modnum
	for (i = 0; i < num_entities; i++) {

		const char *s = ValueForKey(&entities[i], "model");

		if (!g_strcmp0(s, name)) {
			return &entities[i];
		}
	}

	return &entities[0];
}

/**
 * @brief Create surface fragments for light-emitting surfaces so that light sources
 * may be computed along them.
 */
void BuildPatches(void) {
	int32_t i, j, k;
	winding_t *w;
	vec3_t origin;

	for (i = 0; i < bsp_file.num_models; i++) {

		const bsp_model_t *mod = &bsp_file.models[i];
		const entity_t *ent = EntityForModel(i);

		// bmodels with origin brushes need to be offset into their
		// in-use position
		VectorForKey(ent, "origin", origin);

		for (j = 0; j < mod->num_faces; j++) {

			const int32_t facenum = mod->first_face + j;
			bsp_face_t *f = &bsp_file.faces[facenum];

			VectorCopy(origin, face_offset[facenum]);

			if (!HasLight(f)) { // no light
				continue;
			}

			w = WindingForFace(f);

			for (k = 0; k < w->num_points; k++) {
				VectorAdd(w->points[k], origin, w->points[k]);
			}

			BuildPatch(facenum, w);
		}
	}
}

/**
 * @brief
 */
static void FinishSubdividePatch(patch_t *patch, patch_t *newp) {

	VectorCopy(patch->normal, newp->normal);

	VectorCopy(patch->light, newp->light);

	patch->area = WindingArea(patch->winding);

	if (patch->area < 1.0) {
		patch->area = 1.0;
	}

	newp->area = WindingArea(newp->winding);

	if (newp->area < 1.0) {
		newp->area = 1.0;
	}

	WindingCenter(patch->winding, patch->origin);

	// nudge the patch origin out along the normal
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	WindingCenter(newp->winding, newp->origin);

	// nudge the patch origin out along the normal
	VectorMA(newp->origin, 2.0, newp->normal, newp->origin);
}

/**
 * @brief
 */
static void SubdividePatch(patch_t *patch) {
	winding_t *w, *o1, *o2;
	vec3_t mins, maxs;
	vec3_t split;
	vec_t dist;
	int32_t i;
	patch_t *newp;

	w = patch->winding;
	WindingBounds(w, mins, maxs);

	VectorClear(split);

	for (i = 0; i < 3; i++) {
		if (floor((mins[i] + 1) / patch_size) < floor((maxs[i] - 1) / patch_size)) {
			split[i] = 1.0;
			break;
		}
	}

	if (i == 3) { // no splitting needed
		return;
	}

	dist = patch_size * (1 + floor((mins[i] + 1) / patch_size));
	ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

	// create a new patch
	newp = (patch_t *) Mem_TagMalloc(sizeof(*newp), MEM_TAG_PATCH);

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSubdividePatch(patch, newp);

	SubdividePatch(patch);
	SubdividePatch(newp);
}

/**
 * @brief Iterate all of the head face patches, subdividing them as necessary.
 */
void SubdividePatches(void) {

	// patch_size may come from worldspawn
	const vec_t ps = FloatForKey(entities, "patch_size");
	if (ps > 0.0 && (int32_t) patch_size == (int32_t) PATCH_SUBDIVIDE) {
		patch_size = ps;
	}

	for (int32_t i = 0; i < MAX_BSP_FACES; i++) {

		const bsp_face_t *f = &bsp_file.faces[i];
		patch_t *p = face_patches[i];

		if (p && !IsSky(f)) { // break it up
			SubdividePatch(p);
		}
	}
}

/**
 * @brief After light sources have been created, patches may be freed.
 */
void FreePatches(void) {
	int32_t i;

	for (i = 0; i < MAX_BSP_FACES; i++) {

		patch_t *p = face_patches[i];

		while (p) {
			patch_t *pnext = p->next;
			Mem_Free(p);
			p = pnext;
		}
	}
}
