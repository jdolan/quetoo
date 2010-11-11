/*
 * Copyright(c) 1997-2001 Id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quake2World.
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

#include "renderer.h"

/*
 * In video memory, lightmaps are chunked into NxN RGBA blocks.  In the bsp,
 * they are just RGB, and we retrieve them using floating point for precision.
 */

lightmaps_t r_lightmaps;

/*
 * R_UploadLightmapBlock
 */
static void R_UploadLightmapBlock(){

	if(r_lightmaps.lightmap_texnum == MAX_GL_LIGHTMAPS){
		Com_Warn("R_UploadLightmapBlock: MAX_GL_LIGHTMAPS reached.\n");
		return;
	}

	R_BindTexture(r_lightmaps.lightmap_texnum);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size, r_lightmaps.size,
				0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.sample_buffer);

	r_lightmaps.lightmap_texnum++;

	if(r_loadmodel->version == BSP_VERSION_){  // upload deluxe block as well

		if(r_lightmaps.deluxemap_texnum == MAX_GL_DELUXEMAPS){
			Com_Warn("R_UploadLightmapBlock: MAX_GL_DELUXEMAPS reached.\n");
			return;
		}

		R_BindTexture(r_lightmaps.deluxemap_texnum);

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, r_lightmaps.size, r_lightmaps.size,
				0, GL_RGB, GL_UNSIGNED_BYTE, r_lightmaps.direction_buffer);

		r_lightmaps.deluxemap_texnum++;
	}

	// clear the allocation block and buffers
	memset(r_lightmaps.allocated, 0, r_lightmaps.size * 3);
	memset(r_lightmaps.sample_buffer, 0, r_lightmaps.size * r_lightmaps.size * 3);
	memset(r_lightmaps.direction_buffer, 0, r_lightmaps.size * r_lightmaps.size * 3);
}


/*
 * R_AllocLightmapBlock
 */
