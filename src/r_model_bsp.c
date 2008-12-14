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

#include "renderer.h"

static const byte *mod_base;


/*
R_LoadLighting
*/
static void R_LoadLighting(const lump_t *l){
	const char *s, *c;

	if(!l->filelen){
		r_loadmodel->lightdata = NULL;
		r_loadmodel->lightmap_scale = 16;
		return;
	}

	r_loadmodel->lightdata = R_HunkAlloc(l->filelen);
	memcpy(r_loadmodel->lightdata, mod_base + l->fileofs, l->filelen);

	r_loadmodel->lightmap_scale = -1;

	if((s = strstr(Cm_EntityString(), "\"lightmap_scale\""))){  // resolve lightmap scale

		c = Com_Parse(&s);  // parse the string itself
		c = Com_Parse(&s);  // and then the value

		r_loadmodel->lightmap_scale = atoi(c);

		Com_Dprintf("Resolved lightmap_scale: %d\n", r_loadmodel->lightmap_scale);
	}

	if(r_loadmodel->lightmap_scale == -1)  // ensure safe default
		r_loadmodel->lightmap_scale = 16;
}


/*
R_LoadVisibility
*/
static void R_LoadVisibility(const lump_t *l){
	int i;

	if(!l->filelen){
		r_loadmodel->vis = NULL;
		return;
	}

	r_loadmodel->vis = R_HunkAlloc(l->filelen);
	memcpy(r_loadmodel->vis, mod_base + l->fileofs, l->filelen);

	r_loadmodel->vis->numclusters = LittleLong(r_loadmodel->vis->numclusters);
	for(i = 0; i < r_loadmodel->vis->numclusters; i++){
		r_loadmodel->vis->bitofs[i][0] = LittleLong(r_loadmodel->vis->bitofs[i][0]);
		r_loadmodel->vis->bitofs[i][1] = LittleLong(r_loadmodel->vis->bitofs[i][1]);
	}
}


/*
R_LoadVertexes
*/
static void R_LoadVertexes(const lump_t *l){
	const dbspvertex_t *in;
	mvertex_t *out;
	int i, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadVertexes: Funny lump size in %s.", r_loadmodel->name);
	}
	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->vertexes = out;
	r_loadmodel->numvertexes = count;

	for(i = 0; i < count; i++, in++, out++){
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}


/*
R_LoadNormals
*/
static void R_LoadNormals(const lump_t *l){
	const dbspnormal_t *in;
	mvertex_t *out;
	int i, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadNormals: Funny lump size in %s.", r_loadmodel->name);
	}
	count = l->filelen / sizeof(*in);

	if(count != r_loadmodel->numvertexes){  // ensure sane normals count
		Com_Error(ERR_DROP, "R_LoadNormals: unexpected normals count in %s: (%d != %d).",
				r_loadmodel->name, count, r_loadmodel->numvertexes);
	}

	out = r_loadmodel->vertexes;

	for(i = 0; i < count; i++, in++, out++){
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}


/*
R_RadiusFromBounds
*/
static float R_RadiusFromBounds(const vec3_t mins, const vec3_t maxs){
	int i;
	vec3_t corner;

	for(i = 0; i < 3; i++){
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);
	}

	return VectorLength(corner);
}


