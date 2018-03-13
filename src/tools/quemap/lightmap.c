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

#include "qlight.h"

static vec_t lightmap_scale;

// light_info_t is a temporary bucket for lighting calculations
typedef struct light_info_s {
	const bsp_face_t *face;
	const bsp_plane_t *plane;
	const bsp_texinfo_t *texinfo;

	vec3_t normal; // the plane normal, negated if back facing plane
	vec_t dist; // the plane distance, negated if back facing plane

	int32_t num_sample_points;
	vec_t *sample_points; // num_sample_points * num_samples

	vec3_t model_org; // for bsp submodels

	vec3_t tex_org;
	vec3_t tex_to_world[2]; // world = tex_org + s * tex_to_world[0]

	vec2_t exact_mins, exact_maxs;

	int32_t tex_mins[2], tex_size[2];

} light_info_t;

// face extents
typedef struct face_extents_s {
	vec3_t mins, maxs;
	vec3_t center;
	vec2_t st_mins, st_maxs;
} face_extents_t;

static face_extents_t face_extents[MAX_BSP_FACES];

/**
 * @brief Populates face_extents for all d_bsp_face_t, prior to light creation.
 * This is done so that sample positions may be nudged outward along
 * the face normal and towards the face center to help with traces.
 */
void BuildFaceExtents(void) {
	const bsp_vertex_t *v;
	int32_t i, j, k;

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

/**
 * @brief Fills in l->tex_mins[] and l->tex_size[], l->exact_mins[] and l->exact_maxs[]
 */
static void CalcLightInfoExtents(light_info_t *l) {
	vec2_t lm_mins, lm_maxs;

	const vec_t *st_mins = face_extents[l->face - bsp_file.faces].st_mins;
	const vec_t *st_maxs = face_extents[l->face - bsp_file.faces].st_maxs;

	for (int32_t i = 0; i < 2; i++) {
		l->exact_mins[i] = st_mins[i];
		l->exact_maxs[i] = st_maxs[i];

		lm_mins[i] = floor(st_mins[i] * lightmap_scale);
		lm_maxs[i] = ceil(st_maxs[i] * lightmap_scale);

		l->tex_mins[i] = lm_mins[i];
		l->tex_size[i] = lm_maxs[i] - lm_mins[i];
	}

	// if a surface lightmap is too large to fit in a single lightmap block,
	// we must fail here -- practically speaking, this is very unlikely
	if (l->tex_size[0] * l->tex_size[1] > MAX_BSP_LIGHTMAP) {
		const winding_t *w = WindingForFace(l->face);

		Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points,
		                va("Surface too large to light (%dx%d)\n", l->tex_size[0], l->tex_size[1]));
	}
}

/**
 * @brief Fills in tex_org, world_to_tex. and tex_to_world
 */
static void CalcLightInfoVectors(light_info_t *l) {

	const bsp_texinfo_t *tex = l->texinfo;

	// calculate a normal to the texture axis. points can be moved along this
	// without changing their S/T
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
		l->tex_org[i] = -tex->vecs[0][3] * l->tex_to_world[0][i] - tex->vecs[1][3]
		                * l->tex_to_world[1][i];
	}

	// project back to the face plane
	vec_t dist = DotProduct(l->tex_org, l->normal) - l->dist - 1.0;
	dist *= dist_scale;

	VectorMA(l->tex_org, -dist, tex_normal, l->tex_org);

	// compensate for org'd bmodels
	VectorAdd(l->tex_org, l->model_org, l->tex_org);

	// total sample count
	l->num_sample_points = (l->tex_size[0] + 1) * (l->tex_size[1] + 1);
}

/**
 * @brief For each texture aligned grid point, back project onto the plane
 * to get the world xyz value of the sample point
 */
static void CalcSamplePoints(const light_info_t *l, vec_t sofs, vec_t tofs, vec_t *out) {

	const int32_t h = l->tex_size[1] + 1;
	const int32_t w = l->tex_size[0] + 1;

	const int32_t step = 1.0 / lightmap_scale;
	const vec_t starts = l->tex_mins[0] * step;
	const vec_t startt = l->tex_mins[1] * step;

	for (int32_t t = 0; t < h; t++) {
		for (int32_t s = 0; s < w; s++, out += 3) {
			const vec_t us = starts + (s + sofs) * step;
			const vec_t ut = startt + (t + tofs) * step;

			// calculate texture point
			for (int32_t j = 0; j < 3; j++) {
				out[j] = l->tex_org[j] + l->tex_to_world[0][j] * us + l->tex_to_world[1][j] * ut;
			}
		}
	}
}