static qboolean R_AllocLightmapBlock(int w, int h, int *x, int *y){
	int i, j;
	int best;

	best = r_lightmaps.size;

	for(i = 0; i < r_lightmaps.size - w; i++){
		int best2 = 0;

		for(j = 0; j < w; j++){
			if(r_lightmaps.allocated[i + j] >= best)
				break;
			if(r_lightmaps.allocated[i + j] > best2)
				best2 = r_lightmaps.allocated[i + j];
		}
		if(j == w){  // this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}

	if(best + h > r_lightmaps.size)
		return false;

	for(i = 0; i < w; i++)
		r_lightmaps.allocated[*x + i] = best + h;

	return true;
}


/*
 * R_BuildDefaultLightmap
 */
static void R_BuildDefaultLightmap(msurface_t *surf, byte *sout, byte *dout, int stride){
	int i, j;

	const int smax = (surf->stextents[0] / r_loadmodel->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / r_loadmodel->lightmap_scale) + 1;

	stride -= (smax * 3);

	for(i = 0; i < tmax; i++, sout += stride, dout += stride){
		for(j = 0; j < smax; j++){

			sout[0] = 255;
			sout[1] = 255;
			sout[2] = 255;

			sout += 3;

			if(r_loadmodel->version == BSP_VERSION_){
				dout[0] = 127;
				dout[1] = 127;
				dout[2] = 255;

				dout += 3;
			}
		}
	}

	VectorSet(surf->color, 1.0, 1.0, 1.0);
	surf->color[3] = 1.0;
}


/*
 * R_BuildLightmap
 *
 * Consume raw lightmap and deluxemap RGB/XYZ data from the surface samples,
 * writing processed lightmap and deluxemap RGBA to the specified destinations.
 */
static void R_BuildLightmap(msurface_t *surf, byte *sout, byte *dout, int stride){
	int i, j;
	byte *lightmap, *lm, *l, *deluxemap, *dm;

	const int smax = (surf->stextents[0] / r_loadmodel->lightmap_scale) + 1;
	const int tmax = (surf->stextents[1] / r_loadmodel->lightmap_scale) + 1;

	const int size = smax * tmax;
	stride -= (smax * 3);

	lightmap = (byte *)Z_Malloc(size * 3);
	lm = lightmap;

	deluxemap = dm = NULL;

	if(r_loadmodel->version == BSP_VERSION_){
		deluxemap = (byte *)Z_Malloc(size * 3);
		dm = deluxemap;
	}

	// convert the raw lightmap samples to RGBA for softening
	for(i = j = 0; i < size; i++, lm += 3, dm += 3){
		lm[0] = surf->samples[j++];
		lm[1] = surf->samples[j++];
		lm[2] = surf->samples[j++];

		// read in directional samples for deluxe mapping as well
		if(r_loadmodel->version == BSP_VERSION_){
			dm[0] = surf->samples[j++];
			dm[1] = surf->samples[j++];
			dm[2] = surf->samples[j++];
		}
	}

	// apply modulate, contrast, resolve average surface color, etc..
	R_FilterTexture(lightmap, smax, tmax, surf->color, it_lightmap);

	if(surf->texinfo->flags & (SURF_BLEND33 | SURF_ALPHATEST))
		surf->color[3] = 0.25;
	else if(surf->texinfo->flags & SURF_BLEND66)
		surf->color[3] = 0.50;
	else
		surf->color[3] = 1.0;

	// soften it if it's sufficiently large
	if(r_soften->value && size > 128){
		for(i = 0; i < r_soften->value; i++){
			R_SoftenTexture(lightmap, smax, tmax, it_lightmap);

			if(r_loadmodel->version == BSP_VERSION_)
				R_SoftenTexture(deluxemap, smax, tmax, it_deluxemap);
		}
	}

	// the final lightmap is uploaded to the card via the strided lightmap
	// block, and also cached on the surface for fast point lighting lookups

	surf->lightmap = (byte *)R_HunkAlloc(size * 3);
	l = surf->lightmap;

	lm = lightmap;

	if(r_loadmodel->version == BSP_VERSION_)
		dm = deluxemap;

	for(i = 0; i < tmax; i++, sout += stride, dout += stride){
		for(j = 0; j < smax; j++){

			// copy the lightmap to the strided block
			sout[0] = lm[0];
			sout[1] = lm[1];
			sout[2] = lm[2];
			sout += 3;

			// and to the surface, discarding alpha
			l[0] = lm[0];
			l[1] = lm[1];
			l[2] = lm[2];
			l += 3;

			lm += 3;

			// lastly copy the deluxemap to the strided block
			if(r_loadmodel->version == BSP_VERSION_){
				dout[0] = dm[0];
				dout[1] = dm[1];
				dout[2] = dm[2];
				dout += 3;

				dm += 3;
			}
		}
	}

	Z_Free(lightmap);

	if(r_loadmodel->version == BSP_VERSION_)
		Z_Free(deluxemap);
}


/*
 * R_CreateSurfaceLightmap
 */
void R_CreateSurfaceLightmap(msurface_t *surf){
	int smax, tmax, stride;
	byte *samples, *directions;

	if(!(surf->flags & MSURF_LIGHTMAP))
		return;

	smax = (surf->stextents[0] / r_loadmodel->lightmap_scale) + 1;
	tmax = (surf->stextents[1] / r_loadmodel->lightmap_scale) + 1;

	if(!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)){

		R_UploadLightmapBlock();  // upload the last block

		if(!R_AllocLightmapBlock(smax, tmax, &surf->light_s, &surf->light_t)){
			Com_Error(ERR_DROP, "R_CreateSurfaceLightmap: Consecutive calls to "
					"R_AllocLightmapBlock(%d,%d) failed.", smax, tmax);
		}
	}

	surf->lightmap_texnum = r_lightmaps.lightmap_texnum;
	surf->deluxemap_texnum = r_lightmaps.deluxemap_texnum;

	samples = r_lightmaps.sample_buffer;
	samples += (surf->light_t * r_lightmaps.size + surf->light_s) * 3;

	directions = r_lightmaps.direction_buffer;
	directions += (surf->light_t * r_lightmaps.size + surf->light_s) * 3;

	stride = r_lightmaps.size * 3;

	if(surf->samples)
		R_BuildLightmap(surf, samples, directions, stride);
	else
		R_BuildDefaultLightmap(surf, samples, directions, stride);
}


/*
 * R_BeginBuildingLightmaps
 */
void R_BeginBuildingLightmaps(void){
	int max;

	// users can tune lightmap size for their card
	r_lightmaps.size = (int)r_lightmapsize->value;

	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max);

	// but clamp it to the card's capability to avoid errors
	if(r_lightmaps.size < 256)
		r_lightmaps.size = 256;
	if(r_lightmaps.size > max)
		r_lightmaps.size = max;

	r_lightmaps.allocated = (unsigned *)R_HunkAlloc(r_lightmaps.size * sizeof(unsigned));

	r_lightmaps.sample_buffer = (byte *)R_HunkAlloc(
			r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));

	r_lightmaps.direction_buffer = (byte *)R_HunkAlloc(
			r_lightmaps.size * r_lightmaps.size * sizeof(unsigned));

	r_lightmaps.lightmap_texnum = TEXNUM_LIGHTMAPS;
	r_lightmaps.deluxemap_texnum = TEXNUM_DELUXEMAPS;
}


