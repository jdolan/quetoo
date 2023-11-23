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
#include "points.h"
#include "polylib.h"
#include "qlight.h"

lightmap_t *lightmaps;

/**
 * @brief Resolves the lightmap winding, offset for the model origin.
 */
static void BuildLightmapWinding(lightmap_t *lm) {

	lm->winding = Cm_WindingForFace(&bsp_file, lm->face);

	for (int32_t i = 0; i < lm->winding->num_points; i++) {
		lm->winding->points[i] = Vec3_Add(lm->winding->points[i], lm->model_origin);
	}
}

/**
 * @brief Resolves the texture projection matrices for the given lightmap.
 */
static void BuildLightmapMatrices(lightmap_t *lm) {

	vec3_t s = Vec3_Normalize(Vec4_XYZ(lm->brush_side->axis[0]));
	vec3_t t = Vec3_Normalize(Vec4_XYZ(lm->brush_side->axis[1]));

	s = Vec3_Scale(s, 1.f / BSP_LIGHTMAP_LUXEL_SIZE);
	t = Vec3_Scale(t, 1.f / BSP_LIGHTMAP_LUXEL_SIZE);

	lm->matrix = Mat4((const float[]) {
		s.x, t.x, lm->plane->normal.x, 0.f,
		s.y, t.y, lm->plane->normal.y, 0.f,
		s.z, t.z, lm->plane->normal.z, 0.f,
		0.f, 0.f, -lm->plane->dist,    1.f
	});

	const mat4_t translate = Mat4_FromTranslation(lm->model_origin);
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

	// add 2 luxels of padding around the lightmap for bilinear filtering
	lm->w = floorf(lm->st_maxs.x - lm->st_mins.x) + 2;
	lm->h = floorf(lm->st_maxs.y - lm->st_mins.y) + 2;
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

/**
 * @brief Authors a .map file which can be imported into Radiant to view the luxel projections.
 */
static void DebugLightmapLuxels(void) {

	file_t *file = NULL;

	lightmap_t *lm = lightmaps;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, lm++) {

		if (lm->brush_side->surface & SURF_DEBUG_LUXEL) {

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

				Fs_Print(file, "{\n");
				Fs_Print(file, "  \"classname\" \"info_luxel\"\n");
				Fs_Print(file, "  \"origin\" \"%g %g %g\"\n", l->origin.x, l->origin.y, l->origin.z);
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

		const cm_entity_t *entity = Cm_Bsp()->entities[model->entity];
		const vec3_t model_origin = Cm_EntityValue(entity, "origin")->vec3;

		for (int32_t j = 0; j < model->num_faces; j++) {
			lightmaps[model->first_face + j].model = model;
			lightmaps[model->first_face + j].model_origin = model_origin;
		}
	}

	const bsp_face_t *face = bsp_file.faces;
	for (int32_t i = 0; i < bsp_file.num_faces; i++, face++) {

		lightmap_t *lm = &lightmaps[i];

		assert(lm->node);
		assert(lm->model);
		
		lm->face = &bsp_file.faces[i];
		lm->brush_side = &bsp_file.brush_sides[lm->face->brush_side];
		lm->plane = &bsp_file.planes[lm->face->plane];
		lm->material = &materials[lm->brush_side->material];

		if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		BuildLightmapWinding(lm);

		BuildLightmapMatrices(lm);

		BuildLightmapExtents(lm);

		BuildLightmapLuxels(lm);
	}

	DebugLightmapLuxels();
}

/**
 * @brief Calculate the Phong-interpolated normal vector for a given luxel using barycentric
 * coordinates. This reqiures first finding the triangle within the face that best encloses
 * the luxel origin. The triangles are expanded to help with luxels residing along edges.
 */
static vec3_t LuxelNormal(const lightmap_t *lm, const vec3_t origin) {

	vec3_t normal = lm->plane->normal;

	float best = FLT_MAX;

	const int32_t *e = bsp_file.elements + lm->face->first_element;
	for (int32_t i = 0; i < lm->face->num_elements; i += 3, e += 3) {

		const bsp_vertex_t *v0 = bsp_file.vertexes + e[0];
		const bsp_vertex_t *v1 = bsp_file.vertexes + e[1];
		const bsp_vertex_t *v2 = bsp_file.vertexes + e[2];

		const vec3_t center = Vec3_Scale(Vec3_Add(Vec3_Add(v0->position, v1->position), v2->position), 1.f / 3.f);
		const float padding = BSP_LIGHTMAP_LUXEL_SIZE * 1.f;

		const vec3_t a = Vec3_Fmaf(v0->position, padding, Vec3_Direction(v0->position, center));
		const vec3_t b = Vec3_Fmaf(v1->position, padding, Vec3_Direction(v1->position, center));
		const vec3_t c = Vec3_Fmaf(v2->position, padding, Vec3_Direction(v2->position, center));

		vec3_t out;
		const float bary = Cm_Barycentric(a, b, c, origin, &out);
		const float delta = fabsf(1.f - bary);
		if (delta < best) {
			best = delta;

			normal = Vec3_Zero();
			normal = Vec3_Fmaf(normal, out.x, v0->normal);
			normal = Vec3_Fmaf(normal, out.y, v1->normal);
			normal = Vec3_Fmaf(normal, out.z, v2->normal);
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

	const float padding_s = ((lm->st_maxs.x - lm->st_mins.x) - lm->w) * 0.5f;
	const float padding_t = ((lm->st_maxs.y - lm->st_mins.y) - lm->h) * 0.5f;

	const float s = lm->st_mins.x + padding_s + l->s + 0.5f + soffs;
	const float t = lm->st_mins.y + padding_t + l->t + 0.5f + toffs;

	l->origin = Mat4_Transform(lm->inverse_matrix, Vec3(s, t, 0.f));
	l->normal = LuxelNormal(lm, l->origin);
	l->origin = Vec3_Fmaf(l->origin, ON_EPSILON, l->normal);
	l->dist = Vec3_Dot(l->origin, l->normal);

	return Light_PointContents(l->origin, lm->model->head_node);
}

/**
 * @brief
 */
static void LightmapLuxel_Ambient(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {

	const float padding_s = ((lightmap->st_maxs.x - lightmap->st_mins.x) - lightmap->w) * 0.5f;
	const float padding_t = ((lightmap->st_maxs.y - lightmap->st_mins.y) - lightmap->h) * 0.5f;

	const float s = lightmap->st_mins.x + padding_s + luxel->s + 0.5f;
	const float t = lightmap->st_mins.y + padding_t + luxel->t + 0.5f;

	const vec3_t points[] = DOME_COSINE_16X;
	float sample_fraction = scale / lengthof(points);

	lumen_t lumen = {
		.light = light
	};

	for (size_t i = 0; i < lengthof(points); i++) {

		vec3_t sample = points[i];

		// Add some jitter to hide undersampling
		sample.x += RandomRangef(-.02f, .02f);
		sample.y += RandomRangef(-.02f, .02f);

		// Scale the sample and move it into position
		sample = Vec3_Scale(sample, light->radius / BSP_LIGHTMAP_LUXEL_SIZE);

		sample.x += s;
		sample.y += t;

		const vec3_t point = Mat4_Transform(lightmap->inverse_matrix, sample);
		const cm_trace_t trace = Light_Trace(luxel->origin, point, lightmap->model->head_node, CONTENTS_SOLID);

		const float lumens = sample_fraction * trace.fraction * light->intensity;
		lumen.color = Vec3_Fmaf(lumen.color, lumens, light->color);
	}

	if (!Vec3_Equal(lumen.color, Vec3_Zero())) {
		Luxel_Illuminate(luxel, &lumen);
	}
}

/**
 * @brief
 */
static void LightmapLuxel_Sun(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {

	for (int32_t i = 0; i < light->num_points; i++) {

		const vec3_t dir = Vec3_Negate(light->points[i]);
		const vec3_t end = Vec3_Fmaf(luxel->origin, MAX_WORLD_DIST, dir);
		const cm_trace_t trace = Light_Trace(luxel->origin, end, lightmap->model->head_node, CONTENTS_SOLID);

		if (trace.surface & SURF_SKY) {
			const float lumens = (1.f / light->num_points) * scale * light->intensity;

			Luxel_Illuminate(luxel, &(const lumen_t) {
				.light = light,
				.color = Vec3_Scale(light->color, lumens),
				.direction = Vec3_Scale(dir, lumens)
			});
		}
	}
}

/**
 * @brief
 */
static void LightmapLuxel_Point(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {
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

	const float lumens = atten * scale * light->intensity;

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], lightmap->model->head_node, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
		break;
	}
}

/**
 * @brief
 */
static void LightmapLuxel_Spot(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {
	vec3_t dir;

	float dist = Vec3_DistanceDir(light->origin, luxel->origin, &dir);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
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

	const float dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
	atten *= Smoothf(dot, light->theta - light->phi, light->theta + light->phi);

	const float lumens = atten * scale * light->intensity;
	if (lumens <= 0.f) {
		return;
	}

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], lightmap->model->head_node, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
		break;
	}
}

/**
 * @brief
 */
static void LightmapLuxel_Face(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {
	vec3_t dir;

	if (light->model != bsp_file.models && light->model != lightmap->model) {
		return;
	}

	if (Vec3_Dot(luxel->origin, light->plane->normal) - light->plane->dist < -BSP_LIGHTMAP_LUXEL_SIZE) {
		return;
	}

	if (Vec3_Dot(light->origin, luxel->normal) - luxel->dist < -BSP_LIGHTMAP_LUXEL_SIZE) {
		return;
	}

	// For neighboring emissive faces of the same material, do not light each other. This avoids
	// light seams on slime, lava, and other large emissive brush sides that are split by BSP.

	float dist = Cm_DistanceToWinding(light->winding, luxel->origin, &dir);

	if (light->plane == lightmap->plane) {
		if (light->brush_side->material == lightmap->brush_side->material) {
			if (light->face == lightmap->face) {
				dist = 0.f;
				dir = Vec3_Negate(light->normal);
			} else {
				return;
			}
		}
	}

	if (dist > light->radius) {
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

	const float dot = Vec3_Dot(dir, Vec3_Negate(light->normal));
	atten *= Smoothf(dot, light->theta - light->phi, light->theta + light->phi);

	const float lumens = atten * scale * light->intensity;
	if (lumens <= 0.f) {
		return;
	}

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], lightmap->model->head_node, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, lumens),
			.direction = Vec3_Scale(dir, lumens)
		});
		break;
	}
}