/**
 * @brief Converts world space direction into tangent space for given TBN
 */
static void WorldSpaceToTangentSpace(const vec3_t tangent, const vec3_t bitangent, const vec3_t normal, const vec3_t direction, vec3_t result) {
	// temporary storage in case if direction and result are the same pointer
	vec3_t temporary_result;

	// transform it into tangent space
	temporary_result[0] = DotProduct(direction, tangent);
	temporary_result[1] = DotProduct(direction, bitangent);
	temporary_result[2] = DotProduct(direction, normal);

	VectorCopy(temporary_result, result);
}

/**
 * @brief Light sample accumulation for each face.
 */
typedef struct {
	int32_t num_samples;
	vec_t *origins;
	vec_t *direct;
	vec_t *directions;
	vec_t *indirect;
} face_lighting_t;

static face_lighting_t face_lighting[MAX_BSP_FACES];


typedef struct light_s { // a light source
	struct light_s *next;
	light_type_t type;

	vec_t radius;
	vec3_t origin;
	vec3_t color;
	vec3_t normal; // spotlight direction
	vec_t cone; // spotlight cone, in radians
} light_t;

static light_t *lights[MAX_BSP_LEAFS];
static int32_t num_lights;

// sunlight, borrowed from ufo2map
typedef struct {
	vec_t light;
	vec3_t color;
	vec3_t angles; // entity-style angles
	vec3_t dir; // normalized direction
} sun_t;

static sun_t sun;

/**
 * @brief
 */
static entity_t *FindTargetEntity(const char *target) {
	int32_t i;

	for (i = 0; i < num_entities; i++) {
		const char *n = ValueForKey(&entities[i], "targetname");
		if (!g_strcmp0(n, target)) {
			return &entities[i];
		}
	}

	return NULL;
}

#define ANGLE_UP	-1.0
#define ANGLE_DOWN	-2.0

/**
 * @brief
 */
