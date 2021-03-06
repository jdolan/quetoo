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

	vec3_t s = Vec3_Normalize(Vec4_XYZ(lm->texinfo->vecs[0]));
	vec3_t t = Vec3_Normalize(Vec4_XYZ(lm->texinfo->vecs[1]));

	s = Vec3_Scale(s, 1.0 / luxel_size);
	t = Vec3_Scale(t, 1.0 / luxel_size);

	lm->matrix = Mat4((const float[]) {
		s.x, t.x, lm->plane->normal.x, 0.0,
		s.y, t.y, lm->plane->normal.y, 0.0,
		s.z, t.z, lm->plane->normal.z, 0.0,
		0.0, 0.0, -lm->plane->dist,    1.0
	});

	const vec3_t origin = patches[lm - lightmaps].origin;
	const mat4_t translate = Mat4_FromTranslation(origin);
	const mat4_t inverse = Mat4_Inverse(lm->matrix);
	lm->inverse_matrix = Mat4_Concat(translate, inverse);
}

/**
 * @brief Resolves the extents, in texture space, for the given lightmap.
 */
static void BuildLightmapExtents(lightmap_t *lm) {

	lm->st_mins = Vec2_Mins();
	lm->st_maxs = Vec2_Maxs();

	const bsp_vertex_t *v = &bsp_file.vertexes[lm->face->first_vertex];
	for (int32_t i = 0; i < lm->face->num_vertexes; i++, v++) {
		const vec3_t st = Mat4_Transform(lm->matrix, v->position);

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

static int32_t ProjectLightmapLuxel(const lightmap_t *lm, luxel_t *l, float soffs, float toffs);

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
static void DebugLightmapLuxels(void) {

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

				ProjectLightmapLuxel(lm, l, 0.0, 0.0);

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

	const bsp_node_t *node = bsp_file.nodes;
	for (int32_t i = 0; i < bsp_file.num_nodes; i++, node++) {
		for (int32_t j = 0; j < node->num_faces; j++) {
			lightmaps[node->first_face + j].node = node;
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

		assert(lm->node);
		assert(lm->model);
		
		lm->face = &bsp_file.faces[i];
		lm->texinfo = &bsp_file.texinfo[lm->face->texinfo];
		lm->plane = &bsp_file.planes[lm->face->plane_num];

		if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
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
	for (int32_t i = 0; i < lm->face->num_elements / 3; i++, e += 3) {

		const bsp_vertex_t *a = bsp_file.vertexes + e[0];
		const bsp_vertex_t *b = bsp_file.vertexes + e[1];
		const bsp_vertex_t *c = bsp_file.vertexes + e[2];

		vec3_t out;
		const float bary = Cm_Barycentric(a->position, b->position, c->position, origin, &out);
		const float delta = fabsf(1.0f - bary);
		if (delta < best) {
			best = delta;

			normal = Vec3_Zero();
			normal = Vec3_Fmaf(normal, out.x, a->normal);
			normal = Vec3_Fmaf(normal, out.y, b->normal);
			normal = Vec3_Fmaf(normal, out.z, c->normal);
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
 * @return The contents mask at the projected luxel origin.
 */
static int32_t ProjectLightmapLuxel(const lightmap_t *lm, luxel_t *l, float soffs, float toffs) {

	const float padding_s = ((lm->st_maxs.x - lm->st_mins.x) - lm->w) * 0.5;
	const float padding_t = ((lm->st_maxs.y - lm->st_mins.y) - lm->h) * 0.5;

	const float s = lm->st_mins.x + padding_s + l->s + 0.5 + soffs;
	const float t = lm->st_mins.y + padding_t + l->t + 0.5 + toffs;

	l->origin = Mat4_Transform(lm->inverse_matrix, Vec3(s, t, 0.f));

	if (lm->texinfo->flags & SURF_PHONG) {
		l->normal = PhongLuxel(lm, l->origin);
	} else {
		l->normal = lm->plane->normal;
	}

	l->origin = Vec3_Add(l->origin, l->normal);
	return Light_PointContents(l->origin, lm->model->head_node);
}

/**
 * @brief Iterates lights, accumulating color and direction on the specified luxel.
 * @param lights The light sources to iterate.
 * @param lightmap The lightmap containing the luxel.
 * @param luxel The luxel to light.
 * @param scale A scalar applied to both color and direction.
 */
static void LightLightmapLuxel(const GPtrArray *lights, const lightmap_t *lightmap, luxel_t *luxel, float scale) {

	for (guint i = 0; i < lights->len; i++) {

		const light_t *light = g_ptr_array_index(lights, i);

		if (light->type == LIGHT_INDIRECT) {
			if (light->face == lightmap->face) {
				continue;
			}
		}

		float dist_squared = 0.f;
		switch (light->type) {
			case LIGHT_SUN:
				break;
			default:
				dist_squared = Vec3_DistanceSquared(light->origin, luxel->origin);
				break;
		}

		if (light->atten != LIGHT_ATTEN_NONE) {
			if (dist_squared > light->radius * light->radius) {
				continue;
			}
		}

		const float dist = sqrtf(dist_squared);

		vec3_t dir;
		if (light->type == LIGHT_SUN) {
			dir = Vec3_Negate(light->normal);
		} else {
			dir = Vec3_Normalize(Vec3_Subtract(light->origin, luxel->origin));
		}

		float dot;
		if (light->type == LIGHT_AMBIENT) {
			dot = 1.f;
		} else {
			dot = Vec3_Dot(dir, luxel->normal);
			if (dot <= 0.f) {
				continue;
			}
		}

		float intensity = Clampf(light->radius, 0.f, MAX_WORLD_COORD) * dot;

		switch (light->type) {
			case LIGHT_SPOT: {
				const float cone_dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
				const float thresh = cosf(light->theta);
				const float smooth = 0.03f;
				intensity *= Smoothf(cone_dot, thresh - smooth, thresh + smooth);
			}
				break;
			default:
				break;
		}

		float atten = 1.f;

		switch (light->atten) {
			case LIGHT_ATTEN_NONE:
				break;
			case LIGHT_ATTEN_LINEAR:
				atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
				break;
			case LIGHT_ATTEN_INVERSE_SQUARE:
				atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
				atten *= atten;
				break;
		}

		intensity *= atten;

		if (intensity <= 0.f) {
			continue;
		}

		const int32_t head_node = lightmap->model->head_node;

		if (light->type == LIGHT_AMBIENT) {

			const float padding_s = ((lightmap->st_maxs.x - lightmap->st_mins.x) - lightmap->w) * 0.5f;
			const float padding_t = ((lightmap->st_maxs.y - lightmap->st_mins.y) - lightmap->h) * 0.5f;

			const float s = lightmap->st_mins.x + padding_s + luxel->s + 0.5f;
			const float t = lightmap->st_mins.y + padding_t + luxel->t + 0.5f;

			const vec3_t points[] = DOME_COSINE_36X;
			float sample_fraction = 1.f / lengthof(points);

			float exposure = 0.f;

			for (size_t i = 0; i < lengthof(points); i++) {

				vec3_t sample = points[i];

				// Add some jitter to hide undersampling
				sample.x += RandomRangef(-.02f, .02f);
				sample.y += RandomRangef(-.02f, .02f);

				// Scale the sample and move it into position
				sample = Vec3_Scale(sample, 256.f / BSP_LIGHTMAP_LUXEL_SIZE);
				
				sample.x += s;
				sample.y += t;

				const vec3_t point = Mat4_Transform(lightmap->inverse_matrix, sample);

				const cm_trace_t trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);

				exposure += sample_fraction * trace.fraction;
			}

			intensity *= exposure;

		} else if (light->type == LIGHT_SUN) {

			const vec3_t sun_origin = Vec3_Fmaf(luxel->origin, -MAX_WORLD_DIST, light->normal);

			cm_trace_t trace = Light_Trace(luxel->origin, sun_origin, head_node, CONTENTS_SOLID);
			if (!(trace.texinfo && (trace.texinfo->flags & SURF_SKY))) {
				float exposure = 0.f;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Fmaf(sun_origin, i * LIGHT_SIZE_STEP, points[j]);

						trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);
						if (!(trace.texinfo && (trace.texinfo->flags & SURF_SKY))) {
							continue;
						}

						exposure += 1.f / num_samples;
						break;
					}
				}

				intensity *= exposure;
			}

		} else {
			cm_trace_t trace = Light_Trace(luxel->origin, light->origin, head_node, CONTENTS_SOLID);
			if (trace.fraction < 1.f) {
				float exposure = 0.f;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Fmaf(light->origin, (i + 1) * LIGHT_SIZE_STEP, points[j]);

						trace = Light_Trace(luxel->origin, point, head_node, CONTENTS_SOLID);
						if (trace.fraction < 1.f) {
							continue;
						}

						exposure += 1.f / num_samples;
						break;
					}
				}

				intensity *= exposure;
			}
		}

		intensity *= scale;

		if (intensity <= 0.f) {
			continue;
		}

		switch (light->type) {
			case LIGHT_INVALID:
				break;
			case LIGHT_AMBIENT:
				luxel->ambient = Vec3_Fmaf(luxel->ambient, intensity, light->color);
				break;
			case LIGHT_SUN:
			case LIGHT_POINT:
			case LIGHT_SPOT:
			case LIGHT_PATCH:
				luxel->diffuse = Vec3_Fmaf(luxel->diffuse, intensity, light->color);
				luxel->direction = Vec3_Fmaf(luxel->direction, intensity, dir);
				break;
			case LIGHT_INDIRECT:
				luxel->radiosity[bounce] = Vec3_Fmaf(luxel->radiosity[bounce], intensity, light->color);
				break;
		}
	}
}

/**
 * @brief Calculates direct lighting for the given face. Luxels are projected into world space.
 * We then query the light sources that intersect the lightmap's node, and accumulate their ambient,
 * diffuse and directional contributions as non-normalized floating point.
 */
void DirectLightmap(int32_t face_num) {

	const vec3_t offsets[] = {
		Vec3(+0.0f, +0.0f, 0.195346f), Vec3(-1.0f, -1.0f, 0.077847f), Vec3(+0.0f, -1.0f, 0.123317f),
		Vec3(+1.0f, -1.0f, 0.077847f), Vec3(-1.0f, +0.0f, 0.123317f), Vec3(+1.0f, +0.0f, 0.123317f),
		Vec3(-1.0f, +1.0f, 0.077847f), Vec3(+0.0f, +1.0f, 0.123317f), Vec3(+1.0f, +1.0f, 0.077847f),
	};

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	const GPtrArray *lights = node_lights[lm->node - bsp_file.nodes];

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		float contribution = 0.0;

		for (size_t j = 0; j < lengthof(offsets) && contribution < 1.f; j++) {

			const float soffs = offsets[j].x;
			const float toffs = offsets[j].y;

			const float weight = antialias ? offsets[j].z : 1.f;

			if (ProjectLightmapLuxel(lm, l, soffs, toffs) == CONTENTS_SOLID) {
				continue;
			}

			contribution += weight;

			LightLightmapLuxel(lights, lm, l, weight);
		}

		if (contribution > 0.f) {
			if (contribution < 1.f) {
				l->ambient = Vec3_Scale(l->ambient, 1.f / contribution);
				l->diffuse = Vec3_Scale(l->diffuse, 1.f / contribution);
				l->direction = Vec3_Scale(l->direction, 1.f / contribution);
			}
		} else {
			// For inline models, always add ambient light sources, even if the sample resides
			// in solid. This prevents completely unlit tops of doors, bottoms of plats, etc.
			if (lm->model != bsp_file.models) {
				for (guint j = 0; j < unattenuated_lights->len; j++) {
					const light_t *light = g_ptr_array_index(lights, j);
					if (light->type == LIGHT_AMBIENT) {
						l->ambient = Vec3_Fmaf(l->ambient, light->radius, light->color);
					}
				}
			}
		}
	}
}

