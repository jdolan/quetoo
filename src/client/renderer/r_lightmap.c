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
#include "r_gl.h"


/**
 * @brief Calculates the lightmap extents and pushes it to list of blocks to be processed.
 */
void R_CreateBspSurfaceLightmap(const r_bsp_model_t *bsp, r_bsp_surface_t *surf) {

	r_bsp_surface_lightmap_t *lm = &surf->lightmap;

	vec3_t s, t;

	VectorNormalize2(surf->texinfo->vecs[0], s);
	VectorNormalize2(surf->texinfo->vecs[1], t);

	VectorScale(s, 1.0 / bsp->luxel_size, s);
	VectorScale(t, 1.0 / bsp->luxel_size, t);

	Matrix4x4_FromArrayFloatGL(&lm->matrix, (const vec_t[]) {
		s[0], t[0], surf->plane->normal[0], 0.0,
		s[1], t[1], surf->plane->normal[1], 0.0,
		s[2], t[2], surf->plane->normal[2], 0.0,
		0.0,  0.0,  -surf->plane->dist,     1.0
	});

	Matrix4x4_Invert_Full(&lm->inverse_matrix, &lm->matrix);

	ClearStBounds(lm->st_mins, lm->st_maxs);

	const r_bsp_vertex_t *v = bsp->vertexes + surf->vertex;
	for (int32_t i = 0; i < surf->num_vertexes; i++, v++) {

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, v->position, st);

		AddStToBounds(st, lm->st_mins, lm->st_maxs);
	}

	lm->w = floorf(lm->st_maxs[0] - lm->st_mins[0]) + 2;
	lm->h = floorf(lm->st_maxs[1] - lm->st_mins[1]) + 2;
}

