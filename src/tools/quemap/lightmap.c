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
#include "light.h"
#include "lightmap.h"
#include "patch.h"
#include "points.h"
#include "polylib.h"
#include "qlight.h"

lightmap_t *lightmaps;

/**
 * @brief Resolves the texture projection matrices for the given lightmap.
 */
static void BuildLightmapMatrices(lightmap_t *lm) {

	vec3_t s, t;
	VectorNormalize2(lm->texinfo->vecs[0], s);
	VectorNormalize2(lm->texinfo->vecs[1], t);

	VectorScale(s, 1.0 / luxel_size, s);
	VectorScale(t, 1.0 / luxel_size, t);

	Matrix4x4_FromArrayFloatGL(&lm->matrix, (const vec_t[]) {
		s[0], t[0], lm->plane->normal[0], 0.0,
		s[1], t[1], lm->plane->normal[1], 0.0,
		s[2], t[2], lm->plane->normal[2], 0.0,
		0.0,  0.0,  -lm->plane->dist,     1.0
	});

	const vec_t *origin = patches[lm - lightmaps].origin;

	matrix4x4_t translate;
	Matrix4x4_CreateTranslate(&translate, origin[0], origin[1], origin[2]);

	matrix4x4_t inverse;
	Matrix4x4_Invert_Full(&inverse, &lm->matrix);

	Matrix4x4_Concat(&lm->inverse_matrix, &translate, &inverse);
}

/**
 * @brief Resolves the extents, in texture space, for the given lightmap.
 */
static void BuildLightmapExtents(lightmap_t *lm) {

	ClearStBounds(lm->st_mins, lm->st_maxs);

	const bsp_vertex_t *v = &bsp_file.vertexes[lm->face->first_vertex];
	for (int32_t i = 0; i < lm->face->num_vertexes; i++, v++) {

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, v->position, st);

		AddStToBounds(st, lm->st_mins, lm->st_maxs);
	}

	// add 4 luxels of padding around the lightmap for bicubic filtering
	lm->w = floorf(lm->st_maxs[0] - lm->st_mins[0]) + 4;
	lm->h = floorf(lm->st_maxs[1] - lm->st_mins[1]) + 4;
}

/**
 * @brief Allocates and seeds the luxels for the given lightmap. Projection of the luxels into world
 * space is handled individually by the actual lighting passes, as they have different projections.
 */
static void BuildLightmapLuxels(lightmap_t *lm) {

	lm->num_luxels = lm->w * lm->h;
	lm->luxels = Mem_TagMalloc(lm->num_luxels * sizeof(luxel_t), MEM_TAG_LIGHTMAP);

	luxel_t *l = lm->luxels;

	for (int32_t t = 0; t < lm->h; t++) {
		for (int32_t s = 0; s < lm->w; s++, l++) {

			l->s = s;
			l->t = t;
		}
	}
}

