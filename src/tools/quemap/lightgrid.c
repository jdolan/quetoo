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
#include "lightmap.h"
#include "lightgrid.h"
#include "points.h"
#include "qlight.h"

typedef struct {
	vec3_t stu_mins, stu_maxs;
	int32_t size[3];

	matrix4x4_t matrix;
	matrix4x4_t inverse_matrix;

	size_t num_luxels;
	luxel_t *luxels;
} lightgrid_t;

static lightgrid_t lg;

/**
 * @brief
 */
static void BuildLightgridMatrices(void) {

	const bsp_model_t *world = bsp_file.models;

	Matrix4x4_CreateTranslate(&lg.matrix, -world->mins[0], -world->mins[1], -world->mins[2]);
	Matrix4x4_ConcatScale3(&lg.matrix,
						   1.0 / BSP_LIGHTGRID_LUXEL_SIZE,
						   1.0 / BSP_LIGHTGRID_LUXEL_SIZE,
						   1.0 / BSP_LIGHTGRID_LUXEL_SIZE);

	Matrix4x4_Invert_Full(&lg.inverse_matrix, &lg.matrix);
}

/**
 * @brief
 */
static void BuildLightgridExtents(void) {

	const bsp_model_t *world = bsp_file.models;

	vec3_t mins, maxs;
	VectorCopy(world->mins, mins);
	VectorCopy(world->maxs, maxs);

	Matrix4x4_Transform(&lg.matrix, mins, lg.stu_mins);
	Matrix4x4_Transform(&lg.matrix, maxs, lg.stu_maxs);

	for (int32_t i = 0; i < 3; i++) {
		lg.size[i] = floorf(lg.stu_maxs[i] - lg.stu_mins[i]) + 2;
	}
}

/**
 * @brief
 */
void BuildLightgridLuxels(void) {

	lg.num_luxels = lg.size[0] * lg.size[1] * lg.size[2];

	if (lg.num_luxels > MAX_BSP_LIGHTGRID_LUXELS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTGRID_LUXELS\n");
	}

	lg.luxels = Mem_TagMalloc(lg.num_luxels * sizeof(luxel_t), MEM_TAG_LIGHTGRID);

	luxel_t *l = lg.luxels;

	for (int32_t u = 0; u < lg.size[2]; u++) {
		for (int32_t t = 0; t < lg.size[1]; t++) {
			for (int32_t s = 0; s < lg.size[0]; s++, l++) {

				l->s = s;
				l->t = t;
				l->u = u;
			}
		}
	}
}

/**
 * @brief
 */
static int32_t ProjectLightgridLuxel(luxel_t *l, vec_t soffs, vec_t toffs, vec_t uoffs) {

	const vec_t padding_s = ((lg.stu_maxs[0] - lg.stu_mins[0]) - lg.size[0]) * 0.5;
	const vec_t padding_t = ((lg.stu_maxs[1] - lg.stu_mins[1]) - lg.size[1]) * 0.5;
	const vec_t padding_u = ((lg.stu_maxs[2] - lg.stu_mins[2]) - lg.size[2]) * 0.5;

	const vec_t s = lg.stu_mins[0] + padding_s + l->s + 0.5 + soffs;
	const vec_t t = lg.stu_mins[1] + padding_t + l->t + 0.5 + toffs;
	const vec_t u = lg.stu_mins[2] + padding_u + l->u + 0.5 + uoffs;

	const vec3_t stu = { s, t, u };
	Matrix4x4_Transform(&lg.inverse_matrix, stu, l->origin);

	return Light_PointContents(l->origin, 0);
}

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
void DebugLightgridLuxels(void) {
#if 0
	const char *path = va("maps/%s.lightgrid.map", map_base);

	file_t *file = Fs_OpenWrite(path);
	if (file == NULL) {
		Com_Warn("Failed to open %s\n", path);
		return;
	}

	luxel_t *l = lg.luxels;
	for (size_t i = 0; i < lg.num_luxels; i++, l++) {

		ProjectLightgridLuxel(l, 0.0, 0.0, 0.0);

		Fs_Print(file, "{\n");
		Fs_Print(file, "  \"classname\" \"info_luxel\"\n");
		Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin[0], l->origin[1], l->origin[2]);
		Fs_Print(file, "  \"s\" \"%d\"\n", l->s);
		Fs_Print(file, "  \"t\" \"%d\"\n", l->t);
		Fs_Print(file, "  \"u\" \"%d\"\n", l->u);
		Fs_Print(file, "}\n");
	}

	Fs_Close(file);