/*
 * R_EndBuildingLightmaps
 */
void R_EndBuildingLightmaps(void){

	// upload the pending lightmap block
	R_UploadLightmapBlock();
}


/*
 * R_LightPointColor_
 *
 * Clip to all surfaces within the specified range, accumulating static lighting
 * color to the specified vector in the event of an intersection.
 */
static qboolean R_LightPointColor_(const int firstsurface, const int numsurfaces,
		const vec3_t point, vec3_t color){
	msurface_t *surf;
	mtexinfo_t *tex;
	int i, width, sample;
	float s, t;

	// resolve the surface that was impacted
	surf = r_worldmodel->surfaces + firstsurface;

	for(i = 0; i < numsurfaces; i++, surf++){

		if(!(surf->flags & MSURF_LIGHTMAP))
			continue;  // no lightmap

		if(strcmp(r_view.trace.surface->name, surf->texinfo->name))
			continue;  // wrong material

		if(!VectorCompare(r_view.trace.plane.normal, surf->plane->normal))
			continue;  // facing the wrong way

		if(surf->trace_num == r_locals.trace_num)
			continue;  // already checked this trace

		surf->trace_num = r_locals.trace_num;

		tex = surf->texinfo;

		s = DotProduct(point, tex->vecs[0]) + tex->vecs[0][3] - surf->stmins[0];
		t = DotProduct(point, tex->vecs[1]) + tex->vecs[1][3] - surf->stmins[1];

		if((s < 0.0 || s > surf->stextents[0]) || (t < 0.0 || t > surf->stextents[1]))
			continue;

		// we've hit, resolve the texture coordinates
		s /= r_worldmodel->lightmap_scale;
		t /= r_worldmodel->lightmap_scale;

		// resolve the lightmap at intersection
		width = (surf->stextents[0] / r_worldmodel->lightmap_scale) + 1;
		sample = (int)(3 * ((int)t * width + (int)s));

		// and convert it to floating point
		VectorScale((&surf->lightmap[sample]), 1.0 / 255.0, color);
		return true;
	}

	return false;
}


#define STATIC_LIGHTING_MIN_COMPONENT 0.05
#define STATIC_LIGHTING_MIN_SUM 0.75

/*
 * R_LightPointClamp
 *
 * Clamps and scales the newly resolved sample color into an acceptable range.
 */
static void R_LightPointClamp(static_lighting_t *lighting){
	qboolean clamped;
	float f;
	int i;

	clamped = false;

	for(i = 0; i < 3; i++){  // clamp it
		if(lighting->colors[0][i] < STATIC_LIGHTING_MIN_COMPONENT){
			lighting->colors[0][i] = STATIC_LIGHTING_MIN_COMPONENT;
			clamped = true;
		}
	}

	// scale it into acceptable range
	while(VectorSum(lighting->colors[0]) < STATIC_LIGHTING_MIN_SUM){
		VectorScale(lighting->colors[0], 1.25, lighting->colors[0]);
		clamped = true;
	}

	if(!clamped)  // the color was fine, leave it be
		return;

	// pale it out some to minimize scaling artifacts
	f = VectorSum(lighting->colors[0]) / 3.0;

	for(i = 0; i < 3; i++)
		lighting->colors[0][i] = (lighting->colors[0][i] + f) / 2.0;
}


/*
 * R_LightPointColor
 *
 * Resolve the lighting color at the point of impact from the downward trace.
 * If the trace did not intersect a surface, use the level's ambient color.
 */
static void R_LightPointColor(static_lighting_t *lighting){

	// shuffle the samples
	VectorCopy(lighting->colors[0], lighting->colors[1]);

	if(r_view.trace.leafnum){  // hit something

		VectorSet(lighting->colors[0], 1.0, 1.0, 1.0);

		if(r_worldmodel->lightdata){  // resolve the lighting sample

			if(r_view.trace_ent){  // clip to all surfaces of the bsp entity

				VectorSubtract(r_view.trace.endpos,
						r_view.trace_ent->origin, r_view.trace.endpos);

				R_LightPointColor_(r_view.trace_ent->model->firstmodelsurface,
						r_view.trace_ent->model->nummodelsurfaces,
						r_view.trace.endpos, lighting->colors[0]);
			}
			else {  // general case is to recurse up the nodes
				mleaf_t *leaf = &r_worldmodel->leafs[r_view.trace.leafnum];
				mnode_t *node = leaf->parent;

				while(node){

					if(R_LightPointColor_(node->firstsurface, node->numsurfaces,
								r_view.trace.endpos, lighting->colors[0]))
						break;

					node = node->parent;
				}
			}
		}
	}
	else {  // use the level's ambient lighting
		VectorCopy(r_view.ambient_light, lighting->colors[0]);
	}

	R_LightPointClamp(lighting);
}


