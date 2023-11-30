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

#include "fog.h"
#include "light.h"
#include "material.h"

GArray *fogs;

/**
 * @brief
 */
_Bool PointInsideFog(const vec3_t point, const fog_t *fog) {

	if (Box3_ContainsPoint(fog->bounds, point)) {

		for (guint i = 0; i < fog->brushes->len; i++) {
			const cm_bsp_brush_t *brush = g_ptr_array_index(fog->brushes, i);
			if (Cm_PointInsideBrush(point, brush)) {
				return true;
			}
		}
	}

	return false;
}

/**
 * @brief
 */
void FreeFog(void) {

	if (!fogs) {
		return;
	}

	g_array_free(fogs, true);
	fogs = NULL;
}

/**
 * @brief Default permutation vector; this comes from the original paper, and looks pretty good.
 */
static const uint8_t simplex_perm[256] = {
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

/**
 * @brief Sets the permutation vector for this fog volume, copying from
 * other fogs if they have the same seed.
 * @param fog The fog to set the vector on
 */
static void FogSetPermutationVector(fog_t *fog) {

	for (guint i = 0; i < fogs->len; i++) {
		const fog_t *f = &g_array_index(fogs, fog_t, i);

		if (fog->seed == f->seed) {
			memcpy(fog->permutation_vector, f->permutation_vector, sizeof(fog->permutation_vector));
			return;
		}
	}

	memcpy(fog->permutation_vector, simplex_perm, sizeof(fog->permutation_vector));

	if (fog->seed) {
		GRand *rand = g_rand_new_with_seed((guint32) fog->seed);
		const size_t len = lengthof(fog->permutation_vector);

		for (guint i = 0; i < len; i++) {
			const int32_t j = g_rand_int_range(rand, i, len);
			const uint8_t o = fog->permutation_vector[i];
			fog->permutation_vector[i] = fog->permutation_vector[j];
			fog->permutation_vector[j] = o;
		}

		g_rand_free(rand);
	}
}

/**
 * @brief
 */
static void FogForEntity(const cm_entity_t *entity) {

	const char *class_name = Cm_EntityValue(entity, "classname")->string;
	if (!g_strcmp0(class_name, "misc_fog")) {

		fog_t fog = {};
		fog.type = FOG_VOLUME;
		fog.entity = entity;

		fog.brushes = Cm_EntityBrushes(entity);
		fog.bounds = Box3_Null();

		int32_t material = -1;

		for (guint i = 0; i < fog.brushes->len; i++) {
			const cm_bsp_brush_t *brush = g_ptr_array_index(fog.brushes, i);
			fog.bounds = Box3_Union(fog.bounds, brush->bounds);

			if (g_strcmp0(brush->brush_sides->material->name, "common/fog")) {
				material = FindMaterial(brush->brush_sides->material->name);
			}
		}

		if (Cm_EntityValue(entity, "absorption")->parsed & ENTITY_FLOAT) {
			fog.absorption = Cm_EntityValue(entity, "absorption")->value;
		} else {
			fog.absorption = FOG_ABSORPTION;
		}

		if (Cm_EntityValue(entity, "_color")->parsed & ENTITY_VEC3) {
			fog.color = Cm_EntityValue(entity, "_color")->vec3;
		} else if (material != -1) {
			fog.color = GetMaterialColor(material);
		} else {
			fog.color = FOG_COLOR;
		}

		fog.density = Cm_EntityValue(entity, "density")->value ?: FOG_DENSITY;
		fog.noise = Cm_EntityValue(entity, "noise")->value ?: FOG_NOISE;
		fog.frequency = Cm_EntityValue(entity, "frequency")->value ?: FOG_FREQUENCY;
		fog.amplitude = Cm_EntityValue(entity, "amplitude")->value ?: FOG_AMPLITUDE;
		fog.lacunarity = Cm_EntityValue(entity, "lacunarity")->value ?: FOG_LACUNARITY;
		fog.persistence = Cm_EntityValue(entity, "persistence")->value ?: FOG_PERSISTENCE;
		fog.octaves = Cm_EntityValue(entity, "octaves")->integer ?: FOG_OCTAVES;
		fog.seed = Cm_EntityValue(entity, "seed")->integer ?: FOG_SEED;
		fog.offset = (Cm_EntityValue(entity, "offset")->parsed & ENTITY_VEC3) ? Cm_EntityValue(entity, "offset")->vec3 : FOG_OFFSET;

		FogSetPermutationVector(&fog);

		g_array_append_val(fogs, fog);
	}
}

/**
 * @brief
 */
void BuildFog(void) {

	FreeFog();

	fogs = g_array_new(false, false, sizeof(fog_t));

	cm_entity_t **entity = Cm_Bsp()->entities;
	for (int32_t i = 0; i < Cm_Bsp()->num_entities; i++, entity++) {
		FogForEntity(*entity);
	}

	Com_Verbose("Fog lighting for %d sources\n", fogs->len);
}