/*
R_LoadSubmodels
*/
static void R_LoadSubmodels(const lump_t *l){
	const dbspmodel_t *in;
	msubmodel_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadSubmodels: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->submodels = out;
	r_loadmodel->numsubmodels = count;

	for(i = 0; i < count; i++, in++, out++){
		for(j = 0; j < 3; j++){  // spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0;
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0;
			out->origin[j] = LittleFloat(in->origin[j]);
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);

		out->headnode = LittleLong(in->headnode);

		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}


/*
R_LoadEdges
*/
static void R_LoadEdges(const lump_t *l){
	const dedge_t *in;
	medge_t *out;
	int i, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadEdges: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc((count + 1) * sizeof(*out));

	r_loadmodel->edges = out;
	r_loadmodel->numedges = count;

	for(i = 0; i < count; i++, in++, out++){
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}


/*
R_LoadTexinfo
*/
static void R_LoadTexinfo(const lump_t *l){
	const dtexinfo_t *in;
	mtexinfo_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadTexinfo: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->texinfo = out;
	r_loadmodel->numtexinfo = count;

	for(i = 0; i < count; i++, in++, out++){
		for(j = 0; j < 8; j++)
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);

		out->flags = LittleLong(in->flags);
		out->value = LittleLong(in->value);

		strncpy(out->name, in->texture, sizeof(in->texture));
		out->image = R_LoadImage(va("textures/%s", out->name), it_world);
	}
}


/*
R_SetSurfaceExtents

Sets in s->mins, s->maxs, s->stmins, s->stmaxs, ..
*/
static void R_SetSurfaceExtents(msurface_t *surf){
	vec3_t mins, maxs;
	vec2_t stmins, stmaxs;
	int i, j;
	const mvertex_t *v;
	const mtexinfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	stmins[0] = stmins[1] = 999999;
	stmaxs[0] = stmaxs[1] = -999999;

	tex = surf->texinfo;

	for(i = 0; i < surf->numedges; i++){
		const int e = r_loadmodel->surfedges[surf->firstedge + i];
		if(e >= 0)
			v = &r_loadmodel->vertexes[r_loadmodel->edges[e].v[0]];
		else
			v = &r_loadmodel->vertexes[r_loadmodel->edges[-e].v[1]];

		for(j = 0; j < 3; j++){  // calculate mins, maxs
			if(v->position[j] > maxs[j])
				maxs[j] = v->position[j];
			if(v->position[j] < mins[j])
				mins[j] = v->position[j];
		}

		for(j = 0; j < 2; j++){  // calculate stmins, stmaxs
			const float val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if(val < stmins[j])
				stmins[j] = val;
			if(val > stmaxs[j])
				stmaxs[j] = val;
		}
	}

	VectorCopy(mins, surf->mins);
	VectorCopy(maxs, surf->maxs);

	for(i = 0; i < 3; i++)
		surf->center[i] = (surf->mins[i] + surf->maxs[i]) / 2.0;

	for(i = 0; i < 2; i++){
		const int bmins = floor(stmins[i] / r_loadmodel->lightmap_scale);
		const int bmaxs = ceil(stmaxs[i] / r_loadmodel->lightmap_scale);

		surf->stmins[i] = bmins * r_loadmodel->lightmap_scale;
		surf->stmaxs[i] = bmaxs * r_loadmodel->lightmap_scale;

		surf->stcenter[i] = (surf->stmaxs[i] + surf->stmins[i]) / 2.0;
		surf->stextents[i] = surf->stmaxs[i] - surf->stmins[i];
	}
}


/*
R_LoadSurfaces
*/
static void R_LoadSurfaces(const lump_t *l){
	const dface_t *in;
	msurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadSurfaces: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->surfaces = out;
	r_loadmodel->numsurfaces = count;

	R_BeginBuildingLightmaps();

	for(surfnum = 0; surfnum < count; surfnum++, in++, out++){

		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		// resolve plane
		planenum = LittleShort(in->planenum);
		out->plane = r_loadmodel->planes + planenum;

		// and sidedness
		side = LittleShort(in->side);
		if(side){
			out->flags |= MSURF_PLANEBACK;
			VectorNegate(out->plane->normal, out->normal);
		}
		else
			VectorCopy(out->plane->normal, out->normal);

		// then texinfo
		ti = LittleShort(in->texinfo);
		if(ti < 0 || ti >= r_loadmodel->numtexinfo){
			Com_Error(ERR_DROP, "R_LoadSurfaces: Bad texinfo number.");
		}
		out->texinfo = r_loadmodel->texinfo + ti;

		if(!(out->texinfo->flags & (SURF_WARP | SURF_SKY)))
			out->flags |= MSURF_LIGHTMAP;

		// and size, texcoords, etc
		R_SetSurfaceExtents(out);

		// lastly lighting info
		i = LittleLong(in->lightofs);

		if(i != -1)
			out->samples = r_loadmodel->lightdata + i;
		else
			out->samples = NULL;

		// create lightmaps
		R_CreateSurfaceLightmap(out);

		// and flare
		R_CreateSurfaceFlare(out);
	}

	R_EndBuildingLightmaps();
}


/*
R_SetParent
*/
static void R_SetParent(mnode_t *node, mnode_t *parent){

	node->parent = parent;

	if(node->contents != CONTENTS_NODE)  // leaf
		return;

	R_SetParent(node->children[0], node);
	R_SetParent(node->children[1], node);
}


/*
R_LoadNodes
*/
static void R_LoadNodes(const lump_t *l){
	int i, j, count, p;
	const dnode_t *in;
	mnode_t *out;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadNodes: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->nodes = out;
	r_loadmodel->numnodes = count;

	for(i = 0; i < count; i++, in++, out++){

		for(j = 0; j < 3; j++){
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		p = LittleLong(in->planenum);
		out->plane = r_loadmodel->planes + p;

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);
		out->contents = CONTENTS_NODE;  // differentiate from leafs

		for(j = 0; j < 2; j++){
			p = LittleLong(in->children[j]);
			if(p >= 0)
				out->children[j] = r_loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(r_loadmodel->leafs + (-1 - p));
		}
	}

	R_SetParent(r_loadmodel->nodes, NULL);  // sets nodes and leafs
}


/*
R_LoadLeafs
*/
static void R_LoadLeafs(const lump_t *l){
	const dleaf_t *in;
	mleaf_t *out;
	int i, j, count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadLeafs: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->leafs = out;
	r_loadmodel->numleafs = count;

	for(i = 0; i < count; i++, in++, out++){

		for(j = 0; j < 3; j++){
			out->minmaxs[j] = LittleShort(in->mins[j]);
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]);
		}

		out->contents = LittleLong(in->contents);

		out->cluster = LittleShort(in->cluster);
		out->area = LittleShort(in->area);

		out->firstleafsurface = r_loadmodel->leafsurfaces +
				LittleShort(in->firstleafface);

		out->numleafsurfaces = LittleShort(in->numleaffaces);
	}
}


