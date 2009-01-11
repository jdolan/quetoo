/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
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

static int lightmap_scale;

// lightinfo is a temporary bucket for lighting calculations
typedef struct {
	vec_t facedist;
	vec3_t facenormal;

	int numsurfpt;
	vec3_t *surfpt;

	vec3_t modelorg;  // for origined bmodels

	vec3_t texorg;
	vec3_t worldtotex[2];  // s = (world - texorg) . worldtotex[0]
	vec3_t textoworld[2];  // world = texorg + s * textoworld[0]

	vec2_t exactmins, exactmaxs;

	int texmins[2], texsize[2];
	dface_t *face;
} lightinfo_t;

// face extents
typedef struct extents_s {
	vec3_t mins, maxs;
	vec3_t center;
	vec2_t stmins, stmaxs;
} extents_t;

static extents_t face_extents[MAX_BSP_FACES];


/*
 * BuildFaceExtents
 *
 * Populates face_extents for all dsurface_t, prior to light creation.
 * This is done so that sample positions may be nudged outward along
 * the face normal and towards the face center to help with traces.
 */
static void BuildFaceExtents(void){
	const dbspvertex_t *v;
	int i, j, k;

	for(k = 0; k < numfaces; k++){

		const dface_t *s = &dfaces[k];
		const dtexinfo_t *tex = &texinfo[s->texinfo];

		float *mins = face_extents[s - dfaces].mins;
		float *maxs = face_extents[s - dfaces].maxs;

		float *center = face_extents[s - dfaces].center;

		float *stmins = face_extents[s - dfaces].stmins;
		float *stmaxs = face_extents[s - dfaces].stmaxs;

		VectorSet(mins, 999999, 999999, 999999);
		VectorSet(maxs, -999999, -999999, -999999);

		stmins[0] = stmins[1] = 999999;
		stmaxs[0] = stmaxs[1] = -999999;

		for(i = 0; i < s->numedges; i++){
			const int e = dsurfedges[s->firstedge + i];
			if(e >= 0)
				v = dvertexes + dedges[e].v[0];
			else
				v = dvertexes + dedges[-e].v[1];

			for(j = 0; j < 3; j++){  // calculate mins, maxs
				if(v->point[j] > maxs[j])
					maxs[j] = v->point[j];
				if(v->point[j] < mins[j])
					mins[j] = v->point[j];
			}

			for(j = 0; j < 2; j++){  // calculate stmins, stmaxs
				const float val = DotProduct(v->point, tex->vecs[j]) + tex->vecs[j][3];
				if(val < stmins[j])
					stmins[j] = val;
				if(val > stmaxs[j])
					stmaxs[j] = val;
			}
		}

		for(i = 0; i < 3; i++)  // calculate center
			center[i] = (mins[i] + maxs[i]) / 2.0;
	}
}


/*
 * CalcLightinfoExtents
 *
 * Fills in l->texmins[] and l->texsize[], l->exactmins[] and l->exactmaxs[]
 */
static void CalcLightinfoExtents(lightinfo_t *l){
	const dface_t *s;
	float *mins, *maxs;
	float *stmins, *stmaxs;
	vec2_t lm_mins, lm_maxs;
	int i;

	s = l->face;

	mins = face_extents[s - dfaces].mins;
	maxs = face_extents[s - dfaces].maxs;

	stmins = face_extents[s - dfaces].stmins;
	stmaxs = face_extents[s - dfaces].stmaxs;

	for(i = 0; i < 2; i++){
		l->exactmins[i] = stmins[i];
		l->exactmaxs[i] = stmaxs[i];

		lm_mins[i] = floor(stmins[i] / lightmap_scale);
		lm_maxs[i] = ceil(stmaxs[i] / lightmap_scale);

		l->texmins[i] = lm_mins[i];
		l->texsize[i] = lm_maxs[i] - lm_mins[i];
	}

	if(l->texsize[0] * l->texsize[1] > MAX_BSP_LIGHTMAP)
		Error("Surface too large to light (%dx%d)\n", l->texsize[0], l->texsize[1]);
}