#endif
}

/**
 * @brief
 */
size_t BuildLightgrid(void) {

	memset(&lg, 0, sizeof(lg));

	BuildLightgridMatrices();

	BuildLightgridExtents();

	BuildLightgridLuxels();

	DebugLightgridLuxels();

	return lg.num_luxels;
}

/**
 * @brief Iterates all lights, accumulating color and directional samples to the specified buffers.
 * @param luxel The luxel to light.
 * @param pvs The PVS for the luxel's origin.
 * @param scale A scalar applied to both light and direction.
 */
static void LightLuxel(luxel_t *luxel, const byte *pvs, vec_t scale) {

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
		if (light->type == LIGHT_AMBIENT) {
			VectorClear(dir);
		} else if (light->type == LIGHT_SUN) {
			VectorNegate(light->normal, dir);
		} else {
			VectorSubtract(light->origin, luxel->origin, dir);
		}

		const vec_t dist = VectorNormalize(dir);
		if (light->atten != LIGHT_ATTEN_NONE) {
			if (dist > light->radius) {
				continue;
			}
		}

		vec_t intensity = Clamp(light->radius, 0.0, LIGHT_RADIUS);

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

		if (light->type == LIGHT_AMBIENT) {

			const bsp_model_t *world = bsp_file.models;

			const vec_t padding_s = (world->maxs[0] - world->mins[0]) - (lg.size[0] * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5;
			const vec_t padding_t = (world->maxs[1] - world->mins[1]) - (lg.size[1] * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5;
			const vec_t padding_u = (world->maxs[2] - world->mins[2]) - (lg.size[2] * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5;

			const vec_t s = lg.stu_mins[0] + padding_s + luxel->s + 0.5;
			const vec_t t = lg.stu_mins[1] + padding_t + luxel->t + 0.5;
			const vec_t u = lg.stu_mins[2] + padding_u + luxel->u + 0.5;

			const vec3_t points[] = CUBE_8;
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
						  sample[2] + Randomc() * 0.04);

				// Scale the sample and move it into position
				VectorSet(sample,
						  sample[0] * ao_radius + s,
						  sample[1] * ao_radius + t,
						  sample[2] * ao_radius + u);

				vec3_t point;
				Matrix4x4_Transform(&lg.inverse_matrix, sample, point);

				const cm_trace_t trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);

				ambient_occlusion += sample_fraction * trace.fraction;
			}

			intensity *= 1.0 - (1.0 - ambient_occlusion) * (1.0 - ambient_occlusion);

		} else if (light->type == LIGHT_SUN) {

			vec3_t sun_origin;
			VectorMA(luxel->origin, -MAX_WORLD_DIST, light->normal, sun_origin);

			cm_trace_t trace = Light_Trace(luxel->origin, sun_origin, 0, CONTENTS_SOLID);
			if (!(trace.surface && (trace.surface->flags & SURF_SKY))) {
				vec_t exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						vec3_t point;
						VectorMA(sun_origin, i * LIGHT_SIZE_STEP, points[j], point);

						trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);
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
			cm_trace_t trace = Light_Trace(luxel->origin, light->origin, 0, CONTENTS_SOLID);
			if (trace.fraction < 1.0) {
				vec_t exposure = 0.0;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						vec3_t point;
						VectorMA(light->origin, (i + 1) * LIGHT_SIZE_STEP, points[j], point);

						trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);
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
 * @brief
 */
void DirectLightgrid(int32_t luxel_num) {

	static const vec4_t offsets[] = {
		{ +0.000, +0.000, +0.000, 0.400 },
		{ -0.25, -0.25, -0.25, 0.075 }, { -0.25, +0.25, -0.25, 0.075 },
		{ +0.25, -0.25, -0.25, 0.075 }, { +0.25, +0.25, -0.25, 0.075 },
		{ -0.25, -0.25, +0.25, 0.075 }, { -0.25, +0.25, +0.25, 0.075 },
		{ +0.25, -0.25, +0.25, 0.075 }, { +0.25, +0.25, +0.25, 0.075 },
	};

	luxel_t *l = &lg.luxels[luxel_num];

	vec_t contribution = 0.0;

	for (size_t j = 0; j < lengthof(offsets); j++) {

		const vec_t soffs = offsets[j][0];
		const vec_t toffs = offsets[j][1];
		const vec_t uoffs = offsets[j][2];
		const vec_t scale = offsets[j][3];

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		byte pvs[MAX_BSP_LEAFS >> 3];
		Light_PointPVS(l->origin, 0, pvs);

		contribution += scale;

		LightLuxel(l, pvs, scale);

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

/**
 * @brief
 */
void IndirectLightgrid(int32_t luxel_num) {

	static const vec3_t offsets[] = {
		{ +0.000, +0.000, +0.000 },
		{ -0.125, -0.125, -0.125 }, { -0.125, +0.125, -0.125 },
		{ +0.125, -0.125, -0.125 }, { +0.125, +0.125, -0.125 },
		{ -0.125, -0.125, +0.125 }, { -0.125, +0.125, +0.125 },
		{ +0.125, -0.125, +0.125 }, { +0.125, +0.125, +0.125 },
	};

	luxel_t *l = &lg.luxels[luxel_num];

	byte pvs[MAX_BSP_LEAFS >> 3];

	ProjectLightgridLuxel(l, 0.0, 0.0, 0.0);
	Light_PointPVS(l->origin, 0, pvs);

	for (size_t j = 0; j < lengthof(offsets); j++) {

		const vec_t soffs = offsets[j][0];
		const vec_t toffs = offsets[j][1];
		const vec_t uoffs = offsets[j][2];

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		LightLuxel(l, pvs, radiosity);
		break;
	}
}

/**
 * @brief
 */
void FinalizeLightgrid(int32_t luxel_num) {

	luxel_t *l = &lg.luxels[luxel_num];

	VectorScale(l->ambient, 1.0 / 255.0, l->ambient);
	ColorFilter(l->ambient, l->ambient, brightness, saturation, contrast);

	VectorScale(l->diffuse, 1.0 / 255.0, l->diffuse);
	ColorFilter(l->diffuse, l->diffuse, brightness, saturation, contrast);

	VectorScale(l->radiosity, 1.0 / 255.0, l->radiosity);
	ColorFilter(l->radiosity, l->radiosity, brightness, saturation, contrast);

	VectorNormalize(l->direction);
}

/**
 * @brief
 */
static SDL_Surface *CreateLightgridSurface(void *pixels) {
	const int32_t w = lg.size[0], h = lg.size[1];

	return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 24, w * BSP_LIGHTMAP_BPP, SDL_PIXELFORMAT_RGB24);
}

/**
 * @brief
 */
void EmitLightgrid(void) {

	const size_t texture_size = lg.num_luxels * BSP_LIGHTGRID_BPP;

	bsp_file.lightgrid_size = (int32_t) (sizeof(bsp_lightgrid_t) + texture_size * BSP_LIGHTGRID_TEXTURES);

	Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTGRID, bsp_file.lightgrid_size);

	bsp_file.lightgrid->size[0] = lg.size[0];
	bsp_file.lightgrid->size[1] = lg.size[1];
	bsp_file.lightgrid->size[2] = lg.size[2];

	byte *out = (byte *) bsp_file.lightgrid + sizeof(bsp_lightgrid_t);

	byte *out_amb = out + 0 * texture_size;
	byte *out_dif = out + 1 * texture_size;
	byte *out_dir = out + 2 * texture_size;

	const luxel_t *l = lg.luxels;
	for (int32_t u = 0; u < lg.size[2]; u++) {

		SDL_Surface *amb = CreateLightgridSurface(out_amb);
		SDL_Surface *dif = CreateLightgridSurface(out_dif);
		SDL_Surface *dir = CreateLightgridSurface(out_dir);

		for (int32_t t = 0; t < lg.size[1]; t++) {
			for (int32_t s = 0; s < lg.size[0]; s++, l++) {

				for (int32_t i = 0; i < 3; i++) {
					*out_amb++ = (byte) Clamp((l->ambient[i] + l->radiosity[i]) * 255.0, 0, 255);
					*out_dif++ = (byte) Clamp((l->diffuse[i] + 0.0) * 255.0, 0, 255);
					*out_dir++ = (byte) Clamp((l->direction[i] + 1.0) * 0.5 * 255.0, 0, 255);
				}
			}
		}

//		IMG_SavePNG(amb, va("/tmp/%s_lg_amb_%d.png", map_base, u));
//		IMG_SavePNG(dif, va("/tmp/%s_lg_dif_%d.png", map_base, u));
//		IMG_SavePNG(dir, va("/tmp/%s_lg_dir_%d.png", map_base, u));

		SDL_FreeSurface(amb);
		SDL_FreeSurface(dif);
		SDL_FreeSurface(dir);
	}
}
