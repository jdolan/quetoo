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
#include "simplex.h"

typedef struct {
	box3_t stu_bounds;
	vec3i_t size;

	mat4_t matrix;
	mat4_t inverse_matrix;

	size_t num_luxels;
	luxel_t *luxels;
} lightgrid_t;

static lightgrid_t lg;

/**
 * @brief
 */
static void BuildLightgridMatrices(void) {

	const bsp_model_t *world = bsp_file.models;

	lg.matrix = Mat4_FromTranslation(Vec3_Negate(world->bounds.mins));
	lg.matrix = Mat4_ConcatScale(lg.matrix, 1.f / BSP_LIGHTGRID_LUXEL_SIZE);
	lg.inverse_matrix = Mat4_Inverse(lg.matrix);
}

/**
 * @brief
 */
static void BuildLightgridExtents(void) {

	const bsp_model_t *world = bsp_file.models;

	lg.stu_bounds = Mat4_TransformBounds(lg.matrix, world->bounds);

	for (int32_t i = 0; i < 3; i++) {
		lg.size.xyz[i] = floorf(lg.stu_bounds.maxs.xyz[i] - lg.stu_bounds.mins.xyz[i]) + 2;
	}
}

/**
 * @brief
 */
static void BuildLightgridLuxels(void) {

	lg.num_luxels = lg.size.x * lg.size.y * lg.size.z;

	if (lg.num_luxels > MAX_BSP_LIGHTGRID_LUXELS) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTGRID_LUXELS\n");
	}

	lg.luxels = Mem_TagMalloc(lg.num_luxels * sizeof(luxel_t), MEM_TAG_LIGHTGRID);

	luxel_t *l = lg.luxels;

	for (int32_t u = 0; u < lg.size.z; u++) {
		for (int32_t t = 0; t < lg.size.y; t++) {
			for (int32_t s = 0; s < lg.size.x; s++, l++) {

				l->s = s;
				l->t = t;
				l->u = u;

				l->direction = Vec3_Up();
			}
		}
	}
}

/**
 * @brief
 */
static int32_t ProjectLightgridLuxel(luxel_t *l, float soffs, float toffs, float uoffs) {

	const float padding_s = ((lg.stu_bounds.maxs.x - lg.stu_bounds.mins.x) - lg.size.x) * 0.5;
	const float padding_t = ((lg.stu_bounds.maxs.y - lg.stu_bounds.mins.y) - lg.size.y) * 0.5;
	const float padding_u = ((lg.stu_bounds.maxs.z - lg.stu_bounds.mins.z) - lg.size.z) * 0.5;

	const float s = lg.stu_bounds.mins.x + padding_s + l->s + 0.5 + soffs;
	const float t = lg.stu_bounds.mins.y + padding_t + l->t + 0.5 + toffs;
	const float u = lg.stu_bounds.mins.z + padding_u + l->u + 0.5 + uoffs;

	l->origin = Mat4_Transform(lg.inverse_matrix, Vec3(s, t, u));

	return Light_PointContents(l->origin, 0);
}

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
static void DebugLightgridLuxels(void) {
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
		Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin.x, l->origin.y, l->origin.z);
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
 * @brief
 */
static void LightgridLuxel_Ambient(const light_t *light, luxel_t *luxel, float scale) {

	const vec3_t points[] = CUBE_8;
	float sample_fraction = scale / lengthof(points);

	float intensity = 0.f;

	for (size_t i = 0; i < lengthof(points); i++) {

		const vec3_t end = Vec3_Fmaf(luxel->origin, light->radius, points[i]);
		const cm_trace_t trace = Light_Trace(luxel->origin, end, 0, CONTENTS_SOLID);

		intensity += sample_fraction * trace.fraction;
	}

	luxel->ambient = Vec3_Fmaf(luxel->ambient, light->radius * intensity, light->color);
}

/**
 * @brief
 */
