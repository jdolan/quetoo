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

#include "luxel.h"
#include "qlight.h"

/**
 * @brief
 */
void Luxel_LightLumen(const light_t *light, luxel_t *luxel, const vec3_t direction, float lumens) {

	if (luxel->lumens == NULL) {
		luxel->lumens = g_array_new(false, true, sizeof(lumen_t));
	} else {
		for (guint i = 0; i < luxel->lumens->len; i++) {
			lumen_t *lumen = &g_array_index(luxel->lumens, lumen_t, i);

			if (lumen->light_id == light->id) {
				lumen->diffuse = Vec3_Fmaf(lumen->diffuse, lumens, light->color);
				lumen->direction = Vec3_Fmaf(lumen->direction, lumens, direction);
				return;
			}
		}
	}

	const lumen_t lumen = (lumen_t) {
		.diffuse = Vec3_Scale(light->color, lumens),
		.direction = Vec3_Scale(direction, lumens),
		.light_id = light->id,
		.light_type = light->type,
		.indirect_bounce = indirect_bounce
	};

	luxel->lumens = g_array_append_val(luxel->lumens, lumen);
}

/**
 * @brief Comparator for sorting luxel lumens by diffuse color descending.
 */
static gint Luxel_SortLumensCmp(gconstpointer a, gconstpointer b) {

	const lumen_t *a_lumen = (lumen_t *) a;
	const lumen_t *b_lumen = (lumen_t *) b;

	float a_intensity = Vec3_LengthSquared(a_lumen->diffuse);
	switch (a_lumen->light_type) {
		case LIGHT_SUN:
			a_intensity *= 8.f;
			break;
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_PATCH:
			a_intensity *= 16.f;
			break;
		default:
			break;
	}

	float b_intensity = Vec3_LengthSquared(b_lumen->diffuse);
	switch (b_lumen->light_type) {
		case LIGHT_SUN:
			a_intensity *= 8.f;
			break;
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_PATCH:
			b_intensity *= 16.f;
			break;
		default:
			break;
	}

	return 1000.f * (b_intensity - a_intensity);
}

/**
 * @brief Sorts luxel lumens by diffuse color descending.
 */
void Luxel_SortLumens(luxel_t *luxel) {

	assert(luxel);

	if (luxel->lumens) {
		g_array_sort(luxel->lumens, Luxel_SortLumensCmp);
	}
}

/**
 * @brief
 */
void Luxel_FreeLumens(luxel_t *luxel) {

	assert(luxel);

	if (luxel->lumens) {
		g_array_free(luxel->lumens, true);
	}
}