/*
R_LoadLeafsurfaces
*/
static void R_LoadLeafsurfaces(const lump_t *l){
	int i, count;
	const short *in;
	msurface_t **out;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadLeafsurfaces: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->leafsurfaces = out;
	r_loadmodel->numleafsurfaces = count;

	for(i = 0; i < count; i++){

		const int j = LittleShort(in[i]);

		if(j < 0 || j >= r_loadmodel->numsurfaces){
			Com_Error(ERR_DROP, "R_LoadLeafsurfaces: Bad surface number.");
		}

		out[i] = r_loadmodel->surfaces + j;
	}
}


/*
R_LoadSurfedges
*/
static void R_LoadSurfedges(const lump_t *l){
	int i, count;
	const int *in;
	int *out;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadSurfedges: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	if(count < 1 || count >= MAX_BSP_SURFEDGES){
		Com_Error(ERR_DROP, "R_LoadSurfedges: Bad surfedges count in %s: %i.",
				  r_loadmodel->name, count);
	}

	out = R_HunkAlloc(count * sizeof(*out));

	r_loadmodel->surfedges = out;
	r_loadmodel->numsurfedges = count;

	for(i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}


/*
R_LoadPlanes
*/
static void R_LoadPlanes(const lump_t *l){
	int i, j;
	cplane_t *out;
	const dplane_t *in;
	int count;

	in = (const void *)(mod_base + l->fileofs);
	if(l->filelen % sizeof(*in)){
		Com_Error(ERR_DROP, "R_LoadPlanes: Funny lump size in %s.", r_loadmodel->name);
	}

	count = l->filelen / sizeof(*in);
	out = R_HunkAlloc(count * 2 * sizeof(*out));

	r_loadmodel->planes = out;
	r_loadmodel->numplanes = count;

	for(i = 0; i < count; i++, in++, out++){
		int bits = 0;
		for(j = 0; j < 3; j++){
			out->normal[j] = LittleFloat(in->normal[j]);
			if(out->normal[j] < 0)
				bits |= 1 << j;
		}

		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
		out->signbits = bits;
	}
}


/*
R_AddBspVertexColor
*/
static void R_AddBspVertexColor(mvertex_t *vert, const msurface_t *surf){
	int i;

	vert->surfaces++;

	for(i = 0; i < 4; i++){

		const float vc = vert->color[i] * (vert->surfaces - 1) / vert->surfaces;
		const float sc = surf->color[i] / vert->surfaces;

		vert->color[i] = vc + sc;
	}
}


/*
R_LoadBspVertexArrays
*/
static void R_LoadBspVertexArrays(void){
	int i, j;
	int vertind, coordind, tangind, colorind;
	float soff, toff, s, t;
	float *point, *normal, *sdir, *tdir;
	vec4_t tangent;
	vec3_t binormal;
	msurface_t *surf;
	medge_t *edge;
	mvertex_t *vert;

	R_AllocVertexArrays(r_loadmodel);  // allocate the arrays

	if(!r_loadmodel->vertexcount)
		return;

	vertind = coordind = tangind = 0;
	surf = r_loadmodel->surfaces;

	for(i = 0; i < r_loadmodel->numsurfaces; i++, surf++){

		surf->index = vertind / 3;

		for(j = 0; j < surf->numedges; j++){
			const int index = r_loadmodel->surfedges[surf->firstedge + j];

			// vertex
			if(index > 0){  // negative indices to differentiate which end of the edge
				edge = &r_loadmodel->edges[index];
				vert = &r_loadmodel->vertexes[edge->v[0]];
			} else {
				edge = &r_loadmodel->edges[-index];
				vert = &r_loadmodel->vertexes[edge->v[1]];
			}

			point = vert->position;
			memcpy(&r_loadmodel->verts[vertind], point, sizeof(vec3_t));

			// texture directional vectors and offsets
			sdir = surf->texinfo->vecs[0];
			soff = surf->texinfo->vecs[0][3];

			tdir = surf->texinfo->vecs[1];
			toff = surf->texinfo->vecs[1][3];

			// texture coordinates
			s = DotProduct(point, sdir) + soff;
			s /= surf->texinfo->image->width;

			t = DotProduct(point, tdir) + toff;
			t /= surf->texinfo->image->height;

			r_loadmodel->texcoords[coordind + 0] = s;
			r_loadmodel->texcoords[coordind + 1] = t;

			if(surf->flags & MSURF_LIGHTMAP){  // lightmap coordinates
				s = DotProduct(point, sdir) + soff;
				s -= surf->stmins[0];
				s += surf->light_s * r_loadmodel->lightmap_scale;
				s += r_loadmodel->lightmap_scale / 2.0;
				s /= r_lightmaps.size * r_loadmodel->lightmap_scale;

				t = DotProduct(point, tdir) + toff;
				t -= surf->stmins[1];
				t += surf->light_t * r_loadmodel->lightmap_scale;
				t += r_loadmodel->lightmap_scale / 2.0;
				t /= r_lightmaps.size * r_loadmodel->lightmap_scale;
			}

			r_loadmodel->lmtexcoords[coordind + 0] = s;
			r_loadmodel->lmtexcoords[coordind + 1] = t;

			// normal vector
			if(surf->texinfo->flags & SURF_PHONG &&
					!VectorCompare(vert->normal, vec3_origin))  // phong shaded
				normal = vert->normal;
			else  // per-plane
				normal = surf->normal;

			memcpy(&r_loadmodel->normals[vertind], normal, sizeof(vec3_t));

			// tangent vector
			TangentVectors(normal, sdir, tdir, tangent, binormal);
			memcpy(&r_loadmodel->tangents[tangind], tangent, sizeof(vec4_t));

			/*printf("%1.3f %1.3f %1.3f\n%1.3f %1.3f %1.3f\n%1.3f %1.3f %1.3f\n---\n",
					tangent[0], binormal[0], normal[0],
					tangent[1], binormal[1], normal[1],
					tangent[2], binormal[2], normal[2]);*/

			// accumulate colors
			R_AddBspVertexColor(vert, surf);

			vertind += 3;
			coordind += 2;
			tangind += 4;
		}
	}

	colorind = 0;
	surf = r_loadmodel->surfaces;

	// now iterate over the verts again, assembling the accumulated colors
	for(i = 0; i < r_loadmodel->numsurfaces; i++, surf++){

		for(j = 0; j < surf->numedges; j++){
			const int index = r_loadmodel->surfedges[surf->firstedge + j];

			// vertex
			if(index > 0){  // negative indices to differentiate which end of the edge
				edge = &r_loadmodel->edges[index];
				vert = &r_loadmodel->vertexes[edge->v[0]];
			} else {
				edge = &r_loadmodel->edges[-index];
				vert = &r_loadmodel->vertexes[edge->v[1]];
			}

			memcpy(&r_loadmodel->colors[colorind], vert->color, sizeof(vec4_t));
			colorind += 4;
		}
	}
}


// temporary space for sorting surfaces by texnum
static msurfaces_t *r_sorted_surfaces[MAX_GL_TEXTURES];

/*
R_SortSurfacesArrays_
*/
static void R_SortSurfacesArrays_(msurfaces_t *surfs){
	int i, j;

	for(i = 0; i < surfs->count; i++){

		const int texnum = surfs->surfaces[i]->texinfo->image->texnum;

		R_SurfaceToSurfaces(r_sorted_surfaces[texnum], surfs->surfaces[i]);
	}

	surfs->count = 0;

	for(i = 0; i < r_numimages; i++){

		msurfaces_t *sorted = r_sorted_surfaces[r_images[i].texnum];

		if(sorted && sorted->count){

			for(j = 0; j < sorted->count; j++)
				R_SurfaceToSurfaces(surfs, sorted->surfaces[j]);

			sorted->count = 0;
		}
	}
}


/*
R_SortSurfacesArrays

Reorders all surfaces arrays for the specified model, grouping the surface
pointers by texture.  This dramatically reduces glBindTexture calls.
*/
static void R_SortSurfacesArrays(model_t *mod){
	msurface_t *surf, *s;
	int i, ns;

	// resolve the start surface and total surface count
	if(mod->type == mod_bsp){  // world model
		s = mod->surfaces;
		ns = mod->numsurfaces;
	}
	else {  // submodels
		s = &mod->surfaces[mod->firstmodelsurface];
		ns = mod->nummodelsurfaces;
	}

	memset(r_sorted_surfaces, 0, sizeof(r_sorted_surfaces));

	// allocate the per-texture surfaces arrays and determine counts
	for(i = 0, surf = s; i < ns; i++, surf++){

		msurfaces_t *surfs = r_sorted_surfaces[surf->texinfo->image->texnum];

		if(!surfs){  // allocate it
			surfs = (msurfaces_t *)Z_Malloc(sizeof(*surfs));
			r_sorted_surfaces[surf->texinfo->image->texnum] = surfs;
		}

		surfs->count++;
	}

	// allocate the surfaces pointers based on counts
	for(i = 0; i < r_numimages; i++){

		msurfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if(surfs){
			surfs->surfaces = (msurface_t **)Z_Malloc(sizeof(msurface_t *) * surfs->count);
			surfs->count = 0;
		}
	}

	// sort the model's surfaces arrays into the per-texture arrays
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++){

		if(mod->sorted_surfaces[i]->count)
			R_SortSurfacesArrays_(mod->sorted_surfaces[i]);
	}

	// free the per-texture surfaces arrays
	for(i = 0; i < r_numimages; i++){

		msurfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];

		if(surfs){
			if(surfs->surfaces)
				Z_Free(surfs->surfaces);
			Z_Free(surfs);
		}
	}
}


