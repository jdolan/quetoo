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

#include "bsp.h"
#include "map.h"
#include "texture.h"

/**
 * @brief Resolve the texture with identical properties to the one specified, or
 * allocate a new one.
 */
int32_t FindTexture(const char *name) {

	const bsp_texture_t *texture = bsp_file.textures;
	for (int32_t i = 0; i < bsp_file.num_textures; i++, texture++) {

		if (!g_strcmp0(name, texture->name)) {
			return i;
		}
	}

	if (bsp_file.num_textures == MAX_BSP_TEXTURES) {
		Com_Error(ERROR_FATAL, "MAX_BSP_TEXTURES\n");
	}

	bsp_texture_t *out = bsp_file.textures + bsp_file.num_textures;
	g_strlcpy(out->name, name, sizeof(out->name));

	bsp_file.num_textures++;

	return (int32_t) (ptrdiff_t) (out - bsp_file.textures);
}

/**
 * @brief
 */
static void TextureAxisFromPlane(const plane_t *plane, vec3_t *xv, vec3_t *yv) {
	static const vec3_t base_axis[18] = { // base texture axis
		{ {  0,  0,  1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // floor
		{ {  0,  0, -1 } },
		{ {  1,  0,  0 } },
		{ {  0, -1,  0 } }, // ceiling
		{ {  1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // west wall
		{ { -1,  0,  0 } },
		{ {  0,  1,  0 } },
		{ {  0,  0, -1 } }, // east wall
		{ {  0,  1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // south wall
		{ {  0, -1,  0 } },
		{ {  1,  0,  0 } },
		{ {  0,  0, -1 } }, // north wall
	};

	int32_t best_axis = 0;
	float best = 0.0;

	for (int32_t i = 0; i < 6; i++) {
		const float dot = Vec3_Dot(plane->normal, base_axis[i * 3]);
		if (dot > best) {
			best = dot;
			best_axis = i;
		}
	}

	*xv = base_axis[best_axis * 3 + 1];
	*yv = base_axis[best_axis * 3 + 2];
}

/**
 * @brief
 */
void TextureVectorsForBrushSide(const brush_side_t *side, const vec3_t origin, vec4_t *out) {

	vec3_t vecs[2];
	TextureAxisFromPlane(&planes[side->plane], &vecs[0], &vecs[1]);

	vec2_t offset;
	offset.x = Vec3_Dot(origin, vecs[0]);
	offset.y = Vec3_Dot(origin, vecs[1]);

	vec2_t scale;
	scale.x = side->scale.x ?: 1.f;
	scale.y = side->scale.y ?: 1.f;

	// rotate axis
	float sinv, cosv;
	if (side->rotate == 0.f || side->rotate == -0.f) {
		sinv = 0.f;
		cosv = 1.f;
	} else if (side->rotate == 90.0 || side->rotate == -270.f) {
		sinv = 1.f;
		cosv = 0.f;
	} else if (side->rotate == 180.0 || side->rotate == -180.f) {
		sinv = 0.f;
		cosv = -1.f;
	} else if (side->rotate == 270.0 || side->rotate == -90.f) {
		sinv = -1.f;
		cosv = 0.f;
	} else {
		sinv = sinf(Radians(side->rotate));
		cosv = cosf(Radians(side->rotate));
	}

	int32_t sv;
	if (vecs[0].x) {
		sv = 0;
	} else if (vecs[0].y) {
		sv = 1;
	} else {
		sv = 2;
	}

	int32_t tv;
	if (vecs[1].x) {
		tv = 0;
	} else if (vecs[1].y) {
		tv = 1;
	} else {
		tv = 2;
	}

	for (int32_t i = 0; i < 2; i++) {
		const float ns = cosv * vecs[i].xyz[sv] - sinv * vecs[i].xyz[tv];
		const float nt = sinv * vecs[i].xyz[sv] + cosv * vecs[i].xyz[tv];
		vecs[i].xyz[sv] = ns;
		vecs[i].xyz[tv] = nt;
	}

	for (int32_t i = 0; i < 2; i++) {
		for (int32_t j = 0; j < 3; j++) {
			out[i].xyzw[j] = vecs[i].xyz[j] / scale.xy[i];
		}
	}

	out[0].w = side->shift.x + offset.x;
	out[1].w = side->shift.y + offset.y;
}
