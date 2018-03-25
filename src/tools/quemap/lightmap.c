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

lightmap_t lightmaps[MAX_BSP_FACES];

/**
 * @brief Resolves the extents, in world space and texture space, for the given lightmap.
 */
static void BuildLightmapExtents(lightmap_t *lm) {

	ClearBounds(lm->mins, lm->maxs);

	lm->st_mins[0] = lm->st_mins[1] = FLT_MAX;
	lm->st_maxs[0] = lm->st_maxs[1] = -FLT_MAX;

	for (int32_t i = 0; i < lm->face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[lm->face->first_edge + i];
		const bsp_vertex_t *v;

		if (e >= 0) {
			v = bsp_file.vertexes + bsp_file.edges[e].v[0];
		} else {
			v = bsp_file.vertexes + bsp_file.edges[-e].v[1];
		}

		for (int32_t k = 0; k < 3; k++) {
			if (v->point[k] > lm->maxs[k]) {
				lm->maxs[k] = v->point[k];
			}
			if (v->point[k] < lm->mins[k]) {
				lm->mins[k] = v->point[k];
			}
		}

		ddvec3_t point;
		VectorCopy(v->point, point);

		for (int32_t j = 0; j < 2; j++) {
			const vec_t val = DotProduct(point, lm->texinfo->vecs[j]) + lm->texinfo->vecs[j][3];
			if (val < lm->st_mins[j]) {
				lm->st_mins[j] = val;
			}
			if (val > lm->st_maxs[j]) {
				lm->st_maxs[j] = val;
			}
		}
	}

	for (int32_t i = 0; i < 2; i++) {
		lm->lm_mins[i] = floorf(lm->st_mins[i] * lightmap_scale);
		lm->lm_maxs[i] = ceilf(lm->st_maxs[i] * lightmap_scale);
	}

	lm->width = lm->lm_maxs[0] - lm->lm_mins[0] + 1;
	lm->height = lm->lm_maxs[1] - lm->lm_mins[1] + 1;
}

/**
 * @brief Resolves the texture projection matrices for the given lightmap.
 */
static void BuildLightmapMatrices(lightmap_t *lm) {

	vec_t dist;
	if (lm->face->side) {
		VectorNegate(lm->plane->normal, lm->normal);
		dist = -lm->plane->dist;
	} else {
		VectorCopy(lm->plane->normal, lm->normal);
		dist = lm->plane->dist;
	}

	Matrix4x4_FromArrayFloatGL(&lm->world_to_tex, (const vec_t[]) {
		lm->texinfo->vecs[0][0], lm->texinfo->vecs[1][0], lm->normal[0], 0.0,
		lm->texinfo->vecs[0][1], lm->texinfo->vecs[1][1], lm->normal[1], 0.0,
		lm->texinfo->vecs[0][2], lm->texinfo->vecs[1][2], lm->normal[2], 0.0,
		lm->texinfo->vecs[0][3], lm->texinfo->vecs[1][3], -dist, 1.0
	});

	Matrix4x4_Invert_Full(&lm->tex_to_world, &lm->world_to_tex);
}

/**
 * @brief Allocates and seeds the luxels for the given lightmap. Projection of the luxels into world
 * space is handled individually by the actual lighting passes, as they have different projections.
 */
static void BuildLightmapLuxels(lightmap_t *lm) {

	lm->num_luxels = lm->width * lm->height;

	if (lm->num_luxels > MAX_BSP_LIGHTMAP) {
		const winding_t *w = WindingForFace(lm->face);
		Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points,
						va("Surface too large to light (%zd)\n", lm->num_luxels));
	}

	lm->luxels = Mem_TagMalloc(sizeof(luxel_t) * lm->num_luxels, MEM_TAG_LIGHTMAP);

	luxel_t *l = lm->luxels;

	for (int32_t t = 0; t < lm->height; t++) {
		for (int32_t s = 0; s < lm->width; s++, l++) {

			l->s = s;
			l->t = t;

			VectorCopy(lm->normal, l->direction);
		}
	}
}

static _Bool ProjectLuxel(const lightmap_t *lm, luxel_t *l, vec_t soffs, vec_t toffs, byte *pvs);

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
void DebugLightmapLuxels(void) {

	file_t *file = Fs_OpenWrite(va("maps/%s.luxels.map", map_base));
	if (file) {

		lightmap_t *lm = lightmaps;
		for (int32_t i = 0; i < bsp_file.num_faces; i++, lm++) {

			if (lm->texinfo->flags & SURF_DEBUG_LUXEL) {

				luxel_t *l = lm->luxels;
				for (size_t j = 0; j < lm->num_luxels; j++, l++) {

					byte pvs[(MAX_BSP_LEAFS + 7) / 8];

					ProjectLuxel(lm, l, 0.0, 0.0, pvs);

					Fs_Print(file, "{\n");
					Fs_Print(file, "  \"classname\" \"info_luxel\"\n");
					Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin[0], l->origin[1], l->origin[2]);
					Fs_Print(file, "  \"normal\" \"%g %g %g\"\n", l->normal[0], l->normal[1], l->normal[2]);
					Fs_Print(file, "  \"face\" \"%d\"\n", i);
					Fs_Print(file, "  \"s\" \"%d\"\n", l->s);
					Fs_Print(file, "  \"t\" \"%d\"\n", l->t);
					Fs_Print(file, "}\n");
				}
			}
		}

		Fs_Close(file);
	}
}

