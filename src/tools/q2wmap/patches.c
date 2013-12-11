/*
 * Copyright(c) 1997-2001 id Software, Inc.
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

#include "qlight.h"
#include "image.h"

static vec3_t texture_reflectivity[MAX_BSP_TEXINFO];

/*
 * @brief
 */
void CalcTextureReflectivity(void) {
	char path[MAX_OSPATH];
	int32_t i, j, texels;
	uint32_t color[3];
	SDL_Surface *surf;

	// always set index 0 even if no textures
	VectorSet(texture_reflectivity[0], 0.5, 0.5, 0.5);

	for (i = 0; i < d_bsp.num_texinfo; i++) {

		// see if an earlier texinfo already got the value
		for (j = 0; j < i; j++) {
			if (!g_strcmp0(d_bsp.texinfo[i].texture, d_bsp.texinfo[j].texture)) {
				VectorCopy(texture_reflectivity[j], texture_reflectivity[i]);
				break;
			}
		}

		if (j != i) // earlier texinfo found, continue
			continue;

		// load the image to calculate reflectivity
		g_snprintf(path, sizeof(path), "textures/%s", d_bsp.texinfo[i].texture);

		if (!Img_LoadImage(path, &surf)) {
			Com_Warn("Couldn't load %s\n", path);
			VectorSet(texture_reflectivity[i], 0.5, 0.5, 0.5);
			continue;
		}

		// calculate average color
		texels = surf->w * surf->h;
		color[0] = color[1] = color[2] = 0;

		for (j = 0; j < texels; j++) {
			const byte *pos = (byte *) surf->pixels + j * 4;
			color[0] += *pos++; // r
			color[1] += *pos++; // g
			color[2] += *pos++; // b
		}

		Com_Verbose("Loaded %s (%dx%d)\n", d_bsp.texinfo[i].texture, surf->w, surf->h);

		SDL_FreeSurface(surf);

		for (j = 0; j < 3; j++) {
			const vec_t r = color[j] / texels / 255.0;
			texture_reflectivity[i][j] = r;
		}
	}
}

/*
 * @brief
 */
static inline _Bool HasLight(const d_bsp_face_t *f) {
	const d_bsp_texinfo_t *tex;

	tex = &d_bsp.texinfo[f->texinfo];
	return (tex->flags & SURF_LIGHT) && tex->value;
}

/*
 * @brief
 */
static inline _Bool IsSky(const d_bsp_face_t * f) {
	const d_bsp_texinfo_t *tex;

	tex = &d_bsp.texinfo[f->texinfo];
	return tex->flags & SURF_SKY;
}

/*
 * @brief
 */
static inline void EmissiveLight(patch_t *patch) {

	if (HasLight(patch->face)) {
		const d_bsp_texinfo_t *tex = &d_bsp.texinfo[patch->face->texinfo];
		const vec_t *ref = texture_reflectivity[patch->face->texinfo];

		VectorScale(ref, tex->value, patch->light);
	}
}

/*
 * @brief
 */
static void BuildPatch(int32_t fn, winding_t *w) {
	patch_t *patch;
	d_bsp_plane_t *plane;

	patch = (patch_t *) Mem_Malloc(sizeof(*patch));

	face_patches[fn] = patch;

	patch->face = &d_bsp.faces[fn];
	patch->winding = w;

	// resolve the normal
	plane = &d_bsp.planes[patch->face->plane_num];

	if (patch->face->side)
		VectorNegate(plane->normal, patch->normal);
	else
		VectorCopy(plane->normal, patch->normal);

	WindingCenter(w, patch->origin);

	// nudge the origin out along the normal
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	patch->area = WindingArea(w);

	if (patch->area < 1.0) // clamp area
		patch->area = 1.0;

	EmissiveLight(patch); // surface light
}

/*
 * @brief
 */
static entity_t *EntityForModel(int32_t num) {
	int32_t i;
	char name[16];

	g_snprintf(name, sizeof(name), "*%i", num);

	// search the entities for one using modnum
	for (i = 0; i < num_entities; i++) {

		const char *s = ValueForKey(&entities[i], "model");

		if (!g_strcmp0(s, name))
			return &entities[i];
	}

	return &entities[0];
}

/*
 * @brief Create surface fragments for light-emitting surfaces so that light sources
 * may be computed along them.
 */
void BuildPatches(void) {
	int32_t i, j, k;
	winding_t *w;
	vec3_t origin;

	for (i = 0; i < d_bsp.num_models; i++) {

		const d_bsp_model_t *mod = &d_bsp.models[i];
		const entity_t *ent = EntityForModel(i);

		// bmodels with origin brushes need to be offset into their
		// in-use position
		VectorForKey(ent, "origin", origin);

		for (j = 0; j < mod->num_faces; j++) {

			const int32_t facenum = mod->first_face + j;
			d_bsp_face_t *f = &d_bsp.faces[facenum];

			VectorCopy(origin, face_offset[facenum]);

			if (!HasLight(f)) // no light
				continue;

			w = WindingForFace(f);

			for (k = 0; k < w->num_points; k++) {
				VectorAdd(w->points[k], origin, w->points[k]);
			}

			BuildPatch(facenum, w);
		}
	}
}

#define PATCH_SUBDIVIDE 64

/*
 * @brief
 */
static void FinishSubdividePatch(patch_t *patch, patch_t *newp) {

	VectorCopy(patch->normal, newp->normal);

	VectorCopy(patch->light, newp->light);

	patch->area = WindingArea(patch->winding);

	if (patch->area < 1.0)
		patch->area = 1.0;

	newp->area = WindingArea(newp->winding);

	if (newp->area < 1.0)
		newp->area = 1.0;

	WindingCenter(patch->winding, patch->origin);

	// nudge the patch origin out along the normal
	VectorMA(patch->origin, 2.0, patch->normal, patch->origin);

	WindingCenter(newp->winding, newp->origin);

	// nudge the patch origin out along the normal
	VectorMA(newp->origin, 2.0, newp->normal, newp->origin);
}

/*
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
		if (floor((mins[i] + 1) / PATCH_SUBDIVIDE) < floor((maxs[i] - 1) / PATCH_SUBDIVIDE)) {
			split[i] = 1.0;
			break;
		}
	}

	if (i == 3) // no splitting needed
		return;

	dist = PATCH_SUBDIVIDE * (1 + floor((mins[i] + 1) / PATCH_SUBDIVIDE));
	ClipWindingEpsilon(w, split, dist, ON_EPSILON, &o1, &o2);

	// create a new patch
	newp = (patch_t *) Mem_Malloc(sizeof(*newp));

	newp->next = patch->next;
	patch->next = newp;

	patch->winding = o1;
	newp->winding = o2;

	FinishSubdividePatch(patch, newp);

	SubdividePatch(patch);
	SubdividePatch(newp);
}

/*
 * @brief Iterate all of the head face patches, subdividing them as necessary.
 */
void SubdividePatches(void) {
	int32_t i;

	for (i = 0; i < MAX_BSP_FACES; i++) {

		const d_bsp_face_t *f = &d_bsp.faces[i];
		patch_t *p = face_patches[i];

		if (p && !IsSky(f)) // break it up
			SubdividePatch(p);
	}
}

/*
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