/*
 * CalcLightinfoVectors
 *
 * Fills in texorg, worldtotex. and textoworld
 */
static void CalcLightinfoVectors(lightinfo_t *l){
	const dtexinfo_t *tex;
	int i;
	vec3_t texnormal;
	vec_t distscale;
	vec_t dist;

	tex = &texinfo[l->face->texinfo];

	// convert from float to double
	for(i = 0; i < 2; i++)
		VectorCopy(tex->vecs[i], l->worldtotex[i]);

	// calculate a normal to the texture axis.  points can be moved along this
	// without changing their S/T
	texnormal[0] = tex->vecs[1][1] * tex->vecs[0][2]
	               - tex->vecs[1][2] * tex->vecs[0][1];
	texnormal[1] = tex->vecs[1][2] * tex->vecs[0][0]
	               - tex->vecs[1][0] * tex->vecs[0][2];
	texnormal[2] = tex->vecs[1][0] * tex->vecs[0][1]
	               - tex->vecs[1][1] * tex->vecs[0][0];
	VectorNormalize(texnormal);

	// flip it towards plane normal
	distscale = DotProduct(texnormal, l->facenormal);
	if(distscale == 0.0){
		Print("WARNING: Texture axis perpendicular to face\n");
		distscale = 1.0;
	}
	if(distscale < 0.0){
		distscale = -distscale;
		VectorSubtract(vec3_origin, texnormal, texnormal);
	}
	// distscale is the ratio of the distance along the texture normal to
	// the distance along the plane normal
	distscale = 1.0 / distscale;

	for(i = 0; i < 2; i++){
		const vec_t len = VectorLength(l->worldtotex[i]);
		const vec_t distance = DotProduct(l->worldtotex[i], l->facenormal) * distscale;
		VectorMA(l->worldtotex[i], -distance, texnormal, l->textoworld[i]);
		VectorScale(l->textoworld[i], (1 / len) * (1 / len), l->textoworld[i]);
	}

	// calculate texorg on the texture plane
	for(i = 0; i < 3; i++)
		l->texorg[i] =
		    -tex->vecs[0][3] * l->textoworld[0][i] -
		    tex->vecs[1][3] * l->textoworld[1][i];

	// project back to the face plane
	dist = DotProduct(l->texorg, l->facenormal) - l->facedist - 1;
	dist *= distscale;
	VectorMA(l->texorg, -dist, texnormal, l->texorg);

	// compensate for org'd bmodels
	VectorAdd(l->texorg, l->modelorg, l->texorg);

	// total sample count
	l->numsurfpt = (l->texsize[0] + 1) * (l->texsize[1] + 1);
	l->surfpt = Z_Malloc(l->numsurfpt * sizeof(vec3_t));
}


/*
 * CalcPoints
 *
 * For each texture aligned grid point, back project onto the plane
 * to get the world xyz value of the sample point
 */
static void CalcPoints(lightinfo_t *l, float sofs, float tofs){
	int s, t, j;
	int w, h, step;
	vec_t starts, startt;
	vec_t *surf;

	surf = l->surfpt[0];

	h = l->texsize[1] + 1;
	w = l->texsize[0] + 1;

	step = lightmap_scale;
	starts = l->texmins[0] * step;
	startt = l->texmins[1] * step;

	for(t = 0; t < h; t++){
		for(s = 0; s < w; s++, surf += 3){
			const vec_t us = starts + (s + sofs) * step;
			const vec_t ut = startt + (t + tofs) * step;

			// calculate texture point
			for(j = 0; j < 3; j++)
				surf[j] = l->texorg[j] + l->textoworld[0][j] * us +
					l->textoworld[1][j] * ut;
		}
	}
}


typedef struct {  // buckets for sample accumulation
	int numsamples;
	float *origins;
	float *samples;
	float *directions;
} facelight_t;

static facelight_t facelight[MAX_BSP_FACES];

typedef struct light_s {  // a light source
	struct light_s *next;
	emittype_t type;

	float intensity;  // brightness
	vec3_t origin;
	vec3_t color;
	vec3_t normal;  // spotlight direction
	float stopdot;  // spotlight cone
} light_t;