/**
 * @brief
 */
static void LightmapLuxel_Patch(const light_t *light, const lightmap_t *lightmap, luxel_t *luxel, float scale) {

	if (light->model != bsp_file.models && light->model != lightmap->model) {
		return;
	}

	if (Vec3_Dot(luxel->origin, light->plane->normal) - light->plane->dist < -BSP_LIGHTMAP_LUXEL_SIZE) {
		return;
	}

	if (Vec3_Dot(light->origin, luxel->normal) - luxel->dist < -BSP_LIGHTMAP_LUXEL_SIZE) {
		return;
	}

	if (light->plane == lightmap->plane && light->brush_side->material == lightmap->brush_side->material) {
		return;
	}

	float dist = Vec3_Distance(light->origin, luxel->origin);
	dist = Maxf(0.f, dist - light->size * .5f);
	if (dist > light->radius) {
		return;
	}

	const float atten = Clampf(1.f - dist / light->radius, 0.f, 1.f);

	const float lumens = atten * scale * light->intensity;
	if (lumens <= 0.f) {
		return;
	}

	for (int32_t i = 0; i < light->num_points; i++) {

		const cm_trace_t trace = Light_Trace(luxel->origin, light->points[i], lightmap->model->head_node, CONTENTS_SOLID);
		if (trace.fraction < 1.f) {
			continue;
		}

		Luxel_Illuminate(luxel, &(const lumen_t) {
			.light = light,
			.color = Vec3_Scale(light->color, lumens),
		});
		break;
	}
}