static int32_t ProjectLuxel(const lightmap_t *lm, luxel_t *l, vec_t soffs, vec_t toffs);

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
void DebugLightmapLuxels(void) {

	file_t *file = NULL;

	lightmap_t *lm = lightmaps;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, lm++) {

		if (lm->texinfo->flags & SURF_DEBUG_LUXEL) {

			if (file == NULL) {
				const char *path = va("maps/%s.luxels.map", map_base);
				file = Fs_OpenWrite(path);
				if (file == NULL) {
					Com_Warn("Failed to open %s\n", path);
					return;
				}
			}

			luxel_t *l = lm->luxels;
			for (size_t j = 0; j < lm->num_luxels; j++, l++) {

				ProjectLuxel(lm, l, 0.0, 0.0);

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

	if (file) {
		Fs_Close(file);
	}
}

/**
 * @brief Builds a lightmap for each face in the BSP.
 */
void BuildLightmaps(void) {

	lightmaps = Mem_TagMalloc(sizeof(lightmap_t) * bsp_file.num_faces, MEM_TAG_LIGHTMAP);

	bsp_draw_elements_t *draw = bsp_file.draw_elements;
	for (int32_t i = 0; i < bsp_file.num_draw_elements; i++, draw++) {
		for (int32_t j = 0; j < draw->num_faces; j++) {
			lightmaps[draw->first_face + j].draw = draw;
		}
	}

	const bsp_leaf_t *leaf = bsp_file.leafs;
	for (int32_t i = 0; i < bsp_file.num_leafs; i++, leaf++) {
		const int32_t *leaf_face = bsp_file.leaf_faces + leaf->first_leaf_face;
		for (int32_t j = 0; j < leaf->num_leaf_faces; j++, leaf_face++) {
			lightmaps[*leaf_face].leaf = leaf;
		}
	}

	const bsp_model_t *model = bsp_file.models;
	for (int32_t i = 0; i < bsp_file.num_models; i++, model++) {
		for (int32_t j = 0; j < model->num_faces; j++) {
			lightmaps[model->first_face + j].model = model;
		}
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		lightmap_t *lm = &lightmaps[i];

		assert(lm->leaf);
		assert(lm->draw);

		lm->face = &bsp_file.faces[i];
		lm->texinfo = &bsp_file.texinfo[lm->face->texinfo];
		lm->plane = &bsp_file.planes[lm->face->plane_num];

		if (lm->texinfo->flags & SURF_SKY) {
			continue;
		}

		lm->material = LoadMaterial(lm->texinfo->texture, ASSET_CONTEXT_TEXTURES);

		BuildLightmapMatrices(lm);

		BuildLightmapExtents(lm);

		BuildLightmapLuxels(lm);
	}

	DebugLightmapLuxels();
}

/**
 * @brief For Phong-shaded luxels, calculate the interpolated normal vector using
 * barycentric coordinates over the face's triangles.
 */
static void PhongLuxel(const lightmap_t *lm, const vec3_t origin, vec3_t normal) {

	VectorCopy(lm->plane->normal, normal);

	vec_t best = FLT_MAX;

	const int32_t *e = bsp_file.elements + lm->face->first_element;
	for (int32_t i = 0; i < lm->face->num_elements; i++, e += 3) {

		const bsp_vertex_t *a = bsp_file.vertexes + e[0];
		const bsp_vertex_t *b = bsp_file.vertexes + e[1];
		const bsp_vertex_t *c = bsp_file.vertexes + e[2];

		vec3_t out;
		const vec_t bary = BarycentricCoordinates(a->position, b->position, c->position, origin, out);
		const vec_t delta = fabsf(1.0f - bary);
		if (delta < best) {
			best = delta;

			VectorClear(normal);

			VectorMA(normal, out[0], a->normal, normal);
			VectorMA(normal, out[1], b->normal, normal);
			VectorMA(normal, out[2], c->normal, normal);
		}
	}

	VectorNormalize(normal);
}

/**
 * @brief Projects the luxel for the given lightmap into world space.
 * @param lm The lightmap.
 * @param l The luxel.
 * @param soffs The S offset in texture space, for antialiasing.
 * @param toffs The T offset in texture space, for antialiasing.
 * @param pvs A pointer to receive the PVS data for the projected luxel origin.
 * @return The contents mask at the projected luxel origin.
 */
static int32_t ProjectLuxel(const lightmap_t *lm, luxel_t *l, vec_t soffs, vec_t toffs) {

	const vec_t padding_s = ((lm->st_maxs[0] - lm->st_mins[0]) - lm->w) * 0.5;
	const vec_t padding_t = ((lm->st_maxs[1] - lm->st_mins[1]) - lm->h) * 0.5;

	const vec_t s = lm->st_mins[0] + padding_s + l->s + 0.5 + soffs;
	const vec_t t = lm->st_mins[1] + padding_t + l->t + 0.5 + toffs;

	const vec3_t st = { s , t, 0.0 };
	Matrix4x4_Transform(&lm->inverse_matrix, st, l->origin);

	if (lm->texinfo->flags & SURF_PHONG) {
		PhongLuxel(lm, l->origin, l->normal);
	} else {
		VectorCopy(lm->plane->normal, l->normal);
	}

	VectorAdd(l->origin, l->normal, l->origin);
	return Light_PointContents(l->origin, lm->model->head_node);
}

/**
 * @brief Iterates all lights, accumulating color and direction to the appropriate buffers.
 * @param lightmap The lightmap containing the luxel.
 * @param luxel The luxel to light.
 * @param pvs The PVS for the luxel's origin.
 * @param scale A scalar applied to both light and direction.
 */
static void LightLuxel(const lightmap_t *lightmap, luxel_t *luxel, const byte *pvs, vec_t scale) {

	for (const GList *list = lights; list; list = list->next) {
		const light_t *light = list->data;

		if (light->type == LIGHT_INVALID) {
			continue;
		}

		if (light->cluster != -1) {
			if (!(pvs[light->cluster >> 3] & (1 << (light->cluster & 7)))) {
				continue;
			}
		}

		vec3_t dir;
		vec_t dist;
		if (light->type == LIGHT_AMBIENT) {
			VectorSet(dir, 0.0, 0.0, 1.0);
			dist = 0.0;
		} else if (light->type == LIGHT_SUN) {
			VectorNegate(light->normal, dir);
			dist = 0.0;
		} else {
			VectorSubtract(light->origin, luxel->origin, dir);
			dist = VectorNormalize(dir);
		}

		if (light->atten != LIGHT_ATTEN_NONE) {
			if (dist > light->radius) {
				continue;
			}
		}

		vec_t dot;
		if (light->type == LIGHT_AMBIENT) {
			dot = 1.0;
		} else {
			dot = DotProduct(dir, luxel->normal);
			if (dot <= 0.0) {
				continue;
			}
		}

		vec_t intensity = Clamp(light->radius * dot, 0.0, LIGHT_RADIUS);

		switch (light->type) {
			case LIGHT_INVALID:
				break;
			case LIGHT_AMBIENT:
				break;
			case LIGHT_SUN:
				break;
			case LIGHT_POINT:
				intensity *= DEFAULT_BSP_PATCH_SIZE;
				break;
			case LIGHT_SPOT: {
				const vec_t cone_dot = DotProduct(dir, -light->normal);
				const vec_t thresh = cosf(light->theta);
				const vec_t smooth = 0.03;
				intensity *= SmoothStep(thresh - smooth, thresh + smooth, cone_dot);
				intensity *= DEFAULT_BSP_PATCH_SIZE;
			}
				break;
			case LIGHT_PATCH:
			case LIGHT_INDIRECT:
				intensity *= patch_size / DEFAULT_BSP_PATCH_SIZE;
				break;
		}

		vec_t atten = 1.0;

		switch (light->atten) {
			case LIGHT_ATTEN_NONE:
				break;
			case LIGHT_ATTEN_LINEAR:
				atten = Clamp(1.0 - dist / light->radius, 0.0, 1.0);
				break;
			case LIGHT_ATTEN_INVERSE_SQUARE:
				atten = Clamp(1.0 - dist / light->radius, 0.0, 1.0);
				atten *= atten;
				break;
		}

		intensity *= atten;

		if (intensity <= 0.0) {
			continue;
		}

		const int32_t head_node = lightmap->model->head_node;

		if (occlude_ambient && light->type == LIGHT_AMBIENT) {

			const vec_t padding_s = ((lightmap->st_maxs[0] - lightmap->st_mins[0]) - lightmap->w) * 0.5;
			const vec_t padding_t = ((lightmap->st_maxs[1] - lightmap->st_mins[1]) - lightmap->h) * 0.5;

			const vec_t s = lightmap->st_mins[0] + padding_s + luxel->s + 0.5;
			const vec_t t = lightmap->st_mins[1] + padding_t + luxel->t + 0.5;

			const vec3_t points[] = DOME_COSINE_36X;
			const vec_t ao_radius = 64.0;

			vec_t ambient_occlusion = 0.0;
			vec_t sample_fraction = 1.0 / lengthof(points);

			for (size_t i = 0; i < lengthof(points); i++) {

				vec3_t sample;
				VectorCopy(points[i], sample);

				// Add some jitter to hide undersampling
				VectorSet(sample,
					sample[0] + Randomc() * 0.04,
					sample[1] + Randomc() * 0.04,
					sample[2]);

				// Scale the sample and move it into position
				VectorSet(sample,
					sample[0] * ao_radius + s,
					sample[1] * ao_radius + t,
					sample[2] * ao_radius);

				vec3_t point;
				Matrix4x4_Transform(&lightmap->inverse_matrix, sample, point);

				const cm_trace_t trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);

				ambient_occlusion += sample_fraction * trace.fraction;
			}

			intensity *= 1.0 - (1.0 - ambient_occlusion) * (1.0 - ambient_occlusion);

		} else if (light->type == LIGHT_SUN) {

			vec3_t sun_origin;
			VectorMA(luxel->origin, -MAX_WORLD_DIST, light->normal, sun_origin);

			cm_trace_t trace = Light_Trace(luxel->origin, sun_origin, head_node, CONTENTS_SOLID);
			if (!(trace.surface && (trace.surface->flags & SURF_SKY))) {
				vec_t exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						vec3_t point;
						VectorMA(sun_origin, i * LIGHT_SIZE_STEP, points[j], point);

						trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);
						if (!(trace.surface && (trace.surface->flags & SURF_SKY))) {
							continue;
						}

						exposure += 1.0 / num_samples;
						break;
					}
				}

				intensity *= exposure;
			}

		} else {
			cm_trace_t trace = Light_Trace(luxel->origin, light->origin, head_node, CONTENTS_SOLID);
			if (trace.fraction < 1.0) {
				vec_t exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						vec3_t point;
						VectorMA(light->origin, (i + 1) * LIGHT_SIZE_STEP, points[j], point);

						trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);
						if (trace.fraction < 1.0) {
							continue;
						}

						exposure += 1.0 / num_samples;
						break;
					}
				}

				intensity *= exposure;
			}
		}

		intensity *= scale;

		if (intensity <= 0.0) {
			continue;
		}

		switch (light->type) {
			case LIGHT_INVALID:
				break;
			case LIGHT_AMBIENT:
				VectorMA(luxel->ambient, intensity, light->color, luxel->ambient);
				VectorMA(luxel->direction, intensity, luxel->normal, luxel->direction);
				break;
			case LIGHT_SUN:
			case LIGHT_POINT:
			case LIGHT_SPOT:
			case LIGHT_PATCH:
				VectorMA(luxel->diffuse, intensity, light->color, luxel->diffuse);
				VectorMA(luxel->direction, intensity, dir, luxel->direction);
				break;
			case LIGHT_INDIRECT:
				VectorMA(luxel->radiosity, intensity, light->color, luxel->radiosity);
				VectorMA(luxel->direction, intensity, luxel->normal, luxel->direction);
				break;
		}
	}
}