static light_t *lights[MAX_BSP_LEAFS];
static int numlights;

// sunlight, borrowed from ufo2map
typedef struct sun_s {
	float intensity;
	vec3_t color;
	vec3_t angles;  // entity-style angles
	vec3_t normal;  // normalized direction
} sun_t;

static sun_t sun;


/*
 * FindTargetEntity
 */
static entity_t *FindTargetEntity(const char *target){
	int i;

	for(i = 0; i < num_entities; i++){
		const char *n = ValueForKey(&entities[i], "targetname");
		if(!strcmp(n, target))
			return &entities[i];
	}

	return NULL;
}


#define ANGLE_UP	-1.0
#define ANGLE_DOWN	-2.0

/*
 * BuildLights
 */
void BuildLights(void){
	int i;
	light_t *l;
	const dleaf_t *leaf;
	int cluster;
	const char *target;
	vec3_t dest;
	const char *color;
	const char *angles;
	float f, intensity;

	// surfaces
	for(i = 0; i < MAX_BSP_FACES; i++){

		patch_t *p = face_patches[i];

		while(p){  // iterate subdivided patches

			if(VectorCompare(p->light, vec3_origin))
				continue;

			numlights++;
			l = Z_Malloc(sizeof(*l));

			VectorCopy(p->origin, l->origin);

			leaf = Light_PointInLeaf(l->origin);
			cluster = leaf->cluster;
			l->next = lights[cluster];
			lights[cluster] = l;

			l->type = emit_surface;

			l->intensity = ColorNormalize(p->light, l->color);
			l->intensity *= p->area * surface_scale;

			p = p->next;
		}
	}

	// entities
	for(i = 1; i < num_entities; i++){

		const entity_t *e = &entities[i];

		const char *name = ValueForKey(e, "classname");

		if(strncmp(name, "light", 5))  // not a light
			continue;

		numlights++;
		l = Z_Malloc(sizeof(*l));

		GetVectorForKey(e, "origin", l->origin);

		leaf = Light_PointInLeaf(l->origin);
		cluster = leaf->cluster;

		l->next = lights[cluster];
		lights[cluster] = l;

		intensity = FloatForKey(e, "light");
		if(!intensity)
			intensity = FloatForKey(e, "_light");
		if(!intensity)
			intensity = 300.0;

		color = ValueForKey(e, "_color");
		if(color && color[0]){
			sscanf(color, "%f %f %f", &l->color[0], &l->color[1], &l->color[2]);
			ColorNormalize(l->color, l->color);
		} else {
			VectorSet(l->color, 1.0, 1.0, 1.0);
		}

		l->intensity = intensity * entity_scale;
		l->type = emit_point;

		target = ValueForKey(e, "target");
		if(!strcmp(name, "light_spot") || target[0]){

			l->type = emit_spotlight;

			l->stopdot = FloatForKey(e, "_cone");

			if(!l->stopdot)  // reasonable default cone
				l->stopdot = 10;

			l->stopdot = cos(l->stopdot / 180.0 * M_PI);

			if(target[0]){  // point towards target
				entity_t *e2 = FindTargetEntity(target);
				if(!e2){
					Print("WARNING: light at (%i %i %i) has missing target\n",
							(int)l->origin[0], (int)l->origin[1],
							(int)l->origin[2]);
				} else {
					GetVectorForKey(e2, "origin", dest);
					VectorSubtract(dest, l->origin, l->normal);
					VectorNormalize(l->normal);
				}
			} else {  // point down angle
				const float angle = FloatForKey(e, "angle");
				if(angle == ANGLE_UP){
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = 1.0;
				} else if(angle == ANGLE_DOWN){
					l->normal[0] = l->normal[1] = 0.0;
					l->normal[2] = -1.0;
				} else {  // general case
					l->normal[2] = 0;
					l->normal[0] = cos(angle / 180.0 * M_PI);
					l->normal[1] = sin(angle / 180.0 * M_PI);
				}
			}
		}
	}

	Verbose("Lighting %i lights\n", numlights);

	{
		// sun.intensity parameters come from worldspawn
		const entity_t *e = &entities[0];

		sun.intensity = FloatForKey(e, "sun_light");

		VectorSet(sun.color, 1.0, 1.0, 1.0);
		color = ValueForKey(e, "sun_color");
		if(color && color[0]){
			sscanf(color, "%f %f %f", &sun.color[0], &sun.color[1], &sun.color[2]);
			ColorNormalize(sun.color, sun.color);
		}

		angles = ValueForKey(e, "sun_angles");

		VectorClear(sun.angles);
		sscanf(angles, "%f %f", &sun.angles[0], &sun.angles[1]);

		AngleVectors(sun.angles, sun.normal, NULL, NULL);

		if(sun.intensity)
			Verbose("Sun defined with light %3.0f, color %0.2f %0.2f %0.2f, "
					"angles %1.3f %1.3f %1.3f\n",
					sun.intensity, sun.color[0], sun.color[1], sun.color[2],
					sun.angles[0], sun.angles[1], sun.angles[2]);

		// ambient light, also from worldspawn
		color = ValueForKey(e, "ambient_light");
		sscanf(color, "%f %f %f", &ambient[0], &ambient[1], &ambient[2]);

		if(VectorLength(ambient))
			Verbose("Ambient lighting defined with color %0.2f %0.2f %0.2f\n",
					ambient[0], ambient[1], ambient[2]);

		// optionally pull brightness from worldspawn
		f = FloatForKey(e, "brightness");
		if(f > 0.0)
			brightness = f;

		// saturation as well
		f = FloatForKey(e, "saturation");
		if(f > 0.0)
			saturation = f;

		f = FloatForKey(e, "contrast");
		if(f > 0.0)
			contrast = f;

		// lightmap resolution downscale (e.g. 8 = 1 / 8)
		lightmap_scale = (int)FloatForKey(e, "lightmap_scale");
		if(!lightmap_scale)
			lightmap_scale = DEFAULT_LIGHTMAP_SCALE;
	}
}