/**
 * @brief Iterates lights, accumulating color and direction on the specified luxel.
 * @param lights The light sources to iterate.
 * @param lightmap The lightmap containing the luxel.
 * @param luxel The luxel to light.
 * @param scale A scalar applied to both color and direction.
 */
static inline void LightmapLuxel(const GPtrArray *lights, const lightmap_t *lightmap, luxel_t *luxel, float scale) {

	for (guint i = 0; i < lights->len; i++) {

		const light_t *light = g_ptr_array_index(lights, i);

		switch (light->type) {
			case LIGHT_AMBIENT:
				LightmapLuxel_Ambient(light, lightmap, luxel, scale);
				break;
			case LIGHT_SUN:
				LightmapLuxel_Sun(light, lightmap, luxel, scale);
				break;
			case LIGHT_POINT:
				LightmapLuxel_Point(light, lightmap, luxel, scale);
				break;
			case LIGHT_SPOT:
				LightmapLuxel_Spot(light, lightmap, luxel, scale);
				break;
			case LIGHT_FACE:
				LightmapLuxel_Face(light, lightmap, luxel, scale);
				break;
			case LIGHT_PATCH:
				LightmapLuxel_Patch(light, lightmap, luxel, scale);
				break;
			default:
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

	if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	const GPtrArray *lights = node_lights[lm->node - bsp_file.nodes];

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		for (size_t j = 0; j < lengthof(offsets); j++) {

			const float soffs = offsets[j].x;
			const float toffs = offsets[j].y;

			const float weight = antialias ? offsets[j].z : 1.f;

			if (ProjectLightmapLuxel(lm, l, soffs, toffs) == CONTENTS_SOLID) {
				continue;
			}

			LightmapLuxel(lights, lm, l, weight);
			if (!antialias) {
				break;
			}
		}

		// For inline models, always add ambient light sources, even if the sample resides
		// in solid. This prevents completely unlit tops of doors, bottoms of plats, etc.

		if (lm->model != bsp_file.models && l->diffuse->light == NULL) {
			for (guint j = 0; j < lights->len; j++) {
				const light_t *light = g_ptr_array_index(lights, j);
				if (light->type == LIGHT_AMBIENT) {
					Luxel_Illuminate(l, &(const lumen_t) {
						.light = light,
						.color = light->color
					});
					break;
				}
			}
		}
	}
}

/**
 * @brief Calculates indirect lighting for the given face.
 */
void IndirectLightmap(int32_t face_num) {

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	const GPtrArray *lights = node_lights[lm->node - bsp_file.nodes];

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		if (ProjectLightmapLuxel(lm, l, 0.f, 0.f) == CONTENTS_SOLID) {
			continue;
		}

		LightmapLuxel(lights, lm, l, 1.f);
	}
}

