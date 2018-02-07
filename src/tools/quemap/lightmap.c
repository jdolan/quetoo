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
	vec_t face_dist;
	vec3_t face_normal;

	int32_t num_sample_points;
	vec3_t *sample_points;

	vec3_t model_org; // for bsp submodels

	vec3_t tex_org;
	vec3_t world_to_tex[2]; // s = (world - tex_org) . world_to_tex[0]
	vec3_t tex_to_world[2]; // world = tex_org + s * tex_to_world[0]

	vec2_t exact_mins, exact_maxs;

	int32_t tex_mins[2], tex_size[2];
	bsp_face_t *face;
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
 * @brief Fills in l->texmins[] and l->texsize[], l->exactmins[] and l->exactmaxs[]
 */
static void CalcLightinfoExtents(light_info_t *l) {
	const bsp_face_t *s;
	vec_t *st_mins, *st_maxs;
	vec2_t lm_mins, lm_maxs;
	int32_t i;

	s = l->face;

	st_mins = face_extents[s - bsp_file.faces].st_mins;
	st_maxs = face_extents[s - bsp_file.faces].st_maxs;

	for (i = 0; i < 2; i++) {
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
		const winding_t *w = WindingForFace(s);

		Mon_SendWinding(MON_ERROR, (const vec3_t *) w->points, w->num_points,
		                va("Surface too large to light (%dx%d)\n", l->tex_size[0], l->tex_size[1]));
	}
}

/**
 * @brief Fills in tex_org, world_to_tex. and tex_to_world
 */
static void CalcLightinfoVectors(light_info_t *l) {
	const bsp_texinfo_t *tex;
	int32_t i;
	vec3_t tex_normal;
	vec_t dist_scale;
	vec_t dist;

	tex = &bsp_file.texinfo[l->face->texinfo];

	// convert from vec_t to double
	for (i = 0; i < 2; i++) {
		VectorCopy(tex->vecs[i], l->world_to_tex[i]);
	}

	// calculate a normal to the texture axis. points can be moved along this
	// without changing their S/T
	tex_normal[0] = tex->vecs[1][1] * tex->vecs[0][2] - tex->vecs[1][2] * tex->vecs[0][1];
	tex_normal[1] = tex->vecs[1][2] * tex->vecs[0][0] - tex->vecs[1][0] * tex->vecs[0][2];
	tex_normal[2] = tex->vecs[1][0] * tex->vecs[0][1] - tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize(tex_normal);

	// flip it towards plane normal
	dist_scale = DotProduct(tex_normal, l->face_normal);
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

	for (i = 0; i < 2; i++) {
		const vec_t len = VectorLength(l->world_to_tex[i]);
		const vec_t distance = DotProduct(l->world_to_tex[i], l->face_normal) * dist_scale;
		VectorMA(l->world_to_tex[i], -distance, tex_normal, l->tex_to_world[i]);
		VectorScale(l->tex_to_world[i], (1 / len) * (1 / len), l->tex_to_world[i]);
	}

	// calculate tex_org on the texture plane
	for (i = 0; i < 3; i++)
		l->tex_org[i] = -tex->vecs[0][3] * l->tex_to_world[0][i] - tex->vecs[1][3]
		                * l->tex_to_world[1][i];

	// project back to the face plane
	dist = DotProduct(l->tex_org, l->face_normal) - l->face_dist - 1;
	dist *= dist_scale;
	VectorMA(l->tex_org, -dist, tex_normal, l->tex_org);

	// compensate for org'd bmodels
	VectorAdd(l->tex_org, l->model_org, l->tex_org);

	// total sample count
	l->num_sample_points = (l->tex_size[0] + 1) * (l->tex_size[1] + 1);
	l->sample_points = Mem_TagMalloc(l->num_sample_points * sizeof(vec3_t), MEM_TAG_SAMPLES);
}

/**
 * @brief For each texture aligned grid point, back project onto the plane
 * to get the world xyz value of the sample point
 */