/**
 * @brief Calculates direct lighting for the given face. Luxels are projected into world space.
 * We then query the light sources that are in PVS for each luxel, and accumulate their diffuse
 * and directional contributions as non-normalized floating point.
 */
void DirectLightmap(int32_t face_num) {

	static const vec3_t offsets[] = {
		{ +0.0, +0.0, 0.195346 }, { -1.0, -1.0, 0.077847 }, { +0.0, -1.0, 0.123317 },
		{ +1.0, -1.0, 0.077847 }, { -1.0, +0.0, 0.123317 }, { +1.0, +0.0, 0.123317 },
		{ -1.0, +1.0, 0.077847 }, { +0.0, +1.0, 0.123317 }, { +1.0, +1.0, 0.077847 }
	};

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_SKY) {
		return;
	}

	byte pvs[MAX_BSP_LEAFS >> 3];
	Light_ClusterPVS(lm->leaf->cluster, pvs);

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		vec_t contribution = 0.0;

		for (size_t j = 0; j < lengthof(offsets); j++) {

			const vec_t soffs = offsets[j][0];
			const vec_t toffs = offsets[j][1];
			const vec_t scale = offsets[j][2];

			if (ProjectLuxel(lm, l, soffs, toffs) == CONTENTS_SOLID) {
				continue;
			}

			contribution += scale;

			LightLuxel(lm, l, pvs, scale);

			if (!antialias) {
				break;
			}
		}

		if (contribution > 0.0 && contribution < 1.0) {
			VectorScale(l->ambient, 1.0 / contribution, l->ambient);
			VectorScale(l->diffuse, 1.0 / contribution, l->diffuse);
			VectorScale(l->direction, 1.0 / contribution, l->direction);
		}
	}
}

