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

/**
 * @brief A temporary bucket for storing lightmap calculations.
 */
typedef struct {
	const bsp_face_t *face;
	const bsp_plane_t *plane;
	const bsp_texinfo_t *texinfo;

	vec3_t normal; // the plane normal, negated if back facing plane
	vec_t dist; // the plane distance, negated if back facing plane

	vec3_t model_org; // for bsp submodels

	vec3_t tex_org;
	vec3_t tex_to_world[2]; // world = tex_org + s * tex_to_world[0]

	int32_t tex_mins[2], tex_maxs[2], tex_size[2];

	size_t num_luxels;

} light_info_t;

face_extents_t face_extents[MAX_BSP_FACES];
face_lighting_t face_lighting[MAX_BSP_FACES];

#define MAX_SAMPLES 9

static const vec_t sample_offsets[MAX_SAMPLES][2] = {
	{ +0.0, +0.0 }, { -0.5, -0.5 }, { +0.0, -0.5 },
	{ +0.5, -0.5 },	{ -0.5, +0.0 }, { +0.5, +0.0 },
	{ -0.5, +0.5 }, { +0.0, +0.5 }, { +0.5, +0.5 }
};

static const vec_t sample_weights[MAX_SAMPLES] = {
	0.195346, 0.077847, 0.123317,
	0.077847, 0.123317, 0.123317,
	0.077847, 0.123317, 0.077847
};

/**
 * @brief Populates face_extents for all d_bsp_face_t, prior to light creation.
 * This is done so that sample positions may be nudged outward along
 * the face normal and towards the face center to help with traces.
 */