/*
 * GatherSampleSunlight
 *
 * A follow-up to GatherSampleLight, simply trace along the sun normal, adding
 * sunlight when a sky surface is struck.
 */
static void GatherSampleSunlight(const vec3_t pos, const vec3_t normal,
		float *sample, float *direction, float scale){

	vec3_t delta;
	float dot, light;
	trace_t trace;

	if(!sun.intensity)
		return;

	dot = DotProduct(sun.normal, normal);

	if(dot <= 0.001)
		return;  // wrong direction

	VectorMA(pos, 2 * MAX_WORLD_WIDTH, sun.normal, delta);

	Light_Trace(&trace, pos, delta, MASK_SOLID);

	if(trace.fraction < 1.0 && !(trace.surface->flags & SURF_SKY))
		return;  // occluded

	light = sun.intensity * dot * scale;

	// add some light to it
	VectorMA(sample, light, sun.color, sample);

	// and accumulate the direction
	VectorMix(normal, sun.normal, light / sun.intensity, delta);
	VectorMA(direction, light * scale, delta, direction);
}


/*
 * GatherSampleLight
 *
 * Iterate over all light sources for the sample position's PVS, accumulating
 * light and directional information to the specified pointers.
 */
static void GatherSampleLight(vec3_t pos, vec3_t normal, byte *pvs,
		float *sample, float *direction, float scale){

	light_t *l;
	vec3_t delta;
	float dot, dot2;
	float dist;
	trace_t trace;
	int i;

	// iterate over lights, which are in buckets by cluster
	for(i = 0; i < dvis->numclusters; i++){

		if(!(pvs[i >> 3] & (1 << (i & 7))))
			continue;

		for(l = lights[i]; l; l = l->next){

			float light = 0.0;

			VectorSubtract(l->origin, pos, delta);
			dist = VectorNormalize(delta);

			dot = DotProduct(delta, normal);
			if(dot <= 0.001)
				continue;  // behind sample surface

			switch(l->type){
				case emit_point:  // linear falloff
					light = (l->intensity - dist) * dot;
					break;

				case emit_surface:  // exponential falloff
					light = (l->intensity / (dist * dist)) * dot;
					break;

				case emit_spotlight:  // linear falloff with cone
					dot2 = -DotProduct(delta, l->normal);
					if(dot2 > l->stopdot)  // inside the cone
						light = (l->intensity - dist) * dot;
					else  // outside the cone
						light = (l->intensity * 0.5 - dist) * dot;
					break;
				default:
					Error("Bad l->type\n");
			}

			if(light <= 0.0)  // no light
				continue;

			Light_Trace(&trace, l->origin, pos, MASK_SOLID);

			if(trace.fraction < 1.0)
				continue;  // occluded

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

/*
 * NudgeSamplePosition
 *
 * Move the incoming sample position towards the surface center and along the
 * surface normal to reduce false-positive traces.  Test the PVS at the new
 * position, returning true if the new point is valid, false otherwise.
 */
static qboolean NudgeSamplePosition(const vec3_t in, const vec3_t normal, const vec3_t center,
		vec3_t out, byte *pvs){

	vec3_t dir;

	VectorCopy(in, out);

	// move into the level using the normal and surface center
	VectorSubtract(out, center, dir);
	VectorNormalize(dir);

	VectorMA(out, SAMPLE_NUDGE, dir, out);
	VectorMA(out, SAMPLE_NUDGE, normal, out);

	return PvsForOrigin(out, pvs);
}


#define MAX_VERT_FACES 256

/*
 * FacesWithEdge
 *
 * Populate faces with indexes of all dface_t's referencing the specified edge.
 * The number of dface_t's referencing edge is returned in nfaces.
 */
static void FacesWithVert(int vert, int *faces, int *nfaces){
	int i, j, k;

	k = 0;
	for(i = 0; i < numfaces; i++){
		const dface_t *face = &dfaces[i];

		if(!(texinfo[face->texinfo].flags & SURF_PHONG))
			continue;

		for(j = 0; j < face->numedges; j++){

			const int e = dsurfedges[face->firstedge + j];
			const int v = e >= 0 ? dedges[e].v[0] : dedges[-e].v[1];

			if(v == vert){  // face references vert
				faces[k++] = i;
				if(k == MAX_VERT_FACES)
					Error("MAX_VERT_FACES\n");
				break;
			}
		}
	}
	*nfaces = k;
}


/*
 * BuildVertexNormals
 *
 * Calculate per-vertex (instead of per-plane) normal vectors.  This is done by
 * finding all of the faces which share a given vertex, and calculating a weighted
 * average of their normals.
 */
void BuildVertexNormals(void){
	int vert_faces[MAX_VERT_FACES];
	int num_vert_faces;
	vec3_t norm, delta;
	float scale;
	int i, j;

	BuildFaceExtents();

	for(i = 0; i < numvertexes; i++){

		VectorClear(dnormals[i].normal);

		FacesWithVert(i, vert_faces, &num_vert_faces);

		if(!num_vert_faces)  // rely on plane normal only
			continue;

		for(j = 0; j < num_vert_faces; j++){
			const dface_t *face = &dfaces[vert_faces[j]];
			const dplane_t *plane = &dplanes[face->planenum];

			// scale the contribution of each face based on size
			VectorSubtract(face_extents[vert_faces[j]].maxs,
					face_extents[vert_faces[j]].mins, delta);

			scale = VectorLength(delta);

			if(face->side)
				VectorScale(plane->normal, -scale, norm);
			else
				VectorScale(plane->normal, scale, norm);

			VectorAdd(dnormals[i].normal, norm, dnormals[i].normal);
		}

		VectorNormalize(dnormals[i].normal);
	}
}


/*
 * SampleNormal
 *
 * For Phong-shaded samples, interpolate the vertex normals for the surface in
 * question, weighing them according to their proximity to the sample position.
 */
static void SampleNormal(const lightinfo_t *l, const vec3_t pos, vec3_t normal){
	vec3_t temp;
	float dist[MAX_VERT_FACES];
	float nearest;
	int i, v, nearv;

	nearest = 9999.0;
	nearv = 0;

	// calculate the distance to each vertex
	for(i = 0; i < l->face->numedges; i++){
		const int e = dsurfedges[l->face->firstedge + i];
		if(e >= 0)
			v = dedges[e].v[0];
		else
			v = dedges[-e].v[1];

		VectorSubtract(pos, dvertexes[v].point, temp);
		dist[i] = VectorLength(temp);

		if(dist[i] < nearest){
			nearest = dist[i];
			nearv = v;
		}
	}

	VectorCopy(dnormals[nearv].normal, normal);
}


#define MAX_SAMPLES 5
static const float sampleofs[MAX_SAMPLES][2] = {
	{0.0, 0.0},
	{-0.125, -0.125}, {0.125, -0.125}, {0.125, 0.125}, {-0.125, 0.125}
};

/*
 * BuildFacelights
 */
void BuildFacelights(int facenum){
	dface_t *face;
	dplane_t *plane;
	dtexinfo_t *tex;
	float *center;
	float *sdir, *tdir, scale;
	vec3_t pos;
	vec3_t normal, binormal;
	vec4_t tangent;
	lightinfo_t l[MAX_SAMPLES];
	facelight_t *fl;
	int numsamples;
	int i, j;

	if(facenum >= MAX_BSP_FACES){
		Verbose("MAX_BSP_FACES hit\n");
		return;
	}

	face = &dfaces[facenum];
	plane = &dplanes[face->planenum];

	tex = &texinfo[face->texinfo];

	if(tex->flags & (SURF_SKY | SURF_WARP))
		return;  // non-lit texture

	sdir = tex->vecs[0];
	tdir = tex->vecs[1];

	if(extrasamples)  // rad -extra antialiasing
		numsamples = MAX_SAMPLES;
	else
		numsamples = 1;

	scale = 1.0 / numsamples;  // each sample contributes this much

	memset(l, 0, sizeof(l));

	for(i = 0; i < numsamples; i++){  // assemble the lightinfo

		l[i].face = face;
		l[i].facedist = plane->dist;

		VectorCopy(plane->normal, l[i].facenormal);

		if(face->side){  // negate the normal and dist
			VectorNegate(l[i].facenormal, l[i].facenormal);
			l[i].facedist = -l[i].facedist;
		}

		// get the origin offset for rotating bmodels
		VectorCopy(face_offset[facenum], l[i].modelorg);

		// calculate lightmap texture mins and maxs
		CalcLightinfoExtents(&l[i]);

		// and the lightmap texture vectors
		CalcLightinfoVectors(&l[i]);

		// now generate all of the sample points
		CalcPoints(&l[i], sampleofs[i][0], sampleofs[i][1]);
	}

	fl = &facelight[facenum];
	fl->numsamples = l[0].numsurfpt;

	fl->origins = Z_Malloc(fl->numsamples * sizeof(vec3_t));
	memcpy(fl->origins, l[0].surfpt, fl->numsamples * sizeof(vec3_t));

	fl->samples = Z_Malloc(fl->numsamples * sizeof(vec3_t));
	fl->directions = Z_Malloc(fl->numsamples * sizeof(vec3_t));

	center = face_extents[facenum].center;  // center of the face

	for(i = 0; i < fl->numsamples; i++){  // calculate light for each sample

		float *sample = fl->samples + i * 3;  // accumulate lighting here
		float *direction = fl->directions + i * 3;  // accumulate direction here

		for(j = 0; j < numsamples; j++){  // with antialiasing
			byte pvs[(MAX_BSP_LEAFS + 7) / 8];

			if(tex->flags & SURF_PHONG){  // interpolated normal
				SampleNormal(&l[0], l[j].surfpt[i], normal);
			}
			else {  // or just plane normal
				VectorCopy(l[0].facenormal, normal);
			}

			if(!NudgeSamplePosition(l[j].surfpt[i], normal, center, pos, pvs))
				continue;  // not a valid point

			GatherSampleLight(pos, normal, pvs, sample, direction, scale);
		}

		if(!legacy){  // finalize the lighting direction for the sample

			if(!VectorCompare(direction, vec3_origin)){
				vec3_t dir;

				// normalize it
				VectorNormalize(direction);

				// transform it into tangent space
				TangentVectors(normal, sdir, tdir, tangent, binormal);

				dir[0] = DotProduct(direction, tangent);
				dir[1] = DotProduct(direction, binormal);
				dir[2] = DotProduct(direction, normal);

				VectorCopy(dir, direction);
			}
		}
	}

	if(!legacy){  // pad underlit samples with default direction

		for(i = 0; i < l[0].numsurfpt; i++){  // pad them

			float *direction = fl->directions + i * 3;

			if(VectorCompare(direction, vec3_origin))
				VectorSet(direction, 0.0, 0.0, 1.0);
		}
	}

	// free the sample positions for the face
	for(i = 0; i < numsamples; i++){
		Z_Free(l[i].surfpt);
	}
}

static const vec3_t luminosity = {0.2125, 0.7154, 0.0721};

/*
 * FinalLightFace
 *
 * Add the indirect lighting on top of the direct lighting and save into
 * final map format.
 */
void FinalLightFace(int facenum){
	dface_t *f;
	int j, k;
	vec3_t temp, intensity;
	vec3_t dir;
	facelight_t *fl;
	byte *dest;
	float max, d;

	f = &dfaces[facenum];
	fl = &facelight[facenum];

	if(texinfo[f->texinfo].flags & (SURF_WARP | SURF_SKY))
		return;  // non-lit texture

	f->unused[0] = 0;  // pack the old lightstyles array for legacy games
	f->unused[1] = f->unused[2] = f->unused[3] = 255;

	ThreadLock();

	f->lightofs = lightdatasize;
	lightdatasize += fl->numsamples * 3;

	if(!legacy)  // account for light direction data as well
		lightdatasize += fl->numsamples * 3;

	if(lightdatasize > MAX_BSP_LIGHTING)
		Error("MAX_BSP_LIGHTING\n");

	ThreadUnlock();

	// write it out
	dest = &dlightdata[f->lightofs];

	for(j = 0; j < fl->numsamples; j++){

		// start with raw sample data
		VectorCopy((fl->samples + j * 3), temp);

		// convert to float
		VectorScale(temp, 1.0 / 255.0, temp);

		// add an ambient term if desired
		VectorAdd(temp, ambient, temp);

		// apply global scale factor
		VectorScale(temp, brightness, temp);

		max = 0.0;

		for(k = 0; k < 3; k++){  // find the brightest component

			if(temp[k] < 0.0)  // enforcing positive values
				temp[k] = 0.0;

			if(temp[k] > max)
				max = temp[k];
		}

		if(max > 255.0)  // clamp without changing hue
			VectorScale(temp, 255.0 / max, temp);

		for(k = 0; k < 3; k++){  // apply contrast

			temp[k] -= 0.5;  // normalize to -0.5 through 0.5

			temp[k] *= contrast;  // scale

			temp[k] += 0.5;

			if(temp[k] > 1.0)  // clamp
				temp[k] = 1.0;
			else if(temp[k] < 0)
				temp[k] = 0;
		}

		// apply saturation
		d = DotProduct(temp, luminosity);

		VectorSet(intensity, d, d, d);
		VectorMix(intensity, temp, saturation, temp);

		// write the lightmap sample data
		for(k = 0; k < 3; k++){

			temp[k] *= 255.0;  // back to byte

			if(temp[k] > 255.0)  // clamp
				temp[k] = 255.0;
			else if(temp[k] < 0.0)
				temp[k] = 0.0;

			*dest++ = (byte)temp[k];
		}

		if(!legacy){  // also write the directional data
			VectorCopy((fl->directions + j * 3), dir);
			for(k = 0; k < 3; k++){
				*dest++ = (byte)((dir[k] + 1.0) * 127.0);
			}
		}
	}
}