static void LightgridLuxel_Sun(const light_t *light, luxel_t *luxel, float scale) {

	for (int32_t i = 0; i < light->num_points; i++) {

		const vec3_t dir = Vec3_Negate(light->points[i]);
		const vec3_t end = Vec3_Fmaf(luxel->origin, MAX_WORLD_DIST, dir);

		const cm_trace_t trace = Light_Trace(luxel->origin, end, 0, CONTENTS_SOLID);
		if (trace.surface & SURF_SKY) {

			const float intensity = (light->radius / light->num_points);

			luxel->diffuse = Vec3_Fmaf(luxel->diffuse, intensity * scale, light->color);
			luxel->direction = Vec3_Fmaf(luxel->direction, intensity * scale, dir);
		}
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Point(const light_t *light, luxel_t *luxel, float scale) {
	vec3_t dir;

	float dist = Vec3_DistanceDir(light->origin, luxel->origin, &dir);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
		return;
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

	const float intensity = light->radius * atten * scale;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		luxel->diffuse = Vec3_Fmaf(luxel->diffuse, intensity, light->color);
		luxel->direction = Vec3_Fmaf(luxel->direction, intensity, dir);

		break;
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Spot(const light_t *light, luxel_t *luxel, float scale) {
	vec3_t dir;

	float dist = Vec3_DistanceDir(light->origin, luxel->origin, &dir);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
		return;
	}

	const float cone_dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
	const float thresh = cosf(light->theta);
	const float smooth = 0.03f;
	const float cutoff = Smoothf(cone_dot, thresh - smooth, thresh + smooth);

	if (cutoff <= 0.f) {
		return;
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

	const float intensity = light->radius * cutoff * atten * scale;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		luxel->diffuse = Vec3_Fmaf(luxel->diffuse, intensity, light->color);
		luxel->direction = Vec3_Fmaf(luxel->direction, intensity, dir);
		break;
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Patch(const light_t *light, luxel_t *luxel, float scale) {
	vec3_t dir;

	if (light->model != bsp_file.models) {
		return;
	}

	if (Vec3_Dot(luxel->origin, light->plane->normal) - light->plane->dist < -BSP_LIGHTGRID_LUXEL_SIZE) {
		return;
	}

	float dist = Vec3_DistanceDir(light->origin, luxel->origin, &dir);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
		return;
	}

#if 0
	const float cone_dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
	const float thresh = cosf(light->theta);
	const float smooth = 0.03f;
	const float cutoff = Smoothf(cone_dot, thresh - smooth, thresh + smooth);

	if (cutoff <= 0.f) {
		return;
	}
#else
	const float cutoff = 1.f;
#endif

	const float atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
	const float intensity = light->radius * cutoff * atten * atten * scale;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		luxel->diffuse = Vec3_Fmaf(luxel->diffuse, intensity, light->color);
		luxel->direction = Vec3_Fmaf(luxel->direction, intensity, dir);
		break;
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Indirect(const light_t *light, luxel_t *luxel, float scale) {
	vec3_t dir;

	if (light->model != bsp_file.models) {
		return;
	}

	if (Vec3_Dot(luxel->origin, light->plane->normal) - light->plane->dist < -BSP_LIGHTGRID_LUXEL_SIZE) {
		return;
	}
	
	float dist = Vec3_DistanceDir(light->origin, luxel->origin, &dir);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
		return;
	}

#if 1
	const float cone_dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
	const float thresh = cosf(light->theta);
	const float smooth = 0.03f;
	const float cutoff = Smoothf(cone_dot, thresh - smooth, thresh + smooth);

	if (cutoff <= 0.f) {
		return;
	}
#else
	const float cutoff = 1.f;
#endif

	const float atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
	const float intensity = light->radius * cutoff * atten * atten * scale;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		luxel->indirect[indirect_bounce] = Vec3_Fmaf(luxel->indirect[indirect_bounce], intensity, light->color);
		break;
	}
}

/**
 * @brief Iterates lights, accumulating color and direction on the specified luxel.
 * @param lights The light sources to iterate.
 * @param luxel The luxel to light.
 * @param scale A scalar applied to both color and direction.
 */
static inline void LightgridLuxel(const GPtrArray *lights, luxel_t *luxel, float scale) {

	for (guint i = 0; i < lights->len; i++) {

		const light_t *light = g_ptr_array_index(lights, i);

		switch (light->type) {
			case LIGHT_INVALID:
				break;
			case LIGHT_AMBIENT:
				LightgridLuxel_Ambient(light, luxel, scale);
				break;
			case LIGHT_SUN:
				LightgridLuxel_Sun(light, luxel, scale);
				break;
			case LIGHT_POINT:
				LightgridLuxel_Point(light, luxel, scale);
				break;
			case LIGHT_SPOT:
				LightgridLuxel_Spot(light, luxel, scale);
				break;
			case LIGHT_PATCH:
				LightgridLuxel_Patch(light, luxel, scale);
				break;
			case LIGHT_INDIRECT:
				LightgridLuxel_Indirect(light, luxel, scale);
				break;
		}
	}
}

/**
 * @brief
 */
void DirectLightgrid(int32_t luxel_num) {

	const vec3_t offsets[] = {
		Vec3(+0.00f, +0.00f, +0.00f),
		Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
		Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
		Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
		Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
	};

	const float weight = antialias ? 1.f / lengthof(offsets) : 1.f;

	luxel_t *l = &lg.luxels[luxel_num];

	float contribution = 0.f;

	for (size_t i = 0; i < lengthof(offsets) && contribution < 1.f; i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		const GPtrArray *lights = leaf_lights[Cm_PointLeafnum(l->origin, 0)];
		if (!lights) {
			continue;
		}

		contribution += weight;

		LightgridLuxel(lights, l, weight);
	}

	if (contribution > 0.f) {
		if (contribution < 1.f) {
			l->ambient = Vec3_Scale(l->ambient, 1.f / contribution);
			l->diffuse = Vec3_Scale(l->diffuse, 1.f / contribution);
			l->direction = Vec3_Scale(l->direction, 1.f / contribution);
		}
	} else {
		// Even luxels in solids should receive at least ambient light
		for (guint j = 0; j < unattenuated_lights->len; j++) {
			const light_t *light = g_ptr_array_index(unattenuated_lights, j);
			if (light->type == LIGHT_AMBIENT) {
				l->ambient = Vec3_Fmaf(l->ambient, light->radius, light->color);
			}
		}
	}
}

/**
 * @brief
 */
void IndirectLightgrid(int32_t luxel_num) {

	const vec3_t offsets[] = {
		Vec3(+0.00f, +0.00f, +0.00f),
		Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
		Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
		Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
		Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
	};

	const float weight = antialias ? 1.f / lengthof(offsets) : 1.f;

	luxel_t *l = &lg.luxels[luxel_num];

	float contribution = 0.f;

	for (size_t i = 0; i < lengthof(offsets) && contribution < 1.f; i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		const GPtrArray *lights = leaf_lights[Cm_PointLeafnum(l->origin, 0)];
		if (!lights) {
			continue;
		}

		contribution += weight;

		LightgridLuxel(lights, l, weight);
	}

	if (contribution > 0.f) {
		if (contribution < 1.f) {
			l->indirect[indirect_bounce] = Vec3_Scale(l->indirect[indirect_bounce], 1.f / contribution);
		}
	}
}

/**
 * @brief
 */
static void CausticsLightgridLuxel(luxel_t *luxel, float scale) {

	vec3_t c = Vec3_Zero();

	const int32_t contents = Light_PointContents(luxel->origin, 0);
	if (contents & CONTENTS_MASK_LIQUID) {
		if (contents & CONTENTS_LAVA) {
			c = Vec3_Add(c, Vec3(1.f, 0.f, 0.f));
		}
		if (contents & CONTENTS_SLIME) {
			c = Vec3_Add(c, Vec3(0.f, 1.f, 0.f));
		}
		if (contents & CONTENTS_WATER) {
			c = Vec3_Add(c, Vec3(0.f, 0.f, 1.f));
		}
	}
	
	const vec3_t points[] = CUBE_8;
	float sample_fraction = 1.f / lengthof(points);

	for (size_t i = 0; i < lengthof(points); i++) {

		const vec3_t point = Vec3_Fmaf(luxel->origin, 128.f, points[i]);

		const cm_trace_t tr = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID | CONTENTS_MASK_LIQUID);
		if ((tr.contents & CONTENTS_MASK_LIQUID) && (tr.surface & SURF_MASK_TRANSLUCENT)) {

			float f = sample_fraction * (1.f - tr.fraction);

			if (tr.contents & CONTENTS_LAVA) {
				c = Vec3_Add(c, Vec3(f, 0.f, 0.f));
			}
			if (tr.contents & CONTENTS_SLIME) {
				c = Vec3_Add(c, Vec3(0.f, f, 0.f));
			}
			if (tr.contents & CONTENTS_WATER) {
				c = Vec3_Add(c, Vec3(0.f, 0.f, f));
			}
		}
	}

	luxel->caustics = Vec3_Fmaf(luxel->caustics, scale * caustics, c);
}

/**
 * @brief
 */
void CausticsLightgrid(int32_t luxel_num) {

	const vec3_t offsets[] = {
		Vec3(+0.00f, +0.00f, +0.00f),
		Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
		Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
		Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
		Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
	};

	const float weight = antialias ? 1.f / lengthof(offsets) : 1.f;

	luxel_t *l = &lg.luxels[luxel_num];

	float contribution = 0.f;

	for (size_t i = 0; i < lengthof(offsets) && contribution < 1.f; i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		contribution += weight;

		CausticsLightgridLuxel(l, weight);
	}

	if (contribution > 0.f) {
		if (contribution < 1.f) {
			l->caustics = Vec3_Scale(l->caustics, 1.f / contribution);
		}
	}
}

/**
 * @brief
 */
static void FogLightgridLuxel(GArray *fogs, luxel_t *l, float scale) {

	const fog_t *fog = (fog_t *) fogs->data;
	for (guint i = 0; i < fogs->len; i++, fog++) {

		float intensity = 1.f;

		switch (fog->type) {
			case FOG_VOLUME:
				if (!PointInsideFog(l->origin, fog)) {
					intensity = 0.f;
				}
				break;
			default:
				break;
		}

		float noise = SimplexNoiseFBM(fog->octaves,
									  fog->frequency,
									  fog->amplitude,
									  fog->lacunarity,
									  fog->persistence,
									  (l->s + fog->offset.x) / (float) MAX_BSP_LIGHTGRID_WIDTH,
									  (l->t + fog->offset.y) / (float) MAX_BSP_LIGHTGRID_WIDTH,
									  (l->u + fog->offset.z) / (float) MAX_BSP_LIGHTGRID_WIDTH,
									  fog->permutation_vector);

		intensity *= fog->density + (fog->noise * noise);

		intensity = Clampf(intensity * scale, 0.f, 1.f);

		if (intensity == 0.f) {
			continue;
		}

		const vec3_t diffuse = Vec3_Scale(l->diffuse, 1.f / 255.f);
		const vec3_t color = Vec3_Fmaf(fog->color, Clampf(fog->absorption, 0.f, 1.f), diffuse);

		switch (fog->type) {
			case FOG_INVALID:
				break;
			case FOG_GLOBAL:
			case FOG_VOLUME:
				l->fog = Vec4_Add(l->fog, Vec3_ToVec4(color, intensity));
				break;
		}
	}
}

/**
 * @brief
 */
void FogLightgrid(int32_t luxel_num) {

	const vec3_t offsets[] = {
		Vec3(+0.00f, +0.00f, +0.00f),
		Vec3(-0.25f, -0.25f, -0.25f), Vec3(-0.25f, +0.25f, -0.25f),
		Vec3(+0.25f, -0.25f, -0.25f), Vec3(+0.25f, +0.25f, -0.25f),
		Vec3(-0.25f, -0.25f, +0.25f), Vec3(-0.25f, +0.25f, +0.25f),
		Vec3(+0.25f, -0.25f, +0.25f), Vec3(+0.25f, +0.25f, +0.25f),
	};

	const float weight = antialias ? 1.f / lengthof(offsets) : 1.f;

	luxel_t *l = &lg.luxels[luxel_num];

	float contribution = 0.f;

	for (size_t i = 0; i < lengthof(offsets) && contribution < 1.f; i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		contribution += weight;

		FogLightgridLuxel(fogs, l, weight);
	}

	if (contribution > 0.f) {
		if (contribution < 1.f) {
			l->fog = Vec4_Scale(l->fog, 1.f / contribution);
		}
	}
}

/**
 * @brief
 */
void FinalizeLightgrid(int32_t luxel_num) {

	luxel_t *l = &lg.luxels[luxel_num];

	for (int32_t i = 0; i < num_indirect_bounces; i++) {
		l->ambient = Vec3_Add(l->ambient, l->indirect[i]);
	}

	l->ambient = Vec3_Scale(l->ambient, 1.f / 255.f);
	l->ambient = ColorFilter(l->ambient);

	l->diffuse = Vec3_Scale(l->diffuse, 1.f / 255.f);
	l->diffuse = ColorFilter(l->diffuse);

	l->direction = Vec3_Normalize(l->direction);

	l->caustics = ColorNormalize(l->caustics);

	l->fog = Vec3_ToVec4(ColorFilter(Vec4_XYZ(l->fog)), Clampf(l->fog.w, 0.f, 1.f));
}

/**
 * @brief
 */
static SDL_Surface *CreateLightgridSurfaceFrom(void *pixels, int32_t w, int32_t h) {
	return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 24, w * BSP_LIGHTMAP_BPP, SDL_PIXELFORMAT_RGB24);
}

