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
static const vec3_t skyclip[6] = {
	{ 1,  1,  0},
	{ 1, -1,  0},
	{ 0, -1,  1},
	{ 0,  1,  1},
	{ 1,  0,  1},
	{-1,  0,  1}
};

// 1 = s, 2 = t, 3 = SKY_DISTANCE
static const int st_to_vec[6][3] = {
	{ 3, -1,  2},
	{-3,  1,  2},

	{ 1,  3,  2},
	{-1, -3,  2},

	{-2, -1,  3},   // 0 degrees yaw, look straight up
	{ 2, -1, -3}  // look straight down
};

// s = [0]/[2], t = [1]/[2]
static const int vec_to_st[6][3] = {
	{ -2,  3,  1},
	{  2,  3, -1},

	{  1,  3,  2},
	{ -1,  3, -2},

	{ -2, -1,  3},
	{ -2,  1, -3}
};

// sky structure
typedef struct sky_s {
	r_image_t *images[6];
	vec2_t st_mins[6];
	vec2_t st_maxs[6];
	vec_t stmin;
	vec_t stmax;
	int texcoord_index;
	int vert_index;
} sky_t;

static sky_t sky;

/*
 * R_DrawSkySurface
 */
static void R_DrawSkySurface(int nump, vec3_t vecs) {
	int i, j;
	vec3_t v, av;
	float s, t, dv;
	int axis;
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

		if (s < sky.st_mins[0][axis])
			sky.st_mins[0][axis] = s;
		if (t < sky.st_mins[1][axis])
			sky.st_mins[1][axis] = t;

		if (s > sky.st_maxs[0][axis])
			sky.st_maxs[0][axis] = s;
		if (t > sky.st_maxs[1][axis])
			sky.st_maxs[1][axis] = t;
	}
}

#define ON_EPSILON		0.1  // point on plane side epsilon
#define MAX_CLIP_VERTS	64

/*
 * R_ClipSkySurface
 */
static void R_ClipSkySurface(int nump, vec3_t vecs, int stage) {
	const float *norm;
	float *v;
	boolean_t front, back;
	float d, e;
	float dists[MAX_CLIP_VERTS];
	int sides[MAX_CLIP_VERTS];
	vec3_t newv[2][MAX_CLIP_VERTS];
	int newc[2];
	int i, j;

	if (nump > MAX_CLIP_VERTS - 2)
		Com_Error(ERR_DROP, "R_ClipSkyPoly: MAX_CLIP_VERTS");

	if (stage == 6) { // fully clipped, so draw it
		R_DrawSkySurface(nump, vecs);
		return;
	}

	front = back = false;
	norm = skyclip[stage];
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
 * R_AddSkySurface
 */
static void R_AddSkySurface(const r_bsp_surface_t *surf) {
	int i, index;
	vec3_t verts[MAX_CLIP_VERTS];

	if (r_draw_wireframe->value)
		return;

	index = surf->index * 3; // raw index into cached vertex arrays

	// calculate distance to surface verts
	for (i = 0; i < surf->num_edges; i++) {

		const float *v = &r_world_model->verts[index + i * 3];

		VectorSubtract(v, r_view.origin, verts[i]);
	}

	R_ClipSkySurface(surf->num_edges, verts[0], 0);
}

/*
 * R_ClearSkyBox
 */
void R_ClearSkyBox(void) {
	int i;

	for (i = 0; i < 6; i++) {
		sky.st_mins[0][i] = sky.st_mins[1][i] = 9999;
		sky.st_maxs[0][i] = sky.st_maxs[1][i] = -9999;
	}
}

/*
 * R_MakeSkyVec
 */
static void R_MakeSkyVec(float s, float t, int axis) {
	vec3_t v, b;
	int j;

	b[0] = s * SKY_DISTANCE;
	b[1] = t * SKY_DISTANCE;
	b[2] = SKY_DISTANCE;

	for (j = 0; j < 3; j++) {
		const int k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
	}

	// avoid bilerp seam
	s = (s + 1) * 0.5;
	t = (t + 1) * 0.5;

	if (s < sky.stmin)
		s = sky.stmin;
	else if (s > sky.stmax)
		s = sky.stmax;
	if (t < sky.stmin)
		t = sky.stmin;
	else if (t > sky.stmax)
		t = sky.stmax;

	t = 1.0 - t;

	texunit_diffuse.texcoord_array[sky.texcoord_index++] = s;
	texunit_diffuse.texcoord_array[sky.texcoord_index++] = t;

	memcpy(&r_state.vertex_array_3d[sky.vert_index], v, sizeof(vec3_t));
	sky.vert_index += 3;
}

int skytexorder[6] = { 0, 2, 1, 3, 4, 5 };

/*
 * R_DrawSkyBox
 */
void R_DrawSkyBox(void) {
	r_bsp_surfaces_t *surfs;
	unsigned int i, j;

	surfs = r_world_model->sky_surfaces;
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

	sky.texcoord_index = sky.vert_index = 0;

	for (i = 0; i < 6; i++) {

		if (sky.st_mins[0][i] >= sky.st_maxs[0][i] || sky.st_mins[1][i]
				>= sky.st_maxs[1][i])
			continue; // nothing on this plane

		R_BindTexture(sky.images[skytexorder[i]]->texnum);

		R_MakeSkyVec(sky.st_mins[0][i], sky.st_mins[1][i], i);
		R_MakeSkyVec(sky.st_mins[0][i], sky.st_maxs[1][i], i);
		R_MakeSkyVec(sky.st_maxs[0][i], sky.st_maxs[1][i], i);
		R_MakeSkyVec(sky.st_maxs[0][i], sky.st_mins[1][i], i);

		glDrawArrays(GL_QUADS, 0, sky.vert_index / 3);
		sky.texcoord_index = sky.vert_index = 0;
	}

	if (r_state.fog_enabled)
		glFogf(GL_FOG_END, FOG_END);

	glPopMatrix();
}

// 3dstudio environment map names
char *suf[6] = { "rt", "bk", "lf", "ft", "up", "dn" };

/*
 * R_SetSky
 */
void R_SetSky(char *name) {
	int i;
	char pathname[MAX_QPATH];

	for (i = 0; i < 6; i++) {

		snprintf(pathname, sizeof(pathname), "env/%s%s", name, suf[i]);
		sky.images[i] = R_LoadImage(pathname, it_sky);

		if (!sky.images[i] || sky.images[i] == r_null_image) { // try unit1_
			if (strcmp(name, "unit1_")) {
				R_SetSky("unit1_");
				return;
			}
		}

		sky.stmin = 1.0 / (float) sky.images[i]->width;
		sky.stmax = (sky.images[i]->width - 1.0) / (float) sky.images[i]->width;
	}
}
