/*
 * Copyright(c) 1997-2001 Id Software, Inc.
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

#include "qbsp.h"

/*
 * TextureAxisFromPlane
 */
static const vec3_t base_axis[18] = { { 0, 0, 1 }, { 1, 0, 0 }, { 0, -1, 0 }, // floor
		{ 0, 0, -1 }, { 1, 0, 0 }, { 0, -1, 0 }, // ceiling
		{ 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, // west wall
		{ -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, // east wall
		{ 0, 1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, // south wall
		{ 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, -1 }, // north wall
		};

static void TextureAxisFromPlane(map_plane_t *pln, vec3_t xv, vec3_t yv) {
	int bestaxis;
	vec_t dot, best;
	int i;

	best = 0;
	bestaxis = 0;

	for (i = 0; i < 6; i++) {
		dot = DotProduct(pln->normal, base_axis[i * 3]);
		if (dot > best) {
			best = dot;
			bestaxis = i;
		}
	}

	VectorCopy(base_axis[bestaxis * 3 + 1], xv);
	VectorCopy(base_axis[bestaxis * 3 + 2], yv);
}

/*
 * FindTexinfo
 *
 * Resolve the texinfo with identical properties to the one specified, or
 * allocate a new one.
 */
static int FindTexinfo(d_bsp_texinfo_t *tx) {
	int i;
	d_bsp_texinfo_t *tc = d_bsp.texinfo;

	for (i = 0; i < d_bsp.num_texinfo; i++, tc++) {

		if (tc->flags != tx->flags)
			continue;

		if (tc->value != tx->value)
			continue;

		if (strncmp(tc->texture, tx->texture, sizeof(tc->texture)))
			continue;

		if (memcmp((char *)(tc->vecs), (char *)(tx->vecs), sizeof(tx->vecs)))
			continue;

		return i;
	}

	if (i == MAX_BSP_TEXINFO) {
		Com_Error(ERR_FATAL, "TexinfoForBrushTexture: MAX_BSP_TEXINFO");
	}

	*tc = *tx;
	d_bsp.num_texinfo++;
	return i;
}

/*
 * TexinfoForBrushTexture
 */
int TexinfoForBrushTexture(map_plane_t *plane, map_brush_texture_t *bt,
		vec3_t origin) {
	vec3_t vecs[2];
	vec_t ang, sinv, cosv;
	vec_t ns, nt;
	vec2_t shift;
	d_bsp_texinfo_t tx;
	int i, j, sv, tv;

	if (!bt->name[0])
		return 0;

	memset(&tx, 0, sizeof(tx));
	strcpy(tx.texture, bt->name);

	TextureAxisFromPlane(plane, vecs[0], vecs[1]);

	shift[0] = DotProduct(origin, vecs[0]);
	shift[1] = DotProduct(origin, vecs[1]);

	if (!bt->scale[0])
		bt->scale[0] = 1.0;
	if (!bt->scale[1])
		bt->scale[1] = 1.0;

	// rotate axis
	if (bt->rotate == 0.0) {
		sinv = 0.0;
		cosv = 1.0;
	} else if (bt->rotate == 90.0) {
		sinv = 1.0;
		cosv = 0.0;
	} else if (bt->rotate == 180.0) {
		sinv = 0.0;
		cosv = -1.0;
	} else if (bt->rotate == 270.0) {
		sinv = -1.0;
		cosv = 0.0;
	} else {
		ang = bt->rotate / 180.0 * M_PI;
		sinv = sin(ang);
		cosv = cos(ang);
	}

	if (vecs[0][0])
		sv = 0;
	else if (vecs[0][1])
		sv = 1;
	else
		sv = 2;

	if (vecs[1][0])
		tv = 0;
	else if (vecs[1][1])
		tv = 1;
	else
		tv = 2;

	for (i = 0; i < 2; i++) {
		ns = cosv * vecs[i][sv] - sinv * vecs[i][tv];
		nt = sinv * vecs[i][sv] + cosv * vecs[i][tv];
		vecs[i][sv] = ns;
		vecs[i][tv] = nt;
	}

	for (i = 0; i < 2; i++)
		for (j = 0; j < 3; j++)
			tx.vecs[i][j] = vecs[i][j] / bt->scale[i];

	tx.vecs[0][3] = bt->shift[0] + shift[0];
	tx.vecs[1][3] = bt->shift[1] + shift[1];
	tx.flags = bt->flags;
	tx.value = bt->value;
	tx.next_texinfo = 0;

	return FindTexinfo(&tx);
}