/**
 * @brief
 */
static SDL_Surface *CreateFogSurfaceFrom(void *pixels, int32_t w, int32_t h) {
	return SDL_CreateRGBSurfaceWithFormatFrom(pixels, w, h, 32, w * BSP_FOG_BPP, SDL_PIXELFORMAT_RGBA32);
}

/**
 * @brief
 */
void EmitLightgrid(void) {

	bsp_file.lightgrid_size = sizeof(bsp_lightgrid_t);
	bsp_file.lightgrid_size += lg.num_luxels * BSP_LIGHTGRID_TEXTURES * BSP_LIGHTGRID_BPP;
	bsp_file.lightgrid_size += lg.num_luxels * BSP_FOG_TEXTURES * BSP_FOG_BPP;

	Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTGRID, bsp_file.lightgrid_size);

	bsp_file.lightgrid->size = lg.size;

	byte *out_ambient = (byte *) bsp_file.lightgrid + sizeof(bsp_lightgrid_t);
	byte *out_diffuse = out_ambient + lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_direction = out_diffuse + lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_caustics = out_direction + lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_fog = out_caustics + lg.num_luxels * BSP_LIGHTGRID_BPP;

	const luxel_t *l = lg.luxels;
	for (int32_t u = 0; u < lg.size.z; u++) {

		SDL_Surface *ambient = CreateLightgridSurfaceFrom(out_ambient, lg.size.x, lg.size.y);
		SDL_Surface *diffuse = CreateLightgridSurfaceFrom(out_diffuse, lg.size.x, lg.size.y);
		SDL_Surface *direction = CreateLightgridSurfaceFrom(out_direction, lg.size.x, lg.size.y);
		SDL_Surface *caustics = CreateLightgridSurfaceFrom(out_caustics, lg.size.x, lg.size.y);
		SDL_Surface *fog = CreateFogSurfaceFrom(out_fog, lg.size.x, lg.size.y);

		for (int32_t t = 0; t < lg.size.y; t++) {
			for (int32_t s = 0; s < lg.size.x; s++, l++) {

				for (int32_t i = 0; i < BSP_LIGHTGRID_BPP; i++) {
					*out_ambient++ = (byte) Clampf(l->ambient.xyz[i] * 255.f, 0.f, 255.f);
					*out_diffuse++ = (byte) Clampf(l->diffuse.xyz[i] * 255.f, 0.f, 255.f);
					*out_direction++ = (byte) Clampf((l->direction.xyz[i] + 1.f) * 0.5f * 255.f, 0.f, 255.f);
					*out_caustics++ = (byte) Clampf(l->caustics.xyz[i] * 255, 0.f, 255.f);
				}

				for (int32_t i = 0; i < BSP_FOG_BPP; i++) {
					*out_fog++ = (byte) Clampf(l->fog.xyzw[i] * 255.f, 0.f, 255.f);
				}
			}
		}

		//IMG_SavePNG(ambient, va("/tmp/%s_lg_ambient_%d.png", map_base, u));
		//IMG_SavePNG(diffuse, va("/tmp/%s_lg_diffuse_%d.png", map_base, u));
		//IMG_SavePNG(direction, va("/tmp/%s_lg_direction_%d.png", map_base, u));
		//IMG_SavePNG(caustics, va("/tmp/%s_lg_caustics_%d.png", map_base, u));
		//IMG_SavePNG(fog, va("/tmp/%s_lg_fog_%d.png", map_base, u));

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse);
		SDL_FreeSurface(direction);
		SDL_FreeSurface(caustics);
		SDL_FreeSurface(fog);
	}
}
