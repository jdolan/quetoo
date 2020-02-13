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
	s = Vec3_Normalize(Vec4_XYZ(lm->texinfo->vecs[0]));
	t = Vec3_Normalize(Vec4_XYZ(lm->texinfo->vecs[1]));

	s = Vec3_Scale(s, 1.0 / luxel_size);
	t = Vec3_Scale(t, 1.0 / luxel_size);

	Matrix4x4_FromArrayFloatGL(&lm->matrix, (const float[]) {
		s.x, t.x, lm->plane->normal.x, 0.0,
		s.y, t.y, lm->plane->normal.y, 0.0,
		s.z, t.z, lm->plane->normal.z, 0.0,
		0.0, 0.0, -lm->plane->dist,    1.0
	});

	const vec3_t origin = patches[lm - lightmaps].origin;

	mat4_t translate;
	Matrix4x4_CreateTranslate(&translate, origin.x, origin.y, origin.z);

	mat4_t inverse;
	Matrix4x4_Invert_Full(&inverse, &lm->matrix);

	Matrix4x4_Concat(&lm->inverse_matrix, &translate, &inverse);
}

/**
 * @brief Resolves the extents, in texture space, for the given lightmap.
 */
static void BuildLightmapExtents(lightmap_t *lm) {

	lm->st_mins = Vec2_Mins();
	lm->st_maxs = Vec2_Maxs();

	const bsp_vertex_t *v = &bsp_file.vertexes[lm->face->first_vertex];
	for (int32_t i = 0; i < lm->face->num_vertexes; i++, v++) {

		vec3_t st;
		Matrix4x4_Transform(&lm->matrix, v->position.xyz, st.xyz);

		lm->st_mins = Vec2_Minf(lm->st_mins, Vec3_XY(st));
		lm->st_maxs = Vec2_Maxf(lm->st_maxs, Vec3_XY(st));
	}

	// add 4 luxels of padding around the lightmap for bicubic filtering
	lm->w = floorf(lm->st_maxs.x - lm->st_mins.x) + 4;
	lm->h = floorf(lm->st_maxs.y - lm->st_mins.y) + 4;
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

static int32_t ProjectLuxel(const lightmap_t *lm, luxel_t *l, float soffs, float toffs);

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
				Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin.x, l->origin.y, l->origin.z);
				Fs_Print(file, "  \"normal\" \"%g %g %g\"\n", l->normal.x, l->normal.y, l->normal.z);
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
static vec3_t PhongLuxel(const lightmap_t *lm, const vec3_t origin) {

	vec3_t normal = lm->plane->normal;

	float best = FLT_MAX;

	const int32_t *e = bsp_file.elements + lm->face->first_element;
	for (int32_t i = 0; i < lm->face->num_elements; i++, e += 3) {

		const bsp_vertex_t *a = bsp_file.vertexes + e[0];
		const bsp_vertex_t *b = bsp_file.vertexes + e[1];
		const bsp_vertex_t *c = bsp_file.vertexes + e[2];

		vec3_t out;
		const float bary = Cm_Barycentric(a->position, b->position, c->position, origin, &out);
		const float delta = fabsf(1.0f - bary);
		if (delta < best) {
			best = delta;

			normal = Vec3_Zero();

			normal = Vec3_Add(normal, Vec3_Scale(a->normal, out.x));
			normal = Vec3_Add(normal, Vec3_Scale(b->normal, out.y));
			normal = Vec3_Add(normal, Vec3_Scale(c->normal, out.z));
		}
	}

	return Vec3_Normalize(normal);
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
static int32_t ProjectLuxel(const lightmap_t *lm, luxel_t *l, float soffs, float toffs) {

	const float padding_s = ((lm->st_maxs.x - lm->st_mins.x) - lm->w) * 0.5;
	const float padding_t = ((lm->st_maxs.y - lm->st_mins.y) - lm->h) * 0.5;

	const float s = lm->st_mins.x + padding_s + l->s + 0.5 + soffs;
	const float t = lm->st_mins.y + padding_t + l->t + 0.5 + toffs;

	const vec3_t st = { s , t, 0.0 };
	Matrix4x4_Transform(&lm->inverse_matrix, st.xyz, l->origin.xyz);

	if (lm->texinfo->flags & SURF_PHONG) {
		l->normal = PhongLuxel(lm, l->origin);
	} else {
		l->normal = lm->plane->normal;
	}

	l->origin = Vec3_Add(l->origin, l->normal);
	return Light_PointContents(l->origin, lm->model->head_node);
}

/**
 * @brief Iterates all lights, accumulating color and direction to the appropriate buffers.
 * @param lightmap The lightmap containing the luxel.
 * @param luxel The luxel to light.
 * @param pvs The PVS for the luxel's origin.
 * @param scale A scalar applied to both light and direction.
 */
static void LightLuxel(const lightmap_t *lightmap, luxel_t *luxel, const byte *pvs, float scale) {

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

		vec3_t dir; float dist;
		if (light->type == LIGHT_AMBIENT) {
			dir = Vec3(0.0, 0.0, 1.0);
			dist = 0.0;
		} else if (light->type == LIGHT_SUN) {
			dir = Vec3_Negate(light->normal);
			dist = 0.0;
		} else {
			dist = Vec3_Distance(light->origin, luxel->origin);
			dir = Vec3_Normalize(Vec3_Subtract(light->origin, luxel->origin));
		}

		if (light->atten != LIGHT_ATTEN_NONE) {
			if (dist > light->radius) {
				continue;
			}
		}

		float dot;
		if (light->type == LIGHT_AMBIENT) {
			dot = 1.0;
		} else {
			dot = Vec3_Dot(dir, luxel->normal);
			if (dot <= 0.0) {
				continue;
			}
		}

		float intensity = Clampf(light->radius * dot, 0.0, LIGHT_RADIUS);

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
				const float cone_dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
				const float thresh = cosf(light->theta);
				const float smooth = 0.03;
				intensity *= Smoothf(thresh - smooth, thresh + smooth, cone_dot);
				intensity *= DEFAULT_BSP_PATCH_SIZE;
			}
				break;
			case LIGHT_PATCH:
			case LIGHT_INDIRECT:
				intensity *= patch_size / DEFAULT_BSP_PATCH_SIZE;
				break;
		}

		float atten = 1.0;

		switch (light->atten) {
			case LIGHT_ATTEN_NONE:
				break;
			case LIGHT_ATTEN_LINEAR:
				atten = Clampf(1.0 - dist / light->radius, 0.0, 1.0);
				break;
			case LIGHT_ATTEN_INVERSE_SQUARE:
				atten = Clampf(1.0 - dist / light->radius, 0.0, 1.0);
				atten *= atten;
				break;
		}

		intensity *= atten;

		if (intensity <= 0.0) {
			continue;
		}

		const int32_t head_node = lightmap->model->head_node;

		if (light->type == LIGHT_AMBIENT) {

			const float padding_s = ((lightmap->st_maxs.x - lightmap->st_mins.x) - lightmap->w) * 0.5;
			const float padding_t = ((lightmap->st_maxs.y - lightmap->st_mins.y) - lightmap->h) * 0.5;

			const float s = lightmap->st_mins.x + padding_s + luxel->s + 0.5;
			const float t = lightmap->st_mins.y + padding_t + luxel->t + 0.5;

			const vec3_t points[] = DOME_COSINE_36X;
			const float ao_radius = 64.0;

			float occlusion = 0.0;
			float sample_fraction = 1.0 / lengthof(points);

			for (size_t i = 0; i < lengthof(points); i++) {

				vec3_t sample = points[i];

				// Add some jitter to hide undersampling
				sample.x += Randomc() * 0.04;
				sample.y += Randomc() * 0.04;

				// Scale the sample and move it into position
				sample = Vec3_Scale(sample, ao_radius);
				
				sample.x += s;
				sample.y += t;

				vec3_t point;
				Matrix4x4_Transform(&lightmap->inverse_matrix, sample.xyz, point.xyz);

				const cm_trace_t trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);

				occlusion += sample_fraction * trace.fraction;
			}

			intensity *= 1.0 - (1.0 - occlusion) * (1.0 - occlusion);

		} else if (light->type == LIGHT_SUN) {

			const vec3_t sun_origin = Vec3_Add(luxel->origin, Vec3_Scale(light->normal, -MAX_WORLD_DIST));

			cm_trace_t trace = Light_Trace(luxel->origin, sun_origin, head_node, CONTENTS_SOLID);
			if (!(trace.surface && (trace.surface->flags & SURF_SKY))) {
				float exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Add(sun_origin, Vec3_Scale(points[j], i * LIGHT_SIZE_STEP));

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
				float exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Add(light->origin, Vec3_Scale(points[j], (i + 1) * LIGHT_SIZE_STEP));

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
				luxel->ambient = Vec3_Add(luxel->ambient, Vec3_Scale(light->color, intensity));
				break;
			case LIGHT_SUN:
			case LIGHT_POINT:
			case LIGHT_SPOT:
			case LIGHT_PATCH:
				luxel->diffuse = Vec3_Add(luxel->diffuse, Vec3_Scale(light->color, intensity));
				luxel->diffuse_dir = Vec3_Add(luxel->diffuse_dir, Vec3_Scale(dir, intensity));
				break;
			case LIGHT_INDIRECT:
				luxel->radiosity = Vec3_Add(luxel->radiosity, Vec3_Scale(light->color, intensity));
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

		float contribution = 0.0;

		for (size_t j = 0; j < lengthof(offsets); j++) {

			const float soffs = offsets[j].x;
			const float toffs = offsets[j].y;
			const float scale = offsets[j].z;

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
			l->ambient = Vec3_Scale(l->ambient, 1.0 / contribution);
			l->diffuse = Vec3_Scale(l->diffuse, 1.0 / contribution);
			l->diffuse_dir = Vec3_Scale(l->diffuse_dir, 1.0 / contribution);
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

			const float soffs = offsets[j].x;
			const float toffs = offsets[j].y;

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
static SDL_Surface *CreateLightmapSurfaceFrom(int32_t w, int32_t h, void *pixels) {
	return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 24, w * BSP_LIGHTMAP_BPP, SDL_PIXELFORMAT_RGB24);
}

/**
 * @brief
 */
static SDL_Surface *CreateLightmapSurface(int32_t w, int32_t h) {
	return CreateLightmapSurfaceFrom(w, h, Mem_TagMalloc(w * h * BSP_LIGHTMAP_BPP, MEM_TAG_LIGHTMAP));
}

/**
 * @brief Finalize light values for the given face, and create its lightmap textures.
 */
void FinalizeLightmap(int32_t face_num) {

	lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_SKY) {
		return;
	}

	lm->ambient = CreateLightmapSurface(lm->w, lm->h);
	byte *out_ambient = lm->ambient->pixels;

	lm->diffuse = CreateLightmapSurface(lm->w, lm->h);
	byte *out_diffuse = lm->diffuse->pixels;

	lm->radiosity = CreateLightmapSurface(lm->w, lm->h);
	byte *out_radiosity = lm->radiosity->pixels;

	lm->diffuse_dir = CreateLightmapSurface(lm->w, lm->h);
	byte *out_diffuse_dir = lm->diffuse_dir->pixels;

	// write it out
	const luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {
		vec3_t ambient, diffuse, radiosity;

		// convert to float
		ambient = Vec3_Scale(l->ambient, 1.0 / 255.0);
		diffuse = Vec3_Scale(l->diffuse, 1.0 / 255.0);
		radiosity = Vec3_Scale(l->radiosity, 1.0 / 255.0);

		// apply brightness, saturation and contrast
		ambient = ColorFilter(ambient);
		diffuse = ColorFilter(diffuse);
		radiosity = ColorFilter(radiosity);

		// write the color sample data as bytes
		for (int32_t j = 0; j < 3; j++) {
			*out_ambient++ = (byte) Clampf(ambient.xyz[j] * 255.0, 0, 255);
			*out_diffuse++ = (byte) Clampf(diffuse.xyz[j] * 255.0, 0, 255);
			*out_radiosity++ = (byte) Clampf(radiosity.xyz[j] * 255.0, 0, 255);
		}

		// write the directional sample data, in tangent space
		vec3_t diffuse_dir;

		if (!Vec3_Equal(l->diffuse_dir, Vec3_Zero())) {

			const vec3_t sdir = Vec4_XYZ(lm->texinfo->vecs[0]);
			const vec3_t tdir = Vec4_XYZ(lm->texinfo->vecs[1]);

			vec3_t tangent, bitangent;
			Vec3_Tangents(l->normal, sdir, tdir, &tangent, &bitangent);

			diffuse_dir.x = Vec3_Dot(l->diffuse_dir, tangent);
			diffuse_dir.y = Vec3_Dot(l->diffuse_dir, bitangent);
			diffuse_dir.z = Vec3_Dot(l->diffuse_dir, l->normal);

			diffuse_dir = Vec3_Add(diffuse_dir, Vec3_Up());
			diffuse_dir = Vec3_Normalize(diffuse_dir);
		} else {
			diffuse_dir = Vec3_Up();
		}

		// pack floating point -1.0 to 1.0 to positive bytes (0.0 becomes 127)
		for (int32_t j = 0; j < 3; j++) {
			*out_diffuse_dir++ = (byte) Clampf((diffuse_dir.xyz[j] + 1.0) * 0.5 * 255.0, 0, 255);
		}
	}
}

/**
 * @brief
 */
void EmitLightmap(void) {

	atlas_t *atlas = Atlas_Create(BSP_LIGHTMAP_LAYERS);
	assert(atlas);

	atlas_node_t *nodes[bsp_file.num_faces];

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_SKY) {
			continue;
		}

		nodes[i] = Atlas_Insert(atlas, lm->ambient, lm->diffuse, lm->radiosity, lm->diffuse_dir);
	}

	int32_t width;
	for (width = MIN_BSP_LIGHTMAP_WIDTH; width <= MAX_BSP_LIGHTMAP_WIDTH; width += 256) {

		const int32_t layer_size = width * width * BSP_LIGHTMAP_BPP;

		bsp_file.lightmap_size = sizeof(bsp_lightmap_t) + layer_size * BSP_LIGHTMAP_LAYERS;

		Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTMAP, bsp_file.lightmap_size);
 		memset(bsp_file.lightmap, 0, bsp_file.lightmap_size);

		bsp_file.lightmap->width = width;

		byte *out = (byte *) bsp_file.lightmap + sizeof(bsp_lightmap_t);

		SDL_Surface *ambient = CreateLightmapSurfaceFrom(width, width, out + 0 * layer_size);
		SDL_Surface *diffuse = CreateLightmapSurfaceFrom(width, width, out + 1 * layer_size);
		SDL_Surface *radiosity = CreateLightmapSurfaceFrom(width, width, out + 2 * layer_size);
		SDL_Surface *diffuse_dir = CreateLightmapSurfaceFrom(width, width, out + 3 * layer_size);

		if (Atlas_Compile(atlas, 0, ambient, diffuse, radiosity, diffuse_dir) == 0) {

			IMG_SavePNG(ambient, va("/tmp/%s_lm_ambient.png", map_base));
			IMG_SavePNG(diffuse, va("/tmp/%s_lm_diffuse.png", map_base));
			IMG_SavePNG(radiosity, va("/tmp/%s_lm_radiosity.png", map_base));
			IMG_SavePNG(diffuse_dir, va("/tmp/%s_lm_diffuse_dir.png", map_base));

			SDL_FreeSurface(ambient);
			SDL_FreeSurface(diffuse);
			SDL_FreeSurface(radiosity);
			SDL_FreeSurface(diffuse_dir);

			break;
		}

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse);
		SDL_FreeSurface(radiosity);
		SDL_FreeSurface(diffuse_dir);
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

		SDL_FreeSurface(lm->ambient);
		SDL_FreeSurface(lm->diffuse);
		SDL_FreeSurface(lm->radiosity);
		SDL_FreeSurface(lm->diffuse_dir);
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
			Matrix4x4_Transform(&lm->matrix, v->position.xyz, st.xyz);

			st.x -= lm->st_mins.x;
			st.y -= lm->st_mins.y;

			const float padding_s = (lm->w - (lm->st_maxs.x - lm->st_mins.x)) * 0.5;
			const float padding_t = (lm->h - (lm->st_maxs.y - lm->st_mins.y)) * 0.5;

			const float s = (lm->s + padding_s + st.x) / bsp_file.lightmap->width;
			const float t = (lm->t + padding_t + st.x) / bsp_file.lightmap->width;

			v->lightmap.x = s;
			v->lightmap.y = t;
		}
	}
}
