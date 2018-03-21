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

#include "light.h"
#include "lightmap.h"
#include "bspfile.h"
#include "patches.h"
#include "polylib.h"
#include "qlight.h"

face_lighting_t face_lighting[MAX_BSP_FACES];

/**
 * @brief Sets up all per-face lighting calculations. This includes world space bounds, offsets,
 * and normals, as well as tangent space bounds, normals, and vectors to convert between the two.
 * The texture math here would probably be better served by a matrix, but that's a project for a
 * very rainy day.
 */
void BuildFaceLighting(void) {
	const bsp_vertex_t *v;

	memset(face_lighting, 0, sizeof(face_lighting));

	face_lighting_t *l = face_lighting;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, l++) {

		const bsp_face_t *face = l->face = &bsp_file.faces[i];
		const bsp_texinfo_t *tex = l->texinfo = &bsp_file.texinfo[face->texinfo];

		VectorCopy(face_offsets[i], l->offset);

		VectorSet(l->mins, 999999, 999999, 999999);
		VectorSet(l->maxs, -999999, -999999, -999999);

		l->st_mins[0] = l->st_mins[1] = 999999;
		l->st_maxs[0] = l->st_maxs[1] = -999999;

		for (int32_t j = 0; j < face->num_edges; j++) {
			const int32_t e = bsp_file.face_edges[face->first_edge + j];
			if (e >= 0) {
				v = bsp_file.vertexes + bsp_file.edges[e].v[0];
			} else {
				v = bsp_file.vertexes + bsp_file.edges[-e].v[1];
			}

			for (int32_t k = 0; k < 3; k++) {
				if (v->point[k] > l->maxs[k]) {
					l->maxs[k] = v->point[k];
				}
				if (v->point[k] < l->mins[k]) {
					l->mins[k] = v->point[k];
				}
			}

			for (int32_t k = 0; k < 2; k++) {
				const vec_t val = DotProduct(v->point, tex->vecs[k]) + tex->vecs[k][3];
				if (val < l->st_mins[k]) {
					l->st_mins[k] = val;
				}
				if (val > l->st_maxs[k]) {
					l->st_maxs[k] = val;
				}
			}
		}

		VectorMix(l->mins, l->maxs, 0.5, l->center);

		const bsp_plane_t *plane = &bsp_file.planes[face->plane_num];
		vec_t dist;
		if (face->side) {
			VectorNegate(plane->normal, l->normal);
			dist = -plane->dist;
		} else {
			VectorCopy(plane->normal, l->normal);
			dist = plane->dist;
		}

		// calculate the texture normal vector
		CrossProduct(tex->vecs[0], tex->vecs[1], l->st_normal);
		VectorNormalize(l->st_normal);

		// flip it towards plane normal
		vec_t scale = DotProduct(l->st_normal, l->normal);
		if (scale == 0.0) {
			Com_Warn("Texture axis perpendicular to face\n");
			scale = 1.0;
		} else if (scale < 0.0) {
			scale = -scale;
			VectorNegate(l->st_normal, l->st_normal);
		}

		// we're interested in texture to world, so invert it
		scale = 1.0 / scale;

		// calculate the tangent and bitangent vectors in texture space
		const vec_t tangent_len = VectorLength(tex->vecs[0]);
		const vec_t tangent_dist = DotProduct(tex->vecs[0], l->normal) * scale;

		VectorMA(tex->vecs[0], -tangent_dist, l->st_normal, l->st_tangent);
		VectorScale(l->st_tangent, (1.0 / tangent_len) * (1.0 / tangent_len), l->st_tangent);

		const vec_t bitangent_len = VectorLength(tex->vecs[1]);
		const vec_t bitangent_dist = DotProduct(tex->vecs[1], l->normal) * scale;

		VectorMA(tex->vecs[1], -bitangent_dist, l->st_normal, l->st_bitangent);
		VectorScale(l->st_bitangent, (1.0 / bitangent_len) * (1.0 / bitangent_len), l->st_bitangent);

		// calculate texture origin on the texture plane
		for (int32_t i = 0; i < 3; i++) {
			l->st_origin[i] = -tex->vecs[0][3] * l->st_tangent[i] - tex->vecs[1][3] * l->st_bitangent[i];
		}

		// project back to the face plane
		const vec_t back = DotProduct(l->st_origin, l->normal) - dist - 1.0;

		VectorMA(l->st_origin, -back * scale, l->st_normal, l->st_origin);

		// add the offset
		VectorAdd(l->st_origin, l->offset, l->st_origin);

		// calculate the lightmap mins and maxs
		for (int32_t i = 0; i < 2; i++) {

			l->lm_mins[i] = floorf(l->st_mins[i] * lightmap_scale);
			l->lm_maxs[i] = ceilf(l->st_maxs[i] * lightmap_scale);

			l->lm_size[i] = l->lm_maxs[i] - l->lm_mins[i] + 1;
		}

		l->num_luxels = l->lm_size[0] * l->lm_size[1];

		if (l->num_luxels > MAX_BSP_LIGHTMAP) {
			const winding_t *w = WindingForFace(face);
			Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points,
							va("Surface too large to light (%zd)\n", l->num_luxels));
		}

		for (patch_t *p = face_patches[face - bsp_file.faces]; p; p = p->next) {
			// now back the lightmap mins and maxs into the patches belonging to this face
		}

		// allocate the luxel origins, normals and sample buffers
		l->origins = Mem_TagMalloc(l->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
		l->normals = Mem_TagMalloc(l->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
		l->direct = Mem_TagMalloc(l->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
		l->directions = Mem_TagMalloc(l->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
		l->indirect = Mem_TagMalloc(l->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	}
}

#define MAX_VERT_FACES 256

/**
 * @brief Populate faces with indexes of all Phong-shaded d_bsp_face_t's
 * referencing the specified vertex. The number of d_bsp_face_t's referencing
 * the vertex is returned in nfaces.
 */
static int32_t FacesWithVert(int32_t vert, int32_t *faces) {

	int32_t count = 0;

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const bsp_face_t *face = &bsp_file.faces[i];

		if (!(bsp_file.texinfo[face->texinfo].flags & SURF_PHONG)) {
			continue;
		}

		for (int32_t j = 0; j < face->num_edges; j++) {

			const int32_t e = bsp_file.face_edges[face->first_edge + j];
			const int32_t v = e >= 0 ? bsp_file.edges[e].v[0] : bsp_file.edges[-e].v[1];

			if (v == vert) { // face references vert
				faces[count++] = i;
				if (count == MAX_VERT_FACES) {
					Mon_SendPoint(MON_ERROR, bsp_file.vertexes[v].point, "MAX_VERT_FACES");
				}
				break;
			}
		}
	}

	return count;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by
 * finding all of the faces which share a given vertex, and calculating a weighted
 * average of their normals.
 */
void BuildVertexNormals(void) {
	int32_t vert_faces[MAX_VERT_FACES];
	vec3_t normal, delta;

	for (int32_t i = 0; i < bsp_file.num_vertexes; i++) {

		VectorClear(bsp_file.normals[i].normal);

		const int32_t count = FacesWithVert(i, vert_faces);
		if (!count) {
			continue;
		}

		for (int32_t j = 0; j < count; j++) {

			const bsp_face_t *face = &bsp_file.faces[vert_faces[j]];
			const bsp_plane_t *plane = &bsp_file.planes[face->plane_num];

			// scale the contribution of each face based on size
			const face_lighting_t *lighting = &face_lighting[vert_faces[j]];
			VectorSubtract(lighting->maxs, lighting->mins, delta);

			const vec_t scale = VectorLength(delta);

			if (face->side) {
				VectorScale(plane->normal, -scale, normal);
			} else {
				VectorScale(plane->normal, scale, normal);
			}

			VectorAdd(bsp_file.normals[i].normal, normal, bsp_file.normals[i].normal);
		}

		VectorNormalize(bsp_file.normals[i].normal);
	}
}

/**
 * @brief A bucket for resolving Phong-interpolated normal vectors.
 */
typedef struct {
	vec3_t normal;
	vec_t dist;
} phong_vertex_t;

/**
 * @brief Qsort comparator for PhongNormal.
 */
static int32_t PhongNormal_sort(const void *a, const void *b) {
	return ((phong_vertex_t *) a)->dist - ((phong_vertex_t *) b)->dist;
}

/**
 * @brief For Phong-shaded samples, calculate the interpolated normal vector using
 * the three nearest vertexes.
 */
static void PhongNormal(const bsp_face_t *face, const vec3_t pos, vec3_t normal) {
	phong_vertex_t verts[face->num_edges];

	for (int32_t i = 0; i < face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[face->first_edge + i];
		uint16_t v;

		if (e >= 0) {
			v = bsp_file.edges[e].v[0];
		} else {
			v = bsp_file.edges[-e].v[1];
		}

		VectorCopy(bsp_file.normals[v].normal, verts[i].normal);

		vec3_t delta;
		VectorSubtract(pos, bsp_file.vertexes[v].point, delta);
		verts[i].dist = VectorLength(delta);

	}

	qsort(verts, face->num_edges, sizeof(phong_vertex_t), PhongNormal_sort);

	VectorClear(normal);

	const vec_t dist = (verts[0].dist + verts[1].dist + verts[2].dist);

	for (int32_t i = 0; i < 3; i++) {
		const vec_t scale = 1.0 - verts[i].dist / dist;
		VectorMA(normal, scale, verts[i].normal, normal);
	}

	VectorNormalize(normal);
}

/**
 * @brief For each texture aligned grid point, back project onto the plane to yield the origin.
 * Additionally calculate the per-sample normal vector, which may use Phong interpolation.
 */
static void BuildFaceLightingPoints(face_lighting_t *l) {

	const int32_t w = l->lm_size[0];
	const int32_t h = l->lm_size[1];

	const int32_t step = 1.0 / lightmap_scale;

	const vec_t start_s = l->lm_mins[0] * step;
	const vec_t start_t = l->lm_mins[1] * step;

	vec_t *origins = l->origins;
	vec_t *normals = l->normals;

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++, origins += 3, normals += 3) {

			const vec_t us = start_s + s * step;
			const vec_t ut = start_t + t * step;

			for (int32_t i = 0; i < 3; i++) {
				origins[i] = l->st_origin[i] + l->st_tangent[i] * us + l->st_bitangent[i] * ut;
			}

			if (l->texinfo->flags & SURF_PHONG) {
				PhongNormal(l->face, origins, normals);
			} else {
				VectorCopy(l->normal, normals);
			}
		}
	}
}

#define SAMPLE_NUDGE 0.25

/**
 * @brief Nudge the sample origin outward along the surface normal to reduce false-positive traces.
 * Test the PVS at the new position, returning true if the new point is valid, false otherwise.
 */
static _Bool NudgeSamplePosition(const face_lighting_t *l, const vec3_t origin, const vec3_t normal,
								 vec_t soffs, vec_t toffs, vec3_t out, byte *pvs) {

	VectorCopy(origin, out);

	if (soffs || toffs) {
		const int32_t step = 1.0 / lightmap_scale;

		const vec_t us = soffs * step;
		const vec_t ut = toffs * step;

		for (int32_t i = 0; i < 3; i++) {
			out[i] += l->st_tangent[i] * us + l->st_bitangent[i] * ut;
		}
	}

	if (Light_PointPVS(out, pvs)) {
		return true;
	}

	VectorMA(out, SAMPLE_NUDGE, normal, out);

	if (Light_PointPVS(out, pvs)) {
		return true;
	}

	vec3_t delta;
	VectorSubtract(l->center, origin, delta);
	VectorNormalize(delta);

	VectorMA(out, SAMPLE_NUDGE, delta, out);

	if (Light_PointPVS(out, pvs)) {
		return true;
	}

	return false;
}

/**
 * @brief Iterates all sun light sources, tracing towards them and adding light if a sky was found.
 */
static void GatherSampleSunlight(const vec3_t pos, const vec3_t normal, vec_t *sample, vec_t *direction, vec_t scale) {

	const sun_t *sun = suns;
	for (size_t i = 0; i < num_suns; i++, sun++) {

		const vec_t dot = DotProduct(sun->normal, normal);
		if (dot <= 0.0) {
			continue;
		}

		vec3_t end;
		VectorMA(pos, MAX_WORLD_DIST, sun->normal, end);

		cm_trace_t trace;
		Light_Trace(&trace, pos, end, CONTENTS_SOLID);

		if (trace.fraction < 1.0 && (trace.surface->flags & SURF_SKY)) {

			const vec_t diffuse = sun->light * DEFAULT_LIGHT * dot * DEFAULT_PATCH_SIZE;

			VectorMA(sample, diffuse * scale, sun->color, sample);

			if (direction) {
				VectorMA(direction, diffuse * scale / DEFAULT_LIGHT, sun->normal, direction);
			}
		}
	}
}

/**
 * @brief Iterate over all light sources for the sample position's PVS, accumulating light and
 * directional information to the specified pointers.
 */
static void GatherSampleLight(const vec3_t pos, const vec3_t normal, const byte *pvs, vec_t *sample, vec_t *direction, vec_t scale) {

	for (int32_t cluster = 0; cluster < bsp_file.vis_data.vis->num_clusters; cluster++) {

		if (!(pvs[cluster >> 3] & (1 << (cluster & 7)))) {
			continue;
		}

		for (const light_t *l = lights[cluster]; l; l = l->next) {

			vec3_t dir;
			VectorSubtract(l->origin, pos, dir);

			const vec_t dist = VectorNormalize(dir);
			if (dist > l->radius) {
				continue;
			}

			const vec_t dot = DotProduct(dir, normal);
			if (dot <= 0.0) {
				continue;
			}

			vec_t diffuse = l->radius * dot;

			switch (l->type) {

				case LIGHT_SURFACE:
					diffuse *= patch_size / DEFAULT_PATCH_SIZE;
					break;

				case LIGHT_POINT:
					diffuse *= DEFAULT_PATCH_SIZE;
					break;

				case LIGHT_SPOT: {
					const vec_t dot2 = DotProduct(dir, l->normal);
					if (dot2 <= l->cone) {
						if (dot2 <= 0.1) {
							diffuse = 0.0;
						} else {
							diffuse *= l->cone - (1.0 - dot2);
						}
					} else {
						diffuse *= DEFAULT_PATCH_SIZE;
					}
				}
					break;
			}

			if (diffuse <= 0.0) {
				continue;
			}

			const vec_t atten = Clamp(1.0 - dist / l->radius, 0.0, 1.0);
			const vec_t atten_squared = atten * atten;

			diffuse *= atten_squared;

			cm_trace_t trace;
			Light_Trace(&trace, l->origin, pos, CONTENTS_SOLID);

			if (trace.fraction < 1.0) {
				continue;
			}

			VectorMA(sample, diffuse * scale, l->color, sample);

			if (direction) {
				VectorMA(direction, diffuse * scale / l->radius, dir, direction);
			}
		}
	}
}

static const vec3_t invalid = { -1.0, -1.0, -1.0 };

/**
 * @brief Calculates direct lighting for the given face. An origin and normal vector in world
 * space are calculated for each luxel, using the texture vectors provided in face_lighting_t.
 * We then query the light sources that are in PVS for each luxel, and accumulate their diffuse
 * and directional contributions as non-normalized floating point.
 */
void DirectLighting(int32_t face_num) {

	static const vec2_t offsets[] = {
		{ +0.0, +0.0 }, { -0.5, -0.5 }, { +0.0, -0.5 },
		{ +0.5, -0.5 },	{ -0.5, +0.0 }, { +0.5, +0.0 },
		{ -0.5, +0.5 }, { +0.0, +0.5 }, { +0.5, +0.5 }
	};

	static const vec_t weights[] = {
		0.195346, 0.077847, 0.123317,
		0.077847, 0.123317, 0.123317,
		0.077847, 0.123317, 0.077847
	};

	byte pvs[(MAX_BSP_LEAFS + 7) / 8];

	face_lighting_t *l = &face_lighting[face_num];

	if (l->texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	// calculate the origins and normals for all sample points
	BuildFaceLightingPoints(l);

	// for each luxel, peek our head out into the world and attempt to gather light
	for (size_t i = 0; i < l->num_luxels; i++) {

		const vec_t *origin = l->origins + i * 3;
		const vec_t *normal = l->normals + i * 3;

		vec_t *direct = l->direct + i * 3;
		vec_t *direction = l->directions + i * 3;

		size_t num_samples = antialias ? lengthof(offsets) : 1, valid_samples = 0;

		for (size_t j = 0; j < num_samples; j++) {

			const vec_t soffs = offsets[j][0];
			const vec_t toffs = offsets[j][1];

			vec3_t pos;

			if (!NudgeSamplePosition(l, origin, normal, soffs, toffs, pos, pvs)) {
				continue;
			}

			valid_samples++;

			const vec_t scale = antialias ? weights[j] : 1.0;

			// gather lighting from direct light sources
			GatherSampleLight(pos, normal, pvs, direct, direction, scale);

			// including all configured suns
			GatherSampleSunlight(pos, normal, direct, direction, scale);
		}

		if (valid_samples == 0) {
		} else if (valid_samples < num_samples) {
			const vec_t scale = 1.0 / ((vec_t) valid_samples / num_samples);
			VectorScale(direct, scale, direct);
			VectorScale(direction, scale, direction);
		}
	}
}

/**
 * @brief
 */
void IndirectLighting(int32_t face_num) {
	byte pvs[(MAX_BSP_LEAFS + 7) / 8];

	face_lighting_t *l = &face_lighting[face_num];

	if (l->texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	// for each luxel, peek our head out into the world and attempt to gather light
	for (size_t i = 0; i < l->num_luxels; i++) {

		const vec_t *origin = l->origins + i * 3;
		const vec_t *normal = l->normals + i * 3;

		vec_t *indirect = l->indirect + i * 3;

		vec3_t pos;

		if (!NudgeSamplePosition(l, origin, normal, 0.0, 0.0, pos, pvs)) {
			continue;
		}

		// gather indirect lighting from indirect light sources
		GatherSampleLight(pos, normal, pvs, indirect, NULL, 0.125);
	}
}

/**
 * @brief Finalize and write the lightmap data for the given face.
 */
void FinalizeLighting(int32_t face_num) {

	bsp_face_t *f = &bsp_file.faces[face_num];
	const bsp_texinfo_t *tex = &bsp_file.texinfo[f->texinfo];

	if (tex->flags & (SURF_WARP | SURF_SKY)) {
		return;
	}

	const int32_t lightmap_color_channels = (legacy ? 3 : 4);

	f->unused[0] = 0; // pack the old lightstyles array for legacy games
	f->unused[1] = f->unused[2] = f->unused[3] = 255;

	ThreadLock();

	face_lighting_t *l = &face_lighting[face_num];

	f->light_ofs = bsp_file.lightmap_data_size;
	bsp_file.lightmap_data_size += l->num_luxels * lightmap_color_channels;

	if (!legacy) { // account for light direction data as well
		bsp_file.lightmap_data_size += l->num_luxels * lightmap_color_channels;
	}

	if (bsp_file.lightmap_data_size > MAX_BSP_LIGHTING) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTING\n");
	}

	ThreadUnlock();

	// write it out
	byte *dest = &bsp_file.lightmap_data[f->light_ofs];

	for (size_t i = 0; i < l->num_luxels; i++) {
		vec3_t lightmap;
		vec4_t hdr_lightmap;

		const vec_t *direct = l->direct + i * 3;
		const vec_t *indirect = l->indirect + i * 3;

		// start with raw sample data
		VectorAdd(direct, indirect, lightmap);
		//VectorCopy(indirect, lightmap); // uncomment this to see just indirect lighting

		// convert to float
		VectorScale(lightmap, 1.0 / 255.0, lightmap);

		// add an ambient term if desired
		VectorAdd(lightmap, ambient, lightmap);

		// apply brightness, saturation and contrast
		ColorFilter(lightmap, lightmap, brightness, saturation, contrast);

		if (legacy) { // write out good old RGB lightmap samples, converted to bytes
			for (int32_t j = 0; j < 3; j++) {
				*dest++ = (byte) Clamp(floor(lightmap[j] * 255.0 + 0.5), 0, 255);
			}
		} else { // write out HDR lightmaps and deluxemaps

			ColorEncodeRGBM(lightmap, hdr_lightmap);

			for (int32_t j = 0; j < 4; j++) {
				*dest++ = (byte) Clamp(floor(hdr_lightmap[j] * 255.0 + 0.5), 0, 255);
			}

			vec3_t direction, deluxemap;
			vec4_t hdr_deluxemap;

			// start with the raw direction data
			VectorCopy((l->directions + i * 3), direction);

			// if the sample was lit, it will have a directional vector in world space
			if (!VectorCompare(direction, vec3_origin)) {

				const vec_t *normal = l->normals + i * 3;

				vec4_t tangent;
				vec3_t bitangent;

				TangentVectors(normal, tex->vecs[0], tex->vecs[1], tangent, bitangent);

				// transform it into tangent space
				deluxemap[0] = DotProduct(direction, tangent);
				deluxemap[1] = DotProduct(direction, bitangent);
				deluxemap[2] = DotProduct(direction, normal);

				VectorAdd(deluxemap, vec3_up, deluxemap);
				VectorNormalize(deluxemap);
			} else {
				VectorCopy(vec3_up, deluxemap);
			}

			// pack floating point -1.0 to 1.0 to positive bytes (0.0 becomes 127)
			for (int32_t j = 0; j < 3; j++) {
				deluxemap[j] = (deluxemap[j] + 1.0) * 0.5;
			}

			ColorEncodeRGBM(deluxemap, hdr_deluxemap);

			for (int32_t j = 0; j < 4; j++) {
				*dest++ = (byte) Clamp(floor(hdr_deluxemap[j] * 255.0 + 0.5), 0, 255);
			}
		}
	}
}
