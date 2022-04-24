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
void Luxel_Illuminate(luxel_t *luxel, const lumen_t *lumen) {

	assert(lumen->light);

	switch (lumen->light->type) {
		case LIGHT_SUN:
		case LIGHT_POINT:
		case LIGHT_SPOT:
		case LIGHT_PATCH: {

			lumen_t *diffuse = luxel->diffuse;
			for (int32_t i = 0; i < BSP_LIGHTMAP_CHANNELS; i++, diffuse++) {

				if (diffuse->light == NULL) {
					*diffuse = *lumen;
					return;
				}

				if (diffuse->light == lumen->light) {
					diffuse->color = Vec3_Add(diffuse->color, lumen->color);
					diffuse->direction = Vec3_Add(diffuse->direction, lumen->direction);
					return;
				}

				if (Vec3_LengthSquared(lumen->color) > Vec3_LengthSquared(diffuse->color)) {

					const lumen_t last = luxel->diffuse[BSP_LIGHTMAP_CHANNELS - 1];

					for (int32_t j = BSP_LIGHTMAP_CHANNELS - 1; j > i; j--) {
						luxel->diffuse[j] = luxel->diffuse[j - 1];
					}

					*diffuse = *lumen;

					if (last.light) {
						Luxel_Illuminate(luxel, &last);
					}

					return;
				}
			}

			vec3_t c = lumen->color;
			vec3_t d = lumen->direction;

			diffuse = luxel->diffuse;
			for (int32_t j = 0; j < BSP_LIGHTMAP_CHANNELS; j++, diffuse++) {

				const vec3_t a = Vec3_Normalize(lumen->direction);
				const vec3_t b = Vec3_Normalize(diffuse->direction);

				const float dot = Clampf(Vec3_Dot(a, b), 0.f, 1.f);

				diffuse->color = Vec3_Fmaf(diffuse->color, dot, c);
				diffuse->direction = Vec3_Fmaf(diffuse->direction, dot, d);

				c = Vec3_Fmaf(c, -dot, c);
				d = Vec3_Fmaf(d, -dot, d);
			}

			luxel->ambient.color = Vec3_Add(luxel->ambient.color, c);
		}

			break;
		default:
			luxel->ambient.color = Vec3_Add(luxel->ambient.color, lumen->color);
			break;
	}
}