/*
R_LoadSurfacesArrays_
*/
static void R_LoadSurfacesArrays_(model_t *mod){
	msurface_t *surf, *s;
	int i, ns;

	// allocate the surfaces array structures
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++)
		mod->sorted_surfaces[i] = (msurfaces_t *)R_HunkAlloc(sizeof(msurfaces_t));

	// resolve the start surface and total surface count
	if(mod->type == mod_bsp){  // world model
		s = mod->surfaces;
		ns = mod->numsurfaces;
	}
	else {  // submodels
		s = &mod->surfaces[mod->firstmodelsurface];
		ns = mod->nummodelsurfaces;
	}

	// determine the maximum counts for each rendered type in order to
	// allocate only what is necessary for the specified model
	for(i = 0, surf = s; i < ns; i++, surf++){

		if(surf->texinfo->flags & SURF_SKY){
			mod->sky_surfaces->count++;
			continue;
		}

		if(surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)){
			if(surf->texinfo->flags & SURF_WARP)
				mod->blend_warp_surfaces->count++;
			else
				mod->blend_surfaces->count++;
		} else {
			if(surf->texinfo->flags & SURF_WARP)
				mod->opaque_warp_surfaces->count++;
			else if(surf->texinfo->flags & SURF_ALPHATEST)
				mod->alpha_test_surfaces->count++;
			else
				mod->opaque_surfaces->count++;
		}

		if(surf->texinfo->image->material.flags & STAGE_RENDER)
			mod->material_surfaces->count++;

		if(surf->texinfo->image->material.flags & STAGE_FLARE)
			mod->flare_surfaces->count++;

		if(!(surf->texinfo->flags & SURF_WARP))
			mod->back_surfaces->count++;
	}

	// allocate the surfaces pointers based on the counts
	for(i = 0; i < NUM_SURFACES_ARRAYS; i++){

		if(mod->sorted_surfaces[i]->count){
			mod->sorted_surfaces[i]->surfaces = (msurface_t **)R_HunkAlloc(
					sizeof(msurface_t *) * mod->sorted_surfaces[i]->count);

			mod->sorted_surfaces[i]->count = 0;
		}
	}

	// iterate the surfaces again, populating the allocated arrays based
	// on primary render type
	for(i = 0, surf = s; i < ns; i++, surf++){

		if(surf->texinfo->flags & SURF_SKY){
			R_SurfaceToSurfaces(mod->sky_surfaces, surf);
			continue;
		}

		if(surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)){
			if(surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->blend_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->blend_surfaces, surf);
		} else {
			if(surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->opaque_warp_surfaces, surf);
			else if(surf->texinfo->flags & SURF_ALPHATEST)
				R_SurfaceToSurfaces(mod->alpha_test_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->opaque_surfaces, surf);
		}

		if(surf->texinfo->image->material.flags & STAGE_RENDER)
			R_SurfaceToSurfaces(mod->material_surfaces, surf);

		if(surf->texinfo->image->material.flags & STAGE_FLARE)
			R_SurfaceToSurfaces(mod->flare_surfaces, surf);

		if(!(surf->texinfo->flags & SURF_WARP))
			R_SurfaceToSurfaces(mod->back_surfaces, surf);
	}

	// now sort them by texture
	R_SortSurfacesArrays(mod);
}