/*
 * R_LightPointPosition
 *
 * Resolves the closest static light source, populating the lighting's position
 * element.  This facilitates directional shading in the fragment program.
 */
static void R_LightPointPosition(static_lighting_t *lighting){
	mbsplight_t *l;
	float light, best;
	vec3_t delta;
	int i;

	if(!r_state.lighting_enabled)  // don't bother
		return;

	// shuffle the samples
	VectorCopy(lighting->positions[0], lighting->positions[1]);

	// set a safe default in case we don't find a nearby light
	VectorSet(delta, 0.0, 0.0, 1.0);
	VectorMA(lighting->origin, 64.0, delta, lighting->positions[0]);

	best = 0.0;

	l = r_worldmodel->bsplights;
	for(i = 0; i < r_worldmodel->numbsplights; i++, l++){

		if(l->leaf->vis_frame != r_locals.vis_frame)
			continue;

		VectorSubtract(l->org, lighting->origin, delta);
		light = l->radius - VectorLength(delta);

		if(light > best){  // it's close, but is it in sight

			R_Trace(l->org, lighting->origin, 0.0, CONTENTS_SOLID);
			if(r_view.trace.fraction < 1.0)
				continue;

			best = light;
			VectorCopy(l->org, lighting->positions[0]);
		}
	}

	// if we've changed light sources, force a long lerp
	if(!VectorCompare(lighting->positions[0], lighting->positions[1])){
		VectorMix(lighting->positions[0], lighting->positions[1], 0.5, lighting->positions[0]);
	}
}


#define STATIC_LIGHTING_INTERVAL 0.1

/*
 * R_LightPointLerp
 *
 * Interpolate color and position information.
 */
static void R_LightPointLerp(static_lighting_t *lighting){
	float lerp;

	if(VectorCompare(lighting->colors[1], vec3_origin)){  // only one sample available
		VectorCopy(lighting->colors[0], lighting->color);
		VectorCopy(lighting->positions[0], lighting->position);
		return;
	}

	// calculate the lerp fraction
	lerp = (r_view.time - lighting->time) / STATIC_LIGHTING_INTERVAL;

	// and lerp the colors
	VectorMix(lighting->colors[1], lighting->colors[0], lerp, lighting->color);

	// and the positions
	VectorMix(lighting->positions[1], lighting->positions[0], lerp, lighting->position);
}


/*
 * R_LightPoint
 *
 * Resolve static lighting information for the specified point.  Linear
 * interpolation is used to blend the previous lighting information, provided
 * it is recent.
 */
void R_LightPoint(const vec3_t point, static_lighting_t *lighting){
	vec3_t start, end;
	float delta;

	lighting->dirty = false;  // mark it clean

	if(lighting->time > r_view.time)  // new level check
		lighting->time = 0.0;

	// copy the origin
	VectorCopy(point, lighting->origin);

	// do the trace
	VectorCopy(lighting->origin, start);
	VectorCopy(lighting->origin, end);
	end[2] -= 512.0;

	R_Trace(start, end, 0.0, MASK_SOLID);

	// resolve the shadow origin and direction

	if(r_view.trace.leafnum){  // hit something
		VectorCopy(r_view.trace.endpos, lighting->point);
		VectorCopy(r_view.trace.plane.normal, lighting->normal);
	}
	else {  // clear it
		VectorClear(lighting->point);
		VectorClear(lighting->normal);
	}

	// resolve the lighting sample using linear interpolation

	delta = r_view.time - lighting->time;

	if(lighting->time && delta < STATIC_LIGHTING_INTERVAL){
		R_LightPointLerp(lighting);  // still valid, just lerp
		return;
	}

	// bump the time
	lighting->time = r_view.time;

	// resolve the lighting color
	R_LightPointColor(lighting);

	// resolve the static light source position
	R_LightPointPosition(lighting);

	// interpolate the static lighting information
	R_LightPointLerp(lighting);
}