void BuildLights(void) {

	// surfaces
	for (size_t i = 0; i < lengthof(face_patches); i++) {
		
		const patch_t *p = face_patches[i];
		while (p) { // iterate patches

			if (p->light == 0.0) {
				continue;
			}

			num_lights++;
			light_t *l = Mem_TagMalloc(sizeof(*l), MEM_TAG_LIGHT);

			VectorCopy(p->origin, l->origin);

			const bsp_leaf_t *leaf = &bsp_file.leafs[Light_PointLeafnum(l->origin)];
			const int16_t cluster = leaf->cluster;
			l->next = lights[cluster];
			lights[cluster] = l;

			l->type = LIGHT_FACE;

			l->radius = p->light * surface_scale;
			ColorNormalize(p->color, l->color);

			p = p->next;
		}
	}

	// entities
	for (size_t i = 1; i < num_entities; i++) {
		const entity_t *e = &entities[i];

		const char *name = ValueForKey(e, "classname");
		if (strncmp(name, "light", 5)) { // not a light
			continue;
		}

		num_lights++;
		light_t *l = Mem_TagMalloc(sizeof(*l), MEM_TAG_LIGHT);

		VectorForKey(e, "origin", l->origin);

		const bsp_leaf_t *leaf = &bsp_file.leafs[Light_PointLeafnum(l->origin)];
		const int16_t cluster = leaf->cluster;

		l->next = lights[cluster];
		lights[cluster] = l;

		vec_t radius = FloatForKey(e, "light");
		if (!radius) {
			radius = FloatForKey(e, "_light");
		}
		if (!radius) {
			radius = DEFAULT_LIGHT;
		}

		const char *color = ValueForKey(e, "_color");
		if (color && color[0]) {
			sscanf(color, "%f %f %f", &l->color[0], &l->color[1], &l->color[2]);
			ColorNormalize(l->color, l->color);
		} else {
			VectorSet(l->color, 1.0, 1.0, 1.0);
		}

		l->radius = radius * light_scale;
		l->type = LIGHT_POINT;

		const char *target = ValueForKey(e, "target");
		if (!g_strcmp0(name, "light_spot") || target[0]) {

			l->type = LIGHT_SPOT;

			l->cone = FloatForKey(e, "cone");
			if (!l->cone) {
				l->cone = FloatForKey(e, "_cone");
			}
			if (!l->cone) { // reasonable default cone
				l->cone = 20.0;
			}

			l->cone = cos(Radians(l->cone));

			if (target[0]) { // point towards target
				entity_t *e2 = FindTargetEntity(target);
				if (!e2) {
					Mon_SendSelect(MON_WARN, i, 0, va("Light at %s missing target", vtos(l->origin)));
					VectorCopy(vec3_down, l->normal);
				} else {
					vec3_t org;
					VectorForKey(e2, "origin", org);
					VectorSubtract(org, l->origin, l->normal);
					VectorNormalize(l->normal);
				}
			} else { // point down angle
				const vec_t angle = FloatForKey(e, "angle");
				if (angle == ANGLE_UP) {
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = 1.0;
				} else if (angle == ANGLE_DOWN) {
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = -1.0;
				} else { // general case
					l->normal[2] = 0;
					l->normal[0] = cos(Radians(angle));
					l->normal[1] = sin(Radians(angle));
				}
			}
		}
	}

	Com_Verbose("Lighting %i lights\n", num_lights);

	{
		// sun.light parameters come from worldspawn
		const entity_t *e = &entities[0];

		sun.light = FloatForKey(e, "sun_light");

		VectorSet(sun.color, 1.0, 1.0, 1.0);
		const char *color = ValueForKey(e, "sun_color");
		if (color && color[0]) {
			sscanf(color, "%f %f %f", &sun.color[0], &sun.color[1], &sun.color[2]);
			ColorNormalize(sun.color, sun.color);
		}

		const char *angles = ValueForKey(e, "sun_angles");

		VectorClear(sun.angles);
		sscanf(angles, "%f %f", &sun.angles[0], &sun.angles[1]);

		AngleVectors(sun.angles, sun.dir, NULL, NULL);

		if (sun.light) {
			Com_Verbose("Sun defined with light %3.0f, color %0.2f %0.2f %0.2f, "
			            "angles %1.3f %1.3f %1.3f\n", sun.light, sun.color[0], sun.color[1],
			            sun.color[2], sun.angles[0], sun.angles[1], sun.angles[2]);
		}

		// ambient light, also from worldspawn
		const char *ambient_ = ValueForKey(e, "ambient");
		sscanf(ambient_, "%f %f %f", &ambient[0], &ambient[1], &ambient[2]);

		if (VectorLength(ambient)) {
			Com_Verbose("Ambient lighting defined with color %0.2f %0.2f %0.2f\n", ambient[0],
						ambient[1], ambient[2]);
		}

		// optionally pull brightness from worldspawn
		vec_t v = FloatForKey(e, "brightness");
		if (v > 0.0) {
			brightness = v;
		}

		// saturation as well
		v = FloatForKey(e, "saturation");
		if (v > 0.0) {
			saturation = v;
		}

		v = FloatForKey(e, "contrast");
		if (v > 0.0) {
			contrast = v;
		}

		v = FloatForKey(e, "light_scale");
		if (v > 0.0) {
			light_scale = v;
		}

		v = FloatForKey(e, "surface_scale");
		if (v > 0.0) {
			surface_scale = v;
		}

		// lightmap resolution downscale (e.g. 0.125, 0.0625)
		lightmap_scale = FloatForKey(e, "lightmap_scale");
		if (lightmap_scale == 0.0) {
			lightmap_scale = BSP_DEFAULT_LIGHTMAP_SCALE;
		}
	}
}

/**
 * @brief A follow-up to GatherSampleLight, simply trace along the sun normal, adding
 * sunlight when a sky surface is struck.
 */