/*
R_LoadSurfacesArrays
*/
static void R_LoadSurfacesArrays(void){
	int i;

	R_LoadSurfacesArrays_(r_loadmodel);

	for(i = 0; i < r_loadmodel->numsubmodels; i++){
		R_LoadSurfacesArrays_(&r_inlinemodels[i]);
	}
}


/*
R_SetModel
*/
static void R_SetModel(mnode_t *node, model_t *model){

	node->model = model;

	if(node->contents != CONTENTS_NODE)
		return;

	R_SetModel(node->children[0], model);
	R_SetModel(node->children[1], model);
}


/*
R_SetupSubmodels

The submodels have been loaded into memory, but are not yet
represented as mmodel_t.  Convert them.
*/
static void R_SetupSubmodels(void){
	int i;

	for(i = 0; i < r_loadmodel->numsubmodels; i++){

		model_t *mod = &r_inlinemodels[i];
		const msubmodel_t *sub = &r_loadmodel->submodels[i];

		*mod = *r_loadmodel;  // copy most info from world
		mod->type = mod_bsp_submodel;

		snprintf(mod->name, sizeof(mod->name), "*%d", i);

		// copy the rest from the submodel
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		mod->firstnode = sub->headnode;
		mod->nodes = &r_loadmodel->nodes[mod->firstnode];

		R_SetModel(mod->nodes, mod);

		mod->firstmodelsurface = sub->firstface;
		mod->nummodelsurfaces = sub->numfaces;
	}
}