/**
 * @brief Calculates indirect lighting for the given face.
 */
void IndirectLightmap(int32_t face_num) {

	static const vec2_t offsets[] = {
		{ +0.0, +0.0 }, { -1.0, -1.0 }, { +0.0, -1.0 },
		{ +1.0, -1.0 }, { -1.0, +0.0 }, { +1.0, +0.0 },
		{ -1.0, +1.0 }, { +0.0, +1.0 }, { +1.0, +1.0 }
	};

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_SKY) {
		return;
	}

	byte pvs[MAX_BSP_LEAFS >> 3];
	Light_ClusterPVS(lm->leaf->cluster, pvs);

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		for (size_t j = 0; j < lengthof(offsets); j++) {

			const vec_t soffs = offsets[j][0];
			const vec_t toffs = offsets[j][1];

			if (ProjectLuxel(lm, l, soffs, toffs) == CONTENTS_SOLID) {
				continue;
			}

			LightLuxel(lm, l, pvs, radiosity);
			break;
		}
	}
}

/**
 * @brief
 */
static SDL_Surface *CreateLightmapSurface(int32_t w, int32_t h, void *pixels) {
	return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 24, w * BSP_LIGHTMAP_BPP, SDL_PIXELFORMAT_RGB24);
}

/**
 * @brief Blurs the ambient occlusion to get rid of noise
 */