static void GatherSampleSunlight(const vec3_t pos, const vec3_t tangent, const vec3_t bitangent, const vec3_t normal, vec_t *sample,
                                 vec_t *direction, vec_t scale) {

	if (!sun.light) {
		return;
	}

	const vec_t dot = DotProduct(sun.dir, normal);

	if (dot <= 0.001) {
		return; // wrong direction
	}

	vec3_t delta;
	VectorMA(pos, MAX_WORLD_DIST, sun.dir, delta);

	cm_trace_t trace;
	Light_Trace(&trace, pos, delta, CONTENTS_SOLID);

	if (trace.fraction < 1.0 && !(trace.surface->flags & SURF_SKY)) {
		return; // occluded
	}

	const vec_t light = sun.light * dot;

	// add some light to it
	VectorMA(sample, light * scale, sun.color, sample);

	// and accumulate the direction
	VectorMix(normal, sun.dir, light / sun.light, delta);

	WorldSpaceToTangentSpace(tangent, bitangent, normal, delta, delta);
	VectorMA(direction, light * scale, delta, direction);	
}

/**
 * @brief Iterate over all light sources for the sample position's PVS, accumulating
 * light and directional information to the specified pointers.
 */
static void GatherSampleLight(vec3_t pos, vec3_t tangent, vec3_t bitangent, vec3_t normal,  byte *pvs, vec_t *sample, vec_t *direction, vec_t scale) {

	// iterate over lights, which are in buckets by cluster
	for (int32_t i = 0; i < bsp_file.vis_data.vis->num_clusters; i++) {

		if (!(pvs[i >> 3] & (1 << (i & 7)))) {
			continue;
		}

		for (light_t *l = lights[i]; l; l = l->next) {

			vec3_t delta;
			VectorSubtract(l->origin, pos, delta);

			const vec_t dist = VectorNormalize(delta);
			if (dist > l->radius) {
				continue;
			}

			const vec_t dot = DotProduct(delta, normal);
			if (dot < SIDE_EPSILON) {
				continue;
			}

			vec_t light = 0.0;

			switch (l->type) {
				case LIGHT_POINT: // linear falloff
				case LIGHT_FACE:
					light = (l->radius - dist) * dot;
					break;

				case LIGHT_SPOT: { // linear falloff with cone
					const vec_t dot2 = -DotProduct(delta, l->normal);
					if (dot2 > l->cone) { // inside the cone
						light = (l->radius - dist) * dot;
					} else { // outside the cone
						const vec_t decay = 1.0 + l->cone - dot2;
						light = (l->radius - decay * decay * dist) * dot;
					}
				}
					break;
				default:
					Mon_SendPoint(MON_WARN, l->origin, "Light with bad type");
					break;
			}

			if (light <= 0.0) { // no light
				continue;
			}

			cm_trace_t trace;
			Light_Trace(&trace, l->origin, pos, CONTENTS_SOLID);

			if (trace.fraction < 1.0) {
				continue; // occluded
			}

			// add some light to it
			VectorMA(sample, light * scale, l->color, sample);

			// and add some direction
			VectorMix(normal, delta, 2.0 * light / l->radius, delta);

			WorldSpaceToTangentSpace(tangent, bitangent, normal, delta, delta);

			VectorMA(direction, light * scale, delta, direction);
		}
	}

	GatherSampleSunlight(pos, tangent, bitangent, normal, sample, direction, scale);
}

#define SAMPLE_NUDGE 0.25

/**
 * @brief Move the incoming sample position towards the surface center and along the
 * surface normal to reduce false-positive traces. Test the PVS at the new
 * position, returning true if the new point is valid, false otherwise.
 */
static _Bool NudgeSamplePosition(const vec3_t in, const vec3_t normal, const vec3_t center,
                                 vec3_t out, byte *pvs) {
	vec3_t dir;

	VectorCopy(in, out);

	// move into the level using the normal and surface center
	VectorSubtract(out, center, dir);
	VectorNormalize(dir);

	VectorMA(out, SAMPLE_NUDGE, dir, out);
	VectorMA(out, SAMPLE_NUDGE, normal, out);

	return Light_PointPVS(out, pvs);
}


#define MAX_VERT_FACES 256

/**
 * @brief Populate faces with indexes of all Phong-shaded d_bsp_face_t's
 * referencing the specified vertex. The number of d_bsp_face_t's referencing
 * the vertex is returned in nfaces.
 */