static void CalcPoints(light_info_t *l, vec_t sofs, vec_t tofs) {
	int32_t s, t, j;
	int32_t w, h, step;
	vec_t starts, startt;
	vec_t *surf;

	surf = l->sample_points[0];

	h = l->tex_size[1] + 1;
	w = l->tex_size[0] + 1;

	step = 1.0 / lightmap_scale;
	starts = l->tex_mins[0] * step;
	startt = l->tex_mins[1] * step;

	for (t = 0; t < h; t++) {
		for (s = 0; s < w; s++, surf += 3) {
			const vec_t us = starts + (s + sofs) * step;
			const vec_t ut = startt + (t + tofs) * step;

			// calculate texture point
			for (j = 0; j < 3; j++) {
				surf[j] = l->tex_org[j] + l->tex_to_world[0][j] * us + l->tex_to_world[1][j] * ut;
			}
		}
	}
}

typedef struct { // buckets for sample accumulation
	int32_t num_samples;
	vec_t *origins;
	vec_t *samples;
	vec_t *directions;
} face_light_t;

static face_light_t face_lights[MAX_BSP_FACES];

typedef struct light_s { // a light source
	struct light_s *next;
	light_type_t type;

	vec_t intensity; // brightness
	vec3_t origin;
	vec3_t color;
	vec3_t normal; // spotlight direction
	vec_t stopdot; // spotlight cone
} light_t;

static light_t *lights[MAX_BSP_LEAFS];
static int32_t num_lights;