/**
 * @brief
 */
static void CausticsLightmapLuxel(luxel_t *luxel, float scale) {

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
 * @brief Calculates caustics lighting for the given face.
 */
void CausticsLightmap(int32_t face_num) {

	const lightmap_t *lm = &lightmaps[face_num];

	if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		if (ProjectLightmapLuxel(lm, l, 0.f, 0.f) == CONTENTS_SOLID) {
			continue;
		}

		CausticsLightmapLuxel(l, 1.f);
	}
}

/**
 * @brief
 */
static SDL_Surface *CreateLightmapSurfaceRGB32F(int32_t w, int32_t h) {
	return CreateLuxelSurface(w, h, sizeof(vec3_t), Mem_TagMalloc(w * h * sizeof(vec3_t), MEM_TAG_LIGHTMAP));
}

/**
 * @brief
 */
static SDL_Surface *CreateLightmapSurfaceRGB8(int32_t w, int32_t h) {
	return CreateLuxelSurface(w, h, sizeof(color24_t), Mem_TagMalloc(w * h * sizeof(color24_t), MEM_TAG_LIGHTMAP));
}

/**
 * @brief
 */
static void FinalizeLightmapLuxel(const lightmap_t *lightmap, luxel_t *luxel) {

	// re-project the luxel so that it reflects its centered normal vector
	ProjectLightmapLuxel(lightmap, luxel, 0.f, 0.f);

	// calculate the tangents to encode the light direction
	const vec3_t sdir = Vec4_XYZ(lightmap->brush_side->axis[0]);
	const vec3_t tdir = Vec4_XYZ(lightmap->brush_side->axis[1]);

	vec3_t tangent, bitangent;
	Vec3_Tangents(luxel->normal, sdir, tdir, &tangent, &bitangent);

	lumen_t *diffuse = luxel->diffuse;
	for (int32_t i = 0; i < BSP_LIGHTMAP_CHANNELS; i++, diffuse++) {

		// lerp the direction with the normal, according to its intensity
		const float intensity = Clampf(Vec3_Length(diffuse->direction), 0.f, 1.f);
		const vec3_t dir = Vec3_Normalize(Vec3_Mix(Vec3_Normalize(diffuse->direction), luxel->normal, 1.f - intensity));

		//assert(Vec3_Dot(dir, luxel->normal) >= 0.f);

		// transform the direction into tangent space
		diffuse->direction.x = Vec3_Dot(dir, tangent);
		diffuse->direction.y = Vec3_Dot(dir, bitangent);
		diffuse->direction.z = Vec3_Dot(dir, luxel->normal);

		diffuse->direction = Vec3_Normalize(diffuse->direction);

		//assert(diffuse->direction.z >= 0.f);
	}

	// and clamp the cuastics
	luxel->caustics = Vec3_Clampf(luxel->caustics, 0.f, 1.f);
}

