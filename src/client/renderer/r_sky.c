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

#include "r_local.h"

#define SKY_DISTANCE 8192

// clipping matrix
static const vec3_t r_sky_clip[6] = {
	{ 1.0,  1.0,  0.0},
	{ 1.0, -1.0,  0.0},
	{ 0.0, -1.0,  1.0},
	{ 0.0,  1.0,  1.0},
	{ 1.0,  0.0,  1.0},
	{-1.0,  0.0,  1.0}
};

// 1 = s, 2 = t, 3 = SKY_DISTANCE
static const int32_t st_to_vec[6][3] = {
	{ 3, -1,  2},
	{-3,  1,  2},

	{ 1,  3,  2},
	{-1, -3,  2},

	{-2, -1,  3}, // 0 degrees yaw, look straight up
	{ 2, -1, -3} // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int32_t vec_to_st[6][3] = {
	{ -2,  3,  1},
	{  2,  3, -1},

	{  1,  3,  2},
	{ -1,  3, -2},

	{ -2, -1,  3},
	{ -2,  1, -3}
};

// sky structure
typedef struct {
	r_image_t *images[6];
	vec2_t st_mins[6];
	vec2_t st_maxs[6];
	vec_t st_min;
	vec_t st_max;
	GLuint texcoord_index;
	GLuint vert_index;
} r_sky_t;

static r_sky_t r_sky;

/*
 * @brief
 */
static void R_DrawSkySurface(int32_t nump, vec3_t vecs) {
	int32_t i, j;
	vec3_t v, av;
	float s, t, dv;
	int32_t axis;
	float *vp;

	// decide which face it maps to
	VectorClear(v);

	for (i = 0, vp = vecs; i < nump; i++, vp += 3)
		VectorAdd(v, vp, v); // sum them

	av[0] = fabsf(v[0]);
	av[1] = fabsf(v[1]);
	av[2] = fabsf(v[2]);

	// find the weight of the verts, and therefore the correct axis
	if (av[0] > av[1] && av[0] > av[2]) {
		if (v[0] < 0)
			axis = 1;
		else
			axis = 0;
	} else if (av[1] > av[2] && av[1] > av[0]) {
		if (v[1] < 0)
			axis = 3;
		else
			axis = 2;
	} else {
		if (v[2] < 0)
			axis = 5;
		else
			axis = 4;
	}

	// project new texture coords
	for (i = 0; i < nump; i++, vecs += 3) {

		j = vec_to_st[axis][2];
		if (j > 0)
			dv = vecs[j - 1];
		else
			dv = -vecs[-j - 1];

		if (dv < 0.001)
			continue; // don't divide by zero

		j = vec_to_st[axis][0];
		if (j < 0)
			s = -vecs[-j - 1] / dv;
		else
			s = vecs[j - 1] / dv;

		j = vec_to_st[axis][1];
		if (j < 0)
			t = -vecs[-j - 1] / dv;
		else
			t = vecs[j - 1] / dv;

		// extend the bounds of the sky surface
		if (s < r_sky.st_mins[0][axis])
			r_sky.st_mins[0][axis] = s;
		if (t < r_sky.st_mins[1][axis])
			r_sky.st_mins[1][axis] = t;

		if (s > r_sky.st_maxs[0][axis])
			r_sky.st_maxs[0][axis] = s;
		if (t > r_sky.st_maxs[1][axis])
			r_sky.st_maxs[1][axis] = t;
	}
}

#define ON_EPSILON		0.1 // point on plane side epsilon
#define MAX_CLIP_VERTS	64

/*
 * @brief
 */
static void R_ClipSkySurface(int32_t nump, vec3_t vecs, int32_t stage) {
	const float *norm;
	float *v;
	bool front, back;
	float d, e;
	float dists[MAX_CLIP_VERTS];
	int32_t sides[MAX_CLIP_VERTS];
	vec3_t newv[2][MAX_CLIP_VERTS];
	int32_t newc[2];
	int32_t i, j;

	if (nump > MAX_CLIP_VERTS - 2)
		Com_Error(ERR_DROP, "R_ClipSkySurface: MAX_CLIP_VERTS");

	if (stage == 6) { // fully clipped, so draw it
		R_DrawSkySurface(nump, vecs);
		return;
	}

	front = back = false;
	norm = r_sky_clip[stage];
	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		d = DotProduct(v, norm);
		if (d > ON_EPSILON) {
			front = true;
			sides[i] = SIDE_FRONT;
		} else if (d < -ON_EPSILON) {
			back = true;
			sides[i] = SIDE_BACK;
		} else
			sides[i] = SIDE_BOTH;
		dists[i] = d;
	}

	if (!front || !back) { // not clipped
		R_ClipSkySurface(nump, vecs, stage + 1);
		return;
	}

	// clip it
	sides[i] = sides[0];
	dists[i] = dists[0];
	VectorCopy(vecs, (vecs + (i * 3)));
	newc[0] = newc[1] = 0;

	for (i = 0, v = vecs; i < nump; i++, v += 3) {
		switch (sides[i]) {
		case SIDE_FRONT:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			break;
		case SIDE_BACK:
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		case SIDE_BOTH:
			VectorCopy(v, newv[0][newc[0]]);
			newc[0]++;
			VectorCopy(v, newv[1][newc[1]]);
			newc[1]++;
			break;
		}

		if (sides[i] == SIDE_BOTH || sides[i + 1] == SIDE_BOTH || sides[i + 1]
				== sides[i])
			continue;

		d = dists[i] / (dists[i] - dists[i + 1]);
		for (j = 0; j < 3; j++) {
			e = v[j] + d * (v[j + 3] - v[j]);
			newv[0][newc[0]][j] = e;
			newv[1][newc[1]][j] = e;
		}
		newc[0]++;
		newc[1]++;
	}

	// continue
	R_ClipSkySurface(newc[0], newv[0][0], stage + 1);
	R_ClipSkySurface(newc[1], newv[1][0], stage + 1);
}

