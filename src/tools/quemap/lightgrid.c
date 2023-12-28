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
#include "lightgrid.h"
#include "points.h"
#include "qlight.h"
#include "simplex.h"

lightgrid_t lg;

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

				l->lights = g_ptr_array_new();
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

	Luxel_Illuminate(luxel, &(const lumen_t) {
		.light = light,
		.color = Vec3_Scale(light->color, light->intensity * scale)
	});
}

/**
 * @brief
 */
static void LightgridLuxel_Sun(const light_t *light, luxel_t *luxel, float scale) {

	const float lumens = (1.f / light->num_points) * scale;

	for (int32_t i = 0; i < light->num_points; i++) {

		const vec3_t dir = Vec3_Negate(light->points[i]);
		const vec3_t end = Vec3_Fmaf(luxel->origin, MAX_WORLD_DIST, dir);

		const cm_trace_t trace = Light_Trace(luxel->origin, end, 0, CONTENTS_SOLID);
		if (trace.surface & SURF_SKY) {
			Luxel_Illuminate(luxel, &(const lumen_t) {
				.light = light,
				.color = Vec3_Scale(light->color, light->intensity * lumens),
				.direction = Vec3_Scale(dir, lumens)
			});
		}
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Point(const light_t *light, luxel_t *luxel, float scale) {

	const float dist = Maxf(0.f, Vec3_Distance(light->origin, luxel->origin) - light->size * .5f);
	if (dist >= light->radius) {
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

	const float lumens = atten * scale / light->num_points;

	for (int32_t i = 0; i < light->num_points; i++) {

		const vec3_t dir = Vec3_Direction(light->points[i], luxel->origin);

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, light->intensity * lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Spot(const light_t *light, luxel_t *luxel, float scale) {
	
	const float dist = Maxf(0.f, Vec3_Distance(light->origin, luxel->origin) - light->size * .5f);
	if (dist >= light->radius) {
		return;
	}

	float atten;

	switch (light->atten) {
		case LIGHT_ATTEN_NONE:
			atten = 1.f;
			break;
		case LIGHT_ATTEN_LINEAR:
			atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
			break;
		case LIGHT_ATTEN_INVERSE_SQUARE:
			atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
			atten *= atten;
			break;
	}

	for (int32_t i = 0; i < light->num_points; i++) {

		float lumens = atten * scale / light->num_points;

		const vec3_t dir = Vec3_Direction(light->points[i], luxel->origin);
		const float dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
		if (dot < light->theta) {

			if (dot < light->phi) {
				continue;
			}

			lumens *= Smoothf(dot, light->phi, light->theta);
		}

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, light->intensity * lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Face(const light_t *light, luxel_t *luxel, float scale) {

	if (light->model != bsp_file.models) {
		return;
	}

	const float dist = Cm_DistanceToWinding(light->winding, luxel->origin, NULL);
	if (dist >= light->radius) {
		return;
	}

	float atten;

	switch (light->atten) {
		case LIGHT_ATTEN_NONE:
			atten = 1.f;
			break;
		case LIGHT_ATTEN_LINEAR:
			atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
			break;
		case LIGHT_ATTEN_INVERSE_SQUARE:
			atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
			atten *= atten;
			break;
	}

	for (int32_t i = 0; i < light->num_points; i++) {

		float lumens = atten * scale / light->num_points;

		const vec3_t dir = Vec3_Direction(light->points[i], luxel->origin);
		const float dot = Vec3_Dot(Vec3_Negate(dir), light->normal);
		if (dot <= light->theta) {

			if (dot <= light->phi) {
				continue;
			}

			lumens *= Smoothf(dot, light->phi, light->theta);
		}

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, light->intensity * lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
	}
}

/**
 * @brief
 */
static void LightgridLuxel_Patch(const light_t *light, luxel_t *luxel, float scale) {

	if (light->model != bsp_file.models) {
		return;
	}

	if (Vec3_Dot(luxel->origin, light->plane->normal) - light->plane->dist < ON_EPSILON) {
		return;
	}

	const float dist = Maxf(0.f, Vec3_Distance(light->origin, luxel->origin) - light->size * .5f);
	if (dist >= light->radius) {
		return;
	}

	const float atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);
	const float lumens = atten * scale / light->num_points;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], 0, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, light->intensity * lumens)
		});
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
			case LIGHT_FACE:
				LightgridLuxel_Face(light, luxel, scale);
				break;
			case LIGHT_PATCH:
				LightgridLuxel_Patch(light, luxel, scale);
				break;
			default:
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

	const float weight = 1.f / lengthof(offsets);

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < lengthof(offsets); i++) {

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

		LightgridLuxel(lights, l, weight);
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

	const float weight = 1.f / lengthof(offsets);

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < lengthof(offsets); i++) {

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

		LightgridLuxel(lights, l, weight);
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

	luxel->caustics = Vec3_Fmaf(luxel->caustics, scale * caustic_intensity, c);
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

	const float weight = 1.f / lengthof(offsets);

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < lengthof(offsets); i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		CausticsLightgridLuxel(l, weight);
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

		const vec3_t light = Vec3_Add(l->ambient, l->diffuse);
		const vec3_t color = Vec3_Fmaf(fog->color, Clampf(fog->absorption, 0.f, 1.f), light);

		switch (fog->type) {
			case FOG_INVALID:
				break;
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

	const float weight = 1.f / lengthof(offsets);

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < lengthof(offsets); i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		FogLightgridLuxel(fogs, l, weight);
	}
}

/**
 * @brief
 */
void FinalizeLightgrid(int32_t luxel_num) {

	luxel_t *luxel = &lg.luxels[luxel_num];

	if (Vec3_Equal(luxel->direction, Vec3_Zero())) {
		luxel->direction = Vec3_Up();
	}
}

/**
 * @brief
 */
void EmitLightgrid(void) {

	bsp_file.lightgrid_size = sizeof(bsp_lightgrid_t);
	bsp_file.lightgrid_size += lg.num_luxels * sizeof(color24_t);
	bsp_file.lightgrid_size += lg.num_luxels * sizeof(vec3_t);
	bsp_file.lightgrid_size += lg.num_luxels * sizeof(color24_t);
	bsp_file.lightgrid_size += lg.num_luxels * sizeof(color24_t);
	bsp_file.lightgrid_size += lg.num_luxels * sizeof(color32_t);

	Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTGRID, bsp_file.lightgrid_size);
	memset(bsp_file.lightgrid, 0, bsp_file.lightgrid_size);

	bsp_file.lightgrid->size = lg.size;

	byte *out = (byte *) bsp_file.lightgrid + sizeof(bsp_lightgrid_t);

	color24_t *out_ambient = (color24_t *) out;
	out += lg.num_luxels * sizeof(color24_t);

	vec3_t *out_diffuse = (vec3_t *) out;
	out += lg.num_luxels * sizeof(vec3_t);

	color24_t *out_direction = (color24_t *) out;
	out += lg.num_luxels * sizeof(color24_t);

	color24_t *out_caustics = (color24_t *) out;
	out += lg.num_luxels * sizeof(color24_t);

	color32_t *out_fog = (color32_t *) out;
	out += lg.num_luxels * sizeof(color32_t);

	const luxel_t *l = lg.luxels;
	for (int32_t u = 0; u < lg.size.z; u++) {

		SDL_Surface *ambient = CreateLuxelSurface(lg.size.x, lg.size.y, sizeof(color24_t), out_ambient);
		SDL_Surface *diffuse = CreateLuxelSurface(lg.size.x, lg.size.y, sizeof(vec3_t), out_diffuse);
		SDL_Surface *direction = CreateLuxelSurface(lg.size.x, lg.size.y, sizeof(color24_t), out_direction);
		SDL_Surface *caustics = CreateLuxelSurface(lg.size.x, lg.size.y, sizeof(color24_t), out_caustics);
		SDL_Surface *fog = CreateLuxelSurface(lg.size.x, lg.size.y, sizeof(color32_t), out_fog);

		for (int32_t t = 0; t < lg.size.y; t++) {
			for (int32_t s = 0; s < lg.size.x; s++, l++) {

				*out_ambient++ = Color_Color24(Color3fv(l->ambient));
				*out_diffuse++ = l->diffuse;
				*out_direction++ = Color24i(Vec3_Bytes(l->direction));
				*out_caustics++ = Color_Color24(Color3fv(l->caustics));
				*out_fog++ = Color_Color32(Color4fv(l->fog));
			}
		}

		if (debug) {
			WriteLuxelSurface(ambient, va("/tmp/%s_lg_ambient_%d.png", map_base, u));
			WriteLuxelSurface(diffuse, va("/tmp/%s_lg_diffuse_%d.png", map_base, u));
			WriteLuxelSurface(direction, va("/tmp/%s_lg_direction_%d.png", map_base, u));
			WriteLuxelSurface(caustics, va("/tmp/%s_lg_caustics_%d.png", map_base, u));
			WriteLuxelSurface(fog, va("/tmp/%s_lg_fog_%d.png", map_base, u));
		}

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse);
		SDL_FreeSurface(direction);
		SDL_FreeSurface(caustics);
		SDL_FreeSurface(fog);
	}
}
