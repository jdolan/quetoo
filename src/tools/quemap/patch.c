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

#include "material.h"
#include "patch.h"
#include "qlight.h"

patch_t *patches;

static vec3_t material_colors[MAX_BSP_MATERIALS];

/**
 * @brief
 */
vec3_t GetMaterialColor(int32_t material) {

	vec3_t *color = material_colors + material;
	if (Vec3_Equal(*color, Vec3_Zero())) {

		const char *name = bsp_file.materials[material].name;
		SDL_Surface *surf = LoadDiffusemap(name);
		if (surf) {
			Com_Debug(DEBUG_ALL, "Loaded %s (%dx%d)\n", name, surf->w, surf->h);

			const int32_t texels = surf->w * surf->h;
			uint32_t c[3] = { 0, 0, 0 };

			for (int32_t j = 0; j < texels; j++) {

				const byte *pos = (byte *) surf->pixels + j * 4;

				c[0] += *pos++; // r
				c[1] += *pos++; // g
				c[2] += *pos++; // b
			}

			for (int32_t j = 0; j < 3; j++) {
				color->xyz[j] = (c[j] / texels) / 255.0;
			}
		} else {
			Com_Warn("Couldn't load %s\n", name);
		}
	}

	return *color;
}

/**
 * @brief
 */
static patch_t *BuildPatch(const bsp_face_t *face, const vec3_t origin, cm_winding_t *w) {

	patch_t *patch = &patches[face - bsp_file.faces];

	patch->face = face;
	patch->origin = origin;
	patch->winding = w;

	return patch;
}

/**
 * @brief
 */
static const cm_entity_t *EntityForModel(int32_t num) {

	if (num == 0) {
		return Cm_Bsp()->entities[0];
	}

	char model[16];
	g_snprintf(model, sizeof(model), "*%d", num);

	cm_entity_t **e = Cm_Bsp()->entities;
	for (size_t i = 0; i < Cm_Bsp()->num_entities; i++, e++) {
		if (!g_strcmp0(Cm_EntityValue(*e, "model")->string, model)) {
			return *e;
		}
	}

	Com_Error(ERROR_FATAL, "No entity for inline model %s", model);
}

/**
 * @brief Create surface fragments for emissive lights and radiosity.
 */
void BuildPatches(void) {

	patches = Mem_TagMalloc(bsp_file.num_faces * sizeof(patch_t), MEM_TAG_PATCH);

	for (int32_t i = 0; i < bsp_file.num_models; i++) {

		const bsp_model_t *mod = &bsp_file.models[i];
		const cm_entity_t *ent = EntityForModel(i);

		// inline models need to be offset into their in-use position
		const vec3_t origin = Cm_EntityValue(ent, "origin")->vec3;

		for (int32_t j = 0; j < mod->num_faces; j++) {

			const int32_t face_num = mod->first_face + j;
			const bsp_face_t *face = &bsp_file.faces[face_num];

			cm_winding_t *w = Cm_WindingForFace(&bsp_file, face);

			for (int32_t k = 0; k < w->num_points; k++) {
				w->points[k] = Vec3_Add(w->points[k], origin);
			}

			BuildPatch(face, origin, w);
		}
	}
}

/**
 * @brief
 */
static void SubdividePatch_r(patch_t *patch) {

	const cm_winding_t *w = patch->winding;

	box3_t bounds = Cm_WindingBounds(w);

	vec3_t normal = Vec3_Zero();

	const bsp_brush_side_t *brush_side = &bsp_file.brush_sides[patch->face->brush_side];
	const bsp_material_t *material = &bsp_file.materials[brush_side->material];

	const cm_material_t *cm = LoadMaterial(material->name, ASSET_CONTEXT_TEXTURES);
	const float size = cm->patch_size ?: patch_size;

	int32_t i;
	for (i = 0; i < 3; i++) {
		if (floorf((bounds.mins.xyz[i] + 1.0) / size) < floorf((bounds.maxs.xyz[i] - 1.0) / size)) {
			normal.xyz[i] = 1.0;
			break;
		}
	}

	if (i == 3) {
		return;
	}

	const float dist = size * (1.0 + floorf((bounds.mins.xyz[i] + 1.0) / size));

	cm_winding_t *front, *back;
	Cm_SplitWinding(w, normal, dist, SIDE_EPSILON, &front, &back);

	if (!front || !back) {
		return;
	}

	// create a new patch
	patch_t *p = (patch_t *) Mem_TagMalloc(sizeof(*p), MEM_TAG_PATCH);
	p->face = patch->face;

	p->origin = patch->origin;

	patch->winding = front;
	p->winding = back;

	p->next = patch->next;
	patch->next = p;

	SubdividePatch_r(patch);
	SubdividePatch_r(p);
}

/**
 * @brief Subdivides the given face patch into patch_size patches.
 */
void SubdividePatch(int32_t patch_num) {

	patch_t *patch = &patches[patch_num];

	const bsp_brush_side_t *brush_side = &bsp_file.brush_sides[patch->face->brush_side];

	if (brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	SubdividePatch_r(patch);
}