/*
 * @brief
 */
static void R_AddSkySurface(const r_bsp_surface_t *surf) {
	vec3_t verts[MAX_CLIP_VERTS];
	uint16_t i;

	if (r_draw_wireframe->value)
		return;

	const GLuint index = surf->index * 3;

	// calculate distance to surface verts
	for (i = 0; i < surf->num_edges; i++) {
		const float *v = &r_model_state.world->verts[index + i * 3];
		VectorSubtract(v, r_view.origin, verts[i]);
	}

	R_ClipSkySurface(surf->num_edges, verts[0], 0);
}

/*
 * @brief
 */
void R_ClearSkyBox(void) {
	int32_t i;

	for (i = 0; i < 6; i++) {
		r_sky.st_mins[0][i] = r_sky.st_mins[1][i] = 9999;
		r_sky.st_maxs[0][i] = r_sky.st_maxs[1][i] = -9999;
	}
}

/*
 * @brief
 */
static void R_MakeSkyVec(float s, float t, int32_t axis) {
	vec3_t v, b;
	int32_t j;

	VectorSet(b, s * SKY_DISTANCE, t * SKY_DISTANCE, SKY_DISTANCE);

	for (j = 0; j < 3; j++) {
		const int32_t k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	memcpy(&r_state.vertex_array_3d[r_sky.vert_index], v, sizeof(vec3_t));
	r_sky.vert_index += 3;

	// avoid bilerp seam
	s = (s + 1.0) * 0.5;
	t = (t + 1.0) * 0.5;

	s = Clamp(s, r_sky.st_min, r_sky.st_max);
	t = 1.0 - Clamp(t, r_sky.st_min, r_sky.st_max);

	texunit_diffuse.texcoord_array[r_sky.texcoord_index++] = s;
	texunit_diffuse.texcoord_array[r_sky.texcoord_index++] = t;
}

/*
 * @brief
 */
void R_DrawSkyBox(void) {
	const int32_t sky_order[6] = { 0, 2, 1, 3, 4, 5 };
	r_bsp_surfaces_t *surfs;
	uint32_t i, j;

	surfs = &r_model_state.world->bsp->sorted_surfaces->sky;
	j = 0;

	// first add all visible sky surfaces to the sky bounds
	for (i = 0; i < surfs->count; i++) {
		if (surfs->surfaces[i]->frame == r_locals.frame) {
			R_AddSkySurface(surfs->surfaces[i]);
			j++;
		}
	}

	if (!j) // no visible sky surfaces
		return;

	R_ResetArrayState();

	glPushMatrix();
	glTranslatef(r_view.origin[0], r_view.origin[1], r_view.origin[2]);

	if (r_state.fog_enabled)
		glFogf(GL_FOG_END, FOG_END * 8);

	r_sky.texcoord_index = r_sky.vert_index = 0;

	for (i = 0; i < 6; i++) {

		if (r_sky.st_mins[0][i] >= r_sky.st_maxs[0][i] || r_sky.st_mins[1][i]
				>= r_sky.st_maxs[1][i])
			continue; // nothing on this plane

		R_BindTexture(r_sky.images[sky_order[i]]->texnum);

		R_MakeSkyVec(r_sky.st_mins[0][i], r_sky.st_mins[1][i], i);
		R_MakeSkyVec(r_sky.st_mins[0][i], r_sky.st_maxs[1][i], i);
		R_MakeSkyVec(r_sky.st_maxs[0][i], r_sky.st_maxs[1][i], i);
		R_MakeSkyVec(r_sky.st_maxs[0][i], r_sky.st_mins[1][i], i);

		glDrawArrays(GL_QUADS, 0, r_sky.vert_index / 3);
		r_sky.texcoord_index = r_sky.vert_index = 0;
	}

	if (r_state.fog_enabled)
		glFogf(GL_FOG_END, FOG_END);

	glPopMatrix();
}

/*
 * @brief Sets the sky to the specified environment map.
 */
void R_SetSky(const char *name) {
	const char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };
	uint32_t i;

	for (i = 0; i < lengthof(suf); i++) {
		char path[MAX_QPATH];

		g_snprintf(path, sizeof(path), "env/%s%s", name, suf[i]);
		r_sky.images[i] = R_LoadImage(path, IT_SKY);

		if (r_sky.images[i]->type == IT_NULL) { // try unit1_
			if (strcmp(name, "unit1_")) {
				R_SetSky("unit1_");
				return;
			}
		}
	}

	// assume all sky components are the same size
	r_sky.st_min = 1.0 / (float) r_sky.images[0]->width;
	r_sky.st_max = (r_sky.images[0]->width - 1.0) / (float) r_sky.images[0]->width;
}

/*
 * @brief
 */
void R_Sky_f(void) {

	if (Cmd_Argc() != 2) {
		Com_Print("Usage: %s <basename>\n", Cmd_Argv(0));
		return;
	}

	R_SetSky(Cmd_Argv(1));
}
