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

	Matrix4x4_CreateTranslate(&lg.matrix, -world->mins.x, -world->mins.y, -world->mins.z);
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

	const vec3_t mins = Vec3(world->mins.x, world->mins.y, world->mins.z);
	const vec3_t maxs = Vec3(world->maxs.x, world->maxs.y, world->maxs.z);

	Matrix4x4_Transform(&lg.matrix, mins.xyz, lg.stu_mins.xyz);
	Matrix4x4_Transform(&lg.matrix, maxs.xyz, lg.stu_maxs.xyz);

	for (int32_t i = 0; i < 3; i++) {
		lg.size.xyz[i] = floorf(lg.stu_maxs.xyz[i] - lg.stu_mins.xyz[i]) + 2;
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
			}
		}
	}
}

/**
 * @brief
 */
static int32_t ProjectLightgridLuxel(luxel_t *l, float soffs, float toffs, float uoffs) {

	const float padding_s = ((lg.stu_maxs.x - lg.stu_mins.x) - lg.size.x) * 0.5;
	const float padding_t = ((lg.stu_maxs.y - lg.stu_mins.y) - lg.size.y) * 0.5;
	const float padding_u = ((lg.stu_maxs.z - lg.stu_mins.z) - lg.size.z) * 0.5;

	const float s = lg.stu_mins.x + padding_s + l->s + 0.5 + soffs;
	const float t = lg.stu_mins.y + padding_t + l->t + 0.5 + toffs;
	const float u = lg.stu_mins.z + padding_u + l->u + 0.5 + uoffs;

	const vec3_t stu = Vec3(s, t, u);
	Matrix4x4_Transform(&lg.inverse_matrix, stu.xyz, l->origin.xyz);

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
 * @brief Iterates the specified lights, accumulating color and direction to the appropriate buffers.
 * @param lights The lights to iterate.
 * @param luxel The luxel to light.
 * @param scale A scalar applied to both light and direction.
 */
static void LightLuxel(const GPtrArray *lights, luxel_t *luxel, float scale) {

	for (guint i = 0; i < lights->len; i++) {

		const light_t *light = g_ptr_array_index(lights, i);

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

		float intensity = Clampf(light->radius, 0.f, MAX_WORLD_COORD);

		switch (light->type) {
			case LIGHT_SPOT: {
				const float cone_dot = Vec3_Dot(dir, light->normal);
				const float thresh = cosf(light->theta);
				const float smooth = 0.03f;
				intensity *= Smoothf(thresh - smooth, thresh + smooth, cone_dot);
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

		if (light->type == LIGHT_AMBIENT) {

			const bsp_model_t *world = bsp_file.models;

			const float padding_s = (world->maxs.x - world->mins.x) - (lg.size.x * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5f;
			const float padding_t = (world->maxs.y - world->mins.y) - (lg.size.y * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5f;
			const float padding_u = (world->maxs.z - world->mins.z) - (lg.size.z * BSP_LIGHTGRID_LUXEL_SIZE) * 0.5f;

			const float s = lg.stu_mins.x + padding_s + luxel->s + 0.5f;
			const float t = lg.stu_mins.y + padding_t + luxel->t + 0.5f;
			const float u = lg.stu_mins.z + padding_u + luxel->u + 0.5f;

			const vec3_t points[] = CUBE_8;
			const float ao_radius = 64.f;

			float occlusion = 0.f;
			float sample_fraction = 1.f / lengthof(points);

			for (size_t i = 0; i < lengthof(points); i++) {

				vec3_t sample = points[i];

				// Add some jitter to hide undersampling
				sample.x += RandomRangef(-.04f, .04f);
				sample.y += RandomRangef(-.04f, .04f);
				sample.z += RandomRangef(-.04f, .04f);

				// Scale the sample and move it into position
				sample = Vec3_Scale(sample, ao_radius);

				sample.x += s;
				sample.y += t;
				sample.z += u;

				vec3_t point;
				Matrix4x4_Transform(&lg.inverse_matrix, sample.xyz, point.xyz);

				const cm_trace_t trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);

				occlusion += sample_fraction * trace.fraction;
			}

			intensity *= 1.f - (1.f - occlusion) * (1.f - occlusion);

		} else if (light->type == LIGHT_SUN) {

			const vec3_t sun_origin = Vec3_Fmaf(luxel->origin, -MAX_WORLD_DIST, light->normal);

			cm_trace_t trace = Light_Trace(luxel->origin, sun_origin, 0, CONTENTS_SOLID);
			if (!(trace.texinfo && (trace.texinfo->flags & SURF_SKY))) {
				float exposure = 0.f;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Fmaf(sun_origin, i * LIGHT_SIZE_STEP, points[j]);

						trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);
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
			cm_trace_t trace = Light_Trace(luxel->origin, light->origin, 0, CONTENTS_SOLID);
			if (trace.fraction < 1.f) {
				float exposure = 0.f;

				const int32_t num_samples = ceilf(light->size / LIGHT_SIZE_STEP);
				for (int32_t i = 0; i < num_samples; i++) {

					const vec3_t points[] = CUBE_8;
					for (size_t j = 0; j < lengthof(points); j++) {

						const vec3_t point = Vec3_Fmaf(light->origin, (i + 1) * LIGHT_SIZE_STEP, points[j]);

						trace = Light_Trace(luxel->origin, point, 0, CONTENTS_SOLID);
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

		LightLuxel(lights, l, 1.f);
		break;
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
	const size_t num_offsets = antialias ? lengthof(offsets) : 1;

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < num_offsets; i++) {

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

		LightLuxel(lights, l, 1.f);
	}
}

/**
 * @brief Permutation vector for simplex noise
 */
static const uint8_t simplex_perm[256] = {
    151, 160, 137, 91, 90, 15,
    131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
    190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
    88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
    77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
    102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
    135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
    5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
    223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
    129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
    251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
    49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
    138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

static inline uint8_t SimplexHash(int32_t i) {
    return simplex_perm[(uint8_t)i];
}

/**
 * Helper functions to compute gradients-dot-residual vectors (3D)
 *
 * @param[in] hash  hash value
 * @param[in] x     x coord of the distance to the corner
 * @param[in] y     y coord of the distance to the corner
 * @param[in] z     z coord of the distance to the corner
 *
 * @return gradient value
 */
static float SimplexGrad(int32_t hash, float x, float y, float z) {
    int32_t h = hash & 15;     // Convert low 4 bits of hash code into 12 simple
    float u = h < 8 ? x : y; // gradient directions, and compute dot product.
    float v = h < 4 ? y : h == 12 || h == 14 ? x : z; // Fix repeats at h = 12 to 15
    return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

/**
 * 3D Perlin simplex noise
 *
 * @param[in] x float coordinate
 * @param[in] y float coordinate
 * @param[in] z float coordinate
 *
 * @return Noise value in the range[-1; 1], value of 0 on all integer coordinates.
 */
float SimplexNoise(float x, float y, float z) {
    float n0, n1, n2, n3; // Noise contributions from the four corners

    // Skewing/Unskewing factors for 3D
    static const float F3 = 1.0f / 3.0f;
    static const float G3 = 1.0f / 6.0f;

    // Skew the input space to determine which simplex cell we're in
    float s = (x + y + z) * F3; // Very nice and simple skew factor for 3D
    int32_t i = (int32_t)floorf(x + s);
    int32_t j = (int32_t)floorf(y + s);
    int32_t k = (int32_t)floorf(z + s);
    float t = (i + j + k) * G3;
    float X0 = i - t; // Unskew the cell origin back to (x,y,z) space
    float Y0 = j - t;
    float Z0 = k - t;
    float x0 = x - X0; // The x,y,z distances from the cell origin
    float y0 = y - Y0;
    float z0 = z - Z0;

    // For the 3D case, the simplex shape is a slightly irregular tetrahedron.
    // Determine which simplex we are in.
    int32_t i1, j1, k1; // Offsets for second corner of simplex in (i,j,k) coords
    int32_t i2, j2, k2; // Offsets for third corner of simplex in (i,j,k) coords
    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 1; k2 = 0; // X Y Z order
        } else if (x0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0; i2 = 1; j2 = 0; k2 = 1; // X Z Y order
        } else {
            i1 = 0; j1 = 0; k1 = 1; i2 = 1; j2 = 0; k2 = 1; // Z X Y order
        }
    } else { // x0<y0
        if (y0 < z0) {
            i1 = 0; j1 = 0; k1 = 1; i2 = 0; j2 = 1; k2 = 1; // Z Y X order
        } else if (x0 < z0) {
            i1 = 0; j1 = 1; k1 = 0; i2 = 0; j2 = 1; k2 = 1; // Y Z X order
        } else {
            i1 = 0; j1 = 1; k1 = 0; i2 = 1; j2 = 1; k2 = 0; // Y X Z order
        }
    }

    // A step of (1,0,0) in (i,j,k) means a step of (1-c,-c,-c) in (x,y,z),
    // a step of (0,1,0) in (i,j,k) means a step of (-c,1-c,-c) in (x,y,z), and
    // a step of (0,0,1) in (i,j,k) means a step of (-c,-c,1-c) in (x,y,z), where
    // c = 1/6.
    float x1 = x0 - i1 + G3; // Offsets for second corner in (x,y,z) coords
    float y1 = y0 - j1 + G3;
    float z1 = z0 - k1 + G3;
    float x2 = x0 - i2 + 2.0f * G3; // Offsets for third corner in (x,y,z) coords
    float y2 = y0 - j2 + 2.0f * G3;
    float z2 = z0 - k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3; // Offsets for last corner in (x,y,z) coords
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    // Work out the hashed gradient indices of the four simplex corners
    int32_t gi0 = SimplexHash(i + SimplexHash(j + SimplexHash(k)));
    int32_t gi1 = SimplexHash(i + i1 + SimplexHash(j + j1 + SimplexHash(k + k1)));
    int32_t gi2 = SimplexHash(i + i2 + SimplexHash(j + j2 + SimplexHash(k + k2)));
    int32_t gi3 = SimplexHash(i + 1 + SimplexHash(j + 1 + SimplexHash(k + 1)));

    // Calculate the contribution from the four corners
    float t0 = 0.6f - x0*x0 - y0*y0 - z0*z0;
    if (t0 < 0) {
        n0 = 0.0;
    } else {
        t0 *= t0;
        n0 = t0 * t0 * SimplexGrad(gi0, x0, y0, z0);
    }
    float t1 = 0.6f - x1*x1 - y1*y1 - z1*z1;
    if (t1 < 0) {
        n1 = 0.0;
    } else {
        t1 *= t1;
        n1 = t1 * t1 * SimplexGrad(gi1, x1, y1, z1);
    }
    float t2 = 0.6f - x2*x2 - y2*y2 - z2*z2;
    if (t2 < 0) {
        n2 = 0.0;
    } else {
        t2 *= t2;
        n2 = t2 * t2 * SimplexGrad(gi2, x2, y2, z2);
    }
    float t3 = 0.6f - x3*x3 - y3*y3 - z3*z3;
    if (t3 < 0) {
        n3 = 0.0;
    } else {
        t3 *= t3;
        n3 = t3 * t3 * SimplexGrad(gi3, x3, y3, z3);
    }
    // Add contributions from each corner to get the final noise value.
    // The result is scaled to stay just inside [-1,1]
    return 32.0f*(n0 + n1 + n2 + n3);
}

/**
 * Fractal/Fractional Brownian Motion (fBm) summation of 3D Perlin Simplex noise
 *
 * @param[in] octaves   number of fraction of noise to sum
 * @param[in] x         x float coordinate
 * @param[in] y         y float coordinate
 * @param[in] z         z float coordinate
 *
 * @return Noise value in the range[-1; 1], value of 0 on all integer coordinates.
 */
float SimplexNoiseFBM(size_t octaves, float frequency, float amplitude, float lacunarity, float persistence, float x, float y, float z) {
    float output = 0.f;
    float denom  = 0.f;

    for (size_t i = 0; i < octaves; i++) {
        output += (amplitude * SimplexNoise(x * frequency, y * frequency, z * frequency));
        denom += amplitude;

        frequency *= lacunarity;
        amplitude *= persistence;
    }

    return (output / denom);
}

/**
 * @brief
 */
static void FogLuxel(GArray *fogs, luxel_t *l, float scale) {

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

		float noise = SimplexNoiseFBM(fog->octaves, fog->frequency, fog->amplitude, fog->lacunarity,fog->persistence,
			l->s / (float)MAX_BSP_LIGHTGRID_WIDTH, l->t / (float)MAX_BSP_LIGHTGRID_WIDTH, l->u / (float)MAX_BSP_LIGHTGRID_WIDTH);

		intensity *= fog->density + (fog->noise * noise);

		intensity = Clampf(intensity, 0.f, 1.f);

		if (intensity == 0.f) {
			continue;
		}

		const float diffuse = Clampf(Vec3_Length(l->diffuse) / DEFAULT_LIGHT, 0.f, 1.f);

		const float absorption = Clampf(diffuse * fog->absorption, 0.f, 1.f);

		const vec3_t color = Vec3_Mix(fog->color, l->diffuse, absorption);

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
	const size_t num_offsets = antialias ? lengthof(offsets) : 1;

	luxel_t *l = &lg.luxels[luxel_num];

	for (size_t i = 0; i < num_offsets; i++) {

		const float soffs = offsets[i].x;
		const float toffs = offsets[i].y;
		const float uoffs = offsets[i].z;

		if (ProjectLightgridLuxel(l, soffs, toffs, uoffs) == CONTENTS_SOLID) {
			continue;
		}

		FogLuxel(fogs, l, 1.f);

		l->fog = Vec3_ToVec4(ColorFilter(Vec4_XYZ(l->fog)), Clampf(l->fog.w, 0.f, 1.f));
	}
}

/**
 * @brief
 */
void FinalizeLightgrid(int32_t luxel_num) {

	luxel_t *l = &lg.luxels[luxel_num];

	for (int32_t i = 0; i < num_bounces; i++) {
		l->ambient = Vec3_Add(l->ambient, l->radiosity[i]);
	}

	l->ambient = Vec3_Scale(l->ambient, 1.f / 255.f);
	l->ambient = ColorFilter(l->ambient);

	l->diffuse = Vec3_Scale(l->diffuse, 1.f / 255.f);
	l->diffuse = ColorFilter(l->diffuse);

	l->direction = Vec3_Add(l->direction, Vec3_Up());
	l->direction = Vec3_Normalize(l->direction);
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

	const size_t lightgrid_bytes = lg.num_luxels * BSP_LIGHTGRID_TEXTURES * BSP_LIGHTGRID_BPP;
	const size_t fog_bytes = lg.num_luxels * BSP_FOG_TEXTURES * BSP_FOG_BPP;

	bsp_file.lightgrid_size = (int32_t) (sizeof(bsp_lightgrid_t) + lightgrid_bytes + fog_bytes);

	Bsp_AllocLump(&bsp_file, BSP_LUMP_LIGHTGRID, bsp_file.lightgrid_size);

	bsp_file.lightgrid->size = lg.size;

	byte *out = (byte *) bsp_file.lightgrid + sizeof(bsp_lightgrid_t);

	byte *out_ambient = out + 0 * lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_diffuse = out + 1 * lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_direction = out + 2 * lg.num_luxels * BSP_LIGHTGRID_BPP;
	byte *out_fog = out + 3 * lg.num_luxels * BSP_LIGHTGRID_BPP;

	const luxel_t *l = lg.luxels;
	for (int32_t u = 0; u < lg.size.z; u++) {

		SDL_Surface *ambient = CreateLightgridSurfaceFrom(out_ambient, lg.size.x, lg.size.y);
		SDL_Surface *diffuse = CreateLightgridSurfaceFrom(out_diffuse, lg.size.x, lg.size.y);
		SDL_Surface *direction = CreateLightgridSurfaceFrom(out_direction, lg.size.x, lg.size.y);
		SDL_Surface *fog = CreateFogSurfaceFrom(out_fog, lg.size.x, lg.size.y);

		for (int32_t t = 0; t < lg.size.y; t++) {
			for (int32_t s = 0; s < lg.size.x; s++, l++) {

				for (int32_t i = 0; i < BSP_LIGHTGRID_BPP; i++) {
					*out_ambient++ = (byte) Clampf(l->ambient.xyz[i] * 255.f, 0.f, 255.f);
					*out_diffuse++ = (byte) Clampf(l->diffuse.xyz[i] * 255.f, 0.f, 255.f);
					*out_direction++ = (byte) Clampf((l->direction.xyz[i] + 1.f) * 0.5f * 255.f, 0.f, 255.f);
				}

				for (int32_t i = 0; i < BSP_FOG_BPP; i++) {
					*out_fog++ = (byte) Clampf(l->fog.xyzw[i] * 255.f, 0.f, 255.f);
				}
			}
		}

		//IMG_SavePNG(ambient, va("/tmp/%s_lg_ambient_%d.png", map_base, u));
		//IMG_SavePNG(diffuse, va("/tmp/%s_lg_diffuse_%d.png", map_base, u));
		//IMG_SavePNG(direction, va("/tmp/%s_lg_direction_%d.png", map_base, u));
		//IMG_SavePNG(fog, va("/tmp/%s_lg_fog_%d.png", map_base, u));

		SDL_FreeSurface(ambient);
		SDL_FreeSurface(diffuse);
		SDL_FreeSurface(direction);
		SDL_FreeSurface(fog);
	}
}