/**
 * @brief Finalize light values for the given face, and create its lightmap textures.
 */
void FinalizeLightmap(int32_t face_num) {

	lightmap_t *lm = &lightmaps[face_num];

	if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
		return;
	}

	lm->ambient = CreateLightmapSurfaceRGB8(lm->w, lm->h);
	color24_t *out_ambient = lm->ambient->pixels;

	vec3_t *out_diffuse[BSP_LIGHTMAP_CHANNELS];
	vec3_t *out_direction[BSP_LIGHTMAP_CHANNELS];

	for (int32_t c = 0; c < BSP_LIGHTMAP_CHANNELS; c++) {
		lm->diffuse[c] = CreateLightmapSurfaceRGB32F(lm->w, lm->h);
		out_diffuse[c] = lm->diffuse[c]->pixels;

		lm->direction[c] = CreateLightmapSurfaceRGB32F(lm->w, lm->h);
		out_direction[c] = lm->direction[c]->pixels;
	}

	lm->caustics = CreateLightmapSurfaceRGB8(lm->w, lm->h);
	color24_t *out_caustics = lm->caustics->pixels;

	// write it out
	luxel_t *l = lm->luxels;
	for (size_t i = 0; i < lm->num_luxels; i++, l++) {

		FinalizeLightmapLuxel(lm, l);

		*out_ambient++ = Color_Color24(Color3fv(l->ambient.color));

		for (int32_t c = 0; c < BSP_LIGHTMAP_CHANNELS; c++) {
			*out_diffuse[c]++ = l->diffuse[c].color;
			*out_direction[c]++ = l->diffuse[c].direction;
		}

		*out_caustics++ = Color_Color24(Color3fv(l->caustics));
	}
}

/**
 * @brief
 */