static void BlurAmbient(lightmap_t *lm) {

	// temporary buffer
	vec3_t *tmp = Mem_TagMalloc(lm->w * lm->h * sizeof(vec3_t), MEM_TAG_LIGHTMAP);

	for (int y = 0; y < lm->h; y++) {
		for (int x = 0; x < lm->w; x++) {

			vec3_t luxel = { 0.0, 0.0, 0.0 };

			static const vec3_t offsets[] = {
				{ +0.0, +0.0, 0.195346 }, { -1.0, -1.0, 0.077847 }, { +0.0, -1.0, 0.123317 },
				{ +1.0, -1.0, 0.077847 }, { -1.0, +0.0, 0.123317 }, { +1.0, +0.0, 0.123317 },
				{ -1.0, +1.0, 0.077847 }, { +0.0, +1.0, 0.123317 }, { +1.0, +1.0, 0.077847 }
			};

			vec_t valid_weight = 1.0; // to avoid loss of brightness near edges

			// sum samples into luxel
			for (size_t i = 0; i < lengthof(offsets); i++) {

				int32_t x_off = offsets[i][0];
				int32_t y_off = offsets[i][1];
				int32_t x_pos = x + x_off;
				int32_t y_pos = y + y_off;

				// is the sample out of bounds?
				if (x_pos < 0 || x_pos >= lm->w || y_pos < 0 || y_pos >= lm->h) {
					// is the offset orthogonal or diagonal?
					// remove the correct amount of total weight
					if (abs(x_off) == abs(y_off)) {
						valid_weight -= 0.077847;
					} else {
						valid_weight -= 0.123317;
					}
					continue;
				}

				// grab the valid sample
				luxel_t *sample_luxel = &lm->luxels[y_pos * lm->w + x_pos];
				vec3_t sample_value;
				vec_t  sample_weight = offsets[i][2];
				VectorCopy(sample_luxel->ambient, sample_value);
				// scale the sample by it's gaussian weight
				VectorScale(sample_value, sample_weight, sample_value);
				// sum the sample with the luxel
				VectorAdd(luxel, sample_value, luxel);

			}

			// compensate for invalid samples
			VectorScale(luxel, 1.0 / valid_weight, luxel);

			// write the luxel to the temp buffer
			VectorCopy(luxel, tmp[y * lm->w + x]);
		}
	}

	// copy back results from temp buffer
	for (size_t i = 0; i < lm->num_luxels; i++) {
		VectorCopy(tmp[i], lm->luxels[i].ambient);
	}
}

/**
 * @brief Finalize light values for the given face, and create its lightmap textures.
 */
void FinalizeLightmap(int32_t face_num) {

	lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_SKY) {
		return;
	}

	if (occlude_ambient) {
		// TODO: this bleeds a bit around the edges, but it's usually unnoticable.
		BlurAmbient(lm);
	}

	lm->lightmap = CreateLightmapSurface(lm->w, lm->h, Mem_TagMalloc(lm->w * lm->h * 3, MEM_TAG_LIGHTMAP));
	byte *out_lm = lm->lightmap->pixels;

	lm->deluxemap = CreateLightmapSurface(lm->w, lm->h, Mem_TagMalloc(lm->w * lm->h * 3, MEM_TAG_LIGHTMAP));
	byte *out_dm = lm->deluxemap->pixels;

	// write it out
	const luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {
		vec3_t lightmap, deluxemap;

		// add all color components together
		VectorClear(lightmap);
		VectorAdd(lightmap, l->ambient, lightmap);
		VectorAdd(lightmap, l->diffuse, lightmap);
		VectorAdd(lightmap, l->radiosity, lightmap);

		// convert to float
		VectorScale(lightmap, 1.0 / 255.0, lightmap);

		// apply brightness, saturation and contrast
		ColorFilter(lightmap, lightmap, brightness, saturation, contrast);

		// write the lightmap sample data as bytes
		for (int32_t j = 0; j < 3; j++) {
			*out_lm++ = (byte) Clamp(lightmap[j] * 255.0, 0, 255);
		}

		// write the deluxemap sample data, in tangent space
		if (!VectorCompare(l->direction, vec3_origin)) {

			vec3_t tangent;
			vec3_t bitangent;

			TangentVectors(l->normal, lm->texinfo->vecs[0], lm->texinfo->vecs[1], tangent, bitangent);

			// transform it into tangent space
			deluxemap[0] = DotProduct(l->direction, tangent);
			deluxemap[1] = DotProduct(l->direction, bitangent);
			deluxemap[2] = DotProduct(l->direction, l->normal);

			VectorAdd(deluxemap, vec3_up, deluxemap);
			VectorNormalize(deluxemap);
		} else {
			VectorCopy(vec3_up, deluxemap);
		}

		// pack floating point -1.0 to 1.0 to positive bytes (0.0 becomes 127)
		for (int32_t j = 0; j < 3; j++) {
			*out_dm++ = (byte) Clamp((deluxemap[j] + 1.0) * 0.5 * 255.0, 0, 255);
		}
	}
}