static void FacesWithVert(int32_t vert, int32_t *faces, int32_t *nfaces) {
	int32_t i, j, k;

	k = 0;
	for (i = 0; i < bsp_file.num_faces; i++) {
		const bsp_face_t *face = &bsp_file.faces[i];

		if (!(bsp_file.texinfo[face->texinfo].flags & SURF_PHONG)) {
			continue;
		}

		for (j = 0; j < face->num_edges; j++) {

			const int32_t e = bsp_file.face_edges[face->first_edge + j];
			const int32_t v = e >= 0 ? bsp_file.edges[e].v[0] : bsp_file.edges[-e].v[1];

			if (v == vert) { // face references vert
				faces[k++] = i;
				if (k == MAX_VERT_FACES) {
					Mon_SendPoint(MON_ERROR, bsp_file.vertexes[v].point, "MAX_VERT_FACES");
				}
				break;
			}
		}
	}
	*nfaces = k;
}

/**
 * @brief Calculate per-vertex (instead of per-plane) normal vectors. This is done by
 * finding all of the faces which share a given vertex, and calculating a weighted
 * average of their normals.
 */
void BuildVertexNormals(void) {
	int32_t vert_faces[MAX_VERT_FACES];
	int32_t num_vert_faces;
	vec3_t norm, delta;
	int32_t i, j;

	for (i = 0; i < bsp_file.num_vertexes; i++) {

		VectorClear(bsp_file.normals[i].normal);

		FacesWithVert(i, vert_faces, &num_vert_faces);

		if (!num_vert_faces) { // rely on plane normal only
			continue;
		}

		for (j = 0; j < num_vert_faces; j++) {

			const bsp_face_t *face = &bsp_file.faces[vert_faces[j]];
			const bsp_plane_t *plane = &bsp_file.planes[face->plane_num];

			// scale the contribution of each face based on size
			const face_extents_t *extents = &face_extents[vert_faces[j]];
			VectorSubtract(extents->maxs, extents->mins, delta);

			const vec_t scale = VectorLength(delta);

			if (face->side) {
				VectorScale(plane->normal, -scale, norm);
			} else {
				VectorScale(plane->normal, scale, norm);
			}

			VectorAdd(bsp_file.normals[i].normal, norm, bsp_file.normals[i].normal);
		}

		VectorNormalize(bsp_file.normals[i].normal);
	}
}

/**
 * @brief For Phong-shaded samples, calculate the interpolated normal vector using
 * linear interpolation between the nearest and farthest vertexes.
 */