void EmitLightmap(void) {

	atlas_t *atlas = Atlas_Create(BSP_LIGHTMAP_LAST);
	assert(atlas);

	atlas->blit = BlitLuxelSurface;

	atlas_node_t *nodes[bsp_file.num_faces];

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
			continue;
		}

		nodes[i] = Atlas_Insert(atlas,
								lm->ambient,
								lm->diffuse[0],
								lm->direction[0],
								lm->diffuse[1],
								lm->direction[1],
								lm->caustics);
	}

	int32_t width;
	for (width = MIN_BSP_LIGHTMAP_WIDTH; width <= MAX_BSP_LIGHTMAP_WIDTH; width += 256) {

		const int32_t layer_size = width * width;

		bsp_file.lightmap_size = sizeof(bsp_lightmap_t);
		bsp_file.lightmap_size += layer_size * sizeof(color24_t);
		bsp_file.lightmap_size += layer_size * sizeof(vec3_t);
		bsp_file.lightmap_size += layer_size * sizeof(vec3_t);
		bsp_file.lightmap_size += layer_size * sizeof(vec3_t);
		bsp_file.lightmap_size += layer_size * sizeof(vec3_t);
		bsp_file.lightmap_size += layer_size * sizeof(color24_t);

		Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTMAP, bsp_file.lightmap_size);
		memset(bsp_file.lightmap, 0, bsp_file.lightmap_size);

		bsp_file.lightmap->width = width;

		byte *out = (byte *) bsp_file.lightmap + sizeof(bsp_lightmap_t);

		SDL_Surface *ambient = CreateLuxelSurface(width, width, sizeof(color24_t), out);
		out += layer_size * sizeof(color24_t);

		SDL_Surface *diffuse0 = CreateLuxelSurface(width, width, sizeof(vec3_t), out);
		out += layer_size * sizeof(vec3_t);

		SDL_Surface *direction0 = CreateLuxelSurface(width, width, sizeof(vec3_t), out);
		out += layer_size * sizeof(vec3_t);

		SDL_Surface *diffuse1 = CreateLuxelSurface(width, width, sizeof(vec3_t), out);
		out += layer_size * sizeof(vec3_t);

		SDL_Surface *direction1 = CreateLuxelSurface(width, width, sizeof(vec3_t), out);
		out += layer_size * sizeof(vec3_t);

		SDL_Surface *caustics = CreateLuxelSurface(width, width, sizeof(color24_t), out);
		out += layer_size * sizeof(color24_t);

		if (Atlas_Compile(atlas, 0, ambient, diffuse0, direction0, diffuse1, direction1, caustics) == 0) {

			if (debug) {
				WriteLuxelSurface(ambient, va("/tmp/%s_lm_ambient.png", map_base));
				WriteLuxelSurface(diffuse0, va("/tmp/%s_lm_diffuse0.png", map_base));
				WriteLuxelSurface(direction0, va("/tmp/%s_lm_direction0.png", map_base));
				WriteLuxelSurface(diffuse1, va("/tmp/%s_lm_diffuse1.png", map_base));
				WriteLuxelSurface(direction1, va("/tmp/%s_lm_direction1.png", map_base));
				WriteLuxelSurface(caustics, va("/tmp/%s_lm_caustics.png", map_base));
			}

			SDL_FreeSurface(ambient);
			SDL_FreeSurface(diffuse0);
			SDL_FreeSurface(direction0);
			SDL_FreeSurface(diffuse1);
			SDL_FreeSurface(direction1);
			SDL_FreeSurface(caustics);

			break;
		}

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse0);
		SDL_FreeSurface(direction0);
		SDL_FreeSurface(diffuse1);
		SDL_FreeSurface(direction1);
		SDL_FreeSurface(caustics);
	}

	if (width > MAX_BSP_LIGHTMAP_WIDTH) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTMAP_WIDTH\n");
	}

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		lightmap_t *lm = &lightmaps[i];

		if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
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
		SDL_FreeSurface(lm->diffuse[0]);
		SDL_FreeSurface(lm->direction[0]);
		SDL_FreeSurface(lm->diffuse[1]);
		SDL_FreeSurface(lm->direction[1]);
		SDL_FreeSurface(lm->caustics);
	}

	Atlas_Destroy(atlas);
}

/**
 * @brief
 */
void EmitLightmapTexcoords(void) {

	for (int32_t i = 0; i < bsp_file.num_faces; i++) {
		const lightmap_t *lm = &lightmaps[i];

		if (lm->brush_side->surface & SURF_MASK_NO_LIGHTMAP) {
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