/**
 * @brief
 */
void EmitLightmap(void) {

	atlas_t *atlas = Atlas_Create(2);
	assert(atlas);

	atlas_node_t *nodes[bsp_file.num_faces];

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_SKY) {
			continue;
		}

		nodes[i] = Atlas_Insert(atlas, lm->lightmap, lm->deluxemap);
	}

	int32_t width;
	for (width = MIN_BSP_LIGHTMAP_WIDTH; width <= MAX_BSP_LIGHTMAP_WIDTH; width += 256) {

		const int32_t layer_size = width * width * BSP_LIGHTMAP_BPP;

		bsp_file.lightmap_size = sizeof(bsp_lightmap_t) + layer_size * BSP_LIGHTMAP_LAYERS;

		Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTMAP, bsp_file.lightmap_size);
 		memset(bsp_file.lightmap, 0, bsp_file.lightmap_size);

		bsp_file.lightmap->width = width;

		byte *out = (byte *) bsp_file.lightmap + sizeof(bsp_lightmap_t);

		byte *out_lm = out + 0 * layer_size;
		byte *out_dm = out + 1 * layer_size;

		SDL_Surface *lightmap = CreateLightmapSurface(width, width, out_lm);
		SDL_Surface *deluxemap = CreateLightmapSurface(width, width, out_dm);

		if (Atlas_Compile(atlas, 0, lightmap, deluxemap) == 0) {

//			IMG_SavePNG(lightmap, va("/tmp/%s_lm.png", map_base));
//			IMG_SavePNG(deluxemap, va("/tmp/%s_dm.png", map_base));

			SDL_FreeSurface(lightmap);
			SDL_FreeSurface(deluxemap);

			break;
		}

		SDL_FreeSurface(lightmap);
		SDL_FreeSurface(deluxemap);
	}

	if (width > MAX_BSP_LIGHTMAP_WIDTH) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTMAP_WIDTH\n");
	}

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_SKY) {
			continue;
		}

		lm->face->lightmap.s = lm->s = nodes[i]->x;
		lm->face->lightmap.t = lm->t = nodes[i]->y;
		lm->face->lightmap.w = lm->w;
		lm->face->lightmap.h = lm->h;

		SDL_FreeSurface(lm->lightmap);
		SDL_FreeSurface(lm->deluxemap);
	}

	Atlas_Destroy(atlas);
}

/**
 * @brief
 */
void EmitLightmapTexcoords(void) {

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_SKY) {
			continue;
		}

		bsp_vertex_t *v = &bsp_file.vertexes[lm->face->first_vertex];
		for (int32_t j = 0; j < lm->face->num_vertexes; j++, v++) {

			vec3_t st;
			Matrix4x4_Transform(&lm->matrix, v->position, st);

			st[0] -= lm->st_mins[0];
			st[1] -= lm->st_mins[1];

			const vec_t padding_s = (lm->w - (lm->st_maxs[0] - lm->st_mins[0])) * 0.5;
			const vec_t padding_t = (lm->h - (lm->st_maxs[1] - lm->st_mins[1])) * 0.5;

			const vec_t s = (lm->s + padding_s + st[0]) / bsp_file.lightmap->width;
			const vec_t t = (lm->t + padding_t + st[1]) / bsp_file.lightmap->width;

			v->lightmap[0] = s;
			v->lightmap[1] = t;
		}
	}
}