static void SampleNormal(const bsp_face_t *face, const vec3_t pos, vec3_t normal) {
	vec_t total_dist = 0.0;
	vec_t dist[MAX_VERT_FACES];

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

#define MAX_SAMPLES 9

static const vec_t sampleofs[MAX_SAMPLES][2] = {
	{ +0.0, +0.0 }, { -0.5, -0.5 }, { +0.0, -0.5 },
	{ +0.5, -0.5 },	{ -0.5, +0.0 }, { +0.5, +0.0 },
	{ -0.5, +0.5 }, { +0.0, +0.5 }, { +0.5, +0.5 }
};

static const vec_t samplewgh[MAX_SAMPLES] = {
	0.195346, 0.077847, 0.123317,
	0.077847, 0.123317, 0.123317,
	0.077847, 0.123317, 0.077847
};

/**
 * @brief
 */
void DirectLighting(int32_t face_num) {

	if (face_num >= MAX_BSP_FACES) {
		Com_Warn("MAX_BSP_FACES hit\n");
		return;
	}

	light_info_t light;
	memset(&light, 0, sizeof(light));

	light.face = &bsp_file.faces[face_num];
	light.plane = &bsp_file.planes[light.face->plane_num];
	light.texinfo = &bsp_file.texinfo[light.face->texinfo];

	if (light.texinfo->flags & (SURF_SKY | SURF_WARP)) {
		return;
	}

	// determine the correct normal and distance for back-facing planes
	if (light.face->side) {
		VectorNegate(light.plane->normal, light.normal);
		light.dist = -light.plane->dist;
	} else {
		VectorCopy(light.plane->normal, light.normal);
		light.dist = light.plane->dist;
	}

	// get the origin offset for rotating bmodels
	VectorCopy(face_offset[face_num], light.model_org);

	// calculate lightmap texture mins and maxs
	CalcLightInfoExtents(&light);

	// and the lightmap texture vectors
	CalcLightInfoVectors(&light);

	// determine the number of sample passes (-antialias)
	const int32_t num_samples = antialias ? MAX_SAMPLES : 1;

	// now allocate all of the sample points
	const size_t size = light.num_sample_points * num_samples * sizeof(vec3_t);
	light.sample_points = Mem_Malloc(size);

	for (int32_t i = 0; i < num_samples; i++) { // and calculate them in world space

		const vec_t sofs = sampleofs[i][0];
		const vec_t tofs = sampleofs[i][1];

		vec_t *out = light.sample_points + (i * light.num_sample_points * 3);

		CalcSamplePoints(&light, sofs, tofs, out);
	}

	face_lighting_t *fl = &face_lighting[face_num];
	fl->num_samples = light.num_sample_points;

	fl->origins = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	memcpy(fl->origins, light.sample_points, fl->num_samples * sizeof(vec3_t));

	fl->direct = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	fl->directions = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);
	fl->indirect = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACE_LIGHTING);

	memset(fl->direct, 0, fl->num_samples * sizeof(vec3_t));
	memset(fl->directions, 0, fl->num_samples * sizeof(vec3_t));
	memset(fl->indirect, 0, fl->num_samples * sizeof(vec3_t));

	const vec_t *center = face_extents[face_num].center; // center of the face

	for (int32_t i = 0; i < fl->num_samples; i++) { // calculate light for each sample

		vec_t *sample = fl->direct + i * 3; // accumulate lighting here
		vec_t *direction = fl->directions + i * 3; // accumulate direction here

		for (int32_t j = 0; j < num_samples; j++) { // with antialiasing

			byte pvs[(MAX_BSP_LEAFS + 7) / 8];
			vec3_t normal;

			const vec_t *point = light.sample_points + (j * light.num_sample_points + i) * 3;

			if (light.texinfo->flags & SURF_PHONG) { // interpolated normal
				SampleNormal(light.face, point, normal);
			} else { // or just plane normal
				VectorCopy(light.normal, normal);
			}

			vec3_t pos;

			if (!NudgeSamplePosition(point, normal, center, pos, pvs)) {
				continue; // not a valid point
			}

			vec4_t tangent;
			vec3_t bitangent;

			TangentVectors(normal, light.texinfo->vecs[0], light.texinfo->vecs[1], tangent, bitangent);

			// query all light sources within range for their contribution
			GatherSampleLight(pos, tangent, bitangent, normal, pvs, sample, direction, samplewgh[j]);
		}
	}

	// free the sample points
	Mem_Free(light.sample_points);
}

/**
* @brief
*/
void BuildIndirectLights(void) {
	int32_t i, face_num;

	num_lights = 0;
	memset(lights, 0, sizeof(lights));

	for (face_num = 0; face_num < bsp_file.num_faces; face_num++) {
		const bsp_face_t *face = &bsp_file.faces[face_num];
		const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];
		const bsp_plane_t *plane = &bsp_file.planes[face->plane_num];

		if (texinfo->flags & (SURF_SKY | SURF_WARP | SURF_LIGHT | SURF_HINT | SURF_NO_DRAW)) {
			continue; // we have no light to reflect
		}

		const face_lighting_t *fl = &face_lighting[face_num];

		vec3_t normal;

		if (face->side) {
			VectorNegate(plane->normal, normal);
		}
		else {
			VectorCopy(plane->normal, normal);
		}

		const vec_t *center = face_extents[face_num].center;

		for (i = 0; i < fl->num_samples; i++) {
			byte pvs[(MAX_BSP_LEAFS + 7) / 8];

			vec3_t direct, indirect, color;
			VectorCopy(fl->direct + i * 3, direct);
			VectorCopy(fl->indirect + i * 3, indirect);
			VectorAdd(direct, indirect, color);

			if (VectorCompare(color, vec3_origin)) {
				continue;
			}

			vec3_t point, position;

			VectorCopy(fl->origins + i * 3, point);

			if (texinfo->flags & SURF_PHONG) {
				SampleNormal(face, position, normal);
			}

			vec3_t offset;
			VectorScale(normal, 1.0, offset);

			VectorAdd(point, offset, point);

			if (!NudgeSamplePosition(point, normal, center, position, pvs)) {
				continue;
			}			

			num_lights++;
			light_t *light = Mem_TagMalloc(sizeof(*light), MEM_TAG_LIGHT);

			VectorCopy(position, light->origin);

			const bsp_leaf_t *leaf = &bsp_file.leafs[Light_PointLeafnum(light->origin)];
			const int16_t cluster = leaf->cluster;
			light->next = lights[cluster];
			lights[cluster] = light;

			light->type = LIGHT_SPOT;
			light->cone = 1.0;

			VectorCopy(normal, light->normal);

			VectorScale(color, 1.0 / 255.0 / 256.0 / (indirect_bounce + 1.0), light->color);
			light->radius = 256;
		}
	}
}