// sunlight, borrowed from ufo2map
typedef struct sun_s {
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
	size_t i;
	light_t *l;
	const bsp_leaf_t *leaf;
	int32_t cluster;
	const char *target;
	vec3_t dest;
	const char *color;
	const char *angles;
	vec_t v, intensity;

	// surfaces
	for (i = 0; i < lengthof(face_patches); i++) {
		const patch_t *p = face_patches[i];

		while (p) { // iterate subdivided patches

			if (VectorCompare(p->light, vec3_origin)) {
				continue;
			}

			num_lights++;
			l = Mem_TagMalloc(sizeof(*l), MEM_TAG_LIGHT);

			VectorCopy(p->origin, l->origin);

			leaf = &bsp_file.leafs[Light_PointLeafnum(l->origin)];
			cluster = leaf->cluster;
			l->next = lights[cluster];
			lights[cluster] = l;

			l->type = LIGHT_FACE;

			l->intensity = ColorNormalize(p->light, l->color);
			l->intensity *= p->area * surface_scale;

			p = p->next;
		}
	}

	// entities
	for (i = 1; i < num_entities; i++) {
		const entity_t *e = &entities[i];

		const char *name = ValueForKey(e, "classname");
		if (strncmp(name, "light", 5)) { // not a light
			continue;
		}

		num_lights++;
		l = Mem_TagMalloc(sizeof(*l), MEM_TAG_LIGHT);

		VectorForKey(e, "origin", l->origin);

		leaf = &bsp_file.leafs[Light_PointLeafnum(l->origin)];
		cluster = leaf->cluster;

		l->next = lights[cluster];
		lights[cluster] = l;

		intensity = FloatForKey(e, "light");
		if (!intensity) {
			intensity = FloatForKey(e, "_light");
		}
		if (!intensity) {
			intensity = 300.0;
		}

		color = ValueForKey(e, "_color");
		if (color && color[0]) {
			sscanf(color, "%f %f %f", &l->color[0], &l->color[1], &l->color[2]);
			ColorNormalize(l->color, l->color);
		} else {
			VectorSet(l->color, 1.0, 1.0, 1.0);
		}

		l->intensity = intensity * entity_scale;
		l->type = LIGHT_POINT;

		target = ValueForKey(e, "target");
		if (!g_strcmp0(name, "light_spot") || target[0]) {

			l->type = LIGHT_SPOT;

			l->stopdot = FloatForKey(e, "cone");
			if (!l->stopdot) {
				l->stopdot = FloatForKey(e, "_cone");
			}
			if (!l->stopdot) { // reasonable default cone
				l->stopdot = 20.0;
			}

			l->stopdot = cos(Radians(l->stopdot));

			if (target[0]) { // point towards target
				entity_t *e2 = FindTargetEntity(target);
				if (!e2) {
					Mon_SendSelect(MON_WARN, i, 0, va("Light at %s missing target", vtos(l->origin)));
				} else {
					VectorForKey(e2, "origin", dest);
					VectorSubtract(dest, l->origin, l->normal);
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
		color = ValueForKey(e, "sun_color");
		if (color && color[0]) {
			sscanf(color, "%f %f %f", &sun.color[0], &sun.color[1], &sun.color[2]);
			ColorNormalize(sun.color, sun.color);
		}

		angles = ValueForKey(e, "sun_angles");

		VectorClear(sun.angles);
		sscanf(angles, "%f %f", &sun.angles[0], &sun.angles[1]);

		AngleVectors(sun.angles, sun.dir, NULL, NULL);

		if (sun.light)
			Com_Verbose("Sun defined with light %3.0f, color %0.2f %0.2f %0.2f, "
			            "angles %1.3f %1.3f %1.3f\n", sun.light, sun.color[0], sun.color[1],
			            sun.color[2], sun.angles[0], sun.angles[1], sun.angles[2]);

		// ambient light, also from worldspawn
		color = ValueForKey(e, "ambient_light");
		sscanf(color, "%f %f %f", &ambient[0], &ambient[1], &ambient[2]);

		if (VectorLength(ambient))
			Com_Verbose("Ambient lighting defined with color %0.2f %0.2f %0.2f\n", ambient[0],
			            ambient[1], ambient[2]);

		// optionally pull brightness from worldspawn
		v = FloatForKey(e, "brightness");
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
static void GatherSampleSunlight(const vec3_t pos, const vec3_t normal, vec_t *sample,
                                 vec_t *direction, vec_t scale) {

	vec3_t delta;
	vec_t dot, light;
	cm_trace_t trace;

	if (!sun.light) {
		return;
	}

	dot = DotProduct(sun.dir, normal);

	if (dot <= 0.001) {
		return;    // wrong direction
	}

	VectorMA(pos, MAX_WORLD_DIST, sun.dir, delta);

	Light_Trace(&trace, pos, delta, CONTENTS_SOLID);

	if (trace.fraction < 1.0 && !(trace.surface->flags & SURF_SKY)) {
		return;    // occluded
	}

	light = sun.light * dot * scale;

	// add some light to it
	VectorMA(sample, light, sun.color, sample);

	// and accumulate the direction
	VectorMix(normal, sun.dir, light / sun.light, delta);
	VectorMA(direction, light * scale, delta, direction);
}

/**
 * @brief Iterate over all light sources for the sample position's PVS, accumulating
 * light and directional information to the specified pointers.
 */
static void GatherSampleLight(vec3_t pos, vec3_t normal, byte *pvs, vec_t *sample,
                              vec_t *direction, vec_t scale) {

	light_t *l;
	vec3_t delta;
	vec_t dot, dot2;
	vec_t dist;
	cm_trace_t trace;
	int32_t i;

	// iterate over lights, which are in buckets by cluster
	for (i = 0; i < bsp_file.vis_data.vis->num_clusters; i++) {

		if (!(pvs[i >> 3] & (1 << (i & 7)))) {
			continue;
		}

		for (l = lights[i]; l; l = l->next) {

			vec_t light = 0.0;

			VectorSubtract(l->origin, pos, delta);
			dist = VectorNormalize(delta);

			dot = DotProduct(delta, normal);
			if (dot <= 0.001) {
				continue;    // behind sample surface
			}

			switch (l->type) {
				case LIGHT_POINT: // linear falloff
					light = (l->intensity - dist) * dot;
					break;

				case LIGHT_FACE: // exponential falloff
					light = (l->intensity / (dist * dist)) * dot;
					break;

				case LIGHT_SPOT: // linear falloff with cone
					dot2 = -DotProduct(delta, l->normal);
					if (dot2 > l->stopdot) { // inside the cone
						light = (l->intensity - dist) * dot;
					} else { // outside the cone
						const vec_t decay = 1.0 + l->stopdot - dot2;
						light = (l->intensity - decay * decay * dist) * dot;
					}
					break;
				default:
					Mon_SendPoint(MON_WARN, l->origin, "Light with bad type");
					break;
			}

			if (light <= 0.0) { // no light
				continue;
			}

			Light_Trace(&trace, l->origin, pos, CONTENTS_SOLID);

			if (trace.fraction < 1.0) {
				continue;    // occluded
			}

			// add some light to it
			VectorMA(sample, light * scale, l->color, sample);

			// and add some direction
			VectorMix(normal, delta, 2.0 * light / l->intensity, delta);
			VectorMA(direction, light * scale, delta, direction);
		}
	}

	GatherSampleSunlight(pos, normal, sample, direction, scale);
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
static void SampleNormal(const light_info_t *l, const vec3_t pos, vec3_t normal) {
	vec_t total_dist = 0.0;
	vec_t dist[MAX_VERT_FACES];
	int32_t i;

	// calculate the distance to each vertex
	for (i = 0; i < l->face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[l->face->first_edge + i];
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
	const vec_t max_dist = 2.0 * total_dist / l->face->num_edges;

	// add in weighted components from the vertex normals
	for (i = 0; i < l->face->num_edges; i++) {
		const int32_t e = bsp_file.face_edges[l->face->first_edge + i];
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

#define MAX_SAMPLES 5

static const vec_t sampleofs[MAX_SAMPLES][2] = {
	{ 0.0, 0.0 },
	{ -0.5, -0.5 },
	{ 0.5, -0.5 },
	{ 0.5, 0.5 },
	{ -0.5, 0.5 }
};

/**
 * @brief
 */
void BuildFacelights(int32_t face_num) {
	bsp_face_t *face;
	bsp_plane_t *plane;
	bsp_texinfo_t *tex;
	vec_t *center;
	vec_t *sdir, *tdir, scale;
	vec3_t pos;
	vec3_t normal, bitangent;
	vec4_t tangent;
	light_info_t l[MAX_SAMPLES];
	face_light_t *fl;
	int32_t num_samples;
	int32_t i, j;

	if (face_num >= MAX_BSP_FACES) {
		Com_Verbose("MAX_BSP_FACES hit\n");
		return;
	}

	face = &bsp_file.faces[face_num];
	plane = &bsp_file.planes[face->plane_num];
	tex = &bsp_file.texinfo[face->texinfo];

	if (tex->flags & (SURF_SKY | SURF_WARP)) {
		return;    // non-lit texture
	}

	sdir = tex->vecs[0];
	tdir = tex->vecs[1];

	if (extra_samples) { // -light -extra antialiasing
		num_samples = MAX_SAMPLES;
	} else {
		num_samples = 1;
	}

	scale = 1.0 / num_samples; // each sample contributes this much

	memset(l, 0, sizeof(l));

	for (i = 0; i < num_samples; i++) { // assemble the light_info

		l[i].face = face;
		l[i].face_dist = plane->dist;

		VectorCopy(plane->normal, l[i].face_normal);

		if (face->side) { // negate the normal and dist
			VectorNegate(l[i].face_normal, l[i].face_normal);
			l[i].face_dist = -l[i].face_dist;
		}

		// get the origin offset for rotating bmodels
		VectorCopy(face_offset[face_num], l[i].model_org);

		// calculate lightmap texture mins and maxs
		CalcLightinfoExtents(&l[i]);

		// and the lightmap texture vectors
		CalcLightinfoVectors(&l[i]);

		// now generate all of the sample points
		CalcPoints(&l[i], sampleofs[i][0], sampleofs[i][1]);
	}

	fl = &face_lights[face_num];
	fl->num_samples = l[0].num_sample_points;

	fl->origins = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACELIGHT);
	memcpy(fl->origins, l[0].sample_points, fl->num_samples * sizeof(vec3_t));

	fl->samples = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACELIGHT);
	fl->directions = Mem_TagMalloc(fl->num_samples * sizeof(vec3_t), MEM_TAG_FACELIGHT);

	center = face_extents[face_num].center; // center of the face

	for (i = 0; i < fl->num_samples; i++) { // calculate light for each sample

		vec_t *sample = fl->samples + i * 3; // accumulate lighting here
		vec_t *direction = fl->directions + i * 3; // accumulate direction here

		for (j = 0; j < num_samples; j++) { // with antialiasing
			byte pvs[(MAX_BSP_LEAFS + 7) / 8];

			if (tex->flags & SURF_PHONG) { // interpolated normal
				SampleNormal(&l[0], l[j].sample_points[i], normal);
			} else { // or just plane normal
				VectorCopy(l[0].face_normal, normal);
			}

			if (!NudgeSamplePosition(l[j].sample_points[i], normal, center, pos, pvs)) {
				continue;    // not a valid point
			}

			GatherSampleLight(pos, normal, pvs, sample, direction, scale);
		}

		if (!legacy) { // finalize the lighting direction for the sample

			if (!VectorCompare(direction, vec3_origin)) {
				vec3_t dir;

				// normalize it
				VectorNormalize(direction);

				// transform it into tangent space
				TangentVectors(normal, sdir, tdir, tangent, bitangent);

				dir[0] = DotProduct(direction, tangent);
				dir[1] = DotProduct(direction, bitangent);
				dir[2] = DotProduct(direction, normal);

				VectorCopy(dir, direction);
			}
		}
	}

	if (!legacy) { // pad underlit samples with default direction

		for (i = 0; i < l[0].num_sample_points; i++) { // pad them

			vec_t *direction = fl->directions + i * 3;

			if (VectorCompare(direction, vec3_origin)) {
				VectorSet(direction, 0.0, 0.0, 1.0);
			}
		}
	}

	// free the sample positions for the face
	for (i = 0; i < num_samples; i++) {
		Mem_Free(l[i].sample_points);
	}
}

/**
 * @brief Add the indirect lighting on top of the direct lighting and save into
 * final map format.
 */
void FinalLightFace(int32_t face_num) {
	bsp_face_t *f;
	int32_t j, k;
	vec3_t temp;
	vec3_t dir;
	face_light_t *fl;
	byte *dest;

	f = &bsp_file.faces[face_num];
	fl = &face_lights[face_num];

	if (bsp_file.texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY)) {
		return;    // non-lit texture
	}

	f->unused[0] = 0; // pack the old lightstyles array for legacy games
	f->unused[1] = f->unused[2] = f->unused[3] = 255;

	ThreadLock();

	f->light_ofs = bsp_file.lightmap_data_size;
	bsp_file.lightmap_data_size += fl->num_samples * 3;

	if (!legacy) { // account for light direction data as well
		bsp_file.lightmap_data_size += fl->num_samples * 3;
	}

	if (bsp_file.lightmap_data_size > MAX_BSP_LIGHTING) {
		Com_Error(ERROR_FATAL, "MAX_BSP_LIGHTING\n");
	}

	ThreadUnlock();

	// write it out
	dest = &bsp_file.lightmap_data[f->light_ofs];

	for (j = 0; j < fl->num_samples; j++) {

		// start with raw sample data
		VectorCopy((fl->samples + j * 3), temp);

		// convert to float
		VectorScale(temp, 1.0 / 255.0, temp);

		// add an ambient term if desired
		VectorAdd(temp, ambient, temp);

		// apply brightness, saturation and contrast
		ColorFilter(temp, temp, brightness, saturation, contrast);

		// write the lightmap sample data as bytes
		for (k = 0; k < 3; k++) {
			*dest++ = (byte) Clamp(temp[k] * 255, 0, 255);
		}

		if (!legacy) { // also write the directional data
			VectorCopy((fl->directions + j * 3), dir);
			for (k = 0; k < 3; k++) {
				*dest++ = (byte) ((dir[k] + 1.0) * 127.0);
			}
		}
	}
}