/*
R_LoadBspModel
*/
void R_LoadBspModel(model_t *mod, void *buffer){
	extern void Cl_LoadProgress(int percent);
	dbspheader_t *header;
	int i, version;

	if(r_worldmodel){
		Com_Error(ERR_DROP, "R_LoadBspModel: Loaded bsp model after world.");
	}

	header = (dbspheader_t *)buffer;

	version = LittleLong(header->version);
	if(version != BSP_VERSION && version != BSP_VERSION_){
		Com_Error(ERR_DROP, "R_LoadBspModel: %s has unsupported version: %d",
				mod->name, version);
	}

	mod->type = mod_bsp;
	mod->version = version;

	// swap all the lumps
	mod_base = (byte *)header;

	for(i = 0; i < sizeof(*header) / 4; i++)
		((int *)header)[i] = LittleLong(((int *)header)[i]);

	// load into heap
	R_LoadVertexes(&header->lumps[LUMP_VERTEXES]);
	Cl_LoadProgress( 4);

	if(header->version == BSP_VERSION_)  // enhanced format
		R_LoadNormals(&header->lumps[LUMP_NORMALS]);

	R_LoadEdges(&header->lumps[LUMP_EDGES]);
	Cl_LoadProgress( 8);

	R_LoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	Cl_LoadProgress(12);

	R_LoadLighting(&header->lumps[LUMP_LIGHTING]);
	Cl_LoadProgress(16);

	R_LoadPlanes(&header->lumps[LUMP_PLANES]);
	Cl_LoadProgress(20);

	R_LoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	Cl_LoadProgress(24);

	R_LoadSurfaces(&header->lumps[LUMP_FACES]);
	Cl_LoadProgress(28);

	R_LoadLeafsurfaces(&header->lumps[LUMP_LEAFFACES]);
	Cl_LoadProgress(32);

	R_LoadVisibility(&header->lumps[LUMP_VISIBILITY]);
	Cl_LoadProgress(36);

	R_LoadLeafs(&header->lumps[LUMP_LEAFS]);
	Cl_LoadProgress(40);

	R_LoadNodes(&header->lumps[LUMP_NODES]);
	Cl_LoadProgress(44);

	R_LoadSubmodels(&header->lumps[LUMP_MODELS]);
	Cl_LoadProgress(48);

	R_SetupSubmodels();

	R_LoadBspVertexArrays();

	R_LoadSurfacesArrays();

	memset(&r_locals, 0, sizeof(r_locals));
	r_locals.oldcluster = -1;  // force bsp iteration
}