/**
 * @brief Calculates indirect lighting via photon bouncing.
 * @details Direct lighting results are reflected outwards. Hits on neighboring surfaces are
 * traced to their lightmap sample, much like stain mapping.
 * @remarks TODO: Ensure this is thread safe for impacted faces.
 */
void IndirectLighting(int32_t face_num) {

	const bsp_face_t *face = &bsp_file.faces[face_num];
	const bsp_texinfo_t *texinfo = &bsp_file.texinfo[face->texinfo];

	if (texinfo->flags & (SURF_SKY | SURF_WARP | SURF_HINT | SURF_NO_DRAW)) {
		return; // we have no light to reflect
	}

	const bsp_plane_t *plane = &bsp_file.planes[face->plane_num];

	vec3_t normal;

	if (face->side) {
		VectorNegate(plane->normal, normal);
	} else {
		VectorCopy(plane->normal, normal);
	}
	
	const vec_t *center = face_extents[face_num].center; // center of the face

	face_lighting_t *fl = &face_lighting[face_num];

	for (int32_t i = 0; i < fl->num_samples; i++) { // calculate light for each sample

		vec_t *sample = fl->indirect + i * 3; // accumulate lighting here
		vec_t *direction = fl->directions + i * 3; // accumulate direction here

		byte pvs[(MAX_BSP_LEAFS + 7) / 8];

		const vec_t *point = fl->origins + i * 3;

		if (texinfo->flags & SURF_PHONG) { // interpolated normal
			SampleNormal(face, point, normal);
		}

		vec3_t position;

		if (!NudgeSamplePosition(point, normal, center, position, pvs)) {
			continue; // not a valid point
		}

		vec4_t tangent;
		vec3_t bitangent;

		TangentVectors(normal, texinfo->vecs[0], texinfo->vecs[1], tangent, bitangent);

		// query all light sources within range for their contribution
		GatherSampleLight(position, tangent, bitangent, normal, pvs, sample, direction, 1.0);
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
	bsp_file.lightmap_data_size += fl->num_samples * lightmap_color_channels;

	if (!legacy) { // account for light direction data as well
		bsp_file.lightmap_data_size += fl->num_samples * lightmap_color_channels;
	}

	if (bsp_file.lightmap_data_size > MAX_BSP_LIGHTING) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTING\n");
	}

	ThreadUnlock();

	// write it out
	byte *dest = &bsp_file.lightmap_data[f->light_ofs];

	for (int32_t i = 0; i < fl->num_samples; i++) {
		vec3_t lightmap;
		vec4_t hdr_lightmap;

		const vec_t *direct = fl->direct + i * 3;
		const vec_t *indirect = fl->indirect + i * 3;

		// start with raw sample data
		VectorAdd(direct, indirect, lightmap);

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
				VectorCopy(direction, deluxemap);
			} else {
				VectorCopy(vec3_up, deluxemap);
			}

			VectorNormalize(deluxemap);

			static const vec3_t vec3_ones = { 1.0, 1.0, 1.0 };

			VectorAdd(deluxemap, vec3_ones, deluxemap);
			VectorScale(deluxemap, 0.5, deluxemap);

			ColorEncodeRGBM(deluxemap, hdr_deluxemap);

			for (int32_t j = 0; j < 4; j++) {
				*dest++ = (byte) Clamp(floor(hdr_deluxemap[j] * 255.0 + 0.5), 0, 255);
			}
		}
	}
}