/**
 * @brief Builds a lightmap for each face in the BSP.
 */
void BuildLightmaps(void) {

	memset(lightmaps, 0, sizeof(lightmaps));

	lightmap_t *lm = lightmaps;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, lm++) {

		lm->face = &bsp_file.faces[i];
		lm->texinfo = &bsp_file.texinfo[lm->face->texinfo];
		lm->plane = &bsp_file.planes[lm->face->plane_num];

		if (lm->texinfo->flags & (SURF_SKY | SURF_WARP)) {
			continue;
		}

		VectorCopy(face_offsets[i], lm->offset);

		BuildLightmapMatrices(lm);

		BuildLightmapExtents(lm);

		BuildLightmapLuxels(lm);
	}

	DebugLightmapLuxels();
}

#define MAX_VERT_FACES 256

/**
 * @brief Populate faces with indexes of all Phong-shaded bsp_face_t's referencing the vertex.
 * @return The number of bsp_face_t's referencing the vertex.
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
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by finding all of
 * the faces which share a given vertex, and calculating a weighted average of their normals.
 */
void BuildVertexNormals(void) {
	int32_t vert_faces[MAX_VERT_FACES];

	for (int32_t i = 0; i < bsp_file.num_vertexes; i++) {

		vec_t *normal = bsp_file.normals[i].normal;
		VectorClear(normal);

		const int32_t count = FacesWithVert(i, vert_faces);
		if (count) {

			for (int32_t j = 0; j < count; j++) {
				const lightmap_t *lm = &lightmaps[vert_faces[j]];
				VectorMA(normal, lm->num_luxels, lm->normal, normal);
			}

			VectorNormalize(normal);
		}
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
 * @brief
 */
static _Bool ProjectLuxel(const lightmap_t *lm, luxel_t *l, vec_t soffs, vec_t toffs, byte *pvs) {

	const vec_t mid_s = (lm->width * 0.5);
	const vec_t mid_t = (lm->height * 0.5);

	vec_t ds = lm->lm_mins[0] + l->s + soffs;
	vec_t dt = lm->lm_mins[1] + l->t + toffs;

	ds += 0.5 * (1.0 - l->s / mid_s);
	dt += 0.5 * (1.0 - l->t / mid_t);

	const vec3_t dst = { ds / lightmap_scale, dt / lightmap_scale, 0.0 };
	Matrix4x4_Transform(&lm->tex_to_world, dst, l->origin);

	if (lm->texinfo->flags & SURF_PHONG) {
		PhongNormal(lm->face, l->origin, l->normal);
	} else {
		VectorCopy(lm->normal, l->normal);
	}

	VectorAdd(l->origin, l->normal, l->origin);
	VectorAdd(l->origin, lm->offset, l->origin);

	return Light_PointPVS(l->origin, pvs);
}

/**
 * @brief Iterates all sun light sources, tracing towards them and adding light if a sky was found.
 */
static void SunlightLuxel(const luxel_t *l, vec_t *sample, vec_t *direction, vec_t scale) {

	const sun_t *sun = suns;
	for (size_t i = 0; i < num_suns; i++, sun++) {

		const vec_t dot = DotProduct(sun->normal, l->normal);
		if (dot <= 0.0) {
			continue;
		}

		vec3_t end;
		VectorMA(l->origin, MAX_WORLD_DIST, sun->normal, end);

		cm_trace_t trace;
		Light_Trace(&trace, l->origin, end, CONTENTS_SOLID);

		if (trace.fraction < 1.0 && (trace.surface->flags & SURF_SKY)) {

			const vec_t diffuse = sun->light * dot * DEFAULT_PATCH_SIZE;

			if (sample) {
				VectorMA(sample, diffuse * scale, sun->color, sample);
			}

			if (direction) {
				VectorMA(direction, diffuse * scale, sun->normal, direction);
			}
		}
	}
}

/**
 * @brief Iterate over all light sources for the luxel's PVS, accumulating diffuse and directional information to the specified pointers.
 */
static void LightLuxel(const luxel_t *l, const byte *pvs, vec_t *sample, vec_t *direction, vec_t scale) {

	for (int32_t cluster = 0; cluster < bsp_file.vis_data.vis->num_clusters; cluster++) {

		if (!(pvs[cluster >> 3] & (1 << (cluster & 7)))) {
			continue;
		}

		for (const light_t *light = lights[cluster]; light; light = light->next) {

			vec3_t dir;
			VectorSubtract(light->origin, l->origin, dir);

			const vec_t dist = VectorNormalize(dir);
			if (dist > light->radius) {
				continue;
			}

			const vec_t dot = DotProduct(dir, l->normal);
			if (dot <= 0.0) {
				continue;
			}

			vec_t diffuse = Clamp(light->radius * dot, 0.0, 255.0);;

			switch (light->type) {

				case LIGHT_PATCH:
					diffuse *= patch_size / DEFAULT_PATCH_SIZE;
					break;

				case LIGHT_POINT:
					diffuse *= DEFAULT_PATCH_SIZE;
					break;

				case LIGHT_SPOT: {
					const vec_t dot2 = DotProduct(dir, light->normal);
					if (dot2 <= light->cone) {
						if (dot2 <= 0.1) {
							diffuse = 0.0;
						} else {
							diffuse *= light->cone - (1.0 - dot2);
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

			const vec_t atten = Clamp(1.0 - dist / light->radius, 0.0, 1.0);
			const vec_t atten_squared = atten * atten;

			diffuse *= atten_squared;

			cm_trace_t trace;
			Light_Trace(&trace, light->origin, l->origin, CONTENTS_SOLID);

			if (trace.fraction < 1.0) {
				continue;
			}

			if (sample) {
				VectorMA(sample, diffuse * scale, light->color, sample);
			}

			if (direction) {
				VectorMA(direction, diffuse * scale / light->radius, dir, direction);
			}
		}
	}
}

/**
 * @brief Calculates direct lighting for the given face. Luxels are projected into world space.
 * We then query the light sources that are in PVS for each luxel, and accumulate their diffuse
 * and directional contributions as non-normalized floating point.
 */
void DirectLighting(int32_t face_num) {

	static const vec3_t offsets[] = {
		{ +0.0, +0.0, 0.195346 }, { -0.5, -0.5, 0.077847 }, { +0.0, -0.5, 0.123317 },
		{ +0.5, -0.5, 0.077847 }, { -0.5, +0.0, 0.123317 }, { +0.5, +0.0, 0.123317 },
		{ -0.5, +0.5, 0.077847 }, { +0.0, +0.5, 0.123317 }, { +0.5, +0.5, 0.077847 }
	};

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		size_t num_samples = antialias ? lengthof(offsets) : 1, valid_samples = 0;

		for (size_t j = 0; j < num_samples; j++) {

			const vec_t soffs = offsets[j][0];
			const vec_t toffs = offsets[j][1];
			const vec_t scale = offsets[j][2];

			byte pvs[(MAX_BSP_LEAFS + 7) / 8];

			if (!ProjectLuxel(lm, l, soffs, toffs, pvs)) {
				continue;
			}

			valid_samples++;

			LightLuxel(l, pvs, l->direct, l->direction, antialias ? scale : 1.0);

			SunlightLuxel(l, l->direct, l->direction, antialias ? scale : 1.0);
		}

		if (valid_samples > 0 && valid_samples < num_samples) {

			const vec_t rescale = 1.0 / ((vec_t) valid_samples / num_samples);

			VectorScale(l->direct, rescale, l->direct);
			VectorScale(l->direction, rescale, l->direction);
		}
	}
}

/**
 * @brief
 */
void IndirectLighting(int32_t face_num) {

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		byte pvs[(MAX_BSP_LEAFS + 7) / 8];

		if (!ProjectLuxel(lm, l, 0.0, 0.0, pvs)) {
			continue;
		}

		LightLuxel(l, pvs, l->indirect, NULL, 0.0125);
	}
}

/**
 * @brief Finalize and write the lightmap data for the given face.
 */
void FinalizeLighting(int32_t face_num) {

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & (SURF_WARP | SURF_SKY)) {
		return;
	}

	const int32_t lightmap_color_channels = (legacy ? 3 : 4);

	bsp_face_t *f = &bsp_file.faces[face_num];

	f->unused[0] = 0; // pack the old lightstyles array for legacy games
	f->unused[1] = f->unused[2] = f->unused[3] = 255;

	ThreadLock();

	f->light_ofs = bsp_file.lightmap_data_size;
	bsp_file.lightmap_data_size += lm->num_luxels * lightmap_color_channels;

	if (!legacy) { // account for light direction data as well
		bsp_file.lightmap_data_size += lm->num_luxels * lightmap_color_channels;
	}

	if (bsp_file.lightmap_data_size > MAX_BSP_LIGHTING) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTING\n");
	}

	ThreadUnlock();

	// write it out
	byte *dest = &bsp_file.lightmap_data[f->light_ofs];

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {
		vec3_t lightmap;
		vec4_t hdr_lightmap;

		// start with raw sample data
		VectorAdd(l->direct, l->indirect, lightmap);
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
			VectorCopy(l->direction, direction);

			// if the sample was lit, it will have a directional vector in world space
			if (!VectorCompare(direction, vec3_origin)) {

				vec4_t tangent;
				vec3_t bitangent;

				TangentVectors(l->normal, lm->texinfo->vecs[0], lm->texinfo->vecs[1], tangent, bitangent);

				// transform it into tangent space
				deluxemap[0] = DotProduct(direction, tangent);
				deluxemap[1] = DotProduct(direction, bitangent);
				deluxemap[2] = DotProduct(direction, l->normal);

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