void BuildFaceExtents(void) {
	const bsp_vertex_t *v;
	int32_t i, j, k;

	memset(face_extents, 0, sizeof(face_extents));
	memset(face_lighting, 0, sizeof(face_lighting));

	for (k = 0; k < bsp_file.num_faces; k++) {

		const bsp_face_t *s = &bsp_file.faces[k];
		const bsp_texinfo_t *tex = &bsp_file.texinfo[s->texinfo];
		const size_t face_index = (ptrdiff_t) (s - bsp_file.faces);

		vec_t *mins = face_extents[face_index].mins;
		vec_t *maxs = face_extents[face_index].maxs;

		vec_t *center = face_extents[face_index].center;

		vec_t *st_mins = face_extents[face_index].st_mins;
		vec_t *st_maxs = face_extents[face_index].st_maxs;

		VectorSet(mins, 999999, 999999, 999999);
		VectorSet(maxs, -999999, -999999, -999999);

		st_mins[0] = st_mins[1] = 999999;
		st_maxs[0] = st_maxs[1] = -999999;

		for (i = 0; i < s->num_edges; i++) {
			const int32_t e = bsp_file.face_edges[s->first_edge + i];
			if (e >= 0) {
				v = bsp_file.vertexes + bsp_file.edges[e].v[0];
			} else {
				v = bsp_file.vertexes + bsp_file.edges[-e].v[1];
			}

			for (j = 0; j < 3; j++) { // calculate mins, maxs
				if (v->point[j] > maxs[j]) {
					maxs[j] = v->point[j];
				}
				if (v->point[j] < mins[j]) {
					mins[j] = v->point[j];
				}
			}

			for (j = 0; j < 2; j++) { // calculate st_mins, st_maxs
				const vec_t val = DotProduct(v->point, tex->vecs[j]) + tex->vecs[j][3];
				if (val < st_mins[j]) {
					st_mins[j] = val;
				}
				if (val > st_maxs[j]) {
					st_maxs[j] = val;
				}
			}
		}

		for (i = 0; i < 3; i++) { // calculate center
			center[i] = (mins[i] + maxs[i]) / 2.0;
		}
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
			const face_extents_t *extents = &face_extents[vert_faces[j]];
			VectorSubtract(extents->maxs, extents->mins, delta);

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
 * @brief Fills in l->tex_mins, l->tex_size[]
 */
static void CalcLightInfoExtents(light_info_t *l) {

	const vec_t *st_mins = face_extents[l->face - bsp_file.faces].st_mins;
	const vec_t *st_maxs = face_extents[l->face - bsp_file.faces].st_maxs;

	for (int32_t i = 0; i < 2; i++) {

		l->tex_mins[i] = floorf(st_mins[i] * lightmap_scale);
		l->tex_maxs[i] = ceilf(st_maxs[i] * lightmap_scale);

		l->tex_size[i] = l->tex_maxs[i] - l->tex_mins[i];
	}

	l->num_luxels = (l->tex_size[0] + 1) * (l->tex_size[1] + 1);

	// if a surface lightmap is too large to fit in a single lightmap block, we must fail here
	// practically speaking, this will never happen, as long as the BSP is subdivided
	if (l->num_luxels > MAX_BSP_LIGHTMAP) {
		const winding_t *w = WindingForFace(l->face);
		Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points,
						va("Surface too large to light (%dx%d)\n", l->tex_size[0], l->tex_size[1]));
	}
}

/**
 * @brief Fills in tex_org, world_to_tex. and tex_to_world
 */
static void CalcLightInfoVectors(light_info_t *l) {

	// determine the correct normal and distance for back-facing planes
	if (l->face->side) {
		VectorNegate(l->plane->normal, l->normal);
		l->dist = -l->plane->dist;
	} else {
		VectorCopy(l->plane->normal, l->normal);
		l->dist = l->plane->dist;
	}

	// get the origin offset for rotating bmodels
	VectorCopy(face_offsets[l->face - bsp_file.faces], l->model_org);

	const bsp_texinfo_t *tex = l->texinfo;

	// calculate a normal to the texture axis. points can be moved along this without changing their S/T
	vec3_t tex_normal;
	tex_normal[0] = tex->vecs[1][1] * tex->vecs[0][2] - tex->vecs[1][2] * tex->vecs[0][1];
	tex_normal[1] = tex->vecs[1][2] * tex->vecs[0][0] - tex->vecs[1][0] * tex->vecs[0][2];
	tex_normal[2] = tex->vecs[1][0] * tex->vecs[0][1] - tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize(tex_normal);

	// flip it towards plane normal
	vec_t dist_scale = DotProduct(tex_normal, l->normal);
	if (dist_scale == 0.0) {
		Com_Warn("Texture axis perpendicular to face\n");
		dist_scale = 1.0;
	}
	if (dist_scale < 0.0) {
		dist_scale = -dist_scale;
		VectorSubtract(vec3_origin, tex_normal, tex_normal);
	}

	// dist_scale is the ratio of the distance along the texture normal to
	// the distance along the plane normal
	dist_scale = 1.0 / dist_scale;

	for (int32_t i = 0; i < 2; i++) {
		const vec_t len = VectorLength(tex->vecs[i]);
		const vec_t distance = DotProduct(tex->vecs[i], l->normal) * dist_scale;
		VectorMA(tex->vecs[i], -distance, tex_normal, l->tex_to_world[i]);
		VectorScale(l->tex_to_world[i], (1 / len) * (1 / len), l->tex_to_world[i]);
	}

	// calculate tex_org on the texture plane
	for (int32_t i = 0; i < 3; i++) {
		l->tex_org[i] = -tex->vecs[0][3] * l->tex_to_world[0][i] - tex->vecs[1][3] * l->tex_to_world[1][i];
	}

	// project back to the face plane
	vec_t dist = DotProduct(l->tex_org, l->normal) - l->dist - 1.0;
	dist *= dist_scale;

	VectorMA(l->tex_org, -dist, tex_normal, l->tex_org);

	// compensate for org'd bmodels
	VectorAdd(l->tex_org, l->model_org, l->tex_org);
}

/**
 * @brief For Phong-shaded samples, calculate the interpolated normal vector using
 * linear interpolation between the nearest and farthest vertexes.
 */
static void PhongNormal(const bsp_face_t *face, const vec3_t pos, vec3_t normal) {
	vec_t total_dist = 0.0;
	vec_t dist[face->num_edges];

	// calculate the distance to each vertex
	for (int32_t i = 0; i < face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[face->first_edge + i];
		uint16_t v;

		if (e >= 0) {
			v = bsp_file.edges[e].v[0];
		} else {
			v = bsp_file.edges[-e].v[1];
		}

		vec3_t delta;
		VectorSubtract(pos, bsp_file.vertexes[v].point, delta);

		dist[i] = VectorLength(delta);
		total_dist += dist[i];
	}

	VectorSet(normal, 0.0, 0.0, 0.0);
	const vec_t max_dist = 2.0 * total_dist / face->num_edges;

	// add in weighted components from the vertex normals
	for (int32_t i = 0; i < face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[face->first_edge + i];
		uint16_t v;

		if (e >= 0) {
			v = bsp_file.edges[e].v[0];
		} else {
			v = bsp_file.edges[-e].v[1];
		}

		const vec_t mix = powf(max_dist - dist[i], 4.0) / powf(max_dist, 4.0);
		VectorMA(normal, mix, bsp_file.normals[v].normal, normal);

		// printf("%03.2f / %03.2f contributes %01.2f\n", dist[i], max_dist, mix);
	}

	VectorNormalize(normal);
}

/**
 * @brief For each texture aligned grid point, back project onto the plane to yield the origin.
 * Additionally calculate the per-sample normal vector, which may use Phong interpolation.
 */
static void BuildFaceLightingPoints(const light_info_t *l, vec_t *origins, vec_t *normals) {

	const int32_t h = l->tex_size[1] + 1;
	const int32_t w = l->tex_size[0] + 1;

	const int32_t step = 1.0 / lightmap_scale;

	const vec_t start_s = l->tex_mins[0] * step;
	const vec_t start_t = l->tex_mins[1] * step;

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++, origins += 3, normals += 3) {

			const vec_t us = start_s + s * step;
			const vec_t ut = start_t + t * step;

			for (int32_t j = 0; j < 3; j++) {
				origins[j] = l->tex_org[j] + l->tex_to_world[0][j] * us + l->tex_to_world[1][j] * ut;
			}

			if (l->texinfo->flags & SURF_PHONG) {
				PhongNormal(l->face, origins, normals);
			} else {
				VectorCopy(l->normal, normals);
			}
		}
	}
}