/**
 * @brief Calculates indirect lighting for the given face.
 */
void IndirectLightmap(int32_t face_num) {

	const vec2_t offsets[] = {
		Vec2(+0.0f, +0.0f), Vec2(-1.0f, -1.0f), Vec2(+0.0f, -1.0f),
		Vec2(+1.0f, -1.0f), Vec2(-1.0f, +0.0f), Vec2(+1.0f, +0.0f),
		Vec2(-1.0f, +1.0f), Vec2(+0.0f, +1.0f), Vec2(+1.0f, +1.0f),
	};

	const float weight = antialias ? 1.f / lengthof(offsets) : 1.f;

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	const GPtrArray *lights = node_lights[lm->node - bsp_file.nodes];

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		float contribution = 0.f;

		for (size_t j = 0; j < lengthof(offsets) && contribution < 1.f; j++) {

			const float soffs = offsets[j].x;
			const float toffs = offsets[j].y;

			if (ProjectLightmapLuxel(lm, l, soffs, toffs) == CONTENTS_SOLID) {
				continue;
			}

			contribution += weight;

			LightLightmapLuxel(lights, lm, l, weight);
		}

		if (contribution > 0.f && contribution < 1.f) {
			l->radiosity[bounce] = Vec3_Scale(l->radiosity[bounce], 1.f / contribution);
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

	if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	lm->ambient = CreateLightmapSurface(lm->w, lm->h);
	byte *out_ambient = lm->ambient->pixels;

	lm->diffuse = CreateLightmapSurface(lm->w, lm->h);
	byte *out_diffuse = lm->diffuse->pixels;

	lm->direction = CreateLightmapSurface(lm->w, lm->h);
	byte *out_direction = lm->direction->pixels;

	// write it out
	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		// accumulate radiosity in ambient
		for (int32_t i = 0; i < num_bounces; i++) {
			l->ambient = Vec3_Add(l->ambient, l->radiosity[i]);
		}

		// convert to float
		vec3_t ambient = Vec3_Scale(l->ambient, 1.f / 255.f);
		vec3_t diffuse = Vec3_Scale(l->diffuse, 1.f / 255.f);

		// apply brightness, saturation and contrast
		ambient = ColorFilter(ambient);
		diffuse = ColorFilter(diffuse);

		// write the color sample data as bytes
		for (int32_t j = 0; j < 3; j++) {
			*out_ambient++ = (byte) Clampf(ambient.xyz[j] * 255.f, 0.f, 255.f);
			*out_diffuse++ = (byte) Clampf(diffuse.xyz[j] * 255.f, 0.f, 255.f);
		}

		// re-project the luxel to calculate its centered normal
		ProjectLightmapLuxel(lm, l, 0.f, 0.f);

		// write the directional sample data, in tangent space
		const vec3_t sdir = Vec4_XYZ(lm->texinfo->vecs[0]);
		const vec3_t tdir = Vec4_XYZ(lm->texinfo->vecs[1]);

		vec3_t tangent, bitangent;
		Vec3_Tangents(l->normal, sdir, tdir, &tangent, &bitangent);

		vec3_t direction;
		direction.x = Vec3_Dot(l->direction, tangent);
		direction.y = Vec3_Dot(l->direction, bitangent);
		direction.z = Vec3_Dot(l->direction, l->normal);

		direction = Vec3_Normalize(direction);

		// pack floating point -1.0 to 1.0 to positive bytes (0.0 becomes 127)
		for (int32_t j = 0; j < 3; j++) {
			*out_direction++ = (byte) Clampf((direction.xyz[j] + 1.f) * 0.5f * 255.f, 0.f, 255.f);
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

		if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		nodes[i] = Atlas_Insert(atlas, lm->ambient, lm->diffuse, lm->direction);
		nodes[i]->w = lm->w;
		nodes[i]->h = lm->h;
	}

	int32_t width;
	for (width = MIN_BSP_LIGHTMAP_WIDTH; width <= MAX_BSP_LIGHTMAP_WIDTH; width += 256) {

		const int32_t layer_bytes = width * width * BSP_LIGHTMAP_BPP;

		bsp_file.lightmap_size = sizeof(bsp_lightmap_t) + layer_bytes * BSP_LIGHTMAP_LAYERS;

		Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTMAP, bsp_file.lightmap_size);
 		memset(bsp_file.lightmap, 0, bsp_file.lightmap_size);

		bsp_file.lightmap->width = width;

		byte *out = (byte *) bsp_file.lightmap + sizeof(bsp_lightmap_t);

		SDL_Surface *ambient = CreateLightmapSurfaceFrom(width, width, out + 0 * layer_bytes);
		SDL_Surface *diffuse = CreateLightmapSurfaceFrom(width, width, out + 1 * layer_bytes);
		SDL_Surface *direction = CreateLightmapSurfaceFrom(width, width, out + 2 * layer_bytes);

		if (Atlas_Compile(atlas, 0, ambient, diffuse, direction) == 0) {

//			IMG_SavePNG(ambient, va("/tmp/%s_lm_ambient.png", map_base));
//			IMG_SavePNG(diffuse, va("/tmp/%s_lm_diffuse.png", map_base));
//			IMG_SavePNG(direction, va("/tmp/%s_lm_direction.png", map_base));

			SDL_FreeSurface(ambient);
			SDL_FreeSurface(diffuse);
			SDL_FreeSurface(direction);

			break;
		}

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse);
		SDL_FreeSurface(direction);
	}

	if (width > MAX_BSP_LIGHTMAP_WIDTH) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTMAP_WIDTH\n");
	}

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		lm->face->lightmap.s = lm->s = nodes[i]->x;
		lm->face->lightmap.t = lm->t = nodes[i]->y;
		lm->face->lightmap.w = lm->w;
		lm->face->lightmap.h = lm->h;

		lm->face->lightmap.matrix = lm->matrix;

		lm->face->lightmap.st_mins = lm->st_mins;
		lm->face->lightmap.st_maxs = lm->st_maxs;

		SDL_FreeSurface(lm->ambient);
		SDL_FreeSurface(lm->diffuse);
		SDL_FreeSurface(lm->direction);
	}

	Atlas_Destroy(atlas);
}

/**
 * @brief
 */
void EmitLightmapTexcoords(void) {

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->texinfo->flags & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		bsp_vertex_t *v = &bsp_file.vertexes[lm->face->first_vertex];
		for (int32_t j = 0; j < lm->face->num_vertexes; j++, v++) {

			vec3_t st = Mat4_Transform(lm->matrix, v->position);

			st.x -= lm->st_mins.x;
			st.y -= lm->st_mins.y;

			const float padding_s = (lm->w - (lm->st_maxs.x - lm->st_mins.x)) * 0.5;
			const float padding_t = (lm->h - (lm->st_maxs.y - lm->st_mins.y)) * 0.5;

			const float s = (lm->s + padding_s + st.x) / bsp_file.lightmap->width;
			const float t = (lm->t + padding_t + st.y) / bsp_file.lightmap->width;

			v->lightmap.x = s;
			v->lightmap.y = t;
		}
	}
}