/**
 * @brief Allocates and populates the buffers that will contain origins, normals and lighting
 * results for the face referenced by the given light info.
 */
static face_lighting_t *BuildFaceLighting(const light_info_t *light) {

	face_lighting_t *fl = &face_lighting[light->face - bsp_file.faces];

	fl->num_luxels = light->num_luxels;

	fl->origins = Mem_TagMalloc(fl->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	fl->normals = Mem_TagMalloc(fl->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);

	BuildFaceLightingPoints(light, fl->origins, fl->normals);

	fl->direct = Mem_TagMalloc(fl->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	fl->directions = Mem_TagMalloc(fl->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	fl->indirect = Mem_TagMalloc(fl->num_luxels * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);

	return fl;
}

#define SAMPLE_NUDGE 0.25

/**
 * @brief Nudge the sample origin outward along the surface normal to reduce false-positive traces.
 * Test the PVS at the new position, returning true if the new point is valid, false otherwise.
 */
static _Bool NudgeSamplePosition(const light_info_t *l, const vec3_t origin, const vec3_t normal,
								 vec_t soffs, vec_t toffs, vec3_t out, byte *pvs) {

	VectorCopy(origin, out);

	if (soffs || toffs) {
		const int32_t step = 1.0 / lightmap_scale;

		const vec_t us = soffs * step;
		const vec_t ut = toffs * step;

		for (int32_t i = 0; i < 3; i++) {
			out[i] += l->tex_to_world[0][i] * us + l->tex_to_world[1][i] * ut;
		}
	}

	if (Light_PointPVS(out, pvs)) {
		return true;
	}

	VectorMA(out, SAMPLE_NUDGE, normal, out);

	if (Light_PointPVS(out, pvs)) {
		return true;
	}

	const vec_t *center = face_extents[l->face - bsp_file.faces].center;

	vec3_t delta;
	VectorSubtract(center, origin, delta);
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
				VectorMA(direction, diffuse * scale, sun->normal, direction);
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
						if (dot2 <= 0.0) {
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

/**
 * @brief
 */
void DirectLighting(int32_t face_num) {
	byte pvs[(MAX_BSP_LEAFS + 7) / 8];

	light_info_t light;
	memset(&light, 0, sizeof(light));

	light.face = &bsp_file.faces[face_num];
	light.plane = &bsp_file.planes[light.face->plane_num];
	light.texinfo = &bsp_file.texinfo[light.face->texinfo];

	if (light.texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	// calculate lightmap texture mins and maxs
	CalcLightInfoExtents(&light);

	// and the lighting vectors
	CalcLightInfoVectors(&light);

	// build the face lighting struct, which will outlive direct lighting
	face_lighting_t *fl = BuildFaceLighting(&light);

	// for each luxel, peek our head out into the world and attempt to gather light
	for (size_t i = 0; i < fl->num_luxels; i++) {

		const vec_t *origin = fl->origins + i * 3;
		const vec_t *normal = fl->normals + i * 3;

		vec_t *direct = fl->direct + i * 3;
		vec_t *direction = fl->directions + i * 3;

		const size_t num_samples = antialias ? MAX_SAMPLES : 1;

		for (size_t j = 0; j < num_samples; j++) {

			// shift the sample origins by weighted offsets
			const vec_t soffs = sample_offsets[j][0];
			const vec_t toffs = sample_offsets[j][1];

			vec3_t pos;

			if (!NudgeSamplePosition(&light, origin, normal, soffs, toffs, pos, pvs)) {
				continue;
			}

			const vec_t scale = antialias ? sample_weights[j] : 1.0;

			// gather lighting from direct light sources
			GatherSampleLight(pos, normal, pvs, direct, direction, scale);

			// including all configured suns
			GatherSampleSunlight(pos, normal, direct, direction, scale);
		}
	}
}

/**
 * @brief
 */
void IndirectLighting(int32_t face_num) {
	byte pvs[(MAX_BSP_LEAFS + 7) / 8];

	light_info_t light;
	memset(&light, 0, sizeof(light));

	light.face = &bsp_file.faces[face_num];
	light.plane = &bsp_file.planes[light.face->plane_num];
	light.texinfo = &bsp_file.texinfo[light.face->texinfo];

	if (light.texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	// calculate lightmap texture mins and maxs
	CalcLightInfoExtents(&light);

	// and the lighting vectors
	CalcLightInfoVectors(&light);

	face_lighting_t *fl = &face_lighting[face_num];

	// for each luxel, peek our head out into the world and attempt to gather light
	for (size_t i = 0; i < fl->num_luxels; i++) {

		const vec_t *origin = fl->origins + i * 3;
		const vec_t *normal = fl->normals + i * 3;

		vec_t *indirect = fl->indirect + i * 3;

		vec3_t pos;

		if (!NudgeSamplePosition(&light, origin, normal, 0.0, 0.0, pos, pvs)) {
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

	face_lighting_t *fl = &face_lighting[face_num];

	f->light_ofs = bsp_file.lightmap_data_size;
	bsp_file.lightmap_data_size += fl->num_luxels * lightmap_color_channels;

	if (!legacy) { // account for light direction data as well
		bsp_file.lightmap_data_size += fl->num_luxels * lightmap_color_channels;
	}

	if (bsp_file.lightmap_data_size > MAX_BSP_LIGHTING) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTING\n");
	}

	ThreadUnlock();

	// write it out
	byte *dest = &bsp_file.lightmap_data[f->light_ofs];

	for (size_t i = 0; i < fl->num_luxels; i++) {
		vec3_t lightmap;
		vec4_t hdr_lightmap;

		const vec_t *direct = fl->direct + i * 3;
		const vec_t *indirect = fl->indirect + i * 3;

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
			VectorCopy((fl->directions + i * 3), direction);

			// if the sample was lit, it will have a directional vector in world space
			if (!VectorCompare(direction, vec3_origin)) {

				const vec_t *normal = fl->normals + i * 3;

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
